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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <mach/board.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <media/msm_vidc.h>
#include "msm_vidc_internal.h"
#include "msm_vidc_debug.h"
#include "vidc_hfi_api.h"
#include "msm_smem.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_resources.h"

#define BASE_DEVICE_NUMBER 32

struct msm_vidc_drv *vidc_driver;

struct buffer_info {
	struct list_head list;
	int type;
	int num_planes;
	int fd[VIDEO_MAX_PLANES];
	int buff_off[VIDEO_MAX_PLANES];
	int size[VIDEO_MAX_PLANES];
	u32 uvaddr[VIDEO_MAX_PLANES];
	u32 device_addr[VIDEO_MAX_PLANES];
	struct msm_smem *handle[VIDEO_MAX_PLANES];
};

struct msm_v4l2_vid_inst {
	struct msm_vidc_inst *vidc_inst;
	void *mem_client;
	struct list_head registered_bufs;
};

struct master_slave {
	int masters_ocmem[2];
	int masters_ddr[2];
	int slaves_ocmem[2];
	int slaves_ddr[2];
};

struct bus_pdata_config {
	int *masters;
	int *slaves;
	char *name;
};

static struct master_slave bus_vectors_masters_slaves = {
	.masters_ocmem = {MSM_BUS_MASTER_VIDEO_P0_OCMEM,
				MSM_BUS_MASTER_VIDEO_P1_OCMEM},
	.masters_ddr = {MSM_BUS_MASTER_VIDEO_P0, MSM_BUS_MASTER_VIDEO_P1},
	.slaves_ocmem = {MSM_BUS_SLAVE_OCMEM, MSM_BUS_SLAVE_OCMEM},
	.slaves_ddr = {MSM_BUS_SLAVE_EBI_CH0, MSM_BUS_SLAVE_EBI_CH0},
};

static struct bus_pdata_config bus_pdata_config_vector[] = {
{
	.masters = bus_vectors_masters_slaves.masters_ocmem,
	.slaves = bus_vectors_masters_slaves.slaves_ocmem,
	.name = "qcom,enc-ocmem-ab-ib",
},
{
	.masters = bus_vectors_masters_slaves.masters_ocmem,
	.slaves = bus_vectors_masters_slaves.slaves_ocmem,
	.name = "qcom,dec-ocmem-ab-ib",
},
{
	.masters = bus_vectors_masters_slaves.masters_ddr,
	.slaves = bus_vectors_masters_slaves.slaves_ddr,
	.name = "qcom,enc-ddr-ab-ib",
},
{
	.masters = bus_vectors_masters_slaves.masters_ddr,
	.slaves = bus_vectors_masters_slaves.slaves_ddr,
	.name = "qcom,dec-ddr-ab-ib",
},
};

static inline struct msm_vidc_inst *get_vidc_inst(struct file *filp, void *fh)
{
	return container_of(filp->private_data,
					struct msm_vidc_inst, event_handler);
}

static inline struct msm_v4l2_vid_inst *get_v4l2_inst(struct file *filp,
			void *fh)
{
	struct msm_vidc_inst *vidc_inst;
	vidc_inst = container_of(filp->private_data,
			struct msm_vidc_inst, event_handler);
	return (struct msm_v4l2_vid_inst *)vidc_inst->priv;
}

struct buffer_info *get_registered_buf(struct list_head *list,
				int fd, u32 buff_off, u32 size, int *plane)
{
	struct buffer_info *temp;
	struct buffer_info *ret = NULL;
	int i;
	if (!list || fd < 0 || !plane) {
		dprintk(VIDC_ERR, "Invalid input\n");
		goto err_invalid_input;
	}
	*plane = 0;
	if (!list_empty(list)) {
		list_for_each_entry(temp, list, list) {
			for (i = 0; (i < temp->num_planes)
				&& (i < VIDEO_MAX_PLANES); i++) {
				if (temp && temp->fd[i] == fd &&
						(CONTAINS(temp->buff_off[i],
						temp->size[i], buff_off)
						 || CONTAINS(buff_off,
						 size, temp->buff_off[i])
						 || OVERLAPS(buff_off, size,
						 temp->buff_off[i],
						 temp->size[i]))) {
					dprintk(VIDC_DBG,
							"This memory region is already mapped\n");
					ret = temp;
					*plane = i;
					break;
				}
			}
			if (ret)
				break;
		}
	}
err_invalid_input:
	return ret;
}

struct buffer_info *get_same_fd_buffer(struct list_head *list,
		int fd, int *plane)
{
	struct buffer_info *temp;
	struct buffer_info *ret = NULL;
	int i;
	if (!list || fd < 0 || !plane) {
		dprintk(VIDC_ERR, "Invalid input\n");
		goto err_invalid_input;
	}
	*plane = 0;
	if (!list_empty(list)) {
		list_for_each_entry(temp, list, list) {
			for (i = 0; (i < temp->num_planes)
				&& (i < VIDEO_MAX_PLANES); i++) {
				if (temp && temp->fd[i] == fd)  {
					dprintk(VIDC_INFO,
					"Found same fd buffer\n");
					ret = temp;
					*plane = i;
					break;
				}
			}
			if (ret)
				break;
		}
	}
err_invalid_input:
	return ret;
}

static struct buffer_info *device_to_uvaddr(
	struct list_head *list, u32 device_addr)
{
	struct buffer_info *temp = NULL;
	int found = 0;
	int i;
	if (!list || !device_addr) {
		dprintk(VIDC_ERR, "Invalid input\n");
		goto err_invalid_input;
	}
	if (!list_empty(list)) {
		list_for_each_entry(temp, list, list) {
			for (i = 0; (i < temp->num_planes)
				&& (i < VIDEO_MAX_PLANES); i++) {
				if (temp && temp->device_addr[i]
						== device_addr)  {
					dprintk(VIDC_INFO,
					"Found same fd buffer\n");
					found = 1;
					break;
				}
			}
			if (found)
				break;
		}
	}
err_invalid_input:
	return temp;
}

