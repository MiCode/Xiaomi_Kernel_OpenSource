// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

#include <tcpm.h>
#include "adapter_class.h"

struct info_notifier_block {
	struct notifier_block nb;
	struct mtk_pd_adapter_info *info;
};

struct mtk_pd_adapter_info {
	struct device *dev;
	int active_idx;
	struct mutex idx_lock;
	u32 nr_port;
	struct tcpc_device **tcpc;
	int *pd_type;
	struct info_notifier_block *pd_nb;
	bool force_cv;
	struct adapter_device *adapter;
};

static enum adapter_event pd_connect_tbl[] = {
	MTK_PD_CONNECT_NONE,
	MTK_PD_CONNECT_TYPEC_ONLY_SNK,
	MTK_PD_CONNECT_TYPEC_ONLY_SNK,
	MTK_PD_CONNECT_NONE,
	MTK_PD_CONNECT_PE_READY_SNK,
	MTK_PD_CONNECT_NONE,
	MTK_PD_CONNECT_PE_READY_SNK_PD30,
	MTK_PD_CONNECT_NONE,
	MTK_PD_CONNECT_PE_READY_SNK_APDO,
	MTK_PD_CONNECT_HARD_RESET,
	MTK_PD_CONNECT_SOFT_RESET,
	MTK_PD_CONNECT_TYPEC_ONLY_SNK,
	MTK_PD_CONNECT_TYPEC_ONLY_SNK,
};

struct apdo_pps_range {
	u32 prog_mv;
	u32 min_mv;
	u32 max_mv;
};

static struct apdo_pps_range apdo_pps_tbl[] = {
	{5000, 3300, 5900},	/* 5V Prog */
	{9000, 3300, 11000},	/* 9V Prog */
	{15000, 3300, 16000},	/* 15 VProg */
	{20000, 3300, 21000},	/* 20V Prog */
};

static inline int to_mtk_adapter_ret(int tcpm_ret)
{
	switch (tcpm_ret) {
	case TCP_DPM_RET_SUCCESS:
		return MTK_ADAPTER_OK;
	case TCP_DPM_RET_NOT_SUPPORT:
		return MTK_ADAPTER_NOT_SUPPORT;
	case TCP_DPM_RET_TIMEOUT:
		return MTK_ADAPTER_TIMEOUT;
	case TCP_DPM_RET_REJECT:
		return MTK_ADAPTER_REJECT;
	default:
		return MTK_ADAPTER_ERROR;
	}
}

static void find_active_idx_and_notification(struct mtk_pd_adapter_info *info,
					     int report_idx)
{
	int i = 0, active_idx = 0, pre_active_idx = info->active_idx;
	struct adapter_device *adapter = info->adapter;

	/* lower index has higher priority */
	for (i = 0; i < info->nr_port; i++) {
		if (info->pd_type[i] != MTK_PD_CONNECT_NONE) {
			active_idx = i;
			break;
		}
	}
	if (i >= info->nr_port)
		active_idx = pre_active_idx;

	if (active_idx != pre_active_idx) {
		if (info->pd_type[pre_active_idx] != MTK_PD_CONNECT_NONE)
			tcpm_reset_pd_charging_policy(
					info->tcpc[pre_active_idx], NULL);
		if (info->pd_type[pre_active_idx] != MTK_PD_CONNECT_NONE ||
		    report_idx == pre_active_idx)
			srcu_notifier_call_chain(&adapter->evt_nh,
						 MTK_PD_CONNECT_NONE,
						 &pre_active_idx);
		srcu_notifier_call_chain(&adapter->evt_nh,
					 info->pd_type[active_idx],
					 &active_idx);
	} else if (report_idx == active_idx) {
		srcu_notifier_call_chain(&adapter->evt_nh,
					 info->pd_type[active_idx],
					 &active_idx);
	}

	info->active_idx = active_idx;
}

