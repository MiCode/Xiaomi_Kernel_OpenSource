// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/version.h>

#include "ssmr/memory_ssmr.h"
#include "private/tmem_entry.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "public/trusted_mem_api.h"

static inline void trusted_mem_type_enum_validate(void)
{
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_SVP_REGION == (int)TRUSTED_MEM_SVP_REGION);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_SVP_PAGE == (int)TRUSTED_MEM_SVP_PAGE);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_PROT_REGION == (int)TRUSTED_MEM_PROT_REGION);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_PROT_PAGE == (int)TRUSTED_MEM_PROT_PAGE);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_WFD_REGION == (int)TRUSTED_MEM_WFD_REGION);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_WFD_PAGE == (int)TRUSTED_MEM_WFD_PAGE);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_HAPP == (int)TRUSTED_MEM_HAPP);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_HAPP_EXTRA
		       == (int)TRUSTED_MEM_HAPP_EXTRA);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_SDSP == (int)TRUSTED_MEM_SDSP);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_SDSP_SHARED
		       == (int)TRUSTED_MEM_SDSP_SHARED);
	COMPILE_ASSERT((int)TRUSTED_MEM_REQ_2D_FR == (int)TRUSTED_MEM_2D_FR);
	COMPILE_ASSERT((int)(TRUSTED_MEM_MAX - 1) == (int)TRUSTED_MEM_SAPU_ENGINE_SHM);
}

static inline enum TRUSTED_MEM_TYPE
get_mem_type(enum TRUSTED_MEM_REQ_TYPE req_type)
{
	trusted_mem_type_enum_validate();

	switch (req_type) {
	case TRUSTED_MEM_REQ_SVP_REGION:
		return TRUSTED_MEM_SVP_REGION;
	case TRUSTED_MEM_REQ_SVP_PAGE:
		return TRUSTED_MEM_SVP_PAGE;
	case TRUSTED_MEM_REQ_PROT_REGION:
		return TRUSTED_MEM_PROT_REGION;
	case TRUSTED_MEM_REQ_PROT_PAGE:
		return TRUSTED_MEM_PROT_PAGE;
	case TRUSTED_MEM_REQ_WFD_REGION:
		return TRUSTED_MEM_WFD_REGION;
	case TRUSTED_MEM_REQ_WFD_PAGE:
		return TRUSTED_MEM_WFD_PAGE;
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
	case TRUSTED_MEM_REQ_SAPU_DATA_SHM:
		return TRUSTED_MEM_SAPU_DATA_SHM;
	case TRUSTED_MEM_REQ_SAPU_ENGINE_SHM:
		return TRUSTED_MEM_SAPU_ENGINE_SHM;
	default:
		return TRUSTED_MEM_SVP_REGION;
	}
}

static inline enum TRUSTED_MEM_TYPE
get_region_mem_type(enum TRUSTED_MEM_TYPE req_type)
{
	switch (req_type) {
	case TRUSTED_MEM_SVP_PAGE:
		return TRUSTED_MEM_SVP_REGION;
	case TRUSTED_MEM_PROT_PAGE:
		return TRUSTED_MEM_PROT_REGION;
	case TRUSTED_MEM_WFD_PAGE:
		return TRUSTED_MEM_WFD_REGION;
	default:
		return req_type;
	}
}

int trusted_mem_api_alloc(enum TRUSTED_MEM_REQ_TYPE req_mem_type, u32 alignment,
			  u32 *size, u32 *refcount, u32 *sec_handle,
			  uint8_t *owner, uint32_t id, struct ssheap_buf_info **buf_info)
{
	enum TRUSTED_MEM_TYPE mem_type = get_mem_type(req_mem_type);

	if((mem_type == TRUSTED_MEM_SVP_REGION ||
		mem_type == TRUSTED_MEM_SVP_PAGE ||
		mem_type == TRUSTED_MEM_WFD_REGION ||
		mem_type == TRUSTED_MEM_WFD_PAGE) &&
		!is_svp_enabled()) {
		pr_info("[TMEM][%d] %s: TMEM_OPERATION_NOT_REGISTERED\n", mem_type, __func__);
		return TMEM_OPERATION_NOT_REGISTERED;
	}

	if (is_page_based_memory(mem_type)) {
		pr_info("[TMEM][%d] %s: page-base: size = 0x%x\n", mem_type, __func__, *size);
		return tmem_core_alloc_page(mem_type, *size, buf_info);
	} else {
		/* IOMMU need to map 1MB alignment space */
		if (alignment == SZ_1M)
			*size = (((*size - 1) / SZ_1M) + 1) * SZ_1M;

		/* return error when page-based memory is not enabled */
		if (mem_type != get_region_mem_type(mem_type)) {
			pr_info("[TMEM][%d] %s: page-based disable\n", mem_type, __func__);
			return TMEM_PARAMETER_ERROR;
		}

		pr_info("[TMEM][%d] %s: region-base: size = 0x%x\n", mem_type, __func__, *size);
		return tmem_core_alloc_chunk(mem_type, alignment, *size,
				     refcount, sec_handle, owner, id, 0);
	}
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

int trusted_mem_api_unref(enum TRUSTED_MEM_REQ_TYPE req_mem_type, u32 sec_handle,
			  uint8_t *owner, uint32_t id, struct ssheap_buf_info *buf_info)
{
	enum TRUSTED_MEM_TYPE mem_type = get_mem_type(req_mem_type);

	if (is_page_based_memory(mem_type)) {
		return tmem_core_unref_page(mem_type, buf_info);
	} else {
		mem_type = get_region_mem_type(mem_type);

		return tmem_core_unref_chunk(mem_type, sec_handle, owner, id);
	}
}
EXPORT_SYMBOL(trusted_mem_api_unref);

#if IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT)
bool trusted_mem_api_get_region_info(enum TRUSTED_MEM_REQ_TYPE mem_type,
				     u64 *pa, u32 *size)
{
	return tmem_core_get_region_info(get_mem_type(mem_type), pa, size);
}
#else
bool trusted_mem_api_get_region_info(enum TRUSTED_MEM_REQ_TYPE mem_type,
				     u32 *pa, u32 *size) // for 32bit
{
	return tmem_core_get_region_info(get_mem_type(mem_type), pa, size);
}
#endif
EXPORT_SYMBOL(trusted_mem_api_get_region_info);

int trusted_mem_api_query_pa(enum TRUSTED_MEM_REQ_TYPE mem_type, u32 alignment,
			      u32 size, u32 *refcount, u32 *gz_handle,
			      u8 *owner, u32 id, u32 clean, uint64_t *phy_addr)
{
#if IS_ENABLED(CONFIG_MTK_GZ_KREE)
	return tmem_query_gz_handle_to_pa(get_mem_type(mem_type), alignment, size,
				refcount, gz_handle, owner, id, 0, phy_addr);
#else
	return TMEM_OPERATION_NOT_REGISTERED;
#endif
}
EXPORT_SYMBOL(trusted_mem_api_query_pa);

enum TRUSTED_MEM_REQ_TYPE trusted_mem_api_get_page_replace(
                  enum TRUSTED_MEM_REQ_TYPE req_type)
{
	switch (req_type) {
	case TRUSTED_MEM_REQ_SVP_PAGE:
		return TRUSTED_MEM_REQ_SVP_REGION;
	case TRUSTED_MEM_REQ_PROT_PAGE:
		return TRUSTED_MEM_REQ_PROT_REGION;
	default:
		return req_type;
	}
}
EXPORT_SYMBOL(trusted_mem_api_get_page_replace);
