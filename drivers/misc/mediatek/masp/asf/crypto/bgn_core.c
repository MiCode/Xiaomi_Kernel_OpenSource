#include "sec_osal_light.h"
#include <mach/sec_osal.h>
#include "sec_cust_struct.h"
#include "bgn_internal.h"
#include "bgn_asm.h"

#define MOD "BGN"

/**************************************************************************
 *  DEBUG DEFINITION
 **************************************************************************/
#define SMSG                 	printk

/**************************************************************************
 *  TYPEDEF
 **************************************************************************/
typedef unsigned int uint32;
typedef unsigned char uchar;

/**************************************************************************
 *  INIT/FREE FUNCTIONS
 **************************************************************************/
 
void bgn_init( bgn *X )
{
    X->s = 1;
    X->n = 0;
    X->p = NULL;
}

void bgn_free( bgn *X )
{
    if( X->p != NULL )
    {
        memset( X->p, 0, X->n * ciL );
        osal_kfree( X->p );
        
    }
    X->s = 1;
    X->n = 0;
    X->p = NULL;
    
}

/**************************************************************************
 *  CORE FUNCTION
 **************************************************************************/  
int bgn_exp_mod( bgn *X, const bgn *E, const bgn *N, bgn *P_RR )
{
    ulong ei, mm, state;
    bgn RR, T, W[64];
    int ret, i, j, ws, wbits;
    int bs, nbl, nbi;
   
    if( bgn_cmp_int( N, 0 ) < 0 || ( N->p[0] & 1 ) == 0 )
    {
        return( E_BGN_BAD_INPUT_DATA );
    }

    montg_init( &mm, N );

    bgn_init( &RR );
    bgn_init( &T );

    memset( W, 0, sizeof( W ) );

    /* ws = 6. only accept E = 0x10001 */
    //ws = 6;
    i = bgn_msb( E );    
    ws = ( i > 671 ) ? 6 : ( i > 239 ) ? 5 : ( i >  79 ) ? 4 : ( i >  23 ) ? 3 : 1;
    

    j = N->n + 1;

    /* ------------------------------------------ */
    /* bgn_grow : enlarge to the specified number */
    /* ------------------------------------------ */
    if(0 != (ret = bgn_grow( X, j )))
    {
        goto _exit;
    }
    
    if(0 != (ret = bgn_grow( &W[1],  j )))
    {
        goto _exit;
    }
    
    if(0 != (ret = bgn_grow( &T, j * 2 )))
    {
        goto _exit;
    }

    /* --------------------- */    
    /* P_RR = NULL           */
    /* --------------------- */    
    if(0 != (ret = bgn_lset( &RR, 1 )))
    {
        goto _exit;
    }
        
    if(0 != (ret = bgn_shift_l( &RR, N->n * 2 * biL )))
    {
        goto _exit;
    }
        
    if(0 != (ret = bgn_mod_bgn( &RR, &RR, N )))
    {
        goto _exit;
    }

    if( P_RR != NULL )
    {
        memcpy( P_RR, &RR, sizeof( bgn ) );
    }
        
    /* --------------------- */    
    /* compare signed values */
    /* --------------------- */    
    if( bgn_cmp_num( X, N ) >= 0 )
    {
        bgn_mod_bgn( &W[1], X, N );
    }
    else
    {   
        bgn_copy( &W[1], X );
    }

    /* --------------------- */    
    /* A = A* B * R^-1 mod N */
    /* --------------------- */    
    montg_mul( &W[1], &RR, N, mm, &T );

    if(0 != (ret = bgn_copy( X, &RR )))
    {
        goto _exit;
    }
    
    montg_red( X, N, mm, &T );


    /* --------------------- */    
    /* ws > 1                */
    /* --------------------- */    
    j =  1 << (ws - 1);

    if(0 != (ret = bgn_grow( &W[j], N->n + 1 )))
    {
        goto _exit;
    }
        
    if(0 != (ret = bgn_copy( &W[j], &W[1]    )))
    {
        goto _exit;
    }

    for( i = 0; i < ws - 1; i++ )
    {
        montg_mul( &W[j], &W[j], N, mm, &T );
    }
    
    for( i = j + 1; i < (1 << ws); i++ )
    {
        if(0 != (ret = bgn_grow( &W[i], N->n + 1 )))
        {
            goto _exit;
        }

        if(0 != (ret = bgn_copy( &W[i], &W[i - 1] )))
        {
            goto _exit;
        }

        montg_mul( &W[i], &W[1], N, mm, &T );
    }

    nbl = E->n;
    bs = 0;
    nbi   = 0;
    wbits   = 0;
    state   = 0;

    while( 1 )
    {
        if( bs == 0 )
        {
            if( nbl-- == 0 )
            {
                break;
            }
            
            bs = sizeof( ulong ) << 3;
        }

        bs--;

        ei = (E->p[nbl] >> bs) & 1;

        if( ei == 0 && state == 0 )
        {
            continue;
        }
        
        if( ei == 0 && state == 1 )
        {
            montg_mul( X, X, N, mm, &T );
            continue;
        }

        state = 2;

        nbi++;
        wbits |= (ei << (ws - nbi));

        if( nbi == ws )
        {
            for( i = 0; i < ws; i++ )
            {
                montg_mul( X, X, N, mm, &T );
            }

            montg_mul( X, &W[wbits], N, mm, &T );

            state--;
            nbi = 0;
            wbits = 0;
        }
    }

    for( i = 0; i < nbi; i++ )
    {
        montg_mul( X, X, N, mm, &T );

        wbits <<= 1;

        if( (wbits & (1 << ws)) != 0 )
        {
            montg_mul( X, &W[1], N, mm, &T );
        }
    }

    montg_red( X, N, mm, &T );

_exit:

    for( i = (1 << (ws - 1)); i < (1 << ws); i++ )
    {
        bgn_free( &W[i] );
    }
    
    if( P_RR != NULL )
    {
        bgn_free( &W[1] );
        bgn_free( &T );
    }
    else 
    {   
        bgn_free( &W[1] );
        bgn_free( &T );
        bgn_free( &RR );
    }

    return ret;
}

