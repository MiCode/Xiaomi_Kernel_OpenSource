// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include "gen7_reg.h"
#include "adreno.h"
#include "adreno_gen7.h"
#include "adreno_gen7_gmu.h"
#include "adreno_snapshot.h"
#include "kgsl_device.h"

static const u32 gen7_gmu_registers[] = {
	0x10001, 0x10001, 0x10003, 0x10003, 0x10401, 0x10401, 0x10403, 0x10403,
	0x10801, 0x10801, 0x10803, 0x10803, 0x10c01, 0x10c01, 0x10c03, 0x10c03,
	0x11001, 0x11001, 0x11003, 0x11003, 0x11401, 0x11401, 0x11403, 0x11403,
	0x11801, 0x11801, 0x11803, 0x11803, 0x11c01, 0x11c01, 0x11c03, 0x11c03,
	0x1f400, 0x1f40d, 0x1f40f, 0x1f411, 0x1f500, 0x1f500, 0x1f507, 0x1f507,
	0x1f509, 0x1f50b, 0x1f800, 0x1f804, 0x1f807, 0x1f808, 0x1f80b, 0x1f80c,
	0x1f80f, 0x1f80f, 0x1f811, 0x1f811, 0x1f813, 0x1f817, 0x1f819, 0x1f81c,
	0x1f824, 0x1f82a, 0x1f82d, 0x1f830, 0x1f840, 0x1f853, 0x1f860, 0x1f860,
	0x1f870, 0x1f879, 0x1f87f, 0x1f87f, 0x1f888, 0x1f889, 0x1f8a0, 0x1f8a2,
	0x1f8a4, 0x1f8af, 0x1f8c0, 0x1f8c1, 0x1f8c3, 0x1f8c4, 0x1f8d0, 0x1f8d0,
	0x1f8ec, 0x1f8ec, 0x1f8f0, 0x1f8f1, 0x1f910, 0x1f914, 0x1f920, 0x1f921,
	0x1f924, 0x1f925, 0x1f928, 0x1f929, 0x1f92c, 0x1f92d, 0x1f940, 0x1f940,
	0x1f942, 0x1f944, 0x1f948, 0x1f94a, 0x1f94f, 0x1f951, 0x1f958, 0x1f95a,
	0x1f95d, 0x1f95d, 0x1f962, 0x1f962, 0x1f964, 0x1f96b, 0x1f970, 0x1f979,
	0x1f980, 0x1f981, 0x1f984, 0x1f986, 0x1f992, 0x1f993, 0x1f996, 0x1f99e,
	0x1f9c0, 0x1f9c0, 0x1f9c5, 0x1f9d4, 0x1f9f0, 0x1f9f1, 0x1f9f8, 0x1f9fa,
	0x1fa00, 0x1fa03, 0x20000, 0x20005, 0x20008, 0x2000c, 0x20010, 0x20012,
	0x20018, 0x20018, 0x20020, 0x20023, 0x20030, 0x20031, 0x23801, 0x23801,
	0x23803, 0x23803, 0x23805, 0x23805, 0x23807, 0x23807, 0x23809, 0x23809,
	0x2380b, 0x2380b, 0x2380d, 0x2380d, 0x2380f, 0x2380f, 0x23811, 0x23811,
	0x23813, 0x23813, 0x23815, 0x23815, 0x23817, 0x23817, 0x23819, 0x23819,
	0x2381b, 0x2381b, 0x2381d, 0x2381d, 0x2381f, 0x23820, 0x23822, 0x23822,
	0x23824, 0x23824, 0x23826, 0x23826, 0x23828, 0x23828, 0x2382a, 0x2382a,
	0x2382c, 0x2382c, 0x2382e, 0x2382e, 0x23830, 0x23830, 0x23832, 0x23832,
	0x23834, 0x23834, 0x23836, 0x23836, 0x23838, 0x23838, 0x2383a, 0x2383a,
	0x2383c, 0x2383c, 0x2383e, 0x2383e, 0x23840, 0x23847, 0x23b00, 0x23b01,
	0x23b03, 0x23b03, 0x23b05, 0x23b0e, 0x23b10, 0x23b13, 0x23b15, 0x23b16,
	0x23b20, 0x23b20, 0x23b28, 0x23b28, 0x23b30, 0x23b30,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_gmu_registers), 8));

