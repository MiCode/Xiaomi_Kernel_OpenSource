/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _IPA_UT_FRAMEWORK_H_
#define _IPA_UT_FRAMEWORK_H_

#include <linux/kernel.h>
#include "ipa_common_i.h"
#include "ipa_ut_i.h"

#define IPA_UT_DRV_NAME "ipa_ut"

#define IPA_UT_DBG(fmt, args...) \
	do { \
		pr_debug(IPA_UT_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf(), \
			IPA_UT_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf_low(), \
			IPA_UT_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_UT_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(IPA_UT_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf_low(), \
			IPA_UT_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_UT_ERR(fmt, args...) \
	do { \
		pr_err(IPA_UT_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf(), \
			IPA_UT_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf_low(), \
			IPA_UT_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_UT_INFO(fmt, args...) \
	do { \
		pr_info(IPA_UT_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf(), \
			IPA_UT_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa3_get_ipc_logbuf_low(), \
			IPA_UT_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

/**
 * struct ipa_ut_tst_fail_report - Information on test failure
 * @valid: When a test posts a report, valid will be marked true
 * @file: File name containing  the failed test.
 * @line: Number of line in the file where the test failed.
 * @func: Function where the test failed in.
 * @info: Information about the failure.
 */
struct ipa_ut_tst_fail_report {
	bool valid;
	const char *file;
	int line;
	const char *func;
	const char *info;
};

/**
 * Report on test failure
 * To be used by tests to report a point were a test fail.
 * Failures are saved in a stack manner.
 * Dumping the failure info will dump the fail reports
 *  from all the function in the calling stack
 */
#define IPA_UT_TEST_FAIL_REPORT(__info) \
	do { \
		extern struct ipa_ut_tst_fail_report \
			_IPA_UT_TEST_FAIL_REPORT_DATA \
			[_IPA_UT_TEST_FAIL_REPORT_SIZE]; \
		extern u32 _IPA_UT_TEST_FAIL_REPORT_IDX; \
		struct ipa_ut_tst_fail_report *entry; \
		if (_IPA_UT_TEST_FAIL_REPORT_IDX >= \
			_IPA_UT_TEST_FAIL_REPORT_SIZE) \
			break; \
		entry = &(_IPA_UT_TEST_FAIL_REPORT_DATA \
			[_IPA_UT_TEST_FAIL_REPORT_IDX]); \
		entry->file = __FILENAME__; \
		entry->line = __LINE__; \
		entry->func = __func__; \
		if (__info) \
			entry->info = __info; \
		else \
			entry->info = ""; \
		_IPA_UT_TEST_FAIL_REPORT_IDX++; \
	} while (0)

/**
 * To be used by tests to log progress and ongoing information
 * Logs are not printed to user, but saved to a buffer.
 * I/S shall print the buffer at different occasions - e.g. in test failure
 */
#define IPA_UT_LOG(fmt, args...) \
	do { \
		extern char *_IPA_UT_TEST_LOG_BUF_NAME; \
		char __buf[512]; \
		IPA_UT_DBG(fmt, ## args); \
		if (!_IPA_UT_TEST_LOG_BUF_NAME) {\
			pr_err(IPA_UT_DRV_NAME " %s:%d " fmt, \
				__func__, __LINE__, ## args); \
			break; \
		} \
		scnprintf(__buf, sizeof(__buf), \
			" %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		strlcat(_IPA_UT_TEST_LOG_BUF_NAME, __buf, \
			_IPA_UT_TEST_LOG_BUF_SIZE); \
	} while (0)

/**
 * struct ipa_ut_suite_meta - Suite meta-data
 * @name: Suite unique name
 * @desc: Suite description
 * @setup: Setup Call-back of the suite
 * @teardown: Teardown Call-back of the suite
 * @priv: Private pointer of the suite
 *
 * Setup/Teardown  will be called once for the suite when running a tests of it.
 * priv field is shared between the Setup/Teardown and the tests
 */
struct ipa_ut_suite_meta {
	char *name;
	char *desc;
	int (*setup)(void **ppriv);
	int (*teardown)(void *priv);
	void *priv;
};

/* Test suite data structure declaration */
struct ipa_ut_suite;

/**
 * struct ipa_ut_test - Test information
 * @name: Test name
 * @desc: Test description
 * @run: Test execution call-back
 * @run_in_regression: To run this test as part of regression?
 * @min_ipa_hw_ver: Minimum IPA H/W version where the test is supported?
 * @max_ipa_hw_ver: Maximum IPA H/W version where the test is supported?
 * @suite: Pointer to suite containing this test
 * @res: Test execution result. Will be updated after running a test as part
 * of suite tests run
 */
struct ipa_ut_test {
	char *name;
	char *desc;
	int (*run)(void *priv);
	bool run_in_regression;
	int min_ipa_hw_ver;
	int max_ipa_hw_ver;
	struct ipa_ut_suite *suite;
	enum ipa_ut_test_result res;
};

/**
 * struct ipa_ut_suite - Suite information
 * @meta_data: Pointer to meta-data structure of the suite
 * @tests: Pointer to array of tests belongs to the suite
 * @tests_cnt: Number of tests
 */
struct ipa_ut_suite {
	struct ipa_ut_suite_meta *meta_data;
	struct ipa_ut_test *tests;
	size_t tests_cnt;
};


/**
 * Add a test to a suite.
 * Will add entry to tests array and update its info with
 * the given info, thus adding new test.
 */
#define IPA_UT_ADD_TEST(__name, __desc, __run, __run_in_regression, \
	__min_ipa_hw_ver, __max_ipa__hw_ver) \
	{ \
		.name = #__name, \
		.desc = __desc, \
		.run = __run, \
		.run_in_regression = __run_in_regression, \
		.min_ipa_hw_ver = __min_ipa_hw_ver, \
		.max_ipa_hw_ver = __max_ipa__hw_ver, \
		.suite = NULL, \
	}

/**
 * Declare a suite
 * Every suite need to be declared  before it is registered.
 */
#define IPA_UT_DECLARE_SUITE(__name) \
	extern struct ipa_ut_suite _IPA_UT_SUITE_DATA(__name)

/**
 * Register a suite
 * Registering a suite is mandatory so it will be considered.
 */
#define IPA_UT_REGISTER_SUITE(__name) \
	(&_IPA_UT_SUITE_DATA(__name))

/**
 * Start/End suite definition
 * Will create the suite global structures and adds adding tests to it.
 * Use IPA_UT_ADD_TEST() with these macros to add tests when defining
 * a suite
 */
#define IPA_UT_DEFINE_SUITE_START(__name, __desc, __setup, __teardown) \
	static struct ipa_ut_suite_meta _IPA_UT_SUITE_META_DATA(__name) = \
	{ \
		.name = #__name, \
		.desc = __desc, \
		.setup = __setup, \
		.teardown = __teardown, \
	}; \
	static struct ipa_ut_test _IPA_UT_SUITE_TESTS(__name)[] =
#define IPA_UT_DEFINE_SUITE_END(__name) \
	; \
	struct ipa_ut_suite _IPA_UT_SUITE_DATA(__name) = \
	{ \
		.meta_data = &_IPA_UT_SUITE_META_DATA(__name), \
		.tests = _IPA_UT_SUITE_TESTS(__name), \
		.tests_cnt = ARRAY_SIZE(_IPA_UT_SUITE_TESTS(__name)), \
	}

#endif /* _IPA_UT_FRAMEWORK_H_ */
