/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
	{ A4XX_RBBM_DEBBUS_SP_1_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_SP_2_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_SP_3_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_TPL1_0_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_TPL1_1_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_TPL1_2_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_TPL1_3_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_RB_0_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_RB_1_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_RB_2_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_RB_3_ID, 0x100, },
	{ A4XX_RBBM_DEBBUS_MARB_0_ID, 0x100 },
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
 * @snapshot: Pointer to binary snapshot data blob being made
 * @remain: Number of remaining bytes in the snapshot blob
 * @priv: Unused parameter
 *
 */
static int a4xx_snapshot_shader_memory(struct kgsl_device *device,
	void *snapshot, int remain, void *priv)
{
	struct kgsl_snapshot_debug *header = snapshot;
	unsigned int i, j;
	unsigned int *data = snapshot + sizeof(*header);
	unsigned int shader_read_len = A4XX_SHADER_MEMORY_SIZE;

	if (shader_read_len > (device->shader_mem_len >> 2))
		shader_read_len = (device->shader_mem_len >> 2);

	if (remain < DEBUG_SECTION_SZ(shader_read_len *
				A4XX_NUM_SHADER_BANKS)) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_SHADER_MEMORY;
	header->size = shader_read_len * A4XX_NUM_SHADER_BANKS;

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

	for (j = 0; j < A4XX_NUM_SHADER_BANKS; j++) {
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


	return DEBUG_SECTION_SZ(shader_read_len * A4XX_NUM_SHADER_BANKS);
}

/*
 * a4xx_init_debbus() - Sets up the trace bus to enable debug bus dump
 * @device: POinter to device whose trace bus is set up
 */
void a4xx_init_debbus(struct kgsl_device *device)
{
	unsigned int val = 0;
	/* start trace bus */
	val |= (0x20 <<
		A4XX_RBBM_CFG_DEBBUS_CTLT_ENT_SHIFT);
	val |= (4 << A4XX_RBBM_CFG_DEBBUS_CTLT_GRANU_SHIFT);
	val |= (0xf << A4XX_RBBM_CFG_DEBBUS_CTLT_SEGT_SHIFT);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_CTLT, val);
	val = 0;
	/* start misr */
	val |= 0xf << A4XX_RBBM_CFG_DEBBUS_CTLT_ENABLE_SHIFT;
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_CTLM, val);
	val = 0;
	/* do not invert bits */
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_IVTL_0, 0);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_IVTL_1, 0);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_IVTL_2, 0);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_IVTL_3, 0);
	/* route bits for 1 to 1 mapping */
	val |= (1 << A4XX_RBBM_CFG_DEBBUS_BYTEL_0_BYTEL1_SHIFT);
	val |= (2 << A4XX_RBBM_CFG_DEBBUS_BYTEL_0_BYTEL2_SHIFT);
	val |= (3 << A4XX_RBBM_CFG_DEBBUS_BYTEL_0_BYTEL3_SHIFT);
	val |= (4 << A4XX_RBBM_CFG_DEBBUS_BYTEL_0_BYTEL4_SHIFT);
	val |= (5 << A4XX_RBBM_CFG_DEBBUS_BYTEL_0_BYTEL5_SHIFT);
	val |= (6 << A4XX_RBBM_CFG_DEBBUS_BYTEL_0_BYTEL6_SHIFT);
	val |= (7 << A4XX_RBBM_CFG_DEBBUS_BYTEL_0_BYTEL7_SHIFT);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_BYTEL_0, val);
	val = 0;
	val |= (8 << A4XX_RBBM_CFG_DEBBUS_BYTEL_1_BYTEL8_SHIFT);
	val |= (9 << A4XX_RBBM_CFG_DEBBUS_BYTEL_1_BYTEL9_SHIFT);
	val |= (10 << A4XX_RBBM_CFG_DEBBUS_BYTEL_1_BYTEL10_SHIFT);
	val |= (11 << A4XX_RBBM_CFG_DEBBUS_BYTEL_1_BYTEL11_SHIFT);
	val |= (12 << A4XX_RBBM_CFG_DEBBUS_BYTEL_1_BYTEL12_SHIFT);
	val |= (13 << A4XX_RBBM_CFG_DEBBUS_BYTEL_1_BYTEL13_SHIFT);
	val |= (14 << A4XX_RBBM_CFG_DEBBUS_BYTEL_1_BYTEL14_SHIFT);
	val |= (15 << A4XX_RBBM_CFG_DEBBUS_BYTEL_1_BYTEL15_SHIFT);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_BYTEL_1, val);
	val = 0;
	/* set up to apss through w/o including bits in logic operations */
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_MASKL_0, 0);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_MASKL_1, 0);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_MASKL_2, 0);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_MASKL_3, 0);
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
	reg |= (block_id << A4XX_RBBM_CFG_DEBBUS_SEL_PONG_BLK_SEL_SHIFT);
	reg |= (index << A4XX_RBBM_CFG_DEBBUS_SEL_PING_INDEX_SHIFT);
	reg |= (index << A4XX_RBBM_CFG_DEBBUS_SEL_PONG_INDEX_SHIFT);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_SEL_A, reg);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_SEL_B, reg);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_SEL_C, reg);
	kgsl_regwrite(device, A4XX_RBBM_CFG_DEBBUS_SEL_D, reg);

	kgsl_regread(device, A4XX_RBBM_CFG_DEBBUS_TRACE_BUF1, val);
	val++;
	kgsl_regread(device, A4XX_RBBM_CFG_DEBBUS_TRACE_BUF0, val);
}

