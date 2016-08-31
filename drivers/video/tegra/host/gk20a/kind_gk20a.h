/*
 * drivers/video/tegra/host/gk20a/kind_gk20a.h
 *
 * GK20A memory kind management
 *
 * Copyright (c) 2011, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef __KIND_GK20A_H__
#define __KIND_GK20A_H__


void gk20a_init_uncompressed_kind_map(void);
void gk20a_init_kind_attr(void);

extern u16 gk20a_kind_attr[];
#define GK20A_KIND_ATTR_SUPPORTED    BIT(0)
#define GK20A_KIND_ATTR_COMPRESSIBLE BIT(1)
#define GK20A_KIND_ATTR_Z            BIT(2)
#define GK20A_KIND_ATTR_C            BIT(3)
#define GK20A_KIND_ATTR_ZBC          BIT(4)

static inline bool gk20a_kind_is_supported(u8 k)
{
	return !!(gk20a_kind_attr[k] & GK20A_KIND_ATTR_SUPPORTED);
}
static inline bool gk20a_kind_is_compressible(u8 k)
{
	return !!(gk20a_kind_attr[k] & GK20A_KIND_ATTR_COMPRESSIBLE);
}

static inline bool gk20a_kind_is_z(u8 k)
{
	return !!(gk20a_kind_attr[k] & GK20A_KIND_ATTR_Z);
}

static inline bool gk20a_kind_is_c(u8 k)
{
	return !!(gk20a_kind_attr[k] & GK20A_KIND_ATTR_C);
}
static inline bool gk20a_kind_is_zbc(u8 k)
{
	return !!(gk20a_kind_attr[k] & GK20A_KIND_ATTR_ZBC);
}

/* maps kind to its uncompressed version */
extern u8 gk20a_uc_kind_map[];
static inline u8 gk20a_get_uncompressed_kind(u8 k)
{
	return gk20a_uc_kind_map[k];
}

#endif /* __KIND_GK20A_H__ */
