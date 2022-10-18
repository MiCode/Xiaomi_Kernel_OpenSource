// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#include <linux/delay.h>
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"
#include "cre_core.h"
#include "cre_hw.h"
#include "cre_dev_intf.h"
#include "cre_bus_rd.h"
#include <media/cam_cre.h>

static struct cre_bus_rd *bus_rd;

#define update_cre_reg_set(cre_reg_buf, off, val) \
	do {                                           \
		cre_reg_buf->rd_reg_set[cre_reg_buf->num_rd_reg_set].offset = (off); \
		cre_reg_buf->rd_reg_set[cre_reg_buf->num_rd_reg_set].value = (val); \
		cre_reg_buf->num_rd_reg_set++; \
	} while (0)

static int cam_cre_bus_rd_in_port_idx(uint32_t input_port_id)
{
	int i;

	for (i = 0; i < CRE_MAX_IN_RES; i++)
		if (bus_rd->in_port_to_rm[i].input_port_id ==
			input_port_id)
			return i;

	return -EINVAL;
}

static void cam_cre_update_read_reg_val(struct plane_info p_info,
	struct cam_cre_bus_rd_client_reg_val *rd_client_reg_val)
{
	switch (p_info.format) {
		case CAM_FORMAT_MIPI_RAW_10:
			rd_client_reg_val->format = 0xd;
			break;
		case CAM_FORMAT_MIPI_RAW_12:
			rd_client_reg_val->format = 0xe;
			break;
		case CAM_FORMAT_MIPI_RAW_14:
			rd_client_reg_val->format = 0xf;
			break;
		case CAM_FORMAT_MIPI_RAW_20:
			rd_client_reg_val->format = 0x13;
			break;
		case CAM_FORMAT_PLAIN128:
			rd_client_reg_val->format = 0x0;
			break;
		default:
			CAM_ERR(CAM_CRE, "Unsupported read format");
			return;
	}

	CAM_DBG(CAM_CRE,
		"format %d width(in bytes) %d height %d stride(in byte) %d",
		p_info.format, p_info.width, p_info.height, p_info.stride);
	CAM_DBG(CAM_CRE, "alignment 0x%x",
		p_info.alignment);

	/* Fetch engine width has to be updated in number of bytes */
	rd_client_reg_val->img_width  = p_info.stride;
	rd_client_reg_val->stride     = p_info.stride;
	rd_client_reg_val->img_height = p_info.height;
	rd_client_reg_val->alignment  = p_info.alignment;
}

static int cam_cre_bus_rd_release(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	if (ctx_id < 0 || ctx_id >= CRE_CTX_MAX) {
		CAM_ERR(CAM_CRE, "Invalid data: %d", ctx_id);
		return -EINVAL;
	}

	vfree(bus_rd->bus_rd_ctx[ctx_id]);
	bus_rd->bus_rd_ctx[ctx_id] = NULL;

	return 0;
}

