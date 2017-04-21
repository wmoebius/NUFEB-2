/*
 * fix_metabolism.h
 *
 *  Created on: 15 Aug 2016
 *      Author: bowen
 */

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

#include "fix_bio_diffusion.h"

#include <math.h>
#include <stddef.h>
#include <string.h>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

#include "Eigen/Eigen"
#include "unsupported/Eigen/KroneckerProduct"
#include "atom.h"
#include "domain.h"
#include "error.h"

#include "bio.h"
#include "fix_bio_kinetics.h"
#include "fix_bio_kinetics.h"
#include "force.h"
#include "input.h"
#include "lammps.h"
#include "modify.h"
#include "pointers.h"
#include "update.h"
#include "variable.h"

using namespace LAMMPS_NS;
using namespace FixConst;

using namespace std;
using namespace Eigen;


/* ---------------------------------------------------------------------- */

FixDiffusion::FixDiffusion(LAMMPS *lmp, int narg, char **arg) : Fix(lmp, narg, arg)
{

  if (narg != 10) error->all(FLERR,"Not enough arguments in fix diffusion command");
  var = new char*[2];
  ivar = new int[2];

  if(strcmp(arg[3], "exp") == 0) sflag = 0;
  else if(strcmp(arg[3], "imp") == 0) sflag = 1;
  else error->all(FLERR,"Illegal PDE method command");

  for (int i = 0; i < 2; i++) {
    int n = strlen(&arg[4+i][2]) + 1;
    var[i] = new char[n];
    strcpy(var[i],&arg[4+i][2]);
  }

  //set boundary condition flag:
  //0=PERIODIC-PERIODIC,  1=DIRiCH-DIRICH, 2=NEU-DIRICH, 3=NEU-NEU, 4=DIRICH-NEU
  if(strcmp(arg[6], "pp") == 0) xbcflag = 0;
  else if(strcmp(arg[6], "dd") == 0) xbcflag = 1;
  else if(strcmp(arg[6], "nd") == 0) xbcflag = 2;
  else if(strcmp(arg[6], "nn") == 0) xbcflag = 3;
  else if(strcmp(arg[6], "dn") == 0) xbcflag = 4;
  else error->all(FLERR,"Illegal x-axis boundary condition command");

  if(strcmp(arg[7], "pp") == 0) ybcflag = 0;
  else if(strcmp(arg[7], "dd") == 0) ybcflag = 1;
  else if(strcmp(arg[7], "nd") == 0) ybcflag = 2;
  else if(strcmp(arg[7], "nn") == 0) ybcflag = 3;
  else if(strcmp(arg[7], "dn") == 0) ybcflag = 4;
  else error->all(FLERR,"Illegal y-axis boundary condition command");

  if(strcmp(arg[8], "pp") == 0) zbcflag = 0;
  else if(strcmp(arg[8], "dd") == 0) zbcflag = 1;
  else if(strcmp(arg[8], "nd") == 0) zbcflag = 2;
  else if(strcmp(arg[8], "nn") == 0) zbcflag = 3;
  else if(strcmp(arg[8], "dn") == 0) zbcflag = 4;
  else error->all(FLERR,"Illegal z-axis boundary condition command");

  rflag = 0;
  rstep = atoi(arg[9]);
  if (rstep > 1) rflag = 1;

}

/* ---------------------------------------------------------------------- */

FixDiffusion::~FixDiffusion()
{
  int i;
  for (i = 0; i < 2; i++) {
    delete [] var[i];
  }
  delete [] var;
  delete [] ivar;
  delete [] r;
  delete [] maxBC;
  delete [] prevS;
}

/* ---------------------------------------------------------------------- */

int FixDiffusion::setmask()
{
  int mask = 0;
  mask |= PRE_FORCE;
  return mask;
}

/* ---------------------------------------------------------------------- */

