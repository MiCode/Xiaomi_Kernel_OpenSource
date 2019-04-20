/* Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
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
#include "ipa_i.h"
#include <linux/dmapool.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include "ipa_qmi_service.h"

#define IPA_HOLB_TMR_DIS 0x0

#define IPA_HW_INTERFACE_WDI_VERSION 0x0001
#define IPA_HW_WDI_RX_MBOX_START_INDEX 48
#define IPA_HW_WDI_TX_MBOX_START_INDEX 50
#define IPA_WDI_RING_ALIGNMENT 8

#define IPA_GSI_EVT_RING_INT_MODT (32 * 1) /* 1ms under 32KHz clock */

#define IPA_AGGR_PKT_LIMIT 1
#define IPA_AGGR_HARD_BYTE_LIMIT 2 /*2 Kbytes Agger hard byte limit*/
#define UPDATE_RI_MODERATION_THRESHOLD		8


#define IPA_WDI_CONNECTED BIT(0)
#define IPA_WDI_ENABLED BIT(1)
#define IPA_WDI_RESUMED BIT(2)
#define IPA_UC_POLL_SLEEP_USEC 100

#define GSI_STOP_MAX_RETRY_CNT 10

struct ipa_wdi_res {
	struct ipa_wdi_buffer_info *res;
	unsigned int nents;
	bool valid;
};

static struct ipa_wdi_res wdi_res[IPA_WDI_MAX_RES];

static void ipa3_uc_wdi_loaded_handler(void);

/**
 * enum ipa_hw_2_cpu_wdi_events - Values that represent HW event to be sent to
 * CPU.
 * @IPA_HW_2_CPU_EVENT_WDI_ERROR : Event to specify that HW detected an error
 * in WDI
 */
enum ipa_hw_2_cpu_wdi_events {
	IPA_HW_2_CPU_EVENT_WDI_ERROR =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 0),
};

/**
 * enum ipa_hw_wdi_channel_states - Values that represent WDI channel state
 * machine.
 * @IPA_HW_WDI_CHANNEL_STATE_INITED_DISABLED : Channel is initialized but
 * disabled
 * @IPA_HW_WDI_CHANNEL_STATE_ENABLED_SUSPEND : Channel is enabled but in
 * suspended state
 * @IPA_HW_WDI_CHANNEL_STATE_RUNNING : Channel is running. Entered after
 * SET_UP_COMMAND is processed successfully
 * @IPA_HW_WDI_CHANNEL_STATE_ERROR : Channel is in error state
 * @IPA_HW_WDI_CHANNEL_STATE_INVALID : Invalid state. Shall not be in use in
 * operational scenario
 *
 * These states apply to both Tx and Rx paths. These do not reflect the
 * sub-state the state machine may be in.
 */
enum ipa_hw_wdi_channel_states {
	IPA_HW_WDI_CHANNEL_STATE_INITED_DISABLED = 1,
	IPA_HW_WDI_CHANNEL_STATE_ENABLED_SUSPEND = 2,
	IPA_HW_WDI_CHANNEL_STATE_RUNNING         = 3,
	IPA_HW_WDI_CHANNEL_STATE_ERROR           = 4,
	IPA_HW_WDI_CHANNEL_STATE_INVALID         = 0xFF
};

/**
 * enum ipa3_cpu_2_hw_commands -  Values that represent the WDI commands from
 * CPU
 * @IPA_CPU_2_HW_CMD_WDI_TX_SET_UP : Command to set up WDI Tx Path
 * @IPA_CPU_2_HW_CMD_WDI_RX_SET_UP : Command to set up WDI Rx Path
 * @IPA_CPU_2_HW_CMD_WDI_RX_EXT_CFG : Provide extended config info for Rx path
 * @IPA_CPU_2_HW_CMD_WDI_CH_ENABLE : Command to enable a channel
 * @IPA_CPU_2_HW_CMD_WDI_CH_DISABLE : Command to disable a channel
 * @IPA_CPU_2_HW_CMD_WDI_CH_SUSPEND : Command to suspend a channel
 * @IPA_CPU_2_HW_CMD_WDI_CH_RESUME : Command to resume a channel
 * @IPA_CPU_2_HW_CMD_WDI_TEAR_DOWN : Command to tear down WDI Tx/ Rx Path
 */
enum ipa_cpu_2_hw_wdi_commands {
	IPA_CPU_2_HW_CMD_WDI_TX_SET_UP  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 0),
	IPA_CPU_2_HW_CMD_WDI_RX_SET_UP  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 1),
	IPA_CPU_2_HW_CMD_WDI_RX_EXT_CFG =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 2),
	IPA_CPU_2_HW_CMD_WDI_CH_ENABLE  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 3),
	IPA_CPU_2_HW_CMD_WDI_CH_DISABLE =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 4),
	IPA_CPU_2_HW_CMD_WDI_CH_SUSPEND =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 5),
	IPA_CPU_2_HW_CMD_WDI_CH_RESUME  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 6),
	IPA_CPU_2_HW_CMD_WDI_TEAR_DOWN  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 7),
};

/**
 * enum ipa_hw_2_cpu_cmd_resp_status -  Values that represent WDI related
 * command response status to be sent to CPU.
 */
enum ipa_hw_2_cpu_cmd_resp_status {
	IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS            =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 0),
	IPA_HW_2_CPU_MAX_WDI_TX_CHANNELS               =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 1),
	IPA_HW_2_CPU_WDI_CE_RING_OVERRUN_POSSIBILITY   =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 2),
	IPA_HW_2_CPU_WDI_CE_RING_SET_UP_FAILURE        =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 3),
	IPA_HW_2_CPU_WDI_CE_RING_PARAMS_UNALIGNED      =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 4),
	IPA_HW_2_CPU_WDI_COMP_RING_OVERRUN_POSSIBILITY =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 5),
	IPA_HW_2_CPU_WDI_COMP_RING_SET_UP_FAILURE      =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 6),
	IPA_HW_2_CPU_WDI_COMP_RING_PARAMS_UNALIGNED    =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 7),
	IPA_HW_2_CPU_WDI_UNKNOWN_TX_CHANNEL            =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 8),
	IPA_HW_2_CPU_WDI_TX_INVALID_FSM_TRANSITION     =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 9),
	IPA_HW_2_CPU_WDI_TX_FSM_TRANSITION_ERROR       =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 10),
	IPA_HW_2_CPU_MAX_WDI_RX_CHANNELS               =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 11),
	IPA_HW_2_CPU_WDI_RX_RING_PARAMS_UNALIGNED      =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 12),
	IPA_HW_2_CPU_WDI_RX_RING_SET_UP_FAILURE        =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 13),
	IPA_HW_2_CPU_WDI_UNKNOWN_RX_CHANNEL            =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 14),
	IPA_HW_2_CPU_WDI_RX_INVALID_FSM_TRANSITION     =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 15),
	IPA_HW_2_CPU_WDI_RX_FSM_TRANSITION_ERROR       =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 16),
};

/**
 * enum ipa_hw_wdi_errors - WDI specific error types.
 * @IPA_HW_WDI_ERROR_NONE : No error persists
 * @IPA_HW_WDI_CHANNEL_ERROR : Error is specific to channel
 */
enum ipa_hw_wdi_errors {
	IPA_HW_WDI_ERROR_NONE    = 0,
	IPA_HW_WDI_CHANNEL_ERROR = 1
};

/**
 * enum ipa_hw_wdi_ch_errors = List of WDI Channel error types. This is present
 * in the event param.
 * @IPA_HW_WDI_CH_ERR_NONE : No error persists
 * @IPA_HW_WDI_TX_COMP_RING_WP_UPDATE_FAIL : Write pointer update failed in Tx
 * Completion ring
 * @IPA_HW_WDI_TX_FSM_ERROR : Error in the state machine transition
 * @IPA_HW_WDI_TX_COMP_RE_FETCH_FAIL : Error while calculating num RE to bring
 * @IPA_HW_WDI_CH_ERR_RESERVED : Reserved - Not available for CPU to use
 */
enum ipa_hw_wdi_ch_errors {
	IPA_HW_WDI_CH_ERR_NONE                 = 0,
	IPA_HW_WDI_TX_COMP_RING_WP_UPDATE_FAIL = 1,
	IPA_HW_WDI_TX_FSM_ERROR                = 2,
	IPA_HW_WDI_TX_COMP_RE_FETCH_FAIL       = 3,
	IPA_HW_WDI_CH_ERR_RESERVED             = 0xFF
};

/**
 * struct IpaHwSharedMemWdiMapping_t  - Structure referring to the common and
 * WDI section of 128B shared memory located in offset zero of SW Partition in
 * IPA SRAM.
 *
 * The shared memory is used for communication between IPA HW and CPU.
 */
struct IpaHwSharedMemWdiMapping_t {
	struct IpaHwSharedMemCommonMapping_t common;
	u32 reserved_2B_28;
	u32 reserved_2F_2C;
	u32 reserved_33_30;
	u32 reserved_37_34;
	u32 reserved_3B_38;
	u32 reserved_3F_3C;
	u16 interfaceVersionWdi;
	u16 reserved_43_42;
	u8  wdi_tx_ch_0_state;
	u8  wdi_rx_ch_0_state;
	u16 reserved_47_46;
} __packed;

/**
 * struct IpaHwWdiTxSetUpCmdData_t - Structure holding the parameters for
 * IPA_CPU_2_HW_CMD_WDI_TX_SET_UP command.
 * @comp_ring_base_pa : This is the physical address of the base of the Tx
 * completion ring
 * @comp_ring_size : This is the size of the Tx completion ring
 * @reserved_comp_ring : Reserved field for expansion of Completion ring params
 * @ce_ring_base_pa : This is the physical address of the base of the Copy
 * Engine Source Ring
 * @ce_ring_size : Copy Engine Ring size
 * @reserved_ce_ring : Reserved field for expansion of CE ring params
 * @ce_ring_doorbell_pa : This is the physical address of the doorbell that the
 * IPA uC has to write into to trigger the copy engine
 * @num_tx_buffers : Number of pkt buffers allocated. The size of the CE ring
 * and the Tx completion ring has to be atleast ( num_tx_buffers + 1)
 * @ipa_pipe_number : This is the IPA pipe number that has to be used for the
 * Tx path
 * @reserved : Reserved field
 *
 * Parameters are sent as pointer thus should be reside in address accessible
 * to HW
 */
struct IpaHwWdiTxSetUpCmdData_t {
	u32 comp_ring_base_pa;
	u16 comp_ring_size;
	u16 reserved_comp_ring;
	u32 ce_ring_base_pa;
	u16 ce_ring_size;
	u16 reserved_ce_ring;
	u32 ce_ring_doorbell_pa;
	u16 num_tx_buffers;
	u8  ipa_pipe_number;
	u8  reserved;
} __packed;

struct IpaHwWdi2TxSetUpCmdData_t {
	u32 comp_ring_base_pa;
	u32 comp_ring_base_pa_hi;
	u16 comp_ring_size;
	u16 reserved_comp_ring;
	u32 ce_ring_base_pa;
	u32 ce_ring_base_pa_hi;
	u16 ce_ring_size;
	u16 reserved_ce_ring;
	u32 ce_ring_doorbell_pa;
	u32 ce_ring_doorbell_pa_hi;
	u16 num_tx_buffers;
	u8  ipa_pipe_number;
	u8  reserved;
} __packed;
/**
 * struct IpaHwWdiRxSetUpCmdData_t -  Structure holding the parameters for
 * IPA_CPU_2_HW_CMD_WDI_RX_SET_UP command.
 * @rx_ring_base_pa : This is the physical address of the base of the Rx ring
 * (containing Rx buffers)
 * @rx_ring_size : This is the size of the Rx ring
 * @rx_ring_rp_pa : This is the physical address of the location through which
 * IPA uc is expected to communicate about the Read pointer into the Rx Ring
 * @ipa_pipe_number : This is the IPA pipe number that has to be used for the
 * Rx path
 *
 * Parameters are sent as pointer thus should be reside in address accessible
 * to HW
 */
struct IpaHwWdiRxSetUpCmdData_t {
	u32 rx_ring_base_pa;
	u32 rx_ring_size;
	u32 rx_ring_rp_pa;
	u8  ipa_pipe_number;
} __packed;

struct IpaHwWdi2RxSetUpCmdData_t {
	u32 rx_ring_base_pa;
	u32 rx_ring_base_pa_hi;
	u32 rx_ring_size;
	u32 rx_ring_rp_pa;
	u32 rx_ring_rp_pa_hi;
	u32 rx_comp_ring_base_pa;
	u32 rx_comp_ring_base_pa_hi;
	u32 rx_comp_ring_size;
	u32 rx_comp_ring_wp_pa;
	u32 rx_comp_ring_wp_pa_hi;
	u8  ipa_pipe_number;
} __packed;
/**
 * union IpaHwWdiRxExtCfgCmdData_t - Structure holding the parameters for
 * IPA_CPU_2_HW_CMD_WDI_RX_EXT_CFG command.
 * @ipa_pipe_number : The IPA pipe number for which this config is passed
 * @qmap_id : QMAP ID to be set in the metadata register
 * @reserved : Reserved
 *
 * The parameters are passed as immediate params in the shared memory
 */
union IpaHwWdiRxExtCfgCmdData_t {
	struct IpaHwWdiRxExtCfgCmdParams_t {
		u32 ipa_pipe_number:8;
		u32 qmap_id:8;
		u32 reserved:16;
	} __packed params;
	u32 raw32b;
} __packed;

/**
 * union IpaHwWdiCommonChCmdData_t -  Structure holding the parameters for
 * IPA_CPU_2_HW_CMD_WDI_TEAR_DOWN,
 * IPA_CPU_2_HW_CMD_WDI_CH_ENABLE,
 * IPA_CPU_2_HW_CMD_WDI_CH_DISABLE,
 * IPA_CPU_2_HW_CMD_WDI_CH_SUSPEND,
 * IPA_CPU_2_HW_CMD_WDI_CH_RESUME command.
 * @ipa_pipe_number :  The IPA pipe number. This could be Tx or an Rx pipe
 * @reserved : Reserved
 *
 * The parameters are passed as immediate params in the shared memory
 */
