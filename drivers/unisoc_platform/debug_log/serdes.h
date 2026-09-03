/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __SERDES_R4PX_H__
#define __SERDES_R4PX_H__

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/init.h>

#define CH_MAX 16

struct serdes_drv_data {
	void __iomem *base;
	u32 version;
	u32 cut_off;
	u8 channel;
	u8 la_sample_rate;
	u8 enabled;
	u8 dc_blnc_fix;
	u8 ov_flow;
	u8 ov_flow_last;
	u8 ch_num;
	u32 ch_map[CH_MAX];
	const char *ch_str[CH_MAX];
};

int serdes_enable(struct serdes_drv_data *serdes, int enable);

#endif