static int pd_tcp_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct info_notifier_block *pd_nb =
		container_of(nb, struct info_notifier_block, nb);
	struct mtk_pd_adapter_info *info = pd_nb->info;
	struct adapter_device *adapter = info->adapter;
	int idx = pd_nb - info->pd_nb;
	uint8_t old_state = TYPEC_UNATTACHED, new_state = TYPEC_UNATTACHED;

	dev_info(info->dev, "%s event = %lu, idx = %d\n", __func__, event, idx);

	switch (event) {
	case TCP_NOTIFY_PD_STATE:
		dev_info(info->dev, "%s pd state = %d\n",
				    __func__, noti->pd_state.connected);
		mutex_lock(&info->idx_lock);
		if (noti->pd_state.connected >= ARRAY_SIZE(pd_connect_tbl))
			info->pd_type[idx] = MTK_PD_CONNECT_NONE;
		else
			info->pd_type[idx] =
				pd_connect_tbl[noti->pd_state.connected];
		find_active_idx_and_notification(info, idx);
		mutex_unlock(&info->idx_lock);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		old_state = noti->typec_state.old_state;
		new_state = noti->typec_state.new_state;

		if (old_state == TYPEC_UNATTACHED &&
		    (new_state == TYPEC_ATTACHED_NORP_SRC ||
		     new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		     new_state == TYPEC_ATTACHED_DBGACC_SNK)) {
			mutex_lock(&info->idx_lock);
			info->pd_type[idx] = MTK_PD_CONNECT_TYPEC_ONLY_SNK;
			find_active_idx_and_notification(info, idx);
			mutex_unlock(&info->idx_lock);
		} else if ((old_state == TYPEC_ATTACHED_NORP_SRC ||
			    old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			    old_state == TYPEC_ATTACHED_DBGACC_SNK) &&
			    new_state == TYPEC_UNATTACHED) {
			mutex_lock(&info->idx_lock);
			info->pd_type[idx] = MTK_PD_CONNECT_NONE;
			find_active_idx_and_notification(info, idx);
			mutex_unlock(&info->idx_lock);
		}
		break;
	case TCP_NOTIFY_WD_STATUS:
		srcu_notifier_call_chain(&adapter->evt_nh, MTK_TYPEC_WD_STATUS,
					 &noti->wd_status.water_detected);
		break;
	}

	return NOTIFY_OK;
}

static int pd_get_property(struct adapter_device *dev,
			   enum adapter_property pro)
{
	struct mtk_pd_adapter_info *info = adapter_dev_get_drvdata(dev);
	int ret = -EINVAL;

	if (info == NULL)
		return ret;

	switch (pro) {
	case TYPEC_RP_LEVEL:
		mutex_lock(&info->idx_lock);
		ret = tcpm_inquire_typec_remote_rp_curr(
				info->tcpc[info->active_idx]);
		mutex_unlock(&info->idx_lock);
		break;
	case PD_TYPE:
		mutex_lock(&info->idx_lock);
		ret = info->pd_type[info->active_idx];
		mutex_unlock(&info->idx_lock);
		break;
	default:
		break;
	}

	return ret;
}

static int pd_get_status(struct adapter_device *dev, struct adapter_status *sta)
{
	struct mtk_pd_adapter_info *info = adapter_dev_get_drvdata(dev);
	struct pd_status status = {0,};
	int ret = MTK_ADAPTER_ERROR, active_idx = 0;

	if (info == NULL)
		return ret;

	mutex_lock(&info->idx_lock);
	active_idx = info->active_idx;
	mutex_unlock(&info->idx_lock);

	ret = tcpm_dpm_pd_get_status(info->tcpc[active_idx], NULL, &status);
	sta->temperature = status.internal_temp;
	sta->ocp = !!(status.event_flags & PD_STATUS_EVENT_OCP);
	sta->otp = !!(status.event_flags & PD_STATUS_EVENT_OTP);
	sta->ovp = !!(status.event_flags & PD_STATUS_EVENT_OVP);

	return to_mtk_adapter_ret(ret);
}

