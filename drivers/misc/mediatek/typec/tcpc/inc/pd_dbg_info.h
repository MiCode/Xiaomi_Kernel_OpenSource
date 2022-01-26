/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef PD_DBG_INFO_H_INCLUDED
#define PD_DBG_INFO_H_INCLUDED

#include <linux/kernel.h>
#include <linux/module.h>
#include "tcpci_config.h"

#ifdef CONFIG_PD_DBG_INFO
extern int pd_dbg_info(const char *fmt, ...);
extern void pd_dbg_info_lock(void);
extern void pd_dbg_info_unlock(void);
#else
static inline int pd_dbg_info(const char *fmt, ...)
{
	return 0;
}
static inline void pd_dbg_info_lock(void) {}
static inline void pd_dbg_info_unlock(void) {}
#endif	/* CONFIG_PD_DBG_INFO */

#endif /* PD_DBG_INFO_H_INCLUDED */
