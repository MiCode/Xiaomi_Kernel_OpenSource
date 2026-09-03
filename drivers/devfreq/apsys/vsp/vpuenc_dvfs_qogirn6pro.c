/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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

#include "sprd_dvfs_apsys.h"
#include "sprd_dvfs_vsp.h"
#include "../sys/apsys_dvfs_qogirn6pro.h"

static struct ip_dvfs_map_cfg vsp_dvfs_config_table[] = {
	{0, VOL65, VSP_CLK_INDEX_256, VSP_CLK256, "0.65v " },
	{1, VOL65, VSP_CLK_INDEX_307, VSP_CLK307, "0.65v " },
	{2, VOL70, VSP_CLK_INDEX_384, VSP_CLK384, "0.7v " },
	{3, VOL75, VSP_CLK_INDEX_512, VSP_CLK512, "0.75v " },
};

static void vsp_hw_dvfs_en(struct vsp_dvfs *vsp, u32 dvfs_eb)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	if (dvfs_eb)
		reg->dpu_vsp_dfs_en_ctrl |= BIT(1);
	else
		reg->dpu_vsp_dfs_en_ctrl &= ~BIT(1);
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
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;
	u32 index = 0;

	get_vsp_index_from_table(work_freq, &index);

	reg->vpu_enc_dvfs_index_cfg = index;

	pr_debug("dvfs ops: %s, work_freq=%u, index=%d,\n",
		__func__, work_freq, index);
}

static u32 get_vsp_work_freq(struct vsp_dvfs *vsp)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;
	u32 freq = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(vsp_dvfs_config_table); i++) {
		if (vsp_dvfs_config_table[i].map_index ==
			reg->vpu_enc_dvfs_index_cfg) {
			freq = vsp_dvfs_config_table[i].clk_rate;
			break;
		}
	}

	return freq;
}

static void set_vsp_idle_freq(struct vsp_dvfs *vsp, u32 idle_freq)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;
	u32 index = 0;

	get_vsp_index_from_table(idle_freq, &index);

	reg->vpu_enc_dvfs_index_idle_cfg = index;

	pr_debug("dvfs ops: %s, work_freq=%u, index=%d,\n",
		__func__, idle_freq, index);
}

static u32 get_vsp_idle_freq(struct vsp_dvfs *vsp)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;
	u32 freq = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(vsp_dvfs_config_table); i++) {
		if (vsp_dvfs_config_table[i].map_index ==
			reg->vpu_dec_dvfs_index_idle_cfg) {
			freq = vsp_dvfs_config_table[i].clk_rate;
			break;
		}
	}

	return freq;
}

static void set_vsp_work_index(struct vsp_dvfs *vsp, u32 index)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;
	reg->vpu_enc_dvfs_index_cfg = index;
}

static u32 get_vsp_work_index(struct vsp_dvfs *vsp)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;
	return reg->vpu_enc_dvfs_index_cfg;
}

static void set_vsp_idle_index(struct vsp_dvfs *vsp, u32 index)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;

	reg->vpu_enc_dvfs_index_idle_cfg = index;
}

static u32 get_vsp_idle_index(struct vsp_dvfs *vsp)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;
	return reg->vpu_enc_dvfs_index_idle_cfg;
}

static void get_vsp_dvfs_status(struct vsp_dvfs *vsp, struct ip_dvfs_status *ip_status)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	ip_status->apsys_cur_volt =
		qogirn6pro_vpu_val_to_volt(reg->dpu_vsp_dvfs_state_dbg >> 8 & 0x7);
	ip_status->vpuenc_vote_volt =
		qogirn6pro_vpu_val_to_volt(reg->dpu_vsp_dvfs_voltage_dbg >> 8 & 0xf);
	ip_status->vsp_vote_volt =
		qogirn6pro_vpu_val_to_volt(reg->dpu_vsp_dvfs_voltage_dbg >> 12 & 0xf);
	ip_status->vpuenc_cur_freq =
		qogirn6pro_vpuenc_val_to_freq(reg->dpu_vsp_vpu_enc_dvfs_cgm_cfg_dbg & 0x3);
	ip_status->vsp_cur_freq =
		qogirn6pro_vpudec_val_to_freq(reg->dpu_vsp_vpu_dec_dvfs_cgm_cfg_dbg & 0x7);
	mutex_unlock(&vsp->apsys->reg_lock);
}

static void set_vsp_gfree_wait_delay(struct vsp_dvfs *vsp, u32 wind_para)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	reg->dpu_vsp_gfree_wait_delay_cfg1 |= ((wind_para & 0x3ff) << 10);
	mutex_unlock(&vsp->apsys->reg_lock);
}

static void set_vsp_freq_upd_en_byp(struct vsp_dvfs *vsp, u32 on)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	if (on)
		reg->dpu_vsp_freq_update_bypass |= BIT(1);
	else
		reg->dpu_vsp_freq_update_bypass &= ~BIT(1);
	mutex_unlock(&vsp->apsys->reg_lock);
}

static void set_vsp_freq_upd_delay_en(struct vsp_dvfs *vsp, u32 on)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	if (on)
		reg->dpu_vsp_freq_upd_type_cfg |= BIT(3);
	else
		reg->dpu_vsp_freq_upd_type_cfg &= ~BIT(3);
	mutex_unlock(&vsp->apsys->reg_lock);
}

static void set_vsp_freq_upd_hdsk_en(struct vsp_dvfs *vsp, u32 on)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	if (on)
		reg->dpu_vsp_freq_upd_type_cfg |= BIT(2);
	else
		reg->dpu_vsp_freq_upd_type_cfg &= ~BIT(2);
	mutex_unlock(&vsp->apsys->reg_lock);
}

static void set_vsp_dvfs_swtrig_en(struct vsp_dvfs *vsp, u32 en)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;

	mutex_lock(&vsp->apsys->reg_lock);
	if (en)
		reg->dpu_vsp_sw_trig_ctrl |= BIT(0);
	else
		reg->dpu_vsp_sw_trig_ctrl &= ~BIT(0);
	mutex_unlock(&vsp->apsys->reg_lock);
}

static void vsp_dvfs_map_cfg(struct vsp_dvfs *vsp)
{
	struct dpu_vspsys_dvfs_reg *reg =
		(struct dpu_vspsys_dvfs_reg *)vsp->apsys->apsys_base;

	reg->vpu_enc_index0_map = vsp_dvfs_config_table[0].clk_level |
		vsp_dvfs_config_table[0].volt_level << 2;
	reg->vpu_enc_index1_map = vsp_dvfs_config_table[1].clk_level |
		vsp_dvfs_config_table[1].volt_level << 2;
	reg->vpu_enc_index2_map = vsp_dvfs_config_table[2].clk_level |
		vsp_dvfs_config_table[2].volt_level << 2;
	reg->vpu_enc_index3_map = vsp_dvfs_config_table[3].clk_level |
		vsp_dvfs_config_table[3].volt_level << 2;
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
	if (dpu_vsp_dvfs_check_clkeb()) {
		pr_info("%s(), dpu_vsp eb is not on\n", __func__);
		return 0;
	}
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

const struct ip_dvfs_ops qogirn6pro_vpuenc_vsp_dvfs_ops  =  {
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
