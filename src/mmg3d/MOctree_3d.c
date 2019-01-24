/* =============================================================================
**  This file is part of the mmg software package for the tetrahedral
**  mesh modification.
**  Copyright (c) Bx INP/CNRS/Inria/UBordeaux/UPMC, 2004-
**
**  mmg is free software: you can redistribute it and/or modify it
**  under the terms of the GNU Lesser General Public License as published
**  by the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  mmg is distributed in the hope that it will be useful, but WITHOUT
**  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
**  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
**  License for more details.
**
**  You should have received a copy of the GNU Lesser General Public
**  License and of the GNU General Public License along with mmg (in
**  files COPYING.LESSER and COPYING). If not, see
**  <http://www.gnu.org/licenses/>. Please read their terms carefully and
**  use this copy of the mmg distribution only if you accept them.
** =============================================================================
*/

/**
 * \file MOctree_3d.c
 * \brief Tools to manage an octree mesh.
 * \author Juliette Busquet (Enseirb-Matmeca)
 * \author Antoine Huc (Enseirb-Matmeca)
 * \author Fanny Kuhn (Enseirb-Matmeca)
 * \author Algiane Froehly (Inria)
 * \version 1
 * \copyright GNU Lesser General Public License.
 *
 * Functions related to the creation of an octree from a grid and the associated
 * signed distance function. The octree cells are allowed to merge if they do
 * not cross the 0 levels-set of the signed distance function. At the end, the
 * octree must be 2:1 balanced (only 1 depth level of difference betwwen to
 * adjacent cells).
 *
 */

#include "mmg3d.h"

#define MMG3D_EPSRAD       1.00005
#define MMG3D_EPSCON       1e-5 //5.0e-4
#define MMG3D_LONMAX       4096
/**
 * \param mesh pointer toward a MMG5 mesh
 * \param q pointer toward the MOctree root
 * \param ip index of the bottom-left-front corner of the root cell
 * \param length length of the octree in each direction
 * \param depth_max maximal depth of the octree
 *
 * \return 1 if success, 0 if fail.
 *
 * Allocate and init an MOctree structure.
 *
 */
int MMG3D_init_MOctree  ( MMG5_pMesh mesh, MMG5_pMOctree *q, int ip,
                          double length[3],int depth_max ) {


  /** Root allocation */
  MMG5_ADD_MEM(mesh,sizeof(MMG5_MOctree),"MOctree root",
               return 0);
  MMG5_SAFE_MALLOC( *q,1, MMG5_MOctree, return 0);

  (*q)->length[0] = length[0];
  (*q)->length[1] = length[1];
  (*q)->length[2] = length[2];

  (*q)->depth_max = depth_max;

  (*q)->nspan_at_root = pow(2,depth_max);

  /** Check that we have enough memory to allocate a new cell */
  MMG5_ADD_MEM(mesh,sizeof(MMG5_MOctree_s),"initial MOctree cell",
               return 0);

  /** New cell allocation */
  MMG5_SAFE_MALLOC( (*q)->root,1, MMG5_MOctree_s, return 0);
  MMG3D_init_MOctree_s( mesh,(*q)->root,ip,0,0);
  (*q)->root->nsons = 0;

  return 1;
}

/**
 * \param mesh pointer toward a MMG5 mesh
 * \param q pointer toward the MOctree cell
 * \param ip index of the bottom-left-front corner of the cell
 * \param depth cell's depth
 * \param split_ls 1 if the cell is intersected by the level-set
 *
 * \return 1 if success, 0 if fail.
 *
 * Allocate and init an MOctree Cell
 *
 */
int MMG3D_init_MOctree_s( MMG5_pMesh mesh, MMG5_MOctree_s* q,int ip, int depth,int8_t split_ls ) {

  q->father = NULL;
  q->sons   = NULL;
  q->nsons  = 0;

  q->blf_ip = ip;
  q->depth  = depth;

  q->split_ls = split_ls;
  q->leaf = 0;
  q->coordoct[0]=0;
  q->coordoct[1]=0;
  q->coordoct[2]=0;
  return 1;
}

/**
 * \param mesh pointer toward a MMG5 mesh
 * \param q pointer toward the MOctree cell
 *
 * \return 1 if success, 0 if fail.
 *
 * Set the parameter split_ls of a cell to 1 if the level-set intersect the cell for every cell of the octree max.
 *
 */
int  MMG3D_set_splitls_MOctree ( MMG5_pMesh mesh, MMG5_MOctree_s* q, MMG5_pSol sol) {
  int ip;
  int FDL,FDR,BDL,BDR,FUL,FUR,BUL,BUR;// ip of the 8 vertices of an octree cell
  ip=q->blf_ip;

  FDL=ip; // Front Down Left corner
  FDR=FDL+1;
  BDL=ip+mesh->freeint[0];
  BDR=BDL+1;
  FUL=ip+mesh->freeint[0]*mesh->freeint[1];
  FUR=FUL+1;
  BUL=ip+mesh->freeint[0]*(1+mesh->freeint[1]);
  BUR=BUL+1;

  if(sol->m[FDL]*sol->m[FDR]<0 || sol->m[FDR]*sol->m[BDL]<0 ||\
     sol->m[BDL]*sol->m[BDR]<0 || sol->m[BDR]*sol->m[FUL]<0 ||\
     sol->m[FUL]*sol->m[FUR]<0 || sol->m[FUR]*sol->m[BUL]<0 || (sol->m[BUL]*sol->m[BUR]<0))
  {
    q->split_ls=1;
  }
  else
  {
    q->split_ls=0;
  }

  return 1;
}

/**
 * \param mesh pointer toward a MMG5 mesh
 * \param q pointer toward the MOctree cell
 *
 * \return 1 if success, 0 if fail.
 *
 * Split an MOctree cell \ref q into \ref MMG3D_SIZE_OCTREESONS MOctree Cells.
 * \ref q must be a leaf.
 *
 */
 int  MMG3D_split_MOctree_s ( MMG5_pMesh mesh,MMG5_MOctree_s* q,MMG5_pSol sol) {
   int i;
   int depth_max = mesh->octree->depth_max;
   int ncells_x = mesh->freeint[0];
   int ncells_y = mesh->freeint[1];
   int ncells_z = mesh->freeint[2];

   if(q->depth < depth_max)
   {
     MMG5_SAFE_MALLOC(q->sons,q->nsons, MMG5_MOctree_s, return 0);
     for(i=0; i<q->nsons; i++)
     {
       MMG3D_init_MOctree_s(mesh, &q->sons[i], 0, q->depth + 1, 0);
       //calculus of octree coordinates and ip
       int power = pow(2,depth_max-(q->depth+1));

       q->sons[i].coordoct[0]=q->coordoct[0];
       q->sons[i].coordoct[1]=q->coordoct[1];
       q->sons[i].coordoct[2]=q->coordoct[2];

       if(i==1)
       {
         q->sons[i].coordoct[0]+=power;
       }
       else if(i==2)
       {
         q->sons[i].coordoct[1]+=power;
       }
       else if(i==3)
       {
         q->sons[i].coordoct[0]+=power;
         q->sons[i].coordoct[1]+=power;
       }
       else if(i==4)
       {
         q->sons[i].coordoct[2]+=power;
       }
       else if(i==5)
       {
         q->sons[i].coordoct[0]+=power;
         q->sons[i].coordoct[2]+=power;
       }
       else if(i==6)
       {
         q->sons[i].coordoct[1]+=power;
         q->sons[i].coordoct[2]+=power;
       }
       else if(i==7)
       {
         q->sons[i].coordoct[0]+=power;
         q->sons[i].coordoct[1]+=power;
         q->sons[i].coordoct[2]+=power;
       }

       q->sons[i].father = q;
       q->sons[i].nsons = 8;
       if(q->sons[i].coordoct[0] < ncells_x-1 && q->sons[i].coordoct[1] < ncells_y-1 && q->sons[i].coordoct[2] < ncells_z-1)
       {
         q->sons[i].ghost = 0;
         q->blf_ip=q->coordoct[2]*ncells_x*ncells_y+q->coordoct[1]*ncells_x+q->coordoct[0]+1;
       }
       else
       {
         q->sons[i].ghost = 1;
       }
       MMG3D_split_MOctree_s(mesh, &q->sons[i], sol);
     }
   }
   else{
     /*calculus of ip for leaves*/
     int span_y,span_z;
     int ncells_xy = ncells_x * ncells_y;

     span_y = ncells_x;
     span_z = ncells_xy;
     if(q->ghost == 0)
     {
       q->blf_ip=q->coordoct[2]*ncells_xy+q->coordoct[1]*ncells_x+q->coordoct[0]+1;       if(q->coordoct[0] < ncells_x-1 && q->coordoct[1] < ncells_y-1 && q->coordoct[2] < ncells_z-1)
       MMG3D_set_splitls_MOctree (mesh, q, sol);
     }
     q->leaf=1;
   }

   return 1;
 }