static int msm_v4l2_open(struct file *filp)
{
	int rc = 0;
	struct video_device *vdev = video_devdata(filp);
	struct msm_video_device *vid_dev =
		container_of(vdev, struct msm_video_device, vdev);
	struct msm_vidc_core *core = video_drvdata(filp);
	struct msm_v4l2_vid_inst *v4l2_inst = kzalloc(sizeof(*v4l2_inst),
						GFP_KERNEL);
	if (!v4l2_inst) {
		dprintk(VIDC_ERR,
			"Failed to allocate memory for this instance\n");
		rc = -ENOMEM;
		goto fail_nomem;
	}
	v4l2_inst->mem_client = msm_smem_new_client(SMEM_ION, &core->resources);
	if (!v4l2_inst->mem_client) {
		dprintk(VIDC_ERR, "Failed to create memory client\n");
		rc = -ENOMEM;
		goto fail_mem_client;
	}

	v4l2_inst->vidc_inst = msm_vidc_open(core->id, vid_dev->type);
	if (!v4l2_inst->vidc_inst) {
		dprintk(VIDC_ERR,
		"Failed to create video instance, core: %d, type = %d\n",
		core->id, vid_dev->type);
		rc = -ENOMEM;
		goto fail_open;
	}
	INIT_LIST_HEAD(&v4l2_inst->registered_bufs);
	v4l2_inst->vidc_inst->priv = v4l2_inst;
	clear_bit(V4L2_FL_USES_V4L2_FH, &vdev->flags);
	filp->private_data = &(v4l2_inst->vidc_inst->event_handler);
	return rc;
fail_open:
	msm_smem_delete_client(v4l2_inst->mem_client);
fail_mem_client:
	kfree(v4l2_inst);
fail_nomem:
	return rc;
}
static int msm_v4l2_release_output_buffers(struct msm_v4l2_vid_inst *v4l2_inst)
{
	struct list_head *ptr, *next;
	struct buffer_info *bi;
	struct v4l2_buffer buffer_info;
	struct v4l2_plane plane[VIDEO_MAX_PLANES];
	int rc = 0;
	int i;
	list_for_each_safe(ptr, next, &v4l2_inst->registered_bufs) {
		bi = list_entry(ptr, struct buffer_info, list);
		if (bi->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			buffer_info.type = bi->type;
			for (i = 0; (i < bi->num_planes)
				&& (i < VIDEO_MAX_PLANES); i++) {
				plane[i].reserved[0] = bi->fd[i];
				plane[i].reserved[1] = bi->buff_off[i];
				plane[i].length = bi->size[i];
				plane[i].m.userptr = bi->device_addr[i];
				buffer_info.m.planes = plane;
				dprintk(VIDC_DBG,
					"Releasing buffer: %d, %d, %d\n",
					buffer_info.m.planes[i].reserved[0],
					buffer_info.m.planes[i].reserved[1],
					buffer_info.m.planes[i].length);
			}
			buffer_info.length = bi->num_planes;
			rc = msm_vidc_release_buf(v4l2_inst->vidc_inst,
					&buffer_info);
			if (rc)
				dprintk(VIDC_ERR,
					"Failed Release buffer: %d, %d, %d\n",
					buffer_info.m.planes[0].reserved[0],
					buffer_info.m.planes[0].reserved[1],
					buffer_info.m.planes[0].length);
			list_del(&bi->list);
			for (i = 0; i < bi->num_planes; i++) {
				if (bi->handle[i])
					msm_smem_free(v4l2_inst->mem_client,
							bi->handle[i]);
			}
			kfree(bi);
		}
	}
	return rc;
}

static int msm_v4l2_close(struct file *filp)
{
	int rc = 0;
	struct list_head *ptr, *next;
	struct buffer_info *bi;
	struct msm_vidc_inst *vidc_inst;
	struct msm_v4l2_vid_inst *v4l2_inst;
	int i;
	vidc_inst = get_vidc_inst(filp, NULL);
	v4l2_inst = get_v4l2_inst(filp, NULL);
	rc = msm_v4l2_release_output_buffers(v4l2_inst);
	if (rc)
		dprintk(VIDC_WARN,
			"Failed in %s for release output buffers\n", __func__);
	list_for_each_safe(ptr, next, &v4l2_inst->registered_bufs) {
		bi = list_entry(ptr, struct buffer_info, list);
		if (bi->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			list_del(&bi->list);
			for (i = 0; (i < bi->num_planes)
				&& (i < VIDEO_MAX_PLANES); i++) {
				if (bi->handle[i])
					msm_smem_free(v4l2_inst->mem_client,
							bi->handle[i]);
			}
			kfree(bi);
		}
	}
	msm_smem_delete_client(v4l2_inst->mem_client);
	rc = msm_vidc_close(vidc_inst);
	kfree(v4l2_inst);
	return rc;
}

static int msm_v4l2_querycap(struct file *filp, void *fh,
			struct v4l2_capability *cap)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(filp, fh);
	return msm_vidc_querycap((void *)vidc_inst, cap);
}

int msm_v4l2_enum_fmt(struct file *file, void *fh,
					struct v4l2_fmtdesc *f)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_enum_fmt((void *)vidc_inst, f);
}

int msm_v4l2_s_fmt(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_s_fmt((void *)vidc_inst, f);
}

int msm_v4l2_g_fmt(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_g_fmt((void *)vidc_inst, f);
}

int msm_v4l2_s_ctrl(struct file *file, void *fh,
					struct v4l2_control *a)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_s_ctrl((void *)vidc_inst, a);
}

int msm_v4l2_g_ctrl(struct file *file, void *fh,
					struct v4l2_control *a)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_g_ctrl((void *)vidc_inst, a);
}

int msm_v4l2_reqbufs(struct file *file, void *fh,
				struct v4l2_requestbuffers *b)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	struct msm_v4l2_vid_inst *v4l2_inst;
	int rc = 0;
	v4l2_inst = get_v4l2_inst(file, NULL);
	if (b->count == 0)
		rc = msm_v4l2_release_output_buffers(v4l2_inst);
	if (rc)
		dprintk(VIDC_WARN,
			"Failed in %s for release output buffers\n", __func__);
	return msm_vidc_reqbufs((void *)vidc_inst, b);
}

int msm_v4l2_prepare_buf(struct file *file, void *fh,
				struct v4l2_buffer *b)
{
	struct msm_smem *handle = NULL;
	struct buffer_info *binfo;
	struct buffer_info *temp;
	struct msm_vidc_inst *vidc_inst;
	struct msm_v4l2_vid_inst *v4l2_inst;
	int plane = 0;
	int i, rc = 0;
	struct hfi_device *hdev;
	enum hal_buffer buffer_type;

	vidc_inst = get_vidc_inst(file, fh);
	v4l2_inst = get_v4l2_inst(file, fh);
	if (!v4l2_inst || !vidc_inst || !vidc_inst->core
		|| !vidc_inst->core->device) {
		rc = -EINVAL;
		goto exit;
	}

	hdev = vidc_inst->core->device;

