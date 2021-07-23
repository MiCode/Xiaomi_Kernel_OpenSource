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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/of.h>

#include <tcpm.h>
#include <mt-plat/prop_chgalgo_class.h>

#define PCA_PPS_TA_VERSION	"1.0.6_G"
#define PCA_PPS_CMD_RETRY_COUNT	2

#ifndef MIN
#define MIN(A, B) (((A) < (B)) ? (A) : (B))
#endif

struct pca_pps_desc {
	u32 ita_min;
	bool force_cv;
};

static struct pca_pps_desc pca_pps_desc_defval = {
	.ita_min = 0,
	.force_cv = false,
};

struct pca_pps_info {
	struct device *dev;
	struct pca_pps_desc *desc;
	struct prop_chgalgo_device *pca;
	struct tcpc_device *tcpc;
	struct notifier_block tcp_nb;
	bool is_pps_en_unlock;
	int hrst_cnt;
};

struct apdo_pps_range {
	u32 prog_mv;
	u32 min_mv;
	u32 max_mv;
};

static struct apdo_pps_range apdo_pps_tbl[] = {
	{5000, 3300, 5900},	/* 5VProg */
	{9000, 3300, 11000},	/* 9VProg */
	{15000, 3300, 16000},	/* 15VProg */
	{20000, 3300, 21000},	/* 20VProg */
};

static inline int check_typec_attached_snk(struct tcpc_device *tcpc)
{
	if (tcpm_inquire_typec_attach_state(tcpc) != TYPEC_ATTACHED_SNK)
		return -EINVAL;
	return 0;
}

static int pca_pps_enable_charging(struct prop_chgalgo_device *pca, bool en,
				   u32 mV, u32 mA)
{
	struct pca_pps_info *info = prop_chgalgo_get_drvdata(pca);
	int ret, cnt = 0;

	if (check_typec_attached_snk(info->tcpc) < 0)
		return -EINVAL;
	PCA_DBG("en = %d, %dmV, %dmA\n", en, mV, mA);

	do {
		if (en)
			ret = tcpm_set_apdo_charging_policy(info->tcpc,
				DPM_CHARGING_POLICY_PPS, mV, mA, NULL);
		else
			ret = tcpm_reset_pd_charging_policy(info->tcpc, NULL);
		cnt++;
	} while (ret != TCP_DPM_RET_SUCCESS && cnt < PCA_PPS_CMD_RETRY_COUNT);

	if (ret != TCP_DPM_RET_SUCCESS)
		PCA_ERR("fail(%d)\n", ret);
	return ret > 0 ? -ret : ret;
}

static int pca_pps_set_cap(struct prop_chgalgo_device *pca, u32 mV, u32 mA)
{
	struct pca_pps_info *info = prop_chgalgo_get_drvdata(pca);
	int ret, cnt = 0;

	if (check_typec_attached_snk(info->tcpc) < 0)
		return -EINVAL;
	PCA_DBG("%dmV, %dmA\n", mV, mA);

	if (!tcpm_inquire_pd_connected(info->tcpc)) {
		PCA_ERR("pd not connected\n");
		return -EINVAL;
	}

	do {
		ret = tcpm_dpm_pd_request(info->tcpc, mV, mA, NULL);
		cnt++;
	} while (ret != TCP_DPM_RET_SUCCESS && cnt < PCA_PPS_CMD_RETRY_COUNT);

	if (ret != TCP_DPM_RET_SUCCESS)
		PCA_ERR("fail(%d)\n", ret);
	return ret > 0 ? -ret : ret;
}

