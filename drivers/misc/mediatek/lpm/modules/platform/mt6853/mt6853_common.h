/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MT6853_COMMON_H__
#define __MT6853_COMMON_H__

#include <linux/delay.h>

#include <mtk_lpm_type.h>

/* Platform pwr state */
#define PLAT_XO_UFS_OFF			(1<<1L)
#define PLAT_CLKBUF_ENTER_BBLPM		(1<<2L)

#define PLAT_SUSPEND			(1<<8L)
#define PLAT_MCUSYS_PROTECTED		(1<<9L)
#define PLAT_CLUSTER_PROTECTED		(1<<10L)
#define PLAT_VCORE_LP_MODE		(1<<11L)
#define PLAT_PMIC_VCORE_SRCLKEN0	(1<<12L)
#define PLAT_PMIC_VCORE_SRCLKEN2	(1<<13L)

#define PLAT_MAINPLL_OFF		(1<<27L)
#define PLAT_AP_WDT_RST_MODE		(1<<28L)
#define PLAT_MCUSYSOFF_PREPARED		(1<<29L)
#define PLAT_GIC_MASKED			(1<<30L)
#define PLAT_PROTECTED_FAIL		(1<<31L)

#endif
