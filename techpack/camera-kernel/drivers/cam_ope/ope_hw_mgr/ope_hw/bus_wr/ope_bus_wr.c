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
#include "ope_hw.h"
#include "ope_dev_intf.h"
#include "ope_bus_wr.h"
#include "cam_cdm_util.h"

static struct ope_bus_wr *wr_info;

enum cam_ope_bus_packer_format {
	PACKER_FMT_PLAIN_128                   = 0x0,
	PACKER_FMT_PLAIN_8                     = 0x1,
	PACKER_FMT_PLAIN_8_ODD_EVEN            = 0x2,
	PACKER_FMT_PLAIN_8_LSB_MSB_10          = 0x3,
	PACKER_FMT_PLAIN_8_LSB_MSB_10_ODD_EVEN = 0x4,
	PACKER_FMT_PLAIN_16_10BPP              = 0x5,
	PACKER_FMT_PLAIN_16_12BPP              = 0x6,
	PACKER_FMT_PLAIN_16_14BPP              = 0x7,
	PACKER_FMT_PLAIN_16_16BPP              = 0x8,
	PACKER_FMT_PLAIN_32                    = 0x9,
	PACKER_FMT_PLAIN_64                    = 0xA,
	PACKER_FMT_TP_10                       = 0xB,
	PACKER_FMT_MIPI_10                     = 0xC,
	PACKER_FMT_MIPI_12                     = 0xD,
	PACKER_FMT_MAX                         = 0xE,
};

static int cam_ope_bus_en_port_idx(
	struct cam_ope_request *ope_request,
	uint32_t batch_idx,
	uint32_t output_port_id)
{
	int i;
	struct ope_io_buf *io_buf;

	if (batch_idx >= OPE_MAX_BATCH_SIZE) {
		CAM_ERR(CAM_OPE, "Invalid batch idx: %d", batch_idx);
		return -EINVAL;
	}

	for (i = 0; i < ope_request->num_io_bufs[batch_idx]; i++) {
		io_buf = ope_request->io_buf[batch_idx][i];
		if (io_buf->direction != CAM_BUF_OUTPUT)
			continue;
		if (io_buf->resource_type == output_port_id)
			return i;
	}

	return -EINVAL;
}
static int cam_ope_bus_wr_out_port_idx(uint32_t output_port_id)
{
	int i;

	for (i = 0; i < OPE_OUT_RES_MAX; i++)
		if (wr_info->out_port_to_wm[i].output_port_id == output_port_id)
			return i;

	return -EINVAL;
}


static int cam_ope_bus_wr_subsample(
	struct cam_ope_ctx *ctx_data,
	struct ope_hw *ope_hw_info,
	struct cam_ope_bus_wr_client_reg *wr_reg_client,
	struct ope_io_buf *io_buf,
	uint32_t *temp_reg, uint32_t count,
	int plane_idx, int stripe_idx)
{
	int k, l;
	struct cam_ope_bus_wr_reg *wr_reg;
	struct cam_ope_bus_wr_reg_val *wr_reg_val;

	wr_reg = ope_hw_info->bus_wr_reg;
	wr_reg_val = ope_hw_info->bus_wr_reg_val;

	if (plane_idx >= OPE_MAX_PLANES) {
		CAM_ERR(CAM_OPE, "Invalid plane idx: %d", plane_idx);
		return count;
	}
	k = plane_idx;
	l = stripe_idx;

	/* subsample period and pattern */
	if ((ctx_data->ope_acquire.dev_type ==
		OPE_DEV_TYPE_OPE_RT) && l == 0) {
		temp_reg[count++] = wr_reg->offset +
			wr_reg_client->subsample_period;
		temp_reg[count++] = io_buf->num_stripes[k];

		temp_reg[count++] = wr_reg->offset +
			wr_reg_client->subsample_pattern;
		temp_reg[count++] = 1 <<
			(io_buf->num_stripes[k] - 1);
	} else if ((ctx_data->ope_acquire.dev_type ==
		OPE_DEV_TYPE_OPE_NRT) &&
		((l %
		ctx_data->ope_acquire.nrt_stripes_for_arb) ==
		0)) {
		if (io_buf->num_stripes[k] >=
			(l +
			ctx_data->ope_acquire.nrt_stripes_for_arb)){
			temp_reg[count++] = wr_reg->offset +
				wr_reg_client->subsample_period;
			temp_reg[count++] =
				ctx_data->ope_acquire.nrt_stripes_for_arb;

			temp_reg[count++] = wr_reg->offset +
				wr_reg_client->subsample_pattern;
			temp_reg[count++] = 1 <<
				(ctx_data->ope_acquire.nrt_stripes_for_arb -
				1);
		} else {
			temp_reg[count++] = wr_reg->offset +
				wr_reg_client->subsample_period;
			temp_reg[count++] = io_buf->num_stripes[k] - l;

			/* subsample pattern */
			temp_reg[count++] = wr_reg->offset +
				wr_reg_client->subsample_pattern;
			temp_reg[count++] = 1 << (io_buf->num_stripes[k] -
				l - 1);
		}
	}
	return count;
}