	if (!v4l2_inst->mem_client) {
		dprintk(VIDC_ERR, "Failed to get memory client\n");
		rc = -ENOMEM;
		goto exit;
	}
	binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
	if (!binfo) {
		dprintk(VIDC_ERR, "Out of memory\n");
		rc = -ENOMEM;
		goto exit;
	}
	if (b->length > VIDEO_MAX_PLANES) {
		dprintk(VIDC_ERR, "Num planes exceeds max: %d, %d\n",
			b->length, VIDEO_MAX_PLANES);
		rc = -EINVAL;
		goto exit;
	}
	for (i = 0; i < b->length; ++i) {
		buffer_type = HAL_BUFFER_OUTPUT;
		if (EXTRADATA_IDX(b->length) &&
			(i == EXTRADATA_IDX(b->length)) &&
			!b->m.planes[i].length) {
			continue;
		}
		temp = get_registered_buf(&v4l2_inst->registered_bufs,
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length, &plane);
		if (temp) {
			dprintk(VIDC_DBG,
				"This memory region has already been prepared\n");
			rc = -EINVAL;
			kfree(binfo);
			goto exit;
		}
		if (b->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			buffer_type = HAL_BUFFER_INPUT;

		temp = get_same_fd_buffer(&v4l2_inst->registered_bufs,
				b->m.planes[i].reserved[0], &plane);

		if (temp) {
			binfo->type = b->type;
			binfo->fd[i] = b->m.planes[i].reserved[0];
			binfo->buff_off[i] = b->m.planes[i].reserved[1];
			binfo->size[i] = b->m.planes[i].length;
			binfo->uvaddr[i] = b->m.planes[i].m.userptr;
			binfo->device_addr[i] =
			temp->handle[plane]->device_addr + binfo->buff_off[i];
			binfo->handle[i] = NULL;
		} else {
			handle = msm_smem_user_to_kernel(v4l2_inst->mem_client,
					b->m.planes[i].reserved[0],
					b->m.planes[i].reserved[1],
					buffer_type);
			if (!handle) {
				dprintk(VIDC_ERR,
					"Failed to get device buffer address\n");
				kfree(binfo);
				goto exit;
			}
			binfo->type = b->type;
			binfo->fd[i] = b->m.planes[i].reserved[0];
			binfo->buff_off[i] = b->m.planes[i].reserved[1];
			binfo->size[i] = b->m.planes[i].length;
			binfo->uvaddr[i] = b->m.planes[i].m.userptr;
			binfo->device_addr[i] =
				handle->device_addr + binfo->buff_off[i];
			binfo->handle[i] = handle;
			dprintk(VIDC_DBG, "Registering buffer: %d, %d, %d\n",
					b->m.planes[i].reserved[0],
					b->m.planes[i].reserved[1],
					b->m.planes[i].length);
			rc = msm_smem_cache_operations(v4l2_inst->mem_client,
				binfo->handle[i], SMEM_CACHE_CLEAN);
			if (rc)
				dprintk(VIDC_WARN,
					"CACHE Clean failed: %d, %d, %d\n",
					b->m.planes[i].reserved[0],
					b->m.planes[i].reserved[1],
					b->m.planes[i].length);
		}
		b->m.planes[i].m.userptr = binfo->device_addr[i];
	}
	binfo->num_planes = b->length;
	list_add_tail(&binfo->list, &v4l2_inst->registered_bufs);
	rc = msm_vidc_prepare_buf(v4l2_inst->vidc_inst, b);
exit:
	return rc;
}

int msm_v4l2_qbuf(struct file *file, void *fh,
				struct v4l2_buffer *b)
{
	struct msm_vidc_inst *vidc_inst;
	struct msm_v4l2_vid_inst *v4l2_inst;
	struct buffer_info *binfo;
	int plane = 0;
	int rc = 0;
	int i;
	if (b->length > VIDEO_MAX_PLANES) {
		dprintk(VIDC_ERR, "num planes exceeds max: %d\n",
			b->length);
		return -EINVAL;
	}
	vidc_inst = get_vidc_inst(file, fh);
	v4l2_inst = get_v4l2_inst(file, fh);
	for (i = 0; i < b->length; ++i) {
		if (EXTRADATA_IDX(b->length) &&
			(i == EXTRADATA_IDX(b->length)) &&
			!b->m.planes[i].length) {
			b->m.planes[i].m.userptr = 0;
			continue;
		}
		binfo = get_registered_buf(&v4l2_inst->registered_bufs,
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length, &plane);
		if (!binfo) {
			dprintk(VIDC_ERR,
				"This buffer is not registered: %d, %d, %d\n",
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length);
			rc = -EINVAL;
			goto err_invalid_buff;
		}
		b->m.planes[i].m.userptr = binfo->device_addr[i];
		dprintk(VIDC_DBG, "Queueing device address = 0x%x\n",
				binfo->device_addr[i]);
		if (binfo->handle[i] &&
			(b->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)) {
			rc = msm_smem_cache_operations(v4l2_inst->mem_client,
					binfo->handle[i], SMEM_CACHE_CLEAN);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to clean caches: %d\n", rc);
				goto err_invalid_buff;
			}
		}
	}
	rc = msm_vidc_qbuf(v4l2_inst->vidc_inst, b);
err_invalid_buff:
	return rc;
}

int msm_v4l2_dqbuf(struct file *file, void *fh,
				struct v4l2_buffer *b)
{
	int rc = 0;
	int i;
	struct msm_v4l2_vid_inst *v4l2_inst;
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	struct buffer_info *buffer_info;
	if (b->length > VIDEO_MAX_PLANES) {
		dprintk(VIDC_ERR, "num planes exceed maximum: %d\n",
			b->length);
		return -EINVAL;
	}
	v4l2_inst = get_v4l2_inst(file, fh);
	rc = msm_vidc_dqbuf((void *)vidc_inst, b);
	if (rc) {
		dprintk(VIDC_DBG,
			"Failed to dqbuf, capability: %d, rc: %d\n",
			b->type, rc);
		goto fail_dq_buf;
	}
	for (i = 0; i < b->length; i++) {
		if (EXTRADATA_IDX(b->length) &&
				(i == EXTRADATA_IDX(b->length)) &&
				!b->m.planes[i].m.userptr) {
			continue;
		}
		buffer_info = device_to_uvaddr(
				&v4l2_inst->registered_bufs,
				b->m.planes[i].m.userptr);
		b->m.planes[i].m.userptr = buffer_info->uvaddr[i];
		if (!b->m.planes[i].m.userptr) {
			dprintk(VIDC_ERR,
			"Failed to find user virtual address, 0x%lx, %d, %d\n",
			b->m.planes[i].m.userptr, b->type, i);
			rc = -EINVAL;
			goto fail_dq_buf;
		}
		if (buffer_info->handle[i] &&
			(b->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
			rc = msm_smem_cache_operations(v4l2_inst->mem_client,
				buffer_info->handle[i], SMEM_CACHE_INVALIDATE);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to clean caches: %d\n", rc);
				goto fail_dq_buf;
			}
		}
	}
fail_dq_buf:
	return rc;
}

int msm_v4l2_streamon(struct file *file, void *fh,
				enum v4l2_buf_type i)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_streamon((void *)vidc_inst, i);
}

