// SPDX-License-Identifier: GPL-2.0-only
/*
 * slp_test.c - Unisoc platform driver
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <linux/delay.h>
#include <linux/kthread.h>
#include <misc/wcn_bus.h>

#include "slp_mgr.h"
#include "slp_sdio.h"
#include "wcn_glb_reg.h"
#include "slp_dbg.h"

static int test_cnt;
static int sleep_test_thread(void *data)
{
	unsigned int ram_val = 0;

	while (1) {
		if (test_cnt)
			msleep(5000);
		else
			msleep(30000);

		slp_mgr_drv_sleep(DT_READ, FALSE);
		slp_mgr_wakeup(DT_READ);

		sprdwcn_bus_reg_read(get_cp_start_addr(), &ram_val, 0x4);
		WCN_INFO("ram_val is 0x%x\n", ram_val);

		msleep(5000);
		slp_mgr_drv_sleep(DT_READ, TRUE);
		test_cnt++;
	}

	return 0;
}

static struct task_struct *slp_test_task;
int slp_test_init(void)
{
	WCN_INFO("create slp_mgr test thread\n");
	if (!slp_test_task)
		slp_test_task = kthread_create(sleep_test_thread,
			NULL, "sleep_test_thread");
	if (slp_test_task) {
		wake_up_process(slp_test_task);
		return 0;
	}

	WCN_ERR("create sleep_test_thread fail\n");

	return -1;
}
