/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include "adreno_snapshot.h"
#include "a6xx_reg.h"
#include "adreno_a6xx.h"


#define A6XX_NUM_CTXTS 2

static const unsigned int a6xx_gras_cluster[] = {
	0x8000, 0x8006, 0x8010, 0x8092, 0x8094, 0x809D, 0x80A0, 0x80A6,
	0x80AF, 0x80F1, 0x8100, 0x8107, 0x8109, 0x8109, 0x8110, 0x8110,
	0x8400, 0x840B,
};

static const unsigned int a6xx_ps_cluster[] = {
	0x8800, 0x8806, 0x8809, 0x8811, 0x8818, 0x881E, 0x8820, 0x8865,
	0x8870, 0x8879, 0x8880, 0x8889, 0x8890, 0x8891, 0x8898, 0x8898,
	0x88C0, 0x88c1, 0x88D0, 0x88E3, 0x88F0, 0x88F3, 0x8900, 0x891A,
	0x8927, 0x8928, 0x8C00, 0x8C01, 0x8C17, 0x8C33, 0x9200, 0x9216,
	0x9218, 0x9236, 0x9300, 0x9306,
};

static const unsigned int a6xx_fe_cluster[] = {
	0x9300, 0x9306, 0x9800, 0x9806, 0x9B00, 0x9B07, 0xA000, 0xA009,
	0xA00E, 0xA0EF, 0xA0F8, 0xA0F8,
};

static const unsigned int a6xx_pc_vs_cluster[] = {
	0x9100, 0x9108, 0x9300, 0x9306, 0x9980, 0x9981, 0x9B00, 0x9B07,
};

static struct a6xx_cluster_registers {
	unsigned int id;
	const unsigned int *regs;
	unsigned int num_sets;
	unsigned int offset0;
	unsigned int offset1;
} a6xx_clusters[] = {
	{ CP_CLUSTER_GRAS, a6xx_gras_cluster, ARRAY_SIZE(a6xx_gras_cluster)/2 },
	{ CP_CLUSTER_PS, a6xx_ps_cluster, ARRAY_SIZE(a6xx_ps_cluster)/2 },
	{ CP_CLUSTER_FE, a6xx_fe_cluster, ARRAY_SIZE(a6xx_fe_cluster)/2 },
	{ CP_CLUSTER_PC_VS, a6xx_pc_vs_cluster,
					ARRAY_SIZE(a6xx_pc_vs_cluster)/2 },
};

struct a6xx_cluster_regs_info {
	struct a6xx_cluster_registers *cluster;
	unsigned int ctxt_id;
};

static const unsigned int a6xx_vbif_ver_20xxxxxx_registers[] = {
	/* VBIF */
	0x3000, 0x3007, 0x300C, 0x3014, 0x3018, 0x302D, 0x3030, 0x3031,
	0x3034, 0x3036, 0x303C, 0x303D, 0x3040, 0x3040, 0x3042, 0x3042,
	0x3049, 0x3049, 0x3058, 0x3058, 0x305A, 0x3061, 0x3064, 0x3068,
	0x306C, 0x306D, 0x3080, 0x3088, 0x308B, 0x308C, 0x3090, 0x3094,
	0x3098, 0x3098, 0x309C, 0x309C, 0x30C0, 0x30C0, 0x30C8, 0x30C8,
	0x30D0, 0x30D0, 0x30D8, 0x30D8, 0x30E0, 0x30E0, 0x3100, 0x3100,
	0x3108, 0x3108, 0x3110, 0x3110, 0x3118, 0x3118, 0x3120, 0x3120,
	0x3124, 0x3125, 0x3129, 0x3129, 0x3131, 0x3131, 0x3154, 0x3154,
	0x3156, 0x3156, 0x3158, 0x3158, 0x315A, 0x315A, 0x315C, 0x315C,
	0x315E, 0x315E, 0x3160, 0x3160, 0x3162, 0x3162, 0x340C, 0x340C,
	0x3410, 0x3410, 0x3800, 0x3801,
};

