/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author:
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
#ifndef MTK_HCP_SUPPORT_H
#define MTK_HCP_SUPPORT_H

#include <mtk-hcp.h>

int CM4_SUPPORT_TABLE[][2] = {
#ifdef CONFIG_ISP_CM4_SUPPORT
	{MODULE_ISP, 1},
#endif
#ifdef CONFI_FD_CM4_SUPPORT
	{MODULE_FD, 1},
#endif
	{MODULE_MAX_ID, 0}
};

#endif /* _MTK_HCP_SUPPORT_H */
