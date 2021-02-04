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

#ifndef __MTK_DVFSRC_REG_H
#define __MTK_DVFSRC_REG_H

#if defined(CONFIG_MACH_MT6765)
#include <mtk_dvfsrc_reg_mt6765.h>
#elif defined(CONFIG_MACH_MT6761)
#include <mtk_dvfsrc_reg_mt6761.h>
#elif defined(CONFIG_MACH_MT3967)
#include <mtk_dvfsrc_reg_mt3967.h>
#elif defined(CONFIG_MACH_MT6779)
#include <mtk_dvfsrc_reg_mt6779.h>
#else
#include <mtk_dvfsrc_reg_mt67xx.h>
#endif


#endif /* __MTK_DVFSRC_REG_H */
