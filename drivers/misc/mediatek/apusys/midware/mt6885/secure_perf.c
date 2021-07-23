/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/pm_qos.h>

static struct pm_qos_request ddr_opp_req;

void secure_perf_init(void)
{
	pm_qos_add_request(&ddr_opp_req, PM_QOS_DDR_OPP,
						PM_QOS_DDR_OPP_DEFAULT_VALUE);
}

void secure_perf_remove(void)
{
	pm_qos_remove_request(&ddr_opp_req);
}

void secure_perf_raise(void)
{
	/* Fix DDR to higher frequency (OPP 3) */
	//pm_qos_update_request(&ddr_opp_req, 3);

}

void secure_perf_restore(void)
{
	/* Reset DDR to default OPP */
	//pm_qos_update_request(&ddr_opp_req, PM_QOS_DDR_OPP_DEFAULT_VALUE);
}
