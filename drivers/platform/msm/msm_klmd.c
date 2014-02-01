/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <mach/rpm-regulator.h>
#include <linux/io.h>
#include <linux/clk/msm-clk.h>
#include <linux/debugfs.h>

#define TSPP2_NUM_KEY_TABLES	32

static int klm_fs_open(struct inode *ip, struct file *fp);
static int klm_fs_release(struct inode *ip, struct file *fp);
static int klm_dev_open(void);
static int klm_dev_close(void);

static const char klm_name[] = "klm";

struct debugfs_entry {
	const char *name;
	mode_t mode;
	int offset;
};

/**
 * struct klm_device - KLM device
 *
 * @pdev: Platform device.
 * @dev: Device structure, used for driver prints.
 * @opened: A flag to indicate whether the KLM device is opened.
 * @mutex: Mutex for accessing global structures.
 * @gdsc: GDSC power regulator.
 * @tspp2_core_clk: TSPP2 core clock.
 * @tspp2_ahb_clk: TSPP2 AHB clock.
 * @tspp2_klm_ahb_clk: TSPP2 KLM AHB clock.
 * @bcss_hlos_base:	BCSS HLOS memory base address.
 * @bcss_klm_base:	BCSS KLM memory base address.
 * @bcss_tspp2_base:	BCSS TSPP2 memory base address.
 * @debugfs_entry: KLM device debugfs entry.
 */
struct klm_device {
	struct platform_device *pdev;
	struct device *dev;
	int opened;
	struct mutex mutex;
	struct regulator *gdsc;
	struct clk *tspp2_core_clk;
	struct clk *tspp2_ahb_clk;
	struct clk *tspp2_klm_ahb_clk;
	void __iomem *bcss_hlos_base;
	void __iomem *bcss_klm_base;
	void __iomem *bcss_tspp2_base;
	struct dentry *debugfs_entry;
};

/* file operations for KLM device */
static const struct file_operations klm_fops = {
	.open = klm_fs_open,
	.release = klm_fs_release,
};

static struct miscdevice klm_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = klm_name,
	.fops = &klm_fops,
};

static struct klm_device *klm_device_context;

/**
 * klm_clock_start() - Enable the required KLM clocks
 *
 * @device:	The KLM device.
 *
 * Return 0 on success, error value otherwise.
 */
static int klm_clock_start(struct klm_device *device)
{
	int tspp2_core_clk = 0;
	int tspp2_klm_ahb_clk = 0;
	int rc;

	if (device == NULL) {
		pr_err("%s: Can't start clocks, invalid device\n", __func__);
		return -EINVAL;
	}

	rc = regulator_enable(device->gdsc);
	if (rc) {
		pr_err("%s: Can't enable regulator\n", __func__);
		goto err_regulator;
	}

	if (device->tspp2_core_clk) {
		if (clk_prepare_enable(device->tspp2_core_clk) != 0) {
			pr_err("%s: Can't start tspp2_core_clk\n", __func__);
			goto err_clocks;
		}
		tspp2_core_clk = 1;
	}

	if (device->tspp2_klm_ahb_clk) {
		if (clk_prepare_enable(device->tspp2_klm_ahb_clk) != 0) {
			pr_err("%s: Can't start tspp2_klm_ahb_clk\n", __func__);
			goto err_clocks;
		}
		tspp2_klm_ahb_clk = 1;
	}

	return 0;

err_clocks:
	if (tspp2_core_clk)
		clk_disable_unprepare(device->tspp2_core_clk);

	if (tspp2_klm_ahb_clk)
		clk_disable_unprepare(device->tspp2_klm_ahb_clk);

	if (regulator_disable(device->gdsc))
		pr_err("%s: Error disabling power regulator\n", __func__);

err_regulator:
	return -EBUSY;
}

/**
 * klm_clock_stop() - Disable KLM clocks
 *
 * @device:	The KLM device.
 */
static void klm_clock_stop(struct klm_device *device)
{
	if (device == NULL) {
		pr_err("%s: Can't stop clocks, invalid device\n", __func__);
		return;
	}

	if (device->tspp2_klm_ahb_clk)
		clk_disable_unprepare(device->tspp2_klm_ahb_clk);

	if (device->tspp2_core_clk)
		clk_disable_unprepare(device->tspp2_core_clk);

	if (regulator_disable(device->gdsc))
		pr_err("%s: Error disabling power regulator\n", __func__);
}

