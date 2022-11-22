// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include "audio_task_manager.h"

#include <linux/string.h>

#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#include <linux/io.h>
#include <linux/mutex.h>

#include "audio_log.h"
#include "audio_assert.h"

#include <audio_ipi_dma.h>

#include "audio_ipi_queue.h"
#include "audio_messenger_ipi.h"


struct audio_task_t {
	struct ipi_queue_handler_t *ipi_queue_handler;
};

static struct audio_task_t g_audio_task_array[TASK_SCENE_SIZE];



int audio_task_register_callback(
	const uint8_t task_scene,
	recv_message_t  recv_message)
{
	struct audio_task_t *task = &g_audio_task_array[task_scene];

	/* create hanlder when registering */
	task->ipi_queue_handler = create_ipi_queue_handler(task_scene);

	audio_reg_recv_message(task_scene, recv_message);

	return 0;
}
EXPORT_SYMBOL_GPL(audio_task_register_callback);


void audio_task_manager_init(void)
{
	memset(g_audio_task_array, 0, sizeof(g_audio_task_array));
}

void audio_task_manager_deinit(void)
{
	struct audio_task_t *task = NULL;
	uint8_t i = 0;

	for (i = 0; i < TASK_SCENE_SIZE; i++) {
		task = &g_audio_task_array[i];
		if (task->ipi_queue_handler != NULL) {
			disable_ipi_queue_handler(task->ipi_queue_handler);
			flush_ipi_queue_handler(task->ipi_queue_handler);
			destroy_ipi_queue_handler(task->ipi_queue_handler);
			task->ipi_queue_handler = NULL;
		}
	}
}

