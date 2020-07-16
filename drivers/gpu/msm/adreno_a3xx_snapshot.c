// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2017,2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/io.h>

#include "adreno.h"
#include "adreno_a3xx.h"
#include "adreno_snapshot.h"
#include "kgsl_device.h"

/*
 * Set of registers to dump for A3XX on snapshot.
 * Registers in pairs - first value is the start offset, second
 * is the stop offset (inclusive)
 */

static const unsigned int a3xx_registers[] = {
	0x0000, 0x0002, 0x0010, 0x0012, 0x0018, 0x0018, 0x0020, 0x0027,
	0x0029, 0x002b, 0x002e, 0x0033, 0x0040, 0x0042, 0x0050, 0x005c,
	0x0060, 0x006c, 0x0080, 0x0082, 0x0084, 0x0088, 0x0090, 0x00e5,
	0x00ea, 0x00ed, 0x0100, 0x0100, 0x0110, 0x0123, 0x01c0, 0x01c1,
	0x01c3, 0x01c5, 0x01c7, 0x01c7, 0x01d5, 0x01d9, 0x01dc, 0x01dd,
	0x01ea, 0x01ea, 0x01ee, 0x01f1, 0x01f5, 0x01f6, 0x01f8, 0x01f9,
	0x01fc, 0x01ff,
	0x0440, 0x0440, 0x0443, 0x0443, 0x0445, 0x0445, 0x044d, 0x044f,
	0x0452, 0x0452, 0x0454, 0x046f, 0x047c, 0x047c, 0x047f, 0x047f,
	0x0578, 0x057f, 0x0600, 0x0602, 0x0605, 0x0607, 0x060a, 0x060e,
	0x0612, 0x0614, 0x0c01, 0x0c02, 0x0c06, 0x0c1d, 0x0c3d, 0x0c3f,
	0x0c48, 0x0c4b, 0x0c80, 0x0c80, 0x0c88, 0x0c8b, 0x0ca0, 0x0cb7,
	0x0cc0, 0x0cc1, 0x0cc6, 0x0cc7, 0x0ce4, 0x0ce5,
	0x0e41, 0x0e45, 0x0e64, 0x0e65,
	0x0e80, 0x0e82, 0x0e84, 0x0e89, 0x0ea0, 0x0ea1, 0x0ea4, 0x0ea7,
	0x0ec4, 0x0ecb, 0x0ee0, 0x0ee0, 0x0f00, 0x0f01, 0x0f03, 0x0f09,
	0x2040, 0x2040, 0x2044, 0x2044, 0x2048, 0x204d, 0x2068, 0x2069,
	0x206c, 0x206d, 0x2070, 0x2070, 0x2072, 0x2072, 0x2074, 0x2075,
	0x2079, 0x207a, 0x20c0, 0x20d3, 0x20e4, 0x20ef, 0x2100, 0x2109,
	0x210c, 0x210c, 0x210e, 0x210e, 0x2110, 0x2111, 0x2114, 0x2115,
	0x21e4, 0x21e4, 0x21ea, 0x21ea, 0x21ec, 0x21ed, 0x21f0, 0x21f0,
	0x2240, 0x227e,
	0x2280, 0x228b, 0x22c0, 0x22c0, 0x22c4, 0x22ce, 0x22d0, 0x22d8,
	0x22df, 0x22e6, 0x22e8, 0x22e9, 0x22ec, 0x22ec, 0x22f0, 0x22f7,
	0x22ff, 0x22ff, 0x2340, 0x2343,
	0x2440, 0x2440, 0x2444, 0x2444, 0x2448, 0x244d,
	0x2468, 0x2469, 0x246c, 0x246d, 0x2470, 0x2470, 0x2472, 0x2472,
	0x2474, 0x2475, 0x2479, 0x247a, 0x24c0, 0x24d3, 0x24e4, 0x24ef,
	0x2500, 0x2509, 0x250c, 0x250c, 0x250e, 0x250e, 0x2510, 0x2511,
	0x2514, 0x2515, 0x25e4, 0x25e4, 0x25ea, 0x25ea, 0x25ec, 0x25ed,
	0x25f0, 0x25f0,
	0x2640, 0x267e, 0x2680, 0x268b, 0x26c0, 0x26c0, 0x26c4, 0x26ce,
	0x26d0, 0x26d8, 0x26df, 0x26e6, 0x26e8, 0x26e9, 0x26ec, 0x26ec,
	0x26f0, 0x26f7, 0x26ff, 0x26ff, 0x2740, 0x2743,
	0x300C, 0x300E, 0x301C, 0x301D,
	0x302A, 0x302A, 0x302C, 0x302D, 0x3030, 0x3031, 0x3034, 0x3036,
	0x303C, 0x303C, 0x305E, 0x305F,
};

