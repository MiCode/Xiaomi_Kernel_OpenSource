/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include "audio_ipi_client_spkprotect.h"

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by kmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/kthread.h>

#include <linux/io.h>

#include <linux/wait.h>
#include <linux/time.h>

#include "audio_log.h"
#include "audio_assert.h"
#include "audio_wakelock.h"

#include "audio_task_manager.h"
#include <audio_ipi_dma.h>
#include "audio_spkprotect_msg_id.h"
#include <scp_helper.h>

#define DUMP_SMARTPA_PCM_DATA_PATH "/data/vendor/audiohal/audio_dump"
#define FRAME_BUF_SIZE (8192)
static struct wakeup_source wakelock_scp_dump_lock;

enum { /* dump_data_t */
	DUMP_PCM_PRE = 0,
	DUMP_IV_DATA = 1,
	DUMP_DEBUG_DATA = 2,
	NUM_DUMP_DATA,
};


struct dump_work_t {
	struct work_struct work;
	uint32_t rw_idx;
	uint32_t data_size;
};


struct dump_package_t {
	uint8_t dump_data_type;
	uint32_t rw_idx;
	uint32_t data_size;
};


struct dump_queue_t {
	struct dump_package_t dump_package[256];
	uint8_t idx_r;
	uint8_t idx_w;
};

struct scp_spk_reserved_mem_t {
	char *start_phy;
	char *start_virt;
	uint32_t size;
};

static struct task_struct *spk_dump_split_task;
static struct dump_queue_t *dump_queue;
static DEFINE_SPINLOCK(dump_queue_lock);
static struct dump_work_t dump_work[NUM_DUMP_DATA];
static struct workqueue_struct *dump_workqueue[NUM_DUMP_DATA];
static struct task_struct *speaker_dump_task;
static int spk_pcm_dump_split_kthread(void *data);
static int spkprotect_dump_kthread(void *data);
static wait_queue_head_t wq_dump_pcm;
static uint32_t dump_data_routine_cnt_pass;
static bool b_enable_dump;
static bool split_dump_enable;
static struct file *file_spk_pcm;
static struct file *file_spk_iv;
static struct file *file_spk_debug;
static struct scp_spk_reserved_mem_t scp_spk_dump_mem;

struct pcm_dump_t {
	char decode_pcm[FRAME_BUF_SIZE];
};

void spk_pcm_dump_split_task_enable(void)
{
	/* only enable when debug pcm dump on */
	aud_wake_lock(&wakelock_scp_dump_lock);

	if (dump_queue == NULL) {
		dump_queue = kmalloc(sizeof(struct dump_queue_t), GFP_KERNEL);
		if (dump_queue != NULL)
			memset(dump_queue, 0, sizeof(struct dump_queue_t));
	}

	if (!spk_dump_split_task) {
		spk_dump_split_task =
			kthread_create(spk_pcm_dump_split_kthread,
				       NULL,
				       "spk_pcm_dump_split_kthread");

		if (IS_ERR(spk_dump_split_task))
			AUD_LOG_E(
				"can not create spk_pcm_dump_split_kthread\n");

		split_dump_enable = true;
		wake_up_process(spk_dump_split_task);
	}

}

void spk_pcm_dump_split_task_disable(void)
{
	if (split_dump_enable == false)
		return;

	split_dump_enable = false;

	if (spk_dump_split_task) {
		kthread_stop(spk_dump_split_task);
		spk_dump_split_task = NULL;
	}
	AUD_LOG_D("%s(), dump_queue = %p\n", __func__, dump_queue);
	kfree(dump_queue);
	dump_queue = NULL;

	aud_wake_unlock(&wakelock_scp_dump_lock);
}

