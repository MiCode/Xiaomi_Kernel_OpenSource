/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _MTK_FLL_H_
#define _MTK_FLL_H_

/************************************************
 * Need maintain for each project
 ************************************************/
/* Core ID control by project */
#define NR_FLL_CPU 4
#define FLL_CPU_START_ID 4
#define FLL_CPU_END_ID 7

/************************************************
 * Register addr, offset, bits range
 ************************************************/
/* FLL List Macro*/
#define FllFastKpOnline "FllFastKpOnline"
#define FllFastKiOnline "FllFastKiOnline"
#define FllSlowKpOnline "FllSlowKpOnline"
#define FllSlowKiOnline "FllSlowKiOnline"
#define FllKpOffline "FllKpOffline"
#define FllKiOffline "FllKiOffline"
#define FllCttTargetScaleDisable "FllCttTargetScaleDisable"
#define FllTargetScale "FllTargetScale"
#define FllInFreqOverrideVal "FllInFreqOverrideVal"
#define FllFreqErrWtOnline "FllFreqErrWtOnline"
#define FllFreqErrWtOffline "FllFreqErrWtOffline"
#define FllPhaseErrWt "FllPhaseErrWt"
#define FllFreqErrCapOnline "FllFreqErrCapOnline"
#define FllFreqErrCapOffline "FllFreqErrCapOffline"
#define FllPhaseErrCap "FllPhaseErrCap"
#define FllStartInPong "FllStartInPong"
#define FllPingMaxThreshold "FllPingMaxThreshold"
#define FllPongMinThreshold "FllPongMinThreshold"
#define FllDccEn "FllDccEn"
#define FllDccClkShaperCalin "FllDccClkShaperCalin"
#define FllClk26mDetDis "FllClk26mDetDis"
#define FllClk26mEn "FllClk26mEn"
#define FllControl "FllControl"
#define FllPhaselockThresh "FllPhaselockThresh"
#define FllPhaselockCycles "FllPhaselockCycles"
#define FllFreqlockRatio "FllFreqlockRatio"
#define FllFreqlockCycles "FllFreqlockCycles"
#define FllFreqUnlock "FllFreqUnlock"
#define FllTst_cctrl "FllTst_cctrl"
#define FllTst_fctrl "FllTst_fctrl"
#define FllTst_sctrl "FllTst_sctrl"
#define FllSlowReqCode "FllSlowReqCode"
#define FllSlowReqResponseSelectMode "FllSlowReqResponseSelectMode"
#define FllSlowReqFastResponseCycles "FllSlowReqFastResponseCycles"
#define FllSlowReqErrorMask "FllSlowReqErrorMask"
#define FllEventSource0 "FllEventSource0"
#define FllEventSource1 "FllEventSource1"
#define FllEventType0 "FllEventType0"
#define FllEventType1 "FllEventType1"
#define FllEventSourceThresh0 "FllEventSourceThresh0"
#define FllEventSourceThresh1 "FllEventSourceThresh1"
#define FllEventFreeze "FllEventFreeze"
#define FllWGTriggerSource "FllWGTriggerSource"
#define FllWGTriggerCaptureDelay "FllWGTriggerCaptureDelay"
#define FllWGTriggerEdge "FllWGTriggerEdge"
#define FllWGTriggerVal "FllWGTriggerVal"
#define FllInFreq "FllInFreq"
#define FllOutFreq "FllOutFreq"
#define FllCalFreq "FllCalFreq"
#define FllStatus "FllStatus"
#define FllPhaseErr "FllPhaseErr"
#define FllPhaseLockDet "FllPhaseLockDet"
#define FllFreqLockDet "FllFreqLockDet"
#define FllClk26mDet "FllClk26mDet"
#define FllErrOnline "FllErrOnline"
#define FllErrOffline "FllErrOffline"
#define Fllfsm_state_sr "Fllfsm_state_sr"
#define Fllfsm_state "Fllfsm_state"
#define FllSctrl_pong "FllSctrl_pong"
#define FllCctrl_ping "FllCctrl_ping"
#define FllFctrl_ping "FllFctrl_ping"
#define FllSctrl_ping "FllSctrl_ping"
#define FllCctrl_pong "FllCctrl_pong"
#define FllFctrl_pong "FllFctrl_pong"
#define FllEventCount0 "FllEventCount0"
#define FllEventCount1 "FllEventCount1"
#define V111 "V111"
#define V112 "V112"
#define V113 "V113"
#define V114 "V114"
#define V115 "V115"
#define V116 "V116"
#define V117 "V117"
#define V118 "V118"
#define V119 "V119"
#define V120 "V120"
#define V121 "V121"
#define V122 "V122"
#define V123 "V123"
#define V124 "V124"
#define V125 "V125"
#define V126 "V126"


/* FLL Cfg metadata offset */
#define FLL_CFG_OFFSET_VALUE 0
#define FLL_CFG_OFFSET_OPTION 20
#define FLL_CFG_OFFSET_CPU 28

/* FLL Cfg metadata bitmask */
#define FLL_CFG_BITMASK_VALUE 0xFFFF
#define FLL_CFG_BITMASK_OPTION 0xFF00000
#define FLL_CFG_BITMASK_CPU 0xF0000000

/************************************************
 * config enum
 ************************************************/
enum FLL_NODE {
	FLL_NODE_LIST_READ,
	FLL_NODE_LIST_WRITE,
	FLL_NODE_RW_REG_READ,
	FLL_NODE_RW_REG_WRITE,
	FLL_NODE_RO_REG_READ,

