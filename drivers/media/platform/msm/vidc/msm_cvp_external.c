// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/ion_kernel.h>
#include "msm_cvp_external.h"
#include "msm_vidc_common.h"

static void print_cvp_buffer(u32 tag, const char *str,
		struct msm_vidc_inst *inst, struct msm_cvp_buf *cbuf)
{
	struct msm_cvp_external *cvp;

	if (!(tag & msm_vidc_debug) || !inst || !inst->cvp || !cbuf)
		return;

	cvp = inst->cvp;
	dprintk(tag,
		"%s: %x : idx %d fd %d size %d offset %d dbuf %pK kvaddr %pK\n",
		str, cvp->session_id, cbuf->index, cbuf->fd, cbuf->size,
		cbuf->offset, cbuf->dbuf, cbuf->kvaddr);
}

static int msm_cvp_fill_planeinfo(struct cvp_kmd_color_plane_info *plane_info,
		u32 color_fmt, u32 width, u32 height)
{
	int rc = 0;
	u32 y_stride, y_sclines, uv_stride, uv_sclines;
	u32 y_meta_stride, y_meta_scalines;
	u32 uv_meta_stride, uv_meta_sclines;

	switch (color_fmt) {
	case COLOR_FMT_NV12:
	case COLOR_FMT_P010:
	case COLOR_FMT_NV12_512:
	{
		y_stride = VENUS_Y_STRIDE(color_fmt, width);
		y_sclines = VENUS_Y_SCANLINES(color_fmt, height);
		uv_stride = VENUS_UV_STRIDE(color_fmt, width);
		uv_sclines = VENUS_UV_SCANLINES(color_fmt, height);

		plane_info->stride[HFI_COLOR_PLANE_METADATA] = 0;
		plane_info->stride[HFI_COLOR_PLANE_PICDATA] = y_stride;
		plane_info->stride[HFI_COLOR_PLANE_UV_META] = 0;
		plane_info->stride[HFI_COLOR_PLANE_UV] = uv_stride;
		plane_info->buf_size[HFI_COLOR_PLANE_METADATA] = 0;
		plane_info->buf_size[HFI_COLOR_PLANE_PICDATA] =
			y_stride * y_sclines;
		plane_info->buf_size[HFI_COLOR_PLANE_UV_META] = 0;
		plane_info->buf_size[HFI_COLOR_PLANE_UV] =
			uv_stride * uv_sclines;
		break;
	}
	case COLOR_FMT_NV12_UBWC:
	case COLOR_FMT_NV12_BPP10_UBWC:
	{
		y_meta_stride = VENUS_Y_META_STRIDE(color_fmt, width);
		y_meta_scalines = VENUS_Y_META_SCANLINES(color_fmt, height);
		uv_meta_stride = VENUS_UV_META_STRIDE(color_fmt, width);
		uv_meta_sclines = VENUS_UV_META_SCANLINES(color_fmt, height);

		y_stride = VENUS_Y_STRIDE(color_fmt, width);
		y_sclines = VENUS_Y_SCANLINES(color_fmt, height);
		uv_stride = VENUS_UV_STRIDE(color_fmt, width);
		uv_sclines = VENUS_UV_SCANLINES(color_fmt, height);

		plane_info->stride[HFI_COLOR_PLANE_METADATA] = y_meta_stride;
		plane_info->stride[HFI_COLOR_PLANE_PICDATA] = y_stride;
		plane_info->stride[HFI_COLOR_PLANE_UV_META] = uv_meta_stride;
		plane_info->stride[HFI_COLOR_PLANE_UV] = uv_stride;
		plane_info->buf_size[HFI_COLOR_PLANE_METADATA] =
			MSM_MEDIA_ALIGN(y_meta_stride * y_meta_scalines, 4096);
		plane_info->buf_size[HFI_COLOR_PLANE_PICDATA] =
			MSM_MEDIA_ALIGN(y_stride * y_sclines, 4096);
		plane_info->buf_size[HFI_COLOR_PLANE_UV_META] =
			MSM_MEDIA_ALIGN(uv_meta_stride * uv_meta_sclines, 4096);
		plane_info->buf_size[HFI_COLOR_PLANE_UV] =
			MSM_MEDIA_ALIGN(uv_stride * uv_sclines, 4096);
		break;
	}
	default:
		dprintk(VIDC_ERR, "%s: invalid color_fmt %#x\n",
			__func__, color_fmt);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int msm_cvp_free_buffer(struct msm_vidc_inst *inst,
		struct msm_cvp_buf *buffer)
{
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp || !buffer) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	if (buffer->kvaddr) {
		dma_buf_vunmap(buffer->dbuf, buffer->kvaddr);
		buffer->kvaddr = NULL;
	}
	if (buffer->dbuf) {
		dma_buf_put(buffer->dbuf);
		buffer->dbuf = NULL;
	}
	return 0;
}

static int msm_cvp_allocate_buffer(struct msm_vidc_inst *inst,
		struct msm_cvp_buf *buffer, bool kernel_map)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	int ion_flags = 0;
	unsigned long heap_mask = 0;
	struct dma_buf *dbuf;
	int fd;

	if (!inst || !inst->cvp || !buffer) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	heap_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);
	if (inst->flags & VIDC_SECURE) {
		ion_flags = ION_FLAG_SECURE | ION_FLAG_CP_NON_PIXEL;
		heap_mask = ION_HEAP(ION_SECURE_HEAP_ID);
	}

	dbuf = ion_alloc(buffer->size, heap_mask, ion_flags);
	if (IS_ERR_OR_NULL(dbuf)) {
		dprintk(VIDC_ERR,
			"%s: failed to allocate, size %d heap_mask %#lx flags %d\n",
			__func__, buffer->size, heap_mask, ion_flags);
		rc = -ENOMEM;
		goto error;
	}
	buffer->dbuf = dbuf;

	fd = dma_buf_fd(dbuf, O_CLOEXEC);
	if (fd < 0) {
		dprintk(VIDC_ERR, "%s: failed to get fd\n", __func__);
		rc = -ENOMEM;
		goto error;
	}
	buffer->fd = fd;

	if (kernel_map) {
		buffer->kvaddr = dma_buf_vmap(dbuf);
		if (!buffer->kvaddr) {
			dprintk(VIDC_ERR,
				"%s: dma_buf_vmap failed\n", __func__);
			rc = -EINVAL;
			goto error;
		}
	} else {
		buffer->kvaddr = NULL;
	}
	buffer->index = cvp->buffer_idx++;
	buffer->offset = 0;

	return 0;
