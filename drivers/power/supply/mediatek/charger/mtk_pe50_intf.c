/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#include <mt-plat/v1/prop_chgalgo_class.h>
#include "mtk_charger_intf.h"
#include "mtk_switch_charging.h"

static int __pe50_notifier_call(struct notifier_block *nb, unsigned long event,
				void *data)
{
	struct mtk_pe50 *pe50 = container_of(nb, struct mtk_pe50, nb);
	struct charger_manager *chgmgr =
		container_of(pe50, struct charger_manager, pe5);

	chr_info("%s %s\n", __func__, prop_chgalgo_notify_evt_tostring(event));
	switch (event) {
	case PCA_NOTIEVT_ALGO_STOP:
		_wake_up_charger(chgmgr);
		break;
	default:
		break;
	}
	return 0;
}

int mtk_pe50_init(struct charger_manager *chgmgr)
{
	int ret;
	struct mtk_pe50 *pe50 = &chgmgr->pe5;

	if (!chgmgr->enable_pe_5)
		return -ENOTSUPP;

	pe50->pca_algo = prop_chgalgo_dev_get_by_name("pca_algo_dv2");
	if (!pe50->pca_algo) {
		chr_err("[PE50] Get pca_algo fail\n");
		ret = -EINVAL;
	} else {
		ret = prop_chgalgo_init_algo(pe50->pca_algo);
		if (ret < 0) {
			chr_err("[PE50] Init algo fail\n");
			pe50->pca_algo = NULL;
			goto out;
		}
		pe50->nb.notifier_call = __pe50_notifier_call;
		ret = prop_chgalgo_notifier_register(pe50->pca_algo, &pe50->nb);

		pe50->is_enabled = true;
	}
out:
	return ret;
}

int mtk_pe50_deinit(struct charger_manager *chgmgr)
{
	struct mtk_pe50 *pe50 = &chgmgr->pe5;

	if (!chgmgr->enable_pe_5)
		return -ENOTSUPP;
	if (!pe50->pca_algo)
		return 0;

	return prop_chgalgo_notifier_unregister(pe50->pca_algo, &pe50->nb);
}

bool mtk_pe50_is_ready(struct charger_manager *chgmgr)
{
	struct mtk_pe50 *pe50 = &chgmgr->pe5;

	if (!chgmgr->enable_pe_5 || !chgmgr->enable_hv_charging)
		return false;
	return prop_chgalgo_is_algo_ready(pe50->pca_algo);
}

int mtk_pe50_start(struct charger_manager *chgmgr)
{
	int ret;
	struct mtk_pe50 *pe50 = &chgmgr->pe5;

	charger_enable_vbus_ovp(chgmgr, false);
	ret = prop_chgalgo_start_algo(pe50->pca_algo);
	if (ret < 0)
		charger_enable_vbus_ovp(chgmgr, true);
	return ret;
}

bool mtk_pe50_is_running(struct charger_manager *chgmgr)
{
	bool running;
	struct mtk_pe50 *pe50 = &chgmgr->pe5;

	running = prop_chgalgo_is_algo_running(pe50->pca_algo);
	if (!running)
		charger_enable_vbus_ovp(chgmgr, true);
	return running;
}

int mtk_pe50_plugout_reset(struct charger_manager *chgmgr)
{
	int ret;
	struct mtk_pe50 *pe50 = &chgmgr->pe5;
	struct switch_charging_alg_data *swchgalg = chgmgr->algorithm_data;

	ret = prop_chgalgo_plugout_reset(pe50->pca_algo);
	pe50->online = false;
	swchgalg->state = CHR_CC;
	charger_enable_vbus_ovp(chgmgr, true);
	return ret;
}

bool mtk_pe50_get_is_connect(struct charger_manager *chgmgr)
{
	if (chgmgr->enable_pe_5 == false)
		return false;

	return chgmgr->pe5.online;
}

bool mtk_pe50_get_is_enable(struct charger_manager *chgmgr)
{
	if (chgmgr->enable_pe_5 == false)
		return false;

	return chgmgr->pe5.is_enabled;
}

void mtk_pe50_set_is_enable(struct charger_manager *chgmgr, bool enable)
{
	if (chgmgr->enable_pe_5 == false)
		return;

	chgmgr->pe5.is_enabled = enable;
}

