/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LBAT_SERVICE_H__
#define __MTK_LBAT_SERVICE_H__

struct lbat_user;

/* extern function */
struct lbat_user *lbat_user_register_ext(const char *name, unsigned int *thd_volt_arr,
					 unsigned int thd_volt_size,
					 void (*callback)(unsigned int thd_volt));
struct lbat_user *lbat_user_register(const char *name, unsigned int hv_thd_volt,
				     unsigned int lv1_thd_volt,
				     unsigned int lv2_thd_volt,
				     void (*callback)(unsigned int thd_volt));
int lbat_user_set_debounce(struct lbat_user *user,
			   unsigned int hv_deb_prd, unsigned int hv_deb_times,
			   unsigned int lv_deb_prd, unsigned int lv_deb_times);
unsigned int lbat_read_raw(void);
unsigned int lbat_read_volt(void);

#endif	/* __MTK_LBAT_SERVICE_H__ */
