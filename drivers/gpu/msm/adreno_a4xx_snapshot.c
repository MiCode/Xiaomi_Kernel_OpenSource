/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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

#include <linux/io.h>
#include "kgsl.h"
#include "adreno.h"
#include "kgsl_snapshot.h"
#include "a4xx_reg.h"
#include "adreno_snapshot.h"
#include "adreno_a4xx.h"

/*
 * Set of registers to dump for A4XX on snapshot.
 * Registers in pairs - first value is the start offset, second
 * is the stop offset (inclusive)
 */

static const unsigned int a4xx_registers[] = {
	/* RBBM */
	0x0000, 0x0002, 0x0004, 0x0021, 0x0023, 0x0024, 0x0026, 0x0026,
	0x0028, 0x002B, 0x002E, 0x0034, 0x0037, 0x0044, 0x0047, 0x0066,
	0x0068, 0x0095, 0x009C, 0x0170, 0x0174, 0x01AF,
	/* CP */
	0x0200, 0x0226, 0x0228, 0x0233, 0x0240, 0x0258, 0x04C0, 0x04D0,
	0x04D2, 0x04DD, 0x0500, 0x050B, 0x0578, 0x058F,
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

static const unsigned int a4xx_sp_tp_registers[] = {
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

static const unsigned int a4xx_ppd_registers[] = {
	/* V2 Thresholds */
	0x01B2, 0x01B5,
	/* Control and Status */
	0x01B9, 0x01BE,
};

static const unsigned int a4xx_xpu_registers[] = {
	/* XPU */
	0x2C00, 0x2C01, 0x2C10, 0x2C10, 0x2C12, 0x2C16, 0x2C1D, 0x2C20,
	0x2C28, 0x2C28, 0x2C30, 0x2C30, 0x2C32, 0x2C36, 0x2C40, 0x2C40,
	0x2C50, 0x2C50, 0x2C52, 0x2C56, 0x2C80, 0x2C80, 0x2C94, 0x2C95,
};

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
	0x67D6, 0x67D6, 0x67EE, 0x67EE,
};

static const unsigned int a4xx_vbif_ver_20020000_registers[] = {
	0x3000, 0x3007, 0x300C, 0x3014, 0x3018, 0x301D, 0x3020, 0x3022,
	0x3024, 0x3026, 0x3028, 0x302A, 0x302C, 0x302D, 0x3030, 0x3031,
	0x3034, 0x3036, 0x3038, 0x3038, 0x303C, 0x303D, 0x3040, 0x3040,
	0x3049, 0x3049, 0x3058, 0x3058, 0x305B, 0x3061, 0x3064, 0x3068,
	0x306C, 0x306D, 0x3080, 0x3088, 0x308B, 0x308C, 0x3090, 0x3094,
	0x3098, 0x3098, 0x309C, 0x309C, 0x30C0, 0x30C0, 0x30C8, 0x30C8,
	0x30D0, 0x30D0, 0x30D8, 0x30D8, 0x30E0, 0x30E0, 0x3100, 0x3100,
	0x3108, 0x3108, 0x3110, 0x3110, 0x3118, 0x3118, 0x3120, 0x3120,
	0x3124, 0x3125, 0x3129, 0x3129, 0x3131, 0x3131, 0x4800, 0x4802,
	0x480F, 0x480F, 0x4811, 0x4811, 0x4813, 0x4813, 0x4815, 0x4816,
	0x482B, 0x482B, 0x4857, 0x4857, 0x4883, 0x4883, 0x48AF, 0x48AF,
	0x48C5, 0x48C5, 0x48E5, 0x48E5, 0x4905, 0x4905, 0x4925, 0x4925,
	0x4945, 0x4945, 0x4950, 0x4950, 0x495B, 0x495B, 0x4980, 0x498E,
	0x4C00, 0x4C00, 0x4D00, 0x4D00, 0x4E00, 0x4E00, 0x4E80, 0x4E80,
	0x4F00, 0x4F00, 0x4F08, 0x4F08, 0x4F10, 0x4F10, 0x4F18, 0x4F18,
	0x4F20, 0x4F20, 0x4F30, 0x4F30, 0x4F60, 0x4F60, 0x4F80, 0x4F81,
	0x4F88, 0x4F89, 0x4FEE, 0x4FEE, 0x4FF3, 0x4FF3, 0x6000, 0x6001,
	0x6008, 0x600F, 0x6014, 0x6016, 0x6018, 0x601B, 0x61FD, 0x61FD,
	0x623C, 0x623C, 0x6380, 0x6380, 0x63A0, 0x63A0, 0x63C0, 0x63C1,
	0x63C8, 0x63C9, 0x63D0, 0x63D4, 0x63D6, 0x63D6, 0x63EE, 0x63EE,
	0x6400, 0x6401, 0x6408, 0x640F, 0x6414, 0x6416, 0x6418, 0x641B,
	0x65FD, 0x65FD, 0x663C, 0x663C, 0x6780, 0x6780, 0x67A0, 0x67A0,
	0x67C0, 0x67C1, 0x67C8, 0x67C9, 0x67D0, 0x67D4, 0x67D6, 0x67D6,
	0x67EE, 0x67EE,
};

static const unsigned int a4xx_vbif_ver_20050000_registers[] = {
	/* VBIF version 0x20050000 and 0x20090000 */
	0x3000, 0x3007, 0x302C, 0x302C, 0x3030, 0x3030, 0x3034, 0x3036,
	0x3038, 0x3038, 0x303C, 0x303D, 0x3040, 0x3040, 0x3049, 0x3049,
	0x3058, 0x3058, 0x305B, 0x3061, 0x3064, 0x3068, 0x306C, 0x306D,
	0x3080, 0x3088, 0x308B, 0x308C, 0x3090, 0x3094, 0x3098, 0x3098,
	0x309C, 0x309C, 0x30C0, 0x30C0, 0x30C8, 0x30C8, 0x30D0, 0x30D0,
	0x30D8, 0x30D8, 0x30E0, 0x30E0, 0x3100, 0x3100, 0x3108, 0x3108,
	0x3110, 0x3110, 0x3118, 0x3118, 0x3120, 0x3120, 0x3124, 0x3125,
	0x3129, 0x3129, 0x340C, 0x340C, 0x3410, 0x3410,
};

static const struct adreno_vbif_snapshot_registers
					a4xx_vbif_snapshot_registers[] = {
	{ 0x20000000, a4xx_vbif_ver_20000000_registers,
				ARRAY_SIZE(a4xx_vbif_ver_20000000_registers)/2},
	{ 0x20020000, a4xx_vbif_ver_20020000_registers,
				ARRAY_SIZE(a4xx_vbif_ver_20020000_registers)/2},
	{ 0x20050000, a4xx_vbif_ver_20050000_registers,
				ARRAY_SIZE(a4xx_vbif_ver_20050000_registers)/2},
	{ 0x20070000, a4xx_vbif_ver_20020000_registers,
				ARRAY_SIZE(a4xx_vbif_ver_20020000_registers)/2},
	{ 0x20090000, a4xx_vbif_ver_20050000_registers,
				ARRAY_SIZE(a4xx_vbif_ver_20050000_registers)/2},
};

#define A4XX_NUM_SHADER_BANKS 4
#define A405_NUM_SHADER_BANKS 1
/* Shader memory size in words */
#define A4XX_SHADER_MEMORY_SIZE 0x4000

static const struct adreno_debugbus_block a4xx_debugbus_blocks[] = {
	{ A4XX_RBBM_DEBBUS_CP_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_RBBM_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_VBIF_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_HLSQ_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_UCHE_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_DPM_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_TESS_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_PC_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_VFD_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_VPC_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_TSE_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_RAS_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_VSC_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_COM_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_DCOM_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_SP_0_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_TPL1_0_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_RB_0_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_MARB_0_ID, 0x100 },
};

static const struct adreno_debugbus_block a420_debugbus_blocks[] = {
	{ A4XX_RBBM_DEBBUS_SP_1_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_SP_2_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_SP_3_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_TPL1_1_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_TPL1_2_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_TPL1_3_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_RB_1_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_RB_2_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_RB_3_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_MARB_1_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_MARB_2_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_MARB_3_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_CCU_0_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_CCU_1_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_CCU_2_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_CCU_3_ID, 0x100, },
};

/**
 * a4xx_snapshot_shader_memory - Helper function to dump the GPU shader
 * memory to the snapshot buffer.
 * @device: GPU device whose shader memory is to be dumped
 * @buf: Pointer to binary snapshot data blob being made
 * @remain: Number of remaining bytes in the snapshot blob
 * @priv: Unused parameter
 *
 */
static size_t a4xx_snapshot_shader_memory(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int i, j;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int shader_read_len = A4XX_SHADER_MEMORY_SIZE;
	unsigned int shader_banks = A4XX_NUM_SHADER_BANKS;

	if (shader_read_len > (device->shader_mem_len >> 2))
		shader_read_len = (device->shader_mem_len >> 2);

	if (adreno_is_a405(adreno_dev))
		shader_banks = A405_NUM_SHADER_BANKS;

	if (remain < DEBUG_SECTION_SZ(shader_read_len *
				shader_banks)) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_SHADER_MEMORY;
	header->size = shader_read_len * shader_banks;

	/* Map shader memory to kernel, for dumping */
	if (device->shader_mem_virt == NULL)
		device->shader_mem_virt = devm_ioremap(device->dev,
					device->shader_mem_phys,
					device->shader_mem_len);

	if (device->shader_mem_virt == NULL) {
		KGSL_DRV_ERR(device,
		"Unable to map shader memory region\n");
		return 0;
	}

	for (j = 0; j < shader_banks; j++) {
		unsigned int val;
		/* select the SPTP */
		kgsl_regread(device, A4XX_HLSQ_SPTP_RDSEL, &val);
		val &= ~0x3;
		val |= j;
		kgsl_regwrite(device, A4XX_HLSQ_SPTP_RDSEL, val);
		/* Now, dump shader memory to snapshot */
		for (i = 0; i < shader_read_len; i++)
			adreno_shadermem_regread(device, i,
				&data[i + j * shader_read_len]);
	}


	return DEBUG_SECTION_SZ(shader_read_len * shader_banks);
}

/*
 * a4xx_rbbm_debug_bus_read() - Read data from trace bus
 * @device: Device whose data bus is read
 * @block_id: Trace bus block ID
 * @index: Index of data to read
 * @val: Output parameter where data is read
 */
static void a4xx_rbbm_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int reg = 0;

	reg |= (block_id << A4XX_RBBM_CFG_DEBBUS_SEL_PING_BLK_SEL_SHIFT);
	reg |= (index << A4XX_RBBM_CFG_DEBBUS_SEL_PING_INDEX_SHIFT);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_SEL_A, reg);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_SEL_B, reg);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_SEL_C, reg);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_SEL_D, reg);

	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_IDX, 0x3020000);
	kgsl_regread(device, A4XX_RBBM_CFG_DEBBUS_TRACE_BUF4, val);
	val++;
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_IDX, 0x1000000);
	kgsl_regread(device, A4XX_RBBM_CFG_DEBBUS_TRACE_BUF4, val);
}

