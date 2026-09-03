// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2016-2024 Unisoc (Shanghai) Technologies Co., Ltd
 */

#include <linux/cpu_pm.h>
#include <linux/cpufreq.h>
#include <linux/printk.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/time.h>
#include <trace/events/power.h>
#include "unisoc_cpuidle_info.h"
#include "unisoc_pnp_common.h"

extern struct sprd_pnp_platform_info *sprd_pnp_data;

static void all_logs_to_buf(char *buf, struct per_cpu_item *item)
{
	if (item->r_pos <= item->w_pos) {

		for ( ; item->r_pos < item->w_pos; (item->r_pos)++)
			strcat(buf, item->page[item->r_pos].event);

	} else {
		for ( ; item->r_pos < CPU_FREQ_MAX; (item->r_pos)++)
			strcat(buf, item->page[item->r_pos].event);

		item->r_pos = 0;
		for ( ; item->r_pos < item->w_pos; (item->r_pos)++)
			strcat(buf, item->page[item->r_pos].event);

	}
}

static int sprd_cpu_freq_info_read(struct seq_file *m, void *v)
{
	int i;
	char *buf = sprd_pnp_data->cpu_freq_data.cpu_freq_buf;
	struct per_cpu_item *item = sprd_pnp_data->cpu_freq_data.item;

	if (!item || !buf)
		return -ENOMEM;

	if (sprd_pnp_data->cpu_freq_data.overflow)
		goto Fill;

	if (buf)
		memset(buf, 0, EVENT_MAX * CPU_FREQ_MAX * CPU_MAX);

	for (i = 0; i < CPU_MAX; i++) {
		if (!(item[i].non_empty))
			continue;
		all_logs_to_buf(buf, &item[i]);
	}

Fill:
	seq_printf(m, "%s", buf);

	if (m->count >= m->size)
		sprd_pnp_data->cpu_freq_data.overflow = 1;
	else
		sprd_pnp_data->cpu_freq_data.overflow = 0;

	return 0;
}

int sprd_cpu_freq_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_cpu_freq_info_read, NULL);
}

static int write_data_to_buf(struct cpu_info_buffer *buffer, struct per_cpu_item *item,
				u64 time, unsigned int frequency, unsigned int cpu_id)
{
	int len;

	len = snprintf(buffer[item->w_pos].event, EVENT_MAX - 1,
				"time: %lu, cpu_freq: %lu, cpu: %u\n",
				(unsigned long)time, (unsigned long)frequency, cpu_id);

	buffer[item->w_pos].len = len;
	return len;
}

static void cpu_freq_notify_probe(struct cpufreq_freqs *freqs)
{
	unsigned int freq, cpu;
	int len;
	struct timespec64 ts;
	u64 time_us;
	struct per_cpu_item *item;
	struct cpu_info_buffer *buf;

	cpu = freqs->policy->cpu;
	freq = freqs->new;

	if (cpu > CPU_MAX - 1) {
		SPRD_PNP_ERR("%s: the num of cpu too larger!\n", __func__);
		return;
	}

	/* Time unit is us */
	ktime_get_real_ts64(&ts);
	time_us = (u64)ts.tv_sec * 1000000 + div_u64(ts.tv_nsec, 1000);

	item = &sprd_pnp_data->cpu_freq_data.item[cpu];
	buf = item->page;

	if (item->w_pos < CPU_FREQ_MAX) {
		len = write_data_to_buf(buf, item, time_us, freq, cpu);
		item->w_pos += 1;
	} else {
		item->w_pos = 0;
		len = write_data_to_buf(buf, item, time_us, freq, cpu);
		item->w_pos += 1;
	}

	item->non_empty = 1;
}