/**
 * \param q pointer toward the MOctree
 *
 * \return 1 if success, 0 if fail.
 *
 * Free a MOctree.
 *
 */
int MMG3D_free_MOctree  ( MMG5_pMOctree** q, MMG5_pMesh mesh) {
  MMG5_DEL_MEM(mesh,*q);
  return 1;
}

/**
 * \param q pointer toward the MOctree cell
 *
 * \return 1 if success, 0 if fail.
 *
 * Free a MOctree cell.
 *
 */
int MMG3D_free_MOctree_s( MMG5_MOctree_s* q, MMG5_pMesh mesh) {
  MMG5_DEL_MEM(mesh,q);
  return 1;
}

/**
 * \param mesh pointer toward the mesh
 * \param q pointer toward the MOctree cell
 *
 * \return 1 if success, 0 if fail.
 *
 * Merge the \ref q MOctree cell (remove the \ref MMG3D_SIZE OCTREESONS sons of
 * \ref q, all sons being leafs).
 *
 */
int  MMG3D_merge_MOctree_s ( MMG5_MOctree_s* q, MMG5_pMesh mesh) {

  q->nsons = 0;
  q->leaf = 1;
  MMG3D_free_MOctree_s(q->sons, mesh);
  return 1;
}

/**
 * \param mesh pointer toward the mesh
 * \param q pointer toward the MOctree cell
 * \param span span between a corner and the other corner in one given direction
 * \param ip0 pointer toward the index of the bottom left front corner of the cell (to fill)
 * \param ip1 pointer toward the index of the bottom right front corner of the cell (to fill)
 * \param ip2 pointer toward the index of the bottom right back corner of the cell (to fill)
 * \param ip3 pointer toward the index of the bottom left back corner of the cell (to fill)
 * \param ip4 pointer toward the index of the top left front corner of the cell (to fill)
 * \param ip5 pointer toward the index of the top right front corner of the cell (to fill)
 * \param ip6 pointer toward the index of the top right back corner of the cell (to fill)
 * \param ip7 pointer toward the index of the top left back corner of the cell (to fill)
 *
 * \return 1 if success, 0 if fail.
 *
 * Compute the indices of the corners of the octree cell (vertices are numbering
 * as in the initial grid)
 *
 */
int MMG3D_get_MOctreeCornerIndices ( MMG5_pMesh mesh, MMG5_MOctree_s *q,int span,int *ip0,
                                     int *ip1, int *ip2,int *ip3,int *ip4,int *ip5,
                                     int *ip6,int *ip7 ) {

  int span_y,span_z;
  int ncells_x  = mesh->freeint[0];
  int ncells_y  = mesh->freeint[1];
  int ncells_z  = mesh->freeint[2];
  int ncells_xy = ncells_x*ncells_y;
  int ncells_xyz= ncells_x*ncells_y*ncells_z;

  span_y = span*ncells_x;
  span_z = span*ncells_xy;

  if (q->coordoct[0]+1 +span > ncells_x)
  {
    span = ncells_x-(q->coordoct[0]+1);
  }
  if ((q->coordoct[1]+1)*ncells_x + span_y > ncells_xy)
  {
    span_y = ncells_xy-(q->coordoct[1]+1)*ncells_x;
  }
  if ((q->coordoct[2]+1)*ncells_xy + span_z > ncells_xyz)
  {
    span_z = ncells_xyz-(q->coordoct[2]+1)*ncells_xy;
  }
  *ip0 = q->blf_ip;
  *ip1 = q->blf_ip + span ;
  *ip2 = q->blf_ip + span_y + span;
  *ip3 = q->blf_ip + span_y;
  *ip4 = q->blf_ip + span_z;
  *ip5 = q->blf_ip + span_z + span;
  *ip6 = q->blf_ip + span_z + span_y + span;
  *ip7 = q->blf_ip + span_z + span_y;

  return 1;
}
/**
 * \param mesh pointer toward the mesh
 * \param q pointer toward the MOctree cell
 * \param span span between a corner and the other corner in one given direction
 * \param np pointer toward the number of used points (to fill)
 * \param nc pointer toward the number of cell leafs (to fill)
 *
 * \return 1 if success, 0 if fail.
 *
 * Mark as used the points that are at the corners of the octree cells
 * leafs. Count the number of use points and the number of leafs.
 *
 */
int  MMG3D_mark_MOctreeCellCorners ( MMG5_pMesh mesh, MMG5_MOctree_s* q,int span,int *np,int *nc ) {
  MMG5_pPoint ppt;
  int         i,ip[8];
  int ncells_x = mesh->freeint[0];
  int ncells_y = mesh->freeint[1];
  int ncells_z = mesh->freeint[2];

  if ( q->leaf==1) {
    if(q->coordoct[0] < ncells_x-1 && q->coordoct[1] < ncells_y-1 && q->coordoct[2] < ncells_z-1)
    {
      if ( !MMG3D_get_MOctreeCornerIndices ( mesh,q,span,ip,ip+1,ip+2,ip+3,ip+4,ip+5,ip+6,ip+7 ) ) {
        fprintf(stderr,"\n  ## Error: %s: unable to compute the indices of the"
        " corners of the octree cell.\n",__func__);
        return 0;
      }

      for ( i=0; i<8; ++i ) {
        ppt = &mesh->point[ip[i]];
        if ( !MG_VOK(ppt) ) {
          ++(*np);
          ppt->tag &= ~MG_NUL;
        }
      }
      ++(*nc);
    }
  }
  else {
    span /= 2;

    for ( i=0; i<q->nsons; ++i ) {
      if ( !MMG3D_mark_MOctreeCellCorners ( mesh,&q->sons[i],span,np,nc ) ) return 0;
    }
  }

  return 1;
}


/**
 * \param mesh pointer toward the mesh
 * \param q pointer toward the MOctree cell
 * \param span span between a corner and the other corner in one given direction
 * \param inm pointer toward the file in which we save the octree cells
 *
 * \return 1 if success, 0 if fail.
 *
 * Write the hexahedron associated to the octree leafs.
 *
 */
int  MMG3D_write_MOctreeCell ( MMG5_pMesh mesh, MMG5_MOctree_s* q,int span,FILE *inm ) {
  int i,ip0,ip1,ip2,ip3,ip4,ip5,ip6,ip7;
  static int nvert = 8;
  int ncells_x = mesh->freeint[0];
  int ncells_y = mesh->freeint[1];
  int ncells_z = mesh->freeint[2];

  if ( q->leaf==1) {
    if(q->coordoct[0] < ncells_x-1 && q->coordoct[1] < ncells_y-1 && q->coordoct[2] < ncells_z-1){
      if ( !MMG3D_get_MOctreeCornerIndices ( mesh,q,span,&ip0,&ip1,&ip2,&ip3,
        &ip4,&ip5,&ip6,&ip7 ) ) {
          fprintf(stderr,"\n  ## Error: %s: unable to compute the indices of the"
          " corners of the octree cell.\n",__func__);
          return 0;
        }

        fprintf(inm,"%d %d %d %d %d %d %d %d %d\n",nvert,mesh->point[ip0].tmp,
        mesh->point[ip1].tmp,mesh->point[ip2].tmp,mesh->point[ip3].tmp,
        mesh->point[ip4].tmp,mesh->point[ip5].tmp,mesh->point[ip6].tmp,
        mesh->point[ip7].tmp);

        mesh->point[ip0].ref=22;
        mesh->point[ip1].ref=22;
        mesh->point[ip2].ref=22;
        mesh->point[ip3].ref=22;
        mesh->point[ip4].ref=22;
        mesh->point[ip5].ref=22;
        mesh->point[ip6].ref=22;
        mesh->point[ip7].ref=22;
      }
    }
  else {
    span /= 2;

    for ( i=0; i<q->nsons; ++i ) {
      if ( !MMG3D_write_MOctreeCell ( mesh,&q->sons[i],span,inm ) ) {
        return 0;
      }
    }
  }

  return 1;
}


