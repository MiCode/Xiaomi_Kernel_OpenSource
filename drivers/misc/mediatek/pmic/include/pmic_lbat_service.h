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

#ifndef __PMIC_LBAT_SERVICE_H__
#define __PMIC_LBAT_SERVICE_H__

#include <linux/timer.h>

struct lbat_thd_t;

struct lbat_user {
	char name[30];
	struct lbat_thd_t *hv_thd;
	struct lbat_thd_t *lv1_thd;
	struct lbat_thd_t *lv2_thd;
	void (*callback)(unsigned int);
	unsigned int deb_cnt;
	struct lbat_thd_t *deb_thd_ptr;
	unsigned int hv_deb_prd;
	unsigned int hv_deb_times;
	unsigned int lv_deb_prd;
	unsigned int lv_deb_times;
	struct timer_list deb_timer;
	struct work_struct deb_work;
};

/* extern function */
extern int lbat_service_init(void);
extern int lbat_user_register(struct lbat_user *user, const char *name,
	unsigned int hv_thd_volt, unsigned int lv1_thd_volt,
	unsigned int lv2_thd_volt, void (*callback)(unsigned int));
extern int lbat_user_set_debounce(struct lbat_user *user,
	unsigned int hv_deb_prd, unsigned int hv_deb_cnt,
	unsigned int lv_deb_prd, unsigned int lv_deb_cnt);
extern void lbat_suspend(void);
extern void lbat_resume(void);
extern unsigned int lbat_read_raw(void);
extern unsigned int lbat_read_volt(void);
extern void lbat_dump_reg(void);
extern int lbat_debug_init(struct dentry *debug_dir);

#endif	/* __PMIC_LBAT_SERVICE_H__ */
