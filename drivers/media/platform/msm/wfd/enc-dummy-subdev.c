/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/list.h>
#include <linux/mutex.h>
#include <media/msm_media_info.h>
#include <media/v4l2-subdev.h>
#include "enc-subdev.h"
#include "wfd-util.h"

#ifndef CONFIG_MSM_WFD_DEBUG
#error "Dummy subdevice must only be used when CONFIG_MSM_WFD_DEBUG=y"
#endif

#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080
#define ALLOC_SIZE ALIGN((DEFAULT_WIDTH * DEFAULT_HEIGHT * 3) / 2, SZ_1M)
#define FILL_SIZE ((DEFAULT_WIDTH * DEFAULT_HEIGHT * 3) / 2)

static struct ion_client *venc_ion_client;
struct venc_inst {
	struct mutex lock;
	struct venc_msg_ops vmops;
	struct mem_region output_bufs, input_bufs;
	struct workqueue_struct *wq;
};

struct encode_task {
	struct venc_inst *inst;
	struct work_struct work;
};

int venc_load_fw(struct v4l2_subdev *sd)
{
	return 0;
}

int venc_init(struct v4l2_subdev *sd, u32 val)
{
	if (!venc_ion_client)
		venc_ion_client = msm_ion_client_create(-1, "wfd_enc_subdev");

	return venc_ion_client ? 0 : -ENOMEM;
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

	INIT_LIST_HEAD(&inst->output_bufs.list);
	INIT_LIST_HEAD(&inst->input_bufs.list);
	mutex_init(&inst->lock);
	inst->wq = create_workqueue("venc-dummy-subdev");
	inst->vmops = *vmops;
	sd->dev_priv = inst;
	vmops->cookie = inst;
	return 0;
venc_open_fail:
	return rc;
}

static long venc_close(struct v4l2_subdev *sd, void *arg)
{
	struct venc_inst *inst = NULL;
	int rc = 0;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_close_fail;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	destroy_workqueue(inst->wq);
	kfree(inst);
	sd->dev_priv = inst = NULL;
venc_close_fail:
	return rc;

}

static long venc_get_buffer_req(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct bufreq *bufreq = arg;
	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_buf_req_fail;
	} else if (!arg) {
		WFD_MSG_ERR("Invalid buffer requirements\n");
		rc = -EINVAL;
		goto venc_buf_req_fail;
	}


	bufreq->count = 3;
	bufreq->size = ALLOC_SIZE;
venc_buf_req_fail:
	return rc;
}

static long venc_set_buffer_req(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct bufreq *bufreq = arg;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		rc = -EINVAL;
		goto venc_buf_req_fail;
	} else if (!arg) {
		WFD_MSG_ERR("Invalid buffer requirements\n");
		rc = -EINVAL;
		goto venc_buf_req_fail;
	}

	bufreq->size = ALLOC_SIZE;
venc_buf_req_fail:
	return rc;

}

static long venc_start(struct v4l2_subdev *sd)
{
	return 0;
}

static long venc_stop(struct v4l2_subdev *sd)
{
	struct venc_inst *inst = NULL;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		return -EINVAL;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	flush_workqueue(inst->wq);
	return 0;
}