int msm_v4l2_streamoff(struct file *file, void *fh,
				enum v4l2_buf_type i)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_streamoff((void *)vidc_inst, i);
}

static int msm_v4l2_subscribe_event(struct v4l2_fh *fh,
				struct v4l2_event_subscription *sub)
{
	struct msm_vidc_inst *vidc_inst = container_of(fh,
			struct msm_vidc_inst, event_handler);
	return msm_vidc_subscribe_event((void *)vidc_inst, sub);
}

static int msm_v4l2_unsubscribe_event(struct v4l2_fh *fh,
				struct v4l2_event_subscription *sub)
{
	struct msm_vidc_inst *vidc_inst = container_of(fh,
			struct msm_vidc_inst, event_handler);
	return msm_vidc_unsubscribe_event((void *)vidc_inst, sub);
}

static int msm_v4l2_decoder_cmd(struct file *file, void *fh,
				struct v4l2_decoder_cmd *dec)
{
	struct msm_v4l2_vid_inst *v4l2_inst;
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	int rc = 0;
	v4l2_inst = get_v4l2_inst(file, NULL);
	if (dec->cmd == V4L2_DEC_CMD_STOP)
		rc = msm_v4l2_release_output_buffers(v4l2_inst);
	if (rc)
		dprintk(VIDC_WARN,
			"Failed to release dec output buffers: %d\n", rc);
	return msm_vidc_decoder_cmd((void *)vidc_inst, dec);
}

static int msm_v4l2_encoder_cmd(struct file *file, void *fh,
				struct v4l2_encoder_cmd *enc)
{
	struct msm_v4l2_vid_inst *v4l2_inst;
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	int rc = 0;
	v4l2_inst = get_v4l2_inst(file, NULL);
	if (enc->cmd == V4L2_ENC_CMD_STOP)
		rc = msm_v4l2_release_output_buffers(v4l2_inst);
	if (rc)
		dprintk(VIDC_WARN,
			"Failed to release enc output buffers: %d\n", rc);
	return msm_vidc_encoder_cmd((void *)vidc_inst, enc);
}
static int msm_v4l2_s_parm(struct file *file, void *fh,
			struct v4l2_streamparm *a)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_s_parm((void *)vidc_inst, a);
}
static int msm_v4l2_g_parm(struct file *file, void *fh,
		struct v4l2_streamparm *a)
{
	return 0;
}

static int msm_v4l2_enum_framesizes(struct file *file, void *fh,
				struct v4l2_frmsizeenum *fsize)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_enum_framesizes((void *)vidc_inst, fsize);
}
static const struct v4l2_ioctl_ops msm_v4l2_ioctl_ops = {
	.vidioc_querycap = msm_v4l2_querycap,
	.vidioc_enum_fmt_vid_cap_mplane = msm_v4l2_enum_fmt,
	.vidioc_enum_fmt_vid_out_mplane = msm_v4l2_enum_fmt,
	.vidioc_s_fmt_vid_cap_mplane = msm_v4l2_s_fmt,
	.vidioc_s_fmt_vid_out_mplane = msm_v4l2_s_fmt,
	.vidioc_g_fmt_vid_cap_mplane = msm_v4l2_g_fmt,
	.vidioc_g_fmt_vid_out_mplane = msm_v4l2_g_fmt,
	.vidioc_reqbufs = msm_v4l2_reqbufs,
	.vidioc_prepare_buf = msm_v4l2_prepare_buf,
	.vidioc_qbuf = msm_v4l2_qbuf,
	.vidioc_dqbuf = msm_v4l2_dqbuf,
	.vidioc_streamon = msm_v4l2_streamon,
	.vidioc_streamoff = msm_v4l2_streamoff,
	.vidioc_s_ctrl = msm_v4l2_s_ctrl,
	.vidioc_g_ctrl = msm_v4l2_g_ctrl,
	.vidioc_subscribe_event = msm_v4l2_subscribe_event,
	.vidioc_unsubscribe_event = msm_v4l2_unsubscribe_event,
	.vidioc_decoder_cmd = msm_v4l2_decoder_cmd,
	.vidioc_encoder_cmd = msm_v4l2_encoder_cmd,
	.vidioc_s_parm = msm_v4l2_s_parm,
	.vidioc_g_parm = msm_v4l2_g_parm,
	.vidioc_enum_framesizes = msm_v4l2_enum_framesizes,
};

static const struct v4l2_ioctl_ops msm_v4l2_enc_ioctl_ops = {
};

static unsigned int msm_v4l2_poll(struct file *filp,
	struct poll_table_struct *pt)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(filp, NULL);
	return msm_vidc_poll((void *)vidc_inst, filp, pt);
}

static const struct v4l2_file_operations msm_v4l2_vidc_fops = {
	.owner = THIS_MODULE,
	.open = msm_v4l2_open,
	.release = msm_v4l2_close,
	.ioctl = video_ioctl2,
	.poll = msm_v4l2_poll,
};

void msm_vidc_release_video_device(struct video_device *pvdev)
{
}

static size_t get_u32_array_num_elements(struct platform_device *pdev,
					char *name)
{
	struct device_node *np = pdev->dev.of_node;
	int len;
	size_t num_elements = 0;
	if (!of_get_property(np, name, &len)) {
		dprintk(VIDC_ERR, "Failed to read %s from device tree\n",
			name);
		goto fail_read;
	}

	num_elements = len / sizeof(u32);
	if (num_elements <= 0) {
		dprintk(VIDC_ERR, "%s not specified in device tree\n",
			name);
		goto fail_read;
	}
	return num_elements / 2;

fail_read:
	return 0;
}

static int read_hfi_type(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int rc = 0;
	const char *hfi_name = NULL;

	if (np) {
		rc = of_property_read_string(np, "qcom,hfi", &hfi_name);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to read hfi from device tree\n");
			goto err_hfi_read;
		}
		if (!strcmp(hfi_name, "venus"))
			rc = VIDC_HFI_VENUS;
		else if (!strcmp(hfi_name, "q6"))
			rc = VIDC_HFI_Q6;
		else
			rc = -EINVAL;
	} else
		rc = VIDC_HFI_Q6;

err_hfi_read:
	return rc;
}

static inline void msm_vidc_free_freq_table(
		struct msm_vidc_platform_resources *res)
{
	kfree(res->load_freq_tbl);
	res->load_freq_tbl = NULL;
}

static inline void msm_vidc_free_reg_table(
			struct msm_vidc_platform_resources *res)
{
	kfree(res->reg_set.reg_tbl);
	res->reg_set.reg_tbl = NULL;
}

