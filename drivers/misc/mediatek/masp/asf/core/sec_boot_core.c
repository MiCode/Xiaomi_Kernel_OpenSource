/******************************************************************************
 *  INCLUDE LIBRARY
 ******************************************************************************/
#include "sec_boot_lib.h"
#include <mach/sec_osal.h>  
#include <mach/mt_sec_hal.h>

/**************************************************************************
 *  DEFINITIONS
 **************************************************************************/
#define MOD                         "ASF"

/**************************************************************************
 *  LOCAL VARIABLE
 **************************************************************************/

/**************************************************************************
 *  GLOBAL VARIABLE
 **************************************************************************/
AND_ROMINFO_T                       rom_info;
SECURE_INFO                         sec_info;
SECCFG_U                            seccfg;

bool bMsg                           = FALSE;
bool bSECROInit                     = FALSE;


/******************************************************************************
 * CHECK IF SECURITY CHIP IS ENABLED
******************************************************************************/
int sec_schip_enabled (void)
{
    if(TRUE == masp_hal_sbc_enabled())
    {
        SMSG(true,"SC\n");
        return 1;
    }
    else
    {    
        SMSG(true,"NSC\n");    
        return 0;
    }

    return 0;
}


/******************************************************************************
 * CHECK IF SDS (SML DEFAULT SETTING BINARY) IS ENABLED
******************************************************************************/
/* not used */
int sec_sds_enabled (void)
{
    switch(rom_info.m_SEC_CTRL.m_sec_sds_en)
    {
        case 0:
            SMSG(bMsg,"[%s] SDS is disabled\n",MOD);
            return 1;
            
        case 1:
            SMSG(bMsg,"[%s] SDS is enabled\n",MOD);
            return 1;

       default:
            SMSG(true,"[%s] invalid SDS config (0x%x)\n",MOD,rom_info.m_SEC_CTRL.m_sec_sds_en);
            SEC_ASSERT(0);
    }

    return 0;
}


/******************************************************************************
 * CHECK IF SECURE BOOT IS NEEDED
******************************************************************************/
int sec_boot_enabled (void)
{
    switch(rom_info.m_SEC_CTRL.m_sec_boot)
    {
        case ATTR_SBOOT_ENABLE:
            SMSG(bMsg,"[%s] SBOOT is enabled\n",MOD);
            SMSG(bMsg,"0x%x, SB-FORCE\n",ATTR_SBOOT_ENABLE);            
            return 1;

        /* secure boot can't be disabled on security chip */
        case ATTR_SBOOT_DISABLE:
        case ATTR_SBOOT_ONLY_ENABLE_ON_SCHIP:
            SMSG(bMsg,"[%s] SBOOT is only enabled on S-CHIP\n",MOD);            
            if(TRUE == masp_hal_sbc_enabled())
            {
                SMSG(true,"0x%x, SB-SC\n",ATTR_SBOOT_ONLY_ENABLE_ON_SCHIP);
                return 1;
            }
            else
            {
                SMSG(true,"0x%x, SB-NSC\n",ATTR_SBOOT_ONLY_ENABLE_ON_SCHIP);
                return 0;
            }
       default:
            SMSG(true,"[%s] invalid sboot config (SB-0x%x)\n",MOD,rom_info.m_SEC_CTRL.m_sec_boot);
            SEC_ASSERT(0);
    }

    return 0;

}

/******************************************************************************
 * CHECK IF MODEM AUTH IS NEEDED
******************************************************************************/
int sec_modem_auth_enabled (void)
{

    switch(rom_info.m_SEC_CTRL.m_sec_modem_auth)
    {
        case 0:
            SMSG(bMsg,"[%s] MODEM AUTH is disabled\n",MOD);
            return 0;
            
        case 1:
            SMSG(bMsg,"[%s] MODEM AUTH is enabled\n",MOD);        
            return 1;
       default:
            SMSG(true,"[%s] invalid modem auth config (0x%x)\n",MOD,rom_info.m_SEC_CTRL.m_sec_modem_auth);
            SEC_ASSERT(0);
    }

    return 0;
}  

/**************************************************************************
 *  SECURE BOOT
 **************************************************************************/
int sec_boot_key_init (void)
{
    int ret = SEC_OK;

    if(TRUE == sec_info.bKeyInitDis)
    {
        SMSG(true,"[%s] key init disabled\n",MOD); 
        goto _end;
    }

    /* ------------------------------ */
    /* init aes                       */
    /* ------------------------------ */    
    if(SEC_OK != (ret = sec_aes_init()))
    {
        goto _end;        
    }

    /* ------------------------------ */
    /* init rsa                       */ 
    /* ------------------------------ */    
    if(SEC_OK != (ret = sec_init_key( rom_info.m_SEC_KEY.sml_auth_rsa_n,
                                      sizeof(rom_info.m_SEC_KEY.sml_auth_rsa_n),
                                      rom_info.m_SEC_KEY.sml_auth_rsa_e,
                                      sizeof(rom_info.m_SEC_KEY.sml_auth_rsa_e))))
    {
        goto _end;        
    }

_end:
    return ret;
}

