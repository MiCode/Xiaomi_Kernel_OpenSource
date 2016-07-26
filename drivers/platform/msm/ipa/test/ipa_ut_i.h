/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _IPA_UT_I_H_
#define _IPA_UT_I_H_

/* Suite data global structure  name */
#define _IPA_UT_SUITE_DATA(__name) ipa_ut_ ##__name ##_data

/* Suite meta-data global structure name */
#define _IPA_UT_SUITE_META_DATA(__name) ipa_ut_ ##__name ##_meta_data

/* Suite global array of tests */
#define _IPA_UT_SUITE_TESTS(__name) ipa_ut_ ##__name ##_tests

/* Global array of all suites */
#define _IPA_UT_ALL_SUITES ipa_ut_all_suites_data

/* Meta-test "all" name - test to run all tests in given suite */
#define _IPA_UT_RUN_ALL_TEST_NAME "all"

/**
 * Meta-test "regression" name -
 * test to run all regression tests in given suite
 */
#define _IPA_UT_RUN_REGRESSION_TEST_NAME "regression"


/* Test Log buffer name and size */
#define _IPA_UT_TEST_LOG_BUF_NAME ipa_ut_tst_log_buf
#define _IPA_UT_TEST_LOG_BUF_SIZE 8192

/* Global structure  for test fail execution result information */
#define _IPA_UT_TEST_FAIL_REPORT_DATA ipa_ut_tst_fail_report_data
#define _IPA_UT_TEST_FAIL_REPORT_SIZE 5
#define _IPA_UT_TEST_FAIL_REPORT_IDX ipa_ut_tst_fail_report_data_index

/* Start/End definitions of the array of suites */
#define IPA_UT_DEFINE_ALL_SUITES_START \
	static struct ipa_ut_suite *_IPA_UT_ALL_SUITES[] =
#define IPA_UT_DEFINE_ALL_SUITES_END

/**
 * Suites iterator - Array-like container
 * First index, number of elements  and element fetcher
 */
#define IPA_UT_SUITE_FIRST_INDEX 0
#define IPA_UT_SUITES_COUNT \
	ARRAY_SIZE(_IPA_UT_ALL_SUITES)
#define IPA_UT_GET_SUITE(__index) \
	_IPA_UT_ALL_SUITES[__index]

/**
 * enum ipa_ut_test_result - Test execution result
 * @IPA_UT_TEST_RES_FAIL: Test executed and failed
 * @IPA_UT_TEST_RES_SUCCESS: Test executed and succeeded
 * @IPA_UT_TEST_RES_SKIP: Test was not executed.
 *
 * When running all tests in a suite, a specific test could
 * be skipped and not executed. For example due to mismatch of
 * IPA H/W version.
 */
enum ipa_ut_test_result {
	IPA_UT_TEST_RES_FAIL,
	IPA_UT_TEST_RES_SUCCESS,
	IPA_UT_TEST_RES_SKIP,
};

/**
 * enum ipa_ut_meta_test_type - Type of suite meta-test
 * @IPA_UT_META_TEST_ALL: Represents all tests in suite
 * @IPA_UT_META_TEST_REGRESSION: Represents all regression tests in suite
 */
enum ipa_ut_meta_test_type {
	IPA_UT_META_TEST_ALL,
	IPA_UT_META_TEST_REGRESSION,
};

#endif /* _IPA_UT_I_H_ */
