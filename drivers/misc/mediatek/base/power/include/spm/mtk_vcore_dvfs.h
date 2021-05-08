/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_VCORE_DVFS_H
#define __MTK_VCORE_DVFS_H

extern int spm_dvfs_flag_init(int dvfsrc_en);
extern u32 spm_vcorefs_get_MD_status(void);
extern u32 spm_vcorefs_get_md_srcclkena(void);
extern void spm_freq_hopping_cmd(int gps_on);

#endif  /* __MTK_VCORE_DVFS_H */
