/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_DEF_H
#define _FRAME_SYNC_DEF_H

#ifdef FS_UT
#include <string.h>
#include <stdlib.h>         /* Needed by memory allocate */
#else
/* INSTEAD of using stdio.h, you have to use the following include */
#include <linux/slab.h>     /* Needed by memory allocate */
#include <linux/string.h>
#endif // FS_UT


/******************************************************************************/
// global define / variable / macro
/******************************************************************************/
#define SENSOR_MAX_NUM 6
#define CAMMUX_ID_INVALID 256

#define TS_DIFF_TABLE_LEN (((SENSOR_MAX_NUM)*(SENSOR_MAX_NUM-1))/2)

#define FS_TOLERANCE 1000

#define ALGO_AUTO_LISTEN_VSYNC 0

#if !defined(FS_UT)
#define TWO_STAGE_FS 1
#if defined(TWO_STAGE_FS)
#define QUERY_CCU_TS_AT_SOF 1
#endif // TWO_STAGE_FS
#endif // FS_UT


/*
 * get timestamp by using bellow method
 * e.g. CCU / N3D / TSREC / etc.
 */
#define USING_CCU
#ifdef USING_CCU
/*
 * delay power ON/OFF and operate CCU to fs_set_sync()
 *
 * P.S: due to sensor streaming on before seninf/tg setup,
 *      this is the way to get correct tg information
 *      after seninf being config compeleted.
 */
#define DELAY_CCU_OP
#endif // USING_CCU

// #define USING_N3D


#define SUPPORT_FS_NEW_METHOD
#ifdef SUPPORT_FS_NEW_METHOD
#define MASTER_IDX_NONE 255

#define SUPPORT_AUTO_EN_SA_MODE

#define FORCE_USING_SA_MODE

/*
 * force adjust smaller diff one for MW-frame no. matching
 */
#define FORCE_ADJUST_SMALLER_DIFF

#endif // SUPPORT_FS_NEW_METHOD

#if defined(SUPPORT_FS_NEW_METHOD)
#ifdef FS_UT
#include <stdatomic.h>
#define FS_Atomic_T atomic_int
#define FS_ATOMIC_INIT(n, p)      (atomic_init((p), (n)))
#define FS_ATOMIC_SET(n, p)       (atomic_store((p), (n)))
#define FS_ATOMIC_READ(p)         (atomic_load(p))
#define FS_ATOMIC_FETCH_OR(n, p)  (atomic_fetch_or((p), (n)))
#define FS_ATOMIC_FETCH_AND(n, p) (atomic_fetch_and((p), (n)))
#define FS_ATOMIC_XCHG(n, p)      (atomic_exchange((p), (n)))
#else
#include <linux/atomic.h>
#define FS_Atomic_T atomic_t
#define FS_ATOMIC_INIT(n, p)      (atomic_set((p), (n)))
#define FS_ATOMIC_SET(n, p)       (atomic_set((p), (n)))
#define FS_ATOMIC_READ(p)         (atomic_read(p))
#define FS_ATOMIC_FETCH_OR(n, p)  (atomic_fetch_or((n), (p)))
#define FS_ATOMIC_FETCH_AND(n, p) (atomic_fetch_and((n), (p)))
#define FS_ATOMIC_XCHG(n, p)      (atomic_xchg((p), (n)))
#endif // FS_UT
#endif // SUPPORT_FS_NEW_METHOD


/*
 * macro for clear code
 */
#ifdef FS_UT
#define likely(x)          (__builtin_expect((x), 1))
#define unlikely(x)        (__builtin_expect((x), 0))
#define FS_POPCOUNT(n)     (__builtin_popcount(n))
#define FS_MUTEX_LOCK(p)   (pthread_mutex_lock(p))
#define FS_MUTEX_UNLOCK(p) (pthread_mutex_unlock(p))
#define FS_CALLOC(LEN, T)  (calloc((LEN), (T)))
#define FS_FREE(buf)       (free(buf))
#else
#define FS_POPCOUNT(n)     (hweight32(n))
#define FS_MUTEX_LOCK(p)   (mutex_lock(p))
#define FS_MUTEX_UNLOCK(p) (mutex_unlock(p))
#define FS_CALLOC(LEN, T)  (kcalloc((LEN), (T), (GFP_KERNEL)))
#define FS_FREE(buf)       (kfree(buf))
#endif // FS_UT

#define FS_CHECK_BIT(n, p)    (check_bit_atomic((n), (p)))
#define FS_WRITE_BIT(n, i, p) (write_bit_atomic((n), (i), (p)))
#define FS_READ_BITS(p)       (FS_ATOMIC_READ((p)))


/* using v4l2_ctrl_request_setup */
#define USING_V4L2_CTRL_REQUEST_SETUP


/* for (FrameSync + Sensor Driver + CCU) Single Cam IT using */
// #define FS_SENSOR_CCU_IT


/*
 * for test using, sync with diff => un-sync
 */
// #define SYNC_WITH_CUSTOM_DIFF
#if defined(SYNC_WITH_CUSTOM_DIFF)
#define CUSTOM_DIFF_SENSOR_IDX 255
#define CUSTOM_DIFF_US 0
#endif // SYNC_WITH_CUSTOM_DIFF


#if !defined(FS_UT)
/*
 * frame_sync_console
 */
#include <linux/device.h>  /* for device structure */
#endif // FS_UT
/******************************************************************************/


/******************************************************************************/
// frame_record_st (record shutter and framelength settings)
/******************************************************************************/
#define RECORDER_DEPTH 4
struct frame_record_st {
	unsigned int *framelength_lc;
	unsigned int *shutter_lc;
};

#endif
