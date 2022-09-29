// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Cheng-Jung Ho <cheng-jung.ho@mediatek.com>
 */


#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/types.h>
#include <linux/list.h>
#if !IS_ENABLED(CONFIG_64BIT)
#include <asm/div64.h>
#endif
#include <linux/slab.h>
#include "vcodec_dvfs.h"
#include <linux/time.h>
#include <linux/ktime.h>
#include "mtk_vcodec_drv.h"
#include "vcp_feature_define.h"

struct vcodec_inst *get_inst(struct mtk_vcodec_ctx *ctx)
{
	int codec_type = ctx->type;
	struct mtk_vcodec_dev *dev = ctx->dev;
	struct list_head *item;
	struct vcodec_inst *inst;

	if (codec_type == MTK_INST_DECODER) {
		list_for_each(item, &dev->vdec_dvfs_inst) {
			inst = list_entry(item, struct vcodec_inst, list);
			if (inst->id == ctx->id)
				return inst;
		}

	} else if (codec_type == MTK_INST_ENCODER) {
		list_for_each(item, &dev->venc_dvfs_inst) {
			inst = list_entry(item, struct vcodec_inst, list);
			if (inst->id == ctx->id)
				return inst;
		}

	} else {
		mtk_v4l2_debug(0, "[VDVFS] no valid codec type");
		return 0;
	}

	return 0;
}

int get_cfg(struct vcodec_inst *inst, struct mtk_vcodec_ctx *ctx)
{
	struct mtk_vcodec_dev *dev;
	u32 mb_per_sec;
	int i;

	if (inst->op_rate <= 0)
		mb_per_sec = inst->width * inst->height / 256 * 30;
	else
		mb_per_sec = inst->width * inst->height / 256 * inst->op_rate;

	dev = ctx->dev;

	if (!dev->venc_cfg_cnt)
		return -1;

	for (i = 0; i < dev->venc_cfg_cnt; i++) {
		mtk_v4l2_debug(8, "[VDVFS] config %d, fmt %u, mb_thresh %u, t_fmt %u, t_mb %u",
				i, dev->venc_cfg[i].codec_fmt, dev->venc_cfg[i].mb_thresh,
				inst->codec_fmt, mb_per_sec);
		if (dev->venc_cfg[i].codec_fmt == inst->codec_fmt &&
			mb_per_sec <= dev->venc_cfg[i].mb_thresh) {
			inst->config = ctx->enc_params.highquality ?
					dev->venc_cfg[i].config_2 :
					dev->venc_cfg[i].config_1;
			mtk_v4l2_debug(6, "[VDVFS] found config %d %d %u = %d",
				dev->venc_cfg[i].config_1, dev->venc_cfg[i].config_2,
				ctx->enc_params.highquality, inst->config);
			return 0;
		}
	}

	return -1;
}