union IpaHwWdiCommonChCmdData_t {
	struct IpaHwWdiCommonChCmdParams_t {
		u32 ipa_pipe_number:8;
		u32 reserved:24;
	} __packed params;
	u32 raw32b;
} __packed;

/**
 * union IpaHwWdiErrorEventData_t - parameters for IPA_HW_2_CPU_EVENT_WDI_ERROR
 * event.
 * @wdi_error_type : The IPA pipe number to be torn down. This could be Tx or
 * an Rx pipe
 * @reserved : Reserved
 * @ipa_pipe_number : IPA pipe number on which error has happened. Applicable
 * only if error type indicates channel error
 * @wdi_ch_err_type : Information about the channel error (if available)
 *
 * The parameters are passed as immediate params in the shared memory
 */
union IpaHwWdiErrorEventData_t {
	struct IpaHwWdiErrorEventParams_t {
		u32 wdi_error_type:8;
		u32 reserved:8;
		u32 ipa_pipe_number:8;
		u32 wdi_ch_err_type:8;
	} __packed params;
	u32 raw32b;
} __packed;

static void ipa3_uc_wdi_event_log_info_handler(
struct IpaHwEventLogInfoData_t *uc_event_top_mmio)

{
	struct Ipa3HwEventInfoData_t *stats_ptr = &uc_event_top_mmio->statsInfo;

	if ((uc_event_top_mmio->protocolMask &
		(1 << IPA_HW_PROTOCOL_WDI)) == 0) {
		IPAERR("WDI protocol missing 0x%x\n",
			uc_event_top_mmio->protocolMask);
		return;
	}

	if (stats_ptr->featureInfo[IPA_HW_PROTOCOL_WDI].params.size !=
		sizeof(struct IpaHwStatsWDIInfoData_t)) {
		IPAERR("wdi stats sz invalid exp=%zu is=%u\n",
			sizeof(struct IpaHwStatsWDIInfoData_t),
			stats_ptr->featureInfo[
				IPA_HW_PROTOCOL_WDI].params.size);
		return;
	}

	ipa3_ctx->uc_wdi_ctx.wdi_uc_stats_ofst =
		stats_ptr->baseAddrOffset +
		stats_ptr->featureInfo[IPA_HW_PROTOCOL_WDI].params.offset;
	IPAERR("WDI stats ofst=0x%x\n", ipa3_ctx->uc_wdi_ctx.wdi_uc_stats_ofst);
	if (ipa3_ctx->uc_wdi_ctx.wdi_uc_stats_ofst +
		sizeof(struct IpaHwStatsWDIInfoData_t) >=
		ipa3_ctx->ctrl->ipa_reg_base_ofst +
		ipahal_get_reg_n_ofst(IPA_SW_AREA_RAM_DIRECT_ACCESS_n, 0) +
		ipa3_ctx->smem_sz) {
		IPAERR("uc_wdi_stats 0x%x outside SRAM\n",
			ipa3_ctx->uc_wdi_ctx.wdi_uc_stats_ofst);
		return;
	}

	ipa3_ctx->uc_wdi_ctx.wdi_uc_stats_mmio =
		ioremap(ipa3_ctx->ipa_wrapper_base +
		ipa3_ctx->uc_wdi_ctx.wdi_uc_stats_ofst,
		sizeof(struct IpaHwStatsWDIInfoData_t));
	if (!ipa3_ctx->uc_wdi_ctx.wdi_uc_stats_mmio) {
		IPAERR("fail to ioremap uc wdi stats\n");
		return;
	}
}

static void ipa3_uc_wdi_event_handler(struct IpaHwSharedMemCommonMapping_t
				     *uc_sram_mmio)

{
	union IpaHwWdiErrorEventData_t wdi_evt;
	struct IpaHwSharedMemWdiMapping_t *wdi_sram_mmio_ext;

	if (uc_sram_mmio->eventOp ==
		IPA_HW_2_CPU_EVENT_WDI_ERROR) {
		wdi_evt.raw32b = uc_sram_mmio->eventParams;
		IPADBG("uC WDI evt errType=%u pipe=%d cherrType=%u\n",
			wdi_evt.params.wdi_error_type,
			wdi_evt.params.ipa_pipe_number,
			wdi_evt.params.wdi_ch_err_type);
		wdi_sram_mmio_ext =
			(struct IpaHwSharedMemWdiMapping_t *)
			uc_sram_mmio;
		IPADBG("tx_ch_state=%u rx_ch_state=%u\n",
			wdi_sram_mmio_ext->wdi_tx_ch_0_state,
			wdi_sram_mmio_ext->wdi_rx_ch_0_state);
	}
}

/**
 * ipa3_get_wdi_stats() - Query WDI statistics from uc
 * @stats:	[inout] stats blob from client populated by driver
 *
 * Returns:	0 on success, negative on failure
 *
 * @note Cannot be called from atomic context
 *
 */
int ipa3_get_wdi_stats(struct IpaHwStatsWDIInfoData_t *stats)
{
#define TX_STATS(y) stats->tx_ch_stats.y = \
	ipa3_ctx->uc_wdi_ctx.wdi_uc_stats_mmio->tx_ch_stats.y
#define RX_STATS(y) stats->rx_ch_stats.y = \
	ipa3_ctx->uc_wdi_ctx.wdi_uc_stats_mmio->rx_ch_stats.y

	if (!stats || !ipa3_ctx->uc_wdi_ctx.wdi_uc_stats_mmio) {
		IPAERR("bad parms stats=%pK wdi_stats=%pK\n",
			stats,
			ipa3_ctx->uc_wdi_ctx.wdi_uc_stats_mmio);
		return -EINVAL;
	}
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	TX_STATS(num_pkts_processed);
	TX_STATS(copy_engine_doorbell_value);
	TX_STATS(num_db_fired);
	TX_STATS(tx_comp_ring_stats.ringFull);
	TX_STATS(tx_comp_ring_stats.ringEmpty);
	TX_STATS(tx_comp_ring_stats.ringUsageHigh);
	TX_STATS(tx_comp_ring_stats.ringUsageLow);
	TX_STATS(tx_comp_ring_stats.RingUtilCount);
	TX_STATS(bam_stats.bamFifoFull);
	TX_STATS(bam_stats.bamFifoEmpty);
	TX_STATS(bam_stats.bamFifoUsageHigh);
	TX_STATS(bam_stats.bamFifoUsageLow);
	TX_STATS(bam_stats.bamUtilCount);
	TX_STATS(num_db);
	TX_STATS(num_unexpected_db);
	TX_STATS(num_bam_int_handled);
	TX_STATS(num_bam_int_in_non_running_state);
	TX_STATS(num_qmb_int_handled);
	TX_STATS(num_bam_int_handled_while_wait_for_bam);

	RX_STATS(max_outstanding_pkts);
	RX_STATS(num_pkts_processed);
	RX_STATS(rx_ring_rp_value);
	RX_STATS(rx_ind_ring_stats.ringFull);
	RX_STATS(rx_ind_ring_stats.ringEmpty);
	RX_STATS(rx_ind_ring_stats.ringUsageHigh);
	RX_STATS(rx_ind_ring_stats.ringUsageLow);
	RX_STATS(rx_ind_ring_stats.RingUtilCount);
	RX_STATS(bam_stats.bamFifoFull);
	RX_STATS(bam_stats.bamFifoEmpty);
	RX_STATS(bam_stats.bamFifoUsageHigh);
	RX_STATS(bam_stats.bamFifoUsageLow);
	RX_STATS(bam_stats.bamUtilCount);
	RX_STATS(num_bam_int_handled);
	RX_STATS(num_db);
	RX_STATS(num_unexpected_db);
	RX_STATS(num_pkts_in_dis_uninit_state);
	RX_STATS(num_ic_inj_vdev_change);
	RX_STATS(num_ic_inj_fw_desc_change);
	RX_STATS(num_qmb_int_handled);
	RX_STATS(reserved1);
	RX_STATS(reserved2);

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return 0;
}

int ipa3_wdi_init(void)
{
	struct ipa3_uc_hdlrs uc_wdi_cbs = { 0 };

	uc_wdi_cbs.ipa_uc_event_hdlr = ipa3_uc_wdi_event_handler;
	uc_wdi_cbs.ipa_uc_event_log_info_hdlr =
		ipa3_uc_wdi_event_log_info_handler;
	uc_wdi_cbs.ipa_uc_loaded_hdlr =
		ipa3_uc_wdi_loaded_handler;

	ipa3_uc_register_handlers(IPA_HW_FEATURE_WDI, &uc_wdi_cbs);

	return 0;
}

static int ipa_create_ap_smmu_mapping_pa(phys_addr_t pa, size_t len,
		bool device, unsigned long *iova)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_AP);
	unsigned long va = roundup(cb->next_addr, PAGE_SIZE);
	int prot = IOMMU_READ | IOMMU_WRITE;
	size_t true_len = roundup(len + pa - rounddown(pa, PAGE_SIZE),
			PAGE_SIZE);
	int ret;

	if (!cb->valid) {
		IPAERR("No SMMU CB setup\n");
		return -EINVAL;
	}

	ret = ipa3_iommu_map(cb->mapping->domain, va, rounddown(pa, PAGE_SIZE),
			true_len,
			device ? (prot | IOMMU_MMIO) : prot);
	if (ret) {
		IPAERR("iommu map failed for pa=%pa len=%zu\n", &pa, true_len);
		return -EINVAL;
	}

	ipa3_ctx->wdi_map_cnt++;
	cb->next_addr = va + true_len;
	*iova = va + pa - rounddown(pa, PAGE_SIZE);
	return 0;
}

static int ipa_create_uc_smmu_mapping_pa(phys_addr_t pa, size_t len,
		bool device, unsigned long *iova)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_UC);
	unsigned long va = roundup(cb->next_addr, PAGE_SIZE);
	int prot = IOMMU_READ | IOMMU_WRITE;
	size_t true_len = roundup(len + pa - rounddown(pa, PAGE_SIZE),
			PAGE_SIZE);
	int ret;

	if (!cb->valid) {
		IPAERR("No SMMU CB setup\n");
		return -EINVAL;
	}

	ret = ipa3_iommu_map(cb->mapping->domain, va, rounddown(pa, PAGE_SIZE),
			true_len,
			device ? (prot | IOMMU_MMIO) : prot);
	if (ret) {
		IPAERR("iommu map failed for pa=%pa len=%zu\n", &pa, true_len);
		return -EINVAL;
	}

	ipa3_ctx->wdi_map_cnt++;
	cb->next_addr = va + true_len;
	*iova = va + pa - rounddown(pa, PAGE_SIZE);
	return 0;
}

static int ipa_create_ap_smmu_mapping_sgt(struct sg_table *sgt,
		unsigned long *iova)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_AP);
	unsigned long va = roundup(cb->next_addr, PAGE_SIZE);
	int prot = IOMMU_READ | IOMMU_WRITE;
	int ret, i;
	struct scatterlist *sg;
	unsigned long start_iova = va;
	phys_addr_t phys;
	size_t len;
	int count = 0;

	if (!cb->valid) {
		IPAERR("No SMMU CB setup\n");
		return -EINVAL;
	}
	if (!sgt) {
		IPAERR("Bad parameters, scatter / gather list is NULL\n");
		return -EINVAL;
	}

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		/* directly get sg_tbl PA from wlan-driver */
		phys = sg->dma_address;
		len = PAGE_ALIGN(sg->offset + sg->length);

		ret = ipa3_iommu_map(cb->mapping->domain, va, phys, len, prot);
		if (ret) {
			IPAERR("iommu map failed for pa=%pa len=%zu\n",
					&phys, len);
			goto bad_mapping;
		}
		va += len;
		ipa3_ctx->wdi_map_cnt++;
		count++;
	}
	cb->next_addr = va;
	*iova = start_iova;

	return 0;

bad_mapping:
	for_each_sg(sgt->sgl, sg, count, i)
		iommu_unmap(cb->mapping->domain, sg_dma_address(sg),
				sg_dma_len(sg));
	return -EINVAL;
}


static int ipa_create_uc_smmu_mapping_sgt(struct sg_table *sgt,
		unsigned long *iova)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_UC);
	unsigned long va = roundup(cb->next_addr, PAGE_SIZE);
	int prot = IOMMU_READ | IOMMU_WRITE;
	int ret;
	int i;
	struct scatterlist *sg;
	unsigned long start_iova = va;
	phys_addr_t phys;
	size_t len;
	int count = 0;

	if (!cb->valid) {
		IPAERR("No SMMU CB setup\n");
		return -EINVAL;
	}
	if (!sgt) {
		IPAERR("Bad parameters, scatter / gather list is NULL\n");
		return -EINVAL;
	}

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		/* directly get sg_tbl PA from wlan-driver */
		phys = sg->dma_address;
		len = PAGE_ALIGN(sg->offset + sg->length);

		ret = ipa3_iommu_map(cb->mapping->domain, va, phys, len, prot);
		if (ret) {
			IPAERR("iommu map failed for pa=%pa len=%zu\n",
					&phys, len);
			goto bad_mapping;
		}
		va += len;
		ipa3_ctx->wdi_map_cnt++;
		count++;
	}
	cb->next_addr = va;
	*iova = start_iova;

	return 0;

bad_mapping:
	for_each_sg(sgt->sgl, sg, count, i)
		iommu_unmap(cb->mapping->domain, sg_dma_address(sg),
				sg_dma_len(sg));
	return -EINVAL;
}

