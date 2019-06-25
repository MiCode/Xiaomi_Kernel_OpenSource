// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */
#include "ipa_reg_dump.h"
#include "ipa_access_control.h"

/* Total size required for test bus */
#define IPA_MEM_OVERLAY_SIZE     0x66000

/*
 * The following structure contains a hierarchy of structures that
 * ultimately leads to a series of leafs. The leafs are structures
 * containing detailed, bit level, register definitions.
 */
static struct regs_save_hierarchy_s ipa_reg_save;

static unsigned int ipa_testbus_mem[IPA_MEM_OVERLAY_SIZE];

/*
 * The following data structure contains a list of the registers
 * (whose data are to be copied) and the locations (within
 * ipa_reg_save above) into which the registers' values need to be
 * copied.
 */
static struct map_src_dst_addr_s ipa_regs_to_save_array[] = {
	/*
	 * =====================================================================
	 * IPA register definitions begin here...
	 * =====================================================================
	 */

	/* IPA General Registers */
	GEN_SRC_DST_ADDR_MAP(IPA_STATE,
			     ipa.gen,
			     ipa_state),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_RX_ACTIVE,
			     ipa.gen,
			     ipa_state_rx_active),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_TX_WRAPPER,
			     ipa.gen,
			     ipa_state_tx_wrapper),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_TX0,
			     ipa.gen,
			     ipa_state_tx0),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_TX1,
			     ipa.gen,
			     ipa_state_tx1),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_AGGR_ACTIVE,
			     ipa.gen,
			     ipa_state_aggr_active),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_DFETCHER,
			     ipa.gen,
			     ipa_state_dfetcher),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_FETCHER_MASK_0,
			     ipa.gen,
			     ipa_state_fetcher_mask_0),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_FETCHER_MASK_1,
			     ipa.gen,
			     ipa_state_fetcher_mask_1),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_GSI_AOS,
			     ipa.gen,
			     ipa_state_gsi_aos),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_GSI_IF,
			     ipa.gen,
			     ipa_state_gsi_if),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_GSI_SKIP,
			     ipa.gen,
			     ipa_state_gsi_skip),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_GSI_TLV,
			     ipa.gen,
			     ipa_state_gsi_tlv),
	GEN_SRC_DST_ADDR_MAP(IPA_DPL_TIMER_LSB,
			     ipa.gen,
			     ipa_dpl_timer_lsb),
	GEN_SRC_DST_ADDR_MAP(IPA_DPL_TIMER_MSB,
			     ipa.gen,
			     ipa_dpl_timer_msb),
	GEN_SRC_DST_ADDR_MAP(IPA_PROC_IPH_CFG,
			     ipa.gen,
			     ipa_proc_iph_cfg),
	GEN_SRC_DST_ADDR_MAP(IPA_ROUTE,
			     ipa.gen,
			     ipa_route),
	GEN_SRC_DST_ADDR_MAP(IPA_SPARE_REG_1,
			     ipa.gen,
			     ipa_spare_reg_1),
	GEN_SRC_DST_ADDR_MAP(IPA_SPARE_REG_2,
			     ipa.gen,
			     ipa_spare_reg_2),
	GEN_SRC_DST_ADDR_MAP(IPA_LOG,
			     ipa.gen,
			     ipa_log),
	GEN_SRC_DST_ADDR_MAP(IPA_LOG_BUF_STATUS_CFG,
			     ipa.gen,
			     ipa_log_buf_status_cfg),
	GEN_SRC_DST_ADDR_MAP(IPA_LOG_BUF_STATUS_ADDR,
			     ipa.gen,
			     ipa_log_buf_status_addr),
	GEN_SRC_DST_ADDR_MAP(IPA_LOG_BUF_STATUS_WRITE_PTR,
			     ipa.gen,
			     ipa_log_buf_status_write_ptr),
	GEN_SRC_DST_ADDR_MAP(IPA_LOG_BUF_STATUS_RAM_PTR,
			     ipa.gen,
			     ipa_log_buf_status_ram_ptr),
	GEN_SRC_DST_ADDR_MAP(IPA_LOG_BUF_HW_CMD_CFG,
			     ipa.gen,
			     ipa_log_buf_hw_cmd_cfg),
	GEN_SRC_DST_ADDR_MAP(IPA_LOG_BUF_HW_CMD_ADDR,
			     ipa.gen,
			     ipa_log_buf_hw_cmd_addr),
	GEN_SRC_DST_ADDR_MAP(IPA_LOG_BUF_HW_CMD_WRITE_PTR,
			     ipa.gen,
			     ipa_log_buf_hw_cmd_write_ptr),
	GEN_SRC_DST_ADDR_MAP(IPA_LOG_BUF_HW_CMD_RAM_PTR,
			     ipa.gen,
			     ipa_log_buf_hw_cmd_ram_ptr),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_DPL_FIFO,
			     ipa.gen,
			     ipa_state_dpl_fifo),
	GEN_SRC_DST_ADDR_MAP(IPA_COMP_HW_VERSION,
			     ipa.gen,
			     ipa_comp_hw_version),
	GEN_SRC_DST_ADDR_MAP(IPA_FILT_ROUT_HASH_EN,
			     ipa.gen,
			     ipa_filt_rout_hash_en),
	GEN_SRC_DST_ADDR_MAP(IPA_FILT_ROUT_HASH_FLUSH,
			     ipa.gen,
			     ipa_filt_rout_hash_flush),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_FETCHER,
			     ipa.gen,
			     ipa_state_fetcher),
	GEN_SRC_DST_ADDR_MAP(IPA_IPV4_FILTER_INIT_VALUES,
			     ipa.gen,
			     ipa_ipv4_filter_init_values),
	GEN_SRC_DST_ADDR_MAP(IPA_IPV6_FILTER_INIT_VALUES,
			     ipa.gen,
			     ipa_ipv6_filter_init_values),
	GEN_SRC_DST_ADDR_MAP(IPA_IPV4_ROUTE_INIT_VALUES,
			     ipa.gen,
			     ipa_ipv4_route_init_values),
	GEN_SRC_DST_ADDR_MAP(IPA_IPV6_ROUTE_INIT_VALUES,
			     ipa.gen,
			     ipa_ipv6_route_init_values),
	GEN_SRC_DST_ADDR_MAP(IPA_BAM_ACTIVATED_PORTS,
			     ipa.gen,
			     ipa_bam_activated_ports),
	GEN_SRC_DST_ADDR_MAP(IPA_TX_COMMANDER_CMDQ_STATUS,
			     ipa.gen,
			     ipa_tx_commander_cmdq_status),
	GEN_SRC_DST_ADDR_MAP(IPA_LOG_BUF_HW_SNIF_EL_EN,
			     ipa.gen,
			     ipa_log_buf_hw_snif_el_en),
	GEN_SRC_DST_ADDR_MAP(IPA_LOG_BUF_HW_SNIF_EL_WR_N_RD_SEL,
			     ipa.gen,
			     ipa_log_buf_hw_snif_el_wr_n_rd_sel),
	GEN_SRC_DST_ADDR_MAP(IPA_LOG_BUF_HW_SNIF_EL_CLI_MUX,
			     ipa.gen,
			     ipa_log_buf_hw_snif_el_cli_mux),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_ACL,
			     ipa.gen,
			     ipa_state_acl),
	GEN_SRC_DST_ADDR_MAP(IPA_SYS_PKT_PROC_CNTXT_BASE,
			     ipa.gen,
			     ipa_sys_pkt_proc_cntxt_base),
	GEN_SRC_DST_ADDR_MAP(IPA_SYS_PKT_PROC_CNTXT_BASE_MSB,
			     ipa.gen,
			     ipa_sys_pkt_proc_cntxt_base_msb),
	GEN_SRC_DST_ADDR_MAP(IPA_LOCAL_PKT_PROC_CNTXT_BASE,
			     ipa.gen,
			     ipa_local_pkt_proc_cntxt_base),
	GEN_SRC_DST_ADDR_MAP(IPA_RSRC_GRP_CFG,
			     ipa.gen,
			     ipa_rsrc_grp_cfg),
	GEN_SRC_DST_ADDR_MAP(IPA_PIPELINE_DISABLE,
			     ipa.gen,
			     ipa_pipeline_disable),
	GEN_SRC_DST_ADDR_MAP(IPA_COMP_CFG,
			     ipa.gen,
			     ipa_comp_cfg),
	GEN_SRC_DST_ADDR_MAP(IPA_STATE_NLO_AGGR,
			     ipa.gen,
			     ipa_state_nlo_aggr),
	GEN_SRC_DST_ADDR_MAP(IPA_NLO_PP_CFG1,
			     ipa.gen,
			     ipa_nlo_pp_cfg1),
	GEN_SRC_DST_ADDR_MAP(IPA_NLO_PP_CFG2,
			     ipa.gen,
			     ipa_nlo_pp_cfg2),
	GEN_SRC_DST_ADDR_MAP(IPA_NLO_PP_ACK_LIMIT_CFG,
			     ipa.gen,
			     ipa_nlo_pp_ack_limit_cfg),
	GEN_SRC_DST_ADDR_MAP(IPA_NLO_PP_DATA_LIMIT_CFG,
			     ipa.gen,
			     ipa_nlo_pp_data_limit_cfg),
	GEN_SRC_DST_ADDR_MAP(IPA_NLO_MIN_DSM_CFG,
			     ipa.gen,
			     ipa_nlo_min_dsm_cfg),
	GEN_SRC_DST_ADDR_MAP(IPA_NLO_VP_FLUSH_REQ,
			     ipa.gen,
			     ipa_nlo_vp_flush_req),
	GEN_SRC_DST_ADDR_MAP(IPA_NLO_VP_FLUSH_COOKIE,
			     ipa.gen,
			     ipa_nlo_vp_flush_cookie),
	GEN_SRC_DST_ADDR_MAP(IPA_NLO_VP_FLUSH_ACK,
			     ipa.gen,
			     ipa_nlo_vp_flush_ack),
	GEN_SRC_DST_ADDR_MAP(IPA_NLO_VP_DSM_OPEN,
			     ipa.gen,
			     ipa_nlo_vp_dsm_open),
	GEN_SRC_DST_ADDR_MAP(IPA_NLO_VP_QBAP_OPEN,
			     ipa.gen,
			     ipa_nlo_vp_qbap_open),

	/* Debug Registers */
	GEN_SRC_DST_ADDR_MAP(IPA_DEBUG_DATA,
			     ipa.dbg,
			     ipa_debug_data),
	GEN_SRC_DST_ADDR_MAP(IPA_STEP_MODE_BREAKPOINTS,
			     ipa.dbg,
			     ipa_step_mode_breakpoints),
	GEN_SRC_DST_ADDR_MAP(IPA_STEP_MODE_STATUS,
			     ipa.dbg,
			     ipa_step_mode_status),

	IPA_REG_SAVE_RX_SPLT_CMDQ(
		IPA_RX_SPLT_CMDQ_CMD_n, ipa_rx_splt_cmdq_cmd_n),
	IPA_REG_SAVE_RX_SPLT_CMDQ(
		IPA_RX_SPLT_CMDQ_CFG_n, ipa_rx_splt_cmdq_cfg_n),
	IPA_REG_SAVE_RX_SPLT_CMDQ(
		IPA_RX_SPLT_CMDQ_DATA_WR_0_n, ipa_rx_splt_cmdq_data_wr_0_n),
	IPA_REG_SAVE_RX_SPLT_CMDQ(
		IPA_RX_SPLT_CMDQ_DATA_WR_1_n, ipa_rx_splt_cmdq_data_wr_1_n),
	IPA_REG_SAVE_RX_SPLT_CMDQ(
		IPA_RX_SPLT_CMDQ_DATA_WR_2_n, ipa_rx_splt_cmdq_data_wr_2_n),
	IPA_REG_SAVE_RX_SPLT_CMDQ(
		IPA_RX_SPLT_CMDQ_DATA_WR_3_n, ipa_rx_splt_cmdq_data_wr_3_n),
	IPA_REG_SAVE_RX_SPLT_CMDQ(
		IPA_RX_SPLT_CMDQ_DATA_RD_0_n, ipa_rx_splt_cmdq_data_rd_0_n),
	IPA_REG_SAVE_RX_SPLT_CMDQ(
		IPA_RX_SPLT_CMDQ_DATA_RD_1_n, ipa_rx_splt_cmdq_data_rd_1_n),
	IPA_REG_SAVE_RX_SPLT_CMDQ(
		IPA_RX_SPLT_CMDQ_DATA_RD_2_n, ipa_rx_splt_cmdq_data_rd_2_n),
	IPA_REG_SAVE_RX_SPLT_CMDQ(
		IPA_RX_SPLT_CMDQ_DATA_RD_3_n, ipa_rx_splt_cmdq_data_rd_3_n),
	IPA_REG_SAVE_RX_SPLT_CMDQ(
		IPA_RX_SPLT_CMDQ_STATUS_n, ipa_rx_splt_cmdq_status_n),

	GEN_SRC_DST_ADDR_MAP(IPA_RX_HPS_CMDQ_CFG_WR,
				  ipa.dbg,
				  ipa_rx_hps_cmdq_cfg_wr),
	GEN_SRC_DST_ADDR_MAP(IPA_RX_HPS_CMDQ_CFG_RD,
				  ipa.dbg,
				  ipa_rx_hps_cmdq_cfg_rd),
	GEN_SRC_DST_ADDR_MAP(IPA_RX_HPS_CMDQ_CMD,
			     ipa.dbg,
			     ipa_rx_hps_cmdq_cmd),
	GEN_SRC_DST_ADDR_MAP(IPA_RX_HPS_CMDQ_STATUS_EMPTY,
			     ipa.dbg,
			     ipa_rx_hps_cmdq_status_empty),
	GEN_SRC_DST_ADDR_MAP(IPA_RX_HPS_CLIENTS_MIN_DEPTH_0,
			     ipa.dbg,
			     ipa_rx_hps_clients_min_depth_0),
	GEN_SRC_DST_ADDR_MAP(IPA_RX_HPS_CLIENTS_MAX_DEPTH_0,
			     ipa.dbg,
			     ipa_rx_hps_clients_max_depth_0),
	GEN_SRC_DST_ADDR_MAP(IPA_HPS_DPS_CMDQ_CMD,
			     ipa.dbg,
			     ipa_hps_dps_cmdq_cmd),
	GEN_SRC_DST_ADDR_MAP(IPA_HPS_DPS_CMDQ_STATUS_EMPTY,
			     ipa.dbg,
			     ipa_hps_dps_cmdq_status_empty),
	GEN_SRC_DST_ADDR_MAP(IPA_DPS_TX_CMDQ_CMD,
			     ipa.dbg,
			     ipa_dps_tx_cmdq_cmd),
	GEN_SRC_DST_ADDR_MAP(IPA_DPS_TX_CMDQ_STATUS_EMPTY,
			     ipa.dbg,
			     ipa_dps_tx_cmdq_status_empty),
	GEN_SRC_DST_ADDR_MAP(IPA_ACKMNGR_CMDQ_CMD,
			     ipa.dbg,
			     ipa_ackmngr_cmdq_cmd),
	GEN_SRC_DST_ADDR_MAP(IPA_ACKMNGR_CMDQ_STATUS_EMPTY,
			     ipa.dbg,
			     ipa_ackmngr_cmdq_status_empty),

	/*
	 * NOTE: That GEN_SRC_DST_ADDR_MAP() not used below.  This is
	 *       because the following registers are not scaler, rather
	 *       they are register arrays...
	 */
	IPA_REG_SAVE_CFG_ENTRY_GEN_EE(IPA_IRQ_STTS_EE_n,
				      ipa_irq_stts_ee_n),
	IPA_REG_SAVE_CFG_ENTRY_GEN_EE(IPA_IRQ_EN_EE_n,
				      ipa_irq_en_ee_n),
	IPA_REG_SAVE_CFG_ENTRY_GEN_EE(IPA_FEC_ADDR_EE_n,
				      ipa_fec_addr_ee_n),
	IPA_REG_SAVE_CFG_ENTRY_GEN_EE(IPA_FEC_ATTR_EE_n,
				      ipa_fec_attr_ee_n),
	IPA_REG_SAVE_CFG_ENTRY_GEN_EE(IPA_SNOC_FEC_EE_n,
				      ipa_snoc_fec_ee_n),
	IPA_REG_SAVE_CFG_ENTRY_GEN_EE(IPA_HOLB_DROP_IRQ_INFO_EE_n,
				      ipa_holb_drop_irq_info_ee_n),
	IPA_REG_SAVE_CFG_ENTRY_GEN_EE(IPA_SUSPEND_IRQ_INFO_EE_n,
				      ipa_suspend_irq_info_ee_n),
	IPA_REG_SAVE_CFG_ENTRY_GEN_EE(IPA_SUSPEND_IRQ_EN_EE_n,
				      ipa_suspend_irq_en_ee_n),

	/* Pipe Endp Registers */
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_CTRL_n,
					 ipa_endp_init_ctrl_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_CTRL_SCND_n,
					 ipa_endp_init_ctrl_scnd_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_CFG_n,
					 ipa_endp_init_cfg_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_NAT_n,
					 ipa_endp_init_nat_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_HDR_n,
					 ipa_endp_init_hdr_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_HDR_EXT_n,
					 ipa_endp_init_hdr_ext_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_HDR_METADATA_MASK_n,
					 ipa_endp_init_hdr_metadata_mask_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_HDR_METADATA_n,
					 ipa_endp_init_hdr_metadata_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_MODE_n,
					 ipa_endp_init_mode_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_AGGR_n,
					 ipa_endp_init_aggr_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_HOL_BLOCK_EN_n,
					 ipa_endp_init_hol_block_en_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_HOL_BLOCK_TIMER_n,
					 ipa_endp_init_hol_block_timer_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_DEAGGR_n,
					 ipa_endp_init_deaggr_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_STATUS_n,
					 ipa_endp_status_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_RSRC_GRP_n,
					 ipa_endp_init_rsrc_grp_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_INIT_SEQ_n,
					 ipa_endp_init_seq_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_GSI_CFG_TLV_n,
					 ipa_endp_gsi_cfg_tlv_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_GSI_CFG_AOS_n,
					 ipa_endp_gsi_cfg_aos_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_GSI_CFG1_n,
					 ipa_endp_gsi_cfg1_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(IPA_ENDP_FILTER_ROUTER_HSH_CFG_n,
					 ipa_endp_filter_router_hsh_cfg_n),

	/* Source Resource Group Config Registers */
	IPA_REG_SAVE_CFG_ENTRY_SRC_RSRC_GRP(IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					    ipa_src_rsrc_grp_01_rsrc_type_n),
	IPA_REG_SAVE_CFG_ENTRY_SRC_RSRC_GRP(IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n,
					    ipa_src_rsrc_grp_23_rsrc_type_n),
	IPA_REG_SAVE_CFG_ENTRY_SRC_RSRC_GRP(IPA_SRC_RSRC_GRP_45_RSRC_TYPE_n,
					    ipa_src_rsrc_grp_45_rsrc_type_n),

	/* Destination Resource Group Config Registers */
	IPA_REG_SAVE_CFG_ENTRY_DST_RSRC_GRP(IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					    ipa_dst_rsrc_grp_01_rsrc_type_n),
	IPA_REG_SAVE_CFG_ENTRY_DST_RSRC_GRP(IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
					    ipa_dst_rsrc_grp_23_rsrc_type_n),
	IPA_REG_SAVE_CFG_ENTRY_DST_RSRC_GRP(IPA_DST_RSRC_GRP_45_RSRC_TYPE_n,
					    ipa_dst_rsrc_grp_45_rsrc_type_n),

	/* Source Resource Group Count Registers */
	IPA_REG_SAVE_CFG_ENTRY_SRC_RSRC_CNT_GRP(
		IPA_SRC_RSRC_GRP_0123_RSRC_TYPE_CNT_n,
		ipa_src_rsrc_grp_0123_rsrc_type_cnt_n),
	IPA_REG_SAVE_CFG_ENTRY_SRC_RSRC_CNT_GRP(
		IPA_SRC_RSRC_GRP_4567_RSRC_TYPE_CNT_n,
		ipa_src_rsrc_grp_4567_rsrc_type_cnt_n),

	/* Destination Resource Group Count Registers */
	IPA_REG_SAVE_CFG_ENTRY_DST_RSRC_CNT_GRP(
		IPA_DST_RSRC_GRP_0123_RSRC_TYPE_CNT_n,
		ipa_dst_rsrc_grp_0123_rsrc_type_cnt_n),
	IPA_REG_SAVE_CFG_ENTRY_DST_RSRC_CNT_GRP(
		IPA_DST_RSRC_GRP_4567_RSRC_TYPE_CNT_n,
		ipa_dst_rsrc_grp_4567_rsrc_type_cnt_n),

	/*
	 * =====================================================================
	 * GSI register definitions begin here...
	 * =====================================================================
	 */

	/* GSI General Registers */
	GEN_SRC_DST_ADDR_MAP(GSI_CFG,
			     gsi.gen,
			     gsi_cfg),
	GEN_SRC_DST_ADDR_MAP(GSI_REE_CFG,
			     gsi.gen,
			     gsi_ree_cfg),
	IPA_REG_SAVE_GSI_VER(
			     IPA_GSI_TOP_GSI_INST_RAM_n,
			     ipa_gsi_top_gsi_inst_ram_n),

	/* GSI Debug Registers */
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_DEBUG_BUSY_REG,
			     gsi.debug,
			     ipa_gsi_top_gsi_debug_busy_reg),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_DEBUG_EVENT_PENDING,
			     gsi.debug,
			     ipa_gsi_top_gsi_debug_event_pending),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_DEBUG_TIMER_PENDING,
			     gsi.debug,
			     ipa_gsi_top_gsi_debug_timer_pending),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_DEBUG_RD_WR_PENDING,
			     gsi.debug,
			     ipa_gsi_top_gsi_debug_rd_wr_pending),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_DEBUG_PC_FROM_SW,
			     gsi.debug,
			     ipa_gsi_top_gsi_debug_pc_from_sw),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_DEBUG_SW_STALL,
			     gsi.debug,
			     ipa_gsi_top_gsi_debug_sw_stall),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_DEBUG_PC_FOR_DEBUG,
			     gsi.debug,
			     ipa_gsi_top_gsi_debug_pc_for_debug),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_DEBUG_QSB_LOG_ERR_TRNS_ID,
			     gsi.debug,
			     ipa_gsi_top_gsi_debug_qsb_log_err_trns_id),

	IPA_REG_SAVE_CFG_ENTRY_GSI_QSB_DEBUG(
		GSI_DEBUG_QSB_LOG_LAST_MISC_IDn, qsb_log_last_misc),

	/* GSI IRAM pointers Registers */
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_IRAM_PTR_CH_CMD,
			     gsi.debug.gsi_iram_ptrs,
			     ipa_gsi_top_gsi_iram_ptr_ch_cmd),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_IRAM_PTR_EE_GENERIC_CMD,
			     gsi.debug.gsi_iram_ptrs,
			     ipa_gsi_top_gsi_iram_ptr_ee_generic_cmd),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_IRAM_PTR_CH_DB,
			     gsi.debug.gsi_iram_ptrs,
			     ipa_gsi_top_gsi_iram_ptr_ch_db),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_IRAM_PTR_EV_DB,
			     gsi.debug.gsi_iram_ptrs,
			     ipa_gsi_top_gsi_iram_ptr_ev_db),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_IRAM_PTR_NEW_RE,
			     gsi.debug.gsi_iram_ptrs,
			     ipa_gsi_top_gsi_iram_ptr_new_re),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_IRAM_PTR_CH_DIS_COMP,
			     gsi.debug.gsi_iram_ptrs,
			     ipa_gsi_top_gsi_iram_ptr_ch_dis_comp),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_IRAM_PTR_CH_EMPTY,
			     gsi.debug.gsi_iram_ptrs,
			     ipa_gsi_top_gsi_iram_ptr_ch_empty),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_IRAM_PTR_EVENT_GEN_COMP,
			     gsi.debug.gsi_iram_ptrs,
			     ipa_gsi_top_gsi_iram_ptr_event_gen_comp),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_IRAM_PTR_TIMER_EXPIRED,
			     gsi.debug.gsi_iram_ptrs,
			     ipa_gsi_top_gsi_iram_ptr_timer_expired),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_IRAM_PTR_WRITE_ENG_COMP,
			     gsi.debug.gsi_iram_ptrs,
			     ipa_gsi_top_gsi_iram_ptr_write_eng_comp),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_IRAM_PTR_READ_ENG_COMP,
			     gsi.debug.gsi_iram_ptrs,
			     ipa_gsi_top_gsi_iram_ptr_read_eng_comp),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_IRAM_PTR_UC_GP_INT,
			     gsi.debug.gsi_iram_ptrs,
			     ipa_gsi_top_gsi_iram_ptr_uc_gp_int),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_IRAM_PTR_INT_MOD_STOPPED,
			     gsi.debug.gsi_iram_ptrs,
			     ipa_gsi_top_gsi_iram_ptr_int_mod_stopped),

	/* GSI SHRAM pointers Registers */
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_SHRAM_PTR_CH_CNTXT_BASE_ADDR,
			     gsi.debug.gsi_shram_ptrs,
			     ipa_gsi_top_gsi_shram_ptr_ch_cntxt_base_addr),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_SHRAM_PTR_EV_CNTXT_BASE_ADDR,
			     gsi.debug.gsi_shram_ptrs,
			     ipa_gsi_top_gsi_shram_ptr_ev_cntxt_base_addr),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_SHRAM_PTR_RE_STORAGE_BASE_ADDR,
			     gsi.debug.gsi_shram_ptrs,
			     ipa_gsi_top_gsi_shram_ptr_re_storage_base_addr),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_SHRAM_PTR_RE_ESC_BUF_BASE_ADDR,
			     gsi.debug.gsi_shram_ptrs,
			     ipa_gsi_top_gsi_shram_ptr_re_esc_buf_base_addr),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_SHRAM_PTR_EE_SCRACH_BASE_ADDR,
			     gsi.debug.gsi_shram_ptrs,
			     ipa_gsi_top_gsi_shram_ptr_ee_scrach_base_addr),
	GEN_SRC_DST_ADDR_MAP(IPA_GSI_TOP_GSI_SHRAM_PTR_FUNC_STACK_BASE_ADDR,
			     gsi.debug.gsi_shram_ptrs,
			     ipa_gsi_top_gsi_shram_ptr_func_stack_base_addr),

	/*
	 * NOTE: That GEN_SRC_DST_ADDR_MAP() not used below.  This is
	 *       because the following registers are not scaler, rather
	 *       they are register arrays...
	 */

	/* GSI General EE Registers */
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(GSI_MANAGER_EE_QOS_n,
					      gsi_manager_ee_qos_n),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_GSI_STATUS,
					      ee_n_gsi_status),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_TYPE_IRQ,
					      ee_n_cntxt_type_irq),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_TYPE_IRQ_MSK,
					      ee_n_cntxt_type_irq_msk),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_SRC_GSI_CH_IRQ,
					      ee_n_cntxt_src_gsi_ch_irq),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_SRC_EV_CH_IRQ,
					      ee_n_cntxt_src_ev_ch_irq),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_SRC_GSI_CH_IRQ_MSK,
					      ee_n_cntxt_src_gsi_ch_irq_msk),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_SRC_EV_CH_IRQ_MSK,
					      ee_n_cntxt_src_ev_ch_irq_msk),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_SRC_IEOB_IRQ,
					      ee_n_cntxt_src_ieob_irq),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_SRC_IEOB_IRQ_MSK,
					      ee_n_cntxt_src_ieob_irq_msk),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_GSI_IRQ_STTS,
					      ee_n_cntxt_gsi_irq_stts),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_GLOB_IRQ_STTS,
					      ee_n_cntxt_glob_irq_stts),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_ERROR_LOG,
					      ee_n_error_log),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_SCRATCH_0,
					      ee_n_cntxt_scratch_0),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_SCRATCH_1,
					      ee_n_cntxt_scratch_1),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_INTSET,
					      ee_n_cntxt_intset),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_MSI_BASE_LSB,
					      ee_n_cntxt_msi_base_lsb),
	IPA_REG_SAVE_CFG_ENTRY_GSI_GENERAL_EE(EE_n_CNTXT_MSI_BASE_MSB,
					      ee_n_cntxt_msi_base_msb),

	/* GSI Channel Context Registers */
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_CNTXT_0,
					    ee_n_gsi_ch_k_cntxt_0),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_CNTXT_1,
					    ee_n_gsi_ch_k_cntxt_1),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_CNTXT_2,
					    ee_n_gsi_ch_k_cntxt_2),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_CNTXT_3,
					    ee_n_gsi_ch_k_cntxt_3),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_CNTXT_4,
					    ee_n_gsi_ch_k_cntxt_4),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_CNTXT_5,
					    ee_n_gsi_ch_k_cntxt_5),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_CNTXT_6,
					    ee_n_gsi_ch_k_cntxt_6),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_CNTXT_7,
					    ee_n_gsi_ch_k_cntxt_7),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_RE_FETCH_READ_PTR,
					    ee_n_gsi_ch_k_re_fetch_read_ptr),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_RE_FETCH_WRITE_PTR,
					    ee_n_gsi_ch_k_re_fetch_write_ptr),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_QOS,
					    ee_n_gsi_ch_k_qos),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_SCRATCH_0,
					    ee_n_gsi_ch_k_scratch_0),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_SCRATCH_1,
					    ee_n_gsi_ch_k_scratch_1),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_SCRATCH_2,
					    ee_n_gsi_ch_k_scratch_2),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(EE_n_GSI_CH_k_SCRATCH_3,
					    ee_n_gsi_ch_k_scratch_3),
	IPA_REG_SAVE_CFG_ENTRY_GSI_CH_CNTXT(GSI_MAP_EE_n_CH_k_VP_TABLE,
					    gsi_map_ee_n_ch_k_vp_table),

	/* GSI Channel Event Context Registers */
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_0,
					     ee_n_ev_ch_k_cntxt_0),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_1,
					     ee_n_ev_ch_k_cntxt_1),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_2,
					     ee_n_ev_ch_k_cntxt_2),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_3,
					     ee_n_ev_ch_k_cntxt_3),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_4,
					     ee_n_ev_ch_k_cntxt_4),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_5,
					     ee_n_ev_ch_k_cntxt_5),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_6,
					     ee_n_ev_ch_k_cntxt_6),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_7,
					     ee_n_ev_ch_k_cntxt_7),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_8,
					     ee_n_ev_ch_k_cntxt_8),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_9,
					     ee_n_ev_ch_k_cntxt_9),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_10,
					     ee_n_ev_ch_k_cntxt_10),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_11,
					     ee_n_ev_ch_k_cntxt_11),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_12,
					     ee_n_ev_ch_k_cntxt_12),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_CNTXT_13,
					     ee_n_ev_ch_k_cntxt_13),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_SCRATCH_0,
					     ee_n_ev_ch_k_scratch_0),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(EE_n_EV_CH_k_SCRATCH_1,
					     ee_n_ev_ch_k_scratch_1),
	IPA_REG_SAVE_CFG_ENTRY_GSI_EVT_CNTXT(GSI_DEBUG_EE_n_EV_k_VP_TABLE,
					     gsi_debug_ee_n_ev_k_vp_table),

