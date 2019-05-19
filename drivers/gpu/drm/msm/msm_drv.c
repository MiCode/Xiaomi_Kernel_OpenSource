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
/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission. The copyright holders make no representations
 * about the suitability of this software for any purpose. It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <linux/of_address.h>
#include "msm_drv.h"
#include "msm_gpu.h"
#include "msm_kms.h"
#include "sde_wb.h"

#define TEARDOWN_DEADLOCK_RETRY_MAX 5
#include "msm_gem.h"
#include "msm_mmu.h"

static struct completion wait_display_completion;
static bool msm_drm_probed;

static void msm_drm_helper_hotplug_event(struct drm_device *dev)
{
	struct drm_connector *connector;
	char *event_string;
	char const *connector_name;
	char *envp[2];

	if (!dev) {
		DRM_ERROR("hotplug_event failed, invalid input\n");
		return;
	}

	if (!dev->mode_config.poll_enabled)
		return;

	event_string = kzalloc(SZ_4K, GFP_KERNEL);
	if (!event_string) {
		DRM_ERROR("failed to allocate event string\n");
		return;
	}

	mutex_lock(&dev->mode_config.mutex);
	drm_for_each_connector(connector, dev) {
		/* Only handle HPD capable connectors. */
		if (!(connector->polled & DRM_CONNECTOR_POLL_HPD))
			continue;

		connector->status = connector->funcs->detect(connector, false);

		if (connector->name)
			connector_name = connector->name;
		else
			connector_name = "unknown";

		snprintf(event_string, SZ_4K, "name=%s status=%s\n",
			connector_name,
			drm_get_connector_status_name(connector->status));
		DRM_DEBUG("generating hotplug event [%s]\n", event_string);
		envp[0] = event_string;
		envp[1] = NULL;
		kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE,
				envp);
	}
	mutex_unlock(&dev->mode_config.mutex);
	kfree(event_string);
}

static void msm_fb_output_poll_changed(struct drm_device *dev)
{
	struct msm_drm_private *priv = NULL;

	if (!dev) {
		DRM_ERROR("output_poll_changed failed, invalid input\n");
		return;
	}

	priv = dev->dev_private;

	if (priv->fbdev)
		drm_fb_helper_hotplug_event(priv->fbdev);
	else
		msm_drm_helper_hotplug_event(dev);
}

static const struct drm_mode_config_funcs mode_config_funcs = {
	.fb_create = msm_framebuffer_create,
	.output_poll_changed = msm_fb_output_poll_changed,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = msm_atomic_commit,
};

#ifdef CONFIG_DRM_MSM_REGISTER_LOGGING
static bool reglog = false;
MODULE_PARM_DESC(reglog, "Enable register read/write logging");
module_param(reglog, bool, 0600);
#else
#define reglog 0
#endif

#ifdef CONFIG_DRM_FBDEV_EMULATION
static bool fbdev = true;
MODULE_PARM_DESC(fbdev, "Enable fbdev compat layer");
module_param(fbdev, bool, 0600);
#endif

static char *vram = "16m";
MODULE_PARM_DESC(vram, "Configure VRAM size (for devices without IOMMU/GPUMMU");
module_param(vram, charp, 0);

/*
 * Util/helpers:
 */

void __iomem *msm_ioremap(struct platform_device *pdev, const char *name,
		const char *dbgname)
{
	struct resource *res;
	unsigned long size;
	void __iomem *ptr;

	if (name)
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	else
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		dev_err(&pdev->dev, "failed to get memory resource: %s\n", name);
		return ERR_PTR(-EINVAL);
	}

	size = resource_size(res);

	ptr = devm_ioremap_nocache(&pdev->dev, res->start, size);
	if (!ptr) {
		dev_err(&pdev->dev, "failed to ioremap: %s\n", name);
		return ERR_PTR(-ENOMEM);
	}

	if (reglog)
		dev_dbg(&pdev->dev, "IO:region %s %pK %08lx\n",
			dbgname, ptr, size);

	return ptr;
}

void msm_iounmap(struct platform_device *pdev, void __iomem *addr)
{
	devm_iounmap(&pdev->dev, addr);
}

void msm_writel(u32 data, void __iomem *addr)
{
	if (reglog)
		pr_debug("IO:W %pK %08x\n", addr, data);
	writel(data, addr);
}

u32 msm_readl(const void __iomem *addr)
{
	u32 val = readl(addr);
	if (reglog)
		pr_err("IO:R %pK %08x\n", addr, val);
	return val;
}

struct vblank_event {
	struct list_head node;
	int crtc_id;
	bool enable;
};

static void vblank_ctrl_worker(struct kthread_work *work)
{
	struct msm_vblank_ctrl *vbl_ctrl = container_of(work,
						struct msm_vblank_ctrl, work);
	struct msm_drm_private *priv = container_of(vbl_ctrl,
					struct msm_drm_private, vblank_ctrl);
	struct msm_kms *kms = priv->kms;
	struct vblank_event *vbl_ev, *tmp;
	unsigned long flags;
	LIST_HEAD(tmp_head);

	spin_lock_irqsave(&vbl_ctrl->lock, flags);
	list_for_each_entry_safe(vbl_ev, tmp, &vbl_ctrl->event_list, node) {
		list_del(&vbl_ev->node);
		list_add_tail(&vbl_ev->node, &tmp_head);
	}
	spin_unlock_irqrestore(&vbl_ctrl->lock, flags);

	list_for_each_entry_safe(vbl_ev, tmp, &tmp_head, node) {
		if (vbl_ev->enable)
			kms->funcs->enable_vblank(kms,
						priv->crtcs[vbl_ev->crtc_id]);
		else
			kms->funcs->disable_vblank(kms,
						priv->crtcs[vbl_ev->crtc_id]);

		kfree(vbl_ev);
	}
}

static int vblank_ctrl_queue_work(struct msm_drm_private *priv,
					int crtc_id, bool enable)
{
	struct msm_vblank_ctrl *vbl_ctrl = &priv->vblank_ctrl;
	struct vblank_event *vbl_ev;
	unsigned long flags;

	vbl_ev = kzalloc(sizeof(*vbl_ev), GFP_ATOMIC);
	if (!vbl_ev)
		return -ENOMEM;

	vbl_ev->crtc_id = crtc_id;
	vbl_ev->enable = enable;

	spin_lock_irqsave(&vbl_ctrl->lock, flags);
	list_add_tail(&vbl_ev->node, &vbl_ctrl->event_list);
	spin_unlock_irqrestore(&vbl_ctrl->lock, flags);

	queue_kthread_work(&priv->disp_thread[crtc_id].worker, &vbl_ctrl->work);

	return 0;
}

/*
 * DRM operations:
 */

static int msm_unload(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct platform_device *pdev = dev->platformdev;
	struct msm_kms *kms = priv->kms;
	struct msm_gpu *gpu = priv->gpu;
	struct msm_vblank_ctrl *vbl_ctrl = &priv->vblank_ctrl;
	struct vblank_event *vbl_ev, *tmp;
	int i;

	/* We must cancel and cleanup any pending vblank enable/disable
	 * work before drm_irq_uninstall() to avoid work re-enabling an
	 * irq after uninstall has disabled it.
	 */
	flush_kthread_work(&vbl_ctrl->work);
	list_for_each_entry_safe(vbl_ev, tmp, &vbl_ctrl->event_list, node) {
		list_del(&vbl_ev->node);
		kfree(vbl_ev);
	}

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
	drm_vblank_cleanup(dev);

	pm_runtime_get_sync(dev->dev);
	drm_irq_uninstall(dev);
	pm_runtime_put_sync(dev->dev);

	flush_workqueue(priv->wq);
	destroy_workqueue(priv->wq);

	if (kms) {
		pm_runtime_disable(dev->dev);
		kms->funcs->destroy(kms);
	}

	if (gpu) {
		mutex_lock(&dev->struct_mutex);
		/*
		 * XXX what do we do here?
		 * pm_runtime_enable(&pdev->dev);
		 */
		gpu->funcs->pm_suspend(gpu);
		mutex_unlock(&dev->struct_mutex);
		gpu->funcs->destroy(gpu);
	}

	if (priv->vram.paddr) {
		DEFINE_DMA_ATTRS(attrs);
		dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
		drm_mm_takedown(&priv->vram.mm);
		dma_free_attrs(dev->dev, priv->vram.size, NULL,
				priv->vram.paddr, &attrs);
	}

	sde_dbg_destroy();

	sde_power_client_destroy(&priv->phandle, priv->pclient);
	sde_power_resource_deinit(pdev, &priv->phandle);

	component_unbind_all(dev->dev, dev);

	dev->dev_private = NULL;

	kfree(priv);

	return 0;
}

