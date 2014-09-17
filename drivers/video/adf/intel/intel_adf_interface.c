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
#include <video/adf_client.h>
#include "intel_adf.h"

static long intel_interface_obj_ioctl(struct adf_obj *obj,
	unsigned int cmd, unsigned long arg)
{
	return -EOPNOTSUPP;
}

static u32 to_intel_pipe_event(enum adf_event_type event)
{
	u32 pipe_event;

	switch (event) {
	case ADF_EVENT_VSYNC:
		pipe_event =  INTEL_PIPE_EVENT_VSYNC;
		break;
	case ADF_EVENT_HOTPLUG:
		pipe_event = (INTEL_PIPE_EVENT_HOTPLUG_CONNECTED |
			INTEL_PIPE_EVENT_HOTPLUG_DISCONNECTED);
		break;
	default:
		pipe_event = INTEL_PIPE_EVENT_UNKNOWN;
		break;
	}

	return pipe_event;
}

static bool intel_interface_obj_supports_event(struct adf_obj *obj,
	enum adf_event_type type)
{
	u32 pipe_event = to_intel_pipe_event(type);
	struct adf_interface *intf = adf_obj_to_interface(obj);
	struct intel_adf_interface *i_intf = to_intel_intf(intf);
	struct intel_pipe *pipe = i_intf->pipe;
	u32 supported_events;

	if (!pipe || !pipe->ops || !pipe->ops->get_supported_events)
		return false;

	if (pipe_event == INTEL_PIPE_EVENT_UNKNOWN)
		return false;

	supported_events = pipe->ops->get_supported_events(pipe);

	if ((supported_events & pipe_event) != pipe_event)
		return false;

	return true;
}

static void intel_interface_obj_set_event(struct adf_obj *obj,
	enum adf_event_type type, bool enabled)
{
	u32 pipe_event = to_intel_pipe_event(type);
	struct adf_interface *intf = adf_obj_to_interface(obj);
	struct intel_adf_interface *i_intf = to_intel_intf(intf);
	struct intel_pipe *pipe = i_intf->pipe;

	if (!pipe || !pipe->ops || !pipe->ops->set_event)
		return;

	if (pipe_event == INTEL_PIPE_EVENT_UNKNOWN)
		return;

	pipe->ops->set_event(pipe, pipe_event, enabled);
}

static int intel_interface_blank(struct adf_interface *intf, u8 state)
{
	struct adf_device *parent = adf_interface_parent(intf);
	struct device *dev = &parent->base.dev;
	struct intel_adf_interface *i_intf = to_intel_intf(intf);
	struct intel_pipe *pipe = i_intf->pipe;

	dev_info(dev, "%s: state %d\n", __func__, state);

	if (!pipe || !pipe->ops || !pipe->ops->dpms)
		return -EOPNOTSUPP;

	return pipe->ops->dpms(pipe, state);
}

static int intel_interface_alloc_simple_buffer(struct adf_interface *intf,
	u16 w, u16 h, u32 format, struct dma_buf **dma_buf, u32 *offset,
	u32 *pitch)
{
	struct intel_adf_device *dev = intf_to_dev(intf);
#ifndef CONFIG_ADF_INTEL_VLV
	struct intel_adf_mm *mm = &dev->mm;
#endif
	u8 bpp = adf_format_bpp(format);
	u32 stride = ((w * bpp / 8) + 63) & ~63;
	u32 size = stride * h;

	*offset = 0;
	*pitch = stride;

	dev_info(dev->base.dev, "%s: size %d\n", __func__, size);

#ifdef CONFIG_ADF_INTEL_VLV
	/*
	 * For VLV we use GEM memory manager. Will be enabled when
	 * we fix the FB driver. As of now memory is preallocate in
	 * user space via DRM PRIME interface which gives DMA Buf
	 * handles over GEM
	 */
	return -EOPNOTSUPP;
#else
	/*allocate buffer*/
	return intel_adf_mm_alloc_buf(mm, size, dma_buf);
#endif
}

