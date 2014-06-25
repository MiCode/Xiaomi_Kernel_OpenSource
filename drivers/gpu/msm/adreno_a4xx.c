/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "adreno.h"
#include "a4xx_reg.h"
#include "adreno_a3xx.h"
#include "adreno_a4xx.h"
#include "adreno_cp_parser.h"
#include "adreno_trace.h"

#define SP_TP_PWR_ON BIT(20)

/*
 * Set of registers to dump for A4XX on snapshot.
 * Registers in pairs - first value is the start offset, second
 * is the stop offset (inclusive)
 */

const unsigned int a4xx_registers[] = {
	/* RBBM */
	0x0000, 0x0002, 0x0004, 0x0021, 0x0023, 0x0024, 0x0026, 0x0026,
	0x0028, 0x002B, 0x002E, 0x0034, 0x0037, 0x0044, 0x0047, 0x0066,
	0x0068, 0x0095, 0x009C, 0x0170, 0x0174, 0x01AF,
	/* CP */
	0x0200, 0x0233, 0x0240, 0x0250, 0x04C0, 0x04DD, 0x0500, 0x050B,
	0x0578, 0x058F,
	/* VSC */
	0x0C00, 0x0C03, 0x0C08, 0x0C41, 0x0C50, 0x0C51,
	/* GRAS */
	0x0C80, 0x0C81, 0x0C88, 0x0C8F,
	/* RB */
	0x0CC0, 0x0CC0, 0x0CC4, 0x0CD2,
	/* PC */
	0x0D00, 0x0D0C, 0x0D10, 0x0D17, 0x0D20, 0x0D23,
	/* VFD */
	0x0E40, 0x0E4A,
	/* VPC */
	0x0E60, 0x0E61, 0x0E63, 0x0E68,
	/* UCHE */
	0x0E80, 0x0E84, 0x0E88, 0x0E95,
	/* GRAS CTX 0 */
	0x2000, 0x2004, 0x2008, 0x2067, 0x2070, 0x2078, 0x207B, 0x216E,
	/* PC CTX 0 */
	0x21C0, 0x21C6, 0x21D0, 0x21D0, 0x21D9, 0x21D9, 0x21E5, 0x21E7,
	/* VFD CTX 0 */
	0x2200, 0x2204, 0x2208, 0x22A9,
	/* GRAS CTX 1 */
	0x2400, 0x2404, 0x2408, 0x2467, 0x2470, 0x2478, 0x247B, 0x256E,
	/* PC CTX 1 */
	0x25C0, 0x25C6, 0x25D0, 0x25D0, 0x25D9, 0x25D9, 0x25E5, 0x25E7,
	/* VFD CTX 1 */
	0x2600, 0x2604, 0x2608, 0x26A9,
};

const unsigned int a4xx_registers_count = ARRAY_SIZE(a4xx_registers) / 2;

const unsigned int a4xx_sp_tp_registers[] = {
	/* SP */
	0x0EC0, 0x0ECF,
	/* TPL1 */
	0x0F00, 0x0F0B,
	/* SP CTX 0 */
	0x22C0, 0x22C1, 0x22C4, 0x22E5, 0x22E8, 0x22F8, 0x2300, 0x2306,
	0x230C, 0x2312, 0x2318, 0x2339, 0x2340, 0x2360,
	/* TPL1 CTX 0 */
	0x2380, 0x2382, 0x2384, 0x238F, 0x23A0, 0x23A6,
	/* SP CTX 1 */+
	0x26C0, 0x26C1, 0x26C4, 0x26E5, 0x26E8, 0x26F8, 0x2700, 0x2706,
	0x270C, 0x2712, 0x2718, 0x2739, 0x2740, 0x2760,
	/* TPL1 CTX 1 */
	0x2780, 0x2782, 0x2784, 0x278F, 0x27A0, 0x27A6,
};

const unsigned int a4xx_sp_tp_registers_count =
			ARRAY_SIZE(a4xx_sp_tp_registers) / 2;

const unsigned int a4xx_xpu_registers[] = {
	/* XPU */
	0x2C00, 0x2C01, 0x2C10, 0x2C10, 0x2C12, 0x2C16, 0x2C1D, 0x2C20,
	0x2C28, 0x2C28, 0x2C30, 0x2C30, 0x2C32, 0x2C36, 0x2C40, 0x2C40,
	0x2C50, 0x2C50, 0x2C52, 0x2C56, 0x2C80, 0x2C80, 0x2C94, 0x2C95,
};

const unsigned int a4xx_xpu_reg_cnt =
				ARRAY_SIZE(a4xx_xpu_registers)/2;

static const unsigned int a4xx_vbif_ver_20000000_registers[] = {
	/* VBIF version 0x20000000 & IOMMU V1 */
	0x3000, 0x3007, 0x300C, 0x3014, 0x3018, 0x301D, 0x3020, 0x3022,
	0x3024, 0x3026, 0x3028, 0x302A, 0x302C, 0x302D, 0x3030, 0x3031,
	0x3034, 0x3036, 0x3038, 0x3038, 0x303C, 0x303D, 0x3040, 0x3040,
	0x3049, 0x3049, 0x3058, 0x3058, 0x305B, 0x3061, 0x3064, 0x3068,
	0x306C, 0x306D, 0x3080, 0x3088, 0x308B, 0x308C, 0x3090, 0x3094,
	0x3098, 0x3098, 0x309C, 0x309C, 0x30C0, 0x30C0, 0x30C8, 0x30C8,
	0x30D0, 0x30D0, 0x30D8, 0x30D8, 0x30E0, 0x30E0, 0x3100, 0x3100,
	0x3108, 0x3108, 0x3110, 0x3110, 0x3118, 0x3118, 0x3120, 0x3120,
	0x3124, 0x3125, 0x3129, 0x3129, 0x3131, 0x3131, 0x330C, 0x330C,
	0x3310, 0x3310, 0x3400, 0x3401, 0x3410, 0x3410, 0x3412, 0x3416,
	0x341D, 0x3420, 0x3428, 0x3428, 0x3430, 0x3430, 0x3432, 0x3436,
	0x3440, 0x3440, 0x3450, 0x3450, 0x3452, 0x3456, 0x3480, 0x3480,
	0x3494, 0x3495, 0x4000, 0x4000, 0x4002, 0x4002, 0x4004, 0x4004,
	0x4008, 0x400A, 0x400C, 0x400D, 0x400F, 0x4012, 0x4014, 0x4016,
	0x401D, 0x401D, 0x4020, 0x4027, 0x4060, 0x4062, 0x4200, 0x4200,
	0x4300, 0x4300, 0x4400, 0x4400, 0x4500, 0x4500, 0x4800, 0x4802,
	0x480F, 0x480F, 0x4811, 0x4811, 0x4813, 0x4813, 0x4815, 0x4816,
	0x482B, 0x482B, 0x4857, 0x4857, 0x4883, 0x4883, 0x48AF, 0x48AF,
	0x48C5, 0x48C5, 0x48E5, 0x48E5, 0x4905, 0x4905, 0x4925, 0x4925,
	0x4945, 0x4945, 0x4950, 0x4950, 0x495B, 0x495B, 0x4980, 0x498E,
	0x4B00, 0x4B00, 0x4C00, 0x4C00, 0x4D00, 0x4D00, 0x4E00, 0x4E00,
	0x4E80, 0x4E80, 0x4F00, 0x4F00, 0x4F08, 0x4F08, 0x4F10, 0x4F10,
	0x4F18, 0x4F18, 0x4F20, 0x4F20, 0x4F30, 0x4F30, 0x4F60, 0x4F60,
	0x4F80, 0x4F81, 0x4F88, 0x4F89, 0x4FEE, 0x4FEE, 0x4FF3, 0x4FF3,
	0x6000, 0x6001, 0x6008, 0x600F, 0x6014, 0x6016, 0x6018, 0x601B,
	0x61FD, 0x61FD, 0x623C, 0x623C, 0x6380, 0x6380, 0x63A0, 0x63A0,
	0x63C0, 0x63C1, 0x63C8, 0x63C9, 0x63D0, 0x63D4, 0x63D6, 0x63D6,
	0x63EE, 0x63EE, 0x6400, 0x6401, 0x6408, 0x640F, 0x6414, 0x6416,
	0x6418, 0x641B, 0x65FD, 0x65FD, 0x663C, 0x663C, 0x6780, 0x6780,
	0x67A0, 0x67A0, 0x67C0, 0x67C1, 0x67C8, 0x67C9, 0x67D0, 0x67D4,
	0x67D6, 0x67D6, 0x67EE, 0x67EE, 0x6800, 0x6801, 0x6808, 0x680F,
	0x6814, 0x6816, 0x6818, 0x681B, 0x69FD, 0x69FD, 0x6A3C, 0x6A3C,
	0x6B80, 0x6B80, 0x6BA0, 0x6BA0, 0x6BC0, 0x6BC1, 0x6BC8, 0x6BC9,
	0x6BD0, 0x6BD4, 0x6BD6, 0x6BD6, 0x6BEE, 0x6BEE,
};

static const unsigned int a4xx_vbif_ver_20050000_registers[] = {
	/* VBIF version 0x20050000*/
	0x3000, 0x3007, 0x302C, 0x302C, 0x3030, 0x3030, 0x3034, 0x3036,
	0x3038, 0x3038, 0x303C, 0x303D, 0x3040, 0x3040, 0x3049, 0x3049,
	0x3058, 0x3058, 0x305B, 0x3061, 0x3064, 0x3068, 0x306C, 0x306D,
	0x3080, 0x3088, 0x308B, 0x308C, 0x3090, 0x3094, 0x3098, 0x3098,
	0x309C, 0x309C, 0x30C0, 0x30C0, 0x30C8, 0x30C8, 0x30D0, 0x30D0,
	0x30D8, 0x30D8, 0x30E0, 0x30E0, 0x3100, 0x3100, 0x3108, 0x3108,
	0x3110, 0x3110, 0x3118, 0x3118, 0x3120, 0x3120, 0x3124, 0x3125,
	0x3129, 0x3129, 0x340C, 0x340C, 0x3410, 0x3410,
};

