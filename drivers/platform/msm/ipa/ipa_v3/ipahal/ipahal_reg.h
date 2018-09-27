/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#ifndef _IPAHAL_REG_H_
#define _IPAHAL_REG_H_

#include <linux/ipa.h>

/*
 * Registers names
 *
 * NOTE:: Any change to this enum, need to change to ipareg_name_to_str
 *	array as well.
 */
enum ipahal_reg_name {
	IPA_ROUTE,
	IPA_IRQ_STTS_EE_n,
	IPA_IRQ_EN_EE_n,
	IPA_IRQ_CLR_EE_n,
	IPA_SUSPEND_IRQ_INFO_EE_n,
	IPA_SUSPEND_IRQ_EN_EE_n,
	IPA_SUSPEND_IRQ_CLR_EE_n,
	IPA_HOLB_DROP_IRQ_INFO_EE_n,
	IPA_HOLB_DROP_IRQ_EN_EE_n,
	IPA_HOLB_DROP_IRQ_CLR_EE_n,
	IPA_BCR,
	IPA_ENABLED_PIPES,
	IPA_VERSION,
	IPA_TAG_TIMER,
	IPA_NAT_TIMER,
	IPA_COMP_HW_VERSION,
	IPA_COMP_CFG,
	IPA_STATE_TX_WRAPPER,
	IPA_STATE_TX1,
	IPA_STATE_FETCHER,
	IPA_STATE_FETCHER_MASK,
	IPA_STATE_FETCHER_MASK_0,
	IPA_STATE_FETCHER_MASK_1,
	IPA_STATE_DFETCHER,
	IPA_STATE_ACL,
	IPA_STATE,
	IPA_STATE_RX_ACTIVE,
	IPA_STATE_TX0,
	IPA_STATE_AGGR_ACTIVE,
	IPA_COUNTER_CFG,
	IPA_STATE_GSI_TLV,
	IPA_STATE_GSI_AOS,
	IPA_STATE_GSI_IF,
	IPA_STATE_GSI_SKIP,
	IPA_STATE_GSI_IF_CONS,
	IPA_STATE_DPL_FIFO,
	IPA_STATE_COAL_MASTER,
	IPA_GENERIC_RAM_ARBITER_PRIORITY,
	IPA_STATE_NLO_AGGR,
	IPA_STATE_COAL_MASTER_1,
	IPA_ENDP_INIT_HDR_n,
	IPA_ENDP_INIT_HDR_EXT_n,
	IPA_ENDP_INIT_AGGR_n,
	IPA_AGGR_FORCE_CLOSE,
	IPA_ENDP_INIT_ROUTE_n,
	IPA_ENDP_INIT_MODE_n,
	IPA_ENDP_INIT_NAT_n,
	IPA_ENDP_INIT_CONN_TRACK_n,
	IPA_ENDP_INIT_CTRL_n,
	IPA_ENDP_INIT_CTRL_SCND_n,
	IPA_ENDP_INIT_CTRL_STATUS_n,
	IPA_ENDP_INIT_HOL_BLOCK_EN_n,
	IPA_ENDP_INIT_HOL_BLOCK_TIMER_n,
	IPA_ENDP_INIT_DEAGGR_n,
	IPA_ENDP_INIT_SEQ_n,
	IPA_DEBUG_CNT_REG_n,
	IPA_ENDP_INIT_CFG_n,
	IPA_IRQ_EE_UC_n,
	IPA_ENDP_INIT_HDR_METADATA_MASK_n,
	IPA_ENDP_INIT_HDR_METADATA_n,
	IPA_ENDP_INIT_PROD_CFG_n,
	IPA_ENDP_INIT_RSRC_GRP_n,
	IPA_SHARED_MEM_SIZE,
	IPA_SW_AREA_RAM_DIRECT_ACCESS_n,
	IPA_DEBUG_CNT_CTRL_n,
	IPA_UC_MAILBOX_m_n,
	IPA_FILT_ROUT_HASH_FLUSH,
	IPA_FILT_ROUT_HASH_EN,
	IPA_SINGLE_NDP_MODE,
	IPA_QCNCM,
	IPA_SYS_PKT_PROC_CNTXT_BASE,
	IPA_LOCAL_PKT_PROC_CNTXT_BASE,
	IPA_ENDP_STATUS_n,
	IPA_ENDP_YELLOW_RED_MARKER_CFG_n,
	IPA_ENDP_FILTER_ROUTER_HSH_CFG_n,
	IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
	IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n,
	IPA_SRC_RSRC_GRP_45_RSRC_TYPE_n,
	IPA_SRC_RSRC_GRP_67_RSRC_TYPE_n,
	IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
	IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
	IPA_DST_RSRC_GRP_45_RSRC_TYPE_n,
	IPA_DST_RSRC_GRP_67_RSRC_TYPE_n,
	IPA_RX_HPS_CLIENTS_MIN_DEPTH_0,
	IPA_RX_HPS_CLIENTS_MIN_DEPTH_1,
	IPA_RX_HPS_CLIENTS_MAX_DEPTH_0,
	IPA_RX_HPS_CLIENTS_MAX_DEPTH_1,
	IPA_HPS_FTCH_ARB_QUEUE_WEIGHT,
	IPA_QSB_MAX_WRITES,
	IPA_QSB_MAX_READS,
	IPA_TX_CFG,
	IPA_IDLE_INDICATION_CFG,
	IPA_DPS_SEQUENCER_FIRST,
	IPA_DPS_SEQUENCER_LAST,
	IPA_HPS_SEQUENCER_FIRST,
	IPA_HPS_SEQUENCER_LAST,
	IPA_CLKON_CFG,
	IPA_QTIME_TIMESTAMP_CFG,
	IPA_TIMERS_PULSE_GRAN_CFG,
	IPA_TIMERS_XO_CLK_DIV_CFG,
	IPA_STAT_QUOTA_BASE_n,
	IPA_STAT_QUOTA_MASK_n,
	IPA_STAT_TETHERING_BASE_n,
	IPA_STAT_TETHERING_MASK_n,
	IPA_STAT_FILTER_IPV4_BASE,
	IPA_STAT_FILTER_IPV6_BASE,
	IPA_STAT_ROUTER_IPV4_BASE,
	IPA_STAT_ROUTER_IPV6_BASE,
	IPA_STAT_FILTER_IPV4_START_ID,
	IPA_STAT_FILTER_IPV6_START_ID,
	IPA_STAT_ROUTER_IPV4_START_ID,
	IPA_STAT_ROUTER_IPV6_START_ID,
	IPA_STAT_FILTER_IPV4_END_ID,
	IPA_STAT_FILTER_IPV6_END_ID,
	IPA_STAT_ROUTER_IPV4_END_ID,
	IPA_STAT_ROUTER_IPV6_END_ID,
	IPA_STAT_DROP_CNT_BASE_n,
	IPA_STAT_DROP_CNT_MASK_n,
	IPA_SNOC_FEC_EE_n,
	IPA_FEC_ADDR_EE_n,
	IPA_FEC_ADDR_MSB_EE_n,
	IPA_FEC_ATTR_EE_n,
	IPA_ENDP_GSI_CFG1_n,
	IPA_ENDP_GSI_CFG_AOS_n,
	IPA_ENDP_GSI_CFG_TLV_n,
	IPA_REG_MAX,
};

