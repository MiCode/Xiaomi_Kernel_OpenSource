/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/android_pmem.h>
#include <linux/clk.h>
#include <mach/msm_subsystem_map.h>
#include "vidc_type.h"
#include "vcd_api.h"
#include "venc_internal.h"
#include "vidc_init.h"

#if DEBUG
#define DBG(x...) printk(KERN_DEBUG x)
#else
#define DBG(x...)
#endif

#define ERR(x...) printk(KERN_ERR x)
static unsigned int vidc_mmu_subsystem[] = {
	MSM_SUBSYSTEM_VIDEO};


u32 vid_enc_set_get_base_cfg(struct video_client_ctx *client_ctx,
		struct venc_basecfg *base_config, u32 set_flag)
{
	struct venc_targetbitrate venc_bitrate;
	struct venc_framerate frame_rate;
	u32 current_codec;

	if (!client_ctx || !base_config)
		return false;

	if (!vid_enc_set_get_codec(client_ctx, &current_codec, false))
			return false;

	DBG("%s(): Current Codec Type = %u\n", __func__, current_codec);
	if (current_codec != base_config->codectype) {
		if (!vid_enc_set_get_codec(client_ctx,
				(u32 *)&base_config->codectype, set_flag))
			return false;
	}

	if (!vid_enc_set_get_inputformat(client_ctx,
			(u32 *)&base_config->inputformat, set_flag))
		return false;

	if (!vid_enc_set_get_framesize(client_ctx,
			(u32 *)&base_config->input_height,
			(u32 *)&base_config->input_width, set_flag))
		return false;

	if (set_flag)
		venc_bitrate.target_bitrate = base_config->targetbitrate;

	if (!vid_enc_set_get_bitrate(client_ctx, &venc_bitrate, set_flag))
		return false;

	if (!set_flag)
		base_config->targetbitrate = venc_bitrate.target_bitrate;

	if (set_flag) {
		frame_rate.fps_denominator = base_config->fps_den;
		frame_rate.fps_numerator = base_config->fps_num;
	}

	if (!vid_enc_set_get_framerate(client_ctx, &frame_rate, set_flag))
		return false;

	if (!set_flag) {
		base_config->fps_den = frame_rate.fps_denominator;
		base_config->fps_num = frame_rate.fps_numerator;
	}

	return true;
}

u32 vid_enc_set_get_inputformat(struct video_client_ctx *client_ctx,
		u32 *input_format, u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_buffer_format format;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 status = true;

	if (!client_ctx || !input_format)
		return false;

	vcd_property_hdr.prop_id = VCD_I_BUFFER_FORMAT;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_buffer_format);

	if (set_flag) {
		switch (*input_format) {
		case VEN_INPUTFMT_NV12:
			format.buffer_format = VCD_BUFFER_FORMAT_NV12;
			break;
		case VEN_INPUTFMT_NV12_16M2KA:
			format.buffer_format =
				VCD_BUFFER_FORMAT_NV12_16M2KA;
			break;
		default:
			status = false;
			break;
		}

		if (status) {
			vcd_status = vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &format);
			if (vcd_status) {
				status = false;
				ERR("%s(): Set VCD_I_BUFFER_FORMAT Failed\n",
						 __func__);
			}
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &format);

		if (vcd_status) {
			status = false;
			ERR("%s(): Get VCD_I_BUFFER_FORMAT Failed\n", __func__);
		} else {
			switch (format.buffer_format) {
			case VCD_BUFFER_FORMAT_NV12:
				*input_format = VEN_INPUTFMT_NV12;
				break;
			case VCD_BUFFER_FORMAT_TILE_4x2:
				*input_format = VEN_INPUTFMT_NV21;
				break;
			default:
				status = false;
				break;
			}
		}
	}
	return status;
}

u32 vid_enc_set_get_codec(struct video_client_ctx *client_ctx, u32 *codec,
		u32 set_flag)
{
	struct vcd_property_codec vcd_property_codec;
	struct vcd_property_hdr vcd_property_hdr;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 status = true;

	if (!client_ctx || !codec)
		return false;

	vcd_property_hdr.prop_id = VCD_I_CODEC;
	vcd_property_hdr.sz = sizeof(struct vcd_property_codec);

	if (set_flag) {
		switch (*codec) {
		case VEN_CODEC_MPEG4:
			vcd_property_codec.codec = VCD_CODEC_MPEG4;
			break;
		case VEN_CODEC_H263:
			vcd_property_codec.codec = VCD_CODEC_H263;
			break;
		case VEN_CODEC_H264:
			vcd_property_codec.codec = VCD_CODEC_H264;
			break;
		default:
			status = false;
			break;
		}

		if (status) {
			vcd_status = vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_codec);
			if (vcd_status) {
				status = false;
				ERR("%s(): Set VCD_I_CODEC Failed\n", __func__);
			}
		}
	}	else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_codec);

		if (vcd_status) {
			status = false;
			ERR("%s(): Get VCD_I_CODEC Failed\n",
					 __func__);
		} else {
			switch (vcd_property_codec.codec) {
			case VCD_CODEC_H263:
				*codec = VEN_CODEC_H263;
				break;
			case VCD_CODEC_H264:
				*codec = VEN_CODEC_H264;
				break;
			case VCD_CODEC_MPEG4:
				*codec = VEN_CODEC_MPEG4;
				break;
			case VCD_CODEC_DIVX_3:
			case VCD_CODEC_DIVX_4:
			case VCD_CODEC_DIVX_5:
			case VCD_CODEC_DIVX_6:
			case VCD_CODEC_MPEG1:
			case VCD_CODEC_MPEG2:
			case VCD_CODEC_VC1:
			case VCD_CODEC_VC1_RCV:
			case VCD_CODEC_XVID:
			default:
				status = false;
				break;
			}
		}
	}
	return status;
}

u32 vid_enc_set_get_framesize(struct video_client_ctx *client_ctx,
		u32 *height, u32 *width, u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_frame_size frame_size;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !height || !width)
		return false;

	vcd_property_hdr.prop_id = VCD_I_FRAME_SIZE;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_frame_size);

	vcd_status = vcd_get_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &frame_size);

	if (vcd_status) {
		ERR("%s(): Get VCD_I_FRAME_SIZE Failed\n",
				__func__);
		return false;
	}
	if (set_flag) {
		if (frame_size.height != *height ||
			frame_size.width != *width) {
			DBG("%s(): ENC Set Size (%d x %d)\n",
				__func__, *height, *width);
			frame_size.height = *height;
			frame_size.width = *width;
			vcd_status = vcd_set_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &frame_size);
			if (vcd_status) {
				ERR("%s(): Set VCD_I_FRAME_SIZE Failed\n",
						__func__);
				return false;
			}
		}
	} else {
		*height = frame_size.height;
		*width = frame_size.width;
	}
	return true;
}

