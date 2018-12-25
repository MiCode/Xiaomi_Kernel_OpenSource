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

#ifndef __CMDQ_DEF_H__
#define __CMDQ_DEF_H__

#include <linux/kernel.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "cmdq_subsys_common.h"

#define CMDQ_DRIVER_DEVICE_NAME         "mtk_cmdq"

/* #define CMDQ_COMMON_ENG_SUPPORT */
#ifdef CMDQ_COMMON_ENG_SUPPORT
#include "cmdq_engine_common.h"
#else
#include "cmdq_engine.h"
#endif

#define CMDQ_SPECIAL_SUBSYS_ADDR (99)

#define CMDQ_GPR_SUPPORT

#define CMDQ_MAX_PROFILE_MARKER_IN_TASK (5)

#define CMDQ_INVALID_THREAD		(-1)

#define CMDQ_MAX_THREAD_COUNT		(24)
#define CMDQ_MAX_TASK_IN_THREAD		(16)
#define CMDQ_MAX_READ_SLOT_COUNT	(4)
#define CMDQ_INIT_FREE_TASK_COUNT	(8)

/* Thread that are high-priority (display threads) */
#define CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT (8)
#define CMDQ_DELAY_THREAD_ID		CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT
#define CMDQ_MIN_SECURE_THREAD_ID	(CMDQ_DELAY_THREAD_ID + 1)
#define CMDQ_MAX_SECURE_THREAD_COUNT	(3)

#ifdef CMDQ_SECURE_PATH_SUPPORT
#define CMDQ_DYNAMIC_THREAD_ID_START	(CMDQ_MIN_SECURE_THREAD_ID + \
	CMDQ_MAX_SECURE_THREAD_COUNT)
#else
#define CMDQ_DYNAMIC_THREAD_ID_START	(CMDQ_DELAY_THREAD_ID + 1)
#endif

#define CMDQ_MAX_ERROR_COUNT            (2)
#define CMDQ_MAX_RETRY_COUNT            (1)
/* ram optimization related configuration */
#ifdef CONFIG_MTK_GMO_RAM_OPTIMIZE
#define CMDQ_MAX_RECORD_COUNT           (64)
#else
#define CMDQ_MAX_RECORD_COUNT           (128)
#endif

#define CMDQ_INITIAL_CMD_BLOCK_SIZE     (PAGE_SIZE)
/* instruction is 64-bit */
#define CMDQ_DMA_POOL_COUNT		32

#define CMDQ_MAX_LOOP_COUNT             (1000000)
#define CMDQ_MAX_INST_CYCLE             (27)
#define CMDQ_MIN_AGE_VALUE              (5)
#define CMDQ_MAX_ERROR_SIZE             (8 * 1024)

#define CMDQ_MAX_TASK_IN_SECURE_THREAD	(10)

/* max value of CMDQ_THR_EXEC_CMD_CNT (value starts from 0) */
#ifdef CMDQ_USE_LARGE_MAX_COOKIE
#define CMDQ_MAX_COOKIE_VALUE           (0xFFFFFFFF)
#else
#define CMDQ_MAX_COOKIE_VALUE           (0xFFFF)
#endif
#define CMDQ_ARG_A_SUBSYS_MASK          (0x001F0000)

#ifdef CONFIG_MTK_FPGA
#define CMDQ_DEFAULT_TIMEOUT_MS         (10000)
#else
#define CMDQ_DEFAULT_TIMEOUT_MS         (1000)
#endif

#define CMDQ_ACQUIRE_THREAD_TIMEOUT_MS  (2000)
#define CMDQ_PREDUMP_TIMEOUT_MS         (200)

#ifndef CONFIG_MTK_FPGA
#define CMDQ_PWR_AWARE		/* FPGA does not have ClkMgr */
#else
#undef CMDQ_PWR_AWARE
#endif

typedef u64 CMDQ_VARIABLE;
/*
 * SPR / CPR / VAR naming rule and number
 **********************************************
 *              <-  SPR    ->   <-            CPR            ->
 *           reserved              < FREE use >   <  delay >
 * VAR#     0    1    2    3    4    5    6    7    8    9    10
 * CPR#                            0    1    2    3    4    5    6
 */

#define CMDQ_SPR_FOR_TEMP		(0)
#define CMDQ_SPR_FOR_LOOP_DEBUG		(1)
#define CMDQ_THR_SPR_START		(2)
#define CMDQ_THR_SPR_MAX		(4)
#define CMDQ_THR_FREE_CPR_MAX		(4)
#define CMDQ_THR_FREE_USR_VAR_MAX	(CMDQ_THR_SPR_MAX + \
	CMDQ_THR_FREE_CPR_MAX)
