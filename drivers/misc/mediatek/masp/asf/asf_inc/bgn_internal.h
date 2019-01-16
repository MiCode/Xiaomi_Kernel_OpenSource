#ifndef _BIGNUM_INTERNAL_H
#define _BIGNUM_INTERNAL_H

#include "bgn_export.h"

/**************************************************************************
 *  INTERNAL DEFINIITION
 **************************************************************************/
#define ciL    ((int) sizeof(unsigned long))
#define biL    (ciL << 3)               
#define biH    (ciL << 2) 

#define B_T_L(i)  (((i) + biL - 1) / biL)
#define C_T_L(i) (((i) + ciL - 1) / ciL)

/**************************************************************************
 *  INTERNAL FUNCTIONS
 **************************************************************************/
void bgn_init( bgn *P_X );
void bgn_free( bgn *P_X );
int bgn_grow( bgn *P_X, int nbl );
int bgn_copy( bgn *P_X, const bgn *P_Y );
void bgn_swap( bgn *P_X, bgn *P_Y );
int bgn_lset( bgn *P_X, int z );
int bgn_lsb( const bgn *P_X );
int bgn_msb( const bgn *P_X );
int bgn_size( const bgn *P_X );
int bgn_shift_l( bgn *P_X, int count );
int bgn_shift_r( bgn *P_X, int count );
int bgn_cmp_abs( const bgn *P_X, const bgn *P_Y );
int bgn_cmp_num( const bgn *P_X, const bgn *P_Y );
int bgn_cmp_int( const bgn *P_X, int z );
int bgn_add_abs( bgn *P_X, const bgn *P_A, const bgn *P_B );
int bgn_sub_abs( bgn *P_X, const bgn *P_A, const bgn *P_B );
int bgn_add_bgn( bgn *P_X, const bgn *P_A, const bgn *P_B );
int bgn_sub_bgn( bgn *P_X, const bgn *P_A, const bgn *P_B );
int bgn_add_int( bgn *P_X, const bgn *P_A, int b );
int bgn_sub_int( bgn *P_X, const bgn *P_A, int b );
int bgn_mul_bgn( bgn *P_X, const bgn *P_A, const bgn *B );
int bgn_mul_int( bgn *P_X, const bgn *P_A, unsigned long b );
int bgn_div_bgn( bgn *P_Q, bgn *P_R, const bgn *P_A, const bgn *P_B );
int bgn_div_int( bgn *P_Q, bgn *P_R, const bgn *P_A, int b );
int bgn_mod_bgn( bgn *P_R, const bgn *P_A, const bgn *P_B );
int bgn_mod_int( unsigned long *r, const bgn *P_A, int b );
int bgn_exp_mod( bgn *P_X, const bgn *P_E, const bgn *P_N, bgn *_RR );
int bgn_inv_mod( bgn *P_X, const bgn *P_A, const bgn *P_N );
int bgn_is_prime( bgn *P_X, int (*f_rng)(void *), void *p_rng );

void montg_init( unsigned long *mm, const bgn *P_N );
void montg_mul( bgn *P_A, const bgn *P_B, const bgn *P_N, unsigned long mm, const bgn *P_T );
void montg_red( bgn *P_A, const bgn *P_N, unsigned long mm, const bgn *P_T );
void bgn_sub_hlp( int n, unsigned long *s, unsigned long *d );
void bgn_mul_hlp( int i, unsigned long *s, unsigned long *d, unsigned long b );

#endif