/* Removed the following HLSQ register ranges from being read during
 * fault tolerance since reading the registers may cause the device to hang:
 */
static const unsigned int a3xx_hlsq_registers[] = {
	0x0e00, 0x0e05, 0x0e0c, 0x0e0c, 0x0e22, 0x0e23,
	0x2200, 0x2212, 0x2214, 0x2217, 0x221a, 0x221a,
	0x2600, 0x2612, 0x2614, 0x2617, 0x261a, 0x261a,
};

/* The set of additional registers to be dumped for A330 */

static const unsigned int a330_registers[] = {
	0x1d0, 0x1d0, 0x1d4, 0x1d4, 0x453, 0x453,
};

/* Shader memory size in words */
#define SHADER_MEMORY_SIZE 0x4000

/**
 * _rbbm_debug_bus_read - Helper function to read data from the RBBM
 * debug bus.
 * @device - GPU device to read/write registers
 * @block_id - Debug bus block to read from
 * @index - Index in the debug bus block to read
 * @ret - Value of the register read
 */
static void _rbbm_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int block = (block_id << 8) | 1 << 16;

	kgsl_regwrite(device, A3XX_RBBM_DEBUG_BUS_CTL, block | index);
	kgsl_regread(device, A3XX_RBBM_DEBUG_BUS_DATA_STATUS, val);
}

/**
 * a3xx_snapshot_shader_memory - Helper function to dump the GPU shader
 * memory to the snapshot buffer.
 * @device: GPU device whose shader memory is to be dumped
 * @buf: Pointer to binary snapshot data blob being made
 * @remain: Number of remaining bytes in the snapshot blob
 * @priv: Unused parameter
 *
 */
static size_t a3xx_snapshot_shader_memory(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	void *data = buf + sizeof(*header);
	unsigned int shader_read_len = SHADER_MEMORY_SIZE;

	if (remain < DEBUG_SECTION_SZ(shader_read_len)) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_SHADER_MEMORY;
	header->size = shader_read_len;

	/* Map shader memory to kernel, for dumping */
	if (IS_ERR_OR_NULL(device->shader_mem_virt)) {
		struct resource *res;

		res = platform_get_resource_byname(device->pdev,
			IORESOURCE_MEM, "kgsl_3d0_shader_memory");

		if (res)
			device->shader_mem_virt =
				devm_ioremap_resource(&device->pdev->dev, res);
	}

	if (IS_ERR_OR_NULL(device->shader_mem_virt)) {
		dev_err(device->dev, "Unable to map the shader memory\n");
		return 0;
	}

	memcpy_fromio(data, device->shader_mem_virt, shader_read_len << 2);

	return DEBUG_SECTION_SZ(shader_read_len);
}

static size_t a3xx_snapshot_debugbus_block(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header
		= (struct kgsl_snapshot_debugbus *)buf;
	struct adreno_debugbus_block *block = priv;
	int i;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	size_t size;

	size = (0x40 * sizeof(unsigned int)) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = block->block_id;
	header->count = 0x40;

	for (i = 0; i < 0x40; i++)
		_rbbm_debug_bus_read(device, block->block_id, i, &data[i]);

	return size;
}

