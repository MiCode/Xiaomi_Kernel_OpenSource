// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 MediaTek Inc.
 */

/******************************************************************************
 * CHIP SELECTION
 ******************************************************************************/
/*#include <mach/mt_typedefs.h>*/
#include "hacc_mach.h"
/******************************************************************************
 * REGISTER
 ******************************************************************************/
#include "sec_boot_lib.h"
#include "sec_mod.h"


/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/
int masp_hal_sbc_enabled(void)
{
	return g_hw_sbcen;
}

int masp_hal_get_sbc_checksum(unsigned int *pChecksum)
{
	int i;

	for (i = 0; i < NUM_SBC_PUBK_HASH; i++)
		*pChecksum += g_sbc_pubk_hash[i];

	return 0;
}
