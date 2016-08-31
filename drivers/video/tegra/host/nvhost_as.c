/*
 * drivers/video/tegra/host/nvhost_as.c
 *
 * Tegra Host Address Spaces
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#include <trace/events/nvhost.h>

#include <linux/nvhost_as_ioctl.h>

#include "dev.h"
#include "bus_client.h"
#include "nvhost_hwctx.h"
#include "nvhost_as.h"

int nvhost_as_dev_open(struct inode *inode, struct file *filp)
{
	struct nvhost_as_share *as_share;
	struct nvhost_channel *ch;
	int err;

	nvhost_dbg_fn("");

	/* this will come from module, not channel, later */
	ch = container_of(inode->i_cdev, struct nvhost_channel, as_cdev);
	if (!ch->as) {
		nvhost_dbg_fn("no as for the channel!");
		return -ENOENT;
	}

	ch = nvhost_getchannel(ch, false);
	if (!ch) {
		nvhost_dbg_fn("fail to get channel!");
		return -ENOMEM;
	}

	err = nvhost_as_alloc_share(ch, &as_share);
	if (err) {
		nvhost_dbg_fn("failed to alloc share");
		goto clean_up;
	}

	filp->private_data = as_share;

clean_up:
	return 0;
}

int nvhost_as_dev_release(struct inode *inode, struct file *filp)
{
	struct nvhost_as_share *as_share = filp->private_data;
	struct nvhost_channel *ch;
	int ret;

	nvhost_dbg_fn("");

	ch = container_of(inode->i_cdev, struct nvhost_channel, as_cdev);

	ret = nvhost_as_release_share(as_share, 0/* no hwctx to release */);

	nvhost_putchannel(ch);

	return ret;
}

long nvhost_as_dev_ctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct nvhost_as_share *as_share = filp->private_data;
	struct nvhost_channel *ch = as_share->ch;
	struct device *dev = as_share->as_dev;

	u8 buf[NVHOST_AS_IOCTL_MAX_ARG_SIZE];

	if ((_IOC_TYPE(cmd) != NVHOST_AS_IOCTL_MAGIC) ||
		(_IOC_NR(cmd) == 0) ||
		(_IOC_NR(cmd) > NVHOST_AS_IOCTL_LAST))
		return -EFAULT;

	BUG_ON(_IOC_SIZE(cmd) > NVHOST_AS_IOCTL_MAX_ARG_SIZE);

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	nvhost_module_busy(ch->dev);

	switch (cmd) {
	case NVHOST_AS_IOCTL_BIND_CHANNEL:
		trace_nvhost_as_ioctl_bind_channel(dev_name(&ch->dev->dev));
		err = nvhost_as_ioctl_bind_channel(as_share,
			       (struct nvhost_as_bind_channel_args *)buf);

		break;
	case NVHOST32_AS_IOCTL_ALLOC_SPACE:
	{
		struct nvhost32_as_alloc_space_args *args32 =
			(struct nvhost32_as_alloc_space_args *)buf;
		struct nvhost_as_alloc_space_args args;

		args.pages = args32->pages;
		args.page_size = args32->page_size;
		args.flags = args32->flags;
		args.o_a.offset = args32->o_a.offset;
		trace_nvhost_as_ioctl_alloc_space(dev_name(&ch->dev->dev));
		err = nvhost_as_ioctl_alloc_space(as_share, &args);
		args32->o_a.offset = args.o_a.offset;
		break;
	}
	case NVHOST_AS_IOCTL_ALLOC_SPACE:
		trace_nvhost_as_ioctl_alloc_space(dev_name(&ch->dev->dev));
		err = nvhost_as_ioctl_alloc_space(as_share,
				  (struct nvhost_as_alloc_space_args *)buf);
		break;
	case NVHOST_AS_IOCTL_FREE_SPACE:
		trace_nvhost_as_ioctl_free_space(dev_name(&ch->dev->dev));
		err = nvhost_as_ioctl_free_space(as_share,
				       (struct nvhost_as_free_space_args *)buf);
		break;
	case NVHOST_AS_IOCTL_MAP_BUFFER:
		trace_nvhost_as_ioctl_map_buffer(dev_name(&ch->dev->dev));
		err = nvhost_as_ioctl_map_buffer(as_share,
				       (struct nvhost_as_map_buffer_args *)buf);
		break;
	case NVHOST_AS_IOCTL_UNMAP_BUFFER:
		trace_nvhost_as_ioctl_unmap_buffer(dev_name(&ch->dev->dev));
		err = nvhost_as_ioctl_unmap_buffer(as_share,
			       (struct nvhost_as_unmap_buffer_args *)buf);
		break;
	default:
		nvhost_err(dev, "unrecognized aspace ioctl cmd: 0x%x", cmd);
		err = -ENOTTY;
		break;
	}

	nvhost_module_idle(ch->dev);

	if ((err == 0) && (_IOC_DIR(cmd) & _IOC_READ))
		err = copy_to_user((void __user *)arg, buf, _IOC_SIZE(cmd));

	return err;
}


