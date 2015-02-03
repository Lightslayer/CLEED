/*********************************************************************
 *                        SRPO.C
 *
 *  Copyright 1992-2014 Georg Held <g.held@reading.ac.uk>
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 * @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
 *
 * Changes:
 *   GH/23.08.95 - creation
 *********************************************************************/

#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <math.h>

#include "csearch.h"

/*!
 * Perform a search according to POWELL'S METHOD.
 *
 * \param n_dim number of dimensions for search?
 * \param bak_file filename of vertex backup file *.vbk
 * \param log_file filename of log file.
 */
void sr_po(size_t n_dim, const char *bak_file, const char *log_file)
{
  size_t i_par, j_par;
  size_t nfunc;

  real rmin;

  FILE *log_stream;

  cleed_vector *p = CLEED_VECTOR_ALLOC(n_dim);
  cleed_basic_matrix *xi = CLEED_BASIC_MATRIX_ALLOC(n_dim, n_dim);

/***********************************************************************
 * POWELL'S METHOD
 ***********************************************************************/

  if( (log_stream = fopen(log_file, "a")) == NULL)
  {
    ERROR_MSG("Could not append to log file '%s'\n", log_file);
  }
  fprintf(log_stream, "=> POWELL'S METHOD\n\n");

/***********************************************************************
 * Set up initial direction set
 ***********************************************************************/
  if(strncmp(bak_file, "---", 3) == 0)
  {
    fprintf(log_stream, "=> Set up initial direction set:\n");
    fclose(log_stream);

    for (i_par = 0; i_par < n_dim; i_par ++)
    {
      for (j_par = 0; j_par < n_dim; j_par ++)
      {
        if(i_par == j_par)
          CLEED_BASIC_MATRIX_SET(xi, i_par, j_par, n_dim, n_dim, 1.);
        /* else: memory already zeroed */
      }
    }

  } /* if */
  else
  {
    /* Read direction set from file */
  }

/***********************************************************************
 * Enter search function sr_powell
 ***********************************************************************/

  CONTROL_MSG(CONTROL, "Enter sr_powell #%i\n", i_par);

  fprintf(log_stream, "=> Start search (abs. tolerance = %.3e)\n", R_TOLERANCE);

  /* close log file before entering the search */
  fclose(log_stream);

  sr_powell(p, xi, n_dim, R_TOLERANCE, &nfunc, &rmin, SR_RF);

/***********************************************************************
 * Write final results to log file
 ***********************************************************************/

  CONTROL_MSG(CONTROL, "%d iterations in sr_powell\n", nfunc);

  if( (log_stream = fopen(log_file, "a")) == NULL)
  {
    ERROR_MSG("Could not append to log file '%s'\n", log_file);
  }

  fprintf(log_stream, "\n=> No. of iterations in sr_powell: %3d\n", nfunc);
  fprintf(log_stream, "=> Optimum parameter set:\n");
  for (j_par = 0; j_par < n_dim; j_par++ )
  {
   fprintf(log_stream, "%.6f ", CLEED_VECTOR_GET(p, j_par));
  }
  fprintf(log_stream, "\n");

  fprintf(log_stream, "=> Optimum function value:\n");
  fprintf(log_stream, "rmin = %.6f\n", rmin);

  fclose(log_stream);

  /* free memory */
  CLEED_BASIC_MATRIX_FREE(xi);
  CLEED_VECTOR_FREE(p);

}  /* end of function sr_po */
