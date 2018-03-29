 /*
  * Copyright (C) 2016 MediaTek Inc.
  *
  * Author: Sakya <jeff_chang@richtek.com>
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
};

#ifdef CONFIG_RT7207_ADAPTER
enum { /* charge status */
	RT7207_CC_MODE,
	RT7207_CV_MODE,
};

#define MTK_VDM_FAIL  (-1)
#define MTK_VDM_SUCCESS  (0)

/* mtk_check_pe_ready_snk
 * return true --> pe state == PD_CONNECT_PE_READY_SNK
 * return false --> pe state != PD_CONNECT_PE_READY
 */
extern bool mtk_check_pe_ready_snk(void);

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
 *	return id or MTK_VDM_FAIL */
extern int mtk_get_ta_id(struct tcpc_device *tcpc);

/* mtk_get_ta_charger_status
 *	return RT7207_CC_MODE/RT7207_CV_MODE/MTK_VDM_FAIL */
extern int mtk_get_ta_charger_status(struct tcpc_device *tcpc);


/* mtk_get_ta_temperature
 *	return temperature/MTK_VDM_FAIL */
extern int mtk_get_ta_temperature(struct tcpc_device *tcpc);

/* mtk_show_ta_info
 *	print voltage/current/temperature
 *	return MTK_VDM_SUCCESS/MTK_VDM_FAIL */
extern int mtk_show_ta_info(struct tcpc_device *tcpc);

/* mtk_set_ta_boundary_cap
 *	use mtk_vdm_ta_cap to pass target voltage & current
 *	return MTK_VDM_SUCCESS/MTK_VDM_FAIL */
extern int mtk_set_ta_boundary_cap(
	struct tcpc_device *tcpc, struct mtk_vdm_ta_cap *cap);


/* mtk_set_ta_uvlo
 *	set uvlo voltage threshold
 *	return MTK_VDM_SUCCESS/MTK_VDM_FAIL */
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



#else /* not config RT7027 PD adapter */
static inline int mtk_get_ta_id(struct tcpc_device *tcpc)
{
	return -1;
}

static inline struct vdm_ta_info *mtk_get_ta_cap(struct tcpc_device *tcpc)
{
	return NULL;
}

static inline int mtk_get_ta_charger_status(struct tcpc_device *tcpc)
{
	return -1;
}

static inline int mtk_get_ta_current_cap(
		struct tcpc_device *tcpc, struct mtk_vdm_ta_cap *cap)
{
	return -1;
}

static inline int mtk_get_ta_temperature(struct tcpc_device *tcpc)
{
	return -1;
}

static inline int mtk_update_ta_info(struct tcpc_device *tcpc)
{
	return -1;
}

static inline int mtk_set_ta_boundary_cap(
		struct tcpc_device *tcpc, struct mtk_vdm_ta_cap *cap)
{
	return -1;
}

static inline int mtk_rqst_ta_cap(
		struct tcpc_device *tcpc, struct mtk_vdm_ta_cap *cap)
{
	return -1;
}

static inline int mtk_set_ta_uvlo(struct tcpc_device *tcpc, int mv)
{
	return -1;
}
#endif /* CONFIG_RT7207_ADAPTER */

#endif /* __LINUX_TA_VDM_H */