error:
	msm_cvp_free_buffer(inst, buffer);
	return rc;
}

static void msm_cvp_deinit_downscale_buffers(struct msm_vidc_inst *inst)
{
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return;
	}
	cvp = inst->cvp;
	dprintk(VIDC_DBG, "%s:\n", __func__);

	if (cvp->src_buffer.dbuf) {
		print_cvp_buffer(VIDC_DBG, "free: src_buffer",
				inst, &cvp->src_buffer);
		if (msm_cvp_free_buffer(inst, &cvp->src_buffer))
			print_cvp_buffer(VIDC_ERR,
				"free failed: src_buffer",
				inst, &cvp->src_buffer);
	}
	if (cvp->ref_buffer.dbuf) {
		print_cvp_buffer(VIDC_DBG, "free: ref_buffer",
				inst, &cvp->ref_buffer);
		if (msm_cvp_free_buffer(inst, &cvp->ref_buffer))
			print_cvp_buffer(VIDC_ERR,
				"free failed: ref_buffer",
				inst, &cvp->ref_buffer);
	}
}

static int msm_cvp_init_downscale_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	if (!cvp->downscale) {
		dprintk(VIDC_DBG, "%s: downscaling not enabled\n", __func__);
		return 0;
	}
	dprintk(VIDC_DBG, "%s:\n", __func__);

	cvp->src_buffer.size = VENUS_BUFFER_SIZE(COLOR_FMT_NV12_UBWC,
			cvp->ds_width, cvp->ds_height);
	rc = msm_cvp_allocate_buffer(inst, &cvp->src_buffer, false);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"allocate failed: src_buffer",
			inst, &cvp->src_buffer);
		goto error;
	}
	print_cvp_buffer(VIDC_DBG, "alloc: src_buffer",
			inst, &cvp->src_buffer);

	cvp->ref_buffer.size = cvp->src_buffer.size;
	rc = msm_cvp_allocate_buffer(inst, &cvp->ref_buffer, false);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"allocate failed: ref_buffer",
			inst, &cvp->ref_buffer);
		goto error;
	}
	print_cvp_buffer(VIDC_DBG, "alloc: ref_buffer",
			inst, &cvp->ref_buffer);

	return rc;

