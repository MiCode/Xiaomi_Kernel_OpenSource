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

#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/ipa.h>
#include "../ipa_v3/ipa_i.h"
#include "ipa_ut_framework.h"
#include "ipa_ut_suite_list.h"
#include "ipa_ut_i.h"


#define IPA_UT_DEBUG_WRITE_BUF_SIZE 256
#define IPA_UT_DEBUG_READ_BUF_SIZE 1024

#define IPA_UT_READ_WRITE_DBG_FILE_MODE \
	(S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP)

/**
 * struct ipa_ut_context - I/S context
 * @inited: Will wait till IPA is ready. Will create the enable file
 * @enabled: All tests and suite debugfs files are created
 * @lock: Lock for mutual exclustion
 * @ipa_dbgfs_root: IPA root debugfs folder
 * @test_dbgfs_root: UT root debugfs folder. Sub-folder of IPA root
 * @test_dbgfs_suites: Suites root debugfs folder. Sub-folder of UT root
 */
struct ipa_ut_context {
	bool inited;
	bool enabled;
	struct mutex lock;
	struct dentry *ipa_dbgfs_root;
	struct dentry *test_dbgfs_root;
	struct dentry *test_dbgfs_suites;
};

static ssize_t ipa_ut_dbgfs_enable_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t ipa_ut_dbgfs_enable_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos);
static ssize_t ipa_ut_dbgfs_test_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t ipa_ut_dbgfs_test_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos);
static int ipa_ut_dbgfs_all_test_open(struct inode *inode,
	struct file *filp);
static int ipa_ut_dbgfs_regression_test_open(struct inode *inode,
	struct file *filp);
static ssize_t ipa_ut_dbgfs_meta_test_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t ipa_ut_dbgfs_meta_test_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos);


static const struct file_operations ipa_ut_dbgfs_enable_fops = {
	.read = ipa_ut_dbgfs_enable_read,
	.write = ipa_ut_dbgfs_enable_write,
};
static const struct file_operations ipa_ut_dbgfs_test_fops = {
	.read = ipa_ut_dbgfs_test_read,
	.write = ipa_ut_dbgfs_test_write,
};
static const struct file_operations ipa_ut_dbgfs_all_test_fops = {
	.open = ipa_ut_dbgfs_all_test_open,
	.read = ipa_ut_dbgfs_meta_test_read,
	.write = ipa_ut_dbgfs_meta_test_write,
};
static const struct file_operations ipa_ut_dbgfs_regression_test_fops = {
	.open = ipa_ut_dbgfs_regression_test_open,
	.read = ipa_ut_dbgfs_meta_test_read,
	.write = ipa_ut_dbgfs_meta_test_write,
};

static struct ipa_ut_context *ipa_ut_ctx;
char *_IPA_UT_TEST_LOG_BUF_NAME;
struct ipa_ut_tst_fail_report
	_IPA_UT_TEST_FAIL_REPORT_DATA[_IPA_UT_TEST_FAIL_REPORT_SIZE];
u32 _IPA_UT_TEST_FAIL_REPORT_IDX;

/**
 * ipa_ut_print_log_buf() - Dump given buffer via kernel error mechanism
 * @buf: Buffer to print
 *
 * Tokenize the string according to new-line and then print
 *
 * Note: Assumes lock acquired
 */
static void ipa_ut_print_log_buf(char *buf)
{
	char *token;

	if (!buf) {
		IPA_UT_ERR("Input error - no buf\n");
		return;
	}

	for (token = strsep(&buf, "\n"); token; token = strsep(&buf, "\n"))
		pr_err("%s\n", token);
}

/**
 * ipa_ut_dump_fail_report_stack() - dump the report info stack via kernel err
 *
 * Note: Assumes lock acquired
 */
static void ipa_ut_dump_fail_report_stack(void)
{
	int i;

	IPA_UT_DBG("Entry\n");

	if (_IPA_UT_TEST_FAIL_REPORT_IDX == 0) {
		IPA_UT_DBG("no report info\n");
		return;
	}

	for (i = 0 ; i < _IPA_UT_TEST_FAIL_REPORT_IDX; i++) {
		if (i == 0)
			pr_err("***** FAIL INFO STACK *****:\n");
		else
			pr_err("Called From:\n");

		pr_err("\tFILE = %s\n\tFUNC = %s()\n\tLINE = %d\n",
			_IPA_UT_TEST_FAIL_REPORT_DATA[i].file,
			_IPA_UT_TEST_FAIL_REPORT_DATA[i].func,
			_IPA_UT_TEST_FAIL_REPORT_DATA[i].line);
		pr_err("\t%s\n", _IPA_UT_TEST_FAIL_REPORT_DATA[i].info);
	}
}

