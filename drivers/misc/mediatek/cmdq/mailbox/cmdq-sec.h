/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CMDQ_SEC_H__
#define __CMDQ_SEC_H__

#include <linux/kernel.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

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

	/* for ISP kernel driver */
	CMDQ_SCENARIO_ISP_RSC = 43,
	CMDQ_SCENARIO_ISP_FDVT = 44,
	CMDQ_SCENARIO_ISP_DPE = 45,

	CMDQ_MAX_SCENARIO_COUNT	/* ALWAYS keep at the end */
};

/* General Purpose Register */
enum cmdq_gpr_reg {
	/* Value Reg, we use 32-bit
	 * Address Reg, we use 64-bit
	 * Note that R0-R15 and P0-P7 actullay share same memory
	 * and R1 cannot be used.
	 */
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

struct cmdqSecAddrMetadataStruct {
	/* [IN]_d, index of instruction.
	 * Update its arg_b value to real PA/MVA in secure world
	 */
	uint32_t instrIndex;

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

	uint32_t type;		/* [IN] addr handle type */
	uint64_t baseHandle;	/* [IN]_h, secure address handle */
	/* [IN]_b, block offset from handle(PA) to current block(plane) */
	uint32_t blockOffset;
	uint32_t offset;	/* [IN]_b, buffser offset to secure handle */
	uint32_t size;		/* buffer size */
	uint32_t port;		/* hw port id (i.e. M4U port id) */
};

struct cmdqMetaBuf {
	uint64_t va;
	uint64_t size;
};

#define CMDQ_ISP_META_CNT	8

struct cmdqSecIspMeta {
	struct cmdqMetaBuf ispBufs[CMDQ_ISP_META_CNT];
	uint64_t CqSecHandle;
	uint32_t CqSecSize;
	uint32_t CqDesOft;
	uint32_t CqVirtOft;
	uint64_t TpipeSecHandle;
	uint32_t TpipeSecSize;
	uint32_t TpipeOft;
	uint64_t BpciHandle;
	uint64_t LsciHandle;
	uint64_t LceiHandle;
	uint64_t DepiHandle;
	uint64_t DmgiHandle;
};

struct cmdqSecDataStruct {
	bool is_secure;		/* [IN]true for secure command */

	/* address metadata, used to translate secure buffer PA
	 * related instruction in secure world
	 */
	uint32_t addrMetadataCount;	/* [IN] count of element in addrList */
	/* [IN] array of cmdqSecAddrMetadataStruct */
	uint64_t addrMetadatas;
	uint32_t addrMetadataMaxCount;	/*[Reserved] */

	enum CMDQ_SCENARIO_ENUM scenario;

	uint64_t enginesNeedDAPC;
	uint64_t enginesNeedPortSecurity;

	/* [Reserved] This is for CMDQ driver usage itself. Not for client.
	 * task index in thread's tasklist. -1 for not in tasklist.
	 */
	int32_t waitCookie;
	/* reset HW thread in SWd */
	bool resetExecCnt;

	/* ISP metadata for secure camera */
	struct cmdqSecIspMeta ispMeta;
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
	CMDQ_SAM_PH_2_MVA = 3,	/* protected handle to sec MVA */
};

s32 cmdq_sec_pkt_set_data(struct cmdq_pkt *pkt, const u64 dapc_engine,
	const u64 port_sec_engine, const enum CMDQ_SCENARIO_ENUM scenario);
s32 cmdq_sec_pkt_write_reg(struct cmdq_pkt *pkt, u32 addr, u64 base,
	const enum CMDQ_SEC_ADDR_METADATA_TYPE type,
	const u32 offset, const u32 size, const u32 port);
void cmdq_sec_dump_secure_data(struct cmdq_pkt *pkt);
#endif
