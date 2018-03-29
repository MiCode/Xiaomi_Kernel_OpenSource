/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __MT_SPM_VCOREFS__H__
#define __MT_SPM_VCOREFS__H__

#include "mt_spm.h"

#define SPM_VCORE_DVFS_EN	0
#define DYNAMIC_LOAD		0

/* FIXME: */
#define SPM_DVFS_TIMEOUT	1000	/* 1ms */

#define MSDC1	1
#define MSDC3	3

void spm_go_to_vcorefs(int spm_flags);
int spm_set_vcore_dvfs(int opp);
void spm_vcorefs_init(void);
int spm_dvfs_flag_init(void);
char *spm_vcorefs_dump_dvfs_regs(char *p);
void spm_to_sspm_pwarp_cmd(void);
void spm_msdc_setting(int msdc, bool status);

#endif /* __MT_SPM_VCOREFS__H__ */
