// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include "genc_reg.h"
#include "adreno.h"
#include "adreno_genc.h"
#include "adreno_genc_gmu.h"
#include "adreno_snapshot.h"
#include "kgsl_device.h"

static const unsigned int genc_gmu_gx_registers[] = {
	/* GMU GX */
	0x1a800, 0x1a800, 0x1a810, 0x1a813, 0x1a816, 0x1a816, 0x1a818, 0x1a81b,
	0x1a81e, 0x1a81e, 0x1a820, 0x1a823, 0x1a826, 0x1a826, 0x1a828, 0x1a82b,
	0x1a82e, 0x1a82e, 0x1a830, 0x1a833, 0x1a836, 0x1a836, 0x1a838, 0x1a83b,
	0x1a83e, 0x1a83e, 0x1a840, 0x1a843, 0x1a846, 0x1a846, 0x1a880, 0x1a884,
	0x1a900, 0x1a92b, 0x1a940, 0x1a940,
};

static const unsigned int genc_gmu_tcm_registers[] = {
	/* ITCM */
	0x1b400, 0x1c3ff,
	/* DTCM */
	0x1c400, 0x1d3ff,
};

static const unsigned int genc_gmu_registers[] = {
	/* GMU CX */
	0x1f400, 0x1f407, 0x1f410, 0x1f412, 0x1f500, 0x1f500, 0x1f507, 0x1f50a,
	0x1f800, 0x1f804, 0x1f807, 0x1f808, 0x1f80b, 0x1f80c, 0x1f80f, 0x1f81c,
	0x1f824, 0x1f82a, 0x1f82d, 0x1f830, 0x1f840, 0x1f853, 0x1f887, 0x1f889,
	0x1f8a0, 0x1f8a2, 0x1f8a4, 0x1f8af, 0x1f8c0, 0x1f8c3, 0x1f8d0, 0x1f8d0,
	0x1f8e4, 0x1f8e4, 0x1f8e8, 0x1f8ec, 0x1f900, 0x1f903, 0x1f940, 0x1f940,
	0x1f942, 0x1f944, 0x1f94c, 0x1f94d, 0x1f94f, 0x1f951, 0x1f954, 0x1f954,
	0x1f957, 0x1f958, 0x1f95d, 0x1f95d, 0x1f962, 0x1f962, 0x1f964, 0x1f965,
	0x1f980, 0x1f986, 0x1f990, 0x1f99e, 0x1f9c0, 0x1f9c0, 0x1f9c5, 0x1f9cc,
	0x1f9e0, 0x1f9e2, 0x1f9f0, 0x1f9f0, 0x1fa00, 0x1fa01,
	/* GMU AO */
	0x23b00, 0x23b16,
	/* GPU CC */
	0x24000, 0x24012, 0x24040, 0x24052, 0x24400, 0x24404, 0x24407, 0x2440b,
	0x24415, 0x2441c, 0x2441e, 0x2442d, 0x2443c, 0x2443d, 0x2443f, 0x24440,
	0x24442, 0x24449, 0x24458, 0x2445a, 0x24540, 0x2455e, 0x24800, 0x24802,
	0x24c00, 0x24c02, 0x25400, 0x25402, 0x25800, 0x25802, 0x25c00, 0x25c02,
	0x26000, 0x26002,
	/* GPU CC ACD */
	0x26400, 0x26416, 0x26420, 0x26427,
};

static const unsigned int genc_rscc_registers[] = {
	0x23400, 0x23434, 0x23436, 0x23436, 0x23440, 0x23440, 0x23480, 0x23484,
	0x23489, 0x2348c, 0x23491, 0x23494, 0x23499, 0x2349c, 0x234a1, 0x234a4,
	0x234a9, 0x234ac, 0x23500, 0x23502, 0x23504, 0x23507, 0x23514, 0x23519,
	0x23524, 0x2352b, 0x23580, 0x23597, 0x23740, 0x23741, 0x23744, 0x23747,
	0x2374c, 0x23787, 0x237ec, 0x237ef, 0x237f4, 0x2382f, 0x23894, 0x23897,
	0x2389c, 0x238d7, 0x2393c, 0x2393f, 0x23944, 0x2397f,
};

struct gmu_mem_type_desc {
	struct gmu_memdesc *memdesc;
	u32 type;
};

static size_t genc_snapshot_gmu_mem(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_gmu_mem *mem_hdr =
		(struct kgsl_snapshot_gmu_mem *)buf;
	unsigned int *data = (unsigned int *)
		(buf + sizeof(*mem_hdr));
	struct gmu_mem_type_desc *desc = priv;

	if (priv == NULL)
		return 0;

	if (remain < desc->memdesc->size + sizeof(*mem_hdr)) {
		dev_err(device->dev,
			"snapshot: Not enough memory for the gmu section %d\n",
			desc->type);
		return 0;
	}

	memset(mem_hdr, 0, sizeof(*mem_hdr));
	mem_hdr->type = desc->type;
	mem_hdr->hostaddr = (uintptr_t)desc->memdesc->hostptr;
	mem_hdr->gmuaddr = desc->memdesc->gmuaddr;
	mem_hdr->gpuaddr = 0;

	/* Just copy the ringbuffer, there are no active IBs */
	memcpy(data, desc->memdesc->hostptr, desc->memdesc->size);

	return desc->memdesc->size + sizeof(*mem_hdr);
}