/**
 * \param mesh pointer toward the mesh
 * \param q pointer toward the MOctree cell
 * \param dir direction to find the neighbour
 * \param Neighbour pointer toward the neighbour cell
 *
 * \return 1 if success, 0 if fail.
 *
 * Find the neighbours of a cell.
 *
 */

int MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(MMG5_pMesh mesh, MMG5_MOctree_s* q, int dir, MMG5_MOctree_s* Neighbour)
{
  int i;
  MMG5_MOctree_s* Temp_Neighbour;
  MMG5_SAFE_MALLOC(Temp_Neighbour,1, MMG5_MOctree_s, return 0);
  MMG3D_init_MOctree_s(mesh, Temp_Neighbour, 0, 0, 0 ); // peut être changer l'initialisation pour les voisins j'ai mis 0 par défaut.

  if(dir == 0) //UP
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==0 || i==1 || i==2 || i==3)  && q==&q->father->sons[i]) // Is q a south child child?
      {
        *Neighbour=q->father->sons[i+4];
        return 1;
      }
    }

    MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,dir,Temp_Neighbour);
    if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
    {
      *Neighbour=*Temp_Neighbour;
      return 1;
    }

    //q is guaranted to be a North child
    for (i=0; i<8; i++)
    {
      if((i==4 || i==5 || i==6 || i==7) && q==&q->father->sons[i]) // Which north child is q?
      {
        *Neighbour=Temp_Neighbour->sons[i-4];
        return 1;
      }
    }
  }

  if(dir == 1) //DOWN
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==4 || i==5 || i==6 || i==7)  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i-4];
        return 1;
      }
    }

    MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,dir,Temp_Neighbour);
    if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
    {
      *Neighbour=*Temp_Neighbour;
      return 1;
    }


    for (i=0; i<8; i++)
    {
      if((i==0 || i==1 || i==2 || i==3) && q==&q->father->sons[i])
      {
        *Neighbour=Temp_Neighbour->sons[i+4];
        return 1;
      }
    }
  }

  if(dir == 2) //LEFT
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==1 || i==3 || i==5 || i==7)  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i-1];
        return 1;
      }
    }

    MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, dir,Temp_Neighbour);
    if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
    {
      *Neighbour=*Temp_Neighbour;
      return 1;
    }


    for (i=0; i<8; i++)
    {
      if((i==0 || i==2 || i==4 || i==6) && q==&q->father->sons[i])
      {
        *Neighbour=Temp_Neighbour->sons[i+1];
        return 1;
      }
    }
  }


  if(dir == 3) //RIGHT
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==0 || i==2 || i==4 || i==6) && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i+1];
        return 1;
      }
    }

    MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, dir,Temp_Neighbour);
    if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
    {
      *Neighbour=*Temp_Neighbour;
      return 1;
    }


    for (i=0; i<8; i++)
    {
      if((i==1 || i==3 || i==5 || i==7)  && q==&q->father->sons[i])
      {
        *Neighbour=Temp_Neighbour->sons[i-1];
        return 1;
      }
    }
  }

  if(dir == 4) //BACK
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==0 || i==1 || i==4 || i==5) && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i+2];
        return 1;
      }
    }

    MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, dir,Temp_Neighbour);
    if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
    {
      *Neighbour=*Temp_Neighbour;
      return 1;
    }


    for (i=0; i<8; i++)
    {
      if((i==2 || i==3 || i==6 || i==7)  && q==&q->father->sons[i])
      {
        *Neighbour=Temp_Neighbour->sons[i-2];
        return 1;
      }
    }
  }



  if(dir == 5) //FRONT
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==2 || i==3 || i==6 || i==7)  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i-2];
        return 1;
      }
    }

    MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, dir,Temp_Neighbour);
    if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
    {
      *Neighbour=*Temp_Neighbour;
      return 1;
    }


    for (i=0; i<8; i++)
    {
      if((i==0 || i==1 || i==4 || i==5) && q==&q->father->sons[i])
      {
        *Neighbour=Temp_Neighbour->sons[i+2];
        return 1;
      }
    }
  }



  if(dir == 6) //UPLEFT
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==1 || i==3 )  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i+3];
      }
      else if((i==0 || i==2 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,2,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+5];
        return 1;
      }
      else if((i==5 || i==7 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,0,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-5];
        return 1;
      }
      else if((i==4 || i==6 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,6,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-3];
        return 1;
      }
    }
  }

  if(dir == 7) //UPRIGHT
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==0 || i==2 )  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i+5];
      }
      else if((i==1 || i==3 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,3,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+3];
        return 1;
      }
      else if((i==4 || i==6 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,0,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-3];
        return 1;
      }
      else if((i==5 || i==7 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,7,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-5];
        return 1;
      }
    }
  }

  if(dir == 8) ///DOWNLEFT
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==5 || i==7 )  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i-5];
      }
      else if((i==4 || i==6 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,2,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-3];
        return 1;
      }
      else if((i==1 || i==3 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,1,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+3];
        return 1;
      }
      else if((i==0 || i==2 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,8,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+5];
        return 1;
      }
    }
  }


  if(dir == 9) //DOWNRIGHT
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==4 || i==6 )  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i-3];
      }
      else if((i==5 || i==7 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,3,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-5];
        return 1;
      }
      else if((i==0 || i==2 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,1,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+5];
        return 1;
      }
      else if((i==1 || i==3 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,9,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+3];
        return 1;
      }
    }
  }


  if(dir == 10) //UPFRONT
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==2 || i==3 )  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i+2];
      }
      else if((i==0 || i==1 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,5,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+6];
        return 1;
      }
      else if((i==6 || i==7 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, 0,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-6];
        return 1;
      }
      else if((i==4 || i==5 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, 10,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-2];
        return 1;
      }
    }
  }


  if(dir == 11) //UPBACK
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==0 || i==1 )  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i+6];
      }
      else if((i==2 || i==3 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, 4,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+2];
        return 1;
      }
      else if((i==4 || i==5 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, 0,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-2];
        return 1;
      }
      else if((i==6 || i==7 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, 11,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-6];
        return 1;
      }
    }
  }



  if(dir == 12) //DOWNFRONT
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==6 || i==7 )  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i-6];
      }
      else if((i==4 || i==5 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,5,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-2];
        return 1;
      }
      else if((i==2 || i==3 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,1,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+2];
        return 1;
      }
      else if((i==0 || i==1 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, 12,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+6];
        return 1;
      }
    }
  }


  if(dir == 13) //DOWNBACK
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==4 || i==5 )  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i-2];
      }
      else if((i==6 || i==7 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, 4,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-6];
        return 1;
      }
      else if((i==0 || i==1 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,1,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+6];
        return 1;
      }
      else if((i==2 || i==3 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, 13,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+2];
        return 1;
      }
    }
  }


  if(dir == 14) //LEFTFRONT
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==3 || i==7 )  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i-3];
      }
      else if((i==1 || i==5 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,5,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+1];
        return 1;
      }
      else if((i==2 || i==6 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,2,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-1];
        return 1;
      }
      else if((i==0 || i==4 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,14,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+3];
        return 1;
      }
    }
  }


  if(dir == 15) //LEFTBACK
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==1 || i==5 )  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i+1];
      }
      else if((i==3 || i==7 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, 4,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-3];
        return 1;
      }
      else if((i==0 || i==4 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,2,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+3];
        return 1;
      }
      else if((i==2 || i==6 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,15,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-1];
        return 1;
      }
    }
  }


  if(dir == 16) //RIGHTFRONT
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==2 || i==6 )  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i-1];
      }
      else if((i==0 || i==4 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,5,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+3];
        return 1;
      }
      else if((i==3 || i==7 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,3,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-3];
        return 1;
      }
      else if((i==1 || i==5 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,16,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+1];
        return 1;
      }
    }
  }


  if(dir == 17) //RIGHTBACK
  {
    if (q==mesh->octree->root) // q is root = q has no neighbour
    {
      *Neighbour=*mesh->octree->root;
      return 1;
    }

    for (i=0; i<8; i++)
    {
      if((i==0 || i==4 )  && q==&q->father->sons[i])
      {
        *Neighbour=q->father->sons[i+3];
      }
      else if((i==2 || i==6 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father, 4,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-1];
        return 1;
      }
      else if((i==1 || i==5 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,3,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i+1];
        return 1;
      }
      else if((i==3 || i==7 )  && q==&q->father->sons[i])
      {
        MMG3D_find_Neighbour_of_Bigger_or_Equal_Size(mesh,q->father,17,Temp_Neighbour);
        if(Temp_Neighbour->depth==0 || Temp_Neighbour->leaf==1)
        {
          *Neighbour=*Temp_Neighbour;
          return 1;
        }
        *Neighbour=Temp_Neighbour->sons[i-3];
        return 1;
      }
    }
  }
  return 0;
}

