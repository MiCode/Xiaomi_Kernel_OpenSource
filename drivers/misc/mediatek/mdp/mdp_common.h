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

#ifndef __MDP_COMMON_H__
#define __MDP_COMMON_H__

#include "mdp_def.h"
#include "mdp_def_ex.h"
#include "mdp_cmdq_helper_ext.h"
#include <linux/types.h>

#ifdef CONFIG_MTK_SMI_EXT

/* translate port */
typedef uint32_t (*CmdqTranslatePort) (uint32_t engineId);

/* get request */
typedef struct mm_qos_request *(*CmdqGetRequest) (
	uint32_t thread_id, uint32_t port);

/* init pmqos mdp */
typedef void (*CmdqInitPmqosMdp) (s32 index, struct plist_head *owner_list);

/* init pmqos isp */
typedef void (*CmdqInitPmqosIsp) (s32 index, struct plist_head *owner_list);

#endif	/* CONFIG_MTK_SMI_EXT */


#ifdef CMDQ_SECURE_PATH_SUPPORT
extern const u32 isp_iwc_buf_size[];
#endif

/* dump mmsys config */
typedef void (*CmdqDumpMMSYSConfig) (void);

/* query MDP clock is on  */
typedef bool(*CmdqMdpClockIsOn) (enum CMDQ_ENG_ENUM engine);

/* enable MDP clock  */
typedef void (*CmdqEnableMdpClock) (bool enable, enum CMDQ_ENG_ENUM engine);

/* Common Clock Framework */
typedef void (*CmdqMdpInitModuleClk) (void);

/* MDP callback function */
typedef s32(*CmdqMdpClockOn) (u64 engineFlag);

typedef s32(*CmdqMdpDumpInfo) (u64 engineFlag, int level);

typedef s32(*CmdqMdpResetEng) (u64 engineFlag);

typedef s32(*CmdqMdpClockOff) (u64 engineFlag);

/* MDP Initialization setting */
typedef void(*CmdqMdpInitialSet) (void);

/* Initialization & de-initialization MDP base VA */
typedef void (*CmdqMdpInitModuleBaseVA) (void);

typedef void (*CmdqMdpDeinitModuleBaseVA) (void);

/* MDP engine dump */
typedef void (*CmdqMdpDumpRSZ) (const unsigned long base, const char *label);

typedef void (*CmdqMdpDumpTDSHP) (const unsigned long base, const char *label);

/* test MDP clock function */
typedef u32(*CmdqMdpRdmaGetRegOffsetSrcAddr) (void);

typedef u32(*CmdqMdpWrotGetRegOffsetDstAddr) (void);

typedef u32(*CmdqMdpWdmaGetRegOffsetDstAddr) (void);

typedef void (*CmdqTestcaseClkmgrMdp) (void);

typedef const char*(*CmdqDispatchModule) (u64 engineFlag);

typedef void (*CmdqTrackTask) (const struct cmdqRecStruct *task);

typedef const char *(*CmdqPraseErrorModByEngFlag) (
	const struct cmdqRecStruct *task);

typedef const char *(*CmdqPraseHandleErrorModByEngFlag) (
	const struct cmdqRecStruct *handle);

typedef u64 (*CmdqMdpGetEngineGroupBits) (u32 engine_group);

typedef void (*CmdqMdpEnableCommonClock) (bool enable);

typedef void (*CmdqCheckHwStatus) (struct cmdqRecStruct *handle);

typedef u64(*CmdqMdpGetSecEngine) (u64 engine_flag);

typedef void (*CmdqMdpResolveToken) (u64 engine_flag,
	const struct cmdqRecStruct *task);

typedef void (*CmdqMdpComposeReadback) (struct cmdqRecStruct *handle,
	u16 engine, dma_addr_t dma, u32 param);

typedef void (*CmdqMdpReadbackEngine) (struct cmdqRecStruct *handle,
	u16 engine, phys_addr_t base, dma_addr_t pa, u32 param);

