/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#include <linux/bitmap.h>
#include <linux/completion.h>
#include <linux/ion.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <mach/iommu_domains.h>
#include <media/msm_vidc.h>
#include <media/v4l2-subdev.h>
#include "enc-subdev.h"
#include "wfd-util.h"

#define BUF_TYPE_OUTPUT V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
#define BUF_TYPE_INPUT V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE

static struct ion_client *venc_ion_client;
static long venc_secure(struct v4l2_subdev *sd);

struct index_bitmap {
	unsigned long *bitmap;
	int size;
	int size_bits; /*Size in bits, not necessarily size/8 */
};

struct venc_inst {
	void *vidc_context;
	struct mutex lock;
	struct venc_msg_ops vmops;
	struct mem_region registered_input_bufs, registered_output_bufs;
	struct index_bitmap free_input_indices, free_output_indices;
	int num_output_planes, num_input_planes;
	struct task_struct *callback_thread;
	bool callback_thread_running;
	struct completion dq_complete, cmd_complete;
	bool secure;
	struct workqueue_struct *fill_buf_wq;
};

struct fill_buf_work {
	struct venc_inst *inst;
	struct mem_region *mregion;
	struct work_struct work;
};

static const int subscribed_events[] = {
	V4L2_EVENT_MSM_VIDC_CLOSE_DONE,
	V4L2_EVENT_MSM_VIDC_FLUSH_DONE,
	V4L2_EVENT_MSM_VIDC_SYS_ERROR,
};

int venc_load_fw(struct v4l2_subdev *sd)
{
	/*No need to explicitly load the fw */
	return 0;
}

int venc_init(struct v4l2_subdev *sd, u32 val)
{
	if (!venc_ion_client)
		venc_ion_client = msm_ion_client_create(-1, "wfd_enc_subdev");

	return venc_ion_client ? 0 : -ENOMEM;
}

static int invalidate_cache(struct ion_client *client,
		struct mem_region *mregion)
{
	if (!client || !mregion) {
		WFD_MSG_ERR(
			"Failed to flush ion buffer: invalid client or region\n");
		return -EINVAL;
	} else if (!mregion->ion_handle) {
		WFD_MSG_ERR(
			"Failed to flush ion buffer: not an ion buffer\n");
		return -EINVAL;
	}

	return msm_ion_do_cache_op(client,
			mregion->ion_handle,
			mregion->kvaddr,
			mregion->size,
			ION_IOC_INV_CACHES);

}
static int next_free_index(struct index_bitmap *index_bitmap)
{
	int index = find_first_zero_bit(index_bitmap->bitmap,
			index_bitmap->size_bits);

	return (index >= index_bitmap->size_bits) ?
		-1 : index;
}

static int mark_index_busy(struct index_bitmap *index_bitmap, int index)
{
	if (index > index_bitmap->size_bits) {
		WFD_MSG_WARN("Marking unknown index as busy\n");
		return -EINVAL;
	}
	set_bit(index, index_bitmap->bitmap);
	return 0;
}

static int mark_index_free(struct index_bitmap *index_bitmap, int index)
{
	if (index > index_bitmap->size_bits) {
		WFD_MSG_WARN("Marking unknown index as free\n");
		return -EINVAL;
	}
	clear_bit(index, index_bitmap->bitmap);
	return 0;
}

static int get_list_len(struct mem_region *list)
{
	struct mem_region *curr = NULL;
	int index = 0;
	list_for_each_entry(curr, &list->list, list) {
		++index;
	}

	return index;
}

static struct mem_region *get_registered_mregion(struct mem_region *list,
		struct mem_region *mregion)
{
	struct mem_region *curr = NULL;
	list_for_each_entry(curr, &list->list, list) {
		if (unlikely(mem_region_equals(curr, mregion)))
			return curr;
	}

	return NULL;
}

