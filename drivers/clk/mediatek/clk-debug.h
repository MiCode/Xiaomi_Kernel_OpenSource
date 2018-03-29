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

#ifndef __DRV_CLK_DEBUG_H
#define __DRV_CLK_DEBUG_H

/*
 * This is a private header file. DO NOT include it except clk-*.c.
 */

#ifdef CLK_DEBUG
#undef CLK_DEBUG
#endif

#define CLK_DEBUG		1

#define TRACE(fmt, args...) \
	pr_warn("[clk-mtk] %s():%d: " fmt, __func__, __LINE__, ##args)

#define PR_ERR(fmt, args...) pr_warn("%s(): " fmt, __func__, ##args)

#ifdef pr_debug
#undef pr_debug
#define pr_debug TRACE
#endif

#ifdef pr_err
#undef pr_err
#define pr_err PR_ERR
#endif

#endif /* __DRV_CLK_DEBUG_H */
