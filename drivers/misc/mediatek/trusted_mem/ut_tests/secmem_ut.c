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
#include "private/ut_macros.h"
#include "private/ut_common.h"

#include "private/ut_tests.h"
#include "private/ut_cmd.h"
DEFINE_UT_SUPPORT(secmem);

#define SECMEM_UT_OWNER "secmem_ut"

static enum UT_RET_STATE secmem_basic_test(struct ut_params *params)
{
	int reg_final_state = params->param1;

	BEGIN_UT_TEST;

	ASSERT_EQ(0, mem_basic_test(TRUSTED_MEM_SVP, reg_final_state),
		  "secmem basic test check");

	END_UT_TEST;
}

static enum UT_RET_STATE secmem_alloc_simple_test(struct ut_params *params)
{
	int reg_final_state = params->param1;
	int un_order_size_cfg = params->param2;

	BEGIN_UT_TEST;

	ASSERT_EQ(0, mem_alloc_simple_test(TRUSTED_MEM_SVP, SECMEM_UT_OWNER,
					   reg_final_state, un_order_size_cfg),
		  "secmem alloc simple test");

	END_UT_TEST;
}

static enum UT_RET_STATE secmem_alloc_alignment_test(struct ut_params *params)
{
	int reg_final_state = params->param1;

	BEGIN_UT_TEST;

	ASSERT_EQ(0, mem_alloc_alignment_test(TRUSTED_MEM_SVP, SECMEM_UT_OWNER,
					      reg_final_state),
		  "secmem alloc alignment test");

	END_UT_TEST;
}

static enum UT_RET_STATE secmem_alloc_saturation_test(struct ut_params *params)
{
	int ret;
	int reg_final_state = params->param1;
	int round = params->param2;

	BEGIN_UT_TEST;

	ASSERT_EQ(0, mem_handle_list_init(TRUSTED_MEM_SVP),
		  "secmem alloc handle list check");
	ret = mem_alloc_saturation_test(TRUSTED_MEM_SVP, SECMEM_UT_OWNER,
					reg_final_state, round);
	mem_handle_list_deinit();
	ASSERT_EQ(0, ret, "secmem alloc saturation test");

	END_UT_TEST;
}

static enum UT_RET_STATE
secmem_regmgr_region_defer_off_test(struct ut_params *params)
{
	int reg_final_state = params->param1;

	BEGIN_UT_TEST;

	ASSERT_EQ(0, mem_regmgr_region_defer_off_test(
			     TRUSTED_MEM_SVP, SECMEM_UT_OWNER, reg_final_state),
		  "secmem region defer off test");

	END_UT_TEST;
}

static enum UT_RET_STATE
secmem_regmgr_region_online_count_test(struct ut_params *params)
{
	int reg_final_state = params->param1;

	BEGIN_UT_TEST;

	ASSERT_EQ(0, mem_regmgr_region_online_count_test(
			     TRUSTED_MEM_SVP, SECMEM_UT_OWNER, reg_final_state),
		  "secmem region online count test");

	END_UT_TEST;
}

static enum UT_RET_STATE
secmem_region_on_off_stress_test(struct ut_params *params)
{
	int reg_final_state = params->param1;
	int round = params->param2;

	BEGIN_UT_TEST;

	ASSERT_EQ(0, mem_region_on_off_stress_test(TRUSTED_MEM_SVP,
						   reg_final_state, round),
		  "secmem region on/off stress test");

	END_UT_TEST;
}

static enum UT_RET_STATE secmem_alloc_multithread_test(struct ut_params *params)
{
	int reg_final_state = params->param1;

	UNUSED(reg_final_state);

	BEGIN_UT_TEST;

	ASSERT_EQ(0, mem_alloc_multithread_test(TRUSTED_MEM_SVP),
		  "secmem alloc multithread test");

	END_UT_TEST;
}

static enum UT_RET_STATE secmem_regmgr_run_all(struct ut_params *params)
{
	int ret;
	int region_final_state = params->param1;

	BEGIN_UT_TEST;

	params->param1 = REGMGR_REGION_FINAL_STATE_ON;
	ret = secmem_basic_test(params);
	ASSERT_EQ(0, ret, "basic test");

	params->param1 = REGMGR_REGION_FINAL_STATE_ON;
	params->param2 = MEM_UNORDER_SIZE_TEST_CFG_DISABLE;
	ret = secmem_alloc_simple_test(params);
	ASSERT_EQ(0, ret, "alloc simple test");

