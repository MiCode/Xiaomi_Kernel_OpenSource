// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include "adreno_genc_snapshot.h"
#include "adreno.h"
#include "adreno_snapshot.h"

static struct kgsl_memdesc *genc_capturescript;
static struct kgsl_memdesc *genc_crashdump_registers;
static u32 *genc_cd_reg_end;

#define GENC_DEBUGBUS_BLOCK_SIZE 0x100

#define GENC_SP_READ_SEL_VAL(_location, _pipe, _statetype, _usptp, _sptp) \
				(FIELD_PREP(GENMASK(19, 18), _location) | \
				 FIELD_PREP(GENMASK(17, 16), _pipe) | \
				 FIELD_PREP(GENMASK(15, 8), _statetype) | \
				 FIELD_PREP(GENMASK(7, 4), _usptp) | \
				 FIELD_PREP(GENMASK(3, 0), _sptp))

#define GENC_CP_APERTURE_REG_VAL(_pipe, _cluster, _context) \
			(FIELD_PREP(GENMASK(13, 12), _pipe) | \
			 FIELD_PREP(GENMASK(10, 8), _cluster) | \
			 FIELD_PREP(GENMASK(5, 4), _context))

#define GENC_DEBUGBUS_SECTION_SIZE (sizeof(struct kgsl_snapshot_debugbus) \
			+ (GENC_DEBUGBUS_BLOCK_SIZE << 3))

#define CD_REG_END 0xaaaaaaaa

static int CD_WRITE(u64 *ptr, u32 offset, u32 val)
{
	ptr[0] = val;
	ptr[1] = FIELD_PREP(GENMASK(63, 44), offset) | BIT(21) | BIT(0);

	return 2;
}

static int CD_READ(u64 *ptr, u32 offset, u32 size, u64 target)
{
	ptr[0] = target;
	ptr[1] = FIELD_PREP(GENMASK(63, 44), offset) | size;

	return 2;
}

static void CD_FINISH(u64 *ptr, u32 offset)
{
	genc_cd_reg_end = genc_crashdump_registers->hostptr + offset;
	*genc_cd_reg_end = CD_REG_END;
	ptr[0] = genc_crashdump_registers->gpuaddr + offset;
	ptr[1] = FIELD_PREP(GENMASK(63, 44), GENC_CP_CRASH_DUMP_STATUS) | BIT(0);
	ptr[2] = 0;
	ptr[3] = 0;
}

static bool CD_SCRIPT_CHECK(struct kgsl_device *device)
{
	return (genc_is_smmu_stalled(device) || (!device->snapshot_crashdumper) ||
		IS_ERR_OR_NULL(genc_capturescript) ||
		IS_ERR_OR_NULL(genc_crashdump_registers));
}

static bool _genc_do_crashdump(struct kgsl_device *device)
{
	unsigned int reg = 0;
	ktime_t timeout;

	kgsl_regwrite(device, GENC_CP_CRASH_SCRIPT_BASE_LO,
			lower_32_bits(genc_capturescript->gpuaddr));
	kgsl_regwrite(device, GENC_CP_CRASH_SCRIPT_BASE_HI,
			upper_32_bits(genc_capturescript->gpuaddr));
	kgsl_regwrite(device, GENC_CP_CRASH_DUMP_CNTL, 1);

	timeout = ktime_add_ms(ktime_get(), CP_CRASH_DUMPER_TIMEOUT);

	might_sleep();
	for (;;) {
		/* make sure we're reading the latest value */
		rmb();
		if ((*genc_cd_reg_end) != CD_REG_END)
			break;
		if (ktime_compare(ktime_get(), timeout) > 0)
			break;
		/* Wait 1msec to avoid unnecessary looping */
		usleep_range(100, 1000);
	}

	kgsl_regread(device, GENC_CP_CRASH_DUMP_STATUS, &reg);

	if (WARN(!(reg & 0x2), "Crashdumper timed out\n"))
		return false;

	return true;
}

static size_t genc_legacy_snapshot_registers(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct reg_list *regs = priv;

	if (regs->sel)
		kgsl_regwrite(device, regs->sel->host_reg, regs->sel->val);

	return adreno_snapshot_registers_v2(device, buf, remain, (void *)regs->regs);
}

static size_t genc_snapshot_registers(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct reg_list *regs = (struct reg_list *)priv;
	const u32 *ptr = regs->regs;
	unsigned int *data = (unsigned int *)buf;
	unsigned int *src;
	unsigned int size = adreno_snapshot_regs_count(ptr) * 4;

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	src = genc_crashdump_registers->hostptr + regs->offset;

	for (ptr = regs->regs; ptr[0] != UINT_MAX; ptr += 2) {
		unsigned int cnt = REG_COUNT(ptr);

		if (cnt == 1)
			*data++ = BIT(31) | ptr[0];
		else {
			*data++ = ptr[0];
			*data++ = cnt;
		}
		memcpy(data, src, cnt << 2);
		data += cnt;
		src += cnt;
	}

	/* Return the size of the section */
	return size;
}

