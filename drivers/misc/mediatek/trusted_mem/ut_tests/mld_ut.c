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
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include "private/mld_helper.h"
#include "private/tmem_utils.h"

#include "private/ut_tests.h"
#include "private/ut_cmd.h"
DEFINE_UT_SUPPORT(mld_helper);

static enum UT_RET_STATE mld_check_test(struct ut_params *params)
{
	size_t start_size, check_size;
	u32 diff_size;
	void *mem_ptr;

	BEGIN_UT_TEST;

	start_size = mld_stamp();
	ASSERT_EQ(MLD_CHECK_PASS, mld_stamp_check(start_size),
		  "mld initial check");

	start_size = mld_stamp();
	mem_ptr = mld_kmalloc(SIZE_512B, GFP_KERNEL);
	ASSERT_NOTNULL(mem_ptr, "mld kmalloc ptr check");
	ASSERT_EQ(MLD_CHECK_FAIL, mld_stamp_check(start_size),
		  "mld malloc check");
	check_size = mld_stamp();
	diff_size = (u32)(check_size - start_size);
	ASSERT_EQ(SIZE_512B, diff_size, "mld malloc diff size check");

	mld_kfree(mem_ptr);
	ASSERT_EQ(MLD_CHECK_PASS, mld_stamp_check(start_size),
		  "mld free check");
	check_size = mld_stamp();
	diff_size = (u32)(check_size - start_size);
	ASSERT_EQ(0, diff_size, "mld free diff size check");

	END_UT_TEST;
}

BEGIN_TEST_SUITE(TMEM_MEMORY_LEAK_DETECTION_CHECK,
		 TMEM_MEMORY_LEAK_DETECTION_CHECK, mld_ut_run, NULL)
DEFINE_TEST_CASE(TMEM_MEMORY_LEAK_DETECTION_CHECK, mld_check_test)
END_TEST_SUITE(NULL)
REGISTER_TEST_SUITE(TMEM_MEMORY_LEAK_DETECTION_CHECK,
		    TMEM_MEMORY_LEAK_DETECTION_CHECK, mld_ut_run)