/*
 * struct ipahal_reg_route - IPA route register
 * @route_dis: route disable
 * @route_def_pipe: route default pipe
 * @route_def_hdr_table: route default header table
 * @route_def_hdr_ofst: route default header offset table
 * @route_frag_def_pipe: Default pipe to route fragmented exception
 *    packets and frag new rule statues, if source pipe does not have
 *    a notification status pipe defined.
 * @route_def_retain_hdr: default value of retain header. It is used
 *    when no rule was hit
 */
struct ipahal_reg_route {
	u32 route_dis;
	u32 route_def_pipe;
	u32 route_def_hdr_table;
	u32 route_def_hdr_ofst;
	u8  route_frag_def_pipe;
	u32 route_def_retain_hdr;
};

/*
 * struct ipahal_reg_endp_init_route - IPA ENDP_INIT_ROUTE_n register
 * @route_table_index: Default index of routing table (IPA Consumer).
 */
struct ipahal_reg_endp_init_route {
	u32 route_table_index;
};

/*
 * struct ipahal_reg_endp_init_rsrc_grp - IPA_ENDP_INIT_RSRC_GRP_n register
 * @rsrc_grp: Index of group for this ENDP. If this ENDP is a source-ENDP,
 *	index is for source-resource-group. If destination ENPD, index is
 *	for destination-resoruce-group.
 */