static inline void msm_vidc_free_bus_vectors(
			struct msm_vidc_platform_resources *res)
{
	int i, j;
	if (res->bus_pdata) {
		for (i = 0; i < ARRAY_SIZE(bus_pdata_config_vector); i++) {
			for (j = 0; j < res->bus_pdata[i].num_usecases; j++) {
				kfree(res->bus_pdata[i].usecase[j].vectors);
				res->bus_pdata[i].usecase[j].vectors = NULL;
			}
			kfree(res->bus_pdata[i].usecase);
			res->bus_pdata[i].usecase = NULL;
		}
		kfree(res->bus_pdata);
		res->bus_pdata = NULL;
	}
}

static inline void msm_vidc_free_iommu_groups(
			struct msm_vidc_platform_resources *res)
{
	kfree(res->iommu_group_set.iommu_maps);
	res->iommu_group_set.iommu_maps = NULL;
}

static inline void msm_vidc_free_buffer_usage_table(
			struct msm_vidc_platform_resources *res)
{
	kfree(res->buffer_usage_set.buffer_usage_tbl);
	res->buffer_usage_set.buffer_usage_tbl = NULL;
}

static int msm_vidc_load_freq_table(struct msm_vidc_platform_resources *res)
{
	int rc = 0;
	int num_elements = 0;
	struct platform_device *pdev = res->pdev;

	num_elements = get_u32_array_num_elements(pdev, "qcom,load-freq-tbl");
	if (num_elements == 0) {
		dprintk(VIDC_ERR, "no elements in frequency table\n");
		return rc;
	}

	res->load_freq_tbl = kzalloc(num_elements * sizeof(*res->load_freq_tbl),
			GFP_KERNEL);
	if (!res->load_freq_tbl) {
		dprintk(VIDC_ERR,
				"%s Failed to alloc load_freq_tbl\n",
				__func__);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(pdev->dev.of_node,
		"qcom,load-freq-tbl", (u32 *)res->load_freq_tbl,
		num_elements * 2)) {
		dprintk(VIDC_ERR, "Failed to read frequency table\n");
		msm_vidc_free_freq_table(res);
		return -EINVAL;
	}

	res->load_freq_tbl_size = num_elements;
	return rc;
}

static int msm_vidc_load_reg_table(struct msm_vidc_platform_resources *res)
{
	struct reg_set *reg_set;
	struct platform_device *pdev = res->pdev;
	int i;
	int rc = 0;

	reg_set = &res->reg_set;
	reg_set->count = get_u32_array_num_elements(pdev, "qcom,reg-presets");
	if (reg_set->count == 0) {
		dprintk(VIDC_DBG, "no elements in reg set\n");
		return rc;
	}

	reg_set->reg_tbl = kzalloc(reg_set->count *
			sizeof(*(reg_set->reg_tbl)), GFP_KERNEL);
	if (!reg_set->reg_tbl) {
		dprintk(VIDC_ERR, "%s Failed to alloc register table\n",
			__func__);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(pdev->dev.of_node, "qcom,reg-presets",
		(u32 *)reg_set->reg_tbl, reg_set->count * 2)) {
		dprintk(VIDC_ERR, "Failed to read register table\n");
		msm_vidc_free_reg_table(res);
		return -EINVAL;
	}
	for (i = 0; i < reg_set->count; i++) {
		dprintk(VIDC_DBG,
			"reg = %x, value = %x\n",
			reg_set->reg_tbl[i].reg,
			reg_set->reg_tbl[i].value
		);
	}
	return rc;
}

static void msm_vidc_free_bus_vector(struct msm_bus_scale_pdata *bus_pdata)
{
	int i;
	for (i = 0; i < bus_pdata->num_usecases; i++) {
		kfree(bus_pdata->usecase[i].vectors);
		bus_pdata->usecase[i].vectors = NULL;
	}

	kfree(bus_pdata->usecase);
	bus_pdata->usecase = NULL;
}

static int msm_vidc_load_bus_vector(struct platform_device *pdev,
			struct msm_bus_scale_pdata *bus_pdata, u32 num_ports,
			struct bus_pdata_config *bus_pdata_config)
{
	struct bus_values {
	    u32 ab;
	    u32 ib;
	};
	struct bus_values *values;
	int i, j;
	int rc = 0;

	values = kzalloc(sizeof(*values) * bus_pdata->num_usecases, GFP_KERNEL);
	if (!values) {
		dprintk(VIDC_ERR, "%s Failed to alloc bus_values\n", __func__);
		rc = -ENOMEM;
		goto err_mem_alloc;
	}

	if (of_property_read_u32_array(pdev->dev.of_node,
		    bus_pdata_config->name, (u32 *)values,
		    bus_pdata->num_usecases * (sizeof(*values)/sizeof(u32)))) {
		dprintk(VIDC_ERR, "%s Failed to read bus values\n", __func__);
		rc = -EINVAL;
		goto err_parse_dt;
	}

	bus_pdata->usecase = kzalloc(sizeof(*bus_pdata->usecase) *
		    bus_pdata->num_usecases, GFP_KERNEL);
	if (!bus_pdata->usecase) {
		dprintk(VIDC_ERR,
			"%s Failed to alloc bus_pdata usecase\n", __func__);
		rc = -ENOMEM;
		goto err_parse_dt;
	}
	bus_pdata->name = bus_pdata_config->name;
	for (i = 0; i < bus_pdata->num_usecases; i++) {
		bus_pdata->usecase[i].vectors = kzalloc(
			sizeof(*bus_pdata->usecase[i].vectors) * num_ports,
			GFP_KERNEL);
		if (!bus_pdata->usecase) {
			dprintk(VIDC_ERR,
				"%s Failed to alloc bus_pdata usecase\n",
				__func__);
			break;
		}
		for (j = 0; j < num_ports; j++) {
			bus_pdata->usecase[i].vectors[j].ab = (u64)values[i].ab
									* 1000;
			bus_pdata->usecase[i].vectors[j].ib = (u64)values[i].ib
									* 1000;
			bus_pdata->usecase[i].vectors[j].src =
						bus_pdata_config->masters[j];
			bus_pdata->usecase[i].vectors[j].dst =
						bus_pdata_config->slaves[j];
			dprintk(VIDC_DBG,
				"ab = %llu, ib = %llu, src = %d, dst = %d\n",
				bus_pdata->usecase[i].vectors[j].ab,
				bus_pdata->usecase[i].vectors[j].ib,
				bus_pdata->usecase[i].vectors[j].src,
				bus_pdata->usecase[i].vectors[j].dst);
		}
		bus_pdata->usecase[i].num_paths = num_ports;
	}
	if (i < bus_pdata->num_usecases) {
		for (--i; i >= 0; i--) {
			kfree(bus_pdata->usecase[i].vectors);
			bus_pdata->usecase[i].vectors = NULL;
		}
		kfree(bus_pdata->usecase);
		bus_pdata->usecase = NULL;
		rc = -EINVAL;
	}
err_parse_dt:
	kfree(values);
err_mem_alloc:
	return rc;
}