static int intel_interface_describe_simple_post(
					struct adf_interface *intf,
					struct adf_buffer *fb,
					void *data,
					size_t *size)
{
	struct intel_adf_device *adf_dev = intf_to_dev(intf);
	struct device *dev = adf_dev->base.dev;
	struct intel_adf_post_custom_data *custom_data = data;
	size_t custom_size;

	dev_info(dev, "%s: buffer %dx%d\n", __func__, fb->w, fb->h);

	custom_size = sizeof(struct intel_adf_post_custom_data) +
			sizeof(struct intel_adf_overlay);

	custom_data->version = INTEL_ADF_VERSION;
	custom_data->flags = 0;
	custom_data->num_overlays = 1;
	custom_data->overlays[0].plane.overlay_id = -1;
	custom_data->overlays[0].plane.inteface_id = intf->idx;
	custom_data->overlays[0].plane.buffer_id = 0;
	custom_data->overlays[0].plane.flags = 0;
	custom_data->overlays[0].plane.dst_x = 0;
	custom_data->overlays[0].plane.dst_y = 0;
	custom_data->overlays[0].plane.dst_w = fb->w;
	custom_data->overlays[0].plane.dst_h = fb->h;
	custom_data->overlays[0].plane.src_x = 0;
	custom_data->overlays[0].plane.src_y = 0;
	custom_data->overlays[0].plane.src_w = fb->w;
	custom_data->overlays[0].plane.src_h = fb->h;
	custom_data->overlays[0].plane.alpha = 0xff;
	custom_data->overlays[0].plane.compression = INTEL_ADF_UNCOMPRESSED;
	custom_data->overlays[0].plane.blending = INTEL_ADF_BLENDING_NONE;
	custom_data->overlays[0].plane.transform = INTEL_ADF_TRANSFORM_NONE;

	*size = custom_size;

	return 0;
}

static int intel_interface_modeset(struct adf_interface *intf,
				struct drm_mode_modeinfo *mode)
{
	struct intel_adf_device *adf_dev = intf_to_dev(intf);
	struct device *dev = adf_dev->base.dev;
	struct intel_adf_interface *i_intf = to_intel_intf(intf);
	struct intel_pipe *pipe = i_intf->pipe;

	if (!mode) {
		dev_err(dev, "%s: invalid mode\n", __func__);
		return -EINVAL;
	}

	if (!pipe || !pipe->ops || !pipe->ops->modeset)
		return -EOPNOTSUPP;

	return pipe->ops->modeset(pipe, mode);
}

static int intel_interface_screen_size(struct adf_interface *intf,
				u16 *width_mm, u16 *height_mm)
{
	struct intel_adf_interface *i_intf = to_intel_intf(intf);
	struct intel_pipe *pipe = i_intf->pipe;

	if (!pipe || !pipe->ops || !pipe->ops->get_screen_size)
		return -EOPNOTSUPP;

	return pipe->ops->get_screen_size(pipe, width_mm, height_mm);
}


static const struct adf_interface_ops intel_adf_interface_ops = {
	.base = {
		.ioctl = intel_interface_obj_ioctl,
		.supports_event = intel_interface_obj_supports_event,
		.set_event = intel_interface_obj_set_event,
	},
	.blank = intel_interface_blank,
	.alloc_simple_buffer = intel_interface_alloc_simple_buffer,
	.describe_simple_post = intel_interface_describe_simple_post,
	.modeset = intel_interface_modeset,
	.screen_size = intel_interface_screen_size,
	.type_str = adf_interface_type_str
};

static inline enum adf_interface_type to_adf_interface_type(
		enum intel_pipe_type type)
{
	switch (type) {
	case INTEL_PIPE_DSI:
		return ADF_INTF_DSI;
	case INTEL_PIPE_HDMI:
		return ADF_INTF_HDMI;
	default:
		return ADF_INTF_TYPE_MAX;
	}
}

