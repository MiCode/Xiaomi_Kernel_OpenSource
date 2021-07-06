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
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
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
#include "private/tmem_entry.h"
#include "private/tmem_priv.h"
#include "private/ut_cmd.h"
#ifdef TCORE_MEMORY_LEAK_DETECTION_SUPPORT
#include "private/mld_helper.h"
#endif
#include "tests/ut_api.h"
#include "tests/ut_common.h"
#include "tee_impl/tee_invoke.h"
#include "tee_impl/tee_regions.h"

#ifdef CONFIG_MTK_ENG_BUILD
#define UT_SATURATION_STRESS_ROUNDS (1)
#else
#define UT_SATURATION_STRESS_ROUNDS (5)
#endif

struct test_case {
	u64 cmd;
	u64 param1;
	u64 param2;
	u64 param3;
	char *description;
	int (*func)(struct ut_params *params, char *test_desc);
};

/* clang-format off */
#define CASE(ut_cmd, desc, p1, p2, p3, cb) \
	{ \
		.cmd = ut_cmd, \
		.description = desc, \
		.param1 = p1, \
		.param2 = p2, \
		.param3 = p3, \
		.func = cb \
	}
/* clang-format on */

static enum UT_RET_STATE tmem_basic_test(struct ut_params *params,
					 char *test_desc)
{
	enum TRUSTED_MEM_TYPE mem_type = params->param1;
	int reg_final_state = params->param2;

	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	ASSERT_EQ(0, mem_basic_test(mem_type, reg_final_state), test_desc);
	return UT_STATE_PASS;
}

static enum UT_RET_STATE tmem_alloc_simple_test(struct ut_params *params,
						char *test_desc)
{
	enum TRUSTED_MEM_TYPE mem_type = params->param1;
	int reg_final_state = params->param2;
	int un_order_size_cfg = params->param3;

	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	ASSERT_EQ(0, mem_alloc_simple_test(mem_type, NULL, reg_final_state,
					   un_order_size_cfg),
		  test_desc);
	return UT_STATE_PASS;
}

static enum UT_RET_STATE tmem_alloc_alignment_test(struct ut_params *params,
						   char *test_desc)
{
	enum TRUSTED_MEM_TYPE mem_type = params->param1;
	int reg_final_state = params->param2;

	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	ASSERT_EQ(0, mem_alloc_alignment_test(mem_type, NULL, reg_final_state),
		  test_desc);
	return UT_STATE_PASS;
}

static enum UT_RET_STATE tmem_alloc_saturation_test(struct ut_params *params,
						    char *test_desc)
{
	int ret;
	enum TRUSTED_MEM_TYPE mem_type = params->param1;
	int reg_final_state = params->param2;
	int round = params->param3;

	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	/* Make sure region online/offline is okay for single item tests */
	ASSERT_EQ(0, tmem_core_regmgr_online(mem_type), "regmgr region online");
	ASSERT_EQ(0, tmem_core_regmgr_offline(mem_type),
		  "regmgr region offline");

	ASSERT_EQ(0, mem_handle_list_init(mem_type), "alloc handle list check");
	ret = mem_alloc_saturation_test(mem_type, NULL, reg_final_state, round);
	mem_handle_list_deinit();
	ASSERT_EQ(0, ret, test_desc);
	return UT_STATE_PASS;
}


static enum UT_RET_STATE
tmem_regmgr_region_defer_off_test(struct ut_params *params, char *test_desc)
{
	enum TRUSTED_MEM_TYPE mem_type = params->param1;
	int reg_final_state = params->param2;

	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	ASSERT_EQ(0, mem_regmgr_region_defer_off_test(mem_type, NULL,
						      reg_final_state),
		  test_desc);
	return UT_STATE_PASS;
}

static enum UT_RET_STATE
tmem_regmgr_region_online_count_test(struct ut_params *params, char *test_desc)
{
	enum TRUSTED_MEM_TYPE mem_type = params->param1;
	int reg_final_state = params->param2;

	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	ASSERT_EQ(0, mem_regmgr_region_online_count_test(mem_type, NULL,
							 reg_final_state),
		  test_desc);
	return UT_STATE_PASS;
}

static enum UT_RET_STATE
tmem_region_on_off_stress_test(struct ut_params *params, char *test_desc)
{
	enum TRUSTED_MEM_TYPE mem_type = params->param1;
	int reg_final_state = params->param2;
	int round = params->param3;

	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	ASSERT_EQ(0, mem_region_on_off_stress_test(mem_type, reg_final_state,
						   round),
		  test_desc);
	return UT_STATE_PASS;
}

