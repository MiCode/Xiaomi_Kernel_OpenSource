// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/iopoll.h>
#include <media/cam_ope.h>
#include "cam_io_util.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "ope_core.h"
#include "ope_soc.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"
#include "cam_cdm_util.h"
#include "ope_hw.h"
#include "ope_dev_intf.h"
#include "ope_bus_rd.h"
#include "cam_common_util.h"

static struct ope_bus_rd *bus_rd;

enum cam_ope_bus_unpacker_format {
	UNPACKER_FMT_PLAIN_128                   = 0x0,
	UNPACKER_FMT_PLAIN_8                     = 0x1,
	UNPACKER_FMT_PLAIN_16_10BPP              = 0x2,
	UNPACKER_FMT_PLAIN_16_12BPP              = 0x3,
	UNPACKER_FMT_PLAIN_16_14BPP              = 0x4,
	UNPACKER_FMT_PLAIN_32_20BPP              = 0x5,
	UNPACKER_FMT_ARGB_16_10BPP               = 0x6,
	UNPACKER_FMT_ARGB_16_12BPP               = 0x7,
	UNPACKER_FMT_ARGB_16_14BPP               = 0x8,
	UNPACKER_FMT_PLAIN_32                    = 0x9,
	UNPACKER_FMT_PLAIN_64                    = 0xA,
	UNPACKER_FMT_TP_10                       = 0xB,
	UNPACKER_FMT_MIPI_8                      = 0xC,
	UNPACKER_FMT_MIPI_10                     = 0xD,
	UNPACKER_FMT_MIPI_12                     = 0xE,
	UNPACKER_FMT_MIPI_14                     = 0xF,
	UNPACKER_FMT_PLAIN_16_16BPP              = 0x10,
	UNPACKER_FMT_PLAIN_128_ODD_EVEN          = 0x11,
	UNPACKER_FMT_PLAIN_8_ODD_EVEN            = 0x12,
	UNPACKER_FMT_MAX                         = 0x13,
};

static int cam_ope_bus_rd_in_port_idx(uint32_t input_port_id)
{
	int i;

	for (i = 0; i < OPE_IN_RES_MAX; i++)
		if (bus_rd->in_port_to_rm[i].input_port_id ==
			input_port_id)
			return i;

	return -EINVAL;
}

static int cam_ope_bus_rd_combo_idx(uint32_t format)
{
	int rc = -EINVAL;

	switch (format) {
	case CAM_FORMAT_YUV422:
	case CAM_FORMAT_NV21:
	case CAM_FORMAT_NV12:
		rc = BUS_RD_YUV;
		break;
	case CAM_FORMAT_MIPI_RAW_6:
	case CAM_FORMAT_MIPI_RAW_8:
	case CAM_FORMAT_MIPI_RAW_10:
	case CAM_FORMAT_MIPI_RAW_12:
	case CAM_FORMAT_MIPI_RAW_14:
	case CAM_FORMAT_MIPI_RAW_16:
	case CAM_FORMAT_MIPI_RAW_20:
	case CAM_FORMAT_QTI_RAW_8:
	case CAM_FORMAT_QTI_RAW_10:
	case CAM_FORMAT_QTI_RAW_12:
	case CAM_FORMAT_QTI_RAW_14:
	case CAM_FORMAT_PLAIN8:
	case CAM_FORMAT_PLAIN16_8:
	case CAM_FORMAT_PLAIN16_10:
	case CAM_FORMAT_PLAIN16_12:
	case CAM_FORMAT_PLAIN16_14:
	case CAM_FORMAT_PLAIN16_16:
	case CAM_FORMAT_PLAIN32_20:
	case CAM_FORMAT_PLAIN64:
	case CAM_FORMAT_PLAIN128:
		rc = BUS_RD_BAYER;
		break;
	default:
		break;
		}

	CAM_DBG(CAM_OPE, "Input format = %u rc = %d",
		format, rc);
	return rc;
}

static int cam_ope_bus_is_rm_enabled(
	struct cam_ope_request *ope_request,
	uint32_t batch_idx,
	uint32_t rm_id)
{
	int i, k;
	int32_t combo_idx;
	struct ope_io_buf *io_buf;
	struct ope_bus_in_port_to_rm *in_port_to_rm;

	if (batch_idx >= OPE_MAX_BATCH_SIZE) {
		CAM_ERR(CAM_OPE, "Invalid batch idx: %d", batch_idx);
		return -EINVAL;
	}

	for (i = 0; i < ope_request->num_io_bufs[batch_idx]; i++) {
		io_buf = ope_request->io_buf[batch_idx][i];
		if (io_buf->direction != CAM_BUF_INPUT)
			continue;
		in_port_to_rm =
			&bus_rd->in_port_to_rm[io_buf->resource_type - 1];
		combo_idx = cam_ope_bus_rd_combo_idx(io_buf->format);
		if (combo_idx < 0) {
			CAM_ERR(CAM_OPE, "Invalid combo_idx");
			return -EINVAL;
		}
		for (k = 0; k < io_buf->num_planes; k++) {
			if (rm_id ==
				in_port_to_rm->rm_port_id[combo_idx][k])
				return true;
		}
	}

	return false;
}

