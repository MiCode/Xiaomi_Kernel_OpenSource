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