#define KMS_MDP4 0
#define KMS_SDE  1

static int get_mdp_ver(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	static const struct of_device_id match_types[] = { {
		.compatible = "qcom,sde-kms",
		.data	= (void	*)KMS_SDE,
	},
	{} };
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	match = of_match_node(match_types, dev->of_node);
	if (match)
		return (int)(unsigned long)match->data;
#endif
	return KMS_MDP4;
}

static int msm_init_vram(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	unsigned long size = 0;
	int ret = 0;

#ifdef CONFIG_OF
	/* In the device-tree world, we could have a 'memory-region'
	 * phandle, which gives us a link to our "vram".  Allocating
	 * is all nicely abstracted behind the dma api, but we need
	 * to know the entire size to allocate it all in one go. There
	 * are two cases:
	 *  1) device with no IOMMU, in which case we need exclusive
	 *     access to a VRAM carveout big enough for all gpu
	 *     buffers
	 *  2) device with IOMMU, but where the bootloader puts up
	 *     a splash screen.  In this case, the VRAM carveout
	 *     need only be large enough for fbdev fb.  But we need
	 *     exclusive access to the buffer to avoid the kernel
	 *     using those pages for other purposes (which appears
	 *     as corruption on screen before we have a chance to
	 *     load and do initial modeset)
	 */
	struct device_node *node;

	node = of_parse_phandle(dev->dev->of_node, "memory-region", 0);
	if (node) {
		struct resource r;
		ret = of_address_to_resource(node, 0, &r);
		if (ret)
			return ret;
		size = r.end - r.start;
		DRM_INFO("using VRAM carveout: %lx@%pa\n", size, &r.start);
	} else
#endif

	/* if we have no IOMMU, then we need to use carveout allocator.
	 * Grab the entire CMA chunk carved out in early startup in
	 * mach-msm:
	 */
	if (!iommu_present(&platform_bus_type)) {
		DRM_INFO("using %s VRAM carveout\n", vram);
		size = memparse(vram, NULL);
	}

	if (size) {
		DEFINE_DMA_ATTRS(attrs);
		void *p;

		priv->vram.size = size;

		drm_mm_init(&priv->vram.mm, 0, (size >> PAGE_SHIFT) - 1);
		spin_lock_init(&priv->vram.lock);

		dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
		dma_set_attr(DMA_ATTR_WRITE_COMBINE, &attrs);

		/* note that for no-kernel-mapping, the vaddr returned
		 * is bogus, but non-null if allocation succeeded:
		 */
		p = dma_alloc_attrs(dev->dev, size,
				&priv->vram.paddr, GFP_KERNEL, &attrs);
		if (!p) {
			dev_err(dev->dev, "failed to allocate VRAM\n");
			priv->vram.paddr = 0;
			return -ENOMEM;
		}

		dev_info(dev->dev, "VRAM: %08x->%08x\n",
				(uint32_t)priv->vram.paddr,
				(uint32_t)(priv->vram.paddr + size));
	}

	return ret;
}

#ifdef CONFIG_OF
static int msm_component_bind_all(struct device *dev,
				struct drm_device *drm_dev)
{
	int ret;

	ret = component_bind_all(dev, drm_dev);
	if (ret)
		DRM_ERROR("component_bind_all failed: %d\n", ret);

	return ret;
}
#else
static int msm_component_bind_all(struct device *dev,
				struct drm_device *drm_dev)
{
	return 0;
}
#endif

static int msm_power_enable_wrapper(void *handle, void *client, bool enable)
{
	return sde_power_resource_enable(handle, client, enable);
}

static int msm_load(struct drm_device *dev, unsigned long flags)
{
	struct platform_device *pdev = dev->platformdev;
	struct msm_drm_private *priv;
	struct msm_kms *kms;
	struct sde_dbg_power_ctrl dbg_power_ctrl = { NULL };
	int ret, i;
	struct sched_param param;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev->dev, "failed to allocate private data\n");
		return -ENOMEM;
	}

	dev->dev_private = priv;

	priv->wq = alloc_ordered_workqueue("msm_drm", 0);
	init_waitqueue_head(&priv->fence_event);
	init_waitqueue_head(&priv->pending_crtcs_event);

	INIT_LIST_HEAD(&priv->client_event_list);
	INIT_LIST_HEAD(&priv->inactive_list);
	INIT_LIST_HEAD(&priv->fence_cbs);
	INIT_LIST_HEAD(&priv->vblank_ctrl.event_list);
	init_kthread_work(&priv->vblank_ctrl.work, vblank_ctrl_worker);
	spin_lock_init(&priv->vblank_ctrl.lock);
	hash_init(priv->mn_hash);
	mutex_init(&priv->mn_lock);

	drm_mode_config_init(dev);

	platform_set_drvdata(pdev, dev);

	ret = sde_power_resource_init(pdev, &priv->phandle);
	if (ret) {
		pr_err("sde power resource init failed\n");
		goto fail;
	}

	priv->pclient = sde_power_client_create(&priv->phandle, "sde");
	if (IS_ERR_OR_NULL(priv->pclient)) {
		pr_err("sde power client create failed\n");
		ret = -EINVAL;
		goto fail;
	}

	/* Bind all our sub-components: */
	ret = msm_component_bind_all(dev->dev, dev);
	if (ret)
		return ret;

	ret = msm_init_vram(dev);
	if (ret)
		goto fail;

	dbg_power_ctrl.handle = &priv->phandle;
	dbg_power_ctrl.client = priv->pclient;
	dbg_power_ctrl.enable_fn = msm_power_enable_wrapper;
	ret = sde_dbg_init(dev->primary->debugfs_root, &pdev->dev,
			&dbg_power_ctrl);
	if (ret) {
		dev_err(dev->dev, "failed to init sde dbg: %d\n", ret);
		goto fail;
	}

	switch (get_mdp_ver(pdev)) {
	case KMS_MDP4:
		kms = mdp4_kms_init(dev);
		break;
	case KMS_SDE:
		kms = sde_kms_init(dev);
		break;
	default:
		kms = ERR_PTR(-ENODEV);
		break;
	}

	if (IS_ERR(kms)) {
		/*
		 * NOTE: once we have GPU support, having no kms should not
		 * be considered fatal.. ideally we would still support gpu
		 * and (for example) use dmabuf/prime to share buffers with
		 * imx drm driver on iMX5
		 */
		priv->kms = NULL;
		dev_err(dev->dev, "failed to load kms\n");
		ret = PTR_ERR(kms);
		goto fail;
	}

	priv->kms = kms;
	pm_runtime_enable(dev->dev);

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

	dev->mode_config.funcs = &mode_config_funcs;

	ret = drm_vblank_init(dev, priv->num_crtcs);
	if (ret < 0) {
		dev_err(dev->dev, "failed to initialize vblank\n");
		goto fail;
	}

	pm_runtime_get_sync(dev->dev);
	ret = drm_irq_install(dev, platform_get_irq(dev->platformdev, 0));
	pm_runtime_put_sync(dev->dev);
	if (ret < 0) {
		dev_err(dev->dev, "failed to install IRQ handler\n");
		goto fail;
	}

	drm_mode_config_reset(dev);

#ifdef CONFIG_DRM_FBDEV_EMULATION
	if (fbdev)
		priv->fbdev = msm_fbdev_init(dev);
#endif

	ret = msm_debugfs_late_init(dev);
	if (ret)
		goto fail;

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
	msm_unload(dev);
	return ret;
}

#ifdef CONFIG_QCOM_KGSL
static void load_gpu(struct drm_device *dev)
{
}
#else
static void load_gpu(struct drm_device *dev)
{
	static DEFINE_MUTEX(init_lock);
	struct msm_drm_private *priv = dev->dev_private;

	mutex_lock(&init_lock);

	if (!priv->gpu)
		priv->gpu = adreno_load_gpu(dev);

	mutex_unlock(&init_lock);
}
#endif

static struct msm_file_private *setup_pagetable(struct msm_drm_private *priv)
{
	struct msm_file_private *ctx;

