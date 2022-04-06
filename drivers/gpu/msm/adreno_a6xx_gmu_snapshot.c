// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include "a6xx_reg.h"
#include "adreno.h"
#include "adreno_a6xx.h"
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
};

static const unsigned int a660_gmu_registers[] = {
	/* GMU CX */
	0x1F408, 0x1F40D, 0x1F40F, 0x1F40F, 0x1F50B, 0x1F50B, 0x1F860, 0x1F860,
	0x1F870, 0x1F877, 0x1F8C4, 0x1F8C4, 0x1F8F0, 0x1F8F1, 0x1F948, 0x1F94A,
	0x1F966, 0x1F96B, 0x1F970, 0x1F970, 0x1F972, 0x1F979, 0x1F9CD, 0x1F9D4,
	0x1FA02, 0x1FA03, 0x20000, 0x20001, 0x20004, 0x20004, 0x20008, 0x20012,
	0x20018, 0x20018,
	/* GMU AO LPAC */
	0x23B30, 0x23B30,
};

static const unsigned int a6xx_gmu_gpucc_registers[] = {
	/* GPU CC */
	0x24000, 0x24012, 0x24040, 0x24052, 0x24400, 0x24404, 0x24407, 0x2440B,
	0x24415, 0x2441C, 0x2441E, 0x2442D, 0x2443C, 0x2443D, 0x2443F, 0x24440,
	0x24442, 0x24449, 0x24458, 0x2445A, 0x24540, 0x2455E, 0x24800, 0x24802,
	0x24C00, 0x24C02, 0x25400, 0x25402, 0x25800, 0x25802, 0x25C00, 0x25C02,
	0x26000, 0x26002,
	/* GPU CC ACD */
	0x26400, 0x26416, 0x26420, 0x26427,
};

static const unsigned int a662_gmu_gpucc_registers[] = {
	/* GPU CC */
	0x24000, 0x2400e, 0x24400, 0x2440e, 0x24800, 0x24805, 0x24c00, 0x24cff,
	0x25800, 0x25804, 0x25c00, 0x25c04, 0x26000, 0x26004, 0x26400, 0x26405,
	0x26414, 0x2641d, 0x2642a, 0x26430, 0x26432, 0x26432, 0x26441, 0x26455,
	0x26466, 0x26468, 0x26478, 0x2647a, 0x26489, 0x2648a, 0x2649c, 0x2649e,
	0x264a0, 0x264a3, 0x264b3, 0x264b5, 0x264c5, 0x264c7, 0x264d6, 0x264d8,
	0x264e8, 0x264e9, 0x264f9, 0x264fc, 0x2650b, 0x2650c, 0x2651c, 0x2651e,
	0x26540, 0x26570, 0x26600, 0x26616, 0x26620, 0x2662d,
};

static const unsigned int a630_rscc_snapshot_registers[] = {
	0x23400, 0x23434, 0x23436, 0x23436, 0x23480, 0x23484, 0x23489, 0x2348C,
	0x23491, 0x23494, 0x23499, 0x2349C, 0x234A1, 0x234A4, 0x234A9, 0x234AC,
	0x23500, 0x23502, 0x23504, 0x23507, 0x23514, 0x23519, 0x23524, 0x2352B,
	0x23580, 0x23597, 0x23740, 0x23741, 0x23744, 0x23747, 0x2374C, 0x23787,
	0x237EC, 0x237EF, 0x237F4, 0x2382F, 0x23894, 0x23897, 0x2389C, 0x238D7,
	0x2393C, 0x2393F, 0x23944, 0x2397F,
};

static const unsigned int a6xx_rscc_snapshot_registers[] = {
	0x23400, 0x23434, 0x23436, 0x23436, 0x23440, 0x23440, 0x23480, 0x23484,
	0x23489, 0x2348C, 0x23491, 0x23494, 0x23499, 0x2349C, 0x234A1, 0x234A4,
	0x234A9, 0x234AC, 0x23500, 0x23502, 0x23504, 0x23507, 0x23514, 0x23519,
	0x23524, 0x2352B, 0x23580, 0x23597, 0x23740, 0x23741, 0x23744, 0x23747,
	0x2374C, 0x23787, 0x237EC, 0x237EF, 0x237F4, 0x2382F, 0x23894, 0x23897,
	0x2389C, 0x238D7, 0x2393C, 0x2393F, 0x23944, 0x2397F,
};