int spkprotect_open_dump_file(void)
{
	struct timespec curr_tm;
	char string_time[16];
	char string_decode_pcm[16] = "spk_dump.pcm";
	char string_iv_pcm[16] = "spk_ivdump.pcm";
	char string_debug_pcm[16] = "spk_ddump.pcm";
	char path_decode_pcm[64];
	char path_decode_ivpcm[64];
	char path_decode_debugpcm[64];

	/* only enable when debug pcm dump on */
	aud_wake_lock(&wakelock_scp_dump_lock);
	getnstimeofday(&curr_tm);

	memset(string_time, '\0', 16);
	sprintf(string_time, "%.2lu_%.2lu_%.2lu",
		(8 + (curr_tm.tv_sec / 3600)) % (24),
		(curr_tm.tv_sec / 60) % (60),
		curr_tm.tv_sec % 60);

	sprintf(path_decode_pcm, "%s/%s_%s",
		DUMP_SMARTPA_PCM_DATA_PATH, string_time, string_decode_pcm);
	pr_debug("%s(), path_decode_pcm= %s\n", __func__, path_decode_pcm);
	sprintf(path_decode_ivpcm, "%s/%s_%s",
		DUMP_SMARTPA_PCM_DATA_PATH, string_time, string_iv_pcm);
	pr_debug("%s(), path_decode_iv_pcm= %s\n",
		 __func__, path_decode_ivpcm);
	sprintf(path_decode_debugpcm, "%s/%s_%s",
		DUMP_SMARTPA_PCM_DATA_PATH, string_time, string_debug_pcm);
	pr_debug("%s(), path_decode_debug_pcm= %s\n", __func__,
		path_decode_debugpcm);

	file_spk_pcm = filp_open(path_decode_pcm, O_CREAT | O_WRONLY, 0644);
	if (IS_ERR(file_spk_pcm)) {
		pr_info("%s(), %s file open error: %ld\n",
			__func__, path_decode_pcm, PTR_ERR(file_spk_pcm));
		return PTR_ERR(file_spk_pcm);
	}

	file_spk_iv = filp_open(path_decode_ivpcm, O_CREAT | O_WRONLY, 0644);
	if (IS_ERR(file_spk_iv)) {
		pr_info("%s(), %s file open error: %ld\n",
			__func__, path_decode_ivpcm, PTR_ERR(file_spk_iv));
		return PTR_ERR(file_spk_iv);
	}

	file_spk_debug = filp_open(path_decode_debugpcm,
				   O_CREAT | O_WRONLY, 0644);
	if (IS_ERR(file_spk_debug)) {
		pr_info("%s(), %s file open error: %ld\n",
			__func__, path_decode_debugpcm,
			PTR_ERR(file_spk_debug));
		return PTR_ERR(file_spk_debug);
	}

	if (dump_queue == NULL) {
		dump_queue = kmalloc(sizeof(struct dump_queue_t), GFP_KERNEL);
		if (dump_queue != NULL)
			memset_io(dump_queue, 0, sizeof(struct dump_queue_t));
	}

	if (!speaker_dump_task) {
		speaker_dump_task = kthread_create(spkprotect_dump_kthread,
						   NULL,
						   "spkprotect_dump_kthread");
		if (IS_ERR(speaker_dump_task))
			pr_notice("can not create speaker_dump_task kthread\n");

		b_enable_dump = true;
		wake_up_process(speaker_dump_task);
	}

	dump_data_routine_cnt_pass = 0;

	return 0;
}


void spkprotect_close_dump_file(void)
{
	if (b_enable_dump == false)
		return;

	b_enable_dump = false;

	pr_debug("%s(), pass: %d\n", __func__, dump_data_routine_cnt_pass);

	if (speaker_dump_task) {
		kthread_stop(speaker_dump_task);
		speaker_dump_task = NULL;
	}
	pr_debug("dump_queue = %p\n", dump_queue);
	kfree(dump_queue);
	dump_queue = NULL;

	if (!IS_ERR(file_spk_pcm)) {
		filp_close(file_spk_pcm, NULL);
		file_spk_pcm = NULL;
	}
	if (!IS_ERR(file_spk_iv)) {
		filp_close(file_spk_iv, NULL);
		file_spk_iv = NULL;
	}
	if (!IS_ERR(file_spk_debug)) {
		filp_close(file_spk_debug, NULL);
		file_spk_debug = NULL;
	}
	aud_wake_unlock(&wakelock_scp_dump_lock);
}