static int cam_cre_bus_rd_update(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, struct cre_reg_buffer *cre_reg_buf, int batch_idx,
	int io_idx, struct cam_cre_dev_prepare_req *prepare)
{
	int k, in_port_idx;
	uint32_t req_idx, val;
	uint32_t iova_base, iova_offset;
	struct cam_hw_prepare_update_args *prepare_args;
	struct cam_cre_ctx *ctx_data;
	struct cam_cre_request *cre_request;
	struct cre_io_buf *io_buf;
	struct cam_cre_bus_rd_reg *rd_reg;
	struct cam_cre_bus_rd_client_reg *rd_reg_client;
	struct cam_cre_bus_rd_reg_val *rd_reg_val;
	struct cam_cre_bus_rd_client_reg_val *rd_client_reg_val;

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
	CAM_DBG(CAM_CRE, "req_idx = %d req_id = %lld",
		req_idx, cre_request->request_id);
	rd_reg = cam_cre_hw_info->bus_rd_reg_offset;
	rd_reg_val = cam_cre_hw_info->bus_rd_reg_val;
	io_buf = cre_request->io_buf[batch_idx][io_idx];

	CAM_DBG(CAM_CRE,
		"req_idx = %d req_id = %lld rsc %d",
		req_idx, cre_request->request_id,
		io_buf->resource_type);
	CAM_DBG(CAM_CRE, "batch:%d iobuf:%d direction:%d",
		batch_idx, io_idx, io_buf->direction);

	in_port_idx =
	cam_cre_bus_rd_in_port_idx(io_buf->resource_type);

	CAM_DBG(CAM_CRE, "in_port_idx %d", in_port_idx);
	for (k = 0; k < io_buf->num_planes; k++) {
		rd_reg_client = &rd_reg->rd_clients[in_port_idx];
		rd_client_reg_val = &rd_reg_val->rd_clients[in_port_idx];

		/* security cfg */
		update_cre_reg_set(cre_reg_buf,
				rd_reg->offset + rd_reg->security_cfg,
				ctx_data->cre_acquire.secure_mode & 0x1);

		/* enable client */
		update_cre_reg_set(cre_reg_buf,
			rd_reg->offset + rd_reg_client->core_cfg,
			1);

		/* ccif meta data */
		update_cre_reg_set(cre_reg_buf,
			(rd_reg->offset + rd_reg_client->ccif_meta_data),
			0);
		/*
		 * As CRE have 36 Bit addressing support Image Address
		 * register will have 28 bit MSB of 36 bit iova.
		 * and addr_config will have 8 bit byte offset.
		 */
		iova_base = CAM_36BIT_INTF_GET_IOVA_BASE(
				io_buf->p_info[k].iova_addr);
		update_cre_reg_set(cre_reg_buf,
			rd_reg->offset + rd_reg_client->img_addr,
			iova_base);
		iova_offset = CAM_36BIT_INTF_GET_IOVA_OFFSET(
				io_buf->p_info[k].iova_addr);
		update_cre_reg_set(cre_reg_buf,
			rd_reg->offset + rd_reg_client->addr_cfg,
			iova_offset);

		cam_cre_update_read_reg_val(io_buf->p_info[k],
			rd_client_reg_val);

		/* Buffer size */
		update_cre_reg_set(cre_reg_buf,
			rd_reg->offset + rd_reg_client->rd_width,
			rd_client_reg_val->img_width);
		update_cre_reg_set(cre_reg_buf,
			rd_reg->offset + rd_reg_client->rd_height,
			rd_client_reg_val->img_height);

		/* stride */
		update_cre_reg_set(cre_reg_buf,
			rd_reg->offset + rd_reg_client->rd_stride,
			rd_client_reg_val->stride);

		val = 0;
		val |= (rd_client_reg_val->format &
			rd_client_reg_val->format_mask) <<
			rd_client_reg_val->format_shift;
		val |= (rd_client_reg_val->alignment &
			rd_client_reg_val->alignment_mask) <<
			rd_client_reg_val->alignment_shift;
		/* unpacker cfg : format and alignment */
		update_cre_reg_set(cre_reg_buf,
			rd_reg->offset + rd_reg_client->unpacker_cfg,
			val);

		/* Enable Debug cfg */
		val = 0xFFFF;
		update_cre_reg_set(cre_reg_buf,
			rd_reg->offset + rd_reg_client->debug_status_cfg,
			val);
	}

	return 0;
}

