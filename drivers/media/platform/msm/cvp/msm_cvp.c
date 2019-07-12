// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include "msm_cvp.h"
#include "cvp_hfi.h"
#include <synx_api.h>
#include "cvp_core_hfi.h"

void print_cvp_internal_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct msm_cvp_internal_buffer *cbuf)
{
	if (!(tag & msm_cvp_debug) || !inst || !cbuf)
		return;

	dprintk(tag,
		"%s: %x : idx %2d fd %d off %d daddr %x size %d type %d flags 0x%x\n",
		str, hash32_ptr(inst->session), cbuf->buf.index, cbuf->buf.fd,
		cbuf->buf.offset, cbuf->smem.device_addr, cbuf->buf.size,
		cbuf->buf.type, cbuf->buf.flags);
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

static int msm_cvp_scale_clocks_and_bus(struct msm_cvp_inst *inst)
{
	int rc = 0;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = msm_cvp_set_clocks(inst->core);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: failed set_clocks for inst %pK (%#x)\n",
			__func__, inst, hash32_ptr(inst->session));
		goto exit;
	}

	rc = msm_cvp_comm_vote_bus(inst->core);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: failed vote_bus for inst %pK (%#x)\n",
			__func__, inst, hash32_ptr(inst->session));
		goto exit;
	}

exit:
	return rc;
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
		if (cbuf->buf.fd == in_buf->fd &&
			cbuf->buf.size == in_buf->size &&
			cbuf->buf.offset == in_buf->offset) {
			*iova = cbuf->smem.device_addr + cbuf->buf.offset;
			print_client_buffer(CVP_DBG, "found", inst, &cbuf->buf);
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
		print_client_buffer(CVP_ERR, "duplicate", inst, buf);
		return -EINVAL;
	}

	cbuf = kmem_cache_zalloc(inst->internal_buf_cache, GFP_KERNEL);
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
			 buf->size, buf->offset, buf->index,
			(uint32_t)cbuf->smem.device_addr);
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
	kmem_cache_free(inst->internal_buf_cache, cbuf);
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
		rc = cvp_dsp_deregister_buffer((uint32_t)cbuf->smem.device_addr,
			buf->index, buf->size, hash32_ptr(session));
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

	kmem_cache_free(inst->internal_buf_cache, cbuf);
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
		print_client_buffer(CVP_ERR, "duplicate", inst, &cbuf->buf);
		return -EINVAL;
	}

	cbuf = kmem_cache_zalloc(inst->internal_buf_cache, GFP_KERNEL);
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
		print_client_buffer(CVP_ERR, "map failed", inst, &cbuf->buf);
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
	kmem_cache_free(inst->internal_buf_cache, cbuf);
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

static int msm_cvp_map_buf_cpu(struct msm_cvp_inst *inst,
				struct cvp_buf_type *in_buf,
				u32 *iova,
				struct msm_cvp_frame *frame)
{
	int rc = 0;
	struct msm_cvp_internal_buffer *cbuf;
	struct msm_cvp_frame_buf *frame_buf;

	if (!inst || !iova || !frame) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = msm_cvp_session_get_iova_addr(inst, in_buf, iova);
	if (!rc && *iova != 0)
		return 0;

	cbuf = kmem_cache_zalloc(inst->internal_buf_cache, GFP_KERNEL);
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
	cbuf->smem.offset = 0;
	rc = msm_cvp_smem_map_dma_buf(inst, &cbuf->smem);
	if (rc) {
		print_client_buffer(CVP_ERR, "map failed", inst, &cbuf->buf);
		goto exit;
	}

	mutex_lock(&inst->cvpcpubufs.lock);
	list_add_tail(&cbuf->list, &inst->cvpcpubufs.list);
	mutex_unlock(&inst->cvpcpubufs.lock);

	__msm_cvp_cache_operations(cbuf);

	*iova = cbuf->smem.device_addr + cbuf->buf.offset;

	frame_buf = kmem_cache_zalloc(inst->frame_buf_cache, GFP_KERNEL);
	if (!frame_buf) {
		rc = -ENOMEM;
		goto exit2;
	}

	memcpy(&frame_buf->buf, in_buf, sizeof(frame_buf->buf));

