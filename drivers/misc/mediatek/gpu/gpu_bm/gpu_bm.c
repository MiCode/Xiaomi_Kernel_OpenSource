/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/timer.h>

#ifdef MTK_QOS_FRAMEWORK
#include <mtk_qos_ipi.h>
#endif
#include <mtk_gpu_utility.h>
#include <mtk_gpufreq.h>

struct v1_data {
	unsigned int version;
	unsigned int ctx;
	unsigned int frame;
	unsigned int job;
	unsigned int freq;
};
struct v1_data *gpu_info_buf;
//uint32_t __iomem *gpu_info_buf;

static void _mgq_proc_show_v1(struct seq_file *m)
{
	seq_printf(m, "ctx: \t%d\n", gpu_info_buf->ctx);
	seq_printf(m, "frame: \t%d\n", gpu_info_buf->frame);
	seq_printf(m, "job: \t%d\n", gpu_info_buf->job);
	seq_printf(m, "freq: \t%d\n", gpu_info_buf->freq);
	//seq_printf(m, "bw: \t0x%x\n", readl(gpu_info_buf + 5));
	//seq_printf(m, "pbw: \t0x%x\n", readl(gpu_info_buf + 6));
}

static int _mgq_proc_show(struct seq_file *m, void *v)
{
	if (gpu_info_buf) {
		unsigned int version = readl(gpu_info_buf + 0);

		seq_printf(m, "version: %d\n", version);
		if (version == 1)
			_mgq_proc_show_v1(m);
		else
			seq_printf(m, "unknown version: 0x%x\n", version);
	} else {
		seq_puts(m, "gpu_info_buf == null\n");
	}
	return 0;
}

static int _mgq_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, _mgq_proc_show, NULL);
}
static const struct file_operations _mgq_proc_fops = {
	.owner = THIS_MODULE,
	.open = _mgq_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int _MTKGPUQoS_initDebugFS(void)
{
	struct proc_dir_entry *dir = NULL;

	dir = proc_mkdir("mgq", NULL);
	if (!dir) {
		pr_debug("@%s: create /proc/mgq failed\n", __func__);
		return -ENOMEM;
	}

	if (!proc_create("job_status", 0644, dir, &_mgq_proc_fops))
		pr_debug("@%s: create /proc/mgq/job_status failed\n", __func__);

	return 0;
}

struct timer_list timer_setupFW;

struct setupfw_t {
	phys_addr_t phyaddr;
	size_t size;
};

static void setupfw_timer_callback(unsigned long _data)
{
#ifdef MTK_QOS_FRAMEWORK
	struct qos_ipi_data qos_d;
	int ret;
	struct setupfw_t data = *(struct setupfw_t *)_data;

	qos_d.cmd = QOS_IPI_SETUP_GPU_INFO;
	qos_d.u.gpu_info.addr = (unsigned int)data.phyaddr;
	qos_d.u.gpu_info.addr_hi = (unsigned int)(data.phyaddr >> 32);
	qos_d.u.gpu_info.size = (unsigned int)data.size;
	ret = qos_ipi_to_sspm_command(&qos_d, 4);

	pr_debug("%s: addr:0x%x, addr_hi:0x%x, ret:%d\n",
		__func__,
		qos_d.u.gpu_info.addr,
		qos_d.u.gpu_info.addr_hi,
		ret);

	if (ret == 0) {
		kfree((void *)_data);
	} else {
		pr_debug("%s: sspm_ipi fail (%d)\n", __func__, ret);

		timer_setupFW.expires = jiffies + HZ * 5;
		add_timer(&timer_setupFW);
	}
#endif
}

static void _MTKGPUQoS_setupFW(phys_addr_t phyaddr, size_t size)
{
	struct setupfw_t *_data = (struct setupfw_t *)
			kmalloc(sizeof(struct setupfw_t), GFP_KERNEL);

	if (_data == NULL) {
		pr_debug("%s: kmalloc fail\n", __func__);
		return;
	}
	_data->phyaddr = phyaddr;
	_data->size = size;

	init_timer(&timer_setupFW);
	timer_setupFW.function = setupfw_timer_callback;
	timer_setupFW.data = (unsigned long)_data;
	timer_setupFW.expires = jiffies;

	setupfw_timer_callback(timer_setupFW.data);
}

static void bw_v1_gpu_power_change_notify(int power_on)
{
	static int ctx;
	unsigned int loading, idx, min_idx;

	mtk_get_gpu_loading(&loading);
	idx = mt_gpufreq_get_cur_freq_index();
	min_idx = mt_gpufreq_get_dvfs_table_num()-1;

	if (!power_on) {
		ctx = gpu_info_buf->ctx;
		gpu_info_buf->ctx = 0; // ctx
	} else {
		gpu_info_buf->ctx = ctx;

		/*
		 * if gpu loading < 40% and gpu freq is lowest,
		 * don't do GPU QoS prediction.
		 */
		if ((idx == min_idx) && (loading < 40))
			gpu_info_buf->freq = 5566;
		else
			gpu_info_buf->freq = 0;
	}

}

void MTKGPUQoS_setup(struct v1_data *v1, phys_addr_t phyaddr, size_t size)
{
	gpu_info_buf = v1;

	_MTKGPUQoS_initDebugFS();
	_MTKGPUQoS_setupFW(phyaddr, size);

	mtk_register_gpu_power_change("qpu_qos", bw_v1_gpu_power_change_notify);
}
EXPORT_SYMBOL(MTKGPUQoS_setup);

uint32_t MTKGPUQoS_getBW(uint32_t offset)
{
	return 0;
}
EXPORT_SYMBOL(MTKGPUQoS_getBW);

