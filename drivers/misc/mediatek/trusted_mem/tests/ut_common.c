// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include <linux/sizes.h>

#include "private/mld_helper.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "private/tmem_proc.h"
#include "private/tmem_cfg.h"
#include "private/tmem_entry.h"
#include "private/tmem_priv.h"
#include "private/ut_cmd.h"
#include "tests/ut_common.h"

static enum UT_RET_STATE regmgr_state_check(int mem_idx, int region_final_state)
{
	if (region_final_state == REGMGR_REGION_FINAL_STATE_ON) {
		ASSERT_TRUE(tmem_core_is_regmgr_region_on(mem_idx),
			    "regmgr reg state check");
	} else {
		/* wait defer timeout before check for off state */
		mdelay(REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS);

		ASSERT_FALSE(tmem_core_is_regmgr_region_on(mem_idx),
			     "regmgr reg state check");
	}

	return UT_STATE_PASS;
}

enum UT_RET_STATE all_regmgr_state_off_check(void)
{
	int mem_idx;

	/* wait defer timeout before check for off state */
	mdelay(REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS);

	for (mem_idx = 0; mem_idx < TRUSTED_MEM_MAX; mem_idx++) {
		if (tmem_core_is_device_registered(mem_idx)) {
			/* wait one more time for 2D_FR region for safe
			 * due to pmem will invoke it when pmem unrefence mem
			 */
			if (mem_idx == TRUSTED_MEM_2D_FR)
				mdelay(REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS);

			ASSERT_EQ(0,
				  tmem_core_get_regmgr_region_ref_cnt(mem_idx),
				  "reg reference count check");
			ASSERT_FALSE(tmem_core_is_regmgr_region_on(mem_idx),
				     "regmgr reg state check");
		}
	}

	return UT_STATE_PASS;
}

enum UT_RET_STATE mem_basic_test(enum TRUSTED_MEM_TYPE mem_type,
				 int region_final_state)
{
	ASSERT_EQ(0, tmem_core_ssmr_allocate(mem_type), "ssmr allocate check");
	ASSERT_EQ(0, tmem_core_ssmr_release(mem_type), "ssmr release check");
	ASSERT_EQ(0, tmem_core_session_open(mem_type), "peer session open");
	ASSERT_EQ(0, tmem_core_session_close(mem_type), "peer session close");
	ASSERT_EQ(0, tmem_core_regmgr_online(mem_type), "regmgr region online");
	ASSERT_EQ(0, tmem_core_regmgr_offline(mem_type),
		  "regmgr region offline");
	ASSERT_EQ(0, regmgr_state_check(mem_type, region_final_state),
		  "reg state check");

	return UT_STATE_PASS;
}

static enum UT_RET_STATE mem_alloc_variant(enum TRUSTED_MEM_TYPE mem_type,
					   u8 *mem_owner, bool align,
					   bool clean, bool un_order_sz_enable)
{
	int ret;
	u32 alignment, chunk_size;
	u32 handle = 0;
	u32 ref_count = 0;
	u32 try_size;
	u32 max_try_size = SZ_16M;
	u32 min_alloc_sz = tmem_core_get_min_chunk_size(mem_type);

	for (chunk_size = min_alloc_sz; chunk_size <= max_try_size;
	     chunk_size *= 2) {
		alignment = (align ? chunk_size : 0);

		if (un_order_sz_enable)
			try_size = chunk_size + SZ_1K;
		else
			try_size = chunk_size;

		if (try_size > max_try_size)
			try_size = max_try_size;

		ret = tmem_core_alloc_chunk(mem_type, alignment, try_size,
					    &ref_count, &handle, mem_owner, 0,
					    clean);

		if ((alignment == 0) || (alignment >= try_size)) {
			ASSERT_EQ(0, ret, "alloc chunk memory");
			ASSERT_EQ(1, ref_count, "reference count check");
			ASSERT_NE(0, handle, "handle check");

			ret = tmem_core_unref_chunk(mem_type, handle, mem_owner,
						    0);
			ASSERT_EQ(0, ret, "free chunk memory");
		} else { /* alignment < try_size */
#ifdef TMEM_SMALL_ALIGNMENT_AUTO_ADJUST
			ASSERT_EQ(0, ret, "alloc chunk memory");
			ASSERT_EQ(1, ref_count, "reference count check");
			ASSERT_NE(0, handle, "handle check");

			ret = tmem_core_unref_chunk(mem_type, handle, mem_owner,
						    0);
			ASSERT_EQ(0, ret, "free chunk memory");
#else
			ASSERT_NE(0, ret, "alloc chunk memory");
#endif
		}
	}

	return UT_STATE_PASS;
}