static int venc_vidc_callback_thread(void *data)
{
	struct venc_inst *inst = data;
	WFD_MSG_DBG("Starting callback thread\n");
	while (!kthread_should_stop()) {
		bool dequeue_buf = false;
		struct v4l2_buffer buffer = {0};
		struct v4l2_event event = {0};
		int num_planes = 0;
		int flags = msm_vidc_wait(inst->vidc_context);

		if (flags & POLLERR) {
			WFD_MSG_ERR("Encoder reported error\n");
			break;
		}

		if (flags & POLLPRI) {
			bool bail_out = false;

			msm_vidc_dqevent(inst->vidc_context, &event);

			switch (event.type) {
			case V4L2_EVENT_MSM_VIDC_CLOSE_DONE:
				WFD_MSG_DBG("enc callback thread shutting " \
						"down normally\n");
				bail_out = true;
				break;
			case V4L2_EVENT_MSM_VIDC_SYS_ERROR:
				inst->vmops.on_event(inst->vmops.cbdata,
						VENC_EVENT_HARDWARE_ERROR);
				bail_out = true;
				break;
			default:
				WFD_MSG_INFO("Got unknown event %d, ignoring\n",
						event.type);
			}

			complete_all(&inst->cmd_complete);
			if (bail_out)
				break;
		}

		if (flags & POLLIN || flags & POLLRDNORM) {
			buffer.type = BUF_TYPE_OUTPUT;
			dequeue_buf = true;
			num_planes = inst->num_output_planes;
			WFD_MSG_DBG("Output buffer ready!\n");
		}

		if (flags & POLLOUT || flags & POLLWRNORM) {
			buffer.type = BUF_TYPE_INPUT;
			dequeue_buf = true;
			num_planes = inst->num_input_planes;
			WFD_MSG_DBG("Input buffer ready!\n");
		}

		if (dequeue_buf) {
			int rc = 0;
			struct v4l2_plane *planes = NULL;
			struct mem_region *curr = NULL, *mregion = NULL;
			struct list_head *reg_bufs = NULL;
			struct index_bitmap *bitmap = NULL;

			planes = kzalloc(sizeof(*planes) * num_planes,
					GFP_KERNEL);
			buffer.m.planes = planes;
			buffer.length = 1;
			buffer.memory = V4L2_MEMORY_USERPTR;
			rc = msm_vidc_dqbuf(inst->vidc_context, &buffer);

			if (rc) {
				WFD_MSG_ERR("Error dequeuing buffer " \
						"from vidc: %d", rc);
				goto abort_dequeue;
			}

			reg_bufs = buffer.type == BUF_TYPE_OUTPUT ?
				&inst->registered_output_bufs.list :
				&inst->registered_input_bufs.list;

			bitmap = buffer.type == BUF_TYPE_OUTPUT ?
				&inst->free_output_indices :
				&inst->free_input_indices;

			list_for_each_entry(curr, reg_bufs, list) {
				if ((u32)curr->paddr ==
						buffer.m.planes[0].m.userptr) {
					mregion = curr;
					break;
				}
			}

			if (!mregion) {
				WFD_MSG_ERR("Got done msg for unknown buf\n");
				goto abort_dequeue;
			}

			if (buffer.type == BUF_TYPE_OUTPUT &&
				inst->vmops.op_buffer_done) {
				struct vb2_buffer *vb =
					(struct vb2_buffer *)mregion->cookie;

				vb->v4l2_buf.flags = buffer.flags;
				vb->v4l2_buf.timestamp = buffer.timestamp;
				vb->v4l2_planes[0].bytesused =
					buffer.m.planes[0].bytesused;

				/* Buffer is on its way to userspace, so
				 * invalidate the cache */
				rc = invalidate_cache(venc_ion_client, mregion);
				if (rc) {
					WFD_MSG_WARN(
						"Failed to invalidate cache %d\n",
						rc);
					/* Not fatal, move on */
				}

				inst->vmops.op_buffer_done(
					inst->vmops.cbdata, 0, vb);
			} else if (buffer.type == BUF_TYPE_INPUT &&
					inst->vmops.ip_buffer_done) {
				inst->vmops.ip_buffer_done(
						inst->vmops.cbdata,
						0, mregion);
			}

			complete_all(&inst->dq_complete);
			mutex_lock(&inst->lock);
			mark_index_free(bitmap, buffer.index);
			mutex_unlock(&inst->lock);
abort_dequeue:
			kfree(planes);
		}
	}


	WFD_MSG_DBG("Exiting callback thread\n");
	mutex_lock(&inst->lock);
	inst->callback_thread_running = false;
	mutex_unlock(&inst->lock);
	return 0;
}

static long set_default_properties(struct venc_inst *inst)
{
	struct v4l2_control ctrl = {0};

	/* Set the IDR period as 1.  The venus core doesn't give
	 * the sps/pps for I-frames, only IDR. */
	ctrl.id = V4L2_CID_MPEG_VIDC_VIDEO_IDR_PERIOD;
	ctrl.value = 1;

	return msm_vidc_s_ctrl(inst->vidc_context, &ctrl);
}

static int subscribe_events(struct venc_inst *inst)
{
	struct v4l2_event_subscription event = {0};
	int c = 0, rc = 0;

	for (c = 0; c < ARRAY_SIZE(subscribed_events); c++) {
		event.type = subscribed_events[c];
		rc = msm_vidc_subscribe_event(inst->vidc_context, &event);
		if (rc) {
			WFD_MSG_ERR("Failed to subscribe to event 0x%x\n",
					subscribed_events[c]);
			return rc;
		}
	}

	return 0;
}

static void unsubscribe_events(struct venc_inst *inst)
{
	struct v4l2_event_subscription event = {0};
	int c = 0, rc = 0;
	for (c = 0; c < ARRAY_SIZE(subscribed_events); c++) {
		event.type = subscribed_events[c];
		rc = msm_vidc_unsubscribe_event(inst->vidc_context, &event);
		if (rc) {
			/* Just log and ignore failiures */
			WFD_MSG_WARN("Failed to unsubscribe to event 0x%x\n",
					subscribed_events[c]);
		}
	}
}

static long venc_open(struct v4l2_subdev *sd, void *arg)
{
	struct venc_inst *inst = NULL;
	struct venc_msg_ops *vmops = arg;
	int rc = 0;

	if (!vmops) {
		WFD_MSG_ERR("Callbacks required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_open_fail;
	} else if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_open_fail;
	}

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst) {
		WFD_MSG_ERR("Failed to allocate memory\n");
		rc = -EINVAL;
		goto venc_open_fail;
	}

	inst->vmops = *vmops;
	inst->secure = vmops->secure; /* We need to inform vidc, but defer
					 until after s_fmt() */
	INIT_LIST_HEAD(&inst->registered_output_bufs.list);
	INIT_LIST_HEAD(&inst->registered_input_bufs.list);
	init_completion(&inst->dq_complete);
	init_completion(&inst->cmd_complete);
	mutex_init(&inst->lock);

	inst->fill_buf_wq = create_singlethread_workqueue("venc_vidc_ftb_wq");
	if (!inst->fill_buf_wq) {
		WFD_MSG_ERR("Failed to create ftb wq\n");
		rc = -ENOMEM;
		goto vidc_wq_create_fail;
	}

	inst->vidc_context = msm_vidc_open(MSM_VIDC_CORE_VENUS,
				MSM_VIDC_ENCODER);
	if (!inst->vidc_context) {
		WFD_MSG_ERR("Failed to create vidc context\n");
		rc = -ENXIO;
		goto vidc_open_fail;
	}

	rc = subscribe_events(inst);
	if (rc) {
		WFD_MSG_ERR("Failed to subscribe to events\n");
		goto vidc_subscribe_fail;
	}

	inst->callback_thread = kthread_run(venc_vidc_callback_thread, inst,
					"venc_vidc_callback_thread");
	if (IS_ERR(inst->callback_thread)) {
		WFD_MSG_ERR("Failed to create callback thread\n");
		rc = PTR_ERR(inst->callback_thread);
		inst->callback_thread = NULL;
		goto vidc_kthread_create_fail;
	}
	inst->callback_thread_running = true;

	sd->dev_priv = inst;
	vmops->cookie = inst;
	return 0;
