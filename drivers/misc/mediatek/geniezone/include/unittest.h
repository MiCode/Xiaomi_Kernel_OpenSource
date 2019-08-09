/*
 * Copyright (C) 2015 MediaTek Inc.
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

#define UT_LOG(fmt...) pr_debug("[GZTEST]" fmt)

#define INIT_UNITTESTS                                                         \
	static uint _uts_total;                                                \
	static uint _uts_failed

#define RESET_UNITTESTS                                                        \
	do {                                                                   \
		_uts_total = 0;                                                \
		_uts_failed = 0;                                               \
	} while (0)

#define REPORT_UNITTESTS                                                       \
	{                                                                      \
		UT_LOG("======== UT report =======\n");                        \
		UT_LOG("total test cases: %u\n", _uts_total);                  \
		UT_LOG("FAILED test cases: %u\n", _uts_failed);                \
		if (_uts_failed > 0) {                                         \
			UT_LOG("===========> UT FAILED\n");                    \
		} else {                                                       \
			UT_LOG("===========> UT SUCCESSED\n");                 \
		}                                                              \
	}

/*
 *   Begin and end test macro
 */
#define TEST_BEGIN(name)                                                       \
	bool _success = true;                                                  \
	char *_test = name


#define TEST_END                                                               \
	{                                                                      \
		if (_success)                                                  \
			UT_LOG("%s: PASSED\n", _test);                         \
		else                                                           \
			UT_LOG("%s: FAILED\n", _test);                         \
	}

#define TEST_END_NO_PRT							\
{									\
	if (_success)                                                   \
		_success = true;					\
	else                                                            \
		_success = false;					\
	strcpy(_test, _test);						\
}
/*
 * CHECK_* macros to check test results.
 */
#define CHECK_EQ(expected, actual, msg)                                        \
	{                                                                      \
		typeof(actual) _e = expected;                                  \
		typeof(actual) _a = actual;                                    \
		_uts_total++;                                                  \
		if (_e != _a) {                                                \
			UT_LOG("%s: expected " #expected                       \
			       " (%d), "                                       \
			       "actual " #actual " (%d)\n",                    \
			       msg, (int)_e, (int)_a);                         \
			_uts_failed++;                                         \
			_success = false;                                      \
		}                                                              \
	}

#define CHECK_NEQ(expected, actual, msg)                                       \
	{                                                                      \
		typeof(actual) _e = expected;                                  \
		typeof(actual) _a = actual;                                    \
		_uts_total++;                                                  \
		if (_e == _a) {                                                \
			UT_LOG("%s: NOT expected " #expected                   \
			       " (%d), "                                       \
			       "actual " #actual " (%d)\n",                    \
			       msg, (int)_e, (int)_a);                         \
			_uts_failed++;                                         \
			_success = false;                                      \
		}                                                              \
	}

#define CHECK_GT(expected, actual, msg)                                        \
	{                                                                      \
		typeof(actual) _e = expected;                                  \
		typeof(actual) _a = actual;                                    \
		_uts_total++;                                                  \
		if (_e <= _a) {                                                \
			UT_LOG("%s: expected " #expected                       \
			       " (%d), "                                       \
			       "actual " #actual " (%d)\n",                    \
			       msg, (int)_e, (int)_a);                         \
			_uts_failed++;                                         \
			_success = false;                                      \
		}                                                              \
	}

#define CHECK_GE_ZERO(actual, msg)                                             \
	{                                                                      \
		typeof(actual) _a = actual;                                    \
		_uts_total++;                                                  \
		if (_a < 0) {                                                  \
			UT_LOG("%s: expected >= 0, "                           \
			       "actual " #actual " (%d)\n",                    \
			       msg, (int)_a);                                  \
			_uts_failed++;                                         \
			_success = false;                                      \
		}                                                              \
	}


#define CHECK_GT_ZERO(actual, msg)                                             \
	{                                                                      \
		typeof(actual) _a = actual;                                    \
		_uts_total++;                                                  \
		if (_a <= 0) {                                                 \
			UT_LOG("%s: expected > 0, "                            \
			       "actual " #actual " (%d)\n",                    \
			       msg, (int)_a);                                  \
			_uts_failed++;                                         \
			_success = false;                                      \
		}                                                              \
	}
