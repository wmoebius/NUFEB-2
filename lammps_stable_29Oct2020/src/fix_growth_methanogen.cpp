/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include "fix_growth_methanogen.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include "atom.h"
#include "error.h"
#include "grid.h"
#include "group.h"
#include "grid_masks.h"
#include "math_const.h"
#include "comm.h"

#include <iostream>
#include <fstream>

using namespace LAMMPS_NS;
using namespace FixConst;
using namespace MathConst;

/* ---------------------------------------------------------------------- */

double vmax_h2 = 0.174;         // maximum uptake rate for H2 ( mol / s * gDW ) at pH 7.6 for M. formicicum

FixGrowthMethanogen::FixGrowthMethanogen(LAMMPS *lmp, int narg, char **arg) :
        FixGrowth(lmp, narg, arg)
{
  if (narg < 9)
    error->all(FLERR, "Illegal fix nufeb/growth/methanogen command");

  if (!grid->chemostat_flag)
    error->all(FLERR, "fix nufeb/growth/methanogen requires grid_style nufeb/chemostat");

  if (!atom->coccus_flag)
    error->all(FLERR, "fix nufeb/growth/methanogen requires atom_style coccus");

  ih2 = -1;
  ico2 = -1;
  ich4 = -1;

  h2_affinity = 0.0;      // Km for H2 (mol)
  co2_affinity = 0.0;     // Km for CO2 (mol)

  growth = 0.0;           // maximum growth rate (gDW/s)
  yield = 1.0;            // yield coefficient = cell growth rate / substrate uptake rate (gDW/mol)
  maintain = 0.0;         // maintenance coefficient (1/s)
  decay = 0.0;            // decay rate (gDW/s)
  eps_yield = 0.0;        // yield coefficient for EPS production (gDW/mol)
  eps_dens = 1.0;         // EPS density (gDW/L)

  ih2 = grid->find(arg[3]);
  if (ih2 < 0)
    error->all(FLERR, "Can't find H2");
  h2_affinity = utils::numeric(FLERR,arg[4],true,lmp);

  ico2 = grid->find(arg[5]);
  if (ico2 < 0)
    error->all(FLERR, "Can't find CO2");
  co2_affinity = utils::numeric(FLERR,arg[6],true,lmp);

  ich4 = grid->find(arg[7]);
  if (ich4 < 0)
    error->all(FLERR, "Can't find CH4");

  int iarg = 8;
  while (iarg < narg) {
    if (strcmp(arg[iarg], "growth") == 0) {
      growth = utils::numeric(FLERR,arg[iarg+1],true,lmp);
      iarg += 2;
    } else if (strcmp(arg[iarg], "yield") == 0) {
      yield = utils::numeric(FLERR,arg[iarg+1],true,lmp);
      iarg += 2;
    } else if (strcmp(arg[iarg], "maintain") == 0) {
      maintain = utils::numeric(FLERR,arg[iarg+1],true,lmp);
      iarg += 2;
    } else if (strcmp(arg[iarg], "decay") == 0) {
      decay = utils::numeric(FLERR,arg[iarg+1],true,lmp);
      iarg += 2;
    } else if (strcmp(arg[iarg], "epsyield") == 0) {
      eps_yield = utils::numeric(FLERR,arg[iarg+1],true,lmp);
      iarg += 2;
    } else if (strcmp(arg[iarg], "epsdens") == 0) {
      eps_dens = utils::numeric(FLERR,arg[iarg+1],true,lmp);
      iarg += 2;
    } else {
      error->all(FLERR, "Illegal fix nufeb/growth/methanogen command");
    }
  }
}

/* ---------------------------------------------------------------------- */

void FixGrowthMethanogen::update_cells()
{
  double **conc = grid->conc;     // grid concentrations
  double **reac = grid->reac;     // grid reactions
  double **dens = grid->dens;     // grid densities

  for (int i = 0; i < grid->ncells; i++) {
    // formicicum growth based on hydrogen and co2
    // 0.25 because 4H -> 1CH4
    double tmp1 = growth * (0.25 * vmax_h2 * conc[ih2][i] / (h2_affinity + conc[ih2][i])) * (
            conc[ico2][i] / (co2_affinity + conc[ico2][i]));

    // TODO - talk about this with Andreia
    // maintenance based on co2 or acetate?
    //double tmp2 = maintain * (conc[ico2][i] / (co2_affinity + conc[ico2][i]));

    if (!(grid->mask[i] & GHOST_MASK)) {    // only update non-ghost cells
      // nutrient utilization
      reac[ih2][i] -= 4 / yield * tmp1 * dens[igroup][i];
      reac[ico2][i] -= 1 / yield * tmp1 * dens[igroup][i];
      // methane evolution
      reac[ich4][i] += 1 / yield * tmp1 * dens[igroup][i];
      // water not considered
    }
  }
}

/* ---------------------------------------------------------------------- */

void FixGrowthMethanogen::update_atoms()
{
  double **x = atom->x;
  double *radius = atom->radius;
  double *rmass = atom->rmass;
  double *biomass = atom->biomass;
  double *outer_radius = atom->outer_radius;
  double *outer_mass = atom->outer_mass;
  double **conc = grid->conc;

  const double three_quarters_pi = (3.0 / (4.0 * MY_PI));
  const double four_thirds_pi = 4.0 * MY_PI / 3.0;
  const double third = 1.0 / 3.0;

  for (int i = 0; i < grid->ncells; i++) {
    // formicicum growth based on hydrogen and co2
    // 0.25 because 4H -> 1CH4
    double tmp1 = growth * (0.25 * vmax_h2 * conc[ih2][i] / (h2_affinity + conc[ih2][i])) * (
        conc[ico2][i] / (co2_affinity + conc[ico2][i]));

    grid->growth[igroup][i][0] = tmp1 - decay - maintain;
    grid->growth[igroup][i][1] = (eps_yield / yield) * tmp1;
  }

  for (int i = 0; i < atom->nlocal; i++) {
    if (atom->mask[i] & groupbit) {
      const int cell = grid->cell(x[i]);
      const double density = rmass[i] / (four_thirds_pi * radius[i] * radius[i] * radius[i]);
      // forward Euler to update biomass and rmass
      rmass[i] = rmass[i] * (1 + grid->growth[igroup][cell][0] * dt);
      outer_mass[i] = four_thirds_pi *
                      (outer_radius[i] * outer_radius[i] * outer_radius[i] -
                       radius[i] * radius[i] * radius[i]) *
                      eps_dens + grid->growth[igroup][cell][1] * rmass[i] * dt;
      radius[i] = pow(three_quarters_pi * (rmass[i] / density), third);
      outer_radius[i] = pow(three_quarters_pi * (rmass[i] / density + outer_mass[i] / eps_dens), third);
    }
  }
}