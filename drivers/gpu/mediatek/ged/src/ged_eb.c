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
#include "ged_global.h"

#include <mt-plat/mtk_gpu_utility.h>

#if defined(CONFIG_MTK_GPUFREQ_V2)
#include <ged_gpufreq_v2.h>
#else
#include <ged_gpufreq_v1.h>
#endif /* CONFIG_MTK_GPUFREQ_V2 */

static void __iomem *mtk_gpueb_dvfs_sysram_base_addr;

struct fdvfs_ipi_rcv_data fdvfs_ipi_rcv_msg;
static int g_fast_dvfs_ipi_channel = -1;

static int g_fdvfs_event_ipi_channel = -1;
uint32_t fdvfs_event_ipi_rcv_msg[4];

int g_is_fastdvfs_enable;
int g_is_fulltrace_enable;

unsigned int fastdvfs_mode = 1;
bool need_to_refresh_mode = true;
static struct hrtimer g_HT_fdvfs_debug;
#define GED_FDVFS_TIMER_TIMEOUT 1000000 // 1ms

#define DVFS_trace_counter(name, value) \
	ged_log_perf_trace_counter(name, value, 5566, 0, 0)

static DEFINE_SPINLOCK(counter_info_lock);
static int mfg_is_power_on;

#define FDVFS_IPI_ATTR "ipi_dev:%p, ch:%d, DATA_LEN: %d, TIMEOUT: %d(ms)"

static struct workqueue_struct *g_psEBWorkQueue;

#define MAX_EB_NOTIFY_CNT 120
struct GED_EB_EVENT eb_notify[MAX_EB_NOTIFY_CNT];
int eb_notify_index;

static struct work_struct sg_notify_ged_ready_work;

static void ged_eb_work_cb(struct work_struct *psWork)
{
	struct GED_EB_EVENT *psEBEvent =
		GED_CONTAINER_OF(psWork, struct GED_EB_EVENT, sWork);

	mtk_notify_gpu_freq_change(0, psEBEvent->freq_new);
	psEBEvent->bUsed = false;
}

/*
 * handle events from EB
 * @param id    : ipi id
 * @param prdata: ipi handler parameter
 * @param data  : ipi data
 * @param len   : length of ipi data
 */
static int fast_dvfs_eb_event_handler(unsigned int id, void *prdata, void *data,
				    unsigned int len)
{
	struct GED_EB_EVENT *psEBEvent =
		&(eb_notify[((eb_notify_index++) % MAX_EB_NOTIFY_CNT)]);

	if (eb_notify_index >= MAX_EB_NOTIFY_CNT)
		eb_notify_index = 0;

	if (data != NULL && psEBEvent && psEBEvent->bUsed == false) {
		psEBEvent->freq_new = ((struct fastdvfs_event_data *)data)->u.set_para.arg[0];

		/*get rate from EB*/
		GPUFDVFS_LOGD("%s@%d top clock: %d (KHz)\n",
			__func__, __LINE__, psEBEvent->freq_new);

		psEBEvent->bUsed = true;

		INIT_WORK(&psEBEvent->sWork, ged_eb_work_cb);
		queue_work(g_psEBWorkQueue, &psEBEvent->sWork);
	}

	return 0;
}

