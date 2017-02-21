/* Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
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

#include "msm_gpu.h"
#include "msm_gem.h"
#include "a5xx_gpu.h"
#include "msm_snapshot_api.h"

#define A5XX_NR_SHADER_BANKS 4

/*
 * These are a list of the registers that need to be read through the HLSQ
 * aperture through the crashdumper.  These are not nominally accessible from
 * the CPU on a secure platform.
 */
static const struct {
	u32 type;
	u32 regoffset;
	u32 count;
} a5xx_hlsq_aperture_regs[] = {
	{ 0x35, 0xE00, 0x32 },   /* HSLQ non-context */
	{ 0x31, 0x2080, 0x1 },   /* HLSQ 2D context 0 */
	{ 0x33, 0x2480, 0x1 },   /* HLSQ 2D context 1 */
	{ 0x32, 0xE780, 0x62 },  /* HLSQ 3D context 0 */
	{ 0x34, 0xEF80, 0x62 },  /* HLSQ 3D context 1 */
	{ 0x3f, 0x0EC0, 0x40 },  /* SP non-context */
	{ 0x3d, 0x2040, 0x1 },   /* SP 2D context 0 */
	{ 0x3b, 0x2440, 0x1 },   /* SP 2D context 1 */
	{ 0x3e, 0xE580, 0x180 }, /* SP 3D context 0 */
	{ 0x3c, 0xED80, 0x180 }, /* SP 3D context 1 */
	{ 0x3a, 0x0F00, 0x1c },  /* TP non-context */
	{ 0x38, 0x2000, 0xa },   /* TP 2D context 0 */
	{ 0x36, 0x2400, 0xa },   /* TP 2D context 1 */
	{ 0x39, 0xE700, 0x80 },  /* TP 3D context 0 */
	{ 0x37, 0xEF00, 0x80 },  /* TP 3D context 1 */
};

/*
 * The debugbus registers contain device state that presumably makes
 * sense to the hardware designers. 'count' is the number of indexes to read,
 * each index value is 64 bits
 */