static void ipa_release_ap_smmu_mappings(enum ipa_client_type client)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_AP);
	int i, j, start, end;

	if (IPA_CLIENT_IS_CONS(client)) {
		start = IPA_WDI_TX_RING_RES;
		end = IPA_WDI_CE_DB_RES;
	} else {
		start = IPA_WDI_RX_RING_RES;
		if (ipa3_ctx->ipa_wdi2)
			end = IPA_WDI_RX_COMP_RING_WP_RES;
		else
			end = IPA_WDI_RX_RING_RP_RES;
	}

	for (i = start; i <= end; i++) {
		if (wdi_res[i].valid) {
			for (j = 0; j < wdi_res[i].nents; j++) {
				iommu_unmap(cb->mapping->domain,
					wdi_res[i].res[j].iova,
					wdi_res[i].res[j].size);
				ipa3_ctx->wdi_map_cnt--;
			}
			kfree(wdi_res[i].res);
			wdi_res[i].valid = false;
		}
	}

	if (ipa3_ctx->wdi_map_cnt == 0)
		cb->next_addr = cb->va_end;
}

static void ipa_release_uc_smmu_mappings(enum ipa_client_type client)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_UC);
	int i;
	int j;
	int start;
	int end;

	if (IPA_CLIENT_IS_CONS(client)) {
		start = IPA_WDI_TX_RING_RES;
		end = IPA_WDI_CE_DB_RES;
	} else {
		start = IPA_WDI_RX_RING_RES;
		if (ipa3_ctx->ipa_wdi2)
			end = IPA_WDI_RX_COMP_RING_WP_RES;
		else
			end = IPA_WDI_RX_RING_RP_RES;
	}

	for (i = start; i <= end; i++) {
		if (wdi_res[i].valid) {
			for (j = 0; j < wdi_res[i].nents; j++) {
				iommu_unmap(cb->mapping->domain,
					wdi_res[i].res[j].iova,
					wdi_res[i].res[j].size);
				ipa3_ctx->wdi_map_cnt--;
			}
			kfree(wdi_res[i].res);
			wdi_res[i].valid = false;
		}
	}

	if (ipa3_ctx->wdi_map_cnt == 0)
		cb->next_addr = cb->va_end;

}

static void ipa_save_uc_smmu_mapping_pa(int res_idx, phys_addr_t pa,
		unsigned long iova, size_t len)
{
	IPADBG("--res_idx=%d pa=0x%pa iova=0x%lx sz=0x%zx\n", res_idx,
		&pa, iova, len);
	wdi_res[res_idx].res = kzalloc(sizeof(*wdi_res[res_idx].res),
		GFP_KERNEL);
	if (!wdi_res[res_idx].res) {
		WARN_ON(1);
		return;
	}
	wdi_res[res_idx].nents = 1;
	wdi_res[res_idx].valid = true;
	wdi_res[res_idx].res->pa = rounddown(pa, PAGE_SIZE);
	wdi_res[res_idx].res->iova = rounddown(iova, PAGE_SIZE);
	wdi_res[res_idx].res->size = roundup(len + pa - rounddown(pa,
				PAGE_SIZE), PAGE_SIZE);
	IPADBG("res_idx=%d pa=0x%pa iova=0x%lx sz=0x%zx\n", res_idx,
			&wdi_res[res_idx].res->pa, wdi_res[res_idx].res->iova,
			wdi_res[res_idx].res->size);
}

static void ipa_save_uc_smmu_mapping_sgt(int res_idx, struct sg_table *sgt,
		unsigned long iova)
{
	int i;
	struct scatterlist *sg;
	unsigned long curr_iova = iova;

	if (!sgt) {
		IPAERR("Bad parameters, scatter / gather list is NULL\n");
		return;
	}

	wdi_res[res_idx].res = kcalloc(sgt->nents,
		sizeof(*wdi_res[res_idx].res),
			GFP_KERNEL);
	if (!wdi_res[res_idx].res) {
		WARN_ON(1);
		return;
	}
	wdi_res[res_idx].nents = sgt->nents;
	wdi_res[res_idx].valid = true;
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		/* directly get sg_tbl PA from wlan */
		wdi_res[res_idx].res[i].pa = sg->dma_address;
		wdi_res[res_idx].res[i].iova = curr_iova;
		wdi_res[res_idx].res[i].size = PAGE_ALIGN(sg->offset +
				sg->length);
		IPADBG("res_idx=%d pa=0x%pa iova=0x%lx sz=0x%zx\n", res_idx,
			&wdi_res[res_idx].res[i].pa,
			wdi_res[res_idx].res[i].iova,
			wdi_res[res_idx].res[i].size);
		curr_iova += wdi_res[res_idx].res[i].size;
	}
}

int ipa_create_uc_smmu_mapping(int res_idx, bool wlan_smmu_en,
		phys_addr_t pa, struct sg_table *sgt, size_t len, bool device,
		unsigned long *iova)
{
	/* support for SMMU on WLAN but no SMMU on IPA */
	if (wlan_smmu_en && ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_UC]) {
		IPAERR("Unsupported SMMU pairing\n");
		return -EINVAL;
	}

	/* legacy: no SMMUs on either end */
	if (!wlan_smmu_en && ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_UC]) {
		*iova = pa;
		return 0;
	}

	/* no SMMU on WLAN but SMMU on IPA */
	if (!wlan_smmu_en && !ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_UC]) {
		if (ipa_create_uc_smmu_mapping_pa(pa, len,
			(res_idx == IPA_WDI_CE_DB_RES) ? true : false, iova)) {
			IPAERR("Fail to create mapping res %d\n", res_idx);
			return -EFAULT;
		}
		ipa_save_uc_smmu_mapping_pa(res_idx, pa, *iova, len);
		return 0;
	}

	/* SMMU on WLAN and SMMU on IPA */
	if (wlan_smmu_en && !ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_UC]) {
		switch (res_idx) {
		case IPA_WDI_RX_RING_RP_RES:
		case IPA_WDI_RX_COMP_RING_WP_RES:
		case IPA_WDI_CE_DB_RES:
		case IPA_WDI_TX_DB_RES:
			if (ipa_create_uc_smmu_mapping_pa(pa, len,
				(res_idx == IPA_WDI_CE_DB_RES) ? true : false,
				iova)) {
				IPAERR("Fail to create mapping res %d\n",
						res_idx);
				return -EFAULT;
			}
			ipa_save_uc_smmu_mapping_pa(res_idx, pa, *iova, len);
			break;
		case IPA_WDI_RX_RING_RES:
		case IPA_WDI_RX_COMP_RING_RES:
		case IPA_WDI_TX_RING_RES:
		case IPA_WDI_CE_RING_RES:
			if (ipa_create_uc_smmu_mapping_sgt(sgt, iova)) {
				IPAERR("Fail to create mapping res %d\n",
						res_idx);
				WARN_ON(1);
				return -EFAULT;
			}
			ipa_save_uc_smmu_mapping_sgt(res_idx, sgt, *iova);
			break;
		default:
			WARN_ON(1);
		}
	}

	return 0;
}

static int ipa_create_gsi_smmu_mapping(int res_idx, bool wlan_smmu_en,
		phys_addr_t pa, struct sg_table *sgt, size_t len, bool device,
		unsigned long *iova)
{
	/* support for SMMU on WLAN but no SMMU on IPA */
	if (wlan_smmu_en && ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_AP]) {
		IPAERR("Unsupported SMMU pairing\n");
		return -EINVAL;
	}

	/* legacy: no SMMUs on either end */
	if (!wlan_smmu_en && ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_AP]) {
		*iova = pa;
		return 0;
	}

	/* no SMMU on WLAN but SMMU on IPA */
	if (!wlan_smmu_en && !ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_AP]) {
		if (ipa_create_ap_smmu_mapping_pa(pa, len,
				(res_idx == IPA_WDI_CE_DB_RES) ? true : false,
					iova)) {
			IPAERR("Fail to create mapping res %d\n",
					res_idx);
			return -EFAULT;
		}
		ipa_save_uc_smmu_mapping_pa(res_idx, pa, *iova, len);
		return 0;
	}
	/* SMMU on WLAN and SMMU on IPA */
	if (wlan_smmu_en && !ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_AP]) {
		switch (res_idx) {
		case IPA_WDI_RX_RING_RP_RES:
		case IPA_WDI_RX_COMP_RING_WP_RES:
		case IPA_WDI_CE_DB_RES:
		case IPA_WDI_TX_DB_RES:
			if (ipa_create_ap_smmu_mapping_pa(pa, len,
				(res_idx == IPA_WDI_CE_DB_RES) ? true : false,
						iova)) {
				IPAERR("Fail to create mapping res %d\n",
						res_idx);
				return -EFAULT;
			}
			ipa_save_uc_smmu_mapping_pa(res_idx, pa, *iova, len);
			break;
		case IPA_WDI_RX_RING_RES:
		case IPA_WDI_RX_COMP_RING_RES:
		case IPA_WDI_TX_RING_RES:
		case IPA_WDI_CE_RING_RES:
			if (ipa_create_ap_smmu_mapping_sgt(sgt, iova)) {
				IPAERR("Fail to create mapping res %d\n",
						res_idx);
				return -EFAULT;
			}
			ipa_save_uc_smmu_mapping_sgt(res_idx, sgt, *iova);
			break;
		default:
			WARN_ON(1);
		}
	}
	return 0;
}

static void ipa_gsi_evt_ring_err_cb(struct gsi_evt_err_notify *notify)
{
	switch (notify->evt_id) {
	case GSI_EVT_OUT_OF_BUFFERS_ERR:
		IPAERR("Got GSI_EVT_OUT_OF_BUFFERS_ERR\n");
		break;
	case GSI_EVT_OUT_OF_RESOURCES_ERR:
		IPAERR("Got GSI_EVT_OUT_OF_RESOURCES_ERR\n");
		break;
	case GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR:
		IPAERR("Got GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR\n");
		break;
	case GSI_EVT_EVT_RING_EMPTY_ERR:
		IPAERR("Got GSI_EVT_EVT_RING_EMPTY_ERR\n");
		break;
	default:
		IPAERR("Unexpected err evt: %d\n", notify->evt_id);
	}
	ipa_assert();
}

static void ipa_gsi_chan_err_cb(struct gsi_chan_err_notify *notify)
{
	switch (notify->evt_id) {
	case GSI_CHAN_INVALID_TRE_ERR:
		IPAERR("Got GSI_CHAN_INVALID_TRE_ERR\n");
		break;
	case GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR:
		IPAERR("Got GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR\n");
		break;
	case GSI_CHAN_OUT_OF_BUFFERS_ERR:
		IPAERR("Got GSI_CHAN_OUT_OF_BUFFERS_ERR\n");
		break;
	case GSI_CHAN_OUT_OF_RESOURCES_ERR:
		IPAERR("Got GSI_CHAN_OUT_OF_RESOURCES_ERR\n");
		break;
	case GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR:
		IPAERR("Got GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR\n");
		break;
	case GSI_CHAN_HWO_1_ERR:
		IPAERR("Got GSI_CHAN_HWO_1_ERR\n");
		break;
	default:
		IPAERR("Unexpected err evt: %d\n", notify->evt_id);
	}
	ipa_assert();
}
static int ipa3_wdi2_gsi_alloc_evt_ring(
			struct gsi_evt_ring_props *evt_ring_props,
			enum ipa_client_type client,
			unsigned long *evt_ring_hdl)
{
	union __packed gsi_evt_scratch evt_scratch;
	int result = -EFAULT;

	/* GSI EVENT RING allocation */
	evt_ring_props->intf = GSI_EVT_CHTYPE_WDI2_EV;
	evt_ring_props->intr = GSI_INTR_IRQ;

	if (IPA_CLIENT_IS_PROD(client))
		evt_ring_props->re_size = GSI_EVT_RING_RE_SIZE_8B;
	else
		evt_ring_props->re_size = GSI_EVT_RING_RE_SIZE_16B;

	evt_ring_props->exclusive = true;
	evt_ring_props->err_cb = ipa_gsi_evt_ring_err_cb;
	evt_ring_props->user_data = NULL;
	evt_ring_props->int_modt = IPA_GSI_EVT_RING_INT_MODT;
	evt_ring_props->int_modc = 1;
	IPADBG("GSI evt ring len: %d\n", evt_ring_props->ring_len);
	IPADBG("client=%d moderation threshold cycles=%u cnt=%u\n",
			client,
			evt_ring_props->int_modt,
			evt_ring_props->int_modc);


	result = gsi_alloc_evt_ring(evt_ring_props,
			ipa3_ctx->gsi_dev_hdl, evt_ring_hdl);
	IPADBG("gsi_alloc_evt_ring result: %d\n", result);
	if (result != GSI_STATUS_SUCCESS)
		goto fail_alloc_evt_ring;

	evt_scratch.wdi.update_ri_moderation_config =
				UPDATE_RI_MODERATION_THRESHOLD;
	evt_scratch.wdi.update_ri_mod_timer_running = 0;
	evt_scratch.wdi.evt_comp_count = 0;
	evt_scratch.wdi.last_update_ri = 0;
	evt_scratch.wdi.resvd1 = 0;
	evt_scratch.wdi.resvd2 = 0;
	result = gsi_write_evt_ring_scratch(*evt_ring_hdl, evt_scratch);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("Error writing WDI event ring scratch: %d\n", result);
		gsi_dealloc_evt_ring(*evt_ring_hdl);
		return -EFAULT;
	}

