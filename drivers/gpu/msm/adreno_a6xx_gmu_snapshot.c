// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "a6xx_reg.h"
#include "adreno.h"
#include "adreno_a6xx_gmu.h"
#include "adreno_snapshot.h"
#include "kgsl_device.h"

static const unsigned int a6xx_gmu_gx_registers[] = {
	/* GMU GX */
	0x1A800, 0x1A800, 0x1A810, 0x1A813, 0x1A816, 0x1A816, 0x1A818, 0x1A81B,
	0x1A81E, 0x1A81E, 0x1A820, 0x1A823, 0x1A826, 0x1A826, 0x1A828, 0x1A82B,
	0x1A82E, 0x1A82E, 0x1A830, 0x1A833, 0x1A836, 0x1A836, 0x1A838, 0x1A83B,
	0x1A83E, 0x1A83E, 0x1A840, 0x1A843, 0x1A846, 0x1A846, 0x1A880, 0x1A884,
	0x1A900, 0x1A92B, 0x1A940, 0x1A940,
};

static const unsigned int a6xx_gmu_tcm_registers[] = {
	/* ITCM */
	0x1B400, 0x1C3FF,
	/* DTCM */
	0x1C400, 0x1D3FF,
};

static const unsigned int a6xx_gmu_registers[] = {
	/* GMU CX */
	0x1F400, 0x1F407, 0x1F410, 0x1F412, 0x1F500, 0x1F500, 0x1F507, 0x1F50A,
	0x1F800, 0x1F804, 0x1F807, 0x1F808, 0x1F80B, 0x1F80C, 0x1F80F, 0x1F81C,
	0x1F824, 0x1F82A, 0x1F82D, 0x1F830, 0x1F840, 0x1F853, 0x1F887, 0x1F889,
	0x1F8A0, 0x1F8A2, 0x1F8A4, 0x1F8AF, 0x1F8C0, 0x1F8C3, 0x1F8D0, 0x1F8D0,
	0x1F8E4, 0x1F8E4, 0x1F8E8, 0x1F8EC, 0x1F900, 0x1F903, 0x1F940, 0x1F940,
	0x1F942, 0x1F944, 0x1F94C, 0x1F94D, 0x1F94F, 0x1F951, 0x1F954, 0x1F954,
	0x1F957, 0x1F958, 0x1F95D, 0x1F95D, 0x1F962, 0x1F962, 0x1F964, 0x1F965,
	0x1F980, 0x1F986, 0x1F990, 0x1F99E, 0x1F9C0, 0x1F9C0, 0x1F9C5, 0x1F9CC,
	0x1F9E0, 0x1F9E2, 0x1F9F0, 0x1F9F0, 0x1FA00, 0x1FA01,
	/* GMU AO */
	0x23B00, 0x23B16,
	/* GPU CC */
	0x24000, 0x24012, 0x24040, 0x24052, 0x24400, 0x24404, 0x24407, 0x2440B,
	0x24415, 0x2441C, 0x2441E, 0x2442D, 0x2443C, 0x2443D, 0x2443F, 0x24440,
	0x24442, 0x24449, 0x24458, 0x2445A, 0x24540, 0x2455E, 0x24800, 0x24802,
	0x24C00, 0x24C02, 0x25400, 0x25402, 0x25800, 0x25802, 0x25C00, 0x25C02,
	0x26000, 0x26002,
	/* GPU CC ACD */
	0x26400, 0x26416, 0x26420, 0x26427,
};

static const unsigned int a660_gmu_registers[] = {
	/* GMU CX */
	0x1F408, 0x1F40D, 0x1F40F, 0x1F40F, 0x1F50B, 0x1F50B, 0x1F860, 0x1F860,
	0x1F870, 0x1F877, 0x1F8C4, 0x1F8C4, 0x1F8F0, 0x1F8F1, 0x1F948, 0x1F94A,
	0x1F966, 0x1F96B, 0x1F970, 0x1F970, 0x1F972, 0x1F979, 0x1F9CD, 0x1F9D4,
	0x1FA02, 0x1FA03, 0x20000, 0x20001, 0x20004, 0x20004, 0x20008, 0x20012,
	0x20018, 0x20018,
};

