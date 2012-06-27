/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/videodev2.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>

#include <linux/android_pmem.h>
#include <media/msm_camera.h>
#include <media/msm_isp.h>
#include "msm.h"
#include "msm_vfe_stats_buf.h"

#ifdef CONFIG_MSM_CAMERA_DEBUG
	#define D(fmt, args...) pr_debug("msm_stats: " fmt, ##args)
#else
	#define D(fmt, args...) do {} while (0)
#endif

static int msm_stats_init(struct msm_stats_bufq_ctrl *stats_ctrl)
{
	int rc = 0;
	/* cannot get spinlock here */
	if (stats_ctrl->init_done > 0) {
		pr_err("%s: already initialized stats ctrl. no op", __func__);
		return 0;
	}
	memset(stats_ctrl,  0,  sizeof(struct msm_stats_bufq_ctrl));
	spin_lock_init(&stats_ctrl->lock);
	stats_ctrl->init_done = 1;
	return rc;
}

static int msm_stats_reqbuf(struct msm_stats_bufq_ctrl *stats_ctrl,
	struct msm_stats_reqbuf *reqbuf,
	struct ion_client *client)
{
	int rc = 0;
	struct msm_stats_bufq *bufq;
	struct msm_stats_meta_buf *bufs;
	int idx = reqbuf->stats_type;
	int i;

	D("%s: type : %d, buf num : %d\n", __func__,
		reqbuf->stats_type, reqbuf->num_buf);
	if (reqbuf->num_buf > 0) {
		if (stats_ctrl->bufq[idx]) {
			/* already in use. Error */
			pr_err("%s: stats type %d aleady requested",
				 __func__, reqbuf->stats_type);
			rc = -EEXIST;
			goto end;
		} else {
			/* good case */
			bufq = (struct msm_stats_bufq *)
				kzalloc(
					sizeof(struct msm_stats_bufq),
					GFP_KERNEL);
			if (!bufq) {
				/* no memory */
				rc = -ENOMEM;
				pr_err("%s: no mem for stats type %d",
					__func__, reqbuf->stats_type);
				goto end;
			}
			bufs = (struct msm_stats_meta_buf *)
				kzalloc((reqbuf->num_buf *
					sizeof(struct msm_stats_meta_buf)),
					GFP_KERNEL);
			if (!bufs) {
				/* no memory */
				rc = -ENOMEM;
				pr_err("%s: no mem for stats buf, stats type = %d",
					__func__, reqbuf->stats_type);
				kfree(bufq);
				goto end;
			}
			/* init bufq list head */
			INIT_LIST_HEAD(&bufq->head);
			/* set the meta buf state to initialized */
			bufq->num_bufs = reqbuf->num_buf;
			for (i = 0; i < reqbuf->num_buf; i++)
				bufs[i].state =
					MSM_STATS_BUFFER_STATE_INITIALIZED;
			bufq->bufs = bufs;
			bufq->num_bufs = reqbuf->num_buf;
			bufq->type = reqbuf->stats_type;
			stats_ctrl->bufq[idx] = bufq;
			/* done reqbuf (larger than zero case) */
			goto end;
		}
	} else if (reqbuf->num_buf == 0) {
		if (stats_ctrl->bufq[idx] == NULL) {
			/* double free case? */
			pr_err("%s: stats type %d aleady freed",
				 __func__, reqbuf->stats_type);
			rc = -ENXIO;
			goto end;
		} else {
			/* good case. need to de-reqbuf */
			kfree(stats_ctrl->bufq[idx]->bufs);
			kfree(stats_ctrl->bufq[idx]);
			stats_ctrl->bufq[idx] = NULL;
			goto end;
		}
	} else {
		/* error case */
		pr_err("%s: stats type = %d, req_num_buf = %d, error",
			   __func__, reqbuf->stats_type, reqbuf->num_buf);
		rc = -EPERM;
		goto end;
	}
end:
	return rc;
}
static int msm_stats_deinit(struct msm_stats_bufq_ctrl *stats_ctrl)
{
	int rc = 0;
	int i;

	if (stats_ctrl->init_done == 0) {
		pr_err("%s: not inited yet. no op", __func__);
		return 0;
	}
	/* safe guard in case deallocate memory not done yet. */
	for (i = 0; i < MSM_STATS_TYPE_MAX; i++) {
		if (stats_ctrl->bufq[i]) {
			if (stats_ctrl->bufq[i]->bufs) {
				rc = -1;
				pr_err("%s: stats type = %d, buf not freed yet",
					 __func__, i);
				BUG_ON(stats_ctrl->bufq[i]->bufs);
			} else {
				rc = -1;
				pr_err("%s: stats type = %d, bufq not freed yet",
					__func__, i);
				BUG_ON(stats_ctrl->bufq[i]);
			}
		}
	}
	memset(stats_ctrl,  0,  sizeof(struct msm_stats_bufq_ctrl));
	return rc;
}