fail_alloc_evt_ring:
	return result;

}
static int ipa3_wdi2_gsi_alloc_channel_ring(
				struct gsi_chan_props *channel_props,
				enum ipa_client_type client,
				unsigned long *chan_hdl,
				unsigned long evt_ring_hdl)
{
	int result = -EFAULT;
	const struct ipa_gsi_ep_config *ep_cfg;

	ep_cfg = ipa3_get_gsi_ep_info(client);
	if (!ep_cfg) {
		IPAERR("Failed getting GSI EP info for client=%d\n",
				client);
		return -EPERM;
	}

	if (IPA_CLIENT_IS_PROD(client)) {
		IPAERR("Client is PROD\n");
		channel_props->dir = GSI_CHAN_DIR_TO_GSI;
		channel_props->re_size = GSI_CHAN_RE_SIZE_16B;
	} else {
		IPAERR("Client is CONS");
		channel_props->dir = GSI_CHAN_DIR_FROM_GSI;
		channel_props->re_size = GSI_CHAN_RE_SIZE_8B;
	}

	channel_props->prot = GSI_CHAN_PROT_WDI2;
	channel_props->ch_id = ep_cfg->ipa_gsi_chan_num;
	channel_props->evt_ring_hdl = evt_ring_hdl;

	IPADBG("ch_id: %d\n", channel_props->ch_id);
	IPADBG("evt_ring_hdl: %ld\n", channel_props->evt_ring_hdl);
	IPADBG("re_size: %d\n", channel_props->re_size);
	IPADBG("Config GSI xfer cb func");
	IPADBG("GSI channel ring len: %d\n", channel_props->ring_len);
	channel_props->xfer_cb = NULL;

	IPADBG("channel ring  base vaddr = 0x%pa\n",
			channel_props->ring_base_vaddr);

	channel_props->use_db_eng = GSI_CHAN_DB_MODE;
	channel_props->max_prefetch = GSI_ONE_PREFETCH_SEG;
	channel_props->prefetch_mode = ep_cfg->prefetch_mode;
	channel_props->low_weight = 1;
	channel_props->err_cb = ipa_gsi_chan_err_cb;

	IPADBG("Allocating GSI channel\n");
	result =  gsi_alloc_channel(channel_props,
			ipa3_ctx->gsi_dev_hdl,
			chan_hdl);
	if (result != GSI_STATUS_SUCCESS)
		goto fail_alloc_channel;

	IPADBG("gsi_chan_hdl: %ld\n", *chan_hdl);

fail_alloc_channel:
	return result;
}


int ipa3_connect_gsi_wdi_pipe(struct ipa_wdi_in_params *in,
	struct ipa_wdi_out_params *out)
{
	u32 len;
	int ipa_ep_idx, num_ring_ele;
	int result = -EFAULT;
	enum gsi_status gsi_res;
	struct ipa3_ep_context *ep;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	struct gsi_chan_props gsi_channel_props;
	struct gsi_evt_ring_props gsi_evt_ring_props;
	union __packed gsi_channel_scratch gsi_scratch;
	phys_addr_t pa;
	unsigned long va;
	u32 gsi_db_reg_phs_addr_lsb;
	u32 gsi_db_reg_phs_addr_msb;

	ipa_ep_idx = ipa3_get_ep_mapping(in->sys.client);
	if (ipa_ep_idx == -1) {
		IPAERR("fail to alloc EP.\n");
		goto fail;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (ep->valid) {
		IPAERR("EP already allocated.\n");
		goto fail;
	}

	IPA_ACTIVE_CLIENTS_INC_EP(in->sys.client);

	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));
	memset(&gsi_evt_ring_props, 0, sizeof(gsi_evt_ring_props));
	memset(&gsi_channel_props, 0, sizeof(gsi_channel_props));
	memset(&gsi_scratch, 0, sizeof(gsi_scratch));

	IPADBG("client=%d ep=%d\n", in->sys.client, ipa_ep_idx);

	if (IPA_CLIENT_IS_CONS(in->sys.client)) {
		if (in->smmu_enabled) {
			IPADBG("comp_ring_size=%d\n",
				in->u.dl_smmu.comp_ring_size);
			IPADBG("ce_ring_size=%d\n", in->u.dl_smmu.ce_ring_size);
			IPADBG("ce_ring_doorbell_pa=0x%pa\n",
					&in->u.dl_smmu.ce_door_bell_pa);
			IPADBG("num_tx_buffers=%d\n",
				in->u.dl_smmu.num_tx_buffers);
		} else {
			IPADBG("comp_ring_base_pa=0x%pa\n",
					&in->u.dl.comp_ring_base_pa);
			IPADBG("comp_ring_size=%d\n", in->u.dl.comp_ring_size);
			IPADBG("ce_ring_base_pa=0x%pa\n",
				&in->u.dl.ce_ring_base_pa);
			IPADBG("ce_ring_size=%d\n", in->u.dl.ce_ring_size);
			IPADBG("ce_ring_doorbell_pa=0x%pa\n",
					&in->u.dl.ce_door_bell_pa);
			IPADBG("num_tx_buffers=%d\n", in->u.dl.num_tx_buffers);
		}
	} else {
		if (in->smmu_enabled) {
			IPADBG("rx_ring_size=%d\n",
				in->u.ul_smmu.rdy_ring_size);
			IPADBG("rx_ring_rp_pa=0x%pa\n",
				&in->u.ul_smmu.rdy_ring_rp_pa);
			IPADBG("rx_comp_ring_size=%d\n",
				in->u.ul_smmu.rdy_comp_ring_size);
			IPADBG("rx_comp_ring_wp_pa=0x%pa\n",
				&in->u.ul_smmu.rdy_comp_ring_wp_pa);
			ipa3_ctx->wdi2_ctx.rdy_ring_rp_pa =
				in->u.ul_smmu.rdy_ring_rp_pa;
			ipa3_ctx->wdi2_ctx.rdy_ring_size =
				in->u.ul_smmu.rdy_ring_size;
			ipa3_ctx->wdi2_ctx.rdy_comp_ring_wp_pa =
				in->u.ul_smmu.rdy_comp_ring_wp_pa;
			ipa3_ctx->wdi2_ctx.rdy_comp_ring_size =
				in->u.ul_smmu.rdy_comp_ring_size;
		} else {
			IPADBG("rx_ring_base_pa=0x%pa\n",
				&in->u.ul.rdy_ring_base_pa);
			IPADBG("rx_ring_size=%d\n",
				in->u.ul.rdy_ring_size);
			IPADBG("rx_ring_rp_pa=0x%pa\n",
				&in->u.ul.rdy_ring_rp_pa);
			IPADBG("rx_comp_ring_base_pa=0x%pa\n",
				&in->u.ul.rdy_comp_ring_base_pa);
			IPADBG("rx_comp_ring_size=%d\n",
				in->u.ul.rdy_comp_ring_size);
			IPADBG("rx_comp_ring_wp_pa=0x%pa\n",
				&in->u.ul.rdy_comp_ring_wp_pa);
			ipa3_ctx->wdi2_ctx.rdy_ring_base_pa =
				in->u.ul.rdy_ring_base_pa;
			ipa3_ctx->wdi2_ctx.rdy_ring_rp_pa =
				in->u.ul.rdy_ring_rp_pa;
			ipa3_ctx->wdi2_ctx.rdy_ring_size =
				in->u.ul.rdy_ring_size;
			ipa3_ctx->wdi2_ctx.rdy_comp_ring_base_pa =
				in->u.ul.rdy_comp_ring_base_pa;
			ipa3_ctx->wdi2_ctx.rdy_comp_ring_wp_pa =
				in->u.ul.rdy_comp_ring_wp_pa;
			ipa3_ctx->wdi2_ctx.rdy_comp_ring_size =
				in->u.ul.rdy_comp_ring_size;
		}
	}
	if (IPA_CLIENT_IS_CONS(in->sys.client)) {
		len = in->smmu_enabled ? in->u.dl_smmu.comp_ring_size :
			in->u.dl.comp_ring_size;
		IPADBG("TX ring smmu_en=%d ring_size=%d %d\n",
				in->smmu_enabled,
				in->u.dl_smmu.comp_ring_size,
				in->u.dl.comp_ring_size);
		if (ipa_create_gsi_smmu_mapping(IPA_WDI_TX_RING_RES,
					in->smmu_enabled,
					in->u.dl.comp_ring_base_pa,
					&in->u.dl_smmu.comp_ring,
					len,
					false,
					&va)) {
			IPAERR("fail to create gsi mapping TX ring.\n");
			result = -ENOMEM;
			goto gsi_timeout;
		}
		gsi_channel_props.ring_base_addr = va;
		gsi_channel_props.ring_base_vaddr = NULL;
		gsi_channel_props.ring_len = len;

		len = in->smmu_enabled ? in->u.dl_smmu.ce_ring_size :
			in->u.dl.ce_ring_size;
		IPADBG("CE ring smmu_en=%d ring_size=%d %d\n",
				in->smmu_enabled,
				in->u.dl_smmu.ce_ring_size,
				in->u.dl.ce_ring_size);

		/* WA: wlan passed ce_ring sg_table PA directly */
		if (ipa_create_gsi_smmu_mapping(IPA_WDI_CE_RING_RES,
					in->smmu_enabled,
					in->u.dl.ce_ring_base_pa,
					&in->u.dl_smmu.ce_ring,
					len,
					false,
					&va)) {
			IPAERR("fail to create gsi mapping CE ring.\n");
			result = -ENOMEM;
			goto gsi_timeout;
		}
		gsi_evt_ring_props.ring_base_addr = va;
		gsi_evt_ring_props.ring_base_vaddr = NULL;
		gsi_evt_ring_props.ring_len = len;
		pa = in->smmu_enabled ? in->u.dl_smmu.ce_door_bell_pa :
			in->u.dl.ce_door_bell_pa;
		if (ipa_create_gsi_smmu_mapping(IPA_WDI_CE_DB_RES,
					in->smmu_enabled,
					pa,
					NULL,
					4,
					true,
					&va)) {
			IPAERR("fail to create gsi mapping CE DB.\n");
			result = -ENOMEM;
			goto gsi_timeout;
		}
		gsi_evt_ring_props.rp_update_addr = va;
	} else {
		len = in->smmu_enabled ? in->u.ul_smmu.rdy_ring_size :
			in->u.ul.rdy_ring_size;
		IPADBG("RX ring smmu_en=%d ring_size=%d %d\n",
				in->smmu_enabled,
				in->u.ul_smmu.rdy_ring_size,
				in->u.ul.rdy_ring_size);
		if (ipa_create_gsi_smmu_mapping(IPA_WDI_RX_RING_RES,
					in->smmu_enabled,
					in->u.ul.rdy_ring_base_pa,
					&in->u.ul_smmu.rdy_ring,
					len,
					false,
					&va)) {
			IPAERR("fail to create gsi RX ring.\n");
			result = -ENOMEM;
			goto gsi_timeout;
		}
		gsi_channel_props.ring_base_addr = va;
		gsi_channel_props.ring_base_vaddr =  NULL;
		gsi_channel_props.ring_len = len;
		pa = in->smmu_enabled ? in->u.ul_smmu.rdy_ring_rp_pa :
			in->u.ul.rdy_ring_rp_pa;
		if (ipa_create_gsi_smmu_mapping(IPA_WDI_RX_RING_RP_RES,
					in->smmu_enabled,
					pa,
					NULL,
					4,
					false,
					&va)) {
			IPAERR("fail to create gsi RX rng RP\n");
			result = -ENOMEM;
			goto gsi_timeout;
		}
		gsi_scratch.wdi.wifi_rx_ri_addr_low =
			va & 0xFFFFFFFF;
		gsi_scratch.wdi.wifi_rx_ri_addr_high =
			(va & 0xFFFFF00000000) >> 32;

		len = in->smmu_enabled ?
			in->u.ul_smmu.rdy_comp_ring_size :
			in->u.ul.rdy_comp_ring_size;
		IPADBG("RX ring smmu_en=%d comp_ring_size=%d %d\n",
				in->smmu_enabled,
				in->u.ul_smmu.rdy_comp_ring_size,
				in->u.ul.rdy_comp_ring_size);
		if (ipa_create_gsi_smmu_mapping(
					IPA_WDI_RX_COMP_RING_RES,
					in->smmu_enabled,
					in->u.ul.rdy_comp_ring_base_pa,
					&in->u.ul_smmu.rdy_comp_ring,
					len,
					false,
					&va)) {
			IPAERR("fail to create gsi RX comp_ring.\n");
			result = -ENOMEM;
			goto gsi_timeout;
		}
		gsi_evt_ring_props.ring_base_addr = va;
		gsi_evt_ring_props.ring_base_vaddr = NULL;
		gsi_evt_ring_props.ring_len = len;
		pa = in->smmu_enabled ?
			in->u.ul_smmu.rdy_comp_ring_wp_pa :
			in->u.ul.rdy_comp_ring_wp_pa;
		if (ipa_create_gsi_smmu_mapping(
					IPA_WDI_RX_COMP_RING_WP_RES,
					in->smmu_enabled,
					pa,
					NULL,
					4,
					false,
					&va)) {
			IPAERR("fail to create gsi RX comp_rng WP\n");
			result = -ENOMEM;
			goto gsi_timeout;
		}
		gsi_evt_ring_props.rp_update_addr = va;
		gsi_scratch.wdi.wdi_rx_vdev_id = 0xff;
		gsi_scratch.wdi.wdi_rx_fw_desc = 0xff;
		gsi_scratch.wdi.endp_metadatareg_offset =
					ipahal_get_reg_mn_ofst(
					IPA_ENDP_INIT_HDR_METADATA_n, 0,
							ipa_ep_idx)/4;
		gsi_scratch.wdi.qmap_id = 0;
	}

	ep->valid = 1;
	ep->client = in->sys.client;
	ep->keep_ipa_awake = in->sys.keep_ipa_awake;
	ep->skip_ep_cfg = in->sys.skip_ep_cfg;
	ep->client_notify = in->sys.notify;
	ep->priv = in->sys.priv;
	if (IPA_CLIENT_IS_PROD(in->sys.client)) {
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_delay = true;
		ipa3_cfg_ep_ctrl(ipa_ep_idx, &ep_cfg_ctrl);
	}

	if (IPA_CLIENT_IS_CONS(in->sys.client)) {
		in->sys.ipa_ep_cfg.aggr.aggr_en = IPA_ENABLE_AGGR;
		in->sys.ipa_ep_cfg.aggr.aggr = IPA_GENERIC;
		in->sys.ipa_ep_cfg.aggr.aggr_pkt_limit = IPA_AGGR_PKT_LIMIT;
		in->sys.ipa_ep_cfg.aggr.aggr_byte_limit =
						IPA_AGGR_HARD_BYTE_LIMIT;
		in->sys.ipa_ep_cfg.aggr.aggr_hard_byte_limit_en =
						IPA_ENABLE_AGGR;
	}
	if (!ep->skip_ep_cfg) {
		if (ipa3_cfg_ep(ipa_ep_idx, &in->sys.ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto ipa_cfg_ep_fail;
		}
		IPADBG("ep configuration successful\n");
	} else {
		IPADBG("Skipping endpoint configuration.\n");
	}
	result = ipa3_wdi2_gsi_alloc_evt_ring(&gsi_evt_ring_props,
				in->sys.client,
				&ep->gsi_evt_ring_hdl);
	if (result)
		goto fail_alloc_evt_ring;
	/*copy mem info */
	ep->gsi_mem_info.evt_ring_len = gsi_evt_ring_props.ring_len;
	ep->gsi_mem_info.evt_ring_base_addr = gsi_evt_ring_props.ring_base_addr;
	ep->gsi_mem_info.evt_ring_base_vaddr =
				gsi_evt_ring_props.ring_base_vaddr;
	IPAERR("evt ring len: %d\n", ep->gsi_mem_info.evt_ring_len);
	IPAERR("element size: %d\n", gsi_evt_ring_props.re_size);

	result = ipa3_wdi2_gsi_alloc_channel_ring(&gsi_channel_props,
					in->sys.client,
				&ep->gsi_chan_hdl, ep->gsi_evt_ring_hdl);
	if (result)
		goto fail_alloc_channel;
	ep->gsi_mem_info.chan_ring_len = gsi_channel_props.ring_len;
	ep->gsi_mem_info.chan_ring_base_addr = gsi_channel_props.ring_base_addr;
	ep->gsi_mem_info.chan_ring_base_vaddr =
		gsi_channel_props.ring_base_vaddr;

	num_ring_ele = ep->gsi_mem_info.evt_ring_len/gsi_evt_ring_props.re_size;
	IPAERR("UPDATE_RI_MODERATION_THRESHOLD: %d\n", num_ring_ele);
	gsi_scratch.wdi.update_ri_moderation_threshold =
		min(UPDATE_RI_MODERATION_THRESHOLD, num_ring_ele);
	gsi_scratch.wdi.update_ri_moderation_counter = 0;
	gsi_scratch.wdi.wdi_rx_tre_proc_in_progress = 0;
	gsi_scratch.wdi.resv1 = 0;
	result = gsi_write_channel_scratch(ep->gsi_chan_hdl,
			gsi_scratch);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("gsi_write_channel_scratch failed %d\n",
				result);
		goto fail_write_channel_scratch;
	}

	/* for AP+STA stats update */
	if (in->wdi_notify)
		ipa3_ctx->uc_wdi_ctx.stats_notify = in->wdi_notify;
	else
		IPADBG("in->wdi_notify is null\n");

	ipa3_enable_data_path(ipa_ep_idx);

	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(in->sys.client))
		ipa3_install_dflt_flt_rules(ipa_ep_idx);

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(in->sys.client);

	IPADBG("GSI connected.\n");
	gsi_res = gsi_query_channel_db_addr(ep->gsi_chan_hdl,
			&gsi_db_reg_phs_addr_lsb,
			&gsi_db_reg_phs_addr_msb);
	out->uc_door_bell_pa = gsi_db_reg_phs_addr_lsb;
	IPADBG("GSI query result: %d\n", gsi_res);
	IPADBG("GSI lsb addr: %d\n", gsi_db_reg_phs_addr_lsb);
	IPADBG("GSI msb addr: %d\n", gsi_db_reg_phs_addr_msb);

	ep->gsi_offload_state |= IPA_WDI_CONNECTED;
	out->clnt_hdl = ipa_ep_idx;
	return 0;

