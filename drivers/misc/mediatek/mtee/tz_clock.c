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


#include <linux/module.h>
#include <linux/types.h>
#include <linux/clk.h>

#include "tz_cross/trustzone.h"
#include "trustzone/kree/system.h"
#include "kree_int.h"

#ifdef CONFIG_OF
int KREE_ServEnableClock(u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
	struct ree_service_clock *param = (struct ree_service_clock *)uparam;
	struct clk *clk = mtee_clk_get(param->clk_name);

	if (clk == NULL) {
		pr_warn("can not find clk %s\n", param->clk_name);
		return TZ_RESULT_ERROR_GENERIC;
	}

	clk_prepare_enable(clk);

	return TZ_RESULT_SUCCESS;
}

int KREE_ServDisableClock(u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
	struct ree_service_clock *param = (struct ree_service_clock *)uparam;
	struct clk *clk = mtee_clk_get(param->clk_name);

	if (clk == NULL) {
		pr_warn("can not find clk %s\n", param->clk_name);
		return TZ_RESULT_ERROR_GENERIC;
	}

	clk_disable_unprepare(clk);

	return TZ_RESULT_SUCCESS;
}
#else
int KREE_ServEnableClock(u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
/*	struct ree_service_clock *param = (struct ree_service_clock *)uparam; */
	int ret = TZ_RESULT_ERROR_GENERIC;
/*	int rret; */

/*	rret = enable_clock(param->clk_id, param->clk_name); */
/*	if (rret < 0) */
/*		ret = TZ_RESULT_ERROR_GENERIC; */

	return ret;
}

int KREE_ServDisableClock(u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
/*	struct ree_service_clock *param = (struct ree_service_clock *)uparam; */
	int ret = TZ_RESULT_ERROR_GENERIC;
/*	int rret; */

/*	rret = disable_clock(param->clk_id, param->clk_name); */
/*	if (rret < 0) */
/*		ret = TZ_RESULT_ERROR_GENERIC; */

	return ret;
}
#endif