static size_t genc_legacy_snapshot_shader(struct kgsl_device *device,
				u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_shader_v2 *header =
		(struct kgsl_snapshot_shader_v2 *) buf;
	struct genc_shader_block_info *info = (struct genc_shader_block_info *) priv;
	struct genc_shader_block *block = info->block;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int read_sel;

	if (!device->snapshot_legacy)
		return 0;

	if (remain < (sizeof(*header) + (block->size << 2))) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	header->type = block->statetype;
	header->index = block->sp_id;
	header->size = block->size;
	header->usptp = block->usptp;
	header->location = block->location;
	header->pipe_id = block->pipeid;

	read_sel = GENC_SP_READ_SEL_VAL(block->location, block->pipeid,
				block->statetype, block->usptp, block->sp_id);

	kgsl_regwrite(device, GENC_SP_READ_SEL, read_sel);

	/*
	 * An explicit barrier is needed so that reads do not happen before
	 * the register write.
	 */
	mb();

	kgsl_regmap_bulk_read(&device->regmap, GENC_SP_AHB_READ_APERTURE,
			&data, block->size);

	return (sizeof(*header) + (block->size << 2));
}

static size_t genc_snapshot_shader_memory(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_shader_v2 *header =
		(struct kgsl_snapshot_shader_v2 *) buf;
	struct genc_shader_block_info *info = (struct genc_shader_block_info *) priv;
	struct genc_shader_block *block = info->block;
	unsigned int *data = (unsigned int *) (buf + sizeof(*header));

	if (remain < (sizeof(*header) + (block->size << 2))) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	header->type = block->statetype;
	header->index = block->sp_id;
	header->size = block->size;
	header->usptp = block->usptp;
	header->location = block->location;
	header->pipe_id = block->pipeid;

	memcpy(data, genc_crashdump_registers->hostptr + info->offset,
			(block->size << 2));

	return (sizeof(*header) + (block->size << 2));
}

static void genc_snapshot_shader(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	unsigned int i;
	struct genc_shader_block_info info;
	u64 *ptr;
	u32 offset = 0;
	size_t (*func)(struct kgsl_device *device, u8 *buf, size_t remain,
		void *priv) = genc_legacy_snapshot_shader;

	if (CD_SCRIPT_CHECK(device)) {
		for (i = 0; i < ARRAY_SIZE(genc_shader_blocks); i++) {
			info.block = &genc_shader_blocks[i];
			info.offset = offset;
			offset += genc_shader_blocks[i].size << 2;

			/* Shader working/shadow memory */
			kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_SHADER_V2,
				snapshot, func, &info);
		}
		return;
	}

	/* Build the crash script */
	ptr = genc_capturescript->hostptr;
	offset = 0;

	for (i = 0; i < ARRAY_SIZE(genc_shader_blocks); i++) {
		struct genc_shader_block *block = &genc_shader_blocks[i];

		/* Program the aperture */
		ptr += CD_WRITE(ptr, GENC_SP_READ_SEL,
			GENC_SP_READ_SEL_VAL(block->location, block->pipeid,
				block->statetype, block->usptp, block->sp_id));

		/* Read all the data in one chunk */
		ptr += CD_READ(ptr, GENC_SP_AHB_READ_APERTURE, block->size,
			genc_crashdump_registers->gpuaddr + offset);

		offset += block->size << 2;
	}

	/* Marker for end of script */
	CD_FINISH(ptr, offset);

	/* Try to run the crash dumper */
	if (_genc_do_crashdump(device))
		func = genc_snapshot_shader_memory;

	offset = 0;

	for (i = 0; i < ARRAY_SIZE(genc_shader_blocks); i++) {
		info.block = &genc_shader_blocks[i];
		info.offset = offset;
		offset += genc_shader_blocks[i].size << 2;

		/* Shader working/shadow memory */
		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_SHADER_V2,
			snapshot, func, &info);
	}
}

static void genc_snapshot_mempool(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	/* set CP_CHICKEN_DBG[StabilizeMVC] to stabilize it while dumping */
	kgsl_regrmw(device, GENC_CP_CHICKEN_DBG, 0x4, 0x4);
	kgsl_regrmw(device, GENC_CP_BV_CHICKEN_DBG, 0x4, 0x4);

	kgsl_snapshot_indexed_registers(device, snapshot,
		GENC_CP_MEM_POOL_DBG_ADDR, GENC_CP_MEM_POOL_DBG_DATA,
		0, 0x2100);

	kgsl_snapshot_indexed_registers(device, snapshot,
		GENC_CP_BV_MEM_POOL_DBG_ADDR, GENC_CP_BV_MEM_POOL_DBG_DATA,
		0, 0x2100);

	kgsl_regrmw(device, GENC_CP_CHICKEN_DBG, 0x4, 0x0);
	kgsl_regrmw(device, GENC_CP_BV_CHICKEN_DBG, 0x4, 0x0);
}