/*
 * a4xx_snapshot_vbif_debugbus() - Dump the VBIF debug data
 * @device: Device pointer for which the debug data is dumped
 * @buf: Pointer to the memory where the data is dumped
 * @remain: Amout of bytes remaining in snapshot
 * @priv: Pointer to debug bus block
 *
 * Returns the number of bytes dumped
 */
static size_t a4xx_snapshot_vbif_debugbus(struct kgsl_device *device,
			u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header =
		(struct kgsl_snapshot_debugbus *)buf;
	struct adreno_debugbus_block *block = priv;
	int i, j;
	/*
	 * Total number of VBIF data words considering 3 sections:
	 * 2 arbiter blocks of 16 words
	 * 5 AXI XIN blocks of 4 dwords each
	 * 5 core clock side XIN blocks of 5 dwords each
	 */
	unsigned int dwords = (16 * A4XX_NUM_AXI_ARB_BLOCKS) +
			(4 * A4XX_NUM_XIN_BLOCKS) + (5 * A4XX_NUM_XIN_BLOCKS);
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	size_t size;
	unsigned int reg_clk;

	size = (dwords * sizeof(unsigned int)) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}
	header->id = block->block_id;
	header->count = dwords;

	kgsl_regread(device, A4XX_VBIF_CLKON, &reg_clk);
	kgsl_regwrite(device, A4XX_VBIF_CLKON, reg_clk |
			(A4XX_VBIF_CLKON_FORCE_ON_TESTBUS_MASK <<
			A4XX_VBIF_CLKON_FORCE_ON_TESTBUS_SHIFT));
	kgsl_regwrite(device, A4XX_VBIF_TEST_BUS1_CTRL0, 0);
	kgsl_regwrite(device, A4XX_VBIF_TEST_BUS_OUT_CTRL,
			(A4XX_VBIF_TEST_BUS_OUT_CTRL_EN_MASK <<
			A4XX_VBIF_TEST_BUS_OUT_CTRL_EN_SHIFT));
	for (i = 0; i < A4XX_NUM_AXI_ARB_BLOCKS; i++) {
		kgsl_regwrite(device, A4XX_VBIF_TEST_BUS2_CTRL0,
			(1 << (i + 16)));
		for (j = 0; j < 16; j++) {
			kgsl_regwrite(device, A4XX_VBIF_TEST_BUS2_CTRL1,
				((j & A4XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_MASK)
				<< A4XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_SHIFT));
			kgsl_regread(device, A4XX_VBIF_TEST_BUS_OUT,
					data);
			data++;
		}
	}

	/* XIN blocks AXI side */
	for (i = 0; i < A4XX_NUM_XIN_BLOCKS; i++) {
		kgsl_regwrite(device, A4XX_VBIF_TEST_BUS2_CTRL0, 1 << i);
		for (j = 0; j < 4; j++) {
			kgsl_regwrite(device, A4XX_VBIF_TEST_BUS2_CTRL1,
				((j & A4XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_MASK)
				<< A4XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_SHIFT));
			kgsl_regread(device, A4XX_VBIF_TEST_BUS_OUT,
				data);
			data++;
		}
	}

	/* XIN blocks core clock side */
	for (i = 0; i < A4XX_NUM_XIN_BLOCKS; i++) {
		kgsl_regwrite(device, A4XX_VBIF_TEST_BUS1_CTRL0, 1 << i);
		for (j = 0; j < 5; j++) {
			kgsl_regwrite(device, A4XX_VBIF_TEST_BUS1_CTRL1,
				((j & A4XX_VBIF_TEST_BUS1_CTRL1_DATA_SEL_MASK)
				<< A4XX_VBIF_TEST_BUS1_CTRL1_DATA_SEL_SHIFT));
			kgsl_regread(device, A4XX_VBIF_TEST_BUS_OUT,
				data);
			data++;
		}
	}
	/* restore the clock of VBIF */
	kgsl_regwrite(device, A4XX_VBIF_CLKON, reg_clk);
	return size;
}

