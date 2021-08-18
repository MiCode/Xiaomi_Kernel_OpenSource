// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/semaphore.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>


#include <gpueb_ipi.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>

#include "ged_eb.h"
#include "ged_base.h"
#include "ged_dvfs.h"

#include "ged_log.h"

#include <mt-plat/mtk_gpu_utility.h>

#if defined(CONFIG_MTK_GPUFREQ_V2)
#include <ged_gpufreq_v2.h>
#else
#include <ged_gpufreq_v1.h>
#endif /* CONFIG_MTK_GPUFREQ_V2 */

int mtk_gpueb_sysram_read(int offset);

static void __iomem *mtk_gpueb_dvfs_sysram_base_addr;
// should sync with PIN_R_MSG_SIZE_FAST_DVFS
uint32_t fdvfs_ipi_rcv_msg[6];
static int g_fast_dvfs_ipi_channel = -1;

int g_is_fastdvfs_enable;
int g_is_fulltrace_enable;

enum action_map {
	ACTION_MAP_FASTDVFS = 0,
	ACTION_MAP_FULLTRACE = 1,

	NR_ACTION_MAP
};

unsigned int fastdvfs_mode = 1;
static struct hrtimer g_HT_fdvfs_debug;
#define GED_FDVFS_TIMER_TIMEOUT 1000000 // 1ms

#define DVFS_trace_counter(name, value) \
	ged_log_perf_trace_counter(name, value, 5566, 0, 0)

static DEFINE_SPINLOCK(counter_info_lock);
static int mfg_is_power_on;

#define FDVFS_IPI_ATTR "ipi_dev:%p, ch:%d, DATA_LEN: %d, TIMEOUT: %d(ms)"

int ged_to_fdvfs_command(unsigned int cmd, struct fdvfs_ipi_data *ipi_data)
{
	int ret = 0;

	/* init ipi channel */
	if (g_fast_dvfs_ipi_channel <= 0) {
		g_fast_dvfs_ipi_channel =
			gpueb_get_send_PIN_ID_by_name("IPI_ID_FAST_DVFS");
		if (unlikely(g_fast_dvfs_ipi_channel <= 0)) {
			GPUFDVFS_LOGE("fail to get fast dvfs IPI channel id (ENOENT)");
			g_is_fastdvfs_enable = -1;

			return -ENOENT;
		}

		mtk_ipi_register(get_gpueb_ipidev(), g_fast_dvfs_ipi_channel,
			NULL, NULL, (void *)&fdvfs_ipi_rcv_msg);
	}

	GPUFDVFS_LOGD("%s(%d), cmd: %d, channel: %d, ipi_size: %d (X4)\n",
		__func__,
		__LINE__,
		cmd,
		g_fast_dvfs_ipi_channel,
		FDVFS_IPI_DATA_LEN);

	if (ipi_data != NULL) {
		ipi_data->cmd = cmd;
	} else {
		GPUFDVFS_LOGI("%s(%d), cmd: %d, invalid ipi_data = NULL\n",
			__func__, __LINE__, cmd);
		return -ENOENT;
	}

	switch (cmd) {
	// Set +
	case GPUFDVFS_IPI_SET_FRAME_DONE:
	case GPUFDVFS_IPI_SET_NEW_FREQ:
	case GPUFDVFS_IPI_SET_FRAME_BASE_DVFS:
	case GPUFDVFS_IPI_SET_TARGET_FRAME_TIME:
	case GPUFDVFS_IPI_SET_FRAG_DONE_INTERVAL:
	case GPUFDVFS_IPI_SET_MODE:
		ret = mtk_ipi_send_compl(get_gpueb_ipidev(),
			g_fast_dvfs_ipi_channel,
			IPI_SEND_POLLING, ipi_data,
			FDVFS_IPI_DATA_LEN,
			FASTDVFS_IPI_TIMEOUT);

		if (ret != 0) {
			GPUFDVFS_LOGI("%s(%d), cmd: %d, ret: %d, data: %p,"FDVFS_IPI_ATTR"\n",
				__func__, __LINE__, cmd, ret, ipi_data,
				get_gpueb_ipidev(),
				g_fast_dvfs_ipi_channel,
				FDVFS_IPI_DATA_LEN, FASTDVFS_IPI_TIMEOUT);
		} else {
			ret = fdvfs_ipi_rcv_msg[0];
		}
	break;
	// Set -

	// Get +
	case GPUFDVFS_IPI_GET_FRAME_LOADING:
	case GPUFDVFS_IPI_GET_CURR_FREQ:
		ret = mtk_ipi_send_compl(get_gpueb_ipidev(),
			g_fast_dvfs_ipi_channel,
			IPI_SEND_POLLING, ipi_data,
			FDVFS_IPI_DATA_LEN,
			FASTDVFS_IPI_TIMEOUT);

		if (ret != 0) {
			GPUFDVFS_LOGI("%s(%d), cmd: %d, mtk_ipi_send_compl, ret: %d\n",
				__func__, __LINE__, cmd, ret);
		} else {
			ret = fdvfs_ipi_rcv_msg[0];
		}
	break;
	// Get

	case GPUFDVFS_IPI_PMU_START:
		ret = mtk_ipi_send_compl(get_gpueb_ipidev(),
			g_fast_dvfs_ipi_channel,
			IPI_SEND_POLLING, ipi_data,
			FDVFS_IPI_DATA_LEN,
			FASTDVFS_IPI_TIMEOUT);

		if (ret != 0) {
			GPUFDVFS_LOGI("%s(%d), cmd: %d, mtk_ipi_send_compl, ret: %d\n",
				__func__, __LINE__, cmd, ret);
		}
	break;

	default:
		GPUFDVFS_LOGI("%s(%d), cmd: %d wrong!!!\n",
			__func__, __LINE__, cmd);
	break;
	}

	GPUFDVFS_LOGD("%s(%d), cmd: %d, fdvfs_ipi_rcv_msg[0]: %d\n",
		__func__,
		__LINE__,
		cmd,
		fdvfs_ipi_rcv_msg[0]);

	return ret;
}

