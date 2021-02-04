/*
 * Copyright (C) 2018 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MT635X_AUXADC_H_
#define __MT635X_AUXADC_H_

#include <linux/device.h>
#include <linux/iio/iio.h>

#include <dt-bindings/iio/mt635x-auxadc.h>

#include <mt-plat/upmu_common.h>

struct auxadc_regs {
	enum PMU_FLAGS_LIST ch_rqst;
	enum PMU_FLAGS_LIST ch_rdy;
	enum PMU_FLAGS_LIST ch_out;
};

extern unsigned short pmic_set_hk_reg_value(enum PMU_FLAGS_LIST flagname,
					    unsigned int val);

extern void auxadc_set_regs(int channel, struct auxadc_regs *regs);
extern void auxadc_set_convert_fn(int channel,
				  void (*convert_fn)(unsigned char convert));
extern void auxadc_set_cali_fn(int channel,
			       int (*cali_fn)(int val, int precision_factor));
extern int auxadc_priv_read_channel(int channel);
extern unsigned char *auxadc_get_r_ratio(int channel);

extern void pmic_auxadc_chip_timeout_handler(struct device *dev,
					     bool is_timeout,
					     unsigned char ch_num);
extern int pmic_auxadc_chip_init(struct device *dev);

#endif				/* __MT635X_AUXADC_H_ */