error:
	msm_cvp_deinit_downscale_buffers(inst);
	return rc;
}

static void msm_cvp_deinit_context_buffers(struct msm_vidc_inst *inst)
{
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return;
	}
	cvp = inst->cvp;
	dprintk(VIDC_DBG, "%s:\n", __func__);

	if (cvp->context_buffer.dbuf) {
		print_cvp_buffer(VIDC_DBG, "free: context_buffer",
				inst, &cvp->context_buffer);
		if (msm_cvp_free_buffer(inst, &cvp->context_buffer))
			print_cvp_buffer(VIDC_ERR,
				"free failed: context_buffer",
				inst, &cvp->context_buffer);
	}
	if (cvp->refcontext_buffer.dbuf) {
		print_cvp_buffer(VIDC_DBG, "free: refcontext_buffer",
				inst, &cvp->refcontext_buffer);
		if (msm_cvp_free_buffer(inst, &cvp->refcontext_buffer))
			print_cvp_buffer(VIDC_ERR,
				"free failed: refcontext_buffer",
				inst, &cvp->refcontext_buffer);
	}
}

static int msm_cvp_init_context_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;
	dprintk(VIDC_DBG, "%s:\n", __func__);

	cvp->context_buffer.size = HFI_DME_FRAME_CONTEXT_BUFFER_SIZE;
	rc = msm_cvp_allocate_buffer(inst, &cvp->context_buffer, false);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"allocate failed: context_buffer",
			inst, &cvp->context_buffer);
		goto error;
	}
	print_cvp_buffer(VIDC_DBG, "alloc: context_buffer",
			inst, &cvp->context_buffer);

	cvp->refcontext_buffer.size = cvp->context_buffer.size;
	rc = msm_cvp_allocate_buffer(inst, &cvp->refcontext_buffer, false);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"allocate failed: refcontext_buffer",
			inst, &cvp->refcontext_buffer);
		goto error;
	}
	print_cvp_buffer(VIDC_DBG, "alloc: refcontext_buffer",
			inst, &cvp->refcontext_buffer);

	return rc;

error:
	msm_cvp_deinit_context_buffers(inst);
	return rc;
}

static void msm_cvp_deinit_internal_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct cvp_kmd_arg *arg;
	struct msm_cvp_session_release_persist_buffers_packet persist2_packet;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return;
	}

	cvp = inst->cvp;
	dprintk(VIDC_DBG, "%s:\n", __func__);

	if (cvp->output_buffer.dbuf) {
		print_cvp_buffer(VIDC_DBG, "free: output_buffer",
				inst, &cvp->output_buffer);
		rc = msm_cvp_free_buffer(inst, &cvp->output_buffer);
		if (rc)
			print_cvp_buffer(VIDC_ERR,
				"unregister failed: output_buffer",
				inst, &cvp->output_buffer);
	}

	if (cvp->persist2_buffer.dbuf) {
		print_cvp_buffer(VIDC_DBG, "free: persist2_buffer",
			inst, &cvp->persist2_buffer);
		memset(&persist2_packet, 0, sizeof(struct
			msm_cvp_session_release_persist_buffers_packet));
		persist2_packet.size = sizeof(struct
			msm_cvp_session_release_persist_buffers_packet);
		persist2_packet.packet_type =
			HFI_CMD_SESSION_CVP_RELEASE_PERSIST_BUFFERS;
		persist2_packet.session_id = cvp->session_id;
		persist2_packet.cvp_op = CVP_DME;
		persist2_packet.persist2_buffer.buffer_addr =
			cvp->persist2_buffer.fd;
		persist2_packet.persist2_buffer.size =
			cvp->persist2_buffer.size;

		arg = kzalloc(sizeof(struct cvp_kmd_arg), GFP_KERNEL);
		if (arg) {
			memset(arg, 0, sizeof(struct cvp_kmd_arg));
			arg->type = CVP_KMD_HFI_PERSIST_CMD;
			memcpy(&(arg->data.pbuf_cmd), &persist2_packet,
			sizeof(struct
			msm_cvp_session_release_persist_buffers_packet));
			rc = msm_cvp_private(cvp->priv,
				CVP_KMD_HFI_PERSIST_CMD, arg);
			if (rc)
				print_cvp_buffer(VIDC_ERR,
					"release failed: persist2_buffer",
					inst, &cvp->persist2_buffer);
			kfree(arg);
		}

		rc = msm_cvp_free_buffer(inst, &cvp->persist2_buffer);
		if (rc)
			print_cvp_buffer(VIDC_ERR,
				"free failed: persist2_buffer",
				inst, &cvp->persist2_buffer);
	}
}

