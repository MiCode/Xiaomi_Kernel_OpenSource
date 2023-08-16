/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2013-2015,2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __ADRENO_SNAPSHOT_H
#define __ADRENO_SNAPSHOT_H

#include "kgsl_snapshot.h"

#define CP_CRASH_DUMPER_TIMEOUT 500

#define DEBUG_SECTION_SZ(_dwords) (((_dwords) * sizeof(unsigned int)) \
		+ sizeof(struct kgsl_snapshot_debug))

#define SHADER_SECTION_SZ(_dwords) (((_dwords) * sizeof(unsigned int)) \
		+ sizeof(struct kgsl_snapshot_shader))

/* Macro to make it super easy to dump registers */
#define SNAPSHOT_REGISTERS(_d, _s, _r) \
	adreno_snapshot_registers((_d), (_s), \
		(unsigned int *) _r, ARRAY_SIZE(_r) /  2)

#define REG_COUNT(_ptr) ((_ptr[1] - _ptr[0]) + 1)

void adreno_snapshot_registers(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		const unsigned int *regs, unsigned int count);

/**
 * adreno_snapshot_regs_count - Helper function to calculate register and
 * header size
 * @ptr: Pointer to the register array
 *
 * Return: Number of registers in the array
 *
 * Helper function to count the total number of regsiters
 * in a given array plus the header space needed for each group.
 */
int adreno_snapshot_regs_count(const u32 *ptr);

/**
 * adreno_snapshot_registers_v2 - Dump a series of registers
 * @device: Pointer to the kgsl device
 * @buf: The snapshot buffer
 * @remain: The size remaining in the snapshot buffer
 * @priv: Pointer to the register array to be dumped
 *
 * Return: Number of bytes written to the snapshot
 *
 * This function dumps the registers in a way that we need to
 * only dump the start address and count for each pair of register
 * in the array. This helps us save some memory in snapshot.
 */
size_t adreno_snapshot_registers_v2(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv);

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
 * adreno_parse_ib_lpac - Parse the given LPAC IB
 * @device: Pointer to the kgsl device
 * @snapshot: Pointer to the snapshot structure
 * @process: Process to which this LPAC IB belongs
 * @gpuaddr: Gpu address of the LPAC IB
 * @dwords: Size in dwords of the LPAC IB
 *
 * We want to store the last executed LPAC IB1 and IB2 in the static region to ensure
 * that we get at least some information out of the snapshot even if we can't
 * access the dynamic data from the sysfs file.  Push all other IBs on the
 * dynamic list
 */
void adreno_parse_ib_lpac(struct kgsl_device *device,
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

/**
 * adreno_snapshot_dump_all_ibs - To dump all ibs from ringbuffer
 * @device: Pointer to the kgsl device
 * @rbptr: Ringbuffer host pointer
 * @snapshot: Pointer to the snapshot structure
 *
 * Parse all IBs from the ringbuffer and add to IB dump list.
 */
void adreno_snapshot_dump_all_ibs(struct kgsl_device *device,
			unsigned int *rbptr,
			struct kgsl_snapshot *snapshot);

#endif /*__ADRENO_SNAPSHOT_H */
