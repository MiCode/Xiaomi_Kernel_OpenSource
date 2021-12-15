/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MT_PBM_
#define _MT_PBM_

/* #include <cust_pmic.h> */
#include <mach/mtk_pmic.h>
#include <mach/mtk_mdpm_api.h>

#ifdef DISABLE_DLPT_FEATURE
#define DISABLE_PBM_FEATURE
#endif

extern void kicker_pbm_by_dlpt(unsigned int i_max);
extern void kicker_pbm_by_md(enum pbm_kicker kicker, bool status);
extern void kicker_pbm_by_cpu(unsigned int loading, int core, int voltage);
extern void kicker_pbm_by_gpu(bool status, unsigned int loading, int voltage);
extern void kicker_pbm_by_flash(bool status);

#ifndef DISABLE_PBM_FEATURE
extern int g_dlpt_stop;
#endif

#endif
