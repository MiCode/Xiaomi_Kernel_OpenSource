/* Copyright (c) 2015, 2016 The Linux Foundation. All rights reserved.
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

#include <linux/ipa.h>
#include "ipa_i.h"

/* MHI uC interface definitions */
#define IPA_HW_INTERFACE_MHI_VERSION            0x0004

#define IPA_HW_MAX_NUMBER_OF_CHANNELS	2
#define IPA_HW_MAX_NUMBER_OF_EVENTRINGS	2
#define IPA_HW_MAX_CHANNEL_HANDLE	(IPA_HW_MAX_NUMBER_OF_CHANNELS-1)

/**
 * Values that represent the MHI commands from CPU to IPA HW.
 * @IPA_CPU_2_HW_CMD_MHI_INIT: Initialize HW to be ready for MHI processing.
 *	Once operation was completed HW shall respond with
 *	IPA_HW_2_CPU_RESPONSE_CMD_COMPLETED.
 * @IPA_CPU_2_HW_CMD_MHI_INIT_CHANNEL: Initialize specific channel to be ready
 *	to serve MHI transfers. Once initialization was completed HW shall
 *	respond with IPA_HW_2_CPU_RESPONSE_MHI_CHANGE_CHANNEL_STATE.
 *		IPA_HW_MHI_CHANNEL_STATE_ENABLE
 * @IPA_CPU_2_HW_CMD_MHI_UPDATE_MSI: Update MHI MSI interrupts data.
 *	Once operation was completed HW shall respond with
 *	IPA_HW_2_CPU_RESPONSE_CMD_COMPLETED.
 * @IPA_CPU_2_HW_CMD_MHI_CHANGE_CHANNEL_STATE: Change specific channel
 *	processing state following host request. Once operation was completed
 *	HW shall respond with IPA_HW_2_CPU_RESPONSE_MHI_CHANGE_CHANNEL_STATE.
 * @IPA_CPU_2_HW_CMD_MHI_DL_UL_SYNC_INFO: Info related to DL UL syncronization.
 * @IPA_CPU_2_HW_CMD_MHI_STOP_EVENT_UPDATE: Cmd to stop event ring processing.
 */
enum ipa_cpu_2_hw_mhi_commands {
	IPA_CPU_2_HW_CMD_MHI_INIT
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 0),
	IPA_CPU_2_HW_CMD_MHI_INIT_CHANNEL
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 1),
	IPA_CPU_2_HW_CMD_MHI_UPDATE_MSI
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 2),
	IPA_CPU_2_HW_CMD_MHI_CHANGE_CHANNEL_STATE
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 3),
	IPA_CPU_2_HW_CMD_MHI_DL_UL_SYNC_INFO
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 4),
	IPA_CPU_2_HW_CMD_MHI_STOP_EVENT_UPDATE
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 5)
};

/**
 * Values that represent MHI related HW responses to CPU commands.
 * @IPA_HW_2_CPU_RESPONSE_MHI_CHANGE_CHANNEL_STATE: Response to
 *	IPA_CPU_2_HW_CMD_MHI_INIT_CHANNEL or
 *	IPA_CPU_2_HW_CMD_MHI_CHANGE_CHANNEL_STATE commands.
 */
enum ipa_hw_2_cpu_mhi_responses {
	IPA_HW_2_CPU_RESPONSE_MHI_CHANGE_CHANNEL_STATE
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 0),
};

/**
 * Values that represent MHI related HW event to be sent to CPU.
 * @IPA_HW_2_CPU_EVENT_MHI_CHANNEL_ERROR: Event specify the device detected an
 *	error in an element from the transfer ring associated with the channel
 * @IPA_HW_2_CPU_EVENT_MHI_CHANNEL_WAKE_UP_REQUEST: Event specify a bam
 *	interrupt was asserted when MHI engine is suspended
 */
enum ipa_hw_2_cpu_mhi_events {
	IPA_HW_2_CPU_EVENT_MHI_CHANNEL_ERROR
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 0),
	IPA_HW_2_CPU_EVENT_MHI_CHANNEL_WAKE_UP_REQUEST
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 1),
};

/**
 * Channel error types.
 * @IPA_HW_CHANNEL_ERROR_NONE: No error persists.
 * @IPA_HW_CHANNEL_INVALID_RE_ERROR: Invalid Ring Element was detected
 */
enum ipa_hw_channel_errors {
	IPA_HW_CHANNEL_ERROR_NONE,
	IPA_HW_CHANNEL_INVALID_RE_ERROR
};

/**
 * MHI error types.
 * @IPA_HW_INVALID_MMIO_ERROR: Invalid data read from MMIO space
 * @IPA_HW_INVALID_CHANNEL_ERROR: Invalid data read from channel context array
 * @IPA_HW_INVALID_EVENT_ERROR: Invalid data read from event ring context array
 * @IPA_HW_NO_ED_IN_RING_ERROR: No event descriptors are available to report on
 *	secondary event ring
 * @IPA_HW_LINK_ERROR: Link error
 */