vidc_kthread_create_fail:
	unsubscribe_events(inst);
vidc_subscribe_fail:
	msm_vidc_close(inst->vidc_context);
vidc_open_fail:
	destroy_workqueue(inst->fill_buf_wq);
vidc_wq_create_fail:
	kfree(inst);
venc_open_fail:
	return rc;
}

static long venc_close(struct v4l2_subdev *sd, void *arg)
{
	struct venc_inst *inst = NULL;
	struct v4l2_encoder_cmd enc_cmd = {0};
	int rc = 0;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_close_fail;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	enc_cmd.cmd = V4L2_ENC_CMD_STOP;
	msm_vidc_encoder_cmd(inst->vidc_context, &enc_cmd);

	wait_for_completion(&inst->cmd_complete);

	destroy_workqueue(inst->fill_buf_wq);
	if (inst->callback_thread && inst->callback_thread_running)
		kthread_stop(inst->callback_thread);

	unsubscribe_events(inst);

	rc = msm_vidc_close(inst->vidc_context);
	if (rc)
		WFD_MSG_WARN("Failed to close vidc context\n");

	kfree(inst);
	sd->dev_priv = inst = NULL;
venc_close_fail:
	return rc;
}

static long venc_get_buffer_req(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = NULL;
	struct bufreq *bufreq = arg;
	struct v4l2_requestbuffers v4l2_bufreq = {0};
	struct v4l2_format v4l2_format = {0};

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_buf_req_fail;
	} else if (!arg) {
		WFD_MSG_ERR("Invalid buffer requirements\n");
		rc = -EINVAL;
		goto venc_buf_req_fail;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	/* Get buffer count */
	v4l2_bufreq = (struct v4l2_requestbuffers) {
		.count = bufreq->count,
		.type = BUF_TYPE_OUTPUT,
		.memory = V4L2_MEMORY_USERPTR,
	};

	rc = msm_vidc_reqbufs(inst->vidc_context, &v4l2_bufreq);
	if (rc) {
		WFD_MSG_ERR("Failed getting buffer requirements\n");
		goto venc_buf_req_fail;
	}

	/* Get buffer size */
	v4l2_format.type = BUF_TYPE_OUTPUT;
	rc = msm_vidc_g_fmt(inst->vidc_context, &v4l2_format);
	if (rc) {
		WFD_MSG_ERR("Failed getting OP buffer size\n");
		goto venc_buf_req_fail;
	}

	bufreq->count = v4l2_bufreq.count;
	bufreq->size = v4l2_format.fmt.pix_mp.plane_fmt[0].sizeimage;

	inst->free_output_indices.size_bits = bufreq->count;
	inst->free_output_indices.size = roundup(bufreq->count,
				sizeof(unsigned long)) / sizeof(unsigned long);
	inst->free_output_indices.bitmap = kzalloc(inst->free_output_indices.
						size, GFP_KERNEL);
venc_buf_req_fail:
	return rc;
}

static long venc_set_buffer_req(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = NULL;
	struct bufreq *bufreq = arg;
	struct v4l2_requestbuffers v4l2_bufreq = {0};
	struct v4l2_format v4l2_format = {0};

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_buf_req_fail;
	} else if (!arg) {
		WFD_MSG_ERR("Invalid buffer requirements\n");
		rc = -EINVAL;
		goto venc_buf_req_fail;
	}

	inst = (struct venc_inst *)sd->dev_priv;

	/* Attempt to set buffer count */
	v4l2_bufreq = (struct v4l2_requestbuffers) {
		.count = bufreq->count,
		.type = BUF_TYPE_INPUT,
		.memory = V4L2_MEMORY_USERPTR,
	};

	rc = msm_vidc_reqbufs(inst->vidc_context, &v4l2_bufreq);
	if (rc) {
		WFD_MSG_ERR("Failed getting buffer requirements");
		goto venc_buf_req_fail;
	}

	/* Get buffer size */
	v4l2_format.type = BUF_TYPE_INPUT;
	rc = msm_vidc_g_fmt(inst->vidc_context, &v4l2_format);
	if (rc) {
		WFD_MSG_ERR("Failed getting OP buffer size\n");
		goto venc_buf_req_fail;
	}

	bufreq->count = v4l2_bufreq.count;
	bufreq->size = ALIGN(v4l2_format.fmt.pix_mp.plane_fmt[0].sizeimage,
			inst->secure ? SZ_1M : SZ_4K);

	inst->free_input_indices.size_bits = bufreq->count;
	inst->free_input_indices.size = roundup(bufreq->count,
				sizeof(unsigned long)) / sizeof(unsigned long);
	inst->free_input_indices.bitmap = kzalloc(inst->free_input_indices.
						size, GFP_KERNEL);
venc_buf_req_fail:
	return rc;
}

static long venc_start(struct v4l2_subdev *sd)
{
	struct venc_inst *inst = NULL;
	int rc = 0;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_start_fail;
	}

	inst = (struct venc_inst *)sd->dev_priv;

	if (set_default_properties(inst))
		WFD_MSG_WARN("Couldn't set default properties\n");

	rc = msm_vidc_streamon(inst->vidc_context, BUF_TYPE_OUTPUT);
	if (rc) {
		WFD_MSG_ERR("Failed to streamon vidc's output port");
		goto venc_start_fail;
	}

	rc = msm_vidc_streamon(inst->vidc_context, BUF_TYPE_INPUT);
	if (rc) {
		WFD_MSG_ERR("Failed to streamon vidc's input port");
		goto venc_start_fail;
	}

venc_start_fail:
	return rc;
}

