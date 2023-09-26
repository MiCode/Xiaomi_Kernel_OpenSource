/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __MTK_PE_50_INTF_H
#define __MTK_PE_50_INTF_H

#include <mt-plat/prop_chgalgo_class.h>

struct mtk_pe50 {
	struct prop_chgalgo_device *pca_algo;
	struct notifier_block nb;
	bool online;
	bool is_enabled;
};

enum mtk_pe50_notify_src {
	MTK_PE50_NOTISRC_TCP,
	MTK_PE50_NOTISRC_CHG,
	MTK_PE50_NOTISRC_MAX,
};

#ifdef CONFIG_MTK_PUMP_EXPRESS_50_SUPPORT
extern int mtk_pe50_init(struct charger_manager *chgmgr);
extern bool mtk_pe50_is_ready(struct charger_manager *chgmgr);
extern int mtk_pe50_start(struct charger_manager *chgmgr);
extern bool mtk_pe50_is_running(struct charger_manager *chgmgr);
extern int mtk_pe50_plugout_reset(struct charger_manager *chgmgr);
extern bool mtk_pe50_get_is_connect(struct charger_manager *chgmgr);
extern bool mtk_pe50_get_is_enable(struct charger_manager *chgmgr);
extern void mtk_pe50_set_is_enable(struct charger_manager *chgmgr, bool enable);
extern int mtk_pe50_notifier_call(struct charger_manager *chgmgr,
				  enum mtk_pe50_notify_src src,
				  unsigned long event, void *data);
extern int mtk_pe50_deinit(struct charger_manager *chgmgr);
extern int mtk_pe50_thermal_throttling(struct charger_manager *chgmgr, int uA);
extern int mtk_pe50_set_jeita_vbat_cv(struct charger_manager *chgmgr, int uV);
extern int mtk_pe50_stop_algo(struct charger_manager *chgmgr, bool rerun);
#else
static inline int mtk_pe50_init(struct charger_manager *chgmgr)
{
	return -ENOTSUPP;
}

static inline bool mtk_pe50_is_ready(struct charger_manager *chgmgr)
{
	return false;
}

static inline int mtk_pe50_start(struct charger_manager *chgmgr)
{
	return -ENOTSUPP;
}

static inline bool mtk_pe50_is_running(struct charger_manager *chgmgr)
{
	return false;
}

static inline int mtk_pe50_plugout_reset(struct charger_manager *chgmgr)
{
	return -ENOTSUPP;
}

static inline bool mtk_pe50_get_is_connect(struct charger_manager *chgmgr)
{
	return false;
}

static inline bool mtk_pe50_get_is_enable(struct charger_manager *chgmgr)
{
	return false;
}

static inline void mtk_pe50_set_is_enable(struct charger_manager *chgmgr,
					  bool enable)
{
}

static inline int mtk_pe50_notifier_call(struct charger_manager *chgmgr,
					 enum mtk_pe50_notify_src src,
					 unsigned long event, void *data)
{
	return -ENOTSUPP;
}

static inline int mtk_pe50_deinit(struct charger_manager *chgmgr)
{
	return -ENOTSUPP;
}

static inline int mtk_pe50_thermal_throttling(struct charger_manager *chgmgr,
					      int uA)
{
	return -ENOTSUPP;
}

static inline int mtk_pe50_set_jeita_vbat_cv(struct charger_manager *chgmgr,
					     int uV)
{
	return -ENOTSUPP;
}

static inline int mtk_pe50_stop_algo(struct charger_manager *chgmgr, bool rerun)
{
	return -ENOTSUPP;
}
#endif /* CONFIG_MTK_PUMP_EXPRESS_50_SUPPORT */
#endif /* __MTK_PE_50_INTF_H */
