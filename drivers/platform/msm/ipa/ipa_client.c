/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <asm/barrier.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include "ipa_i.h"

/*
 * These values were determined empirically and shows good E2E bi-
 * directional throughputs
 */
#define IPA_HOLB_TMR_EN 0x1
#define IPA_HOLB_TMR_DIS 0x0
#define IPA_HOLB_TMR_DEFAULT_VAL 0x1ff

#define IPA_PKT_FLUSH_TO_US 100

#define IPA_RAM_WDI_SMEM_SIZE 128
#define IPA_HW_INTERFACE_VERSION 0x010E
#define IPA_HW_INTERFACE_WDI_VERSION 0x0001
#define IPA_HW_WDI_RX_MBOX_START_INDEX 48
#define IPA_HW_WDI_TX_MBOX_START_INDEX 50
#define IPA_HW_NUM_FEATURES 0x8
#define IPA_WDI_DMA_POOL_SIZE (max(sizeof(struct IpaHwWdiTxSetUpCmdData_t), \
			sizeof(struct IpaHwWdiRxSetUpCmdData_t)))
#define IPA_WDI_DMA_POOL_ALIGNMENT 8
#define IPA_WDI_DMA_POOL_BOUNDARY 1024
#define FEATURE_ENUM_VAL(feature, opcode) ((feature << 5) | opcode)

#define IPA_WDI_CONNECTED BIT(0)
#define IPA_WDI_ENABLED BIT(1)
#define IPA_WDI_RESUMED BIT(2)

/**
 * enum ipa_hw_features - Values that represent the features supported in IPA HW
 * @IPA_HW_FEATURE_COMMON : Feature related to common operation of IPA HW
 * @IPA_HW_FEATURE_WDI : Feature related to WDI operation in IPA HW
*/
enum ipa_hw_features {
	IPA_HW_FEATURE_COMMON = 0x0,
	IPA_HW_FEATURE_WDI = 0x3,
	IPA_HW_FEATURE_MAX = IPA_HW_NUM_FEATURES
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
 * enum ipa_cpu_2_hw_commands -  Values that represent the WDI commands from CPU
 * @IPA_CPU_2_HW_CMD_WDI_TX_SET_UP : Command to set up WDI Tx Path
 * @IPA_CPU_2_HW_CMD_WDI_RX_SET_UP : Command to set up WDI Rx Path
 * @IPA_CPU_2_HW_CMD_WDI_RX_EXT_CFG : Provide extended config info for Rx path
 * @IPA_CPU_2_HW_CMD_WDI_CH_ENABLE : Command to enable a channel
 * @IPA_CPU_2_HW_CMD_WDI_CH_DISABLE : Command to disable a channel
 * @IPA_CPU_2_HW_CMD_WDI_CH_SUSPEND : Command to suspend a channel
 * @IPA_CPU_2_HW_CMD_WDI_CH_RESUME : Command to resume a channel
 * @IPA_CPU_2_HW_CMD_WDI_TEAR_DOWN : Command to tear down WDI Tx/ Rx Path
 */
enum ipa_cpu_2_hw_commands {
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
 * enum ipa_hw_2_cpu_responses -  Values that represent common HW responses
 * to CPU commands.
 * @IPA_HW_2_CPU_RESPONSE_INIT_COMPLETED : HW shall send this command once
 * boot sequence is completed and HW is ready to serve commands from CPU
 * @IPA_HW_2_CPU_RESPONSE_CMD_COMPLETED: Response to CPU commands
 */
enum ipa_hw_2_cpu_responses {
	IPA_HW_2_CPU_RESPONSE_INIT_COMPLETED =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 1),
	IPA_HW_2_CPU_RESPONSE_CMD_COMPLETED  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 2),
};

/**
 * enum ipa_hw_2_cpu_events - Values that represent HW event to be sent to CPU.
 * @IPA_HW_2_CPU_EVENT_ERROR : Event specify a system error is detected by the
 * device
 * @IPA_HW_2_CPU_EVENT_WDI_ERROR : Event to specify that HW detected an error
 * in WDI
 */
enum ipa_hw_2_cpu_events {
	IPA_HW_2_CPU_EVENT_ERROR     =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 1),
	IPA_HW_2_CPU_EVENT_WDI_ERROR =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_WDI, 0),
};

/**
 * enum ipa_hw_errors - Common error types.
 * @IPA_HW_ERROR_NONE : No error persists
 * @IPA_HW_INVALID_DOORBELL_ERROR : Invalid data read from doorbell
 * @IPA_HW_DMA_ERROR : Unexpected DMA error
 * @IPA_HW_FATAL_SYSTEM_ERROR : HW has crashed and requires reset.
 * @IPA_HW_INVALID_OPCODE : Invalid opcode sent
 */
enum ipa_hw_errors {
	IPA_HW_ERROR_NONE              =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 0),
	IPA_HW_INVALID_DOORBELL_ERROR  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 1),
	IPA_HW_DMA_ERROR               =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 2),
	IPA_HW_FATAL_SYSTEM_ERROR      =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 3),
	IPA_HW_INVALID_OPCODE          =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 4)
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
 * struct IpaHwSharedMemCommonMapping_t - Strucuture referring to the common
 * section in 128B shared memory located in offset zero of SW Partition in IPA
 * SRAM.
 * @cmdOp : CPU->HW command opcode. See IPA_CPU_2_HW_COMMANDS
 * @cmdParams : CPU->HW command parameter. The parameter filed can hold 32 bits
 * of parameters (immediate parameters) and point on structure in system memory
 * (in such case the address must be accessible for HW)
 * @responseOp : HW->CPU response opcode. See IPA_HW_2_CPU_RESPONSES
 * @responseParams : HW->CPU response parameter. The parameter filed can hold 32
 * bits of parameters (immediate parameters) and point on structure in system
 * memory
 * @eventOp : HW->CPU event opcode. See IPA_HW_2_CPU_EVENTS
 * @eventParams : HW->CPU event parameter. The parameter filed can hold 32 bits of
 * parameters (immediate parameters) and point on structure in system memory
 * @firstErrorAddress : Contains the address of first error-source on SNOC
 * @hwState : State of HW. The state carries information regarding the error type.
 * @warningCounter : The warnings counter. The counter carries information regarding
 * non fatal errors in HW
 * @interfaceVersionCommon : The Common interface version as reported by HW
 *
 * The shared memory is used for communication between IPA HW and CPU.
 */
