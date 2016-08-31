/*
 * drivers/video/tegra/host/dev.c
 *
 * Tegra Graphics Host Driver Entrypoint
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation. All rights reserved.
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
#include <linux/tegra-soc.h>

#include "dev.h"
#include <trace/events/nvhost.h>

#include <linux/nvhost.h>
#include <linux/nvhost_ioctl.h>

#include <mach/pm_domains.h>

#include "debug.h"
#include "bus_client.h"
#include "nvhost_acm.h"
#include "nvhost_channel.h"
#include "nvhost_job.h"
#include "nvhost_memmgr.h"

#ifdef CONFIG_TEGRA_GRHOST_SYNC
#include "nvhost_sync.h"
#endif

#include "nvhost_scale.h"
#include "chip_support.h"
#include "t114/t114.h"
#include "t148/t148.h"
#include "t124/t124.h"

#define DRIVER_NAME		"host1x"

static const char *num_syncpts_name = "num_pts";
static const char *num_mutexes_name = "num_mlocks";
static const char *num_waitbases_name = "num_bases";
static const char *gather_filter_enabled_name = "gather_filter_enabled";

struct nvhost_master *nvhost;

struct nvhost_ctrl_userctx {
	struct nvhost_master *dev;
	u32 *mod_locks;
};

struct nvhost_capability_node {
	struct kobj_attribute attr;
	struct nvhost_master *host;
	int (*func)(struct nvhost_syncpt *sp);
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
		/* FIXME: MAX_SCHEDULE_TIMEOUT is ulong which can be bigger
                   than u32 so we should fix nvhost_syncpt_wait_timeout to
                   take ulong not u32. */
		timeout = (u32)MAX_SCHEDULE_TIMEOUT;
	else
		timeout = (u32)msecs_to_jiffies(args->timeout);

	err = nvhost_syncpt_wait_timeout(&ctx->dev->syncpt, args->id,
					args->thresh, timeout, &args->value,
					NULL, true);
	trace_nvhost_ioctl_ctrl_syncpt_wait(args->id, args->thresh,
	  args->timeout, args->value, err);

	return err;
}

static int nvhost_ioctl_ctrl_syncpt_waitmex(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_syncpt_waitmex_args *args)
{
	ulong timeout;
	int err;
	struct timespec ts;
	if (args->id >= nvhost_syncpt_nb_pts(&ctx->dev->syncpt))
		return -EINVAL;
	if (args->timeout == NVHOST_NO_TIMEOUT)
		timeout = MAX_SCHEDULE_TIMEOUT;
	else
		timeout = (u32)msecs_to_jiffies(args->timeout);

	err = nvhost_syncpt_wait_timeout(&ctx->dev->syncpt, args->id,
					args->thresh, timeout, &args->value,
					&ts, true);
	args->tv_sec = ts.tv_sec;
	args->tv_nsec = ts.tv_nsec;
	trace_nvhost_ioctl_ctrl_syncpt_wait(args->id, args->thresh,
					    args->timeout, args->value, err);

	return err;
}

static int nvhost_ioctl_ctrl_sync_fence_create(struct nvhost_ctrl_userctx *ctx,
	struct nvhost_ctrl_sync_fence_create_args *args)
{
#ifdef CONFIG_TEGRA_GRHOST_SYNC
	int err;
	int i;
	struct nvhost_ctrl_sync_fence_info *pts;
	char name[32];
	const char __user *args_name =
		(const char __user *)(uintptr_t)args->name;
	const void __user *args_pts =
		(const void __user *)(uintptr_t)args->pts;

	if (args_name) {
		if (strncpy_from_user(name, args_name, sizeof(name)) < 0)
			return -EFAULT;
		name[sizeof(name) - 1] = '\0';
	} else {
		name[0] = '\0';
	}

	pts = kmalloc(sizeof(*pts) * args->num_pts, GFP_KERNEL);
	if (!pts)
		return -ENOMEM;


	if (copy_from_user(pts, args_pts, sizeof(*pts) * args->num_pts)) {
		err = -EFAULT;
		goto out;
	}

	for (i = 0; i < args->num_pts; i++) {
		if (pts[i].id >= nvhost_syncpt_nb_pts(&ctx->dev->syncpt) &&
		    pts[i].id != NVSYNCPT_INVALID) {
			err = -EINVAL;
			goto out;
		}
	}

