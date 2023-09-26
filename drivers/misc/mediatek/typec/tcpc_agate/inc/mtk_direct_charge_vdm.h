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


#ifndef __LINUX_TA_VDM_H
#define __LINUX_TA_VDM_H

#include "tcpm.h"

struct mtk_vdm_ta_cap {
	int cur;
	int vol;
	int temp;
};

enum {
	PD_USB_NOT_SUPPORT = -1,
	PD_USB_DISCONNECT = 0,
	PD_USB_CONNECT = 1,
};

struct pd_ta_stat {
	unsigned char chg_mode:1;
	unsigned char dc_en:1;
	unsigned char dpc_en:1;
	unsigned char pc_en:1;
	unsigned char ovp:1;
	unsigned char otp:1;
	unsigned char uvp:1;
	unsigned char rvs_cur:1;
	unsigned char ping_chk_fail:1;
};

static inline bool mtk_check_pe_ready_snk(void)
{
	return false;
}

#ifdef CONFIG_TCPC_CLASS

/* tcpc_is_usb_connect
 * return PD_USB_NOT_SUPPORT : not support
 * return PD_USB_DISCONNECT : usb disconnect
 * return PD_USB_CONNECT : usb connect
 */
int tcpc_is_usb_connect(void);
bool mtk_is_pd_chg_ready(void);
bool mtk_is_ta_typec_only(void);
bool mtk_is_pep30_en_unlock(void);
#else
static inline int tcpc_is_usb_connect(void)
{
	return PD_USB_NOT_SUPPORT;
}

static inline int mtk_is_ta_typec_only(void)
{
	return true;
}

#if CONFIG_MTK_GAUGE_VERSION == 20
static inline bool mtk_is_pd_chg_ready(void)
{
	return false;
}

static inline bool mtk_is_pep30_en_unlock(void)
{
	return false;
}
#endif

#endif /* CONFIG_TCPC_RT1711H */

#ifdef CONFIG_RT7207_ADAPTER
enum { /* charge status */
	RT7207_CC_MODE,
	RT7207_CV_MODE,
};

#define MTK_VDM_FAIL  (-1)
#define MTK_VDM_SUCCESS  (0)
#define MTK_VDM_SW_BUSY	(1)


/* mtk_direct_charge_vdm_init
 *	1. get tcpc_device handler
 *	2. init mutex & wakelock
 *	3. register tcp notifier
 *	4. add debugfs node
 */
extern int mtk_direct_charge_vdm_init(void);

/* mtk_vdm_config_dfp
 *	only DFP can use vdm request, this function will check your mode.
 *	if mode == DFP, return 0
 *	if mode != DFP, try to request data swap
 *	return 0 --> mode now = DFP
 *	reutrn <0 --> config fail
 */
extern int mtk_vdm_config_dfp(void);

/* mtk_get_ta_id
 * return id or MTK_VDM_FAIL
 */
extern int mtk_get_ta_id(struct tcpc_device *tcpc);

/* mtk_get_ta_charger_status
 *	return RT7207_CC_MODE/RT7207_CV_MODE/MTK_VDM_FAIL
 */
extern int mtk_get_ta_charger_status(
		struct tcpc_device *tcpc, struct pd_ta_stat *ta);


/* mtk_get_ta_temperature
 *	return temperature/MTK_VDM_FAIL
 */
extern int mtk_get_ta_temperature(struct tcpc_device *tcpc, int *temp);

/* mtk_set_ta_boundary_cap
 *	use mtk_vdm_ta_cap to pass target voltage & current
 *	return MTK_VDM_SUCCESS/MTK_VDM_FAIL
 */
extern int mtk_set_ta_boundary_cap(
	struct tcpc_device *tcpc, struct mtk_vdm_ta_cap *cap);


/* mtk_set_ta_uvlo
 *	set uvlo voltage threshold
 *	return MTK_VDM_SUCCESS/MTK_VDM_FAIL
 */
extern int mtk_set_ta_uvlo(struct tcpc_device *tcpc, int mv);