u32 vid_enc_set_get_bitrate(struct video_client_ctx *client_ctx,
		struct venc_targetbitrate *venc_bitrate, u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_target_bitrate bit_rate;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !venc_bitrate)
		return false;

	vcd_property_hdr.prop_id = VCD_I_TARGET_BITRATE;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_target_bitrate);
	if (set_flag) {
		bit_rate.target_bitrate = venc_bitrate->target_bitrate;
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &bit_rate);

		if (vcd_status) {
			ERR("%s(): Set VCD_I_TARGET_BITRATE Failed\n",
					__func__);
			return false;
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &bit_rate);

		if (vcd_status) {
			ERR("%s(): Get VCD_I_TARGET_BITRATE Failed\n",
					__func__);
			return false;
		}
		venc_bitrate->target_bitrate = bit_rate.target_bitrate;
	}
	return true;
}

u32 vid_enc_set_get_framerate(struct video_client_ctx *client_ctx,
		struct venc_framerate *frame_rate, u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_frame_rate vcd_frame_rate;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !frame_rate)
		return false;

	vcd_property_hdr.prop_id = VCD_I_FRAME_RATE;
	vcd_property_hdr.sz =
				sizeof(struct vcd_property_frame_rate);

	if (set_flag) {
		vcd_frame_rate.fps_denominator = frame_rate->fps_denominator;
		vcd_frame_rate.fps_numerator = frame_rate->fps_numerator;
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &vcd_frame_rate);

		if (vcd_status) {
			ERR("%s(): Set VCD_I_FRAME_RATE Failed\n",
					__func__);
			return false;
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_frame_rate);

		if (vcd_status) {
			ERR("%s(): Get VCD_I_FRAME_RATE Failed\n",
					__func__);
			return false;
		}
		frame_rate->fps_denominator = vcd_frame_rate.fps_denominator;
		frame_rate->fps_numerator = vcd_frame_rate.fps_numerator;
	}
	return true;
}

u32 vid_enc_set_get_live_mode(struct video_client_ctx *client_ctx,
		struct venc_switch *encoder_switch, u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_live live_mode;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx)
		return false;

	vcd_property_hdr.prop_id = VCD_I_LIVE;
	vcd_property_hdr.sz =
				sizeof(struct vcd_property_live);

	if (set_flag) {
		live_mode.live = 1;
		if (!encoder_switch->status)
			live_mode.live = 0;

		vcd_status = vcd_set_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &live_mode);
		if (vcd_status) {
			ERR("%s(): Set VCD_I_LIVE Failed\n",
					__func__);
			return false;
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &live_mode);

		if (vcd_status) {
			ERR("%s(): Get VCD_I_LIVE Failed\n",
					__func__);
			return false;
		}	else {
			encoder_switch->status = 1;
			if (!live_mode.live)
				encoder_switch->status = 0;
		}
	}
	return true;
}

u32 vid_enc_set_get_short_header(struct video_client_ctx *client_ctx,
		struct venc_switch *encoder_switch,	u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_short_header short_header;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !encoder_switch)
		return false;

	vcd_property_hdr.prop_id = VCD_I_SHORT_HEADER;
	vcd_property_hdr.sz =
				sizeof(struct vcd_property_short_header);

	if (set_flag) {
		short_header.short_header = (u32) encoder_switch->status;
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
			&vcd_property_hdr, &short_header);

		if (vcd_status) {
			ERR("%s(): Set VCD_I_SHORT_HEADER Failed\n",
					__func__);
			return false;
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &short_header);

		if (vcd_status) {
			ERR("%s(): Get VCD_I_SHORT_HEADER Failed\n",
					__func__);
			return false;
		}	else {
			encoder_switch->status =
				(u8) short_header.short_header;
		}
	}
	return true;
}

u32 vid_enc_set_get_profile(struct video_client_ctx *client_ctx,
		struct venc_profile *profile, u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_profile profile_type;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 status = true;

	if (!client_ctx || !profile)
		return false;

	vcd_property_hdr.prop_id = VCD_I_PROFILE;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_profile);

	if (set_flag) {
		switch (profile->profile) {
		case VEN_PROFILE_MPEG4_SP:
			profile_type.profile = VCD_PROFILE_MPEG4_SP;
			break;
		case VEN_PROFILE_MPEG4_ASP:
			profile_type.profile = VCD_PROFILE_MPEG4_ASP;
			break;
		case VEN_PROFILE_H264_BASELINE:
			profile_type.profile = VCD_PROFILE_H264_BASELINE;
			break;
		case VEN_PROFILE_H264_MAIN:
			profile_type.profile = VCD_PROFILE_H264_MAIN;
			break;
		case VEN_PROFILE_H264_HIGH:
			profile_type.profile = VCD_PROFILE_H264_HIGH;
			break;
		case VEN_PROFILE_H263_BASELINE:
			profile_type.profile = VCD_PROFILE_H263_BASELINE;
			break;
		default:
			status = false;
			break;
		}

		if (status) {
			vcd_status = vcd_set_property(client_ctx->vcd_handle,
			&vcd_property_hdr, &profile_type);

			if (vcd_status) {
				ERR("%s(): Set VCD_I_PROFILE Failed\n",
						__func__);
				return false;
			}
		}
	}	else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &profile_type);

		if (vcd_status) {
			ERR("%s(): Get VCD_I_PROFILE Failed\n",
					__func__);
			return false;
		} else {
			switch (profile_type.profile) {
			case VCD_PROFILE_H263_BASELINE:
				profile->profile = VEN_PROFILE_H263_BASELINE;
				break;
			case VCD_PROFILE_H264_BASELINE:
				profile->profile = VEN_PROFILE_H264_BASELINE;
				break;
			case VCD_PROFILE_H264_HIGH:
				profile->profile = VEN_PROFILE_H264_HIGH;
				break;
			case VCD_PROFILE_H264_MAIN:
				profile->profile = VEN_PROFILE_H264_MAIN;
				break;
			case VCD_PROFILE_MPEG4_ASP:
				profile->profile = VEN_PROFILE_MPEG4_ASP;
				break;
			case VCD_PROFILE_MPEG4_SP:
				profile->profile = VEN_PROFILE_MPEG4_SP;
				break;
			default:
				status = false;
				break;
			}
		}
	}
	return status;
}

