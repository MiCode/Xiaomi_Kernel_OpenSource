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
#ifdef CONFIG_OF
#define CMDQ_OF_SUPPORT		/* enable device tree support */
#else
#undef  CMDQ_OF_SUPPORT
#endif

/* CMDQ capability*/
#define CMDQ_GPR_SUPPORT

#define CMDQ_DUMP_GIC (0)	/* TODO: enable when GIC porting done */
#define CMDQ_PROFILE_MMP (0)

#include <linux/kernel.h>
#include "cmdq_def_idv.h"
#ifdef CMDQ_SECURE_PATH_SUPPORT
#include "cmdq_sec_iwc_common.h"
#endif
#include "cmdq_sec_iwc_common.h"

#ifdef CONFIG_MTK_SMI_EXT
/* #define CMDQ_CONFIG_SMI (1) */
#endif

#define CMDQ_INIT_FREE_TASK_COUNT       (8)
#define CMDQ_MAX_THREAD_COUNT           (16)
#define CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT (6)	/* Thread that are high-priority (display threads) */
#define CMDQ_MIN_SECURE_THREAD_ID		(12)
#define CMDQ_MAX_SECURE_THREAD_COUNT	(3)
#define CMDQ_MAX_TRIGGER_LOOP_COUNT     (2)
#define CMDQ_MAX_RECORD_COUNT           (1024)
#define CMDQ_MAX_ERROR_COUNT            (2)
#define CMDQ_MAX_RETRY_COUNT            (1)
#define CMDQ_MAX_TASK_IN_THREAD         (16)
#define CMDQ_MAX_READ_SLOT_COUNT        (4)

#define CMDQ_MAX_PREFETCH_INSTUCTION    (240)	/* Maximum prefetch buffer size, in instructions. */
#define CMDQ_INITIAL_CMD_BLOCK_SIZE     (PAGE_SIZE)
#define CMDQ_EMERGENCY_BLOCK_SIZE       (256 * 1024)	/* 128KB command buffer */
#define CMDQ_EMERGENCY_BLOCK_COUNT      (4)
#define CMDQ_INST_SIZE                  (2 * sizeof(uint32_t))	/* instruction is 64-bit */

#define CMDQ_MAX_LOOP_COUNT             (1000000)
#define CMDQ_MAX_INST_CYCLE             (27)
#define CMDQ_MIN_AGE_VALUE              (5)
#define CMDQ_MAX_ERROR_SIZE             (8 * 1024)

#define CMDQ_MAX_COOKIE_VALUE           (0xFFFF)	/* max value of CMDQ_THR_EXEC_CMD_CNT (value starts from 0) */
#define CMDQ_ARG_A_SUBSYS_MASK          (0x001F0000)

#ifdef CONFIG_MTK_FPGA
#define CMDQ_DEFAULT_TIMEOUT_MS         (10000)
#else
#define CMDQ_DEFAULT_TIMEOUT_MS         (1000)
#endif

#define CMDQ_ACQUIRE_THREAD_TIMEOUT_MS  (2000)
#define CMDQ_PREDUMP_TIMEOUT_MS         (200)
#define CMDQ_PREDUMP_RETRY_COUNT        (5)

#define CMDQ_INVALID_THREAD             (-1)

#define CMDQ_DRIVER_DEVICE_NAME         "mtk_cmdq"

#define CMDQ_SEC_SHARED_MEM_THR_CNT_BASE (0)
#ifndef CONFIG_MTK_FPGA
#define CMDQ_PWR_AWARE 1	/* FPGA does not have ClkMgr */
#else
#undef CMDQ_PWR_AWARE
#endif

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
	CMDQ_SCENARIO_RDMA2_DISP = 9,

	CMDQ_SCENARIO_TRIGGER_LOOP = 10,	/* Trigger loop scenario does not enable HWs */

	CMDQ_SCENARIO_DISP_CONFIG_AAL = 11,
	CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA = 12,
	CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA = 13,
	CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER = 14,
	CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER = 15,
	CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM = 16,
	CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM = 17,
	CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ = 18,
	CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ = 19,
	CMDQ_SCENARIO_DISP_CONFIG_OD = 20,

	CMDQ_SCENARIO_USER_SPACE = 21,	/* client from user space, so the cmd buffer is in user space. */

	CMDQ_SCENARIO_DEBUG = 22,
	CMDQ_SCENARIO_DEBUG_PREFETCH = 23,

	CMDQ_SCENARIO_DISP_ESD_CHECK = 24,	/* ESD check */
	/* for screen capture to wait for RDMA-done without blocking config thread */
	CMDQ_SCENARIO_DISP_SCREEN_CAPTURE = 25,
	CMDQ_SCENARIO_USER_MDP = 26,	/* client from user space, so the cmd buffer is in user space. */
	CMDQ_SCENARIO_SECURE_NOTIFY_LOOP = 27,	/* notifiy there are some tasks exec done in secure path */

	CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH = 28,
	CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH = 29,

	CMDQ_SCENARIO_DISP_COLOR = 30,	/* color path request from kernel */
	CMDQ_SCENARIO_USER_DISP_COLOR = 31,	/* color path request from user sapce */


	CMDQ_SCENARIO_DISP_MIRROR_MODE = 32,

	CMDQ_MAX_SCENARIO_COUNT	/* ALWAYS keep at the end */
};