int add_inst(struct mtk_vcodec_ctx *ctx)
{
	struct vcodec_inst *new_inst;
	struct mtk_vcodec_dev *dev = ctx->dev;
	int ret;

	new_inst = vzalloc(sizeof(struct vcodec_inst));
	if (!new_inst) {
		/* mtk_v4l2_debug(0, "[VDVFS] allocate inst mem failed"); */
		return -1;
	}

	new_inst->id = ctx->id;
	new_inst->codec_type = ctx->type;
	new_inst->codec_fmt = (new_inst->codec_type == MTK_INST_ENCODER) ?
		ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc : ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
	new_inst->yuv_fmt = (new_inst->codec_type == MTK_INST_ENCODER) ?
		ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc : ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc;
	new_inst->config = DEFAULT_VENC_CONFIG; /* Read from register when encode done */
	new_inst->core_cnt = 2; /* Depends on WP */
	new_inst->b_frame = (new_inst->codec_type == MTK_INST_ENCODER) ?
		ctx->enc_params.num_b_frame : 0;
	new_inst->wp = (new_inst->codec_type == MTK_INST_ENCODER) ?
		ctx->enc_params.scenario : 0;
	if (new_inst->wp == WP_SCENARIO)
		new_inst->core_cnt = 1;
	new_inst->op_rate = (new_inst->codec_type == MTK_INST_ENCODER) ?
		ctx->enc_params.operationrate : ctx->dec_params.operating_rate;
	new_inst->priority = (new_inst->codec_type == MTK_INST_ENCODER) ?
				ctx->enc_params.priority : ctx->dec_params.priority;
	if (new_inst->op_rate == 0) {
		new_inst->op_rate = (new_inst->codec_type == MTK_INST_ENCODER) ?
			(ctx->enc_params.framerate_denom == 0 ? 0 :
			(ctx->enc_params.framerate_num / ctx->enc_params.framerate_denom)) :
			0; /* TODO: Update with decoder frame rate */
	}
	new_inst->width = (new_inst->codec_type == MTK_INST_ENCODER) ?
		ctx->q_data[MTK_Q_DATA_SRC].visible_width :
		ctx->q_data[MTK_Q_DATA_DST].coded_width;
	new_inst->height = (new_inst->codec_type == MTK_INST_ENCODER) ?
		ctx->q_data[MTK_Q_DATA_SRC].visible_height :
		ctx->q_data[MTK_Q_DATA_DST].coded_height;
	new_inst->last_access = ktime_get_boottime_ns();
	new_inst->is_active = 1;
	new_inst->ctx = ctx;

	/* Calculate config */
	if (new_inst->codec_type == MTK_INST_ENCODER) {
		ret = get_cfg(new_inst, ctx);
		if (ret != 0)
			mtk_v4l2_debug(0, "[VDVFS] VENC no config");
	}

	mtk_v4l2_debug(4, "[VDVFS] New inst id %d, type %u, fmt %u, cfg %d, ccnt %u, op_rate %d, priority %d",
			new_inst->id, new_inst->codec_type, new_inst->codec_fmt, new_inst->config,
			new_inst->core_cnt, new_inst->op_rate, new_inst->priority);
	mtk_v4l2_debug(4, "[VDVFS] width %u, height %u, is_encoder %d",
			new_inst->width, new_inst->height,
			new_inst->codec_type == MTK_INST_ENCODER);

	if (new_inst->codec_type == MTK_INST_ENCODER)
		list_add_tail(&new_inst->list, &dev->venc_dvfs_inst);
	else if (new_inst->codec_type == MTK_INST_DECODER)
		list_add_tail(&new_inst->list, &dev->vdec_dvfs_inst);

	return 0;
}

bool setting_changed(struct vcodec_inst *inst, struct mtk_vcodec_ctx *ctx)
{
	s32 op_rate;
	int ret;

	if (inst->codec_type == MTK_INST_DECODER) {
		if (inst->width != ctx->q_data[MTK_Q_DATA_DST].coded_width ||
			inst->height != ctx->q_data[MTK_Q_DATA_DST].coded_height ||
			inst->op_rate != ctx->dec_params.operating_rate) {
			inst->width = ctx->q_data[MTK_Q_DATA_DST].coded_width;
			inst->height = ctx->q_data[MTK_Q_DATA_DST].coded_height;
			inst->op_rate = ctx->dec_params.operating_rate;
			return true;
		}
	} else if (inst->codec_type == MTK_INST_ENCODER) {
		op_rate = ctx->enc_params.operationrate;
		if (op_rate == 0) {
			op_rate = ctx->enc_params.framerate_denom == 0 ? 0 :
				(ctx->enc_params.framerate_num /
				ctx->enc_params.framerate_denom);
		}

		if (inst->width != ctx->q_data[MTK_Q_DATA_SRC].visible_width ||
			inst->height != ctx->q_data[MTK_Q_DATA_SRC].visible_height ||
			inst->op_rate != op_rate) {
			inst->width = ctx->q_data[MTK_Q_DATA_SRC].visible_width;
			inst->height = ctx->q_data[MTK_Q_DATA_SRC].visible_height;
			inst->op_rate = op_rate;
			ret = get_cfg(inst, ctx);
			if (ret != 0)
				mtk_v4l2_debug(0, "[VDVFS] VENC no config");

			return true;
		}
	}
	return false;
}

