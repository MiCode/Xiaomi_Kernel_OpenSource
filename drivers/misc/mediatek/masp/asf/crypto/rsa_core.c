#include "sec_osal_light.h"
#include <mach/sec_osal.h>
#include "sec_cust_struct.h"
#include "rsa_def.h"
#include "bgn_internal.h"

#define MOD "RSA"

/**************************************************************************
 *  TYPEDEF
 **************************************************************************/
typedef unsigned int uint32;
typedef unsigned char uchar;


/**************************************************************************
*  EXTERNAL VARIABLE
**************************************************************************/
extern uchar                        sha1sum[];

/**************************************************************************
*  GLOBAL VARIABLES
**************************************************************************/
rsa_ctx                             rsa;
uchar                               rsa_ci[RSA_KEY_LEN];
static unsigned char *rsa_buf = NULL;

/**************************************************************************
 *  RSA DEBUG FLAG
 **************************************************************************/
#define RSA_DEBUG_LOG               (0)


/**************************************************************************
 *  DEBUG DEFINITION
 **************************************************************************/
#if RSA_DEBUG_LOG
#define RSA_LOG                     printf
#else
#define RSA_LOG                   
#endif


/**************************************************************************
 *  INTERNAL DEFINIITION
 **************************************************************************/
#define MASK 0xFF
#define RSA_BUF_SIZE 1024


/**************************************************************************
 *  INTERNAL FUNCTIONS
 **************************************************************************/
void rsa_init( rsa_ctx *ctx, int pad, int h_id, int (*f_rng)(void *), void *p_rng );
int rsa_pub( rsa_ctx *ctx, const uchar *ip, uchar *op );
int rsa_pri( rsa_ctx *ctx, const uchar *ip, uchar *op );
void rsa_free( rsa_ctx *ctx );

/**************************************************************************
 *  FUNCTIONS
 **************************************************************************/
void rsa_init( rsa_ctx *ctx, int pad, int h_id,  int (*f_rng)(void *), void *p_rng )
{
    memset( ctx, 0, sizeof( rsa_ctx ) );

    ctx->pad = pad;
    ctx->h_id = h_id;

    ctx->f_rng = f_rng;
    ctx->p_rng = p_rng;
}

int rsa_pub( rsa_ctx *ctx, const uchar *ip, uchar *op )
{
    int ret = 0, olen = 0;
    bgn B;

    bgn_init( &B );

    /* if no RSA pad, input data won't be extended to key length */
    if(0 != (ret = bgn_read_bin( &B, ip, RSA_KEY_LEN )))
    {
        goto _exit;
    }

    if( bgn_cmp_num( &B, &ctx->N ) >= 0 )
    {
        bgn_free( &B );
        ret = E_RSA_BAD_INPUT_DATA;
        goto _err;
    }

    olen = ctx->len;

    /* ============================================================ */
    /* Following is the formula of RSA public key crypto algorithm  */
    /*                                                              */
    /* => public key use ctx->E (public exponent)                   */
    /*                                                              */
    /* => Message = Cipher ^ (&ctx->E) mod (&ctx->N)                */
    /*                                                              */
    /* ============================================================ */
    if(0 != (ret = bgn_exp_mod( &B, &ctx->E, &ctx->N, &ctx->RN )))
    {
        goto _exit;
    }
    
    if(0 != (ret = bgn_write_bin( &B, op, olen )))
    {
        goto _exit;
    }

_exit:

    bgn_free( &B );

    if( 0 != ret )
    {   
        //RSA_LOG("[%s] E_RSA_PUBLIC_FAILED\n",MOD);
        ret = E_RSA_PUBLIC_FAILED;
        goto _err;
    }

    return 0;

_err:    

    return ret;
}