#if 1
int mtk_pe50_notifier_call(struct charger_manager *chgmgr,
			   enum mtk_pe50_notify_src src, unsigned long event,
			   void *data)
{
	struct mtk_pe50 *pe50 = &chgmgr->pe5;
	struct prop_chgalgo_notify pca_notify;

	if (!pe50->pca_algo)
		return -EINVAL;
	switch (src) {
	case MTK_PE50_NOTISRC_TCP:
		pca_notify.src = PCA_NOTISRC_TCP;
		switch (event) {
		case MTK_PD_CONNECT_NONE:
			pca_notify.evt = PCA_NOTIEVT_DETACH;
			break;
		case MTK_PD_CONNECT_HARD_RESET:
			pca_notify.evt = PCA_NOTIEVT_HARDRESET;
			break;
		default:
			return -EINVAL;
		}
		break;
	case MTK_PE50_NOTISRC_CHG:
		pca_notify.src = PCA_NOTISRC_CHG;
		switch (event) {
		case CHARGER_DEV_NOTIFY_VBUS_OVP:
			chr_err("%s vbusovp\n", __func__);
			pca_notify.evt = PCA_NOTIEVT_VBUSOVP;
			break;
		case CHARGER_DEV_NOTIFY_IBUSOCP:
			chr_err("%s ibusocp\n", __func__);
			pca_notify.evt = PCA_NOTIEVT_IBUSOCP;
			break;
		case CHARGER_DEV_NOTIFY_IBUSUCP_FALL:
			chr_err("%s ibusucp fall\n", __func__);
			pca_notify.evt = PCA_NOTIEVT_IBUSUCP_FALL;
			break;
		case CHARGER_DEV_NOTIFY_BAT_OVP:
			chr_err("%s vbatovp\n", __func__);
			pca_notify.evt = PCA_NOTIEVT_VBATOVP;
			break;
		case CHARGER_DEV_NOTIFY_IBATOCP:
			chr_err("%s ibatocp\n", __func__);
			pca_notify.evt = PCA_NOTIEVT_IBATOCP;
			break;
		case CHARGER_DEV_NOTIFY_VBATOVP_ALARM:
			chr_err("%s vbatovp alarm\n", __func__);
			pca_notify.evt = PCA_NOTIEVT_VBATOVP_ALARM;
			break;
		case CHARGER_DEV_NOTIFY_VBUSOVP_ALARM:
			chr_err("%s vbusovp alarm\n", __func__);
			pca_notify.evt = PCA_NOTIEVT_VBUSOVP_ALARM;
			break;
		case CHARGER_DEV_NOTIFY_VOUTOVP:
			chr_err("%s voutovp\n", __func__);
			pca_notify.evt = PCA_NOTIEVT_VOUTOVP;
			break;
		case CHARGER_DEV_NOTIFY_VDROVP:
			chr_err("%s vdrovp\n", __func__);
			pca_notify.evt = PCA_NOTIEVT_VDROVP;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return prop_chgalgo_notifier_call(pe50->pca_algo, &pca_notify);
}

int mtk_pe50_thermal_throttling(struct charger_manager *chgmgr, int uA)
{
	struct mtk_pe50 *pe50 = &chgmgr->pe5;

	uA = (uA >= 0) ? uA / 1000 : -1;
	return prop_chgalgo_thermal_throttling(pe50->pca_algo, uA);
}

int mtk_pe50_set_jeita_vbat_cv(struct charger_manager *chgmgr, int uV)
{
	struct mtk_pe50 *pe50 = &chgmgr->pe5;

	uV = (uV >= 0) ? uV / 1000 : -1;
	return prop_chgalgo_set_jeita_vbat_cv(pe50->pca_algo, uV);
}

/*
 * rerun = 1, it allows pe50 to run again. Otherwise, it only run again after
 * plugging in/out happened.
 */
int mtk_pe50_stop_algo(struct charger_manager *chgmgr, bool rerun)
{
	int ret = 0;
	struct mtk_pe50 *pe50 = &chgmgr->pe5;

	if (chgmgr->enable_pe_5) {
		chr_err("%s: rerun:%d\n", __func__, rerun);
		ret = prop_chgalgo_stop_algo(pe50->pca_algo, rerun);
		pe50->is_enabled = false;
		pe50->online = false;
	}

	return ret;
}
#endif
