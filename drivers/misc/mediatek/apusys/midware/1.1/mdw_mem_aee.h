/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __APUSYS_MDW_MEM_AEE_H__
#define __APUSYS_MDW_MEM_AEE_H__

#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/sched/clock.h>
#include <apusys_secure.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include <aee.h>
#endif
#include "mdw_cmn.h"


#define AEE_KEY "APUMEM"
#define AEE_LOG_SIZE 100
#define DUMP_LOG_SIZE 0x1000


#ifdef CONFIG_MTK_AEE_FEATURE
#define apusys_aee_print(string, args...) do {\
	char msg[AEE_LOG_SIZE];\
	int n = 0;\
	n = snprintf(msg, AEE_LOG_SIZE, string, ##args); \
	if (n < 0 || n > AEE_LOG_SIZE) { \
		mdw_drv_err("AEE_LOG_SIZE invalid %d\n", n); \
		break; \
	} \
	aee_kernel_warning(AEE_KEY, \
			"\nCRDISPATCH_KEY: " AEE_KEY "\n"string, ##args); \
	mdw_drv_err(string, ##args);  \
	} while (0)
#else
#define apusys_aee_print(string, args...) do {\
		char msg[AEE_LOG_SIZE];\
		int n = 0;\
		n = snprintf(msg, AEE_LOG_SIZE, string, ##args); \
		if (n < 0 || n > AEE_LOG_SIZE) { \
			mdw_drv_err("AEE_LOG_SIZE invalid %d\n", n); \
			break; \
		} \
		mdw_drv_err(string, ##args);  \
	} while (0)
#endif

#define DUMP_LOG(cur, end, string, args...) do {\
		if (cur < end) { \
			cur += snprintf(cur, end - cur, string, ##args); \
		} \
	} while (0)


struct mdw_usr_mem_aee {
	unsigned char log_buf[DUMP_LOG_SIZE];
	struct mutex mtx;
};

#endif