struct ipahal_reg_endp_init_rsrc_grp {
	u32 rsrc_grp;
};

/*
 * struct ipahal_reg_endp_init_mode - IPA ENDP_INIT_MODE_n register
 * @dst_pipe_number: This parameter specifies destination output-pipe-packets
 *	will be routed to. Valid for DMA mode only and for Input
 *	Pipes only (IPA Consumer)
 */
struct ipahal_reg_endp_init_mode {
	u32 dst_pipe_number;
	struct ipa_ep_cfg_mode ep_mode;
};

/*
 * struct ipahal_reg_shared_mem_size - IPA_SHARED_MEM_SIZE register
 * @shared_mem_sz: Available size [in 8Bytes] of SW partition within
 *	IPA shared memory.
 * @shared_mem_baddr: Offset of SW partition within IPA
 *	shared memory[in 8Bytes]. To get absolute address of SW partition,
 *	add this offset to IPA_SW_AREA_RAM_DIRECT_ACCESS_n baddr.
 */
struct ipahal_reg_shared_mem_size {
	u32 shared_mem_sz;
	u32 shared_mem_baddr;
};

/*
 * struct ipahal_reg_ep_cfg_status - status configuration in IPA end-point
 * @status_en: Determines if end point supports Status Indications. SW should
 *	set this bit in order to enable Statuses. Output Pipe - send
 *	Status indications only if bit is set. Input Pipe - forward Status
 *	indication to STATUS_ENDP only if bit is set. Valid for Input
 *	and Output Pipes (IPA Consumer and Producer)
 * @status_ep: Statuses generated for this endpoint will be forwarded to the
 *	specified Status End Point. Status endpoint needs to be
 *	configured with STATUS_EN=1 Valid only for Input Pipes (IPA
 *	Consumer)
 * @status_location: Location of PKT-STATUS on destination pipe.
 *	If set to 0 (default), PKT-STATUS will be appended before the packet
 *	for this endpoint. If set to 1, PKT-STATUS will be appended after the
 *	packet for this endpoint. Valid only for Output Pipes (IPA Producer)
 * @status_pkt_suppress: Disable notification status, when statistics is enabled
 */
struct ipahal_reg_ep_cfg_status {
	bool status_en;
	u8 status_ep;
	bool status_location;
	u8 status_pkt_suppress;
};

/*
 * struct ipahal_reg_clkon_cfg-  Enables SW bypass clock-gating for the IPA core
 *
 * @all: Enables SW bypass clock-gating controls for this sub-module;
 *	0: CGC is enabled by internal logic, 1: No CGC (clk is always 'ON').
 *	sub-module affected is based on var name -> ex: open_rx refers
 *	to IPA_RX sub-module and open_global refers to global IPA 1x clock
 */