static int msm_cvp_init_internal_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct cvp_kmd_arg *arg;
	struct msm_cvp_session_set_persist_buffers_packet persist2_packet;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	arg = kzalloc(sizeof(struct cvp_kmd_arg), GFP_KERNEL);
	if (!arg)
		return -ENOMEM;

	cvp = inst->cvp;
	dprintk(VIDC_DBG, "%s:\n", __func__);

	cvp->persist2_buffer.size = HFI_DME_INTERNAL_PERSIST_2_BUFFER_SIZE;
	rc = msm_cvp_allocate_buffer(inst, &cvp->persist2_buffer, false);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"allocate failed: persist2_buffer",
			inst, &cvp->persist2_buffer);
		goto error;
	}
	print_cvp_buffer(VIDC_DBG, "alloc: persist2_buffer",
			inst, &cvp->persist2_buffer);

	/* set buffer */
	memset(&persist2_packet, 0,
		sizeof(struct msm_cvp_session_set_persist_buffers_packet));
	persist2_packet.size =
		sizeof(struct msm_cvp_session_set_persist_buffers_packet);
	persist2_packet.packet_type = HFI_CMD_SESSION_CVP_SET_PERSIST_BUFFERS;
	persist2_packet.session_id = cvp->session_id;
	persist2_packet.cvp_op = CVP_DME;
	persist2_packet.persist2_buffer.buffer_addr = cvp->persist2_buffer.fd;
	persist2_packet.persist2_buffer.size = cvp->persist2_buffer.size;

	memset(arg, 0, sizeof(struct cvp_kmd_arg));
	arg->type = CVP_KMD_HFI_PERSIST_CMD;
	if (sizeof(struct cvp_kmd_persist_buf) <
		sizeof(struct msm_cvp_session_set_persist_buffers_packet)) {
		dprintk(VIDC_ERR, "%s: insufficient size\n", __func__);
		goto error;
	}
	memcpy(&(arg->data.pbuf_cmd), &persist2_packet,
		sizeof(struct msm_cvp_session_set_persist_buffers_packet));
	rc = msm_cvp_private(cvp->priv, CVP_KMD_HFI_PERSIST_CMD, arg);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"set failed: persist2_buffer",
			inst, &cvp->persist2_buffer);
		goto error;
	}

	/* allocate one output buffer for internal use */
	cvp->output_buffer.size = HFI_DME_OUTPUT_BUFFER_SIZE;
	rc = msm_cvp_allocate_buffer(inst, &cvp->output_buffer, true);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"allocate failed: output_buffer",
			inst, &cvp->output_buffer);
		goto error;
	}
	print_cvp_buffer(VIDC_DBG, "alloc: output_buffer",
			inst, &cvp->output_buffer);

	kfree(arg);
	return rc;

error:
	msm_cvp_deinit_internal_buffers(inst);
	kfree(arg);
	return rc;
}

