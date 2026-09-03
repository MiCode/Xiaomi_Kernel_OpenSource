// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include "sprd_dvfs_apsys.h"
#include "sprd_dvfs_vdsp.h"
#include "../sys/apsys_dvfs_sharkl5pro.h"
#include "vdsp_dvfs_sharkl5pro.h"

#define CLK_PLL_NUM 3

enum {
	VDSP_TWPLL = 0,
	VDSP_LPLL,
	VDSP_ISPPLL,
};

static struct vdsp_sharkl5pro_dvfs_map_cfg map_table[] = {
	{0, SHARKL5PRO_VOLT70, SHARKL5PRO_VDSP_CLK_INDEX_256M,
		SHARKL5PRO_VDSP_CLK256M, SHARKL5PRO_EDAP_DIV_1},
	{1, SHARKL5PRO_VOLT70, SHARKL5PRO_VDSP_CLK_INDEX_384M,
		SHARKL5PRO_VDSP_CLK384M, SHARKL5PRO_EDAP_DIV_1},
	{2, SHARKL5PRO_VOLT70, SHARKL5PRO_VDSP_CLK_INDEX_512M,
		SHARKL5PRO_VDSP_CLK512M, SHARKL5PRO_EDAP_DIV_1},
	{3, SHARKL5PRO_VOLT75, SHARKL5PRO_VDSP_CLK_INDEX_614M4,
		SHARKL5PRO_VDSP_CLK614M4, SHARKL5PRO_EDAP_DIV_1},
	{4, SHARKL5PRO_VOLT75, SHARKL5PRO_VDSP_CLK_INDEX_768M,
		SHARKL5PRO_VDSP_CLK768M, SHARKL5PRO_EDAP_DIV_1},
	{5, SHARKL5PRO_VOLT80, SHARKL5PRO_VDSP_CLK_INDEX_936M,
		SHARKL5PRO_VDSP_CLK936M, SHARKL5PRO_EDAP_DIV_1},
};

static struct clk *clk_pll[CLK_PLL_NUM];

static const char *pll_names[CLK_PLL_NUM] = {
	"vdsp_twpll",
	"vdsp_lpll",
	"vdsp_isppll",
};

static u32 trans_index_to_freq(int index)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].map_index == index)
			return map_table[i].clk_rate;
	}
	return 0;
}

static int transfer_freq_to_pllindex(u32 freq)
{
	switch (freq) {
	case SHARKL5PRO_VDSP_CLK256M:
	case SHARKL5PRO_VDSP_CLK384M:
	case SHARKL5PRO_VDSP_CLK512M:
	case SHARKL5PRO_VDSP_CLK768M:
		return VDSP_TWPLL;
	case SHARKL5PRO_VDSP_CLK614M4:
		return VDSP_LPLL;
	case SHARKL5PRO_VDSP_CLK936M:
		return VDSP_ISPPLL;
	default:
		return VDSP_TWPLL;
	}
}

static int transfer_mapindex_to_pllindex(u32 index)
{
	switch (index) {
	case 0:
	case 1:
	case 2:
	case 3:
		return VDSP_TWPLL;
	case 4:
		return VDSP_LPLL;
	case 5:
		return VDSP_ISPPLL;
	default:
		return VDSP_TWPLL;
	}
}

static void vdsp_hw_dfs_en(struct vdsp_dvfs *vdsp, bool dfs_en)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;
	mutex_lock(&vdsp->apsys->reg_lock);
	if (dfs_en)
		reg->ap_dfs_en_ctrl |= BIT(2);
	else
		reg->ap_dfs_en_ctrl &= ~BIT(2);
	mutex_unlock(&vdsp->apsys->reg_lock);
}