struct ipahal_reg_clkon_cfg {
	bool open_dpl_fifo;
	bool open_global_2x_clk;
	bool open_global;
	bool open_gsi_if;
	bool open_weight_arb;
	bool open_qmb;
	bool open_ram_slaveway;
	bool open_aggr_wrapper;
	bool open_qsb2axi_cmdq_l;
	bool open_fnr;
	bool open_tx_1;
	bool open_tx_0;
	bool open_ntf_tx_cmdqs;
	bool open_dcmp;
	bool open_h_dcph;
	bool open_d_dcph;
	bool open_ack_mngr;
	bool open_ctx_handler;
	bool open_rsrc_mngr;
	bool open_dps_tx_cmdqs;
	bool open_hps_dps_cmdqs;
	bool open_rx_hps_cmdqs;
	bool open_dps;
	bool open_hps;
	bool open_ftch_dps;
	bool open_ftch_hps;
	bool open_ram_arb;
	bool open_misc;
	bool open_tx_wrapper;
	bool open_proc;
	bool open_rx;
};

/*
 * struct ipahal_reg_qtime_timestamp_cfg - IPA timestamp configuration
 *  Relevant starting IPA 4.5.
 *  IPA timestamps are based on QTIMER which is 56bit length which is
 *  based on XO clk running at 19.2MHz (52nsec resolution).
 *  Specific timestamps (TAG, NAT, DPL) my require lower resolution.
 *  This can be achieved by omitting LSB bits from 56bit QTIMER.
 *  e.g. if we omit (shift) 24 bit then we get (2^24)*(52n)=0.87sec resolution.
 *
 * @dpl_timestamp_lsb: Shifting Qtime value. Value will be used as LSB of
 *  DPL timestamp.
 * @dpl_timestamp_sel: if false, DPL timestamp will be based on legacy
 *  DPL_TIMER which counts in 1ms. if true, it will be based on QTIME
 *  value shifted by dpl_timestamp_lsb.
 * @tag_timestamp_lsb: Shifting Qtime value. Value will be used as LSB of
 *  TAG timestamp.
 * @nat_timestamp_lsb: Shifting Qtime value. Value will be used as LSB of
 *  NAT timestamp.
 */
struct ipahal_reg_qtime_timestamp_cfg {
	u32 dpl_timestamp_lsb;
	bool dpl_timestamp_sel;
	u32 tag_timestamp_lsb;
	u32 nat_timestamp_lsb;
};

/*
 * enum ipa_timers_time_gran_type - Time granularity to be used with timers
 *
 * e.g. for HOLB and Aggregation timers
 */
enum ipa_timers_time_gran_type {
	IPA_TIMERS_TIME_GRAN_10_USEC,
	IPA_TIMERS_TIME_GRAN_20_USEC,
	IPA_TIMERS_TIME_GRAN_50_USEC,
	IPA_TIMERS_TIME_GRAN_100_USEC,
	IPA_TIMERS_TIME_GRAN_1_MSEC,
	IPA_TIMERS_TIME_GRAN_10_MSEC,
	IPA_TIMERS_TIME_GRAN_100_MSEC,
	IPA_TIMERS_TIME_GRAN_NEAR_HALF_SEC, /* 0.65536s */
	IPA_TIMERS_TIME_GRAN_MAX,
};

/*
 * struct ipahal_reg_timers_pulse_gran_cfg - Counters tick granularities
 *  Relevant starting IPA 4.5.
 *  IPA timers are based on XO CLK running 19.2MHz (52ns resolution) deviced
 *  by clock divider (see IPA_TIMERS_XO_CLK_DIV_CFG) - default 100Khz (10usec).
 *  IPA timers instances (e.g. HOLB or AGGR) may require different resolutions.
 *  There are 3 global pulse generators with configurable granularity. Each
 *  timer instance can choose one of the three generators to work with.
 *  Each generator granularity can be one of supported ones.
 *
 * @gran_X: granularity tick of counterX
 */
struct ipahal_reg_timers_pulse_gran_cfg {
	enum ipa_timers_time_gran_type gran_0;
	enum ipa_timers_time_gran_type gran_1;
	enum ipa_timers_time_gran_type gran_2;
};