static int cam_ope_bus_wr_release(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;

	if (ctx_id < 0 || ctx_id >= OPE_CTX_MAX) {
		CAM_ERR(CAM_OPE, "Invalid data: %d", ctx_id);
		return -EINVAL;
	}

	vfree(wr_info->bus_wr_ctx[ctx_id]);
	wr_info->bus_wr_ctx[ctx_id] = NULL;

	return rc;
}

static uint32_t *cam_ope_bus_wr_update(struct ope_hw *ope_hw_info,
	int32_t ctx_id, struct cam_ope_dev_prepare_req *prepare,
	int batch_idx, int io_idx,
	uint32_t *kmd_buf, uint32_t *num_stripes)
{
	int k, l, m, out_port_idx;
	uint32_t idx;
	uint32_t num_wm_ports;
	uint32_t comb_idx;
	uint32_t req_idx;
	uint32_t temp_reg[128];
	uint32_t count = 0;
	uint32_t temp = 0;
	uint32_t wm_port_id;
	uint32_t header_size;
	uint32_t *next_buff_addr = NULL;
	struct cam_hw_prepare_update_args *prepare_args;
	struct cam_ope_ctx *ctx_data;
	struct cam_ope_request *ope_request;
	struct ope_io_buf *io_buf;
	struct ope_stripe_io *stripe_io;
	struct ope_bus_wr_ctx *bus_wr_ctx;
	struct cam_ope_bus_wr_reg *wr_reg;
	struct cam_ope_bus_wr_client_reg *wr_reg_client;
	struct cam_ope_bus_wr_reg_val *wr_reg_val;
	struct cam_ope_bus_wr_client_reg_val *wr_res_val_client;
	struct ope_bus_out_port_to_wm *out_port_to_wm;
	struct ope_bus_wr_io_port_cdm_batch *io_port_cdm_batch;
	struct ope_bus_wr_io_port_cdm_info *io_port_cdm;
	struct cam_cdm_utils_ops *cdm_ops;


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
	bus_wr_ctx = wr_info->bus_wr_ctx[ctx_id];
	io_port_cdm_batch = &bus_wr_ctx->io_port_cdm_batch;
	wr_reg = ope_hw_info->bus_wr_reg;
	wr_reg_val = ope_hw_info->bus_wr_reg_val;

	CAM_DBG(CAM_OPE, "kmd_buf = %x req_idx = %d req_id = %lld offset = %d",
		kmd_buf, req_idx, ope_request->request_id,
		prepare->kmd_buf_offset);

	io_buf = ope_request->io_buf[batch_idx][io_idx];
	CAM_DBG(CAM_OPE, "batch = %d io buf num = %d dir = %d rsc %d",
		batch_idx, io_idx, io_buf->direction, io_buf->resource_type);

	io_port_cdm =
		&bus_wr_ctx->io_port_cdm_batch.io_port_cdm[batch_idx];
	out_port_idx =
		cam_ope_bus_wr_out_port_idx(io_buf->resource_type);
	if (out_port_idx < 0) {
		CAM_ERR(CAM_OPE, "Invalid idx for rsc type: %d",
			io_buf->resource_type);
		return NULL;
	}
	out_port_to_wm = &wr_info->out_port_to_wm[out_port_idx];
	comb_idx = BUS_WR_YUV;
	num_wm_ports = out_port_to_wm->num_wm[comb_idx];

	for (k = 0; k < io_buf->num_planes; k++) {
		*num_stripes = io_buf->num_stripes[k];
		for (l = 0; l < io_buf->num_stripes[k]; l++) {
			stripe_io = &io_buf->s_io[k][l];
			CAM_DBG(CAM_OPE, "comb_idx = %d p_idx = %d s_idx = %d",
				comb_idx, k, l);
			/* frame level info */
			/* stripe level info */
			wm_port_id = out_port_to_wm->wm_port_id[comb_idx][k];
			wr_reg_client = &wr_reg->wr_clients[wm_port_id];
			wr_res_val_client = &wr_reg_val->wr_clients[wm_port_id];

			/* Core cfg: enable, Mode */
			temp_reg[count++] = wr_reg->offset +
				wr_reg_client->core_cfg;
			temp = 0;
			if (!stripe_io->disable_bus)
				temp = wr_res_val_client->core_cfg_en;
			temp |= ((wr_res_val_client->mode &
				wr_res_val_client->mode_mask) <<
				wr_res_val_client->mode_shift);
			temp_reg[count++] = temp;

			/* Address of the Image */
			temp_reg[count++] = wr_reg->offset +
				wr_reg_client->img_addr;
			temp_reg[count++] = stripe_io->iova_addr;

			/* Buffer size */
			temp_reg[count++] = wr_reg->offset +
				wr_reg_client->img_cfg;
			temp = 0;
			temp = stripe_io->width;
			temp |= (stripe_io->height &
				wr_res_val_client->height_mask) <<
				wr_res_val_client->height_shift;
			temp_reg[count++] = temp;

			/* x_init */
			temp_reg[count++] = wr_reg->offset +
				wr_reg_client->x_init;
			temp_reg[count++] = stripe_io->x_init;

			/* stride */
			temp_reg[count++] = wr_reg->offset +
				wr_reg_client->stride;
			temp_reg[count++] = stripe_io->stride;

			/* pack cfg : Format and alignment */
			temp_reg[count++] = wr_reg->offset +
				wr_reg_client->pack_cfg;
			temp = 0;

			/*
			 * In case of NV12, change the packer format of chroma
			 * plane to odd even byte swapped format
			 */

			if (k == 1 && stripe_io->format == CAM_FORMAT_NV12)
				stripe_io->pack_format =
					PACKER_FMT_PLAIN_8_ODD_EVEN;

			temp |= ((stripe_io->pack_format &
				wr_res_val_client->format_mask) <<
				wr_res_val_client->format_shift);
			temp |= ((stripe_io->alignment &
				wr_res_val_client->alignment_mask) <<
				wr_res_val_client->alignment_shift);
			temp_reg[count++] = temp;

			/* subsample period and pattern */
			count = cam_ope_bus_wr_subsample(
					ctx_data, ope_hw_info,
					wr_reg_client, io_buf,
					temp_reg, count, k, l);

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
				prepare->kmd_buf_offset +=
					((count + header_size) * sizeof(temp));
			kmd_buf = next_buff_addr;

			CAM_DBG(CAM_OPE, "b:%d io:%d p:%d s:%d",
				batch_idx, io_idx, k, l);
			for (m = 0; m < count; m += 2)
				CAM_DBG(CAM_OPE, "%d: off: 0x%x val: 0x%x",
					m, temp_reg[m], temp_reg[m+1]);
			CAM_DBG(CAM_OPE, "kmdbuf:%x, offset:%d",
				kmd_buf, prepare->kmd_buf_offset);
			CAM_DBG(CAM_OPE, "WR cmd bufs = %d off:%d len:%d",
				io_port_cdm->num_s_cmd_bufs[l],
				io_port_cdm->s_cdm_info[l][idx].offset,
				io_port_cdm->s_cdm_info[l][idx].len);
			count = 0;
		}
	}

	return kmd_buf;
}

