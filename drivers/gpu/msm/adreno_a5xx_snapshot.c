/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include "adreno_snapshot.h"
#include "a5xx_reg.h"
#include "adreno_a5xx.h"

enum a5xx_rbbm_debbus_id {
	A5XX_RBBM_DBGBUS_CP          = 0x1,
	A5XX_RBBM_DBGBUS_RBBM        = 0x2,
	A5XX_RBBM_DBGBUS_VBIF        = 0x3,
	A5XX_RBBM_DBGBUS_HLSQ        = 0x4,
	A5XX_RBBM_DBGBUS_UCHE        = 0x5,
	A5XX_RBBM_DBGBUS_DPM         = 0x6,
	A5XX_RBBM_DBGBUS_TESS        = 0x7,
	A5XX_RBBM_DBGBUS_PC          = 0x8,
	A5XX_RBBM_DBGBUS_VFDP        = 0x9,
	A5XX_RBBM_DBGBUS_VPC         = 0xa,
	A5XX_RBBM_DBGBUS_TSE         = 0xb,
	A5XX_RBBM_DBGBUS_RAS         = 0xc,
	A5XX_RBBM_DBGBUS_VSC         = 0xd,
	A5XX_RBBM_DBGBUS_COM         = 0xe,
	A5XX_RBBM_DBGBUS_DCOM        = 0xf,
	A5XX_RBBM_DBGBUS_LRZ         = 0x10,
	A5XX_RBBM_DBGBUS_A2D_DSP     = 0x11,
	A5XX_RBBM_DBGBUS_CCUFCHE     = 0x12,
	A5XX_RBBM_DBGBUS_GPMU        = 0x13,
	A5XX_RBBM_DBGBUS_RBP         = 0x14,
	A5XX_RBBM_DBGBUS_HM          = 0x15,
	A5XX_RBBM_DBGBUS_RBBM_CFG    = 0x16,
	A5XX_RBBM_DBGBUS_VBIF_CX     = 0x17,
	A5XX_RBBM_DBGBUS_GPC         = 0x1d,
	A5XX_RBBM_DBGBUS_LARC        = 0x1e,
	A5XX_RBBM_DBGBUS_HLSQ_SPTP   = 0x1f,
	A5XX_RBBM_DBGBUS_RB_0        = 0x20,
	A5XX_RBBM_DBGBUS_RB_1        = 0x21,
	A5XX_RBBM_DBGBUS_RB_2        = 0x22,
	A5XX_RBBM_DBGBUS_RB_3        = 0x23,
	A5XX_RBBM_DBGBUS_CCU_0       = 0x28,
	A5XX_RBBM_DBGBUS_CCU_1       = 0x29,
	A5XX_RBBM_DBGBUS_CCU_2       = 0x2a,
	A5XX_RBBM_DBGBUS_CCU_3       = 0x2b,
	A5XX_RBBM_DBGBUS_A2D_RAS_0   = 0x30,
	A5XX_RBBM_DBGBUS_A2D_RAS_1   = 0x31,
	A5XX_RBBM_DBGBUS_A2D_RAS_2   = 0x32,
	A5XX_RBBM_DBGBUS_A2D_RAS_3   = 0x33,
	A5XX_RBBM_DBGBUS_VFD_0       = 0x38,
	A5XX_RBBM_DBGBUS_VFD_1       = 0x39,
	A5XX_RBBM_DBGBUS_VFD_2       = 0x3a,
	A5XX_RBBM_DBGBUS_VFD_3       = 0x3b,
	A5XX_RBBM_DBGBUS_SP_0        = 0x40,
	A5XX_RBBM_DBGBUS_SP_1        = 0x41,
	A5XX_RBBM_DBGBUS_SP_2        = 0x42,
	A5XX_RBBM_DBGBUS_SP_3        = 0x43,
	A5XX_RBBM_DBGBUS_TPL1_0      = 0x48,
	A5XX_RBBM_DBGBUS_TPL1_1      = 0x49,
	A5XX_RBBM_DBGBUS_TPL1_2      = 0x4a,
	A5XX_RBBM_DBGBUS_TPL1_3      = 0x4b
};

