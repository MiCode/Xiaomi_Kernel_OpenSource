// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "adreno.h"
#include "adreno_gen7_snapshot.h"
#include "adreno_snapshot.h"

static struct kgsl_memdesc *gen7_capturescript;
static struct kgsl_memdesc *gen7_crashdump_registers;
static u32 *gen7_cd_reg_end;
static const struct gen7_snapshot_block_list *gen7_snapshot_block_list;
static bool gen7_crashdump_timedout;

const struct gen7_snapshot_block_list gen7_0_0_snapshot_block_list = {
	.pre_crashdumper_regs = gen7_0_0_pre_crashdumper_registers,
	.cp_indexed_reg_list = gen7_0_0_cp_indexed_reg_list,
	.cp_indexed_reg_list_len = ARRAY_SIZE(gen7_0_0_cp_indexed_reg_list),
	.debugbus_blocks = gen7_0_0_debugbus_blocks,
	.debugbus_blocks_len = ARRAY_SIZE(gen7_0_0_debugbus_blocks),
	.gbif_debugbus_blocks = gen7_gbif_debugbus_blocks,
	.gbif_debugbus_blocks_len = ARRAY_SIZE(gen7_gbif_debugbus_blocks),
	.cx_debugbus_blocks = gen7_cx_dbgc_debugbus_blocks,
	.cx_debugbus_blocks_len = ARRAY_SIZE(gen7_cx_dbgc_debugbus_blocks),
	.external_core_regs = gen7_0_0_external_core_regs,
	.num_external_core_regs = ARRAY_SIZE(gen7_0_0_external_core_regs),
	.gmu_regs = gen7_0_0_gmu_registers,
	.gmu_gx_regs = gen7_0_0_gmu_gx_registers,
	.rscc_regs = gen7_0_0_rscc_registers,
	.reg_list = gen7_0_0_reg_list,
	.shader_blocks = gen7_0_0_shader_blocks,
	.num_shader_blocks = ARRAY_SIZE(gen7_0_0_shader_blocks),
	.clusters = gen7_0_0_clusters,
	.num_clusters = ARRAY_SIZE(gen7_0_0_clusters),
	.sptp_clusters = gen7_0_0_sptp_clusters,
	.num_sptp_clusters = ARRAY_SIZE(gen7_0_0_sptp_clusters),
	.post_crashdumper_regs = gen7_0_0_post_crashdumper_registers,
};

const struct gen7_snapshot_block_list gen7_3_0_snapshot_block_list = {
	.pre_crashdumper_regs = gen7_0_0_pre_crashdumper_registers,
	.cp_indexed_reg_list = gen7_3_0_cp_indexed_reg_list,
	.cp_indexed_reg_list_len = ARRAY_SIZE(gen7_3_0_cp_indexed_reg_list),
	.debugbus_blocks = gen7_3_0_debugbus_blocks,
	.debugbus_blocks_len = ARRAY_SIZE(gen7_3_0_debugbus_blocks),
	.gbif_debugbus_blocks = gen7_gbif_debugbus_blocks,
	.gbif_debugbus_blocks_len = ARRAY_SIZE(gen7_gbif_debugbus_blocks),
	.cx_debugbus_blocks = gen7_cx_dbgc_debugbus_blocks,
	.cx_debugbus_blocks_len = ARRAY_SIZE(gen7_cx_dbgc_debugbus_blocks),
	.external_core_regs = gen7_3_0_external_core_regs,
	.num_external_core_regs = ARRAY_SIZE(gen7_3_0_external_core_regs),
	.gmu_regs = gen7_3_0_gmu_registers,
	.gmu_gx_regs = gen7_3_0_gmu_gx_registers,
	.rscc_regs = gen7_0_0_rscc_registers,
	.reg_list = gen7_3_0_reg_list,
	.shader_blocks = gen7_3_0_shader_blocks,
	.num_shader_blocks = ARRAY_SIZE(gen7_3_0_shader_blocks),
	.clusters = gen7_3_0_clusters,
	.num_clusters = ARRAY_SIZE(gen7_3_0_clusters),
	.sptp_clusters = gen7_3_0_sptp_clusters,
	.num_sptp_clusters = ARRAY_SIZE(gen7_3_0_sptp_clusters),
	.post_crashdumper_regs = gen7_0_0_post_crashdumper_registers,
};

const struct gen7_snapshot_block_list gen7_6_0_snapshot_block_list = {
	.pre_crashdumper_regs = gen7_0_0_pre_crashdumper_registers,
	.cp_indexed_reg_list = gen7_6_0_cp_indexed_reg_list,
	.cp_indexed_reg_list_len = ARRAY_SIZE(gen7_6_0_cp_indexed_reg_list),
	.debugbus_blocks = gen7_6_0_debugbus_blocks,
	.debugbus_blocks_len = ARRAY_SIZE(gen7_6_0_debugbus_blocks),
	.gbif_debugbus_blocks = gen7_gbif_debugbus_blocks,
	.gbif_debugbus_blocks_len = ARRAY_SIZE(gen7_gbif_debugbus_blocks),
	.cx_debugbus_blocks = gen7_cx_dbgc_debugbus_blocks,
	.cx_debugbus_blocks_len = ARRAY_SIZE(gen7_cx_dbgc_debugbus_blocks),
	.external_core_regs = gen7_6_0_external_core_regs,
	.num_external_core_regs = ARRAY_SIZE(gen7_6_0_external_core_regs),
	.gmu_regs = gen7_6_0_gmu_registers,
	.gmu_gx_regs = gen7_6_0_gmu_gx_registers,
	.rscc_regs = gen7_6_0_rscc_registers,
	.reg_list = gen7_6_0_reg_list,
	.shader_blocks = gen7_6_0_shader_blocks,
	.num_shader_blocks = ARRAY_SIZE(gen7_6_0_shader_blocks),
	.clusters = gen7_6_0_clusters,
	.num_clusters = ARRAY_SIZE(gen7_6_0_clusters),
	.sptp_clusters = gen7_6_0_sptp_clusters,
	.num_sptp_clusters = ARRAY_SIZE(gen7_6_0_sptp_clusters),
	.post_crashdumper_regs = gen7_0_0_post_crashdumper_registers,
};

#define GEN7_DEBUGBUS_BLOCK_SIZE 0x100

