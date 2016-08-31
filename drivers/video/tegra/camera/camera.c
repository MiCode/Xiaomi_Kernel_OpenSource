/*
 * drivers/video/tegra/camera/camera.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/export.h>

#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "camera_priv_defs.h"
#include "camera_clk.h"
#include "camera_power.h"
#include "camera_emc.h"
#include "camera_irq.h"

#define TEGRA_CAMERA_NAME "tegra_camera"

static struct clock_data clock_init[] = {
	{ CAMERA_ISP_CLK, "isp", true, 0},
	{ CAMERA_VI_CLK, "vi", true, 0},
	{ CAMERA_CSI_CLK, "csi", true, 0},
	{ CAMERA_EMC_CLK, "emc", true, 0},
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	{ CAMERA_CILAB_CLK, "cilab", true, 0},
	{ CAMERA_CILE_CLK, "cile", true, 0},
	{ CAMERA_PLL_D2_CLK, "pll_d2", false, 0},
#endif
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
	{ CAMERA_CILCD_CLK, "cilcd", true, 0},
#endif
	{ CAMERA_SCLK, "sclk", true, 80000000},
};

static int vi_out0_show(struct seq_file *s, void *unused)
{
	struct tegra_camera *camera = s->private;

	seq_printf(s, "overflow: %u\n",
		atomic_read(&(camera->vi_out0.overflow)));

	return 0;
}

static int vi_out0_open(struct inode *inode, struct file *file)
{
	return single_open(file, vi_out0_show, inode->i_private);
}

static int vi_out1_show(struct seq_file *s, void *unused)
{
	struct tegra_camera *camera = s->private;
	seq_printf(s, "overflow: %u\n",
		atomic_read(&(camera->vi_out1.overflow)));

	return 0;
}

static int vi_out1_open(struct inode *inode, struct file *file)
{
	return single_open(file, vi_out1_show, inode->i_private);
}

static const struct file_operations vi_out0_fops = {
	.open		= vi_out0_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations vi_out1_fops = {
	.open		= vi_out1_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void tegra_camera_remove_debugfs(struct tegra_camera *camera)
{
	if (camera->debugdir)
		debugfs_remove_recursive(camera->debugdir);
	camera->debugdir = NULL;
}

static void tegra_camera_create_debugfs(struct tegra_camera *camera)
{
	struct dentry *ret;

	camera->debugdir = debugfs_create_dir(TEGRA_CAMERA_NAME, NULL);
	if (!camera->debugdir) {
		dev_err(camera->dev, "%s: failed to create %s directory",
			__func__, TEGRA_CAMERA_NAME);
		goto create_debugfs_fail;
	}

	ret = debugfs_create_file("vi_out0", S_IRUGO,
			camera->debugdir, camera, &vi_out0_fops);
	if (!ret) {
		dev_err(camera->dev, "%s: failed to create vi_out0", __func__);
		goto create_debugfs_fail;
	}

	ret = debugfs_create_file("vi_out1", S_IRUGO,
			camera->debugdir, camera, &vi_out1_fops);
	if (!ret) {
		dev_err(camera->dev, "%s: failed to create vi_out1", __func__);
		goto create_debugfs_fail;
	}

	return;

create_debugfs_fail:
	dev_err(camera->dev, "%s: could not create debugfs", __func__);
	tegra_camera_remove_debugfs(camera);
}

static long tegra_camera_ioctl(struct file *file,
			       unsigned int cmd, unsigned long arg)
{
	uint id;
	struct tegra_camera *camera = file->private_data;

	/* first element of arg must be u32 with id of module to talk to */
	if (copy_from_user(&id, (const void __user *)arg, sizeof(uint))) {
		dev_err(camera->dev,
				"%s: Failed to copy arg from user", __func__);
		return -EFAULT;
	}

	if (id >= TEGRA_CAMERA_MODULE_MAX) {
		dev_err(camera->dev,
				"%s: Invalid id to tegra isp ioctl%d\n",
				__func__, id);
		return -EINVAL;
	}

	switch (cmd) {
	/*
	 * Clock enable/disable and reset should be handled in kernel.
	 * In order to support legacy code in user space, we don't remove
	 * these IOCTL.
	 */
	case TEGRA_CAMERA_IOCTL_ENABLE:
	case TEGRA_CAMERA_IOCTL_DISABLE:
	case TEGRA_CAMERA_IOCTL_RESET:
		return 0;
	case TEGRA_CAMERA_IOCTL_CLK_SET_RATE:
	{
		int ret;

		if (copy_from_user(&camera->info, (const void __user *)arg,
				   sizeof(struct tegra_camera_clk_info))) {
			dev_err(camera->dev,
				"%s: Failed to copy arg from user\n", __func__);
			return -EFAULT;
		}
		ret = tegra_camera_clk_set_rate(camera);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)arg, &camera->info,
				 sizeof(struct tegra_camera_clk_info))) {
			dev_err(camera->dev,
				"%s: Failed to copy arg to user\n", __func__);
			return -EFAULT;
		}
		return 0;
	}
	default:
		dev_err(camera->dev,
				"%s: Unknown tegra_camera ioctl.\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int tegra_camera_open(struct inode *inode, struct file *file)
{
	int ret;
	struct miscdevice *miscdev = file->private_data;
	struct tegra_camera *camera = container_of(miscdev,
						struct tegra_camera,
						misc_dev);

	dev_info(camera->dev, "%s: ++\n", __func__);

	if (atomic_xchg(&camera->in_use, 1))
		return -EBUSY;

	file->private_data = camera;

	mutex_lock(&camera->tegra_camera_lock);

	/* turn on CSI regulator */
	ret = tegra_camera_power_on(camera);
	if (ret)
		goto power_on_fail;
	/* set EMC request */
	ret = tegra_camera_enable_emc(camera);
	if (ret)
		goto enable_emc_fail;

	/* read initial clock info */
	tegra_camera_init_clk(camera, clock_init);

	/* enable camera HW clock */
	ret = tegra_camera_enable_clk(camera);
	if (ret)
		goto enable_clk_fail;

	ret = tegra_camera_enable_irq(camera);
	if (ret)
		goto enable_irq_fail;

	mutex_unlock(&camera->tegra_camera_lock);

	return 0;

enable_irq_fail:
	tegra_camera_disable_clk(camera);
enable_clk_fail:
	tegra_camera_disable_emc(camera);
enable_emc_fail:
	tegra_camera_power_off(camera);
power_on_fail:
	mutex_unlock(&camera->tegra_camera_lock);
	return ret;
}

static int tegra_camera_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct tegra_camera *camera = file->private_data;

	dev_info(camera->dev, "%s++\n", __func__);

	mutex_lock(&camera->tegra_camera_lock);

	ret = tegra_camera_disable_irq(camera);
	if (ret)
		goto release_exit;
	/* disable HW clock */
	ret = tegra_camera_disable_clk(camera);
	if (ret)
		goto release_exit;
	/* nullify EMC request */
	ret = tegra_camera_disable_emc(camera);
	if (ret)
		goto release_exit;
	/* turn off CSI regulator */
	ret = tegra_camera_power_off(camera);
	if (ret)
		goto release_exit;

release_exit:
	mutex_unlock(&camera->tegra_camera_lock);
	WARN_ON(!atomic_xchg(&camera->in_use, 0));
	return ret;
}