	err = nvhost_sync_create_fence(&ctx->dev->syncpt, pts, args->num_pts,
				       name, &args->fence_fd);
out:
	kfree(pts);
	return err;
#else
	return -EINVAL;
#endif
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
	u32 __user *offsets = (u32 *)(uintptr_t)args->offsets;
	u32 __user *values = (u32 *)(uintptr_t)args->values;
	u32 *vals;
	u32 *p1;
	int remaining;
	int err;

	struct platform_device *ndev;
	trace_nvhost_ioctl_ctrl_module_regrdwr(args->id,
			args->num_offsets, args->write);

	/* Check that there is something to read and that block size is
	 * u32 aligned */
	if (num_offsets == 0 || args->block_size & 3)
		return -EINVAL;

	ndev = nvhost_device_list_match_by_id(args->id);
	if (!ndev)
		return -ENODEV;

	remaining = args->block_size >> 2;

	vals = kmalloc(num_offsets * args->block_size,
				GFP_KERNEL);
	if (!vals)
		return -ENOMEM;
	p1 = vals;

	if (args->write) {
		if (copy_from_user((char *)vals, (char *)values,
				num_offsets * args->block_size)) {
			kfree(vals);
			return -EFAULT;
		}
		while (num_offsets--) {
			u32 offs;
			if (get_user(offs, offsets)) {
				kfree(vals);
				return -EFAULT;
			}
			offsets++;
			err = nvhost_write_module_regs(ndev,
					offs, remaining, p1);
			if (err) {
				kfree(vals);
				return err;
			}
			p1 += remaining;
		}
		kfree(vals);
	} else {
		while (num_offsets--) {
			u32 offs;
			if (get_user(offs, offsets)) {
				kfree(vals);
				return -EFAULT;
			}
			offsets++;
			err = nvhost_read_module_regs(ndev,
					offs, remaining, p1);
			if (err) {
				kfree(vals);
				return err;
			}
			p1 += remaining;
		}

		if (copy_to_user((char *)values, (char *)vals,
				args->num_offsets * args->block_size)) {
			kfree(vals);
			return -EFAULT;
		}
		kfree(vals);
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
	case NVHOST32_IOCTL_CTRL_SYNC_FENCE_CREATE:
	{
		struct nvhost32_ctrl_sync_fence_create_args *args32 =
			(struct nvhost32_ctrl_sync_fence_create_args *)buf;
		struct nvhost_ctrl_sync_fence_create_args args;
		args.name = args32->name;
		args.pts = args32->pts;
		args.num_pts = args32->num_pts;
		err = nvhost_ioctl_ctrl_sync_fence_create(priv, &args);
		args32->fence_fd = args.fence_fd;
		break;
	}
	case NVHOST_IOCTL_CTRL_SYNC_FENCE_CREATE:
		err = nvhost_ioctl_ctrl_sync_fence_create(priv, (void *)buf);
		break;
	case NVHOST_IOCTL_CTRL_MODULE_MUTEX:
		err = nvhost_ioctl_ctrl_module_mutex(priv, (void *)buf);
		break;
	case NVHOST32_IOCTL_CTRL_MODULE_REGRDWR:
	{
		struct nvhost32_ctrl_module_regrdwr_args *args32 =
			(struct nvhost32_ctrl_module_regrdwr_args *)buf;
		struct nvhost_ctrl_module_regrdwr_args args;
		args.id = args32->id;
		args.num_offsets = args32->num_offsets;
		args.block_size = args32->block_size;
		args.offsets = args32->offsets;
		args.values = args32->values;
		args.write = args32->write;
		err = nvhost_ioctl_ctrl_module_regrdwr(priv, &args);
		break;
	}
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
	case NVHOST_IOCTL_CTRL_SYNCPT_WAITMEX:
		err = nvhost_ioctl_ctrl_syncpt_waitmex(priv, (void *)buf);
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
	.unlocked_ioctl = nvhost_ctrlctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nvhost_ctrlctl,
#endif
};

#ifdef CONFIG_PM
static int power_on_host(struct platform_device *dev)
{
	struct nvhost_master *host = nvhost_get_private_data(dev);

	nvhost_syncpt_reset(&host->syncpt);
	return 0;
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
#endif

static int nvhost_gather_filter_enabled(struct nvhost_syncpt *sp)
{
	return 0;
}

static ssize_t nvhost_syncpt_capability_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct nvhost_capability_node *node =
		container_of(attr, struct nvhost_capability_node, attr);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			node->func(&node->host->syncpt));
}