#define GEN7_SP_READ_SEL_VAL(_location, _pipe, _statetype, _usptp, _sptp) \
				(FIELD_PREP(GENMASK(19, 18), _location) | \
				 FIELD_PREP(GENMASK(17, 16), _pipe) | \
				 FIELD_PREP(GENMASK(15, 8), _statetype) | \
				 FIELD_PREP(GENMASK(7, 4), _usptp) | \
				 FIELD_PREP(GENMASK(3, 0), _sptp))

#define GEN7_CP_APERTURE_REG_VAL(_pipe, _cluster, _context) \
			(FIELD_PREP(GENMASK(13, 12), _pipe) | \
			 FIELD_PREP(GENMASK(10, 8), _cluster) | \
			 FIELD_PREP(GENMASK(5, 4), _context))

#define GEN7_DEBUGBUS_SECTION_SIZE (sizeof(struct kgsl_snapshot_debugbus) \
			+ (GEN7_DEBUGBUS_BLOCK_SIZE << 3))

#define CD_REG_END 0xaaaaaaaa

static int CD_WRITE(u64 *ptr, u32 offset, u64 val)
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
	gen7_cd_reg_end = gen7_crashdump_registers->hostptr + offset;
	*gen7_cd_reg_end = CD_REG_END;
	ptr[0] = gen7_crashdump_registers->gpuaddr + offset;
	ptr[1] = FIELD_PREP(GENMASK(63, 44), GEN7_CP_CRASH_DUMP_STATUS) | BIT(0);
	ptr[2] = 0;
	ptr[3] = 0;
}

static bool CD_SCRIPT_CHECK(struct kgsl_device *device)
{
	return (gen7_is_smmu_stalled(device) || (!device->snapshot_crashdumper) ||
		IS_ERR_OR_NULL(gen7_capturescript) ||
		IS_ERR_OR_NULL(gen7_crashdump_registers) ||
		gen7_crashdump_timedout);
}

static bool _gen7_do_crashdump(struct kgsl_device *device)
{
	unsigned int reg = 0;
	ktime_t timeout;

	kgsl_regwrite(device, GEN7_CP_CRASH_SCRIPT_BASE_LO,
			lower_32_bits(gen7_capturescript->gpuaddr));
	kgsl_regwrite(device, GEN7_CP_CRASH_SCRIPT_BASE_HI,
			upper_32_bits(gen7_capturescript->gpuaddr));
	kgsl_regwrite(device, GEN7_CP_CRASH_DUMP_CNTL, 1);

	timeout = ktime_add_ms(ktime_get(), CP_CRASH_DUMPER_TIMEOUT);

	if (!device->snapshot_atomic)
		might_sleep();
	for (;;) {
		/* make sure we're reading the latest value */
		rmb();
		if ((*gen7_cd_reg_end) != CD_REG_END)
			break;
		if (ktime_compare(ktime_get(), timeout) > 0)
			break;
		/* Wait 1msec to avoid unnecessary looping */
		if (!device->snapshot_atomic)
			usleep_range(100, 1000);
	}

	kgsl_regread(device, GEN7_CP_CRASH_DUMP_STATUS, &reg);

	/*
	 * Writing to the GEN7_CP_CRASH_DUMP_CNTL also resets the
	 * GEN7_CP_CRASH_DUMP_STATUS. Make sure the read above is
	 * complete before we change the value
	 */
	rmb();

	kgsl_regwrite(device, GEN7_CP_CRASH_DUMP_CNTL, 0);

	if (WARN(!(reg & 0x2), "Crashdumper timed out\n")) {
		/*
		 * Gen7 crash dumper script is broken down into multiple chunks
		 * and script will be invoked multiple times to capture snapshot
		 * of different sections of GPU. If crashdumper fails once, it is
		 * highly likely it will fail subsequently as well. Hence update
		 * gen7_crashdump_timedout variable to avoid running crashdumper
		 * after it fails once.
		 */
		gen7_crashdump_timedout = true;
		return false;
	}

	return true;
}

static size_t gen7_legacy_snapshot_registers(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct reg_list *regs = priv;

	if (regs->sel)
		kgsl_regwrite(device, regs->sel->host_reg, regs->sel->val);

	return adreno_snapshot_registers_v2(device, buf, remain, (void *)regs->regs);
}

