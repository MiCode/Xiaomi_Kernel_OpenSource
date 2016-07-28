/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef _IPA_UC_OFFLOAD_I_H_
#define _IPA_UC_OFFLOAD_I_H_

#include <linux/ipa.h>
#include "ipa_i.h"

/*
 * Neutrino protocol related data structures
 */

#define IPA_UC_MAX_NTN_TX_CHANNELS 1
#define IPA_UC_MAX_NTN_RX_CHANNELS 1

#define IPA_NTN_TX_DIR 1
#define IPA_NTN_RX_DIR 2

/**
 *  @brief   Enum value determined based on the feature it
 *           corresponds to
 *  +----------------+----------------+
 *  |    3 bits      |     5 bits     |
 *  +----------------+----------------+
 *  |   HW_FEATURE   |     OPCODE     |
 *  +----------------+----------------+
 *
 */
#define FEATURE_ENUM_VAL(feature, opcode) ((feature << 5) | opcode)
#define EXTRACT_UC_FEATURE(value) (value >> 5)

#define IPA_HW_NUM_FEATURES 0x8

/**
 * enum ipa3_hw_features - Values that represent the features supported in IPA HW
 * @IPA_HW_FEATURE_COMMON : Feature related to common operation of IPA HW
 * @IPA_HW_FEATURE_MHI : Feature related to MHI operation in IPA HW
 * @IPA_HW_FEATURE_POWER_COLLAPSE: Feature related to IPA Power collapse
 * @IPA_HW_FEATURE_WDI : Feature related to WDI operation in IPA HW
 * @IPA_HW_FEATURE_ZIP: Feature related to CMP/DCMP operation in IPA HW
 * @IPA_HW_FEATURE_NTN : Feature related to NTN operation in IPA HW
 * @IPA_HW_FEATURE_OFFLOAD : Feature related to NTN operation in IPA HW
*/
enum ipa3_hw_features {
	IPA_HW_FEATURE_COMMON		=	0x0,
	IPA_HW_FEATURE_MHI		=	0x1,
	IPA_HW_FEATURE_POWER_COLLAPSE	=	0x2,
	IPA_HW_FEATURE_WDI		=	0x3,
	IPA_HW_FEATURE_ZIP		=	0x4,
	IPA_HW_FEATURE_NTN		=	0x5,
	IPA_HW_FEATURE_OFFLOAD	=	0x6,
	IPA_HW_FEATURE_MAX		=	IPA_HW_NUM_FEATURES
};

/**
 * enum ipa3_hw_2_cpu_events - Values that represent HW event to be sent to CPU.
 * @IPA_HW_2_CPU_EVENT_NO_OP : No event present
 * @IPA_HW_2_CPU_EVENT_ERROR : Event specify a system error is detected by the
 *  device
 * @IPA_HW_2_CPU_EVENT_LOG_INFO : Event providing logging specific information
 */
enum ipa3_hw_2_cpu_events {
	IPA_HW_2_CPU_EVENT_NO_OP     =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 0),
	IPA_HW_2_CPU_EVENT_ERROR     =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 1),
	IPA_HW_2_CPU_EVENT_LOG_INFO  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 2),
};

/**
 * enum ipa3_hw_errors - Common error types.
 * @IPA_HW_ERROR_NONE : No error persists
 * @IPA_HW_INVALID_DOORBELL_ERROR : Invalid data read from doorbell
 * @IPA_HW_DMA_ERROR : Unexpected DMA error
 * @IPA_HW_FATAL_SYSTEM_ERROR : HW has crashed and requires reset.
 * @IPA_HW_INVALID_OPCODE : Invalid opcode sent
 * @IPA_HW_INVALID_PARAMS : Invalid params for the requested command
 * @IPA_HW_GSI_CH_NOT_EMPTY_FAILURE : GSI channel emptiness validation failed
 */
enum ipa3_hw_errors {
	IPA_HW_ERROR_NONE              =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 0),
	IPA_HW_INVALID_DOORBELL_ERROR  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 1),
	IPA_HW_DMA_ERROR               =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 2),
	IPA_HW_FATAL_SYSTEM_ERROR      =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 3),
	IPA_HW_INVALID_OPCODE          =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 4),
	IPA_HW_INVALID_PARAMS        =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 5),
	IPA_HW_CONS_DISABLE_CMD_GSI_STOP_FAILURE =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 6),
	IPA_HW_PROD_DISABLE_CMD_GSI_STOP_FAILURE =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 7),
	IPA_HW_GSI_CH_NOT_EMPTY_FAILURE =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_COMMON, 8)
};