bool need_update(struct mtk_vcodec_ctx *ctx)
{
	struct vcodec_inst *inst = 0;

	if (!ctx)
		return false;

	inst = get_inst(ctx);
	if (!inst) {
		add_inst(ctx);
		return true;
	}

	inst->last_access = ktime_get_boottime_ns();

	if (inst->codec_type == MTK_INST_ENCODER &&
		inst->config == DEFAULT_VENC_CONFIG)
		return true;

	if (setting_changed(inst, ctx))
		return true;

	return false;
}

bool remove_update(struct mtk_vcodec_ctx *ctx)
{
	struct vcodec_inst *inst = 0;

	if (!ctx)
		return false;

	inst = get_inst(ctx);
	if (!inst)
		return false;

	list_del(&inst->list);
	vfree(inst);

	return true;
}

u32 find_dflt_op_rate(struct vcodec_inst *inst, struct mtk_vcodec_dev *dev)
{
	const u32 dflt_op_rate = 30u;
	int i, j;
	u32 pixel_cnt, op_rate;

	op_rate = dflt_op_rate;
	if (inst->codec_type == MTK_INST_DECODER) {
		for (i = 0 ; i < dev->vdec_op_rate_cnt; i++) {
			if (!dev->vdec_dflt_op_rate) {
				mtk_v4l2_debug(0, "[VDVFS] no vdec_dflt_op_rate");
				return dflt_op_rate;
			}

			mtk_v4l2_debug(8, "[VDVFS] %d ,fmt %u, perf fmt %u",
				i, inst->codec_fmt, dev->vdec_dflt_op_rate[i].codec_fmt);

			if (inst->codec_fmt == dev->vdec_dflt_op_rate[i].codec_fmt) {
				pixel_cnt = inst->width * inst->height;

				for (j = 0; j < MAX_OP_CNT; j++) {
					mtk_v4l2_debug(8, "[VDVFS] %d pix table %u op %u pix %u",
						j, dev->vdec_dflt_op_rate[i].pixel_per_frame[j],
						dev->vdec_dflt_op_rate[i].max_op_rate[j],
						pixel_cnt);
					if (dev->vdec_dflt_op_rate[i].pixel_per_frame[j] >=
						pixel_cnt) {
						op_rate = dev->vdec_dflt_op_rate[i].max_op_rate[j];
						mtk_v4l2_debug(8, "[VDVFS] %s set oprate %u",
							__func__, op_rate);
						break;
					}
				}
				return op_rate;
			}
		}
		mtk_v4l2_debug(0, "[VDVFS] VDEC %u found no default op rate", inst->codec_fmt);
	} else if (inst->codec_type == MTK_INST_ENCODER) {
		mtk_v4l2_debug(0, "[VDVFS] VENC should be given operating rate");
		return dflt_op_rate;
	}
	return dflt_op_rate;
}


struct vcodec_perf *find_perf(struct vcodec_inst *inst, struct mtk_vcodec_dev *dev)
{
	int i = 0;

