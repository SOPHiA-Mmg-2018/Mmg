/* =============================================================================
**  This file is part of the mmg software package for the tetrahedral
**  mesh modification.
**  Copyright (c) Bx INP/Inria/UBordeaux/UPMC, 2004- .
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
 * \file common/API_functions.c
 * \brief C API functions definitions for MMG library.
 * \author Charles Dapogny (UPMC)
 * \author Cécile Dobrzynski (Bx INP/Inria/UBordeaux)
 * \author Pascal Frey (UPMC)
 * \author Algiane Froehly (Inria/UBordeaux)
 * \version 5
 * \date 03 2014
 * \copyright GNU Lesser General Public License.
 *
 * \note This file contains some internal functions for the API, see the \a
 * common/libmmgcommon.h, \a mmgs/libmmgs.h and \a mmg3d/libmmg3d.h header files
 * for the documentation of all the usefull user's API functions.
 *
 * C API for MMG library.
 *
 */

#include "mmgcommon.h"

/**
 * \param mesh pointer toward the mesh structure.
 *
 * Initialization of the input parameters (stored in the Info structure).
 *
 */
void _MMG5_Init_parameters(MMG5_pMesh mesh) {

  memset(&mesh->info,0, sizeof(MMG5_Info));

  /* default values for integers */
  /** MMG5_IPARAM_verbose = 1 */
  mesh->info.imprim   =  1;  /* [-1..10], Tune level of imprim */
  /** MMG*_IPARAM_iso = 0 */
  mesh->info.iso      =  0;  /* [0/1]    ,Turn on/off levelset meshing */
  /** MMG5_IPARAM_mem = -1 */
  mesh->info.mem      = -1;  /* [n/-1]   ,Set memory size to n Mbytes/keep the default value */
  /** MMG5_IPARAM_debug = 0 */
  mesh->info.ddebug   =  0;  /* [0/1]    ,Turn on/off debug mode */
  /** MMG5_IPARAM_npar = 0 */
  mesh->info.npar     =  0;  /* [n]      ,number of local parameters */
  /** MMG5_IPARAM_noinsert = 0 */
  mesh->info.noinsert =  0;  /* [0/1]    ,avoid/allow point insertion/deletion */
  /** MMG5_IPARAM_noswap = 0 */
  mesh->info.noswap   =  0;  /* [0/1]    ,avoid/allow edge or face flipping */
  /** MMG5_IPARAM_nomove = 0 */
  mesh->info.nomove   =  0;  /* [0/1]    ,avoid/allow point relocation */
  /** MMG5_IPARAM nmat = 0 */
  mesh->info.nmat = 0;  /* [n]    ,number of user-defined references */

  /* default values for doubles */
  /** MMG5_DPARAM_angleDetection = \ref _MMG5_ANGEDG */
  mesh->info.dhd      = _MMG5_ANGEDG;   /* angle detection; */
  /** MMG5_DPARAM_hmin = 0.001 \f$\times\f$ bounding box size; */
  mesh->info.hmin     = -1.;      /* minimal mesh size; */
  /** MMG5_DPARAM_hmax = double of the bounding box size */
  mesh->info.hmax     = -1.;      /* maximal mesh size; */
  /** MMG5_DPARAM_hsiz= -1. */
  mesh->info.hsiz     = -1.;      /* constant mesh size; */
  /** MMG5_DPARAM_hausd = 0.01 */
  mesh->info.hausd    = 0.01;     /* control Hausdorff */
  /** MMG5_DPARAM_hgrad = 1.3 */
  mesh->info.hgrad    = 0.26236426446;      /* control gradation; */

  /* default values for pointers */
  /** MMG5_PPARAM = NULL */
  mesh->info.mat = NULL;  /* list of user-defined references */

  /** MMG3D_IPARAM_lag = -1 used by mmg3d only but need to be negative in the
   * scaleMesh function */
  mesh->info.lag      = -1;

  /* initial value for memMax and gap */
  mesh->gap = 0.2;
  mesh->memMax = _MMG5_memSize();
  if ( mesh->memMax )
    /* maximal memory = 50% of total physical memory */
    mesh->memMax = mesh->memMax*50/100;
  else {
    /* default value = 800 MB */
    printf("  Maximum memory set to default value: %d MB.\n",_MMG5_MEMMAX);
    mesh->memMax = _MMG5_MEMMAX << 20;
  }

}


/**
 * \param mesh pointer toward the mesh structure.
 * \param sol pointer toward the sol structure.
 *
 * Initialize file names to their default values.
 *
 */