/*
 * a4xx_snapshot_debugbus_block() - Capture debug data for a gpu block
 * @device: Pointer to device
 * @snapshot: Memory where data is captured
 * @remain: Number of bytes left in snapshot
 * @priv: Pointer to debug bus block
 *
 * Returns the number of bytes written
 */
static int a4xx_snapshot_debugbus_block(struct kgsl_device *device,
	void *snapshot, int remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header = snapshot;
	struct adreno_debugbus_block *block = priv;
	int i;
	unsigned int *data = snapshot + sizeof(*header);
	unsigned int dwords;
	int size;

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
 * @snapshot: Memory where data is captured
 * @remain: Number of bytes remaining at snapshot memory
 */
static void *a4xx_snapshot_debugbus(struct kgsl_device *device,
	void *snapshot, int *remain)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(a4xx_debugbus_blocks); i++) {
		snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS, snapshot, remain,
			a4xx_snapshot_debugbus_block,
			(void *) &a4xx_debugbus_blocks[i]);
	}

	return snapshot;
}

static void a4xx_reset_hlsq(struct kgsl_device *device)
{
	unsigned long waittime;
	unsigned int val;

	/* reset cp */
	kgsl_regwrite(device, A4XX_RBBM_BLOCK_SW_RESET_CMD, 1 << 20);
	waittime = jiffies + 100;
	while (time_before(jiffies, waittime))
		;

	/* reset hlsq */
	kgsl_regwrite(device, A4XX_RBBM_BLOCK_SW_RESET_CMD, 1 << 25);
	waittime = jiffies + 100;
	while (time_before(jiffies, waittime))
		;

	/* clear reset bits */
	kgsl_regwrite(device, A4XX_RBBM_BLOCK_SW_RESET_CMD, 0);
	waittime = jiffies + 100;
	while (time_before(jiffies, waittime))
		;

	/* set HLSQ_TIMEOUT_THRESHOLD.cycle_timeout_limit_sp to 26 */
	kgsl_regread(device, A4XX_HLSQ_TIMEOUT_THRESHOLD, &val);
	val &= (0x1F << 24);
	val |= (26 << 24);
	kgsl_regwrite(device, A4XX_HLSQ_TIMEOUT_THRESHOLD, val);
}

/*
 * a4xx_snapshot() - A4XX GPU snapshot function
 * @adreno_dev: Device being snapshotted
 * @snapshot: Memory where snapshot is saved
 * @remain: Amount of space left in snapshot memory
 * @hang: If set means snapshot was triggered by a hang
 *
 * This is where all of the A3XX/A4XX specific bits and pieces are grabbed
 * into the snapshot memory
 */