static unsigned int genc_read_dbgahb(struct kgsl_device *device,
				unsigned int regbase, unsigned int reg)
{
	unsigned int val;

	kgsl_regread(device, (GENC_SP_AHB_READ_APERTURE + reg - regbase), &val);
	return val;
}

static size_t genc_legacy_snapshot_cluster_dbgahb(struct kgsl_device *device,
				u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs_v2 *header =
				(struct kgsl_snapshot_mvc_regs_v2 *)buf;
	struct genc_sptp_cluster_registers *cluster =
			(struct genc_sptp_cluster_registers *)priv;
	const u32 *ptr = cluster->regs;
	unsigned int read_sel;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int j;
	unsigned int size = adreno_snapshot_regs_count(ptr) * 4;

	if (!device->snapshot_legacy)
		return 0;

	if (remain < (sizeof(*header) + size)) {
		SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
		return 0;
	}

	header->ctxt_id = cluster->context_id;
	header->cluster_id = cluster->cluster_id;
	header->pipe_id = cluster->pipe_id;
	header->location_id = cluster->location_id;

	read_sel = GENC_SP_READ_SEL_VAL(cluster->location_id, cluster->pipe_id,
			cluster->statetype, 0, 0);

	kgsl_regwrite(device, GENC_SP_READ_SEL, read_sel);

	for (ptr = cluster->regs; ptr[0] != UINT_MAX; ptr += 2) {
		unsigned int count = REG_COUNT(ptr);

		if (count == 1)
			*data++ = ptr[0];
		else {
			*data++ = ptr[0] | (1 << 31);
			*data++ = ptr[1];
		}
		for (j = ptr[0]; j <= ptr[1]; j++)
			*data++ = genc_read_dbgahb(device, cluster->regbase, j);
	}

	return (size + sizeof(*header));
}

static size_t genc_snapshot_cluster_dbgahb(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs_v2 *header =
				(struct kgsl_snapshot_mvc_regs_v2 *)buf;
	struct genc_sptp_cluster_registers *cluster =
				(struct genc_sptp_cluster_registers *)priv;
	const u32 *ptr = cluster->regs;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int *src;
	unsigned int size = adreno_snapshot_regs_count(ptr) * 4;

	if (remain < (sizeof(*header) + size)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	header->ctxt_id = cluster->context_id;
	header->cluster_id = cluster->cluster_id;
	header->pipe_id = cluster->pipe_id;
	header->location_id = cluster->location_id;

	src = genc_crashdump_registers->hostptr + cluster->offset;

	for (ptr = cluster->regs; ptr[0] != UINT_MAX; ptr += 2) {
		unsigned int cnt = REG_COUNT(ptr);

		if (cnt == 1)
			*data++ = ptr[0];
		else {
			*data++ = ptr[0] | (1 << 31);
			*data++ = ptr[1];
		}
		memcpy(data, src, cnt << 2);
		data += cnt;
		src += cnt;
	}

	return (size + sizeof(*header));
}

static void genc_snapshot_dbgahb_regs(struct kgsl_device *device,
			struct kgsl_snapshot *snapshot)
{
	int i;
	u64 *ptr, offset = 0;
	unsigned int count;
	size_t (*func)(struct kgsl_device *device, u8 *buf, size_t remain,
		void *priv) = genc_legacy_snapshot_cluster_dbgahb;

	if (CD_SCRIPT_CHECK(device)) {
		for (i = 0; i < ARRAY_SIZE(genc_sptp_clusters); i++)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_MVC_V2, snapshot, func,
				&genc_sptp_clusters[i]);
		return;
	}

	/* Build the crash script */
	ptr = genc_capturescript->hostptr;

	for (i = 0; i < ARRAY_SIZE(genc_sptp_clusters); i++) {
		struct genc_sptp_cluster_registers *cluster = &genc_sptp_clusters[i];
		const u32 *regs = cluster->regs;

		cluster->offset = offset;

		/* Program the aperture */
		ptr += CD_WRITE(ptr, GENC_SP_READ_SEL, GENC_SP_READ_SEL_VAL
			(cluster->location_id, cluster->pipe_id, cluster->statetype, 0, 0));

		for (; regs[0] != UINT_MAX; regs += 2) {
			count = REG_COUNT(regs);
			ptr += CD_READ(ptr, (GENC_SP_AHB_READ_APERTURE +
				regs[0] - cluster->regbase), count,
				(genc_crashdump_registers->gpuaddr + offset));

			offset += count * sizeof(unsigned int);
		}
	}
	/* Marker for end of script */
	CD_FINISH(ptr, offset);

	/* Try to run the crash dumper */
	if (_genc_do_crashdump(device))
		func = genc_snapshot_cluster_dbgahb;

	/* Capture the registers in snapshot */
	for (i = 0; i < ARRAY_SIZE(genc_sptp_clusters); i++)
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_MVC_V2, snapshot, func, &genc_sptp_clusters[i]);
}

