// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __APSYS_REG_ROC1_H__
#define __APSYS_REG_ROC1_H__

#include <linux/kernel.h>
#include <linux/types.h>

struct apsys_dvfs_reg {
	u32 ap_dvfs_hold_ctrl;
	u32 ap_dvfs_wait_window_cfg;
	u32 ap_dfs_en_ctrl;
	u32 ap_sw_trig_ctrl;
	u32 ap_min_voltage_cfg;
	u32 ap_dfs_sw_trig_cfg;
	u32 reserved_0x0018_0x0030[7];
	u32 ap_sw_dvfs_ctrl;
	u32 ap_freq_update_bypass;
	u32 cgm_ap_dvfs_clk_gate_ctrl;
	u32 ap_dvfs_voltage_dbg;
	u32 reserved_0x0044_0x0048[2];
	u32 ap_dvfs_cgm_cfg_dbg;
	u32 ap_dvfs_state_dbg;
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
	u32 vsp_dvfs_index_cfg;
	u32 vsp_dvfs_index_idle_cfg;
	u32 dispc_dvfs_index_cfg;
	u32 dispc_dvfs_index_idle_cfg;
	u32 ap_freq_upd_state;
	u32 ap_gfree_wait_delay_cfg;
	u32 ap_freq_upd_type_cfg;
	u32 reserved_0x0138_0x013c[2];
	u32 ap_dfs_idle_disable_cfg;
	u32 reserved_0x0144_0x014c[3];
	u32 ap_dvfs_reserved_reg_cfg0;
	u32 ap_dvfs_reserved_reg_cfg1;
	u32 ap_dvfs_reserved_reg_cfg2;
	u32 ap_dvfs_reserved_reg_cfg3;
};

char *roc1_apsys_val_to_volt(u32 val);
char *roc1_dpu_val_to_freq(u32 val);
char *roc1_vdsp_val_to_freq(u32 val);
char *roc1_vsp_val_to_freq(u32 val);

#endif
