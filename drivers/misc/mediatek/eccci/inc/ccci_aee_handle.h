/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __CCCI_AEE_HANDLE_H__
#define __CCCI_AEE_HANDLE_H__

#if defined(CONFIG_MTK_AEE_FEATURE)

#include <linux/kernel.h>

void ccci_tracing_off(void);

void ccci_aed_md_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt);

#endif

#endif /* __CCCI_AEE_HANDLE_H__ */