/**
 * \param mesh pointer toward the mesh
 *
 *
 * Delete unused points of the coarse grid.
 *
 */
void  MMG3D_del_UnusedPoints ( MMG5_pMesh mesh) {
  int i;

  for(i=0 ; i<mesh->np ; i++)
  {
    if(mesh->point[i].ref != 22)
    {
      MMG3D_delPt(mesh,i);
    }
    else
    {
      mesh->point[i].ref = 0;
    }
  }
}


/**
 * \param mesh pointer toward the mesh
 * \param ip_bb_pt_list pointer toward the list of index of the bounding box points
 * \param ip_bb_elt_list pointer toward the list of index of the bounding box elements
 *
 *
 * Create the points of the bounding box and its 5 tetrahedrons. The bounding box is 3/2 times bigger than the
 * initial grid.
 *
 */
void  MMG3D_build_bounding_box ( MMG5_pMesh mesh, int* ip_bb_pt_list, int* ip_bb_elt_list) {
  double         o[3];
  double origin_x = mesh->info.min[0];
  double origin_y = mesh->info.min[1];
  double origin_z = mesh->info.min[2];
  double a = mesh->info.max[0] * (double)mesh->freeint[0];
  double b = mesh->info.max[1] * (double)mesh->freeint[1];
  double c = mesh->info.max[2] * (double)mesh->freeint[2];

  //point 0
  o[0] = origin_x-a*0.5;
  o[1] = origin_y-b*0.5;
  o[2] = origin_z-c*0.5;
  *(ip_bb_pt_list+0) = MMG3D_newPt(mesh,o,MG_NOTAG);

  //point 1
  o[0] = origin_x+a*1.5;
  o[1] = origin_y-b*0.5;
  o[2] = origin_z-c*0.5;
  *(ip_bb_pt_list+1) = MMG3D_newPt(mesh,o,MG_NOTAG);

  //point 2
  o[0] = origin_x-a*0.5;
  o[1] = origin_y+b*1.5;
  o[2] = origin_z-c*0.5;
  *(ip_bb_pt_list+2) = MMG3D_newPt(mesh,o,MG_NOTAG);

  //point 3
  o[0] = origin_x+a*1.5;
  o[1] = origin_y+b*1.5;
  o[2] = origin_z-c*0.5;
  *(ip_bb_pt_list+3) = MMG3D_newPt(mesh,o,MG_NOTAG);

  //point 4
  o[0] = origin_x-a*0.5;
  o[1] = origin_y-b*0.5;
  o[2] = origin_z+c*1.5;
  *(ip_bb_pt_list+4) = MMG3D_newPt(mesh,o,MG_NOTAG);

  //point 5
  o[0] = origin_x+a*1.5;
  o[1] = origin_y-b*0.5;
  o[2] = origin_z+c*1.5;
  *(ip_bb_pt_list+5) = MMG3D_newPt(mesh,o,MG_NOTAG);

  //point 6
  o[0] = origin_x-a*0.5;
  o[1] = origin_y+b*1.5;
  o[2] = origin_z+c*1.5;
  *(ip_bb_pt_list+6) = MMG3D_newPt(mesh,o,MG_NOTAG);

  //point 7
  o[0] = origin_x+a*1.5;
  o[1] = origin_y+b*1.5;
  o[2] = origin_z+c*1.5;
  *(ip_bb_pt_list+7) = MMG3D_newPt(mesh,o,MG_NOTAG);

  //tetra 0
  *(ip_bb_elt_list+0) = MMG3D_newElt(mesh);
  mesh->tetra[*(ip_bb_elt_list+0)].v[0] = *(ip_bb_pt_list+0);
  mesh->tetra[*(ip_bb_elt_list+0)].v[1] = *(ip_bb_pt_list+1);
  mesh->tetra[*(ip_bb_elt_list+0)].v[2] = *(ip_bb_pt_list+3);
  mesh->tetra[*(ip_bb_elt_list+0)].v[3] = *(ip_bb_pt_list+5);

  //tetra 1
  *(ip_bb_elt_list+1) = MMG3D_newElt(mesh);
  mesh->tetra[*(ip_bb_elt_list+1)].v[0] = *(ip_bb_pt_list+0);
  mesh->tetra[*(ip_bb_elt_list+1)].v[1] = *(ip_bb_pt_list+5);
  mesh->tetra[*(ip_bb_elt_list+1)].v[2] = *(ip_bb_pt_list+6);
  mesh->tetra[*(ip_bb_elt_list+1)].v[3] = *(ip_bb_pt_list+4);

  //tetra 2
  *(ip_bb_elt_list+2) = MMG3D_newElt(mesh);
  mesh->tetra[*(ip_bb_elt_list+2)].v[0] = *(ip_bb_pt_list+6);
  mesh->tetra[*(ip_bb_elt_list+2)].v[1] = *(ip_bb_pt_list+5);
  mesh->tetra[*(ip_bb_elt_list+2)].v[2] = *(ip_bb_pt_list+3);
  mesh->tetra[*(ip_bb_elt_list+2)].v[3] = *(ip_bb_pt_list+7);

  //tetra 3
  *(ip_bb_elt_list+3) = MMG3D_newElt(mesh);
  mesh->tetra[*(ip_bb_elt_list+3)].v[0] = *(ip_bb_pt_list+0);
  mesh->tetra[*(ip_bb_elt_list+3)].v[1] = *(ip_bb_pt_list+3);
  mesh->tetra[*(ip_bb_elt_list+3)].v[2] = *(ip_bb_pt_list+2);
  mesh->tetra[*(ip_bb_elt_list+3)].v[3] = *(ip_bb_pt_list+6);

  //tetra 4
  *(ip_bb_elt_list+4) = MMG3D_newElt(mesh);
  mesh->tetra[*(ip_bb_elt_list+4)].v[0] = *(ip_bb_pt_list+0);
  mesh->tetra[*(ip_bb_elt_list+4)].v[1] = *(ip_bb_pt_list+5);
  mesh->tetra[*(ip_bb_elt_list+4)].v[2] = *(ip_bb_pt_list+3);
  mesh->tetra[*(ip_bb_elt_list+4)].v[3] = *(ip_bb_pt_list+6);
}

/**
* \param mesh pointer toward the mesh structure.
* \param iel tetra index.
* \param ip point local index in \a iel.
*
* Finds if point ip is located in tetra iel by computing its barycentric values in tetra iel.
*
*/