extern int mtk_get_ta_current_cap(struct tcpc_device *tcpc,
					struct mtk_vdm_ta_cap *cap);

extern int mtk_get_ta_setting_dac(struct tcpc_device *tcpc,
					struct mtk_vdm_ta_cap *cap);


extern int mtk_get_ta_boundary_cap(struct tcpc_device *tcpc,
					struct mtk_vdm_ta_cap *cap);


extern int mtk_set_ta_cap(struct tcpc_device *tcpc, struct mtk_vdm_ta_cap *cap);

extern int mtk_get_ta_cap(struct tcpc_device *tcpc,
					struct mtk_vdm_ta_cap *cap);
extern int mtk_monitor_ta_inform(struct tcpc_device *tcpc,
					struct mtk_vdm_ta_cap *cap);

/* mtk_enable_direct_charge
 */
extern int mtk_enable_direct_charge(struct tcpc_device *tcpc, bool en);

extern int mtk_enable_ta_dplus_dect(
			struct tcpc_device *tcpc, bool en, int time);

#if CONFIG_MTK_GAUGE_VERSION == 20
extern int mtk_clr_ta_pingcheck_fault(struct tcpc_device *tcpc);
#endif

#else /* not config RT7027 PD adapter */

static inline int mtk_direct_charge_vdm_init(void)
{
	return -1;
}

static inline int mtk_get_ta_id(void *tcpc)
{
	return -1;
}

static inline int mtk_set_ta_cap(
		void *tcpc, struct mtk_vdm_ta_cap *cap)
{
	cap->cur = cap->vol = cap->temp = 0;
	return -1;
}

static inline int mtk_get_ta_cap(
		void *tcpc, struct mtk_vdm_ta_cap *cap)
{
	cap->cur = cap->vol = cap->temp = 0;
	return -1;
}

static inline int mtk_get_ta_charger_status(
			void *tcpc, struct pd_ta_stat *ta)
{
	return -1;
}

static inline int mtk_get_ta_current_cap(
		void *tcpc, struct mtk_vdm_ta_cap *cap)
{
	cap->cur = cap->vol = cap->temp = 0;
	return -1;
}

static inline int mtk_get_ta_temperature(void *tcpc, int *temp)
{
	return -1;
}

static inline int mtk_update_ta_info(void *tcpc)
{
	return -1;
}

static inline int mtk_set_ta_boundary_cap(
		void *tcpc, struct mtk_vdm_ta_cap *cap)
{
	cap->cur = cap->vol = cap->temp = 0;
	return -1;
}

static inline int mtk_rqst_ta_cap(
		void *tcpc, struct mtk_vdm_ta_cap *cap)
{
	cap->cur = cap->vol = cap->temp = 0;
	return -1;
}

static inline int mtk_set_ta_uvlo(void *tcpc, int mv)
{
	return -1;
}

static inline int mtk_show_ta_info(void *tcpc)
{
	return -1;
}

static inline int mtk_get_ta_setting_dac(
			void *tcpc, struct mtk_vdm_ta_cap *cap)
{
	cap->cur = cap->vol = cap->temp = 0;
	return -1;
}

static inline int mtk_get_ta_boundary_cap(
			void *tcpc, struct mtk_vdm_ta_cap *cap)
{
	cap->cur = cap->vol = cap->temp = 0;
	return -1;
}

static inline int mtk_enable_direct_charge(void *tcpc, bool en)
{
	return -1;
}

static inline int mtk_enable_ta_dplus_dect(
			void *tcpc, bool en, int time)
{
	return -1;
}

#if CONFIG_MTK_GAUGE_VERSION == 20
static inline int mtk_clr_ta_pingcheck_fault(struct tcpc_device *tcpc)
{
	return -1;
}
#endif

static inline int mtk_monitor_ta_inform(void *tcpc,
					struct mtk_vdm_ta_cap *cap)
{
	cap->cur = cap->vol = cap->temp = 0;
	return -1;
}
#endif /* CONFIG_RT7207_ADAPTER */

#endif /* __LINUX_TA_VDM_H */
