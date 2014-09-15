/*
 * Copyright (C) 2014, Intel Corporation.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <drm/i915_drm.h>
#include <drm/i915_dmabuf.h>

#include "intel_adf.h"

#define INTEL_ADF_DEVICE_NAME		"intel-adf-dev"
#define INTEL_DC_REG_OFFSET		0x0

#ifdef CONFIG_ADF_INTEL_VLV
	#define INTEL_DC_REG_SIZE	(2 * 1024 * 1024)
#else
	#define INTEL_DC_REG_SIZE	0x80000
#endif
#define INTEL_VSYNC_FENCE_TIMEOUT	(5 * MSEC_PER_SEC)


static struct intel_adf_device *g_intel_adf_dev;

u32 REG_READ(u32 reg)
{
	return ioread32(g_intel_adf_dev->mmio + (reg));
}

void REG_WRITE(u32 reg, u32 val)
{
	iowrite32(val, g_intel_adf_dev->mmio + reg);
}

u32 REG_POSTING_READ(u32 reg)
{
	return ioread32(g_intel_adf_dev->mmio + (reg));
}

struct post_obj {
	void *obj;
	struct list_head next;
};

struct post_obj_set {
	struct list_head objs;
};

struct flip {
	struct intel_adf_overlay_engine *eng;
	struct intel_buffer buf;
	struct intel_plane_config config;
	struct list_head list;
};

struct driver_state {
	struct post_obj_set post_intfs;
	struct post_obj_set post_engs;
	struct list_head post_flips;
	u64 timestamp;
};

/**
 * for_each_post_obj - iterate over post object set
 * @po: post_obj * to use as a loop cursor.
 * @set: post_obj_set * for a post object set
 */
#define for_each_post_obj(po, set) \
	list_for_each_entry(po, &((set)->objs), next)

static struct post_obj *post_obj_set_find_obj(struct post_obj_set *set,
	void *obj)
{
	bool existed = false;
	struct post_obj *po;

	for_each_post_obj(po, set) {
		if (po->obj == obj) {
			existed = true;
			break;
		}
	}

	return existed ? po : NULL;
}

static struct post_obj *create_add_post_obj(struct post_obj_set *set,
	void *obj)
{
	struct post_obj *po;

	if (!set || !obj)
		return NULL;

	po = post_obj_set_find_obj(set, obj);
	if (!po) {
		po = kzalloc(sizeof(*po), GFP_KERNEL);
		if (po) {
			po->obj = obj;
			INIT_LIST_HEAD(&po->next);
			list_add_tail(&po->next, &set->objs);
		}
	}

	return po;
}

static void post_obj_set_init(struct post_obj_set *set)
{
	INIT_LIST_HEAD(&set->objs);
}

static void post_obj_set_destroy(struct post_obj_set *set)
{
	struct post_obj *po, *tmp;

	if (!set)
		return;

	list_for_each_entry_safe(po, tmp, &set->objs, next) {
		list_del(&po->next);
		kfree(po);
	}
}

/*----------------------------------------------------------------------------*/
/*
static int intel_adf_device_attach(struct adf_device *dev,
				struct adf_overlay_engine *eng,
				struct adf_interface *intf)
{
	return 0;
}

static int intel_adf_device_detach(struct adf_device *dev,
				struct adf_overlay_engine *eng,
				struct adf_interface *intf)
{
	return 0;
}

static int intel_adf_device_validate_custom_format(struct adf_device *dev,
						struct adf_buffer *buf)
{
	return 0;
}
*/

static int intel_adf_device_validate_custom_format(struct adf_device *dev,
	struct adf_buffer *buf)
{
	struct intel_adf_overlay_engine *eng =
		to_intel_eng(buf->overlay_engine);
	struct intel_plane *plane = eng->plane;

	if (plane && plane->ops && plane->ops->validate_custom_format)
		return plane->ops->validate_custom_format(plane, buf->format,
			buf->w, buf->h);
	return 0;
}

