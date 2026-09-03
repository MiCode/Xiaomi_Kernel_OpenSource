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

#include "sipa_sys_phy_v3.h"
#include "../sipa_sys_pd.h"

#define SPRD_IPA_POWERON_POLL_US 50
#define SPRD_IPA_POWERON_TIMEOUT 5000

#define REG_APB_EB 0x0
#define REG_ANALOG_USB31_PLL_V_USB31PLL_CTRL0 0x0
#define REG_ANALOG_USB31_PLL_V_REG_SEL_CFG_0 0x0028

#define MASK_USB31PLL_EB BIT(4)
#define MASK_ANALOG_USB31_PLL_V_RG_USB31PLLV_PD BIT(30)
#define MASK_DBG_SEL_ANALOG_USB31_PLL_V_RG_USB31PLLV_PD BIT(8)

static const char * const reg_name_tb_v3[] = {
	"ipa-sys-autoshutdownen",
	"ipa-sys-dslpen",
	"ipa-sys-state",
	"ipa-sys-forcelslp",
	"ipa-sys-lslpen",
	"ipa-sys-smartlslpen",
	"ipa-sys-accessen",
};

enum sipa_sys_reg_v3 {
	IPA_SYS_AUTOSHUTDOWNEN,
	IPA_SYS_DSLPEN,
	IPA_SYS_STATE,
	IPA_SYS_FORCELSLP,
	IPA_SYS_LSLPEN,
	IPA_SYS_SMARTLSLPEN,
	IPA_SYS_ACCESSEN,
};

static int sipa_sys_wait_power_on(struct sipa_sys_pd_drv *drv,
				  struct sipa_sys_register *reg_info)
{
	int ret = 0;
	u32 val = 0;

	if (reg_info->rmap)
		ret = regmap_read_poll_timeout(reg_info->rmap,
					       reg_info->reg,
					       val,
					       (((u32)(val & reg_info->mask)
						 & 0x1F00) >> 8) == 0,
					       SPRD_IPA_POWERON_POLL_US,
					       SPRD_IPA_POWERON_TIMEOUT);
	else
		usleep_range((SPRD_IPA_POWERON_TIMEOUT >> 2) + 1, 5000);

	if (ret)
		dev_err(drv->dev,
			"Polling check power on reg timed out: %x\n", val);

	return ret;
}

void ufs_cfg(struct sipa_sys_pd_drv *drv)
{
	regmap_update_bits(drv->dispc1_reg, REG_APB_EB,
			   MASK_USB31PLL_EB, MASK_USB31PLL_EB);
	regmap_update_bits(drv->anlg_reg,
			   REG_ANALOG_USB31_PLL_V_REG_SEL_CFG_0,
			   MASK_DBG_SEL_ANALOG_USB31_PLL_V_RG_USB31PLLV_PD,
			   MASK_DBG_SEL_ANALOG_USB31_PLL_V_RG_USB31PLLV_PD);
	regmap_update_bits(drv->anlg_reg,
			   REG_ANALOG_USB31_PLL_V_USB31PLL_CTRL0,
			   MASK_ANALOG_USB31_PLL_V_RG_USB31PLLV_PD,
			   0);
	regmap_update_bits(drv->dispc1_reg, REG_APB_EB, MASK_USB31PLL_EB, 0);
}

int sipa_sys_do_power_on_cb_v3(void *priv)
{
	u32 val = 0;
	int ret = 0;
	struct sipa_sys_register *reg_info;
	struct sipa_sys_pd_drv *drv = (struct sipa_sys_pd_drv *)priv;

	if (!drv)
		return -ENODEV;

	dev_dbg(drv->dev, "do power on\n");

	reg_info = &drv->regs[IPA_SYS_DSLPEN];
	if (reg_info->rmap) {
		ret = regmap_update_bits(reg_info->rmap,
					 reg_info->reg,
					 reg_info->mask,
					 ~reg_info->mask);
		if (ret < 0)
			dev_warn(drv->dev, "clear ipa dslp en fail\n");
	}

	/* check pd_ipa_auto_shutdown_en */
	reg_info = &drv->regs[IPA_SYS_AUTOSHUTDOWNEN];
	if (reg_info->rmap) {
		ret = regmap_read(reg_info->rmap,
				  reg_info->reg,
				  &val);
		if (ret < 0) {
			dev_warn(drv->dev,
				 "read ipa sys autoshutdownen error\n");
		}

		if (!((val & reg_info->mask) >> 24)) {
			ret = regmap_update_bits(reg_info->rmap,
						 reg_info->reg,
						 reg_info->mask,
						 reg_info->mask);
			if (ret < 0)
				dev_warn(drv->dev, "set ipa sys autoshutdown en\n");
		}
	}

	/* wait ipa_sys power on */
	reg_info = &drv->regs[IPA_SYS_STATE];
	ret = sipa_sys_wait_power_on(drv, reg_info);
	if (ret)
		dev_warn(drv->dev, "wait pwr on timeout\n");

	/* enable ipa_access eb bit, for asic initail value fault */
	reg_info = &drv->regs[IPA_SYS_ACCESSEN];
	if (reg_info->rmap) {
		ret = regmap_update_bits(reg_info->rmap,
					 reg_info->reg,
					 reg_info->mask,
					 reg_info->mask);
		if (ret < 0)
			dev_warn(drv->dev, "update access en fail\n");
	}

	/* set ipa core clock */
	if (drv->ipa_core_clk && drv->ipa_core_parent &&
	    drv->clk_ipa_ckg_eb) {
		ret = clk_prepare_enable(drv->ipa_core_parent);
		if (ret) {
			dev_err(drv->dev,
				"enable ipa_core_parent error\n");
			return ret;
		}
		ret = clk_prepare_enable(drv->clk_ipa_ckg_eb);
		if (ret) {
			dev_err(drv->dev,
				"enable clk_ipa_ckg_eb error\n");
			return ret;
		}
		clk_set_parent(drv->ipa_core_clk, drv->ipa_core_parent);
	}

	/*let ufs hibernate exit*/
	ufs_cfg(drv);

	return ret;
}

