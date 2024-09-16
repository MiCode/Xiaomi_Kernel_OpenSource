// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/printk.h>

#include "connsys_debug_utility.h"
#include "connsys_coredump_hw_config.h"

static char* wifi_task_str[] = {
	"Task_WIFI",
	"Task_TST_WFSYS",
	"Task_Idle_WFSYS",
};

static char* bt_task_str[] = {
	"Task_WMT",
	"Task_BT",
	"Task_TST_BTSYS",
	"Task_BT2",
	"Task_Idle_BTSYS",
};

struct coredump_hw_config g_coredump_config[CONN_DEBUG_TYPE_END] = {
	/* Wi-Fi config */
	{
		.name = "WFSYS",
#ifdef CONFIG_FPGA_EARLY_PORTING
		.start_offset = 0x0004f000,
#else
		.start_offset = 0x027f000,
#endif
		.size = 0x18000,
		.seg1_cr = 0x1800112c,
		.seg1_value_end = 0x187fffff,
		.seg1_start_addr = 0x18001120,
		.seg1_phy_addr = 0x18500000,
		.task_table_size = sizeof(wifi_task_str)/sizeof(char*),
		.task_map_table = wifi_task_str,
		.exception_tag_name = "combo_wifi",
	},
	/* BT config */
	{
		.name = "BTSYS",
		.start_offset = 0x33000,
		.size = 0x18000,
		.seg1_cr = 0x18001110,
		.seg1_value_end = 0x18bfffff,
		.seg1_start_addr = 0x18001104,
		.seg1_phy_addr = 0x18900000,
		.task_table_size = sizeof(bt_task_str)/sizeof(char*),
		.task_map_table = bt_task_str,
		.exception_tag_name = "combo_bt",
	},
};

struct coredump_hw_config* get_coredump_platform_config(int conn_type)
{
	if (conn_type < 0 || conn_type >= CONN_DEBUG_TYPE_END) {
		pr_err("Incorrect type: %d\n", conn_type);
		return NULL;
	}
	return &g_coredump_config[conn_type];
}

unsigned int get_coredump_platform_chipid(void)
{
	return 0x6885;
}

char* get_task_string(int conn_type, unsigned int task_id)
{
	if (conn_type < 0 || conn_type >= CONN_DEBUG_TYPE_END) {
		pr_err("Incorrect type: %d\n", conn_type);
		return NULL;
	}

	if (task_id > g_coredump_config[conn_type].task_table_size) {
		pr_err("[%s] Incorrect task: %d\n",
			g_coredump_config[conn_type].name, task_id);
		return NULL;
	}

	return g_coredump_config[conn_type].task_map_table[task_id];
}

char* get_sys_name(int conn_type)
{
	if (conn_type < 0 || conn_type >= CONN_DEBUG_TYPE_END) {
		pr_err("Incorrect type: %d\n", conn_type);
		return NULL;
	}
	return g_coredump_config[conn_type].name;
}

bool is_host_view_cr(unsigned int addr, unsigned int* host_view)
{
	if (addr >= 0x7C000000 && addr <= 0x7Cffffff) {
		if (host_view) {
			*host_view = ((addr - 0x7c000000) + 0x18000000);
		}
		return true;
	} else if (addr >= 0x18000000 && addr <= 0x18ffffff) {
		if (host_view) {
			*host_view = addr;
		}
		return true;
	}
	return false;
}