	if (!priv || !priv->gpu)
		return NULL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ctx->aspace = msm_gem_address_space_create_instance(
		priv->gpu->aspace->mmu, "gpu", 0x100000000ULL,
		TASK_SIZE_64 - 1);

	if (IS_ERR(ctx->aspace)) {
		int ret = PTR_ERR(ctx->aspace);

		/*
		 * If dynamic domains are not supported, everybody uses the
		 * same pagetable
		 */
		if (ret != -EOPNOTSUPP) {
			kfree(ctx);
			return ERR_PTR(ret);
		}

		ctx->aspace = priv->gpu->aspace;
	}

	ctx->aspace->mmu->funcs->attach(ctx->aspace->mmu, NULL, 0);
	return ctx;
}

static int msm_open(struct drm_device *dev, struct drm_file *file)
{
	struct msm_file_private *ctx = NULL;
	struct msm_drm_private *priv;
	struct msm_kms *kms;

	if (!dev || !dev->dev_private)
		return -ENODEV;

	priv = dev->dev_private;
	/* For now, load gpu on open.. to avoid the requirement of having
	 * firmware in the initrd.
	 */
	load_gpu(dev);

	ctx = setup_pagetable(priv);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	if (ctx) {
		INIT_LIST_HEAD(&ctx->counters);
		msm_submitqueue_init(ctx);
	}

	file->driver_priv = ctx;

	kms = priv->kms;

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

	msm_submitqueue_close(ctx);

	if (priv->gpu) {
		msm_gpu_cleanup_counters(priv->gpu, ctx);

		if (ctx->aspace && ctx->aspace != priv->gpu->aspace) {
			ctx->aspace->mmu->funcs->detach(ctx->aspace->mmu);
			msm_gem_address_space_put(ctx->aspace);
		}
	}

	kfree(ctx);
}

static int msm_disable_all_modes_commit(
		struct drm_device *dev,
		struct drm_atomic_state *state)
{
	struct drm_plane *plane;
	struct drm_crtc *crtc;
	unsigned plane_mask;
	int ret;

	plane_mask = 0;
	drm_for_each_plane(plane, dev) {
		struct drm_plane_state *plane_state;

		plane_state = drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(plane_state)) {
			ret = PTR_ERR(plane_state);
			goto fail;
		}

		plane_state->rotation = BIT(DRM_ROTATE_0);

		plane->old_fb = plane->fb;
		plane_mask |= 1 << drm_plane_index(plane);

		/* disable non-primary: */
		if (plane->type == DRM_PLANE_TYPE_PRIMARY)
			continue;

		DRM_DEBUG("disabling plane %d\n", plane->base.id);

		ret = __drm_atomic_helper_disable_plane(plane, plane_state);
		if (ret != 0)
			DRM_ERROR("error %d disabling plane %d\n", ret,
					plane->base.id);
	}

	drm_for_each_crtc(crtc, dev) {
		struct drm_mode_set mode_set;

		memset(&mode_set, 0, sizeof(struct drm_mode_set));
		mode_set.crtc = crtc;

		DRM_DEBUG("disabling crtc %d\n", crtc->base.id);

		ret = __drm_atomic_helper_set_config(&mode_set, state);
		if (ret != 0)
			DRM_ERROR("error %d disabling crtc %d\n", ret,
					crtc->base.id);
	}

	DRM_DEBUG("committing disables\n");
	ret = drm_atomic_commit(state);

fail:
	drm_atomic_clean_old_fb(dev, plane_mask, ret);
	DRM_DEBUG("disables result %d\n", ret);
	return ret;
}

/**
 * msm_clear_all_modes - disables all planes and crtcs via an atomic commit
 *	based on restore_fbdev_mode_atomic in drm_fb_helper.c
 * @dev: device pointer
 * @Return: 0 on success, otherwise -error
 */
static int msm_disable_all_modes(struct drm_device *dev)
{
	struct drm_atomic_state *state;
	int ret, i;

	state = drm_atomic_state_alloc(dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = dev->mode_config.acquire_ctx;

	for (i = 0; i < TEARDOWN_DEADLOCK_RETRY_MAX; i++) {
		ret = msm_disable_all_modes_commit(dev, state);
		if (ret != -EDEADLK)
			break;
		drm_atomic_state_clear(state);
		drm_atomic_legacy_backoff(state);
	}

	/* on successful atomic commit state ownership transfers to framework */
	if (ret != 0)
		drm_atomic_state_free(state);

	return ret;
}

static void msm_lastclose(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	int i;

	/*
	 * clean up vblank disable immediately as this is the last close.
	 */
	for (i = 0; i < dev->num_crtcs; i++) {
		struct drm_vblank_crtc *vblank = &dev->vblank[i];
		struct timer_list *disable_timer = &vblank->disable_timer;

		if (del_timer_sync(disable_timer))
			disable_timer->function(disable_timer->data);
	}

	/* wait for pending vblank requests to be executed by worker thread */
	flush_workqueue(priv->wq);

	if (priv->fbdev) {
		drm_fb_helper_restore_fbdev_mode_unlocked(priv->fbdev);
	} else {
		drm_modeset_lock_all(dev);
		msm_disable_all_modes(dev);
		drm_modeset_unlock_all(dev);
		if (kms && kms->funcs && kms->funcs->lastclose)
			kms->funcs->lastclose(kms);
	}
}

static irqreturn_t msm_irq(int irq, void *arg)
{
	struct drm_device *dev = arg;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	BUG_ON(!kms);
	return kms->funcs->irq(kms);
}

static void msm_irq_preinstall(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	BUG_ON(!kms);
	kms->funcs->irq_preinstall(kms);
}

static int msm_irq_postinstall(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	BUG_ON(!kms);
	return kms->funcs->irq_postinstall(kms);
}

static void msm_irq_uninstall(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	BUG_ON(!kms);
	kms->funcs->irq_uninstall(kms);
}

static int msm_enable_vblank(struct drm_device *dev, unsigned int pipe)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	if (!kms)
		return -ENXIO;
	DBG("dev=%pK, crtc=%u", dev, pipe);
	return vblank_ctrl_queue_work(priv, pipe, true);
}

static void msm_disable_vblank(struct drm_device *dev, unsigned int pipe)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	if (!kms)
		return;
	DBG("dev=%pK, crtc=%u", dev, pipe);
	vblank_ctrl_queue_work(priv, pipe, false);
}

/*
 * DRM debugfs:
 */

#ifdef CONFIG_DEBUG_FS
static int msm_gpu_show(struct drm_device *dev, struct seq_file *m)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_gpu *gpu = priv->gpu;

	if (gpu) {
		seq_printf(m, "%s Status:\n", gpu->name);
		pm_runtime_get_sync(&gpu->pdev->dev);
		gpu->funcs->show(gpu, m);
		pm_runtime_put_sync(&gpu->pdev->dev);
	}

	return 0;
}

static int msm_snapshot_show(struct drm_device *dev, struct seq_file *m)
{
	struct msm_drm_private *priv = dev->dev_private;

	return msm_snapshot_write(priv->gpu, m);
}

static int msm_gem_show(struct drm_device *dev, struct seq_file *m)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_gpu *gpu = priv->gpu;

	if (gpu) {
		seq_printf(m, "Active Objects (%s):\n", gpu->name);
		msm_gem_describe_objects(&gpu->active_list, m);
	}

	seq_printf(m, "Inactive Objects:\n");
	msm_gem_describe_objects(&priv->inactive_list, m);

	return 0;
}

static int msm_mm_show(struct drm_device *dev, struct seq_file *m)
{
	return drm_mm_dump_table(m, &dev->vma_offset_manager->vm_addr_space_mm);
}

static int msm_fb_show(struct drm_device *dev, struct seq_file *m)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct drm_framebuffer *fb, *fbdev_fb = NULL;

	if (priv->fbdev) {
		seq_printf(m, "fbcon ");
		fbdev_fb = priv->fbdev->fb;
		msm_framebuffer_describe(fbdev_fb, m);
	}

	mutex_lock(&dev->mode_config.fb_lock);
	list_for_each_entry(fb, &dev->mode_config.fb_list, head) {
		if (fb == fbdev_fb)
			continue;

		seq_printf(m, "user ");
		msm_framebuffer_describe(fb, m);
	}
	mutex_unlock(&dev->mode_config.fb_lock);

	return 0;
}

static int show_locked(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	int (*show)(struct drm_device *dev, struct seq_file *m) =
			node->info_ent->data;
	int ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret)
		return ret;

	ret = show(dev, m);

	mutex_unlock(&dev->struct_mutex);

	return ret;
}

