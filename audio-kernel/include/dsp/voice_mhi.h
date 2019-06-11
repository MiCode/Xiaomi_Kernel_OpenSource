/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#ifndef __VOICE_MHI_H__
#define __VOICE_MHI_H__

#ifdef CONFIG_VOICE_MHI
int voice_mhi_start(void);
int voice_mhi_end(void);
#else
static inline int voice_mhi_start(void)
{
	return 0;
}

static inline int voice_mhi_end(void)
{
	return 0;
}
#endif

#endif
