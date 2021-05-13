// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */


#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include "mtk-mml-driver.h"
#include "mtk-mml-core.h"

struct platform_device *mml_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *mml_node;
	struct platform_device *mml_pdev;

	mml_node = of_parse_phandle(dev->of_node, "mediatek,mml", 0);
	if (!mml_node) {
		dev_err(dev, "cannot get mml node\n");
		return NULL;
	}

	mml_pdev = of_find_device_by_node(mml_node);
	of_node_put(mml_node);
	if (WARN_ON(!mml_pdev)) {
		dev_err(dev, "mml pdev failed\n");
		return NULL;
	}

	return mml_pdev;
}
EXPORT_SYMBOL_GPL(mml_get_plat_device);


static int comp_sys_init(struct device *dev, struct mml_dev *mml)
{
	return 0;
}

static void comp_sys_deinit(struct device *dev)
{
}

static int mml_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_dev *mml;
	int ret;

	mml = devm_kzalloc(dev, sizeof(*mml), GFP_KERNEL);
	if (!mml)
		return -ENOMEM;

	mml->pdev = pdev;
	ret = comp_sys_init(dev, mml);
	if (ret) {
		dev_err(dev, "failed to initialize mml comp system\n");
		goto err_init_comp;
	}

	mml->cmdq_base = cmdq_register_device(dev);
	mml->cmdq_clt = cmdq_mbox_create(dev, 0);
	if (IS_ERR(mml->cmdq_clt)) {
		dev_err(dev, "unable to create cmdq mbox on %p:%d\n", dev, 0);
		ret = PTR_ERR(mml->cmdq_clt);
		goto err_mbox_create;
	}

	platform_set_drvdata(pdev, mml);
	return 0;

err_mbox_create:
	comp_sys_deinit(dev);
err_init_comp:
	kfree(mml);
	return ret;
}

static int mml_remove(struct platform_device *pdev)
{
	struct mml_dev *mml = platform_get_drvdata(pdev);

	comp_sys_deinit(&pdev->dev);
	kfree(mml);
	return 0;
}

static int __maybe_unused mml_pm_suspend(struct device *dev)
{
	mml_msg("%s ignore\n", __func__);
	return 0;
}

static int __maybe_unused mml_pm_resume(struct device *dev)
{
	mml_msg("%s ignore\n", __func__);
	return 0;
}

static int __maybe_unused mml_suspend(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;
	return mml_pm_suspend(dev);
}

static int __maybe_unused mml_resume(struct device *dev)
{
	if (pm_runtime_active(dev))
		return 0;
	return mml_pm_resume(dev);
}

static const struct dev_pm_ops mml_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mml_suspend, mml_resume)
	SET_RUNTIME_PM_OPS(mml_pm_suspend, mml_pm_resume, NULL)
};

static const struct of_device_id mml_of_ids[] = {
	{
		.compatible = "mediatek,mt6893-mml",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_of_ids);

static struct platform_driver mml_driver = {
	.probe = mml_probe,
	.remove = mml_remove,
	.driver = {
		.name = "mtk-mml",
		.owner = THIS_MODULE,
		.pm = &mml_pm_ops,
		.of_match_table = mml_of_ids,
	},
};

static int __init mml_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&mml_driver);
	if (ret) {
		mml_err("failed to register %s driver\n",
			mml_driver.driver.name);
		return ret;
	}

	/* register pm notifier */

	return 0;
}
module_init(mml_driver_init);

static void __exit mml_driver_exit(void)
{
	platform_driver_unregister(&mml_driver);
}
module_exit(mml_driver_exit);

MODULE_DESCRIPTION("MediaTek multimedia-layer driver");
MODULE_AUTHOR("Ping-Hsun Wu <ping-hsun.wu@mediatek.com>");
MODULE_LICENSE("GPL v2");
