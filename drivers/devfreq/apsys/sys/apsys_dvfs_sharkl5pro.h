// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __APSYS_REG_SHARKL5PRO_H__
#define __APSYS_REG_SHARKL5PRO_H__

struct apsys_dvfs_reg {
	u32 ap_dvfs_hold_ctrl;
	u32 ap_dvfs_wait_window_cfg;
	u32 ap_dfs_en_ctrl;
	u32 ap_sw_trig_ctrl;
	u32 ap_min_voltage_cfg;
	u32 reserved_0x0014_0x0030[8];
	u32 ap_sw_dvfs_ctrl;
	u32 ap_freq_update_bypass;
	u32 cgm_ap_dvfs_clk_gate_ctrl;
	u32 ap_dvfs_voltage_dbg;
	u32 reserved_0x0044_0x0048[2];
	u32 ap_dvfs_cgm_cfg_dbg;
	u32 ap_dvfs_state_dbg;
	u32 vdsp_index0_map;
	u32 vdsp_index1_map;
	u32 vdsp_index2_map;
	u32 vdsp_index3_map;
	u32 vdsp_index4_map;
	u32 vdsp_index5_map;
	u32 vdsp_index6_map;
	u32 vdsp_index7_map;
	u32 vsp_index0_map;
	u32 vsp_index1_map;
	u32 vsp_index2_map;
	u32 vsp_index3_map;
	u32 vsp_index4_map;
	u32 vsp_index5_map;
	u32 vsp_index6_map;
	u32 vsp_index7_map;
	u32 dispc_index0_map;
	u32 dispc_index1_map;
	u32 dispc_index2_map;
	u32 reserved_0x00a0_0x00fc[24];
	u32 dispc_index3_map;
	u32 dispc_index4_map;
	u32 dispc_index5_map;
	u32 dispc_index6_map;
	u32 dispc_index7_map;
	u32 vdsp_dvfs_index_cfg;
	u32 vdsp_dvfs_index_idle_cfg;
	u32 vsp_dvfs_index_cfg;
	u32 vsp_dvfs_index_idle_cfg;
	u32 dispc_dvfs_index_cfg;
	u32 dispc_dvfs_index_idle_cfg;
	u32 ap_freq_upd_state;
	u32 ap_gfree_wait_delay_cfg;
	u32 ap_freq_upd_type_cfg;
	u32 ap_dvfs_reserved_reg_cfg0;
	u32 ap_dvfs_reserved_reg_cfg1;
	u32 ap_dvfs_reserved_reg_cfg2;
	u32 ap_dvfs_reserved_reg_cfg3;
};

char *sharkl5pro_apsys_val_to_volt(u32 val);
char *sharkl5pro_dpu_val_to_freq(u32 val);
char *sharkl5pro_vsp_val_to_freq(u32 val);
char *sharkl5pro_vdsp_val_to_freq(u32 val);
#endif