/*
 * a4xx_snapshot_debugbus_block() - Capture debug data for a gpu block
 * @device: Pointer to device
 * @buf: Memory where data is captured
 * @remain: Number of bytes left in snapshot
 * @priv: Pointer to debug bus block
 *
 * Returns the number of bytes written
 */
static size_t a4xx_snapshot_debugbus_block(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header =
		(struct kgsl_snapshot_debugbus *)buf;
	struct adreno_debugbus_block *block = priv;
	int i;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int dwords;
	size_t size;

	dwords = block->dwords;

	/* For a4xx each debug bus data unit is 2 DWRODS */
	size = (dwords * sizeof(unsigned int) * 2) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = block->block_id;
	header->count = dwords * 2;

	for (i = 0; i < dwords; i++)
		a4xx_rbbm_debug_bus_read(device, block->block_id, i,
					&data[i*2]);

	return size;
}

/*
 * a4xx_snapshot_debugbus() - Capture debug bus data
 * @device: The device for which data is captured
 * @snapshot: Pointer to the snapshot instance
 */
static void a4xx_snapshot_debugbus(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i;

	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_CTLM,
		0xf << A4XX_RBBM_CFG_DEBBUS_CTLT_ENABLE_SHIFT);

	for (i = 0; i < ARRAY_SIZE(a4xx_debugbus_blocks); i++) {
		if (A4XX_RBBM_DEBBUS_VBIF_ID ==
			a4xx_debugbus_blocks[i].block_id)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot, a4xx_snapshot_vbif_debugbus,
				(void *) &a4xx_debugbus_blocks[i]);
		else
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot, a4xx_snapshot_debugbus_block,
				(void *) &a4xx_debugbus_blocks[i]);
	}

	if (!adreno_is_a405(adreno_dev)) {
		for (i = 0; i < ARRAY_SIZE(a420_debugbus_blocks); i++)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot, a4xx_snapshot_debugbus_block,
				(void *) &a420_debugbus_blocks[i]);

	}
}

