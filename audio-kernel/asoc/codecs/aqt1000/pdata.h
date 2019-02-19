/*
 * Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _AQT1000_PDATA_H_
#define _AQT1000_PDATA_H_

#include <linux/kernel.h>
#include <linux/device.h>
#include "../msm-cdc-supply.h"

struct aqt1000_micbias_setting {
	u8 ldoh_v;
	u32 cfilt1_mv;
	u32 micb1_mv;
	u8 bias1_cfilt_sel;
};

struct aqt1000_pdata {
	unsigned int irq_gpio;
	unsigned int irq_flags;
	struct cdc_regulator *regulator;
	int num_supplies;
	struct aqt1000_micbias_setting micbias;
	struct device_node *aqt_rst_np;
	u32 mclk_rate;
	u32 ext_clk_rate;
	u32 ext_1p8v_supply;
};

#endif /* _AQT1000_PDATA_H_ */