#define PPS_STATUS_VTA_NOTSUPP	(0xFFFF)
#define PPS_STATUS_ITA_NOTSUPP	(0xFF)
static int pca_pps_get_measure_cap(struct prop_chgalgo_device *pca, u32 *mV,
				   u32 *mA)
{
	struct pca_pps_info *info = prop_chgalgo_get_drvdata(pca);
	int ret, cnt = 0;
	struct pd_pps_status pps_status;

	if (check_typec_attached_snk(info->tcpc) < 0)
		return -EINVAL;

	do {
		ret = tcpm_dpm_pd_get_pps_status(info->tcpc, NULL, &pps_status);
		cnt++;
	} while (ret != TCP_DPM_RET_SUCCESS && cnt < PCA_PPS_CMD_RETRY_COUNT);

	if (ret == TCP_DPM_RET_SUCCESS) {
		if (pps_status.output_mv == -1 || pps_status.output_ma == -1) {
			*mV = PPS_STATUS_VTA_NOTSUPP;
			*mA = PPS_STATUS_ITA_NOTSUPP;
			PCA_INFO("NOT SUPPORT\n");
		} else {
			*mV = pps_status.output_mv;
			*mA = pps_status.output_ma;
			PCA_DBG("%dmV, %dmA\n", *mV, *mA);
		}
	} else
		PCA_ERR("fail(%d)\n", ret);

	return ret > 0 ? -ret : ret;
}

static int pca_pps_get_temperature(struct prop_chgalgo_device *pca, int *temp)
{
	struct pca_pps_info *info = prop_chgalgo_get_drvdata(pca);
	int ret, cnt = 0;
	struct pd_status status;

	if (check_typec_attached_snk(info->tcpc) < 0)
		return -EINVAL;

	do {
		ret = tcpm_dpm_pd_get_status(info->tcpc, NULL, &status);
		cnt++;
	} while (ret != TCP_DPM_RET_SUCCESS && cnt < PCA_PPS_CMD_RETRY_COUNT);

	if (ret == TCP_DPM_RET_SUCCESS) {
		/* 0 means NOT SUPPORT */
		*temp = (status.internal_temp == 0) ? 25 : status.internal_temp;
		PCA_DBG("%d degree\n", *temp);
	} else
		PCA_ERR("fail(%d)\n", ret);

	return ret > 0 ? -ret : ret;
}

static int pca_pps_get_status(struct prop_chgalgo_device *pca,
			      struct prop_chgalgo_ta_status *ta_status)
{
	struct pca_pps_info *info = prop_chgalgo_get_drvdata(pca);
	int ret, cnt = 0;
	struct pd_status status;

	if (check_typec_attached_snk(info->tcpc) < 0)
		return -EINVAL;

	do {
		ret = tcpm_dpm_pd_get_status(info->tcpc, NULL, &status);
		cnt++;
	} while (ret != TCP_DPM_RET_SUCCESS && cnt < PCA_PPS_CMD_RETRY_COUNT);

	if (ret == TCP_DPM_RET_SUCCESS) {
		ta_status->temp1 = status.internal_temp;
		ta_status->present_input = status.present_input;
		ta_status->present_battery_input = status.present_battey_input;
		ta_status->ocp = !!(status.event_flags & PD_STASUS_EVENT_OCP);
		ta_status->otp = !!(status.event_flags & PD_STATUS_EVENT_OTP);
		ta_status->ovp = !!(status.event_flags & PD_STATUS_EVENT_OVP);
		ta_status->temp_level = PD_STATUS_TEMP_PTF(status.temp_status);
	} else
		PCA_ERR("fail(%d)\n", ret);
	return ret > 0 ? -ret : ret;
}

static int pca_pps_is_cc(struct prop_chgalgo_device *pca, bool *cc)
{
	struct pca_pps_info *info = prop_chgalgo_get_drvdata(pca);
	int ret, cnt = 0;
	struct pd_pps_status pps_status;

	do {
		ret = tcpm_dpm_pd_get_pps_status(info->tcpc, NULL, &pps_status);
		cnt++;
	} while (ret != TCP_DPM_RET_SUCCESS && cnt < PCA_PPS_CMD_RETRY_COUNT);

	if (ret == TCP_DPM_RET_SUCCESS)
		*cc = !!(pps_status.real_time_flags & PD_PPS_FLAGS_CFF);
	else
		PCA_ERR("fail(%d)\n", ret);
	return ret > 0 ? -ret : ret;
}

