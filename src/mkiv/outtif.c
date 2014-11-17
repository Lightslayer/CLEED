/*********************************************************************
 *                        OUTTIF.C
 *
 *  Copyright 1992-2014 Georg Held <g.held@reading.ac.uk>
 *  Copyright 2013-2014 Liam Deacon <liam.deacon@diamond.ac.uk>
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 * @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
 *
 * Changes:
 *   VJ/07.02.03
 *   GH/21.03.03
 *   LD/16.06.14 - mkiv_image update
 **************************************************************************/

/*! \file
 *
 * Contains out_tif() high-level function for converting and outputting a TIFF
 * image to file.
 */

#include <stdio.h>
#include <stdlib.h>
#include "mkiv.h"

/*!
 * Converts internal #mkiv_image into TIFF parameters for display purposes and
 * then writes the new data into an output TIFF file. This function is used in
 * markref.c and drawbound.c
 *
 * \param[in] mat_image The internal matrix structure for handling image data.
 * \param[in] filename Filename or path string for output TIFF image.
 *  * \return Integer indicating function success (using normal C convention).
 * \retval 0 if function is successful.
 * \retval -1 if the function was unable to allocate memory for TIFF image.
 * \retval >0 The number of errors that occurred when calling sub-functions.
 */
int out_tif(const mkiv_image *mat_image, const char *filename)
{
  mkiv_tif_values *tif_image;
  int i_aux = 0;

  /* Allocate memory for tif_values pointer tif_image*/
  tif_image = (mkiv_tif_values *)malloc(sizeof(mkiv_tif_values));
  if(tif_image == NULL)
  {
    #ifdef EXIT_ON_ERROR
    fprintf(stderr, "***error (out_tif): "
           "failed to allocate memory for tif_image\n");
    exit(1);
    #else
    return(-1);
    #endif
  }
  tif_image->buf = NULL;

  /* Convert mkiv_image into tif_values & copy all imagedata to buf*/
  if (conv_mat2tif(mat_image, tif_image) != 0) i_aux++;

  /* Write the 8 bit TIFF to a file */
  if (writetif(tif_image, filename) != 0) i_aux++;

  free(tif_image->buf);
  free(tif_image);

  if (i_aux > 0) return(i_aux);

  return(0);
}