static long venc_stop(struct v4l2_subdev *sd)
{
	struct venc_inst *inst = NULL;
	int rc = 0;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_stop_fail;
	}

	inst = (struct venc_inst *)sd->dev_priv;

	flush_workqueue(inst->fill_buf_wq);
	rc = msm_vidc_streamoff(inst->vidc_context, BUF_TYPE_INPUT);
	if (rc) {
		WFD_MSG_ERR("Failed to streamoff vidc's input port");
		goto venc_stop_fail;
	}

	rc = msm_vidc_streamoff(inst->vidc_context, BUF_TYPE_OUTPUT);
	if (rc) {
		WFD_MSG_ERR("Failed to streamoff vidc's output port");
		goto venc_stop_fail;
	}

venc_stop_fail:
	return rc;
}

static void populate_planes(struct v4l2_plane *planes, int num_planes,
		void *userptr, int size)
{
	int c = 0;

	planes[0] = (struct v4l2_plane) {
		.length = size,
		.m.userptr = (int)userptr,
	};

	for (c = 1; c < num_planes - 1; ++c) {
		planes[c] = (struct v4l2_plane) {
			.length = 0,
			.m.userptr = (int)NULL,
		};
	}
}

static long venc_set_input_buffer(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = NULL;
	struct v4l2_buffer buf = {0};
	struct v4l2_plane *planes = NULL;
	struct mem_region *mregion = arg;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc =  -EINVAL;
		goto set_input_buffer_fail;
	} else if (!arg) {
		WFD_MSG_ERR("Invalid input buffer\n");
		rc =  -EINVAL;
		goto set_input_buffer_fail;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	if (get_registered_mregion(&inst->registered_input_bufs, mregion)) {
		WFD_MSG_ERR("Duplicate input buffer\n");
		rc = -EEXIST;
		goto set_input_buffer_fail;
	}

	mregion = kzalloc(sizeof(*mregion), GFP_KERNEL);
	planes = kzalloc(sizeof(*planes) * inst->num_input_planes, GFP_KERNEL);
	if (!mregion || !planes)
		return -ENOMEM;

	*mregion = *(struct mem_region *)arg;
	populate_planes(planes, inst->num_input_planes,
			mregion->paddr, mregion->size);

	buf = (struct v4l2_buffer) {
		.index = get_list_len(&inst->registered_input_bufs),
		.type = BUF_TYPE_INPUT,
		.bytesused = 0,
		.memory = V4L2_MEMORY_USERPTR,
		.m.planes = planes,
		.length = inst->num_input_planes,
	};

	WFD_MSG_DBG("Prepare %p with index, %d",
		(void *)buf.m.planes[0].m.userptr, buf.index);
	rc = msm_vidc_prepare_buf(inst->vidc_context, &buf);
	if (rc) {
		WFD_MSG_ERR("Failed to prepare input buffer\n");
		goto set_input_buffer_fail;
	}

	list_add_tail(&mregion->list, &inst->registered_input_bufs.list);

	kfree(planes);
	return 0;
set_input_buffer_fail:
	kfree(mregion);
	kfree(planes);
	return rc;
}

#ifdef CONFIG_MSM_WFD_DEBUG
static void *venc_map_kernel(struct ion_client *client,
		struct ion_handle *handle)
{
	return ion_map_kernel(client, handle);
}

static void venc_unmap_kernel(struct ion_client *client,
		struct ion_handle *handle)
{
	ion_unmap_kernel(client, handle);
}
#else

static void *venc_map_kernel(struct ion_client *client,
		struct ion_handle *handle)
{
	return NULL;
}

static void venc_unmap_kernel(struct ion_client *client,
		struct ion_handle *handle)
{
	return;
}
#endif

static int venc_map_user_to_kernel(struct venc_inst *inst,
		struct mem_region *mregion)
{
	int rc = 0;
	unsigned long size = 0, align_req = 0, flags = 0;
	int domain = 0, partition = 0;

	if (!mregion) {
		rc = -EINVAL;
		goto venc_map_fail;
	}

	align_req = inst->secure ? SZ_1M : SZ_4K;
	if (mregion->size % align_req != 0) {
		WFD_MSG_ERR("Memregion not aligned to %ld\n", align_req);
		rc = -EINVAL;
		goto venc_map_fail;
	}

	mregion->ion_handle = ion_import_dma_buf(venc_ion_client, mregion->fd);
	if (IS_ERR_OR_NULL(mregion->ion_handle)) {
		rc = PTR_ERR(mregion->ion_handle);
		WFD_MSG_ERR("Failed to get handle: %p, %d, %d, %d\n",
			venc_ion_client, mregion->fd, mregion->offset, rc);
		mregion->ion_handle = NULL;
		goto venc_map_fail;
	}

	rc = ion_handle_get_flags(venc_ion_client, mregion->ion_handle, &flags);
	if (rc) {
		WFD_MSG_ERR("Failed to get ion flags %d\n", rc);
		goto venc_map_fail;
	}

	mregion->kvaddr = inst->secure ? NULL :
		venc_map_kernel(venc_ion_client, mregion->ion_handle);

	if (inst->secure) {
		rc = msm_ion_secure_buffer(venc_ion_client,
			mregion->ion_handle, VIDEO_BITSTREAM, 0);
		if (rc) {
			WFD_MSG_ERR("Failed to secure output buffer\n");
			goto venc_map_iommu_map_fail;
		}
	}

	rc = msm_vidc_get_iommu_domain_partition(inst->vidc_context,
			flags, BUF_TYPE_OUTPUT, &domain, &partition);
	if (rc) {
		WFD_MSG_ERR("Failed to get domain for output buffer\n");
		goto venc_domain_fail;
	}

