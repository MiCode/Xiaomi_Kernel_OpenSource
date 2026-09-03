// SPDX-License-Identifier: GPL-2.0-only
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

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/sipa.h>

#include "sipa_sys_phy_v1.h"
#include "../sipa_sys_pd.h"

#define SPRD_IPA_POWERON_POLL_US 50
#define SPRD_IPA_POWERON_TIMEOUT 5000

static const char * const reg_name_tb_v1[] = {
	"ipa-sys-forcewakeup",
	"ipa-sys-forceshutdown",
	"ipa-sys-autoshutdown",
	"ipa-sys-forcedslp",
	"ipa-sys-dslpen",
	"ipa-sys-forcelslp",
	"ipa-sys-lslpen",
	"ipa-sys-smartlslpen",
	"ipa-sys-ipaeb",
	"ipa-sys-cm4eb",
	"ipa-sys-autogaten",
	"ipa-sys-s5-autogaten",
	"ipa-sys-pwr-state",
};

enum sipa_sys_reg_v1 {
	IPA_SYS_FORCEWAKEUP,
	IPA_SYS_FORCESHUTDOWN,
	IPA_SYS_AUTOSHUTDOWN,
	IPA_SYS_FORCEDSLP,
	IPA_SYS_DSLPEN,
	IPA_SYS_FORCELSLP,
	IPA_SYS_LSLPEN,
	IPA_SYS_SMARTSLPEN,
	IPA_SYS_IPAEB,
	IPA_SYS_CM4EB,
	IPA_SYS_AUTOGATEN,
	IPA_SYS_S5_AUTOGATEN,
	IPA_SYS_PWR_STATE,
};

static int sipa_sys_wait_power_on(struct sipa_sys_pd_drv *drv,
				  struct sipa_sys_register *reg_info)
{
	int ret = 0;
	u32 val;

	if (reg_info->rmap) {
		dev_info(drv->dev, "sipa start poll ipa sys power status\n");
		ret = regmap_read_poll_timeout(reg_info->rmap,
					       reg_info->reg,
					       val,
					       !(val & reg_info->mask),
					       SPRD_IPA_POWERON_POLL_US,
					       SPRD_IPA_POWERON_TIMEOUT);
	} else {
		usleep_range((SPRD_IPA_POWERON_TIMEOUT >> 2) + 1, 5000);
	}

	if (ret)
		dev_err(drv->dev,
			"sipa polling check power on reg timed out: %x\n", val);

	return ret;
}

int sipa_sys_do_power_on_cb_v1(void *priv)
{
	int ret = 0;
	struct sipa_sys_register *reg_info;
	struct sipa_sys_pd_drv *drv = (struct sipa_sys_pd_drv *)priv;

	if (!drv)
		return -ENODEV;

	dev_info(drv->dev, "sipa do power on\n");

	reg_info = &drv->regs[IPA_SYS_FORCEWAKEUP];
	if (reg_info->rmap) {
		ret = regmap_update_bits(reg_info->rmap,
					 reg_info->reg,
					 reg_info->mask,
					 reg_info->mask);
		if (ret < 0)
			dev_warn(drv->dev, "set forcewakeup bits fail\n");
	}

	/* access IPA_SYS_CM4EB register need wait ipa_sys power on */
	reg_info = &drv->regs[IPA_SYS_PWR_STATE];
	ret = sipa_sys_wait_power_on(drv, reg_info);
	if (ret)
		dev_warn(drv->dev, "wait pwr on timeout\n");

	/* disable ipa_cm4 eb bit, for asic initail value fault */
	reg_info = &drv->regs[IPA_SYS_CM4EB];
	if (reg_info->rmap) {
		ret = regmap_update_bits(reg_info->rmap,
					 reg_info->reg,
					 reg_info->mask,
					 ~reg_info->mask);
		if (ret < 0)
			dev_warn(drv->dev, "update cm4eb fail\n");
	}

	/* set ipa core clock */
	if (drv->ipa_core_clk && drv->ipa_core_parent)
		clk_set_parent(drv->ipa_core_clk, drv->ipa_core_parent);

	return ret;
}