static int adf_buffer_to_intel_buffer(struct adf_buffer *adf_buf,
	struct adf_buffer_mapping *mapping, struct intel_buffer *intel_buf)
{
	u32 gtt_in_pages = 0;
#ifdef CONFIG_ADF_INTEL_VLV
	struct dma_buf_attachment *buf_attach = mapping->attachments[0];
	struct i915_drm_dmabuf_attachment *i915_buf_attach =
		(struct i915_drm_dmabuf_attachment *)buf_attach->priv;

	gtt_in_pages = i915_buf_attach->gtt_offset;
#else
	struct dma_buf *dma_buf = adf_buf->dma_bufs[0];
	int err;
	err = intel_adf_mm_gtt(dma_buf, &gtt_in_pages);
	if (err)
		return err;
#endif

	intel_buf->format = adf_buf->format;
	intel_buf->w = adf_buf->w;
	intel_buf->h = adf_buf->h;
	intel_buf->gtt_offset_in_pages = gtt_in_pages;
	intel_buf->stride = adf_buf->pitch[0];

	return 0;
}

static void adf_plane_to_intel_plane_config(
	struct intel_adf_plane *adf_plane, struct intel_adf_interface *intf,
	struct intel_plane_config *config)
{
	config->dst_x = adf_plane->dst_x;
	config->dst_y = adf_plane->dst_y;
	config->dst_w = adf_plane->dst_w;
	config->dst_h = adf_plane->dst_h;
	config->src_x = adf_plane->src_x;
	config->src_y = adf_plane->src_y;
	config->src_w = adf_plane->src_w;
	config->src_h = adf_plane->src_h;
	config->alpha = adf_plane->alpha;
	/*TODO: map adf_plane to plane_config*/
	config->compression = adf_plane->compression;
	config->blending = adf_plane->blending;
	config->transform = adf_plane->transform;
	config->pipe = intf->pipe;
}

static struct driver_state *driver_state_create_and_init(void)
{
	struct driver_state *state = NULL;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (state) {
		post_obj_set_init(&state->post_intfs);
		post_obj_set_init(&state->post_engs);
		INIT_LIST_HEAD(&state->post_flips);
	}

	return state;
}

static void driver_state_destroy(struct driver_state *state)
{
	struct flip *f, *tmp;

	if (!state)
		return;

	post_obj_set_destroy(&state->post_engs);
	post_obj_set_destroy(&state->post_intfs);
	list_for_each_entry_safe(f, tmp, &state->post_flips, list) {
		list_del(&f->list);
		kfree(f);
	}

	kfree(state);
}

static void driver_state_add_interface(struct driver_state *state,
	struct intel_adf_interface *intf)
{
	create_add_post_obj(&state->post_intfs, intf);
}

static void driver_state_add_overlay_engine(struct driver_state *state,
	struct intel_adf_overlay_engine *eng)
{
	create_add_post_obj(&state->post_engs, eng);
}

static struct flip *driver_state_create_add_flip(
	struct driver_state *state, struct intel_adf_overlay_engine *eng,
	struct intel_adf_interface *intf, struct adf_buffer *buf,
	struct adf_buffer_mapping *mapping, struct intel_adf_plane *plane)
{
	struct flip *f = NULL;
	int err;

	f = kzalloc(sizeof(*f), GFP_KERNEL);
	if (f) {
		f->eng = eng;
		adf_buffer_to_intel_buffer(buf, mapping, &f->buf);
		adf_plane_to_intel_plane_config(plane, intf, &f->config);

		/*validate the buffer and config before adding it*/
		if (eng->plane && eng->plane->ops &&
			eng->plane->ops->validate) {
			err = eng->plane->ops->validate(eng->plane, &f->buf,
				&f->config);
			if (err)
				goto err;
		}
		INIT_LIST_HEAD(&f->list);
		list_add_tail(&f->list, &state->post_flips);
	}

	return f;
err:
	kfree(f);
	return NULL;
}

