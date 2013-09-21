/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/msm_mdp.h>
#include <linux/slab.h>
#include <mach/iommu_domains.h>
#include <media/videobuf2-core.h>
#include "enc-subdev.h"
#include "mdp-subdev.h"
#include "wfd-util.h"

struct mdp_instance {
	struct fb_info *mdp;
	u32 height;
	u32 width;
	bool secure;
	bool uses_iommu_split_domain;
};

int mdp_init(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

int mdp_open(struct v4l2_subdev *sd, void *arg)
{
	struct mdp_instance *inst = kzalloc(sizeof(struct mdp_instance),
					GFP_KERNEL);
	struct mdp_msg_ops *mops = arg;
	int rc = 0;
	struct fb_info *fbi = NULL;

	if (!inst) {
		WFD_MSG_ERR("Out of memory\n");
		rc = -ENOMEM;
		goto mdp_open_fail;
	} else if (!mops) {
		WFD_MSG_ERR("Invalid arguments\n");
		rc = -EINVAL;
		goto mdp_open_fail;
	}

	fbi = msm_fb_get_writeback_fb();
	if (!fbi) {
		WFD_MSG_ERR("Failed to acquire mdp instance\n");
		rc = -ENODEV;
		goto mdp_open_fail;
	}

	msm_fb_writeback_init(fbi);
	inst->mdp = fbi;
	inst->secure = mops->secure;
	inst->uses_iommu_split_domain = mops->iommu_split_domain;

	mops->cookie = inst;
	return rc;
mdp_open_fail:
	kfree(inst);
	return rc;
}

int mdp_start(struct v4l2_subdev *sd, void *arg)
{
	struct mdp_instance *inst = arg;
	int rc = 0;
	struct fb_info *fbi = NULL;
	if (inst) {
		rc = msm_fb_writeback_start(inst->mdp);
		if (rc) {
			WFD_MSG_ERR("Failed to start MDP mode\n");
			goto exit;
		}
		fbi = msm_fb_get_writeback_fb();
		if (!fbi) {
			WFD_MSG_ERR("Failed to acquire mdp instance\n");
			rc = -ENODEV;
			goto exit;
		}
	}
exit:
	return rc;
}
int mdp_stop(struct v4l2_subdev *sd, void *arg)
{
	struct mdp_instance *inst = arg;
	int rc = 0;
	struct fb_info *fbi = NULL;
	if (inst) {
		rc = msm_fb_writeback_stop(inst->mdp);
		if (rc) {
			WFD_MSG_ERR("Failed to stop writeback mode\n");
			return rc;
		}
		fbi = (struct fb_info *)inst->mdp;
	}
	return 0;
}
int mdp_close(struct v4l2_subdev *sd, void *arg)
{
	struct mdp_instance *inst = arg;
	struct fb_info *fbi = NULL;
	if (inst) {
		fbi = (struct fb_info *)inst->mdp;
		msm_fb_writeback_terminate(fbi);
		kfree(inst);
	}
	return 0;
}
int mdp_q_buffer(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct mdp_buf_info *binfo = arg;
	struct msmfb_data fbdata;
	struct mdp_instance *inst;
	if (!binfo || !binfo->inst || !binfo->cookie) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}
	inst = binfo->inst;
	fbdata.offset = binfo->offset;
	fbdata.memory_id = binfo->fd;
	fbdata.iova = binfo->paddr;
	fbdata.id = 0;
	fbdata.flags = 0;
	fbdata.priv = (uint32_t)binfo->cookie;

	WFD_MSG_INFO("queue buffer to mdp with offset = %u, fd = %u, "\
			"priv = %p, iova = %p\n",
			fbdata.offset, fbdata.memory_id,
			(void *)fbdata.priv, (void *)fbdata.iova);
	rc = msm_fb_writeback_queue_buffer(inst->mdp, &fbdata);

	if (rc)
		WFD_MSG_ERR("Failed to queue buffer\n");
	return rc;
}
int mdp_dq_buffer(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct mdp_buf_info *obuf = arg;
	struct msmfb_data fbdata;
	struct mdp_instance *inst;
	if (!arg) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}

	inst = obuf->inst;
	fbdata.flags = MSMFB_WRITEBACK_DEQUEUE_BLOCKING;
	rc = msm_fb_writeback_dequeue_buffer(inst->mdp, &fbdata);
	if (rc) {
		WFD_MSG_ERR("Failed to dequeue buffer\n");
		return rc;
	}
	WFD_MSG_DBG("dequeue buf from mdp with priv = %u\n",
			fbdata.priv);
	obuf->cookie = (void *)fbdata.priv;
	return rc;
}
int mdp_set_prop(struct v4l2_subdev *sd, void *arg)
{
	struct mdp_prop *prop = (struct mdp_prop *)arg;
	struct mdp_instance *inst = prop->inst;
	if (!prop || !inst) {
		WFD_MSG_ERR("Invalid arguments\n");
		return -EINVAL;
	}
	inst->height = prop->height;
	inst->width = prop->width;
	return 0;
}

