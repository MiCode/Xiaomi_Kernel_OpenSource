/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef AUDIO_ASSERT_H
#define AUDIO_ASSERT_H

#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

#ifdef CONFIG_MTK_AEE_FEATURE
#define AUD_ASSERT(exp) \
	do { \
		if (!(exp)) { \
			aee_kernel_exception_api(__FILE__, \
						 __LINE__, \
						 DB_OPT_DEFAULT, \
						 "[Audio]", \
						 "ASSERT("#exp") fail!!"); \
		} \
	} while (0)
#else
#define AUD_ASSERT(exp) \
	do { \
		if (!(exp)) { \
			pr_notice("ASSERT("#exp")!! \""  __FILE__ "\", %uL\n", \
				  __LINE__); \
		} \
	} while (0)
#endif

#endif /* end of AUDIO_ASSERT_H */

