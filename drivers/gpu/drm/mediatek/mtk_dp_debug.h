/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_DP_DEBUG_H__
#define __MTK_DP_DEBUG_H__
#include <linux/types.h>

void mtk_dp_debug_enable(bool enable);
bool mtk_dp_debug_get(void);
void mtk_dp_debug(const char *opt);
int mtk_dp_debugfs_init(void);
void mtk_dp_debugfs_deinit(void);


#define DPTXFUNC(fmt, arg...)		\
	pr_info("[DPTX][%s line:%d]"pr_fmt(fmt), __func__, __LINE__, ##arg)

#define DPTXDBG(fmt, arg...)              \
	do {                                 \
		if (mtk_dp_debug_get())                  \
			pr_info("[DPTX]"pr_fmt(fmt), ##arg);     \
	} while (0)

#define DPTXMSG(fmt, arg...)                                  \
		pr_info("[DPTX]"pr_fmt(fmt), ##arg)

#define DPTXERR(fmt, arg...)                                   \
		pr_err("[DPTX][ERROR]"pr_fmt(fmt), ##arg)


#endif

