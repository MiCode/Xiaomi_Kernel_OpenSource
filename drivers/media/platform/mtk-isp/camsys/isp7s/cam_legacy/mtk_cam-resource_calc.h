/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_RAW_RESOURCE_H
#define __MTK_CAM_RAW_RESOURCE_H

#include <linux/minmax.h>

struct mtk_cam_res_calc {
	long line_time;		/* ns */
	int width;
	int height;

	int raw_num;		/* [1..3] */
	int raw_pixel_mode;	/* [1,2] */
	int clk;		/* Hz */

	int cbn_type : 4; /* 0: disable, 1: 2x2, 2: 3x3 3: 4x4 */
	int qbnd_en : 4;
	int qbn_type : 4; /* 0: disable, 1: w/2, 2: w/4 */
	int bin_en : 4;
};

struct raw_resource_stepper {
	/* pixel mode */
	int pixel_mode_min;
	int pixel_mode_max;

	/* raw num */
	int num_raw_min;
	int num_raw_max;

	/* freq */
	unsigned int opp_num;
	unsigned int *clklv;
	unsigned int *voltlv;

	int pixel_mode;
	int num_raw;
	int opp_idx;
};

static int _pixel_mode(struct mtk_cam_res_calc *c)
{
	return min(8, c->raw_num * (c->bin_en ? 4 : 1) * c->raw_pixel_mode);
}

static int _hor_twin_loss(struct mtk_cam_res_calc *c)
{
	static const int loss[] = {0, 108, 148};
	int idx = c->raw_num - 1;

	if (idx < 0 || idx >= ARRAY_SIZE(loss))
		return 0;

	return loss[idx];
}

static int _twin_loss(struct mtk_cam_res_calc *c)
{
	int loss;

	loss = _hor_twin_loss(c);

	/* TODO: tsfs issue in 3-raw case */
	return loss;
}

static int _hor_overhead(struct mtk_cam_res_calc *c)
{
	int oh;
	const int HOR_BUBBLE = 25;

	oh = HOR_BUBBLE * c->raw_num * c->raw_pixel_mode
		+ c->raw_num * _twin_loss(c);

	/* TODO: what if cbn? */
	if (c->bin_en)
		oh = oh * 2;

	return max(_pixel_mode(c) == 8 ? 999 : 0, oh);

}

static int _twin_overhead(struct mtk_cam_res_calc *c)
{
	return _hor_overhead(c) * 100 / c->width;
}

static inline int _process_pxl_per_line(struct mtk_cam_res_calc *c,
					bool is_raw, bool enable_log)
{
	const int hopping = 2; /* percent */
	int twin_overhead = 0;
	int freq_Mhz = c->clk / 1000000;
	int w_processed;

	if (is_raw) {
		twin_overhead = _twin_overhead(c);
		w_processed = c->line_time * freq_Mhz * _pixel_mode(c)
			* (100 - hopping) / 100
			* (100 - twin_overhead) / 100
			/ 1000;
	} else
		w_processed = c->line_time * freq_Mhz * 8
			* (100 - hopping) / 100
			/ 1000;

	if (enable_log)
		pr_info("%s: wp %d lt %ld ov %d clk %d pxl %d num %d\n",
			__func__, w_processed, c->line_time, twin_overhead,
			freq_Mhz, c->raw_pixel_mode, c->raw_num);

	return w_processed;
}

static inline bool _valid_cbn_type(int cbn_type)
{
	return cbn_type >= 0 && cbn_type < 4;
}

static inline bool _valid_qbn_type(int qbn_type)
{
	return qbn_type >= 0 && qbn_type < 3;
}

static inline bool mtk_cam_raw_check_xbin_valid(struct mtk_cam_res_calc *c,
						bool enable_log)
{
	bool cbn_en = !!(c->cbn_type);
	bool invalid =
		!_valid_cbn_type(c->cbn_type) ||
		!_valid_qbn_type(c->qbn_type) ||
		(cbn_en + c->qbnd_en + c->bin_en > 1) ||
		(c->qbn_type && (c->qbnd_en || c->bin_en)); /* QBN w. CBN only */

	if (invalid && enable_log)
		pr_info("%s: cbn %d qbnd %d qbn %d bin %d failed\n",
			__func__, c->cbn_type, c->qbnd_en, c->qbn_type,
			c->bin_en);

	return !invalid;
}