fail_write_channel_scratch:
	gsi_dealloc_channel(ep->gsi_chan_hdl);
fail_alloc_channel:
	if (ep->gsi_evt_ring_hdl != ~0) {
		gsi_dealloc_evt_ring(ep->gsi_evt_ring_hdl);
		ep->gsi_evt_ring_hdl = ~0;
	}
fail_alloc_evt_ring:
ipa_cfg_ep_fail:
	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));
gsi_timeout:
	ipa_release_ap_smmu_mappings(in->sys.client);
	IPA_ACTIVE_CLIENTS_DEC_EP(in->sys.client);
fail:
	return result;
}

/**
 * ipa3_connect_wdi_pipe() - WDI client connect
 * @in:	[in] input parameters from client
 * @out: [out] output params to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_connect_wdi_pipe(struct ipa_wdi_in_params *in,
		struct ipa_wdi_out_params *out)
{
	int ipa_ep_idx;
	int result = -EFAULT;
	struct ipa3_ep_context *ep;
	struct ipa_mem_buffer cmd;
	struct IpaHwWdiTxSetUpCmdData_t *tx;
	struct IpaHwWdiRxSetUpCmdData_t *rx;
	struct IpaHwWdi2TxSetUpCmdData_t *tx_2;
	struct IpaHwWdi2RxSetUpCmdData_t *rx_2;

	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	unsigned long va;
	phys_addr_t pa;
	u32 len;

	if (in == NULL || out == NULL || in->sys.client >= IPA_CLIENT_MAX) {
		IPAERR("bad parm. in=%pK out=%pK\n", in, out);
		if (in)
			IPAERR("client = %d\n", in->sys.client);
		return -EINVAL;
	}

	if (!in->smmu_enabled) {
		if (IPA_CLIENT_IS_CONS(in->sys.client)) {
			if (in->u.dl.comp_ring_base_pa %
				IPA_WDI_RING_ALIGNMENT ||
				in->u.dl.ce_ring_base_pa %
				IPA_WDI_RING_ALIGNMENT) {
				IPAERR("alignment failure on TX\n");
					return -EINVAL;
			}
		} else {
			if (in->u.ul.rdy_ring_base_pa %
				IPA_WDI_RING_ALIGNMENT) {
				IPAERR("alignment failure on RX\n");
				return -EINVAL;
			}
		}
	}

	if (ipa3_ctx->ipa_wdi2_over_gsi)
		return ipa3_connect_gsi_wdi_pipe(in, out);

	result = ipa3_uc_state_check();
	if (result)
		return result;

	ipa_ep_idx = ipa3_get_ep_mapping(in->sys.client);
	if (ipa_ep_idx == -1) {
		IPAERR("fail to alloc EP.\n");
		goto fail;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (ep->valid) {
		IPAERR("EP already allocated.\n");
		goto fail;
	}

	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));
	IPA_ACTIVE_CLIENTS_INC_EP(in->sys.client);

	IPADBG("client=%d ep=%d\n", in->sys.client, ipa_ep_idx);
	if (IPA_CLIENT_IS_CONS(in->sys.client)) {
		if (ipa3_ctx->ipa_wdi2)
			cmd.size = sizeof(*tx_2);
		else
			cmd.size = sizeof(*tx);
		if (in->smmu_enabled) {
			IPADBG("comp_ring_size=%d\n",
				in->u.dl_smmu.comp_ring_size);
			IPADBG("ce_ring_size=%d\n", in->u.dl_smmu.ce_ring_size);
			IPADBG("ce_ring_doorbell_pa=0x%pa\n",
					&in->u.dl_smmu.ce_door_bell_pa);
			IPADBG("num_tx_buffers=%d\n",
				in->u.dl_smmu.num_tx_buffers);
		} else {
			IPADBG("comp_ring_base_pa=0x%pa\n",
					&in->u.dl.comp_ring_base_pa);
			IPADBG("comp_ring_size=%d\n", in->u.dl.comp_ring_size);
			IPADBG("ce_ring_base_pa=0x%pa\n",
				&in->u.dl.ce_ring_base_pa);
			IPADBG("ce_ring_size=%d\n", in->u.dl.ce_ring_size);
			IPADBG("ce_ring_doorbell_pa=0x%pa\n",
					&in->u.dl.ce_door_bell_pa);
			IPADBG("num_tx_buffers=%d\n", in->u.dl.num_tx_buffers);
		}
	} else {
		if (ipa3_ctx->ipa_wdi2)
			cmd.size = sizeof(*rx_2);
		else
			cmd.size = sizeof(*rx);
		if (in->smmu_enabled) {
			IPADBG("rx_ring_size=%d\n",
				in->u.ul_smmu.rdy_ring_size);
			IPADBG("rx_ring_rp_pa=0x%pa\n",
				&in->u.ul_smmu.rdy_ring_rp_pa);
			IPADBG("rx_comp_ring_size=%d\n",
				in->u.ul_smmu.rdy_comp_ring_size);
			IPADBG("rx_comp_ring_wp_pa=0x%pa\n",
				&in->u.ul_smmu.rdy_comp_ring_wp_pa);
			ipa3_ctx->uc_ctx.rdy_ring_rp_pa =
				in->u.ul_smmu.rdy_ring_rp_pa;
			ipa3_ctx->uc_ctx.rdy_ring_size =
				in->u.ul_smmu.rdy_ring_size;
			ipa3_ctx->uc_ctx.rdy_comp_ring_wp_pa =
				in->u.ul_smmu.rdy_comp_ring_wp_pa;
			ipa3_ctx->uc_ctx.rdy_comp_ring_size =
				in->u.ul_smmu.rdy_comp_ring_size;
		} else {
			IPADBG("rx_ring_base_pa=0x%pa\n",
				&in->u.ul.rdy_ring_base_pa);
			IPADBG("rx_ring_size=%d\n",
				in->u.ul.rdy_ring_size);
			IPADBG("rx_ring_rp_pa=0x%pa\n",
				&in->u.ul.rdy_ring_rp_pa);
			IPADBG("rx_comp_ring_base_pa=0x%pa\n",
				&in->u.ul.rdy_comp_ring_base_pa);
			IPADBG("rx_comp_ring_size=%d\n",
				in->u.ul.rdy_comp_ring_size);
			IPADBG("rx_comp_ring_wp_pa=0x%pa\n",
				&in->u.ul.rdy_comp_ring_wp_pa);
			ipa3_ctx->uc_ctx.rdy_ring_base_pa =
				in->u.ul.rdy_ring_base_pa;
			ipa3_ctx->uc_ctx.rdy_ring_rp_pa =
				in->u.ul.rdy_ring_rp_pa;
			ipa3_ctx->uc_ctx.rdy_ring_size =
				in->u.ul.rdy_ring_size;
			ipa3_ctx->uc_ctx.rdy_comp_ring_base_pa =
				in->u.ul.rdy_comp_ring_base_pa;
			ipa3_ctx->uc_ctx.rdy_comp_ring_wp_pa =
				in->u.ul.rdy_comp_ring_wp_pa;
			ipa3_ctx->uc_ctx.rdy_comp_ring_size =
				in->u.ul.rdy_comp_ring_size;
		}
	}

	cmd.base = dma_alloc_coherent(ipa3_ctx->uc_pdev, cmd.size,
			&cmd.phys_base, GFP_KERNEL);
	if (cmd.base == NULL) {
		IPAERR("fail to get DMA memory.\n");
		result = -ENOMEM;
		goto dma_alloc_fail;
	}

	if (IPA_CLIENT_IS_CONS(in->sys.client)) {
		if (ipa3_ctx->ipa_wdi2) {
			tx_2 = (struct IpaHwWdi2TxSetUpCmdData_t *)cmd.base;

			len = in->smmu_enabled ? in->u.dl_smmu.comp_ring_size :
				in->u.dl.comp_ring_size;
			IPADBG("TX_2 ring smmu_en=%d ring_size=%d %d\n",
				in->smmu_enabled,
				in->u.dl_smmu.comp_ring_size,
				in->u.dl.comp_ring_size);
			if (ipa_create_uc_smmu_mapping(IPA_WDI_TX_RING_RES,
					in->smmu_enabled,
					in->u.dl.comp_ring_base_pa,
					&in->u.dl_smmu.comp_ring,
					len,
					false,
					&va)) {
				IPAERR("fail to create uc mapping TX ring.\n");
				result = -ENOMEM;
				goto uc_timeout;
			}
			tx_2->comp_ring_base_pa_hi =
				(u32) ((va & 0xFFFFFFFF00000000) >> 32);
			tx_2->comp_ring_base_pa = (u32) (va & 0xFFFFFFFF);
			tx_2->comp_ring_size = len;
			IPADBG("TX_2 comp_ring_base_pa_hi=0x%08x :0x%08x\n",
					tx_2->comp_ring_base_pa_hi,
					tx_2->comp_ring_base_pa);

			len = in->smmu_enabled ? in->u.dl_smmu.ce_ring_size :
				in->u.dl.ce_ring_size;
			IPADBG("TX_2 CE ring smmu_en=%d ring_size=%d %d\n",
					in->smmu_enabled,
					in->u.dl_smmu.ce_ring_size,
					in->u.dl.ce_ring_size);
			/* WA: wlan passed ce_ring sg_table PA directly */
			if (ipa_create_uc_smmu_mapping(IPA_WDI_CE_RING_RES,
						in->smmu_enabled,
						in->u.dl.ce_ring_base_pa,
						&in->u.dl_smmu.ce_ring,
						len,
						false,
						&va)) {
				IPAERR("fail to create uc mapping CE ring.\n");
				result = -ENOMEM;
				goto uc_timeout;
			}
			tx_2->ce_ring_base_pa_hi =
				(u32) ((va & 0xFFFFFFFF00000000) >> 32);
			tx_2->ce_ring_base_pa = (u32) (va & 0xFFFFFFFF);
			tx_2->ce_ring_size = len;
			IPADBG("TX_2 ce_ring_base_pa_hi=0x%08x :0x%08x\n",
					tx_2->ce_ring_base_pa_hi,
					tx_2->ce_ring_base_pa);

			pa = in->smmu_enabled ? in->u.dl_smmu.ce_door_bell_pa :
				in->u.dl.ce_door_bell_pa;
			if (ipa_create_uc_smmu_mapping(IPA_WDI_CE_DB_RES,
						in->smmu_enabled,
						pa,
						NULL,
						4,
						true,
						&va)) {
				IPAERR("fail to create uc mapping CE DB.\n");
				result = -ENOMEM;
				goto uc_timeout;
			}
			tx_2->ce_ring_doorbell_pa_hi =
				(u32) ((va & 0xFFFFFFFF00000000) >> 32);
			tx_2->ce_ring_doorbell_pa = (u32) (va & 0xFFFFFFFF);
			IPADBG("TX_2 ce_ring_doorbell_pa_hi=0x%08x :0x%08x\n",
					tx_2->ce_ring_doorbell_pa_hi,
					tx_2->ce_ring_doorbell_pa);

			tx_2->num_tx_buffers = in->smmu_enabled ?
				in->u.dl_smmu.num_tx_buffers :
				in->u.dl.num_tx_buffers;
			tx_2->ipa_pipe_number = ipa_ep_idx;
		} else {
			tx = (struct IpaHwWdiTxSetUpCmdData_t *)cmd.base;

			len = in->smmu_enabled ? in->u.dl_smmu.comp_ring_size :
				in->u.dl.comp_ring_size;
			IPADBG("TX ring smmu_en=%d ring_size=%d %d\n",
					in->smmu_enabled,
					in->u.dl_smmu.comp_ring_size,
					in->u.dl.comp_ring_size);
			if (ipa_create_uc_smmu_mapping(IPA_WDI_TX_RING_RES,
						in->smmu_enabled,
						in->u.dl.comp_ring_base_pa,
						&in->u.dl_smmu.comp_ring,
						len,
						false,
						&va)) {
				IPAERR("fail to create uc mapping TX ring.\n");
				result = -ENOMEM;
				goto uc_timeout;
			}
			tx->comp_ring_base_pa = va;
			tx->comp_ring_size = len;
			len = in->smmu_enabled ? in->u.dl_smmu.ce_ring_size :
				in->u.dl.ce_ring_size;
			IPADBG("TX CE ring smmu_en=%d ring_size=%d %d 0x%lx\n",
					in->smmu_enabled,
					in->u.dl_smmu.ce_ring_size,
					in->u.dl.ce_ring_size,
					va);
			if (ipa_create_uc_smmu_mapping(IPA_WDI_CE_RING_RES,
						in->smmu_enabled,
						in->u.dl.ce_ring_base_pa,
						&in->u.dl_smmu.ce_ring,
						len,
						false,
						&va)) {
				IPAERR("fail to create uc mapping CE ring.\n");
				result = -ENOMEM;
				goto uc_timeout;
			}
			tx->ce_ring_base_pa = va;
			tx->ce_ring_size = len;
			pa = in->smmu_enabled ? in->u.dl_smmu.ce_door_bell_pa :
				in->u.dl.ce_door_bell_pa;
			if (ipa_create_uc_smmu_mapping(IPA_WDI_CE_DB_RES,
						in->smmu_enabled,
						pa,
						NULL,
						4,
						true,
						&va)) {
				IPAERR("fail to create uc mapping CE DB.\n");
				result = -ENOMEM;
				goto uc_timeout;
			}

			IPADBG("CE doorbell pa: 0x%pa va:0x%lx\n", &pa, va);
			IPADBG("Is wdi_over_pcie ? (%s)\n",
				ipa3_ctx->wdi_over_pcie ? "Yes":"No");

			if (ipa3_ctx->wdi_over_pcie)
				tx->ce_ring_doorbell_pa = pa;
			else
				tx->ce_ring_doorbell_pa = va;

			tx->num_tx_buffers = in->smmu_enabled ?
				in->u.dl_smmu.num_tx_buffers :
				in->u.dl.num_tx_buffers;
			tx->ipa_pipe_number = ipa_ep_idx;
		}
		out->uc_door_bell_pa = ipa3_ctx->ipa_wrapper_base +
				ipahal_get_reg_base() +
				ipahal_get_reg_mn_ofst(IPA_UC_MAILBOX_m_n,
				IPA_HW_WDI_TX_MBOX_START_INDEX/32,
				IPA_HW_WDI_TX_MBOX_START_INDEX % 32);
	} else {
		if (ipa3_ctx->ipa_wdi2) {
			rx_2 = (struct IpaHwWdi2RxSetUpCmdData_t *)cmd.base;

			len = in->smmu_enabled ? in->u.ul_smmu.rdy_ring_size :
				in->u.ul.rdy_ring_size;
			IPADBG("RX_2 ring smmu_en=%d ring_size=%d %d\n",
				in->smmu_enabled,
				in->u.ul_smmu.rdy_ring_size,
				in->u.ul.rdy_ring_size);
			if (ipa_create_uc_smmu_mapping(IPA_WDI_RX_RING_RES,
						in->smmu_enabled,
						in->u.ul.rdy_ring_base_pa,
						&in->u.ul_smmu.rdy_ring,
						len,
						false,
						&va)) {
				IPAERR("fail to create uc RX_2 ring.\n");
				result = -ENOMEM;
				goto uc_timeout;
			}
			rx_2->rx_ring_base_pa_hi =
				(u32) ((va & 0xFFFFFFFF00000000) >> 32);
			rx_2->rx_ring_base_pa = (u32) (va & 0xFFFFFFFF);
			rx_2->rx_ring_size = len;
			IPADBG("RX_2 rx_ring_base_pa_hi=0x%08x:0x%08x\n",
					rx_2->rx_ring_base_pa_hi,
					rx_2->rx_ring_base_pa);

			pa = in->smmu_enabled ? in->u.ul_smmu.rdy_ring_rp_pa :
				in->u.ul.rdy_ring_rp_pa;
			if (ipa_create_uc_smmu_mapping(IPA_WDI_RX_RING_RP_RES,
						in->smmu_enabled,
						pa,
						NULL,
						4,
						false,
						&va)) {
				IPAERR("fail to create uc RX_2 rng RP\n");
				result = -ENOMEM;
				goto uc_timeout;
			}
			rx_2->rx_ring_rp_pa_hi =
				(u32) ((va & 0xFFFFFFFF00000000) >> 32);
			rx_2->rx_ring_rp_pa = (u32) (va & 0xFFFFFFFF);
			IPADBG("RX_2 rx_ring_rp_pa_hi=0x%08x :0x%08x\n",
					rx_2->rx_ring_rp_pa_hi,
					rx_2->rx_ring_rp_pa);
			len = in->smmu_enabled ?
				in->u.ul_smmu.rdy_comp_ring_size :
				in->u.ul.rdy_comp_ring_size;
			IPADBG("RX_2 ring smmu_en=%d comp_ring_size=%d %d\n",
					in->smmu_enabled,
					in->u.ul_smmu.rdy_comp_ring_size,
					in->u.ul.rdy_comp_ring_size);
			if (ipa_create_uc_smmu_mapping(IPA_WDI_RX_COMP_RING_RES,
						in->smmu_enabled,
						in->u.ul.rdy_comp_ring_base_pa,
						&in->u.ul_smmu.rdy_comp_ring,
						len,
						false,
						&va)) {
				IPAERR("fail to create uc RX_2 comp_ring.\n");
				result = -ENOMEM;
				goto uc_timeout;
			}
			rx_2->rx_comp_ring_base_pa_hi =
				(u32) ((va & 0xFFFFFFFF00000000) >> 32);
			rx_2->rx_comp_ring_base_pa = (u32) (va & 0xFFFFFFFF);
			rx_2->rx_comp_ring_size = len;
			IPADBG("RX_2 rx_comp_ring_base_pa_hi=0x%08x:0x%08x\n",
					rx_2->rx_comp_ring_base_pa_hi,
					rx_2->rx_comp_ring_base_pa);

			pa = in->smmu_enabled ?
				in->u.ul_smmu.rdy_comp_ring_wp_pa :
				in->u.ul.rdy_comp_ring_wp_pa;
			if (ipa_create_uc_smmu_mapping(
						IPA_WDI_RX_COMP_RING_WP_RES,
						in->smmu_enabled,
						pa,
						NULL,
						4,
						false,
						&va)) {
				IPAERR("fail to create uc RX_2 comp_rng WP\n");
				result = -ENOMEM;
				goto uc_timeout;
			}
			rx_2->rx_comp_ring_wp_pa_hi =
				(u32) ((va & 0xFFFFFFFF00000000) >> 32);
			rx_2->rx_comp_ring_wp_pa = (u32) (va & 0xFFFFFFFF);
			IPADBG("RX_2 rx_comp_ring_wp_pa_hi=0x%08x:0x%08x\n",
					rx_2->rx_comp_ring_wp_pa_hi,
					rx_2->rx_comp_ring_wp_pa);
			rx_2->ipa_pipe_number = ipa_ep_idx;
		} else {
			rx = (struct IpaHwWdiRxSetUpCmdData_t *)cmd.base;

			len = in->smmu_enabled ? in->u.ul_smmu.rdy_ring_size :
				in->u.ul.rdy_ring_size;
			IPADBG("RX ring smmu_en=%d ring_size=%d %d\n",
					in->smmu_enabled,
					in->u.ul_smmu.rdy_ring_size,
					in->u.ul.rdy_ring_size);
			if (ipa_create_uc_smmu_mapping(IPA_WDI_RX_RING_RES,
						in->smmu_enabled,
						in->u.ul.rdy_ring_base_pa,
						&in->u.ul_smmu.rdy_ring,
						len,
						false,
						&va)) {
				IPAERR("fail to create uc mapping RX ring.\n");
				result = -ENOMEM;
				goto uc_timeout;
			}
			rx->rx_ring_base_pa = va;
			rx->rx_ring_size = len;

			pa = in->smmu_enabled ? in->u.ul_smmu.rdy_ring_rp_pa :
				in->u.ul.rdy_ring_rp_pa;
			if (ipa_create_uc_smmu_mapping(IPA_WDI_RX_RING_RP_RES,
						in->smmu_enabled,
						pa,
						NULL,
						4,
						false,
						&va)) {
				IPAERR("fail to create uc mapping RX rng RP\n");
				result = -ENOMEM;
				goto uc_timeout;
			}
			rx->rx_ring_rp_pa = va;
			rx->ipa_pipe_number = ipa_ep_idx;
		}
		out->uc_door_bell_pa = ipa3_ctx->ipa_wrapper_base +
				ipahal_get_reg_base() +
				ipahal_get_reg_mn_ofst(IPA_UC_MAILBOX_m_n,
					IPA_HW_WDI_RX_MBOX_START_INDEX/32,
					IPA_HW_WDI_RX_MBOX_START_INDEX % 32);
	}

	ep->valid = 1;
	ep->client = in->sys.client;
	ep->keep_ipa_awake = in->sys.keep_ipa_awake;
	result = ipa3_disable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("disable data path failed res=%d clnt=%d.\n", result,
			ipa_ep_idx);
		goto uc_timeout;
	}
	if (IPA_CLIENT_IS_PROD(in->sys.client)) {
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_delay = true;
		ipa3_cfg_ep_ctrl(ipa_ep_idx, &ep_cfg_ctrl);
	}

	result = ipa3_uc_send_cmd((u32)(cmd.phys_base),
				IPA_CLIENT_IS_CONS(in->sys.client) ?
				IPA_CPU_2_HW_CMD_WDI_TX_SET_UP :
				IPA_CPU_2_HW_CMD_WDI_RX_SET_UP,
				IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS,
				false, 10*HZ);

	if (result) {
		result = -EFAULT;
		goto uc_timeout;
	}

	ep->skip_ep_cfg = in->sys.skip_ep_cfg;
	ep->client_notify = in->sys.notify;
	ep->priv = in->sys.priv;

	/* for AP+STA stats update */
	if (in->wdi_notify)
		ipa3_ctx->uc_wdi_ctx.stats_notify = in->wdi_notify;
	else
		IPADBG("in->wdi_notify is null\n");

	if (IPA_CLIENT_IS_CONS(in->sys.client)) {
		in->sys.ipa_ep_cfg.aggr.aggr_en = IPA_ENABLE_AGGR;
		in->sys.ipa_ep_cfg.aggr.aggr = IPA_GENERIC;
		in->sys.ipa_ep_cfg.aggr.aggr_pkt_limit = IPA_AGGR_PKT_LIMIT;
		in->sys.ipa_ep_cfg.aggr.aggr_byte_limit =
						IPA_AGGR_HARD_BYTE_LIMIT;
		in->sys.ipa_ep_cfg.aggr.aggr_hard_byte_limit_en =
						IPA_ENABLE_AGGR;
	}
	if (!ep->skip_ep_cfg) {
		if (ipa3_cfg_ep(ipa_ep_idx, &in->sys.ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto ipa_cfg_ep_fail;
		}
		IPADBG("ep configuration successful\n");
	} else {
		IPADBG("Skipping endpoint configuration.\n");
	}

	ipa3_enable_data_path(ipa_ep_idx);

	out->clnt_hdl = ipa_ep_idx;

	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(in->sys.client))
		ipa3_install_dflt_flt_rules(ipa_ep_idx);

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(in->sys.client);

	dma_free_coherent(ipa3_ctx->uc_pdev, cmd.size, cmd.base, cmd.phys_base);
	ep->uc_offload_state |= IPA_WDI_CONNECTED;
	IPADBG("client %d (ep: %d) connected\n", in->sys.client, ipa_ep_idx);

	return 0;