void FixDiffusion::init()
{
  if (!atom->radius_flag)
    error->all(FLERR,"Fix requires atom attribute diameter");

  for (int n = 0; n < 2; n++) {
    ivar[n] = input->variable->find(var[n]);
    if (ivar[n] < 0)
      error->all(FLERR,"Variable name for fix diffusion does not exist");
    if (!input->variable->equalstyle(ivar[n]))
      error->all(FLERR,"Variable for fix diffusion is invalid style");
  }


  // register fix kinetics with this class
  int nfix = modify->nfix;
  for (int j = 0; j < nfix; j++) {
    if (strcmp(modify->fix[j]->style,"kinetics") == 0) {
      kinetics = static_cast<FixKinetics *>(lmp->modify->fix[j]);
      break;
    }
  }

  if (kinetics == NULL)
    lmp->error->all(FLERR,"The fix kinetics command is required for running iBM simulation");

  bio = kinetics->bio;
  diffT = input->variable->compute_equal(ivar[0]);
  tol = input->variable->compute_equal(ivar[1]);

  //set grid size
  nx = kinetics->nx;
  ny = kinetics->ny;
  nz = kinetics->nz;
  ngrids=nx*ny*nz;
  nuS = kinetics->nuS;
  nuR = kinetics->nuR;

  nnus = bio->nnus;
  iniS = bio->iniS;
  diffCoeff = bio->diffCoeff;

  r = new double[nnus+1]();
  maxBC = new double[nnus+1]();
  prevS = new double[nnus+1]();

  //Get computational domain size
  if (domain->triclinic == 0) {
    xlo = domain->boxlo[0];
    xhi = domain->boxhi[0];
    ylo = domain->boxlo[1];
    yhi = domain->boxhi[1];
    zlo = domain->boxlo[2];
    zhi = domain->boxhi[2];
  }
  else {
    xlo = domain->boxlo_bound[0];
    xhi = domain->boxhi_bound[0];
    ylo = domain->boxlo_bound[1];
    yhi = domain->boxhi_bound[1];
    zlo = domain->boxlo_bound[2];
    zhi = domain->boxhi_bound[2];
  }

  double gridx = (xhi-xlo)/nx;
  double gridy = (yhi-ylo)/ny;
  double gridz = (zhi-zlo)/nz;

 // if (!isEuqal(gridx, gridy, gridz)) error->all(FLERR,"Grid is not cubic");
  grid = gridx;

  //inlet concentration, diffusion constant
  //and maximum boundary condition conc value
  for (int i = 1; i <= nnus; i++) {
    r[i] = diffCoeff[i]/(2 * gridx * gridx);

    double bc[6];
    for (int j = 0; j < 6; j++ ) {
      bc[j] = iniS[i][j+1];
    }
    maxBC[i] = *max_element(bc, bc+6);
  }

  LAP = laplacian_matrix_3d();
  //create identity matrix
  SparseMatrix<double> In(ngrids, ngrids);
  In.setIdentity();
  I = In;
}


/* ----------------------------------------------------------------------
  build 3d laplacian matrix
------------------------------------------------------------------------- */

SparseMatrix<double> FixDiffusion::laplacian_matrix_3d()
{
  VectorXi ex1(nx);
  ex1.setOnes();
  MatrixXi  ex (nx, 3);
  ex << ex1, -3*ex1, ex1;
  VectorXi dx(3);
  dx << -1, 0, 1;
  SparseMatrix<double> Lx = spdiags(ex, dx, nx, ny, 3);
  VectorXi ey1(nx);
  ey1.setOnes();
  MatrixXi  ey (nx, 3);
  ey << ey1, -3*ey1, ey1;
  VectorXi dy(3);
  dy << -1, 0, 1;
  SparseMatrix<double> Ly = spdiags(ex, dy, nx, ny, 3);
  SparseMatrix<double> Ix(nx, nx);
  Ix.setIdentity();
  SparseMatrix<double> Iy(ny, ny);
  Iy.setIdentity();

  SparseMatrix<double> L2a = kroneckerProduct(Iy, Lx);
  SparseMatrix<double> L2b = kroneckerProduct(Ly, Ix);
  SparseMatrix<double> L2 = L2a + L2b;

  VectorXi ez1(ngrids);
  ez1.setOnes();
  MatrixXi  ez (ngrids, 2);
  ez << ez1, ez1;
  VectorXi dz(2);
  dz << -nx*ny, nx*ny;
  SparseMatrix<double> L = spdiags(ez, dz, ngrids, ngrids, 2);

  SparseMatrix<double> Iz(nz, nz);
  Iz.setIdentity();
  SparseMatrix<double> Aa = kroneckerProduct(Iz, L2);

  SparseMatrix<double> A = Aa + L;

  return A;
}

