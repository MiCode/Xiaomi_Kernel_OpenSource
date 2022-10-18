// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#include <linux/delay.h>
#include "cam_io_util.h"
#include "cam_hw_intf.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"
#include "cre_core.h"
#include "cre_hw.h"
#include "cre_dev_intf.h"
#include "cre_bus_wr.h"
#include <media/cam_cre.h>

static struct cre_bus_wr *wr_info;

#define update_cre_reg_set(cre_reg_buf, off, val) \
	do {                                           \
		cre_reg_buf->wr_reg_set[cre_reg_buf->num_wr_reg_set].offset = (off); \
		cre_reg_buf->wr_reg_set[cre_reg_buf->num_wr_reg_set].value = (val); \
		cre_reg_buf->num_wr_reg_set++; \
	} while (0)

static int cam_cre_translate_write_format(struct plane_info p_info,
	struct cam_cre_bus_wr_client_reg_val *wr_client_reg_val)
{
	CAM_DBG(CAM_CRE, "width 0x%x, height 0x%x stride 0x%x alignment 0x%x",
		p_info.width, p_info.height, p_info.stride, p_info.alignment);

	/* Number of output pixels */
	wr_client_reg_val->width     = p_info.width;
	/* Number of output bytes */
	wr_client_reg_val->stride    = p_info.stride;
	/* Number of output lines */
	wr_client_reg_val->height    = p_info.height;
	wr_client_reg_val->alignment = p_info.alignment;

	/*
	 * Update packer format to zero irrespective of output format
	 * This is as per the recomendation from CRE HW team for CRE 1.0
	 * This logic has to be updated for CRE 1.1
	 */
	wr_client_reg_val->format = 0;

	return 0;
}

static int cam_cre_bus_wr_out_port_idx(uint32_t output_port_id)
{
	int i;

	for (i = 0; i < CRE_MAX_OUT_RES; i++)
		if (wr_info->out_port_to_wm[i].output_port_id == output_port_id)
			return i;

	return -EINVAL;
}

static int cam_cre_bus_wr_reg_set_update(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	int i;
	uint32_t num_reg_set;
	struct cre_reg_set *wr_reg_set;
	struct cam_cre_dev_reg_set_update *reg_set_upd_cmd =
		(struct cam_cre_dev_reg_set_update *)data;

	num_reg_set = reg_set_upd_cmd->cre_reg_buf.num_wr_reg_set;
	wr_reg_set = reg_set_upd_cmd->cre_reg_buf.wr_reg_set;

	for (i = 0; i < num_reg_set; i++) {
		CAM_DBG(CAM_CRE, "base 0x%x CRE value 0x%x offset 0x%x",
				cam_cre_hw_info->bus_wr_reg_offset->base,
				wr_reg_set[i].value, wr_reg_set[i].offset);
		cam_io_w_mb(wr_reg_set[i].value,
			cam_cre_hw_info->bus_wr_reg_offset->base + wr_reg_set[i].offset);
	}
	return 0;
}

static int cam_cre_bus_wr_release(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	if (ctx_id < 0 || ctx_id >= CRE_CTX_MAX) {
		CAM_ERR(CAM_CRE, "Invalid data: %d", ctx_id);
		return -EINVAL;
	}

	vfree(wr_info->bus_wr_ctx[ctx_id]);
	wr_info->bus_wr_ctx[ctx_id] = NULL;

	return 0;
}

static int cam_cre_bus_wr_update(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, struct cam_cre_dev_prepare_req *prepare,
	int batch_idx, int io_idx,
	struct cre_reg_buffer *cre_reg_buf)
{
	int rc, k, out_port_idx;
	uint32_t req_idx;
	uint32_t val = 0;
	uint32_t iova_base, iova_offset;
	struct cam_hw_prepare_update_args *prepare_args;
	struct cam_cre_ctx *ctx_data;
	struct cam_cre_request *cre_request;
	struct cre_io_buf *io_buf;
	struct cam_cre_bus_wr_reg *wr_reg;
	struct cam_cre_bus_wr_client_reg *wr_reg_client;
	struct cam_cre_bus_wr_reg_val *wr_reg_val;
	struct cam_cre_bus_wr_client_reg_val *wr_client_reg_val;

	if (ctx_id < 0 || !prepare) {
		CAM_ERR(CAM_CRE, "Invalid data: %d %x", ctx_id, prepare);
		return -EINVAL;
	}

