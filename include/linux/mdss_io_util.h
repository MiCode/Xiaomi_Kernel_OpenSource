/* Copyright (c) 2012, 2017-2018, The Linux Foundation. All rights reserved.
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
#include <linux/i2c.h>
#include <linux/types.h>

#ifdef DEBUG
#define DEV_DBG(fmt, args...)   pr_err(fmt, ##args)
#else
#define DEV_DBG(fmt, args...)   pr_debug(fmt, ##args)
#endif
#define DEV_INFO(fmt, args...)  pr_info(fmt, ##args)
#define DEV_WARN(fmt, args...)  pr_warn(fmt, ##args)
#define DEV_ERR(fmt, args...)   pr_err(fmt, ##args)

struct mdss_io_data {
	u32 len;
	void __iomem *base;
};

void mdss_reg_w(struct mdss_io_data *io, u32 offset, u32 value, u32 debug);
u32 mdss_reg_r(struct mdss_io_data *io, u32 offset, u32 debug);
void mdss_reg_dump(void __iomem *base, u32 len, const char *prefix, u32 debug);

#define DSS_REG_W_ND(io, offset, val)  mdss_reg_w(io, offset, val, false)
#define DSS_REG_W(io, offset, val)     mdss_reg_w(io, offset, val, true)
#define DSS_REG_R_ND(io, offset)       mdss_reg_r(io, offset, false)
#define DSS_REG_R(io, offset)          mdss_reg_r(io, offset, true)

enum mdss_vreg_type {
	DSS_REG_LDO,
	DSS_REG_VS,
};

enum mdss_vreg_mode {
	DSS_REG_MODE_ENABLE,
	DSS_REG_MODE_DISABLE,
	DSS_REG_MODE_LP,
	DSS_REG_MODE_ULP,
	DSS_REG_MODE_MAX,
};

struct mdss_vreg {
	struct regulator *vreg; /* vreg handle */
	char vreg_name[32];
	int min_voltage;
	int max_voltage;
	u32 load[DSS_REG_MODE_MAX];
	int pre_on_sleep;
	int post_on_sleep;
	int pre_off_sleep;
	int post_off_sleep;
};

struct mdss_gpio {
	unsigned int gpio;
	unsigned int value;
	char gpio_name[32];
};

enum mdss_clk_type {
	DSS_CLK_AHB, /* no set rate. rate controlled through rpm */
	DSS_CLK_PCLK,
	DSS_CLK_OTHER,
};

struct mdss_clk {
	struct clk *clk; /* clk handle */
	char clk_name[32];
	enum mdss_clk_type type;
	unsigned long rate;
};

struct mdss_module_power {
	unsigned int num_vreg;
	struct mdss_vreg *vreg_config;
	unsigned int num_gpio;
	struct mdss_gpio *gpio_config;
	unsigned int num_clk;
	struct mdss_clk *clk_config;
};

int msm_mdss_ioremap_byname(struct platform_device *pdev,
	struct mdss_io_data *io_data, const char *name);
void msm_mdss_iounmap(struct mdss_io_data *io_data);

int msm_mdss_enable_gpio(struct mdss_gpio *in_gpio, int num_gpio, int enable);
int msm_mdss_gpio_enable(struct mdss_gpio *in_gpio, int num_gpio, int enable);

int msm_mdss_config_vreg(struct device *dev, struct mdss_vreg *in_vreg,
	int num_vreg, int config);
int msm_mdss_enable_vreg(struct mdss_vreg *in_vreg, int num_vreg, int enable);
int msm_mdss_config_vreg_opt_mode(struct mdss_vreg *in_vreg, int num_vreg,
	 enum mdss_vreg_mode mode);

int msm_mdss_get_clk(struct device *dev, struct mdss_clk *clk_arry,
		     int num_clk);
void msm_mdss_put_clk(struct mdss_clk *clk_arry, int num_clk);
int msm_mdss_clk_set_rate(struct mdss_clk *clk_arry, int num_clk);
int msm_mdss_enable_clk(struct mdss_clk *clk_arry, int num_clk, int enable);

int mdss_i2c_byte_read(struct i2c_client *client, uint8_t slave_addr,
		       uint8_t reg_offset, uint8_t *read_buf);
int mdss_i2c_byte_write(struct i2c_client *client, uint8_t slave_addr,
			uint8_t reg_offset, uint8_t *value);
int mdss_prim_panel_fb_unblank(int timeout);

#endif /* __MDSS_IO_UTIL_H__ */