static size_t genc_legacy_snapshot_mvc(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs_v2 *header =
					(struct kgsl_snapshot_mvc_regs_v2 *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	struct genc_cluster_registers *cluster =
			(struct genc_cluster_registers *)priv;
	const u32 *ptr = cluster->regs;
	unsigned int j;
	unsigned int size = adreno_snapshot_regs_count(ptr) * 4;

	if (remain < (sizeof(*header) + size)) {
		SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
		return 0;
	}

	header->ctxt_id = (cluster->context_id == STATE_FORCE_CTXT_1) ? 1 : 0;
	header->cluster_id = cluster->cluster_id;
	header->pipe_id = cluster->pipe_id;
	header->location_id = UINT_MAX;

	/*
	 * Set the AHB control for the Host to read from the
	 * cluster/context for this iteration.
	 */
	kgsl_regwrite(device, GENC_CP_APERTURE_CNTL_HOST, GENC_CP_APERTURE_REG_VAL
			(cluster->pipe_id, cluster->cluster_id, cluster->context_id));

	if (cluster->sel)
		kgsl_regwrite(device, cluster->sel->host_reg, cluster->sel->val);

	for (ptr = cluster->regs; ptr[0] != UINT_MAX; ptr += 2) {
		unsigned int count = REG_COUNT(ptr);

		if (count == 1)
			*data++ = ptr[0];
		else {
			*data++ = ptr[0] | (1 << 31);
			*data++ = ptr[1];
		}
		for (j = ptr[0]; j <= ptr[1]; j++) {
			kgsl_regread(device, j, data);
			data++;
		}
	}

	return (size + sizeof(*header));
}

static size_t genc_snapshot_mvc(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs_v2 *header =
				(struct kgsl_snapshot_mvc_regs_v2 *)buf;
	struct genc_cluster_registers *cluster =
			(struct genc_cluster_registers *)priv;
	const u32 *ptr = cluster->regs;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int *src;
	unsigned int cnt;
	unsigned int size = adreno_snapshot_regs_count(ptr) * 4;

	if (remain < (sizeof(*header) + size)) {
		SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
		return 0;
	}

	header->ctxt_id = (cluster->context_id == STATE_FORCE_CTXT_1) ? 1 : 0;
	header->cluster_id = cluster->cluster_id;
	header->pipe_id = cluster->pipe_id;
	header->location_id = UINT_MAX;

	src = genc_crashdump_registers->hostptr + cluster->offset;

	for (ptr = cluster->regs; ptr[0] != UINT_MAX; ptr += 2) {
		cnt = REG_COUNT(ptr);

		if (cnt == 1)
			*data++ = ptr[0];
		else {
			*data++ = ptr[0] | (1 << 31);
			*data++ = ptr[1];
		}
		memcpy(data, src, cnt << 2);
		src += cnt;
		data += cnt;
	}

	return (size + sizeof(*header));

}

static void genc_snapshot_mvc_regs(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	int i;
	u64 *ptr, offset = 0;
	unsigned int count;
	size_t (*func)(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv) = genc_legacy_snapshot_mvc;

	if (CD_SCRIPT_CHECK(device)) {
		for (i = 0; i < ARRAY_SIZE(genc_clusters); i++)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_MVC_V2, snapshot, func, &genc_clusters[i]);
		return;
	}

	/* Build the crash script */
	ptr = genc_capturescript->hostptr;

	for (i = 0; i < ARRAY_SIZE(genc_clusters); i++) {
		struct genc_cluster_registers *cluster = &genc_clusters[i];
		const u32 *regs = cluster->regs;

		if (cluster->sel)
			ptr += CD_WRITE(ptr, cluster->sel->cd_reg, cluster->sel->val);

		cluster->offset = offset;
		ptr += CD_WRITE(ptr, GENC_CP_APERTURE_CNTL_CD, GENC_CP_APERTURE_REG_VAL
			(cluster->pipe_id, cluster->cluster_id, cluster->context_id));

		for (; regs[0] != UINT_MAX; regs += 2) {
			count = REG_COUNT(regs);

			ptr += CD_READ(ptr, regs[0],
				count, (genc_crashdump_registers->gpuaddr + offset));

			offset += count * sizeof(unsigned int);
		}
	}

	/* Marker for end of script */
	CD_FINISH(ptr, offset);

	/* Try to run the crash dumper */
	if (_genc_do_crashdump(device))
		func = genc_snapshot_mvc;

	for (i = 0; i < ARRAY_SIZE(genc_clusters); i++)
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_MVC_V2, snapshot, func, &genc_clusters[i]);
}

