// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include "thermal_ipi.h"

#define THERMAL_IPI_TIMEOUT_MS		(2000)

#define for_each_ipi_target(i)	\
	for (i = 0; i < NUM_THERMAL_IPI_TARGET; i++)
#define is_target_invalid(t)	(t != IPI_TARGET_ALL && t >= NUM_THERMAL_IPI_TARGET)

static DEFINE_MUTEX(ipi_send_lock);
static struct thermal_ipi_target_data ipi_data[NUM_THERMAL_IPI_TARGET] = {
	[IPI_TARGET_SSPM] = {
		.name = "sspm",
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
		.config = {
			.dev = &sspm_ipidev,
			.id = IPIS_C_THERMAL,
			.use_platform_ipi = 0,
			.opt = IPI_SEND_POLLING,
		},
#endif
	},
	[IPI_TARGET_MCUPM] = {
		.name = "mcupm",
#if IS_ENABLED(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
		.config = {
			/* symbol is not exported */
			/* .dev = &mcupm_ipidev, */
			.id = CH_S_PLATFORM,
			.use_platform_ipi = 1,
			.opt = IPI_SEND_WAIT,
		},
#endif
	},
};

static int thermal_ipi_get_idx(char *name)
{
	int i;

	if (!strncasecmp(name, "all", 3))
		return IPI_TARGET_ALL;

	for_each_ipi_target(i) {
		if (!strncasecmp(name, ipi_data[i].name, strlen(name)))
			return i;
	}

	return -1;
}

static int thermal_ipi_register(enum thermal_ipi_target target)
{
	struct thermal_ipi_config *cfg;
	int ret = -1;

	if (target >= NUM_THERMAL_IPI_TARGET)
		return -EINVAL;

	cfg = &ipi_data[target].config;
	if (!cfg->dev) {
		pr_err("[thermal_ipi] target %s is not support\n", ipi_data[target].name);
		return -EPERM;
	}

	/* no need to register if we share IPI pin with platform driver */
	if (cfg->use_platform_ipi)
		goto register_done;

	ret = mtk_ipi_register(cfg->dev, cfg->id, NULL, NULL, (void *)&cfg->ack_data);
	if (ret != 0) {
		pr_err("[thermal_ipi] Fail to register %s IPI, ret:%d\n",
			ipi_data[target].name, ret);
		return ret;
	}

register_done:
	pr_info("[thermal_ipi] register %s ipi done\n", ipi_data[target].name);
	ipi_data[target].is_registered = 1;

	return 0;
}

static int thermal_ipi_send(unsigned int cmd, enum thermal_ipi_target target,
			struct thermal_ipi_data *thermal_data)
{
	struct thermal_ipi_config *cfg;
	unsigned int ack_data;
	int ret = -1;

	if (target >= NUM_THERMAL_IPI_TARGET)
		return IPI_NOT_SUPPORT;

	mutex_lock(&ipi_send_lock);

	if (!ipi_data[target].is_registered) {
		ack_data = IPI_NOT_SUPPORT;
		goto end;
	}

	cfg = &ipi_data[target].config;
	thermal_data->cmd = cmd;
	ret = mtk_ipi_send_compl(cfg->dev, cfg->id, cfg->opt, thermal_data,
				THERMAL_SLOT_NUM, THERMAL_IPI_TIMEOUT_MS);
	if (ret != 0) {
		pr_err("[thermal_ipi] send cmd(%d) to %s error ret:%d\n",
			cmd, ipi_data[target].name, ret);
		ack_data = IPI_FAIL;
		goto end;
	}

	ack_data = cfg->ack_data;

	pr_info("[thermal_ipi] %s cmd(%d) send done, ack: %u\n",
		ipi_data[target].name, cmd, ack_data);

end:
	mutex_unlock(&ipi_send_lock);

	return ack_data;
}

