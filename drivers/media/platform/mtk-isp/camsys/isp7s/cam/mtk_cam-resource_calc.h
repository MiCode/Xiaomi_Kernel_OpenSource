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
	int clk;		/* MHz */

	int bin;
};

static int _pixel_mode(struct mtk_cam_res_calc *tp)
{
	return min(8, tp->raw_num * (tp->bin ? 4 : 1) * tp->raw_pixel_mode);
}

static int _hor_twin_loss(struct mtk_cam_res_calc *tp)
{
	static const int loss[] = {0, 108, 148};
	int idx = tp->raw_num - 1;

	if (idx < 0 || idx >= ARRAY_SIZE(loss))
		return 0;

	return loss[idx];
}

static int _twin_loss(struct mtk_cam_res_calc *tp)
{
	int loss;

	loss = _hor_twin_loss(tp);

	/* TODO: tsfs issue in 3-raw case */
	return loss;
}

static int _hor_overhead(struct mtk_cam_res_calc *tp)
{
	int oh;
	const int HOR_BUBBLE = 25;

	oh = HOR_BUBBLE * tp->raw_num * tp->raw_pixel_mode
		+ tp->raw_num * _twin_loss(tp);

	/* TODO: what if cbn? */
	if (tp->bin)
		oh = oh * 2;

	return max(_pixel_mode(tp) == 8 ? 999 : 0, oh);

}

static int _twin_overhead(struct mtk_cam_res_calc *tp)
{
	return _hor_overhead(tp) * 100 / tp->width;
}

static inline int _process_pxl_per_line(struct mtk_cam_res_calc *tp,
					bool include_hor_verhead)
{
	long linetime = tp->line_time;
	int pxlmode = tp->raw_pixel_mode;
	int clk = tp->clk;
	const int hopping = 2; /* percent */
	int twin_overhead = _twin_overhead(tp);
	int w_processed;

	w_processed = linetime * clk * pxlmode
		* (100 - hopping) / 100
		* (include_hor_verhead ? (100 - twin_overhead) : 100) / 100
		/ 1000;
	return w_processed;
}

static inline bool mtk_cam_raw_check_throughput(struct mtk_cam_res_calc *tp)
{
	return tp->width <= _process_pxl_per_line(tp, 1);
}

static inline bool mtk_cam_sv_check_throughput(struct mtk_cam_res_calc *tp)
{
	return tp->width <= _process_pxl_per_line(tp, 0);
}

/* TODO: check vb */
/* TODO: check line buffer */

#endif //__MTK_CAM_RAW_RESOURCE_H
