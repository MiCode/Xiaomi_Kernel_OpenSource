/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <media/msm_vidc.h>
#include "msm_vidc_internal.h"
#include "msm_vidc_debug.h"
#include "vidc_hal_api.h"
#include "msm_smem.h"

#define BASE_DEVICE_NUMBER 32
#define MAX_EVENTS 30
#define SHARED_QSIZE 0x1000000

static struct msm_bus_vectors enc_ocmem_init_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors enc_ocmem_perf1_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 138200000,
		.ib = 1222000000,
	},
};

static struct msm_bus_vectors enc_ocmem_perf2_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 414700000,
		.ib = 1222000000,
	},
};

static struct msm_bus_vectors enc_ocmem_perf3_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 940000000,
		.ib = 2444000000U,
	},
};

static struct msm_bus_vectors enc_ocmem_perf4_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 1880000000,
		.ib = 2444000000U,
	},
};

static struct msm_bus_vectors enc_ocmem_perf5_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 3008000000U,
		.ib = 3910400000U,
	},
};

static struct msm_bus_vectors enc_ocmem_perf6_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 3760000000U,
		.ib = 3910400000U,
	},
};


static struct msm_bus_vectors dec_ocmem_init_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors dec_ocmem_perf1_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 176900000,
		.ib = 1556640000,
	},
};

static struct msm_bus_vectors dec_ocmem_perf2_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 456200000,
		.ib = 1556640000,
	},
};

static struct msm_bus_vectors dec_ocmem_perf3_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 864800000,
		.ib = 1556640000,
	},
};

static struct msm_bus_vectors dec_ocmem_perf4_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 1729600000,
		.ib = 3113280000U,
	},
};

static struct msm_bus_vectors dec_ocmem_perf5_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 2767360000U,
		.ib = 3113280000U,
	},
};

static struct msm_bus_vectors dec_ocmem_perf6_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.dst = MSM_BUS_SLAVE_OCMEM,
		.ab = 3459200000U,
		.ib = 3459200000U,
	},
};

static struct msm_bus_paths enc_ocmem_perf_vectors[]  = {
	{
		ARRAY_SIZE(enc_ocmem_init_vectors),
		enc_ocmem_init_vectors,
	},
	{
		ARRAY_SIZE(enc_ocmem_perf1_vectors),
		enc_ocmem_perf1_vectors,
	},
	{
		ARRAY_SIZE(enc_ocmem_perf2_vectors),
		enc_ocmem_perf2_vectors,
	},
	{
		ARRAY_SIZE(enc_ocmem_perf3_vectors),
		enc_ocmem_perf3_vectors,
	},
	{
		ARRAY_SIZE(enc_ocmem_perf4_vectors),
		enc_ocmem_perf4_vectors,
	},
	{
		ARRAY_SIZE(enc_ocmem_perf5_vectors),
		enc_ocmem_perf5_vectors,
	},
	{
		ARRAY_SIZE(enc_ocmem_perf6_vectors),
		enc_ocmem_perf6_vectors,
	},
};

static struct msm_bus_paths dec_ocmem_perf_vectors[]  = {
	{
		ARRAY_SIZE(dec_ocmem_init_vectors),
		dec_ocmem_init_vectors,
	},
	{
		ARRAY_SIZE(dec_ocmem_perf1_vectors),
		dec_ocmem_perf1_vectors,
	},
	{
		ARRAY_SIZE(dec_ocmem_perf2_vectors),
		dec_ocmem_perf2_vectors,
	},
	{
		ARRAY_SIZE(dec_ocmem_perf3_vectors),
		dec_ocmem_perf3_vectors,
	},
	{
		ARRAY_SIZE(dec_ocmem_perf4_vectors),
		dec_ocmem_perf4_vectors,
	},
	{
		ARRAY_SIZE(dec_ocmem_perf5_vectors),
		dec_ocmem_perf5_vectors,
	},
	{
		ARRAY_SIZE(dec_ocmem_perf6_vectors),
		dec_ocmem_perf6_vectors,
	},
};


