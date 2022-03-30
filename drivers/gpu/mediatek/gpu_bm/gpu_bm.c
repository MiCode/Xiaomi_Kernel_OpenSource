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
#include <linux/kernel.h>
#include <linux/module.h>

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
#include <mtk_qos_ipi.h>
#endif /* CONFIG_MTK_QOS_FRAMEWORK */
#include <mt-plat/mtk_gpu_utility.h>
#include <mtk_gpufreq.h>

#define GPU_BW_RATIO_CEIL             300
#define GPU_BW_RATIO_FLOOR             10

#define GPU_BW_DEFAULT_MODE             0
#define GPU_BW_SPORT_MODE               1
#define GPU_BW_NO_PRED_MODE            -1

struct v1_data {
	unsigned int version;
	unsigned int ctx;
	unsigned int frame;
	unsigned int job;
	unsigned int freq;
};
struct v1_data *gpu_info_buf;
static int gpu_bm_inited;
static int g_mode;

static void _mgq_proc_show_v1(struct seq_file *m)
{
	seq_printf(m, "ctx: \t%d\n", gpu_info_buf->ctx);
	seq_printf(m, "frame: \t%d\n", gpu_info_buf->frame);
	seq_printf(m, "job: \t%d\n", gpu_info_buf->job);
	seq_printf(m, "freq: \t%d\n", gpu_info_buf->freq);
	seq_printf(m, "mode: \t%d\n", g_mode);
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

static ssize_t
_mgq_proc_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *f_pos)
{
	int value = 0;
	char buf[32];
	unsigned int len = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len))
		return 0;
	buf[len] = '\0';

	/* 0      : default bw prediction.
	 * 1      : sport mode specialized
	 * 10-300 : apply a ratio for bw prediction
	 * -1     : no bw prediction
	 */
	if (!kstrtoint(buf, 10, &value)) {
		if (value == GPU_BW_SPORT_MODE) {
			g_mode = GPU_BW_SPORT_MODE;
		} else {
			if (value == GPU_BW_DEFAULT_MODE)
				g_mode = GPU_BW_DEFAULT_MODE;
			else if (value == GPU_BW_NO_PRED_MODE)
				g_mode = GPU_BW_NO_PRED_MODE;
			else if (value >= GPU_BW_RATIO_FLOOR && value <= GPU_BW_RATIO_CEIL)
				g_mode = value;
		}
		gpu_info_buf->freq = g_mode;
	}
	return count;
}

static const struct proc_ops _mgq_proc_fops = {
	.proc_open = _mgq_proc_open,
	.proc_read = seq_read,
	.proc_write = _mgq_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
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

struct setupfw_t {
	phys_addr_t phyaddr;
	size_t size;
};

static struct setupfw_t setupfw_data;
static void setupfw_work_handler(struct work_struct *work);
static DECLARE_DELAYED_WORK(g_setupfw_work, setupfw_work_handler);

static void setupfw_work_handler(struct work_struct *work)
{
#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
	struct qos_ipi_data qos_d;
	int ret;

	qos_d.cmd = QOS_IPI_SETUP_GPU_INFO;
	qos_d.u.gpu_info.addr = (unsigned int)setupfw_data.phyaddr;
	qos_d.u.gpu_info.addr_hi = (unsigned int)(setupfw_data.phyaddr >> 32);
	qos_d.u.gpu_info.size = (unsigned int)setupfw_data.size;

#ifdef MTK_SCMI
	ret = qos_ipi_to_sspm_scmi_command(qos_d.cmd,
		qos_d.u.gpu_info.addr,
		qos_d.u.gpu_info.addr_hi,
		qos_d.u.gpu_info.size, QOS_IPI_SCMI_SET);

	if (ret)
		pr_info("%s: sspm_ipi_to_scmi fail (%d)\n", __func__, ret);
	else
		pr_info("%s: sspm_ipi_to_scmi success! (%d)\n", __func__, ret);

#else
	ret = qos_ipi_to_sspm_command(&qos_d, 4);

	if (ret == 1)
		pr_info("%s: sspm_ipi success! (%d)\n", __func__, ret);
	else
		pr_info("%s: sspm_ipi fail (%d)\n", __func__, ret);

#endif /* MTK_SCMI */
	gpu_bm_inited = 1;
	pr_debug("%s: addr:0x%x, addr_hi:0x%x, ret:%d\n",
		__func__,
		qos_d.u.gpu_info.addr,
		qos_d.u.gpu_info.addr_hi,
		ret);

#else
	pr_debug("%s: sspm_ipi is not support!\n", __func__);
#endif /* MTK_QOS_FRAMEWORK */
}

static void _MTKGPUQoS_setupFW(phys_addr_t phyaddr, size_t size)
{

	setupfw_data.phyaddr = phyaddr;
	setupfw_data.size = size;

	schedule_delayed_work(&g_setupfw_work, 1);
}

void MTKGPUQoS_mode(void)
{
	unsigned int loading, idx, min_idx, high_idx;

	mtk_get_gpu_loading(&loading);
#if defined(CONFIG_MTK_GPUFREQ_V2)
	idx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);
	min_idx = gpufreq_get_opp_num(TARGET_DEFAULT) - 1;
	high_idx = (gpufreq_get_opp_num(TARGET_DEFAULT) - 1) / 4 + 1;
#else
	idx = mt_gpufreq_get_cur_freq_index();
	min_idx = mt_gpufreq_get_dvfs_table_num() - 1;
	high_idx = (mt_gpufreq_get_dvfs_table_num()-1) / 4 + 1;
#endif
	/* sport mode */
	if (g_mode == GPU_BW_SPORT_MODE) {
		/*
		 * if gpu freq at top quartile, boost dram freq.
		 */
		if (idx <= high_idx)
			gpu_info_buf->freq = GPU_BW_RATIO_CEIL;
		else
			gpu_info_buf->freq = 0;
	/* default prediction  */
	} else if (g_mode == GPU_BW_DEFAULT_MODE) {
		/*
		 * if gpu loading < 40% and gpu freq is lowest,
		 * don't do GPU QoS prediction.
		 */
		if ((idx == min_idx) && (loading < 40))
			gpu_info_buf->freq = 5566;
		else
			gpu_info_buf->freq = 0;
	/* apply a ratio for bw prediction */
	} else if (g_mode >= GPU_BW_RATIO_FLOOR && g_mode <= GPU_BW_RATIO_CEIL) {
		gpu_info_buf->freq = g_mode;
	/* no bw prediction */
	} else if (g_mode == GPU_BW_NO_PRED_MODE) {
		gpu_info_buf->freq = 5566;
	}
}
EXPORT_SYMBOL(MTKGPUQoS_mode);

