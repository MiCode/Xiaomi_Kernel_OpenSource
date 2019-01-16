#include <mach/sec_osal.h> 
#include <mach/mt_sec_hal.h>
#include <mach/mt_sec_export.h>
#include "sec_typedef.h"
#include "sec_rom_info.h"
#include "sec_usbdl.h"
#include "sec_secroimg.h"
#include "sec_error.h"
#include "sec_log.h"
#include "alg_sha1.h"
#include "sec_dev.h"

/******************************************************************************
 *  DEFINITIONS
 ******************************************************************************/
#define MOD                         "ASF"


/******************************************************************************
 *  GLOBAL VARIABLES
 ******************************************************************************/
AND_SECROIMG_T                      secroimg;
AND_SECROIMG_V5a_T                  secroimg_v5;

uint32                              secro_v3_off = MAX_SECRO_V3_OFFSET;
bool                                bSecroExist = FALSE;
bool                                bSecroIntergiy = FALSE;
bool                                bSecroV5Exist = FALSE;
bool                                bSecroV5Intergiy = FALSE;


              
/******************************************************************************
 *  EXTERNAL VARIABLES
 ******************************************************************************/
extern AND_ROMINFO_T                rom_info;
extern uchar                        sha1sum[];


/******************************************************************************
 * VALIDATE SECRO
 ******************************************************************************/
uint32 sec_secro_check (void)
{
    uint32 ret = SEC_OK;

    /* ------------------------ */       
    /* check header             */
    /* ------------------------ */                
    if(AC_ANDRO_MAGIC != secroimg.m_andro.magic_number)
    {
        ret = ERR_SECROIMG_HACC_AP_DECRYPT_FAIL;
        goto _end;
    }

    if(AC_MD_MAGIC != secroimg.m_md.magic_number)
    {
        ret = ERR_SECROIMG_HACC_MD_DECRYPT_FAIL;                    
        goto _end;
    }

    if(AC_MD2_MAGIC != secroimg.m_md2.magic_number)
    {
        ret = ERR_SECROIMG_HACC_MD_DECRYPT_FAIL;                    
        goto _end;
    }

    /* ------------------------ */       
    /* check integrity          */
    /* ------------------------ */
    if(FALSE == bSecroIntergiy)
    {
        sha1((uchar*)&secroimg, sizeof(AND_SECROIMG_T) - sizeof(secroimg.hash) 
            - sizeof(AND_SECROIMG_PADDING_T), sha1sum);
#if 0
        SMSG(TRUE,"[%s] hash value :\n",MOD);                        
        dump_buf(sha1sum,secroimg.m_header.hash_length);
        SMSG(TRUE,"[%s] correct :\n",MOD);                        
        dump_buf(secroimg.hash,secroimg.m_header.hash_length);                             
#endif                    
        if(0 != mcmp(secroimg.hash, sha1sum, secroimg.m_header.hash_length))
        {
            SMSG(TRUE,"[%s] SECRO hash check fail\n",MOD);
            ret = ERR_SECROIMG_HASH_CHECK_FAIL;                    
            goto _end;                    
        }                

        bSecroIntergiy = TRUE;
        SMSG(TRUE,"[%s] SECRO hash check pass\n",MOD);
    }

_end:

    return ret;
}

/******************************************************************************
 * VALIDATE SECRO V5
 ******************************************************************************/
uint32 sec_secro_v5_check (void)
{
    uint32 ret = SEC_OK;

    /* ------------------------ */       
    /* check header             */
    /* ------------------------ */                
    if(AC_MD_INFO_MAGIC != secroimg_v5.m_md_info_v5a.magic_number)
    {
        ret = ERR_SECROIMG_HACC_MD_DECRYPT_FAIL;                    
        goto _end;
    }

    if(AC_SV5_MAGIC_MD_V5a != secroimg_v5.m_md_sro_v5a.magic_number)
    {
        ret = ERR_SECROIMG_HACC_MD_DECRYPT_FAIL;                    
        goto _end;
    }

    /* ------------------------ */       
    /* check integrity          */
    /* ------------------------ */
    if(FALSE == bSecroV5Intergiy)
    {
        sha1((uchar*)&secroimg_v5, sizeof(AND_AC_HEADER_V5a_T) + sizeof(AND_AC_MD_INFO_V5a_T) 
            + sizeof(AND_AC_MD_V5a_T), sha1sum);
#if 0            
        SMSG(TRUE,"[%s] hash value :\n",MOD);                        
        dump_buf(sha1sum,secroimg_v5.m_header_v5a.hash_len);
        SMSG(TRUE,"[%s] correct :\n",MOD);                        
        dump_buf(secroimg_v5.hash_v5a,secroimg_v5.m_header_v5a.hash_len);                             
#endif                    
        if(0 != mcmp(secroimg_v5.hash_v5a, sha1sum, secroimg_v5.m_header_v5a.hash_len))
        {
            SMSG(TRUE,"[%s] SECRO V5 hash check fail\n",MOD);
            ret = ERR_SECROIMG_V5_HASH_CHECK_FAIL;                    
            goto _end;                    
        }                

        bSecroV5Intergiy = TRUE;
        SMSG(TRUE,"[%s] SECRO V5 hash check pass\n",MOD);
    }

_end:

    return ret;
}