static const struct file_operations tegra_camera_fops = {
	.owner = THIS_MODULE,
	.open = tegra_camera_open,
	.unlocked_ioctl = tegra_camera_ioctl,
	.release = tegra_camera_release,
};

static int tegra_camera_clk_get(struct platform_device *ndev, const char *name,
				struct clk **clk)
{
	*clk = clk_get(&ndev->dev, name);
	if (IS_ERR_OR_NULL(*clk)) {
		dev_err(&ndev->dev, "%s: unable to get clock for %s\n",
			__func__, name);
		*clk = NULL;
		return PTR_ERR(*clk);
	}
	return 0;
}

struct tegra_camera *tegra_camera_register(struct platform_device *ndev)
{
	struct tegra_camera *camera = NULL;
	int ret = 0;
	int i;

	dev_info(&ndev->dev, "%s: ++\n", __func__);

	camera = kzalloc(sizeof(struct tegra_camera), GFP_KERNEL);
	if (!camera) {
		dev_err(&ndev->dev, "can't allocate memory for tegra_camera\n");
		return camera;
	}

	mutex_init(&camera->tegra_camera_lock);

	/* Powergate VE when boot */
	mutex_lock(&camera->tegra_camera_lock);
#ifndef CONFIG_ARCH_TEGRA_2x_SOC

