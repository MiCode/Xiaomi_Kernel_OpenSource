// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2016-2024 Unisoc (Shanghai) Technologies Co., Ltd
 */

#include <linux/cpu_pm.h>
#include <linux/cpufreq.h>
#include <linux/printk.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/time.h>
#include <trace/events/devfreq.h>
#include "unisoc_pnp_common.h"

extern struct sprd_pnp_platform_info *sprd_pnp_data;

static void all_logs_to_buf(char *buf, struct gpu_freq_page *gpu_page)
{
	if (gpu_page->r_pos <= gpu_page->w_pos) {

		for ( ; gpu_page->r_pos < gpu_page->w_pos; (gpu_page->r_pos)++)
			strcat(buf, gpu_page->page[gpu_page->r_pos].event);

	} else {
		for ( ; gpu_page->r_pos < GPU_FREQ_MAX; (gpu_page->r_pos)++)
			strcat(buf, gpu_page->page[gpu_page->r_pos].event);

		gpu_page->r_pos = 0;
		for ( ; gpu_page->r_pos < gpu_page->w_pos; (gpu_page->r_pos)++)
			strcat(buf, gpu_page->page[gpu_page->r_pos].event);

	}
}

static int sprd_gpu_freq_info_read(struct seq_file *m, void *v)
{
	char *buf = sprd_pnp_data->gpu_freq_data.gpu_freq_buf;
	struct gpu_freq_page *page = sprd_pnp_data->gpu_freq_data.ctrl;

	if (!page || !buf)
		return -ENOMEM;

	if (sprd_pnp_data->gpu_freq_data.overflow)
		goto Fill;

	if (buf)
		memset(buf, 0, EVENT_MAX * GPU_FREQ_MAX);

	all_logs_to_buf(buf, page);

Fill:
	seq_printf(m, "%s", buf);

	if (m->count >= m->size)
		sprd_pnp_data->gpu_freq_data.overflow = 1;
	else
		sprd_pnp_data->gpu_freq_data.overflow = 0;


	return 0;
}

int sprd_gpu_freq_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_gpu_freq_info_read, NULL);
}

static int write_data_to_buf(struct gpu_info_buffer *buffer,
		struct gpu_freq_page *gpu_page, u64 time, struct devfreq *devfreq,
		unsigned long frequency, unsigned long pre_frequency)
{
	int len;
	unsigned long busy_time, total_time;

	busy_time = devfreq->last_status.busy_time;
	total_time = devfreq->last_status.total_time;

	len = snprintf(buffer[gpu_page->w_pos].event, EVENT_MAX - 1,
			"time: %lu, freq: %lu, pre_freq: %lu,busy_time: %lu, total_time: %lu\n",
			(unsigned long)time, frequency, pre_frequency, busy_time, total_time);

	buffer[gpu_page->w_pos].len = len;
	return len;
}

static void gpu_freq_trace_probe(void *unuse,
	struct devfreq *devfreq, unsigned long frequency, unsigned long pre_frequency)
{
	unsigned long freq, pre_freq;
	int len;
	struct timespec64 ts;
	u64 time_us;
	struct devfreq *devf;
	struct gpu_freq_page *page;
	struct gpu_info_buffer *buf;

	devf = devfreq;
	freq = frequency;
	pre_freq = pre_frequency;

	page = sprd_pnp_data->gpu_freq_data.ctrl;
	buf = page->page;

	/* Time unit is us */
	ktime_get_real_ts64(&ts);
	time_us = (u64)ts.tv_sec * 1000000 + div_u64(ts.tv_nsec, 1000);

	if (page->w_pos < GPU_FREQ_MAX) {
		len = write_data_to_buf(buf, page, time_us, devf, freq, pre_freq);
		page->w_pos += 1;
	} else {
		page->w_pos = 0;
		len = write_data_to_buf(buf, page, time_us, devf, freq, pre_freq);
		page->w_pos += 1;
	}
}

static int alloc_for_gpu_ctrl(struct sprd_pnp_platform_info *pnp_info)
{

	struct gpu_info_buffer *buf;
	struct gpu_freq_page *page =  kzalloc(sizeof(struct gpu_freq_page), GFP_KERNEL);

	if (!page) {
		SPRD_PNP_ERR("%s: Failed to malloc page\n", __func__);
		return -ENOMEM;
	}

	buf = kzalloc(GPU_FREQ_MAX * sizeof(struct gpu_info_buffer), GFP_KERNEL);
	if (!buf) {
		SPRD_PNP_ERR("%s: Failed to malloc gpu_info_buffer\n", __func__);
		kfree(page);
		page = NULL;
		return -ENOMEM;
	}

	page->page = buf;
	pnp_info->gpu_freq_data.ctrl = page;

	return 0;
}