ipa_cfg_ep_fail:
	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));
uc_timeout:
	ipa_release_uc_smmu_mappings(in->sys.client);
	dma_free_coherent(ipa3_ctx->uc_pdev, cmd.size, cmd.base, cmd.phys_base);
dma_alloc_fail:
	IPA_ACTIVE_CLIENTS_DEC_EP(in->sys.client);
fail:
	return result;
}

int ipa3_disconnect_gsi_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa3_ep_context *ep;

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (ep->gsi_offload_state != IPA_WDI_CONNECTED) {
		IPAERR("WDI channel bad state %d\n", ep->gsi_offload_state);
		return -EFAULT;
	}

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_reset_gsi_channel(clnt_hdl);
	ipa3_reset_gsi_event_ring(clnt_hdl);

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	result = ipa3_release_gsi_channel(clnt_hdl);
	if (result) {
		IPAERR("GSI dealloc channel failed %d\n",
				result);
		goto fail_dealloc_channel;
	}
	ipa_release_ap_smmu_mappings(clnt_hdl);

	/* for AP+STA stats update */
	if (ipa3_ctx->uc_wdi_ctx.stats_notify)
		ipa3_ctx->uc_wdi_ctx.stats_notify = NULL;
	else
		IPADBG("uc_wdi_ctx.stats_notify already null\n");

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

