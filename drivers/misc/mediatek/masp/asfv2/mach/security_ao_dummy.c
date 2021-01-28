// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 MediaTek Inc.
 */

#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "sec_boot_lib.h"
#include "mt-plat/sync_write.h"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

int masp_hal_set_dm_verity_error(void)
{
	int ret = 0;

	/* do nothing, used when platform security porting is not completed */

	return ret;
}