static void bw_v1_gpu_power_change_notify(int power_on)
{
	static int ctx;
	unsigned int loading, idx, min_idx, high_idx;

	mtk_get_gpu_loading(&loading);
#if defined(CONFIG_MTK_GPUFREQ_V2)
	idx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);
	min_idx = gpufreq_get_opp_num(TARGET_DEFAULT) - 1;
	high_idx = (gpufreq_get_opp_num(TARGET_DEFAULT) - 1) / 4 + 1;
#else
	idx = mt_gpufreq_get_cur_freq_index();
	min_idx = mt_gpufreq_get_dvfs_table_num()-1;
	high_idx = (mt_gpufreq_get_dvfs_table_num()-1) / 4 + 1;
#endif

	if (!power_on) {
		ctx = gpu_info_buf->ctx;
		gpu_info_buf->ctx = 0; // ctx
		return;
	}

	gpu_info_buf->ctx = ctx;

	/* sport mode */
	if (g_mode == GPU_BW_SPORT_MODE) {
		/*
		 * if gpu freq at top quartile, boost dram freq.
		 */
		if (idx <= high_idx)
			gpu_info_buf->freq = GPU_BW_RATIO_CEIL;
		else
			gpu_info_buf->freq = 0;
	/* default prediction  */
	} else if (g_mode == GPU_BW_DEFAULT_MODE) {
		/*
		 * if gpu loading < 40% and gpu freq is lowest,
		 * don't do GPU QoS prediction.
		 */
		if ((idx == min_idx) && (loading < 40))
			gpu_info_buf->freq = 5566;
		else
			gpu_info_buf->freq = 0;
	/* apply a ratio for bw prediction */
	} else if (g_mode >= GPU_BW_RATIO_FLOOR && g_mode <= GPU_BW_RATIO_CEIL) {
		gpu_info_buf->freq = g_mode;
	/* no bw prediction */
	} else if (g_mode == GPU_BW_NO_PRED_MODE) {
		gpu_info_buf->freq = 5566;
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

int MTKGPUQoS_is_inited(void)
{
	return gpu_bm_inited;
}
EXPORT_SYMBOL(MTKGPUQoS_is_inited);

uint32_t MTKGPUQoS_getBW(uint32_t offset)
{
	return 0;
}
EXPORT_SYMBOL(MTKGPUQoS_getBW);

static int __init mtk_gpu_qos_init(void)
{
	/*Do Nothing*/
	return 0;
}

static void __exit mtk_gpu_qos_exit(void)
{
	/*Do Nothing*/
	;
}

arch_initcall(mtk_gpu_qos_init);
module_exit(mtk_gpu_qos_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GPU QOS");
MODULE_AUTHOR("MediaTek Inc.");
