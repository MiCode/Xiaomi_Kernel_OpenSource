// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include "msm_cvp.h"
#include "cvp_hfi.h"
#include <synx_api.h>
#include "cvp_core_hfi.h"
#include "cvp_hfi_helper.h"

struct cvp_power_level {
	unsigned long core_sum;
	unsigned long op_core_sum;
	unsigned long bw_sum;
};

void print_internal_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct msm_cvp_internal_buffer *cbuf)
{
	if (!(tag & msm_cvp_debug) || !inst || !cbuf)
		return;

	if (cbuf->smem.dma_buf) {
		dprintk(tag,
		"%s: %x : idx %2d fd %d off %d %s size %d flags %#x iova %#x",
		str, hash32_ptr(inst->session), cbuf->buf.index, cbuf->buf.fd,
		cbuf->buf.offset, cbuf->smem.dma_buf->name, cbuf->buf.size,
		cbuf->buf.flags, cbuf->smem.device_addr);
	} else {
		dprintk(tag,
		"%s: %x : idx %2d fd %d off %d size %d flags %#x iova %#x",
		str, hash32_ptr(inst->session), cbuf->buf.index, cbuf->buf.fd,
		cbuf->buf.offset, cbuf->buf.size, cbuf->buf.flags,
		cbuf->smem.device_addr);
	}
}

static enum hal_buffer get_hal_buftype(const char *str, unsigned int type)
{
	enum hal_buffer buftype = HAL_BUFFER_NONE;

	if (type == CVP_KMD_BUFTYPE_INPUT)
		buftype = HAL_BUFFER_INPUT;
	else if (type == CVP_KMD_BUFTYPE_OUTPUT)
		buftype = HAL_BUFFER_OUTPUT;
	else if (type == CVP_KMD_BUFTYPE_INTERNAL_1)
		buftype = HAL_BUFFER_INTERNAL_SCRATCH_1;
	else if (type == CVP_KMD_BUFTYPE_INTERNAL_2)
		buftype = HAL_BUFFER_INTERNAL_SCRATCH_1;
	else
		dprintk(CVP_ERR, "%s: unknown buffer type %#x\n",
			str, type);

	return buftype;
}

static int msm_cvp_get_session_info(struct msm_cvp_inst *inst,
		struct cvp_kmd_session_info *session)
{
	int rc = 0;
	struct msm_cvp_inst *s;

	if (!inst || !inst->core || !session) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	s->cur_cmd_type = CVP_KMD_GET_SESSION_INFO;
	session->session_id = hash32_ptr(inst->session);
	dprintk(CVP_DBG, "%s: id 0x%x\n", __func__, session->session_id);

	s->cur_cmd_type = 0;
	cvp_put_inst(s);
	return rc;
}

static int msm_cvp_session_get_iova_addr_d(
	struct msm_cvp_inst *inst,
	struct msm_cvp_internal_buffer **cbuf_ptr,
	unsigned int search_fd, unsigned int search_size,
	unsigned int *iova,
	unsigned int *iova_size)
{
	bool found = false;
	struct msm_cvp_internal_buffer *cbuf;

	mutex_lock(&inst->cvpcpubufs.lock);
	list_for_each_entry(cbuf, &inst->cvpcpubufs.list, list) {
		if (cbuf->buf.fd == search_fd) {
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->cvpcpubufs.lock);
	if (!found)
		return -ENOENT;

	if (search_size != cbuf->buf.size) {
		dprintk(CVP_ERR,
			"%s: invalid size received fd = %d, size 0x%x 0x%x\n",
			__func__, search_fd, search_size, cbuf->buf.size);
		return -EINVAL;
	}
	*iova = cbuf->smem.device_addr;
	*iova_size = cbuf->buf.size;

	if (cbuf_ptr)
		*cbuf_ptr = cbuf;

	return 0;
}

static int msm_cvp_session_get_iova_addr(
	struct msm_cvp_inst *inst,
	struct cvp_buf_type *in_buf,
	unsigned int *iova)
{
	struct msm_cvp_internal_buffer *cbuf;

	if (!inst | !iova) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&inst->cvpcpubufs.lock);
	list_for_each_entry(cbuf, &inst->cvpcpubufs.list, list) {
		if (cbuf->smem.dma_buf == in_buf->dbuf &&
			cbuf->buf.size == in_buf->size &&
			cbuf->buf.offset == in_buf->offset) {
			*iova = cbuf->smem.device_addr + cbuf->buf.offset;
			print_internal_buffer(CVP_DBG, "found", inst, cbuf);
			mutex_unlock(&inst->cvpcpubufs.lock);
			return 0;
		}
	}
	mutex_unlock(&inst->cvpcpubufs.lock);
	*iova = 0;

	return -ENOENT;
}

static int msm_cvp_map_buf_dsp(struct msm_cvp_inst *inst,
	struct cvp_kmd_buffer *buf)
{
	int rc = 0;
	bool found;
	struct msm_cvp_internal_buffer *cbuf;
	struct cvp_hal_session *session;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (buf->offset) {
		dprintk(CVP_ERR,
			"%s: offset is deprecated, set to 0.\n",
			__func__);
		return -EINVAL;
	}

	session = (struct cvp_hal_session *)inst->session;
	mutex_lock(&inst->cvpdspbufs.lock);
	found = false;
	list_for_each_entry(cbuf, &inst->cvpdspbufs.list, list) {
		if (cbuf->buf.fd == buf->fd) {
			if (cbuf->buf.size != buf->size) {
				dprintk(CVP_ERR, "%s: buf size mismatch\n",
					__func__);
				mutex_unlock(&inst->cvpdspbufs.lock);
				return -EINVAL;
			}
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->cvpdspbufs.lock);
	if (found) {
		print_internal_buffer(CVP_ERR, "duplicate", inst, cbuf);
		return -EINVAL;
	}

	cbuf = kmem_cache_zalloc(cvp_driver->internal_buf_cache, GFP_KERNEL);
	if (!cbuf) {
		return -ENOMEM;
	}

	memcpy(&cbuf->buf, buf, sizeof(struct cvp_kmd_buffer));
	cbuf->smem.buffer_type = get_hal_buftype(__func__, buf->type);
	cbuf->smem.fd = buf->fd;
	cbuf->smem.offset = buf->offset;
	cbuf->smem.size = buf->size;
	cbuf->smem.flags = buf->flags;

	rc = msm_cvp_smem_map_dma_buf(inst, &cbuf->smem);
	if (rc) {
		print_client_buffer(CVP_ERR, "map failed", inst, buf);
		goto exit;
	}

	if (buf->index) {
		rc = cvp_dsp_register_buffer(hash32_ptr(session), buf->fd,
			cbuf->smem.dma_buf->size, buf->size, buf->offset,
			buf->index, (uint32_t)cbuf->smem.device_addr);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: failed dsp registration for fd=%d rc=%d",
				__func__, buf->fd, rc);
			goto exit;
		}
	} else {
		dprintk(CVP_ERR, "%s: buf index is 0 fd=%d",
				__func__, buf->fd);
		rc = -EINVAL;
		goto exit;
	}

	mutex_lock(&inst->cvpdspbufs.lock);
	list_add_tail(&cbuf->list, &inst->cvpdspbufs.list);
	mutex_unlock(&inst->cvpdspbufs.lock);

	return rc;

exit:
	if (cbuf->smem.device_addr)
		msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
	kmem_cache_free(cvp_driver->internal_buf_cache, cbuf);
	cbuf = NULL;

	return rc;
}

static int msm_cvp_unmap_buf_dsp(struct msm_cvp_inst *inst,
	struct cvp_kmd_buffer *buf)
{
	int rc = 0;
	bool found;
	struct msm_cvp_internal_buffer *cbuf;
	struct cvp_hal_session *session;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	session = (struct cvp_hal_session *)inst->session;
	if (!session) {
		dprintk(CVP_ERR, "%s: invalid session\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&inst->cvpdspbufs.lock);
	found = false;
	list_for_each_entry(cbuf, &inst->cvpdspbufs.list, list) {
		if (cbuf->buf.fd == buf->fd) {
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->cvpdspbufs.lock);
	if (!found) {
		print_client_buffer(CVP_ERR, "invalid", inst, buf);
		return -EINVAL;
	}

	if (buf->index) {
		rc = cvp_dsp_deregister_buffer(hash32_ptr(session), buf->fd,
			cbuf->smem.dma_buf->size, buf->size, buf->offset,
			buf->index, (uint32_t)cbuf->smem.device_addr);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: failed dsp deregistration fd=%d rc=%d",
				__func__, buf->fd, rc);
			return rc;
		}
	}

	if (cbuf->smem.device_addr)
		msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);

	mutex_lock(&inst->cvpdspbufs.lock);
	list_del(&cbuf->list);
	mutex_unlock(&inst->cvpdspbufs.lock);

	kmem_cache_free(cvp_driver->internal_buf_cache, cbuf);
	return rc;
}

static int msm_cvp_map_buf_cpu_d(struct msm_cvp_inst *inst,
	unsigned int fd,
	unsigned int size,
	struct msm_cvp_internal_buffer **cbuf_ptr)
{
	int rc = 0;
	bool found;
	struct msm_cvp_internal_buffer *cbuf;