/*
 * struct ipahal_reg_timers_xo_clk_div_cfg - IPA timers clock divider
 * Used to control clock divider which gets XO_CLK of 19.2MHz as input.
 * Output of CDIV is used to generate IPA timers granularity
 *
 * @enable: Enable of the clock divider for all IPA and GSI timers.
 *  clock is disabled by default, and need to be enabled when system is up.
 * @value: Divided value to be used by CDIV. POR value is set to 191
 *  to generate 100KHz clk based on XO_CLK.
 *  Values of ipahal_reg_timers_pulse_gran_cfg are based on this default.
 */
struct ipahal_reg_timers_xo_clk_div_cfg {
	bool enable;
	u32 value;
};

/*
 * struct ipahal_reg_comp_cfg- IPA Core QMB/Master Port selection
 *
 * @enable / @ipa_dcmp_fast_clk_en: are not relevant starting IPA4.5
 * @ipa_full_flush_wait_rsc_closure_en: relevant starting IPA4.5
 */
struct ipahal_reg_comp_cfg {
	bool ipa_full_flush_wait_rsc_closure_en;
	u8 ipa_atomic_fetcher_arb_lock_dis;
	bool ipa_qmb_select_by_address_global_en;
	bool gsi_multi_axi_masters_dis;
	bool gsi_snoc_cnoc_loop_protection_disable;
	bool gen_qmb_0_snoc_cnoc_loop_protection_disable;
	bool gen_qmb_1_multi_inorder_wr_dis;
	bool gen_qmb_0_multi_inorder_wr_dis;
	bool gen_qmb_1_multi_inorder_rd_dis;
	bool gen_qmb_0_multi_inorder_rd_dis;
	bool gsi_multi_inorder_wr_dis;
	bool gsi_multi_inorder_rd_dis;
	bool ipa_qmb_select_by_address_prod_en;
	bool ipa_qmb_select_by_address_cons_en;
	bool ipa_dcmp_fast_clk_en;
	bool gen_qmb_1_snoc_bypass_dis;
	bool gen_qmb_0_snoc_bypass_dis;
	bool gsi_snoc_bypass_dis;
	bool enable;
};

/*
 * struct ipa_hash_tuple - Hash tuple members for flt and rt
 *  the fields tells if to be masked or not
 * @src_id: pipe number for flt, table index for rt
 * @src_ip_addr: IP source address
 * @dst_ip_addr: IP destination address
 * @src_port: L4 source port
 * @dst_port: L4 destination port
 * @protocol: IP protocol field
 * @meta_data: packet meta-data
 *
 */
struct ipahal_reg_hash_tuple {
	/* src_id: pipe in flt, tbl index in rt */
	bool src_id;
	bool src_ip_addr;
	bool dst_ip_addr;
	bool src_port;
	bool dst_port;
	bool protocol;
	bool meta_data;
};

/*
 * struct ipahal_reg_fltrt_hash_tuple - IPA hash tuple register
 * @flt: Hash tuple info for filtering
 * @rt: Hash tuple info for routing
 * @undefinedX: Undefined/Unused bit fields set of the register
 */
struct ipahal_reg_fltrt_hash_tuple {
	struct ipahal_reg_hash_tuple flt;
	struct ipahal_reg_hash_tuple rt;
	u32 undefined1;
	u32 undefined2;
};

/*
 * enum ipahal_reg_dbg_cnt_type - Debug Counter Type
 * DBG_CNT_TYPE_IPV4_FLTR - Count IPv4 filtering rules
 * DBG_CNT_TYPE_IPV4_ROUT - Count IPv4 routing rules
 * DBG_CNT_TYPE_GENERAL - General counter
 * DBG_CNT_TYPE_IPV6_FLTR - Count IPv6 filtering rules
 * DBG_CNT_TYPE_IPV4_ROUT - Count IPv6 routing rules
 */
