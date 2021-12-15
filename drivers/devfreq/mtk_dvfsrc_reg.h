/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_DVFSRC_REG_H
#define __MTK_DVFSRC_REG_H

#if defined(CONFIG_MACH_MT6775)

#include "mtk_dvfsrc_reg_mt6775.h"

#elif defined(CONFIG_MACH_MT6765)
#include <mtk_dvfsrc_reg_mt6765.h>
#elif defined(CONFIG_MACH_MT6771)

#include "mtk_dvfsrc_reg_mt6771.h"
#elif defined(CONFIG_MACH_MT6768)
#include <mtk_dvfsrc_reg_mt6768.h>
#elif defined(CONFIG_MACH_MT6765)
#include <mtk_dvfsrc_reg_mt6765.h>
#else
#include <mtk_dvfsrc_reg_mt67xx.h>
#endif

#endif /* __MTK_DVFSRC_REG_H */


