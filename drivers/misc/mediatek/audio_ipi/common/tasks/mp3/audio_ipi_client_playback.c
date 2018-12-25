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

#include "audio_ipi_client_playback.h"

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
#include <linux/wakelock.h>


#include <linux/io.h>

#include <linux/wait.h>
#include <linux/time.h>

#include "audio_log.h"
#include "audio_assert.h"

#include "audio_task_manager.h"
#include "audio_dma_buf_control.h"


#define DUMP_DSP_PCM_DATA_PATH "/sdcard/mtklog"
static struct wake_lock playback_pcm_dump_wake_lock;

enum { /* dump_data_t */
	DUMP_DECODE = 0,
	NUM_DUMP_DATA = 1,
};



struct dump_work_t {
	struct work_struct work;
	char *dma_addr;
};


struct dump_package_t {
	uint8_t dump_data_type; /* dump_data_t */
	char *data_addr;
};


struct dump_queue_t {
	struct dump_package_t dump_package[256];
	uint8_t idx_r;
	uint8_t idx_w;
};


static struct dump_queue_t *dump_queue;
static DEFINE_SPINLOCK(dump_queue_lock);


static struct dump_work_t dump_work[NUM_DUMP_DATA];
static struct workqueue_struct *dump_workqueue[NUM_DUMP_DATA];

struct task_struct *playback_dump_task;

static int dump_kthread(void *data);

static wait_queue_head_t wq_dump_pcm;

static uint32_t dump_data_routine_cnt_pass;

static bool b_enable_dump;
static int datasize;

#define FRAME_BUF_SIZE   (4608)
#define MP3_PCMDUMP_OK (24)

struct pcm_dump_t {
	char decode_pcm[FRAME_BUF_SIZE];
};

static struct audio_resv_dram_t *p_resv_dram;

struct file *file_decode_pcm;

void playback_open_dump_file(void)
{
	struct timespec curr_tm;

	char string_time[16];

	char string_decode_pcm[16] = "mp3_decode.pcm";

	char path_decode_pcm[64];


	/* only enable when debug pcm dump on */
	wake_lock(&playback_pcm_dump_wake_lock);

	getnstimeofday(&curr_tm);

	memset(string_time, '\0', 16);
	sprintf(string_time, "%.2lu_%.2lu_%.2lu",
		(8 + (curr_tm.tv_sec / 3600)) % (24),
		(curr_tm.tv_sec / 60) % (60),
		curr_tm.tv_sec % 60);

	sprintf(path_decode_pcm, "%s/%s_%s",
		DUMP_DSP_PCM_DATA_PATH, string_time, string_decode_pcm);

	file_decode_pcm = filp_open(path_decode_pcm, O_CREAT | O_WRONLY, 0);
	if (IS_ERR(file_decode_pcm)) {
		pr_info("file_decode_pcm < 0,path_decode_pcm = %s\n",
			path_decode_pcm);
		return;
	}

	if (dump_queue == NULL) {
		dump_queue = kmalloc(sizeof(struct dump_queue_t), GFP_KERNEL);
		if (dump_queue != NULL)
			memset_io(dump_queue, 0, sizeof(struct dump_queue_t));
	}

	if (!playback_dump_task) {
		playback_dump_task = kthread_create(dump_kthread, NULL,
						    "dump_kthread");
		if (IS_ERR(playback_dump_task))
			pr_notice("can not create playback_dump_task kthread\n");

		b_enable_dump = true;
		wake_up_process(playback_dump_task);
	}


	dump_data_routine_cnt_pass = 0;
}


void playback_close_dump_file(void)
{
	if (b_enable_dump == false)
		return;

	b_enable_dump = false;

	pr_debug("pass: %d\n", dump_data_routine_cnt_pass);

	if (playback_dump_task) {
		kthread_stop(playback_dump_task);
		playback_dump_task = NULL;
	}
	pr_debug("dump_queue = %p\n", dump_queue);
	kfree(dump_queue);
	dump_queue = NULL;

	if (!IS_ERR(file_decode_pcm)) {
		filp_close(file_decode_pcm, NULL);
		file_decode_pcm = NULL;
	}
	wake_unlock(&playback_pcm_dump_wake_lock);
}