static int msm_cvp_prepare_extradata(struct msm_vidc_inst *inst,
		struct vb2_buffer *vb)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct dma_buf *dbuf = NULL;
	char *kvaddr = NULL;
	struct msm_vidc_extradata_header *e_hdr;
	bool input_extradata, found_end;

	if (!inst || !inst->cvp || !vb) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	if (vb->num_planes <= 1) {
		dprintk(VIDC_ERR, "%s: extradata plane not enabled\n",
			__func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	dbuf = dma_buf_get(vb->planes[1].m.fd);
	if (!dbuf) {
		dprintk(VIDC_ERR, "%s: dma_buf_get(%d) failed\n",
			__func__, vb->planes[1].m.fd);
		return -EINVAL;
	}
	if (dbuf->size < vb->planes[1].length) {
		dprintk(VIDC_ERR, "%s: invalid size %d vs %d\n", __func__,
			dbuf->size, vb->planes[1].length);
		rc = -EINVAL;
		goto error;
	}
	rc = dma_buf_begin_cpu_access(dbuf, DMA_BIDIRECTIONAL);
	if (rc) {
		dprintk(VIDC_ERR, "%s: begin_cpu_access failed\n", __func__);
		goto error;
	}
	kvaddr = dma_buf_vmap(dbuf);
	if (!kvaddr) {
		dprintk(VIDC_ERR, "%s: dma_buf_vmap(%d) failed\n",
			__func__, vb->planes[1].m.fd);
		rc = -EINVAL;
		goto error;
	}
	e_hdr = (struct msm_vidc_extradata_header *)((char *)kvaddr +
			vb->planes[1].data_offset);

	input_extradata =
		!!((inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_ROI) ||
		(inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_HDR10PLUS));
	found_end = false;
	while ((char *)e_hdr < (char *)(kvaddr + dbuf->size)) {
		if (!input_extradata) {
			found_end = true;
			break;
		}
		if (e_hdr->type == MSM_VIDC_EXTRADATA_NONE) {
			found_end = true;
			break;
		}
		e_hdr += e_hdr->size;
	}
	if (!found_end) {
		dprintk(VIDC_ERR, "%s: extradata_none not found\n", __func__);
		e_hdr = (struct msm_vidc_extradata_header *)((char *)kvaddr +
				vb->planes[1].data_offset);
	}
	/* check if sufficient space available */
	if (((char *)e_hdr + sizeof(struct msm_vidc_extradata_header) +
			sizeof(struct msm_vidc_enc_cvp_metadata_payload) +
			sizeof(struct msm_vidc_extradata_header)) >
			(kvaddr + dbuf->size)) {
		dprintk(VIDC_ERR,
			"%s: couldn't append extradata, (e_hdr[%pK] - kvaddr[%pK]) %#x, size %d\n",
			__func__, e_hdr, kvaddr, (char *)e_hdr - (char *)kvaddr,
			dbuf->size);
		goto error;
	}
	/* copy payload */
	e_hdr->version = 0x00000001;
	e_hdr->port_index = 1;
	e_hdr->type = MSM_VIDC_EXTRADATA_CVP_METADATA;
	e_hdr->data_size = sizeof(struct msm_vidc_enc_cvp_metadata_payload);
	e_hdr->size = sizeof(struct msm_vidc_extradata_header) +
			e_hdr->data_size;
	dma_buf_begin_cpu_access(cvp->output_buffer.dbuf, DMA_BIDIRECTIONAL);
	memcpy(e_hdr->data, cvp->output_buffer.kvaddr,
			sizeof(struct msm_vidc_enc_cvp_metadata_payload));
	dma_buf_end_cpu_access(cvp->output_buffer.dbuf, DMA_BIDIRECTIONAL);
	/* fill extradata none */
	e_hdr = (struct msm_vidc_extradata_header *)
			((char *)e_hdr + e_hdr->size);
	e_hdr->version = 0x00000001;
	e_hdr->port_index = 1;
	e_hdr->type = MSM_VIDC_EXTRADATA_NONE;
	e_hdr->data_size = 0;
	e_hdr->size = sizeof(struct msm_vidc_extradata_header) +
			e_hdr->data_size;

	dma_buf_vunmap(dbuf, kvaddr);
	dma_buf_end_cpu_access(dbuf, DMA_BIDIRECTIONAL);
	dma_buf_put(dbuf);

	return rc;

error:
	if (kvaddr) {
		dma_buf_vunmap(dbuf, kvaddr);
		dma_buf_end_cpu_access(dbuf, DMA_BIDIRECTIONAL);
	}
	if (dbuf)
		dma_buf_put(dbuf);

	return rc;
}

static int msm_cvp_reference_management(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct msm_cvp_buf temp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	/* swap context buffers */
	memcpy(&temp, &cvp->refcontext_buffer, sizeof(struct msm_cvp_buf));
	memcpy(&cvp->refcontext_buffer, &cvp->context_buffer,
			sizeof(struct msm_cvp_buf));
	memcpy(&cvp->context_buffer, &temp, sizeof(struct msm_cvp_buf));

	/* swap downscale buffers */
	if (cvp->downscale) {
		memcpy(&temp, &cvp->ref_buffer, sizeof(struct msm_cvp_buf));
		memcpy(&cvp->ref_buffer, &cvp->src_buffer,
				sizeof(struct msm_cvp_buf));
		memcpy(&cvp->src_buffer, &temp, sizeof(struct msm_cvp_buf));
	}

	return rc;
}

static int msm_cvp_frame_process(struct msm_vidc_inst *inst,
		struct vb2_buffer *vb)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct cvp_kmd_arg *arg;
	struct msm_cvp_dme_frame_packet *frame;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	arg = kzalloc(sizeof(struct cvp_kmd_arg), GFP_KERNEL);
	if (!arg)
		return -ENOMEM;

	cvp = inst->cvp;
	cvp->fullres_buffer.index = vb->index;
	cvp->fullres_buffer.fd = vb->planes[0].m.fd;
	cvp->fullres_buffer.size = vb->planes[0].length;
	cvp->fullres_buffer.offset = vb->planes[0].data_offset;

	arg->type = CVP_KMD_SEND_CMD_PKT;
	frame = (struct msm_cvp_dme_frame_packet *)&arg->data.hfi_pkt.pkt_data;

	frame->size = sizeof(struct msm_cvp_dme_frame_packet);
	frame->packet_type = HFI_CMD_SESSION_CVP_DME_FRAME;
	frame->session_id = cvp->session_id;
	if (!cvp->framecount)
		frame->skip_mv_calc = 1;
	else
		frame->skip_mv_calc = 0;
	frame->min_fpx_threshold = 2;
	frame->enable_descriptor_lpf = 1;
	frame->enable_ncc_subpel = 1;
	frame->descmatch_threshold = 52;
	frame->ncc_robustness_threshold = 0;

	frame->fullres_srcbuffer.buffer_addr = cvp->fullres_buffer.fd;
	frame->fullres_srcbuffer.size = cvp->fullres_buffer.size;
	frame->videospatialtemporal_statsbuffer.buffer_addr =
			cvp->output_buffer.fd;
	frame->videospatialtemporal_statsbuffer.size =
			cvp->output_buffer.size;

	frame->src_buffer.buffer_addr = cvp->fullres_buffer.fd;
	frame->src_buffer.size = cvp->fullres_buffer.size;
	if (cvp->downscale) {
		frame->src_buffer.buffer_addr = cvp->src_buffer.fd;
		frame->src_buffer.size = cvp->src_buffer.size;
		frame->ref_buffer.buffer_addr = cvp->ref_buffer.fd;
		frame->ref_buffer.size = cvp->ref_buffer.size;
	}
	frame->srcframe_contextbuffer.buffer_addr = cvp->context_buffer.fd;
	frame->srcframe_contextbuffer.size = cvp->context_buffer.size;
	frame->refframe_contextbuffer.buffer_addr = cvp->refcontext_buffer.fd;
	frame->refframe_contextbuffer.size = cvp->refcontext_buffer.size;

	print_cvp_buffer(VIDC_DBG, "input frame", inst, &cvp->fullres_buffer);
	rc = msm_cvp_private(cvp->priv, CVP_KMD_SEND_CMD_PKT, arg);
	if (rc) {
		print_cvp_buffer(VIDC_ERR, "send failed: input frame",
			inst, &cvp->fullres_buffer);
		goto error;
	}
	/* wait for frame done */
	arg->type = CVP_KMD_RECEIVE_MSG_PKT;
	rc = msm_cvp_private(cvp->priv, CVP_KMD_RECEIVE_MSG_PKT, arg);
	if (rc) {
		print_cvp_buffer(VIDC_ERR, "wait failed: input frame",
			inst, &cvp->fullres_buffer);
		goto error;
	}
	cvp->framecount++;

error:
	kfree(arg);
	return rc;
}