/* Platform driver related functions: */

/**
 * klm_clocks_put() - Put clocks and disable regulator.
 *
 * @device:	KLM device.
 */
static void klm_clocks_put(struct klm_device *device)
{
	if (device->tspp2_klm_ahb_clk)
		clk_put(device->tspp2_klm_ahb_clk);

	if (device->tspp2_core_clk)
		clk_put(device->tspp2_core_clk);

	device->tspp2_core_clk = NULL;
	device->tspp2_klm_ahb_clk = NULL;
}

/**
 * msm_klm_clocks_setup() - Get clocks and set their rate, enable regulator.
 *
 * @pdev:	Platform device, containing platform information.
 * @device:	KLM device.
 *
 * Return 0 on success, error value otherwise.
 */
static int msm_klm_clocks_setup(struct platform_device *pdev,
						struct klm_device *device)
{
	int ret = 0;
	unsigned long rate_in_hz = 0;
	struct clk *tspp2_core_clk_src = NULL;

	/* Get power regulator (GDSC) */
	device->gdsc = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(device->gdsc)) {
		pr_err("%s: Failed to get vdd power regulator\n", __func__);
		ret = PTR_ERR(device->gdsc);
		device->gdsc = NULL;
		return ret;
	}

	device->tspp2_ahb_clk = NULL;
	device->tspp2_core_clk = NULL;
	device->tspp2_klm_ahb_clk = NULL;

	device->tspp2_ahb_clk =
		clk_get(&pdev->dev, "bcc_tspp2_ahb_clk");
	if (IS_ERR(device->tspp2_ahb_clk)) {
		pr_err("%s: Failed to get %s", __func__, "bcc_tspp2_ahb_clk");
		ret = PTR_ERR(device->tspp2_ahb_clk);
		device->tspp2_ahb_clk = NULL;
		goto err_clocks;
	}

	device->tspp2_core_clk =
		clk_get(&pdev->dev, "bcc_tspp2_core_clk");
	if (IS_ERR(device->tspp2_core_clk)) {
		pr_err("%s: Failed to get %s", __func__, "bcc_tspp2_core_clk");
		ret = PTR_ERR(device->tspp2_core_clk);
		device->tspp2_core_clk = NULL;
		goto err_clocks;
	}

	device->tspp2_klm_ahb_clk =
		clk_get(&pdev->dev, "bcc_klm_ahb_clk");
	if (IS_ERR(device->tspp2_klm_ahb_clk)) {
		pr_err("%s: Failed to get %s", __func__, "bcc_klm_ahb_clk");
		ret = PTR_ERR(device->tspp2_klm_ahb_clk);
		device->tspp2_klm_ahb_clk = NULL;
		goto err_clocks;
	}

	/* We need to set the rate of tspp2_core_clk_src */
	tspp2_core_clk_src = clk_get_parent(device->tspp2_core_clk);
	if (tspp2_core_clk_src) {
		rate_in_hz = clk_round_rate(tspp2_core_clk_src, 1);
		if (clk_set_rate(tspp2_core_clk_src, rate_in_hz)) {
			pr_err("%s: Failed to set rate %lu to tspp2_core_clk_src\n",
				__func__, rate_in_hz);
			goto err_clocks;
		}
	} else {
		pr_err("%s: Failed to get tspp2_core_clk parent\n", __func__);
		goto err_clocks;
	}

	return 0;

err_clocks:
	klm_clocks_put(device);

	return ret;
}

/**
 * msm_klm_map_io_memory() - Map memory resources to kernel
 * space.
 *
 * @pdev:	Platform device, containing platform information.
 * @device:	KLM device.
 *
 * Return 0 on success, error value otherwise.
 */
static int msm_klm_map_io_memory(struct platform_device *pdev,
						struct klm_device *device)
{
	struct resource *mem_bcss_klm;
	struct resource *mem_bcss_hlos;
	struct resource *mem_bcss_tspp2;

