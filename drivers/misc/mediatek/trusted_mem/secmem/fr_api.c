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

#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/version.h>

#include "private/ut_entry.h"
#ifdef TCORE_UT_FWK_SUPPORT
#include "private/ut_common.h"
#endif
#include "private/tmem_device.h"
#include "private/tmem_error.h"
#include "tee_impl/tee_common.h"
#include "tee_impl/tee_invoke.h"

int secmem_fr_set_prot_shared_region(u64 pa, u32 size)
{
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_SEC_MEM_SET_PROT_REGION;
	cmd_params.param0 = pa;
	cmd_params.param1 = size;

#ifdef TCORE_UT_FWK_SUPPORT
	if (is_multi_type_alloc_multithread_test_locked()) {
		pr_debug("%s:%d return for UT purpose!\n", __func__, __LINE__);
		return TMEM_OK;
	}
#endif

	return tee_directly_invoke_cmd(&cmd_params);
}

int secmem_fr_dump_info(void)
{
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_SEC_MEM_DUMP_MEM_INFO;
	return tee_directly_invoke_cmd(&cmd_params);
}
