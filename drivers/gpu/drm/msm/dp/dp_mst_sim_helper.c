/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

/*
 * Copyright © 2014 Red Hat
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <linux/types.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <drm/drm_fixed.h>
#include <drm/drm_edid.h>
#include <drm_dp_mst_helper.h>
#include <soc/qcom/msm_dp_mst_sim_helper.h>

#define DDC_SEGMENT_ADDR 0x30

struct msm_dp_mst_sim_context {
	void *host_dev;
	void (*host_hpd_irq)(void *host_dev);
	void (*host_req)(void *host_dev, const u8 *in, int in_size,
			u8 *out, int *out_size);

	struct msm_dp_mst_sim_port *ports;
	u32 port_num;

	struct drm_dp_sideband_msg_rx down_req;
	struct drm_dp_sideband_msg_rx down_rep;

	struct mutex session_lock;
	struct completion session_comp;
	struct workqueue_struct *wq;

	u8 esi[16];
	u8 guid[16];
	u8 dpcd[1024];
};

struct msm_dp_mst_sim_work {
	struct work_struct base;
	struct msm_dp_mst_sim_context *ctx;
	unsigned int address;
	u8 buffer[256];
	size_t size;
};

#ifdef CONFIG_DYNAMIC_DEBUG
static void msm_dp_sideband_hex_dump(const char *name,
		u32 address, u8 *buffer, size_t size)
{
	char prefix[64];
	int i, linelen, remaining = size;
	const int rowsize = 16;
	u8 linebuf[64];

	snprintf(prefix, sizeof(prefix), "%s(%d) %4xh(%2zu): ",
		name, current->pid, address, size);

	for (i = 0; i < size; i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;

		hex_dump_to_buffer(buffer + i, linelen, rowsize, 1,
			linebuf, sizeof(linebuf), false);

		pr_debug("%s%s\n", prefix, linebuf);
	}
}
#else
static void msm_dp_sideband_hex_dump(const char *name,
		u32 address, u8 *buffer, size_t size)
{
}
#endif

static u8 drm_dp_msg_header_crc4(const uint8_t *data, size_t num_nibbles)
{
	u8 bitmask = 0x80;
	u8 bitshift = 7;
	u8 array_index = 0;
	int number_of_bits = num_nibbles * 4;
	u8 remainder = 0;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		remainder |= (data[array_index] & bitmask) >> bitshift;
		bitmask >>= 1;
		bitshift--;
		if (bitmask == 0) {
			bitmask = 0x80;
			bitshift = 7;
			array_index++;
		}
		if ((remainder & 0x10) == 0x10)
			remainder ^= 0x13;
	}

	number_of_bits = 4;
	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		if ((remainder & 0x10) != 0)
			remainder ^= 0x13;
	}

	return remainder;
}

static u8 drm_dp_msg_data_crc4(const uint8_t *data, u8 number_of_bytes)
{
	u8 bitmask = 0x80;
	u8 bitshift = 7;
	u8 array_index = 0;
	int number_of_bits = number_of_bytes * 8;
	u16 remainder = 0;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		remainder |= (data[array_index] & bitmask) >> bitshift;
		bitmask >>= 1;
		bitshift--;
		if (bitmask == 0) {
			bitmask = 0x80;
			bitshift = 7;
			array_index++;
		}
		if ((remainder & 0x100) == 0x100)
			remainder ^= 0xd5;
	}

	number_of_bits = 8;
	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		if ((remainder & 0x100) != 0)
			remainder ^= 0xd5;
	}

	return remainder & 0xff;
}

static bool drm_dp_decode_sideband_msg_hdr(struct drm_dp_sideband_msg_hdr *hdr,
					   u8 *buf, int buflen, u8 *hdrlen)
{
	u8 crc4;
	u8 len;
	int i;
	u8 idx;

	if (buf[0] == 0)
		return false;
	len = 3;
	len += ((buf[0] & 0xf0) >> 4) / 2;
	if (len > buflen)
		return false;
	crc4 = drm_dp_msg_header_crc4(buf, (len * 2) - 1);

	if ((crc4 & 0xf) != (buf[len - 1] & 0xf)) {
		DRM_DEBUG_KMS("crc4 mismatch 0x%x 0x%x\n", crc4, buf[len - 1]);
		return false;
	}

	hdr->lct = (buf[0] & 0xf0) >> 4;
	hdr->lcr = (buf[0] & 0xf);
	idx = 1;
	for (i = 0; i < (hdr->lct / 2); i++)
		hdr->rad[i] = buf[idx++];
	hdr->broadcast = (buf[idx] >> 7) & 0x1;
	hdr->path_msg = (buf[idx] >> 6) & 0x1;
	hdr->msg_len = buf[idx] & 0x3f;
	idx++;
	hdr->somt = (buf[idx] >> 7) & 0x1;
	hdr->eomt = (buf[idx] >> 6) & 0x1;
	hdr->seqno = (buf[idx] >> 4) & 0x1;
	idx++;
	*hdrlen = idx;
	return true;
}

static bool drm_dp_sideband_msg_build(struct drm_dp_sideband_msg_rx *msg,
				      u8 *replybuf, u8 replybuflen, bool hdr)
{
	int ret;
	u8 crc4;

	if (hdr) {
		u8 hdrlen;
		struct drm_dp_sideband_msg_hdr recv_hdr;

		ret = drm_dp_decode_sideband_msg_hdr(&recv_hdr,
			replybuf, replybuflen, &hdrlen);
		if (ret == false)
			return false;

		/*
		 * ignore out-of-order messages or messages that are part of a
		 * failed transaction
		 */
		if (!recv_hdr.somt && !msg->have_somt)
			return false;

		/* get length contained in this portion */
		msg->curchunk_len = recv_hdr.msg_len;
		msg->curchunk_hdrlen = hdrlen;

		/* we have already gotten an somt - don't bother parsing */
		if (recv_hdr.somt && msg->have_somt)
			return false;

		if (recv_hdr.somt) {
			memcpy(&msg->initial_hdr, &recv_hdr,
				sizeof(struct drm_dp_sideband_msg_hdr));
			msg->have_somt = true;
		}
		if (recv_hdr.eomt)
			msg->have_eomt = true;

		/* copy the bytes for the remainder of this header chunk */
		msg->curchunk_idx = min(msg->curchunk_len,
			(u8)(replybuflen - hdrlen));
		memcpy(&msg->chunk[0], replybuf + hdrlen, msg->curchunk_idx);
	} else {
		memcpy(&msg->chunk[msg->curchunk_idx], replybuf, replybuflen);
		msg->curchunk_idx += replybuflen;
	}

	if (msg->curchunk_idx >= msg->curchunk_len) {
		/* do CRC */
		crc4 = drm_dp_msg_data_crc4(msg->chunk, msg->curchunk_len - 1);
		/* copy chunk into bigger msg */
		memcpy(&msg->msg[msg->curlen], msg->chunk,
			msg->curchunk_len - 1);
		msg->curlen += msg->curchunk_len - 1;
	}
	return true;
}

