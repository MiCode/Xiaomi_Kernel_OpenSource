/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MHL_MSM_H__
#define __MHL_MSM_H__

#include <linux/types.h>
#include <linux/platform_device.h>
#include <mach/board.h>

#include "mhl_devcap.h"
#include "mhl_defs.h"

#define MHL_DEVICE_NAME "sii8334"
#define MHL_DRIVER_NAME "sii8334"

#define HPD_UP               1
#define HPD_DOWN             0

struct mhl_msm_state_t {
	struct i2c_client *i2c_client;
	struct i2c_driver *i2c_driver;
	uint8_t      cur_state;
	uint8_t chip_rev_id;
	struct msm_mhl_platform_data *mhl_data;
};

enum {
	TX_PAGE_TPI          = 0x00,
	TX_PAGE_L0           = 0x01,
	TX_PAGE_L1           = 0x02,
	TX_PAGE_2            = 0x03,
	TX_PAGE_3            = 0x04,
	TX_PAGE_CBUS         = 0x05,
	TX_PAGE_DDC_EDID     = 0x06,
	TX_PAGE_DDC_SEGM     = 0x07,
};

enum mhl_st_type {
	POWER_STATE_D0_NO_MHL = 0,
	POWER_STATE_D0_MHL    = 2,
	POWER_STATE_D3        = 3,
};

enum {
	DEV_PAGE_TPI_0      = (0x72),
	DEV_PAGE_TX_L0_0    = (0x72),
	DEV_PAGE_TPI_1      = (0x76),
	DEV_PAGE_TX_L0_1    = (0x76),
	DEV_PAGE_TX_L1_0    = (0x7A),
	DEV_PAGE_TX_L1_1    = (0x7E),
	DEV_PAGE_TX_2_0     = (0x92),
	DEV_PAGE_TX_2_1     = (0x96),
	DEV_PAGE_TX_3_0	    = (0x9A),
	DEV_PAGE_TX_3_1	    = (0x9E),
	DEV_PAGE_CBUS       = (0xC8),
	DEV_PAGE_DDC_EDID   = (0xA0),
	DEV_PAGE_DDC_SEGM   = (0x60),
};

#endif /* __MHL_MSM_H__ */
