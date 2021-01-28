// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "cpuhp: " fmt

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <mtk_ppm_api.h>
#include <linux/nmi.h>

#include "mtk_cpuhp_private.h"

static struct cpumask ppm_online_cpus;
static struct task_struct *ppm_kthread;
static DEFINE_MUTEX(ppm_mutex);

#ifdef CONFIG_PM_SLEEP
static struct wakeup_source *hps_ws;
#endif

#ifdef CONFIG_ARM
#define CPU_DOWN	cpu_down
#define CPU_UP		cpu_up
#else
#define CPU_DOWN(i)	device_offline(get_cpu_device(i))
#define CPU_UP(i)	device_online(get_cpu_device(i))
#endif

#define HPS_RETRY	10

static int ppm_thread_fn(void *data)
{
	int request_cpu_up;
	int i;
	int rc;

	struct cpumask ppm_cpus_req;

	while (!kthread_should_stop()) {
		if (cpumask_equal(&ppm_online_cpus, cpu_online_mask)) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			continue;
		}

		set_current_state(TASK_RUNNING);

		cpumask_copy(&ppm_cpus_req, &ppm_online_cpus);

#ifdef CONFIG_PM_SLEEP
		if (hps_ws)
			__pm_stay_awake(hps_ws);
#endif

		pr_debug_ratelimited("%s: ppm_cpus_req: %*pbl cpu_online_mask: %*pbl\n"
			, __func__
			, cpumask_pr_args(&ppm_cpus_req)
			, cpumask_pr_args(cpu_online_mask));


		/* process the request of up each CPUs from PPM */
		for_each_possible_cpu(i) {
			request_cpu_up = cpumask_test_cpu(i, &ppm_cpus_req);

			if (request_cpu_up && cpu_is_offline(i)) {
				int retry = 0;

				pr_debug_ratelimited("CPU%d: ppm-request=%d, offline->powerup\n",
					 i, request_cpu_up);
Retry_ON:
				rc = CPU_UP(i);
				if (rc)	{
					if (retry > HPS_RETRY) {
						pr_debug_ratelimited(
							"fail to bringup cpu(%d) rc: %d\n"
							, i, rc);
						trigger_all_cpu_backtrace();
						continue;
					}
					retry++;
					goto Retry_ON;
				}
				continue;
			}
		}

		/* process the request of down each CPUs from PPM */
		for_each_possible_cpu(i) {
			request_cpu_up = cpumask_test_cpu(i, &ppm_cpus_req);

			if (!request_cpu_up && cpu_online(i)) {
				int retry = 0;

				pr_debug_ratelimited("CPU%d: ppm-request=%d, online->powerdown\n",
					 i, request_cpu_up);
Retry_OFF:
				rc = CPU_DOWN(i);
				if (rc) {
					if (retry > HPS_RETRY) {
						pr_debug_ratelimited(
							"fail to shutdown cpu(%d) rc: %d\n"
							, i, rc);
						trigger_all_cpu_backtrace();
						continue;
					}
					retry++;
					goto Retry_OFF;
				}
				continue;
			}
		}

#ifdef CONFIG_PM_SLEEP
		if (hps_ws)
			__pm_relax(hps_ws);
#endif

	}

	return 0;
}

static void ppm_limit_callback(struct ppm_client_req req)
{
	mutex_lock(&ppm_mutex);
	cpumask_copy(&ppm_online_cpus, &req.online_core[0]);
	mutex_unlock(&ppm_mutex);

	wake_up_process(ppm_kthread);
}


void ppm_notifier(void)
{
	unsigned int cpu;
	struct device_node *dn = 0;
	const char *smp_method = 0;

	cpumask_copy(&ppm_online_cpus, cpu_online_mask);

	for_each_present_cpu(cpu) {
		dn = of_get_cpu_node(cpu, NULL);
		smp_method = of_get_property(dn, "smp-method", NULL);
		if (smp_method != NULL) {
			if (!strcmp("disabled", smp_method)) {
				pr_info("[ENTER Hotplug DEBUG MODE!!!]\n");
				return;
			}
		}
	}

	/* register PPM callback */
	mt_ppm_register_client(PPM_CLIENT_HOTPLUG, &ppm_limit_callback);

	/* create a kthread to serve the requests from PPM */
	ppm_kthread = kthread_create(ppm_thread_fn, NULL, "cpuhp-ppm");
	if (IS_ERR(ppm_kthread)) {
		pr_notice("error creating ppm kthread (%ld)\n",
		       PTR_ERR(ppm_kthread));
		return;
	}

	hps_ws = wakeup_source_register(NULL, "hps");
	if (!hps_ws)
		pr_debug("hps wakelock register fail!\n");
}