	if (!inst || !inst->core || !cbuf_ptr) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&inst->cvpcpubufs.lock);
	found = false;
	list_for_each_entry(cbuf, &inst->cvpcpubufs.list, list) {
		if (cbuf->buf.fd == fd) {
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->cvpcpubufs.lock);
	if (found) {
		print_internal_buffer(CVP_ERR, "duplicate", inst, cbuf);
		return -EINVAL;
	}

	cbuf = kmem_cache_zalloc(cvp_driver->internal_buf_cache, GFP_KERNEL);
	if (!cbuf)
		return -ENOMEM;

	cbuf->buf.fd = fd;
	cbuf->buf.size = size;
	/* HFI doesn't have buffer type, set it as HAL_BUFFER_INPUT */
	cbuf->smem.buffer_type = HAL_BUFFER_INPUT;
	cbuf->smem.fd = cbuf->buf.fd;
	cbuf->smem.size = cbuf->buf.size;
	cbuf->smem.flags = 0;
	cbuf->smem.offset = 0;

	rc = msm_cvp_smem_map_dma_buf(inst, &cbuf->smem);
	if (rc) {
		print_internal_buffer(CVP_ERR, "map failed", inst, cbuf);
		goto exit;
	}

	mutex_lock(&inst->cvpcpubufs.lock);
	list_add_tail(&cbuf->list, &inst->cvpcpubufs.list);
	mutex_unlock(&inst->cvpcpubufs.lock);

	*cbuf_ptr = cbuf;

	return rc;

exit:
	if (cbuf->smem.device_addr)
		msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
	kmem_cache_free(cvp_driver->internal_buf_cache, cbuf);
	cbuf = NULL;

	return rc;
}

static void __msm_cvp_cache_operations(struct msm_cvp_internal_buffer *cbuf)
{
	enum smem_cache_ops cache_op;

	if (!cbuf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return;
	}

	switch (cbuf->buf.type) {
	case CVP_KMD_BUFTYPE_INPUT:
		cache_op = SMEM_CACHE_CLEAN;
		break;
	case CVP_KMD_BUFTYPE_OUTPUT:
		cache_op = SMEM_CACHE_INVALIDATE;
		break;
	default:
		cache_op = SMEM_CACHE_CLEAN_INVALIDATE;
	}

	msm_cvp_smem_cache_operations(cbuf->smem.dma_buf, cache_op,
				cbuf->buf.offset, cbuf->buf.size);
}

static int msm_cvp_map_buf_user_persist(struct msm_cvp_inst *inst,
					struct cvp_buf_type *in_buf,
					u32 *iova)
{
	int rc = 0;
	struct cvp_internal_buf *cbuf;
	struct dma_buf *dma_buf;

	if (!inst || !iova) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (in_buf->fd >= 0) {
		dma_buf = msm_cvp_smem_get_dma_buf(in_buf->fd);
		if (!dma_buf) {
			dprintk(CVP_ERR, "%s: Invalid fd=%d", __func__,
				in_buf->fd);
			return -EINVAL;
		}
		in_buf->dbuf = dma_buf;
		msm_cvp_smem_put_dma_buf(dma_buf);
	}

	rc = msm_cvp_session_get_iova_addr(inst, in_buf, iova);
	if (!rc && *iova != 0)
		return 0;
	cbuf = kzalloc(sizeof(*cbuf), GFP_KERNEL);
	if (!cbuf)
		return -ENOMEM;

	cbuf->smem.buffer_type = in_buf->flags;
	cbuf->smem.fd = in_buf->fd;
	cbuf->smem.size = in_buf->size;
	cbuf->smem.flags = 0;
	cbuf->smem.offset = 0;
	cbuf->smem.dma_buf = in_buf->dbuf;
	cbuf->buffer_ownership = CLIENT;

	rc = msm_cvp_smem_map_dma_buf(inst, &cbuf->smem);
	if (rc) {
		dprintk(CVP_ERR,
		"%s: %x : fd %d size %d",
		"map persist failed", hash32_ptr(inst->session), cbuf->smem.fd,
		cbuf->smem.size);
		goto exit;
	}

	/* Assign mapped dma_buf back because it could be zero previously */
	in_buf->dbuf = cbuf->smem.dma_buf;

	mutex_lock(&inst->persistbufs.lock);
	list_add_tail(&cbuf->list, &inst->persistbufs.list);
	mutex_unlock(&inst->persistbufs.lock);

	*iova = cbuf->smem.device_addr;

	dprintk(CVP_DBG,
	"%s: %x : fd %d %s size %d", "map persist", hash32_ptr(inst->session),
	cbuf->smem.fd, cbuf->smem.dma_buf->name, cbuf->smem.size);
	return rc;

exit:
	kfree(cbuf);
	cbuf = NULL;

	return rc;
}

static int msm_cvp_map_buf_cpu(struct msm_cvp_inst *inst,
				struct cvp_buf_type *in_buf,
				u32 *iova,
				struct msm_cvp_frame *frame)
{
	int rc = 0;
	struct msm_cvp_internal_buffer *cbuf;
	struct msm_cvp_frame_buf *frame_buf;
	struct dma_buf *dma_buf;

	if (!inst || !iova || !frame) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (in_buf->fd >= 0) {
		dma_buf = msm_cvp_smem_get_dma_buf(in_buf->fd);
		if (!dma_buf) {
			dprintk(CVP_ERR, "%s: Invalid fd=%d", __func__,
				in_buf->fd);
			return -EINVAL;
		}
		in_buf->dbuf = dma_buf;
		msm_cvp_smem_put_dma_buf(dma_buf);
	}

	rc = msm_cvp_session_get_iova_addr(inst, in_buf, iova);
	if (!rc && *iova != 0)
		return 0;

	cbuf = kmem_cache_zalloc(cvp_driver->internal_buf_cache, GFP_KERNEL);
	if (!cbuf)
		return -ENOMEM;

	cbuf->buf.fd = in_buf->fd;
	cbuf->buf.size = in_buf->size;
	cbuf->buf.offset = in_buf->offset;
	cbuf->buf.flags = in_buf->flags;
	cbuf->buf.type = CVP_KMD_BUFTYPE_INPUT | CVP_KMD_BUFTYPE_OUTPUT;

	cbuf->smem.buffer_type = in_buf->flags;
	cbuf->smem.fd = cbuf->buf.fd;
	cbuf->smem.size = cbuf->buf.size;
	cbuf->smem.flags = 0;
	cbuf->smem.offset = in_buf->offset;
	cbuf->smem.dma_buf = in_buf->dbuf;

	rc = msm_cvp_smem_map_dma_buf(inst, &cbuf->smem);
	if (rc) {
		print_internal_buffer(CVP_ERR, "map failed", inst, cbuf);
		goto exit;
	}

	/* Assign mapped dma_buf back because it could be zero previously */
	in_buf->dbuf = cbuf->smem.dma_buf;

	mutex_lock(&inst->cvpcpubufs.lock);
	list_add_tail(&cbuf->list, &inst->cvpcpubufs.list);
	mutex_unlock(&inst->cvpcpubufs.lock);

	__msm_cvp_cache_operations(cbuf);

	*iova = cbuf->smem.device_addr + cbuf->buf.offset;

	frame_buf = kmem_cache_zalloc(cvp_driver->frame_buf_cache, GFP_KERNEL);
	if (!frame_buf) {
		rc = -ENOMEM;
		goto exit2;
	}

	memcpy(&frame_buf->buf, in_buf, sizeof(frame_buf->buf));

	mutex_lock(&frame->bufs.lock);
	list_add_tail(&frame_buf->list, &frame->bufs.list);
	mutex_unlock(&frame->bufs.lock);

	print_internal_buffer(CVP_DBG, "map", inst, cbuf);
	return rc;

exit2:
	if (cbuf->smem.device_addr)
		msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
	mutex_lock(&inst->cvpcpubufs.lock);
	list_del(&cbuf->list);
	mutex_unlock(&inst->cvpcpubufs.lock);
exit:
	kmem_cache_free(cvp_driver->internal_buf_cache, cbuf);
	cbuf = NULL;

	return rc;
}

static void __unmap_buf(struct msm_cvp_inst *inst,
		struct msm_cvp_frame_buf *frame_buf)
{
	struct msm_cvp_internal_buffer *cbuf, *dummy;
	struct cvp_buf_type *buf;

	if (!inst || !frame_buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return;
	}

	buf = &frame_buf->buf;
	mutex_lock(&inst->cvpcpubufs.lock);
	list_for_each_entry_safe(cbuf, dummy, &inst->cvpcpubufs.list, list) {
		if (cbuf->smem.dma_buf == buf->dbuf &&
			cbuf->buf.size == buf->size &&
			cbuf->buf.offset == buf->offset) {
			list_del(&cbuf->list);
			print_internal_buffer(CVP_DBG, "unmap", inst, cbuf);
			msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
			kmem_cache_free(cvp_driver->internal_buf_cache, cbuf);
			break;
		}
	}
	mutex_unlock(&inst->cvpcpubufs.lock);
}

void msm_cvp_unmap_buf_cpu(struct msm_cvp_inst *inst, u64 ktid)
{
	struct msm_cvp_frame *frame, *dummy1;
	struct msm_cvp_frame_buf *frame_buf, *dummy2;
	bool found;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return;
	}

	dprintk(CVP_DBG, "%s: unmap frame %llu\n", __func__, ktid);
	found = false;
	mutex_lock(&inst->frames.lock);
	list_for_each_entry_safe(frame, dummy1, &inst->frames.list, list) {
		if (frame->ktid == ktid) {
			found = true;
			list_del(&frame->list);
			mutex_lock(&frame->bufs.lock);
			list_for_each_entry_safe(frame_buf, dummy2,
						&frame->bufs.list, list) {
				list_del(&frame_buf->list);
				__unmap_buf(inst, frame_buf);
				kmem_cache_free(cvp_driver->frame_buf_cache,
						frame_buf);
			}
			mutex_unlock(&frame->bufs.lock);
			DEINIT_MSM_CVP_LIST(&frame->bufs);
			kmem_cache_free(cvp_driver->frame_cache, frame);
			break;
		}
	}
	mutex_unlock(&inst->frames.lock);

	if (!found) {
		dprintk(CVP_WARN, "%s frame %#llx not found!\n",
				__func__, ktid);
	}
}

static bool _cvp_msg_pending(struct msm_cvp_inst *inst,
			struct cvp_session_queue *sq,
			struct cvp_session_msg **msg)
{
	struct cvp_session_msg *mptr = NULL;
	bool result = false;