static const struct {
	enum a5xx_debugbus id;
	u32 count;
} a5xx_debugbus_blocks[] = {
	{  A5XX_RBBM_DBGBUS_CP, 0x100, },
	{  A5XX_RBBM_DBGBUS_RBBM, 0x100, },
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

/*
 * The shader blocks are read from the HLSQ aperture - each one has its own
 * identifier for the aperture read
 */
static const struct {
	enum a5xx_shader_blocks id;
	u32 size;
} a5xx_shader_blocks[] = {
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

/*
 * The A5XX architecture has a a built in engine to asynchronously dump
 * registers from the GPU. It is used to accelerate the copy of hundreds
 * (thousands) of registers and as a safe way to access registers that might
 * have secure data in them (if the GPU is in secure, the crashdumper returns
 * bogus values for those registers). On a fully secured device the CPU will be
 * blocked from accessing those registers directly and so the crashdump is the
 * only way that we can access context registers and the shader banks for debug
 * purposes.
 *
 * The downside of the crashdump is that it requires access to GPU accessible
 * memory (so the VBIF and the bus and the SMMU need to be up and working) and
 * you need enough memory to write the script for the crashdumper and to store
 * the data that you are dumping so there is a balancing act between the work to
 * set up a crash dumper and the value we get out of it.
 */

/*
 * The crashdump uses a pseudo-script format to read and write registers.  Each
 * operation is two 64 bit values.
 *
 * READ:
 *  [qword 0] [64:00] - The absolute IOVA address target for the register value
 *  [qword 1] [63:44] - the dword address of the register offset to read
 *            [15:00] - Number of dwords to read at once
 *
 * WRITE:
 *  [qword 0] [31:0] 32 bit value to write to the register
 *  [qword 1] [63:44] - the dword address of the register offset to write
 *            [21:21] - set 1 to write
 *            [15:00] - Number of dwords to write (usually 1)
 *
 * At the bottom of the script, write quadword zeros to trigger the end.
 */
struct crashdump {
	struct drm_gem_object *bo;
	void *ptr;
	u64 iova;
	u32 index;
};

#define CRASHDUMP_BO_SIZE (SZ_1M)
#define CRASHDUMP_SCRIPT_SIZE (256 * SZ_1K)
#define CRASHDUMP_DATA_SIZE (CRASHDUMP_BO_SIZE - CRASHDUMP_SCRIPT_SIZE)

static int crashdump_init(struct msm_gpu *gpu, struct crashdump *crashdump)
{
	struct drm_device *drm = gpu->dev;
	int ret = -ENOMEM;

	crashdump->bo = msm_gem_new(drm, CRASHDUMP_BO_SIZE, MSM_BO_UNCACHED);
	if (IS_ERR(crashdump->bo)) {
		ret = PTR_ERR(crashdump->bo);
		crashdump->bo = NULL;
		return ret;
	}

	crashdump->ptr = msm_gem_vaddr_locked(crashdump->bo);
	if (!crashdump->ptr)
		goto out;

	ret = msm_gem_get_iova_locked(crashdump->bo, gpu->aspace,
		&crashdump->iova);

out:
	if (ret) {
		drm_gem_object_unreference(crashdump->bo);
		crashdump->bo = NULL;
	}

	return ret;
}

static int crashdump_run(struct msm_gpu *gpu, struct crashdump *crashdump)
{
	if (!crashdump->ptr || !crashdump->index)
		return -EINVAL;

	gpu_write(gpu, REG_A5XX_CP_CRASH_SCRIPT_BASE_LO,
		lower_32_bits(crashdump->iova));
	gpu_write(gpu, REG_A5XX_CP_CRASH_SCRIPT_BASE_HI,
		upper_32_bits(crashdump->iova));

	gpu_write(gpu, REG_A5XX_CP_CRASH_DUMP_CNTL, 1);

	return spin_until(gpu_read(gpu, REG_A5XX_CP_CRASH_DUMP_CNTL) & 0x04);
}

static void crashdump_destroy(struct msm_gpu *gpu, struct crashdump *crashdump)
{
	if (!crashdump->bo)
		return;

	if (crashdump->iova)
		msm_gem_put_iova(crashdump->bo, gpu->aspace);

	drm_gem_object_unreference(crashdump->bo);

	memset(crashdump, 0, sizeof(*crashdump));
}

static inline void CRASHDUMP_SCRIPT_WRITE(struct crashdump *crashdump,
		u32 reg, u32 val)
{
	u64 *ptr = crashdump->ptr + crashdump->index;

	if (WARN_ON(crashdump->index + (2 * sizeof(u64))
		>= CRASHDUMP_SCRIPT_SIZE))
		return;

	/* This is the value to write */
	ptr[0] = (u64) val;

	/*
	 * This triggers a write to the specified register.  1 is the size of
	 * the write in dwords
	 */
	ptr[1] = (((u64) reg) << 44) | (1 << 21) | 1;

	crashdump->index += 2 * sizeof(u64);
}

static inline void CRASHDUMP_SCRIPT_READ(struct crashdump *crashdump,
		u32 reg, u32 count, u32 offset)
{
	u64 *ptr = crashdump->ptr + crashdump->index;

	if (WARN_ON(crashdump->index + (2 * sizeof(u64))
		>= CRASHDUMP_SCRIPT_SIZE))
		return;

	if (WARN_ON(offset + (count * sizeof(u32)) >= CRASHDUMP_DATA_SIZE))
		return;

	ptr[0] = (u64) crashdump->iova + CRASHDUMP_SCRIPT_SIZE + offset;
	ptr[1] = (((u64) reg) << 44) | count;

	crashdump->index += 2 * sizeof(u64);
}

static inline void *CRASHDUMP_DATA_PTR(struct crashdump *crashdump, u32 offset)
{
	if (WARN_ON(!crashdump->ptr || offset >= CRASHDUMP_DATA_SIZE))
		return NULL;

	return crashdump->ptr + CRASHDUMP_SCRIPT_SIZE + offset;
}

static inline u32 CRASHDUMP_DATA_READ(struct crashdump *crashdump, u32 offset)
{
	return *((u32 *) CRASHDUMP_DATA_PTR(crashdump, offset));
}

static inline void CRASHDUMP_RESET(struct crashdump *crashdump)
{
	crashdump->index = 0;
}

static inline void CRASHDUMP_END(struct crashdump *crashdump)
{
	u64 *ptr = crashdump->ptr + crashdump->index;

	if (WARN_ON((crashdump->index + (2 * sizeof(u64)))
		>= CRASHDUMP_SCRIPT_SIZE))
		return;

	ptr[0] = 0;
	ptr[1] = 0;

	crashdump->index += 2 * sizeof(u64);
}

static u32 _crashdump_read_hlsq_aperture(struct crashdump *crashdump,
		u32 offset, u32 statetype, u32 bank,
		u32 count)
{
	CRASHDUMP_SCRIPT_WRITE(crashdump, REG_A5XX_HLSQ_DBG_READ_SEL,
		A5XX_HLSQ_DBG_READ_SEL_STATETYPE(statetype) | bank);

	CRASHDUMP_SCRIPT_READ(crashdump, REG_A5XX_HLSQ_DBG_AHB_READ_APERTURE,
		count, offset);

	return count * sizeof(u32);
}

static u32 _copy_registers(struct msm_snapshot *snapshot,
		struct crashdump *crashdump, u32 reg, u32 count,
		u32 offset)
{
	int i;
	u32 *ptr = (u32 *) (crashdump->ptr + CRASHDUMP_SCRIPT_SIZE + offset);
	/*
	 * Write the offset of the first register of the group and the number of
	 * registers in the group
	 */
	SNAPSHOT_WRITE_U32(snapshot, ((count << 16) | reg));

	/* Followed by each register value in the group */
	for (i = 0; i < count; i++)
		SNAPSHOT_WRITE_U32(snapshot, ptr[i]);

	return count * sizeof(u32);
}

/*
 * Return the number of registers in each register group from the
 * adreno_gpu->rgisters
 */
static inline u32 REG_COUNT(const unsigned int *ptr)
{
	return (ptr[1] - ptr[0]) + 1;
}

/*
 * Capture what registers we can from the CPU in case the crashdumper is
 * unavailable or broken.  This will omit the SP,TP and HLSQ registers, but
 * you'll get everything else and that ain't bad
 */
static void a5xx_snapshot_registers_cpu(struct msm_gpu *gpu,
		struct msm_snapshot *snapshot)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct msm_snapshot_regs header;
	u32 regcount = 0, groups = 0;
	int i;

	/*
	 * Before we write the section we need to figure out how big our data
	 * section will be
	 */
	for (i = 0; adreno_gpu->registers[i] != ~0; i += 2) {
		regcount += REG_COUNT(&(adreno_gpu->registers[i]));
		groups++;
	}

	header.count = groups;

	/*
	 * We need one dword for each group and then one dword for each register
	 * value in that group
	 */
	if (!SNAPSHOT_HEADER(snapshot, header, SNAPSHOT_SECTION_REGS_V2,
		regcount + groups))
		return;

	for (i = 0; adreno_gpu->registers[i] != ~0; i += 2) {
		u32 count = REG_COUNT(&(adreno_gpu->registers[i]));
		u32 reg = adreno_gpu->registers[i];
		int j;

		/* Write the offset and count for the group */
		SNAPSHOT_WRITE_U32(snapshot, (count << 16) | reg);

		/* Write each value in the group */
		for (j = 0; j < count; j++)
			SNAPSHOT_WRITE_U32(snapshot, gpu_read(gpu, reg++));
	}
}

static void a5xx_snapshot_registers(struct msm_gpu *gpu,
		struct msm_snapshot *snapshot)
{
	struct msm_snapshot_regs header;
	struct crashdump *crashdump = snapshot->priv;
	u32 offset = 0, regcount = 0, groups = 0;
	int i;

	/*
	 * First snapshot all the registers that we can from the CPU.  Do this
	 * because the crashdumper has a tendency to "taint" the value of some
	 * of the registers (because the GPU implements the crashdumper) so we
	 * only want to use the crash dump facility if we have to
	 */
	a5xx_snapshot_registers_cpu(gpu, snapshot);

	if (!crashdump)
		return;

	CRASHDUMP_RESET(crashdump);

	/* HLSQ and context registers behind the aperture */
	for (i = 0; i < ARRAY_SIZE(a5xx_hlsq_aperture_regs); i++) {
		u32 count = a5xx_hlsq_aperture_regs[i].count;

		offset += _crashdump_read_hlsq_aperture(crashdump, offset,
			a5xx_hlsq_aperture_regs[i].type, 0, count);
		regcount += count;

		groups++;
	}

	CRASHDUMP_END(crashdump);

	if (crashdump_run(gpu, crashdump))
		return;

	header.count = groups;

	/*
	 * The size of the data will be one dword for each "group" of registers,
	 * and then one dword for each of the registers in that group
	 */
	if (!SNAPSHOT_HEADER(snapshot, header, SNAPSHOT_SECTION_REGS_V2,
		groups + regcount))
		return;

	/* Copy the registers to the snapshot */
	for (i = 0; i < ARRAY_SIZE(a5xx_hlsq_aperture_regs); i++)
		offset += _copy_registers(snapshot, crashdump,
			a5xx_hlsq_aperture_regs[i].regoffset,
			a5xx_hlsq_aperture_regs[i].count, offset);
}

static void _a5xx_snapshot_shader_bank(struct msm_snapshot *snapshot,
		struct crashdump *crashdump, u32 block, u32 bank,
		u32 size, u32 offset)
{
	void *src;

	struct msm_snapshot_shader header = {
		.type = block,
		.index = bank,
		.size = size,
	};

	if (!SNAPSHOT_HEADER(snapshot, header, SNAPSHOT_SECTION_SHADER, size))
		return;

	src = CRASHDUMP_DATA_PTR(crashdump, offset);

	if (src)
		SNAPSHOT_MEMCPY(snapshot, src, size * sizeof(u32));
}

static void a5xx_snapshot_shader_memory(struct msm_gpu *gpu,
		struct msm_snapshot *snapshot)
{
	struct crashdump *crashdump = snapshot->priv;
	u32 offset = 0;
	int i;

	/* We can only get shader memory through the crashdump */
	if (!crashdump)
		return;

	CRASHDUMP_RESET(crashdump);

	/* For each shader block */
	for (i = 0; i < ARRAY_SIZE(a5xx_shader_blocks); i++) {
		int j;

		/* For each block, dump 4 banks */
		for (j = 0; j < A5XX_NR_SHADER_BANKS; j++)
			offset += _crashdump_read_hlsq_aperture(crashdump,
				offset, a5xx_shader_blocks[i].id, j,
				a5xx_shader_blocks[i].size);
	}

	CRASHDUMP_END(crashdump);

	/* If the crashdump fails we can't get shader memory any other way */
	if (crashdump_run(gpu, crashdump))
		return;

	/* Each bank of each shader gets its own snapshot section */
	for (offset = 0, i = 0; i < ARRAY_SIZE(a5xx_shader_blocks); i++) {
		int j;

		for (j = 0; j < A5XX_NR_SHADER_BANKS; j++) {
			_a5xx_snapshot_shader_bank(snapshot, crashdump,
				a5xx_shader_blocks[i].id, j,
				a5xx_shader_blocks[i].size, offset);
			offset += a5xx_shader_blocks[i].size * sizeof(u32);
		}
	}
}

#define A5XX_NUM_AXI_ARB_BLOCKS 2
#define A5XX_NUM_XIN_BLOCKS     4
#define VBIF_DATA_SIZE ((16 * A5XX_NUM_AXI_ARB_BLOCKS) + \
	(18 * A5XX_NUM_XIN_BLOCKS) + (12 * A5XX_NUM_XIN_BLOCKS))