static int pd_set_cap(struct adapter_device *dev, enum adapter_cap_type type,
		      int mV, int mA)
{
	struct mtk_pd_adapter_info *info = adapter_dev_get_drvdata(dev);
	int ret = MTK_ADAPTER_ERROR, active_idx = 0;

	if (info == NULL)
		return ret;

	mutex_lock(&info->idx_lock);
	active_idx = info->active_idx;
	mutex_unlock(&info->idx_lock);

	dev_info(info->dev, "%s type:%d %dmV %dmA\n", __func__, type, mV, mA);

	if (type == MTK_PD_APDO_START)
		ret = tcpm_set_apdo_charging_policy(info->tcpc[active_idx],
						    DPM_CHARGING_POLICY_PPS,
						    mV, mA, NULL);
	else {
		if (type == MTK_PD_APDO_END)
			tcpm_reset_pd_charging_policy(info->tcpc[active_idx],
						      NULL);
		ret = tcpm_dpm_pd_request(info->tcpc[active_idx], mV, mA, NULL);
	}

	return to_mtk_adapter_ret(ret);
}

static inline int pd_get_cap_apdo(struct mtk_pd_adapter_info *info,
				  int active_idx, struct adapter_power_cap *cap)
{
	struct tcpm_power_cap_val apdo_cap;
	uint8_t cap_idx = 0;
	int ret = 0, i = 0;

repeat:
	ret = tcpm_inquire_pd_source_apdo(info->tcpc[active_idx],
					  TCPM_POWER_CAP_APDO_TYPE_PPS,
					  &cap_idx, &apdo_cap);
	if (ret != TCPM_SUCCESS)
		goto out;

	/* If TA has PDP, we set pwr_limit as true */
	cap->pwr_limit[i] = apdo_cap.pwr_limit || cap->pdp ? 1 : 0;
	cap->max_mv[i] = apdo_cap.max_mv;
	cap->min_mv[i] = apdo_cap.min_mv;
	cap->ma[i] = apdo_cap.ma;
	cap->maxwatt[i] = apdo_cap.max_mv * apdo_cap.ma;
	cap->minwatt[i] = apdo_cap.min_mv * apdo_cap.ma;
	cap->type[i] = MTK_PD_APDO;

	dev_info(info->dev, "%s cap_idx[%d], %dmV ~ %dmV, %dmA pl:%d\n",
			    __func__, cap_idx, apdo_cap.min_mv,
			    apdo_cap.max_mv, apdo_cap.ma, apdo_cap.pwr_limit);
	if (++i >= ADAPTER_CAP_MAX_NR)
		goto out;
	goto repeat;
out:
	cap->nr = i;
	dev_notice(info->dev, "%s cap number = %d\n", __func__, cap->nr);
	for (i = 0; i < cap->nr; i++) {
		dev_info(info->dev,
			 "%s pps_cap[%d], %dmV ~ %dmV, %dmA pl:%d pdp:%d\n",
			 __func__, i, cap->min_mv[i], cap->max_mv[i],
			 cap->ma[i], cap->pwr_limit[i], cap->pdp);
	}

	return cap->nr ? MTK_ADAPTER_OK : MTK_ADAPTER_ERROR;
}