static int msm_vidc_load_bus_vectors(struct msm_vidc_platform_resources *res)
{
	u32 num_ports = 0;
	int rc = 0;
	int i;
	struct platform_device *pdev = res->pdev;
	u32 num_bus_pdata = ARRAY_SIZE(bus_pdata_config_vector);

	if (of_property_read_u32_array(pdev->dev.of_node, "qcom,bus-ports",
			(u32 *)&num_ports, 1) || (num_ports == 0))
		goto err_mem_alloc;

	res->bus_pdata = kzalloc(sizeof(*res->bus_pdata) * num_bus_pdata,
				GFP_KERNEL);
	if (!res->bus_pdata) {
		dprintk(VIDC_ERR, "Failed to alloc memory\n");
		rc = -ENOMEM;
		goto err_mem_alloc;
	}
	for (i = 0; i < num_bus_pdata; i++) {
		if (!res->has_ocmem &&
			(!strcmp(bus_pdata_config_vector[i].name,
				"qcom,enc-ocmem-ab-ib")
			|| !strcmp(bus_pdata_config_vector[i].name,
				"qcom,dec-ocmem-ab-ib"))) {
			continue;
		}
		res->bus_pdata[i].num_usecases = get_u32_array_num_elements(
					pdev, bus_pdata_config_vector[i].name);
		if (res->bus_pdata[i].num_usecases == 0) {
			dprintk(VIDC_ERR, "no elements in %s\n",
				bus_pdata_config_vector[i].name);
			rc = -EINVAL;
			break;
		}

		rc = msm_vidc_load_bus_vector(pdev, &res->bus_pdata[i],
				num_ports, &bus_pdata_config_vector[i]);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to load bus vector: %d\n", i);
			break;
		}
	}
	if (i < num_bus_pdata) {
		for (--i; i >= 0; i--)
			msm_vidc_free_bus_vector(&res->bus_pdata[i]);
		kfree(res->bus_pdata);
		res->bus_pdata = NULL;
	}
err_mem_alloc:
	return rc;
}

static int msm_vidc_load_iommu_groups(struct msm_vidc_platform_resources *res)
{
	int rc = 0;
	struct platform_device *pdev = res->pdev;
	struct device_node *ctx_node;
	struct iommu_set *iommu_group_set = &res->iommu_group_set;
	int array_size;
	int i;
	struct iommu_info *iommu_map;
	u32 *buffer_types = NULL;

	if (!of_get_property(pdev->dev.of_node, "qcom,iommu-groups",
				&array_size)) {
		dprintk(VIDC_DBG, "iommu_groups property not present\n");
		iommu_group_set->count = 0;
		return 0;
	}

	iommu_group_set->count = array_size / sizeof(u32);
	if (iommu_group_set->count == 0) {
		dprintk(VIDC_ERR, "No group present in iommu_groups\n");
		rc = -ENOENT;
		goto err_no_of_node;
	}

	iommu_group_set->iommu_maps = kzalloc(iommu_group_set->count *
			sizeof(*(iommu_group_set->iommu_maps)), GFP_KERNEL);
	if (!iommu_group_set->iommu_maps) {
		dprintk(VIDC_ERR, "%s Failed to alloc iommu_maps\n",
			__func__);
		rc = -ENOMEM;
		goto err_no_of_node;
	}

	buffer_types = kzalloc(iommu_group_set->count * sizeof(*buffer_types),
				GFP_KERNEL);
	if (!buffer_types) {
		dprintk(VIDC_ERR,
			"%s Failed to alloc iommu group buffer types\n",
			__func__);
		rc = -ENOMEM;
		goto err_load_groups;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,iommu-group-buffer-types", buffer_types,
			iommu_group_set->count);
	if (rc) {
		dprintk(VIDC_ERR,
		    "%s Failed to read iommu group buffer types\n", __func__);
		goto err_load_groups;
	}

	for (i = 0; i < iommu_group_set->count; i++) {
		iommu_map = &iommu_group_set->iommu_maps[i];
		ctx_node = of_parse_phandle(pdev->dev.of_node,
				"qcom,iommu-groups", i);
		if (!ctx_node) {
			dprintk(VIDC_ERR, "Unable to parse phandle : %u\n", i);
			rc = -EBADHANDLE;
			goto err_load_groups;
		}

		rc = of_property_read_string(ctx_node, "label",
				&(iommu_map->name));
		if (rc) {
			dprintk(VIDC_ERR, "Could not find label property\n");
			goto err_load_groups;
		}

		if (!of_get_property(ctx_node, "qcom,virtual-addr-pool",
				&array_size)) {
			dprintk(VIDC_ERR,
				"Could not find any addr pool for group : %s\n",
				iommu_map->name);
			rc = -EBADHANDLE;
			goto err_load_groups;
		}

		iommu_map->npartitions = array_size / sizeof(u32) / 2;

		rc = of_property_read_u32_array(ctx_node,
				"qcom,virtual-addr-pool",
				(u32 *)iommu_map->addr_range,
				iommu_map->npartitions * 2);
		if (rc) {
			dprintk(VIDC_ERR,
				"Could not read addr pool for group : %s\n",
				iommu_map->name);
			goto err_load_groups;
		}

		iommu_map->buffer_type = buffer_types[i];
		iommu_map->is_secure =
			of_property_read_bool(ctx_node,	"qcom,secure-domain");
	}
	kfree(buffer_types);
	return 0;
err_load_groups:
	kfree(buffer_types);
	msm_vidc_free_iommu_groups(res);
err_no_of_node:
	return rc;
}

static int msm_vidc_load_buffer_usage_table(
		struct msm_vidc_platform_resources *res)
{
	int rc = 0;
	struct platform_device *pdev = res->pdev;
	struct buffer_usage_set *buffer_usage_set = &res->buffer_usage_set;

	buffer_usage_set->count = get_u32_array_num_elements(
				    pdev, "qcom,buffer-type-tz-usage-table");
	if (buffer_usage_set->count == 0) {
		dprintk(VIDC_DBG, "no elements in buffer usage set\n");
		return 0;
	}

	buffer_usage_set->buffer_usage_tbl = kzalloc(buffer_usage_set->count *
			sizeof(*(buffer_usage_set->buffer_usage_tbl)),
			GFP_KERNEL);
	if (!buffer_usage_set->buffer_usage_tbl) {
		dprintk(VIDC_ERR, "%s Failed to alloc buffer usage table\n",
			__func__);
		rc = -ENOMEM;
		goto err_load_buf_usage;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node,
		    "qcom,buffer-type-tz-usage-table",
		(u32 *)buffer_usage_set->buffer_usage_tbl,
		buffer_usage_set->count *
		(sizeof(*buffer_usage_set->buffer_usage_tbl)/sizeof(u32)));
	if (rc) {
		dprintk(VIDC_ERR, "Failed to read buffer usage table\n");
		goto err_load_buf_usage;
	}