static void a5xx_snapshot_debugbus_vbif(struct msm_gpu *gpu,
		struct msm_snapshot *snapshot)
{
	int i;
	struct msm_snapshot_debugbus header = {
		.id = A5XX_RBBM_DBGBUS_VBIF,
		.count = VBIF_DATA_SIZE,
	};

	if (!SNAPSHOT_HEADER(snapshot, header, SNAPSHOT_SECTION_DEBUGBUS,
		VBIF_DATA_SIZE))
		return;

	gpu_rmw(gpu, REG_A5XX_VBIF_CLKON, A5XX_VBIF_CLKON_FORCE_ON_TESTBUS,
		A5XX_VBIF_CLKON_FORCE_ON_TESTBUS);

	gpu_write(gpu, REG_A5XX_VBIF_TEST_BUS1_CTRL0, 0);
	gpu_write(gpu, REG_A5XX_VBIF_TEST_BUS_OUT_CTRL,
		A5XX_VBIF_TEST_BUS_OUT_CTRL_TEST_BUS_CTRL_EN);

	for (i = 0; i < A5XX_NUM_AXI_ARB_BLOCKS; i++) {
		int j;

		gpu_write(gpu, REG_A5XX_VBIF_TEST_BUS2_CTRL0, 1 << (i + 16));
		for (j = 0; j < 16; j++) {
			gpu_write(gpu, REG_A5XX_VBIF_TEST_BUS2_CTRL1,
			A5XX_VBIF_TEST_BUS2_CTRL1_TEST_BUS2_DATA_SEL(j));
			SNAPSHOT_WRITE_U32(snapshot, gpu_read(gpu,
				REG_A5XX_VBIF_TEST_BUS_OUT));
		}
	}

