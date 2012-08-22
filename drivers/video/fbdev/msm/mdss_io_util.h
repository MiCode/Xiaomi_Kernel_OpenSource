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
 */

#ifndef __MDSS_IO_UTIL_H__
#define __MDSS_IO_UTIL_H__

#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

struct dss_io_data {
	u32 len;
	void __iomem *base;
};

enum dss_vreg_type {
	DSS_REG_LDO,
	DSS_REG_VS,
};

struct dss_vreg {
	struct regulator *vreg; /* vreg handle */
	char vreg_name[32];
	enum dss_vreg_type type;
	int min_voltage;
	int max_voltage;
	int optimum_voltage;
};

struct dss_gpio {
	unsigned gpio;
	char gpio_name[32];
};

struct dss_module_power {
	unsigned num_vreg;
	struct dss_vreg *vreg_config;
	unsigned num_gpio;
	struct dss_gpio *gpio_config;
};

int msm_dss_ioremap_byname(struct platform_device *pdev,
	struct dss_io_data *io_data, const char *name);
int msm_dss_enable_gpio(struct dss_gpio *in_gpio, int num_gpio, int enable);
int msm_dss_gpio_enable(struct dss_gpio *in_gpio, int num_gpio, int enable);
int msm_dss_config_vreg(struct device *dev, struct dss_vreg *in_vreg,
	int num_vreg, int config);
int msm_dss_enable_vreg(struct dss_vreg *in_vreg, int num_vreg,	int enable);

#endif /* __MDSS_IO_UTIL_H__ */