static size_t gen7_snapshot_registers(struct kgsl_device *device, u8 *buf,
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

	src = gen7_crashdump_registers->hostptr + regs->offset;

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

static size_t gen7_legacy_snapshot_shader(struct kgsl_device *device,
				u8 *buf, size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_shader_v2 *header =
		(struct kgsl_snapshot_shader_v2 *) buf;
	struct gen7_shader_block_info *info = (struct gen7_shader_block_info *) priv;
	struct gen7_shader_block *block = info->block;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int read_sel;
	int i;

	if (remain < (sizeof(*header) + (block->size << 2))) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	/*
	 * If crashdumper times out, accessing some readback states from
	 * AHB path might fail. Hence, skip SP_INST_TAG and SP_INST_DATA*
	 * state types during snapshot dump in legacy flow.
	 */
	if (adreno_is_gen7_0_x_family(adreno_dev)) {
		if (block->statetype == SP_INST_TAG ||
			block->statetype == SP_INST_DATA ||
			block->statetype == SP_INST_DATA_1 ||
			block->statetype == SP_INST_DATA_2)
			return 0;
	}

	header->type = block->statetype;
	header->index = info->sp_id;
	header->size = block->size;
	header->usptp = info->usptp;
	header->location = block->location;
	header->pipe_id = block->pipeid;

	read_sel = GEN7_SP_READ_SEL_VAL(block->location, block->pipeid,
				block->statetype, info->usptp, info->sp_id);

	kgsl_regwrite(device, GEN7_SP_READ_SEL, read_sel);

	/*
	 * An explicit barrier is needed so that reads do not happen before
	 * the register write.
	 */
	mb();

	for (i = 0; i < block->size; i++)
		data[i] = kgsl_regmap_read(&device->regmap, GEN7_SP_AHB_READ_APERTURE + i);

	return (sizeof(*header) + (block->size << 2));
}

static size_t gen7_snapshot_shader_memory(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_shader_v2 *header =
		(struct kgsl_snapshot_shader_v2 *) buf;
	struct gen7_shader_block_info *info = (struct gen7_shader_block_info *) priv;
	struct gen7_shader_block *block = info->block;
	unsigned int *data = (unsigned int *) (buf + sizeof(*header));

	if (remain < (sizeof(*header) + (block->size << 2))) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	header->type = block->statetype;
	header->index = info->sp_id;
	header->size = block->size;
	header->usptp = info->usptp;
	header->location = block->location;
	header->pipe_id = block->pipeid;

	memcpy(data, gen7_crashdump_registers->hostptr + info->offset,
			(block->size << 2));

	return (sizeof(*header) + (block->size << 2));
}

static void gen7_snapshot_shader(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	unsigned int i;
	struct gen7_shader_block_info info;
	u64 *ptr;
	u32 offset = 0;
	struct gen7_shader_block *shader_blocks = gen7_snapshot_block_list->shader_blocks;
	size_t num_shader_blocks = gen7_snapshot_block_list->num_shader_blocks;
	unsigned int sp;
	unsigned int usptp;
	size_t (*func)(struct kgsl_device *device, u8 *buf, size_t remain,
		void *priv) = gen7_legacy_snapshot_shader;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (adreno_is_gen7_0_x_family(adreno_dev))
		kgsl_regrmw(device, GEN7_SP_DBG_CNTL, GENMASK(1, 0), BIT(0) | BIT(1));

	if (CD_SCRIPT_CHECK(device)) {
		for (i = 0; i < num_shader_blocks; i++) {
			struct gen7_shader_block *block = &shader_blocks[i];

			for (sp = 0; sp < block->num_sps; sp++) {
				for (usptp = 0; usptp < block->num_usptps; usptp++) {
					info.block = block;
					info.sp_id = sp;
					info.usptp = usptp;
					info.offset = offset;
					offset += block->size << 2;

					/* Shader working/shadow memory */
					kgsl_snapshot_add_section(device,
						KGSL_SNAPSHOT_SECTION_SHADER_V2,
						snapshot, func, &info);
				}
			}
		}

		goto done;
	}

	for (i = 0; i < num_shader_blocks; i++) {
		struct gen7_shader_block *block = &shader_blocks[i];

		/* Build the crash script */
		ptr = gen7_capturescript->hostptr;
		offset = 0;

		for (sp = 0; sp < block->num_sps; sp++) {
			for (usptp = 0; usptp < block->num_usptps; usptp++) {
				/* Program the aperture */
				ptr += CD_WRITE(ptr, GEN7_SP_READ_SEL,
					GEN7_SP_READ_SEL_VAL(block->location, block->pipeid,
						block->statetype, usptp, sp));

				/* Read all the data in one chunk */
				ptr += CD_READ(ptr, GEN7_SP_AHB_READ_APERTURE, block->size,
					gen7_crashdump_registers->gpuaddr + offset);
				offset += block->size << 2;
			}
		}
		/* Marker for end of script */
		CD_FINISH(ptr, offset);

		/* Try to run the crash dumper */
		func = gen7_legacy_snapshot_shader;
		if (_gen7_do_crashdump(device))
			func = gen7_snapshot_shader_memory;

		offset = 0;
		for (sp = 0; sp < block->num_sps; sp++) {
			for (usptp = 0; usptp < block->num_usptps; usptp++) {
				info.block = block;
				info.sp_id = sp;
				info.usptp = usptp;
				info.offset = offset;
				offset += block->size << 2;

				/* Shader working/shadow memory */
				kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_SHADER_V2,
					snapshot, func, &info);
			}
		}
	}

done:
	if (adreno_is_gen7_0_x_family(adreno_dev))
		kgsl_regrmw(device, GEN7_SP_DBG_CNTL, GENMASK(1, 0), 0x0);
}

static void gen7_snapshot_mempool(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	/* set CP_CHICKEN_DBG[StabilizeMVC] to stabilize it while dumping */
	kgsl_regrmw(device, GEN7_CP_CHICKEN_DBG, 0x4, 0x4);

	kgsl_snapshot_indexed_registers(device, snapshot,
		GEN7_CP_MEM_POOL_DBG_ADDR, GEN7_CP_MEM_POOL_DBG_DATA,
		0, 0x2100);

	if (!adreno_is_gen7_3_0(ADRENO_DEVICE(device))) {
		kgsl_regrmw(device, GEN7_CP_BV_CHICKEN_DBG, 0x4, 0x4);
		kgsl_snapshot_indexed_registers(device, snapshot,
			GEN7_CP_BV_MEM_POOL_DBG_ADDR, GEN7_CP_BV_MEM_POOL_DBG_DATA,
			0, 0x2100);
		kgsl_regrmw(device, GEN7_CP_BV_CHICKEN_DBG, 0x4, 0x0);
	}

	kgsl_regrmw(device, GEN7_CP_CHICKEN_DBG, 0x4, 0x0);
}

static unsigned int gen7_read_dbgahb(struct kgsl_device *device,
				unsigned int regbase, unsigned int reg)
{
	unsigned int val;

	kgsl_regread(device, (GEN7_SP_AHB_READ_APERTURE + reg - regbase), &val);
	return val;
}

static size_t gen7_legacy_snapshot_cluster_dbgahb(struct kgsl_device *device,
				u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs_v2 *header =
				(struct kgsl_snapshot_mvc_regs_v2 *)buf;
	struct gen7_sptp_cluster_registers *cluster =
			(struct gen7_sptp_cluster_registers *)priv;
	const u32 *ptr = cluster->regs;
	unsigned int read_sel;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int j;
	unsigned int size = adreno_snapshot_regs_count(ptr) * 4;

	if (remain < (sizeof(*header) + size)) {
		SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
		return 0;
	}

	header->ctxt_id = cluster->context_id;
	header->cluster_id = cluster->cluster_id;
	header->pipe_id = cluster->pipe_id;
	header->location_id = cluster->location_id;

	read_sel = GEN7_SP_READ_SEL_VAL(cluster->location_id, cluster->pipe_id,
			cluster->statetype, 0, 0);

	kgsl_regwrite(device, GEN7_SP_READ_SEL, read_sel);

	for (ptr = cluster->regs; ptr[0] != UINT_MAX; ptr += 2) {
		unsigned int count = REG_COUNT(ptr);

		if (count == 1)
			*data++ = ptr[0];
		else {
			*data++ = ptr[0] | (1 << 31);
			*data++ = ptr[1];
		}
		for (j = ptr[0]; j <= ptr[1]; j++)
			*data++ = gen7_read_dbgahb(device, cluster->regbase, j);
	}

	return (size + sizeof(*header));
}

