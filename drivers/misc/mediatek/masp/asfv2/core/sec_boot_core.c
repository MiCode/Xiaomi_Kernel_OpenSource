// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

/******************************************************************************
 *  INCLUDE LIBRARY
 ******************************************************************************/
#include "sec_boot_lib.h"
#include "sec_osal.h"
#include "sec_hal.h"

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
unsigned int g_rom_info_sbc_attr;
unsigned int g_rom_info_sdl_attr;
unsigned int g_hw_sbcen;
unsigned int g_lock_state;
unsigned int g_random_id[NUM_RID];
unsigned char g_crypto_seed[NUM_CRYPTO_SEED];
unsigned int g_sbc_pubk_hash[NUM_SBC_PUBK_HASH];


int sec_get_random_id(unsigned int *rid)
{
	int ret = 0;

	if (rid == NULL) {
		SEC_ASSERT(0);
		ret = ERR_SBOOT_CHECK_NULL_POINTER;
	}
	if (rid)
		memcpy(rid, g_random_id, 16);
	return ret;
}


/******************************************************************************
 * CHECK IF SECURITY CHIP IS ENABLED
 ******************************************************************************/
int sec_schip_enabled(void)
{
	if (true == masp_hal_sbc_enabled()) {
		pr_debug("SC\n");
		return 1;
	}

	pr_debug("NSC\n");

	return 0;
}


/******************************************************************************
 * CHECK IF SECURE USBDL IS ENABLED
 ******************************************************************************/
int sec_usbdl_enabled(void)
{
	switch (g_rom_info_sdl_attr) {
	case ATTR_SUSBDL_ENABLE:
		pr_debug("[%s] SUSBDL is enabled\n", MOD);
		pr_debug("0x%x, SD-FORCE\n", ATTR_SUSBDL_ENABLE);
		return 1;

	/* SUSBDL can't be disabled on security chip */
	case ATTR_SUSBDL_DISABLE:
	case ATTR_SUSBDL_ONLY_ENABLE_ON_SCHIP:
		pr_debug("[%s] SUSBDL is only enabled on S-CHIP\n", MOD);
		if (true == masp_hal_sbc_enabled()) {
			pr_debug("0x%x, SD-SC\n",
				 ATTR_SUSBDL_ONLY_ENABLE_ON_SCHIP);
			return 1;
		}
		pr_debug("0x%x, SD-NSC\n", ATTR_SUSBDL_ONLY_ENABLE_ON_SCHIP);
		return 0;

	default:
		pr_debug("[%s] invalid susbdl config (SD-0x%x)\n",
			 MOD, g_rom_info_sdl_attr);
		SEC_ASSERT(0);
		return 1;
	}
}

/******************************************************************************
 * CHECK IF SECURE BOOT IS NEEDED
 ******************************************************************************/
int sec_boot_enabled(void)
{
	switch (g_rom_info_sbc_attr) {
	case ATTR_SBOOT_ENABLE:
		pr_debug("[%s] SBOOT is enabled\n", MOD);
		pr_debug("0x%x, SB-FORCE\n", ATTR_SBOOT_ENABLE);
		return 1;

	/* secure boot can't be disabled on security chip */
	case ATTR_SBOOT_DISABLE:
	case ATTR_SBOOT_ONLY_ENABLE_ON_SCHIP:
		pr_debug("[%s] SBOOT is only enabled on S-CHIP\n", MOD);
		if (true == masp_hal_sbc_enabled()) {
			pr_debug("0x%x, SB-SC\n",
				 ATTR_SBOOT_ONLY_ENABLE_ON_SCHIP);
			return 1;
		}

		pr_debug("0x%x, SB-NSC\n", ATTR_SBOOT_ONLY_ENABLE_ON_SCHIP);
		return 0;

	default:
		pr_debug("[%s] invalid sboot config (SB-0x%x)\n",
			 MOD, g_rom_info_sbc_attr);
		SEC_ASSERT(0);
	}

	return 0;

}

/**************************************************************************
 *  SECURE BOOT INIT HACC
 **************************************************************************/
unsigned int sec_boot_hacc_init(void)
{
	unsigned int ret = SEC_OK;

	/* ----------------------------------- */
	/* lnit hacc key                        */
	/* ----------------------------------- */
	ret = masp_hal_sp_hacc_init(g_crypto_seed, sizeof(g_crypto_seed));
	if (ret != SEC_OK)
		goto end;

end:
	return ret;
}


/**************************************************************************
 *  SECURE BOOT INIT
 **************************************************************************/
int masp_boot_init(void)
{
	int ret = SEC_OK;

	pr_debug("[%s] error (0x%x)\n", MOD, ret);

	return ret;
}
