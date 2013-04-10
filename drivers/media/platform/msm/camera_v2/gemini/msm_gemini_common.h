/* Copyright (c) 2010,2013, The Linux Foundation. All rights reserved.
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

#ifndef MSM_GEMINI_COMMON_H
#define MSM_GEMINI_COMMON_H

#define MSM_GEMINI_DEBUG
#ifdef MSM_GEMINI_DEBUG
#define GMN_DBG(fmt, args...) pr_debug(fmt, ##args)
#else
#define GMN_DBG(fmt, args...) do { } while (0)
#endif

#define GMN_PR_ERR   pr_err

enum GEMINI_MODE {
	GEMINI_MODE_DISABLE,
	GEMINI_MODE_OFFLINE,
	GEMINI_MODE_REALTIME,
	GEMINI_MODE_REALTIME_ROTATION
};

enum GEMINI_ROTATION {
	GEMINI_ROTATION_0,
	GEMINI_ROTATION_90,
	GEMINI_ROTATION_180,
	GEMINI_ROTATION_270
};

#endif /* MSM_GEMINI_COMMON_H */