	/* Get memory resources */
	mem_bcss_klm = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "MSM_BCSS_KLM");
	if (!mem_bcss_klm) {
		dev_err(&pdev->dev, "%s: Missing BCSS_KLM MEM resource",
			__func__);
		return -ENXIO;
	}

	mem_bcss_hlos = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "MSM_BCSS_HLOS");
	if (!mem_bcss_hlos) {
		dev_err(&pdev->dev, "%s: Missing BCSS_HLOS MEM resource",
			__func__);
		return -ENXIO;
	}

	mem_bcss_tspp2 = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "MSM_BCSS_TSPP2");
	if (!mem_bcss_tspp2) {
		dev_err(&pdev->dev, "%s: Missing BCSS_TSPP2 MEM resource",
			__func__);
		return -ENXIO;
	}

	/* Map memory physical addresses to kernel space */
	device->bcss_klm_base = ioremap(mem_bcss_klm->start,
		resource_size(mem_bcss_klm));
	if (!device->bcss_klm_base) {
		dev_err(&pdev->dev, "%s: ioremap failed", __func__);
		goto err_map_dev_klm;
	}

	device->bcss_hlos_base = ioremap(mem_bcss_hlos->start,
		resource_size(mem_bcss_hlos));
	if (!device->bcss_hlos_base) {
		dev_err(&pdev->dev, "%s: ioremap failed", __func__);
		goto err_map_dev_hlos;
	}

	device->bcss_tspp2_base = ioremap(mem_bcss_tspp2->start,
		resource_size(mem_bcss_tspp2));
	if (!device->bcss_tspp2_base) {
		dev_err(&pdev->dev, "%s: ioremap failed", __func__);
		goto err_map_dev_tspp2;
	}

	return 0;

err_map_dev_tspp2:
	iounmap(device->bcss_hlos_base);

err_map_dev_hlos:
	iounmap(device->bcss_klm_base);

err_map_dev_klm:
	return -ENXIO;
}

static int debugfs_iomem_bcss_x32_set(void *data, u64 val)
{
	if (!klm_device_context->opened)
		return -ENODEV;

	if (klm_device_context->tspp2_ahb_clk) {
		if (clk_prepare_enable(klm_device_context->tspp2_ahb_clk)
			!= 0) {
			pr_err("%s: Can't start tspp2_ahb_clk\n", __func__);
			return -EBUSY;
		}
	}

	writel_relaxed(val, data);
	wmb();

	clk_disable_unprepare(klm_device_context->tspp2_ahb_clk);

	return 0;
}

static int debugfs_iomem_bcss_x32_get(void *data, u64 *val)
{
	if (!klm_device_context->opened)
		return -ENODEV;

	if (klm_device_context->tspp2_ahb_clk) {
		if (clk_prepare_enable(klm_device_context->tspp2_ahb_clk)
			!= 0) {
			pr_err("%s: Can't start tspp2_ahb_clk\n", __func__);
			return -EBUSY;
		}
	}

	*val = readl_relaxed(data);

	clk_disable_unprepare(klm_device_context->tspp2_ahb_clk);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_iomem_bcss_x32, debugfs_iomem_bcss_x32_get,
			debugfs_iomem_bcss_x32_set, "0x%08llX");

static int debugfs_dev_open_set(void *data, u64 val)
{
	int ret = 0;

	if (val == 1)
		ret = klm_dev_open();
	else
		ret = klm_dev_close();

	return ret;
}

static int debugfs_dev_open_get(void *data, u64 *val)
{
	if (!klm_device_context)
		return -ENODEV;

	*val = klm_device_context->opened;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_device_open, debugfs_dev_open_get,
			debugfs_dev_open_set, "0x%08llX");

/**
 * debugfs service to print key table information.
 */
