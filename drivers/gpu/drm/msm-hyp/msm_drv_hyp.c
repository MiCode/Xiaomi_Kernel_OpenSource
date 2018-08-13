/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Copyright (C) 2014 Red Hat
 * Copyright (C) 2014 Intel Corp.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 * Rob Clark <robdclark@gmail.com>
 * Daniel Vetter <daniel.vetter@ffwll.ch>
 */

#include "msm_drv_hyp.h"

/*
 * DRM operations:
 */

static int msm_unload(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;

	dev->dev_private = NULL;

	kfree(priv);

	return 0;
}

static int msm_load(struct drm_device *dev, unsigned long flags)
{
	struct msm_drm_private *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev->dev_private = priv;

	return 0;
}

static int msm_open(struct drm_device *dev, struct drm_file *file)
{
	struct msm_file_private *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	INIT_LIST_HEAD(&ctx->dmabuf_list);
	mutex_init(&ctx->dmabuf_lock);

	file->driver_priv = ctx;

	return 0;
}

static void msm_preclose(struct drm_device *dev, struct drm_file *file)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_file_private *ctx = file->driver_priv;
	struct msm_dmabuf *dmabuf, *pt;
	struct dma_buf *dma_buf;

	mutex_lock(&ctx->dmabuf_lock);
	list_for_each_entry_safe(dmabuf, pt, &ctx->dmabuf_list, node) {
		dma_buf = (struct dma_buf *)dmabuf->dma_id;
		dma_buf_put(dma_buf);
		list_del(&dmabuf->node);
		kfree(dmabuf);
	}
	mutex_unlock(&ctx->dmabuf_lock);

	mutex_lock(&dev->struct_mutex);
	if (ctx == priv->lastctx)
		priv->lastctx = NULL;
	mutex_unlock(&dev->struct_mutex);

	kfree(ctx);
}

static struct drm_pending_vblank_event *create_vblank_event(
		struct drm_device *dev, struct drm_file *file_priv, u32 type,
		uint64_t user_data)
{
	struct drm_pending_vblank_event *e = NULL;
	unsigned long flags;

	spin_lock_irqsave(&dev->event_lock, flags);
	if (file_priv->event_space < sizeof(e->event)) {
		spin_unlock_irqrestore(&dev->event_lock, flags);
		goto out;
	}
	file_priv->event_space -= sizeof(e->event);
	spin_unlock_irqrestore(&dev->event_lock, flags);

	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (e == NULL) {
		spin_lock_irqsave(&dev->event_lock, flags);
		file_priv->event_space += sizeof(e->event);
		spin_unlock_irqrestore(&dev->event_lock, flags);
		goto out;
	}

	e->event.base.type = type;
	e->event.base.length = sizeof(e->event);
	e->event.user_data = user_data;
	e->base.event = &e->event.base;
	e->base.file_priv = file_priv;
	e->base.destroy = (void (*) (struct drm_pending_event *)) kfree;

out:
	return e;
}

struct event_req {
	u32 type;
	u64 user_data;
};

struct drm_msm_hyp_gem {
	__u64 handle;
	__u32 size;
	__s32 fd;
};

#define DRM_MSM_HYP_GEM_GET            0x1
#define DRM_MSM_HYP_GEM_PUT            0x2
#define DRM_MSM_HYP_GEM_QRY            0x3
#define DRM_MSM_HYP_QRY_CLT_ID         0x4

#define DRM_IOCTL_MSM_HYP_GEM_GET\
	DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_HYP_GEM_GET, struct drm_msm_hyp_gem)
#define DRM_IOCTL_MSM_HYP_GEM_PUT\
	DRM_IOW(DRM_COMMAND_BASE + DRM_MSM_HYP_GEM_PUT, struct drm_msm_hyp_gem)
#define DRM_IOCTL_MSM_HYP_GEM_QRY\
	DRM_IOWR(DRM_COMMAND_BASE + DRM_MSM_HYP_GEM_QRY, struct drm_msm_hyp_gem)
#define DRM_IOCTL_MSM_HYP_QRY_CLT_ID\
	DRM_IOR(DRM_COMMAND_BASE + DRM_MSM_HYP_QRY_CLT_ID, u32)

#define CLIENT_ID_LEN_IN_CHARS         5