	spin_lock(&sq->lock);
	if (sq->state != QUEUE_ACTIVE) {
		/* The session is being deleted */
		spin_unlock(&sq->lock);
		*msg = NULL;
		return true;
	}
	result = list_empty(&sq->msgs);
	if (!result) {
		mptr =
		list_first_entry(&sq->msgs, struct cvp_session_msg, node);
		list_del_init(&mptr->node);
		sq->msg_count--;
	}
	spin_unlock(&sq->lock);
	*msg = mptr;
	return !result;
}


static int msm_cvp_session_receive_hfi(struct msm_cvp_inst *inst,
			struct cvp_kmd_hfi_packet *out_pkt)
{
	unsigned long wait_time;
	struct cvp_session_msg *msg = NULL;
	struct cvp_session_queue *sq;
	struct cvp_kmd_session_control *sc;
	struct msm_cvp_inst *s;
	int rc = 0;
	u32 version;

	if (!inst) {
		dprintk(CVP_ERR, "%s invalid session\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	s->cur_cmd_type = CVP_KMD_RECEIVE_MSG_PKT;
	sq = &inst->session_queue;
	sc = (struct cvp_kmd_session_control *)out_pkt;

	wait_time = msecs_to_jiffies(CVP_MAX_WAIT_TIME);

	if (wait_event_timeout(sq->wq,
		_cvp_msg_pending(inst, sq, &msg), wait_time) == 0) {
		dprintk(CVP_WARN, "session queue wait timeout\n");
		rc = -ETIMEDOUT;
		goto exit;
	}

	version = (get_hfi_version() & HFI_VERSION_MINOR_MASK)
				>> HFI_VERSION_MINOR_SHIFT;

	if (msg == NULL) {
		dprintk(CVP_WARN,
			"%s: session deleted, queue state %d, msg cnt %d\n",
			__func__, inst->session_queue.state,
			inst->session_queue.msg_count);

		if (inst->state >= MSM_CVP_CLOSE_DONE ||
				sq->state != QUEUE_ACTIVE) {
			rc = -ECONNRESET;
			goto exit;
		}

		msm_cvp_comm_kill_session(inst);
	} else {
		if (version >= 1) {
			u64 ktid;
			u32 kdata1, kdata2;

			kdata1 = msg->pkt.client_data.kdata1;
			kdata2 = msg->pkt.client_data.kdata2;
			ktid = ((u64)kdata2 << 32) | kdata1;
			msm_cvp_unmap_buf_cpu(inst, ktid);
		}

		memcpy(out_pkt, &msg->pkt,
			sizeof(struct cvp_hfi_msg_session_hdr));
		kmem_cache_free(cvp_driver->msg_cache, msg);
	}

exit:
	s->cur_cmd_type = 0;
	cvp_put_inst(inst);
	return rc;
}

static int msm_cvp_map_user_persist(struct msm_cvp_inst *inst,
	struct cvp_kmd_hfi_packet *in_pkt,
	unsigned int offset, unsigned int buf_num)
{
	struct cvp_buf_desc *buf_ptr;
	struct cvp_buf_type *new_buf;
	int i, rc = 0;
	unsigned int iova;

	if (!offset || !buf_num)
		return 0;

	for (i = 0; i < buf_num; i++) {
		buf_ptr = (struct cvp_buf_desc *)&in_pkt->pkt_data[offset];

		offset += sizeof(*new_buf) >> 2;
		new_buf = (struct cvp_buf_type *)buf_ptr;

		/*
		 * Make sure fd or dma_buf field doesn't have any
		 * garbage value.
		 */
		if (inst->session_type == MSM_CVP_USER) {
			new_buf->dbuf = 0;
		} else if (inst->session_type == MSM_CVP_KERNEL) {
			new_buf->fd = 0;
		} else if (inst->session_type >= MSM_CVP_UNKNOWN) {
			dprintk(CVP_ERR,
				"%s: unknown session type %d\n",
				__func__, inst->session_type);
			return -EINVAL;
		}

		if ((new_buf->fd < 0 || new_buf->size == 0) && !new_buf->dbuf)
			continue;

		rc = msm_cvp_map_buf_user_persist(inst, new_buf, &iova);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: buf %d register failed.\n",
				__func__, i);

			return rc;
		}
		new_buf->fd = iova;
	}
	return rc;
}

static int msm_cvp_map_buf(struct msm_cvp_inst *inst,
	struct cvp_kmd_hfi_packet *in_pkt,
	unsigned int offset, unsigned int buf_num)
{
	struct msm_cvp_internal_buffer *cbuf = NULL;
	struct cvp_buf_desc *buf_ptr;
	struct cvp_buf_type *new_buf;
	int i, rc = 0;
	u32 version;
	unsigned int iova;
	u64 ktid;
	struct msm_cvp_frame *frame;

	version = get_hfi_version();
	version = (version & HFI_VERSION_MINOR_MASK) >> HFI_VERSION_MINOR_SHIFT;

	if (version >= 1 && buf_num) {
		struct cvp_hfi_cmd_session_hdr *cmd_hdr;

		cmd_hdr = (struct cvp_hfi_cmd_session_hdr *)in_pkt;
		ktid = atomic64_inc_return(&inst->core->kernel_trans_id);
		cmd_hdr->client_data.kdata1 = (u32)ktid;
		cmd_hdr->client_data.kdata2 = (u32)(ktid >> 32);

		frame = kmem_cache_zalloc(cvp_driver->frame_cache, GFP_KERNEL);
		if (!frame)
			return -ENOMEM;

		INIT_MSM_CVP_LIST(&frame->bufs);
		frame->ktid = ktid;
	} else {
		frame = NULL;
	}

	if (!offset || !buf_num)
		return 0;

	for (i = 0; i < buf_num; i++) {
		buf_ptr = (struct cvp_buf_desc *)
				&in_pkt->pkt_data[offset];
		if (version >= 1)
			offset += sizeof(*new_buf) >> 2;
		else
			offset += sizeof(*buf_ptr) >> 2;

		if (version >= 1) {
			new_buf = (struct cvp_buf_type *)buf_ptr;

			/*
			 * Make sure fd or dma_buf field doesn't have any
			 * garbage value.
			 */
			if (inst->session_type == MSM_CVP_USER) {
				new_buf->dbuf = 0;
			} else if (inst->session_type == MSM_CVP_KERNEL) {
				new_buf->fd = 0;
			} else if (inst->session_type >= MSM_CVP_UNKNOWN) {
				dprintk(CVP_ERR,
					"%s: unknown session type %d\n",
					__func__, inst->session_type);
				return -EINVAL;
			}

			if ((new_buf->fd < 0 || new_buf->size == 0) &&
				!new_buf->dbuf)
				continue;

			rc = msm_cvp_map_buf_cpu(inst, new_buf, &iova, frame);
			if (rc) {
				struct msm_cvp_frame_buf *frame_buf, *dummy;

				dprintk(CVP_ERR,
					"%s: buf %d register failed.\n",
					__func__, i);

				list_for_each_entry_safe(frame_buf,
					dummy, &frame->bufs.list, list) {
					list_del(&frame_buf->list);
					__unmap_buf(inst, frame_buf);
					kmem_cache_free(
					cvp_driver->frame_buf_cache,
					frame_buf);
				}
				DEINIT_MSM_CVP_LIST(&frame->bufs);
				kmem_cache_free(cvp_driver->frame_cache,
						frame);
				return rc;
			}
			new_buf->fd = iova;
		} else {
			if (!buf_ptr->fd)
				continue;

			rc = msm_cvp_session_get_iova_addr_d(inst,
						&cbuf,
						buf_ptr->fd,
						buf_ptr->size,
						&buf_ptr->fd,
						&buf_ptr->size);

			if (rc == -ENOENT) {
				dprintk(CVP_DBG,
					"%s map buf fd %d size %d\n",
					__func__, buf_ptr->fd,
					buf_ptr->size);
				rc = msm_cvp_map_buf_cpu_d(inst,
						buf_ptr->fd,
						buf_ptr->size, &cbuf);
				if (rc || !cbuf) {
					dprintk(CVP_ERR,
					"%s: buf %d register failed. rc=%d\n",
					__func__, i, rc);
					return rc;
				}
				buf_ptr->fd = cbuf->smem.device_addr;
				buf_ptr->size = cbuf->buf.size;
			} else if (rc) {
				dprintk(CVP_ERR,
				"%s: buf %d register failed. rc=%d\n",
				__func__, i, rc);
				return rc;
			}
			msm_cvp_smem_cache_operations(
					cbuf->smem.dma_buf,
					SMEM_CACHE_CLEAN_INVALIDATE,
					0, buf_ptr->size);
		}
	}

	if (frame != NULL) {
		mutex_lock(&inst->frames.lock);
		list_add_tail(&frame->list, &inst->frames.list);
		mutex_unlock(&inst->frames.lock);
		dprintk(CVP_DBG, "%s: map frame %llu\n", __func__, ktid);
	}

	return rc;
}

