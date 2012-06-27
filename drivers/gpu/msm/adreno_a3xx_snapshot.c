/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include "kgsl.h"
#include "adreno.h"
#include "kgsl_snapshot.h"
#include "a3xx_reg.h"

#define DEBUG_SECTION_SZ(_dwords) (((_dwords) * sizeof(unsigned int)) \
		+ sizeof(struct kgsl_snapshot_debug))

#define SHADER_MEMORY_SIZE 0x4000

static int a3xx_snapshot_shader_memory(struct kgsl_device *device,
	void *snapshot, int remain, void *priv)
{
	struct kgsl_snapshot_debug *header = snapshot;
	unsigned int *data = snapshot + sizeof(*header);
	int i;

	if (remain < DEBUG_SECTION_SZ(SHADER_MEMORY_SIZE)) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_SHADER_MEMORY;
	header->size = SHADER_MEMORY_SIZE;

	for (i = 0; i < SHADER_MEMORY_SIZE; i++)
		adreno_regread(device, 0x4000 + i, &data[i]);

	return DEBUG_SECTION_SZ(SHADER_MEMORY_SIZE);
}

#define VPC_MEMORY_BANKS 4
#define VPC_MEMORY_SIZE 512

static int a3xx_snapshot_vpc_memory(struct kgsl_device *device, void *snapshot,
		int remain, void *priv)
{
	struct kgsl_snapshot_debug *header = snapshot;
	unsigned int *data = snapshot + sizeof(*header);
	int size = VPC_MEMORY_BANKS * VPC_MEMORY_SIZE;
	int bank, addr, i = 0;

	if (remain < DEBUG_SECTION_SZ(size)) {
		SNAPSHOT_ERR_NOMEM(device, "VPC MEMORY");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_VPC_MEMORY;
	header->size = size;

	for (bank = 0; bank < VPC_MEMORY_BANKS; bank++) {
		for (addr = 0; addr < VPC_MEMORY_SIZE; addr++) {
			unsigned int val = bank | (addr << 4);
			adreno_regwrite(device,
				A3XX_VPC_VPC_DEBUG_RAM_SEL, val);
			adreno_regread(device,
				A3XX_VPC_VPC_DEBUG_RAM_READ, &data[i++]);
		}
	}

	return DEBUG_SECTION_SZ(size);
}

#define CP_MEQ_SIZE 16
static int a3xx_snapshot_cp_meq(struct kgsl_device *device, void *snapshot,
		int remain, void *priv)
{
	struct kgsl_snapshot_debug *header = snapshot;
	unsigned int *data = snapshot + sizeof(*header);
	int i;

	if (remain < DEBUG_SECTION_SZ(CP_MEQ_SIZE)) {
		SNAPSHOT_ERR_NOMEM(device, "CP MEQ DEBUG");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_CP_MEQ;
	header->size = CP_MEQ_SIZE;

	adreno_regwrite(device, A3XX_CP_MEQ_ADDR, 0x0);
	for (i = 0; i < CP_MEQ_SIZE; i++)
		adreno_regread(device, A3XX_CP_MEQ_DATA, &data[i]);

	return DEBUG_SECTION_SZ(CP_MEQ_SIZE);
}

static int a3xx_snapshot_cp_pm4_ram(struct kgsl_device *device, void *snapshot,
		int remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = snapshot;
	unsigned int *data = snapshot + sizeof(*header);
	int i, size = adreno_dev->pm4_fw_size - 1;

	if (remain < DEBUG_SECTION_SZ(size)) {
		SNAPSHOT_ERR_NOMEM(device, "CP PM4 RAM DEBUG");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_CP_PM4_RAM;
	header->size = size;

	/*
	 * Read the firmware from the GPU rather than use our cache in order to
	 * try to catch mis-programming or corruption in the hardware.  We do
	 * use the cached version of the size, however, instead of trying to
	 * maintain always changing hardcoded constants
	 */

	adreno_regwrite(device, REG_CP_ME_RAM_RADDR, 0x0);
	for (i = 0; i < size; i++)
		adreno_regread(device, REG_CP_ME_RAM_DATA, &data[i]);

	return DEBUG_SECTION_SZ(size);
}

static int a3xx_snapshot_cp_pfp_ram(struct kgsl_device *device, void *snapshot,
		int remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = snapshot;
	unsigned int *data = snapshot + sizeof(*header);
	int i, size = adreno_dev->pfp_fw_size - 1;

	if (remain < DEBUG_SECTION_SZ(size)) {
		SNAPSHOT_ERR_NOMEM(device, "CP PFP RAM DEBUG");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_CP_PFP_RAM;
	header->size = size;

	/*
	 * Read the firmware from the GPU rather than use our cache in order to
	 * try to catch mis-programming or corruption in the hardware.  We do
	 * use the cached version of the size, however, instead of trying to
	 * maintain always changing hardcoded constants
	 */
	kgsl_regwrite(device, A3XX_CP_PFP_UCODE_ADDR, 0x0);
	for (i = 0; i < size; i++)
		adreno_regread(device, A3XX_CP_PFP_UCODE_DATA, &data[i]);

	return DEBUG_SECTION_SZ(size);
}

#define CP_ROQ_SIZE 128

static int a3xx_snapshot_cp_roq(struct kgsl_device *device, void *snapshot,
		int remain, void *priv)
{
	struct kgsl_snapshot_debug *header = snapshot;
	unsigned int *data = snapshot + sizeof(*header);
	int i;

	if (remain < DEBUG_SECTION_SZ(CP_ROQ_SIZE)) {
		SNAPSHOT_ERR_NOMEM(device, "CP ROQ DEBUG");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_CP_ROQ;
	header->size = CP_ROQ_SIZE;

	adreno_regwrite(device, A3XX_CP_ROQ_ADDR, 0x0);
	for (i = 0; i < CP_ROQ_SIZE; i++)
		adreno_regread(device, A3XX_CP_ROQ_DATA, &data[i]);

	return DEBUG_SECTION_SZ(CP_ROQ_SIZE);
}

#define DEBUGFS_BLOCK_SIZE 0x40

static int a3xx_snapshot_debugbus_block(struct kgsl_device *device,
	void *snapshot, int remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header = snapshot;
	unsigned int id = (unsigned int) priv;
	unsigned int val;
	int i;
	unsigned int *data = snapshot + sizeof(*header);
	int size =
		(DEBUGFS_BLOCK_SIZE * sizeof(unsigned int)) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	val = (id << 8) | (1 << 16);

	header->id = id;
	header->count = DEBUGFS_BLOCK_SIZE;

	for (i = 0; i < DEBUGFS_BLOCK_SIZE; i++) {
		adreno_regwrite(device, A3XX_RBBM_DEBUG_BUS_CTL, val | i);
		adreno_regread(device, A3XX_RBBM_DEBUG_BUS_DATA_STATUS,
			&data[i]);
	}

	return size;
}

static unsigned int debugbus_blocks[] = {
	RBBM_BLOCK_ID_CP,
	RBBM_BLOCK_ID_RBBM,
	RBBM_BLOCK_ID_VBIF,
	RBBM_BLOCK_ID_HLSQ,
	RBBM_BLOCK_ID_UCHE,
	RBBM_BLOCK_ID_PC,
	RBBM_BLOCK_ID_VFD,
	RBBM_BLOCK_ID_VPC,
	RBBM_BLOCK_ID_TSE,
	RBBM_BLOCK_ID_RAS,
	RBBM_BLOCK_ID_VSC,
	RBBM_BLOCK_ID_SP_0,
	RBBM_BLOCK_ID_SP_1,
	RBBM_BLOCK_ID_SP_2,
	RBBM_BLOCK_ID_SP_3,
	RBBM_BLOCK_ID_TPL1_0,
	RBBM_BLOCK_ID_TPL1_1,
	RBBM_BLOCK_ID_TPL1_2,
	RBBM_BLOCK_ID_TPL1_3,
	RBBM_BLOCK_ID_RB_0,
	RBBM_BLOCK_ID_RB_1,
	RBBM_BLOCK_ID_RB_2,
	RBBM_BLOCK_ID_RB_3,
	RBBM_BLOCK_ID_MARB_0,
	RBBM_BLOCK_ID_MARB_1,
	RBBM_BLOCK_ID_MARB_2,
	RBBM_BLOCK_ID_MARB_3,
};

static void *a3xx_snapshot_debugbus(struct kgsl_device *device,
	void *snapshot, int *remain)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(debugbus_blocks); i++) {
		snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS, snapshot, remain,
			a3xx_snapshot_debugbus_block,
			(void *) debugbus_blocks[i]);
	}

	return snapshot;
}

/* A3XX GPU snapshot function - this is where all of the A3XX specific
 * bits and pieces are grabbed into the snapshot memory
 */

void *a3xx_snapshot(struct adreno_device *adreno_dev, void *snapshot,
	int *remain, int hang)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct kgsl_snapshot_registers regs;

	regs.regs = (unsigned int *) a3xx_registers;
	regs.count = a3xx_registers_count;

	/* Master set of (non debug) registers */
	snapshot = kgsl_snapshot_add_section(device,
		KGSL_SNAPSHOT_SECTION_REGS, snapshot, remain,
		kgsl_snapshot_dump_regs, &regs);

	/* CP_STATE_DEBUG indexed registers */
	snapshot = kgsl_snapshot_indexed_registers(device, snapshot,
			remain, REG_CP_STATE_DEBUG_INDEX,
			REG_CP_STATE_DEBUG_DATA, 0x0, 0x14);

	/* CP_ME indexed registers */
	snapshot = kgsl_snapshot_indexed_registers(device, snapshot,
			remain, REG_CP_ME_CNTL, REG_CP_ME_STATUS,
			64, 44);

	/* Disable Clock gating temporarily for the debug bus to work */
	adreno_regwrite(device, A3XX_RBBM_CLOCK_CTL, 0x00);

	/* VPC memory */
	snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUG, snapshot, remain,
			a3xx_snapshot_vpc_memory, NULL);

	/* CP MEQ */
	snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUG, snapshot, remain,
			a3xx_snapshot_cp_meq, NULL);

	/* Shader working/shadow memory */
	snapshot = kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUG, snapshot, remain,
			a3xx_snapshot_shader_memory, NULL);


	/* CP PFP and PM4 */
	/* Reading these will hang the GPU if it isn't already hung */

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
			a3xx_snapshot_cp_roq, NULL);

	snapshot = a3xx_snapshot_debugbus(device, snapshot, remain);

	/* Enable Clock gating */
	adreno_regwrite(device, A3XX_RBBM_CLOCK_CTL,
			A3XX_RBBM_CLOCK_CTL_DEFAULT);

	return snapshot;
}
