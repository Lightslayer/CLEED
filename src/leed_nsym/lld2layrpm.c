/*********************************************************************
 *                          LLD2LAYRPM.C
 *
 *  Copyright 1992-2014 Georg Held <g.held@reading.ac.uk>
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 * @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
 *
 * Changes:
 *   GH/26.01.95 - Creation: copied from leed_ld_2lay and modified
 *********************************************************************/

/*! \file
 *
 * Implements the calculations for the reflection matrix \f$ R_{ab}^{+-} \f$
 * using layer doubling.
 */

#include <math.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "leed.h"


/*!
 * Calculates only the reflection matrix R+- for a stack of two (super) layers
 * "a" (can be the complete bulk) and "b" (z(a) < z(b)) by layer doubling:
 *
 *  z(a) < z(b) => \c vec_ab[3] > 0 (otherwise no convergence!)
 *
 * \f[ \textbf{R}_{ab}^{+-} = \textbf{R}_b^{+-} +
 * (\textbf{T}_b^{++} \textbf{P}^+ \textbf{R}_a^{+-} \textbf{P}^-) \times
 * (\textbf{I} - \textbf{R}_b^{-+} \textbf{P}^+ \textbf{R}_a^{+-}
 * \textbf{P}^-)^{-1} \times \textbf{T}_b^{--} \f]
 *
 * \param[out] Rpm_ab Reflection matrix (+-) of the double layer ab
 * \f$ \textbf{R}_{ab}^{+-} \f$
 * \param[in] Rpm_a Reflection matrix (+-) of lower layer a
 * \f$ \textbf{R}_a^{+-} \f$
 * \param[in] Tpp_b Transmission matrix (++) of the upper layer b
 * \f$ \textbf{T}_b^{++} \f$
 * \param[in] Tmm_b Transmission matrix (--) of the upper layer b
 * \f$ \textbf{T}_b^{--} \f$
 * \param[in] Rpm_b Reflection matrix (+-) of the upper layer b
 * \f$ \textbf{R}_b^{+-} \f$
 * \param[in] Rmp_b Reflection matrix (-+) of the upper layer b
 * \f$ \textbf{R}_b^{-+} \f$
 * \param[in] beams Pointer to structure containing information about the
 * diffracted beams.
 * \param[in] vec_ab Pointer to a vector from the origin of layer a to the
 * origin of layer b. The usual indexing convention for vectors is used
 * (x=1, y=2, z=3).
 * \return Reflection matrix \f$ R_{ab}^{+-} \f$ for the dual atomic layers
 * a & b.
 * \note The returned reflection matrix is not necessarily identical to \p Rpm_ab
 */