/**
 * ipa_ut_show_suite_exec_summary() - Show tests run summary
 * @suite: suite to print its running summary
 *
 * Print list of succeeded tests, failed tests and skipped tests
 *
 * Note: Assumes lock acquired
 */
static void ipa_ut_show_suite_exec_summary(const struct ipa_ut_suite *suite)
{
	int i;

	IPA_UT_DBG("Entry\n");

	ipa_assert_on(!suite);

	pr_info("\n\n");
	pr_info("\t  Suite '%s' summary\n", suite->meta_data->name);
	pr_info("===========================\n");
	pr_info("Successful tests\n");
	pr_info("----------------\n");
	for (i = 0 ; i < suite->tests_cnt ; i++) {
		if (suite->tests[i].res != IPA_UT_TEST_RES_SUCCESS)
			continue;
		pr_info("\t%s\n", suite->tests[i].name);
	}
	pr_info("\nFailed tests\n");
	pr_info("------------\n");
	for (i = 0 ; i < suite->tests_cnt ; i++) {
		if (suite->tests[i].res != IPA_UT_TEST_RES_FAIL)
			continue;
		pr_info("\t%s\n", suite->tests[i].name);
	}
	pr_info("\nSkipped tests\n");
	pr_info("-------------\n");
	for (i = 0 ; i < suite->tests_cnt ; i++) {
		if (suite->tests[i].res != IPA_UT_TEST_RES_SKIP)
			continue;
		pr_info("\t%s\n", suite->tests[i].name);
	}
	pr_info("\n");
}

/**
 * ipa_ut_dbgfs_meta_test_write() - Debugfs write func for a for a meta test
 * @params: write fops
 *
 * Used to run all/regression tests in a suite
 * Create log buffer that the test can use to store ongoing logs
 * IPA clocks need to be voted.
 * Run setup() once before running the tests and teardown() once after
 * If no such call-backs then ignore it; if failed then fail the suite
 * Print tests progress during running
 * Test log and fail report will be showed only if the test failed.
 * Finally show Summary of the suite tests running
 *
 * Note: If test supported IPA H/W version mismatch, skip it
 *	 If a test lack run function, skip it
 *	 If test doesn't belong to regression and it is regression run, skip it
 * Note: Running mode: Do not stop running on failure
 *
 * Return: Negative in failure, given characters amount in success
 */
