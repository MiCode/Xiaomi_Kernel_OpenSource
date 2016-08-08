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

#include "ipa_ut_framework.h"

/**
 * Example IPA Unit-test suite
 * To be a reference for writing new suites and tests.
 * This suite is also used as unit-test for the testing framework itself.
 * Structure:
 *	1- Define the setup and teardown  functions
 *	 Not Mandatory. Null may be used as well
 *	2- For each test, define its Run() function
 *	3- Use IPA_UT_DEFINE_SUITE_START() to start defining the suite
 *	4- use IPA_UT_ADD_TEST() for adding tests within
 *	 the suite definition block
 *	5- IPA_UT_DEFINE_SUITE_END() close the suite definition
 */

static int ipa_test_example_dummy;

static int ipa_test_example_suite_setup(void **ppriv)
{
	IPA_UT_DBG("Start Setup - set 0x1234F\n");

	ipa_test_example_dummy = 0x1234F;
	*ppriv = (void *)&ipa_test_example_dummy;

	return 0;
}

static int ipa_test_example_teardown(void *priv)
{
	IPA_UT_DBG("Start Teardown\n");
	IPA_UT_DBG("priv=0x%p - value=0x%x\n", priv, *((int *)priv));

	return 0;
}

static int ipa_test_example_test1(void *priv)
{
	IPA_UT_LOG("priv=0x%p - value=0x%x\n", priv, *((int *)priv));
	ipa_test_example_dummy++;

	return 0;
}

static int ipa_test_example_test2(void *priv)
{
	IPA_UT_LOG("priv=0x%p - value=0x%x\n", priv, *((int *)priv));
	ipa_test_example_dummy++;

	return 0;
}

static int ipa_test_example_test3(void *priv)
{
	IPA_UT_LOG("priv=0x%p - value=0x%x\n", priv, *((int *)priv));
	ipa_test_example_dummy++;

	return 0;
}

static int ipa_test_example_test4(void *priv)
{
	IPA_UT_LOG("priv=0x%p - value=0x%x\n", priv, *((int *)priv));
	ipa_test_example_dummy++;

	IPA_UT_TEST_FAIL_REPORT("failed on test");

	return -EFAULT;
}

/* Suite definition block */
IPA_UT_DEFINE_SUITE_START(example, "Example suite",
	ipa_test_example_suite_setup, ipa_test_example_teardown)
{
	IPA_UT_ADD_TEST(test1, "This is test number 1",
		ipa_test_example_test1, false, IPA_HW_v1_0, IPA_HW_MAX),

	IPA_UT_ADD_TEST(test2, "This is test number 2",
		ipa_test_example_test2, false, IPA_HW_v1_0, IPA_HW_MAX),

	IPA_UT_ADD_TEST(test3, "This is test number 3",
		ipa_test_example_test3, false, IPA_HW_v1_1, IPA_HW_v2_6),

	IPA_UT_ADD_TEST(test4, "This is test number 4",
		ipa_test_example_test4, false, IPA_HW_v1_1, IPA_HW_MAX),

} IPA_UT_DEFINE_SUITE_END(example);