static uint32_t *cam_ope_bus_rd_update(struct ope_hw *ope_hw_info,
	int32_t ctx_id, uint32_t *kmd_buf, int batch_idx,
	int io_idx, struct cam_ope_dev_prepare_req *prepare,
	int32_t *num_stripes)
{
	int k, l, m;
	uint32_t idx;
	int32_t combo_idx;
	uint32_t req_idx, count = 0, temp;
	uint32_t temp_reg[128] = {0};
	uint32_t rm_id, header_size;
	uint32_t rsc_type;
	uint32_t *next_buff_addr = NULL;
	struct cam_hw_prepare_update_args *prepare_args;
	struct cam_ope_ctx *ctx_data;
	struct cam_ope_request *ope_request;
	struct ope_io_buf *io_buf;
	struct ope_stripe_io *stripe_io;
	struct ope_bus_rd_ctx *bus_rd_ctx;
	struct cam_ope_bus_rd_reg *rd_reg;
	struct cam_ope_bus_rd_client_reg *rd_reg_client;
	struct cam_ope_bus_rd_reg_val *rd_reg_val;
	struct cam_ope_bus_rd_client_reg_val *rd_res_val_client;
	struct ope_bus_in_port_to_rm *in_port_to_rm;
	struct ope_bus_rd_io_port_cdm_info *io_port_cdm;
	struct cam_cdm_utils_ops *cdm_ops;
	struct ope_bus_rd_io_port_info *io_port_info;


	if (ctx_id < 0 || !prepare) {
		CAM_ERR(CAM_OPE, "Invalid data: %d %x", ctx_id, prepare);
		return NULL;
	}

	if (batch_idx >= OPE_MAX_BATCH_SIZE) {
		CAM_ERR(CAM_OPE, "Invalid batch idx: %d", batch_idx);
		return NULL;
	}

	if (io_idx >= OPE_MAX_IO_BUFS) {
		CAM_ERR(CAM_OPE, "Invalid IO idx: %d", io_idx);
		return NULL;
	}

	prepare_args = prepare->prepare_args;
	ctx_data = prepare->ctx_data;
	req_idx = prepare->req_idx;
	cdm_ops = ctx_data->ope_cdm.cdm_ops;

	ope_request = ctx_data->req_list[req_idx];
	CAM_DBG(CAM_OPE, "req_idx = %d req_id = %lld KMDbuf %x offset %d",
		req_idx, ope_request->request_id,
		kmd_buf, prepare->kmd_buf_offset);
	bus_rd_ctx = bus_rd->bus_rd_ctx[ctx_id];
	io_port_info = &bus_rd_ctx->io_port_info;
	rd_reg = ope_hw_info->bus_rd_reg;
	rd_reg_val = ope_hw_info->bus_rd_reg_val;
	io_buf = ope_request->io_buf[batch_idx][io_idx];

	CAM_DBG(CAM_OPE,
		"req_idx = %d req_id = %lld KMDbuf 0x%x offset %d rsc %d",
		req_idx, ope_request->request_id,
		kmd_buf, prepare->kmd_buf_offset,
		io_buf->resource_type);
	CAM_DBG(CAM_OPE, "batch:%d iobuf:%d direction:%d",
		batch_idx, io_idx, io_buf->direction);
	io_port_cdm =
	&bus_rd_ctx->io_port_cdm_batch.io_port_cdm[batch_idx];
	in_port_to_rm =
	&bus_rd->in_port_to_rm[io_buf->resource_type - 1];
	combo_idx = cam_ope_bus_rd_combo_idx(io_buf->format);
	if (combo_idx < 0) {
		CAM_ERR(CAM_OPE, "Invalid combo_idx");
		return NULL;
	}

	for (k = 0; k < io_buf->num_planes; k++) {
		for (l = 0; l < io_buf->num_stripes[k]; l++) {
			*num_stripes = io_buf->num_stripes[k];
			stripe_io = &io_buf->s_io[k][l];
			rsc_type = io_buf->resource_type - 1;
			/* frame level info */
			/* stripe level info */
			rm_id = in_port_to_rm->rm_port_id[combo_idx][k];
			rd_reg_client = &rd_reg->rd_clients[rm_id];
			rd_res_val_client = &rd_reg_val->rd_clients[rm_id];

			/* security cfg */
			temp_reg[count++] = rd_reg->offset +
				rd_reg->security_cfg;
			temp_reg[count++] =
				ctx_data->ope_acquire.secure_mode;

			/* enable client */
			temp_reg[count++] = rd_reg->offset +
				rd_reg_client->core_cfg;
			temp_reg[count++] = 1;

			/* ccif meta data */
			temp_reg[count++] = rd_reg->offset +
				rd_reg_client->ccif_meta_data;
			temp = 0;
			temp |= stripe_io->s_location &
				rd_res_val_client->stripe_location_mask;
			temp |= (io_buf->pix_pattern &
				rd_res_val_client->pix_pattern_mask) <<
				rd_res_val_client->pix_pattern_shift;
			temp_reg[count++] = temp;

			/* Address of the Image */
			temp_reg[count++] = rd_reg->offset +
				rd_reg_client->img_addr;
			temp_reg[count++] = stripe_io->iova_addr;

			/* Buffer size */
			temp_reg[count++] = rd_reg->offset +
				rd_reg_client->img_cfg;
			temp = 0;
			temp = stripe_io->height;
			temp |=
			(stripe_io->width &
				rd_res_val_client->img_width_mask) <<
				rd_res_val_client->img_width_shift;
			temp_reg[count++] = temp;

			/* stride */
			temp_reg[count++] = rd_reg->offset +
				rd_reg_client->stride;
			temp_reg[count++] = stripe_io->stride;

			/*
			 * In case of NV12, change the unpacker format of
			 * chroma plane to odd even byte swapped format.
			 */
			if (k == 1 && stripe_io->format == CAM_FORMAT_NV12)
				stripe_io->unpack_format =
					UNPACKER_FMT_PLAIN_8_ODD_EVEN;

			/* Unpack cfg : Mode and alignment */
			temp_reg[count++] = rd_reg->offset +
				rd_reg_client->unpack_cfg;
			temp = 0;
			temp |= (stripe_io->unpack_format &
				rd_res_val_client->mode_mask) <<
				rd_res_val_client->mode_shift;
			temp |= (stripe_io->alignment &
				rd_res_val_client->alignment_mask) <<
				rd_res_val_client->alignment_shift;
			temp_reg[count++] = temp;

			/* latency buffer allocation */
			temp_reg[count++] = rd_reg->offset +
				rd_reg_client->latency_buf_allocation;
			temp_reg[count++] = io_port_info->latency_buf_size;

			header_size = cdm_ops->cdm_get_cmd_header_size(
				CAM_CDM_CMD_REG_RANDOM);
			idx = io_port_cdm->num_s_cmd_bufs[l];
			io_port_cdm->s_cdm_info[l][idx].len = sizeof(temp) *
				(count + header_size);
			io_port_cdm->s_cdm_info[l][idx].offset =
				prepare->kmd_buf_offset;
			io_port_cdm->s_cdm_info[l][idx].addr = kmd_buf;
			io_port_cdm->num_s_cmd_bufs[l]++;

			next_buff_addr = cdm_ops->cdm_write_regrandom(
				kmd_buf, count/2, temp_reg);
			if (next_buff_addr > kmd_buf)
				prepare->kmd_buf_offset +=
					((count + header_size) * sizeof(temp));
			kmd_buf = next_buff_addr;
			CAM_DBG(CAM_OPE, "b:%d io:%d p:%d s:%d",
				batch_idx, io_idx, k, l);
			for (m = 0; m < count; m += 2)
				CAM_DBG(CAM_OPE, "%d: off: 0x%x val: 0x%x",
					m, temp_reg[m], temp_reg[m+1]);
			CAM_DBG(CAM_OPE, "kmd_buf:%x offset:%d",
				kmd_buf, prepare->kmd_buf_offset);
			CAM_DBG(CAM_OPE, "RD cmdbufs:%d off:%d len %d",
				io_port_cdm->num_s_cmd_bufs[l],
				io_port_cdm->s_cdm_info[l][idx].offset,
				io_port_cdm->s_cdm_info[l][idx].len);
			count = 0;
		}
	}

	return kmd_buf;
}

