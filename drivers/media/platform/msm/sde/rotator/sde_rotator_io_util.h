/* Copyright (c) 2012, 2015-2016, The Linux Foundation. All rights reserved.
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


#ifndef __SDE_ROTATOR_IO_UTIL_H__
#define __SDE_ROTATOR_IO_UTIL_H__

#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <linux/types.h>

#ifdef DEBUG
#define DEV_DBG(fmt, args...)   pr_err("<SDEROT_ERR> " fmt, ##args)
#else
#define DEV_DBG(fmt, args...)   pr_debug("<SDEROT_DBG> " fmt, ##args)
#endif
#define DEV_INFO(fmt, args...)  pr_info("<SDEROT_INFO> " fmt, ##args)
#define DEV_WARN(fmt, args...)  pr_warn("<SDEROT_WARN> " fmt, ##args)
#define DEV_ERR(fmt, args...)   pr_err("<SDEROT_ERR> " fmt, ##args)

struct sde_io_data {
	u32 len;
	void __iomem *base;
};

void sde_reg_w(struct sde_io_data *io, u32 offset, u32 value, u32 debug);
u32 sde_reg_r(struct sde_io_data *io, u32 offset, u32 debug);
void sde_reg_dump(void __iomem *base, u32 len, const char *prefix, u32 debug);

#define SDE_REG_W_ND(io, offset, val)  sde_reg_w(io, offset, val, false)
#define SDE_REG_W(io, offset, val)     sde_reg_w(io, offset, val, true)
#define SDE_REG_R_ND(io, offset)       sde_reg_r(io, offset, false)
#define SDE_REG_R(io, offset)          sde_reg_r(io, offset, true)

enum sde_vreg_type {
	SDE_REG_LDO,
	SDE_REG_VS,
};

struct sde_vreg {
	struct regulator *vreg; /* vreg handle */
	char vreg_name[32];
	int min_voltage;
	int max_voltage;
	int enable_load;
	int disable_load;
	int pre_on_sleep;
	int post_on_sleep;
	int pre_off_sleep;
	int post_off_sleep;
};

struct sde_gpio {
	unsigned int gpio;
	unsigned int value;
	char gpio_name[32];
};

enum sde_clk_type {
	SDE_CLK_AHB, /* no set rate. rate controlled through rpm */
	SDE_CLK_PCLK,
	SDE_CLK_OTHER,
};

struct sde_clk {
	struct clk *clk; /* clk handle */
	char clk_name[32];
	enum sde_clk_type type;
	unsigned long rate;
};

struct sde_module_power {
	unsigned int num_vreg;
	struct sde_vreg *vreg_config;
	unsigned int num_gpio;
	struct sde_gpio *gpio_config;
	unsigned int num_clk;
	struct sde_clk *clk_config;
};

int sde_rot_ioremap_byname(struct platform_device *pdev,
	struct sde_io_data *io_data, const char *name);
void sde_rot_iounmap(struct sde_io_data *io_data);

int sde_rot_config_vreg(struct device *dev, struct sde_vreg *in_vreg,
	int num_vreg, int config);
int sde_rot_enable_vreg(struct sde_vreg *in_vreg, int num_vreg,	int enable);

int sde_rot_get_clk(struct device *dev, struct sde_clk *clk_arry, int num_clk);
void sde_rot_put_clk(struct sde_clk *clk_arry, int num_clk);
int sde_rot_clk_set_rate(struct sde_clk *clk_arry, int num_clk);
int sde_rot_enable_clk(struct sde_clk *clk_arry, int num_clk, int enable);

#endif /* __SDE_ROTATOR_IO_UTIL_H__ */