/* ----------------------------------------------------------------------
  build 2d laplacian matrix
------------------------------------------------------------------------- */

SparseMatrix<double> FixDiffusion::laplacian_matrix_2d()
{
  VectorXi e(nx);
  e.setOnes();

  MatrixXi  ex (nx, 3);
  ex << e, -2*e, e;
  VectorXi dx(3);
  dx << -1, 0, 1;
  SparseMatrix<double> spe = spdiags(ex, dx, nx, nx, 3);

  SparseMatrix<double> Iz(nz, nz);
  Iz.setIdentity();

  SparseMatrix<double> sp1 = kroneckerProduct(Iz, spe);
  SparseMatrix<double> sp2 = kroneckerProduct(spe, Iz);
  SparseMatrix<double> A = sp1 + sp2;

  return A;
}

/* ----------------------------------------------------------------------
  extract and create sparse band and diagonal matrices
------------------------------------------------------------------------- */

SparseMatrix<double> FixDiffusion::spdiags(MatrixXi& B, VectorXi& d, int m, int n, int size_d)
{
  SparseMatrix<double> A(m,n);

  for (int k = 0; k < size_d; k++) {
    int i_min = max(0, (int)(-d(k)));
    int i_max = min(m - 1, (int)(n - d(k) - 1));
    int B_idx_start = m >= n ? d(k) : 0;

    for (int i = i_min; i <= i_max; i++) {
     A.insert(i, (double)(i+d(k))) = B(B_idx_start + i, k);
    }
  }
  return A;
}

/* ----------------------------------------------------------------------
  solve diffusion equations and metabolism
------------------------------------------------------------------------- */