enum CMDQ_HW_THREAD_PRIORITY_ENUM {
	CMDQ_THR_PRIO_NORMAL = 0,	/* nomral (lowest) priority */
	CMDQ_THR_PRIO_DISPLAY_TRIGGER = 1,	/* trigger loop (enables display mutex) */

	CMDQ_THR_PRIO_DISPLAY_ESD = 3,	/* display ESD check (every 2 secs) */
	CMDQ_THR_PRIO_DISPLAY_CONFIG = 3,	/* display config (every frame) */

	CMDQ_THR_PRIO_MAX = 7,	/* maximum possible priority */
};

enum CMDQ_DATA_REGISTER_ENUM {
	/* Value Reg, we use 32-bit */
	/* Address Reg, we use 64-bit */
	/* Note that R0-R15 and P0-P7 actullay share same memory */
	/* and R1 cannot be used. */

	CMDQ_DATA_REG_JPEG = 0x00,	/* R0 */
	CMDQ_DATA_REG_JPEG_DST = 0x11,	/* P1 */

	CMDQ_DATA_REG_PQ_COLOR = 0x04,	/* R4 */
	CMDQ_DATA_REG_PQ_COLOR_DST = 0x13,	/* P3 */

	CMDQ_DATA_REG_2D_SHARPNESS_0 = 0x05,	/* R5 */
	CMDQ_DATA_REG_2D_SHARPNESS_0_DST = 0x14,	/* P4 */

	CMDQ_DATA_REG_2D_SHARPNESS_1 = 0x0a,	/* R10 */
	CMDQ_DATA_REG_2D_SHARPNESS_1_DST = 0x16,	/* P6 */

	CMDQ_DATA_REG_DEBUG = 0x0b,	/* R11 */
	CMDQ_DATA_REG_DEBUG_DST = 0x17,	/* P7 */

	/* sentinel value for invalid register ID */
	CMDQ_DATA_REG_INVALID = -1,
};

/* CMDQ Events */
#undef DECLARE_CMDQ_EVENT
#define DECLARE_CMDQ_EVENT(name, val) name = val,
enum CMDQ_EVENT_ENUM {
#include "cmdq_event.h"
};
#undef DECLARE_CMDQ_EVENT

/* Custom "wide" pointer type for 64-bit job handle (pointer to VA) */
typedef unsigned long long cmdqJobHandle_t;
/* Custom "wide" pointer type for 64-bit compatibility. Always cast from uint32_t*. */
typedef unsigned long long cmdqU32Ptr_t;
#define CMDQ_U32_PTR(x) ((uint32_t *)(unsigned long)x)

struct cmdqReadRegStruct {
	uint32_t count;		/* number of entries in regAddresses */
	cmdqU32Ptr_t regAddresses;	/* an array of 32-bit register addresses (uint32_t) */
};

struct cmdqRegValueStruct {
	/* number of entries in result */
	uint32_t count;
	/* number of entries in result */
	/* array of 32-bit register values (uint32_t). */
	/* in the same order as cmdqReadRegStruct */
	cmdqU32Ptr_t regValues;
};

struct cmdqReadAddressStruct {
	uint32_t count;		/* [IN] number of entries in result. */
	/* [IN] array of physical addresses to read. */
	/* these value must allocated by CMDQ_IOCTL_ALLOC_WRITE_ADDRESS ioctl */
	/*  */
	/* indeed param dmaAddresses should be UNSIGNED LONG type for 64 bit kernel. */
	/* Considering our plartform supports max 4GB RAM(upper-32bit don't care for SW) */
	/* and consistent common code interface, remain uint32_t type. */
	cmdqU32Ptr_t dmaAddresses;

	cmdqU32Ptr_t values;	/* [OUT] uint32_t values that dmaAddresses point into */
};
/*
 * Secure address metadata:
 * According to handle type, translate handle and replace (_d)th instruciton to
 *     1. sec_addr = hadnle_sec_base_addr(baseHandle) + offset(_b)
 *     2. sec_mva = mva( hadnle_sec_base_addr(baseHandle) + offset(_b) )
 *     3. secure world normal mva = map(baseHandle)
 *        . pass normal mva to parameter baseHandle
 *        . use case: OVL reads from secure and normal buffers at the same time)
 */