enum UT_RET_STATE mem_alloc_simple_test(enum TRUSTED_MEM_TYPE mem_type,
					u8 *mem_owner, int region_final_state,
					int un_order_sz_cfg)
{
	int ret;
	u32 handle, ref_count;
	bool un_order_sz_enable =
		(un_order_sz_cfg == MEM_UNORDER_SIZE_TEST_CFG_ENABLE);

	/* out of memory check */
	ret = tmem_core_alloc_chunk(mem_type, 0, SZ_256M * 2, &ref_count,
				    &handle, mem_owner, 0, 0);
	ASSERT_NE(0, ret, "out of memory check");

	/* alloc one and free one */
	ASSERT_EQ(0, mem_alloc_variant(mem_type, mem_owner, false, false,
				       un_order_sz_enable),
		  "alloc wo align wo clean");
	ASSERT_EQ(0, mem_alloc_variant(mem_type, mem_owner, false, true,
				       un_order_sz_enable),
		  "alloc wo align w clean");
	ASSERT_EQ(0, mem_alloc_variant(mem_type, mem_owner, true, false,
				       un_order_sz_enable),
		  "alloc w align wo clean");
	ASSERT_EQ(0, mem_alloc_variant(mem_type, mem_owner, true, true,
				       un_order_sz_enable),
		  "alloc w align w clean");

	ASSERT_EQ(0, regmgr_state_check(mem_type, region_final_state),
		  "reg state check");

	return UT_STATE_PASS;
}

enum UT_RET_STATE mem_alloc_alignment_test(enum TRUSTED_MEM_TYPE mem_type,
					   u8 *mem_owner,
					   int region_final_state)
{
	int ret;
	u32 alignment, chunk_size;
	u32 handle = 0;
	u32 ref_count = 0;
	u32 min_chunk_sz = tmem_core_get_min_chunk_size(mem_type);

	/* alignment is less than size, we expect result by defines:
	 * expect fail if TMEM_SMALL_ALIGNMENT_AUTO_ADJUST is not defined
	 * expect pass if TMEM_SMALL_ALIGNMENT_AUTO_ADJUST is defined
	 */
	for (chunk_size = min_chunk_sz; chunk_size <= SZ_16M; chunk_size *= 2) {
		alignment = chunk_size / 2;

		ret = tmem_core_alloc_chunk(mem_type, alignment, chunk_size,
					    &ref_count, &handle, mem_owner, 0,
					    0);
#ifdef TMEM_SMALL_ALIGNMENT_AUTO_ADJUST
		ASSERT_EQ(0, ret, "alloc chunk memory");
		ASSERT_EQ(1, ref_count, "reference count check");
		ASSERT_NE(0, handle, "handle check");

		ret = tmem_core_unref_chunk(mem_type, handle, mem_owner, 0);
		ASSERT_EQ(0, ret, "free chunk memory");
#else
		ASSERT_NE(0, ret, "alloc chunk memory");
#endif
	}

	/* alignment is larger than size, we expect pass */
	for (chunk_size = min_chunk_sz; chunk_size <= SZ_16M; chunk_size *= 2) {
		alignment = chunk_size * 2;

		ret = tmem_core_alloc_chunk(mem_type, alignment, chunk_size,
					    &ref_count, &handle, mem_owner, 0,
					    0);
		ASSERT_EQ(0, ret, "alloc chunk memory");
		ASSERT_EQ(1, ref_count, "reference count check");
		ASSERT_NE(0, handle, "handle check");

		ret = tmem_core_unref_chunk(mem_type, handle, mem_owner, 0);
		ASSERT_EQ(0, ret, "free chunk memory");
	}

	ASSERT_EQ(0, regmgr_state_check(mem_type, region_final_state),
		  "reg state check");

	return UT_STATE_PASS;
}