struct IpaHwSharedMemCommonMapping_t {
	u8  cmdOp;
	u8  reserved_01;
	u16 reserved_03_02;
	u32 cmdParams;
	u8  responseOp;
	u8  reserved_09;
	u16 reserved_0B_0A;
	u32 responseParams;
	u8  eventOp;
	u8  reserved_11;
	u16 reserved_13_12;
	u32 eventParams;
	u32 reserved_1B_18;
	u32 firstErrorAddress;
	u8  hwState;
	u8  warningCounter;
	u16 reserved_23_22;
	u16 interfaceVersionCommon;
	u16 reserved_27_26;
} __packed;

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
 * union IpaHwCpuCmdCompletedResponseData_t - Structure holding the parameters
 * for IPA_HW_2_CPU_RESPONSE_CMD_COMPLETED response.
 * @originalCmdOp : The original command opcode
 * @status : 0 for success indication, otherwise failure
 * @reserved : Reserved
 *
 * Parameters are sent as 32b immediate parameters.
 */
union IpaHwCpuCmdCompletedResponseData_t {
	struct IpaHwCpuCmdCompletedResponseParams_t {
		u32 originalCmdOp:8;
		u32 status:8;
		u32 reserved:16;
	} __packed params;
	u32 raw32b;
} __packed;

/**
 * union IpaHwErrorEventData_t - HW->CPU Common Events
 * @errorType : Entered when a system error is detected by the HW. Type of
 * error is specified by IPA_HW_ERRORS
 * @reserved : Reserved
 */