static enum UT_RET_STATE tmem_alloc_multithread_test(struct ut_params *params,
						     char *test_desc)
{
	enum TRUSTED_MEM_TYPE mem_type = params->param1;
	int reg_final_state = params->param2;

	UNUSED(reg_final_state);

	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	/* Make sure region online/offline is okay for single item tests */
	ASSERT_EQ(0, tmem_core_regmgr_online(mem_type), "regmgr region online");
	ASSERT_EQ(0, tmem_core_regmgr_offline(mem_type),
		  "regmgr region offline");

	ASSERT_EQ(0, mem_alloc_multithread_test(mem_type), test_desc);
	return UT_STATE_PASS;
}

#ifdef CONFIG_MTK_PROT_MEM_SUPPORT
static enum UT_RET_STATE tmem_alloc_mixed_size(struct ut_params *params,
					       char *test_desc)
{
	int ret;
	enum TRUSTED_MEM_TYPE mem_type = params->param1;
	int reg_final_state = params->param2;

	UNUSED(reg_final_state);

	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	/* Make sure region online/offline is okay for single item tests */
	ASSERT_EQ(0, tmem_core_regmgr_online(mem_type), "regmgr region online");
	ASSERT_EQ(0, tmem_core_regmgr_offline(mem_type),
		  "regmgr region offline");

	ASSERT_EQ(0, mem_handle_list_init(mem_type), "alloc handle list check");
	ret = mem_alloc_mixed_size_test(mem_type, NULL, reg_final_state);
	mem_handle_list_deinit();
	ASSERT_EQ(0, ret, test_desc);

	return UT_STATE_PASS;
}
#endif

static enum UT_RET_STATE tmem_regmgr_run_all(struct ut_params *params,
					     char *test_desc)
{
	int ret;
	enum TRUSTED_MEM_TYPE mem_type = params->param1;
	int region_final_state = params->param2;

	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	params->param1 = mem_type;
	params->param2 = REGMGR_REGION_FINAL_STATE_ON;
	ret = tmem_basic_test(params, test_desc);
	ASSERT_EQ(0, ret, "basic test");

	params->param1 = mem_type;
	params->param2 = REGMGR_REGION_FINAL_STATE_ON;
	params->param3 = MEM_UNORDER_SIZE_TEST_CFG_DISABLE;
	ret = tmem_alloc_simple_test(params, test_desc);
	ASSERT_EQ(0, ret, "alloc simple test");

	params->param1 = mem_type;
	params->param2 = REGMGR_REGION_FINAL_STATE_ON;
	ret = tmem_alloc_alignment_test(params, test_desc);
	ASSERT_EQ(0, ret, "alloc alignment test");

	params->param1 = mem_type;
	params->param2 = REGMGR_REGION_FINAL_STATE_ON;
	params->param3 = MEM_UNORDER_SIZE_TEST_CFG_ENABLE;
	ret = tmem_alloc_simple_test(params, test_desc);
	ASSERT_EQ(0, ret, "alloc un-ordered size test");

	params->param1 = mem_type;
	params->param2 = REGMGR_REGION_FINAL_STATE_ON;
	params->param3 = UT_SATURATION_STRESS_ROUNDS;
	ret = tmem_alloc_saturation_test(params, test_desc);
	ASSERT_EQ(0, ret, "alloc saturation test");

	params->param1 = mem_type;
	params->param2 = REGMGR_REGION_FINAL_STATE_ON;
	ret = tmem_regmgr_region_online_count_test(params, test_desc);
	ASSERT_EQ(0, ret, "region online count test");

	params->param1 = mem_type;
	params->param2 = REGMGR_REGION_FINAL_STATE_OFF;
	ret = tmem_regmgr_region_defer_off_test(params, test_desc);
	ASSERT_EQ(0, ret, "region defer off test");

	params->param1 = mem_type;
	params->param2 = region_final_state;
	ret = tmem_alloc_multithread_test(params, test_desc);
	ASSERT_EQ(0, ret, "multithread alloc test");

	ASSERT_EQ(0, all_regmgr_state_off_check(),
		  "all region state off check");

	return UT_STATE_PASS;
}

#ifdef TCORE_MEMORY_LEAK_DETECTION_SUPPORT
static enum UT_RET_STATE mld_check_test(struct ut_params *params,
					char *test_desc)
{
	size_t start_size, check_size;
	u32 diff_size;
	void *mem_ptr;

	UNUSED(test_desc);

	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

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

	return UT_STATE_PASS;
}
#endif

#ifdef TCORE_PROFILING_SUPPORT
static enum UT_RET_STATE profile_dump_all(struct ut_params *params,
					  char *test_desc)
{
	int mem_idx;
	struct trusted_mem_device *mem_device;

	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	ASSERT_EQ(0, 0, test_desc);

	for (mem_idx = 0; mem_idx < TRUSTED_MEM_MAX; mem_idx++) {
		mem_device = get_trusted_mem_device(mem_idx);
		if (VALID(mem_device))
			trusted_mem_core_profile_dump(mem_device);
	}

	return UT_STATE_PASS;
}
#endif

