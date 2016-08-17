/*
 * drivers/video/tegra/host/dev.c
 *
 * Tegra Graphics Host Driver Entrypoint
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION, All rights reserved.
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

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/clk.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include "dev.h"
#include <trace/events/nvhost.h>

#include <linux/nvhost.h>
#include <linux/nvhost_ioctl.h>

#include "debug.h"
#include "bus_client.h"
#include "nvhost_acm.h"
#include "nvhost_channel.h"
#include "nvhost_job.h"
#include "chip_support.h"
#include "t20/t20.h"
#include "t30/t30.h"
#include "t114/t114.h"

#define DRIVER_NAME		"host1x"

struct nvhost_master *nvhost;

struct nvhost_ctrl_userctx {
	struct nvhost_master *dev;
	u32 *mod_locks;
};

static int nvhost_ctrlrelease(struct inode *inode, struct file *filp)
{
	struct nvhost_ctrl_userctx *priv = filp->private_data;
	int i;

	trace_nvhost_ctrlrelease(priv->dev->dev->name);

	filp->private_data = NULL;
	if (priv->mod_locks[0])
		nvhost_module_idle(priv->dev->dev);
	for (i = 1; i < nvhost_syncpt_nb_mlocks(&priv->dev->syncpt); i++)
		if (priv->mod_locks[i])
			nvhost_mutex_unlock(&priv->dev->syncpt, i);
	kfree(priv->mod_locks);
	kfree(priv);
	return 0;
}

static int nvhost_ctrlopen(struct inode *inode, struct file *filp)
{
	struct nvhost_master *host =
		container_of(inode->i_cdev, struct nvhost_master, cdev);
	struct nvhost_ctrl_userctx *priv;
	u32 *mod_locks;

	trace_nvhost_ctrlopen(host->dev->name);

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	mod_locks = kzalloc(sizeof(u32)
			* nvhost_syncpt_nb_mlocks(&host->syncpt),
			GFP_KERNEL);

	if (!(priv && mod_locks)) {
		kfree(priv);
		kfree(mod_locks);
		return -ENOMEM;
	}

	priv->dev = host;
	priv->mod_locks = mod_locks;
	filp->private_data = priv;
	return 0;
}

static int nvhost_ioctl_ctrl_syncpt_read(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_syncpt_read_args *args)
{
	if (args->id >= nvhost_syncpt_nb_pts(&ctx->dev->syncpt))
		return -EINVAL;
	args->value = nvhost_syncpt_read(&ctx->dev->syncpt, args->id);
	trace_nvhost_ioctl_ctrl_syncpt_read(args->id, args->value);
	return 0;
}

static int nvhost_ioctl_ctrl_syncpt_incr(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_syncpt_incr_args *args)
{
	if (args->id >= nvhost_syncpt_nb_pts(&ctx->dev->syncpt))
		return -EINVAL;
	trace_nvhost_ioctl_ctrl_syncpt_incr(args->id);
	nvhost_syncpt_incr(&ctx->dev->syncpt, args->id);
	return 0;
}

static int nvhost_ioctl_ctrl_syncpt_waitex(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_syncpt_waitex_args *args)
{
	u32 timeout;
	int err;
	if (args->id >= nvhost_syncpt_nb_pts(&ctx->dev->syncpt))
		return -EINVAL;
	if (args->timeout == NVHOST_NO_TIMEOUT)
		timeout = MAX_SCHEDULE_TIMEOUT;
	else
		timeout = (u32)msecs_to_jiffies(args->timeout);

	err = nvhost_syncpt_wait_timeout(&ctx->dev->syncpt, args->id,
					args->thresh, timeout, &args->value);
	trace_nvhost_ioctl_ctrl_syncpt_wait(args->id, args->thresh,
	  args->timeout, args->value, err);

	return err;
}

static int nvhost_ioctl_ctrl_module_mutex(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_module_mutex_args *args)
{
	int err = 0;
	if (args->id >= nvhost_syncpt_nb_mlocks(&ctx->dev->syncpt) ||
	    args->lock > 1)
		return -EINVAL;

	trace_nvhost_ioctl_ctrl_module_mutex(args->lock, args->id);
	if (args->lock && !ctx->mod_locks[args->id]) {
		if (args->id == 0)
			nvhost_module_busy(ctx->dev->dev);
		else
			err = nvhost_mutex_try_lock(&ctx->dev->syncpt,
					args->id);
		if (!err)
			ctx->mod_locks[args->id] = 1;
	} else if (!args->lock && ctx->mod_locks[args->id]) {
		if (args->id == 0)
			nvhost_module_idle(ctx->dev->dev);
		else
			nvhost_mutex_unlock(&ctx->dev->syncpt, args->id);
		ctx->mod_locks[args->id] = 0;
	}
	return err;
}

static int nvhost_ioctl_ctrl_module_regrdwr(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_module_regrdwr_args *args)
{
	u32 num_offsets = args->num_offsets;
	u32 *offsets = args->offsets;
	u32 *values = args->values;
	u32 vals[64];
	struct platform_device *ndev;

	trace_nvhost_ioctl_ctrl_module_regrdwr(args->id,
			args->num_offsets, args->write);

	/* Check that there is something to read and that block size is
	 * u32 aligned */
	if (num_offsets == 0 || args->block_size & 3)
		return -EINVAL;

	ndev = nvhost_device_list_match_by_id(args->id);
	BUG_ON(!ndev);

	while (num_offsets--) {
		int err;
		int remaining = args->block_size >> 2;
		u32 offs;
		if (get_user(offs, offsets))
			return -EFAULT;
		offsets++;
		while (remaining) {
			int batch = min(remaining, 64);
			if (args->write) {
				if (copy_from_user(vals, values,
							batch*sizeof(u32)))
					return -EFAULT;
				err = nvhost_write_module_regs(ndev,
						offs, batch, vals);
				if (err)
					return err;
			} else {
				err = nvhost_read_module_regs(ndev,
						offs, batch, vals);
				if (err)
					return err;
				if (copy_to_user(values, vals,
							batch*sizeof(u32)))
					return -EFAULT;
			}
			remaining -= batch;
			offs += batch*sizeof(u32);
			values += batch;
		}
	}

	return 0;
}