static const struct adreno_vbif_snapshot_registers
a6xx_vbif_snapshot_registers[] = {
	{ 0x20040000, 0xFF000000, a6xx_vbif_ver_20xxxxxx_registers,
				ARRAY_SIZE(a6xx_vbif_ver_20xxxxxx_registers)/2},
};

/*
 * Set of registers to dump for A6XX on snapshot.
 * Registers in pairs - first value is the start offset, second
 * is the stop offset (inclusive)
 */

static const unsigned int a6xx_registers[] = {
	/* RBBM */
	0x0000, 0x0002, 0x0010, 0x0010, 0x0012, 0x0012, 0x0014, 0x0014,
	0x0018, 0x001B, 0x001e, 0x0032, 0x0038, 0x003C, 0x0042, 0x0042,
	0x0044, 0x0044, 0x0047, 0x0047, 0x0056, 0x0056, 0x00AD, 0x00AE,
	0x00B0, 0x00FB, 0x0100, 0x011D, 0x0200, 0x020D, 0x0210, 0x0213,
	0x0218, 0x023D, 0x0400, 0x04F9, 0x0500, 0x0500, 0x0505, 0x050B,
	0x050E, 0x0511, 0x0533, 0x0533, 0x0540, 0x0555,
	/* CP */
	0x0800, 0x0808, 0x0810, 0x0813, 0x0820, 0x0821, 0x0823, 0x0827,
	0x0830, 0x0833, 0x0840, 0x0843, 0x084F, 0x086F, 0x0880, 0x088A,
	0x08A0, 0x08AB, 0x08C0, 0x08C4, 0x08D0, 0x08DD, 0x08F0, 0x08F3,
	0x0900, 0x0903, 0x0908, 0x0911, 0x0928, 0x093E, 0x0942, 0x094D,
	0x0980, 0x0984, 0x098D, 0x0996, 0x0998, 0x099E, 0x09A0, 0x09A6,
	0x09A8, 0x09AE, 0x09B0, 0x09B1, 0x09C2, 0x09C8, 0x0A00, 0x0A03,
	/* VSC */
	0x0C00, 0x0C04, 0x0C06, 0x0C06, 0x0C10, 0x0CD9, 0x0E00, 0x0E0E,
	/* UCHE */
	0x0E10, 0x0E13, 0x0E17, 0x0E19, 0x0E1C, 0x0E2B, 0x0E30, 0x0E32,
	0x0E38, 0x0E39,
	/* GRAS */
	0x8600, 0x8601, 0x8604, 0x8605, 0x8610, 0x861B, 0x8620, 0x8620,
	0x8628, 0x862B, 0x8630, 0x8637,
	/* RB */
	0x8E01, 0x8E01, 0x8E04, 0x8E05, 0x8E07, 0x8E08, 0x8E0C, 0x8E0C,
	0x8E10, 0x8E1C, 0x8E20, 0x8E25, 0x8E28, 0x8E28, 0x8E2C, 0x8E2F,
	0x8E3B, 0x8E3E, 0x8E40, 0x8E43, 0x8E50, 0x8E5E, 0x8E70, 0x8E77,
	/* VPC */
	0x9600, 0x9604, 0x9624, 0x9637,
	/* PC */
	0x9E00, 0x9E01, 0x9E03, 0x9E0E, 0x9E11, 0x9E16, 0x9E19, 0x9E19,
	0x9E1C, 0x9E1C, 0x9E20, 0x9E23, 0x9E30, 0x9E31, 0x9E34, 0x9E34,
	0x9E70, 0x9E72, 0x9E78, 0x9E79, 0x9E80, 0x9FFF,
	/* VFD */
	0xA600, 0xA601, 0xA603, 0xA603, 0xA60A, 0xA60A, 0xA610, 0xA617,
	0xA630, 0xA630, 0xD200, 0xD263,
};


static struct kgsl_memdesc a6xx_capturescript;
static struct kgsl_memdesc a6xx_crashdump_registers;
static bool crash_dump_valid;

static size_t a6xx_legacy_snapshot_registers(struct kgsl_device *device,
		u8 *buf, size_t remain)
{
	struct kgsl_snapshot_registers regs = {
		.regs = a6xx_registers,
		.count = ARRAY_SIZE(a6xx_registers) / 2,
	};

	return kgsl_snapshot_dump_registers(device, buf, remain, &regs);
}