	if (batch_idx >= CRE_MAX_BATCH_SIZE) {
		CAM_ERR(CAM_CRE, "Invalid batch idx: %d", batch_idx);
		return -EINVAL;
	}

	if (io_idx >= CRE_MAX_IO_BUFS) {
		CAM_ERR(CAM_CRE, "Invalid IO idx: %d", io_idx);
		return -EINVAL;
	}

	prepare_args = prepare->prepare_args;
	ctx_data = prepare->ctx_data;
	req_idx = prepare->req_idx;

	cre_request = ctx_data->req_list[req_idx];
	wr_reg = cam_cre_hw_info->bus_wr_reg_offset;
	wr_reg_val = cam_cre_hw_info->bus_wr_reg_val;

	CAM_DBG(CAM_CRE, "req_idx = %d req_id = %lld",
		req_idx, cre_request->request_id);

	io_buf = cre_request->io_buf[batch_idx][io_idx];
	CAM_DBG(CAM_CRE, "batch = %d io buf num = %d dir = %d rsc %d",
		batch_idx, io_idx, io_buf->direction, io_buf->resource_type);

	out_port_idx =
		cam_cre_bus_wr_out_port_idx(io_buf->resource_type);
	if (out_port_idx < 0) {
		CAM_ERR(CAM_CRE, "Invalid idx for rsc type: %d",
			io_buf->resource_type);
		return -EINVAL;
	}

	CAM_DBG(CAM_CRE, "out_port_idx = %d", out_port_idx);

	for (k = 0; k < io_buf->num_planes; k++) {
		wr_reg_client = &wr_reg->wr_clients[out_port_idx];
		wr_client_reg_val = &wr_reg_val->wr_clients[out_port_idx];

		CAM_DBG(CAM_CRE, "wr_reg_client %x wr_client_reg_val %x",
				wr_reg_client, wr_client_reg_val, wr_client_reg_val);

		/* Core cfg: enable, Mode */
		val = 0;
		val |= ((wr_client_reg_val->mode &
			wr_client_reg_val->mode_mask) <<
			wr_client_reg_val->mode_shift);
		val |= wr_client_reg_val->client_en;

		update_cre_reg_set(cre_reg_buf,
			wr_reg->offset + wr_reg_client->client_cfg,
			val);

		/*
		 * As CRE have 36 Bit addressing support Image Address
		 * register will have 28 bit MSB of 36 bit iova.
		 * and addr_config will have 8 bit byte offset.
		 */
		iova_base = CAM_36BIT_INTF_GET_IOVA_BASE(io_buf->p_info[k].iova_addr);
		update_cre_reg_set(cre_reg_buf,
			wr_reg->offset + wr_reg_client->img_addr,
			iova_base);
		iova_offset = CAM_36BIT_INTF_GET_IOVA_OFFSET(io_buf->p_info[k].iova_addr);
		update_cre_reg_set(cre_reg_buf,
			wr_reg->offset + wr_reg_client->addr_cfg,
			iova_offset);

		rc = cam_cre_translate_write_format(io_buf->p_info[k],
				wr_client_reg_val);
		if (rc < 0)
			return -EINVAL;

		/* Buffer size */
		val = 0;
		val = wr_client_reg_val->width;
		val |= (wr_client_reg_val->height &
				wr_client_reg_val->height_mask) <<
				wr_client_reg_val->height_shift;
		update_cre_reg_set(cre_reg_buf,
			wr_reg->offset + wr_reg_client->img_cfg_0,
			val);

		/* stride */
		update_cre_reg_set(cre_reg_buf,
			wr_reg->offset + wr_reg_client->img_cfg_2,
			wr_client_reg_val->stride);

		val = 0;
		val |= ((wr_client_reg_val->format &
			wr_client_reg_val->format_mask) <<
			wr_client_reg_val->format_shift);
		val |= ((wr_client_reg_val->alignment &
			wr_client_reg_val->alignment_mask) <<
			wr_client_reg_val->alignment_shift);
		/* pack cfg : Format and alignment */
		update_cre_reg_set(cre_reg_buf,
			wr_reg->offset + wr_reg_client->packer_cfg,
			val);

		/* Upadte debug status CFG*/
		val = 0xFFFF;
		update_cre_reg_set(cre_reg_buf,
			wr_reg->offset + wr_reg_client->debug_status_cfg,
			val);
	}

