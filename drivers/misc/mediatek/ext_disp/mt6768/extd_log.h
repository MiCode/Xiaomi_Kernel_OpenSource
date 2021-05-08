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

#ifndef __EXTD_DRV_LOG_H__
#define __EXTD_DRV_LOG_H__

#include <linux/printk.h>

#include "disp_drv_log.h"

extern unsigned int g_extd_mobilelog;


#define EXTDFUNC()					\
	do {								\
		DISPFUNC();	\
		if ((!g_mobilelog) &&	\
				(g_extd_mobilelog))	\
			pr_info("[EXTD]func|%s\n", __func__);		\
	} while (0)

#define EXTDINFO(string, args...)					\
	do {								\
		DISPINFO("[EXTD]"string, ##args);	\
		if ((!g_mobilelog) &&	\
				(g_extd_mobilelog))	\
			pr_info("[EXTD]:"string, ##args);		\
	} while (0)

#define EXTDMSG(string, args...)	DISPMSG("[EXTD]"string, ##args)

#define EXTDERR(string, args...)	DISPERR("[EXTD]"string, ##args)

#endif
