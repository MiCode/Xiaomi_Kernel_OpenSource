// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>


#include "mtk_charger_intf.h"

/* PD */
#include <tcpm.h>


struct mtk_pd_adapter_info {
	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	struct adapter_device *adapter_dev;
	struct task_struct *adapter_task;
	const char *adapter_dev_name;
	bool enable_kpoc_shdn;
};

static int pd_tcp_notifier_call(struct notifier_block *pnb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct mtk_pd_adapter_info *pinfo;

	pinfo = container_of(pnb, struct mtk_pd_adapter_info, pd_nb);

	chr_err("PD charger event:%d %d\n", (int)event,
		(int)noti->pd_state.connected);
	switch (event) {
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case  PD_CONNECT_NONE:
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_PD_CONNECT_NONE, NULL);
			break;

		case PD_CONNECT_HARD_RESET:
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_PD_CONNECT_HARD_RESET, NULL);
			break;

		case PD_CONNECT_PE_READY_SNK:
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_PD_CONNECT_PE_READY_SNK, NULL);
			break;

		case PD_CONNECT_PE_READY_SNK_PD30:
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_PD_CONNECT_PE_READY_SNK_PD30, NULL);
			break;

		case PD_CONNECT_PE_READY_SNK_APDO:
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_PD_CONNECT_PE_READY_SNK_APDO, NULL);
			break;

		case PD_CONNECT_TYPEC_ONLY_SNK:
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_PD_CONNECT_TYPEC_ONLY_SNK, NULL);
			break;
		};
		break;
	case TCP_NOTIFY_WD_STATUS:
		notify_adapter_event(MTK_PD_ADAPTER,
			MTK_TYPEC_WD_STATUS, &noti->wd_status.water_detected);
		break;
	case TCP_NOTIFY_HARD_RESET_STATE:
		if (noti->hreset_state.state == TCP_HRESET_RESULT_DONE ||
			noti->hreset_state.state == TCP_HRESET_RESULT_FAIL) {
			pinfo->enable_kpoc_shdn = true;
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_TYPEC_HRESET_STATUS,
				&pinfo->enable_kpoc_shdn);
		} else if (noti->hreset_state.state == TCP_HRESET_SIGNAL_SEND ||
			noti->hreset_state.state == TCP_HRESET_SIGNAL_RECV) {
			pinfo->enable_kpoc_shdn = false;
			notify_adapter_event(MTK_PD_ADAPTER,
				MTK_TYPEC_HRESET_STATUS,
				&pinfo->enable_kpoc_shdn);
		}
		break;
	}
	return NOTIFY_OK;
}

static int pd_get_property(struct adapter_device *dev,
	enum adapter_property sta)
{
	struct mtk_pd_adapter_info *info;

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL)
		return -1;

	switch (sta) {
	case TYPEC_RP_LEVEL:
		{
			return tcpm_inquire_typec_remote_rp_curr(info->tcpc);
		}
		break;
	default:
		{
		}
		break;
	}
	return -1;
}

static int pd_set_cap(struct adapter_device *dev, enum adapter_cap_type type,
		int mV, int mA)
{
	int ret = MTK_ADAPTER_OK;
	int tcpm_ret = TCPM_SUCCESS;
	struct mtk_pd_adapter_info *info;

	chr_err("[%s] type:%d mV:%d mA:%d\n",
		__func__, type, mV, mA);


	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL) {
		chr_err("[%s] info null\n", __func__);
		return -1;
	}

	if (type == MTK_PD_APDO_START) {
		tcpm_ret = tcpm_set_apdo_charging_policy(info->tcpc,
			DPM_CHARGING_POLICY_PPS, mV, mA, NULL);
	} else if (type == MTK_PD_APDO_END) {
		tcpm_ret = tcpm_set_pd_charging_policy(info->tcpc,
			DPM_CHARGING_POLICY_VSAFE5V, NULL);
	} else if (type == MTK_PD_APDO) {
		tcpm_ret = tcpm_dpm_pd_request(info->tcpc, mV, mA, NULL);
	} else if (type == MTK_PD) {
		tcpm_ret = tcpm_dpm_pd_request(info->tcpc, mV,
					mA, NULL);
	}

	chr_err("[%s] type:%d mV:%d mA:%d ret:%d\n",
		__func__, type, mV, mA, tcpm_ret);


	if (tcpm_ret == TCP_DPM_RET_REJECT)
		return MTK_ADAPTER_REJECT;
	else if (tcpm_ret != 0)
		return MTK_ADAPTER_ERROR;

	return ret;
}