static void a4xx_reset_hlsq(struct kgsl_device *device)
{
	unsigned int val, dummy = 0;

	/* reset cp */
	kgsl_regwrite(device, A4XX_RBBM_BLOCK_SW_RESET_CMD, 1 << 20);
	kgsl_regread(device, A4XX_RBBM_BLOCK_SW_RESET_CMD, &dummy);

	/* reset hlsq */
	kgsl_regwrite(device, A4XX_RBBM_BLOCK_SW_RESET_CMD, 1 << 25);
	kgsl_regread(device, A4XX_RBBM_BLOCK_SW_RESET_CMD, &dummy);

	/* clear reset bits */
	kgsl_regwrite(device, A4XX_RBBM_BLOCK_SW_RESET_CMD, 0);
	kgsl_regread(device, A4XX_RBBM_BLOCK_SW_RESET_CMD, &dummy);


	/* set HLSQ_TIMEOUT_THRESHOLD.cycle_timeout_limit_sp to 26 */
	kgsl_regread(device, A4XX_HLSQ_TIMEOUT_THRESHOLD, &val);
	val &= (0x1F << 24);
	val |= (26 << 24);
	kgsl_regwrite(device, A4XX_HLSQ_TIMEOUT_THRESHOLD, val);
}

/*
 * a4xx_snapshot() - A4XX GPU snapshot function
 * @adreno_dev: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the A4XX specific bits and pieces are grabbed
 * into the snapshot memory
 */
