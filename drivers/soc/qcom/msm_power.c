/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"msm_power: %s: " fmt, __func__

#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <trace/events/power.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk/msm-clk.h>
#include <soc/qcom/rpm-smd.h>

#define MSM_POWER_DEV_NAME "qcom,msm-power"
#define GPU_CLK_NAME "gpu_clk"
#define DDR_RSC_TYPE	0x326b6c63
#define DDR_RSC_ID	0x0
#define MAX_KEY		0x0078616d

#define DDR_LO_FREQ 1550000
#define DDR_HI_FREQ 0

static unsigned int cur_cpu_freq[NR_CPUS];
static unsigned int max_cpu_freq;
static unsigned int cpu_freq_threshold = 1400000;
static unsigned int cpu_ddr_vote = DDR_HI_FREQ;
static unsigned int system_max_cpu_freq;
static unsigned int cur_gpu_freq;
static unsigned int gpu_freq_threshold = 500000;
static unsigned int gpu_ddr_vote = DDR_LO_FREQ;
static unsigned int system_max_gpu_freq;
static unsigned int max_ddr_freq = DDR_HI_FREQ;
static unsigned int prev_max_ddr_freq = DDR_HI_FREQ;

static DEFINE_MUTEX(ddr_freq_lock);

/*******************************sysfs start***********************************/
static int set_cpu_freq_threshold(const char *buf,
		const struct kernel_param *kp)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	if (val > system_max_cpu_freq) {
		pr_debug("Ignored %u since system max cpu freq is %u\n",
				val, system_max_cpu_freq);
		return -EINVAL;
	}

	cpu_freq_threshold = val;
	pr_debug("cpu_freq_threshold updated to %u\n", cpu_freq_threshold);

	return 0;
}

static int get_cpu_freq_threshold(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%u", cpu_freq_threshold);
}

static const struct kernel_param_ops param_ops_cpu_freq_threshold = {
	.set = set_cpu_freq_threshold,
	.get = get_cpu_freq_threshold,
};
device_param_cb(cpu_freq_threshold, &param_ops_cpu_freq_threshold, NULL, 0644);

static int set_gpu_freq_threshold(const char *buf,
		const struct kernel_param *kp)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	if (val > system_max_gpu_freq) {
		pr_debug("Ignored %u since system max gpu freq is %u\n",
				val, system_max_gpu_freq);
		return -EINVAL;
	}

	gpu_freq_threshold = val;
	pr_debug("gpu_freq_threshold updated to %u\n", gpu_freq_threshold);

	return 0;
}

static int get_gpu_freq_threshold(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%u", gpu_freq_threshold);
}

static const struct kernel_param_ops param_ops_gpu_freq_threshold = {
	.set = set_gpu_freq_threshold,
	.get = get_gpu_freq_threshold,
};
device_param_cb(gpu_freq_threshold, &param_ops_gpu_freq_threshold, NULL, 0644);
/********************************sysfs end************************************/

/* Sends message to RPM to limit DDR frequency to @ddr_freq */
static int msm_rpm_limit_ddr_freq(uint32_t ddr_freq)
{
	int rc = 0;

	struct msm_rpm_kvp kvp = {
		.key = MAX_KEY,
		.data = (void *)&ddr_freq,
		.length = sizeof(ddr_freq),
	};

	pr_debug("RPM Msg: type=0x%08x, id=%u, key=0x%08x, data=%u, len=%lu\n",
			DDR_RSC_TYPE, DDR_RSC_ID, MAX_KEY, ddr_freq,
			sizeof(ddr_freq));
	rc = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET, DDR_RSC_TYPE,
			DDR_RSC_ID, &kvp, 1);

	return rc;
}

/* Updates max DDR freq based on votes from CPU and GPU */
static int update_max_ddr_freq(void)
{
	int rc = 0;

	pr_debug("cpu_ddr_vote = %u and gpu_ddr_vote = %u\n",
			cpu_ddr_vote, gpu_ddr_vote);
	prev_max_ddr_freq = max_ddr_freq;
	if (cpu_ddr_vote == DDR_LO_FREQ && gpu_ddr_vote == DDR_LO_FREQ)
		max_ddr_freq = DDR_LO_FREQ;
	else
		max_ddr_freq = DDR_HI_FREQ;

	/* Update max_ddr_freq if it changed */
	if (prev_max_ddr_freq != max_ddr_freq) {
		pr_debug("prev_max_ddr_freq = %u; max_ddr_freq = %u\n",
				prev_max_ddr_freq, max_ddr_freq);
		rc = msm_rpm_limit_ddr_freq(max_ddr_freq);
		pr_debug("Called msm_rpm_limit_ddr_freq and got ret: %d", rc);
		trace_msmpower_max_ddr(prev_max_ddr_freq, max_ddr_freq);
	}

	return rc;
}