static int msm_cvp_session_process_hfi(
	struct msm_cvp_inst *inst,
	struct cvp_kmd_hfi_packet *in_pkt,
	unsigned int in_offset,
	unsigned int in_buf_num)
{
	int pkt_idx, rc = 0;
	struct cvp_hfi_device *hdev;
	unsigned int offset, buf_num, signal;
	struct cvp_session_queue *sq;
	struct msm_cvp_inst *s;
	unsigned int max_buf_num;

	if (!inst || !inst->core || !in_pkt) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	inst->cur_cmd_type = CVP_KMD_SEND_CMD_PKT;
	hdev = inst->core->device;

	pkt_idx = get_pkt_index((struct cvp_hal_session_cmd_pkt *)in_pkt);
	if (pkt_idx < 0) {
		dprintk(CVP_ERR, "%s incorrect packet %d, %x\n", __func__,
				in_pkt->pkt_data[0],
				in_pkt->pkt_data[1]);
		offset = in_offset;
		buf_num = in_buf_num;
		signal = HAL_NO_RESP;
	} else {
		offset = cvp_hfi_defs[pkt_idx].buf_offset;
		buf_num = cvp_hfi_defs[pkt_idx].buf_num;
		signal = cvp_hfi_defs[pkt_idx].resp;
	}
	if (signal == HAL_NO_RESP) {
		/* Frame packets are not allowed before session starts*/
		sq = &inst->session_queue;
		spin_lock(&sq->lock);
		if (sq->state != QUEUE_ACTIVE) {
			spin_unlock(&sq->lock);
			dprintk(CVP_ERR, "%s: invalid queue state\n", __func__);
			rc = -EINVAL;
			goto exit;
		}
		spin_unlock(&sq->lock);
	}

	if (in_offset && in_buf_num) {
		offset = in_offset;
		buf_num = in_buf_num;
	}

	max_buf_num = sizeof(struct cvp_kmd_hfi_packet)
			/ sizeof(struct cvp_buf_type);

	if (buf_num > max_buf_num)
		return -EINVAL;

	if ((offset + buf_num * sizeof(struct cvp_buf_type)) >
					sizeof(struct cvp_kmd_hfi_packet))
		return -EINVAL;

	if (in_pkt->pkt_data[1] == HFI_CMD_SESSION_CVP_SET_PERSIST_BUFFERS)
		rc = msm_cvp_map_user_persist(inst, in_pkt, offset, buf_num);
	else
		rc = msm_cvp_map_buf(inst, in_pkt, offset, buf_num);

	if (rc)
		goto exit;

	rc = call_hfi_op(hdev, session_send,
			(void *)inst->session, in_pkt);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: Failed in call_hfi_op %d, %x\n",
			__func__, in_pkt->pkt_data[0], in_pkt->pkt_data[1]);
		goto exit;
	}

	if (signal != HAL_NO_RESP) {
		rc = wait_for_sess_signal_receipt(inst, signal);
		if (rc)
			dprintk(CVP_ERR,
				"%s: wait for signal failed, rc %d %d, %x %d\n",
				__func__, rc,
				in_pkt->pkt_data[0],
				in_pkt->pkt_data[1],
				signal);

	}
exit:
	inst->cur_cmd_type = 0;
	cvp_put_inst(inst);
	return rc;
}

#define CVP_FENCE_RUN	0x100
static int msm_cvp_thread_fence_run(void *data)
{
	int i, rc = 0;
	unsigned long timeout_ms = 100;
	int synx_obj;
	struct cvp_hfi_device *hdev;
	struct msm_cvp_fence_thread_data *fence_thread_data;
	struct cvp_kmd_hfi_fence_packet *in_fence_pkt;
	struct cvp_kmd_hfi_packet *in_pkt;
	struct msm_cvp_inst *inst;
	int *fence;
	int ica_enabled = 0;
	int pkt_idx;
	int synx_state = SYNX_STATE_SIGNALED_SUCCESS;

	if (!data) {
		dprintk(CVP_ERR, "%s Wrong input data %pK\n", __func__, data);
		do_exit(-EINVAL);
	}

	fence_thread_data = data;
	inst = fence_thread_data->inst;
	if (!inst) {
		dprintk(CVP_ERR, "%s Wrong inst %pK\n", __func__, inst);
		rc = -EINVAL;
		return rc;
	}
	inst->cur_cmd_type = CVP_FENCE_RUN;
	in_fence_pkt = (struct cvp_kmd_hfi_fence_packet *)
					&fence_thread_data->in_fence_pkt;
	in_pkt = (struct cvp_kmd_hfi_packet *)(in_fence_pkt);
	pkt_idx = get_pkt_index((struct cvp_hal_session_cmd_pkt *)in_pkt);

	if (pkt_idx < 0) {
		dprintk(CVP_ERR, "%s incorrect packet %d, %x\n", __func__,
			in_pkt->pkt_data[0],
			in_pkt->pkt_data[1]);
		rc = pkt_idx;
		goto exit;
	}

	fence = (int *)(in_fence_pkt->fence_data);
	hdev = inst->core->device;

	//wait on synx before signaling HFI
	switch (cvp_hfi_defs[pkt_idx].type) {
	case HFI_CMD_SESSION_CVP_DME_FRAME:
	{
		for (i = 0; i < HFI_DME_BUF_NUM-1; i++) {
			if (fence[(i<<1)]) {
				rc = synx_import(fence[(i<<1)],
					fence[((i<<1)+1)], &synx_obj);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_import failed\n",
						__func__);
					synx_state = SYNX_STATE_SIGNALED_ERROR;
				}
				rc = synx_wait(synx_obj, timeout_ms);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_wait failed\n",
						__func__);
					synx_state = SYNX_STATE_SIGNALED_ERROR;
				}
				rc = synx_release(synx_obj);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_release failed\n",
						__func__);
					synx_state = SYNX_STATE_SIGNALED_ERROR;
				}
				if (i == 0) {
					ica_enabled = 1;
					/*
					 * Increase loop count to skip fence
					 * waiting on downscale image.
					 */
					i = i+1;
				}
			}
		}

		if (synx_state != SYNX_STATE_SIGNALED_ERROR) {
			mutex_lock(&inst->fence_lock);
			rc = call_hfi_op(hdev, session_send,
					(void *)inst->session, in_pkt);
			if (rc) {
				dprintk(CVP_ERR,
					"%s: Failed in call_hfi_op %d, %x\n",
					__func__, in_pkt->pkt_data[0],
					in_pkt->pkt_data[1]);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}

			rc = wait_for_sess_signal_receipt_fence(inst,
					HAL_SESSION_DME_FRAME_CMD_DONE);
			if (rc) {
				dprintk(CVP_ERR,
				"%s: wait for signal failed, rc %d\n",
				__func__, rc);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
			mutex_unlock(&inst->fence_lock);
		}

		if (ica_enabled) {
			rc = synx_import(fence[2], fence[3], &synx_obj);
			if (rc) {
				dprintk(CVP_ERR, "%s: synx_import failed\n",
					__func__);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
			rc = synx_signal(synx_obj, synx_state);
			if (rc) {
				dprintk(CVP_ERR, "%s: synx_signal failed\n",
					__func__);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}

			rc = synx_release(synx_obj);
			if (rc) {
				dprintk(CVP_ERR, "%s: synx_release failed\n",
					__func__);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
		}

		rc = synx_import(fence[((HFI_DME_BUF_NUM-1)<<1)],
				fence[((HFI_DME_BUF_NUM-1)<<1)+1],
				&synx_obj);
		if (rc) {
			dprintk(CVP_ERR, "%s: synx_import failed\n", __func__);
			synx_state = SYNX_STATE_SIGNALED_ERROR;
		}
		rc = synx_signal(synx_obj, synx_state);
		if (rc) {
			dprintk(CVP_ERR, "%s: synx_signal failed\n", __func__);
			synx_state = SYNX_STATE_SIGNALED_ERROR;
		}
		rc = synx_release(synx_obj);
		if (rc) {
			dprintk(CVP_ERR, "%s: synx_release failed\n",
				__func__);
			synx_state = SYNX_STATE_SIGNALED_ERROR;
		}
		break;
	}
	case HFI_CMD_SESSION_CVP_ICA_FRAME:
	{
		for (i = 0; i < cvp_hfi_defs[pkt_idx].buf_num-1; i++) {
			if (fence[(i<<1)]) {
				rc = synx_import(fence[(i<<1)],
					fence[((i<<1)+1)], &synx_obj);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_import failed\n",
						__func__);
					goto exit;
				}
				rc = synx_wait(synx_obj, timeout_ms);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_wait failed\n",
						__func__);
					goto exit;
				}
				rc = synx_release(synx_obj);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_release failed\n",
						__func__);
					goto exit;
				}
				if (i == 0) {
					/*
					 * Increase loop count to skip fence
					 * waiting on output corrected image.
					 */
					i = i+1;
				}
			}
		}

		mutex_lock(&inst->fence_lock);
		rc = call_hfi_op(hdev, session_send,
				(void *)inst->session, in_pkt);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: Failed in call_hfi_op %d, %x\n",
				__func__, in_pkt->pkt_data[0],
				in_pkt->pkt_data[1]);
			synx_state = SYNX_STATE_SIGNALED_ERROR;
		}

		if (synx_state != SYNX_STATE_SIGNALED_ERROR) {
			rc = wait_for_sess_signal_receipt_fence(inst,
					HAL_SESSION_ICA_FRAME_CMD_DONE);
			if (rc) {
				dprintk(CVP_ERR,
				"%s: wait for signal failed, rc %d\n",
				__func__, rc);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
		}
		mutex_unlock(&inst->fence_lock);

		rc = synx_import(fence[2], fence[3], &synx_obj);
		if (rc) {
			dprintk(CVP_ERR, "%s: synx_import failed\n", __func__);
			goto exit;
		}
		rc = synx_signal(synx_obj, synx_state);
		if (rc) {
			dprintk(CVP_ERR, "%s: synx_signal failed\n", __func__);
			goto exit;
		}
		rc = synx_release(synx_obj);
		if (rc) {
			dprintk(CVP_ERR, "%s: synx_release failed\n", __func__);
			goto exit;
		}
		break;
	}
	case HFI_CMD_SESSION_CVP_FD_FRAME:
	{
		int in_fence_num = fence[0];
		int out_fence_num = fence[1];
		int start_out = in_fence_num + 1;

		for (i = 1; i < in_fence_num + 1; i++) {
			if (fence[(i<<1)]) {
				rc = synx_import(fence[(i<<1)],
					fence[((i<<1)+1)], &synx_obj);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_import %d failed\n",
						__func__, i<<1);
					goto exit;
				}
				rc = synx_wait(synx_obj, timeout_ms);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_wait %d failed\n",
						__func__, i<<1);
					goto exit;
				}
				rc = synx_release(synx_obj);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_release %d failed\n",
						__func__, i<<1);
					goto exit;
				}
			}
		}

		mutex_lock(&inst->fence_lock);
		rc = call_hfi_op(hdev, session_send,
				(void *)inst->session, in_pkt);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: Failed in call_hfi_op %d, %x\n",
				__func__, in_pkt->pkt_data[0],
				in_pkt->pkt_data[1]);
			synx_state = SYNX_STATE_SIGNALED_ERROR;
		}

		if (synx_state != SYNX_STATE_SIGNALED_ERROR) {
			rc = wait_for_sess_signal_receipt_fence(inst,
					HAL_SESSION_FD_FRAME_CMD_DONE);
			if (rc) {
				dprintk(CVP_ERR,
				"%s: wait for signal failed, rc %d\n",
				__func__, rc);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
		}
		mutex_unlock(&inst->fence_lock);

		for (i = start_out; i <  start_out + out_fence_num; i++) {
			if (fence[(i<<1)]) {
				rc = synx_import(fence[(i<<1)],
					fence[((i<<1)+1)], &synx_obj);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_import %d failed\n",
						__func__, i<<1);
					goto exit;
				}
				rc = synx_signal(synx_obj, synx_state);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_signal %d failed\n",
						__func__, i<<1);
					goto exit;
				}
				rc = synx_release(synx_obj);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_release %d failed\n",
						__func__, i<<1);
					goto exit;
				}
			}
		}
		break;
	}
	default:
		dprintk(CVP_ERR, "%s: unknown hfi cmd type 0x%x\n",
			__func__, fence_thread_data->arg_type);
		rc = -EINVAL;
		goto exit;
		break;
	}

