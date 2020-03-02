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
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/completion.h>

#include "private/mld_helper.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "private/ut_entry.h"
#include "private/secmem_ext.h"
#include "private/ut_macros.h"
#include "private/ut_common.h"

#include "private/ut_tests.h"
#include "private/ut_cmd.h"
DEFINE_UT_SUPPORT(fr);

#define PROT_TEST_PA_ADDR64_START (0x180000000ULL)
#define PROT_TEST_PA_ADDR64_ZERO (0x0ULL)
#define PROT_TEST_POOL_SIZE_NORMAL SIZE_256M
#define PROT_TEST_POOL_SIZE_INVALID SIZE_32K
#define PROT_TEST_POOL_SIZE_MINIMAL_ALLOW SIZE_64K
#define PROT_TEST_POOL_SIZE_ZERO (0x0)
static enum UT_RET_STATE fr_set_prot_region(struct ut_params *params)
{
	int ret;

	BEGIN_UT_TEST;

	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state off check");
	ret = secmem_fr_set_prot_shared_region(PROT_TEST_PA_ADDR64_START,
					       PROT_TEST_POOL_SIZE_NORMAL);
	ASSERT_EQ(0, ret, "set valid pa region check");
	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state off check");
	ret = secmem_fr_set_prot_shared_region(
		PROT_TEST_PA_ADDR64_START, PROT_TEST_POOL_SIZE_MINIMAL_ALLOW);
	ASSERT_EQ(0, ret, "set valid pa region size check");
	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state off check");

	ret = secmem_fr_set_prot_shared_region(PROT_TEST_PA_ADDR64_START,
					       PROT_TEST_POOL_SIZE_INVALID);
	ASSERT_NE(0, ret, "set invalid region size check");
	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state off check");
	ret = secmem_fr_set_prot_shared_region(PROT_TEST_PA_ADDR64_ZERO,
					       PROT_TEST_POOL_SIZE_NORMAL);
	ASSERT_NE(0, ret, "set invalid pa start addr check");
	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state off check");

	ret = secmem_fr_set_prot_shared_region(PROT_TEST_PA_ADDR64_ZERO,
					       PROT_TEST_POOL_SIZE_ZERO);
	ASSERT_EQ(0, ret, "clean pa region check");
	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state off check");
	mdelay(REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS);
	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state off check");

	END_UT_TEST;
}

static enum UT_RET_STATE fr_run_all(struct ut_params *params)
{
	int ret;

	BEGIN_UT_TEST;

	ret = fr_set_prot_region(params);
	ASSERT_EQ(0, ret, "fr set protected share region");

	ASSERT_EQ(0, all_regmgr_state_off_check(),
		  "all region state off check");

	END_UT_TEST;
}

BEGIN_TEST_SUITE(FR_UT_PROC_BASE, FR_UT_PROC_MAX, fr_ut_run, NULL)
DEFINE_TEST_CASE(FR_UT_PROC_CONFIG_PROT_REGION, fr_set_prot_region)
DEFINE_TEST_CASE(FR_UT_PROC_ALL, fr_run_all)
END_TEST_SUITE(NULL)
REGISTER_TEST_SUITE(FR_UT_PROC_BASE, FR_UT_PROC_MAX, fr_ut_run)