void FixDiffusion::diffusion()
{
  int iteration = 0;
  bool isConv = false;
  bool* conv = new bool[nnus+1]();
  VectorXd* vecS = new VectorXd[nnus+1];
  VectorXd* vecR = new VectorXd[nnus+1];
  VectorXd* nRES = new VectorXd[nnus+1];
//  double testMax = 0;

  nuS = kinetics->nuS;
  nuR = kinetics->nuR;

  //initialization
  for (int i = 1; i <= nnus; i++) {
    //convert concentration and consumption data types to Eigen vector type
    vecS[i] = Map<VectorXd> (nuS[i], ngrids, 1);
    vecR[i] = Map<VectorXd> (nuR[i], ngrids, 1);
    nRES[i] = VectorXd(ngrids);
    nRES[i].setZero();
    conv [i] = false;
  }

  VectorXd BC(ngrids);
  VectorXd RES(ngrids);

  SparseMatrix<double> sdiagBC;
  MatrixXd B;
  MatrixXd A;
  MatrixXd diagBC;
  VectorXd vecPrvS;

  while (!isConv) {
    iteration ++;
    isConv = true;

    for (int i = 1; i <= nnus; i++) {
      if (bio->nuType[i] == 0 && diffCoeff[i] != 0) {
        if (!conv[i]) {
          xbcm = iniS[i][1];
          xbcp = iniS[i][2];
          ybcm = iniS[i][3];
          ybcp = iniS[i][4];
          zbcm = iniS[i][5];
          zbcp = iniS[i][6];
          BC = bc_vec(vecS[i], grid);
          double max = 0.0;
          //Explicit method
          if (sflag == 0) {
            RES = LAP * vecS[i] + BC;
            RES = (2 * r[i] * RES + vecR[i]) * diffT;
            //decide
            if (rflag == 1) {
              if (iteration % rstep != 0) {
                nRES[i] = nRES[i] + RES;
              } else{
                max = nRES[i].array().abs().maxCoeff();
                nRES[i].setZero();
              }
            } else {
              max = RES.array().abs().maxCoeff();
            }

            vecS[i] = RES + vecS[i];
          //Implicit method
          } else {
            vecPrvS = vecS[i].replicate(1, 1);
            diagBC = BC.asDiagonal();
            sdiagBC = diagBC.sparseView();

            B = ((r[i] * LAP + I) * vecS[i] + r[i] * BC +  vecR[i]) * diffT;
            A = I - r[i] * LAP * diffT - r[i] * sdiagBC * diffT;
            vecS[i] = A.colPivHouseholderQr().solve(B);

            VectorXd vecDiffS = vecS[i] - vecPrvS;
            max = vecDiffS.array().abs().maxCoeff();
          }

          if (iteration % rstep == 0) {
            //test code
//            double maxS = 0.0;
//            maxS =  vecS[i].array().abs().maxCoeff();
//
//            if ((maxS > testMax) && strcmp("no2", bio->nuName[i]) == 0) {
//              testMax = maxS;
//            }

            // if prevS is zero, use maxS in current step
            if (prevS[i] == 0) {
              double max = vecS[i].array().abs().maxCoeff();
              if (max != 0) prevS[i] = max;
              else prevS[i] = 1;
            }

            double ratio = max / prevS[i];

            if (ratio < tol)  {
              conv[i] = true;
            } else {
              isConv = false;
            }
          } else {
            isConv = false;
          }
        }
      }
    }
    if (iteration > 500,000) {
      lmp->error->warning(FLERR,"# of iterations exceed 500,000, force to converge.");
    }
  }

  cout << "number of iteration: " << iteration << endl;
//  cout << "max NO2: " << testMax << endl;

  //convert concentration vector into normal data type
  for (int i = 1; i <= nnus; i++) {
    //take the maximum S
    prevS[i] = vecS[i].array().abs().maxCoeff();
    for (int j = 0; j < ngrids; j++) {
      if (vecS[i][j] < 0) vecS[i][j] = 1e-20;
    }
    Map<MatrixXd>(nuS[i], vecS[i].rows(), vecS[i].cols()) =  vecS[i];
  }

  //test();
  delete [] conv;
  delete [] vecS;
  delete [] vecR;
  delete [] nRES;
}

/* ----------------------------------------------------------------------
  build matrix of boundary condition
------------------------------------------------------------------------- */