static uint32_t *cam_ope_bus_rm_disable(struct ope_hw *ope_hw_info,
	int32_t ctx_id, struct cam_ope_dev_prepare_req *prepare,
	int batch_idx, int rm_idx,
	uint32_t *kmd_buf, uint32_t num_stripes)
{
	int l;
	uint32_t idx;
	uint32_t req_idx;
	uint32_t temp_reg[128];
	uint32_t count = 0;
	uint32_t temp = 0;
	uint32_t header_size;
	uint32_t *next_buff_addr = NULL;
	struct cam_ope_ctx *ctx_data;
	struct ope_bus_rd_ctx *bus_rd_ctx;
	struct cam_ope_bus_rd_reg *rd_reg;
	struct cam_ope_bus_rd_client_reg *rd_reg_client;
	struct ope_bus_rd_io_port_cdm_batch *io_port_cdm_batch;
	struct ope_bus_rd_io_port_cdm_info *io_port_cdm;
	struct cam_cdm_utils_ops *cdm_ops;


	if (ctx_id < 0 || !prepare) {
		CAM_ERR(CAM_OPE, "Invalid data: %d %x", ctx_id, prepare);
		return NULL;
	}

	if (batch_idx >= OPE_MAX_BATCH_SIZE) {
		CAM_ERR(CAM_OPE, "Invalid batch idx: %d", batch_idx);
		return NULL;
	}

	if (rm_idx >= MAX_RD_CLIENTS) {
		CAM_ERR(CAM_OPE, "Invalid read client: %d", rm_idx);
		return NULL;
	}

	ctx_data = prepare->ctx_data;
	req_idx = prepare->req_idx;
	cdm_ops = ctx_data->ope_cdm.cdm_ops;

	bus_rd_ctx = bus_rd->bus_rd_ctx[ctx_id];
	io_port_cdm_batch = &bus_rd_ctx->io_port_cdm_batch;
	rd_reg = ope_hw_info->bus_rd_reg;

	CAM_DBG(CAM_OPE,
		"kmd_buf = 0x%x req_idx = %d offset = %d rd_idx %d b %d",
		kmd_buf, req_idx, prepare->kmd_buf_offset, rm_idx, batch_idx);

	io_port_cdm =
		&bus_rd_ctx->io_port_cdm_batch.io_port_cdm[batch_idx];

	for (l = 0; l < num_stripes; l++) {
		/* stripe level info */
		rd_reg_client = &rd_reg->rd_clients[rm_idx];

		/* Core cfg: enable, Mode */
		temp_reg[count++] = rd_reg->offset +
			rd_reg_client->core_cfg;
		temp_reg[count++] = 0;

		header_size = cdm_ops->cdm_get_cmd_header_size(
			CAM_CDM_CMD_REG_RANDOM);
		idx = io_port_cdm->num_s_cmd_bufs[l];
		io_port_cdm->s_cdm_info[l][idx].len =
			sizeof(temp) * (count + header_size);
		io_port_cdm->s_cdm_info[l][idx].offset =
			prepare->kmd_buf_offset;
		io_port_cdm->s_cdm_info[l][idx].addr = kmd_buf;
		io_port_cdm->num_s_cmd_bufs[l]++;

		next_buff_addr = cdm_ops->cdm_write_regrandom(
			kmd_buf, count/2, temp_reg);
		if (next_buff_addr > kmd_buf)
			prepare->kmd_buf_offset += ((count + header_size) *
				sizeof(temp));
		kmd_buf = next_buff_addr;
		CAM_DBG(CAM_OPE, "RD cmd bufs = %d",
			io_port_cdm->num_s_cmd_bufs[l]);
		CAM_DBG(CAM_OPE, "stripe %d off:%d len:%d",
			l, io_port_cdm->s_cdm_info[l][idx].offset,
			io_port_cdm->s_cdm_info[l][idx].len);
		count = 0;
	}

	prepare->rd_cdm_batch = &bus_rd_ctx->io_port_cdm_batch;

	return kmd_buf;
}