const struct adreno_vbif_snapshot_registers a4xx_vbif_snapshot_registers[] = {
	{ 0x20000000, a4xx_vbif_ver_20000000_registers,
				ARRAY_SIZE(a4xx_vbif_ver_20000000_registers)/2},
	{ 0x20020000, a4xx_vbif_ver_20000000_registers,
				ARRAY_SIZE(a4xx_vbif_ver_20000000_registers)/2},
	{ 0x20050000, a4xx_vbif_ver_20050000_registers,
				ARRAY_SIZE(a4xx_vbif_ver_20050000_registers)/2},
};

const unsigned int a4xx_vbif_snapshot_reg_cnt =
				ARRAY_SIZE(a4xx_vbif_snapshot_registers);

/*
 * Define registers for a4xx that contain addresses used by the
 * cp parser logic
 */
const unsigned int a4xx_cp_addr_regs[ADRENO_CP_ADDR_MAX] = {
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_0,
				A4XX_VSC_PIPE_DATA_ADDRESS_0),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_0,
				A4XX_VSC_PIPE_DATA_LENGTH_0),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_1,
				A4XX_VSC_PIPE_DATA_ADDRESS_1),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_1,
				A4XX_VSC_PIPE_DATA_LENGTH_1),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_2,
				A4XX_VSC_PIPE_DATA_ADDRESS_2),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_2,
				A4XX_VSC_PIPE_DATA_LENGTH_2),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_3,
				A4XX_VSC_PIPE_DATA_ADDRESS_3),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_3,
				A4XX_VSC_PIPE_DATA_LENGTH_3),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_4,
				A4XX_VSC_PIPE_DATA_ADDRESS_4),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_4,
				A4XX_VSC_PIPE_DATA_LENGTH_4),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_5,
				A4XX_VSC_PIPE_DATA_ADDRESS_5),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_5,
				A4XX_VSC_PIPE_DATA_LENGTH_5),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_6,
				A4XX_VSC_PIPE_DATA_ADDRESS_6),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_6,
				A4XX_VSC_PIPE_DATA_LENGTH_6),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_7,
				A4XX_VSC_PIPE_DATA_ADDRESS_7),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_7,
				A4XX_VSC_PIPE_DATA_LENGTH_7),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_0,
				A4XX_VFD_FETCH_INSTR_1_0),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_1,
				A4XX_VFD_FETCH_INSTR_1_1),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_2,
				A4XX_VFD_FETCH_INSTR_1_2),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_3,
				A4XX_VFD_FETCH_INSTR_1_3),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_4,
				A4XX_VFD_FETCH_INSTR_1_4),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_5,
				A4XX_VFD_FETCH_INSTR_1_5),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_6,
				A4XX_VFD_FETCH_INSTR_1_6),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_7,
				A4XX_VFD_FETCH_INSTR_1_7),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_8,
				A4XX_VFD_FETCH_INSTR_1_8),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_9,
				A4XX_VFD_FETCH_INSTR_1_9),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_10,
				A4XX_VFD_FETCH_INSTR_1_10),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_11,
				A4XX_VFD_FETCH_INSTR_1_11),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_12,
				A4XX_VFD_FETCH_INSTR_1_12),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_13,
				A4XX_VFD_FETCH_INSTR_1_13),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_14,
				A4XX_VFD_FETCH_INSTR_1_14),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_15,
				A4XX_VFD_FETCH_INSTR_1_15),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_16,
				A4XX_VFD_FETCH_INSTR_1_16),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_17,
				A4XX_VFD_FETCH_INSTR_1_17),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_18,
				A4XX_VFD_FETCH_INSTR_1_18),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_19,
				A4XX_VFD_FETCH_INSTR_1_19),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_20,
				A4XX_VFD_FETCH_INSTR_1_20),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_21,
				A4XX_VFD_FETCH_INSTR_1_21),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_22,
				A4XX_VFD_FETCH_INSTR_1_22),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_23,
				A4XX_VFD_FETCH_INSTR_1_23),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_24,
				A4XX_VFD_FETCH_INSTR_1_24),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_25,
				A4XX_VFD_FETCH_INSTR_1_25),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_26,
				A4XX_VFD_FETCH_INSTR_1_26),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_27,
				A4XX_VFD_FETCH_INSTR_1_27),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_28,
				A4XX_VFD_FETCH_INSTR_1_28),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_29,
				A4XX_VFD_FETCH_INSTR_1_29),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_30,
				A4XX_VFD_FETCH_INSTR_1_30),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_31,
				A4XX_VFD_FETCH_INSTR_1_31),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_VSC_SIZE_ADDRESS,
				A4XX_VSC_SIZE_ADDRESS),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_SP_VS_PVT_MEM_ADDR,
				A4XX_SP_VS_PVT_MEM_ADDR),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_SP_FS_PVT_MEM_ADDR,
				A4XX_SP_FS_PVT_MEM_ADDR),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_SP_VS_OBJ_START_REG,
				A4XX_SP_VS_OBJ_START),
	ADRENO_REG_DEFINE(ADRENO_CP_ADDR_SP_FS_OBJ_START_REG,
				A4XX_SP_FS_OBJ_START),
	ADRENO_REG_DEFINE(ADRENO_CP_UCHE_INVALIDATE0,
				A4XX_UCHE_INVALIDATE0),
	ADRENO_REG_DEFINE(ADRENO_CP_UCHE_INVALIDATE1,
				A4XX_UCHE_INVALIDATE1),
};

static const struct adreno_vbif_data a405_vbif[] = {
	{ A4XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x00000003 },
	{0, 0},
};

static const struct adreno_vbif_data a420_vbif[] = {
	{ A4XX_VBIF_ABIT_SORT, 0x0001001F },
	{ A4XX_VBIF_ABIT_SORT_CONF, 0x000000A4 },
	{ A4XX_VBIF_GATE_OFF_WRREQ_EN, 0x00000001 },
	{ A4XX_VBIF_IN_RD_LIM_CONF0, 0x18181818 },
	{ A4XX_VBIF_IN_RD_LIM_CONF1, 0x00000018 },
	{ A4XX_VBIF_IN_WR_LIM_CONF0, 0x18181818 },
	{ A4XX_VBIF_IN_WR_LIM_CONF1, 0x00000018 },
	{ A4XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x00000003 },
	{0, 0},
};

static const struct adreno_vbif_data a430_vbif[] = {
	{ A4XX_VBIF_GATE_OFF_WRREQ_EN, 0x00000001 },
	{ A4XX_VBIF_IN_RD_LIM_CONF0, 0x18181818 },
	{ A4XX_VBIF_IN_RD_LIM_CONF1, 0x00000018 },
	{ A4XX_VBIF_IN_WR_LIM_CONF0, 0x18181818 },
	{ A4XX_VBIF_IN_WR_LIM_CONF1, 0x00000018 },
	{ A4XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x00000003 },
	{0, 0},
};

static const struct adreno_vbif_platform a4xx_vbif_platforms[] = {
	{ adreno_is_a405, a405_vbif },
	{ adreno_is_a420, a420_vbif },
	{ adreno_is_a430, a430_vbif },
};

/*
 * a4xx_is_sptp_idle() - A430 SP/TP should be off to be considered idle
 * @adreno_dev: The adreno device pointer
 */
static bool a4xx_is_sptp_idle(struct adreno_device *adreno_dev)
{
	unsigned int reg;
	struct kgsl_device *device = &adreno_dev->dev;
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_SPTP_PC))
		return true;

	/* If SP/TP pc isn't enabled, don't worry about power */
	kgsl_regread(device, A4XX_CP_POWER_COLLAPSE_CNTL, &reg);
	if (!(reg & 0x10))
		return true;

	/* Check that SP/TP is off */
	kgsl_regread(device, A4XX_RBBM_POWER_STATUS, &reg);
	return !(reg & SP_TP_PWR_ON);
}

/*
 * a4xx_regulator_enable() - Enable any necessary HW regulators
 * @adreno_dev: The adreno device pointer
 *
 * Some HW blocks may need their regulators explicitly enabled
 * on a restart.  Clocks must be on during this call.
 */
static void a4xx_regulator_enable(struct adreno_device *adreno_dev)
{
	unsigned int reg;
	struct kgsl_device *device = &adreno_dev->dev;
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_SPTP_PC))
		return;

	/* Set the default register values; set SW_COLLAPSE to 0 */
	kgsl_regwrite(device, A4XX_RBBM_POWER_CNTL_IP, 0x778000);
	do {
		udelay(5);
		kgsl_regread(device, A4XX_RBBM_POWER_STATUS, &reg);
	} while (!(reg & SP_TP_PWR_ON));
	trace_adreno_sp_tp((unsigned long) __builtin_return_address(0));
}

/*
 * a4xx_regulator_disable() - Disable any necessary HW regulators
 * @adreno_dev: The adreno device pointer
 *
 * Some HW blocks may need their regulators explicitly disabled
 * on a power down to prevent current spikes.  Clocks must be on
 * during this call.
 */
static void a4xx_regulator_disable(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_SPTP_PC))
		return;

	/* Set the default register values; set SW_COLLAPSE to 1 */
	kgsl_regwrite(device, A4XX_RBBM_POWER_CNTL_IP, 0x778001);
	trace_adreno_sp_tp((unsigned long) __builtin_return_address(0));
}

/*
 * a4xx_enable_pc() - Enable the SP/TP block power collapse
 * @adreno_dev: The adreno device pointer
 */
static void a4xx_enable_pc(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_SPTP_PC) ||
		!test_bit(ADRENO_SPTP_PC_CTRL, &adreno_dev->pwrctrl_flag))
		return;

	kgsl_regwrite(device, A4XX_CP_POWER_COLLAPSE_CNTL, 0x00400010);
	trace_adreno_sp_tp((unsigned long) __builtin_return_address(0));
};