static int intel_adf_device_validate(struct adf_device *dev,
				struct adf_post *cfg,
				void **driver_state)
{
	struct intel_adf_post_custom_data *custom = cfg->custom_data;
	struct intel_adf_overlay *custom_overlay;
	struct intel_adf_overlay_engine *eng;
	struct driver_state *state;
	struct adf_interface *intf;
	struct adf_buffer *buf;
	struct adf_buffer_mapping *mapping;
	struct flip *f;

	size_t n_bufs = cfg->n_bufs;
	u32 n_overlays;
	size_t custom_size;
	int err;
	int i;

	if (!custom) {
		dev_err(dev->dev, "%s: no custom data found\n", __func__);
		return -EINVAL;
	}

	/*verify version*/
	if (custom->version != INTEL_ADF_VERSION) {
		dev_err(dev->dev, "%s: version mismatch\n", __func__);
		return -EINVAL;
	}

	n_overlays = custom->num_overlays;

	/*verify custom size*/
	custom_size = sizeof(struct intel_adf_post_custom_data) +
		n_overlays * sizeof(struct intel_adf_overlay);
	if (custom_size != cfg->custom_data_size) {
		dev_err(dev->dev, "%s: invalid custom size\n", __func__);
		return -EINVAL;
	}

	/*allocate driver state*/
	state = driver_state_create_and_init();
	if (!state) {
		dev_err(dev->dev, "%s: failed to allocate driver state\n",
			__func__);
		return -ENOMEM;
	}

	/*verify custom overlays*/
	for (i = 0; i < n_overlays; i++) {
		custom_overlay = &custom->overlays[i];
		/*verify buffer id set in plane*/
		if (custom_overlay->plane.buffer_id > n_bufs) {
			dev_err(dev->dev, "%s: invalid custom buffer id %d\n",
				__func__, custom_overlay->plane.buffer_id);
			err = -EINVAL;
			goto err;
		}
		/*verify interface id set in plane*/
		intf = idr_find(&dev->interfaces,
			custom_overlay->plane.inteface_id);
		if (!intf) {
			dev_err(dev->dev, "%s: invalid interface id %d\n",
				__func__, custom_overlay->plane.inteface_id);
			err = -EINVAL;
			goto err;
		}
		driver_state_add_interface(state, to_intel_intf(intf));

		/*get adf_buffer for this overlay*/
		buf = &cfg->bufs[custom_overlay->plane.buffer_id];
		mapping = &cfg->mappings[custom_overlay->plane.buffer_id];
		eng = to_intel_eng(buf->overlay_engine);
		driver_state_add_overlay_engine(state, eng);

		/*create and queue a flip for this overlay*/
		f = driver_state_create_add_flip(state, eng,
			to_intel_intf(intf), buf, mapping,
				&custom_overlay->plane);
		if (!f) {
			dev_err(dev->dev, "%s: failed to create flip\n",
				__func__);
			err = -ENOMEM;
			goto err;
		}
	}

	*driver_state = state;

	return 0;
err:
	driver_state_destroy(state);
	return err;
}

static struct sync_fence *intel_adf_device_complete_fence(
	struct adf_device *dev, struct adf_post *cfg, void *driver_state)
{
	struct intel_adf_device *i_dev = to_intel_dev(dev);
	struct intel_adf_sync_timeline *tl = i_dev->post_timeline;
	struct driver_state *state = driver_state;
	struct sync_fence *post_fence;
	u64 timestamp;

	timestamp = ktime_to_ns(ktime_get());
	state->timestamp = timestamp;

	post_fence = intel_adf_sync_fence_create(tl, timestamp);
	if (!post_fence)
		return ERR_PTR(-ENOMEM);

	return post_fence;
}

static void disable_unused_overlay_engines(struct list_head *active_engs,
	struct post_obj_set *post_engs)
{
	struct intel_adf_overlay_engine *eng;

	list_for_each_entry(eng, active_engs, active_list) {
		if (post_obj_set_find_obj(post_engs, eng))
			continue;
		/*disable this engine*/
		eng->plane->ops->disable(eng->plane);
	}
}

static void update_active_overlay_engines(struct list_head *active_engs,
	struct post_obj_set *post_engs)
{
	struct intel_adf_overlay_engine *eng, *tmp;
	struct post_obj *po;

