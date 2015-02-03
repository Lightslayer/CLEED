/*********************************************************************
 *                          LBMSELECT.C
 *
 *  Copyright 1992-2014 Georg Held <g.held@reading.ac.uk>
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 * @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
 *
 * Changes:
 *   GH/26.08.94 - Creation
 *   GH/04.09.97 - use memcpy for copying beams.
 *********************************************************************/

/*! @file
 *
 * Contains leed_beam_get_selection() to find all beams to be included at
 * the current energy.
 */

#include <math.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>

#include "leed.h"

/*!
 * Find all beams to be included at the current energy and write them to a
 * list.
 *
 * \param[out] p_beams_out Pointer to the list of beams to be included at the
 * current energy. The list will be terminated by #F_END_OF_LIST in the member
 * @leed_beam::k_par .
 *
 * \param[in] beams_in Pointer to list of all beams used throughout the energy
 * loop. The list must be terminated by #F_END_OF_LIST in the member
 * @leed_beam::k_par .
 *
 * \param[in] v_par Pointer to struct containing all the parameters that change
 * during the energy loop (for details see leed_def.h ). Members used:
 *  - eng_r, eng_i - current complex energy (in Hartree)
 *  - k_in - incident k-vector.
 *  - epsilon - parameter determining the cutoff radius for
 *  - k_par. (maximum amplitude which can propagate between two layers).
 *
 * \param dmin Minimum distance between two successive layers.
 *
 * \return i_beams_out; the number of beams in the list pointed to by @p p_beams
 * \retval -1 if failed (not implemented)
 */
