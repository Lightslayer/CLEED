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
 *16.09.00 
  file contains function:

  leed_par_mktl_nd(mat *p_tl, leed_phase *phs_shifts, int l_max, real energy)

 Calculate atomic scattering factors for a given energy.

CHANGES:
GH/20.08.94 - Creation
GH/18.07.95 - temperature dependent phase shifts.
GH/03.05.00 - read non-diagonal t-matrix
GH/16.09.00 - calculate non-diagonal t-matrix.

*********************************************************************/

#include <math.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "leed.h"

/*!
 * Updates matrix \p p_tl containing the atomic scattering factors for all types
 * of atoms used in the calculation.
 *
 * \param[in,out] p_tl Array of scattering factor matrices. The function
 * returns its first argument. If \p p_tl is \c NULL then the structure will be
 * created and allocated memory.
 * \param[in] phs_shifts Pointer to phase shifts.
 * \param l_max Maximum linear angular momentum.
 * \param energy New energy (real part).
 * \return Pointer to the new set of scattering factor matrices.
 * \retval NULL if any error occurred and #EXIT_ON_ERROR is not defined.
 */
mat *leed_par_mktl_nd(mat *p_tl, const leed_phase *phs_shifts,
                     size_t l_max, real energy)
{

  size_t l, l_set_1;
  size_t i_set, n_set;
  size_t i_eng, iaux;

  real delta;
  real faux_r, faux_i;

  /* Search through list "phs_shifts".
   * - Find number of sets (n_set) and
   * - max. number of l (l_max: > input value if there is one larger
   *   set of phase shifts);
   * - call mkcg_coeff to ensure all C.G. coefficients are available.
   *  - allocate p_tl.
   */
  for(n_set = 0; phs_shifts[n_set].lmax != I_END_OF_LIST; n_set ++)
  { ; }

  #ifdef CONTROL
  fprintf(STDCTR, "(leed_par_mktl_nd): "
                  "energy = %.2f H, n_set = %u, l_max = %u\n",
                  energy /*HART*/, n_set, l_max);
  #endif

  if(p_tl == NULL)
  {
    p_tl = (mat *)malloc(n_set * sizeof(mat));
    for(i_set = 0; i_set < n_set; i_set ++) p_tl[i_set] = NULL;
  }

  /* Calculate tl (diagonal or non-diagonal scattering matrix
   * for each set of phase shifts. */
  for(i_set = 0; i_set < n_set; i_set ++)
  {
    l_set_1 = phs_shifts[i_set].lmax + 1;

    /* check for unknown t_type */
    if( (phs_shifts[i_set].t_type != T_DIAG) &&
        (phs_shifts[i_set].t_type != T_NOND) )
    {
      #ifdef ERROR
      fprintf(STDERR, "*** error (leed_par_mktl_nd): "
              "t_type %d has no valid value for set No. %d\n",
              phs_shifts[i_set].t_type, i_set);
      fprintf(STDERR, "\t => exit\n");
      #endif

      #ifdef EXIT_ON_ERROR
      exit(1);
      #else
      free(p_tl);
      return(NULL);
      #endif
    } /* neither T_DIAG nor T_NOND */

    #ifdef CONTROL_X
    fprintf(STDCTR, "(leed_par_mktl_nd):"
            "i_set = %u, lmax(set) = %u, n_eng = %u, t_type = %d\n",
            i_set, l_set_1 - 1,
            phs_shifts[i_set].n_eng, phs_shifts[i_set].t_type);
    #endif

    #ifdef CONTROL
    fprintf(STDCTR, "(leed_par_mktl_nd):  d %u: (%s)\n",
            i_set, phs_shifts[i_set].input_file);
    #endif

    p_tl[i_set] = matalloc(p_tl[i_set], l_set_1, 1, NUM_COMPLEX);

    /* Check if energy is inside the energy range, exit if too small */
    if(energy < phs_shifts[i_set].eng_min)
    {

      /* Abort for too low energies */
      #ifdef ERROR
      fprintf(STDERR, "*** error (leed_par_mktl_nd): "
              "%.1f H is lower than the min. energy for set No. %u\n",
              energy, i_set);
      fprintf(STDERR, "\t => exit\n");
      #endif

      #ifdef EXIT_ON_ERROR
      exit(1);
      #else
      free(p_tl);
      return(NULL);
      #endif
    } /* if (energy too low) */
    else if(energy >= phs_shifts[i_set].eng_max)
    {
      /* Extrapolate for too high energies */
      #ifdef WARNING
      fprintf(STDWAR, "* warning (leed_par_mktl_nd): "
              "%.2f H is higher than max. energy for set No. %u\n",
              energy, i_set);
      fprintf(STDWAR, "\t => calculate extrapolated phase shift values\n");
      #endif

      i_eng = phs_shifts[i_set].n_eng - 1;
      for(l = 0; l < l_set_1; l ++)
      {
        delta =  phs_shifts[i_set].pshift[i_eng*l_set_1 + l] -
             ( ( phs_shifts[i_set].pshift[i_eng*l_set_1 + l] -
                 phs_shifts[i_set].pshift[(i_eng - 1)*l_set_1 + l] )
             / ( phs_shifts[i_set].energy[i_eng] -
                 phs_shifts[i_set].energy[i_eng - 1] ) )
             * ( phs_shifts[i_set].energy[i_eng] - energy);

        iaux = 1 + l;
        faux_r = R_cos(delta);
        faux_i = R_sin(delta);
        cri_mul(p_tl[i_set]->rel+iaux, p_tl[i_set]->iel+iaux,
              faux_r, faux_i, faux_i, 0.);
      }

      /* Include temperature in atomic scattering factors. */
      if(phs_shifts[i_set].t_type == T_DIAG)
      {
        leed_par_temp_tl(p_tl[i_set], p_tl[i_set], phs_shifts[i_set].dr[0],
                         energy, l_max, phs_shifts[i_set].lmax);

        #ifdef CONTROL_X
        fprintf(STDCTR, "(leed_par_mktl_nd): "
                "after leed_par_temp_tl, dr[0] = %.3f A^2:\n",
                phs_shifts[i_set].dr[0]*BOHR*BOHR);
        matshowabs(p_tl[i_set]);
        #endif
      } /* T_DIAG */
      else if(phs_shifts[i_set].t_type == T_NOND)
      {
        leed_par_cumulative_tl(p_tl[i_set],
                               p_tl[i_set],
                               phs_shifts[i_set].dr[1],
                               phs_shifts[i_set].dr[2],
                               phs_shifts[i_set].dr[3],
                               energy,
                               l_max,
                               phs_shifts[i_set].lmax);

        #ifdef CONTROL_X
        fprintf(STDCTR, "(leed_par_mktl_nd): non-diag. Tlm for set %u:\n",
                i_set);
        matshowabs(p_tl[i_set]);
        #endif
      } /* T_NOND */
    } /* else if (energy too high) */
    else
    {
      /* If the energy is within the range of phase shifts:
       * scan through the energy list and find the right values to
       * interpolate:
       *
       * dl(E) = dl(i) - (dl(i) - dl(i-1))/(E(i) - E(i-1)) * ((E(i) - E)
       */
      for(i_eng = 0; phs_shifts[i_set].energy[i_eng] < energy; i_eng ++)
      { ; }

      for(l = 0; l < l_set_1; l ++)
      {
        delta =  phs_shifts[i_set].pshift[i_eng*l_set_1 + l] -
             ( ( phs_shifts[i_set].pshift[i_eng*l_set_1+l] -
                 phs_shifts[i_set].pshift[(i_eng - 1)*l_set_1+l] )
             / ( phs_shifts[i_set].energy[i_eng] -
                 phs_shifts[i_set].energy[i_eng - 1] ) )
             * ( phs_shifts[i_set].energy[i_eng] - energy);

        iaux = 1 + l;
        faux_r = R_cos(delta);
        faux_i = R_sin(delta);
        cri_mul(p_tl[i_set]->rel+iaux, p_tl[i_set]->iel+iaux,
                faux_r, faux_i, faux_i, 0.);
      }

      /* Include temperature in atomic scattering factors. */
      if(phs_shifts[i_set].t_type == T_DIAG)
      {
        leed_par_temp_tl(p_tl[i_set], p_tl[i_set], phs_shifts[i_set].dr[0],
                         energy, l_max, phs_shifts[i_set].lmax);

        #ifdef CONTROL_X
        fprintf(STDCTR, "(leed_par_mktl_nd): "
                "after leed_par_temp_tl, dr[0] = %.3f A^2:\n",
                phs_shifts[i_set].dr[0]*BOHR*BOHR);
        matshowabs(p_tl[i_set]);
        #endif
      } /* T_DIAG */
      else if(phs_shifts[i_set].t_type == T_NOND)
      {
        leed_par_cumulative_tl(p_tl[i_set],
                               p_tl[i_set],
                               phs_shifts[i_set].dr[1],
                               phs_shifts[i_set].dr[2],
                               phs_shifts[i_set].dr[3],
                               energy,
                               l_max,
                               phs_shifts[i_set].lmax);

        #ifdef CONTROL_X
        fprintf(STDCTR, "(leed_par_mktl_nd): non-diag. Tlm for set %u:\n",
                i_set);
        matshowabs(p_tl[i_set]);
        #endif
      } /* T_NOND */

    } /* else (in the right energy range) */
 
  } /* for i_set */
 
  return(p_tl);
} /* end of function leed_par_mktl_nd */