int rsa_pri( rsa_ctx *ctx, const uchar *ip, uchar *op )
{
    int ret = 0, olen = 0;
    bgn B, B1, B2;

    bgn_init( &B );  
    bgn_init( &B1 ); 
    bgn_init( &B2 );  

    if(0 != (ret = bgn_read_bin( &B, ip, ctx->len )))
    {
        goto _exit;
    }

    if( bgn_cmp_num( &B, &ctx->N ) >= 0 )
    {
        bgn_free( &B );
        ret = E_RSA_BAD_INPUT_DATA;
        goto _err;
    }

    /* ============================================================ */
    /* Following is the formula of RSA private key crypto algorithm */
    /*                                                              */
    /* => private key use ctx->D (private exponent)                 */
    /*                                                              */
    /* => M' = M ^ (&ctx->D) mod (&ctx->N)                          */
    /*                                                              */
    /* ============================================================ */
    if(0 != (ret = bgn_exp_mod( &B, &ctx->D, &ctx->N, &ctx->RN )))
    {
        goto _exit;
    }
    
    olen = ctx->len;

    if(0 != (ret = bgn_write_bin( &B, op, olen )))
    {
        goto _exit;
    }

_exit:

    bgn_free( &B );
    bgn_free( &B1 );
    bgn_free( &B2 );

    if( ret != 0 )
    {
        //RSA_LOG("[%s] E_RSA_PRIVATE_FAILED\n",MOD);           
        ret = E_RSA_PRIVATE_FAILED;
        goto _err;
        
    }

    return 0;

_err:     
    
    return ret;
}

int rsa_sign( rsa_ctx *ctx, int h_len, const uchar *hash, uchar *sig )
{
    int ret = 0;
    int nb_pad = 0, olen = 0;
    uchar *p = sig;

    /* ---------------- */
    /* init             */
    /* ---------------- */    
    olen = ctx->len;
    *p++ = 0;
    *p++ = RSA_SIGN;    

    /* ---------------- */    
    /* PKCS 1.5         */
    /* ---------------- */   
    nb_pad = olen - 3 - h_len; 

    if( nb_pad < 8 )
    {
        ret = E_RSA_BAD_INPUT_DATA;
        goto _err;
    }

    memset( p, MASK, nb_pad );
    p = p + nb_pad;
    *p++ = 0;

    /* ---------------- */
    /* RAW padding      */
    /* ---------------- */    
    memcpy( p, hash, h_len );

    /* ---------------- */
    /* private key      */
    /* ---------------- */    
    return rsa_pri( ctx, sig, sig );

_err: 

    return ret;
}


int rsa_verify( rsa_ctx *ctx, int h_len, const uchar *hash, uchar *sig )
{
    int ret = 0, len = 0, siglen = 0;
    uchar *p;
    uchar *buf;

    if(rsa_buf == NULL)
    {
        rsa_buf = (unsigned char *)osal_kmalloc(RSA_BUF_SIZE);
    }

    memset(rsa_buf, 0x00, RSA_BUF_SIZE);

    buf = rsa_buf;

    siglen = ctx->len;

    if( siglen < 16 || siglen > RSA_BUF_SIZE )
    {
        ret = E_RSA_BAD_INPUT_DATA;
        goto _err;
    }

    /* ---------------- */
    /* public key       */
    /* ---------------- */    
    if(0 != (ret = rsa_pub( ctx, sig, buf )))
    {
        goto _err;
    }    

    p = buf;

    /* ---------------- */
    /* PKCS 1.5         */
    /* ---------------- */    
    if( *p++ != 0 || *p++ != RSA_SIGN )
    {
        ret = E_RSA_INVALID_PADDING;
        goto _err;        
    }

    while( *p != 0 )
    {
        if( p >= buf + siglen - 1 || *p != MASK )
        {
            ret = E_RSA_INVALID_PADDING;
            goto _err;            
        }

        p++;
    }
            
    p++;

    len = siglen - (int)( p - buf );

    /* ---------------- */
    /* RAW padding      */    
    /* ---------------- */    
    if( len == h_len)
    {
        if( 0 == memcmp( p, hash, h_len ) )
        {
            return 0;
        }
        else
        {
            ret = E_RSA_VERIFY_FAILED;
            goto _err;              
        }
    } 

    ret = E_RSA_INVALID_PADDING;

_err: 

    return ret;    
}

void rsa_free( rsa_ctx *ctx )
{
    bgn_free( &ctx->RQ );
    bgn_free( &ctx->RP );
    bgn_free( &ctx->RN );
    bgn_free( &ctx->D );
    bgn_free( &ctx->E );
    bgn_free( &ctx->N );
}

