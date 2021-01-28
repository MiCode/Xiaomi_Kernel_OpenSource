/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

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
#include <linux/types.h>
#include "sec_error.h"
#include "sec_osal.h"
#include "sec_mod.h"

/**************************************************************************
 * [S-BOOT]
 **************************************************************************/
/* S-BOOT Attribute */
#define ATTR_SBOOT_DISABLE                  0x00
#define ATTR_SBOOT_ENABLE                   0x11
#define ATTR_SBOOT_ONLY_ENABLE_ON_SCHIP     0x22

/**************************************************************************
 * [S-USBDL]
 **************************************************************************/
/* S-USBDL Attribute */
#define ATTR_SUSBDL_DISABLE                 0x00
#define ATTR_SUSBDL_ENABLE                  0x11
#define ATTR_SUSBDL_ONLY_ENABLE_ON_SCHIP    0x22




/**************************************************************************
 *  EXTERNAL VARIABLE
 **************************************************************************/
/*extern AND_ROMINFO_T rom_info;*/
/*extern SECURE_INFO sec_info;*/
/*extern SECCFG_U seccfg;*/
/*extern AND_SECROIMG_T secroimg;*/

extern unsigned int g_rom_info_sbc_attr;
extern unsigned int g_rom_info_sdl_attr;
extern unsigned int g_hw_sbcen;
extern unsigned int g_lock_state;
extern unsigned int g_random_id[NUM_RID];
extern unsigned char g_crypto_seed[NUM_CRYPTO_SEED];
extern unsigned int g_sbc_pubk_hash[NUM_SBC_PUBK_HASH];
extern unsigned int lks;

/**************************************************************************
 * EXPORT FUNCTION
 **************************************************************************/
extern int masp_boot_init(void);
extern int sec_boot_enabled(void);
extern int sec_usbdl_enabled(void);
extern int sec_modem_auth_enabled(void);
extern int sec_schip_enabled(void);
extern int sec_get_random_id(unsigned int *rid);

/* HACC HW init */
extern unsigned int sec_boot_hacc_init(void);


#endif				/* SEC_BOOT_LIB_H */