/*
 * a4xx_enable_hwcg() - Program the clock control registers
 * @device: The adreno device pointer
 */
static void a4xx_enable_hwcg(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_TP0, 0x02222202);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_TP1, 0x02222202);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_TP2, 0x02222202);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_TP3, 0x02222202);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_TP0, 0x00002222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_TP1, 0x00002222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_TP2, 0x00002222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_TP3, 0x00002222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_HYST_TP0, 0x0E739CE7);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_HYST_TP1, 0x0E739CE7);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_HYST_TP2, 0x0E739CE7);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_HYST_TP3, 0x0E739CE7);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_TP0, 0x00111111);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_TP1, 0x00111111);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_TP2, 0x00111111);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_TP3, 0x00111111);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_SP0, 0x22222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_SP1, 0x22222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_SP2, 0x22222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_SP3, 0x22222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_SP0, 0x00222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_SP1, 0x00222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_SP2, 0x00222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_SP3, 0x00222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_HYST_SP0, 0x00000104);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_HYST_SP1, 0x00000104);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_HYST_SP2, 0x00000104);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_HYST_SP3, 0x00000104);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_SP0, 0x00000081);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_SP1, 0x00000081);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_SP2, 0x00000081);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_SP3, 0x00000081);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_UCHE, 0x22222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_UCHE, 0x02222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL3_UCHE, 0x00000000);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL4_UCHE, 0x00000000);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_HYST_UCHE, 0x00004444);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_UCHE, 0x00001112);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_RB0, 0x22222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_RB1, 0x22222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_RB2, 0x22222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_RB3, 0x22222222);
	/* Disable L1 clocking in A420 due to CCU issues with it */
	if (adreno_is_a420(adreno_dev)) {
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_RB0, 0x00002020);
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_RB1, 0x00002020);
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_RB2, 0x00002020);
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_RB3, 0x00002020);
	} else {
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_RB0, 0x00022020);
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_RB1, 0x00022020);
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_RB2, 0x00022020);
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_RB3, 0x00022020);
	}
	/* No CCU for A405 */
	if (!adreno_is_a405(adreno_dev)) {
		kgsl_regwrite(device,
			A4XX_RBBM_CLOCK_CTL_MARB_CCU0, 0x00000922);
		kgsl_regwrite(device,
			A4XX_RBBM_CLOCK_CTL_MARB_CCU1, 0x00000922);
		kgsl_regwrite(device,
			A4XX_RBBM_CLOCK_CTL_MARB_CCU2, 0x00000922);
		kgsl_regwrite(device,
			A4XX_RBBM_CLOCK_CTL_MARB_CCU3, 0x00000922);
		kgsl_regwrite(device,
			A4XX_RBBM_CLOCK_HYST_RB_MARB_CCU0, 0x00000000);
		kgsl_regwrite(device,
			A4XX_RBBM_CLOCK_HYST_RB_MARB_CCU1, 0x00000000);
		kgsl_regwrite(device,
			A4XX_RBBM_CLOCK_HYST_RB_MARB_CCU2, 0x00000000);
		kgsl_regwrite(device,
			A4XX_RBBM_CLOCK_HYST_RB_MARB_CCU3, 0x00000000);
		kgsl_regwrite(device,
				A4XX_RBBM_CLOCK_DELAY_RB_MARB_CCU_L1_0,
				0x00000001);
		kgsl_regwrite(device,
				A4XX_RBBM_CLOCK_DELAY_RB_MARB_CCU_L1_1,
				0x00000001);
		kgsl_regwrite(device,
				A4XX_RBBM_CLOCK_DELAY_RB_MARB_CCU_L1_2,
				0x00000001);
		kgsl_regwrite(device,
				A4XX_RBBM_CLOCK_DELAY_RB_MARB_CCU_L1_3,
				0x00000001);
	}
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_MODE_GPC, 0x02222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_HYST_GPC, 0x04100104);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_GPC, 0x00022222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_COM_DCOM, 0x00000022);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_HYST_COM_DCOM, 0x0000010F);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_COM_DCOM, 0x00000022);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_TSE_RAS_RBBM, 0x00222222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM, 0x00004104);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM, 0x00000222);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_HLSQ , 0x00000000);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_HYST_HLSQ, 0x00000000);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_HLSQ, 0x00020000);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL, 0xAAAAAAAA);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2, 0);
}

/**
 * a4xx_protect_init() - Initializes register protection on a4xx
 * @device: Pointer to the device structure
 * Performs register writes to enable protected access to sensitive
 * registers
 */
static void a4xx_protect_init(struct kgsl_device *device)
{
	int index = 0;
	struct kgsl_protected_registers *iommu_regs;

	/* enable access protection to privileged registers */
	kgsl_regwrite(device, A4XX_CP_PROTECT_CTRL, 0x00000007);
	/* RBBM registers */
	adreno_set_protected_registers(device, &index, 0x4, 2);
	adreno_set_protected_registers(device, &index, 0x8, 3);
	adreno_set_protected_registers(device, &index, 0x10, 4);
	adreno_set_protected_registers(device, &index, 0x20, 5);
	adreno_set_protected_registers(device, &index, 0x40, 6);
	adreno_set_protected_registers(device, &index, 0x80, 4);

	/* Content protection registers */
	if (kgsl_mmu_is_secured(&device->mmu)) {
		adreno_set_protected_registers(device, &index,
			   A4XX_RBBM_SECVID_TSB_TRUSTED_BASE, 3);
		adreno_set_protected_registers(device, &index,
			   A4XX_RBBM_SECVID_TRUST_CONTROL, 1);
	}

	/* CP registers */
	adreno_set_protected_registers(device, &index, 0x200, 7);
	adreno_set_protected_registers(device, &index, 0x580, 4);

	/* RB registers */
	adreno_set_protected_registers(device, &index, 0xCC0, 0);

	/* HLSQ registers */
	adreno_set_protected_registers(device, &index, 0xE00, 0);

	/* VPC registers */
	adreno_set_protected_registers(device, &index, 0xE60, 1);

	/* SMMU registers */
	iommu_regs = kgsl_mmu_get_prot_regs(&device->mmu);
	if (iommu_regs)
		adreno_set_protected_registers(device, &index,
				iommu_regs->base, iommu_regs->range);
}

static void a4xx_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;

	adreno_vbif_start(device, a4xx_vbif_platforms,
			ARRAY_SIZE(a4xx_vbif_platforms));
	/* Make all blocks contribute to the GPU BUSY perf counter */
	kgsl_regwrite(device, A4XX_RBBM_GPU_BUSY_MASKED, 0xFFFFFFFF);

	/* Tune the hystersis counters for SP and CP idle detection */
	kgsl_regwrite(device, A4XX_RBBM_SP_HYST_CNT, 0x10);
	kgsl_regwrite(device, A4XX_RBBM_WAIT_IDLE_CLOCKS_CTL, 0x10);
	if (adreno_is_a430(adreno_dev))
		kgsl_regwrite(device, A4XX_RBBM_WAIT_IDLE_CLOCKS_CTL2, 0x30);

	/*
	 * Enable the RBBM error reporting bits.  This lets us get
	 * useful information on failure
	 */

	kgsl_regwrite(device, A4XX_RBBM_AHB_CTL0, 0x00000001);

	/* Enable AHB error reporting */
	kgsl_regwrite(device, A4XX_RBBM_AHB_CTL1, 0xA6FFFFFF);

	/* Turn on the power counters */
	kgsl_regwrite(device, A4XX_RBBM_RBBM_CTL, 0x00000030);

	/*
	 * Turn on hang detection - this spews a lot of useful information
	 * into the RBBM registers on a hang
	 */

	kgsl_regwrite(device, A4XX_RBBM_INTERFACE_HANG_INT_CTL,
			(1 << 30) | 0xFFFF);

	/* Set the GMEM/OCMEM base address for A4XX */
	kgsl_regwrite(device, A4XX_RB_GMEM_BASE_ADDR,
			(unsigned int)(adreno_dev->gmem_base >> 14));

	/* Turn on performance counters */
	kgsl_regwrite(device, A4XX_RBBM_PERFCTR_CTL, 0x01);
	/* Turn on the GPU busy counter and let it run free */
	memset(&adreno_dev->busy_data, 0, sizeof(adreno_dev->busy_data));

	/* Disable L2 bypass to avoid UCHE out of bounds errors */
	kgsl_regwrite(device, UCHE_TRAP_BASE_LO, 0xffff0000);
	kgsl_regwrite(device, UCHE_TRAP_BASE_HI, 0xffff0000);

	/* On A420 cores turn on SKIP_IB2_DISABLE in addition to the default */
	kgsl_regwrite(device, A4XX_CP_DEBUG, A4XX_CP_DEBUG_DEFAULT |
			(adreno_is_a420(adreno_dev) ? (1 << 29) : 0));

	/* On A430 enable SP regfile sleep for power savings */
	if (!adreno_is_a420(adreno_dev)) {
		kgsl_regwrite(device, A4XX_RBBM_SP_REGFILE_SLEEP_CNTL_0,
				0x00000441);
		kgsl_regwrite(device, A4XX_RBBM_SP_REGFILE_SLEEP_CNTL_1,
				0x00000441);
	}

	a4xx_enable_hwcg(device);
	/*
	 * For A420 set RBBM_CLOCK_DELAY_HLSQ.CGC_HLSQ_TP_EARLY_CYC >= 2
	 * due to timing issue with HLSQ_TP_CLK_EN
	 */
	if (adreno_is_a420(adreno_dev)) {
		unsigned int val;
		kgsl_regread(device, A4XX_RBBM_CLOCK_DELAY_HLSQ, &val);
		val &= ~A4XX_CGC_HLSQ_TP_EARLY_CYC_MASK;
		val |= 2 << A4XX_CGC_HLSQ_TP_EARLY_CYC_SHIFT;
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_DELAY_HLSQ, val);
	}

	a4xx_protect_init(device);
}

