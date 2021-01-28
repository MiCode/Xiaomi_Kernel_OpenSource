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

/**********************************************
 * unified_power_internal.h
 * This header file includes:
 * 1. Externs of time profiling related APIs
 **********************************************/
#ifndef UNIFIED_POWER_INTERNAL_H
#define UNIFIED_POWER_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/errno.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#define UPOWER_ENABLE (1)

/* #define EARLY_PORTING_SPOWER */
/* will not get leakage from leakage driver */
/* #define UPOWER_UT */
/* #define UPOWER_PROFILE_API_TIME */
#define UPOWER_RCU_LOCK

/* for unified power driver internal use */
#define UPOWER_LOG (1)
#define UPOWER_TAG "[UPOWER]"

#if UPOWER_LOG
#define upower_error(fmt, args...) pr_debug(UPOWER_TAG fmt, ##args)
#define upower_debug(fmt, args...) pr_debug(UPOWER_TAG fmt, ##args)
#else
#define upower_error(fmt, args...)
#define upower_debug(fmt, args...)
#endif

/*
 * bit operation
 */
#undef BIT
#define BIT(bit) (1U << (bit))

#define MSB(range)	(1 ? range)
#define LSB(range)	(0 ? range)
/**
 * Genearte a mask wher MSB to LSB are all 0b1
 * @r:	Range in the form of MSB:LSB
 */
#define BITMASK(r) \
	(((unsigned long)-1 >> (31 - MSB(r))) & ~((1U << LSB(r)) - 1))

/**
 * Set value at MSB:LSB. For example, BITS(7:3, 0x5A)
 * will return a value where bit 3 to bit 7 is 0x5A
 * @r:	Range in the form of MSB:LSB
 */
/* BITS(MSB:LSB, value) => Set value at MSB:LSB  */
#define BITS(r, val) ((val << LSB(r)) & BITMASK(r))

#define GET_BITS_VAL(_bits_, _val_) \
	(((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))

#ifdef UPOWER_RCU_LOCK
extern void upower_read_lock(void);
extern void upower_read_unlock(void);
#endif

#ifdef UPOWER_PROFILE_API_TIME
enum {
	GET_PWR,
	GET_TBL_PTR,
	UPDATE_TBL_PTR,
	TEST_NUM
};
extern void upower_get_start_time_us(unsigned int type);
extern void upower_get_diff_time_us(unsigned int type);
extern void print_diff_results(unsigned int type);
#endif

#ifdef __cplusplus
}
#endif

#endif
