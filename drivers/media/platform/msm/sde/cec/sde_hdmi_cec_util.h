/* Copyright (c) 2012, 2015-2017, The Linux Foundation. All rights reserved.
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

#ifndef __SDE_HDMI_CEC_UTIL_H__
#define __SDE_HDMI_CEC_UTIL_H__

#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/types.h>

#ifdef DEBUG
#define CEC_REG_WRITE(hw, off, val) \
	sde_hdmi_cec_reg_w(&(hw)->io_res, (off), (val), true)
#define CEC_REG_READ(hw, off) \
	sde_hdmi_cec_reg_r(&(hw)->io_res, (off), true)
#else
#define CEC_REG_WRITE(hw, off, val) \
	sde_hdmi_cec_reg_w(&(hw)->io_res, (off), (val), false)
#define CEC_REG_READ(hw, off) \
	sde_hdmi_cec_reg_r(&(hw)->io_res, (off), false)
#endif

struct cec_io_data {
	u32 len;
	void __iomem *base;
};

enum cec_vreg_type {
	CEC_REG_LDO,
	CEC_REG_VS,
};

struct cec_vreg {
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

struct cec_clk {
	struct clk *clk; /* clk handle */
	char clk_name[32];
};

struct cec_pin_res {
	struct pinctrl *pinctrl;
	struct pinctrl_state *state_active;
	struct pinctrl_state *state_sleep;
};

struct cec_hw_resource {
	/* power */
	unsigned num_vreg;
	struct cec_vreg *vreg_config;
	unsigned num_clk;
	struct cec_clk *clk_config;
	struct cec_pin_res pin_res;

	/* io */
	struct cec_io_data io_res;
};

void sde_hdmi_cec_reg_w(struct cec_io_data *io,
	u32 offset, u32 value, bool debug);
u32 sde_hdmi_cec_reg_r(struct cec_io_data *io, u32 offset, bool debug);
void sde_hdmi_cec_reg_dump(void __iomem *base, u32 length, const char *prefix,
	bool debug);

int sde_hdmi_cec_init_resource(struct platform_device *pdev,
	struct cec_hw_resource *hw);
void sde_hdmi_cec_deinit_resource(struct platform_device *pdev,
	struct cec_hw_resource *hw);
int sde_hdmi_cec_enable_power(struct cec_hw_resource *hw, bool enable);

#endif /* __SDE_HDMI_CEC_UTIL_H__ */

