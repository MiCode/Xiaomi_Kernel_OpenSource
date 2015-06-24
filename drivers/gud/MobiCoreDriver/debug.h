/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _MC_DEBUG_H_
#define _MC_DEBUG_H_

#include "main.h"	/* g_ctx */

#define MCDRV_ERROR(txt, ...) \
	dev_err(g_ctx.mcd, "%s() ### ERROR: " txt "\n", \
		__func__, \
		##__VA_ARGS__)

/* dummy function helper macro. */
#define DUMMY_FUNCTION()	do {} while (0)

#ifdef DEBUG

#ifdef DEBUG_VERBOSE
#define MCDRV_DBG_VERBOSE	MCDRV_DBG
#else
#define MCDRV_DBG_VERBOSE(...)	DUMMY_FUNCTION()
#endif

#define MCDRV_DBG(txt, ...) \
	dev_info(g_ctx.mcd, "%s(): " txt "\n", \
		 __func__, \
		 ##__VA_ARGS__)

#define MCDRV_DBG_WARN(txt, ...) \
	dev_warn(g_ctx.mcd, "%s() WARNING: " txt "\n", \
		 __func__, \
		 ##__VA_ARGS__)

#define MCDRV_ASSERT(cond) \
	do { \
		if (unlikely(!(cond))) { \
			panic("Assertion failed: %s:%d\n", \
			      __FILE__, __LINE__); \
		} \
	} while (0)

#else /* DEBUG */

#define MCDRV_DBG_VERBOSE(...)	DUMMY_FUNCTION()
#define MCDRV_DBG(...)		DUMMY_FUNCTION()
#define MCDRV_DBG_WARN(...)	DUMMY_FUNCTION()

#define MCDRV_ASSERT(...)	DUMMY_FUNCTION()

#endif /* !DEBUG */

#endif /* _MC_DEBUG_H_ */
