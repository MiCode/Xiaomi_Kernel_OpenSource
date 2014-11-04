/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */



/* The name "mmu.h is already taken" */
#include "mmu_device.h"

#ifndef __INLINE_MMU__
#include "mmu_private.h"
#endif /* __INLINE_MMU__ */

void mmu_set_page_table_base_index(
	const mmu_ID_t		ID,
	const hrt_data		base_index)
{
	mmu_reg_store(ID, _HRT_MMU_PAGE_TABLE_BASE_ADDRESS_REG_IDX, base_index);
return;
}

hrt_data mmu_get_page_table_base_index(
	const mmu_ID_t		ID)
{
return mmu_reg_load(ID, _HRT_MMU_PAGE_TABLE_BASE_ADDRESS_REG_IDX);
}

void mmu_invalidate_cache(
	const mmu_ID_t		ID)
{
	mmu_reg_store(ID, _HRT_MMU_INVALIDATE_TLB_REG_IDX, 1);
return;
}

void mmu_invalidate_cache_all(void)
{
	mmu_ID_t	mmu_id;
	for (mmu_id = (mmu_ID_t)0;mmu_id < N_MMU_ID; mmu_id++) {
		mmu_invalidate_cache(mmu_id);
	}
}

