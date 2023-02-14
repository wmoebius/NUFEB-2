/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef COMPUTE_CLASS

ComputeStyle(nufeb/porosity,ComputePorosity)

#else

#ifndef LMP_COMPUTE_POROSITY_H
#define LMP_COMPUTE_POROSITY_H

#include "compute.h"

namespace LAMMPS_NS {

class ComputePorosity : public Compute {
 public:
  ComputePorosity(class LAMMPS *, int, char **);
  virtual ~ComputePorosity() {}
  virtual void init() {}
  virtual double compute_scalar();

  class AtomVecBacillus *avec;
};

}

#endif
#endif

/* ERROR/WARNING messages:
*/