#ifdef CONFIG_ANDROID_PMEM
static int msm_stats_check_pmem_info(struct msm_stats_buf_info *info, int len)
{
	if (info->offset < len &&
		info->offset + info->len <= len &&
		info->planar0_off < len && info->planar1_off < len)
		return 0;

	pr_err("%s: check failed: off %d len %d y %d cbcr %d (total len %d)\n",
		   __func__,
		   info->offset,
		   info->len,
		   info->planar0_off,
		   info->planar1_off,
		   len);
	return -EINVAL;
}
#endif

static int msm_stats_buf_prepare(struct msm_stats_bufq_ctrl *stats_ctrl,
	struct msm_stats_buf_info *info, struct ion_client *client)
{
	unsigned long paddr;
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
	unsigned long kvstart;
	struct file *file;
#endif
	int rc = 0;
	unsigned long len;
	struct msm_stats_bufq *bufq = NULL;
	struct msm_stats_meta_buf *stats_buf = NULL;

	D("%s: type : %d, buf num : %d\n", __func__,
		info->type, info->buf_idx);

	bufq = stats_ctrl->bufq[info->type];
	stats_buf = &bufq->bufs[info->buf_idx];
	if (stats_buf->state == MSM_STATS_BUFFER_STATE_UNUSED) {
		pr_err("%s: need reqbuf first, stats type = %d",
			__func__, info->type);
		rc = -1;
		goto out1;
	}
	if (stats_buf->state != MSM_STATS_BUFFER_STATE_INITIALIZED) {
		D("%s: stats already mapped, no op, stats type = %d",
			__func__, info->type);
		goto out1;
	}
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	stats_buf->handle = ion_import_dma_buf(client, info->fd);
	if (IS_ERR_OR_NULL(stats_buf->handle)) {
		rc = -EINVAL;
		pr_err("%s: stats_buf has null/error ION handle %p",
			   __func__, stats_buf->handle);
		goto out1;
	}
	if (ion_map_iommu(client, stats_buf->handle,
			CAMERA_DOMAIN, GEN_POOL, SZ_4K,
			0, &paddr, &len, UNCACHED, 0) < 0) {
		rc = -EINVAL;
		pr_err("%s: cannot map address", __func__);
		goto out2;
	}
#elif CONFIG_ANDROID_PMEM
	rc = get_pmem_file(info->fd, &paddr, &kvstart, &len, &file);
	if (rc < 0) {
		pr_err("%s: get_pmem_file fd %d error %d\n",
			   __func__, info->fd, rc);
		goto out1;
	}
	stats_buf->file = file;
#else
	paddr = 0;
	file = NULL;
	kvstart = 0;
#endif
	if (!info->len)
		info->len = len;
	rc = msm_stats_check_pmem_info(info, len);
	if (rc < 0) {
		pr_err("%s: msm_stats_check_pmem_info err = %d", __func__, rc);
		goto out3;
	}
	paddr += info->offset;
	len = info->len;
	stats_buf->paddr = paddr;
	stats_buf->len = len;
	memcpy(&stats_buf->info, info, sizeof(stats_buf->info));
	D("%s Adding buf to list with type %d\n", __func__,
	  stats_buf->info.type);
	D("%s pmem_stats address is 0x%ld\n", __func__, paddr);
	stats_buf->state = MSM_STATS_BUFFER_STATE_PREPARED;
	return 0;
out3:
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	ion_unmap_iommu(client, stats_buf->handle, CAMERA_DOMAIN, GEN_POOL);
#endif
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
out2:
	ion_free(client, stats_buf->handle);
#elif CONFIG_ANDROID_PMEM
	put_pmem_file(stats_buf->file);
#endif
out1:
	return rc;
}
static int msm_stats_buf_unprepare(struct msm_stats_bufq_ctrl *stats_ctrl,
	enum msm_stats_enum_type stats_type, int buf_idx,
	struct ion_client *client)
{
	int rc = 0;
	struct msm_stats_bufq *bufq = NULL;
	struct msm_stats_meta_buf *stats_buf = NULL;

