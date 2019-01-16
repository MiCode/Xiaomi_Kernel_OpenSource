#ifndef SEC_ROMINFO_H
#define SEC_ROMINFO_H

#include "sec_boot.h"
#include "sec_key.h"
#include "sec_ctrl.h"


/**************************************************************************
 *  INIT VARIABLES
 **************************************************************************/ 
#define RI_NAME                             "AND_ROMINFO_v"
#define RI_NAME_LEN                         13
/* VER1 - only a ROM INFO region is provided */
/* VER2 - ANTI-CLONE feature is supported */
#define ROM_INFO_VER                        0x2
#define ROM_INFO_SEC_RO_EXIST               0x1
#define ROM_INFO_ANTI_CLONE_OFFSET          0x54
#define ROM_INFO_ANTI_CLONE_LENGTH          0xE0
#define ROM_INFO_DEFAULT_SEC_CFG_OFFSET     0x360000
#define ROM_INFO_SEC_CFG_LENGTH             0x20000

/**************************************************************************
 *  ANDRIOD ROM INFO FORMAT
 **************************************************************************/ 
/* this structure should always sync with FlashLib 
   becuase FlashLib will search storage to find ROM_INFO */
#define AND_ROM_INFO_SIZE                  (960)   
typedef struct {

    unsigned char                   m_id[16];           /* MTK */
    unsigned int                    m_rom_info_ver;     /* MTK */
    unsigned char                   m_platform_id[16];  /* CUSTOMER */
    unsigned char                   m_project_id[16];
    
    unsigned int                    m_sec_ro_exist;     /* MTK */
    unsigned int                    m_sec_ro_offset;    /* MTK */
    unsigned int                    m_sec_ro_length;    /* MTK */
    
    unsigned int                    m_ac_offset;        /* MTK : 
                                                            no use */

    unsigned int                    m_ac_length;        /* MTK : 
                                                            no use */
                                                            
    unsigned int                    m_sec_cfg_offset;   /* MTK :
                                                            part info. from 
                                                            parititon table.

                                                            tool will refer to
                                                            this setting to 
                                                            find SEC CFG */

    unsigned int                    m_sec_cfg_length;   /* MTK :
                                                            part info. from 
                                                            parititon table.
                                                            
                                                            tool will refer to
                                                            this setting to 
                                                            find SEC CFG */ 

    unsigned char                   m_reserve1[128];    
    
    AND_SECCTRL_T                   m_SEC_CTRL;         /* CUSTOMER :
                                                            secure feature 
                                                            control */

    unsigned char                   m_reserve2[18];

                                                        /* CUSTOMER :
                                                            secure boot check 
                                                            partition */
    AND_SECBOOT_CHECK_PART_T        m_SEC_BOOT_CHECK_PART; 

    AND_SECKEY_T                    m_SEC_KEY;          /* CUSTOMER :
                                                            key */

} AND_ROMINFO_T;

#endif /* SEC_ROMINFO_H */