/* genc_dbgc_debug_bus_read() - Read data from trace bus */
static void genc_dbgc_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int reg;

	reg = FIELD_PREP(GENMASK(7, 0), index) |
		FIELD_PREP(GENMASK(24, 16), block_id);

	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_SEL_A, reg);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_SEL_B, reg);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_SEL_C, reg);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_SEL_D, reg);

	/*
	 * There needs to be a delay of 1 us to ensure enough time for correct
	 * data is funneled into the trace buffer
	 */
	udelay(1);

	kgsl_regread(device, GENC_DBGC_CFG_DBGBUS_TRACE_BUF2, val);
	val++;
	kgsl_regread(device, GENC_DBGC_CFG_DBGBUS_TRACE_BUF1, val);
}

/* genc_snapshot_dbgc_debugbus_block() - Capture debug data for a gpu block */
static size_t genc_snapshot_dbgc_debugbus_block(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header =
		(struct kgsl_snapshot_debugbus *)buf;
	const u32 *block = priv;
	int i;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));

	if (remain < GENC_DEBUGBUS_SECTION_SIZE) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = *block;
	header->count = GENC_DEBUGBUS_BLOCK_SIZE * 2;

	for (i = 0; i < GENC_DEBUGBUS_BLOCK_SIZE; i++)
		genc_dbgc_debug_bus_read(device, *block, i, &data[i*2]);

	return GENC_DEBUGBUS_SECTION_SIZE;
}

/* genc_cx_dbgc_debug_bus_read() - Read data from trace bus */
static void genc_cx_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int reg;

	reg = FIELD_PREP(GENMASK(7, 0), index) |
		FIELD_PREP(GENMASK(24, 16), block_id);

	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_SEL_A, reg);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_SEL_B, reg);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_SEL_C, reg);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_SEL_D, reg);

	/*
	 * There needs to be a delay of 1 us to ensure enough time for correct
	 * data is funneled into the trace buffer
	 */
	udelay(1);

	adreno_cx_dbgc_regread(device, GENC_CX_DBGC_CFG_DBGBUS_TRACE_BUF2, val);
	val++;
	adreno_cx_dbgc_regread(device, GENC_CX_DBGC_CFG_DBGBUS_TRACE_BUF1, val);
}

/*
 * genc_snapshot_cx_dbgc_debugbus_block() - Capture debug data for a gpu
 * block from the CX DBGC block
 */
static size_t genc_snapshot_cx_dbgc_debugbus_block(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header =
		(struct kgsl_snapshot_debugbus *)buf;
	const u32 *block = priv;
	int i;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));

	if (remain < GENC_DEBUGBUS_SECTION_SIZE) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = *block;
	header->count = GENC_DEBUGBUS_BLOCK_SIZE * 2;

	for (i = 0; i < GENC_DEBUGBUS_BLOCK_SIZE; i++)
		genc_cx_debug_bus_read(device, *block, i, &data[i*2]);

	return GENC_DEBUGBUS_SECTION_SIZE;
}