int sipa_sys_do_power_off_cb_v1(void *priv)
{
	int ret = 0;
	struct sipa_sys_register *reg_info;
	struct sipa_sys_pd_drv *drv = (struct sipa_sys_pd_drv *)priv;

	if (!drv)
		return -ENODEV;

	dev_info(drv->dev, "sipa do power off\n");

	/* set ipa core clock to default */
	if (drv->ipa_core_clk && drv->ipa_core_default)
		clk_set_parent(drv->ipa_core_clk, drv->ipa_core_default);

	reg_info = &drv->regs[IPA_SYS_FORCEWAKEUP];
	if (reg_info->rmap) {
		ret = regmap_update_bits(reg_info->rmap,
					 reg_info->reg,
					 reg_info->mask,
					 ~reg_info->mask);
		if (ret < 0)
			dev_warn(drv->dev, "clear forcewakeup bits fail\n");
	}

	return ret;
}

static int sipa_sys_set_register(struct sipa_sys_pd_drv *drv,
				 struct sipa_sys_register *reg_info,
				 bool set)
{
	int ret = 0;
	u32 val = set ? reg_info->mask : (~reg_info->mask);

	if (reg_info->rmap) {
		ret = regmap_update_bits(reg_info->rmap,
					 reg_info->reg,
					 reg_info->mask,
					 val);
		if (ret < 0)
			dev_warn(drv->dev, "set register bits fail\n");
	}
	return ret;
}

void sipa_sys_init_cb_v1(void *priv)
{
	struct sipa_sys_pd_drv *drv = (struct sipa_sys_pd_drv *)priv;

	if (!drv)
		return;

	/* step1 clear force shutdown:0x32280538[25] */
	sipa_sys_set_register(drv, &drv->regs[IPA_SYS_FORCESHUTDOWN], false);
	/* set auto shutdown enable:0x32280538[24] */
	sipa_sys_set_register(drv, &drv->regs[IPA_SYS_AUTOSHUTDOWN], true);

	/* step2 clear ipa force deep sleep:0x32280544[6] */
	sipa_sys_set_register(drv, &drv->regs[IPA_SYS_FORCEDSLP], false);
	/* set ipa deep sleep enable:0x3228022c[0] */
	sipa_sys_set_register(drv, &drv->regs[IPA_SYS_DSLPEN], true);

	/* step3 clear ipa force light sleep:0x32280548[6] */
	sipa_sys_set_register(drv, &drv->regs[IPA_SYS_FORCELSLP], false);
	/* set ipa light sleep enable:0x32280230[6] */
	sipa_sys_set_register(drv, &drv->regs[IPA_SYS_LSLPEN], true);
	/* set ipa smart light sleep enable:0x32280230[7] */
	sipa_sys_set_register(drv, &drv->regs[IPA_SYS_SMARTSLPEN], true);
}

int sipa_sys_parse_dts_cb_v1(void *priv)
{
	int i;
	u32 reg_info[2];
	const char *reg_name;
	struct regmap *rmap;
	struct device_node *np;
	struct sipa_sys_pd_drv *drv = (struct sipa_sys_pd_drv *)priv;

	if (!drv)
		return -ENODEV;

	np = drv->dev->of_node;
	/* read regmap info */
	for (i = 0; i < ARRAY_SIZE(reg_name_tb_v1); i++) {
		reg_name = reg_name_tb_v1[i];
		rmap = syscon_regmap_lookup_by_phandle_args(np, reg_name,
							    2, reg_info);
		if (IS_ERR(rmap)) {
			dev_warn(drv->dev, "Parse drs %s args fail\n",
				 reg_name);
			continue;
		}
		drv->regs[i].rmap = rmap;
		drv->regs[i].reg = reg_info[0];
		drv->regs[i].mask = reg_info[1];
		dev_dbg(drv->dev, "dts %p, 0x%x, 0x%x\n",
			drv->regs[i].rmap,
			drv->regs[i].reg,
			drv->regs[i].mask);
	}

	return 0;
}

int sipa_sys_clk_enable_cb_v1(void *priv)
{
	return 0;
}
