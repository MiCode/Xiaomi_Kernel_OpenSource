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

#define MMDVFS_DBG(fmt, args...) \
	pr_notice("[mmdvfs_ccu][dbg]%s: "fmt"\n", __func__, ##args)


static int mmdvfs_call_ccu(struct platform_device *pdev,
	enum mtk_ccu_feature_type featureType,
	uint32_t msgId, void *inDataPtr, uint32_t inDataSize)
{
	int ret = 0;
	//MMDVFS_DBG("mmdvfs_call_ccu is called!!!!!!");
	ret = mtk_ccu_rproc_ipc_send(
		pdev, featureType, msgId, inDataPtr, inDataSize);

	return ret;
}

static int mmdvfs_ccu_probe(struct platform_device *pdev)
{
	MMDVFS_DBG("is called!!!!!!");
	mmdvfs_call_ccu_set_fp(mmdvfs_call_ccu);

	return 0;
}


static const struct of_device_id of_mmdvfs_ccu_match_tbl[] = {
	{
		.compatible = "mediatek,mmdvfs-ccu",
	},
	{}
};

static struct platform_driver mmdvfs_ccu_drv = {
	.probe = mmdvfs_ccu_probe,
	.driver = {
		.name = "mtk-mmdvfs-ccu",
		.of_match_table = of_mmdvfs_ccu_match_tbl,
	},
};

static int __init mmdvfs_ccu_init(void)
{
	int ret;

	ret = platform_driver_register(&mmdvfs_ccu_drv);
	if (ret)
		MMDVFS_DBG("failed:%d", ret);

	return ret;
}

static void __exit mmdvfs_ccu_exit(void)
{
	platform_driver_unregister(&mmdvfs_ccu_drv);
}

module_init(mmdvfs_ccu_init);
module_exit(mmdvfs_ccu_exit);
MODULE_DESCRIPTION("MMDVFS ccu Driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");

