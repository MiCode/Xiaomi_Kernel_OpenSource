/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