static struct cdregs {
	const unsigned int *regs;
	unsigned int size;
} _a6xx_cd_registers[] = {
	{ a6xx_registers, ARRAY_SIZE(a6xx_registers) },
};

#define REG_PAIR_COUNT(_a, _i) \
	(((_a)[(2 * (_i)) + 1] - (_a)[2 * (_i)]) + 1)

static size_t a6xx_snapshot_registers(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header = (struct kgsl_snapshot_regs *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int *src = (unsigned int *)a6xx_crashdump_registers.hostptr;
	unsigned int i, j, k;
	unsigned int count = 0;

	if (crash_dump_valid == false)
		return a6xx_legacy_snapshot_registers(device, buf, remain);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	for (i = 0; i < ARRAY_SIZE(_a6xx_cd_registers); i++) {
		struct cdregs *regs = &_a6xx_cd_registers[i];

		for (j = 0; j < regs->size / 2; j++) {
			unsigned int start = regs->regs[2 * j];
			unsigned int end = regs->regs[(2 * j) + 1];

			if (remain < ((end - start) + 1) * 8) {
				SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
				goto out;
			}

			remain -= ((end - start) + 1) * 8;

			for (k = start; k <= end; k++, count++) {
				*data++ = k;
				*data++ = *src++;
			}
		}
	}

out:
	header->count = count;

	/* Return the size of the section */
	return (count * 8) + sizeof(*header);
}

static size_t a6xx_legacy_snapshot_mvc(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
					(struct kgsl_snapshot_mvc_regs *)buf;
	struct a6xx_cluster_regs_info *info =
					(struct a6xx_cluster_regs_info *)priv;
	struct a6xx_cluster_registers *cur_cluster = info->cluster;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int ctxt = info->ctxt_id;
	unsigned int start, end, i, j, aperture_cntl = 0;
	unsigned int data_size = 0;

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	header->ctxt_id = info->ctxt_id;
	header->cluster_id = cur_cluster->id;

	/*
	 * Set the AHB control for the Host to read from the
	 * cluster/context for this iteration.
	 */
	aperture_cntl = ((cur_cluster->id & 0x7) << 8) | (ctxt << 4) | ctxt;
	kgsl_regwrite(device, A6XX_CP_APERTURE_CNTL_HOST, aperture_cntl);

	for (i = 0; i < cur_cluster->num_sets; i++) {
		start = cur_cluster->regs[2 * i];
		end = cur_cluster->regs[2 * i + 1];

		if (remain < (end - start + 3) * 4) {
			SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
			goto out;
		}

		remain -= (end - start + 3) * 4;
		data_size += (end - start + 3) * 4;

		*data++ = start | (1 << 31);
		*data++ = end;
		for (j = start; j <= end; j++) {
			unsigned int val;

			kgsl_regread(device, j, &val);
			*data++ = val;
		}
	}
out:
	return data_size + sizeof(*header);
}

static size_t a6xx_snapshot_mvc(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
				(struct kgsl_snapshot_mvc_regs *)buf;
	struct a6xx_cluster_regs_info *info =
				(struct a6xx_cluster_regs_info *)priv;
	struct a6xx_cluster_registers *cluster = info->cluster;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int *src;
	int i, j;
	unsigned int start, end;
	size_t data_size = 0;

	if (crash_dump_valid == false)
		return a6xx_legacy_snapshot_mvc(device, buf, remain, info);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	header->ctxt_id = info->ctxt_id;
	header->cluster_id = cluster->id;

	src = (unsigned int *)(a6xx_crashdump_registers.hostptr +
		(header->ctxt_id ? cluster->offset1 : cluster->offset0));

	for (i = 0; i < cluster->num_sets; i++) {
		start = cluster->regs[2 * i];
		end = cluster->regs[2 * i + 1];

		if (remain < (end - start + 3) * 4) {
			SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
			goto out;
		}

		remain -= (end - start + 3) * 4;
		data_size += (end - start + 3) * 4;

		*data++ = start | (1 << 31);
		*data++ = end;
		for (j = start; j <= end; j++)
			*data++ = *src++;
	}

out:
	return data_size + sizeof(*header);

}

static void a6xx_snapshot_mvc_regs(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	int i, j;
	struct a6xx_cluster_regs_info info;

	for (i = 0; i < ARRAY_SIZE(a6xx_clusters); i++) {
		struct a6xx_cluster_registers *cluster = &a6xx_clusters[i];

		info.cluster = cluster;
		for (j = 0; j < A6XX_NUM_CTXTS; j++) {
			info.ctxt_id = j;

			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_MVC, snapshot,
				a6xx_snapshot_mvc, &info);
		}
	}
}

