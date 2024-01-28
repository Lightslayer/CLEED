/*********************************************************************
  GH/27.09.00 
  file contains functions:

  leed_ms_compl_sym           (03.02.95)
     Transformation of the multiple scattering matrix from angular 
     momentum space into k-space.

 Changes:

 GH/03-13.04.95 - Creation (it took only 6 days to create the world
               but it took more than 2 months to create this function!)
 GH/12.07.95 - correct signs in giant matrix (Mbg) and wave propagators.
 GH/14.07.95 - change directions (z is opposite to VHT convention)
 GH/28.07.95 - change parameter list (layer instead of atoms etc.).
               replace matrix array Tii by pointer p_Tii.
               find smallest l_max necessary.
 GH/04.08.95 - order atoms such that the most populated plane comes first.
 GH/07.08.95 - perform matrix inversion by partitioning (function 
               ms_partinv instead of matinv).
 WB/04.05.98 - copy lmscompl.c
               input sum over equivalent beams
 WB/25.04.98 - sum over eq... befor transformation into k -space
 WB/24.06.98 - if l_type = OVER
 WB/14.08.98 - correct summation over all equvalent beams (introduce k_x_sym etc.)
 GH/14.08.98 - replace lm loop in Rp/m and write all m's where they belong to.
               ... und es funktioniert ...
 GH/27.09.00 - remove n_rot from parameter list (not used)

*********************************************************************/

#include <math.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include "leed.h"

#define CPUTIME

#ifdef CPUTIME
#define CTIME(x) leed_cpu_time(STDCPU,x)
#else
#define CTIME(x) 
#endif

#ifndef GEO_TOLERANCE          /* should be defined in "leed_def.h" */
#define GEO_TOLERANCE 0.0001                /* ca. 0.00005 A */
#endif

#ifndef K_TOLERANCE            /* should be defined in "leed_def.h" */
#define K_TOLERANCE 0.0001                  /* tolerance in k_par */
#endif

/*======================================================================*/
/*======================================================================*/


int leed_ms_compl_sym( mat *p_Tpp, mat *p_Tmm, mat *p_Rpm, mat *p_Rmp,
                 leed_var *v_par, 
                 leed_layer * layer,
                 leed_beam * beams )