static inline int pd_get_cap_pdo(struct mtk_pd_adapter_info *info,
				 int active_idx, struct adapter_power_cap *cap)
{
	struct tcpm_remote_power_cap pd_cap;
	int ret = 0, i = 0, j = 0;

	ret = tcpm_get_remote_power_cap(info->tcpc[active_idx], &pd_cap);
	if (ret != TCPM_SUCCESS || pd_cap.nr == 0)
		return MTK_ADAPTER_ERROR;

	dev_info(info->dev, "%s nr:%d idx:%d\n",
			    __func__, pd_cap.nr, pd_cap.selected_cap_idx);
	cap->selected_cap_idx = pd_cap.selected_cap_idx - 1;
	for (i = 0, j = 0; i < pd_cap.nr && j < ADAPTER_CAP_MAX_NR; i++) {
		if (pd_cap.type[i] != TCPM_POWER_CAP_VAL_TYPE_FIXED)
			continue;

		cap->max_mv[j] = pd_cap.max_mv[i];
		cap->min_mv[j] = pd_cap.min_mv[i];
		cap->ma[j] = pd_cap.ma[i];
		cap->maxwatt[j] = pd_cap.max_mv[i] * pd_cap.ma[i];
		cap->minwatt[j] = pd_cap.min_mv[i] * pd_cap.ma[i];
		cap->type[j] = MTK_PD;
		j++;
	}
	cap->nr = j;
	dev_notice(info->dev, "%s cap number = %d\n", __func__, cap->nr);
	for (i = 0; i < cap->nr; i++) {
		dev_info(info->dev, "%s cap[%d], %dmV, %dmA, %duW\n",
				    __func__, i, cap->max_mv[i],
				    cap->ma[i], cap->maxwatt[i]);
	}

	return cap->nr ? MTK_ADAPTER_OK : MTK_ADAPTER_ERROR;
}

static int pd_get_cap(struct adapter_device *dev, enum adapter_cap_type type,
		      struct adapter_power_cap *cap)
{
	struct mtk_pd_adapter_info *info = adapter_dev_get_drvdata(dev);
	int ret = MTK_ADAPTER_ERROR, active_idx = 0;
	struct pd_source_cap_ext src_cap_ext;

	if (info == NULL)
		return ret;

	mutex_lock(&info->idx_lock);
	active_idx = info->active_idx;
	mutex_unlock(&info->idx_lock);

	memset(cap, 0, sizeof(*cap));

	ret = tcpm_dpm_pd_get_source_cap_ext(info->tcpc[active_idx],
					     NULL, &src_cap_ext);
	if (ret == TCP_DPM_RET_SUCCESS)
		cap->pdp = src_cap_ext.source_pdp;

	if (type == MTK_PD_APDO)
		ret = pd_get_cap_apdo(info, active_idx, cap);
	else if (type == MTK_PD)
		ret = pd_get_cap_pdo(info, active_idx, cap);
	else
		ret = MTK_ADAPTER_ERROR;

	return ret;
}

static int pd_get_output(struct adapter_device *dev, int *mV, int *mA)
{
	struct mtk_pd_adapter_info *info = adapter_dev_get_drvdata(dev);
	int ret = MTK_ADAPTER_ERROR, active_idx = 0;
	struct pd_pps_status pps_status;

	if (info == NULL)
		return ret;

	mutex_lock(&info->idx_lock);
	active_idx = info->active_idx;
	mutex_unlock(&info->idx_lock);

	ret = tcpm_dpm_pd_get_pps_status(info->tcpc[active_idx], NULL,
					 &pps_status);
	ret = to_mtk_adapter_ret(ret);
	if (ret != MTK_ADAPTER_OK)
		return ret;

	*mV = pps_status.output_mv;
	*mA = pps_status.output_ma;

	return ret;
}

#define PPS_STATUS_VTA_NOTSUPP	(-1)
#define PPS_STATUS_ITA_NOTSUPP	(-1)
static int pd_authentication(struct adapter_device *dev,
			     struct adapter_auth_data *data)
{
	struct mtk_pd_adapter_info *info = adapter_dev_get_drvdata(dev);
	int ret = MTK_ADAPTER_ERROR, active_idx = 0, i = 0;
	struct tcpm_power_cap_val apdo_cap, selected_apdo_cap;
	uint8_t cap_idx = 0, apdo_idx = 0;
	struct pd_source_cap_ext src_cap_ext;
	u32 prog_mv = 0;
	int vta_meas = 0, ita_meas = 0;
	struct adapter_status status;