static u32 *g_mem_handle_list;
enum UT_RET_STATE mem_handle_list_init(enum TRUSTED_MEM_TYPE mem_type)
{
	int max_pool_size = tmem_core_get_max_pool_size(mem_type);
	u32 min_chunk_sz = tmem_core_get_min_chunk_size(mem_type);

	int max_handle_cnt = max_pool_size / min_chunk_sz;

	if (INVALID(g_mem_handle_list)) {
		g_mem_handle_list =
			mld_kmalloc(sizeof(u32) * max_handle_cnt, GFP_KERNEL);
	}
	ASSERT_NOTNULL(g_mem_handle_list, "alloc memory for mem handles");

	return UT_STATE_PASS;
}

static enum UT_RET_STATE mem_handle_list_re_init(enum TRUSTED_MEM_TYPE mem_type,
						 u32 min_chunk_sz)
{
	int max_pool_size = tmem_core_get_max_pool_size(mem_type);
	int max_handle_cnt = max_pool_size / min_chunk_sz;

	mem_handle_list_deinit();
	if (INVALID(g_mem_handle_list)) {
		g_mem_handle_list =
			mld_kmalloc(sizeof(u32) * max_handle_cnt, GFP_KERNEL);
	}
	ASSERT_NOTNULL(g_mem_handle_list, "alloc memory for mem handles");

	return UT_STATE_PASS;
}

enum UT_RET_STATE mem_handle_list_deinit(void)
{
	if (VALID(g_mem_handle_list)) {
		mld_kfree(g_mem_handle_list);
		g_mem_handle_list = NULL;
	}

	return UT_STATE_PASS;
}

static u32 get_saturation_test_min_chunk_size(enum TRUSTED_MEM_TYPE mem_type)
{
	if (is_mtee_mchunks(mem_type))
		return get_saturation_stress_pmem_min_chunk_size();

	return tmem_core_get_min_chunk_size(mem_type);
}

static enum UT_RET_STATE
mem_alloc_saturation_variant(enum TRUSTED_MEM_TYPE mem_type, u8 *mem_owner,
			     bool align, bool clean)
{
	int ret;
	int chunk_num;
	u32 alignment = 0, chunk_size;
	u32 ref_count = 0;
	u32 one_more_handle;
	int max_pool_size = tmem_core_get_max_pool_size(mem_type);
	int max_items;
	u32 min_chunk_sz = get_saturation_test_min_chunk_size(mem_type);

	for (chunk_size = min_chunk_sz; chunk_size <= SZ_16M; chunk_size *= 2) {
		max_items = (max_pool_size / chunk_size);

		/* alloc until full */
		for (chunk_num = 0; chunk_num < max_items; chunk_num++) {
			alignment = (align ? chunk_size : 0);

			ret = tmem_core_alloc_chunk(
				mem_type, alignment, chunk_size, &ref_count,
				&g_mem_handle_list[chunk_num], mem_owner, 0,
				clean);
			ASSERT_EQ(0, ret, "alloc chunk memory");
			ASSERT_EQ(1, ref_count, "reference count check");
			ASSERT_NE(0, g_mem_handle_list[chunk_num],
				  "handle check");
		}

		/* one more allocation (expect fail) */
		ret = tmem_core_alloc_chunk(mem_type, alignment, chunk_size,
					    &ref_count, &one_more_handle,
					    mem_owner, 0, clean);
		ASSERT_NE(0, ret, "alloc one more chunk memory");

		/* free all chunk */
		for (chunk_num = 0; chunk_num < max_items; chunk_num++) {
			ret = tmem_core_unref_chunk(
				mem_type, g_mem_handle_list[chunk_num],
				mem_owner, 0);
			ASSERT_EQ(0, ret, "free chunk memory");
		}
	}

	return UT_STATE_PASS;
}

enum UT_RET_STATE mem_alloc_saturation_test(enum TRUSTED_MEM_TYPE mem_type,
					    u8 *mem_owner,
					    int region_final_state, int round)
{
	int ret;

	while (round-- > 0) {
		ret = mem_alloc_saturation_variant(mem_type, mem_owner, false,
						   false);
		ASSERT_EQ(0, ret, "alloc saturation wo align wo clean");
		ret = mem_alloc_saturation_variant(mem_type, mem_owner, false,
						   true);
		ASSERT_EQ(0, ret, "alloc saturation wo align w clean");
		ret = mem_alloc_saturation_variant(mem_type, mem_owner, true,
						   false);
		ASSERT_EQ(0, ret, "alloc saturation w align wo clean");
		ret = mem_alloc_saturation_variant(mem_type, mem_owner, true,
						   true);
		ASSERT_EQ(0, ret, "alloc saturation w align w clean");
	}

	ASSERT_EQ(0, regmgr_state_check(mem_type, region_final_state),
		  "reg state check");

	return UT_STATE_PASS;
}