exit:
	kmem_cache_free(cvp_driver->fence_data_cache, fence_thread_data);
	inst->cur_cmd_type = 0;
	cvp_put_inst(inst);
	do_exit(rc);
}

static int msm_cvp_session_process_hfi_fence(
	struct msm_cvp_inst *inst,
	struct cvp_kmd_arg *arg)
{
	static int thread_num;
	struct task_struct *thread;
	int rc = 0;
	char thread_fence_name[32];
	int pkt_idx;
	struct cvp_kmd_hfi_packet *in_pkt;
	unsigned int signal, offset, buf_num, in_offset, in_buf_num;
	struct msm_cvp_inst *s;
	unsigned int max_buf_num;
	struct msm_cvp_fence_thread_data *fence_thread_data;

	dprintk(CVP_DBG, "%s: Enter inst = %#x", __func__, inst);

	if (!inst || !inst->core || !arg || !inst->core->device) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	inst->cur_cmd_type = CVP_KMD_SEND_FENCE_CMD_PKT;
	fence_thread_data = kmem_cache_alloc(cvp_driver->fence_data_cache,
			GFP_KERNEL);
	if (!fence_thread_data) {
		dprintk(CVP_ERR, "%s: fence_thread_data alloc failed\n",
				__func__);
		rc = -ENOMEM;
		goto exit;
	}

	in_offset = arg->buf_offset;
	in_buf_num = arg->buf_num;
	in_pkt = (struct cvp_kmd_hfi_packet *)&arg->data.hfi_pkt;
	pkt_idx = get_pkt_index((struct cvp_hal_session_cmd_pkt *)in_pkt);
	if (pkt_idx < 0) {
		dprintk(CVP_ERR, "%s incorrect packet %d, %x\n", __func__,
				in_pkt->pkt_data[0],
				in_pkt->pkt_data[1]);
		offset = in_offset;
		buf_num = in_buf_num;
		signal = HAL_NO_RESP;
	} else {
		offset = cvp_hfi_defs[pkt_idx].buf_offset;
		buf_num = cvp_hfi_defs[pkt_idx].buf_num;
		signal = cvp_hfi_defs[pkt_idx].resp;
	}

	if (in_offset && in_buf_num) {
		offset = in_offset;
		buf_num = in_buf_num;
	}

	max_buf_num = sizeof(struct cvp_kmd_hfi_packet)
						/ sizeof(struct cvp_buf_type);

	if (buf_num > max_buf_num)
		return -EINVAL;

	if ((offset + buf_num * sizeof(struct cvp_buf_type)) >
					sizeof(struct cvp_kmd_hfi_packet))
		return -EINVAL;

	rc = msm_cvp_map_buf(inst, in_pkt, offset, buf_num);
	if (rc)
		goto free_and_exit;

	thread_num = thread_num + 1;
	fence_thread_data->inst = inst;
	fence_thread_data->device_id = (unsigned int)inst->core->id;
	memcpy(&fence_thread_data->in_fence_pkt, &arg->data.hfi_fence_pkt,
				sizeof(struct cvp_kmd_hfi_fence_packet));
	fence_thread_data->arg_type = arg->type;
	snprintf(thread_fence_name, sizeof(thread_fence_name),
				"thread_fence_%d", thread_num);
	thread = kthread_run(msm_cvp_thread_fence_run,
			fence_thread_data, thread_fence_name);
	if (!thread) {
		dprintk(CVP_ERR, "%s fail to create kthread\n", __func__);
		rc = -ECHILD;
		goto free_and_exit;
	}

	return 0;

free_and_exit:
	kmem_cache_free(cvp_driver->fence_data_cache, fence_thread_data);
exit:
	inst->cur_cmd_type = 0;
	cvp_put_inst(s);
	return rc;
}

static int msm_cvp_session_cvp_dfs_frame_response(
	struct msm_cvp_inst *inst,
	struct cvp_kmd_hfi_packet *dfs_frame)
{
	dprintk(CVP_ERR, "Deprecated system call: DFS_CMD_RESPONSE\n");
		return -EINVAL;
}

static int msm_cvp_session_cvp_dme_frame_response(
	struct msm_cvp_inst *inst,
	struct cvp_kmd_hfi_packet *dme_frame)
{
	dprintk(CVP_ERR, "Deprecated system call: DME_CMD_RESPONSE\n");
		return -EINVAL;
}

static int msm_cvp_session_cvp_persist_response(
	struct msm_cvp_inst *inst,
	struct cvp_kmd_hfi_packet *pbuf_cmd)
{
	dprintk(CVP_ERR, "Deprecated system call: PERSIST_CMD_RESPONSE\n");
		return -EINVAL;
}

static int msm_cvp_send_cmd(struct msm_cvp_inst *inst,
		struct cvp_kmd_send_cmd *send_cmd)
{
	dprintk(CVP_ERR, "Deprecated system call: cvp_send_cmd\n");

	return 0;
}

static inline int div_by_1dot5(unsigned int a)
{
	unsigned long i = a << 1;

	return (unsigned int) i/3;
}

static inline int max_3(unsigned int a, unsigned int b, unsigned int c)
{
	return (a >= b) ? ((a >= c) ? a : c) : ((b >= c) ? b : c);
}

static bool is_subblock_profile_existed(struct msm_cvp_inst *inst)
{
	return (inst->prop.od_cycles ||
			inst->prop.mpu_cycles ||
			inst->prop.fdu_cycles ||
			inst->prop.ica_cycles);
}

static void aggregate_power_update(struct msm_cvp_core *core,
	struct cvp_power_level *nrt_pwr,
	struct cvp_power_level *rt_pwr,
	unsigned int max_clk_rate)
{
	struct msm_cvp_inst *inst;
	int i;
	unsigned long fdu_sum[2] = {0}, od_sum[2] = {0}, mpu_sum[2] = {0};
	unsigned long ica_sum[2] = {0}, fw_sum[2] = {0};
	unsigned long op_fdu_max[2] = {0}, op_od_max[2] = {0};
	unsigned long op_mpu_max[2] = {0}, op_ica_max[2] = {0};
	unsigned long op_fw_max[2] = {0}, bw_sum[2] = {0}, op_bw_max[2] = {0};

	list_for_each_entry(inst, &core->instances, list) {
		if (inst->state == MSM_CVP_CORE_INVALID ||
			inst->state == MSM_CVP_CORE_UNINIT ||
			!is_subblock_profile_existed(inst))
			continue;
		if (inst->prop.priority <= CVP_RT_PRIO_THRESHOLD) {
			/* Non-realtime session use index 0 */
			i = 0;
		} else {
			i = 1;
		}
		dprintk(CVP_PROF, "pwrUpdate %pK fdu %u od %u mpu %u ica %u\n",
			inst->prop.fdu_cycles,
			inst->prop.od_cycles,
			inst->prop.mpu_cycles,
			inst->prop.ica_cycles);

		dprintk(CVP_PROF, "pwrUpdate fw %u fdu_o %u od_o %u mpu_o %u\n",
			inst->prop.fw_cycles,
			inst->prop.fdu_op_cycles,
			inst->prop.od_op_cycles,
			inst->prop.mpu_op_cycles);

		dprintk(CVP_PROF, "pwrUpdate ica_o %u fw_o %u bw %u bw_o %u\n",
			inst->prop.ica_op_cycles,
			inst->prop.fw_op_cycles,
			inst->prop.ddr_bw,
			inst->prop.ddr_op_bw);

		fdu_sum[i] += inst->prop.fdu_cycles;
		od_sum[i] += inst->prop.od_cycles;
		mpu_sum[i] += inst->prop.mpu_cycles;
		ica_sum[i] += inst->prop.ica_cycles;
		fw_sum[i] += inst->prop.fw_cycles;
		op_fdu_max[i] =
			(op_fdu_max[i] >= inst->prop.fdu_op_cycles) ?
			op_fdu_max[i] : inst->prop.fdu_op_cycles;
		op_od_max[i] =
			(op_od_max[i] >= inst->prop.od_op_cycles) ?
			op_od_max[i] : inst->prop.od_op_cycles;
		op_mpu_max[i] =
			(op_mpu_max[i] >= inst->prop.mpu_op_cycles) ?
			op_mpu_max[i] : inst->prop.mpu_op_cycles;
		op_ica_max[i] =
			(op_ica_max[i] >= inst->prop.ica_op_cycles) ?
			op_ica_max[i] : inst->prop.ica_op_cycles;
		op_fw_max[i] =
			(op_fw_max[i] >= inst->prop.fw_op_cycles) ?
			op_fw_max[i] : inst->prop.fw_op_cycles;
		bw_sum[i] += inst->prop.ddr_bw;
		op_bw_max[i] =
			(op_bw_max[i] >= inst->prop.ddr_op_bw) ?
			op_bw_max[i] : inst->prop.ddr_op_bw;
	}

	for (i = 0; i < 2; i++) {
		fdu_sum[i] = max_3(fdu_sum[i], od_sum[i], mpu_sum[i]);
		fdu_sum[i] = max_3(fdu_sum[i], ica_sum[i], fw_sum[i]);

		op_fdu_max[i] = max_3(op_fdu_max[i], op_od_max[i],
			op_mpu_max[i]);
		op_fdu_max[i] = max_3(op_fdu_max[i],
			op_ica_max[i], op_fw_max[i]);
		op_fdu_max[i] =
			(op_fdu_max[i] > max_clk_rate) ?
			max_clk_rate : op_fdu_max[i];
		bw_sum[i] = (bw_sum[i] >= op_bw_max[i]) ?
			bw_sum[i] : op_bw_max[i];
	}

	nrt_pwr->core_sum += fdu_sum[0];
	nrt_pwr->op_core_sum = (nrt_pwr->op_core_sum >= op_fdu_max[0]) ?
			nrt_pwr->op_core_sum : op_fdu_max[0];
	nrt_pwr->bw_sum += bw_sum[0];
	rt_pwr->core_sum += fdu_sum[1];
	rt_pwr->op_core_sum = (rt_pwr->op_core_sum >= op_fdu_max[1]) ?
			rt_pwr->op_core_sum : op_fdu_max[1];
	rt_pwr->bw_sum += bw_sum[1];
}


