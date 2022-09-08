/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_VCORE_DVFS_H
#define __MTK_VCORE_DVFS_H

extern int spm_dvfs_flag_init(int dvfsrc_en);
extern u32 spm_vcorefs_get_MD_status(void);
extern u32 spm_vcorefs_get_md_srcclkena(void);
extern void spm_freq_hopping_cmd(int gps_on);

#endif  /* __MTK_VCORE_DVFS_H */
