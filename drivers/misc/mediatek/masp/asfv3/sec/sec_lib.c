// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 MediaTek Inc.
 */

#include "sec_boot_lib.h"
#include "sec_mod.h"

#define MOD                         "MASP"

int masp_hal_sbc_enabled(void)
{
	// LCOV_EXCL_START
	return g_hw_sbcen;
	// LCOV_EXCL_STOP
}

int masp_hal_get_sbc_checksum(unsigned int *pChecksum)
{
	// LCOV_EXCL_START
	int i;

	for (i = 0; i < NUM_SBC_PUBK_HASH; i++)
		*pChecksum += g_sbc_pubk_hash[i];

	return 0;
	// LCOV_EXCL_STOP
}
