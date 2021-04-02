/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MTK_PRINTK_CTRL_H__
#define __MTK_PRINTK_CTRL_H__

#if IS_ENABLED(CONFIG_MTK_PRINTK)
void set_detect_count(int val);
int get_detect_count(void);
void mt_disable_uart(void);
void mt_enable_uart(void);
#else
static inline void set_detect_count(int val)
{
}

static inline int get_detect_count(void)
{
	return 0;
}
static inline void mt_disable_uart(void)
{
}

static inline void mt_enable_uart(void)
{
}
#endif

#endif
