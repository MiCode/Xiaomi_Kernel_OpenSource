/*
 * Copyright (C) 2011 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
