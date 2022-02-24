/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MTK_PRINTK_CTRL_H__
#define __MTK_PRINTK_CTRL_H__

#if IS_ENABLED(CONFIG_MTK_PRINTK)
void set_detect_count(int val);
int get_detect_count(void);
bool mt_get_uartlog_status(void);
void update_uartlog_status(bool new_value, int value);
void set_logtoomuch_enable(void);
void set_logtoomuch_disable(void);
bool get_logtoomuch_status(void);
#else
static inline void set_detect_count(int val)
{
}

static inline int get_detect_count(void)
{
	return 0;
}
static inline bool mt_get_uartlog_status(void)
{
	return false;
}

static inline void update_uartlog_status(bool new_value, int value)
{
}
static inline void set_logtoomuch_enable(void)
{
}

static inline void set_logtoomuch_disable(void)
{
}

bool get_logtoomuch_status(void)
{
	return false;
}

#endif

#endif
