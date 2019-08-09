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

#ifndef KERN_UT_TESTS_H
#define KERN_UT_TESTS_H

#ifndef LOG_TAG
#define LOG_TAG "[KERN_UT]"
#endif

#ifndef pr_fmt
#define pr_fmt(fmt) LOG_TAG fmt
#endif

#define KERN_UT_RESERVED_COMMAND_RUN_ALL_SUITES (16888)
#define KERN_UT_RUN_ALL_STOP_ONCE_FAILED (1)

enum UT_RET_STATE {
	UT_STATE_PASS = 0,
	UT_STATE_FAIL = -1,
	UT_STATE_UNSUPPORTED_CMD = -2,
};

struct ut_params {
	u64 cmd;
	s64 param1;
	s64 param2;
	s64 param3;
};

struct ut_test_suite_statistics {
	enum UT_RET_STATE ret;
	u32 pass_cnt;
	u32 fail_cnt;
	u32 spend_sec;
	u32 spend_msec;
};

struct ut_test_suite {
	u64 cmd_start;
	u64 cmd_end;
	struct ut_test_suite_statistics *cmds_last_run_statistics;
	char *test_suite_name;
	int (*test_suite_cb)(u64 cmd, u64 param1, u64 param2, u64 param3,
			     struct ut_test_suite_statistics *statistics);
};

struct UT_TEST_SUITE {
	struct ut_test_suite item;
	struct list_head list;
};

void register_ut_test_suite(
	char *suite_name, u64 cmd_start, u64 cmd_end,
	int (*tc)(u64 cmd, u64 param1, u64 param2, u64 param3,
		  struct ut_test_suite_statistics *statistics));
void invoke_ut_test_suite(u64 cmd, u64 param1, u64 param2, u64 param3);

/* clang-format off */

#define GET_TIME_DIFF_SEC(start, end) \
	(int)((end.tv_usec > start.tv_usec) ? (end.tv_sec - start.tv_sec) \
					    : (end.tv_sec - start.tv_sec - 1))

#define GET_TIME_DIFF_USEC(start, end) \
	(int)((end.tv_usec > start.tv_usec) \
		      ? (end.tv_usec - start.tv_usec) \
		      : (1000000 + end.tv_usec - start.tv_usec))