static uint32_t *cam_ope_bus_wm_disable(struct ope_hw *ope_hw_info,
	int32_t ctx_id, struct cam_ope_dev_prepare_req *prepare,
	int batch_idx, int io_idx,
	uint32_t *kmd_buf, uint32_t num_stripes)
{
	int k, l;
	uint32_t idx;
	uint32_t num_wm_ports;
	uint32_t comb_idx;
	uint32_t req_idx;
	uint32_t temp_reg[128];
	uint32_t count = 0;
	uint32_t temp = 0;
	uint32_t wm_port_id;
	uint32_t header_size;
	uint32_t *next_buff_addr = NULL;
	struct cam_ope_ctx *ctx_data;
	struct ope_bus_wr_ctx *bus_wr_ctx;
	struct cam_ope_bus_wr_reg *wr_reg;
	struct cam_ope_bus_wr_client_reg *wr_reg_client;
	struct ope_bus_out_port_to_wm *out_port_to_wm;
	struct ope_bus_wr_io_port_cdm_batch *io_port_cdm_batch;
	struct ope_bus_wr_io_port_cdm_info *io_port_cdm;
	struct cam_cdm_utils_ops *cdm_ops;


	if (ctx_id < 0 || !prepare) {
		CAM_ERR(CAM_OPE, "Invalid data: %d %x", ctx_id, prepare);
		return NULL;
	}

	if (batch_idx >= OPE_MAX_BATCH_SIZE) {
		CAM_ERR(CAM_OPE, "Invalid batch idx: %d", batch_idx);
		return NULL;
	}

	ctx_data = prepare->ctx_data;
	req_idx = prepare->req_idx;
	cdm_ops = ctx_data->ope_cdm.cdm_ops;

	bus_wr_ctx = wr_info->bus_wr_ctx[ctx_id];
	io_port_cdm_batch = &bus_wr_ctx->io_port_cdm_batch;
	wr_reg = ope_hw_info->bus_wr_reg;

	CAM_DBG(CAM_OPE,
		"kmd_buf = %x req_idx = %d offset = %d out_idx %d b %d",
		kmd_buf, req_idx, prepare->kmd_buf_offset, io_idx, batch_idx);

	io_port_cdm =
		&bus_wr_ctx->io_port_cdm_batch.io_port_cdm[batch_idx];
	out_port_to_wm = &wr_info->out_port_to_wm[io_idx];
	comb_idx = BUS_WR_YUV;
	num_wm_ports = out_port_to_wm->num_wm[comb_idx];

	for (k = 0; k < num_wm_ports; k++) {
		for (l = 0; l < num_stripes; l++) {
			/* frame level info */
			/* stripe level info */
			wm_port_id = out_port_to_wm->wm_port_id[comb_idx][k];
			wr_reg_client = &wr_reg->wr_clients[wm_port_id];

			/* Core cfg: enable, Mode */
			temp_reg[count++] = wr_reg->offset +
				wr_reg_client->core_cfg;
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
				prepare->kmd_buf_offset +=
					((count + header_size) * sizeof(temp));
			kmd_buf = next_buff_addr;

			CAM_DBG(CAM_OPE, "WR cmd bufs = %d",
				io_port_cdm->num_s_cmd_bufs[l]);
			CAM_DBG(CAM_OPE, "s:%d off:%d len:%d",
				l, io_port_cdm->s_cdm_info[l][idx].offset,
				io_port_cdm->s_cdm_info[l][idx].len);
			count = 0;
		}
	}

	prepare->wr_cdm_batch = &bus_wr_ctx->io_port_cdm_batch;

	return kmd_buf;
}

