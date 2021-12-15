/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef TMEM_UT_API_H
#define TMEM_UT_API_H

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

/* clang-format off */

/* Using glue macro to generate a 'return' to bypass checkpatch's check.
 * ERROR: Macros with flow control statements should be avoided.
 */
#define UT_GLUE_RET(a, b) a##b

#define UT_OP(val, got, op, msg, halt) \
	do { \
		typeof(val) ret_val = val; \
		typeof(got) ret_got = got; \
		if (ret_val op ret_got) { \
			pr_err("'%s' FAILED!\n", msg); \
			pr_err( \
				"[ERR]%s:%d unexpected: 0x%x, " \
				"expected: 0x%x\n", \
				__func__, __LINE__, ret_got, ret_val); \
			ut_increase_fail_cnt(); \
			if (halt) { \
				ut_set_halt(); \
				UT_GLUE_RET(ret, urn) UT_STATE_FAIL; \
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
				"[ERR]%s:%d unexpected: %p, " \
				"expected not null\n", \
				__func__, __LINE__, ret_got); \
			ut_increase_fail_cnt(); \
			if (halt) { \
				ut_set_halt(); \
				UT_GLUE_RET(ret, urn) UT_STATE_FAIL; \
			} \
		} else { \
			ut_increase_pass_cnt(); \
			pr_debug("'%s' PASSED!\n", msg); \
		} \
	} while (0)

#define ASSERT_EQ(val, got, msg)   UT_OP(val, got, !=, msg, 1)
#define ASSERT_NE(val, got, msg)   UT_OP(val, got, ==, msg, 1)
#define ASSERT_LE(val, got, msg)   UT_OP(val, got, >, msg, 1)
#define ASSERT_GE(val, got, msg)   UT_OP(val, got, <, msg, 1)
#define ASSERT_TRUE(got, msg)      UT_OP(1, got, !=, msg, 1)
#define ASSERT_FALSE(got, msg)     UT_OP(1, got, ==, msg, 1)
#define ASSERT_NOTNULL(got, msg)   UT_OP_NULL(got, msg, 1)
/* clang-format on */

void ut_set_halt(void);
bool ut_is_halt(void);
void ut_increase_pass_cnt(void);
void ut_increase_fail_cnt(void);

int invoke_ut_cases(u64 cmd, u64 param1, u64 param2, u64 param3);
void register_ut_test_case(char *test_description, u64 cmd, u64 param1,
			   u64 param2, u64 param3,
			   int (*tc)(struct ut_params *params,
				     char *test_desc));

#endif /* end of TMEM_UT_API_H */
