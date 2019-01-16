#include "sec_osal_light.h"
#include "sec_cust_struct.h"
#include "bgn_internal.h"
#include "bgn_asm.h"

#define MOD "BGN"

/**************************************************************************
 *  TYPEDEF
 **************************************************************************/
typedef unsigned int uint32;
typedef unsigned char uchar;


/**************************************************************************
 *  FUNCTIONS
 **************************************************************************/
void montg_init( ulong *mm, const bgn *P_N )
{
    ulong x, m0 = P_N->p[0];

    x  = m0;
    x += ( ( m0 + 2 ) & 4 ) << 1;
    x *= ( 2 - ( m0 * x ) );

    if( biL >= 16 ) 
    {   
        x *= ( 2 - ( m0 * x ) );
    }
    
    if( biL >= 32 )
    {
        x *= ( 2 - ( m0 * x ) );
    }
    
    if( biL >= 64 ) 
    {
        x *= ( 2 - ( m0 * x ) );
    }

    *mm = ~x + 1;
}

void montg_mul( bgn *P_A, const bgn *P_B, const bgn *P_N, ulong mm, const bgn *P_T )
{
    int i, n, m;
    ulong u0, u1, *d;

    memset( P_T->p, 0, P_T->n * ciL );

    d = P_T->p;
    n = P_N->n;
    m = ( P_B->n < n ) ? P_B->n : n;

    for( i = 0; i < n; i++ )
    {
        u0 = P_A->p[i];
        u1 = ( d[0] + u0 * P_B->p[0] ) * mm;

        bgn_mul_hlp( m, P_B->p, d, u0 );
        bgn_mul_hlp( n, P_N->p, d, u1 );

        *d++ = u0; d[n + 1] = 0;
    }

    memcpy( P_A->p, d, (n + 1) * ciL );

    if( bgn_cmp_abs( P_A, P_N ) >= 0 )
    {
        bgn_sub_hlp( n, P_N->p, P_A->p );
    }
    else
    {
        bgn_sub_hlp( n, P_A->p, P_T->p );
    }
}

void montg_red( bgn *P_A, const bgn *P_N, ulong mm, const bgn *P_T )
{
    ulong z = 1;
    bgn U;

    U.n = U.s = z;
    U.p = &z;

    montg_mul( P_A, &U, P_N, mm, P_T );
}