union IpaHwErrorEventData_t {
	struct IpaHwErrorEventParams_t {
		u32 errorType:8;
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

static void ipa_wdi_evt_handler(enum ipa_irq_type interrupt,
				void *private_data,
				void *interrupt_data)
{
	union IpaHwErrorEventData_t evt;
	union IpaHwWdiErrorEventData_t wdi_evt;

	WARN_ON(private_data != ipa_ctx);

	ipa_inc_client_enable_clks();
	IPAERR("WDI evt opcode=%u\n",
			ipa_ctx->wdi.ipa_sram_mmio->common.eventOp);

	if (ipa_ctx->wdi.ipa_sram_mmio->common.eventOp ==
			IPA_HW_2_CPU_EVENT_ERROR) {
		evt.raw32b = ipa_ctx->wdi.ipa_sram_mmio->common.eventParams;
		IPAERR("WDI evt errorType=%u\n", evt.params.errorType);
	} else if (ipa_ctx->wdi.ipa_sram_mmio->common.eventOp ==
			IPA_HW_2_CPU_EVENT_WDI_ERROR) {
		wdi_evt.raw32b = ipa_ctx->wdi.ipa_sram_mmio->common.eventParams;
		IPAERR("WDI evt errorType=%u pipe=%d cherrorType=%u\n",
				wdi_evt.params.wdi_error_type,
				wdi_evt.params.ipa_pipe_number,
				wdi_evt.params.wdi_ch_err_type);
		IPAERR("tx_ch_state=%u rx_ch_state=%u\n",
				ipa_ctx->wdi.ipa_sram_mmio->wdi_tx_ch_0_state,
				ipa_ctx->wdi.ipa_sram_mmio->wdi_rx_ch_0_state);

	} else {
		IPAERR("unsupported WDI evt opcode=%u\n",
				ipa_ctx->wdi.ipa_sram_mmio->common.eventOp);
	}
	ipa_dec_client_disable_clks();
}

static void ipa_wdi_rsp_handler(enum ipa_irq_type interrupt,
				void *private_data,
				void *interrupt_data)
{
	union IpaHwCpuCmdCompletedResponseData_t wdi_rsp;
	WARN_ON(private_data != ipa_ctx);

	ipa_inc_client_enable_clks();
	IPADBG("WDI rsp opcode=%u\n",
			ipa_ctx->wdi.ipa_sram_mmio->common.responseOp);

	if (ipa_ctx->wdi.ipa_sram_mmio->common.responseOp ==
			IPA_HW_2_CPU_RESPONSE_INIT_COMPLETED) {
		ipa_ctx->wdi.uc_loaded = true;
		IPADBG("IPA uc loaded\n");
	} else if (ipa_ctx->wdi.ipa_sram_mmio->common.responseOp ==
			IPA_HW_2_CPU_RESPONSE_CMD_COMPLETED) {
		wdi_rsp.raw32b =
			ipa_ctx->wdi.ipa_sram_mmio->common.responseParams;
		IPADBG("WDI cmd opcode=%u status=%u\n",
				wdi_rsp.params.originalCmdOp,
				wdi_rsp.params.status);
		if (wdi_rsp.params.originalCmdOp == ipa_ctx->wdi.pending_cmd) {
			ipa_ctx->wdi.last_resp = wdi_rsp.params.status;
			complete_all(&ipa_ctx->wdi.cmd_rsp);
		} else {
			IPAERR("expected cmd=%u rcvd cmd=%u\n",
					ipa_ctx->wdi.pending_cmd,
					wdi_rsp.params.originalCmdOp);
		}
	} else {
		IPAERR("Unsupported WDI rsp opcode = %u\n",
				ipa_ctx->wdi.ipa_sram_mmio->common.responseOp);
	}
	ipa_dec_client_disable_clks();
}

int ipa_wdi_init(void)
{
	int result;
	unsigned long phys_addr;

	ipa_ctx->wdi.dma_pool = dma_pool_create("ipa_wdi1k",
			ipa_ctx->pdev,
			IPA_WDI_DMA_POOL_SIZE, IPA_WDI_DMA_POOL_ALIGNMENT,
			IPA_WDI_DMA_POOL_BOUNDARY);
	if (!ipa_ctx->wdi.dma_pool) {
		IPAERR("fail to setup DMA pool\n");
		result = -ENOMEM;
		goto pool_fail;
	}

	mutex_init(&ipa_ctx->wdi.lock);

	if (ipa_ctx->ipa_hw_type == IPA_HW_v2_5) {
		phys_addr = ipa_ctx->ipa_wrapper_base +
			ipa_ctx->ctrl->ipa_reg_base_ofst +
			IPA_SRAM_SW_FIRST_v2_5;
	} else {
		phys_addr = ipa_ctx->ipa_wrapper_base +
			ipa_ctx->ctrl->ipa_reg_base_ofst +
			IPA_SRAM_DIRECT_ACCESS_N_OFST_v2_0(
			ipa_ctx->smem_restricted_bytes / 4);
	}

	ipa_ctx->wdi.ipa_sram_mmio = ioremap(phys_addr, IPA_RAM_WDI_SMEM_SIZE);
	if (!ipa_ctx->wdi.ipa_sram_mmio) {
		IPAERR("fail to ioremap IPA SRAM\n");
		result = -ENOMEM;
		goto remap_fail;
	}

	result = ipa_add_interrupt_handler(IPA_UC_IRQ_0,
			ipa_wdi_evt_handler, true,
			ipa_ctx);
	if (result) {
		IPAERR("fail to register for UC_IRQ0 evt interrupt\n");
		result = -EFAULT;
		goto irq0_fail;
	}

	result = ipa_add_interrupt_handler(IPA_UC_IRQ_1,
			ipa_wdi_rsp_handler, true,
			ipa_ctx);
	if (result) {
		IPAERR("fail to register for UC_IRQ1 rsp interrupt\n");
		result = -EFAULT;
		goto irq1_fail;
	}

	return 0;

irq1_fail:
	ipa_remove_interrupt_handler(IPA_UC_IRQ_0);
irq0_fail:
	iounmap(ipa_ctx->wdi.ipa_sram_mmio);
remap_fail:
	dma_pool_destroy(ipa_ctx->wdi.dma_pool);
pool_fail:
	return result;
}

/**
 * ipa_connect_wdi_pipe() - WDI client connect
 * @in:	[in] input parameters from client
 * @out: [out] output params to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_connect_wdi_pipe(struct ipa_wdi_in_params *in,
		struct ipa_wdi_out_params *out)
{
	int ipa_ep_idx;
	int result = -EFAULT;
	struct ipa_ep_context *ep;
	struct ipa_mem_buffer cmd;
	struct IpaHwWdiTxSetUpCmdData_t *tx;
	struct IpaHwWdiRxSetUpCmdData_t *rx;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;

	if (in == NULL || out == NULL || in->sys.client >= IPA_CLIENT_MAX) {
		IPAERR("bad parm. in=%p out=%p\n", in, out);
		if (in)
			IPAERR("client = %d\n", in->sys.client);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(in->sys.client)) {
		if (in->u.dl.comp_ring_base_pa % IPA_WDI_DMA_POOL_ALIGNMENT ||
			in->u.dl.ce_ring_base_pa % IPA_WDI_DMA_POOL_ALIGNMENT) {
			IPAERR("alignment failure on TX\n");
			return -EINVAL;
		}
	} else {
		if (in->u.ul.rdy_ring_base_pa % IPA_WDI_DMA_POOL_ALIGNMENT) {
			IPAERR("alignment failure on RX\n");
			return -EINVAL;
		}
	}

	if (!ipa_ctx->wdi.uc_loaded) {
		IPAERR("IPA uc not loaded\n");
		return -EFAULT;
	}

	if (ipa_ctx->wdi.uc_failed) {
		IPAERR("IPA uc in failed state\n");
		return -EFAULT;
	}

	ipa_ep_idx = ipa_get_ep_mapping(in->sys.client);
	if (ipa_ep_idx == -1) {
		IPAERR("fail to alloc EP.\n");
		goto fail;
	}

	ep = &ipa_ctx->ep[ipa_ep_idx];

	if (ep->valid) {
		IPAERR("EP already allocated.\n");
		goto fail;
	}

	memset(&ipa_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa_ep_context));
	ipa_inc_client_enable_clks();

	IPADBG("client=%d ep=%d\n", in->sys.client, ipa_ep_idx);
	if (IPA_CLIENT_IS_CONS(in->sys.client)) {
		cmd.size = sizeof(*tx);
		IPADBG("comp_ring_base_pa=0x%pa\n",
				&in->u.dl.comp_ring_base_pa);
		IPADBG("comp_ring_size=%d\n", in->u.dl.comp_ring_size);
		IPADBG("ce_ring_base_pa=0x%pa\n", &in->u.dl.ce_ring_base_pa);
		IPADBG("ce_ring_size=%d\n", in->u.dl.ce_ring_size);
		IPADBG("ce_ring_doorbell_pa=0x%pa\n",
				&in->u.dl.ce_door_bell_pa);
		IPADBG("num_tx_buffers=%d\n", in->u.dl.num_tx_buffers);
	} else {
		cmd.size = sizeof(*rx);
		IPADBG("rx_ring_base_pa=0x%pa\n", &in->u.ul.rdy_ring_base_pa);
		IPADBG("rx_ring_size=%d\n", in->u.ul.rdy_ring_size);
		IPADBG("rx_ring_rp_pa=0x%pa\n", &in->u.ul.rdy_ring_rp_pa);
	}

	cmd.base = dma_pool_alloc(ipa_ctx->wdi.dma_pool, GFP_KERNEL,
			&cmd.phys_base);
	if (cmd.base == NULL) {
		IPAERR("fail to get DMA memory.\n");
		result = -ENOMEM;
		goto dma_alloc_fail;
	}

	if (IPA_CLIENT_IS_CONS(in->sys.client)) {
		tx = (struct IpaHwWdiTxSetUpCmdData_t *)cmd.base;
		tx->comp_ring_base_pa = in->u.dl.comp_ring_base_pa;
		tx->comp_ring_size = in->u.dl.comp_ring_size;
		tx->ce_ring_base_pa = in->u.dl.ce_ring_base_pa;
		tx->ce_ring_size = in->u.dl.ce_ring_size;
		tx->ce_ring_doorbell_pa = in->u.dl.ce_door_bell_pa;
		tx->num_tx_buffers = in->u.dl.num_tx_buffers;
		tx->ipa_pipe_number = ipa_ep_idx;
		out->uc_door_bell_pa = ipa_ctx->ipa_wrapper_base +
		IPA_UC_MAILBOX_m_n_OFFS(IPA_HW_WDI_TX_MBOX_START_INDEX/32,
					IPA_HW_WDI_TX_MBOX_START_INDEX % 32);
	} else {
		rx = (struct IpaHwWdiRxSetUpCmdData_t *)cmd.base;
		rx->rx_ring_base_pa = in->u.ul.rdy_ring_base_pa;
		rx->rx_ring_size = in->u.ul.rdy_ring_size;
		rx->rx_ring_rp_pa = in->u.ul.rdy_ring_rp_pa;
		rx->ipa_pipe_number = ipa_ep_idx;
		out->uc_door_bell_pa = ipa_ctx->ipa_wrapper_base +
		IPA_UC_MAILBOX_m_n_OFFS(IPA_HW_WDI_RX_MBOX_START_INDEX/32,
					IPA_HW_WDI_RX_MBOX_START_INDEX % 32);
	}

	mutex_lock(&ipa_ctx->wdi.lock);
	ipa_ctx->wdi.ipa_sram_mmio->common.cmdParams = cmd.phys_base;
	if (IPA_CLIENT_IS_CONS(in->sys.client))
		ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp =
			IPA_CPU_2_HW_CMD_WDI_TX_SET_UP;
	else
		ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp =
			IPA_CPU_2_HW_CMD_WDI_RX_SET_UP;
	init_completion(&ipa_ctx->wdi.cmd_rsp);
	ipa_ctx->wdi.pending_cmd = ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp;
	wmb();
	ep->valid = 1;
	ep->client = in->sys.client;
	ep->keep_ipa_awake = in->sys.keep_ipa_awake;
	result = ipa_disable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("disable data path failed res=%d clnt=%d.\n", result,
				ipa_ep_idx);
		goto uc_timeout;
	}
	if (IPA_CLIENT_IS_PROD(in->sys.client)) {
		memset(&ep_cfg_ctrl, 0 , sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_delay = true;
		ipa_cfg_ep_ctrl(ipa_ep_idx, &ep_cfg_ctrl);
	}
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_EE_UC_n_OFFS(0), 0x1);
	if (wait_for_completion_timeout(&ipa_ctx->wdi.cmd_rsp, 10*HZ) == 0) {
		IPAERR("uc timed out on setup ep=%d.\n", ipa_ep_idx);
		result = -EFAULT;
		ipa_ctx->wdi.uc_failed = true;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	if (ipa_ctx->wdi.last_resp != IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS) {
		IPAERR("cmd failed on setup ep=%d status=%d.\n", ipa_ep_idx,
				ipa_ctx->wdi.last_resp);
		result = -EFAULT;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	mutex_unlock(&ipa_ctx->wdi.lock);

	ep->skip_ep_cfg = in->sys.skip_ep_cfg;
	ep->client_notify = in->sys.notify;
	ep->priv = in->sys.priv;

	if (!ep->skip_ep_cfg) {
		if (ipa_cfg_ep(ipa_ep_idx, &in->sys.ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto ipa_cfg_ep_fail;
		}
		IPADBG("ep configuration successful\n");
	} else {
		IPADBG("Skipping endpoint configuration.\n");
	}

	out->clnt_hdl = ipa_ep_idx;

	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(in->sys.client))
		ipa_install_dflt_flt_rules(ipa_ep_idx);

	if (!ep->keep_ipa_awake)
		ipa_dec_client_disable_clks();

	dma_pool_free(ipa_ctx->wdi.dma_pool, cmd.base, cmd.phys_base);
	ep->wdi_state |= IPA_WDI_CONNECTED;
	IPADBG("client %d (ep: %d) connected\n", in->sys.client, ipa_ep_idx);

	return 0;

ipa_cfg_ep_fail:
	memset(&ipa_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa_ep_context));
uc_timeout:
	dma_pool_free(ipa_ctx->wdi.dma_pool, cmd.base, cmd.phys_base);
dma_alloc_fail:
	ipa_dec_client_disable_clks();
fail:
	return result;
}
EXPORT_SYMBOL(ipa_connect_wdi_pipe);

/**
 * ipa_disconnect_wdi_pipe() - WDI client disconnect
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_disconnect_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa_ep_context *ep;
	union IpaHwWdiCommonChCmdData_t tear;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	if (!ipa_ctx->wdi.uc_loaded) {
		IPAERR("IPA uc not loaded\n");
		return -EFAULT;
	}

	if (ipa_ctx->wdi.uc_failed) {
		IPAERR("IPA uc in failed state\n");
		return -EFAULT;
	}

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa_ctx->ep[clnt_hdl];

	if (ep->wdi_state != IPA_WDI_CONNECTED) {
		IPAERR("WDI channel bad state %d\n", ep->wdi_state);
		return -EFAULT;
	}

	if (!ep->keep_ipa_awake)
		ipa_inc_client_enable_clks();

	tear.params.ipa_pipe_number = clnt_hdl;
	mutex_lock(&ipa_ctx->wdi.lock);
	ipa_ctx->wdi.ipa_sram_mmio->common.cmdParams = tear.raw32b;
	ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp =
		IPA_CPU_2_HW_CMD_WDI_TEAR_DOWN;
	init_completion(&ipa_ctx->wdi.cmd_rsp);
	ipa_ctx->wdi.pending_cmd = ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp;
	wmb();
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_EE_UC_n_OFFS(0), 0x1);
	if (wait_for_completion_timeout(&ipa_ctx->wdi.cmd_rsp, 10*HZ) == 0) {
		IPAERR("uc timed out on tear down ep=%d.\n", clnt_hdl);
		result = -EFAULT;
		ipa_ctx->wdi.uc_failed = true;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	if (ipa_ctx->wdi.last_resp != IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS) {
		IPAERR("cmd failed on tear down ep=%d status=%d.\n", clnt_hdl,
				ipa_ctx->wdi.last_resp);
		result = -EFAULT;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	mutex_unlock(&ipa_ctx->wdi.lock);

	ipa_delete_dflt_flt_rules(clnt_hdl);
	memset(&ipa_ctx->ep[clnt_hdl], 0, sizeof(struct ipa_ep_context));
	ipa_dec_client_disable_clks();

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

uc_timeout:
	return result;
}
EXPORT_SYMBOL(ipa_disconnect_wdi_pipe);

/**
 * ipa_enable_wdi_pipe() - WDI client enable
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_enable_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa_ep_context *ep;
	union IpaHwWdiCommonChCmdData_t enable;
	struct ipa_ep_cfg_holb holb_cfg;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	if (!ipa_ctx->wdi.uc_loaded) {
		IPAERR("IPA uc not loaded\n");
		return -EFAULT;
	}

	if (ipa_ctx->wdi.uc_failed) {
		IPAERR("IPA uc in failed state\n");
		return -EFAULT;
	}

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa_ctx->ep[clnt_hdl];

	if (ep->wdi_state != IPA_WDI_CONNECTED) {
		IPAERR("WDI channel bad state %d\n", ep->wdi_state);
		return -EFAULT;
	}

	ipa_inc_client_enable_clks();
	enable.params.ipa_pipe_number = clnt_hdl;
	mutex_lock(&ipa_ctx->wdi.lock);
	ipa_ctx->wdi.ipa_sram_mmio->common.cmdParams = enable.raw32b;
	ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp =
		IPA_CPU_2_HW_CMD_WDI_CH_ENABLE;
	init_completion(&ipa_ctx->wdi.cmd_rsp);
	ipa_ctx->wdi.pending_cmd = ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp;
	wmb();
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_EE_UC_n_OFFS(0), 0x1);
	if (wait_for_completion_timeout(&ipa_ctx->wdi.cmd_rsp, 10*HZ) == 0) {
		IPAERR("uc timed out on enable ep=%d.\n", clnt_hdl);
		result = -EFAULT;
		ipa_ctx->wdi.uc_failed = true;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	if (ipa_ctx->wdi.last_resp != IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS) {
		IPAERR("cmd failed on enable ep=%d status=%d.\n", clnt_hdl,
				ipa_ctx->wdi.last_resp);
		result = -EFAULT;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	mutex_unlock(&ipa_ctx->wdi.lock);

	/* On IPA 2.0, disable HOLB */
	if (ipa_ctx->ipa_hw_type == IPA_HW_v2_0 &&
	    IPA_CLIENT_IS_CONS(ep->client)) {
		memset(&holb_cfg, 0 , sizeof(holb_cfg));
		holb_cfg.en = IPA_HOLB_TMR_DIS;
		holb_cfg.tmr_val = 0;
		result = ipa_cfg_ep_holb(clnt_hdl, &holb_cfg);
	}

