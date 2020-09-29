/* SPDX-License-Identifier: GPL-2.0 */
/*
 * vow_assert.h  --  VoW assertion definition
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

#ifndef VOW_ASSERT_H
#define VOW_ASSERT_H

#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif
#include "vow.h"

#ifdef CONFIG_MTK_AEE_FEATURE
#define VOW_ASSERT(exp) \
	do { \
		if (!(exp)) { \
			aee_kernel_exception_api(__FILE__, \
						 __LINE__, \
						 DB_OPT_DEFAULT, \
						 "[VOW]", \
						 "ASSERT("#exp") fail!!"); \
		} \
	} while (0)
#else
#define VOW_ASSERT(exp) \
	do { \
		if (!(exp)) { \
			pr_notice("ASSERT("#exp")fail:\""__FILE__"\", %uL\n", \
			__LINE__); \
		} \
	} while (0)
#endif

#endif /* end of VOW_ASSERT_H */