static inline int _xbin_scale_ratio(struct mtk_cam_res_calc *c)
{
	int ratio = 1;

	ratio *= c->bin_en ? 2 : 1;
	ratio *= c->qbn_type ? (1 << c->qbn_type) : 1;
	ratio *= c->qbnd_en ? 2 : 1;
	ratio *= c->cbn_type ? (c->cbn_type + 1) : 1;
	return ratio;
}

static inline bool mtk_cam_raw_check_line_buffer(struct mtk_cam_res_calc *c,
						 bool enable_log)
{
	const int max_main_pipe_w = 6632;
	const int max_main_pipe_twin_w = 6200;
	int max_width;
	bool valid;

	if (!mtk_cam_raw_check_xbin_valid(c, enable_log))
		return false;

	max_width = c->raw_num > 1 ?
		(c->raw_num * max_main_pipe_twin_w) : max_main_pipe_w;
	max_width *= _xbin_scale_ratio(c);

	valid = (max_width >= c->width);
	if (!valid && enable_log)
		pr_info("%s: w %d max_w %d n %d cbn %d qbnd %d qbn %d bin %d failed\n",
			__func__, c->width, max_width,
			c->raw_num,
			c->cbn_type, c->qbnd_en, c->qbn_type, c->bin_en);

	return valid;
}

static inline bool mtk_cam_raw_check_throughput(struct mtk_cam_res_calc *c,
						bool enable_log)
{
	int processed_w = _process_pxl_per_line(c, 1, enable_log);

	return c->width <= processed_w;
}

static inline bool mtk_cam_sv_check_throughput(struct mtk_cam_res_calc *c,
					       bool enable_log)
{
	int processed_w = _process_pxl_per_line(c, 0, enable_log);

	return c->width <= processed_w;
}

static __maybe_unused int step_next_raw_num(struct raw_resource_stepper *stepper)
{
	if (stepper->num_raw < stepper->num_raw_max) {
		++stepper->num_raw;
		return 0;
	}
	stepper->num_raw = stepper->num_raw_min;
	return -1;
}

static __maybe_unused int step_next_opp(struct raw_resource_stepper *stepper)
{
	++stepper->opp_idx;
	if (stepper->opp_idx < stepper->opp_num)
		return 0;

	stepper->opp_idx = 0;
	return -1;
}

static __maybe_unused int step_next_pixel_mode(struct raw_resource_stepper *stepper)
{
	if (stepper->pixel_mode < stepper->pixel_mode_max) {
		++stepper->pixel_mode;
		return 0;
	}
	stepper->pixel_mode = stepper->pixel_mode_min;
	return -1;
}

typedef int (*step_fn_t)(struct raw_resource_stepper *stepper);

static __maybe_unused bool valid_resouce_set(struct raw_resource_stepper *stepper)
{
	/* invalid sets */
	bool invalid =
		(stepper->pixel_mode == 1 && stepper->opp_idx != 0) ||
		(stepper->pixel_mode == 1 && stepper->num_raw > 1);

	return !invalid;
}

static __maybe_unused int loop_resource_till_valid(struct mtk_cam_res_calc *c,
				    struct raw_resource_stepper *stepper,
				    const step_fn_t *arr_step, int arr_size)
{
	const bool enable_log = true;
	int i;
	int ret = -1;

	do {
		bool pass;

		i = 0;
		if (valid_resouce_set(stepper)) {

			c->raw_pixel_mode = stepper->pixel_mode;
			c->raw_num = stepper->num_raw;
			c->clk = stepper->clklv[stepper->opp_idx];

			pass = mtk_cam_raw_check_line_buffer(c, enable_log) &&
				mtk_cam_raw_check_throughput(c, enable_log);
			if (pass) {
				ret = 0;
				break;
			}
		}

		if (arr_step[0](stepper)) {

			i = 1;
			while (i < arr_size && arr_step[i](stepper))
				++i;
		}
	} while (i < arr_size);

	return ret;
}

static __maybe_unused int mtk_raw_find_combination(struct mtk_cam_res_calc *c,
				    struct raw_resource_stepper *stepper)
{
	static const step_fn_t policy[] = {
		step_next_pixel_mode,
		step_next_opp,
		step_next_raw_num
	};

	/* initial value */
	stepper->pixel_mode = stepper->pixel_mode_min;
	stepper->num_raw = stepper->num_raw_min;
	stepper->opp_idx = 0;

	return loop_resource_till_valid(c, stepper,
					policy, ARRAY_SIZE(policy));
}
#endif //__MTK_CAM_RAW_RESOURCE_H
