/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _IPA_UT_SUITE_LIST_H_
#define _IPA_UT_SUITE_LIST_H_

#include "ipa_ut_framework.h"
#include "ipa_ut_i.h"

/**
 * Declare every suite here so that it will be found later below
 * No importance for order.
 */
IPA_UT_DECLARE_SUITE(mhi);
IPA_UT_DECLARE_SUITE(dma);
IPA_UT_DECLARE_SUITE(pm);
IPA_UT_DECLARE_SUITE(example);
IPA_UT_DECLARE_SUITE(hw_stats);
IPA_UT_DECLARE_SUITE(wdi3);


/**
 * Register every suite inside the below block.
 * Unregistered suites will be ignored
 */
IPA_UT_DEFINE_ALL_SUITES_START
{
	IPA_UT_REGISTER_SUITE(mhi),
	IPA_UT_REGISTER_SUITE(dma),
	IPA_UT_REGISTER_SUITE(pm),
	IPA_UT_REGISTER_SUITE(example),
	IPA_UT_REGISTER_SUITE(hw_stats),
	IPA_UT_REGISTER_SUITE(wdi3),
} IPA_UT_DEFINE_ALL_SUITES_END;

#endif /* _IPA_UT_SUITE_LIST_H_ */
