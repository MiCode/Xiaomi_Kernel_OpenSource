// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/string.h>

#include "sla.h"
struct sla_dev_info sla_info[SLA_DEV_TYPE_MAX];

//TODO
#define DOWNLOAD_FLAG 1
bool sla_enable;
int sla_work_mode;
bool enable_sla_to_user;

static bool is_wlan_speed_good(int index)
{
	bool ret = false;

	if (sla_info[index].max_speed >= 300 &&
		sla_info[index].sla_avg_rtt < 150 &&
		sla_info[index].wlan_score >= 60 &&
		sla_info[index].download_flag < DOWNLOAD_FLAG) {
		ret = true;
	}

	return ret;
}

//TODDO
void reset_network_state_by_speed(int index, int speed)
{
	if (speed > 400) {
		if (sla_info[index].avg_rtt > 150)
			sla_info[index].avg_rtt -= 50;

		if (sla_info[index].sla_avg_rtt > 150)
			sla_info[index].sla_avg_rtt -= 50;
		sla_info[index].congestion_flag = CONGESTION_LEVEL_NORMAL;
	}

	if (speed >= 50 &&
		sla_info[index].weight_state == WEIGHT_STATE_USELESS) {
		sla_info[index].weight_state = WEIGHT_STATE_NORMAL;
	}
}

int calc_weight_with_little_speed(int neta, int netb)
{
	if ((is_wlan_speed_good(netb) &&
		sla_info[neta].max_speed <= 50) ||
		(sla_info[neta].max_speed < 10 &&
		sla_info[neta].congestion_flag == CONGESTION_LEVEL_HIGH)) {
		sla_info[neta].weight = 0;
		sla_info[netb].weight = 100;
		return 1;
	}
	return 0;
}

void reset_invalid_network_info(struct sla_dev_info *node)
{
	if (node) {
		memset(node, 0, sizeof(struct sla_dev_info));
		node->weight_state = WEIGHT_STATE_USELESS;
	}
}