void MMG5_Init_fileNames(MMG5_pMesh mesh,MMG5_pSol sol
  ) {
  MMG5_Set_inputMeshName(mesh,"");
  MMG5_Set_outputMeshName(mesh,"");

  MMG5_Set_inputSolName(mesh,sol,"");
  MMG5_Set_outputSolName(mesh,sol,"");

  return;
}


/**
 * \param mesh pointer toward the mesh structure.
 * \param meshin input mesh name.
 * \return 1 if success, 0 if fail
 *
 * Set the name of input mesh.
 *
 */
int MMG5_Set_inputMeshName(MMG5_pMesh mesh, const char* meshin) {

  if ( mesh->namein ){
    _MMG5_DEL_MEM(mesh,mesh->namein,(strlen(mesh->namein)+1)*sizeof(char));
  }

  if ( strlen(meshin) ) {
    _MMG5_ADD_MEM(mesh,(strlen(meshin)+1)*sizeof(char),"input mesh name",
                  fprintf(stderr,"  Exit program.\n");
                  return 0);
    _MMG5_SAFE_CALLOC(mesh->namein,strlen(meshin)+1,char,0);
    strcpy(mesh->namein,meshin);
  }
  else {
    _MMG5_ADD_MEM(mesh,10*sizeof(char),"input mesh name",
                  fprintf(stderr,"  Exit program.\n");
                  return 0);
    _MMG5_SAFE_CALLOC(mesh->namein,10,char,0);
    strcpy(mesh->namein,"mesh.mesh");
    if ( (mesh->info.imprim > 5) || mesh->info.ddebug ) {
      fprintf(stderr,"\n  ## Warning: %s: no name given for input mesh.\n",__func__);
      fprintf(stderr,"              Use of default value \"mesh.mesh\".\n");
    }
  }
  return(1);
}

/**
 * \param mesh pointer toward the mesh structure.
 * \param sol pointer toward the sol structure.
 * \param solin name of the input solution file.
 * \return 1 if success, 0 if fail
 *
 * Set the name of input solution file.
 *
 */
int MMG5_Set_inputSolName(MMG5_pMesh mesh,MMG5_pSol sol, const char* solin) {
  char *ptr;

  if ( sol->namein )
    _MMG5_DEL_MEM(mesh,sol->namein,(strlen(sol->namein)+1)*sizeof(char));

  if ( strlen(solin) ) {
    _MMG5_ADD_MEM(mesh,(strlen(solin)+1)*sizeof(char),"input sol name",
                  fprintf(stderr,"  Exit program.\n");
                  return 0);
    _MMG5_SAFE_CALLOC(sol->namein,strlen(solin)+1,char,0);
    strcpy(sol->namein,solin);
  }
  else {
    if ( strlen(mesh->namein) ) {
      _MMG5_SAFE_CALLOC(sol->namein,strlen(mesh->namein)+1,char,0);
      strcpy(sol->namein,mesh->namein);
      ptr = strstr(sol->namein,".mesh");
      if ( ptr ) {
        /* the sol file is renamed with the meshfile without extension */
        *ptr = '\0';
        _MMG5_SAFE_REALLOC(sol->namein,(strlen(sol->namein)+1),char,
                           "input sol name",0);
      }
      _MMG5_ADD_MEM(mesh,(strlen(sol->namein)+1)*sizeof(char),"input sol name",
                    fprintf(stderr,"  Exit program.\n");
                    return 0);
    }
    else {
      _MMG5_ADD_MEM(mesh,9*sizeof(char),"input sol name",
                    fprintf(stderr,"  Exit program.\n");
                    return 0);
      _MMG5_SAFE_CALLOC(sol->namein,9,char,0);
      strcpy(sol->namein,"mesh.sol");
    }
  }
  return(1);
}

/**
 * \param mesh pointer toward the mesh structure.
 * \param meshout name of the output mesh file.
 * \return 1 if success, 0 if fail.
 *
 * Set the name of output mesh file.
 *
 */
