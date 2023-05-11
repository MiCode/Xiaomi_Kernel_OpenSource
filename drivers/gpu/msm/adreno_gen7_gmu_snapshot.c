// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "gen7_reg.h"
#include "adreno.h"
#include "adreno_gen7.h"
#include "adreno_gen7_gmu.h"
#include "adreno_gen7_snapshot.h"
#include "adreno_snapshot.h"
#include "kgsl_device.h"

size_t gen7_snapshot_gmu_mem(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_gmu_mem *mem_hdr =
		(struct kgsl_snapshot_gmu_mem *)buf;
	unsigned int *data = (unsigned int *)
		(buf + sizeof(*mem_hdr));
	struct gmu_mem_type_desc *desc = priv;

	if (priv == NULL || desc->memdesc->hostptr == NULL)
		return 0;

	if (remain < desc->memdesc->size + sizeof(*mem_hdr)) {
		dev_err(device->dev,
			"snapshot: Not enough memory for the gmu section %d\n",
			desc->type);
		return 0;
	}

	mem_hdr->type = desc->type;
	mem_hdr->hostaddr = (u64)(uintptr_t)desc->memdesc->hostptr;
	mem_hdr->gmuaddr = desc->memdesc->gmuaddr;
	mem_hdr->gpuaddr = 0;

	memcpy(data, desc->memdesc->hostptr, desc->memdesc->size);

	return desc->memdesc->size + sizeof(*mem_hdr);
}

static size_t gen7_gmu_snapshot_dtcm(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_gmu_mem *mem_hdr =
		(struct kgsl_snapshot_gmu_mem *)buf;
	struct gen7_gmu_device *gmu = (struct gen7_gmu_device *)priv;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	u32 *data = (u32 *)(buf + sizeof(*mem_hdr));
	u32 i;

	if (remain < gmu->vma[GMU_DTCM].size + sizeof(*mem_hdr)) {
		SNAPSHOT_ERR_NOMEM(device, "GMU DTCM Memory");
		return 0;
	}

	mem_hdr->type = SNAPSHOT_GMU_MEM_BIN_BLOCK;
	mem_hdr->hostaddr = 0;
	mem_hdr->gmuaddr = gmu->vma[GMU_DTCM].start;
	mem_hdr->gpuaddr = 0;

	/*
	 * Read of GMU TCMs over side-band debug controller interface is
	 * supported on gen7_6_0
	 */
	if (adreno_is_gen7_6_0(adreno_dev)) {
		/*
		 * region [20]: Dump ITCM/DTCM. Select 1 for DTCM.
		 * autoInc [31]: Autoincrement the address field after each
		 * access to TCM_DBG_DATA
		 */
		adreno_cx_dbgc_regwrite(device, GEN7_CX_DBGC_TCM_DBG_ADDR,
					BIT(20) | BIT(31));

		for (i = 0; i < (gmu->vma[GMU_DTCM].size >> 2); i++)
			adreno_cx_dbgc_regread(device, GEN7_CX_DBGC_TCM_DBG_DATA, data++);
	} else {
		for (i = 0; i < (gmu->vma[GMU_DTCM].size >> 2); i++)
			gmu_core_regread(device, GEN7_GMU_CM3_DTCM_START + i, data++);
	}

	return gmu->vma[GMU_DTCM].size + sizeof(*mem_hdr);
}

static size_t gen7_gmu_snapshot_itcm(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_gmu_mem *mem_hdr =
			(struct kgsl_snapshot_gmu_mem *)buf;
	void *dest = buf + sizeof(*mem_hdr);
	struct gen7_gmu_device *gmu = (struct gen7_gmu_device *)priv;

	if (!gmu->itcm_shadow) {
		dev_err(&gmu->pdev->dev, "No memory allocated for ITCM shadow capture\n");
		return 0;
	}

	if (remain < gmu->vma[GMU_ITCM].size + sizeof(*mem_hdr)) {
		SNAPSHOT_ERR_NOMEM(device, "GMU ITCM Memory");
		return 0;
	}

	mem_hdr->type = SNAPSHOT_GMU_MEM_BIN_BLOCK;
	mem_hdr->hostaddr = 0;
	mem_hdr->gmuaddr = gmu->vma[GMU_ITCM].start;
	mem_hdr->gpuaddr = 0;

	memcpy(dest, gmu->itcm_shadow, gmu->vma[GMU_ITCM].size);

	return gmu->vma[GMU_ITCM].size + sizeof(*mem_hdr);
}

static void gen7_gmu_snapshot_memories(struct kgsl_device *device,
	struct gen7_gmu_device *gmu, struct kgsl_snapshot *snapshot)
{
	struct gmu_mem_type_desc desc;
	struct kgsl_memdesc *md;
	int i;

	for (i = 0; i < ARRAY_SIZE(gmu->gmu_globals); i++) {

		md = &gmu->gmu_globals[i];
		if (!md->size)
			continue;

		desc.memdesc = md;
		if (md == gmu->hfi.hfi_mem)
			desc.type = SNAPSHOT_GMU_MEM_HFI;
		else if (md == gmu->gmu_log)
			desc.type = SNAPSHOT_GMU_MEM_LOG;
		else if (md == gmu->dump_mem)
			desc.type = SNAPSHOT_GMU_MEM_DEBUG;
		else
			desc.type = SNAPSHOT_GMU_MEM_BIN_BLOCK;

		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
			snapshot, gen7_snapshot_gmu_mem, &desc);
	}
}

struct kgsl_snapshot_gmu_version {
	u32 type;
	u32 value;
};

