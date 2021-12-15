/* Copyright (c) 2019, 2021, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#if !defined(_GSI_HWIO_DEF_H_)
#define _GSI_HWIO_DEF_H_

/* *****************************************************************************
 *
 * HWIO register definitions
 *
 * *****************************************************************************
 */
struct gsi_hwio_def_gsi_cfg_s {
	u32	gsi_enable : 1;
	u32	mcs_enable : 1;
	u32	double_mcs_clk_freq : 1;
	u32	uc_is_mcs : 1;
	u32	gsi_pwr_clps : 1;
	u32	bp_mtrix_disable : 1;
	u32	reserved0 : 26;
};
union gsi_hwio_def_gsi_cfg_u {
	struct gsi_hwio_def_gsi_cfg_s	def;
	u32			value;
};
struct gsi_hwio_def_gsi_ree_cfg_s {
	u32	move_to_esc_clr_mode_trsh : 1;
	u32	reserved0 : 7;
	u32	max_burst_size : 8;
	u32	reserved1 : 16;
};
union gsi_hwio_def_gsi_ree_cfg_u {
	struct gsi_hwio_def_gsi_ree_cfg_s	def;
	u32				value;
};
struct gsi_hwio_def_gsi_manager_ee_qos_n_s {
	u32	ee_prio : 2;
	u32	reserved0 : 6;
	u32	max_ch_alloc : 5;
	u32	reserved1 : 3;
	u32	max_ev_alloc : 5;
	u32	reserved2 : 11;
};
union gsi_hwio_def_gsi_manager_ee_qos_n_u {
	struct gsi_hwio_def_gsi_manager_ee_qos_n_s	def;
	u32					value;
};
struct gsi_hwio_def_gsi_shram_n_s {
	u32 shram : 32;
};
union gsi_hwio_def_gsi_shram_n_u {
	struct gsi_hwio_def_gsi_shram_n_s	def;
	u32				value;
};
struct gsi_hwio_def_gsi_test_bus_sel_s {
	u32	gsi_testbus_sel : 8;
	u32	reserved0 : 8;
	u32	gsi_hw_events_sel : 4;
	u32	reserved1 : 12;
};
union gsi_hwio_def_gsi_test_bus_sel_u {
	struct gsi_hwio_def_gsi_test_bus_sel_s	def;
	u32				value;
};
struct gsi_hwio_def_gsi_test_bus_reg_s {
	u32 gsi_testbus_reg : 32;
};
union gsi_hwio_def_gsi_test_bus_reg_u {
	struct gsi_hwio_def_gsi_test_bus_reg_s	def;
	u32				value;
};
struct gsi_hwio_def_gsi_debug_countern_s {
	u32	counter_value : 16;
	u32	reserved0 : 16;
};
union gsi_hwio_def_gsi_debug_countern_u {
	struct gsi_hwio_def_gsi_debug_countern_s	def;
	u32					value;
};
struct gsi_hwio_def_gsi_debug_qsb_log_last_misc_idn_s {
	u32	addr_20_0 : 21;
	u32	write : 1;
	u32	tid : 5;
	u32	mid : 5;
};
union gsi_hwio_def_gsi_debug_qsb_log_last_misc_idn_u {
	struct gsi_hwio_def_gsi_debug_qsb_log_last_misc_idn_s	def;
	u32						value;
};
struct gsi_hwio_def_gsi_debug_sw_rf_n_read_s {
	u32 rf_reg : 32;
};
union gsi_hwio_def_gsi_debug_sw_rf_n_read_u {
	struct gsi_hwio_def_gsi_debug_sw_rf_n_read_s	def;
	u32					value;
};
struct gsi_hwio_def_gsi_map_ee_n_ch_k_vp_table_s {
	u32	phy_ch : 5;
	u32	valid : 1;
	u32	reserved0 : 26;
};
union gsi_hwio_def_gsi_map_ee_n_ch_k_vp_table_u {
	struct gsi_hwio_def_gsi_map_ee_n_ch_k_vp_table_s	def;
	u32							value;
};
struct gsi_hwio_def_gsi_debug_ee_n_ev_k_vp_table_s {
	u32	phy_ev_ch : 5;
	u32	valid : 1;
	u32	reserved0 : 26;
};
union gsi_hwio_def_gsi_debug_ee_n_ev_k_vp_table_u {
	struct gsi_hwio_def_gsi_debug_ee_n_ev_k_vp_table_s	def;
	u32						value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_0_s {
	u32	chtype_protocol : 3;
	u32	chtype_dir : 1;
	u32	ee : 4;
	u32	chid : 5;
	u32	reserved0 : 1;
	u32	erindex : 5;
	u32	reserved1 : 1;
	u32	chstate : 4;
	u32	element_size : 8;
};
union gsi_hwio_def_ee_n_gsi_ch_k_cntxt_0_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_0_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_1_s {
	u32	r_length : 16;
	u32	reserved0 : 16;
};
union gsi_hwio_def_ee_n_gsi_ch_k_cntxt_1_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_1_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_2_s {
	u32 r_base_addr_lsbs : 32;
};
union gsi_hwio_def_ee_n_gsi_ch_k_cntxt_2_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_2_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_3_s {
	u32 r_base_addr_msbs : 32;
};
union gsi_hwio_def_ee_n_gsi_ch_k_cntxt_3_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_3_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_4_s {
	u32 read_ptr_lsb : 32;
};
union gsi_hwio_def_ee_n_gsi_ch_k_cntxt_4_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_4_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_5_s {
	u32 read_ptr_msb : 32;
};
union gsi_hwio_def_ee_n_gsi_ch_k_cntxt_5_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_5_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_6_s {
	u32 write_ptr_lsb : 32;
};
union gsi_hwio_def_ee_n_gsi_ch_k_cntxt_6_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_6_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_7_s {
	u32 write_ptr_msb : 32;
};
union gsi_hwio_def_ee_n_gsi_ch_k_cntxt_7_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_cntxt_7_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_re_fetch_read_ptr_s {
	u32	read_ptr : 16;
	u32	reserved0 : 16;
};
union gsi_hwio_def_ee_n_gsi_ch_k_re_fetch_read_ptr_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_re_fetch_read_ptr_s	def;
	u32						value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_re_fetch_write_ptr_s {
	u32	re_intr_db : 16;
	u32	reserved0 : 16;
};
union gsi_hwio_def_ee_n_gsi_ch_k_re_fetch_write_ptr_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_re_fetch_write_ptr_s	def;
	u32						value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_qos_s {
	u32	wrr_weight : 4;
	u32	reserved0 : 4;
	u32	max_prefetch : 1;
	u32	use_db_eng : 1;
	u32	use_escape_buf_only : 1;
	u32	reserved1 : 21;
};
union gsi_hwio_def_ee_n_gsi_ch_k_qos_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_qos_s def;
	u32				value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_scratch_0_s {
	u32 scratch : 32;
};
union gsi_hwio_def_ee_n_gsi_ch_k_scratch_0_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_scratch_0_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_scratch_1_s {
	u32 scratch : 32;
};
union gsi_hwio_def_ee_n_gsi_ch_k_scratch_1_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_scratch_1_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_scratch_2_s {
	u32 scratch : 32;
};
union gsi_hwio_def_ee_n_gsi_ch_k_scratch_2_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_scratch_2_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_gsi_ch_k_scratch_3_s {
	u32 scratch : 32;
};
union gsi_hwio_def_ee_n_gsi_ch_k_scratch_3_u {
	struct gsi_hwio_def_ee_n_gsi_ch_k_scratch_3_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_0_s {
	u32	chtype : 4;
	u32	ee : 4;
	u32	evchid : 8;
	u32	intype : 1;
	u32	reserved0 : 3;
	u32	chstate : 4;
	u32	element_size : 8;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_0_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_0_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_1_s {
	u32	r_length : 16;
	u32	reserved0 : 16;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_1_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_1_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_2_s {
	u32 r_base_addr_lsbs : 32;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_2_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_2_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_3_s {
	u32 r_base_addr_msbs : 32;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_3_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_3_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_4_s {
	u32 read_ptr_lsb : 32;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_4_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_4_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_5_s {
	u32 read_ptr_msb : 32;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_5_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_5_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_6_s {
	u32 write_ptr_lsb : 32;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_6_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_6_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_7_s {
	u32 write_ptr_msb : 32;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_7_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_7_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_8_s {
	u32	int_modt : 16;
	u32	int_modc : 8;
	u32	int_mod_cnt : 8;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_8_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_8_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_9_s {
	u32 intvec : 32;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_9_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_9_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_10_s {
	u32 msi_addr_lsb : 32;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_10_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_10_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_11_s {
	u32 msi_addr_msb : 32;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_11_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_11_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_12_s {
	u32 rp_update_addr_lsb : 32;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_12_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_12_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_13_s {
	u32 rp_update_addr_msb : 32;
};
union gsi_hwio_def_ee_n_ev_ch_k_cntxt_13_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_cntxt_13_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_scratch_0_s {
	u32 scratch : 32;
};
union gsi_hwio_def_ee_n_ev_ch_k_scratch_0_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_scratch_0_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_ev_ch_k_scratch_1_s {
	u32 scratch : 32;
};
union gsi_hwio_def_ee_n_ev_ch_k_scratch_1_u {
	struct gsi_hwio_def_ee_n_ev_ch_k_scratch_1_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_gsi_status_s {
	u32	enabled : 1;
	u32	reserved0 : 31;
};
union gsi_hwio_def_ee_n_gsi_status_u {
	struct gsi_hwio_def_ee_n_gsi_status_s	def;
	u32				value;
};
struct gsi_hwio_def_ee_n_cntxt_type_irq_s {
	u32	ch_ctrl : 1;
	u32	ev_ctrl : 1;
	u32	glob_ee : 1;
	u32	ieob : 1;
	u32	inter_ee_ch_ctrl : 1;
	u32	inter_ee_ev_ctrl : 1;
	u32	general : 1;
	u32	reserved0 : 25;
};
union gsi_hwio_def_ee_n_cntxt_type_irq_u {
	struct gsi_hwio_def_ee_n_cntxt_type_irq_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_cntxt_type_irq_msk_s {
	u32	ch_ctrl : 1;
	u32	ev_ctrl : 1;
	u32	glob_ee : 1;
	u32	ieob : 1;
	u32	inter_ee_ch_ctrl : 1;
	u32	inter_ee_ev_ctrl : 1;
	u32	general : 1;
	u32	reserved0 : 25;
};
union gsi_hwio_def_ee_n_cntxt_type_irq_msk_u {
	struct gsi_hwio_def_ee_n_cntxt_type_irq_msk_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_cntxt_src_gsi_ch_irq_s {
	u32 gsi_ch_bit_map : 32;
};
union gsi_hwio_def_ee_n_cntxt_src_gsi_ch_irq_u {
	struct gsi_hwio_def_ee_n_cntxt_src_gsi_ch_irq_s def;
	u32					value;
};
struct gsi_hwio_def_ee_n_cntxt_src_ev_ch_irq_s {
	u32 ev_ch_bit_map : 32;
};
union gsi_hwio_def_ee_n_cntxt_src_ev_ch_irq_u {
	struct gsi_hwio_def_ee_n_cntxt_src_ev_ch_irq_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_cntxt_src_gsi_ch_irq_msk_s {
	u32	gsi_ch_bit_map_msk : 17;
	u32	reserved0 : 15;
};
union gsi_hwio_def_ee_n_cntxt_src_gsi_ch_irq_msk_u {
	struct gsi_hwio_def_ee_n_cntxt_src_gsi_ch_irq_msk_s	def;
	u32						value;
};
struct gsi_hwio_def_ee_n_cntxt_src_ev_ch_irq_msk_s {
	u32	ev_ch_bit_map_msk : 12;
	u32	reserved0 : 20;
};
union gsi_hwio_def_ee_n_cntxt_src_ev_ch_irq_msk_u {
	struct gsi_hwio_def_ee_n_cntxt_src_ev_ch_irq_msk_s	def;
	u32						value;
};
struct gsi_hwio_def_ee_n_cntxt_src_gsi_ch_irq_clr_s {
	u32 gsi_ch_bit_map : 32;
};
union gsi_hwio_def_ee_n_cntxt_src_gsi_ch_irq_clr_u {
	struct gsi_hwio_def_ee_n_cntxt_src_gsi_ch_irq_clr_s	def;
	u32						value;
};
struct gsi_hwio_def_ee_n_cntxt_src_ev_ch_irq_clr_s {
	u32 ev_ch_bit_map : 32;
};
union gsi_hwio_def_ee_n_cntxt_src_ev_ch_irq_clr_u {
	struct gsi_hwio_def_ee_n_cntxt_src_ev_ch_irq_clr_s	def;
	u32						value;
};
struct gsi_hwio_def_ee_n_cntxt_src_ieob_irq_s {
	u32 ev_ch_bit_map : 32;
};
union gsi_hwio_def_ee_n_cntxt_src_ieob_irq_u {
	struct gsi_hwio_def_ee_n_cntxt_src_ieob_irq_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_cntxt_src_ieob_irq_msk_s {
	u32	ev_ch_bit_map_msk : 12;
	u32	reserved0 : 20;
};
union gsi_hwio_def_ee_n_cntxt_src_ieob_irq_msk_u {
	struct gsi_hwio_def_ee_n_cntxt_src_ieob_irq_msk_s	def;
	u32						value;
};
struct gsi_hwio_def_ee_n_cntxt_src_ieob_irq_clr_s {
	u32 ev_ch_bit_map : 32;
};
union gsi_hwio_def_ee_n_cntxt_src_ieob_irq_clr_u {
	struct gsi_hwio_def_ee_n_cntxt_src_ieob_irq_clr_s	def;
	u32						value;
};
struct gsi_hwio_def_ee_n_cntxt_glob_irq_stts_s {
	u32	error_int : 1;
	u32	gp_int1 : 1;
	u32	gp_int2 : 1;
	u32	gp_int3 : 1;
	u32	reserved0 : 28;
};
union gsi_hwio_def_ee_n_cntxt_glob_irq_stts_u {
	struct gsi_hwio_def_ee_n_cntxt_glob_irq_stts_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_cntxt_gsi_irq_stts_s {
	u32	gsi_break_point : 1;
	u32	gsi_bus_error : 1;
	u32	gsi_cmd_fifo_ovrflow : 1;
	u32	gsi_mcs_stack_ovrflow : 1;
	u32	reserved0 : 28;
};
union gsi_hwio_def_ee_n_cntxt_gsi_irq_stts_u {
	struct gsi_hwio_def_ee_n_cntxt_gsi_irq_stts_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_cntxt_intset_s {
	u32	intype : 1;
	u32	reserved0 : 31;
};
union gsi_hwio_def_ee_n_cntxt_intset_u {
	struct gsi_hwio_def_ee_n_cntxt_intset_s def;
	u32					value;
};
struct gsi_hwio_def_ee_n_cntxt_msi_base_lsb_s {
	u32 msi_addr_lsb : 32;
};
union gsi_hwio_def_ee_n_cntxt_msi_base_lsb_u {
	struct gsi_hwio_def_ee_n_cntxt_msi_base_lsb_s	def;
	u32						value;
};
struct gsi_hwio_def_ee_n_cntxt_msi_base_msb_s {
	u32 msi_addr_msb : 32;
};
union gsi_hwio_def_ee_n_cntxt_msi_base_msb_u {
	struct gsi_hwio_def_ee_n_cntxt_msi_base_msb_s	def;
	u32						value;
};
struct gsi_hwio_def_ee_n_error_log_s {
	u32 error_log : 32;
};
union gsi_hwio_def_ee_n_error_log_u {
	struct gsi_hwio_def_ee_n_error_log_s	def;
	u32				value;
};
struct gsi_hwio_def_ee_n_error_log_clr_s {
	u32 error_log_clr : 32;
};
union gsi_hwio_def_ee_n_error_log_clr_u {
	struct gsi_hwio_def_ee_n_error_log_clr_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_cntxt_scratch_0_s {
	u32 scratch : 32;
};
union gsi_hwio_def_ee_n_cntxt_scratch_0_u {
	struct gsi_hwio_def_ee_n_cntxt_scratch_0_s	def;
	u32					value;
};
struct gsi_hwio_def_ee_n_cntxt_scratch_1_s {
	u32 scratch : 32;
};
union gsi_hwio_def_ee_n_cntxt_scratch_1_u {
	struct gsi_hwio_def_ee_n_cntxt_scratch_1_s	def;
	u32					value;
};
#endif