static struct adreno_debugbus_block debugbus_blocks[] = {
	{ RBBM_BLOCK_ID_CP, 0x52, },
	{ RBBM_BLOCK_ID_RBBM, 0x40, },
	{ RBBM_BLOCK_ID_VBIF, 0x40, },
	{ RBBM_BLOCK_ID_HLSQ, 0x40, },
	{ RBBM_BLOCK_ID_UCHE, 0x40, },
	{ RBBM_BLOCK_ID_PC, 0x40, },
	{ RBBM_BLOCK_ID_VFD, 0x40, },
	{ RBBM_BLOCK_ID_VPC, 0x40, },
	{ RBBM_BLOCK_ID_TSE, 0x40, },
	{ RBBM_BLOCK_ID_RAS, 0x40, },
	{ RBBM_BLOCK_ID_VSC, 0x40, },
	{ RBBM_BLOCK_ID_SP_0, 0x40, },
	{ RBBM_BLOCK_ID_SP_1, 0x40, },
	{ RBBM_BLOCK_ID_SP_2, 0x40, },
	{ RBBM_BLOCK_ID_SP_3, 0x40, },
	{ RBBM_BLOCK_ID_TPL1_0, 0x40, },
	{ RBBM_BLOCK_ID_TPL1_1, 0x40, },
	{ RBBM_BLOCK_ID_TPL1_2, 0x40, },
	{ RBBM_BLOCK_ID_TPL1_3, 0x40, },
	{ RBBM_BLOCK_ID_RB_0, 0x40, },
	{ RBBM_BLOCK_ID_RB_1, 0x40, },
	{ RBBM_BLOCK_ID_RB_2, 0x40, },
	{ RBBM_BLOCK_ID_RB_3, 0x40, },
	{ RBBM_BLOCK_ID_MARB_0, 0x40, },
	{ RBBM_BLOCK_ID_MARB_1, 0x40, },
	{ RBBM_BLOCK_ID_MARB_2, 0x40, },
	{ RBBM_BLOCK_ID_MARB_3, 0x40, },
};

static void a3xx_snapshot_debugbus(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(debugbus_blocks); i++) {
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS, snapshot,
			a3xx_snapshot_debugbus_block,
			(void *) &debugbus_blocks[i]);
	}
}

static void _snapshot_hlsq_regs(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot)
{
	unsigned int next_pif = 0;

	/*
	 * Trying to read HLSQ registers when the HLSQ block is busy
	 * will cause the device to hang.  The RBBM_DEBUG_BUS has information
	 * that will tell us if the HLSQ block is busy or not.  Read values
	 * from the debug bus to ensure the HLSQ block is not busy (this
	 * is hardware dependent).  If the HLSQ block is busy do not
	 * dump the registers, otherwise dump the HLSQ registers.
	 */

	/*
	 * tpif status bits: RBBM_BLOCK_ID_HLSQ index 4 [4:0]
	 * spif status bits: RBBM_BLOCK_ID_HLSQ index 7 [5:0]
	 *
	 * if ((tpif == 0, 1, 28) && (spif == 0, 1, 10))
	 * then dump HLSQ registers
	 */

	/* check tpif */
	_rbbm_debug_bus_read(device, RBBM_BLOCK_ID_HLSQ, 4, &next_pif);
	next_pif &= 0x1f;
	if (next_pif != 0 && next_pif != 1 && next_pif != 28)
		return;

	/* check spif */
	_rbbm_debug_bus_read(device, RBBM_BLOCK_ID_HLSQ, 7, &next_pif);
	next_pif &= 0x3f;
	if (next_pif != 0 && next_pif != 1 && next_pif != 10)
		return;

	SNAPSHOT_REGISTERS(device, snapshot, a3xx_hlsq_registers);
}

#define VPC_MEM_SIZE 512