	if (inst->codec_type == MTK_INST_DECODER) {
		for (i = 0 ; i < dev->vdec_tput_cnt; i++) {
			mtk_v4l2_debug(8, "[VDVFS] %d ,fmt %u, perf fmt %u",
				i, inst->codec_fmt, dev->vdec_tput[i].codec_fmt);

			if (inst->codec_fmt == dev->vdec_tput[i].codec_fmt)
				return &dev->vdec_tput[i];
		}
		mtk_v4l2_debug(0, "[VDVFS] VDEC %u found no throughput", inst->codec_fmt);

	} else if (inst->codec_type == MTK_INST_ENCODER) {
		for (i = 0 ; i < dev->venc_tput_cnt; i++) {
			mtk_v4l2_debug(8, "[VDVFS] %d ,fmt %u, perf fmt %u",
				i, inst->codec_fmt, dev->venc_tput[i].codec_fmt);

			if (inst->codec_fmt == dev->venc_tput[i].codec_fmt &&
				inst->config == dev->venc_tput[i].config)
				return &dev->venc_tput[i];
		}
		mtk_v4l2_debug(0, "[VDVFS] VENC %u cfg %d found no throughput",
			inst->codec_fmt, inst->config);
	}
	return 0;
}

u32 match_avail_freq(struct mtk_vcodec_dev *dev, int codec_type, u64 freq)
{
	int i;
	u32 match_freq = 0;

	if (codec_type == MTK_INST_DECODER) {
		match_freq = dev->vdec_freqs[0];

		for (i = 0; i < MAX_CODEC_FREQ_STEP-1; i++) {
			mtk_v4l2_debug(8, "[VDVFS] VDEC i %d, freq %u, in_freq %llu",
				i, dev->vdec_freqs[i], freq);

			if (dev->vdec_freqs[i] < freq)
				match_freq = dev->vdec_freqs[i];
			else if (dev->vdec_freqs[i] >= freq) {
				match_freq = dev->vdec_freqs[i];
				break;
			}

			if (dev->vdec_freqs[i+1] == 0)
				break;
		}

		mtk_v4l2_debug(6, "[VDVFS] VDEC match_freq %u", match_freq);

	} else if (codec_type == MTK_INST_ENCODER) {
		match_freq = dev->venc_freqs[0];

		for (i = 0; i < MAX_CODEC_FREQ_STEP-1; i++) {
			mtk_v4l2_debug(8, "[VDVFS] VENC i %d, freq %u, in_freq %llu",
				i, dev->venc_freqs[i], freq);

			if (dev->venc_freqs[i] < freq)
				match_freq = dev->venc_freqs[i];
			else if (dev->venc_freqs[i] >= freq) {
				match_freq = dev->venc_freqs[i];
				break;
			}

			if (dev->venc_freqs[i+1] == 0)
				break;
		}

		mtk_v4l2_debug(6, "[VDVFS] VENC match_freq %u",  match_freq);
	}

	return match_freq;
}

/**
 * Calculate single instance freqency.
 * inst: target instance
 * dev: device
 * Return: hz
 */
