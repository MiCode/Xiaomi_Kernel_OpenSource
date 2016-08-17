/*
 * drivers/video/tegra/host/msenc/hw_msenc.h
 *
 * Tegra MSENC Module Hardware defines
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __NVHOST_HW_MSENC_H__
#define __NVHOST_HW_MSENC_H__

#include <linux/nvhost.h>

static inline u32 msenc_irqmset_r(void)
{
	return 0x1010;
}

static inline u32 msenc_irqmset_wdtmr_set_f(void)
{
	return 0x2;
}

static inline u32 msenc_irqmset_ext_f(u32 v)
{
	return (v & 0xff) << 8;
}

static inline u32 msenc_irqmset_halt_set_f(void)
{
	return 0x10;
}

static inline u32 msenc_irqmset_exterr_set_f(void)
{
	return 0x20;
}

static inline u32 msenc_irqmset_swgen0_set_f(void)
{
	return 0x40;
}
static inline u32 msenc_irqmset_swgen1_set_f(void)
{
	return 0x80;
}

static inline u32 msenc_irqdest_r(void)
{
	return 0x101c;
}

static inline u32 msenc_irqdest_host_halt_host_f(void)
{
	return 0x10;
}
static inline u32 msenc_irqdest_host_ext_f(u32 v)
{
	return (v & 0xff) << 8;
}

static inline u32 msenc_irqdest_host_exterr_host_f(void)
{
	return 0x20;
}

static inline u32 msenc_irqdest_host_swgen0_host_f(void)
{
	return 0x40;
}

static inline u32 msenc_irqdest_host_swgen1_host_f(void)
{
	return 0x80;
}

static inline u32 msenc_itfen_r(void)
{
	return 0x1048;
}

static inline u32 msenc_itfen_ctxen_enable_f(void)
{
	return 0x1;
}

static inline u32 msenc_itfen_mthden_enable_f(void)
{
	return 0x2;
}

static inline u32 msenc_idlestate_r(void)
{
	return 0x104c;
}

static inline u32 msenc_cpuctl_r(void)
{
	return 0x1100;
}
static inline u32 msenc_cpuctl_startcpu_true_f(void)
{
	return 0x2;
}

static inline u32 msenc_bootvec_r(void)
{
	return 0x1104;
}

static inline u32 msenc_bootvec_vec_f(u32 v)
{
	return (v & 0xffffffff) << 0;
}

static inline u32 msenc_dmactl_r(void)
{
	return 0x110c;
}

static inline u32 msenc_dmatrfbase_r(void)
{
	return 0x1110;
}

static inline u32 msenc_dmatrfmoffs_r(void)
{
	return 0x1114;
}

static inline u32 msenc_dmatrfmoffs_offs_f(u32 v)
{
	return (v & 0xffff) << 0;
}

static inline u32 msenc_dmatrfcmd_r(void)
{
	return 0x1118;
}

static inline u32 msenc_dmatrfcmd_idle_v(u32 r)
{
	return (r >> 1) & 0x1;
}

static inline u32 msenc_dmatrfcmd_idle_true_v(void)
{
	return 0x00000001;
}

static inline u32 msenc_dmatrfcmd_size_256b_f(void)
{
	return 0x600;
}

static inline u32 msenc_dmatrfcmd_imem_true_f(void)
{
	return 0x10;
}

static inline u32 msenc_dmatrffboffs_r(void)
{
	return 0x111c;
}

static inline u32 msenc_dmatrffboffs_offs_f(u32 v)
{
	return (v & 0xffffffff) << 0;
}

#endif
