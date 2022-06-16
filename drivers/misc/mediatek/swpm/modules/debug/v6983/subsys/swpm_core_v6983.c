// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/perf_event.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#if IS_ENABLED(CONFIG_MEDIATEK_CPUFREQ_DEBUG_LITE)
/* extern int get_devinfo(int i); */
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#endif

#if IS_ENABLED(CONFIG_MTK_THERMAL)
#include <thermal_interface.h>
#endif

#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_core_v6983.h>

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
/* rt => /100000, uA => *1000, res => 100 */
#define CORE_DEFAULT_DEG (30)
#define CORE_STATIC_MA (107)
#define CORE_STATIC_ID (7)
#define CORE_STATIC_RT_RES (100)
#define V_OF_CORE_STATIC (750)
static unsigned int core_static;
static unsigned int core_static_replaced;
static unsigned short core_static_rt[NR_CORE_STATIC_TYPE] = {
	8256, 9725, 10563, 11125, 16295, 20843, 23192
};

/* share sram for swpm core */
static struct core_swpm_data *core_swpm_data_ptr;

unsigned int swpm_core_static_data_get(void)
{
	return core_static;
}

void swpm_core_static_replaced_data_set(unsigned int data)
{
	core_static_replaced = data;
}

void swpm_core_static_data_init(void)
{
	unsigned int static_p = 0, scaled_p;
	unsigned int i, j;

	if (!core_swpm_data_ptr)
		return;

#if IS_ENABLED(CONFIG_MEDIATEK_CPUFREQ_DEBUG_LITE)
	/* TODO: default mA from get_devinfo */
	/* static_p = get_devinfo(CORE_STATIC_ID); */
#endif

	/* default CORE_STATIC mA */
	if (!static_p)
		static_p = CORE_STATIC_MA;

	/* recording default static data, and check replacement data */
	core_static = static_p;
	static_p = (!core_static_replaced) ?
		static_p : core_static_replaced;

	/* static power unit mA with voltage scaling */
	for (i = 0; i < NR_CORE_VOLT; i++) {
		scaled_p = static_p *
			core_swpm_data_ptr->core_volt_tbl[i] / V_OF_CORE_STATIC;
		for (j = 0; j < NR_CORE_STATIC_TYPE; j++) {
			/* calc static ratio and transfer unit to uA */
			core_swpm_data_ptr->core_static_pwr[i][j] =
			scaled_p * core_static_rt[j] / CORE_STATIC_RT_RES;
		}
	}
}

int swpm_core_v6983_init(void)
{
	unsigned int offset;

	offset = swpm_set_and_get_cmd(0, 0, 0, CORE_CMD_TYPE);

	core_swpm_data_ptr = (struct core_swpm_data *)sspm_sbuf_get(offset);

	swpm_core_static_data_init();

	/* exception control for illegal sbuf request */
	if (!core_swpm_data_ptr) {
		pr_notice("swpm core share sram offset fail\n");
		return -1;
	}

	pr_notice("swpm core init offset = 0x%x\n", offset);
	return 0;
}

void swpm_core_v6983_exit(void)
{
}