#if defined(CONFIG_MTK_SECURE_MEM_SUPPORT)                                     \
	&& defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
#define PROT_TEST_PA_ADDR64_START (0x180000000ULL)
#define PROT_TEST_PA_ADDR64_ZERO (0x0ULL)
#define PROT_TEST_POOL_SIZE_NORMAL SIZE_256M
#define PROT_TEST_POOL_SIZE_INVALID SIZE_32K
#define PROT_TEST_POOL_SIZE_MINIMAL_ALLOW SIZE_64K
#define PROT_TEST_POOL_SIZE_ZERO (0x0)
static enum UT_RET_STATE config_tee_prot_region_test(struct ut_params *params,
						     char *test_desc)
{
	int ret;

	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	ASSERT_FALSE(tmem_core_is_regmgr_region_on(TRUSTED_MEM_2D_FR),
		     "FR region state off check");
	ret = secmem_fr_set_prot_shared_region(PROT_TEST_PA_ADDR64_START,
					       PROT_TEST_POOL_SIZE_NORMAL,
					       TEE_SMEM_PROT);
	ASSERT_EQ(0, ret, "set valid pa region check");
	ASSERT_FALSE(tmem_core_is_regmgr_region_on(TRUSTED_MEM_2D_FR),
		     "FR region state off check");
	ret = secmem_fr_set_prot_shared_region(
		PROT_TEST_PA_ADDR64_START, PROT_TEST_POOL_SIZE_MINIMAL_ALLOW,
		TEE_SMEM_PROT);
	ASSERT_EQ(0, ret, "set valid pa region size check");
	ASSERT_FALSE(tmem_core_is_regmgr_region_on(TRUSTED_MEM_2D_FR),
		     "FR region state off check");

	ret = secmem_fr_set_prot_shared_region(PROT_TEST_PA_ADDR64_START,
					       PROT_TEST_POOL_SIZE_INVALID,
					       TEE_SMEM_PROT);
	ASSERT_NE(0, ret, "set invalid region size check");
	ASSERT_FALSE(tmem_core_is_regmgr_region_on(TRUSTED_MEM_2D_FR),
		     "FR region state off check");
	ret = secmem_fr_set_prot_shared_region(PROT_TEST_PA_ADDR64_ZERO,
					       PROT_TEST_POOL_SIZE_NORMAL,
					       TEE_SMEM_PROT);
	ASSERT_NE(0, ret, "set invalid pa start addr check");
	ASSERT_FALSE(tmem_core_is_regmgr_region_on(TRUSTED_MEM_2D_FR),
		     "FR region state off check");

	ret = secmem_fr_set_prot_shared_region(PROT_TEST_PA_ADDR64_ZERO,
					       PROT_TEST_POOL_SIZE_ZERO,
					       TEE_SMEM_PROT);
	ASSERT_EQ(0, ret, "clean pa region check");
	ASSERT_FALSE(tmem_core_is_regmgr_region_on(TRUSTED_MEM_2D_FR),
		     "FR region state off check");
	mdelay(REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS);
	ASSERT_FALSE(tmem_core_is_regmgr_region_on(TRUSTED_MEM_2D_FR),
		     "FR region state off check");

	return UT_STATE_PASS;
}
#endif

static enum UT_RET_STATE multiple_ssmr_region_request(struct ut_params *params,
						      char *test_desc)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	if (tmem_core_is_device_registered(TRUSTED_MEM_PROT))
		ASSERT_EQ(0, tmem_core_ssmr_allocate(TRUSTED_MEM_PROT),
			  "pmem ssmr allocate check");
	if (tmem_core_is_device_registered(TRUSTED_MEM_2D_FR)) {
		ASSERT_EQ(0, tmem_core_ssmr_allocate(TRUSTED_MEM_2D_FR),
			  "FR ssmr allocate check");
		ASSERT_EQ(0, tmem_core_ssmr_release(TRUSTED_MEM_2D_FR),
			  "FR ssmr release check");
	}
	if (tmem_core_is_device_registered(TRUSTED_MEM_PROT))
		ASSERT_EQ(0, tmem_core_ssmr_release(TRUSTED_MEM_PROT),
			  "pmem ssmr release check");

	if (tmem_core_is_device_registered(TRUSTED_MEM_SVP))
		ASSERT_EQ(0, tmem_core_ssmr_allocate(TRUSTED_MEM_SVP),
			  "svp ssmr allocate check");
	if (tmem_core_is_device_registered(TRUSTED_MEM_WFD)) {
		ASSERT_EQ(0, tmem_core_ssmr_allocate(TRUSTED_MEM_WFD),
			  "wfd ssmr allocate check");
		ASSERT_EQ(0, tmem_core_ssmr_release(TRUSTED_MEM_WFD),
			  "wfd ssmr release check");
	}
	if (tmem_core_is_device_registered(TRUSTED_MEM_SVP))
		ASSERT_EQ(0, tmem_core_ssmr_release(TRUSTED_MEM_SVP),
			  "svp ssmr release check");

	if (tmem_core_is_device_registered(TRUSTED_MEM_SVP))
		ASSERT_EQ(0, tmem_core_ssmr_allocate(TRUSTED_MEM_SVP),
			  "svp ssmr allocate check");
	if (tmem_core_is_device_registered(TRUSTED_MEM_2D_FR)) {
		ASSERT_NE(0, tmem_core_ssmr_allocate(TRUSTED_MEM_2D_FR),
			  "FR ssmr allocate check");
	}
	if (tmem_core_is_device_registered(TRUSTED_MEM_SVP))
		ASSERT_EQ(0, tmem_core_ssmr_release(TRUSTED_MEM_SVP),
			  "svp ssmr release check");

	return UT_STATE_PASS;
}

