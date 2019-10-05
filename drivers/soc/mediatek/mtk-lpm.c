// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/cpuidle.h>
#include <linux/soc/mediatek/mtk-lpm.h>

#define MTK_PWR_CONSERVATION_PREPARE	(0)
#define MTK_PWR_CONSERVATION_RESUME	(1)

typedef int (*mtk_pwr_conservation_fn)(int type,
				struct cpuidle_driver *drv,
				int index);

static struct mtk_cpuidle_op *mtk_lpm_ops __read_mostly;

int mtk_lpm_drv_cpuidle_ops_set(struct mtk_cpuidle_op *op)
{
	int ret = 0;

	cpuidle_pause_and_lock();

	if ((!op && mtk_lpm_ops) || (op && !mtk_lpm_ops))
		rcu_assign_pointer(mtk_lpm_ops, op);
	else
		ret = -EACCES;

	cpuidle_resume_and_unlock();

	return ret;
}
EXPORT_SYMBOL(mtk_lpm_drv_cpuidle_ops_set);

int mtk_lpm_pwr_conservation(int type,
			     struct cpuidle_driver *drv,
			     int index)
{
	int ret = -EBADR;

	if (!mtk_lpm_ops)
		return ret;

	switch (type) {
	case MTK_PWR_CONSERVATION_PREPARE:
		if (mtk_lpm_ops->cpuidle_prepare)
			ret = mtk_lpm_ops->cpuidle_prepare(drv, index);
		break;
	case MTK_PWR_CONSERVATION_RESUME:
		if (mtk_lpm_ops->cpuidle_resume)
			mtk_lpm_ops->cpuidle_resume(drv, index);
		ret = 0;
		break;
	default:
		break;
	}

	return ret;
}

static int mtk_lp_pm_driver_probe(struct platform_device *pdev)
{
	struct platform_device	*mtk_cpuidle_pm_dev;
	int ret = -ENOMEM;
	mtk_pwr_conservation_fn mtk_lpm_pwr = mtk_lpm_pwr_conservation;

	mtk_cpuidle_pm_dev =
		platform_device_alloc(MTK_CPUIDLE_PM_NAME, -1);

	if (!mtk_cpuidle_pm_dev)
		goto out_probe_alloc_fail;

	mtk_cpuidle_pm_dev->dev.parent = &pdev->dev;
	ret = platform_device_add(mtk_cpuidle_pm_dev);

	if (ret)
		goto put_device;

	platform_device_add_data(mtk_cpuidle_pm_dev,
				       &mtk_lpm_pwr,
				       sizeof(mtk_lpm_pwr));

	device_init_wakeup(&mtk_cpuidle_pm_dev->dev, true);

	return 0;

put_device:
	platform_device_put(mtk_cpuidle_pm_dev);
out_probe_alloc_fail:

	return ret;
}

static const struct of_device_id of_mtk_lp_pm_match[] = {
	{ .compatible = "mediatek,mtk-lpm" },
	{}
};

static struct platform_driver mtk_lp_pm_driver = {
	.probe = mtk_lp_pm_driver_probe,
	.driver = {
	   .name = "mtk_lpm_driver",
	   .owner = THIS_MODULE,
	   .of_match_table = of_match_ptr(of_mtk_lp_pm_match),
	},
};
builtin_platform_driver(mtk_lp_pm_driver);
