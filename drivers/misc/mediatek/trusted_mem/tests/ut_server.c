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
#include "private/ut_cmd.h"
#include "tests/ut_api.h"

#define KERN_UT_RUN_ALL_STOP_ONCE_FAILED (1)

struct ut_test_case_statistics {
	enum UT_RET_STATE ret;
	u32 pass_cnt;
	u32 fail_cnt;
	u32 spend_sec;
	u32 spend_msec;
};

struct ut_test_case {
	u64 cmd;
	u64 param1;
	u64 param2;
	u64 param3;
	struct ut_test_case_statistics statistics;
	char *test_description;
	int (*test_case_cb)(struct ut_params *params, char *test_desc);
};

struct UT_TEST_CASE {
	struct ut_test_case item;
	struct list_head list;
};

static u32 ut_case_pass_count;
static u32 ut_case_fail_count;
static bool ut_case_halt;
static u64 ut_case_command;
static struct timeval ut_case_start_time;
static struct timeval ut_case_end_time;
void ut_set_halt(void)
{
	ut_case_halt = true;
}

bool ut_is_halt(void)
{
	return ut_case_halt;
}

void ut_increase_pass_cnt(void)
{
	ut_case_pass_count++;
}

void ut_increase_fail_cnt(void)
{
	ut_case_fail_count++;
}

static u32 ut_get_pass_cnt(void)
{
	return ut_case_pass_count;
}

static u32 ut_get_fail_cnt(void)
{
	return ut_case_fail_count;
}

static u32 ut_get_spend_sec(void)
{
	struct timeval *start_time = &ut_case_start_time;
	struct timeval *end_time = &ut_case_end_time;

	return GET_TIME_DIFF_SEC_P(start_time, end_time);
}

static u32 ut_get_spend_msec(void)
{
	struct timeval *start_time = &ut_case_start_time;
	struct timeval *end_time = &ut_case_end_time;

	return GET_TIME_DIFF_USEC_P(start_time, end_time);
}

static void ut_status_reset(u64 ut_cmd)
{
	ut_case_pass_count = 0;
	ut_case_fail_count = 0;
	ut_case_halt = false;
	ut_case_command = ut_cmd;
	do_gettimeofday(&ut_case_start_time);
}

static void ut_status_dump(void)
{
	do_gettimeofday(&ut_case_end_time);
	pr_info("[UT_CASE]================================================\n");
	pr_info("[UT_CASE]Executing UT command: %lld\n", ut_case_command);
	pr_info("[UT_CASE]  TOTAL TEST ITEMS: %d\n",
		ut_case_pass_count + ut_case_fail_count);
	pr_info("[UT_CASE]  PASS TEST ITEMS: %d\n", ut_case_pass_count);
	pr_info("[UT_CASE]  FAIL TEST ITEMS: %d\n", ut_case_fail_count);
	if (ut_case_fail_count == 0)
		pr_info("[UT_CASE]all UT test items are passed!!!\n");
	else
		pr_info("[UT_CASE]some items are failed, please check!!!\n");
	pr_info("[UT_CASE]  Spend time: %d.%d seconds\n", ut_get_spend_sec(),
		ut_get_spend_msec());
	pr_info("[UT_CASE]================================================\n");
}

static struct UT_TEST_CASE g_test_case_list;
static void reset_all_cases_status(void)
{
	struct UT_TEST_CASE *t_case, *tmp;
	struct ut_test_case_statistics *t_statistics;

	/* clang-format off */
	list_for_each_entry_safe(t_case, tmp, &g_test_case_list.list, list) {
		/* clang-format on */
		t_statistics = &t_case->item.statistics;
		t_statistics->ret = UT_STATE_UNSUPPORTED_CMD;
		t_statistics->pass_cnt = 0;
		t_statistics->fail_cnt = 0;
		t_statistics->spend_sec = 0;
		t_statistics->spend_msec = 0;
	}
}

static int run_one_test_case(struct UT_TEST_CASE *t_case, u64 cmd, u64 param1,
			     u64 param2, u64 param3)
{
	enum UT_RET_STATE case_ret;
	struct ut_params params;
	struct ut_test_case_statistics *statistics = &t_case->item.statistics;

	pr_info("[UT_SUITE]        UT case:%lld .....\n", cmd);

	ut_status_reset(cmd);

	params.cmd = cmd;
	params.param1 = param1;
	params.param2 = param2;
	params.param3 = param3;
	case_ret = t_case->item.test_case_cb(&params,
					     t_case->item.test_description);

	if (case_ret == UT_STATE_PASS)
		pr_debug("  UT case:%lld passed!\n", cmd);
	else if (case_ret == UT_STATE_FAIL)
		pr_debug("  UT case:%lld failed!\n", cmd);
	else
		pr_debug("  UT case:%lld unsupported!\n", cmd);

	ut_status_dump();

	statistics->pass_cnt = ut_get_pass_cnt();
	statistics->fail_cnt = ut_get_fail_cnt();
	statistics->spend_sec = ut_get_spend_sec();
	statistics->spend_msec = ut_get_spend_msec();
	statistics->ret = case_ret;
	return case_ret;
}