#if MULTIPLE_REGION_MULTIPLE_THREAD_TEST_ENABLE
static enum UT_RET_STATE
multiple_region_multiple_thread_alloc(struct ut_params *params, char *test_desc)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	ASSERT_EQ(0, mem_multi_type_alloc_multithread_test(), test_desc);
	return UT_STATE_PASS;
}
#endif

#if MTEE_MCHUNKS_MULTIPLE_THREAD_TEST_ENABLE
static enum UT_RET_STATE
mtee_mchunks_multiple_thread_alloc(struct ut_params *params, char *test_desc)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (ut_is_halt())
		return UT_STATE_FAIL;

	ASSERT_EQ(0, mem_mtee_mchunks_alloc_multithread_test(), test_desc);
	return UT_STATE_PASS;
}
#endif

static struct test_case test_cases[] = {
#ifdef CONFIG_MTK_SECURE_MEM_SUPPORT
	CASE(SECMEM_UT_PROC_BASIC, "SVP Basic", TRUSTED_MEM_SVP,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_basic_test),
	CASE(SECMEM_UT_PROC_SIMPLE_ALLOC, "SVP Alloc Simple", TRUSTED_MEM_SVP,
	     REGMGR_REGION_FINAL_STATE_OFF, MEM_UNORDER_SIZE_TEST_CFG_DISABLE,
	     tmem_alloc_simple_test),
	CASE(SECMEM_UT_PROC_UNORDERED_SIZE, "SVP Alloc Un-ordered Size",
	     TRUSTED_MEM_SVP, REGMGR_REGION_FINAL_STATE_OFF,
	     MEM_UNORDER_SIZE_TEST_CFG_ENABLE, tmem_alloc_simple_test),
	CASE(SECMEM_UT_PROC_ALIGNMENT, "SVP Alloc Alignment", TRUSTED_MEM_SVP,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_alloc_alignment_test),
	CASE(SECMEM_UT_PROC_SATURATION, "SVP Saturation", TRUSTED_MEM_SVP,
	     REGMGR_REGION_FINAL_STATE_OFF, 1, tmem_alloc_saturation_test),
	CASE(SECMEM_UT_PROC_SATURATION_STRESS, "SVP Saturation Stress",
	     TRUSTED_MEM_SVP, REGMGR_REGION_FINAL_STATE_OFF,
	     UT_SATURATION_STRESS_ROUNDS, tmem_alloc_saturation_test),
	CASE(SECMEM_UT_PROC_REGION_DEFER, "SVP Region Defer Off",
	     TRUSTED_MEM_SVP, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_defer_off_test),
	CASE(SECMEM_UT_PROC_REGION_ONLINE_CNT, "SVP Region Online Count",
	     TRUSTED_MEM_SVP, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_online_count_test),
	CASE(SECMEM_UT_PROC_REGION_STRESS, "SVP Region On/Off Stress",
	     TRUSTED_MEM_SVP, REGMGR_REGION_FINAL_STATE_OFF,
	     MEM_REGION_ON_OFF_STREE_ROUND, tmem_region_on_off_stress_test),
	CASE(SECMEM_UT_PROC_ALLOC_MULTITHREAD, "SVP Alloc Multi-thread",
	     TRUSTED_MEM_SVP, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_alloc_multithread_test),
	CASE(SECMEM_UT_PROC_ALL, "SVP Run ALL", TRUSTED_MEM_SVP,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_regmgr_run_all),
#endif

#ifdef CONFIG_MTK_PROT_MEM_SUPPORT
	CASE(PMEM_UT_PROC_BASIC, "PROT Basic", TRUSTED_MEM_PROT,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_basic_test),
	CASE(PMEM_UT_PROC_SIMPLE_ALLOC, "PROT Alloc Simple", TRUSTED_MEM_PROT,
	     REGMGR_REGION_FINAL_STATE_OFF, MEM_UNORDER_SIZE_TEST_CFG_DISABLE,
	     tmem_alloc_simple_test),
	CASE(PMEM_UT_PROC_UNORDERED_SIZE, "PROT Alloc Un-ordered Size",
	     TRUSTED_MEM_PROT, REGMGR_REGION_FINAL_STATE_OFF,
	     MEM_UNORDER_SIZE_TEST_CFG_ENABLE, tmem_alloc_simple_test),
	CASE(PMEM_UT_PROC_ALIGNMENT, "PROT Alloc Alignment", TRUSTED_MEM_PROT,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_alloc_alignment_test),
	CASE(PMEM_UT_PROC_SATURATION, "PROT Saturation", TRUSTED_MEM_PROT,
	     REGMGR_REGION_FINAL_STATE_OFF, 1, tmem_alloc_saturation_test),
	CASE(PMEM_UT_PROC_SATURATION_STRESS, "PROT Saturation Stress",
	     TRUSTED_MEM_PROT, REGMGR_REGION_FINAL_STATE_OFF,
	     UT_SATURATION_STRESS_ROUNDS, tmem_alloc_saturation_test),
	CASE(PMEM_UT_PROC_REGION_DEFER, "PROT Region Defer Off",
	     TRUSTED_MEM_PROT, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_defer_off_test),
	CASE(PMEM_UT_PROC_REGION_ONLINE_CNT, "PROT Region Online Count",
	     TRUSTED_MEM_PROT, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_online_count_test),
	CASE(PMEM_UT_PROC_REGION_STRESS, "PROT Region On/Off Stress",
	     TRUSTED_MEM_PROT, REGMGR_REGION_FINAL_STATE_OFF,
	     MEM_REGION_ON_OFF_STREE_ROUND, tmem_region_on_off_stress_test),
	CASE(PMEM_UT_PROC_ALLOC_MULTITHREAD, "PROT Alloc Multi-thread",
	     TRUSTED_MEM_PROT, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_alloc_multithread_test),
	CASE(PMEM_UT_PROC_ALLOC_MIXED_SIZE, "PROT Alloc Diff Size",
	     TRUSTED_MEM_PROT, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_alloc_mixed_size),
	CASE(PMEM_UT_PROC_ALL, "PROT Run ALL", TRUSTED_MEM_PROT,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_regmgr_run_all),
#endif

#ifdef CONFIG_MTK_WFD_SMEM_SUPPORT
	CASE(WFD_SMEM_UT_PROC_BASIC, "WFD Basic", TRUSTED_MEM_WFD,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_basic_test),
	CASE(WFD_SMEM_UT_PROC_SIMPLE_ALLOC, "WFD Alloc Simple", TRUSTED_MEM_WFD,
	     REGMGR_REGION_FINAL_STATE_OFF, MEM_UNORDER_SIZE_TEST_CFG_DISABLE,
	     tmem_alloc_simple_test),
	CASE(WFD_SMEM_UT_PROC_UNORDERED_SIZE, "WFD Alloc Un-ordered Size",
	     TRUSTED_MEM_WFD, REGMGR_REGION_FINAL_STATE_OFF,
	     MEM_UNORDER_SIZE_TEST_CFG_ENABLE, tmem_alloc_simple_test),
	CASE(WFD_SMEM_UT_PROC_ALIGNMENT, "WFD Alloc Alignment", TRUSTED_MEM_WFD,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_alloc_alignment_test),
	CASE(WFD_SMEM_UT_PROC_SATURATION, "WFD Saturation", TRUSTED_MEM_WFD,
	     REGMGR_REGION_FINAL_STATE_OFF, 1, tmem_alloc_saturation_test),
	CASE(WFD_SMEM_UT_PROC_SATURATION_STRESS, "WFD Saturation Stress",
	     TRUSTED_MEM_WFD, REGMGR_REGION_FINAL_STATE_OFF,
	     UT_SATURATION_STRESS_ROUNDS, tmem_alloc_saturation_test),
	CASE(WFD_SMEM_UT_PROC_REGION_DEFER, "WFD Region Defer Off",
	     TRUSTED_MEM_WFD, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_defer_off_test),
	CASE(WFD_SMEM_UT_PROC_REGION_ONLINE_CNT, "WFD Region Online Count",
	     TRUSTED_MEM_WFD, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_online_count_test),
	CASE(WFD_SMEM_UT_PROC_REGION_STRESS, "WFD Region On/Off Stress",
	     TRUSTED_MEM_WFD, REGMGR_REGION_FINAL_STATE_OFF,
	     MEM_REGION_ON_OFF_STREE_ROUND, tmem_region_on_off_stress_test),
	CASE(WFD_SMEM_UT_PROC_ALLOC_MULTITHREAD, "WFD Alloc Multi-thread",
	     TRUSTED_MEM_WFD, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_alloc_multithread_test),
	CASE(WFD_SMEM_UT_PROC_ALL, "WFD Run ALL", TRUSTED_MEM_WFD,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_regmgr_run_all),
#endif

#ifdef CONFIG_MTK_HAPP_MEM_SUPPORT
	CASE(HAPP_UT_PROC_BASIC, "HAPP Basic", TRUSTED_MEM_HAPP,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_basic_test),
	CASE(HAPP_UT_PROC_SIMPLE_ALLOC, "HAPP Alloc Simple", TRUSTED_MEM_HAPP,
	     REGMGR_REGION_FINAL_STATE_OFF, MEM_UNORDER_SIZE_TEST_CFG_DISABLE,
	     tmem_alloc_simple_test),
	CASE(HAPP_UT_PROC_UNORDERED_SIZE, "HAPP Alloc Un-ordered Size",
	     TRUSTED_MEM_HAPP, REGMGR_REGION_FINAL_STATE_OFF,
	     MEM_UNORDER_SIZE_TEST_CFG_ENABLE, tmem_alloc_simple_test),
	CASE(HAPP_UT_PROC_ALIGNMENT, "HAPP Alloc Alignment", TRUSTED_MEM_HAPP,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_alloc_alignment_test),
	CASE(HAPP_UT_PROC_SATURATION, "HAPP Saturation", TRUSTED_MEM_HAPP,
	     REGMGR_REGION_FINAL_STATE_OFF, 1, tmem_alloc_saturation_test),
	CASE(HAPP_UT_PROC_SATURATION_STRESS, "HAPP Saturation Stress",
	     TRUSTED_MEM_HAPP, REGMGR_REGION_FINAL_STATE_OFF,
	     UT_SATURATION_STRESS_ROUNDS, tmem_alloc_saturation_test),
	CASE(HAPP_UT_PROC_REGION_DEFER, "HAPP Region Defer Off",
	     TRUSTED_MEM_HAPP, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_defer_off_test),
	CASE(HAPP_UT_PROC_REGION_ONLINE_CNT, "HAPP Region Online Count",
	     TRUSTED_MEM_HAPP, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_online_count_test),
	CASE(HAPP_UT_PROC_REGION_STRESS, "HAPP Region On/Off Stress",
	     TRUSTED_MEM_HAPP, REGMGR_REGION_FINAL_STATE_OFF,
	     MEM_REGION_ON_OFF_STREE_ROUND, tmem_region_on_off_stress_test),
	CASE(HAPP_UT_PROC_ALLOC_MULTITHREAD, "HAPP Alloc Multi-thread",
	     TRUSTED_MEM_HAPP, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_alloc_multithread_test),
	CASE(HAPP_UT_PROC_ALL, "HAPP Run ALL", TRUSTED_MEM_HAPP,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_regmgr_run_all),

