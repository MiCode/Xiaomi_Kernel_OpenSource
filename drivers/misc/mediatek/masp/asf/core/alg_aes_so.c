#include "sec_osal_light.h"
#include "sec_log.h"
#include "aes_so.h"

/**************************************************************************
 *  TYPEDEF
 **************************************************************************/
typedef unsigned int uint32;
typedef unsigned char uchar;

/**************************************************************************
 *  DEFINITIONS
 **************************************************************************/
#define MOD                             "AES_SO"
#define CIPHER_BLOCK_SIZE               (16)

#define CT_AES128_LEN                   16      // 16B (AES128)
#define CT_AES192_LEN                   24      // 24B (AES192)
#define CT_AES256_LEN                   32      // 32B (AES256)

/**************************************************************************
 *  EXTERNAL FUNCTION
 **************************************************************************/
extern void * mcpy(void *dest, const void *src, int  cnt);

/**************************************************************************
 *  INTERNAL DEFINIITION
 **************************************************************************/
#define MASK 0xFF
#define T_SZ 256

#define EXP(x,y) (x^y)

/**************************************************************************
 *  GLOBAL VARIABLES
 **************************************************************************/
uint32                                  aes_key_len = 0;

static uchar FS[T_SZ];
static ulong FT0[T_SZ]; 
static ulong FT1[T_SZ]; 
static ulong FT2[T_SZ]; 
static ulong FT3[T_SZ]; 

static uchar RS[T_SZ];
static ulong RT0[T_SZ];
static ulong RT1[T_SZ];
static ulong RT2[T_SZ];
static ulong RT3[T_SZ];

static ulong RCON[10];

static int aes_init_done = 0;

static int pow[T_SZ];
static int log[T_SZ];

#define A_F(X0,X1,X2,X3,Y0,Y1,Y2,Y3)     \
{                                               \
    X0 = *RK++ ^ FT0[ ( Y0       ) & MASK ] ^   \
                 FT1[ ( Y1 >>  8 ) & MASK ] ^   \
                 FT2[ ( Y2 >> 16 ) & MASK ] ^   \
                 FT3[ ( Y3 >> 24 ) & MASK ];    \
                                                \
    X1 = *RK++ ^ FT0[ ( Y1       ) & MASK ] ^   \
                 FT1[ ( Y2 >>  8 ) & MASK ] ^   \
                 FT2[ ( Y3 >> 16 ) & MASK ] ^   \
                 FT3[ ( Y0 >> 24 ) & MASK ];    \
                                                \
    X2 = *RK++ ^ FT0[ ( Y2       ) & MASK ] ^   \
                 FT1[ ( Y3 >>  8 ) & MASK ] ^   \
                 FT2[ ( Y0 >> 16 ) & MASK ] ^   \
                 FT3[ ( Y1 >> 24 ) & MASK ];    \
                                                \
    X3 = *RK++ ^ FT0[ ( Y3       ) & MASK ] ^   \
                 FT1[ ( Y0 >>  8 ) & MASK ] ^   \
                 FT2[ ( Y1 >> 16 ) & MASK ] ^   \
                 FT3[ ( Y2 >> 24 ) & MASK ];    \
}

#define A_R(X0,X1,X2,X3,Y0,Y1,Y2,Y3)     \
{                                               \
    X0 = *RK++ ^ RT0[ ( Y0       ) & MASK ] ^   \
                 RT1[ ( Y3 >>  8 ) & MASK ] ^   \
                 RT2[ ( Y2 >> 16 ) & MASK ] ^   \
                 RT3[ ( Y1 >> 24 ) & MASK ];    \
                                                \
    X1 = *RK++ ^ RT0[ ( Y1       ) & MASK ] ^   \
                 RT1[ ( Y0 >>  8 ) & MASK ] ^   \
                 RT2[ ( Y3 >> 16 ) & MASK ] ^   \
                 RT3[ ( Y2 >> 24 ) & MASK ];    \
                                                \
    X2 = *RK++ ^ RT0[ ( Y2       ) & MASK ] ^   \
                 RT1[ ( Y1 >>  8 ) & MASK ] ^   \
                 RT2[ ( Y0 >> 16 ) & MASK ] ^   \
                 RT3[ ( Y3 >> 24 ) & MASK ];    \
                                                \
    X3 = *RK++ ^ RT0[ ( Y3       ) & MASK ] ^   \
                 RT1[ ( Y2 >>  8 ) & MASK ] ^   \
                 RT2[ ( Y1 >> 16 ) & MASK ] ^   \
                 RT3[ ( Y0 >> 24 ) & MASK ];    \
}