enum UT_RET_STATE
mem_regmgr_region_defer_off_test(enum TRUSTED_MEM_TYPE mem_type, u8 *mem_owner,
				 int region_final_state)
{
	int ret;
	u32 handle, ref_count;
	int defer_ms = (REGMGR_REGION_DEFER_OFF_DELAY_MS - 100);
	int defer_end_ms = (REGMGR_REGION_DEFER_OFF_OPERATION_LATENCY_MS + 100);
	u32 min_chunk_sz = tmem_core_get_min_chunk_size(mem_type);

	/* alloc op will turn on regmgr region */
	ret = tmem_core_alloc_chunk(mem_type, min_chunk_sz, min_chunk_sz,
				    &ref_count, &handle, mem_owner, 0, 0);
	ASSERT_EQ(0, ret, "alloc chunk memory");

	/* regmgr region is deferred off, so still on */
	ASSERT_TRUE(tmem_core_is_regmgr_region_on(mem_type),
		    "regmgr reg st on check");
	ASSERT_EQ(1, ref_count, "reference count check");
	ASSERT_NE(0, handle, "handle check");

	/* remain on */
	ASSERT_TRUE(tmem_core_is_regmgr_region_on(mem_type),
		    "regmgr reg st on check");

	/* remain on (mem is still occupied) */
	mdelay(defer_ms);
	ASSERT_TRUE(tmem_core_is_regmgr_region_on(mem_type),
		    "regmgr reg st on check");

	/* free op will turn off regmgr region if success */
	ret = tmem_core_unref_chunk(mem_type, handle, mem_owner, 0);
	ASSERT_EQ(0, ret, "free chunk memory");

	/* remain on (defer off is still not happened) */
	mdelay(defer_ms);
	ASSERT_TRUE(tmem_core_is_regmgr_region_on(mem_type),
		    "regmgr reg st on check");

	/* should be off (defer off should have happened) */
	mdelay(defer_end_ms);
	ASSERT_FALSE(tmem_core_is_regmgr_region_on(mem_type),
		     "regmgr reg st off check");

	/* final check */
	ASSERT_EQ(0, regmgr_state_check(mem_type, region_final_state),
		  "reg state check");

	return UT_STATE_PASS;
}

static enum UT_RET_STATE mem_delay_after_free(enum TRUSTED_MEM_TYPE mem_type,
					      u8 *mem_owner, int alloc_cnt,
					      long delay_ms)
{
	int ret;
	int chunk_num;
	u32 handle = 0;
	u32 ref_count;
	u32 min_chunk_sz = tmem_core_get_min_chunk_size(mem_type);

	for (chunk_num = 0; chunk_num < alloc_cnt; chunk_num++) {
		ret = tmem_core_alloc_chunk(mem_type, 0, min_chunk_sz,
					    &ref_count, &handle, mem_owner, 0,
					    0);
		ASSERT_EQ(0, ret, "alloc chunk memory");
		ASSERT_EQ(1, ref_count, "reference count check");
		ASSERT_NE(0, handle, "handle check");

		ret = tmem_core_unref_chunk(mem_type, handle, mem_owner, 0);
		ASSERT_EQ(0, ret, "free chunk memory");

		mdelay(delay_ms);
	}

	return UT_STATE_PASS;
}