static int key_table_debugfs_print(struct seq_file *s, void *p)
{
	int i = (int)(s->private);
	uint32_t *puint32;

	if (!klm_device_context->opened)
		return -ENODEV;

	if (klm_device_context->tspp2_ahb_clk) {
		if (clk_prepare_enable(klm_device_context->tspp2_ahb_clk)
			!= 0) {
			pr_err("%s: Can't start tspp2_ahb_clk\n", __func__);
			return -EBUSY;
		}
	}

	/* Enable debug mode */
	writel_relaxed(1, klm_device_context->bcss_hlos_base + 0xC);

	puint32 = (uint32_t *)(klm_device_context->bcss_klm_base + i*64);
	seq_printf(s, "CW even: 0x%x 0x%x 0x%x 0x%x\n",
		*(puint32), *(puint32 + 1),
		*(puint32 + 2), *(puint32 + 3));

	puint32 = (uint32_t *)(klm_device_context->bcss_klm_base + i*64 + 16);
	seq_printf(s, "IV even: 0x%x 0x%x 0x%x 0x%x\n",
		*(puint32), *(puint32 + 1),
		*(puint32 + 2), *(puint32 + 3));

	puint32 = (uint32_t *)(klm_device_context->bcss_klm_base + i*64 + 32);
	seq_printf(s, "CW odd: 0x%x 0x%x 0x%x 0x%x\n",
		*(puint32), *(puint32 + 1),
		*(puint32 + 2), *(puint32 + 3));

	puint32 = (uint32_t *)(klm_device_context->bcss_klm_base + i*64 + 48);
	seq_printf(s, "IV odd: 0x%x 0x%x 0x%x 0x%x\n",
		*(puint32), *(puint32 + 1),
		*(puint32 + 2), *(puint32 + 3));

	puint32 = (uint32_t *)(klm_device_context->bcss_klm_base + 32*64 + 8*i);
	seq_printf(s, "UR: 0x%04x\n", *(puint32));
	seq_printf(s, "Config: 0x%04x\n", *(puint32 + 1));

	/* Disable debug mode */
	writel_relaxed(0, klm_device_context->bcss_hlos_base + 0xC);

	clk_disable_unprepare(klm_device_context->tspp2_ahb_clk);

	return 0;
}

static int key_table_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, key_table_debugfs_print, inode->i_private);
}

static const struct file_operations dbgfs_key_table_fops = {
	.open = key_table_dbgfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

/**
 * klm_debugfs_init() - KLM device debugfs initialization.
 *
 * @device:	KLM device.
 */
static void klm_debugfs_init(struct klm_device *device)
{
	int i;
	char name[80];
	struct dentry *dentry;

	device->debugfs_entry = debugfs_create_dir("klm", NULL);

	if (!device->debugfs_entry)
		return;

	/* Support device open/close */
	debugfs_create_file("open", S_IRUGO | S_IWUSR, device->debugfs_entry,
		NULL, &fops_device_open);

	/* Directory for accessing BCSS HLOS registers */
	dentry = debugfs_create_dir("bcss_hlos", device->debugfs_entry);
	if (dentry) {
		debugfs_create_file("BCSS_TEST_BUS_CFG", S_IRUGO | S_IWUSR,
			dentry, device->bcss_hlos_base + 0x000C,
			&fops_iomem_bcss_x32);
	}

	/* Directory for accessing TSPP2 registers */
	dentry = debugfs_create_dir("bcss_tspp2", device->debugfs_entry);
	if (dentry) {
		debugfs_create_file("TSPP2_VERSION", S_IRUGO, dentry,
			device->bcss_tspp2_base + 0x6FFC, &fops_iomem_bcss_x32);
	}

	/* Directory for accessing key tables */
	dentry = debugfs_create_dir("key_tables", device->debugfs_entry);
	if (dentry) {
		for (i = 0; i < TSPP2_NUM_KEY_TABLES; i++) {
			snprintf(name, 20, "key_table%02i", i);
			debugfs_create_file(name, S_IRUGO, dentry,
				(void *)i, &dbgfs_key_table_fops);
		}
	}
}

/**
 * klm_debugfs_exit() - TSPP2 device debugfs teardown.
 *
 * @device:	KLM device.
 */
static void klm_debugfs_exit(struct klm_device *device)
{
	debugfs_remove_recursive(device->debugfs_entry);
	device->debugfs_entry = NULL;
}

/* Device driver probe function */
static int msm_klm_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct klm_device *device;

	device = devm_kzalloc(&pdev->dev, sizeof(struct klm_device),
		GFP_KERNEL);
	if (!device) {
		pr_err("%s: Failed to allocate memory for device\n", __func__);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, device);
	device->pdev = pdev;
	device->dev = &pdev->dev;

	klm_device_context = device;

	rc = msm_klm_clocks_setup(pdev, device);
	if (rc) {
		pr_err("%s: Failed to setup clocks\n", __func__);
		goto quit;
	}

	rc = msm_klm_map_io_memory(pdev, device);
	if (rc)
		goto err_put_clocks;

	rc = klm_clock_start(device);
	if (rc) {
		pr_err("%s: Failed to start clocks\n", __func__);
		goto err_unmap_io_memory;
	}

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	mutex_init(&device->mutex);

	klm_debugfs_init(device);

	rc = misc_register(&klm_dev);
	if (rc)
		goto err_mutex_destroy;

	goto quit;

err_mutex_destroy:
	klm_debugfs_exit(device);
	mutex_destroy(&device->mutex);
	klm_clock_stop(device);

err_unmap_io_memory:
	/* Unmap memory */
	iounmap(device->bcss_klm_base);
	iounmap(device->bcss_hlos_base);
	iounmap(device->bcss_tspp2_base);

err_put_clocks:
	klm_clocks_put(device);

quit:
	return rc;
}