	rc = ion_map_iommu(venc_ion_client, mregion->ion_handle,
			domain, partition, align_req, 0,
			(unsigned long *)&mregion->paddr, &size, 0, 0);
	if (rc) {
		WFD_MSG_ERR("Failed to map into iommu\n");
		goto venc_map_iommu_map_fail;
	} else if (size < mregion->size) {
		WFD_MSG_ERR("Failed to iommu map the correct size\n");
		goto venc_map_iommu_size_fail;
	}

	return 0;
venc_map_iommu_size_fail:
	ion_unmap_iommu(venc_ion_client, mregion->ion_handle,
			domain, partition);
venc_domain_fail:
	if (inst->secure)
		msm_ion_unsecure_buffer(venc_ion_client, mregion->ion_handle);
venc_map_iommu_map_fail:
	if (!inst->secure && !IS_ERR_OR_NULL(mregion->kvaddr))
		venc_unmap_kernel(venc_ion_client, mregion->ion_handle);
venc_map_fail:
	return rc;
}

static int venc_unmap_user_to_kernel(struct venc_inst *inst,
		struct mem_region *mregion)
{
	unsigned long flags = 0;
	int domain = 0, partition = 0, rc = 0;

	if (!mregion || !mregion->ion_handle)
		return 0;

	rc = ion_handle_get_flags(venc_ion_client, mregion->ion_handle, &flags);
	if (rc) {
		WFD_MSG_ERR("Failed to get ion flags %d\n", rc);
		return rc;
	}

	rc = msm_vidc_get_iommu_domain_partition(inst->vidc_context,
		flags, BUF_TYPE_OUTPUT, &domain, &partition);
	if (rc) {
		WFD_MSG_ERR("Failed to get domain for input buffer\n");
		return rc;
	}

	if (mregion->paddr) {
		ion_unmap_iommu(venc_ion_client, mregion->ion_handle,
				domain, partition);
		mregion->paddr = NULL;
	}

	if (!IS_ERR_OR_NULL(mregion->kvaddr)) {
		venc_unmap_kernel(venc_ion_client, mregion->ion_handle);
		mregion->kvaddr = NULL;
	}

	if (inst->secure)
		msm_ion_unsecure_buffer(venc_ion_client, mregion->ion_handle);

	ion_free(venc_ion_client, mregion->ion_handle);
	return rc;
}

static long venc_set_output_buffer(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = NULL;
	struct v4l2_buffer buf = {0};
	struct v4l2_plane *planes = NULL;
	struct mem_region *mregion = arg;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_set_output_buffer_fail;
	} else if (!mregion) {
		WFD_MSG_ERR("Invalid output buffer\n");
		rc = -EINVAL;
		goto venc_set_output_buffer_fail;
	}

	inst = (struct venc_inst *)sd->dev_priv;

	/* Check if buf already registered */
	if (get_registered_mregion(&inst->registered_output_bufs, mregion)) {
		WFD_MSG_ERR("Duplicate output buffer\n");
		rc = -EEXIST;
		goto venc_set_output_buffer_fail;
	}

	mregion = kzalloc(sizeof(*mregion), GFP_KERNEL);
	planes = kzalloc(sizeof(*planes) * inst->num_output_planes, GFP_KERNEL);

	if (!mregion || !planes) {
		WFD_MSG_ERR("Failed to allocate memory\n");
		goto venc_set_output_buffer_fail;
	}

	*mregion = *(struct mem_region *)arg;
	INIT_LIST_HEAD(&mregion->list);

	rc = venc_map_user_to_kernel(inst, mregion);
	if (rc) {
		WFD_MSG_ERR("Failed to map output buffer\n");
		goto venc_set_output_buffer_map_fail;
	}

	populate_planes(planes, inst->num_output_planes,
			mregion->paddr, mregion->size);

	buf = (struct v4l2_buffer) {
		.index = get_list_len(&inst->registered_output_bufs),
		.type = BUF_TYPE_OUTPUT,
		.bytesused = 0,
		.memory = V4L2_MEMORY_USERPTR,
		.m.planes = planes,
		.length = inst->num_output_planes,
	};

	WFD_MSG_DBG("Prepare %p with index, %d",
		(void *)buf.m.planes[0].m.userptr, buf.index);
	rc = msm_vidc_prepare_buf(inst->vidc_context, &buf);
	if (rc) {
		WFD_MSG_ERR("Failed to prepare output buffer\n");
		goto venc_set_output_buffer_prepare_fail;
	}

	list_add_tail(&mregion->list, &inst->registered_output_bufs.list);

	kfree(planes);
	return 0;
venc_set_output_buffer_prepare_fail:
	venc_unmap_user_to_kernel(inst, mregion);
venc_set_output_buffer_map_fail:
	kfree(mregion);
	kfree(planes);
venc_set_output_buffer_fail:
	return rc;
}

static long venc_set_format(struct v4l2_subdev *sd, void *arg)
{
	struct venc_inst *inst = NULL;
	struct v4l2_format *fmt = arg, temp;
	int rc = 0, align_req = 0;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_set_format_fail;
	} else if (!fmt) {
		WFD_MSG_ERR("Invalid format\n");
		rc = -EINVAL;
		goto venc_set_format_fail;
	} else if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		WFD_MSG_ERR("Invalid buffer type %d\n", fmt->type);
		rc = -ENOTSUPP;
		goto venc_set_format_fail;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	temp = (struct v4l2_format) {
		.type = BUF_TYPE_OUTPUT,
		.fmt.pix_mp = (struct v4l2_pix_format_mplane) {
			.width = fmt->fmt.pix.width,
			.height = fmt->fmt.pix.height,
			.pixelformat = fmt->fmt.pix.pixelformat,
		},
	};

	rc = msm_vidc_s_fmt(inst->vidc_context, &temp);

	if (rc) {
		WFD_MSG_ERR("Failed to format for output port\n");
		goto venc_set_format_fail;
	} else if (!temp.fmt.pix_mp.num_planes) {
		WFD_MSG_ERR("No. of planes for output buffers make no sense\n");
		rc = -EINVAL;
		goto venc_set_format_fail;
	}

	align_req = inst->secure ? SZ_1M : SZ_4K;
	fmt->fmt.pix.sizeimage = ALIGN(temp.fmt.pix_mp.plane_fmt[0].sizeimage,
					align_req);
	inst->num_output_planes = temp.fmt.pix_mp.num_planes;

	temp.type = BUF_TYPE_INPUT;
	temp.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
	rc = msm_vidc_s_fmt(inst->vidc_context, &temp);
	inst->num_input_planes = temp.fmt.pix_mp.num_planes;

	if (rc) {
		WFD_MSG_ERR("Failed to format for input port\n");
		goto venc_set_format_fail;
	}

	/* If the device was secured previously, we need to inform vidc _now_ */
	if (inst->secure) {
		rc = venc_secure(sd);
		if (rc) {
			WFD_MSG_ERR("Failed secure vidc\n");
			goto venc_set_format_fail;
		}
	}