static int cam_ope_bus_rd_prepare(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	int i, j;
	int32_t combo_idx;
	uint32_t req_idx, count = 0, temp;
	uint32_t temp_reg[32] = {0};
	uint32_t header_size;
	uint32_t *kmd_buf;
	uint32_t *next_buff_addr = NULL;
	int is_rm_enabled;
	struct cam_ope_dev_prepare_req *prepare;
	struct cam_ope_ctx *ctx_data;
	struct cam_ope_request *ope_request;
	struct ope_io_buf *io_buf;
	struct ope_bus_rd_ctx *bus_rd_ctx;
	struct cam_ope_bus_rd_reg *rd_reg;
	struct cam_ope_bus_rd_reg_val *rd_reg_val;
	struct ope_bus_rd_io_port_cdm_batch *io_port_cdm_batch;
	struct ope_bus_rd_io_port_cdm_info *io_port_cdm = NULL;
	struct cam_cdm_utils_ops *cdm_ops;
	int32_t num_stripes = 0;

	if (ctx_id < 0 || !data) {
		CAM_ERR(CAM_OPE, "Invalid data: %d %x", ctx_id, data);
		return -EINVAL;
	}
	prepare = data;

	ctx_data = prepare->ctx_data;
	req_idx = prepare->req_idx;
	cdm_ops = ctx_data->ope_cdm.cdm_ops;

	ope_request = ctx_data->req_list[req_idx];
	kmd_buf = (uint32_t *)ope_request->ope_kmd_buf.cpu_addr +
		prepare->kmd_buf_offset;
	CAM_DBG(CAM_OPE, "req_idx = %d req_id = %lld",
		req_idx, ope_request->request_id);
	CAM_DBG(CAM_OPE, "KMD buf and offset = %x %d",
		kmd_buf, prepare->kmd_buf_offset);
	bus_rd_ctx = bus_rd->bus_rd_ctx[ctx_id];
	io_port_cdm_batch =
		&bus_rd_ctx->io_port_cdm_batch;
	memset(io_port_cdm_batch, 0,
		sizeof(struct ope_bus_rd_io_port_cdm_batch));
	rd_reg = ope_hw_info->bus_rd_reg;
	rd_reg_val = ope_hw_info->bus_rd_reg_val;

	for (i = 0; i < ope_request->num_batch; i++) {
		for (j = 0; j < ope_request->num_io_bufs[i]; j++) {
			io_buf = ope_request->io_buf[i][j];
			if (io_buf->direction != CAM_BUF_INPUT)
				continue;

			CAM_DBG(CAM_OPE, "batch:%d iobuf:%d direction:%d",
				i, j, io_buf->direction);
			io_port_cdm =
			&bus_rd_ctx->io_port_cdm_batch.io_port_cdm[i];

			combo_idx = cam_ope_bus_rd_combo_idx(io_buf->format);
			if (combo_idx < 0) {
				CAM_ERR(CAM_OPE, "Invalid combo_idx");
				return combo_idx;
			}

			kmd_buf = cam_ope_bus_rd_update(ope_hw_info,
				ctx_id, kmd_buf, i, j, prepare,
				&num_stripes);
			if (!kmd_buf) {
				rc = -EINVAL;
				goto end;
			}
		}
	}

	if (!io_port_cdm) {
		rc = -EINVAL;
		goto end;
	}

	/* Disable RMs which are not enabled */
	for (i = 0; i < ope_request->num_batch; i++) {
		for (j = 0; j < rd_reg_val->num_clients; j++) {
			is_rm_enabled = cam_ope_bus_is_rm_enabled(
				ope_request, i, j);
			if (is_rm_enabled < 0) {
				rc = -EINVAL;
				goto end;
			}
			if (is_rm_enabled)
				continue;

			kmd_buf = cam_ope_bus_rm_disable(ope_hw_info,
				ctx_id, prepare, i, j,
				kmd_buf, num_stripes);
			if (!kmd_buf) {
				rc = -EINVAL;
				goto end;
			}
		}
	}

	/* Go command */
	count = 0;
	temp_reg[count++] = rd_reg->offset +
		rd_reg->input_if_cmd;
	temp = 0;
	temp |= rd_reg_val->go_cmd;
	temp_reg[count++] = temp;

	header_size =
	cdm_ops->cdm_get_cmd_header_size(CAM_CDM_CMD_REG_RANDOM);
	for (i = 0; i < ope_request->num_batch; i++) {
		io_port_cdm =
			&bus_rd_ctx->io_port_cdm_batch.io_port_cdm[i];
		io_port_cdm->go_cmd_addr = kmd_buf;
		io_port_cdm->go_cmd_len =
			sizeof(temp) * (count + header_size);
		io_port_cdm->go_cmd_offset =
			prepare->kmd_buf_offset;
	}
	next_buff_addr = cdm_ops->cdm_write_regrandom(
		kmd_buf, count/2, temp_reg);
	if (next_buff_addr > kmd_buf)
		prepare->kmd_buf_offset +=
			((count + header_size) * sizeof(temp));
	kmd_buf = next_buff_addr;
	CAM_DBG(CAM_OPE, "kmd_buf:%x,offset:%d",
		kmd_buf, prepare->kmd_buf_offset);
	CAM_DBG(CAM_OPE, "t_reg:%xcount: %d size:%d",
		 temp_reg, count, header_size);
	prepare->rd_cdm_batch = &bus_rd_ctx->io_port_cdm_batch;

end:
	return rc;
}