	D("%s: type : %d, idx : %d\n", __func__, stats_type, buf_idx);
	bufq = stats_ctrl->bufq[stats_type];
	stats_buf = &bufq->bufs[buf_idx];
	if (stats_buf->state == MSM_STATS_BUFFER_STATE_UNUSED) {
		pr_err("%s: need reqbuf first, stats type = %d",
			__func__, stats_type);
		rc = -1;
		goto end;
	}
	if (stats_buf->state == MSM_STATS_BUFFER_STATE_INITIALIZED) {
		D("%s: stats already mapped, no op, stats type = %d",
			__func__, stats_type);
		goto end;
	}
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	ion_unmap_iommu(client, stats_buf->handle,
					CAMERA_DOMAIN, GEN_POOL);
	ion_free(client, stats_buf->handle);
#else
	put_pmem_file(stats_buf->file);
#endif
	if (stats_buf->state == MSM_STATS_BUFFER_STATE_QUEUED) {
		/* buf queued need delete from list */
		D("%s: delete stats buf, type = %d, idx = %d",
		  __func__,  stats_type,  buf_idx);
		list_del_init(&stats_buf->list);
	}
end:
	return rc;
}

static int msm_stats_bufq_flush(struct msm_stats_bufq_ctrl *stats_ctrl,
	enum msm_stats_enum_type stats_type, struct ion_client *client)
{
	int rc = 0;
	int i;
	struct msm_stats_bufq *bufq = NULL;
	struct msm_stats_meta_buf *stats_buf = NULL;

	D("%s: type : %d\n", __func__, stats_type);
	bufq = stats_ctrl->bufq[stats_type];

	for (i = 0; i < bufq->num_bufs; i++) {
		stats_buf = &bufq->bufs[i];
		switch (stats_buf->state) {
		case MSM_STATS_BUFFER_STATE_QUEUED:
			/* buf queued in stats free queue */
			stats_buf->state = MSM_STATS_BUFFER_STATE_PREPARED;
			list_del_init(&stats_buf->list);
			break;
		case MSM_STATS_BUFFER_STATE_DEQUEUED:
			/* if stats buf in VFE reset the state */
			stats_buf->state = MSM_STATS_BUFFER_STATE_PREPARED;
			break;
		case MSM_STATS_BUFFER_STATE_DISPATCHED:
			/* if stats buf in userspace reset the state */
			stats_buf->state = MSM_STATS_BUFFER_STATE_PREPARED;
			break;
		default:
			break;
		}
	}
	return rc;
}

static int msm_stats_dqbuf(struct msm_stats_bufq_ctrl *stats_ctrl,
	enum msm_stats_enum_type stats_type,
	struct msm_stats_meta_buf **pp_stats_buf)
{
	int rc = 0;
	struct msm_stats_bufq *bufq = NULL;
	struct msm_stats_meta_buf *stats_buf = NULL;

	D("%s: type : %d\n", __func__, stats_type);
	*pp_stats_buf = NULL;
	bufq = stats_ctrl->bufq[stats_type];

	list_for_each_entry(stats_buf, &bufq->head, list) {
		if (stats_buf->state == MSM_STATS_BUFFER_STATE_QUEUED) {
			/* found one buf */
			list_del_init(&stats_buf->list);
			*pp_stats_buf = stats_buf;
			break;
		}
	}
	if (!(*pp_stats_buf)) {
		pr_err("%s: no free stats buf, type = %d",
			__func__, stats_type);
		rc = -1;
		return rc;
	}
	stats_buf->state = MSM_STATS_BUFFER_STATE_DEQUEUED;
	return rc;
}


static int msm_stats_querybuf(struct msm_stats_bufq_ctrl *stats_ctrl,
	struct msm_stats_buf_info *info,
	struct msm_stats_meta_buf **pp_stats_buf)
{
	int rc = 0;
	struct msm_stats_bufq *bufq = NULL;

	*pp_stats_buf = NULL;
	D("%s: stats type : %d, buf_idx : %d", __func__, info->type,
		   info->buf_idx);
	bufq = stats_ctrl->bufq[info->type];
	*pp_stats_buf = &bufq->bufs[info->buf_idx];

	return rc;
}