static const struct adreno_debugbus_block a5xx_debugbus_blocks[] = {
	{  A5XX_RBBM_DBGBUS_CP, 0x100, },
	{  A5XX_RBBM_DBGBUS_RBBM, 0x100, },
	{  A5XX_RBBM_DBGBUS_VBIF, 0x100, },
	{  A5XX_RBBM_DBGBUS_HLSQ, 0x100, },
	{  A5XX_RBBM_DBGBUS_UCHE, 0x100, },
	{  A5XX_RBBM_DBGBUS_DPM, 0x100, },
	{  A5XX_RBBM_DBGBUS_TESS, 0x100, },
	{  A5XX_RBBM_DBGBUS_PC, 0x100, },
	{  A5XX_RBBM_DBGBUS_VFDP, 0x100, },
	{  A5XX_RBBM_DBGBUS_VPC, 0x100, },
	{  A5XX_RBBM_DBGBUS_TSE, 0x100, },
	{  A5XX_RBBM_DBGBUS_RAS, 0x100, },
	{  A5XX_RBBM_DBGBUS_VSC, 0x100, },
	{  A5XX_RBBM_DBGBUS_COM, 0x100, },
	{  A5XX_RBBM_DBGBUS_DCOM, 0x100, },
	{  A5XX_RBBM_DBGBUS_LRZ, 0x100, },
	{  A5XX_RBBM_DBGBUS_A2D_DSP, 0x100, },
	{  A5XX_RBBM_DBGBUS_CCUFCHE, 0x100, },
	{  A5XX_RBBM_DBGBUS_GPMU, 0x100, },
	{  A5XX_RBBM_DBGBUS_RBP, 0x100, },
	{  A5XX_RBBM_DBGBUS_HM, 0x100, },
	{  A5XX_RBBM_DBGBUS_RBBM_CFG, 0x100, },
	{  A5XX_RBBM_DBGBUS_VBIF_CX, 0x100, },
	{  A5XX_RBBM_DBGBUS_GPC, 0x100, },
	{  A5XX_RBBM_DBGBUS_LARC, 0x100, },
	{  A5XX_RBBM_DBGBUS_HLSQ_SPTP, 0x100, },
	{  A5XX_RBBM_DBGBUS_RB_0, 0x100, },
	{  A5XX_RBBM_DBGBUS_RB_1, 0x100, },
	{  A5XX_RBBM_DBGBUS_RB_2, 0x100, },
	{  A5XX_RBBM_DBGBUS_RB_3, 0x100, },
	{  A5XX_RBBM_DBGBUS_CCU_0, 0x100, },
	{  A5XX_RBBM_DBGBUS_CCU_1, 0x100, },
	{  A5XX_RBBM_DBGBUS_CCU_2, 0x100, },
	{  A5XX_RBBM_DBGBUS_CCU_3, 0x100, },
	{  A5XX_RBBM_DBGBUS_A2D_RAS_0, 0x100, },
	{  A5XX_RBBM_DBGBUS_A2D_RAS_1, 0x100, },
	{  A5XX_RBBM_DBGBUS_A2D_RAS_2, 0x100, },
	{  A5XX_RBBM_DBGBUS_A2D_RAS_3, 0x100, },
	{  A5XX_RBBM_DBGBUS_VFD_0, 0x100, },
	{  A5XX_RBBM_DBGBUS_VFD_1, 0x100, },
	{  A5XX_RBBM_DBGBUS_VFD_2, 0x100, },
	{  A5XX_RBBM_DBGBUS_VFD_3, 0x100, },
	{  A5XX_RBBM_DBGBUS_SP_0, 0x100, },
	{  A5XX_RBBM_DBGBUS_SP_1, 0x100, },
	{  A5XX_RBBM_DBGBUS_SP_2, 0x100, },
	{  A5XX_RBBM_DBGBUS_SP_3, 0x100, },
	{  A5XX_RBBM_DBGBUS_TPL1_0, 0x100, },
	{  A5XX_RBBM_DBGBUS_TPL1_1, 0x100, },
	{  A5XX_RBBM_DBGBUS_TPL1_2, 0x100, },
	{  A5XX_RBBM_DBGBUS_TPL1_3, 0x100, },
};

#define A5XX_NUM_AXI_ARB_BLOCKS	2
#define A5XX_NUM_XIN_BLOCKS	5

