/*
 * Copyright (C) 2012-2013 NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/slab.h>

#include <linux/platform_data/nvshm.h>
#include <asm/mach/map.h>

#include "nvshm_types.h"
#include "nvshm_if.h"
#include "nvshm_priv.h"
#include "nvshm_ipc.h"
#include "nvshm_iobuf.h"

static struct nvshm_handle *_nvshm_instance;

static int nvshm_probe(struct platform_device *pdev)
{
	struct nvshm_handle *handle = NULL;
	struct tegra_bb_platform_data *bb_pdata;
	struct nvshm_platform_data *pdata =
		pdev->dev.platform_data;

	if (!pdata) {
		pr_err("%s platform_data not available\n", __func__);
		goto fail;
	}

	handle = kzalloc(sizeof(struct nvshm_handle), GFP_KERNEL);

	if (handle == NULL) {
		pr_err("%s fail to alloc memory\n", __func__);
		goto fail;
	}

	_nvshm_instance = handle;

	spin_lock_init(&handle->lock);
	spin_lock_init(&handle->qlock);

	wake_lock_init(&handle->ul_lock, WAKE_LOCK_SUSPEND, "SHM-UL");
	wake_lock_init(&handle->dl_lock, WAKE_LOCK_SUSPEND, "SHM-DL");

	handle->ipc_base_virt = pdata->ipc_base_virt;
	handle->ipc_size = pdata->ipc_size;

	handle->mb_base_virt = pdata->mb_base_virt;
	handle->mb_size = pdata->mb_size;

	handle->dev = &pdev->dev;
	handle->instance = pdev->id;

	handle->tegra_bb = pdata->tegra_bb;
	bb_pdata = handle->tegra_bb->dev.platform_data;

	handle->bb_irq = pdata->bb_irq;
	platform_set_drvdata(pdev, handle);
	nvshm_register_ipc(handle);
	return 0;
fail:
	kfree(handle);
	return -1;
}

static int __exit nvshm_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int nvshm_suspend(struct platform_device *pdev, pm_message_t state)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int nvshm_resume(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static struct platform_driver nvshm_driver = {
	.driver = {
		.name = "nvshm",
		.owner = THIS_MODULE,
	},
	.probe = nvshm_probe,
	.remove = __exit_p(nvshm_remove),
#ifdef CONFIG_PM
	.suspend = nvshm_suspend,
	.resume = nvshm_resume,
#endif
};

inline struct nvshm_handle *nvshm_get_handle()
{
	return _nvshm_instance;
}

static int __init nvshm_startup(void)
{
	int ret;
	ret = platform_driver_register(&nvshm_driver);
	pr_debug("%s ret %d\n", __func__, ret);
	return ret;
}

static void __exit nvshm_exit(void)
{
	struct nvshm_handle *handle = nvshm_get_handle();
	pr_debug("%s\n", __func__);
	nvshm_tty_cleanup();
	nvshm_unregister_ipc(handle);
	wake_lock_destroy(&handle->dl_lock);
	wake_lock_destroy(&handle->ul_lock);
	kfree(handle);
	platform_driver_unregister(&nvshm_driver);
}

module_init(nvshm_startup);
module_exit(nvshm_exit);

MODULE_DESCRIPTION("NV Shared Memory Interface");
MODULE_LICENSE("GPL");

