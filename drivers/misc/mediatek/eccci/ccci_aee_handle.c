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

#include <mt-plat/aee.h>

#include "ccci_aee_handle.h"



#if defined(CONFIG_MTK_AEE_FEATURE)

static unsigned int tracing_flag = DB_OPT_DEFAULT;


void ccci_tracing_off(void)
{
	if (tracing_is_on()) {
		tracing_flag = DB_OPT_TRACING_OFF_CCCI;
		tracing_off();

	} else
		tracing_flag = DB_OPT_DEFAULT;

}

void ccci_aed_md_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt)
{
	aed_md_exception_api(log, log_size, phy, phy_size, detail,
			db_opt | tracing_flag);

	tracing_flag = DB_OPT_DEFAULT;

}



#endif