static inline int nvhost_set_sysfs_capability_node(
				struct nvhost_master *host, const char *name,
				struct nvhost_capability_node *node,
				int (*func)(struct nvhost_syncpt *sp))
{
	node->attr.attr.name = name;
	node->attr.attr.mode = S_IRUGO;
	node->attr.show = nvhost_syncpt_capability_show;
	node->func = func;
	node->host = host;

	return sysfs_create_file(host->caps_kobj, &node->attr.attr);
}

static int nvhost_user_init(struct nvhost_master *host)
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

	host->caps_nodes = devm_kzalloc(&host->dev->dev,
			sizeof(struct nvhost_capability_node) * 4, GFP_KERNEL);
	if (!host->caps_nodes) {
		err = -ENOMEM;
		goto fail;
	}

	host->caps_kobj = kobject_create_and_add("capabilities",
			&host->dev->dev.kobj);
	if (!host->caps_kobj) {
		err = -EIO;
		goto fail;
	}

	if (nvhost_set_sysfs_capability_node(host, num_syncpts_name,
		host->caps_nodes, &nvhost_syncpt_nb_pts)) {
		err = -EIO;
		goto fail;
	}

	if (nvhost_set_sysfs_capability_node(host, num_waitbases_name,
		host->caps_nodes + 1, &nvhost_syncpt_nb_bases)) {
		err = -EIO;
		goto fail;
	}

	if (nvhost_set_sysfs_capability_node(host, num_mutexes_name,
		host->caps_nodes + 2, &nvhost_syncpt_nb_mlocks)) {
		err = -EIO;
		goto fail;
	}

	if (nvhost_set_sysfs_capability_node(host,
		gather_filter_enabled_name, host->caps_nodes + 3,
		nvhost_gather_filter_enabled)) {
		err = -EIO;
		goto fail;
	}

	return 0;
fail:
	return err;
}

struct nvhost_channel *nvhost_alloc_channel(struct platform_device *dev)
{
	return host_device_op().alloc_nvhost_channel(dev);
}

void nvhost_free_channel(struct nvhost_channel *ch)
{
	host_device_op().free_nvhost_channel(ch);
}

static void nvhost_free_resources(struct nvhost_master *host)
{
	kfree(host->intr.syncpt);
	host->intr.syncpt = 0;
}

static int nvhost_alloc_resources(struct nvhost_master *host)
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

static struct of_device_id tegra_host1x_of_match[] = {
#ifdef TEGRA_11X_OR_HIGHER_CONFIG
	{ .compatible = "nvidia,tegra114-host1x",
		.data = (struct nvhost_device_data *)&t11_host1x_info },
#endif
#ifdef TEGRA_14X_OR_HIGHER_CONFIG
	{ .compatible = "nvidia,tegra148-host1x",
		.data = (struct nvhost_device_data *)&t14_host1x_info },
#endif
#ifdef TEGRA_12X_OR_HIGHER_CONFIG
	{ .compatible = "nvidia,tegra124-host1x",
		.data = (struct nvhost_device_data *)&t124_host1x_info },
#endif
	{ },
};

void nvhost_host1x_update_clk(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = NULL;
	struct nvhost_device_profile *profile;

	/* There are only two chips which need this workaround, so hardcode */
#ifdef TEGRA_11X_OR_HIGHER_CONFIG
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA11)
		pdata = &t11_gr3d_info;
#endif
#ifdef TEGRA_14X_OR_HIGHER_CONFIG
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA14)
		pdata = &t14_gr3d_info;
#endif
	if (!pdata)
		return;

	profile = pdata->power_profile;

	if (profile && profile->actmon)
		actmon_op().update_sample_period(profile->actmon);
}

int nvhost_host1x_finalize_poweron(struct platform_device *dev)
{
	return power_on_host(dev);
}