mat leed_ld_2lay_rpm ( mat Rpm_ab,
                mat Rpm_a,
                mat Tpp_b,  mat Tmm_b,  mat Rpm_b,  mat Rmp_b,
                leed_beam *beams, real *vec_ab )
{
  int k;
  int n_beams, nn_beams;             /* total number of beams */

  real faux_r, faux_i;
  real *ptr_r, *ptr_i, *ptr_end;

  mat Pp, Pm, Maux_a, Maux_b;        /* temp. storage space */
  mat Res;                           /* result will be copied to Rpm_ab */


  Res = Pp = Pm = Maux_a = Maux_b = NULL;

/*************************************************************************
  Allocate memory and set up propagators Pp and Pm. 

  Pp = exp[ i *( k_x*v_ab_x + k_y*v_ab_y + k_z*v_ab_z) ]
  Pm = exp[-i *( k_x*v_ab_x + k_y*v_ab_y - k_z*v_ab_z) ]
     = exp[ i *(-k_x*v_ab_x - k_y*v_ab_y + k_z*v_ab_z) ]
*************************************************************************/
  n_beams = Rpm_a->cols;
  nn_beams = n_beams * n_beams;

  Pp = matalloc(NULL, n_beams, 1, NUM_COMPLEX );
  Pm = matalloc(NULL, n_beams, 1, NUM_COMPLEX );

  #ifdef CONTROL_X
  fprintf(STDCTR, "(leed_ld_2lay_rpm):vec_ab(%.2f %.2f %.2f)\n",
          vec_ab[1],vec_ab[2],vec_ab[3]);
  #endif

  for( k = 0; k < n_beams; k++)
  {
    #ifdef CONTROL
    fprintf(STDCTR, "ld: %2d: k_r = %5.2f %5.2f %5.2f\tk_i = %5.2f;",
        k, (beams+k)->k_r[1], (beams+k)->k_r[2], (beams+k)->k_r[3],
        (beams+k)->k_i[3]);
    #endif

    faux_r = (beams+k)->k_r[1] * vec_ab[1] +
             (beams+k)->k_r[2] * vec_ab[2] +
             (beams+k)->k_r[3] * vec_ab[3];
    faux_i = (beams+k)->k_i[3] * vec_ab[3];
   
    cri_expi(Pp->rel+k+1, Pp->iel+k+1, faux_r, faux_i);

    faux_r -= 2 * (beams+k)->k_r[3] * vec_ab[3];

    cri_expi(Pm->rel+k+1, Pm->iel+k+1, -faux_r, faux_i);

  }
 
  /*
   * Prepare the quantities (Ra+- P-) and  -(Rb-+ P+):
   * Multiply the k-th column of Ra+- / Rb-+ with the k-th element of P-/+.
   */
  Maux_a = matcopy(Maux_a, Rpm_a);
  Maux_b = matcopy(Maux_b, Rmp_b);

  for(k = 1; k <= n_beams; k ++)
  {
    faux_r = *(Pm->rel+k);
    faux_i = *(Pm->iel+k);

    ptr_end = Maux_a->rel+nn_beams;
    for (ptr_r = Maux_a->rel+k, ptr_i = Maux_a->iel+k;
         ptr_r <= ptr_end; ptr_r += n_beams,  ptr_i += n_beams)
    {
      cri_mul(ptr_r, ptr_i, *ptr_r, *ptr_i, faux_r, faux_i);
    }

    faux_r = - *(Pp->rel+k);
    faux_i = - *(Pp->iel+k);

    ptr_end = Maux_b->rel+nn_beams;
    for (ptr_r = Maux_b->rel+k, ptr_i = Maux_b->iel+k;
         ptr_r <= ptr_end; ptr_r += n_beams,  ptr_i += n_beams)
    {
      cri_mul(ptr_r, ptr_i, *ptr_r, *ptr_i, faux_r, faux_i);
    }
  }

  /*
   * (i) Calculate
   *     -(Rb-+ P+ Ra+- P-) = Maux_b * Maux_a (-> Maux_b)
   *     and add unity.
   *
   * (ii) Matrix inversion:
   *      Maux_b = ( I  - (Rb-+ P+ Ra+- P-))^(-1)
   *
   * (iii) Multiply with T--_b and store the result in Maux_b:
   *      Maux_b = ( I - (Rb-+ P+ Ra+- P-))^(-1) * Tb--
   *
   * (iv) Prepare Res:
   *     Res ->
   *        Ra+- P- * ( I - (Rb-+ P+ Ra+- P-))^(-1) * Tb-- = Maux_a * Maux_b
   */

  /* (i) */
  Maux_b = matmul(Maux_b, Maux_b, Maux_a);

  for(k = 1; k <= nn_beams; k+= Maux_b->cols + 1)
  {
    Maux_b->rel[k] += 1.;
  }

  /* (ii) */
  Maux_b = matinv(Maux_b, Maux_b);

  /* (iii) */
  Maux_b = matmul(Maux_b, Maux_b, Tmm_b);

  /* (iv) */
  Res = matmul(Res, Maux_a, Maux_b);

  /*
   * Prepare Maux_b = (Tb++ P+):
   * Multiply the k-th column of Ta-- / Tb++ with the k-th element of P-/+.
   */
  Maux_b = matcopy(Maux_b, Tpp_b);

  for(k = 1; k <= n_beams; k ++)
  {
    faux_r = *(Pp->rel+k);
    faux_i = *(Pp->iel+k);

    ptr_end = Maux_b->rel+nn_beams;
    for (ptr_r = Maux_b->rel+k, ptr_i = Maux_b->iel+k;
         ptr_r <= ptr_end;  ptr_r += n_beams, ptr_i += n_beams)
    {
      cri_mul(ptr_r, ptr_i, *ptr_r, *ptr_i, faux_r, faux_i);
    }
  }

  /*
   * (i) Complete the computation of the matrix product in Rab+- and Rab-+:
   *    Res = Maux_b * Res,
   *
   * (ii) Finally add the reflection matrix of a single layer to the matrix
   *    product in Rab+-:
   */

  /* (i) */
  Res = matmul(Res, Maux_b, Res);

  /* (ii) */
  ptr_end = Res->rel + nn_beams;
  for(ptr_r = Res->rel+1, ptr_i = Rpm_b->rel+1;
      ptr_r <= ptr_end; ptr_r ++, ptr_i ++)
  {
    *ptr_r += *ptr_i;
  }

  ptr_end = Res->iel + nn_beams;
  for(ptr_r = Res->iel+1, ptr_i = Rpm_b->iel+1;
      ptr_r <= ptr_end; ptr_r ++, ptr_i ++)
  {
    *ptr_r += *ptr_i;
  }

  /*
   * - Free temporary storage space,
   * - Write results to output pointers
   * - Return.
   */

  matfree(Pp);
  matfree(Pm);
  matfree(Maux_a);
  matfree(Maux_b);

  Rpm_ab = matcopy(Rpm_ab, Res);
  matfree(Res);

  return(Rpm_ab);
}