static void _a6xx_do_crashdump(struct kgsl_device *device)
{
	unsigned long wait_time;
	unsigned int reg = 0;
	unsigned int val;

	crash_dump_valid = false;

	if (a6xx_capturescript.gpuaddr == 0 ||
		a6xx_crashdump_registers.gpuaddr == 0)
		return;

	/* IF the SMMU is stalled we cannot do a crash dump */
	kgsl_regread(device, A6XX_RBBM_STATUS3, &val);
	if (val & BIT(24))
		return;

	/* Turn on APRIV so we can access the buffers */
	kgsl_regwrite(device, A6XX_CP_MISC_CNTL, 1);

	kgsl_regwrite(device, A6XX_CP_CRASH_SCRIPT_BASE_LO,
			lower_32_bits(a6xx_capturescript.gpuaddr));
	kgsl_regwrite(device, A6XX_CP_CRASH_SCRIPT_BASE_HI,
			upper_32_bits(a6xx_capturescript.gpuaddr));
	kgsl_regwrite(device, A6XX_CP_CRASH_DUMP_CNTL, 1);

	wait_time = jiffies + msecs_to_jiffies(CP_CRASH_DUMPER_TIMEOUT);
	while (!time_after(jiffies, wait_time)) {
		kgsl_regread(device, A6XX_CP_CRASH_DUMP_STATUS, &reg);
		if (reg & 0x2)
			break;
		cpu_relax();
	}

	kgsl_regwrite(device, A6XX_CP_MISC_CNTL, 0);

	if (!(reg & 0x2)) {
		KGSL_CORE_ERR("Crash dump timed out: 0x%X\n", reg);
		return;
	}

	crash_dump_valid = true;
}

/*
 * a6xx_snapshot() - A6XX GPU snapshot function
 * @adreno_dev: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the A6XX specific bits and pieces are grabbed
 * into the snapshot memory
 */
void a6xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_snapshot_data *snap_data = gpudev->snapshot_data;

	/* Try to run the crash dumper */
	_a6xx_do_crashdump(device);

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
		snapshot, a6xx_snapshot_registers, NULL);

	adreno_snapshot_vbif_registers(device, snapshot,
		a6xx_vbif_snapshot_registers,
		ARRAY_SIZE(a6xx_vbif_snapshot_registers));

	/* CP_SQE indexed registers */
	kgsl_snapshot_indexed_registers(device, snapshot,
		A6XX_CP_SQE_STAT_ADDR, A6XX_CP_SQE_STAT_DATA,
		0, snap_data->sect_sizes->cp_pfp);

	/* CP_DRAW_STATE */
	kgsl_snapshot_indexed_registers(device, snapshot,
		A6XX_CP_DRAW_STATE_ADDR, A6XX_CP_DRAW_STATE_DATA,
		0, 0x100);

	 /* SQE_UCODE Cache */
	kgsl_snapshot_indexed_registers(device, snapshot,
		A6XX_CP_SQE_UCODE_DBG_ADDR, A6XX_CP_SQE_UCODE_DBG_DATA,
		0, 0x6000);

	/* CP ROQ */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, adreno_snapshot_cp_roq,
		&snap_data->sect_sizes->roq);

	/* MVC register section */
	a6xx_snapshot_mvc_regs(device, snapshot);

}