/* genc_snapshot_debugbus() - Capture debug bus data */
static void genc_snapshot_debugbus(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	int i;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_CNTLT,
			FIELD_PREP(GENMASK(31, 28), 0xf));

	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_CNTLM,
			FIELD_PREP(GENMASK(27, 24), 0xf));

	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_IVTL_0, 0);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_IVTL_1, 0);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_IVTL_2, 0);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_IVTL_3, 0);

	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_BYTEL_0,
			FIELD_PREP(GENMASK(3, 0), 0x0) |
			FIELD_PREP(GENMASK(7, 4), 0x1) |
			FIELD_PREP(GENMASK(11, 8), 0x2) |
			FIELD_PREP(GENMASK(15, 12), 0x3) |
			FIELD_PREP(GENMASK(19, 16), 0x4) |
			FIELD_PREP(GENMASK(23, 20), 0x5) |
			FIELD_PREP(GENMASK(27, 24), 0x6) |
			FIELD_PREP(GENMASK(31, 28), 0x7));
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_BYTEL_1,
			FIELD_PREP(GENMASK(3, 0), 0x8) |
			FIELD_PREP(GENMASK(7, 4), 0x9) |
			FIELD_PREP(GENMASK(11, 8), 0xa) |
			FIELD_PREP(GENMASK(15, 12), 0xb) |
			FIELD_PREP(GENMASK(19, 16), 0xc) |
			FIELD_PREP(GENMASK(23, 20), 0xd) |
			FIELD_PREP(GENMASK(27, 24), 0xe) |
			FIELD_PREP(GENMASK(31, 28), 0xf));

	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_MASKL_0, 0);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_MASKL_1, 0);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_MASKL_2, 0);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_MASKL_3, 0);

	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_CNTLT,
			FIELD_PREP(GENMASK(31, 28), 0xf));

	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_CNTLM,
			FIELD_PREP(GENMASK(27, 24), 0xf));

	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_IVTL_0, 0);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_IVTL_1, 0);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_IVTL_2, 0);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_IVTL_3, 0);

	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_BYTEL_0,
			FIELD_PREP(GENMASK(3, 0), 0x0) |
			FIELD_PREP(GENMASK(7, 4), 0x1) |
			FIELD_PREP(GENMASK(11, 8), 0x2) |
			FIELD_PREP(GENMASK(15, 12), 0x3) |
			FIELD_PREP(GENMASK(19, 16), 0x4) |
			FIELD_PREP(GENMASK(23, 20), 0x5) |
			FIELD_PREP(GENMASK(27, 24), 0x6) |
			FIELD_PREP(GENMASK(31, 28), 0x7));
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_BYTEL_1,
			FIELD_PREP(GENMASK(3, 0), 0x8) |
			FIELD_PREP(GENMASK(7, 4), 0x9) |
			FIELD_PREP(GENMASK(11, 8), 0xa) |
			FIELD_PREP(GENMASK(15, 12), 0xb) |
			FIELD_PREP(GENMASK(19, 16), 0xc) |
			FIELD_PREP(GENMASK(23, 20), 0xd) |
			FIELD_PREP(GENMASK(27, 24), 0xe) |
			FIELD_PREP(GENMASK(31, 28), 0xf));

	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_MASKL_0, 0);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_MASKL_1, 0);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_MASKL_2, 0);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_MASKL_3, 0);

	for (i = 0; i < ARRAY_SIZE(genc_debugbus_blocks); i++) {
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS,
			snapshot, genc_snapshot_dbgc_debugbus_block,
			(void *) &genc_debugbus_blocks[i]);
	}

	/*
	 * GBIF has same debugbus as of other GPU blocks hence fall back to
	 * default path if GPU uses GBIF.
	 * GBIF uses exactly same ID as of VBIF so use it as it is.
	 */
	kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS,
			snapshot, genc_snapshot_dbgc_debugbus_block,
			(void *) &genc_gbif_debugbus_blocks[0]);

	kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS,
			snapshot, genc_snapshot_dbgc_debugbus_block,
			(void *) &genc_gbif_debugbus_blocks[1]);

	/* Dump the CX debugbus data if the block exists */
	if (adreno_is_cx_dbgc_register(device, GENC_CX_DBGC_CFG_DBGBUS_SEL_A)) {
		for (i = 0; i < ARRAY_SIZE(genc_cx_dbgc_debugbus_blocks); i++) {
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot, genc_snapshot_cx_dbgc_debugbus_block,
				(void *) &genc_cx_dbgc_debugbus_blocks[i]);
		}
		/*
		 * Get debugbus for GBIF CX part if GPU has GBIF block
		 * GBIF uses exactly same ID as of VBIF so use
		 * it as it is.
		 */
		kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS, snapshot,
				genc_snapshot_cx_dbgc_debugbus_block,
				(void *) &genc_gbif_debugbus_blocks[0]);
	}
}



/* genc_snapshot_sqe() - Dump SQE data in snapshot */
static size_t genc_snapshot_sqe(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);

	if (remain < DEBUG_SECTION_SZ(1)) {
		SNAPSHOT_ERR_NOMEM(device, "SQE VERSION DEBUG");
		return 0;
	}

	/* Dump the SQE firmware version */
	header->type = SNAPSHOT_DEBUG_SQE_VERSION;
	header->size = 1;
	*data = fw->version;

	return DEBUG_SECTION_SZ(1);
}

/* Snapshot the preemption related buffers */
static size_t snapshot_preemption_record(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_memdesc *memdesc = priv;
	struct kgsl_snapshot_gpu_object_v2 *header =
		(struct kgsl_snapshot_gpu_object_v2 *)buf;
	u8 *ptr = buf + sizeof(*header);

	if (remain < (GENC_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES +
						sizeof(*header))) {
		SNAPSHOT_ERR_NOMEM(device, "PREEMPTION RECORD");
		return 0;
	}

	header->size = GENC_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES >> 2;
	header->gpuaddr = memdesc->gpuaddr;
	header->ptbase =
		kgsl_mmu_pagetable_get_ttbr0(device->mmu.defaultpagetable);
	header->type = SNAPSHOT_GPU_OBJECT_GLOBAL;

	memcpy(ptr, memdesc->hostptr, GENC_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES);

	return GENC_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES + sizeof(*header);
}