static size_t gen7_snapshot_cluster_dbgahb(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs_v2 *header =
				(struct kgsl_snapshot_mvc_regs_v2 *)buf;
	struct gen7_sptp_cluster_registers *cluster =
				(struct gen7_sptp_cluster_registers *)priv;
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

	src = gen7_crashdump_registers->hostptr + cluster->offset;

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

static void gen7_snapshot_dbgahb_regs(struct kgsl_device *device,
			struct kgsl_snapshot *snapshot)
{
	int i;
	u64 *ptr, offset = 0;
	unsigned int count;
	struct gen7_sptp_cluster_registers *sptp_clusters = gen7_snapshot_block_list->sptp_clusters;
	size_t num_sptp_clusters = gen7_snapshot_block_list->num_sptp_clusters;
	size_t (*func)(struct kgsl_device *device, u8 *buf, size_t remain,
		void *priv) = gen7_legacy_snapshot_cluster_dbgahb;

	if (CD_SCRIPT_CHECK(device)) {
		for (i = 0; i < num_sptp_clusters; i++)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_MVC_V2, snapshot, func,
				&sptp_clusters[i]);
		return;
	}

	/* Build the crash script */
	ptr = gen7_capturescript->hostptr;

	for (i = 0; i < num_sptp_clusters; i++) {
		struct gen7_sptp_cluster_registers *cluster = &sptp_clusters[i];
		const u32 *regs = cluster->regs;

		cluster->offset = offset;

		/* Program the aperture */
		ptr += CD_WRITE(ptr, GEN7_SP_READ_SEL, GEN7_SP_READ_SEL_VAL
			(cluster->location_id, cluster->pipe_id, cluster->statetype, 0, 0));

		for (; regs[0] != UINT_MAX; regs += 2) {
			count = REG_COUNT(regs);
			ptr += CD_READ(ptr, (GEN7_SP_AHB_READ_APERTURE +
				regs[0] - cluster->regbase), count,
				(gen7_crashdump_registers->gpuaddr + offset));

			offset += count * sizeof(unsigned int);
		}
	}
	/* Marker for end of script */
	CD_FINISH(ptr, offset);

	/* Try to run the crash dumper */
	if (_gen7_do_crashdump(device))
		func = gen7_snapshot_cluster_dbgahb;

	/* Capture the registers in snapshot */
	for (i = 0; i < num_sptp_clusters; i++)
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_MVC_V2, snapshot, func, &sptp_clusters[i]);
}

static size_t gen7_legacy_snapshot_mvc(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs_v2 *header =
					(struct kgsl_snapshot_mvc_regs_v2 *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	struct gen7_cluster_registers *cluster =
			(struct gen7_cluster_registers *)priv;
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
	kgsl_regwrite(device, GEN7_CP_APERTURE_CNTL_HOST, GEN7_CP_APERTURE_REG_VAL
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

static size_t gen7_snapshot_mvc(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs_v2 *header =
				(struct kgsl_snapshot_mvc_regs_v2 *)buf;
	struct gen7_cluster_registers *cluster =
			(struct gen7_cluster_registers *)priv;
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

	src = gen7_crashdump_registers->hostptr + cluster->offset;

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

static void gen7_snapshot_mvc_regs(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	int i;
	u64 *ptr, offset = 0;
	unsigned int count;
	struct gen7_cluster_registers *clusters = gen7_snapshot_block_list->clusters;
	size_t num_clusters = gen7_snapshot_block_list->num_clusters;
	size_t (*func)(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv) = gen7_legacy_snapshot_mvc;

	if (CD_SCRIPT_CHECK(device)) {
		for (i = 0; i < num_clusters; i++)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_MVC_V2, snapshot, func, &clusters[i]);
		return;
	}

	/* Build the crash script */
	ptr = gen7_capturescript->hostptr;

	for (i = 0; i < num_clusters; i++) {
		struct gen7_cluster_registers *cluster = &clusters[i];
		const u32 *regs = cluster->regs;

		cluster->offset = offset;
		ptr += CD_WRITE(ptr, GEN7_CP_APERTURE_CNTL_CD, GEN7_CP_APERTURE_REG_VAL
			(cluster->pipe_id, cluster->cluster_id, cluster->context_id));

		if (cluster->sel)
			ptr += CD_WRITE(ptr, cluster->sel->cd_reg, cluster->sel->val);

		for (; regs[0] != UINT_MAX; regs += 2) {
			count = REG_COUNT(regs);

			ptr += CD_READ(ptr, regs[0],
				count, (gen7_crashdump_registers->gpuaddr + offset));

			offset += count * sizeof(unsigned int);
		}
	}

	/* Marker for end of script */
	CD_FINISH(ptr, offset);

	/* Try to run the crash dumper */
	if (_gen7_do_crashdump(device))
		func = gen7_snapshot_mvc;

	for (i = 0; i < num_clusters; i++)
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_MVC_V2, snapshot, func, &clusters[i]);
}

/* gen7_dbgc_debug_bus_read() - Read data from trace bus */
static void gen7_dbgc_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int reg;

	reg = FIELD_PREP(GENMASK(7, 0), index) |
		FIELD_PREP(GENMASK(24, 16), block_id);

	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_SEL_A, reg);
	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_SEL_B, reg);
	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_SEL_C, reg);
	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_SEL_D, reg);

	/*
	 * There needs to be a delay of 1 us to ensure enough time for correct
	 * data is funneled into the trace buffer
	 */
	udelay(1);

	kgsl_regread(device, GEN7_DBGC_CFG_DBGBUS_TRACE_BUF2, val);
	val++;
	kgsl_regread(device, GEN7_DBGC_CFG_DBGBUS_TRACE_BUF1, val);
}