static ssize_t ipa_ut_dbgfs_meta_test_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	struct ipa_ut_suite *suite;
	int i;
	enum ipa_hw_type ipa_ver;
	int rc = 0;
	long meta_type;
	bool tst_fail = false;

	IPA_UT_DBG("Entry\n");

	mutex_lock(&ipa_ut_ctx->lock);
	suite = file->f_inode->i_private;
	ipa_assert_on(!suite);
	meta_type = (long)(file->private_data);
	IPA_UT_DBG("Meta test type %ld\n", meta_type);

	_IPA_UT_TEST_LOG_BUF_NAME = kzalloc(_IPA_UT_TEST_LOG_BUF_SIZE,
		GFP_KERNEL);
	if (!_IPA_UT_TEST_LOG_BUF_NAME) {
		IPA_UT_ERR("failed to allocate %d bytes\n",
			_IPA_UT_TEST_LOG_BUF_SIZE);
		rc = -ENOMEM;
		goto unlock_mutex;
	}

	if (!suite->tests_cnt || !suite->tests) {
		pr_info("No tests for suite '%s'\n", suite->meta_data->name);
		goto free_mem;
	}

	ipa_ver = ipa_get_hw_type();

	IPA_ACTIVE_CLIENTS_INC_SPECIAL("IPA_UT");

	if (suite->meta_data->setup) {
		pr_info("*** Suite '%s': Run setup ***\n",
			suite->meta_data->name);
		rc = suite->meta_data->setup(&suite->meta_data->priv);
		if (rc) {
			IPA_UT_ERR("Setup failed for suite %s\n",
				suite->meta_data->name);
			rc = -EFAULT;
			goto release_clock;
		}
	} else {
		pr_info("*** Suite '%s': No Setup ***\n",
			suite->meta_data->name);
	}

	pr_info("*** Suite '%s': Run %s tests ***\n\n",
		suite->meta_data->name,
		meta_type == IPA_UT_META_TEST_REGRESSION ? "regression" : "all"
		);
	for (i = 0 ; i < suite->tests_cnt ; i++) {
		if (meta_type == IPA_UT_META_TEST_REGRESSION &&
			!suite->tests[i].run_in_regression) {
			pr_info(
				"*** Test '%s': Skip - Not in regression ***\n\n"
				, suite->tests[i].name);
			suite->tests[i].res = IPA_UT_TEST_RES_SKIP;
			continue;
		}
		if (suite->tests[i].min_ipa_hw_ver > ipa_ver ||
			suite->tests[i].max_ipa_hw_ver < ipa_ver) {
			pr_info(
				"*** Test '%s': Skip - IPA VER mismatch ***\n\n"
				, suite->tests[i].name);
			suite->tests[i].res = IPA_UT_TEST_RES_SKIP;
			continue;
		}
		if (!suite->tests[i].run) {
			pr_info(
				"*** Test '%s': Skip - No Run function ***\n\n"
				, suite->tests[i].name);
			suite->tests[i].res = IPA_UT_TEST_RES_SKIP;
			continue;
		}

		_IPA_UT_TEST_LOG_BUF_NAME[0] = '\0';
		_IPA_UT_TEST_FAIL_REPORT_IDX = 0;
		pr_info("*** Test '%s': Running... ***\n",
			suite->tests[i].name);
		rc = suite->tests[i].run(suite->meta_data->priv);
		if (rc) {
			tst_fail = true;
			suite->tests[i].res = IPA_UT_TEST_RES_FAIL;
			ipa_ut_print_log_buf(_IPA_UT_TEST_LOG_BUF_NAME);
		} else {
			suite->tests[i].res = IPA_UT_TEST_RES_SUCCESS;
		}

		pr_info(">>>>>>**** TEST '%s': %s ****<<<<<<\n",
			suite->tests[i].name, tst_fail ? "FAIL" : "SUCCESS");

		if (tst_fail)
			ipa_ut_dump_fail_report_stack();

		pr_info("\n");
	}

	if (suite->meta_data->teardown) {
		pr_info("*** Suite '%s': Run Teardown ***\n",
			suite->meta_data->name);
		rc = suite->meta_data->teardown(suite->meta_data->priv);
		if (rc) {
			IPA_UT_ERR("Teardown failed for suite %s\n",
				suite->meta_data->name);
			rc = -EFAULT;
			goto release_clock;
		}
	} else {
		pr_info("*** Suite '%s': No Teardown ***\n",
			suite->meta_data->name);
	}

	ipa_ut_show_suite_exec_summary(suite);

release_clock:
	IPA_ACTIVE_CLIENTS_DEC_SPECIAL("IPA_UT");
free_mem:
	kfree(_IPA_UT_TEST_LOG_BUF_NAME);
	_IPA_UT_TEST_LOG_BUF_NAME = NULL;
unlock_mutex:
	mutex_unlock(&ipa_ut_ctx->lock);
	return ((!rc && !tst_fail) ? count : -EFAULT);
}

/**
 * ipa_ut_dbgfs_meta_test_read() - Debugfs read func for a meta test
 * @params: read fops
 *
 * Meta test, is a test that describes other test or bunch of tests.
 *  for example, the 'all' test. Running this test will run all
 *  the tests in the suite.
 *
 * Show information regard the suite. E.g. name and description
 * If regression - List the regression tests names
 *
 * Return: Amount of characters written to user space buffer
 */
