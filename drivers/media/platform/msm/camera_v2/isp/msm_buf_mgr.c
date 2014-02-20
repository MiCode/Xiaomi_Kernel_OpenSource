/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/videodev2.h>
#include <linux/proc_fs.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>


#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>
#include <media/msm_camera.h>
#include <media/msm_isp.h>

#include <mach/iommu.h>

#include "msm.h"
#include "msm_buf_mgr.h"

/*#define CONFIG_MSM_ISP_DBG*/
#undef CDBG
#ifdef CONFIG_MSM_ISP_DBG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

static struct msm_isp_bufq *msm_isp_get_bufq(
	struct msm_isp_buf_mgr *buf_mgr,
	uint32_t bufq_handle)
{
	struct msm_isp_bufq *bufq = NULL;
	uint32_t bufq_index = bufq_handle & 0xFF;
	if (bufq_index > buf_mgr->num_buf_q)
		return bufq;

	bufq = &buf_mgr->bufq[bufq_index];
	if (bufq->bufq_handle == bufq_handle)
		return bufq;

	return NULL;
}

static struct msm_isp_buffer *msm_isp_get_buf_ptr(
	struct msm_isp_buf_mgr *buf_mgr,
	uint32_t bufq_handle, uint32_t buf_index)
{
	struct msm_isp_bufq *bufq = NULL;
	struct msm_isp_buffer *buf_info = NULL;

	bufq = msm_isp_get_bufq(buf_mgr, bufq_handle);
	if (!bufq) {
		pr_err("%s: Invalid bufq\n", __func__);
		return buf_info;
	}

	if (bufq->num_bufs <= buf_index) {
		pr_err("%s: Invalid buf index\n", __func__);
		return buf_info;
	}
	buf_info = &bufq->bufs[buf_index];
	return buf_info;
}

static uint32_t msm_isp_get_buf_handle(
	struct msm_isp_buf_mgr *buf_mgr,
	uint32_t session_id, uint32_t stream_id)
{
	int i;
	if ((buf_mgr->buf_handle_cnt << 8) == 0)
		buf_mgr->buf_handle_cnt++;

	for (i = 0; i < buf_mgr->num_buf_q; i++) {
		if (buf_mgr->bufq[i].session_id == session_id &&
			buf_mgr->bufq[i].stream_id == stream_id)
			return 0;
	}

	for (i = 0; i < buf_mgr->num_buf_q; i++) {
		if (buf_mgr->bufq[i].bufq_handle == 0) {
			memset(&buf_mgr->bufq[i],
				0, sizeof(struct msm_isp_bufq));
			buf_mgr->bufq[i].bufq_handle =
				(++buf_mgr->buf_handle_cnt) << 8 | i;
			return buf_mgr->bufq[i].bufq_handle;
		}
	}
	return 0;
}

static int msm_isp_free_buf_handle(struct msm_isp_buf_mgr *buf_mgr,
	uint32_t bufq_handle)
{
	struct msm_isp_bufq *bufq =
		msm_isp_get_bufq(buf_mgr, bufq_handle);
	if (!bufq)
		return -EINVAL;
	memset(bufq, 0, sizeof(struct msm_isp_bufq));
	return 0;
}

static int msm_isp_prepare_v4l2_buf(struct msm_isp_buf_mgr *buf_mgr,
	struct msm_isp_buffer *buf_info,
	struct v4l2_buffer *v4l2_buf)
{
	int i, rc = -1;
	struct msm_isp_buffer_mapped_info *mapped_info;
	struct buffer_cmd *buf_pending = NULL;

