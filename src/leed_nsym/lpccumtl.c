/************************************************************************
 * <FILENAME>
 *
 *  Copyright 1992-2014 Georg Held <g.held@reading.ac.uk>
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 * @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
 *
 * Changes:
 *11.07.03 
  file contains function:

  leed_par_cumulative_tl(mat Tmat, mat tl_0, real ux, real uy, real uz, 
             real energy, int l_max_t, int l_max_0)

 Calculate non-diagonal temperature dependent atomic scattering matrix
 according to the cumulants expansion (P. de Andres).

CHANGES:
GH/16.09.00 - Creation - copy from pctemtl and cumulants (PdA)
GH/22.09.00 - output is -kappa * Tmat (according to the definitions in 
              the P.DeAndres / D.A. King paper)
GH/23.09.00 - Convergence test is proportional to number of matrix elements
GH/03.10.00 - bug fix in set up of T_n (T=0): use RMATEL, IMATEL
GH/11.07.03 - bug fix in output of T_mat for T=0: multiply with (-kappa)

*********************************************************************/

#include <math.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "leed.h"


static const size_t NITER = 1000;
/* #define CONV_TEST 0.00000390625 */  /* to be multiplied by (l_max+1)^2 */
static const double CONV_TEST = 0.000001; /*!< to be multiplied by (l_max+1)^2 */

static int n_call = 0;
static int last_l = -1;

static mat Mx = NULL,   My = NULL,   Mz = NULL;
static mat MxMx = NULL, MyMy = NULL, MzMz = NULL;

/*!
 * Calculates non-diagonal temperature dependent atomic scattering matrix
 * according to the cumulants expansion (P. de Andres).
 *
 * See: P. de Andres, D. A. King, "Anisotropic and Anharmonic effects through
 * the t-matrix for Low-Energy Electron Diffraction (TMAT V1.1)",
 * Comp. Phys. Comm., sect. 2.4. the equation numbers refer to this paper.
 *
 * The output matrix (\p Tmat ) is the one described in the above paper
 * multiplied by \f$ \kappa = \sqrt{2E} \f$
 *
 * \param[output] Tmat Pointer to the output non-diagonal temperature
 * dependent scattering matrix into its first argument. If Tmat is \c NULL,
 * the structure will be created.
 * \param[in] tl_0 Pointer to matrix of scattering factors \f$ \textbf{tl} \f$ :
 * \f$ \sin(\delta l) \times \exp(i \delta l) \f$ for T = 0.
 * \f$ \textbf{tl}_0 \f$ will be stored before \p Tmat is calculated,
 * therefore \f$ \textbf{tl}_0 \f$ and \p Tmat can be equal.
 * \param ux Root mean sqaure of the anisotropic vibrational displacement
 * at a given temperature \f$ <(dx)^2>(T) \f$ in x.
 * \param uy Root mean sqaure of the anisotropic vibrational displacement
 * at a given temperature \f$ <(dy)^2>(T) \f$ in y.
 * \param uz Root mean sqaure of the anisotropic vibrational displacement
 * at a given temperature \f$ <(dz)^2>(T) \f$ in z.
 * \param[in] energy Real part of the energy in atomic units.
 * \param l_max_t Required \f$ l \f$ quantum number of output scattering matrix.
 * \param l_max_0 Maximum \f$ l \f$ quantum number of input scattering matrix.
 * \return Pointer to the non-diagonal temperature dependent scattering matrix
 * multiplied by \f$ kappa \f$ (see description).
 * \retval \c NULL if any error occurred and #EXIT_ON_ERROR is not defined.
 */
