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

#include "audio_ipi_client_phone_call.h"

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
#include "audio_speech_msg_id.h"


#define DUMP_DSP_PCM_DATA_PATH "/sdcard/mtklog/aurisys_dump"
static struct wake_lock call_pcm_dump_wake_lock;


enum { /* dump_data_t */
	DUMP_UL = 0,
	DUMP_DL = 1,
	NUM_DUMP_DATA = 2
};



struct dump_work_t {
	struct work_struct work;
	char *dma_addr;
};


struct dump_package_t {
	uint8_t dump_data_type;
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

struct task_struct *dump_task;

static int dump_kthread(void *data);

static wait_queue_head_t wq_dump_pcm;

static uint32_t irq_cnt[NUM_DUMP_DATA];
static uint32_t irq_cnt_w[NUM_DUMP_DATA];
static uint32_t irq_cnt_k[NUM_DUMP_DATA];
static uint32_t dump_data_routine_cnt_pass;

static bool b_enable_dump;


#define MAX_FRAME_BUF_SIZE   (1280)

struct pcm_dump_ul_t {
	char ul_in_ch1[MAX_FRAME_BUF_SIZE];
	char ul_in_ch2[MAX_FRAME_BUF_SIZE];
	char ul_in_ch3[MAX_FRAME_BUF_SIZE];
	char aec_in[MAX_FRAME_BUF_SIZE];
	char ul_out[MAX_FRAME_BUF_SIZE];
	uint32_t frame_buf_size;
};

struct pcm_dump_dl_t {
	char dl_in[MAX_FRAME_BUF_SIZE];
	char dl_out[MAX_FRAME_BUF_SIZE];
	uint32_t frame_buf_size;
};


static struct audio_resv_dram_t *p_resv_dram;

struct file *file_ul_in_ch1;
struct file *file_ul_in_ch2;
struct file *file_ul_in_ch3;
struct file *file_ul_out;
struct file *file_aec_in;
struct file *file_dl_in;
struct file *file_dl_out;

void open_dump_file(void)
{
	struct timespec curr_tm;

	char string_time[16];

	char string_ul_in_ch1[16] = "ul_in_ch1.pcm";
	char string_ul_in_ch2[16] = "ul_in_ch2.pcm";
	char string_ul_in_ch3[16] = "ul_in_ch3.pcm";
	char string_aec_in[16]    = "aec_in.pcm";
	char string_ul_out[16]    = "ul_out.pcm";
	char string_dl_in[16]     = "dl_in.pcm";
	char string_dl_out[16]    = "dl_out.pcm";

	char path_ul_in_ch1[64];
	char path_ul_in_ch2[64];
	char path_ul_in_ch3[64];
	char path_aec_in[64];
	char path_ul_out[64];
	char path_dl_in[64];
	char path_dl_out[64];


	/* only enable when debug pcm dump on */
	wake_lock(&call_pcm_dump_wake_lock);

	getnstimeofday(&curr_tm);

	memset(string_time, '\0', 16);
	snprintf(string_time, sizeof(string_time), "%.2lu_%.2lu_%.2lu",
		 (8 + (curr_tm.tv_sec / 3600)) % (24),
		 (curr_tm.tv_sec / 60) % (60),
		 curr_tm.tv_sec % 60);

	snprintf(path_ul_in_ch1, sizeof(path_ul_in_ch1), "%s/%s_%s",
		 DUMP_DSP_PCM_DATA_PATH, string_time, string_ul_in_ch1);
	snprintf(path_ul_in_ch2, sizeof(path_ul_in_ch2), "%s/%s_%s",
		 DUMP_DSP_PCM_DATA_PATH, string_time, string_ul_in_ch2);
	snprintf(path_ul_in_ch3, sizeof(path_ul_in_ch3), "%s/%s_%s",
		 DUMP_DSP_PCM_DATA_PATH, string_time, string_ul_in_ch3);
	snprintf(path_aec_in, sizeof(path_aec_in), "%s/%s_%s",
		 DUMP_DSP_PCM_DATA_PATH, string_time, string_aec_in);
	snprintf(path_ul_out, sizeof(path_ul_out), "%s/%s_%s",
		 DUMP_DSP_PCM_DATA_PATH, string_time, string_ul_out);
	snprintf(path_dl_in, sizeof(path_dl_in), "%s/%s_%s",
		 DUMP_DSP_PCM_DATA_PATH, string_time, string_dl_in);
	snprintf(path_dl_out, sizeof(path_dl_out), "%s/%s_%s",
		 DUMP_DSP_PCM_DATA_PATH, string_time, string_dl_out);

	file_ul_in_ch1 = filp_open(path_ul_in_ch1, O_CREAT | O_WRONLY, 0);
	if (IS_ERR(file_ul_in_ch1)) {
		pr_info("file_ul_in_ch1 < 0\n");
		return;
	}

	file_ul_in_ch2 = filp_open(path_ul_in_ch2, O_CREAT | O_WRONLY, 0);
	if (IS_ERR(file_ul_in_ch2)) {
		pr_info("file_ul_in_ch2 < 0\n");
		return;
	}

	file_ul_in_ch3 = filp_open(path_ul_in_ch3, O_CREAT | O_WRONLY, 0);
	if (IS_ERR(file_ul_in_ch3)) {
		pr_info("file_ul_in_ch3 < 0\n");
		return;
	}

	file_aec_in = filp_open(path_aec_in, O_CREAT | O_WRONLY, 0);
	if (IS_ERR(file_aec_in)) {
		pr_info("file_aec_in < 0\n");
		return;
	}

	file_ul_out = filp_open(path_ul_out, O_CREAT | O_WRONLY, 0);
	if (IS_ERR(file_ul_out)) {
		pr_info("file_ul_out < 0\n");
		return;
	}

	file_dl_in = filp_open(path_dl_in, O_CREAT | O_WRONLY, 0);
	if (IS_ERR(file_dl_in)) {
		pr_info("file_dl_in < 0\n");
		return;
	}

	file_dl_out = filp_open(path_dl_out, O_CREAT | O_WRONLY, 0);
	if (IS_ERR(file_dl_out)) {
		pr_info("file_dl_out < 0\n");
		return;
	}

	if (dump_queue == NULL) {
		dump_queue = kmalloc(sizeof(struct dump_queue_t), GFP_KERNEL);
		if (dump_queue != NULL)
			memset_io(dump_queue, 0, sizeof(struct dump_queue_t));
	}

	if (!dump_task) {
		dump_task = kthread_create(dump_kthread, NULL, "dump_kthread");
		if (IS_ERR(dump_task))
			pr_notice("can not create dump_task kthread\n");

		b_enable_dump = true;
		wake_up_process(dump_task);
	}

	irq_cnt[DUMP_UL] = 0;
	irq_cnt[DUMP_DL] = 0;
	irq_cnt_w[DUMP_UL] = 0;
	irq_cnt_w[DUMP_DL] = 0;
	irq_cnt_k[DUMP_UL] = 0;
	irq_cnt_k[DUMP_DL] = 0;

	dump_data_routine_cnt_pass = 0;
}


void close_dump_file(void)
{
	if (b_enable_dump == false)
		return;

	b_enable_dump = false;

	pr_debug("UL: %d %d %d. DL: %d %d %d. pass: %d\n",
		 irq_cnt[DUMP_UL], irq_cnt_w[DUMP_UL], irq_cnt_k[DUMP_UL],
		 irq_cnt[DUMP_DL], irq_cnt_w[DUMP_DL], irq_cnt_k[DUMP_DL],
		 dump_data_routine_cnt_pass);

	if (dump_task) {
		kthread_stop(dump_task);
		dump_task = NULL;
	}
	pr_debug("dump_queue = %p\n", dump_queue);
	kfree(dump_queue);
	dump_queue = NULL;

	if (!IS_ERR(file_ul_in_ch1)) {
		filp_close(file_ul_in_ch1, NULL);
		file_ul_in_ch1 = NULL;
	}
	if (!IS_ERR(file_ul_in_ch2)) {
		filp_close(file_ul_in_ch2, NULL);
		file_ul_in_ch2 = NULL;
	}
	if (!IS_ERR(file_ul_in_ch3)) {
		filp_close(file_ul_in_ch3, NULL);
		file_ul_in_ch3 = NULL;
	}
	if (!IS_ERR(file_ul_out)) {
		filp_close(file_ul_out, NULL);
		file_ul_out = NULL;
	}
	if (!IS_ERR(file_aec_in)) {
		filp_close(file_aec_in, NULL);
		file_aec_in = NULL;
	}
	if (!IS_ERR(file_dl_in)) {
		filp_close(file_dl_in, NULL);
		file_dl_in = NULL;
	}
	if (!IS_ERR(file_dl_out)) {
		filp_close(file_dl_out, NULL);
		file_dl_out = NULL;
	}

	wake_unlock(&call_pcm_dump_wake_lock);
}


void phone_call_recv_message(struct ipi_msg_t *p_ipi_msg)
{
	int ret = 0;
	uint8_t idx;

	AUD_ASSERT(p_ipi_msg->task_scene == TASK_SCENE_PHONE_CALL);

	if (p_ipi_msg->msg_id == IPI_MSG_D2A_PCM_DUMP_DATA_NOTIFY) {
		idx = (uint8_t)p_ipi_msg->param2;

		irq_cnt[idx]++;
		dump_work[idx].dma_addr = p_ipi_msg->dma_addr;

		ret = queue_work(dump_workqueue[idx], &dump_work[idx].work);
		if (ret == 0)
			dump_data_routine_cnt_pass++;

	}
}


void phone_call_task_unloaded(void)
{
	pr_debug("%s()\n", __func__);
}


static void dump_data_routine_ul(struct work_struct *ws)
{
	struct dump_work_t *dump_work = NULL;
	char *data_addr = NULL;

	unsigned long flags = 0;

	irq_cnt_w[DUMP_UL]++;

	dump_work = container_of(ws, struct dump_work_t, work);
	data_addr = get_resv_dram_vir_addr(dump_work->dma_addr);
	AUD_LOG_V("data %p, dma %p, vp %p, pp %p\n",
		  data_addr, dump_work->dma_addr,
		  p_resv_dram->vir_addr, p_resv_dram->phy_addr);

	spin_lock_irqsave(&dump_queue_lock, flags);
	dump_queue->dump_package[dump_queue->idx_w].dump_data_type = DUMP_UL;
	dump_queue->dump_package[dump_queue->idx_w].data_addr = data_addr;
	dump_queue->idx_w++;
	spin_unlock_irqrestore(&dump_queue_lock, flags);

	wake_up_interruptible(&wq_dump_pcm);
}


static void dump_data_routine_dl(struct work_struct *ws)
{
	struct dump_work_t *dump_work = NULL;
	char *data_addr = NULL;

	unsigned long flags = 0;

	irq_cnt_w[DUMP_DL]++;

	dump_work = container_of(ws, struct dump_work_t, work);
	data_addr = get_resv_dram_vir_addr(dump_work->dma_addr);
	AUD_LOG_V("data %p, dma %p, vp %p, pp %p\n",
		  data_addr, dump_work->dma_addr,
		  p_resv_dram->vir_addr, p_resv_dram->phy_addr);

	spin_lock_irqsave(&dump_queue_lock, flags);
	dump_queue->dump_package[dump_queue->idx_w].dump_data_type = DUMP_DL;
	dump_queue->dump_package[dump_queue->idx_w].data_addr = data_addr;
	dump_queue->idx_w++;
	spin_unlock_irqrestore(&dump_queue_lock, flags);

	wake_up_interruptible(&wq_dump_pcm);
}


static int dump_kthread(void *data)
{
	unsigned long flags = 0;
	int ret = 0;

	uint8_t current_idx = 0;

	struct pcm_dump_ul_t *pcm_dump_ul = NULL;
	struct pcm_dump_dl_t *pcm_dump_dl = NULL;

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

		AUD_LOG_V("current_idx = %d\n", current_idx);

		dump_package = &dump_queue->dump_package[current_idx];

		switch (dump_package->dump_data_type) {
		case DUMP_UL: {
			pcm_dump_ul =
				(struct pcm_dump_ul_t *)dump_package->data_addr;
			AUD_LOG_V("pcm_dump_ul = %p\n", pcm_dump_ul);
			if (!IS_ERR(file_ul_in_ch1)) {
				ret = file_ul_in_ch1->f_op->write(
					      file_ul_in_ch1,
					      pcm_dump_ul->ul_in_ch1,
					      pcm_dump_ul->frame_buf_size,
					      &file_ul_in_ch1->f_pos);
			}
			if (!IS_ERR(file_ul_in_ch2)) {
				ret = file_ul_in_ch2->f_op->write(
					      file_ul_in_ch2,
					      pcm_dump_ul->ul_in_ch2,
					      pcm_dump_ul->frame_buf_size,
					      &file_ul_in_ch2->f_pos);
			}
			if (!IS_ERR(file_ul_in_ch3)) {
				ret = file_ul_in_ch3->f_op->write(
					      file_ul_in_ch3,
					      pcm_dump_ul->ul_in_ch3,
					      pcm_dump_ul->frame_buf_size,
					      &file_ul_in_ch3->f_pos);
			}
			if (!IS_ERR(file_aec_in)) {
				ret = file_aec_in->f_op->write(
					      file_aec_in,
					      pcm_dump_ul->aec_in,
					      pcm_dump_ul->frame_buf_size,
					      &file_aec_in->f_pos);
			}
			if (!IS_ERR(file_ul_out)) {
				ret = file_ul_out->f_op->write(
					      file_ul_out,
					      pcm_dump_ul->ul_out,
					      pcm_dump_ul->frame_buf_size,
					      &file_ul_out->f_pos);
			}
			irq_cnt_k[DUMP_UL]++;
			break;
		}
		case DUMP_DL: {
			pcm_dump_dl =
				(struct pcm_dump_dl_t *)dump_package->data_addr;
			AUD_LOG_V("pcm_dump_sl = %p\n", pcm_dump_dl);
			if (!IS_ERR(file_dl_in)) {
				ret = file_dl_in->f_op->write(
					      file_dl_in,
					      pcm_dump_dl->dl_in,
					      pcm_dump_dl->frame_buf_size,
					      &file_dl_in->f_pos);
			}
			if (!IS_ERR(file_dl_out)) {
				ret = file_dl_out->f_op->write(
					      file_dl_out,
					      pcm_dump_dl->dl_out,
					      pcm_dump_dl->frame_buf_size,
					      &file_dl_out->f_pos);
			}
			irq_cnt_k[DUMP_DL]++;
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



ssize_t audio_ipi_client_phone_call_read(
	struct file *file, char *buf, size_t count, loff_t *ppos)
{
	return 0;
}


void audio_ipi_client_phone_call_init(void)
{
	wake_lock_init(&call_pcm_dump_wake_lock, WAKE_LOCK_SUSPEND,
		       "call_pcm_dump_wake_lock");

	audio_task_register_callback(
		TASK_SCENE_PHONE_CALL,
		phone_call_recv_message,
		phone_call_task_unloaded);

	dump_workqueue[DUMP_UL] = create_workqueue("dump_pcm_ul");
	if (dump_workqueue[DUMP_UL] == NULL)
		pr_notice("dump_workqueue[DUMP_UL] = %p\n",
			  dump_workqueue[DUMP_UL]);
	AUD_ASSERT(dump_workqueue[DUMP_UL] != NULL);

	dump_workqueue[DUMP_DL] = create_workqueue("dump_pcm_dl");
	if (dump_workqueue[DUMP_DL] == NULL)
		pr_notice("dump_workqueue[DUMP_DL] = %p\n",
			  dump_workqueue[DUMP_DL]);
	AUD_ASSERT(dump_workqueue[DUMP_DL] != NULL);

	INIT_WORK(&dump_work[DUMP_UL].work, dump_data_routine_ul);
	INIT_WORK(&dump_work[DUMP_DL].work, dump_data_routine_dl);

	init_waitqueue_head(&wq_dump_pcm);

	dump_task = NULL;

	/* TODO: ring buf */
	p_resv_dram = get_reserved_dram();
	AUD_ASSERT(p_resv_dram != NULL);
}


void audio_ipi_client_phone_call_deinit(void)
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

