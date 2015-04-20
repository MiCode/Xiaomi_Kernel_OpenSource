/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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
#ifndef __ADRENO_SNAPSHOT_H
#define __ADRENO_SNAPSHOT_H

#include "kgsl_snapshot.h"

#define DEBUG_SECTION_SZ(_dwords) (((_dwords) * sizeof(unsigned int)) \
		+ sizeof(struct kgsl_snapshot_debug))

#define SHADER_SECTION_SZ(_dwords) (((_dwords) * sizeof(unsigned int)) \
		+ sizeof(struct kgsl_snapshot_shader))

/* Section sizes for A320 */
#define A320_SNAPSHOT_CP_STATE_SECTION_SIZE	0x2e
#define A320_SNAPSHOT_ROQ_SECTION_SIZE		512
#define A320_SNAPSHOT_CP_MERCIU_SECTION_SIZE	32

/* snapshot functions used for A4XX as well */
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

static inline void adreno_snapshot_regs(struct kgsl_snapshot_registers *regs,
	struct kgsl_snapshot_registers_list *list,
	const unsigned int *registers,
	const unsigned int count, int dump)
{
	regs[list->count].regs = (unsigned int *)registers;
	regs[list->count].count = count;
	regs[list->count].dump = dump;
	regs[list->count].snap_addr = NULL;
	list->count++;
}
#endif /*__ADRENO_SNAPSHOT_H */