static void vdsp_dvfs_map_cfg(struct vdsp_dvfs *vdsp)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;
	reg->vdsp_index0_map = map_table[0].clk_level |
		map_table[0].edap_div << 3 |
		map_table[0].volt_level << 5;
	reg->vdsp_index1_map = map_table[1].clk_level |
		map_table[1].edap_div << 3 |
		map_table[1].volt_level << 5;
	reg->vdsp_index2_map = map_table[2].clk_level |
		map_table[2].edap_div << 3 |
		map_table[2].volt_level << 5;
	reg->vdsp_index3_map = map_table[3].clk_level |
		map_table[3].edap_div << 3 |
		map_table[3].volt_level << 5;
	reg->vdsp_index4_map = map_table[4].clk_level |
		map_table[4].edap_div << 3 |
		map_table[4].volt_level << 5;
	reg->vdsp_index5_map = map_table[5].clk_level |
		map_table[5].edap_div << 3 |
		map_table[5].volt_level << 5;
}

static int set_vdsp_work_freq(struct vdsp_dvfs *vdsp, u32 freq)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;
	int i;
	int pll_index;
	int ret;

	pll_index = transfer_freq_to_pllindex(freq);
	ret = clk_prepare_enable(clk_pll[pll_index]);
	if (ret) {
		pr_err("func:%s clk_prepare_enable failed ret:%d , pllindex:%d\n",
			__func__, ret, pll_index);
		return ret;
	}
	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].clk_rate == freq) {
			reg->vdsp_dvfs_index_cfg = i;
			break;
		}
	}
	return ret;
}

static u32 get_vdsp_work_freq(struct vdsp_dvfs *vdsp)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;
	u32 freq = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].map_index ==
			reg->vdsp_dvfs_index_cfg) {
			freq = map_table[i].clk_rate;
			break;
		}
	}

	return freq;
}

static void set_vdsp_idle_freq(struct vdsp_dvfs *vdsp, u32 freq)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].clk_rate == freq) {
			reg->vdsp_dvfs_index_idle_cfg = i;
			break;
		}
	}
}

static u32 get_vdsp_idle_freq(struct vdsp_dvfs *vdsp)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;
	u32 freq = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].map_index ==
			reg->vdsp_dvfs_index_idle_cfg) {
			freq = map_table[i].clk_rate;
			break;
		}
	}

	return freq;
}

static int set_vdsp_work_index(struct vdsp_dvfs *vdsp, int index)
{
	int pll_index;
	int ret;
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;
	pll_index = transfer_mapindex_to_pllindex(index);
	ret = clk_prepare_enable(clk_pll[pll_index]);
	if (ret) {
		pr_err("func:%s clk_prepare_enable failed ret:%d , pllindex:%d\n",
			__func__, ret, pll_index);
		return ret;
	}
	reg->vdsp_dvfs_index_cfg = index;
	return ret;
}

static int get_vdsp_work_index(struct vdsp_dvfs *vdsp)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;

	return reg->vdsp_dvfs_index_cfg;
}

static void set_vdsp_idle_index(struct vdsp_dvfs *vdsp, int index)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;

	reg->vdsp_dvfs_index_idle_cfg = index;
}

static int get_vdsp_idle_index(struct vdsp_dvfs *vdsp)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;

	return reg->vdsp_dvfs_index_idle_cfg;
}

static void set_vdsp_gfree_wait_delay(struct vdsp_dvfs *vdsp, u32 para)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;
	u32 temp;

	mutex_lock(&vdsp->apsys->reg_lock);
	temp = reg->ap_gfree_wait_delay_cfg;
	temp &= GENMASK(19, 0);
	reg->ap_gfree_wait_delay_cfg = para << 20 | temp;
	mutex_unlock(&vdsp->apsys->reg_lock);
}

static void set_vdsp_freq_upd_en_byp(struct vdsp_dvfs *vdsp, bool enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;

	mutex_lock(&vdsp->apsys->reg_lock);
	if (enable)
		reg->ap_freq_update_bypass |= BIT(2);
	else
		reg->ap_freq_update_bypass &= ~BIT(2);
	mutex_unlock(&vdsp->apsys->reg_lock);
}