static int cam_ope_bus_wr_prepare(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	int i, j = 0;
	uint32_t req_idx;
	uint32_t *kmd_buf;
	struct cam_ope_dev_prepare_req *prepare;
	struct cam_ope_ctx *ctx_data;
	struct cam_ope_request *ope_request;
	struct ope_io_buf *io_buf;
	uint32_t temp;
	int io_buf_idx;
	uint32_t num_stripes = 0;
	struct ope_bus_wr_io_port_cdm_batch *io_port_cdm_batch;
	struct ope_bus_wr_ctx *bus_wr_ctx;

	if (ctx_id < 0 || !data) {
		CAM_ERR(CAM_OPE, "Invalid data: %d %x", ctx_id, data);
		return -EINVAL;
	}
	prepare = data;
	ctx_data = prepare->ctx_data;
	req_idx = prepare->req_idx;
	bus_wr_ctx = wr_info->bus_wr_ctx[ctx_id];

	ope_request = ctx_data->req_list[req_idx];
	kmd_buf = (uint32_t *)ope_request->ope_kmd_buf.cpu_addr +
		(prepare->kmd_buf_offset / sizeof(temp));


	CAM_DBG(CAM_OPE, "kmd_buf = %x req_idx = %d req_id = %lld offset = %d",
		kmd_buf, req_idx, ope_request->request_id,
		prepare->kmd_buf_offset);

	io_port_cdm_batch = &wr_info->bus_wr_ctx[ctx_id]->io_port_cdm_batch;
	memset(io_port_cdm_batch, 0,
		sizeof(struct ope_bus_wr_io_port_cdm_batch));

	for (i = 0; i < ope_request->num_batch; i++) {
		for (j = 0; j < ope_request->num_io_bufs[i]; j++) {
			io_buf = ope_request->io_buf[i][j];
			CAM_DBG(CAM_OPE, "batch = %d io buf num = %d dir = %d",
				i, j, io_buf->direction);
			if (io_buf->direction != CAM_BUF_OUTPUT)
				continue;

			kmd_buf = cam_ope_bus_wr_update(ope_hw_info,
				ctx_id, prepare, i, j,
				kmd_buf, &num_stripes);
			if (!kmd_buf) {
				rc = -EINVAL;
				goto end;
			}
		}
	}

	/* Disable WMs which are not enabled */
	for (i = 0; i < ope_request->num_batch; i++) {
		for (j = OPE_OUT_RES_VIDEO; j <= OPE_OUT_RES_MAX; j++) {
			io_buf_idx = cam_ope_bus_en_port_idx(ope_request, i, j);
			if (io_buf_idx >= 0)
				continue;

			io_buf_idx = cam_ope_bus_wr_out_port_idx(j);
			if (io_buf_idx < 0) {
				CAM_ERR(CAM_OPE, "Invalid idx for rsc type:%d",
					j);
				return io_buf_idx;
			}
			kmd_buf = cam_ope_bus_wm_disable(ope_hw_info,
				ctx_id, prepare, i, io_buf_idx,
				kmd_buf, num_stripes);
		}
	}
	prepare->wr_cdm_batch = &bus_wr_ctx->io_port_cdm_batch;

end:
	return rc;
}

