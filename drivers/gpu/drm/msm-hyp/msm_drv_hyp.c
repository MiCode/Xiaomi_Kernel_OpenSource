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

	file->driver_priv = ctx;

	return 0;
}

static void msm_preclose(struct drm_device *dev, struct drm_file *file)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_file_private *ctx = file->driver_priv;

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

static size_t msm_drm_write(struct file *filp, const char __user *buffer,
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

static const struct file_operations fops = {
	.owner              = THIS_MODULE,
	.open               = drm_open,
	.release            = drm_release,
	.unlocked_ioctl     = drm_ioctl,
	.poll               = drm_poll,
	.read               = drm_read,
	.write              = msm_drm_write,
	.llseek             = no_llseek,
};

static struct drm_driver msm_driver = {
	.driver_features    = 0,
	.load               = msm_load,
	.unload             = msm_unload,
	.open               = msm_open,
	.preclose           = msm_preclose,
	.set_busid          = drm_platform_set_busid,
	.get_vblank_counter = drm_vblank_no_hw_counter,
	.num_ioctls         = 0,
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
