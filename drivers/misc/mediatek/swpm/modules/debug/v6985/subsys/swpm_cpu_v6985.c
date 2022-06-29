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
#include <swpm_cpu_v6985.h>

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
/* share sram for swpm cpu data */
static struct cpu_swpm_data *cpu_swpm_data_ptr;

static void update_cpu_temp(void)
{
	unsigned int temp = 30000, i;

	for (i = 0; i < NR_CPU_CORE; i++) {
#if IS_ENABLED(CONFIG_MTK_THERMAL)
		temp = get_cpu_temp(i);
		if (temp > 100000)
			temp = 100000;
#endif
		if (cpu_swpm_data_ptr)
			cpu_swpm_data_ptr->cpu_temp[i] = (unsigned int)temp;
	}
}

static int cpu_swpm_event(struct notifier_block *nb,
			  unsigned long event, void *v)
{
	switch (event) {
	case SWPM_LOG_DATA_NOTIFY:
		update_cpu_temp();
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block cpu_swpm_notifier = {
	.notifier_call = cpu_swpm_event,
};

int swpm_cpu_v6985_init(void)
{
	unsigned int offset;

	offset = swpm_set_and_get_cmd(0, 0, CPU_GET_ADDR, CPU_CMD_TYPE);

	cpu_swpm_data_ptr = (struct cpu_swpm_data *)
		sspm_sbuf_get(offset);

	/* exception control for illegal sbuf request */
	if (!cpu_swpm_data_ptr) {
		pr_notice("swpm cpu share sram offset fail\n");
		return -1;
	}

	pr_notice("swpm cpu init offset = 0x%x\n", offset);
	swpm_register_event_notifier(&cpu_swpm_notifier);

	return 0;
}

void swpm_cpu_v6985_exit(void)
{
	swpm_unregister_event_notifier(&cpu_swpm_notifier);
}