	CASE(HAPP_EXTRA_UT_PROC_BASIC, "HAPP Extra Basic",
	     TRUSTED_MEM_HAPP_EXTRA, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_basic_test),
	CASE(HAPP_EXTRA_UT_PROC_SIMPLE_ALLOC, "HAPP Extra Alloc Simple",
	     TRUSTED_MEM_HAPP_EXTRA, REGMGR_REGION_FINAL_STATE_OFF,
	     MEM_UNORDER_SIZE_TEST_CFG_DISABLE, tmem_alloc_simple_test),
	CASE(HAPP_EXTRA_UT_PROC_UNORDERED_SIZE,
	     "HAPP Extra Alloc Un-ordered Size", TRUSTED_MEM_HAPP_EXTRA,
	     REGMGR_REGION_FINAL_STATE_OFF, MEM_UNORDER_SIZE_TEST_CFG_ENABLE,
	     tmem_alloc_simple_test),
	CASE(HAPP_EXTRA_UT_PROC_ALIGNMENT, "HAPP Extra Alloc Alignment",
	     TRUSTED_MEM_HAPP_EXTRA, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_alloc_alignment_test),
	CASE(HAPP_EXTRA_UT_PROC_SATURATION, "HAPP Extra Saturation",
	     TRUSTED_MEM_HAPP_EXTRA, REGMGR_REGION_FINAL_STATE_OFF, 1,
	     tmem_alloc_saturation_test),
	CASE(HAPP_EXTRA_UT_PROC_SATURATION_STRESS,
	     "HAPP Extra Saturation Stress", TRUSTED_MEM_HAPP_EXTRA,
	     REGMGR_REGION_FINAL_STATE_OFF, UT_SATURATION_STRESS_ROUNDS,
	     tmem_alloc_saturation_test),
	CASE(HAPP_EXTRA_UT_PROC_REGION_DEFER, "HAPP Extra Region Defer Off",
	     TRUSTED_MEM_HAPP_EXTRA, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_defer_off_test),
	CASE(HAPP_EXTRA_UT_PROC_REGION_ONLINE_CNT,
	     "HAPP Extra Region Online Count", TRUSTED_MEM_HAPP_EXTRA,
	     REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_online_count_test),
	CASE(HAPP_EXTRA_UT_PROC_REGION_STRESS,
	     "HAPP Extra Region On/Off Stress", TRUSTED_MEM_HAPP_EXTRA,
	     REGMGR_REGION_FINAL_STATE_OFF, MEM_REGION_ON_OFF_STREE_ROUND,
	     tmem_region_on_off_stress_test),
	CASE(HAPP_EXTRA_UT_PROC_ALLOC_MULTITHREAD,
	     "HAPP Extra Alloc Multi-thread", TRUSTED_MEM_HAPP_EXTRA,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_alloc_multithread_test),
	CASE(HAPP_EXTRA_UT_PROC_ALL, "HAPP Extra Run ALL",
	     TRUSTED_MEM_HAPP_EXTRA, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_run_all),
#endif

#ifdef CONFIG_MTK_SDSP_MEM_SUPPORT
	CASE(SDSP_UT_PROC_BASIC, "SDSP Basic", TRUSTED_MEM_SDSP,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_basic_test),
	CASE(SDSP_UT_PROC_SIMPLE_ALLOC, "SDSP Alloc Simple", TRUSTED_MEM_SDSP,
	     REGMGR_REGION_FINAL_STATE_OFF, MEM_UNORDER_SIZE_TEST_CFG_DISABLE,
	     tmem_alloc_simple_test),
	CASE(SDSP_UT_PROC_UNORDERED_SIZE, "SDSP Alloc Un-ordered Size",
	     TRUSTED_MEM_SDSP, REGMGR_REGION_FINAL_STATE_OFF,
	     MEM_UNORDER_SIZE_TEST_CFG_ENABLE, tmem_alloc_simple_test),
	CASE(SDSP_UT_PROC_ALIGNMENT, "SDSP Alloc Alignment", TRUSTED_MEM_SDSP,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_alloc_alignment_test),
	CASE(SDSP_UT_PROC_SATURATION, "SDSP Saturation", TRUSTED_MEM_SDSP,
	     REGMGR_REGION_FINAL_STATE_OFF, 1, tmem_alloc_saturation_test),
	CASE(SDSP_UT_PROC_SATURATION_STRESS, "SDSP Saturation Stress",
	     TRUSTED_MEM_SDSP, REGMGR_REGION_FINAL_STATE_OFF,
	     UT_SATURATION_STRESS_ROUNDS, tmem_alloc_saturation_test),
	CASE(SDSP_UT_PROC_REGION_DEFER, "SDSP Region Defer Off",
	     TRUSTED_MEM_SDSP, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_defer_off_test),
	CASE(SDSP_UT_PROC_REGION_ONLINE_CNT, "SDSP Region Online Count",
	     TRUSTED_MEM_SDSP, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_online_count_test),
	CASE(SDSP_UT_PROC_REGION_STRESS, "SDSP Region On/Off Stress",
	     TRUSTED_MEM_SDSP, REGMGR_REGION_FINAL_STATE_OFF,
	     MEM_REGION_ON_OFF_STREE_ROUND, tmem_region_on_off_stress_test),
	CASE(SDSP_UT_PROC_ALLOC_MULTITHREAD, "SDSP Alloc Multi-thread",
	     TRUSTED_MEM_SDSP, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_alloc_multithread_test),
	CASE(SDSP_UT_PROC_ALL, "SDSP Run ALL", TRUSTED_MEM_SDSP,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_regmgr_run_all),
#endif

#ifdef CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT
	CASE(SDSP_SHARED_UT_PROC_BASIC, "SDSP Shared Basic",
	     TRUSTED_MEM_SDSP_SHARED, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_basic_test),
	CASE(SDSP_SHARED_UT_PROC_SIMPLE_ALLOC, "SDSP Shared Alloc Simple",
	     TRUSTED_MEM_SDSP_SHARED, REGMGR_REGION_FINAL_STATE_OFF,
	     MEM_UNORDER_SIZE_TEST_CFG_DISABLE, tmem_alloc_simple_test),
	CASE(SDSP_SHARED_UT_PROC_UNORDERED_SIZE,
	     "SDSP Shared Alloc Un-ordered Size", TRUSTED_MEM_SDSP_SHARED,
	     REGMGR_REGION_FINAL_STATE_OFF, MEM_UNORDER_SIZE_TEST_CFG_ENABLE,
	     tmem_alloc_simple_test),
	CASE(SDSP_SHARED_UT_PROC_ALIGNMENT, "SDSP Shared Alloc Alignment",
	     TRUSTED_MEM_SDSP_SHARED, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_alloc_alignment_test),
	CASE(SDSP_SHARED_UT_PROC_SATURATION, "SDSP Shared Saturation",
	     TRUSTED_MEM_SDSP_SHARED, REGMGR_REGION_FINAL_STATE_OFF, 1,
	     tmem_alloc_saturation_test),
	CASE(SDSP_SHARED_UT_PROC_SATURATION_STRESS,
	     "SDSP Shared Saturation Stress", TRUSTED_MEM_SDSP_SHARED,
	     REGMGR_REGION_FINAL_STATE_OFF, UT_SATURATION_STRESS_ROUNDS,
	     tmem_alloc_saturation_test),
	CASE(SDSP_SHARED_UT_PROC_REGION_DEFER, "SDSP Shared Region Defer Off",
	     TRUSTED_MEM_SDSP_SHARED, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_defer_off_test),
	CASE(SDSP_SHARED_UT_PROC_REGION_ONLINE_CNT,
	     "SDSP Shared Region Online Count", TRUSTED_MEM_SDSP_SHARED,
	     REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_region_online_count_test),
	CASE(SDSP_SHARED_UT_PROC_REGION_STRESS,
	     "SDSP Shared Region On/Off Stress", TRUSTED_MEM_SDSP_SHARED,
	     REGMGR_REGION_FINAL_STATE_OFF, MEM_REGION_ON_OFF_STREE_ROUND,
	     tmem_region_on_off_stress_test),
	CASE(SDSP_SHARED_UT_PROC_ALLOC_MULTITHREAD,
	     "SDSP Shared Alloc Multi-thread", TRUSTED_MEM_SDSP_SHARED,
	     REGMGR_REGION_FINAL_STATE_OFF, 0, tmem_alloc_multithread_test),
	CASE(SDSP_SHARED_UT_PROC_ALL, "SDSP Shared Run ALL",
	     TRUSTED_MEM_SDSP_SHARED, REGMGR_REGION_FINAL_STATE_OFF, 0,
	     tmem_regmgr_run_all),
#endif