static void drm_dp_encode_sideband_msg_hdr(struct drm_dp_sideband_msg_hdr *hdr,
					   u8 *buf, int *len)
{
	int idx = 0;
	int i;
	u8 crc4;

	buf[idx++] = ((hdr->lct & 0xf) << 4) | (hdr->lcr & 0xf);
	for (i = 0; i < (hdr->lct / 2); i++)
		buf[idx++] = hdr->rad[i];
	buf[idx++] = (hdr->broadcast << 7) | (hdr->path_msg << 6) |
		(hdr->msg_len & 0x3f);
	buf[idx++] = (hdr->somt << 7) | (hdr->eomt << 6) | (hdr->seqno << 4);

	crc4 = drm_dp_msg_header_crc4(buf, (idx * 2) - 1);
	buf[idx - 1] |= (crc4 & 0xf);

	*len = idx;
}

static bool msm_dp_get_one_sb_msg(struct drm_dp_sideband_msg_rx *msg,
		struct drm_dp_aux_msg *aux_msg)
{
	int ret;

	if (!msg->have_somt) {
		ret = drm_dp_sideband_msg_build(msg,
			aux_msg->buffer, aux_msg->size, true);
		if (!ret) {
			pr_err("sideband hdr build failed\n");
			return false;
		}
	} else {
		ret = drm_dp_sideband_msg_build(msg,
			aux_msg->buffer, aux_msg->size, false);
		if (!ret) {
			pr_err("sideband msg build failed\n");
			return false;
		}
	}