	for (i = 0; i < v4l2_buf->length; i++) {
		mapped_info = &buf_info->mapped_info[i];
		mapped_info->handle =
		ion_import_dma_buf(buf_mgr->client,
			v4l2_buf->m.planes[i].m.userptr);
		if (IS_ERR_OR_NULL(mapped_info->handle)) {
			pr_err("%s: buf has null/error ION handle %p\n",
				__func__, mapped_info->handle);
			goto ion_map_error;
		}
		if (ion_map_iommu(buf_mgr->client, mapped_info->handle,
				buf_mgr->iommu_domain_num, 0, SZ_4K,
				0, &(mapped_info->paddr),
				&(mapped_info->len), 0, 0) < 0) {
			rc = -EINVAL;
			pr_err("%s: cannot map address", __func__);
			ion_free(buf_mgr->client, mapped_info->handle);
			goto ion_map_error;
		}
		mapped_info->paddr += v4l2_buf->m.planes[i].data_offset;
		CDBG("%s: plane: %d addr:%lu\n",
			__func__, i, mapped_info->paddr);

		buf_pending = kzalloc(sizeof(struct buffer_cmd), GFP_ATOMIC);
		if (!buf_pending) {
			pr_err("No free memory for buf_pending\n");
			return rc;
		}

		buf_pending->mapped_info = mapped_info;
		list_add_tail(&buf_pending->list, &buf_mgr->buffer_q);
	}
	buf_info->num_planes = v4l2_buf->length;
	return 0;
ion_map_error:
	for (--i; i >= 0; i--) {
		mapped_info = &buf_info->mapped_info[i];
		ion_unmap_iommu(buf_mgr->client, mapped_info->handle,
		buf_mgr->iommu_domain_num, 0);
		ion_free(buf_mgr->client, mapped_info->handle);
	}
	return rc;
}

static void msm_isp_unprepare_v4l2_buf(
	struct msm_isp_buf_mgr *buf_mgr,
	struct msm_isp_buffer *buf_info)
{
	int i;
	struct msm_isp_buffer_mapped_info *mapped_info;
	struct buffer_cmd *buf_pending = NULL;

	for (i = 0; i < buf_info->num_planes; i++) {
		mapped_info = &buf_info->mapped_info[i];

		list_for_each_entry(buf_pending, &buf_mgr->buffer_q, list) {
			if (!buf_pending)
				break;

			if (buf_pending->mapped_info == mapped_info) {
				ion_unmap_iommu(buf_mgr->client,
					mapped_info->handle,
					buf_mgr->iommu_domain_num, 0);
				ion_free(buf_mgr->client, mapped_info->handle);
				list_del_init(&buf_pending->list);
				kfree(buf_pending);
				break;
			}
		}
	}
	return;
}

static int msm_isp_buf_prepare(struct msm_isp_buf_mgr *buf_mgr,
	struct msm_isp_qbuf_info *info, struct vb2_buffer *vb2_buf)
{
	int rc = -1;
	unsigned long flags;
	struct msm_isp_bufq *bufq = NULL;
	struct msm_isp_buffer *buf_info = NULL;
	struct v4l2_buffer *buf = NULL;
	struct v4l2_plane *plane = NULL;

	buf_info = msm_isp_get_buf_ptr(buf_mgr,
		info->handle, info->buf_idx);
	if (!buf_info) {
		pr_err("Invalid buffer prepare\n");
		return rc;
	}

	bufq = msm_isp_get_bufq(buf_mgr, buf_info->bufq_handle);
	if (!bufq) {
		pr_err("%s: Invalid bufq\n", __func__);
		return rc;
	}

	spin_lock_irqsave(&bufq->bufq_lock, flags);
	if (buf_info->state == MSM_ISP_BUFFER_STATE_DIVERTED) {
		rc = buf_info->state;
		spin_unlock_irqrestore(&bufq->bufq_lock, flags);
		return rc;
	}

	if (buf_info->state != MSM_ISP_BUFFER_STATE_INITIALIZED) {
		pr_err("%s: Invalid buffer state: %d\n",
			__func__, buf_info->state);
		spin_unlock_irqrestore(&bufq->bufq_lock, flags);
		return rc;
	}
	spin_unlock_irqrestore(&bufq->bufq_lock, flags);

	if (vb2_buf) {
		buf = &vb2_buf->v4l2_buf;
		buf_info->vb2_buf = vb2_buf;
	} else {
		buf = &info->buffer;
		plane =
			kzalloc(sizeof(struct v4l2_plane) * buf->length,
				GFP_KERNEL);
		if (!plane) {
			pr_err("%s: Cannot alloc plane: %d\n",
			__func__, buf_info->state);
			return rc;
		}
		if (copy_from_user(plane,
				(void __user *)(buf->m.planes),
			sizeof(struct v4l2_plane) * buf->length)) {
			kfree(plane);
			return rc;
		}
		buf->m.planes = plane;
	}