int nvhost_as_init_device(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = nvhost_get_devdata(dev);
	struct nvhost_channel *ch = pdata->channel;
	struct nvhost_as *as;
	int err = 0;

	if (!ch) {
		nvhost_err(&dev->dev, "no channel in nvhost_as_init for %s",
			   dev->name);
		return -ENODEV;
	}

	if (!ch->as) {

		nvhost_dbg_fn("allocating as for %s", dev->name);
		as = kzalloc(sizeof(*as), GFP_KERNEL);
		if (!as) {
			err = -ENOMEM;
			goto failed;
		}
		ch->as = as;
		as->ch = ch;

		mutex_init(&as->share_list_lock);
		INIT_LIST_HEAD(&as->share_list);
	}

	return 0;

 failed:
	kfree(as);
	ch->as = 0;

	return err;

}

/* dumb allocator... */
static int generate_as_share_id(struct nvhost_as *as)
{
	nvhost_dbg_fn("");
	return ++as->last_share_id;
}
/* still dumb */
static void release_as_share_id(struct nvhost_as *as, int id)
{
	nvhost_dbg_fn("");
	return;
}

int nvhost_as_alloc_share(struct nvhost_channel *ch,
			  struct nvhost_as_share **_as_share)
{
	struct nvhost_device_data *pdata = nvhost_get_devdata(ch->dev);
	struct nvhost_as *as = ch->as;
	struct nvhost_as_share *as_share;
	int err = 0;

	nvhost_dbg_fn("");

	*_as_share = 0;
	as_share = kzalloc(sizeof(*as_share), GFP_KERNEL);
	if (!as_share)
		return -ENOMEM;

	as_share->ch      = ch;
	as_share->as      = as;
	as_share->host    = nvhost_get_host(ch->dev);
	as_share->as_dev  = ch->as_node;
	as_share->id      = generate_as_share_id(as_share->as);

	/* call module to allocate hw resources */
	err = pdata->as_ops->alloc_share(as_share);
	if (err)
		goto failed;

	/* When an fd is attached we'll get a call to release the as when the
	 * process exits (or otherwise closes the fd for the share).
	 * Setting up the ref_cnt in this manner allows for us to properly
	 * handle both that case and when we've created and bound a share
	 * w/o the attached fd.
	 */
	as_share->ref_cnt.counter = 1;
	/* else set at from kzalloc above 0 */

	/* add the share to the set of all shares on the module */
	mutex_lock(&as->share_list_lock);
	list_add_tail(&as_share->share_list_node, &as->share_list);
	mutex_unlock(&as->share_list_lock);

	/* initialize the bound list */
	mutex_init(&as_share->bound_list_lock);
	INIT_LIST_HEAD(&as_share->bound_list);

	*_as_share = as_share;
	return 0;

 failed:
	kfree(as_share);
	return err;
}