static struct msm_bus_scale_pdata enc_ocmem_bus_data = {
	.usecase = enc_ocmem_perf_vectors,
	.num_usecases = ARRAY_SIZE(enc_ocmem_perf_vectors),
	.name = "msm_vidc_enc_ocmem",
};

static struct msm_bus_scale_pdata dec_ocmem_bus_data = {
	.usecase = dec_ocmem_perf_vectors,
	.num_usecases = ARRAY_SIZE(dec_ocmem_perf_vectors),
	.name = "msm_vidc_dec_ocmem",
};

static struct msm_bus_vectors enc_ddr_init_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};


static struct msm_bus_vectors enc_ddr_perf1_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 60000000,
		.ib = 664950000,
	},
};

static struct msm_bus_vectors enc_ddr_perf2_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 181000000,
		.ib = 664950000,
	},
};

static struct msm_bus_vectors enc_ddr_perf3_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 403000000,
		.ib = 664950000,
	},
};

static struct msm_bus_vectors enc_ddr_perf4_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 806000000,
		.ib = 1329900000,
	},
};

static struct msm_bus_vectors enc_ddr_perf5_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 1289600000,
		.ib = 2127840000U,
	},
};

static struct msm_bus_vectors enc_ddr_perf6_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 161200000,
		.ib = 2659800000U,
	},
};

static struct msm_bus_vectors dec_ddr_init_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors dec_ddr_perf1_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 110000000,
		.ib = 909000000,
	},
};

static struct msm_bus_vectors dec_ddr_perf2_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 268000000,
		.ib = 909000000,
	},
};

static struct msm_bus_vectors dec_ddr_perf3_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 505000000,
		.ib = 909000000,
	},
};

static struct msm_bus_vectors dec_ddr_perf4_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 1010000000,
		.ib = 1818000000,
	},
};

static struct msm_bus_vectors dec_ddr_perf5_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 1616000000,
		.ib = 2908800000U,
	},
};

static struct msm_bus_vectors dec_ddr_perf6_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_P0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 2020000000U,
		.ib = 3636000000U,
	},
};

static struct msm_bus_paths enc_ddr_perf_vectors[]  = {
	{
		ARRAY_SIZE(enc_ddr_init_vectors),
		enc_ddr_init_vectors,
	},
	{
		ARRAY_SIZE(enc_ddr_perf1_vectors),
		enc_ddr_perf1_vectors,
	},
	{
		ARRAY_SIZE(enc_ddr_perf2_vectors),
		enc_ddr_perf2_vectors,
	},
	{
		ARRAY_SIZE(enc_ddr_perf3_vectors),
		enc_ddr_perf3_vectors,
	},
	{
		ARRAY_SIZE(enc_ddr_perf4_vectors),
		enc_ddr_perf4_vectors,
	},
	{
		ARRAY_SIZE(enc_ddr_perf5_vectors),
		enc_ddr_perf5_vectors,
	},
	{
		ARRAY_SIZE(enc_ddr_perf6_vectors),
		enc_ddr_perf6_vectors,
	},
};

static struct msm_bus_paths dec_ddr_perf_vectors[]  = {
	{
		ARRAY_SIZE(dec_ddr_init_vectors),
		dec_ddr_init_vectors,
	},
	{
		ARRAY_SIZE(dec_ddr_perf1_vectors),
		dec_ddr_perf1_vectors,
	},
	{
		ARRAY_SIZE(dec_ddr_perf2_vectors),
		dec_ddr_perf2_vectors,
	},
	{
		ARRAY_SIZE(dec_ddr_perf3_vectors),
		dec_ddr_perf3_vectors,
	},
	{
		ARRAY_SIZE(dec_ddr_perf4_vectors),
		dec_ddr_perf4_vectors,
	},
	{
		ARRAY_SIZE(dec_ddr_perf5_vectors),
		dec_ddr_perf5_vectors,
	},
	{
		ARRAY_SIZE(dec_ddr_perf6_vectors),
		dec_ddr_perf6_vectors,
	},
};

static struct msm_bus_scale_pdata enc_ddr_bus_data = {
	.usecase = enc_ddr_perf_vectors,
	.num_usecases = ARRAY_SIZE(enc_ddr_perf_vectors),
	.name = "msm_vidc_enc_ddr",
};

