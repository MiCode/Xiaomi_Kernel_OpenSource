/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "npu_common.h"

#define CLASS_NAME "npu"
#define DRIVER_NAME "msm_npu"

static ssize_t npu_show_capabilities(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "hw_version :0x%X",
			NPU_FIRMWARE_VERSION);
	return ret;
}

static DEVICE_ATTR(caps, 0444, npu_show_capabilities, NULL);

static struct attribute *npu_fs_attrs[] = {
	&dev_attr_caps.attr,
	NULL
};

static struct attribute_group npu_fs_attr_group = {
	.attrs = npu_fs_attrs
};

static int npu_get_info(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	struct msm_npu_get_info_ioctl_t req;
	void __user *argp = (void __user *)arg;
	int ret;

	ret = copy_from_user(&req, argp, sizeof(req));

	if (ret) {
		pr_err("fail to copy from user\n");
		return ret;
	}
	req.firmware_version = NPU_FIRMWARE_VERSION;
	ret = copy_to_user(argp, &req, sizeof(req));

	if (ret) {
		pr_err("fail to copy to user\n");
		return ret;
	}
	return 0;
}

static int npu_open(struct inode *inode, struct file *file)
{
	struct npu_device_t *npu_dev = container_of(inode->i_cdev,
		struct npu_device_t, cdev);

	file->private_data = npu_dev;
	return 0;
}

static int npu_close(struct inode *inode, struct file *file)
{
	return 0;
}

static long npu_ioctl(struct file *file, unsigned int cmd,
						 unsigned long arg)
{
	int ret = -EPERM;

	switch (cmd) {
	case MSM_NPU_GET_INFO:
		ret = npu_get_info(file, cmd, arg);
		break;
	case MSM_NPU_MAP_BUF:
		break;
	case MSM_NPU_UNMAP_BUF:
		break;
	case MSM_NPU_LOAD_NETWORK:
		break;
	case MSM_NPU_UNLOAD_NETWORK:
		break;
	case MSM_NPU_EXEC_NETWORK:
		break;
	default:
		pr_err("unexpected IOCTL %d\n", cmd);
	}

	return ret;
}

/* 32 bit */
#ifdef CONFIG_COMPAT
static long npu_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	int ret = -EPERM;

	switch (cmd) {
	case MSM_NPU_GET_INFO_32:
		ret = npu_get_info(file, cmd, arg);
		break;
	case MSM_NPU_MAP_BUF_32:
		break;
	case MSM_NPU_UNMAP_BUF_32:
		break;
	case MSM_NPU_LOAD_NETWORK_32:
		break;
	case MSM_NPU_UNLOAD_NETWORK_32:
		break;
	case MSM_NPU_EXEC_NETWORK_32:
		break;
	default:
		pr_err("unexpected IOCTL %d\n", cmd);
	}

	return ret;
}
#endif

static const struct file_operations npu_fops = {
	.owner = THIS_MODULE,
	.open = npu_open,
	.release = npu_close,
	.unlocked_ioctl = npu_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = npu_compat_ioctl,
#endif
};

static int npu_parse_dt_clock(struct npu_device_t *npu_dev)
{
	u32 i = 0, rc = 0;
	const char *clock_name;
	int num_clk;
	struct npu_clk_t *core_clks = npu_dev->core_clks;
	struct platform_device *pdev = npu_dev->pdev;

	num_clk = of_property_count_strings(pdev->dev.of_node,
			"clock-names");
	if (num_clk <= 0) {
		pr_err("clocks are not defined\n");
		goto clk_err;
	}
	if (num_clk > NPU_MAX_CLK_NUM) {
		pr_err("clock number is over the limit %d\n", num_clk);
		num_clk = NPU_MAX_CLK_NUM;
	}

	npu_dev->core_clk_num = num_clk;
	for (i = 0; i < num_clk; i++) {
		of_property_read_string_index(pdev->dev.of_node, "clock-names",
							i, &clock_name);
		strlcpy(core_clks[i].clk_name, clock_name,
				sizeof(core_clks[i].clk_name));
		core_clks[i].clk = devm_clk_get(&pdev->dev, clock_name);
		if (IS_ERR(core_clks[i].clk)) {
			pr_err("unable to get clk: %s\n", clock_name);
			rc = -EINVAL;
			break;
		}
	}

clk_err:
	return rc;
}

static int npu_parse_dt_regulator(struct npu_device_t *npu_dev)
{
	u32 i = 0, rc = 0;
	const char *name;
	int num;
	struct npu_regulator_t *regulators = npu_dev->regulators;
	struct platform_device *pdev = npu_dev->pdev;

	num = of_property_count_strings(pdev->dev.of_node,
			"qcom,proxy-reg-names");
	if (num <= 0) {
		pr_err("regulator not defined\n");
		goto regulator_err;
	}
	if (num > NPU_MAX_REGULATOR_NUM) {
		pr_err("regulator number is over the limit %d", num);
		num = NPU_MAX_REGULATOR_NUM;
	}

	npu_dev->regulator_num = num;
	for (i = 0; i < num; i++) {
		of_property_read_string_index(pdev->dev.of_node,
			"qcom,proxy-reg-names", i, &name);
		strlcpy(regulators[i].regulator_name, name,
				sizeof(regulators[i].regulator_name));
		regulators[i].regulator = devm_regulator_get(&pdev->dev, name);
		if (IS_ERR(regulators[i].regulator)) {
			pr_err("unable to get regulator: %s\n", name);
			rc = -EINVAL;
			break;
		}
	}

regulator_err:
	return rc;
}

