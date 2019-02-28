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
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/slab.h>

#include "private/tmem_error.h"
#include "private/tmem_device.h"
#include "private/tmem_utils.h"
#include "private/ut_entry.h"
#include "private/ut_macros.h"
#include "private/ut_common.h"

#include "private/ut_tests.h"
#include "private/ut_cmd.h"
DEFINE_UT_SUPPORT(tmem_core);

static enum UT_RET_STATE multiple_ssmr_region_request(struct ut_params *params)
{
	BEGIN_UT_TEST;

	if (is_pmem_registered())
		ASSERT_EQ(0, pmem_ssmr_allocate(), "pmem ssmr allocate check");
	if (is_secmem_registered()) {
		ASSERT_EQ(0, secmem_ssmr_allocate(), "svp ssmr allocate check");
		ASSERT_EQ(0, secmem_ssmr_release(), "svp ssmr release check");
	}
	if (is_pmem_registered())
		ASSERT_EQ(0, pmem_ssmr_release(), "pmem ssmr release check");

	if (is_pmem_registered())
		ASSERT_EQ(0, pmem_ssmr_allocate(), "pmem ssmr allocate check");
	if (is_fr_registered()) {
		ASSERT_EQ(0, fr_ssmr_allocate(), "FR ssmr allocate check");
		ASSERT_EQ(0, fr_ssmr_release(), "FR ssmr release check");
	}
	if (is_pmem_registered())
		ASSERT_EQ(0, pmem_ssmr_release(), "pmem ssmr release check");

	END_UT_TEST;
}

#if defined(CONFIG_MTK_SECURE_MEM_SUPPORT)                                     \
	&& defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
static enum UT_RET_STATE device_virt_region_basic(struct ut_params *params)
{
	BEGIN_UT_TEST;

	ASSERT_EQ(0, secmem_ssmr_allocate(), "svp ssmr allocate check");
	ASSERT_EQ(0, secmem_ssmr_release(), "svp ssmr release check");
	ASSERT_EQ(0, secmem_regmgr_online(), "svp regmgr region online check");
	ASSERT_TRUE(secmem_is_regmgr_region_on(), "svp region state check");
	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state check");
	ASSERT_EQ(0, secmem_regmgr_offline(),
		  "svp regmgr region offline check");
	ASSERT_TRUE(secmem_is_regmgr_region_on(), "svp region state on check");
	mdelay(REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS);
	ASSERT_FALSE(secmem_is_regmgr_region_on(),
		     "svp region state off check");

	ASSERT_EQ(0, fr_ssmr_allocate(), "FR ssmr allocate check");
	ASSERT_EQ(0, fr_ssmr_release(), "FR ssmr release check");
	ASSERT_EQ(0, fr_regmgr_online(), "FR regmgr region online check");
	ASSERT_TRUE(fr_is_regmgr_region_on(), "FR region state check");
	ASSERT_FALSE(secmem_is_regmgr_region_on(), "svp region state check");
	ASSERT_EQ(0, fr_regmgr_offline(), "FR regmgr region offline check");
	ASSERT_TRUE(fr_is_regmgr_region_on(), "FR region state on check");
	mdelay(REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS);
	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state off check");

	END_UT_TEST;
}

static enum UT_RET_STATE device_virt_region_switch(struct ut_params *params)
{
	BEGIN_UT_TEST;

	ASSERT_EQ(0, secmem_regmgr_online(), "svp regmgr region online check");
	ASSERT_TRUE(secmem_is_regmgr_region_on(), "svp region state check");
	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state check");
	ASSERT_NE(0, fr_regmgr_online(), "FR regmgr region online fail check");
	ASSERT_EQ(0, secmem_regmgr_offline(),
		  "svp regmgr region offline check");
	ASSERT_TRUE(secmem_is_regmgr_region_on(), "svp region state on check");
	mdelay(REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS);
	ASSERT_FALSE(secmem_is_regmgr_region_on(),
		     "svp region state off check");

	ASSERT_EQ(0, fr_regmgr_online(), "FR regmgr region online check");
	ASSERT_TRUE(fr_is_regmgr_region_on(), "FR region state check");
	ASSERT_FALSE(secmem_is_regmgr_region_on(), "svp region state check");
	ASSERT_NE(0, secmem_regmgr_online(),
		  "svp regmgr region online fail check");
	ASSERT_EQ(0, fr_regmgr_offline(), "FR regmgr region offline check");
	ASSERT_TRUE(fr_is_regmgr_region_on(), "FR region state on check");
	mdelay(REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS);
	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state off check");

	END_UT_TEST;
}

static enum UT_RET_STATE device_virt_region_alloc(struct ut_params *params)
{
	int ret;
	u32 secmem_handle, secmem_ref_count;
	u32 fr_handle, fr_ref_count;
	u32 pmem_handle, pmem_ref_count;

	BEGIN_UT_TEST;

	ASSERT_FALSE(secmem_is_regmgr_region_on(),
		     "svp region state off check");
	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state off check");

	ret = secmem_alloc_chunk(0, SIZE_64K, &secmem_ref_count, &secmem_handle,
				 NULL, 0, 0);
	ASSERT_EQ(0, ret, "svp alloc chunk memory check");
	ASSERT_EQ(1, secmem_ref_count, "svp reference count check");
	ASSERT_NE(0, secmem_handle, "svp handle check");
	ret = fr_alloc_chunk(0, SIZE_64K, &fr_ref_count, &fr_handle, NULL, 0,
			     0);
	ASSERT_EQ(TMEM_SHARED_DEVICE_REGION_IS_BUSY, ret,
		  "fr alloc chunk memory fail check");

