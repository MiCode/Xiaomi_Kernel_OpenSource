#include "sec_osal_light.h"
#include "sec_typedef.h"
#include "rsa_def.h"
#include "alg_sha1.h"
#include "bgn_export.h"
#include "sec_cust_struct.h"
#include "sec_auth.h"
#include "sec_error.h"
#include "sec_rom_info.h"
#include "sec_boot_lib.h"
#include "sec_sign_header.h"
#include "sec_key_util.h"
#include "sec_log.h"

/**************************************************************************
 *  MODULE NAME
 **************************************************************************/
#define MOD                         "AUTHEN"

/************************************************************************** 
*  LOCAL VARIABLE 
**************************************************************************/
uchar                               bRsaKeyInit = false;
uchar                               bRsaImgKeyInit = false;

/**************************************************************************
 *  EXTERNAL VARIABLE
 **************************************************************************/
extern uchar                        sha1sum[];
extern rsa_ctx                      rsa;
extern uchar                        rsa_ci[];
extern AND_ROMINFO_T                rom_info;
CUST_SEC_INTER                      g_cus_inter;

/**************************************************************************
 *  RSA SML KEY INIT
 **************************************************************************/
int lib_init_key (uchar *nKey, uint32 nKey_len, uchar *eKey, uint32 eKey_len)
{
    int ret = SEC_OK;

    /* ------------------------------ */
    /* avoid re-init aes key
       if re-init key again, key value will be decoded twice ..*/
    /* ------------------------------ */       
    if(true == bRsaKeyInit)
    {
        return ret;
    }

    bRsaKeyInit = true;        

    if(0 != mcmp(rom_info.m_id,RI_NAME,RI_NAME_LEN))
    {
        SMSG(true,"[%s] error. key not found\n",MOD);
        ret = ERR_RSA_KEY_NOT_FOUND;
        goto _end;
    }

    /* ------------------------------ */
    /* clean rsa variable             */
    /* ------------------------------ */
    memset( &rsa, 0, sizeof(rsa_ctx));

    /* ------------------------------ */
    /* init RSA module / exponent key */
    /* ------------------------------ */    
    rsa.len = RSA_KEY_SIZE;

    /* ------------------------------ */
    /* decode key                     */
    /* ------------------------------ */    
    sec_decode_key( nKey, nKey_len, 
                    rom_info.m_SEC_KEY.crypto_seed, 
                    sizeof(rom_info.m_SEC_KEY.crypto_seed));

    /* ------------------------------ */
    /* init mpi library               */
    /* ------------------------------ */    
    bgn_read_str( &rsa.N , 16, (char*)nKey, nKey_len );    
    bgn_read_str( &rsa.E , 16, (char*)eKey, eKey_len );        

    /* ------------------------------ */
    /* debugging                      */
    /* ------------------------------ */
    dump_buf(nKey,0x4);   

_end:

    return ret;

}

/**************************************************************************
 *  SIGNING
 **************************************************************************/
int lib_sign(uchar* data_buf,  uint32 data_len, uchar* sig_buf, uint32 sig_len)
{

#if 0

    int i = 0;

    if (RSA_KEY_LEN != sig_len)
    {   
        SMSG(true,"signature length is wrong (%d)\n",sig_len);
        goto _err;
    }

    /* hash the plain text */
    sha1(data_buf, data_len, sha1sum );       
    
    /* 
        2011.09.27. Add OpenSSL compatibility support 

        OpenSSL's command : 
        openssl rsautl -sign -in xxxxx -inkey xxx.pem -out signature
    
        RSA padding type : SIG_RSA_SHA1 
            original implementation for W1126 and W1132 MP release 
            This padding rule can't be compatible with OpenSSL
            The cipher results are not the same 
            
        RSA padding type : SIG_RSA_RAW
            This padding rule can be compatible with OpenSSL
            The cipher results are the same 

    */
    /* encrypt the hash value (sign) */ 
    SMSG(true,"[%s] RSA padding : RAW \n",MOD);
    if( rsa_sign( &rsa, HASH_LEN, sha1sum, sig_buf ) != 0 )
    {
        SMSG(true, "failed\n" );
        goto _err;
    }   
    SMSG(true,"[%s] sign image ... pass\n\n",MOD);


    /* output signature */
    SMSG(true,"[%s] output signature: \n",MOD);	
    SMSG(true," ------------------------------------\n");	
    for(i=0;i<RSA_KEY_LEN;i++)
    {
        if(i==RSA_KEY_LEN-1)
        {
            if(sig_buf[i]<0x10)
            {   
                SMSG(true,"0x0%x",sig_buf[i]);
            }
            else
            {   
                SMSG(true,"0x%x",sig_buf[i]);
            }
        }
        else
        {
            if(sig_buf[i]<0x10)
            {   
                SMSG(true,"0x0%x,",sig_buf[i]);
            }
            else
            {   
                SMSG(true,"0x%x,",sig_buf[i]);
            }
        }
    }
    SMSG(true,"\n");

    /* self testing : verify this signature */
    SMSG(true,"\n[%s] verify signature",MOD);
    if( rsa_verify( &rsa, HASH_LEN, sha1sum, sig_buf ) != 0 )
    {
        SMSG(true, "failed\n" );
        goto _err;
    }    
    SMSG(true,"... pass\n");

#endif

    return 0;    

#if 0

_err:

    return -1;
    
#endif    

}


/**************************************************************************
 *  HASHING
 **************************************************************************/
int lib_hash (uchar* data_buf,  uint32 data_len, uchar* hash_buf, uint32 hash_len)
{

    if (HASH_LEN != hash_len)
    {   
        SMSG(true,"hash length is wrong (%d)\n",hash_len);
        goto _err;
    }

    /* hash the plain text */
    sha1(data_buf, data_len, hash_buf );

    return 0;    

_err:

    return -1;

}


/**************************************************************************
 *  VERIFY SIGNATURE
 **************************************************************************/
int lib_verify (uchar* data_buf,  uint32 data_len, uchar* sig_buf, uint32 sig_len)
{

    if (RSA_KEY_LEN != sig_len)
    {   
        SMSG(true,"signature length is wrong (%d)\n",sig_len);
        goto _err;
    }

    SMSG(true,"[%s] 0x%x,0x%x,0x%x,0x%x\n",MOD,data_buf[0],data_buf[1],data_buf[2],data_buf[3]);	

    /* hash the plain text */
    sha1(data_buf, data_len, sha1sum);

    /* verify this signature */
    SMSG(true,"[%s] verify signature",MOD);
    if( rsa_verify( &rsa, HASH_LEN, sha1sum, sig_buf ) != 0 )
    {
        SMSG(true, " ... failed\n" );
        goto _err;
    }    
    SMSG(true," ... pass\n");

    return 0;

_err:

    return -1;
}