enum UT_RET_STATE
mem_regmgr_region_online_count_test(enum TRUSTED_MEM_TYPE mem_type,
				    u8 *mem_owner, int region_final_state)
{
	u64 regmgr_reg_on_cnt_start;
	u64 regmgr_reg_on_cnt_end;
	u64 regmgr_reg_on_cnt_diff;
	long delay_ms_after_free;
	int alloc_cnt;

	/* make sure region is off before starting */
	mdelay(REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS);

	/* regmgr region will online 4 times */
	regmgr_reg_on_cnt_start =
		tmem_core_get_regmgr_region_online_cnt(mem_type);
	alloc_cnt = 4;
	delay_ms_after_free = REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS;
	ASSERT_EQ(0, mem_delay_after_free(mem_type, mem_owner, alloc_cnt,
					  delay_ms_after_free),
		  "alloc and free with delay");
	regmgr_reg_on_cnt_end =
		tmem_core_get_regmgr_region_online_cnt(mem_type);
	regmgr_reg_on_cnt_diff =
		regmgr_reg_on_cnt_end - regmgr_reg_on_cnt_start;
	ASSERT_EQ(4, (u32)regmgr_reg_on_cnt_diff, "region online cnt");
	ASSERT_FALSE(tmem_core_is_regmgr_region_on(mem_type),
		     "regmgr reg state check");

	/* regmgr region will online 1 time */
	regmgr_reg_on_cnt_start =
		tmem_core_get_regmgr_region_online_cnt(mem_type);
	alloc_cnt = 10;
	delay_ms_after_free = 10;
	ASSERT_EQ(0, mem_delay_after_free(mem_type, mem_owner, alloc_cnt,
					  delay_ms_after_free),
		  "alloc and free with delay");
	regmgr_reg_on_cnt_end =
		tmem_core_get_regmgr_region_online_cnt(mem_type);
	regmgr_reg_on_cnt_diff =
		regmgr_reg_on_cnt_end - regmgr_reg_on_cnt_start;
	ASSERT_EQ(1, (u32)regmgr_reg_on_cnt_diff, "region online cnt");
	ASSERT_TRUE(tmem_core_is_regmgr_region_on(mem_type),
		    "regmgr reg state check");

	/* final check */
	ASSERT_EQ(0, regmgr_state_check(mem_type, region_final_state),
		  "reg state check");

	return UT_STATE_PASS;
}

enum UT_RET_STATE mem_region_on_off_stress_test(enum TRUSTED_MEM_TYPE mem_type,
						int region_final_state,
						int round)
{
	while (round-- > 1) {
		ASSERT_EQ(0, tmem_core_ssmr_allocate(mem_type),
			  "ssmr allocate check");
		ASSERT_EQ(0, tmem_core_ssmr_release(mem_type),
			  "ssmr release check");
	}

	ASSERT_EQ(0, tmem_core_regmgr_online(mem_type), "regmgr region online");
	ASSERT_EQ(0, tmem_core_regmgr_offline(mem_type),
		  "regmgr region offline");

	ASSERT_EQ(0, regmgr_state_check(mem_type, region_final_state),
		  "reg state check");

	return UT_STATE_PASS;
}

#define MEM_SPAWN_THREAD_COUNT (8)
#define MEM_THREAD_NAME_LEN (32)
struct mem_thread_param {
	char name[MEM_THREAD_NAME_LEN];
	int alloc_chunk_size;
	int alloc_total_size;
	u32 *handle_list;
	int thread_id;
	bool running;
	enum TRUSTED_MEM_TYPE mem_type;
	struct completion comp;
};
static struct mem_thread_param thread_param[TRUSTED_MEM_MAX]
					   [MEM_SPAWN_THREAD_COUNT];
static struct task_struct *mem_kthread[TRUSTED_MEM_MAX][MEM_SPAWN_THREAD_COUNT];

static int mem_thread_alloc_test(void *data)
{
	int ret;
	struct mem_thread_param *param = (struct mem_thread_param *)data;
	int chunk_size = param->alloc_chunk_size;
	int max_items = param->alloc_total_size / chunk_size;
	int idx;
	u32 ref_count;
	u8 *owner = param->name;
	int mem_type = param->mem_type;

	for (idx = 0; idx < max_items; idx++) {
		ret = tmem_core_alloc_chunk(mem_type, 0, chunk_size, &ref_count,
					    &param->handle_list[idx], owner, 0,
					    0);
		ASSERT_EQ(0, ret, "alloc chunk memory");
		ASSERT_EQ(1, ref_count, "reference count check");
		ASSERT_NE(0, param->handle_list[idx], "handle check");
	}

	for (idx = (max_items - 1); idx >= 0; idx--) {
		ret = tmem_core_unref_chunk(mem_type, param->handle_list[idx],
					    owner, 0);
		ASSERT_EQ(0, ret, "free chunk memory");
	}

	complete(&param->comp);

	pr_debug("[UT_TEST]mem%d_thread_%d is done\n", mem_type,
		 param->thread_id);
	return UT_STATE_PASS;
}