void mtk_gpueb_dvfs_commit(unsigned long ui32NewFreqID,
		GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited)
{
	int ret;
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = ui32NewFreqID;
	ipi_data.u.set_para.arg[1] = eCommitType;
	ipi_data.u.set_para.arg[2] = 0;
	ipi_data.u.set_para.arg[3] = 0xFFFFFFFF;

	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_NEW_FREQ, &ipi_data);

	if (pbCommited) {
		if (ret == 0)
			*pbCommited = true;
		else
			*pbCommited = false;
	}
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_commit);

void mtk_gpueb_dvfs_dcs_commit(unsigned int platform_freq_idx,
		GED_DVFS_COMMIT_TYPE eCommitType,
		unsigned int virtual_freq_in_MHz)
{
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = platform_freq_idx;
	ipi_data.u.set_para.arg[1] = eCommitType;
	ipi_data.u.set_para.arg[2] = virtual_freq_in_MHz;
	ipi_data.u.set_para.arg[3] = 0xFFFFFFFF;

	ged_to_fdvfs_command(GPUFDVFS_IPI_SET_NEW_FREQ, &ipi_data);
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_dcs_commit);

int mtk_gpueb_dvfs_set_frame_done(void)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_FRAME_DONE, &ipi_data);

	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_frame_done);

unsigned int mtk_gpueb_dvfs_set_frag_done_interval(int frag_done_interval_in_ns)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = (unsigned int)frag_done_interval_in_ns;

	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_FRAG_DONE_INTERVAL,
		&ipi_data);

	return (ret > 0) ? ret:0xFFFFFFFF;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_frag_done_interval);


unsigned int mtk_gpueb_dvfs_set_frame_base_dvfs(unsigned int enable)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = (enable && g_is_fastdvfs_enable);
	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_FRAME_BASE_DVFS, &ipi_data);

	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_frame_base_dvfs);


int mtk_gpueb_dvfs_set_taget_frame_time(unsigned int target_frame_time)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = target_frame_time;
	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_TARGET_FRAME_TIME, &ipi_data);

	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_taget_frame_time);

unsigned int mtk_gpueb_dvfs_set_mode(unsigned int action)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = action;
	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_MODE, &ipi_data);

	fastdvfs_mode = action;

	g_is_fastdvfs_enable =
		((action & (0x1 << ACTION_MAP_FASTDVFS)))>>ACTION_MAP_FASTDVFS;
	g_is_fulltrace_enable =
		((action & (0x1 << ACTION_MAP_FULLTRACE)))>>ACTION_MAP_FULLTRACE;

	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_mode);