/************************************************************************
 
  Calculate scattering matrix for composite layer (combined space method)

 INPUT:

   mat * p_Tpp, p_Tmm, p_Rpm, p_Rmp - (output) pointers to composite layer 
              diffraction matrices in k-space.
              Tpp  k(+) -> k(+) (transmission matrix)
              Tmm  k(-) -> k(-) (transmission matrix)
              Rpm  k(-) -> k(+) (reflection matrix)
              Rmp  k(+) -> k(-) (reflection matrix)

   leed_var v_par - (input) parameters which vary during the energy 
              loop. Used: k_in    (parallel components of the incoming beam), 
                          l_max   (max. l quantum number), 
                          tl      (atomic scattering factors), 
                          epsilon (smallesti relative wave amplitude to 
                                   be considered).
   
   leed_layer * layer - (input) list containing all relevant 
              information about the composite layer. Used in this function:

              int natoms: number of atoms in the layer.
              leed_atom * atoms: positions, and scattering properties 
                     of all atoms.
              real a_lat[5]: lattice vectors:
                     a_lat[1] = a1_x, a_lat[2] = a2_x,
                     a_lat[3] = a1_y, a_lat[4] = a2_y;
              real rel_area: area of the unit cell relative to 1x1.

   leed_beam * beams - (input) additional information about the k 
              vectors involved (in this case: (A kz)^-1).
              The order of beams must be equal to the first dimension of
              Ykl (not checked).


 DESIGN

  R/T(k'k) = pref_i * |k'><l,m|M(lm,l'm')|l'm'><k| + dkk'

  Ylm =  |k'><l,m| = Ylm(k')     (k_max rows) x ((l_max+1)^2 columns)

  pref_i = -i*16*PI^2 * EXP/ (A*k'z+)
  EXP = exp[i(-k(+)r1 + k'(+)rN)] for ++
        exp[i(-k(+)r1 - k'(-)r1)] for -+
        exp[i(+k(-)rN - k'(-)r1)] for --
        exp[i(+k(-)rN + k'(+)rN)] for +-

  r1 = (0, 0, (atoms+0)->pos[3]).
       Projection of the origin to the atomic subplane with the greatest
       z coordinate (atom No. 0):

  rN = (0, 0, (atoms+n_atoms-1)->pos[3]).
       Projection of the origin to the atomic subplane with the largest
       z coordinate (atom No. n_atoms-1):

 FUNCTION CALLS
  
  matarralloc
  matalloc
  matfree
  matcheck
  matins
  matinv
  matmul
  
  leed_ms_lsum_ii
  leed_ms_lsum_ij
  leed_ms_tmat_ii
  leed_ms_tmat_ij

  ms_partinv
  
 RETURN VALUES:

  1 if o.k.
 -1 if not.

*************************************************************************/
{
int iaux;
int off_row, off_col;

int l_max, l_max_2;
int i_type, n_type, l_type , i_layer ;      /**** new  l_type ***/
int n_atoms, i_atoms, j_atoms; 
int n_beams, i_beams, k, l, i_c;            /**new m, i_beams, i_r ,i_c**/
int n_plane;


real d_ij[4];

real faux_r, faux_i;
real pref_i;                    /* pref_i is used as common prefactor. do not change! */

real z_max, z_min;              /* z coordinates of the outer-most atoms */
real z_plane;                   /* z coordinate ofo the most populated plane */

real *ptr_r, *ptr_i, *ptr_end;

leed_atom * atoms;        /* atomic positions and scattering properties */

mat Ylm;                        /* spherical harmonics (for exit beams) */
mat Llm_ij, Llm_ji;             /* interlayer lattice sums */
mat Maux, Mbg, Mark;            /* dummy matrices */
mat L_p, L_m, R_p, R_m;         /* dummy matrices */

mat Tpp, Tmm, Rpm, Rmp;         /* Layer diffraction matrices in k-space 
                                   will be copied to output */
mat * p_Tii;                    /* Array of Bravais layer scattering matrices */

 Ylm = NULL;

 Llm_ij = NULL;
 Llm_ji = NULL;

 Mark = NULL;
 Maux = NULL;
 Mbg = NULL;

 L_p = NULL;
 L_m = NULL;
 R_p = NULL;
 R_m = NULL;

 CTIME("(leed_ms_compl): start of function\t\t");

/********************************************************************** 
 Check the validity of input matrices p_T/R
 iaux is used as error flag
**********************************************************************/

 iaux = 0;
 if (matcheck(*p_Tpp)  < 0) {
#ifdef ERROR_LOG
fprintf(STDERR," *** error (leed_ms_compl_sym): invalid input matrix (1st argument)\n");
#endif
  iaux = 1; }
 if (matcheck(*p_Tmm)  < 0) {
#ifdef ERROR_LOG
fprintf(STDERR," *** error (leed_ms_compl_sym): invalid input matrix (2nd argument)\n");
#endif
  iaux = 1; }
 if (matcheck(*p_Rmp)  < 0) {
#ifdef ERROR_LOG
fprintf(STDERR," *** error (leed_ms_compl_sym): invalid input matrix (3rd argument)\n");
#endif
  iaux = 1; }
 if (matcheck(*p_Rpm)  < 0) {
#ifdef ERROR_LOG
fprintf(STDERR," *** error (leed_ms_compl_sym): invalid input matrix (4th argument)\n");
#endif
  iaux = 1; }

 if(iaux != 0)
 {
#ifdef EXIT_ON_ERROR
   exit(1);
#else
   return(-1);
#endif
  }

/********************************************************************** 
 Prepare Calculation:
  (i) Allocate memory for array atoms and copy the relevant information
      into the list.
      Find max number of atom types (n_type),
      outer-most z coordinates (z_min and z_max),
      and max. l necessary.
 (ii) Sort atoms according to their z position (pos[3]): smallest z first.
**********************************************************************/

/* (o) preset often used values */

 Tpp = *p_Tpp;
 Tmm = *p_Tmm;
 Rpm = *p_Rpm;
 Rmp = *p_Rmp;

/* find number of beams */
 for(n_beams = 0; 
     ! IS_EQUAL_REAL((beams + n_beams)->k_par, F_END_OF_LIST); n_beams ++);

/* 
   (i) Copy layer->atoms to atoms, 
       find max number of atom types and outer-most z coordinates 
       find maximum l necessary for all atom types
*/

 n_atoms = layer->n_atoms;
 atoms = (leed_atom *) calloc(n_atoms+1, sizeof(leed_atom) );

 n_type = (atoms+0)->type;;
 l_type = layer->bulk_over;        /*****new*****/
 i_layer = layer->no_of_layer;
 z_min = z_max = (atoms+0)->pos[3];
 l_max = 1;

 for(i_atoms = 0; i_atoms < n_atoms; i_atoms ++)
 {

/* copy atom information */
   (atoms+i_atoms)->type    = (layer->atoms+i_atoms)->type;
   (atoms+i_atoms)->pos[1]  = (layer->atoms+i_atoms)->pos[1];
   (atoms+i_atoms)->pos[2]  = (layer->atoms+i_atoms)->pos[2];
   (atoms+i_atoms)->pos[3]  = (layer->atoms+i_atoms)->pos[3];
   
/* find n_type and z_min/z_max */
   n_type = MAX( (atoms+i_atoms)->type, n_type);

   z_max = MAX( (atoms+i_atoms)->pos[3], z_max);
   z_min = MIN( (atoms+i_atoms)->pos[3], z_min);

/* Find maximum l necessary */
   i_type = (atoms+i_atoms)->type;
   for( iaux = v_par->l_max;
        (cri_abs( (v_par->p_tl[i_type])->rel[iaux+1],
                  (v_par->p_tl[i_type])->iel[iaux+1] )
        < v_par->epsilon ) && (iaux > 1);
        iaux --)
   {;}
   l_max = MAX(l_max, iaux);
 }

 (atoms+n_atoms)->type = I_END_OF_LIST;   /* terminate list atoms */
 n_type ++;                               /* n_type = number of types */

/* this would reset l_max to its maximum value */
/*
 l_max = v_par->l_max;
*/
 l_max_2 = (l_max+1)*(l_max+1);

#ifdef CONTROL_X
 fprintf(STDCTR,"(leed_ms_compl_sym): l_max = %d, No of beams = %d, No of atoms = %d\n", 
         l_max, n_beams, n_atoms);
 fprintf(STDCTR,"(leed_ms_compl_sym): before sorting:\n");
#endif

/* (ii)a 
   Find plane containing most atoms
*/
 z_plane = (atoms+0)->pos[3];
 n_plane = 1;

 for(i_atoms = 0; i_atoms < n_atoms; i_atoms ++)
 {
   iaux = 0;
   for(j_atoms = 0; j_atoms < n_atoms; j_atoms ++)
   {
     if( cleed_real_fabs( (atoms+j_atoms)->pos[3] - (atoms+i_atoms)->pos[3] ) 
         < GEO_TOLERANCE )
     { iaux ++; }
   }    /* for j_atoms */
   
   if(iaux > n_plane)
   {
     n_plane = iaux;
     z_plane = (atoms+i_atoms)->pos[3];
   }
#ifdef CONTROL_X
   fprintf(STDCTR,"\t(%d) pos: (%5.2f,%5.2f,%5.2f) A type: %d\n",
           i_atoms, (atoms+i_atoms)->pos[1]*BOHR,
           (atoms+i_atoms)->pos[2]*BOHR, (atoms+i_atoms)->pos[3]*BOHR,
           (atoms+i_atoms)->type);
#endif
 }    /* for i_atoms */

/* (ii)b 
   Move atoms of the most populated plane to the front of atoms list 
*/
 for(i_atoms = 0, j_atoms = 0; i_atoms < n_atoms; i_atoms ++)
 {
   if( cleed_real_fabs( (atoms+i_atoms)->pos[3] - z_plane ) < GEO_TOLERANCE )
   {
  /* Exchange atom parameters if i_atoms != j_atoms */
     if( i_atoms != j_atoms )
     {
       iaux = (atoms+i_atoms)->type;
       (atoms+i_atoms)->type = (atoms+j_atoms)->type;
       (atoms+j_atoms)->type = iaux;
  
       for(iaux = 1; iaux <= 3; iaux ++)
       {
         faux_r = (atoms+i_atoms)->pos[iaux];
         (atoms+i_atoms)->pos[iaux]  = (atoms+j_atoms)->pos[iaux];
         (atoms+j_atoms)->pos[iaux]  = faux_r;
       }
     }  /* if i_atoms ... */

     j_atoms ++;
   }  /* if cleed_real_fabs ... */
 }  /* for i_atoms */

#ifdef CONTROL_X
 fprintf(STDCTR,"(leed_ms_compl_sym): after sorting:\n");
 for(i_atoms = 0; i_atoms < n_atoms; i_atoms ++)
 {
   fprintf(STDCTR,"\t(%d) pos: (%5.2f,%5.2f,%5.2f) A type: %d\n",
           i_atoms, (atoms+i_atoms)->pos[1]*BOHR,
           (atoms+i_atoms)->pos[2]*BOHR, (atoms+i_atoms)->pos[3]*BOHR,
           (atoms+i_atoms)->type);
 }  /* for i_atoms */
#endif

/********************************************************************** 
  Create Bravais layer scattering matrices Tii
  - Allocate a matrix array p_Tii n_type long (max. number of different atoms)
  - Calculate lattice sum for a single Bravais lattice (stored in L_ij).
  - Calculate the single Bravais lattice scattering matrix for each atom
    type and multiply with factor -1/2k0.
**********************************************************************/

 p_Tii = (mat *) malloc(n_type * sizeof(mat));
 if( p_Tii  == NULL )
 {
#ifdef ERROR_LOG
   fprintf(STDERR,"*** error (leed_ms_compl): Allocation error for p_Tii\n");
#endif
#ifdef EXIT_ON_ERROR
   exit(1);
#else
   return(-1);
#endif
 }
 else
 {
   for(i_type = 0; i_type < n_type; i_type ++) 
   {
     p_Tii[i_type] = NULL;
   }
 }

#ifdef CONTROL_X
   fprintf(STDCTR,"(leed_ms_compl_sym):  Calculate Bravais lattice sum\n");
#endif

/* Calculate Bravais lattice sum (only once) */
 Llm_ij = leed_ms_lsum_ii(Llm_ij, beams->k_r[0], beams->k_i[0],
                     v_par->k_in, layer->a_lat, 2 * l_max, v_par->epsilon );
 
#ifdef CONTROL_X
   fprintf(STDCTR,"(leed_ms_compl_sym):  Calculate scattering matrices\n");
#endif

/* 
   Calculate scattering matrix Tii[type] for Bravais lattices and store in
   p_Tii
*/
 for(i_atoms = 0; i_atoms < n_atoms; i_atoms ++)
 {
   i_type = (atoms+i_atoms)->type;
   if( p_Tii [i_type] == NULL )
   {
#ifdef CONTROL_X
   fprintf(STDCTR,"(leed_ms_compl_sym):  before leed_ms_tmat_ii\n");
#endif
     p_Tii[i_type] = 
         leed_ms_tmat_ii( p_Tii[i_type], Llm_ij, v_par->p_tl[i_type], l_max);
#ifdef CONTROL_X
   fprintf(STDCTR,"(leed_ms_compl_sym):  before mattrans\n");
#endif
     p_Tii[i_type] = 
         mattrans(p_Tii [i_type], p_Tii [i_type]);


#ifdef CONTROL_X
   fprintf(STDCTR,"(leed_ms_compl_sym):  before loop\n");
#endif
/* Multiply matrix elements of Tii[type] with -1/2k0 */
     cri_div(&faux_r, &faux_i, -0.5, 0., beams->k_r[0], beams->k_i[0]);
     for(ptr_r = (p_Tii[i_type])->rel + 1, ptr_i = (p_Tii[i_type])->iel + 1,
         ptr_end = (p_Tii[i_type])->rel +
                   (p_Tii[i_type])->cols * (p_Tii[i_type])->rows;
         ptr_r <= ptr_end; ptr_r ++, ptr_i ++)
     { cri_mul(ptr_r, ptr_i, *ptr_r, *ptr_i, faux_r, faux_i); }

#ifdef CONTROL_X
   fprintf(STDCTR,"(leed_ms_compl_sym): leed_ms_tmat_ii(%d)\n", (atoms+i_atoms)->type);
#endif
   }
 }

/********************************************************************** 
  Giant Matrix Inversion
  - Allocate giant matrix Mbg to be inverted.
  - Create interlayer propagators Gij/Gji (Maux) 
  - Calculate -Tii * Gij and -Tjj * Gji and copy into Mbg.
  - Add identity to Mbg and invert giant matrix.
  - free storage space for interlayer lattice sums.
**********************************************************************/

 iaux = l_max_2 * n_atoms;
 Mbg = matalloc(Mbg, iaux, iaux, NUM_COMPLEX);
 Mark = matalloc(Mark, n_atoms, n_atoms, NUM_REAL);

 for(i_atoms = 0, off_row = 1; i_atoms < n_atoms;
     i_atoms ++, off_row += l_max_2)
 {
   for(j_atoms = i_atoms + 1, off_col = off_row + l_max_2;
       j_atoms < n_atoms;
       j_atoms ++, off_col += l_max_2)
   if( ! IS_EQUAL_REAL(RMATEL(i_atoms+1, j_atoms+1, Mark), 1.))
   {
/* d_ij = vector rj - ri */
     d_ij[1] = (atoms+j_atoms)->pos[1] - (atoms+i_atoms)->pos[1];
     d_ij[2] = (atoms+j_atoms)->pos[2] - (atoms+i_atoms)->pos[2];
     d_ij[3] = (atoms+j_atoms)->pos[3] - (atoms+i_atoms)->pos[3];

#ifdef CONTROL_X
   fprintf(STDCTR,"(leed_ms_compl_sym): d(%d->%d) = (%5.2f, %5.2f, %5.2f) A\n",
           i_atoms, j_atoms, d_ij[1]*BOHR, d_ij[2]*BOHR, d_ij[3]*BOHR);
#endif

     leed_ms_lsum_ij ( &Llm_ij, &Llm_ji, beams->k_r[0], beams->k_i[0],
                  v_par->k_in, layer->a_lat, d_ij, 2 * l_max, v_par->epsilon );
#ifdef CONTROL_LSUM
     fprintf(STDCTR,"(leed_ms_compl_sym): Lij\n");
     Maux = mattrans(Maux, Llm_ij);
     matshow(Maux);
#endif
/*
   Copy matrix Tjj * Gji to position (j,i) = (off_col,off_row)
    and matrix Tii * Gij to position (i,j) = (off_row,off_col)
*/
     Maux = leed_ms_tmat_ij( Maux, Llm_ij, 
                        p_Tii[(atoms+j_atoms)->type], l_max);
     Mbg  = matins(Mbg, Maux, off_col, off_row);

     Maux = leed_ms_tmat_ij( Maux, Llm_ji, 
                        p_Tii[(atoms+i_atoms)->type], l_max);
     Mbg  = matins(Mbg, Maux, off_row, off_col);

/*
   - Mark the positions in Mark.
   - Check for other occurences of the same interlayer vector
*/

     RMATEL(i_atoms+1, j_atoms+1, Mark) = 1.;
     RMATEL(j_atoms+1, i_atoms+1, Mark) = 1.;

   } /* for j_atoms / if not marked */
 } /* for i_atoms */
 
 matfree(Llm_ij);
 matfree(Llm_ji);
 matfree(Mark);

/* Add identity to Mbg */
 for(ptr_r = Mbg->rel+1, ptr_end = Mbg->rel + Mbg->cols*Mbg->rows;
     ptr_r <= ptr_end; ptr_r += Mbg->cols +1)
   *ptr_r += 1.;

#ifdef CONTROL
   fprintf(STDCTR,
   "(leed_ms_compl_sym): giant matrix inversion (%d x %d), E = %.1f eV ...\n",
   Mbg->cols, Mbg->rows, v_par->eng_v*HART);
#endif

#ifdef CONTROL_MBG
   matnattovht(Mbg, l_max, n_atoms);
#endif

 CTIME("(leed_ms_compl_sym): before giant matrix inversion");

 Mbg = ms_partinv(Mbg, Mbg, n_plane, l_max);

/*  ALTERNATIVES
 Mbg = matinv(Mbg, Mbg);
 Mbg = ms_partinv(Mbg, Mbg, n_plane, l_max);
*/

#ifdef CONTROL
   fprintf(STDCTR,"(leed_ms_compl_sym): ... completed\n");
   fprintf(STDCTR,"(leed_ms_compl_sym):Mbg cols %d  Mbg rows %d \n",Mbg->cols,Mbg->rows); 
#endif
 CTIME("(leed_ms_compl_sym): after giant matrix inversion");

/********************************************************************** 
  Prepare matrices for conversion into plane waves:

   L_p(g',jlm) = Ylm(g'+) * exp(- ikg'(+) * rj)
   L_m(g',jlm) = Ylm(g'-) * exp(+ ikg'(-) * rj)
  to be multiplied with Mbg from the l.h.s. and 
   R_p(ilm',g)  = exp(+ ikg(+) * ri) *Tii * Ylm'*(g+) 
   R_m(ilm',g)  = exp(- ikg(-) * ri) *Tii * Ylm'*(g-) 
  to be multiplied from the r.h.s.

  Ylm = spherical harmonics.
  kg(+/-) = (kp, +/-kz)
    where kp = parallel componets of k
          kz = sqrt(2E - kp^2) (positive)

**********************************************************************/
#ifdef CONTROL_X
   fprintf(STDCTR,"(leed_ms_compl_sym): relative u.c. area: %.3f\n", layer->rel_area);
#endif


/* calculate spherical harmonics Ylm */

 Ylm = leed_ms_ymat(Ylm, l_max, beams, n_beams);

/* allocate storage space (Ylm->rows = number of beams) */
 iaux = l_max_2 * n_atoms;
 L_p = matalloc(L_p, n_beams, iaux, NUM_COMPLEX);
 L_m = matalloc(L_m, n_beams, iaux, NUM_COMPLEX);

 R_p = matalloc(R_p, iaux, n_beams, NUM_COMPLEX);
 R_m = matalloc(R_m, iaux, n_beams, NUM_COMPLEX);

#ifdef CONTROL
   fprintf(STDCTR,"(leed_ms_compl_sym): Prepare matrices R_x and L_x (%d x %d)\n",
           n_beams, iaux);
#endif

/* prefactor including relative area of unit cell */

 pref_i = -16.*PI*PI / layer->rel_area;

 for(i_atoms = 0;i_atoms  <  n_atoms; i_atoms++)
 {
   off_row = i_atoms * l_max_2 + 1;

/****************************************************************************
  R_p(ilm',g)  = exp(+ ikg(+) * ri) *Tii * Ylm'*(g+)
*****************************************************************************/
   Maux = leed_ms_yp_yxm(Maux, Ylm);
   Maux = matmul(Maux, p_Tii[(atoms+i_atoms)->type], Maux);
   Maux = leed_ms_comp_k_sum(Maux, beams, atoms+i_atoms, l_type,l_max,0);
   R_p  = matins(R_p, Maux, off_row, 1);

/***************************************************************************
   R_m(ilm',g)  = exp(+ ikg(-) * ri) *Tii * Ylm'*(g-)
****************************************************************************/
   Maux = leed_ms_yp_yxp(Maux, Ylm);
   Maux = matmul(Maux, p_Tii[(atoms+i_atoms)->type], Maux);
   Maux = leed_ms_comp_k_sum(Maux, beams, atoms+i_atoms, l_type,l_max,1);
   R_m  = matins(R_m, Maux, off_row, 1);

/*****************************************************************************
  L_p(g',jlm) = Ylm(g'+) * exp(- ikg'(+) * rj)
******************************************************************************/
   Maux = leed_ms_yp_ym(Maux, Ylm);
   
 /* Multiply the rows of Maux with exp(- ikg(+) * ri) */
   for(k = 0; k < Maux->rows; k ++)
   {
     faux_r = -(beams+k)->k_r[1] * (atoms+i_atoms)->pos[1]
              -(beams+k)->k_r[2] * (atoms+i_atoms)->pos[2]
              -(beams+k)->k_r[3] * (atoms+i_atoms)->pos[3];
     faux_i = -(beams+k)->k_i[3] * (atoms+i_atoms)->pos[3];
     cri_expi(&faux_r, &faux_i, faux_r, faux_i);
     cri_mul (&faux_r, &faux_i, faux_r, faux_i,
             (beams+k)->Akz_r, (beams+k)->Akz_i);
     cri_mul (&faux_r, &faux_i, faux_r, faux_i, 0., pref_i);

     for(ptr_r = Maux->rel + k*Maux->cols + 1,
         ptr_i = Maux->iel + k*Maux->cols + 1,
         ptr_end = ptr_r + Maux->cols;
         ptr_r < ptr_end; ptr_r ++,  ptr_i ++)
     { cri_mul(ptr_r, ptr_i, *ptr_r, *ptr_i, faux_r, faux_i); }
   } /* k */

   ptr_r = Maux->rel + 1;
   ptr_i = Maux->iel + 1;

   if(l_type == BULK)
   {
     for(i_beams = 0; i_beams < Maux->rows; i_beams ++)
     {
       faux_r = *((beams+i_beams)->eout_b_r+i_layer); 
       faux_i = *((beams+i_beams)->eout_b_i+i_layer);
       for(i_c = 0; i_c < Maux->cols; i_c ++, ptr_r ++, ptr_i ++ )
       {
         cri_mul(ptr_r, ptr_i, *ptr_r, *ptr_i, faux_r, faux_i); 
       }

     }  /* for i_beams */

   } /** if BULK **/ 


   if(l_type == OVER)
   {
     for(i_beams = 0; i_beams < Maux->rows; i_beams ++)
     {
       faux_r = *((beams+i_beams)->eout_s_r+i_layer);
       faux_i = *((beams+i_beams)->eout_s_i+i_layer);
       for(i_c = 0; i_c < Maux->cols; i_c ++, ptr_r ++, ptr_i ++ )
       {
         cri_mul(ptr_r, ptr_i, *ptr_r, *ptr_i, faux_r, faux_i);
       }

     }  /* for i_beams */

    }/** if OVER **/


   L_p  = matins(L_p, Maux, 1, off_row);

/*
  L_m(g',jlm) = Ylm(g'-) * exp(- ikg'(-) * rj)
*/
   Maux = matcopy(Maux, Ylm);

 /* Multiply the rows of Maux with exp(- ikg'(-) * ri) */
   for(k = 0; k < Maux->rows; k ++)
   {
     faux_r = -(beams+k)->k_r[1] * (atoms+i_atoms)->pos[1]
              -(beams+k)->k_r[2] * (atoms+i_atoms)->pos[2]
              +(beams+k)->k_r[3] * (atoms+i_atoms)->pos[3];
     faux_i = +(beams+k)->k_i[3] * (atoms+i_atoms)->pos[3];
     cri_expi(&faux_r, &faux_i, faux_r, faux_i);
     cri_mul (&faux_r, &faux_i, faux_r, faux_i,
             (beams+k)->Akz_r, (beams+k)->Akz_i);
     cri_mul (&faux_r, &faux_i, faux_r, faux_i, 0., pref_i);

     for(ptr_r = Maux->rel + k*Maux->cols + 1,
         ptr_i = Maux->iel + k*Maux->cols + 1,
         ptr_end = ptr_r + Maux->cols;
         ptr_r < ptr_end; ptr_r ++,  ptr_i ++)
     { cri_mul(ptr_r, ptr_i, *ptr_r, *ptr_i, faux_r, faux_i); }
   } /* k */

/**********************************NEW**************************************/
   ptr_r = Maux->rel + 1;
   ptr_i = Maux->iel + 1;

   if(l_type == BULK)
   {
     for(i_beams = 0; i_beams < Maux->rows; i_beams ++)
     {
       faux_r = *((beams+i_beams)->eout_b_r+i_layer);
       faux_i = *((beams+i_beams)->eout_b_i+i_layer);
       for(i_c = 0; i_c < Maux->cols; i_c ++, ptr_r ++, ptr_i ++ )
       {
         cri_mul(ptr_r, ptr_i, *ptr_r, *ptr_i, faux_r, faux_i);
       }

     }  /* for i_beams */

    }/** if BULK **/

   if(l_type == OVER)
   {
     for(i_beams = 0; i_beams < Maux->rows; i_beams ++)
     {
       faux_r = *((beams+i_beams)->eout_s_r+i_layer);
       faux_i = *((beams+i_beams)->eout_s_i+i_layer);
       for(i_c = 0; i_c < Maux->cols; i_c ++, ptr_r ++, ptr_i ++ )
       {
         cri_mul(ptr_r, ptr_i, *ptr_r, *ptr_i, faux_r, faux_i);
       }

     }  /* for i_beams */

   }/** if OVER **/ 

/**************************************************************************/

   L_m  = matins(L_m, Maux, 1, off_row);

 } /*** for i_atoms ***/

 CTIME("(leed_ms_compl_sym): after preparation of R_p ... ");
 matfree(Ylm);

/**********************************************************************
 Multiply matrices: L*Mbg*R
**********************************************************************/

 Maux = matmul(Maux, Mbg, R_p);
 Tpp = matmul(Tpp, L_p, Maux);
 Rmp = matmul(Rmp, L_m, Maux);

 Maux = matmul(Maux, Mbg, R_m);
 Tmm = matmul(Tmm, L_m, Maux);
 Rpm = matmul(Rpm, L_p, Maux);

 CTIME("(leed_ms_compl_sym): after multiplication R * Mbg * L");


#ifdef CONTROL
   fprintf(STDCTR,"(leed_ms_compl_sym): ... completed\n");
#endif

 matfree(Maux);
 matfree(Mbg);

/**********************************************************************
 Extrapolation of origin and Prefactor:

 Multiply the matrix elements with
  EXP = exp[i(+k'(+)rN - k(+)r1)] for ++
        exp[i(+k'(-)r1 - k(+)r1)] for -+
        exp[i(+k'(-)r1 - k(-)rN)] for --
        exp[i(+k'(+)rN - k(-)rN)] for +-

 r1 = (0, 0, (atoms+0)->pos[3]).
      Projection of the origin to the atomic subplane with the smallest
      z coordinate (atom No. 0):

 rN = (0, 0, (atoms+n_atoms-1)->pos[3]).
      Projection of the origin to the atomic subplane with the largest
      z coordinate (atom No. n_atoms-1):

 =>
  EXP = exp[i(+k'[3]rN - k[3]r1)] for ++
        exp[i(-k'[3]r1 - k[3]r1)] for -+
        exp[i(-k'[3]r1 + k[3]rN)] for --
        exp[i(+k'[3]rN + k[3]rN)] for +-

 Set up vectors L_p/m and R_p/m containing the appropriate factors and
 multiply the above matrices R/T with those vectors from l.h.s and r.h.s.,
 respectively.
 Loop over k' (exit beams): rows of T/Rk'k, i.e. l.h.s
  - Multiply with factor exp[- ik'(+/-)rN1]
  - reuse L_p/m
 Loop over k (incoming beams): cols of T/Rk'k, i.e. r.h.s
  - Multiply with factor exp[+ ik(+/-)r1N]
  - reuse R_p/m
**********************************************************************/

#ifdef CONTROL
   fprintf(STDCTR,"(leed_ms_compl_sym): origin shift ... \n");
#endif

 L_p = matalloc(L_p, 1, n_beams, NUM_COMPLEX);
 L_m = matalloc(L_m, 1, n_beams, NUM_COMPLEX);

 R_p = matalloc(R_p, n_beams, 1, NUM_COMPLEX);
 R_m = matalloc(R_m, n_beams, 1, NUM_COMPLEX);

/* Set up vectors L_p/m and R_p/m */

 for(k = 0; k < n_beams; k ++)
 {

/* R_m (exp[- ikz(-)zmax) = L_p (exp[+ ikz(+)zmax) */ 

   faux_r = +(beams+k)->k_r[3] * z_max;
   faux_i = +(beams+k)->k_i[3] * z_max;
   cri_expi(R_m->rel+k+1, R_m->iel+k+1, faux_r, faux_i);
   L_p->rel[k+1] = R_m->rel[k+1];
   L_p->iel[k+1] = R_m->iel[k+1];

/* R_p (exp[- ik(+)zmin) = L_m (exp[+ ik(-)zmin) */ 

   faux_r = -(beams+k)->k_r[3] * z_min;
   faux_i = -(beams+k)->k_i[3] * z_min;
   cri_expi(R_p->rel+k+1, R_p->iel+k+1, faux_r, faux_i);
   L_m->rel[k+1] = R_p->rel[k+1];
   L_m->iel[k+1] = R_p->iel[k+1];

 } /* k */

/*
  Final multiplications of matrix elements:
  Multiply cols with R_x and rows with L_x
*/

/*
  Tpp
*/
 for(k = 1; k <= Tpp->rows; k++)         /* loop over row No's (1st index) */
   for(ptr_r = Tpp->rel + (k-1)*Tpp->cols + 1,
       ptr_i = Tpp->iel + (k-1)*Tpp->cols + 1,
       ptr_end = ptr_r + Tpp->cols, l = 1;
       ptr_r < ptr_end;
       ptr_r ++, ptr_i ++, l ++)         /* loop over col No's (2nd index) */
   {
     cri_mul(ptr_r, ptr_i, *(L_p->rel+k), *(L_p->iel+k), *ptr_r, *ptr_i);
     cri_mul(ptr_r, ptr_i, *(R_p->rel+l), *(R_p->iel+l), *ptr_r, *ptr_i);
   }

/*
  Tmm
*/
 for(k = 1; k <= Tmm->rows; k++)         /* loop over row No's (1st index) */
   for(ptr_r = Tmm->rel + (k-1)*Tmm->cols + 1,
       ptr_i = Tmm->iel + (k-1)*Tmm->cols + 1,
       ptr_end = ptr_r + Tmm->cols, l = 1;
       ptr_r < ptr_end;
       ptr_r ++, ptr_i ++, l ++)         /* loop over col No's (2nd index) */
   {
     cri_mul(ptr_r, ptr_i, *(L_m->rel+k), *(L_m->iel+k), *ptr_r, *ptr_i);
     cri_mul(ptr_r, ptr_i, *(R_m->rel+l), *(R_m->iel+l), *ptr_r, *ptr_i);
   }

/*
  Rpm
*/
 for(k = 1; k <= Rpm->rows; k++)         /* loop over row No's (1st index) */
   for(ptr_r = Rpm->rel + (k-1)*Rpm->cols + 1,
       ptr_i = Rpm->iel + (k-1)*Rpm->cols + 1,
       ptr_end = ptr_r + Rpm->cols, l = 1;
       ptr_r < ptr_end;
       ptr_r ++, ptr_i ++, l ++)         /* loop over col No's (2nd index) */
   {
     cri_mul(ptr_r, ptr_i, *(L_p->rel+k), *(L_p->iel+k), *ptr_r, *ptr_i);
     cri_mul(ptr_r, ptr_i, *(R_m->rel+l), *(R_m->iel+l), *ptr_r, *ptr_i);
   }

/*
  Rmp
*/
 for(k = 1; k <= Rmp->rows; k++)         /* loop over row No's (1st index) */
   for(ptr_r = Rmp->rel + (k-1)*Rmp->cols + 1,
       ptr_i = Rmp->iel + (k-1)*Rmp->cols + 1,
       ptr_end = ptr_r + Rmp->cols, l = 1;
       ptr_r < ptr_end;
       ptr_r ++, ptr_i ++, l ++)         /* loop over col No's (2nd index) */
   {
     cri_mul(ptr_r, ptr_i, *(L_m->rel+k), *(L_m->iel+k), *ptr_r, *ptr_i);
     cri_mul(ptr_r, ptr_i, *(R_p->rel+l), *(R_p->iel+l), *ptr_r, *ptr_i);
   }

/*
  Add propagator of the unscattered wave to Tpp/Tmm: 
    exp[-ikz(+) * (zn - z1)]
*/

 for(iaux = 1, k = 0; k < n_beams; iaux += n_beams + 1, k ++)
 {
/* exp[-ikz(+) * (zn - z1)] */
   cri_mul(&faux_r, &faux_i, 
           L_p->rel[k+1], L_p->iel[k+1], R_p->rel[k+1], R_p->iel[k+1]);

   *(Tmm->rel+iaux) += faux_r;
   *(Tmm->iel+iaux) += faux_i;
   *(Tpp->rel+iaux) += faux_r;
   *(Tpp->iel+iaux) += faux_i;
 }

#ifdef CONTROL
   fprintf(STDCTR,"(leed_ms_compl): ... completed\n");
#endif

/**********************************************************************
 Free dummy matrices and copy results to p_R/T**
**********************************************************************/

 matfree(R_p);
 matfree(R_m);
 matfree(L_p);
 matfree(L_m);

 free(atoms);

 for(iaux = 0; iaux < n_type; iaux++) 
   if(p_Tii[iaux] != NULL) matfree( p_Tii[iaux] );

 free(p_Tii);

 *p_Tpp = Tpp;
 *p_Tmm = Tmm;
 *p_Rpm = Rpm;
 *p_Rmp = Rmp;

 CTIME("(leed_ms_compl): end of function");

 return(1);
} /* end of function leed_ms_compl */

/*======================================================================*/