#if defined(CONFIG_IPA3_REGDUMP_NUM_EXTRA_ENDP_REGS) && \
	CONFIG_IPA3_REGDUMP_NUM_EXTRA_ENDP_REGS > 0
	/* Endp Registers for remaining pipes */
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_CTRL_n,
					       ipa_endp_init_ctrl_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_CTRL_SCND_n,
					       ipa_endp_init_ctrl_scnd_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_CFG_n,
					       ipa_endp_init_cfg_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_NAT_n,
					       ipa_endp_init_nat_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_HDR_n,
					       ipa_endp_init_hdr_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_HDR_EXT_n,
					       ipa_endp_init_hdr_ext_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA
		(IPA_ENDP_INIT_HDR_METADATA_MASK_n,
		ipa_endp_init_hdr_metadata_mask_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_HDR_METADATA_n,
					       ipa_endp_init_hdr_metadata_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_MODE_n,
					       ipa_endp_init_mode_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_AGGR_n,
					       ipa_endp_init_aggr_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_HOL_BLOCK_EN_n,
					       ipa_endp_init_hol_block_en_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_HOL_BLOCK_TIMER_n,
					       ipa_endp_init_hol_block_timer_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_DEAGGR_n,
					       ipa_endp_init_deaggr_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_STATUS_n,
					       ipa_endp_status_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_RSRC_GRP_n,
					       ipa_endp_init_rsrc_grp_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_INIT_SEQ_n,
					       ipa_endp_init_seq_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_GSI_CFG_TLV_n,
					       ipa_endp_gsi_cfg_tlv_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_GSI_CFG_AOS_n,
					       ipa_endp_gsi_cfg_aos_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(IPA_ENDP_GSI_CFG1_n,
					       ipa_endp_gsi_cfg1_n),
	IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA
		(IPA_ENDP_FILTER_ROUTER_HSH_CFG_n,
		 ipa_endp_filter_router_hsh_cfg_n),
#endif
};

