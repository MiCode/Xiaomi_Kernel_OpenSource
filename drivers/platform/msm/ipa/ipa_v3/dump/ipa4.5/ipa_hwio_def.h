/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */
#if !defined(_IPA_HWIO_DEF_H_)
#define _IPA_HWIO_DEF_H_
struct ipa_hwio_def_ipa_gsi_top_gsi_cfg_s {
	u32	gsi_enable : 1;
	u32	mcs_enable : 1;
	u32	double_mcs_clk_freq : 1;
	u32	uc_is_mcs : 1;
	u32	gsi_pwr_clps : 1;
	u32	bp_mtrix_disable : 1;
	u32	reserved0 : 2;
	u32	sleep_clk_div : 4;
	u32	reserved1 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_cfg_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_cfg_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_ree_cfg_s {
	u32	move_to_esc_clr_mode_trsh : 1;
	u32	channel_empty_int_enable : 1;
	u32	reserved0 : 6;
	u32	max_burst_size : 8;
	u32	reserved1 : 16;
};
union ipa_hwio_def_ipa_gsi_top_gsi_ree_cfg_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_ree_cfg_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_manager_ee_qos_n_s {
	u32	ee_prio : 2;
	u32	reserved0 : 6;
	u32	max_ch_alloc : 5;
	u32	reserved1 : 3;
	u32	max_ev_alloc : 5;
	u32	reserved2 : 11;
};
union ipa_hwio_def_ipa_gsi_top_gsi_manager_ee_qos_n_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_manager_ee_qos_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_ch_cntxt_base_addr_s {
	u32	shram_ptr : 16;
	u32	reserved0 : 16;
};
union ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_ch_cntxt_base_addr_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_ch_cntxt_base_addr_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_ev_cntxt_base_addr_s {
	u32	shram_ptr : 16;
	u32	reserved0 : 16;
};
union ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_ev_cntxt_base_addr_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_ev_cntxt_base_addr_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_re_storage_base_addr_s {
	u32	shram_ptr : 16;
	u32	reserved0 : 16;
};
union ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_re_storage_base_addr_u {
	struct
	ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_re_storage_base_addr_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_re_esc_buf_base_addr_s {
	u32	shram_ptr : 16;
	u32	reserved0 : 16;
};
union ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_re_esc_buf_base_addr_u {
	struct
	ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_re_esc_buf_base_addr_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_ee_scrach_base_addr_s {
	u32	shram_ptr : 16;
	u32	reserved0 : 16;
};
union ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_ee_scrach_base_addr_u {
	struct
	ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_ee_scrach_base_addr_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_func_stack_base_addr_s {
	u32	shram_ptr : 16;
	u32	reserved0 : 16;
};
union ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_func_stack_base_addr_u {
	struct
	ipa_hwio_def_ipa_gsi_top_gsi_shram_ptr_func_stack_base_addr_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_cmd_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_cmd_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_cmd_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ee_generic_cmd_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ee_generic_cmd_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ee_generic_cmd_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_db_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_db_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_db_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ev_db_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ev_db_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ev_db_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_new_re_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_new_re_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_new_re_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_dis_comp_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_dis_comp_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_dis_comp_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_empty_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_empty_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_ch_empty_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_event_gen_comp_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_event_gen_comp_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_event_gen_comp_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_timer_expired_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_timer_expired_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_timer_expired_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_write_eng_comp_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_write_eng_comp_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_write_eng_comp_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_read_eng_comp_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_read_eng_comp_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_read_eng_comp_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_uc_gp_int_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_uc_gp_int_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_uc_gp_int_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_int_mod_stopped_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_int_mod_stopped_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_iram_ptr_int_mod_stopped_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_inst_ram_n_s {
	u32 inst_byte_0 : 8;
	u32 inst_byte_1 : 8;
	u32 inst_byte_2 : 8;
	u32 inst_byte_3 : 8;
};
union ipa_hwio_def_ipa_gsi_top_gsi_inst_ram_n_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_inst_ram_n_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_shram_n_s {
	u32 shram : 32;
};
union ipa_hwio_def_ipa_gsi_top_gsi_shram_n_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_shram_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_map_ee_n_ch_k_vp_table_s {
	u32	phy_ch : 5;
	u32	valid : 1;
	u32	reserved0 : 26;
};
union ipa_hwio_def_ipa_gsi_top_gsi_map_ee_n_ch_k_vp_table_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_map_ee_n_ch_k_vp_table_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_test_bus_sel_s {
	u32	gsi_testbus_sel : 8;
	u32	reserved0 : 8;
	u32	gsi_hw_events_sel : 4;
	u32	reserved1 : 12;
};
union ipa_hwio_def_ipa_gsi_top_gsi_test_bus_sel_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_test_bus_sel_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_test_bus_reg_s {
	u32 gsi_testbus_reg : 32;
};
union ipa_hwio_def_ipa_gsi_top_gsi_test_bus_reg_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_test_bus_reg_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_debug_busy_reg_s {
	u32	csr_busy : 1;
	u32	ree_busy : 1;
	u32	mcs_busy : 1;
	u32	timer_busy : 1;
	u32	rd_wr_busy : 1;
	u32	ev_eng_busy : 1;
	u32	int_eng_busy : 1;
	u32	ree_pwr_clps_busy : 1;
	u32	db_eng_busy : 1;
	u32	dbg_cnt_busy : 1;
	u32	uc_busy : 1;
	u32	ic_busy : 1;
	u32	sdma_busy : 1;
	u32	reserved0 : 19;
};
union ipa_hwio_def_ipa_gsi_top_gsi_debug_busy_reg_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_busy_reg_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_debug_event_pending_s {
	u32 chid_bit_map : 32;
};
union ipa_hwio_def_ipa_gsi_top_gsi_debug_event_pending_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_event_pending_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_debug_timer_pending_s {
	u32 chid_bit_map : 32;
};
union ipa_hwio_def_ipa_gsi_top_gsi_debug_timer_pending_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_timer_pending_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_debug_rd_wr_pending_s {
	u32 chid_bit_map : 32;
};
union ipa_hwio_def_ipa_gsi_top_gsi_debug_rd_wr_pending_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_rd_wr_pending_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_debug_countern_s {
	u32	counter_value : 16;
	u32	reserved0 : 16;
};
union ipa_hwio_def_ipa_gsi_top_gsi_debug_countern_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_countern_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_debug_pc_from_sw_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_debug_pc_from_sw_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_pc_from_sw_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_debug_sw_stall_s {
	u32	mcs_stall : 1;
	u32	reserved0 : 31;
};
union ipa_hwio_def_ipa_gsi_top_gsi_debug_sw_stall_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_sw_stall_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_debug_pc_for_debug_s {
	u32	iram_ptr : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_gsi_top_gsi_debug_pc_for_debug_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_pc_for_debug_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_debug_qsb_log_err_trns_id_s {
	u32	err_write : 1;
	u32	reserved0 : 7;
	u32	err_tid : 8;
	u32	err_mid : 8;
	u32	err_saved : 1;
	u32	reserved1 : 7;
};
union ipa_hwio_def_ipa_gsi_top_gsi_debug_qsb_log_err_trns_id_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_qsb_log_err_trns_id_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_debug_sw_rf_n_read_s {
	u32 rf_reg : 32;
};
union ipa_hwio_def_ipa_gsi_top_gsi_debug_sw_rf_n_read_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_sw_rf_n_read_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_gsi_debug_ee_n_ev_k_vp_table_s {
	u32	phy_ev_ch : 5;
	u32	valid : 1;
	u32	reserved0 : 26;
};
union ipa_hwio_def_ipa_gsi_top_gsi_debug_ee_n_ev_k_vp_table_u {
	struct ipa_hwio_def_ipa_gsi_top_gsi_debug_ee_n_ev_k_vp_table_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_0_s {
	u32	chtype_protocol : 3;
	u32	chtype_dir : 1;
	u32	ee : 4;
	u32	chid : 5;
	u32	chtype_protocol_msb : 1;
	u32	erindex : 5;
	u32	reserved0 : 1;
	u32	chstate : 4;
	u32	element_size : 8;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_0_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_0_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_1_s {
	u32	r_length : 16;
	u32	reserved0 : 16;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_1_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_1_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_2_s {
	u32 r_base_addr_lsbs : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_2_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_2_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_3_s {
	u32 r_base_addr_msbs : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_3_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_3_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_4_s {
	u32 read_ptr_lsb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_4_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_4_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_5_s {
	u32 read_ptr_msb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_5_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_5_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_6_s {
	u32 write_ptr_lsb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_6_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_6_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_7_s {
	u32 write_ptr_msb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_7_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_cntxt_7_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_re_fetch_read_ptr_s {
	u32	read_ptr : 16;
	u32	reserved0 : 16;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_re_fetch_read_ptr_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_re_fetch_read_ptr_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_re_fetch_write_ptr_s {
	u32	re_intr_db : 16;
	u32	reserved0 : 16;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_re_fetch_write_ptr_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_re_fetch_write_ptr_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_qos_s {
	u32	wrr_weight : 4;
	u32	reserved0 : 4;
	u32	max_prefetch : 1;
	u32	use_db_eng : 1;
	u32	prefetch_mode : 4;
	u32	reserved1 : 2;
	u32	empty_lvl_thrshold : 8;
	u32	reserved2 : 8;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_qos_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_qos_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_scratch_0_s {
	u32 scratch : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_scratch_0_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_scratch_0_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_scratch_1_s {
	u32 scratch : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_scratch_1_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_scratch_1_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_scratch_2_s {
	u32 scratch : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_scratch_2_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_scratch_2_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_scratch_3_s {
	u32 scratch : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_scratch_3_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_ch_k_scratch_3_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_0_s {
	u32	chtype : 4;
	u32	ee : 4;
	u32	evchid : 8;
	u32	intype : 1;
	u32	reserved0 : 3;
	u32	chstate : 4;
	u32	element_size : 8;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_0_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_0_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_1_s {
	u32	r_length : 16;
	u32	reserved0 : 16;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_1_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_1_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_2_s {
	u32 r_base_addr_lsbs : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_2_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_2_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_3_s {
	u32 r_base_addr_msbs : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_3_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_3_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_4_s {
	u32 read_ptr_lsb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_4_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_4_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_5_s {
	u32 read_ptr_msb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_5_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_5_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_6_s {
	u32 write_ptr_lsb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_6_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_6_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_7_s {
	u32 write_ptr_msb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_7_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_7_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_8_s {
	u32	int_modt : 16;
	u32	int_modc : 8;
	u32	int_mod_cnt : 8;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_8_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_8_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_9_s {
	u32 intvec : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_9_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_9_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_10_s {
	u32 msi_addr_lsb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_10_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_10_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_11_s {
	u32 msi_addr_msb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_11_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_11_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_12_s {
	u32 rp_update_addr_lsb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_12_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_12_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_13_s {
	u32 rp_update_addr_msb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_13_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_cntxt_13_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_scratch_0_s {
	u32 scratch : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_scratch_0_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_scratch_0_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_scratch_1_s {
	u32 scratch : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_scratch_1_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_ev_ch_k_scratch_1_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_status_s {
	u32	enabled : 1;
	u32	reserved0 : 31;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_gsi_status_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_gsi_status_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_type_irq_s {
	u32	ch_ctrl : 1;
	u32	ev_ctrl : 1;
	u32	glob_ee : 1;
	u32	ieob : 1;
	u32	inter_ee_ch_ctrl : 1;
	u32	inter_ee_ev_ctrl : 1;
	u32	general : 1;
	u32	reserved0 : 25;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_type_irq_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_type_irq_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_type_irq_msk_s {
	u32	ch_ctrl : 1;
	u32	ev_ctrl : 1;
	u32	glob_ee : 1;
	u32	ieob : 1;
	u32	inter_ee_ch_ctrl : 1;
	u32	inter_ee_ev_ctrl : 1;
	u32	general : 1;
	u32	reserved0 : 25;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_type_irq_msk_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_type_irq_msk_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_gsi_ch_irq_s {
	u32 gsi_ch_bit_map : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_gsi_ch_irq_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_gsi_ch_irq_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ev_ch_irq_s {
	u32 ev_ch_bit_map : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ev_ch_irq_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ev_ch_irq_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_gsi_ch_irq_msk_s {
	u32	gsi_ch_bit_map_msk : 23;
	u32	reserved0 : 9;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_gsi_ch_irq_msk_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_gsi_ch_irq_msk_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ev_ch_irq_msk_s {
	u32	ev_ch_bit_map_msk : 20;
	u32	reserved0 : 12;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ev_ch_irq_msk_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ev_ch_irq_msk_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_gsi_ch_irq_clr_s {
	u32 gsi_ch_bit_map : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_gsi_ch_irq_clr_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_gsi_ch_irq_clr_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ev_ch_irq_clr_s {
	u32 ev_ch_bit_map : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ev_ch_irq_clr_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ev_ch_irq_clr_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ieob_irq_s {
	u32 ev_ch_bit_map : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ieob_irq_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ieob_irq_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ieob_irq_msk_s {
	u32	ev_ch_bit_map_msk : 20;
	u32	reserved0 : 12;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ieob_irq_msk_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ieob_irq_msk_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ieob_irq_clr_s {
	u32 ev_ch_bit_map : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ieob_irq_clr_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_src_ieob_irq_clr_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_glob_irq_stts_s {
	u32	error_int : 1;
	u32	gp_int1 : 1;
	u32	gp_int2 : 1;
	u32	gp_int3 : 1;
	u32	reserved0 : 28;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_glob_irq_stts_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_glob_irq_stts_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_gsi_irq_stts_s {
	u32	gsi_break_point : 1;
	u32	gsi_bus_error : 1;
	u32	gsi_cmd_fifo_ovrflow : 1;
	u32	gsi_mcs_stack_ovrflow : 1;
	u32	reserved0 : 28;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_gsi_irq_stts_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_gsi_irq_stts_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_intset_s {
	u32	intype : 1;
	u32	reserved0 : 31;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_intset_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_intset_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_msi_base_lsb_s {
	u32 msi_addr_lsb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_msi_base_lsb_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_msi_base_lsb_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_msi_base_msb_s {
	u32 msi_addr_msb : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_msi_base_msb_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_msi_base_msb_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_error_log_s {
	u32 error_log : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_error_log_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_error_log_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_error_log_clr_s {
	u32 error_log_clr : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_error_log_clr_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_error_log_clr_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_scratch_0_s {
	u32 scratch : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_scratch_0_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_scratch_0_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_scratch_1_s {
	u32 scratch : 32;
};
union ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_scratch_1_u {
	struct ipa_hwio_def_ipa_gsi_top_ee_n_cntxt_scratch_1_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_comp_hw_version_s {
	u32	step : 16;
	u32	minor : 12;
	u32	major : 4;
};
union ipa_hwio_def_ipa_comp_hw_version_u {
	struct ipa_hwio_def_ipa_comp_hw_version_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_comp_cfg_s {
	u32	reserved0 : 1;
	u32	gsi_snoc_bypass_dis : 1;
	u32	gen_qmb_0_snoc_bypass_dis : 1;
	u32	gen_qmb_1_snoc_bypass_dis : 1;
	u32	reserved1 : 1;
	u32	ipa_qmb_select_by_address_cons_en : 1;
	u32	ipa_qmb_select_by_address_prod_en : 1;
	u32	gsi_multi_inorder_rd_dis : 1;
	u32	gsi_multi_inorder_wr_dis : 1;
	u32	gen_qmb_0_multi_inorder_rd_dis : 1;
	u32	gen_qmb_1_multi_inorder_rd_dis : 1;
	u32	gen_qmb_0_multi_inorder_wr_dis : 1;
	u32	gen_qmb_1_multi_inorder_wr_dis : 1;
	u32	gen_qmb_0_snoc_cnoc_loop_protection_disable : 1;
	u32	gsi_snoc_cnoc_loop_protection_disable : 1;
	u32	gsi_multi_axi_masters_dis : 1;
	u32	ipa_qmb_select_by_address_global_en : 1;
	u32	ipa_atomic_fetcher_arb_lock_dis : 4;
	u32	ipa_full_flush_wait_rsc_closure_en : 1;
	u32	reserved2 : 10;
};
union ipa_hwio_def_ipa_comp_cfg_u {
	struct ipa_hwio_def_ipa_comp_cfg_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_route_s {
	u32	route_dis : 1;
	u32	route_def_pipe : 5;
	u32	route_def_hdr_table : 1;
	u32	route_def_hdr_ofst : 10;
	u32	route_frag_def_pipe : 5;
	u32	reserved0 : 2;
	u32	route_def_retain_hdr : 1;
	u32	reserved1 : 7;
};
union ipa_hwio_def_ipa_route_u {
	struct ipa_hwio_def_ipa_route_s def;
	u32				value;
};
struct ipa_hwio_def_ipa_proc_iph_cfg_s {
	u32	iph_threshold : 2;
	u32	iph_pipelining_disable : 1;
	u32	reserved0 : 1;
	u32	status_from_iph_frst_always : 1;
	u32	iph_nat_blind_invalidate_tport_offset_disable : 1;
	u32	pipestage_overlap_disable : 1;
	u32	ftch_dcph_overlap_enable : 1;
	u32	iph_pkt_parser_protocol_stop_enable : 1;
	u32	iph_pkt_parser_protocol_stop_hop : 1;
	u32	iph_pkt_parser_protocol_stop_dest : 1;
	u32	iph_pkt_parser_ihl_to_2nd_frag_en : 1;
	u32	reserved1 : 4;
	u32	iph_pkt_parser_protocol_stop_value : 8;
	u32	d_dcph_multi_engine_disable : 1;
	u32	reserved2 : 7;
};
union ipa_hwio_def_ipa_proc_iph_cfg_u {
	struct ipa_hwio_def_ipa_proc_iph_cfg_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_dpl_timer_lsb_s {
	u32 tod_lsb : 32;
};
union ipa_hwio_def_ipa_dpl_timer_lsb_u {
	struct ipa_hwio_def_ipa_dpl_timer_lsb_s def;
	u32					value;
};
struct ipa_hwio_def_ipa_dpl_timer_msb_s {
	u32	tod_msb : 16;
	u32	reserved0 : 15;
	u32	timer_en : 1;
};
union ipa_hwio_def_ipa_dpl_timer_msb_u {
	struct ipa_hwio_def_ipa_dpl_timer_msb_s def;
	u32					value;
};
struct ipa_hwio_def_ipa_state_tx_wrapper_s {
	u32	tx0_idle : 1;
	u32	tx1_idle : 1;
	u32	ipa_prod_ackmngr_db_empty : 1;
	u32	ipa_prod_ackmngr_state_idle : 1;
	u32	ipa_prod_bresp_empty : 1;
	u32	ipa_prod_bresp_toggle_idle : 1;
	u32	ipa_mbim_pkt_fms_idle : 1;
	u32	mbim_direct_dma : 2;
	u32	trnseq_force_valid : 1;
	u32	pkt_drop_cnt_idle : 1;
	u32	nlo_direct_dma : 2;
	u32	coal_direct_dma : 2;
	u32	coal_slave_idle : 1;
	u32	coal_slave_ctx_idle : 1;
	u32	reserved0 : 8;
	u32	coal_slave_open_frame : 4;
	u32	reserved1 : 3;
};
union ipa_hwio_def_ipa_state_tx_wrapper_u {
	struct ipa_hwio_def_ipa_state_tx_wrapper_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_state_tx1_s {
	u32	flopped_arbit_type : 3;
	u32	arbit_type : 3;
	u32	pa_idle : 1;
	u32	pa_ctx_idle : 1;
	u32	pa_rst_idle : 1;
	u32	pa_pub_cnt_empty : 1;
	u32	tx_cmd_main_idle : 1;
	u32	tx_cmd_trnseq_idle : 1;
	u32	tx_cmd_snif_idle : 1;
	u32	tx_cmd_bresp_aloc_idle : 1;
	u32	tx_cmd_bresp_inj_idle : 1;
	u32	ar_idle : 1;
	u32	dmaw_idle : 1;
	u32	dmaw_last_outsd_idle : 1;
	u32	pf_idle : 1;
	u32	pf_empty : 1;
	u32	aligner_empty : 1;
	u32	holb_idle : 1;
	u32	holb_mask_idle : 1;
	u32	rsrcrel_idle : 1;
	u32	suspend_empty : 1;
	u32	cs_snif_idle : 1;
	u32	last_cmd_pipe : 5;
	u32	suspend_req_empty : 1;
};
union ipa_hwio_def_ipa_state_tx1_u {
	struct ipa_hwio_def_ipa_state_tx1_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_state_fetcher_s {
	u32	ipa_hps_ftch_state_idle : 1;
	u32	ipa_hps_ftch_alloc_state_idle : 1;
	u32	ipa_hps_ftch_pkt_state_idle : 1;
	u32	ipa_hps_ftch_imm_state_idle : 1;
	u32	ipa_hps_ftch_cmplt_state_idle : 1;
	u32	ipa_hps_dmar_state_idle : 7;
	u32	ipa_hps_dmar_slot_state_idle : 7;
	u32	ipa_hps_imm_cmd_exec_state_idle : 1;
	u32	reserved0 : 12;
};
union ipa_hwio_def_ipa_state_fetcher_u {
	struct ipa_hwio_def_ipa_state_fetcher_s def;
	u32					value;
};
struct ipa_hwio_def_ipa_state_fetcher_mask_0_s {
	u32	mask_queue_dmar_uses_queue : 8;
	u32	mask_queue_imm_exec : 8;
	u32	mask_queue_no_resources_context : 8;
	u32	mask_queue_no_resources_hps_dmar : 8;
};
union ipa_hwio_def_ipa_state_fetcher_mask_0_u {
	struct ipa_hwio_def_ipa_state_fetcher_mask_0_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_state_fetcher_mask_1_s {
	u32	mask_queue_no_resources_ack_entry : 8;
	u32	mask_queue_arb_lock : 8;
	u32	mask_queue_step_mode : 8;
	u32	mask_queue_no_space_dpl_fifo : 8;
};
union ipa_hwio_def_ipa_state_fetcher_mask_1_u {
	struct ipa_hwio_def_ipa_state_fetcher_mask_1_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_state_dpl_fifo_s {
	u32	pop_fsm_state : 3;
	u32	reserved0 : 29;
};
union ipa_hwio_def_ipa_state_dpl_fifo_u {
	struct ipa_hwio_def_ipa_state_dpl_fifo_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_state_coal_master_s {
	u32	vp_vld : 4;
	u32	main_fsm_state : 4;
	u32	find_open_fsm_state : 4;
	u32	hash_calc_fsm_state : 4;
	u32	check_fit_fsm_state : 4;
	u32	init_vp_fsm_state : 4;
	u32	lru_vp : 4;
	u32	vp_timer_expired : 4;
};
union ipa_hwio_def_ipa_state_coal_master_u {
	struct ipa_hwio_def_ipa_state_coal_master_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_state_dfetcher_s {
	u32	ipa_dps_ftch_pkt_state_idle : 1;
	u32	ipa_dps_ftch_cmplt_state_idle : 1;
	u32	reserved0 : 2;
	u32	ipa_dps_dmar_state_idle : 6;
	u32	reserved1 : 2;
	u32	ipa_dps_dmar_slot_state_idle : 6;
	u32	reserved2 : 14;
};
union ipa_hwio_def_ipa_state_dfetcher_u {
	struct ipa_hwio_def_ipa_state_dfetcher_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_state_acl_s {
	u32	ipa_hps_h_dcph_empty : 1;
	u32	ipa_hps_h_dcph_active : 1;
	u32	ipa_hps_pkt_parser_empty : 1;
	u32	ipa_hps_pkt_parser_active : 1;
	u32	ipa_hps_filter_nat_empty : 1;
	u32	ipa_hps_filter_nat_active : 1;
	u32	ipa_hps_router_empty : 1;
	u32	ipa_hps_router_active : 1;
	u32	ipa_hps_hdri_empty : 1;
	u32	ipa_hps_hdri_active : 1;
	u32	ipa_hps_ucp_empty : 1;
	u32	ipa_hps_ucp_active : 1;
	u32	ipa_hps_enqueuer_empty : 1;
	u32	ipa_hps_enqueuer_active : 1;
	u32	ipa_dps_d_dcph_empty : 1;
	u32	ipa_dps_d_dcph_active : 1;
	u32	reserved0 : 2;
	u32	ipa_dps_dispatcher_empty : 1;
	u32	ipa_dps_dispatcher_active : 1;
	u32	ipa_dps_d_dcph_2_empty : 1;
	u32	ipa_dps_d_dcph_2_active : 1;
	u32	ipa_hps_sequencer_idle : 1;
	u32	ipa_dps_sequencer_idle : 1;
	u32	ipa_dps_d_dcph_2nd_empty : 1;
	u32	ipa_dps_d_dcph_2nd_active : 1;
	u32	ipa_hps_coal_master_empty : 1;
	u32	ipa_hps_coal_master_active : 1;
	u32	reserved1 : 4;
};
union ipa_hwio_def_ipa_state_acl_u {
	struct ipa_hwio_def_ipa_state_acl_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_state_gsi_tlv_s {
	u32	ipa_gsi_toggle_fsm_idle : 1;
	u32	reserved0 : 31;
};
union ipa_hwio_def_ipa_state_gsi_tlv_u {
	struct ipa_hwio_def_ipa_state_gsi_tlv_s def;
	u32					value;
};
struct ipa_hwio_def_ipa_state_gsi_aos_s {
	u32	ipa_gsi_aos_fsm_idle : 1;
	u32	reserved0 : 31;
};
union ipa_hwio_def_ipa_state_gsi_aos_u {
	struct ipa_hwio_def_ipa_state_gsi_aos_s def;
	u32					value;
};
struct ipa_hwio_def_ipa_state_gsi_if_s {
	u32	ipa_gsi_prod_fsm_tx_0 : 4;
	u32	ipa_gsi_prod_fsm_tx_1 : 4;
	u32	reserved0 : 24;
};
union ipa_hwio_def_ipa_state_gsi_if_u {
	struct ipa_hwio_def_ipa_state_gsi_if_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_state_gsi_skip_s {
	u32	ipa_gsi_skip_fsm : 2;
	u32	reserved0 : 30;
};
union ipa_hwio_def_ipa_state_gsi_skip_u {
	struct ipa_hwio_def_ipa_state_gsi_skip_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_state_gsi_if_cons_s {
	u32	state : 1;
	u32	cache_vld : 6;
	u32	rx_req : 10;
	u32	rx_req_no_zero : 10;
	u32	reserved0 : 5;
};
union ipa_hwio_def_ipa_state_gsi_if_cons_u {
	struct ipa_hwio_def_ipa_state_gsi_if_cons_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_state_s {
	u32	rx_wait : 1;
	u32	rx_idle : 1;
	u32	tx_idle : 1;
	u32	dpl_fifo_idle : 1;
	u32	bam_gsi_idle : 1;
	u32	ipa_status_sniffer_idle : 1;
	u32	ipa_noc_idle : 1;
	u32	aggr_idle : 1;
	u32	mbim_aggr_idle : 1;
	u32	ipa_rsrc_mngr_db_empty : 1;
	u32	ipa_rsrc_state_idle : 1;
	u32	ipa_ackmngr_db_empty : 1;
	u32	ipa_ackmngr_state_idle : 1;
	u32	ipa_tx_ackq_full : 1;
	u32	ipa_prod_ackmngr_db_empty : 1;
	u32	ipa_prod_ackmngr_state_idle : 1;
	u32	ipa_prod_bresp_idle : 1;
	u32	ipa_full_idle : 1;
	u32	ipa_ntf_tx_empty : 1;
	u32	ipa_tx_ackq_empty : 1;
	u32	ipa_uc_ackq_empty : 1;
	u32	ipa_rx_ackq_empty : 1;
	u32	ipa_tx_commander_cmdq_empty : 1;
	u32	ipa_rx_splt_cmdq_empty : 4;
	u32	reserved0 : 1;
	u32	ipa_rx_hps_empty : 1;
	u32	ipa_hps_dps_empty : 1;
	u32	ipa_dps_tx_empty : 1;
	u32	ipa_uc_rx_hnd_cmdq_empty : 1;
};
union ipa_hwio_def_ipa_state_u {
	struct ipa_hwio_def_ipa_state_s def;
	u32				value;
};
struct ipa_hwio_def_ipa_state_rx_active_s {
	u32	endpoints : 13;
	u32	reserved0 : 19;
};
union ipa_hwio_def_ipa_state_rx_active_u {
	struct ipa_hwio_def_ipa_state_rx_active_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_state_tx0_s {
	u32	last_arbit_type : 2;
	u32	next_arbit_type : 2;
	u32	pa_idle : 1;
	u32	pa_ctx_idle : 1;
	u32	pa_pub_cnt_empty : 1;
	u32	tx_cmd_main_idle : 1;
	u32	tx_cmd_trnseq_idle : 1;
	u32	tx_cmd_snif_idle : 1;
	u32	tx_cmd_bresp_aloc_idle : 1;
	u32	tx_cmd_bresp_inj_idle : 1;
	u32	ar_idle : 1;
	u32	dmaw_idle : 1;
	u32	dmaw_last_outsd_idle : 1;
	u32	pf_idle : 1;
	u32	pf_empty : 1;
	u32	aligner_empty : 1;
	u32	holb_idle : 1;
	u32	holb_mask_idle : 1;
	u32	rsrcrel_idle : 1;
	u32	suspend_empty : 1;
	u32	cs_snif_idle : 1;
	u32	last_cmd_pipe : 5;
	u32	reserved0 : 4;
};
union ipa_hwio_def_ipa_state_tx0_u {
	struct ipa_hwio_def_ipa_state_tx0_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_state_aggr_active_s {
	u32	endpoints : 31;
	u32	reserved0 : 1;
};
union ipa_hwio_def_ipa_state_aggr_active_u {
	struct ipa_hwio_def_ipa_state_aggr_active_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_state_nlo_aggr_s {
	u32 nlo_aggr_state : 32;
};
union ipa_hwio_def_ipa_state_nlo_aggr_u {
	struct ipa_hwio_def_ipa_state_nlo_aggr_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_state_coal_master_1_s {
	u32	init_vp_wr_ctx_line : 6;
	u32	init_vp_rd_pkt_line : 6;
	u32	init_vp_fsm_state : 4;
	u32	check_fit_rd_ctx_line : 6;
	u32	check_fit_fsm_state : 4;
	u32	arbiter_state : 4;
	u32	reserved0 : 2;
};
union ipa_hwio_def_ipa_state_coal_master_1_u {
	struct ipa_hwio_def_ipa_state_coal_master_1_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_filt_rout_hash_en_s {
	u32	ipv6_router_hash_en : 1;
	u32	reserved0 : 3;
	u32	ipv6_filter_hash_en : 1;
	u32	reserved1 : 3;
	u32	ipv4_router_hash_en : 1;
	u32	reserved2 : 3;
	u32	ipv4_filter_hash_en : 1;
	u32	reserved3 : 19;
};
union ipa_hwio_def_ipa_filt_rout_hash_en_u {
	struct ipa_hwio_def_ipa_filt_rout_hash_en_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_filt_rout_hash_flush_s {
	u32	ipv6_router_hash_flush : 1;
	u32	reserved0 : 3;
	u32	ipv6_filter_hash_flush : 1;
	u32	reserved1 : 3;
	u32	ipv4_router_hash_flush : 1;
	u32	reserved2 : 3;
	u32	ipv4_filter_hash_flush : 1;
	u32	reserved3 : 19;
};
union ipa_hwio_def_ipa_filt_rout_hash_flush_u {
	struct ipa_hwio_def_ipa_filt_rout_hash_flush_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_ipv4_filter_init_values_s {
	u32	ip_v4_filter_init_hashed_addr : 16;
	u32	ip_v4_filter_init_non_hashed_addr : 16;
};
union ipa_hwio_def_ipa_ipv4_filter_init_values_u {
	struct ipa_hwio_def_ipa_ipv4_filter_init_values_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_ipv6_filter_init_values_s {
	u32	ip_v6_filter_init_hashed_addr : 16;
	u32	ip_v6_filter_init_non_hashed_addr : 16;
};
union ipa_hwio_def_ipa_ipv6_filter_init_values_u {
	struct ipa_hwio_def_ipa_ipv6_filter_init_values_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_ipv4_route_init_values_s {
	u32	ip_v4_route_init_hashed_addr : 16;
	u32	ip_v4_route_init_non_hashed_addr : 16;
};
union ipa_hwio_def_ipa_ipv4_route_init_values_u {
	struct ipa_hwio_def_ipa_ipv4_route_init_values_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_ipv6_route_init_values_s {
	u32	ip_v6_route_init_hashed_addr : 16;
	u32	ip_v6_route_init_non_hashed_addr : 16;
};
union ipa_hwio_def_ipa_ipv6_route_init_values_u {
	struct ipa_hwio_def_ipa_ipv6_route_init_values_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_bam_activated_ports_s {
	u32	endpoints : 31;
	u32	reserved0 : 1;
};
union ipa_hwio_def_ipa_bam_activated_ports_u {
	struct ipa_hwio_def_ipa_bam_activated_ports_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_sys_pkt_proc_cntxt_base_s {
	u32	zero : 3;
	u32	addr : 29;
};
union ipa_hwio_def_ipa_sys_pkt_proc_cntxt_base_u {
	struct ipa_hwio_def_ipa_sys_pkt_proc_cntxt_base_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_sys_pkt_proc_cntxt_base_msb_s {
	u32 addr : 32;
};
union ipa_hwio_def_ipa_sys_pkt_proc_cntxt_base_msb_u {
	struct ipa_hwio_def_ipa_sys_pkt_proc_cntxt_base_msb_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_local_pkt_proc_cntxt_base_s {
	u32	zero : 3;
	u32	addr : 15;
	u32	reserved0 : 14;
};
union ipa_hwio_def_ipa_local_pkt_proc_cntxt_base_u {
	struct ipa_hwio_def_ipa_local_pkt_proc_cntxt_base_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_src_rsrc_grp_01_rsrc_type_n_s {
	u32	src_rsrc_grp_0_min_limit : 6;
	u32	reserved0 : 2;
	u32	src_rsrc_grp_0_max_limit : 6;
	u32	reserved1 : 2;
	u32	src_rsrc_grp_1_min_limit : 6;
	u32	reserved2 : 2;
	u32	src_rsrc_grp_1_max_limit : 6;
	u32	reserved3 : 2;
};
union ipa_hwio_def_ipa_src_rsrc_grp_01_rsrc_type_n_u {
	struct ipa_hwio_def_ipa_src_rsrc_grp_01_rsrc_type_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_src_rsrc_grp_23_rsrc_type_n_s {
	u32	src_rsrc_grp_2_min_limit : 6;
	u32	reserved0 : 2;
	u32	src_rsrc_grp_2_max_limit : 6;
	u32	reserved1 : 2;
	u32	src_rsrc_grp_3_min_limit : 6;
	u32	reserved2 : 2;
	u32	src_rsrc_grp_3_max_limit : 6;
	u32	reserved3 : 2;
};
union ipa_hwio_def_ipa_src_rsrc_grp_23_rsrc_type_n_u {
	struct ipa_hwio_def_ipa_src_rsrc_grp_23_rsrc_type_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_src_rsrc_grp_45_rsrc_type_n_s {
	u32 src_rsrc_grp_4_min_limit : 6;
	u32 reserved0 : 2;
	u32 src_rsrc_grp_4_max_limit : 6;
	u32 reserved1 : 18;
};
union ipa_hwio_def_ipa_src_rsrc_grp_45_rsrc_type_n_u {
	struct ipa_hwio_def_ipa_src_rsrc_grp_45_rsrc_type_n_s	def;
	u32 value;
};
struct ipa_hwio_def_ipa_src_rsrc_grp_0123_rsrc_type_cnt_n_s {
	u32	src_rsrc_grp_0_cnt : 6;
	u32	reserved0 : 2;
	u32	src_rsrc_grp_1_cnt : 6;
	u32	reserved1 : 2;
	u32	src_rsrc_grp_2_cnt : 6;
	u32	reserved2 : 2;
	u32	src_rsrc_grp_3_cnt : 6;
	u32	reserved3 : 2;
};
union ipa_hwio_def_ipa_src_rsrc_grp_0123_rsrc_type_cnt_n_u {
	struct ipa_hwio_def_ipa_src_rsrc_grp_0123_rsrc_type_cnt_n_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_src_rsrc_grp_4567_rsrc_type_cnt_n_s {
	u32 src_rsrc_grp_4_cnt : 6;
	u32 reserved0 : 26;
};
union ipa_hwio_def_ipa_src_rsrc_grp_4567_rsrc_type_cnt_n_u {
	struct ipa_hwio_def_ipa_src_rsrc_grp_4567_rsrc_type_cnt_n_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_dst_rsrc_grp_01_rsrc_type_n_s {
	u32	dst_rsrc_grp_0_min_limit : 6;
	u32	reserved0 : 2;
	u32	dst_rsrc_grp_0_max_limit : 6;
	u32	reserved1 : 2;
	u32	dst_rsrc_grp_1_min_limit : 6;
	u32	reserved2 : 2;
	u32	dst_rsrc_grp_1_max_limit : 6;
	u32	reserved3 : 2;
};
union ipa_hwio_def_ipa_dst_rsrc_grp_01_rsrc_type_n_u {
	struct ipa_hwio_def_ipa_dst_rsrc_grp_01_rsrc_type_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_dst_rsrc_grp_23_rsrc_type_n_s {
	u32	dst_rsrc_grp_2_min_limit : 6;
	u32	reserved0 : 2;
	u32	dst_rsrc_grp_2_max_limit : 6;
	u32	reserved1 : 2;
	u32	dst_rsrc_grp_3_min_limit : 6;
	u32	reserved2 : 2;
	u32	dst_rsrc_grp_3_max_limit : 6;
	u32	reserved3 : 2;
};
union ipa_hwio_def_ipa_dst_rsrc_grp_23_rsrc_type_n_u {
	struct ipa_hwio_def_ipa_dst_rsrc_grp_23_rsrc_type_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_dst_rsrc_grp_45_rsrc_type_n_s {
	u32 dst_rsrc_grp_4_min_limit : 6;
	u32 reserved0 : 2;
	u32 dst_rsrc_grp_4_max_limit : 6;
	u32 reserved1 : 18;
};
union ipa_hwio_def_ipa_dst_rsrc_grp_45_rsrc_type_n_u {
	struct ipa_hwio_def_ipa_dst_rsrc_grp_45_rsrc_type_n_s	def;
	u32 value;
};
struct ipa_hwio_def_ipa_dst_rsrc_grp_0123_rsrc_type_cnt_n_s {
	u32	dst_rsrc_grp_0_cnt : 6;
	u32	reserved0 : 2;
	u32	dst_rsrc_grp_1_cnt : 6;
	u32	reserved1 : 2;
	u32	dst_rsrc_grp_2_cnt : 6;
	u32	reserved2 : 2;
	u32	dst_rsrc_grp_3_cnt : 6;
	u32	reserved3 : 2;
};
union ipa_hwio_def_ipa_dst_rsrc_grp_0123_rsrc_type_cnt_n_u {
	struct ipa_hwio_def_ipa_dst_rsrc_grp_0123_rsrc_type_cnt_n_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_dst_rsrc_grp_4567_rsrc_type_cnt_n_s {
	u32 dst_rsrc_grp_4_cnt : 8;
	u32 reserved0 : 24;
};
union ipa_hwio_def_ipa_dst_rsrc_grp_4567_rsrc_type_cnt_n_u {
	struct ipa_hwio_def_ipa_dst_rsrc_grp_4567_rsrc_type_cnt_n_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_rsrc_grp_cfg_s {
	u32	src_grp_special_valid : 1;
	u32	reserved0 : 3;
	u32	src_grp_special_index : 3;
	u32	reserved1 : 1;
	u32	dst_pipe_special_valid : 1;
	u32	reserved2 : 3;
	u32	dst_pipe_special_index : 5;
	u32	reserved3 : 3;
	u32	dst_grp_special_valid : 1;
	u32	reserved4 : 3;
	u32	dst_grp_special_index : 6;
	u32	reserved5 : 2;
};
union ipa_hwio_def_ipa_rsrc_grp_cfg_u {
	struct ipa_hwio_def_ipa_rsrc_grp_cfg_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_pipeline_disable_s {
	u32	reserved0 : 3;
	u32	rx_cmdq_splitter_dis : 1;
	u32	reserved1 : 28;
};
union ipa_hwio_def_ipa_pipeline_disable_u {
	struct ipa_hwio_def_ipa_pipeline_disable_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_init_ctrl_n_s {
	u32	endp_suspend : 1;
	u32	endp_delay : 1;
	u32	reserved0 : 30;
};
union ipa_hwio_def_ipa_endp_init_ctrl_n_u {
	struct ipa_hwio_def_ipa_endp_init_ctrl_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_init_ctrl_scnd_n_s {
	u32	reserved0 : 1;
	u32	endp_delay : 1;
	u32	reserved1 : 30;
};
union ipa_hwio_def_ipa_endp_init_ctrl_scnd_n_u {
	struct ipa_hwio_def_ipa_endp_init_ctrl_scnd_n_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_init_cfg_n_s {
	u32	frag_offload_en : 1;
	u32	cs_offload_en : 2;
	u32	cs_metadata_hdr_offset : 4;
	u32	reserved0 : 1;
	u32	gen_qmb_master_sel : 1;
	u32	reserved1 : 23;
};
union ipa_hwio_def_ipa_endp_init_cfg_n_u {
	struct ipa_hwio_def_ipa_endp_init_cfg_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_init_nat_n_s {
	u32	nat_en : 2;
	u32	reserved0 : 30;
};
union ipa_hwio_def_ipa_endp_init_nat_n_u {
	struct ipa_hwio_def_ipa_endp_init_nat_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_init_hdr_n_s {
	u32	hdr_len : 6;
	u32	hdr_ofst_metadata_valid : 1;
	u32	hdr_ofst_metadata : 6;
	u32	hdr_additional_const_len : 6;
	u32	hdr_ofst_pkt_size_valid : 1;
	u32	hdr_ofst_pkt_size : 6;
	u32	hdr_a5_mux : 1;
	u32	hdr_len_inc_deagg_hdr : 1;
	u32	hdr_len_msb : 2;
	u32	hdr_ofst_metadata_msb : 2;
};
union ipa_hwio_def_ipa_endp_init_hdr_n_u {
	struct ipa_hwio_def_ipa_endp_init_hdr_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_init_hdr_ext_n_s {
	u32	hdr_endianness : 1;
	u32	hdr_total_len_or_pad_valid : 1;
	u32	hdr_total_len_or_pad : 1;
	u32	hdr_payload_len_inc_padding : 1;
	u32	hdr_total_len_or_pad_offset : 6;
	u32	hdr_pad_to_alignment : 4;
	u32	reserved0 : 2;
	u32	hdr_total_len_or_pad_offset_msb : 2;
	u32	hdr_ofst_pkt_size_msb : 2;
	u32	hdr_additional_const_len_msb : 2;
	u32	reserved1 : 10;
};
union ipa_hwio_def_ipa_endp_init_hdr_ext_n_u {
	struct ipa_hwio_def_ipa_endp_init_hdr_ext_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_init_hdr_metadata_mask_n_s {
	u32 metadata_mask : 32;
};
union ipa_hwio_def_ipa_endp_init_hdr_metadata_mask_n_u {
	struct ipa_hwio_def_ipa_endp_init_hdr_metadata_mask_n_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_endp_init_hdr_metadata_n_s {
	u32 metadata : 32;
};
union ipa_hwio_def_ipa_endp_init_hdr_metadata_n_u {
	struct ipa_hwio_def_ipa_endp_init_hdr_metadata_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_endp_init_mode_n_s {
	u32	mode : 3;
	u32	dcph_enable : 1;
	u32	dest_pipe_index : 5;
	u32	reserved0 : 3;
	u32	byte_threshold : 16;
	u32	pipe_replicate_en : 1;
	u32	pad_en : 1;
	u32	reserved1 : 2;
};
union ipa_hwio_def_ipa_endp_init_mode_n_u {
	struct ipa_hwio_def_ipa_endp_init_mode_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_init_aggr_n_s {
	u32	aggr_en : 2;
	u32	aggr_type : 3;
	u32	aggr_byte_limit : 6;
	u32	reserved0 : 1;
	u32	aggr_time_limit : 5;
	u32	aggr_pkt_limit : 6;
	u32	aggr_sw_eof_active : 1;
	u32	aggr_force_close : 1;
	u32	reserved1 : 1;
	u32	aggr_hard_byte_limit_enable : 1;
	u32	aggr_gran_sel : 1;
	u32	reserved2 : 4;
};
union ipa_hwio_def_ipa_endp_init_aggr_n_u {
	struct ipa_hwio_def_ipa_endp_init_aggr_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_init_hol_block_en_n_s {
	u32	en : 1;
	u32	reserved0 : 31;
};
union ipa_hwio_def_ipa_endp_init_hol_block_en_n_u {
	struct ipa_hwio_def_ipa_endp_init_hol_block_en_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_endp_init_hol_block_timer_n_s {
	u32	time_limit : 5;
	u32	reserved0 : 3;
	u32	gran_sel : 1;
	u32	reserved1 : 23;
};
union ipa_hwio_def_ipa_endp_init_hol_block_timer_n_u {
	struct ipa_hwio_def_ipa_endp_init_hol_block_timer_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_endp_init_deaggr_n_s {
	u32	deaggr_hdr_len : 6;
	u32	syspipe_err_detection : 1;
	u32	packet_offset_valid : 1;
	u32	packet_offset_location : 6;
	u32	ignore_min_pkt_err : 1;
	u32	reserved0 : 1;
	u32	max_packet_len : 16;
};
union ipa_hwio_def_ipa_endp_init_deaggr_n_u {
	struct ipa_hwio_def_ipa_endp_init_deaggr_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_init_rsrc_grp_n_s {
	u32	rsrc_grp : 3;
	u32	reserved0 : 29;
};
union ipa_hwio_def_ipa_endp_init_rsrc_grp_n_u {
	struct ipa_hwio_def_ipa_endp_init_rsrc_grp_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_init_seq_n_s {
	u32	hps_seq_type : 4;
	u32	dps_seq_type : 4;
	u32	hps_rep_seq_type : 4;
	u32	dps_rep_seq_type : 4;
	u32	reserved0 : 16;
};
union ipa_hwio_def_ipa_endp_init_seq_n_u {
	struct ipa_hwio_def_ipa_endp_init_seq_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_status_n_s {
	u32	status_en : 1;
	u32	status_endp : 5;
	u32	reserved0 : 3;
	u32	status_pkt_suppress : 1;
	u32	reserved1 : 22;
};
union ipa_hwio_def_ipa_endp_status_n_u {
	struct ipa_hwio_def_ipa_endp_status_n_s def;
	u32					value;
};
struct ipa_hwio_def_ipa_endp_filter_router_hsh_cfg_n_s {
	u32	filter_hash_msk_src_id : 1;
	u32	filter_hash_msk_src_ip_add : 1;
	u32	filter_hash_msk_dst_ip_add : 1;
	u32	filter_hash_msk_src_port : 1;
	u32	filter_hash_msk_dst_port : 1;
	u32	filter_hash_msk_protocol : 1;
	u32	filter_hash_msk_metadata : 1;
	u32	reserved0 : 9;
	u32	router_hash_msk_src_id : 1;
	u32	router_hash_msk_src_ip_add : 1;
	u32	router_hash_msk_dst_ip_add : 1;
	u32	router_hash_msk_src_port : 1;
	u32	router_hash_msk_dst_port : 1;
	u32	router_hash_msk_protocol : 1;
	u32	router_hash_msk_metadata : 1;
	u32	reserved1 : 9;
};
union ipa_hwio_def_ipa_endp_filter_router_hsh_cfg_n_u {
	struct ipa_hwio_def_ipa_endp_filter_router_hsh_cfg_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_nlo_pp_cfg1_s {
	u32	nlo_ack_pp : 8;
	u32	nlo_data_pp : 8;
	u32	nlo_status_pp : 8;
	u32	nlo_ack_max_vp : 6;
	u32	reserved0 : 2;
};
union ipa_hwio_def_ipa_nlo_pp_cfg1_u {
	struct ipa_hwio_def_ipa_nlo_pp_cfg1_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_nlo_pp_cfg2_s {
	u32	nlo_ack_close_padd : 8;
	u32	nlo_data_close_padd : 8;
	u32	nlo_ack_buffer_mode : 1;
	u32	nlo_data_buffer_mode : 1;
	u32	nlo_status_buffer_mode : 1;
	u32	reserved0 : 13;
};
union ipa_hwio_def_ipa_nlo_pp_cfg2_u {
	struct ipa_hwio_def_ipa_nlo_pp_cfg2_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_nlo_pp_ack_limit_cfg_s {
	u32	nlo_ack_lower_size : 16;
	u32	nlo_ack_upper_size : 16;
};
union ipa_hwio_def_ipa_nlo_pp_ack_limit_cfg_u {
	struct ipa_hwio_def_ipa_nlo_pp_ack_limit_cfg_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_nlo_pp_data_limit_cfg_s {
	u32	nlo_data_lower_size : 16;
	u32	nlo_data_upper_size : 16;
};
union ipa_hwio_def_ipa_nlo_pp_data_limit_cfg_u {
	struct ipa_hwio_def_ipa_nlo_pp_data_limit_cfg_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_nlo_min_dsm_cfg_s {
	u32	nlo_ack_min_dsm_len : 16;
	u32	nlo_data_min_dsm_len : 16;
};
union ipa_hwio_def_ipa_nlo_min_dsm_cfg_u {
	struct ipa_hwio_def_ipa_nlo_min_dsm_cfg_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_nlo_vp_flush_req_s {
	u32	vp_flush_pp_indx : 8;
	u32	reserved0 : 8;
	u32	vp_flush_vp_indx : 8;
	u32	reserved1 : 7;
	u32	vp_flush_req : 1;
};
union ipa_hwio_def_ipa_nlo_vp_flush_req_u {
	struct ipa_hwio_def_ipa_nlo_vp_flush_req_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_nlo_vp_flush_cookie_s {
	u32 vp_flush_cookie : 32;
};
union ipa_hwio_def_ipa_nlo_vp_flush_cookie_u {
	struct ipa_hwio_def_ipa_nlo_vp_flush_cookie_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_nlo_vp_flush_ack_s {
	u32	vp_flush_ack : 1;
	u32	reserved0 : 31;
};
union ipa_hwio_def_ipa_nlo_vp_flush_ack_u {
	struct ipa_hwio_def_ipa_nlo_vp_flush_ack_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_nlo_vp_dsm_open_s {
	u32 vp_dsm_open : 32;
};
union ipa_hwio_def_ipa_nlo_vp_dsm_open_u {
	struct ipa_hwio_def_ipa_nlo_vp_dsm_open_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_nlo_vp_qbap_open_s {
	u32 vp_qbap_open : 32;
};
union ipa_hwio_def_ipa_nlo_vp_qbap_open_u {
	struct ipa_hwio_def_ipa_nlo_vp_qbap_open_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_rsrc_mngr_db_cfg_s {
	u32	rsrc_grp_sel : 3;
	u32	reserved0 : 1;
	u32	rsrc_type_sel : 3;
	u32	reserved1 : 1;
	u32	rsrc_id_sel : 6;
	u32	reserved2 : 18;
};
union ipa_hwio_def_ipa_rsrc_mngr_db_cfg_u {
	struct ipa_hwio_def_ipa_rsrc_mngr_db_cfg_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_rsrc_mngr_db_rsrc_read_s {
	u32	rsrc_occupied : 1;
	u32	rsrc_next_valid : 1;
	u32	reserved0 : 2;
	u32	rsrc_next_index : 6;
	u32	reserved1 : 22;
};
union ipa_hwio_def_ipa_rsrc_mngr_db_rsrc_read_u {
	struct ipa_hwio_def_ipa_rsrc_mngr_db_rsrc_read_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_rsrc_mngr_db_list_read_s {
	u32	rsrc_list_valid : 1;
	u32	rsrc_list_hold : 1;
	u32	reserved0 : 2;
	u32	rsrc_list_head_rsrc : 6;
	u32	reserved1 : 2;
	u32	rsrc_list_head_cnt : 7;
	u32	reserved2 : 1;
	u32	rsrc_list_entry_cnt : 7;
	u32	reserved3 : 5;
};
union ipa_hwio_def_ipa_rsrc_mngr_db_list_read_u {
	struct ipa_hwio_def_ipa_rsrc_mngr_db_list_read_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_debug_data_s {
	u32 debug_data : 32;
};
union ipa_hwio_def_ipa_debug_data_u {
	struct ipa_hwio_def_ipa_debug_data_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_testbus_sel_s {
	u32	testbus_en : 1;
	u32	reserved0 : 3;
	u32	external_block_select : 8;
	u32	internal_block_select : 8;
	u32	pipe_select : 5;
	u32	reserved1 : 7;
};
union ipa_hwio_def_ipa_testbus_sel_u {
	struct ipa_hwio_def_ipa_testbus_sel_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_step_mode_breakpoints_s {
	u32 hw_en : 32;
};
union ipa_hwio_def_ipa_step_mode_breakpoints_u {
	struct ipa_hwio_def_ipa_step_mode_breakpoints_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_step_mode_status_s {
	u32 hw_en : 32;
};
union ipa_hwio_def_ipa_step_mode_status_u {
	struct ipa_hwio_def_ipa_step_mode_status_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_s {
	u32	reserved0 : 1;
	u32	log_en : 1;
	u32	reserved1 : 2;
	u32	log_pipe : 5;
	u32	reserved2 : 3;
	u32	log_length : 8;
	u32	log_reduction_en : 1;
	u32	log_dpl_l2_remove_en : 1;
	u32	reserved3 : 10;
};
union ipa_hwio_def_ipa_log_u {
	struct ipa_hwio_def_ipa_log_s	def;
	u32				value;
};
struct ipa_hwio_def_ipa_log_buf_hw_cmd_addr_s {
	u32 start_addr : 32;
};
union ipa_hwio_def_ipa_log_buf_hw_cmd_addr_u {
	struct ipa_hwio_def_ipa_log_buf_hw_cmd_addr_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_hw_cmd_addr_msb_s {
	u32 start_addr : 32;
};
union ipa_hwio_def_ipa_log_buf_hw_cmd_addr_msb_u {
	struct ipa_hwio_def_ipa_log_buf_hw_cmd_addr_msb_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_log_buf_hw_cmd_write_ptr_s {
	u32 writr_addr : 32;
};
union ipa_hwio_def_ipa_log_buf_hw_cmd_write_ptr_u {
	struct ipa_hwio_def_ipa_log_buf_hw_cmd_write_ptr_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_log_buf_hw_cmd_write_ptr_msb_s {
	u32 writr_addr : 32;
};
union ipa_hwio_def_ipa_log_buf_hw_cmd_write_ptr_msb_u {
	struct ipa_hwio_def_ipa_log_buf_hw_cmd_write_ptr_msb_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_log_buf_hw_cmd_cfg_s {
	u32	size : 16;
	u32	enable : 1;
	u32	skip_ddr_dma : 1;
	u32	reserved0 : 14;
};
union ipa_hwio_def_ipa_log_buf_hw_cmd_cfg_u {
	struct ipa_hwio_def_ipa_log_buf_hw_cmd_cfg_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_hw_cmd_ram_ptr_s {
	u32	read_ptr : 14;
	u32	reserved0 : 2;
	u32	write_ptr : 14;
	u32	reserved1 : 1;
	u32	skip_ddr_wrap_happened : 1;
};
union ipa_hwio_def_ipa_log_buf_hw_cmd_ram_ptr_u {
	struct ipa_hwio_def_ipa_log_buf_hw_cmd_ram_ptr_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_rx_splt_cmdq_cmd_n_s {
	u32	write_cmd : 1;
	u32	pop_cmd : 1;
	u32	release_rd_cmd : 1;
	u32	release_wr_cmd : 1;
	u32	release_rd_pkt : 1;
	u32	release_wr_pkt : 1;
	u32	release_rd_pkt_enhanced : 1;
	u32	reserved0 : 25;
};
union ipa_hwio_def_ipa_rx_splt_cmdq_cmd_n_u {
	struct ipa_hwio_def_ipa_rx_splt_cmdq_cmd_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_rx_splt_cmdq_cfg_n_s {
	u32	block_rd : 1;
	u32	block_wr : 1;
	u32	reserved0 : 30;
};
union ipa_hwio_def_ipa_rx_splt_cmdq_cfg_n_u {
	struct ipa_hwio_def_ipa_rx_splt_cmdq_cfg_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_rx_splt_cmdq_data_wr_0_n_s {
	u32	cmdq_packet_len_f : 16;
	u32	cmdq_src_len_f : 16;
};
union ipa_hwio_def_ipa_rx_splt_cmdq_data_wr_0_n_u {
	struct ipa_hwio_def_ipa_rx_splt_cmdq_data_wr_0_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_rx_splt_cmdq_data_wr_1_n_s {
	u32	cmdq_src_pipe_f : 8;
	u32	cmdq_order_f : 2;
	u32	cmdq_flags_f : 6;
	u32	cmdq_opcode_f : 8;
	u32	cmdq_metadata_f : 8;
};
union ipa_hwio_def_ipa_rx_splt_cmdq_data_wr_1_n_u {
	struct ipa_hwio_def_ipa_rx_splt_cmdq_data_wr_1_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_rx_splt_cmdq_data_wr_2_n_s {
	u32 cmdq_addr_lsb_f : 32;
};
union ipa_hwio_def_ipa_rx_splt_cmdq_data_wr_2_n_u {
	struct ipa_hwio_def_ipa_rx_splt_cmdq_data_wr_2_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_rx_splt_cmdq_data_wr_3_n_s {
	u32 cmdq_addr_msb_f : 32;
};
union ipa_hwio_def_ipa_rx_splt_cmdq_data_wr_3_n_u {
	struct ipa_hwio_def_ipa_rx_splt_cmdq_data_wr_3_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_rx_splt_cmdq_data_rd_0_n_s {
	u32	cmdq_packet_len_f : 16;
	u32	cmdq_src_len_f : 16;
};
union ipa_hwio_def_ipa_rx_splt_cmdq_data_rd_0_n_u {
	struct ipa_hwio_def_ipa_rx_splt_cmdq_data_rd_0_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_rx_splt_cmdq_data_rd_1_n_s {
	u32	cmdq_src_pipe_f : 8;
	u32	cmdq_order_f : 2;
	u32	cmdq_flags_f : 6;
	u32	cmdq_opcode_f : 8;
	u32	cmdq_metadata_f : 8;
};
union ipa_hwio_def_ipa_rx_splt_cmdq_data_rd_1_n_u {
	struct ipa_hwio_def_ipa_rx_splt_cmdq_data_rd_1_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_rx_splt_cmdq_data_rd_2_n_s {
	u32 cmdq_addr_lsb_f : 32;
};
union ipa_hwio_def_ipa_rx_splt_cmdq_data_rd_2_n_u {
	struct ipa_hwio_def_ipa_rx_splt_cmdq_data_rd_2_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_rx_splt_cmdq_data_rd_3_n_s {
	u32 cmdq_addr_msb_f : 32;
};
union ipa_hwio_def_ipa_rx_splt_cmdq_data_rd_3_n_u {
	struct ipa_hwio_def_ipa_rx_splt_cmdq_data_rd_3_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_rx_splt_cmdq_status_n_s {
	u32	status : 1;
	u32	cmdq_empty : 1;
	u32	cmdq_full : 1;
	u32	cmdq_count : 2;
	u32	cmdq_depth : 2;
	u32	reserved0 : 25;
};
union ipa_hwio_def_ipa_rx_splt_cmdq_status_n_u {
	struct ipa_hwio_def_ipa_rx_splt_cmdq_status_n_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_tx_commander_cmdq_status_s {
	u32	status : 1;
	u32	cmdq_empty : 1;
	u32	cmdq_full : 1;
	u32	reserved0 : 29;
};
union ipa_hwio_def_ipa_tx_commander_cmdq_status_u {
	struct ipa_hwio_def_ipa_tx_commander_cmdq_status_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_rx_hps_cmdq_cmd_s {
	u32	write_cmd : 1;
	u32	pop_cmd : 1;
	u32	cmd_client : 3;
	u32	rd_req : 1;
	u32	reserved0 : 26;
};
union ipa_hwio_def_ipa_rx_hps_cmdq_cmd_u {
	struct ipa_hwio_def_ipa_rx_hps_cmdq_cmd_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_rx_hps_cmdq_cfg_wr_s {
	u32	block_wr : 5;
	u32	reserved0 : 27;
};
union ipa_hwio_def_ipa_rx_hps_cmdq_cfg_wr_u {
	struct ipa_hwio_def_ipa_rx_hps_cmdq_cfg_wr_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_rx_hps_cmdq_cfg_rd_s {
	u32	block_rd : 5;
	u32	reserved0 : 27;
};
union ipa_hwio_def_ipa_rx_hps_cmdq_cfg_rd_u {
	struct ipa_hwio_def_ipa_rx_hps_cmdq_cfg_rd_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_0_s {
	u32	cmdq_packet_len_f : 16;
	u32	cmdq_dest_len_f : 16;
};
union ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_0_u {
	struct ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_0_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_1_s {
	u32	cmdq_src_pipe_f : 8;
	u32	cmdq_order_f : 2;
	u32	cmdq_flags_f : 6;
	u32	cmdq_opcode_f : 8;
	u32	cmdq_metadata_f : 8;
};
union ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_1_u {
	struct ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_1_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_2_s {
	u32 cmdq_addr_lsb_f : 32;
};
union ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_2_u {
	struct ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_2_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_3_s {
	u32 cmdq_addr_msb_f : 32;
};
union ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_3_u {
	struct ipa_hwio_def_ipa_rx_hps_cmdq_data_rd_3_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_rx_hps_cmdq_status_s {
	u32	status : 1;
	u32	cmdq_full : 1;
	u32	cmdq_depth : 7;
	u32	reserved0 : 23;
};
union ipa_hwio_def_ipa_rx_hps_cmdq_status_u {
	struct ipa_hwio_def_ipa_rx_hps_cmdq_status_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_rx_hps_cmdq_status_empty_s {
	u32	cmdq_empty : 5;
	u32	reserved0 : 27;
};
union ipa_hwio_def_ipa_rx_hps_cmdq_status_empty_u {
	struct ipa_hwio_def_ipa_rx_hps_cmdq_status_empty_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_rx_hps_cmdq_count_s {
	u32	fifo_count : 7;
	u32	reserved0 : 25;
};
union ipa_hwio_def_ipa_rx_hps_cmdq_count_u {
	struct ipa_hwio_def_ipa_rx_hps_cmdq_count_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_rx_hps_clients_min_depth_0_s {
	u32	client_0_min_depth : 4;
	u32	reserved0 : 4;
	u32	client_1_min_depth : 4;
	u32	reserved1 : 4;
	u32	client_2_min_depth : 4;
	u32	reserved2 : 4;
	u32	client_3_min_depth : 4;
	u32	client_4_min_depth : 4;
};
union ipa_hwio_def_ipa_rx_hps_clients_min_depth_0_u {
	struct ipa_hwio_def_ipa_rx_hps_clients_min_depth_0_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_rx_hps_clients_max_depth_0_s {
	u32	client_0_max_depth : 4;
	u32	reserved0 : 4;
	u32	client_1_max_depth : 4;
	u32	reserved1 : 4;
	u32	client_2_max_depth : 4;
	u32	reserved2 : 4;
	u32	client_3_max_depth : 4;
	u32	client_4_max_depth : 4;
};
union ipa_hwio_def_ipa_rx_hps_clients_max_depth_0_u {
	struct ipa_hwio_def_ipa_rx_hps_clients_max_depth_0_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_hps_dps_cmdq_cmd_s {
	u32	write_cmd : 1;
	u32	pop_cmd : 1;
	u32	cmd_client : 5;
	u32	rd_req : 1;
	u32	reserved0 : 24;
};
union ipa_hwio_def_ipa_hps_dps_cmdq_cmd_u {
	struct ipa_hwio_def_ipa_hps_dps_cmdq_cmd_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_hps_dps_cmdq_data_rd_0_s {
	u32	cmdq_ctx_id_f : 4;
	u32	cmdq_src_id_f : 8;
	u32	cmdq_src_pipe_f : 5;
	u32	cmdq_opcode_f : 2;
	u32	cmdq_rep_f : 1;
	u32	reserved0 : 12;
};
union ipa_hwio_def_ipa_hps_dps_cmdq_data_rd_0_u {
	struct ipa_hwio_def_ipa_hps_dps_cmdq_data_rd_0_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_hps_dps_cmdq_status_s {
	u32	status : 1;
	u32	cmdq_full : 1;
	u32	cmdq_depth : 6;
	u32	reserved0 : 24;
};
union ipa_hwio_def_ipa_hps_dps_cmdq_status_u {
	struct ipa_hwio_def_ipa_hps_dps_cmdq_status_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_hps_dps_cmdq_status_empty_s {
	u32	cmdq_empty : 31;
	u32	reserved0 : 1;
};
union ipa_hwio_def_ipa_hps_dps_cmdq_status_empty_u {
	struct ipa_hwio_def_ipa_hps_dps_cmdq_status_empty_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_hps_dps_cmdq_count_s {
	u32	fifo_count : 6;
	u32	reserved0 : 26;
};
union ipa_hwio_def_ipa_hps_dps_cmdq_count_u {
	struct ipa_hwio_def_ipa_hps_dps_cmdq_count_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_dps_tx_cmdq_cmd_s {
	u32	write_cmd : 1;
	u32	pop_cmd : 1;
	u32	cmd_client : 4;
	u32	reserved0 : 1;
	u32	rd_req : 1;
	u32	reserved1 : 24;
};
union ipa_hwio_def_ipa_dps_tx_cmdq_cmd_u {
	struct ipa_hwio_def_ipa_dps_tx_cmdq_cmd_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_dps_tx_cmdq_data_rd_0_s {
	u32	cmdq_ctx_id_f : 4;
	u32	cmdq_src_id_f : 8;
	u32	cmdq_src_pipe_f : 5;
	u32	cmdq_opcode_f : 2;
	u32	cmdq_rep_f : 1;
	u32	reserved0 : 12;
};
union ipa_hwio_def_ipa_dps_tx_cmdq_data_rd_0_u {
	struct ipa_hwio_def_ipa_dps_tx_cmdq_data_rd_0_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_dps_tx_cmdq_status_s {
	u32	status : 1;
	u32	cmdq_full : 1;
	u32	cmdq_depth : 7;
	u32	reserved0 : 23;
};
union ipa_hwio_def_ipa_dps_tx_cmdq_status_u {
	struct ipa_hwio_def_ipa_dps_tx_cmdq_status_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_dps_tx_cmdq_status_empty_s {
	u32	cmdq_empty : 10;
	u32	reserved0 : 22;
};
union ipa_hwio_def_ipa_dps_tx_cmdq_status_empty_u {
	struct ipa_hwio_def_ipa_dps_tx_cmdq_status_empty_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_dps_tx_cmdq_count_s {
	u32	fifo_count : 7;
	u32	reserved0 : 25;
};
union ipa_hwio_def_ipa_dps_tx_cmdq_count_u {
	struct ipa_hwio_def_ipa_dps_tx_cmdq_count_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_hw_snif_el_en_s {
	u32	bitmap : 3;
	u32	reserved0 : 29;
};
union ipa_hwio_def_ipa_log_buf_hw_snif_el_en_u {
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_en_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_hw_snif_el_wr_n_rd_sel_s {
	u32	bitmap : 3;
	u32	reserved0 : 29;
};
union ipa_hwio_def_ipa_log_buf_hw_snif_el_wr_n_rd_sel_u {
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_wr_n_rd_sel_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_log_buf_hw_snif_el_cli_mux_s {
	u32	all_cli_mux_concat : 12;
	u32	reserved0 : 20;
};
union ipa_hwio_def_ipa_log_buf_hw_snif_el_cli_mux_u {
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_cli_mux_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_log_buf_hw_snif_el_comp_val_0_cli_n_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_hw_snif_el_comp_val_0_cli_n_u {
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_comp_val_0_cli_n_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_log_buf_hw_snif_el_comp_val_1_cli_n_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_hw_snif_el_comp_val_1_cli_n_u {
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_comp_val_1_cli_n_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_log_buf_hw_snif_el_comp_val_2_cli_n_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_hw_snif_el_comp_val_2_cli_n_u {
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_comp_val_2_cli_n_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_log_buf_hw_snif_el_comp_val_3_cli_n_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_hw_snif_el_comp_val_3_cli_n_u {
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_comp_val_3_cli_n_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_log_buf_hw_snif_el_mask_val_0_cli_n_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_hw_snif_el_mask_val_0_cli_n_u {
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_mask_val_0_cli_n_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_log_buf_hw_snif_el_mask_val_1_cli_n_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_hw_snif_el_mask_val_1_cli_n_u {
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_mask_val_1_cli_n_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_log_buf_hw_snif_el_mask_val_2_cli_n_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_hw_snif_el_mask_val_2_cli_n_u {
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_mask_val_2_cli_n_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_log_buf_hw_snif_el_mask_val_3_cli_n_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_hw_snif_el_mask_val_3_cli_n_u {
	struct ipa_hwio_def_ipa_log_buf_hw_snif_el_mask_val_3_cli_n_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_log_buf_hw_snif_legacy_rx_s {
	u32	src_group_sel : 3;
	u32	reserved0 : 29;
};
union ipa_hwio_def_ipa_log_buf_hw_snif_legacy_rx_u {
	struct ipa_hwio_def_ipa_log_buf_hw_snif_legacy_rx_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_ackmngr_cmdq_cmd_s {
	u32	write_cmd : 1;
	u32	pop_cmd : 1;
	u32	cmd_client : 5;
	u32	rd_req : 1;
	u32	reserved0 : 24;
};
union ipa_hwio_def_ipa_ackmngr_cmdq_cmd_u {
	struct ipa_hwio_def_ipa_ackmngr_cmdq_cmd_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_ackmngr_cmdq_data_rd_s {
	u32	cmdq_src_id : 8;
	u32	cmdq_length : 16;
	u32	cmdq_origin : 1;
	u32	cmdq_sent : 1;
	u32	cmdq_src_id_valid : 1;
	u32	reserved0 : 5;
};
union ipa_hwio_def_ipa_ackmngr_cmdq_data_rd_u {
	struct ipa_hwio_def_ipa_ackmngr_cmdq_data_rd_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_ackmngr_cmdq_status_s {
	u32	status : 1;
	u32	cmdq_full : 1;
	u32	cmdq_depth : 7;
	u32	reserved0 : 23;
};
union ipa_hwio_def_ipa_ackmngr_cmdq_status_u {
	struct ipa_hwio_def_ipa_ackmngr_cmdq_status_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_ackmngr_cmdq_status_empty_s {
	u32	cmdq_empty : 13;
	u32	reserved0 : 19;
};
union ipa_hwio_def_ipa_ackmngr_cmdq_status_empty_u {
	struct ipa_hwio_def_ipa_ackmngr_cmdq_status_empty_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_ackmngr_cmdq_count_s {
	u32	fifo_count : 7;
	u32	reserved0 : 25;
};
union ipa_hwio_def_ipa_ackmngr_cmdq_count_u {
	struct ipa_hwio_def_ipa_ackmngr_cmdq_count_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_gsi_fifo_status_ctrl_s {
	u32	ipa_gsi_fifo_status_port_sel : 5;
	u32	ipa_gsi_fifo_status_en : 1;
	u32	reserved0 : 26;
};
union ipa_hwio_def_ipa_gsi_fifo_status_ctrl_u {
	struct ipa_hwio_def_ipa_gsi_fifo_status_ctrl_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_gsi_tlv_fifo_status_s {
	u32	fifo_wr_ptr : 8;
	u32	fifo_rd_ptr : 8;
	u32	fifo_rd_pub_ptr : 8;
	u32	fifo_empty : 1;
	u32	fifo_empty_pub : 1;
	u32	fifo_almost_full : 1;
	u32	fifo_full : 1;
	u32	fifo_almost_full_pub : 1;
	u32	fifo_full_pub : 1;
	u32	fifo_head_is_bubble : 1;
	u32	reserved0 : 1;
};
union ipa_hwio_def_ipa_gsi_tlv_fifo_status_u {
	struct ipa_hwio_def_ipa_gsi_tlv_fifo_status_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_gsi_aos_fifo_status_s {
	u32	fifo_wr_ptr : 8;
	u32	fifo_rd_ptr : 8;
	u32	fifo_rd_pub_ptr : 8;
	u32	fifo_empty : 1;
	u32	fifo_empty_pub : 1;
	u32	fifo_almost_full : 1;
	u32	fifo_full : 1;
	u32	fifo_almost_full_pub : 1;
	u32	fifo_full_pub : 1;
	u32	fifo_head_is_bubble : 1;
	u32	reserved0 : 1;
};
union ipa_hwio_def_ipa_gsi_aos_fifo_status_u {
	struct ipa_hwio_def_ipa_gsi_aos_fifo_status_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_comp_val_0_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_comp_val_0_u {
	struct ipa_hwio_def_ipa_log_buf_sw_comp_val_0_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_comp_val_1_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_comp_val_1_u {
	struct ipa_hwio_def_ipa_log_buf_sw_comp_val_1_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_comp_val_2_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_comp_val_2_u {
	struct ipa_hwio_def_ipa_log_buf_sw_comp_val_2_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_comp_val_3_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_comp_val_3_u {
	struct ipa_hwio_def_ipa_log_buf_sw_comp_val_3_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_comp_val_4_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_comp_val_4_u {
	struct ipa_hwio_def_ipa_log_buf_sw_comp_val_4_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_comp_val_5_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_comp_val_5_u {
	struct ipa_hwio_def_ipa_log_buf_sw_comp_val_5_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_comp_val_6_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_comp_val_6_u {
	struct ipa_hwio_def_ipa_log_buf_sw_comp_val_6_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_comp_val_7_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_comp_val_7_u {
	struct ipa_hwio_def_ipa_log_buf_sw_comp_val_7_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_mask_val_0_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_mask_val_0_u {
	struct ipa_hwio_def_ipa_log_buf_sw_mask_val_0_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_mask_val_1_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_mask_val_1_u {
	struct ipa_hwio_def_ipa_log_buf_sw_mask_val_1_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_mask_val_2_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_mask_val_2_u {
	struct ipa_hwio_def_ipa_log_buf_sw_mask_val_2_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_mask_val_3_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_mask_val_3_u {
	struct ipa_hwio_def_ipa_log_buf_sw_mask_val_3_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_mask_val_4_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_mask_val_4_u {
	struct ipa_hwio_def_ipa_log_buf_sw_mask_val_4_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_mask_val_5_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_mask_val_5_u {
	struct ipa_hwio_def_ipa_log_buf_sw_mask_val_5_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_mask_val_6_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_mask_val_6_u {
	struct ipa_hwio_def_ipa_log_buf_sw_mask_val_6_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_sw_mask_val_7_s {
	u32 value : 32;
};
union ipa_hwio_def_ipa_log_buf_sw_mask_val_7_u {
	struct ipa_hwio_def_ipa_log_buf_sw_mask_val_7_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_ntf_tx_cmdq_cmd_s {
	u32	write_cmd : 1;
	u32	pop_cmd : 1;
	u32	cmd_client : 5;
	u32	rd_req : 1;
	u32	reserved0 : 24;
};
union ipa_hwio_def_ipa_ntf_tx_cmdq_cmd_u {
	struct ipa_hwio_def_ipa_ntf_tx_cmdq_cmd_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_ntf_tx_cmdq_data_rd_0_s {
	u32	cmdq_ctx_id_f : 4;
	u32	cmdq_src_id_f : 8;
	u32	cmdq_src_pipe_f : 5;
	u32	cmdq_opcode_f : 2;
	u32	cmdq_rep_f : 1;
	u32	reserved0 : 12;
};
union ipa_hwio_def_ipa_ntf_tx_cmdq_data_rd_0_u {
	struct ipa_hwio_def_ipa_ntf_tx_cmdq_data_rd_0_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_ntf_tx_cmdq_status_s {
	u32	status : 1;
	u32	cmdq_full : 1;
	u32	cmdq_depth : 7;
	u32	reserved0 : 23;
};
union ipa_hwio_def_ipa_ntf_tx_cmdq_status_u {
	struct ipa_hwio_def_ipa_ntf_tx_cmdq_status_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_ntf_tx_cmdq_status_empty_s {
	u32	cmdq_empty : 31;
	u32	reserved0 : 1;
};
union ipa_hwio_def_ipa_ntf_tx_cmdq_status_empty_u {
	struct ipa_hwio_def_ipa_ntf_tx_cmdq_status_empty_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_ntf_tx_cmdq_count_s {
	u32	fifo_count : 7;
	u32	reserved0 : 25;
};
union ipa_hwio_def_ipa_ntf_tx_cmdq_count_u {
	struct ipa_hwio_def_ipa_ntf_tx_cmdq_count_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_prod_ackmngr_cmdq_cmd_s {
	u32	write_cmd : 1;
	u32	pop_cmd : 1;
	u32	cmd_client : 5;
	u32	rd_req : 1;
	u32	reserved0 : 24;
};
union ipa_hwio_def_ipa_prod_ackmngr_cmdq_cmd_u {
	struct ipa_hwio_def_ipa_prod_ackmngr_cmdq_cmd_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_prod_ackmngr_cmdq_data_rd_s {
	u32	cmdq_src_id : 8;
	u32	cmdq_length : 16;
	u32	cmdq_origin : 1;
	u32	cmdq_sent : 1;
	u32	cmdq_src_id_valid : 1;
	u32	cmdq_userdata : 5;
};
union ipa_hwio_def_ipa_prod_ackmngr_cmdq_data_rd_u {
	struct ipa_hwio_def_ipa_prod_ackmngr_cmdq_data_rd_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_prod_ackmngr_cmdq_status_s {
	u32	status : 1;
	u32	cmdq_full : 1;
	u32	cmdq_depth : 7;
	u32	reserved0 : 23;
};
union ipa_hwio_def_ipa_prod_ackmngr_cmdq_status_u {
	struct ipa_hwio_def_ipa_prod_ackmngr_cmdq_status_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_prod_ackmngr_cmdq_status_empty_s {
	u32	cmdq_empty : 31;
	u32	reserved0 : 1;
};
union ipa_hwio_def_ipa_prod_ackmngr_cmdq_status_empty_u {
	struct ipa_hwio_def_ipa_prod_ackmngr_cmdq_status_empty_s
		def;
	u32 value;
};
struct ipa_hwio_def_ipa_prod_ackmngr_cmdq_count_s {
	u32	fifo_count : 7;
	u32	reserved0 : 25;
};
union ipa_hwio_def_ipa_prod_ackmngr_cmdq_count_u {
	struct ipa_hwio_def_ipa_prod_ackmngr_cmdq_count_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_spare_reg_1_s {
	u32	spare_bit0 : 1;
	u32	spare_bit1 : 1;
	u32	genqmb_aooowr : 1;
	u32	spare_bit3 : 1;
	u32	spare_bit4 : 1;
	u32	acl_inorder_multi_disable : 1;
	u32	acl_dispatcher_frag_notif_check_disable : 1;
	u32	acl_dispatcher_frag_notif_check_each_cmd_disable : 1;
	u32	spare_bit8 : 1;
	u32	acl_dispatcher_frag_notif_check_notif_mid_disable : 1;
	u32	acl_dispatcher_pkt_check_disable : 1;
	u32	tx_gives_sspnd_ack_on_open_aggr_frame : 1;
	u32	spare_bit12 : 1;
	u32	tx_block_aggr_query_on_holb_packet : 1;
	u32	frag_mngr_fairness_eviction_on_constructing : 1;
	u32	rx_cmdq_splitter_cmdq_pending_mux_disable : 1;
	u32	qmb_ram_rd_cache_disable : 1;
	u32	rx_stall_on_mbim_deaggr_error : 1;
	u32	rx_stall_on_gen_deaggr_error : 1;
	u32	spare_bit19 : 1;
	u32	revert_warb_fix : 1;
	u32	gsi_if_out_of_buf_stop_reset_mask_enable : 1;
	u32	bam_idle_in_ipa_misc_cgc_en : 1;
	u32	spare_bit23 : 1;
	u32	spare_bit24 : 1;
	u32	spare_bit25 : 1;
	u32	ram_slaveway_access_protection_disable : 1;
	u32	dcph_ram_rd_prefetch_disable : 1;
	u32	warb_force_arb_round_finish_special_disable : 1;
	u32	spare_ackinj_pipe8_mask_enable : 1;
	u32	spare_bit30 : 1;
	u32	spare_bit31 : 1;
};
union ipa_hwio_def_ipa_spare_reg_1_u {
	struct ipa_hwio_def_ipa_spare_reg_1_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_spare_reg_2_s {
	u32	tx_bresp_inj_with_flop : 1;
	u32	cmdq_split_not_wait_data_desc_prior_hdr_push : 1;
	u32	spare_bits : 30;
};
union ipa_hwio_def_ipa_spare_reg_2_u {
	struct ipa_hwio_def_ipa_spare_reg_2_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_endp_gsi_cfg1_n_s {
	u32	reserved0 : 16;
	u32	endp_en : 1;
	u32	reserved1 : 14;
	u32	init_endp : 1;
};
union ipa_hwio_def_ipa_endp_gsi_cfg1_n_u {
	struct ipa_hwio_def_ipa_endp_gsi_cfg1_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_gsi_cfg_tlv_n_s {
	u32	fifo_base_addr : 16;
	u32	fifo_size : 8;
	u32	reserved0 : 8;
};
union ipa_hwio_def_ipa_endp_gsi_cfg_tlv_n_u {
	struct ipa_hwio_def_ipa_endp_gsi_cfg_tlv_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_endp_gsi_cfg_aos_n_s {
	u32	fifo_base_addr : 16;
	u32	fifo_size : 8;
	u32	reserved0 : 8;
};
union ipa_hwio_def_ipa_endp_gsi_cfg_aos_n_u {
	struct ipa_hwio_def_ipa_endp_gsi_cfg_aos_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_ctxh_ctrl_s {
	u32	ctxh_lock_id : 4;
	u32	reserved0 : 27;
	u32	ctxh_lock : 1;
};
union ipa_hwio_def_ipa_ctxh_ctrl_u {
	struct ipa_hwio_def_ipa_ctxh_ctrl_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_irq_stts_ee_n_s {
	u32	bad_snoc_access_irq : 1;
	u32	reserved0 : 1;
	u32	uc_irq_0 : 1;
	u32	uc_irq_1 : 1;
	u32	uc_irq_2 : 1;
	u32	uc_irq_3 : 1;
	u32	uc_in_q_not_empty_irq : 1;
	u32	uc_rx_cmd_q_not_full_irq : 1;
	u32	proc_to_uc_ack_q_not_empty_irq : 1;
	u32	rx_err_irq : 1;
	u32	deaggr_err_irq : 1;
	u32	tx_err_irq : 1;
	u32	step_mode_irq : 1;
	u32	proc_err_irq : 1;
	u32	tx_suspend_irq : 1;
	u32	tx_holb_drop_irq : 1;
	u32	bam_gsi_idle_irq : 1;
	u32	pipe_yellow_marker_below_irq : 1;
	u32	pipe_red_marker_below_irq : 1;
	u32	pipe_yellow_marker_above_irq : 1;
	u32	pipe_red_marker_above_irq : 1;
	u32	ucp_irq : 1;
	u32	reserved1 : 1;
	u32	gsi_ee_irq : 1;
	u32	gsi_ipa_if_tlv_rcvd_irq : 1;
	u32	gsi_uc_irq : 1;
	u32	tlv_len_min_dsm_irq : 1;
	u32	reserved2 : 5;
};
union ipa_hwio_def_ipa_irq_stts_ee_n_u {
	struct ipa_hwio_def_ipa_irq_stts_ee_n_s def;
	u32					value;
};
struct ipa_hwio_def_ipa_irq_en_ee_n_s {
	u32	bad_snoc_access_irq_en : 1;
	u32	reserved0 : 1;
	u32	uc_irq_0_irq_en : 1;
	u32	uc_irq_1_irq_en : 1;
	u32	uc_irq_2_irq_en : 1;
	u32	uc_irq_3_irq_en : 1;
	u32	uc_in_q_not_empty_irq_en : 1;
	u32	uc_rx_cmd_q_not_full_irq_en : 1;
	u32	proc_to_uc_ack_q_not_empty_irq_en : 1;
	u32	rx_err_irq_en : 1;
	u32	deaggr_err_irq_en : 1;
	u32	tx_err_irq_en : 1;
	u32	step_mode_irq_en : 1;
	u32	proc_err_irq_en : 1;
	u32	tx_suspend_irq_en : 1;
	u32	tx_holb_drop_irq_en : 1;
	u32	bam_gsi_idle_irq_en : 1;
	u32	pipe_yellow_marker_below_irq_en : 1;
	u32	pipe_red_marker_below_irq_en : 1;
	u32	pipe_yellow_marker_above_irq_en : 1;
	u32	pipe_red_marker_above_irq_en : 1;
	u32	ucp_irq_en : 1;
	u32	reserved1 : 1;
	u32	gsi_ee_irq_en : 1;
	u32	gsi_ipa_if_tlv_rcvd_irq_en : 1;
	u32	gsi_uc_irq_en : 1;
	u32	tlv_len_min_dsm_irq_en : 1;
	u32	reserved2 : 5;
};
union ipa_hwio_def_ipa_irq_en_ee_n_u {
	struct ipa_hwio_def_ipa_irq_en_ee_n_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_snoc_fec_ee_n_s {
	u32	client : 8;
	u32	qmb_index : 1;
	u32	reserved0 : 3;
	u32	tid : 4;
	u32	reserved1 : 15;
	u32	read_not_write : 1;
};
union ipa_hwio_def_ipa_snoc_fec_ee_n_u {
	struct ipa_hwio_def_ipa_snoc_fec_ee_n_s def;
	u32					value;
};
struct ipa_hwio_def_ipa_fec_addr_ee_n_s {
	u32 addr : 32;
};
union ipa_hwio_def_ipa_fec_addr_ee_n_u {
	struct ipa_hwio_def_ipa_fec_addr_ee_n_s def;
	u32					value;
};
struct ipa_hwio_def_ipa_fec_attr_ee_n_s {
	u32	opcode : 6;
	u32	error_info : 26;
};
union ipa_hwio_def_ipa_fec_attr_ee_n_u {
	struct ipa_hwio_def_ipa_fec_attr_ee_n_s def;
	u32					value;
};
struct ipa_hwio_def_ipa_suspend_irq_info_ee_n_s {
	u32	endpoints : 31;
	u32	reserved0 : 1;
};
union ipa_hwio_def_ipa_suspend_irq_info_ee_n_u {
	struct ipa_hwio_def_ipa_suspend_irq_info_ee_n_s def;
	u32						value;
};
struct ipa_hwio_def_ipa_suspend_irq_en_ee_n_s {
	u32	endpoints : 31;
	u32	reserved0 : 1;
};
union ipa_hwio_def_ipa_suspend_irq_en_ee_n_u {
	struct ipa_hwio_def_ipa_suspend_irq_en_ee_n_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_holb_drop_irq_info_ee_n_s {
	u32	reserved0 : 13;
	u32	endpoints : 18;
	u32	reserved1 : 1;
};
union ipa_hwio_def_ipa_holb_drop_irq_info_ee_n_u {
	struct ipa_hwio_def_ipa_holb_drop_irq_info_ee_n_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_log_buf_status_addr_s {
	u32 start_addr : 32;
};
union ipa_hwio_def_ipa_log_buf_status_addr_u {
	struct ipa_hwio_def_ipa_log_buf_status_addr_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_status_addr_msb_s {
	u32 start_addr : 32;
};
union ipa_hwio_def_ipa_log_buf_status_addr_msb_u {
	struct ipa_hwio_def_ipa_log_buf_status_addr_msb_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_log_buf_status_write_ptr_s {
	u32 write_addr : 32;
};
union ipa_hwio_def_ipa_log_buf_status_write_ptr_u {
	struct ipa_hwio_def_ipa_log_buf_status_write_ptr_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_log_buf_status_write_ptr_msb_s {
	u32 write_addr : 32;
};
union ipa_hwio_def_ipa_log_buf_status_write_ptr_msb_u {
	struct ipa_hwio_def_ipa_log_buf_status_write_ptr_msb_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_log_buf_status_cfg_s {
	u32	size : 16;
	u32	enable : 1;
	u32	reserved0 : 15;
};
union ipa_hwio_def_ipa_log_buf_status_cfg_u {
	struct ipa_hwio_def_ipa_log_buf_status_cfg_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_log_buf_status_ram_ptr_s {
	u32	read_ptr : 16;
	u32	write_ptr : 16;
};
union ipa_hwio_def_ipa_log_buf_status_ram_ptr_u {
	struct ipa_hwio_def_ipa_log_buf_status_ram_ptr_s	def;
	u32							value;
};
struct ipa_hwio_def_ipa_uc_qmb_sys_addr_s {
	u32 addr : 32;
};
union ipa_hwio_def_ipa_uc_qmb_sys_addr_u {
	struct ipa_hwio_def_ipa_uc_qmb_sys_addr_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_uc_qmb_sys_addr_msb_s {
	u32 addr_msb : 32;
};
union ipa_hwio_def_ipa_uc_qmb_sys_addr_msb_u {
	struct ipa_hwio_def_ipa_uc_qmb_sys_addr_msb_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_uc_qmb_local_addr_s {
	u32	addr : 18;
	u32	reserved0 : 14;
};
union ipa_hwio_def_ipa_uc_qmb_local_addr_u {
	struct ipa_hwio_def_ipa_uc_qmb_local_addr_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_uc_qmb_length_s {
	u32	length : 7;
	u32	reserved0 : 25;
};
union ipa_hwio_def_ipa_uc_qmb_length_u {
	struct ipa_hwio_def_ipa_uc_qmb_length_s def;
	u32					value;
};
struct ipa_hwio_def_ipa_uc_qmb_trigger_s {
	u32	direction : 1;
	u32	reserved0 : 3;
	u32	posting : 2;
	u32	reserved1 : 26;
};
union ipa_hwio_def_ipa_uc_qmb_trigger_u {
	struct ipa_hwio_def_ipa_uc_qmb_trigger_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_uc_qmb_pending_tid_s {
	u32	tid : 6;
	u32	reserved0 : 2;
	u32	error_bus : 1;
	u32	reserved1 : 3;
	u32	error_max_os : 1;
	u32	reserved2 : 3;
	u32	error_max_comp : 1;
	u32	reserved3 : 3;
	u32	error_security : 1;
	u32	reserved4 : 11;
};
union ipa_hwio_def_ipa_uc_qmb_pending_tid_u {
	struct ipa_hwio_def_ipa_uc_qmb_pending_tid_s	def;
	u32						value;
};
struct ipa_hwio_def_ipa_uc_qmb_completed_rd_fifo_peek_s {
	u32	tid : 6;
	u32	reserved0 : 2;
	u32	error : 1;
	u32	reserved1 : 3;
	u32	valid : 1;
	u32	reserved2 : 19;
};
union ipa_hwio_def_ipa_uc_qmb_completed_rd_fifo_peek_u {
	struct ipa_hwio_def_ipa_uc_qmb_completed_rd_fifo_peek_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_uc_qmb_completed_wr_fifo_peek_s {
	u32	tid : 6;
	u32	reserved0 : 2;
	u32	error : 1;
	u32	reserved1 : 3;
	u32	valid : 1;
	u32	reserved2 : 19;
};
union ipa_hwio_def_ipa_uc_qmb_completed_wr_fifo_peek_u {
	struct ipa_hwio_def_ipa_uc_qmb_completed_wr_fifo_peek_s def;
	u32							value;
};
struct ipa_hwio_def_ipa_uc_qmb_misc_s {
	u32	user : 10;
	u32	reserved0 : 2;
	u32	rd_priority : 2;
	u32	reserved1 : 2;
	u32	wr_priority : 2;
	u32	reserved2 : 2;
	u32	ooord : 1;
	u32	reserved3 : 3;
	u32	ooowr : 1;
	u32	reserved4 : 3;
	u32	swap : 1;
	u32	irq_coal : 1;
	u32	posted_stall : 1;
	u32	qmb_hready_bcr : 1;
};
union ipa_hwio_def_ipa_uc_qmb_misc_u {
	struct ipa_hwio_def_ipa_uc_qmb_misc_s	def;
	u32					value;
};
struct ipa_hwio_def_ipa_uc_qmb_status_s {
	u32	max_outstanding_rd : 4;
	u32	outstanding_rd_cnt : 4;
	u32	completed_rd_cnt : 4;
	u32	completed_rd_fifo_full : 1;
	u32	reserved0 : 3;
	u32	max_outstanding_wr : 4;
	u32	outstanding_wr_cnt : 4;
	u32	completed_wr_cnt : 4;
	u32	completed_wr_fifo_full : 1;
	u32	reserved1 : 3;
};
union ipa_hwio_def_ipa_uc_qmb_status_u {
	struct ipa_hwio_def_ipa_uc_qmb_status_s def;
	u32					value;
};
struct ipa_hwio_def_ipa_uc_qmb_bus_attrib_s {
	u32	memtype : 3;
	u32	reserved0 : 1;
	u32	noallocate : 1;
	u32	reserved1 : 3;
	u32	innershared : 1;
	u32	reserved2 : 3;
	u32	shared : 1;
	u32	reserved3 : 19;
};
union ipa_hwio_def_ipa_uc_qmb_bus_attrib_u {
	struct ipa_hwio_def_ipa_uc_qmb_bus_attrib_s	def;
	u32						value;
};
#endif