/**************************************************************************
 *  MTK SECRET
 **************************************************************************/
static uint32 g_AES_IV[4]= {
    0x6c8d3259, 0x86911412, 0x55975412, 0x6c8d3257
};

static uint32 g_AES_IV_TEMP[4]= {
    0x0,0x0,0x0,0x0
};

uint32 g_AES_Key[4] = {
    0x0, 0x0, 0x0, 0x0
};


/**************************************************************************
 *  INTERNAL VARIABLES
 **************************************************************************/
a_ctx aes;

#ifndef G_U_LE
#define G_U_LE(n,b,i)                           \
{                                               \
    (n) = ( (ulong) (b)[(i)    ]       )        \
        | ( (ulong) (b)[(i) + 1] <<  8 )        \
        | ( (ulong) (b)[(i) + 2] << 16 )        \
        | ( (ulong) (b)[(i) + 3] << 24 );       \
}
#endif

#ifndef P_U_LE
#define P_U_LE(n,b,i)                           \
{                                               \
    (b)[(i)    ] = (uchar) ( (n)       );       \
    (b)[(i) + 1] = (uchar) ( (n) >>  8 );       \
    (b)[(i) + 2] = (uchar) ( (n) >> 16 );       \
    (b)[(i) + 3] = (uchar) ( (n) >> 24 );       \
}
#endif

#define ROTL8(x) ( ( x << 8 ) & 0xFFFFFFFF ) | ( x >> 24 )
#define XTIME(x) ( ( x << 1 ) ^ ( ( x & 0x80 ) ? 0x1B : 0x00 ) )
#define MUL(x,y) ( ( x && y ) ? pow[(log[x]+log[y]) % 255] : 0 )

/**************************************************************************
 *  FUNCTIONS
 **************************************************************************/
static void a_gen_tables( void )
{
    int i, x, y, z;

    for( i = 0, x = 1; i < T_SZ; i++ )
    {
        pow[i] = x;
        log[x] = i;
        x = ( x ^ XTIME( x ) ) & MASK;
    }

    for( i = 0, x = 1; i < 10; i++ )
    {
        RCON[i] = (ulong) x;
        x = XTIME( x ) & MASK;
    }

    FS[0x00] = 0x63;
    RS[0x63] = 0x00;

    for( i = 1; i < T_SZ; i++ )
    {
        x = pow[255 - log[i]];

        y  = x; y = ( (y << 1) | (y >> 7) ) & MASK;
        x ^= y; y = ( (y << 1) | (y >> 7) ) & MASK;
        x ^= y; y = ( (y << 1) | (y >> 7) ) & MASK;
        x ^= y; y = ( (y << 1) | (y >> 7) ) & MASK;
        x ^= y ^ 0x63;

        FS[i] = (uchar) x;
        RS[x] = (uchar) i;
    }

    for( i = 0; i < T_SZ; i++ )
    {
        x = FS[i];
        y = XTIME( x ) & MASK;
        z =  ( y ^ x ) & MASK;

        FT0[i] = ( (ulong) y       ) ^
                     ( (ulong) x <<  8 ) ^
                     ( (ulong) x << 16 ) ^
                     ( (ulong) z << 24 );

        FT1[i] = ROTL8( FT0[i] );
        FT2[i] = ROTL8( FT1[i] );
        FT3[i] = ROTL8( FT2[i] );

        x = RS[i];

        RT0[i] = ( (ulong) MUL( 0x0E, x )       ) ^
                     ( (ulong) MUL( 0x09, x ) <<  8 ) ^
                     ( (ulong) MUL( 0x0D, x ) << 16 ) ^
                     ( (ulong) MUL( 0x0B, x ) << 24 );

        RT1[i] = ROTL8( RT0[i] );
        RT2[i] = ROTL8( RT1[i] );
        RT3[i] = ROTL8( RT2[i] );
    }
}