/* IPA uC PER registers save Cfg array */
static struct map_src_dst_addr_s ipa_uc_regs_to_save_array[] = {
	/* HWP registers */
	GEN_SRC_DST_ADDR_MAP(IPA_UC_QMB_SYS_ADDR,
			     ipa.hwp,
			     ipa_uc_qmb_sys_addr),
	GEN_SRC_DST_ADDR_MAP(IPA_UC_QMB_LOCAL_ADDR,
			     ipa.hwp,
			     ipa_uc_qmb_local_addr),
	GEN_SRC_DST_ADDR_MAP(IPA_UC_QMB_LENGTH,
			     ipa.hwp,
			     ipa_uc_qmb_length),
	GEN_SRC_DST_ADDR_MAP(IPA_UC_QMB_TRIGGER,
			     ipa.hwp,
			     ipa_uc_qmb_trigger),
	GEN_SRC_DST_ADDR_MAP(IPA_UC_QMB_PENDING_TID,
			     ipa.hwp,
			     ipa_uc_qmb_pending_tid),
	GEN_SRC_DST_ADDR_MAP(IPA_UC_QMB_COMPLETED_RD_FIFO_PEEK,
			     ipa.hwp,
			     ipa_uc_qmb_completed_rd_fifo_peek),
	GEN_SRC_DST_ADDR_MAP(IPA_UC_QMB_COMPLETED_WR_FIFO_PEEK,
			     ipa.hwp,
			     ipa_uc_qmb_completed_wr_fifo_peek),
	GEN_SRC_DST_ADDR_MAP(IPA_UC_QMB_MISC,
			     ipa.hwp,
			     ipa_uc_qmb_misc),
	GEN_SRC_DST_ADDR_MAP(IPA_UC_QMB_STATUS,
			     ipa.hwp,
			     ipa_uc_qmb_status),
	GEN_SRC_DST_ADDR_MAP(IPA_UC_QMB_BUS_ATTRIB,
			     ipa.hwp,
			     ipa_uc_qmb_bus_attrib),
};