static ssize_t ipa_ut_dbgfs_meta_test_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	char *buf;
	struct ipa_ut_suite *suite;
	int nbytes;
	ssize_t cnt;
	long meta_type;
	int i;

	IPA_UT_DBG("Entry\n");

	mutex_lock(&ipa_ut_ctx->lock);
	suite = file->f_inode->i_private;
	ipa_assert_on(!suite);
	meta_type = (long)(file->private_data);
	IPA_UT_DBG("Meta test type %ld\n", meta_type);

	buf = kmalloc(IPA_UT_DEBUG_READ_BUF_SIZE, GFP_KERNEL);
	if (!buf) {
		IPA_UT_ERR("failed to allocate %d bytes\n",
			IPA_UT_DEBUG_READ_BUF_SIZE);
		cnt = 0;
		goto unlock_mutex;
	}

	if (meta_type == IPA_UT_META_TEST_ALL) {
		nbytes = scnprintf(buf, IPA_UT_DEBUG_READ_BUF_SIZE,
			"\tMeta-test running all the tests in the suite:\n"
			"\tSuite Name: %s\n"
			"\tDescription: %s\n"
			"\tNumber of test in suite: %zu\n",
			suite->meta_data->name,
			suite->meta_data->desc ?: "",
			suite->tests_cnt);
	} else {
		nbytes = scnprintf(buf, IPA_UT_DEBUG_READ_BUF_SIZE,
			"\tMeta-test running regression tests in the suite:\n"
			"\tSuite Name: %s\n"
			"\tDescription: %s\n"
			"\tRegression tests:\n",
			suite->meta_data->name,
			suite->meta_data->desc ?: "");
		for (i = 0 ; i < suite->tests_cnt ; i++) {
			if (!suite->tests[i].run_in_regression)
				continue;
			nbytes += scnprintf(buf + nbytes,
				IPA_UT_DEBUG_READ_BUF_SIZE - nbytes,
				"\t\t%s\n", suite->tests[i].name);
		}
	}

	cnt = simple_read_from_buffer(ubuf, count, ppos, buf, nbytes);
	kfree(buf);

unlock_mutex:
	mutex_unlock(&ipa_ut_ctx->lock);
	return cnt;
}

/**
 * ipa_ut_dbgfs_regression_test_open() - Debugfs open function for
 * 'regression' tests
 * @params: open fops
 *
 * Mark "Regression tests" for meta-tests later operations.
 *
 * Return: Zero (always success).
 */
static int ipa_ut_dbgfs_regression_test_open(struct inode *inode,
	struct file *filp)
{
	IPA_UT_DBG("Entry\n");

	filp->private_data = (void *)(IPA_UT_META_TEST_REGRESSION);

	return 0;
}

/**
 * ipa_ut_dbgfs_all_test_open() - Debugfs open function for 'all' tests
 * @params: open fops
 *
 * Mark "All tests" for meta-tests later operations.
 *
 * Return: Zero (always success).
 */
static int ipa_ut_dbgfs_all_test_open(struct inode *inode,
	struct file *filp)
{
	IPA_UT_DBG("Entry\n");

	filp->private_data = (void *)(IPA_UT_META_TEST_ALL);

	return 0;
}

/**
 * ipa_ut_dbgfs_test_write() - Debugfs write function for a test
 * @params: write fops
 *
 * Used to run a test.
 * Create log buffer that the test can use to store ongoing logs
 * IPA clocks need to be voted.
 * Run setup() before the test and teardown() after the tests.
 * If no such call-backs then ignore it; if failed then fail the test
 * If all succeeds, no printing to user
 * If failed, test logs and failure report will be printed to user
 *
 * Note: Test must has run function and it's supported IPA H/W version
 * must be matching. Otherwise test will fail.
 *
 * Return: Negative in failure, given characters amount in success
 */