static int cam_ope_bus_rd_release(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;

	if (ctx_id < 0 || ctx_id >= OPE_CTX_MAX) {
		CAM_ERR(CAM_OPE, "Invalid data: %d", ctx_id);
		return -EINVAL;
	}

	vfree(bus_rd->bus_rd_ctx[ctx_id]);
	bus_rd->bus_rd_ctx[ctx_id] = NULL;

	return rc;
}

static int cam_ope_bus_rd_acquire(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0, i;
	struct ope_acquire_dev_info *in_acquire;
	struct ope_bus_rd_ctx *bus_rd_ctx;
	struct ope_bus_in_port_to_rm *in_port_to_rm;
	struct cam_ope_bus_rd_reg_val *bus_rd_reg_val;
	int combo_idx;
	int in_port_idx;


	if (ctx_id < 0 || !data || !ope_hw_info || ctx_id >= OPE_CTX_MAX) {
		CAM_ERR(CAM_OPE, "Invalid data: %d %x %x",
			ctx_id, data, ope_hw_info);
		return -EINVAL;
	}

	bus_rd->bus_rd_ctx[ctx_id] = vzalloc(sizeof(struct ope_bus_rd_ctx));
	if (!bus_rd->bus_rd_ctx[ctx_id]) {
		CAM_ERR(CAM_OPE, "Out of memory");
		return -ENOMEM;
	}

	bus_rd->bus_rd_ctx[ctx_id]->ope_acquire = data;
	in_acquire = data;
	bus_rd_ctx = bus_rd->bus_rd_ctx[ctx_id];
	bus_rd_ctx->num_in_ports = in_acquire->num_in_res;
	bus_rd_ctx->security_flag = in_acquire->secure_mode;
	bus_rd_reg_val = ope_hw_info->bus_rd_reg_val;

	for (i = 0; i < in_acquire->num_in_res; i++) {
		if (!in_acquire->in_res[i].width)
			continue;

		CAM_DBG(CAM_OPE, "i = %d format = %u width = %x height = %x",
			i, in_acquire->in_res[i].format,
			in_acquire->in_res[i].width,
			in_acquire->in_res[i].height);
		CAM_DBG(CAM_OPE, "pix_pattern:%u alignment:%u unpack_format:%u",
			in_acquire->in_res[i].pixel_pattern,
			in_acquire->in_res[i].alignment,
			in_acquire->in_res[i].unpacker_format);
		CAM_DBG(CAM_OPE, "max_stripe = %u fps = %u",
			in_acquire->in_res[i].max_stripe_size,
			in_acquire->in_res[i].fps);

		in_port_idx = cam_ope_bus_rd_in_port_idx(i + 1);
		if (in_port_idx < 0) {
			CAM_ERR(CAM_OPE, "Invalid in_port_idx: %d", i + 1);
			rc = -EINVAL;
			goto end;
		}

		in_port_to_rm = &bus_rd->in_port_to_rm[in_port_idx];
		combo_idx = cam_ope_bus_rd_combo_idx(
			in_acquire->in_res[i].format);
		if (combo_idx < 0) {
			CAM_ERR(CAM_OPE, "Invalid format: %d",
				in_acquire->in_res[i].format);
			rc = -EINVAL;
			goto end;
		}

		if (!in_port_to_rm->num_rm[combo_idx]) {
			CAM_ERR(CAM_OPE, "Invalid format for Input port");
			rc = -EINVAL;
			goto end;
		}

		bus_rd_ctx->io_port_info.input_port_id[i] =
			in_acquire->in_res[i].res_id;
		bus_rd_ctx->io_port_info.input_format_type[i] =
			in_acquire->in_res[i].format;
		if (in_acquire->in_res[i].pixel_pattern >
			PIXEL_PATTERN_CRYCBY) {
			CAM_ERR(CAM_OPE, "Invalid pix pattern = %u",
				in_acquire->in_res[i].pixel_pattern);
			rc = -EINVAL;
			goto end;
		}

		bus_rd_ctx->io_port_info.pixel_pattern[i] =
			in_acquire->in_res[i].pixel_pattern;
		bus_rd_ctx->io_port_info.latency_buf_size =
			bus_rd_reg_val->latency_buf_size;

		CAM_DBG(CAM_OPE, "i:%d port_id = %u format %u pix_pattern = %u",
			i, bus_rd_ctx->io_port_info.input_port_id[i],
			bus_rd_ctx->io_port_info.input_format_type[i],
			bus_rd_ctx->io_port_info.pixel_pattern[i]);
		CAM_DBG(CAM_OPE, "latency_buf_size = %u",
			bus_rd_ctx->io_port_info.latency_buf_size);
	}

end:
	return rc;
}