	return 0;
}

static int cam_cre_bus_wr_prepare(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	int i, j = 0;
	uint32_t req_idx;
	struct cam_cre_dev_prepare_req *prepare;
	struct cam_cre_ctx *ctx_data;
	struct cam_cre_request *cre_request;
	struct cre_io_buf *io_buf;
	struct cre_reg_buffer *cre_reg_buf;

	if (ctx_id < 0 || !data) {
		CAM_ERR(CAM_CRE, "Invalid data: %d %x", ctx_id, data);
		return -EINVAL;
	}
	prepare = data;
	ctx_data = prepare->ctx_data;
	req_idx = prepare->req_idx;

	cre_request = ctx_data->req_list[req_idx];

	CAM_DBG(CAM_CRE, "req_idx = %d req_id = %lld num_io_bufs = %d",
		req_idx, cre_request->request_id, cre_request->num_io_bufs[0]);

	for (i = 0; i < cre_request->num_batch; i++) {
		cre_reg_buf = &cre_request->cre_reg_buf[i];
		for (j = 0; j < cre_request->num_io_bufs[i]; j++) {
			io_buf = cre_request->io_buf[i][j];
			CAM_DBG(CAM_CRE, "batch = %d io buf num = %d",
				i, j);
			if (io_buf->direction != CAM_BUF_OUTPUT)
				continue;

			rc = cam_cre_bus_wr_update(cam_cre_hw_info,
				ctx_id, prepare, i, j,
				cre_reg_buf);
			if (rc)
				goto end;
		}
	}

end:
	return rc;
}

static int cam_cre_bus_wr_acquire(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0, i;
	struct cam_cre_acquire_dev_info *in_acquire;
	struct cre_bus_wr_ctx *bus_wr_ctx;
	struct cre_bus_out_port_to_wm *out_port_to_wr;
	int out_port_idx;

	if (ctx_id < 0 || !data || ctx_id >= CRE_CTX_MAX) {
		CAM_ERR(CAM_CRE, "Invalid data: %d %x", ctx_id, data);
		return -EINVAL;
	}

	wr_info->bus_wr_ctx[ctx_id] = vzalloc(sizeof(struct cre_bus_wr_ctx));
	if (!wr_info->bus_wr_ctx[ctx_id]) {
		CAM_ERR(CAM_CRE, "Out of memory");
		return -ENOMEM;
	}

	wr_info->bus_wr_ctx[ctx_id]->cre_acquire = data;
	in_acquire = data;
	bus_wr_ctx = wr_info->bus_wr_ctx[ctx_id];
	bus_wr_ctx->num_out_ports = in_acquire->num_out_res;
	bus_wr_ctx->security_flag = in_acquire->secure_mode;

	for (i = 0; i < in_acquire->num_out_res; i++) {
		if (!in_acquire->out_res[i].width)
			continue;

		CAM_DBG(CAM_CRE, "i = %d format = %u width = 0x%x height = 0x%x res_id %d",
			i, in_acquire->out_res[i].format,
			in_acquire->out_res[i].width,
			in_acquire->out_res[i].height,
			in_acquire->in_res[i].res_id);

		out_port_idx =
		cam_cre_bus_wr_out_port_idx(in_acquire->out_res[i].res_id);
		if (out_port_idx < 0) {
			CAM_DBG(CAM_CRE, "Invalid out_port_idx: %d",
				in_acquire->out_res[i].res_id);
			rc = -EINVAL;
			goto end;
		}
		CAM_DBG(CAM_CRE, "out_port_idx %d", out_port_idx);
		out_port_to_wr = &wr_info->out_port_to_wm[out_port_idx];
		if (!out_port_to_wr->num_wm) {
			CAM_DBG(CAM_CRE, "Invalid format for Input port");
			rc = -EINVAL;
			goto end;
		}
	}

end:
	return rc;
}