static struct msm_bus_scale_pdata dec_ddr_bus_data = {
	.usecase = dec_ddr_perf_vectors,
	.num_usecases = ARRAY_SIZE(dec_ddr_perf_vectors),
	.name = "msm_vidc_dec_ddr",
};

struct msm_vidc_drv *vidc_driver;

struct buffer_info {
	struct list_head list;
	int type;
	int fd;
	int buff_off;
	int size;
	u32 uvaddr;
	u32 device_addr;
	struct msm_smem *handle;
};

struct msm_v4l2_vid_inst {
	struct msm_vidc_inst vidc_inst;
	void *mem_client;
	struct list_head registered_bufs;
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
	return container_of((void *)vidc_inst,
			struct msm_v4l2_vid_inst, vidc_inst);
}

static int msm_vidc_v4l2_setup_event_queue(void *inst,
				struct video_device *pvdev)
{
	int rc = 0;
	struct msm_vidc_inst *vidc_inst = (struct msm_vidc_inst *)inst;
	spin_lock_init(&pvdev->fh_lock);
	INIT_LIST_HEAD(&pvdev->fh_list);

	v4l2_fh_init(&vidc_inst->event_handler, pvdev);
	v4l2_fh_add(&vidc_inst->event_handler);

	return rc;
}

struct buffer_info *get_registered_buf(struct list_head *list,
				int fd, u32 buff_off, u32 size)
{
	struct buffer_info *temp;
	struct buffer_info *ret = NULL;
	if (!list || fd < 0) {
		dprintk(VIDC_ERR, "Invalid input\n");
		goto err_invalid_input;
	}
	if (!list_empty(list)) {
		list_for_each_entry(temp, list, list) {
			if (temp && temp->fd == fd &&
			(CONTAINS(temp->buff_off, temp->size, buff_off)
			|| CONTAINS(buff_off, size, temp->buff_off)
			|| OVERLAPS(buff_off, size,
				temp->buff_off, temp->size))) {
				dprintk(VIDC_WARN,
				"This memory region is already mapped\n");
				ret = temp;
				break;
			}
		}
	}
err_invalid_input:
	return ret;
}

struct buffer_info *get_same_fd_buffer(struct list_head *list,
		int fd)
{
	struct buffer_info *temp;
	struct buffer_info *ret = NULL;
	if (!list || fd < 0) {
		dprintk(VIDC_ERR, "Invalid input\n");
		goto err_invalid_input;
	}
	if (!list_empty(list)) {
		list_for_each_entry(temp, list, list) {
			if (temp && temp->fd == fd)  {
				dprintk(VIDC_ERR, "Found same fd buffer\n");
				ret = temp;
				break;
			}
		}
	}
err_invalid_input:
	return ret;
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
	v4l2_inst->mem_client = msm_smem_new_client(SMEM_ION);
	if (!v4l2_inst->mem_client) {
		dprintk(VIDC_ERR, "Failed to create memory client\n");
		rc = -ENOMEM;
		goto fail_mem_client;
	}
	rc = msm_vidc_open(&v4l2_inst->vidc_inst, core->id, vid_dev->type);
	if (rc) {
		dprintk(VIDC_ERR,
		"Failed to create video instance, core: %d, type = %d\n",
		core->id, vid_dev->type);
		rc = -ENOMEM;
		goto fail_open;
	}
	INIT_LIST_HEAD(&v4l2_inst->registered_bufs);
	rc = msm_vidc_v4l2_setup_event_queue(&v4l2_inst->vidc_inst, vdev);
	clear_bit(V4L2_FL_USES_V4L2_FH, &vdev->flags);
	filp->private_data = &(v4l2_inst->vidc_inst.event_handler);
	return rc;
fail_open:
	msm_smem_delete_client(v4l2_inst->mem_client);
fail_mem_client:
	kfree(v4l2_inst);
fail_nomem:
	return rc;
}