	rc = msm_isp_prepare_v4l2_buf(buf_mgr, buf_info, buf);
	if (rc < 0) {
		pr_err("%s: Prepare buffer error\n", __func__);
		kfree(plane);
		return rc;
	}
	spin_lock_irqsave(&bufq->bufq_lock, flags);
	buf_info->state = MSM_ISP_BUFFER_STATE_PREPARED;
	spin_unlock_irqrestore(&bufq->bufq_lock, flags);
	kfree(plane);
	return rc;
}

static int msm_isp_buf_unprepare(struct msm_isp_buf_mgr *buf_mgr,
	uint32_t buf_handle)
{
	int rc = -1, i;
	struct msm_isp_bufq *bufq = NULL;
	struct msm_isp_buffer *buf_info = NULL;
	bufq = msm_isp_get_bufq(buf_mgr, buf_handle);
	if (!bufq) {
		pr_err("%s: Invalid bufq\n", __func__);
		return rc;
	}

	for (i = 0; i < bufq->num_bufs; i++) {
		buf_info = msm_isp_get_buf_ptr(buf_mgr, buf_handle, i);
		if (buf_info->state == MSM_ISP_BUFFER_STATE_UNUSED ||
				buf_info->state ==
					MSM_ISP_BUFFER_STATE_INITIALIZED)
			continue;

		if (!BUF_SRC(bufq->stream_id)) {
			if (buf_info->state == MSM_ISP_BUFFER_STATE_DEQUEUED ||
			buf_info->state == MSM_ISP_BUFFER_STATE_DIVERTED)
				buf_mgr->vb2_ops->put_buf(buf_info->vb2_buf,
					bufq->session_id, bufq->stream_id);
		}
		msm_isp_unprepare_v4l2_buf(buf_mgr, buf_info);
	}
	return 0;
}