static long venc_set_input_buffer(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static long venc_set_output_buffer(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static long venc_set_format(struct v4l2_subdev *sd, void *arg)
{
	struct venc_inst *inst = NULL;
	struct v4l2_format *fmt = arg;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		return -EINVAL;
	} else if (!fmt) {
		WFD_MSG_ERR("Invalid format\n");
		return -EINVAL;
	} else if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		WFD_MSG_ERR("Invalid buffer type %d\n", fmt->type);
		return -ENOTSUPP;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	fmt->fmt.pix.sizeimage = ALLOC_SIZE;
	return 0;
}

static long venc_set_framerate(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static int venc_map_user_to_kernel(struct venc_inst *inst,
		struct mem_region *mregion)
{
	int rc = 0;
	unsigned long flags = 0;
	if (!mregion) {
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

	mregion->paddr = mregion->kvaddr = ion_map_kernel(venc_ion_client,
				mregion->ion_handle);

	if (IS_ERR_OR_NULL(mregion->kvaddr)) {
		WFD_MSG_ERR("Failed to map buffer into kernel\n");
		rc = PTR_ERR(mregion->kvaddr);
		mregion->kvaddr = NULL;
		goto venc_map_fail;
	}

venc_map_fail:
	return rc;
}

static int venc_unmap_user_to_kernel(struct venc_inst *inst,
		struct mem_region *mregion)
{
	if (!mregion || !mregion->ion_handle)
		return 0;

	if (mregion->kvaddr) {
		ion_unmap_kernel(venc_ion_client, mregion->ion_handle);
		mregion->kvaddr = NULL;
	}


	return 0;
}

#ifdef CONFIG_MSM_VIDC_V4L2
static int encode_memcpy(uint8_t *dst, uint8_t *src, int size)
{
	int y_stride = VENUS_Y_STRIDE(COLOR_FMT_NV12, DEFAULT_WIDTH),
	    y_scan = VENUS_Y_SCANLINES(COLOR_FMT_NV12, DEFAULT_HEIGHT),
	    uv_stride = VENUS_UV_STRIDE(COLOR_FMT_NV12, DEFAULT_WIDTH),
	    uv_scan = VENUS_UV_SCANLINES(COLOR_FMT_NV12, DEFAULT_HEIGHT);
	int y_size = y_stride * y_scan;
	int c = 0, dst_offset = 0, src_offset = 0;

	/* copy the luma */
	for (c = 0; c < DEFAULT_HEIGHT; ++c) {
		memcpy(dst + dst_offset, src + src_offset, DEFAULT_WIDTH);
		src_offset += y_stride;
		dst_offset += DEFAULT_WIDTH;
	}

	/* skip over padding between luma and chroma */
	src_offset = y_size;
	/* now do the chroma */
	for (c = 0; c < DEFAULT_HEIGHT / 2; ++c) {
		memcpy(dst + dst_offset, src + src_offset, DEFAULT_WIDTH);
		src_offset += uv_stride;
		dst_offset += DEFAULT_WIDTH;
	}

	(void)uv_scan;
	return dst_offset;
}
#else
static int encode_memcpy(uint8_t *dst, uint8_t *src, int size)
{
	memcpy(dst, src, size);
	return size;
}
#endif

static void encode(struct work_struct *work)
{
	struct encode_task *et = NULL;
	struct venc_inst *inst = NULL;
	struct mem_region *input, *output;
	struct vb2_buffer *vb;
	int bytes_copied = 0;
	if (!work)
		return;

	et = container_of(work, struct encode_task, work);
	inst = et->inst;

	mutex_lock(&inst->lock);
	if (list_empty(&inst->input_bufs.list)) {
		WFD_MSG_ERR("Can't find an input buffer");
		mutex_unlock(&inst->lock);
		return;
	}

	if (list_empty(&inst->output_bufs.list)) {
		WFD_MSG_ERR("Can't find an output buffer");
		mutex_unlock(&inst->lock);
		return;
	}

	/* Grab an i/p & o/p buffer pair */
	input = list_first_entry(&inst->input_bufs.list,
			struct mem_region, list);
	list_del(&input->list);

	output = list_first_entry(&inst->output_bufs.list,
			struct mem_region, list);
	list_del(&output->list);
	mutex_unlock(&inst->lock);

	/* This is our "encode" */
	bytes_copied = encode_memcpy(output->kvaddr, input->kvaddr,
			min(output->size, input->size));

	vb = (struct vb2_buffer *)output->cookie;
	vb->v4l2_planes[0].bytesused = bytes_copied;
	inst->vmops.op_buffer_done(
			inst->vmops.cbdata, 0, vb);

	inst->vmops.ip_buffer_done(
			inst->vmops.cbdata,
			0, input);

	venc_unmap_user_to_kernel(inst, output);
	kfree(input);
	kfree(output);
	kfree(et);
}

static void prepare_for_encode(struct venc_inst *inst)
{
	struct encode_task *et = kzalloc(sizeof(*et), GFP_KERNEL);
	et->inst = inst;
	INIT_WORK(&et->work, encode);
	queue_work(inst->wq, &et->work);
}

static long venc_fill_outbuf(struct v4l2_subdev *sd, void *arg)
{
	struct venc_inst *inst = NULL;
	struct mem_region *mregion = NULL;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		return -EINVAL;
	} else if (!arg) {
		WFD_MSG_ERR("Invalid output buffer in %s", __func__);
		return -EINVAL;
	}

	inst = (struct venc_inst *)sd->dev_priv;

	/* check for dupes */
	list_for_each_entry(mregion, &inst->output_bufs.list, list) {
		struct mem_region *temp = arg;
		if (mem_region_equals(temp, mregion)) {
			WFD_MSG_ERR("Attempt to queue dupe buffer\n");
			return -EEXIST;
		}
	}

	mregion = kzalloc(sizeof(*mregion), GFP_KERNEL);
	if (!mregion) {
		WFD_MSG_ERR("Invalid output buffer in %s", __func__);
		return -EINVAL;
	}

	*mregion = *(struct mem_region *)arg;

	if (venc_map_user_to_kernel(inst, mregion)) {
		WFD_MSG_ERR("unable to map buffer into kernel\n");
		return -EFAULT;
	}

	mutex_lock(&inst->lock);
	list_add_tail(&mregion->list, &inst->output_bufs.list);

	if (!list_empty(&inst->input_bufs.list))
		prepare_for_encode(inst);
	mutex_unlock(&inst->lock);

	return 0;
}

static long venc_encode_frame(struct v4l2_subdev *sd, void *arg)
{
	struct venc_inst *inst = NULL;
	struct venc_buf_info *venc_buf = arg;
	struct mem_region *mregion = NULL;

	if (!sd) {
		WFD_MSG_ERR("Subdevice required for %s\n", __func__);
		return -EINVAL;
	} else if (!venc_buf) {
		WFD_MSG_ERR("Invalid output buffer ot fill\n");
		return -EINVAL;
	}

	mregion = kzalloc(sizeof(*mregion), GFP_KERNEL);
	if (!mregion) {
		WFD_MSG_ERR("Invalid output buffer in %s", __func__);
		return -EINVAL;
	}

	inst = (struct venc_inst *)sd->dev_priv;
	*mregion = *venc_buf->mregion;

	mutex_lock(&inst->lock);
	list_add_tail(&mregion->list, &inst->input_bufs.list);

	if (!list_empty(&inst->output_bufs.list))
		prepare_for_encode(inst);
	mutex_unlock(&inst->lock);
	return 0;
}

static long venc_alloc_recon_buffers(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static long venc_free_output_buffer(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static long venc_flush_buffers(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static long venc_free_input_buffer(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static long venc_free_recon_buffers(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static long venc_set_property(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static long venc_get_property(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static long venc_mmap(struct v4l2_subdev *sd, void *arg)
{
	struct mem_region_map *mmap = arg;
	struct mem_region *mregion;

	mregion = mmap->mregion;
	mregion->paddr = mregion->kvaddr;
	return 0;
}

static long venc_munmap(struct v4l2_subdev *sd, void *arg)
{
	return 0;
}

static long venc_set_framerate_mode(struct v4l2_subdev *sd,
				void *arg)
{
	return 0;
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
