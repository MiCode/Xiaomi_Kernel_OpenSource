/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
IPA_UT_DECLARE_SUITE(example);


/**
 * Register every suite inside the below block.
 * Unregistered suites will be ignored
 */
IPA_UT_DEFINE_ALL_SUITES_START
{
	IPA_UT_REGISTER_SUITE(mhi),
	IPA_UT_REGISTER_SUITE(dma),
	IPA_UT_REGISTER_SUITE(example),
} IPA_UT_DEFINE_ALL_SUITES_END;

#endif /* _IPA_UT_SUITE_LIST_H_ */