static int msm_isp_get_buf(struct msm_isp_buf_mgr *buf_mgr, uint32_t id,
	uint32_t bufq_handle, struct msm_isp_buffer **buf_info)
{
	int rc = -1;
	unsigned long flags;
	struct msm_isp_buffer *temp_buf_info;
	struct msm_isp_bufq *bufq = NULL;
	struct vb2_buffer *vb2_buf = NULL;
	struct buffer_cmd *buf_pending = NULL;
	struct msm_isp_buffer_mapped_info *mped_info_tmp1;
	struct msm_isp_buffer_mapped_info *mped_info_tmp2;
	bufq = msm_isp_get_bufq(buf_mgr, bufq_handle);
	if (!bufq) {
		pr_err("%s: Invalid bufq\n", __func__);
		return rc;
	}
	if (!bufq->bufq_handle) {
		pr_err("%s: Invalid bufq handle\n", __func__);
		return rc;
	}

	*buf_info = NULL;
	spin_lock_irqsave(&bufq->bufq_lock, flags);
	if (bufq->buf_type == ISP_SHARE_BUF) {
		list_for_each_entry(temp_buf_info,
			&bufq->share_head, share_list) {
			if (!temp_buf_info->buf_used[id]) {
				temp_buf_info->buf_used[id] = 1;
				temp_buf_info->buf_get_count++;
				if (temp_buf_info->buf_get_count ==
					bufq->buf_client_count)
					list_del_init(
					&temp_buf_info->share_list);
				if (temp_buf_info->buf_reuse_flag) {
					kfree(temp_buf_info);
				} else {
					*buf_info = temp_buf_info;
					rc = 0;
				}
				spin_unlock_irqrestore(
					&bufq->bufq_lock, flags);
				return rc;
			}
		}
	}

	if (BUF_SRC(bufq->stream_id)) {
		list_for_each_entry(temp_buf_info, &bufq->head, list) {
			if (temp_buf_info->state ==
					MSM_ISP_BUFFER_STATE_QUEUED) {

				list_for_each_entry(buf_pending, &buf_mgr->buffer_q, list) {
					if (!buf_pending)
						break;
					mped_info_tmp1 = buf_pending->mapped_info;
					mped_info_tmp2 = &temp_buf_info->mapped_info[0];

					if (mped_info_tmp1 == mped_info_tmp2
						&& (mped_info_tmp1->len == mped_info_tmp2->len)
						&& (mped_info_tmp1->paddr == mped_info_tmp2->paddr)) {
						/* found one buf */
						list_del_init(&temp_buf_info->list);
						*buf_info = temp_buf_info;
						break;
					}
				}
				break;
			}
		}
	} else {
		vb2_buf = buf_mgr->vb2_ops->get_buf(
			bufq->session_id, bufq->stream_id);
		if (vb2_buf) {
			if (vb2_buf->v4l2_buf.index < bufq->num_bufs) {

				list_for_each_entry(buf_pending, &buf_mgr->buffer_q, list) {
					if (!buf_pending)
						break;
					mped_info_tmp1 = buf_pending->mapped_info;
					mped_info_tmp2 =
						&bufq->bufs[vb2_buf->v4l2_buf.index].mapped_info[0];

					if (mped_info_tmp1 == mped_info_tmp2
						&& (mped_info_tmp1->len == mped_info_tmp2->len)
						&& (mped_info_tmp1->paddr == mped_info_tmp2->paddr)) {
						*buf_info = &bufq->bufs[vb2_buf->v4l2_buf.index];
						(*buf_info)->vb2_buf = vb2_buf;
						break;
					}
				}
			} else {
				pr_err("%s: Incorrect buf index %d\n",
					__func__, vb2_buf->v4l2_buf.index);
				rc = -EINVAL;
			}
		}
	}

	if (!(*buf_info)) {
		if (bufq->buf_type == ISP_SHARE_BUF) {
			temp_buf_info = kzalloc(
			   sizeof(struct msm_isp_buffer), GFP_ATOMIC);
			if (temp_buf_info) {
				temp_buf_info->buf_reuse_flag = 1;
				temp_buf_info->buf_used[id] = 1;
				temp_buf_info->buf_get_count = 1;
				list_add_tail(&temp_buf_info->share_list,
							  &bufq->share_head);
			} else
				rc = -ENOMEM;
		}
	} else {
		(*buf_info)->state = MSM_ISP_BUFFER_STATE_DEQUEUED;
		if (bufq->buf_type == ISP_SHARE_BUF) {
			memset((*buf_info)->buf_used, 0,
				   sizeof(uint8_t) * bufq->buf_client_count);
			(*buf_info)->buf_used[id] = 1;
			(*buf_info)->buf_get_count = 1;
			(*buf_info)->buf_put_count = 0;
			(*buf_info)->buf_reuse_flag = 0;
			list_add_tail(&(*buf_info)->share_list,
						  &bufq->share_head);
		}
		rc = 0;
	}
	spin_unlock_irqrestore(&bufq->bufq_lock, flags);
	return rc;
}

static int msm_isp_put_buf(struct msm_isp_buf_mgr *buf_mgr,
	uint32_t bufq_handle, uint32_t buf_index)
{
	int rc = -1;
	unsigned long flags;
	struct msm_isp_bufq *bufq = NULL;
	struct msm_isp_buffer *buf_info = NULL;

	bufq = msm_isp_get_bufq(buf_mgr, bufq_handle);
	if (!bufq) {
		pr_err("%s: Invalid bufq\n", __func__);
		return rc;
	}

	buf_info = msm_isp_get_buf_ptr(buf_mgr, bufq_handle, buf_index);
	if (!buf_info) {
		pr_err("%s: buf not found\n", __func__);
		return rc;
	}

	spin_lock_irqsave(&bufq->bufq_lock, flags);
	switch (buf_info->state) {
	case MSM_ISP_BUFFER_STATE_PREPARED:
	case MSM_ISP_BUFFER_STATE_DEQUEUED:
	case MSM_ISP_BUFFER_STATE_DIVERTED:
		if (BUF_SRC(bufq->stream_id))
			list_add_tail(&buf_info->list, &bufq->head);
		else
			buf_mgr->vb2_ops->put_buf(buf_info->vb2_buf,
				bufq->session_id, bufq->stream_id);
		buf_info->state = MSM_ISP_BUFFER_STATE_QUEUED;
		rc = 0;
		break;
	case MSM_ISP_BUFFER_STATE_DISPATCHED:
		buf_info->state = MSM_ISP_BUFFER_STATE_QUEUED;
		rc = 0;
		break;
	case MSM_ISP_BUFFER_STATE_QUEUED:
		rc = 0;
		break;
	default:
		pr_err("%s: incorrect state = %d",
			__func__, buf_info->state);
		break;
	}
	spin_unlock_irqrestore(&bufq->bufq_lock, flags);

	return rc;
}