	mutex_lock(&frame->bufs.lock);
	list_add_tail(&frame_buf->list, &frame->bufs.list);
	mutex_unlock(&frame->bufs.lock);

	print_client_buffer(CVP_DBG, "map", inst, &cbuf->buf);
	return rc;

exit2:
	if (cbuf->smem.device_addr)
		msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
	mutex_lock(&inst->cvpcpubufs.lock);
	list_del(&cbuf->list);
	mutex_unlock(&inst->cvpcpubufs.lock);
exit:
	kmem_cache_free(inst->internal_buf_cache, cbuf);
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
		if (cbuf->buf.fd == buf->fd &&
			cbuf->buf.size == buf->size &&
			cbuf->buf.offset == buf->offset) {
			list_del(&cbuf->list);
			msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
			print_client_buffer(CVP_DBG, "unmap", inst, &cbuf->buf);
			kmem_cache_free(inst->internal_buf_cache, cbuf);
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
				kmem_cache_free(inst->frame_buf_cache,
						frame_buf);
			}
			mutex_unlock(&frame->bufs.lock);
			DEINIT_MSM_CVP_LIST(&frame->bufs);
			kmem_cache_free(inst->frame_cache, frame);
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
		msm_cvp_comm_kill_session(inst);
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

		if (inst->state >= MSM_CVP_CLOSE_DONE) {
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
		kmem_cache_free(inst->session_queue.msg_cache, msg);
	}

exit:
	s->cur_cmd_type = 0;
	cvp_put_inst(inst);
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

		frame = kmem_cache_zalloc(inst->frame_cache, GFP_KERNEL);
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

		if (!buf_ptr->fd)
			continue;

		if (version >= 1) {
			new_buf = (struct cvp_buf_type *)buf_ptr;
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
					inst->frame_buf_cache,
					frame_buf);
				}
				DEINIT_MSM_CVP_LIST(&frame->bufs);
				kmem_cache_free(inst->frame_cache,
						frame);
				return rc;
			}
			new_buf->fd = iova;
		} else {
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

	if (!inst || !inst->core || !in_pkt) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	inst->cur_cmd_type = CVP_KMD_SEND_CMD_PKT;
	sq = &inst->session_queue;
	spin_lock(&sq->lock);
	if (sq->state != QUEUE_ACTIVE) {
		spin_unlock(&sq->lock);
		dprintk(CVP_ERR, "%s: invalid queue state\n", __func__);
		rc = -EINVAL;
		goto exit;
	}
	spin_unlock(&sq->lock);

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

	if (in_offset && in_buf_num) {
		offset = in_offset;
		buf_num = in_buf_num;
	}

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
	unsigned long timeout_ms = 1000;
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
					ica_enabled = 1;
					/*
					 * Increase loop count to skip fence
					 * waiting on downscale image.
					 */
					i = i+1;
				}
			}
		}

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
			rc = wait_for_sess_signal_receipt(inst,
					HAL_SESSION_DME_FRAME_CMD_DONE);
			if (rc) {
				dprintk(CVP_ERR,
				"%s: wait for signal failed, rc %d\n",
				__func__, rc);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
		}

		if (ica_enabled) {
			rc = synx_import(fence[2], fence[3], &synx_obj);
			if (rc) {
				dprintk(CVP_ERR, "%s: synx_import failed\n",
					__func__);
				goto exit;
			}
			rc = synx_signal(synx_obj, synx_state);
			if (rc) {
				dprintk(CVP_ERR, "%s: synx_signal failed\n",
					__func__);
				goto exit;
			}
			if (synx_get_status(synx_obj) !=
				SYNX_STATE_SIGNALED_SUCCESS) {
				dprintk(CVP_ERR,
					"%s: synx_get_status failed\n",
					__func__);
				goto exit;
			}
			rc = synx_release(synx_obj);
			if (rc) {
				dprintk(CVP_ERR, "%s: synx_release failed\n",
					__func__);
				goto exit;
			}
		}

		rc = synx_import(fence[((HFI_DME_BUF_NUM-1)<<1)],
				fence[((HFI_DME_BUF_NUM-1)<<1)+1],
				&synx_obj);
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
			dprintk(CVP_ERR, "%s: synx_release failed\n",
				__func__);
			goto exit;
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
			rc = wait_for_sess_signal_receipt(inst,
					HAL_SESSION_ICA_FRAME_CMD_DONE);
			if (rc)	{
				dprintk(CVP_ERR,
				"%s: wait for signal failed, rc %d\n",
				__func__, rc);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
		}

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
	default:
		dprintk(CVP_ERR, "%s: unknown hfi cmd type 0x%x\n",
			__func__, fence_thread_data->arg_type);
		rc = -EINVAL;
		goto exit;
		break;
	}