int pd_get_output(struct adapter_device *dev, int *mV, int *mA)
{
	int ret = MTK_ADAPTER_OK;
	int tcpm_ret = TCPM_SUCCESS;
	struct pd_pps_status pps_status;
	struct mtk_pd_adapter_info *info;

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL)
		return MTK_ADAPTER_NOT_SUPPORT;


	tcpm_ret = tcpm_dpm_pd_get_pps_status(info->tcpc, NULL, &pps_status);
	if (tcpm_ret == TCP_DPM_RET_NOT_SUPPORT)
		return MTK_ADAPTER_NOT_SUPPORT;
	else if (tcpm_ret != 0)
		return MTK_ADAPTER_ERROR;

	*mV = pps_status.output_mv;
	*mA = pps_status.output_ma;

	return ret;
}

int pd_get_status(struct adapter_device *dev,
	struct adapter_status *sta)
{
	struct pd_status TAstatus = {0,};
	int ret = MTK_ADAPTER_OK;
	int tcpm_ret = TCPM_SUCCESS;
	struct mtk_pd_adapter_info *info;

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL)
		return MTK_ADAPTER_ERROR;

	tcpm_ret = tcpm_dpm_pd_get_status(info->tcpc, NULL, &TAstatus);

	sta->temperature = TAstatus.internal_temp;
	sta->ocp = TAstatus.event_flags & PD_STASUS_EVENT_OCP;
	sta->otp = TAstatus.event_flags & PD_STATUS_EVENT_OTP;
	sta->ovp = TAstatus.event_flags & PD_STATUS_EVENT_OVP;

	if (tcpm_ret == TCP_DPM_RET_NOT_SUPPORT)
		return MTK_ADAPTER_NOT_SUPPORT;
	else if (tcpm_ret == TCP_DPM_RET_TIMEOUT)
		return MTK_ADAPTER_TIMEOUT;
	else if (tcpm_ret == TCP_DPM_RET_SUCCESS)
		return MTK_ADAPTER_OK;
	else
		return MTK_ADAPTER_ERROR;

	return ret;

}

static int pd_get_cap(struct adapter_device *dev,
	enum adapter_cap_type type,
	struct adapter_power_cap *tacap)
{
	struct tcpm_power_cap_val apdo_cap;
	struct tcpm_remote_power_cap pd_cap;
	struct pd_source_cap_ext cap_ext;

	uint8_t cap_i = 0;
	int ret;
	int idx = 0;
	int i;
	struct mtk_pd_adapter_info *info;

	info = (struct mtk_pd_adapter_info *)adapter_dev_get_drvdata(dev);
	if (info == NULL || info->tcpc == NULL)
		return MTK_ADAPTER_ERROR;

	if (type == MTK_PD_APDO) {
		while (1) {
			ret = tcpm_inquire_pd_source_apdo(info->tcpc,
					TCPM_POWER_CAP_APDO_TYPE_PPS,
					&cap_i, &apdo_cap);
			if (ret == TCPM_ERROR_NOT_FOUND) {
				break;
			} else if (ret != TCPM_SUCCESS) {
				chr_err("[%s] tcpm_inquire_pd_source_apdo failed(%d)\n",
					__func__, ret);
				break;
			}

			ret = tcpm_dpm_pd_get_source_cap_ext(info->tcpc,
					NULL, &cap_ext);
			if (ret == TCPM_SUCCESS)
				tacap->pdp = cap_ext.source_pdp;
			else {
				tacap->pdp = 0;
				chr_err("[%s] tcpm_dpm_pd_get_source_cap_ext failed(%d)\n",
					__func__, ret);
			}

			tacap->pwr_limit[idx] = apdo_cap.pwr_limit;
			tacap->ma[idx] = apdo_cap.ma;
			tacap->max_mv[idx] = apdo_cap.max_mv;
			tacap->min_mv[idx] = apdo_cap.min_mv;
			tacap->maxwatt[idx] = apdo_cap.max_mv * apdo_cap.ma;
			tacap->minwatt[idx] = apdo_cap.min_mv * apdo_cap.ma;
			tacap->type[idx] = MTK_PD_APDO;

			idx++;
			chr_err("pps_boundary[%d], %d mv ~ %d mv, %d ma pl:%d\n",
				cap_i,
				apdo_cap.min_mv, apdo_cap.max_mv,
				apdo_cap.ma, apdo_cap.pwr_limit);
			if (idx >= ADAPTER_CAP_MAX_NR) {
				chr_err("CAP NR > %d\n", ADAPTER_CAP_MAX_NR);
				break;
			}
		}
		tacap->nr = idx;

		for (i = 0; i < tacap->nr; i++) {
			chr_err("pps_cap[%d:%d], %d mv ~ %d mv, %d ma pl:%d pdp:%d\n",
				i, (int)tacap->nr, tacap->min_mv[i],
				tacap->max_mv[i], tacap->ma[i],
				tacap->pwr_limit[i], tacap->pdp);
		}

		if (cap_i == 0)
			chr_err("no APDO for pps\n");

	} else if (type == MTK_PD) {
		pd_cap.nr = 0;
		pd_cap.selected_cap_idx = 0;
		tcpm_get_remote_power_cap(info->tcpc, &pd_cap);

		if (pd_cap.nr != 0) {
			tacap->nr = pd_cap.nr;
			tacap->selected_cap_idx = pd_cap.selected_cap_idx - 1;
			chr_err("[%s] nr:%d idx:%d\n",
			__func__, pd_cap.nr, pd_cap.selected_cap_idx - 1);
			for (i = 0; i < pd_cap.nr; i++) {
				tacap->ma[i] = pd_cap.ma[i];
				tacap->max_mv[i] = pd_cap.max_mv[i];
				tacap->min_mv[i] = pd_cap.min_mv[i];
				tacap->maxwatt[i] =
					tacap->max_mv[i] * tacap->ma[i];
				if (pd_cap.type[i] == 0)
					tacap->type[i] = MTK_PD;
				else if (pd_cap.type[i] == 3)
					tacap->type[i] = MTK_PD_APDO;
				else
					tacap->type[i] = MTK_CAP_TYPE_UNKNOWN;
				tacap->type[i] = pd_cap.type[i];

				chr_err("[%s]:%d mv:[%d,%d] %d max:%d min:%d type:%d %d\n",
					__func__, i, tacap->min_mv[i],
					tacap->max_mv[i], tacap->ma[i],
					tacap->maxwatt[i], tacap->minwatt[i],
					tacap->type[i], pd_cap.type[i]);
			}
		}
	}