	for (i = 0; i < A5XX_NUM_XIN_BLOCKS; i++) {
		int j;

		gpu_write(gpu, REG_A5XX_VBIF_TEST_BUS2_CTRL0, 1 << i);
		for (j = 0; j < 18; j++) {
			gpu_write(gpu, REG_A5XX_VBIF_TEST_BUS2_CTRL1,
			A5XX_VBIF_TEST_BUS2_CTRL1_TEST_BUS2_DATA_SEL(j));
			SNAPSHOT_WRITE_U32(snapshot,
				gpu_read(gpu, REG_A5XX_VBIF_TEST_BUS_OUT));
		}
	}

	for (i = 0; i < A5XX_NUM_XIN_BLOCKS; i++) {
		int j;

		gpu_write(gpu, REG_A5XX_VBIF_TEST_BUS1_CTRL0, 1 << i);
		for (j = 0; j < 12; j++) {
			gpu_write(gpu, REG_A5XX_VBIF_TEST_BUS1_CTRL1,
			A5XX_VBIF_TEST_BUS1_CTRL1_TEST_BUS1_DATA_SEL(j));
			SNAPSHOT_WRITE_U32(snapshot, gpu_read(gpu,
				REG_A5XX_VBIF_TEST_BUS_OUT));
		}
	}

}

