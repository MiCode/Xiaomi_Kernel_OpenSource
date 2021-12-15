/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/
#ifndef __MT635X_AUXADC_H_
#define __MT635X_AUXADC_H_
#include <linux/device.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/mt635x-auxadc.h>

extern void auxadc_set_convert_fn(unsigned int channel,
				  void (*convert_fn)(unsigned char convert));
extern void auxadc_set_cali_fn(unsigned int channel,
			       int (*cali_fn)(int val, int precision_factor));
extern int auxadc_priv_read_channel(struct device *dev, int channel);
extern unsigned char *auxadc_get_r_ratio(int channel);
extern int pmic_auxadc_chip_init(struct device *dev);
#endif				/* __MT635X_AUXADC_H_ */
