/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_FMT_PM_H
#define MTK_FMT_PM_H
#include "vdec_fmt_driver.h"

void fmt_init_pm(struct mtk_vdec_fmt *fmt);
int32_t fmt_clock_on(struct mtk_vdec_fmt *fmt);
int32_t fmt_clock_off(struct mtk_vdec_fmt *fmt);
void fmt_prepare_dvfs_emi_bw(struct mtk_vdec_fmt *fmt);
void fmt_unprepare_dvfs_emi_bw(void);
void fmt_start_dvfs_emi_bw(struct mtk_vdec_fmt *fmt,
	struct fmt_pmqos pmqos_param, int id);
void fmt_end_dvfs_emi_bw(struct mtk_vdec_fmt *fmt, int id);

#endif /* _MTK_FMT_PM_H */
