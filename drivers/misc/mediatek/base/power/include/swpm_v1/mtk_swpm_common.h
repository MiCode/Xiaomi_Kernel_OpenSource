/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MTK_SWPM_COMMON_H__
#define __MTK_SWPM_COMMON_H__

#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <mt-plat/sync_write.h>
#include <mtk_swpm_platform.h>


/* LOG */
#undef TAG
#define TAG     "[SWPM] "

#define swpm_err                swpm_info
#define swpm_warn               swpm_info
#define swpm_info(fmt, args...) pr_notice(TAG""fmt, ##args)
#define swpm_dbg(fmt, args...)                           \
	do {                                             \
		if (swpm_debug)                          \
			swpm_info(fmt, ##args);          \
		else                                     \
			pr_debug(TAG""fmt, ##args);      \
	} while (0)

#define swpm_readl(addr)	__raw_readl(addr)
#define swpm_writel(addr, val)	mt_reg_sync_writel(val, addr)

#define swpm_lock(lock)		mutex_lock(lock)
#define swpm_unlock(lock)	mutex_unlock(lock)

#define swpm_get_status(type)  ((swpm_status & (1 << type)) >> type)
#define swpm_set_status(type)  (swpm_status |= (1 << type))
#define swpm_clr_status(type)  (swpm_status &= ~(1 << type))
#define for_each_pwr_mtr(i)    for (i = 0; i < NR_POWER_METER; i++)

extern bool swpm_debug;
extern unsigned int swpm_status;
extern struct mutex swpm_mutex;

extern char *swpm_power_rail_to_string(enum power_rail p);
extern int swpm_platform_init(void);
extern void swpm_set_enable(unsigned int type, unsigned int enable);
extern void swpm_set_update_cnt(unsigned int type, unsigned int cnt);
extern void swpm_send_init_ipi(unsigned int addr, unsigned int size,
	unsigned int ch_num);
extern void swpm_update_lkg_table(void);
#endif