	list_for_each_entry_safe(eng, tmp, active_engs, active_list) {
		list_del_init(&eng->active_list);
	}

	for_each_post_obj(po, post_engs) {
		eng = po->obj;
		INIT_LIST_HEAD(&eng->active_list);
		list_add_tail(&eng->active_list, active_engs);
	}
}

static void update_active_interfaces(struct list_head *active_intfs,
	struct post_obj_set *post_intfs)
{
	struct intel_adf_interface *intf, *tmp;
	struct post_obj *po;

	list_for_each_entry_safe(intf, tmp, active_intfs, active_list) {
		list_del_init(&intf->active_list);
	}

	for_each_post_obj(po, post_intfs) {
		intf = po->obj;
		INIT_LIST_HEAD(&intf->active_list);
		list_add_tail(&intf->active_list, active_intfs);
	}
}

static void intel_adf_device_post(struct adf_device *dev,
				struct adf_post *cfg,
				void *driver_state)
{
	struct intel_adf_device *i_dev = to_intel_dev(dev);
	struct driver_state *state = driver_state;
	struct intel_adf_overlay_engine *eng;
	struct intel_adf_interface *intf;
	struct post_obj *po;
	struct flip *f;

	/*disable unused overlay engines*/
	disable_unused_overlay_engines(&i_dev->active_engs,
		&state->post_engs);

	/* To forbid DSR */
	for_each_post_obj(po, &state->post_intfs) {
		intf = po->obj;
		if (intf->pipe && intf->pipe->ops && intf->pipe->ops->pre_post)
			intf->pipe->ops->pre_post(intf->pipe);
	}

	/*flip planes*/
	list_for_each_entry(f, &state->post_flips, list) {
		eng = f->eng;
		if (!eng->plane || !eng->plane->ops || !eng->plane->ops->flip) {
			dev_err(dev->dev, "%s: invalid plane\n", __func__);
			return;
		}
		eng->plane->ops->flip(eng->plane, &f->buf, &f->config);
	}

	/*trigger pipe processing, if necessary*/
	for_each_post_obj(po, &state->post_intfs) {
		intf = po->obj;
		if (intf->pipe && intf->pipe->ops && intf->pipe->ops->on_post)
			intf->pipe->ops->on_post(intf->pipe);
	}

	update_active_overlay_engines(&i_dev->active_engs,
		&state->post_engs);
	update_active_interfaces(&i_dev->active_intfs, &state->post_intfs);
}

static struct sync_fence *create_vsync_fence(struct post_obj_set *intfs_set)
{
	struct sync_fence *vsync_fence, *fence, *merged_fence;
	struct intel_adf_interface *intf;
	struct post_obj *po;

	vsync_fence = NULL;
	for_each_post_obj(po, intfs_set) {
		intf = po->obj;
		fence = intel_adf_interface_create_vsync_fence(intf, 1);
		if (!fence)
			continue;

		if (!vsync_fence)
			vsync_fence = fence;
		else {
			merged_fence = sync_fence_merge("intel-adf",
				vsync_fence, fence);
			if (!merged_fence)
				continue;
			sync_fence_put(vsync_fence);
			sync_fence_put(fence);

			vsync_fence = merged_fence;
		}
	}

	return vsync_fence;
}

static void intel_adf_device_advance_timeline(struct adf_device *dev,
	struct adf_post *cfg, void *driver_state)
{
	struct driver_state *state = driver_state;
	struct sync_fence *vsync_fence;
	int err;

	vsync_fence = create_vsync_fence(&state->post_intfs);
	if (!vsync_fence) {
		dev_err(dev->dev, "%s: failed to create vsync fence\n",
			__func__);
		return;
	}

	/*wait for vsync fence*/
	err = sync_fence_wait(vsync_fence, INTEL_VSYNC_FENCE_TIMEOUT);
	if (err == -ETIME) {
		dev_err(dev->dev, "%s: vsync fence wait timeout\n", __func__);
		goto out_err0;
	} else if (err < 0) {
		dev_err(dev->dev, "%s: vsync fence wait err\n", __func__);
		goto out_err0;
	}

out_err0:
	sync_fence_put(vsync_fence);
	return;
}