	if (info == NULL)
		return ret;

	mutex_lock(&info->idx_lock);
	active_idx = info->active_idx;

	if (tcpm_inquire_typec_attach_state(info->tcpc[active_idx]) !=
			TYPEC_ATTACHED_SNK) {
		dev_notice(info->dev, "%s not Attached.SNK\n", __func__);
		mutex_unlock(&info->idx_lock);
		return ret;
	}

	if (info->pd_type[active_idx] != MTK_PD_CONNECT_PE_READY_SNK_APDO) {
		dev_notice(info->dev, "%s pd type is not snk apdo\n", __func__);
		mutex_unlock(&info->idx_lock);
		return ret;
	}
	mutex_unlock(&info->idx_lock);

	if (!tcpm_inquire_pd_pe_ready(info->tcpc[active_idx])) {
		dev_notice(info->dev, "%s pd pe not ready\n", __func__);
		return ret;
	}

repeat:
	ret = tcpm_inquire_pd_source_apdo(info->tcpc[active_idx],
					  TCPM_POWER_CAP_APDO_TYPE_PPS,
					  &cap_idx, &apdo_cap);
	if (ret != TCPM_SUCCESS) {
		if (apdo_idx == 0)
			dev_notice(info->dev, "%s inquire pd apdo fail(%d)\n",
					      __func__, ret);
		goto stop_repeat;
	}

	dev_info(info->dev, "%s cap_idx[%d], %dmV ~ %dmV, %dmA pl:%d\n",
			    __func__, cap_idx, apdo_cap.min_mv,
			    apdo_cap.max_mv, apdo_cap.ma, apdo_cap.pwr_limit);

	if (apdo_cap.min_mv > data->vcap_min ||
	    apdo_cap.max_mv < data->vcap_max ||
	    apdo_cap.ma < data->icap_min)
		goto repeat;
	if (apdo_idx == 0 || apdo_cap.ma > selected_apdo_cap.ma) {
		selected_apdo_cap = apdo_cap;
		apdo_idx = cap_idx;
		dev_info(info->dev, "%s select potential cap_idx[%d]\n",
				    __func__, cap_idx);
	}
	goto repeat;
stop_repeat:
	if (apdo_idx == 0) {
		dev_notice(info->dev, "%s no suitable apdo\n", __func__);
		return MTK_ADAPTER_ERROR;
	}

	data->vta_min = selected_apdo_cap.min_mv;
	data->vta_max = selected_apdo_cap.max_mv;
	data->ita_max = selected_apdo_cap.ma;
	data->pwr_lmt = !!selected_apdo_cap.pwr_limit;
	data->support_meas_cap = true;
	data->support_status = true;
	data->support_cc = true;
	data->vta_step = 20;
	data->ita_step = 50;
	data->ita_gap_per_vstep = 200;
	ret = tcpm_dpm_pd_get_source_cap_ext(info->tcpc[active_idx],
					     NULL, &src_cap_ext);
	if (ret == TCP_DPM_RET_SUCCESS) {
		data->pdp = src_cap_ext.source_pdp;
		if (data->pdp > 0 && !data->pwr_lmt)
			data->pwr_lmt = true;
	} else {
		dev_info(info->dev, "%s inquire pdp fail(%d)\n", __func__, ret);
		if (data->pwr_lmt) {
			for (i = 0; i < ARRAY_SIZE(apdo_pps_tbl); i++) {
				if (apdo_pps_tbl[i].max_mv < data->vta_max)
					continue;
				prog_mv = min_t(u32, apdo_pps_tbl[i].prog_mv,
						data->vta_max);
				data->pdp = prog_mv * data->ita_max / 1000000;
			}
		}
	}
	ret = pd_set_cap(dev, MTK_PD_APDO_START, 5000, 3000);
	if (ret != MTK_ADAPTER_OK)
		goto out;
	ret = pd_get_output(dev, &vta_meas, &ita_meas);
	if (ret != MTK_ADAPTER_OK &&
	    ret != MTK_ADAPTER_NOT_SUPPORT)
		goto out;
	if (ret == MTK_ADAPTER_NOT_SUPPORT ||
	    vta_meas == PPS_STATUS_VTA_NOTSUPP ||
	    ita_meas == PPS_STATUS_ITA_NOTSUPP) {
		data->support_meas_cap = false;
		data->support_cc = false;
		ret = MTK_ADAPTER_OK;
	}
	ret = pd_get_status(dev, &status);
	if (ret == MTK_ADAPTER_NOT_SUPPORT) {
		data->support_status = false;
		ret = MTK_ADAPTER_OK;
	} else if (ret != MTK_ADAPTER_OK)
		goto out;
	if (info->force_cv)
		data->support_cc = false;
	dev_info(info->dev, "%s select cap_idx[%d], power limit[%d,%dW]\n",
			    __func__, apdo_idx, data->pwr_lmt, data->pdp);
out:
	if (ret != MTK_ADAPTER_OK)
		dev_notice(info->dev, "%s fail(%d)\n", __func__, ret);
	return ret;
}

