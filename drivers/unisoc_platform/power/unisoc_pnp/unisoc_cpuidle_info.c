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
#include "unisoc_pnp_common.h"

extern struct sprd_pnp_platform_info *sprd_pnp_data;

DEFINE_SPINLOCK(pnp_cpuidle_lock);

static void all_logs_to_buf(char *buf, struct cpu_idle_page *idle_page)
{
	if (idle_page->r_pos <= idle_page->w_pos) {

		for ( ; idle_page->r_pos < idle_page->w_pos; (idle_page->r_pos)++)
			strcat(buf, idle_page->page[idle_page->r_pos].event);

	} else {
		for ( ; idle_page->r_pos < CPU_IDLE_MAX; (idle_page->r_pos)++)
			strcat(buf, idle_page->page[idle_page->r_pos].event);

		idle_page->r_pos = 0;

		for ( ; idle_page->r_pos < idle_page->w_pos; (idle_page->r_pos)++)
			strcat(buf, idle_page->page[idle_page->r_pos].event);

	}
}

static int sprd_cpu_idle_info_read(struct seq_file *m, void *v)
{
	char *buf = sprd_pnp_data->cpu_idle_data.cpu_idle_buf;
	struct cpu_idle_page *page = sprd_pnp_data->cpu_idle_data.ctrl;

	if (!page || !buf)
		return -ENOMEM;

	if (sprd_pnp_data->cpu_idle_data.overflow)
		goto Fill;

	if (buf)
		memset(buf, 0, EVENT_MAX * CPU_IDLE_MAX);

	all_logs_to_buf(buf, page);

Fill:
	seq_printf(m, "%s", buf);

	if (m->count >= m->size)
		sprd_pnp_data->cpu_idle_data.overflow = 1;
	else
		sprd_pnp_data->cpu_idle_data.overflow = 0;

	return 0;
}

int sprd_cpu_idle_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_cpu_idle_info_read, NULL);
}


static int write_data_to_buf(struct cpu_info_buffer *buffer, struct cpu_idle_page *idle_page,
				u64 time, unsigned int state, unsigned int cpu_id)
{
	int len;

	len = snprintf(buffer[idle_page->w_pos].event, EVENT_MAX - 1,
				"time: %lu, state: %lu, cpu: %u\n",
				(unsigned long)time, (unsigned long)state, cpu_id);

	buffer[idle_page->w_pos].len = len;
	return len;
}

static void cpu_idle_trace_probe(void *unuse,
				unsigned int state, unsigned int cpu_id)
{
	unsigned int stat, cpu;
	int len;
	struct timespec64 ts;
	u64 time_us;
	struct cpu_idle_page *page;
	struct cpu_info_buffer *buf;

	stat = state;
	cpu = cpu_id;

	page = sprd_pnp_data->cpu_idle_data.ctrl;
	buf = page->page;

	/* Time unit is us */
	ktime_get_real_ts64(&ts);
	time_us = (u64)ts.tv_sec * 1000000 + div_u64(ts.tv_nsec, 1000);

	spin_lock(&pnp_cpuidle_lock);

	if (page->w_pos < CPU_IDLE_MAX) {
		len = write_data_to_buf(buf, page, time_us, stat, cpu);
		page->w_pos += 1;
	} else {
		page->w_pos = 0;
		len = write_data_to_buf(buf, page, time_us, stat, cpu);
		page->w_pos += 1;
	}

	spin_unlock(&pnp_cpuidle_lock);
}

static int alloc_for_idle_ctrl(struct sprd_pnp_platform_info *pnp_info)
{
	struct cpu_info_buffer *buf;
	struct cpu_idle_page *page;

	page =  kzalloc(sizeof(struct cpu_idle_page), GFP_KERNEL);
	if (!page) {
		SPRD_PNP_ERR("%s: Failed to malloc page\n", __func__);
		return -ENOMEM;
	}

	buf = kzalloc(CPU_IDLE_MAX * sizeof(struct cpu_info_buffer), GFP_KERNEL);
	if (!buf) {
		SPRD_PNP_ERR("%s: Failed to malloc cpu_info_buffer\n", __func__);
		kfree(page);
		page = NULL;
		return -ENOMEM;
	}

	page->page = buf;
	pnp_info->cpu_idle_data.ctrl = page;

	return 0;
}

int register_cpuidle_trace_info(struct sprd_pnp_platform_info *pnp_data)
{
	int ret = 0;
	struct cpu_idle_page *page;

	if (!pnp_data->cpu_idle_data.regis_cnt) {

		if (!pnp_data->cpu_idle_data.cpu_idle_buf) {
			pnp_data->cpu_idle_data.cpu_idle_buf =
					kmalloc(EVENT_MAX * CPU_IDLE_MAX, GFP_KERNEL);
			if (!pnp_data->cpu_idle_data.cpu_idle_buf) {
				SPRD_PNP_ERR("%s: Failed to malloc cpu_idle_buf mem\n", __func__);
				return -ENOMEM;
			}

			ret = alloc_for_idle_ctrl(pnp_data);
			if (ret) {
				SPRD_PNP_ERR("%s: Failed to malloc mem\n", __func__);
				kfree(pnp_data->cpu_idle_data.cpu_idle_buf);
				pnp_data->cpu_idle_data.cpu_idle_buf = NULL;
				return -ENOMEM;
			}
		} else {
			SPRD_PNP_WARN("%s: the mem is already allocated\n", __func__);
		}

		ret = register_trace_cpu_idle(cpu_idle_trace_probe, NULL);
		if (ret) {
			SPRD_PNP_ERR("%s: Failed to register callback, ret=%d\n",
						__func__,  ret);
			kfree(pnp_data->cpu_idle_data.cpu_idle_buf);
			pnp_data->cpu_idle_data.cpu_idle_buf = NULL;

			page = pnp_data->cpu_idle_data.ctrl;
			kfree(page->page);
			kfree(page);
			pnp_data->cpu_idle_data.ctrl = NULL;
		} else {
			pnp_data->cpu_idle_data.regis_cnt += 1;
		}
	} else {
		SPRD_PNP_WARN("%s: cpu idle hook has already registered!\n", __func__);
	}

	return ret;
}

int unregister_cpuidle_trace_info(struct sprd_pnp_platform_info *pnp_data)
{
	struct cpu_idle_page *page;
	int ret = 0;

	if (pnp_data->cpu_idle_data.regis_cnt > 0) {

		ret = unregister_trace_cpu_idle(cpu_idle_trace_probe, NULL);
		if (ret) {
			SPRD_PNP_ERR("%s: Failed to unregister callback, ret=%d\n",
						__func__,  ret);
			return ret;
		}
		pnp_data->cpu_idle_data.regis_cnt -= 1;

		kfree(pnp_data->cpu_idle_data.cpu_idle_buf);
		pnp_data->cpu_idle_data.cpu_idle_buf = NULL;

		page = pnp_data->cpu_idle_data.ctrl;
		kfree(page->page);
		kfree(page);
		pnp_data->cpu_idle_data.ctrl = NULL;

	} else {
		SPRD_PNP_WARN("%s: cpu idle hook has already unregisted!\n", __func__);
	}
	return ret;
}
