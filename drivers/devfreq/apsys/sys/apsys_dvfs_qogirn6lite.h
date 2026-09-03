// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __DPU_VSP_SYS_REG_qogirn6lite_H__
#define __DPU_VSP_SYS_REG_qogirn6lite_H__

enum {
	VOL55 = 0, //0.55v
	VOL60, //0.60v
	VOL65, //0.65v
	VOL70, //0.70v
	VOL75, //0.75v
};

struct dpu_vspsys_dvfs_reg {
	u32 dpu_vsp_dvfs_hold_ctrl;
	u32 dpu_vsp_dvfs_wait_window_cfg;
	u32 reserved_0x08_0x10[3];
	u32 dpu_vsp_dfs_en_ctrl;
	u32 reserved_0x18;
	u32 dpu_vsp_sw_trig_ctrl;
	u32 reserved_0x20;
	u32 dpu_vsp_min_voltage_cfg;
	u32 reserved_0x0028_0x0030[3];
	u32 dpu_vsp_auto_tune_en_ctrl;
	u32 dpu_vsp_sw_dvfs_ctrl;
	u32 reserved_0x003c_0x0044[3];
	u32 dpu_vsp_freq_update_bypass;
	u32 reserved_0x004c;
	u32 cgm_dpu_vsp_dvfs_clk_gate_ctrl;
	u32 dpu_vsp_dvfs_voltage_dbg;
	u32 reserved_0x58_0x64[4];
	u32 dpu_vsp_vpu_mtx_dvfs_cgm_cfg_dbg;
	u32 dpu_vsp_vpu_enc_dvfs_cgm_cfg_dbg;
	u32 dpu_vsp_vpu_dec_dvfs_cgm_cfg_dbg;
	u32 dpu_vsp_vpu_gsp0_dvfs_cgm_cfg_dbg;
	u32 dpu_vsp_vpu_dispc0_dvfs_cgm_cfg_dbg;
	u32 reserved_0x7c_0x12c[45];
	u32 dpu_vsp_dvfs_state_dbg;
	u32 reserved_0x134_0x150[8];
	u32 vpu_mtx_index0_map;
	u32 vpu_mtx_index1_map;
	u32 vpu_mtx_index2_map;
	u32 vpu_mtx_index3_map;
	u32 vpu_mtx_index4_map;
	u32 vpu_mtx_index5_map;
	u32 vpu_mtx_index6_map;
	u32 vpu_mtx_index7_map;
	u32 vpu_enc_index0_map;
	u32 vpu_enc_index1_map;
	u32 vpu_enc_index2_map;
	u32 vpu_enc_index3_map;
	u32 vpu_enc_index4_map;
	u32 vpu_enc_index5_map;
	u32 vpu_enc_index6_map;
	u32 vpu_enc_index7_map;
	u32 vpu_dec_index0_map;
	u32 vpu_dec_index1_map;
	u32 vpu_dec_index2_map;
	u32 vpu_dec_index3_map;
	u32 vpu_dec_index4_map;
	u32 vpu_dec_index5_map;
	u32 vpu_dec_index6_map;
	u32 vpu_dec_index7_map;
	u32 gsp0_index0_map;
	u32 gsp0_index1_map;
	u32 gsp0_index2_map;
	u32 gsp0_index3_map;
	u32 gsp0_index4_map;
	u32 gsp0_index5_map;
	u32 gsp0_index6_map;
	u32 gsp0_index7_map;
	u32 dispc_index0_map;
	u32 dispc_index1_map;
	u32 dispc_index2_map;
	u32 dispc_index3_map;
	u32 dispc_index4_map;
	u32 dispc_index5_map;
	u32 dispc_index6_map;
	u32 dispc_index7_map;
	u32 reserved_0x1f4_0x790[360];
	u32 vpu_mtx_dvfs_index_cfg;
	u32 vpu_mtx_dvfs_index_idle_cfg;
	u32 vpu_enc_dvfs_index_cfg;
	u32 vpu_enc_dvfs_index_idle_cfg;
	u32 vpu_dec_dvfs_index_cfg;
	u32 vpu_dec_dvfs_index_idle_cfg;
	u32 vpu_gsp0_dvfs_index_cfg;
	u32 vpu_gsp0_dvfs_index_idle_cfg;
	u32 dispc_dvfs_index_cfg;
	u32 dispc_dvfs_index_idle_cfg;
	u32 reserved_0x7bc_0x920[90];
	u32 dpu_vsp_freq_upd_state;
	u32 reserved_0x928_0x940[7];
	u32 dpu_vsp_gfree_wait_delay_cfg0;
	u32 dpu_vsp_gfree_wait_delay_cfg1;
	u32 reserved_0x94c_0x9a8[24];
	u32 dpu_vsp_freq_upd_type_cfg;
	u32 reserved_0x9b0;
	u32 dpu_vsp_idle_disable_cfg;
	u32 reserved_0x9b8_0xa54[40];
	u32 dpu_vsp_dvfs_reserved_reg_cfg0;
	u32 dpu_vsp_dvfs_reserved_reg_cfg1;
	u32 dpu_vsp_dvfs_reserved_reg_cfg2;
	u32 dpu_vsp_dvfs_reserved_reg_cfg3;
};

char *qogirn6lite_apsys_val_to_volt(u32 val);
char *qogirn6lite_dpu_vspsys_val_to_volt(u32 val);
char *qogirn6lite_dpu_val_to_freq(u32 val);
char *qogirn6lite_gsp_val_to_volt(u32 val);
char *qogirn6lite_gsp_val_to_freq(u32 val);
char *qogirn6lite_vpu_val_to_volt(u32 val);
char *qogirn6lite_vpuenc_val_to_freq(u32 val);
char *qogirn6lite_vpudec_val_to_freq(u32 val);
char *qogirn6lite_vpu_mtx_val_to_freq(u32 val);
#endif