static int msm_isp_buf_done(struct msm_isp_buf_mgr *buf_mgr,
	uint32_t bufq_handle, uint32_t buf_index,
	struct timeval *tv, uint32_t frame_id, uint32_t output_format)
{
	int rc = -1;
	unsigned long flags;
	struct msm_isp_bufq *bufq = NULL;
	struct msm_isp_buffer *buf_info = NULL;
	enum msm_isp_buffer_state state;

	bufq = msm_isp_get_bufq(buf_mgr, bufq_handle);
	if (!bufq) {
		pr_err("Invalid bufq\n");
		return rc;
	}

	buf_info = msm_isp_get_buf_ptr(buf_mgr, bufq_handle, buf_index);
	if (!buf_info) {
		pr_err("%s: buf not found\n", __func__);
		return rc;
	}

	spin_lock_irqsave(&bufq->bufq_lock, flags);
	state = buf_info->state;
	spin_unlock_irqrestore(&bufq->bufq_lock, flags);

	if (state == MSM_ISP_BUFFER_STATE_DEQUEUED ||
		state == MSM_ISP_BUFFER_STATE_DIVERTED) {
		spin_lock_irqsave(&bufq->bufq_lock, flags);
		if (bufq->buf_type == ISP_SHARE_BUF) {
			buf_info->buf_put_count++;
			if (buf_info->buf_put_count != ISP_SHARE_BUF_CLIENT) {
				rc = buf_info->buf_put_count;
				spin_unlock_irqrestore(&bufq->bufq_lock, flags);
				return rc;
			}
		}
		buf_info->state = MSM_ISP_BUFFER_STATE_DISPATCHED;
		spin_unlock_irqrestore(&bufq->bufq_lock, flags);
		if ((BUF_SRC(bufq->stream_id))) {
			rc = msm_isp_put_buf(buf_mgr, buf_info->bufq_handle,
						buf_info->buf_idx);
			if (rc < 0) {
				pr_err("%s: Buf put failed\n", __func__);
				return rc;
			}
		} else {
			buf_info->vb2_buf->v4l2_buf.timestamp = *tv;
			buf_info->vb2_buf->v4l2_buf.sequence  = frame_id;
			buf_info->vb2_buf->v4l2_buf.reserved = output_format;
			buf_mgr->vb2_ops->buf_done(buf_info->vb2_buf,
				bufq->session_id, bufq->stream_id);
		}
	}

	return 0;
}

static int msm_isp_flush_buf(struct msm_isp_buf_mgr *buf_mgr,
		uint32_t bufq_handle, enum msm_isp_buffer_flush_t flush_type)
{
	int rc = -1, i;
	unsigned long flags;
	struct msm_isp_bufq *bufq = NULL;
	struct msm_isp_buffer *buf_info = NULL;

	bufq = msm_isp_get_bufq(buf_mgr, bufq_handle);
	if (!bufq) {
		pr_err("Invalid bufq\n");
		return rc;
	}

	for (i = 0; i < bufq->num_bufs; i++) {
		buf_info = msm_isp_get_buf_ptr(buf_mgr, bufq_handle, i);
		if (!buf_info) {
			pr_err("%s: buf not found\n", __func__);
			continue;
		}

		spin_lock_irqsave(&bufq->bufq_lock, flags);
		if (flush_type == MSM_ISP_BUFFER_FLUSH_DIVERTED &&
			buf_info->state == MSM_ISP_BUFFER_STATE_DIVERTED) {
			buf_info->state = MSM_ISP_BUFFER_STATE_QUEUED;
		} else if (flush_type == MSM_ISP_BUFFER_FLUSH_ALL &&
			(buf_info->state == MSM_ISP_BUFFER_STATE_DEQUEUED ||
			buf_info->state == MSM_ISP_BUFFER_STATE_DIVERTED ||
			buf_info->state == MSM_ISP_BUFFER_STATE_DISPATCHED)) {
			buf_info->state = MSM_ISP_BUFFER_STATE_QUEUED;
		}
		spin_unlock_irqrestore(&bufq->bufq_lock, flags);
	}
	return 0;
}