static void spk_dump_data_routine(struct work_struct *ws)
{
	struct dump_work_t *dump_work = NULL;
	uint32_t rw_idx = 0;
	uint32_t data_size = 0;
	unsigned long flags = 0;

	dump_work = container_of(ws, struct dump_work_t, work);

	rw_idx = dump_work->rw_idx;
	data_size = dump_work->data_size;

	spin_lock_irqsave(&dump_queue_lock, flags);
	dump_queue->dump_package[dump_queue->idx_w].dump_data_type =
		DUMP_PCM_PRE;
	dump_queue->dump_package[dump_queue->idx_w].rw_idx = rw_idx;
	dump_queue->dump_package[dump_queue->idx_w].data_size = data_size;
	dump_queue->idx_w++;
	spin_unlock_irqrestore(&dump_queue_lock, flags);

	wake_up_interruptible(&wq_dump_pcm);
}


static void spk_dump_ivdata_routine(struct work_struct *ws)
{
	struct dump_work_t *dump_work = NULL;
	uint32_t rw_idx = 0;
	uint32_t data_size = 0;
	unsigned long flags = 0;

	dump_work = container_of(ws, struct dump_work_t, work);

	rw_idx = dump_work->rw_idx;
	data_size = dump_work->data_size;


	spin_lock_irqsave(&dump_queue_lock, flags);
	dump_queue->dump_package[dump_queue->idx_w].dump_data_type =
		DUMP_IV_DATA;
	dump_queue->dump_package[dump_queue->idx_w].rw_idx = rw_idx;
	dump_queue->dump_package[dump_queue->idx_w].data_size = data_size;
	dump_queue->idx_w++;
	spin_unlock_irqrestore(&dump_queue_lock, flags);

	wake_up_interruptible(&wq_dump_pcm);
}

static void spk_dump_debug_data_routine(struct work_struct *ws)
{
	struct dump_work_t *dump_work = NULL;
	uint32_t rw_idx = 0;
	uint32_t data_size = 0;
	unsigned long flags = 0;

	dump_work = container_of(ws, struct dump_work_t, work);

	rw_idx = dump_work->rw_idx;
	data_size = dump_work->data_size;

	spin_lock_irqsave(&dump_queue_lock, flags);
	dump_queue->dump_package[dump_queue->idx_w].dump_data_type =
		DUMP_DEBUG_DATA;
	dump_queue->dump_package[dump_queue->idx_w].rw_idx = rw_idx;
	dump_queue->dump_package[dump_queue->idx_w].data_size = data_size;
	dump_queue->idx_w++;
	spin_unlock_irqrestore(&dump_queue_lock, flags);

	wake_up_interruptible(&wq_dump_pcm);
}

void spkprotect_dump_message(struct ipi_msg_t *ipi_msg)
{
	int ret = 0;
	uint32_t dump_idx = 0; /* dump_data_t */
	uint32_t *temp_payload = (uint32_t *)(ipi_msg->payload);

	/*  payload[0]: dump id(dump_data_t)
	 *  payload[1]: write data size
	 *  payload[2]: dump buffer write pointer offset
	 */
	dump_idx = *temp_payload;

	if (ipi_msg == NULL) {
		pr_info("%s err\n", __func__);
		return;
	} else if (ipi_msg->msg_id == SPK_PROTECT_PCMDUMP_OK) {
		dump_work[dump_idx].data_size  = *(temp_payload + 1);
		dump_work[dump_idx].rw_idx = *(temp_payload + 2);
		ret = queue_work(dump_workqueue[dump_idx],
				 &dump_work[dump_idx].work);
		if (ret == 0)
			dump_data_routine_cnt_pass++;
	} else
		pr_info("%s unknown command\n", __func__);

}