static int cpufreq_changed_event(struct notifier_block *nb, unsigned long state, void *ptr)
{
	switch (state) {
	case CPUFREQ_POSTCHANGE:
		cpu_freq_notify_probe((struct cpufreq_freqs *)ptr);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

struct notifier_block cpu_freq_notifier = {
	.notifier_call = cpufreq_changed_event,
};

static int alloc_for_per_cpu(struct sprd_pnp_platform_info *pnp_info)
{
	int i;
	struct cpu_info_buffer *buf;
	struct per_cpu_item *item;

	item =  kcalloc(CPU_MAX, sizeof(struct per_cpu_item), GFP_KERNEL);
	if (!item) {
		SPRD_PNP_ERR("%s: Failed to malloc item\n", __func__);
		return -ENOMEM;
	}

	buf = kcalloc(CPU_MAX * CPU_FREQ_MAX, sizeof(struct cpu_info_buffer), GFP_KERNEL);
	if (!buf) {
		SPRD_PNP_ERR("%s: Failed to malloc cpu_info_buffer\n", __func__);
		kfree(item);
		item = NULL;
		return -ENOMEM;
	}
	for (i = 0; i < CPU_MAX; i++)
		item[i].page = &buf[i * CPU_FREQ_MAX];

	pnp_info->cpu_freq_data.item = item;
	pnp_info->cpu_freq_data.buf = buf;

	return 0;
}

int register_cpufreq_notify_info(struct sprd_pnp_platform_info *pnp_data)
{
	int ret = 0;

	if (!pnp_data->cpu_freq_data.regis_cnt) {

		if (!pnp_data->cpu_freq_data.cpu_freq_buf) {
			pnp_data->cpu_freq_data.cpu_freq_buf =
					kmalloc(EVENT_MAX * CPU_FREQ_MAX * CPU_MAX, GFP_KERNEL);
			if (!pnp_data->cpu_freq_data.cpu_freq_buf) {
				SPRD_PNP_ERR("%s: Failed to malloc cpu_freq_buf mem\n", __func__);
				return -ENOMEM;
			}

			ret = alloc_for_per_cpu(pnp_data);
			if (ret) {
				SPRD_PNP_ERR("%s: Failed to malloc mem\n", __func__);
				kfree(pnp_data->cpu_freq_data.cpu_freq_buf);
				pnp_data->cpu_freq_data.cpu_freq_buf = NULL;
				return -ENOMEM;
			}
		} else {
			SPRD_PNP_WARN("%s: the mem is already allocated\n", __func__);
		}

		ret = cpufreq_register_notifier(&cpu_freq_notifier,
					CPUFREQ_TRANSITION_NOTIFIER);
		if (ret) {
			SPRD_PNP_ERR("%s: Failed to register callback, ret=%d\n",
						__func__,  ret);
			kfree(pnp_data->cpu_freq_data.cpu_freq_buf);
			pnp_data->cpu_freq_data.cpu_freq_buf = NULL;

			kfree(pnp_data->cpu_freq_data.item);
			pnp_data->cpu_freq_data.item = NULL;

			kfree(pnp_data->cpu_freq_data.buf);
			pnp_data->cpu_freq_data.buf = NULL;
		} else {
			pnp_data->cpu_freq_data.regis_cnt += 1;
		}
	} else {
		SPRD_PNP_WARN("%s: cpu freq hook has already registered!\n", __func__);
	}
	return ret;
}

int unregister_cpufreq_notify_info(struct sprd_pnp_platform_info *pnp_data)
{
	int ret = 0;

	if (pnp_data->cpu_freq_data.regis_cnt > 0) {

		ret = cpufreq_unregister_notifier(&cpu_freq_notifier,
					CPUFREQ_TRANSITION_NOTIFIER);
		if (ret) {
			SPRD_PNP_ERR("%s: Failed to unregister callback, ret=%d\n",
						__func__,  ret);
			return ret;
		}
		pnp_data->cpu_freq_data.regis_cnt -= 1;

		kfree(pnp_data->cpu_freq_data.cpu_freq_buf);
		pnp_data->cpu_freq_data.cpu_freq_buf = NULL;

		kfree(pnp_data->cpu_freq_data.item);
		pnp_data->cpu_freq_data.item = NULL;

		kfree(pnp_data->cpu_freq_data.buf);
		pnp_data->cpu_freq_data.buf = NULL;

	} else {
		SPRD_PNP_WARN("%s: cpu freq hook has already unregisted!\n", __func__);
	}
	return ret;
}

ssize_t cpu_en_read(struct file *file, char __user *in_buf, size_t count, loff_t *ppos)
{
	ssize_t ret;

	ret = common_en_read(&sprd_pnp_data->cpu_freq_data.debug_en, file, in_buf, count, ppos);
	return ret;
}

ssize_t cpu_en_write(struct file *file, const char __user *user_buf, size_t count,
			     loff_t *ppos)
{
	ssize_t ret;
	int regis_ret, unregis_ret;
	bool debug_en;

	ret = common_en_write(&debug_en, file, user_buf, count, ppos);
	sprd_pnp_data->cpu_idle_data.debug_en = sprd_pnp_data->cpu_freq_data.debug_en = debug_en;
	SPRD_PNP_WARN("%s: debug_en = %d\n", __func__, debug_en);

	if (debug_en) {
		regis_ret = register_cpufreq_notify_info(sprd_pnp_data);
		if (regis_ret)
			SPRD_PNP_ERR("%s: Failed to register cpu freq trace info\n", __func__);

		regis_ret = register_cpuidle_trace_info(sprd_pnp_data);
		if (regis_ret)
			SPRD_PNP_ERR("%s: Failed to register cpu idle trace info\n", __func__);

		wake_up_interruptible(&sprd_pnp_data->poll_wq);
	} else {
		unregis_ret = unregister_cpufreq_notify_info(sprd_pnp_data);
		if (unregis_ret)
			SPRD_PNP_ERR("%s: Failed to unregister cpu freq trace info\n", __func__);

		unregis_ret = unregister_cpuidle_trace_info(sprd_pnp_data);
		if (unregis_ret)
			SPRD_PNP_ERR("%s: Failed to unregister cpu idle trace info\n", __func__);
	}

	return ret;
}

__poll_t cpu_en_poll(struct file *file, poll_table *wait)
{
	__poll_t mask = 0;

	poll_wait(file, &sprd_pnp_data->poll_wq, wait);
	if (sprd_pnp_data->cpu_freq_data.debug_en)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}