venc_set_format_fail:
	return rc;
}

static long venc_set_framerate(struct v4l2_subdev *sd, void *arg)
{
	struct venc_inst *inst = NULL;
	struct v4l2_streamparm p = {0};

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		return -EINVAL;
	} else if (!arg) {
		WFD_MSG_ERR("Invalid framerate\n");
		return -EINVAL;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	p.type = BUF_TYPE_INPUT;
	p.parm.output.timeperframe = *(struct v4l2_fract *)arg;
	return msm_vidc_s_parm(inst->vidc_context, &p);
}

static long fill_outbuf(struct venc_inst *inst, struct mem_region *mregion)
{
	struct v4l2_buffer buffer = {0};
	struct v4l2_plane plane = {0};
	int index = 0, rc = 0;

	if (!mregion) {
		WFD_MSG_ERR("Output buffer not registered\n");
		return -ENOENT;
	}

	plane = (struct v4l2_plane) {
		.length = mregion->size,
		.m.userptr = (u32)mregion->paddr,
	};

	while (true) {
		mutex_lock(&inst->lock);
		index = next_free_index(&inst->free_output_indices);
		mutex_unlock(&inst->lock);

		if (index < 0)
			wait_for_completion(&inst->dq_complete);
		else
			break;
	}

	buffer = (struct v4l2_buffer) {
		.index = index,
		.type = BUF_TYPE_OUTPUT,
		.memory = V4L2_MEMORY_USERPTR,
		.m.planes = &plane,
		.length = 1,
	};

	WFD_MSG_DBG("Fill buffer %p with index, %d",
		(void *)buffer.m.planes[0].m.userptr, buffer.index);
	rc = msm_vidc_qbuf(inst->vidc_context, &buffer);
	if (!rc) {
		mutex_lock(&inst->lock);
		mark_index_busy(&inst->free_output_indices, index);
		mutex_unlock(&inst->lock);
	}

	return rc;
}

static void fill_outbuf_helper(struct work_struct *work)
{
	int rc;
	struct fill_buf_work *fbw =
		container_of(work, struct fill_buf_work, work);

	rc = fill_outbuf(fbw->inst, fbw->mregion);
	if (rc) {
		struct vb2_buffer *vb = NULL;

		WFD_MSG_ERR("Failed to fill buffer async\n");
		vb = (struct vb2_buffer *)fbw->mregion->cookie;
		vb->v4l2_buf.flags = 0;
		vb->v4l2_buf.timestamp = ns_to_timeval(-1);
		vb->v4l2_planes[0].bytesused = 0;

		fbw->inst->vmops.op_buffer_done(
				fbw->inst->vmops.cbdata, rc, vb);
	}

	kfree(fbw);
}

static long venc_fill_outbuf(struct v4l2_subdev *sd, void *arg)
{
	struct fill_buf_work *fbw;
	struct venc_inst *inst = NULL;
	struct mem_region *mregion;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		return -EINVAL;
	} else if (!arg) {
		WFD_MSG_ERR("Invalid output buffer ot fill\n");
		return -EINVAL;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	mregion = get_registered_mregion(&inst->registered_output_bufs, arg);
	if (!mregion) {
		WFD_MSG_ERR("Output buffer not registered\n");
		return -ENOENT;
	}

	fbw = kzalloc(sizeof(*fbw), GFP_KERNEL);
	if (!fbw) {
		WFD_MSG_ERR("Couldn't allocate memory\n");
		return -ENOMEM;
	}

	INIT_WORK(&fbw->work, fill_outbuf_helper);
	fbw->inst = inst;
	fbw->mregion = mregion;
	/* XXX: The need for a wq to qbuf to vidc is necessitated as a
	 * workaround for a bug in the v4l2 framework. VIDIOC_QBUF from
	 * triggers a down_read(current->mm->mmap_sem).  There is another
	 * _read(..) as msm_vidc_qbuf() depends on videobuf2 framework
	 * as well. However, a _write(..) after the first _read() by a
	 * different driver will prevent the second _read(...) from
	 * suceeding.
	 *
	 * As we can't modify the framework, we're working around by issue
	 * by queuing in a different thread effectively.
	 */
	queue_work(inst->fill_buf_wq, &fbw->work);

	return 0;
}