unsigned int mtk_gpueb_dvfs_get_mode(unsigned int *pAction)
{
	int ret = 0;

	if (pAction != NULL)
		*pAction = fastdvfs_mode;

	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_mode);


unsigned int mtk_gpueb_dvfs_get_cur_freq(void)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_GET_CURR_FREQ, &ipi_data);

	return (ret > 0) ? ret:0xFFFF;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_cur_freq);

unsigned int mtk_gpueb_dvfs_get_frame_loading(void)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_GET_FRAME_LOADING, &ipi_data);

	return (ret > 0) ? ret:0xFFFF;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_frame_loading);

int mtk_gpueb_sysram_write(int offset, unsigned int val)
{
	if (!mtk_gpueb_dvfs_sysram_base_addr)
		return -EADDRNOTAVAIL;

	if ((offset % 4) != 0)
		return -EINVAL;

	__raw_writel(val, mtk_gpueb_dvfs_sysram_base_addr + offset);
	mb(); /* make sure register access in order */

	return 0;
}
EXPORT_SYMBOL(mtk_gpueb_sysram_write);

int mtk_gpueb_sysram_batch_read(int max_read_count,
	char *batch_string, int batch_str_size)
{
	if (g_is_fulltrace_enable == 0) {
		int index_freq = 0;
		int curr_str_len = 0;
		int read_freq = 0;
		int avg_freq;
		int value_cnt = 0;
		int frequency1, frequency2;


		if (!mtk_gpueb_dvfs_sysram_base_addr
			|| max_read_count <= 0
			|| batch_string == NULL)
			return 0;

		curr_str_len = 0;
		avg_freq = 0;
		for (index_freq = 0 ; index_freq < max_read_count; index_freq++) {
			read_freq = (__raw_readl(mtk_gpueb_dvfs_sysram_base_addr +
				SYSRAM_GPU_CURR_FREQ + (index_freq<<2)));

			frequency1 = ((read_freq>>16)&0xffff);
			if (frequency1 > 0) {
				value_cnt++;
				avg_freq += frequency1;
				curr_str_len += snprintf(batch_string + curr_str_len,
					batch_str_size, "|%d", frequency1);

				frequency2 = ((read_freq)&0x0000ffff);
				if (frequency2 > 0) {
					value_cnt++;
					avg_freq += frequency2;
					curr_str_len += snprintf(batch_string + curr_str_len,
						batch_str_size, "|%d", frequency2);
				} else
					break;
			} else {
				GPUFDVFS_LOGD("batch_string: %s, index_freq: %d\n",
					batch_string, index_freq);

				break;
			}

			GPUFDVFS_LOGD("read_freq: 0x%x, frequency1: %d, frequency2: %d\n",
				read_freq, frequency1, frequency2);

			/* Reset the read data */
			__raw_writel(0, mtk_gpueb_dvfs_sysram_base_addr +
			SYSRAM_GPU_CURR_FREQ + (index_freq<<2));
		}

		avg_freq /= value_cnt;

		return avg_freq;

	} else {
		GPUFDVFS_LOGD("QQ - %s(). cur_freq: %d", __func__, (ged_get_cur_freq() / 1000));
		return (ged_get_cur_freq() / 1000);
	}

	return -1;
}
EXPORT_SYMBOL(mtk_gpueb_sysram_batch_read);

int mtk_gpueb_sysram_read(int offset)
{
	if (!mtk_gpueb_dvfs_sysram_base_addr)
		return -1;

	if ((offset % 4) != 0)
		return -1;

	return (int)(__raw_readl(mtk_gpueb_dvfs_sysram_base_addr + offset));
}
EXPORT_SYMBOL(mtk_gpueb_sysram_read);