static void intel_adf_device_state_free(struct adf_device *dev,
				void *driver_state)
{
	struct intel_adf_device *i_dev = to_intel_dev(dev);
	struct driver_state *state = driver_state;

	/*signal post timeline*/
	intel_adf_sync_timeline_signal(i_dev->post_timeline,
		state->timestamp);

	driver_state_destroy(driver_state);
}

static const struct adf_device_ops intel_adf_device_ops = {
	.owner = THIS_MODULE,
	.validate_custom_format = intel_adf_device_validate_custom_format,
	.validate = intel_adf_device_validate,
	.post = intel_adf_device_post,
	.complete_fence = intel_adf_device_complete_fence,
	.advance_timeline = intel_adf_device_advance_timeline,
	.state_free = intel_adf_device_state_free,
};
/*----------------------------------------------------------------------------*/
struct intel_adf_device *intel_adf_device_create(struct pci_dev *pdev,
	struct intel_dc_memory *mem)
{
	int err = 0;
	struct intel_adf_device *dev;
	unsigned long reg_phy;
	u8 *mmio;

	dev_info(&pdev->dev, "%s\n", __func__);

	if (!pdev) {
		dev_err(&pdev->dev, "%s: invalid pci device\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	/*allocate adf device*/
	dev = kzalloc(sizeof(struct intel_adf_device), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "%s: failed to allocate adf device\n",
			__func__);
		goto err_out0;
	}

	INIT_LIST_HEAD(&dev->active_intfs);
	INIT_LIST_HEAD(&dev->active_engs);
	dev->pdev = pdev;

	/*mmio init*/
	reg_phy = pci_resource_start(dev->pdev, 0) + INTEL_DC_REG_OFFSET;
	mmio = ioremap(reg_phy, INTEL_DC_REG_SIZE);
	if (!mmio) {
		dev_err(&pdev->dev, "%s: failed to map display mmio\n",
			__func__);
		err = -ENOMEM;
		goto err_out1;
	}
	dev->mmio = mmio;

	/*create post timeline*/
	dev->post_timeline = intel_adf_sync_timeline_create("post");
	if (!dev->post_timeline) {
		dev_err(&pdev->dev, "%s: failed to create post timeline\n",
			__func__);
		err = -ENOMEM;
		goto err_out2;
	}
	/*signal post timeline*/
	intel_adf_sync_timeline_signal(dev->post_timeline,
		ktime_to_ns(ktime_get()));

	/*adf device init*/
	err = adf_device_init(&dev->base, &pdev->dev,
			&intel_adf_device_ops, INTEL_ADF_DEVICE_NAME);
	if (err) {
		dev_err(&pdev->dev, "%s: failed to init adf device, %d\n",
			__func__, err);
		goto err_out3;
	}

#ifndef CONFIG_ADF_INTEL_VLV
	/*init mm*/
	err = intel_adf_mm_init(&dev->mm, &dev->base.base.dev, mem);
	if (err) {
		dev_err(&pdev->dev, "%s: failed to init adf memory manager\n",
			__func__);
		goto err_out4;
	}
#endif

	g_intel_adf_dev = dev;

	dev_info(&pdev->dev, "%s: success\n", __func__);

	return dev;
#ifndef CONFIG_ADF_INTEL_VLV
err_out4:
#endif
	adf_device_destroy(&dev->base);
err_out3:
	intel_adf_sync_timeline_destroy(dev->post_timeline);
err_out2:
	iounmap(mmio);
err_out1:
	kfree(dev);
err_out0:
	return ERR_PTR(err);
}

void intel_adf_device_destroy(struct intel_adf_device *dev)
{
	if (dev) {
#ifndef CONFIG_ADF_INTEL_VLV
		intel_adf_mm_destroy(&dev->mm);
#endif
		adf_device_destroy(&dev->base);
		intel_adf_sync_timeline_destroy(dev->post_timeline);
		iounmap(dev->mmio);
		kfree(dev);
	}
}
