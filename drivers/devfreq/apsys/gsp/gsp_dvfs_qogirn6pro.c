// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/soc/sprd/hwfeature.h>

#include "sprd_dvfs_apsys.h"
#include "sprd_dvfs_gsp.h"
#include "../sys/apsys_dvfs_qogirn6pro.h"

static struct ip_dvfs_map_cfg map_table[] = {
	{0, VOLT60, GSP_CLK_INDEX_256M, GSP_CLK256M},
	{1, VOLT65, GSP_CLK_INDEX_307M2, GSP_CLK307M2},
	{2, VOLT70, GSP_CLK_INDEX_384M, GSP_CLK384M},
	{3, VOLT75, GSP_CLK_INDEX_512M, GSP_CLK512M},
};

static void gsp_hw_dfs_en(struct gsp_dvfs *gsp, bool dfs_en)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;

	mutex_lock(&gsp->apsys->reg_lock);
	if (dfs_en)
		reg->dpu_vsp_dfs_en_ctrl |= BIT(2);
	else
		reg->dpu_vsp_dfs_en_ctrl &= ~BIT(2);
	mutex_unlock(&gsp->apsys->reg_lock);
}

static void gsp_dvfs_map_cfg(struct gsp_dvfs *gsp)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;

	reg->gsp0_index0_map = map_table[0].clk_level |
		map_table[0].volt_level << 2;
	reg->gsp0_index1_map = map_table[1].clk_level |
		map_table[1].volt_level << 2;
	reg->gsp0_index2_map = map_table[2].clk_level |
		map_table[2].volt_level << 2;
	reg->gsp0_index3_map = map_table[3].clk_level |
		map_table[3].volt_level << 2;
}

static void set_gsp_work_freq(struct gsp_dvfs *gsp, u32 freq)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].clk_rate == freq) {
			reg->vpu_gsp0_dvfs_index_cfg = i;
			break;
		}
	}
}

static u32 get_gsp_work_freq(struct gsp_dvfs *gsp)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;
	u32 freq = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].map_index ==
			reg->vpu_gsp0_dvfs_index_cfg) {
			freq = map_table[i].clk_rate;
			break;
		}
	}

	return freq;
}

static void set_gsp_idle_freq(struct gsp_dvfs *gsp, u32 freq)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].clk_rate == freq) {
			reg->vpu_gsp0_dvfs_index_idle_cfg = i;
			break;
		}
	}
}

static u32 get_gsp_idle_freq(struct gsp_dvfs *gsp)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;
	u32 freq = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].map_index ==
			reg->vpu_gsp0_dvfs_index_idle_cfg) {
			freq = map_table[i].clk_rate;
			break;
		}
	}

	return freq;
}

static void set_gsp_work_index(struct gsp_dvfs *gsp, int index)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;

	reg->vpu_gsp0_dvfs_index_cfg = index;
}

static int get_gsp_work_index(struct gsp_dvfs *gsp)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;

	return reg->vpu_gsp0_dvfs_index_cfg;
}

static void set_gsp_idle_index(struct gsp_dvfs *gsp, int index)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;

	reg->vpu_gsp0_dvfs_index_idle_cfg = index;
}

static int get_gsp_idle_index(struct gsp_dvfs *gsp)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;

	return reg->vpu_gsp0_dvfs_index_idle_cfg;
}

static void set_gsp_gfree_wait_delay(struct gsp_dvfs *gsp, u32 para)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;
	u32 temp;
	mutex_lock(&gsp->apsys->reg_lock);
	temp = reg->dpu_vsp_gfree_wait_delay_cfg0;
	temp &= GENMASK(9, 0);
	reg->dpu_vsp_gfree_wait_delay_cfg0 = para << 0 | temp;
	mutex_unlock(&gsp->apsys->reg_lock);
}

static void set_gsp_freq_upd_en_byp(struct gsp_dvfs *gsp, bool enable)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;

	mutex_lock(&gsp->apsys->reg_lock);
	if (enable)
		reg->dpu_vsp_freq_update_bypass |= BIT(3);
	else
		reg->dpu_vsp_freq_update_bypass &= ~BIT(3);
	mutex_unlock(&gsp->apsys->reg_lock);
}

static void set_gsp_freq_upd_delay_en(struct gsp_dvfs *gsp, bool enable)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;

	mutex_lock(&gsp->apsys->reg_lock);
	if (enable)
		reg->dpu_vsp_freq_upd_type_cfg |= BIT(7);
	else
		reg->dpu_vsp_freq_upd_type_cfg &= ~BIT(7);
	mutex_unlock(&gsp->apsys->reg_lock);
}

static void set_gsp_freq_upd_hdsk_en(struct gsp_dvfs *gsp, bool enable)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;

	mutex_lock(&gsp->apsys->reg_lock);
	if (enable)
		reg->dpu_vsp_freq_upd_type_cfg |= BIT(6);
	else
		reg->dpu_vsp_freq_upd_type_cfg &= ~BIT(6);
	mutex_unlock(&gsp->apsys->reg_lock);
}

static void set_gsp_dvfs_swtrig_en(struct gsp_dvfs *gsp, bool enable)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;

	mutex_lock(&gsp->apsys->reg_lock);
	if (enable)
		reg->dpu_vsp_sw_trig_ctrl |= BIT(0);
	else
		reg->dpu_vsp_sw_trig_ctrl &= ~BIT(0);
	mutex_unlock(&gsp->apsys->reg_lock);
}

static void set_gsp_dvfs_table(struct ip_dvfs_map_cfg *dvfs_table)
{
	memcpy(map_table, dvfs_table, sizeof(struct
		ip_dvfs_map_cfg));
}