u32 vid_enc_set_get_profile_level(struct video_client_ctx *client_ctx,
		struct ven_profilelevel *profile_level,	u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_level level;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 status = true;

	if (!client_ctx || !profile_level)
		return false;

	vcd_property_hdr.prop_id = VCD_I_LEVEL;
	vcd_property_hdr.sz =
			sizeof(struct vcd_property_level);

	if (set_flag) {
		switch (profile_level->level) {
		case VEN_LEVEL_MPEG4_0:
			level.level = VCD_LEVEL_MPEG4_0;
			break;
		case VEN_LEVEL_MPEG4_1:
			level.level = VCD_LEVEL_MPEG4_1;
			break;
		case VEN_LEVEL_MPEG4_2:
			level.level = VCD_LEVEL_MPEG4_2;
			break;
		case VEN_LEVEL_MPEG4_3:
			level.level = VCD_LEVEL_MPEG4_3;
			break;
		case VEN_LEVEL_MPEG4_4:
			level.level = VCD_LEVEL_MPEG4_4;
			break;
		case VEN_LEVEL_MPEG4_5:
			level.level = VCD_LEVEL_MPEG4_5;
			break;
		case VEN_LEVEL_MPEG4_3b:
			level.level = VCD_LEVEL_MPEG4_3b;
			break;
		case VEN_LEVEL_MPEG4_6:
			level.level = VCD_LEVEL_MPEG4_6;
			break;
		case VEN_LEVEL_H264_1:
			level.level = VCD_LEVEL_H264_1;
			break;
		case VEN_LEVEL_H264_1b:
			level.level = VCD_LEVEL_H264_1b;
			break;
		case VEN_LEVEL_H264_1p1:
			level.level = VCD_LEVEL_H264_1p1;
			break;
		case VEN_LEVEL_H264_1p2:
			level.level = VCD_LEVEL_H264_1p2;
			break;
		case VEN_LEVEL_H264_1p3:
			level.level = VCD_LEVEL_H264_1p3;
			break;
		case VEN_LEVEL_H264_2:
			level.level = VCD_LEVEL_H264_2;
			break;
		case VEN_LEVEL_H264_2p1:
			level.level = VCD_LEVEL_H264_2p1;
			break;
		case VEN_LEVEL_H264_2p2:
			level.level = VCD_LEVEL_H264_2p2;
			break;
		case VEN_LEVEL_H264_3:
			level.level = VCD_LEVEL_H264_3;
			break;
		case VEN_LEVEL_H264_3p1:
			level.level = VCD_LEVEL_H264_3p1;
			break;
		case VEN_LEVEL_H264_4:
			level.level = VCD_LEVEL_H264_4;
			break;
		case VEN_LEVEL_H263_10:
			level.level = VCD_LEVEL_H263_10;
			break;
		case VEN_LEVEL_H263_20:
			level.level = VCD_LEVEL_H263_20;
			break;
		case VEN_LEVEL_H263_30:
			level.level = VCD_LEVEL_H263_30;
			break;
		case VEN_LEVEL_H263_40:
			level.level = VCD_LEVEL_H263_40;
			break;
		case VEN_LEVEL_H263_45:
			level.level = VCD_LEVEL_H263_45;
			break;
		case VEN_LEVEL_H263_50:
			level.level = VCD_LEVEL_H263_50;
			break;
		case VEN_LEVEL_H263_60:
			level.level = VCD_LEVEL_H263_60;
			break;
		case VEN_LEVEL_H263_70:
			level.level = VCD_LEVEL_H263_70;
			break;
		default:
			status = false;
			break;
		}
		if (status) {
			vcd_status = vcd_set_property(client_ctx->vcd_handle,
						&vcd_property_hdr, &level);

			if (vcd_status) {
				ERR("%s(): Set VCD_I_LEVEL Failed\n",
						__func__);
				return false;
			}
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &level);

		if (vcd_status) {
			ERR("%s(): Get VCD_I_LEVEL Failed\n",
					__func__);
			return false;
		} else {
			switch (level.level) {
			case VCD_LEVEL_MPEG4_0:
				profile_level->level = VEN_LEVEL_MPEG4_0;
				break;
			case VCD_LEVEL_MPEG4_1:
				profile_level->level = VEN_LEVEL_MPEG4_1;
				break;
			case VCD_LEVEL_MPEG4_2:
				profile_level->level = VEN_LEVEL_MPEG4_2;
				break;
			case VCD_LEVEL_MPEG4_3:
				profile_level->level = VEN_LEVEL_MPEG4_3;
				break;
			case VCD_LEVEL_MPEG4_4:
				profile_level->level = VEN_LEVEL_MPEG4_4;
				break;
			case VCD_LEVEL_MPEG4_5:
				profile_level->level = VEN_LEVEL_MPEG4_5;
				break;
			case VCD_LEVEL_MPEG4_3b:
				profile_level->level = VEN_LEVEL_MPEG4_3b;
				break;
			case VCD_LEVEL_H264_1:
				profile_level->level = VEN_LEVEL_H264_1;
				break;
			case VCD_LEVEL_H264_1b:
				profile_level->level = VEN_LEVEL_H264_1b;
				break;
			case VCD_LEVEL_H264_1p1:
				profile_level->level = VEN_LEVEL_H264_1p1;
				break;
			case VCD_LEVEL_H264_1p2:
				profile_level->level = VEN_LEVEL_H264_1p2;
				break;
			case VCD_LEVEL_H264_1p3:
				profile_level->level = VEN_LEVEL_H264_1p3;
				break;
			case VCD_LEVEL_H264_2:
				profile_level->level = VEN_LEVEL_H264_2;
				break;
			case VCD_LEVEL_H264_2p1:
				profile_level->level = VEN_LEVEL_H264_2p1;
				break;
			case VCD_LEVEL_H264_2p2:
				profile_level->level = VEN_LEVEL_H264_2p2;
				break;
			case VCD_LEVEL_H264_3:
				profile_level->level = VEN_LEVEL_H264_3;
				break;
			case VCD_LEVEL_H264_3p1:
				profile_level->level = VEN_LEVEL_H264_3p1;
				break;
			case VCD_LEVEL_H264_3p2:
				status = false;
				break;
			case VCD_LEVEL_H264_4:
				profile_level->level = VEN_LEVEL_H264_4;
				break;
			case VCD_LEVEL_H263_10:
				profile_level->level = VEN_LEVEL_H263_10;
				break;
			case VCD_LEVEL_H263_20:
				profile_level->level = VEN_LEVEL_H263_20;
				break;
			case VCD_LEVEL_H263_30:
				profile_level->level = VEN_LEVEL_H263_30;
				break;
			case VCD_LEVEL_H263_40:
				profile_level->level = VEN_LEVEL_H263_40;
				break;
			case VCD_LEVEL_H263_45:
				profile_level->level = VEN_LEVEL_H263_45;
				break;
			case VCD_LEVEL_H263_50:
				profile_level->level = VEN_LEVEL_H263_50;
				break;
			case VCD_LEVEL_H263_60:
				profile_level->level = VEN_LEVEL_H263_60;
				break;
			case VCD_LEVEL_H263_70:
				status = false;
				break;
			default:
				status = false;
				break;
			}
		}
	}
	return status;
}