/*
 * hwctxs and the device nodes call this to release.
 * once the ref_cnt hits zero the share is deleted.
 * hwctx == 0 when the device node is being released.
 * otherwise it is a hwctx unbind.
 */
int nvhost_as_release_share(struct nvhost_as_share *as_share,
			     struct nvhost_hwctx *hwctx)
{
	int err;
	struct nvhost_device_data *pdata =
		nvhost_get_devdata(as_share->ch->dev);

	nvhost_dbg_fn("");

	if (hwctx) {
		hwctx->as_share = 0;

		mutex_lock(&as_share->bound_list_lock);
		list_del(&hwctx->as_share_bound_list_node);
		mutex_unlock(&as_share->bound_list_lock);
	}

	if (atomic_dec_return(&as_share->ref_cnt) > 0)
		return 0;

	err = pdata->as_ops->release_share(as_share);

	mutex_lock(&as_share->as->share_list_lock);
	list_del(&as_share->share_list_node);
	mutex_unlock(&as_share->as->share_list_lock);

	release_as_share_id(as_share->as, as_share->id);

	kfree(as_share);

	return err;
}


static int bind_share(struct nvhost_as_share *as_share,
		      struct nvhost_hwctx *hwctx)
{
	int err = 0;
	struct nvhost_device_data *pdata =
		nvhost_get_devdata(as_share->ch->dev);
	nvhost_dbg_fn("");

	atomic_inc(&as_share->ref_cnt);
	err = pdata->as_ops->bind_hwctx(as_share, hwctx);
	if (err) {
		atomic_dec(&as_share->ref_cnt);
		return err;
	}
	hwctx->as_share = as_share;

	mutex_lock(&as_share->bound_list_lock);
	list_add_tail(&hwctx->as_share_bound_list_node, &as_share->bound_list);
	mutex_unlock(&as_share->bound_list_lock);

	return 0;
}

int nvhost_as_ioctl_bind_channel(struct nvhost_as_share *as_share,
				 struct nvhost_as_bind_channel_args *args)
{
	int err = 0;
	struct nvhost_hwctx *hwctx;

	nvhost_dbg_fn("");

	hwctx = nvhost_channel_get_file_hwctx(args->channel_fd);
	if (!hwctx || hwctx->as_share)
		return -EINVAL;

	err = bind_share(as_share, hwctx);

	return err;
}

int nvhost_as_ioctl_alloc_space(struct nvhost_as_share *as_share,
				struct nvhost_as_alloc_space_args *args)
{
	struct nvhost_device_data *pdata =
		nvhost_get_devdata(as_share->ch->dev);
	nvhost_dbg_fn("");
	return pdata->as_ops->alloc_space(as_share, args);

}

int nvhost_as_ioctl_free_space(struct nvhost_as_share *as_share,
			       struct nvhost_as_free_space_args *args)
{
	struct nvhost_device_data *pdata =
		nvhost_get_devdata(as_share->ch->dev);
	nvhost_dbg_fn("");
	return pdata->as_ops->free_space(as_share, args);
}

int nvhost_as_ioctl_map_buffer(struct nvhost_as_share *as_share,
			       struct nvhost_as_map_buffer_args *args)
{
	struct nvhost_device_data *pdata =
		nvhost_get_devdata(as_share->ch->dev);
	nvhost_dbg_fn("");

	return pdata->as_ops->map_buffer(as_share,
					     args->nvmap_fd, args->nvmap_handle,
					     &args->o_a.align, args->flags);
	/* args->o_a.offset will be set if !err */
}

int nvhost_as_ioctl_unmap_buffer(struct nvhost_as_share *as_share,
				 struct nvhost_as_unmap_buffer_args *args)
{
	struct nvhost_device_data *pdata =
		nvhost_get_devdata(as_share->ch->dev);
	nvhost_dbg_fn("");

	return pdata->as_ops->unmap_buffer(as_share, args->offset);
}