static ssize_t ipa_ut_dbgfs_test_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	struct ipa_ut_test *test;
	struct ipa_ut_suite *suite;
	bool tst_fail = false;
	int rc = 0;
	enum ipa_hw_type ipa_ver;

	IPA_UT_DBG("Entry\n");

	mutex_lock(&ipa_ut_ctx->lock);
	test = file->f_inode->i_private;
	ipa_assert_on(!test);

	_IPA_UT_TEST_LOG_BUF_NAME = kzalloc(_IPA_UT_TEST_LOG_BUF_SIZE,
		GFP_KERNEL);
	if (!_IPA_UT_TEST_LOG_BUF_NAME) {
		IPA_UT_ERR("failed to allocate %d bytes\n",
			_IPA_UT_TEST_LOG_BUF_SIZE);
		rc = -ENOMEM;
		goto unlock_mutex;
	}

	if (!test->run) {
		IPA_UT_ERR("*** Test %s - No run func ***\n",
			test->name);
		rc = -EFAULT;
		goto free_mem;
	}

	ipa_ver = ipa_get_hw_type();
	if (test->min_ipa_hw_ver > ipa_ver ||
		test->max_ipa_hw_ver < ipa_ver) {
		IPA_UT_ERR("Cannot run test %s on IPA HW Ver %s\n",
			test->name, ipa_get_version_string(ipa_ver));
		rc = -EFAULT;
		goto free_mem;
	}

	IPA_ACTIVE_CLIENTS_INC_SPECIAL("IPA_UT");

	suite = test->suite;
	if (suite && suite->meta_data->setup) {
		IPA_UT_DBG("*** Suite '%s': Run setup ***\n",
			suite->meta_data->name);
		rc = suite->meta_data->setup(&suite->meta_data->priv);
		if (rc) {
			IPA_UT_ERR("Setup failed for suite %s\n",
				suite->meta_data->name);
			rc = -EFAULT;
			goto release_clock;
		}
	} else {
		IPA_UT_DBG("*** Suite '%s': No Setup ***\n",
			suite->meta_data->name);
	}

	IPA_UT_DBG("*** Test '%s': Running... ***\n", test->name);
	_IPA_UT_TEST_FAIL_REPORT_IDX = 0;
	rc = test->run(suite->meta_data->priv);
	if (rc)
		tst_fail = true;
	IPA_UT_DBG("*** Test %s - ***\n", tst_fail ? "FAIL" : "SUCCESS");
	if (tst_fail) {
		pr_info("=================>>>>>>>>>>>\n");
		ipa_ut_print_log_buf(_IPA_UT_TEST_LOG_BUF_NAME);
		pr_info("**** TEST %s FAILED ****\n", test->name);
		ipa_ut_dump_fail_report_stack();
		pr_info("<<<<<<<<<<<=================\n");
	}

	if (suite && suite->meta_data->teardown) {
		IPA_UT_DBG("*** Suite '%s': Run Teardown ***\n",
			suite->meta_data->name);
		rc = suite->meta_data->teardown(suite->meta_data->priv);
		if (rc) {
			IPA_UT_ERR("Teardown failed for suite %s\n",
				suite->meta_data->name);
			rc = -EFAULT;
			goto release_clock;
		}
	} else {
		IPA_UT_DBG("*** Suite '%s': No Teardown ***\n",
			suite->meta_data->name);
	}

release_clock:
	IPA_ACTIVE_CLIENTS_DEC_SPECIAL("IPA_UT");
free_mem:
	kfree(_IPA_UT_TEST_LOG_BUF_NAME);
	_IPA_UT_TEST_LOG_BUF_NAME = NULL;
unlock_mutex:
	mutex_unlock(&ipa_ut_ctx->lock);
	return ((!rc && !tst_fail) ? count : -EFAULT);
}

/**
 * ipa_ut_dbgfs_test_read() - Debugfs read function for a test
 * @params: read fops
 *
 * print information regard the test. E.g. name and description
 *
 * Return: Amount of characters written to user space buffer
 */
static ssize_t ipa_ut_dbgfs_test_read(struct file *file, char __user *ubuf,
	size_t count, loff_t *ppos)
{
	char *buf;
	struct ipa_ut_test *test;
	int nbytes;
	ssize_t cnt;

	IPA_UT_DBG("Entry\n");

	mutex_lock(&ipa_ut_ctx->lock);
	test = file->f_inode->i_private;
	ipa_assert_on(!test);

	buf = kmalloc(IPA_UT_DEBUG_READ_BUF_SIZE, GFP_KERNEL);
	if (!buf) {
		IPA_UT_ERR("failed to allocate %d bytes\n",
			IPA_UT_DEBUG_READ_BUF_SIZE);
		cnt = 0;
		goto unlock_mutex;
	}

	nbytes = scnprintf(buf, IPA_UT_DEBUG_READ_BUF_SIZE,
		"\t Test Name: %s\n"
		"\t Description: %s\n"
		"\t Suite Name: %s\n"
		"\t Run In Regression: %s\n"
		"\t Supported IPA versions: [%s -> %s]\n",
		test->name, test->desc ?: "", test->suite->meta_data->name,
		test->run_in_regression ? "Yes" : "No",
		ipa_get_version_string(test->min_ipa_hw_ver),
		test->max_ipa_hw_ver == IPA_HW_MAX ? "MAX" :
			ipa_get_version_string(test->max_ipa_hw_ver));

	if (nbytes > count)
		IPA_UT_ERR("User buf too small - return partial info\n");

	cnt = simple_read_from_buffer(ubuf, count, ppos, buf, nbytes);
	kfree(buf);

unlock_mutex:
	mutex_unlock(&ipa_ut_ctx->lock);
	return cnt;
}

