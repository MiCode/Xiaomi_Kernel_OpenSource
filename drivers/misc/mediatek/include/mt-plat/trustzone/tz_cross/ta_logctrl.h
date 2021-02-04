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

#ifndef __TA_LOG_CTRL_H__
#define __TA_LOG_CTRL_H__

#define TZ_TA_LOG_CTRL_UUID   "a80ef6e1-de27-11e2-a28f-0800200c9a66"

/* should match MTEE_LOG_LVL in log.h */
#define MTEE_LOG_CTRL_LVL_INFO    0x00000000
#define MTEE_LOG_CTRL_LVL_DEBUG   0x00000001
#define MTEE_LOG_CTRL_LVL_PRINTF  0x00000002
#define MTEE_LOG_CTRL_LVL_WARN    0x00000003
#define MTEE_LOG_CTRL_LVL_BUG     0x00000004
#define MTEE_LOG_CTRL_LVL_ASSERT  0x00000005
#define MTEE_LOG_CTRL_LVL_DISABLE 0x0000000f

/* Command for Log ctrl TA */
#define TZCMD_LOG_CTRL_SET_LVL   0

#endif	/* __TA_LOG_CTRL_H__ */
