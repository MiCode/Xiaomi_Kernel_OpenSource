// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Yuan Jung Kuo <yuan-jung.kuo@mediatek.com>
 */

#include <linux/io.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/notifier.h>
#include <linux/timekeeping.h>
#include <linux/remoteproc/mtk_ccu.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <soc/mediatek/mmdvfs_v3.h>

#include <mt-plat/mtk-vmm-notifier.h>


#define ISP_LOGI(fmt, args...) \
	pr_notice("[VMM_NOTI] %s(): " fmt "\n", \
		__func__, ##args)
#define ISP_LOGE(fmt, args...) \
	pr_notice("[VMM_NOTI] error %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)
#define ISP_LOGF(fmt, args...) \
	pr_notice("[VMM_NOTI] fatal %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)

struct mutex ctrl_mutex;
static int vmm_user_counter;
static int vmm_locked_isp_open(void)
{
	vmm_user_counter++;
	if (vmm_user_counter == 1) {
		ISP_LOGI("ISP mtcmos on");
		mtk_mmdvfs_camera_notify(1);
	}

	return 0;
}

static int vmm_locked_isp_close(void)
{
	/* no need to counter down at probe stage */
	if (vmm_user_counter == 0)
		return 0;

	vmm_user_counter--;
	if (vmm_user_counter == 0) {
		ISP_LOGI("ISP mtcmos off");
		mtk_mmdvfs_camera_notify(0);
	}

	return 0;
}

static int mtk_camera_pd_callback(struct notifier_block *nb,
		unsigned long flags, void *data)
{
	int ret = 0;

	mutex_lock(&ctrl_mutex);

	if (flags == GENPD_NOTIFY_PRE_ON)
		ret = vmm_locked_isp_open();
	else if (flags == GENPD_NOTIFY_OFF)
		ret = vmm_locked_isp_close();

	mutex_unlock(&ctrl_mutex);

	return ret;
}

int vmm_isp_ctrl_notify(int openIsp)
{
	int ret;

	mutex_lock(&ctrl_mutex);

	if (openIsp)
		ret = vmm_locked_isp_open();
	else
		ret = vmm_locked_isp_close();

	mutex_unlock(&ctrl_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(vmm_isp_ctrl_notify);

static struct notifier_block mtk_pd_notifier = {
	.notifier_call = mtk_camera_pd_callback,
	.priority = 0,
};

static int vmm_notifier_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;

	mutex_init(&ctrl_mutex);

	pm_runtime_enable(dev);
	ret = dev_pm_genpd_add_notifier(dev, &mtk_pd_notifier);
	if (ret)
		ISP_LOGE("gen pd add notifier fail(%d)", ret);

	return 0;
}

static const struct of_device_id of_vmm_notifier_match_tbl[] = {
	{
		.compatible = "mediatek,vmm_notifier",
	},
	{}
};

static struct platform_driver drv_vmm_notifier = {
	.probe = vmm_notifier_probe,
	.driver = {
		.name = "mtk-vmm-notifier",
		.of_match_table = of_vmm_notifier_match_tbl,
	},
};

static int __init mtk_vmm_notifier_init(void)
{
	s32 status;

	status = platform_driver_register(&drv_vmm_notifier);
	if (status) {
		pr_notice("Failed to register VMM dbg driver(%d)\n", status);
		return -ENODEV;
	}

	return 0;
}

static void __exit mtk_vmm_notifier_exit(void)
{
	platform_driver_unregister(&drv_vmm_notifier);
}

module_init(mtk_vmm_notifier_init);
module_exit(mtk_vmm_notifier_exit);
MODULE_DESCRIPTION("MTK VMM notifier driver");
MODULE_AUTHOR("Yuan Jung Kuo <yuan-jung.kuo@mediatek.com>");
MODULE_LICENSE("GPL v2");