	params->param1 = REGMGR_REGION_FINAL_STATE_ON;
	ret = secmem_alloc_alignment_test(params);
	ASSERT_EQ(0, ret, "alloc alignment test");

	params->param1 = REGMGR_REGION_FINAL_STATE_ON;
	params->param2 = MEM_UNORDER_SIZE_TEST_CFG_ENABLE;
	ret = secmem_alloc_simple_test(params);
	ASSERT_EQ(0, ret, "alloc un-ordered size test");

	params->param1 = REGMGR_REGION_FINAL_STATE_ON;
	params->param2 = MEM_SATURATION_STREE_ROUND;
	ret = secmem_alloc_saturation_test(params);
	ASSERT_EQ(0, ret, "alloc saturation test");

	params->param1 = REGMGR_REGION_FINAL_STATE_ON;
	ret = secmem_regmgr_region_online_count_test(params);
	ASSERT_EQ(0, ret, "region online count test");

	params->param1 = REGMGR_REGION_FINAL_STATE_OFF;
	ret = secmem_regmgr_region_defer_off_test(params);
	ASSERT_EQ(0, ret, "region defer off test");

	params->param1 = region_final_state;
	ret = secmem_alloc_multithread_test(params);
	ASSERT_EQ(0, ret, "multithread alloc test");

	ASSERT_EQ(0, all_regmgr_state_off_check(),
		  "all region state off check");

	END_UT_TEST;
}

BEGIN_TEST_SUITE(SECMEM_UT_PROC_BASE, SECMEM_UT_PROC_MAX, secmem_ut_run, NULL)
DEFINE_TEST_CASE_PARAM1(SECMEM_UT_PROC_BASIC, secmem_basic_test,
			REGMGR_REGION_FINAL_STATE_OFF)
DEFINE_TEST_CASE_PARAM2(SECMEM_UT_PROC_SIMPLE_ALLOC, secmem_alloc_simple_test,
			REGMGR_REGION_FINAL_STATE_OFF,
			MEM_UNORDER_SIZE_TEST_CFG_DISABLE)
DEFINE_TEST_CASE_PARAM1(SECMEM_UT_PROC_ALIGNMENT, secmem_alloc_alignment_test,
			REGMGR_REGION_FINAL_STATE_OFF)
DEFINE_TEST_CASE_PARAM2(SECMEM_UT_PROC_UNORDERED_SIZE, secmem_alloc_simple_test,
			REGMGR_REGION_FINAL_STATE_OFF,
			MEM_UNORDER_SIZE_TEST_CFG_ENABLE)
DEFINE_TEST_CASE_PARAM2(SECMEM_UT_PROC_SATURATION, secmem_alloc_saturation_test,
			REGMGR_REGION_FINAL_STATE_OFF, 1)
DEFINE_TEST_CASE_PARAM2(SECMEM_UT_PROC_SATURATION_STRESS,
			secmem_alloc_saturation_test,
			REGMGR_REGION_FINAL_STATE_OFF,
			MEM_SATURATION_STREE_ROUND)
DEFINE_TEST_CASE_PARAM1(SECMEM_UT_PROC_REGION_DEFER,
			secmem_regmgr_region_defer_off_test,
			REGMGR_REGION_FINAL_STATE_OFF)
DEFINE_TEST_CASE_PARAM1(SECMEM_UT_PROC_REGION_ONLINE_CNT,
			secmem_regmgr_region_online_count_test,
			REGMGR_REGION_FINAL_STATE_OFF)
DEFINE_TEST_CASE_PARAM2(SECMEM_UT_PROC_REGION_STRESS,
			secmem_region_on_off_stress_test,
			REGMGR_REGION_FINAL_STATE_OFF,
			MEM_REGION_ON_OFF_STREE_ROUND)
DEFINE_TEST_CASE_PARAM1(SECMEM_UT_PROC_ALLOC_MULTITHREAD,
			secmem_alloc_multithread_test,
			REGMGR_REGION_FINAL_STATE_OFF)
DEFINE_TEST_CASE_PARAM1(SECMEM_UT_PROC_ALL, secmem_regmgr_run_all,
			REGMGR_REGION_FINAL_STATE_OFF)
END_TEST_SUITE(NULL)
REGISTER_TEST_SUITE(SECMEM_UT_PROC_BASE, SECMEM_UT_PROC_MAX, secmem_ut_run)