static enum UT_RET_STATE mem_create_run_thread(enum TRUSTED_MEM_TYPE mem_type)
{
	int idx;
	int chunk_cnt;
	u32 min_alloc_sz = tmem_core_get_min_chunk_size(mem_type);
	u32 max_total_sz =
		tmem_core_get_max_pool_size(mem_type) / MEM_SPAWN_THREAD_COUNT;

	/* to speed up test */
	if (is_mtee_mchunks(mem_type))
		min_alloc_sz = SZ_512K;

	/* create new thread */
	for (idx = 0; idx < MEM_SPAWN_THREAD_COUNT; idx++) {
		memset(&thread_param[mem_type][idx], 0x0,
		       sizeof(struct mem_thread_param));
		snprintf(thread_param[mem_type][idx].name, MEM_THREAD_NAME_LEN,
			 "mem%d_thread_%d", mem_type, idx);
		thread_param[mem_type][idx].mem_type = mem_type;
		thread_param[mem_type][idx].alloc_chunk_size = min_alloc_sz;
		thread_param[mem_type][idx].alloc_total_size = max_total_sz;
		chunk_cnt = thread_param[mem_type][idx].alloc_total_size
			    / thread_param[mem_type][idx].alloc_chunk_size;
		thread_param[mem_type][idx].running = false;
		thread_param[mem_type][idx].thread_id = idx;
		thread_param[mem_type][idx].handle_list =
			mld_kmalloc(sizeof(u32) * chunk_cnt, GFP_KERNEL);
		ASSERT_NOTNULL(thread_param[mem_type][idx].handle_list,
			       "create handle list");
		init_completion(&thread_param[mem_type][idx].comp);

		mem_kthread[mem_type][idx] =
			kthread_run(mem_thread_alloc_test,
				    (void *)&thread_param[mem_type][idx],
				    thread_param[mem_type][idx].name);
		if (IS_ERR(mem_kthread[mem_type][idx]))
			ASSERT_NOTNULL(NULL, "create kthread");
		thread_param[mem_type][idx].running = true;
	}

	return UT_STATE_PASS;
}

static enum UT_RET_STATE mem_wait_run_thread(enum TRUSTED_MEM_TYPE mem_type)
{
	int idx;
	int ret;
	int wait_timeout_ms = get_multithread_test_wait_completion_time();

	/* wait for thread to complete */
	for (idx = 0; idx < MEM_SPAWN_THREAD_COUNT; idx++) {
		if (thread_param[mem_type][idx].running) {
			pr_debug("[UT_TEST]waiting for mem%d_thread_%d...\n",
				 mem_type, idx);
			ret = wait_for_completion_timeout(
				&thread_param[mem_type][idx].comp,
				msecs_to_jiffies(wait_timeout_ms));
			ASSERT_NE(0, ret, "kthread timeout check");
			pr_debug("[UT_TEST]mem%d_thread_%d is finished!\n",
				 mem_type, idx);
		} else {
			pr_err("[UT_TEST]mem%d_thread_%d is not started!\n",
			       mem_type, idx);
		}

		if (VALID(thread_param[mem_type][idx].handle_list))
			mld_kfree(thread_param[mem_type][idx].handle_list);
	}

	return UT_STATE_PASS;
}

enum UT_RET_STATE mem_alloc_multithread_test(enum TRUSTED_MEM_TYPE mem_type)
{
	int ret;

	ret = mem_create_run_thread(mem_type);
	ASSERT_EQ(0, ret, "create run thread check");

	ret = mem_wait_run_thread(mem_type);
	ASSERT_EQ(0, ret, "wait for running thread to stop check");

	ret = all_regmgr_state_off_check();
	ASSERT_EQ(0, ret, "all region state off check");

	return UT_STATE_PASS;
}