int a4xx_perfcounter_enable_vbif(struct adreno_device *adreno_dev,
				unsigned int counter,
				unsigned int countable)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_register *reg;

	if (counters == NULL ||
		(counter > 3 || countable > A4XX_VBIF_PERF_CNT_SEL_MASK))
		return -EINVAL;

	reg = &counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF].regs[counter];
	kgsl_regwrite(device, reg->select - A4XX_VBIF_PERF_CLR_REG_SEL_OFF, 1);
	kgsl_regwrite(device, reg->select - A4XX_VBIF_PERF_CLR_REG_SEL_OFF, 0);
	kgsl_regwrite(device, reg->select,
			countable & A4XX_VBIF_PERF_CNT_SEL_MASK);
	/* enable reg is 8 DWORDS before select reg */
	kgsl_regwrite(device, reg->select - A4XX_VBIF_PERF_EN_REG_SEL_OFF, 1);
	/* reset the saved value field */
	counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF].regs[counter].value = 0;
	return 0;
}

uint64_t a4xx_perfcounter_read_vbif(struct adreno_device *adreno_dev,
					unsigned int counter)
{
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_register *reg;
	unsigned int lo = 0, hi = 0;

	if (counters == NULL || counter > 3)
		return 0;

	reg = &counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF].regs[counter];

	kgsl_regread(&adreno_dev->dev, reg->offset, &lo);
	kgsl_regread(&adreno_dev->dev, reg->offset_hi, &hi);

	return ((((uint64_t) hi) << 32) | lo)
		+ counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF]
				.regs[counter].value;
}

int a4xx_perfcounter_enable_vbif_pwr(struct adreno_device *adreno_dev,
					unsigned int counter)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_register *reg;

	if (counters == NULL || counter > 3)
		return -EINVAL;

	reg = &counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF_PWR].regs[counter];

	/*
	 * since power registers dont have select register, the enable
	 * reg is stored here
	 */
	kgsl_regwrite(device, reg->select + A4XX_VBIF_PERF_PWR_CLR_REG_EN_OFF,
			1);
	kgsl_regwrite(device, reg->select + A4XX_VBIF_PERF_PWR_CLR_REG_EN_OFF,
			0);
	kgsl_regwrite(device, reg->select, 1);
	/* reset the saved value field */
	counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF_PWR]
				.regs[counter].value = 0;
	return 0;
}

uint64_t a4xx_perfcounter_read_vbif_pwr(struct adreno_device *adreno_dev,
						unsigned int counter)
{
	struct adreno_perfcounters *counters = ADRENO_PERFCOUNTERS(adreno_dev);
	struct adreno_perfcount_register *reg;
	unsigned int lo = 0, hi = 0;

	 if (counters == NULL || counter > 3)
		return 0;

	reg = &counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF_PWR].regs[counter];

	kgsl_regread(&adreno_dev->dev, reg->offset, &lo);
	kgsl_regread(&adreno_dev->dev, reg->offset_hi, &hi);

	return ((((uint64_t) hi) << 32) | lo)
		+ counters->groups[KGSL_PERFCOUNTER_GROUP_VBIF_PWR]
						.regs[counter].value;
}

static void a4xx_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = &adreno_dev->dev;

	switch (bit) {
	case A4XX_INT_RBBM_ETS_MS_TIMEOUT:
		KGSL_DRV_CRIT_RATELIMIT(device,
				"RBBM: ME master split timeout\n");
		break;
	case A4XX_INT_RBBM_ASYNC_OVERFLOW:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: ASYNC overflow\n");
		break;
	case A4XX_INT_RBBM_GPC_ERR:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: GPC error\n");
		break;
	case A4XX_INT_CP_OPCODE_ERR:
		KGSL_DRV_CRIT(device, "ringbuffer opcode error interrupt\n");
		return;
	case A4XX_INT_RBBM_ATB_BUS_OVERFLOW:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: ATB bus overflow\n");
		break;
	case A4XX_INT_RBBM_DPM_CALC_ERR:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: dpm calc error\n");
		break;
	case A4XX_INT_RBBM_DPM_EPOCH_ERR:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: dpm epoch error\n");
		break;
	case A4XX_INT_RBBM_DPM_THERMAL_YELLOW_ERR:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: dpm thermal yellow\n");
		break;
	case A4XX_INT_RBBM_DPM_THERMAL_RED_ERR:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: dpm thermal red\n");
		break;
	default:
		return;
	}
}

/* Register offset defines for A4XX, in order of enum adreno_regs */
static unsigned int a4xx_register_offsets[ADRENO_REG_REGISTER_MAX] = {
	ADRENO_REG_DEFINE(ADRENO_REG_CP_DEBUG, A4XX_CP_DEBUG),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_RAM_WADDR, A4XX_CP_ME_RAM_WADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_RAM_DATA, A4XX_CP_ME_RAM_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PFP_UCODE_DATA, A4XX_CP_PFP_UCODE_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PFP_UCODE_ADDR, A4XX_CP_PFP_UCODE_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_WFI_PEND_CTR, A4XX_CP_WFI_PEND_CTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE, A4XX_CP_RB_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR_ADDR, A4XX_CP_RB_RPTR_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR, A4XX_CP_RB_RPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_WPTR, A4XX_CP_RB_WPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PROTECT_CTRL, A4XX_CP_PROTECT_CTRL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_CNTL, A4XX_CP_ME_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_CNTL, A4XX_CP_RB_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE, A4XX_CP_IB1_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BUFSZ, A4XX_CP_IB1_BUFSZ),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE, A4XX_CP_IB2_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BUFSZ, A4XX_CP_IB2_BUFSZ),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_TIMESTAMP, A4XX_CP_SCRATCH_REG0),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_RAM_RADDR, A4XX_CP_ME_RAM_RADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ROQ_ADDR, A4XX_CP_ROQ_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ROQ_DATA, A4XX_CP_ROQ_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MERCIU_ADDR, A4XX_CP_MERCIU_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MERCIU_DATA, A4XX_CP_MERCIU_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MERCIU_DATA2, A4XX_CP_MERCIU_DATA2),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MEQ_ADDR, A4XX_CP_MEQ_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_MEQ_DATA, A4XX_CP_MEQ_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_HW_FAULT, A4XX_CP_HW_FAULT),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PROTECT_STATUS, A4XX_CP_PROTECT_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_SCRATCH_ADDR, A4XX_CP_SCRATCH_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_SCRATCH_UMSK, A4XX_CP_SCRATCH_UMASK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS, A4XX_RBBM_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_CTL, A4XX_RBBM_PERFCTR_CTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD0,
					A4XX_RBBM_PERFCTR_LOAD_CMD0),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD1,
					A4XX_RBBM_PERFCTR_LOAD_CMD1),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD2,
				A4XX_RBBM_PERFCTR_LOAD_CMD2),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_PWR_1_LO,
					A4XX_RBBM_PERFCTR_PWR_1_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_MASK, A4XX_RBBM_INT_0_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_STATUS, A4XX_RBBM_INT_0_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_AHB_ERROR_STATUS,
					A4XX_RBBM_AHB_ERROR_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_AHB_CMD, A4XX_RBBM_AHB_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_CLOCK_CTL, A4XX_RBBM_CLOCK_CTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_AHB_ME_SPLIT_STATUS,
			A4XX_RBBM_AHB_ME_SPLIT_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_AHB_PFP_SPLIT_STATUS,
			A4XX_RBBM_AHB_PFP_SPLIT_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_VPC_DEBUG_RAM_SEL,
					A4XX_VPC_DEBUG_RAM_SEL),
	ADRENO_REG_DEFINE(ADRENO_REG_VPC_DEBUG_RAM_READ,
					A4XX_VPC_DEBUG_RAM_READ),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_CLEAR_CMD,
				A4XX_RBBM_INT_CLEAR_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_VSC_PIPE_DATA_ADDRESS_0,
				A4XX_VSC_PIPE_DATA_ADDRESS_0),
	ADRENO_REG_DEFINE(ADRENO_REG_VSC_PIPE_DATA_LENGTH_7,
					A4XX_VSC_PIPE_DATA_LENGTH_7),
	ADRENO_REG_DEFINE(ADRENO_REG_VSC_SIZE_ADDRESS, A4XX_VSC_SIZE_ADDRESS),
	ADRENO_REG_DEFINE(ADRENO_REG_VFD_CONTROL_0, A4XX_VFD_CONTROL_0),
	ADRENO_REG_DEFINE(ADRENO_REG_VFD_FETCH_INSTR_0_0,
					A4XX_VFD_FETCH_INSTR_0_0),
	ADRENO_REG_DEFINE(ADRENO_REG_VFD_FETCH_INSTR_1_F,
					A4XX_VFD_FETCH_INSTR_1_31),
	ADRENO_REG_DEFINE(ADRENO_REG_SP_VS_PVT_MEM_ADDR_REG,
				A4XX_SP_VS_PVT_MEM_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_SP_FS_PVT_MEM_ADDR_REG,
				A4XX_SP_FS_PVT_MEM_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_SP_VS_OBJ_START_REG,
				A4XX_SP_VS_OBJ_START),
	ADRENO_REG_DEFINE(ADRENO_REG_SP_FS_OBJ_START_REG,
				A4XX_SP_FS_OBJ_START),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_RBBM_CTL, A4XX_RBBM_RBBM_CTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SW_RESET_CMD, A4XX_RBBM_SW_RESET_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_UCHE_INVALIDATE0, A4XX_UCHE_INVALIDATE0),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_LO,
				A4XX_RBBM_PERFCTR_LOAD_VALUE_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_HI,
				A4XX_RBBM_PERFCTR_LOAD_VALUE_HI),
};

const struct adreno_reg_offsets a4xx_reg_offsets = {
	.offsets = a4xx_register_offsets,
	.offset_0 = ADRENO_REG_REGISTER_MAX,
};