static int cam_ope_bus_rd_init(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	struct cam_ope_bus_rd_reg_val *bus_rd_reg_val;
	struct cam_ope_bus_rd_reg *bus_rd_reg;
	struct cam_ope_dev_init *dev_init = data;

	if (!ope_hw_info) {
		CAM_ERR(CAM_OPE, "Invalid ope_hw_info");
		return -EINVAL;
	}

	bus_rd_reg_val = ope_hw_info->bus_rd_reg_val;
	bus_rd_reg = ope_hw_info->bus_rd_reg;
	bus_rd_reg->base = dev_init->core_info->ope_hw_info->ope_bus_rd_base;

	/* OPE SW RESET */
	init_completion(&bus_rd->reset_complete);

	/* enable interrupt mask */
	cam_io_w_mb(bus_rd_reg_val->irq_mask,
		ope_hw_info->bus_rd_reg->base + bus_rd_reg->irq_mask);

	cam_io_w_mb(bus_rd_reg_val->sw_reset,
		ope_hw_info->bus_rd_reg->base + bus_rd_reg->sw_reset);

	rc = cam_common_wait_for_completion_timeout(
		&bus_rd->reset_complete, msecs_to_jiffies(30000));

	if (!rc || rc < 0) {
		CAM_ERR(CAM_OPE, "reset error result = %d", rc);
		if (!rc)
			rc = -ETIMEDOUT;
	} else {
		rc = 0;
	}

	cam_io_w_mb(bus_rd_reg_val->irq_mask,
		ope_hw_info->bus_rd_reg->base + bus_rd_reg->irq_mask);

	return rc;
}