u32 vid_enc_set_get_session_qp(struct video_client_ctx *client_ctx,
		struct venc_sessionqp *session_qp, u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_session_qp qp;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !session_qp)
		return false;

	vcd_property_hdr.prop_id = VCD_I_SESSION_QP;
	vcd_property_hdr.sz =
			sizeof(struct vcd_property_session_qp);

	if (set_flag) {
		qp.i_frame_qp = session_qp->iframeqp;
		qp.p_frame_qp = session_qp->pframqp;

		vcd_status = vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &qp);

		if (vcd_status) {
			ERR("%s(): Set VCD_I_SESSION_QP Failed\n",
					__func__);
			return false;
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &qp);

		if (vcd_status) {
			ERR("%s(): Set VCD_I_SESSION_QP Failed\n",
					__func__);
			return false;
		} else {
			session_qp->iframeqp = qp.i_frame_qp;
			session_qp->pframqp = qp.p_frame_qp;
		}
	}
	return true;
}

u32 vid_enc_set_get_intraperiod(struct video_client_ctx *client_ctx,
		struct venc_intraperiod *intraperiod,	u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_i_period period;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !intraperiod)
		return false;

	vcd_property_hdr.prop_id = VCD_I_INTRA_PERIOD;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_i_period);

	if (set_flag) {
		period.p_frames = intraperiod->num_pframes;
		period.b_frames = intraperiod->num_bframes;
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &period);

		if (vcd_status) {
			ERR("%s(): Set VCD_I_INTRA_PERIOD Failed\n",
					__func__);
			return false;
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &period);

		if (vcd_status) {
			ERR("%s(): Get VCD_I_INTRA_PERIOD Failed\n",
					__func__);
			return false;
		} else
			intraperiod->num_pframes = period.p_frames;
	}
	return true;
}

u32 vid_enc_request_iframe(struct video_client_ctx *client_ctx)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_req_i_frame request;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 status = true;

	if (!client_ctx)
		return false;

	vcd_property_hdr.prop_id = VCD_I_REQ_IFRAME;
	vcd_property_hdr.sz =
				sizeof(struct vcd_property_req_i_frame);
	request.req_i_frame = 1;

	vcd_status = vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &request);

	if (vcd_status) {
		ERR("%s(): Set VCD_I_REQ_IFRAME Failed\n",
				__func__);
		return false;
	}
	return status;
}

u32 vid_enc_get_sequence_header(struct video_client_ctx *client_ctx,
		struct venc_seqheader	*seq_header)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_sequence_hdr hdr;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 status = true;

	if (!client_ctx ||
			!seq_header || !seq_header->bufsize)
		return false;

	vcd_property_hdr.prop_id = VCD_I_SEQ_HEADER;
	vcd_property_hdr.sz =
		sizeof(struct vcd_sequence_hdr);

	hdr.sequence_header =
		kzalloc(seq_header->bufsize, GFP_KERNEL);
	seq_header->hdrbufptr = hdr.sequence_header;

	if (!hdr.sequence_header)
		return false;
	hdr.sequence_header_len = seq_header->bufsize;
	vcd_status = vcd_get_property(client_ctx->vcd_handle,
			&vcd_property_hdr, &hdr);

	if (vcd_status) {
		ERR("%s(): Get VCD_I_SEQ_HEADER Failed\n",
				__func__);
		status = false;
	}
	return true;
}

u32 vid_enc_set_get_entropy_cfg(struct video_client_ctx *client_ctx,
		struct venc_entropycfg *entropy_cfg, u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_entropy_control control;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 status = true;

	if (!client_ctx || !entropy_cfg)
		return false;

	vcd_property_hdr.prop_id = VCD_I_ENTROPY_CTRL;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_entropy_control);
	if (set_flag) {
		switch (entropy_cfg->longentropysel) {
		case VEN_ENTROPY_MODEL_CAVLC:
			control.entropy_sel = VCD_ENTROPY_SEL_CAVLC;
			break;
		case VEN_ENTROPY_MODEL_CABAC:
			control.entropy_sel = VCD_ENTROPY_SEL_CABAC;
			break;
		default:
			status = false;
			break;
		}

		if (status && entropy_cfg->cabacmodel ==
				VCD_ENTROPY_SEL_CABAC) {
			switch (entropy_cfg->cabacmodel) {
			case VEN_CABAC_MODEL_0:
				control.cabac_model =
					VCD_CABAC_MODEL_NUMBER_0;
				break;
			case VEN_CABAC_MODEL_1:
				control.cabac_model =
					VCD_CABAC_MODEL_NUMBER_1;
				break;
			case VEN_CABAC_MODEL_2:
				control.cabac_model =
					VCD_CABAC_MODEL_NUMBER_2;
				break;
			default:
				status = false;
				break;
			}
		}
		if (status) {
			vcd_status = vcd_set_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &control);

			if (vcd_status) {
				ERR("%s(): Set VCD_I_ENTROPY_CTRL Failed\n",
						__func__);
				status = false;
			}
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &control);

		if (vcd_status) {
			ERR("%s(): Get VCD_I_ENTROPY_CTRL Failed\n",
					__func__);
			status = false;
		} else {
			switch (control.entropy_sel) {
			case VCD_ENTROPY_SEL_CABAC:
				entropy_cfg->cabacmodel =
					VEN_ENTROPY_MODEL_CABAC;
				break;
			case VCD_ENTROPY_SEL_CAVLC:
				entropy_cfg->cabacmodel =
					VEN_ENTROPY_MODEL_CAVLC;
				break;
			default:
				status = false;
				break;
			}

			if (status && control.entropy_sel ==
					VCD_ENTROPY_SEL_CABAC) {
				switch (control.cabac_model) {
				case VCD_CABAC_MODEL_NUMBER_0:
					entropy_cfg->cabacmodel =
						VEN_CABAC_MODEL_0;
					break;
				case VCD_CABAC_MODEL_NUMBER_1:
					entropy_cfg->cabacmodel =
						VEN_CABAC_MODEL_1;
					break;
				case VCD_CABAC_MODEL_NUMBER_2:
					entropy_cfg->cabacmodel =
						VEN_CABAC_MODEL_2;
					break;
				default:
					status = false;
					break;
				}
			}
		}
	}
	return status;
}