	ret = secmem_unref_chunk(secmem_handle, NULL, 0);
	ASSERT_EQ(0, ret, "svp free chunk memory check");
	ret = fr_alloc_chunk(0, SIZE_64K, &fr_ref_count, &fr_handle, NULL, 0,
			     0);
	ASSERT_EQ(TMEM_SHARED_DEVICE_REGION_IS_BUSY, ret,
		  "fr alloc chunk memory check");

	mdelay(REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS);
	ASSERT_FALSE(secmem_is_regmgr_region_on(),
		     "svp region state off check");

	ret = fr_alloc_chunk(0, SIZE_64K, &fr_ref_count, &fr_handle, NULL, 0,
			     0);
	ASSERT_EQ(0, ret, "fr alloc chunk memory check");
	ASSERT_EQ(1, fr_ref_count, "fr reference count check");
	ASSERT_NE(0, fr_handle, "fr handle check");
	ret = secmem_alloc_chunk(0, SIZE_64K, &secmem_ref_count, &secmem_handle,
				 NULL, 0, 0);
	ASSERT_EQ(TMEM_SHARED_DEVICE_REGION_IS_BUSY, ret,
		  "svp alloc chunk memory fail check");
	ret = fr_unref_chunk(fr_handle, NULL, 0);
	ASSERT_EQ(0, ret, "fr free chunk memory check");
	ret = secmem_alloc_chunk(0, SIZE_64K, &secmem_ref_count, &secmem_handle,
				 NULL, 0, 0);
	ASSERT_EQ(TMEM_SHARED_DEVICE_REGION_IS_BUSY, ret,
		  "svp alloc chunk memory fail check");

	mdelay(REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS);
	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state off check");
	ASSERT_FALSE(secmem_is_regmgr_region_on(),
		     "svp region state off check");

	ret = pmem_alloc_chunk(0, SIZE_64K, &pmem_ref_count, &pmem_handle, NULL,
			       0, 0);
	ASSERT_EQ(0, ret, "pmem alloc chunk memory check");
	ASSERT_EQ(1, pmem_ref_count, "pmem reference count check");
	ASSERT_NE(0, pmem_handle, "pmem handle check");
	ASSERT_FALSE(fr_is_regmgr_region_on(), "FR region state off check");
	ret = secmem_alloc_chunk(0, SIZE_64K, &secmem_ref_count, &secmem_handle,
				 NULL, 0, 0);
	ASSERT_EQ(0, ret, "svp alloc chunk memory check");
	ASSERT_EQ(1, secmem_ref_count, "svp reference count check");
	ASSERT_NE(0, secmem_handle, "svp handle check");
	ret = secmem_unref_chunk(secmem_handle, NULL, 0);
	ASSERT_EQ(0, ret, "svp free chunk memory check");
	ret = pmem_unref_chunk(pmem_handle, NULL, 0);
	ASSERT_EQ(0, ret, "pmem free chunk memory check");

	mdelay(REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS);
	ASSERT_FALSE(secmem_is_regmgr_region_on(),
		     "svp region state off check");
	ASSERT_FALSE(pmem_is_regmgr_region_on(), "pmem region state off check");

	ASSERT_EQ(0, all_regmgr_state_off_check(),
		  "all region state off check");

	END_UT_TEST;
}

static enum UT_RET_STATE device_core_run_all(struct ut_params *params)
{
	int ret;

	BEGIN_UT_TEST;

	ret = multiple_ssmr_region_request(params);
	ASSERT_EQ(0, ret, "multiple region online test");
	ret = device_virt_region_basic(params);
	ASSERT_EQ(0, ret, "virtual region basic test");
	ret = device_virt_region_switch(params);
	ASSERT_EQ(0, ret, "virtual region switch test");
	ret = device_virt_region_alloc(params);
	ASSERT_EQ(0, ret, "virtual region alloc test");

	ASSERT_EQ(0, all_regmgr_state_off_check(),
		  "all region state off check");

	END_UT_TEST;
}
#endif

static enum UT_RET_STATE
multiple_region_multiple_thread_alloc(struct ut_params *params)
{
	BEGIN_UT_TEST;

	ASSERT_EQ(0, mem_multi_type_alloc_multithread_test(),
		  "multi mem type multithread alloc test");

	END_UT_TEST;
}

BEGIN_TEST_SUITE(TMEM_UT_CORE_BASE, TMEM_UT_CORE_MAX, tmem_core_ut_run, NULL)
DEFINE_TEST_CASE(TMEM_UT_CORE_MULTIPLE_SSMR_REGION_REQUEST,
		 multiple_ssmr_region_request)
#if defined(CONFIG_MTK_SECURE_MEM_SUPPORT)                                     \
	&& defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
DEFINE_TEST_CASE(TMEM_UT_CORE_DEVICE_VIRT_REGION_BASIC,
		 device_virt_region_basic)
DEFINE_TEST_CASE(TMEM_UT_CORE_DEVICE_VIRT_REGION_SWITCH,
		 device_virt_region_switch)
DEFINE_TEST_CASE(TMEM_UT_CORE_DEVICE_VIRT_REGION_ALLOC,
		 device_virt_region_alloc)
DEFINE_TEST_CASE(TMEM_UT_CORE_ALL, device_core_run_all)
#endif
DEFINE_TEST_CASE(TMEM_UT_CORE_MULTIPLE_REGION_MULTIPLE_THREAD_ALLOC,
		 multiple_region_multiple_thread_alloc)
END_TEST_SUITE(NULL)
REGISTER_TEST_SUITE(TMEM_UT_CORE_BASE, TMEM_UT_CORE_MAX, tmem_core_ut_run)