struct cmdqMDPFuncStruct {
#ifdef CONFIG_MTK_SMI_EXT
	CmdqTranslatePort translatePort;
	CmdqGetRequest getRequest;
	CmdqInitPmqosMdp initPmqosMdp;
	CmdqInitPmqosIsp initPmqosIsp;
#endif	/* CONFIG_MTK_SMI_EXT */
	CmdqDumpMMSYSConfig dumpMMSYSConfig;
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
	CmdqPraseErrorModByEngFlag parseErrModByEngFlag;
	CmdqPraseHandleErrorModByEngFlag parseHandleErrModByEngFlag;
	CmdqMdpGetEngineGroupBits getEngineGroupBits;
	CmdqErrorResetCB errorReset;
	CmdqMdpEnableCommonClock mdpEnableCommonClock;
	CmdqBeginTaskCB beginTask;
	CmdqEndTaskCB endTask;
	CmdqBeginTaskCB beginISPTask;
	CmdqEndTaskCB endISPTask;
	CmdqCheckHwStatus CheckHwStatus;
	CmdqMdpGetSecEngine mdpGetSecEngine;
	CmdqMdpResolveToken resolve_token;
	CmdqMdpComposeReadback mdpComposeReadback;
	CmdqMdpReadbackEngine mdpReadbackAal;
	CmdqMdpReadbackEngine mdpReadbackHdr;
};

struct mdp_pmqos_record {
	uint32_t mdp_throughput;
	struct timeval submit_tm;
	struct timeval end_tm;
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
#define MDP_TOTAL_THREAD 16
#ifdef CMDQ_SECURE_PATH_SUPPORT
#define MDP_THREAD_START (CMDQ_MIN_SECURE_THREAD_ID + 2)
#define MDP_MAX_METADATA_COUNT_SIZE   ( \
	(0xFFFFFFFF) / (sizeof(struct cmdqSecAddrMetadataStruct)))
#else
#define MDP_THREAD_START CMDQ_DYNAMIC_THREAD_ID_START
#endif

/* MDP common kernel logic */

void cmdq_mdp_fix_command_scenario_for_user_space(
	struct cmdqCommandStruct *command);
bool cmdq_mdp_is_request_from_user_space(
	const enum CMDQ_SCENARIO_ENUM scenario);
s32 cmdq_mdp_query_usage(s32 *counters);
s32 cmdq_mdp_get_smi_usage(void);

void cmdq_mdp_reset_resource(void);
void cmdq_mdp_dump_thread_usage(void);
void cmdq_mdp_dump_engine_usage(void);
void cmdq_mdp_dump_resource(u32 event);
void cmdq_mdp_init_resource(u32 engine_id,
	enum cmdq_event res_event);
void cmdq_mdp_enable_res(u64 engine_flag, bool enable);
void cmdq_mdp_lock_resource(u64 engine_flag, bool from_notify);
bool cmdq_mdp_acquire_resource(enum cmdq_event res_event,
	u64 *engine_flag_out);
void cmdq_mdp_release_resource(enum cmdq_event res_event,
	u64 *engine_flag_out);
void cmdq_mdp_set_resource_callback(enum cmdq_event res_event,
	CmdqResourceAvailableCB res_available,
	CmdqResourceReleaseCB res_release);
void cmdq_mdp_unlock_thread(struct cmdqRecStruct *handle);
void cmdq_mdp_op_readback(struct cmdqRecStruct *handle, u16 engine,
	dma_addr_t dma, u32 param);
s32 cmdq_mdp_flush_async(struct cmdqCommandStruct *desc, bool user_space,
	struct cmdqRecStruct **handle_out);
s32 cmdq_mdp_flush_async_impl(struct cmdqRecStruct *handle);
struct cmdqRecStruct *cmdq_mdp_get_valid_handle(unsigned long job);
s32 cmdq_mdp_wait(struct cmdqRecStruct *handle,
	struct cmdqRegValueStruct *results);
