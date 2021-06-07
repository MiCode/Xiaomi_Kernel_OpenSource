/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef MTK_FMT_PM_H
#define MTK_FMT_PM_H
#include "vdec_fmt_driver.h"

void fmt_init_pm(struct mtk_vdec_fmt *fmt);
int32_t fmt_clock_on(struct mtk_vdec_fmt *fmt);
int32_t fmt_clock_off(struct mtk_vdec_fmt *fmt);
void fmt_prepare_dvfs_emi_bw(void);
void fmt_unprepare_dvfs_emi_bw(void);
void fmt_start_dvfs_emi_bw(struct fmt_pmqos pmqos_param);
void fmt_end_dvfs_emi_bw(void);
void fmt_translation_fault_callback_setting(struct mtk_vdec_fmt *fmt);

#endif /* _MTK_FMT_PM_H */