static int cam_cre_bus_rd_prepare(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	int i, j;
	uint32_t req_idx;
	struct cam_cre_dev_prepare_req *prepare;
	struct cam_cre_ctx *ctx_data;
	struct cam_cre_request *cre_request;
	struct cre_io_buf *io_buf;
	struct cam_cre_bus_rd_reg *rd_reg;
	struct cam_cre_bus_rd_reg_val *rd_reg_val;
	struct cre_reg_buffer *cre_reg_buf;

	int val;

	if (ctx_id < 0 || !data) {
		CAM_ERR(CAM_CRE, "Invalid data: %d %x", ctx_id, data);
		return -EINVAL;
	}
	prepare = data;

	ctx_data = prepare->ctx_data;
	req_idx = prepare->req_idx;

	cre_request = ctx_data->req_list[req_idx];

	CAM_DBG(CAM_CRE, "req_idx = %d req_id = %lld",
		req_idx, cre_request->request_id);
	rd_reg = cam_cre_hw_info->bus_rd_reg_offset;
	rd_reg_val = cam_cre_hw_info->bus_rd_reg_val;

	for (i = 0; i < cre_request->num_batch; i++) {
		cre_reg_buf = &cre_request->cre_reg_buf[i];
		for (j = 0; j < cre_request->num_io_bufs[i]; j++) {
			io_buf = cre_request->io_buf[i][j];
			if (io_buf->direction != CAM_BUF_INPUT)
				continue;

			CAM_DBG(CAM_CRE, "batch:%d iobuf:%d direction:%d",
				i, j, io_buf->direction);

			rc = cam_cre_bus_rd_update(cam_cre_hw_info,
				ctx_id, cre_reg_buf, i, j, prepare);
			if (rc)
				goto end;
		}

		/* Go command */
		val = 0;
		val |= rd_reg_val->go_cmd;
		val |= rd_reg_val->static_prg & rd_reg_val->static_prg_mask;
		update_cre_reg_set(cre_reg_buf,
			rd_reg->offset + rd_reg->input_if_cmd,
			val);
	}

	for (i = 0; i < cre_reg_buf->num_rd_reg_set; i++) {
		CAM_DBG(CAM_CRE, "CRE value 0x%x offset 0x%x",
				cre_reg_buf->rd_reg_set[i].value,
				cre_reg_buf->rd_reg_set[i].offset);
	}
end:
	return 0;
}

static int cam_cre_bus_rd_acquire(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0, i;
	struct cam_cre_acquire_dev_info *in_acquire;
	struct cre_bus_rd_ctx *bus_rd_ctx;
	struct cre_bus_in_port_to_rm *in_port_to_rm;
	struct cam_cre_bus_rd_reg_val *bus_rd_reg_val;
	int in_port_idx;

	if (ctx_id < 0 || !data || !cam_cre_hw_info || ctx_id >= CRE_CTX_MAX) {
		CAM_ERR(CAM_CRE, "Invalid data: %d %x %x",
			ctx_id, data, cam_cre_hw_info);
		return -EINVAL;
	}

	bus_rd->bus_rd_ctx[ctx_id] = vzalloc(sizeof(struct cre_bus_rd_ctx));
	if (!bus_rd->bus_rd_ctx[ctx_id]) {
		CAM_ERR(CAM_CRE, "Out of memory");
		return -ENOMEM;
	}

	bus_rd->bus_rd_ctx[ctx_id]->cre_acquire = data;
	in_acquire = data;
	bus_rd_ctx = bus_rd->bus_rd_ctx[ctx_id];
	bus_rd_ctx->num_in_ports = in_acquire->num_in_res;
	bus_rd_ctx->security_flag = in_acquire->secure_mode;
	bus_rd_reg_val = cam_cre_hw_info->bus_rd_reg_val;

	for (i = 0; i < in_acquire->num_in_res; i++) {
		if (!in_acquire->in_res[i].width)
			continue;

		CAM_DBG(CAM_CRE, "i = %d format = %u width = 0x%x height = 0x%x res id %d",
			i, in_acquire->in_res[i].format,
			in_acquire->in_res[i].width,
			in_acquire->in_res[i].height,
			in_acquire->in_res[i].res_id);

		in_port_idx =
		cam_cre_bus_rd_in_port_idx(in_acquire->in_res[i].res_id);
		if (in_port_idx < 0) {
			CAM_ERR(CAM_CRE, "Invalid in_port_idx: %d", i + 1);
			rc = -EINVAL;
			goto end;
		}

		in_port_to_rm = &bus_rd->in_port_to_rm[in_port_idx];
		if (!in_port_to_rm->num_rm) {
			CAM_ERR(CAM_CRE, "Invalid format for Input port");
			rc = -EINVAL;
			goto end;
		}

		CAM_DBG(CAM_CRE, "i:%d port_id = %u format %u",
			i, in_acquire->in_res[i].res_id,
			in_acquire->in_res[i].format);
	}

end:
	return rc;
}