static int cam_ope_bus_wr_acquire(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0, i;
	struct ope_acquire_dev_info *in_acquire;
	struct ope_bus_wr_ctx *bus_wr_ctx;
	struct ope_bus_out_port_to_wm *out_port_to_wr;
	int combo_idx;
	int out_port_idx;

	if (ctx_id < 0 || !data || ctx_id >= OPE_CTX_MAX) {
		CAM_ERR(CAM_OPE, "Invalid data: %d %x", ctx_id, data);
		return -EINVAL;
	}

	wr_info->bus_wr_ctx[ctx_id] = vzalloc(sizeof(struct ope_bus_wr_ctx));
	if (!wr_info->bus_wr_ctx[ctx_id]) {
		CAM_ERR(CAM_OPE, "Out of memory");
		return -ENOMEM;
	}

	wr_info->bus_wr_ctx[ctx_id]->ope_acquire = data;
	in_acquire = data;
	bus_wr_ctx = wr_info->bus_wr_ctx[ctx_id];
	bus_wr_ctx->num_out_ports = in_acquire->num_out_res;
	bus_wr_ctx->security_flag = in_acquire->secure_mode;

	for (i = 0; i < in_acquire->num_out_res; i++) {
		if (!in_acquire->out_res[i].width)
			continue;

		CAM_DBG(CAM_OPE, "i = %d format = %u width = %x height = %x",
			i, in_acquire->out_res[i].format,
			in_acquire->out_res[i].width,
			in_acquire->out_res[i].height);
		CAM_DBG(CAM_OPE, "pix_pattern:%u alignment:%u packer_format:%u",
			in_acquire->out_res[i].pixel_pattern,
			in_acquire->out_res[i].alignment,
			in_acquire->out_res[i].packer_format);
		CAM_DBG(CAM_OPE, "subsample_period = %u subsample_pattern = %u",
			in_acquire->out_res[i].subsample_period,
			in_acquire->out_res[i].subsample_pattern);

		out_port_idx =
		cam_ope_bus_wr_out_port_idx(in_acquire->out_res[i].res_id);
		if (out_port_idx < 0) {
			CAM_DBG(CAM_OPE, "Invalid in_port_idx: %d",
				in_acquire->out_res[i].res_id);
			rc = -EINVAL;
			goto end;
		}
		out_port_to_wr = &wr_info->out_port_to_wm[out_port_idx];
		combo_idx = BUS_WR_YUV;
		if (!out_port_to_wr->num_wm[combo_idx]) {
			CAM_DBG(CAM_OPE, "Invalid format for Input port");
			rc = -EINVAL;
			goto end;
		}

		bus_wr_ctx->io_port_info.output_port_id[i] =
			in_acquire->out_res[i].res_id;
		bus_wr_ctx->io_port_info.output_format_type[i] =
			in_acquire->out_res[i].format;
		if (in_acquire->out_res[i].pixel_pattern >
			PIXEL_PATTERN_CRYCBY) {
			CAM_DBG(CAM_OPE, "Invalid pix pattern = %u",
				in_acquire->out_res[i].pixel_pattern);
			rc = -EINVAL;
			goto end;
		}

		bus_wr_ctx->io_port_info.pixel_pattern[i] =
			in_acquire->out_res[i].pixel_pattern;
		bus_wr_ctx->io_port_info.latency_buf_size = 4096;
		CAM_DBG(CAM_OPE, "i:%d port_id = %u format %u pix_pattern = %u",
			i, bus_wr_ctx->io_port_info.output_port_id[i],
			bus_wr_ctx->io_port_info.output_format_type[i],
			bus_wr_ctx->io_port_info.pixel_pattern[i]);
		CAM_DBG(CAM_OPE, "latency_buf_size = %u",
			bus_wr_ctx->io_port_info.latency_buf_size);
	}

end:
	return rc;
}