	return true;
}

static int msm_dp_sideband_build_nak_rep(
		struct msm_dp_mst_sim_context *ctx)
{
	struct drm_dp_sideband_msg_rx *msg = &ctx->down_req;
	u8 *buf = ctx->down_rep.msg;
	int idx = 0;

	buf[idx] = msg->msg[0] | 0x80;
	idx++;

	memcpy(&buf[idx], ctx->guid, 16);
	idx += 16;

	buf[idx] = 0x4;
	idx++;

	buf[idx] = 0;
	idx++;

	return idx;
}


static int msm_dp_sideband_build_link_address_rep(
		struct msm_dp_mst_sim_context *ctx)
{
	struct msm_dp_mst_sim_port *port;
	u8 *buf = ctx->down_rep.msg;
	int idx = 0;
	u32 i, tmp;

	buf[idx] = DP_LINK_ADDRESS;
	idx++;

	memcpy(&buf[idx], ctx->guid, 16);
	idx += 16;

	buf[idx] = ctx->port_num;
	idx++;

	for (i = 0; i < ctx->port_num; i++) {
		port = &ctx->ports[i];

		tmp = 0;
		if (port->input)
			tmp |= 0x80;
		tmp |= port->pdt << 4;
		tmp |= i & 0xF;
		buf[idx] = tmp;
		idx++;

		tmp = 0;
		if (port->mcs)
			tmp |= 0x80;
		if (port->ddps)
			tmp |= 0x40;

		if (port->input) {
			buf[idx] = tmp;
			idx++;
			continue;
		}

		if (port->ldps)
			tmp |= 0x20;
		buf[idx] = tmp;
		idx++;

		buf[idx] = port->dpcd_rev;
		idx++;

		memcpy(&buf[idx], port->peer_guid, 16);
		idx += 16;

		buf[idx] = (port->num_sdp_streams << 4) |
			(port->num_sdp_stream_sinks);
		idx++;
	}

	return idx;
}

static int msm_dp_sideband_build_remote_i2c_read_rep(
		struct msm_dp_mst_sim_context *ctx)
{
	struct msm_dp_mst_sim_port *port;
	struct drm_dp_remote_i2c_read i2c_read;
	u8 *buf;
	int idx;
	u32 i, start, len;

	buf = ctx->down_req.msg;
	idx = 1;

	i2c_read.num_transactions = buf[idx] & 0x3;
	i2c_read.port_number = buf[idx] >> 4;
	idx++;

	if (i2c_read.port_number >= ctx->port_num)
		goto err;

	for (i = 0; i < i2c_read.num_transactions; i++) {
		i2c_read.transactions[i].i2c_dev_id = buf[idx] & 0x7f;
		idx++;

		i2c_read.transactions[i].num_bytes = buf[idx];
		idx++;

		i2c_read.transactions[i].bytes = &buf[idx];
		idx += i2c_read.transactions[i].num_bytes;

		i2c_read.transactions[i].no_stop_bit = (buf[idx] >> 4) & 0x1;
		i2c_read.transactions[i].i2c_transaction_delay = buf[idx] & 0xf;
		idx++;
	}

	i2c_read.read_i2c_device_id = buf[idx];
	idx++;

	i2c_read.num_bytes_read = buf[idx];
	idx++;

	port = &ctx->ports[i2c_read.port_number];