static int show_unlocked(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	int (*show)(struct drm_device *dev, struct seq_file *m) =
			node->info_ent->data;

	return show(dev, m);
}

static struct drm_info_list msm_debugfs_list[] = {
		{"gpu", show_locked, 0, msm_gpu_show},
		{"gem", show_locked, 0, msm_gem_show},
		{ "mm", show_locked, 0, msm_mm_show },
		{ "fb", show_locked, 0, msm_fb_show },
		{ "snapshot", show_unlocked, 0, msm_snapshot_show },
};

static int late_init_minor(struct drm_minor *minor)
{
	int ret;

	if (!minor)
		return 0;

	ret = msm_rd_debugfs_init(minor);
	if (ret) {
		dev_err(minor->dev->dev, "could not install rd debugfs\n");
		return ret;
	}

	ret = msm_perf_debugfs_init(minor);
	if (ret) {
		dev_err(minor->dev->dev, "could not install perf debugfs\n");
		return ret;
	}

	return 0;
}

int msm_debugfs_late_init(struct drm_device *dev)
{
	int ret;
	ret = late_init_minor(dev->primary);
	if (ret)
		return ret;
	ret = late_init_minor(dev->render);
	if (ret)
		return ret;
	ret = late_init_minor(dev->control);
	return ret;
}

static int msm_debugfs_init(struct drm_minor *minor)
{
	struct drm_device *dev = minor->dev;
	int ret;

	ret = drm_debugfs_create_files(msm_debugfs_list,
			ARRAY_SIZE(msm_debugfs_list),
			minor->debugfs_root, minor);

	if (ret) {
		dev_err(dev->dev, "could not install msm_debugfs_list\n");
		return ret;
	}

	return 0;
}

static void msm_debugfs_cleanup(struct drm_minor *minor)
{
	drm_debugfs_remove_files(msm_debugfs_list,
			ARRAY_SIZE(msm_debugfs_list), minor);
	if (!minor->dev->dev_private)
		return;
	msm_rd_debugfs_cleanup(minor);
	msm_perf_debugfs_cleanup(minor);
}
#endif

/*
 * Fences:
 */

int msm_wait_fence(struct drm_device *dev, uint32_t fence,
		ktime_t *timeout , bool interruptible)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_gpu *gpu = priv->gpu;
	int index = FENCE_RING(fence);
	uint32_t submitted;
	int ret;

	if (!gpu)
		return -ENXIO;

	if (index > MSM_GPU_MAX_RINGS || index >= gpu->nr_rings ||
		!gpu->rb[index])
		return -EINVAL;

	submitted = gpu->funcs->submitted_fence(gpu, gpu->rb[index]);

	if (fence > submitted) {
		DRM_ERROR("waiting on invalid fence: %u (of %u)\n",
			fence, submitted);
		return -EINVAL;
	}

	if (!timeout) {
		/* no-wait: */
		ret = fence_completed(dev, fence) ? 0 : -EBUSY;
	} else {
		ktime_t now = ktime_get();
		unsigned long remaining_jiffies;

		if (ktime_compare(*timeout, now) < 0) {
			remaining_jiffies = 0;
		} else {
			ktime_t rem = ktime_sub(*timeout, now);
			struct timespec ts = ktime_to_timespec(rem);
			remaining_jiffies = timespec_to_jiffies(&ts);
		}

		if (interruptible)
			ret = wait_event_interruptible_timeout(priv->fence_event,
				fence_completed(dev, fence),
				remaining_jiffies);
		else
			ret = wait_event_timeout(priv->fence_event,
				fence_completed(dev, fence),
				remaining_jiffies);

		if (ret == 0) {
			DBG("timeout waiting for fence: %u (completed: %u)",
					fence, priv->completed_fence[index]);
			ret = -ETIMEDOUT;
		} else if (ret != -ERESTARTSYS) {
			ret = 0;
		}
	}

	return ret;
}

int msm_queue_fence_cb(struct drm_device *dev,
		struct msm_fence_cb *cb, uint32_t fence)
{
	struct msm_drm_private *priv = dev->dev_private;
	int index = FENCE_RING(fence);
	int ret = 0;

	mutex_lock(&dev->struct_mutex);
	if (!list_empty(&cb->work.entry)) {
		ret = -EINVAL;
	} else if (fence > priv->completed_fence[index]) {
		cb->fence = fence;
		list_add_tail(&cb->work.entry, &priv->fence_cbs);
	} else {
		queue_work(priv->wq, &cb->work);
	}
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

/* called from workqueue */
void msm_update_fence(struct drm_device *dev, uint32_t fence)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_fence_cb *cb, *tmp;
	int index = FENCE_RING(fence);

	if (index >= MSM_GPU_MAX_RINGS)
		return;

	mutex_lock(&dev->struct_mutex);
	priv->completed_fence[index] = max(fence, priv->completed_fence[index]);

	list_for_each_entry_safe(cb, tmp, &priv->fence_cbs, work.entry) {
		if (COMPARE_FENCE_LTE(cb->fence,
			priv->completed_fence[index])) {
			list_del_init(&cb->work.entry);
			queue_work(priv->wq, &cb->work);
		}
	}

	mutex_unlock(&dev->struct_mutex);

	wake_up_all(&priv->fence_event);
}

void __msm_fence_worker(struct work_struct *work)
{
	struct msm_fence_cb *cb = container_of(work, struct msm_fence_cb, work);
	cb->func(cb);
}

/*
 * DRM ioctls:
 */

static int msm_ioctl_get_param(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct drm_msm_param *args = data;
	struct msm_gpu *gpu;

	/* for now, we just have 3d pipe.. eventually this would need to
	 * be more clever to dispatch to appropriate gpu module:
	 */
	if (args->pipe != MSM_PIPE_3D0)
		return -EINVAL;

	gpu = priv->gpu;

	if (!gpu)
		return -ENXIO;

	return gpu->funcs->get_param(gpu, args->param, &args->value);
}

static int msm_ioctl_gem_new(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct drm_msm_gem_new *args = data;

	if (args->flags & ~MSM_BO_FLAGS) {
		DRM_ERROR("invalid flags: %08x\n", args->flags);
		return -EINVAL;
	}

	return msm_gem_new_handle(dev, file, args->size,
			args->flags, &args->handle);
}

static int msm_ioctl_gem_svm_new(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct drm_msm_gem_svm_new *args = data;

	if (args->flags & ~MSM_BO_FLAGS) {
		DRM_ERROR("invalid flags: %08x\n", args->flags);
		return -EINVAL;
	}

	return msm_gem_svm_new_handle(dev, file, args->hostptr, args->size,
			args->flags, &args->handle);
}

static inline ktime_t to_ktime(struct drm_msm_timespec timeout)
{
	return ktime_set(timeout.tv_sec, timeout.tv_nsec);
}

static int msm_ioctl_gem_cpu_prep(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct drm_msm_gem_cpu_prep *args = data;
	struct drm_gem_object *obj;
	ktime_t timeout = to_ktime(args->timeout);
	int ret;

	if (args->op & ~MSM_PREP_FLAGS) {
		DRM_ERROR("invalid op: %08x\n", args->op);
		return -EINVAL;
	}

	obj = drm_gem_object_lookup(dev, file, args->handle);
	if (!obj)
		return -ENOENT;

	ret = msm_gem_cpu_prep(obj, args->op, &timeout);

	drm_gem_object_unreference_unlocked(obj);

	return ret;
}

static int msm_ioctl_gem_cpu_fini(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct drm_msm_gem_cpu_fini *args = data;
	struct drm_gem_object *obj;
	int ret;

	obj = drm_gem_object_lookup(dev, file, args->handle);
	if (!obj)
		return -ENOENT;

	ret = msm_gem_cpu_fini(obj);

	drm_gem_object_unreference_unlocked(obj);

	return ret;
}

