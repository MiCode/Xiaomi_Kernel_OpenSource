// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 UNISOC Communications Inc.
 */
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kstrtox.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>
#include "../base/base.h"
#define PMICDUMP_CMD_MARGIC 'P'
#define PMICDUMP_CMD_READ	_IOR(PMICDUMP_CMD_MARGIC, 0x1, \
							struct pmic_info_t)
#define PMIC_INFO_SIZE (4000)
#define PMICDUMP_INFO(fmt, ...) \
	pr_info("[%s] "pr_fmt(fmt), "PMICDUMP_INFO", ##__VA_ARGS__)
struct pmic_info_t {
	u32 pmic_frt;
	u64 rtc_tm;
	u32 chip_id;
	char pwroff_reason[64];
	char reboot_reason[64];
	u32 core_vol;
	u32 sram_vol;
	u32 dcdc_status;
	u32 ldo_status;
	u64 ldo_socp;
};
struct pmic_dump_data {
	struct regmap	*pmic_mem;
	struct miscdevice mdev;
};
static struct pmic_info_t __iomem *pmic_info;
static long sprd_pmic_dump_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	void __user *uarg = (void __user *)arg;
	struct pmic_info_t pmic_data;

	pmic_data.pmic_frt = pmic_info->pmic_frt;
	pmic_data.rtc_tm = pmic_info->rtc_tm;
	pmic_data.chip_id = pmic_info->chip_id;
	strscpy(pmic_data.pwroff_reason, pmic_info->pwroff_reason,
		sizeof(pmic_data.pwroff_reason));
	strscpy(pmic_data.reboot_reason, pmic_info->reboot_reason,
		sizeof(pmic_data.reboot_reason));
	pmic_data.core_vol = pmic_info->core_vol;
	pmic_data.sram_vol = pmic_info->sram_vol;
	pmic_data.dcdc_status = pmic_info->dcdc_status;
	pmic_data.ldo_status = pmic_info->ldo_status;
	pmic_data.ldo_socp = pmic_info->ldo_socp;
	switch (cmd) {
	case PMICDUMP_CMD_READ:
		PMICDUMP_INFO("ioctrl:PMICDUMP_CMD_READ.\n");
		if (copy_to_user(uarg, &pmic_data, sizeof(pmic_data))) {
			PMICDUMP_INFO("ioctrl:pmic_dump copy_to_user fail.\n");
			return -EFAULT;
		}
		break;
	default:
		PMICDUMP_INFO("pmic_dump ioctrl default.\n");
		return -EINVAL;
	}
	return 0;
}
static const struct file_operations sprd_pmic_dump_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = sprd_pmic_dump_ioctl,
	.compat_ioctl	= sprd_pmic_dump_ioctl,
};
/*
 * DEVSYS interfaces:
 * /dev/sprd_pmic_dump [rw] [userspace]
 */
static int sprd_pmic_dump_device_create(struct pmic_dump_data *pmic_dump)
{
	int ret;

	pmic_dump->mdev.minor = MISC_DYNAMIC_MINOR;
	pmic_dump->mdev.name = "sprd_pmic_dump";
	pmic_dump->mdev.fops = &sprd_pmic_dump_fops;
	pmic_dump->mdev.parent = NULL;
	ret = misc_register(&pmic_dump->mdev);
	if (ret) {
		PMICDUMP_INFO("pmic_dump: failed to register misc device.\n");
		return ret;
	}
	return 0;
}
static int sprd_pmic_dump_probe(struct platform_device *pdev)
{
	struct pmic_dump_data *pmic_dump;
	struct device_node *np;
	struct resource r;
	resource_size_t size, size_target;
	char *virt_info;
	int ret;

	pmic_dump = devm_kzalloc(&pdev->dev, sizeof(struct pmic_dump_data), GFP_KERNEL);
	if (!pmic_dump)
		return -ENOMEM;
	np = of_parse_phandle(pdev->dev.of_node, "sprd,pmic-dump-mem", 0);
	if (!np) {
		dev_err(&pdev->dev, "No sprd,pmic-dump-mem specified\n");
		return -EINVAL;
	}
	ret = of_address_to_resource(np, 0, &r);
	of_node_put(np);
	if (ret) {
		dev_err(&pdev->dev, "of_address_to_resource fail\n");
		return ret;
	}
	size = resource_size(&r);
	size_target = PMIC_INFO_SIZE;
	if (size_target > size) {
		dev_err(&pdev->dev, "rmem_init size error\n");
		return -ENOMEM;
	}
	pmic_dump->pmic_mem =  memremap(r.start, size, MEMREMAP_WB);
	if (!pmic_dump->pmic_mem) {
		dev_err(&pdev->dev, "memremap fail\n");
		return -ENOMEM;
	}
	virt_info = ((char *)pmic_dump->pmic_mem);
	pmic_info = (struct pmic_info_t *)virt_info;
	ret = sprd_pmic_dump_device_create(pmic_dump);
	if (ret) {
		dev_err(&pdev->dev, "failed to sprd pmic dump device create\n");
		return ret;
	}
	platform_set_drvdata(pdev, pmic_dump);
	return 0;
}

static int sprd_pmic_dump_remove(struct platform_device *pdev)
{
	struct pmic_dump_data *pmic_dump = dev_get_drvdata(&pdev->dev);

	misc_deregister(&pmic_dump->mdev);
	return 0;
}
static const struct of_device_id sprd_pmic_dump_match[] = {
	{ .compatible = "sprd,pmic-dump"},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pmic_dump_match);

static struct platform_driver sprd_pmic_dump_driver = {
	.probe = sprd_pmic_dump_probe,
	.remove = sprd_pmic_dump_remove,
	.driver = {
		.name = "sprd-pmic-dump",
		.of_match_table = sprd_pmic_dump_match,
	},
};

module_platform_driver(sprd_pmic_dump_driver);
MODULE_AUTHOR("XiaoQing Wu <xiaoqing.wu@unisoc.com>");
MODULE_DESCRIPTION("UNISOC pmic dump driver");
MODULE_LICENSE("GPL");
