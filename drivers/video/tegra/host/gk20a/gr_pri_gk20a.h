/*
 * GK20A Graphics Context Pri Register Addressing
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _NVHOST_GR_PRI_GK20A_H_
#define _NVHOST_GR_PRI_GK20A_H_

/*
 * These convenience macros are generally for use in the management/modificaiton
 * of the context state store for gr/compute contexts.
 */

/*
 * GPC pri addressing
 */
static inline u32 pri_gpccs_addr_width(void)
{
	return 15; /*from where?*/
}
static inline u32 pri_gpccs_addr_mask(u32 addr)
{
	return addr & ((1 << pri_gpccs_addr_width()) - 1);
}
static inline u32 pri_gpc_addr(u32 addr, u32 gpc)
{
	return proj_gpc_base_v() + (gpc * proj_gpc_stride_v()) + addr;
}
static inline bool pri_is_gpc_addr_shared(u32 addr)
{
	return (addr >= proj_gpc_shared_base_v()) &&
		(addr < proj_gpc_shared_base_v() + proj_gpc_stride_v());
}
static inline bool pri_is_gpc_addr(u32 addr)
{
	return	((addr >= proj_gpc_base_v()) &&
		 (addr < proj_gpc_base_v() +
		  proj_scal_litter_num_gpcs_v() * proj_gpc_stride_v())) ||
		pri_is_gpc_addr_shared(addr);
}
static inline u32 pri_get_gpc_num(u32 addr)
{
	u32 i, start;
	u32 num_gpcs = proj_scal_litter_num_gpcs_v();

	for (i = 0; i < num_gpcs; i++) {
		start = proj_gpc_base_v() + (i * proj_gpc_stride_v());
		if ((addr >= start) && (addr < (start + proj_gpc_stride_v())))
			return i;
	}
	return 0;
}
/*
 * TPC pri addressing
 */
static inline u32 pri_tpccs_addr_width(void)
{
	return 11; /* from where? */
}
static inline u32 pri_tpccs_addr_mask(u32 addr)
{
	return addr & ((1 << pri_tpccs_addr_width()) - 1);
}
static inline u32 pri_tpc_addr(u32 addr, u32 gpc, u32 tpc)
{
	return proj_gpc_base_v() + (gpc * proj_gpc_stride_v()) +
		proj_tpc_in_gpc_base_v() + (tpc * proj_tpc_in_gpc_stride_v()) +
		addr;
}
static inline bool pri_is_tpc_addr_shared(u32 addr)
{
	return (addr >= proj_tpc_in_gpc_shared_base_v()) &&
		(addr < (proj_tpc_in_gpc_shared_base_v() +
			 proj_tpc_in_gpc_stride_v()));
}
static inline bool pri_is_tpc_addr(u32 addr)
{
	return ((addr >= proj_tpc_in_gpc_base_v()) &&
		(addr < proj_tpc_in_gpc_base_v() + (proj_scal_litter_num_tpc_per_gpc_v() *
						    proj_tpc_in_gpc_stride_v())))
		||
		pri_is_tpc_addr_shared(addr);
}
static inline u32 pri_get_tpc_num(u32 addr)
{
	u32 i, start;
	u32 num_tpcs = proj_scal_litter_num_tpc_per_gpc_v();

	for (i = 0; i < num_tpcs; i++) {
		start = proj_tpc_in_gpc_base_v() + (i * proj_tpc_in_gpc_stride_v());
		if ((addr >= start) && (addr < (start + proj_tpc_in_gpc_stride_v())))
			return i;
	}
	return 0;
}

/*
 * BE pri addressing
 */
static inline u32 pri_becs_addr_width(void)
{
	return 10;/* from where? */
}
static inline u32 pri_becs_addr_mask(u32 addr)
{
	return addr & ((1 << pri_becs_addr_width()) - 1);
}
static inline bool pri_is_be_addr_shared(u32 addr)
{
	return (addr >= proj_rop_shared_base_v()) &&
		(addr < proj_rop_shared_base_v() + proj_rop_stride_v());
}
static inline u32 pri_be_shared_addr(u32 addr)
{
	return proj_rop_shared_base_v() + pri_becs_addr_mask(addr);
}
static inline bool pri_is_be_addr(u32 addr)
{
	return	((addr >= proj_rop_base_v()) &&
		 (addr < proj_rop_base_v()+proj_scal_litter_num_fbps_v() * proj_rop_stride_v())) ||
		pri_is_be_addr_shared(addr);
}

static inline u32 pri_get_be_num(u32 addr)
{
	u32 i, start;
	u32 num_fbps = proj_scal_litter_num_fbps_v();
	for (i = 0; i < num_fbps; i++) {
		start = proj_rop_base_v() + (i * proj_rop_stride_v());
		if ((addr >= start) && (addr < (start + proj_rop_stride_v())))
			return i;
	}
	return 0;
}

/*
 * PPC pri addressing
 */
static inline u32 pri_ppccs_addr_width(void)
{
	return 9; /* from where? */
}
static inline u32 pri_ppccs_addr_mask(u32 addr)
{
	return addr & ((1 << pri_ppccs_addr_width()) - 1);
}
static inline u32 pri_ppc_addr(u32 addr, u32 gpc, u32 ppc)
{
	return proj_gpc_base_v() + (gpc * proj_gpc_stride_v()) +
		proj_ppc_in_gpc_base_v() + (ppc * proj_ppc_in_gpc_stride_v()) + addr;
}

enum ctxsw_addr_type {
	CTXSW_ADDR_TYPE_SYS = 0,
	CTXSW_ADDR_TYPE_GPC = 1,
	CTXSW_ADDR_TYPE_TPC = 2,
	CTXSW_ADDR_TYPE_BE  = 3,
	CTXSW_ADDR_TYPE_PPC = 4
};

#define PRI_BROADCAST_FLAGS_NONE  0
#define PRI_BROADCAST_FLAGS_GPC   BIT(0)
#define PRI_BROADCAST_FLAGS_TPC   BIT(1)
#define PRI_BROADCAST_FLAGS_BE    BIT(2)
#define PRI_BROADCAST_FLAGS_PPC   BIT(3)

#endif /*_NVHOST_GR_PRI_GK20A_H_ */