static int msm_ioctl_gem_info(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct drm_msm_gem_info *args = data;
	struct drm_gem_object *obj;
	struct msm_gem_object *msm_obj;
	struct msm_file_private *ctx = file->driver_priv;
	int ret = 0;

	if (args->flags & ~MSM_INFO_FLAGS)
		return -EINVAL;

	obj = drm_gem_object_lookup(dev, file, args->handle);
	if (!obj)
		return -ENOENT;

	msm_obj = to_msm_bo(obj);
	if (args->flags & MSM_INFO_IOVA) {
		struct msm_gem_address_space *aspace = NULL;
		struct msm_drm_private *priv = dev->dev_private;
		uint64_t iova;

		if (msm_obj->flags & MSM_BO_SECURE && priv->gpu)
			aspace = priv->gpu->secure_aspace;
		else if (ctx)
			aspace = ctx->aspace;

		if (!aspace) {
			ret = -EINVAL;
			goto out;
		}

		ret = msm_gem_get_iova(obj, aspace, &iova);
		if (!ret)
			args->offset = iova;
	} else {
		if (msm_obj->flags & MSM_BO_SVM) {
			/*
			 * Offset for an SVM object is not needed as they are
			 * already mmap'ed before the SVM ioctl is invoked.
			 */
			ret = -EACCES;
			goto out;
		}
		args->offset = msm_gem_mmap_offset(obj);
	}

out:
	drm_gem_object_unreference_unlocked(obj);

	return ret;
}

static int msm_ioctl_wait_fence(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct drm_msm_wait_fence *args = data;
	ktime_t timeout;


	if (args->pad) {
		DRM_ERROR("invalid pad: %08x\n", args->pad);
		return -EINVAL;
	}

	/*
	 * Special case - if the user passes a timeout of 0.0 just return the
	 * current fence status (0 for retired, -EBUSY for active) with no
	 * accompanying kernel logs. This can be a poor man's way of
	 * determining the status of a fence.
	 */
	if (args->timeout.tv_sec == 0 && args->timeout.tv_nsec == 0)
		return msm_wait_fence(dev, args->fence, NULL, true);

	timeout = to_ktime(args->timeout);
	return msm_wait_fence(dev, args->fence, &timeout, true);
}

static int msm_event_supported(struct drm_device *dev,
		struct drm_msm_event_req *req)
{
	int ret = -EINVAL;
	struct drm_mode_object *arg_obj;
	struct drm_crtc *crtc;

	arg_obj = drm_mode_object_find(dev, req->object_id, req->object_type);
	if (!arg_obj)
		return -ENOENT;

	if (arg_obj->type == DRM_MODE_OBJECT_CRTC) {
		crtc = obj_to_crtc(arg_obj);
		req->index = drm_crtc_index(crtc);
	}

	switch (req->event) {
	case DRM_EVENT_VBLANK:
	case DRM_EVENT_HISTOGRAM:
	case DRM_EVENT_AD:
		if (arg_obj->type == DRM_MODE_OBJECT_CRTC)
			ret = 0;
		break;
	default:
		break;
	}
	return ret;
}

static void msm_vblank_read_cb(struct drm_pending_event *e)
{
	struct drm_pending_vblank_event *vblank;
	struct msm_drm_private *priv;
	struct drm_file *file_priv;
	struct drm_device *dev;
	struct msm_drm_event *v;
	int ret = 0;
	bool need_vblank = false;

	if (!e) {
		DRM_ERROR("invalid pending event payload\n");
		return;
	}

	vblank = container_of(e, struct drm_pending_vblank_event, base);
	file_priv = vblank->base.file_priv;
	dev = (file_priv && file_priv->minor) ? file_priv->minor->dev : NULL;
	priv = (dev) ? dev->dev_private : NULL;
	if (!priv) {
		DRM_ERROR("invalid msm private\n");
		return;
	}

	list_for_each_entry(v, &priv->client_event_list, base.link) {
		if (v->base.file_priv != file_priv ||
		    (v->event.type != DRM_EVENT_VBLANK &&
		     v->event.type != DRM_EVENT_AD))
			continue;
		need_vblank = true;
		/**
		 * User-space client requests for N vsyncs when event
		 * requested is DRM_EVENT_AD. Once the count reaches zero,
		 * notify stop requesting for additional vsync's.
		 */
		if (v->event.type == DRM_EVENT_AD) {
			if (vblank->event.user_data)
				vblank->event.user_data--;
			need_vblank = (vblank->event.user_data) ? true : false;
		}
		break;
	}

	if (!need_vblank) {
		kfree(vblank);
	} else {
		ret = drm_vblank_get(dev, vblank->pipe);
		if (!ret) {
			list_add(&vblank->base.link, &dev->vblank_event_list);
		} else {
			DRM_ERROR("vblank enable failed ret %d\n", ret);
			kfree(vblank);
		}
	}
}

static int msm_enable_vblank_event(struct drm_device *dev,
			struct drm_msm_event_req *req, struct drm_file *file)
{
	struct drm_pending_vblank_event *e;
	int ret = 0;
	unsigned long flags;
	struct drm_vblank_crtc *vblank;

	if (WARN_ON(req->index >= dev->num_crtcs))
		return -EINVAL;

	vblank = &dev->vblank[req->index];
	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (!e)
		return -ENOMEM;

	e->pipe = req->index;
	e->base.pid = current->pid;
	e->event.base.type = DRM_EVENT_VBLANK;
	e->event.base.length = sizeof(e->event);
	e->event.user_data = req->client_context;
	e->base.event = &e->event.base;
	e->base.file_priv = file;
	e->base.destroy = msm_vblank_read_cb;

	ret = drm_vblank_get(dev, e->pipe);
	if (ret) {
		DRM_ERROR("failed to enable the vblank\n");
		goto free;
	}

	spin_lock_irqsave(&dev->event_lock, flags);
	if (!vblank->enabled) {
		ret = -EINVAL;
		goto err_unlock;
	}

	if (file->event_space < sizeof(e->event)) {
		ret = -EBUSY;
		goto err_unlock;
	}
	file->event_space -= sizeof(e->event);
	list_add_tail(&e->base.link, &dev->vblank_event_list);
err_unlock:
	spin_unlock_irqrestore(&dev->event_lock, flags);
free:
	if (ret)
		kfree(e);
	return ret;
}

static int msm_enable_event(struct drm_device *dev,
			struct drm_msm_event_req *req, struct drm_file *file)
{
	int ret = -EINVAL;

	switch (req->event) {
	case DRM_EVENT_AD:
	case DRM_EVENT_VBLANK:
		ret = msm_enable_vblank_event(dev, req, file);
		break;
	default:
		break;
	}
	return ret;
}

static int msm_disable_vblank_event(struct drm_device *dev,
			    struct drm_msm_event_req *req,
			    struct drm_file *file)
{
	struct drm_pending_vblank_event *e, *t;

	list_for_each_entry_safe(e, t, &dev->vblank_event_list, base.link) {
		if (e->pipe != req->index || file != e->base.file_priv)
			continue;
		list_del(&e->base.link);
		drm_vblank_put(dev, req->index);
		kfree(e);
	}
	return 0;
}

static int msm_disable_event(struct drm_device *dev,
			    struct drm_msm_event_req *req,
			    struct drm_file *file)
{
	int ret = -EINVAL;

	switch (req->event) {
	case DRM_EVENT_AD:
	case DRM_EVENT_VBLANK:
		ret = msm_disable_vblank_event(dev, req, file);
		break;
	default:
		break;
	}
	return ret;
}


static int msm_ioctl_register_event(struct drm_device *dev, void *data,
				    struct drm_file *file)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct drm_msm_event_req *req_event = data;
	struct msm_drm_event *client;
	struct msm_drm_event *v;
	unsigned long flag = 0;
	bool dup_request = false;
	int ret = 0;

	if (msm_event_supported(dev, req_event)) {
		DRM_ERROR("unsupported event %x object %x object id %d\n",
			req_event->event, req_event->object_type,
			req_event->object_id);
		return -EINVAL;
	}

	spin_lock_irqsave(&dev->event_lock, flag);
	list_for_each_entry(v, &priv->client_event_list, base.link) {
		if (v->base.file_priv != file)
			continue;
		if (v->event.type == req_event->event &&
			v->info.object_id == req_event->object_id) {
			DRM_ERROR("duplicate request for event %x obj id %d\n",
				v->event.type, v->info.object_id);
			dup_request = true;
			break;
		}
	}
	spin_unlock_irqrestore(&dev->event_lock, flag);

	if (dup_request)
		return -EINVAL;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	client->base.file_priv = file;
	client->base.pid = current->pid;
	client->base.event = &client->event;
	client->base.destroy = (void (*) (struct drm_pending_event *)) kfree;
	client->event.type = req_event->event;
	memcpy(&client->info, req_event, sizeof(client->info));

	spin_lock_irqsave(&dev->event_lock, flag);
	list_add_tail(&client->base.link, &priv->client_event_list);
	spin_unlock_irqrestore(&dev->event_lock, flag);

	ret = msm_enable_event(dev, req_event, file);
	if (ret) {
		DRM_ERROR("failed to enable event %x object %x object id %d\n",
			req_event->event, req_event->object_type,
			req_event->object_id);
		spin_lock_irqsave(&dev->event_lock, flag);
		list_del(&client->base.link);
		spin_unlock_irqrestore(&dev->event_lock, flag);
		kfree(client);
	}
	return ret;
}