static void thermal_ipi_send_all(unsigned int cmd, struct thermal_ipi_data *thermal_data)
{
	unsigned int i;

	for_each_ipi_target(i)
		thermal_ipi_send(cmd, (enum thermal_ipi_target)i, thermal_data);
}

static int thermal_ipi_set_throttle_enable(enum thermal_ipi_target target, int enable)
{
	struct thermal_ipi_data thermal_data;
	unsigned int ipi_cmd;
	int ret = 0;

	if (is_target_invalid(target))
		return -EINVAL;

	ipi_cmd = THERMAL_THROTTLE_DISABLE;
	thermal_data.arg[0] = (enable) ? 0 : 1;
	thermal_data.arg[1] = 0;
	thermal_data.arg[2] = 0;

	if (target == IPI_TARGET_ALL)
		thermal_ipi_send_all(ipi_cmd, &thermal_data);
	else
		ret = thermal_ipi_send(ipi_cmd, target, &thermal_data);

	return ret;
}

static ssize_t setting_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int idx;
	char cmd[20], name[THERMAL_TARGET_NAME_LEN];

	if (sscanf(buf, "%9s %19s", name, cmd) == 2) {
		idx = thermal_ipi_get_idx(name);
		if (idx < 0) {
			pr_err("[thermal_ipi] target %s not found\n", name);
			return -EINVAL;
		}

		pr_info("[thermal_ipi] idx(%d) cmd(%s)\n", idx, cmd);

		if (!strncasecmp(cmd, "disable", 7)) {
			thermal_ipi_set_throttle_enable((enum thermal_ipi_target)idx, 0);
		} else if (!strncasecmp(cmd, "enable", 6)) {
			thermal_ipi_set_throttle_enable((enum thermal_ipi_target)idx, 1);
		} else {
			pr_err("[thermal_ipi] invalid cmd(%s)\n", cmd);
			return -EINVAL;
		}
	} else {
		pr_err("[thermal_ipi] invalid input\n");
		return -EINVAL;
	}

	return count;
}

static struct kobj_attribute thermal_ipi_attr = __ATTR_WO(setting);
static struct attribute *thermal_ipi_attrs[] = {
	&thermal_ipi_attr.attr,
	NULL
};
static struct attribute_group thermal_ipi_attr_group = {
	.name	= "thermal_ipi",
	.attrs	= thermal_ipi_attrs,
};

static const struct of_device_id thermal_ipi_of_match[] = {
	{ .compatible = "mediatek,thermal-ipi", },
	{},
};
MODULE_DEVICE_TABLE(of, thermal_ipi_of_match);

static int thermal_ipi_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	unsigned int target_bitmask;
	int ret, i;

	if (!np) {
		dev_err(dev, "thermal_ipi DT node not found\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "target-bitmask", &target_bitmask);
	if (ret < 0) {
		dev_err(dev, "failed to ipi target bitmask from device tree\n");
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
	ipi_data[IPI_TARGET_MCUPM].config.dev = (struct mtk_ipi_device *)get_mcupm_ipidev();
#endif

	for_each_ipi_target(i) {
		if (target_bitmask & (1 << i))
			thermal_ipi_register((enum thermal_ipi_target)i);
	}

	ret = sysfs_create_group(kernel_kobj, &thermal_ipi_attr_group);
	if (ret)
		dev_err(dev, "failed to create thermal_ipi sysfs, ret=%d!\n", ret);


	return ret;
}

static int thermal_ipi_remove(struct platform_device *pdev)
{
	sysfs_create_group(kernel_kobj, &thermal_ipi_attr_group);

	return 0;
}

static struct platform_driver thermal_ipi_driver = {
	.probe = thermal_ipi_probe,
	.remove = thermal_ipi_remove,
	.driver = {
		.name = "mtk-thermal-ipi",
		.of_match_table = thermal_ipi_of_match,
	},
};
module_platform_driver(thermal_ipi_driver);

MODULE_AUTHOR("Shun-Yao Yang <brian-sy.yang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek thermal ipi driver");
MODULE_LICENSE("GPL v2");