static int cam_cre_bus_rd_reg_set_update(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	int i;
	uint32_t num_reg_set;
	struct cre_reg_set *rd_reg_set;
	struct cam_cre_dev_reg_set_update *reg_set_upd_cmd =
		(struct cam_cre_dev_reg_set_update *)data;

	num_reg_set = reg_set_upd_cmd->cre_reg_buf.num_rd_reg_set;
	rd_reg_set = reg_set_upd_cmd->cre_reg_buf.rd_reg_set;

	for (i = 0; i < num_reg_set; i++) {
		CAM_DBG(CAM_CRE, "base 0x%x CRE value 0x%x offset 0x%x",
			cam_cre_hw_info->bus_rd_reg_offset->base,
			rd_reg_set[i].value, rd_reg_set[i].offset);
		cam_io_w_mb(rd_reg_set[i].value,
			cam_cre_hw_info->bus_rd_reg_offset->base + rd_reg_set[i].offset);
	}
	return 0;
}

static int cam_cre_bus_rd_init(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	struct cam_cre_bus_rd_reg_val *bus_rd_reg_val;
	struct cam_cre_bus_rd_reg *bus_rd_reg;
	struct cam_cre_dev_init *dev_init = data;

	if (!cam_cre_hw_info) {
		CAM_ERR(CAM_CRE, "Invalid cam_cre_hw_info");
		return -EINVAL;
	}

	bus_rd_reg_val = cam_cre_hw_info->bus_rd_reg_val;
	bus_rd_reg = cam_cre_hw_info->bus_rd_reg_offset;
	bus_rd_reg->base =
	dev_init->core_info->cre_hw_info->cre_hw->bus_rd_reg_offset->base;

	/* enable interrupt mask */
	cam_io_w_mb(bus_rd_reg_val->irq_mask,
		cam_cre_hw_info->bus_rd_reg_offset->base + bus_rd_reg->irq_mask);

	return 0;
}

static int cam_cre_bus_rd_probe(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	int i, k, rm_idx;
	struct cam_cre_bus_rd_reg_val *bus_rd_reg_val;
	struct cam_cre_bus_rd_reg *bus_rd_reg;
	struct cre_bus_in_port_to_rm *in_port_to_rm;
	uint32_t input_port_idx;

	if (!cam_cre_hw_info) {
		CAM_ERR(CAM_CRE, "Invalid cam_cre_hw_info");
		return -EINVAL;
	}

	bus_rd = kzalloc(sizeof(struct cre_bus_rd), GFP_KERNEL);
	if (!bus_rd) {
		CAM_ERR(CAM_CRE, "Out of memory");
		return -ENOMEM;
	}
	bus_rd->cre_hw_info = cam_cre_hw_info;
	bus_rd_reg_val = cam_cre_hw_info->bus_rd_reg_val;
	bus_rd_reg = cam_cre_hw_info->bus_rd_reg_offset;

	for (i = 0; i < bus_rd_reg_val->num_clients; i++) {
		input_port_idx =
			bus_rd_reg_val->rd_clients[i].rm_port_id;
		in_port_to_rm = &bus_rd->in_port_to_rm[input_port_idx];

		rm_idx = in_port_to_rm->num_rm;
		in_port_to_rm->input_port_id =
			bus_rd_reg_val->rd_clients[i].input_port_id;
		in_port_to_rm->rm_port_id[rm_idx] =
			bus_rd_reg_val->rd_clients[i].rm_port_id;
		in_port_to_rm->num_rm++;
	}

	for (i = 0; i < CRE_MAX_IN_RES; i++) {
		in_port_to_rm = &bus_rd->in_port_to_rm[i];
		CAM_DBG(CAM_CRE, "input port id = %d",
			in_port_to_rm->input_port_id);
			CAM_DBG(CAM_CRE, "num_rms = %d",
				in_port_to_rm->num_rm);
			for (k = 0; k < in_port_to_rm->num_rm; k++) {
				CAM_DBG(CAM_CRE, "rm port id = %d",
					in_port_to_rm->rm_port_id[k]);
			}
	}

	return 0;
}