fail_dealloc_channel:
	return result;
}

/**
 * ipa3_disconnect_wdi_pipe() - WDI client disconnect
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_disconnect_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa3_ep_context *ep;
	union IpaHwWdiCommonChCmdData_t tear;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm, %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (ipa3_ctx->ipa_wdi2_over_gsi)
		return ipa3_disconnect_gsi_wdi_pipe(clnt_hdl);

	result = ipa3_uc_state_check();
	if (result)
		return result;

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (ep->uc_offload_state != IPA_WDI_CONNECTED) {
		IPAERR("WDI channel bad state %d\n", ep->uc_offload_state);
		return -EFAULT;
	}

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	tear.params.ipa_pipe_number = clnt_hdl;

	result = ipa3_uc_send_cmd(tear.raw32b,
				IPA_CPU_2_HW_CMD_WDI_TEAR_DOWN,
				IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS,
				false, 10*HZ);

	if (result) {
		result = -EFAULT;
		goto uc_timeout;
	}

	ipa3_delete_dflt_flt_rules(clnt_hdl);
	ipa_release_uc_smmu_mappings(ep->client);

	memset(&ipa3_ctx->ep[clnt_hdl], 0, sizeof(struct ipa3_ep_context));

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

	/* for AP+STA stats update */
	if (ipa3_ctx->uc_wdi_ctx.stats_notify)
		ipa3_ctx->uc_wdi_ctx.stats_notify = NULL;
	else
		IPADBG("uc_wdi_ctx.stats_notify already null\n");

uc_timeout:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

int ipa3_enable_gsi_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa3_ep_context *ep;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	int ipa_ep_idx;

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa3_ctx->ep[clnt_hdl];
	if (ep->gsi_offload_state != IPA_WDI_CONNECTED) {
		IPAERR("WDI channel bad state %d\n", ep->gsi_offload_state);
		return -EFAULT;
	}

	ipa_ep_idx = ipa3_get_ep_mapping(ipa3_get_client_mapping(clnt_hdl));
	if (ipa_ep_idx == -1) {
		IPAERR("fail to alloc EP.\n");
		return -EPERM;
	}

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
	ipa3_cfg_ep_ctrl(ipa_ep_idx, &ep_cfg_ctrl);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	ep->gsi_offload_state |= IPA_WDI_ENABLED;
	IPADBG("client (ep: %d) enabled\n", clnt_hdl);

	return result;
}
int ipa3_disable_gsi_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa3_ep_context *ep;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	u32 prod_hdl;

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (ep->gsi_offload_state != (IPA_WDI_CONNECTED | IPA_WDI_ENABLED)) {
		IPAERR("WDI channel bad state %d\n", ep->gsi_offload_state);
		return -EFAULT;
	}
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	result = ipa3_disable_data_path(clnt_hdl);
	if (result) {
		IPAERR("disable data path failed res=%d clnt=%d.\n", result,
				clnt_hdl);
		goto gsi_timeout;
	}

	/**
	 * To avoid data stall during continuous SAP on/off before
	 * setting delay to IPA Consumer pipe, remove delay and enable
	 * holb on IPA Producer pipe
	 */
	if (IPA_CLIENT_IS_PROD(ep->client)) {
		IPADBG("Stopping PROD channel - hdl=%d clnt=%d\n",
				clnt_hdl, ep->client);
		/* remove delay on wlan-prod pipe*/
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);

		prod_hdl = ipa3_get_ep_mapping(IPA_CLIENT_WLAN1_CONS);
		if (ipa3_ctx->ep[prod_hdl].valid == 1) {
			result = ipa3_disable_data_path(prod_hdl);
			if (result) {
				IPAERR("disable data path failed\n");
				IPAERR("res=%d clnt=%d\n",
						result, prod_hdl);
				goto gsi_timeout;
			}
		}
		usleep_range(IPA_UC_POLL_SLEEP_USEC * IPA_UC_POLL_SLEEP_USEC,
			IPA_UC_POLL_SLEEP_USEC * IPA_UC_POLL_SLEEP_USEC);

	}

	/* Set the delay after disabling IPA Producer pipe */
	if (IPA_CLIENT_IS_PROD(ep->client)) {
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_delay = true;
		ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
	}
	ep->gsi_offload_state &= ~IPA_WDI_ENABLED;
	IPADBG("client (ep: %d) disabled\n", clnt_hdl);

gsi_timeout:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}
/**
 * ipa3_enable_wdi_pipe() - WDI client enable
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_enable_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa3_ep_context *ep;
	union IpaHwWdiCommonChCmdData_t enable;
	struct ipa_ep_cfg_holb holb_cfg;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm, %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (ipa3_ctx->ipa_wdi2_over_gsi)
		return ipa3_enable_gsi_wdi_pipe(clnt_hdl);

	result = ipa3_uc_state_check();
	if (result)
		return result;

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (ep->uc_offload_state != IPA_WDI_CONNECTED) {
		IPAERR("WDI channel bad state %d\n", ep->uc_offload_state);
		return -EFAULT;
	}
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));
	enable.params.ipa_pipe_number = clnt_hdl;

	result = ipa3_uc_send_cmd(enable.raw32b,
		IPA_CPU_2_HW_CMD_WDI_CH_ENABLE,
		IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS,
		false, 10*HZ);

	if (result) {
		result = -EFAULT;
		goto uc_timeout;
	}

	if (IPA_CLIENT_IS_CONS(ep->client)) {
		memset(&holb_cfg, 0, sizeof(holb_cfg));
		holb_cfg.en = IPA_HOLB_TMR_DIS;
		holb_cfg.tmr_val = 0;
		result = ipa3_cfg_ep_holb(clnt_hdl, &holb_cfg);
	}

	ep->uc_offload_state |= IPA_WDI_ENABLED;
	IPADBG("client (ep: %d) enabled\n", clnt_hdl);

uc_timeout:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

/**
 * ipa3_disable_wdi_pipe() - WDI client disable
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_disable_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa3_ep_context *ep;
	union IpaHwWdiCommonChCmdData_t disable;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	u32 prod_hdl;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm, %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (ipa3_ctx->ipa_wdi2_over_gsi)
		return ipa3_disable_gsi_wdi_pipe(clnt_hdl);

	result = ipa3_uc_state_check();
	if (result)
		return result;

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (ep->uc_offload_state != (IPA_WDI_CONNECTED | IPA_WDI_ENABLED)) {
		IPAERR("WDI channel bad state %d\n", ep->uc_offload_state);
		return -EFAULT;
	}
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	result = ipa3_disable_data_path(clnt_hdl);
	if (result) {
		IPAERR("disable data path failed res=%d clnt=%d.\n", result,
			clnt_hdl);
		result = -EPERM;
		goto uc_timeout;
	}

	/**
	 * To avoid data stall during continuous SAP on/off before
	 * setting delay to IPA Consumer pipe, remove delay and enable
	 * holb on IPA Producer pipe
	 */
	if (IPA_CLIENT_IS_PROD(ep->client)) {
		IPADBG("Stopping PROD channel - hdl=%d clnt=%d\n",
			clnt_hdl, ep->client);
		/* remove delay on wlan-prod pipe*/
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);

		prod_hdl = ipa3_get_ep_mapping(IPA_CLIENT_WLAN1_CONS);
		if (ipa3_ctx->ep[prod_hdl].valid == 1) {
			result = ipa3_disable_data_path(prod_hdl);
			if (result) {
				IPAERR("disable data path failed\n");
				IPAERR("res=%d clnt=%d\n",
					result, prod_hdl);
				result = -EPERM;
				goto uc_timeout;
			}
		}
		usleep_range(IPA_UC_POLL_SLEEP_USEC * IPA_UC_POLL_SLEEP_USEC,
			IPA_UC_POLL_SLEEP_USEC * IPA_UC_POLL_SLEEP_USEC);

	}

	disable.params.ipa_pipe_number = clnt_hdl;
	result = ipa3_uc_send_cmd(disable.raw32b,
		IPA_CPU_2_HW_CMD_WDI_CH_DISABLE,
		IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS,
		false, 10*HZ);

	if (result) {
		result = -EFAULT;
		goto uc_timeout;
	}

	/* Set the delay after disabling IPA Producer pipe */
	if (IPA_CLIENT_IS_PROD(ep->client)) {
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_delay = true;
		ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
	}
	ep->uc_offload_state &= ~IPA_WDI_ENABLED;
	IPADBG("client (ep: %d) disabled\n", clnt_hdl);


uc_timeout:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

int ipa3_resume_gsi_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa3_ep_context *ep;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	struct gsi_chan_info chan_info;
	union __packed gsi_channel_scratch gsi_scratch;

	IPADBG("ep=%d\n", clnt_hdl);
	ep = &ipa3_ctx->ep[clnt_hdl];

	if (ep->gsi_offload_state != (IPA_WDI_CONNECTED | IPA_WDI_ENABLED)) {
		IPAERR("WDI channel bad state %d\n", ep->gsi_offload_state);
		return -EFAULT;
	}
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
	result = ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
	if (result)
		IPAERR("client (ep: %d) fail un-susp/delay result=%d\n",
				clnt_hdl, result);
	else
		IPADBG("client (ep: %d) un-susp/delay\n", clnt_hdl);

	result =  gsi_start_channel(ep->gsi_chan_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("gsi_start_channel failed %d\n", result);
		ipa_assert();
	}
	gsi_query_channel_info(ep->gsi_chan_hdl, &chan_info);
	gsi_read_channel_scratch(ep->gsi_chan_hdl, &gsi_scratch);
	IPADBG("ch=%lu channel base = 0x%llx , event base 0x%llx\n",
				ep->gsi_chan_hdl,
				ep->gsi_mem_info.chan_ring_base_addr,
				ep->gsi_mem_info.evt_ring_base_addr);
	IPADBG("RP=0x%llx WP=0x%llx ev_valid=%d ERP=0x%llx EWP=0x%llx\n",
			chan_info.rp, chan_info.wp, chan_info.evt_valid,
			chan_info.evt_rp, chan_info.evt_wp);
	IPADBG("Scratch 0 = %x Scratch 1 = %x Scratch 2 = %x Scratch 3 = %x\n",
				gsi_scratch.data.word1, gsi_scratch.data.word2,
				gsi_scratch.data.word3, gsi_scratch.data.word4);

	ep->gsi_offload_state |= IPA_WDI_RESUMED;
	IPADBG("exit\n");
	return result;
}

/**
 * ipa3_resume_wdi_pipe() - WDI client resume
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_resume_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa3_ep_context *ep;
	union IpaHwWdiCommonChCmdData_t resume;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm, %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (ipa3_ctx->ipa_wdi2_over_gsi)
		return ipa3_resume_gsi_wdi_pipe(clnt_hdl);

	result = ipa3_uc_state_check();
	if (result)
		return result;

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (ep->uc_offload_state != (IPA_WDI_CONNECTED | IPA_WDI_ENABLED)) {
		IPAERR("WDI channel bad state %d\n", ep->uc_offload_state);
		return -EFAULT;
	}
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));
	resume.params.ipa_pipe_number = clnt_hdl;

	result = ipa3_uc_send_cmd(resume.raw32b,
		IPA_CPU_2_HW_CMD_WDI_CH_RESUME,
		IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS,
		false, 10*HZ);

	if (result) {
		result = -EFAULT;
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
		goto uc_timeout;
	}

	memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
	result = ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
	if (result)
		IPAERR("client (ep: %d) fail un-susp/delay result=%d\n",
				clnt_hdl, result);
	else
		IPADBG("client (ep: %d) un-susp/delay\n", clnt_hdl);

	ep->uc_offload_state |= IPA_WDI_RESUMED;
	IPADBG("client (ep: %d) resumed\n", clnt_hdl);

uc_timeout:
	return result;
}

int ipa3_suspend_gsi_wdi_pipe(u32 clnt_hdl)
{
	int ipa_ep_idx;
	struct ipa3_ep_context *ep;
	int res = 0;
	u32 source_pipe_bitmask = 0;
	bool disable_force_clear = false;
	struct ipahal_ep_cfg_ctrl_scnd ep_ctrl_scnd = { 0 };
	int retry_cnt = 0;
	struct gsi_chan_info chan_info;
	union __packed gsi_channel_scratch gsi_scratch;

	ipa_ep_idx = ipa3_get_ep_mapping(ipa3_get_client_mapping(clnt_hdl));
	if (ipa_ep_idx < 0) {
		IPAERR("IPA client mapping failed\n");
		return -EPERM;
	}
	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (ep->gsi_offload_state != (IPA_WDI_CONNECTED | IPA_WDI_ENABLED |
				IPA_WDI_RESUMED)) {
		IPAERR("WDI channel bad state %d\n", ep->gsi_offload_state);
		return -EFAULT;
	}
	if (ep->valid) {
		IPADBG("suspended pipe %d\n", ipa_ep_idx);
		source_pipe_bitmask = 1 <<
			ipa3_get_ep_mapping(ep->client);
		res = ipa3_enable_force_clear(clnt_hdl,
				false, source_pipe_bitmask);
		if (res) {
			/*
			 * assuming here modem SSR, AP can remove
			 * the delay in this case
			 */
			IPAERR("failed to force clear %d\n", res);
			IPAERR("remove delay from SCND reg\n");
			ep_ctrl_scnd.endp_delay = false;
			ipahal_write_reg_n_fields(
					IPA_ENDP_INIT_CTRL_SCND_n, clnt_hdl,
					&ep_ctrl_scnd);
		} else {
			disable_force_clear = true;
		}
