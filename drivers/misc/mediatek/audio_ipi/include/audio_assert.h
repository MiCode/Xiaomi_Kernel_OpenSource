/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef AUDIO_ASSERT_H
#define AUDIO_ASSERT_H

#define AUD_ASSERT(exp) \
	do { \
		if (!(exp)) { \
			pr_notice("ASSERT("#exp")!! \""  __FILE__ "\", %uL\n", \
				  __LINE__); \
			WARN_ON(1); \
		} \
	} while (0)

#define AUD_WARNING(string) \
	do { \
		pr_notice("AUD_WARNING(" string"): \""  __FILE__ "\", %uL\n", \
			  __LINE__); \
		WARN_ON(1); \
	} while (0)


#endif /* end of AUDIO_ASSERT_H */