	return 0;
err_load_buf_usage:
	msm_vidc_free_buffer_usage_table(res);
	return rc;
}


static int read_platform_resources_from_dt(
		struct msm_vidc_platform_resources *res)
{
	struct platform_device *pdev = res->pdev;
	struct resource *kres = NULL;
	int rc = 0;

	if (!pdev->dev.of_node) {
		dprintk(VIDC_ERR, "DT node not found\n");
		return -ENOENT;
	}

	res->fw_base_addr = 0x0;

	kres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res->register_base = kres ? kres->start : -1;
	res->register_size = kres ? (kres->end + 1 - kres->start) : -1;

	kres = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	res->irq = kres ? kres->start : -1;

	res->has_ocmem = of_property_read_bool(pdev->dev.of_node,
						"qcom,has-ocmem");

	rc = msm_vidc_load_freq_table(res);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to load freq table: %d\n", rc);
		goto err_load_freq_table;
	}
	rc = msm_vidc_load_reg_table(res);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to load reg table: %d\n", rc);
		goto err_load_reg_table;
	}
	rc = msm_vidc_load_bus_vectors(res);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to load bus vectors: %d\n", rc);
		goto err_load_bus_vectors;
	}
	rc = msm_vidc_load_iommu_groups(res);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to load iommu groups: %d\n", rc);
		goto err_load_iommu_groups;
	}
	rc = msm_vidc_load_buffer_usage_table(res);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to load buffer usage table: %d\n", rc);
		goto err_load_buffer_usage_table;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,max-hw-load",
			&res->max_load);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to determine max load supported: %d\n", rc);
		goto err_load_buffer_usage_table;
	}

	return rc;

err_load_buffer_usage_table:
	msm_vidc_free_iommu_groups(res);
err_load_iommu_groups:
	msm_vidc_free_bus_vectors(res);
err_load_bus_vectors:
	msm_vidc_free_reg_table(res);
err_load_reg_table:
	msm_vidc_free_freq_table(res);
err_load_freq_table:
	return rc;
}

static int read_platform_resources_from_board(
		struct msm_vidc_platform_resources *res)
{
	struct resource *kres = NULL;
	struct platform_device *pdev = res->pdev;
	struct msm_vidc_v4l2_platform_data *pdata = pdev->dev.platform_data;
	int c = 0, rc = 0;

	if (!pdata) {
		dprintk(VIDC_ERR, "Platform data not found\n");
		return -ENOENT;
	}

	res->fw_base_addr = 0x0;

	kres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res->register_base = kres ? kres->start : -1;
	res->register_size = kres ? (kres->end + 1 - kres->start) : -1;

	kres = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	res->irq = kres ? kres->start : -1;

	res->load_freq_tbl = kzalloc(pdata->num_load_table *
			sizeof(*res->load_freq_tbl), GFP_KERNEL);

	if (!res->load_freq_tbl) {
		dprintk(VIDC_ERR, "%s Failed to alloc load_freq_tbl\n",
				__func__);
		return -ENOMEM;
	}

	res->load_freq_tbl_size = pdata->num_load_table;
	for (c = 0; c > pdata->num_load_table; ++c) {
		res->load_freq_tbl[c].load = pdata->load_table[c][0];
		res->load_freq_tbl[c].freq = pdata->load_table[c][1];
	}

	res->max_load = pdata->max_load;
	return rc;
}

static int read_platform_resources(struct msm_vidc_core *core,
		struct platform_device *pdev)
{
	if (!core || !pdev) {
		dprintk(VIDC_ERR, "%s: Invalid params %p %p\n",
			__func__, core, pdev);
		return -EINVAL;
	}
	core->hfi_type = read_hfi_type(pdev);
	if (core->hfi_type < 0) {
		dprintk(VIDC_ERR, "Failed to identify core type\n");
		return core->hfi_type;
	}

	core->resources.pdev = pdev;
	if (pdev->dev.of_node) {
		/* Target supports DT, parse from it */
		return read_platform_resources_from_dt(&core->resources);
	} else {
		/* Legacy board file usage */
		return read_platform_resources_from_board(
				&core->resources);
	}
}
static int msm_vidc_initialize_core(struct platform_device *pdev,
				struct msm_vidc_core *core)
{
	int i = 0;
	int rc = 0;
	if (!core)
		return -EINVAL;
	rc = read_platform_resources(core, pdev);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get platform resources\n");
		return rc;
	}

	INIT_LIST_HEAD(&core->instances);
	mutex_init(&core->sync_lock);
	mutex_init(&core->lock);

	core->state = VIDC_CORE_UNINIT;
	for (i = SYS_MSG_INDEX(SYS_MSG_START);
		i <= SYS_MSG_INDEX(SYS_MSG_END); i++) {
		init_completion(&core->completions[i]);
	}

	return rc;
}

static ssize_t msm_vidc_link_name_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct msm_vidc_core *core = dev_get_drvdata(dev);
	if (core)
		if (dev == &core->vdev[MSM_VIDC_DECODER].vdev.dev)
			if (core->hfi_type == VIDC_HFI_Q6)
				return snprintf(buf, PAGE_SIZE, "q6_dec");
			else
				return snprintf(buf, PAGE_SIZE, "venus_dec");
		else if (dev == &core->vdev[MSM_VIDC_ENCODER].vdev.dev)
			if (core->hfi_type == VIDC_HFI_Q6)
				return snprintf(buf, PAGE_SIZE, "q6_enc");
			else
				return snprintf(buf, PAGE_SIZE, "venus_enc");
		else
			return 0;
	else
		return 0;
}

static DEVICE_ATTR(link_name, 0644, msm_vidc_link_name_show, NULL);

