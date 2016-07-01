/***************************************************************************
                                  cg_cmm.h
                             -------------------
                            W. Michael Brown (ORNL)

  Class for acceleration of the lj/sdk pair style

 __________________________________________________________________________
    This file is part of the LAMMPS Accelerator Library (LAMMPS_AL)
 __________________________________________________________________________

    begin                :
    email                : brownw@ornl.gov
 ***************************************************************************/

#ifndef LAL_CG_CMM_H
#define LAL_CG_CMM_H

#include "lal_base_atomic.h"

namespace LAMMPS_AL {

template <class numtyp, class acctyp>
class CGCMM : public BaseAtomic<numtyp, acctyp> {
 public:
  CGCMM();
  ~CGCMM();

  /// Clear any previous data and set up for a new LAMMPS run
  /** \param max_nbors initial number of rows in the neighbor matrix
    * \param cell_size cutoff + skin
    * \param gpu_split fraction of particles handled by device
    *
    * Returns:
    * -  0 if successfull
    * - -1 if fix gpu not found
    * - -3 if there is an out of memory error
    * - -4 if the GPU library was not compiled for GPU
    * - -5 Double precision is not supported on card **/
  int init(const int ntypes, double **host_cutsq, int **host_cg_type,
           double **host_lj1, double **host_lj2, double **host_lj3,
           double **host_lj4, double **host_offset, double *host_special_lj,
           const int nlocal, const int nall, const int max_nbors,
           const int maxspecial, const double cell_size,
           const double gpu_split, FILE *screen);

  /// Clear all host and device data
  /** \note This is called at the beginning of the init() routine **/
  void clear();

  /// Returns memory usage on device per atom
  int bytes_per_atom(const int max_nbors) const;

  /// Total host memory used by library for pair style
  double host_memory_usage() const;

  // --------------------------- TYPE DATA --------------------------

  /// lj1.x = cutsq, lj1.y=cg_type, lj1.z = lj1, lj1.w = lj2
  UCL_D_Vec<numtyp4> lj1;
  /// lj3.x = lj3, lj3.y = lj4, lj3.z = offset
  UCL_D_Vec<numtyp4> lj3;
  /// Special LJ values
  UCL_D_Vec<numtyp> sp_lj;

  /// If atom type constants fit in shared memory, use fast kernels
  bool shared_types;

  /// Number of atom types
  int _cmm_types;

 private:
  bool _allocated;
  void loop(const bool _eflag, const bool _vflag);
};

}

#endif