int ged_to_fdvfs_command(unsigned int cmd, struct fdvfs_ipi_data *ipi_data)
{
	int ret = 0;
	ktime_t cmd_start, cmd_now, cmd_duration;

	if (ipi_data != NULL &&
		g_fast_dvfs_ipi_channel >= 0 && g_fdvfs_event_ipi_channel >= 0) {
		ipi_data->cmd = cmd;
	} else {
		GPUFDVFS_LOGI("(%d), Can't send cmd(%d) ipi_data:%p, ch:(%d)(%d)\n",
			__LINE__, cmd, ipi_data,
			g_fast_dvfs_ipi_channel, g_fdvfs_event_ipi_channel);
		return -ENOENT;
	}

	GPUFDVFS_LOGD("(%d), send cmd: %d, msg[0]: %d\n",
		__LINE__,
		cmd,
		ipi_data->u.set_para.arg[0]);

	cmd_start = ktime_get();
	ipi_data->u.set_para.arg[3] = (cmd_start & 0xFFFFFFFF00000000) >> 32;
	ipi_data->u.set_para.arg[4] = (u32)(cmd_start & 0x00000000FFFFFFFF);


	switch (cmd) {
	// Set +
	case GPUFDVFS_IPI_SET_FRAME_DONE:
	case GPUFDVFS_IPI_SET_NEW_FREQ:
	case GPUFDVFS_IPI_SET_FRAME_BASE_DVFS:
	case GPUFDVFS_IPI_SET_TARGET_FRAME_TIME:
	case GPUFDVFS_IPI_SET_FEEDBACK_INFO:
	case GPUFDVFS_IPI_SET_MODE:

	case GPUFDVFS_IPI_SET_GED_READY:
		ret = mtk_ipi_send_compl(get_gpueb_ipidev(),
			g_fast_dvfs_ipi_channel,
			IPI_SEND_POLLING, ipi_data,
			FDVFS_IPI_DATA_LEN,
			FASTDVFS_IPI_TIMEOUT);

		if (ret != 0) {
			GPUFDVFS_LOGI("(%d), cmd: %d, ret: %d, data: %p,"FDVFS_IPI_ATTR"\n",
				__LINE__, cmd, ret, ipi_data,
				get_gpueb_ipidev(),
				g_fast_dvfs_ipi_channel,
				FDVFS_IPI_DATA_LEN, FASTDVFS_IPI_TIMEOUT);
		} else {
			ret = fdvfs_ipi_rcv_msg.u.set_para.arg[0];
		}
	break;
	// Set -

	// Get +
	case GPUFDVFS_IPI_GET_FRAME_LOADING:
	case GPUFDVFS_IPI_GET_CURR_FREQ:
	case GPUFDVFS_IPI_GET_MODE:
		ret = mtk_ipi_send_compl(get_gpueb_ipidev(),
			g_fast_dvfs_ipi_channel,
			IPI_SEND_POLLING, ipi_data,
			FDVFS_IPI_DATA_LEN,
			FASTDVFS_IPI_TIMEOUT);

		if (ret != 0) {
			GPUFDVFS_LOGI("(%d), cmd: %u, mtk_ipi_send_compl, ret: %d\n",
				__LINE__, cmd, ret);
		} else {
			ret = fdvfs_ipi_rcv_msg.u.set_para.arg[0];
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
			GPUFDVFS_LOGI("(%d), cmd: %d, mtk_ipi_send_compl, ret: %d\n",
				__LINE__, cmd, ret);
		}
	break;

	default:
		GPUFDVFS_LOGI("(%d), cmd: %d wrong!!!\n",
			__LINE__, cmd);
	break;
	}

	cmd_now = ktime_get();
	cmd_duration = ktime_sub(cmd_now,
		(((u64)((u64)(fdvfs_ipi_rcv_msg.u.set_para.arg[3]) << 32) & 0xFFFFFFFF00000000) +
		fdvfs_ipi_rcv_msg.u.set_para.arg[4]));

	GPUFDVFS_LOGD("(%d), cmd: %d, ack cmd: %d, msg[0]: %d. IPI duration: %llu ns(%llu ns)\n",
		__LINE__,
		cmd,
		fdvfs_ipi_rcv_msg.cmd,
		fdvfs_ipi_rcv_msg.u.set_para.arg[0],
		ktime_to_ns(cmd_duration), ktime_to_ns(ktime_sub(cmd_now, cmd_start)));

	return ret;
}

