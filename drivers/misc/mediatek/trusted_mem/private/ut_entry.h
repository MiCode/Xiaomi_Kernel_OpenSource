/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TMEM_UT_ENTRY_H
#define TMEM_UT_ENTRY_H

#include "private/tmem_device.h"

int tmem_core_session_open(enum TRUSTED_MEM_TYPE mem_type);
int tmem_core_session_close(enum TRUSTED_MEM_TYPE mem_type);
int tmem_core_ssmr_allocate(enum TRUSTED_MEM_TYPE mem_type);
int tmem_core_ssmr_release(enum TRUSTED_MEM_TYPE mem_type);
int tmem_core_alloc_chunk(enum TRUSTED_MEM_TYPE mem_type, u32 alignment,
			  u32 size, u32 *refcount, u32 *sec_handle, u8 *owner,
			  u32 id, u32 clean);
int tmem_core_alloc_chunk_priv(enum TRUSTED_MEM_TYPE mem_type, u32 alignment,
			       u32 size, u32 *refcount, u32 *sec_handle,
			       u8 *owner, u32 id, u32 clean);
int tmem_core_unref_chunk(enum TRUSTED_MEM_TYPE mem_type, u32 sec_handle,
			  u8 *owner, u32 id);

bool tmem_core_is_regmgr_region_on(enum TRUSTED_MEM_TYPE mem_type);
u64 tmem_core_get_regmgr_region_online_cnt(enum TRUSTED_MEM_TYPE mem_type);
u32 tmem_core_get_regmgr_region_ref_cnt(enum TRUSTED_MEM_TYPE mem_type);
int tmem_core_regmgr_online(enum TRUSTED_MEM_TYPE mem_type);
int tmem_core_regmgr_offline(enum TRUSTED_MEM_TYPE mem_type);

bool tmem_core_is_device_registered(enum TRUSTED_MEM_TYPE mem_type);
u32 tmem_core_get_min_chunk_size(enum TRUSTED_MEM_TYPE mem_type);
u32 tmem_core_get_max_pool_size(enum TRUSTED_MEM_TYPE mem_type);
bool tmem_core_get_region_info(enum TRUSTED_MEM_TYPE mem_type, u64 *pa,
			       u32 *size);

#endif /* end of TMEM_UT_ENTRY_H */
