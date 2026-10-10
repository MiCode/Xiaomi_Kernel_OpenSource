

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>


#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#include "pd_dpm_prv.h"
#include "inc/tcpm.h"
#ifdef CONFIG_RECV_BAT_ABSENT_NOTIFY
#include "mtk_battery.h"
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY */
#endif /* CONFIG_USB_POWER_DELIVERY */

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#ifdef CONFIG_RECV_BAT_ABSENT_NOTIFY
static int fg_bat_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct pd_port *pd_port = container_of(nb, struct pd_port, fg_bat_nb);
	struct tcpc_device *tcpc = pd_port->tcpc;

	switch (event) {
	case EVENT_BATTERY_PLUG_OUT:
		dev_info(&tcpc->dev, "%s: fg battery absent\n", __func__);
		schedule_work(&pd_port->fg_bat_work);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY */
#endif /* CONFIG_USB_POWER_DELIVERY */

static int __tcpc_class_complete_work(struct device *dev, void *data)
{
	struct tcpc_device *tcpc = dev_get_drvdata(dev);
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#ifdef CONFIG_RECV_BAT_ABSENT_NOTIFY
	struct notifier_block *fg_bat_nb = &tcpc->pd_port.fg_bat_nb;
	int ret = 0;
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY */
#endif /* CONFIG_USB_POWER_DELIVERY */

	pr_err("__tcpc_class_complete_work\n");
	if (tcpc != NULL) {
		pr_err("%s = %s\n", __func__, dev_name(dev));
		tcpc_device_irq_enable(tcpc);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#ifdef CONFIG_RECV_BAT_ABSENT_NOTIFY
		fg_bat_nb->notifier_call = fg_bat_notifier_call;
#if 0 /* CONFIG_MTK_GAUGE_VERSION == 30 */
		ret = register_battery_notifier(fg_bat_nb);
#endif
		if (ret < 0) {
			pr_notice("%s: register bat notifier fail\n", __func__);
			return -EINVAL;
		}
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY */
#endif /* CONFIG_USB_POWER_DELIVERY */
	}
	return 0;
}

int tcpc_class_complete_init(void)
{
	pr_err("%s tcpc_class_complete_init\n", __func__);
	if (!IS_ERR(tcpc_class)) {
		class_for_each_device(tcpc_class, NULL, NULL,
			__tcpc_class_complete_work);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tcpc_class_complete_init);

MODULE_DESCRIPTION("Richtek TypeC Port Late Sync Driver");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION("1.0.0_MTK");
MODULE_LICENSE("GPL");