static size_t gen7_snapshot_gmu_version(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	u32 *data = (u32 *) (buf + sizeof(*header));
	struct kgsl_snapshot_gmu_version *ver = priv;

	if (remain < DEBUG_SECTION_SZ(1)) {
		SNAPSHOT_ERR_NOMEM(device, "GMU Version");
		return 0;
	}

	header->type = ver->type;
	header->size = 1;

	*data = ver->value;

	return DEBUG_SECTION_SZ(1);
}

static void gen7_gmu_snapshot_versions(struct kgsl_device *device,
		struct gen7_gmu_device *gmu,
		struct kgsl_snapshot *snapshot)
{
	int i;

	struct kgsl_snapshot_gmu_version gmu_vers[] = {
		{ .type = SNAPSHOT_DEBUG_GMU_CORE_VERSION,
			.value = gmu->ver.core, },
		{ .type = SNAPSHOT_DEBUG_GMU_CORE_DEV_VERSION,
			.value = gmu->ver.core_dev, },
		{ .type = SNAPSHOT_DEBUG_GMU_PWR_VERSION,
			.value = gmu->ver.pwr, },
		{ .type = SNAPSHOT_DEBUG_GMU_PWR_DEV_VERSION,
			.value = gmu->ver.pwr_dev, },
		{ .type = SNAPSHOT_DEBUG_GMU_HFI_VERSION,
			.value = gmu->ver.hfi, },
	};

	for (i = 0; i < ARRAY_SIZE(gmu_vers); i++)
		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
				snapshot, gen7_snapshot_gmu_version,
				&gmu_vers[i]);
}

#define RSCC_OFFSET_DWORDS 0x14000

static size_t gen7_snapshot_rscc_registers(struct kgsl_device *device, u8 *buf,
	size_t remain, void *priv)
{
	const u32 *regs = priv;
	unsigned int *data = (unsigned int *)buf;
	int count = 0, k;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	/* Figure out how many registers we are going to dump */
	count = adreno_snapshot_regs_count(regs);

	if (remain < (count * 4)) {
		SNAPSHOT_ERR_NOMEM(device, "RSCC REGISTERS");
		return 0;
	}

	for (regs = priv; regs[0] != UINT_MAX; regs += 2) {
		unsigned int cnt = REG_COUNT(regs);

		if (cnt == 1) {
			*data++ = BIT(31) |  regs[0];
			*data++ =  __raw_readl(gmu->rscc_virt +
				((regs[0] - RSCC_OFFSET_DWORDS) << 2));
			continue;
		}
		*data++ = regs[0];
		*data++ = cnt;
		for (k = regs[0]; k <= regs[1]; k++)
			*data++ =  __raw_readl(gmu->rscc_virt +
				((k - RSCC_OFFSET_DWORDS) << 2));
	}

	/* Return the size of the section */
	return (count * 4);
}

/*
 * gen7_gmu_device_snapshot() - GEN7 GMU snapshot function
 * @device: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the GEN7 GMU specific bits and pieces are grabbed
 * into the snapshot memory
 */
static void gen7_gmu_device_snapshot(struct kgsl_device *device,
	struct kgsl_snapshot *snapshot)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	const struct adreno_gen7_core *gpucore = to_gen7_core(ADRENO_DEVICE(device));
	const struct gen7_snapshot_block_list *gen7_snapshot_block_list =
						gpucore->gen7_snapshot_block_list;

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
		snapshot, gen7_gmu_snapshot_itcm, gmu);

	gen7_gmu_snapshot_versions(device, gmu, snapshot);

	gen7_gmu_snapshot_memories(device, gmu, snapshot);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2, snapshot,
		adreno_snapshot_registers_v2, (void *) gen7_snapshot_block_list->gmu_regs);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2, snapshot,
		gen7_snapshot_rscc_registers, (void *) gen7_snapshot_block_list->rscc_regs);

	if (!gen7_gmu_gx_is_on(adreno_dev))
		goto dtcm;

	/* Set fence to ALLOW mode so registers can be read */
	kgsl_regwrite(device, GEN7_GMU_AO_AHB_FENCE_CTRL, 0);
	/* Make sure the previous write posted before reading */
	wmb();

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2, snapshot,
		adreno_snapshot_registers_v2, (void *) gen7_snapshot_block_list->gmu_gx_regs);

	/*
	 * A stalled SMMU can lead to NoC timeouts when host accesses DTCM.
	 * DTCM can be read through side-band DBGC interface on gen7_6_0.
	 */
	if (gen7_is_smmu_stalled(device) && !adreno_is_gen7_6_0(adreno_dev)) {
		dev_err(&gmu->pdev->dev,
			"Not dumping dtcm because SMMU is stalled\n");
		return;
	}

dtcm:
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
		snapshot, gen7_gmu_snapshot_dtcm, gmu);
}

void gen7_gmu_snapshot(struct adreno_device *adreno_dev,
	struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/*
	 * Dump external register first to have GPUCC and other external
	 * register in snapshot to analyze the system state even in partial
	 * snapshot dump
	 */
	gen7_snapshot_external_core_regs(device, snapshot);

	gen7_gmu_device_snapshot(device, snapshot);

	gen7_snapshot(adreno_dev, snapshot);

	gmu_core_regwrite(device, GEN7_GMU_GMU2HOST_INTR_CLR, UINT_MAX);
	gmu_core_regwrite(device, GEN7_GMU_GMU2HOST_INTR_MASK, HFI_IRQ_MASK);
}