int MMG5_Set_outputMeshName(MMG5_pMesh mesh, const char* meshout) {
  char *ptrMed,*ptrGmsh;

  ptrMed = ptrGmsh = NULL;

  if ( mesh->nameout )
    _MMG5_DEL_MEM(mesh,mesh->nameout,(strlen(mesh->nameout)+1)*sizeof(char));

  if ( strlen(meshout) ) {
    _MMG5_ADD_MEM(mesh,(strlen(meshout)+1)*sizeof(char),"output mesh name",
                  fprintf(stderr,"  Exit program.\n");
                  return 0);
    _MMG5_SAFE_CALLOC(mesh->nameout,strlen(meshout)+1,char,0);
    strcpy(mesh->nameout,meshout);
  }
  else {
    if ( strlen(mesh->namein) ) {
      _MMG5_ADD_MEM(mesh,(strlen(mesh->namein)+3)*sizeof(char),"output mesh name",
                    fprintf(stderr,"  Exit program.\n");
                    return 0);
      _MMG5_SAFE_CALLOC(mesh->nameout,strlen(mesh->namein)+3,char,0);
      strcpy(mesh->nameout,mesh->namein);

      /* medit format? */
      ptrMed = strstr(mesh->nameout,".mesh");
      if ( !ptrMed )
        /* Gmsh format? */
        ptrGmsh = strstr(mesh->nameout,".msh");

      if ( !ptrMed && !ptrGmsh ) {
        /* filename without extension */
        strcat(mesh->nameout,".o");
      }
      else if ( ptrMed ) {
        *ptrMed = '\0';
        strcat(mesh->nameout,".o.mesh");
      }
      else if ( ptrGmsh ) {
        *ptrGmsh = '\0';
        strcat(mesh->nameout,".o.msh");
      }

      ptrMed = strstr(mesh->namein,".meshb");
      ptrGmsh = strstr(mesh->namein,".mshb");
      if ( ptrMed || ptrGmsh ) {
        /* binary file */
        strcat(mesh->nameout,"b");
      }


    }
    else {
      _MMG5_ADD_MEM(mesh,12*sizeof(char),"output mesh name",
                    fprintf(stderr,"  Exit program.\n");
                    return 0);
      _MMG5_SAFE_CALLOC(mesh->nameout,12,char,0);
      if ( (mesh->info.imprim > 5) || mesh->info.ddebug ) {
        fprintf(stderr,"\n  ## Warning: %s: no name given for output mesh.\n",
                __func__);
        fprintf(stderr,"              Use of default value \"mesh.o.mesh\".\n");
      }
      strcpy(mesh->nameout,"mesh.o.mesh");
    }
  }
  return(1);
}


/**
 * \param mesh pointer toward the mesh structure.
 * \param sol pointer toward the sol structure.
 * \param solout name of the output solution file.
 * \return 0 if failed, 1 otherwise.
 *
 *  Set the name of output solution file.
 *
 */
int MMG5_Set_outputSolName(MMG5_pMesh mesh,MMG5_pSol sol, const char* solout) {
  char *ptr;

  if ( sol->nameout )
    _MMG5_DEL_MEM(mesh,sol->nameout,(strlen(sol->nameout)+1)*sizeof(char));

  if ( strlen(solout) ) {
    _MMG5_ADD_MEM(mesh,(strlen(solout)+1)*sizeof(char),"output sol name",
                  fprintf(stderr,"  Exit program.\n");
                  return 0);
    _MMG5_SAFE_CALLOC(sol->nameout,strlen(solout)+1,char,0);
    strcpy(sol->nameout,solout);
  }
  else {
    if ( strlen(mesh->nameout) ) {
      ptr = strstr(mesh->nameout,".mesh");
      if ( ptr )
        _MMG5_SAFE_CALLOC(sol->nameout,strlen(mesh->nameout)+1,char,0);
      else
        _MMG5_SAFE_CALLOC(sol->nameout,strlen(mesh->nameout)+5,char,0);
      strcpy(sol->nameout,mesh->nameout);
      ptr = strstr(sol->nameout,".mesh");
      if ( ptr )
        /* the sol file is renamed with the meshfile without extension */
        *ptr = '\0';
      strcat(sol->nameout,".sol");
      _MMG5_ADD_MEM(mesh,(strlen(sol->nameout)+1)*sizeof(char),"output sol name",
                    fprintf(stderr,"  Exit program.\n");
                    return 0);
      _MMG5_SAFE_REALLOC(sol->nameout,(strlen(sol->nameout)+1),char,
                         "output sol name",0);
    }
    else {
      fprintf(stderr,"\n  ## Error: %s: no name for output mesh. please, use",
              __func__);
      fprintf(stderr," the MMG5_Set_outputMeshName to set the mesh name.\n");
      return(0);
    }
  }
  return(1);
}

/**
 * \param mesh pointer toward the mesh structure.
 * \param sol pointer toward the sol structure.
 *
 * Structures unallocation before return (common structures between all codes).
 *
 */