	ret = tegra_camera_powergate_init(camera);
	if (ret) {
		mutex_unlock(&camera->tegra_camera_lock);
		goto regulator_fail;
	}
#endif
	mutex_unlock(&camera->tegra_camera_lock);

	camera->dev = &ndev->dev;

	/* Get regulator pointer */
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	camera->reg = regulator_get(&ndev->dev, "vcsi");
#else
	camera->reg = regulator_get(&ndev->dev, "avdd_dsi_csi");
#endif

	if (IS_ERR(camera->reg)) {
		ret = -ENODEV;
		if (camera->reg == ERR_PTR(-ENODEV)) {
			camera->reg = NULL;
			dev_info(&ndev->dev,
				"%s: no regulator device, overriding\n",
							__func__);
		} else {
			dev_err(&ndev->dev, "%s: couldn't get regulator\n",
							__func__);
			goto regulator_fail;
		}
	}

	camera->misc_dev.minor = MISC_DYNAMIC_MINOR;
	camera->misc_dev.name = TEGRA_CAMERA_NAME;
	camera->misc_dev.fops = &tegra_camera_fops;
	camera->misc_dev.parent = &ndev->dev;
	ret = misc_register(&camera->misc_dev);
	if (ret) {
		dev_err(&ndev->dev, "%s: unable to register misc device!\n",
			TEGRA_CAMERA_NAME);
		goto misc_register_fail;
	}

	for (i = 0; i < ARRAY_SIZE(clock_init); i++) {
		ret = tegra_camera_clk_get(ndev, clock_init[i].name,
				&camera->clock[clock_init[i].index].clk);
		if (ret)
			goto clk_get_fail;
	}

	ret = tegra_camera_isomgr_register(camera);
	if (ret)
		goto clk_get_fail;

	/* Init intterupt bottom half */
	INIT_WORK(&camera->stats_work, tegra_camera_stats_worker);

	ret = tegra_camera_intr_init(camera);
	if (ret)
		goto intr_init_fail;

	tegra_camera_create_debugfs(camera);

	return camera;

intr_init_fail:
	tegra_camera_isomgr_unregister(camera);
clk_get_fail:
	while (i--)
		clk_put(camera->clock[clock_init[i].index].clk);
	misc_deregister(&camera->misc_dev);
misc_register_fail:
	regulator_put(camera->reg);
regulator_fail:
	kfree(camera);
	camera = NULL;
	return camera;
}
EXPORT_SYMBOL(tegra_camera_register);

int tegra_camera_unregister(struct tegra_camera *camera)
{
	int i;
	int ret;

	dev_info(camera->dev, "%s: ++\n", __func__);

	tegra_camera_remove_debugfs(camera);

	/* Free IRQ */
	tegra_camera_intr_free(camera);

	for (i = 0; i < CAMERA_CLK_MAX; i++)
		clk_put(camera->clock[i].clk);
	tegra_camera_isomgr_unregister(camera);

	ret = misc_deregister(&camera->misc_dev);
	if (ret)
		dev_err(camera->dev, "deregister misc dev fail, %d\n", ret);

	if (camera->reg)
		regulator_put(camera->reg);

	kfree(camera);

	return 0;
}
EXPORT_SYMBOL(tegra_camera_unregister);

#ifdef CONFIG_PM
int tegra_camera_suspend(struct tegra_camera *camera)
{
	int ret = 0;

	dev_dbg(camera->dev, "%s: ++\n", __func__);
	mutex_lock(&camera->tegra_camera_lock);
	if (camera->power_on) {
		ret = -EBUSY;
		dev_err(camera->dev,
		"tegra_camera cannot suspend, "
		"application is holding on to camera.\n");
	}
	mutex_unlock(&camera->tegra_camera_lock);

	return ret;
}
EXPORT_SYMBOL(tegra_camera_suspend);

int tegra_camera_resume(struct tegra_camera *camera)
{
	dev_info(camera->dev, "%s: ++\n", __func__);
	return 0;
}
EXPORT_SYMBOL(tegra_camera_resume);
#endif
