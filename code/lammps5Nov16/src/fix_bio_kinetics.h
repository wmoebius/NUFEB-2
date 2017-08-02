/*
 * fix_metabolism.h
 *
 *  Created on: 15 Aug 2016
 *      Author: bowen
 */

#ifdef FIX_CLASS

FixStyle(kinetics,FixKinetics)

#else

#ifndef SRC_FIX_KINETICS_H
#define SRC_FIX_KINETICS_H

#include "fix.h"

namespace LAMMPS_NS {

class FixKinetics : public Fix {
  friend class FixKineticsMonod;
  friend class FixKineticsThermo;
  friend class FixKineticsDiffusionS;
  friend class FixKineticsPH;
  friend class FixImmigration;
  friend class DumpBio;

 public:
  FixKinetics(class LAMMPS *, int, char **);
  ~FixKinetics();
  int setmask();
  virtual void pre_force(int);
  void init();

 private:
  char **var;
  int *ivar;

  int nx, ny, nz;                  // number of grids in x y z axis
  int ngrids;                      // # of grids
  double iph;                       // initial ph
  int nnus;                        // # of nutrients

  double **nuS;                    // nutrient concentration [nutrient][grid]
  double **nuR;                    // nutrient consumption [nutrient][grid]
  double **qGas;                   // gas chemicals [nutrient][grid]
  double **gYield;                 // inverse yield [type][grid]
  double ***activity;              // activities of chemical species [nutrient][5 charges][grid]
  double gVol, gasTrans;           // gas volume and gas transfer constant
  double temp, rth;                // uiversal gas constant (thermodynamics) and temperature
  double **DRGCat;                 // Gibbs free energy of catabolism [type][grid]
  double **DRGAn;                  // Gibbs free energy of anabolism [type][grid]
  double **kEq;                    // equilibrium constants [nutrient][4]
  double *Sh;
  bool *nuConv;
  double diffT;                    // diffsuion timestep

  class AtomVecBio *avec;
  class BIO *bio;
  class FixKineticsDiffusionS *diffusionS;
  class FixKineticsMonod *monod;
  class FixKineticsPH *ph;
  class FixKineticsThermo *thermo;

  void init_activity();
  void init_keq();
  void integration();
  void grow();
};

}

#endif
#endif