int a_enc (a_ctx *ctx, const uchar *key, uint32 keysize)
{
    uint32 i;
    ulong *RK;

    if( aes_init_done == 0 )
    {
        a_gen_tables();
        aes_init_done = 1;
    }

    switch( keysize )
    {
        case 128: 
            ctx->nr = 10; 
            break;
        case 192: 
            ctx->nr = 12; 
            break;
        case 256: 
            ctx->nr = 14; 
            break;
        default : 
            return( -1 );
    }

    ctx->rk = RK = ctx->buf;

    for( i = 0; i < (keysize >> 5); i++ )
    {
        G_U_LE( RK[i], key, i << 2 );
    }

    switch( ctx->nr )
    {
        case 10:

            for( i = 0; i < 10; i++, RK += 4 )
            {
                RK[4]  = RK[0] ^ RCON[i] ^
                ( (ulong) FS[ ( RK[3] >>  8 ) & MASK ]       ) ^
                ( (ulong) FS[ ( RK[3] >> 16 ) & MASK ] <<  8 ) ^
                ( (ulong) FS[ ( RK[3] >> 24 ) & MASK ] << 16 ) ^
                ( (ulong) FS[ ( RK[3]       ) & MASK ] << 24 );

                RK[5]  = EXP(RK[1],RK[4]);
                RK[6]  = EXP(RK[2],RK[5]);
                RK[7]  = EXP(RK[3],RK[6]);
            }
            break;

        case 12:

            for( i = 0; i < 8; i++, RK += 6 )
            {
                RK[6]  = RK[0] ^ RCON[i] ^
                ( (ulong) FS[ ( RK[5] >>  8 ) & MASK ]       ) ^
                ( (ulong) FS[ ( RK[5] >> 16 ) & MASK ] <<  8 ) ^
                ( (ulong) FS[ ( RK[5] >> 24 ) & MASK ] << 16 ) ^
                ( (ulong) FS[ ( RK[5]       ) & MASK ] << 24 );

                RK[7]  = EXP(RK[1],RK[6]);
                RK[8]  = EXP(RK[2],RK[7]);
                RK[9]  = EXP(RK[3],RK[8]);
                RK[10] = EXP(RK[4],RK[9]);
                RK[11] = EXP(RK[5],RK[10]);
            }
            break;

        case 14:

            for( i = 0; i < 7; i++, RK += 8 )
            {
                RK[8]  = RK[0] ^ RCON[i] ^
                ( (ulong) FS[ ( RK[7] >>  8 ) & MASK ]       ) ^
                ( (ulong) FS[ ( RK[7] >> 16 ) & MASK ] <<  8 ) ^
                ( (ulong) FS[ ( RK[7] >> 24 ) & MASK ] << 16 ) ^
                ( (ulong) FS[ ( RK[7]       ) & MASK ] << 24 );

                RK[9]  = EXP(RK[1],RK[8]);
                RK[10] = EXP(RK[2],RK[9]);
                RK[11] = EXP(RK[3],RK[10]);

                RK[12] = RK[4] ^
                ( (ulong) FS[ ( RK[11]       ) & MASK ]       ) ^
                ( (ulong) FS[ ( RK[11] >>  8 ) & MASK ] <<  8 ) ^
                ( (ulong) FS[ ( RK[11] >> 16 ) & MASK ] << 16 ) ^
                ( (ulong) FS[ ( RK[11] >> 24 ) & MASK ] << 24 );

                RK[13] = EXP(RK[5],RK[12]);
                RK[14] = EXP(RK[6],RK[13]);
                RK[15] = EXP(RK[7],RK[14]);
            }
            break;

        default:

            break;
    }

    return( 0 );
}

int a_dec (a_ctx *ctx, const uchar *key, uint32 keysize)
{
    int i, j;
    a_ctx cty;
    ulong *RK;
    ulong *SK;
    int ret;

    switch( keysize )
    {
        case 128: 
            ctx->nr = 10; 
            break;
        case 192: 
            ctx->nr = 12; 
            break;
        case 256: 
            ctx->nr = 14; 
            break;
        default : 
            return( -1 );
    }

    ctx->rk = RK = ctx->buf;

    ret = a_enc( &cty, key, keysize );
    if( ret != 0 )
        return( ret );

    SK = cty.rk + cty.nr * 4;

    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;

    for( i = ctx->nr - 1, SK -= 8; i > 0; i--, SK -= 8 )
    {
        for( j = 0; j < 4; j++, SK++ )
        {
            *RK++ = RT0[ FS[ ( *SK       ) & MASK ] ] ^
                    RT1[ FS[ ( *SK >>  8 ) & MASK ] ] ^
                    RT2[ FS[ ( *SK >> 16 ) & MASK ] ] ^
                    RT3[ FS[ ( *SK >> 24 ) & MASK ] ];
        }
    }

    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;

    memset( &cty, 0, sizeof( a_ctx ) );

    return( 0 );
}

