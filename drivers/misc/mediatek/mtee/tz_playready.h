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

#ifndef _TZ_PLAYREADY_H_INCLUDE
#define _TZ_PLAYREADY_H_INCLUDE
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
/* #define TZ_PLAYREADY_SECURETIME_SUPPORT */
#include <tz_cross/ta_playready.h>
#define PR_TIME_FILE_SAVE_PATH "/data/playready/SecureTD"
#define DRM_UINT64 unsigned long long

uint32_t TEE_update_pr_time_intee(KREE_SESSION_HANDLE session);
uint32_t TEE_update_pr_time_infile(KREE_SESSION_HANDLE session);
uint32_t TEE_Icnt_pr_time(KREE_SESSION_HANDLE session);
int update_securetime_thread(void *data);
#endif