u64 calc_freq(struct vcodec_inst *inst, struct mtk_vcodec_dev *dev)
{
	struct vcodec_perf *perf;
	u32 dflt_op_rate;
	u64 freq = 0;

	perf = find_perf(inst, dev);
	if (inst->codec_type == MTK_INST_DECODER) {
		if (perf != 0) {
			freq = (u64)inst->width * inst->height / 256 * inst->op_rate *
				perf->cy_per_mb_1;

			mtk_v4l2_debug(6, "[VDVFS] VDEC w:%u x h:%u / 256 x oprate: %d x mb %u",
				inst->width, inst->height, inst->op_rate, perf->cy_per_mb_1);
		} else
			freq = 100000000;
		/* AV1 boost for 720P180 test */
		if (((inst->priority > 0 && inst->op_rate <= 0) || inst->op_rate >= 135) &&
		perf != 0 && inst->codec_fmt == 808539713 &&
		(inst->width * inst->height <= 1280 * 736)) {

			if (inst->priority > 0)
				inst->op_rate = 3000;
			else
				inst->op_rate = 2500;
			mtk_v4l2_debug(0, "[VDVFS] VDEC w:%u x h:%u priority %d, new oprate %u",
				inst->width, inst->height, inst->priority, inst->op_rate);

			freq = (u64)inst->width * inst->height / 256 * inst->op_rate *
				perf->cy_per_mb_1;

			mtk_v4l2_debug(0, "[VDVFS] VDEC priority:%d oprate:%d, set freq = %llu",
					inst->priority, inst->op_rate, freq);

		} else if (perf != 0 && inst->op_rate <= 0) {
			/* Undefined priority + op_rate combination & max op rate behavior */
			dflt_op_rate = find_dflt_op_rate(inst, dev);

			if (inst->priority < 0) {
				inst->op_rate = 30;
				if (inst->codec_fmt == 808996950) {
					/* performance class WA for VP8 */
					inst->op_rate = 60;
				} else if (
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
					feature_table[VENC_FEATURE_ID].enable > 0 &&
#endif
					inst->codec_fmt == 875967048 && dev->dec_cnt > 1 &&
					(inst->width * inst->height <= 1920 * 1088)) {
					inst->op_rate = 174;
				}
			} else
				inst->op_rate = dflt_op_rate;

			mtk_v4l2_debug(6, "[VDVFS] VDEC w:%u x h:%u priority %d, new oprate %u",
				inst->width, inst->height, inst->priority, inst->op_rate);

			freq = (u64)inst->width * inst->height / 256 * inst->op_rate *
				perf->cy_per_mb_1;

			mtk_v4l2_debug(6, "[VDVFS] VDEC priority:%d oprate:%d, set freq = %llu",
					inst->priority, inst->op_rate, freq);
		}
	} else if (inst->codec_type == MTK_INST_ENCODER) {
		if (perf != 0) {
			freq = (u64)inst->width * inst->height / 256 * inst->op_rate;
			if (inst->b_frame == 0)
				freq = freq * perf->cy_per_mb_1;
			else
				freq = freq * perf->cy_per_mb_2;

			freq = freq / inst->core_cnt;
			/* SW overhead */
			if (inst->width * inst->height <= 1920 * 1088)
				freq = freq / 10 * 11;
			else if (inst->width * inst->height <= 1280 * 736)
				freq = freq / 10 * 12;

			mtk_v4l2_debug(6, "[VDVFS] VENC w:%u x h:%u / 256 x oprate: %d x mb %u",
				inst->width, inst->height, inst->op_rate,
				inst->b_frame == 0 ? perf->cy_per_mb_1 : perf->cy_per_mb_2);
		} else
			freq = 100000000;

		if (inst->op_rate <= 0) {
			freq = (u64)dev->venc_dvfs_params.normal_max_freq;
			mtk_v4l2_debug(6, "[VDVFS] VENC oprate: %d, set freq = %llu",
					inst->op_rate, freq);
		}
	}

	mtk_v4l2_debug(6, "[VDVFS] freq = %llu", freq);
	return freq;
}

bool mtk_vcodec_is_10Bit(u32 cur_codec_fmt, int codec_type)
{
	if (codec_type == MTK_INST_DECODER) {
		/* Todo: add decoder */
	} else if (codec_type == MTK_INST_ENCODER) {
		if (cur_codec_fmt == V4L2_PIX_FMT_RGBA1010102 ||
		    cur_codec_fmt == V4L2_PIX_FMT_BGRA1010102 ||
		    cur_codec_fmt == V4L2_PIX_FMT_ARGB1010102 ||
		    cur_codec_fmt == V4L2_PIX_FMT_ABGR1010102 ||
		    cur_codec_fmt == V4L2_PIX_FMT_RGBA1010102_AFBC ||
		    cur_codec_fmt == V4L2_PIX_FMT_BGRA1010102_AFBC ||
		    cur_codec_fmt == V4L2_PIX_FMT_MT10 || cur_codec_fmt == V4L2_PIX_FMT_MT10S ||
		    cur_codec_fmt == V4L2_PIX_FMT_P010S || cur_codec_fmt == V4L2_PIX_FMT_P010M ||
		    cur_codec_fmt == V4L2_PIX_FMT_NV12_10B_AFBC) {
			mtk_v4l2_debug(8, "[VDVFS] venc ctx is 10bit");
			return true;
		}
	}
	return false;
}

