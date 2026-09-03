// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include "sprd_dvfs_vsp.h"
#include "../sys/apsys_dvfs_sharkl5pro.h"

static struct ip_dvfs_map_cfg vsp_dvfs_config_table[] = {
	{0, VOLT70, VSP_CLK_INDEX_256, VSP_CLK256, "0.7v " },
	{1, VOLT70, VSP_CLK_INDEX_307, VSP_CLK307, "0.7v " },
	{2, VOLT75, VSP_CLK_INDEX_384, VSP_CLK384, "0.75v" },
};

static void vsp_hw_dvfs_en(struct vsp_dvfs *vsp, u32 dvfs_eb)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	if (dvfs_eb)
		reg->ap_dfs_en_ctrl |= BIT(1);
	else
		reg->ap_dfs_en_ctrl &= ~BIT(1);
	mutex_unlock(&vsp->apsys->reg_lock);
}

static void get_vsp_dvfs_table(struct ip_dvfs_map_cfg *dvfs_table)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(vsp_dvfs_config_table); i++) {
		dvfs_table[i].map_index = vsp_dvfs_config_table[i].map_index;
		dvfs_table[i].volt_level = vsp_dvfs_config_table[i].volt_level;
		dvfs_table[i].clk_level = vsp_dvfs_config_table[i].clk_level;
		dvfs_table[i].clk_rate = vsp_dvfs_config_table[i].clk_rate;
		dvfs_table[i].volt_val = vsp_dvfs_config_table[i].volt_val;
	}
}

static void get_vsp_index_from_table(u32 work_freq, u32 *index)
{
	unsigned long set_clk = 0;
	u32 i;

	*index = 0;
	pr_debug("dvfs ops: %s,work_freq=%u\n", __func__, work_freq);
	for (i = 0; i < ARRAY_SIZE(vsp_dvfs_config_table); i++) {
		set_clk = vsp_dvfs_config_table[i].clk_rate;
		if (work_freq == set_clk) {
			*index = i;
			break;
		}
	}
	pr_debug("dvfs ops: %s,index=%d\n", __func__, *index);
}

static void set_vsp_work_freq(struct vsp_dvfs *vsp, u32 work_freq)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;
	u32 index = 0;

	get_vsp_index_from_table(work_freq, &index);

	reg->vsp_dvfs_index_cfg = index;

	pr_debug("dvfs ops: %s, work_freq=%u, index=%d,\n",
		__func__, work_freq, index);
}

static u32 get_vsp_work_freq(struct vsp_dvfs *vsp)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;
	u32 freq = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(vsp_dvfs_config_table); i++) {
		if (vsp_dvfs_config_table[i].map_index ==
			reg->vsp_dvfs_index_cfg) {
			freq = vsp_dvfs_config_table[i].clk_rate;
			break;
		}
	}

	return freq;
}

static void set_vsp_idle_freq(struct vsp_dvfs *vsp, u32 idle_freq)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;
	u32 index = 0;

	get_vsp_index_from_table(idle_freq, &index);

	reg->vsp_dvfs_index_idle_cfg = index;

	pr_debug("dvfs ops: %s, work_freq=%u, index=%d,\n",
		__func__, idle_freq, index);
}

static u32 get_vsp_idle_freq(struct vsp_dvfs *vsp)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;
	u32 freq = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(vsp_dvfs_config_table); i++) {
		if (vsp_dvfs_config_table[i].map_index ==
			reg->vsp_dvfs_index_idle_cfg) {
			freq = vsp_dvfs_config_table[i].clk_rate;
			break;
		}
	}

	return freq;
}

static void set_vsp_work_index(struct vsp_dvfs *vsp, u32 index)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;
	reg->vsp_dvfs_index_cfg = index;
}

static u32 get_vsp_work_index(struct vsp_dvfs *vsp)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;
	return reg->vsp_dvfs_index_cfg;
}

static void set_vsp_idle_index(struct vsp_dvfs *vsp, u32 index)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;

	reg->vsp_dvfs_index_idle_cfg = index;
}

static u32 get_vsp_idle_index(struct vsp_dvfs *vsp)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;
	return reg->vsp_dvfs_index_idle_cfg;
}

static void get_vsp_dvfs_status(struct vsp_dvfs *vsp, struct ip_dvfs_status *ip_status)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	ip_status->apsys_cur_volt =
		sharkl5pro_apsys_val_to_volt(reg->ap_dvfs_voltage_dbg >> 12 & 0x7);
	ip_status->vsp_vote_volt =
		sharkl5pro_apsys_val_to_volt(reg->ap_dvfs_voltage_dbg >> 6 & 0x7);
	ip_status->dpu_vote_volt =
		sharkl5pro_apsys_val_to_volt(reg->ap_dvfs_voltage_dbg >> 3 & 0x7);
	ip_status->vdsp_vote_volt = "N/A";

	ip_status->vsp_cur_freq =
		sharkl5pro_vsp_val_to_freq(reg->ap_dvfs_cgm_cfg_dbg >> 3 & 0x3);
	ip_status->dpu_cur_freq =
		sharkl5pro_dpu_val_to_freq(reg->ap_dvfs_cgm_cfg_dbg & 0x7);
	ip_status->vdsp_cur_freq = "N/A";
	mutex_unlock(&vsp->apsys->reg_lock);
}

static void set_vsp_gfree_wait_delay(struct vsp_dvfs *vsp, u32 wind_para)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	reg->ap_gfree_wait_delay_cfg |= (wind_para & 0x3ff);
	mutex_unlock(&vsp->apsys->reg_lock);
}

