/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <asm/arch_timer.h>

#ifdef CONFIG_PRINT_RPM_CLK_STATUS
#include <linux/syscore_ops.h>
#endif

#define SCLK_HZ 32768
#define MSM_ARCH_TIMER_FREQ 19200000
#define NUM_STATS_RECORD 2
#define STATS_OFFSET 0x14
#define HEAP_OFFSET 0x1c
#define MASTER_START_ADDR_OFFSET 0x150
#define MASTER_OFFSET_ADDRESS 0x1000

struct rpm_system_stats {
	void __iomem *rpm_stats_addr;
	void __iomem *rpm_master_addr;
	int num_masters;
	char **master;
} ss;

struct msm_rpmstats_record {
	char name[32];
	uint32_t id;
	uint32_t val;
};

struct msm_rpm_stats_data {
	uint32_t stat_type;
	uint32_t count;
	uint64_t last_entered_at;
	uint64_t last_exited_at;
	uint64_t accumulated;
	uint32_t client_votes;
	uint32_t reserved[3];
};

struct rpm_master_stats_data {
	uint32_t active_cores;
	uint32_t numshutdowns;
	uint64_t shutdown_req;
	uint64_t wakeup_ind;
	uint64_t bringup_req;
	uint64_t bringup_ack;
	uint32_t wakeup_reason; /* 0 = rude wakeup, 1 = scheduled wakeup */
	uint32_t last_sleep_transition_duration;
	uint32_t last_wake_transition_duration;
	uint32_t xo_count;
	uint64_t xo_last_entered_at;
	uint64_t xo_last_exited_at;
	uint64_t xo_accumulated_duration;
};

static inline uint32_t msm_rpmstats_read_long_register(void __iomem *regbase,
		int index, int offset)
{
	return readl_relaxed(regbase + offset +
			index * sizeof(struct msm_rpm_stats_data));
}

static inline uint64_t msm_rpmstats_read_quad_register(void __iomem *regbase,
		int index, int offset)
{
	uint64_t dst;

	memcpy_fromio(&dst,
		regbase + offset + index * sizeof(struct msm_rpm_stats_data),
		8);
	return dst;
}

static inline unsigned long  msm_rpmstats_read_register(void __iomem *regbase,
		int index, int offset)
{
	return  readl_relaxed(regbase + index * 12 + (offset + 1) * 4);
}

static inline uint64_t get_time_in_msec(u64 counter)
{
	do_div(counter, MSM_ARCH_TIMER_FREQ);
	counter *= MSEC_PER_SEC;
	return counter;
}

static inline uint64_t get_time_in_sec(u64 counter)
{
	do_div(counter, MSM_ARCH_TIMER_FREQ);
	return counter;
}

static void rpm_stats_copy_data(struct msm_rpm_stats_data *data, int idx)
{
	void __iomem *reg = ss.rpm_stats_addr;

	data->stat_type = msm_rpmstats_read_long_register(reg, idx,
			offsetof(struct msm_rpm_stats_data, stat_type));

	data->count = msm_rpmstats_read_long_register(reg, idx,
			offsetof(struct msm_rpm_stats_data, count));
	data->last_entered_at = msm_rpmstats_read_quad_register(reg,
			idx, offsetof(struct msm_rpm_stats_data,
				last_entered_at));
	data->last_exited_at = msm_rpmstats_read_quad_register(reg,
			idx, offsetof(struct msm_rpm_stats_data,
				last_exited_at));

	data->accumulated = msm_rpmstats_read_quad_register(reg,
			idx, offsetof(struct msm_rpm_stats_data,
				accumulated));
	data->client_votes = msm_rpmstats_read_long_register(reg,
			idx, offsetof(struct msm_rpm_stats_data,
				client_votes));
}

static int rpm_stats_write_buf(struct seq_file *m)
{
	struct msm_rpm_stats_data rs;
	int i;

	for (i = 0; i < NUM_STATS_RECORD; i++) {
		char stat_type[5] = {0};
		uint64_t time;

		rpm_stats_copy_data(&rs, i);
		memcpy(stat_type, &rs.stat_type, sizeof(uint32_t));
		seq_printf(m, "RPM Mode:%s\n", stat_type);
		seq_printf(m, "\tcount:%d\n", rs.count);

		time = rs.last_exited_at - rs.last_entered_at;
		time = get_time_in_msec(time);
		seq_printf(m, "\ttime in last mode(msec):%llu\n", time);

		time = arch_counter_get_cntvct() - rs.last_exited_at;
		time = get_time_in_sec(time);
		seq_printf(m, "\ttime since last mode(sec):%llu\n", time);

		time = get_time_in_msec(rs.accumulated);
		seq_printf(m, "\tactual last sleep(msec):%llu\n", time);

		seq_printf(m, "\tclient votes: %#010x\n\n", rs.client_votes);
	}

	return 0;
}