static int get_gsp_dvfs_table(struct ip_dvfs_map_cfg *dvfs_table)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		dvfs_table[i].map_index = map_table[i].map_index;
		dvfs_table[i].volt_level = map_table[i].volt_level;
		dvfs_table[i].clk_level = map_table[i].clk_level;
		dvfs_table[i].clk_rate = map_table[i].clk_rate;
	}

	return i;
}

static void get_gsp_dvfs_status(struct gsp_dvfs *gsp, struct ip_dvfs_status *dvfs_status)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)gsp->apsys->apsys_base;

	mutex_lock(&gsp->apsys->reg_lock);
	/*dvfs_status->gsp1_vote_volt =
		qogirn6pro_gsp_val_to_volt(reg->dpu_vsp_dvfs_voltage_dbg >> 20 & 0x7);*/
	dvfs_status->gsp0_vote_volt =
		qogirn6pro_gsp_val_to_volt(reg->dpu_vsp_dvfs_voltage_dbg >> 16 & 0x7);

	dvfs_status->gsp0_cur_freq =
		qogirn6pro_gsp_val_to_freq(reg->dpu_vsp_vpu_gsp0_dvfs_cgm_cfg_dbg & 0x3);
	/*dvfs_status->gsp1_cur_freq =
		qogirn6pro_gsp_val_to_freq(reg->dpu_vsp_vpu_gsp0_dvfs_cgm_cfg_dbg & 0x3);*/
	mutex_unlock(&gsp->apsys->reg_lock);
}

static int gsp_dvfs_parse_dt(struct gsp_dvfs *gsp,
				struct device_node *np)
{
	int ret;

	pr_info("%s()\n", __func__);

	ret = of_property_read_u32(np, "sprd,gfree-wait-delay",
			&gsp->dvfs_coffe.gfree_wait_delay);
	if (ret)
		gsp->dvfs_coffe.gfree_wait_delay = 0x100;

	ret = of_property_read_u32(np, "sprd,freq-upd-hdsk-en",
			&gsp->dvfs_coffe.freq_upd_hdsk_en);
	if (ret)
		gsp->dvfs_coffe.freq_upd_hdsk_en = 1;

	ret = of_property_read_u32(np, "sprd,freq-upd-delay-en",
			&gsp->dvfs_coffe.freq_upd_delay_en);
	if (ret)
		gsp->dvfs_coffe.freq_upd_delay_en = 1;

	ret = of_property_read_u32(np, "sprd,freq-upd-en-byp",
			&gsp->dvfs_coffe.freq_upd_en_byp);
	if (ret)
		gsp->dvfs_coffe.freq_upd_en_byp = 0;

	ret = of_property_read_u32(np, "sprd,sw-trig-en",
			&gsp->dvfs_coffe.sw_trig_en);
	if (ret)
		gsp->dvfs_coffe.sw_trig_en = 0;

	return ret;
}

static int gsp_dvfs_init(struct gsp_dvfs *gsp)
{
	char chip_type[HWFEATURE_STR_SIZE_LIMIT];

	pr_info("%s()\n", __func__);

	if (dpu_vsp_dvfs_check_clkeb()) {
		pr_info("%s(), dpu_vsp eb is not on\n", __func__);
		return -1;
	}

	gsp_dvfs_map_cfg(gsp);
	set_gsp_gfree_wait_delay(gsp, gsp->dvfs_coffe.gfree_wait_delay);
	set_gsp_freq_upd_hdsk_en(gsp, gsp->dvfs_coffe.freq_upd_hdsk_en);
	set_gsp_freq_upd_delay_en(gsp, gsp->dvfs_coffe.freq_upd_delay_en);
	set_gsp_freq_upd_en_byp(gsp, gsp->dvfs_coffe.freq_upd_en_byp);
	set_gsp_work_index(gsp, gsp->dvfs_coffe.work_index_def);
	set_gsp_idle_index(gsp, gsp->dvfs_coffe.idle_index_def);

	sprd_kproperty_get("auto/chipid", chip_type, "-1");
	if (!strncmp(chip_type, "UMS512-AB", strlen("UMS512-AB")))
		gsp->dvfs_coffe.hw_dfs_en = 1;
	else
		gsp->dvfs_coffe.hw_dfs_en = 0;

	gsp_hw_dfs_en(gsp, gsp->dvfs_coffe.hw_dfs_en);

	return 0;
}

const struct gsp_dvfs_ops qogirn6pro_gsp_dvfs_ops = {
	.parse_dt = gsp_dvfs_parse_dt,
	.dvfs_init = gsp_dvfs_init,
	.hw_dfs_en = gsp_hw_dfs_en,
	.set_work_freq = set_gsp_work_freq,
	.get_work_freq = get_gsp_work_freq,
	.set_idle_freq = set_gsp_idle_freq,
	.get_idle_freq = get_gsp_idle_freq,
	.set_work_index = set_gsp_work_index,
	.get_work_index = get_gsp_work_index,
	.set_idle_index = set_gsp_idle_index,
	.get_idle_index = get_gsp_idle_index,
	.set_dvfs_table = set_gsp_dvfs_table,
	.get_dvfs_table = get_gsp_dvfs_table,
	.get_dvfs_status = get_gsp_dvfs_status,

	.set_gfree_wait_delay = set_gsp_gfree_wait_delay,
	.set_freq_upd_en_byp = set_gsp_freq_upd_en_byp,
	.set_freq_upd_delay_en = set_gsp_freq_upd_delay_en,
	.set_freq_upd_hdsk_en = set_gsp_freq_upd_hdsk_en,
	.set_dvfs_swtrig_en = set_gsp_dvfs_swtrig_en,
};