/* gen7_snapshot_dbgc_debugbus_block() - Capture debug data for a gpu block */
static size_t gen7_snapshot_dbgc_debugbus_block(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header =
		(struct kgsl_snapshot_debugbus *)buf;
	const u32 *block = priv;
	int i;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));

	if (remain < GEN7_DEBUGBUS_SECTION_SIZE) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = *block;
	header->count = GEN7_DEBUGBUS_BLOCK_SIZE * 2;

	for (i = 0; i < GEN7_DEBUGBUS_BLOCK_SIZE; i++)
		gen7_dbgc_debug_bus_read(device, *block, i, &data[i*2]);

	return GEN7_DEBUGBUS_SECTION_SIZE;
}

static u32 gen7_dbgc_side_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index)
{
	u32 val;
	unsigned int reg = FIELD_PREP(GENMASK(7, 0), index) |
			FIELD_PREP(GENMASK(24, 16), block_id);

	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_SEL_A, reg);
	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_SEL_B, reg);
	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_SEL_C, reg);
	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_SEL_D, reg);

	/*
	 * There needs to be a delay of 1 us to ensure enough time for correct
	 * data is funneled into the trace buffer
	 */
	udelay(1);

	val = kgsl_regmap_read(&device->regmap, GEN7_DBGC_CFG_DBGBUS_OVER);

	return FIELD_GET(GENMASK(27, 24), val);
}

static size_t gen7_snapshot_dbgc_side_debugbus_block(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_side_debugbus *header =
		(struct kgsl_snapshot_side_debugbus *)buf;
	const u32 *block = priv;
	int i;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	size_t size = (GEN7_DEBUGBUS_BLOCK_SIZE * sizeof(unsigned int)) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = *block;
	header->size = GEN7_DEBUGBUS_BLOCK_SIZE;
	header->valid_data = 0x4;

	for (i = 0; i < GEN7_DEBUGBUS_BLOCK_SIZE; i++)
		data[i] = gen7_dbgc_side_debug_bus_read(device, *block, i);

	return size;
}

/* gen7_cx_dbgc_debug_bus_read() - Read data from trace bus */
static void gen7_cx_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int reg;

	reg = FIELD_PREP(GENMASK(7, 0), index) |
		FIELD_PREP(GENMASK(24, 16), block_id);

	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_SEL_A, reg);
	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_SEL_B, reg);
	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_SEL_C, reg);
	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_SEL_D, reg);

	/*
	 * There needs to be a delay of 1 us to ensure enough time for correct
	 * data is funneled into the trace buffer
	 */
	udelay(1);

	adreno_cx_dbgc_regread(device, GEN7_CX_DBGC_CFG_DBGBUS_TRACE_BUF2, val);
	val++;
	adreno_cx_dbgc_regread(device, GEN7_CX_DBGC_CFG_DBGBUS_TRACE_BUF1, val);
}

/*
 * gen7_snapshot_cx_dbgc_debugbus_block() - Capture debug data for a gpu
 * block from the CX DBGC block
 */
static size_t gen7_snapshot_cx_dbgc_debugbus_block(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header =
		(struct kgsl_snapshot_debugbus *)buf;
	const u32 *block = priv;
	int i;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));

	if (remain < GEN7_DEBUGBUS_SECTION_SIZE) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = *block;
	header->count = GEN7_DEBUGBUS_BLOCK_SIZE * 2;

	for (i = 0; i < GEN7_DEBUGBUS_BLOCK_SIZE; i++)
		gen7_cx_debug_bus_read(device, *block, i, &data[i*2]);

	return GEN7_DEBUGBUS_SECTION_SIZE;
}

/* gen7_cx_side_dbgc_debug_bus_read() - Read data from trace bus */
static void gen7_cx_side_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int reg = FIELD_PREP(GENMASK(7, 0), index) |
			FIELD_PREP(GENMASK(24, 16), block_id);

	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_SEL_A, reg);
	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_SEL_B, reg);
	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_SEL_C, reg);
	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_SEL_D, reg);

	/*
	 * There needs to be a delay of 1 us to ensure enough time for correct
	 * data is funneled into the trace buffer
	 */
	udelay(1);

	adreno_cx_dbgc_regread(device, GEN7_CX_DBGC_CFG_DBGBUS_OVER, &reg);
	*val = FIELD_GET(GENMASK(27, 24), reg);
}

/*
 * gen7_snapshot_cx_dbgc_debugbus_block() - Capture debug data for a gpu
 * block from the CX DBGC block
 */
static size_t gen7_snapshot_cx_side_dbgc_debugbus_block(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_side_debugbus *header =
		(struct kgsl_snapshot_side_debugbus *)buf;
	const u32 *block = priv;
	int i;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	size_t size = (GEN7_DEBUGBUS_BLOCK_SIZE * sizeof(unsigned int)) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = *block;
	header->size = GEN7_DEBUGBUS_BLOCK_SIZE;
	header->valid_data = 0x4;

	for (i = 0; i < GEN7_DEBUGBUS_BLOCK_SIZE; i++)
		gen7_cx_side_debug_bus_read(device, *block, i, &data[i]);

	return size;
}