u32 vid_enc_set_get_dbcfg(struct video_client_ctx *client_ctx,
		struct venc_dbcfg *dbcfg, u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_db_config control;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 status = true;

	if (!client_ctx || !dbcfg)
		return false;

	vcd_property_hdr.prop_id = VCD_I_DEBLOCKING;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_db_config);

	if (set_flag) {
		switch (dbcfg->db_mode) {
		case VEN_DB_DISABLE:
			control.db_config = VCD_DB_DISABLE;
			break;
		case VEN_DB_ALL_BLKG_BNDRY:
			control.db_config = VCD_DB_ALL_BLOCKING_BOUNDARY;
			break;
		case VEN_DB_SKIP_SLICE_BNDRY:
			control.db_config = VCD_DB_SKIP_SLICE_BOUNDARY;
			break;
		default:
			status = false;
			break;
		}

		if (status) {
			control.slice_alpha_offset =
				dbcfg->slicealpha_offset;
			control.slice_beta_offset =
				dbcfg->slicebeta_offset;
			vcd_status = vcd_set_property(client_ctx->vcd_handle,
			&vcd_property_hdr, &control);
			if (vcd_status) {
				ERR("%s(): Set VCD_I_DEBLOCKING Failed\n",
						__func__);
				status = false;
			}
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &control);
		if (vcd_status) {
			ERR("%s(): Get VCD_I_DEBLOCKING Failed\n",
					__func__);
			status = false;
		} else {
			switch (control.db_config) {
			case VCD_DB_ALL_BLOCKING_BOUNDARY:
				dbcfg->db_mode = VEN_DB_ALL_BLKG_BNDRY;
				break;
			case VCD_DB_DISABLE:
				dbcfg->db_mode = VEN_DB_DISABLE;
				break;
			case VCD_DB_SKIP_SLICE_BOUNDARY:
				dbcfg->db_mode = VEN_DB_SKIP_SLICE_BNDRY;
				break;
			default:
				status = false;
				break;
			}
			dbcfg->slicealpha_offset =
				control.slice_alpha_offset;
			dbcfg->slicebeta_offset =
				control.slice_beta_offset;
		}
	}
	return status;
}

u32 vid_enc_set_get_intrarefresh(struct video_client_ctx *client_ctx,
		struct venc_intrarefresh *intrarefresh, u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_intra_refresh_mb_number control;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !intrarefresh)
		return false;

	vcd_property_hdr.prop_id = VCD_I_INTRA_REFRESH;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_intra_refresh_mb_number);

	if (set_flag) {
		control.cir_mb_number = intrarefresh->mbcount;
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &control);

		if (vcd_status) {
			ERR("%s(): Set VCD_I_INTRA_REFRESH Failed\n",
					__func__);
			return false;
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &control);

		if (vcd_status) {
			ERR("%s(): Set VCD_I_INTRA_REFRESH Failed\n",
					__func__);
			return false;
		} else
			intrarefresh->mbcount = control.cir_mb_number;
	}
	return true;
}

u32 vid_enc_set_get_multiclicecfg(struct video_client_ctx *client_ctx,
		struct venc_multiclicecfg *multiclicecfg,	u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_multi_slice control;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 status = true;

	if (!client_ctx || !multiclicecfg)
		return false;

	vcd_property_hdr.prop_id = VCD_I_MULTI_SLICE;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_multi_slice);

	if (set_flag) {
		switch (multiclicecfg->mslice_mode) {
		case VEN_MSLICE_OFF:
			control.m_slice_sel =
				VCD_MSLICE_OFF;
			break;
		case VEN_MSLICE_CNT_MB:
			control.m_slice_sel =
				VCD_MSLICE_BY_MB_COUNT;
			break;
		case VEN_MSLICE_CNT_BYTE:
			control.m_slice_sel =
				VCD_MSLICE_BY_BYTE_COUNT;
			break;
		case VEN_MSLICE_GOB:
			control.m_slice_sel =
				VCD_MSLICE_BY_GOB;
			break;
		default:
			status = false;
			break;
		}

		if (status) {
			control.m_slice_size =
				multiclicecfg->mslice_size;
			vcd_status = vcd_set_property(client_ctx->vcd_handle,
			&vcd_property_hdr, &control);

			if (vcd_status) {
				ERR("%s(): Set VCD_I_MULTI_SLICE Failed\n",
						__func__);
				status = false;
			}
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
			&vcd_property_hdr, &control);

		if (vcd_status) {
			ERR("%s(): Get VCD_I_MULTI_SLICE Failed\n",
					__func__);
			status = false;
		} else {
			multiclicecfg->mslice_size =
				control.m_slice_size;
			switch (control.m_slice_sel) {
			case VCD_MSLICE_OFF:
				multiclicecfg->mslice_mode = VEN_MSLICE_OFF;
				break;
			case VCD_MSLICE_BY_MB_COUNT:
				multiclicecfg->mslice_mode = VEN_MSLICE_CNT_MB;
				break;
			case VCD_MSLICE_BY_BYTE_COUNT:
				multiclicecfg->mslice_mode =
					VEN_MSLICE_CNT_BYTE;
				break;
			case VCD_MSLICE_BY_GOB:
				multiclicecfg->mslice_mode =
					VEN_MSLICE_GOB;
				break;
			default:
				status = false;
				break;
			}
		}
	}
	return status;
}