static int pd_is_cc(struct adapter_device *dev, bool *cc)
{
	struct mtk_pd_adapter_info *info = adapter_dev_get_drvdata(dev);
	int ret = MTK_ADAPTER_ERROR, active_idx = 0;
	struct pd_pps_status pps_status;

	if (info == NULL)
		return ret;

	mutex_lock(&info->idx_lock);
	active_idx = info->active_idx;
	mutex_unlock(&info->idx_lock);

	ret = tcpm_dpm_pd_get_pps_status(info->tcpc[active_idx], NULL,
					 &pps_status);
	if (ret == TCP_DPM_RET_SUCCESS)
		*cc = !!(pps_status.real_time_flags & PD_PPS_FLAGS_CFF);

	return to_mtk_adapter_ret(ret);
}

static int pd_set_wdt(struct adapter_device *dev, u32 ms)
{
	return MTK_ADAPTER_OK;
}

static int pd_enable_wdt(struct adapter_device *dev, bool en)
{
	return MTK_ADAPTER_OK;
}

static int pd_send_hardreset(struct adapter_device *dev)
{
	struct mtk_pd_adapter_info *info = adapter_dev_get_drvdata(dev);
	int ret = MTK_ADAPTER_ERROR, active_idx = 0;

	if (info == NULL)
		return ret;

	mutex_lock(&info->idx_lock);
	active_idx = info->active_idx;
	mutex_unlock(&info->idx_lock);

	ret = tcpm_dpm_pd_hard_reset(info->tcpc[active_idx], NULL);

	return to_mtk_adapter_ret(ret);
}

static struct adapter_ops adapter_ops = {
	.get_property = pd_get_property,
	.get_status = pd_get_status,
	.set_cap = pd_set_cap,
	.get_cap = pd_get_cap,
	.get_output = pd_get_output,
	.authentication = pd_authentication,
	.is_cc = pd_is_cc,
	.set_wdt = pd_set_wdt,
	.enable_wdt = pd_enable_wdt,
	.send_hardreset = pd_send_hardreset,
};

static void mtk_pd_adapter_remove_helper(struct mtk_pd_adapter_info *info)
{
	int i = 0, ret = 0;

	mutex_destroy(&info->idx_lock);
	for (i = 0; i < info->nr_port; i++) {
		if (!info->tcpc[i])
			return;
		ret = unregister_tcp_dev_notifier(info->tcpc[i],
						  &info->pd_nb[i].nb,
						  TCP_NOTIFY_TYPE_ALL);
		if (ret < 0)
			return;
	}
	if (IS_ERR(info->adapter))
		return;
	adapter_device_unregister(info->adapter);
}