static struct adreno_perfcount_register a4xx_perfcounters_cp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CP_0_LO,
		A4XX_RBBM_PERFCTR_CP_0_HI, 0, A4XX_CP_PERFCTR_CP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CP_1_LO,
		A4XX_RBBM_PERFCTR_CP_1_HI, 0, A4XX_CP_PERFCTR_CP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CP_2_LO,
		A4XX_RBBM_PERFCTR_CP_2_HI, 0, A4XX_CP_PERFCTR_CP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CP_3_LO,
		A4XX_RBBM_PERFCTR_CP_3_HI, 0, A4XX_CP_PERFCTR_CP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CP_4_LO,
		A4XX_RBBM_PERFCTR_CP_4_HI, 0, A4XX_CP_PERFCTR_CP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CP_5_LO,
		A4XX_RBBM_PERFCTR_CP_5_HI, 0, A4XX_CP_PERFCTR_CP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CP_6_LO,
		A4XX_RBBM_PERFCTR_CP_6_HI, 0, A4XX_CP_PERFCTR_CP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CP_7_LO,
		A4XX_RBBM_PERFCTR_CP_7_HI, 0, A4XX_CP_PERFCTR_CP_SEL_7 },
};

/*
 * Special list of CP registers for 420 to account for flaws.  This array is
 * inserted into the tables during perfcounter init
 */
static struct adreno_perfcount_register a420_perfcounters_cp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CP_0_LO,
		A4XX_RBBM_PERFCTR_CP_0_HI, 0, A4XX_CP_PERFCTR_CP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CP_1_LO,
		A4XX_RBBM_PERFCTR_CP_1_HI, 1, A4XX_CP_PERFCTR_CP_SEL_1 },
	/*
	 * The selector registers for 3, 5, and 7 are swizzled on the hardware.
	 * CP_4 and CP_6 are duped to SEL_2 and SEL_3 so we don't enable them
	 * here
	 */
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CP_3_LO,
		A4XX_RBBM_PERFCTR_CP_3_HI, 3, A4XX_CP_PERFCTR_CP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CP_5_LO,
		A4XX_RBBM_PERFCTR_CP_5_HI, 5, A4XX_CP_PERFCTR_CP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CP_7_LO,
		A4XX_RBBM_PERFCTR_CP_7_HI, 7, A4XX_CP_PERFCTR_CP_SEL_4 },
};

static struct adreno_perfcount_register a4xx_perfcounters_rbbm[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RBBM_0_LO,
		A4XX_RBBM_PERFCTR_RBBM_0_HI, 8, A4XX_RBBM_PERFCTR_RBBM_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RBBM_1_LO,
		A4XX_RBBM_PERFCTR_RBBM_1_HI, 9, A4XX_RBBM_PERFCTR_RBBM_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RBBM_2_LO,
		A4XX_RBBM_PERFCTR_RBBM_2_HI, 10, A4XX_RBBM_PERFCTR_RBBM_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RBBM_3_LO,
		A4XX_RBBM_PERFCTR_RBBM_3_HI, 11, A4XX_RBBM_PERFCTR_RBBM_SEL_3 },
};

static struct adreno_perfcount_register a4xx_perfcounters_pc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_PC_0_LO,
		A4XX_RBBM_PERFCTR_PC_0_HI, 12, A4XX_PC_PERFCTR_PC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_PC_1_LO,
		A4XX_RBBM_PERFCTR_PC_1_HI, 13, A4XX_PC_PERFCTR_PC_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_PC_2_LO,
		A4XX_RBBM_PERFCTR_PC_2_HI, 14, A4XX_PC_PERFCTR_PC_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_PC_3_LO,
		A4XX_RBBM_PERFCTR_PC_3_HI, 15, A4XX_PC_PERFCTR_PC_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_PC_4_LO,
		A4XX_RBBM_PERFCTR_PC_4_HI, 16, A4XX_PC_PERFCTR_PC_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_PC_5_LO,
		A4XX_RBBM_PERFCTR_PC_5_HI, 17, A4XX_PC_PERFCTR_PC_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_PC_6_LO,
		A4XX_RBBM_PERFCTR_PC_6_HI, 18, A4XX_PC_PERFCTR_PC_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_PC_7_LO,
		A4XX_RBBM_PERFCTR_PC_7_HI, 19, A4XX_PC_PERFCTR_PC_SEL_7 },
};

static struct adreno_perfcount_register a4xx_perfcounters_vfd[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VFD_0_LO,
		A4XX_RBBM_PERFCTR_VFD_0_HI, 20, A4XX_VFD_PERFCTR_VFD_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VFD_1_LO,
		A4XX_RBBM_PERFCTR_VFD_1_HI, 21, A4XX_VFD_PERFCTR_VFD_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VFD_2_LO,
		A4XX_RBBM_PERFCTR_VFD_2_HI, 22, A4XX_VFD_PERFCTR_VFD_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VFD_3_LO,
		A4XX_RBBM_PERFCTR_VFD_3_HI, 23, A4XX_VFD_PERFCTR_VFD_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VFD_4_LO,
		A4XX_RBBM_PERFCTR_VFD_4_HI, 24, A4XX_VFD_PERFCTR_VFD_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VFD_5_LO,
		A4XX_RBBM_PERFCTR_VFD_5_HI, 25, A4XX_VFD_PERFCTR_VFD_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VFD_6_LO,
		A4XX_RBBM_PERFCTR_VFD_6_HI, 26, A4XX_VFD_PERFCTR_VFD_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VFD_7_LO,
		A4XX_RBBM_PERFCTR_VFD_7_HI, 27, A4XX_VFD_PERFCTR_VFD_SEL_7 },
};

static struct adreno_perfcount_register a4xx_perfcounters_hlsq[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_HLSQ_0_LO,
		A4XX_RBBM_PERFCTR_HLSQ_0_HI, 28, A4XX_HLSQ_PERFCTR_HLSQ_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_HLSQ_1_LO,
		A4XX_RBBM_PERFCTR_HLSQ_1_HI, 29, A4XX_HLSQ_PERFCTR_HLSQ_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_HLSQ_2_LO,
		A4XX_RBBM_PERFCTR_HLSQ_2_HI, 30, A4XX_HLSQ_PERFCTR_HLSQ_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_HLSQ_3_LO,
		A4XX_RBBM_PERFCTR_HLSQ_3_HI, 31, A4XX_HLSQ_PERFCTR_HLSQ_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_HLSQ_4_LO,
		A4XX_RBBM_PERFCTR_HLSQ_4_HI, 32, A4XX_HLSQ_PERFCTR_HLSQ_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_HLSQ_5_LO,
		A4XX_RBBM_PERFCTR_HLSQ_5_HI, 33, A4XX_HLSQ_PERFCTR_HLSQ_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_HLSQ_6_LO,
		A4XX_RBBM_PERFCTR_HLSQ_6_HI, 34, A4XX_HLSQ_PERFCTR_HLSQ_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_HLSQ_7_LO,
		A4XX_RBBM_PERFCTR_HLSQ_7_HI, 35, A4XX_HLSQ_PERFCTR_HLSQ_SEL_7 },
};

static struct adreno_perfcount_register a4xx_perfcounters_vpc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VPC_0_LO,
		A4XX_RBBM_PERFCTR_VPC_0_HI, 36, A4XX_VPC_PERFCTR_VPC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VPC_1_LO,
		A4XX_RBBM_PERFCTR_VPC_1_HI, 37, A4XX_VPC_PERFCTR_VPC_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VPC_2_LO,
		A4XX_RBBM_PERFCTR_VPC_2_HI, 38, A4XX_VPC_PERFCTR_VPC_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VPC_3_LO,
		A4XX_RBBM_PERFCTR_VPC_3_HI, 39, A4XX_VPC_PERFCTR_VPC_SEL_3 },
};

static struct adreno_perfcount_register a4xx_perfcounters_ccu[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CCU_0_LO,
		A4XX_RBBM_PERFCTR_CCU_0_HI, 40, A4XX_RB_PERFCTR_CCU_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CCU_1_LO,
		A4XX_RBBM_PERFCTR_CCU_1_HI, 41, A4XX_RB_PERFCTR_CCU_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CCU_2_LO,
		A4XX_RBBM_PERFCTR_CCU_2_HI, 42, A4XX_RB_PERFCTR_CCU_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_CCU_3_LO,
		A4XX_RBBM_PERFCTR_CCU_3_HI, 43, A4XX_RB_PERFCTR_CCU_SEL_3 },
};

static struct adreno_perfcount_register a4xx_perfcounters_tse[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_TSE_0_LO,
		A4XX_RBBM_PERFCTR_TSE_0_HI, 44, A4XX_GRAS_PERFCTR_TSE_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_TSE_1_LO,
		A4XX_RBBM_PERFCTR_TSE_1_HI, 45, A4XX_GRAS_PERFCTR_TSE_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_TSE_2_LO,
		A4XX_RBBM_PERFCTR_TSE_2_HI, 46, A4XX_GRAS_PERFCTR_TSE_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_TSE_3_LO,
		A4XX_RBBM_PERFCTR_TSE_3_HI, 47, A4XX_GRAS_PERFCTR_TSE_SEL_3 },
};


static struct adreno_perfcount_register a4xx_perfcounters_ras[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RAS_0_LO,
		A4XX_RBBM_PERFCTR_RAS_0_HI, 48, A4XX_GRAS_PERFCTR_RAS_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RAS_1_LO,
		A4XX_RBBM_PERFCTR_RAS_1_HI, 49, A4XX_GRAS_PERFCTR_RAS_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RAS_2_LO,
		A4XX_RBBM_PERFCTR_RAS_2_HI, 50, A4XX_GRAS_PERFCTR_RAS_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RAS_3_LO,
		A4XX_RBBM_PERFCTR_RAS_3_HI, 51, A4XX_GRAS_PERFCTR_RAS_SEL_3 },
};