u32 vid_enc_set_get_ratectrlcfg(struct video_client_ctx *client_ctx,
		struct venc_ratectrlcfg *ratectrlcfg,	u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_rate_control control;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 status = true;

	if (!client_ctx || !ratectrlcfg)
		return false;

	vcd_property_hdr.prop_id = VCD_I_RATE_CONTROL;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_rate_control);

	if (set_flag) {
		switch (ratectrlcfg->rcmode) {
		case VEN_RC_OFF:
			control.rate_control = VCD_RATE_CONTROL_OFF;
			break;
		case VEN_RC_CBR_VFR:
			control.rate_control = VCD_RATE_CONTROL_CBR_VFR;
			break;
		case VEN_RC_VBR_CFR:
			control.rate_control = VCD_RATE_CONTROL_VBR_CFR;
			break;
		case VEN_RC_VBR_VFR:
			control.rate_control = VCD_RATE_CONTROL_VBR_VFR;
			break;
		case VEN_RC_CBR_CFR:
			control.rate_control = VCD_RATE_CONTROL_CBR_CFR;
			break;
		default:
			status = false;
			break;
		}

		if (status) {
			vcd_status = vcd_set_property(client_ctx->vcd_handle,
			&vcd_property_hdr, &control);
			if (vcd_status) {
				ERR("%s(): Set VCD_I_RATE_CONTROL Failed\n",
						__func__);
				status = false;
			}
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
		&vcd_property_hdr, &control);

		if (vcd_status) {
			ERR("%s(): Get VCD_I_RATE_CONTROL Failed\n",
					__func__);
			status = false;
		} else {
			switch (control.rate_control) {
			case VCD_RATE_CONTROL_OFF:
				ratectrlcfg->rcmode = VEN_RC_OFF;
				break;
			case VCD_RATE_CONTROL_CBR_VFR:
				ratectrlcfg->rcmode = VEN_RC_CBR_VFR;
				break;
			case VCD_RATE_CONTROL_VBR_CFR:
				ratectrlcfg->rcmode = VEN_RC_VBR_CFR;
				break;
			case VCD_RATE_CONTROL_VBR_VFR:
				ratectrlcfg->rcmode = VEN_RC_VBR_VFR;
				break;
			case VCD_RATE_CONTROL_CBR_CFR:
				ratectrlcfg->rcmode = VEN_RC_CBR_CFR;
				break;
			default:
				status = false;
				break;
			}
		}
	}
	return status;
}

u32 vid_enc_set_get_voptimingcfg(struct video_client_ctx *client_ctx,
		struct	venc_voptimingcfg *voptimingcfg, u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_vop_timing control;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 status = true;

	if (!client_ctx || !voptimingcfg)
		return false;

	vcd_property_hdr.prop_id = VCD_I_VOP_TIMING;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_vop_timing);

	if (set_flag) {
		control.vop_time_resolution =
		voptimingcfg->voptime_resolution;
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
		&vcd_property_hdr, &control);

		if (vcd_status) {
			ERR("%s(): Set VCD_I_VOP_TIMING Failed\n",
					__func__);
			status = false;
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
		&vcd_property_hdr, &control);
		if (vcd_status) {
			ERR("%s(): Get VCD_I_VOP_TIMING Failed\n",
					__func__);
			status = false;
		} else
			voptimingcfg->voptime_resolution =
			control.vop_time_resolution;
	}
	return status;
}

u32 vid_enc_set_get_headerextension(struct video_client_ctx *client_ctx,
		struct venc_headerextension *headerextension, u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	u32 control;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 status = true;

	if (!client_ctx || !headerextension)
		return false;

	vcd_property_hdr.prop_id = VCD_I_HEADER_EXTENSION;
	vcd_property_hdr.sz = sizeof(u32);

	if (set_flag) {
		control = headerextension->header_extension;
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
		&vcd_property_hdr, &control);
		if (vcd_status) {
			ERR("%s(): Set VCD_I_HEADER_EXTENSION Failed\n",
					__func__);
			status = false;
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
		&vcd_property_hdr, &control);
		if (vcd_status) {
			ERR("%s(): Get VCD_I_HEADER_EXTENSION Failed\n",
					__func__);
			status = false;
		} else {
			headerextension->header_extension = control;
		}
	}
	return status;
}

u32 vid_enc_set_get_qprange(struct video_client_ctx *client_ctx,
		struct venc_qprange *qprange, u32 set_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_qp_range control;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 status = true;

	if (!client_ctx || !qprange)
		return false;

	vcd_property_hdr.prop_id = VCD_I_QP_RANGE;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_qp_range);

	if (set_flag) {
		control.max_qp = qprange->maxqp;
		control.min_qp = qprange->minqp;
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
		&vcd_property_hdr, &control);

		if (vcd_status) {
			ERR("%s(): Set VCD_I_QP_RANGE Failed\n",
					__func__);
			status = false;
		}
	} else {
		vcd_status = vcd_get_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &control);
		if (vcd_status) {
			ERR("%s(): Get VCD_I_QP_RANGE Failed\n",
					__func__);
			status = false;
		} else {
			qprange->maxqp = control.max_qp;
			qprange->minqp = control.min_qp;
		}
	}
	return status;
}

u32 vid_enc_start_stop(struct video_client_ctx *client_ctx, u32 start)
{
	u32 vcd_status;

	if (!client_ctx)
		return false;

	if (start) {
			vcd_status = vcd_encode_start(client_ctx->vcd_handle);

			if (vcd_status) {
				ERR("%s(): vcd_encode_start failed."
				" vcd_status = %u\n", __func__, vcd_status);
				return false;
			}
	} else {
		vcd_status = vcd_stop(client_ctx->vcd_handle);
		if (vcd_status) {
			ERR("%s(): vcd_stop failed.  vcd_status = %u\n",
		__func__, vcd_status);
			return false;
		}
		DBG("Send STOP_DONE message to client = %p\n",
				client_ctx);
	}
	return true;
}

u32 vid_enc_pause_resume(struct video_client_ctx *client_ctx, u32 pause)
{
	u32 vcd_status;

	if (!client_ctx)
		return false;

	if (pause) {
		DBG("PAUSE command from client = %p\n",
				client_ctx);
		vcd_status = vcd_pause(client_ctx->vcd_handle);
	} else {
		DBG("Resume command from client = %p\n",
				client_ctx);
		vcd_status = vcd_resume(client_ctx->vcd_handle);
	}

	if (vcd_status)
		return false;

	return true;
}

u32 vid_enc_flush(struct video_client_ctx *client_ctx,
		struct venc_bufferflush *bufferflush)
{
	u32 status = true, mode, vcd_status;

	if (!client_ctx || !bufferflush)
		return false;

	switch (bufferflush->flush_mode) {
	case VEN_FLUSH_INPUT:
		mode = VCD_FLUSH_INPUT;
		break;
	case VEN_FLUSH_OUTPUT:
		mode = VCD_FLUSH_OUTPUT;
		break;
	case VEN_FLUSH_ALL:
		mode = VCD_FLUSH_ALL;
		break;
	default:
		status = false;
		break;
	}
	if (status) {
		vcd_status = vcd_flush(client_ctx->vcd_handle, mode);
		if (vcd_status)
			status = false;
	}
	return status;
}

u32 vid_enc_get_buffer_req(struct video_client_ctx *client_ctx,
		struct venc_allocatorproperty *venc_buf_req, u32 input_dir)
{
	enum vcd_buffer_type buffer;
	struct vcd_buffer_requirement buffer_req;
	u32 status = true;
	u32 vcd_status;

	if (!client_ctx || !venc_buf_req)
		return false;