/* gen7_snapshot_debugbus() - Capture debug bus data */
static void gen7_snapshot_debugbus(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	int i;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_CNTLT,
			FIELD_PREP(GENMASK(31, 28), 0xf));

	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_CNTLM,
			FIELD_PREP(GENMASK(27, 24), 0xf));

	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_IVTL_0, 0);
	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_IVTL_1, 0);
	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_IVTL_2, 0);
	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_IVTL_3, 0);

	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_BYTEL_0,
			FIELD_PREP(GENMASK(3, 0), 0x0) |
			FIELD_PREP(GENMASK(7, 4), 0x1) |
			FIELD_PREP(GENMASK(11, 8), 0x2) |
			FIELD_PREP(GENMASK(15, 12), 0x3) |
			FIELD_PREP(GENMASK(19, 16), 0x4) |
			FIELD_PREP(GENMASK(23, 20), 0x5) |
			FIELD_PREP(GENMASK(27, 24), 0x6) |
			FIELD_PREP(GENMASK(31, 28), 0x7));
	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_BYTEL_1,
			FIELD_PREP(GENMASK(3, 0), 0x8) |
			FIELD_PREP(GENMASK(7, 4), 0x9) |
			FIELD_PREP(GENMASK(11, 8), 0xa) |
			FIELD_PREP(GENMASK(15, 12), 0xb) |
			FIELD_PREP(GENMASK(19, 16), 0xc) |
			FIELD_PREP(GENMASK(23, 20), 0xd) |
			FIELD_PREP(GENMASK(27, 24), 0xe) |
			FIELD_PREP(GENMASK(31, 28), 0xf));

	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_MASKL_0, 0);
	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_MASKL_1, 0);
	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_MASKL_2, 0);
	kgsl_regwrite(device, GEN7_DBGC_CFG_DBGBUS_MASKL_3, 0);

	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_CNTLT,
			FIELD_PREP(GENMASK(31, 28), 0xf));

	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_CNTLM,
			FIELD_PREP(GENMASK(27, 24), 0xf));

	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_IVTL_0, 0);
	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_IVTL_1, 0);
	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_IVTL_2, 0);
	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_IVTL_3, 0);

	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_BYTEL_0,
			FIELD_PREP(GENMASK(3, 0), 0x0) |
			FIELD_PREP(GENMASK(7, 4), 0x1) |
			FIELD_PREP(GENMASK(11, 8), 0x2) |
			FIELD_PREP(GENMASK(15, 12), 0x3) |
			FIELD_PREP(GENMASK(19, 16), 0x4) |
			FIELD_PREP(GENMASK(23, 20), 0x5) |
			FIELD_PREP(GENMASK(27, 24), 0x6) |
			FIELD_PREP(GENMASK(31, 28), 0x7));
	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_BYTEL_1,
			FIELD_PREP(GENMASK(3, 0), 0x8) |
			FIELD_PREP(GENMASK(7, 4), 0x9) |
			FIELD_PREP(GENMASK(11, 8), 0xa) |
			FIELD_PREP(GENMASK(15, 12), 0xb) |
			FIELD_PREP(GENMASK(19, 16), 0xc) |
			FIELD_PREP(GENMASK(23, 20), 0xd) |
			FIELD_PREP(GENMASK(27, 24), 0xe) |
			FIELD_PREP(GENMASK(31, 28), 0xf));

	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_MASKL_0, 0);
	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_MASKL_1, 0);
	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_MASKL_2, 0);
	adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_CFG_DBGBUS_MASKL_3, 0);

	for (i = 0; i < gen7_snapshot_block_list->debugbus_blocks_len; i++) {
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS,
			snapshot, gen7_snapshot_dbgc_debugbus_block,
			(void *) &gen7_snapshot_block_list->debugbus_blocks[i]);
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_SIDE_DEBUGBUS,
			snapshot, gen7_snapshot_dbgc_side_debugbus_block,
			(void *) &gen7_snapshot_block_list->debugbus_blocks[i]);
	}

	for (i = 0; i < gen7_snapshot_block_list->gbif_debugbus_blocks_len; i++) {
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS,
			snapshot, gen7_snapshot_dbgc_debugbus_block,
			(void *) &gen7_snapshot_block_list->gbif_debugbus_blocks[i]);
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_SIDE_DEBUGBUS,
			snapshot, gen7_snapshot_dbgc_side_debugbus_block,
			(void *) &gen7_snapshot_block_list->gbif_debugbus_blocks[i]);
	}

	/* Dump the CX debugbus data if the block exists */
	if (adreno_is_cx_dbgc_register(device, GEN7_CX_DBGC_CFG_DBGBUS_SEL_A)) {
		for (i = 0; i < gen7_snapshot_block_list->cx_debugbus_blocks_len; i++) {
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot, gen7_snapshot_cx_dbgc_debugbus_block,
				(void *) &gen7_snapshot_block_list->cx_debugbus_blocks[i]);
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_SIDE_DEBUGBUS,
				snapshot, gen7_snapshot_cx_side_dbgc_debugbus_block,
				(void *) &gen7_snapshot_block_list->cx_debugbus_blocks[i]);
		}
	}
}

/* gen7_snapshot_sqe() - Dump SQE data in snapshot */
static size_t gen7_snapshot_sqe(struct kgsl_device *device, u8 *buf,
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
	const struct adreno_gen7_core *gpucore = to_gen7_core(ADRENO_DEVICE(device));
	u64 ctxt_record_size = GEN7_CP_CTXRECORD_SIZE_IN_BYTES;

	if (gpucore->ctxt_record_size)
		ctxt_record_size = gpucore->ctxt_record_size;

	ctxt_record_size = min_t(u64, ctxt_record_size, device->snapshot_ctxt_record_size);

	if (remain < (ctxt_record_size + sizeof(*header))) {
		SNAPSHOT_ERR_NOMEM(device, "PREEMPTION RECORD");
		return 0;
	}

	header->size = ctxt_record_size >> 2;
	header->gpuaddr = memdesc->gpuaddr;
	header->ptbase =
		kgsl_mmu_pagetable_get_ttbr0(device->mmu.defaultpagetable);
	header->type = SNAPSHOT_GPU_OBJECT_GLOBAL;

	memcpy(ptr, memdesc->hostptr, ctxt_record_size);

	return ctxt_record_size + sizeof(*header);
}

static void gen7_reglist_snapshot(struct kgsl_device *device,
					struct kgsl_snapshot *snapshot)
{
	u64 *ptr, offset = 0;
	int i;
	u32 r;
	struct reg_list *reg_list = gen7_snapshot_block_list->reg_list;
	size_t (*func)(struct kgsl_device *device, u8 *buf, size_t remain,
		void *priv) = gen7_legacy_snapshot_registers;

	if (CD_SCRIPT_CHECK(device)) {
		for (i = 0; reg_list[i].regs; i++)
			kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2,
				snapshot, func, &reg_list[i]);
		return;
	}

	/* Build the crash script */
	ptr = (u64 *)gen7_capturescript->hostptr;

	for (i = 0; reg_list[i].regs; i++) {
		struct reg_list *regs = &reg_list[i];
		const u32 *regs_ptr = regs->regs;

		regs->offset = offset;

		/* Program the SEL_CNTL_CD register appropriately */
		if (regs->sel)
			ptr += CD_WRITE(ptr, regs->sel->cd_reg, regs->sel->val);

		for (; regs_ptr[0] != UINT_MAX; regs_ptr += 2) {
			r = REG_COUNT(regs_ptr);
			ptr += CD_READ(ptr, regs_ptr[0], r,
				(gen7_crashdump_registers->gpuaddr + offset));
			offset += r * sizeof(u32);
		}
	}

	/* Marker for end of script */
	CD_FINISH(ptr, offset);

	/* Try to run the crash dumper */
	if (_gen7_do_crashdump(device))
		func = gen7_snapshot_registers;

	for (i = 0; reg_list[i].regs; i++)
		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2,
			snapshot, func, &reg_list[i]);
}