static int pca_pps_send_hardreset(struct prop_chgalgo_device *pca)
{
	struct pca_pps_info *info = prop_chgalgo_get_drvdata(pca);
	int ret, cnt = 0;

	if (check_typec_attached_snk(info->tcpc) < 0)
		return -EINVAL;

	PCA_INFO("++\n");
	do {
		ret = tcpm_dpm_pd_hard_reset(info->tcpc, NULL);
		cnt++;
	} while (ret != TCP_DPM_RET_SUCCESS && cnt < PCA_PPS_CMD_RETRY_COUNT);
	if (ret != TCP_DPM_RET_SUCCESS)
		PCA_ERR("fail(%d)\n", ret);

	return ret > 0 ? -ret : ret;
}

static int pca_pps_authenticate_ta(struct prop_chgalgo_device *pca,
				   struct prop_chgalgo_ta_auth_data *data)
{
	int ret, apdo_idx = -1, i;
	struct pca_pps_info *info = prop_chgalgo_get_drvdata(pca);
	struct tcpm_power_cap_val apdo_cap;
	struct tcpm_power_cap_val selected_apdo_cap;
	struct pd_source_cap_ext src_cap_ext;
	struct prop_chgalgo_ta_status ta_status;
	u8 cap_idx;
	u32 vta_meas, ita_meas, prog_mv;
	int apdo_pps_cnt = ARRAY_SIZE(apdo_pps_tbl);

	PCA_DBG("++\n");
	if (check_typec_attached_snk(info->tcpc) < 0)
		return -EINVAL;

	if (!info->is_pps_en_unlock) {
		PCA_ERR("pps en is locked\n");
		return -EINVAL;
	}

	if (!tcpm_inquire_pd_pe_ready(info->tcpc)) {
		PCA_ERR("PD PE not ready\n");
		return -EINVAL;
	}

	/* select TA boundary */
	cap_idx = 0;
	while (1) {
		ret = tcpm_inquire_pd_source_apdo(info->tcpc,
						  TCPM_POWER_CAP_APDO_TYPE_PPS,
						  &cap_idx, &apdo_cap);
		if (ret != TCP_DPM_RET_SUCCESS) {
			PCA_ERR("inquire pd apdo fail(%d)\n", ret);
			break;
		}

		PCA_INFO("cap_idx[%d], %d mv ~ %d mv, %d ma, pl: %d\n", cap_idx,
			 apdo_cap.min_mv, apdo_cap.max_mv, apdo_cap.ma,
			 apdo_cap.pwr_limit);

		/*
		 * !(apdo_cap.min_mv <= data->vcap_min &&
		 *   apdo_cap.max_mv >= data->vcap_max &&
		 *   apdo_cap.ma >= data->icap_min)
		 */
		if (apdo_cap.min_mv > data->vcap_min ||
		    apdo_cap.max_mv < data->vcap_max ||
		    apdo_cap.ma < data->icap_min)
			continue;
		if (apdo_idx == -1 || apdo_cap.ma > selected_apdo_cap.ma) {
			memcpy(&selected_apdo_cap, &apdo_cap,
			       sizeof(struct tcpm_power_cap_val));
			apdo_idx = cap_idx;
			PCA_INFO("select potential cap_idx[%d]\n", cap_idx);
		}
	}
	if (apdo_idx != -1) {
		data->vta_min = selected_apdo_cap.min_mv;
		data->vta_max = selected_apdo_cap.max_mv;
		data->ita_max = selected_apdo_cap.ma;
		data->ita_min = info->desc->ita_min;
		data->pwr_lmt = selected_apdo_cap.pwr_limit;
		data->support_cc = true;
		data->support_meas_cap = true;
		data->support_status = true;
		data->vta_step = 20;
		data->ita_step = 50;
		data->ita_gap_per_vstep = 200;
		ret = tcpm_dpm_pd_get_source_cap_ext(info->tcpc, NULL,
						     &src_cap_ext);
		if (ret != TCP_DPM_RET_SUCCESS) {
			PCA_ERR("inquire pdp fail(%d)\n", ret);
			if (data->pwr_lmt) {
				for (i = 0; i < apdo_pps_cnt; i++) {
					if (apdo_pps_tbl[i].max_mv <
					    data->vta_max)
						continue;
					prog_mv = MIN(apdo_pps_tbl[i].prog_mv,
						      data->vta_max);
					data->pdp = prog_mv * data->ita_max /
						    1000000;
				}
			}
		} else {
			data->pdp = src_cap_ext.source_pdp;
			if (data->pdp > 0 && !data->pwr_lmt)
				data->pwr_lmt = true;
		}
		/* Check whether TA supports getting pps status */
		ret = pca_pps_enable_charging(pca, true, 5000, 3000);
		if (ret != TCP_DPM_RET_SUCCESS)
			goto out;
		ret = pca_pps_get_measure_cap(pca, &vta_meas, &ita_meas);
		if (ret != TCP_DPM_RET_SUCCESS &&
		    ret != -TCP_DPM_RET_NOT_SUPPORT)
			goto out;
		if (ret == -TCP_DPM_RET_NOT_SUPPORT ||
		    vta_meas == PPS_STATUS_VTA_NOTSUPP ||
		    ita_meas == PPS_STATUS_ITA_NOTSUPP) {
			data->support_cc = false;
			data->support_meas_cap = false;
			ret = TCP_DPM_RET_SUCCESS;
		}
		ret = pca_pps_get_status(pca, &ta_status);
		if (ret == -TCP_DPM_RET_NOT_SUPPORT) {
			data->support_status = false;
			ret = TCP_DPM_RET_SUCCESS;
		} else if (ret != TCP_DPM_RET_SUCCESS)
			goto out;
		if (info->desc->force_cv)
			data->support_cc = false;
		PCA_INFO("select cap_idx[%d], power limit[%d,%dW]\n",
			 cap_idx, data->pwr_lmt, data->pdp);
	} else {
		PCA_ERR("cannot find apdo for pps algo\n");
		return -EINVAL;
	}
out:
	if (ret != TCP_DPM_RET_SUCCESS)
		PCA_ERR("fail(%d)\n", ret);
	return ret > 0 ? -ret : ret;
}

