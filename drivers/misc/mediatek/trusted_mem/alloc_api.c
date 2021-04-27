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

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/version.h>

#include "private/tmem_entry.h"
#include "private/tmem_utils.h"
#include "public/trusted_mem_api.h"

static inline void trusted_mem_type_enum_validate(void)
{
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_SVP == (int)TRUSTED_MEM_SVP);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_PROT == (int)TRUSTED_MEM_PROT);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_WFD == (int)TRUSTED_MEM_WFD);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_HAPP == (int)TRUSTED_MEM_HAPP);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_HAPP_EXTRA
		       == (int)TRUSTED_MEM_HAPP_EXTRA);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_SDSP == (int)TRUSTED_MEM_SDSP);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_SDSP_SHARED
		       == (int)TRUSTED_MEM_SDSP_SHARED);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_2D_FR == (int)TRUSTED_MEM_2D_FR);
	COMPILE_ASSERT((int)(TRUSTED_MEM_MAX - 1) == (int)TRUSTED_MEM_2D_FR);
}

static inline enum TRUSTED_MEM_TYPE
get_mem_type(enum TRUSTED_MEM_REQ_TYPE req_type)
{
	trusted_mem_type_enum_validate();
	switch (req_type) {
	case TRUSTED_MEM_REQ_SVP:
		return TRUSTED_MEM_SVP;
	case TRUSTED_MEM_REQ_PROT:
		return TRUSTED_MEM_PROT;
	case TRUSTED_MEM_REQ_WFD:
		return TRUSTED_MEM_WFD;
	case TRUSTED_MEM_REQ_HAPP:
		return TRUSTED_MEM_HAPP;
	case TRUSTED_MEM_REQ_HAPP_EXTRA:
		return TRUSTED_MEM_HAPP_EXTRA;
	case TRUSTED_MEM_REQ_SDSP:
		return TRUSTED_MEM_SDSP;
	case TRUSTED_MEM_REQ_SDSP_SHARED:
		return TRUSTED_MEM_SDSP_SHARED;
	case TRUSTED_MEM_REQ_2D_FR:
		return TRUSTED_MEM_2D_FR;
	default:
		return TRUSTED_MEM_SVP;
	}
}

int trusted_mem_api_alloc(enum TRUSTED_MEM_REQ_TYPE mem_type, u32 alignment,
			  u32 size, u32 *refcount, u32 *sec_handle,
			  uint8_t *owner, uint32_t id)
{
	return tmem_core_alloc_chunk(get_mem_type(mem_type), alignment, size,
				     refcount, sec_handle, owner, id, 0);
}
EXPORT_SYMBOL(trusted_mem_api_alloc);

int trusted_mem_api_alloc_zero(enum TRUSTED_MEM_REQ_TYPE mem_type,
			       u32 alignment, u32 size, u32 *refcount,
			       u32 *sec_handle, uint8_t *owner, uint32_t id)
{
	return tmem_core_alloc_chunk(get_mem_type(mem_type), alignment, size,
				     refcount, sec_handle, owner, id, 1);
}
EXPORT_SYMBOL(trusted_mem_api_alloc_zero);

int trusted_mem_api_query_pa(enum TRUSTED_MEM_REQ_TYPE mem_type, u32 alignment,
			      u32 size, u32 *refcount, u32 *handle,
			      u8 *owner, u32 id, u32 clean, uint64_t *phy_addr)
{
#if defined(CONFIG_MTK_SVP_ON_MTEE_SUPPORT) && defined(CONFIG_MTK_GZ_KREE)
	return tmem_query_gz_handle_to_pa(get_mem_type(mem_type), alignment, size,
				refcount, handle, owner, id, 0, phy_addr);
#else
	return tmem_query_sec_handle_to_pa(get_mem_type(mem_type), alignment, size,
				refcount, handle, owner, id, 0, phy_addr);
#endif
}
EXPORT_SYMBOL(trusted_mem_api_query_pa);

int trusted_mem_api_unref(enum TRUSTED_MEM_REQ_TYPE mem_type, u32 sec_handle,
			  uint8_t *owner, uint32_t id)
{
	return tmem_core_unref_chunk(get_mem_type(mem_type), sec_handle, owner,
				     id);
}
EXPORT_SYMBOL(trusted_mem_api_unref);

bool trusted_mem_api_get_region_info(enum TRUSTED_MEM_REQ_TYPE mem_type,
				     u64 *pa, u32 *size)
{
	return tmem_core_get_region_info(get_mem_type(mem_type), pa, size);
}
EXPORT_SYMBOL(trusted_mem_api_get_region_info);