static const unsigned int a650_rscc_registers[] = {
	0x38000, 0x38034, 0x38036, 0x38036, 0x38040, 0x38042, 0x38080, 0x38084,
	0x38089, 0x3808C, 0x38091, 0x38094, 0x38099, 0x3809C, 0x380A1, 0x380A4,
	0x380A9, 0x380AC, 0x38100, 0x38102, 0x38104, 0x38107, 0x38114, 0x38119,
	0x38124, 0x3812E, 0x38180, 0x38197, 0x38340, 0x38341, 0x38344, 0x38347,
	0x3834C, 0x3834F, 0x38351, 0x38354, 0x38356, 0x38359, 0x3835B, 0x3835E,
	0x38360, 0x38363, 0x38365, 0x38368, 0x3836A, 0x3836D, 0x3836F, 0x38372,
	0x383EC, 0x383EF, 0x383F4, 0x383F7, 0x383F9, 0x383FC, 0x383FE, 0x38401,
	0x38403, 0x38406, 0x38408, 0x3840B, 0x3840D, 0x38410, 0x38412, 0x38415,
	0x38417, 0x3841A, 0x38494, 0x38497, 0x3849C, 0x3849F, 0x384A1, 0x384A4,
	0x384A6, 0x384A9, 0x384AB, 0x384AE, 0x384B0, 0x384B3, 0x384B5, 0x384B8,
	0x384BA, 0x384BD, 0x384BF, 0x384C2, 0x3853C, 0x3853F, 0x38544, 0x38547,
	0x38549, 0x3854C, 0x3854E, 0x38551, 0x38553, 0x38556, 0x38558, 0x3855B,
	0x3855D, 0x38560, 0x38562, 0x38565, 0x38567, 0x3856A, 0x385E4, 0x385E7,
	0x385EC, 0x385EF, 0x385F1, 0x385F4, 0x385F6, 0x385F9, 0x385FB, 0x385FE,
	0x38600, 0x38603, 0x38605, 0x38608, 0x3860A, 0x3860D, 0x3860F, 0x38612,
	0x3868C, 0x3868F, 0x38694, 0x38697, 0x38699, 0x3869C, 0x3869E, 0x386A1,
	0x386A3, 0x386A6, 0x386A8, 0x386AB, 0x386AD, 0x386B0, 0x386B2, 0x386B5,
	0x386B7, 0x386BA, 0x38734, 0x38737, 0x3873C, 0x3873F, 0x38741, 0x38744,
	0x38746, 0x38749, 0x3874B, 0x3874E, 0x38750, 0x38753, 0x38755, 0x38758,
	0x3875A, 0x3875D, 0x3875F, 0x38762, 0x387DC, 0x387DF, 0x387E4, 0x387E7,
	0x387E9, 0x387EC, 0x387EE, 0x387F1, 0x387F3, 0x387F6, 0x387F8, 0x387FB,
	0x387FD, 0x38800, 0x38802, 0x38805, 0x38807, 0x3880A, 0x38884, 0x38887,
	0x3888C, 0x3888F, 0x38891, 0x38894, 0x38896, 0x38899, 0x3889B, 0x3889E,
	0x388A0, 0x388A3, 0x388A5, 0x388A8, 0x388AA, 0x388AD, 0x388AF, 0x388B2,
	0x3892C, 0x3892F, 0x38934, 0x38937, 0x38939, 0x3893C, 0x3893E, 0x38941,
	0x38943, 0x38946, 0x38948, 0x3894B, 0x3894D, 0x38950, 0x38952, 0x38955,
	0x38957, 0x3895A, 0x38B50, 0x38B51, 0x38B53, 0x38B55, 0x38B5A, 0x38B5A,
	0x38B5F, 0x38B5F, 0x38B64, 0x38B64, 0x38B69, 0x38B69, 0x38B6E, 0x38B6E,
	0x38B73, 0x38B73, 0x38BF8, 0x38BF8, 0x38BFD, 0x38BFD, 0x38C02, 0x38C02,
	0x38C07, 0x38C07, 0x38C0C, 0x38C0C, 0x38C11, 0x38C11, 0x38C16, 0x38C16,
	0x38C1B, 0x38C1B, 0x38CA0, 0x38CA0, 0x38CA5, 0x38CA5, 0x38CAA, 0x38CAA,
	0x38CAF, 0x38CAF, 0x38CB4, 0x38CB4, 0x38CB9, 0x38CB9, 0x38CBE, 0x38CBE,
	0x38CC3, 0x38CC3, 0x38D48, 0x38D48, 0x38D4D, 0x38D4D, 0x38D52, 0x38D52,
	0x38D57, 0x38D57, 0x38D5C, 0x38D5C, 0x38D61, 0x38D61, 0x38D66, 0x38D66,
	0x38D6B, 0x38D6B, 0x38DF0, 0x38DF0, 0x38DF5, 0x38DF5, 0x38DFA, 0x38DFA,
	0x38DFF, 0x38DFF, 0x38E04, 0x38E04, 0x38E09, 0x38E09, 0x38E0E, 0x38E0E,
	0x38E13, 0x38E13, 0x38E98, 0x38E98, 0x38E9D, 0x38E9D, 0x38EA2, 0x38EA2,
	0x38EA7, 0x38EA7, 0x38EAC, 0x38EAC, 0x38EB1, 0x38EB1, 0x38EB6, 0x38EB6,
	0x38EBB, 0x38EBB, 0x38F40, 0x38F40, 0x38F45, 0x38F45, 0x38F4A, 0x38F4A,
	0x38F4F, 0x38F4F, 0x38F54, 0x38F54, 0x38F59, 0x38F59, 0x38F5E, 0x38F5E,
	0x38F63, 0x38F63, 0x38FE8, 0x38FE8, 0x38FED, 0x38FED, 0x38FF2, 0x38FF2,
	0x38FF7, 0x38FF7, 0x38FFC, 0x38FFC, 0x39001, 0x39001, 0x39006, 0x39006,
	0x3900B, 0x3900B, 0x39090, 0x39090, 0x39095, 0x39095, 0x3909A, 0x3909A,
	0x3909F, 0x3909F, 0x390A4, 0x390A4, 0x390A9, 0x390A9, 0x390AE, 0x390AE,
	0x390B3, 0x390B3, 0x39138, 0x39138, 0x3913D, 0x3913D, 0x39142, 0x39142,
	0x39147, 0x39147, 0x3914C, 0x3914C, 0x39151, 0x39151, 0x39156, 0x39156,
	0x3915B, 0x3915B,
};