static int msm_isp_buf_divert(struct msm_isp_buf_mgr *buf_mgr,
	uint32_t bufq_handle, uint32_t buf_index,
	struct timeval *tv, uint32_t frame_id)
{
	int rc = -1;
	unsigned long flags;
	struct msm_isp_bufq *bufq = NULL;
	struct msm_isp_buffer *buf_info = NULL;

	bufq = msm_isp_get_bufq(buf_mgr, bufq_handle);
	if (!bufq) {
		pr_err("Invalid bufq\n");
		return rc;
	}

	buf_info = msm_isp_get_buf_ptr(buf_mgr, bufq_handle, buf_index);
	if (!buf_info) {
		pr_err("%s: buf not found\n", __func__);
		return rc;
	}

	spin_lock_irqsave(&bufq->bufq_lock, flags);
	if (bufq->buf_type == ISP_SHARE_BUF) {
		buf_info->buf_put_count++;
		if (buf_info->buf_put_count != ISP_SHARE_BUF_CLIENT) {
			rc = buf_info->buf_put_count;
			spin_unlock_irqrestore(&bufq->bufq_lock, flags);
			return rc;
		}
	}

	if (buf_info->state == MSM_ISP_BUFFER_STATE_DEQUEUED) {
		buf_info->state = MSM_ISP_BUFFER_STATE_DIVERTED;
		buf_info->tv = tv;
		buf_info->frame_id = frame_id;
	}
	spin_unlock_irqrestore(&bufq->bufq_lock, flags);

	return 0;
}

static int msm_isp_buf_enqueue(struct msm_isp_buf_mgr *buf_mgr,
	struct msm_isp_qbuf_info *info)
{
	int rc = -1, buf_state;
	struct msm_isp_bufq *bufq = NULL;
	struct msm_isp_buffer *buf_info = NULL;
	buf_state = msm_isp_buf_prepare(buf_mgr, info, NULL);
	if (buf_state < 0) {
		pr_err("%s: Buf prepare failed\n", __func__);
		return -EINVAL;
	}

	if (buf_state == MSM_ISP_BUFFER_STATE_DIVERTED) {
		buf_info = msm_isp_get_buf_ptr(buf_mgr,
						info->handle, info->buf_idx);
		if (info->dirty_buf) {
			rc = msm_isp_put_buf(buf_mgr,
				info->handle, info->buf_idx);
		} else {
			if (BUF_SRC(bufq->stream_id))
				pr_err("%s: Invalid native buffer state\n",
					__func__);
			else
				rc = msm_isp_buf_done(buf_mgr,
					info->handle, info->buf_idx,
					buf_info->tv, buf_info->frame_id, 0);
		}
	} else {
		bufq = msm_isp_get_bufq(buf_mgr, info->handle);
		if (bufq && BUF_SRC(bufq->stream_id)) {
			rc = msm_isp_put_buf(buf_mgr,
					info->handle, info->buf_idx);
			if (rc < 0) {
				pr_err("%s: Buf put failed\n", __func__);
				return rc;
			}
		}
	}
	return rc;
}

static int msm_isp_get_bufq_handle(struct msm_isp_buf_mgr *buf_mgr,
	uint32_t session_id, uint32_t stream_id)
{
	int i;
	for (i = 0; i < buf_mgr->num_buf_q; i++) {
		if (buf_mgr->bufq[i].session_id == session_id &&
			buf_mgr->bufq[i].stream_id == stream_id) {
			return buf_mgr->bufq[i].bufq_handle;
		}
	}
	return 0;
}

static int msm_isp_request_bufq(struct msm_isp_buf_mgr *buf_mgr,
	struct msm_isp_buf_request *buf_request)
{
	int rc = -1, i;
	struct msm_isp_bufq *bufq = NULL;
	CDBG("%s: E\n", __func__);