#define CMDQ_THR_CPR_MAX		(CMDQ_THR_FREE_CPR_MAX + 0)
#define CMDQ_THR_VAR_MAX		(CMDQ_THR_SPR_MAX + CMDQ_THR_CPR_MAX)
#define CMDQ_TPR_ID			(56)
#define CMDQ_CPR_STRAT_ID		(0x8000)
#define CMDQ_SRAM_STRAT_ADDR		(0x0)
#define CMDQ_GPR_V3_OFFSET			(0x20)
#define CMDQ_POLLING_TPR_MASK_BIT	(10)
#define CMDQ_SRAM_ADDR(CPR_OFFSRT)	\
	(((CMDQ_SRAM_STRAT_ADDR + CPR_OFFSRT / 2) << 3) + 0x001)
#define CMDQ_CPR_OFFSET(SRAM_ADDR)	\
	(((SRAM_ADDR >> 3) - CMDQ_SRAM_STRAT_ADDR) * 2)
#define CMDQ_INVALID_CPR_OFFSET		(0xFFFFFFFF)

#define CMDQ_MAX_SRAM_OWNER_NAME	(32)

#define CMDQ_DELAY_TPR_MASK_BIT		(11)
#define CMDQ_DELAY_TPR_MASK_VALUE	(1 << 17 | 1 << 14 | 1 << 11)

#define CMDQ_DELAY_MAX_SET		(3)
#define CMDQ_DELAY_SET_START_CPR	(0)
#define CMDQ_DELAY_SET_DURATION_CPR	(1)
#define CMDQ_DELAY_SET_RESULT_CPR	(2)
#define CMDQ_DELAY_SET_MAX_CPR		(3)
#define CMDQ_DELAY_THD_SIZE		(64 * 64) /* delay inst in bytes */

/* #define CMDQ_DUMP_GIC (0) */
/* #define CMDQ_PROFILE_MMP (0) */

#define CMDQ_DUMP_FIRSTERROR
/* #define CMDQ_INSTRUCTION_COUNT */

enum CMDQ_HW_THREAD_PRIORITY_ENUM {
	CMDQ_THR_PRIO_SUPERLOW = 0,	/* low priority monitor loop */

	CMDQ_THR_PRIO_NORMAL = 1,	/* nomral priority */
	/* trigger loop (enables display mutex) */
	CMDQ_THR_PRIO_DISPLAY_TRIGGER = 2,

	/* display ESD check (every 2 secs) */
#ifdef CMDQ_SPECIAL_ESD_PRIORITY
	CMDQ_THR_PRIO_DISPLAY_ESD = 3,
#else
	CMDQ_THR_PRIO_DISPLAY_ESD = 4,
#endif

	/* display config (every frame) */
	CMDQ_THR_PRIO_DISPLAY_CONFIG = 4,

	CMDQ_THR_PRIO_SUPERHIGH = 5,	/* High priority monitor loop */

	CMDQ_THR_PRIO_MAX = 7,	/* maximum possible priority */
};

enum CMDQ_SCENARIO_ENUM {
	CMDQ_SCENARIO_JPEG_DEC = 0,
	CMDQ_SCENARIO_PRIMARY_DISP = 1,
	CMDQ_SCENARIO_PRIMARY_MEMOUT = 2,
	CMDQ_SCENARIO_PRIMARY_ALL = 3,
	CMDQ_SCENARIO_SUB_DISP = 4,
	CMDQ_SCENARIO_SUB_MEMOUT = 5,
	CMDQ_SCENARIO_SUB_ALL = 6,
	CMDQ_SCENARIO_MHL_DISP = 7,
	CMDQ_SCENARIO_RDMA0_DISP = 8,
	CMDQ_SCENARIO_RDMA0_COLOR0_DISP = 9,
	CMDQ_SCENARIO_RDMA1_DISP = 10,

	/* Trigger loop scenario does not enable HWs */
	CMDQ_SCENARIO_TRIGGER_LOOP = 11,

	/* client from user space, so the cmd buffer is in user space. */
	CMDQ_SCENARIO_USER_MDP = 12,

	CMDQ_SCENARIO_DEBUG = 13,
	CMDQ_SCENARIO_DEBUG_PREFETCH = 14,

	/* ESD check */
	CMDQ_SCENARIO_DISP_ESD_CHECK = 15,
	/* for screen capture to wait for RDMA-done
	 * without blocking config thread
	 */
	CMDQ_SCENARIO_DISP_SCREEN_CAPTURE = 16,

	CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH = 18,
	CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH = 19,

	/* color path request from kernel */
	CMDQ_SCENARIO_DISP_COLOR = 20,
	/* color path request from user sapce */
	CMDQ_SCENARIO_USER_DISP_COLOR = 21,

