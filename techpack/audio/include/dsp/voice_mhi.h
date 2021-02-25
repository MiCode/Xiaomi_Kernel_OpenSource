/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