static int fastdvfs_proc_show(struct seq_file *m, void *v)
{
	char show_string[256];
	//counter = mtk_gpueb_sysram_read(0xD0);
	//seq_printf(m, "gpu freq : %u\n", counter);

	snprintf(show_string, 256, "FastDVFS enable : %d\n",
		g_is_fastdvfs_enable);
	seq_puts(m, show_string);

	snprintf(show_string, 256, "FullTrace enable : %d\n",
		g_is_fulltrace_enable);
	seq_puts(m, show_string);

	if (g_is_fulltrace_enable == 1) {
		seq_puts(m, "\n#### Frequency ####\n");

		snprintf(show_string, 256, "Current gpu freq       : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_CURR_FREQ));
		seq_puts(m, show_string);

		snprintf(show_string, 256, "Predicted gpu freq     : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_PRED_FREQ));
		seq_puts(m, show_string);

		//snprintf(show_string, 256, "Current gpu 3D loading : %d\n",
		//  mtk_gpueb_sysram_read(SYSRAM_GPU_FRAGMENT_LOADING));
		//seq_puts(m, show_string);

		seq_puts(m, "\n#### Workload ####\n");
		snprintf(show_string, 256, "Predicted workload     : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_PRED_WORKLOAD));
		seq_puts(m, show_string);

		snprintf(show_string, 256, "Left workload          : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_LEFT_WL));
		seq_puts(m, show_string);

		snprintf(show_string, 256, "Finish workload        : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_FINISHED_WORKLOAD));
		seq_puts(m, show_string);

		snprintf(show_string, 256, "Under Hint WL          : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_UNDER_HINT_WL));
		seq_puts(m, show_string);

		seq_puts(m, "\n#### Time Budget ####\n");
		snprintf(show_string, 256, "Target time            : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_TARGET_TIME));
		seq_puts(m, show_string);

		//snprintf(show_string, 256, "Elapsed time         : %d\n",
		//  mtk_gpueb_sysram_read(SYSRAM_GPU_ELAPSED_TIME));
		//seq_puts(m, show_string);

		snprintf(show_string, 256, "Left time              : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_LEFT_TIME));
		seq_puts(m, show_string);

		seq_puts(m, "\n#### Interval ####\n");
		snprintf(show_string, 256, "Kernel Frame Done Interval     : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_KERNEL_FRAME_DONE_INTERVAL));
		seq_puts(m, show_string);

		snprintf(show_string, 256, "EB Frame Done Interval         : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_EB_FRAME_DONE_INTERVAL));
		seq_puts(m, show_string);
		seq_puts(m, "\n\n\n");
	}

	return 0;
}

static ssize_t
fastdvfs_proc_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *f_pos)
{
	char desc[32];
	int len = 0;
	//int gpu_freq = 0;
	int action = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	// bit 0 : enable/disable FastDVFS
	// bit 1 : Full systrace
	if (!kstrtoint(desc, 10, &action)) {
		g_is_fastdvfs_enable =
			((action & (0x1 << ACTION_MAP_FASTDVFS)))>>ACTION_MAP_FASTDVFS;
		g_is_fulltrace_enable =
			((action & (0x1 << ACTION_MAP_FULLTRACE)))>>ACTION_MAP_FULLTRACE;

		mtk_gpueb_dvfs_set_mode(action);
	}

	if (g_is_fulltrace_enable == 1) {
		if (hrtimer_try_to_cancel(&g_HT_fdvfs_debug)) {
			/* Timer is either queued or in cb
			 * cancel it to ensure it is not bother any way
			 */
			hrtimer_cancel(&g_HT_fdvfs_debug);
			hrtimer_start(&g_HT_fdvfs_debug,
				ns_to_ktime(GED_FDVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
		} else {
			/*
			 * Timer is not existed
			 */
			hrtimer_start(&g_HT_fdvfs_debug,
				ns_to_ktime(GED_FDVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
		}
	} else {
		if (hrtimer_try_to_cancel(&g_HT_fdvfs_debug)) {
			/* Timer is either queued or in cb
			 * cancel it to ensure it is not bother any way
			 */
			hrtimer_cancel(&g_HT_fdvfs_debug);
		}
	}

	return count;
}

static int fastdvfs_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, fastdvfs_proc_show, NULL);
}

static const struct proc_ops fastdvfs_proc_fops = {
	.proc_open = fastdvfs_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = fastdvfs_proc_write,
};

void ged_fastdvfs_systrace(void)
{
	DVFS_trace_counter("(FDVFS)Curr GPU freq",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_CURR_FREQ));
	DVFS_trace_counter("(FDVFS)Pred WL",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_PRED_WORKLOAD));
	DVFS_trace_counter("(FDVFS)Finish WL",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_FINISHED_WORKLOAD));
	DVFS_trace_counter("(FDVFS)EB_FRAME_INTL",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_EB_FRAME_DONE_INTERVAL));
	DVFS_trace_counter("(FDVFS)GED_FRAME_INTL",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_KERNEL_FRAME_DONE_INTERVAL));
	DVFS_trace_counter("(FDVFS)TARGET TIME",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_TARGET_TIME));
	DVFS_trace_counter("(FDVFS)3D loading",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_FRAGMENT_LOADING));
	DVFS_trace_counter("(FDVFS)Frame End Hint Cnt",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_FRAME_END_HINT_CNT));
	DVFS_trace_counter("(FDVFS)Pred GPU freq",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_PRED_FREQ));
	DVFS_trace_counter("(FDVFS)FRAME BOUNDARY",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_TARGET_FRAME_BOUNDARY));
	DVFS_trace_counter("(FDVFS)LEFT WL",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_LEFT_WL));
	DVFS_trace_counter("(FDVFS)ELAPSED TIME",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_ELAPSED_TIME));
	DVFS_trace_counter("(FDVFS)UNDER HINT WL",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_UNDER_HINT_WL));
	DVFS_trace_counter("(FDVFS)LEFT TIME",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_LEFT_TIME));
	DVFS_trace_counter("(FDVFS)UNDER HINT CNT",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_UNDER_HINT_CNT));

}