static void master_stats_copy_data(struct rpm_master_stats_data *data, int idx)
{
	data->shutdown_req = readq_relaxed(ss.rpm_master_addr +
			idx * MASTER_OFFSET_ADDRESS +
			offsetof(struct rpm_master_stats_data, shutdown_req));

	data->wakeup_ind = readq_relaxed(ss.rpm_master_addr +
			(idx * MASTER_OFFSET_ADDRESS +
			 offsetof(struct rpm_master_stats_data, wakeup_ind)));

	data->bringup_req = readq_relaxed(ss.rpm_master_addr +
			(idx * MASTER_OFFSET_ADDRESS +
			 offsetof(struct rpm_master_stats_data, bringup_req)));

	data->bringup_ack = readq_relaxed(ss.rpm_master_addr +
			(idx * MASTER_OFFSET_ADDRESS +
			 offsetof(struct rpm_master_stats_data, bringup_ack)));

	data->xo_last_entered_at = readq_relaxed(ss.rpm_master_addr +
			(idx * MASTER_OFFSET_ADDRESS +
			 offsetof(struct rpm_master_stats_data,
				 xo_last_entered_at)));

	data->xo_last_exited_at = readq_relaxed(ss.rpm_master_addr +
			(idx * MASTER_OFFSET_ADDRESS +
			 offsetof(struct rpm_master_stats_data,
				 xo_last_exited_at)));

	data->xo_accumulated_duration =
		readq_relaxed(ss.rpm_master_addr +
				(idx * MASTER_OFFSET_ADDRESS +
				 offsetof(struct rpm_master_stats_data,
					 xo_accumulated_duration)));

	data->last_sleep_transition_duration =
		readl_relaxed(ss.rpm_master_addr +
				(idx * MASTER_OFFSET_ADDRESS +
				 offsetof(struct rpm_master_stats_data,
					 last_sleep_transition_duration)));

	data->last_wake_transition_duration =
		readl_relaxed(ss.rpm_master_addr +
				(idx * MASTER_OFFSET_ADDRESS +
				 offsetof(struct rpm_master_stats_data,
					 last_wake_transition_duration)));

	data->xo_count =
		readl_relaxed(ss.rpm_master_addr +
				(idx * MASTER_OFFSET_ADDRESS +
				 offsetof(struct rpm_master_stats_data,
					 xo_count)));

	data->wakeup_reason = readl_relaxed(ss.rpm_master_addr +
			(idx * MASTER_OFFSET_ADDRESS +
			 offsetof(struct rpm_master_stats_data,
				 wakeup_reason)));

	data->numshutdowns = readl_relaxed(ss.rpm_master_addr +
			(idx * MASTER_OFFSET_ADDRESS +
			 offsetof(struct rpm_master_stats_data, numshutdowns)));

	data->active_cores = readl_relaxed(ss.rpm_master_addr +
			(idx * MASTER_OFFSET_ADDRESS) +
			offsetof(struct rpm_master_stats_data, active_cores));

}

static int master_stats_write_buf(struct seq_file *m)
{
	struct rpm_master_stats_data ms;
	int i;

	for (i = 0; i < ss.num_masters; i++) {

		master_stats_copy_data(&ms, i);

		seq_printf(m, "%s\n", ss.master[i]);
		seq_printf(m, "\tShutdown Request:0x%llX\n", ms.shutdown_req);
		seq_printf(m, "\tWakeup interrupt:0x%llX\n", ms.wakeup_ind);
		seq_printf(m, "\tBringup Req::0x%llX\n", ms.bringup_req);
		seq_printf(m, "\tBringup Ack:0x%llX\n", ms.bringup_ack);
		seq_printf(m, "\tLast XO Entry:0x%llX\n",
				ms.xo_last_entered_at);
		seq_printf(m, "\tLast XO Exit:0x%llX\n", ms.xo_last_exited_at);
		seq_printf(m, "\tAccumulated XO duration:0x%llX\n",
			ms.xo_accumulated_duration);
		seq_printf(m, "\tLast sleep transition duration:0x%x\n",
			ms.last_sleep_transition_duration);
		seq_printf(m, "\tLast wake transition duration:0x%x\n",
			ms.last_wake_transition_duration);
		seq_printf(m, "\tXO Count:0x%x\n", ms.xo_count);
		seq_printf(m, "\tWakeup Reason:0x%s\n",
			ms.wakeup_reason ? "Sched" : "Rude");
		seq_printf(m, "\tNum Shutdowns:0x%x\n", ms.numshutdowns);
		seq_printf(m, "\tActive Cores:0x%x\n", ms.active_cores);
	}
	return 0;
}

static int ss_file_show(struct seq_file *m, void *v)
{
	int ret = 0;

	ret = rpm_stats_write_buf(m);
	if (ret)
		return ret;

	return master_stats_write_buf(m);
}

static int ss_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, ss_file_show, inode->i_private);
}

static const struct file_operations ss_file_ops = {
	.owner = THIS_MODULE,
	.open = ss_file_open,
	.read = seq_read,
	.llseek = no_llseek,
	.release = single_release,
};