static void dump_data_routine(struct work_struct *ws)
{
	struct dump_work_t *dump_work = NULL;
	char *data_addr = NULL;
	unsigned long flags = 0;

	dump_work = container_of(ws, struct dump_work_t, work);
	data_addr = get_resv_dram_vir_addr(dump_work->dma_addr);
	pr_notice("data %p, dma %p, vp %p, pp %p\n",
		  data_addr, dump_work->dma_addr,
		  p_resv_dram->vir_addr, p_resv_dram->phy_addr);

	spin_lock_irqsave(&dump_queue_lock, flags);
	dump_queue->dump_package[dump_queue->idx_w].dump_data_type =
		DUMP_DECODE;
	dump_queue->dump_package[dump_queue->idx_w].data_addr = data_addr;
	dump_queue->idx_w++;
	spin_unlock_irqrestore(&dump_queue_lock, flags);

	wake_up_interruptible(&wq_dump_pcm);
}


void playback_dump_message(struct ipi_msg_t *ipi_msg)
{
	int ret = 0;

	uint8_t idx = 0; /* dump_data_t */

	datasize = 0;

	if (ipi_msg->msg_id == MP3_PCMDUMP_OK) {
		datasize = ipi_msg->param1;
		dump_work[idx].dma_addr = ipi_msg->dma_addr;
		ret = queue_work(dump_workqueue[idx], &dump_work[idx].work);
		if (ret == 0)
			dump_data_routine_cnt_pass++;
	}
}

static int dump_kthread(void *data)
{
	unsigned long flags = 0;
	int ret = 0;
	int size = 0, writedata = 0;
	uint8_t current_idx = 0;

	struct pcm_dump_t *pcm_dump = NULL;
	struct dump_package_t *dump_package = NULL;

	/* RTPM_PRIO_AUDIO_PLAYBACK */
	struct sched_param param = {.sched_priority = 85 };

	sched_setscheduler(current, SCHED_RR, &param);


	pr_debug("dump_queue = %p\n", dump_queue);

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

		pr_debug("current_idx = %d\n", current_idx);

		dump_package = &dump_queue->dump_package[current_idx];

		switch (dump_package->dump_data_type) {
		case DUMP_DECODE: {
			size = datasize;
			writedata = FRAME_BUF_SIZE;
			pcm_dump =
				(struct pcm_dump_t *)dump_package->data_addr;
			pr_debug("pcm_dump = %p datasize = %d\n",
				 pcm_dump, datasize);

			while (size > 0) {
				if (size < FRAME_BUF_SIZE)
					writedata = size;
				pr_debug("pcm_dump = %p writedata = %d\n",
					 pcm_dump, writedata);
				if (!IS_ERR(file_decode_pcm)) {
					ret = file_decode_pcm->f_op->write(
						      file_decode_pcm,
						      pcm_dump->decode_pcm,
						      writedata,
						      &file_decode_pcm->f_pos);
					size -= writedata;
					pcm_dump++;
				}

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

	pr_debug("dump_kthread exit\n");
	return 0;
}

void audio_ipi_client_playback_init(void)
{
	wake_lock_init(&playback_pcm_dump_wake_lock, WAKE_LOCK_SUSPEND,
		       "playback_pcm_dump_wake_lock");
	dump_workqueue[DUMP_DECODE] = create_workqueue("dump_decode_pcm");
	if (dump_workqueue[DUMP_DECODE] == NULL)
		pr_notice("dump_workqueue[DUMP_DECODE] = %p\n",
			  dump_workqueue[DUMP_DECODE]);
	AUD_ASSERT(dump_workqueue[DUMP_DECODE] != NULL);

	INIT_WORK(&dump_work[DUMP_DECODE].work, dump_data_routine);

	init_waitqueue_head(&wq_dump_pcm);

	playback_dump_task = NULL;

	/* TODO: ring buf */
	p_resv_dram = get_reserved_dram();
	AUD_ASSERT(p_resv_dram != NULL);
}


void audio_ipi_client_playback_deinit(void)
{
	int i = 0;

	for (i = 0; i < NUM_DUMP_DATA; i++) {
		if (dump_workqueue[i]) {
			flush_workqueue(dump_workqueue[i]);
			destroy_workqueue(dump_workqueue[i]);
			dump_workqueue[i] = NULL;
		}
	}

}