static struct adreno_perfcount_register a4xx_perfcounters_uche[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_UCHE_0_LO,
		A4XX_RBBM_PERFCTR_UCHE_0_HI, 52, A4XX_UCHE_PERFCTR_UCHE_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_UCHE_1_LO,
		A4XX_RBBM_PERFCTR_UCHE_1_HI, 53, A4XX_UCHE_PERFCTR_UCHE_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_UCHE_2_LO,
		A4XX_RBBM_PERFCTR_UCHE_2_HI, 54, A4XX_UCHE_PERFCTR_UCHE_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_UCHE_3_LO,
		A4XX_RBBM_PERFCTR_UCHE_3_HI, 55, A4XX_UCHE_PERFCTR_UCHE_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_UCHE_4_LO,
		A4XX_RBBM_PERFCTR_UCHE_4_HI, 56, A4XX_UCHE_PERFCTR_UCHE_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_UCHE_5_LO,
		A4XX_RBBM_PERFCTR_UCHE_5_HI, 57, A4XX_UCHE_PERFCTR_UCHE_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_UCHE_6_LO,
		A4XX_RBBM_PERFCTR_UCHE_6_HI, 58, A4XX_UCHE_PERFCTR_UCHE_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_UCHE_7_LO,
		A4XX_RBBM_PERFCTR_UCHE_7_HI, 59, A4XX_UCHE_PERFCTR_UCHE_SEL_7 },
};

static struct adreno_perfcount_register a4xx_perfcounters_tp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_TP_0_LO,
		A4XX_RBBM_PERFCTR_TP_0_HI, 60, A4XX_TPL1_PERFCTR_TP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_TP_1_LO,
		A4XX_RBBM_PERFCTR_TP_1_HI, 61, A4XX_TPL1_PERFCTR_TP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_TP_2_LO,
		A4XX_RBBM_PERFCTR_TP_2_HI, 62, A4XX_TPL1_PERFCTR_TP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_TP_3_LO,
		A4XX_RBBM_PERFCTR_TP_3_HI, 63, A4XX_TPL1_PERFCTR_TP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_TP_4_LO,
		A4XX_RBBM_PERFCTR_TP_4_HI, 64, A4XX_TPL1_PERFCTR_TP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_TP_5_LO,
		A4XX_RBBM_PERFCTR_TP_5_HI, 65, A4XX_TPL1_PERFCTR_TP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_TP_6_LO,
		A4XX_RBBM_PERFCTR_TP_6_HI, 66, A4XX_TPL1_PERFCTR_TP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_TP_7_LO,
		A4XX_RBBM_PERFCTR_TP_7_HI, 67, A4XX_TPL1_PERFCTR_TP_SEL_7 },
};

static struct adreno_perfcount_register a4xx_perfcounters_sp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_SP_0_LO,
		A4XX_RBBM_PERFCTR_SP_0_HI, 68, A4XX_SP_PERFCTR_SP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_SP_1_LO,
		A4XX_RBBM_PERFCTR_SP_1_HI, 69, A4XX_SP_PERFCTR_SP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_SP_2_LO,
		A4XX_RBBM_PERFCTR_SP_2_HI, 70, A4XX_SP_PERFCTR_SP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_SP_3_LO,
		A4XX_RBBM_PERFCTR_SP_3_HI, 71, A4XX_SP_PERFCTR_SP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_SP_4_LO,
		A4XX_RBBM_PERFCTR_SP_4_HI, 72, A4XX_SP_PERFCTR_SP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_SP_5_LO,
		A4XX_RBBM_PERFCTR_SP_5_HI, 73, A4XX_SP_PERFCTR_SP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_SP_6_LO,
		A4XX_RBBM_PERFCTR_SP_6_HI, 74, A4XX_SP_PERFCTR_SP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_SP_7_LO,
		A4XX_RBBM_PERFCTR_SP_7_HI, 75, A4XX_SP_PERFCTR_SP_SEL_7 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_SP_8_LO,
		A4XX_RBBM_PERFCTR_SP_8_HI, 76, A4XX_SP_PERFCTR_SP_SEL_8 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_SP_9_LO,
		A4XX_RBBM_PERFCTR_SP_9_HI, 77, A4XX_SP_PERFCTR_SP_SEL_9 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_SP_10_LO,
		A4XX_RBBM_PERFCTR_SP_10_HI, 78, A4XX_SP_PERFCTR_SP_SEL_10 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_SP_11_LO,
		A4XX_RBBM_PERFCTR_SP_11_HI, 79, A4XX_SP_PERFCTR_SP_SEL_11 },
};

static struct adreno_perfcount_register a4xx_perfcounters_rb[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RB_0_LO,
		A4XX_RBBM_PERFCTR_RB_0_HI, 80, A4XX_RB_PERFCTR_RB_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RB_1_LO,
		A4XX_RBBM_PERFCTR_RB_1_HI, 81, A4XX_RB_PERFCTR_RB_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RB_2_LO,
		A4XX_RBBM_PERFCTR_RB_2_HI, 82, A4XX_RB_PERFCTR_RB_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RB_3_LO,
		A4XX_RBBM_PERFCTR_RB_3_HI, 83, A4XX_RB_PERFCTR_RB_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RB_4_LO,
		A4XX_RBBM_PERFCTR_RB_4_HI, 84, A4XX_RB_PERFCTR_RB_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RB_5_LO,
		A4XX_RBBM_PERFCTR_RB_5_HI, 85, A4XX_RB_PERFCTR_RB_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RB_6_LO,
		A4XX_RBBM_PERFCTR_RB_6_HI, 86, A4XX_RB_PERFCTR_RB_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_RB_7_LO,
		A4XX_RBBM_PERFCTR_RB_7_HI, 87, A4XX_RB_PERFCTR_RB_SEL_7 },
};

static struct adreno_perfcount_register a4xx_perfcounters_vsc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VSC_0_LO,
		A4XX_RBBM_PERFCTR_VSC_0_HI, 88, A4XX_VSC_PERFCTR_VSC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_VSC_1_LO,
		A4XX_RBBM_PERFCTR_VSC_1_HI, 89, A4XX_VSC_PERFCTR_VSC_SEL_1 },
};

static struct adreno_perfcount_register a4xx_perfcounters_pwr[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_PWR_0_LO,
		A4XX_RBBM_PERFCTR_PWR_0_HI, -1, 0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_RBBM_PERFCTR_PWR_1_LO,
		A4XX_RBBM_PERFCTR_PWR_1_HI, -1, 0},
};

static struct adreno_perfcount_register a4xx_perfcounters_vbif[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_VBIF_PERF_CNT_LOW0,
		A4XX_VBIF_PERF_CNT_HIGH0, -1, A4XX_VBIF_PERF_CNT_SEL0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_VBIF_PERF_CNT_LOW1,
		A4XX_VBIF_PERF_CNT_HIGH1, -1, A4XX_VBIF_PERF_CNT_SEL1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_VBIF_PERF_CNT_LOW2,
		A4XX_VBIF_PERF_CNT_HIGH2, -1, A4XX_VBIF_PERF_CNT_SEL2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_VBIF_PERF_CNT_LOW3,
		A4XX_VBIF_PERF_CNT_HIGH3, -1, A4XX_VBIF_PERF_CNT_SEL3 },
};

static struct adreno_perfcount_register a4xx_perfcounters_vbif_pwr[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_VBIF_PERF_PWR_CNT_LOW0,
		A4XX_VBIF_PERF_PWR_CNT_HIGH0, -1, A4XX_VBIF_PERF_PWR_CNT_EN0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_VBIF_PERF_PWR_CNT_LOW1,
		A4XX_VBIF_PERF_PWR_CNT_HIGH1, -1, A4XX_VBIF_PERF_PWR_CNT_EN1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_VBIF_PERF_PWR_CNT_LOW2,
		A4XX_VBIF_PERF_PWR_CNT_HIGH2, -1, A4XX_VBIF_PERF_PWR_CNT_EN2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A4XX_VBIF_PERF_PWR_CNT_LOW3,
		A4XX_VBIF_PERF_PWR_CNT_HIGH3, -1, A4XX_VBIF_PERF_PWR_CNT_EN3 },
};

#define A4XX_PERFCOUNTER_GROUP(offset, name) \
	ADRENO_PERFCOUNTER_GROUP(a4xx, offset, name)

#define A4XX_PERFCOUNTER_GROUP_FLAGS(offset, name, flags) \
	ADRENO_PERFCOUNTER_GROUP_FLAGS(a4xx, offset, name, flags)

static struct adreno_perfcount_group a4xx_perfcounter_groups
				[KGSL_PERFCOUNTER_GROUP_MAX] = {
	A4XX_PERFCOUNTER_GROUP(CP, cp),
	A4XX_PERFCOUNTER_GROUP(RBBM, rbbm),
	A4XX_PERFCOUNTER_GROUP(PC, pc),
	A4XX_PERFCOUNTER_GROUP(VFD, vfd),
	A4XX_PERFCOUNTER_GROUP(HLSQ, hlsq),
	A4XX_PERFCOUNTER_GROUP(VPC, vpc),
	A4XX_PERFCOUNTER_GROUP(CCU, ccu),
	A4XX_PERFCOUNTER_GROUP(TSE, tse),
	A4XX_PERFCOUNTER_GROUP(RAS, ras),
	A4XX_PERFCOUNTER_GROUP(UCHE, uche),
	A4XX_PERFCOUNTER_GROUP(TP, tp),
	A4XX_PERFCOUNTER_GROUP(SP, sp),
	A4XX_PERFCOUNTER_GROUP(RB, rb),
	A4XX_PERFCOUNTER_GROUP(VSC, vsc),
	A4XX_PERFCOUNTER_GROUP_FLAGS(PWR, pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED),
	A4XX_PERFCOUNTER_GROUP(VBIF, vbif),
	A4XX_PERFCOUNTER_GROUP_FLAGS(VBIF_PWR, vbif_pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED),
};

