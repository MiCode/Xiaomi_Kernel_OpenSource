// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/random.h>
#include <linux/syscore_ops.h>
#include <linux/pm_qos.h>
#include <linux/cpufreq.h>
#include <linux/interconnect.h>
#include <mt-plat/dvfsrc-exp.h>
#include <linux/of.h>

static unsigned int s_cluster_num;
static unsigned int s_cluster_freq_rdy;
static int *s_target_freq;
static struct freq_qos_request *s_tchbst_rq;

/* CPU frequency adjust relate */
static void cpu_cluster_freq_tbl_init(void)
{
	unsigned int i;
	int cpu;
	struct cpufreq_policy *policy = NULL;

	/* query policy number */
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);

		if (policy) {
			s_cluster_num++;
			cpu = cpumask_last(policy->related_cpus);
		}
	}

	if (s_cluster_num == 0) {
		pr_info("ccci: spd: %s, no policy for cpu", __func__);
		return;
	}
	s_tchbst_rq = kcalloc(s_cluster_num, sizeof(struct freq_qos_request), GFP_KERNEL);
	if (s_tchbst_rq == NULL)
		return;

	s_target_freq = kcalloc(s_cluster_num, sizeof(int), GFP_KERNEL);
	if (s_target_freq)
		for (i = 0; i < s_cluster_num; i++)
			s_target_freq[i] = -1;
	else {
		pr_info("ccci: spd: %s s_target_freq fail\n", __func__);
		kfree(s_tchbst_rq);
		return;
	}

	i = 0;
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		if (i >= s_cluster_num) {
			pr_info("ccci: spd: %s fail: i >= s_cluster_num\n", __func__);
			kfree(s_tchbst_rq);
			return;
		}
		freq_qos_add_request(&policy->constraints, &(s_tchbst_rq[i]), FREQ_QOS_MIN, 0);
		cpu = cpumask_last(policy->related_cpus);
		i++;
	}

	s_cluster_freq_rdy = 1;
}

void mtk_ccci_qos_cpu_cluster_freq_update(int freq[], unsigned int num)
{
	unsigned int min_num = s_cluster_num > num ? num : s_cluster_num;
	unsigned int i;
	char buf[256];
	int ret = 0;
	int ret_t[8];
	int update = 0;

	if (!s_cluster_freq_rdy) {
		pr_info("ccci: spd: cpu cluster frequency not ready\n");
		return;
	}

	for (i = 0; i < min_num; i++) {
		ret_t[i] = 0;
		if (s_target_freq[i] != freq[i]) {
			s_target_freq[i] = freq[i];
			ret_t[i] = freq_qos_update_request(&s_tchbst_rq[i], s_target_freq[i]);
			update = 1;
		}
		ret += scnprintf(&buf[ret], sizeof(buf) - ret, "%d(%d),", freq[i], ret_t[i]);
	}
	if (update)
		pr_info("ccci: spd: %s [%u:%u]\n", buf, num, min_num);
}

/* DRAM qos */
static struct device_node *np_node;
static struct icc_path *net_icc_path;

void mtk_ccci_qos_dram_update(int lvl)
{
	static int curr_lvl = -1;
	unsigned int peak_bw;

	if (!net_icc_path || !np_node)
		return;
	if (curr_lvl != lvl) {
		curr_lvl = lvl;
		switch (lvl) {
		case 0:
			peak_bw = dvfsrc_get_required_opp_peak_bw(np_node, 1); /* <<< Note here */
			icc_set_bw(net_icc_path, 0, peak_bw);
			pr_info("ccci: spd: dram:0\n");
			break;
		case 1:
			peak_bw = dvfsrc_get_required_opp_peak_bw(np_node, 0); /* <<< Note here */
			icc_set_bw(net_icc_path, 0, peak_bw);
			pr_info("ccci: spd: dram: 1\n");
			break;
		case -1:
			icc_set_bw(net_icc_path, 0, 0);
			pr_info("ccci: spd: dram:-1\n");
			break;
		default:
			break;
		}
	}
}

static void qos_dram_init(struct device *dev)
{
	/* query interconnect parameters */
	if (!dev)
		return;
	net_icc_path = of_icc_get(dev, "icc-mdspd-bw");
	if (!net_icc_path)
		pr_info("ccci: spd: Get icc-mdspd-bw path fail\n");
	np_node = dev->of_node;
	if (!np_node) {
		pr_info("ccci: spd: No md driver in dtsi\n");
		return;
	}
}

void mtk_ccci_md_spd_qos_init(void *dev)
{
	cpu_cluster_freq_tbl_init();
	qos_dram_init((struct device *)dev);
}