retry_gsi_stop:
		res = ipa3_stop_gsi_channel(ipa_ep_idx);
		if (res != 0 && res != -GSI_STATUS_AGAIN &&
				res != -GSI_STATUS_TIMED_OUT) {
			IPAERR("failed to stop channel res = %d\n", res);
			goto fail_stop_channel;
		} else if (res == -GSI_STATUS_AGAIN) {
			IPADBG("GSI stop channel failed retry cnt = %d\n",
						retry_cnt);
			retry_cnt++;
			if (retry_cnt >= GSI_STOP_MAX_RETRY_CNT)
				goto fail_stop_channel;
			goto retry_gsi_stop;
		} else {
			IPADBG("GSI channel %ld STOP\n", ep->gsi_chan_hdl);
		}
		gsi_query_channel_info(ep->gsi_chan_hdl, &chan_info);
		gsi_read_channel_scratch(ep->gsi_chan_hdl, &gsi_scratch);
		IPADBG("ch=%lu channel base = 0x%llx , event base 0x%llx\n",
				ep->gsi_chan_hdl,
				ep->gsi_mem_info.chan_ring_base_addr,
				ep->gsi_mem_info.evt_ring_base_addr);
		IPADBG("RP=0x%llx WP=0x%llx ev_valid=%d ERP=0x%llx",
				chan_info.rp, chan_info.wp,
				chan_info.evt_valid, chan_info.evt_rp);
		IPADBG("EWP=0x%llx\n", chan_info.evt_wp);
		IPADBG("Scratch 0 = %x Scratch 1 = %x Scratch 2 = %x",
				gsi_scratch.data.word1, gsi_scratch.data.word2,
				gsi_scratch.data.word3);
		IPADBG("Scratch 3 = %x\n", gsi_scratch.data.word4);
	}

	if (disable_force_clear)
		ipa3_disable_force_clear(clnt_hdl);
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	ep->gsi_offload_state &= ~IPA_WDI_RESUMED;
	return res;
fail_stop_channel:
	ipa_assert();
	return res;
}

/**
 * ipa3_suspend_wdi_pipe() - WDI client suspend
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_suspend_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa3_ep_context *ep;
	union IpaHwWdiCommonChCmdData_t suspend;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	u32 source_pipe_bitmask = 0;
	bool disable_force_clear = false;
	struct ipahal_ep_cfg_ctrl_scnd ep_ctrl_scnd = { 0 };

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm, %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (ipa3_ctx->ipa_wdi2_over_gsi)
		return ipa3_suspend_gsi_wdi_pipe(clnt_hdl);

	result = ipa3_uc_state_check();
	if (result)
		return result;

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (ep->uc_offload_state != (IPA_WDI_CONNECTED | IPA_WDI_ENABLED |
				IPA_WDI_RESUMED)) {
		IPAERR("WDI channel bad state %d\n", ep->uc_offload_state);
		return -EFAULT;
	}

	suspend.params.ipa_pipe_number = clnt_hdl;

	if (IPA_CLIENT_IS_PROD(ep->client)) {
		/*
		 * For WDI 2.0 need to ensure pipe will be empty before suspend
		 * as IPA uC will fail to suspend the pipe otherwise.
		 */
		if (ipa3_ctx->ipa_wdi2) {
			source_pipe_bitmask = 1 <<
					ipa3_get_ep_mapping(ep->client);
			result = ipa3_enable_force_clear(clnt_hdl,
				false, source_pipe_bitmask);
			if (result) {
				/*
				 * assuming here modem SSR, AP can remove
				 * the delay in this case
				 */
				IPAERR("failed to force clear %d\n", result);
				IPAERR("remove delay from SCND reg\n");
				ep_ctrl_scnd.endp_delay = false;
				ipahal_write_reg_n_fields(
					IPA_ENDP_INIT_CTRL_SCND_n, clnt_hdl,
					&ep_ctrl_scnd);
			} else {
				disable_force_clear = true;
			}
		}

		IPADBG("Post suspend event first for IPA Producer\n");
		IPADBG("Client: %d clnt_hdl: %d\n", ep->client, clnt_hdl);
		result = ipa3_uc_send_cmd(suspend.raw32b,
			IPA_CPU_2_HW_CMD_WDI_CH_SUSPEND,
			IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS,
			false, 10*HZ);

		if (result) {
			result = -EFAULT;
			goto uc_timeout;
		}
	}

	memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
	if (IPA_CLIENT_IS_CONS(ep->client)) {
		if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
			ep_cfg_ctrl.ipa_ep_suspend = true;
			result = ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
			if (result)
				IPAERR("(ep: %d) failed to suspend result=%d\n",
						clnt_hdl, result);
			else
				IPADBG("(ep: %d) suspended\n", clnt_hdl);
		}
	} else {
		ep_cfg_ctrl.ipa_ep_delay = true;
		result = ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
		if (result)
			IPAERR("client (ep: %d) failed to delay result=%d\n",
					clnt_hdl, result);
		else
			IPADBG("client (ep: %d) delayed\n", clnt_hdl);
	}

	if (IPA_CLIENT_IS_CONS(ep->client)) {
		result = ipa3_uc_send_cmd(suspend.raw32b,
			IPA_CPU_2_HW_CMD_WDI_CH_SUSPEND,
			IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS,
			false, 10*HZ);

		if (result) {
			result = -EFAULT;
			goto uc_timeout;
		}
	}

	if (disable_force_clear)
		ipa3_disable_force_clear(clnt_hdl);

	ipa3_ctx->tag_process_before_gating = true;
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	ep->uc_offload_state &= ~IPA_WDI_RESUMED;
	IPADBG("client (ep: %d) suspended\n", clnt_hdl);

uc_timeout:
	return result;
}

/**
 * ipa_broadcast_wdi_quota_reach_ind() - quota reach
 * @uint32_t fid: [in] input netdev ID
 * @uint64_t num_bytes: [in] used bytes
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_broadcast_wdi_quota_reach_ind(uint32_t fid,
	uint64_t num_bytes)
{
	IPAERR("Quota reached indication on fid(%d) Mbytes(%lu)\n",
			  fid,
			  (unsigned long int) num_bytes);
	ipa3_broadcast_quota_reach_ind(0, IPA_UPSTEAM_WLAN);
	return 0;
}

int ipa3_write_qmapid_gsi_wdi_pipe(u32 clnt_hdl, u8 qmap_id)
{
	int result = 0;
	struct ipa3_ep_context *ep;
	union __packed gsi_wdi_channel_scratch3_reg gsi_scratch;

	memset(&gsi_scratch, 0, sizeof(gsi_scratch));
	ep = &ipa3_ctx->ep[clnt_hdl];
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	gsi_scratch.wdi.qmap_id = qmap_id;
	gsi_scratch.wdi.endp_metadatareg_offset = ipahal_get_reg_mn_ofst(
				IPA_ENDP_INIT_HDR_METADATA_n, 0, clnt_hdl)/4;

	result = gsi_write_channel_scratch3_reg(ep->gsi_chan_hdl, gsi_scratch);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("gsi_write_channel_scratch failed %d\n",
			result);
		goto fail_write_channel_scratch;
	}

	IPADBG("client (ep: %d) qmap_id %d updated\n", clnt_hdl, qmap_id);
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return 0;
fail_write_channel_scratch:
	ipa_assert();
	return result;
}
int ipa3_write_qmapid_wdi_pipe(u32 clnt_hdl, u8 qmap_id)
{
	int result = 0;
	struct ipa3_ep_context *ep;
	union IpaHwWdiRxExtCfgCmdData_t qmap;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR_RL("bad parm, %d\n", clnt_hdl);
		return -EINVAL;
	}
	if (ipa3_ctx->ipa_wdi2_over_gsi)
		return ipa3_write_qmapid_gsi_wdi_pipe(clnt_hdl, qmap_id);

	result = ipa3_uc_state_check();
	if (result)
		return result;

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!(ep->uc_offload_state & IPA_WDI_CONNECTED)) {
		IPAERR_RL("WDI channel bad state %d\n", ep->uc_offload_state);
		return -EFAULT;
	}
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));
	qmap.params.ipa_pipe_number = clnt_hdl;
	qmap.params.qmap_id = qmap_id;

	result = ipa3_uc_send_cmd(qmap.raw32b,
		IPA_CPU_2_HW_CMD_WDI_RX_EXT_CFG,
		IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS,
		false, 10*HZ);

	if (result) {
		result = -EFAULT;
		goto uc_timeout;
	}

	IPADBG("client (ep: %d) qmap_id %d updated\n", clnt_hdl, qmap_id);

uc_timeout:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

/**
 * ipa3_uc_reg_rdyCB() - To register uC
 * ready CB if uC not ready
 * @inout:	[in/out] input/output parameters
 * from/to client
 *
 * Returns:	0 on success, negative on failure
 *
 */
int ipa3_uc_reg_rdyCB(
	struct ipa_wdi_uc_ready_params *inout)
{
	int result = 0;

	if (inout == NULL) {
		IPAERR("bad parm. inout=%pK ", inout);
		return -EINVAL;
	}

	result = ipa3_uc_state_check();
	if (result) {
		inout->is_uC_ready = false;
		ipa3_ctx->uc_wdi_ctx.uc_ready_cb = inout->notify;
		ipa3_ctx->uc_wdi_ctx.priv = inout->priv;
	} else {
		inout->is_uC_ready = true;
	}

	return 0;
}

/**
 * ipa3_uc_dereg_rdyCB() - To de-register uC ready CB
 *
 * Returns:	0 on success, negative on failure
 *
 */
int ipa3_uc_dereg_rdyCB(void)
{
	ipa3_ctx->uc_wdi_ctx.uc_ready_cb = NULL;
	ipa3_ctx->uc_wdi_ctx.priv = NULL;

	return 0;
}


/**
 * ipa3_uc_wdi_get_dbpa() - To retrieve
 * doorbell physical address of wlan pipes
 * @param:  [in/out] input/output parameters
 *          from/to client
 *
 * Returns:	0 on success, negative on failure
 *
 */
int ipa3_uc_wdi_get_dbpa(
	struct ipa_wdi_db_params *param)
{
	if (param == NULL || param->client >= IPA_CLIENT_MAX) {
		IPAERR("bad parm. param=%pK ", param);
		if (param)
			IPAERR("client = %d\n", param->client);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(param->client)) {
		param->uc_door_bell_pa = ipa3_ctx->ipa_wrapper_base +
				ipahal_get_reg_base() +
				ipahal_get_reg_mn_ofst(IPA_UC_MAILBOX_m_n,
					IPA_HW_WDI_TX_MBOX_START_INDEX/32,
					IPA_HW_WDI_TX_MBOX_START_INDEX % 32);
	} else {
		param->uc_door_bell_pa = ipa3_ctx->ipa_wrapper_base +
				ipahal_get_reg_base() +
				ipahal_get_reg_mn_ofst(IPA_UC_MAILBOX_m_n,
					IPA_HW_WDI_RX_MBOX_START_INDEX/32,
					IPA_HW_WDI_RX_MBOX_START_INDEX % 32);
	}

	return 0;
}

static void ipa3_uc_wdi_loaded_handler(void)
{
	if (!ipa3_ctx) {
		IPAERR("IPA ctx is null\n");
		return;
	}

	if (ipa3_ctx->uc_wdi_ctx.uc_ready_cb) {
		ipa3_ctx->uc_wdi_ctx.uc_ready_cb(
			ipa3_ctx->uc_wdi_ctx.priv);

		ipa3_ctx->uc_wdi_ctx.uc_ready_cb =
			NULL;
		ipa3_ctx->uc_wdi_ctx.priv = NULL;
	}
}

int ipa3_create_wdi_mapping(u32 num_buffers, struct ipa_wdi_buffer_info *info)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_WLAN);
	int i;
	int ret = 0;
	int prot = IOMMU_READ | IOMMU_WRITE;

	if (!info) {
		IPAERR("info = %pK\n", info);
		return -EINVAL;
	}

	if (!cb->valid) {
		IPAERR("No SMMU CB setup\n");
		return -EINVAL;
	}

	if (ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_WLAN]) {
		IPAERR("IPA SMMU not enabled\n");
		return -EINVAL;
	}

	for (i = 0; i < num_buffers; i++) {
		IPADBG("i=%d pa=0x%pa iova=0x%lx sz=0x%zx\n", i,
			&info[i].pa, info[i].iova, info[i].size);
		info[i].result = ipa3_iommu_map(cb->iommu,
			rounddown(info[i].iova, PAGE_SIZE),
			rounddown(info[i].pa, PAGE_SIZE),
			roundup(info[i].size + info[i].pa -
				rounddown(info[i].pa, PAGE_SIZE), PAGE_SIZE),
			prot);
	}

	return ret;
}

int ipa3_release_wdi_mapping(u32 num_buffers, struct ipa_wdi_buffer_info *info)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_WLAN);
	int i;
	int ret = 0;

	if (!info) {
		IPAERR("info = %pK\n", info);
		return -EINVAL;
	}

	if (!cb->valid) {
		IPAERR("No SMMU CB setup\n");
		return -EINVAL;
	}

	for (i = 0; i < num_buffers; i++) {
		IPADBG("i=%d pa=0x%pa iova=0x%lx sz=0x%zx\n", i,
			&info[i].pa, info[i].iova, info[i].size);
		info[i].result = iommu_unmap(cb->iommu,
			rounddown(info[i].iova, PAGE_SIZE),
			roundup(info[i].size + info[i].pa -
				rounddown(info[i].pa, PAGE_SIZE), PAGE_SIZE));
	}

	return ret;
}