static int cam_cre_bus_wr_init(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	struct cam_cre_bus_wr_reg_val *bus_wr_reg_val;
	struct cam_cre_bus_wr_reg *bus_wr_reg;
	struct cam_cre_dev_init *dev_init = data;

	if (!cam_cre_hw_info) {
		CAM_ERR(CAM_CRE, "Invalid cam_cre_hw_info");
		return -EINVAL;
	}

	wr_info->cre_hw_info = cam_cre_hw_info;
	bus_wr_reg_val = cam_cre_hw_info->bus_wr_reg_val;
	bus_wr_reg = cam_cre_hw_info->bus_wr_reg_offset;
	bus_wr_reg->base = dev_init->core_info->cre_hw_info->cre_bus_wr_base;

	CAM_DBG(CAM_CRE, "bus_wr_reg->base 0x%x", bus_wr_reg->base);

	cam_io_w_mb(bus_wr_reg_val->irq_mask_0,
		bus_wr_reg->base + bus_wr_reg->irq_mask_0);
	cam_io_w_mb(bus_wr_reg_val->irq_mask_1,
		bus_wr_reg->base + bus_wr_reg->irq_mask_1);

	return 0;
}

static int cam_cre_bus_wr_probe(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	int i, k;
	struct cam_cre_bus_wr_reg_val *bus_wr_reg_val;
	struct cre_bus_out_port_to_wm *out_port_to_wm;
	uint32_t output_port_idx;
	uint32_t wm_idx;

	if (!cam_cre_hw_info) {
		CAM_ERR(CAM_CRE, "Invalid cam_cre_hw_info");
		return -EINVAL;
	}
	wr_info = kzalloc(sizeof(struct cre_bus_wr), GFP_KERNEL);
	if (!wr_info) {
		CAM_ERR(CAM_CRE, "Out of memory");
		return -ENOMEM;
	}

	wr_info->cre_hw_info = cam_cre_hw_info;
	bus_wr_reg_val = cam_cre_hw_info->bus_wr_reg_val;

	for (i = 0; i < bus_wr_reg_val->num_clients; i++) {
		output_port_idx =
			bus_wr_reg_val->wr_clients[i].wm_port_id;
		out_port_to_wm = &wr_info->out_port_to_wm[output_port_idx];
		wm_idx = out_port_to_wm->num_wm;
		out_port_to_wm->output_port_id =
			bus_wr_reg_val->wr_clients[i].output_port_id;
		out_port_to_wm->wm_port_id[wm_idx] =
			bus_wr_reg_val->wr_clients[i].wm_port_id;
		out_port_to_wm->num_wm++;
	}

	for (i = 0; i < CRE_MAX_OUT_RES; i++) {
		out_port_to_wm = &wr_info->out_port_to_wm[i];
		CAM_DBG(CAM_CRE, "output port id = %d",
			out_port_to_wm->output_port_id);
		CAM_DBG(CAM_CRE, "num_wms = %d",
			out_port_to_wm->num_wm);
		for (k = 0; k < out_port_to_wm->num_wm; k++) {
			CAM_DBG(CAM_CRE, "wm port id = %d",
				out_port_to_wm->wm_port_id[k]);
		}
	}

	return 0;
}

