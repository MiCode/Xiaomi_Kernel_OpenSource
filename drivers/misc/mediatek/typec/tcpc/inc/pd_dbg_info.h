/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef PD_DBG_INFO_H_INCLUDED
#define PD_DBG_INFO_H_INCLUDED

#if IS_ENABLED(CONFIG_PD_DBG_INFO)
extern int pd_dbg_info(const char *fmt, ...);
#else
static inline int pd_dbg_info(const char *fmt, ...)
{
	return 0;
}
#endif	/* CONFIG_PD_DBG_INFO */
#endif /* PD_DBG_INFO_H_INCLUDED */