static ssize_t msm_drm_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *offset)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_device *dev = file_priv->minor->dev;
	struct event_req e_req;
	struct drm_pending_vblank_event *e;
	unsigned long flags;
	int ret = 0;

	if (count != sizeof(struct event_req))
		return -EINVAL;

	if (!access_ok(VERIFY_READ, buffer, count))
		return -EFAULT;

	ret = copy_from_user(&e_req, buffer, sizeof(e_req));
	if (ret)
		return -EFAULT;

	if (!(e_req.type & DRM_EVENT_VBLANK) &&
			!(e_req.type & DRM_EVENT_FLIP_COMPLETE))
		return -EINVAL;

	e = create_vblank_event(dev, file_priv, e_req.type, e_req.user_data);
	if (!e)
		return -ENOMEM;

	spin_lock_irqsave(&dev->event_lock, flags);
	drm_send_vblank_event(dev, 2, e);
	spin_unlock_irqrestore(&dev->event_lock, flags);

	return count;
}

static int msm_ioctl_gem_get(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_msm_hyp_gem *args = data;
	struct msm_file_private *ctx = file_priv->driver_priv;
	struct msm_dmabuf *dmabuf;
	struct dma_buf *dma_buf;
	__u64 dma_id;
	int found = 0;
	int ret = 0;

	dma_buf = dma_buf_get(args->fd);
	if (IS_ERR(dma_buf))
		return PTR_ERR(dma_buf);

	dma_id = (__u64)dma_buf;

	mutex_lock(&ctx->dmabuf_lock);
	list_for_each_entry(dmabuf, &ctx->dmabuf_list, node) {
		if (dma_id == dmabuf->dma_id) {
			args->handle = dmabuf->dma_id;
			args->size = dma_buf->size;
			found = 1;
			break;
		}
	}
	mutex_unlock(&ctx->dmabuf_lock);

	if (found)
		goto exit;

	dmabuf = kzalloc(sizeof(*dmabuf), GFP_KERNEL);
	if (!dmabuf) {
		ret = -ENOMEM;
		goto exit;
	}

	dmabuf->dma_id = (__u64)dma_buf;

	mutex_lock(&ctx->dmabuf_lock);
	list_add(&dmabuf->node, &ctx->dmabuf_list);
	mutex_unlock(&ctx->dmabuf_lock);

	args->handle = dmabuf->dma_id;
	args->size = dma_buf->size;
	return 0;

exit:
	dma_buf_put(dma_buf);
	return ret;
}

static int msm_ioctl_gem_query(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_msm_hyp_gem *args = data;
	struct msm_file_private *ctx = file_priv->driver_priv;
	struct msm_dmabuf *dmabuf;
	struct dma_buf *dma_buf;
	int ret = -ENOENT;

	args->size = 0;
	mutex_lock(&ctx->dmabuf_lock);
	list_for_each_entry(dmabuf, &ctx->dmabuf_list, node) {
		if (args->handle == dmabuf->dma_id) {
			dma_buf = (struct dma_buf *)dmabuf->dma_id;
			if (dma_buf->file)
				args->size = atomic_long_read(
					&dma_buf->file->f_count);
			ret = 0;
			break;
		}
	}
	mutex_unlock(&ctx->dmabuf_lock);

	return ret;
}

static int msm_ioctl_gem_put(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_msm_hyp_gem *args = data;
	struct msm_file_private *ctx = file_priv->driver_priv;
	struct msm_dmabuf *dmabuf;
	struct dma_buf *dma_buf;
	int ret = 0;

	mutex_lock(&ctx->dmabuf_lock);
	list_for_each_entry(dmabuf, &ctx->dmabuf_list, node) {
		if (args->handle == dmabuf->dma_id) {
			dma_buf = (struct dma_buf *)dmabuf->dma_id;
			dma_buf_put(dma_buf);
			list_del(&dmabuf->node);
			kfree(dmabuf);
			break;
		}
	}
	mutex_unlock(&ctx->dmabuf_lock);

	return ret;
}