int msm_vidc_cvp_preprocess(struct msm_vidc_inst *inst, struct vb2_buffer *vb)
{
	int rc = 0;
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp || !vb) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	if (inst->state != MSM_VIDC_START_DONE) {
		dprintk(VIDC_ERR, "%s: invalid inst state %d\n",
			__func__, inst->state);
		return -EINVAL;
	}
	cvp = inst->cvp;

	rc = msm_cvp_frame_process(inst, vb);
	if (rc) {
		dprintk(VIDC_ERR, "%s: cvp process failed\n", __func__);
		return rc;
	}

	rc = msm_cvp_prepare_extradata(inst, vb);
	if (rc) {
		dprintk(VIDC_ERR, "%s: prepare extradata failed\n", __func__);
		return rc;
	}

	rc = msm_cvp_reference_management(inst);
	if (rc) {
		dprintk(VIDC_ERR, "%s: ref management failed\n", __func__);
		return rc;
	}

	return rc;
}

static int msm_vidc_cvp_deinit(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	dprintk(VIDC_DBG, "%s: cvp session %#x\n", __func__, cvp->session_id);
	msm_cvp_deinit_internal_buffers(inst);
	msm_cvp_deinit_context_buffers(inst);
	msm_cvp_deinit_downscale_buffers(inst);

	return rc;
}