	if (i2c_read.num_transactions == 1) {
		if (i2c_read.transactions[0].i2c_dev_id != DDC_ADDR ||
		    i2c_read.transactions[0].num_bytes != 1) {
			pr_err("unsupported i2c address\n");
			goto err;
		}

		start = i2c_read.transactions[0].bytes[0];
	} else if (i2c_read.num_transactions == 2) {
		if (i2c_read.transactions[0].i2c_dev_id != DDC_SEGMENT_ADDR ||
		    i2c_read.transactions[0].num_bytes != 1 ||
		    i2c_read.transactions[1].i2c_dev_id != DDC_ADDR ||
		    i2c_read.transactions[1].num_bytes != 1) {
			pr_err("unsupported i2c address\n");
			goto err;
		}

		start = i2c_read.transactions[0].bytes[0] * EDID_LENGTH * 2 +
			i2c_read.transactions[1].bytes[0];
	} else {
		pr_err("unsupported i2c transaction\n");
		goto err;
	}

	len = i2c_read.num_bytes_read;

	if (start + len > port->edid_size) {
		pr_err("edid data exceeds maximum\n");
		goto err;
	}

	buf = ctx->down_rep.msg;
	idx = 0;

	buf[idx] = DP_REMOTE_I2C_READ;
	idx++;

	buf[idx] = i2c_read.port_number;
	idx++;

	buf[idx] = len;
	idx++;

	memcpy(&buf[idx], &port->edid[start], len);
	idx += len;

	return idx;
err:
	return msm_dp_sideband_build_nak_rep(ctx);
}

static int msm_dp_sideband_build_enum_path_resources_rep(
		struct msm_dp_mst_sim_context *ctx)
{
	struct msm_dp_mst_sim_port *port;
	u8 port_num;
	u8 *buf;
	int idx;

	buf = ctx->down_req.msg;
	port_num = buf[1] >> 4;

	if (port_num >= ctx->port_num) {
		pr_err("invalid port num\n");
		goto err;
	}

	port = &ctx->ports[port_num];

	buf = ctx->down_rep.msg;
	idx = 0;

	buf[idx] = DP_ENUM_PATH_RESOURCES;
	idx++;

	buf[idx] = port_num << 4;
	idx++;

	buf[idx] = port->full_pbn >> 8;
	idx++;

	buf[idx] = port->full_pbn & 0xFF;
	idx++;

	buf[idx] = port->avail_pbn >> 8;
	idx++;

	buf[idx] = port->avail_pbn & 0xFF;
	idx++;

	return idx;
err:
	return msm_dp_sideband_build_nak_rep(ctx);
}

static int msm_dp_sideband_build_allocate_payload_rep(
		struct msm_dp_mst_sim_context *ctx)
{
	struct drm_dp_allocate_payload allocate_payload;
	u8 *buf;
	int idx;
	u32 i;

	buf = ctx->down_req.msg;
	idx = 1;

	allocate_payload.port_number = buf[idx] >> 4;
	allocate_payload.number_sdp_streams = buf[idx] & 0xF;
	idx++;

	allocate_payload.vcpi = buf[idx];
	idx++;

	allocate_payload.pbn = (buf[idx] << 8) | buf[idx+1];
	idx += 2;

	for (i = 0; i <  allocate_payload.number_sdp_streams / 2; i++) {
		allocate_payload.sdp_stream_sink[i * 2] = buf[idx] >> 4;
		allocate_payload.sdp_stream_sink[i * 2 + 1] = buf[idx] & 0xf;
		idx++;
	}
	if (allocate_payload.number_sdp_streams & 1) {
		i =  allocate_payload.number_sdp_streams - 1;
		allocate_payload.sdp_stream_sink[i] = buf[idx] >> 4;
		idx++;
	}

	if (allocate_payload.port_number >= ctx->port_num) {
		pr_err("invalid port num\n");
		goto err;
	}

	buf = ctx->down_rep.msg;
	idx = 0;

	buf[idx] = DP_ALLOCATE_PAYLOAD;
	idx++;

	buf[idx] = allocate_payload.port_number;
	idx++;

	buf[idx] = allocate_payload.vcpi;
	idx++;

	buf[idx] = allocate_payload.pbn >> 8;
	idx++;

	buf[idx] = allocate_payload.pbn & 0xFF;
	idx++;

	return idx;
err:
	return msm_dp_sideband_build_nak_rep(ctx);
}

static int msm_dp_sideband_build_power_updown_phy_rep(
		struct msm_dp_mst_sim_context *ctx)
{
	u8 port_num;
	u8 *buf;
	int idx;