enum ipa_hw_mhi_errors {
	IPA_HW_INVALID_MMIO_ERROR
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 0),
	IPA_HW_INVALID_CHANNEL_ERROR
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 1),
	IPA_HW_INVALID_EVENT_ERROR
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 2),
	IPA_HW_NO_ED_IN_RING_ERROR
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 4),
	IPA_HW_LINK_ERROR
		= FEATURE_ENUM_VAL(IPA_HW_FEATURE_MHI, 5),
};


/**
 * Structure referring to the common and MHI section of 128B shared memory
 * located in offset zero of SW Partition in IPA SRAM.
 * The shared memory is used for communication between IPA HW and CPU.
 * @common: common section in IPA SRAM
 * @interfaceVersionMhi: The MHI interface version as reported by HW
 * @mhiState: Overall MHI state
 * @reserved_2B: reserved
 * @mhiCnl0State: State of MHI channel 0.
 *	The state carries information regarding the error type.
 *	See IPA_HW_MHI_CHANNEL_STATES.
 * @mhiCnl0State: State of MHI channel 1.
 * @mhiCnl0State: State of MHI channel 2.
 * @mhiCnl0State: State of MHI channel 3
 * @mhiCnl0State: State of MHI channel 4.
 * @mhiCnl0State: State of MHI channel 5.
 * @mhiCnl0State: State of MHI channel 6.
 * @mhiCnl0State: State of MHI channel 7.
 * @reserved_37_34: reserved
 * @reserved_3B_38: reserved
 * @reserved_3F_3C: reserved
 */
struct IpaHwSharedMemMhiMapping_t {
	struct IpaHwSharedMemCommonMapping_t common;
	u16 interfaceVersionMhi;
	u8 mhiState;
	u8 reserved_2B;
	u8 mhiCnl0State;
	u8 mhiCnl1State;
	u8 mhiCnl2State;
	u8 mhiCnl3State;
	u8 mhiCnl4State;
	u8 mhiCnl5State;
	u8 mhiCnl6State;
	u8 mhiCnl7State;
	u32 reserved_37_34;
	u32 reserved_3B_38;
	u32 reserved_3F_3C;
};


/**
 * Structure holding the parameters for IPA_CPU_2_HW_CMD_MHI_INIT command.
 * Parameters are sent as pointer thus should be reside in address accessible
 * to HW.
 * @msiAddress: The MSI base (in device space) used for asserting the interrupt
 *	(MSI) associated with the event ring
 * mmioBaseAddress: The address (in device space) of MMIO structure in
 *	host space
 * deviceMhiCtrlBaseAddress: Base address of the memory region in the device
 *	address space where the MHI control data structures are allocated by
 *	the host, including channel context array, event context array,
 *	and rings. This value is used for host/device address translation.
 * deviceMhiDataBaseAddress: Base address of the memory region in the device
 *	address space where the MHI data buffers are allocated by the host.
 *	This value is used for host/device address translation.
 * firstChannelIndex: First channel ID. Doorbell 0 is mapped to this channel
 * firstEventRingIndex: First event ring ID. Doorbell 16 is mapped to this
 *	event ring.
 */
struct IpaHwMhiInitCmdData_t {
	u32 msiAddress;
	u32 mmioBaseAddress;
	u32 deviceMhiCtrlBaseAddress;
	u32 deviceMhiDataBaseAddress;
	u32 firstChannelIndex;
	u32 firstEventRingIndex;
};

/**
 * Structure holding the parameters for IPA_CPU_2_HW_CMD_MHI_INIT_CHANNEL
 *	command. Parameters are sent as 32b immediate parameters.
 * @hannelHandle: The channel identifier as allocated by driver.
 *	value is within the range 0 to IPA_HW_MAX_CHANNEL_HANDLE
 * @contexArrayIndex: Unique index for channels, between 0 and 255. The index is
 *	used as an index in channel context array structures.
 * @bamPipeId: The BAM pipe number for pipe dedicated for this channel
 * @channelDirection: The direction of the channel as defined in the channel
 *	type field (CHTYPE) in the channel context data structure.
 * @reserved: reserved.
 */
union IpaHwMhiInitChannelCmdData_t {
	struct IpaHwMhiInitChannelCmdParams_t {
		u32 channelHandle:8;
		u32 contexArrayIndex:8;
		u32 bamPipeId:6;
		u32 channelDirection:2;
		u32 reserved:8;
	} params;
	u32 raw32b;
};

/**
 * Structure holding the parameters for IPA_CPU_2_HW_CMD_MHI_UPDATE_MSI command.
 * @msiAddress_low: The MSI lower base addr (in device space) used for asserting
 *	the interrupt (MSI) associated with the event ring.
 * @msiAddress_hi: The MSI higher base addr (in device space) used for asserting
 *	the interrupt (MSI) associated with the event ring.
 * @msiMask: Mask indicating number of messages assigned by the host to device
 * @msiData: Data Pattern to use when generating the MSI
 */
struct IpaHwMhiMsiCmdData_t {
	u32 msiAddress_low;
	u32 msiAddress_hi;
	u32 msiMask;
	u32 msiData;
};