static int msm_vidc_cvp_close(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	dprintk(VIDC_DBG, "%s: cvp session %#x\n", __func__, cvp->session_id);
	rc = msm_cvp_close(cvp->priv);
	if (rc)
		dprintk(VIDC_ERR,
			"%s: cvp close failed with error %d\n", __func__, rc);
	cvp->priv = NULL;

	kfree(inst->cvp);
	inst->cvp = NULL;

	return rc;
}

int msm_vidc_cvp_unprepare_preprocess(struct msm_vidc_inst *inst)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	if (!inst->cvp) {
		dprintk(VIDC_DBG, "%s: cvp not enabled or closed\n", __func__);
		return 0;
	}

	msm_vidc_cvp_deinit(inst);
	msm_vidc_cvp_close(inst);

	return 0;
}

static int msm_vidc_cvp_init(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct v4l2_format *f;
	struct cvp_kmd_arg *arg;
	struct msm_cvp_dme_basic_config_packet *dmecfg;
	u32 color_fmt;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	arg = kzalloc(sizeof(struct cvp_kmd_arg), GFP_KERNEL);
	if (!arg)
		return -ENOMEM;

	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	cvp = inst->cvp;

	cvp->framecount = 0;
	cvp->width = f->fmt.pix_mp.width;
	cvp->height = f->fmt.pix_mp.height;
	color_fmt = msm_comm_convert_color_fmt(f->fmt.pix_mp.pixelformat);

	/* enable downscale always */
	cvp->downscale = true;
	if (cvp->width * cvp->height < 640 * 480) {
		cvp->ds_width = cvp->width;
		cvp->ds_height = cvp->height;
	} else if (cvp->width * cvp->height < 1920 * 1080) {
		if (cvp->ds_width >= cvp->ds_height) {
			cvp->ds_width = 480;
			cvp->ds_height = 270;
		} else {
			cvp->ds_width = 270;
			cvp->ds_height = 480;
		}
	} else {
		cvp->ds_width = cvp->width / 4;
		cvp->ds_height = cvp->height / 4;
	}
	dprintk(VIDC_DBG,
		"%s: pixelformat %#x, wxh %dx%d downscale %d ds_wxh %dx%d\n",
		__func__, f->fmt.pix_mp.pixelformat,
		cvp->width, cvp->height, cvp->downscale,
		cvp->ds_width, cvp->ds_height);

	memset(arg, 0, sizeof(struct cvp_kmd_arg));
	arg->type = CVP_KMD_SEND_CMD_PKT;
	dmecfg = (struct msm_cvp_dme_basic_config_packet *)
			&arg->data.hfi_pkt.pkt_data;
	dmecfg->size = sizeof(struct msm_cvp_dme_basic_config_packet);
	dmecfg->packet_type = HFI_CMD_SESSION_CVP_DME_BASIC_CONFIG;
	dmecfg->session_id = cvp->session_id;
	/* source buffer format should be NV12_UBWC always */
	dmecfg->srcbuffer_format = HFI_COLOR_FORMAT_NV12_UBWC;
	dmecfg->src_width = cvp->ds_width;
	dmecfg->src_height = cvp->ds_height;
	rc = msm_cvp_fill_planeinfo(&dmecfg->srcbuffer_planeinfo,
		COLOR_FMT_NV12_UBWC, dmecfg->src_width, dmecfg->src_height);
	if (rc)
		goto error;
	dmecfg->fullresbuffer_format = msm_comm_get_hfi_uncompressed(
			f->fmt.pix_mp.pixelformat);
	dmecfg->fullres_width = cvp->width;
	dmecfg->fullres_height = cvp->height;
	rc = msm_cvp_fill_planeinfo(&dmecfg->fullresbuffer_planeinfo,
		color_fmt, dmecfg->fullres_width, dmecfg->fullres_height);
	if (rc)
		goto error;
	dmecfg->ds_enable = cvp->downscale;
	dmecfg->enable_lrme_robustness = 1;
	dmecfg->enable_inlier_tracking = 1;
	rc = msm_cvp_private(cvp->priv, CVP_KMD_SEND_CMD_PKT, arg);
	if (rc) {
		dprintk(VIDC_ERR, "%s: cvp configuration failed\n", __func__);
		goto error;
	}

	rc = msm_cvp_init_downscale_buffers(inst);
	if (rc)
		goto error;
	rc = msm_cvp_init_internal_buffers(inst);
	if (rc)
		goto error;
	rc = msm_cvp_init_context_buffers(inst);
	if (rc)
		goto error;

	kfree(arg);
	return rc;

error:
	msm_vidc_cvp_deinit(inst);
	kfree(arg);
	return rc;
}