static int cam_ope_bus_wr_init(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	struct cam_ope_bus_wr_reg_val *bus_wr_reg_val;
	struct cam_ope_bus_wr_reg *bus_wr_reg;
	struct cam_ope_dev_init *dev_init = data;

	if (!ope_hw_info) {
		CAM_ERR(CAM_OPE, "Invalid ope_hw_info");
		return -EINVAL;
	}

	wr_info->ope_hw_info = ope_hw_info;
	bus_wr_reg_val = ope_hw_info->bus_wr_reg_val;
	bus_wr_reg = ope_hw_info->bus_wr_reg;
	bus_wr_reg->base = dev_init->core_info->ope_hw_info->ope_bus_wr_base;

	cam_io_w_mb(bus_wr_reg_val->irq_mask_0,
		ope_hw_info->bus_wr_reg->base + bus_wr_reg->irq_mask_0);
	cam_io_w_mb(bus_wr_reg_val->irq_mask_1,
		ope_hw_info->bus_wr_reg->base + bus_wr_reg->irq_mask_1);

	return rc;
}

static int cam_ope_bus_wr_probe(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0, i, j, combo_idx, k;
	struct cam_ope_bus_wr_reg_val *bus_wr_reg_val;
	struct ope_bus_out_port_to_wm *out_port_to_wm;
	uint32_t output_port_idx;
	uint32_t wm_idx;

	if (!ope_hw_info) {
		CAM_ERR(CAM_OPE, "Invalid ope_hw_info");
		return -EINVAL;
	}
	wr_info = kzalloc(sizeof(struct ope_bus_wr), GFP_KERNEL);
	if (!wr_info) {
		CAM_ERR(CAM_OPE, "Out of memory");
		return -ENOMEM;
	}

	wr_info->ope_hw_info = ope_hw_info;
	bus_wr_reg_val = ope_hw_info->bus_wr_reg_val;

	for (i = 0; i < bus_wr_reg_val->num_clients; i++) {
		output_port_idx =
			bus_wr_reg_val->wr_clients[i].output_port_id - 1;
		out_port_to_wm = &wr_info->out_port_to_wm[output_port_idx];
		combo_idx = BUS_WR_YUV;
		wm_idx = out_port_to_wm->num_wm[combo_idx];
		out_port_to_wm->output_port_id =
			bus_wr_reg_val->wr_clients[i].output_port_id;
		out_port_to_wm->wm_port_id[combo_idx][wm_idx] =
			bus_wr_reg_val->wr_clients[i].wm_port_id;
		if (!out_port_to_wm->num_wm[combo_idx])
			out_port_to_wm->num_combos++;
		out_port_to_wm->num_wm[combo_idx]++;
	}

	for (i = 0; i < OPE_OUT_RES_MAX; i++) {
		out_port_to_wm = &wr_info->out_port_to_wm[i];
		CAM_DBG(CAM_OPE, "output port id = %d num_combos = %d",
			out_port_to_wm->output_port_id,
			out_port_to_wm->num_combos);
		for (j = 0; j < out_port_to_wm->num_combos; j++) {
			CAM_DBG(CAM_OPE, "combo idx = %d num_wms = %d",
				j, out_port_to_wm->num_wm[j]);
			for (k = 0; k < out_port_to_wm->num_wm[j]; k++) {
				CAM_DBG(CAM_OPE, "wm port id = %d",
					out_port_to_wm->wm_port_id[j][k]);
			}
		}
	}

	return rc;
}