int MMG3D_intetra(MMG5_pMesh mesh,int iel,int ip) {
 double bary[4],A[3],B[3],C[3],D[3],P[3],vol;
 int i;
 for (i=0; i<3;i++)
 {
   A[i]=mesh->point[mesh->tetra[iel].v[0]].c[i];
   B[i]=mesh->point[mesh->tetra[iel].v[1]].c[i];
   C[i]=mesh->point[mesh->tetra[iel].v[2]].c[i];
   D[i]=mesh->point[mesh->tetra[iel].v[3]].c[i];
   P[i]=mesh->point[ip].c[i];
 }
 vol=(A[0]-D[0])*(B[1]-D[1])*(C[2]-D[2])+(B[0]-D[0])*(C[1]-D[1])*(A[2]-D[2])
 +(C[0]-D[0])*(A[1]-D[1])*(B[2]-D[2])-(C[0]-D[0])*(B[1]-D[1])*(A[2]-D[2])
 -(A[0]-D[0])*(C[1]-D[1])*(B[2]-D[2])-(B[0]-D[0])*(A[1]-D[1])*(C[2]-D[2]);

 bary[0]=((P[0]-D[0])*(B[1]-D[1])*(C[2]-D[2])+(B[0]-D[0])*(C[1]-D[1])*(P[2]-D[2])
 +(C[0]-D[0])*(P[1]-D[1])*(B[2]-D[2])-(C[0]-D[0])*(B[1]-D[1])*(P[2]-D[2])
 -(P[0]-D[0])*(C[1]-D[1])*(B[2]-D[2])-(B[0]-D[0])*(P[1]-D[1])*(C[2]-D[2]))/vol;
 if(bary[0]<0 || bary[0]>1)
 {
   return 0;
 }

 bary[1]=((A[0]-D[0])*(P[1]-D[1])*(C[2]-D[2])+(P[0]-D[0])*(C[1]-D[1])*(A[2]-D[2])
 +(C[0]-D[0])*(A[1]-D[1])*(P[2]-D[2])-(C[0]-D[0])*(P[1]-D[1])*(A[2]-D[2])
 -(A[0]-D[0])*(C[1]-D[1])*(P[2]-D[2])-(P[0]-D[0])*(A[1]-D[1])*(C[2]-D[2]))/vol;
 if(bary[1]<0 || bary[1]>1)
 {
   return 0;
 }

 bary[2]=((A[0]-D[0])*(B[1]-D[1])*(P[2]-D[2])+(B[0]-D[0])*(P[1]-D[1])*(A[2]-D[2])
 +(P[0]-D[0])*(A[1]-D[1])*(B[2]-D[2])-(P[0]-D[0])*(B[1]-D[1])*(A[2]-D[2])
 -(A[0]-D[0])*(P[1]-D[1])*(B[2]-D[2])-(B[0]-D[0])*(A[1]-D[1])*(P[2]-D[2]))/vol;
 if(bary[2]<0 || bary[2]>1)
 {
   return 0;
 }

 bary[3]=1-bary[0]-bary[1]-bary[2];
 if(bary[3]<0 || bary[3]>1)
 {
   return 0;
 }
return 1;
}


/**
 * \param mesh pointer toward the mesh
 * \param q pointer toward the MOctree cell
 * \param face_border the number of the treated face
 * \param depth_max the maximum depth of the octree
 * \param listip pointer toward a list of the boundary points indexes
 * \param i pointer toward the index of the next ip to add in listip
 *
 *
 * Find the boundary points of a face and create a list which contains their ip.
 * \return 1 if success, 0 if fail.
 *
 */
