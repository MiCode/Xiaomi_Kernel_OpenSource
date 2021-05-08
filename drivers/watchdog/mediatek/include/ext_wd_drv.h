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

#ifndef __EXT_WD_DRV_H
#define __EXT_WD_DRV_H
#include <linux/types.h>
#include <linux/kthread.h> /*define NR_CPUS*/
#ifdef CONFIG_MTK_WATCHDOG_COMMON
#include <mt-plat/mtk_wd_api.h>
#else
#include <mach/wd_api.h>
#endif

/* direct api */
void wdt_arch_reset(char mode);
void wdt_dump_reg(void);

int  mtk_rgu_cfg_dvfsrc(int enable);
int  mtk_rgu_cfg_emi_dcs(int enable);

int  mtk_rgu_dram_reserved(int enable);
int  mtk_rgu_mcu_cache_preserve(int enable);

/*
 * Query if SYSRST has happened.
 *
 * Return:
 * 1: Happened.
 * 0: Not happened.
 */
int  mtk_rgu_status_is_sysrst(void);

/*
 * Query if EINTRST has happened.
 *
 * Return:
 * 1: Happened.
 * 0: Not happened.
 */
int  mtk_rgu_status_is_eintrst(void);


void mtk_wd_resume(void);
void mtk_wd_suspend(void);

int  mtk_wdt_confirm_hwreboot(void);
int  mtk_wdt_dfd_count_en(int value);
int  mtk_wdt_dfd_thermal1_dis(int value);
int  mtk_wdt_dfd_thermal2_dis(int value);
int  mtk_wdt_dfd_timeout(int value);
int  mtk_wdt_enable(enum wk_wdt_en en);
void mtk_wdt_mode_config(bool dual_mode_en, bool irq, bool ext_en,
			    bool ext_pol, bool wdt_en);
int  mtk_wdt_request_mode_set(int mark_bit, enum wk_req_mode mode);
int  mtk_wdt_request_en_set(int mark_bit, enum wk_req_en en);
void mtk_wdt_restart(enum wd_restart_type type);
void mtk_wdt_set_time_out_value(unsigned int value);
int  mtk_wdt_swsysret_config(int bit, int set_value);

/* direct api */
int mpcore_wk_wdt_config(int reserved, int reserved2, int timeout_val);
int mpcore_wdt_restart(enum wd_restart_type type);
int local_wdt_enable(enum wk_wdt_en en);
/* used for extend request */
int mtk_local_wdt_misc_config(int bit, int set_value, int *reserved);
void mpcore_wk_wdt_stop(void);
extern void dump_wdk_bind_info(void);

#if NR_CPUS == 1
#define nr_cpu_ids		1
#else
extern unsigned int nr_cpu_ids;
#endif

#define __ENABLE_WDT_SYSFS__
#ifdef __ENABLE_WDT_SYSFS__
#include <linux/proc_fs.h>
#include <linux/sysfs.h>
/*---------------------------------------------------------------------------*/
/*define sysfs entry for configuring debug level and sysrq*/
ssize_t mtk_rgu_attr_show(struct kobject *kobj, struct attribute *attr,
			     char *buffer);
ssize_t mtk_rgu_attr_store(struct kobject *kobj, struct attribute *attr,
			      const char *buffer, size_t size);
ssize_t mtk_rgu_pause_wdt_show(struct kobject *kobj, char *page);
ssize_t mtk_rgu_pause_wdt_store(struct kobject *kobj, const char *page,
				    size_t size);
#endif

/* end */
#endif