static int set_preferred_mode(struct intel_adf_interface *intf)
{
	struct intel_pipe *pipe = intf->pipe;
	struct drm_mode_modeinfo *preferred_mode;
	struct drm_mode_modeinfo *modelist;
	size_t n_modes;
	int err;

	if (!pipe || !pipe->ops || !pipe->ops->get_preferred_mode ||
		!pipe->ops->get_modelist)
		return -EINVAL;

	pipe->ops->get_modelist(pipe, &modelist, &n_modes);
	pipe->ops->get_preferred_mode(pipe, &preferred_mode);


	err = adf_interface_set_mode(&intf->base, preferred_mode);
	if (err)
		goto out_err0;

	err = adf_interface_blank(&intf->base, DRM_MODE_DPMS_ON);
	if (err)
		goto out_err0;

	adf_hotplug_notify_connected(&intf->base, modelist, n_modes);

	return 0;
out_err0:
	return err;
}

static void hotplug_connected_work_func(struct kthread_work *work)
{
	struct intel_adf_interface *intf =
		container_of(work, struct intel_adf_interface,
		hotplug_connected_work);

	set_preferred_mode(intf);
}

static void handle_vsync_event(struct intel_adf_interface *intf)
{
	struct intel_pipe *pipe = intf->pipe;
	ktime_t timestamp;
	u32 seqno;

	if (!pipe || !pipe->ops || !pipe->ops->get_vsync_counter)
		return;

	timestamp = ktime_get();
	seqno = pipe->ops->get_vsync_counter(pipe, 0);

	pr_debug("%s: get vsync event on %llu, seqno = %u\n", __func__,
		ktime_to_ms(timestamp), seqno);

	adf_vsync_notify(&intf->base, timestamp);

	intel_adf_sync_timeline_signal(intf->vsync_timeline, seqno);
}

static void handle_hotplug_connected(struct intel_adf_interface *intf)
{
	struct intel_pipe *pipe = intf->pipe;

	if (!pipe || !pipe->ops || !pipe->ops->is_screen_connected)
		return;

	if (pipe->ops->is_screen_connected(pipe))
		queue_kthread_work(&intf->event_handling_worker,
			&intf->hotplug_connected_work);
}

static void handle_hotplug_disconnected(struct intel_adf_interface *intf)
{

}

int intel_adf_interface_handle_event(struct intel_adf_interface *intf)
{
	struct intel_pipe *pipe = intf->pipe;
	u32 events = 0;

	if (!pipe || !pipe->ops || !pipe->ops->get_events)
		return IRQ_NONE;

	pipe->ops->get_events(pipe, &events);

	if (!events)
		return IRQ_NONE;

	if (events & INTEL_PIPE_EVENT_VSYNC) {
		handle_vsync_event(intf);
		events &= ~INTEL_PIPE_EVENT_VSYNC;
	}

	if (events & INTEL_PIPE_EVENT_HOTPLUG_CONNECTED) {
		handle_hotplug_connected(intf);
		events &= ~INTEL_PIPE_EVENT_HOTPLUG_CONNECTED;
	}

	if (events & INTEL_PIPE_EVENT_HOTPLUG_DISCONNECTED) {
		handle_hotplug_disconnected(intf);
		events &= ~INTEL_PIPE_EVENT_HOTPLUG_DISCONNECTED;
	}

	if (events && pipe->ops->handle_events)
		pipe->ops->handle_events(pipe, events);

	return IRQ_HANDLED;
}

struct sync_fence *intel_adf_interface_create_vsync_fence(
	struct intel_adf_interface *intf, u32 interval)
{
	struct intel_pipe *pipe;
	u64 value;

	if (!intf || !interval)
		return NULL;

	pipe = intf->pipe;
	if (!pipe || !pipe->ops || !pipe->ops->get_vsync_counter)
		return NULL;

	value = pipe->ops->get_vsync_counter(pipe, interval);

	return intel_adf_sync_fence_create(intf->vsync_timeline, value);
}