	ipa_dec_client_disable_clks();
	ep->wdi_state |= IPA_WDI_ENABLED;
	IPADBG("client (ep: %d) enabled\n", clnt_hdl);

uc_timeout:
	return result;
}
EXPORT_SYMBOL(ipa_enable_wdi_pipe);

/**
 * ipa_disable_wdi_pipe() - WDI client disable
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_disable_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa_ep_context *ep;
	union IpaHwWdiCommonChCmdData_t disable;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	if (!ipa_ctx->wdi.uc_loaded) {
		IPAERR("IPA uc not loaded\n");
		return -EFAULT;
	}

	if (ipa_ctx->wdi.uc_failed) {
		IPAERR("IPA uc in failed state\n");
		return -EFAULT;
	}

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa_ctx->ep[clnt_hdl];

	if (ep->wdi_state != (IPA_WDI_CONNECTED | IPA_WDI_ENABLED)) {
		IPAERR("WDI channel bad state %d\n", ep->wdi_state);
		return -EFAULT;
	}

	ipa_inc_client_enable_clks();
	disable.params.ipa_pipe_number = clnt_hdl;
	mutex_lock(&ipa_ctx->wdi.lock);
	ipa_ctx->wdi.ipa_sram_mmio->common.cmdParams = disable.raw32b;
	ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp =
		IPA_CPU_2_HW_CMD_WDI_CH_DISABLE;
	init_completion(&ipa_ctx->wdi.cmd_rsp);
	ipa_ctx->wdi.pending_cmd = ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp;
	wmb();
	result = ipa_disable_data_path(clnt_hdl);
	if (result) {
		IPAERR("disable data path failed res=%d clnt=%d.\n", result,
				clnt_hdl);
		result = -EPERM;
		goto uc_timeout;
	}
	if (IPA_CLIENT_IS_PROD(ep->client)) {
		memset(&ep_cfg_ctrl, 0 , sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_delay = true;
		ipa_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
	}
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_EE_UC_n_OFFS(0), 0x1);
	if (wait_for_completion_timeout(&ipa_ctx->wdi.cmd_rsp, 10*HZ) == 0) {
		IPAERR("uc timed out on disable ep=%d.\n", clnt_hdl);
		result = -EFAULT;
		ipa_ctx->wdi.uc_failed = true;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	if (ipa_ctx->wdi.last_resp != IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS) {
		IPAERR("cmd failed on disable ep=%d status=%d.\n", clnt_hdl,
				ipa_ctx->wdi.last_resp);
		result = -EFAULT;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	mutex_unlock(&ipa_ctx->wdi.lock);

	ipa_dec_client_disable_clks();
	ep->wdi_state &= ~IPA_WDI_ENABLED;
	IPADBG("client (ep: %d) disabled\n", clnt_hdl);

uc_timeout:
	return result;
}
EXPORT_SYMBOL(ipa_disable_wdi_pipe);

/**
 * ipa_resume_wdi_pipe() - WDI client resume
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_resume_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa_ep_context *ep;
	union IpaHwWdiCommonChCmdData_t resume;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	if (!ipa_ctx->wdi.uc_loaded) {
		IPAERR("IPA uc not loaded\n");
		return -EFAULT;
	}

	if (ipa_ctx->wdi.uc_failed) {
		IPAERR("IPA uc in failed state\n");
		return -EFAULT;
	}

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa_ctx->ep[clnt_hdl];

	if (ep->wdi_state != (IPA_WDI_CONNECTED | IPA_WDI_ENABLED)) {
		IPAERR("WDI channel bad state %d\n", ep->wdi_state);
		return -EFAULT;
	}

	ipa_inc_client_enable_clks();
	resume.params.ipa_pipe_number = clnt_hdl;
	mutex_lock(&ipa_ctx->wdi.lock);
	ipa_ctx->wdi.ipa_sram_mmio->common.cmdParams = resume.raw32b;
	ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp =
		IPA_CPU_2_HW_CMD_WDI_CH_RESUME;
	init_completion(&ipa_ctx->wdi.cmd_rsp);
	ipa_ctx->wdi.pending_cmd = ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp;
	wmb();
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_EE_UC_n_OFFS(0), 0x1);
	if (wait_for_completion_timeout(&ipa_ctx->wdi.cmd_rsp, 10*HZ) == 0) {
		IPAERR("uc timed out on resume ep=%d.\n", clnt_hdl);
		result = -EFAULT;
		ipa_ctx->wdi.uc_failed = true;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	if (ipa_ctx->wdi.last_resp != IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS) {
		IPAERR("cmd failed on resume ep=%d status=%d.\n", clnt_hdl,
				ipa_ctx->wdi.last_resp);
		result = -EFAULT;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	mutex_unlock(&ipa_ctx->wdi.lock);

	if (IPA_CLIENT_IS_PROD(ep->client) ||
		(IPA_CLIENT_IS_CONS(ep->client) && ep->keep_ipa_awake)) {
		memset(&ep_cfg_ctrl, 0 , sizeof(struct ipa_ep_cfg_ctrl));
		result = ipa_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
		if (result)
			IPAERR("client (ep: %d) fail un-susp/delay result=%d\n",
					clnt_hdl, result);
		else
			IPADBG("client (ep: %d) un-susp/delay\n", clnt_hdl);
	}

	ep->wdi_state |= IPA_WDI_RESUMED;
	IPADBG("client (ep: %d) resumed\n", clnt_hdl);

uc_timeout:
	return result;
}
EXPORT_SYMBOL(ipa_resume_wdi_pipe);

/**
 * ipa_suspend_wdi_pipe() - WDI client suspend
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_suspend_wdi_pipe(u32 clnt_hdl)
{
	int result = 0;
	struct ipa_ep_context *ep;
	union IpaHwWdiCommonChCmdData_t suspend;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	if (!ipa_ctx->wdi.uc_loaded) {
		IPAERR("IPA uc not loaded\n");
		return -EFAULT;
	}

	if (ipa_ctx->wdi.uc_failed) {
		IPAERR("IPA uc in failed state\n");
		return -EFAULT;
	}

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa_ctx->ep[clnt_hdl];

	if (ep->wdi_state != (IPA_WDI_CONNECTED | IPA_WDI_ENABLED |
				IPA_WDI_RESUMED)) {
		IPAERR("WDI channel bad state %d\n", ep->wdi_state);
		return -EFAULT;
	}

	suspend.params.ipa_pipe_number = clnt_hdl;
	mutex_lock(&ipa_ctx->wdi.lock);
	ipa_ctx->wdi.ipa_sram_mmio->common.cmdParams = suspend.raw32b;
	ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp =
		IPA_CPU_2_HW_CMD_WDI_CH_SUSPEND;
	init_completion(&ipa_ctx->wdi.cmd_rsp);
	ipa_ctx->wdi.pending_cmd = ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp;
	wmb();
	memset(&ep_cfg_ctrl, 0 , sizeof(struct ipa_ep_cfg_ctrl));
	if (IPA_CLIENT_IS_CONS(ep->client)) {
		ep_cfg_ctrl.ipa_ep_suspend = true;
		result = ipa_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
		if (result)
			IPAERR("client (ep: %d) failed to suspend result=%d\n",
					clnt_hdl, result);
		else
			IPADBG("client (ep: %d) suspended\n", clnt_hdl);
	} else {
		ep_cfg_ctrl.ipa_ep_delay = true;
		result = ipa_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
		if (result)
			IPAERR("client (ep: %d) failed to delay result=%d\n",
					clnt_hdl, result);
		else
			IPADBG("client (ep: %d) delayed\n", clnt_hdl);
	}
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_EE_UC_n_OFFS(0), 0x1);
	if (wait_for_completion_timeout(&ipa_ctx->wdi.cmd_rsp, 10*HZ) == 0) {
		IPAERR("uc timed out on suspend ep=%d.\n", clnt_hdl);
		result = -EFAULT;
		ipa_ctx->wdi.uc_failed = true;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	if (ipa_ctx->wdi.last_resp != IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS) {
		IPAERR("cmd failed on suspend ep=%d status=%d.\n", clnt_hdl,
				ipa_ctx->wdi.last_resp);
		result = -EFAULT;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	mutex_unlock(&ipa_ctx->wdi.lock);

	ipa_ctx->tag_process_before_gating = true;
	ipa_dec_client_disable_clks();
	ep->wdi_state &= ~IPA_WDI_RESUMED;
	IPADBG("client (ep: %d) suspended\n", clnt_hdl);

uc_timeout:
	return result;
}
EXPORT_SYMBOL(ipa_suspend_wdi_pipe);

int ipa_write_qmapid_wdi_pipe(u32 clnt_hdl, u8 qmap_id)
{
	int result = 0;
	struct ipa_ep_context *ep;
	union IpaHwWdiRxExtCfgCmdData_t qmap;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	if (!ipa_ctx->wdi.uc_loaded) {
		IPAERR("IPA uc not loaded\n");
		return -EFAULT;
	}

	if (ipa_ctx->wdi.uc_failed) {
		IPAERR("IPA uc in failed state\n");
		return -EFAULT;
	}

	IPADBG("ep=%d\n", clnt_hdl);

	ep = &ipa_ctx->ep[clnt_hdl];

	if (!(ep->wdi_state & IPA_WDI_CONNECTED)) {
		IPAERR("WDI channel bad state %d\n", ep->wdi_state);
		return -EFAULT;
	}

	ipa_inc_client_enable_clks();
	qmap.params.ipa_pipe_number = clnt_hdl;
	qmap.params.qmap_id = qmap_id;
	mutex_lock(&ipa_ctx->wdi.lock);
	ipa_ctx->wdi.ipa_sram_mmio->common.cmdParams = qmap.raw32b;
	ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp =
		IPA_CPU_2_HW_CMD_WDI_RX_EXT_CFG;
	init_completion(&ipa_ctx->wdi.cmd_rsp);
	ipa_ctx->wdi.pending_cmd = ipa_ctx->wdi.ipa_sram_mmio->common.cmdOp;
	wmb();
	ipa_write_reg(ipa_ctx->mmio, IPA_IRQ_EE_UC_n_OFFS(0), 0x1);
	if (wait_for_completion_timeout(&ipa_ctx->wdi.cmd_rsp, 10*HZ) == 0) {
		IPAERR("uc timed out on qmap ep=%d.\n", clnt_hdl);
		result = -EFAULT;
		ipa_ctx->wdi.uc_failed = true;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	if (ipa_ctx->wdi.last_resp != IPA_HW_2_CPU_WDI_CMD_STATUS_SUCCESS) {
		IPAERR("cmd failed on qmap ep=%d status=%d.\n", clnt_hdl,
				ipa_ctx->wdi.last_resp);
		result = -EFAULT;
		mutex_unlock(&ipa_ctx->wdi.lock);
		goto uc_timeout;
	}
	mutex_unlock(&ipa_ctx->wdi.lock);

	ipa_dec_client_disable_clks();

	IPADBG("client (ep: %d) qmap_id %d updated\n", clnt_hdl, qmap_id);

uc_timeout:
	return result;
}

int ipa_enable_data_path(u32 clnt_hdl)
{
	struct ipa_ep_context *ep = &ipa_ctx->ep[clnt_hdl];
	struct ipa_ep_cfg_holb holb_cfg;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	int res = 0;

	IPADBG("Enabling data path\n");
	/* IPA_HW_MODE_VIRTUAL lacks support for TAG IC & EP suspend */
	if (ipa_ctx->ipa_hw_mode == IPA_HW_MODE_VIRTUAL)
		return 0;

