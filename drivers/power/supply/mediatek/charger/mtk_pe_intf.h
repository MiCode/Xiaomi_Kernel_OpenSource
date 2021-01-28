/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_PE_INTF_H
#define __MTK_PE_INTF_H


struct mtk_pe {
	struct mutex access_lock;
	struct mutex pmic_sync_lock;
	struct wakeup_source suspend_lock;
	int ta_vchr_org; /* uA */
	bool to_check_chr_type;
	bool to_tune_ta_vchr;
	bool is_cable_out_occur; /* Plug out happened while detecting PE+ */
	bool is_connect;
	bool is_enabled;
};

#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT

extern int mtk_pe_init(struct charger_manager *pinfo);
extern int mtk_pe_reset_ta_vchr(struct charger_manager *pinfo);
extern int mtk_pe_check_charger(struct charger_manager *pinfo);
extern int mtk_pe_start_algorithm(struct charger_manager *pinfo);
extern int mtk_pe_set_charging_current(struct charger_manager *pinfo,
	unsigned int *ichg, unsigned int *aicr);

extern void mtk_pe_set_to_check_chr_type(struct charger_manager *pinfo,
					bool check);
extern void mtk_pe_set_is_enable(struct charger_manager *pinfo, bool enable);
extern void mtk_pe_set_is_cable_out_occur(struct charger_manager *pinfo,
					bool out);

extern bool mtk_pe_get_to_check_chr_type(struct charger_manager *pinfo);
extern bool mtk_pe_get_is_connect(struct charger_manager *pinfo);
extern bool mtk_pe_get_is_enable(struct charger_manager *pinfo);

#else /* NOT CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT */

static inline int mtk_pe_init(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}
static inline int mtk_pe_reset_ta_vchr(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}
static inline int mtk_pe_check_charger(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}
static inline int mtk_pe_start_algorithm(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}

static inline int mtk_pe_plugout_reset(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}

static inline int mtk_pe_set_charging_current(struct charger_manager *pinfo,
	unsigned int *ichg, unsigned int *aicr)
{
	return -ENOTSUPP;
}

static inline void mtk_pe_set_to_check_chr_type(struct charger_manager *pinfo,
						bool check)
{
}
static inline void mtk_pe_set_is_cable_out_occur(struct charger_manager *pinfo,
						bool out)
{
}
static inline void mtk_pe_set_is_enable(struct charger_manager *pinfo,
						bool enable)
{
}

static inline bool mtk_pe_get_to_check_chr_type(struct charger_manager *pinfo)
{
	return false;
}
static inline bool mtk_pe_get_is_connect(struct charger_manager *pinfo)
{
	return false;
}
static inline bool mtk_pe_get_is_enable(struct charger_manager *pinfo)
{
	return false;
}
#endif /* CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT */


#endif /* __MTK_PE_INTF_H */