static const u32 gen7_gmu_gx_registers[] = {
	0x1a400, 0x1a41f, 0x1a440, 0x1a45f, 0x1a480, 0x1a49f, 0x1a4c0, 0x1a4df,
	0x1a500, 0x1a51f, 0x1a540, 0x1a55f, 0x1a580, 0x1a59f, 0x1a5c0, 0x1a5df,
	0x1a780, 0x1a781, 0x1a783, 0x1a785, 0x1a787, 0x1a789, 0x1a78b, 0x1a78d,
	0x1a78f, 0x1a791, 0x1a793, 0x1a795, 0x1a797, 0x1a799, 0x1a79b, 0x1a79b,
	0x1a7c0, 0x1a7c1, 0x1a7c4, 0x1a7c5, 0x1a7c8, 0x1a7c9, 0x1a7cc, 0x1a7cd,
	0x1a7d0, 0x1a7d1, 0x1a7d4, 0x1a7d5, 0x1a7d8, 0x1a7d9, 0x1a7fc, 0x1a7fd,
	0x1a800, 0x1a802, 0x1a804, 0x1a804, 0x1a816, 0x1a816, 0x1a81e, 0x1a81e,
	0x1a826, 0x1a826, 0x1a82e, 0x1a82e, 0x1a836, 0x1a836, 0x1a83e, 0x1a83e,
	0x1a846, 0x1a846, 0x1a860, 0x1a862, 0x1a864, 0x1a867, 0x1a870, 0x1a870,
	0x1a883, 0x1a884, 0x1a8c0, 0x1a8c2, 0x1a8c4, 0x1a8c7, 0x1a8d0, 0x1a8d3,
	0x1a900, 0x1a92b, 0x1a940, 0x1a940,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_gmu_gx_registers), 8));

static const u32 gen7_rscc_registers[] = {
	0x14000, 0x14036, 0x14040, 0x14042, 0x14080, 0x14084, 0x14089, 0x1408c,
	0x14091, 0x14094, 0x14099, 0x1409c, 0x140a1, 0x140a4, 0x140a9, 0x140ac,
	0x14100, 0x14102, 0x14114, 0x14119, 0x14124, 0x1412e, 0x14140, 0x14143,
	0x14180, 0x14197, 0x14340, 0x14342, 0x14344, 0x14347, 0x1434c, 0x14373,
	0x143ec, 0x143ef, 0x143f4, 0x1441b, 0x14494, 0x14497, 0x1449c, 0x144c3,
	0x1453c, 0x1453f, 0x14544, 0x1456b, 0x145e4, 0x145e7, 0x145ec, 0x14613,
	0x1468c, 0x1468f, 0x14694, 0x146bb, 0x14734, 0x14737, 0x1473c, 0x14763,
	0x147dc, 0x147df, 0x147e4, 0x1480b, 0x14884, 0x14887, 0x1488c, 0x148b3,
	0x1492c, 0x1492f, 0x14934, 0x1495b, 0x14f51, 0x14f54,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_rscc_registers), 8));

struct gmu_mem_type_desc {
	struct kgsl_memdesc *memdesc;
	u32 type;
};

static size_t gen7_snapshot_gmu_mem(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_gmu_mem *mem_hdr =
		(struct kgsl_snapshot_gmu_mem *)buf;
	unsigned int *data = (unsigned int *)
		(buf + sizeof(*mem_hdr));
	struct gmu_mem_type_desc *desc = priv;

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
		gmu_core_regread(device, GEN7_GMU_CM3_DTCM_START + i, data++);

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
void gen7_gmu_device_snapshot(struct kgsl_device *device,
	struct kgsl_snapshot *snapshot)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
		snapshot, gen7_gmu_snapshot_itcm, gmu);

	gen7_gmu_snapshot_versions(device, gmu, snapshot);

	gen7_gmu_snapshot_memories(device, gmu, snapshot);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2, snapshot,
		adreno_snapshot_registers_v2, (void *) gen7_gmu_registers);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2, snapshot,
		gen7_snapshot_rscc_registers, (void *) gen7_rscc_registers);

	if (!gen7_gmu_gx_is_on(device))
		goto dtcm;

	/* Set fence to ALLOW mode so registers can be read */
	kgsl_regwrite(device, GEN7_GMU_AO_AHB_FENCE_CTRL, 0);
	/* Make sure the previous write posted before reading */
	wmb();

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS_V2, snapshot,
		adreno_snapshot_registers_v2, (void *) gen7_gmu_gx_registers);

	/* A stalled SMMU can lead to NoC timeouts when host accesses DTCM */
	if (gen7_is_smmu_stalled(device)) {
		dev_err(&gmu->pdev->dev,
			"Not dumping dtcm because SMMU is stalled\n");
		return;
	}

dtcm:
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
		snapshot, gen7_gmu_snapshot_dtcm, gmu);
}