	buffer = VCD_BUFFER_OUTPUT;
	if (input_dir)
		buffer = VCD_BUFFER_INPUT;

	vcd_status = vcd_get_buffer_requirements(client_ctx->vcd_handle,
							buffer, &buffer_req);

	if (vcd_status)
		status = false;

	if (status) {
		venc_buf_req->actualcount = buffer_req.actual_count;
		venc_buf_req->alignment = buffer_req.align;
		venc_buf_req->datasize = buffer_req.sz;
		venc_buf_req->mincount = buffer_req.min_count;
		venc_buf_req->maxcount = buffer_req.max_count;
		venc_buf_req->alignment = buffer_req.align;
		venc_buf_req->bufpoolid = buffer_req.buf_pool_id;
		venc_buf_req->suffixsize = 0;
	}
	return status;
}

u32 vid_enc_set_buffer_req(struct video_client_ctx *client_ctx,
		struct venc_allocatorproperty *venc_buf_req, u32 input_dir)
{
	enum vcd_buffer_type buffer;
	struct vcd_buffer_requirement buffer_req;
	u32 status = true;
	u32 vcd_status;

	if (!client_ctx || !venc_buf_req)
		return false;

	buffer = VCD_BUFFER_OUTPUT;
	if (input_dir)
		buffer = VCD_BUFFER_INPUT;

	buffer_req.actual_count = venc_buf_req->actualcount;
	buffer_req.align = venc_buf_req->alignment;
	buffer_req.sz = venc_buf_req->datasize;
	buffer_req.min_count = venc_buf_req->mincount;
	buffer_req.max_count = venc_buf_req->maxcount;
	buffer_req.align = venc_buf_req->alignment;
	buffer_req.buf_pool_id = 0;

	vcd_status = vcd_set_buffer_requirements(client_ctx->vcd_handle,
				buffer, &buffer_req);

	if (vcd_status)
		status = false;
	return status;
}

u32 vid_enc_set_buffer(struct video_client_ctx *client_ctx,
		struct venc_bufferpayload *buffer_info,
		enum venc_buffer_dir buffer)
{
	enum vcd_buffer_type vcd_buffer_t = VCD_BUFFER_INPUT;
	enum buffer_dir dir_buffer = BUFFER_TYPE_INPUT;
	u32 vcd_status = VCD_ERR_FAIL;
	unsigned long kernel_vaddr, length = 0;

	if (!client_ctx || !buffer_info)
		return false;

	if (buffer == VEN_BUFFER_TYPE_OUTPUT) {
		dir_buffer = BUFFER_TYPE_OUTPUT;
		vcd_buffer_t = VCD_BUFFER_OUTPUT;
	}
	length = buffer_info->sz;
	/*If buffer cannot be set, ignore */
	if (!vidc_insert_addr_table(client_ctx, dir_buffer,
					(unsigned long)buffer_info->pbuffer,
					&kernel_vaddr,
					buffer_info->fd,
					(unsigned long)buffer_info->offset,
					VID_ENC_MAX_NUM_OF_BUFF, length)) {
		DBG("%s() : user_virt_addr = %p cannot be set.",
		    __func__, buffer_info->pbuffer);
		return false;
	}

	vcd_status = vcd_set_buffer(client_ctx->vcd_handle,
				    vcd_buffer_t, (u8 *) kernel_vaddr,
				    buffer_info->sz);

	if (!vcd_status)
		return true;
	else
		return false;
}

u32 vid_enc_free_buffer(struct video_client_ctx *client_ctx,
		struct venc_bufferpayload *buffer_info,
		enum venc_buffer_dir buffer)
{
	enum vcd_buffer_type buffer_vcd = VCD_BUFFER_INPUT;
	enum buffer_dir dir_buffer = BUFFER_TYPE_INPUT;
	u32 vcd_status = VCD_ERR_FAIL;
	unsigned long kernel_vaddr;

	if (!client_ctx || !buffer_info)
		return false;

	if (buffer == VEN_BUFFER_TYPE_OUTPUT) {
		dir_buffer = BUFFER_TYPE_OUTPUT;
		buffer_vcd = VCD_BUFFER_OUTPUT;
	}
	/*If buffer NOT set, ignore */
	if (!vidc_delete_addr_table(client_ctx, dir_buffer,
				(unsigned long)buffer_info->pbuffer,
				&kernel_vaddr)) {
		DBG("%s() : user_virt_addr = %p has not been set.",
		    __func__, buffer_info->pbuffer);
		return true;
	}

	vcd_status = vcd_free_buffer(client_ctx->vcd_handle, buffer_vcd,
					 (u8 *)kernel_vaddr);

	if (!vcd_status)
		return true;
	else
		return false;
}

u32 vid_enc_encode_frame(struct video_client_ctx *client_ctx,
		struct venc_buffer *input_frame_info)
{
	struct vcd_frame_data vcd_input_buffer;
	unsigned long kernel_vaddr, phy_addr, user_vaddr;
	int pmem_fd;
	struct file *file;
	s32 buffer_index = -1;

	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !input_frame_info)
		return false;

	user_vaddr = (unsigned long)input_frame_info->ptrbuffer;

	if (vidc_lookup_addr_table(client_ctx, BUFFER_TYPE_INPUT,
			true, &user_vaddr, &kernel_vaddr,
			&phy_addr, &pmem_fd, &file,
			&buffer_index)) {

		/* kernel_vaddr  is found. send the frame to VCD */
		memset((void *)&vcd_input_buffer, 0,
					sizeof(struct vcd_frame_data));

		vcd_input_buffer.virtual =
		(u8 *) (kernel_vaddr + input_frame_info->offset);

		vcd_input_buffer.offset = input_frame_info->offset;
		vcd_input_buffer.frm_clnt_data =
				(u32) input_frame_info->clientdata;
		vcd_input_buffer.ip_frm_tag =
				(u32) input_frame_info->clientdata;
		vcd_input_buffer.data_len = input_frame_info->len;
		vcd_input_buffer.time_stamp = input_frame_info->timestamp;

		/* Rely on VCD using the same flags as OMX */
		vcd_input_buffer.flags = input_frame_info->flags;

		vcd_status = vcd_encode_frame(client_ctx->vcd_handle,
		&vcd_input_buffer);
		if (!vcd_status)
			return true;
		else {
			ERR("%s(): vcd_encode_frame failed = %u\n",
			__func__, vcd_status);
			return false;
		}

	} else {
		ERR("%s(): kernel_vaddr not found\n",
				__func__);
		return false;
	}
}

