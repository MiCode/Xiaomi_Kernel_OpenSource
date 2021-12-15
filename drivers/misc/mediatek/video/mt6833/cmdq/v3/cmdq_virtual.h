/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __CMDQ_CORE_VIRTUAL_H__
#define __CMDQ_CORE_VIRTUAL_H__

#include "cmdq_def.h"
#include "cmdq_helper_ext.h"

/* get subsys LSB in arg_a */
typedef u32(*CmdqGetSubsysLSBArgA) (void);

/* is a secure thread */
typedef bool(*CmdqIsSecureThread) (const s32 thread);

/* is display scenario */
typedef bool(*CmdqIsDispScenario) (const enum CMDQ_SCENARIO_ENUM scenario);

/* is exclusive thread scenario */
typedef bool(*CmdqIsDynamic) (const enum CMDQ_SCENARIO_ENUM scenario);

/* should enable prefetch */
typedef bool(*CmdqShouldEnablePrefetch) (
	const enum CMDQ_SCENARIO_ENUM scenario);

/* display thread index from scenario */
typedef int (*CmdqDispThread) (enum CMDQ_SCENARIO_ENUM scenario);

/* get thread index from scenario and secure */
typedef int (*CmdqGetThreadID) (enum CMDQ_SCENARIO_ENUM scenario,
	const bool secure);

/* priority from scenario */
typedef enum CMDQ_HW_THREAD_PRIORITY_ENUM(*CmdqPriority) (
	enum CMDQ_SCENARIO_ENUM scenario);

/*  force loop IRQ from scenario */
typedef bool(*cmdq_force_loop_irq) (enum CMDQ_SCENARIO_ENUM scenario);

/*  is disp loop */
typedef bool(*cmdq_is_disp_loop) (enum CMDQ_SCENARIO_ENUM scenario);

/* get register index from hwflag */
typedef void(*CmdqGetRegID) (u64 hwflag,
	enum cmdq_gpr_reg *valueRegId,
	enum cmdq_gpr_reg *destRegId,
	enum cmdq_event *regAccessToken);

/*  module from event index */
typedef const char *(*CmdqModuleFromEvent) (const s32 event,
	struct CmdqCBkStruct *groupCallback, u64 engineFlag);

/* parse module from register addr */
typedef const char *(*CmdqParseModule) (u32 reg_addr);

/* can module entry suspend */
typedef s32(*CmdqModuleEntrySuspend) (struct EngineStruct *engineList);

/* print status clock */
typedef ssize_t(*CmdqPrintStatusClock) (char *buf);

/* print seq status clock */
typedef void (*CmdqPrintStatusSeqClock) (struct seq_file *m);

/* enable GCE clock locked */
typedef void (*CmdqEnableGCEClockLocked) (bool enable);

/* parse error module by hwflag */
typedef const char *(*CmdqParseErrorModule) (const struct cmdqRecStruct *pTask);

/* parse error module by hwflag */
typedef const char *(*CmdqParseHandleErrorModule) (
const struct cmdqRecStruct *pHandle);

/* dump SMI */
typedef int (*CmdqDumpSMI) (const int showSmiDump);

/* dump GPR */
typedef void (*CmdqDumpGPR) (void);

/* flag from scenario */
typedef u64(*CmdqFlagFromScenario) (enum CMDQ_SCENARIO_ENUM scenario);

/* evet backup */
typedef void (*CmdqEventBackup) (void);

/* evet restore */
typedef void (*CmdqEventRestore) (void);

/* test setup */
typedef void (*CmdqTestSetup) (void);

/* test cleanup */
typedef void (*CmdqTestCleanup) (void);

/* test for instruction statistic */
typedef void (*CmdqInitModulePAStat) (void);

struct cmdqCoreFuncStruct {
	CmdqGetSubsysLSBArgA getSubsysLSBArgA;
	CmdqIsSecureThread isSecureThread;
	CmdqIsDispScenario isDispScenario;
	CmdqIsDynamic isDynamic;
	CmdqShouldEnablePrefetch shouldEnablePrefetch;
	CmdqDispThread dispThread;
	CmdqGetThreadID getThreadID;
	CmdqPriority priority;
	cmdq_force_loop_irq force_loop_irq;
	cmdq_is_disp_loop is_disp_loop;
	CmdqGetRegID getRegID;
	CmdqModuleFromEvent moduleFromEvent;
	CmdqParseModule parseModule;
	CmdqModuleEntrySuspend moduleEntrySuspend;
	CmdqPrintStatusClock printStatusClock;
	CmdqPrintStatusSeqClock printStatusSeqClock;
	CmdqEnableGCEClockLocked enableGCEClockLocked;
	CmdqParseErrorModule parseErrorModule;
	CmdqParseHandleErrorModule parseHandleErrorModule;
	CmdqDumpSMI dumpSMI;
	CmdqDumpGPR dumpGPR;
	CmdqFlagFromScenario flagFromScenario;
	CmdqEventBackup eventBackup;
	CmdqEventRestore eventRestore;
	CmdqTestSetup testSetup;
	CmdqTestCleanup testCleanup;
	CmdqInitModulePAStat initModulePAStat;
};

#ifdef __cplusplus
extern "C" {
#endif
	void cmdq_virtual_function_setting(void);
	struct cmdqCoreFuncStruct *cmdq_get_func(void);

#ifdef __cplusplus
}
#endif
#endif				/* __CMDQ_CORE_VIRTUAL_H__ */