static int cam_ope_bus_rd_probe(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0, i, j, combo_idx, k;
	struct cam_ope_bus_rd_reg_val *bus_rd_reg_val;
	struct cam_ope_bus_rd_reg *bus_rd_reg;
	struct ope_bus_in_port_to_rm *in_port_to_rm;
	uint32_t input_port_idx;
	uint32_t rm_idx;

	if (!ope_hw_info) {
		CAM_ERR(CAM_OPE, "Invalid ope_hw_info");
		return -EINVAL;
	}
	bus_rd = kzalloc(sizeof(struct ope_bus_rd), GFP_KERNEL);
	if (!bus_rd) {
		CAM_ERR(CAM_OPE, "Out of memory");
		return -ENOMEM;
	}
	bus_rd->ope_hw_info = ope_hw_info;
	bus_rd_reg_val = ope_hw_info->bus_rd_reg_val;
	bus_rd_reg = ope_hw_info->bus_rd_reg;

	for (i = 0; i < bus_rd_reg_val->num_clients; i++) {
		input_port_idx =
			bus_rd_reg_val->rd_clients[i].input_port_id - 1;
		in_port_to_rm = &bus_rd->in_port_to_rm[input_port_idx];
		if (bus_rd_reg_val->rd_clients[i].format_type &
			BUS_RD_COMBO_BAYER_MASK) {
			combo_idx = BUS_RD_BAYER;
			rm_idx = in_port_to_rm->num_rm[combo_idx];
			in_port_to_rm->input_port_id =
				bus_rd_reg_val->rd_clients[i].input_port_id;
			in_port_to_rm->rm_port_id[combo_idx][rm_idx] =
				bus_rd_reg_val->rd_clients[i].rm_port_id;
			if (!in_port_to_rm->num_rm[combo_idx])
				in_port_to_rm->num_combos++;
			in_port_to_rm->num_rm[combo_idx]++;
		}
		if (bus_rd_reg_val->rd_clients[i].format_type &
			BUS_RD_COMBO_YUV_MASK) {
			combo_idx = BUS_RD_YUV;
			rm_idx = in_port_to_rm->num_rm[combo_idx];
			in_port_to_rm->input_port_id =
				bus_rd_reg_val->rd_clients[i].input_port_id;
			in_port_to_rm->rm_port_id[combo_idx][rm_idx] =
				bus_rd_reg_val->rd_clients[i].rm_port_id;
			if (!in_port_to_rm->num_rm[combo_idx])
				in_port_to_rm->num_combos++;
			in_port_to_rm->num_rm[combo_idx]++;
		}
	}

	for (i = 0; i < OPE_IN_RES_MAX; i++) {
		in_port_to_rm = &bus_rd->in_port_to_rm[i];
		CAM_DBG(CAM_OPE, "input port id = %d num_combos = %d",
			in_port_to_rm->input_port_id,
			in_port_to_rm->num_combos);
		for (j = 0; j < in_port_to_rm->num_combos; j++) {
			CAM_DBG(CAM_OPE, "combo idx = %d num_rms = %d",
				j, in_port_to_rm->num_rm[j]);
			for (k = 0; k < in_port_to_rm->num_rm[j]; k++) {
				CAM_DBG(CAM_OPE, "rm port id = %d",
					in_port_to_rm->rm_port_id[j][k]);
			}
		}
	}

	return rc;
}