/**
 * struct IpaHwSharedMemCommonMapping_t - Structure referring to the common
 * section in 128B shared memory located in offset zero of SW Partition in IPA
 * SRAM.
 * @cmdOp : CPU->HW command opcode. See IPA_CPU_2_HW_COMMANDS
 * @cmdParams : CPU->HW command parameter lower 32bit.
 * @cmdParams_hi : CPU->HW command parameter higher 32bit.
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
	u32 cmdParams_hi;
	u8  responseOp;
	u8  reserved_0D;
	u16 reserved_0F_0E;
	u32 responseParams;
	u8  eventOp;
	u8  reserved_15;
	u16 reserved_17_16;
	u32 eventParams;
	u32 firstErrorAddress;
	u8  hwState;
	u8  warningCounter;
	u16 reserved_23_22;
	u16 interfaceVersionCommon;
	u16 reserved_27_26;
} __packed;

/**
 * union Ipa3HwFeatureInfoData_t - parameters for stats/config blob
 *
 * @offset : Location of a feature within the EventInfoData
 * @size : Size of the feature
 */
union Ipa3HwFeatureInfoData_t {
	struct IpaHwFeatureInfoParams_t {
		u32 offset:16;
		u32 size:16;
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
 * struct Ipa3HwEventInfoData_t - Structure holding the parameters for
 * statistics and config info
 *
 * @baseAddrOffset : Base Address Offset of the statistics or config
 * structure from IPA_WRAPPER_BASE
 * @Ipa3HwFeatureInfoData_t : Location and size of each feature within
 * the statistics or config structure
 *
 * @note    Information about each feature in the featureInfo[]
 * array is populated at predefined indices per the IPA_HW_FEATURES
 * enum definition
 */
struct Ipa3HwEventInfoData_t {
	u32 baseAddrOffset;
	union Ipa3HwFeatureInfoData_t featureInfo[IPA_HW_NUM_FEATURES];
} __packed;

/**
 * struct IpaHwEventLogInfoData_t - Structure holding the parameters for
 * IPA_HW_2_CPU_EVENT_LOG_INFO Event
 *
 * @featureMask : Mask indicating the features enabled in HW.
 * Refer IPA_HW_FEATURE_MASK
 * @circBuffBaseAddrOffset : Base Address Offset of the Circular Event
 * Log Buffer structure
 * @statsInfo : Statistics related information
 * @configInfo : Configuration related information
 *
 * @note    The offset location of this structure from IPA_WRAPPER_BASE
 * will be provided as Event Params for the IPA_HW_2_CPU_EVENT_LOG_INFO
 * Event
 */
struct IpaHwEventLogInfoData_t {
	u32 featureMask;
	u32 circBuffBaseAddrOffset;
	struct Ipa3HwEventInfoData_t statsInfo;
	struct Ipa3HwEventInfoData_t configInfo;

} __packed;

/**
 * struct ipa3_uc_ntn_ctx
 * @ntn_uc_stats_ofst: Neutrino stats offset
 * @ntn_uc_stats_mmio: Neutrino stats
 * @priv: private data of client
 * @uc_ready_cb: uc Ready cb
 */
struct ipa3_uc_ntn_ctx {
	u32 ntn_uc_stats_ofst;
	struct Ipa3HwStatsNTNInfoData_t *ntn_uc_stats_mmio;
	void *priv;
	ipa_uc_ready_cb uc_ready_cb;
};

/**
 * enum ipa3_hw_2_cpu_ntn_events - Values that represent HW event
 *			to be sent to CPU
 * @IPA_HW_2_CPU_EVENT_NTN_ERROR : Event to specify that HW
 *			detected an error in NTN
 *
 */
enum ipa3_hw_2_cpu_ntn_events {
	IPA_HW_2_CPU_EVENT_NTN_ERROR =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_NTN, 0),
};


/**
 * enum ipa3_hw_ntn_errors - NTN specific error types.
 * @IPA_HW_NTN_ERROR_NONE : No error persists
 * @IPA_HW_NTN_CHANNEL_ERROR : Error is specific to channel
 */
enum ipa3_hw_ntn_errors {
	IPA_HW_NTN_ERROR_NONE    = 0,
	IPA_HW_NTN_CHANNEL_ERROR = 1
};

/**
 * enum ipa3_hw_ntn_channel_states - Values that represent NTN
 * channel state machine.
 * @IPA_HW_NTN_CHANNEL_STATE_INITED_DISABLED : Channel is
 *			initialized but disabled
 * @IPA_HW_NTN_CHANNEL_STATE_RUNNING : Channel is running.
 *     Entered after SET_UP_COMMAND is processed successfully
 * @IPA_HW_NTN_CHANNEL_STATE_ERROR : Channel is in error state
 * @IPA_HW_NTN_CHANNEL_STATE_INVALID : Invalid state. Shall not
 * be in use in operational scenario
 *
 * These states apply to both Tx and Rx paths. These do not reflect the
 * sub-state the state machine may be in.
 */
enum ipa3_hw_ntn_channel_states {
	IPA_HW_NTN_CHANNEL_STATE_INITED_DISABLED = 1,
	IPA_HW_NTN_CHANNEL_STATE_RUNNING  = 2,
	IPA_HW_NTN_CHANNEL_STATE_ERROR    = 3,
	IPA_HW_NTN_CHANNEL_STATE_INVALID  = 0xFF
};

/**
 * enum ipa3_hw_ntn_channel_errors - List of NTN Channel error
 * types. This is present in the event param
 * @IPA_HW_NTN_CH_ERR_NONE: No error persists
 * @IPA_HW_NTN_TX_FSM_ERROR: Error in the state machine
 *		transition
 * @IPA_HW_NTN_TX_COMP_RE_FETCH_FAIL: Error while calculating
 *		num RE to bring
 * @IPA_HW_NTN_RX_RING_WP_UPDATE_FAIL: Write pointer update
 *		failed in Rx ring
 * @IPA_HW_NTN_RX_FSM_ERROR: Error in the state machine
 *		transition
 * @IPA_HW_NTN_RX_CACHE_NON_EMPTY:
 * @IPA_HW_NTN_CH_ERR_RESERVED:
 *
 * These states apply to both Tx and Rx paths. These do not
 * reflect the sub-state the state machine may be in.
 */
enum ipa3_hw_ntn_channel_errors {
	IPA_HW_NTN_CH_ERR_NONE            = 0,
	IPA_HW_NTN_TX_RING_WP_UPDATE_FAIL = 1,
	IPA_HW_NTN_TX_FSM_ERROR           = 2,
	IPA_HW_NTN_TX_COMP_RE_FETCH_FAIL  = 3,
	IPA_HW_NTN_RX_RING_WP_UPDATE_FAIL = 4,
	IPA_HW_NTN_RX_FSM_ERROR           = 5,
	IPA_HW_NTN_RX_CACHE_NON_EMPTY     = 6,
	IPA_HW_NTN_CH_ERR_RESERVED        = 0xFF
};


/**
 * struct Ipa3HwNtnSetUpCmdData_t  - Ntn setup command data
 * @ring_base_pa: physical address of the base of the Tx/Rx NTN
 *  ring
 * @buff_pool_base_pa: physical address of the base of the Tx/Rx
 *  buffer pool
 * @ntn_ring_size: size of the Tx/Rx NTN ring
 * @num_buffers: Rx/tx buffer pool size
 * @ntn_reg_base_ptr_pa: physical address of the Tx/Rx NTN
 *  Ring's tail pointer
 * @ipa_pipe_number: IPA pipe number that has to be used for the
 *  Tx/Rx path
 * @dir: Tx/Rx Direction
 * @data_buff_size: size of the each data buffer allocated in
 *  DDR
 */
struct Ipa3HwNtnSetUpCmdData_t {
	u32 ring_base_pa;
	u32 buff_pool_base_pa;
	u16 ntn_ring_size;
	u16 num_buffers;
	u32 ntn_reg_base_ptr_pa;
	u8  ipa_pipe_number;
	u8  dir;
	u16 data_buff_size;

} __packed;

/**
 * struct Ipa3HwNtnCommonChCmdData_t - Structure holding the
 * parameters for Ntn Tear down command data params
 *
 *@ipa_pipe_number: IPA pipe number. This could be Tx or an Rx pipe
 */
union Ipa3HwNtnCommonChCmdData_t {
	struct IpaHwNtnCommonChCmdParams_t {
		u32  ipa_pipe_number :8;
		u32  reserved        :24;
	} __packed params;
	uint32_t raw32b;
} __packed;


/**
 * struct Ipa3HwNTNErrorEventData_t - Structure holding the
 * IPA_HW_2_CPU_EVENT_NTN_ERROR event. The parameters are passed
 * as immediate params in the shared memory
 *
 *@ntn_error_type: type of NTN error (ipa3_hw_ntn_errors)
 *@ipa_pipe_number: IPA pipe number on which error has happened
 *   Applicable only if error type indicates channel error
 *@ntn_ch_err_type: Information about the channel error (if
 *		available)
 */
union Ipa3HwNTNErrorEventData_t {
	struct IpaHwNTNErrorEventParams_t {
		u32  ntn_error_type  :8;
		u32  reserved        :8;
		u32  ipa_pipe_number :8;
		u32  ntn_ch_err_type :8;
	} __packed params;
	uint32_t raw32b;
} __packed;

/**
 * struct NTN3RxInfoData_t - NTN Structure holding the Rx pipe
 * information
 *
 *@max_outstanding_pkts: Number of outstanding packets in Rx
 *		Ring
 *@num_pkts_processed: Number of packets processed - cumulative
 *@rx_ring_rp_value: Read pointer last advertized to the WLAN FW
 *
 *@ntn_ch_err_type: Information about the channel error (if
 *		available)
 *@rx_ind_ring_stats:
 *@bam_stats:
 *@num_bam_int_handled: Number of Bam Interrupts handled by FW
 *@num_db: Number of times the doorbell was rung
 *@num_unexpected_db: Number of unexpected doorbells
 *@num_pkts_in_dis_uninit_state:
 *@num_bam_int_handled_while_not_in_bam: Number of Bam
 *		Interrupts handled by FW
 *@num_bam_int_handled_while_in_bam_state: Number of Bam
 *   Interrupts handled by FW
 */
struct NTN3RxInfoData_t {
	u32  max_outstanding_pkts;
	u32  num_pkts_processed;
	u32  rx_ring_rp_value;
	struct IpaHwRingStats_t rx_ind_ring_stats;
	struct IpaHwBamStats_t bam_stats;
	u32  num_bam_int_handled;
	u32  num_db;
	u32  num_unexpected_db;
	u32  num_pkts_in_dis_uninit_state;
	u32  num_bam_int_handled_while_not_in_bam;
	u32  num_bam_int_handled_while_in_bam_state;
} __packed;


/**
 * struct NTNTxInfoData_t - Structure holding the NTN Tx channel
 * Ensure that this is always word aligned
 *
 *@num_pkts_processed: Number of packets processed - cumulative
 *@tail_ptr_val: Latest value of doorbell written to copy engine
 *@num_db_fired: Number of DB from uC FW to Copy engine
 *
 *@tx_comp_ring_stats:
 *@bam_stats:
 *@num_db: Number of times the doorbell was rung
 *@num_unexpected_db: Number of unexpected doorbells
 *@num_bam_int_handled: Number of Bam Interrupts handled by FW
 *@num_bam_int_in_non_running_state: Number of Bam interrupts
 *			while not in Running state
 *@num_qmb_int_handled: Number of QMB interrupts handled
 *@num_bam_int_handled_while_wait_for_bam: Number of times the
 *		Imm Cmd is injected due to fw_desc change
 */
struct NTNTxInfoData_t {
	u32  num_pkts_processed;
	u32  tail_ptr_val;
	u32  num_db_fired;
	struct IpaHwRingStats_t tx_comp_ring_stats;
	struct IpaHwBamStats_t bam_stats;
	u32  num_db;
	u32  num_unexpected_db;
	u32  num_bam_int_handled;
	u32  num_bam_int_in_non_running_state;
	u32  num_qmb_int_handled;
	u32  num_bam_int_handled_while_wait_for_bam;
	u32  num_bam_int_handled_while_not_in_bam;
} __packed;


/**
 * struct Ipa3HwStatsNTNInfoData_t - Structure holding the NTN Tx
 * channel Ensure that this is always word aligned
 *
 */
struct Ipa3HwStatsNTNInfoData_t {
	struct NTN3RxInfoData_t rx_ch_stats[IPA_UC_MAX_NTN_RX_CHANNELS];
	struct NTNTxInfoData_t tx_ch_stats[IPA_UC_MAX_NTN_TX_CHANNELS];
} __packed;


/*
 * uC offload related data structures
 */
#define IPA_UC_OFFLOAD_CONNECTED BIT(0)
#define IPA_UC_OFFLOAD_ENABLED BIT(1)
#define IPA_UC_OFFLOAD_RESUMED BIT(2)

/**
 * enum ipa_cpu_2_hw_offload_commands -  Values that represent
 * the offload commands from CPU
 * @IPA_CPU_2_HW_CMD_OFFLOAD_CHANNEL_SET_UP : Command to set up
 *				Offload protocol's Tx/Rx Path
 * @IPA_CPU_2_HW_CMD_OFFLOAD_RX_SET_UP : Command to tear down
 *				Offload protocol's Tx/ Rx Path
 */
enum ipa_cpu_2_hw_offload_commands {
	IPA_CPU_2_HW_CMD_OFFLOAD_CHANNEL_SET_UP  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 1),
	IPA_CPU_2_HW_CMD_OFFLOAD_TEAR_DOWN,
};