void mtk_gpueb_dvfs_commit(unsigned long ulNewFreqID,
		GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited)
{
	int ret = 0;

	struct fdvfs_ipi_data ipi_data;
	static unsigned long ulPreFreqID = -1;

	if (ulNewFreqID != ulPreFreqID) {
#ifdef FDVFS_REDUCE_IPI
		mtk_gpueb_sysram_write(SYSRAM_GPU_COMMIT_PLATFORM_FREQ_IDX,
			ulNewFreqID);
		mtk_gpueb_sysram_write(SYSRAM_GPU_COMMIT_TYPE,
			eCommitType);
		mtk_gpueb_sysram_write(SYSRAM_GPU_COMMIT_VIRTUAL_FREQ,
			0);
#else
		ipi_data.u.set_para.arg[0] = ulNewFreqID;
		ipi_data.u.set_para.arg[1] = eCommitType;
		ipi_data.u.set_para.arg[2] = 0;
		ipi_data.u.set_para.arg[3] = 0xFFFFFFFF;

		ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_NEW_FREQ, &ipi_data);
#endif
		if (pbCommited) {
			if (ret == 0)
				*pbCommited = true;
			else
				*pbCommited = false;
		}
	} else {
		if (pbCommited)
			*pbCommited = true;
	}

	ulPreFreqID = ulNewFreqID;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_commit);

void mtk_gpueb_dvfs_dcs_commit(unsigned int platform_freq_idx,
		GED_DVFS_COMMIT_TYPE eCommitType,
		unsigned int virtual_freq_in_MHz)
{
	struct fdvfs_ipi_data ipi_data;
	static unsigned int pre_platform_freq_idx = -1;
	static unsigned int pre_virtual_freq_in_MHz = -1;

	if (platform_freq_idx != pre_platform_freq_idx ||
		virtual_freq_in_MHz != pre_virtual_freq_in_MHz) {
#ifdef FDVFS_REDUCE_IPI
		mtk_gpueb_sysram_write(SYSRAM_GPU_COMMIT_PLATFORM_FREQ_IDX,
			platform_freq_idx);
		mtk_gpueb_sysram_write(SYSRAM_GPU_COMMIT_TYPE,
			eCommitType);
		mtk_gpueb_sysram_write(SYSRAM_GPU_COMMIT_VIRTUAL_FREQ,
			virtual_freq_in_MHz);
#else
		ipi_data.u.set_para.arg[0] = platform_freq_idx;
		ipi_data.u.set_para.arg[1] = eCommitType;
		ipi_data.u.set_para.arg[2] = virtual_freq_in_MHz;
		ipi_data.u.set_para.arg[3] = 0xFFFFFFFF;

		ged_to_fdvfs_command(GPUFDVFS_IPI_SET_NEW_FREQ, &ipi_data);
#endif
	}

	pre_platform_freq_idx = platform_freq_idx;
	pre_virtual_freq_in_MHz = virtual_freq_in_MHz;

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

unsigned int mtk_gpueb_dvfs_set_feedback_info(int frag_done_interval_in_ns,
	struct GpuUtilization_Ex util_ex, unsigned int curr_fps)
{
	int ret = 0;
	unsigned int utils = 0;

#ifdef FDVFS_REDUCE_IPI
	utils = ((util_ex.util_active&0xff)|
		((util_ex.util_3d&0xff)<<8)|
		((util_ex.util_ta&0xff)<<16)|
		((util_ex.util_compute&0xff)<<24));

	mtk_gpueb_sysram_write(SYSRAM_GPU_FEEDBACK_INFO_GPU_UTILS, utils);

	if (frag_done_interval_in_ns > 0)
		mtk_gpueb_sysram_write(SYSRAM_GPU_FEEDBACK_INFO_GPU_TIME,
			frag_done_interval_in_ns);

	if (curr_fps > 0)
		mtk_gpueb_sysram_write(SYSRAM_GPU_FEEDBACK_INFO_CURR_FPS,
			curr_fps);
	ret = mtk_gpueb_sysram_read(SYSRAM_GPU_TA_3D_COEF);
#else
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = (unsigned int)frag_done_interval_in_ns;
	ipi_data.u.set_para.arg[1] =
		(util_ex.util_active&0xff)|
		((util_ex.util_3d&0xff)<<8)|
		((util_ex.util_ta&0xff)<<16)|
		((util_ex.util_compute&0xff)<<24);

	ipi_data.u.set_para.arg[2] = (unsigned int)curr_fps;

	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_FEEDBACK_INFO,
		&ipi_data);
#endif

	return (ret > 0) ? ret:0xFFFFFFFF;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_feedback_info);