static int msm_v4l2_close(struct file *filp)
{
	int rc;
	struct list_head *ptr, *next;
	struct buffer_info *binfo;
	struct msm_vidc_inst *vidc_inst;
	struct msm_v4l2_vid_inst *v4l2_inst;
	vidc_inst = get_vidc_inst(filp, NULL);
	v4l2_inst = get_v4l2_inst(filp, NULL);
	rc = msm_vidc_close(vidc_inst);
	list_for_each_safe(ptr, next, &v4l2_inst->registered_bufs) {
		binfo = list_entry(ptr, struct buffer_info, list);
		list_del(&binfo->list);
		msm_smem_free(v4l2_inst->mem_client, binfo->handle);
		kfree(binfo);
	}
	msm_smem_delete_client(v4l2_inst->mem_client);
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
	struct list_head *ptr, *next;
	int rc;
	struct buffer_info *bi;
	struct v4l2_buffer buffer_info;
	struct v4l2_plane plane;
	v4l2_inst = get_v4l2_inst(file, NULL);
	if (b->count == 0) {
		list_for_each_safe(ptr, next, &v4l2_inst->registered_bufs) {
			bi = list_entry(ptr, struct buffer_info, list);
			if (bi->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
				buffer_info.type = bi->type;
				plane.reserved[0] = bi->fd;
				plane.reserved[1] = bi->buff_off;
				plane.length = bi->size;
				plane.m.userptr = bi->device_addr;
				buffer_info.m.planes = &plane;
				buffer_info.length = 1;
				dprintk(VIDC_DBG,
					"Releasing buffer: %d, %d, %d\n",
					buffer_info.m.planes[0].reserved[0],
					buffer_info.m.planes[0].reserved[1],
					buffer_info.m.planes[0].length);
				rc = msm_vidc_release_buf(&v4l2_inst->vidc_inst,
					&buffer_info);
				list_del(&bi->list);
				if (bi->handle)
					msm_smem_free(v4l2_inst->mem_client,
							bi->handle);
				kfree(bi);
			}
		}
	}
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
	int i, rc = 0;
	vidc_inst = get_vidc_inst(file, fh);
	v4l2_inst = get_v4l2_inst(file, fh);
	if (!v4l2_inst->mem_client) {
		dprintk(VIDC_ERR, "Failed to get memory client\n");
		rc = -ENOMEM;
		goto exit;
	}
	for (i = 0; i < b->length; ++i) {
		binfo = get_registered_buf(&v4l2_inst->registered_bufs,
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length);
		if (binfo) {
			dprintk(VIDC_WARN,
				"This memory region has already been prepared\n");
			rc = -EINVAL;
			goto exit;
		}
		binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
		if (!binfo) {
			dprintk(VIDC_ERR, "Out of memory\n");
			rc = -ENOMEM;
			goto exit;
		}
		temp = get_same_fd_buffer(&v4l2_inst->registered_bufs,
				b->m.planes[i].reserved[0]);
		if (temp) {
			binfo->type = b->type;
			binfo->fd = b->m.planes[i].reserved[0];
			binfo->buff_off = b->m.planes[i].reserved[1];
			binfo->size = b->m.planes[i].length;
			binfo->uvaddr = b->m.planes[i].m.userptr;
			binfo->device_addr =
				temp->handle->device_addr + binfo->buff_off;
			binfo->handle = NULL;
		} else {
			handle = msm_smem_user_to_kernel(v4l2_inst->mem_client,
			b->m.planes[i].reserved[0],
			b->m.planes[i].reserved[1],
			vidc_inst->core->resources.io_map[NS_MAP].domain,
			0);
			if (!handle) {
				dprintk(VIDC_ERR,
					"Failed to get device buffer address\n");
				kfree(binfo);
				goto exit;
			}
			binfo->type = b->type;
			binfo->fd = b->m.planes[i].reserved[0];
			binfo->buff_off = b->m.planes[i].reserved[1];
			binfo->size = b->m.planes[i].length;
			binfo->uvaddr = b->m.planes[i].m.userptr;
			binfo->device_addr =
				handle->device_addr + binfo->buff_off;
			binfo->handle = handle;
			dprintk(VIDC_DBG, "Registering buffer: %d, %d, %d\n",
					b->m.planes[i].reserved[0],
					b->m.planes[i].reserved[1],
					b->m.planes[i].length);
		}
		list_add_tail(&binfo->list, &v4l2_inst->registered_bufs);
		b->m.planes[i].m.userptr = binfo->device_addr;
	}
	rc = msm_vidc_prepare_buf(&v4l2_inst->vidc_inst, b);
exit:
	return rc;
}