	/* From IPA 2.0, disable HOLB */
	if ((ipa_ctx->ipa_hw_type == IPA_HW_v2_0 ||
		ipa_ctx->ipa_hw_type == IPA_HW_v2_5) &&
		IPA_CLIENT_IS_CONS(ep->client)) {
		memset(&holb_cfg, 0 , sizeof(holb_cfg));
		holb_cfg.en = IPA_HOLB_TMR_DIS;
		holb_cfg.tmr_val = 0;
		res = ipa_cfg_ep_holb(clnt_hdl, &holb_cfg);
	}

	/* Enable the pipe */
	if (IPA_CLIENT_IS_CONS(ep->client) &&
	    (ep->keep_ipa_awake ||
	     ep->resume_on_connect ||
	     !ipa_should_pipe_be_suspended(ep->client))) {
		memset(&ep_cfg_ctrl, 0 , sizeof(ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_suspend = false;
		ipa_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
		ep->resume_on_connect = false;
	}

	return res;
}

int ipa_disable_data_path(u32 clnt_hdl)
{
	struct ipa_ep_context *ep = &ipa_ctx->ep[clnt_hdl];
	struct ipa_ep_cfg_holb holb_cfg;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	u32 aggr_init;
	int res = 0;

	IPADBG("Disabling data path\n");
	/* IPA_HW_MODE_VIRTUAL lacks support for TAG IC & EP suspend */
	if (ipa_ctx->ipa_hw_mode == IPA_HW_MODE_VIRTUAL)
		return 0;

	/* On IPA 2.0, enable HOLB in order to prevent IPA from stalling */
	if ((ipa_ctx->ipa_hw_type == IPA_HW_v2_0 ||
		ipa_ctx->ipa_hw_type == IPA_HW_v2_5) &&
		IPA_CLIENT_IS_CONS(ep->client)) {
		memset(&holb_cfg, 0, sizeof(holb_cfg));
		holb_cfg.en = IPA_HOLB_TMR_EN;
		holb_cfg.tmr_val = 0;
		res = ipa_cfg_ep_holb(clnt_hdl, &holb_cfg);
	}

	/* Suspend the pipe */
	if (IPA_CLIENT_IS_CONS(ep->client)) {
		memset(&ep_cfg_ctrl, 0 , sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_suspend = true;
		ipa_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
	}

	udelay(IPA_PKT_FLUSH_TO_US);
	aggr_init = ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_AGGR_N_OFST_v2_0(clnt_hdl));
	if (((aggr_init & IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK) >>
	    IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT) == IPA_ENABLE_AGGR)
		ipa_tag_aggr_force_close(clnt_hdl);

	return res;
}

static int ipa_connect_configure_sps(const struct ipa_connect_params *in,
				     struct ipa_ep_context *ep, int ipa_ep_idx)
{
	int result = -EFAULT;

	/* Default Config */
	ep->ep_hdl = sps_alloc_endpoint();

	if (ep->ep_hdl == NULL) {
		IPAERR("SPS EP alloc failed EP.\n");
		return -EFAULT;
	}

	result = sps_get_config(ep->ep_hdl,
		&ep->connect);
	if (result) {
		IPAERR("fail to get config.\n");
		return -EFAULT;
	}

	/* Specific Config */
	if (IPA_CLIENT_IS_CONS(in->client)) {
		ep->connect.mode = SPS_MODE_SRC;
		ep->connect.destination =
			in->client_bam_hdl;
		ep->connect.source = ipa_ctx->bam_handle;
		ep->connect.dest_pipe_index =
			in->client_ep_idx;
		ep->connect.src_pipe_index = ipa_ep_idx;
	} else {
		ep->connect.mode = SPS_MODE_DEST;
		ep->connect.source = in->client_bam_hdl;
		ep->connect.destination = ipa_ctx->bam_handle;
		ep->connect.src_pipe_index = in->client_ep_idx;
		ep->connect.dest_pipe_index = ipa_ep_idx;
	}

	return 0;
}

static int ipa_connect_allocate_fifo(const struct ipa_connect_params *in,
				     struct sps_mem_buffer *mem_buff_ptr,
				     bool *fifo_in_pipe_mem_ptr,
				     u32 *fifo_pipe_mem_ofst_ptr,
				     u32 fifo_size, int ipa_ep_idx)
{
	dma_addr_t dma_addr;
	u32 ofst;
	int result = -EFAULT;

	mem_buff_ptr->size = fifo_size;
	if (in->pipe_mem_preferred) {
		if (ipa_pipe_mem_alloc(&ofst, fifo_size)) {
			IPAERR("FIFO pipe mem alloc fail ep %u\n",
				ipa_ep_idx);
			mem_buff_ptr->base =
				dma_alloc_coherent(ipa_ctx->pdev,
				mem_buff_ptr->size,
				&dma_addr, GFP_KERNEL);
		} else {
			memset(mem_buff_ptr, 0, sizeof(struct sps_mem_buffer));
			result = sps_setup_bam2bam_fifo(mem_buff_ptr, ofst,
				fifo_size, 1);
			WARN_ON(result);
			*fifo_in_pipe_mem_ptr = 1;
			dma_addr = mem_buff_ptr->phys_base;
			*fifo_pipe_mem_ofst_ptr = ofst;
		}
	} else {
		mem_buff_ptr->base =
			dma_alloc_coherent(ipa_ctx->pdev, mem_buff_ptr->size,
			&dma_addr, GFP_KERNEL);
	}
	mem_buff_ptr->phys_base = dma_addr;
	if (mem_buff_ptr->base == NULL) {
		IPAERR("fail to get DMA memory.\n");
		return -EFAULT;
	}

	return 0;
}

/**
 * ipa_connect() - low-level IPA client connect
 * @in:	[in] input parameters from client
 * @sps:	[out] sps output from IPA needed by client for sps_connect
 * @clnt_hdl:	[out] opaque client handle assigned by IPA to client
 *
 * Should be called by the driver of the peripheral that wants to connect to
 * IPA in BAM-BAM mode. these peripherals are USB and HSIC. this api
 * expects caller to take responsibility to add any needed headers, routing
 * and filtering tables and rules as needed.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_connect(const struct ipa_connect_params *in, struct ipa_sps_params *sps,
		u32 *clnt_hdl)
{
	int ipa_ep_idx;
	int result = -EFAULT;
	struct ipa_ep_context *ep;

	IPADBG("connecting client\n");

	if (in == NULL || sps == NULL || clnt_hdl == NULL ||
	    in->client >= IPA_CLIENT_MAX ||
	    in->desc_fifo_sz == 0 || in->data_fifo_sz == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ipa_ep_idx = ipa_get_ep_mapping(in->client);
	if (ipa_ep_idx == -1) {
		IPAERR("fail to alloc EP.\n");
		goto fail;
	}

	ep = &ipa_ctx->ep[ipa_ep_idx];

	if (ep->valid) {
		IPAERR("EP already allocated.\n");
		goto fail;
	}

	memset(&ipa_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa_ep_context));
	ipa_inc_client_enable_clks();

	ep->skip_ep_cfg = in->skip_ep_cfg;
	ep->valid = 1;
	ep->client = in->client;
	ep->client_notify = in->notify;
	ep->priv = in->priv;
	ep->keep_ipa_awake = in->keep_ipa_awake;

	result = ipa_enable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("enable data path failed res=%d clnt=%d.\n", result,
				ipa_ep_idx);
		goto ipa_cfg_ep_fail;
	}

	if (!ep->skip_ep_cfg) {
		if (ipa_cfg_ep(ipa_ep_idx, &in->ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto ipa_cfg_ep_fail;
		}
		IPADBG("ep configuration successful\n");
	} else {
		IPADBG("Skipping endpoint configuration.\n");
	}

	result = ipa_connect_configure_sps(in, ep, ipa_ep_idx);
	if (result) {
		IPAERR("fail to configure SPS.\n");
		goto ipa_cfg_ep_fail;
	}

	if (in->desc.base == NULL) {
		result = ipa_connect_allocate_fifo(in, &ep->connect.desc,
						  &ep->desc_fifo_in_pipe_mem,
						  &ep->desc_fifo_pipe_mem_ofst,
						  in->desc_fifo_sz, ipa_ep_idx);
		if (result) {
			IPAERR("fail to allocate DESC FIFO.\n");
			goto desc_mem_alloc_fail;
		}
	} else {
		IPADBG("client allocated DESC FIFO\n");
		ep->connect.desc = in->desc;
		ep->desc_fifo_client_allocated = 1;
	}
	IPADBG("Descriptor FIFO pa=%pa, size=%d\n", &ep->connect.desc.phys_base,
	       ep->connect.desc.size);

	if (in->data.base == NULL) {
		result = ipa_connect_allocate_fifo(in, &ep->connect.data,
						&ep->data_fifo_in_pipe_mem,
						&ep->data_fifo_pipe_mem_ofst,
						in->data_fifo_sz, ipa_ep_idx);
		if (result) {
			IPAERR("fail to allocate DATA FIFO.\n");
			goto data_mem_alloc_fail;
		}
	} else {
		IPADBG("client allocated DATA FIFO\n");
		ep->connect.data = in->data;
		ep->data_fifo_client_allocated = 1;
	}
	IPADBG("Data FIFO pa=%pa, size=%d\n", &ep->connect.data.phys_base,
	       ep->connect.data.size);

	if ((ipa_ctx->ipa_hw_type == IPA_HW_v2_0 ||
		ipa_ctx->ipa_hw_type == IPA_HW_v2_5) &&
		IPA_CLIENT_IS_USB_CONS(in->client))
		ep->connect.event_thresh = IPA_USB_EVENT_THRESHOLD;
	else
		ep->connect.event_thresh = IPA_EVENT_THRESHOLD;
	ep->connect.options = SPS_O_AUTO_ENABLE;    /* BAM-to-BAM */

	result = sps_connect(ep->ep_hdl, &ep->connect);
	if (result) {
		IPAERR("sps_connect fails.\n");
		goto sps_connect_fail;
	}

	sps->ipa_bam_hdl = ipa_ctx->bam_handle;
	sps->ipa_ep_idx = ipa_ep_idx;
	*clnt_hdl = ipa_ep_idx;
	memcpy(&sps->desc, &ep->connect.desc, sizeof(struct sps_mem_buffer));
	memcpy(&sps->data, &ep->connect.data, sizeof(struct sps_mem_buffer));

	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(in->client))
		ipa_install_dflt_flt_rules(ipa_ep_idx);

