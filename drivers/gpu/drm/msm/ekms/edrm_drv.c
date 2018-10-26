/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
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

#include <linux/of_address.h>
#include <linux/sde_io_util.h>
#include "msm_drv.h"
#include "msm_gem.h"
#include "msm_mmu.h"
#include "edrm_kms.h"

static int msm_edrm_unload(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	int i;

	/* clean up display commit worker threads */
	for (i = 0; i < priv->num_crtcs; i++) {
		if (priv->disp_thread[i].thread) {
			flush_kthread_worker(&priv->disp_thread[i].worker);
			kthread_stop(priv->disp_thread[i].thread);
			priv->disp_thread[i].thread = NULL;
		}
	}

	drm_kms_helper_poll_fini(dev);
	drm_mode_config_cleanup(dev);

	flush_workqueue(priv->wq);
	destroy_workqueue(priv->wq);

	if (kms)
		kms->funcs->destroy(kms);

	dev->dev_private = NULL;

	kfree(priv);

	return 0;
}

static int msm_edrm_load(struct drm_device *dev, unsigned long flags)
{
	struct platform_device *pdev = dev->platformdev;
	struct msm_drm_private *priv;
	struct msm_kms *kms;
	struct drm_device *master_dev;
	struct msm_drm_private *master_priv;
	struct drm_minor *minor;
	int ret, i;
	struct sched_param param;

	/* main DRM's minor ID is zero */
	minor = drm_minor_acquire(0);
	if (IS_ERR(minor)) {
		pr_err("master drm_minor has no dev, stop early drm loading\n");
		return -ENODEV;
	}
	master_dev = minor->dev;
	drm_minor_release(minor);
	master_priv = master_dev->dev_private;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev->dev_private = priv;

	priv->wq = alloc_ordered_workqueue("msm_edrm", 0);
	init_waitqueue_head(&priv->fence_event);
	init_waitqueue_head(&priv->pending_crtcs_event);
	INIT_LIST_HEAD(&priv->client_event_list);
	INIT_LIST_HEAD(&priv->inactive_list);
	INIT_LIST_HEAD(&priv->fence_cbs);
	hash_init(priv->mn_hash);
	mutex_init(&priv->mn_lock);

	drm_mode_config_init(dev);

	platform_set_drvdata(pdev, dev);
	priv->pclient = master_priv->pclient;
	memcpy((void *)&priv->phandle.mp, (void *) &master_priv->phandle.mp,
		sizeof(struct dss_module_power));
	INIT_LIST_HEAD(&priv->phandle.power_client_clist);
	mutex_init(&priv->phandle.phandle_lock);

	priv->vram.size = 0;
	kms = msm_edrm_kms_init(dev);
	if (IS_ERR(kms)) {
		priv->kms = NULL;
		dev_err(dev->dev, "failed to load kms\n");
		ret = PTR_ERR(kms);
		goto fail;
	}

	priv->kms = kms;
	if (kms && kms->funcs && kms->funcs->hw_init) {
		ret = kms->funcs->hw_init(kms);
		if (ret) {
			dev_err(dev->dev, "kms hw init failed: %d\n", ret);
			goto fail;
		}
	}

	/**
	 * this priority was found during empiric testing to have appropriate
	 * realtime scheduling to process display updates and interact with
	 * other real time and normal priority task
	 */
	param.sched_priority = 16;
	/* initialize commit thread structure */
	for (i = 0; i < priv->num_crtcs; i++) {
		priv->disp_thread[i].crtc_id = priv->crtcs[i]->base.id;
		init_kthread_worker(&priv->disp_thread[i].worker);
		priv->disp_thread[i].dev = dev;
		priv->disp_thread[i].thread =
			kthread_run(kthread_worker_fn,
				&priv->disp_thread[i].worker,
				"crtc_commit:%d",
				priv->disp_thread[i].crtc_id);
		ret = sched_setscheduler(priv->disp_thread[i].thread,
							SCHED_FIFO, &param);
		if (ret)
			pr_warn("display thread priority update failed: %d\n",
									ret);

		if (IS_ERR(priv->disp_thread[i].thread)) {
			dev_err(dev->dev, "failed to create kthread\n");
			priv->disp_thread[i].thread = NULL;
			/* clean up previously created threads if any */
			for (i -= 1; i >= 0; i--) {
				kthread_stop(priv->disp_thread[i].thread);
				priv->disp_thread[i].thread = NULL;
			}
			goto fail;
		}
	}

	/* share same function from master drm */
	dev->mode_config.funcs = master_dev->mode_config.funcs;

	ret = drm_vblank_init(dev, priv->num_crtcs);
	if (ret < 0) {
		dev_err(dev->dev, "failed to initialize vblank\n");
		goto fail;
	}

	drm_mode_config_reset(dev);
	/* perform subdriver post initialization */
	if (kms && kms->funcs && kms->funcs->postinit) {
		ret = kms->funcs->postinit(kms);
		if (ret) {
			dev_err(dev->dev, "kms post init failed: %d\n", ret);
			goto fail;
		}
	}

	drm_kms_helper_poll_init(dev);
	return 0;

fail:
	msm_edrm_unload(dev);
	return ret;
}

