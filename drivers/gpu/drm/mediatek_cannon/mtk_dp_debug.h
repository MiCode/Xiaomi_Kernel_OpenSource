/*
 * Copyright (c) 2020 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