void MMG5_Free_structures(MMG5_pMesh mesh,MMG5_pSol sol){

  if ( mesh->point )
    _MMG5_DEL_MEM(mesh,mesh->point,(mesh->npmax+1)*sizeof(MMG5_Point));

  if ( mesh->xpoint )
    _MMG5_DEL_MEM(mesh,mesh->xpoint,(mesh->xpmax+1)*sizeof(MMG5_xPoint));

  if ( mesh->edge )
    _MMG5_DEL_MEM(mesh,mesh->edge,(mesh->na+1)*sizeof(MMG5_Edge));

  /* sol */
  if ( sol && sol->m )
    _MMG5_DEL_MEM(mesh,sol->m,(sol->size*(sol->npmax+1))*sizeof(double));

  /* mesh->info */
  if ( mesh->info.npar && mesh->info.par )
    _MMG5_DEL_MEM(mesh,mesh->info.par,mesh->info.npar*sizeof(MMG5_Par));

  if ( mesh->info.imprim>5 || mesh->info.ddebug ) {
    printf("  MEMORY USED AT END (Bytes) %lld\n",mesh->memCur);
  }

  return;
}

/**
 * \param mesh pointer toward the mesh structure.
 * \param met pointer toward the sol structure.
 *
 * File name deallocations before return.
 *
 */
void MMG5_mmgFree_names(MMG5_pMesh mesh,MMG5_pSol met){

  /* mesh */
  if ( mesh->nameout ) {
    _MMG5_DEL_MEM(mesh,mesh->nameout,(strlen(mesh->nameout)+1)*sizeof(char));
  }

  if ( mesh->namein ) {
    _MMG5_DEL_MEM(mesh,mesh->namein,(strlen(mesh->namein)+1)*sizeof(char));
  }

  /* met */
  if ( met ) {
    if ( met->namein ) {
      _MMG5_DEL_MEM(mesh,met->namein,(strlen(met->namein)+1)*sizeof(char));
    }

    if ( met->nameout ) {
      _MMG5_DEL_MEM(mesh,met->nameout,(strlen(met->nameout)+1)*sizeof(char));
    }
  }
}

inline
int MMG5_Set_defaultTruncatureSizes(MMG5_pMesh mesh,char sethmin,char sethmax) {

  if ( !sethmin ) {
    if ( sethmax ) {
      mesh->info.hmin  = MG_MIN(0.001,0.001 * mesh->info.hmax);
    } else {
      mesh->info.hmin  = 0.001;
    }
  }

  if ( !sethmax ) {
    if ( sethmin ) {
      mesh->info.hmax = MG_MAX(2.,100. * mesh->info.hmin);
    }
    else {
      mesh->info.hmax  = 2.;
    }
  }

  if ( mesh->info.hmax < mesh->info.hmin ) {
    assert ( sethmin && sethmax );
    fprintf(stderr,"\n  ## Error: %s: mismatch parameters:"
            " minimal mesh size larger than maximal one.\n",__func__);
    return 0;
  }

  return 1;
}

int MMG5_Compute_constantSize(MMG5_pMesh mesh,MMG5_pSol met,double *hsiz) {
  char         sethmin,sethmax;


  if ( mesh->info.hmin > mesh->info.hsiz ) {
    fprintf(stderr,"\n  ## Error: %s: Mismatched options: hmin (%e) is greater"
            " than hsiz (%e). Exit Program.\n",__func__,
            mesh->info.hmin,mesh->info.hsiz);
    return 0;
  }

  if ( mesh->info.hmax > 0. && mesh->info.hmax < mesh->info.hsiz ) {
    fprintf(stderr,"\n  ## Error: %s: Mismatched options: hmax (%e) is lower"
            " than hsiz (%e). Exit Program.\n",__func__,
            mesh->info.hmax,mesh->info.hsiz);
    return 0;
  }

  *hsiz = mesh->info.hsiz;

  sethmin = sethmax = 0;
  if ( mesh->info.hmin > 0. ) {
    sethmin = 1;
    *hsiz    =  MG_MAX(mesh->info.hmin,*hsiz);
  }

  if ( mesh->info.hmax > 0. ) {
    sethmax = 1;
    *hsiz    = MG_MIN(mesh->info.hmax,*hsiz);
  }

  /* Set hmin */
  if ( !sethmin ) {
    if ( sethmax ) {
      mesh->info.hmin  = MG_MIN(0.1*(*hsiz),0.1*mesh->info.hmax);
    } else {
      mesh->info.hmin  = 0.1*(*hsiz);
    }
  }

  /* Set hmax */
  if ( !sethmax ) {
    if ( sethmin ) {
      mesh->info.hmax  = MG_MAX(10.*(*hsiz),10.*mesh->info.hmin);
    } else {
      mesh->info.hmax  = 10.*(*hsiz);
    }
  }

  return 1;
}