unsigned int mtk_gpueb_dvfs_set_frame_base_dvfs(unsigned int enable)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;
	static unsigned int is_fb_dvfs_enabled;

	if (enable != is_fb_dvfs_enabled) {
		ipi_data.u.set_para.arg[0] = (enable && g_is_fastdvfs_enable);
		ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_FRAME_BASE_DVFS, &ipi_data);
	}

	is_fb_dvfs_enabled = enable;

	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_frame_base_dvfs);

int mtk_gpueb_dvfs_set_taget_frame_time(unsigned int target_frame_time,
	unsigned int target_margin)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;
	static unsigned int pre_target_frame_time;
	static unsigned int pre_target_margin;

	if (g_fastdvfs_margin)
		target_margin = 999;

	if (target_frame_time != pre_target_frame_time ||
		target_margin != pre_target_margin) {
#ifdef FDVFS_REDUCE_IPI
		mtk_gpueb_sysram_write(SYSRAM_GPU_SET_TARGET_FRAME_TIME,
			target_frame_time);
		mtk_gpueb_sysram_write(SYSRAM_GPU_SET_TARGET_MARGIN,
			target_margin);
#else
		ipi_data.u.set_para.arg[0] = target_frame_time;
		ipi_data.u.set_para.arg[1] = target_margin;

		ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_TARGET_FRAME_TIME, &ipi_data);
#endif
	}

	pre_target_frame_time = target_frame_time;
	pre_target_margin = target_margin;

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

	need_to_refresh_mode = true;

	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_mode);

unsigned int mtk_gpueb_dvfs_get_mode(unsigned int *pAction)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	if (need_to_refresh_mode)
		ret = ged_to_fdvfs_command(GPUFDVFS_IPI_GET_MODE, &ipi_data);

	if (ret > 0) {
		if (pAction != NULL)
			*pAction = ret;

		fastdvfs_mode = ret;
		need_to_refresh_mode = false;
	} else {
		if (pAction != NULL)
			*pAction = fastdvfs_mode;
	}

	return fastdvfs_mode;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_mode);

int mtk_gpueb_power_modle_cmd(unsigned int enable)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = enable;
	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_PMU_START, &ipi_data);
	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_power_modle_cmd);

int mtk_set_ged_ready(int ged_ready_flag)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = ged_ready_flag;
	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_GED_READY, &ipi_data);
	return ret;
}

