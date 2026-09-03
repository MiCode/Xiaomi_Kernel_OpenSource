// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>

#include "sprd_dvfs_apsys.h"
#include "apsys_dvfs_roc1.h"

char *roc1_apsys_val_to_volt(u32 val)
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

char *roc1_dpu_val_to_freq(u32 val)
{
	switch (val) {
	case 0:
		return "153.6M";
	case 1:
		return "192M";
	case 2:
		return "256M";
	case 3:
		return "307.2M";
	case 4:
		return "384M";
	case 5:
		return "468M";
	default:
		pr_err("invalid frequency value %u\n", val);
		return "N/A";
	}
}

char *roc1_vsp_val_to_freq(u32 val)
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
	default:
		pr_err("invalid frequency value %u\n", val);
		return "N/A";
	}
}

static void apsys_dvfs_force_en(struct apsys_dev *apsys, u32 force_en)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)apsys->apsys_base;

	if (force_en)
		reg->cgm_ap_dvfs_clk_gate_ctrl |= BIT(1);
	else
		reg->cgm_ap_dvfs_clk_gate_ctrl &= ~BIT(1);
}

static void apsys_dvfs_auto_gate(struct apsys_dev *apsys, u32 gate_sel)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)apsys->apsys_base;

	if (gate_sel)
		reg->cgm_ap_dvfs_clk_gate_ctrl |= BIT(0);
	else
		reg->cgm_ap_dvfs_clk_gate_ctrl &= ~BIT(0);
}

static void apsys_dvfs_hold_en(struct apsys_dev *apsys, u32 hold_en)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)apsys->apsys_base;

	reg->ap_dvfs_hold_ctrl = hold_en;
}

static void apsys_dvfs_wait_window(struct apsys_dev *apsys, u32 wait_window)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)apsys->apsys_base;

	reg->ap_dvfs_wait_window_cfg = wait_window;
}

static void apsys_dvfs_min_volt(struct apsys_dev *apsys, u32 min_volt)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)apsys->apsys_base;

	reg->ap_min_voltage_cfg = min_volt;
}

static void apsys_top_dvfs_init(struct apsys_dev *apsys)
{
	void __iomem *base;

	pr_info("%s()\n", __func__);

	base = ioremap(0x322a0000, 0x150);
	if (IS_ERR(base))
		pr_err("ioremap top dvfs address failed\n");

	apsys->top_base = (unsigned long)base;
}

static int dcdc_modem_cur_volt(struct apsys_dev *apsys)
{
	volatile u32 rw32;

	rw32 = *(volatile u32 *)(apsys->top_base + 0x0050);

	return (rw32 >> 20) & 0x07;
}

static int apsys_dvfs_parse_dt(struct apsys_dev *apsys,
				struct device_node *np)
{
	int ret;

	pr_info("%s()\n", __func__);

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
	apsys_dvfs_hold_en(apsys, apsys->dvfs_coffe.dvfs_hold_en);
	apsys_dvfs_force_en(apsys, apsys->dvfs_coffe.dvfs_force_en);
	apsys_dvfs_auto_gate(apsys, apsys->dvfs_coffe.dvfs_auto_gate);
	apsys_dvfs_wait_window(apsys, apsys->dvfs_coffe.dvfs_wait_window);
	apsys_dvfs_min_volt(apsys, apsys->dvfs_coffe.dvfs_min_volt);
}

const struct apsys_dvfs_ops roc1_apsys_dvfs_ops = {
	.parse_dt = apsys_dvfs_parse_dt,
	.dvfs_init = apsys_dvfs_init,
	.apsys_auto_gate = apsys_dvfs_auto_gate,
	.apsys_hold_en = apsys_dvfs_hold_en,
	.apsys_wait_window = apsys_dvfs_wait_window,
	.apsys_min_volt = apsys_dvfs_min_volt,
	.top_dvfs_init = apsys_top_dvfs_init,
	.top_cur_volt = dcdc_modem_cur_volt,
};