static void genc_reglist_snapshot(struct kgsl_device *device,
					struct kgsl_snapshot *snapshot)
{
	u64 *ptr, offset = 0;
	int i;
	u32 r;
	size_t (*func)(struct kgsl_device *device, u8 *buf, size_t remain,
		void *priv) = genc_legacy_snapshot_registers;

	if (CD_SCRIPT_CHECK(device)) {
		for (i = 0; i < ARRAY_SIZE(genc_reg_list); i++)
			kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2,
				snapshot, func, &genc_reg_list[i]);
		return;
	}

	/* Build the crash script */
	ptr = (u64 *)genc_capturescript->hostptr;

	for (i = 0; i < ARRAY_SIZE(genc_reg_list); i++) {
		struct reg_list *regs = &genc_reg_list[i];
		const u32 *regs_ptr = regs->regs;

		regs->offset = offset;

		/* Program the SEL_CNTL_CD register appropriately */
		if (regs->sel)
			ptr += CD_WRITE(ptr, regs->sel->cd_reg, regs->sel->val);

		for (; regs_ptr[0] != UINT_MAX; regs_ptr += 2) {
			r = REG_COUNT(regs_ptr);
			ptr += CD_READ(ptr, regs_ptr[0], r,
				(genc_crashdump_registers->gpuaddr + offset));
			offset += r * sizeof(u32);
		}
	}

	/* Marker for end of script */
	CD_FINISH(ptr, offset);

	/* Try to run the crash dumper */
	if (_genc_do_crashdump(device))
		func = genc_snapshot_registers;

	for (i = 0; i < ARRAY_SIZE(genc_reg_list); i++)
		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2,
			snapshot, func, &genc_reg_list[i]);

}

static void genc_snapshot_br_roq(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	unsigned int roq_size;

	/*
	 * CP ROQ dump units is 4 dwords. The number of units is stored
	 * in CP_ROQ_THRESHOLDS_2[31:20], but it is not accessible to
	 * host. Program the GENC_CP_SQE_UCODE_DBG_ADDR with 0x70d3 offset
	 * and read the value CP_ROQ_THRESHOLDS_2 from
	 * GENC_CP_SQE_UCODE_DBG_DATA
	 */
	kgsl_regwrite(device, GENC_CP_SQE_UCODE_DBG_ADDR, 0x70d3);
	kgsl_regread(device, GENC_CP_SQE_UCODE_DBG_DATA, &roq_size);
	roq_size = roq_size >> 20;
	kgsl_snapshot_indexed_registers(device, snapshot,
			GENC_CP_ROQ_DBG_ADDR, GENC_CP_ROQ_DBG_DATA, 0, (roq_size << 2));
}

static void genc_snapshot_bv_roq(struct kgsl_device *device,
			struct kgsl_snapshot *snapshot)
{
	unsigned int roq_size;

	/*
	 * CP ROQ dump units is 4 dwords. The number of units is stored
	 * in CP_BV_ROQ_THRESHOLDS_2[31:20], but it is not accessible to
	 * host. Program the GENC_CP_BV_SQE_UCODE_DBG_ADDR with 0x70d3 offset
	 * (at which CP stores the roq values) and read the value of
	 * CP_BV_ROQ_THRESHOLDS_2 from GENC_CP_BV_SQE_UCODE_DBG_DATA
	 */
	kgsl_regwrite(device, GENC_CP_BV_SQE_UCODE_DBG_ADDR, 0x70d3);
	kgsl_regread(device, GENC_CP_BV_SQE_UCODE_DBG_DATA, &roq_size);
	roq_size = roq_size >> 20;
	kgsl_snapshot_indexed_registers(device, snapshot,
			GENC_CP_BV_ROQ_DBG_ADDR, GENC_CP_BV_ROQ_DBG_DATA, 0, (roq_size << 2));
}

static void genc_snapshot_lpac_roq(struct kgsl_device *device,
			struct kgsl_snapshot *snapshot)
{
	unsigned int roq_size;

	/*
	 * CP ROQ dump units is 4 dwords. The number of units is stored
	 * in CP_LPAC_ROQ_THRESHOLDS_2[31:20], but it is not accessible to
	 * host. Program the GENC_CP_SQE_AC_UCODE_DBG_ADDR with 0x70d3 offset
	 * (at which CP stores the roq values) and read the value of
	 * CP_LPAC_ROQ_THRESHOLDS_2 from GENC_CP_SQE_AC_UCODE_DBG_DATA
	 */
	kgsl_regwrite(device, GENC_CP_SQE_AC_UCODE_DBG_ADDR, 0x70d3);
	kgsl_regread(device, GENC_CP_SQE_AC_UCODE_DBG_DATA, &roq_size);
	roq_size = roq_size >> 20;
	kgsl_snapshot_indexed_registers(device, snapshot,
			GENC_CP_LPAC_ROQ_DBG_ADDR, GENC_CP_LPAC_ROQ_DBG_DATA, 0, (roq_size << 2));
}

