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
/*
 * Common data types for use by the MobiCore Kernel API Driver
 */
#ifndef _MC_KAPI_COMMON_H
#define _MC_KAPI_COMMON_H

#include "connection.h"
#include "mcinq.h"

void mcapi_insert_connection(struct connection *connection);
void mcapi_remove_connection(uint32_t seq);
unsigned int mcapi_unique_id(void);

#define MC_DAEMON_PID			0xFFFFFFFF
#define MC_DRV_MOD_DEVNODE_FULLPATH	"/dev/mobicore"

/* dummy function helper macro */
#define DUMMY_FUNCTION()		do {} while (0)

/* Found in main.c */
extern struct device *mc_kapi;

/* Found in clientlib.c */
extern struct mutex device_mutex;

/* Found in clientlib.c */
extern struct mutex global_mutex;

#define MCDRV_ERROR(dev, txt, ...) \
	dev_err(dev, "%s() ### ERROR: " txt, __func__, ##__VA_ARGS__)

#if defined(DEBUG)

/* #define DEBUG_VERBOSE */
#if defined(DEBUG_VERBOSE)
#define MCDRV_DBG_VERBOSE		MCDRV_DBG
#else
#define MCDRV_DBG_VERBOSE(...)		DUMMY_FUNCTION()
#endif

#define MCDRV_DBG(dev, txt, ...) \
	dev_info(dev, "%s(): " txt, __func__, ##__VA_ARGS__)

#define MCDRV_DBG_WARN(dev, txt, ...) \
	dev_warn(dev, "%s() WARNING: " txt, __func__, ##__VA_ARGS__)

#define MCDRV_DBG_ERROR(dev, txt, ...) \
	dev_err(dev, "%s() ### ERROR: " txt, __func__, ##__VA_ARGS__)

#define MCDRV_ASSERT(cond) \
	do { \
		if (unlikely(!(cond))) { \
			panic("mc_kernelapi Assertion failed: %s:%d\n", \
			      __FILE__, __LINE__); \
		} \
	} while (0)

#elif defined(NDEBUG)

#define MCDRV_DBG_VERBOSE(...)		DUMMY_FUNCTION()
#define MCDRV_DBG(...)			DUMMY_FUNCTION()
#define MCDRV_DBG_WARN(...)		DUMMY_FUNCTION()
#define MCDRV_DBG_ERROR(...)		DUMMY_FUNCTION()

#define MCDRV_ASSERT(...)		DUMMY_FUNCTION()

#else
#error "Define DEBUG or NDEBUG"
#endif /* [not] defined(DEBUG_MCMODULE) */

#define assert(expr)			MCDRV_ASSERT(expr)

#endif /* _MC_KAPI_COMMON_H */