s32 cmdq_mdp_flush(struct cmdqCommandStruct *desc, bool user_space);
void cmdq_mdp_suspend(void);
void cmdq_mdp_resume(void);
void cmdq_mdp_release_task_by_file_node(void *file_node);
void cmdq_mdp_init(struct platform_device *pdev);
void cmdq_mdp_deinit_pmqos(void);
s32 cmdq_mdp_handle_create(struct cmdqRecStruct **handle_out);
s32 cmdq_mdp_handle_flush(struct cmdqRecStruct *handle);
s32 cmdq_mdp_handle_sec_setup(struct cmdqSecDataStruct *secData,
			struct cmdqRecStruct *handle);
s32 cmdq_mdp_update_sec_addr_index(struct cmdqRecStruct *handle,
	u32 sec_handle, u32 index, u32 instr_index);
u32 cmdq_mdp_handle_get_instr_count(struct cmdqRecStruct *handle);
void cmdq_mdp_meta_replace_sec_addr(struct op_meta *metas,
			struct mdp_submit *user_job,
			struct cmdqRecStruct *handle);

/* Platform dependent function */

void cmdq_mdp_virtual_function_setting(void);
void cmdq_mdp_map_mmsys_VA(void);
long cmdq_mdp_get_module_base_VA_MMSYS_CONFIG(void);
void cmdq_mdp_unmap_mmsys_VA(void);
struct cmdqMDPFuncStruct *cmdq_mdp_get_func(void);

void cmdq_mdp_enable(u64 engineFlag, enum CMDQ_ENG_ENUM engine);

int cmdq_mdp_loop_reset(enum CMDQ_ENG_ENUM engine,
	const unsigned long resetReg,
	const unsigned long resetStateReg,
	const u32 resetMask,
	const u32 resetValue, const bool pollInitResult);

void cmdq_mdp_loop_off(enum CMDQ_ENG_ENUM engine,
	const unsigned long resetReg,
	const unsigned long resetStateReg,
	const u32 resetMask,
	const u32 resetValue, const bool pollInitResult);

const char *cmdq_mdp_get_rsz_state(const u32 state);

void cmdq_mdp_dump_venc(const unsigned long base, const char *label);
void cmdq_mdp_dump_rdma(const unsigned long base, const char *label);
void cmdq_mdp_dump_rot(const unsigned long base, const char *label);
void cmdq_mdp_dump_color(const unsigned long base, const char *label);
void cmdq_mdp_dump_wdma(const unsigned long base, const char *label);

void cmdq_mdp_check_TF_address(unsigned int mva, char *module);
const char *cmdq_mdp_parse_error_module_by_hwflag(
	const struct cmdqRecStruct *task);

const char *cmdq_mdp_parse_handle_error_module_by_hwflag(
	const struct cmdqRecStruct *handle);

/* Platform dependent function */

s32 cmdqMdpClockOn(u64 engineFlag);

s32 cmdqMdpDumpInfo(u64 engineFlag, int level);

s32 cmdqMdpResetEng(u64 engineFlag);

s32 cmdqMdpClockOff(u64 engineFlag);

u32 cmdq_mdp_rdma_get_reg_offset_src_addr(void);
u32 cmdq_mdp_wrot_get_reg_offset_dst_addr(void);
u32 cmdq_mdp_wdma_get_reg_offset_dst_addr(void);

void testcase_clkmgr_mdp(void);

u32 cmdq_mdp_get_hw_reg(enum MDP_ENG_BASE base, u16 offset);
u32 cmdq_mdp_get_hw_port(enum MDP_ENG_BASE base);

/* Platform virtual function setting */
void cmdq_mdp_platform_function_setting(void);

long cmdq_mdp_get_module_base_VA_MDP_WROT0(void);

#endif				/* __MDP_COMMON_H__ */
