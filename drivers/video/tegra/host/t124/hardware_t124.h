/*
 * drivers/video/tegra/host/t124/hardware_t124.h
 *
 * Tegra T124 HOST1X Register Definitions
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __NVHOST_HARDWARE_T124_H
#define __NVHOST_HARDWARE_T124_H

#include "host1x/hw_host1x04_sync.h"
#include "host1x/hw_host1x04_uclass.h"
#include "host1x/hw_host1x04_channel.h"
#include "host1x/hw_host1x04_actmon.h"

#define NV_HOST1X_CHANNELS	9
#define NV_HOST1X_SYNC_MLOCK_NUM 16

/* sync registers */
#define NV_HOST1X_SYNCPT_NB_PTS 192
#define NV_HOST1X_SYNCPT_NB_BASES 64
#define NV_HOST1X_NB_MLOCKS 16
#define HOST1X_CHANNEL_SYNC_REG_BASE 0x2100
#define NV_HOST1X_CHANNEL_MAP_SIZE_BYTES 16384

/* actmon regs */
#define HOST1X_CHANNEL_ACTMON1_REG_BASE 8192
#define HOST1X_CHANNEL_ACTMON2_REG_BASE 8256

/* Generic support */
static inline u32 nvhost_class_host_wait_syncpt(
	unsigned indx, unsigned threshold)
{
	return (indx << 24) | (threshold & 0xffffff);
}

static inline u32 nvhost_class_host_load_syncpt_base(
	unsigned indx, unsigned threshold)
{
	return host1x_uclass_wait_syncpt_indx_f(indx)
		| host1x_uclass_wait_syncpt_thresh_f(threshold);
}

static inline u32 nvhost_class_host_wait_syncpt_base(
	unsigned indx, unsigned base_indx, unsigned offset)
{
	return host1x_uclass_wait_syncpt_base_indx_f(indx)
		| host1x_uclass_wait_syncpt_base_base_indx_f(base_indx)
		| host1x_uclass_wait_syncpt_base_offset_f(offset);
}

static inline u32 nvhost_class_host_incr_syncpt_base(
	unsigned base_indx, unsigned offset)
{
	return host1x_uclass_incr_syncpt_base_base_indx_f(base_indx)
		| host1x_uclass_incr_syncpt_base_offset_f(offset);
}

static inline u32 nvhost_class_host_incr_syncpt(
	unsigned cond, unsigned indx)
{
	return host1x_uclass_incr_syncpt_cond_f(cond)
		| host1x_uclass_incr_syncpt_indx_f(indx);
}

enum {
	NV_HOST_MODULE_HOST1X = 0,
	NV_HOST_MODULE_MPE = 1,
	NV_HOST_MODULE_GR3D = 6
};

static inline u32 nvhost_class_host_indoff_reg_write(
	unsigned mod_id, unsigned offset, bool auto_inc)
{
	u32 v = host1x_uclass_indoff_indbe_f(0xf)
		| host1x_uclass_indoff_indmodid_f(mod_id)
		| host1x_uclass_indoff_indroffset_f(offset);
	if (auto_inc)
		v |= host1x_uclass_indoff_autoinc_f(1);
	return v;
}

static inline u32 nvhost_class_host_indoff_reg_read(
	unsigned mod_id, unsigned offset, bool auto_inc)
{
	u32 v = host1x_uclass_indoff_indmodid_f(mod_id)
		| host1x_uclass_indoff_indroffset_f(offset)
		| host1x_uclass_indoff_rwn_read_v();
	if (auto_inc)
		v |= host1x_uclass_indoff_autoinc_f(1);
	return v;
}

/* cdma opcodes */
static inline u32 nvhost_opcode_setclass(
	unsigned class_id, unsigned offset, unsigned mask)
{
	return (0 << 28) | (offset << 16) | (class_id << 6) | mask;
}

static inline u32 nvhost_opcode_incr(unsigned offset, unsigned count)
{
	return (1 << 28) | (offset << 16) | count;
}

static inline u32 nvhost_opcode_nonincr(unsigned offset, unsigned count)
{
	return (2 << 28) | (offset << 16) | count;
}

static inline u32 nvhost_opcode_mask(unsigned offset, unsigned mask)
{
	return (3 << 28) | (offset << 16) | mask;
}

static inline u32 nvhost_opcode_imm(unsigned offset, unsigned value)
{
	return (4 << 28) | (offset << 16) | value;
}

static inline u32 nvhost_opcode_imm_incr_syncpt(unsigned cond, unsigned indx)
{
	return nvhost_opcode_imm(host1x_uclass_incr_syncpt_r(),
		nvhost_class_host_incr_syncpt(cond, indx));
}

static inline u32 nvhost_opcode_restart(unsigned address)
{
	return (5 << 28) | (address >> 4);
}

static inline u32 nvhost_opcode_gather(unsigned count)
{
	return (6 << 28) | count;
}

static inline u32 nvhost_opcode_gather_nonincr(unsigned offset,	unsigned count)
{
	return (6 << 28) | (offset << 16) | BIT(15) | count;
}

static inline u32 nvhost_opcode_gather_incr(unsigned offset, unsigned count)
{
	return (6 << 28) | (offset << 16) | BIT(15) | BIT(14) | count;
}

static inline u32 nvhost_opcode_gather_insert(unsigned offset, unsigned incr,
		unsigned count)
{
	return (6 << 28) | (offset << 16) | BIT(15) | (incr << 14) | count;
}

#define NVHOST_OPCODE_NOOP nvhost_opcode_nonincr(0, 0)

static inline u32 nvhost_mask2(unsigned x, unsigned y)
{
	return 1 | (1 << (y - x));
}

#endif /* __NVHOST_HARDWARE_T124_H */
