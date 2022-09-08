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

#define WP_SCENARIO 6

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
	new_inst->config = DEFAULT_VENC_CONFIG; /* Read from register when encode done */
	new_inst->core_cnt = 1; /* Depends on WP */
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

u32 match_avail_freq(struct mtk_vcodec_dev *dev, int codec_type, u32 freq)
{
	int i;
	u32 match_freq = 0;

	if (codec_type == MTK_INST_DECODER) {
		match_freq = dev->vdec_freqs[0];

		for (i = 0; i < MAX_CODEC_FREQ_STEP-1; i++) {
			mtk_v4l2_debug(8, "[VDVFS] VDEC i %d, freq %lu, in_freq %u",
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
			mtk_v4l2_debug(8, "[VDVFS] VENC i %d, freq %lu, in_freq %u",
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
u32 calc_freq(struct vcodec_inst *inst, struct mtk_vcodec_dev *dev)
{
	struct vcodec_perf *perf;
	u32 dflt_op_rate;
	u32 freq = 0;

	perf = find_perf(inst, dev);
	if (inst->codec_type == MTK_INST_DECODER) {
		if (perf != 0) {
			freq = inst->width * inst->height / 256 * inst->op_rate *
				perf->cy_per_mb_1;

			mtk_v4l2_debug(6, "[VDVFS] VDEC w:%u x h:%u / 256 x oprate: %d x mb %u",
				inst->width, inst->height, inst->op_rate, perf->cy_per_mb_1);
		} else
			freq = 100000000;

		if (perf != 0 && inst->op_rate <= 0) {
			/* Undefined priority + op_rate combination behavior, to be configurable */
			dflt_op_rate = find_dflt_op_rate(inst, dev);

			if (inst->priority < 0) {
				inst->op_rate = 30;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
				if (inst->codec_fmt == 808996950) {
					/* performance class WA for VP8 */
					inst->op_rate = 60;
				} else if (feature_table[VENC_FEATURE_ID].enable > 0 &&
					inst->codec_fmt == 875967048 && dev->dec_cnt > 1 &&
					(inst->width * inst->height <= 1920 * 1088)) {
					inst->op_rate = 174;
				}
#endif
			} else
				inst->op_rate = dflt_op_rate;

			mtk_v4l2_debug(6, "[VDVFS] VDEC w:%u x h:%u priority %d, new oprate %u",
				inst->width, inst->height, inst->priority, inst->op_rate);

			freq = inst->width * inst->height / 256 * inst->op_rate *
				perf->cy_per_mb_1;

			mtk_v4l2_debug(6, "[VDVFS] VDEC priority:%d oprate:%d, set freq = %u",
					inst->priority, inst->op_rate, freq);
		}
	} else if (inst->codec_type == MTK_INST_ENCODER) {
		if (perf != 0) {
			freq = inst->width * inst->height / 256 * inst->op_rate;
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
			freq = dev->venc_dvfs_params.normal_max_freq;
			mtk_v4l2_debug(6, "[VDVFS] VENC oprate: %d, set freq = %u",
					inst->op_rate, freq);
		}
	}

	mtk_v4l2_debug(6, "[VDVFS] freq = %u", freq);
	return freq;
}


void update_freq(struct mtk_vcodec_dev *dev, int codec_type)
{
	struct list_head *item;
	struct vcodec_inst *inst;
	u32 freq = 0;
	u64 freq_sum = 0;
	u32 op_rate_sum = 0;
	bool no_op_rate_max_freq = false;

	if (codec_type == MTK_INST_DECODER) {
		dev->vdec_dvfs_params.allow_oc = 0;

		if (list_empty(&dev->vdec_dvfs_inst)) {
			freq_sum = match_avail_freq(dev, codec_type, 0);
			dev->vdec_dvfs_params.target_freq = (u32)freq_sum;
			return;
		}

		list_for_each(item, &dev->vdec_dvfs_inst) {
			inst = list_entry(item, struct vcodec_inst, list);
			if (inst) {
				freq = calc_freq(inst, dev);

				if (freq > dev->vdec_dvfs_params.normal_max_freq)
					dev->vdec_dvfs_params.allow_oc = 1;

				freq_sum += freq;
				op_rate_sum += inst->op_rate;
			} else {
				mtk_v4l2_debug(6, "[VDVFS] %s no inst, skip", __func__);
			}
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

		freq_sum = match_avail_freq(dev, codec_type, freq_sum);

		dev->vdec_dvfs_params.target_freq = (u32)freq_sum;
		mtk_v4l2_debug(6, "[VDVFS] VDEC freq = %u", dev->vdec_dvfs_params.target_freq);
	} else if (codec_type == MTK_INST_ENCODER) {
		dev->venc_dvfs_params.allow_oc = 0;

		if (list_empty(&dev->venc_dvfs_inst)) {
			freq_sum = match_avail_freq(dev, codec_type, 0);
			dev->venc_dvfs_params.target_freq = (u32)freq_sum;
			return;
		}

		list_for_each(item, &dev->venc_dvfs_inst) {
			inst = list_entry(item, struct vcodec_inst, list);
			freq = calc_freq(inst, dev);

			if (freq > dev->venc_dvfs_params.normal_max_freq)
				dev->venc_dvfs_params.allow_oc = 1;

			if (inst->op_rate == 0)
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

		freq_sum = match_avail_freq(dev, codec_type, freq_sum);

		dev->venc_dvfs_params.target_freq = (u32)freq_sum;
		mtk_v4l2_debug(6, "[VDVFS] VENC freq = %u", dev->venc_dvfs_params.target_freq);
	}
}