static long venc_encode_frame(struct v4l2_subdev *sd, void *arg)
{
	struct venc_inst *inst = NULL;
	struct venc_buf_info *venc_buf = arg;
	struct mem_region *mregion = NULL;
	struct v4l2_buffer buffer = {0};
	struct v4l2_plane plane = {0};
	int index = 0, rc = 0;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		return -EINVAL;
	} else if (!venc_buf) {
		WFD_MSG_ERR("Invalid output buffer ot fill\n");
		return -EINVAL;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	mregion = venc_buf->mregion;

	plane = (struct v4l2_plane) {
		.length = mregion->size,
		.m.userptr = (u32)mregion->paddr,
		.bytesused = mregion->size,
	};

	while (true) {
		mutex_lock(&inst->lock);
		index = next_free_index(&inst->free_input_indices);
		mutex_unlock(&inst->lock);

		if (index < 0)
			wait_for_completion(&inst->dq_complete);
		else
			break;
	}

	buffer = (struct v4l2_buffer) {
		.index = index,
		.type = BUF_TYPE_INPUT,
		.timestamp = ns_to_timeval(venc_buf->timestamp),
		.memory = V4L2_MEMORY_USERPTR,
		.m.planes = &plane,
		.length = 1,
	};

	WFD_MSG_DBG("Encode buffer %p with index, %d",
		(void *)buffer.m.planes[0].m.userptr, buffer.index);
	rc = msm_vidc_qbuf(inst->vidc_context, &buffer);
	if (!rc) {
		mutex_lock(&inst->lock);
		mark_index_busy(&inst->free_input_indices, index);
		mutex_unlock(&inst->lock);
	}
	return rc;
}

static long venc_alloc_recon_buffers(struct v4l2_subdev *sd, void *arg)
{
	/* vidc driver allocates internally on streamon */
	return 0;
}

static long venc_free_buffer(struct venc_inst *inst, int type,
		struct mem_region *to_free, bool unmap_user_buffer)
{
	struct mem_region *mregion = NULL;
	struct mem_region *buf_list = NULL;

	if (type == BUF_TYPE_OUTPUT) {
		buf_list = &inst->registered_output_bufs;
	} else if (type == BUF_TYPE_INPUT) {
		buf_list = &inst->registered_input_bufs;
	} else {
		WFD_MSG_ERR("Trying to free a buffer of unknown type\n");
		return -EINVAL;
	}
	mregion = get_registered_mregion(buf_list, to_free);

	if (!mregion) {
		WFD_MSG_ERR("Buffer not registered, cannot free\n");
		return -ENOENT;
	}

	if (unmap_user_buffer) {
		int rc = venc_unmap_user_to_kernel(inst, mregion);
		if (rc)
			WFD_MSG_WARN("Unable to unmap user buffer\n");
	}

	list_del(&mregion->list);
	kfree(mregion);
	return 0;
}
static long venc_free_output_buffer(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = NULL;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_free_output_buffer_fail;
	} else if (!arg) {
		WFD_MSG_ERR("Invalid output buffer\n");
		rc = -EINVAL;
		goto venc_free_output_buffer_fail;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	return venc_free_buffer(inst, BUF_TYPE_OUTPUT, arg, true);
venc_free_output_buffer_fail:
	return rc;
}

static long venc_flush_buffers(struct v4l2_subdev *sd, void *arg)
{
	struct venc_inst *inst = NULL;
	struct v4l2_encoder_cmd enc_cmd = {0};
	int rc = 0;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_flush_buffers_fail;
	}

	inst = (struct venc_inst *)sd->dev_priv;

	flush_workqueue(inst->fill_buf_wq);

	enc_cmd.cmd = V4L2_ENC_QCOM_CMD_FLUSH;
	enc_cmd.flags = V4L2_QCOM_CMD_FLUSH_OUTPUT |
		V4L2_QCOM_CMD_FLUSH_CAPTURE;
	msm_vidc_encoder_cmd(inst->vidc_context, &enc_cmd);

	wait_for_completion(&inst->cmd_complete);
venc_flush_buffers_fail:
	return rc;
}

static long venc_free_input_buffer(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = NULL;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_free_input_buffer_fail;
	} else if (!arg) {
		WFD_MSG_ERR("Invalid output buffer\n");
		rc = -EINVAL;
		goto venc_free_input_buffer_fail;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	return venc_free_buffer(inst, BUF_TYPE_INPUT, arg, false);
venc_free_input_buffer_fail:
	return rc;
}

static long venc_free_recon_buffers(struct v4l2_subdev *sd, void *arg)
{
	/* vidc driver takes care of this */
	return 0;
}

static long venc_set_property(struct v4l2_subdev *sd, void *arg)
{
	struct venc_inst *inst = NULL;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		return -EINVAL;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	return msm_vidc_s_ctrl(inst->vidc_context, (struct v4l2_control *)arg);
}

static long venc_get_property(struct v4l2_subdev *sd, void *arg)
{
	struct venc_inst *inst = NULL;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		return -EINVAL;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	return msm_vidc_g_ctrl(inst->vidc_context, (struct v4l2_control *)arg);
}

long venc_mmap(struct v4l2_subdev *sd, void *arg)
{
	struct mem_region_map *mmap = arg;
	struct mem_region *mregion = NULL;
	unsigned long size = 0, align_req = 0, flags = 0;
	int domain = 0, partition = 0, rc = 0;
	void *paddr = NULL;
	struct venc_inst *inst = NULL;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		return -EINVAL;
	} else if (!mmap || !mmap->mregion) {
		WFD_MSG_ERR("Memregion required for %s\n", __func__);
		return -EINVAL;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	mregion = mmap->mregion;

	align_req = inst->secure ? SZ_1M : SZ_4K;
	if (mregion->size % align_req != 0) {
		WFD_MSG_ERR("Memregion not aligned to %ld\n", align_req);
		rc = -EINVAL;
		goto venc_map_bad_align;
	}

	rc = ion_handle_get_flags(mmap->ion_client, mregion->ion_handle,
			&flags);
	if (rc) {
		WFD_MSG_ERR("Failed to get ion flags %d\n", rc);
		goto venc_map_bad_align;
	}

	if (inst->secure) {
		rc = msm_ion_secure_buffer(mmap->ion_client,
				mregion->ion_handle, VIDEO_PIXEL, 0);
		if (rc) {
			WFD_MSG_ERR("Failed to secure input buffer\n");
			goto venc_map_bad_align;
		}
	}

	rc = msm_vidc_get_iommu_domain_partition(inst->vidc_context,
			flags, BUF_TYPE_INPUT, &domain, &partition);
	if (rc) {
		WFD_MSG_ERR("Failed to get domain for output buffer\n");
		goto venc_map_domain_fail;
	}

	rc = ion_map_iommu(mmap->ion_client, mregion->ion_handle,
			domain, partition, align_req, 0,
			(unsigned long *)&paddr, &size, 0, 0);
	if (rc) {
		WFD_MSG_ERR("Failed to get physical addr %d\n", rc);
		paddr = NULL;
		goto venc_map_bad_align;
	} else if (size < mregion->size) {
		WFD_MSG_ERR("Failed to map enough memory\n");
		rc = -ENOMEM;
		goto venc_map_iommu_size_fail;
	}

	mregion->paddr = paddr;
	return rc;

venc_map_iommu_size_fail:
	ion_unmap_iommu(venc_ion_client, mregion->ion_handle,
			domain, partition);
venc_map_domain_fail:
	if (inst->secure)
		msm_ion_unsecure_buffer(mmap->ion_client, mregion->ion_handle);
venc_map_bad_align:
	return rc;
}

