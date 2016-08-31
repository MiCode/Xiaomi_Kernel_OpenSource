/*
 * drivers/video/tegra/host/vi/tegra_vi.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/nvhost.h>
#include <linux/nvhost_vi_ioctl.h>
#include <linux/platform_device.h>

#include <mach/latency_allowance.h>

#include "bus_client.h"
#include "chip_support.h"
#include "host1x/host1x.h"
#include "vi.h"

static DEFINE_MUTEX(la_lock);

#define T12_VI_CFG_CG_CTRL	0xb8
#define T12_CG_2ND_LEVEL_EN	1
#define T12_VI_CSI_0_SW_RESET	0x100
#define T12_VI_CSI_1_SW_RESET	0x200
#define T12_VI_CSI_SW_RESET_MCCIF_RESET 3

#ifdef TEGRA_12X_OR_HIGHER_CONFIG

int nvhost_vi_init(struct platform_device *dev)
{
	int ret = 0;
	struct vi *tegra_vi = nvhost_get_private_data(dev);

	tegra_vi->reg = regulator_get(&dev->dev, "avdd_dsi_csi");
	if (IS_ERR(tegra_vi->reg)) {
		if (tegra_vi->reg == ERR_PTR(-ENODEV)) {
			ret = -ENODEV;
			dev_info(&dev->dev,
					"%s: no regulator device\n",
					__func__);
		} else {
			dev_err(&dev->dev,
					"%s: couldn't get regulator\n",
					__func__);
		}
		tegra_vi->reg = NULL;
		return ret;
	}

	return 0;
}

void nvhost_vi_deinit(struct platform_device *dev)
{
	struct vi *tegra_vi = nvhost_get_private_data(dev);

	if (tegra_vi->reg) {
		regulator_put(tegra_vi->reg);
		tegra_vi->reg = NULL;
	}
}

int nvhost_vi_finalize_poweron(struct platform_device *dev)
{
	int ret = 0, dev_id;
	struct vi *tegra_vi;
	const char *devname = dev_name(&dev->dev);

	ret = sscanf(devname, "vi.%1d", &dev_id);
	if (ret != 1) {
		dev_err(&dev->dev, "Read dev_id failed!\n");
		return -ENODEV;
	}

	tegra_vi = (struct vi *)nvhost_get_private_data(dev);
	if (tegra_vi->reg) {
		ret = regulator_enable(tegra_vi->reg);
		if (ret) {
			dev_err(&dev->dev,
					"%s: enable csi regulator failed.\n",
					__func__);
			goto fail;
		}
	}

	/* Only do this for vi.0 not for slave device vi.1 */
	if (dev_id == 0)
		host1x_writel(dev, T12_VI_CFG_CG_CTRL, T12_CG_2ND_LEVEL_EN);

 fail:
	return ret;
}

int nvhost_vi_prepare_poweroff(struct platform_device *dev)
{
	int ret = 0;
	struct vi *tegra_vi;
	tegra_vi = (struct vi *)nvhost_get_private_data(dev);

	if (tegra_vi->reg) {
		ret = regulator_disable(tegra_vi->reg);
		if (ret) {
			dev_err(&dev->dev,
				"%s: disable csi regulator failed.\n",
				__func__);
			goto fail;
		}
	}
 fail:
	return ret;
}

#if defined(CONFIG_TEGRA_ISOMGR)
static int vi_set_isomgr_request(struct vi *tegra_vi, uint vi_bw, uint lt)
{
	int ret = 0;

	dev_dbg(&tegra_vi->ndev->dev,
		"%s++ bw=%u, lt=%u\n", __func__, vi_bw, lt);

	/* return value of tegra_isomgr_reserve is dvfs latency in usec */
	ret = tegra_isomgr_reserve(tegra_vi->isomgr_handle,
				vi_bw,	/* KB/sec */
				lt);	/* usec */
	if (!ret) {
		dev_err(&tegra_vi->ndev->dev,
		"%s: failed to reserve %u KBps\n", __func__, vi_bw);
		return -ENOMEM;
	}

	/* return value of tegra_isomgr_realize is dvfs latency in usec */
	ret = tegra_isomgr_realize(tegra_vi->isomgr_handle);
	if (ret)
		dev_dbg(&tegra_vi->ndev->dev,
		"%s: tegra_vi isomgr latency is %d usec",
		__func__, ret);
	else {
		dev_err(&tegra_vi->ndev->dev,
		"%s: failed to realize %u KBps\n", __func__, vi_bw);
			return -ENOMEM;
	}
	return ret;
}
#endif