static int spk_pcm_dump_split_kthread(void *data)
{
	unsigned long flags = 0;
	int ret = 0;
	uint8_t current_idx = 0;
	int size = 0, writedata = 0;
	struct pcm_dump_t *pcm_dump = NULL;
	mm_segment_t old_fs;
	struct dump_package_t *dump_package = NULL;

	/* RTPM_PRIO_AUDIO_PLAYBACK */
	struct sched_param param = {.sched_priority = 85};

	char spk_debug_pcm_str[16] = "spk_debug_dump";
	char iv_pcm_str[16] = "spk_ivdump";
	char spk_debug_pcm_path[64];
	char spk_iv_pcm_path[64];
	int debug_file_cnt = 0;
	int iv_file_cnt = 0;
	bool debug_file_open = false;
	bool iv_file_open = false;
	int iv_dump_cnt = 0;
	int debug_dump_cnt = 0;
	int blocks = 32; /* collect 32 buffer */

	sched_setscheduler(current, SCHED_RR, &param);

	AUD_LOG_D("%s(), dump_queue = %p\n", __func__, dump_queue);

	while (split_dump_enable && !kthread_should_stop()) {

		if (!debug_file_open) {
			sprintf(spk_debug_pcm_path, "%s/%s_%d.pcm",
				DUMP_SMARTPA_PCM_DATA_PATH,
				spk_debug_pcm_str, debug_file_cnt);
			file_spk_debug = filp_open(spk_debug_pcm_path,
						   O_CREAT | O_WRONLY, 0644);
			if (IS_ERR(file_spk_debug)) {
				AUD_LOG_W("%s(), %s file open error: %ld\n",
					  __func__, spk_debug_pcm_path,
					  PTR_ERR(file_spk_debug));
				return PTR_ERR(file_spk_debug);
			}

			debug_file_open = true;

			if (debug_file_cnt < 100)
				debug_file_cnt++;
			else
				debug_file_cnt = 0;
		}

		if (!iv_file_open) {
			sprintf(spk_iv_pcm_path, "%s/%s_%d.pcm",
				DUMP_SMARTPA_PCM_DATA_PATH,
				iv_pcm_str, iv_file_cnt);
			file_spk_iv = filp_open(spk_iv_pcm_path,
						O_CREAT | O_WRONLY, 0644);
			if (IS_ERR(file_spk_iv)) {
				AUD_LOG_W("%s(), %s file open error: %ld\n",
					  __func__, spk_iv_pcm_path,
					  PTR_ERR(file_spk_iv));
				return PTR_ERR(file_spk_iv);
			}

			iv_file_open = true;

			if (iv_file_cnt < 100)
				iv_file_cnt++;
			else
				iv_file_cnt = 0;
		}

		spin_lock_irqsave(&dump_queue_lock, flags);
		if (dump_queue->idx_r != dump_queue->idx_w) {
			current_idx = dump_queue->idx_r;
			dump_queue->idx_r++;
			spin_unlock_irqrestore(&dump_queue_lock, flags);
		} else {
			spin_unlock_irqrestore(&dump_queue_lock, flags);
			ret = wait_event_interruptible(
				wq_dump_pcm,
				(dump_queue->idx_r != dump_queue->idx_w) ||
				split_dump_enable == false);
			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				break;
			}
			if (split_dump_enable == false)
				break;

			current_idx = dump_queue->idx_r;
			dump_queue->idx_r++;
		}

		dump_package = &dump_queue->dump_package[current_idx];
		switch (dump_package->dump_data_type) {
		case DUMP_DEBUG_DATA:
			size = dump_package->data_size;
			writedata = size;

			pcm_dump = (struct pcm_dump_t *)
					(scp_spk_dump_mem.start_virt +
					 dump_package->rw_idx);

			while (size > 0) {
				AUD_LOG_V("pcm_dump = %p writedata = %d\n",
					  pcm_dump, writedata);
				if (!IS_ERR(file_spk_debug)) {
					old_fs = get_fs();
					set_fs(KERNEL_DS);
					ret = vfs_write(file_spk_pcm,
							(char __user *)
							pcm_dump->decode_pcm,
							writedata,
							&file_spk_pcm->f_pos);
					set_fs(old_fs);
				}
				size -= writedata;
				pcm_dump++;
			}
			debug_dump_cnt++;
			break;
		case DUMP_IV_DATA:
			size = dump_package->data_size;
			writedata = size;
			pcm_dump = (struct pcm_dump_t *)
					(scp_spk_dump_mem.start_virt +
					 dump_package->rw_idx);

			while (size > 0) {
				AUD_LOG_V("pcm_dump = %p writedata = %d\n",
					  pcm_dump, writedata);
				if (!IS_ERR(file_spk_iv)) {
					old_fs = get_fs();
					set_fs(KERNEL_DS);
					ret = vfs_write(file_spk_iv,
							(char __user *)
							pcm_dump->decode_pcm,
							writedata,
							&file_spk_iv->f_pos);
					set_fs(old_fs);
				}
				size -= writedata;
				pcm_dump++;
			}
			iv_dump_cnt++;
			break;
		default:
			AUD_LOG_V(
				  "current_idx = %d, idx_r = %d, idx_w = %d, type = %d\n",
				  current_idx, dump_queue->idx_r,
				  dump_queue->idx_w,
				  dump_package->dump_data_type);
			break;
		}

		if (debug_file_open && debug_dump_cnt == blocks) {
			debug_file_open = false;
			debug_dump_cnt = 0;
			if (!IS_ERR(file_spk_debug)) {
				filp_close(file_spk_debug, NULL);
				file_spk_debug = NULL;
			}
		}

		if (iv_file_open && iv_dump_cnt == blocks) {
			iv_file_open = false;
			iv_dump_cnt = 0;
			if (!IS_ERR(file_spk_iv)) {
				filp_close(file_spk_iv, NULL);
				file_spk_iv = NULL;
			}
		}
	}

	if (debug_file_open) {
		debug_file_open = false;
		debug_dump_cnt = 0;
		if (!IS_ERR(file_spk_debug)) {
			filp_close(file_spk_debug, NULL);
			file_spk_debug = NULL;
		}
	}

	if (iv_file_open) {
		iv_file_open = false;
		iv_dump_cnt = 0;
		if (!IS_ERR(file_spk_iv)) {
			filp_close(file_spk_iv, NULL);
			file_spk_iv = NULL;
		}
	}

	AUD_LOG_D("spkprotect_dump_kthread exit\n");
	return 0;
}

