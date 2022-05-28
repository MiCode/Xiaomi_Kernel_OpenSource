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

#endif //__MTK_CAM_RAW_RESOURCE_H