static int npu_probe(struct platform_device *pdev)
{
	int rc;
	struct resource *res;
	struct npu_device_t *npu_dev;

	npu_dev = devm_kzalloc(&pdev->dev,
		sizeof(struct npu_device_t), GFP_KERNEL);
	if (!npu_dev)
		return -EFAULT;

	npu_dev->pdev = pdev;
	platform_set_drvdata(pdev, npu_dev);

	res = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "npu_base");
	if (!res) {
		pr_err("unable to get NPU reg base address\n");
		rc = -ENOMEM;
		goto error_get_dev_num;
	}

	npu_dev->reg_size = resource_size(res);

	rc = npu_parse_dt_regulator(npu_dev);
	if (rc)
		goto error_get_dev_num;

	rc = npu_parse_dt_clock(npu_dev);
	if (rc)
		goto error_get_dev_num;

	npu_dev->npu_base = devm_ioremap(&pdev->dev, res->start,
					npu_dev->reg_size);
	if (unlikely(!npu_dev->npu_base)) {
		pr_err("unable to map NPU base\n");
		rc = -ENOMEM;
		goto error_get_dev_num;
	}
	npu_dev->npu_phys = res->start;
	pr_info("NPU HW Base phy_Address=0x%x virt=%pK\n",
		npu_dev->npu_phys, npu_dev->npu_base);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("unable to get NPU irq\n");
		rc = -ENOMEM;
		goto error_get_dev_num;
	}
	npu_dev->irq = res->start;

	/* character device might be optional */
	rc = alloc_chrdev_region(&npu_dev->dev_num, 0, 1, DRIVER_NAME);
	if (rc < 0) {
		pr_err("alloc_chrdev_region failed: %d\n", rc);
		goto error_get_dev_num;
	}

	npu_dev->class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(npu_dev->class)) {
		rc = PTR_ERR(npu_dev->class);
		pr_err("class_create failed: %d\n", rc);
		goto error_class_create;
	}

	npu_dev->device = device_create(npu_dev->class, NULL,
		npu_dev->dev_num, NULL, DRIVER_NAME);
	if (IS_ERR(npu_dev->device)) {
		rc = PTR_ERR(npu_dev->device);
		pr_err("device_create failed: %d\n", rc);
		goto error_class_device_create;
	}

	cdev_init(&npu_dev->cdev, &npu_fops);
	rc = cdev_add(&npu_dev->cdev,
			MKDEV(MAJOR(npu_dev->dev_num), 0), 1);
	if (rc < 0) {
		pr_err("cdev_add failed %d\n", rc);
		goto error_cdev_add;
	}

	rc = sysfs_create_group(&npu_dev->device->kobj, &npu_fs_attr_group);
	if (rc) {
		pr_err("unable to register npu sysfs nodes\n");
		goto error_res_init;
	}
	return rc;
error_res_init:
	cdev_del(&npu_dev->cdev);
error_cdev_add:
	device_destroy(npu_dev->class, npu_dev->dev_num);
error_class_device_create:
	class_destroy(npu_dev->class);
error_class_create:
	unregister_chrdev_region(npu_dev->dev_num, 1);
error_get_dev_num:
	return rc;
}

static int npu_remove(struct platform_device *pdev)
{
	struct npu_device_t *npu_dev;

	npu_dev = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	sysfs_remove_group(&npu_dev->device->kobj, &npu_fs_attr_group);
	cdev_del(&npu_dev->cdev);
	device_destroy(npu_dev->class, npu_dev->dev_num);
	class_destroy(npu_dev->class);
	unregister_chrdev_region(npu_dev->dev_num, 1);
	return 0;
}

static const struct of_device_id npu_dt_match[] = {
	{ .compatible = "qcom,msm-npu",},
	{}
};

MODULE_DEVICE_TABLE(of, npu_dt_match);

static struct platform_driver npu_driver = {
	.probe = npu_probe,
	.remove = npu_remove,
	.driver = {
		.name = "msm_npu",
		.owner = THIS_MODULE,
		.of_match_table = npu_dt_match,
		.pm = NULL,
	},
};

static int __init npu_init(void)
{
	int rc;

	rc = platform_driver_register(&npu_driver);
	if (rc)
		pr_err("npu register failed %d", rc);
	return rc;
}

static void __exit npu_exit(void)
{
	platform_driver_unregister(&npu_driver);
}

module_init(npu_init);
module_exit(npu_exit);

MODULE_DESCRIPTION("MSM NPU driver");
MODULE_LICENSE("GPL v2");