enum ipahal_reg_dbg_cnt_type {
	DBG_CNT_TYPE_IPV4_FLTR,
	DBG_CNT_TYPE_IPV4_ROUT,
	DBG_CNT_TYPE_GENERAL,
	DBG_CNT_TYPE_IPV6_FLTR,
	DBG_CNT_TYPE_IPV6_ROUT,
};

/*
 * struct ipahal_reg_debug_cnt_ctrl - IPA_DEBUG_CNT_CTRL_n register
 * @en - Enable debug counter
 * @type - Type of debugging couting
 * @product - False->Count Bytes . True->Count #packets
 * @src_pipe - Specific Pipe to match. If FF, no need to match
 *	specific pipe
 * @rule_idx_pipe_rule - Global Rule or Pipe Rule. If pipe, then indicated by
 *	src_pipe. Starting at IPA V3_5,
 *	no support on Global Rule. This field will be ignored.
 * @rule_idx - Rule index. Irrelevant for type General
 */
struct ipahal_reg_debug_cnt_ctrl {
	bool en;
	enum ipahal_reg_dbg_cnt_type type;
	bool product;
	u8 src_pipe;
	bool rule_idx_pipe_rule;
	u16 rule_idx;
};

/*
 * struct ipahal_reg_rsrc_grp_cfg - Min/Max values for two rsrc groups
 * @x_min - first group min value
 * @x_max - first group max value
 * @y_min - second group min value
 * @y_max - second group max value
 */
struct ipahal_reg_rsrc_grp_cfg {
	u32 x_min;
	u32 x_max;
	u32 y_min;
	u32 y_max;
};

/*
 * struct ipahal_reg_rx_hps_clients - Min or Max values for RX HPS clients
 * @client_minmax - Min or Max values. In case of depth 0 the 4 or 5 values
 *	are used. In case of depth 1, only the first 2 values are used
 */
struct ipahal_reg_rx_hps_clients {
	u32 client_minmax[5];
};

/*
 * struct ipahal_reg_rx_hps_weights - weight values for RX HPS clients
 * @hps_queue_weight_0 - 4 bit Weight for RX_HPS_CMDQ #0 (3:0)
 * @hps_queue_weight_1 - 4 bit Weight for RX_HPS_CMDQ #1 (7:4)
 * @hps_queue_weight_2 - 4 bit Weight for RX_HPS_CMDQ #2 (11:8)
 * @hps_queue_weight_3 - 4 bit Weight for RX_HPS_CMDQ #3 (15:12)
 */
struct ipahal_reg_rx_hps_weights {
	u32 hps_queue_weight_0;
	u32 hps_queue_weight_1;
	u32 hps_queue_weight_2;
	u32 hps_queue_weight_3;
};

/*
 * struct ipahal_reg_counter_cfg - granularity of counter registers
 * @aggr_granularity  -Defines the granularity of AGGR timers
 *	granularity [msec]=(x+1)/(32)
 */
struct ipahal_reg_counter_cfg {
	enum {
		GRAN_VALUE_125_USEC = 3,
		GRAN_VALUE_250_USEC = 7,
		GRAN_VALUE_500_USEC = 15,
		GRAN_VALUE_MSEC = 31,
	} aggr_granularity;
};


/*
 * struct ipahal_reg_valmask - holding values and masking for registers
 *	HAL application may require only value and mask of it for some
 *	register fields.
 * @val - The value
 * @mask - Tha mask of the value
 */
struct ipahal_reg_valmask {
	u32 val;
	u32 mask;
};

/*
 * struct ipahal_reg_fltrt_hash_flush - Flt/Rt flush configuration
 * @v6_rt - Flush IPv6 Routing cache
 * @v6_flt - Flush IPv6 Filtering cache
 * @v4_rt - Flush IPv4 Routing cache
 * @v4_flt - Flush IPv4 Filtering cache
 */
struct ipahal_reg_fltrt_hash_flush {
	bool v6_rt;
	bool v6_flt;
	bool v4_rt;
	bool v4_flt;
};