int nvhost_host1x_prepare_poweroff(struct platform_device *dev)
{
	return power_off_host(dev);
}
static int nvhost_probe(struct platform_device *dev)
{
	struct nvhost_master *host;
	struct resource *regs;
	int syncpt_irq, generic_irq;
	int err;
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
	if (!regs) {
		dev_err(&dev->dev, "missing host1x regs\n");
		return -ENXIO;
	}

	syncpt_irq = platform_get_irq(dev, 0);
	if (IS_ERR_VALUE(syncpt_irq)) {
		dev_err(&dev->dev, "missing syncpt irq\n");
		return -ENXIO;
	}

	generic_irq = platform_get_irq(dev, 1);
	if (IS_ERR_VALUE(generic_irq)) {
		dev_err(&dev->dev, "missing generic irq\n");
		return -ENXIO;
	}

	host = devm_kzalloc(&dev->dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	nvhost = host;

	host->dev = dev;
	mutex_init(&pdata->lock);

	/* Copy host1x parameters. The private_data gets replaced
	 * by nvhost_master later */
	memcpy(&host->info, pdata->private_data,
			sizeof(struct host1x_device_info));

	pdata->pdev = dev;

	/* set common host1x device data */
	platform_set_drvdata(dev, pdata);

	/* set private host1x device data */
	nvhost_set_private_data(dev, host);

	host->aperture = devm_request_and_ioremap(&dev->dev, regs);
	if (!host->aperture) {
		err = -ENXIO;
		goto fail;
	}

	err = nvhost_alloc_resources(host);
	if (err) {
		dev_err(&dev->dev, "failed to init chip support\n");
		goto fail;
	}

	host->memmgr = nvhost_memmgr_alloc_mgr();
	if (!host->memmgr) {
		dev_err(&dev->dev, "unable to create nvmap client\n");
		err = -EIO;
		goto fail;
	}

	err = nvhost_syncpt_init(dev, &host->syncpt);
	if (err)
		goto fail;

	err = nvhost_intr_init(&host->intr, generic_irq, syncpt_irq);
	if (err)
		goto fail;

	err = nvhost_user_init(host);
	if (err)
		goto fail;

	err = nvhost_module_init(dev);
	if (err)
		goto fail;

#ifdef CONFIG_PM_GENERIC_DOMAINS
	pdata->pd.name = "tegra-host1x";
	err = nvhost_module_add_domain(&pdata->pd, dev);

#endif

	nvhost_module_busy(dev);

	nvhost_syncpt_reset(&host->syncpt);
	nvhost_intr_start(&host->intr, clk_get_rate(pdata->clk[0]));

	nvhost_device_list_init();
	pdata->nvhost_timeout_default =
			CONFIG_TEGRA_GRHOST_DEFAULT_TIMEOUT;
	nvhost_debug_init(host);

	nvhost_module_idle(dev);

	dev_info(&dev->dev, "initialized\n");
	return 0;

fail:
	nvhost_free_resources(host);
	if (host->memmgr)
		nvhost_memmgr_put_mgr(host->memmgr);
	kfree(host);
	return err;
}

static int __exit nvhost_remove(struct platform_device *dev)
{
	struct nvhost_master *host = nvhost_get_private_data(dev);
	nvhost_intr_deinit(&host->intr);
	nvhost_syncpt_deinit(&host->syncpt);
	nvhost_free_resources(host);
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_put(&dev->dev);
	pm_runtime_disable(&dev->dev);
#else
	nvhost_module_disable_clk(&dev->dev);
#endif
	return 0;
}

#ifdef CONFIG_PM
static int nvhost_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvhost_master *host = nvhost_get_private_data(pdev);
	int ret = 0;

	nvhost_module_enable_clk(dev);
	power_off_host(pdev);
	clock_off_host(pdev);
	nvhost_module_disable_clk(dev);

	ret = nvhost_module_suspend(&host->dev->dev);
	dev_info(dev, "suspend status: %d\n", ret);

	return ret;
}

static int nvhost_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	nvhost_module_enable_clk(dev);
	clock_on_host(pdev);
	power_on_host(pdev);
	nvhost_module_disable_clk(dev);

	dev_info(dev, "resuming\n");

	return 0;
}

static const struct dev_pm_ops host1x_pm_ops = {
	.suspend = nvhost_suspend,
	.resume = nvhost_resume,
};
#endif /* CONFIG_PM */

static struct platform_driver platform_driver = {
	.probe = nvhost_probe,
	.remove = __exit_p(nvhost_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = DRIVER_NAME,
#ifdef CONFIG_PM
		.pm = &host1x_pm_ops,
#endif
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

