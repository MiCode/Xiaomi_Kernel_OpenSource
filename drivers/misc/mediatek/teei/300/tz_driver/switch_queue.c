/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/freezer.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <fdrv.h>
#include <linux/vmalloc.h>
#include "switch_queue.h"
#include "utdriver_macro.h"
#include "sched_status.h"
#include "teei_log.h"
#include "teei_common.h"
#include "teei_client_main.h"
#include "backward_driver.h"
#include "irq_register.h"
#include "teei_smc_call.h"
#include "teei_cancel_cmd.h"
#include "tz_log.h"

#include "teei_task_link.h"

#include <notify_queue.h>
#include <teei_secure_api.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

struct completion teei_switch_comp;

#ifdef CONFIG_MICROTRUST_DYNAMIC_CORE
static int last_cpu_id;
#endif

static int add_entry_to_task_link(struct task_entry_struct *entry)
{
	int retVal = 0;

	retVal = teei_add_to_task_link(&(entry->c_link));

	return retVal;
}


int add_work_entry(unsigned long long work_type, unsigned long long x0,
		unsigned long long x1, unsigned long long x2,
		unsigned long long x3)
{
	struct task_entry_struct *task_entry = NULL;
	int retVal = 0;

	task_entry = vmalloc(sizeof(struct task_entry_struct));
	if (task_entry == NULL) {
		IMSG_ERROR("Failed to vmalloc task_entry_struct!\n");
		return -ENOMEM;
	}

	memset(task_entry, 0, sizeof(struct task_entry_struct));

	task_entry->work_type = work_type;
	task_entry->x0 = x0;
	task_entry->x1 = x1;
	task_entry->x2 = x2;
	task_entry->x3 = x3;

	INIT_LIST_HEAD(&(task_entry->c_link));

	retVal = add_entry_to_task_link(task_entry);
	if (retVal != 0) {
		IMSG_ERROR("Failed to insert the entry to link!\n");
		vfree(task_entry);
		return retVal;
	}

	return retVal;
}

#ifdef CONFIG_MICROTRUST_DYNAMIC_CORE
static int teei_bind_current_cpu(void)
{
	struct cpumask mask = { CPU_BITS_NONE };
	int cpu_id = 0;

	/* Get current CPU ID */
	cpu_id = smp_processor_id();

	cpumask_clear(&mask);
	cpumask_set_cpu(cpu_id, &mask);
	set_cpus_allowed_ptr(teei_switch_task, &mask);

	set_current_cpuid(cpu_id);

	if (last_cpu_id != cpu_id) {
		teei_move_cpu_context(cpu_id, last_cpu_id);
		last_cpu_id = cpu_id;
	}

	return 0;
}

static int teei_bind_all_cpu(void)
{
	struct cpumask mask = { CPU_BITS_NONE };

	/* Set the affinity of teei_switch_task to all CPUs */

	if (switch_input_index == switch_output_index) {
		cpumask_clear(&mask);
		cpumask_setall(&mask);
		set_cpus_allowed_ptr(teei_switch_task, &mask);
	}

	return 0;
}
#endif

static int handle_one_switch_task(struct task_entry_struct *entry)
{
	int retVal = 0;

	switch (entry->work_type) {
	case SMC_CALL_TYPE:
		retVal = teei_smc(entry->x0, entry->x1, entry->x2, entry->x3);
		break;
#ifndef CONFIG_MICROTRUST_DYNAMIC_CORE
	case SWITCH_CORE_TYPE:
		retVal = handle_switch_core((int)(entry->x0));
		break;
#endif
	default:
		retVal = -EINVAL;
		break;
	}

	return retVal;
}


static int handle_all_switch_task(void)
{
	struct task_entry_struct *entry = NULL;
	struct tz_driver_state *s = get_tz_drv_state();
	int retVal = 0;

	while (1) {
		entry = teei_get_task_from_link();
		if (entry == NULL)
			return 0;

		retVal = handle_one_switch_task(entry);
		if (retVal != 0) {
			IMSG_ERROR("Failed to handle the task %d\n", retVal);
			return retVal;
		}

		vfree(entry);

		teei_notify_log_fn();
	}

	return 0;
}

int teei_switch_fn(void *work)
{
	int retVal = 0;

	while (1) {
		/*
		 * Block the switch thread and
		 * wait for the new task
		 */
		retVal = wait_for_completion_interruptible(&teei_switch_comp);
		if (retVal != 0)
			continue;

		/*
		 * Check if the task link is empty
		 */
		retVal = is_teei_task_link_empty();
		if (retVal == 1)
			continue;

#ifdef CONFIG_MICROTRUST_DYNAMIC_CORE
		/* Bind the teei switch thread to current CPU */
		retVal = teei_bind_current_cpu();
		if (retVal != 0) {
			IMSG_ERROR("TEEI: Failed to bind current CPU!\n");
			return retVal;
		}
#endif

		retVal = handle_all_switch_task();
		if (retVal != 0) {
			IMSG_ERROR("TEEI: Failed to handle the task link!\n");
			return retVal;
		}

#ifdef CONFIG_MICROTRUST_DYNAMIC_CORE
		retVal = teei_bind_all_cpu();
		if (retVal != 0) {
			IMSG_ERROR("TEEI: Failed to bind all CPUs!\n");
			return retVal;
		}
#endif
	}

	IMSG_PRINTK("TEEI: teei_switch_thread will be exit! (%d)\n", retVal);

	return retVal;
}

int teei_notify_switch_fn(void)
{
	complete(&teei_switch_comp);

	return 0;
}

int init_teei_switch_comp(void)
{
	init_completion(&teei_switch_comp);

	return 0;
}