int a_crypt_ecb( a_ctx *ctx,
                    int mode,
                    const uchar input[16],
                    uchar output[16] )
{
    int i;
    ulong *RK, X0, X1, X2, X3, Y0, Y1, Y2, Y3;

    RK = ctx->rk;

    G_U_LE( X0, input,  0 ); X0 ^= *RK++;
    G_U_LE( X1, input,  4 ); X1 ^= *RK++;
    G_U_LE( X2, input,  8 ); X2 ^= *RK++;
    G_U_LE( X3, input, 12 ); X3 ^= *RK++;

    /* ----------- */
    /* AES_DECRYPT */
    /* ----------- */   
    if( mode == AES_DECRYPT )
    {
        for( i = (ctx->nr >> 1) - 1; i > 0; i-- )
        {
            A_R( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );
            A_R( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );
        }

        A_R( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );

        X0 = *RK++ ^ \
                ( (ulong) RS[ ( Y0       ) & MASK ]       ) ^
                ( (ulong) RS[ ( Y3 >>  8 ) & MASK ] <<  8 ) ^
                ( (ulong) RS[ ( Y2 >> 16 ) & MASK ] << 16 ) ^
                ( (ulong) RS[ ( Y1 >> 24 ) & MASK ] << 24 );

        X1 = *RK++ ^ \
                ( (ulong) RS[ ( Y1       ) & MASK ]       ) ^
                ( (ulong) RS[ ( Y0 >>  8 ) & MASK ] <<  8 ) ^
                ( (ulong) RS[ ( Y3 >> 16 ) & MASK ] << 16 ) ^
                ( (ulong) RS[ ( Y2 >> 24 ) & MASK ] << 24 );

        X2 = *RK++ ^ \
                ( (ulong) RS[ ( Y2       ) & MASK ]       ) ^
                ( (ulong) RS[ ( Y1 >>  8 ) & MASK ] <<  8 ) ^
                ( (ulong) RS[ ( Y0 >> 16 ) & MASK ] << 16 ) ^
                ( (ulong) RS[ ( Y3 >> 24 ) & MASK ] << 24 );

        X3 = *RK++ ^ \
                ( (ulong) RS[ ( Y3       ) & MASK ]       ) ^
                ( (ulong) RS[ ( Y2 >>  8 ) & MASK ] <<  8 ) ^
                ( (ulong) RS[ ( Y1 >> 16 ) & MASK ] << 16 ) ^
                ( (ulong) RS[ ( Y0 >> 24 ) & MASK ] << 24 );
    }
    else /* AES_ENCRYPT */
    {
        for( i = (ctx->nr >> 1) - 1; i > 0; i-- )
        {
            A_F( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );
            A_F( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );
        }

        A_F( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );

        X0 = *RK++ ^ \
                ( (ulong) FS[ ( Y0       ) & MASK ]       ) ^
                ( (ulong) FS[ ( Y1 >>  8 ) & MASK ] <<  8 ) ^
                ( (ulong) FS[ ( Y2 >> 16 ) & MASK ] << 16 ) ^
                ( (ulong) FS[ ( Y3 >> 24 ) & MASK ] << 24 );

        X1 = *RK++ ^ \
                ( (ulong) FS[ ( Y1       ) & MASK ]       ) ^
                ( (ulong) FS[ ( Y2 >>  8 ) & MASK ] <<  8 ) ^
                ( (ulong) FS[ ( Y3 >> 16 ) & MASK ] << 16 ) ^
                ( (ulong) FS[ ( Y0 >> 24 ) & MASK ] << 24 );

        X2 = *RK++ ^ \
                ( (ulong) FS[ ( Y2       ) & MASK ]       ) ^
                ( (ulong) FS[ ( Y3 >>  8 ) & MASK ] <<  8 ) ^
                ( (ulong) FS[ ( Y0 >> 16 ) & MASK ] << 16 ) ^
                ( (ulong) FS[ ( Y1 >> 24 ) & MASK ] << 24 );

        X3 = *RK++ ^ \
                ( (ulong) FS[ ( Y3       ) & MASK ]       ) ^
                ( (ulong) FS[ ( Y0 >>  8 ) & MASK ] <<  8 ) ^
                ( (ulong) FS[ ( Y1 >> 16 ) & MASK ] << 16 ) ^
                ( (ulong) FS[ ( Y2 >> 24 ) & MASK ] << 24 );
    }

    P_U_LE( X0, output,  0 );
    P_U_LE( X1, output,  4 );
    P_U_LE( X2, output,  8 );
    P_U_LE( X3, output, 12 );

    return( 0 );
}