static int msm_ioctl_deregister_event(struct drm_device *dev, void *data,
				      struct drm_file *file)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct drm_msm_event_req *req_event = data;
	struct msm_drm_event *client = NULL;
	struct msm_drm_event *v, *vt;
	unsigned long flag = 0;

	if (msm_event_supported(dev, req_event)) {
		DRM_ERROR("unsupported event %x object %x object id %d\n",
			req_event->event, req_event->object_type,
			req_event->object_id);
		return -EINVAL;
	}

	spin_lock_irqsave(&dev->event_lock, flag);
	msm_disable_event(dev, req_event, file);
	list_for_each_entry_safe(v, vt, &priv->client_event_list, base.link) {
		if (v->event.type == req_event->event &&
		    v->info.object_id == req_event->object_id &&
		    v->base.file_priv == file) {
			client = v;
			list_del(&client->base.link);
			client->base.destroy(&client->base);
			break;
		}
	}
	spin_unlock_irqrestore(&dev->event_lock, flag);

	return 0;
}

static int msm_ioctl_gem_sync(struct drm_device *dev, void *data,
			     struct drm_file *file)
{

	struct drm_msm_gem_sync *arg = data;
	int i;

	for (i = 0; i < arg->nr_ops; i++) {
		struct drm_msm_gem_syncop syncop;
		struct drm_gem_object *obj;
		int ret;
		void __user *ptr =
			(void __user *)(uintptr_t)
				(arg->ops + (i * sizeof(syncop)));

		ret = copy_from_user(&syncop, ptr, sizeof(syncop));
		if (ret)
			return -EFAULT;

		obj = drm_gem_object_lookup(dev, file, syncop.handle);
		if (!obj)
			return -ENOENT;

		msm_gem_sync(obj, syncop.op);

		drm_gem_object_unreference_unlocked(obj);
	}

	return 0;
}

void msm_send_crtc_notification(struct drm_crtc *crtc,
				struct drm_event *event, u8 *payload)
{
	struct drm_device *dev = NULL;
	struct msm_drm_private *priv = NULL;
	unsigned long flags;
	struct msm_drm_event *notify, *v;
	int len = 0;

	if (!crtc || !event || !event->length || !payload) {
		DRM_ERROR("err param crtc %pK event %pK len %d payload %pK\n",
			crtc, event, ((event) ? (event->length) : -1),
			payload);
		return;
	}
	dev = crtc->dev;
	priv = (dev) ? dev->dev_private : NULL;
	if (!dev || !priv) {
		DRM_ERROR("invalid dev %pK priv %pK\n", dev, priv);
		return;
	}

	spin_lock_irqsave(&dev->event_lock, flags);
	list_for_each_entry(v, &priv->client_event_list, base.link) {
		if (v->event.type != event->type ||
			crtc->base.id != v->info.object_id)
			continue;
		len = event->length + sizeof(struct drm_msm_event_resp);
		if (v->base.file_priv->event_space < len) {
			DRM_ERROR("Insufficient space to notify\n");
			continue;
		}
		notify = kzalloc(len, GFP_ATOMIC);
		if (!notify)
			continue;
		notify->base.file_priv = v->base.file_priv;
		notify->base.event = &notify->event;
		notify->base.pid = v->base.pid;
		notify->base.destroy =
			(void (*)(struct drm_pending_event *)) kfree;
		notify->event.type = v->event.type;
		notify->event.length = len;
		list_add(&notify->base.link,
			&notify->base.file_priv->event_list);
		notify->base.file_priv->event_space -= len;
		memcpy(&notify->info, &v->info, sizeof(notify->info));
		memcpy(notify->data, payload, event->length);
		wake_up_interruptible(&notify->base.file_priv->event_wait);
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);
}

static int msm_ioctl_counter_get(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct msm_file_private *ctx = file->driver_priv;
	struct msm_drm_private *priv = dev->dev_private;

	if (priv->gpu)
		return msm_gpu_counter_get(priv->gpu, data, ctx);

	return -ENODEV;
}

static int msm_ioctl_counter_put(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct msm_file_private *ctx = file->driver_priv;
	struct msm_drm_private *priv = dev->dev_private;

	if (priv->gpu)
		return msm_gpu_counter_put(priv->gpu, data, ctx);

	return -ENODEV;
}

static int msm_ioctl_counter_read(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct msm_drm_private *priv = dev->dev_private;

	if (priv->gpu)
		return msm_gpu_counter_read(priv->gpu, data);

	return -ENODEV;
}


static int msm_ioctl_submitqueue_new(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct drm_msm_submitqueue *args = data;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_gpu *gpu = priv->gpu;

	if (args->flags & ~MSM_SUBMITQUEUE_FLAGS)
		return -EINVAL;

	if ((gpu->nr_rings > 1) &&
		(!file->is_master && args->prio == 0)) {
		DRM_ERROR("Only DRM master can set highest priority ringbuffer\n");
		return -EPERM;
	}

	if (args->flags & MSM_SUBMITQUEUE_BYPASS_QOS_TIMEOUT &&
		!capable(CAP_SYS_ADMIN)) {
		DRM_ERROR(
			"Only CAP_SYS_ADMIN processes can bypass the timer\n");
		return -EPERM;
	}

	return msm_submitqueue_create(file->driver_priv, args->prio,
		args->flags, &args->id);
}

static int msm_ioctl_submitqueue_query(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct drm_msm_submitqueue_query *args = data;
	void __user *ptr = (void __user *)(uintptr_t) args->data;

	return msm_submitqueue_query(file->driver_priv, args->id,
		args->param, ptr, args->len);
}

static int msm_ioctl_submitqueue_close(struct drm_device *dev, void *data,
		struct drm_file *file)
{
	struct drm_msm_submitqueue *args = data;

	return msm_submitqueue_remove(file->driver_priv, args->id);
}

int msm_release(struct inode *inode, struct file *filp)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_minor *minor = file_priv->minor;
	struct drm_device *dev = minor->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_drm_event *v, *vt;
	unsigned long flags;

	spin_lock_irqsave(&dev->event_lock, flags);
	list_for_each_entry_safe(v, vt, &priv->client_event_list, base.link) {
		if (v->base.file_priv != file_priv)
			continue;
		list_del(&v->base.link);
		msm_disable_event(dev, &v->info, file_priv);
		v->base.destroy(&v->base);
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);

	return drm_release(inode, filp);
}

/**
 * msm_ioctl_rmfb2 - remove an FB from the configuration
 * @dev: drm device for the ioctl
 * @data: data pointer for the ioctl
 * @file_priv: drm file for the ioctl call
 *
 * Remove the FB specified by the user.
 *
 * Called by the user via ioctl.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
static int msm_ioctl_rmfb2(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_framebuffer *fb = NULL;
	struct drm_framebuffer *fbl = NULL;
	uint32_t *id = data;
	int found = 0;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EINVAL;

	fb = drm_framebuffer_lookup(dev, *id);
	if (!fb)
		return -ENOENT;

	/* drop extra ref from traversing drm_framebuffer_lookup */
	drm_framebuffer_unreference(fb);

	mutex_lock(&file_priv->fbs_lock);
	list_for_each_entry(fbl, &file_priv->fbs, filp_head)
		if (fb == fbl)
			found = 1;
	if (!found) {
		mutex_unlock(&file_priv->fbs_lock);
		return -ENOENT;
	}

	list_del_init(&fb->filp_head);
	mutex_unlock(&file_priv->fbs_lock);

	drm_framebuffer_unreference(fb);

	return 0;
}

