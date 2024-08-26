// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
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
#include <linux/phy/phy.h>
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
#include "../hq_printk.h"
#ifdef TAG
#undef TAG
#define  TAG "[HQ_CHG][adapter]"
#endif

/* PD */
#include "../../../typec/tcpc/inc/tcpm.h"
#include "../charger_class/hq_adapter_class.h"

struct pd_adapter_info {
	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	struct adapter_dev *adapter_dev;
	struct device *dev;
	char *adapter_dev_name;
	struct adapter_cap pd_cap;
};
extern void * adapter_get_private(struct adapter_dev *adapter);
static inline int check_typec_attached_snk(struct tcpc_device *tcpc)
{
	if (tcpm_inquire_typec_attach_state(tcpc) != TYPEC_ATTACHED_SNK)
		return -EINVAL;
	return 0;
}

static int pd_tcp_notifier_call(struct notifier_block *pnb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct pd_adapter_info *pinfo;
	struct adapter_dev *adapter;

	pinfo = container_of(pnb, struct pd_adapter_info, pd_nb);
	adapter = pinfo->adapter_dev;

	hq_err("PD charger event:%d %d\n", (int)event, (int)noti->pd_state.connected);

	switch(event) {
		case TCP_NOTIFY_SOFT_RESET:
			hq_info("tcpc received soft reset.\n");
			adapter->soft_reset = true;
			break;
		default:
			break;
	}

	return 0;
}

static int pd_handshake(struct adapter_dev *dev)
{
	struct pd_adapter_info *info;
	int ret;

	info = (struct pd_adapter_info *)adapter_get_private(dev);

	if (info == NULL || info->tcpc == NULL) {
		hq_err("info null\n");
		return -EINVAL;
	}
	hq_info("---->\n");
	ret = tcpm_inquire_pd_pe_ready(info->tcpc);

	if(!ret) {
		ret = check_typec_attached_snk(info->tcpc);
		if (ret) {
			msleep(1000);
			ret = tcpm_inquire_pd_pe_ready(info->tcpc);
		}
	}
	return ret;
}

static int pd_get_cap(struct adapter_dev *dev, struct adapter_cap *cap)
{
	struct pd_adapter_info *info;
	struct tcpm_remote_power_cap pd_cap;
	int ret, i;

	info = (struct pd_adapter_info *)adapter_get_private(dev);

	if (info == NULL || info->tcpc == NULL) {
		hq_err("info null\n");
		return -EINVAL;
	}

	ret = tcpm_get_remote_power_cap(info->tcpc, &pd_cap);
	if(ret < 0)
		return ret;

	cap->cnt = pd_cap.nr;
	for (i = 0; i < pd_cap.nr; i++) {
		cap->volt_max[i] = pd_cap.max_mv[i];
		cap->volt_min[i] = pd_cap.min_mv[i];
		cap->curr_max[i] = pd_cap.ma[i];
		cap->curr_min[i] = 100;
		cap->type[i] = pd_cap.type[i];
		hq_info("V[%d-%d] I[100-%d]\n", cap->volt_min[i], cap->volt_max[i], cap->curr_max[i]);
	}

	info->pd_cap = *cap;

	return ret;
}

static int pd_set_cap(struct adapter_dev *dev, uint8_t nr,
											uint32_t mv, uint32_t ma)
{
	struct pd_adapter_info *info;
	struct tcpm_power_cap_val curr_cap;
	int ret;

	info = (struct pd_adapter_info *)adapter_get_private(dev);

	if (info == NULL || info->tcpc == NULL) {
		hq_err("info null\n");
		return -EINVAL;
	}
	hq_info("nr:%d mV:%d, mA:%d\n", nr, mv, ma);

	ret = tcpm_inquire_select_source_cap(info->tcpc, &curr_cap);
	if(ret < 0)
		return ret;

	if(info->pd_cap.type[nr] == TCPM_POWER_CAP_VAL_TYPE_AUGMENT
		&& curr_cap.type == TCPM_POWER_CAP_VAL_TYPE_FIXED) {
		ret = tcpm_set_apdo_charging_policy(info->tcpc,
			DPM_CHARGING_POLICY_PPS|DPM_CHARGING_POLICY_IGNORE_MISMATCH_CURR, mv, ma, NULL);
	} else if(info->pd_cap.type[nr] == TCPM_POWER_CAP_VAL_TYPE_FIXED
		&& curr_cap.type == TCPM_POWER_CAP_VAL_TYPE_AUGMENT) {
		ret = tcpm_set_pd_charging_policy(info->tcpc,
			DPM_CHARGING_POLICY_VSAFE5V, NULL);
	} else {
		ret = tcpm_dpm_pd_request(info->tcpc, mv, ma, NULL);
	}

	return ret;
}