static void ipa_hal_save_regs_save_ipa_testbus(void);
static void ipa_reg_save_gsi_fifo_status(void);
static void ipa_reg_save_rsrc_cnts(void);
static void ipa_hal_save_regs_ipa_cmdq(void);
static void ipa_hal_save_regs_rsrc_db(void);
static void ipa_reg_save_anomaly_check(void);

static struct reg_access_funcs_s *get_access_funcs(u32 addr)
{
	u32 i, asub = ipa3_ctx->sd_state;

	for (i = 0; i < ARRAY_SIZE(mem_access_map); i++) {
		if (addr >= mem_access_map[i].addr_range_begin &&
			addr <  mem_access_map[i].addr_range_end) {
			return mem_access_map[i].access[asub];
		}
	}

	IPAERR("Unknown register offset(0x%08X). Using dflt access methods\n",
		   addr);

	return &io_matrix[AA_COMBO];
}

static u32 in_dword(
	u32 addr)
{
	struct reg_access_funcs_s *io = get_access_funcs(addr);

	return io->read(ipa3_ctx->reg_collection_base + addr);
}

static u32 in_dword_masked(
	u32 addr,
	u32 mask)
{
	struct reg_access_funcs_s *io = get_access_funcs(addr);
	u32 val;

	val = io->read(ipa3_ctx->reg_collection_base + addr);

	if (io->read == act_read)
		return val & mask;

	return val;
}

static void out_dword(
	u32 addr,
	u32 val)
{
	struct reg_access_funcs_s *io = get_access_funcs(addr);

	io->write(ipa3_ctx->reg_collection_base + addr, val);
}

/*
 * FUNCTION:  ipa_save_gsi_ver
 *
 * Saves the gsi version
 *
 * @return
 * None
 */
void ipa_save_gsi_ver(void)
{
	if (!ipa3_ctx->do_register_collection_on_crash)
		return;

	ipa_reg_save.gsi.fw_ver =
		IPA_READ_1xVECTOR_REG(IPA_GSI_TOP_GSI_INST_RAM_n, 0);
}

/*
 * FUNCTION:  ipa_save_registers
 *
 * Saves all the IPA register values which are configured
 *
 * @return
 * None
 */
