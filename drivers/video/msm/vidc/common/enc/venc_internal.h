/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#ifndef VENC_INTERNAL_H
#define VENC_INTERNAL_H

#include <linux/msm_vidc_enc.h>
#include <linux/cdev.h>
#include <media/msm/vidc_init.h>

#define VID_ENC_MAX_NUM_OF_BUFF 100

enum venc_buffer_dir{
  VEN_BUFFER_TYPE_INPUT,
  VEN_BUFFER_TYPE_OUTPUT
};

struct vid_enc_msg {
	struct list_head list;
	struct venc_msg venc_msg_info;
};

struct vid_enc_dev {

	struct cdev cdev;
	struct device *device;
	resource_size_t phys_base;
	void __iomem *virt_base;
	unsigned int irq;
	struct clk *hclk;
	struct clk *hclk_div2;
	struct clk *pclk;
	unsigned long hclk_rate;
	struct mutex lock;
	s32 device_handle;
	struct video_client_ctx venc_clients[VIDC_MAX_NUM_CLIENTS];
	u32 num_clients;
};

u32 vid_enc_set_get_base_cfg(struct video_client_ctx *client_ctx,
		struct venc_basecfg *base_config, u32 set_flag);

u32 vid_enc_set_get_inputformat(struct video_client_ctx *client_ctx,
		u32 *input_format, u32 set_flag);

u32 vid_enc_set_get_codec(struct video_client_ctx *client_ctx, u32 *codec,
		u32 set_flag);

u32 vid_enc_set_get_framesize(struct video_client_ctx *client_ctx,
		u32 *height, u32 *width, u32 set_flag);

u32 vid_enc_set_get_bitrate(struct video_client_ctx *client_ctx,
		struct venc_targetbitrate *venc_bitrate, u32 set_flag);

u32 vid_enc_set_get_framerate(struct video_client_ctx *client_ctx,
		struct venc_framerate *frame_rate, u32 set_flag);

u32 vid_enc_set_get_live_mode(struct video_client_ctx *client_ctx,
		struct venc_switch *encoder_switch, u32 set_flag);

u32 vid_enc_set_get_extradata(struct video_client_ctx *client_ctx,
		u32 *extradata_flag, u32 set_flag);

u32 vid_enc_set_get_short_header(struct video_client_ctx *client_ctx,
		struct venc_switch *encoder_switch, u32 set_flag);

u32 vid_enc_set_get_profile(struct video_client_ctx *client_ctx,
		struct venc_profile *profile, u32 set_flag);

u32 vid_enc_set_get_profile_level(struct video_client_ctx *client_ctx,
		struct ven_profilelevel *profile_level, u32 set_flag);

u32 vid_enc_set_get_session_qp(struct video_client_ctx *client_ctx,
		struct venc_sessionqp *session_qp, u32 set_flag);

u32 vid_enc_set_get_intraperiod(struct video_client_ctx *client_ctx,
		struct venc_intraperiod *intraperiod, u32 set_flag);

u32 vid_enc_request_iframe(struct video_client_ctx *client_ctx);

u32 vid_enc_get_sequence_header(struct video_client_ctx *client_ctx,
		struct venc_seqheader *seq_header);

u32 vid_enc_set_get_entropy_cfg(struct video_client_ctx *client_ctx,
		struct venc_entropycfg *entropy_cfg, u32 set_flag);

u32 vid_enc_set_get_dbcfg(struct video_client_ctx *client_ctx,
		struct venc_dbcfg *dbcfg, u32 set_flag);

u32 vid_enc_set_get_intrarefresh(struct video_client_ctx *client_ctx,
		struct venc_intrarefresh *intrarefresh,	u32 set_flag);

u32 vid_enc_set_get_multiclicecfg(struct video_client_ctx *client_ctx,
		struct venc_multiclicecfg *multiclicecfg, u32 set_flag);

u32 vid_enc_set_get_ratectrlcfg(struct video_client_ctx *client_ctx,
		struct venc_ratectrlcfg *ratectrlcfg, u32 set_flag);

u32 vid_enc_set_get_voptimingcfg(struct video_client_ctx *client_ctx,
		struct  venc_voptimingcfg *voptimingcfg, u32 set_flag);

u32 vid_enc_set_get_headerextension(struct video_client_ctx *client_ctx,
		struct venc_headerextension *headerextension, u32 set_flag);

u32 vid_enc_set_get_qprange(struct video_client_ctx *client_ctx,
		struct venc_qprange *qprange, u32 set_flag);

u32 vid_enc_start_stop(struct video_client_ctx *client_ctx, u32 start);

u32 vid_enc_pause_resume(struct video_client_ctx *client_ctx, u32 pause);

u32 vid_enc_flush(struct video_client_ctx *client_ctx,
		struct venc_bufferflush *bufferflush);

u32 vid_enc_get_buffer_req(struct video_client_ctx *client_ctx,
		struct venc_allocatorproperty *venc_buf_req, u32 input_dir);

u32 vid_enc_set_buffer_req(struct video_client_ctx *client_ctx,
		struct venc_allocatorproperty *venc_buf_req, u32 input_dir);

u32 vid_enc_set_buffer(struct video_client_ctx *client_ctx,
		struct venc_bufferpayload *buffer_info,
		enum venc_buffer_dir buffer);

u32 vid_enc_free_buffer(struct video_client_ctx *client_ctx,
		struct venc_bufferpayload *buffer_info,
		enum venc_buffer_dir buffer);

u32 vid_enc_encode_frame(struct video_client_ctx *client_ctx,
		struct venc_buffer *input_frame_info);

u32 vid_enc_fill_output_buffer(struct video_client_ctx *client_ctx,
		struct venc_buffer *output_frame_info);

u32 vid_enc_set_recon_buffers(struct video_client_ctx *client_ctx,
		struct venc_recon_addr *venc_recon);

u32 vid_enc_free_recon_buffers(struct video_client_ctx *client_ctx,
		struct venc_recon_addr *venc_recon);

u32 vid_enc_get_recon_buffer_size(struct video_client_ctx *client_ctx,
		struct venc_recon_buff_size *venc_recon_size);

#endif