static int pca_pps_enable_wdt(struct prop_chgalgo_device *pca, bool en)
{
	return 0;
}

static int pca_pps_set_wdt(struct prop_chgalgo_device *pca, u32 ms)
{
	return 0;
}

static struct prop_chgalgo_desc pca_ta_pps_desc = {
	.name = "pca_ta_pps",
	.type = PCA_DEVTYPE_TA,
};

static struct prop_chgalgo_ta_ops pca_pps_ops = {
	.enable_charging = pca_pps_enable_charging,
	.set_cap = pca_pps_set_cap,
	.get_measure_cap = pca_pps_get_measure_cap,
	.get_temperature = pca_pps_get_temperature,
	.get_status = pca_pps_get_status,
	.is_cc = pca_pps_is_cc,
	.send_hardreset = pca_pps_send_hardreset,
	.authenticate_ta = pca_pps_authenticate_ta,
	.enable_wdt = pca_pps_enable_wdt,
	.set_wdt = pca_pps_set_wdt,
};

static int pca_pps_tcp_notifier_call(struct notifier_block *nb,
				     unsigned long event, void *data)
{
	struct pca_pps_info *info =
		container_of(nb, struct pca_pps_info, tcp_nb);
	struct tcp_notify *noti = data;

	switch (event) {
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			dev_info(info->dev, "detached\n");
			info->is_pps_en_unlock = false;
			info->hrst_cnt = 0;
			break;
		case PD_CONNECT_HARD_RESET:
			info->hrst_cnt++;
			dev_info(info->dev, "pd hardreset, cnt = %d\n",
				 info->hrst_cnt);
			info->is_pps_en_unlock = false;
			break;
		case PD_CONNECT_PE_READY_SNK_APDO:
			if (info->hrst_cnt < 5) {
				dev_info(info->dev, "%s en unlock\n", __func__);
				info->is_pps_en_unlock = true;
			}
			break;
		default:
			break;
		}
	default:
		break;
	}
	return NOTIFY_OK;
}