	if (!buf_request->num_buf) {
		pr_err("Invalid buffer request\n");
		return rc;
	}

	buf_request->handle = msm_isp_get_buf_handle(buf_mgr,
		buf_request->session_id, buf_request->stream_id);
	if (!buf_request->handle) {
		pr_err("Invalid buffer handle\n");
		return rc;
	}

	bufq = msm_isp_get_bufq(buf_mgr, buf_request->handle);
	if (!bufq) {
		pr_err("Invalid buffer queue\n");
		return rc;
	}

	bufq->bufs = kzalloc(sizeof(struct msm_isp_buffer) *
		buf_request->num_buf, GFP_KERNEL);
	if (!bufq->bufs) {
		pr_err("No free memory for buf info\n");
		msm_isp_free_buf_handle(buf_mgr, buf_request->handle);
		return rc;
	}

	spin_lock_init(&bufq->bufq_lock);
	bufq->bufq_handle = buf_request->handle;
	bufq->session_id = buf_request->session_id;
	bufq->stream_id = buf_request->stream_id;
	bufq->num_bufs = buf_request->num_buf;
	bufq->buf_type = buf_request->buf_type;
	if (bufq->buf_type == ISP_SHARE_BUF)
		bufq->buf_client_count = ISP_SHARE_BUF_CLIENT;
	INIT_LIST_HEAD(&bufq->head);
	INIT_LIST_HEAD(&bufq->share_head);
	for (i = 0; i < buf_request->num_buf; i++) {
		bufq->bufs[i].state = MSM_ISP_BUFFER_STATE_INITIALIZED;
		bufq->bufs[i].bufq_handle = bufq->bufq_handle;
		bufq->bufs[i].buf_idx = i;
	}

	return 0;
}

static int msm_isp_release_bufq(struct msm_isp_buf_mgr *buf_mgr,
	uint32_t bufq_handle)
{
	struct msm_isp_bufq *bufq = NULL;
	int rc = -1;
	bufq = msm_isp_get_bufq(buf_mgr, bufq_handle);
	if (!bufq) {
		pr_err("Invalid bufq release\n");
		return rc;
	}

	msm_isp_buf_unprepare(buf_mgr, bufq_handle);

	kfree(bufq->bufs);
	msm_isp_free_buf_handle(buf_mgr, bufq_handle);
	return 0;
}

static void msm_isp_release_all_bufq(
	struct msm_isp_buf_mgr *buf_mgr)
{
	struct msm_isp_bufq *bufq = NULL;
	int i;
	for (i = 0; i < buf_mgr->num_buf_q; i++) {
		bufq = &buf_mgr->bufq[i];
		if (!bufq->bufq_handle)
			continue;
		msm_isp_buf_unprepare(buf_mgr, bufq->bufq_handle);

		kfree(bufq->bufs);
		msm_isp_free_buf_handle(buf_mgr, bufq->bufq_handle);
	}
}

static void msm_isp_register_ctx(struct msm_isp_buf_mgr *buf_mgr,
	struct device **iommu_ctx, int num_iommu_ctx)
{
	int i;
	buf_mgr->num_iommu_ctx = num_iommu_ctx;
	for (i = 0; i < num_iommu_ctx; i++)
		buf_mgr->iommu_ctx[i] = iommu_ctx[i];
}

static int msm_isp_attach_ctx(struct msm_isp_buf_mgr *buf_mgr)
{
	int rc, i;
	for (i = 0; i < buf_mgr->num_iommu_ctx; i++) {
		rc = iommu_attach_device(buf_mgr->iommu_domain,
			buf_mgr->iommu_ctx[i]);
		if (rc) {
			pr_err("%s: Iommu attach error\n", __func__);
			return -EINVAL;
		}
	}
	return 0;
}

static void msm_isp_detach_ctx(struct msm_isp_buf_mgr *buf_mgr)
{
	int i;
	for (i = 0; i < buf_mgr->num_iommu_ctx; i++)
		iommu_detach_device(buf_mgr->iommu_domain,
			buf_mgr->iommu_ctx[i]);
}