int leed_beam_get_selection(leed_beam **p_beams_out,
                            leed_beam * beams_in,
                            leed_var *v_par,
                            real dmin)
{
  size_t iaux;
  int i_beams_in, i_beams_out;

  real faux_r;
  real k_max, k_max_2;
  real k_r, k_i;
  real k_x, k_y;

  leed_beam *beams_out;

  /*
   * Check input array beams_in for max. number of beams
   * and allocate memory for the beam list p_beams_out.
   */
  for(iaux = 0;
      ! IS_EQUAL_REAL((beams_in + iaux)->k_par, F_END_OF_LIST);
      iaux++);
  iaux++;

  if (*p_beams_out == NULL)
  {
    *p_beams_out = (leed_beam *) calloc(iaux, sizeof(leed_beam));
  }
  else
  {
    *p_beams_out =
        (leed_beam *) realloc(*p_beams_out, iaux*sizeof(leed_beam));
  }
 
  if(*p_beams_out == NULL)
  {
    ERROR_MSG("allocation error.\n");
    ERROR_EXIT_RETURN(LEED_ALLOCATION_ERROR, LEED_ALLOCATION_ERROR);
  }
 else
 {
   beams_out = *p_beams_out;
 }

  /*Determine k_max (square of max k_par) from epsilon and dmin. */
  faux_r = R_log(v_par->epsilon) / dmin;
  k_max_2 = faux_r*faux_r + 2*v_par->eng_r;
  k_max = R_sqrt(k_max_2);
 
  CONTROL_MSG(CONTROL_X, "dmin  = %.2f, epsilon = %.2e\n",
              dmin * BOHR, v_par->epsilon);
  CONTROL_MSG(CONTROL_X, "k_max = %.2f, max. No of beams = %2d\n", k_max, iaux);

  /*
   * Copy those beams from list beams_in whose k_par are within the
   * radius defined by k_max into list beams_out.
   * - loop over beam indices.
   */
  CONTROL_MSG(CONTROL, "currently used beams:\n\n");

  /* k_r, k_i is now defined by the complex energy */
  cri_sqrt(&k_r, &k_i, 2.*v_par->eng_r, 2.*v_par->eng_i);

  i_beams_out = 0;
  for(i_beams_in = 0;
      ! IS_EQUAL_REAL((beams_in + i_beams_in)->k_par, F_END_OF_LIST);
      i_beams_in ++)
  {
    k_x = (beams_in + i_beams_in)->k_r[1] + v_par->k_in[1];
    k_y = (beams_in + i_beams_in)->k_r[2] + v_par->k_in[2];
    faux_r = SQUARE(k_x) + SQUARE(k_y);
    if(faux_r <= k_max_2)
    {
      /* copy the complete beam structure */
      memcpy(beams_out + i_beams_out,
             beams_in + i_beams_in, sizeof(leed_beam));

      /* replace, k_par, k_r/i, k_r/ix/y */
      (beams_out + i_beams_out)->k_par = R_sqrt(faux_r);

      (beams_out + i_beams_out)->k_r[0] = k_r;
      (beams_out + i_beams_out)->k_i[0] = k_i;
      (beams_out + i_beams_out)->k_r[1] = k_x;
      (beams_out + i_beams_out)->k_i[1] = 0.;
      (beams_out + i_beams_out)->k_r[2] = k_y;
      (beams_out + i_beams_out)->k_i[2] = 0.;

      /*
       * k_r/i[3] are calculated from the square root of the complex energy
       * minus the energy corresponding to the parallel momentum.
       * sqrt(2E-kpar^2)
       */
      cri_sqrt((beams_out + i_beams_out)->k_r+3,
               (beams_out + i_beams_out)->k_i+3,
               2*(v_par->eng_r) - faux_r, 2*(v_par->eng_i));

      /*
       * cos(theta) = k_z/k.
       * phi = atan (ky/kx).
       */
      cri_div(&(beams_out + i_beams_out)->cth_r,
              &(beams_out + i_beams_out)->cth_i,
               (beams_out + i_beams_out)->k_r[3],
               (beams_out + i_beams_out)->k_i[3], k_r, k_i);
      (beams_out + i_beams_out)->phi = R_atan2(k_y, k_x);

      /* 1/ A*kz */
      cri_div(&(beams_out + i_beams_out)->Akz_r,
              &(beams_out + i_beams_out)->Akz_i,
               (beams_in  + i_beams_in)->Akz_r, 0.,
               (beams_out + i_beams_out)->k_r[3],
               (beams_out + i_beams_out)->k_i[3]);

      #if CONTROL
      /* mark new beam sets */
      if((i_beams_out > 0) &&
          ((beams_out+i_beams_out)->set != (beams_out+i_beams_out-1)->set) )
      {
        fprintf(STDCTR, "\n");
      }

      /* mark evanescent beams */
      if ((beams_out + i_beams_out)->k_par <= k_r) fprintf(STDCTR, "   ");
      else fprintf(STDCTR, "(*)");

      fprintf(STDCTR, "%3d [%d]: (%6.2f, %6.2f)\t", i_beams_out,
              (beams_out + i_beams_out)->set,
              (beams_out + i_beams_out)->ind_1,
              (beams_out + i_beams_out)->ind_2);
      fprintf(STDCTR, "k_par:\t%6.2f\tk_r:(%6.2f, %6.2f, %6.2f)\n",
              (beams_out + i_beams_out)->k_par,
              (beams_out + i_beams_out)->k_r[1],
              (beams_out + i_beams_out)->k_r[2],
              (beams_out + i_beams_out)->k_r[3]);
      #endif

      #ifdef CONTROL_X
      fprintf(STDCTR, "\t\t|k_r|: %6.3f",(beams_out + i_beams_out)->k_r[0]);
      fprintf(STDCTR, "\tk_i\t: %6.2f\t(%6.2f, %6.2f, %6.2f)\n",
              (beams_out + i_beams_out)->k_i[0],
              (beams_out + i_beams_out)->k_i[1],
              (beams_out + i_beams_out)->k_i[2],
              (beams_out + i_beams_out)->k_i[3]);
      fprintf(STDCTR, "\t\t\t\tcos(th)\t: (%6.2f, %6.2f)\tphi:  %6.1f\n",
              (beams_out + i_beams_out)->cth_r,
              (beams_out + i_beams_out)->cth_i,
              (beams_out + i_beams_out)->phi * 180/PI);
      fprintf(STDCTR, "\t\t\t\t1/Akz\t: (%6.4f, %6.4f)\n\n",
              (beams_out + i_beams_out)->Akz_r,
              (beams_out + i_beams_out)->Akz_i);
      #endif /* CONTROL */

      i_beams_out ++;
    } /* if */
  }

  /* Set k_par of the last element of the list to the terminating value. */
  (beams_out + i_beams_out)->k_par = F_END_OF_LIST;

  return(i_beams_out);

  /* dummy statements to avoid compiler warnings due to preprocessor nesting */
  k_max = k_max * 1;
}  /* end of function leed_beam_get_selection */
