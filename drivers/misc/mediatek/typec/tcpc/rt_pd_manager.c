// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/cpumask.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/pm.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>

#ifdef CONFIG_MTK_BOOT
#include <mt-plat/mtk_boot.h>
#endif
#ifdef CONFIG_MTK_CHARGER
#include <charger_class.h>
#endif

#include "inc/tcpm.h"

#define RT_PD_MANAGER_VERSION	"1.0.7_MTK"

struct rt_pd_manager_data {
#ifdef CONFIG_MTK_CHARGER
	struct charger_device *chg_dev;
	struct power_supply *chg_psy;
#endif
	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	struct mutex param_lock;
	bool tcpc_kpoc;
	int sink_mv_new;
	int sink_ma_new;
	int sink_mv_old;
	int sink_ma_old;
};

enum {
	SINK_TYPE_REMOVE,
	SINK_TYPE_TYPEC,
	SINK_TYPE_PD_TRY,
	SINK_TYPE_PD_CONNECTED,
	SINK_TYPE_REQUEST,
};

void __attribute__((weak)) usb_dpdm_pulldown(bool enable)
{
	pr_notice("%s is not defined\n", __func__);
}

static int pd_tcp_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct rt_pd_manager_data *rpmd =
		(struct rt_pd_manager_data *)container_of(nb,
		struct rt_pd_manager_data, pd_nb);
#ifdef CONFIG_MTK_CHARGER
	union power_supply_propval propval;
#endif

	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
		mutex_lock(&rpmd->param_lock);
		rpmd->sink_mv_new = noti->vbus_state.mv;
		rpmd->sink_ma_new = noti->vbus_state.ma;
		pr_info("%s sink vbus %dmV %dmA type(0x%02X)\n", __func__,
			rpmd->sink_mv_new, rpmd->sink_ma_new,
			noti->vbus_state.type);
#ifdef CONFIG_MTK_CHARGER
		if ((rpmd->sink_mv_new != rpmd->sink_mv_old) ||
		    (rpmd->sink_ma_new != rpmd->sink_ma_old)) {
			rpmd->sink_mv_old = rpmd->sink_mv_new;
			rpmd->sink_ma_old = rpmd->sink_ma_new;
			if (rpmd->sink_mv_new && rpmd->sink_ma_new)
				charger_dev_enable_powerpath(rpmd->chg_dev,
							     true);
			else if (!rpmd->tcpc_kpoc)
				charger_dev_enable_powerpath(rpmd->chg_dev,
							     false);
		}
#endif
		mutex_unlock(&rpmd->param_lock);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_AUDIO) {
			/* AUDIO plug in */
			pr_info("%s audio plug in\n", __func__);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			/* AUDIO plug out */
			pr_info("%s audio plug out\n", __func__);
		}
		break;
	case TCP_NOTIFY_PD_STATE:
		pr_info("%s pd state = %d\n",
			__func__, noti->pd_state.connected);
		break;
	case TCP_NOTIFY_EXT_DISCHARGE:
		pr_info("%s ext discharge = %d\n", __func__, noti->en_state.en);
#ifdef CONFIG_MTK_CHARGER
		charger_dev_enable_discharge(rpmd->chg_dev,
					     noti->en_state.en);
#endif
		break;
	case TCP_NOTIFY_WD_STATUS:
		pr_info("%s wd status = %d\n",
			__func__, noti->wd_status.water_detected);

		if (noti->wd_status.water_detected) {
			usb_dpdm_pulldown(false);
			if (rpmd->tcpc_kpoc) {
				pr_info("Water is detected in KPOC, disable HV charging\n");
#ifdef CONFIG_MTK_CHARGER
			propval.intval = false;
			power_supply_set_property(rpmd->chg_psy,
				POWER_SUPPLY_PROP_VOLTAGE_MAX, &propval);
#endif
			}
		} else {
			usb_dpdm_pulldown(true);
			if (rpmd->tcpc_kpoc) {
				pr_info("Water is removed in KPOC, enable HV charging\n");
#ifdef CONFIG_MTK_CHARGER
			propval.intval = true;
			power_supply_set_property(rpmd->chg_psy,
				POWER_SUPPLY_PROP_VOLTAGE_MAX, &propval);
#endif
			}
		}
		break;
	case TCP_NOTIFY_CABLE_TYPE:
		pr_info("%s cable type = %d\n", __func__,
			noti->cable_type.type);
		break;
	case TCP_NOTIFY_PLUG_OUT:
		pr_info("%s typec plug out\n", __func__);
		if (rpmd->tcpc_kpoc) {
			pr_info("[%s] typec cable plug out, power off\n",
				__func__);
			kernel_power_off();
		}
		break;
	default:
		break;
	};
	return NOTIFY_OK;
}

