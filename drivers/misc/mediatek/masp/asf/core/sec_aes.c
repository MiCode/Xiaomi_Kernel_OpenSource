#include "sec_osal_light.h"
#include "sec_aes.h"
#include "sec_cipher_header.h"
#include "sec_typedef.h"
#include "sec_error.h"
#include "sec_rom_info.h"
#include "sec_secroimg.h"
#include "sec_key_util.h"
/* v1. legacy aes. W1128/32 MP */
#include "aes_legacy.h"
/* v2. so aes. W1150 MP */
#include "aes_so.h"

/**************************************************************************
 *  DEFINITIONS
 **************************************************************************/
#define MOD                         "SEC AES"
#define SML_SCRAMBLE_SEED           "78ABD4569EA41795"

/**************************************************************************
*  DEBUG CONTROL
**************************************************************************/
#define SEC_AES_DEBUG_LOG           (0)
#define SMSG                        printk

/**************************************************************************
 *  LOCAL VARIABLE
 **************************************************************************/
static uchar                        bAesKeyInit = FALSE;
 
/**************************************************************************
 *  EXTERNAL VARIABLE
 **************************************************************************/
extern AND_ROMINFO_T                rom_info;
extern AND_SECROIMG_T               secroimg;

/**************************************************************************
 *  EXTERNAL FUNCTION
 **************************************************************************/

/**************************************************************************
 *  DUMP HEADER
 **************************************************************************/
void sec_dump_img_header (const CIPHER_HEADER *img_header)
{
    SMSG("[%s] header magic_number  = %d\n",MOD,img_header->magic_number);
    SMSG("[%s] header cust_name     = %s\n",MOD,img_header->cust_name);    
    SMSG("[%s] header image_version = %d\n",MOD,img_header->image_version);        
    SMSG("[%s] header image_length  = %d\n",MOD,img_header->image_length);            
    SMSG("[%s] header image_offset  = %d\n",MOD,img_header->image_offset); 
    SMSG("[%s] header cipher_length = %d\n",MOD,img_header->cipher_length);        
    SMSG("[%s] header cipher_offset = %d\n",MOD,img_header->cipher_offset);
}

/**************************************************************************
 *  IMPORT KEY
 **************************************************************************/
int sec_aes_import_key(void)
{
    int ret = SEC_OK;
    uchar key[AES_KEY_SIZE] = {0};    
    AES_VER aes_ver = AES_VER_LEGACY; 
    uint32 key_len = 0;    

    /* avoid re-init aes key
       if re-init key again, key value will be decoded twice ..*/
    if(TRUE == bAesKeyInit)
    {
        SMSG("[%s] reset aes vector\n",MOD);
        /* initialize internal crypto engine */    
        if(SEC_OK != (ret = lib_aes_init_vector (rom_info.m_SEC_CTRL.m_sec_aes_legacy?(AES_VER_LEGACY):(AES_VER_SO))))
        {
            goto _end;
        }
        return ret;
    }

    bAesKeyInit = TRUE;        
    
    if(0 != mcmp(rom_info.m_id,RI_NAME,RI_NAME_LEN))
    {
        SMSG("[%s] error. key not found\n",MOD);
        ret = ERR_AES_KEY_NOT_FOUND;
        goto _end;
    }


    /* -------------------------- */
    /* check aes type             */
    /* -------------------------- */
    if(TRUE == rom_info.m_SEC_CTRL.m_sec_aes_legacy)
    {
        aes_ver = AES_VER_LEGACY;
        key_len = 32;        
    }
    else
    {
        aes_ver = AES_VER_SO;
        key_len = 16;        
    }


    /* -------------------------- */
    /* get sml aes key            */
    /* -------------------------- */    
    if(FALSE == rom_info.m_SEC_CTRL.m_sml_aes_key_ac_en)
    {
        sec_decode_key(     rom_info.m_SEC_KEY.sml_aes_key, 
                            sizeof(rom_info.m_SEC_KEY.sml_aes_key), 
                            rom_info.m_SEC_KEY.crypto_seed, 
                            sizeof(rom_info.m_SEC_KEY.crypto_seed));    
        dump_buf(rom_info.m_SEC_KEY.sml_aes_key,4);
        mcpy(key,rom_info.m_SEC_KEY.sml_aes_key,sizeof(key));    
    }
    else 
    {
        SMSG("\n[%s] AC enabled\n",MOD);  
        dump_buf(secroimg.m_andro.sml_aes_key,4);
        sec_decode_key(     secroimg.m_andro.sml_aes_key, 
                            sizeof(secroimg.m_andro.sml_aes_key), 
                            (uchar*)SML_SCRAMBLE_SEED, 
                            sizeof(SML_SCRAMBLE_SEED));
        dump_buf(secroimg.m_andro.sml_aes_key,4);                            
        mcpy(key,secroimg.m_andro.sml_aes_key,sizeof(key));
    }

    /* initialize internal crypto engine */    
    if(SEC_OK != (ret = lib_aes_init_key (key,key_len,aes_ver)))
    {
        goto _end;
    }
    
_end:

    return ret;
}

/**************************************************************************
 *  IMAGE CIPHER AES INIT
 **************************************************************************/
int sec_aes_init(void)
{
    int ret = SEC_OK;

    /* init key */
    if (SEC_OK != (ret = sec_aes_import_key()))
    {
        goto _exit;
    }    

_exit:

    return ret;
}


/**************************************************************************
 *  AES TEST FUNCTION
 **************************************************************************/
/* Note: this function is only for aes test */
int sec_aes_test (void)
{    
    int ret = SEC_OK;
    uchar buf[CIPHER_BLOCK_SIZE] = "AES_TEST";

    SMSG("\n[%s] SW AES test\n",MOD);    

    /* -------------------------- */
    /* sec aes encrypt test       */
    /* -------------------------- */  
    SMSG("[%s] input      = 0x%x,0x%x,0x%x,0x%x\n",MOD,buf[0],buf[1],buf[2],buf[3]);    
    if(SEC_OK != (ret = lib_aes_enc(buf,CIPHER_BLOCK_SIZE,buf,CIPHER_BLOCK_SIZE)))
    {
        SMSG("[%s] error (0x%x)\n",MOD,ret);
        goto _exit;
    }
    SMSG("[%s] cipher     = 0x%x,0x%x,0x%x,0x%x\n",MOD,buf[0],buf[1],buf[2],buf[3]);

    /* -------------------------- */
    /* sec aes decrypt test       */
    /* -------------------------- */
    if(SEC_OK != (ret = lib_aes_dec(buf,CIPHER_BLOCK_SIZE,buf,CIPHER_BLOCK_SIZE)))
    {
        SMSG("[%s] error (0x%x)\n",MOD,ret);
        goto _exit;
    }
    SMSG("[%s] plain text = 0x%x,0x%x,0x%x,0x%x\n",MOD,buf[0],buf[1],buf[2],buf[3]);

_exit:
    return ret;
}

