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
#include <swpm_mem_v6886.h>

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
/* index snapshot */
DEFINE_SPINLOCK(mem_swpm_spinlock);
struct mem_swpm_data mem_idx_snap;

/* share sram for swpm mem */
static struct mem_swpm_data *mem_swpm_data_ptr;

/* snapshot the last completed average index data */
static void mem_idx_snapshot(void)
{
	unsigned long flags;
	int i;

	if (mem_swpm_data_ptr) {
		/* directly copy due to 8 bytes alignment problem */
		spin_lock_irqsave(&mem_swpm_spinlock, flags);

		for (i = 0; i < MAX_EMI_NUM; i++) {
			mem_idx_snap.read_bw[i] =
				mem_swpm_data_ptr->read_bw[i];
			mem_idx_snap.write_bw[i] =
				mem_swpm_data_ptr->write_bw[i];
		}

		spin_unlock_irqrestore(&mem_swpm_spinlock, flags);
	}
}

static int mem_swpm_event(struct notifier_block *nb,
			  unsigned long event, void *v)
{
	switch (event) {
	case SWPM_LOG_DATA_NOTIFY:
		mem_idx_snapshot();
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block mem_swpm_notifier = {
	.notifier_call = mem_swpm_event,
};

int swpm_mem_v6886_init(void)
{
	unsigned int offset;

	offset = swpm_set_and_get_cmd(0, 0, 0, MEM_CMD_TYPE);

	mem_swpm_data_ptr = (struct mem_swpm_data *)sspm_sbuf_get(offset);

	/* exception control for illegal sbuf request */
	if (!mem_swpm_data_ptr) {
		pr_notice("swpm mem share sram offset fail\n");
		return -1;
	}

	pr_notice("swpm mem init offset = 0x%x\n", offset);
	swpm_register_event_notifier(&mem_swpm_notifier);

	return 0;
}

void swpm_mem_v6886_exit(void)
{
	swpm_unregister_event_notifier(&mem_swpm_notifier);
}
