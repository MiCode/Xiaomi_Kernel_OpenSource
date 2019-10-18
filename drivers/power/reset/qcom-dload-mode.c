// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/qcom_scm.h>

struct qcom_dload {
	struct notifier_block panic_nb;
	struct notifier_block reboot_nb;

	bool in_panic;
};

static bool enable_dump =
	IS_ENABLED(CONFIG_POWER_RESET_QCOM_DOWNLOAD_MODE_DEFAULT);
static enum qcom_download_mode current_download_mode = QCOM_DOWNLOAD_NODUMP;

static int set_download_mode(enum qcom_download_mode mode)
{
	current_download_mode = mode;
	qcom_scm_set_download_mode(mode, 0);
	return 0;
}

static int param_set_download_mode(const char *val,
		const struct kernel_param *kp)
{
	int ret;

	/* update enable_dump according to user input */
	ret = param_set_bool(val, kp);
	if (ret)
		return ret;

	set_download_mode(QCOM_DOWNLOAD_FULLDUMP);

	return 0;
}
module_param_call(download_mode, param_set_download_mode, param_get_int,
			&enable_dump, 0644);

static int qcom_dload_panic(struct notifier_block *this, unsigned long event,
			      void *ptr)
{
	struct qcom_dload *poweroff = container_of(this, struct qcom_dload,
						     panic_nb);
	poweroff->in_panic = true;
	if (enable_dump)
		set_download_mode(QCOM_DOWNLOAD_FULLDUMP);
	return NOTIFY_OK;
}

static int qcom_dload_reboot(struct notifier_block *this, unsigned long event,
			      void *ptr)
{
	char *cmd = ptr;
	struct qcom_dload *poweroff = container_of(this, struct qcom_dload,
						     reboot_nb);

	/* Clean shutdown, disable dump mode to allow normal restart */
	if (!poweroff->in_panic)
		set_download_mode(QCOM_DOWNLOAD_NODUMP);

	if (cmd) {
		if (!strcmp(cmd, "edl"))
			set_download_mode(QCOM_DOWNLOAD_EDL);
		else if (!strcmp(cmd, "qcom_dload"))
			set_download_mode(QCOM_DOWNLOAD_FULLDUMP);
	}

	if (current_download_mode != QCOM_DOWNLOAD_NODUMP)
		reboot_mode = REBOOT_WARM;

	return NOTIFY_OK;
}


static int qcom_dload_probe(struct platform_device *pdev)
{
	struct qcom_dload *poweroff;

	poweroff = devm_kzalloc(&pdev->dev, sizeof(*poweroff), GFP_KERNEL);
	if (!poweroff)
		return -ENOMEM;

	if (enable_dump)
		set_download_mode(QCOM_DOWNLOAD_FULLDUMP);
	else {
		set_download_mode(QCOM_DOWNLOAD_NODUMP);
		qcom_scm_disable_sdi();
	}

	poweroff->panic_nb.notifier_call = qcom_dload_panic;
	poweroff->panic_nb.priority = INT_MAX;
	atomic_notifier_chain_register(&panic_notifier_list,
				       &poweroff->panic_nb);

	poweroff->reboot_nb.notifier_call = qcom_dload_reboot;
	poweroff->reboot_nb.priority = 255;
	register_reboot_notifier(&poweroff->reboot_nb);

	platform_set_drvdata(pdev, poweroff);

	return 0;
}

static int qcom_dload_remove(struct platform_device *pdev)
{
	struct qcom_dload *poweroff = platform_get_drvdata(pdev);

	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &poweroff->panic_nb);
	unregister_reboot_notifier(&poweroff->reboot_nb);

	return 0;
}

static const struct of_device_id of_qcom_dload_match[] = {
	{ .compatible = "qcom,dload-mode", },
	{},
};
MODULE_DEVICE_TABLE(of, of_qcom_dload_match);

static struct platform_driver qcom_dload_driver = {
	.probe = qcom_dload_probe,
	.remove = qcom_dload_remove,
	.driver = {
		.name = "qcom-dload-mode",
		.of_match_table = of_match_ptr(of_qcom_dload_match),
	},
};

static int __init qcom_dload_driver_init(void)
{
	return platform_driver_register(&qcom_dload_driver);
}
#if IS_MODULE(CONFIG_POWER_RESET_QCOM_DOWNLOAD_MODE)
module_init(qcom_dload_driver_init);
#else
pure_initcall(qcom_dload_driver_init);
#endif

static void __exit qcom_dload_driver_exit(void)
{
	return platform_driver_unregister(&qcom_dload_driver);
}
module_exit(qcom_dload_driver_exit);

MODULE_DESCRIPTION("MSM Download Mode Driver");
MODULE_LICENSE("GPL v2");