/* Notifier callback for CPU frequency changes */
static int power_cpufreq_trans_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	int i;
	struct cpufreq_freqs *freq = data;

	if (event != CPUFREQ_POSTCHANGE)
		return 0;

	mutex_lock(&ddr_freq_lock);

	/* Recalculate max only if frequency changed */
	if (cur_cpu_freq[freq->cpu] != freq->new) {
		cur_cpu_freq[freq->cpu] = freq->new;
		max_cpu_freq = 0;
		for (i = 0; i < num_possible_cpus(); i++)
			max_cpu_freq = max(max_cpu_freq, cur_cpu_freq[i]);
	}

	if ((max_cpu_freq <= cpu_freq_threshold) &&
			(cpu_ddr_vote == DDR_HI_FREQ)) {
		cpu_ddr_vote = DDR_LO_FREQ;
		update_max_ddr_freq();
	} else if ((max_cpu_freq > cpu_freq_threshold) &&
			(cpu_ddr_vote == DDR_LO_FREQ)) {
		cpu_ddr_vote = DDR_HI_FREQ;
		update_max_ddr_freq();
	}

	mutex_unlock(&ddr_freq_lock);
	return 0;
}

static struct notifier_block power_trans_nb = {
	.notifier_call = power_cpufreq_trans_notifier
};

/* Notifier callback for GPU frequency changes */
static int power_gpu_clk_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct msm_clk_notifier_data *clk_data = data;

	if (event != POST_RATE_CHANGE)
		return 0;

	mutex_lock(&ddr_freq_lock);

	cur_gpu_freq = clk_data->new_rate / 1000;

	if ((cur_gpu_freq <= gpu_freq_threshold) &&
			(gpu_ddr_vote == DDR_HI_FREQ)) {
		gpu_ddr_vote = DDR_LO_FREQ;
		update_max_ddr_freq();
	} else if ((cur_gpu_freq > gpu_freq_threshold) &&
			(gpu_ddr_vote == DDR_LO_FREQ)) {
		gpu_ddr_vote = DDR_HI_FREQ;
		update_max_ddr_freq();
	}

	mutex_unlock(&ddr_freq_lock);
	return 0;
}

static struct notifier_block power_gpu_clk_nb = {
	.notifier_call = power_gpu_clk_notifier
};

static struct of_device_id msm_power_match_table[] = {
	{ .compatible = MSM_POWER_DEV_NAME },
	{}
};

static int msm_power_probe(struct platform_device *pdev)
{
	int cpu, rc = 0;
	struct cpufreq_policy policy;
	struct device *dev = &(pdev->dev);
	struct clk *gpu_clk;

	/* get GPU clock */
	gpu_clk = clk_get(dev, GPU_CLK_NAME);
	if (IS_ERR(gpu_clk)) {
		rc = PTR_ERR(gpu_clk);
		pr_err("Error: Could not get GPU clock resource\n");
		gpu_clk = NULL;
		goto out;
	}

	system_max_gpu_freq = clk_round_rate(gpu_clk, ULONG_MAX) / 1000;

	for_each_possible_cpu(cpu) {
		cpufreq_get_policy(&policy, cpu);
		system_max_cpu_freq = max(system_max_cpu_freq,
				policy.cpuinfo.max_freq);
	}

	cpufreq_register_notifier(&power_trans_nb, CPUFREQ_TRANSITION_NOTIFIER);

	rc = msm_clk_notif_register(gpu_clk, &power_gpu_clk_nb);

out:
	return rc;
}

static struct platform_driver msm_power_driver = {
	.probe = msm_power_probe,
	.driver = {
		.name = "qcom,msm-power",
		.owner = THIS_MODULE,
		.of_match_table = msm_power_match_table,
	},
};

static int __init msm_power_init(void)
{
	return platform_driver_register(&msm_power_driver);
}

static void __exit msm_power_exit(void)
{
	return platform_driver_unregister(&msm_power_driver);
}

module_init(msm_power_init);
module_exit(msm_power_exit);
