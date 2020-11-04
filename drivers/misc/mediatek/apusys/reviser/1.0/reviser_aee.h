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

#ifndef __APUSYS_REVISER_AEE_H__
#define __APUSYS_REVISER_AEE_H__

#include "reviser_cmn.h"

#ifdef CONFIG_MTK_AEE_FEATURE
#include <aee.h>
#endif

#define REVISER_AEE_KEY "REVISER"
#define REVISER_AEE_LOG_SIZE 100

#ifdef CONFIG_MTK_AEE_FEATURE
#define reviser_aee_print(string, args...) do {\
	char msg[REVISER_AEE_LOG_SIZE];\
	int n = 0;\
	n = snprintf(msg, REVISER_AEE_LOG_SIZE, string, ##args); \
	if (n < 0 || n > REVISER_AEE_LOG_SIZE) { \
		LOG_ERR("AEE_LOG_SIZE invalid %d\n", n); \
		break; \
	} \
	aee_kernel_warning(REVISER_AEE_KEY, \
			"\nCRDISPATCH_KEY: " REVISER_AEE_KEY "\n"string, ##args); \
	LOG_ERR(string, ##args);  \
	} while (0)
#else
#define reviser_aee_print(string, args...) do {\
		char msg[REVISER_AEE_LOG_SIZE];\
		int n = 0;\
		n = snprintf(msg, REVISER_AEE_LOG_SIZE, string, ##args); \
		if (n < 0 || n > REVISER_AEE_LOG_SIZE) { \
			LOG_ERR("AEE_LOG_SIZE invalid %d\n", n); \
			break; \
		} \
		LOG_ERR(string, ##args);  \
	} while (0)
#endif


#endif