#define DEFINE_UT_SERVER(server_main) \
	static struct UT_TEST_SUITE g_test_suite_list; \
	static void ut_test_framework_init(void) \
	{ \
		INIT_LIST_HEAD(&g_test_suite_list.list); \
	} \
	static void reset_all_suites_status(void) \
	{ \
		struct UT_TEST_SUITE *suite, *tmp; \
		u64 cmd_idx; \
		u32 cmd_off; \
		struct ut_test_suite_statistics *t_statistics; \
		list_for_each_entry_safe(suite, tmp, &g_test_suite_list.list, \
									list) { \
			for (cmd_idx = suite->item.cmd_start; \
				cmd_idx <= suite->item.cmd_end; cmd_idx++) { \
				cmd_off = cmd_idx - suite->item.cmd_start; \
				t_statistics = &suite->item.cmds_last_run_statistics[cmd_off]; \
				t_statistics->ret = UT_STATE_UNSUPPORTED_CMD; \
				t_statistics->pass_cnt = 0; \
				t_statistics->fail_cnt = 0; \
				t_statistics->spend_sec = 0; \
				t_statistics->spend_msec = 0; \
			} \
		} \
	} \
	static int run_one_test_case(struct UT_TEST_SUITE *suite, u64 cmd, \
			u64 param1, u64 param2, u64 param3) \
	{ \
		enum UT_RET_STATE case_ret; \
		u32 cmd_off = cmd - suite->item.cmd_start; \
		case_ret = suite->item.test_suite_cb(cmd, param1, param2, param3, \
			&suite->item.cmds_last_run_statistics[cmd_off]); \
		if (case_ret == UT_STATE_PASS) \
			pr_debug("  UT case:%lld passed!\n", cmd); \
		else if (case_ret == UT_STATE_FAIL) \
			pr_debug("  UT case:%lld failed!\n", cmd); \
		else \
			pr_debug("  UT case:%lld unsupported!\n", cmd); \
		return case_ret; \
	} \
	static void dump_run_suites_results(u64 cmd, bool run_all_suites) \
	{ \
		struct UT_TEST_SUITE *suite, *tmp; \
		u64 cmd_idx; \
		u32 cmd_off; \
		u32 all_pass_cnt = 0; \
		u32 all_fail_cnt = 0; \
		u32 all_spend_sec = 0; \
		u32 all_spend_msec = 0; \
		struct ut_test_suite_statistics *t_statistics; \
		pr_info("[UT_SUITE]Test Results: RUN %s\n", (run_all_suites?"ALL":"ONE")); \
		pr_info("[UT_SUITE] CASE     PASS       FAIL     TIME\n"); \
		list_for_each_entry_safe(suite, tmp, &g_test_suite_list.list, \
									list) { \
			for (cmd_idx = suite->item.cmd_start; \
				cmd_idx <= suite->item.cmd_end; cmd_idx++) { \
				cmd_off = cmd_idx - suite->item.cmd_start; \
				t_statistics = &suite->item.cmds_last_run_statistics[cmd_off]; \
				if (t_statistics->ret != UT_STATE_UNSUPPORTED_CMD) { \
					if (cmd == cmd_idx || run_all_suites) \
						pr_info("[UT_SUITE]%5lld     %-9d  %-7d  %05d.%06d sec\n", \
							cmd_idx, t_statistics->pass_cnt, \
							t_statistics->fail_cnt, t_statistics->spend_sec, \
							t_statistics->spend_msec); \
						all_pass_cnt += t_statistics->pass_cnt; \
						all_fail_cnt += t_statistics->fail_cnt; \
						all_spend_sec += t_statistics->spend_sec; \
						all_spend_msec += t_statistics->spend_msec; \
				} \
			} \
		} \
		if (run_all_suites) { \
			all_spend_sec += (all_spend_msec / 1000000); \
			all_spend_msec = (all_spend_msec % 1000000); \
			pr_info("[UT_SUITE] Summary:\n"); \
			pr_info("[UT_SUITE]  ALL     %-9d  %-7d  %05d.%06d sec\n", \
				all_pass_cnt, all_fail_cnt, all_spend_sec, all_spend_msec); \
		} \
	} \
	void invoke_ut_test_suite(u64 cmd, u64 param1, u64 param2, u64 param3) \
	{ \
		struct UT_TEST_SUITE *suite, *tmp; \
		enum UT_RET_STATE case_ret = UT_STATE_UNSUPPORTED_CMD; \
		bool run_all_suites = (cmd == KERN_UT_RESERVED_COMMAND_RUN_ALL_SUITES); \
		u64 cmd_idx; \
		reset_all_suites_status(); \
		list_for_each_entry_safe(suite, tmp, &g_test_suite_list.list, \
					 list) { \
			if (run_all_suites) { \
				pr_info("[UT_SUITE]Running UT Suites: %s\n", \
							suite->item.test_suite_name); \
				for (cmd_idx = suite->item.cmd_start; \
					cmd_idx <= suite->item.cmd_end; cmd_idx++) { \
					case_ret = run_one_test_case(suite, cmd_idx, param1, \
							param2, param3); \
					if ((case_ret == UT_STATE_FAIL) && \
						KERN_UT_RUN_ALL_STOP_ONCE_FAILED) { \
						dump_run_suites_results(cmd, run_all_suites); \
						return; \
					} \
				} \
			} else if ((suite->item.cmd_start <= cmd) \
							&& (cmd <= suite->item.cmd_end)) { \
				pr_info("[UT_SUITE]Running UT Suites: %s\n", \
							suite->item.test_suite_name); \
				case_ret = run_one_test_case(suite, cmd, param1, \
						param2, param3); \
				break; \
			} \
		} \
		if (case_ret == UT_STATE_UNSUPPORTED_CMD) { \
			pr_info("[UT_SUITE]No UT Suite is Founded!\n"); \
			pr_info("[UT_SUITE]UT case:%lld is unsupported!\n", cmd); \
		} else { \
			dump_run_suites_results(cmd, run_all_suites); \
		} \
	} \
	void register_ut_test_suite( \
		char *suite_name, u64 cmd_start, u64 cmd_end, \
		int (*tc)(u64 cmd, u64 param1, u64 param2, u64 param3, \
					struct ut_test_suite_statistics *statistics)) \
	{ \
		struct UT_TEST_SUITE *t_suite; \
		struct ut_test_suite_statistics *t_statistics; \
		u32 cmd_cnt = (u32)(cmd_end - cmd_start + 1); \
		t_suite = kmalloc(sizeof(struct UT_TEST_SUITE), GFP_KERNEL); \
		if (INVALID(t_suite)) { \
			pr_err("malloc test suite '%s' failed!\n", suite_name); \
			return; \
		} \
		memset(t_suite, 0x0, sizeof(struct UT_TEST_SUITE)); \
		t_statistics = \
			kmalloc(sizeof(struct ut_test_suite_statistics)*cmd_cnt, GFP_KERNEL); \
		if (INVALID(t_statistics)) { \
			pr_err("malloc test statistics '%s' failed!\n", suite_name); \
			kfree(t_suite); \
			return; \
		} \
		memset(t_statistics, 0x0, \
			sizeof(struct ut_test_suite_statistics)*cmd_cnt); \
		t_suite->item.cmds_last_run_statistics = t_statistics; \
		t_suite->item.cmd_start = cmd_start; \
		t_suite->item.cmd_end = cmd_end; \
		t_suite->item.test_suite_cb = tc; \
		t_suite->item.test_suite_name = suite_name; \
		list_add_tail(&(t_suite->list), &g_test_suite_list.list); \
	} \
	static int __init server_main##_ut_agent(void) \
	{ \
		pr_info("%s:%d\n", __func__, __LINE__); \
		ut_test_framework_init(); \
		pr_info("%s:%d (end)\n", __func__, __LINE__); \
		return 0; \
	} \
	subsys_initcall(server_main##_ut_agent);

#define DEFINE_UT_SUPPORT(suite) \
	static u32 ut_##suite##_pass_count; \
	static u32 ut_##suite##_fail_count; \
	static bool ut_##suite##_halt; \
	static u64 ut_##suite##_command; \
	static struct timeval ut_##suite##_start_time; \
	static struct timeval ut_##suite##_end_time; \
	static void ut_set_halt(void) \
	{ \
		ut_##suite##_halt = true; \
	} \
	static bool ut_is_halt(void) \
	{ \
		return ut_##suite##_halt; \
	} \
	static void ut_increase_pass_cnt(void) \
	{ \
		ut_##suite##_pass_count++; \
	} \
	static void ut_increase_fail_cnt(void) \
	{ \
		ut_##suite##_fail_count++; \
	} \
	static u32 ut_get_pass_cnt(void) \
	{ \
		return ut_##suite##_pass_count; \
	} \
	static u32 ut_get_fail_cnt(void) \
	{ \
		return ut_##suite##_fail_count; \
	} \
	static u32 ut_get_spend_sec(void) \
	{ \
		return GET_TIME_DIFF_SEC(ut_##suite##_start_time, \
			ut_##suite##_end_time); \
	} \
	static u32 ut_get_spend_msec(void) \
	{ \
		return GET_TIME_DIFF_USEC(ut_##suite##_start_time, \
			ut_##suite##_end_time); \
	} \
	static void ut_status_reset(u64 ut_cmd) \
	{ \
		ut_##suite##_pass_count = 0; \
		ut_##suite##_fail_count = 0; \
		ut_##suite##_halt = false; \
		ut_##suite##_command = ut_cmd; \
		do_gettimeofday(&ut_##suite##_start_time); \
	} \
	static void ut_status_dump(void) \
	{ \
		do_gettimeofday(&ut_##suite##_end_time); \
		pr_info("[UT_CASE]================================================\n"); \
		pr_info("[UT_CASE]Executing UT command: %lld\n", ut_##suite##_command); \
		pr_info("[UT_CASE]  TOTAL TEST ITEMS: %d\n", \
			ut_##suite##_pass_count + ut_##suite##_fail_count); \
		pr_info("[UT_CASE]  PASS TEST ITEMS: %d\n", ut_##suite##_pass_count); \
		pr_info("[UT_CASE]  FAIL TEST ITEMS: %d\n", ut_##suite##_fail_count); \
		if (ut_##suite##_fail_count == 0) \
			pr_info("[UT_CASE]all UT test items are passed!!!\n"); \
		else \
			pr_info("[UT_CASE]some items are failed, please check!!!\n"); \
		pr_info("[UT_CASE]  Spend time: %d.%d seconds\n", \
			GET_TIME_DIFF_SEC(ut_##suite##_start_time, \
				ut_##suite##_end_time), \
			GET_TIME_DIFF_USEC(ut_##suite##_start_time, \
				ut_##suite##_end_time)); \
		pr_info("[UT_CASE]================================================\n"); \
	}

#define UT_HALT_CHECK() \
	do { \
		if (ut_is_halt()) \
			return UT_STATE_FAIL; \
	} while (0)

#define BEGIN_UT_TEST \
	do { \
		pr_info("%s:%d\n", __func__, __LINE__); \
		UT_HALT_CHECK(); \
	} while (0)

#define END_UT_TEST \
	do { \
		return UT_STATE_PASS; \
	} while (0)

#define UT_OP(val, got, op, msg, halt) \
	do { \
		typeof(val) ret_val = val; \
		typeof(got) ret_got = got; \
		if (ret_val op ret_got) { \
			pr_err("'%s' FAILED!\n", msg); \
			pr_err( \
				"[ERR]%s:%d unexpected: 0x%x, expected: 0x%x\n", \
				__func__, __LINE__, ret_got, ret_val); \
			ut_increase_fail_cnt(); \
			if (halt) { \
				ut_set_halt(); \
				return UT_STATE_FAIL; \
			} \
		} else { \
			ut_increase_pass_cnt(); \
			pr_debug("'%s' PASSED!\n", msg); \
		} \
	} while (0)

#define UT_OP_64(val, got, op, msg, halt) \
	do { \
		typeof(val) ret_val = val; \
		typeof(got) ret_got = got; \
		if (ret_val op ret_got) { \
			pr_err("'%s' FAILED!\n", msg); \
			pr_err( \
				"[ERR]%s:%d unexpected: 0x%llx, expected: 0x%llx\n", \
				__func__, __LINE__, ret_got, ret_val); \
			ut_increase_fail_cnt(); \
			if (halt) { \
				ut_set_halt(); \
				return UT_STATE_FAIL; \
			} \
		} else { \
			ut_increase_pass_cnt(); \
			pr_debug("'%s' PASSED!\n", msg); \
		} \
	} while (0)

#define UT_OP_NULL(got, msg, halt) \
	do { \
		typeof(got) ret_got = got; \
		if (ret_got == NULL) { \
			pr_err("'%s' FAILED!\n", msg); \
			pr_err( \
				"[ERR]%s:%d unexpected: %p, expected not null\n", \
				__func__, __LINE__, ret_got); \
			ut_increase_fail_cnt(); \
			if (halt) { \
				ut_set_halt(); \
				return UT_STATE_FAIL; \
			} \
		} else { \
			ut_increase_pass_cnt(); \
			pr_debug("'%s' PASSED!\n", msg); \
		} \
	} while (0)

#define EXPECT_EQ(val, got, msg)   UT_OP(val, got, !=, msg, 0)
#define EXPECT_NE(val, got, msg)   UT_OP(val, got, ==, msg, 0)
#define EXPECT_LE(val, got, msg)   UT_OP(val, got, >, msg, 0)
#define EXPECT_GE(val, got, msg)   UT_OP(val, got, <, msg, 0)
#define ASSERT_EQ(val, got, msg)   UT_OP(val, got, !=, msg, 1)
#define ASSERT_NE(val, got, msg)   UT_OP(val, got, ==, msg, 1)
#define ASSERT_LE(val, got, msg)   UT_OP(val, got, >, msg, 1)
#define ASSERT_GE(val, got, msg)   UT_OP(val, got, <, msg, 1)

#define EXPECT_TRUE(got, msg)      UT_OP(1, got, !=, msg, 0)
#define EXPECT_FALSE(got, msg)     UT_OP(1, got, ==, msg, 0)
#define ASSERT_TRUE(got, msg)      UT_OP(1, got, !=, msg, 1)
#define ASSERT_FALSE(got, msg)     UT_OP(1, got, ==, msg, 1)

#define EXPECT_NOTNULL(got, msg)   UT_OP_NULL(got, msg, 0)
#define ASSERT_NOTNULL(got, msg)   UT_OP_NULL(got, msg, 1)

#define EXPECT_EQ64(val, got, msg) UT_OP_64(val, got, !=, msg, 0)
#define EXPECT_NE64(val, got, msg) UT_OP_64(val, got, ==, msg, 0)
#define ASSERT_EQ64(val, got, msg) UT_OP_64(val, got, !=, msg, 1)
#define ASSERT_NE64(val, got, msg) UT_OP_64(val, got, ==, msg, 1)

#define BEGIN_TEST_SUITE(cmd_start, cmd_end, suite_main, init_func) \
	int suite_main(u64 cmd, u64 param1, u64 param2, u64 param3, \
					struct ut_test_suite_statistics *statistics) \
	{ \
		enum UT_RET_STATE (*suite_init)(struct ut_params *params); \
		enum UT_RET_STATE (*suite_deinit)(struct ut_params *params); \
		enum UT_RET_STATE func_ret; \
		struct ut_params params; \
		params.cmd = cmd; \
		params.param1 = param1; \
		params.param2 = param2; \
		params.param3 = param3; \
		suite_init = init_func; \
		if ((cmd < cmd_start) || (cmd > cmd_end)) { \
			statistics->ret = UT_STATE_UNSUPPORTED_CMD; \
			return UT_STATE_UNSUPPORTED_CMD; \
		} \
		ut_status_reset(cmd); \
		if (suite_init != NULL) \
			suite_init(&params); \
		switch (cmd) {

#define DEFINE_TEST_CASE(cmd, func) \
		case cmd: \
			pr_info("[UT_SUITE]        UT case:%d .....\n", cmd); \
			func_ret = func(&params); \
			break;

#define DEFINE_TEST_CASE_PARAM1(cmd, func, p1) \
		case cmd: \
			params.param1 = p1; \
			pr_info("[UT_SUITE]        UT case:%d .....\n", cmd); \
			func_ret = func(&params); \
			break;

#define DEFINE_TEST_CASE_PARAM2(cmd, func, p1, p2) \
		case cmd: \
			params.param1 = p1; \
			params.param2 = p2; \
			pr_info("[UT_SUITE]        UT case:%d .....\n", cmd); \
			func_ret = func(&params); \
			break;

#define DEFINE_TEST_CASE_PARAM3(cmd, func, p1, p2, p3) \
		case cmd: \
			params.param1 = p1; \
			params.param2 = p2; \
			params.param3 = p3; \
			pr_info("[UT_SUITE]        UT case:%d .....\n", cmd); \
			func_ret = func(&params); \
			break;

#define END_TEST_SUITE(deinit_func) \
		default: \
			statistics->ret = UT_STATE_UNSUPPORTED_CMD; \
			func_ret = UT_STATE_UNSUPPORTED_CMD; \
			break; \
		} \
		suite_deinit = deinit_func; \
		if (suite_deinit != NULL) \
			suite_deinit(&params); \
		ut_status_dump(); \
		statistics->pass_cnt = ut_get_pass_cnt(); \
		statistics->fail_cnt = ut_get_fail_cnt(); \
		statistics->spend_sec = ut_get_spend_sec(); \
		statistics->spend_msec = ut_get_spend_msec(); \
		statistics->ret = func_ret; \
		return func_ret; \
	}

#define REGISTER_TEST_SUITE(cmd_start, cmd_end, suite_main) \
	static int __init suite_main##_register(void) \
	{ \
		pr_info("%s:%d\n", __func__, __LINE__); \
		register_ut_test_suite(#suite_main, cmd_start, cmd_end, \
				       suite_main); \
		pr_info("%s:%d (end)\n", __func__, __LINE__); \
		return 0; \
	} \
	late_initcall(suite_main##_register);

/* clang-format on */

#endif /* end of KERN_UT_TESTS_H */
