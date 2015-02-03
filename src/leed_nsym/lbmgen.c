/*********************************************************************
 *                           LBMGEN.C
 *
 *  Copyright 1992-2014 Georg Held <g.held@reading.ac.uk>
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 * @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
 *
 * Changes:
 *   GH/19.08.94 - Creation
 *   GH/01.03.95 - introduce g1, g2;
 *   GH/20.04.95 - include fractional order beams
 *   GH/02.09.97 - return value = number of beam sets
 *   WB/27.02.98 - change eng_max to eng_max - vr when calculating k_max
 *********************************************************************/

/*! \file
 *
 * Contains leed_beam_gen() function to setup lists of beams used in energy loop.
 */

#include <math.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>

#include "leed.h"

/*!
 * Sets up a list of all beams used within the energy loop.
 *
 * The order of the output list is:
 *  - increasing modulus of momentum transfer @leed_beam::k_par (lowest first)
 *  - increasing 1st index (lowest 1st index first for the same @leed_beam::k_par)
 *  - increasing 2nd index (lowest 2nd index first for the same 1st index
 *    and @leed_beam::k_par )
 *
 * \param[out] p_beams Pointer to the list of beams to be included at the
 * current energy.
 * \param[in,out] c_par Pointer to struct containing all the necessary
 * structural parameters (for details see leed_def.h )
 * \param[in,out] v_par Pointer to #leed_var struct containing all the
 * parameters that change during the energy loop (for details see leed_def.h ).
 * The parameters used are:
 *  - @leed_var::vr - the real part of the optical potential.
 *  - @leed_var::theta - used for incident k-vector
 *  - @leed_var::phi - used for incident k-vector
 *  - @leed_var::epsilon - paremeter determining the cutoff radius for
 *    @leed_beam::k_par (maximum amplitude which can propagate between two
 *    layers).
 * \param eng_max maximum energy for the energy loop in Hartees.
 * \return the number of beam sets in the list pointed to by \p p_beams
 * \retval -1 if failed
 */