static int _a6xx_crashdump_init_mvc(uint64_t *ptr, uint64_t *offset)
{
	int qwords = 0;
	unsigned int i, j, k;
	unsigned int count;

	for (i = 0; i < ARRAY_SIZE(a6xx_clusters); i++) {
		struct a6xx_cluster_registers *cluster = &a6xx_clusters[i];

		cluster->offset0 = *offset;
		for (j = 0; j < A6XX_NUM_CTXTS; j++) {

			if (j == 1)
				cluster->offset1 = *offset;

			ptr[qwords++] = (cluster->id << 8) | (j << 4) | j;
			ptr[qwords++] =
				((uint64_t)A6XX_CP_APERTURE_CNTL_HOST << 44) |
				(1 << 21) | 1;

			for (k = 0; k < cluster->num_sets; k++) {
				count = REG_PAIR_COUNT(cluster->regs, k);
				ptr[qwords++] =
				a6xx_crashdump_registers.gpuaddr + *offset;
				ptr[qwords++] =
				(((uint64_t)cluster->regs[2 * k]) << 44) |
						count;

				*offset += count * sizeof(unsigned int);
			}
		}
	}

	return qwords;
}

void a6xx_crashdump_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int script_size = 0;
	unsigned int data_size = 0;
	unsigned int i, j, k;
	uint64_t *ptr;
	uint64_t offset = 0;

	if (a6xx_capturescript.gpuaddr != 0 &&
		a6xx_crashdump_registers.gpuaddr != 0)
		return;

	/*
	 * We need to allocate two buffers:
	 * 1 - the buffer to hold the draw script
	 * 2 - the buffer to hold the data
	 */

	/*
	 * To save the registers, we need 16 bytes per register pair for the
	 * script and a dword for each register in the data
	 */
	for (i = 0; i < ARRAY_SIZE(_a6xx_cd_registers); i++) {
		struct cdregs *regs = &_a6xx_cd_registers[i];

		/* Each pair needs 16 bytes (2 qwords) */
		script_size += (regs->size / 2) * 16;

		/* Each register needs a dword in the data */
		for (j = 0; j < regs->size / 2; j++)
			data_size += REG_PAIR_COUNT(regs->regs, j) *
				sizeof(unsigned int);

	}

	/* Calculate the script and data size for MVC registers */
	for (i = 0; i < ARRAY_SIZE(a6xx_clusters); i++) {
		struct a6xx_cluster_registers *cluster = &a6xx_clusters[i];

		for (j = 0; j < A6XX_NUM_CTXTS; j++) {

			/* 16 bytes for programming the aperture */
			script_size += 16;

			/* Reading each pair of registers takes 16 bytes */
			script_size += 16 * cluster->num_sets;

			/* A dword per register read from the cluster list */
			for (k = 0; k < cluster->num_sets; k++)
				data_size += REG_PAIR_COUNT(cluster->regs, k) *
						sizeof(unsigned int);
		}
	}

	/* Now allocate the script and data buffers */

	/* The script buffers needs 2 extra qwords on the end */
	if (kgsl_allocate_global(device, &a6xx_capturescript,
		script_size + 16, KGSL_MEMFLAGS_GPUREADONLY,
		KGSL_MEMDESC_PRIVILEGED, "capturescript"))
		return;

	if (kgsl_allocate_global(device, &a6xx_crashdump_registers, data_size,
		0, KGSL_MEMDESC_PRIVILEGED, "capturescript_regs")) {
		kgsl_free_global(KGSL_DEVICE(adreno_dev), &a6xx_capturescript);
		return;
	}

	/* Build the crash script */

	ptr = (uint64_t *)a6xx_capturescript.hostptr;

	/* For the registers, program a read command for each pair */
	for (i = 0; i < ARRAY_SIZE(_a6xx_cd_registers); i++) {
		struct cdregs *regs = &_a6xx_cd_registers[i];

		for (j = 0; j < regs->size / 2; j++) {
			unsigned int r = REG_PAIR_COUNT(regs->regs, j);
			*ptr++ = a6xx_crashdump_registers.gpuaddr + offset;
			*ptr++ = (((uint64_t) regs->regs[2 * j]) << 44) | r;
			offset += r * sizeof(unsigned int);
		}
	}

	/* Program the capturescript for the MVC regsiters */
	ptr += _a6xx_crashdump_init_mvc(ptr, &offset);

	*ptr++ = 0;
	*ptr++ = 0;
}
