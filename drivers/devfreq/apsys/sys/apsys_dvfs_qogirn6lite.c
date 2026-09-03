// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>

#include "sprd_dvfs_apsys.h"
#include "apsys_dvfs_qogirn6lite.h"

char *qogirn6lite_apsys_val_to_volt(u32 val)
{
	switch (val) {
	case 0:
		return "0.7v";
	case 1:
		return "0.75v";
	case 2:
		return "0.8v";
	default:
		pr_err("invalid voltage value %u\n", val);
		return "N/A";
	}
}

char *qogirn6lite_dpu_val_to_freq(u32 val)
{
	switch (val) {
	case 0:
		return "256M";
	case 1:
		return "307.2M";
	case 2:
		return "384M";
	case 3:
		return "409.6M";
	case 4:
		return "512M";
	case 5:
		return "614.4M";
	default:
		pr_err("invalid frequency value %u\n", val);
		return "N/A";
	}
}

char *qogirn6lite_gsp_val_to_volt(u32 val)
{
	switch (val) {
	case 0:
		return "0.55v";
	case 1:
		return "0.6v";
	case 2:
		return "0.65v";
	case 3:
		return "0.7v";
	case 4:
		return "0.75v";
	default:
		pr_err("invalid voltage value %u\n", val);
		return "N/A";
	}
}

char *qogirn6lite_gsp_val_to_freq(u32 val)
{
	switch (val) {
	case 0:
		return "256M";
	case 1:
		return "307.2M";
	case 2:
		return "384M";
	case 3:
		return "512M";
	case 4:
		return "614.4M";
	default:
		pr_err("invalid frequency value %u\n", val);
		return "N/A";
	}
}

char *qogirn6lite_vpu_val_to_volt(u32 val)
{
	switch (val) {
	case 0:
		return "0.55v";
	case 1:
		return "0.6v";
	case 2:
		return "0.65v";
	case 3:
		return "0.7v";
	case 4:
		return "0.75v";
	default:
		pr_err("invalid voltage value %u\n", val);
		return "N/A";
	}
}

char *qogirn6lite_vpuenc_val_to_freq(u32 val)
{
	switch (val) {
	case 0:
		return "256M";
	case 1:
		return "307.2M";
	case 2:
		return "384M";
	case 3:
		return "512M";
	case 4:
		return "614.4M";
	default:
		pr_err("invalid frequency value %u\n", val);
		return "N/A";
	}
}

char *qogirn6lite_vpudec_val_to_freq(u32 val)
{
	switch (val) {
	case 0:
		return "256M";
	case 1:
		return "307.2M";
	case 2:
		return "384M";
	case 3:
		return "512M";
	case 4:
		return "614.4M";
	default:
		pr_err("invalid frequency value %u\n", val);
		return "N/A";
	}
}

static void apsys_dvfs_force_en(struct apsys_dev *apsys, u32 force_en)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)apsys->apsys_base;

	if (force_en)
		reg->cgm_dpu_vsp_dvfs_clk_gate_ctrl |= BIT(1);
	else
		reg->cgm_dpu_vsp_dvfs_clk_gate_ctrl &= ~BIT(1);
}

static void apsys_dvfs_auto_gate(struct apsys_dev *apsys, u32 gate_sel)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)apsys->apsys_base;

	if (gate_sel)
		reg->cgm_dpu_vsp_dvfs_clk_gate_ctrl |= BIT(0);
	else
		reg->cgm_dpu_vsp_dvfs_clk_gate_ctrl &= ~BIT(0);
}

static void apsys_dvfs_hold_en(struct apsys_dev *apsys, u32 hold_en)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)apsys->apsys_base;

	reg->dpu_vsp_dvfs_hold_ctrl = hold_en;
}

static void apsys_dvfs_wait_window(struct apsys_dev *apsys, u32 wait_window)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)apsys->apsys_base;

	reg->dpu_vsp_dvfs_wait_window_cfg = wait_window;
}

static void apsys_dvfs_min_volt(struct apsys_dev *apsys, u32 min_volt)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)apsys->apsys_base;

	reg->dpu_vsp_min_voltage_cfg = min_volt;
}

static const struct of_device_id sprd_dvfs_of_match[] = {
	{ .compatible = "sprd,ump962x-syscon", },
	{ /* necessary */ },
};