	if (!ep->keep_ipa_awake)
		ipa_dec_client_disable_clks();

	ipa_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;

	IPADBG("client %d (ep: %d) connected\n", in->client, ipa_ep_idx);

	return 0;

sps_connect_fail:
	if (!ep->data_fifo_in_pipe_mem)
		dma_free_coherent(ipa_ctx->pdev,
				  ep->connect.data.size,
				  ep->connect.data.base,
				  ep->connect.data.phys_base);
	else
		ipa_pipe_mem_free(ep->data_fifo_pipe_mem_ofst,
				  ep->connect.data.size);

data_mem_alloc_fail:
	if (!ep->desc_fifo_in_pipe_mem)
		dma_free_coherent(ipa_ctx->pdev,
				  ep->connect.desc.size,
				  ep->connect.desc.base,
				  ep->connect.desc.phys_base);
	else
		ipa_pipe_mem_free(ep->desc_fifo_pipe_mem_ofst,
				  ep->connect.desc.size);

desc_mem_alloc_fail:
	sps_free_endpoint(ep->ep_hdl);
ipa_cfg_ep_fail:
	memset(&ipa_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa_ep_context));
	ipa_dec_client_disable_clks();
fail:
	return result;
}
EXPORT_SYMBOL(ipa_connect);

/**
 * ipa_disconnect() - low-level IPA client disconnect
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Should be called by the driver of the peripheral that wants to disconnect
 * from IPA in BAM-BAM mode. this api expects caller to take responsibility to
 * free any needed headers, routing and filtering tables and rules as needed.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_disconnect(u32 clnt_hdl)
{
	int result;
	struct ipa_ep_context *ep;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		ipa_inc_client_enable_clks();

	result = ipa_disable_data_path(clnt_hdl);
	if (result) {
		IPAERR("disable data path failed res=%d clnt=%d.\n", result,
				clnt_hdl);
		return -EPERM;
	}

	result = sps_disconnect(ep->ep_hdl);
	if (result) {
		IPAERR("SPS disconnect failed.\n");
		return -EPERM;
	}

	if (!ep->desc_fifo_client_allocated &&
	     ep->connect.desc.base) {
		if (!ep->desc_fifo_in_pipe_mem)
			dma_free_coherent(ipa_ctx->pdev,
					  ep->connect.desc.size,
					  ep->connect.desc.base,
					  ep->connect.desc.phys_base);
		else
			ipa_pipe_mem_free(ep->desc_fifo_pipe_mem_ofst,
					  ep->connect.desc.size);
	}

	if (!ep->data_fifo_client_allocated &&
	     ep->connect.data.base) {
		if (!ep->data_fifo_in_pipe_mem)
			dma_free_coherent(ipa_ctx->pdev,
					  ep->connect.data.size,
					  ep->connect.data.base,
					  ep->connect.data.phys_base);
		else
			ipa_pipe_mem_free(ep->data_fifo_pipe_mem_ofst,
					  ep->connect.data.size);
	}

	result = sps_free_endpoint(ep->ep_hdl);
	if (result) {
		IPAERR("SPS de-alloc EP failed.\n");
		return -EPERM;
	}

	ipa_delete_dflt_flt_rules(clnt_hdl);

	memset(&ipa_ctx->ep[clnt_hdl], 0, sizeof(struct ipa_ep_context));

	ipa_dec_client_disable_clks();

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

	return 0;
}
EXPORT_SYMBOL(ipa_disconnect);

/**
* ipa_reset_endpoint() - reset an endpoint from BAM perspective
* @clnt_hdl: [in] IPA client handle
*
* Returns:	0 on success, negative on failure
*
* Note:	Should not be called from atomic context
*/
int ipa_reset_endpoint(u32 clnt_hdl)
{
	int res;
	struct ipa_ep_context *ep;

	if (clnt_hdl >= IPA_NUM_PIPES) {
		IPAERR("Bad parameters.\n");
		return -EFAULT;
	}
	ep = &ipa_ctx->ep[clnt_hdl];

	ipa_inc_client_enable_clks();
	res = sps_disconnect(ep->ep_hdl);
	if (res) {
		IPAERR("sps_disconnect() failed, res=%d.\n", res);
		goto bail;
	} else {
		res = sps_connect(ep->ep_hdl, &ep->connect);
		if (res) {
			IPAERR("sps_connect() failed, res=%d.\n", res);
			goto bail;
		}
	}

bail:
	ipa_dec_client_disable_clks();

	return res;
}
EXPORT_SYMBOL(ipa_reset_endpoint);