void ipa_save_registers(void)
{
	u32 i = 0;
	/* Fetch the number of registers configured to be saved */
	u32 num_regs = ARRAY_SIZE(ipa_regs_to_save_array);
	u32 num_uc_per_regs = ARRAY_SIZE(ipa_uc_regs_to_save_array);
	union ipa_hwio_def_ipa_rsrc_mngr_db_cfg_u for_cfg;
	union ipa_hwio_def_ipa_rsrc_mngr_db_rsrc_read_u for_read;

	if (!ipa3_ctx->do_register_collection_on_crash)
		return;

	IPAERR("Commencing\n");

	/*
	 * Remove the GSI FIFO and the endp registers for extra pipes for
	 * now.  These would be saved later
	 */
	num_regs -= (CONFIG_IPA3_REGDUMP_NUM_EXTRA_ENDP_REGS *
		     IPA_REG_SAVE_NUM_EXTRA_ENDP_REGS);

	memset(&for_cfg, 0, sizeof(for_cfg));
	memset(&for_read, 0, sizeof(for_read));

	/* Now save all the configured registers */
	for (i = 0; i < num_regs; i++) {
		/* Copy reg value to our data struct */
		*(ipa_regs_to_save_array[i].dst_addr) =
			in_dword(ipa_regs_to_save_array[i].src_addr);
	}

	/*
	 * Set the active flag for all active pipe indexed registers.
	 */
	for (i = 0; i < IPA_HW_PIPE_ID_MAX; i++)
		ipa_reg_save.ipa.pipes[i].active = true;

	/* Now save the per endp registers for the remaining pipes */
	for (i = 0; i < (CONFIG_IPA3_REGDUMP_NUM_EXTRA_ENDP_REGS *
			 IPA_REG_SAVE_NUM_EXTRA_ENDP_REGS); i++) {
		/* Copy reg value to our data struct */
		*(ipa_regs_to_save_array[num_regs + i].dst_addr) =
			in_dword(ipa_regs_to_save_array[num_regs + i].src_addr);
	}

	IPA_HW_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA_ACTIVE();

	num_regs += (CONFIG_IPA3_REGDUMP_NUM_EXTRA_ENDP_REGS *
		     IPA_REG_SAVE_NUM_EXTRA_ENDP_REGS);

	/* Saving GSI FIFO Status registers */
	ipa_reg_save_gsi_fifo_status();

	/*
	 * On targets that support SSR, we generally want to disable
	 * the following reg save functionality as it may cause stalls
	 * in IPA after the SSR.
	 *
	 * To override this, set do_non_tn_collection_on_crash to
	 * true, via dtsi, and the collection will be done.
	 */
	if (ipa3_ctx->do_non_tn_collection_on_crash) {
		/* Save all the uC PER configured registers */
		for (i = 0; i < num_uc_per_regs; i++) {
			/* Copy reg value to our data struct */
			*(ipa_uc_regs_to_save_array[i].dst_addr) =
			    in_dword(ipa_uc_regs_to_save_array[i].src_addr);
		}

		/* Saving CMD Queue registers */
		ipa_hal_save_regs_ipa_cmdq();

		/* Collecting resource DB information */
		ipa_hal_save_regs_rsrc_db();

		/* Save IPA testbus */
		if (ipa3_ctx->do_testbus_collection_on_crash)
			ipa_hal_save_regs_save_ipa_testbus();
	}

	/* GSI test bus */
	for (i = 0;
	     i < ARRAY_SIZE(ipa_reg_save_gsi_ch_test_bus_selector_array);
	     i++) {
		ipa_reg_save.gsi.debug.gsi_test_bus.test_bus_selector[i] =
			ipa_reg_save_gsi_ch_test_bus_selector_array[i];

		/* Write test bus selector */
		IPA_WRITE_SCALER_REG(
			GSI_TEST_BUS_SEL,
			ipa_reg_save_gsi_ch_test_bus_selector_array[i]);

		ipa_reg_save.gsi.debug.gsi_test_bus.test_bus_reg[
		    i].gsi_testbus_reg =
		    (u32) IPA_READ_SCALER_REG(GSI_TEST_BUS_REG);
	}

	ipa_reg_save_rsrc_cnts();

	for (i = 0; i < HWIO_GSI_DEBUG_SW_RF_n_READ_MAXn + 1; i++)
		ipa_reg_save.gsi.debug.gsi_mcs_regs.mcs_reg[i].rf_reg =
			IPA_READ_1xVECTOR_REG(GSI_DEBUG_SW_RF_n_READ, i);

	for (i = 0; i < HWIO_GSI_DEBUG_COUNTERn_MAXn + 1; i++)
		ipa_reg_save.gsi.debug.gsi_cnt_regs.cnt[i].counter_value =
			(u16)IPA_READ_1xVECTOR_REG(GSI_DEBUG_COUNTERn, i);

	for (i = 0; i < IPA_HW_REG_SAVE_GSI_NUM_CH_CNTXT_A7; i++) {
		u32 phys_ch_idx = ipa_reg_save.gsi.ch_cntxt.a7[
			i].gsi_map_ee_n_ch_k_vp_table.phy_ch;
		u32 n = phys_ch_idx * IPA_REG_SAVE_BYTES_PER_CHNL_SHRAM;

		if (!ipa_reg_save.gsi.ch_cntxt.a7[
				i].gsi_map_ee_n_ch_k_vp_table.valid)
			continue;

		ipa_reg_save.gsi.ch_cntxt.a7[
			i].mcs_channel_scratch.scratch4.shram =
			IPA_READ_1xVECTOR_REG(
				GSI_SHRAM_n,
				n + IPA_GSI_OFFSET_WORDS_SCRATCH4);

		ipa_reg_save.gsi.ch_cntxt.a7[
			i].mcs_channel_scratch.scratch5.shram =
			IPA_READ_1xVECTOR_REG(
				GSI_SHRAM_n,
				n + IPA_GSI_OFFSET_WORDS_SCRATCH5);
	}

	for (i = 0; i < IPA_HW_REG_SAVE_GSI_NUM_CH_CNTXT_UC; i++) {
		u32 phys_ch_idx = ipa_reg_save.gsi.ch_cntxt.uc[
			i].gsi_map_ee_n_ch_k_vp_table.phy_ch;
		u32 n = phys_ch_idx * IPA_REG_SAVE_BYTES_PER_CHNL_SHRAM;

		if (!ipa_reg_save.gsi.ch_cntxt.uc[
				i].gsi_map_ee_n_ch_k_vp_table.valid)
			continue;

		ipa_reg_save.gsi.ch_cntxt.uc[
			i].mcs_channel_scratch.scratch4.shram =
			IPA_READ_1xVECTOR_REG(
				GSI_SHRAM_n,
				n + IPA_GSI_OFFSET_WORDS_SCRATCH4);

		ipa_reg_save.gsi.ch_cntxt.uc[
			i].mcs_channel_scratch.scratch5.shram =
			IPA_READ_1xVECTOR_REG(
				GSI_SHRAM_n,
				n + IPA_GSI_OFFSET_WORDS_SCRATCH5);
	}

	/*
	 * On targets that support SSR, we generally want to disable
	 * the following reg save functionality as it may cause stalls
	 * in IPA after the SSR.
	 *
	 * To override this, set do_non_tn_collection_on_crash to
	 * true, via dtsi, and the collection will be done.
	 */
	if (ipa3_ctx->do_non_tn_collection_on_crash) {
		u32 ofst = GEN_2xVECTOR_REG_OFST(IPA_CTX_ID_m_CTX_NUM_n, 0, 0);
		struct reg_access_funcs_s *io = get_access_funcs(ofst);
		/*
		 * If the memory is accessible, copy pkt context directly from
		 * IPA_CTX_ID register space
		 */
		if (io->read == act_read) {
			memcpy((void *)ipa_reg_save.pkt_ctntx,
				   (const void *)
				   (ipa3_ctx->reg_collection_base + ofst),
				   sizeof(ipa_reg_save.pkt_ctntx));

			for_cfg.value =
				IPA_READ_SCALER_REG(IPA_RSRC_MNGR_DB_CFG);

			for_cfg.def.rsrc_type_sel = 0;

			IPA_MASKED_WRITE_SCALER_REG(
				IPA_RSRC_MNGR_DB_CFG,
				for_cfg.value);

			for (i = 0; i < IPA_HW_PKT_CTNTX_MAX; i++) {
				for_cfg.def.rsrc_id_sel = i;

				IPA_MASKED_WRITE_SCALER_REG(
					IPA_RSRC_MNGR_DB_CFG,
					for_cfg.value);

				for_read.value =
					IPA_READ_SCALER_REG(
						IPA_RSRC_MNGR_DB_RSRC_READ);

				if (for_read.def.rsrc_occupied) {
					ipa_reg_save.pkt_ctntx_active[i] = true;
					ipa_reg_save.pkt_cntxt_state[i] =
						(enum ipa_hw_pkt_cntxt_state_e)
						ipa_reg_save.pkt_ctntx[i].state;
				}
			}
		} else {
			IPAERR("IPA_CTX_ID is not currently accessible\n");
		}
	}

	if (ipa3_ctx->do_ram_collection_on_crash) {
		for (i = 0; i < IPA_IU_SIZE / sizeof(u32); i++) {
			ipa_reg_save.ipa.ipa_iu_ptr[i] =
				in_dword(IPA_IU_ADDR + (i * sizeof(u32)));
		}
		for (i = 0; i < IPA_SRAM_SIZE / sizeof(u32); i++) {
			ipa_reg_save.ipa.ipa_sram_ptr[i] =
				in_dword(IPA_SRAM_ADDR + (i * sizeof(u32)));
		}
		for (i = 0; i < IPA_MBOX_SIZE / sizeof(u32); i++) {
			ipa_reg_save.ipa.ipa_mbox_ptr[i] =
				in_dword(IPA_MBOX_ADDR + (i * sizeof(u32)));
		}
		for (i = 0; i < IPA_HRAM_SIZE / sizeof(u32); i++) {
			ipa_reg_save.ipa.ipa_hram_ptr[i] =
				in_dword(IPA_HRAM_ADDR + (i * sizeof(u32)));
		}
		for (i = 0; i < IPA_SEQ_SIZE / sizeof(u32); i++) {
			ipa_reg_save.ipa.ipa_seq_ptr[i] =
				in_dword(IPA_SEQ_ADDR + (i * sizeof(u32)));
		}
		for (i = 0; i < IPA_GSI_SIZE / sizeof(u32); i++) {
			ipa_reg_save.ipa.ipa_gsi_ptr[i] =
				in_dword(IPA_GSI_ADDR + (i * sizeof(u32)));
		}
		IPALOG_VnP_ADDRS(ipa_reg_save.ipa.ipa_iu_ptr);
		IPALOG_VnP_ADDRS(ipa_reg_save.ipa.ipa_sram_ptr);
		IPALOG_VnP_ADDRS(ipa_reg_save.ipa.ipa_mbox_ptr);
		IPALOG_VnP_ADDRS(ipa_reg_save.ipa.ipa_hram_ptr);
		IPALOG_VnP_ADDRS(ipa_reg_save.ipa.ipa_seq_ptr);
		IPALOG_VnP_ADDRS(ipa_reg_save.ipa.ipa_gsi_ptr);
	}

	ipa_reg_save_anomaly_check();

	IPAERR("Completed\n");
}

/*
 * FUNCTION:  ipa_reg_save_gsi_fifo_status
 *
 * This function saves the GSI FIFO Status registers for all endpoints
 *
 * @param
 *
 * @return
 */