	/* [phased out]client from user space,
	 * so the cmd buffer is in user space.
	 */
	CMDQ_SCENARIO_USER_SPACE = 22,

	CMDQ_SCENARIO_DISP_MIRROR_MODE = 23,

	CMDQ_SCENARIO_DISP_CONFIG_AAL = 24,
	CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA = 25,
	CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA = 26,
	CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER = 27,
	CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER = 28,
	CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM = 29,
	CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM = 30,
	CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ = 31,
	CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ = 32,
	CMDQ_SCENARIO_DISP_CONFIG_OD = 33,
	CMDQ_SCENARIO_DISP_VFP_CHANGE = 34,

	CMDQ_SCENARIO_RDMA2_DISP = 35,

	/* for primary trigger loop enable pre-fetch usage */
	CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP = 36,
	/* for low priority monitor loop to polling bus status */
	CMDQ_SCENARIO_LOWP_TRIGGER_LOOP = 37,

	CMDQ_SCENARIO_KERNEL_CONFIG_GENERAL = 38,

	CMDQ_SCENARIO_TIMER_LOOP = 39,
	CMDQ_SCENARIO_MOVE = 40,
	CMDQ_SCENARIO_SRAM_LOOP = 41,

	/* debug scenario use mdp flush */
	CMDQ_SCENARIO_DEBUG_MDP = 42,

	CMDQ_MAX_SCENARIO_COUNT	/* ALWAYS keep at the end */
};

enum CMDQ_MDP_PA_BASE_ENUM {
	CMDQ_MDP_PA_BASE_MM_MUTEX,
	CMDQ_MAX_MDP_PA_BASE_COUNT,	/* ALWAYS keep at the end */
};

#define CMDQ_SUBSYS_GRPNAME_MAX		(30)
/* GCE subsys information */
struct SubsysStruct {
	u32 msb;
	s32 subsysID;
	u32 mask;
	char grpName[CMDQ_SUBSYS_GRPNAME_MAX];
};

struct cmdqDTSDataStruct {
	/* [Out] GCE event table */
	s32 eventTable[CMDQ_SYNC_TOKEN_MAX];
	/* [Out] GCE subsys ID table */
	struct SubsysStruct subsys[CMDQ_SUBSYS_MAX_COUNT];
	/* [Out] MDP Base address */
	u32 MDPBaseAddress[CMDQ_MAX_MDP_PA_BASE_COUNT];
};

/* Custom "wide" pointer type for 64-bit job handle (pointer to VA)
 * typedef unsigned long long cmdqJobHandle_t;
 */
#define cmdqJobHandle_t unsigned long long
/* Custom "wide" pointer type for 64-bit compatibility.
 * Always cast from u32*.
 */
#define cmdqU32Ptr_t unsigned long long

#define CMDQ_U32_PTR(x) ((u32 *)(unsigned long)x)

struct cmdqReadRegStruct {
	/* number of entries in regAddresses */
	u32 count;
	/* an array of 32-bit register addresses (u32) */
	cmdqU32Ptr_t regAddresses;
};

struct cmdqRegValueStruct {
	/* number of entries in result */
	u32 count;

	/* array of 32-bit register values (u32). */
	/* in the same order as cmdqReadRegStruct */
	cmdqU32Ptr_t regValues;
};

struct cmdqReadAddressStruct {
	u32 count;	/* [IN] number of entries in result. */

	/* [IN] array of physical addresses to read.
	 * these value must allocated by CMDQ_IOCTL_ALLOC_WRITE_ADDRESS ioctl
	 *
	 * indeed param dmaAddresses should be UNSIGNED LONG type
	 * for 64 bit kernel.
	 * Considering our plartform supports
	 * max 4GB RAM(upper-32bit don't care for SW)
	 * and consistent common code interface, remain u32 type.
	 */
	cmdqU32Ptr_t dmaAddresses;

	/* [OUT] u32 values that dmaAddresses point into */
	cmdqU32Ptr_t values;
};

/*
 * Secure address metadata:
 * According to handle type,
 * translate handle and replace (_d)th instruciton to
 *     1. sec_addr = hadnle_sec_base_addr(baseHandle) + offset(_b)
 *     2. sec_mva = mva( hadnle_sec_base_addr(baseHandle) + offset(_b) )
 *     3. secure world normal mva = map(baseHandle)
 *        . pass normal mva to parameter baseHandle
 *        . use case: OVL reads from secure and normal buffers
 *          at the same time)
 */
