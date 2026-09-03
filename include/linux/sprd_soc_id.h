// SPDX-License-Identifier: GPL-2.0-only
/*
 * sprd_soc_id.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __SPRD_SOC_ID_DRV_H__
#define __SPRD_SOC_ID_DRV_H__

typedef enum {
	AON_CHIP_ID = 0,
	AON_PLAT_ID,
	AON_IMPL_ID,
	AON_MFT_ID,
	AON_VER_ID,
} sprd_soc_id_type_t;

extern int sprd_get_soc_id(sprd_soc_id_type_t soc_id_type, u32 *id, int id_len);

#endif
