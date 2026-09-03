/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2019 Spreadtrum Communications Inc.
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

#ifndef _SIPA_SYS_PD_H_
#define _SIPA_SYS_PD_H_

#include <linux/sipa.h>

typedef int (*sipa_sys_parse_dts_cb)(void *priv);
typedef void (*sipa_sys_init_cb)(void *priv);
typedef int (*sipa_sys_do_power_on_cb)(void *priv);
typedef int (*sipa_sys_do_power_off_cb)(void *priv);
typedef int (*sipa_sys_clk_enable_cb)(void *priv);

struct sipa_sys_register {
	struct regmap *rmap;
	u32 reg;
	u32 mask;
};

struct sipa_sys_data {
	const u32 version;
};

struct sipa_sys_pd_drv {
	struct device *dev;
	struct generic_pm_domain gpd;
	struct clk *clk_ipa_ckg_eb;
	struct clk *ipa_core_clk;
	struct clk *ipa_core_parent;
	struct clk *ipa_core_default;
	bool pd_on;
	const struct sipa_sys_data *data;
	sipa_sys_parse_dts_cb parse_dts_cb;
	sipa_sys_init_cb init_cb;
	sipa_sys_do_power_on_cb do_power_on_cb;
	sipa_sys_do_power_off_cb do_power_off_cb;
	sipa_sys_clk_enable_cb clk_enable_cb;
	void *cb_priv;
	struct regmap *dispc1_reg;
	struct regmap *anlg_reg;
	struct sipa_sys_register regs[0];
};

#endif