static struct adreno_perfcounters a4xx_perfcounters = {
	a4xx_perfcounter_groups,
	ARRAY_SIZE(a4xx_perfcounter_groups),
};

/*
 * On A420 a number of perfcounters are un-usable. The following defines the
 * array of countables that do not work and should not be used
 */
static const unsigned int a420_pc_invalid_countables[] = {
	PC_INSTANCES, PC_VERTEX_HITS, PC_GENERATED_FIBERS, PC_GENERATED_WAVES,
};

static const unsigned int a420_vfd_invalid_countables[] = {
	VFD_VPC_BYPASS_TRANS, VFD_UPPER_SHADER_FIBERS, VFD_LOWER_SHADER_FIBERS,
};

static const unsigned int a420_hlsq_invalid_countables[] = {
	HLSQ_SP_VS_STAGE_CONSTANT, HLSQ_SP_VS_STAGE_INSTRUCTIONS,
	HLSQ_SP_FS_STAGE_CONSTANT, HLSQ_SP_FS_STAGE_INSTRUCTIONS,
	HLSQ_FS_STAGE_16_WAVES, HLSQ_FS_STAGE_32_WAVES, HLSQ_FS_STAGE_64_WAVES,
	HLSQ_VS_STAGE_16_WAVES, HLSQ_VS_STAGE_32_WAVES,
};

static const unsigned int a420_uche_invalid_countables[] = {
	UCHE_READ_REQUESTS_MARB, UCHE_READ_REQUESTS_SP,
	UCHE_WRITE_REQUESTS_MARB, UCHE_WRITE_REQUESTS_SP,
	UCHE_WRITE_REQUESTS_VPC
};

static const unsigned int a420_tp_invalid_countables[] = {
	TP_OUTPUT_TEXELS_POINT, TP_OUTPUT_TEXELS_BILINEAR, TP_OUTPUT_TEXELS_MIP,
	TP_OUTPUT_TEXELS_ANISO, TP_OUTPUT_TEXELS_OPS16, TP_OUTPUT_TEXELS_OPS32,
	TP_ZERO_LOD, TP_LATENCY, TP_LATENCY_TRANS,
};

static const unsigned int a420_sp_invalid_countables[] = {
	SP_FS_STAGE_BARY_INSTRUCTIONS,
};

static const unsigned int a420_rb_invalid_countables[] = {
	RB_VALID_SAMPLES, RB_Z_FAIL, RB_S_FAIL,
};

static const unsigned int a420_ccu_invalid_countables[] = {
	CCU_VBIF_STALL, CCU_VBIF_LATENCY_CYCLES, CCU_VBIF_LATENCY_SAMPLES,
	CCU_Z_READ, CCU_Z_WRITE, CCU_C_READ, CCU_C_WRITE,
};

static const struct adreno_invalid_countables
	a420_perfctr_invalid_countables[KGSL_PERFCOUNTER_GROUP_MAX] = {
	ADRENO_PERFCOUNTER_INVALID_COUNTABLE(a420_pc, PC),
	ADRENO_PERFCOUNTER_INVALID_COUNTABLE(a420_vfd, VFD),
	ADRENO_PERFCOUNTER_INVALID_COUNTABLE(a420_hlsq, HLSQ),
	ADRENO_PERFCOUNTER_INVALID_COUNTABLE(a420_tp, TP),
	ADRENO_PERFCOUNTER_INVALID_COUNTABLE(a420_sp, SP),
	ADRENO_PERFCOUNTER_INVALID_COUNTABLE(a420_rb, RB),
	ADRENO_PERFCOUNTER_INVALID_COUNTABLE(a420_ccu, CCU),
	ADRENO_PERFCOUNTER_INVALID_COUNTABLE(a420_uche, UCHE),
};

static struct adreno_coresight_register a4xx_coresight_registers[] = {
	{ A4XX_RBBM_CFG_DEBBUS_CTLT },
	{ A4XX_RBBM_CFG_DEBBUS_SEL_A },
	{ A4XX_RBBM_CFG_DEBBUS_SEL_B },
	{ A4XX_RBBM_CFG_DEBBUS_SEL_C },
	{ A4XX_RBBM_CFG_DEBBUS_SEL_D },
	{ A4XX_RBBM_CFG_DEBBUS_OPL },
	{ A4XX_RBBM_CFG_DEBBUS_OPE },
	{ A4XX_RBBM_CFG_DEBBUS_IVTL_0 },
	{ A4XX_RBBM_CFG_DEBBUS_IVTL_1 },
	{ A4XX_RBBM_CFG_DEBBUS_IVTL_2 },
	{ A4XX_RBBM_CFG_DEBBUS_IVTL_3 },
	{ A4XX_RBBM_CFG_DEBBUS_MASKL_0 },
	{ A4XX_RBBM_CFG_DEBBUS_MASKL_1 },
	{ A4XX_RBBM_CFG_DEBBUS_MASKL_2 },
	{ A4XX_RBBM_CFG_DEBBUS_MASKL_3 },
	{ A4XX_RBBM_CFG_DEBBUS_BYTEL_0 },
	{ A4XX_RBBM_CFG_DEBBUS_BYTEL_1 },
	{ A4XX_RBBM_CFG_DEBBUS_IVTE_0 },
	{ A4XX_RBBM_CFG_DEBBUS_IVTE_1 },
	{ A4XX_RBBM_CFG_DEBBUS_IVTE_2 },
	{ A4XX_RBBM_CFG_DEBBUS_IVTE_3 },
	{ A4XX_RBBM_CFG_DEBBUS_MASKE_0 },
	{ A4XX_RBBM_CFG_DEBBUS_MASKE_1 },
	{ A4XX_RBBM_CFG_DEBBUS_MASKE_2 },
	{ A4XX_RBBM_CFG_DEBBUS_MASKE_3 },
	{ A4XX_RBBM_CFG_DEBBUS_NIBBLEE },
	{ A4XX_RBBM_CFG_DEBBUS_PTRC0 },
	{ A4XX_RBBM_CFG_DEBBUS_PTRC1 },
	{ A4XX_RBBM_CFG_DEBBUS_CLRC },
	{ A4XX_RBBM_CFG_DEBBUS_LOADIVT },
	{ A4XX_RBBM_CFG_DEBBUS_IDX },
	{ A4XX_RBBM_CFG_DEBBUS_LOADREG },
	{ A4XX_RBBM_EXT_TRACE_BUS_CTL },
	{ A4XX_RBBM_CFG_DEBBUS_CTLM },
};

static int a4xx_perfcounter_init(struct adreno_device *adreno_dev)
{
	if (adreno_is_a420(adreno_dev)) {
		struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
		struct adreno_perfcounters *counters = gpudev->perfcounters;
		/*
		 * The CP counters on A420 are... special.  Some of the counters
		 * are swizzled so only a subset of them are usable
		 */

		if (counters) {
			counters->groups[KGSL_PERFCOUNTER_GROUP_CP].regs =
				a420_perfcounters_cp;
			counters->groups[KGSL_PERFCOUNTER_GROUP_CP].reg_count =
				ARRAY_SIZE(a420_perfcounters_cp);
		}

		/*
		 * Also on A420 a number of the countables are not functional so
		 * we maintain a blacklist of countables to protect the user
		 */

		gpudev->invalid_countables = a420_perfctr_invalid_countables;
	}

	return a3xx_perfcounter_init(adreno_dev);
}

static ADRENO_CORESIGHT_ATTR(cfg_debbus_ctrlt, &a4xx_coresight_registers[0]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_sela, &a4xx_coresight_registers[1]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_selb, &a4xx_coresight_registers[2]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_selc, &a4xx_coresight_registers[3]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_seld, &a4xx_coresight_registers[4]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_opl, &a4xx_coresight_registers[5]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_ope, &a4xx_coresight_registers[6]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_ivtl0, &a4xx_coresight_registers[7]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_ivtl1, &a4xx_coresight_registers[8]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_ivtl2, &a4xx_coresight_registers[9]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_ivtl3, &a4xx_coresight_registers[10]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_maskl0, &a4xx_coresight_registers[11]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_maskl1, &a4xx_coresight_registers[12]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_maskl2, &a4xx_coresight_registers[13]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_maskl3, &a4xx_coresight_registers[14]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_bytel0, &a4xx_coresight_registers[15]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_bytel1, &a4xx_coresight_registers[16]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_ivte0, &a4xx_coresight_registers[17]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_ivte1, &a4xx_coresight_registers[18]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_ivte2, &a4xx_coresight_registers[19]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_ivte3, &a4xx_coresight_registers[20]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_maske0, &a4xx_coresight_registers[21]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_maske1, &a4xx_coresight_registers[22]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_maske2, &a4xx_coresight_registers[23]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_maske3, &a4xx_coresight_registers[24]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_nibblee, &a4xx_coresight_registers[25]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_ptrc0, &a4xx_coresight_registers[26]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_ptrc1, &a4xx_coresight_registers[27]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_clrc, &a4xx_coresight_registers[28]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_loadivt, &a4xx_coresight_registers[29]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_idx, &a4xx_coresight_registers[30]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_loadreg, &a4xx_coresight_registers[31]);
static ADRENO_CORESIGHT_ATTR(ext_tracebus_ctl, &a4xx_coresight_registers[32]);
static ADRENO_CORESIGHT_ATTR(cfg_debbus_ctrlm, &a4xx_coresight_registers[33]);