/**
 * ipa_ut_framework_load_suites() - Load tests and expose them to user space
 *
 * Creates debugfs folder for each suite and then file for each test in it.
 * Create debugfs "all" file for each suite for meta-test to run all tests.
 *
 * Note: Assumes lock acquired
 *
 * Return: Zero in success, otherwise in failure
 */
int ipa_ut_framework_load_suites(void)
{
	int suite_idx;
	int tst_idx;
	struct ipa_ut_suite *suite;
	struct dentry *s_dent;
	struct dentry *f_dent;

	IPA_UT_DBG("Entry\n");

	for (suite_idx = IPA_UT_SUITE_FIRST_INDEX;
		suite_idx < IPA_UT_SUITES_COUNT; suite_idx++) {
		suite = IPA_UT_GET_SUITE(suite_idx);

		if (!suite->meta_data->name) {
			IPA_UT_ERR("No suite name\n");
			return -EFAULT;
		}

		s_dent = debugfs_create_dir(suite->meta_data->name,
			ipa_ut_ctx->test_dbgfs_suites);

		if (!s_dent || IS_ERR(s_dent)) {
			IPA_UT_ERR("fail create dbg entry - suite %s\n",
				suite->meta_data->name);
			return -EFAULT;
		}

		for (tst_idx = 0; tst_idx < suite->tests_cnt ; tst_idx++) {
			if (!suite->tests[tst_idx].name) {
				IPA_UT_ERR("No test name on suite %s\n",
					suite->meta_data->name);
				return -EFAULT;
			}
			f_dent = debugfs_create_file(
				suite->tests[tst_idx].name,
				IPA_UT_READ_WRITE_DBG_FILE_MODE, s_dent,
				&suite->tests[tst_idx],
				&ipa_ut_dbgfs_test_fops);
			if (!f_dent || IS_ERR(f_dent)) {
				IPA_UT_ERR("fail create dbg entry - tst %s\n",
					suite->tests[tst_idx].name);
				return -EFAULT;
			}
		}

		/* entry for meta-test all to run all tests in suites */
		f_dent = debugfs_create_file(_IPA_UT_RUN_ALL_TEST_NAME,
			IPA_UT_READ_WRITE_DBG_FILE_MODE, s_dent,
			suite, &ipa_ut_dbgfs_all_test_fops);
		if (!f_dent || IS_ERR(f_dent)) {
			IPA_UT_ERR("fail to create dbg entry - %s\n",
				_IPA_UT_RUN_ALL_TEST_NAME);
			return -EFAULT;
		}

		/*
		 * entry for meta-test regression to run all regression
		 * tests in suites
		 */
		f_dent = debugfs_create_file(_IPA_UT_RUN_REGRESSION_TEST_NAME,
			IPA_UT_READ_WRITE_DBG_FILE_MODE, s_dent,
			suite, &ipa_ut_dbgfs_regression_test_fops);
		if (!f_dent || IS_ERR(f_dent)) {
			IPA_UT_ERR("fail to create dbg entry - %s\n",
				_IPA_UT_RUN_ALL_TEST_NAME);
			return -EFAULT;
		}
	}

	return 0;
}

/**
 * ipa_ut_framework_enable() - Enable the framework
 *
 * Creates the tests and suites debugfs entries and load them.
 * This will expose the tests to user space.
 *
 * Return: Zero in success, otherwise in failure
 */
static int ipa_ut_framework_enable(void)
{
	int ret = 0;

	IPA_UT_DBG("Entry\n");

	mutex_lock(&ipa_ut_ctx->lock);

	if (ipa_ut_ctx->enabled) {
		IPA_UT_ERR("Already enabled\n");
		goto unlock_mutex;
	}

	ipa_ut_ctx->test_dbgfs_suites = debugfs_create_dir("suites",
		ipa_ut_ctx->test_dbgfs_root);
	if (!ipa_ut_ctx->test_dbgfs_suites ||
		IS_ERR(ipa_ut_ctx->test_dbgfs_suites)) {
		IPA_UT_ERR("failed to create suites debugfs dir\n");
		ret = -EFAULT;
		goto unlock_mutex;
	}

	if (ipa_ut_framework_load_suites()) {
		IPA_UT_ERR("failed to load the suites into debugfs\n");
		ret = -EFAULT;
		goto fail_clean_suites_dbgfs;
	}

	ipa_ut_ctx->enabled = true;
	goto unlock_mutex;

fail_clean_suites_dbgfs:
	debugfs_remove_recursive(ipa_ut_ctx->test_dbgfs_suites);
unlock_mutex:
	mutex_unlock(&ipa_ut_ctx->lock);
	return ret;
}