enum hrtimer_restart ged_fdvfs_debug_cb(struct hrtimer *timer)
{
	if (g_is_fulltrace_enable == 1) {
		ged_fastdvfs_systrace();
		hrtimer_start(&g_HT_fdvfs_debug,
			ns_to_ktime(GED_FDVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
	}

	return HRTIMER_NORESTART;
}


static void __iomem *_gpu_fastdvfs_of_ioremap(const char *node_name)
{
	struct device_node *node = NULL;
	void __iomem *mapped_addr = NULL;

	node = of_find_compatible_node(NULL, NULL, node_name);

	if (node) {
		mapped_addr = of_iomap(node, 0);
		GPUFDVFS_LOGI("@%d mapped_addr: %p\n", __LINE__, mapped_addr);
		of_node_put(node);
	} else
		GPUFDVFS_LOGE("#@# %s:(%s::%d) Cannot find [%s] of_node\n", "FDVFS",
			__FILE__, __LINE__, node_name);

	return mapped_addr;
}

static void gpu_power_change_notify_fdvfs(int power_on)
{
	spin_lock(&counter_info_lock);

	GPUFDVFS_LOGD("%s() power on: %d\n", __func__, power_on);

	if (g_is_fulltrace_enable == 1) {
		if (power_on) {
			if (!hrtimer_active(&g_HT_fdvfs_debug)) {
				hrtimer_start(&g_HT_fdvfs_debug,
					ns_to_ktime(GED_FDVFS_TIMER_TIMEOUT),
					HRTIMER_MODE_REL);
			}
		}
	}

	mfg_is_power_on = power_on;

	spin_unlock(&counter_info_lock);
}

void fdvfs_init(void)
{
	/* init sysram for debug */
	g_is_fastdvfs_enable = 1;
	g_is_fulltrace_enable = 0;

	mtk_gpueb_dvfs_sysram_base_addr =
		_gpu_fastdvfs_of_ioremap("mediatek,gpu_fdvfs");

	if (mtk_gpueb_dvfs_sysram_base_addr == NULL) {
		GPUFDVFS_LOGE("can't find fdvfs sysram");
		return;
	}

	g_fast_dvfs_ipi_channel = -1;

	mtk_register_gpu_power_change("fdvfs", gpu_power_change_notify_fdvfs);

	hrtimer_init(&g_HT_fdvfs_debug, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	g_HT_fdvfs_debug.function = ged_fdvfs_debug_cb;

	if (g_is_fulltrace_enable == 1)
		hrtimer_start(&g_HT_fdvfs_debug,
			ns_to_ktime(GED_FDVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
}

int fastdvfs_proc_init(void)
{
	proc_create("fastdvfs_proc", 0660, NULL, &fastdvfs_proc_fops);

	return 0;
}

void fastdvfs_proc_exit(void)
{
	remove_proc_entry("fastdvfs_proc", NULL);
}

//MODULE_LICENSE("GPL");
//module_init(fastdvfs_proc_init);
//module_exit(fastdvfs_proc_exit);