static void dump_run_cases_results(u64 cmd, bool run_all_cases)
{
	struct UT_TEST_CASE *t_case, *tmp;
	u32 all_pass_cnt = 0;
	u32 all_fail_cnt = 0;
	u32 all_spend_sec = 0;
	u32 all_spend_msec = 0;
	struct ut_test_case_statistics *t_statistics;

	pr_info("[UT_SUITE]Test Results: RUN %s\n",
		(run_all_cases ? "ALL" : "ONE"));
	pr_info("[UT_SUITE] CASE     PASS       FAIL     TIME\n");

	/* clang-format off */
	list_for_each_entry_safe(t_case, tmp, &g_test_case_list.list, list) {
		/* clang-format on */
		t_statistics = &t_case->item.statistics;
		if (t_statistics->ret != UT_STATE_UNSUPPORTED_CMD) {
			if (cmd == t_case->item.cmd || run_all_cases)
				pr_info("[UT_SUITE]%5lld     %-9d  %-7d  %05d.%06d sec\n",
					t_case->item.cmd,
					t_statistics->pass_cnt,
					t_statistics->fail_cnt,
					t_statistics->spend_sec,
					t_statistics->spend_msec);
			all_pass_cnt += t_statistics->pass_cnt;
			all_fail_cnt += t_statistics->fail_cnt;
			all_spend_sec += t_statistics->spend_sec;
			all_spend_msec += t_statistics->spend_msec;
		}
	}

	if (run_all_cases) {
		all_spend_sec += (all_spend_msec / 1000000);
		all_spend_msec = (all_spend_msec % 1000000);
		pr_info("[UT_SUITE] Summary:\n");
		pr_info("[UT_SUITE]  ALL     %-9d  %-7d  %05d.%06d sec\n",
			all_pass_cnt, all_fail_cnt, all_spend_sec,
			all_spend_msec);
	}
}

int invoke_ut_test_case(u64 cmd)
{
	struct UT_TEST_CASE *t_case, *tmp;
	enum UT_RET_STATE case_ret = UT_STATE_UNSUPPORTED_CMD;
	bool run_all_cases = (cmd == TMEM_RUN_ALL_SUITES);

	reset_all_cases_status();

	/* clang-format off */
	list_for_each_entry_safe(t_case, tmp, &g_test_case_list.list, list) {
		/* clang-format on */
		if (run_all_cases) {
			pr_info("[UT_SUITE]Running UT Suites: %s\n",
				t_case->item.test_description);
			case_ret = run_one_test_case(
				t_case, t_case->item.cmd, t_case->item.param1,
				t_case->item.param2, t_case->item.param3);
			if ((case_ret == UT_STATE_FAIL)
			    && KERN_UT_RUN_ALL_STOP_ONCE_FAILED) {
				dump_run_cases_results(cmd, run_all_cases);
				return TMEM_GENERAL_ERROR;
			}
		} else if (t_case->item.cmd == cmd) {
			pr_info("[UT_SUITE]Running UT Suites: %s\n",
				t_case->item.test_description);
			case_ret = run_one_test_case(
				t_case, t_case->item.cmd, t_case->item.param1,
				t_case->item.param2, t_case->item.param3);
			break;
		}
	}

	if (case_ret == UT_STATE_UNSUPPORTED_CMD) {
		pr_info("[UT_SUITE]No UT Suite is Founded!\n");
		pr_info("[UT_SUITE]UT case:%lld is unsupported!\n", cmd);
		return TMEM_OPERATION_NOT_IMPLEMENTED;
	}

	dump_run_cases_results(cmd, run_all_cases);
	return TMEM_OK;
}

void register_ut_test_case(char *test_description, u64 cmd, u64 param1,
			   u64 param2, u64 param3,
			   int (*tc)(struct ut_params *params, char *test_desc))
{
	struct UT_TEST_CASE *t_case;

	t_case = kmalloc(sizeof(struct UT_TEST_CASE), GFP_KERNEL);
	if (INVALID(t_case)) {
		pr_err("malloc test case '%s' failed!\n", test_description);
		return;
	}
	memset(t_case, 0x0, sizeof(struct UT_TEST_CASE));

	t_case->item.cmd = cmd;
	t_case->item.param1 = param1;
	t_case->item.param2 = param2;
	t_case->item.param3 = param3;
	t_case->item.test_case_cb = tc;
	t_case->item.test_description = test_description;
	list_add_tail(&(t_case->list), &g_test_case_list.list);
}

int invoke_ut_cases(u64 cmd, u64 param1, u64 param2, u64 param3)
{
	UNUSED(param1);
	UNUSED(param2);
	UNUSED(param3);

	pr_info("%s:%d cmd=%lld\n", __func__, __LINE__, cmd);
	return invoke_ut_test_case(cmd);
}

static int __init tmem_ut_server_init(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);

	INIT_LIST_HEAD(&g_test_case_list.list);
	return TMEM_OK;
}

static void __exit tmem_ut_server_exit(void)
{
	struct UT_TEST_CASE *t_case, *tmp;

	/* clang-format off */
	list_for_each_entry_safe(t_case, tmp, &g_test_case_list.list, list) {
		/* clang-format on */
		list_del(&t_case->list);
		kfree(t_case);
	}
}

subsys_initcall(tmem_ut_server_init);
module_exit(tmem_ut_server_exit);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Trusted Memory Test Server");