static int cam_ope_bus_wr_isr(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	uint32_t irq_status_0, irq_status_1, violation_status;
	struct cam_ope_bus_wr_reg *bus_wr_reg;
	struct cam_ope_bus_wr_reg_val *bus_wr_reg_val;
	struct cam_ope_irq_data *irq_data = data;

	if (!ope_hw_info || !irq_data) {
		CAM_ERR(CAM_OPE, "Invalid ope_hw_info");
		return -EINVAL;
	}

	bus_wr_reg = ope_hw_info->bus_wr_reg;
	bus_wr_reg_val = ope_hw_info->bus_wr_reg_val;

	/* Read and Clear Top Interrupt status */
	irq_status_0 = cam_io_r_mb(bus_wr_reg->base + bus_wr_reg->irq_status_0);
	irq_status_1 = cam_io_r_mb(bus_wr_reg->base + bus_wr_reg->irq_status_1);
	cam_io_w_mb(irq_status_0,
		bus_wr_reg->base + bus_wr_reg->irq_clear_0);
	cam_io_w_mb(irq_status_1,
		bus_wr_reg->base + bus_wr_reg->irq_clear_1);

	cam_io_w_mb(bus_wr_reg_val->irq_set_clear,
		bus_wr_reg->base + bus_wr_reg->irq_cmd);

	if (irq_status_0 & bus_wr_reg_val->cons_violation) {
		irq_data->error = 1;
		CAM_ERR(CAM_OPE, "ope bus wr cons_violation");
	}

	if (irq_status_0 & bus_wr_reg_val->violation) {
		irq_data->error = 1;
		violation_status = cam_io_r_mb(bus_wr_reg->base +
			bus_wr_reg->violation_status);
		CAM_ERR(CAM_OPE,
			"ope bus wr violation, violation_status 0x%x",
			violation_status);
	}

	if (irq_status_0 & bus_wr_reg_val->img_size_violation) {
		irq_data->error = 1;
		violation_status = cam_io_r_mb(bus_wr_reg->base +
			bus_wr_reg->image_size_violation_status);
		CAM_ERR(CAM_OPE,
			"ope bus wr img_size_violation, violation_status 0x%x",
			violation_status);
	}

	return rc;
}

int cam_ope_bus_wr_process(struct ope_hw *ope_hw_info,
	int32_t ctx_id, uint32_t cmd_id, void *data)
{
	int rc = 0;

	switch (cmd_id) {
	case OPE_HW_PROBE:
		CAM_DBG(CAM_OPE, "OPE_HW_PROBE: E");
		rc = cam_ope_bus_wr_probe(ope_hw_info, ctx_id, data);
		CAM_DBG(CAM_OPE, "OPE_HW_PROBE: X");
		break;
	case OPE_HW_INIT:
		CAM_DBG(CAM_OPE, "OPE_HW_INIT: E");
		rc = cam_ope_bus_wr_init(ope_hw_info, ctx_id, data);
		CAM_DBG(CAM_OPE, "OPE_HW_INIT: X");
		break;
	case OPE_HW_ACQUIRE:
		CAM_DBG(CAM_OPE, "OPE_HW_ACQUIRE: E");
		rc = cam_ope_bus_wr_acquire(ope_hw_info, ctx_id, data);
		CAM_DBG(CAM_OPE, "OPE_HW_ACQUIRE: X");
		break;
	case OPE_HW_RELEASE:
		CAM_DBG(CAM_OPE, "OPE_HW_RELEASE: E");
		rc = cam_ope_bus_wr_release(ope_hw_info, ctx_id, data);
		CAM_DBG(CAM_OPE, "OPE_HW_RELEASE: X");
		break;
	case OPE_HW_PREPARE:
		CAM_DBG(CAM_OPE, "OPE_HW_PREPARE: E");
		rc = cam_ope_bus_wr_prepare(ope_hw_info, ctx_id, data);
		CAM_DBG(CAM_OPE, "OPE_HW_PREPARE: X");
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
	case OPE_HW_ISR:
		rc = cam_ope_bus_wr_isr(ope_hw_info, 0, data);
		break;
	default:
		CAM_ERR(CAM_OPE, "Unsupported cmd: %d", cmd_id);
		break;
	}

	return rc;
}