static int msm_vidc_cvp_open(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct cvp_kmd_arg *arg;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	arg = kzalloc(sizeof(struct cvp_kmd_arg), GFP_KERNEL);
	if (!arg)
		return -ENOMEM;

	inst->cvp = kzalloc(sizeof(struct msm_cvp_external), GFP_KERNEL);
	if (!inst->cvp) {
		dprintk(VIDC_ERR, "%s: failed to allocate\n", __func__);
		rc = -ENOMEM;
		goto error;
	}
	cvp = inst->cvp;

	dprintk(VIDC_DBG, "%s: opening cvp\n", __func__);
	cvp->priv = msm_cvp_open(0, MSM_VIDC_CVP);
	if (!cvp->priv) {
		dprintk(VIDC_ERR, "%s: failed to open cvp session\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	memset(arg, 0, sizeof(struct cvp_kmd_arg));
	arg->type = CVP_KMD_GET_SESSION_INFO;
	rc = msm_cvp_private(cvp->priv, CVP_KMD_GET_SESSION_INFO, arg);
	if (rc) {
		dprintk(VIDC_ERR, "%s: get_session_info failed\n", __func__);
		goto error;
	}
	cvp->session_id = arg->data.session.session_id;
	dprintk(VIDC_DBG, "%s: cvp session id %#x\n",
		__func__, cvp->session_id);

	kfree(arg);
	return 0;

error:
	msm_vidc_cvp_close(inst);
	kfree(inst->cvp);
	inst->cvp = NULL;
	kfree(arg);
	return rc;
}

int msm_vidc_cvp_prepare_preprocess(struct msm_vidc_inst *inst)
{
	int rc;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = msm_vidc_cvp_open(inst);
	if (rc)
		return rc;

	rc = msm_vidc_cvp_init(inst);
	if (rc)
		return rc;

	return 0;
}