static enum UT_RET_STATE
mem_alloc_mixed_size_test_with_alignment(enum TRUSTED_MEM_TYPE mem_type,
					 u8 *mem_owner, u32 alignment)
{
	int ret;
	int chunk_size, chunk_count;
	int chunk_idx;
	u32 try_size;
	u32 max_try_size = SZ_16M;
	u32 handle, ref_count;
	u32 max_pool_size = tmem_core_get_max_pool_size(mem_type);
	u32 next_free_pos = 0x0;
	int remained_free_size;
	u32 used_size;

	if (!is_mtee_mchunks(mem_type))
		return UT_STATE_FAIL;

	if (mem_handle_list_re_init(mem_type, SZ_1M))
		return UT_STATE_FAIL;

	for (try_size = SZ_1M; try_size <= max_try_size;
	     try_size += SZ_1M) {
		chunk_idx = 0;
		remained_free_size = max_pool_size;
		chunk_size = try_size;
		next_free_pos = 0;

		/* Allocate one chunk of 4KB size */
		ret = tmem_core_alloc_chunk_priv(mem_type, alignment, SZ_4K,
						 &ref_count, &handle, mem_owner,
						 0, 0);
		ASSERT_EQ(0, ret, "alloc 4KB chunk memory");
		ASSERT_EQ(1, ref_count, "reference count check");
		ASSERT_NE(0, handle, "handle check");

		used_size = SZ_4K;
		if (alignment && (next_free_pos % alignment))
			used_size += alignment;
		next_free_pos += used_size;
		remained_free_size -= used_size;

		/* Try allocating more chunks */
		while (remained_free_size >= chunk_size) {

			used_size = chunk_size;
			if (alignment && (next_free_pos % alignment))
				used_size += alignment;

			ret = tmem_core_alloc_chunk_priv(
				mem_type, alignment, chunk_size, &ref_count,
				&g_mem_handle_list[chunk_idx], mem_owner, 0, 0);
			if (used_size <= remained_free_size) {
				ASSERT_EQ(0, ret, "alloc chunk memory");
				ASSERT_EQ(1, ref_count,
					  "reference count check");
				ASSERT_NE(0, g_mem_handle_list[chunk_idx],
					  "handle check");
			} else {
				ASSERT_NE(0, ret, "alloc chunk memory");
				break;
			}

			next_free_pos += used_size;
			remained_free_size -= used_size;
			chunk_idx++;
		}

		/* Should be failed for one more chunk */
		ret = tmem_core_alloc_chunk_priv(
			mem_type, alignment, chunk_size, &ref_count,
			&g_mem_handle_list[chunk_idx], mem_owner, 0, 0);
		ASSERT_NE(0, ret, "alloc chunk memory");

		/* Should be failed if no more 4KB chunk */
		used_size = SZ_4K;
		if (alignment && (next_free_pos % alignment))
			used_size += alignment;

		ret = tmem_core_alloc_chunk_priv(
			mem_type, alignment, SZ_4K, &ref_count,
			&g_mem_handle_list[chunk_idx], mem_owner, 0, 0);
		if (used_size <= remained_free_size) {
			ASSERT_EQ(0, ret, "alloc chunk memory");
			ASSERT_EQ(1, ref_count, "reference count check");
			ASSERT_NE(0, g_mem_handle_list[chunk_idx],
				  "handle check");
			ret = tmem_core_unref_chunk(
				mem_type, g_mem_handle_list[chunk_idx],
				mem_owner, 0);
			ASSERT_EQ(0, ret, "free 4KB chunk memory");
		} else {
			ASSERT_NE(0, ret, "alloc chunk memory");
		}

		/* Try to free all allocated chunks */
		chunk_count = chunk_idx;
		for (chunk_idx = 0; chunk_idx < chunk_count; chunk_idx++) {
			ret = tmem_core_unref_chunk(
				mem_type, g_mem_handle_list[chunk_idx],
				mem_owner, 0);
			ASSERT_EQ(0, ret, "free chunk memory");
		}

		/* Free the allocated one chunk of 4KB size */
		ret = tmem_core_unref_chunk(mem_type, handle, mem_owner, 0);
		ASSERT_EQ(0, ret, "free 4KB chunk memory");
	}

	return UT_STATE_PASS;
}