/******************************************************************************
 * CHECK IF SECROIMG IS USED
 ******************************************************************************/
unsigned char masp_secro_en (void)
{
    /* return ProjectConfig's setting */
    if(TRUE == rom_info.m_SEC_CTRL.m_secro_ac_en)
    {    
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/******************************************************************************
 * CHECK IF SECROIMG AC IS ENABLED
 ******************************************************************************/
bool sec_secro_ac (void)
{
    /* PLEASE NOTE THAT !!!!!!!!!!!!!!!!!!
       SECRO AC is only effected when SUSBDL is on */
    if(TRUE == sec_usbdl_enabled())
    {    
        return TRUE;
    }
    /* If security chip, secroimage must be encrypted */
    else if(TRUE == masp_hal_sbc_enabled())
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/******************************************************************************
 * RETURN SECROIMG BLK SIZE
 ******************************************************************************/
uint32 masp_secro_blk_sz (void)
{
    return masp_hal_sp_hacc_blk_sz();
}


/******************************************************************************
 * RETURN SECROIMG MD LENGTH
 ******************************************************************************/
uint32 masp_secro_md_len (uchar *md_info)
{
    uint32 index = 0;
    AND_AC_MD_INFO_V3a_T* cur_md_info = NULL;
    uint32 md_info_len = 0; 

    SMSG(FALSE,"[%s]md_info:%s \n",MOD,md_info);
    
    if(TRUE == bSecroExist)
    {
        /* check if this secro supports v5 format(world phone) , in v3 format 
        this area should be zero.*/
        if (FALSE == secroimg.m_header.world_phone_support)
        {
            /* read v3 format and return, it depends on first character */
            if ('1' == md_info[0])
        {
                return secroimg.m_header.md_length;
            }
            else if ('2' == md_info[0])
            {
                return secroimg.m_header.md2_length;
            }
        }
        /* if it supports v5 format (world phone) */
        else if (TRUE == secroimg.m_header.world_phone_support)
        {
            if (NULL != md_info)
            {
                md_info_len = strlen(md_info);
            }
            
            /* check if this image's information exist */
            for(index=0; index<MAX_V5_SUPPORT_MD_NUM; index++)
            {   
                cur_md_info = &(secroimg.m_padding.md_v3a_info[index]);

                if(0 == strncmp(md_info,cur_md_info->md_name+strlen("SECURE_RO_"),md_info_len))
                {   
                    SMSG(TRUE,"[%s]md[%d]len:0x%x \n",MOD,index,cur_md_info->md_len);
                    return cur_md_info->md_len;
                }
            }

            if (MAX_V5_SUPPORT_MD_NUM == index)
            {
                /* no match found, return 0 */
                SMSG(TRUE,"[%s]v5 no match \n",MOD);
                return 0;
        }
    }
    }
    else
    {
        SMSG(TRUE,"[%s]Secro v3 does not exist \n",MOD);
        return 0;
    }

    return 0;
}

/******************************************************************************
 * RETURN SECROIMG MD PLAINTEXT DATA
 ******************************************************************************/
uint32 masp_secro_md_get_data (uchar *md_info, uchar* buf, uint32 offset, uint32 len)
{
    uint32 ret = SEC_OK;
    uint32 cipher_len = sizeof(AND_AC_ANDRO_T) + sizeof(AND_AC_MD_T) + sizeof(AND_AC_MD2_T);   
    AND_AC_MD_INFO_V3a_T* cur_md_info = NULL;
    uint32 md_info_len = 0; 
    uint32 index = 0;

    osal_secro_lock();

    /* ----------------- */
    /* check             */
    /* ----------------- */

    if (NULL == md_info)
    {
        ret = ERR_SECROIMG_EMPTY_MD_INFO_STR;
        goto _exit;
    }
    else
    {
        md_info_len = strlen(md_info);    
    }

    if(FALSE == bSecroExist)
    {
        ret = ERR_SECROIMG_IS_EMPTY;
        goto _exit;
    }

    if(len == 0)
    {
        ret = ERR_SECROIMG_INVALID_BUF_LEN;
        goto _exit;
    }

    if (0 != (len % masp_hal_sp_hacc_blk_sz())) 
    {   
        ret = ERR_HACC_DATA_UNALIGNED;
        goto _exit;
    }    

    /* check if it only supports secro v3 format */
    if (0 == secroimg.m_header.world_phone_support)
    {

        SMSG(TRUE,"[%s]sro v3  \n",MOD);
    /* ------------------------ */       
    /* decrypt secroimg         */
    /* ------------------------ */
    if(TRUE == sec_secro_ac())
    {
        masp_hal_sp_hacc_dec((uchar*)&secroimg.m_andro, cipher_len, TRUE,HACC_USER1,TRUE);
    }

    /* ------------------------ */       
    /* check header             */
    /* ------------------------ */                
    if(AC_ANDRO_MAGIC != secroimg.m_andro.magic_number)
    {
        ret = ERR_SECROIMG_HACC_AP_DECRYPT_FAIL;
        goto _exit;
    }

    if(AC_MD_MAGIC != secroimg.m_md.magic_number)
    {
        ret = ERR_SECROIMG_HACC_MD_DECRYPT_FAIL;                    
        goto _exit;
    }
  
    if(AC_MD2_MAGIC != secroimg.m_md2.magic_number)
    {
        ret = ERR_SECROIMG_HACC_MD_DECRYPT_FAIL;                    
        goto _exit;
    }
    
    /* ------------------------ */       
    /* fill buffer              */
    /* ------------------------ */    
    /* only copy the data with user specified length */
        
        /* check if this image's information exist */
        if ('1' == md_info[0])
    {
            mcpy(buf,secroimg.m_md.reserve+offset,len);
        }
        else if ('2' == md_info[0])
        {
            mcpy(buf,secroimg.m_md2.reserve+offset,len);
        }
        else
        {
            SMSG(TRUE,"[%s] MD user not supported!\n",MOD);            
    }
    
    /* ------------------------ */       
    /* encrypt secro image      */
    /* ------------------------ */ 
    if(TRUE == sec_secro_ac())
    {    
        masp_hal_sp_hacc_enc((uchar*)&secroimg.m_andro, cipher_len, TRUE,HACC_USER1,TRUE);
    }
    }
    else 
    {

        SMSG(TRUE,"[%s]sro v5  \n",MOD);
        /* ----------------------------- */       
        /* if it supports v5 format      */
        /* ----------------------------- */

        /* check if this image's information exist */
        for(index=0; index<MAX_V5_SUPPORT_MD_NUM; index++)
        {   
            cur_md_info = &(secroimg.m_padding.md_v3a_info[index]);
            if(0 == strncmp(md_info,cur_md_info->md_name+strlen("SECURE_RO_"),md_info_len))
            {   
            break;
    }
        }

        /* md info dees not exist */
        if (MAX_V5_SUPPORT_MD_NUM == index)
        {
            ret = ERR_SECROIMG_MD_INFO_NOT_EXIST;
            goto _exit;
        }
    
    /* ------------------------ */       
        /* read secro v5 from flash */
        /* ------------------------ */       
        bSecroV5Exist = FALSE;
        bSecroV5Intergiy = FALSE;
        if(SEC_OK != (ret = sec_dev_read_secroimg_v5(index)))
        {
            goto _exit;
        }
        else
        {
            /* ------------------------ */       
            /* decrypt secroimg         */
    /* ------------------------ */ 
            cipher_len = sizeof(AND_AC_MD_INFO_V5a_T) + sizeof(AND_AC_MD_V5a_T);
    if(TRUE == sec_secro_ac())
    {    
        masp_hal_sp_hacc_dec((uchar*)&secroimg_v5.m_md_info_v5a, cipher_len, TRUE,HACC_USER1,TRUE);
    }

            /* ------------------------ */       
            /* check header             */
            /* ------------------------ */                
            if(AC_MD_INFO_MAGIC != secroimg_v5.m_md_info_v5a.magic_number)
            {
                ret = ERR_SECROIMG_HACC_MD_DECRYPT_FAIL;                    
                goto _exit;
            }

            if(AC_SV5_MAGIC_MD_V5a != secroimg_v5.m_md_sro_v5a.magic_number)
            {
                ret = ERR_SECROIMG_HACC_MD_DECRYPT_FAIL;                    
                goto _exit;
            }

            /* ------------------------ */       
            /* fill buffer              */
            /* ------------------------ */    
            /* only copy the data with user specified length */
            mcpy(buf,secroimg_v5.m_md_sro_v5a.reserve+offset,len);

            /* no need to encrypt since next time, we'll read it again from flash*/
        }
    }
    

_exit:

    osal_secro_unlock();

    return ret;
}