static int cam_cre_bus_rd_isr(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, void *data)
{
	uint32_t irq_status;
	uint32_t violation_status;
	uint32_t debug_status_0;
	uint32_t debug_status_1;
	struct cam_cre_bus_rd_reg *bus_rd_reg;
	struct cam_cre_bus_rd_reg_val *bus_rd_reg_val;
	struct cam_cre_irq_data *irq_data = data;

	if (!cam_cre_hw_info) {
		CAM_ERR(CAM_CRE, "Invalid cam_cre_hw_info");
		return -EINVAL;
	}

	bus_rd_reg = cam_cre_hw_info->bus_rd_reg_offset;
	bus_rd_reg_val = cam_cre_hw_info->bus_rd_reg_val;

	/* Read and Clear Top Interrupt status */
	irq_status = cam_io_r_mb(bus_rd_reg->base + bus_rd_reg->irq_status);
	cam_io_w_mb(irq_status,
		bus_rd_reg->base + bus_rd_reg->irq_clear);

	cam_io_w_mb(bus_rd_reg_val->irq_cmd_clear,
		bus_rd_reg->base + bus_rd_reg->irq_cmd);

	if (irq_status & bus_rd_reg_val->rup_done)
		CAM_DBG(CAM_CRE, "CRE Read Bus RUP done");

	if (irq_status & bus_rd_reg_val->rd_buf_done)
		CAM_DBG(CAM_CRE, "CRE Read Bus Buff done");

	if (irq_status & bus_rd_reg_val->cons_violation) {
		irq_data->error = 1;
		violation_status = cam_io_r_mb(bus_rd_reg->base +
			bus_rd_reg->rd_clients[0].cons_violation_status);
		debug_status_0 = cam_io_r_mb(bus_rd_reg->base +
			bus_rd_reg->rd_clients[0].debug_status_0);
		debug_status_1 = cam_io_r_mb(bus_rd_reg->base +
			bus_rd_reg->rd_clients[0].debug_status_1);
		CAM_DBG(CAM_CRE, "CRE Read Bus Violation");
		CAM_DBG(CAM_CRE,
			"violation status 0x%x debug status 0/1 0x%x/0x%x",
			violation_status, debug_status_0, debug_status_1);
	}

	return 0;
}

int cam_cre_bus_rd_process(struct cam_cre_hw *cam_cre_hw_info,
	int32_t ctx_id, uint32_t cmd_id, void *data)
{
	int rc = -EINVAL;

	switch (cmd_id) {
	case CRE_HW_PROBE:
		CAM_DBG(CAM_CRE, "CRE_HW_PROBE: E");
		rc = cam_cre_bus_rd_probe(cam_cre_hw_info, ctx_id, data);
		CAM_DBG(CAM_CRE, "CRE_HW_PROBE: X");
		break;
	case CRE_HW_INIT:
		CAM_DBG(CAM_CRE, "CRE_HW_INIT: E");
		rc = cam_cre_bus_rd_init(cam_cre_hw_info, ctx_id, data);
		CAM_DBG(CAM_CRE, "CRE_HW_INIT: X");
		break;
	case CRE_HW_ACQUIRE:
		CAM_DBG(CAM_CRE, "CRE_HW_ACQUIRE: E");
		rc = cam_cre_bus_rd_acquire(cam_cre_hw_info, ctx_id, data);
		CAM_DBG(CAM_CRE, "CRE_HW_ACQUIRE: X");
		break;
	case CRE_HW_RELEASE:
		CAM_DBG(CAM_CRE, "CRE_HW_RELEASE: E");
		rc = cam_cre_bus_rd_release(cam_cre_hw_info, ctx_id, data);
		CAM_DBG(CAM_CRE, "CRE_HW_RELEASE: X");
		break;
	case CRE_HW_PREPARE:
		CAM_DBG(CAM_CRE, "CRE_HW_PREPARE: E");
		rc = cam_cre_bus_rd_prepare(cam_cre_hw_info, ctx_id, data);
		CAM_DBG(CAM_CRE, "CRE_HW_PREPARE: X");
		break;
	case CRE_HW_ISR:
		rc = cam_cre_bus_rd_isr(cam_cre_hw_info, 0, data);
		break;
	case CRE_HW_REG_SET_UPDATE:
		rc = cam_cre_bus_rd_reg_set_update(cam_cre_hw_info, 0, data);
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
	default:
		CAM_ERR(CAM_CRE, "Unsupported cmd: %d", cmd_id);
		break;
	}

	return rc;
}