static int pca_pps_parse_dt(struct pca_pps_info *info)
{
	struct device_node *np = info->dev->of_node;
	struct pca_pps_desc *desc;

	desc = devm_kzalloc(info->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	info->desc = desc;
	memcpy(desc, &pca_pps_desc_defval, sizeof(*desc));
	desc->force_cv = of_property_read_bool(np, "force_cv");
	of_property_read_u32(np, "ita_min", &desc->ita_min);
	return 0;
}

static int pca_pps_probe(struct platform_device *pdev)
{
	int ret;
	struct pca_pps_info *info;

	dev_info(&pdev->dev, "%s\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->dev = &pdev->dev;
	platform_set_drvdata(pdev, info);

	ret = pca_pps_parse_dt(info);
	if (ret < 0) {
		dev_notice(info->dev, "%s parse dt fail\n", __func__);
		return ret;
	}

	info->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!info->tcpc) {
		dev_notice(info->dev, "%s get tcpc dev fail\n", __func__);
		return -ENODEV;
	}

	/* register tcp notifier callback */
	info->tcp_nb.notifier_call = pca_pps_tcp_notifier_call;
	ret = register_tcp_dev_notifier(info->tcpc, &info->tcp_nb,
					TCP_NOTIFY_TYPE_USB);
	if (ret < 0) {
		dev_notice(info->dev, "register tcpc notifier fail\n");
		return ret;
	}

	info->pca = prop_chgalgo_device_register(info->dev, &pca_ta_pps_desc,
						 &pca_pps_ops, NULL, NULL,
						 info);
	if (!info->pca) {
		dev_notice(info->dev, "%s register pps fail\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	dev_info(info->dev, "%s successfully\n", __func__);
	return 0;
err:
	unregister_tcp_dev_notifier(info->tcpc, &info->tcp_nb,
				    TCP_NOTIFY_TYPE_USB);
	return ret;
}

static int pca_pps_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id pca_pps_of_id[] = {
	{ .compatible = "richtek,pca_pps_ta" },
	{},
};
MODULE_DEVICE_TABLE(of, pca_pps_of_id);

static struct platform_driver pca_pps_platdrv = {
	.driver = {
		.name = "pca_pps_ta",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pca_pps_of_id),
	},
	.probe = pca_pps_probe,
	.remove = pca_pps_remove,
};

static int __init pca_pps_init(void)
{
	return platform_driver_register(&pca_pps_platdrv);
}
device_initcall_sync(pca_pps_init);
// module_init(pca_pps_init);

static void __exit pca_pps_exit(void)
{
	platform_driver_unregister(&pca_pps_platdrv);
}
module_exit(pca_pps_exit);

MODULE_DESCRIPTION("Programmable Power Supply TA For PCA");
MODULE_AUTHOR("ShuFan Lee<shufan_lee@richtek.com>");
MODULE_VERSION(PCA_PPS_TA_VERSION);
MODULE_LICENSE("GPL");

/*
 * 1.0.7_G
 * (1) Add ita_min option in dtsi to support TA which only accepts
 *     request with current greater than certain value
 *
 * 1.0.6_G
 * (1) Move platform device to dtsi
 * (2) Add force_cv option in dtsi
 *
 * 1.0.5_G
 * (1) Select PPS cap whose voltage range is valid and with maximum current cap
 *
 * 1.0.4_G
 * (1) Implement is_cc ops and move cc from get_status to is_cc
 * (2) Add ita_gap_per_vstep in authentication data
 * (3) Handle error code of TCP_DPM_XXX and TCPM_XXX, but always convert postive
 *     error code to negative
 * (4) Always read PDP to decide whether power limit should be true
 * (5) Change name of ta_req_vstep/ta_req_istep/cap_vmax/cap_vmin/cap_imin to
 *     vta_step/ita_step/vcap_max/vcap_min/icap_min
 *
 * 1.0.3_G
 * (1) Supports TA that does not support get_pps_status
 * (2) Using MIN(vprog, ta_vmax) * ta_imax as TA's PDP if TA claims that it
 * is power limited but not supporting source cap ext message
 *
 * 1.0.2_G
 * (1) It means not support if temperature in status message is 0
 *
 * 1.0.1_G
 * (1) Get PDP if power limit is specified in APDO
 *
 * 1.0.0_G
 * Initial release
 */
