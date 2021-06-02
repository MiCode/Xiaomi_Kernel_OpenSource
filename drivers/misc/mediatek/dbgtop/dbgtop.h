/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __DBGTOP_H__
#define __DBGTOP_H__

int mtk_dbgtop_dram_reserved(int enable);
int mtk_dbgtop_cfg_dvfsrc(int enable);
int mtk_dbgtop_pause_dvfsrc(int enable);
int mtk_dbgtop_dfd_count_en(int value);
int mtk_dbgtop_dfd_therm1_dis(int value);
int mtk_dbgtop_dfd_therm2_dis(int value);
int mtk_dbgtop_dfd_timeout(int value_abnormal, int value_normal);
int mtk_dbgtop_dfd_timeout_reset(void);
int mtk_dbgtop_mfg_pwr_on(int value);
int mtk_dbgtop_mfg_pwr_en(int value);

#endif
