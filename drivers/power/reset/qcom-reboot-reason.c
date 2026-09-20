// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/of_address.h>
#include <linux/nvmem-consumer.h>
#include <linux/input/qpnp-power-on.h>

struct qcom_reboot_reason {
	struct device *dev;
	struct notifier_block reboot_nb;
	struct nvmem_cell *nvmem_cell;
};

struct poweroff_reason {
	const char *cmd;
	unsigned char pon_reason;
};

static struct poweroff_reason reasons[] = {
	{ "recovery",			0x01 },
	{ "bootloader",			0x02 },
	{ "rtc",			0x03 },
	{ "dm-verity device corrupted",	0x04 },
	{ "dm-verity enforcing",	0x05 },
	{ "keys clear",			0x06 },
	{}
};

#define RESTART_REASON_PANIC  6
#define RESTART_REASON_NORMAL 7
static struct qcom_reboot_reason *ffu_reboot = NULL;

int ufs_ffu_reboot_reason_reboot(void *ptr)
{
	char *cmd = ptr;
	struct qcom_reboot_reason *reboot = ffu_reboot;
	struct poweroff_reason *reason;

	if(!ffu_reboot)
		return NOTIFY_BAD;

	if (!cmd) {
		nvmem_cell_write(reboot->nvmem_cell,
				 &reasons[RESTART_REASON_NORMAL].pon_reason,
				 sizeof(reasons[RESTART_REASON_NORMAL].pon_reason));
		return NOTIFY_OK;
	}

	for (reason = reasons; reason->cmd; reason++) {
		if (!strcmp(cmd, reason->cmd)) {
			nvmem_cell_write(reboot->nvmem_cell,
					 &reason->pon_reason,
					 sizeof(reason->pon_reason));
			return NOTIFY_OK;
		}
	}

	nvmem_cell_write(reboot->nvmem_cell,
			&reason->pon_reason,
			sizeof(reason->pon_reason));

	return NOTIFY_OK;
}
EXPORT_SYMBOL(ufs_ffu_reboot_reason_reboot);

static int qcom_reboot_reason_reboot(struct notifier_block *this,
				     unsigned long event, void *ptr)
{
	char *cmd = ptr;
	struct qcom_reboot_reason *reboot = container_of(this,
		struct qcom_reboot_reason, reboot_nb);
	struct poweroff_reason *reason;

	if (!cmd)
		return NOTIFY_OK;
	for (reason = reasons; reason->cmd; reason++) {
		if (!strcmp(cmd, reason->cmd)) {
			if (reboot->nvmem_cell)
				nvmem_cell_write(reboot->nvmem_cell,
						 &reason->pon_reason,
						 sizeof(reason->pon_reason));
			else
				qpnp_pon_set_restart_reason(
						(enum pon_restart_reason)reason->pon_reason);
			break;
		}
	}

	return NOTIFY_OK;
}

static int qcom_reboot_reason_probe(struct platform_device *pdev)
{
	struct qcom_reboot_reason *reboot;

	reboot = devm_kzalloc(&pdev->dev, sizeof(*reboot), GFP_KERNEL);
	if (!reboot)
		return -ENOMEM;

	reboot->dev = &pdev->dev;

	if (of_property_read_bool(pdev->dev.of_node, "qcom,no-nvmem-cell-support"))
		reboot->nvmem_cell = NULL;
	else {
		reboot->nvmem_cell = nvmem_cell_get(reboot->dev, "restart_reason");
		if (IS_ERR(reboot->nvmem_cell))
			return PTR_ERR(reboot->nvmem_cell);
	}

	reboot->reboot_nb.notifier_call = qcom_reboot_reason_reboot;
	reboot->reboot_nb.priority = 255;
	register_reboot_notifier(&reboot->reboot_nb);

	platform_set_drvdata(pdev, reboot);
	ffu_reboot = reboot;
	return 0;
}

static int qcom_reboot_reason_remove(struct platform_device *pdev)
{
	struct qcom_reboot_reason *reboot = platform_get_drvdata(pdev);

	unregister_reboot_notifier(&reboot->reboot_nb);

	return 0;
}

static const struct of_device_id of_qcom_reboot_reason_match[] = {
	{ .compatible = "qcom,reboot-reason", },
	{},
};
MODULE_DEVICE_TABLE(of, of_qcom_reboot_reason_match);

static struct platform_driver qcom_reboot_reason_driver = {
	.probe = qcom_reboot_reason_probe,
	.remove = qcom_reboot_reason_remove,
	.driver = {
		.name = "qcom-reboot-reason",
		.of_match_table = of_match_ptr(of_qcom_reboot_reason_match),
	},
};

module_platform_driver(qcom_reboot_reason_driver);

MODULE_DESCRIPTION("MSM Reboot Reason Driver");
MODULE_LICENSE("GPL");
