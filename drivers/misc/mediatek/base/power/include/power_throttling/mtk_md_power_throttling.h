/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef _MTK_MD_POWER_THROTTLING_H_
#define _MTK_MD_POWER_THROTTLING_H_

enum POWER_THROTTLE_NOTIFY_TYPE {
	PT_LOW_BATTERY_VOLTAGE,
	PT_BATTERY_PERCENT,
	PT_OVER_CURRENT,
};

#define TMC_CTRL_CMD_TX_POWER	10
#define LBAT_REDUCE_TX_POWER	6 /* unit : 1 db */
#define OC_REDUCE_TX_POWER	6 /* unit : 1 db */

#endif
