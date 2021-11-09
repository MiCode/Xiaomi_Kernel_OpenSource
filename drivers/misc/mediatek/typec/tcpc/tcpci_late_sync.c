// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/slab.h>

#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#include "pd_dpm_prv.h"
#include "inc/tcpm.h"
#if CONFIG_RECV_BAT_ABSENT_NOTIFY && CONFIG_MTK_BATTERY
#include "mtk_battery.h"
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY && CONFIG_MTK_BATTERY */
#endif /* CONFIG_USB_POWER_DELIVERY */

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#if CONFIG_TCPC_NOTIFIER_LATE_SYNC
#if CONFIG_RECV_BAT_ABSENT_NOTIFY && CONFIG_MTK_BATTERY
static int fg_bat_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct pd_port *pd_port = container_of(nb, struct pd_port, fg_bat_nb);
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	switch (event) {
	case EVT_INT_BAT_PLUGOUT:
		dev_info(&tcpc_dev->dev, "%s: fg battery absent\n", __func__);
		schedule_work(&pd_port->fg_bat_work);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY && CONFIG_MTK_BATTERY */
#endif /* CONFIG_TCPC_NOTIFIER_LATE_SYNC */
#endif /* CONFIG_USB_POWER_DELIVERY */

#if CONFIG_TCPC_NOTIFIER_LATE_SYNC
static int __tcpc_class_complete_work(struct device *dev, void *data)
{
	struct tcpc_device *tcpc = dev_get_drvdata(dev);
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#if CONFIG_RECV_BAT_ABSENT_NOTIFY && CONFIG_MTK_BATTERY
	struct notifier_block *fg_bat_nb = &tcpc->pd_port.fg_bat_nb;
	int ret = 0;
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY && CONFIG_MTK_BATTERY */
#endif /* CONFIG_USB_POWER_DELIVERY */

	if (tcpc != NULL) {
		pr_info("%s = %s\n", __func__, dev_name(dev));
		tcpc_device_irq_enable(tcpc);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#if CONFIG_RECV_BAT_ABSENT_NOTIFY && CONFIG_MTK_BATTERY
		fg_bat_nb->notifier_call = fg_bat_notifier_call;
		ret = register_battery_notifier(fg_bat_nb);
		if (ret < 0) {
			pr_notice("%s: register bat notifier fail\n", __func__);
			return -EINVAL;
		}
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY && CONFIG_MTK_BATTERY */
#endif /* CONFIG_USB_POWER_DELIVERY */
	}
	return 0;
}

static int __init tcpc_class_complete_init(void)
{
	if (!IS_ERR(tcpc_class)) {
		class_for_each_device(tcpc_class, NULL, NULL,
			__tcpc_class_complete_work);
	}
	return 0;
}
late_initcall_sync(tcpc_class_complete_init);
#endif /* CONFIG_TCPC_NOTIFIER_LATE_SYNC */

MODULE_DESCRIPTION("Richtek TypeC Port Late Sync Driver");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION("1.0.0_MTK");
MODULE_LICENSE("GPL");