int msm_v4l2_qbuf(struct file *file, void *fh,
				struct v4l2_buffer *b)
{
	struct msm_vidc_inst *vidc_inst;
	struct msm_v4l2_vid_inst *v4l2_inst;
	struct buffer_info *binfo;
	int rc = 0;
	int i;
	vidc_inst = get_vidc_inst(file, fh);
	v4l2_inst = get_v4l2_inst(file, fh);
	for (i = 0; i < b->length; ++i) {
		binfo = get_registered_buf(&v4l2_inst->registered_bufs,
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length);
		if (!binfo) {
			dprintk(VIDC_ERR,
				"This buffer is not registered: %d, %d, %d\n",
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length);
			rc = -EINVAL;
			goto err_invalid_buff;
		}
		b->m.planes[i].m.userptr = binfo->device_addr;
		dprintk(VIDC_DBG, "Queueing device address = 0x%x\n",
				binfo->device_addr);
		if (binfo->handle) {
			rc = msm_smem_clean_invalidate(v4l2_inst->mem_client,
					binfo->handle);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to clean caches: %d\n", rc);
				goto err_invalid_buff;
			}
		}
	}
	rc = msm_vidc_qbuf(&v4l2_inst->vidc_inst, b);
err_invalid_buff:
	return rc;
}

int msm_v4l2_dqbuf(struct file *file, void *fh,
				struct v4l2_buffer *b)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_dqbuf((void *)vidc_inst, b);
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
	int rc = 0;
	rc = v4l2_event_subscribe(fh, sub, MAX_EVENTS);
	return rc;
}

static int msm_v4l2_unsubscribe_event(struct v4l2_fh *fh,
				struct v4l2_event_subscription *sub)
{
	int rc = 0;
	rc = v4l2_event_unsubscribe(fh, sub);
	return rc;
}

static int msm_v4l2_decoder_cmd(struct file *file, void *fh,
				struct v4l2_decoder_cmd *dec)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_decoder_cmd((void *)vidc_inst, dec);
}

static int msm_v4l2_encoder_cmd(struct file *file, void *fh,
				struct v4l2_encoder_cmd *enc)
{
	struct msm_vidc_inst *vidc_inst = get_vidc_inst(file, fh);
	return msm_vidc_encoder_cmd((void *)vidc_inst, enc);
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

static size_t read_u32_array(struct platform_device *pdev,
		char *name, u32 *arr, size_t size)
{
	int len;
	size_t sz = 0;
	struct device_node *np = pdev->dev.of_node;
	if (!of_get_property(np, name, &len)) {
		dprintk(VIDC_ERR, "Failed to read %s from device tree\n",
			name);
		goto fail_read;
	}
	sz = len / sizeof(u32);
	if (sz <= 0) {
		dprintk(VIDC_ERR, "%s not specified in device tree\n",
			name);
		goto fail_read;
	}
	if (sz > size) {
		dprintk(VIDC_ERR, "Not enough memory to store %s values\n",
			name);
		goto fail_read;
	}
	if (of_property_read_u32_array(np, name, arr, sz)) {
		dprintk(VIDC_ERR,
			"error while reading %s from device tree\n",
			name);
		goto fail_read;
	}
	return sz;
fail_read:
	sz = 0;
	return sz;
}

static int register_iommu_domains(struct platform_device *pdev,
	struct msm_vidc_core *core)
{
	size_t len;
	struct msm_iova_partition partition[2];
	struct msm_iova_layout layout;
	int rc = 0;
	int i;
	struct iommu_info *io_map = core->resources.io_map;
	strlcpy(io_map[CP_MAP].name, "vidc-cp-map",
			sizeof(io_map[CP_MAP].name));
	strlcpy(io_map[CP_MAP].ctx, "venus_cp",
			sizeof(io_map[CP_MAP].ctx));
	strlcpy(io_map[NS_MAP].name, "vidc-ns-map",
			sizeof(io_map[NS_MAP].name));
	strlcpy(io_map[NS_MAP].ctx, "venus_ns",
			sizeof(io_map[NS_MAP].ctx));