/**
 * ipa_ut_framework_disable() - Disable the framework
 *
 * Remove the tests and suites debugfs exposure.
 *
 * Return: Zero in success, otherwise in failure
 */
static int ipa_ut_framework_disable(void)
{
	int ret = 0;

	IPA_UT_DBG("Entry\n");

	mutex_lock(&ipa_ut_ctx->lock);

	if (!ipa_ut_ctx->enabled) {
		IPA_UT_ERR("Already disabled\n");
		goto unlock_mutex;
	}

	debugfs_remove_recursive(ipa_ut_ctx->test_dbgfs_suites);

	ipa_ut_ctx->enabled = false;

unlock_mutex:
	mutex_unlock(&ipa_ut_ctx->lock);
	return ret;
}

/**
 * ipa_ut_dbgfs_enable_write() - Debugfs enable file write fops
 * @params: write fops
 *
 * Input should be number. If 0, then disable. Otherwise enable.
 *
 * Return: if failed then negative value, if succeeds, amount of given chars
 */
static ssize_t ipa_ut_dbgfs_enable_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	char lcl_buf[IPA_UT_DEBUG_WRITE_BUF_SIZE];
	s8 option = 0;
	int ret;

	IPA_UT_DBG("Entry\n");

	if (sizeof(lcl_buf) < count + 1) {
		IPA_UT_ERR("No enough space\n");
		return -E2BIG;
	}

	if (copy_from_user(lcl_buf, buf, count)) {
		IPA_UT_ERR("fail to copy buf from user space\n");
		return -EFAULT;
	}

	lcl_buf[count] = '\0';
	if (kstrtos8(lcl_buf, 0, &option)) {
		IPA_UT_ERR("fail convert str to s8\n");
		return -EINVAL;
	}

	if (option == 0)
		ret = ipa_ut_framework_disable();
	else
		ret = ipa_ut_framework_enable();

	return ret ?: count;
}

/**
 * ipa_ut_dbgfs_enable_read() - Debugfs enable file read fops
 * @params: read fops
 *
 * To show to user space if the I/S is enabled or disabled.
 *
 * Return: amount of characters returned to user space
 */
static ssize_t ipa_ut_dbgfs_enable_read(struct file *file, char __user *ubuf,
	size_t count, loff_t *ppos)
{
	const char *status;

	IPA_UT_DBG("Entry\n");

	mutex_lock(&ipa_ut_ctx->lock);
	status = ipa_ut_ctx->enabled ?
		"Enabled - Write 0 to disable\n" :
		"Disabled - Write 1 to enable\n";
	mutex_unlock(&ipa_ut_ctx->lock);
	return simple_read_from_buffer(ubuf, count, ppos,
		status, strlen(status));
}

/**
 * ipa_ut_framework_init() - Unit-tests framework initialization
 *
 * Complete tests initialization: Each tests needs to point to it's
 * corresponing suite.
 * Creates the framework debugfs root directory  under IPA directory.
 * Create enable debugfs file - to enable/disable the framework.
 *
 * Return: Zero in success, otherwise in failure
 */