static int __devinit msm_vidc_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_vidc_core *core;
	struct device *dev;
	int nr = BASE_DEVICE_NUMBER;

	core = kzalloc(sizeof(*core), GFP_KERNEL);
	if (!core || !vidc_driver) {
		dprintk(VIDC_ERR,
			"Failed to allocate memory for device core\n");
		rc = -ENOMEM;
		goto err_no_mem;
	}
	rc = msm_vidc_initialize_core(pdev, core);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to init core\n");
		goto err_v4l2_register;
	}
	if (core->hfi_type == VIDC_HFI_Q6) {
		dprintk(VIDC_ERR, "Q6 hfi device probe called\n");
		nr += MSM_VIDC_MAX_DEVICES;
	}
	rc = v4l2_device_register(&pdev->dev, &core->v4l2_dev);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register v4l2 device\n");
		goto err_v4l2_register;
	}
	core->vdev[MSM_VIDC_DECODER].vdev.release =
		msm_vidc_release_video_device;
	core->vdev[MSM_VIDC_DECODER].vdev.fops = &msm_v4l2_vidc_fops;
	core->vdev[MSM_VIDC_DECODER].vdev.ioctl_ops = &msm_v4l2_ioctl_ops;
	core->vdev[MSM_VIDC_DECODER].type = MSM_VIDC_DECODER;
	rc = video_register_device(&core->vdev[MSM_VIDC_DECODER].vdev,
					VFL_TYPE_GRABBER, nr);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register video decoder device");
		goto err_dec_register;
	}
	video_set_drvdata(&core->vdev[MSM_VIDC_DECODER].vdev, core);
	dev = &core->vdev[MSM_VIDC_DECODER].vdev.dev;
	rc = device_create_file(dev, &dev_attr_link_name);
	if (rc) {
		dprintk(VIDC_ERR,
				"Failed to create link name sysfs for decoder");
		goto err_dec_attr_link_name;
	}

	core->vdev[MSM_VIDC_ENCODER].vdev.release =
		msm_vidc_release_video_device;
	core->vdev[MSM_VIDC_ENCODER].vdev.fops = &msm_v4l2_vidc_fops;
	core->vdev[MSM_VIDC_ENCODER].vdev.ioctl_ops = &msm_v4l2_ioctl_ops;
	core->vdev[MSM_VIDC_ENCODER].type = MSM_VIDC_ENCODER;
	rc = video_register_device(&core->vdev[MSM_VIDC_ENCODER].vdev,
				VFL_TYPE_GRABBER, nr + 1);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register video encoder device");
		goto err_enc_register;
	}
	video_set_drvdata(&core->vdev[MSM_VIDC_ENCODER].vdev, core);
	dev = &core->vdev[MSM_VIDC_ENCODER].vdev.dev;
	rc = device_create_file(dev, &dev_attr_link_name);
	if (rc) {
		dprintk(VIDC_ERR,
				"Failed to create link name sysfs for encoder");
		goto err_enc_attr_link_name;
	}

	mutex_lock(&vidc_driver->lock);
	if (vidc_driver->num_cores  + 1 > MSM_VIDC_CORES_MAX) {
		mutex_unlock(&vidc_driver->lock);
		dprintk(VIDC_ERR, "Maximum cores already exist, core_no = %d\n",
				vidc_driver->num_cores);
		goto err_cores_exceeded;
	}
	core->id = vidc_driver->num_cores++;
	mutex_unlock(&vidc_driver->lock);

	core->device = vidc_hfi_initialize(core->hfi_type, core->id,
				&core->resources, &handle_cmd_response);
	if (!core->device) {
		dprintk(VIDC_ERR, "Failed to create HFI device\n");
		mutex_lock(&vidc_driver->lock);
		vidc_driver->num_cores--;
		mutex_unlock(&vidc_driver->lock);
		goto err_cores_exceeded;
	}

	mutex_lock(&vidc_driver->lock);
	list_add_tail(&core->list, &vidc_driver->cores);
	mutex_unlock(&vidc_driver->lock);
	core->debugfs_root = msm_vidc_debugfs_init_core(
		core, vidc_driver->debugfs_root);
	pdev->dev.platform_data = core;
	return rc;

err_cores_exceeded:
	device_remove_file(&core->vdev[MSM_VIDC_ENCODER].vdev.dev,
			&dev_attr_link_name);
err_enc_attr_link_name:
	video_unregister_device(&core->vdev[MSM_VIDC_ENCODER].vdev);
err_enc_register:
	device_remove_file(&core->vdev[MSM_VIDC_DECODER].vdev.dev,
			&dev_attr_link_name);
err_dec_attr_link_name:
	video_unregister_device(&core->vdev[MSM_VIDC_DECODER].vdev);
err_dec_register:
	v4l2_device_unregister(&core->v4l2_dev);
err_v4l2_register:
	kfree(core);
err_no_mem:
	return rc;
}

static int __devexit msm_vidc_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_vidc_core *core;

	if (!pdev) {
		dprintk(VIDC_ERR, "%s invalid input %p", __func__, pdev);
		return -EINVAL;
	}
	core = pdev->dev.platform_data;

	if (!core) {
		dprintk(VIDC_ERR, "%s invalid core", __func__);
		return -EINVAL;
	}

	vidc_hfi_deinitialize(core->hfi_type, core->device);
	device_remove_file(&core->vdev[MSM_VIDC_ENCODER].vdev.dev,
				&dev_attr_link_name);
	video_unregister_device(&core->vdev[MSM_VIDC_ENCODER].vdev);
	device_remove_file(&core->vdev[MSM_VIDC_DECODER].vdev.dev,
				&dev_attr_link_name);
	video_unregister_device(&core->vdev[MSM_VIDC_DECODER].vdev);
	v4l2_device_unregister(&core->v4l2_dev);

	msm_vidc_free_freq_table(&core->resources);
	msm_vidc_free_reg_table(&core->resources);
	msm_vidc_free_bus_vectors(&core->resources);
	msm_vidc_free_iommu_groups(&core->resources);
	msm_vidc_free_buffer_usage_table(&core->resources);
	kfree(core);
	return rc;
}
static const struct of_device_id msm_vidc_dt_match[] = {
	{.compatible = "qcom,msm-vidc"},
};

MODULE_DEVICE_TABLE(of, msm_vidc_dt_match);

static struct platform_driver msm_vidc_driver = {
	.probe = msm_vidc_probe,
	.remove = msm_vidc_remove,
	.driver = {
		.name = "msm_vidc_v4l2",
		.owner = THIS_MODULE,
		.of_match_table = msm_vidc_dt_match,
	},
};

static int __init msm_vidc_init(void)
{
	int rc = 0;
	vidc_driver = kzalloc(sizeof(*vidc_driver),
						GFP_KERNEL);
	if (!vidc_driver) {
		dprintk(VIDC_ERR,
			"Failed to allocate memroy for msm_vidc_drv\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&vidc_driver->cores);
	mutex_init(&vidc_driver->lock);
	vidc_driver->debugfs_root = debugfs_create_dir("msm_vidc", NULL);
	if (!vidc_driver->debugfs_root)
		dprintk(VIDC_ERR,
			"Failed to create debugfs for msm_vidc\n");

	rc = platform_driver_register(&msm_vidc_driver);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to register platform driver\n");
		kfree(vidc_driver);
		vidc_driver = NULL;
	}

	return rc;
}

static void __exit msm_vidc_exit(void)
{
	platform_driver_unregister(&msm_vidc_driver);
	debugfs_remove_recursive(vidc_driver->debugfs_root);
	kfree(vidc_driver);
	vidc_driver = NULL;
}

module_init(msm_vidc_init);
module_exit(msm_vidc_exit);
