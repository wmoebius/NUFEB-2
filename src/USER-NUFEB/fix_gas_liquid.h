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

#ifdef FIX_CLASS

FixStyle(nufeb/gas_liquid,FixGasLiquid)

#else

#ifndef LMP_FIX_GAS_LIQUID_H
#define LMP_FIX_GAS_LIQUID_H

#include "fix.h"

namespace LAMMPS_NS {

class FixGasLiquid : public Fix {
 public:
  int compute_flag;

  FixGasLiquid(class LAMMPS *, int, char **);
  virtual ~FixGasLiquid() {}
  int modify_param(int, char **);
  virtual int setmask();
  virtual void post_integrate();
  virtual double compute_scalar();
  virtual void compute();

 protected:
  int iliquid;
  int igas;

  double kga;
  double h;
  double temp;
  double reactor_vhead;
  double reactor_pres;
  double mw;
  double rg;

  double rtotal;
};

}

#endif
#endif

/* ERROR/WARNING messages:
*/