#define INFO_DEVM_KCALLOC(member)					\
	(info->member = devm_kcalloc(info->dev, info->nr_port,		\
				     sizeof(*info->member), GFP_KERNEL))\

static int mtk_pd_adapter_probe(struct platform_device *pdev)
{
	struct mtk_pd_adapter_info *info = NULL;
	int ret = 0, i = 0;
	char name[16];
	struct device_node *np = pdev->dev.of_node;
	const char *adapter_name = NULL;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = &pdev->dev;
	info->active_idx = 0;
	mutex_init(&info->idx_lock);

	ret = of_property_read_u32(np, "nr_port", &info->nr_port);
	if (ret < 0) {
		dev_notice(info->dev, "%s read nr_port property fail(%d)\n",
				      __func__, ret);
		info->nr_port = 1;
	}
	INFO_DEVM_KCALLOC(tcpc);
	INFO_DEVM_KCALLOC(pd_type);
	INFO_DEVM_KCALLOC(pd_nb);
	if (!info->tcpc || !info->pd_type || !info->pd_nb) {
		ret = -ENOMEM;
		goto out;
	}
	platform_set_drvdata(pdev, info);

	for (i = 0; i < info->nr_port; i++) {
		ret = snprintf(name, sizeof(name), "type_c_port%d", i);
		if (ret >= sizeof(name))
			dev_notice(info->dev,
				   "%s type_c name is truncated\n", __func__);

		info->tcpc[i] = tcpc_dev_get_by_name(name);
		if (!info->tcpc[i]) {
			dev_notice(info->dev, "%s get %s fail\n",
					      __func__, name);
			ret = -ENODEV;
			goto out;
		}

		info->pd_nb[i].nb.notifier_call = pd_tcp_notifier_call;
		info->pd_nb[i].info = info;
		ret = register_tcp_dev_notifier(info->tcpc[i],
						&info->pd_nb[i].nb,
						TCP_NOTIFY_TYPE_ALL);
		if (ret < 0) {
			dev_notice(info->dev,
				   "%s register port%d notifier fail(%d)",
				   __func__, i, ret);
			goto out;
		}
	}

	info->force_cv = of_property_read_bool(np, "force_cv");

	ret = of_property_read_string(np, "adapter_name", &adapter_name);
	if (ret < 0) {
		dev_notice(info->dev,
			   "%s read adapter_name property fail(%d)\n",
			   __func__, ret);
		adapter_name = "pd_adapter";
	}
	info->adapter = adapter_device_register(adapter_name, info->dev, info,
						&adapter_ops, NULL);
	if (IS_ERR(info->adapter)) {
		ret = PTR_ERR(info->adapter);
		dev_notice(info->dev, "%s get %s fail(%d)\n",
				      __func__, adapter_name, ret);
		goto out;
	}
	adapter_dev_set_drvdata(info->adapter, info);

	dev_info(info->dev, "%s successfully\n", __func__);

	return 0;
out:
	mtk_pd_adapter_remove_helper(info);

	return ret;
}

static int mtk_pd_adapter_remove(struct platform_device *pdev)
{
	struct mtk_pd_adapter_info *info = platform_get_drvdata(pdev);

	mtk_pd_adapter_remove_helper(info);
	return 0;
}

static const struct of_device_id mtk_pd_adapter_of_match[] = {
	{.compatible = "mediatek,pd_adapter",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_pd_adapter_of_match);

static struct platform_driver mtk_pd_adapter_driver = {
	.probe = mtk_pd_adapter_probe,
	.remove = mtk_pd_adapter_remove,
	.driver = {
		   .name = "pd_adapter",
		   .of_match_table = mtk_pd_adapter_of_match,
	},
};
module_platform_driver(mtk_pd_adapter_driver);

MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK PD Adapter Driver");
MODULE_LICENSE("GPL");