static void gen7_snapshot_br_roq(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	unsigned int roq_size;

	/*
	 * CP ROQ dump units is 4 dwords. The number of units is stored
	 * in CP_ROQ_THRESHOLDS_2[31:20], but it is not accessible to
	 * host. Program the GEN7_CP_SQE_UCODE_DBG_ADDR with 0x70d3 offset
	 * and read the value CP_ROQ_THRESHOLDS_2 from
	 * GEN7_CP_SQE_UCODE_DBG_DATA
	 */
	kgsl_regwrite(device, GEN7_CP_SQE_UCODE_DBG_ADDR, 0x70d3);
	kgsl_regread(device, GEN7_CP_SQE_UCODE_DBG_DATA, &roq_size);
	roq_size = roq_size >> 20;
	kgsl_snapshot_indexed_registers(device, snapshot,
			GEN7_CP_ROQ_DBG_ADDR, GEN7_CP_ROQ_DBG_DATA, 0, (roq_size << 2));
}

static void gen7_snapshot_bv_roq(struct kgsl_device *device,
			struct kgsl_snapshot *snapshot)
{
	unsigned int roq_size;

	/*
	 * CP ROQ dump units is 4 dwords. The number of units is stored
	 * in CP_BV_ROQ_THRESHOLDS_2[31:20], but it is not accessible to
	 * host. Program the GEN7_CP_BV_SQE_UCODE_DBG_ADDR with 0x70d3 offset
	 * (at which CP stores the roq values) and read the value of
	 * CP_BV_ROQ_THRESHOLDS_2 from GEN7_CP_BV_SQE_UCODE_DBG_DATA
	 */
	kgsl_regwrite(device, GEN7_CP_BV_SQE_UCODE_DBG_ADDR, 0x70d3);
	kgsl_regread(device, GEN7_CP_BV_SQE_UCODE_DBG_DATA, &roq_size);
	roq_size = roq_size >> 20;
	kgsl_snapshot_indexed_registers(device, snapshot,
			GEN7_CP_BV_ROQ_DBG_ADDR, GEN7_CP_BV_ROQ_DBG_DATA, 0, (roq_size << 2));
}

static void gen7_snapshot_lpac_roq(struct kgsl_device *device,
			struct kgsl_snapshot *snapshot)
{
	unsigned int roq_size;

	/*
	 * CP ROQ dump units is 4 dwords. The number of units is stored
	 * in CP_LPAC_ROQ_THRESHOLDS_2[31:20], but it is not accessible to
	 * host. Program the GEN7_CP_SQE_AC_UCODE_DBG_ADDR with 0x70d3 offset
	 * (at which CP stores the roq values) and read the value of
	 * CP_LPAC_ROQ_THRESHOLDS_2 from GEN7_CP_SQE_AC_UCODE_DBG_DATA
	 */
	kgsl_regwrite(device, GEN7_CP_SQE_AC_UCODE_DBG_ADDR, 0x70d3);
	kgsl_regread(device, GEN7_CP_SQE_AC_UCODE_DBG_DATA, &roq_size);
	roq_size = roq_size >> 20;
	kgsl_snapshot_indexed_registers(device, snapshot,
			GEN7_CP_LPAC_ROQ_DBG_ADDR, GEN7_CP_LPAC_ROQ_DBG_DATA, 0, (roq_size << 2));
}

void gen7_snapshot_external_core_regs(struct kgsl_device *device,
			struct kgsl_snapshot *snapshot)
{
	size_t i;
	const u32 **external_core_regs;
	unsigned int num_external_core_regs;
	const struct adreno_gen7_core *gpucore = to_gen7_core(ADRENO_DEVICE(device));

	gen7_snapshot_block_list = gpucore->gen7_snapshot_block_list;
	external_core_regs = gen7_snapshot_block_list->external_core_regs;
	num_external_core_regs = gen7_snapshot_block_list->num_external_core_regs;

	for (i = 0; i < num_external_core_regs; i++) {
		const u32 *regs = external_core_regs[i];

		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2,
			snapshot, adreno_snapshot_registers_v2,
			(void *) regs);
	}
}

/*
 * gen7_snapshot() - GEN7 GPU snapshot function
 * @adreno_dev: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the GEN7 specific bits and pieces are grabbed
 * into the snapshot memory
 */