typedef enum CMDQ_SEC_ADDR_METADATA_TYPE {
	CMDQ_SAM_H_2_PA = 0,	/* sec handle to sec PA */
	CMDQ_SAM_H_2_MVA = 1,	/* sec handle to sec MVA */
	CMDQ_SAM_NMVA_2_MVA = 2,	/* map normal MVA to secure world */
	CMDQ_SAM_DDP_REG_HDCP = 3,	/* DDP register needs to set opposite value when HDCP fail */
	CMDQ_SAM_MEMSET_BUFFER = 4,	/*memset this specify buffer*/
	CMDQ_SAM_NMVA2_MVA_REVERSE = 5, /* reverse map normal MVA to secure world */
} CMDQ_SEC_ADDR_METADATA_TYPE;

typedef struct cmdqSecAddrMetadataStruct {
	/* [IN]_d, index of instruction. Update its argB value to real PA/MVA in secure world */
	uint32_t instrIndex;

	CMDQ_SEC_ADDR_METADATA_TYPE type;	/* [IN] addr handle type */
	uint32_t baseHandle;	/* [IN]_h, secure address handle */
	uint32_t offset;	/* [IN]_b, buffser offset to secure handle */
	uint32_t size;		/* buffer size */
	uint32_t port;		/* hw port id (i.e. M4U port id) */
} cmdqSecAddrMetadataStruct;

typedef struct cmdqSecDataStruct {
	bool isSecure;		/* [IN]true for secure command */

	/* address metadata, used to translate secure buffer PA related instruction in secure world */
	uint32_t addrMetadataCount;	/* [IN] count of element in addrList */
	cmdqU32Ptr_t addrMetadatas;	/* [IN] array of cmdqSecAddrMetadataStruct */
	uint32_t addrMetadataMaxCount;	/*[Reserved] */

	uint64_t enginesNeedDAPC;
	uint64_t enginesNeedPortSecurity;

	/* [Reserved] This is for CMDQ driver usage itself. Not for client. */
	int32_t waitCookie;	/* task index in thread's tasklist. -1 for not in tasklist. */
	bool resetExecCnt;	/* reset HW thread in SWd */

	/* for MDP to copy HDCP version from srcHandle to dstHandle */
	uint32_t srcHandle;
	uint32_t dstHandle;
	enum CMDQ_DISP_MODE secMode;

} cmdqSecDataStruct;

struct cmdqCommandStruct {
	uint32_t scenario;	/* [IN] deprecated. will remove in the future. */
	uint32_t priority;	/* [IN] task schedule priority. this is NOT HW thread priority. */
	uint64_t engineFlag;	/* [IN] bit flag of engines used. */
	/* [IN] pointer to instruction buffer. Use 64-bit for compatibility. */
	/* This must point to an 64-bit aligned uint32_t array */
	cmdqU32Ptr_t pVABase;
	uint32_t blockSize;	/* [IN] size of instruction buffer, in bytes. */
	struct cmdqReadRegStruct regRequest;	/* [IN] request to read register values at the end of command */
	struct cmdqRegValueStruct regValue;	/* [OUT] register values of regRequest */
	struct cmdqReadAddressStruct readAddress;	/* [IN/OUT] physical addresses to read value */
	cmdqSecDataStruct secData;	/*[IN] secure execution data */
	uint32_t debugRegDump;	/* [IN] set to non-zero to enable register debug dump. */
	/* [Reserved] This is for CMDQ driver usage itself. */
	/* Not for client. Do not access this field from User Space */
	cmdqU32Ptr_t privateData;
};

struct cmdqSyncHandleHdcpStruct {
	uint32_t srcHandle;
	uint32_t dstHandle;
};

enum CMDQ_CAP_BITS {
	CMDQ_CAP_WFE = 0,	/* bit 0: TRUE if WFE instruction support is ready. FALSE if we need to POLL instead. */
};

/**
 * reply struct for cmdq_sec_cancel_error_task
 */

typedef struct {
	/* [OUT] */
	bool throwAEE;
	bool hasReset;
	int32_t irqFlag;
	uint32_t errInstr[2];
	uint32_t pc;
} cmdqSecCancelTaskResultStruct;
#ifdef CMDQ_OF_SUPPORT
typedef enum CMDQ_SUBSYS_ENUM {
	CMDQ_SUBSYSCLK_SYS_VDE,
	CMDQ_SUBSYSCLK_SYS_ISP,
	CMDQ_SUBSYSCLK_SYS_DIS,
	CMDQ_SUBSYSCLK_NUM
} CMDQ_SUBSYS_ENUM;
#endif

#endif				/* __CMDQ_DEF_H__ */