/**
 * Structure holding the parameters for
 * IPA_CPU_2_HW_CMD_MHI_CHANGE_CHANNEL_STATE command.
 * Parameters are sent as 32b immediate parameters.
 * @requestedState: The requested channel state as was indicated from Host.
 *	Use IPA_HW_MHI_CHANNEL_STATES to specify the requested state
 * @channelHandle: The channel identifier as allocated by driver.
 *	value is within the range 0 to IPA_HW_MAX_CHANNEL_HANDLE
 * @LPTransitionRejected: Indication that low power state transition was
 *	rejected
 * @reserved: reserved
 */
union IpaHwMhiChangeChannelStateCmdData_t {
	struct IpaHwMhiChangeChannelStateCmdParams_t {
		u32 requestedState:8;
		u32 channelHandle:8;
		u32 LPTransitionRejected:8;
		u32 reserved:8;
	} params;
	u32 raw32b;
};

/**
 * Structure holding the parameters for
 *	IPA_CPU_2_HW_CMD_MHI_STOP_EVENT_UPDATE command.
 * Parameters are sent as 32b immediate parameters.
 * @channelHandle: The channel identifier as allocated by driver.
 *	value is within the range 0 to IPA_HW_MAX_CHANNEL_HANDLE
 * @reserved: reserved
 */
union IpaHwMhiStopEventUpdateData_t {
	struct IpaHwMhiStopEventUpdateDataParams_t {
		u32 channelHandle:8;
		u32 reserved:24;
	} params;
	u32 raw32b;
};

/**
 * Structure holding the parameters for
 *	IPA_HW_2_CPU_RESPONSE_MHI_CHANGE_CHANNEL_STATE response.
 * Parameters are sent as 32b immediate parameters.
 * @state: The new channel state. In case state is not as requested this is
 *	error indication for the last command
 * @channelHandle: The channel identifier
 * @additonalParams: For stop: the number of pending bam descriptors currently
 *	queued
*/
union IpaHwMhiChangeChannelStateResponseData_t {
	struct IpaHwMhiChangeChannelStateResponseParams_t {
		u32 state:8;
		u32 channelHandle:8;
		u32 additonalParams:16;
	} params;
	u32 raw32b;
};

/**
 * Structure holding the parameters for
 *	IPA_HW_2_CPU_EVENT_MHI_CHANNEL_ERROR event.
 * Parameters are sent as 32b immediate parameters.
 * @errorType: Type of error - IPA_HW_CHANNEL_ERRORS
 * @channelHandle: The channel identifier as allocated by driver.
 *	value is within the range 0 to IPA_HW_MAX_CHANNEL_HANDLE
 * @reserved: reserved
 */
union IpaHwMhiChannelErrorEventData_t {
	struct IpaHwMhiChannelErrorEventParams_t {
		u32 errorType:8;
		u32 channelHandle:8;
		u32 reserved:16;
	} params;
	u32 raw32b;
};

/**
 * Structure holding the parameters for
 *	IPA_HW_2_CPU_EVENT_MHI_CHANNEL_WAKE_UP_REQUEST event.
 * Parameters are sent as 32b immediate parameters.
 * @channelHandle: The channel identifier as allocated by driver.
 *	value is within the range 0 to IPA_HW_MAX_CHANNEL_HANDLE
 * @reserved: reserved
 */
union IpaHwMhiChannelWakeupEventData_t {
	struct IpaHwMhiChannelWakeupEventParams_t {
		u32 channelHandle:8;
		u32 reserved:24;
	} params;
	u32 raw32b;
};

/**
 * Structure holding the MHI Common statistics
 * @numULDLSync: Number of times UL activity trigged due to DL activity
 * @numULTimerExpired: Number of times UL Accm Timer expired
 */
struct IpaHwStatsMhiCmnInfoData_t {
	u32 numULDLSync;
	u32 numULTimerExpired;
	u32 numChEvCtxWpRead;
	u32 reserved;
};

/**
 * Structure holding the MHI Channel statistics
 * @doorbellInt: The number of doorbell int
 * @reProccesed: The number of ring elements processed
 * @bamFifoFull: Number of times Bam Fifo got full
 * @bamFifoEmpty: Number of times Bam Fifo got empty
 * @bamFifoUsageHigh: Number of times Bam fifo usage went above 75%
 * @bamFifoUsageLow: Number of times Bam fifo usage went below 25%
 * @bamInt: Number of BAM Interrupts
 * @ringFull: Number of times Transfer Ring got full
 * @ringEmpty: umber of times Transfer Ring got empty
 * @ringUsageHigh: Number of times Transfer Ring usage went above 75%
 * @ringUsageLow: Number of times Transfer Ring usage went below 25%
 * @delayedMsi: Number of times device triggered MSI to host after
 *	Interrupt Moderation Timer expiry
 * @immediateMsi: Number of times device triggered MSI to host immediately
 * @thresholdMsi: Number of times device triggered MSI due to max pending
 *	events threshold reached
 * @numSuspend: Number of times channel was suspended
 * @numResume: Number of times channel was suspended
 * @num_OOB: Number of times we indicated that we are OOB
 * @num_OOB_timer_expiry: Number of times we indicated that we are OOB
 *	after timer expiry
 * @num_OOB_moderation_timer_start: Number of times we started timer after
 *	sending OOB and hitting OOB again before we processed threshold
 *	number of packets
 * @num_db_mode_evt: Number of times we indicated that we are in Doorbell mode
 */