static size_t a3xx_snapshot_vpc_memory(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	size_t size = 4 * VPC_MEM_SIZE;
	int bank, addr, i = 0;

	if (remain < DEBUG_SECTION_SZ(size)) {
		SNAPSHOT_ERR_NOMEM(device, "VPC MEMORY");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_VPC_MEMORY;
	header->size = size;

	for (bank = 0; bank < 4; bank++) {
		for (addr = 0; addr < VPC_MEM_SIZE; addr++) {
			unsigned int val = bank | (addr << 4);

			kgsl_regwrite(device, A3XX_VPC_VPC_DEBUG_RAM_SEL, val);
			kgsl_regread(device, A3XX_VPC_VPC_DEBUG_RAM_READ,
				&data[i++]);
		}
	}

	return DEBUG_SECTION_SZ(size);
}

static size_t a3xx_snapshot_cp_pm4_ram(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int i;
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_PM4);
	size_t size = fw->size - 1;

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

	kgsl_regwrite(device, A3XX_CP_ME_RAM_RADDR, 0x0);
	for (i = 0; i < size; i++)
		kgsl_regread(device, A3XX_CP_ME_RAM_DATA, &data[i]);

	return DEBUG_SECTION_SZ(size);
}

static size_t a3xx_snapshot_cp_pfp_ram(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int i;
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_PFP);
	int size = fw->size - 1;

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
		kgsl_regread(device, A3XX_CP_PFP_UCODE_DATA, &data[i]);

	return DEBUG_SECTION_SZ(size);
}

/*
 * a3xx_snapshot() - A3XX GPU snapshot function
 * @adreno_dev: Device being snapshotted
 * @snapshot: Snapshot meta data
 * @remain: Amount of space left in snapshot memory
 *
 * This is where all of the A3XX specific bits and pieces are grabbed
 * into the snapshot memory
 */
void a3xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int reg, val;

	/* Disable Clock gating temporarily for the debug bus to work */
	kgsl_regwrite(device, A3XX_RBBM_CLOCK_CTL, 0x0);

	/* Save some CP information that the generic snapshot uses */
	kgsl_regread(device, A3XX_CP_IB1_BASE, &reg);
	snapshot->ib1base = (u64) reg;

	kgsl_regread(device, A3XX_CP_IB2_BASE, &reg);
	snapshot->ib2base = (u64) reg;

	kgsl_regread(device, A3XX_CP_IB1_BUFSZ, &snapshot->ib1size);
	kgsl_regread(device, A3XX_CP_IB2_BUFSZ, &snapshot->ib2size);

	SNAPSHOT_REGISTERS(device, snapshot, a3xx_registers);

	_snapshot_hlsq_regs(device, snapshot);

	kgsl_snapshot_indexed_registers(device, snapshot,
		A3XX_CP_STATE_DEBUG_INDEX, A3XX_CP_STATE_DEBUG_DATA, 0, 0x14);

	/* CP_ME indexed registers */
	kgsl_snapshot_indexed_registers(device, snapshot,
		A3XX_CP_ME_CNTL, A3XX_CP_ME_STATUS, 64, 44);

	/* VPC memory */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a3xx_snapshot_vpc_memory, NULL);

	/* CP MEQ */
	val = 16;

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG, snapshot,
		adreno_snapshot_cp_meq, &val);

	/* Shader working/shadow memory */
	 kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a3xx_snapshot_shader_memory, NULL);


	/* CP PFP and PM4 */

	/*
	 * Reading the microcode while the CP is running will
	 * basically move the CP instruction pointer to
	 * whatever address we read. Big badaboom ensues. Stop the CP
	 * (if it isn't already stopped) to ensure that we are safe.
	 * We do this here and not earlier to avoid corrupting the RBBM
	 * status and CP registers - by the time we get here we don't
	 * care about the contents of the CP anymore.
	 */

	kgsl_regread(device, A3XX_CP_ME_CNTL, &reg);
	reg |= (1 << 27) | (1 << 28);
	kgsl_regwrite(device, A3XX_CP_ME_CNTL, reg);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a3xx_snapshot_cp_pfp_ram, NULL);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a3xx_snapshot_cp_pm4_ram, NULL);

	/* CP ROQ */
	val = 128;
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, adreno_snapshot_cp_roq, &val);

	a3xx_snapshot_debugbus(device, snapshot);
}