static void set_vdsp_freq_upd_delay_en(struct vdsp_dvfs *vdsp, bool enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;

	mutex_lock(&vdsp->apsys->reg_lock);
	if (enable)
		reg->ap_freq_upd_type_cfg |= BIT(1);
	else
		reg->ap_freq_upd_type_cfg &= ~BIT(1);
	mutex_unlock(&vdsp->apsys->reg_lock);
}

static void set_vdsp_freq_upd_hdsk_en(struct vdsp_dvfs *vdsp, bool enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;

	mutex_lock(&vdsp->apsys->reg_lock);
	if (enable)
		reg->ap_freq_upd_type_cfg |= BIT(0);
	else
		reg->ap_freq_upd_type_cfg &= ~BIT(0);
	mutex_unlock(&vdsp->apsys->reg_lock);
}

static void set_vdsp_dvfs_swtrig_en(struct vdsp_dvfs *vdsp, bool enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;

	mutex_lock(&vdsp->apsys->reg_lock);
	if (enable)
		reg->ap_sw_trig_ctrl |= BIT(0);
	else
		reg->ap_sw_trig_ctrl &= ~BIT(0);
	mutex_unlock(&vdsp->apsys->reg_lock);
}

static void set_vdsp_dvfs_table(struct ip_dvfs_map_cfg *dvfs_table)
{
	memcpy(map_table, dvfs_table, sizeof(struct
		ip_dvfs_map_cfg));
}

static int get_vdsp_dvfs_table(struct ip_dvfs_map_cfg *dvfs_table)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		dvfs_table[i].map_index = map_table[i].map_index;
		dvfs_table[i].volt_level = map_table[i].volt_level;
		dvfs_table[i].clk_level = map_table[i].clk_level;
		dvfs_table[i].clk_rate = map_table[i].clk_rate;
		dvfs_table[i].volt_val = NULL;
	}

	return i;
}

static void get_vdsp_dvfs_status(struct vdsp_dvfs *vdsp, struct ip_dvfs_status *dvfs_status)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vdsp->apsys->apsys_base;

	mutex_lock(&vdsp->apsys->reg_lock);
	dvfs_status->apsys_cur_volt =
		sharkl5pro_apsys_val_to_volt(reg->ap_dvfs_voltage_dbg >> 12 & 0x7);
	dvfs_status->vsp_vote_volt =
		sharkl5pro_apsys_val_to_volt(reg->ap_dvfs_voltage_dbg >> 6 & 0x7);
	dvfs_status->dpu_vote_volt =
		sharkl5pro_apsys_val_to_volt(reg->ap_dvfs_voltage_dbg >> 3 & 0x7);
	dvfs_status->vdsp_vote_volt =
		sharkl5pro_apsys_val_to_volt(reg->ap_dvfs_voltage_dbg >> 9 & 0x7);
	dvfs_status->vsp_cur_freq =
		sharkl5pro_vsp_val_to_freq(reg->ap_dvfs_cgm_cfg_dbg >> 3 & 0x3);
	dvfs_status->dpu_cur_freq =
		sharkl5pro_dpu_val_to_freq(reg->ap_dvfs_cgm_cfg_dbg & 0x7);
	dvfs_status->vdsp_cur_freq =
		sharkl5pro_vdsp_val_to_freq(reg->ap_dvfs_cgm_cfg_dbg >> 5 & 0x7);
	dvfs_status->vdsp_edap_div = reg->ap_dvfs_cgm_cfg_dbg >> 8 & 0x3;
	dvfs_status->vdsp_m0_div = reg->ap_dvfs_cgm_cfg_dbg >> 10 & 0x3;
	mutex_unlock(&vdsp->apsys->reg_lock);
}

static int vdsp_dvfs_parse_dt(struct vdsp_dvfs *vdsp,
				struct device_node *np)
{
	int ret;

	ret = of_property_read_u32(np, "sprd,gfree-wait-delay",
			&vdsp->dvfs_coffe.gfree_wait_delay);
	if (ret)
		vdsp->dvfs_coffe.gfree_wait_delay = 0x100;