static int nvhost_ioctl_ctrl_get_version(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_get_param_args *args)
{
	args->value = NVHOST_SUBMIT_VERSION_MAX_SUPPORTED;
	return 0;
}

static int nvhost_ioctl_ctrl_syncpt_read_max(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_syncpt_read_args *args)
{
	if (args->id >= nvhost_syncpt_nb_pts(&ctx->dev->syncpt))
		return -EINVAL;
	args->value = nvhost_syncpt_read_max(&ctx->dev->syncpt, args->id);
	return 0;
}

static long nvhost_ctrlctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct nvhost_ctrl_userctx *priv = filp->private_data;
	u8 buf[NVHOST_IOCTL_CTRL_MAX_ARG_SIZE];
	int err = 0;

	if ((_IOC_TYPE(cmd) != NVHOST_IOCTL_MAGIC) ||
		(_IOC_NR(cmd) == 0) ||
		(_IOC_NR(cmd) > NVHOST_IOCTL_CTRL_LAST) ||
		(_IOC_SIZE(cmd) > NVHOST_IOCTL_CTRL_MAX_ARG_SIZE))
		return -EFAULT;

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	switch (cmd) {
	case NVHOST_IOCTL_CTRL_SYNCPT_READ:
		err = nvhost_ioctl_ctrl_syncpt_read(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_SYNCPT_INCR:
		err = nvhost_ioctl_ctrl_syncpt_incr(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_SYNCPT_WAIT:
		err = nvhost_ioctl_ctrl_syncpt_waitex(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_MODULE_MUTEX:
		err = nvhost_ioctl_ctrl_module_mutex(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_MODULE_REGRDWR:
		err = nvhost_ioctl_ctrl_module_regrdwr(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_SYNCPT_WAITEX:
		err = nvhost_ioctl_ctrl_syncpt_waitex(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_GET_VERSION:
		err = nvhost_ioctl_ctrl_get_version(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_SYNCPT_READ_MAX:
		err = nvhost_ioctl_ctrl_syncpt_read_max(priv, (void *)buf);
		break;
	default:
		err = -ENOTTY;
		break;
	}

	if ((err == 0) && (_IOC_DIR(cmd) & _IOC_READ))
		err = copy_to_user((void __user *)arg, buf, _IOC_SIZE(cmd));

	return err;
}

static const struct file_operations nvhost_ctrlops = {
	.owner = THIS_MODULE,
	.release = nvhost_ctrlrelease,
	.open = nvhost_ctrlopen,
	.unlocked_ioctl = nvhost_ctrlctl
};

static void power_on_host(struct platform_device *dev)
{
	struct nvhost_master *host = nvhost_get_private_data(dev);

	nvhost_syncpt_reset(&host->syncpt);
}

static int power_off_host(struct platform_device *dev)
{
	struct nvhost_master *host = nvhost_get_private_data(dev);

	nvhost_syncpt_save(&host->syncpt);
	return 0;
}

static void clock_on_host(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	struct nvhost_master *host = nvhost_get_private_data(dev);
	nvhost_intr_start(&host->intr, clk_get_rate(pdata->clk[0]));
}

static int clock_off_host(struct platform_device *dev)
{
	struct nvhost_master *host = nvhost_get_private_data(dev);
	nvhost_intr_stop(&host->intr);
	return 0;
}

static int __devinit nvhost_user_init(struct nvhost_master *host)
{
	int err, devno;

	host->nvhost_class = class_create(THIS_MODULE, IFACE_NAME);
	if (IS_ERR(host->nvhost_class)) {
		err = PTR_ERR(host->nvhost_class);
		dev_err(&host->dev->dev, "failed to create class\n");
		goto fail;
	}

	err = alloc_chrdev_region(&devno, 0, 1, IFACE_NAME);
	if (err < 0) {
		dev_err(&host->dev->dev, "failed to reserve chrdev region\n");
		goto fail;
	}

	cdev_init(&host->cdev, &nvhost_ctrlops);
	host->cdev.owner = THIS_MODULE;
	err = cdev_add(&host->cdev, devno, 1);
	if (err < 0)
		goto fail;
	host->ctrl = device_create(host->nvhost_class, NULL, devno, NULL,
			IFACE_NAME "-ctrl");
	if (IS_ERR(host->ctrl)) {
		err = PTR_ERR(host->ctrl);
		dev_err(&host->dev->dev, "failed to create ctrl device\n");
		goto fail;
	}

	return 0;
fail:
	return err;
}

struct nvhost_channel *nvhost_alloc_channel(struct platform_device *dev)
{
	BUG_ON(!host_device_op().alloc_nvhost_channel);
	return host_device_op().alloc_nvhost_channel(dev);
}

void nvhost_free_channel(struct nvhost_channel *ch)
{
	BUG_ON(!host_device_op().free_nvhost_channel);
	host_device_op().free_nvhost_channel(ch);
}

static void nvhost_free_resources(struct nvhost_master *host)
{
	kfree(host->intr.syncpt);
	host->intr.syncpt = 0;
}

static int __devinit nvhost_alloc_resources(struct nvhost_master *host)
{
	int err;

	err = nvhost_init_chip_support(host);
	if (err)
		return err;

	host->intr.syncpt = kzalloc(sizeof(struct nvhost_intr_syncpt) *
				    nvhost_syncpt_nb_pts(&host->syncpt),
				    GFP_KERNEL);

	if (!host->intr.syncpt) {
		/* frees happen in the support removal phase */
		return -ENOMEM;
	}

	return 0;
}

void nvhost_host1x_update_clk(struct platform_device *pdev)
{
	struct nvhost_master *host = nvhost_get_host(pdev);

	actmon_op().update_sample_period(host);
}

static struct of_device_id tegra_host1x_of_match[] __devinitdata = {
	{ .compatible = "nvidia,tegra20-host1x",
		.data = (struct nvhost_device_data *)&t20_host1x_info },
	{ .compatible = "nvidia,tegra30-host1x",
		.data = (struct nvhost_device_data *)&t30_host1x_info },
	{ .compatible = "nvidia,tegra114-host1x",
		.data = (struct nvhost_device_data *)&t11_host1x_info },
	{ },
};

static int __devinit nvhost_probe(struct platform_device *dev)
{
	struct nvhost_master *host;
	struct resource *regs, *intr0, *intr1;
	int i, err;
	struct nvhost_device_data *pdata = NULL;

	if (dev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(tegra_host1x_of_match, &dev->dev);
		if (match)
			pdata = (struct nvhost_device_data *)match->data;
	} else
		pdata = (struct nvhost_device_data *)dev->dev.platform_data;

	WARN_ON(!pdata);
	if (!pdata) {
		dev_info(&dev->dev, "no platform data\n");
		return -ENODATA;
	}
	regs = platform_get_resource(dev, IORESOURCE_MEM, 0);
	intr0 = platform_get_resource(dev, IORESOURCE_IRQ, 0);
	intr1 = platform_get_resource(dev, IORESOURCE_IRQ, 1);

	if (!regs || !intr0 || !intr1) {
		dev_err(&dev->dev, "missing required platform resources\n");
		return -ENXIO;
	}

	host = kzalloc(sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	nvhost = host;

	host->dev = dev;

	/* Copy host1x parameters. The private_data gets replaced
	 * by nvhost_master later */
	memcpy(&host->info, pdata->private_data,
			sizeof(struct host1x_device_info));

	pdata->finalize_poweron = power_on_host;
	pdata->prepare_poweroff = power_off_host;
	pdata->prepare_clockoff = clock_off_host;
	pdata->finalize_clockon = clock_on_host;

	pdata->pdev = dev;

	/* set common host1x device data */
	platform_set_drvdata(dev, pdata);

	/* set private host1x device data */
	nvhost_set_private_data(dev, host);

	host->reg_mem = request_mem_region(regs->start,
		resource_size(regs), dev->name);
	if (!host->reg_mem) {
		dev_err(&dev->dev, "failed to get host register memory\n");
		err = -ENXIO;
		goto fail;
	}

	host->aperture = ioremap(regs->start, resource_size(regs));
	if (!host->aperture) {
		dev_err(&dev->dev, "failed to remap host registers\n");
		err = -ENXIO;
		goto fail;
	}

	err = nvhost_alloc_resources(host);
	if (err) {
		dev_err(&dev->dev, "failed to init chip support\n");
		goto fail;
	}

	host->memmgr = mem_op().alloc_mgr();
	if (!host->memmgr) {
		dev_err(&dev->dev, "unable to create nvmap client\n");
		err = -EIO;
		goto fail;
	}

	err = nvhost_syncpt_init(dev, &host->syncpt);
	if (err)
		goto fail;

	err = nvhost_intr_init(&host->intr, intr1->start, intr0->start);
	if (err)
		goto fail;

	err = nvhost_user_init(host);
	if (err)
		goto fail;

	err = nvhost_module_init(dev);
	if (err)
		goto fail;

	for (i = 0; i < pdata->num_clks; i++)
		clk_prepare_enable(pdata->clk[i]);
	nvhost_syncpt_reset(&host->syncpt);
	for (i = 0; i < pdata->num_clks; i++)
		clk_disable_unprepare(pdata->clk[i]);

	pm_runtime_use_autosuspend(&dev->dev);
	pm_runtime_set_autosuspend_delay(&dev->dev, 100);
	pm_runtime_enable(&dev->dev);

	nvhost_device_list_init();
	err = nvhost_device_list_add(dev);
	if (err)
		goto fail;

	nvhost_debug_init(host);

	dev_info(&dev->dev, "initialized\n");
	return 0;

fail:
	nvhost_free_resources(host);
	if (host->memmgr)
		mem_op().put_mgr(host->memmgr);
	kfree(host);
	return err;
}

static int __exit nvhost_remove(struct platform_device *dev)
{
	struct nvhost_master *host = nvhost_get_private_data(dev);
	nvhost_intr_deinit(&host->intr);
	nvhost_syncpt_deinit(&host->syncpt);
	nvhost_free_resources(host);
	return 0;
}

static int nvhost_suspend(struct platform_device *dev, pm_message_t state)
{
	struct nvhost_master *host = nvhost_get_private_data(dev);
	int ret = 0;

	ret = nvhost_module_suspend(host->dev);
	dev_info(&dev->dev, "suspend status: %d\n", ret);

	return ret;
}

static int nvhost_resume(struct platform_device *dev)
{
	dev_info(&dev->dev, "resuming\n");
	return 0;
}

static struct platform_driver platform_driver = {
	.probe = nvhost_probe,
	.remove = __exit_p(nvhost_remove),
	.suspend = nvhost_suspend,
	.resume = nvhost_resume,
	.driver = {
		.owner = THIS_MODULE,
		.name = DRIVER_NAME,
#ifdef CONFIG_OF
		.of_match_table = tegra_host1x_of_match,
#endif
	},
};

static int __init nvhost_mod_init(void)
{
	return platform_driver_register(&platform_driver);
}

static void __exit nvhost_mod_exit(void)
{
	platform_driver_unregister(&platform_driver);
}

/* host1x master device needs nvmap to be instantiated first.
 * nvmap is instantiated via fs_initcall.
 * Hence instantiate host1x master device using rootfs_initcall
 * which is one level after fs_initcall. */
rootfs_initcall(nvhost_mod_init);
module_exit(nvhost_mod_exit);