/* a5xx_snapshot_cp_pm4() - Dump PM4 data in snapshot */
static size_t a5xx_snapshot_cp_pm4(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	size_t size = adreno_dev->pm4_fw_size;

	if (remain < DEBUG_SECTION_SZ(size)) {
		SNAPSHOT_ERR_NOMEM(device, "CP PM4 RAM DEBUG");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_CP_PM4_RAM;
	header->size = size;

	memcpy(data, adreno_dev->pm4.hostptr, size * sizeof(uint32_t));

	return DEBUG_SECTION_SZ(size);
}

/* a5xx_snapshot_cp_pfp() - Dump the PFP data on snapshot */
static size_t a5xx_snapshot_cp_pfp(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int size = adreno_dev->pfp_fw_size;

	if (remain < DEBUG_SECTION_SZ(size)) {
		SNAPSHOT_ERR_NOMEM(device, "CP PFP RAM DEBUG");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_CP_PFP_RAM;
	header->size = size;

	memcpy(data, adreno_dev->pfp.hostptr, size * sizeof(uint32_t));

	return DEBUG_SECTION_SZ(size);
}

/* a5xx_rbbm_debug_bus_read() - Read data from trace bus */
static void a5xx_rbbm_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int reg;

	reg = (block_id << A5XX_RBBM_CFG_DBGBUS_SEL_PING_BLK_SEL_SHIFT) |
			(index << A5XX_RBBM_CFG_DBGBUS_SEL_PING_INDEX_SHIFT);
	kgsl_regwrite(device, A5XX_RBBM_CFG_DBGBUS_SEL_A, reg);
	kgsl_regwrite(device, A5XX_RBBM_CFG_DBGBUS_SEL_B, reg);
	kgsl_regwrite(device, A5XX_RBBM_CFG_DBGBUS_SEL_C, reg);
	kgsl_regwrite(device, A5XX_RBBM_CFG_DBGBUS_SEL_D, reg);

	kgsl_regread(device, A5XX_RBBM_CFG_DBGBUS_TRACE_BUF2, val);
	val++;
	kgsl_regread(device, A5XX_RBBM_CFG_DBGBUS_TRACE_BUF1, val);

}

/* a5xx_snapshot_vbif_debugbus() - Dump the VBIF debug data */
static size_t a5xx_snapshot_vbif_debugbus(struct kgsl_device *device,
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
	unsigned int dwords = (16 * A5XX_NUM_AXI_ARB_BLOCKS) +
			(4 * A5XX_NUM_XIN_BLOCKS) + (5 * A5XX_NUM_XIN_BLOCKS);
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

	kgsl_regread(device, A5XX_VBIF_CLKON, &reg_clk);
	kgsl_regwrite(device, A5XX_VBIF_CLKON, reg_clk |
			(A5XX_VBIF_CLKON_FORCE_ON_TESTBUS_MASK <<
			A5XX_VBIF_CLKON_FORCE_ON_TESTBUS_SHIFT));
	kgsl_regwrite(device, A5XX_VBIF_TEST_BUS1_CTRL0, 0);
	kgsl_regwrite(device, A5XX_VBIF_TEST_BUS_OUT_CTRL,
			(A5XX_VBIF_TEST_BUS_OUT_CTRL_EN_MASK <<
			A5XX_VBIF_TEST_BUS_OUT_CTRL_EN_SHIFT));
	for (i = 0; i < A5XX_NUM_AXI_ARB_BLOCKS; i++) {
		kgsl_regwrite(device, A5XX_VBIF_TEST_BUS2_CTRL0,
			(1 << (i + 16)));
		for (j = 0; j < 16; j++) {
			kgsl_regwrite(device, A5XX_VBIF_TEST_BUS2_CTRL1,
				((j & A5XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_MASK)
				<< A5XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_SHIFT));
			kgsl_regread(device, A5XX_VBIF_TEST_BUS_OUT,
					data);
			data++;
		}
	}

	/* XIN blocks AXI side */
	for (i = 0; i < A5XX_NUM_XIN_BLOCKS; i++) {
		kgsl_regwrite(device, A5XX_VBIF_TEST_BUS2_CTRL0, 1 << i);
		for (j = 0; j < 4; j++) {
			kgsl_regwrite(device, A5XX_VBIF_TEST_BUS2_CTRL1,
				((j & A5XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_MASK)
				<< A5XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_SHIFT));
			kgsl_regread(device, A5XX_VBIF_TEST_BUS_OUT,
				data);
			data++;
		}
	}

	/* XIN blocks core clock side */
	for (i = 0; i < A5XX_NUM_XIN_BLOCKS; i++) {
		kgsl_regwrite(device, A5XX_VBIF_TEST_BUS1_CTRL0, 1 << i);
		for (j = 0; j < 5; j++) {
			kgsl_regwrite(device, A5XX_VBIF_TEST_BUS1_CTRL1,
				((j & A5XX_VBIF_TEST_BUS1_CTRL1_DATA_SEL_MASK)
				<< A5XX_VBIF_TEST_BUS1_CTRL1_DATA_SEL_SHIFT));
			kgsl_regread(device, A5XX_VBIF_TEST_BUS_OUT,
				data);
			data++;
		}
	}
	/* restore the clock of VBIF */
	kgsl_regwrite(device, A5XX_VBIF_CLKON, reg_clk);
	return size;
}

/* a5xx_snapshot_debugbus_block() - Capture debug data for a gpu block */
static size_t a5xx_snapshot_debugbus_block(struct kgsl_device *device,
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

	/* For a5xx each debug bus data unit is 2 DWRODS */
	size = (dwords * sizeof(unsigned int) * 2) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = block->block_id;
	header->count = dwords * 2;

	for (i = 0; i < dwords; i++)
		a5xx_rbbm_debug_bus_read(device, block->block_id, i,
					&data[i*2]);

	return size;
}

/* a5xx_snapshot_debugbus() - Capture debug bus data */
static void a5xx_snapshot_debugbus(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot)
{
	int i;

	kgsl_regwrite(device, A5XX_RBBM_CFG_DBGBUS_CNTLM,
		0xf << A5XX_RBBM_CFG_DEBBUS_CTLTM_ENABLE_SHIFT);

	for (i = 0; i < ARRAY_SIZE(a5xx_debugbus_blocks); i++) {
		if (A5XX_RBBM_DBGBUS_VBIF == a5xx_debugbus_blocks[i].block_id)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot, a5xx_snapshot_vbif_debugbus,
				(void *) &a5xx_debugbus_blocks[i]);
		else
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot, a5xx_snapshot_debugbus_block,
				(void *) &a5xx_debugbus_blocks[i]);
	}
}

static const unsigned int a5xx_vbif_ver_20040000_registers[] = {
	/* VBIF version 0x20040000*/
	0x3000, 0x3007, 0x300C, 0x3014, 0x3018, 0x302C, 0x3030, 0x3030,
	0x3034, 0x3036, 0x3038, 0x3038, 0x303C, 0x303D, 0x3040, 0x3040,
	0x3042, 0x3042, 0x3049, 0x3049, 0x3058, 0x3058, 0x305A, 0x3061,
	0x3064, 0x3068, 0x306C, 0x306D, 0x3080, 0x3088, 0x308C, 0x308C,
	0x3090, 0x3094, 0x3098, 0x3098, 0x309C, 0x309C, 0x30C0, 0x30C0,
	0x30C8, 0x30C8, 0x30D0, 0x30D0, 0x30D8, 0x30D8, 0x30E0, 0x30E0,
	0x3100, 0x3100, 0x3108, 0x3108, 0x3110, 0x3110, 0x3118, 0x3118,
	0x3120, 0x3120, 0x3124, 0x3125, 0x3129, 0x3129, 0x3131, 0x3131,
	0x340C, 0x340C, 0x3410, 0x3410, 0x3800, 0x3801,
};

static const struct adreno_vbif_snapshot_registers
a5xx_vbif_snapshot_registers[] = {
	{ 0x20040000, a5xx_vbif_ver_20040000_registers,
				ARRAY_SIZE(a5xx_vbif_ver_20040000_registers)/2},
	{ 0x20040001, a5xx_vbif_ver_20040000_registers,
				ARRAY_SIZE(a5xx_vbif_ver_20040000_registers)/2},
};

