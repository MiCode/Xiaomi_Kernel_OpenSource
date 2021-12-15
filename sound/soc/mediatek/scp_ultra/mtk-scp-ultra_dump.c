/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

//#include "mtk-scp-ultra.h"

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
#include <linux/sched/types.h>
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

#include "audio_ultra_msg_id.h"
#include <scp_helper.h>
#include "mtk-scp-ultra_dump.h"
#include "mtk-scp-ultra-common.h"


#define DUMP_ULTRA_PCM_DATA_PATH "/data/vendor/audiohal/audio_dump"
#define FRAME_BUF_SIZE (8192)
static struct wakeup_source wakelock_ultra_dump_lock;

enum { /* dump_data_t */
	DUMP_PCM_IN = 0,
	DUMP_PCM_OUT = 1,
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

struct scp_ultra_reserved_mem_t {
	char *start_phy;
	char *start_virt;
	uint32_t size;
};

static struct task_struct *ultra_dump_split_task;
static struct dump_queue_t *dump_queue;
static DEFINE_SPINLOCK(dump_queue_lock);
static struct dump_work_t dump_work[NUM_DUMP_DATA];
static struct workqueue_struct *dump_workqueue[NUM_DUMP_DATA];
static struct task_struct *ultra_dump_task;
static int ultra_dump_kthread(void *data);
static wait_queue_head_t wq_dump_pcm;
static uint32_t dump_data_routine_cnt_pass;
static bool b_enable_dump;
static bool b_enable_stread;
//static bool split_dump_enable;
static struct file *fp_pcm_out;
static struct file *fp_pcm_in;
static struct scp_ultra_reserved_mem_t ultra_dump_mem;

struct pcm_dump_t {
	char decode_pcm[FRAME_BUF_SIZE];
};

int ultra_start_engine_thread(void)
{
	int ret = 0;

	/* only enable when debug pcm dump on */
	aud_wake_lock(&wakelock_ultra_dump_lock);

	pr_debug("%s(),b_enable_stread  0546= %d", __func__, b_enable_stread);
	if (true == b_enable_stread)
		return 0;
	if (dump_queue == NULL) {
		dump_queue = kmalloc(sizeof(struct dump_queue_t), GFP_KERNEL);
		if (dump_queue != NULL)
			memset_io(dump_queue, 0, sizeof(struct dump_queue_t));
	}

	if (!ultra_dump_task) {
		ultra_dump_task = kthread_create(ultra_dump_kthread,
				NULL,
				"ultra_dump_kthread");
		if (IS_ERR(ultra_dump_task)) {
			pr_notice("can not create ultra_dump_task kthread\n");
			ret = -1;
		}

		b_enable_stread = true;
		ultra_open_dump_file();
		wake_up_process(ultra_dump_task);
	}
	return ret;
}

void ultra_stop_engine_thread(void)
{
	pr_debug("%s,b_enable_stread = %d", __func__, b_enable_stread);
	if (b_enable_stread == false)
		return;

	b_enable_stread = false;

	if (ultra_dump_task) {
		kthread_stop(ultra_dump_task);
		ultra_dump_task = NULL;
	}
	pr_debug("dump_queue = %p\n", dump_queue);
	kfree(dump_queue);
	dump_queue = NULL;
	ultra_close_dump_file();
	aud_wake_unlock(&wakelock_ultra_dump_lock);
}

int ultra_open_dump_file(void)
{
	struct timespec curr_tm;
	char string_time[16];
	char string_datain_pcm[16] = "ultra_in.pcm";
	char string_dataout_pcm[16] = "ultra_out.pcm";
	char path_datain_pcm[64];
	char path_dataout_pcm[64];

	/* only enable when debug pcm dump on */
	//aud_wake_lock(&wakelock_ultra_dump_lock);
	getnstimeofday(&curr_tm);
	if (true == b_enable_dump) {
		pr_info("ultra dump is alread opend\n");
		return 0;
	}
	memset(string_time, '\0', 16);
	sprintf(string_time, "%.2lu_%.2lu_%.2lu_%.3lu",
			(8 + (curr_tm.tv_sec / 3600)) % (24),
			(curr_tm.tv_sec / 60) % (60),
			curr_tm.tv_sec % 60,
			(curr_tm.tv_nsec / 1000000) % 1000);

	sprintf(path_datain_pcm, "%s/%s_%s",
			DUMP_ULTRA_PCM_DATA_PATH,
			string_time,
			string_datain_pcm);
	pr_debug("%s(), path_in_pcm= %s\n", __func__, path_datain_pcm);
	sprintf(path_dataout_pcm, "%s/%s_%s",
			DUMP_ULTRA_PCM_DATA_PATH,
			string_time,
			string_dataout_pcm);
	pr_debug("%s(), path_decode_out_pcm= %s\n",
			__func__, path_dataout_pcm);

	fp_pcm_in = filp_open(path_datain_pcm,
			O_CREAT | O_WRONLY | O_LARGEFILE, 0);
	if (IS_ERR(fp_pcm_in)) {
		pr_info("%s(), %s file open error: %ld\n",
				__func__,
				path_datain_pcm,
				PTR_ERR(fp_pcm_in));
		return PTR_ERR(fp_pcm_in);
	}
	fp_pcm_out = filp_open(
			path_dataout_pcm,
			O_CREAT | O_WRONLY | O_LARGEFILE,
			0);
	if (IS_ERR(fp_pcm_out)) {
		pr_info("%s(), %s file open error: %ld\n",
				__func__,
				path_dataout_pcm,
				PTR_ERR(fp_pcm_out));
		return PTR_ERR(fp_pcm_out);
	}
	b_enable_dump = true;
	dump_data_routine_cnt_pass = 0;
	return 0;
}


void ultra_close_dump_file(void)
{
	if (b_enable_dump == false)
		return;

	b_enable_dump = false;

	pr_debug("%s(), pass: %d\n", __func__, dump_data_routine_cnt_pass);
	if (!IS_ERR(fp_pcm_in)) {
		filp_close(fp_pcm_in, NULL);
		fp_pcm_in = NULL;
	}
	if (!IS_ERR(fp_pcm_out)) {
		filp_close(fp_pcm_out, NULL);
		fp_pcm_out = NULL;
	}
}

static void ultra_dump_in_data_routine(struct work_struct *ws)
{
	struct dump_work_t *dump_work = NULL;
	uint32_t rw_idx = 0;
	uint32_t data_size = 0;
	unsigned long flags = 0;

	dump_work = container_of(ws, struct dump_work_t, work);
	rw_idx = dump_work->rw_idx;
	data_size = dump_work->data_size;
	if (dump_queue == NULL) {
		pr_info("%s dump_queue == null return\n", __func__);
		return;
	}
	spin_lock_irqsave(&dump_queue_lock, flags);
	dump_queue->dump_package[dump_queue->idx_w].dump_data_type =
			DUMP_PCM_IN;
	dump_queue->dump_package[dump_queue->idx_w].rw_idx = rw_idx;
	dump_queue->dump_package[dump_queue->idx_w].data_size = data_size;
	dump_queue->idx_w++;
	spin_unlock_irqrestore(&dump_queue_lock, flags);
	wake_up_interruptible(&wq_dump_pcm);
}


static void ultra_dump_out_data_routine(struct work_struct *ws)
{
	if (1) {
		struct dump_work_t *dump_work = NULL;
		uint32_t rw_idx = 0;
		uint32_t data_size = 0;
		unsigned long flags = 0;

		dump_work = container_of(ws, struct dump_work_t, work);
		rw_idx = dump_work->rw_idx;
		data_size = dump_work->data_size;
		if (dump_queue == NULL) {
			pr_info("%s dump_queue == null return\n", __func__);
			return;
		}
		spin_lock_irqsave(&dump_queue_lock, flags);
		dump_queue->dump_package[dump_queue->idx_w].dump_data_type =
				DUMP_PCM_OUT;
		dump_queue->dump_package[dump_queue->idx_w].rw_idx = rw_idx;
		dump_queue->dump_package[dump_queue->idx_w].data_size =
			data_size;
		dump_queue->idx_w++;
		spin_unlock_irqrestore(&dump_queue_lock, flags);
		wake_up_interruptible(&wq_dump_pcm);
	}
}

void ultra_dump_message(void *msg_data)
{
	int ret = 0;
	uint32_t dump_idx = 0; /* dump_data_t */
	uint32_t *temp_payload = (uint32_t *)(msg_data);

	/*  payload[0]: dump id(dump_data_t)
	 *  payload[1]: write data size
	 *  payload[2]: dump buffer write pointer offset
	 */
	dump_idx = *temp_payload;

	if (temp_payload == NULL) {
		pr_info("%s err\n", __func__);
		return;
	}
	pr_debug("dump msg in this dys\n");

	if (1) {
		dump_work[dump_idx].data_size  = *(temp_payload + 1);
		dump_work[dump_idx].rw_idx = *(temp_payload + 2);
		ret = queue_work(dump_workqueue[dump_idx],
				 &dump_work[dump_idx].work);
		if (ret == 0)
			dump_data_routine_cnt_pass++;
	}
}

static int ultra_dump_kthread(void *data)
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

