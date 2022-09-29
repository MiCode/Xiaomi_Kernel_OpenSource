// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include "mtk-mmdvfs-debug.h"


#define MMDVFS_DBG(fmt, args...) \
	pr_notice("[mmdvfs_v3_start][dbg]%s: "fmt"\n", __func__, ##args)


static int mmdvfs_v3_start_thread(void *data)
{
	int retry = 0;

	while (!mtk_is_mmdvfs_v3_debug_init_done()) {
		if (++retry > 100) {
			MMDVFS_DBG("mmdvfs_v3 debug init not ready");
			break;
		}
		ssleep(2);
	}

	MMDVFS_DBG("disable vcp!!!!!!");
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_INIT);
	return 0;
}

static int mmdvfs_v3_start_probe(struct platform_device *pdev)
{
	struct task_struct *kthr;

	MMDVFS_DBG("is called!!!!!!");
	kthr = kthread_run(
		mmdvfs_v3_start_thread, NULL, "mmdvfs-v3_start");

	return 0;
}


static const struct of_device_id of_mmdvfs_v3_start_match_tbl[] = {
	{
		.compatible = "mediatek,mmdvfs-v3-start",
	},
	{}
};

static struct platform_driver mmdvfs_v3_start_drv = {
	.probe = mmdvfs_v3_start_probe,
	.driver = {
		.name = "mtk-mmdvfs-v3-start",
		.of_match_table = of_mmdvfs_v3_start_match_tbl,
	},
};

static int __init mmdvfs_v3_start_init(void)
{
	int ret;

	ret = platform_driver_register(&mmdvfs_v3_start_drv);
	if (ret)
		MMDVFS_DBG("failed:%d", ret);

	return ret;
}

static void __exit mmdvfs_v3_start_exit(void)
{
	platform_driver_unregister(&mmdvfs_v3_start_drv);
}

module_init(mmdvfs_v3_start_init);
module_exit(mmdvfs_v3_start_exit);
MODULE_DESCRIPTION("MMDVFS V3 start Driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");