static int register_gpufreq_trace_info(struct sprd_pnp_platform_info *pnp_data)
{
	int ret = 0;
	struct gpu_freq_page *page;

	if (!pnp_data->gpu_freq_data.regis_cnt) {

		if (!pnp_data->gpu_freq_data.gpu_freq_buf) {
			pnp_data->gpu_freq_data.gpu_freq_buf =
					kmalloc(EVENT_MAX * GPU_FREQ_MAX, GFP_KERNEL);
			if (!pnp_data->gpu_freq_data.gpu_freq_buf) {
				SPRD_PNP_ERR("%s: Failed to malloc gpu_freq_buf mem\n", __func__);
				return -ENOMEM;
			}

			ret = alloc_for_gpu_ctrl(pnp_data);
			if (ret) {
				SPRD_PNP_ERR("%s: Failed to malloc mem\n", __func__);
				kfree(pnp_data->gpu_freq_data.gpu_freq_buf);
				pnp_data->gpu_freq_data.gpu_freq_buf = NULL;
				return -ENOMEM;
			}
		} else {
			SPRD_PNP_WARN("%s: the mem is already allocated\n", __func__);
		}

		ret = register_trace_devfreq_frequency(gpu_freq_trace_probe, NULL);
		if (ret) {
			SPRD_PNP_ERR("%s: Failed to register callback, ret=%d\n",
						__func__,  ret);
			kfree(pnp_data->gpu_freq_data.gpu_freq_buf);
			pnp_data->gpu_freq_data.gpu_freq_buf = NULL;

			page = pnp_data->gpu_freq_data.ctrl;
			kfree(page->page);
			kfree(page);
			pnp_data->gpu_freq_data.ctrl = NULL;
		} else {
			pnp_data->gpu_freq_data.regis_cnt += 1;
		}
	} else {
		SPRD_PNP_WARN("%s: gpu freq hook has already registered!\n", __func__);
	}

	return ret;
}

int unregister_gpufreq_trace_info(struct sprd_pnp_platform_info *pnp_data)
{
	struct gpu_freq_page *page;
	int ret = 0;

	if (pnp_data->gpu_freq_data.regis_cnt > 0) {

		ret = unregister_trace_devfreq_frequency(gpu_freq_trace_probe, NULL);
		if (ret) {
			SPRD_PNP_ERR("%s: Failed to unregister callback, ret=%d\n",
						__func__,  ret);
			return ret;
		}
		pnp_data->gpu_freq_data.regis_cnt -= 1;

		kfree(pnp_data->gpu_freq_data.gpu_freq_buf);
		pnp_data->gpu_freq_data.gpu_freq_buf = NULL;

		page = pnp_data->gpu_freq_data.ctrl;
		kfree(page->page);
		kfree(page);
		pnp_data->gpu_freq_data.ctrl = NULL;

	} else {
		SPRD_PNP_WARN("%s: cpu idle hook has already unregisted!\n", __func__);
	}
	return ret;
}

ssize_t gpu_en_read(struct file *file, char __user *in_buf, size_t count, loff_t *ppos)
{
	ssize_t ret;

	ret = common_en_read(&sprd_pnp_data->gpu_freq_data.debug_en, file, in_buf, count, ppos);
	return ret;
}

ssize_t gpu_en_write(struct file *file, const char __user *user_buf, size_t count,
			     loff_t *ppos)
{
	ssize_t ret;
	int regis_ret, unregis_ret;
	bool debug_en;

	ret = common_en_write(&debug_en, file, user_buf, count, ppos);
	sprd_pnp_data->gpu_freq_data.debug_en = debug_en;
	SPRD_PNP_WARN("%s: debug_en = %d\n", __func__, debug_en);

	if (sprd_pnp_data->gpu_freq_data.debug_en) {
		regis_ret = register_gpufreq_trace_info(sprd_pnp_data);
		if (regis_ret)
			SPRD_PNP_ERR("%s: Failed to register gpu freq trace info\n", __func__);

	} else {
		unregis_ret = unregister_gpufreq_trace_info(sprd_pnp_data);
		if (unregis_ret)
			SPRD_PNP_ERR("%s: Failed to unregister gpu freq trace info\n", __func__);
	}

	return ret;
}