static void a5xx_snapshot_debugbus_block(struct msm_gpu *gpu,
		struct msm_snapshot *snapshot, u32 block, u32 count)
{
	int i;
	struct msm_snapshot_debugbus header = {
		.id = block,
		.count = count * 2, /* Each value is 2 dwords */
	};

	if (!SNAPSHOT_HEADER(snapshot, header, SNAPSHOT_SECTION_DEBUGBUS,
		(count * 2)))
		return;

	for (i = 0; i < count; i++) {
		u32 reg = A5XX_RBBM_CFG_DBGBUS_SEL_A_PING_INDEX(i) |
			A5XX_RBBM_CFG_DBGBUS_SEL_A_PING_BLK_SEL(block);

		gpu_write(gpu, REG_A5XX_RBBM_CFG_DBGBUS_SEL_A, reg);
		gpu_write(gpu, REG_A5XX_RBBM_CFG_DBGBUS_SEL_B, reg);
		gpu_write(gpu, REG_A5XX_RBBM_CFG_DBGBUS_SEL_C, reg);
		gpu_write(gpu, REG_A5XX_RBBM_CFG_DBGBUS_SEL_D, reg);

		/* Each debugbus entry is a quad word */
		SNAPSHOT_WRITE_U32(snapshot, gpu_read(gpu,
			REG_A5XX_RBBM_CFG_DBGBUS_TRACE_BUF2));
		SNAPSHOT_WRITE_U32(snapshot,
			gpu_read(gpu, REG_A5XX_RBBM_CFG_DBGBUS_TRACE_BUF1));
	}
}

static void a5xx_snapshot_debugbus(struct msm_gpu *gpu,
		struct msm_snapshot *snapshot)
{
	int i;

	gpu_write(gpu, REG_A5XX_RBBM_CFG_DBGBUS_CNTLM,
		A5XX_RBBM_CFG_DBGBUS_CNTLM_ENABLE(0xF));

	for (i = 0; i < ARRAY_SIZE(a5xx_debugbus_blocks); i++)
		a5xx_snapshot_debugbus_block(gpu, snapshot,
			a5xx_debugbus_blocks[i].id,
			a5xx_debugbus_blocks[i].count);

	/* VBIF is special and not in a good way */
	a5xx_snapshot_debugbus_vbif(gpu, snapshot);
}

static void a5xx_snapshot_cp_merciu(struct msm_gpu *gpu,
		struct msm_snapshot *snapshot)
{
	unsigned int i;
	struct msm_snapshot_debug header = {
		.type = SNAPSHOT_DEBUG_CP_MERCIU,
		.size = 64 << 1, /* Data size is 2 dwords per entry */
	};

	if (!SNAPSHOT_HEADER(snapshot, header, SNAPSHOT_SECTION_DEBUG, 64 << 1))
		return;

	gpu_write(gpu, REG_A5XX_CP_MERCIU_DBG_ADDR, 0);
	for (i = 0; i < 64; i++) {
		SNAPSHOT_WRITE_U32(snapshot,
			gpu_read(gpu, REG_A5XX_CP_MERCIU_DBG_DATA_1));
		SNAPSHOT_WRITE_U32(snapshot,
			gpu_read(gpu, REG_A5XX_CP_MERCIU_DBG_DATA_2));
	}
}

