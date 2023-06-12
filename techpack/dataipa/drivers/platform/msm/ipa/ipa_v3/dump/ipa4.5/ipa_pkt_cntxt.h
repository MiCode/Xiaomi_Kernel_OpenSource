/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */
#if !defined(_IPA_PKT_CNTXT_H_)
#define _IPA_PKT_CNTXT_H_

#define IPA_HW_PKT_CTNTX_MAX        0x10
#define IPA_HW_NUM_SAVE_PKT_CTNTX   0x8
#define IPA_HW_PKT_CTNTX_START_ADDR 0xE434CA00
#define IPA_HW_PKT_CTNTX_SIZE       (sizeof(ipa_pkt_ctntx_opcode_state_s) + \
				     sizeof(ipa_pkt_ctntx_u))

/*
 * Packet Context States
 */
enum ipa_hw_pkt_cntxt_state_e {
	IPA_HW_PKT_CNTXT_STATE_HFETCHER_INIT = 1,
	IPA_HW_PKT_CNTXT_STATE_HFETCHER_DMAR,
	IPA_HW_PKT_CNTXT_STATE_HFETCHER_DMAR_REP,
	IPA_HW_PKT_CNTXT_STATE_H_DCPH,
	IPA_HW_PKT_CNTXT_STATE_PKT_PARSER,
	IPA_HW_PKT_CNTXT_STATE_FILTER_NAT,
	IPA_HW_PKT_CNTXT_STATE_ROUTER,
	IPA_HW_PKT_CNTXT_STATE_HDRI,
	IPA_HW_PKT_CNTXT_STATE_UCP,
	IPA_HW_PKT_CNTXT_STATE_ENQUEUER,
	IPA_HW_PKT_CNTXT_STATE_DFETCHER,
	IPA_HW_PKT_CNTXT_STATE_D_DCPH,
	IPA_HW_PKT_CNTXT_STATE_DISPATCHER,
	IPA_HW_PKT_CNTXT_STATE_TX,
	IPA_HW_PKT_CNTXT_STATE_TX_ZLT,
	IPA_HW_PKT_CNTXT_STATE_DFETCHER_DMAR,
	IPA_HW_PKT_CNTXT_STATE_DCMP,
};

/*
 * Packet Context fields as received from VI/Design
 */