static int cam_ope_bus_rd_isr(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	uint32_t irq_status;
	struct cam_ope_bus_rd_reg *bus_rd_reg;
	struct cam_ope_bus_rd_reg_val *bus_rd_reg_val;
	struct cam_ope_irq_data *irq_data = data;

	if (!ope_hw_info) {
		CAM_ERR(CAM_OPE, "Invalid ope_hw_info");
		return -EINVAL;
	}

	bus_rd_reg = ope_hw_info->bus_rd_reg;
	bus_rd_reg_val = ope_hw_info->bus_rd_reg_val;

	/* Read and Clear Top Interrupt status */
	irq_status = cam_io_r_mb(bus_rd_reg->base + bus_rd_reg->irq_status);
	cam_io_w_mb(irq_status,
		bus_rd_reg->base + bus_rd_reg->irq_clear);

	cam_io_w_mb(bus_rd_reg_val->irq_set_clear,
		bus_rd_reg->base + bus_rd_reg->irq_cmd);

	if (irq_status & bus_rd_reg_val->rst_done) {
		complete(&bus_rd->reset_complete);
		CAM_DBG(CAM_OPE, "ope bus rd reset done");
	}

	if ((irq_status & bus_rd_reg_val->violation) ==
		bus_rd_reg_val->violation) {
		irq_data->error = 1;
		CAM_ERR(CAM_OPE, "ope bus rd CCIF vioalation");
	}

	return rc;
}

int cam_ope_bus_rd_process(struct ope_hw *ope_hw_info,
	int32_t ctx_id, uint32_t cmd_id, void *data)
{
	int rc = -EINVAL;

	switch (cmd_id) {
	case OPE_HW_PROBE:
		CAM_DBG(CAM_OPE, "OPE_HW_PROBE: E");
		rc = cam_ope_bus_rd_probe(ope_hw_info, ctx_id, data);
		CAM_DBG(CAM_OPE, "OPE_HW_PROBE: X");
		break;
	case OPE_HW_INIT:
		CAM_DBG(CAM_OPE, "OPE_HW_INIT: E");
		rc = cam_ope_bus_rd_init(ope_hw_info, ctx_id, data);
		CAM_DBG(CAM_OPE, "OPE_HW_INIT: X");
		break;
	case OPE_HW_ACQUIRE:
		CAM_DBG(CAM_OPE, "OPE_HW_ACQUIRE: E");
		rc = cam_ope_bus_rd_acquire(ope_hw_info, ctx_id, data);
		CAM_DBG(CAM_OPE, "OPE_HW_ACQUIRE: X");
		break;
	case OPE_HW_RELEASE:
		CAM_DBG(CAM_OPE, "OPE_HW_RELEASE: E");
		rc = cam_ope_bus_rd_release(ope_hw_info, ctx_id, data);
		CAM_DBG(CAM_OPE, "OPE_HW_RELEASE: X");
		break;
	case OPE_HW_PREPARE:
		CAM_DBG(CAM_OPE, "OPE_HW_PREPARE: E");
		rc = cam_ope_bus_rd_prepare(ope_hw_info, ctx_id, data);
		CAM_DBG(CAM_OPE, "OPE_HW_PREPARE: X");
		break;
	case OPE_HW_ISR:
		rc = cam_ope_bus_rd_isr(ope_hw_info, 0, data);
		break;
	case OPE_HW_DEINIT:
	case OPE_HW_START:
	case OPE_HW_STOP:
	case OPE_HW_FLUSH:
	case OPE_HW_CLK_UPDATE:
	case OPE_HW_BW_UPDATE:
	case OPE_HW_RESET:
	case OPE_HW_SET_IRQ_CB:
		rc = 0;
		CAM_DBG(CAM_OPE, "Unhandled cmds: %d", cmd_id);
		break;
	default:
		CAM_ERR(CAM_OPE, "Unsupported cmd: %d", cmd_id);
		break;
	}

	return rc;
}