void a4xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_snapshot_data *snap_data = gpudev->snapshot_data;

	/* Disable SP clock gating for the debug bus to work */
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_SP0, 0);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_SP1, 0);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_SP2, 0);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_SP3, 0);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_SP0, 0);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_SP1, 0);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_SP2, 0);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_SP3, 0);

	/* Disable top level clock gating the debug bus to work */
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL, 0);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2, 0);

	/* Master set of (non debug) registers */

	SNAPSHOT_REGISTERS(device, snapshot, a4xx_registers);

	if (adreno_is_a430(adreno_dev))
		SNAPSHOT_REGISTERS(device, snapshot, a4xx_sp_tp_registers);

	if (adreno_is_a420(adreno_dev))
		SNAPSHOT_REGISTERS(device, snapshot, a4xx_xpu_registers);

	if (adreno_is_a430v2(adreno_dev))
		SNAPSHOT_REGISTERS(device, snapshot, a4xx_ppd_registers);

	adreno_snapshot_vbif_registers(device, snapshot,
		a4xx_vbif_snapshot_registers,
		ARRAY_SIZE(a4xx_vbif_snapshot_registers));

	kgsl_snapshot_indexed_registers(device, snapshot,
		A4XX_CP_STATE_DEBUG_INDEX, A4XX_CP_STATE_DEBUG_DATA,
		0, snap_data->sect_sizes->cp_pfp);

	 /* CP_ME indexed registers */
	 kgsl_snapshot_indexed_registers(device, snapshot,
		A4XX_CP_ME_CNTL, A4XX_CP_ME_STATUS, 64, 44);

	/* VPC memory */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, adreno_snapshot_vpc_memory,
		&snap_data->sect_sizes->vpc_mem);

	/* CP MEQ */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, adreno_snapshot_cp_meq,
		&snap_data->sect_sizes->cp_meq);

	/* CP PFP and PM4 */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, adreno_snapshot_cp_pfp_ram, NULL);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, adreno_snapshot_cp_pm4_ram, NULL);

	/* CP ROQ */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, adreno_snapshot_cp_roq,
		&snap_data->sect_sizes->roq);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, adreno_snapshot_cp_merciu,
		&snap_data->sect_sizes->cp_merciu);

	/* Debug bus */
	a4xx_snapshot_debugbus(device, snapshot);

	if (!adreno_is_a430(adreno_dev)) {
		a4xx_reset_hlsq(device);
		SNAPSHOT_REGISTERS(device, snapshot, a4xx_sp_tp_registers);
	}

	/* Shader working/shadow memory */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a4xx_snapshot_shader_memory,
		&snap_data->sect_sizes->shader_mem);
}