	buf = ctx->down_req.msg;
	port_num = buf[1] >> 4;

	if (port_num >= ctx->port_num) {
		pr_err("invalid port num\n");
		goto err;
	}

	buf = ctx->down_rep.msg;
	idx = 0;

	buf[idx] = ctx->down_req.msg[0];
	idx++;

	buf[idx] = port_num;
	idx++;

	return idx;
err:
	return msm_dp_sideband_build_nak_rep(ctx);
}

static int msm_dp_sideband_build_clear_payload_id_table_rep(
		struct msm_dp_mst_sim_context *ctx)
{
	u8 *buf = ctx->down_rep.msg;
	int idx = 0;

	buf[idx] = DP_CLEAR_PAYLOAD_ID_TABLE;
	idx++;

	return idx;
}

static inline int msm_dp_sideband_update_esi(struct msm_dp_mst_sim_context *ctx)
{
	ctx->esi[0] = ctx->port_num;
	ctx->esi[1] = DP_DOWN_REP_MSG_RDY;
	ctx->esi[2] = 0;

	return 0;
}

static inline bool msm_dp_sideband_pending_esi(
		struct msm_dp_mst_sim_context *ctx)
{
	return !!(ctx->esi[1] & DP_DOWN_REP_MSG_RDY);
}

static int msm_dp_mst_sim_clear_esi(struct msm_dp_mst_sim_context *ctx,
		struct drm_dp_aux_msg *msg)
{
	size_t i;
	u8 old_esi = ctx->esi[1];
	u32 addr = msg->address - DP_SINK_COUNT_ESI;

	if (msg->size - addr >= 16) {
		msg->reply = DP_AUX_NATIVE_REPLY_NACK;
		return 0;
	}

	mutex_lock(&ctx->session_lock);

	for (i = 0; i < msg->size; i++)
		ctx->esi[addr + i] &= ~((u8 *)msg->buffer)[i];

	if ((old_esi & DP_DOWN_REP_MSG_RDY) &&
			!(ctx->esi[1] & DP_DOWN_REP_MSG_RDY)) {
		complete(&ctx->session_comp);
	}

	mutex_unlock(&ctx->session_lock);

	msg->reply = DP_AUX_NATIVE_REPLY_ACK;
	return 0;
}

static int msm_dp_mst_sim_read_esi(struct msm_dp_mst_sim_context *ctx,
		struct drm_dp_aux_msg *msg)
{
	u32 addr = msg->address - DP_SINK_COUNT_ESI;

	if (msg->size - addr >= 16) {
		msg->reply = DP_AUX_NATIVE_REPLY_NACK;
		return 0;
	}

	memcpy(msg->buffer, &ctx->esi[addr], msg->size);
	msg->reply = DP_AUX_NATIVE_REPLY_ACK;

	return 0;
}

static int msm_dp_mst_sim_down_req_internal(struct msm_dp_mst_sim_context *ctx,
		struct drm_dp_aux_msg *aux_msg)
{
	struct drm_dp_sideband_msg_rx *msg = &ctx->down_req;
	struct drm_dp_sideband_msg_hdr hdr;
	bool seqno;
	int ret, size, len, hdr_len;

	ret = msm_dp_get_one_sb_msg(msg, aux_msg);
	if (!ret)
		return -EINVAL;

	if (!msg->have_eomt)
		return 0;

	seqno = msg->initial_hdr.seqno;

	switch (msg->msg[0]) {
	case DP_LINK_ADDRESS:
		size = msm_dp_sideband_build_link_address_rep(ctx);
		break;
	case DP_REMOTE_I2C_READ:
		size = msm_dp_sideband_build_remote_i2c_read_rep(ctx);
		break;
	case DP_ENUM_PATH_RESOURCES:
		size = msm_dp_sideband_build_enum_path_resources_rep(ctx);
		break;
	case DP_ALLOCATE_PAYLOAD:
		size = msm_dp_sideband_build_allocate_payload_rep(ctx);
		break;
	case DP_POWER_DOWN_PHY:
	case DP_POWER_UP_PHY:
		size = msm_dp_sideband_build_power_updown_phy_rep(ctx);
		break;
	case DP_CLEAR_PAYLOAD_ID_TABLE:
		size = msm_dp_sideband_build_clear_payload_id_table_rep(ctx);
		break;
	default:
		size = msm_dp_sideband_build_nak_rep(ctx);
		break;
	}