static void ipa_reg_save_gsi_fifo_status(void)
{
	union ipa_hwio_def_ipa_gsi_fifo_status_ctrl_u gsi_fifo_status_ctrl;
	u8 i;

	memset(&gsi_fifo_status_ctrl, 0, sizeof(gsi_fifo_status_ctrl));

	for (i = 0; i < IPA_HW_PIPE_ID_MAX; i++) {
		gsi_fifo_status_ctrl.def.ipa_gsi_fifo_status_en = 1;
		gsi_fifo_status_ctrl.def.ipa_gsi_fifo_status_port_sel = i;

		IPA_MASKED_WRITE_SCALER_REG(IPA_GSI_FIFO_STATUS_CTRL,
				     gsi_fifo_status_ctrl.value);

		ipa_reg_save.gsi_fifo_status[i].gsi_fifo_status_ctrl.value =
			IPA_READ_SCALER_REG(IPA_GSI_FIFO_STATUS_CTRL);
		ipa_reg_save.gsi_fifo_status[i].gsi_tlv_fifo_status.value =
			IPA_READ_SCALER_REG(IPA_GSI_TLV_FIFO_STATUS);
		ipa_reg_save.gsi_fifo_status[i].gsi_aos_fifo_status.value =
			IPA_READ_SCALER_REG(IPA_GSI_AOS_FIFO_STATUS);
	}
}

/*
 * FUNCTION:  ipa_reg_save_rsrc_cnts
 *
 * This function saves the resource counts for all PCIE and DDR
 * resource groups.
 *
 * @param
 * @return
 */
static void ipa_reg_save_rsrc_cnts(void)
{
	union ipa_hwio_def_ipa_src_rsrc_grp_0123_rsrc_type_cnt_n_u
		src_0123_rsrc_cnt;
	union ipa_hwio_def_ipa_dst_rsrc_grp_0123_rsrc_type_cnt_n_u
		dst_0123_rsrc_cnt;

	ipa_reg_save.rsrc_cnts.pcie.resource_group = IPA_HW_PCIE_SRC_RSRP_GRP;
	ipa_reg_save.rsrc_cnts.ddr.resource_group = IPA_HW_DDR_SRC_RSRP_GRP;

	src_0123_rsrc_cnt.value =
		IPA_READ_1xVECTOR_REG(IPA_SRC_RSRC_GRP_0123_RSRC_TYPE_CNT_n, 0);

	ipa_reg_save.rsrc_cnts.pcie.src.pkt_cntxt =
		src_0123_rsrc_cnt.def.src_rsrc_grp_0_cnt;
	ipa_reg_save.rsrc_cnts.ddr.src.pkt_cntxt =
		src_0123_rsrc_cnt.def.src_rsrc_grp_1_cnt;

	src_0123_rsrc_cnt.value =
		IPA_READ_1xVECTOR_REG(IPA_SRC_RSRC_GRP_0123_RSRC_TYPE_CNT_n, 1);

	ipa_reg_save.rsrc_cnts.pcie.src.descriptor_list =
		src_0123_rsrc_cnt.def.src_rsrc_grp_0_cnt;
	ipa_reg_save.rsrc_cnts.ddr.src.descriptor_list =
		src_0123_rsrc_cnt.def.src_rsrc_grp_1_cnt;

	src_0123_rsrc_cnt.value =
		IPA_READ_1xVECTOR_REG(IPA_SRC_RSRC_GRP_0123_RSRC_TYPE_CNT_n, 2);

	ipa_reg_save.rsrc_cnts.pcie.src.data_descriptor_buffer =
		src_0123_rsrc_cnt.def.src_rsrc_grp_0_cnt;
	ipa_reg_save.rsrc_cnts.ddr.src.data_descriptor_buffer =
		src_0123_rsrc_cnt.def.src_rsrc_grp_1_cnt;

	src_0123_rsrc_cnt.value =
		IPA_READ_1xVECTOR_REG(IPA_SRC_RSRC_GRP_0123_RSRC_TYPE_CNT_n, 3);

	ipa_reg_save.rsrc_cnts.pcie.src.hps_dmars =
		src_0123_rsrc_cnt.def.src_rsrc_grp_0_cnt;
	ipa_reg_save.rsrc_cnts.ddr.src.hps_dmars =
		src_0123_rsrc_cnt.def.src_rsrc_grp_1_cnt;

	src_0123_rsrc_cnt.value =
		IPA_READ_1xVECTOR_REG(IPA_SRC_RSRC_GRP_0123_RSRC_TYPE_CNT_n, 4);

	ipa_reg_save.rsrc_cnts.pcie.src.reserved_acks =
		src_0123_rsrc_cnt.def.src_rsrc_grp_0_cnt;
	ipa_reg_save.rsrc_cnts.ddr.src.reserved_acks =
		src_0123_rsrc_cnt.def.src_rsrc_grp_1_cnt;

	dst_0123_rsrc_cnt.value =
		IPA_READ_1xVECTOR_REG(IPA_DST_RSRC_GRP_0123_RSRC_TYPE_CNT_n, 0);

	ipa_reg_save.rsrc_cnts.pcie.dst.reserved_sectors =
		dst_0123_rsrc_cnt.def.dst_rsrc_grp_0_cnt;
	ipa_reg_save.rsrc_cnts.ddr.dst.reserved_sectors =
		dst_0123_rsrc_cnt.def.dst_rsrc_grp_1_cnt;

	dst_0123_rsrc_cnt.value =
		IPA_READ_1xVECTOR_REG(IPA_DST_RSRC_GRP_0123_RSRC_TYPE_CNT_n, 1);

	ipa_reg_save.rsrc_cnts.pcie.dst.dps_dmars =
		dst_0123_rsrc_cnt.def.dst_rsrc_grp_0_cnt;
	ipa_reg_save.rsrc_cnts.ddr.dst.dps_dmars =
		dst_0123_rsrc_cnt.def.dst_rsrc_grp_1_cnt;
}

/*
 * FUNCTION:  ipa_reg_save_rsrc_cnts_test_bus
 *
 * This function saves the resource counts for all PCIE and DDR
 * resource groups collected from test bus.
 *
 * @param
 *
 * @return
 */
void ipa_reg_save_rsrc_cnts_test_bus(void)
{
	int32_t rsrc_type = 0;

	ipa_reg_save.rsrc_cnts.pcie.resource_group = IPA_HW_PCIE_SRC_RSRP_GRP;
	ipa_reg_save.rsrc_cnts.ddr.resource_group = IPA_HW_DDR_SRC_RSRP_GRP;

	rsrc_type = 0;
	ipa_reg_save.rsrc_cnts.pcie.src.pkt_cntxt =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_PCIE_SRC_RSRP_GRP);

	ipa_reg_save.rsrc_cnts.ddr.src.pkt_cntxt =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_DDR_SRC_RSRP_GRP);

	rsrc_type = 1;
	ipa_reg_save.rsrc_cnts.pcie.src.descriptor_list =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_PCIE_SRC_RSRP_GRP);

	ipa_reg_save.rsrc_cnts.ddr.src.descriptor_list =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_DDR_SRC_RSRP_GRP);

	rsrc_type = 2;
	ipa_reg_save.rsrc_cnts.pcie.src.data_descriptor_buffer =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_PCIE_SRC_RSRP_GRP);

	ipa_reg_save.rsrc_cnts.ddr.src.data_descriptor_buffer =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_DDR_SRC_RSRP_GRP);

	rsrc_type = 3;
	ipa_reg_save.rsrc_cnts.pcie.src.hps_dmars =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_PCIE_SRC_RSRP_GRP);

	ipa_reg_save.rsrc_cnts.ddr.src.hps_dmars =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_DDR_SRC_RSRP_GRP);

	rsrc_type = 4;
	ipa_reg_save.rsrc_cnts.pcie.src.reserved_acks =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_PCIE_SRC_RSRP_GRP);

	ipa_reg_save.rsrc_cnts.ddr.src.reserved_acks =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_DDR_SRC_RSRP_GRP);

	rsrc_type = 5;
	ipa_reg_save.rsrc_cnts.pcie.dst.reserved_sectors =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_PCIE_DEST_RSRP_GRP);

	ipa_reg_save.rsrc_cnts.ddr.dst.reserved_sectors =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_DDR_DEST_RSRP_GRP);

	rsrc_type = 6;
	ipa_reg_save.rsrc_cnts.pcie.dst.dps_dmars =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_PCIE_DEST_RSRP_GRP);

	ipa_reg_save.rsrc_cnts.ddr.dst.dps_dmars =
		IPA_DEBUG_TESTBUS_GET_RSRC_TYPE_CNT(rsrc_type,
						    IPA_HW_DDR_DEST_RSRP_GRP);
}

/*
 * FUNCTION:  ipa_hal_save_regs_ipa_cmdq
 *
 * This function saves the various IPA CMDQ registers
 *
 * @param
 *
 * @return
 */