/**
 * enum ipa3_hw_offload_channel_states - Values that represent
 * offload channel state machine.
 * @IPA_HW_OFFLOAD_CHANNEL_STATE_INITED_DISABLED : Channel is initialized but disabled
 * @IPA_HW_OFFLOAD_CHANNEL_STATE_RUNNING : Channel is running. Entered after SET_UP_COMMAND is processed successfully
 * @IPA_HW_OFFLOAD_CHANNEL_STATE_ERROR : Channel is in error state
 * @IPA_HW_OFFLOAD_CHANNEL_STATE_INVALID : Invalid state. Shall not be in use in operational scenario
 *
 * These states apply to both Tx and Rx paths. These do not
 * reflect the sub-state the state machine may be in
 */
enum ipa3_hw_offload_channel_states {
	IPA_HW_OFFLOAD_CHANNEL_STATE_INITED_DISABLED = 1,
	IPA_HW_OFFLOAD_CHANNEL_STATE_RUNNING  = 2,
	IPA_HW_OFFLOAD_CHANNEL_STATE_ERROR    = 3,
	IPA_HW_OFFLOAD_CHANNEL_STATE_INVALID  = 0xFF
};


/**
 * enum ipa3_hw_2_cpu_cmd_resp_status -  Values that represent
 * offload related command response status to be sent to CPU.
 */