unsigned int is_fdvfs_enable(void)
{
	return fastdvfs_mode & 0x1;
}


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
		for (index_freq = 0 ; index_freq < max_read_count &&
			curr_str_len <= batch_str_size; index_freq++) {
			read_freq = (__raw_readl(mtk_gpueb_dvfs_sysram_base_addr +
				SYSRAM_GPU_CURR_FREQ + (index_freq<<2)));

			frequency1 = ((read_freq>>16)&0xffff);
			if (frequency1 > 0) {
				if (frequency1 <= ged_get_max_freq_in_opp()) {
					value_cnt++;
					avg_freq += frequency1;
					curr_str_len += scnprintf(batch_string + curr_str_len,
						batch_str_size, "|%d", frequency1);
				}

				frequency2 = ((read_freq)&0x0000ffff);
				if (frequency2 > 0) {
					if (frequency2 <= ged_get_max_freq_in_opp()) {
						value_cnt++;
						avg_freq += frequency2;
						curr_str_len +=
							scnprintf(batch_string + curr_str_len,
							batch_str_size, "|%d", frequency2);
					}
				} else {
					// Reset the rest data
					for (; index_freq < max_read_count; index_freq++)
						/* Reset the rest unread data */
						__raw_writel(0, mtk_gpueb_dvfs_sysram_base_addr +
							SYSRAM_GPU_CURR_FREQ + (index_freq<<2));
					break;
				}
			} else {
				GPUFDVFS_LOGD("batch_string: %s, index_freq: %d\n",
					batch_string, index_freq);

				// Reset the rest data
				for (; index_freq < max_read_count; index_freq++)
					/* Reset the rest unread data */
					__raw_writel(0, mtk_gpueb_dvfs_sysram_base_addr +
						SYSRAM_GPU_CURR_FREQ + (index_freq<<2));

				break;
			}

			GPUFDVFS_LOGD("read_freq: 0x%x, frequency1: %d, frequency2: %d\n",
				read_freq, frequency1, frequency2);

			/* Reset the read data */
			__raw_writel(0, mtk_gpueb_dvfs_sysram_base_addr +
			SYSRAM_GPU_CURR_FREQ + (index_freq<<2));
		}

		if (value_cnt > 0)
			avg_freq /= value_cnt;
		else {
			avg_freq = ged_get_cur_freq()/1000;
			curr_str_len += scnprintf(batch_string + curr_str_len,
					batch_str_size, "|%d", avg_freq);
		}

		return avg_freq;

	} else {
		GPUFDVFS_LOGD("%s(). cur_freq: %d", __func__, (ged_get_cur_freq() / 1000));
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

int mtk_gpueb_sysram_write(int offset, int val)
{
	if (!mtk_gpueb_dvfs_sysram_base_addr)
		return -EADDRNOTAVAIL;

	if ((offset % 4) != 0)
		return -EINVAL;

	__raw_writel(val, mtk_gpueb_dvfs_sysram_base_addr + offset);
	mb(); /* make sure register access in order */

	if (val != mtk_gpueb_sysram_read(offset))
		GPUFDVFS_LOGE("%s(). failed to update sysram. addr: %p, val: %d/%d",
		__func__, mtk_gpueb_dvfs_sysram_base_addr + offset, val,
		mtk_gpueb_sysram_read(offset));

	return 0;
}
EXPORT_SYMBOL(mtk_gpueb_sysram_write);

static int fastdvfs_proc_show(struct seq_file *m, void *v)
{
	char show_string[256];
	unsigned int ui32FastDVFSMode = 0;

	 mtk_gpueb_dvfs_get_mode(&ui32FastDVFSMode);

	scnprintf(show_string, 256, "FastDVFS enable : %d. (%d)\n",
		g_is_fastdvfs_enable, ui32FastDVFSMode);
	seq_puts(m, show_string);

	scnprintf(show_string, 256, "FullTrace enable : %d. (%d)\n",
		g_is_fulltrace_enable, ui32FastDVFSMode);
	seq_puts(m, show_string);

	if (g_is_fulltrace_enable == 1) {
		seq_puts(m, "\n#### Frequency ####\n");

		scnprintf(show_string, 256, "Current gpu freq       : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_CURR_FREQ));
		seq_puts(m, show_string);

		scnprintf(show_string, 256, "Predicted gpu freq     : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_PRED_FREQ));
		seq_puts(m, show_string);

		//scnprintf(show_string, 256, "Current gpu 3D loading : %d\n",
		//  mtk_gpueb_sysram_read(SYSRAM_GPU_FRAGMENT_LOADING));
		//seq_puts(m, show_string);

		seq_puts(m, "\n#### Workload ####\n");
		scnprintf(show_string, 256, "Predicted workload     : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_PRED_WORKLOAD));
		seq_puts(m, show_string);

		scnprintf(show_string, 256, "Left workload          : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_LEFT_WL));
		seq_puts(m, show_string);

		scnprintf(show_string, 256, "Finish workload        : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_FINISHED_WORKLOAD));
		seq_puts(m, show_string);

		scnprintf(show_string, 256, "Under Hint WL          : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_UNDER_HINT_WL));
		seq_puts(m, show_string);

		seq_puts(m, "\n#### Time Budget ####\n");
		scnprintf(show_string, 256, "Target time            : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_TARGET_TIME));
		seq_puts(m, show_string);

		//scnprintf(show_string, 256, "Elapsed time         : %d\n",
		//  mtk_gpueb_sysram_read(SYSRAM_GPU_ELAPSED_TIME));
		//seq_puts(m, show_string);

		scnprintf(show_string, 256, "Left time              : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_LEFT_TIME));
		seq_puts(m, show_string);

		seq_puts(m, "\n#### Interval ####\n");
		scnprintf(show_string, 256, "Kernel Frame Done Interval     : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_KERNEL_FRAME_DONE_INTERVAL));
		seq_puts(m, show_string);

		scnprintf(show_string, 256, "EB Frame Done Interval         : %d\n",
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
	DVFS_trace_counter("(FDVFS)JS0 DELTA",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_JS0_DELTA));
	DVFS_trace_counter("(FDVFS)COMMIT PROFILE",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_COMMIT_PROFILE));
	DVFS_trace_counter("(FDVFS)DCS",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_DCS));

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
	/* Do something if necessary */
	spin_lock(&counter_info_lock);

	GPUFDVFS_LOGD("%s() power on: %d\n", __func__, power_on);
	mfg_is_power_on = power_on;

	spin_unlock(&counter_info_lock);
}

static void mtk_set_ged_ready_handler(struct work_struct *work)
{
	static int retry_count;
	int ret = 0;

	do {
		retry_count += 1;
		ret = mtk_set_ged_ready(1);
		GPUFDVFS_LOGI("(attempt %d) mtk_set_ged_ready return %d", retry_count,
			ret);
	} while (ret != 0);
}

void fdvfs_init(void)
{
	g_is_fastdvfs_enable = 1;
	g_is_fulltrace_enable = 0;

	g_fast_dvfs_ipi_channel = -1;
	g_fdvfs_event_ipi_channel = -1;

	/* init ipi channel */
	if (g_fast_dvfs_ipi_channel < 0) {
		g_fast_dvfs_ipi_channel =
			gpueb_get_send_PIN_ID_by_name("IPI_ID_FAST_DVFS");
		if (unlikely(g_fast_dvfs_ipi_channel <= 0)) {
			GPUFDVFS_LOGE("fail to get fast dvfs IPI channel id (ENOENT)");
			g_is_fastdvfs_enable = -1;

			return;
		}
		mtk_ipi_register(get_gpueb_ipidev(), g_fast_dvfs_ipi_channel,
			NULL, NULL, (void *)&fdvfs_ipi_rcv_msg);
	}

	if (g_fdvfs_event_ipi_channel < 0) {
		g_fdvfs_event_ipi_channel =
			gpueb_get_recv_PIN_ID_by_name("IPI_ID_FAST_DVFS_EVENT");
		if (unlikely(g_fdvfs_event_ipi_channel < 0)) {
			GPUFDVFS_LOGE("fail to get FDVFS EVENT IPI channel id (ENOENT)");

			return;
		}
		mtk_ipi_register(get_gpueb_ipidev(), g_fdvfs_event_ipi_channel,
				(void *)fast_dvfs_eb_event_handler, NULL, &fdvfs_event_ipi_rcv_msg);
		g_psEBWorkQueue =
			alloc_ordered_workqueue("ged_eb",
				WQ_FREEZABLE | WQ_MEM_RECLAIM);

		// send ready message to GPUEB so top clock can now be handled
		if (g_ged_gpu_freq_notify_support) {
			INIT_WORK(&sg_notify_ged_ready_work, mtk_set_ged_ready_handler);
			schedule_work(&sg_notify_ged_ready_work);
		}
	}

	GPUFDVFS_LOGI("succeed to register channel: (%d)(%d), ipi_size: %u\n",
		g_fast_dvfs_ipi_channel,
		g_fdvfs_event_ipi_channel,
		FDVFS_IPI_DATA_LEN);


	/* init sysram for debug */
	mtk_gpueb_dvfs_sysram_base_addr =
		_gpu_fastdvfs_of_ioremap("mediatek,gpu_fdvfs");

	if (mtk_gpueb_dvfs_sysram_base_addr == NULL) {
		GPUFDVFS_LOGE("can't find fdvfs sysram");
		return;
	}

	mtk_register_gpu_power_change("fdvfs", gpu_power_change_notify_fdvfs);

	hrtimer_init(&g_HT_fdvfs_debug, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	g_HT_fdvfs_debug.function = ged_fdvfs_debug_cb;

	if (g_is_fulltrace_enable == 1)
		hrtimer_start(&g_HT_fdvfs_debug,
			ns_to_ktime(GED_FDVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
}

void fdvfs_exit(void)
{
	destroy_workqueue(g_psEBWorkQueue);
	mtk_unregister_gpu_power_change("fdvfs");
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