void *a4xx_snapshot(struct adreno_device *adreno_dev, void *snapshot,
	int *remain, int hang)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct kgsl_snapshot_registers_list list;
	struct kgsl_snapshot_registers regs[5];
	struct adreno_snapshot_data *snap_data =
			adreno_dev->gpudev->snapshot_data;
	unsigned int clock_ctl;

	list.registers = regs;
	list.count = 0;

	/* Disable Clock gating temporarily for the debug bus to work */
	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_CLOCK_CTL, &clock_ctl);
	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_CLOCK_CTL, 0x00);

	/* Store relevant registers in list to snapshot */
	_snapshot_a3xx_regs(regs, &list, a4xx_registers,
			a4xx_registers_count, 1);

	_snapshot_a3xx_regs(regs, &list, a4xx_sp_tp_registers,
			a4xx_sp_tp_registers_count, 0);

	/* Turn on MMU clocks since we read MMU registers */
	if (kgsl_mmu_enable_clk(&device->mmu, KGSL_IOMMU_CONTEXT_USER)) {
		KGSL_CORE_ERR("Failed to turn on iommu user context clocks\n");
		goto skip_regs;
	}
	if (kgsl_mmu_enable_clk(&device->mmu, KGSL_IOMMU_CONTEXT_PRIV)) {
		KGSL_CORE_ERR("Failed to turn on iommu priv context clocks\n");
		goto skip_regs;
	}
	/* Master set of (non debug) registers */
	snapshot = kgsl_snapshot_add_section(device,
		KGSL_SNAPSHOT_SECTION_REGS, snapshot, remain,
		kgsl_snapshot_dump_regs, &list);
skip_regs:
	snapshot = kgsl_snapshot_indexed_registers(device, snapshot,
		remain,
		A4XX_CP_STATE_DEBUG_INDEX, A4XX_CP_STATE_DEBUG_DATA,
		0, snap_data->sect_sizes->cp_state_deb);

	 /* CP_ME indexed registers */
	 snapshot = kgsl_snapshot_indexed_registers(device, snapshot,
			remain,
			A4XX_CP_ME_CNTL, A4XX_CP_ME_STATUS,
			64, 44);
	/* VPC memory */
	snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUG, snapshot, remain,
			a3xx_snapshot_vpc_memory,
			&snap_data->sect_sizes->vpc_mem);

	/* CP MEQ */
	snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUG, snapshot, remain,
			a3xx_snapshot_cp_meq,
			&snap_data->sect_sizes->cp_meq);

	/* CP PFP and PM4 */
	if (hang) {
		snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUG, snapshot, remain,
			a3xx_snapshot_cp_pfp_ram, NULL);

		snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUG, snapshot, remain,
			a3xx_snapshot_cp_pm4_ram, NULL);
	}

	/* CP ROQ */
	snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUG, snapshot, remain,
			a3xx_snapshot_cp_roq, &snap_data->sect_sizes->roq);

	snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUG, snapshot, remain,
			a330_snapshot_cp_merciu,
			&snap_data->sect_sizes->cp_merciu);

	/* Debug bus */
	a4xx_init_debbus(device);
	snapshot = a4xx_snapshot_debugbus(device, snapshot, remain);
	/*
	 * TODO - Add call to _adreno_coresight_set to restore
	 * coresight registers when coresight patch is merged
	 */

	a4xx_reset_hlsq(device);

	kgsl_snapshot_dump_skipped_regs(device, &list);
	/* Shader working/shadow memory */
	snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUG, snapshot, remain,
			a4xx_snapshot_shader_memory,
			&snap_data->sect_sizes->shader_mem);

	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_CLOCK_CTL,
			clock_ctl);

	/* This will only disable the clock if no one else turned on */
	kgsl_mmu_disable_clk_on_ts(&device->mmu, 0, 0);
	return snapshot;
}
