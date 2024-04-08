
#include "inc/xm_battmngr_init.h"

struct xm_battmngr *g_battmngr;
EXPORT_SYMBOL(g_battmngr);

static int battmngr_notifier_call(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct battmngr_notify *noti_data = data;

	pr_err("%s: event %d\n", __func__, event);

	switch (event) {
	case BATTMNGR_EVENT_FG:
		battery_process_event_fg(noti_data);
		break;
	case BATTMNGR_EVENT_MAINCHG:
		charger_process_event_mainchg(noti_data);
		break;
	case BATTMNGR_EVENT_MCU:
		charger_process_event_mcu(noti_data);
		break;
	case BATTMNGR_EVENT_IRQ:
		charger_process_event_irq(noti_data);
		break;
	case BATTMNGR_EVENT_THERMAL:
		g_xm_charger->system_temp_level = noti_data->misc_msg.thermal_level;
		pr_err("%s: thermal level: %d\n", __func__, g_xm_charger->system_temp_level);
		xm_charger_thermal(g_xm_charger);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int xm_battmngr_probe(struct platform_device *pdev)
{
	struct xm_battmngr *battmngr;
	static int probe_cnt = 0;
	int rc;

	pr_err("%s: Start, probe_cnt = %d\n", __func__, ++probe_cnt);

	battmngr = devm_kzalloc(&pdev->dev, sizeof(*battmngr), GFP_KERNEL);
	if (!battmngr)
		return -ENOMEM;

	battmngr->dev = &pdev->dev;
	platform_set_drvdata(pdev, battmngr);

	if (!g_bcdev) {
		charger_err("%s: g_bcdev is null\n", __func__);
		rc = -EPROBE_DEFER;
		msleep(100);
		if (probe_cnt >= PROBE_CNT_MAX)
			goto out;
		else
			goto g_bcdev_failure;
	}

	battmngr->charger.dev = &pdev->dev;
	battmngr->charger.battmg_dev = NULL;
	g_xm_charger = &battmngr->charger;
	rc = xm_charger_init(&battmngr->charger);
	if (rc < 0) {
		charger_err("%s: xm_charger_init failure\n", __func__);
		return -EINVAL;
	}

	g_battmngr_noti = &battmngr->battmngr_noti;
	mutex_init(&g_battmngr_noti->notify_lock);

	battmngr->battmngr_nb.notifier_call = battmngr_notifier_call;
	rc = battmngr_notifier_register(&battmngr->battmngr_nb);
	if (rc < 0) {
		pr_err("%s: register battmngr notifier fail\n", __func__);
		return -EINVAL;
	}

	g_battmngr = battmngr;
	pr_err("%s: End\n", __func__);

out:
	platform_set_drvdata(pdev, battmngr);
	pr_err("%s %s !!\n", __func__,
		rc == -EPROBE_DEFER ? "Over probe cnt max" : "OK");
	return 0;

g_bcdev_failure:
	return rc;
}

static int xm_battmngr_remove(struct platform_device *pdev)
{
	struct xm_battmngr *battmngr = platform_get_drvdata(pdev);

	xm_charger_deinit(&battmngr->charger);
	battmngr_notifier_unregister(&battmngr->battmngr_nb);
	devm_kfree(&pdev->dev,battmngr);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void xm_battmngr_shutdown(struct platform_device *pdev)
{
	struct xm_battmngr *battmngr = platform_get_drvdata(pdev);

	battmngr_notifier_unregister(&battmngr->battmngr_nb);

	return;
}

static const struct of_device_id match_table[] = {
	{.compatible = "xiaomi,battmngr"},
	{},
};

static struct platform_driver xm_battmngr_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "xm_battmngr",
		.of_match_table = match_table,
	},
	.probe = xm_battmngr_probe,
	.remove = xm_battmngr_remove,
	.shutdown = xm_battmngr_shutdown,
};

static int __init xm_battmngr_init(void)
{
	pr_err("%s: cxlcheck1\n");
	return platform_driver_register(&xm_battmngr_driver);
}
module_init(xm_battmngr_init);

static void __exit xm_battmngr_exit(void)
{
	platform_driver_unregister(&xm_battmngr_driver);
}
module_exit(xm_battmngr_exit);

MODULE_DESCRIPTION("Xiaomi Battery Management");
MODULE_AUTHOR("yinshunan@xiaomi.com");
MODULE_LICENSE("GPL v2");