long venc_munmap(struct v4l2_subdev *sd, void *arg)
{
	struct mem_region_map *mmap = arg;
	struct mem_region *mregion = NULL;
	struct venc_inst *inst = NULL;
	unsigned long flags = 0;
	int domain = 0, partition = 0, rc = 0;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		return -EINVAL;
	} else if (!mmap || !mmap->mregion) {
		WFD_MSG_ERR("Memregion required for %s\n", __func__);
		return -EINVAL;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	mregion = mmap->mregion;

	rc = ion_handle_get_flags(mmap->ion_client,
			mregion->ion_handle, &flags);
	if (rc) {
		WFD_MSG_ERR("Failed to get ion flags %d\n", rc);
		return rc;
	}

	rc = msm_vidc_get_iommu_domain_partition(inst->vidc_context,
		flags, BUF_TYPE_INPUT, &domain, &partition);
	if (rc) {
		WFD_MSG_ERR("Failed to get domain for input buffer\n");
		return rc;
	}

	if (mregion->paddr) {
		ion_unmap_iommu(mmap->ion_client, mregion->ion_handle,
			domain, partition);
		mregion->paddr = NULL;
	}

	if (inst->secure)
		msm_ion_unsecure_buffer(mmap->ion_client, mregion->ion_handle);

	return rc;
}

static long venc_set_framerate_mode(struct v4l2_subdev *sd,
				void *arg)
{
	/* TODO: Unsupported for now, but return false success
	 * to preserve binary compatibility for userspace apps
	 * across targets */
	return 0;
}

static long venc_secure(struct v4l2_subdev *sd)
{
	struct venc_inst *inst = NULL;
	struct v4l2_control ctrl;
	int rc = 0;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		return -EINVAL;
	}

	inst = sd->dev_priv;

	if (!list_empty(&inst->registered_input_bufs.list) ||
		!list_empty(&inst->registered_output_bufs.list)) {
		WFD_MSG_ERR(
			"Attempt to (un)secure encoder not allowed after registering buffers"
			);
		rc = -EEXIST;
	}

	ctrl.id = V4L2_CID_MPEG_VIDC_VIDEO_SECURE;
	rc = msm_vidc_s_ctrl(inst->vidc_context, &ctrl);
	if (rc) {
		WFD_MSG_ERR("Failed to move vidc into secure mode\n");
		goto secure_fail;
	}

secure_fail:
	return rc;
}

long venc_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	long rc = 0;
	switch (cmd) {
	case OPEN:
		rc = venc_open(sd, arg);
		break;
	case CLOSE:
		rc = venc_close(sd, arg);
		break;
	case ENCODE_START:
		rc = venc_start(sd);
		break;
	case ENCODE_FRAME:
		venc_encode_frame(sd, arg);
		break;
	case ENCODE_STOP:
		rc = venc_stop(sd);
		break;
	case SET_PROP:
		rc = venc_set_property(sd, arg);
		break;
	case GET_PROP:
		rc = venc_get_property(sd, arg);
		break;
	case GET_BUFFER_REQ:
		rc = venc_get_buffer_req(sd, arg);
		break;
	case SET_BUFFER_REQ:
		rc = venc_set_buffer_req(sd, arg);
		break;
	case FREE_BUFFER:
		break;
	case FILL_OUTPUT_BUFFER:
		rc = venc_fill_outbuf(sd, arg);
		break;
	case SET_FORMAT:
		rc = venc_set_format(sd, arg);
		break;
	case SET_FRAMERATE:
		rc = venc_set_framerate(sd, arg);
		break;
	case SET_INPUT_BUFFER:
		rc = venc_set_input_buffer(sd, arg);
		break;
	case SET_OUTPUT_BUFFER:
		rc = venc_set_output_buffer(sd, arg);
		break;
	case ALLOC_RECON_BUFFERS:
		rc = venc_alloc_recon_buffers(sd, arg);
		break;
	case FREE_OUTPUT_BUFFER:
		rc = venc_free_output_buffer(sd, arg);
		break;
	case FREE_INPUT_BUFFER:
		rc = venc_free_input_buffer(sd, arg);
		break;
	case FREE_RECON_BUFFERS:
		rc = venc_free_recon_buffers(sd, arg);
		break;
	case ENCODE_FLUSH:
		rc = venc_flush_buffers(sd, arg);
		break;
	case ENC_MMAP:
		rc = venc_mmap(sd, arg);
		break;
	case ENC_MUNMAP:
		rc = venc_munmap(sd, arg);
		break;
	case SET_FRAMERATE_MODE:
		rc = venc_set_framerate_mode(sd, arg);
		break;
	default:
		WFD_MSG_ERR("Unknown ioctl %d to enc-subdev\n", cmd);
		rc = -ENOTSUPP;
		break;
	}
	return rc;
}