int a_crypt_cbc( a_ctx *ctx,
                    int mode,
                    size_t length,
                    uchar iv[16],
                    const uchar *input,
                    uchar *output )
{
    int i;
    uchar temp[16];

    if( length % 16 )
        return( -2 );

    if( mode == AES_DECRYPT )
    {
        while( length > 0 )
        {
            memcpy( temp, input, 16 );
            a_crypt_ecb( ctx, mode, input, output );

            for( i = 0; i < 16; i++ )
                output[i] = (uchar)( output[i] ^ iv[i] );

            memcpy( iv, temp, 16 );

            input  += 16;
            output += 16;
            length -= 16;
        }
    }
    else
    {
        while( length > 0 )
        {
            for( i = 0; i < 16; i++ )
                output[i] = (uchar)( input[i] ^ iv[i] );

            a_crypt_ecb( ctx, mode, output, output );
            memcpy( iv, output, 16 );

            input  += 16;
            output += 16;
            length -= 16;

        }
    }

    return( 0 );
}

/**************************************************************************
 *  SO FUNCTION - ENCRYPTION
 **************************************************************************/
int aes_so_enc (uchar* ip_buf,  uint32 ip_len, uchar* op_buf, uint32 op_len)
{
    uint32 i = 0;
    uint32 ret = 0;

    if (ip_len != op_len)
    {
        SMSG(true,"[%s] error, ip len should be equal to op len\n",MOD);
        return -1;
    }

    if (0 != ip_len % CIPHER_BLOCK_SIZE)
    {
        SMSG(true,"[%s] error, ip len should be mutiple of %d bytes\n",MOD,CIPHER_BLOCK_SIZE);
        return -1;
    }


    if(0 == g_AES_Key[0])
    {
        SMSG(true,"[%s] Enc Key Is ZERO. Fail\n",MOD);
        goto _err;    
    }

    ret = a_enc(&aes, (uchar*)g_AES_Key, aes_key_len*8);

    if (ret != 0) 
    {
        SMSG(true,"a_enc error -%02X\n", -ret);
        goto _err;
    }    

    for (i = 0; i!=ip_len ; i+=CIPHER_BLOCK_SIZE)
    {
        ret = a_crypt_cbc(&aes, AES_ENCRYPT, CIPHER_BLOCK_SIZE, (uchar*)g_AES_IV_TEMP, ip_buf + i, op_buf + i);
        if (ret != 0)
        {
            SMSG(true,"hairtunes: a_cbc error -%02X\n", -ret);
            goto _err;
        }
    }

    return 0;

_err:

    return -1;
}

/**************************************************************************
 *  SO FUNCTION - DECRYPTION
 **************************************************************************/
int aes_so_dec (uchar* ip_buf,  uint32 ip_len, uchar* op_buf, uint32 op_len)
{
    uint32 i = 0;
    uint32 ret = 0;    

    if (ip_len != op_len)
    {
        SMSG(true,"[%s] error, ip len should be equal to op len\n",MOD);
        return -1;
    }

    if (0 != ip_len % CIPHER_BLOCK_SIZE)
    {
        SMSG(true,"[%s] error, ip len should be mutiple of %d bytes\n",MOD,CIPHER_BLOCK_SIZE);
        return -1;
    }

    if(0 == g_AES_Key[0])
    {
        SMSG(true,"[%s] Dec Key Is ZERO. Fail\n",MOD);
        goto _err;    
    }

    ret = a_dec(&aes, (uchar*)g_AES_Key, aes_key_len*8);
    if (ret != 0) 
    {
        SMSG(true,"a_dec error -%02X\n", -ret);
        goto _err;
    }    

    for (i = 0; i!=ip_len ; i+=CIPHER_BLOCK_SIZE)
    {
        ret = a_crypt_cbc(&aes, AES_DECRYPT, 0x10, (uchar*)g_AES_IV_TEMP, ip_buf + i, op_buf + i);
        if (ret != 0)
        {
            SMSG(true,"hairtunes: a_cbc error -%02X\n", -ret);
            goto _err;
        }
    }
    
    return 0;

_err:

    return -1;

}