	ret = of_property_read_u32(np, "sprd,freq-upd-hdsk-en",
			&vdsp->dvfs_coffe.freq_upd_hdsk_en);
	if (ret)
		vdsp->dvfs_coffe.freq_upd_hdsk_en = 1;

	ret = of_property_read_u32(np, "sprd,freq-upd-delay-en",
			&vdsp->dvfs_coffe.freq_upd_delay_en);
	if (ret)
		vdsp->dvfs_coffe.freq_upd_delay_en = 0;

	ret = of_property_read_u32(np, "sprd,freq-upd-en-byp",
			&vdsp->dvfs_coffe.freq_upd_en_byp);
	if (ret)
		vdsp->dvfs_coffe.freq_upd_en_byp = 0;

	ret = of_property_read_u32(np, "sprd,sw-trig-en",
			&vdsp->dvfs_coffe.sw_trig_en);
	if (ret)
		vdsp->dvfs_coffe.sw_trig_en = 0;
	return ret;
}

static int vdsp_dvfs_parse_pll(struct vdsp_dvfs *vdsp, struct device *dev)
{
	int i;

	for (i = 0; i < CLK_PLL_NUM; i++) {
		clk_pll[i] = devm_clk_get(dev, pll_names[i]);
		if (IS_ERR(clk_pll[i])) {
			dev_err(dev, "get clock %s failed\n", pll_names[i]);
			return PTR_ERR(clk_pll[i]);
		}
	}
	return 0;
}

static int vdsp_dvfs_init(struct vdsp_dvfs *vdsp)
{
	vdsp_dvfs_map_cfg(vdsp);
	set_vdsp_gfree_wait_delay(vdsp, vdsp->dvfs_coffe.gfree_wait_delay);
	set_vdsp_freq_upd_hdsk_en(vdsp, vdsp->dvfs_coffe.freq_upd_hdsk_en);
	set_vdsp_freq_upd_delay_en(vdsp, vdsp->dvfs_coffe.freq_upd_delay_en);
	set_vdsp_freq_upd_en_byp(vdsp, vdsp->dvfs_coffe.freq_upd_en_byp);
	set_vdsp_work_index(vdsp, vdsp->dvfs_coffe.work_index_def);
	vdsp->work_freq = trans_index_to_freq(vdsp->dvfs_coffe.work_index_def);
	set_vdsp_idle_index(vdsp, vdsp->dvfs_coffe.idle_index_def);
	vdsp->idle_freq = trans_index_to_freq(vdsp->dvfs_coffe.idle_index_def);
	vdsp_hw_dfs_en(vdsp, vdsp->dvfs_coffe.hw_dfs_en);

	return 0;
}

const struct vdsp_dvfs_ops sharkl5pro_vdsp_dvfs_ops = {
	.parse_dt = vdsp_dvfs_parse_dt,
	.parse_pll = vdsp_dvfs_parse_pll,
	.dvfs_init = vdsp_dvfs_init,
	.hw_dfs_en = vdsp_hw_dfs_en,
	.set_work_freq = set_vdsp_work_freq,
	.get_work_freq = get_vdsp_work_freq,
	.set_idle_freq = set_vdsp_idle_freq,
	.get_idle_freq = get_vdsp_idle_freq,
	.set_work_index = set_vdsp_work_index,
	.get_work_index = get_vdsp_work_index,
	.set_idle_index = set_vdsp_idle_index,
	.get_idle_index = get_vdsp_idle_index,
	.set_dvfs_table = set_vdsp_dvfs_table,
	.get_dvfs_table = get_vdsp_dvfs_table,
	.get_dvfs_status = get_vdsp_dvfs_status,

	.set_gfree_wait_delay = set_vdsp_gfree_wait_delay,
	.set_freq_upd_en_byp = set_vdsp_freq_upd_en_byp,
	.set_freq_upd_delay_en = set_vdsp_freq_upd_delay_en,
	.set_freq_upd_hdsk_en = set_vdsp_freq_upd_hdsk_en,
	.set_dvfs_swtrig_en = set_vdsp_dvfs_swtrig_en,
};
