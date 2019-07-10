/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef LOAD_TRACK_H
#define LOAD_TRACK_H

#ifdef CONFIG_MTK_LOAD_TRACKER

/**
 * DESCRIPTION:
 *	This function MIGHT SLEEP.
 *	Register a Loading Tracking callback function fn of polling
 *	time polling_ms. Callback function fn will be called per
 *	polling_ms with loading value.
 *	Callback function fn: loading may get -EOVERFLOW if idle/wall
 *	counter overflowed, otherwise loading will be between 0~100.
 *
 * RETURN VALUE:
 *	On Success zero is returned.
 *
 * ERRORS:
 *	ENOMEM Out of memory, registration fail.
 *	EINVAL a. Duplicated registration. reg_loading_tracking and
 *	          unreg_loading_tracking must be paired.
 *	       b. fn == NULL or polling_ms == 0 or load_track_init fail
 *	       c. Function not implemented. Please check kernel config
 *	          CONFIG_MTK_LOAD_TRACKER
 */
#define reg_loading_tracking(p_fn, polling_ms) \
reg_loading_tracking_sp(p_fn, polling_ms, __func__)
extern int reg_loading_tracking_sp(void (*fn)(int loading),
	unsigned long polling_ms, const char *caller);

/**
 * DESCRIPTION:
 *	This function MIGHT SLEEP.
 *	Unregister a Loading Tracking callback function fn.
 *	Loading Tracker will disable callback and free related memory.
 *	For resource saving, please make sure to call unreg_loading_tracking
 *	if you don't need to be informed.
 *
 * RETURN VALUE:
 *	On Success zero is returned.
 *
 * ERRORS:
 *	EINVAL a. Duplicated unregistration. reg_loading_tracking and
 *	          unreg_loading_tracking must be paired.
 *	       b. fn == NULL or load_track_init fail
 *	       c. Function not implemented. Please check kernel config
 *	          CONFIG_MTK_LOAD_TRACKER
 */
#define unreg_loading_tracking(p_fn) \
unreg_loading_tracking_sp(p_fn, __func__)
extern int unreg_loading_tracking_sp(void (*fn)(int loading),
	const char *caller);

#else

#define reg_loading_tracking(p_fn, polling_ms) \
reg_loading_tracking_sp(p_fn, polling_ms, __func__)
static inline int reg_loading_tracking_sp(void (*fn)(int loading),
	unsigned long polling_ms, const char *caller)
{ return -EINVAL; }

#define reg_loading_tracking(p_fn, polling_ms) \
reg_loading_tracking_sp(p_fn, polling_ms, __func__)
static inline int unreg_loading_tracking_sp(void (*fn)(int loading),
	const char *caller)
{ return -EINVAL; }

#endif

#endif