static size_t genc_gmu_snapshot_dtcm(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_gmu_mem *mem_hdr =
		(struct kgsl_snapshot_gmu_mem *)buf;
	struct genc_gmu_device *gmu = (struct genc_gmu_device *)priv;
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

	for (i = 0; i < (gmu->vma[GMU_DTCM].size >> 2); i++)
		gmu_core_regread(device, GENC_GMU_CM3_DTCM_START + i, data++);

	return gmu->vma[GMU_DTCM].size + sizeof(*mem_hdr);
}

static size_t genc_gmu_snapshot_itcm(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_gmu_mem *mem_hdr =
			(struct kgsl_snapshot_gmu_mem *)buf;
	void *dest = buf + sizeof(*mem_hdr);
	struct genc_gmu_device *gmu = (struct genc_gmu_device *)priv;

	if (!gmu->itcm_shadow) {
		dev_err(&gmu->pdev->dev, "ITCM not captured\n");
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

static void genc_gmu_snapshot_memories(struct kgsl_device *device,
	struct genc_gmu_device *gmu, struct kgsl_snapshot *snapshot)
{
	struct gmu_mem_type_desc desc;
	struct gmu_memdesc *md;
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
			snapshot, genc_snapshot_gmu_mem, &desc);
	}
}

struct kgsl_snapshot_gmu_version {
	u32 type;
	u32 value;
};

static size_t genc_snapshot_gmu_version(struct kgsl_device *device,
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

static void genc_gmu_snapshot_versions(struct kgsl_device *device,
		struct genc_gmu_device *gmu,
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
				snapshot, genc_snapshot_gmu_version,
				&gmu_vers[i]);
}

#define RSCC_OFFSET_DWORDS 0x38000

static size_t genc_snapshot_rscc_registers(struct kgsl_device *device, u8 *buf,
	size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header = (struct kgsl_snapshot_regs *)buf;
	struct kgsl_snapshot_registers *regs = priv;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int count = 0, j, k;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct genc_gmu_device *gmu = to_genc_gmu(adreno_dev);

	/* Figure out how many registers we are going to dump */
	for (j = 0; j < regs->count; j++) {
		int start = regs->regs[j * 2];
		int end = regs->regs[j * 2 + 1];

		count += (end - start + 1);
	}

	if (remain < (count * 8) + sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "RSCC REGISTERS");
		return 0;
	}

	for (j = 0; j < regs->count; j++) {
		unsigned int start = regs->regs[j * 2];
		unsigned int end = regs->regs[j * 2 + 1];

		for (k = start; k <= end; k++) {
			unsigned int val;

			val = __raw_readl(gmu->rscc_virt +
				((k - RSCC_OFFSET_DWORDS) << 2));
			*data++ = k;
			*data++ = val;
		}
	}

	header->count = count;

	/* Return the size of the section */
	return (count * 8) + sizeof(*header);
}

static void snapshot_rscc_registers(struct adreno_device *adreno_dev,
	struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_snapshot_registers r;

	/* RSCC registers are on cx */
	r.regs = genc_rscc_registers;
	r.count = ARRAY_SIZE(genc_rscc_registers) / 2;

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
			snapshot, genc_snapshot_rscc_registers, &r);
}

/*
 * genc_gmu_device_snapshot() - GENC GMU snapshot function
 * @device: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the GENC GMU specific bits and pieces are grabbed
 * into the snapshot memory
 */
void genc_gmu_device_snapshot(struct kgsl_device *device,
	struct kgsl_snapshot *snapshot)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct genc_gmu_device *gmu = to_genc_gmu(adreno_dev);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
		snapshot, genc_gmu_snapshot_itcm, gmu);

	genc_gmu_snapshot_versions(device, gmu, snapshot);

	genc_gmu_snapshot_memories(device, gmu, snapshot);

	adreno_snapshot_registers(device, snapshot, genc_gmu_registers,
					ARRAY_SIZE(genc_gmu_registers) / 2);

	snapshot_rscc_registers(adreno_dev, snapshot);

	if (!genc_gmu_gx_is_on(device))
		goto dtcm;

	/* Set fence to ALLOW mode so registers can be read */
	kgsl_regwrite(device, GENC_GMU_AO_AHB_FENCE_CTRL, 0);
	/* Make sure the previous write posted before reading */
	wmb();

	adreno_snapshot_registers(device, snapshot,
			genc_gmu_gx_registers,
			ARRAY_SIZE(genc_gmu_gx_registers) / 2);

	/* A stalled SMMU can lead to NoC timeouts when host accesses DTCM */
	if (genc_is_smmu_stalled(device)) {
		dev_err(&gmu->pdev->dev,
			"Not dumping dtcm because SMMU is stalled\n");
		return;
	}

dtcm:
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
		snapshot, genc_gmu_snapshot_dtcm, gmu);
}