	while (b_enable_stread && !kthread_should_stop()) {
		spin_lock_irqsave(&dump_queue_lock, flags);
		if (dump_queue == NULL)
			pr_info("dump queue is null");
		if (dump_queue->idx_r != dump_queue->idx_w) {
			current_idx = dump_queue->idx_r;
			dump_queue->idx_r++;
			spin_unlock_irqrestore(&dump_queue_lock, flags);
		} else {
			spin_unlock_irqrestore(&dump_queue_lock, flags);
			ret = wait_event_interruptible(wq_dump_pcm,
				(dump_queue->idx_r != dump_queue->idx_w)
				|| b_enable_stread == false);
			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				break;
			}
			if (b_enable_stread == false) {
				pr_info("b_enable_stread is false\n");
				break;
			}
			current_idx = dump_queue->idx_r;
			dump_queue->idx_r++;
		}

		dump_package = &dump_queue->dump_package[current_idx];

		if (dump_package == NULL)
			pr_info("dump_package == null");
		switch (dump_package->dump_data_type) {
			case DUMP_PCM_IN: {
				pr_info("DUMP_PCM_IN");
				size = dump_package->data_size;
				writedata = size;
				pcm_dump = (struct pcm_dump_t *)
						(ultra_dump_mem.start_virt +
						dump_package->rw_idx);
				if (fp_pcm_in == NULL ||
						(char __user *)
						pcm_dump->decode_pcm == NULL) {
					pr_info("DUMP_PCM_IN null, break\n");
					break;
				}
				while (size > 0) {
					if (!IS_ERR(fp_pcm_in)) {
						old_fs = get_fs();
						set_fs(KERNEL_DS);
						ret = vfs_write(fp_pcm_in,
							(char __user *)
							pcm_dump->decode_pcm,
							writedata,
							&fp_pcm_in->f_pos);
						set_fs(old_fs);
					} else {
						pr_info("fp_pcm_in err\n");
						break;
					}
					size -= writedata;
					pcm_dump++;
				}
				break;
			}
			case DUMP_PCM_OUT: {
				pr_info("DUMP_PCM_OUT");
				size = dump_package->data_size;
				writedata = size;
				pcm_dump = (struct pcm_dump_t *)
						(ultra_dump_mem.start_virt +
						dump_package->rw_idx);
				if (fp_pcm_out == NULL ||
					(char __user *)
					pcm_dump->decode_pcm == NULL) {
					pr_info("DUMP_PCM_OUT null, break\n");
					break;
				}
				while (size > 0) {
					if (!IS_ERR(fp_pcm_out)) {
						old_fs = get_fs();
						set_fs(KERNEL_DS);
						ret = vfs_write(fp_pcm_out,
							(char __user *)
							pcm_dump->decode_pcm,
							writedata,
							&fp_pcm_out->f_pos);
						set_fs(old_fs);
					} else {
						pr_info("fp_pcm_out err\n");
						break;
					}
					size -= writedata;
					pcm_dump++;
				}
				break;
			}
			default: {
				pr_info("cidx=%d,idx_r=%d,idx_w=%d,type=%d\n",
					current_idx,
					dump_queue->idx_r,
					dump_queue->idx_w,
					dump_package->dump_data_type);
				break;
			}
		}
	}
	pr_debug("%s exit\n", __func__);
	return 0;
}