	return MTK_ADAPTER_OK;
}

static struct adapter_ops adapter_ops = {
	.get_status = pd_get_status,
	.set_cap = pd_set_cap,
	.get_output = pd_get_output,
	.get_property = pd_get_property,
	.get_cap = pd_get_cap,
};

static int adapter_parse_dt(struct mtk_pd_adapter_info *info,
	struct device *dev)
{
	struct device_node *np = dev->of_node;

	chr_err("%s\n", __func__);

	if (!np) {
		chr_err("%s: no device node\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_string(np, "adapter_name",
		&info->adapter_dev_name) < 0)
		chr_err("%s: no adapter name\n", __func__);

	return 0;
}

static int mtk_pd_adapter_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mtk_pd_adapter_info *info = NULL;
	static bool is_deferred;

	chr_err("%s\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(struct mtk_pd_adapter_info),
			GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	adapter_parse_dt(info, &pdev->dev);

	info->adapter_dev = adapter_device_register(info->adapter_dev_name,
		&pdev->dev, info, &adapter_ops, NULL);
	if (IS_ERR_OR_NULL(info->adapter_dev)) {
		ret = PTR_ERR(info->adapter_dev);
		goto err_register_adapter_dev;
	}

	adapter_dev_set_drvdata(info->adapter_dev, info);

	info->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (info->tcpc == NULL) {
		if (is_deferred == false) {
			pr_info("%s: tcpc device not ready, defer\n", __func__);
			is_deferred = true;
			ret = -EPROBE_DEFER;
		} else {
			pr_info("%s: failed to get tcpc device\n", __func__);
			ret = -EINVAL;
		}
		goto err_get_tcpc_dev;
	}

	info->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(info->tcpc, &info->pd_nb,
				TCP_NOTIFY_TYPE_USB | TCP_NOTIFY_TYPE_MISC);
	if (ret < 0) {
		pr_info("%s: register tcpc notifer fail\n", __func__);
		ret = -EINVAL;
		goto err_get_tcpc_dev;
	}

	return 0;

err_get_tcpc_dev:
	adapter_device_unregister(info->adapter_dev);
err_register_adapter_dev:
	devm_kfree(&pdev->dev, info);

	return ret;
}

static int mtk_pd_adapter_remove(struct platform_device *dev)
{
	return 0;
}

static void mtk_pd_adapter_shutdown(struct platform_device *dev)
{
}

static const struct of_device_id mtk_pd_adapter_of_match[] = {
	{.compatible = "mediatek,pd_adapter",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_pd_adapter_of_match);


static struct platform_driver mtk_pd_adapter_driver = {
	.probe = mtk_pd_adapter_probe,
	.remove = mtk_pd_adapter_remove,
	.shutdown = mtk_pd_adapter_shutdown,
	.driver = {
		   .name = "pd_adapter",
		   .of_match_table = mtk_pd_adapter_of_match,
	},
};

static int __init mtk_pd_adapter_init(void)
{
	return platform_driver_register(&mtk_pd_adapter_driver);
}
module_init(mtk_pd_adapter_init);

static void __exit mtk_pd_adapter_exit(void)
{
	platform_driver_unregister(&mtk_pd_adapter_driver);
}
module_exit(mtk_pd_adapter_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK PD Adapter Driver");
MODULE_LICENSE("GPL");