static void a5xx_snapshot_cp_roq(struct msm_gpu *gpu,
		struct msm_snapshot *snapshot)
{
	int i;
	struct msm_snapshot_debug header = {
		.type = SNAPSHOT_DEBUG_CP_ROQ,
		.size = 512,
	};

	if (!SNAPSHOT_HEADER(snapshot, header, SNAPSHOT_SECTION_DEBUG, 512))
		return;

	gpu_write(gpu, REG_A5XX_CP_ROQ_DBG_ADDR, 0);
	for (i = 0; i < 512; i++)
		SNAPSHOT_WRITE_U32(snapshot,
			gpu_read(gpu, REG_A5XX_CP_ROQ_DBG_DATA));
}

static void a5xx_snapshot_cp_meq(struct msm_gpu *gpu,
		struct msm_snapshot *snapshot)
{
	int i;
	struct msm_snapshot_debug header = {
		.type = SNAPSHOT_DEBUG_CP_MEQ,
		.size = 64,
	};

	if (!SNAPSHOT_HEADER(snapshot, header, SNAPSHOT_SECTION_DEBUG, 64))
		return;

	gpu_write(gpu, REG_A5XX_CP_MEQ_DBG_ADDR, 0);
	for (i = 0; i < 64; i++)
		SNAPSHOT_WRITE_U32(snapshot,
			gpu_read(gpu, REG_A5XX_CP_MEQ_DBG_DATA));
}

static void a5xx_snapshot_indexed_registers(struct msm_gpu *gpu,
		struct msm_snapshot *snapshot, u32 addr, u32 data,
		u32 count)
{
	unsigned int i;
	struct msm_snapshot_indexed_regs header = {
		.index_reg = addr,
		.data_reg = data,
		.start = 0,
		.count = count,
	};

	if (!SNAPSHOT_HEADER(snapshot, header, SNAPSHOT_SECTION_INDEXED_REGS,
		count))
		return;

	for (i = 0; i < count; i++) {
		gpu_write(gpu, addr, i);
		SNAPSHOT_WRITE_U32(snapshot, gpu_read(gpu, data));
	}
}

int a5xx_snapshot(struct msm_gpu *gpu, struct msm_snapshot *snapshot)
{
	struct crashdump crashdump = { 0 };

	if (!crashdump_init(gpu, &crashdump))
		snapshot->priv = &crashdump;

	/* To accurately read all registers, disable hardware clock gating */
	a5xx_set_hwcg(gpu, false);

	/* Kick it up to the generic level */
	adreno_snapshot(gpu, snapshot);

	/* Read the GPU registers */
	a5xx_snapshot_registers(gpu, snapshot);

	/* Read the shader memory banks */
	a5xx_snapshot_shader_memory(gpu, snapshot);

	/* Read the debugbus registers */
	a5xx_snapshot_debugbus(gpu, snapshot);

	/* PFP data */
	a5xx_snapshot_indexed_registers(gpu, snapshot,
		REG_A5XX_CP_PFP_STAT_ADDR, REG_A5XX_CP_PFP_STAT_DATA, 36);

	/* ME data */
	a5xx_snapshot_indexed_registers(gpu, snapshot,
		REG_A5XX_CP_ME_STAT_ADDR, REG_A5XX_CP_ME_STAT_DATA, 29);

	/* DRAW_STATE data */
	a5xx_snapshot_indexed_registers(gpu, snapshot,
		REG_A5XX_CP_DRAW_STATE_ADDR, REG_A5XX_CP_DRAW_STATE_DATA,
		256);

	/* ME cache */
	a5xx_snapshot_indexed_registers(gpu, snapshot,
		REG_A5XX_CP_ME_UCODE_DBG_ADDR, REG_A5XX_CP_ME_UCODE_DBG_DATA,
		0x53F);

	/* PFP cache */
	a5xx_snapshot_indexed_registers(gpu, snapshot,
		REG_A5XX_CP_PFP_UCODE_DBG_ADDR, REG_A5XX_CP_PFP_UCODE_DBG_DATA,
		0x53F);

	/* ME queue */
	a5xx_snapshot_cp_meq(gpu, snapshot);

	/* CP ROQ */
	a5xx_snapshot_cp_roq(gpu, snapshot);

	/* CP MERCIU */
	a5xx_snapshot_cp_merciu(gpu, snapshot);

	crashdump_destroy(gpu, &crashdump);
	snapshot->priv = NULL;

	/* Re-enable HWCG */
	a5xx_set_hwcg(gpu, true);
	return 0;
}
