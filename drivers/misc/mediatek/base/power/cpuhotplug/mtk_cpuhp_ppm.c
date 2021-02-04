/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#define pr_fmt(fmt) "cpuhp: " fmt

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

#include <mtk_ppm_api.h>

#include "mtk_cpuhp_private.h"

static struct cpumask ppm_online_cpus;
static struct task_struct *ppm_kthread;
static DEFINE_MUTEX(ppm_mutex);

static int ppm_thread_fn(void *data)
{
	int request_cpu_up;
	int i;

	while (!kthread_should_stop()) {
		if (cpumask_equal(&ppm_online_cpus, cpu_online_mask)) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			continue;
		}

		set_current_state(TASK_RUNNING);

		pr_debug("%s: ppm_online_cpus: %*pbl\n", __func__,
			 cpumask_pr_args(&ppm_online_cpus));


		/* process the request of up/down each CPUs from PPM */
		for_each_possible_cpu(i) {
			request_cpu_up = cpumask_test_cpu(i, &ppm_online_cpus);

			if (request_cpu_up && cpu_is_offline(i)) {
				pr_debug("CPU%d: ppm-request=%d, offline->powerup\n",
					 i, request_cpu_up);
				device_online(get_cpu_device(i));
				continue;
			}

			if (!request_cpu_up && cpu_online(i)) {
				pr_debug("CPU%d: ppm-request=%d, online->powerdown\n",
					 i, request_cpu_up);
				device_offline(get_cpu_device(i));
				continue;
			}
		}
	}

	return 0;
}

#if !defined(CONFIG_FPGA_EARLY_PORTING)
static void ppm_limit_callback(struct ppm_client_req req)
{
	mutex_lock(&ppm_mutex);
	cpumask_copy(&ppm_online_cpus, &req.online_core[0]);
	mutex_unlock(&ppm_mutex);

	wake_up_process(ppm_kthread);
}
#endif

void ppm_notifier(void)
{
	cpumask_copy(&ppm_online_cpus, cpu_online_mask);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	/* register PPM callback */
	mt_ppm_register_client(PPM_CLIENT_HOTPLUG, &ppm_limit_callback);
#endif

	/* create a kthread to serve the requests from PPM */
	ppm_kthread = kthread_create(ppm_thread_fn, NULL, "cpuhp-ppm");
	if (IS_ERR(ppm_kthread)) {
		pr_notice("error creating ppm kthread (%ld)\n",
		       PTR_ERR(ppm_kthread));
		return;
	}
}