/**************************************************************************
 *  SO FUNCTION - KEY INITIALIZATION
 **************************************************************************/
/* WARNING ! this function is not the same as cipher tool */
int aes_so_init_key (uchar* key_buf,  uint32 key_len)
{
    uint32 i = 0;
    uchar temp[CT_AES128_LEN*2];
    uint32 n = 0;
    uint32 val = 0;
    uchar c;
    int j = 0;    
	uchar fmt_str[2] = {0};	


    if(0 == key_buf)
    {
        SMSG(true,"[%s] Init Key Is ZERO. Fail\n",MOD);
        goto _err;    
    }

    /* -------------------------------------------------- */
    /* check key length                                   */
    /* -------------------------------------------------- */
    switch(key_len)
    {
        case CT_AES128_LEN:
            break;        
        case CT_AES192_LEN:
        case CT_AES256_LEN:
            SMSG(true,"[%s] Only AES 128 is supported\n",MOD);
            goto _err;        
        default:
            SMSG(true,"[%s] Len Invalid %d\n",MOD,key_len);
            goto _err;
    }

    aes_key_len = key_len;

    /* -------------------------------------------------- */
    /* copy key to temporarily buffer                     */    
    /* -------------------------------------------------- */
    mcpy(temp,key_buf,CT_AES128_LEN*2);

    /* -------------------------------------------------- */
    /* revert string to accomodate OpenSSL format         */
    /* -------------------------------------------------- */
    for(i=0;i<key_len*2;i+=8)
    {
        c               = temp[i];
        temp[i]         = temp[i+6];
        temp[i+6]       = c;
        c               = temp[i+1];
        temp[i+1]       = temp[i+7];
        temp[i+7]       = c;

        c               = temp[i+2];
        temp[i+2]       = temp[i+4];
        temp[i+4]       = c;
        c               = temp[i+3];
        temp[i+3]       = temp[i+5];
        temp[i+5]       = c;
    }

    /* -------------------------------------------------- */
    /* convert key value from string format to hex format */
    /* -------------------------------------------------- */
  
    i = 0;
    n = 0;
    
    while(n < key_len*2)
    {

        for(j=0; j<8; j++)
        {
            fmt_str[0] = temp[n+j];
            sscanf(fmt_str,"%x",&val);
            g_AES_Key[i] = g_AES_Key[i]*16;
            g_AES_Key[i] += val;             
        }

        /* get next key value */
        i ++;
        n += 8;
    }    

    /* -------------------------------------------------- */
    /* reinit IV                                          */
    /* -------------------------------------------------- */
    for(i=0;i<4;i++)
    {
        g_AES_IV_TEMP[i] = g_AES_IV[i];
    }

    /* dump information for debugging */
    for(i=0; i<1; i++)
    {
        SMSG(true,"0x%x\n",g_AES_Key[i]);
    }

    for(i=0; i<1; i++)
    {
        SMSG(true,"0x%x\n",g_AES_IV_TEMP[i]);
    }

    return 0;

_err:

    return -1;

}

/**************************************************************************
 *  SO FUNCTION - VECTOR INITIALIZATION
 **************************************************************************/
int aes_so_init_vector (void)
{
    uint32 i = 0;
    
    /* -------------------------------------------------- */
    /* reinit IV                                          */
    /* -------------------------------------------------- */
    for(i=0;i<4;i++)
    {
        g_AES_IV_TEMP[i] = g_AES_IV[i];
    }

    /* dump information for debugging */
    for(i=0; i<1; i++)
    {
        SMSG(true,"0x%x\n",g_AES_Key[i]);
    }

    for(i=0; i<1; i++)
    {
        SMSG(true,"0x%x\n",g_AES_IV_TEMP[i]);
    }

    return 0;
}