struct gmu_mem_type_desc {
	struct gmu_memdesc *memdesc;
	uint32_t type;
};

static size_t a6xx_snapshot_gmu_mem(struct kgsl_device *device,
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

static size_t a6xx_gmu_snapshot_dtcm(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_gmu_mem *mem_hdr =
		(struct kgsl_snapshot_gmu_mem *)buf;
	struct a6xx_gmu_device *gmu = (struct a6xx_gmu_device *)priv;
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
		gmu_core_regread(device, A6XX_GMU_CM3_DTCM_START + i, data++);

	return gmu->vma[GMU_DTCM].size + sizeof(*mem_hdr);
}

static size_t a6xx_gmu_snapshot_itcm(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_gmu_mem *mem_hdr =
			(struct kgsl_snapshot_gmu_mem *)buf;
	void *dest = buf + sizeof(*mem_hdr);
	struct a6xx_gmu_device *gmu = (struct a6xx_gmu_device *)priv;

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

static void a6xx_gmu_snapshot_memories(struct kgsl_device *device,
	struct a6xx_gmu_device *gmu, struct kgsl_snapshot *snapshot)
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
			snapshot, a6xx_snapshot_gmu_mem, &desc);
	}
}

struct kgsl_snapshot_gmu_version {
	uint32_t type;
	uint32_t value;
};

static size_t a6xx_snapshot_gmu_version(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	uint32_t *data = (uint32_t *) (buf + sizeof(*header));
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

static void a6xx_gmu_snapshot_versions(struct kgsl_device *device,
		struct a6xx_gmu_device *gmu,
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
				snapshot, a6xx_snapshot_gmu_version,
				&gmu_vers[i]);
}

/*
 * a6xx_gmu_device_snapshot() - A6XX GMU snapshot function
 * @device: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the A6XX GMU specific bits and pieces are grabbed
 * into the snapshot memory
 */
void a6xx_gmu_device_snapshot(struct kgsl_device *device,
	struct kgsl_snapshot *snapshot)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(ADRENO_DEVICE(device));
	unsigned int val;

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
		snapshot, a6xx_gmu_snapshot_itcm, gmu);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
		snapshot, a6xx_gmu_snapshot_dtcm, gmu);

	a6xx_gmu_snapshot_versions(device, gmu, snapshot);

	a6xx_gmu_snapshot_memories(device, gmu, snapshot);

	/* Snapshot tcms as registers for legacy targets */
	if (adreno_is_a630(ADRENO_DEVICE(device)) ||
			adreno_is_a615_family(ADRENO_DEVICE(device)))
		adreno_snapshot_registers(device, snapshot,
				a6xx_gmu_tcm_registers,
				ARRAY_SIZE(a6xx_gmu_tcm_registers) / 2);

	adreno_snapshot_registers(device, snapshot, a6xx_gmu_registers,
					ARRAY_SIZE(a6xx_gmu_registers) / 2);

	/* Snapshot A660 specific GMU registers */
	if (adreno_is_a660(ADRENO_DEVICE(device)))
		adreno_snapshot_registers(device, snapshot, a660_gmu_registers,
					ARRAY_SIZE(a660_gmu_registers) / 2);

	if (!a6xx_gmu_gx_is_on(device))
		return;

	/* Set fence to ALLOW mode so registers can be read */
	kgsl_regwrite(device, A6XX_GMU_AO_AHB_FENCE_CTRL, 0);
	/* Make sure the previous write posted before reading */
	wmb();
	kgsl_regread(device, A6XX_GMU_AO_AHB_FENCE_CTRL, &val);

	adreno_snapshot_registers(device, snapshot,
			a6xx_gmu_gx_registers,
			ARRAY_SIZE(a6xx_gmu_gx_registers) / 2);
}
