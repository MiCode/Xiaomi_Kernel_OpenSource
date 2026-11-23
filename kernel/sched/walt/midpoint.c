// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/string.h>

#include "../../../drivers/misc/hwid/hwid.h"

#define NO_TIMEOUT -1U
#define MIDPOINT_DEFAULT_QOS_TIMEOUT_MS 10000U
static int midpoint_freq = 1900000;
static int qos_timeout_ms = NO_TIMEOUT;

struct project_freq_pair {
	uint32_t project_id;
	int freq;
};

static struct project_freq_pair hardware_projects_freq[] = {
	{HARDWARE_PROJECT_P2, 2500000},
	{HARDWARE_PROJECT_P3, 2500000},
	{HARDWARE_PROJECT_Q200, 2500000},
	{HARDWARE_PROJECT_P1, 2500000},
	{HARDWARE_PROJECT_P11U, 2500000},
};

module_param(midpoint_freq, int, 0400);
module_param(qos_timeout_ms, int, 0400);

static DEFINE_PER_CPU(struct freq_qos_request, qos_max_req);
static DEFINE_PER_CPU(struct freq_qos_request, qos_min_req);
static struct delayed_work qos_remove_work;

static void midpoint_qos_remove(void){
	struct freq_qos_request *req;
	int cpu;

	for_each_possible_cpu(cpu) {
		req = &per_cpu(qos_min_req, cpu);
		freq_qos_remove_request(req);
	}

	pr_info("Removed midpoint min qos.\n");
	return;
}

static void midpoint_qos_remove_work(struct work_struct *work)
{
	midpoint_qos_remove();

	return;
}

void midpoint_init(void)
{
	struct cpufreq_policy *policy;
	struct freq_qos_request *req;
	int cpu, ret;

	// Distinguish projects
	uint32_t hw_platform_ver = 0;
	hw_platform_ver = get_hw_version_platform();
	pr_err("%s: This hardware is %d, midpoint is loading...\n", __func__, hw_platform_ver);

	for (int i = 0; i < ARRAY_SIZE(hardware_projects_freq); i++) {
		if (hw_platform_ver == hardware_projects_freq[i].project_id) {
			midpoint_freq = hardware_projects_freq[i].freq;
			pr_err("%s: It's hardware: %d boot, midpoint freq = %d\n", __func__, hw_platform_ver, midpoint_freq);
			break;
		}
	}

	for_each_possible_cpu(cpu) {

		while (1) {
			policy = cpufreq_cpu_get(cpu);
			if (policy && policy->governor && strlen(policy->governor->name) != 0) {
				if (!strcmp(policy->governor->name, "powersave")) {
					pr_err("%s: Cpufreq policy is powersave for cpu %d\n", __func__, cpu);
					break;
				} else {
					pr_err("%s: It's normal boot for cpu %d\n", __func__, cpu);
					return;
				}
			}
			pr_err("%s: Cpufreq policy not found for cpu %d\n", __func__, cpu);
			msleep(500);
		}

retry_max:
		req = &per_cpu(qos_max_req, cpu);
		ret = freq_qos_add_request(&policy->constraints, req,
						FREQ_QOS_MAX, midpoint_freq);

		if (ret < 0) {
			pr_err("%s: Failed to set max freq on cpu = %d\n",
					__func__, cpu);
			msleep(500);
			goto retry_max;
		}

retry_min:
		req = &per_cpu(qos_min_req, cpu);
		ret = freq_qos_add_request(&policy->constraints, req,
						FREQ_QOS_MIN, midpoint_freq);

		if (ret < 0) {
			pr_err("%s: Failed to set min freq on cpu = %d\n",
					__func__, cpu);
			msleep(500);
			goto retry_min;
		}

		cpufreq_cpu_put(policy);
		pr_err("%s: Put midpoint freq = %d on cpu = %d\n", __func__, midpoint_freq, cpu);
	}

	if (qos_timeout_ms != NO_TIMEOUT) {
		INIT_DELAYED_WORK(&qos_remove_work, midpoint_qos_remove_work);
		schedule_delayed_work(&qos_remove_work, msecs_to_jiffies(qos_timeout_ms));
	}

	pr_info("Added midpoint qos with freq = %d qos_timeout_ms=%d.\n", midpoint_freq, qos_timeout_ms);
	return;
}