static int msm_edrm_open(struct drm_device *dev, struct drm_file *file)
{
	struct msm_file_private *ctx = NULL;
	struct msm_drm_private *priv;
	struct msm_kms *kms;

	if (!dev || !dev->dev_private)
		return -ENODEV;
	priv = dev->dev_private;

	file->driver_priv = ctx;
	kms = priv->kms;

	if (kms) {
		struct msm_edrm_kms *edrm_kms;

		edrm_kms = to_edrm_kms(kms);
		/* return failure if eDRM already handoff display resource
		 * to main DRM
		 */
		if (edrm_kms->handoff_flag)
			return -ENODEV;
	}

	if (kms && kms->funcs && kms->funcs->postopen)
		kms->funcs->postopen(kms, file);

	return 0;
}

static void msm_preclose(struct drm_device *dev, struct drm_file *file)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;

	if (kms && kms->funcs && kms->funcs->preclose)
		kms->funcs->preclose(kms, file);
}

static void msm_postclose(struct drm_device *dev, struct drm_file *file)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_file_private *ctx = file->driver_priv;
	struct msm_kms *kms = priv->kms;

	if (kms && kms->funcs && kms->funcs->postclose)
		kms->funcs->postclose(kms, file);

	if (!ctx)
		return;

	kfree(ctx);
}

static void msm_lastclose(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;

	struct msm_kms *kms = priv->kms;

	/* wait for pending vblank requests to be executed by worker thread */
	flush_workqueue(priv->wq);

	if (kms && kms->funcs && kms->funcs->lastclose)
		kms->funcs->lastclose(kms);
}

static int msm_edrm_enable_vblank(struct drm_device *dev, unsigned int pipe)
{
	return 0;
}

static void msm_edrm_disable_vblank(struct drm_device *dev, unsigned int pipe)
{
}