static void aggregate_power_request(struct msm_cvp_core *core,
	struct cvp_power_level *nrt_pwr,
	struct cvp_power_level *rt_pwr,
	unsigned int max_clk_rate)
{
	struct msm_cvp_inst *inst;
	int i;
	unsigned long core_sum[2] = {0}, ctlr_sum[2] = {0}, fw_sum[2] = {0};
	unsigned long op_core_max[2] = {0}, op_ctlr_max[2] = {0};
	unsigned long op_fw_max[2] = {0}, bw_sum[2] = {0}, op_bw_max[2] = {0};

	list_for_each_entry(inst, &core->instances, list) {
		if (inst->state == MSM_CVP_CORE_INVALID ||
			inst->state == MSM_CVP_CORE_UNINIT ||
			is_subblock_profile_existed(inst))
			continue;
		if (inst->prop.priority <= CVP_RT_PRIO_THRESHOLD) {
			/* Non-realtime session use index 0 */
			i = 0;
		} else {
			i = 1;
		}
		dprintk(CVP_PROF, "pwrReq sess %pK core %u ctl %u fw %u\n",
			inst, inst->power.clock_cycles_a,
			inst->power.clock_cycles_b,
			inst->power.reserved[0]);
		dprintk(CVP_PROF, "pwrReq op_core %u op_ctl %u op_fw %u\n",
			inst->power.reserved[1],
			inst->power.reserved[2],
			inst->power.reserved[3]);

		core_sum[i] += inst->power.clock_cycles_a;
		ctlr_sum[i] += inst->power.clock_cycles_b;
		fw_sum[i] += inst->power.reserved[0];
		op_core_max[i] =
			(op_core_max[i] >= inst->power.reserved[1]) ?
			op_core_max[i] : inst->power.reserved[1];
		op_ctlr_max[i] =
			(op_ctlr_max[i] >= inst->power.reserved[2]) ?
			op_ctlr_max[i] : inst->power.reserved[2];
		op_fw_max[i] =
			(op_fw_max[i] >= inst->power.reserved[3]) ?
			op_fw_max[i] : inst->power.reserved[3];
		bw_sum[i] += inst->power.ddr_bw;
		op_bw_max[i] =
			(op_bw_max[i] >= inst->power.reserved[4]) ?
			op_bw_max[i] : inst->power.reserved[4];
	}

	for (i = 0; i < 2; i++) {
		core_sum[i] = max_3(core_sum[i], ctlr_sum[i], fw_sum[i]);
		op_core_max[i] = max_3(op_core_max[i],
			op_ctlr_max[i], op_fw_max[i]);
		op_core_max[i] =
			(op_core_max[i] > max_clk_rate) ?
			max_clk_rate : op_core_max[i];
		bw_sum[i] = (bw_sum[i] >= op_bw_max[i]) ?
			bw_sum[i] : op_bw_max[i];
	}

	nrt_pwr->core_sum += core_sum[0];
	nrt_pwr->op_core_sum = (nrt_pwr->op_core_sum >= op_core_max[0]) ?
			nrt_pwr->op_core_sum : op_core_max[0];
	nrt_pwr->bw_sum += bw_sum[0];
	rt_pwr->core_sum += core_sum[1];
	rt_pwr->op_core_sum = (rt_pwr->op_core_sum >= op_core_max[1]) ?
			rt_pwr->op_core_sum : op_core_max[1];
	rt_pwr->bw_sum += bw_sum[1];
}

/**
 * adjust_bw_freqs(): calculate CVP clock freq and bw required to sustain
 * required use case.
 * Bandwidth vote will be best-effort, not returning error if the request
 * b/w exceeds max limit.
 * Clock vote from non-realtime sessions will be best effort, not returning
 * error if the aggreated session clock request exceeds max limit.
 * Clock vote from realtime session will be hard request. If aggregated
 * session clock request exceeds max limit, the function will return
 * error.
 */
static int adjust_bw_freqs(void)
{
	struct msm_cvp_core *core;
	struct cvp_hfi_device *hdev;
	struct bus_info *bus;
	struct clock_set *clocks;
	struct clock_info *cl;
	struct allowed_clock_rates_table *tbl = NULL;
	unsigned int tbl_size;
	unsigned int cvp_min_rate, cvp_max_rate, max_bw, min_bw;
	struct cvp_power_level rt_pwr = {0}, nrt_pwr = {0};
	unsigned long core_sum, op_core_sum, bw_sum;
	int i, rc = 0;

	core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);

	hdev = core->device;
	clocks = &core->resources.clock_set;
	cl = &clocks->clock_tbl[clocks->count - 1];
	tbl = core->resources.allowed_clks_tbl;
	tbl_size = core->resources.allowed_clks_tbl_size;
	cvp_min_rate = tbl[0].clock_rate;
	cvp_max_rate = tbl[tbl_size - 1].clock_rate;
	bus = &core->resources.bus_set.bus_tbl[1];
	max_bw = bus->range[1];
	min_bw = max_bw/10;

	aggregate_power_request(core, &nrt_pwr, &rt_pwr, cvp_max_rate);
	dprintk(CVP_DBG, "PwrReq nrt %u %u rt %u %u\n",
		nrt_pwr.core_sum, nrt_pwr.op_core_sum,
		rt_pwr.core_sum, rt_pwr.op_core_sum);
	aggregate_power_update(core, &nrt_pwr, &rt_pwr, cvp_max_rate);
	dprintk(CVP_DBG, "PwrUpdate nrt %u %u rt %u %u\n",
		nrt_pwr.core_sum, nrt_pwr.op_core_sum,
		rt_pwr.core_sum, rt_pwr.op_core_sum);

	if (rt_pwr.core_sum > cvp_max_rate) {
		dprintk(CVP_WARN, "%s clk vote out of range %lld\n",
			__func__, rt_pwr.core_sum);
		return -ENOTSUPP;
	}

	core_sum = rt_pwr.core_sum + nrt_pwr.core_sum;
	op_core_sum = (rt_pwr.op_core_sum >= nrt_pwr.op_core_sum) ?
		rt_pwr.op_core_sum : nrt_pwr.op_core_sum;

	core_sum = (core_sum >= op_core_sum) ?
		core_sum : op_core_sum;

	if (core_sum > cvp_max_rate) {
		core_sum = cvp_max_rate;
	} else	if (core_sum < cvp_min_rate) {
		core_sum = cvp_min_rate;
	} else {
		for (i = 1; i < tbl_size; i++)
			if (core_sum <= tbl[i].clock_rate)
				break;
		core_sum = tbl[i].clock_rate;
	}

	bw_sum = rt_pwr.bw_sum + nrt_pwr.bw_sum;
	bw_sum = (bw_sum > max_bw) ? max_bw : bw_sum;
	bw_sum = (bw_sum < min_bw) ? min_bw : bw_sum;

	dprintk(CVP_PROF, "%s %lld %lld\n", __func__,
		core_sum, bw_sum);
	if (!cl->has_scaling) {
		dprintk(CVP_ERR, "Cannot scale CVP clock\n");
		return -EINVAL;
	}

	mutex_unlock(&core->lock);
	rc = msm_bus_scale_update_bw(bus->client, bw_sum, 0);
	if (rc) {
		dprintk(CVP_ERR, "Failed voting bus %s to ab %u\n",
			bus->name, bw_sum);
		goto exit;
	}

	rc = call_hfi_op(hdev, scale_clocks, hdev->hfi_device_data, core_sum);
	if (rc)
		dprintk(CVP_ERR,
			"Failed to set clock rate %u %s: %d %s\n",
			core_sum, cl->name, rc, __func__);

exit:
	mutex_lock(&core->lock);
	if (!rc)
		core->curr_freq = core_sum;

	return rc;
}