static int pd_set_wdt(struct adapter_dev *dev, uint32_t ms)
{
	hq_info("---->\n");
	return 0;
}

static int pd_reset(struct adapter_dev *dev)
{
	struct pd_adapter_info *info;
	int ret;

	info = (struct pd_adapter_info *)adapter_get_private(dev);

	if (info == NULL || info->tcpc == NULL) {
		hq_err("info null\n");
		return -EINVAL;
	}
	hq_info("---->\n");
	ret = check_typec_attached_snk(info->tcpc);

	ret = tcpm_set_pd_charging_policy(info->tcpc,
			DPM_CHARGING_POLICY_VSAFE5V, NULL);

	return ret;
}

static int set_soft_reset_state(struct adapter_dev *dev, bool val)
{
	dev->soft_reset = !!val;
	hq_info("pd_soft_reset = %d\n", dev->soft_reset);

	return true;
}

static int get_soft_reset_state(struct adapter_dev *dev)
{
	return dev->soft_reset;
}

static struct adapter_ops adapter_ops = {
	.handshake = pd_handshake,
	.get_cap = pd_get_cap,
	.set_cap = pd_set_cap,
	.set_wdt = pd_set_wdt,
	.reset = pd_reset,
	.get_softreset = get_soft_reset_state,
	.set_softreset = set_soft_reset_state,
};

static int adapter_parse_dt(struct pd_adapter_info *info,
	struct device *dev)
{
	struct device_node *np = dev->of_node;
	hq_info("--->\n");

	if (of_property_read_string(np, "pd_adapter,name", (char const **)&info->adapter_dev_name) < 0) {
		hq_err("no charger name\n");
		return -ENOMEM;
	}
	return 0;
}

static int pd_adapter_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pd_adapter_info *info = NULL;
	static bool is_deferred;

	hq_info("start!\n");

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->dev = &pdev->dev;
	adapter_parse_dt(info, &pdev->dev);

	info->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (info->tcpc == NULL) {
		if (is_deferred == false) {
			hq_err("tcpc device not ready, defer\n");
			is_deferred = true;
			ret = -EPROBE_DEFER;
		} else {
			hq_err("failed to get tcpc device\n");
			ret = -EINVAL;
		}
		goto err_get_tcpc_dev;
	}

	info->adapter_dev = adapter_register(info->adapter_dev_name,
		&pdev->dev, &adapter_ops, info);
	if (IS_ERR_OR_NULL(info->adapter_dev)) {
		ret = PTR_ERR(info->adapter_dev);
		goto err_register_adapter_dev;
	}

	info->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(info->tcpc, &info->pd_nb,
				TCP_NOTIFY_TYPE_USB | TCP_NOTIFY_TYPE_MISC |
				TCP_NOTIFY_TYPE_VBUS);
	if (ret < 0) {
		hq_err("register tcpc notifer fail\n");
		ret = -EINVAL;
		goto err_get_tcpc_dev;
	}
	hq_info("successfully!\n");
	return 0;

err_get_tcpc_dev:
err_register_adapter_dev:
	devm_kfree(&pdev->dev, info);

	return ret;
}

static int pd_adapter_remove(struct platform_device *pdev)
{
	struct pd_adapter_info *info = platform_get_drvdata(pdev);
	adapter_unregister(info->adapter_dev);
	devm_kfree(&pdev->dev, info);
	return 0;
}

static void pd_adapter_shutdown(struct platform_device *dev)
{
}

static const struct of_device_id pd_adapter_of_match[] = {
	{.compatible = "huaqin,pd_adapter",},
	{},
};

MODULE_DEVICE_TABLE(of, pd_adapter_of_match);


static struct platform_driver pd_adapter_driver = {
	.probe = pd_adapter_probe,
	.remove = pd_adapter_remove,
	.shutdown = pd_adapter_shutdown,
	.driver = {
			.name = "huaqin,pd_adapter",
			.of_match_table = pd_adapter_of_match,
	},
};

static int __init pd_adapter_init(void)
{
	return platform_driver_register(&pd_adapter_driver);
}
module_init(pd_adapter_init);

static void __exit pd_adapter_exit(void)
{
	platform_driver_unregister(&pd_adapter_driver);
}
module_exit(pd_adapter_exit);


MODULE_DESCRIPTION("Huaqin PD Adapter Class");
MODULE_LICENSE("GPL v2");
/*
 * HQ pd adapter Release Note
 * 1.0.0
 * (1)  xxxxxx
 */