static int spkprotect_dump_kthread(void *data)
{
	unsigned long flags = 0;
	int ret = 0;
	int size = 0, writedata = 0;
	uint8_t current_idx = 0;
	mm_segment_t old_fs;
	struct pcm_dump_t *pcm_dump = NULL;
	struct dump_package_t *dump_package = NULL;

	/* RTPM_PRIO_AUDIO_PLAYBACK */
	struct sched_param param = {.sched_priority = 85};

	sched_setscheduler(current, SCHED_RR, &param);

	/* pr_debug("dump_queue = %p\n", dump_queue); */

	while (b_enable_dump && !kthread_should_stop()) {
		spin_lock_irqsave(&dump_queue_lock, flags);
		if (dump_queue->idx_r != dump_queue->idx_w) {
			current_idx = dump_queue->idx_r;
			dump_queue->idx_r++;
			spin_unlock_irqrestore(&dump_queue_lock, flags);
		} else {
			spin_unlock_irqrestore(&dump_queue_lock, flags);
			ret = wait_event_interruptible(
				      wq_dump_pcm,
				      (dump_queue->idx_r != dump_queue->idx_w)
				      || b_enable_dump == false);
			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				break;
			}
			if (b_enable_dump == false)
				break;

			current_idx = dump_queue->idx_r;
			dump_queue->idx_r++;
		}

		dump_package = &dump_queue->dump_package[current_idx];

		switch (dump_package->dump_data_type) {
		case DUMP_PCM_PRE: {
			size = dump_package->data_size;
			writedata = size;
			pcm_dump = (struct pcm_dump_t *)
					(scp_spk_dump_mem.start_virt +
					 dump_package->rw_idx);

			while (size > 0) {
				/* pr_debug("pcm_dump = %p writedata = %d,
				 *	    current_idx = %d\n",
				 *	    pcm_dump, writedata, current_idx);
				 */
				if (!IS_ERR(file_spk_pcm)) {
					old_fs = get_fs();
					set_fs(KERNEL_DS);
					ret = vfs_write(file_spk_pcm,
							(char __user *)
							pcm_dump->decode_pcm,
							writedata,
							&file_spk_pcm->f_pos);
					set_fs(old_fs);
				}
				size -= writedata;
				pcm_dump++;
			}
			break;
		}
		case DUMP_IV_DATA: {
			size = dump_package->data_size;
			writedata = size;
			pcm_dump = (struct pcm_dump_t *)
					(scp_spk_dump_mem.start_virt +
					 dump_package->rw_idx);

			while (size > 0) {
				/* pr_debug("pcm_dump = %p writedata = %d\n",
				 *	    pcm_dump, writedata);
				 */
				if (!IS_ERR(file_spk_iv)) {
					old_fs = get_fs();
					set_fs(KERNEL_DS);
					ret = vfs_write(file_spk_iv,
							(char __user *)
							pcm_dump->decode_pcm,
							writedata,
							&file_spk_iv->f_pos);
					set_fs(old_fs);
				}
				size -= writedata;
				pcm_dump++;
			}
			break;
		}
		case DUMP_DEBUG_DATA: {
			size = dump_package->data_size;
			writedata = size;
			pcm_dump = (struct pcm_dump_t *)
					(scp_spk_dump_mem.start_virt +
					 dump_package->rw_idx);

			while (size > 0) {
				/* pr_debug("pcm_dump = %p writedata = %d\n",
				 *	    pcm_dump, writedata);
				 */
				if (!IS_ERR(file_spk_debug)) {
					old_fs = get_fs();
					set_fs(KERNEL_DS);
					ret = vfs_write(file_spk_debug,
							(char __user *)
							pcm_dump->decode_pcm,
							writedata,
							&file_spk_debug->f_pos);
					set_fs(old_fs);
				}
				size -= writedata;
				pcm_dump++;
			}
			break;
		}
		default: {
			pr_info("current_idx = %d, idx_r = %d, idx_w = %d, type = %d\n",
				current_idx,
				dump_queue->idx_r, dump_queue->idx_w,
				dump_package->dump_data_type);
			break;
		}
		}
	}
	/* pr_debug("%s exit\n", __func__); */
	return 0;
}

