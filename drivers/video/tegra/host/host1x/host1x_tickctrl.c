/*
 * drivers/video/tegra/host/host1x/host1x_counter.c
 *
 * Tegra Graphics Host Counter support
 *
 * Copyright (c) 2012, NVIDIA Corporation.
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

#include <linux/nvhost.h>
#include <linux/io.h>
#include "dev.h"
#include "chip_support.h"

static int host1x_tickctrl_init_channel(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	void __iomem *regs = pdata->channel->aperture;

	nvhost_module_busy(nvhost_get_parent(dev));

	/* Initialize counter */
	writel(0, regs + host1x_channel_tickcount_hi_r());
	writel(0, regs + host1x_channel_tickcount_lo_r());
	writel(0, regs + host1x_channel_stallcount_hi_r());
	writel(0, regs + host1x_channel_stallcount_lo_r());
	writel(0, regs + host1x_channel_xfercount_hi_r());
	writel(0, regs + host1x_channel_xfercount_lo_r());

	writel(host1x_channel_channelctrl_enabletickcnt_f(1),
			regs + host1x_channel_channelctrl_r());
	writel(host1x_channel_stallctrl_enable_channel_stall_f(1),
			regs + host1x_channel_stallctrl_r());
	writel(host1x_channel_xferctrl_enable_channel_xfer_f(1),
			regs + host1x_channel_xferctrl_r());

	nvhost_module_idle(nvhost_get_parent(dev));

	return 0;
}

static void host1x_tickctrl_deinit_channel(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	void __iomem *regs = pdata->channel->aperture;

	nvhost_module_busy(nvhost_get_parent(dev));
	writel(host1x_channel_stallctrl_enable_channel_stall_f(0),
			regs + host1x_channel_stallctrl_r());
	writel(host1x_channel_xferctrl_enable_channel_xfer_f(0),
			regs + host1x_channel_xferctrl_r());
	writel(host1x_channel_channelctrl_enabletickcnt_f(0),
			regs + host1x_channel_channelctrl_r());
	nvhost_module_idle(nvhost_get_parent(dev));
}

static u64 readl64(void __iomem *reg_hi, void __iomem *reg_lo)
{
	u32 hi, lo, hi2;
	do {
		hi = readl(reg_hi);
		lo = readl(reg_lo);
		rmb();
		hi2 = readl(reg_hi);
	} while (hi2 != hi);
	return ((u64)hi << 32) | (u64)lo;
}

static int host1x_tickctrl_tickcount(struct platform_device *dev, u64 *val)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	void __iomem *regs = pdata->channel->aperture;

	nvhost_module_busy(nvhost_get_parent(dev));

	*val = readl64(regs + host1x_channel_tickcount_hi_r(),
		regs + host1x_channel_tickcount_lo_r());

	rmb();
	nvhost_module_idle(nvhost_get_parent(dev));

	return 0;
}

static int host1x_tickctrl_stallcount(struct platform_device *dev, u64 *val)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	void __iomem *regs = pdata->channel->aperture;

	nvhost_module_busy(nvhost_get_parent(dev));
	*val = readl64(regs + host1x_channel_stallcount_hi_r(),
		regs + host1x_channel_stallcount_lo_r());
	rmb();
	nvhost_module_idle(nvhost_get_parent(dev));

	return 0;
}

static int host1x_tickctrl_xfercount(struct platform_device *dev, u64 *val)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	void __iomem *regs = pdata->channel->aperture;

	nvhost_module_busy(nvhost_get_parent(dev));
	*val = readl64(regs + host1x_channel_xfercount_hi_r(),
		regs + host1x_channel_xfercount_lo_r());
	rmb();
	nvhost_module_idle(nvhost_get_parent(dev));

	return 0;
}

static const struct nvhost_tickctrl_ops host1x_tickctrl_ops = {
	.init_channel = host1x_tickctrl_init_channel,
	.deinit_channel = host1x_tickctrl_deinit_channel,
	.tickcount = host1x_tickctrl_tickcount,
	.stallcount = host1x_tickctrl_stallcount,
	.xfercount = host1x_tickctrl_xfercount,
};