struct IpaHwStatsMhiCnlInfoData_t {
	u32 doorbellInt;
	u32 reProccesed;
	u32 bamFifoFull;
	u32 bamFifoEmpty;
	u32 bamFifoUsageHigh;
	u32 bamFifoUsageLow;
	u32 bamInt;
	u32 ringFull;
	u32 ringEmpty;
	u32 ringUsageHigh;
	u32 ringUsageLow;
	u32 delayedMsi;
	u32 immediateMsi;
	u32 thresholdMsi;
	u32 numSuspend;
	u32 numResume;
	u32 num_OOB;
	u32 num_OOB_timer_expiry;
	u32 num_OOB_moderation_timer_start;
	u32 num_db_mode_evt;
};

/**
 * Structure holding the MHI statistics
 * @mhiCmnStats: Stats pertaining to MHI
 * @mhiCnlStats: Stats pertaining to each channel
 */
struct IpaHwStatsMhiInfoData_t {
	struct IpaHwStatsMhiCmnInfoData_t mhiCmnStats;
	struct IpaHwStatsMhiCnlInfoData_t mhiCnlStats[
						IPA_HW_MAX_NUMBER_OF_CHANNELS];
};

/**
 * Structure holding the MHI Common Config info
 * @isDlUlSyncEnabled: Flag to indicate if DL-UL synchronization is enabled
 * @UlAccmVal: Out Channel(UL) accumulation time in ms when DL UL Sync is
 *	enabled
 * @ulMsiEventThreshold: Threshold at which HW fires MSI to host for UL events
 * @dlMsiEventThreshold: Threshold at which HW fires MSI to host for DL events
 */
struct IpaHwConfigMhiCmnInfoData_t {
	u8 isDlUlSyncEnabled;
	u8 UlAccmVal;
	u8 ulMsiEventThreshold;
	u8 dlMsiEventThreshold;
};

/**
 * Structure holding the parameters for MSI info data
 * @msiAddress_low: The MSI lower base addr (in device space) used for asserting
 *	the interrupt (MSI) associated with the event ring.
 * @msiAddress_hi: The MSI higher base addr (in device space) used for asserting
 *	the interrupt (MSI) associated with the event ring.
 * @msiMask: Mask indicating number of messages assigned by the host to device
 * @msiData: Data Pattern to use when generating the MSI
 */
struct IpaHwConfigMhiMsiInfoData_t {
	u32 msiAddress_low;
	u32 msiAddress_hi;
	u32 msiMask;
	u32 msiData;
};

/**
 * Structure holding the MHI Channel Config info
 * @transferRingSize: The Transfer Ring size in terms of Ring Elements
 * @transferRingIndex: The Transfer Ring channel number as defined by host
 * @eventRingIndex: The Event Ring Index associated with this Transfer Ring
 * @bamPipeIndex: The BAM Pipe associated with this channel
 * @isOutChannel: Indication for the direction of channel
 * @reserved_0: Reserved byte for maintaining 4byte alignment
 * @reserved_1: Reserved byte for maintaining 4byte alignment
 */
struct IpaHwConfigMhiCnlInfoData_t {
	u16 transferRingSize;
	u8  transferRingIndex;
	u8  eventRingIndex;
	u8  bamPipeIndex;
	u8  isOutChannel;
	u8  reserved_0;
	u8  reserved_1;
};

/**
 * Structure holding the MHI Event Config info
 * @msiVec: msi vector to invoke MSI interrupt
 * @intmodtValue: Interrupt moderation timer (in milliseconds)
 * @eventRingSize: The Event Ring size in terms of Ring Elements
 * @eventRingIndex: The Event Ring number as defined by host
 * @reserved_0: Reserved byte for maintaining 4byte alignment
 * @reserved_1: Reserved byte for maintaining 4byte alignment
 * @reserved_2: Reserved byte for maintaining 4byte alignment
 */
struct IpaHwConfigMhiEventInfoData_t {
	u32 msiVec;
	u16 intmodtValue;
	u16 eventRingSize;
	u8  eventRingIndex;
	u8  reserved_0;
	u8  reserved_1;
	u8  reserved_2;
};

/**
 * Structure holding the MHI Config info
 * @mhiCmnCfg: Common Config pertaining to MHI
 * @mhiMsiCfg: Config pertaining to MSI config
 * @mhiCnlCfg: Config pertaining to each channel
 * @mhiEvtCfg: Config pertaining to each event Ring
 */
struct IpaHwConfigMhiInfoData_t {
	struct IpaHwConfigMhiCmnInfoData_t mhiCmnCfg;
	struct IpaHwConfigMhiMsiInfoData_t mhiMsiCfg;
	struct IpaHwConfigMhiCnlInfoData_t mhiCnlCfg[
						IPA_HW_MAX_NUMBER_OF_CHANNELS];
	struct IpaHwConfigMhiEventInfoData_t mhiEvtCfg[
					IPA_HW_MAX_NUMBER_OF_EVENTRINGS];
};


