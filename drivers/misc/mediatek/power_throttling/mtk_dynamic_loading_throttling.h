/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_DYNAMIC_LOADING_H__
#define __MTK_DYNAMIC_LOADING_H__

enum DLPT_PRIO_TAG {
	DLPT_PRIO_PBM = 0,
	DLPT_PRIO_CPU_B = 1,
	DLPT_PRIO_CPU_L = 2,
	DLPT_PRIO_GPU = 3,
	DLPT_PRIO_MD = 4,
	DLPT_PRIO_MD5 = 5,
	DLPT_PRIO_FLASHLIGHT = 6,
	DLPT_PRIO_VIDEO = 7,
	DLPT_PRIO_WIFI = 8,
	DLPT_PRIO_BACKLIGHT = 9
};

typedef void (*dlpt_callback)(unsigned int val);

#if IS_ENABLED(CONFIG_MTK_DYNAMIC_LOADING_POWER_THROTTLING)
void register_dlpt_notify(dlpt_callback dlpt_cb,
			  enum DLPT_PRIO_TAG prio_val);
#else
static int register_dlpt_notify(dlpt_callback dlpt_cb,
				enum DLPT_PRIO_TAG prio_val)
{ return 0; }
#endif

#endif	/* __MTK_DYNAMIC_LOADING_H__ */