	if (ctx->host_req)
		ctx->host_req(ctx->host_dev,
			ctx->down_req.msg, ctx->down_req.curlen,
			ctx->down_rep.msg, &size);

	memset(msg, 0, sizeof(*msg));
	msg = &ctx->down_rep;
	msg->curlen = 0;

	while (msg->curlen < size) {
		/* copy data */
		len = min(size - msg->curlen, 44);
		memcpy(&ctx->dpcd[3], &msg->msg[msg->curlen], len);
		msg->curlen += len;

		/* build header */
		memset(&hdr, 0, sizeof(struct drm_dp_sideband_msg_hdr));
		hdr.broadcast = 0;
		hdr.path_msg = 0;
		hdr.lct = 1;
		hdr.lcr = 0;
		hdr.seqno = seqno;
		hdr.msg_len = len + 1;
		hdr.eomt = (msg->curlen == size);
		hdr.somt = (msg->curlen == len);
		drm_dp_encode_sideband_msg_hdr(&hdr, ctx->dpcd, &hdr_len);

		/* build crc */
		ctx->dpcd[len + 3] = drm_dp_msg_data_crc4(&ctx->dpcd[3], len);

		/* update esi */
		mutex_lock(&ctx->session_lock);
		msm_dp_sideband_update_esi(ctx);
		mutex_unlock(&ctx->session_lock);

		/* notify host */
		ctx->host_hpd_irq(ctx->host_dev);

		/* wait until esi is cleared */
		mutex_lock(&ctx->session_lock);
		while (msm_dp_sideband_pending_esi(ctx)) {
			mutex_unlock(&ctx->session_lock);
			wait_for_completion(&ctx->session_comp);
			mutex_lock(&ctx->session_lock);
		}
		mutex_unlock(&ctx->session_lock);
	}

	return 0;
}

static void msm_dp_mst_sim_down_req_work(struct work_struct *work)
{
	struct msm_dp_mst_sim_work *sim_work =
		container_of(work, struct msm_dp_mst_sim_work, base);
	struct drm_dp_aux_msg msg;

	msg.address = sim_work->address;
	msg.buffer = sim_work->buffer;
	msg.size = sim_work->size;

	msm_dp_mst_sim_down_req_internal(sim_work->ctx, &msg);

	kfree(sim_work);
}

static int msm_dp_mst_sim_down_req(struct msm_dp_mst_sim_context *ctx,
		struct drm_dp_aux_msg *aux_msg)
{
	struct msm_dp_mst_sim_work *work;

	if (aux_msg->size >= 256) {
		aux_msg->reply = DP_AUX_NATIVE_REPLY_NACK;
		return 0;
	}

	msm_dp_sideband_hex_dump("request",
		aux_msg->address, aux_msg->buffer, aux_msg->size);

	work = kzalloc(sizeof(*work), GFP_KERNEL);
	if (!work) {
		aux_msg->reply = DP_AUX_NATIVE_REPLY_NACK;
		return 0;
	}

	work->ctx = ctx;
	work->address = aux_msg->address;
	work->size = aux_msg->size;
	memcpy(work->buffer, aux_msg->buffer, aux_msg->size);

	INIT_WORK(&work->base, msm_dp_mst_sim_down_req_work);
	queue_work(ctx->wq, &work->base);

	aux_msg->reply = DP_AUX_NATIVE_REPLY_ACK;
	return 0;
}

static int msm_dp_mst_sim_down_rep(struct msm_dp_mst_sim_context *ctx,
		struct drm_dp_aux_msg *msg)
{
	u32 addr = msg->address - DP_SIDEBAND_MSG_DOWN_REP_BASE;

	memcpy(msg->buffer, &ctx->dpcd[addr], msg->size);
	msg->reply = DP_AUX_NATIVE_REPLY_ACK;