struct ipa3_uc_mhi_ctx {
	u8 expected_responseOp;
	u32 expected_responseParams;
	void (*ready_cb)(void);
	void (*wakeup_request_cb)(void);
	u32 mhi_uc_stats_ofst;
	struct IpaHwStatsMhiInfoData_t *mhi_uc_stats_mmio;
};

#define PRINT_COMMON_STATS(x) \
	(nBytes += scnprintf(&dbg_buff[nBytes], size - nBytes, \
	#x "=0x%x\n", ipa3_uc_mhi_ctx->mhi_uc_stats_mmio->mhiCmnStats.x))

#define PRINT_CHANNEL_STATS(ch, x) \
	(nBytes += scnprintf(&dbg_buff[nBytes], size - nBytes, \
	#x "=0x%x\n", ipa3_uc_mhi_ctx->mhi_uc_stats_mmio->mhiCnlStats[ch].x))

struct ipa3_uc_mhi_ctx *ipa3_uc_mhi_ctx;

static int ipa3_uc_mhi_response_hdlr(struct IpaHwSharedMemCommonMapping_t
	*uc_sram_mmio, u32 *uc_status)
{
	IPADBG("responseOp=%d\n", uc_sram_mmio->responseOp);
	if (uc_sram_mmio->responseOp == ipa3_uc_mhi_ctx->expected_responseOp &&
	    uc_sram_mmio->responseParams ==
	    ipa3_uc_mhi_ctx->expected_responseParams) {
		*uc_status = 0;
		return 0;
	}
	return -EINVAL;
}

static void ipa3_uc_mhi_event_hdlr(struct IpaHwSharedMemCommonMapping_t
	*uc_sram_mmio)
{
	if (ipa3_ctx->uc_ctx.uc_sram_mmio->eventOp ==
	    IPA_HW_2_CPU_EVENT_MHI_CHANNEL_ERROR) {
		union IpaHwMhiChannelErrorEventData_t evt;

		IPAERR("Channel error\n");
		evt.raw32b = uc_sram_mmio->eventParams;
		IPAERR("errorType=%d channelHandle=%d reserved=%d\n",
			evt.params.errorType, evt.params.channelHandle,
			evt.params.reserved);
	} else if (ipa3_ctx->uc_ctx.uc_sram_mmio->eventOp ==
		   IPA_HW_2_CPU_EVENT_MHI_CHANNEL_WAKE_UP_REQUEST) {
		union IpaHwMhiChannelWakeupEventData_t evt;

		IPADBG("WakeUp channel request\n");
		evt.raw32b = uc_sram_mmio->eventParams;
		IPADBG("channelHandle=%d reserved=%d\n",
			evt.params.channelHandle, evt.params.reserved);
		ipa3_uc_mhi_ctx->wakeup_request_cb();
	}
}

static void ipa3_uc_mhi_event_log_info_hdlr(
	struct IpaHwEventLogInfoData_t *uc_event_top_mmio)

{
	if ((uc_event_top_mmio->featureMask & (1 << IPA_HW_FEATURE_MHI)) == 0) {
		IPAERR("MHI feature missing 0x%x\n",
			uc_event_top_mmio->featureMask);
		return;
	}

	if (uc_event_top_mmio->statsInfo.featureInfo[IPA_HW_FEATURE_MHI].
		params.size != sizeof(struct IpaHwStatsMhiInfoData_t)) {
		IPAERR("mhi stats sz invalid exp=%zu is=%u\n",
			sizeof(struct IpaHwStatsMhiInfoData_t),
			uc_event_top_mmio->statsInfo.
			featureInfo[IPA_HW_FEATURE_MHI].params.size);
		return;
	}

	ipa3_uc_mhi_ctx->mhi_uc_stats_ofst = uc_event_top_mmio->
		statsInfo.baseAddrOffset + uc_event_top_mmio->statsInfo.
		featureInfo[IPA_HW_FEATURE_MHI].params.offset;
	IPAERR("MHI stats ofst=0x%x\n", ipa3_uc_mhi_ctx->mhi_uc_stats_ofst);
	if (ipa3_uc_mhi_ctx->mhi_uc_stats_ofst +
		sizeof(struct IpaHwStatsMhiInfoData_t) >=
		ipa3_ctx->ctrl->ipa_reg_base_ofst +
		ipahal_get_reg_n_ofst(IPA_SRAM_DIRECT_ACCESS_n, 0) +
		ipa3_ctx->smem_sz) {
		IPAERR("uc_mhi_stats 0x%x outside SRAM\n",
			ipa3_uc_mhi_ctx->mhi_uc_stats_ofst);
		return;
	}

	ipa3_uc_mhi_ctx->mhi_uc_stats_mmio =
		ioremap(ipa3_ctx->ipa_wrapper_base +
		ipa3_uc_mhi_ctx->mhi_uc_stats_ofst,
		sizeof(struct IpaHwStatsMhiInfoData_t));
	if (!ipa3_uc_mhi_ctx->mhi_uc_stats_mmio) {
		IPAERR("fail to ioremap uc mhi stats\n");
		return;
	}
}

int ipa3_uc_mhi_init(void (*ready_cb)(void), void (*wakeup_request_cb)(void))
{
	struct ipa3_uc_hdlrs hdlrs;

	if (ipa3_uc_mhi_ctx) {
		IPAERR("Already initialized\n");
		return -EFAULT;
	}

	ipa3_uc_mhi_ctx = kzalloc(sizeof(*ipa3_uc_mhi_ctx), GFP_KERNEL);
	if (!ipa3_uc_mhi_ctx) {
		IPAERR("no mem\n");
		return -ENOMEM;
	}

	ipa3_uc_mhi_ctx->ready_cb = ready_cb;
	ipa3_uc_mhi_ctx->wakeup_request_cb = wakeup_request_cb;

	memset(&hdlrs, 0, sizeof(hdlrs));
	hdlrs.ipa_uc_loaded_hdlr = ipa3_uc_mhi_ctx->ready_cb;
	hdlrs.ipa3_uc_response_hdlr = ipa3_uc_mhi_response_hdlr;
	hdlrs.ipa_uc_event_hdlr = ipa3_uc_mhi_event_hdlr;
	hdlrs.ipa_uc_event_log_info_hdlr = ipa3_uc_mhi_event_log_info_hdlr;
	ipa3_uc_register_handlers(IPA_HW_FEATURE_MHI, &hdlrs);

	IPADBG("Done\n");
	return 0;
}

void ipa3_uc_mhi_cleanup(void)
{
	struct ipa3_uc_hdlrs null_hdlrs = { 0 };

	IPADBG("Enter\n");

	if (!ipa3_uc_mhi_ctx) {
		IPAERR("ipa3_uc_mhi_ctx is not initialized\n");
		return;
	}
	ipa3_uc_register_handlers(IPA_HW_FEATURE_MHI, &null_hdlrs);
	kfree(ipa3_uc_mhi_ctx);
	ipa3_uc_mhi_ctx = NULL;

	IPADBG("Done\n");
}

int ipa3_uc_mhi_init_engine(struct ipa_mhi_msi_info *msi, u32 mmio_addr,
	u32 host_ctrl_addr, u32 host_data_addr, u32 first_ch_idx,
	u32 first_evt_idx)
{
	int res;
	struct ipa3_mem_buffer mem;
	struct IpaHwMhiInitCmdData_t *init_cmd_data;
	struct IpaHwMhiMsiCmdData_t *msi_cmd;

	if (!ipa3_uc_mhi_ctx) {
		IPAERR("Not initialized\n");
		return -EFAULT;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	res = ipa3_uc_update_hw_flags(0);
	if (res) {
		IPAERR("ipa3_uc_update_hw_flags failed %d\n", res);
		goto disable_clks;
	}

	mem.size = sizeof(*init_cmd_data);
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size, &mem.phys_base,
		GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		res = -ENOMEM;
		goto disable_clks;
	}
	memset(mem.base, 0, mem.size);
	init_cmd_data = (struct IpaHwMhiInitCmdData_t *)mem.base;
	init_cmd_data->msiAddress = msi->addr_low;
	init_cmd_data->mmioBaseAddress = mmio_addr;
	init_cmd_data->deviceMhiCtrlBaseAddress = host_ctrl_addr;
	init_cmd_data->deviceMhiDataBaseAddress = host_data_addr;
	init_cmd_data->firstChannelIndex = first_ch_idx;
	init_cmd_data->firstEventRingIndex = first_evt_idx;
	res = ipa3_uc_send_cmd((u32)mem.phys_base, IPA_CPU_2_HW_CMD_MHI_INIT, 0,
		false, HZ);
	if (res) {
		IPAERR("ipa3_uc_send_cmd failed %d\n", res);
		dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base,
			mem.phys_base);
		goto disable_clks;
	}

	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);

	mem.size = sizeof(*msi_cmd);
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size, &mem.phys_base,
		GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		res = -ENOMEM;
		goto disable_clks;
	}

	msi_cmd = (struct IpaHwMhiMsiCmdData_t *)mem.base;
	msi_cmd->msiAddress_hi = msi->addr_hi;
	msi_cmd->msiAddress_low = msi->addr_low;
	msi_cmd->msiData = msi->data;
	msi_cmd->msiMask = msi->mask;
	res = ipa3_uc_send_cmd((u32)mem.phys_base,
		IPA_CPU_2_HW_CMD_MHI_UPDATE_MSI, 0, false, HZ);
	if (res) {
		IPAERR("ipa3_uc_send_cmd failed %d\n", res);
		dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base,
			mem.phys_base);
		goto disable_clks;
	}

	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);

	res = 0;