exit:
	kmem_cache_free(inst->fence_data_cache, fence_thread_data);
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
	fence_thread_data = kmem_cache_alloc(inst->fence_data_cache,
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

	rc = msm_cvp_map_buf(inst, in_pkt, offset, buf_num);
	if (rc)
		goto exit;

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
		kmem_cache_free(inst->fence_data_cache, fence_thread_data);
		dprintk(CVP_ERR, "%s fail to create kthread\n", __func__);
		rc = -ECHILD;
		goto exit;
	}

	return 0;

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

/**
 * adjust_bw_freqs(): calculate CVP clock freq and bw required to sustain
 * required use case.
 */
static int adjust_bw_freqs(void)
{
	struct msm_cvp_core *core;
	struct msm_cvp_inst *inst;
	struct iris_hfi_device *hdev;
	struct bus_info *bus;
	struct clock_set *clocks;
	struct clock_info *cl;
	struct allowed_clock_rates_table *tbl = NULL;
	unsigned int tbl_size;
	unsigned int cvp_min_rate, cvp_max_rate, max_bw;
	unsigned long core_sum = 0, ctlr_sum = 0, fw_sum = 0;
	unsigned long op_core_max = 0, op_ctlr_max = 0, op_fw_max = 0;
	unsigned long bw_sum = 0;
	int i, rc = 0;

	core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);

	hdev = core->device->hfi_device_data;
	clocks = &core->resources.clock_set;
	cl = &clocks->clock_tbl[clocks->count - 1];
	tbl = core->resources.allowed_clks_tbl;
	tbl_size = core->resources.allowed_clks_tbl_size;
	cvp_min_rate = tbl[0].clock_rate;
	cvp_max_rate = tbl[tbl_size - 1].clock_rate;
	bus = &core->resources.bus_set.bus_tbl[1];
	max_bw = bus->range[1];

	list_for_each_entry(inst, &core->instances, list) {
		core_sum += inst->power.clock_cycles_a;
		ctlr_sum += inst->power.clock_cycles_b;
		fw_sum += inst->power.reserved[0];
		op_core_max = (op_core_max >= inst->power.reserved[1]) ?
			op_core_max : inst->power.reserved[1];
		op_ctlr_max = (op_ctlr_max >= inst->power.reserved[2]) ?
			op_ctlr_max : inst->power.reserved[2];
		op_fw_max = (op_fw_max >= inst->power.reserved[3]) ?
			op_fw_max : inst->power.reserved[3];
		bw_sum += inst->power.ddr_bw;
	}

	core_sum = max_3(core_sum, ctlr_sum, fw_sum);
	op_core_max = max_3(op_core_max, op_ctlr_max, op_fw_max);
	op_core_max = (op_core_max > tbl[tbl_size - 1].clock_rate) ?
				tbl[tbl_size - 1].clock_rate : op_core_max;
	core_sum = (core_sum >= op_core_max) ? core_sum : op_core_max;

	if (core_sum < tbl[0].clock_rate) {
		core_sum = tbl[0].clock_rate;
	} else {
		for (i = 1; i < tbl_size; i++)
			if (core_sum <= tbl[i].clock_rate)
				break;

		if (i == tbl_size) {
			dprintk(CVP_WARN, "%s out of range %llx\n",
					__func__, core_sum);
			return -ENOTSUPP;
		}
		core_sum = tbl[i].clock_rate;
	}

	if (bw_sum > max_bw)
		bw_sum = max_bw;

	dprintk(CVP_DBG, "%s %d %lld %lld\n", __func__, core_sum, bw_sum, 0);
	if (!cl->has_scaling) {
		dprintk(CVP_ERR, "Cannot scale CVP clock\n");
		return -EINVAL;
	}

	rc = clk_set_rate(cl->clk, core_sum);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to set clock rate %u %s: %d %s\n",
			core_sum, cl->name, rc, __func__);
		return rc;
	}
	hdev->clk_freq = core_sum;
	rc = msm_bus_scale_update_bw(bus->client,
			bw_sum, 0);
	if (rc)
		dprintk(CVP_ERR, "Failed voting bus %s to ab %u\n",
			bus->name, bw_sum);

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
	if (rc)
		dprintk(CVP_ERR, "Instance %pK power request out of range\n");

	mutex_unlock(&core->lock);
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
	if (inst->state >= MSM_CVP_OPEN && inst->state < MSM_CVP_STOP) {
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
				"Incorrect session state %d for command %d",
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
	if (!session) {
		dprintk(CVP_ERR, "%s: invalid session\n", __func__);
		return -EINVAL;
	}

	rc = msm_cvp_comm_try_state(inst, MSM_CVP_CLOSE_DONE);
	if (rc)
		dprintk(CVP_ERR, "%s: close failed\n", __func__);

	inst->clk_data.min_freq = 0;
	inst->clk_data.ddr_bw = 0;
	inst->clk_data.sys_cache_bw = 0;
	rc = msm_cvp_scale_clocks_and_bus(inst);
	if (rc)
		dprintk(CVP_ERR, "%s: failed to scale_clocks_and_bus\n",
			__func__);

	mutex_lock(&inst->cvpcpubufs.lock);
	list_for_each_entry_safe(cbuf, dummy, &inst->cvpcpubufs.list,
			list) {
		print_client_buffer(CVP_DBG, "remove from cvpcpubufs",
				inst, &cbuf->buf);
		msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
		list_del(&cbuf->list);
		kmem_cache_free(inst->internal_buf_cache, cbuf);
	}
	mutex_unlock(&inst->cvpcpubufs.lock);

	mutex_lock(&inst->cvpdspbufs.lock);
	list_for_each_entry_safe(cbuf, dummy, &inst->cvpdspbufs.list,
			list) {
		print_client_buffer(CVP_DBG, "remove from cvpdspbufs",
				inst, &cbuf->buf);
		rc = cvp_dsp_deregister_buffer(
			(uint32_t)cbuf->smem.device_addr,
			cbuf->buf.index, cbuf->buf.size,
			hash32_ptr(session));
		if (rc)
			dprintk(CVP_ERR,
				"%s: failed dsp deregistration fd=%d rc=%d",
				__func__, cbuf->buf.fd, rc);

		msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
		list_del(&cbuf->list);
		kmem_cache_free(inst->internal_buf_cache, cbuf);
	}
	mutex_unlock(&inst->cvpdspbufs.lock);

	mutex_lock(&inst->frames.lock);
	list_for_each_entry_safe(frame, dummy1, &inst->frames.list, list) {
		list_del(&frame->list);
		mutex_lock(&frame->bufs.lock);
		list_for_each_entry_safe(frame_buf, dummy2, &frame->bufs.list,
									list) {
			struct cvp_buf_type *buf = &frame_buf->buf;

			dprintk(CVP_ERR,
				"%s: %x : fd %d off %d size %d flags 0x%x\n",
				"remove from frame list",
				hash32_ptr(inst->session),
				buf->fd, buf->offset, buf->size, buf->flags);

			list_del(&frame_buf->list);
			kmem_cache_free(inst->frame_buf_cache, frame_buf);
		}
		mutex_unlock(&frame->bufs.lock);
		DEINIT_MSM_CVP_LIST(&frame->bufs);
		kmem_cache_free(inst->frame_cache, frame);
	}
	mutex_unlock(&inst->frames.lock);

	msm_cvp_comm_free_freq_table(inst);

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
	inst->clk_data.core_id = CVP_CORE_ID_2;
	inst->clk_data.min_freq = 1000;
	inst->clk_data.ddr_bw = 1000;
	inst->clk_data.sys_cache_bw = 1000;

	inst->prop.type = HFI_SESSION_CV;
	inst->prop.kernel_mask = 0xFFFFFFFF;
	inst->prop.priority = 0;
	inst->prop.is_secure = 0;
	inst->prop.dsp_mask = 0;

	return rc;
}
