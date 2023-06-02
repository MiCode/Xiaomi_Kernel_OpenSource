
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
	case BATTMNGR_EVENT_CP:
		charger_process_event_cp(noti_data);
		break;
	case BATTMNGR_EVENT_MAINCHG:
		charger_process_event_mainchg(noti_data);
		break;
	case BATTMNGR_EVENT_PD:
		charger_process_event_pd(noti_data);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int xm_battmngr_probe(struct platform_device *pdev)
{
	struct xm_battmngr *battmngr;
	int rc;

	pr_err("%s: Start\n", __func__);

	battmngr = devm_kzalloc(&pdev->dev, sizeof(*battmngr), GFP_KERNEL);
		if (!battmngr)
			return -ENOMEM;

	battmngr->dev = &pdev->dev;
	battmngr->battmngr_iio.dev = &pdev->dev;
	battmngr->battery.dev = &pdev->dev;
	battmngr->charger.dev = &pdev->dev;
	platform_set_drvdata(pdev, battmngr);
	g_battmngr_iio = &battmngr->battmngr_iio;
	g_xm_battery = &battmngr->battery;
	g_xm_charger = &battmngr->charger;
	g_battmngr_noti = &battmngr->battmngr_noti;
	mutex_init(&g_battmngr_noti->notify_lock);

	rc = xm_battmngr_iio_init(&battmngr->battmngr_iio);
	if (rc < 0) {
		pr_err("xm_battmngr_iio_init failed rc=%d\n", rc);
		return rc;
	}

	rc = xm_battery_init(&battmngr->battery);
	if (rc < 0) {
		pr_err("xm_battery_init failed rc=%d\n", rc);
		return rc;
	}

	rc = xm_charger_init(&battmngr->charger);
	if (rc < 0) {
		pr_err("xm_charger_init failed rc=%d\n", rc);
		return rc;
	}

	rc = battmngr_class_init(battmngr);
	if (rc < 0) {
		pr_err("battmngr_class_init failed rc=%d\n", rc);
		return rc;
	}

	battmngr->battmngr_nb.notifier_call = battmngr_notifier_call;
	rc = battmngr_notifier_register(&battmngr->battmngr_nb);
	if (rc < 0) {
		pr_err("%s: register battmngr notifier fail\n", __func__);
		return -EINVAL;
	}

	g_battmngr = battmngr;
	pr_err("%s: End\n", __func__);

	return 0;
}

static int xm_battmngr_remove(struct platform_device *pdev)
{
	struct xm_battmngr *battmngr = platform_get_drvdata(pdev);

	xm_charger_deinit();
	battmngr_class_exit(battmngr);
	devm_kfree(&pdev->dev,battmngr);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void xm_battmngr_shutdown(struct platform_device *pdev)
{
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
	return platform_driver_register(&xm_battmngr_driver);
}
postcore_initcall(xm_battmngr_init);

static void __exit xm_battmngr_exit(void)
{
	platform_driver_unregister(&xm_battmngr_driver);
}
module_exit(xm_battmngr_exit);

MODULE_DESCRIPTION("Xiaomi Battery Management");
MODULE_AUTHOR("getian@xiaomi.com");
MODULE_LICENSE("GPL v2");