static int msm_isp_init_isp_buf_mgr(
	struct msm_isp_buf_mgr *buf_mgr,
	const char *ctx_name, uint16_t num_buf_q)
{
	int rc = -1;
	if (buf_mgr->open_count++)
		return 0;

	if (!num_buf_q) {
		pr_err("Invalid buffer queue number\n");
		return rc;
	}
	CDBG("%s: E\n", __func__);

	msm_isp_attach_ctx(buf_mgr);
	INIT_LIST_HEAD(&buf_mgr->buffer_q);
	buf_mgr->num_buf_q = num_buf_q;
	buf_mgr->bufq =
		kzalloc(sizeof(struct msm_isp_bufq) * num_buf_q,
		GFP_KERNEL);
	if (!buf_mgr->bufq) {
		pr_err("Bufq malloc error\n");
		goto bufq_error;
	}
	buf_mgr->client = msm_ion_client_create(-1, ctx_name);
	buf_mgr->buf_handle_cnt = 0;
	return 0;
bufq_error:
	return rc;
}

static int msm_isp_deinit_isp_buf_mgr(
	struct msm_isp_buf_mgr *buf_mgr)
{
	if (--buf_mgr->open_count)
		return 0;
	msm_isp_release_all_bufq(buf_mgr);
	ion_client_destroy(buf_mgr->client);
	kfree(buf_mgr->bufq);
	buf_mgr->num_buf_q = 0;
	msm_isp_detach_ctx(buf_mgr);
	return 0;
}

int msm_isp_proc_buf_cmd(struct msm_isp_buf_mgr *buf_mgr,
	unsigned int cmd, void *arg)
{
	switch (cmd) {
	case VIDIOC_MSM_ISP_REQUEST_BUF: {
		struct msm_isp_buf_request *buf_req = arg;
		buf_mgr->ops->request_buf(buf_mgr, buf_req);
		break;
	}
	case VIDIOC_MSM_ISP_ENQUEUE_BUF: {
		struct msm_isp_qbuf_info *qbuf_info = arg;
		buf_mgr->ops->enqueue_buf(buf_mgr, qbuf_info);
		break;
	}
	case VIDIOC_MSM_ISP_RELEASE_BUF: {
		struct msm_isp_buf_request *buf_req = arg;
		buf_mgr->ops->release_buf(buf_mgr, buf_req->handle);
		break;
	}
	}
	return 0;
}

static struct msm_isp_buf_ops isp_buf_ops = {
	.request_buf = msm_isp_request_bufq,
	.enqueue_buf = msm_isp_buf_enqueue,
	.release_buf = msm_isp_release_bufq,
	.get_bufq_handle = msm_isp_get_bufq_handle,
	.get_buf = msm_isp_get_buf,
	.put_buf = msm_isp_put_buf,
	.flush_buf = msm_isp_flush_buf,
	.buf_done = msm_isp_buf_done,
	.buf_divert = msm_isp_buf_divert,
	.register_ctx = msm_isp_register_ctx,
	.buf_mgr_init = msm_isp_init_isp_buf_mgr,
	.buf_mgr_deinit = msm_isp_deinit_isp_buf_mgr,
};

int msm_isp_create_isp_buf_mgr(
	struct msm_isp_buf_mgr *buf_mgr,
	struct msm_sd_req_vb2_q *vb2_ops,
	struct msm_iova_layout *iova_layout)
{
	int rc = 0;
	if (buf_mgr->init_done)
		return rc;

	buf_mgr->iommu_domain_num = msm_register_domain(iova_layout);
	if (buf_mgr->iommu_domain_num < 0) {
		pr_err("%s: Invalid iommu domain number\n", __func__);
		rc = -1;
		goto iommu_domain_error;
	}

	buf_mgr->iommu_domain = msm_get_iommu_domain(
		buf_mgr->iommu_domain_num);
	if (!buf_mgr->iommu_domain) {
		pr_err("%s: Invalid iommu domain\n", __func__);
		rc = -1;
		goto iommu_domain_error;
	}

	buf_mgr->ops = &isp_buf_ops;
	buf_mgr->vb2_ops = vb2_ops;
	buf_mgr->init_done = 1;
	buf_mgr->open_count = 0;
	return 0;
iommu_domain_error:
	return rc;
}