bool mtk_vcodec_is_afbc(u32 cur_codec_fmt, int codec_type)
{
	if (codec_type == MTK_INST_DECODER) {
		/* Todo: add decoder */
	} else if (codec_type == MTK_INST_ENCODER) {
		if (cur_codec_fmt == V4L2_PIX_FMT_RGB32_AFBC ||
		    cur_codec_fmt == V4L2_PIX_FMT_BGR32_AFBC ||
		    cur_codec_fmt == V4L2_PIX_FMT_RGBA1010102_AFBC ||
		    cur_codec_fmt == V4L2_PIX_FMT_BGRA1010102_AFBC ||
		    cur_codec_fmt == V4L2_PIX_FMT_NV12_AFBC ||
		    cur_codec_fmt == V4L2_PIX_FMT_NV21_AFBC ||
		    cur_codec_fmt == V4L2_PIX_FMT_NV12_10B_AFBC) {
			mtk_v4l2_debug(8, "[VDVFS] venc ctx is afbc");
			return true;
		}
	}
	return false;
}

u32 mtk_vcodec_get_bw_factor(struct mtk_vcodec_dev *dev, int codec_type)
{
	int i;
	struct list_head *item;
	struct vcodec_inst *inst;
	u32 bw_factor_bit, bw_factor_afbc;
	u32 inst_bw_factor, target_bw_factor = 0;
	u32 freq_sum = dev->venc_dvfs_params.target_freq;

	if (codec_type == MTK_INST_DECODER) {
		/*Todo: decoder use bw factor*/
		return 0;
	} else if (codec_type == MTK_INST_ENCODER) {
		/*get encoder bw factor*/
		list_for_each(item, &dev->venc_dvfs_inst) {
			inst = list_entry(item, struct vcodec_inst, list);
			bw_factor_bit = mtk_vcodec_is_10Bit(inst->yuv_fmt, MTK_INST_ENCODER) ?
				BW_FACTOR_10BIT : 100;
			bw_factor_afbc = mtk_vcodec_is_afbc(inst->yuv_fmt, MTK_INST_ENCODER) ?
				100 : BW_FACTOR_NONAFBC;
			for (i = 0 ; i < dev->venc_tput_cnt; i++) {
				if (inst->config == dev->venc_tput[i].config &&
				    inst->codec_fmt == dev->venc_tput[i].codec_fmt) {
					inst_bw_factor = dev->venc_tput[i].bw_factor * bw_factor_bit
					* bw_factor_afbc * (freq_sum / dev->venc_tput[i].base_freq);
					if (inst_bw_factor > target_bw_factor) {
						mtk_v4l2_debug(8, "[VDVFS] ctx %d, bw_fac:%d",
						inst->id, inst_bw_factor);
						target_bw_factor = inst_bw_factor;
					}
				}
			}
		}
		return target_bw_factor;
	}
	return 0;
}