static struct attribute *a4xx_coresight_attrs[] = {
	&coresight_attr_cfg_debbus_ctrlt.attr.attr,
	&coresight_attr_cfg_debbus_sela.attr.attr,
	&coresight_attr_cfg_debbus_selb.attr.attr,
	&coresight_attr_cfg_debbus_selc.attr.attr,
	&coresight_attr_cfg_debbus_seld.attr.attr,
	&coresight_attr_cfg_debbus_opl.attr.attr,
	&coresight_attr_cfg_debbus_ope.attr.attr,
	&coresight_attr_cfg_debbus_ivtl0.attr.attr,
	&coresight_attr_cfg_debbus_ivtl1.attr.attr,
	&coresight_attr_cfg_debbus_ivtl2.attr.attr,
	&coresight_attr_cfg_debbus_ivtl3.attr.attr,
	&coresight_attr_cfg_debbus_maskl0.attr.attr,
	&coresight_attr_cfg_debbus_maskl1.attr.attr,
	&coresight_attr_cfg_debbus_maskl2.attr.attr,
	&coresight_attr_cfg_debbus_maskl3.attr.attr,
	&coresight_attr_cfg_debbus_bytel0.attr.attr,
	&coresight_attr_cfg_debbus_bytel1.attr.attr,
	&coresight_attr_cfg_debbus_ivte0.attr.attr,
	&coresight_attr_cfg_debbus_ivte1.attr.attr,
	&coresight_attr_cfg_debbus_ivte2.attr.attr,
	&coresight_attr_cfg_debbus_ivte3.attr.attr,
	&coresight_attr_cfg_debbus_maske0.attr.attr,
	&coresight_attr_cfg_debbus_maske1.attr.attr,
	&coresight_attr_cfg_debbus_maske2.attr.attr,
	&coresight_attr_cfg_debbus_maske3.attr.attr,
	&coresight_attr_cfg_debbus_nibblee.attr.attr,
	&coresight_attr_cfg_debbus_ptrc0.attr.attr,
	&coresight_attr_cfg_debbus_ptrc1.attr.attr,
	&coresight_attr_cfg_debbus_clrc.attr.attr,
	&coresight_attr_cfg_debbus_loadivt.attr.attr,
	&coresight_attr_cfg_debbus_idx.attr.attr,
	&coresight_attr_cfg_debbus_loadreg.attr.attr,
	&coresight_attr_ext_tracebus_ctl.attr.attr,
	&coresight_attr_cfg_debbus_ctrlm.attr.attr,
	NULL,
};

static const struct attribute_group a4xx_coresight_group = {
	.attrs = a4xx_coresight_attrs,
};

static const struct attribute_group *a4xx_coresight_groups[] = {
	&a4xx_coresight_group,
	NULL,
};

static struct adreno_coresight a4xx_coresight = {
	.registers = a4xx_coresight_registers,
	.count = ARRAY_SIZE(a4xx_coresight_registers),
	.groups = a4xx_coresight_groups,
};

#define A4XX_INT_MASK \
	((1 << A3XX_INT_RBBM_AHB_ERROR) |		\
	 (1 << A3XX_INT_RBBM_REG_TIMEOUT) |		\
	 (1 << A3XX_INT_RBBM_ME_MS_TIMEOUT) |		\
	 (1 << A3XX_INT_RBBM_PFP_MS_TIMEOUT) |		\
	 (1 << A4XX_INT_RBBM_ETS_MS_TIMEOUT) |		\
	 (1 << A4XX_INT_RBBM_ASYNC_OVERFLOW) |		\
	 (1 << A4XX_INT_CP_OPCODE_ERR) |		\
	 (1 << A3XX_INT_CP_RESERVED_BIT_ERROR) |	\
	 (1 << A3XX_INT_CP_HW_FAULT) |			\
	 (1 << A3XX_INT_CP_IB1_INT) |			\
	 (1 << A3XX_INT_CP_IB2_INT) |			\
	 (1 << A3XX_INT_CP_RB_INT) |			\
	 (1 << A3XX_INT_CP_REG_PROTECT_FAULT) |		\
	 (1 << A3XX_INT_CP_AHB_ERROR_HALT) |		\
	 (1 << A4XX_INT_RBBM_ATB_BUS_OVERFLOW) |	\
	 (1 << A3XX_INT_UCHE_OOB_ACCESS) |		\
	 (1 << A4XX_INT_RBBM_DPM_CALC_ERR) |		\
	 (1 << A4XX_INT_RBBM_DPM_EPOCH_ERR) |		\
	 (1 << A4XX_INT_RBBM_DPM_THERMAL_YELLOW_ERR) |\
	 (1 << A4XX_INT_RBBM_DPM_THERMAL_RED_ERR))

static struct adreno_irq_funcs a4xx_irq_funcs[] = {
	ADRENO_IRQ_CALLBACK(NULL),                   /* 0 - RBBM_GPU_IDLE */
	ADRENO_IRQ_CALLBACK(a3xx_a4xx_err_callback), /* 1 - RBBM_AHB_ERROR */
	ADRENO_IRQ_CALLBACK(a3xx_a4xx_err_callback), /* 2 - RBBM_REG_TIMEOUT */
	/* 3 - RBBM_ME_MS_TIMEOUT */
	ADRENO_IRQ_CALLBACK(a3xx_a4xx_err_callback),
	/* 4 - RBBM_PFP_MS_TIMEOUT */
	ADRENO_IRQ_CALLBACK(a3xx_a4xx_err_callback),
	ADRENO_IRQ_CALLBACK(a4xx_err_callback), /* 5 - RBBM_ETS_MS_TIMEOUT */
	/* 6 - RBBM_ATB_ASYNC_OVERFLOW */
	ADRENO_IRQ_CALLBACK(a4xx_err_callback),
	ADRENO_IRQ_CALLBACK(a4xx_err_callback), /* 7 - RBBM_GPC_ERR */
	ADRENO_IRQ_CALLBACK(NULL), /* 8 - CP_SW */
	ADRENO_IRQ_CALLBACK(a4xx_err_callback), /* 9 - CP_OPCODE_ERROR */
	/* 10 - CP_RESERVED_BIT_ERROR */
	ADRENO_IRQ_CALLBACK(a3xx_a4xx_err_callback),
	ADRENO_IRQ_CALLBACK(a3xx_a4xx_err_callback), /* 11 - CP_HW_FAULT */
	ADRENO_IRQ_CALLBACK(NULL), /* 12 - CP_DMA */
	ADRENO_IRQ_CALLBACK(a3xx_cp_callback), /* 13 - CP_IB2_INT */
	ADRENO_IRQ_CALLBACK(a3xx_cp_callback), /* 14 - CP_IB1_INT */
	ADRENO_IRQ_CALLBACK(a3xx_cp_callback), /* 15 - CP_RB_INT */
	/* 16 - CP_REG_PROTECT_FAULT */
	ADRENO_IRQ_CALLBACK(a3xx_a4xx_err_callback),
	ADRENO_IRQ_CALLBACK(NULL), /* 17 - CP_RB_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 18 - CP_VS_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 19 - CP_PS_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 20 - CP_CACHE_FLUSH_TS */
	/* 21 - CP_AHB_ERROR_FAULT */
	ADRENO_IRQ_CALLBACK(a3xx_a4xx_err_callback),
	ADRENO_IRQ_CALLBACK(a4xx_err_callback), /* 22 - RBBM_ATB_BUS_OVERFLOW */
	ADRENO_IRQ_CALLBACK(NULL), /* 23 - Unused */
	/* 24 - MISC_HANG_DETECT */
	ADRENO_IRQ_CALLBACK(a3xx_fatal_err_callback),
	ADRENO_IRQ_CALLBACK(a3xx_a4xx_err_callback), /* 25 - UCHE_OOB_ACCESS */
	ADRENO_IRQ_CALLBACK(NULL), /* 26 - Unused */
	ADRENO_IRQ_CALLBACK(NULL), /* 27 - RBBM_TRACE_MISR */
	ADRENO_IRQ_CALLBACK(a4xx_err_callback), /* 28 - RBBM_DPM_CALC_ERR */
	ADRENO_IRQ_CALLBACK(a4xx_err_callback), /* 29 - RBBM_DPM_EPOCH_ERR */
	/* 30 - RBBM_DPM_THERMAL_YELLOW_ERR */
	ADRENO_IRQ_CALLBACK(a4xx_err_callback),
	/* 31 - RBBM_DPM_THERMAL_RED_ERR */
	ADRENO_IRQ_CALLBACK(a4xx_err_callback),
};

static struct adreno_irq a4xx_irq = {
	.funcs = a4xx_irq_funcs,
	.funcs_count = ARRAY_SIZE(a4xx_irq_funcs),
	.mask = A4XX_INT_MASK,
};

static struct adreno_snapshot_sizes a4xx_snap_sizes = {
	.cp_state_deb = 0x14,
	.vpc_mem = 2048,
	.cp_meq = 64,
	.shader_mem = 0x4000,
	.cp_merciu = 64,
	.roq = 512,
};

static struct adreno_snapshot_data a4xx_snapshot_data = {
	.sect_sizes = &a4xx_snap_sizes,
};

struct adreno_gpudev adreno_a4xx_gpudev = {
	.reg_offsets = &a4xx_reg_offsets,
	.perfcounters = &a4xx_perfcounters,
	.irq = &a4xx_irq,
	.snapshot_data = &a4xx_snapshot_data,
	.num_prio_levels = 1,

	.perfcounter_init = a4xx_perfcounter_init,
	.perfcounter_close = a3xx_perfcounter_close,
	.rb_init = a3xx_rb_init,
	.irq_control = a3xx_irq_control,
	.irq_handler = a3xx_irq_handler,
	.irq_pending = a3xx_irq_pending,
	.busy_cycles = a3xx_busy_cycles,
	.coresight = &a4xx_coresight,
	.start = a4xx_start,
	.perfcounter_enable = a3xx_perfcounter_enable,
	.perfcounter_read = a3xx_perfcounter_read,
	.perfcounter_save = a3xx_perfcounter_save,
	.perfcounter_restore = a3xx_perfcounter_restore,
	.fault_detect_start = a3xx_fault_detect_start,
	.fault_detect_stop = a3xx_fault_detect_stop,
	.soft_reset = a3xx_soft_reset,
	.snapshot = a4xx_snapshot,
	.is_sptp_idle = a4xx_is_sptp_idle,
	.enable_pc = a4xx_enable_pc,
	.regulator_enable = a4xx_regulator_enable,
	.regulator_disable = a4xx_regulator_disable,
};