static void ipa_hal_save_regs_ipa_cmdq(void)
{
	int32_t i;
	union ipa_hwio_def_ipa_rx_hps_cmdq_cmd_u rx_hps_cmdq_cmd = { { 0 } };
	union ipa_hwio_def_ipa_hps_dps_cmdq_cmd_u hps_dps_cmdq_cmd = { { 0 } };
	union ipa_hwio_def_ipa_dps_tx_cmdq_cmd_u dps_tx_cmdq_cmd = { { 0 } };
	union ipa_hwio_def_ipa_ackmngr_cmdq_cmd_u ackmngr_cmdq_cmd = { { 0 } };
	union ipa_hwio_def_ipa_prod_ackmngr_cmdq_cmd_u
		prod_ackmngr_cmdq_cmd = { { 0 } };
	union ipa_hwio_def_ipa_ntf_tx_cmdq_cmd_u ntf_tx_cmdq_cmd = { { 0 } };

	/* Save RX_HPS CMDQ   */
	for (i = 0; i < IPA_DEBUG_CMDQ_HPS_SELECT_NUM_GROUPS; i++) {
		rx_hps_cmdq_cmd.def.rd_req = 0;
		rx_hps_cmdq_cmd.def.cmd_client = i;
		IPA_MASKED_WRITE_SCALER_REG(IPA_RX_HPS_CMDQ_CMD,
				     rx_hps_cmdq_cmd.value);
		ipa_reg_save.ipa.dbg.ipa_rx_hps_cmdq_count_arr[i].value =
			IPA_READ_SCALER_REG(IPA_RX_HPS_CMDQ_COUNT);
		ipa_reg_save.ipa.dbg.ipa_rx_hps_cmdq_status_arr[i].value =
			IPA_READ_SCALER_REG(IPA_RX_HPS_CMDQ_STATUS);
		rx_hps_cmdq_cmd.def.rd_req = 1;
		rx_hps_cmdq_cmd.def.cmd_client = i;
		IPA_MASKED_WRITE_SCALER_REG(IPA_RX_HPS_CMDQ_CMD,
				     rx_hps_cmdq_cmd.value);
		ipa_reg_save.ipa.dbg.ipa_rx_hps_cmdq_data_rd_0_arr[i].value =
			IPA_READ_SCALER_REG(IPA_RX_HPS_CMDQ_DATA_RD_0);
		ipa_reg_save.ipa.dbg.ipa_rx_hps_cmdq_data_rd_1_arr[i].value =
			IPA_READ_SCALER_REG(IPA_RX_HPS_CMDQ_DATA_RD_1);
		ipa_reg_save.ipa.dbg.ipa_rx_hps_cmdq_data_rd_2_arr[i].value =
			IPA_READ_SCALER_REG(IPA_RX_HPS_CMDQ_DATA_RD_2);
		ipa_reg_save.ipa.dbg.ipa_rx_hps_cmdq_data_rd_3_arr[i].value =
			IPA_READ_SCALER_REG(IPA_RX_HPS_CMDQ_DATA_RD_3);
	}

	/* Save HPS_DPS CMDQ   */
	for (i = 0; i < IPA_TESTBUS_SEL_EP_MAX + 1; i++) {
		hps_dps_cmdq_cmd.def.rd_req = 0;
		hps_dps_cmdq_cmd.def.cmd_client = i;
		IPA_MASKED_WRITE_SCALER_REG(IPA_HPS_DPS_CMDQ_CMD,
				     hps_dps_cmdq_cmd.value);
		ipa_reg_save.ipa.dbg.ipa_hps_dps_cmdq_status_arr[i].value =
			IPA_READ_SCALER_REG(IPA_HPS_DPS_CMDQ_STATUS);
		ipa_reg_save.ipa.dbg.ipa_hps_dps_cmdq_count_arr[i].value =
			IPA_READ_SCALER_REG(IPA_HPS_DPS_CMDQ_COUNT);

		hps_dps_cmdq_cmd.def.rd_req = 1;
		hps_dps_cmdq_cmd.def.cmd_client = i;
		IPA_MASKED_WRITE_SCALER_REG(IPA_HPS_DPS_CMDQ_CMD,
				     hps_dps_cmdq_cmd.value);
		ipa_reg_save.ipa.dbg.ipa_hps_dps_cmdq_data_rd_0_arr[i].value =
			IPA_READ_SCALER_REG(IPA_HPS_DPS_CMDQ_DATA_RD_0);
	}

	/* Save DPS_TX CMDQ   */
	for (i = 0; i < IPA_DEBUG_CMDQ_DPS_SELECT_NUM_GROUPS; i++) {
		dps_tx_cmdq_cmd.def.cmd_client = i;
		dps_tx_cmdq_cmd.def.rd_req = 0;
		IPA_MASKED_WRITE_SCALER_REG(IPA_DPS_TX_CMDQ_CMD,
				     dps_tx_cmdq_cmd.value);
		ipa_reg_save.ipa.dbg.ipa_dps_tx_cmdq_status_arr[i].value =
			IPA_READ_SCALER_REG(IPA_DPS_TX_CMDQ_STATUS);
		ipa_reg_save.ipa.dbg.ipa_dps_tx_cmdq_count_arr[i].value =
			IPA_READ_SCALER_REG(IPA_DPS_TX_CMDQ_COUNT);

		dps_tx_cmdq_cmd.def.cmd_client = i;
		dps_tx_cmdq_cmd.def.rd_req = 1;
		IPA_MASKED_WRITE_SCALER_REG(IPA_DPS_TX_CMDQ_CMD,
				     dps_tx_cmdq_cmd.value);
		ipa_reg_save.ipa.dbg.ipa_dps_tx_cmdq_data_rd_0_arr[i].value =
			IPA_READ_SCALER_REG(IPA_DPS_TX_CMDQ_DATA_RD_0);
	}

	/* Save ACKMNGR CMDQ   */
	for (i = 0; i < IPA_DEBUG_CMDQ_DPS_SELECT_NUM_GROUPS; i++) {
		ackmngr_cmdq_cmd.def.rd_req = 0;
		ackmngr_cmdq_cmd.def.cmd_client = i;
		IPA_MASKED_WRITE_SCALER_REG(IPA_ACKMNGR_CMDQ_CMD,
				     ackmngr_cmdq_cmd.value);
		ipa_reg_save.ipa.dbg.ipa_ackmngr_cmdq_status_arr[i].value =
			IPA_READ_SCALER_REG(IPA_ACKMNGR_CMDQ_STATUS);
		ipa_reg_save.ipa.dbg.ipa_ackmngr_cmdq_count_arr[i].value =
			IPA_READ_SCALER_REG(IPA_ACKMNGR_CMDQ_COUNT);

		ackmngr_cmdq_cmd.def.rd_req = 1;
		ackmngr_cmdq_cmd.def.cmd_client = i;
		IPA_MASKED_WRITE_SCALER_REG(IPA_ACKMNGR_CMDQ_CMD,
				     ackmngr_cmdq_cmd.value);
		ipa_reg_save.ipa.dbg.ipa_ackmngr_cmdq_data_rd_arr[i].value =
			IPA_READ_SCALER_REG(IPA_ACKMNGR_CMDQ_DATA_RD);
	}

	/* Save PROD ACKMNGR CMDQ   */
	for (i = 0; i < IPA_TESTBUS_SEL_EP_MAX + 1; i++) {
		prod_ackmngr_cmdq_cmd.def.rd_req = 0;
		prod_ackmngr_cmdq_cmd.def.cmd_client = i;
		IPA_MASKED_WRITE_SCALER_REG(IPA_PROD_ACKMNGR_CMDQ_CMD,
				     prod_ackmngr_cmdq_cmd.value);
		ipa_reg_save.ipa.dbg.ipa_prod_ackmngr_cmdq_status_arr[i].value
			= IPA_READ_SCALER_REG(
				IPA_PROD_ACKMNGR_CMDQ_STATUS);
		ipa_reg_save.ipa.dbg.ipa_prod_ackmngr_cmdq_count_arr[i].value =
			IPA_READ_SCALER_REG(IPA_PROD_ACKMNGR_CMDQ_COUNT);
		prod_ackmngr_cmdq_cmd.def.rd_req = 1;
		prod_ackmngr_cmdq_cmd.def.cmd_client = i;
		IPA_MASKED_WRITE_SCALER_REG(IPA_PROD_ACKMNGR_CMDQ_CMD,
				     prod_ackmngr_cmdq_cmd.value);
		ipa_reg_save.ipa.dbg.ipa_prod_ackmngr_cmdq_data_rd_arr[
			i].value =
			IPA_READ_SCALER_REG(
				IPA_PROD_ACKMNGR_CMDQ_DATA_RD);
	}

	/* Save NTF_TX CMDQ   */
	for (i = 0; i < IPA_TESTBUS_SEL_EP_MAX + 1; i++) {
		ntf_tx_cmdq_cmd.def.rd_req = 0;
		ntf_tx_cmdq_cmd.def.cmd_client = i;
		IPA_MASKED_WRITE_SCALER_REG(IPA_NTF_TX_CMDQ_CMD,
				     ntf_tx_cmdq_cmd.value);
		ipa_reg_save.ipa.dbg.ipa_ntf_tx_cmdq_status_arr[i].value =
			IPA_READ_SCALER_REG(IPA_NTF_TX_CMDQ_STATUS);
		ipa_reg_save.ipa.dbg.ipa_ntf_tx_cmdq_count_arr[i].value =
			IPA_READ_SCALER_REG(IPA_NTF_TX_CMDQ_COUNT);
		ntf_tx_cmdq_cmd.def.rd_req = 1;
		ntf_tx_cmdq_cmd.def.cmd_client = i;
		IPA_MASKED_WRITE_SCALER_REG(IPA_NTF_TX_CMDQ_CMD,
				     ntf_tx_cmdq_cmd.value);
		ipa_reg_save.ipa.dbg.ipa_ntf_tx_cmdq_data_rd_0_arr[i].value =
			IPA_READ_SCALER_REG(IPA_NTF_TX_CMDQ_DATA_RD_0);
	}
}

/*
 * FUNCTION:  ipa_hal_save_regs_save_ipa_testbus
 *
 * This function saves the IPA testbus
 *
 * @param
 *
 * @return
 */
static void ipa_hal_save_regs_save_ipa_testbus(void)
{
	s32 sel_internal, sel_external, sel_ep;
	union ipa_hwio_def_ipa_testbus_sel_u testbus_sel = { { 0 } };

	if (ipa_reg_save.ipa.testbus == NULL) {
		/*
		 * Test-bus structure not allocated - exit test-bus collection
		 */
		IPADBG("ipa_reg_save.ipa.testbus was not allocated\n");
		return;
	}

	/* Enable Test-bus */
	testbus_sel.value = 0;
	testbus_sel.def.testbus_en = true;

	IPA_WRITE_SCALER_REG(IPA_TESTBUS_SEL, testbus_sel.value);

	for (sel_external = 0;
		 sel_external <= IPA_TESTBUS_SEL_EXTERNAL_MAX;
		 sel_external++) {

		for (sel_internal = 0;
			 sel_internal <= IPA_TESTBUS_SEL_INTERNAL_MAX;
			 sel_internal++) {

			testbus_sel.value = 0;

			testbus_sel.def.pipe_select = 0;
			testbus_sel.def.external_block_select =
				sel_external;
			testbus_sel.def.internal_block_select =
				sel_internal;

			IPA_MASKED_WRITE_SCALER_REG(
				IPA_TESTBUS_SEL,
				testbus_sel.value);

			ipa_reg_save.ipa.testbus->global.global[
				sel_internal][sel_external].testbus_sel.value =
				testbus_sel.value;

			ipa_reg_save.ipa.testbus->global.global[
				sel_internal][sel_external].testbus_data.value =
				IPA_READ_SCALER_REG(IPA_DEBUG_DATA);
		}
	}

	/* Collect per EP test bus */
	for (sel_ep = 0;
		 sel_ep <= IPA_TESTBUS_SEL_EP_MAX;
		 sel_ep++) {

		for (sel_external = 0;
			 sel_external <=
				 IPA_TESTBUS_SEL_EXTERNAL_MAX;
			 sel_external++) {

			for (sel_internal = 0;
				 sel_internal <=
					 IPA_TESTBUS_SEL_INTERNAL_PIPE_MAX;
				 sel_internal++) {

				testbus_sel.value = 0;

				testbus_sel.def.pipe_select = sel_ep;
				testbus_sel.def.external_block_select =
					sel_external;
				testbus_sel.def.internal_block_select =
					sel_internal;

				IPA_MASKED_WRITE_SCALER_REG(
					IPA_TESTBUS_SEL,
					testbus_sel.value);

				ipa_reg_save.ipa.testbus->ep[sel_ep].entry_ep[
					sel_internal][sel_external].
					testbus_sel.value =
					testbus_sel.value;

				ipa_reg_save.ipa.testbus->ep[sel_ep].entry_ep[
					sel_internal][sel_external].
					testbus_data.value =
					IPA_READ_SCALER_REG(
						IPA_DEBUG_DATA);
			}
		}
	}

	/* Disable Test-bus */
	testbus_sel.value = 0;

	IPA_WRITE_SCALER_REG(
		IPA_TESTBUS_SEL,
		testbus_sel.value);
}