enum ipa3_hw_2_cpu_offload_cmd_resp_status {
	IPA_HW_2_CPU_OFFLOAD_CMD_STATUS_SUCCESS  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 0),
	IPA_HW_2_CPU_OFFLOAD_MAX_TX_CHANNELS  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 1),
	IPA_HW_2_CPU_OFFLOAD_TX_RING_OVERRUN_POSSIBILITY  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 2),
	IPA_HW_2_CPU_OFFLOAD_TX_RING_SET_UP_FAILURE  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 3),
	IPA_HW_2_CPU_OFFLOAD_TX_RING_PARAMS_UNALIGNED  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 4),
	IPA_HW_2_CPU_OFFLOAD_UNKNOWN_TX_CHANNEL  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 5),
	IPA_HW_2_CPU_OFFLOAD_TX_INVALID_FSM_TRANSITION  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 6),
	IPA_HW_2_CPU_OFFLOAD_TX_FSM_TRANSITION_ERROR  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 7),
	IPA_HW_2_CPU_OFFLOAD_MAX_RX_CHANNELS  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 8),
	IPA_HW_2_CPU_OFFLOAD_RX_RING_PARAMS_UNALIGNED  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 9),
	IPA_HW_2_CPU_OFFLOAD_RX_RING_SET_UP_FAILURE  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 10),
	IPA_HW_2_CPU_OFFLOAD_UNKNOWN_RX_CHANNEL  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 11),
	IPA_HW_2_CPU_OFFLOAD_RX_INVALID_FSM_TRANSITION  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 12),
	IPA_HW_2_CPU_OFFLOAD_RX_FSM_TRANSITION_ERROR  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 13),
	IPA_HW_2_CPU_OFFLOAD_RX_RING_OVERRUN_POSSIBILITY  =
		FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, 14),
};

/**
 * struct IpaHwSetUpCmd  -
 *
 *
 */
union IpaHwSetUpCmd {
	struct Ipa3HwNtnSetUpCmdData_t NtnSetupCh_params;
} __packed;

/**
 * struct IpaHwOffloadSetUpCmdData_t  -
 *
 *
 */
struct IpaHwOffloadSetUpCmdData_t {
	u8 protocol;
	union IpaHwSetUpCmd SetupCh_params;
} __packed;

/**
 * struct IpaHwCommonChCmd  - Structure holding the parameters
 * for IPA_CPU_2_HW_CMD_OFFLOAD_TEAR_DOWN
 *
 *
 */
union IpaHwCommonChCmd {
	union Ipa3HwNtnCommonChCmdData_t NtnCommonCh_params;
} __packed;

struct IpaHwOffloadCommonChCmdData_t {
	u8 protocol;
	union IpaHwCommonChCmd CommonCh_params;
} __packed;

#endif /* _IPA_UC_OFFLOAD_I_H_ */
