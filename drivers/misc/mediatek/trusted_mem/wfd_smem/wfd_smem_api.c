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

int wfd_smem_dump_info(void)
{
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_WFD_SMEM_DUMP_MEM_INFO;
	return tee_directly_invoke_cmd(&cmd_params);
}

int wfd_smem_api_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
		       u8 *owner, u32 id)
{
	return tmem_core_alloc_chunk(TRUSTED_MEM_WFD, alignment, size, refcount,
				     sec_handle, owner, id, 0);
}
EXPORT_SYMBOL(wfd_smem_api_alloc);

int wfd_smem_api_alloc_zero(u32 alignment, u32 size, u32 *refcount,
			    u32 *sec_handle, u8 *owner, u32 id)
{
	return tmem_core_alloc_chunk(TRUSTED_MEM_WFD, alignment, size, refcount,
				     sec_handle, owner, id, 1);
}
EXPORT_SYMBOL(wfd_smem_api_alloc_zero);

int wfd_smem_api_unref(u32 sec_handle, u8 *owner, u32 id)
{
	return tmem_core_unref_chunk(TRUSTED_MEM_WFD, sec_handle, owner, id);
}
EXPORT_SYMBOL(wfd_smem_api_unref);