/*
 * genc_snapshot() - GENC GPU snapshot function
 * @adreno_dev: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the GENC specific bits and pieces are grabbed
 * into the snapshot memory
 */
void genc_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb;
	unsigned int i;
	u32 hi, lo, val;

	/*
	 * Dump debugbus data here to capture it for both
	 * GMU and GPU snapshot. Debugbus data can be accessed
	 * even if the gx headswitch is off. If gx
	 * headswitch is off, data for gx blocks will show as
	 * 0x5c00bd00.
	 */
	genc_snapshot_debugbus(adreno_dev, snapshot);

	if (!gmu_core_dev_gx_is_on(device))
		return;

	kgsl_regread(device, GENC_CP_IB1_BASE, &lo);
	kgsl_regread(device, GENC_CP_IB1_BASE_HI, &hi);

	snapshot->ib1base = (((u64) hi) << 32) | lo;

	kgsl_regread(device, GENC_CP_IB2_BASE, &lo);
	kgsl_regread(device, GENC_CP_IB2_BASE_HI, &hi);

	snapshot->ib2base = (((u64) hi) << 32) | lo;

	kgsl_regread(device, GENC_CP_IB1_REM_SIZE, &snapshot->ib1size);
	kgsl_regread(device, GENC_CP_IB2_REM_SIZE, &snapshot->ib2size);

	/* Assert the isStatic bit before triggering snapshot */
	kgsl_regwrite(device, GENC_RBBM_SNAPSHOT_STATUS, 0x1);

	/* Dump the registers which get affected by crash dumper trigger */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2,
		snapshot, adreno_snapshot_registers_v2,
		(void *)genc_pre_crashdumper_registers);

	genc_reglist_snapshot(device, snapshot);

	/*
	 * Need to program this register before capturing resource table
	 * to workaround a CGC issue
	 */
	kgsl_regrmw(device, GENC_RBBM_CLOCK_MODE_CP, 0x7, 0);
	kgsl_snapshot_indexed_registers(device, snapshot,
		GENC_CP_RESOURCE_TBL_DBG_ADDR, GENC_CP_RESOURCE_TBL_DBG_DATA,
		0, 0x4100);

	for (i = 0; i < ARRAY_SIZE(genc_cp_indexed_reg_list); i++)
		kgsl_snapshot_indexed_registers(device, snapshot,
			genc_cp_indexed_reg_list[i].addr,
			genc_cp_indexed_reg_list[i].data, 0,
			genc_cp_indexed_reg_list[i].size);

	genc_snapshot_br_roq(device, snapshot);

	genc_snapshot_bv_roq(device, snapshot);

	genc_snapshot_lpac_roq(device, snapshot);

	/* SQE Firmware */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, genc_snapshot_sqe, NULL);

	/* Mempool debug data */
	genc_snapshot_mempool(device, snapshot);

	/* Shader memory */
	genc_snapshot_shader(device, snapshot);

	/* MVC register section */
	genc_snapshot_mvc_regs(device, snapshot);

	/* registers dumped through DBG AHB */
	genc_snapshot_dbgahb_regs(device, snapshot);

	kgsl_regread(device, GENC_RBBM_SNAPSHOT_STATUS, &val);
	if (!val)
		dev_err(device->dev,
			"Interface signals may have changed during snapshot\n");

	kgsl_regwrite(device, GENC_RBBM_SNAPSHOT_STATUS, 0x0);

	/* Preemption record */
	if (adreno_is_preemption_enabled(adreno_dev)) {
		FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
				snapshot, snapshot_preemption_record,
				rb->preemption_desc);
		}
	}
}

void genc_crashdump_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (IS_ERR_OR_NULL(genc_capturescript))
		genc_capturescript = kgsl_allocate_global(device,
			4 * PAGE_SIZE, 0, KGSL_MEMFLAGS_GPUREADONLY,
			KGSL_MEMDESC_PRIVILEGED, "capturescript");

	if (IS_ERR(genc_capturescript))
		return;

	if (IS_ERR_OR_NULL(genc_crashdump_registers))
		genc_crashdump_registers = kgsl_allocate_global(device,
			300 * PAGE_SIZE, 0, 0, KGSL_MEMDESC_PRIVILEGED,
			"capturescript_regs");

	if (IS_ERR(genc_crashdump_registers))
		return;
}
