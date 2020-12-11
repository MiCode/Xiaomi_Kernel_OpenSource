/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2013-2015,2020, The Linux Foundation. All rights reserved.
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

size_t adreno_snapshot_cp_roq(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv);
size_t adreno_snapshot_cp_meq(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv);

void adreno_snapshot_registers(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		const unsigned int *regs, unsigned int count);

void adreno_snapshot_vbif_registers(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		const struct adreno_vbif_snapshot_registers *list,
		unsigned int count);

/**
 * adreno_parse_ib - Parse the given IB
 * @device: Pointer to the kgsl device
 * @snapshot: Pointer to the snapshot structure
 * @process: Process to which this IB belongs
 * @gpuaddr: Gpu address of the IB
 * @dwords: Size in dwords of the IB
 *
 * We want to store the last executed IB1 and IB2 in the static region to ensure
 * that we get at least some information out of the snapshot even if we can't
 * access the dynamic data from the sysfs file.  Push all other IBs on the
 * dynamic list
 */
void adreno_parse_ib(struct kgsl_device *device,
	struct kgsl_snapshot *snapshot,
	struct kgsl_process_private *process,
	u64 gpuaddr, u64 dwords);
/**
 * adreno_snapshot_global - Add global buffer to snapshot
 * @device: Pointer to the kgsl device
 * @buf: Where the global buffer section is to be written
 * @remain: Remaining bytes in snapshot buffer
 * @priv: Opaque data
 *
 * Return: Number of bytes written to the snapshot buffer
 */
size_t adreno_snapshot_global(struct kgsl_device *device, u8 *buf,
	size_t remain, void *priv);
#endif /*__ADRENO_SNAPSHOT_H */