mat leed_par_cumulative_tl(mat Tmat, mat tl_0, real ux, real uy, real uz, 
             real energy, size_t l_max_t, size_t l_max_0)
{

  size_t l1, l2;
  size_t lm1, lm2;
  int m1, m2;
  size_t iaux, i_el;
  size_t i_iter;
  size_t l_max_2 = (l_max_t+1)*(l_max_t+1);    /* matrix dimension */

  real relerr_r, relerr_i;
  real ux2 = ux*ux, uy2 = uy*uy, uz2 = uz*uz;
  real kappa = cleed_real_sqrt(2. * energy);

  real pref, conv_test;

  mat tl_aux;                    /* backup of original scattering factors */
  mat T_n, T_acc;

  /* Ms */
  mat MxMxTn, MxTnMx, TnMxMx;
  mat MyMyTn, MyTnMy, TnMyMy;
  mat MzMzTn, MzTnMz, TnMzMz;

#if CONTROL
  CONTROL_MSG(CONTROL, "Enter function: \n"
              "\t(ux, uy, uz) = (%.3f, %.3f, %.3f) [au]; "
              "energy = %.3f H; lmax_t = %u, lmax_0 = %u\n",
              ux, uy, uz, energy, l_max_t, l_max_0);
  matshow(tl_0);
#endif

  /* Backup original scattering factors
   * and allocate output and dummy arrays.
   */
  tl_aux = T_n = T_acc = NULL;
  tl_aux = matcopy(tl_aux, tl_0);

  T_n   = matalloc(T_n,   l_max_2, l_max_2, NUM_COMPLEX);
  T_acc = matalloc(T_acc, l_max_2, l_max_2, NUM_COMPLEX);

  /* Make sure that l_max_0 is not greater than l_max_t. */
  if(l_max_0 > l_max_t)
  {
    l_max_0 = l_max_t;
    WARNING_MSG("input phase shifts are only used up to l_max = %d\n", l_max_0);
  }
  else if(l_max_0 < l_max_t)
  {
    WARNING_MSG("input phase shifts exist only up to l_max_0 = %u, "
                "for higher l (up to l_max = %u) they are set to zero.\n",
                l_max_0, l_max_t);
  }

  /* Set up zero order t matrix t^(0), write to T_n:
   * t^(0) = - (1./kappa) * Sin[phs_(l)] Exp[i phs_(l)]
   */

  /* lm1, lm2, are counters in natural order */
  for(lm1 = 1, l1 = 0; l1 <= l_max_0; l1 ++)
    for(m1 = -(int)l1; m1 <= (int)l1; m1 ++, lm1 ++)
      for(lm2 = 1, l2 = 0; l2 <= l_max_0; l2 ++)
        for(m2 = -(int)l2; m2 <= (int)l2; m2 ++, lm2 ++)
          if((l1 == l2) && (m1 == m2) )
          {
            *rmatel(lm1, lm2, T_n) = - tl_aux->rel[l1+1] / kappa;
            *imatel(lm1, lm2, T_n) = - tl_aux->iel[l1+1] / kappa;
          }

#if CONTROL_X
  CONTROL_MSG(CONTROL_X, "Tmat(T=0): \n");
  matshowabs(T_n);
#endif

  /* If T = 0, i.e. all ux, uy, and uz are zero, we are allready done.
   * return T_n * (-kappa).
   */
  if( (ux < GEO_TOLERANCE) && (uy < GEO_TOLERANCE) && (uz < GEO_TOLERANCE) )
  {
    Tmat = matcopy(Tmat, T_n);

    WARNING_MSG("All displacements are zero: return Tmat(T=0)\n");

    iaux = Tmat->cols * Tmat->rows;
    for(i_el = 1; i_el <= iaux; i_el ++)
    {
      Tmat->rel[i_el] *= -kappa;
      Tmat->iel[i_el] *= -kappa;
    } /* for i_el */

    return (Tmat);
  }

  /* Prepare matrices for iteration:
   * - Check if Mx, etc. have to be calculated.
   * - allocate T_n etc.
   *  - set T_n etc to their start values.
   */
  if( (n_call == 0) || (last_l != (int)l_max_t) )
  {
    CONTROL_MSG(CONTROL, "calculate Mx, etc. for l_max = %d\n", l_max_t);

    pc_mk_ms( &Mx, &My, &Mz, &MxMx, &MyMy, &MzMz, l_max_t);
  }

#if CONTROL_X
  CONTROL_MSG(CONTROL_X, "Mx: \n");
  matshow(Mx);
  CONTROL_MSG(CONTROL_X, "My: \n");
  matshow(My);
  CONTROL_MSG(CONTROL_X, "Mz: \n");
  matshow(Mz);
#endif

  MxMxTn = MxTnMx = TnMxMx = NULL;

  MxMxTn  =  matalloc(MxMxTn, l_max_2, l_max_2, NUM_COMPLEX);
  MxTnMx  =  matalloc(MxTnMx, l_max_2, l_max_2, NUM_COMPLEX);
  TnMxMx  =  matalloc(TnMxMx, l_max_2, l_max_2, NUM_COMPLEX);

  MyMyTn = MyTnMy = TnMyMy = NULL;

  MyMyTn  =  matalloc(MyMyTn, l_max_2, l_max_2, NUM_COMPLEX);
  MyTnMy  =  matalloc(MyTnMy, l_max_2, l_max_2, NUM_COMPLEX);
  TnMyMy  =  matalloc(TnMyMy, l_max_2, l_max_2, NUM_COMPLEX);

  MzMzTn = MzTnMz = TnMzMz = NULL;

  MzMzTn  =  matalloc(MzMzTn, l_max_2, l_max_2, NUM_COMPLEX);
  MzTnMz  =  matalloc(MzTnMz, l_max_2, l_max_2, NUM_COMPLEX);
  TnMzMz  =  matalloc(TnMzMz, l_max_2, l_max_2, NUM_COMPLEX);

  /* Iteration:
   * Tmat = S_n T(n)
   * T(n+1) = - kappa^2 / (2(n+1))  *
   *          S_(a=xyz) [ua^2 ( Ma*Ma*T(n) + T(n)*Ma*Ma - 2*Ma*T(n)*Ma]
   */

  /* Tmat(T=0) is start value of T_acc for recurrence (i_iter = 0) */
  T_acc = matcopy(T_acc, T_n);

  conv_test = CONV_TEST * l_max_2 * l_max_2;
  relerr_r = relerr_i = 2*conv_test;

  for(i_iter = 1;
      (i_iter < NITER) && ( (relerr_r > conv_test) || (relerr_i > conv_test) );
      i_iter ++)
  {
    /* Calculate products of T_n and Mxyz for later use.
     * T_n will be overwiritten with Tmat(n+1) afterwards.
     */

    MxTnMx = matmul(MxTnMx, T_n, Mx);
    MxTnMx = matmul(MxTnMx, Mx, MxTnMx);

    MxMxTn = matmul(MxMxTn, MxMx,T_n);
    TnMxMx = matmul(TnMxMx, T_n,MxMx);

#if CONTROL_X
    if (i_iter < 4)
    {
      CONTROL_MSG(CONTROL_X, "MxMxTn(%d):\n", i_iter-1);
      matshow(MxMxTn);
      CONTROL_MSG(CONTROL_X, "MxTnMx(%d):\n", i_iter-1);
      matshow(MxTnMx);
      CONTROL_MSG(CONTROL_X, "TnMxMx(%d):\n", i_iter-1);
      matshow(TnMxMx);
    }
#endif

    MyTnMy = matmul(MyTnMy, T_n, My);
    MyTnMy = matmul(MyTnMy, My, MyTnMy);

    MyMyTn = matmul(MyMyTn, MyMy,T_n);
    TnMyMy = matmul(TnMyMy, T_n,MyMy);

    MzTnMz = matmul(MzTnMz, T_n, Mz);
    MzTnMz = matmul(MzTnMz, Mz, MzTnMz);

    MzMzTn = matmul(MzMzTn, MzMz,T_n);
    TnMzMz = matmul(TnMzMz, T_n,MzMz);

#if CONTROL_X
    if (i_iter < 4)
    {
      CONTROL_MSG(CONTROL_X, "MzMzTn(%d):\n", i_iter-1);
      matshow(MzMzTn);
      CONTROL_MSG(CONTROL_X, "MzTnMz(%d):\n", i_iter-1);
      matshow(MzTnMz);
      CONTROL_MSG(CONTROL_X, "TnMzMz(%d):\n", i_iter-1);
      matshow(TnMzMz);
    }
#endif

    /* from here on replace T(n) by T(n+1) */
    pref = - kappa * kappa / i_iter;
    iaux = T_n->cols * T_n->rows;

    for(i_el = 1; i_el <= iaux; i_el ++)
    {
      /* Eq. 35 */
      T_n->rel[i_el] = pref * (
        ux2*(MxMxTn->rel[i_el] + TnMxMx->rel[i_el] - 2.*(MxTnMx->rel[i_el])) +
        uy2*(MyMyTn->rel[i_el] + TnMyMy->rel[i_el] - 2.*(MyTnMy->rel[i_el])) +
        uz2*(MzMzTn->rel[i_el] + TnMzMz->rel[i_el] - 2.*(MzTnMz->rel[i_el])) );

      T_n->iel[i_el] = pref * (
        ux2*(MxMxTn->iel[i_el] + TnMxMx->iel[i_el] - 2.*(MxTnMx->iel[i_el])) +
        uy2*(MyMyTn->iel[i_el] + TnMyMy->iel[i_el] - 2.*(MyTnMy->iel[i_el])) +
        uz2*(MzMzTn->iel[i_el] + TnMzMz->iel[i_el] - 2.*(MzTnMz->iel[i_el])) );

      /* Eq. 34 */
      T_acc->rel[i_el] += T_n->rel[i_el];
      T_acc->iel[i_el] += T_n->iel[i_el];
    }

    /* Calculate relative errors for im. and real parts
     * and check convergence of each element.
     */
    relerr_r = relerr_i = 0.;
    iaux = T_acc->cols * T_acc->rows;
 
    for(i_el = 1; i_el <= iaux; i_el ++)
    {
      if( ! IS_EQUAL_REAL(T_acc->rel[i_el], 0.))
      {
        relerr_r += cleed_real_fabs(T_n->rel[i_el] / T_acc->rel[i_el]);
      }
      if( ! IS_EQUAL_REAL(T_acc->iel[i_el], 0.))
      {
        relerr_i += cleed_real_fabs(T_n->iel[i_el] / T_acc->iel[i_el]);
      }
    } /* for i_el */

    CONTROL_MSG(CONTROL, "iteration No %u: rel. errors: (%.3e, %.3e) <> %.3e\n",
            i_iter, relerr_r, relerr_i, conv_test);
 
  } /* for i_iter */

  if (i_iter == NITER)
  {
    ERROR_MSG("No convergence after %u iterations\n", i_iter);
    ERROR_RETURN(NULL);
  }

  Tmat = matcopy(Tmat, T_acc);

  /* Tmat -> Tmat * kappa */
  iaux = Tmat->cols * Tmat->rows;
  for(i_el = 1; i_el <= iaux; i_el ++)
  {
    Tmat->rel[i_el] *= -kappa;
    Tmat->iel[i_el] *= -kappa;
  } /* for i_el */

  /* Prepare returning:
   * - update n_call and last_l
   * - free matrices
   */
  n_call ++;
  last_l = (int)l_max_t;

  matfree(tl_aux);
  matfree(T_n);
  matfree(T_acc);

  matfree(MxMxTn); matfree(MxTnMx); matfree(TnMxMx);
  matfree(MyMyTn); matfree(MyTnMy); matfree(TnMyMy);
  matfree(MzMzTn); matfree(MzTnMz); matfree(TnMzMz);

  CONTROL_MSG(CONTROL, "End of function \n");

  return (Tmat);
} /* end of function leed_par_cumulative_tl */
