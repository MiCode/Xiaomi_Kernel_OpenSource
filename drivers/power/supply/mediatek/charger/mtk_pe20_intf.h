/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#ifndef __MTK_PE20_INTF_H__
#define __MTK_PE20_INTF_H__

/* pe 2.0*/
struct pe20_profile {
	unsigned int vbat;
	unsigned int vchr;
};

struct mtk_pe20 {
	struct mutex access_lock;
	struct mutex pmic_sync_lock;
	struct wakeup_source *suspend_lock;
	int ta_vchr_org;
	int idx;
	int vbus;
	bool to_check_chr_type;
	bool is_cable_out_occur; /* Plug out happened while detect PE+20 */
	bool is_connect;
	bool is_enabled;
	struct pe20_profile profile[10];

	int vbat_orig; /* Measured VBAT before cable impedance measurement */
	int aicr_cable_imp; /* AICR to set after cable impedance measurement */
};

#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT

extern int mtk_pe20_init(struct charger_manager *pinfo);
extern int mtk_pe20_reset_ta_vchr(struct charger_manager *pinfo);
extern int mtk_pe20_check_charger(struct charger_manager *pinfo);
extern int mtk_pe20_start_algorithm(struct charger_manager *pinfo);
extern int mtk_pe20_set_charging_current(struct charger_manager *pinfo,
					 unsigned int *ichg,
					 unsigned int *aicr);

extern void mtk_pe20_set_to_check_chr_type(struct charger_manager *pinfo,
					bool check);
extern void mtk_pe20_set_is_enable(struct charger_manager *pinfo, bool enable);
extern void mtk_pe20_set_is_cable_out_occur(struct charger_manager *pinfo,
					bool out);

extern bool mtk_pe20_get_to_check_chr_type(struct charger_manager *pinfo);
extern bool mtk_pe20_get_is_connect(struct charger_manager *pinfo);
extern bool mtk_pe20_get_is_enable(struct charger_manager *pinfo);

#else /* NOT CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT */

static inline int mtk_pe20_init(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}

static inline int mtk_pe20_reset_ta_vchr(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}

static inline int mtk_pe20_check_charger(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}

static inline int mtk_pe20_start_algorithm(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}

static inline int mtk_pe20_set_charging_current(struct charger_manager *pinfo,
						unsigned int *ichg,
						unsigned int *aicr)
{
	return -ENOTSUPP;
}

static inline void mtk_pe20_set_to_check_chr_type(struct charger_manager *pinfo,
						  bool check)
{
}

static inline void mtk_pe20_set_is_enable(struct charger_manager *pinfo,
					   bool enable)
{
}

static inline
void mtk_pe20_set_is_cable_out_occur(struct charger_manager *pinfo, bool out)
{
}

static inline bool mtk_pe20_get_to_check_chr_type(struct charger_manager *pinfo)
{
	return false;
}

static inline bool mtk_pe20_get_is_connect(struct charger_manager *pinfo)
{
	return false;
}

static inline bool mtk_pe20_get_is_enable(struct charger_manager *pinfo)
{
	return false;
}

#endif /* CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT */

#endif /* __MTK_PE20_INTF_H__ */