void audio_ipi_client_spkprotect_init(void)
{
	scp_spk_dump_mem.start_virt =
		(char *)scp_get_reserve_mem_virt(SPK_PROTECT_DUMP_MEM_ID);

	aud_wake_lock_init(&wakelock_scp_dump_lock, "scpspkdump lock");

	dump_workqueue[DUMP_PCM_PRE] = create_workqueue("dump_spkprotect_pcm");
	if (dump_workqueue[DUMP_PCM_PRE] == NULL)
		pr_notice("dump_workqueue[dump_spkprotect_pcm] = %p\n",
			  dump_workqueue[DUMP_PCM_PRE]);
	AUD_ASSERT(dump_workqueue[DUMP_PCM_PRE] != NULL);

	dump_workqueue[DUMP_IV_DATA] =
		create_workqueue("dump_spkprotect_ivpcm");
	if (dump_workqueue[DUMP_IV_DATA] == NULL)
		pr_notice("dump_workqueue[dump_spkprotect_ivpcm] = %p\n",
			  dump_workqueue[DUMP_IV_DATA]);
	AUD_ASSERT(dump_workqueue[DUMP_IV_DATA] != NULL);

	dump_workqueue[DUMP_DEBUG_DATA] =
		create_workqueue("dump_spkprotect_debug_pcm");
	if (dump_workqueue[DUMP_DEBUG_DATA] == NULL)
		pr_notice("dump_workqueue[dump_spkprotect_debug_pcm] = %p\n",
			  dump_workqueue[DUMP_DEBUG_DATA]);
	AUD_ASSERT(dump_workqueue[DUMP_DEBUG_DATA] != NULL);

	INIT_WORK(&dump_work[DUMP_PCM_PRE].work, spk_dump_data_routine);
	INIT_WORK(&dump_work[DUMP_IV_DATA].work, spk_dump_ivdata_routine);
	INIT_WORK(&dump_work[DUMP_DEBUG_DATA].work,
		  spk_dump_debug_data_routine);

	init_waitqueue_head(&wq_dump_pcm);

	speaker_dump_task = NULL;
	dump_queue = NULL;
	speaker_dump_task = NULL;
	spk_dump_split_task = NULL;
}

void audio_ipi_client_spkprotect_deinit(void)
{
	int i = 0;

	for (i = 0; i < NUM_DUMP_DATA; i++) {
		if (dump_workqueue[i]) {
			flush_workqueue(dump_workqueue[i]);
			destroy_workqueue(dump_workqueue[i]);
			dump_workqueue[i] = NULL;
		}
	}
	aud_wake_lock_destroy(&wakelock_scp_dump_lock);
}