	NR_FLL_NODE
};

enum FLL_RW_GROUP {
	FLL_RW_GROUP_V111,
	FLL_RW_GROUP_V112,
	FLL_RW_GROUP_V113,
	FLL_RW_GROUP_V114,
	FLL_RW_GROUP_V115,
	FLL_RW_GROUP_V116,
	FLL_RW_GROUP_V117,
	FLL_RW_GROUP_V118,

	NR_FLL_RW_GROUP,
};

enum FLL_RO_GROUP {
	FLL_RO_GROUP_V119,
	FLL_RO_GROUP_V120,
	FLL_RO_GROUP_V121,
	FLL_RO_GROUP_V122,
	FLL_RO_GROUP_V123,
	FLL_RO_GROUP_V124,
	FLL_RO_GROUP_V125,
	FLL_RO_GROUP_V126,

	NR_FLL_RO_GROUP,
};

enum FLL_LIST {
	FLL_LIST_FllFastKpOnline,
	FLL_LIST_FllFastKiOnline,
	FLL_LIST_FllSlowKpOnline,
	FLL_LIST_FllSlowKiOnline,
	FLL_LIST_FllKpOffline,
	FLL_LIST_FllKiOffline,
	FLL_LIST_FllCttTargetScaleDisable,
	FLL_LIST_FllTargetScale,
	FLL_LIST_FllInFreqOverrideVal,
	FLL_LIST_FllFreqErrWtOnline,
	FLL_LIST_FllFreqErrWtOffline,
	FLL_LIST_FllPhaseErrWt,
	FLL_LIST_FllFreqErrCapOnline,
	FLL_LIST_FllFreqErrCapOffline,
	FLL_LIST_FllPhaseErrCap,
	FLL_LIST_FllStartInPong,
	FLL_LIST_FllPingMaxThreshold,
	FLL_LIST_FllPongMinThreshold,
	FLL_LIST_FllDccEn,
	FLL_LIST_FllDccClkShaperCalin,
	FLL_LIST_FllClk26mDetDis,
	FLL_LIST_FllClk26mEn,
	FLL_LIST_FllControl,
	FLL_LIST_FllPhaselockThresh,
	FLL_LIST_FllPhaselockCycles,
	FLL_LIST_FllFreqlockRatio,
	FLL_LIST_FllFreqlockCycles,
	FLL_LIST_FllFreqUnlock,
	FLL_LIST_FllTst_cctrl,
	FLL_LIST_FllTst_fctrl,
	FLL_LIST_FllTst_sctrl,
	FLL_LIST_FllSlowReqCode,
	FLL_LIST_FllSlowReqResponseSelectMode,
	FLL_LIST_FllSlowReqFastResponseCycles,
	FLL_LIST_FllSlowReqErrorMask,
	FLL_LIST_FllEventSource0,
	FLL_LIST_FllEventSource1,
	FLL_LIST_FllEventType0,
	FLL_LIST_FllEventType1,
	FLL_LIST_FllEventSourceThresh0,
	FLL_LIST_FllEventSourceThresh1,
	FLL_LIST_FllEventFreeze,
	FLL_LIST_FllWGTriggerSource,
	FLL_LIST_FllWGTriggerCaptureDelay,
	FLL_LIST_FllWGTriggerEdge,
	FLL_LIST_FllWGTriggerVal,
	FLL_LIST_FllInFreq,
	FLL_LIST_FllOutFreq,
	FLL_LIST_FllCalFreq,
	FLL_LIST_FllStatus,
	FLL_LIST_FllPhaseErr,
	FLL_LIST_FllPhaseLockDet,
	FLL_LIST_FllFreqLockDet,
	FLL_LIST_FllClk26mDet,
	FLL_LIST_FllErrOnline,
	FLL_LIST_FllErrOffline,
	FLL_LIST_Fllfsm_state_sr,
	FLL_LIST_Fllfsm_state,
	FLL_LIST_FllSctrl_pong,
	FLL_LIST_FllCctrl_ping,
	FLL_LIST_FllFctrl_ping,
	FLL_LIST_FllSctrl_ping,
	FLL_LIST_FllCctrl_pong,
	FLL_LIST_FllFctrl_pong,
	FLL_LIST_FllEventCount0,
	FLL_LIST_FllEventCount1,

	NR_FLL_LIST,
};

enum FLL_TRIGGER_STAGE {
	FLL_TRIGGER_STAGE_PROBE,
	FLL_TRIGGER_STAGE_SUSPEND,
	FLL_TRIGGER_STAGE_RESUME,

	NR_FLL_TRIGGER_STAGE,
};

/************************************************
 * Func prototype
 ************************************************/
int fll_resume(struct platform_device *pdev);
int fll_suspend(struct platform_device *pdev, pm_message_t state);
int fll_probe(struct platform_device *pdev);
int fll_create_procfs(const char *proc_name, struct proc_dir_entry *dir);
int fll_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum FLL_TRIGGER_STAGE fll_tri_stage);
void fll_save_memory_info(char *buf, unsigned long long ptp3_mem_size);

/************************************************
 * association with ATF use
 ************************************************/
#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_FLL_CONTROL				0xC200051B
#else
#define MTK_SIP_KERNEL_FLL_CONTROL				0x8200051B
#endif
#endif //_MTK_FLL_H_