/**
 * Use of cvp_kmd_request_power structure
 * clock_cycles_a: CVP core clock freq
 * clock_cycles_b: CVP controller clock freq
 * ddr_bw: b/w vote in Bps
 * reserved[0]: CVP firmware required clock freq
 * reserved[1]: CVP core operational clock freq
 * reserved[2]: CVP controller operational clock freq
 * reserved[3]: CVP firmware operational clock freq
 * reserved[4]: CVP operational b/w vote
 *
 * session's power record only saves normalized freq or b/w vote
 */
static int msm_cvp_request_power(struct msm_cvp_inst *inst,
		struct cvp_kmd_request_power *power)
{
	int rc = 0;
	struct msm_cvp_core *core;
	struct msm_cvp_inst *s;

	if (!inst || !power) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	inst->cur_cmd_type = CVP_KMD_REQUEST_POWER;
	core = inst->core;

	mutex_lock(&core->power_lock);
	mutex_lock(&core->lock);

	memcpy(&inst->power, power, sizeof(*power));

	/* Normalize CVP controller clock freqs */
	inst->power.clock_cycles_b = div_by_1dot5(inst->power.clock_cycles_b);
	inst->power.reserved[0] = div_by_1dot5(inst->power.reserved[0]);
	inst->power.reserved[2] = div_by_1dot5(inst->power.reserved[2]);
	inst->power.reserved[3] = div_by_1dot5(inst->power.reserved[3]);

	/* Convert bps to KBps */
	inst->power.ddr_bw = inst->power.ddr_bw >> 10;

	rc = adjust_bw_freqs();
	if (rc) {
		memset(&inst->power, 0x0, sizeof(inst->power));
		dprintk(CVP_ERR, "Instance %pK power request out of range\n");
	}

	mutex_unlock(&core->lock);
	mutex_unlock(&core->power_lock);
	inst->cur_cmd_type = 0;
	cvp_put_inst(s);

	return rc;
}

static int msm_cvp_update_power(struct msm_cvp_inst *inst)

{	int rc = 0;
	struct msm_cvp_core *core;
	struct msm_cvp_inst *s;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	inst->cur_cmd_type = CVP_KMD_UPDATE_POWER;
	core = inst->core;

	mutex_lock(&core->power_lock);
	mutex_lock(&core->lock);
	rc = adjust_bw_freqs();
	mutex_unlock(&core->lock);
	mutex_unlock(&core->power_lock);
	inst->cur_cmd_type = 0;
	cvp_put_inst(s);

	return rc;
}

static int msm_cvp_register_buffer(struct msm_cvp_inst *inst,
		struct cvp_kmd_buffer *buf)
{
	struct cvp_hfi_device *hdev;
	struct cvp_hal_session *session;
	struct msm_cvp_inst *s;
	int rc = 0;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!buf->index)
		return 0;

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	inst->cur_cmd_type = CVP_KMD_REGISTER_BUFFER;
	session = (struct cvp_hal_session *)inst->session;
	if (!session) {
		dprintk(CVP_ERR, "%s: invalid session\n", __func__);
		rc = -EINVAL;
		goto exit;
	}
	hdev = inst->core->device;
	print_client_buffer(CVP_DBG, "register", inst, buf);

	rc = msm_cvp_map_buf_dsp(inst, buf);
exit:
	inst->cur_cmd_type = 0;
	cvp_put_inst(s);
	return rc;
}

static int msm_cvp_unregister_buffer(struct msm_cvp_inst *inst,
		struct cvp_kmd_buffer *buf)
{
	struct msm_cvp_inst *s;
	int rc = 0;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!buf->index) {
		return 0;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	inst->cur_cmd_type = CVP_KMD_UNREGISTER_BUFFER;
	print_client_buffer(CVP_DBG, "unregister", inst, buf);

	rc = msm_cvp_unmap_buf_dsp(inst, buf);
	inst->cur_cmd_type = 0;
	cvp_put_inst(s);
	return rc;
}

static int msm_cvp_session_create(struct msm_cvp_inst *inst)
{
	int rc = 0;

	if (!inst || !inst->core)
		return -EINVAL;

	if (inst->state >= MSM_CVP_CLOSE_DONE)
		return -ECONNRESET;

	if (inst->state != MSM_CVP_CORE_INIT_DONE ||
		inst->state > MSM_CVP_OPEN_DONE) {
		dprintk(CVP_ERR,
			"%s Incorrect CVP state %d to create session\n",
			__func__, inst->state);
		return -EINVAL;
	}

	rc = msm_cvp_comm_try_state(inst, MSM_CVP_OPEN_DONE);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to move instance to open done state\n");
		goto fail_init;
	}

	rc = cvp_comm_set_arp_buffers(inst);
	if (rc) {
		dprintk(CVP_ERR,
				"Failed to set ARP buffers\n");
		goto fail_init;
	}

fail_init:
	return rc;
}

static int session_state_check_init(struct msm_cvp_inst *inst)
{
	mutex_lock(&inst->lock);
	if (inst->state == MSM_CVP_OPEN || inst->state == MSM_CVP_OPEN_DONE) {
		mutex_unlock(&inst->lock);
		return 0;
	}
	mutex_unlock(&inst->lock);

	return msm_cvp_session_create(inst);
}

static int msm_cvp_session_start(struct msm_cvp_inst *inst,
		struct cvp_kmd_arg *arg)
{
	struct cvp_session_queue *sq;

	sq = &inst->session_queue;
	spin_lock(&sq->lock);
	if (sq->msg_count) {
		dprintk(CVP_ERR, "session start failed queue not empty%d\n",
			sq->msg_count);
		spin_unlock(&sq->lock);
		return -EINVAL;
	}
	sq->state = QUEUE_ACTIVE;
	spin_unlock(&sq->lock);

	return 0;
}

static int msm_cvp_session_stop(struct msm_cvp_inst *inst,
		struct cvp_kmd_arg *arg)
{
	struct cvp_session_queue *sq;
	struct cvp_kmd_session_control *sc = &arg->data.session_ctrl;

	sq = &inst->session_queue;

	spin_lock(&sq->lock);
	if (sq->msg_count) {
		dprintk(CVP_ERR, "session stop incorrect: queue not empty%d\n",
			sq->msg_count);
		sc->ctrl_data[0] = sq->msg_count;
		spin_unlock(&sq->lock);
		return -EUCLEAN;
	}
	sq->state = QUEUE_STOP;

	spin_unlock(&sq->lock);

	wake_up_all(&inst->session_queue.wq);

	return 0;
}

static int msm_cvp_session_ctrl(struct msm_cvp_inst *inst,
		struct cvp_kmd_arg *arg)
{
	struct cvp_kmd_session_control *ctrl = &arg->data.session_ctrl;
	int rc = 0;
	unsigned int ctrl_type;

	ctrl_type = ctrl->ctrl_type;

	if (!inst && ctrl_type != SESSION_CREATE) {
		dprintk(CVP_ERR, "%s invalid session\n", __func__);
		return -EINVAL;
	}

	switch (ctrl_type) {
	case SESSION_STOP:
		rc = msm_cvp_session_stop(inst, arg);
		break;
	case SESSION_START:
		rc = msm_cvp_session_start(inst, arg);
		break;
	case SESSION_CREATE:
		rc = msm_cvp_session_create(inst);
	case SESSION_DELETE:
		break;
	case SESSION_INFO:
	default:
		dprintk(CVP_ERR, "%s Unsupported session ctrl%d\n",
			__func__, ctrl->ctrl_type);
		rc = -EINVAL;
	}
	return rc;
}

static int msm_cvp_get_sysprop(struct msm_cvp_inst *inst,
		struct cvp_kmd_arg *arg)
{
	struct cvp_kmd_sys_properties *props = &arg->data.sys_properties;
	struct cvp_hfi_device *hdev;
	struct iris_hfi_device *hfi;
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;
	hfi = hdev->hfi_device_data;

	switch (props->prop_data.prop_type) {
	case CVP_KMD_PROP_HFI_VERSION:
	{
		props->prop_data.data = hfi->version;
		break;
	}
	default:
		dprintk(CVP_ERR, "unrecognized sys property %d\n",
			props->prop_data.prop_type);
		rc = -EFAULT;
	}
	return rc;
}

static int msm_cvp_set_sysprop(struct msm_cvp_inst *inst,
		struct cvp_kmd_arg *arg)
{
	struct cvp_kmd_sys_properties *props = &arg->data.sys_properties;
	struct cvp_kmd_sys_property *prop_array;
	struct cvp_session_prop *session_prop;
	int i, rc = 0;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (props->prop_num >= MAX_KMD_PROP_NUM) {
		dprintk(CVP_ERR, "Too many properties %d to set\n",
			props->prop_num);
		return -E2BIG;
	}

	prop_array = &arg->data.sys_properties.prop_data;
	session_prop = &inst->prop;

	for (i = 0; i < props->prop_num; i++) {
		switch (prop_array[i].prop_type) {
		case CVP_KMD_PROP_SESSION_TYPE:
			session_prop->type = prop_array[i].data;
			break;
		case CVP_KMD_PROP_SESSION_KERNELMASK:
			session_prop->kernel_mask = prop_array[i].data;
			break;
		case CVP_KMD_PROP_SESSION_PRIORITY:
			session_prop->priority = prop_array[i].data;
			break;
		case CVP_KMD_PROP_SESSION_SECURITY:
			session_prop->is_secure = prop_array[i].data;
			break;
		case CVP_KMD_PROP_SESSION_DSPMASK:
			session_prop->dsp_mask = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_FDU:
			session_prop->fdu_cycles = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_ICA:
			session_prop->ica_cycles =
				div_by_1dot5(prop_array[i].data);
			break;
		case CVP_KMD_PROP_PWR_OD:
			session_prop->od_cycles = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_MPU:
			session_prop->mpu_cycles = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_FW:
			session_prop->fw_cycles =
				div_by_1dot5(prop_array[i].data);
			break;
		case CVP_KMD_PROP_PWR_DDR:
			session_prop->ddr_bw = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_SYSCACHE:
			session_prop->ddr_cache = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_FDU_OP:
			session_prop->fdu_op_cycles = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_ICA_OP:
			session_prop->ica_op_cycles =
				div_by_1dot5(prop_array[i].data);
			break;
		case CVP_KMD_PROP_PWR_OD_OP:
			session_prop->od_op_cycles = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_MPU_OP:
			session_prop->mpu_op_cycles = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_FW_OP:
			session_prop->fw_op_cycles =
				div_by_1dot5(prop_array[i].data);
			break;
		case CVP_KMD_PROP_PWR_DDR_OP:
			session_prop->ddr_op_bw = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_SYSCACHE_OP:
			session_prop->ddr_op_cache = prop_array[i].data;
			break;
		default:
			dprintk(CVP_ERR,
				"unrecognized sys property to set %d\n",
				prop_array[i].prop_type);
			rc = -EFAULT;
		}
	}
	return rc;
}

