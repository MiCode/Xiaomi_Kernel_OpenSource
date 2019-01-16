#ifndef SEC_BOOT_LIB_H
#define SEC_BOOT_LIB_H

/**************************************************************************
 *  INCLUDE LINUX HEADER
 **************************************************************************/
#include "sec_osal_light.h"

/**************************************************************************
 *  INCLUDE MTK HEADERS
 **************************************************************************/
#include "masp_version.h"
#include "sec_aes.h"
#include "aes_legacy.h"
#include "sec_sign_header.h"
#include "sec_cfg.h"
#include "sec_typedef.h"
#include "sec_rom_info.h"
#include "sec_secroimg.h"
#include "sec_mtd.h"
#include "sec_usbdl.h"
#include "sec_mtd_util.h"
#include "sec_boot.h"
#include "sec_fsutil_inter.h"
#include "sec_log.h"
#include "sec_wrapper.h"
#include "sec_boot_core.h"
#include "sec_hdr.h"
#include "sec_cfg_ver.h"
#include "sec_usif.h"
#include "sec_usif_util.h"
#include "sec_dev.h"
#include "sec_dev_util.h"
#include "sec_sign_extension.h"
#include "sec_signfmt_util.h"
#include "sec_signfmt_v2.h"
#include "sec_signfmt_v3.h"
#include "sec_signfmt_v4.h"
#include "sec_signfmt_core.h"
#include "sec_cipherfmt_core.h"
#include "sec_cipher_header.h"
#include "sec_error.h"
#include "sec_nvram.h"

/******************************************************************************
 *  INTERNAL CONFIGURATION
 ******************************************************************************/
typedef struct
{
    bool bKeyInitDis;
    bool bUsifEn;  
    char bMsg;  
} SECURE_INFO;

/**************************************************************************
 *  EXTERNAL VARIABLE
 **************************************************************************/
extern AND_ROMINFO_T                rom_info;
extern SECURE_INFO                  sec_info;
extern SECCFG_U                     seccfg;
extern AND_SECROIMG_T               secroimg;

/**************************************************************************
 *  sec boot util function
 **************************************************************************/
extern char* asf_get_build_info(void);

#endif /* SEC_BOOT_LIB_H */