/*
 * Set of registers to dump for A5XX on snapshot.
 * Registers in pairs - first value is the start offset, second
 * is the stop offset (inclusive)
 */

static const unsigned int a5xx_registers[] = {
	/* RBBM */
	0x0000, 0x0002, 0x0004, 0x0020, 0x0022, 0x0026, 0x0029, 0x002B,
	0x002E, 0x0035, 0x0038, 0x0042, 0x0044, 0x0044, 0x0047, 0x0095,
	0x0097, 0x00BB, 0x03A0, 0x0464, 0x0469, 0x046F, 0x04D2, 0x04D3,
	0x04E0, 0x0533, 0x0540, 0x0555, 0xF400, 0xF400, 0xF800, 0xF807,
	/* CP */
	0x0800, 0x081A, 0x081F, 0x0841, 0x0860, 0x0860, 0x0880, 0x08A0,
	0x0B00, 0x0B12, 0x0B15, 0x0B28, 0x0B78, 0x0B7F, 0x0BB0, 0x0BBD,
	/* VSC */
	0x0BC0, 0x0BC6, 0x0BD0, 0x0C53, 0x0C60, 0x0C61,
	/* GRAS */
	0x0C80, 0x0C82, 0x0C84, 0x0C85, 0x0C90, 0x0C98, 0x0CA0, 0x0CA0,
	0x0CB0, 0x0CB2, 0x2180, 0x2185, 0x2580, 0x2585,
	/* RB */
	0x0CC1, 0x0CC1, 0x0CC4, 0x0CC7, 0x0CCC, 0x0CCC, 0x0CD0, 0x0CD8,
	0x0CE0, 0x0CE5, 0x0CE8, 0x0CE8, 0x0CEC, 0x0CF1, 0x0CFB, 0x0D0E,
	0x2100, 0x211E, 0x2140, 0x2145, 0x2500, 0x251E, 0x2540, 0x2545,
	/* PC */
	0x0D10, 0x0D17, 0x0D20, 0x0D23, 0x0D30, 0x0D30, 0x20C0, 0x20C0,
	0x24C0, 0x24C0,
	/* VFD */
	0x0E40, 0x0E43, 0x0E4A, 0x0E4A, 0x0E50, 0x0E57,
	/* VPC */
	0x0E60, 0x0E7C,
	/* UCHE */
	0x0E80, 0x0E8E, 0x0E90, 0x0E96, 0xEA0, 0xEA8, 0xEB0, 0xEB2,

	/* RB CTX 0 */
	0xE140, 0xE147, 0xE150, 0xE187, 0xE1A0, 0xE1A9, 0xE1B0, 0xE1B6,
	0xE1C0, 0xE1C7, 0xE1D0, 0xE1D1, 0xE200, 0xE201, 0xE210, 0xE21C,
	0xE240, 0xE268,
	/* GRAS CTX 0 */
	0xE000, 0xE006, 0xE010, 0xE09A, 0xE0A0, 0xE0A4, 0xE0AA, 0xE0EB,
	0xE100, 0xE105,
	/* PC CTX 0 */
	0xE380, 0xE38F, 0xE3B0, 0xE3B0,
	/* VFD CTX 0 */
	0xE400, 0xE405, 0xE408, 0xE4E9, 0xE4F0, 0xE4F0,
	/* VPC CTX 0 */
	0xE280, 0xE280, 0xE282, 0xE2A3, 0xE2A5, 0xE2C2,

	/* RB CTX 1 */
	0xE940, 0xE947, 0xE950, 0xE987, 0xE9A0, 0xE9A9, 0xE9B0, 0xE9B6,
	0xE9C0, 0xE9C7, 0xE9D0, 0xE9D1, 0xEA00, 0xEA01, 0xEA10, 0xEA1C,
	0xEA40, 0xEA68,
	/* GRAS CTX 1 */
	0xE800, 0xE806, 0xE810, 0xE89A, 0xE8A0, 0xE8A4, 0xE8AA, 0xE8EB,
	0xE900, 0xE905,
	/* PC CTX 1 */
	0xEB80, 0xEB8F, 0xEBB0, 0xEBB0,
	/* VFD CTX 1 */
	0xEC00, 0xEC05, 0xEC08, 0xECE9, 0xECF0, 0xECF0,
	/* VPC CTX 1 */
	0xEA80, 0xEA80, 0xEA82, 0xEAA3, 0xEAA5, 0xEAC2,
	/* GPMU */
	0xA800, 0xA8FF, 0xAC60, 0xAC60,
	/* DPM */
	0xB000, 0xB97F, 0xB9A0, 0xB9BF,
};

/*
 * The HLSQ registers can only be read via the crash dumper (not AHB) so they
 * need to be in their own array because the array above does double duty for
 * the fallback path too
 */