static void apsys_top_dvfs_init(struct apsys_dev *apsys)
{
	void __iomem *base;
	struct platform_device *pdev_regmap;
	struct device_node *regmap_np;
	const struct of_device_id *dev_id;
	struct regmap *regmap;
	u32 temp;

	base = ioremap(0x64940000, 0x600);
	if (IS_ERR(base))
		pr_err("ioremap top address failed\n");

	apsys->top_base = (unsigned long)base;

	regmap_np = of_find_matching_node_and_match(NULL, sprd_dvfs_of_match,
			&dev_id);
	if (!regmap_np) {
		pr_err("regmap get device node fail\n");
		return;
	}

	pdev_regmap = of_find_device_by_node(regmap_np);
	if (!pdev_regmap) {
		pr_err("parent device get device node fail\n");
		return;
	}

	regmap = dev_get_regmap(pdev_regmap->dev.parent, NULL);
	regmap_read(regmap, 0xa158, &temp);
	/*0xa158 pmic mm dvfs enable register*/
	temp |= BIT(2);
	regmap_write(regmap, 0xa158, temp);
}

static int dcdc_modem_cur_volt(struct apsys_dev *apsys)
{
	volatile u32 rw32;

	rw32 = *(volatile u32 *)(apsys->top_base + 0x03f8);

	return (rw32 >> 20) & 0x07;
}

static int apsys_dvfs_parse_dt(struct apsys_dev *apsys,
				struct device_node *np)
{
	int ret;

	pr_info("%s()\n", __func__);

	regmap_aon_base = syscon_regmap_lookup_by_phandle(np, "sprd,aon_apb_regs_syscon");
	if (IS_ERR(regmap_aon_base))
		pr_err("ioremap aon address failed\n");

	ret = of_property_read_u32(np, "sprd,ap-dvfs-hold",
			&apsys->dvfs_coffe.dvfs_hold_en);
	if (ret)
		apsys->dvfs_coffe.dvfs_hold_en = 0;

	ret = of_property_read_u32(np, "sprd,ap-dvfs-force-en",
			&apsys->dvfs_coffe.dvfs_force_en);
	if (ret)
		apsys->dvfs_coffe.dvfs_force_en = 1;

	ret = of_property_read_u32(np, "sprd,ap-dvfs-auto-gate",
			&apsys->dvfs_coffe.dvfs_auto_gate);
	if (ret)
		apsys->dvfs_coffe.dvfs_auto_gate = 0;

	ret = of_property_read_u32(np, "sprd,ap-dvfs-wait-window",
			&apsys->dvfs_coffe.dvfs_wait_window);
	if (ret)
		apsys->dvfs_coffe.dvfs_wait_window = 0x10080;

	ret = of_property_read_u32(np, "sprd,ap-dvfs-min-volt",
			&apsys->dvfs_coffe.dvfs_min_volt);
	if (ret)
		apsys->dvfs_coffe.dvfs_min_volt = 0;

	return ret;
}

static void apsys_dvfs_init(struct apsys_dev *apsys)
{
	if (dpu_vsp_dvfs_check_clkeb()) {
		pr_info("%s(), dpu_vsp eb is not on\n", __func__);
		return;
	}
	apsys_dvfs_hold_en(apsys, apsys->dvfs_coffe.dvfs_hold_en);
	apsys_dvfs_force_en(apsys, apsys->dvfs_coffe.dvfs_force_en);
	apsys_dvfs_auto_gate(apsys, apsys->dvfs_coffe.dvfs_auto_gate);
	apsys_dvfs_wait_window(apsys, apsys->dvfs_coffe.dvfs_wait_window);
	apsys_dvfs_min_volt(apsys, apsys->dvfs_coffe.dvfs_min_volt);
}

const struct apsys_dvfs_ops qogirn6lite_apsys_dvfs_ops = {
	.parse_dt = apsys_dvfs_parse_dt,
	.dvfs_init = apsys_dvfs_init,
	.apsys_auto_gate = apsys_dvfs_auto_gate,
	.apsys_hold_en = apsys_dvfs_hold_en,
	.apsys_wait_window = apsys_dvfs_wait_window,
	.apsys_min_volt = apsys_dvfs_min_volt,
	.top_dvfs_init = apsys_top_dvfs_init,
	.top_cur_volt = dcdc_modem_cur_volt,
};
