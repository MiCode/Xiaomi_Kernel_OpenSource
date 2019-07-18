/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __AUDIO_EXT_CLK_UP_H_
#define __AUDIO_EXT_CLK_UP_H_

#ifdef CONFIG_COMMON_CLK_QCOM
int audio_ref_clk_platform_init(void);
void audio_ref_clk_platform_exit(void);
#else
static inline int audio_ref_clk_platform_init(void)
{
	return 0;
}

static inline void audio_ref_clk_platform_exit(void)
{
}

#endif /*CONFIG_COMMON_CLK_QCOM*/
#endif