static int msm_stats_qbuf(struct msm_stats_bufq_ctrl *stats_ctrl,
	enum msm_stats_enum_type stats_type,
	int buf_idx)
{
	int rc = 0;
	struct msm_stats_bufq *bufq = NULL;
	struct msm_stats_meta_buf *stats_buf = NULL;
	D("%s: stats type : %d, buf_idx : %d", __func__, stats_type,
		   buf_idx);

	bufq = stats_ctrl->bufq[stats_type];
	if (!bufq) {
		pr_err("%s: null bufq, stats type = %d", __func__, stats_type);
		rc = -1;
		goto end;
	}
	if (buf_idx >= bufq->num_bufs) {
		pr_err("%s: stats type = %d, its idx %d larger than buf count %d",
			   __func__, stats_type, buf_idx, bufq->num_bufs);
		rc = -1;
		goto end;
	}
	stats_buf = &bufq->bufs[buf_idx];
	switch (stats_buf->state) {
	case MSM_STATS_BUFFER_STATE_PREPARED:
	case MSM_STATS_BUFFER_STATE_DEQUEUED:
	case MSM_STATS_BUFFER_STATE_DISPATCHED:
		stats_buf->state = MSM_STATS_BUFFER_STATE_QUEUED;
		list_add_tail(&stats_buf->list, &bufq->head);
		break;
	default:
		pr_err("%s: incorrect state = %d, stats type = %d, cannot qbuf",
			   __func__, stats_buf->state, stats_type);
		rc = -1;
		break;
	}
end:
	return rc;
}

static int msm_stats_buf_dispatch(struct msm_stats_bufq_ctrl *stats_ctrl,
	enum msm_stats_enum_type stats_type,
	unsigned long phy_addr, int *buf_idx,
	void **vaddr, int *fd,
	struct ion_client *client)
{
	int rc = 0;
	int i;
	struct msm_stats_bufq *bufq = NULL;
	struct msm_stats_meta_buf *stats_buf = NULL;
	D("%s: stats type : %d\n", __func__, stats_type);

	*buf_idx = -1;
	*vaddr = NULL;
	*fd = 0;
	bufq = stats_ctrl->bufq[stats_type];
	for (i = 0; i < bufq->num_bufs; i++) {
		if (bufq->bufs[i].paddr == phy_addr) {
			stats_buf = &bufq->bufs[i];
			*buf_idx = i;
			*vaddr = stats_buf->info.vaddr;
			*fd = stats_buf->info.fd;
			break;
		}
	}
	if (!stats_buf) {
		pr_err("%s: no match, phy_addr = 0x%ld, stats_type = %d",
			   __func__, phy_addr, stats_type);
		return -EFAULT;
	}
	switch (stats_buf->state) {
	case MSM_STATS_BUFFER_STATE_DEQUEUED:
		stats_buf->state = MSM_STATS_BUFFER_STATE_DISPATCHED;
		break;
	default:
		pr_err("%s: type = %d, idx = %d, cur_state = %d,\n"
			   "cannot set state to DISPATCHED\n",
			   __func__, stats_type, *buf_idx, stats_buf->state);
		rc = -EFAULT;
		break;
	}
	return rc;
}
static int msm_stats_enqueue_buf(struct msm_stats_bufq_ctrl *stats_ctrl,
	struct msm_stats_buf_info *info, struct ion_client *client)
{
	int rc = 0;
	rc = msm_stats_buf_prepare(stats_ctrl, info, client);
	if (rc < 0) {
		pr_err("%s: buf_prepare failed, rc = %d", __func__, rc);
		return -EINVAL;
	}
	rc = msm_stats_qbuf(stats_ctrl,   info->type, info->buf_idx);
	if (rc < 0) {
		pr_err("%s: msm_stats_qbuf failed, rc = %d", __func__, rc);
		return -EINVAL;
	}
	return rc;
}

int msm_stats_buf_ops_init(struct msm_stats_bufq_ctrl *stats_ctrl,
	struct ion_client *client, struct msm_stats_ops *ops)
{
	ops->stats_ctrl = stats_ctrl;
	ops->client = client;
	ops->enqueue_buf = msm_stats_enqueue_buf;
	ops->qbuf = msm_stats_qbuf;
	ops->dqbuf = msm_stats_dqbuf;
	ops->bufq_flush = msm_stats_bufq_flush;
	ops->buf_unprepare = msm_stats_buf_unprepare;
	ops->buf_prepare = msm_stats_buf_prepare;
	ops->reqbuf = msm_stats_reqbuf;
	ops->querybuf = msm_stats_querybuf;
	ops->dispatch = msm_stats_buf_dispatch;
	ops->stats_ctrl_init = msm_stats_init;
	ops->stats_ctrl_deinit = msm_stats_deinit;
	return 0;
}

