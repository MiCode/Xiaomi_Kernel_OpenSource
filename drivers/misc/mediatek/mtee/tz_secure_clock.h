/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _TZ_SECURE_CLOCK_H_INCLUDE
#define _TZ_SECURE_CLOCK_H_INCLUDE
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/rtc.h>



#include "tz_cross/trustzone.h"
#include "tz_cross/ta_icnt.h"
#include "trustzone/kree/system.h"
#include "kree_int.h"
#include "tz_counter.h"
#include "tz_fileio.h"
#include <trustzone/kree/mem.h>
#include "trustzone/kree/system.h"
#include <tz_cross/ta_mem.h>
#include <linux/mm.h>

#ifdef CONFIG_MTK_RTC /* secure time depends on rtc */
#if 1 /* disable playready driver for now */
#define TZ_SECURETIME_SUPPORT
/* #define CONFIG_HAS_EARLYSUSPEND_GB */
#endif
#endif


#include "tz_cross/ta_securetime.h"
#define GB_TIME_FILE_SAVE_PATH "/data/SecureTD"
#define DRM_UINT64 unsigned long long




uint32_t TEE_update_gb_time_intee(KREE_SESSION_HANDLE session,
					KREE_SESSION_HANDLE mem_session);
uint32_t TEE_update_gb_time_infile(KREE_SESSION_HANDLE session,
					KREE_SESSION_HANDLE mem_session);
uint32_t TEE_Icnt_time(KREE_SESSION_HANDLE session,
					KREE_SESSION_HANDLE mem_session);
int update_securetime_thread_gb(void *data);
#endif  /* _TZ_SECURE_CLOCK_H_INCLUDE */