int MMG3D_borders_delaunay( MMG5_pMesh mesh, MMG5_MOctree_s* q, int face_border, int depth_max, int *listip, int* i)
{
  //  face_border=1 for the front face
  //  face_border=2 for the back face
  //  face_border=3 for the left face
  //  face_border=4 for the right face
  //  face_border=5 for the up face
  //  face_border=6 for the down face

  int ip[8];
  int ncells_x = mesh->freeint[0];
  int ncells_y = mesh->freeint[1];
  int ncells_z = mesh->freeint[2];

  if (face_border==1)
  {
    if (q->leaf!=1)
    {
      if(q->sons[0].blf_ip==0)
      {
        if(q->sons[2].blf_ip!=0)
        {
        MMG3D_borders_delaunay( mesh, &q->sons[2], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[0], face_border, depth_max, listip, i);
      }


      if(q->sons[1].blf_ip==0)
      {
        if(q->sons[3].blf_ip!=0)
        {
          MMG3D_borders_delaunay( mesh, &q->sons[3], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[1], face_border, depth_max, listip, i);
      }


      if(q->sons[4].blf_ip==0)
      {
        if(q->sons[6].blf_ip!=0)
        {
        MMG3D_borders_delaunay( mesh, &q->sons[6], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[4], face_border, depth_max, listip, i);
      }

      if(q->sons[5].blf_ip==0)
      {
        if(q->sons[7].blf_ip!=0)
        {
          MMG3D_borders_delaunay( mesh, &q->sons[7], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[5], face_border, depth_max, listip, i);
      }

    }

    else
    {
      int span = pow(2,depth_max-(q->depth));
      if(q->coordoct[0] < ncells_x-1 && q->coordoct[1] < ncells_y-1 && q->coordoct[2] < ncells_z-1)
      {
        if ( !MMG3D_get_MOctreeCornerIndices ( mesh,q,span,ip,ip+1,ip+2,ip+3,ip+4,ip+5,ip+6,ip+7 ) ) {
          fprintf(stderr,"\n  ## Error: %s: unable to compute the indices of the"
          " corners of the octree cell.\n",__func__);
          return 0;
        }
      }
  //    printf("FACE 1 %d %d %d %d %d %d %d %d %d\n", q->blf_ip, *ip, *(ip+1), *(ip+2), *(ip+3), *(ip+4), *(ip+5), *(ip+6), *(ip+7));
      if(mesh->point[ip[0]].ref!=33)
      {
        mesh->point[ip[0]].ref=33;
        *(listip+*i)=ip[0];
        *i=*i+1;
      }
      if(mesh->point[ip[1]].ref!=33)
      {
        mesh->point[ip[1]].ref=33;
        *(listip+*i)=ip[1];
        *i=*i+1;
      }
      if(mesh->point[ip[4]].ref!=33)
      {
        mesh->point[ip[4]].ref=33;
        *(listip+*i)=ip[4];
        *i=*i+1;
      }
      if(mesh->point[ip[5]].ref!=33)
      {
        mesh->point[ip[5]].ref=33;
        *(listip+*i)=ip[5];
        *i=*i+1;
      }
    }
  }

  else if (face_border==2)
  {
    if (q->leaf!=1)
    {

      if(q->sons[2].blf_ip==0)
      {
        if(q->sons[0].blf_ip!=0)
        {
        MMG3D_borders_delaunay( mesh, &q->sons[0], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[2], face_border, depth_max, listip, i);
      }


      if(q->sons[3].blf_ip==0)
      {
        if(q->sons[1].blf_ip!=0)
        {
          MMG3D_borders_delaunay( mesh, &q->sons[1], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[3], face_border, depth_max, listip, i);
      }


      if(q->sons[6].blf_ip==0)
      {
        if(q->sons[4].blf_ip!=0)
        {
        MMG3D_borders_delaunay( mesh, &q->sons[4], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[6], face_border, depth_max, listip, i);
      }

      if(q->sons[7].blf_ip==0)
      {
        if(q->sons[5].blf_ip!=0)
        {
          MMG3D_borders_delaunay( mesh, &q->sons[5], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[7], face_border, depth_max, listip, i);
      }

    }

    else
    {
      int span = pow(2,depth_max-(q->depth));
      if(q->coordoct[0] < ncells_x-1 && q->coordoct[1] < ncells_y-1 && q->coordoct[2] < ncells_z-1)
      {
        if ( !MMG3D_get_MOctreeCornerIndices ( mesh,q,span,ip,ip+1,ip+2,ip+3,ip+4,ip+5,ip+6,ip+7 ) ) {
          fprintf(stderr,"\n  ## Error: %s: unable to compute the indices of the"
          " corners of the octree cell.\n",__func__);
          return 0;
        }
      }
    //  printf("FACE 2 %d %d %d %d %d %d %d %d %d\n", q->blf_ip, *ip, *(ip+1), *(ip+2), *(ip+3), *(ip+4), *(ip+5), *(ip+6), *(ip+7));
      if(mesh->point[ip[2]].ref!=33)
      {
        mesh->point[ip[2]].ref=33;
        *(listip+*i)=ip[2];
        *i=*i+1;
      }
      if(mesh->point[ip[3]].ref!=33)
      {
        mesh->point[ip[3]].ref=33;
        *(listip+*i)=ip[3];
        *i=*i+1;
      }
      if(mesh->point[ip[6]].ref!=33)
      {
        mesh->point[ip[6]].ref=33;
        *(listip+*i)=ip[6];
        *i=*i+1;
      }
      if(mesh->point[ip[7]].ref!=33)
      {
        mesh->point[ip[7]].ref=33;
        *(listip+*i)=ip[7];
        *i=*i+1;
      }
    }
  }

  else if (face_border==3)
  {
    if (q->leaf!=1)
    {

      if(q->sons[0].blf_ip==0)
      {
        if(q->sons[1].blf_ip!=0)
        {
        MMG3D_borders_delaunay( mesh, &q->sons[1], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[0], face_border, depth_max, listip, i);
      }


      if(q->sons[2].blf_ip==0)
      {
        if(q->sons[3].blf_ip!=0)
        {
          MMG3D_borders_delaunay( mesh, &q->sons[3], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[2], face_border, depth_max, listip, i);
      }


      if(q->sons[4].blf_ip==0)
      {
        if(q->sons[5].blf_ip!=0)
        {
        MMG3D_borders_delaunay( mesh, &q->sons[5], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[4], face_border, depth_max, listip, i);
      }

      if(q->sons[6].blf_ip==0)
      {
        if(q->sons[7].blf_ip!=0)
        {
          MMG3D_borders_delaunay( mesh, &q->sons[7], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[6], face_border, depth_max, listip, i);
      }

    }

    else
    {
      int span = pow(2,depth_max-(q->depth));
      if(q->coordoct[0] < ncells_x-1 && q->coordoct[1] < ncells_y-1 && q->coordoct[2] < ncells_z-1)
      {
        if ( !MMG3D_get_MOctreeCornerIndices ( mesh,q,span,ip,ip+1,ip+2,ip+3,ip+4,ip+5,ip+6,ip+7 ) ) {
          fprintf(stderr,"\n  ## Error: %s: unable to compute the indices of the"
          " corners of the octree cell.\n",__func__);
          return 0;
        }
      }
    //  printf("FACE 3 %d %d %d %d %d %d %d %d %d\n", q->blf_ip, *ip, *(ip+1), *(ip+2), *(ip+3), *(ip+4), *(ip+5), *(ip+6), *(ip+7));
      if(mesh->point[ip[0]].ref!=33)
      {
        mesh->point[ip[0]].ref=33;
        *(listip+*i)=ip[0];
        *i=*i+1;
      }
      if(mesh->point[ip[4]].ref!=33)
      {
        mesh->point[ip[4]].ref=33;
        *(listip+*i)=ip[4];
        *i=*i+1;
      }
      if(mesh->point[ip[3]].ref!=33)
      {
        mesh->point[ip[3]].ref=33;
        *(listip+*i)=ip[3];
        *i=*i+1;
      }
      if(mesh->point[ip[7]].ref!=33)
      {
        mesh->point[ip[7]].ref=33;
        *(listip+*i)=ip[7];
        *i=*i+1;
      }
    }
  }
  else if (face_border==4)
  {
    if (q->leaf!=1)
    {
      if(q->sons[1].blf_ip==0)
      {
        if(q->sons[0].blf_ip!=0)
        {
        MMG3D_borders_delaunay( mesh, &q->sons[0], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[1], face_border, depth_max, listip, i);
      }


      if(q->sons[3].blf_ip==0)
      {
        if(q->sons[2].blf_ip!=0)
        {
          MMG3D_borders_delaunay( mesh, &q->sons[2], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[3], face_border, depth_max, listip, i);
      }


      if(q->sons[5].blf_ip==0)
      {
        if(q->sons[4].blf_ip!=0)
        {
        MMG3D_borders_delaunay( mesh, &q->sons[4], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[5], face_border, depth_max, listip, i);
      }

      if(q->sons[7].blf_ip==0)
      {
        if(q->sons[6].blf_ip!=0)
        {
          MMG3D_borders_delaunay( mesh, &q->sons[6], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[7], face_border, depth_max, listip, i);
      }

    }

    else
    {
      int span = pow(2,depth_max-(q->depth));
      if(q->coordoct[0] < ncells_x-1 && q->coordoct[1] < ncells_y-1 && q->coordoct[2] < ncells_z-1)
      {
        if ( !MMG3D_get_MOctreeCornerIndices ( mesh,q,span,ip,ip+1,ip+2,ip+3,ip+4,ip+5,ip+6,ip+7 ) ) {
          fprintf(stderr,"\n  ## Error: %s: unable to compute the indices of the"
          " corners of the octree cell.\n",__func__);
          return 0;
        }
      }
  //    printf("FACE 4 %d %d %d %d %d %d %d %d %d\n", q->blf_ip, *ip, *(ip+1), *(ip+2), *(ip+3), *(ip+4), *(ip+5), *(ip+6), *(ip+7));
      if(mesh->point[ip[2]].ref!=33)
      {
        mesh->point[ip[2]].ref=33;
        *(listip+*i)=ip[2];
        *i=*i+1;
      }
      if(mesh->point[ip[1]].ref!=33)
      {
        mesh->point[ip[1]].ref=33;
        *(listip+*i)=ip[1];
        *i=*i+1;
      }
      if(mesh->point[ip[6]].ref!=33)
      {
        mesh->point[ip[6]].ref=33;
        *(listip+*i)=ip[6];
        *i=*i+1;
      }
      if(mesh->point[ip[5]].ref!=33)
      {
        mesh->point[ip[5]].ref=33;
        *(listip+*i)=ip[5];
        *i=*i+1;
      }
    }
  }
  else if (face_border==5)
  {
    if (q->leaf!=1)
    {
      if(q->sons[4].blf_ip==0)
      {
        if(q->sons[0].blf_ip!=0)
        {
          MMG3D_borders_delaunay( mesh, &q->sons[0], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[4], face_border, depth_max, listip, i);
      }


      if(q->sons[5].blf_ip==0)
      {
        if(q->sons[1].blf_ip!=0)
        {
          MMG3D_borders_delaunay( mesh, &q->sons[1], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[5], face_border, depth_max, listip, i);
      }

      if(q->sons[6].blf_ip==0)
      {
        if(q->sons[2].blf_ip!=0)
        {
        MMG3D_borders_delaunay( mesh, &q->sons[2], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[6], face_border, depth_max, listip, i);
      }


      if(q->sons[7].blf_ip==0)
      {
        if(q->sons[3].blf_ip!=0)
        {
          MMG3D_borders_delaunay( mesh, &q->sons[3], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[7], face_border, depth_max, listip, i);
      }

    }

    else
    {
      int span = pow(2,depth_max-(q->depth));
      if(q->coordoct[0] < ncells_x-1 && q->coordoct[1] < ncells_y-1 && q->coordoct[2] < ncells_z-1)
      {
        if ( !MMG3D_get_MOctreeCornerIndices ( mesh,q,span,ip,ip+1,ip+2,ip+3,ip+4,ip+5,ip+6,ip+7 ) ) {
          fprintf(stderr,"\n  ## Error: %s: unable to compute the indices of the"
          " corners of the octree cell.\n",__func__);
          return 0;
        }
      }
    //    printf("FACE 5 %d %d %d %d %d %d %d %d %d\n", q->blf_ip, *ip, *(ip+1), *(ip+2), *(ip+3), *(ip+4), *(ip+5), *(ip+6), *(ip+7));
      if(mesh->point[ip[6]].ref!=33)
      {
        mesh->point[ip[6]].ref=33;
        *(listip+*i)=ip[6];
        *i=*i+1;
      }
      if(mesh->point[ip[7]].ref!=33)
      {
        mesh->point[ip[7]].ref=33;
        *(listip+*i)=ip[7];
        *i=*i+1;
      }
      if(mesh->point[ip[4]].ref!=33)
      {
        mesh->point[ip[4]].ref=33;
        *(listip+*i)=ip[4];
        *i=*i+1;
      }
      if(mesh->point[ip[5]].ref!=33)
      {
        mesh->point[ip[5]].ref=33;
        *(listip+*i)=ip[5];
        *i=*i+1;
      }
    }
  }
  else if (face_border==6)
  {
    if (q->leaf!=1)
    {

      if(q->sons[0].blf_ip==0)
      {
        if(q->sons[4].blf_ip!=0)
        {
        MMG3D_borders_delaunay( mesh, &q->sons[4], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[0], face_border, depth_max, listip, i);
      }


      if(q->sons[1].blf_ip==0)
      {
        if(q->sons[5].blf_ip!=0)
        {
          MMG3D_borders_delaunay( mesh, &q->sons[5], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[1], face_border, depth_max, listip, i);
      }


      if(q->sons[2].blf_ip==0)
      {
        if(q->sons[6].blf_ip!=0)
        {
        MMG3D_borders_delaunay( mesh, &q->sons[6], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[2], face_border, depth_max, listip, i);
      }

      if(q->sons[3].blf_ip==0)
      {
        if(q->sons[7].blf_ip!=0)
        {
          MMG3D_borders_delaunay( mesh, &q->sons[7], face_border, depth_max, listip, i);
        }
      }
      else
      {
        MMG3D_borders_delaunay( mesh, &q->sons[3], face_border, depth_max, listip, i);
      }

    }

    else
    {
      int span = pow(2,depth_max-(q->depth));
      if(q->coordoct[0] < ncells_x-1 && q->coordoct[1] < ncells_y-1 && q->coordoct[2] < ncells_z-1)
      {
        if ( !MMG3D_get_MOctreeCornerIndices ( mesh,q,span,ip,ip+1,ip+2,ip+3,ip+4,ip+5,ip+6,ip+7 ) ) {
          fprintf(stderr,"\n  ## Error: %s: unable to compute the indices of the"
          " corners of the octree cell.\n",__func__);
          return 0;
        }
      }
//      printf("FACE 6 %d %d %d %d %d %d %d %d %d\n", q->blf_ip, *ip, *(ip+1), *(ip+2), *(ip+3), *(ip+4), *(ip+5), *(ip+6), *(ip+7));
      if(mesh->point[ip[0]].ref!=33)
      {
        mesh->point[ip[0]].ref=33;
        *(listip+*i)=ip[0];
        *i=*i+1;
      }
      if(mesh->point[ip[1]].ref!=33)
      {
        mesh->point[ip[1]].ref=33;
        *(listip+*i)=ip[1];
        *i=*i+1;
      }
      if(mesh->point[ip[2]].ref!=33)
      {
        mesh->point[ip[2]].ref=33;
        *(listip+*i)=ip[2];
        *i=*i+1;
      }
      if(mesh->point[ip[3]].ref!=33)
      {
        mesh->point[ip[3]].ref=33;
        *(listip+*i)=ip[3];
        *i=*i+1;
      }
    }
  }
  return 1;
}

/**
 * \param mesh pointer toward the mesh
 * \param listip pointer toward a list of the boundary points indexes
 * \param i pointer toward the index of the next ip to add in listip
 *
 *
 * Find the boundary points of the whole grid and create a list which contains their ip.
 * \return 1 if success, 0 if fail.
 *
 */
int MMG3D_build_borders(MMG5_pMesh mesh, int* listip, int depth_max)
{
  MMG5_MOctree_s *po;

  po=mesh->octree->root;
  int p;
  int *i;
  p=0;
  i=&p;
  int j;
  for (j=1; j<=6;j++)
  {
    MMG3D_borders_delaunay( mesh, po,j, depth_max, listip, i);
  }

  return 1;
}

/**
 * \param mesh pointer toward the mesh structure.
 * \param iel tetra index.
 * \param ip point local index in \a iel.
 * \param list pointer toward the list of tetra in the shell of edge where
 * ip will be inserted.
 *
 * Mark elements in cavity, update the list of tetra in the cavity and return size of the list of tetra in the cavity.
 *
 */
int MMG3D_cavity_MOctree(MMG5_pMesh mesh ,int iel,int ip,int *list)
{
  MMG5_pPoint      ppt;
  MMG5_pTetra      tet,tet1,tetadj;
  double           c[3],crit,dd,eps,ray,ct[12];
  int             *adja,*adjb,k,adj,adi,voy,i,j,ilist,ipil,jel,iadr,base;
  int              vois[4],l;
  int              tref,isreq;
  int              lon=1;

  list[0]=iel;
  ilist=1;
  base = ++mesh->base;
  ppt = &mesh->point[ip];
  tet=&mesh->tetra[iel];
  eps   = MMG3D_EPSRAD*MMG3D_EPSRAD;
  ipil=0;

  do {
    tet1=&mesh->tetra[list[ipil]];
    if(tet1->flag!=base)
    {
      tet1->flag=base;
      iadr = (list[ipil]-1)*4 + 1;
      adja = &mesh->adja[iadr]; // on récupère le tétra adjacent par la première face du tétra --> si on connait l'adjacence par la première face, on retrouve les autres faces
      vois[0]  = adja[0]; // adjacent face 0
      vois[1]  = adja[1]; // adjacent face 1
      vois[2]  = adja[2]; // adjacent face 2
      vois[3]  = adja[3]; // adjacent face 3
      for (i=0; i<4; i++)
      {
        adj = vois[i]; // indice du tétra adjacent par la face i
        if ( !adj )  continue;

        adj >>= 2; // équivaut à diviser par 4 --> récupère l'indice du tétra voisin i
        tetadj  = &mesh->tetra[adj]; // pointe sur le tétra voisin i
        if ( tetadj->flag == base )  continue; // si on est bien à l'étape suivante avec ce tétra voisin i, on continue

        for (j=0,l=0; j<4; j++,l+=3)
        {
          memcpy(&ct[l],mesh->point[tetadj->v[j]].c,3*sizeof(double)); // copie les coordonnées des 4 points du voisin i dans ct (taille 12 = 4 points * 3 coords)
        }

        if ( !MMG5_cenrad_iso(mesh,ct,c,&ray) )  continue; // on cherche le centre et le rayon du cercle du tétra voisin i
        crit = eps * ray;

        /* Delaunay criterion */
        dd = (ppt->c[0] - c[0]) * (ppt->c[0] - c[0]) \
        + (ppt->c[1] - c[1]) * (ppt->c[1] - c[1]) \
        + (ppt->c[2] - c[2]) * (ppt->c[2] - c[2]);
        if ( dd > crit )  continue; // vérifie si les coordonnées du point à ajouter sont dans le cercle du tétra voisin i, si ce n'est pas le cas on continue

        /*store tetra*/
        tetadj->flag = base;
        list[ilist] = adj;
        ilist++;
      }
    }
    ipil++;
  }
  while ( ipil < ilist );

  return ilist;
}



/**
* \param mesh pointer toward the mesh
*
*
* Add the boundary points to the mesh (delaunay).
*
*/
int  MMG3D_add_Boundary ( MMG5_pMesh mesh, MMG5_pSol sol, int depth_max) {

  int i,j,ilist;
  int* list_cavity = NULL;
  int* listip= NULL;
  int list_size;
  int k,ier;
  list_size= 2*mesh->freeint[0]*mesh->freeint[1]+2*mesh->freeint[0]*mesh->freeint[2]+2*mesh->freeint[1]*mesh->freeint[2];
  list_cavity=(int*)malloc(list_size*sizeof(int));
  listip=(int*)malloc(list_size*sizeof(int));
  int init_list;
  init_list=2*mesh->freeint[0]*mesh->freeint[1]*mesh->freeint[2];
  //  printf("valeur de p avant initialisation = %ld\n",listip);
  for (k=0; k<list_size;k++)
  {
    *(listip+k)=init_list;
  }

  MMG3D_build_borders(mesh,listip, depth_max);

  i=0;
  while(*(listip+i) < 2*mesh->freeint[0]*mesh->freeint[1]*mesh->freeint[2]-1)
  {
    j=1;
    while(j <= mesh->ne && MMG3D_intetra(mesh,j,*(listip+i))==0)
    {
      j++;
    }
    if(j > mesh->ne)
    {
      fprintf(stdout,"\n  ** Le point d'ip %d n'appartient à aucun tétraèdre.\n", *(listip+i));
      return 0;
    }

    ilist=MMG3D_cavity_MOctree(mesh, j, *(listip+i), list_cavity);
    if ( ilist <= 0 ) {
      printf("  ## Error: %s: unable to compute the cavity of point %d.\n",__func__,*(listip+i));
      return 0;
    }

    ier = MMG5_delone_MOctree(mesh, sol, *(listip+i), list_cavity, ilist);
    if ( ier <= 0 ) {
      printf("  ## Error: %s: unable to insert point %d.\n",__func__,*(listip+i));
      return 0;
    }

    mesh->point[*(listip+i)].ref=44;
    i++;
  }
  free(listip);

  return 1;
}

int MMG5_delone_MOctree(MMG5_pMesh mesh,MMG5_pSol sol,int ip,int *list,int ilist) {
  MMG5_pPoint   ppt;
  MMG5_pTetra   pt,pt1;
  MMG5_xTetra   xt;
  MMG5_pxTetra  pxt0;
  int          *adja,*adjb,i,j,k,l,m,iel,jel,old,v[3],iadr,base,size;
  int           vois[4],iadrold;
  short         i1;
  char          alert;
  int           isused = 0,ixt,ielnum[3*MMG3D_LONMAX+1],ll;
  MMG5_Hash    hedg;
#ifndef NDEBUG
  int tref;
#endif

  base = mesh->base;
  /* external faces */
  size = 0;
  for (k=0; k<ilist; k++) { //on parcourt tous les tétras dans la cavité
    old  = list[k]; //on prend le tétra k dans la liste
    pt1  = &mesh->tetra[old];
    iadr = (old-1)*4 + 1;
    adja = &mesh->adja[iadr]; // on récupère les adja du tétra considéré
    vois[0]  = adja[0] >> 2;
    vois[1]  = adja[1] >> 2;
    vois[2]  = adja[2] >> 2;
    vois[3]  = adja[3] >> 2;
    for (i=0; i<4; i++) {
      jel = vois[i];
      if ( (!jel) || mesh->tetra[jel].flag != base ) { //si l'adja n'existe pas ou n'a pas été traité
        for (j=0; j<3; j++) {
          i1  = MMG5_idir[i][j]; // idir[i]: vertices of face opposite to vertex i MMG5_idir[4][3] = { {1,2,3}, {0,3,2}, {0,1,3}, {0,2,1} };
          ppt = &mesh->point[ pt1->v[i1] ];
          ppt->tagdel |= MG_NOM;
        }
        size++;
      }
    }
  }
  /* check isolated vertex */
  alert = 0;
  for (k=0; k<ilist; k++) {
    old  = list[k];
    pt1  = &mesh->tetra[old];
    for (i=0; i<4; i++) {
      ppt = &mesh->point[ pt1->v[i] ];
      if ( !(ppt->tagdel & MG_NOM) )
      {
        alert = 1;
      }

    }
  }
  /* reset tag */
  for (k=0; k<ilist; k++) {
    old  = list[k];
    pt1  = &mesh->tetra[old];
    for (i=0; i<4; i++) {
      ppt = &mesh->point[ pt1->v[i] ];
      ppt->tagdel &= ~MG_NOM;
    }
  }
  if ( alert )  {return 0;}
  /* hash table params */
  if ( size > 3*MMG3D_LONMAX )  return 0;
  if ( !MMG5_hashNew(mesh,&hedg,size,3*size) ) { /*3*size suffit */
    fprintf(stderr,"\n  ## Error: %s: unable to complete mesh.\n",__func__);
    return -1;
  }

  /*tetra allocation : we create "size" tetra*/
  ielnum[0] = size;
  for (k=1 ; k<=size ; k++) {
    ielnum[k] = MMG3D_newElt(mesh); //newElt renvoie l'indice du dernier tétra créé

    if ( !ielnum[k] ) {
      MMG3D_TETRA_REALLOC(mesh,ielnum[k],mesh->gap,
                          fprintf(stderr,"\n  ## Error: %s: unable to allocate a"
                                  " new element.\n",__func__);
                          for(ll=1 ; ll<k ; ll++) {
                            mesh->tetra[ielnum[ll]].v[0] = 1;
                            if ( !MMG3D_delElt(mesh,ielnum[ll]) )  return -1;
                          }
                          return -1);
    }
  }

  size = 1;
  for (k=0; k<ilist; k++) {
    old  = list[k];

    iadrold = (old-1)*4 + 1;
    adja = &mesh->adja[iadrold];
    vois[0]  = adja[0];
    vois[1]  = adja[1];
    vois[2]  = adja[2];
    vois[3]  = adja[3];

    pt   = &mesh->tetra[old];
    if(pt->xt) {
      pxt0 = &mesh->xtetra[pt->xt];
      memcpy(&xt,pxt0,sizeof(MMG5_xTetra));
      isused=0;
      ixt = 1;
    } else {
      ixt = 0;
    }

    for (i=0; i<4; i++) {
      jel = vois[i] /4;
      j   = vois[i] % 4;

      /* external face */
      if ( !jel || (mesh->tetra[jel].flag != base) ) {
        iel = ielnum[size++];
        assert(iel);

        pt1 = &mesh->tetra[iel];
        memcpy(pt1,pt,sizeof(MMG5_Tetra));
        pt1->v[i] = ip;

        //pt1->qual = MMG5_orcal(mesh,sol,iel);
        pt1->ref = mesh->tetra[old].ref;
        pt1->mark = mesh->mark;
        iadr = (iel-1)*4 + 1;
        adjb = &mesh->adja[iadr];
        adjb[i] = adja[i];

        if(ixt) {
          if( xt.ref[i] || xt.ftag[i]) {
            if(!isused) {
              pt1->xt = pt->xt;
              pt->xt = 0;
              pxt0 = &mesh->xtetra[pt1->xt];
              memset(pxt0,0,sizeof(MMG5_xTetra));
              pxt0->ref[i]   = xt.ref[i] ; pxt0->ftag[i]  = xt.ftag[i];
              pxt0->edg[MMG5_iarf[i][0]] = xt.edg[MMG5_iarf[i][0]];
              pxt0->edg[MMG5_iarf[i][1]] = xt.edg[MMG5_iarf[i][1]];
              pxt0->edg[MMG5_iarf[i][2]] = xt.edg[MMG5_iarf[i][2]];
              pxt0->tag[MMG5_iarf[i][0]] = xt.tag[MMG5_iarf[i][0]];
              pxt0->tag[MMG5_iarf[i][1]] = xt.tag[MMG5_iarf[i][1]];
              pxt0->tag[MMG5_iarf[i][2]] = xt.tag[MMG5_iarf[i][2]];
              pxt0->ori = xt.ori;
              isused=1;
            } else {
              mesh->xt++;
              if ( mesh->xt > mesh->xtmax ) {
                MMG5_TAB_RECALLOC(mesh,mesh->xtetra,mesh->xtmax,0.2,MMG5_xTetra,
                                   "larger xtetra table",
                                   mesh->xt--;
                                   fprintf(stderr,"  Exit program.\n"); return -1;);
              }
              pt1->xt = mesh->xt;
              pxt0 = &mesh->xtetra[pt1->xt];
              pxt0->ref[i]   = xt.ref[i] ; pxt0->ftag[i]  = xt.ftag[i];
              pxt0->edg[MMG5_iarf[i][0]] = xt.edg[MMG5_iarf[i][0]];
              pxt0->edg[MMG5_iarf[i][1]] = xt.edg[MMG5_iarf[i][1]];
              pxt0->edg[MMG5_iarf[i][2]] = xt.edg[MMG5_iarf[i][2]];
              pxt0->tag[MMG5_iarf[i][0]] = xt.tag[MMG5_iarf[i][0]];
              pxt0->tag[MMG5_iarf[i][1]] = xt.tag[MMG5_iarf[i][1]];
              pxt0->tag[MMG5_iarf[i][2]] = xt.tag[MMG5_iarf[i][2]];
              pxt0->ori = xt.ori;
            }
          }
          else {
            pt1->xt = 0;
          }
        }

        if ( jel ) {
          iadr = (jel-1)*4 + 1;
          adjb = &mesh->adja[iadr];
          adjb[j] = iel*4 + i;
        }

        /* internal faces (p1,p2,ip) */
        for (j=0; j<4; j++) {
          if ( j != i ) {
            m = 0;
            for (l=0; l<3; l++)
              if ( pt1->v[ MMG5_idir[j][l] ] != ip ) {
                v[m] = pt1->v[ MMG5_idir[j][l] ];
                m++;
              }
            MMG5_hashEdgeDelone(mesh,&hedg,iel,j,v);
          }
        }
      }
    }
  }

  /* remove old tetra */
#ifndef NDEBUG
  tref = mesh->tetra[list[0]].ref;
#endif
  for (k=0; k<ilist; k++) {
    assert(tref==mesh->tetra[list[k]].ref);
    if ( !MMG3D_delElt(mesh,list[k]) ) return -1;
  }

  // ppt = &mesh->point[ip];
  // ppt->flag = mesh->flag;
  MMG5_DEL_MEM(mesh,hedg.item);
  return 1;
}
