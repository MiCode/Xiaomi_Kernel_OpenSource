/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 */
#ifndef __ADRENO_SNAPSHOT_H
#define __ADRENO_SNAPSHOT_H

#include "kgsl_snapshot.h"

#define CP_CRASH_DUMPER_TIMEOUT 1000

#define DEBUG_SECTION_SZ(_dwords) (((_dwords) * sizeof(unsigned int)) \
		+ sizeof(struct kgsl_snapshot_debug))

#define SHADER_SECTION_SZ(_dwords) (((_dwords) * sizeof(unsigned int)) \
		+ sizeof(struct kgsl_snapshot_shader))

/* Section sizes for A320 */
#define A320_SNAPSHOT_CP_STATE_SECTION_SIZE	0x2e
#define A320_SNAPSHOT_ROQ_SECTION_SIZE		512
#define A320_SNAPSHOT_CP_MERCIU_SECTION_SIZE	32

/* Macro to make it super easy to dump registers */
#define SNAPSHOT_REGISTERS(_d, _s, _r) \
	adreno_snapshot_registers((_d), (_s), \
		(unsigned int *) _r, ARRAY_SIZE(_r) /  2)

size_t adreno_snapshot_cp_merciu(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv);
size_t adreno_snapshot_cp_roq(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv);
size_t adreno_snapshot_cp_pm4_ram(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv);
size_t adreno_snapshot_cp_pfp_ram(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv);
size_t adreno_snapshot_cp_meq(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv);
size_t adreno_snapshot_vpc_memory(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv);

void adreno_snapshot_registers(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		const unsigned int *regs, unsigned int count);

void adreno_snapshot_vbif_registers(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		const struct adreno_vbif_snapshot_registers *list,
		unsigned int count);

#endif /*__ADRENO_SNAPSHOT_H */