void gen7_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	const struct adreno_gen7_core *gpucore = to_gen7_core(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct cp_indexed_reg_list *cp_indexed_reglist;
	struct adreno_ringbuffer *rb;
	size_t cp_indexed_reglist_len;
	unsigned int i;
	u32 hi, lo, cgc = 0, cgc1 = 0, cgc2 = 0;
	int is_current_rt;

	gen7_crashdump_timedout = false;
	gen7_snapshot_block_list = gpucore->gen7_snapshot_block_list;
	cp_indexed_reglist = gen7_snapshot_block_list->cp_indexed_reg_list;
	cp_indexed_reglist_len = gen7_snapshot_block_list->cp_indexed_reg_list_len;

	/* External registers are dumped in the beginning of gmu snapshot */
	if (!gmu_core_isenabled(device))
		gen7_snapshot_external_core_regs(device, snapshot);

	/*
	 * Dump debugbus data here to capture it for both
	 * GMU and GPU snapshot. Debugbus data can be accessed
	 * even if the gx headswitch is off. If gx
	 * headswitch is off, data for gx blocks will show as
	 * 0x5c00bd00. Disable clock gating for SP and TP to capture
	 * debugbus data.
	 */
	if (device->ftbl->is_hwcg_on(device)) {
		kgsl_regread(device, GEN7_RBBM_CLOCK_CNTL2_SP0, &cgc);
		kgsl_regread(device, GEN7_RBBM_CLOCK_CNTL_TP0, &cgc1);
		kgsl_regread(device, GEN7_RBBM_CLOCK_CNTL3_TP0, &cgc2);
		kgsl_regrmw(device, GEN7_RBBM_CLOCK_CNTL2_SP0, GENMASK(22, 20), 0);
		kgsl_regrmw(device, GEN7_RBBM_CLOCK_CNTL_TP0, GENMASK(2, 0), 0);
		kgsl_regrmw(device, GEN7_RBBM_CLOCK_CNTL3_TP0, GENMASK(14, 12), 0);
	}

	gen7_snapshot_debugbus(adreno_dev, snapshot);

	/* Restore the value of the clockgating registers */
	if (device->ftbl->is_hwcg_on(device)) {
		kgsl_regwrite(device, GEN7_RBBM_CLOCK_CNTL2_SP0, cgc);
		kgsl_regwrite(device, GEN7_RBBM_CLOCK_CNTL_TP0, cgc1);
		kgsl_regwrite(device, GEN7_RBBM_CLOCK_CNTL3_TP0, cgc2);
	}

	if (!adreno_gx_is_on(adreno_dev)) {
		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2,
			snapshot, adreno_snapshot_registers_v2,
			(void *)gen7_0_0_cx_misc_registers);
		return;
	}

	is_current_rt = rt_task(current);

	if (is_current_rt)
		sched_set_normal(current, 0);

	kgsl_regread(device, GEN7_CP_IB1_BASE, &lo);
	kgsl_regread(device, GEN7_CP_IB1_BASE_HI, &hi);

	snapshot->ib1base = (((u64) hi) << 32) | lo;

	kgsl_regread(device, GEN7_CP_IB2_BASE, &lo);
	kgsl_regread(device, GEN7_CP_IB2_BASE_HI, &hi);

	snapshot->ib2base = (((u64) hi) << 32) | lo;

	kgsl_regread(device, GEN7_CP_IB1_REM_SIZE, &snapshot->ib1size);
	kgsl_regread(device, GEN7_CP_IB2_REM_SIZE, &snapshot->ib2size);

	kgsl_regread(device, GEN7_CP_LPAC_IB1_BASE, &lo);
	kgsl_regread(device, GEN7_CP_LPAC_IB1_BASE_HI, &hi);

	snapshot->ib1base_lpac = (((u64) hi) << 32) | lo;

	kgsl_regread(device, GEN7_CP_LPAC_IB2_BASE, &lo);
	kgsl_regread(device, GEN7_CP_LPAC_IB2_BASE_HI, &hi);

	snapshot->ib2base_lpac = (((u64) hi) << 32) | lo;

	kgsl_regread(device, GEN7_CP_LPAC_IB1_REM_SIZE, &snapshot->ib1size_lpac);
	kgsl_regread(device, GEN7_CP_LPAC_IB2_REM_SIZE, &snapshot->ib2size_lpac);

	/* Assert the isStatic bit before triggering snapshot */
	kgsl_regwrite(device, GEN7_RBBM_SNAPSHOT_STATUS, 0x1);

	/* Dump the registers which get affected by crash dumper trigger */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2,
		snapshot, adreno_snapshot_registers_v2,
		(void *) gen7_snapshot_block_list->pre_crashdumper_regs);

	gen7_reglist_snapshot(device, snapshot);

	/*
	 * Need to program and save this register before capturing resource table
	 * to workaround a CGC issue
	 */
	if (device->ftbl->is_hwcg_on(device)) {
		kgsl_regread(device, GEN7_RBBM_CLOCK_MODE_CP, &cgc);
		kgsl_regrmw(device, GEN7_RBBM_CLOCK_MODE_CP, 0x7, 0);
	}

	/* Reprogram the register back to the original stored value */
	if (device->ftbl->is_hwcg_on(device))
		kgsl_regwrite(device, GEN7_RBBM_CLOCK_MODE_CP, cgc);

	for (i = 0; i < cp_indexed_reglist_len; i++)
		kgsl_snapshot_indexed_registers(device, snapshot,
			cp_indexed_reglist[i].addr, cp_indexed_reglist[i].data, 0,
			cp_indexed_reglist[i].size);

	if (!adreno_is_gen7_3_0(adreno_dev)) {
		kgsl_snapshot_indexed_registers(device, snapshot,
			GEN7_CP_RESOURCE_TBL_DBG_ADDR, GEN7_CP_RESOURCE_TBL_DBG_DATA,
			0, 0x4100);
		gen7_snapshot_bv_roq(device, snapshot);
		gen7_snapshot_lpac_roq(device, snapshot);
	}

	gen7_snapshot_br_roq(device, snapshot);

	/* SQE Firmware */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, gen7_snapshot_sqe, NULL);

	/* Mempool debug data */
	gen7_snapshot_mempool(device, snapshot);

	/* Shader memory */
	gen7_snapshot_shader(device, snapshot);

	/* MVC register section */
	gen7_snapshot_mvc_regs(device, snapshot);

	/* registers dumped through DBG AHB */
	gen7_snapshot_dbgahb_regs(device, snapshot);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2,
		snapshot, adreno_snapshot_registers_v2,
		(void *) gen7_snapshot_block_list->post_crashdumper_regs);

	kgsl_regwrite(device, GEN7_RBBM_SNAPSHOT_STATUS, 0x0);

	/* Preemption record */
	if (adreno_is_preemption_enabled(adreno_dev)) {
		FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
				snapshot, snapshot_preemption_record,
				rb->preemption_desc);
		}
	}
	if (is_current_rt)
		sched_set_fifo(current);
}

void gen7_crashdump_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (IS_ERR_OR_NULL(gen7_capturescript))
		gen7_capturescript = kgsl_allocate_global(device,
			3 * PAGE_SIZE, 0, KGSL_MEMFLAGS_GPUREADONLY,
			KGSL_MEMDESC_PRIVILEGED, "capturescript");

	if (IS_ERR(gen7_capturescript))
		return;

	if (IS_ERR_OR_NULL(gen7_crashdump_registers))
		gen7_crashdump_registers = kgsl_allocate_global(device,
			25 * PAGE_SIZE, 0, 0, KGSL_MEMDESC_PRIVILEGED,
			"capturescript_regs");

	if (IS_ERR(gen7_crashdump_registers))
		return;
}