/*
 * struct ipahal_reg_single_ndp_mode - IPA SINGLE_NDP_MODE register
 * @single_ndp_en: When set to '1', IPA builds MBIM frames with up to 1
 *	NDP-header.
 * @unused: undefined bits of the register
 */
struct ipahal_reg_single_ndp_mode {
	bool single_ndp_en;
	u32 undefined;
};

/*
 * struct ipahal_reg_qcncm - IPA QCNCM register
 * @mode_en: When QCNCM_MODE_EN=1, IPA will use QCNCM signature.
 * @mode_val: Used only when QCNCM_MODE_EN=1 and sets SW Signature in
 *	the NDP header.
 * @unused: undefined bits of the register
 */
struct ipahal_reg_qcncm {
	bool mode_en;
	u32 mode_val;
	u32 undefined;
};

/*
 * struct ipahal_reg_qsb_max_writes - IPA QSB Max Writes register
 * @qmb_0_max_writes: Max number of outstanding writes for GEN_QMB_0
 * @qmb_1_max_writes: Max number of outstanding writes for GEN_QMB_1
 */
struct ipahal_reg_qsb_max_writes {
	u32 qmb_0_max_writes;
	u32 qmb_1_max_writes;
};

/*
 * struct ipahal_reg_qsb_max_reads - IPA QSB Max Reads register
 * @qmb_0_max_reads: Max number of outstanding reads for GEN_QMB_0
 * @qmb_1_max_reads: Max number of outstanding reads for GEN_QMB_1
 * @qmb_0_max_read_beats: Max number of outstanding read beats for GEN_QMB_0
 * @qmb_1_max_read_beats: Max number of outstanding read beats for GEN_QMB_1
 */
struct ipahal_reg_qsb_max_reads {
	u32 qmb_0_max_reads;
	u32 qmb_1_max_reads;
	u32 qmb_0_max_read_beats;
	u32 qmb_1_max_read_beats;
};

/*
 * struct ipahal_reg_tx_cfg - IPA TX_CFG register
 * @tx0_prefetch_disable: Disable prefetch on TX0
 * @tx1_prefetch_disable: Disable prefetch on TX1
 * @tx0_prefetch_almost_empty_size: Prefetch almost empty size on TX0
 * @tx1_prefetch_almost_empty_size: Prefetch almost empty size on TX1
 * @dmaw_scnd_outsd_pred_threshold: threshold for DMAW_SCND_OUTSD_PRED_EN
 * @dmaw_max_beats_256_dis:
 * @dmaw_scnd_outsd_pred_en:
 * @pa_mask_en:
 * @dual_tx_enable: When 1 TX0 and TX1 are enabled. When 0 only TX0 is enabled
 *  Relevant starting IPA4.5
 */
struct ipahal_reg_tx_cfg {
	bool tx0_prefetch_disable;
	bool tx1_prefetch_disable;
	u32 tx0_prefetch_almost_empty_size;
	u32 tx1_prefetch_almost_empty_size;
	u32 dmaw_scnd_outsd_pred_threshold;
	u32 dmaw_max_beats_256_dis;
	u32 dmaw_scnd_outsd_pred_en;
	u32 pa_mask_en;
	bool dual_tx_enable;
};

/*
 * struct ipahal_reg_idle_indication_cfg - IPA IDLE_INDICATION_CFG register
 * @const_non_idle_enable: enable the asserting of the IDLE value and DCD
 * @enter_idle_debounce_thresh:  configure the debounce threshold
 */
struct ipahal_reg_idle_indication_cfg {
	u16 enter_idle_debounce_thresh;
	bool const_non_idle_enable;
};

/*
 * struct ipa_ep_cfg_ctrl_scnd - PA_ENDP_INIT_CTRL_SCND_n register
 * @endp_delay: delay endpoint
 */
struct ipahal_ep_cfg_ctrl_scnd {
	bool endp_delay;
};

/*
 * ipahal_print_all_regs() - Loop and read and print all the valid registers
 *  Parameterized registers are also printed for all the valid ranges.
 *  Print to dmsg and IPC logs
 */