static const struct vm_operations_struct vm_ops = {
	.fault = msm_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static const struct file_operations fops = {
	.owner              = THIS_MODULE,
	.open               = drm_open,
	.release            = drm_release,
	.unlocked_ioctl     = drm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl       = drm_compat_ioctl,
#endif
	.poll               = drm_poll,
	.read               = drm_read,
	.llseek             = no_llseek,
	.mmap               = msm_gem_mmap,
};

static struct drm_driver msm_edrm_driver = {
	.driver_features    = DRIVER_HAVE_IRQ |
				DRIVER_GEM |
				DRIVER_PRIME |
				DRIVER_RENDER |
				DRIVER_ATOMIC |
				DRIVER_MODESET,
	.load               = msm_edrm_load,
	.unload             = msm_edrm_unload,
	.open               = msm_edrm_open,
	.preclose           = msm_preclose,
	.postclose          = msm_postclose,
	.lastclose          = msm_lastclose,
	.set_busid          = drm_platform_set_busid,
	.get_vblank_counter = drm_vblank_no_hw_counter,
	.enable_vblank      = msm_edrm_enable_vblank,
	.disable_vblank     = msm_edrm_disable_vblank,
	.gem_free_object    = msm_gem_free_object,
	.gem_vm_ops         = &vm_ops,
	.dumb_create        = msm_gem_dumb_create,
	.dumb_map_offset    = msm_gem_dumb_map_offset,
	.dumb_destroy       = drm_gem_dumb_destroy,
	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export   = drm_gem_prime_export,
	.gem_prime_import   = drm_gem_prime_import,
	.gem_prime_res_obj  = msm_gem_prime_res_obj,
	.gem_prime_pin      = msm_gem_prime_pin,
	.gem_prime_unpin    = msm_gem_prime_unpin,
	.gem_prime_get_sg_table = msm_gem_prime_get_sg_table,
	.gem_prime_import_sg_table = msm_gem_prime_import_sg_table,
	.gem_prime_vmap     = msm_gem_prime_vmap,
	.gem_prime_vunmap   = msm_gem_prime_vunmap,
	.gem_prime_mmap     = msm_gem_prime_mmap,

	.ioctls             = NULL,
	.num_ioctls         = 0,
	.fops               = &fops,
	.name               = "msm",
	.desc               = "MSM Snapdragon DRM",
	.date               = "20181024",
	.major              = 1,
	.minor              = 1,
};

static int msm_pdev_edrm_probe(struct platform_device *pdev)
{
	int ret;
	struct drm_minor *minor;
	struct drm_device *master_dev;
	struct msm_drm_private *master_priv;
	struct msm_kms *master_kms;

	/* main DRM's minor ID is zero */
	minor = drm_minor_acquire(0);
	if (IS_ERR(minor)) {
		pr_err("drm_minor has no dev, defer the probe\n");
		return -EPROBE_DEFER;
	}
	master_dev = minor->dev;
	drm_minor_release(minor);
	if (!master_dev) {
		pr_err("master_dev is null, defer the probe\n");
		return -EPROBE_DEFER;
	}

	master_priv = master_dev->dev_private;
	if (!master_priv) {
		pr_err("master_priv is null, defer the probe\n");
		return -EPROBE_DEFER;
	}

	master_kms = master_priv->kms;
	if (!master_kms) {
		pr_err("master KMS is null, defer the probe\n");
		return -EPROBE_DEFER;
	}

	/* on all devices that I am aware of, iommu's which cna map
	 * any address the cpu can see are used:
	 */
	ret = dma_set_mask_and_coherent(&pdev->dev, ~0);
	if (ret) {
		pr_err("dma_set_mask_and_coherent return %d\n", ret);
		return ret;
	}

	ret = drm_platform_init(&msm_edrm_driver,
			to_platform_device(&pdev->dev));
	if (ret)
		DRM_ERROR("drm_platform_init failed: %d\n", ret);

	return ret;
}

static int msm_pdev_edrm_remove(struct platform_device *pdev)
{
	drm_put_dev(platform_get_drvdata(to_platform_device(&pdev->dev)));
	return 0;
}

static const struct platform_device_id msm_edrm_id[] = {
	{ "edrm_mdp", 0 },
	{ }
};

static void msm_edrm_lastclose(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;

	if (kms && kms->funcs && kms->funcs->lastclose)
		kms->funcs->lastclose(kms);
}

static void msm_pdev_edrm_shutdown(struct platform_device *pdev)
{
	struct drm_device *ddev = platform_get_drvdata(pdev);
	struct msm_drm_private *priv = NULL;

	priv = ddev->dev_private;
	msm_edrm_lastclose(ddev);

	/* set this after lastclose to allow kickoff from lastclose */
	priv->shutdown_in_progress = true;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,msm-kms-edrm" },  /* sde  */
	{}
};
MODULE_DEVICE_TABLE(of, dt_match);

static struct platform_driver msm_platform_driver = {
	.probe      = msm_pdev_edrm_probe,
	.remove     = msm_pdev_edrm_remove,
	.shutdown   = msm_pdev_edrm_shutdown,
	.driver     = {
		.name   = "msm_early_drm",
		.of_match_table = dt_match,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.id_table   = msm_edrm_id,
};

static int __init msm_edrm_register(void)
{
	DBG("init");
	return platform_driver_register(&msm_platform_driver);
}

static void __exit msm_edrm_unregister(void)
{
	DBG("fini");
	platform_driver_unregister(&msm_platform_driver);
}

module_init(msm_edrm_register);
module_exit(msm_edrm_unregister);

MODULE_DESCRIPTION("MSM EARLY DRM Driver");
MODULE_LICENSE("GPL v2");