static const struct drm_ioctl_desc msm_ioctls[] = {
	DRM_IOCTL_DEF_DRV(MSM_GET_PARAM,    msm_ioctl_get_param,    DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_GEM_NEW,      msm_ioctl_gem_new,      DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_GEM_INFO,     msm_ioctl_gem_info,     DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_GEM_CPU_PREP, msm_ioctl_gem_cpu_prep, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_GEM_CPU_FINI, msm_ioctl_gem_cpu_fini, DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_GEM_SUBMIT,   msm_ioctl_gem_submit,   DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_WAIT_FENCE,   msm_ioctl_wait_fence,   DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(SDE_WB_CONFIG, sde_wb_config, DRM_UNLOCKED|DRM_AUTH),
	DRM_IOCTL_DEF_DRV(MSM_REGISTER_EVENT,  msm_ioctl_register_event,
			  DRM_UNLOCKED|DRM_CONTROL_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_DEREGISTER_EVENT,  msm_ioctl_deregister_event,
			  DRM_UNLOCKED|DRM_CONTROL_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_COUNTER_GET, msm_ioctl_counter_get,
			  DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_COUNTER_PUT, msm_ioctl_counter_put,
			  DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_COUNTER_READ, msm_ioctl_counter_read,
			  DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_GEM_SYNC, msm_ioctl_gem_sync,
			  DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_GEM_SVM_NEW, msm_ioctl_gem_svm_new,
			  DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_SUBMITQUEUE_NEW,  msm_ioctl_submitqueue_new,
			  DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_SUBMITQUEUE_CLOSE, msm_ioctl_submitqueue_close,
			  DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_SUBMITQUEUE_QUERY, msm_ioctl_submitqueue_query,
			  DRM_AUTH|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_RMFB2, msm_ioctl_rmfb2,
			  DRM_CONTROL_ALLOW|DRM_UNLOCKED),
};

static const struct vm_operations_struct vm_ops = {
	.fault = msm_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static const struct file_operations fops = {
	.owner              = THIS_MODULE,
	.open               = drm_open,
	.release            = msm_release,
	.unlocked_ioctl     = drm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl       = drm_compat_ioctl,
#endif
	.poll               = drm_poll,
	.read               = drm_read,
	.llseek             = no_llseek,
	.mmap               = msm_gem_mmap,
};

static struct drm_driver msm_driver = {
	.driver_features    = DRIVER_HAVE_IRQ |
				DRIVER_GEM |
				DRIVER_PRIME |
				DRIVER_RENDER |
				DRIVER_ATOMIC |
				DRIVER_MODESET,
	.load               = msm_load,
	.unload             = msm_unload,
	.open               = msm_open,
	.preclose           = msm_preclose,
	.postclose          = msm_postclose,
	.lastclose          = msm_lastclose,
	.set_busid          = drm_platform_set_busid,
	.irq_handler        = msm_irq,
	.irq_preinstall     = msm_irq_preinstall,
	.irq_postinstall    = msm_irq_postinstall,
	.irq_uninstall      = msm_irq_uninstall,
	.get_vblank_counter = drm_vblank_no_hw_counter,
	.enable_vblank      = msm_enable_vblank,
	.disable_vblank     = msm_disable_vblank,
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
#ifdef CONFIG_DEBUG_FS
	.debugfs_init       = msm_debugfs_init,
	.debugfs_cleanup    = msm_debugfs_cleanup,
#endif
	.ioctls             = msm_ioctls,
	.num_ioctls         = ARRAY_SIZE(msm_ioctls),
	.fops               = &fops,
	.name               = "msm_drm",
	.desc               = "MSM Snapdragon DRM",
	.date               = "20130625",
	.major              = 1,
	.minor              = 0,
};

#ifdef CONFIG_PM_SLEEP
static int msm_pm_suspend(struct device *dev)
{
	struct drm_device *ddev;
	struct drm_modeset_acquire_ctx *ctx;
	struct drm_connector *conn;
	struct drm_atomic_state *state;
	struct drm_crtc_state *crtc_state;
	struct msm_drm_private *priv;
	int ret = 0;

	if (!dev)
		return -EINVAL;

	ddev = dev_get_drvdata(dev);
	if (!ddev || !ddev->dev_private)
		return -EINVAL;

	priv = ddev->dev_private;
	SDE_EVT32(0);

	/* acquire modeset lock(s) */
	drm_modeset_lock_all(ddev);
	ctx = ddev->mode_config.acquire_ctx;

	/* save current state for resume */
	if (priv->suspend_state)
		drm_atomic_state_free(priv->suspend_state);
	priv->suspend_state = drm_atomic_helper_duplicate_state(ddev, ctx);
	if (IS_ERR_OR_NULL(priv->suspend_state)) {
		DRM_ERROR("failed to back up suspend state\n");
		priv->suspend_state = NULL;
		goto unlock;
	}

	/* create atomic state to disable all CRTCs */
	state = drm_atomic_state_alloc(ddev);
	if (IS_ERR_OR_NULL(state)) {
		DRM_ERROR("failed to allocate crtc disable state\n");
		goto unlock;
	}

	state->acquire_ctx = ctx;
	drm_for_each_connector(conn, ddev) {

		if (!conn->state || !conn->state->crtc ||
				conn->dpms != DRM_MODE_DPMS_ON)
			continue;

		/* force CRTC to be inactive */
		crtc_state = drm_atomic_get_crtc_state(state,
				conn->state->crtc);
		if (IS_ERR_OR_NULL(crtc_state)) {
			DRM_ERROR("failed to get crtc %d state\n",
					conn->state->crtc->base.id);
			drm_atomic_state_free(state);
			goto unlock;
		}
		crtc_state->active = false;
	}

	/* commit the "disable all" state */
	ret = drm_atomic_commit(state);
	if (ret < 0) {
		DRM_ERROR("failed to disable crtcs, %d\n", ret);
		drm_atomic_state_free(state);
	}

unlock:
	drm_modeset_unlock_all(ddev);

	/* disable hot-plug polling */
	drm_kms_helper_poll_disable(ddev);

	return 0;
}

static int msm_pm_resume(struct device *dev)
{
	struct drm_device *ddev;
	struct msm_drm_private *priv;
	int ret;

	if (!dev)
		return -EINVAL;

	ddev = dev_get_drvdata(dev);
	if (!ddev || !ddev->dev_private)
		return -EINVAL;

	priv = ddev->dev_private;

	SDE_EVT32(priv->suspend_state != NULL);

	drm_mode_config_reset(ddev);

	drm_modeset_lock_all(ddev);

	if (priv->suspend_state) {
		priv->suspend_state->acquire_ctx =
			ddev->mode_config.acquire_ctx;
		ret = drm_atomic_commit(priv->suspend_state);
		if (ret < 0) {
			DRM_ERROR("failed to restore state, %d\n", ret);
			drm_atomic_state_free(priv->suspend_state);
		}
		priv->suspend_state = NULL;
	}
	drm_modeset_unlock_all(ddev);

	/* enable hot-plug polling */
	drm_kms_helper_poll_enable(ddev);

	return 0;
}

static int msm_pm_freeze(struct device *dev)
{
	struct drm_device *ddev;
	struct drm_crtc *crtc;
	struct drm_modeset_acquire_ctx *ctx;
	struct drm_atomic_state *state;
	struct msm_drm_private *priv;
	struct msm_kms *kms;
	int early_display = 0;
	int ret = 0;

	if (!dev)
		return -EINVAL;

	ddev = dev_get_drvdata(dev);
	if (!ddev || !ddev->dev_private)
		return -EINVAL;

	priv = ddev->dev_private;

	kms = priv->kms;
	if (kms && kms->funcs && kms->funcs->early_display_status)
		early_display = kms->funcs->early_display_status(kms);

	SDE_EVT32(0);

	if (early_display) {
		/* acquire modeset lock(s) */
		drm_modeset_lock_all(ddev);
		ctx = ddev->mode_config.acquire_ctx;

		/* save current state for restore */
		if (priv->suspend_state)
			drm_atomic_state_free(priv->suspend_state);

		priv->suspend_state =
			drm_atomic_helper_duplicate_state(ddev, ctx);

		if (IS_ERR_OR_NULL(priv->suspend_state)) {
			DRM_ERROR("failed to back up suspend state\n");
			priv->suspend_state = NULL;
			goto unlock;
		}

		/* create atomic null state to idle CRTCs */
		state = drm_atomic_state_alloc(ddev);
		if (IS_ERR_OR_NULL(state)) {
			DRM_ERROR("failed to allocate null atomic state\n");
			goto unlock;
		}

		state->acquire_ctx = ctx;

		/* commit the null state */
		ret = drm_atomic_commit(state);
		if (ret < 0) {
			DRM_ERROR("failed to commit null state, %d\n", ret);
			drm_atomic_state_free(state);
		}

		drm_for_each_crtc(crtc, ddev)
			drm_crtc_vblank_off(crtc);

unlock:
		drm_modeset_unlock_all(ddev);
	} else {
		ret = msm_pm_suspend(dev);
		if (ret)
			return ret;
	}
	return 0;
}

static int msm_pm_restore(struct device *dev)
{
	struct drm_device *ddev;
	struct drm_crtc *crtc;
	struct msm_drm_private *priv;
	struct msm_kms *kms;
	int early_display = 0;
	int ret;

	if (!dev)
		return -EINVAL;

	ddev = dev_get_drvdata(dev);
	if (!ddev || !ddev->dev_private)
		return -EINVAL;

	priv = ddev->dev_private;

	kms = priv->kms;
	if (kms && kms->funcs && kms->funcs->early_display_status)
		early_display = kms->funcs->early_display_status(kms);


	SDE_EVT32(priv->suspend_state != NULL);

	if (early_display) {
		drm_mode_config_reset(ddev);

		drm_modeset_lock_all(ddev);

		drm_for_each_crtc(crtc, ddev)
			drm_crtc_vblank_on(crtc);

		if (priv->suspend_state) {
			priv->suspend_state->acquire_ctx =
				ddev->mode_config.acquire_ctx;

			ret = drm_atomic_commit(priv->suspend_state);
			if (ret < 0) {
				DRM_ERROR("failed to restore state, %d\n", ret);
				drm_atomic_state_free(priv->suspend_state);
			}

			priv->suspend_state = NULL;
		}

		drm_modeset_unlock_all(ddev);
	} else {
		ret = msm_pm_resume(dev);
		if (ret)
			return ret;
	}

	return 0;
}

static int msm_pm_thaw(struct device *dev)
{
	msm_pm_restore(dev);

	return 0;
}
#endif

static const struct dev_pm_ops msm_pm_ops = {
	.suspend = msm_pm_suspend,
	.resume = msm_pm_resume,
	.freeze = msm_pm_freeze,
	.restore = msm_pm_restore,
	.thaw = msm_pm_thaw,
};

static int msm_drm_bind(struct device *dev)
{
	int ret;

	ret = drm_platform_init(&msm_driver, to_platform_device(dev));
	if (ret)
		DRM_ERROR("drm_platform_init failed: %d\n", ret);

	return ret;
}

static void msm_drm_unbind(struct device *dev)
{
	drm_put_dev(platform_get_drvdata(to_platform_device(dev)));
}

static const struct component_master_ops msm_drm_ops = {
	.bind = msm_drm_bind,
	.unbind = msm_drm_unbind,
};

/*
 * Componentized driver support:
 */

#ifdef CONFIG_OF
/* NOTE: the CONFIG_OF case duplicates the same code as exynos or imx
 * (or probably any other).. so probably some room for some helpers
 */
static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int add_components(struct device *dev, struct component_match **matchptr,
		const char *name)
{
	struct device_node *np = dev->of_node;
	unsigned i;

	for (i = 0; ; i++) {
		struct device_node *node;

		node = of_parse_phandle(np, name, i);
		if (!node)
			break;

		component_match_add(dev, matchptr, compare_of, node);
	}

	return 0;
}

static int msm_add_master_component(struct device *dev,
					struct component_match *match)
{
	int ret;

	ret = component_master_add_with_match(dev, &msm_drm_ops, match);
	if (ret)
		DRM_ERROR("component add match failed: %d\n", ret);

	return ret;
}

#else
static int compare_dev(struct device *dev, void *data)
{
	return dev == data;
}

static int msm_add_master_component(struct device *dev,
					struct component_match *match)
{
	return 0;
}
#endif

/*
 * Platform driver:
 */

static int msm_pdev_probe(struct platform_device *pdev)
{
	int ret;
	struct component_match *match = NULL;

	msm_drm_probed = true;

#ifdef CONFIG_OF
	add_components(&pdev->dev, &match, "connectors");
#ifndef CONFIG_QCOM_KGSL
	add_components(&pdev->dev, &match, "gpus");
#endif
#else
	/* For non-DT case, it kinda sucks.  We don't actually have a way
	 * to know whether or not we are waiting for certain devices (or if
	 * they are simply not present).  But for non-DT we only need to
	 * care about apq8064/apq8060/etc (all mdp4/a3xx):
	 */
	static const char *devnames[] = {
			"hdmi_msm.0", "kgsl-3d0.0",
	};
	int i;

	DBG("Adding components..");

	for (i = 0; i < ARRAY_SIZE(devnames); i++) {
		struct device *dev;

		dev = bus_find_device_by_name(&platform_bus_type,
				NULL, devnames[i]);
		if (!dev) {
			dev_info(&pdev->dev, "still waiting for %s\n", devnames[i]);
			return -EPROBE_DEFER;
		}

		component_match_add(&pdev->dev, &match, compare_dev, dev);
	}
#endif
	/* on all devices that I am aware of, iommu's which cna map
	 * any address the cpu can see are used:
	 */
	ret = dma_set_mask_and_coherent(&pdev->dev, ~0);
	if (ret)
		return ret;

	ret = msm_add_master_component(&pdev->dev, match);
	complete(&wait_display_completion);

	return ret;
}

static int msm_pdev_remove(struct platform_device *pdev)
{
	msm_drm_unbind(&pdev->dev);
	component_master_del(&pdev->dev, &msm_drm_ops);
	return 0;
}

static const struct platform_device_id msm_id[] = {
	{ "mdp", 0 },
	{ }
};

static void msm_pdev_shutdown(struct platform_device *pdev)
{
	struct drm_device *ddev = platform_get_drvdata(pdev);
	struct msm_drm_private *priv = NULL;

	if (!ddev) {
		DRM_ERROR("invalid drm device node\n");
		return;
	}

	priv = ddev->dev_private;
	if (!priv) {
		DRM_ERROR("invalid msm drm private node\n");
		return;
	}

	msm_lastclose(ddev);

	/* set this after lastclose to allow kickoff from lastclose */
	priv->shutdown_in_progress = true;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,mdp" },      /* mdp4 */
	{ .compatible = "qcom,sde-kms" },  /* sde  */
	{}
};
MODULE_DEVICE_TABLE(of, dt_match);

static int find_match(struct device *dev, void *data)
{
	struct device_driver *drv = data;

	return drv->bus->match(dev, drv);
}

static bool find_device(struct platform_driver *pdrv)
{
	struct device_driver *drv = &pdrv->driver;

	return bus_for_each_dev(drv->bus, NULL, drv, find_match);
}

static struct platform_driver msm_platform_driver = {
	.probe      = msm_pdev_probe,
	.remove     = msm_pdev_remove,
	.shutdown   = msm_pdev_shutdown,
	.driver     = {
		.name   = "msm_drm",
		.of_match_table = dt_match,
		.pm     = &msm_pm_ops,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.id_table   = msm_id,
};

#ifdef CONFIG_QCOM_KGSL
void __init adreno_register(void)
{
}

void __exit adreno_unregister(void)
{
}
#endif

static int __init msm_drm_register(void)
{
	DBG("init");
	msm_smmu_driver_init();
	msm_dsi_register();
	msm_edp_register();
	hdmi_register();
	adreno_register();
	init_completion(&wait_display_completion);
	return platform_driver_register(&msm_platform_driver);
}

static void __exit msm_drm_unregister(void)
{
	DBG("fini");
	platform_driver_unregister(&msm_platform_driver);
	hdmi_unregister();
	adreno_unregister();
	msm_edp_unregister();
	msm_dsi_unregister();
	msm_smmu_driver_cleanup();
}

static int __init msm_drm_late_register(void)
{
	struct platform_driver *pdrv;

	pdrv = &msm_platform_driver;
	if (msm_drm_probed || find_device(pdrv)) {
		pr_debug("wait for display probe completion\n");
		wait_for_completion(&wait_display_completion);
	}
	return 0;
}

module_init(msm_drm_register);
module_exit(msm_drm_unregister);
/* init level 7 */
late_initcall(msm_drm_late_register);

MODULE_AUTHOR("Rob Clark <robdclark@gmail.com");
MODULE_DESCRIPTION("MSM DRM Driver");
MODULE_LICENSE("GPL");