void ipahal_print_all_regs(bool print_to_dmesg);

/*
 * ipahal_reg_name_str() - returns string that represent the register
 * @reg_name: [in] register name
 */
const char *ipahal_reg_name_str(enum ipahal_reg_name reg_name);

/*
 * ipahal_read_reg_n() - Get the raw value of n parameterized reg
 */
u32 ipahal_read_reg_n(enum ipahal_reg_name reg, u32 n);

/*
 * ipahal_read_reg_mn() - Get mn parameterized reg value
 */
u32 ipahal_read_reg_mn(enum ipahal_reg_name reg, u32 m, u32 n);

/*
 * ipahal_write_reg_mn() - Write to m/n parameterized reg a raw value
 */
void ipahal_write_reg_mn(enum ipahal_reg_name reg, u32 m, u32 n, u32 val);

/*
 * ipahal_write_reg_n() - Write to n parameterized reg a raw value
 */
static inline void ipahal_write_reg_n(enum ipahal_reg_name reg,
	u32 n, u32 val)
{
	ipahal_write_reg_mn(reg, 0, n, val);
}

/*
 * ipahal_read_reg_n_fields() - Get the parsed value of n parameterized reg
 */
u32 ipahal_read_reg_n_fields(enum ipahal_reg_name reg, u32 n, void *fields);

/*
 * ipahal_write_reg_n_fields() - Write to n parameterized reg a prased value
 */
void ipahal_write_reg_n_fields(enum ipahal_reg_name reg, u32 n,
	const void *fields);

/*
 * ipahal_read_reg() - Get the raw value of a reg
 */
static inline u32 ipahal_read_reg(enum ipahal_reg_name reg)
{
	return ipahal_read_reg_n(reg, 0);
}

/*
 * ipahal_write_reg() - Write to reg a raw value
 */
static inline void ipahal_write_reg(enum ipahal_reg_name reg,
	u32 val)
{
	ipahal_write_reg_mn(reg, 0, 0, val);
}

/*
 * ipahal_read_reg_fields() - Get the parsed value of a reg
 */
static inline u32 ipahal_read_reg_fields(enum ipahal_reg_name reg, void *fields)
{
	return ipahal_read_reg_n_fields(reg, 0, fields);
}

/*
 * ipahal_write_reg_fields() - Write to reg a parsed value
 */
static inline void ipahal_write_reg_fields(enum ipahal_reg_name reg,
	const void *fields)
{
	ipahal_write_reg_n_fields(reg, 0, fields);
}

/*
 * Get the offset of a m/n parameterized register
 */
u32 ipahal_get_reg_mn_ofst(enum ipahal_reg_name reg, u32 m, u32 n);

/*
 * Get the offset of a n parameterized register
 */
static inline u32 ipahal_get_reg_n_ofst(enum ipahal_reg_name reg, u32 n)
{
	return ipahal_get_reg_mn_ofst(reg, 0, n);
}

/*
 * Get the offset of a register
 */
static inline u32 ipahal_get_reg_ofst(enum ipahal_reg_name reg)
{
	return ipahal_get_reg_mn_ofst(reg, 0, 0);
}

/*
 * Get the register base address
 */
u32 ipahal_get_reg_base(void);

/*
 * Specific functions
 * These functions supply specific register values for specific operations
 *  that cannot be reached by generic functions.
 * E.g. To disable aggregation, need to write to specific bits of the AGGR
 *  register. The other bits should be untouched. This operation is very
 *  specific and cannot be generically defined. For such operations we define
 *  these specific functions.
 */
void ipahal_get_aggr_force_close_valmask(int ep_idx,
	struct ipahal_reg_valmask *valmask);
void ipahal_get_fltrt_hash_flush_valmask(
	struct ipahal_reg_fltrt_hash_flush *flush,
	struct ipahal_reg_valmask *valmask);

#endif /* _IPAHAL_REG_H_ */