u32 vid_enc_fill_output_buffer(struct video_client_ctx *client_ctx,
		struct venc_buffer *output_frame_info)
{
	unsigned long kernel_vaddr, phy_addr, user_vaddr;
	int pmem_fd;
	struct file *file;
	s32 buffer_index = -1;
	u32 vcd_status = VCD_ERR_FAIL;

	struct vcd_frame_data vcd_frame;

	if (!client_ctx || !output_frame_info)
		return false;

	user_vaddr = (unsigned long)output_frame_info->ptrbuffer;

	if (vidc_lookup_addr_table(client_ctx, BUFFER_TYPE_OUTPUT,
			true, &user_vaddr, &kernel_vaddr,
			&phy_addr, &pmem_fd, &file,
			&buffer_index)) {

		memset((void *)&vcd_frame, 0,
					 sizeof(struct vcd_frame_data));
		vcd_frame.virtual = (u8 *) kernel_vaddr;
		vcd_frame.frm_clnt_data = (u32) output_frame_info->clientdata;
		vcd_frame.alloc_len = output_frame_info->sz;

		vcd_status = vcd_fill_output_buffer(client_ctx->vcd_handle,
								&vcd_frame);
		if (!vcd_status)
			return true;
		else {
			ERR("%s(): vcd_fill_output_buffer failed = %u\n",
					__func__, vcd_status);
			return false;
		}
	} else {
		ERR("%s(): kernel_vaddr not found\n", __func__);
		return false;
	}
}
u32 vid_enc_set_recon_buffers(struct video_client_ctx *client_ctx,
		struct venc_recon_addr *venc_recon)
{
	u32 vcd_status = VCD_ERR_FAIL;
	u32 len, i, flags = 0;
	struct file *file;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_enc_recon_buffer *control = NULL;
	struct msm_mapped_buffer *mapped_buffer = NULL;
	if (!client_ctx || !venc_recon) {
		pr_err("%s() Invalid params", __func__);
		return false;
	}
	len = sizeof(client_ctx->recon_buffer)/
		sizeof(struct vcd_property_enc_recon_buffer);
	for (i = 0; i < len; i++) {
		if (!client_ctx->recon_buffer[i].kernel_virtual_addr) {
			control = &client_ctx->recon_buffer[i];
			break;
		}
	}
	if (!control) {
		pr_err("Exceeded max recon buffer setting");
		return false;
	}
	control->buffer_size = venc_recon->buffer_size;
	control->kernel_virtual_addr = NULL;
	control->physical_addr = NULL;
	control->pmem_fd = venc_recon->pmem_fd;
	control->offset = venc_recon->offset;
	control->user_virtual_addr = venc_recon->pbuffer;
	if (get_pmem_file(control->pmem_fd, (unsigned long *)
		(&(control->physical_addr)), (unsigned long *)
		(&control->kernel_virtual_addr),
		(unsigned long *) (&len), &file)) {
			ERR("%s(): get_pmem_file failed\n", __func__);
			return false;
		}
		put_pmem_file(file);
		flags = MSM_SUBSYSTEM_MAP_IOVA;
		mapped_buffer = msm_subsystem_map_buffer(
		(unsigned long)control->physical_addr, len,
		flags, vidc_mmu_subsystem,
		sizeof(vidc_mmu_subsystem)/sizeof(unsigned int));
		if (IS_ERR(mapped_buffer)) {
			pr_err("buffer map failed");
			return false;
		}
		control->client_data = (void *) mapped_buffer;
		control->dev_addr = (u8 *)mapped_buffer->iova[0];
		vcd_property_hdr.prop_id = VCD_I_RECON_BUFFERS;
		vcd_property_hdr.sz =
			sizeof(struct vcd_property_enc_recon_buffer);

		vcd_status = vcd_set_property(client_ctx->vcd_handle,
						&vcd_property_hdr, control);
		if (!vcd_status) {
			DBG("vcd_set_property returned success\n");
			return true;
		} else {
			ERR("%s(): vid_enc_set_recon_buffers failed = %u\n",
					__func__, vcd_status);
			return false;
		}
}

u32 vid_enc_free_recon_buffers(struct video_client_ctx *client_ctx,
			struct venc_recon_addr *venc_recon)
{
	u32 vcd_status = VCD_ERR_FAIL;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_enc_recon_buffer *control = NULL;
	u32 len = 0, i;

	if (!client_ctx || !venc_recon) {
		pr_err("%s() Invalid params", __func__);
		return false;
	}
	len = sizeof(client_ctx->recon_buffer)/
		sizeof(struct vcd_property_enc_recon_buffer);
	pr_err(" %s() address  %p", __func__,
	venc_recon->pbuffer);
	for (i = 0; i < len; i++) {
		if (client_ctx->recon_buffer[i].user_virtual_addr
			== venc_recon->pbuffer) {
			control = &client_ctx->recon_buffer[i];
			break;
		}
	}
	if (!control) {
		pr_err(" %s() address not found %p", __func__,
			venc_recon->pbuffer);
		return false;
	}
	if (control->client_data)
		msm_subsystem_unmap_buffer((struct msm_mapped_buffer *)
		control->client_data);

	vcd_property_hdr.prop_id = VCD_I_FREE_RECON_BUFFERS;
	vcd_property_hdr.sz = sizeof(struct vcd_property_buffer_size);
	vcd_status = vcd_set_property(client_ctx->vcd_handle,
						&vcd_property_hdr, control);
	memset(control, 0, sizeof(struct vcd_property_enc_recon_buffer));
	return true;
}

u32 vid_enc_get_recon_buffer_size(struct video_client_ctx *client_ctx,
		struct venc_recon_buff_size *venc_recon_size)
{
	u32 vcd_status = VCD_ERR_FAIL;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_buffer_size control;

	control.width = venc_recon_size->width;
	control.height = venc_recon_size->height;

	vcd_property_hdr.prop_id = VCD_I_GET_RECON_BUFFER_SIZE;
	vcd_property_hdr.sz = sizeof(struct vcd_property_buffer_size);

	vcd_status = vcd_get_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &control);

	venc_recon_size->width = control.width;
	venc_recon_size->height = control.height;
	venc_recon_size->size = control.size;
	venc_recon_size->alignment = control.alignment;
	DBG("W: %d, H: %d, S: %d, A: %d", venc_recon_size->width,
			venc_recon_size->height, venc_recon_size->size,
			venc_recon_size->alignment);

	if (!vcd_status) {
		DBG("vcd_set_property returned success\n");
		return true;
		} else {
			ERR("%s(): vid_enc_get_recon_buffer_size failed = %u\n",
				__func__, vcd_status);
			return false;
		}
}