struct gmu_mem_type_desc {
	struct kgsl_memdesc *memdesc;
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

	if (priv == NULL || desc->memdesc->hostptr == NULL)
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

	/* FIXME: use a bulk read? */
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

#define RSCC_OFFSET_DWORDS 0x38000

static size_t a6xx_snapshot_rscc_registers(struct kgsl_device *device, u8 *buf,
	size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header = (struct kgsl_snapshot_regs *)buf;
	struct kgsl_snapshot_registers *regs = priv;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int count = 0, j, k;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

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

	/* RSCC registers are on cx */
	if (adreno_is_a650_family(adreno_dev)) {
		struct kgsl_snapshot_registers r;

		r.regs = a650_rscc_registers;
		r.count = ARRAY_SIZE(a650_rscc_registers) / 2;

		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
			snapshot, a6xx_snapshot_rscc_registers, &r);
	} else if (adreno_is_a615_family(adreno_dev) ||
			adreno_is_a630(adreno_dev)) {
		adreno_snapshot_registers(device, snapshot,
			a630_rscc_snapshot_registers,
			ARRAY_SIZE(a630_rscc_snapshot_registers) / 2);
	} else if (adreno_is_a640(adreno_dev) || adreno_is_a680(adreno_dev)) {
		adreno_snapshot_registers(device, snapshot,
			a6xx_rscc_snapshot_registers,
			ARRAY_SIZE(a6xx_rscc_snapshot_registers) / 2);
	}
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
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
		snapshot, a6xx_gmu_snapshot_itcm, gmu);

	a6xx_gmu_snapshot_versions(device, gmu, snapshot);

	a6xx_gmu_snapshot_memories(device, gmu, snapshot);

	/* Snapshot tcms as registers for legacy targets */
	if (adreno_is_a630(adreno_dev) ||
			adreno_is_a615_family(adreno_dev))
		adreno_snapshot_registers(device, snapshot,
				a6xx_gmu_tcm_registers,
				ARRAY_SIZE(a6xx_gmu_tcm_registers) / 2);

	adreno_snapshot_registers(device, snapshot, a6xx_gmu_registers,
					ARRAY_SIZE(a6xx_gmu_registers) / 2);

	if (adreno_is_a662(adreno_dev) || adreno_is_a621(adreno_dev))
		adreno_snapshot_registers(device, snapshot,
			a662_gmu_gpucc_registers,
			ARRAY_SIZE(a662_gmu_gpucc_registers) / 2);
	else
		adreno_snapshot_registers(device, snapshot,
			a6xx_gmu_gpucc_registers,
			ARRAY_SIZE(a6xx_gmu_gpucc_registers) / 2);

	/* Snapshot A660 specific GMU registers */
	if (adreno_is_a660(adreno_dev))
		adreno_snapshot_registers(device, snapshot, a660_gmu_registers,
					ARRAY_SIZE(a660_gmu_registers) / 2);

	snapshot_rscc_registers(adreno_dev, snapshot);

	if (!a6xx_gmu_gx_is_on(adreno_dev))
		goto dtcm;

	/* Set fence to ALLOW mode so registers can be read */
	kgsl_regwrite(device, A6XX_GMU_AO_AHB_FENCE_CTRL, 0);
	/* Make sure the previous write posted before reading */
	wmb();

	adreno_snapshot_registers(device, snapshot,
			a6xx_gmu_gx_registers,
			ARRAY_SIZE(a6xx_gmu_gx_registers) / 2);

	/* A stalled SMMU can lead to NoC timeouts when host accesses DTCM */
	if (a6xx_is_smmu_stalled(device)) {
		dev_err(&gmu->pdev->dev,
			"Not dumping dtcm because SMMU is stalled\n");
		return;
	}

dtcm:
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
		snapshot, a6xx_gmu_snapshot_dtcm, gmu);
}
