/*
 * Copyright (C) 2018 MediaTek Inc.
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

/******************************************************************************
 *
 *
 * Filename:
 * ---------
 *   audio_wakelock.h
 *
 * Project:
 * --------
 *   MT6765
 *
 * Description:
 * ------------
 *   Audio wake lock
 *
 * Author:
 * -------
 *   YS Hsieh
 *
 *---------------------------------------------------------------------------
 *
 *
 ****************************************************************************
 */

#ifndef AUDIO_WAKELOCK_H
#define AUDIO_WAKELOCK_H


#include <linux/device.h>
#include <linux/pm_wakeup.h>

/* wake lock relate*/
#define aud_wake_lock_init(ws, name) wakeup_source_init(ws, name)
#define aud_wake_lock_destroy(ws) wakeup_source_trash(ws)
#define aud_wake_lock(ws) __pm_stay_awake(ws)
#define aud_wake_unlock(ws) __pm_relax(ws)

#endif