static int _msm_parse_dt(struct device_node *node, u32 *client_id)
{
	int len = 0;
	int ret = 0;
	const char *client_id_str;

	client_id_str = of_get_property(node, "qcom,client-id", &len);
	if (!client_id_str || len != CLIENT_ID_LEN_IN_CHARS) {
		DBG("client_id_str len(%d) is invalid\n", len);
		ret = -EINVAL;
	} else {
		/* Try node as a hex value */
		ret = kstrtouint(client_id_str, 16, client_id);
		if (ret) {
			/* Otherwise, treat at 4cc code */
			*client_id = fourcc_code(client_id_str[0],
						client_id_str[1],
						client_id_str[2],
						client_id_str[3]);

			ret = 0;
		}
	}

	return ret;
}

static int msm_ioctl_query_client_id(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct platform_device *pdev = dev->platformdev;
	u32 *arg = (u32 *)data;
	int ret = 0;

	if (!pdev || !pdev->dev.of_node) {
		DBG("pdev not found\n");
		return -ENODEV;
	}

	ret = _msm_parse_dt(pdev->dev.of_node, arg);

	return ret;
}

static long msm_drm_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case DRM_IOCTL_PRIME_FD_TO_HANDLE:
	{
		struct drm_prime_handle cmd_data;

		if (copy_from_user(&cmd_data, (void __user *)arg,
				sizeof(struct drm_prime_handle)) != 0)
			return -EFAULT;
		cmd_data.handle = cmd_data.fd;
		if (copy_to_user((void __user *)arg, &cmd_data,
				sizeof(struct drm_prime_handle)) != 0)
			return -EFAULT;
		return 0;
	}
	case DRM_IOCTL_GEM_CLOSE:
		return 0;
	default:
		return drm_ioctl(filp, cmd, arg);
	}
}

static const struct file_operations fops = {
	.owner              = THIS_MODULE,
	.open               = drm_open,
	.release            = drm_release,
	.unlocked_ioctl     = msm_drm_ioctl,
	.poll               = drm_poll,
	.read               = drm_read,
	.write              = msm_drm_write,
	.llseek             = no_llseek,
};

static const struct drm_ioctl_desc msm_ioctls[] = {
	DRM_IOCTL_DEF_DRV(MSM_HYP_GEM_GET,  msm_ioctl_gem_get,
		DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_HYP_GEM_PUT,  msm_ioctl_gem_put,
		DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_HYP_GEM_QRY,  msm_ioctl_gem_query,
		DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_HYP_QRY_CLT_ID,  msm_ioctl_query_client_id,
		DRM_AUTH|DRM_RENDER_ALLOW),
};

static struct drm_driver msm_driver = {
	.driver_features    = 0,
	.load               = msm_load,
	.unload             = msm_unload,
	.open               = msm_open,
	.preclose           = msm_preclose,
	.set_busid          = drm_platform_set_busid,
	.get_vblank_counter = drm_vblank_no_hw_counter,
	.ioctls             = msm_ioctls,
	.num_ioctls         = ARRAY_SIZE(msm_ioctls),
	.fops               = &fops,
	.name               = "msm_drm_hyp",
	.desc               = "MSM Snapdragon DRM",
	.date               = "20170831",
	.major              = 1,
	.minor              = 0,
};

/*
 * Platform driver:
 */

static int msm_pdev_probe(struct platform_device *pdev)
{
	return drm_platform_init(&msm_driver, pdev);
}

static int msm_pdev_remove(struct platform_device *pdev)
{
	drm_put_dev(platform_get_drvdata(pdev));

	return 0;
}

static const struct platform_device_id msm_id[] = {
	{ "mdp-hyp", 0 },
	{ }
};

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,sde-kms-hyp" },
	{}
};
MODULE_DEVICE_TABLE(of, dt_match);

static struct platform_driver msm_platform_driver = {
	.probe      = msm_pdev_probe,
	.remove     = msm_pdev_remove,
	.driver     = {
		.name   = "msm_drm_hyp",
		.of_match_table = dt_match,
	},
	.id_table   = msm_id,
};

static int __init msm_drm_register(void)
{
	DBG("init");
	return platform_driver_register(&msm_platform_driver);
}

static void __exit msm_drm_unregister(void)
{
	DBG("fini");
	platform_driver_unregister(&msm_platform_driver);
}

module_init(msm_drm_register);
module_exit(msm_drm_unregister);

MODULE_DESCRIPTION("MSM DRM Mini Driver");
MODULE_LICENSE("GPL v2");
