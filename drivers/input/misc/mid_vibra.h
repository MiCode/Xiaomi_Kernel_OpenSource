/*
 *  mid_vibra.h - Intel vibrator header
 *
 *  Copyright (C) 2013 Intel Corp
 *  Author: B, Jayachandran <jayachandran.b@intel.com>
 *  Author: Omair Md Abdullah <omair.m.abdullah@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef __MID_VIBRA_H
#define __MID_VIBRA_H

#include <linux/gpio.h>

struct vibra_info {
	int     enabled;
	struct mutex	lock;
	struct device	*dev;
	void __iomem	*shim;
	const char	*name;
	unsigned long *base_unit;
	unsigned long *duty_cycle;
	unsigned int max_base_unit;
	u8  max_duty_cycle;
	int gpio_en;
	int gpio_pwm;
	int alt_fn;
	int ext_drv;
	bool use_gpio_en; /* Whether vibra uses gpio based enable control */

	void (*enable)(struct vibra_info *info);
	void (*disable)(struct vibra_info *info);
	int (*pwm_configure)(struct vibra_info *info, bool enable);

	const struct attribute_group *vibra_attr_group;
};

struct vibra_info *mid_vibra_setup(struct device *dev,
			struct mid_vibra_pdata *data);
void *mid_vibra_acpi_get_drvdata(const char *hid);
int intel_mid_plat_vibra_probe(struct platform_device *pdev);
int intel_mid_plat_vibra_remove(struct platform_device *pdev);

extern struct mid_vibra_pdata pmic_vibra_data_cht;

#define vibra_gpio_set_value(info, v) \
{if ((info)->use_gpio_en) \
gpio_set_value((info)->gpio_en, (v)); }

#define vibra_gpio_set_value_cansleep(info, v) \
{if ((info)->use_gpio_en) \
gpio_set_value_cansleep((info)->gpio_en, (v)); }

#define vibra_gpio_free(info) \
{if ((info)->use_gpio_en) \
gpio_free((info)->gpio_en); }

#endif /* __MID_VIBRA_H */
