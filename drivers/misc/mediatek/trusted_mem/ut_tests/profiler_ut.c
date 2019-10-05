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

#define TMEM_UT_TEST_FMT
#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/slab.h>

#include "private/tmem_error.h"
#include "private/tmem_device.h"
#include "private/tmem_utils.h"
#include "private/tmem_priv.h"

#include "private/ut_tests.h"
#include "private/ut_cmd.h"
DEFINE_UT_SUPPORT(profiler);

static enum UT_RET_STATE profile_dump_all(struct ut_params *params)
{
	int mem_idx;
	struct trusted_mem_device *mem_device;

	BEGIN_UT_TEST;

	ASSERT_EQ(0, 0, "dummy check");

	for (mem_idx = 0; mem_idx < TRUSTED_MEM_MAX; mem_idx++) {
		mem_device = get_trusted_mem_device(mem_idx);
		if (VALID(mem_device))
			trusted_mem_core_profile_dump(mem_device);
	}

	END_UT_TEST;
}

BEGIN_TEST_SUITE(TMEM_PROFILE_DUMP, TMEM_PROFILE_DUMP, profiler_ut_run, NULL)
DEFINE_TEST_CASE(TMEM_PROFILE_DUMP, profile_dump_all)
END_TEST_SUITE(NULL)
REGISTER_TEST_SUITE(TMEM_PROFILE_DUMP, TMEM_PROFILE_DUMP, profiler_ut_run)