	for (i = 0; i < MAX_MAP; i++) {
		len = read_u32_array(pdev, io_map[i].name,
				io_map[i].addr_range,
				(sizeof(io_map[i].addr_range)/sizeof(u32)));
		if (!len) {
			dprintk(VIDC_ERR,
				"Error in reading cp address range\n");
			rc = -EINVAL;
			break;
		}
		partition[0].start = io_map[i].addr_range[0];
		if (i == NS_MAP) {
			partition[0].size =
				io_map[i].addr_range[1] - SHARED_QSIZE;
			partition[1].start =
				partition[0].start + io_map[i].addr_range[1]
					- SHARED_QSIZE;
			partition[1].size = SHARED_QSIZE;
			layout.npartitions = 2;
		} else {
			partition[0].size = io_map[i].addr_range[1];
			layout.npartitions = 1;
		}
		layout.partitions = &partition[0];
		layout.client_name = io_map[i].name;
		layout.domain_flags = 0;
		dprintk(VIDC_DBG, "Registering domain 1 with: %lx, %lx, %s\n",
			partition[0].start, partition[0].size,
			layout.client_name);
		dprintk(VIDC_DBG, "Registering domain 2 with: %lx, %lx, %s\n",
			partition[1].start, partition[1].size,
			layout.client_name);
		io_map[i].domain = msm_register_domain(&layout);
		if (io_map[i].domain < 0) {
			dprintk(VIDC_ERR, "Failed to register cp domain\n");
			rc = -EINVAL;
			break;
		}
	}
	/* There is no api provided as msm_unregister_domain, so
	 * we are not able to unregister the previously
	 * registered domains if any domain registration fails.*/
	BUG_ON(i < MAX_MAP);
	return rc;
}

static inline int msm_vidc_init_clocks(struct platform_device *pdev,
		struct msm_vidc_core *core)
{
	struct core_clock *cl;
	int i;
	int rc = 0;
	struct core_clock *clock;
	if (!core) {
		dprintk(VIDC_ERR, "Invalid params: %p\n", core);
		return -EINVAL;
	}
	clock = core->resources.clock;
	strlcpy(clock[VCODEC_CLK].name, "core_clk",
		sizeof(clock[VCODEC_CLK].name));
	strlcpy(clock[VCODEC_AHB_CLK].name, "iface_clk",
		sizeof(clock[VCODEC_AHB_CLK].name));
	strlcpy(clock[VCODEC_AXI_CLK].name, "bus_clk",
		sizeof(clock[VCODEC_AXI_CLK].name));
	strlcpy(clock[VCODEC_OCMEM_CLK].name, "mem_clk",
		sizeof(clock[VCODEC_OCMEM_CLK].name));

	clock[VCODEC_CLK].count = read_u32_array(pdev,
		"load-freq-tbl", (u32 *)clock[VCODEC_CLK].load_freq_tbl,
		(sizeof(clock[VCODEC_CLK].load_freq_tbl)/sizeof(u32)));
	clock[VCODEC_CLK].count /= 2;
	dprintk(VIDC_DBG, "count = %d\n", clock[VCODEC_CLK].count);
	if (!clock[VCODEC_CLK].count) {
		dprintk(VIDC_ERR, "Failed to read clock frequency\n");
		goto fail_init_clocks;
	}
	for (i = 0; i <	clock[VCODEC_CLK].count; i++) {
		dprintk(VIDC_DBG,
				"load = %d, freq = %d\n",
				clock[VCODEC_CLK].load_freq_tbl[i].load,
				clock[VCODEC_CLK].load_freq_tbl[i].freq
			  );
	}

