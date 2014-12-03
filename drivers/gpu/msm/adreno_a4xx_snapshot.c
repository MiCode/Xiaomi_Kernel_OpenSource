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

#include <linux/io.h>
#include "kgsl.h"
#include "adreno.h"
#include "kgsl_snapshot.h"
#include "a4xx_reg.h"
#include "adreno_a3xx_snapshot.h"

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
void a4xx_rbbm_debug_bus_read(struct kgsl_device *device,
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

static void a4xx_snapshot_vbif_registers(struct kgsl_device *device,
				struct kgsl_snapshot_registers *regs,
				struct kgsl_snapshot_registers_list *list)
{
	unsigned int vbif_version = 0;
	int i;
	int found = 0;

	kgsl_regread(device, A4XX_VBIF_VERSION, &vbif_version);

	for (i = 0; i < a4xx_vbif_snapshot_reg_cnt; i++) {
		if (vbif_version ==
			a4xx_vbif_snapshot_registers[i].vbif_version) {
			found = 1;
			break;
		}
	}
	if (found)
		_snapshot_a3xx_regs(regs, list,
				a4xx_vbif_snapshot_registers[i].
					vbif_snapshot_registers,
				a4xx_vbif_snapshot_registers[i].
					vbif_snapshot_registers_count, 1);
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
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_snapshot_registers_list list;
	struct kgsl_snapshot_registers regs[5];
	struct adreno_snapshot_data *snap_data = gpudev->snapshot_data;

	list.registers = regs;
	list.count = 0;

	/* Disable SP clock gating for the debug bus to work on A430v2/A418 */
	if (adreno_is_a430v2(adreno_dev) || adreno_is_a418(adreno_dev)) {
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_SP0, 0);
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_SP1, 0);
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_SP2, 0);
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL_SP3, 0);
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_SP0, 0);
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_SP1, 0);
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_SP2, 0);
		kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2_SP3, 0);
	}
	/* Disable top level clock gating the debug bus to work */
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL, 0);
	kgsl_regwrite(device, A4XX_RBBM_CLOCK_CTL2, 0);

	/* Store relevant registers in list to snapshot */
	_snapshot_a3xx_regs(regs, &list, a4xx_registers,
			a4xx_registers_count, 1);

	_snapshot_a3xx_regs(regs, &list, a4xx_sp_tp_registers,
			a4xx_sp_tp_registers_count,
			(adreno_is_a430(adreno_dev) ? 1 : 0));

	if (adreno_is_a420(adreno_dev)) {
		_snapshot_a3xx_regs(regs, &list, a4xx_xpu_registers,
				a4xx_xpu_reg_cnt, 1);
	}

	if (adreno_is_a430v2(adreno_dev)) {
		_snapshot_a3xx_regs(regs, &list, a4xx_ppd_registers,
				a4xx_ppd_registers_count, 1);
	}

	a4xx_snapshot_vbif_registers(device, regs, &list);

	/* Turn on MMU clocks since we read MMU registers */
	kgsl_mmu_enable_clk(&device->mmu, KGSL_IOMMU_MAX_UNITS);

	/* Master set of (non debug) registers */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS, snapshot,
		kgsl_snapshot_dump_regs, &list);

	kgsl_mmu_disable_clk(&device->mmu, KGSL_IOMMU_MAX_UNITS);

	kgsl_snapshot_indexed_registers(device, snapshot,
		A4XX_CP_STATE_DEBUG_INDEX, A4XX_CP_STATE_DEBUG_DATA,
		0, snap_data->sect_sizes->cp_state_deb);

	 /* CP_ME indexed registers */
	 kgsl_snapshot_indexed_registers(device, snapshot,
		A4XX_CP_ME_CNTL, A4XX_CP_ME_STATUS, 64, 44);

	/* VPC memory */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a3xx_snapshot_vpc_memory,
		&snap_data->sect_sizes->vpc_mem);

	/* CP MEQ */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a3xx_snapshot_cp_meq,
		&snap_data->sect_sizes->cp_meq);

	/* CP PFP and PM4 */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a3xx_snapshot_cp_pfp_ram, NULL);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a3xx_snapshot_cp_pm4_ram, NULL);

	/* CP ROQ */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a3xx_snapshot_cp_roq,
		&snap_data->sect_sizes->roq);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a330_snapshot_cp_merciu,
		&snap_data->sect_sizes->cp_merciu);

	/* Debug bus */
	a4xx_snapshot_debugbus(device, snapshot);

	if (!adreno_is_a430(adreno_dev)) {
		a4xx_reset_hlsq(device);

		kgsl_snapshot_dump_skipped_regs(device, &list);
	}
	/* Shader working/shadow memory */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a4xx_snapshot_shader_memory,
		&snap_data->sect_sizes->shader_mem);
}
