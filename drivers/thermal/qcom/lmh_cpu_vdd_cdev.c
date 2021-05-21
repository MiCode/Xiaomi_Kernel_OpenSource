// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/cpu_cooling.h>
#include <linux/idr.h>

#define LMH_CPU_VDD_MAX_LVL	1
#define LIMITS_CLUSTER_MIN_FREQ_OFFSET	0x3C0

struct lmh_cpu_vdd_cdev {
	struct list_head node;
	int id;
	bool cpu_vdd_state;
	void *min_freq_reg;
	struct thermal_cooling_device *cdev;
};

static DEFINE_IDA(lmh_cpu_vdd_ida);
static DEFINE_MUTEX(lmh_cpu_vdd_lock);
static LIST_HEAD(lmh_cpu_vdd_list);

static int lmh_cpu_vdd_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct lmh_cpu_vdd_cdev *vdd_cdev = cdev->devdata;

	if (state > LMH_CPU_VDD_MAX_LVL)
		return -EINVAL;

	state = !!state;
	/* Check if the old cooling action is same as new cooling action */
	if (vdd_cdev->cpu_vdd_state == state)
		return 0;

	writel_relaxed(state, vdd_cdev->min_freq_reg);
	vdd_cdev->cpu_vdd_state = state;

	pr_debug("%s limits CPU VDD restriction for %s\n",
		state ? "Triggered" : "Cleared", vdd_cdev->cdev->type);

	return 0;
}

static int lmh_cpu_vdd_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct lmh_cpu_vdd_cdev *lmh_cpu_vdd_cdev = cdev->devdata;

	*state = (lmh_cpu_vdd_cdev->cpu_vdd_state) ?
			LMH_CPU_VDD_MAX_LVL : 0;

	return 0;
}

static int lmh_cpu_vdd_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	*state = LMH_CPU_VDD_MAX_LVL;
	return 0;
}

static struct thermal_cooling_device_ops lmh_cpu_vdd_cooling_ops = {
	.get_max_state = lmh_cpu_vdd_get_max_state,
	.get_cur_state = lmh_cpu_vdd_get_cur_state,
	.set_cur_state = lmh_cpu_vdd_set_cur_state,
};


static int lmh_cpu_vdd_probe(struct platform_device *pdev)
{
	int ret = -1;
	struct lmh_cpu_vdd_cdev *lmh_cpu_vdd_cdev;
	struct device_node *dn = pdev->dev.of_node;
	uint32_t min_reg;
	char cdev_name[THERMAL_NAME_LENGTH] = "";
	const __be32 *addr;

	lmh_cpu_vdd_cdev = devm_kzalloc(&pdev->dev, sizeof(*lmh_cpu_vdd_cdev),
					GFP_KERNEL);
	if (!lmh_cpu_vdd_cdev)
		return -ENOMEM;

	addr = of_get_address(dn, 0, NULL, NULL);
	if (!addr) {
		dev_err(&pdev->dev, "Property llm-base-addr not found\n");
		return -EINVAL;
	}

	min_reg = be32_to_cpu(addr[0]) + LIMITS_CLUSTER_MIN_FREQ_OFFSET;
	lmh_cpu_vdd_cdev->min_freq_reg = devm_ioremap(&pdev->dev, min_reg, 0x4);
	if (!lmh_cpu_vdd_cdev->min_freq_reg) {
		dev_err(&pdev->dev, "lmh cpu vdd register remap failed\n");
		return -ENOMEM;
	}

	mutex_lock(&lmh_cpu_vdd_lock);
	ret = ida_simple_get(&lmh_cpu_vdd_ida, 0, 0, GFP_KERNEL);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to create ida\n");
		goto unlock_exit;
	}
	lmh_cpu_vdd_cdev->id = ret;
	ret = 0;

	snprintf(cdev_name, THERMAL_NAME_LENGTH, "lmh-cpu-vdd%d",
			lmh_cpu_vdd_cdev->id);

	lmh_cpu_vdd_cdev->cdev = thermal_of_cooling_device_register(
					dn,
					cdev_name,
					lmh_cpu_vdd_cdev,
					&lmh_cpu_vdd_cooling_ops);
	if (IS_ERR(lmh_cpu_vdd_cdev->cdev)) {
		ret = PTR_ERR(lmh_cpu_vdd_cdev->cdev);
		dev_err(&pdev->dev, "Cooling register failed for %s, ret:%d\n",
			cdev_name, ret);
		lmh_cpu_vdd_cdev->cdev = NULL;
		goto remove_ida;
	}
	list_add(&lmh_cpu_vdd_cdev->node, &lmh_cpu_vdd_list);
	mutex_unlock(&lmh_cpu_vdd_lock);

	pr_debug("Cooling device [%s] registered.\n", cdev_name);

	return ret;

remove_ida:
	ida_simple_remove(&lmh_cpu_vdd_ida, lmh_cpu_vdd_cdev->id);

unlock_exit:
	mutex_unlock(&lmh_cpu_vdd_lock);

	return ret;
}

static int lmh_cpu_vdd_remove(struct platform_device *pdev)
{
	struct lmh_cpu_vdd_cdev *lmh_cpu_vdd, *c_next;

	mutex_lock(&lmh_cpu_vdd_lock);
	list_for_each_entry_safe(lmh_cpu_vdd, c_next,
			&lmh_cpu_vdd_list, node) {
		if (lmh_cpu_vdd->cdev) {
			thermal_cooling_device_unregister(
				lmh_cpu_vdd->cdev);
			lmh_cpu_vdd->cdev = NULL;
		}
		ida_simple_remove(&lmh_cpu_vdd_ida, lmh_cpu_vdd->id);
		list_del(&lmh_cpu_vdd->node);
	}
	mutex_unlock(&lmh_cpu_vdd_lock);

	return 0;
}
static const struct of_device_id lmh_cpu_vdd_match[] = {
	{ .compatible = "qcom,lmh-cpu-vdd", },
	{},
};

static struct platform_driver lmh_cpu_vdd_driver = {
	.probe		= lmh_cpu_vdd_probe,
	.remove         = lmh_cpu_vdd_remove,
	.driver		= {
		.name = KBUILD_MODNAME,
		.of_match_table = lmh_cpu_vdd_match,
	},
};
builtin_platform_driver(lmh_cpu_vdd_driver);
MODULE_LICENSE("GPL v2");