static int msm_rpmstats_probe(struct platform_device *pdev)
{
	struct dentry *dent = NULL;
	struct device_node *node = NULL;
	uint32_t offset = 0;
	void __iomem *offset_addr = NULL;
	struct resource res;
	int i, ret = 0;

	if (!pdev)
		return -EINVAL;

	node = of_parse_phandle(pdev->dev.of_node, "qcom,rpm-msg-ram", 0);
	if (!node)
		return -EINVAL;

	ret = of_address_to_resource(node, 1, &res);
	if (ret)
		return ret;

	offset_addr = ioremap_nocache(res.start + STATS_OFFSET,
			resource_size(&res));
	if (!offset_addr)
		return -ENOMEM;

	offset = readl_relaxed(offset_addr);
	iounmap(offset_addr);

	ret = of_address_to_resource(node, 0, &res);
	if (ret)
		return ret;

	ss.rpm_stats_addr = devm_ioremap_nocache(&pdev->dev,
			res.start + offset,
			resource_size(&res));

	if (!ss.rpm_stats_addr)
		return -ENOMEM;

	node = of_parse_phandle(pdev->dev.of_node, "qcom,rpm-code-ram", 0);
	if (!node)
		return -EINVAL;

	ret = of_address_to_resource(node, 0, &res);
	if (ret)
		return ret;

	ss.rpm_master_addr = devm_ioremap_nocache(
			&pdev->dev, res.start + MASTER_START_ADDR_OFFSET,
			resource_size(&res));

	if (!ss.rpm_master_addr)
		return -ENOMEM;

	ss.num_masters = of_property_count_strings(pdev->dev.of_node,
			"qcom,masters");
	if (ss.num_masters < 0) {
		dev_err(&pdev->dev, "Failed to get number of masters =%d\n",
						ss.num_masters);
		return -EINVAL;
	}

	ss.master = devm_kzalloc(&pdev->dev,
			sizeof(char *) * ss.num_masters, GFP_KERNEL);

	if (!ss.master) {
		dev_err(&pdev->dev, "%s:Failed to allocated memory\n",
				__func__);
		return -ENOMEM;
	}

	/*
	 * Read master names from DT
	 */
	for (i = 0; i < ss.num_masters; i++) {
		const char *master_name;

		of_property_read_string_index(pdev->dev.of_node,
				"qcom,masters",
				i, &master_name);
		ss.master[i] = devm_kzalloc(&pdev->dev,
				sizeof(char) * strlen(master_name) + 1,
				GFP_KERNEL);
		if (!ss.master[i]) {
			pr_err("%s:Failed to get memory\n", __func__);
			return -ENOMEM;
		}
		strlcpy(ss.master[i], master_name,
					strlen(master_name) + 1);
	}

	dent = debugfs_create_file("system_stats", S_IRUGO, NULL,
			&ss, &ss_file_ops);

	if (!dent) {
		pr_err("%s: ERROR rpm_stats debugfs_create_file fail\n",
				__func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, dent);
	return 0;
}

static int msm_rpmstats_remove(struct platform_device *pdev)
{
	struct dentry *dent;

	dent = platform_get_drvdata(pdev);
	debugfs_remove(dent);
	return 0;
}

static const struct of_device_id rpm_stats_table[] = {
	       {.compatible = "qcom,system-stats"},
	       {},
};

static struct platform_driver msm_system_stats_driver = {
	.probe = msm_rpmstats_probe,
	.remove = msm_rpmstats_remove,
	.driver = {
		.name = "msm_stat",
		.owner = THIS_MODULE,
		.of_match_table = rpm_stats_table,
	},
};

#ifdef CONFIG_PRINT_RPM_CLK_STATUS
static u32 debug_sysstats = 1;
static void msm_sysstats_resume(void)
{
	struct msm_rpm_stats_data rs;
	struct rpm_master_stats_data ms;
	int m;
	int n;

	if (unlikely(!debug_sysstats))
		return;

	for (m = 0; m < NUM_STATS_RECORD; m++) {
		char stat_type[5] = {0};

		rpm_stats_copy_data(&rs, m);
		memcpy(stat_type, &rs.stat_type, sizeof(uint32_t));
		pr_info("RPM Mode:%s\n", stat_type);
		pr_info("count:%d\n", rs.count);
	}

	for (n = 0; n < ss.num_masters; n++) {
		master_stats_copy_data(&ms, n);
		pr_info("%s\n", ss.master[n]);
		pr_info(" Wakeup interrupt:0x%llX\n", ms.wakeup_ind);
		pr_info(" XO Count:0x%x\n", ms.xo_count);
		pr_info(" Wakeup Reason:0x%s\n", ms.wakeup_reason ? "Sched" : "Rude");
		pr_info(" Num Shutdowns:0x%x\n", ms.numshutdowns);
		pr_info(" Active Cores:0x%x\n", ms.active_cores);
	}
}

static struct syscore_ops msm_sysstats_ops = {
	.suspend	= NULL,
	.resume		= msm_sysstats_resume,
};

static int __init msm_sysstats_syscore_init(void)
{
	register_syscore_ops(&msm_sysstats_ops);

	return 0;
}
device_initcall(msm_sysstats_syscore_init);
#endif

module_platform_driver(msm_system_stats_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM Statistics driver");
MODULE_ALIAS("platform:stat_log");