int leed_beam_gen(leed_beam **p_beams, leed_crystal *c_par,
           leed_var *v_par, real eng_max)
{
  int iaux;
  int n1,n2;
  int n1_max,n2_max;
  int offset;

  int i_beams, i_set;
  int n_set;

  real faux_r, faux_i;
  real k_max, k_max_2;
  real a1, a2;

  real k_in[3];
  real k_r, k_i;

  real g1_x, g1_y, g2_x, g2_y;
  real k_x, k_y;
  real m11, m12, m21, m22;

  leed_beam *beams, beam_aux;
  leed_beam *bm_off;

  /*!
   * Allocate storage space
   * - Set eng_max to vacuum energy - optical potential
   * - Determine k_max (square of max k_par) from epsilon and dmin.
   * - Determine the max. number of beams within this radius (iaux)
   * and allocate memory for the beam list. (The formula given in
   * VHT p. 24 is not an upper limit. A prefactor 1/(PI*PI)
   * (0.10132118) is saver than 1/(4*PI) ).
   */

  eng_max -= v_par->vr;
  faux_r = R_log(v_par->epsilon) / c_par->dmin;
  k_max_2 = faux_r*faux_r + 2*eng_max;
  k_max = R_sqrt(k_max_2);

  iaux =  2 + (int)( 0.10132118 * c_par->rel_area_sup * c_par->area * k_max_2 );

  if (*p_beams == NULL)
  {
    *p_beams = (leed_beam *)calloc(iaux, sizeof(leed_beam));
  }
  else
  {
    *p_beams = (leed_beam *)realloc(*p_beams, iaux*sizeof(leed_beam));
  }

  if(*p_beams == NULL)
  {
    ERROR_MSG("allocation error.\n");
    exit(1);
  }
  else
  {
    beams = *p_beams;
  }

  CONTROL_MSG(CONTROL_X, "eng_max  = %.2f, vr = %.2e\n",
              eng_max * HART, v_par->vr * HART);
  CONTROL_MSG(CONTROL_X, "dmin  = %.2f, epsilon = %.2e\n",
                 c_par->dmin * BOHR, v_par->epsilon);
  CONTROL_MSG(CONTROL_X, "k_max = %.2f, max. No of beams = %2d\n", k_max, iaux);

  /*
   * Some often used values:
   * - indices of the basic superstructure vectors (mij)
   * - reciprocal (1x1) lattice vectors (g1/2_x/y)
   * - k_in at max. energy (k_in)
   */

  m11 = c_par->m_recip[1], m12 = c_par->m_recip[2];
  m21 = c_par->m_recip[3], m22 = c_par->m_recip[4];

  g1_x = c_par->a_1[1]; g1_y = c_par->a_1[2];
  g2_x = c_par->a_1[3]; g2_y = c_par->a_1[4];

  faux_r = R_sin(v_par->theta) * R_sqrt(2*eng_max);
  k_in[0] = faux_r;
  k_in[1] = faux_r * R_cos(v_par->phi);
  k_in[2] = faux_r * R_sin(v_par->phi);

  CONTROL_MSG(CONTROL_X, "a1 = (%.2f, %.2f)\ta2 = (%.2f, %.2f)\n",
                          g1_x, g1_y, g2_x, g2_y);

  /*
   * Determine number of beam sets (n_set)
   * and offsets (store in bm_off)
   * Each beam set is represented exactly once within the first BZ
   * (i.e. within the diamond: (0,0)(1,0)(0,1)(1,1))
   * => raster through the first BZ and store all fractional
   *    order beams in the array bm_off.
   */
 
  n_set = (int) R_nint(c_par->rel_area_sup);
  bm_off = (leed_beam *)calloc(n_set, sizeof(leed_beam));

  (bm_off+0)->ind_1 = 0.;
  (bm_off+0)->ind_2 = 0.;
  (bm_off+0)->k_r[1] = 0.;
  (bm_off+0)->k_r[2] = 0.;

  CONTROL_MSG(CONTROL_X, "set %d: %5.2f %5.2f (%5.2f %5.2f)\n", 0,
      (bm_off)->ind_1, (bm_off)->ind_2, (bm_off)->k_r[1], (bm_off)->k_r[2]);

  for(n1 = -n_set, i_set = 1; n1 <= n_set; n1++)
  {
    for(n2 = -n_set; (n2 <= n_set) && (i_set < n_set); n2++)
    {
      k_x = n1*m11 + n2*m21;
      k_y = n1*m12 + n2*m22;

      if( (k_x >= 0.) && (k_x + K_TOLERANCE < 1.) &&
          (k_y >= 0.) && (k_y + K_TOLERANCE < 1.) &&
          (R_hypot(k_x, k_y) > K_TOLERANCE)          )
      {
        (bm_off+i_set)->ind_1 = k_x;
        (bm_off+i_set)->ind_2 = k_y;
        (bm_off+i_set)->k_r[1] = k_x*g1_x + k_y*g2_x;
        (bm_off+i_set)->k_r[2] = k_x*g1_y + k_y*g2_y;

        i_set ++;
      }

    } /* for n2 */

  } /* for n1 */
 
  #if WARNING_LOG
  if( i_set != n_set)
  {
    WARNING_MSG("wrong number of beam sets found.\n"
                "                    found: %d, should be: %d\n", i_set, n_set);
  }
  #endif

  /*
   * Find the beams within the radius defined by k_max
   * - determine boundaries for beam indices n1 and n2
   * - loop over beam indices.
   */

  /* a1 = length of g1 */
  a1 = R_hypot(g1_x, g1_y);
  /* a2 = length of g2 */
  a2 = R_hypot(g2_x, g2_y);

  /* a2 * cos(a1,a2) */
  faux_r = R_fabs((g1_x*g2_x + g1_y*g2_y)/a1);
  /* a2 * sin(a1,a2) */
  faux_i = R_fabs((g1_x*g2_y - g1_y*g2_x)/a1);

  /*
   * n2_max = k_max / (sin(a1,a2) * a2) + k_in/a2
   * n1_max = k_max / a1 + n2_max * (cos(a1,a2) *a2)/a1 + k_in/a1
   */
  n2_max = 2 + (int)(k_max/faux_i + k_in[0]/a2);
  n1_max = 2 + (int)( k_max/a1 + n2_max * faux_r/ a1 + k_in[0]/a1);

  CONTROL_MSG(CONTROL_X, "n1_max = %2d, n2_max = %2d\n", n1_max, n2_max);

  /*
   * k_r, k_i is now defined by the complex energy
   */
  cri_sqrt(&k_r, &k_i, 2.*eng_max, 2.*v_par->eng_i);

  /* Loop over beam sets */
  for(i_set = 0, i_beams = 0, offset = 0;
      i_set < n_set;
      i_set ++, offset = i_beams)
  {
    /*
     * Find the beams within the radius defined by k_max
     * (loop over beam indices)
     */
    for(n1 = -n1_max; n1 <= n1_max; n1 ++)
    {
      for(n2 = -n2_max; n2 <= n2_max; n2 ++)
      {
        k_x = n1*g1_x + n2*g2_x + k_in[1] + (bm_off+i_set)->k_r[1];
        k_y = n1*g1_y + n2*g2_y + k_in[2] + (bm_off+i_set)->k_r[2];

        faux_r = SQUARE(k_x) + SQUARE(k_y);
        if(faux_r <= k_max_2)
        {
          /* indices, k_par, k_x/y */
          (beams + i_beams)->ind_1 = (real)n1 + (bm_off+i_set)->ind_1;
          (beams + i_beams)->ind_2 = (real)n2 + (bm_off+i_set)->ind_2;

          k_x = n1*g1_x + n2*g2_x + (bm_off+i_set)->k_r[1];
          k_y = n1*g1_y + n2*g2_y + (bm_off+i_set)->k_r[2];

          (beams + i_beams)->k_r[1] = k_x;
          (beams + i_beams)->k_r[2] = k_y;
          (beams + i_beams)->k_par  = SQUARE(k_x) + SQUARE(k_y);

          (beams + i_beams)->k_i[1] = 0.;
          (beams + i_beams)->k_i[2] = 0.;

          (beams + i_beams)->set = i_set;

          /* Akz_r = (area of the unit cell)^-1 */
          (beams + i_beams)->Akz_r = 1./c_par->area;

          i_beams ++;
        } /* if inside k_max */

      } /* for n2 */

    } /* for n1 */

    /*
     * 1st pass: Sort the beams according to the parallel component
     * (i.e. smallest k_par first)
     */
    CONTROL_MSG(CONTROL, "SORTING %2d beams in set %d:\n",
                i_beams-offset, i_set);

    for(n1 = offset; n1 < i_beams; n1 ++)
    {
      for(n2 = n1+1; n2 < i_beams; n2 ++)
      {
        if((beams + n2)->k_par < (beams + n1)->k_par )
        {
          memcpy( & beam_aux, beams + n2, sizeof(leed_beam) );
          memcpy( beams + n2, beams + n1, sizeof(leed_beam) );
          memcpy( beams + n1, & beam_aux, sizeof(leed_beam) );
        }
      } /* n2 */
    }  /* n1 */

    /*
     * 2nd pass: Sort the beams according to the 1st and 2nd index
     * (i.e. smallest indices first)
     */
    for(n1 = offset; n1 < i_beams; n1 ++)
    {
      for(n2 = n1+1;
         fabs( (beams + n2)->k_par - (beams + n1)->k_par ) < K_TOLERANCE; 
         n2++)
      {
        if((beams + n2)->ind_1 < (beams + n1)->ind_1 )
        {
          memcpy( & beam_aux, beams + n2, sizeof(leed_beam) );
          memcpy( beams + n2, beams + n1, sizeof(leed_beam) );
          memcpy( beams + n1, & beam_aux, sizeof(leed_beam) );
        }
        if( (IS_EQUAL_REAL((beams + n2)->ind_1, (beams + n1)->ind_1 )) &&
            ((beams + n2)->ind_2 < (beams + n1)->ind_2 )  )
        {
          memcpy( & beam_aux, beams + n2, sizeof(leed_beam) );
          memcpy( beams + n2, beams + n1, sizeof(leed_beam) );
          memcpy( beams + n1, & beam_aux, sizeof(leed_beam) );
        }
      } /* n2 */

      CONTROL_MSG(CONTROL, "%2d: (%6.2f, %6.2f):\t",
              n1, (beams + n1)->ind_1, (beams + n1)->ind_2);
      CONTROL_MSG(CONTROL, "\td_par: %.2f\tk_r: (%5.2f, %5.2f, %5.2f)\n",
              R_sqrt((beams + n1)->k_par),  (beams + n1)->k_r[1],
              (beams + n1)->k_r[2], (beams + n1)->k_r[3]);
    }  /* n1 */

  } /* for i_set */

  /* Set k_par of the last element of the list to the terminating value. */
  (beams + i_beams)->k_par = F_END_OF_LIST;

  return(n_set);
}  /* end of function leed_beam_gen */