void update_freq(struct mtk_vcodec_dev *dev, int codec_type)
{
	struct list_head *item;
	struct vcodec_inst *inst;
	u64 freq = 0;
	u64 freq_sum = 0;
	u32 op_rate_sum = 0;
	u32 target_bw_factor;
	bool no_op_rate_max_freq = false;

	if (codec_type == MTK_INST_DECODER) {
		if (dev->vdec_reg == 0 && dev->vdec_mmdvfs_clk == 0)
			return;

		dev->vdec_dvfs_params.allow_oc = 0;

		if (list_empty(&dev->vdec_dvfs_inst)) {
			freq_sum = match_avail_freq(dev, codec_type, 0);
			dev->vdec_dvfs_params.freq_sum = (u32)freq_sum;
			dev->vdec_dvfs_params.target_freq = (u32)freq_sum;
			return;
		}

		list_for_each(item, &dev->vdec_dvfs_inst) {
			inst = list_entry(item, struct vcodec_inst, list);
			if (inst && inst->is_active) {
				freq = calc_freq(inst, dev);

				if (freq > dev->vdec_dvfs_params.normal_max_freq)
					dev->vdec_dvfs_params.allow_oc = 1;

				freq_sum += freq;
				op_rate_sum += inst->op_rate;
			} else
				mtk_v4l2_debug(6, "[VDVFS] %s no inst, skip", __func__);

		}
		mtk_v4l2_debug(6, "[VDVFS] VDEC freq_sum = %llu, op_rate_sum = %u",
			freq_sum, op_rate_sum);

		if (dev->vdec_dvfs_params.allow_oc == 0) { /* normal max */
			if (freq_sum > dev->vdec_dvfs_params.normal_max_freq)
				freq_sum = dev->vdec_dvfs_params.normal_max_freq;
		}

		if (op_rate_sum < dev->vdec_dvfs_params.per_frame_adjust_op_rate)
			dev->vdec_dvfs_params.per_frame_adjust = 1;
		else
			dev->vdec_dvfs_params.per_frame_adjust = 0;

		dev->vdec_dvfs_params.freq_sum = (u32)freq_sum;
		freq_sum = match_avail_freq(dev, codec_type, freq_sum);

		dev->vdec_dvfs_params.target_freq = (u32)freq_sum;
		mtk_v4l2_debug(6, "[VDVFS] VDEC freq = %u", dev->vdec_dvfs_params.target_freq);
	} else if (codec_type == MTK_INST_ENCODER) {

		if (dev->venc_reg == 0 && dev->venc_mmdvfs_clk == 0)
			return;

		dev->venc_dvfs_params.allow_oc = 0;

		if (list_empty(&dev->venc_dvfs_inst)) {
			freq_sum = match_avail_freq(dev, codec_type, 0);
			dev->venc_dvfs_params.freq_sum = (u32)freq_sum;
			dev->venc_dvfs_params.target_freq = (u32)freq_sum;
			return;
		}

		list_for_each(item, &dev->venc_dvfs_inst) {
			inst = list_entry(item, struct vcodec_inst, list);
			freq = calc_freq(inst, dev);

			if (freq > dev->venc_dvfs_params.normal_max_freq)
				dev->venc_dvfs_params.allow_oc = 1;

			if (inst->op_rate == 0 || inst->op_rate >= 960)
				no_op_rate_max_freq = 1;

			freq_sum += freq;
			op_rate_sum += inst->op_rate;
		}
		mtk_v4l2_debug(6, "[VDVFS] VENC freq_sum = %llu, op_rate_sum = %u",
			freq_sum, op_rate_sum);

		if (dev->venc_dvfs_params.allow_oc == 0) { /* normal max */
			if (freq_sum > dev->venc_dvfs_params.normal_max_freq ||
				no_op_rate_max_freq == 1)
				freq_sum = dev->venc_dvfs_params.normal_max_freq;
		} else { /* allow oc */
			if (no_op_rate_max_freq == 1)
				freq_sum = MAX_VCODEC_FREQ;
		}

		if (op_rate_sum < dev->venc_dvfs_params.per_frame_adjust_op_rate)
			dev->venc_dvfs_params.per_frame_adjust = 1;
		else
			dev->venc_dvfs_params.per_frame_adjust = 0;

		dev->venc_dvfs_params.freq_sum = (u32)freq_sum;
		freq_sum = match_avail_freq(dev, codec_type, freq_sum);

		dev->venc_dvfs_params.target_freq = (u32)freq_sum;
		mtk_v4l2_debug(6, "[VDVFS] VENC freq = %u",
			dev->venc_dvfs_params.target_freq);

		target_bw_factor = mtk_vcodec_get_bw_factor(dev, MTK_INST_ENCODER);
		dev->venc_dvfs_params.target_bw_factor = target_bw_factor;
		mtk_v4l2_debug(6, "[VDVFS] VENC bw_factor = %u",
			dev->venc_dvfs_params.target_bw_factor);
	}
}