static int vi_set_la(struct vi *tegra_vi1, uint vi_bw)
{
	struct nvhost_device_data *pdata_vi1, *pdata_vi2;
	struct vi *tegra_vi2;
	struct clk *clk_vi;
	int ret;
	uint total_vi_bw;

	pdata_vi1 =
		(struct nvhost_device_data *)tegra_vi1->ndev->dev.platform_data;

	if (!pdata_vi1)
	    return -ENODEV;

	/* Copy device data for other vi device */
	mutex_lock(&la_lock);

	tegra_vi1->vi_bw = vi_bw / 1000;
	total_vi_bw = tegra_vi1->vi_bw;
	if (pdata_vi1->master)
		pdata_vi2 = (struct nvhost_device_data *)
			pdata_vi1->master->dev.platform_data;
	else
		pdata_vi2 = (struct nvhost_device_data *)
			pdata_vi1->slave->dev.platform_data;

	tegra_vi2 = (struct vi *)pdata_vi2->private_data;

	clk_vi = clk_get(&tegra_vi2->ndev->dev, "emc");
	if (tegra_is_clk_enabled(clk_vi))
		total_vi_bw += tegra_vi2->vi_bw;

	mutex_unlock(&la_lock);

	ret = tegra_set_camera_ptsa(TEGRA_LA_VI_W, total_vi_bw, 1);

	return ret;
}


long vi_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct vi *tegra_vi;

	if (_IOC_TYPE(cmd) != NVHOST_VI_IOCTL_MAGIC)
		return -EFAULT;

	tegra_vi = file->private_data;
	switch (cmd) {
	case NVHOST_VI_IOCTL_ENABLE_TPG: {
		uint enable;
		int ret;
		struct clk *clk;

		if (copy_from_user(&enable,
			(const void __user *)arg, sizeof(uint))) {
			dev_err(&tegra_vi->ndev->dev,
				"%s: Failed to copy arg from user\n", __func__);
			return -EFAULT;
		}

		clk = clk_get(&tegra_vi->ndev->dev, "pll_d");
		if (enable)
			ret = tegra_clk_cfg_ex(clk,
				TEGRA_CLK_PLLD_CSI_OUT_ENB, 1);
		else
			ret = tegra_clk_cfg_ex(clk,
				TEGRA_CLK_MIPI_CSI_OUT_ENB, 1);
		clk_put(clk);

		return ret;
	}
	case NVHOST_VI_IOCTL_SET_EMC_INFO: {
		uint vi_bw;
		int ret;
		if (copy_from_user(&vi_bw,
			(const void __user *)arg, sizeof(uint))) {
			dev_err(&tegra_vi->ndev->dev,
				"%s: Failed to copy arg from user\n", __func__);
			return -EFAULT;
		}

		ret = vi_set_la(tegra_vi, vi_bw);
		if (ret) {
			dev_err(&tegra_vi->ndev->dev,
			"%s: failed to set la for vi_bw %u MBps\n",
			__func__, vi_bw/1000);
			return -ENOMEM;
		}

#if defined(CONFIG_TEGRA_ISOMGR)
		/*
		 * Set VI ISO BW requirements.
		 * There is no way to figure out what latency
		 * can be tolerated in VI without reading VI
		 * registers for now. 3 usec is minimum time
		 * to switch PLL source. Let's put 4 usec as
		 * latency for now.
		 */
		if (tegra_vi->isomgr_handle) {
			ret = vi_set_isomgr_request(tegra_vi, vi_bw, 4);

			if (!ret) {
				dev_err(&tegra_vi->ndev->dev,
				"%s: failed to reserve %u KBps\n",
				__func__, vi_bw);
				return -ENOMEM;
			}
		}
#endif
		return ret;
	}
	default:
		dev_err(&tegra_vi->ndev->dev,
			"%s: Unknown vi ioctl.\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int vi_open(struct inode *inode, struct file *file)
{
	struct nvhost_device_data *pdata;
	struct vi *vi;

	pdata = container_of(inode->i_cdev,
		struct nvhost_device_data, ctrl_cdev);
	if (WARN_ONCE(pdata == NULL, "pdata not found, %s failed\n", __func__))
		return -ENODEV;

	vi = (struct vi *)pdata->private_data;
	if (WARN_ONCE(vi == NULL, "vi not found, %s failed\n", __func__))
		return -ENODEV;

	file->private_data = vi;
	return 0;
}

static int vi_release(struct inode *inode, struct file *file)
{
	return 0;
}

const struct file_operations tegra_vi_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = vi_open,
	.unlocked_ioctl = vi_ioctl,
	.release = vi_release,
};
#endif

void nvhost_vi_reset(struct platform_device *pdev)
{
	u32 reset_reg;

	if (pdev->id == 0)
		reset_reg = T12_VI_CSI_0_SW_RESET;
	else
		reset_reg = T12_VI_CSI_1_SW_RESET;

	host1x_writel(pdev, reset_reg, T12_VI_CSI_SW_RESET_MCCIF_RESET);

	udelay(10);

	host1x_writel(pdev, reset_reg, 0);
}

