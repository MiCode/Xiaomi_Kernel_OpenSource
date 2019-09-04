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

extern void auxadc_set_convert_fn(int channel,
				  void (*convert_fn)(unsigned char convert));
extern void auxadc_set_cali_fn(int channel,
			       int (*cali_fn)(int val, int precision_factor));
extern int auxadc_priv_read_channel(struct device *dev, int channel);
extern unsigned char *auxadc_get_r_ratio(int channel);
extern int pmic_auxadc_chip_init(struct device *dev);
#endif				/* __MT635X_AUXADC_H_ */