static const unsigned int a5xx_hlsq_registers[] = {
	/* SP non context */
	0x0EC0, 0xEC2, 0xED0, 0xEE0, 0xEF0, 0xEF2, 0xEFA, 0xEFF,
	/* SP CTX 0 2D */
	0x2040, 0x2040,
	/* SP CTX 1 2D */
	0x2440, 0x2440,
	/* SP CTXT 0 3D */
	0xE580, 0xE580, 0xE584, 0xE58B, 0xE590, 0xE5B1, 0xE5C0, 0xE5DF,
	0xE5F0, 0xE5F9, 0xE600, 0xE608, 0xE610, 0xE631, 0xE640, 0xE661,
	0xE670, 0xE673, 0xE6F0, 0xE6F0,
	/* SP CTXT 1 3D */
	0xED80, 0xED80, 0xED84, 0xED8B, 0xED90, 0xEDB1, 0xEDC0, 0xEDDF,
	0xEDF0, 0xEDF9, 0xEE00, 0xEE08, 0xEE10, 0xEE31, 0xEE40, 0xEE61,
	0xEE70, 0xEE73, 0xEEF0, 0xEEF0,
	/* TP non context */
	0xF00, 0xF03, 0xF08, 0xF08, 0xF10, 0xF1B,
	/* TP CTX 0 2D */
	0x2000, 0x2009,
	/* TP CTX 1 2D */
	0x2400, 0x2409,
	/* TP CTX 0 3D */
	0xE700, 0xE707, 0xE70E, 0xE731,
	0xE750, 0xE751, 0xE75A, 0xE764, 0xE76C, 0xE77F,
	/* TP CTX 1 3D */
	0xEF00, 0xEF07, 0xEF0E, 0xEF31,
	0xEF50, 0xEF51, 0xEF5A, 0xEF64, 0xEF6C, 0xEF7F,
	/* HLSQ non context */
	0xE00, 0xE01, 0xE04, 0xE06, 0xE08, 0xE09, 0xE10, 0xE17,
	0xE20, 0xE25,
	/* HLSQ CTXT 0 3D */
	0xE784, 0xE789, 0xE78B, 0xE796, 0xE7A0, 0xE7A2, 0xE7B0, 0xE7BB,
	0xE7C0, 0xE7DD, 0xE7E0, 0xE7E1,
	/* HLSQ CTXT 1 3D */
	0xEF84, 0xEF89, 0xEF8B, 0xEF96, 0xEFA0, 0xEFA2, 0xEFB0, 0xEFBB,
	0xEFC0, 0xEFDD, 0xEFE0, 0xEFE1,
};

#define A5XX_NUM_SHADER_BANKS 4
#define A5XX_SHADER_STATETYPE_SHIFT 8

enum a5xx_shader_obj {
	A5XX_TP_W_MEMOBJ = 1,
	A5XX_TP_W_SAMPLER = 2,
	A5XX_TP_W_MIPMAP_BASE = 3,
	A5XX_TP_W_MEMOBJ_TAG = 4,
	A5XX_TP_W_SAMPLER_TAG = 5,
	A5XX_TP_S_3D_MEMOBJ = 6,
	A5XX_TP_S_3D_SAMPLER = 0x7,
	A5XX_TP_S_3D_MEMOBJ_TAG = 0x8,
	A5XX_TP_S_3D_SAMPLER_TAG = 0x9,
	A5XX_TP_S_CS_MEMOBJ = 0xA,
	A5XX_TP_S_CS_SAMPLER = 0xB,
	A5XX_TP_S_CS_MEMOBJ_TAG = 0xC,
	A5XX_TP_S_CS_SAMPLER_TAG = 0xD,
	A5XX_SP_W_INSTR = 0xE,
	A5XX_SP_W_CONST = 0xF,
	A5XX_SP_W_UAV_SIZE = 0x10,
	A5XX_SP_W_CB_SIZE = 0x11,
	A5XX_SP_W_UAV_BASE = 0x12,
	A5XX_SP_W_CB_BASE = 0x13,
	A5XX_SP_W_INST_TAG = 0x14,
	A5XX_SP_W_STATE = 0x15,
	A5XX_SP_S_3D_INSTR = 0x16,
	A5XX_SP_S_3D_CONST = 0x17,
	A5XX_SP_S_3D_CB_BASE = 0x18,
	A5XX_SP_S_3D_CB_SIZE = 0x19,
	A5XX_SP_S_3D_UAV_BASE = 0x1A,
	A5XX_SP_S_3D_UAV_SIZE = 0x1B,
	A5XX_SP_S_CS_INSTR = 0x1C,
	A5XX_SP_S_CS_CONST = 0x1D,
	A5XX_SP_S_CS_CB_BASE = 0x1E,
	A5XX_SP_S_CS_CB_SIZE = 0x1F,
	A5XX_SP_S_CS_UAV_BASE = 0x20,
	A5XX_SP_S_CS_UAV_SIZE = 0x21,
	A5XX_SP_S_3D_INSTR_DIRTY = 0x22,
	A5XX_SP_S_3D_CONST_DIRTY = 0x23,
	A5XX_SP_S_3D_CB_BASE_DIRTY = 0x24,
	A5XX_SP_S_3D_CB_SIZE_DIRTY = 0x25,
	A5XX_SP_S_3D_UAV_BASE_DIRTY = 0x26,
	A5XX_SP_S_3D_UAV_SIZE_DIRTY = 0x27,
	A5XX_SP_S_CS_INSTR_DIRTY = 0x28,
	A5XX_SP_S_CS_CONST_DIRTY = 0x29,
	A5XX_SP_S_CS_CB_BASE_DIRTY = 0x2A,
	A5XX_SP_S_CS_CB_SIZE_DIRTY = 0x2B,
	A5XX_SP_S_CS_UAV_BASE_DIRTY = 0x2C,
	A5XX_SP_S_CS_UAV_SIZE_DIRTY = 0x2D,
	A5XX_HLSQ_ICB = 0x2E,
	A5XX_HLSQ_ICB_DIRTY = 0x2F,
	A5XX_HLSQ_ICB_CB_BASE_DIRTY = 0x30,
	A5XX_SP_POWER_RESTORE_RAM = 0x40,
	A5XX_SP_POWER_RESTORE_RAM_TAG = 0x41,
	A5XX_TP_POWER_RESTORE_RAM = 0x42,
	A5XX_TP_POWER_RESTORE_RAM_TAG = 0x43,

};

