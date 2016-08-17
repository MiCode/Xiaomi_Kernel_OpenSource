/*
 * arch/arm/mach-tegra/la_priv_common.h
 *
 * Copyright (C) 2012 NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MACH_TEGRA_LA_PRIV_H_
#define _MACH_TEGRA_LA_PRIV_H_

/* maximum valid value for latency allowance */
#define MC_LA_MAX_VALUE		255

#define MC_RA(r) \
	((u32)IO_ADDRESS(TEGRA_MC_BASE) + (MC_##r))
#define RA(r) \
	((u32)IO_ADDRESS(TEGRA_MC_BASE) + (MC_LA_##r))

#define MASK(x) \
	((0xFFFFFFFFUL >> (31 - (1 ? x) + (0 ? x))) << (0 ? x))
#define SHIFT(x) \
	(0 ? x)
#define ID(id) \
	TEGRA_LA_##id

#define LA_INFO(f, e, a, r, i, ss, la) \
{ \
	.fifo_size_in_atoms = f, \
	.expiration_in_ns = e, \
	.reg_addr = RA(a), \
	.mask = MASK(r), \
	.shift = SHIFT(r), \
	.id = ID(i), \
	.name = __stringify(i), \
	.scaling_supported = ss, \
	.init_la = la, \
}

struct la_client_info {
	unsigned int fifo_size_in_atoms;
	unsigned int expiration_in_ns;	/* worst case expiration value */
	unsigned long reg_addr;
	unsigned long mask;
	unsigned long shift;
	enum tegra_la_id id;
	char *name;
	bool scaling_supported;
	unsigned int init_la;		/* initial la to set for client */
};

struct la_scaling_info {
	unsigned int threshold_low;
	unsigned int threshold_mid;
	unsigned int threshold_high;
	int scaling_ref_count;
	int actual_la_to_set;
	int la_set;
};

struct la_scaling_reg_info {
	enum tegra_la_id id;
	unsigned int tl_reg_addr;
	unsigned int tl_mask;
	unsigned int tl_shift;
	unsigned int tm_reg_addr;
	unsigned int tm_mask;
	unsigned int tm_shift;
	unsigned int th_reg_addr;
	unsigned int th_mask;
	unsigned int th_shift;
};

#endif /* _MACH_TEGRA_LA_PRIV_H_ */