struct ipa_pkt_ctntx_s {
	u64	opcode                           : 8;
	u64	state                            : 5;
	u64	not_used_1                       : 2;
	u64	tx_pkt_dma_done                  : 1;
	u64	exc_deagg                        : 1;
	u64	exc_pkt_version                  : 1;
	u64	exc_pkt_len                      : 1;
	u64	exc_threshold                    : 1;
	u64	exc_sw                           : 1;
	u64	exc_nat                          : 1;
	u64	exc_frag_miss                    : 1;
	u64	filter_bypass                    : 1;
	u64	router_bypass                    : 1;
	u64	nat_bypass                       : 1;
	u64	hdri_bypass                      : 1;
	u64	dcph_bypass                      : 1;
	u64	security_credentials_select      : 1;
	u64	pkt_2nd_pass                     : 1;
	u64	xlat_bypass                      : 1;
	u64	dcph_valid                       : 1;
	u64	ucp_on                           : 1;
	u64	replication                      : 1;
	u64	src_status_en                    : 1;
	u64	dest_status_en                   : 1;
	u64	frag_status_en                   : 1;
	u64	eot_dest                         : 1;
	u64	eot_notif                        : 1;
	u64	prev_eot_dest                    : 1;
	u64	src_hdr_len                      : 8;
	u64	tx_valid_sectors                 : 8;
	u64	rx_flags                         : 8;
	u64	rx_packet_length                 : 16;
	u64	revised_packet_length            : 16;
	u64	frag_en                          : 1;
	u64	frag_bypass                      : 1;
	u64	frag_process                     : 1;
	u64	notif_pipe                       : 5;
	u64	src_id                           : 8;
	u64	tx_pkt_transferred               : 1;
	u64	src_pipe                         : 5;
	u64	dest_pipe                        : 5;
	u64	frag_pipe                        : 5;
	u64	ihl_offset                       : 8;
	u64	protocol                         : 8;
	u64	tos                              : 8;
	u64	id                               : 16;
	u64	v6_reserved                      : 4;
	u64	ff                               : 1;
	u64	mf                               : 1;
	u64	pkt_israg                        : 1;
	u64	tx_holb_timer_overflow           : 1;
	u64	tx_holb_timer_running            : 1;
	u64	trnseq_0                         : 3;
	u64	trnseq_1                         : 3;
	u64	trnseq_2                         : 3;
	u64	trnseq_3                         : 3;
	u64	trnseq_4                         : 3;
	u64	trnseq_ex_length                 : 8;
	u64	trnseq_4_length                  : 8;
	u64	trnseq_4_offset                  : 8;
	u64	dps_tx_pop_cnt                   : 2;
	u64	dps_tx_push_cnt                  : 2;
	u64	vol_ic_dcph_cfg                  : 1;
	u64	vol_ic_tag_stts                  : 1;
	u64	vol_ic_pxkt_init_e               : 1;
	u64	vol_ic_pkt_init                  : 1;
	u64	tx_holb_counter                  : 32;
	u64	trnseq_0_length                  : 8;
	u64	trnseq_0_offset                  : 8;
	u64	trnseq_1_length                  : 8;
	u64	trnseq_1_offset                  : 8;
	u64	trnseq_2_length                  : 8;
	u64	trnseq_2_offset                  : 8;
	u64	trnseq_3_length                  : 8;
	u64	trnseq_3_offset                  : 8;
	u64	dmar_valid_length                : 16;
	u64	dcph_valid_length                : 16;
	u64	frag_hdr_offset                  : 9;
	u64	ip_payload_offset                : 9;
	u64	frag_rule                        : 4;
	u64	frag_table                       : 1;
	u64	frag_hit                         : 1;
	u64	data_cmdq_ptr                    : 8;
	u64	filter_result                    : 6;
	u64	router_result                    : 6;
	u64	nat_result                       : 6;
	u64	hdri_result                      : 6;
	u64	dcph_result                      : 6;
	u64	dcph_result_valid                : 1;
	u32	not_used_2                       : 4;
	u64	tx_pkt_suspended                 : 1;
	u64	tx_pkt_dropped                   : 1;
	u32	not_used_3                       : 3;
	u64	metadata_valid                   : 1;
	u64	metadata_type                    : 4;
	u64	ul_cs_start_diff                 : 9;
	u64	cs_disable_trlr_vld_bit          : 1;
	u64	cs_required                      : 1;
	u64	dest_hdr_len                     : 8;
	u64	fr_l                             : 1;
	u64	fl_h                             : 1;
	u64	fr_g                             : 1;
	u64	fr_ret                           : 1;
	u64	fr_rule_id                       : 10;
	u64	rt_l                             : 1;
	u64	rt_h                             : 1;
	u64	rtng_tbl_index                   : 5;
	u64	rt_match                         : 1;
	u64	rt_rule_id                       : 10;
	u64	nat_tbl_index                    : 13;
	u64	nat_type                         : 2;
	u64	hdr_l                            : 1;
	u64	header_offset                    : 10;
	u64	not_used_4                       : 1;
	u64	filter_result_valid              : 1;
	u64	router_result_valid              : 1;
	u64	nat_result_valid                 : 1;
	u64	hdri_result_valid                : 1;
	u64	not_used_5                       : 1;
	u64	stream_id                        : 8;
	u64	not_used_6                       : 6;
	u64	dcph_context_index               : 2;
	u64	dcph_cfg_size                    : 16;
	u64	dcph_cfg_count                   : 32;
	u64	tag_info                         : 48;
	u64	ucp_cmd_id                       : 16;
	u64	metadata                         : 32;
	u64	ucp_cmd_params                   : 32;
	u64	nat_ip_address                   : 32;
	u64	nat_ip_cs_diff                   : 16;
	u64	frag_dest_pipe                   : 5;
	u64	frag_nat_type                    : 2;
	u64	fragr_ret                        : 1;
	u64	frag_protocol                    : 8;
	u64	src_ip_address                   : 32;
	u64	dest_ip_address                  : 32;
	u64	not_used_7                       : 37;
	u64	frag_hdr_l                       : 1;
	u64	frag_header_offset               : 10;
	u64	frag_id                          : 16;
} __packed;

#endif /* #if !defined(_IPA_PKT_CNTXT_H_) */