disable_clks:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return res;

}

int ipa3_uc_mhi_init_channel(int ipa_ep_idx, int channelHandle,
	int contexArrayIndex, int channelDirection)

{
	int res;
	union IpaHwMhiInitChannelCmdData_t init_cmd;
	union IpaHwMhiChangeChannelStateResponseData_t uc_rsp;

	if (!ipa3_uc_mhi_ctx) {
		IPAERR("Not initialized\n");
		return -EFAULT;
	}

	if (ipa_ep_idx < 0  || ipa_ep_idx >= ipa3_ctx->ipa_num_pipes) {
		IPAERR("Invalid ipa_ep_idx.\n");
		return -EINVAL;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	memset(&uc_rsp, 0, sizeof(uc_rsp));
	uc_rsp.params.state = IPA_HW_MHI_CHANNEL_STATE_RUN;
	uc_rsp.params.channelHandle = channelHandle;
	ipa3_uc_mhi_ctx->expected_responseOp =
		IPA_HW_2_CPU_RESPONSE_MHI_CHANGE_CHANNEL_STATE;
	ipa3_uc_mhi_ctx->expected_responseParams = uc_rsp.raw32b;

	memset(&init_cmd, 0, sizeof(init_cmd));
	init_cmd.params.channelHandle = channelHandle;
	init_cmd.params.contexArrayIndex = contexArrayIndex;
	init_cmd.params.bamPipeId = ipa_ep_idx;
	init_cmd.params.channelDirection = channelDirection;

	res = ipa3_uc_send_cmd(init_cmd.raw32b,
		IPA_CPU_2_HW_CMD_MHI_INIT_CHANNEL, 0, false, HZ);
	if (res) {
		IPAERR("ipa3_uc_send_cmd failed %d\n", res);
		goto disable_clks;
	}

	res = 0;

disable_clks:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return res;
}


int ipa3_uc_mhi_reset_channel(int channelHandle)
{
	union IpaHwMhiChangeChannelStateCmdData_t cmd;
	union IpaHwMhiChangeChannelStateResponseData_t uc_rsp;
	int res;

	if (!ipa3_uc_mhi_ctx) {
		IPAERR("Not initialized\n");
		return -EFAULT;
	}
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	memset(&uc_rsp, 0, sizeof(uc_rsp));
	uc_rsp.params.state = IPA_HW_MHI_CHANNEL_STATE_DISABLE;
	uc_rsp.params.channelHandle = channelHandle;
	ipa3_uc_mhi_ctx->expected_responseOp =
		IPA_HW_2_CPU_RESPONSE_MHI_CHANGE_CHANNEL_STATE;
	ipa3_uc_mhi_ctx->expected_responseParams = uc_rsp.raw32b;

	memset(&cmd, 0, sizeof(cmd));
	cmd.params.requestedState = IPA_HW_MHI_CHANNEL_STATE_DISABLE;
	cmd.params.channelHandle = channelHandle;
	res = ipa3_uc_send_cmd(cmd.raw32b,
		IPA_CPU_2_HW_CMD_MHI_CHANGE_CHANNEL_STATE, 0, false, HZ);
	if (res) {
		IPAERR("ipa3_uc_send_cmd failed %d\n", res);
		goto disable_clks;
	}

	res = 0;

disable_clks:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return res;
}

int ipa3_uc_mhi_suspend_channel(int channelHandle)
{
	union IpaHwMhiChangeChannelStateCmdData_t cmd;
	union IpaHwMhiChangeChannelStateResponseData_t uc_rsp;
	int res;

	if (!ipa3_uc_mhi_ctx) {
		IPAERR("Not initialized\n");
		return -EFAULT;
	}
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	memset(&uc_rsp, 0, sizeof(uc_rsp));
	uc_rsp.params.state = IPA_HW_MHI_CHANNEL_STATE_SUSPEND;
	uc_rsp.params.channelHandle = channelHandle;
	ipa3_uc_mhi_ctx->expected_responseOp =
		IPA_HW_2_CPU_RESPONSE_MHI_CHANGE_CHANNEL_STATE;
	ipa3_uc_mhi_ctx->expected_responseParams = uc_rsp.raw32b;

	memset(&cmd, 0, sizeof(cmd));
	cmd.params.requestedState = IPA_HW_MHI_CHANNEL_STATE_SUSPEND;
	cmd.params.channelHandle = channelHandle;
	res = ipa3_uc_send_cmd(cmd.raw32b,
		IPA_CPU_2_HW_CMD_MHI_CHANGE_CHANNEL_STATE, 0, false, HZ);
	if (res) {
		IPAERR("ipa3_uc_send_cmd failed %d\n", res);
		goto disable_clks;
	}

	res = 0;

disable_clks:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return res;
}

int ipa3_uc_mhi_resume_channel(int channelHandle, bool LPTransitionRejected)
{
	union IpaHwMhiChangeChannelStateCmdData_t cmd;
	union IpaHwMhiChangeChannelStateResponseData_t uc_rsp;
	int res;

	if (!ipa3_uc_mhi_ctx) {
		IPAERR("Not initialized\n");
		return -EFAULT;
	}
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	memset(&uc_rsp, 0, sizeof(uc_rsp));
	uc_rsp.params.state = IPA_HW_MHI_CHANNEL_STATE_RUN;
	uc_rsp.params.channelHandle = channelHandle;
	ipa3_uc_mhi_ctx->expected_responseOp =
		IPA_HW_2_CPU_RESPONSE_MHI_CHANGE_CHANNEL_STATE;
	ipa3_uc_mhi_ctx->expected_responseParams = uc_rsp.raw32b;

	memset(&cmd, 0, sizeof(cmd));
	cmd.params.requestedState = IPA_HW_MHI_CHANNEL_STATE_RUN;
	cmd.params.channelHandle = channelHandle;
	cmd.params.LPTransitionRejected = LPTransitionRejected;
	res = ipa3_uc_send_cmd(cmd.raw32b,
		IPA_CPU_2_HW_CMD_MHI_CHANGE_CHANNEL_STATE, 0, false, HZ);
	if (res) {
		IPAERR("ipa3_uc_send_cmd failed %d\n", res);
		goto disable_clks;
	}

	res = 0;

disable_clks:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return res;
}

int ipa3_uc_mhi_stop_event_update_channel(int channelHandle)
{
	union IpaHwMhiStopEventUpdateData_t cmd;
	int res;

	if (!ipa3_uc_mhi_ctx) {
		IPAERR("Not initialized\n");
		return -EFAULT;
	}
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	memset(&cmd, 0, sizeof(cmd));
	cmd.params.channelHandle = channelHandle;

	ipa3_uc_mhi_ctx->expected_responseOp =
		IPA_CPU_2_HW_CMD_MHI_STOP_EVENT_UPDATE;
	ipa3_uc_mhi_ctx->expected_responseParams = cmd.raw32b;

	res = ipa3_uc_send_cmd(cmd.raw32b,
		IPA_CPU_2_HW_CMD_MHI_STOP_EVENT_UPDATE, 0, false, HZ);
	if (res) {
		IPAERR("ipa3_uc_send_cmd failed %d\n", res);
		goto disable_clks;
	}

	res = 0;
disable_clks:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return res;
}

int ipa3_uc_mhi_send_dl_ul_sync_info(union IpaHwMhiDlUlSyncCmdData_t cmd)
{
	int res;

	if (!ipa3_uc_mhi_ctx) {
		IPAERR("Not initialized\n");
		return -EFAULT;
	}

	IPADBG("isDlUlSyncEnabled=0x%x UlAccmVal=0x%x\n",
		cmd.params.isDlUlSyncEnabled, cmd.params.UlAccmVal);
	IPADBG("ulMsiEventThreshold=0x%x dlMsiEventThreshold=0x%x\n",
		cmd.params.ulMsiEventThreshold, cmd.params.dlMsiEventThreshold);

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	res = ipa3_uc_send_cmd(cmd.raw32b,
		IPA_CPU_2_HW_CMD_MHI_DL_UL_SYNC_INFO, 0, false, HZ);
	if (res) {
		IPAERR("ipa3_uc_send_cmd failed %d\n", res);
		goto disable_clks;
	}

	res = 0;
disable_clks:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return res;
}

int ipa3_uc_mhi_print_stats(char *dbg_buff, int size)
{
	int nBytes = 0;
	int i;

	if (!ipa3_uc_mhi_ctx->mhi_uc_stats_mmio) {
		IPAERR("MHI uc stats is not valid\n");
		return 0;
	}

	nBytes += scnprintf(&dbg_buff[nBytes], size - nBytes,
		"Common Stats:\n");
	PRINT_COMMON_STATS(numULDLSync);
	PRINT_COMMON_STATS(numULTimerExpired);
	PRINT_COMMON_STATS(numChEvCtxWpRead);

	for (i = 0; i < IPA_HW_MAX_NUMBER_OF_CHANNELS; i++) {
		nBytes += scnprintf(&dbg_buff[nBytes], size - nBytes,
			"Channel %d Stats:\n", i);
		PRINT_CHANNEL_STATS(i, doorbellInt);
		PRINT_CHANNEL_STATS(i, reProccesed);
		PRINT_CHANNEL_STATS(i, bamFifoFull);
		PRINT_CHANNEL_STATS(i, bamFifoEmpty);
		PRINT_CHANNEL_STATS(i, bamFifoUsageHigh);
		PRINT_CHANNEL_STATS(i, bamFifoUsageLow);
		PRINT_CHANNEL_STATS(i, bamInt);
		PRINT_CHANNEL_STATS(i, ringFull);
		PRINT_CHANNEL_STATS(i, ringEmpty);
		PRINT_CHANNEL_STATS(i, ringUsageHigh);
		PRINT_CHANNEL_STATS(i, ringUsageLow);
		PRINT_CHANNEL_STATS(i, delayedMsi);
		PRINT_CHANNEL_STATS(i, immediateMsi);
		PRINT_CHANNEL_STATS(i, thresholdMsi);
		PRINT_CHANNEL_STATS(i, numSuspend);
		PRINT_CHANNEL_STATS(i, numResume);
		PRINT_CHANNEL_STATS(i, num_OOB);
		PRINT_CHANNEL_STATS(i, num_OOB_timer_expiry);
		PRINT_CHANNEL_STATS(i, num_OOB_moderation_timer_start);
		PRINT_CHANNEL_STATS(i, num_db_mode_evt);
	}

	return nBytes;
}