	CASE(TMEM_UT_CORE_MULTIPLE_SSMR_REGION_REQUEST,
	     "Multiple SSMR Region Request", 0, 0, 0,
	     multiple_ssmr_region_request),

#if MULTIPLE_REGION_MULTIPLE_THREAD_TEST_ENABLE
	CASE(TMEM_UT_CORE_MULTIPLE_REGION_MULTIPLE_THREAD_ALLOC,
	     "Multiple Region Multiple Thread", 0, 0, 0,
	     multiple_region_multiple_thread_alloc),
#endif

#if MTEE_MCHUNKS_MULTIPLE_THREAD_TEST_ENABLE
	CASE(TMEM_UT_CORE_MTEE_MCHUNKS_MULTIPLE_THREAD_ALLOC,
	     "MTEE MChunks Multiple Thread", 0, 0, 0,
	     mtee_mchunks_multiple_thread_alloc),
#endif

#if defined(CONFIG_MTK_SECURE_MEM_SUPPORT)                                     \
	&& defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
	CASE(FR_UT_PROC_CONFIG_PROT_REGION, "Set TEE Protect Region Test", 0, 0,
	     0, config_tee_prot_region_test),
#endif

#ifdef TCORE_MEMORY_LEAK_DETECTION_SUPPORT
	CASE(TMEM_MEMORY_LEAK_DETECTION_CHECK, "Memory Leak Detection Test", 0,
	     0, 0, mld_check_test),
#endif

#ifdef TCORE_PROFILING_SUPPORT
	CASE(TMEM_PROFILE_DUMP, "Profiling Dump Test", 0, 0, 0,
	     profile_dump_all),
#endif
};

#define TEST_CASE_COUNT ARRAY_SIZE(test_cases)

static int __init tmem_ut_cases_init(void)
{
	int idx;

	pr_info("%s:%d\n", __func__, __LINE__);
	UNUSED(tmem_region_on_off_stress_test);
	UNUSED(tmem_regmgr_run_all);

	for (idx = 0; idx < TEST_CASE_COUNT; idx++) {
		register_ut_test_case(
			test_cases[idx].description, test_cases[idx].cmd,
			test_cases[idx].param1, test_cases[idx].param2,
			test_cases[idx].param3, test_cases[idx].func);
	}

	pr_info("%s:%d (end)\n", __func__, __LINE__);
	return TMEM_OK;
}

static void __exit tmem_ut_cases_exit(void)
{
}

module_init(tmem_ut_cases_init);
module_exit(tmem_ut_cases_exit);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Trusted Memory Test Cases");