	for (i = 0; i < VCODEC_MAX_CLKS; i++) {
		cl = &core->resources.clock[i];
		if (!cl->clk) {
			cl->clk = devm_clk_get(&pdev->dev, cl->name);
			if (IS_ERR_OR_NULL(cl->clk)) {
				dprintk(VIDC_ERR,
					"Failed to get clock: %s\n", cl->name);
				rc = PTR_ERR(cl->clk);
				break;
			}
		}
	}

	if (i < VCODEC_MAX_CLKS) {
		for (--i; i >= 0; i--) {
			cl = &core->resources.clock[i];
			clk_put(cl->clk);
		}
	}
fail_init_clocks:
	return rc;
}

static inline void msm_vidc_deinit_clocks(struct msm_vidc_core *core)
{
	int i;
	if (!core) {
		dprintk(VIDC_ERR, "Invalid args\n");
		return;
	}
	for (i = 0; i < VCODEC_MAX_CLKS; i++)
		clk_put(core->resources.clock[i].clk);
}

static int msm_vidc_initialize_core(struct platform_device *pdev,
				struct msm_vidc_core *core)
{
	struct resource *res;
	int i = 0;
	int rc = 0;
	struct on_chip_mem *ocmem;
	if (!core)
		return -EINVAL;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dprintk(VIDC_ERR, "Failed to get IORESOURCE_MEM\n");
		rc = -ENODEV;
		goto core_init_failed;
	}
	core->register_base = res->start;
	core->register_size = resource_size(res);
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dprintk(VIDC_ERR, "Failed to get IORESOURCE_IRQ\n");
		rc = -ENODEV;
		goto core_init_failed;
	}
	core->irq = res->start;
	INIT_LIST_HEAD(&core->instances);
	mutex_init(&core->sync_lock);
	spin_lock_init(&core->lock);
	core->base_addr = 0x0;
	core->state = VIDC_CORE_UNINIT;
	for (i = SYS_MSG_INDEX(SYS_MSG_START);
		i <= SYS_MSG_INDEX(SYS_MSG_END); i++) {
		init_completion(&core->completions[i]);
	}
	rc = msm_vidc_init_clocks(pdev, core);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to init clocks\n");
		rc = -ENODEV;
		goto core_init_failed;
	}
	core->resources.bus_info.ddr_handle[MSM_VIDC_ENCODER] =
		msm_bus_scale_register_client(&enc_ddr_bus_data);
	if (!core->resources.bus_info.ddr_handle[MSM_VIDC_ENCODER]) {
		dprintk(VIDC_ERR, "Failed to register bus scale client\n");
		goto fail_register_enc_ddr_bus;
	}
	core->resources.bus_info.ddr_handle[MSM_VIDC_DECODER] =
		msm_bus_scale_register_client(&dec_ddr_bus_data);
	if (!core->resources.bus_info.ddr_handle[MSM_VIDC_DECODER]) {
		dprintk(VIDC_ERR, "Failed to register bus scale client\n");
		goto fail_register_dec_ddr_bus;
	}
	core->resources.bus_info.ocmem_handle[MSM_VIDC_ENCODER] =
		msm_bus_scale_register_client(&enc_ocmem_bus_data);
	if (!core->resources.bus_info.ocmem_handle[MSM_VIDC_ENCODER]) {
		dprintk(VIDC_ERR, "Failed to register bus scale client\n");
		goto fail_register_enc_ocmem;
	}
	core->resources.bus_info.ocmem_handle[MSM_VIDC_DECODER] =
		msm_bus_scale_register_client(&dec_ocmem_bus_data);
	if (!core->resources.bus_info.ocmem_handle[MSM_VIDC_DECODER]) {
		dprintk(VIDC_ERR, "Failed to register bus scale client\n");
		goto fail_register_dec_ocmem;
	}
	rc = register_iommu_domains(pdev, core);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register iommu domains: %d\n", rc);
		goto fail_register_domains;
	}
	ocmem = &core->resources.ocmem;
	ocmem->vidc_ocmem_nb.notifier_call = msm_vidc_ocmem_notify_handler;
	ocmem->handle =
		ocmem_notifier_register(OCMEM_VIDEO, &ocmem->vidc_ocmem_nb);
	if (!ocmem->handle) {
		dprintk(VIDC_WARN, "Failed to register OCMEM notifier.");
		dprintk(VIDC_WARN, " Performance will be impacted\n");
	}
	return rc;
