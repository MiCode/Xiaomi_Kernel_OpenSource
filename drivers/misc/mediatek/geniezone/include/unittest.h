/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
 */

#define UT_LOG(fmt...) pr_info("[GZTEST]" fmt)

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
			UT_LOG("===========> UT SUCCEEDED\n");                 \
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
