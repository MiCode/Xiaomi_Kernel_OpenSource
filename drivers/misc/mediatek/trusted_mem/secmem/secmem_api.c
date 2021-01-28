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

#include "private/ut_entry.h"
#include "tee_impl/tee_common.h"
#include "tee_impl/tee_invoke.h"
#include "secmem_api.h"

int secmem_svp_dump_info(void)
{
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_SEC_MEM_DUMP_MEM_INFO;
	return tee_directly_invoke_cmd(&cmd_params);
}

int secmem_dynamic_debug_control(bool enable_dbg)
{
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_SEC_MEM_DYNAMIC_DEBUG_CONFIG;
	cmd_params.param2 = enable_dbg;
	return tee_directly_invoke_cmd(&cmd_params);
}

int secmem_force_hw_protection(void)
{
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_SEC_MEM_FORCE_HW_PROTECTION;
	return tee_directly_invoke_cmd(&cmd_params);
}

static enum TRUSTED_MEM_TYPE
get_device_mem_type(enum SECMEM_VIRT_SHARE_REGION region)
{
	switch (region) {
	case SECMEM_VIRT_SHARE_REGION_2D_FR:
		return TRUSTED_MEM_SVP_VIRT_2D_FR;
	case SECMEM_VIRT_SHARE_REGION_SVP:
	default:
		return TRUSTED_MEM_SVP;
	}
}

int secmem_api_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
		     u8 *owner, u32 id)
{
	return tmem_core_alloc_chunk(TRUSTED_MEM_SVP, alignment, size, refcount,
				     sec_handle, owner, id, 0);
}
EXPORT_SYMBOL(secmem_api_alloc);

int secmem_api_alloc_zero(u32 alignment, u32 size, u32 *refcount,
			  u32 *sec_handle, u8 *owner, u32 id)
{
	return tmem_core_alloc_chunk(TRUSTED_MEM_SVP, alignment, size, refcount,
				     sec_handle, owner, id, 1);
}
EXPORT_SYMBOL(secmem_api_alloc_zero);

int secmem_api_unref(u32 sec_handle, u8 *owner, u32 id)
{
	return tmem_core_unref_chunk(TRUSTED_MEM_SVP, sec_handle, owner, id);
}
EXPORT_SYMBOL(secmem_api_unref);

int secmem_api_alloc_ext(u32 alignment, u32 size, u32 *refcount,
			 u32 *sec_handle, u8 *owner, u32 id,
			 enum SECMEM_VIRT_SHARE_REGION region)
{
	return tmem_core_alloc_chunk(get_device_mem_type(region), alignment,
				     size, refcount, sec_handle, owner, id, 0);
}
EXPORT_SYMBOL(secmem_api_alloc_ext);

int secmem_api_alloc_zero_ext(u32 alignment, u32 size, u32 *refcount,
			      u32 *sec_handle, u8 *owner, u32 id,
			      enum SECMEM_VIRT_SHARE_REGION region)
{
	return tmem_core_alloc_chunk(get_device_mem_type(region), alignment,
				     size, refcount, sec_handle, owner, id, 1);
}
EXPORT_SYMBOL(secmem_api_alloc_zero_ext);

int secmem_api_unref_ext(u32 sec_handle, u8 *owner, u32 id,
			 enum SECMEM_VIRT_SHARE_REGION region)
{
	return tmem_core_unref_chunk(get_device_mem_type(region), sec_handle,
				     owner, id);
}
EXPORT_SYMBOL(secmem_api_unref_ext);
