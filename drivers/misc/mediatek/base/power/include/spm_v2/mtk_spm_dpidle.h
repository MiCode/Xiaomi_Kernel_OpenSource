/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __MTK_SPM_DPIDLE__H__
#define __MTK_SPM_DPIDLE__H__

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)

#include "mtk_spm_dpidle_mt6757.h"

#endif

extern void spm_dpidle_pre_process(void);
extern void spm_dpidle_post_process(void);
extern void spm_deepidle_chip_init(void);

#endif /* __MTK_SPM_DPIDLE__H__ */

