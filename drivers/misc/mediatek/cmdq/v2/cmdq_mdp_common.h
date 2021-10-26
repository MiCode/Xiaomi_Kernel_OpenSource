/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __CMDQ_MDP_COMMON_H__
#define __CMDQ_MDP_COMMON_H__

#include "cmdq_def.h"
#include "cmdq_core.h"
#include "cmdq_virtual.h"

#include <linux/types.h>

#if defined(CMDQ_USE_LEGACY)
#include <linux/clk.h>
#endif

/* VENC callback function */
typedef int32_t(*CmdqVEncDumpInfo) (uint64_t engineFlag, int level);

/* query MDP clock is on  */
typedef bool(*CmdqMdpClockIsOn) (enum CMDQ_ENG_ENUM engine);

/* enable MDP clock  */
typedef void (*CmdqEnableMdpClock) (bool enable, enum CMDQ_ENG_ENUM engine);

/* Common Clock Framework */
typedef void (*CmdqMdpInitModuleClk) (void);

/* MDP callback function */
typedef int32_t(*CmdqMdpClockOn) (uint64_t engineFlag);

typedef int32_t(*CmdqMdpDumpInfo) (uint64_t engineFlag, int level);

typedef int32_t(*CmdqMdpResetEng) (uint64_t engineFlag);

typedef int32_t(*CmdqMdpClockOff) (uint64_t engineFlag);

/* MDP Initialization setting */
typedef void(*CmdqMdpInitialSet) (void);

/* Initialization & de-initialization MDP base VA */
typedef void (*CmdqMdpInitModuleBaseVA) (void);

typedef void (*CmdqMdpDeinitModuleBaseVA) (void);

/* MDP engine dump */
typedef void (*CmdqMdpDumpRSZ) (const unsigned long base, const char *label);

typedef void (*CmdqMdpDumpTDSHP) (const unsigned long base, const char *label);

/* test MDP clock function */
typedef uint32_t(*CmdqMdpRdmaGetRegOffsetSrcAddr) (void);

typedef uint32_t(*CmdqMdpWrotGetRegOffsetDstAddr) (void);

typedef uint32_t(*CmdqMdpWdmaGetRegOffsetDstAddr) (void);

typedef void (*CmdqTestcaseClkmgrMdp) (void);

typedef const char*(*CmdqDispatchModule) (uint64_t engineFlag);

typedef void (*CmdqTrackTask) (const struct TaskStruct *pTask);

#if defined(CMDQ_USE_LEGACY)
typedef void (*CmdqMdpInitModuleClkMutex32K) (void);
#endif
#ifdef CONFIG_MTK_CMDQ_TAB
typedef void (*CmdqMdpSmiLarbEnableClock) (bool enable);
#endif

#ifdef CMDQ_OF_SUPPORT
typedef void (*CmdqMdpGetModulePa) (long *startPA, long *endPA);
#endif

#ifdef CMDQ_USE_LEGACY
typedef void (*CmdqMdpEnableClockMutex32k) (bool enable);
#endif

struct cmdqMDPFuncStruct {
	CmdqDumpMMSYSConfig dumpMMSYSConfig;
	CmdqVEncDumpInfo vEncDumpInfo;
	CmdqMdpInitModuleBaseVA initModuleBaseVA;
	CmdqMdpDeinitModuleBaseVA deinitModuleBaseVA;
	CmdqMdpClockIsOn mdpClockIsOn;
	CmdqEnableMdpClock enableMdpClock;
	CmdqMdpInitModuleClk initModuleCLK;
	CmdqMdpDumpRSZ mdpDumpRsz;
	CmdqMdpDumpTDSHP mdpDumpTdshp;
	CmdqMdpClockOn mdpClockOn;
	CmdqMdpDumpInfo mdpDumpInfo;
	CmdqMdpResetEng mdpResetEng;
	CmdqMdpClockOff mdpClockOff;
	CmdqMdpInitialSet mdpInitialSet;
	CmdqMdpRdmaGetRegOffsetSrcAddr rdmaGetRegOffsetSrcAddr;
	CmdqMdpWrotGetRegOffsetDstAddr wrotGetRegOffsetDstAddr;
	CmdqMdpWdmaGetRegOffsetDstAddr wdmaGetRegOffsetDstAddr;
	CmdqTestcaseClkmgrMdp testcaseClkmgrMdp;
	CmdqDispatchModule dispatchModule;
	CmdqTrackTask trackTask;
#if defined(CMDQ_USE_LEGACY)
	CmdqMdpInitModuleClkMutex32K mdpInitModuleClkMutex32K;
#endif
#ifdef CONFIG_MTK_CMDQ_TAB
	CmdqMdpSmiLarbEnableClock mdpSmiLarbEnableClock;
#endif
#ifdef CMDQ_OF_SUPPORT
	CmdqMdpGetModulePa mdpGetModulePa;
#endif
#ifdef CMDQ_USE_LEGACY
	CmdqMdpEnableClockMutex32k mdpEnableClockMutex32k;
#endif
};