void audio_ipi_client_ultra_init(void)
{
	ultra_dump_mem.start_virt =
		(char *)scp_get_reserve_mem_virt(ULTRA_MEM_ID);

	pr_info("%s()", __func__);
	if (ultra_dump_mem.start_virt == NULL) {
		pr_info("%s() ultra_dump_mem.start_virt:%p", __func__,
			ultra_dump_mem.start_virt);
	}
	aud_wake_lock_init(&wakelock_ultra_dump_lock, "ultradump lock");

	dump_workqueue[DUMP_PCM_IN] = create_workqueue("dump_ultra_pcm_in");
	if (dump_workqueue[DUMP_PCM_IN] == NULL) {
		pr_notice("dump_workqueue[DUMP_PCM_IN] = %p\n",
			  dump_workqueue[DUMP_PCM_IN]);
		AUDIO_AEE("dump_workqueue[DUMP_PCM_IN] == NULL");
	}

	dump_workqueue[DUMP_PCM_OUT] =
			create_workqueue("dump_ultra_pcm_out");
	if (dump_workqueue[DUMP_PCM_OUT] == NULL) {
		pr_notice("dump_workqueue[DUMP_PCM_OUT] = %p\n",
			  dump_workqueue[DUMP_PCM_OUT]);
		AUDIO_AEE("dump_workqueue[DUMP_PCM_OUT] == NULL");
	}

	INIT_WORK(&dump_work[DUMP_PCM_IN].work, ultra_dump_in_data_routine);
	INIT_WORK(&dump_work[DUMP_PCM_OUT].work, ultra_dump_out_data_routine);

	init_waitqueue_head(&wq_dump_pcm);

	ultra_dump_task = NULL;
	dump_queue = NULL;
	ultra_dump_split_task = NULL;
	b_enable_stread = false;
}

void audio_ipi_client_ultra_deinit(void)
{
	int i = 0;

	for (i = 0; i < NUM_DUMP_DATA; i++) {
		if (dump_workqueue[i]) {
			flush_workqueue(dump_workqueue[i]);
			destroy_workqueue(dump_workqueue[i]);
			dump_workqueue[i] = NULL;
		}
	}
	aud_wake_lock_destroy(&wakelock_ultra_dump_lock);
}