static void set_vsp_freq_upd_en_byp(struct vsp_dvfs *vsp, u32 on)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	if (on)
		reg->ap_freq_update_bypass |= BIT(1);
	else
		reg->ap_freq_update_bypass &= ~BIT(1);
	mutex_unlock(&vsp->apsys->reg_lock);
}

static void set_vsp_freq_upd_delay_en(struct vsp_dvfs *vsp, u32 on)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	if (on)
		reg->ap_freq_upd_type_cfg |= BIT(3);
	else
		reg->ap_freq_upd_type_cfg &= ~BIT(3);
	mutex_unlock(&vsp->apsys->reg_lock);
}

static void set_vsp_freq_upd_hdsk_en(struct vsp_dvfs *vsp, u32 on)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	if (on)
		reg->ap_freq_upd_type_cfg |= BIT(2);
	else
		reg->ap_freq_upd_type_cfg &= ~BIT(2);
	mutex_unlock(&vsp->apsys->reg_lock);
}

static void set_vsp_dvfs_swtrig_en(struct vsp_dvfs *vsp, u32 en)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	if (en)
		reg->ap_sw_trig_ctrl |= BIT(1);
	else
		reg->ap_sw_trig_ctrl &= ~BIT(1);
	mutex_unlock(&vsp->apsys->reg_lock);
}

static void vsp_dvfs_map_cfg(struct vsp_dvfs *vsp)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)vsp->apsys->apsys_base;

	reg->vsp_index0_map = vsp_dvfs_config_table[0].clk_level |
		vsp_dvfs_config_table[0].volt_level << 2;
	reg->vsp_index1_map = vsp_dvfs_config_table[1].clk_level |
		vsp_dvfs_config_table[1].volt_level << 2;
	reg->vsp_index2_map = vsp_dvfs_config_table[2].clk_level |
		vsp_dvfs_config_table[2].volt_level << 2;
}
static void vsp_dvfs_parse_dt(struct vsp_dvfs *vsp,
				struct device_node *np)
{
	if (of_property_read_u32(np, "sprd,gfree-wait-delay",
			&vsp->ip_coeff.gfree_wait_delay))
		vsp->ip_coeff.gfree_wait_delay = 0x100;

	if (of_property_read_u32(np, "sprd,freq-upd-hdsk-en",
			&vsp->ip_coeff.freq_upd_hdsk_en))
		vsp->ip_coeff.freq_upd_hdsk_en = 1;

	if (of_property_read_u32(np, "sprd,freq-upd-delay-en",
			&vsp->ip_coeff.freq_upd_delay_en))
		vsp->ip_coeff.freq_upd_delay_en = 1;

	if (of_property_read_u32(np, "sprd,freq-upd-en-byp",
			&vsp->ip_coeff.freq_upd_en_byp))
		vsp->ip_coeff.freq_upd_en_byp = 0;

	if (of_property_read_u32(np, "sprd,sw-trig-en",
			&vsp->ip_coeff.sw_trig_en))
		vsp->ip_coeff.sw_trig_en = 0;
}

static int vsp_dvfs_init(struct vsp_dvfs *vsp)
{
	vsp_dvfs_map_cfg(vsp);
	set_vsp_gfree_wait_delay(vsp, vsp->ip_coeff.gfree_wait_delay);
	set_vsp_freq_upd_hdsk_en(vsp, vsp->ip_coeff.freq_upd_hdsk_en);
	set_vsp_freq_upd_delay_en(vsp, vsp->ip_coeff.freq_upd_delay_en);
	set_vsp_freq_upd_en_byp(vsp, vsp->ip_coeff.freq_upd_en_byp);

	set_vsp_work_freq(vsp, vsp->work_freq);
	set_vsp_idle_freq(vsp, vsp->idle_freq);
	vsp_hw_dvfs_en(vsp, vsp->ip_coeff.hw_dfs_en);

	return 0;
}

static void updata_vsp_target_freq(struct vsp_dvfs *vsp, u32 freq, set_freq_type freq_type)
{
	if (freq_type == DVFS_WORK)
		set_vsp_work_freq(vsp, freq);
	else
		set_vsp_idle_freq(vsp, freq);
}

const struct ip_dvfs_ops sharkl5pro_vsp_dvfs_ops  =  {
	.parse_dt = vsp_dvfs_parse_dt,
	.dvfs_init = vsp_dvfs_init,
	.hw_dvfs_en = vsp_hw_dvfs_en,

	.set_work_freq = set_vsp_work_freq,
	.get_work_freq = get_vsp_work_freq,
	.set_idle_freq = set_vsp_idle_freq,
	.get_idle_freq = get_vsp_idle_freq,
	.set_work_index = set_vsp_work_index,
	.get_work_index = get_vsp_work_index,
	.set_idle_index = set_vsp_idle_index,
	.get_idle_index = get_vsp_idle_index,

	.get_dvfs_table = get_vsp_dvfs_table,
	.get_dvfs_status = get_vsp_dvfs_status,
	.updata_target_freq = updata_vsp_target_freq,

	.set_gfree_wait_delay = set_vsp_gfree_wait_delay,
	.set_freq_upd_en_byp = set_vsp_freq_upd_en_byp,
	.set_freq_upd_delay_en = set_vsp_freq_upd_delay_en,
	.set_freq_upd_hdsk_en = set_vsp_freq_upd_hdsk_en,
	.set_dvfs_swtrig_en = set_vsp_dvfs_swtrig_en,
};