enum CMDQ_SEC_ADDR_METADATA_TYPE {
	CMDQ_SAM_H_2_PA = 0,	/* sec handle to sec PA */
	CMDQ_SAM_H_2_MVA = 1,	/* sec handle to sec MVA */
	CMDQ_SAM_NMVA_2_MVA = 2,	/* map normal MVA to secure world */
};

struct cmdqSecAddrMetadataStruct {
	/* [IN]_d, index of instruction.
	 * Update its arg_b value to real PA/MVA in secure world
	 */
	u32 instrIndex;

	/*
	 * Note: Buffer and offset
	 *
	 *   -------------
	 *   |     |     |
	 *   -------------
	 *   ^     ^  ^  ^
	 *   A     B  C  D
	 *
	 * A: baseHandle
	 * B: baseHandle + blockOffset
	 * C: baseHandle + blockOffset + offset
	 * A~B or B~D: size
	 */

	u32 type;	/* [IN] addr handle type */
	u64 baseHandle;	/* [IN]_h, secure address handle */
	/* [IN]_b, block offset from handle(PA) to current block(plane) */
	u32 blockOffset;
	u32 offset;	/* [IN]_b, buffser offset to secure handle */
	u32 size;	/* buffer size */
	u32 port;	/* hw port id (i.e. M4U port id) */
};

struct cmdqSecDataStruct {
	bool is_secure;		/* [IN]true for secure command */

	/* address metadata, used to translate secure buffer PA
	 * related instruction in secure world
	 */
	u32 addrMetadataCount;	/* [IN] count of element in addrList */
	/* [IN] array of cmdqSecAddrMetadataStruct */
	cmdqU32Ptr_t addrMetadatas;
	u32 addrMetadataMaxCount;	/*[Reserved] */

	u64 enginesNeedDAPC;
	u64 enginesNeedPortSecurity;

	/* [Reserved] This is for CMDQ driver usage itself.
	 * Not for client.
	 */

	/* task index in thread's tasklist. -1 for not in tasklist. */
	s32 waitCookie;
	/* reset HW thread in SWd */
	bool resetExecCnt;
};

struct cmdq_v3_replace_struct {
	/* [IN] count of element in instr_position */
	u32 number;
	/* [IN] position of instruction */
	cmdqU32Ptr_t position;
};

struct cmdqProfileMarkerStruct {
	u32 count;
	/* i.e. cmdqBackupSlotHandle, physical start address of backup slot */
	long long hSlot;
	cmdqU32Ptr_t tag[CMDQ_MAX_PROFILE_MARKER_IN_TASK];
};

struct cmdqCommandStruct {
	/* [IN] deprecated. will remove in the future. */
	u32 scenario;
	/* [IN] task schedule priority. this is NOT HW thread priority. */
	u32 priority;
	/* [IN] bit flag of engines used. */
	u64 engineFlag;
	/* [IN] pointer to instruction buffer. Use 64-bit for compatibility. */
	/* This must point to an 64-bit aligned u32 array */
	cmdqU32Ptr_t pVABase;
	/* [IN] size of instruction buffer, in bytes. */
	u32 blockSize;
	/* [IN] request to read register values at the end of command */
	struct cmdqReadRegStruct regRequest;
	/* [OUT] register values of regRequest */
	struct cmdqRegValueStruct regValue;
	/* [IN/OUT] physical addresses to read value */
	struct cmdqReadAddressStruct readAddress;
	/* [IN] secure execution data */
	struct cmdqSecDataStruct secData;
	/* [IN] CPR position */
	struct cmdq_v3_replace_struct replace_instr;
	/* [IN] use SRAM buffer or not */
	bool use_sram_buffer;
	/* [IN] SRAM buffer owner name */
	char sram_owner_name[CMDQ_MAX_SRAM_OWNER_NAME];
	/* [IN] set to non-zero to enable register debug dump. */
	u32 debugRegDump;
	/* [Reserved] This is for CMDQ driver usage itself.
	 * Not for client. Do not access this field from User Space
	 */
	cmdqU32Ptr_t privateData;
	/* task property */
	u32 prop_size;
	cmdqU32Ptr_t prop_addr;
	struct cmdqProfileMarkerStruct profileMarker;
	cmdqU32Ptr_t userDebugStr;
	u32 userDebugStrLen;
};

enum CMDQ_CAP_BITS {
	/* bit 0: TRUE if WFE instruction support is ready.
	 * FALSE if we need to POLL instead.
	 */
	CMDQ_CAP_WFE = 0,
};


/* reply struct for cmdq_sec_cancel_error_task */
struct cmdqSecCancelTaskResultStruct {
	/* [OUT] */
	bool throwAEE;
	bool hasReset;
	s32 irqFlag;
	u32 errInstr[2];
	u32 regValue;
	u32 pc;
};

#endif	/* __CMDQ_DEF_H__ */