struct a5xx_shader_block {
	unsigned int statetype;
	unsigned int sz;
	uint64_t offset;
};

struct a5xx_shader_block_info {
	struct a5xx_shader_block *block;
	unsigned int bank;
	uint64_t offset;
};

static struct a5xx_shader_block a5xx_shader_blocks[] = {
	{A5XX_TP_W_MEMOBJ,              0x200},
	{A5XX_TP_W_MIPMAP_BASE,         0x3C0},
	{A5XX_TP_W_SAMPLER_TAG,          0x40},
	{A5XX_TP_S_3D_SAMPLER,           0x80},
	{A5XX_TP_S_3D_SAMPLER_TAG,       0x20},
	{A5XX_TP_S_CS_SAMPLER,           0x40},
	{A5XX_TP_S_CS_SAMPLER_TAG,       0x10},
	{A5XX_SP_W_CONST,               0x800},
	{A5XX_SP_W_CB_SIZE,              0x30},
	{A5XX_SP_W_CB_BASE,              0xF0},
	{A5XX_SP_W_STATE,                 0x1},
	{A5XX_SP_S_3D_CONST,            0x800},
	{A5XX_SP_S_3D_CB_SIZE,           0x28},
	{A5XX_SP_S_3D_UAV_SIZE,          0x80},
	{A5XX_SP_S_CS_CONST,            0x400},
	{A5XX_SP_S_CS_CB_SIZE,            0x8},
	{A5XX_SP_S_CS_UAV_SIZE,          0x80},
	{A5XX_SP_S_3D_CONST_DIRTY,       0x12},
	{A5XX_SP_S_3D_CB_SIZE_DIRTY,      0x1},
	{A5XX_SP_S_3D_UAV_SIZE_DIRTY,     0x2},
	{A5XX_SP_S_CS_CONST_DIRTY,        0xA},
	{A5XX_SP_S_CS_CB_SIZE_DIRTY,      0x1},
	{A5XX_SP_S_CS_UAV_SIZE_DIRTY,     0x2},
	{A5XX_HLSQ_ICB_DIRTY,             0xB},
	{A5XX_SP_POWER_RESTORE_RAM_TAG,   0xA},
	{A5XX_TP_POWER_RESTORE_RAM_TAG,   0xA},
	{A5XX_TP_W_SAMPLER,              0x80},
	{A5XX_TP_W_MEMOBJ_TAG,           0x40},
	{A5XX_TP_S_3D_MEMOBJ,           0x200},
	{A5XX_TP_S_3D_MEMOBJ_TAG,        0x20},
	{A5XX_TP_S_CS_MEMOBJ,           0x100},
	{A5XX_TP_S_CS_MEMOBJ_TAG,        0x10},
	{A5XX_SP_W_INSTR,               0x800},
	{A5XX_SP_W_UAV_SIZE,             0x80},
	{A5XX_SP_W_UAV_BASE,             0x80},
	{A5XX_SP_W_INST_TAG,             0x40},
	{A5XX_SP_S_3D_INSTR,            0x800},
	{A5XX_SP_S_3D_CB_BASE,           0xC8},
	{A5XX_SP_S_3D_UAV_BASE,          0x80},
	{A5XX_SP_S_CS_INSTR,            0x400},
	{A5XX_SP_S_CS_CB_BASE,           0x28},
	{A5XX_SP_S_CS_UAV_BASE,          0x80},
	{A5XX_SP_S_3D_INSTR_DIRTY,        0x1},
	{A5XX_SP_S_3D_CB_BASE_DIRTY,      0x5},
	{A5XX_SP_S_3D_UAV_BASE_DIRTY,     0x2},
	{A5XX_SP_S_CS_INSTR_DIRTY,        0x1},
	{A5XX_SP_S_CS_CB_BASE_DIRTY,      0x1},
	{A5XX_SP_S_CS_UAV_BASE_DIRTY,     0x2},
	{A5XX_HLSQ_ICB,                 0x200},
	{A5XX_HLSQ_ICB_CB_BASE_DIRTY,     0x4},
	{A5XX_SP_POWER_RESTORE_RAM,     0x140},
	{A5XX_TP_POWER_RESTORE_RAM,      0x40},
};

static struct kgsl_memdesc capturescript;
static struct kgsl_memdesc registers;
static bool crash_dump_valid;

static size_t a5xx_snapshot_shader_memory(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_shader *header =
		(struct kgsl_snapshot_shader *) buf;
	struct a5xx_shader_block_info *info =
		(struct a5xx_shader_block_info *) priv;
	struct a5xx_shader_block *block = info->block;
	unsigned int *data = (unsigned int *) (buf + sizeof(*header));

	if (remain < SHADER_SECTION_SZ(block->sz)) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	header->type = block->statetype;
	header->index = info->bank;
	header->size = block->sz;

	memcpy(data, registers.hostptr + info->offset, block->sz);

	return SHADER_SECTION_SZ(block->sz);
}

static void a5xx_snapshot_shader(struct kgsl_device *device,
			   struct kgsl_snapshot *snapshot)
{
	unsigned int i, j;
	struct a5xx_shader_block_info info;

	/* Shader blocks can only be read by the crash dumper */
	if (crash_dump_valid == false)
		return;

