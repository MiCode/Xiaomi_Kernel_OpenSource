/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MTK_HCP_SUPPORT_H
#define MTK_HCP_SUPPORT_H

#include <mtk-hcp.h>

int CM4_SUPPORT_CONFIGURE_TABLE[][2] = {
#ifdef CONFIG_ISP_CM4_SUPPORT
	{MODULE_ISP, 1},
#endif
#ifdef CONFI_FD_CM4_SUPPORT
	{MODULE_FD, 1},
#endif
	{MODULE_MAX_ID, 0}
};

#endif /* _MTK_HCP_SUPPORT_H */
