//#include "sec_osal_light.h"
#include "sec_signfmt_util.h"

/**************************************************************************
  *  GLOBAL VARIABLES
  **************************************************************************/ 
unsigned int sec_crypto_hash_size[] =
{
    CRYPTO_SIZE_UNKNOWN,
    SEC_SIZE_HASH_MD5,
    SEC_SIZE_HASH_SHA1,
    SEC_SIZE_HASH_SHA256,
    SEC_SIZE_HASH_SHA512
};
 
unsigned int sec_crypto_sig_size[] =
{
    CRYPTO_SIZE_UNKNOWN,
    SEC_SIZE_SIG_RSA512,
    SEC_SIZE_SIG_RSA1024,
    SEC_SIZE_SIG_RSA2048
};

/**************************************************************************
 *  UTILITY FUNCTIONS
 **************************************************************************/
unsigned int get_hash_size(SEC_CRYPTO_HASH_TYPE hash)
{
    return sec_crypto_hash_size[hash];
}
 
unsigned int get_signature_size(SEC_CRYPTO_SIGNATURE_TYPE sig)
{
    return sec_crypto_sig_size[sig];
}

unsigned char is_signfmt_v1(SEC_IMG_HEADER *hdr)
{
    if( 0 == hdr->signature_length )
    {        
        return true;
    }
    
    return false;
}

unsigned char is_signfmt_v2(SEC_IMG_HEADER *hdr)
{
    if( 0 == hdr->signature_length )
    {        
        return false;
    }
    else if( SEC_EXTENSION_MAGIC == hdr->sign_offset )
    {
        return false;
    }

    return true;
}

unsigned char is_signfmt_v3(SEC_IMG_HEADER *hdr)
{

    if( SEC_EXTENSION_MAGIC == hdr->sign_offset )
    {
        return true;
    }
    
    return false;
}


unsigned char is_signfmt_v4(SEC_IMG_HEADER *hdr)
{
    SEC_IMG_HEADER_V4 *v4_hdr = (SEC_IMG_HEADER_V4 *)hdr;

    if( SEC_EXTENSION_MAGIC_V4 == v4_hdr->ext_magic)
    {
        return true;
    }
    
    return false;
}