	for (i = 0; i < ARRAY_SIZE(a5xx_shader_blocks); i++) {
		for (j = 0; j < A5XX_NUM_SHADER_BANKS; j++) {
			info.block = &a5xx_shader_blocks[i];
			info.bank = j;
			info.offset = a5xx_shader_blocks[i].offset +
				(j * a5xx_shader_blocks[i].sz);

			/* Shader working/shadow memory */
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_SHADER,
				snapshot, a5xx_snapshot_shader_memory, &info);
		}
	}
}

static size_t a5xx_legacy_snapshot_registers(struct kgsl_device *device,
		u8 *buf, size_t remain)
{
	struct kgsl_snapshot_registers regs = {
		.regs = a5xx_registers,
		.count = ARRAY_SIZE(a5xx_registers) / 2,
	};

	return kgsl_snapshot_dump_registers(device, buf, remain, &regs);
}

static struct cdregs {
	const unsigned int *regs;
	unsigned int size;
} _a5xx_cd_registers[] = {
	{ a5xx_registers, ARRAY_SIZE(a5xx_registers) },
	{ a5xx_hlsq_registers, ARRAY_SIZE(a5xx_hlsq_registers) },
};

#define REG_PAIR_COUNT(_a, _i) \
	(((_a)[(2 * (_i)) + 1] - (_a)[2 * (_i)]) + 1)

static size_t a5xx_snapshot_registers(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header = (struct kgsl_snapshot_regs *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int *src = (unsigned int *) registers.hostptr;
	unsigned int i, j, k;
	unsigned int count = 0;

	if (crash_dump_valid == false)
		return a5xx_legacy_snapshot_registers(device, buf, remain);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	for (i = 0; i < ARRAY_SIZE(_a5xx_cd_registers); i++) {
		struct cdregs *regs = &_a5xx_cd_registers[i];

		for (j = 0; j < regs->size / 2; j++) {
			unsigned int start = regs->regs[2 * j];
			unsigned int end = regs->regs[(2 * j) + 1];

			if (remain < ((end - start) + 1) * 8) {
				SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
				goto out;
			}

			remain -= ((end - start) + 1) * 8;

			for (k = start; k <= end; k++, count++) {
				*data++ = k;
				*data++ = *src++;
			}
		}
	}

out:
	header->count = count;

	/* Return the size of the section */
	return (count * 8) + sizeof(*header);
}

static void _a5xx_do_crashdump(struct kgsl_device *device)
{
	unsigned long wait_time;
	unsigned int reg = 0;
	unsigned int val;

	crash_dump_valid = false;

	if (capturescript.gpuaddr == 0 || registers.gpuaddr == 0)
		return;

	/* IF the SMMU is stalled we cannot do a crash dump */
	kgsl_regread(device, A5XX_RBBM_STATUS3, &val);
	if (val & BIT(24))
		return;

	/* Turn on APRIV so we can access the buffers */
	kgsl_regwrite(device, A5XX_CP_CNTL, 1);

	kgsl_regwrite(device, A5XX_CP_CRASH_SCRIPT_BASE_LO,
			lower_32_bits(capturescript.gpuaddr));
	kgsl_regwrite(device, A5XX_CP_CRASH_SCRIPT_BASE_HI,
			upper_32_bits(capturescript.gpuaddr));
	kgsl_regwrite(device, A5XX_CP_CRASH_DUMP_CNTL, 1);

	wait_time = jiffies + msecs_to_jiffies(CP_CRASH_DUMPER_TIMEOUT);
	while (!time_after(jiffies, wait_time)) {
		kgsl_regread(device, A5XX_CP_CRASH_DUMP_CNTL, &reg);
		if (reg & 0x4)
			break;
		cpu_relax();
	}

	kgsl_regwrite(device, A5XX_CP_CNTL, 0);

	if (!(reg & 0x4)) {
		KGSL_CORE_ERR("Crash dump timed out: 0x%X\n", reg);
		return;
	}

	crash_dump_valid = true;
}

/*
 * a5xx_snapshot() - A5XX GPU snapshot function
 * @adreno_dev: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the A5XX specific bits and pieces are grabbed
 * into the snapshot memory
 */
void a5xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_snapshot_data *snap_data = gpudev->snapshot_data;
	unsigned int reg;

	/* Disable Clock gating temporarily for the debug bus to work */
	a5xx_hwcg_set(adreno_dev, false);

	/* Try to run the crash dumper */
	_a5xx_do_crashdump(device);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
		snapshot, a5xx_snapshot_registers, NULL);

	adreno_snapshot_vbif_registers(device, snapshot,
		a5xx_vbif_snapshot_registers,
		ARRAY_SIZE(a5xx_vbif_snapshot_registers));

	/* CP_PFP indexed registers */
	kgsl_snapshot_indexed_registers(device, snapshot,
		A5XX_CP_PFP_STAT_ADDR, A5XX_CP_PFP_STAT_DATA,
		0, snap_data->sect_sizes->cp_pfp);

	 /* CP_ME indexed registers */
	 kgsl_snapshot_indexed_registers(device, snapshot,
		A5XX_CP_ME_STAT_ADDR, A5XX_CP_ME_STAT_DATA,
		0, snap_data->sect_sizes->cp_me);

	 /* CP_DRAW_STATE */
	 kgsl_snapshot_indexed_registers(device, snapshot,
		A5XX_CP_DRAW_STATE_ADDR, A5XX_CP_DRAW_STATE_DATA,
		0, 128);

	 /*
	  * CP needs to be halted on a530v1 before reading CP_PFP_UCODE_DBG_DATA
	  * and CP_PM4_UCODE_DBG_DATA registers
	  */
	 if (adreno_is_a530v1(adreno_dev)) {
		adreno_readreg(adreno_dev, ADRENO_REG_CP_ME_CNTL, &reg);
		reg |= (1 << 27) | (1 << 28);
		adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_CNTL, reg);
	 }

	 /* ME_UCODE Cache */
	 kgsl_snapshot_indexed_registers(device, snapshot,
		A5XX_CP_ME_UCODE_DBG_ADDR, A5XX_CP_ME_UCODE_DBG_DATA,
		0, 0x53F);

	 /* PFP_UCODE Cache */
	 kgsl_snapshot_indexed_registers(device, snapshot,
		A5XX_CP_PFP_UCODE_DBG_ADDR, A5XX_CP_PFP_UCODE_DBG_DATA,
		0, 0x53F);

	/* CP MEQ */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, adreno_snapshot_cp_meq,
		&snap_data->sect_sizes->cp_meq);

	/* CP ROQ */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, adreno_snapshot_cp_roq,
		&snap_data->sect_sizes->roq);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, adreno_snapshot_cp_merciu,
		&snap_data->sect_sizes->cp_merciu);

	/* CP PFP and PM4 */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a5xx_snapshot_cp_pfp, NULL);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a5xx_snapshot_cp_pm4, NULL);

	/* Shader memory */
	a5xx_snapshot_shader(device, snapshot);

	/* Debug bus */
	a5xx_snapshot_debugbus(device, snapshot);
}