static int rt_pd_manager_probe(struct platform_device *pdev)
{
	struct rt_pd_manager_data *rpmd;
	int ret = 0;

	pr_info("%s (%s)\n", __func__, RT_PD_MANAGER_VERSION);

	rpmd = devm_kzalloc(&pdev->dev, sizeof(*rpmd), GFP_KERNEL);
	if (!rpmd)
		return -ENOMEM;

	mutex_init(&rpmd->param_lock);
	rpmd->tcpc_kpoc = false;
	rpmd->sink_mv_old = -1;
	rpmd->sink_ma_old = -1;
	platform_set_drvdata(pdev, rpmd);

#ifdef CONFIG_MTK_BOOT
	ret = get_boot_mode();
	if (ret == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    ret == LOW_POWER_OFF_CHARGING_BOOT)
		rpmd->tcpc_kpoc = true;
	pr_info("%s KPOC is %d\n", __func__, rpmd->tcpc_kpoc);
#endif

#ifdef CONFIG_MTK_CHARGER
	/* get charger device */
	rpmd->chg_dev = get_charger_by_name("primary_chg");
	if (!rpmd->chg_dev) {
		pr_err("%s: get primary charger device failed\n", __func__);
		ret = -ENODEV;
		goto err_mutex;
	}

	rpmd->chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (IS_ERR(rpmd->chg_psy)) {
		dev_notice(&pdev->dev, "Failed to get charger psy\n");
		ret = PTR_ERR(rpmd->chg_psy);
		goto err_mutex;
	}
#endif

	rpmd->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!rpmd->tcpc) {
		pr_err("%s get tcpc device type_c_port0 fail\n", __func__);
		ret = -ENODEV;
		goto err_mutex;
	}
	rpmd->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(rpmd->tcpc, &rpmd->pd_nb,
					TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		pr_err("%s: register tcpc notifer fail\n", __func__);
		ret = -EINVAL;
		goto err_mutex;
	}

	pr_info("%s OK!!\n", __func__);
	return 0;
err_mutex:
	mutex_destroy(&rpmd->param_lock);
	return ret;
}

static const struct of_device_id rt_pd_manager_of_match[] = {
	{ .compatible = "mediatek,rt-pd-manager" },
	{ }
};
MODULE_DEVICE_TABLE(of, rt_pd_manager_of_match);

static struct platform_driver rt_pd_manager_driver = {
	.driver = {
		.name = "rt-pd-manager",
		.of_match_table = of_match_ptr(rt_pd_manager_of_match),
	},
	.probe = rt_pd_manager_probe,
};

static int __init rt_pd_manager_init(void)
{
	return platform_driver_register(&rt_pd_manager_driver);
}

static void __exit rt_pd_manager_exit(void)
{
	platform_driver_unregister(&rt_pd_manager_driver);
}

late_initcall(rt_pd_manager_init);
module_exit(rt_pd_manager_exit);

MODULE_AUTHOR("Jeff Chang");
MODULE_DESCRIPTION("Richtek pd manager driver");
MODULE_LICENSE("GPL");

/*
 * Release Note
 * 1.0.6
 * (1) enable power path in sink vbus
 *
 * 1.0.6
 * (1) refactor data struct and remove unuse part
 * (2) move bc12 relative to charger ic driver
 */