int intel_adf_interface_init(struct intel_adf_interface *intf,
		struct intel_adf_device *dev, struct intel_pipe *pipe)
{
	enum adf_interface_type type;
	u32 flags = 0;
	int err;

	if (!intf || !dev || !pipe || !pipe->ops ||
		!pipe->ops->is_screen_connected ||
		!pipe->ops->get_vsync_counter)
		return -EINVAL;

	memset(intf, 0, sizeof(struct intel_adf_interface));

	INIT_LIST_HEAD(&intf->active_list);

	if (pipe->primary)
		flags |= ADF_INTF_FLAG_PRIMARY;
	else
		flags |= ADF_INTF_FLAG_EXTERNAL;

	type = to_adf_interface_type(pipe->type);
	if (type == ADF_INTF_TYPE_MAX) {
		dev_err(dev->base.dev, "%s: invalid pipe type %d\n",
			__func__, pipe->type);
		return -EINVAL;
	}

	if (pipe->ops->hw_init) {
		err = pipe->ops->hw_init(pipe);
		if (err) {
			dev_err(dev->base.dev, "%s: failed to init pipe\n",
				__func__);
			goto out_err0;
		}
	}

	intf->pipe = pipe;
	init_kthread_worker(&intf->event_handling_worker);
	init_kthread_work(&intf->hotplug_connected_work,
		hotplug_connected_work_func);
	intf->event_handling_thread = kthread_run(kthread_worker_fn,
		&intf->event_handling_worker, "intf-worker-%d", pipe->base.idx);
	if (IS_ERR(intf->event_handling_thread)) {
		dev_err(dev->base.dev,
			"%s: failed to create event handling thread\n",
			__func__);
		err = PTR_ERR(intf->event_handling_thread);
		goto out_err1;
	}

	/*create vsync timeline*/
	intf->vsync_timeline = intel_adf_sync_timeline_create("vsync");
	if (IS_ERR(intf->vsync_timeline)) {
		dev_err(dev->base.dev, "%s: failed to create vsync timeline\n",
			__func__);
		err = PTR_ERR(intf->vsync_timeline);
		goto out_err2;
	}
	/*signal vsync timeline*/
	intel_adf_sync_timeline_signal(intf->vsync_timeline,
		pipe->ops->get_vsync_counter(pipe, 0));

	err = adf_interface_init(&intf->base, &dev->base, type,
				(u32)pipe->base.idx, flags,
				&intel_adf_interface_ops, "intel_intf_%s",
				pipe->base.name);
	if (err)
		goto out_err3;

	/* fill the interface modelist */
	pipe->ops->get_modelist(pipe, &intf->base.modelist,
				&intf->base.n_modes);

	/*turn on this interface if screen was connected*/
	if (pipe->ops->is_screen_connected(pipe)) {
		err = set_preferred_mode(intf);
		if (err) {
			dev_err(dev->base.dev, "%s: failed to handle hotplug\n",
				__func__);
			goto out_err4;
		}
	}
	return 0;
out_err4:
	adf_interface_destroy(&intf->base);
out_err3:
	intel_adf_sync_timeline_destroy(intf->vsync_timeline);
out_err2:
	kthread_stop(intf->event_handling_thread);
out_err1:
	if (pipe->ops->hw_deinit)
		pipe->ops->hw_deinit(pipe);
out_err0:
	return err;
}

void intel_adf_interface_destroy(struct intel_adf_interface *intf)
{
	struct intel_pipe *pipe;

	if (intf) {
		if (intf->event_handling_thread) {
			flush_kthread_worker(&intf->event_handling_worker);
			kthread_stop(intf->event_handling_thread);
		}

		pipe = intf->pipe;
		if (pipe && pipe->ops && pipe->ops->hw_deinit)
			pipe->ops->hw_deinit(pipe);

		intel_adf_sync_timeline_destroy(intf->vsync_timeline);

		intf->pipe = NULL;
		adf_interface_destroy(&intf->base);
	}
}