static int _a5xx_crashdump_init(struct a5xx_shader_block *block, uint64_t *ptr,
		uint64_t *offset)
{
	int qwords = 0;
	unsigned int j;

	/* Capture each bank in the block */
	for (j = 0; j < A5XX_NUM_SHADER_BANKS; j++) {
		/* Program the aperture */
		ptr[qwords++] =
			(block->statetype << A5XX_SHADER_STATETYPE_SHIFT) | j;
		ptr[qwords++] = (((uint64_t) A5XX_HLSQ_DBG_READ_SEL << 44)) |
			(1 << 21) | 1;

		/* Read all the data in one chunk */
		ptr[qwords++] = registers.gpuaddr + *offset;
		ptr[qwords++] =
			(((uint64_t) A5XX_HLSQ_DBG_AHB_READ_APERTURE << 44)) |
			block->sz;

		/* Remember the offset of the first bank for easy access */
		if (j == 0)
			block->offset = *offset;

		*offset += block->sz * sizeof(unsigned int);
	}

	return qwords;
}

void a5xx_crashdump_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int script_size = 0;
	unsigned int data_size = 0;
	unsigned int i, j;
	uint64_t *ptr;
	uint64_t offset = 0;

	if (capturescript.gpuaddr != 0 && registers.gpuaddr != 0)
		return;

	/*
	 * We need to allocate two buffers:
	 * 1 - the buffer to hold the draw script
	 * 2 - the buffer to hold the data
	 */

	/*
	 * To save the registers, we need 16 bytes per register pair for the
	 * script and a dword for each register int the data
	 */
	for (i = 0; i < ARRAY_SIZE(_a5xx_cd_registers); i++) {
		struct cdregs *regs = &_a5xx_cd_registers[i];

		/* Each pair needs 16 bytes (2 qwords) */
		script_size += (regs->size / 2) * 16;

		/* Each register needs a dword in the data */
		for (j = 0; j < regs->size / 2; j++)
			data_size += REG_PAIR_COUNT(regs->regs, j) *
				sizeof(unsigned int);

	}

	/*
	 * To save the shader blocks for each block in each type we need 32
	 * bytes for the script (16 bytes to program the aperture and 16 to
	 * read the data) and then a block specific number of bytes to hold
	 * the data
	 */
	for (i = 0; i < ARRAY_SIZE(a5xx_shader_blocks); i++) {
		script_size += 32 * A5XX_NUM_SHADER_BANKS;
		data_size += a5xx_shader_blocks[i].sz * sizeof(unsigned int) *
			A5XX_NUM_SHADER_BANKS;
	}

	/* Now allocate the script and data buffers */

	/* The script buffers needs 2 extra qwords on the end */
	if (kgsl_allocate_global(device, &capturescript,
		script_size + 16, KGSL_MEMFLAGS_GPUREADONLY,
		KGSL_MEMDESC_PRIVILEGED))
		return;

	if (kgsl_allocate_global(device, &registers, data_size, 0,
		KGSL_MEMDESC_PRIVILEGED)) {
		kgsl_free_global(KGSL_DEVICE(adreno_dev), &capturescript);
		return;
	}

	/* Build the crash script */

	ptr = (uint64_t *) capturescript.hostptr;

	/* For the registers, program a read command for each pair */
	for (i = 0; i < ARRAY_SIZE(_a5xx_cd_registers); i++) {
		struct cdregs *regs = &_a5xx_cd_registers[i];

		for (j = 0; j < regs->size / 2; j++) {
			unsigned int r = REG_PAIR_COUNT(regs->regs, j);
			*ptr++ = registers.gpuaddr + offset;
			*ptr++ = (((uint64_t) regs->regs[2 * j]) << 44) | r;
			offset += r * sizeof(unsigned int);
		}
	}

	/* Program each shader block */
	for (i = 0; i < ARRAY_SIZE(a5xx_shader_blocks); i++) {
		ptr += _a5xx_crashdump_init(&a5xx_shader_blocks[i], ptr,
			&offset);
	}

	*ptr++ = 0;
	*ptr++ = 0;
}