/* track MDP task */
#define DEBUG_STR_LEN 1024 /* debug str length */
#define MDP_MAX_TASK_NUM 5 /* num of tasks to be keep */
#define MDP_MAX_PLANE_NUM 3 /* max color format plane num */
/* each plane has 2 info address and size */
#define MDP_PORT_BUF_INFO_NUM (MDP_MAX_PLANE_NUM * 2)
#define MDP_BUF_INFO_STR_LEN 8 /* each buf info length */
/* dispatch key format is MDP_(ThreadName) */
#define MDP_DISPATCH_KEY_STR_LEN (TASK_COMM_LEN + 5)

struct cmdqMDPTaskStruct {
	char callerName[TASK_COMM_LEN];
	char userDebugStr[DEBUG_STR_LEN];
};

#ifdef __cplusplus
extern "C" {
#endif
	void cmdq_mdp_virtual_function_setting(void);
	struct cmdqMDPFuncStruct *cmdq_mdp_get_func(void);

	void cmdq_mdp_enable(uint64_t engineFlag,
		enum CMDQ_ENG_ENUM engine);

	int cmdq_mdp_loop_reset(enum CMDQ_ENG_ENUM engine,
				const unsigned long resetReg,
				const unsigned long resetStateReg,
				const uint32_t resetMask,
				const uint32_t resetValue,
				const bool pollInitResult);

	void cmdq_mdp_loop_off(enum CMDQ_ENG_ENUM engine,
			       const unsigned long resetReg,
			       const unsigned long resetStateReg,
			       const uint32_t resetMask,
			       const uint32_t resetValue,
			       const bool pollInitResult);

	const char *cmdq_mdp_get_rsz_state(const uint32_t state);

	void cmdq_mdp_dump_venc(const unsigned long base,
		const char *label);
	void cmdq_mdp_dump_rdma(const unsigned long base,
		const char *label);
	void cmdq_mdp_dump_rot(const unsigned long base,
		const char *label);
	void cmdq_mdp_dump_color(const unsigned long base,
		const char *label);
	void cmdq_mdp_dump_wdma(const unsigned long base,
		const char *label);

	void cmdq_mdp_check_TF_address(unsigned int mva,
		char *module);

/******************************************/
/*********                    Platform dependent function               ******/
/***********************************************************/

	int32_t cmdqMdpClockOn(uint64_t engineFlag);

	int32_t cmdqMdpDumpInfo(uint64_t engineFlag, int level);

	int32_t cmdqVEncDumpInfo(uint64_t engineFlag, int level);

	int32_t cmdqMdpResetEng(uint64_t engineFlag);

	int32_t cmdqMdpClockOff(uint64_t engineFlag);

	uint32_t cmdq_mdp_rdma_get_reg_offset_src_addr(void);
	uint32_t cmdq_mdp_wrot_get_reg_offset_dst_addr(void);
	uint32_t cmdq_mdp_wdma_get_reg_offset_dst_addr(void);

	void testcase_clkmgr_mdp(void);

	u32 cmdq_mdp_get_hw_reg(enum MDP_ENG_BASE base, u16 offset);
	u32 cmdq_mdp_get_hw_port(enum MDP_ENG_BASE base);

	/* Platform virtual function setting */
	void cmdq_mdp_platform_function_setting(void);

#ifdef __cplusplus
}
#endif
#endif				/* __CMDQ_MDP_COMMON_H__ */