	msm_dp_sideband_hex_dump("reply",
		addr, msg->buffer, msg->size);

	return 0;
}

int msm_dp_mst_sim_transfer(void *mst_sim_context, struct drm_dp_aux_msg *msg)
{
	if (msg->request == DP_AUX_NATIVE_WRITE) {
		if (msg->address >= DP_SIDEBAND_MSG_DOWN_REQ_BASE &&
		    msg->address < DP_SIDEBAND_MSG_DOWN_REQ_BASE + 256)
			return msm_dp_mst_sim_down_req(mst_sim_context, msg);

		if (msg->address >= DP_SINK_COUNT_ESI &&
		    msg->address < DP_SINK_COUNT_ESI + 14)
			return msm_dp_mst_sim_clear_esi(mst_sim_context, msg);
	} else if (msg->request == DP_AUX_NATIVE_READ) {
		if (msg->address >= DP_SIDEBAND_MSG_DOWN_REP_BASE &&
		    msg->address < DP_SIDEBAND_MSG_DOWN_REP_BASE + 256)
			return msm_dp_mst_sim_down_rep(mst_sim_context, msg);

		if (msg->address >= DP_SINK_COUNT_ESI &&
		    msg->address < DP_SINK_COUNT_ESI + 14)
			return msm_dp_mst_sim_read_esi(mst_sim_context, msg);
	}

	return -EINVAL;
}

int msm_dp_mst_sim_update(void *mst_sim_context, u32 port_num,
		struct msm_dp_mst_sim_port *ports)
{
	struct msm_dp_mst_sim_context *ctx = mst_sim_context;
	u8 *edid;
	int rc = 0;
	u32 i;

	if (port_num >= 15)
		return -EINVAL;

	mutex_lock(&ctx->session_lock);

	for (i = 0; i < ctx->port_num; i++)
		kfree(ctx->ports[i].edid);
	kfree(ctx->ports);
	ctx->port_num = 0;

	ctx->ports = kcalloc(port_num, sizeof(*ports), GFP_KERNEL);
	if (!ctx->ports) {
		rc = -ENOMEM;
		goto fail;
	}

	ctx->port_num = port_num;
	for (i = 0; i < port_num; i++) {
		ctx->ports[i] = ports[i];
		if (ports[i].edid_size) {
			if (!ports[i].edid) {
				rc = -EINVAL;
				goto fail;
			}

			edid = kzalloc(ports[i].edid_size,
					GFP_KERNEL);
			if (!edid) {
				rc = -ENOMEM;
				goto fail;
			}

			memcpy(edid, ports[i].edid, ports[i].edid_size);
			ctx->ports[i].edid = edid;
		}
	}

fail:
	if (rc) {
		for (i = 0; i < ctx->port_num; i++)
			kfree(ctx->ports[i].edid);
		kfree(ctx->ports);
	}

	mutex_unlock(&ctx->session_lock);
	return rc;
}

int msm_dp_mst_sim_create(const struct msm_dp_mst_sim_cfg *cfg,
		void **mst_sim_context)
{
	struct msm_dp_mst_sim_context *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->host_dev = cfg->host_dev;
	ctx->host_hpd_irq = cfg->host_hpd_irq;
	ctx->host_req = cfg->host_req;
	memcpy(ctx->guid, cfg->guid, 16);

	mutex_init(&ctx->session_lock);
	init_completion(&ctx->session_comp);

	ctx->wq = create_singlethread_workqueue("dp_mst_sim");
	if (IS_ERR_OR_NULL(ctx->wq)) {
		pr_err("Error creating wq\n");
		kfree(ctx);
		return -EPERM;
	}

	*mst_sim_context = ctx;
	return 0;
}

int msm_dp_mst_sim_destroy(void *mst_sim_context)
{
	struct msm_dp_mst_sim_context *ctx = mst_sim_context;
	u32 i;

	for (i = 0; i < ctx->port_num; i++)
		kfree(ctx->ports[i].edid);
	kfree(ctx->ports);

	destroy_workqueue(ctx->wq);

	return 0;
}

