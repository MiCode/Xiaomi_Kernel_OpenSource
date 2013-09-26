/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/rpm.h>
#include <mach/socinfo.h>
#include "msm_bus_core.h"
#include "msm_bus_noc.h"
#include "msm_bus_bimc.h"

static uint32_t master_iids[MSM_BUS_MASTER_LAST];
static uint32_t slave_iids[MSM_BUS_SLAVE_LAST - SLAVE_ID_KEY];

static void msm_bus_assign_iids(struct msm_bus_fabric_registration
	*fabreg, int fabid)
{
	int i;
	for (i = 0; i < fabreg->len; i++) {
		if (!fabreg->info[i].gateway) {
			fabreg->info[i].priv_id = fabid + fabreg->info[i].id;
			if (fabreg->info[i].id < SLAVE_ID_KEY) {
				if (fabreg->info[i].id >= MSM_BUS_MASTER_LAST) {
					WARN(1, "id %d exceeds array size!\n",
						fabreg->info[i].id);
					continue;
				}

				master_iids[fabreg->info[i].id] =
					fabreg->info[i].priv_id;
			} else {
				if ((fabreg->info[i].id - SLAVE_ID_KEY) >=
					(MSM_BUS_SLAVE_LAST - SLAVE_ID_KEY)) {
					WARN(1, "id %d exceeds array size!\n",
						fabreg->info[i].id);
					continue;
				}

				slave_iids[fabreg->info[i].id - (SLAVE_ID_KEY)]
					= fabreg->info[i].priv_id;
			}
		} else {
			fabreg->info[i].priv_id = fabreg->info[i].id;
		}
	}
}

static int msm_bus_get_iid(int id)
{
	if ((id < SLAVE_ID_KEY && id >= MSM_BUS_MASTER_LAST) ||
		id >= MSM_BUS_SLAVE_LAST) {
		MSM_BUS_ERR("Cannot get iid. Invalid id %d passed\n", id);
		return -EINVAL;
	}

	return CHECK_ID(((id < SLAVE_ID_KEY) ? master_iids[id] :
		slave_iids[id - SLAVE_ID_KEY]), id);
}


static struct msm_bus_board_algorithm msm_bus_id_algo = {
	.get_iid = msm_bus_get_iid,
	.assign_iids = msm_bus_assign_iids,
};

int msm_bus_board_rpm_get_il_ids(uint16_t *id)
{
	return -ENXIO;
}

void msm_bus_board_init(struct msm_bus_fabric_registration *pdata)
{
	if (machine_is_msm8226())
		msm_bus_id_algo.board_nfab = NFAB_MSM8226;
	else if (machine_is_msm8610())
		msm_bus_id_algo.board_nfab = NFAB_MSM8610;

	pdata->board_algo = &msm_bus_id_algo;
}
