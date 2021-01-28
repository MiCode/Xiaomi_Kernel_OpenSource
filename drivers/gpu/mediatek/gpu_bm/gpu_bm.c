// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include <mt-plat/mtk_gpu_utility.h>

#define	MTKGPUQoS_UNREFERENCED(param) ((void)(param))

uint32_t __iomem *gpu_info_buf;

static void _mgq_proc_show_v1(struct seq_file *m)
{
	seq_printf(m, "ctx: \t0x%x\n", readl(gpu_info_buf + 1));
	seq_printf(m, "frame: \t0x%x\n", readl(gpu_info_buf + 2));
	seq_printf(m, "job: \t0x%x\n", readl(gpu_info_buf + 3));
	seq_printf(m, "freq: \t0x%x\n", readl(gpu_info_buf + 4));
	seq_printf(m, "bw: \t0x%x\n", readl(gpu_info_buf + 5));
	seq_printf(m, "pbw: \t0x%x\n", readl(gpu_info_buf + 6));
}

static int _mgq_proc_show(struct seq_file *m, void *v)
{
	if (gpu_info_buf) {
		unsigned int version = readl(gpu_info_buf + 0);

		seq_printf(m, "version: 0x%x\n", version);
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

	if (!proc_create("job_status", 0664, dir, &_mgq_proc_fops))
		pr_debug("@%s: create /proc/mgq/job_status failed\n", __func__);

	return 0;
}

struct timer_list timer_setupFW;

struct setupfw_t {
	phys_addr_t phyaddr;
	size_t size;
};

static void _MTKGPUQoS_setupFW(phys_addr_t phyaddr, size_t size)
{
#ifdef MTK_QOS_FRAMEWORK
	struct qos_ipi_data qos_d;
	int ret;

	qos_d.cmd = QOS_IPI_SETUP_GPU_INFO;
	qos_d.u.gpu_info.addr = (unsigned int)phyaddr;
	qos_d.u.gpu_info.addr_hi = (unsigned int)(phyaddr >> 32);
	qos_d.u.gpu_info.size = (unsigned int)size;
	ret = qos_ipi_to_sspm_command(&qos_d, 4);

	pr_debug("%s: addr:0x%x, addr_hi:0x%x, ret:%d\n",
		__func__,
		qos_d.u.gpu_info.addr,
		qos_d.u.gpu_info.addr_hi,
		ret);
#endif
}

static void bw_v1_gpu_power_change_notify(int power_on)
{
	static int ctx;

	if (!power_on) {
		ctx = readl(gpu_info_buf + 1);
		writel(0, gpu_info_buf + 1); // ctx
	} else {
		writel(ctx, gpu_info_buf + 1);
	}
}

void MTKGPUQoS_setup(uint32_t *cpuaddr, phys_addr_t phyaddr, size_t size)
{
	gpu_info_buf = cpuaddr;

	_MTKGPUQoS_initDebugFS();
	_MTKGPUQoS_setupFW(phyaddr, size);
#if defined(CONFIG_MTK_GPU_SUPPORT)
	mtk_register_gpu_power_change("qpu_qos", bw_v1_gpu_power_change_notify);
#else
	MTKGPUQoS_UNREFERENCED(bw_v1_gpu_power_change_notify);
#endif
}
EXPORT_SYMBOL(MTKGPUQoS_setup);

uint32_t MTKGPUQoS_getBW(uint32_t offset)
{
	unsigned int version;

	if (!gpu_info_buf)
		return 0;

	version = readl(gpu_info_buf + 0);

	if (version == 1) {
		if (offset == 0)
			return readl(gpu_info_buf + 5);
		if (offset == 1)
			return readl(gpu_info_buf + 6);
	}

	return 0;
}
EXPORT_SYMBOL(MTKGPUQoS_getBW);