static int ipa_ut_framework_init(void)
{
	struct dentry *dfile_enable;
	int ret;
	int suite_idx;
	int test_idx;
	struct ipa_ut_suite *suite;

	IPA_UT_DBG("Entry\n");

	ipa_assert_on(!ipa_ut_ctx);

	ipa_ut_ctx->ipa_dbgfs_root = ipa_debugfs_get_root();
	if (!ipa_ut_ctx->ipa_dbgfs_root) {
		IPA_UT_ERR("No IPA debugfs root entry\n");
		return -EFAULT;
	}

	mutex_lock(&ipa_ut_ctx->lock);

	/* tests needs to point to their corresponding suites structures */
	for (suite_idx = IPA_UT_SUITE_FIRST_INDEX;
		suite_idx < IPA_UT_SUITES_COUNT; suite_idx++) {
		suite = IPA_UT_GET_SUITE(suite_idx);
		ipa_assert_on(!suite);
		if (!suite->tests) {
			IPA_UT_DBG("No tests for suite %s\n",
				suite->meta_data->name);
			continue;
		}
		for (test_idx = 0; test_idx < suite->tests_cnt; test_idx++) {
			suite->tests[test_idx].suite = suite;
			IPA_UT_DBG("Updating test %s info for suite %s\n",
				suite->tests[test_idx].name,
				suite->meta_data->name);
		}
	}

	ipa_ut_ctx->test_dbgfs_root = debugfs_create_dir("test",
		ipa_ut_ctx->ipa_dbgfs_root);
	if (!ipa_ut_ctx->test_dbgfs_root ||
		IS_ERR(ipa_ut_ctx->test_dbgfs_root)) {
		IPA_UT_ERR("failed to create test debugfs dir\n");
		ret = -EFAULT;
		goto unlock_mutex;
	}

	dfile_enable = debugfs_create_file("enable",
		IPA_UT_READ_WRITE_DBG_FILE_MODE,
		ipa_ut_ctx->test_dbgfs_root, 0, &ipa_ut_dbgfs_enable_fops);
	if (!dfile_enable || IS_ERR(dfile_enable)) {
		IPA_UT_ERR("failed to create enable debugfs file\n");
		ret = -EFAULT;
		goto fail_clean_dbgfs;
	}

	_IPA_UT_TEST_FAIL_REPORT_IDX = 0;
	ipa_ut_ctx->inited = true;
	IPA_UT_DBG("Done\n");
	ret = 0;
	goto unlock_mutex;

fail_clean_dbgfs:
	debugfs_remove_recursive(ipa_ut_ctx->test_dbgfs_root);
unlock_mutex:
	mutex_unlock(&ipa_ut_ctx->lock);
	return ret;
}

/**
 * ipa_ut_framework_destroy() - Destroy the UT framework info
 *
 * Disable it if enabled.
 * Remove the debugfs entries using the root entry
 */
static void ipa_ut_framework_destroy(void)
{
	IPA_UT_DBG("Entry\n");

	mutex_lock(&ipa_ut_ctx->lock);
	if (ipa_ut_ctx->enabled)
		ipa_ut_framework_disable();
	if (ipa_ut_ctx->inited)
		debugfs_remove_recursive(ipa_ut_ctx->test_dbgfs_root);
	mutex_unlock(&ipa_ut_ctx->lock);
}

/**
 * ipa_ut_ipa_ready_cb() - IPA ready CB
 *
 * Once IPA is ready starting initializing  the unit-test framework
 */
static void ipa_ut_ipa_ready_cb(void *user_data)
{
	IPA_UT_DBG("Entry\n");
	(void)ipa_ut_framework_init();
}

/**
 * ipa_ut_module_init() - Module init
 *
 * Create the framework context, wait for IPA driver readiness
 * and Initialize it.
 * If IPA driver already ready, continue initialization immediately.
 * if not, wait for IPA ready notification by IPA driver context
 */
static int __init ipa_ut_module_init(void)
{
	int ret;

	IPA_UT_INFO("Loading IPA test module...\n");

	ipa_ut_ctx = kzalloc(sizeof(struct ipa_ut_context), GFP_KERNEL);
	if (!ipa_ut_ctx) {
		IPA_UT_ERR("Failed to allocate ctx\n");
		return -ENOMEM;
	}
	mutex_init(&ipa_ut_ctx->lock);

	if (!ipa_is_ready()) {
		IPA_UT_DBG("IPA driver not ready, registering callback\n");
		ret = ipa_register_ipa_ready_cb(ipa_ut_ipa_ready_cb, NULL);

		/*
		 * If we received -EEXIST, IPA has initialized. So we need
		 * to continue the initing process.
		 */
		if (ret != -EEXIST) {
			if (ret) {
				IPA_UT_ERR("IPA CB reg failed - %d\n", ret);
				kfree(ipa_ut_ctx);
				ipa_ut_ctx = NULL;
			}
			return ret;
		}
	}

	ret = ipa_ut_framework_init();
	if (ret) {
		IPA_UT_ERR("framework init failed\n");
		kfree(ipa_ut_ctx);
		ipa_ut_ctx = NULL;
	}
	return ret;
}

/**
 * ipa_ut_module_exit() - Module exit function
 *
 * Destroys the Framework and removes its context
 */
static void ipa_ut_module_exit(void)
{
	IPA_UT_DBG("Entry\n");

	if (!ipa_ut_ctx)
		return;

	ipa_ut_framework_destroy();
	kfree(ipa_ut_ctx);
	ipa_ut_ctx = NULL;
}

module_init(ipa_ut_module_init);
module_exit(ipa_ut_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA Unit Test module");