/**************************************************************************
 *  SECURE BOOT INIT HACC
 **************************************************************************/
uint32 sec_boot_hacc_init (void)
{
    uint32 ret = SEC_OK;    
    
    /* ----------------------------------- */
    /* check if secure boot is enabled     */
    /* ----------------------------------- */   
    if(0 != mcmp(rom_info.m_id,RI_NAME,RI_NAME_LEN))
    {
        ret = ERR_ROM_INFO_MTD_NOT_FOUND;
        goto _end;
    }

    /* ----------------------------------- */
    /* lnit hacc key                        */
    /* ----------------------------------- */    
    if(SEC_OK != (ret = masp_hal_sp_hacc_init (rom_info.m_SEC_KEY.crypto_seed,sizeof(rom_info.m_SEC_KEY.crypto_seed))))
    {
        goto _end;
    }        

_end:

    return ret;    
}

/**************************************************************************
 *  SECURE BOOT CHECK PART ENABLE CHECK
 **************************************************************************/
bool sec_boot_check_part_enabled (char* part_name)
{
    bool bCheckEn = false;
    uint32 i = 0;
    uint32 chk_num = 0;
    
    AND_SECBOOT_CHECK_PART_T *chk_part = NULL;
    SMSG(bMsg,"[%s] find part_name '%s'\n",MOD,part_name);

    chk_part = &rom_info.m_SEC_BOOT_CHECK_PART;
    chk_num = sizeof(AND_SECBOOT_CHECK_PART_T)/sizeof(chk_part->name[0]);        

    for(i=0; i<chk_num; i++)
    {   
        SMSG(bMsg,"[%s] chk_part->name[%d] = %s\n",MOD,i,chk_part->name[i]);
        if(0 == mcmp(part_name, chk_part->name[i], strlen(part_name)))
        {
            bCheckEn = true;
            break;
        }        
    }        

    return bCheckEn;    
}

/**************************************************************************
 *  SECURE BOOT INIT
 **************************************************************************/ 
int masp_boot_init (void)
{
    int ret = SEC_OK;

    SMSG(true,"[%s] '%s%s'\n",MOD,BUILD_TIME,BUILD_BRANCH);    

#if !defined(CONFIG_MTK_GPT_SCHEME_SUPPORT)        
    /* ----------------------------------- */
    /* check usif status                   */
    /* ----------------------------------- */
    if(SEC_OK != (ret = sec_usif_check()))
    {
        goto _error;
    }
#endif 
    /* ----------------------------------- */
    /* scan partition map                  */
    /* ----------------------------------- */
    sec_dev_find_parts();

    /* ----------------------------------- */
    /* read rom info                       */
    /* ----------------------------------- */
    /* read rom info to get security config. */
    if(SEC_OK != (ret = sec_dev_read_rom_info()))
    {
        goto _error;
    }

    SMSG(true,"[%s] ROM INFO is found\n",MOD);     

    if(0 != mcmp(rom_info.m_id,RI_NAME,RI_NAME_LEN))
    {
        SMSG(true,"[%s] error. ROM INFO not found\n",MOD);
        ret = ERR_ROM_INFO_MTD_NOT_FOUND;
        goto _error;
    }    

    /* ----------------------------------- */
    /* read secro                          */
    /* ----------------------------------- */
    if(TRUE == rom_info.m_SEC_CTRL.m_secro_ac_en)
    {
        if(FALSE == bSECROInit)
        {    
#if 0        
            if(TRUE == sec_secro_ac())
            {
                if(SEC_OK != (ret = sec_dev_read_secroimg()))
                {
                    goto _error;
                }
            }
            else
            {
                /* only for non-security platform */
                if(SEC_OK != (ret = sec_fs_read_secroimg(FS_SECRO_PATH,(uchar*)&secroimg)))
                {
                    goto _error;
                }                
            }
#else
            if(SEC_OK != (ret = sec_dev_read_secroimg()))
            {
                goto _error;
            }

#endif
            bSECROInit = TRUE;
        }
    }

    /* ----------------------------------- */
    /* init key                            */
    /* ----------------------------------- */
    /* TODO : add support to read SML DEC key from SEC_RO */
    if(SEC_OK != (ret = sec_boot_key_init()))
    {
        goto _error;        
    }

    return ret;

_error:

    SMSG(true,"[%s] error (0x%x)\n",MOD,ret);   

    return ret;
}