/* Device driver remove function */
static int msm_klm_remove(struct platform_device *pdev)
{
	struct klm_device *device = platform_get_drvdata(pdev);

	klm_debugfs_exit(device);

	mutex_destroy(&device->mutex);
	klm_clocks_put(device);

	klm_device_context = NULL;

	/* Unmap memory */
	iounmap(device->bcss_klm_base);
	iounmap(device->bcss_hlos_base);
	iounmap(device->bcss_tspp2_base);

	misc_deregister(&klm_dev);

	return 0;
}

static int klm_dev_open(void)
{
	int ret;

	if (!klm_device_context)
		return -ENODEV;

	ret = pm_runtime_get_sync(klm_device_context->dev);
	if (ret < 0)
		return ret;
	else {
		klm_device_context->opened = 1;
		return 0;
	}
}

static int klm_dev_close(void)
{
	int ret;

	if (!klm_device_context)
		return -ENODEV;

	ret = pm_runtime_put_sync(klm_device_context->dev);
	if (ret < 0)
		return ret;
	else {
		klm_device_context->opened = 0;
		return 0;
	}
}

/* KLM DEV operations */

static int klm_fs_open(struct inode *ip, struct file *fp)
{
	return klm_dev_open();
}

static int klm_fs_release(struct inode *ip, struct file *fp)
{
	return klm_dev_close();
}

/* Power Management */

static int klm_runtime_suspend(struct device *dev)
{
	struct klm_device *device;
	struct platform_device *pdev;

	pdev = container_of(dev, struct platform_device, dev);
	device = platform_get_drvdata(pdev);

	mutex_lock(&device->mutex);

	klm_clock_stop(device);

	mutex_unlock(&device->mutex);

	dev_dbg(dev, "%s\n", __func__);

	return 0;
}

static int klm_runtime_resume(struct device *dev)
{

	int ret = 0;
	struct klm_device *device;
	struct platform_device *pdev;

	pdev = container_of(dev, struct platform_device, dev);
	device = platform_get_drvdata(pdev);

	mutex_lock(&device->mutex);

	ret = klm_clock_start(device);

	mutex_unlock(&device->mutex);

	dev_dbg(dev, "%s\n", __func__);

	return ret;
}

static const struct dev_pm_ops klm_dev_pm_ops = {
	.runtime_suspend = klm_runtime_suspend,
	.runtime_resume = klm_runtime_resume,
};

/* Platform driver information */

static struct of_device_id msm_klm_match_table[] = {
	{.compatible = "qcom,klm"},
	{}
};

static struct platform_driver msm_klm_driver = {
	.probe          = msm_klm_probe,
	.remove         = msm_klm_remove,
	.driver         = {
		.name   = "msm-klm",
		.pm     = &klm_dev_pm_ops,
		.of_match_table = msm_klm_match_table,
	},
};

/**
 * klm_module_init() - KLM driver module init function.
 *
 * Return 0 on success, error value otherwise.
 */
static int __init klm_module_init(void)
{
	int rc;

	rc = platform_driver_register(&msm_klm_driver);
	if (rc)
		pr_err("%s: platform_driver_register failed: %d\n",
			__func__, rc);

	return rc;
}

/**
 * klm_module_exit() - KLM driver module exit function.
 */
static void __exit klm_module_exit(void)
{
	platform_driver_unregister(&msm_klm_driver);
}

module_init(klm_module_init);
module_exit(klm_module_exit);

MODULE_DESCRIPTION("KLM (Key Ladder Module) platform device driver");
MODULE_LICENSE("GPL v2");