/*
 * FUNCTION:  ipa_reg_save_init
 *
 * This function initializes and memsets the register save struct.
 *
 * @param
 *
 * @return
 */
int ipa_reg_save_init(u32 value)
{
	u32 i, num_regs = ARRAY_SIZE(ipa_regs_to_save_array);

	if (!ipa3_ctx->do_register_collection_on_crash)
		return 0;

	memset(&ipa_reg_save, value, sizeof(ipa_reg_save));

	ipa_reg_save.ipa.testbus = NULL;

	if (ipa3_ctx->do_testbus_collection_on_crash) {
		memset(ipa_testbus_mem, value, sizeof(ipa_testbus_mem));
		ipa_reg_save.ipa.testbus =
		    (struct ipa_reg_save_ipa_testbus_s *) ipa_testbus_mem;
	}

	/* setup access for register collection/dump on crash */
	IPADBG("Mapping 0x%x bytes starting at 0x%x\n",
	       ipa3_ctx->entire_ipa_block_size,
	       ipa3_ctx->ipa_wrapper_base);

	ipa3_ctx->reg_collection_base =
		ioremap_nocache(ipa3_ctx->ipa_wrapper_base,
			ipa3_ctx->entire_ipa_block_size);

	if (!ipa3_ctx->reg_collection_base) {
		IPAERR(":register collection ioremap err\n");
		goto alloc_fail1;
	}

	num_regs -=
		(CONFIG_IPA3_REGDUMP_NUM_EXTRA_ENDP_REGS *
		 IPA_REG_SAVE_NUM_EXTRA_ENDP_REGS);

	for (i = 0;
		 i < (CONFIG_IPA3_REGDUMP_NUM_EXTRA_ENDP_REGS *
			  IPA_REG_SAVE_NUM_EXTRA_ENDP_REGS);
		 i++)
		*(ipa_regs_to_save_array[num_regs + i].dst_addr) = 0x0;

	ipa_reg_save.ipa.ipa_gsi_ptr  = NULL;
	ipa_reg_save.ipa.ipa_seq_ptr  = NULL;
	ipa_reg_save.ipa.ipa_hram_ptr = NULL;
	ipa_reg_save.ipa.ipa_mbox_ptr = NULL;
	ipa_reg_save.ipa.ipa_sram_ptr = NULL;
	ipa_reg_save.ipa.ipa_iu_ptr   = NULL;

	if (ipa3_ctx->do_ram_collection_on_crash) {
		ipa_reg_save.ipa.ipa_iu_ptr =
			alloc_and_init(IPA_IU_SIZE, value);
		if (!ipa_reg_save.ipa.ipa_iu_ptr) {
			IPAERR("ipa_iu_ptr memory alloc failed\n");
			goto alloc_fail2;
		}

		ipa_reg_save.ipa.ipa_sram_ptr =
			alloc_and_init(IPA_SRAM_SIZE, value);
		if (!ipa_reg_save.ipa.ipa_sram_ptr) {
			IPAERR("ipa_sram_ptr memory alloc failed\n");
			goto alloc_fail2;
		}

		ipa_reg_save.ipa.ipa_mbox_ptr =
			alloc_and_init(IPA_MBOX_SIZE, value);
		if (!ipa_reg_save.ipa.ipa_mbox_ptr) {
			IPAERR("ipa_mbox_ptr memory alloc failed\n");
			goto alloc_fail2;
		}

		ipa_reg_save.ipa.ipa_hram_ptr =
			alloc_and_init(IPA_HRAM_SIZE, value);
		if (!ipa_reg_save.ipa.ipa_hram_ptr) {
			IPAERR("ipa_hram_ptr memory alloc failed\n");
			goto alloc_fail2;
		}

		ipa_reg_save.ipa.ipa_seq_ptr =
			alloc_and_init(IPA_SEQ_SIZE, value);
		if (!ipa_reg_save.ipa.ipa_seq_ptr) {
			IPAERR("ipa_seq_ptr memory alloc failed\n");
			goto alloc_fail2;
		}

		ipa_reg_save.ipa.ipa_gsi_ptr =
			alloc_and_init(IPA_GSI_SIZE, value);
		if (!ipa_reg_save.ipa.ipa_gsi_ptr) {
			IPAERR("ipa_gsi_ptr memory alloc failed\n");
			goto alloc_fail2;
		}
	}

	return 0;

alloc_fail2:
	kfree(ipa_reg_save.ipa.ipa_seq_ptr);
	kfree(ipa_reg_save.ipa.ipa_hram_ptr);
	kfree(ipa_reg_save.ipa.ipa_mbox_ptr);
	kfree(ipa_reg_save.ipa.ipa_sram_ptr);
	kfree(ipa_reg_save.ipa.ipa_iu_ptr);
	iounmap(ipa3_ctx->reg_collection_base);
alloc_fail1:
	return -ENOMEM;
}

/*
 * FUNCTION:  ipa_hal_save_regs_rsrc_db
 *
 * This function saves the various IPA RSRC_MNGR_DB registers
 *
 * @param
 *
 * @return
 */
static void ipa_hal_save_regs_rsrc_db(void)
{
	u32 rsrc_type = 0;
	u32 rsrc_id = 0;
	u32 rsrc_group = 0;
	union ipa_hwio_def_ipa_rsrc_mngr_db_cfg_u
		ipa_rsrc_mngr_db_cfg = { { 0 } };

	ipa_rsrc_mngr_db_cfg.def.rsrc_grp_sel = rsrc_group;

	for (rsrc_type = 0; rsrc_type <= IPA_RSCR_MNGR_DB_RSRC_TYPE_MAX;
	     rsrc_type++) {
		for (rsrc_id = 0; rsrc_id <= IPA_RSCR_MNGR_DB_RSRC_ID_MAX;
		     rsrc_id++) {
			ipa_rsrc_mngr_db_cfg.def.rsrc_id_sel = rsrc_id;
			ipa_rsrc_mngr_db_cfg.def.rsrc_type_sel = rsrc_type;
			IPA_MASKED_WRITE_SCALER_REG(IPA_RSRC_MNGR_DB_CFG,
					     ipa_rsrc_mngr_db_cfg.value);
			ipa_reg_save.ipa.dbg.ipa_rsrc_mngr_db_rsrc_read_arr
			    [rsrc_type][rsrc_id].value =
			    IPA_READ_SCALER_REG(
					IPA_RSRC_MNGR_DB_RSRC_READ);
			ipa_reg_save.ipa.dbg.ipa_rsrc_mngr_db_list_read_arr
			    [rsrc_type][rsrc_id].value =
			    IPA_READ_SCALER_REG(
					IPA_RSRC_MNGR_DB_LIST_READ);
		}
	}
}

/*
 * FUNCTION:  ipa_reg_save_anomaly_check
 *
 * Checks RX state and TX state upon crash dump collection and prints
 * anomalies.
 *
 * TBD- Add more anomaly checks in the future.
 *
 * @return
 */
static void ipa_reg_save_anomaly_check(void)
{
	if ((ipa_reg_save.ipa.gen.ipa_state.rx_wait != 0)
	    || (ipa_reg_save.ipa.gen.ipa_state.rx_idle != 1)) {
		IPADBG(
		    "RX ACTIVITY, ipa_state.rx_wait = %d, ipa_state.rx_idle = %d, ipa_state_rx_active.endpoints = %d (bitmask)\n",
		    ipa_reg_save.ipa.gen.ipa_state.rx_wait,
		    ipa_reg_save.ipa.gen.ipa_state.rx_idle,
		    ipa_reg_save.ipa.gen.ipa_state_rx_active.endpoints);

		if (ipa_reg_save.ipa.gen.ipa_state.tx_idle != 1) {
			IPADBG(
			    "TX ACTIVITY, ipa_state.idle = %d, ipa_state_tx_wrapper.tx0_idle = %d, ipa_state_tx_wrapper.tx1_idle = %d\n",
			    ipa_reg_save.ipa.gen.ipa_state.tx_idle,
			    ipa_reg_save.ipa.gen.ipa_state_tx_wrapper.tx0_idle,
			    ipa_reg_save.ipa.gen.ipa_state_tx_wrapper.tx1_idle);

			IPADBG(
			    "ipa_state_tx0.last_cmd_pipe = %d, ipa_state_tx1.last_cmd_pipe = %d\n",
			    ipa_reg_save.ipa.gen.ipa_state_tx0.last_cmd_pipe,
			    ipa_reg_save.ipa.gen.ipa_state_tx1.last_cmd_pipe);
		}
	}
}