VectorXd FixDiffusion::bc_vec(VectorXd& S, double h)
{
  VectorXd B(ngrids);
  B.setZero();
  int i_m;
  int i_p;

  //X-AXIS SURFACE
  for(int i = 1; i < ny +1; i++) {
    for(int j = 1; j < nz +1; j++) {
      int k = 1+(i-1)*nx+(j-1)*nx*ny;

      i_m = k-1 ;
      i_p = k+nx-2;
      //cout << i_m << " ip " << i_p << endl;
      if (xbcflag == 0) {
        B(i_m) = B(i_m)+S(i_p); //BOTTOM, X_MINUS
        B(i_p) = B(i_p)+S(i_m); //TOP, X_PLUS
      }
      else if (xbcflag == 1) {
        B(i_m) = B(i_m)+2*xbcm-S(i_m); //BOTTOM, X_MINUS
        B(i_p) = B(i_p)+2*xbcp-S(i_p); //TOP, X_PLUS
      }
      else if (xbcflag == 2) {
        B(i_m) = B(i_m)-h*xbcm+S(i_m); //BOTTOM,X_MINUS
        B(i_p) = B(i_p)+2*xbcp-S(i_p); //TOP, X_PLUS
      }
      else if (xbcflag == 3) {
        B(i_m) = B(i_m)-h*xbcm+S(i_m); //BOTTOM,X_MINUS
        B(i_p) = B(i_p)+h*xbcp+S(i_p); //TOP,X_PLUS
      }
      else if (xbcflag == 4) {
        B(i_m) = B(i_m)+2*xbcm-S(i_m); //BOTTOM, X_MINUS
        B(i_p) = B(i_p)+h*xbcp+S(i_p); //TOP,X_PLUS
      }
    }
  }

  //Y-AXIS SURFACE
  for(int i = 1; i < nz +1; i++) {
    i_p = (i)*nx*ny-nx;
    for (i_m = (i-1)*nx*ny; i_m <= (i-1)*nx*ny+nx-1; i_m++) {
      //error ip im?
      if (ybcflag == 0) {
        B(i_m) = B(i_m)+ S(i_p); //BOTTOM, Y_MINUS
        B(i_p)=B(i_p)+S(i_m); //TOP, Y_PLUS
      }
      else if (ybcflag == 1) {
        B(i_m)=B(i_m)+2*ybcm-S(i_m); //BOTTOM, Y_MINUS
        B(i_p)=B(i_p)+2*ybcp-S(i_p); //TOP, Y_PLUS
      }
      else if (ybcflag == 2) {
        B(i_m)=B(i_m)-h*ybcm+S(i_m); //BOTTOM,Y_MINUS
        B(i_p)=B(i_p)+2*ybcp-S(i_p); //TOP, Y_PLUS
      }
      else if (ybcflag == 3) {
        B(i_m)=B(i_m)-h*ybcm+S(i_m); //BOTTOM,Y_MINUS
        B(i_p)=B(i_p)+h*ybcp+S(i_p); //TOP,Y_PLUS
      }
      else if (ybcflag == 4) {
        B(i_m)=B(i_m)+2*ybcm-S(i_m); //BOTTOM, Y_MINUS
        B(i_p)=B(i_p)+h*ybcp+S(i_p); //TOP,Y_PLUS
      }
      i_p++;
    }
  }

  //Z-AXIS SURFACE
  i_p = nx*ny*(nz-1);
  for (i_m = 0; i_m < nx*ny; i_m++) {

    //error ip im?
    if (zbcflag == 0) {
      B(i_m)=B(i_m)+S(i_p); //BOTTOM, Z_MINUS
      B(i_p)=B(i_p)+S(i_m); //TOP, Z_PLUS
    }
    else if (zbcflag == 1) {
      B(i_m)=B(i_m)+2*zbcm-S(i_m); //BOTTOM, Z_MINUS
      B(i_p)=B(i_p)+2*zbcp-S(i_p); //TOP, Z_PLUS
    }
    else if (zbcflag == 2) {
      B(i_m)=B(i_m)-h*zbcm+S(i_m); //BOTTOM,Z_MINUS
      B(i_p)=B(i_p)+2*zbcp-S(i_p); //TOP, Z_PLUS
    }
    else if (zbcflag == 3) {
      B(i_m)=B(i_m)-h*zbcm+S(i_m); //BOTTOM,Z_MINUS
      B(i_p)=B(i_p)+h*zbcp+S(i_p); //TOP,Z_PLUS
    }
    else if (zbcflag == 4) {
      B(i_m)=B(i_m)+2*zbcm-S(i_m); //BOTTOM, Z_MINUS
      B(i_p)=B(i_p)+h*zbcp+S(i_p); //TOP,Z_PLUS
    }
    i_p++;
  }

  return B;
}

/* ----------------------------------------------------------------------
  compare double values for equality
------------------------------------------------------------------------- */

bool FixDiffusion::isEuqal(double a, double b, double c)
{
  double epsilon = 1e-10;
  if ((fabs(a - b) > epsilon)|| (fabs(a - b) > epsilon) || (fabs(a - b) > epsilon))
    return false;

  return true;
}

void FixDiffusion::test(){

  cout << nnus << endl;
  for (int i = 1; i <= nnus; i++) {
    cout << bio->nuName[i] << endl;
    for (int j = 0; j < ngrids; j++)
      cout << nuS[i][j] << endl;
  }
}