fail_register_domains:
	msm_bus_scale_unregister_client(
		core->resources.bus_info.ocmem_handle[MSM_VIDC_DECODER]);
fail_register_dec_ocmem:
	msm_bus_scale_unregister_client(
		core->resources.bus_info.ocmem_handle[MSM_VIDC_ENCODER]);
fail_register_enc_ocmem:
	msm_bus_scale_unregister_client(
		core->resources.bus_info.ddr_handle[MSM_VIDC_DECODER]);
fail_register_dec_ddr_bus:
	msm_bus_scale_unregister_client(
		core->resources.bus_info.ddr_handle[MSM_VIDC_ENCODER]);
fail_register_enc_ddr_bus:
	msm_vidc_deinit_clocks(core);
core_init_failed:
	return rc;
}

static int __devinit msm_vidc_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_vidc_core *core;
	unsigned long flags;
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
					VFL_TYPE_GRABBER, BASE_DEVICE_NUMBER);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register video decoder device");
		goto err_dec_register;
	}
	video_set_drvdata(&core->vdev[MSM_VIDC_DECODER].vdev, core);

	core->vdev[MSM_VIDC_ENCODER].vdev.release =
		msm_vidc_release_video_device;
	core->vdev[MSM_VIDC_ENCODER].vdev.fops = &msm_v4l2_vidc_fops;
	core->vdev[MSM_VIDC_ENCODER].vdev.ioctl_ops = &msm_v4l2_ioctl_ops;
	core->vdev[MSM_VIDC_ENCODER].type = MSM_VIDC_ENCODER;
	rc = video_register_device(&core->vdev[MSM_VIDC_ENCODER].vdev,
				VFL_TYPE_GRABBER, BASE_DEVICE_NUMBER + 1);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to register video encoder device");
		goto err_enc_register;
	}
	video_set_drvdata(&core->vdev[MSM_VIDC_ENCODER].vdev, core);
	core->device = vidc_hal_add_device(core->id, core->base_addr,
			core->register_base, core->register_size, core->irq,
			&handle_cmd_response);
	if (!core->device) {
		dprintk(VIDC_ERR, "Failed to create interrupt handler");
		goto err_cores_exceeded;
	}

	spin_lock_irqsave(&vidc_driver->lock, flags);
	if (vidc_driver->num_cores  + 1 > MSM_VIDC_CORES_MAX) {
		spin_unlock_irqrestore(&vidc_driver->lock, flags);
		dprintk(VIDC_ERR, "Maximum cores already exist, core_no = %d\n",
				vidc_driver->num_cores);
		goto err_cores_exceeded;
	}

	core->id = vidc_driver->num_cores++;
	list_add_tail(&core->list, &vidc_driver->cores);
	spin_unlock_irqrestore(&vidc_driver->lock, flags);
	core->debugfs_root = msm_vidc_debugfs_init_core(
		core, vidc_driver->debugfs_root);
	pdev->dev.platform_data = core;
	return rc;

err_cores_exceeded:
	video_unregister_device(&core->vdev[MSM_VIDC_ENCODER].vdev);
err_enc_register:
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
	struct msm_vidc_core *core = pdev->dev.platform_data;
	int i;
	for (i = 0; i < MSM_VIDC_MAX_DEVICES; ++i) {
		msm_bus_scale_unregister_client(
			core->resources.bus_info.ddr_handle[i]);
		msm_bus_scale_unregister_client(
			core->resources.bus_info.ocmem_handle[i]);
	}
	vidc_hal_delete_device(core->device);
	video_unregister_device(&core->vdev[MSM_VIDC_ENCODER].vdev);
	video_unregister_device(&core->vdev[MSM_VIDC_DECODER].vdev);
	v4l2_device_unregister(&core->v4l2_dev);
	if (core->resources.ocmem.handle)
		ocmem_notifier_unregister(core->resources.ocmem.handle,
				&core->resources.ocmem.vidc_ocmem_nb);
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
		.name = "msm_vidc",
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
	spin_lock_init(&vidc_driver->lock);
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