static int cam_cre_bus_wr_isr(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	uint32_t irq_status_0, irq_status_1;
	struct cam_cre_bus_wr_reg *bus_wr_reg;
	struct cam_cre_bus_wr_reg_val *bus_wr_reg_val;
	struct cam_cre_irq_data *irq_data = data;
	uint32_t debug_status_0;
	uint32_t debug_status_1;
	uint32_t img_violation_status;
	uint32_t violation_status;
	int i;

	if (!cam_cre_hw_info || !irq_data) {
		CAM_ERR(CAM_CRE, "Invalid cam_cre_hw_info");
		return -EINVAL;
	}

	bus_wr_reg = cam_cre_hw_info->bus_wr_reg_offset;
	bus_wr_reg_val = cam_cre_hw_info->bus_wr_reg_val;

	/* Read and Clear Top Interrupt status */
	irq_status_0 = cam_io_r_mb(bus_wr_reg->base + bus_wr_reg->irq_status_0);
	irq_status_1 = cam_io_r_mb(bus_wr_reg->base + bus_wr_reg->irq_status_1);

	CAM_DBG(CAM_CRE, "BUS irq_status_0 0x%x irq_status_1 0x%x",
		irq_status_0, irq_status_1);

	cam_io_w_mb(irq_status_0,
		bus_wr_reg->base + bus_wr_reg->irq_clear_0);
	cam_io_w_mb(irq_status_1,
		bus_wr_reg->base + bus_wr_reg->irq_clear_1);

	cam_io_w_mb(bus_wr_reg_val->irq_cmd_clear,
		bus_wr_reg->base + bus_wr_reg->irq_cmd);

	if (irq_status_0 & bus_wr_reg_val->cons_violation) {
		irq_data->error = 1;
		CAM_ERR(CAM_CRE, "cre bus wr cons_violation");
	}

	if ((irq_status_0 & bus_wr_reg_val->violation) ||
		(irq_status_0 & bus_wr_reg_val->img_size_violation) ||
		(irq_status_0 & bus_wr_reg_val->cons_violation)) {
		irq_data->error = 1;
		img_violation_status = cam_io_r_mb(bus_wr_reg->base +
			bus_wr_reg->image_size_violation_status);
		violation_status = cam_io_r_mb(bus_wr_reg->base +
			bus_wr_reg->violation_status);

		debug_status_0 = cam_io_r_mb(bus_wr_reg->base +
			bus_wr_reg->wr_clients[0].debug_status_0);
		debug_status_1 = cam_io_r_mb(bus_wr_reg->base +
			bus_wr_reg->wr_clients[0].debug_status_1);
		CAM_ERR(CAM_CRE,
			"violation status 0x%x 0x%x debug status 0/1 0x%x/0x%x",
			violation_status, img_violation_status,
			debug_status_0, debug_status_1);
	}

	if (irq_status_0 & bus_wr_reg_val->comp_buf_done) {
		for (i = 0; i < bus_wr_reg_val->num_clients; i++) {
			if (irq_status_1 & bus_wr_reg_val->
				wr_clients[i].client_buf_done)
				CAM_INFO(CAM_CRE, "Cleint %d Buff done", i);
				irq_data->wr_buf_done = 1 << i;
		}
	}

	return 0;
}

int cam_cre_bus_wr_process(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, uint32_t cmd_id, void *data)
{
	int rc = 0;

	switch (cmd_id) {
	case CRE_HW_PROBE:
		CAM_DBG(CAM_CRE, "CRE_HW_PROBE: E");
		rc = cam_cre_bus_wr_probe(cam_cre_hw_info, ctx_id, data);
		CAM_DBG(CAM_CRE, "CRE_HW_PROBE: X");
		break;
	case CRE_HW_INIT:
		CAM_DBG(CAM_CRE, "CRE_HW_INIT: E");
		rc = cam_cre_bus_wr_init(cam_cre_hw_info, ctx_id, data);
		CAM_DBG(CAM_CRE, "CRE_HW_INIT: X");
		break;
	case CRE_HW_ACQUIRE:
		CAM_DBG(CAM_CRE, "CRE_HW_ACQUIRE: E");
		rc = cam_cre_bus_wr_acquire(cam_cre_hw_info, ctx_id, data);
		CAM_DBG(CAM_CRE, "CRE_HW_ACQUIRE: X");
		break;
	case CRE_HW_RELEASE:
		CAM_DBG(CAM_CRE, "CRE_HW_RELEASE: E");
		rc = cam_cre_bus_wr_release(cam_cre_hw_info, ctx_id, data);
		CAM_DBG(CAM_CRE, "CRE_HW_RELEASE: X");
		break;
	case CRE_HW_PREPARE:
		CAM_DBG(CAM_CRE, "CRE_HW_PREPARE: E");
		rc = cam_cre_bus_wr_prepare(cam_cre_hw_info, ctx_id, data);
		CAM_DBG(CAM_CRE, "CRE_HW_PREPARE: X");
		break;
	case CRE_HW_REG_SET_UPDATE:
		rc = cam_cre_bus_wr_reg_set_update(cam_cre_hw_info, 0, data);
		break;
	case CRE_HW_DEINIT:
	case CRE_HW_START:
	case CRE_HW_STOP:
	case CRE_HW_FLUSH:
	case CRE_HW_CLK_UPDATE:
	case CRE_HW_BW_UPDATE:
	case CRE_HW_RESET:
	case CRE_HW_SET_IRQ_CB:
		rc = 0;
		CAM_DBG(CAM_CRE, "Unhandled cmds: %d", cmd_id);
		break;
	case CRE_HW_ISR:
		rc = cam_cre_bus_wr_isr(cam_cre_hw_info, 0, data);
		break;
	default:
		CAM_ERR(CAM_CRE, "Unsupported cmd: %d", cmd_id);
		break;
	}

	return rc;
}
