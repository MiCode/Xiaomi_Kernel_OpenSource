// SPDX-License-Identifier: GPL-2.0-only
/*
 * wcn_integrate_boot.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __WCN_INTEGRATE_BOOT_H__
#define __WCN_INTEGRATE_BOOT_H__

#include <misc/wcn_integrate_platform.h>

int start_integrate_wcn(u32 subsys);
int stop_integrate_wcn(u32 subsys);
int start_integrate_wcn_truely(u32 subsys);
int stop_integrate_wcn_truely(u32 subsys);
int wcn_proc_native_start(void *arg);
int wcn_proc_native_stop(void *arg);
void wcn_boot_init(void);
void wcn_power_wq(struct work_struct *pwork);
void wcn_device_poweroff(void);
int wcn_reset_mdbg_notifier_init(void);
int wcn_reset_mdbg_notifier_deinit(void);
extern void wcn_dfs_status_clear(void);
struct reg_wcn_aon_ahb_reserved2 {
	u32 priority : 2;
	u32 reserved : 30;
};
#define BTWF_SYS_ABNORMAL 0x0deadbad
#define GNSS_SYS_ABNORMAL 0x1deadbad

#ifndef BTWF_SW_DEEP_SLEEP_MAGIC
#define BTWF_SW_DEEP_SLEEP_MAGIC (0x504C5344)
#endif

#endif