int mdp_mmap(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0, domain = -1;
	struct mem_region_map *mmap = arg;
	struct mem_region *mregion;
	bool use_iommu = true;
	struct mdp_instance *inst = NULL;

	if (!mmap || !mmap->mregion || !mmap->cookie) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}

	inst = mmap->cookie;
	mregion = mmap->mregion;
	if (mregion->size % SZ_4K != 0) {
		WFD_MSG_ERR("Memregion not aligned to %d\n", SZ_4K);
		return -EINVAL;
	}

	if (inst->uses_iommu_split_domain) {
		if (inst->secure)
			use_iommu = false;
		else
			domain = DISPLAY_WRITE_DOMAIN;
	} else {
		domain = DISPLAY_READ_DOMAIN;
	}

	if (use_iommu) {
		rc = ion_map_iommu(mmap->ion_client, mregion->ion_handle,
				domain, GEN_POOL, SZ_4K, 0,
				&mregion->paddr,
				(unsigned long *)&mregion->size,
				0, 0);
	} else {
		rc = ion_phys(mmap->ion_client,	mregion->ion_handle,
				&mregion->paddr,
				(size_t *)&mregion->size);
	}

	return rc;
}

int mdp_munmap(struct v4l2_subdev *sd, void *arg)
{
	struct mem_region_map *mmap = arg;
	struct mem_region *mregion;
	bool use_iommu = false;
	int domain = -1;
	struct mdp_instance *inst = NULL;

	if (!mmap || !mmap->mregion || !mmap->cookie) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}

	inst = mmap->cookie;
	mregion = mmap->mregion;

	if (inst->uses_iommu_split_domain) {
		if (inst->secure)
			use_iommu = false;
		else
			domain = DISPLAY_WRITE_DOMAIN;
	} else {
		domain = DISPLAY_READ_DOMAIN;
	}

	if (use_iommu)
		ion_unmap_iommu(mmap->ion_client,
				mregion->ion_handle,
				domain, GEN_POOL);

	return 0;
}

long mdp_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int rc = 0;
	if (!sd) {
		WFD_MSG_ERR("Invalid arguments\n");
		return -EINVAL;
	}
	switch (cmd) {
	case MDP_Q_BUFFER:
		rc = mdp_q_buffer(sd, arg);
		break;
	case MDP_DQ_BUFFER:
		rc = mdp_dq_buffer(sd, arg);
		break;
	case MDP_OPEN:
		rc = mdp_open(sd, arg);
		break;
	case MDP_START:
		rc = mdp_start(sd, arg);
		break;
	case MDP_STOP:
		rc = mdp_stop(sd, arg);
		break;
	case MDP_SET_PROP:
		rc = mdp_set_prop(sd, arg);
		break;
	case MDP_CLOSE:
		rc = mdp_close(sd, arg);
		break;
	case MDP_MMAP:
		rc = mdp_mmap(sd, arg);
		break;
	case MDP_MUNMAP:
		rc = mdp_munmap(sd, arg);
		break;
	default:
		WFD_MSG_ERR("IOCTL: %u not supported\n", cmd);
		rc = -EINVAL;
		break;
	}
	return rc;
}