enum UT_RET_STATE mem_alloc_mixed_size_test(enum TRUSTED_MEM_TYPE mem_type,
					    u8 *mem_owner,
					    int region_final_state)
{
	int ret;
	u32 min_alloc_sz = tmem_core_get_min_chunk_size(mem_type);

	ret = mem_alloc_mixed_size_test_with_alignment(mem_type, mem_owner, 0);
	ASSERT_EQ(0, ret, "alloc mixed size alignment is 0");

	ret = mem_alloc_mixed_size_test_with_alignment(mem_type, mem_owner,
						       min_alloc_sz);
	ASSERT_EQ(0, ret, "alloc mixed size alignment with minimal chunk size");

	ASSERT_EQ(0, regmgr_state_check(mem_type, region_final_state),
		  "reg state check");

	return UT_STATE_PASS;
}

#if MULTIPLE_REGION_MULTIPLE_THREAD_TEST_ENABLE
static enum UT_RET_STATE mem_multi_type_alloc_multithread_test_locked(void)
{
	int mem_idx;
	int ret;

	for (mem_idx = TRUSTED_MEM_START; mem_idx < TRUSTED_MEM_MAX;
	     mem_idx++) {
		if (mem_idx == TRUSTED_MEM_2D_FR)
			continue;
		if (tmem_core_is_device_registered(mem_idx)) {
			ret = mem_create_run_thread(mem_idx);
			ASSERT_EQ(0, ret, "create run thread check");
		}
	}

	for (mem_idx = TRUSTED_MEM_START; mem_idx < TRUSTED_MEM_MAX;
	     mem_idx++) {
		if (mem_idx == TRUSTED_MEM_2D_FR)
			continue;
		if (tmem_core_is_device_registered(mem_idx)) {
			ret = mem_wait_run_thread(mem_idx);
			ASSERT_EQ(0, ret,
				  "wait for running thread to stop check");
		}
	}

	ret = all_regmgr_state_off_check();
	ASSERT_EQ(0, ret, "all region state off check");

	return UT_STATE_PASS;
}
#endif

#if MTEE_MCHUNKS_MULTIPLE_THREAD_TEST_ENABLE
static enum UT_RET_STATE mem_mtee_mchunks_alloc_multithread_test_locked(void)
{
	int mem_idx;
	int ret;

	for (mem_idx = TRUSTED_MEM_START; mem_idx < TRUSTED_MEM_MAX;
	     mem_idx++) {
		if (!is_mtee_mchunks(mem_idx))
			continue;
		if (mem_idx == TRUSTED_MEM_2D_FR)
			continue;
		if (tmem_core_is_device_registered(mem_idx)) {
			ret = mem_create_run_thread(mem_idx);
			ASSERT_EQ(0, ret, "create run thread check");
		}
	}

	for (mem_idx = TRUSTED_MEM_START; mem_idx < TRUSTED_MEM_MAX;
	     mem_idx++) {
		if (!is_mtee_mchunks(mem_idx))
			continue;
		if (mem_idx == TRUSTED_MEM_2D_FR)
			continue;
		if (tmem_core_is_device_registered(mem_idx)) {
			ret = mem_wait_run_thread(mem_idx);
			ASSERT_EQ(0, ret,
				  "wait for running thread to stop check");
		}
	}

	ret = all_regmgr_state_off_check();
	ASSERT_EQ(0, ret, "all region state off check");

	return UT_STATE_PASS;
}
#endif

static bool multi_type_alloc_multithread_locked;
static inline void set_multi_type_test_lock(bool lock_en)
{
	multi_type_alloc_multithread_locked = lock_en;
}
bool is_multi_type_alloc_multithread_test_locked(void)
{
	return multi_type_alloc_multithread_locked;
}

#if MULTIPLE_REGION_MULTIPLE_THREAD_TEST_ENABLE
enum UT_RET_STATE mem_multi_type_alloc_multithread_test(void)
{
	int ret;

	set_multi_type_test_lock(true);
	ret = mem_multi_type_alloc_multithread_test_locked();
	set_multi_type_test_lock(false);

	ASSERT_EQ(0, ret, "multi type alloc multithread test check");
	return UT_STATE_PASS;
}
#endif

#if MTEE_MCHUNKS_MULTIPLE_THREAD_TEST_ENABLE
enum UT_RET_STATE mem_mtee_mchunks_alloc_multithread_test(void)
{
	int ret;

	set_multi_type_test_lock(true);
	ret = mem_mtee_mchunks_alloc_multithread_test_locked();
	set_multi_type_test_lock(false);

	ASSERT_EQ(0, ret, "mtee multiple chunks multithread test check");
	return UT_STATE_PASS;
}
#endif