int sipa_sys_do_power_off_cb_v3(void *priv)
{
	int ret = 0;
	struct sipa_sys_register *reg_info;
	struct sipa_sys_pd_drv *drv = (struct sipa_sys_pd_drv *)priv;

	if (!drv)
		return -ENODEV;

	dev_dbg(drv->dev, "do power off\n");

	/* set ipa core clock to default */
	if (drv->ipa_core_clk && drv->ipa_core_parent &&
	    drv->ipa_core_default && drv->clk_ipa_ckg_eb) {
		clk_set_parent(drv->ipa_core_clk, drv->ipa_core_default);
		clk_disable_unprepare(drv->ipa_core_parent);
		clk_disable_unprepare(drv->clk_ipa_ckg_eb);
	}

	reg_info = &drv->regs[IPA_SYS_DSLPEN];
	if (reg_info->rmap) {
		ret = regmap_update_bits(reg_info->rmap,
					 reg_info->reg,
					 reg_info->mask,
					 reg_info->mask);
		if (ret < 0)
			dev_warn(drv->dev, "set dslp en bits fail\n");
	}

	/* disable ipa_access eb bit */
	reg_info = &drv->regs[IPA_SYS_ACCESSEN];
	if (reg_info->rmap) {
		ret = regmap_update_bits(reg_info->rmap,
					 reg_info->reg,
					 reg_info->mask,
					 ~reg_info->mask);
		if (ret < 0)
			dev_warn(drv->dev, "update access en fail\n");
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

void sipa_sys_init_cb_v3(void *priv)
{
	struct sipa_sys_pd_drv *drv = (struct sipa_sys_pd_drv *)priv;

	if (!drv)
		return;

	/* clear ipa force light sleep:0x0830[4] */
	sipa_sys_set_register(drv, &drv->regs[IPA_SYS_FORCELSLP], false);
	/* set ipa light sleep enable:0x0808[4] */
	sipa_sys_set_register(drv, &drv->regs[IPA_SYS_LSLPEN], true);
	/* set ipa smart light sleep enable:0x08cc[4] */
	sipa_sys_set_register(drv, &drv->regs[IPA_SYS_SMARTLSLPEN], true);
}

int sipa_sys_parse_dts_cb_v3(void *priv)
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
	for (i = 0; i < ARRAY_SIZE(reg_name_tb_v3); i++) {
		reg_name = reg_name_tb_v3[i];
		rmap = syscon_regmap_lookup_by_phandle_args(np, reg_name,
							    2, reg_info);
		if (IS_ERR(rmap)) {
			dev_warn(drv->dev, "Parse dts %s regmap fail\n",
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

	drv->dispc1_reg = syscon_regmap_lookup_by_phandle(np,
							  "sprd,syscon-dispc1-glb");
	if (IS_ERR(drv->dispc1_reg)) {
		dev_err(drv->dev, "dispc1 syscon failed\n");
		return PTR_ERR(drv->dispc1_reg);
	}

	drv->anlg_reg = syscon_regmap_lookup_by_phandle(np,
							"sprd,syscon-anlg-phy");
	if (IS_ERR(drv->anlg_reg)) {
		dev_err(drv->dev, "anlg syscon failed\n");
		return PTR_ERR(drv->anlg_reg);
	}

	return 0;
}

int sipa_sys_clk_enable_cb_v3(void *priv)
{
	struct sipa_sys_pd_drv *drv = (struct sipa_sys_pd_drv *)priv;

	if (!drv)
		return -ENODEV;

	drv->clk_ipa_ckg_eb = devm_clk_get(drv->dev, "clk_ipa_ckg_eb");
	if (IS_ERR(drv->clk_ipa_ckg_eb)) {
		dev_warn(drv->dev, "sipa_sys can't get the clk ipa ckg eb\n");
		return PTR_ERR(drv->clk_ipa_ckg_eb);
	}

	return 0;
}