int msm_cvp_handle_syscall(struct msm_cvp_inst *inst, struct cvp_kmd_arg *arg)
{
	int rc = 0;

	if (!inst || !arg) {
		dprintk(CVP_ERR, "%s: invalid args\n", __func__);
		return -EINVAL;
	}
	dprintk(CVP_DBG, "%s: arg->type = %x", __func__, arg->type);

	if (arg->type != CVP_KMD_SESSION_CONTROL &&
		arg->type != CVP_KMD_SET_SYS_PROPERTY &&
		arg->type != CVP_KMD_GET_SYS_PROPERTY) {

		rc = session_state_check_init(inst);
		if (rc) {
			dprintk(CVP_ERR,
				"Incorrect session state %d for command %#x",
				inst->state, arg->type);
			return rc;
		}
	}

	switch (arg->type) {
	case CVP_KMD_GET_SESSION_INFO:
	{
		struct cvp_kmd_session_info *session =
			(struct cvp_kmd_session_info *)&arg->data.session;

		rc = msm_cvp_get_session_info(inst, session);
		break;
	}
	case CVP_KMD_REQUEST_POWER:
	{
		struct cvp_kmd_request_power *power =
			(struct cvp_kmd_request_power *)&arg->data.req_power;

		rc = msm_cvp_request_power(inst, power);
		break;
	}
	case CVP_KMD_UPDATE_POWER:
	{
		rc = msm_cvp_update_power(inst);
		break;
	}
	case CVP_KMD_REGISTER_BUFFER:
	{
		struct cvp_kmd_buffer *buf =
			(struct cvp_kmd_buffer *)&arg->data.regbuf;

		rc = msm_cvp_register_buffer(inst, buf);
		break;
	}
	case CVP_KMD_UNREGISTER_BUFFER:
	{
		struct cvp_kmd_buffer *buf =
			(struct cvp_kmd_buffer *)&arg->data.unregbuf;

		rc = msm_cvp_unregister_buffer(inst, buf);
		break;
	}
	case CVP_KMD_HFI_SEND_CMD:
	{
		struct cvp_kmd_send_cmd *send_cmd =
			(struct cvp_kmd_send_cmd *)&arg->data.send_cmd;

		rc = msm_cvp_send_cmd(inst, send_cmd);
		break;
	}
	case CVP_KMD_RECEIVE_MSG_PKT:
	{
		struct cvp_kmd_hfi_packet *out_pkt =
			(struct cvp_kmd_hfi_packet *)&arg->data.hfi_pkt;
		rc = msm_cvp_session_receive_hfi(inst, out_pkt);
		break;
	}
	case CVP_KMD_SEND_CMD_PKT:
	case CVP_KMD_HFI_DFS_CONFIG_CMD:
	case CVP_KMD_HFI_DFS_FRAME_CMD:
	case CVP_KMD_HFI_DME_CONFIG_CMD:
	case CVP_KMD_HFI_DME_FRAME_CMD:
	case CVP_KMD_HFI_FD_FRAME_CMD:
	case CVP_KMD_HFI_PERSIST_CMD:
	{
		struct cvp_kmd_hfi_packet *in_pkt =
			(struct cvp_kmd_hfi_packet *)&arg->data.hfi_pkt;

		rc = msm_cvp_session_process_hfi(inst, in_pkt,
				arg->buf_offset, arg->buf_num);
		break;
	}
	case CVP_KMD_HFI_DFS_FRAME_CMD_RESPONSE:
	{
		struct cvp_kmd_hfi_packet *dfs_frame =
			(struct cvp_kmd_hfi_packet *)&arg->data.hfi_pkt;

		rc = msm_cvp_session_cvp_dfs_frame_response(inst, dfs_frame);
		break;
	}
	case CVP_KMD_HFI_DME_FRAME_CMD_RESPONSE:
	{
		struct cvp_kmd_hfi_packet *dme_frame =
			(struct cvp_kmd_hfi_packet *)&arg->data.hfi_pkt;

		rc = msm_cvp_session_cvp_dme_frame_response(inst, dme_frame);
		break;
	}
	case CVP_KMD_HFI_PERSIST_CMD_RESPONSE:
	{
		struct cvp_kmd_hfi_packet *pbuf_cmd =
			(struct cvp_kmd_hfi_packet *)&arg->data.hfi_pkt;

		rc = msm_cvp_session_cvp_persist_response(inst, pbuf_cmd);
		break;
	}
	case CVP_KMD_HFI_DME_FRAME_FENCE_CMD:
	case CVP_KMD_SEND_FENCE_CMD_PKT:
	{
		rc = msm_cvp_session_process_hfi_fence(inst, arg);
		break;
	}
	case CVP_KMD_SESSION_CONTROL:
		rc = msm_cvp_session_ctrl(inst, arg);
		break;
	case CVP_KMD_GET_SYS_PROPERTY:
		rc = msm_cvp_get_sysprop(inst, arg);
		break;
	case CVP_KMD_SET_SYS_PROPERTY:
		rc = msm_cvp_set_sysprop(inst, arg);
		break;
	default:
		dprintk(CVP_DBG, "%s: unknown arg type %#x\n",
				__func__, arg->type);
		rc = -ENOTSUPP;
		break;
	}

	return rc;
}

int msm_cvp_session_deinit(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct cvp_hal_session *session;
	struct msm_cvp_internal_buffer *cbuf, *dummy;
	struct msm_cvp_frame *frame, *dummy1;
	struct msm_cvp_frame_buf *frame_buf, *dummy2;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	dprintk(CVP_DBG, "%s: inst %pK (%#x)\n", __func__,
		inst, hash32_ptr(inst->session));

	session = (struct cvp_hal_session *)inst->session;
	if (!session)
		return rc;

	rc = msm_cvp_comm_try_state(inst, MSM_CVP_CLOSE_DONE);
	if (rc)
		dprintk(CVP_ERR, "%s: close failed\n", __func__);

	mutex_lock(&inst->cvpcpubufs.lock);
	list_for_each_entry_safe(cbuf, dummy, &inst->cvpcpubufs.list,
			list) {
		print_internal_buffer(CVP_DBG, "remove from cvpcpubufs", inst,
									cbuf);
		msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
		list_del(&cbuf->list);
		kmem_cache_free(cvp_driver->internal_buf_cache, cbuf);
	}
	mutex_unlock(&inst->cvpcpubufs.lock);

	mutex_lock(&inst->cvpdspbufs.lock);
	list_for_each_entry_safe(cbuf, dummy, &inst->cvpdspbufs.list,
			list) {
		print_internal_buffer(CVP_DBG, "remove from cvpdspbufs", inst,
									cbuf);
		rc = cvp_dsp_deregister_buffer(hash32_ptr(session),
			cbuf->buf.fd, cbuf->smem.dma_buf->size, cbuf->buf.size,
			cbuf->buf.offset, cbuf->buf.index,
			(uint32_t)cbuf->smem.device_addr);
		if (rc)
			dprintk(CVP_ERR,
				"%s: failed dsp deregistration fd=%d rc=%d",
				__func__, cbuf->buf.fd, rc);

		msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
		list_del(&cbuf->list);
		kmem_cache_free(cvp_driver->internal_buf_cache, cbuf);
	}
	mutex_unlock(&inst->cvpdspbufs.lock);

	mutex_lock(&inst->frames.lock);
	list_for_each_entry_safe(frame, dummy1, &inst->frames.list, list) {
		list_del(&frame->list);
		mutex_lock(&frame->bufs.lock);
		list_for_each_entry_safe(frame_buf, dummy2, &frame->bufs.list,
									list) {
			struct cvp_buf_type *buf = &frame_buf->buf;

			dprintk(CVP_DBG,
				"%s: %x : fd %d off %d size %d %s\n",
				"remove from frame list",
				hash32_ptr(inst->session),
				buf->fd, buf->offset, buf->size,
				buf->dbuf->name);

			list_del(&frame_buf->list);
			kmem_cache_free(cvp_driver->frame_buf_cache,
					frame_buf);
		}
		mutex_unlock(&frame->bufs.lock);
		DEINIT_MSM_CVP_LIST(&frame->bufs);
		kmem_cache_free(cvp_driver->frame_cache, frame);
	}
	mutex_unlock(&inst->frames.lock);

	return rc;
}

int msm_cvp_session_init(struct msm_cvp_inst *inst)
{
	int rc = 0;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	dprintk(CVP_DBG, "%s: inst %pK (%#x)\n", __func__,
		inst, hash32_ptr(inst->session));

	/* set default frequency */
	inst->clk_data.core_id = 0;
	inst->clk_data.min_freq = 1000;
	inst->clk_data.ddr_bw = 1000;
	inst->clk_data.sys_cache_bw = 1000;

	inst->prop.type = HFI_SESSION_CV;
	if (inst->session_type == MSM_CVP_KERNEL)
		inst->prop.type = HFI_SESSION_DME;

	inst->prop.kernel_mask = 0xFFFFFFFF;
	inst->prop.priority = 0;
	inst->prop.is_secure = 0;
	inst->prop.dsp_mask = 0;

	return rc;
}
