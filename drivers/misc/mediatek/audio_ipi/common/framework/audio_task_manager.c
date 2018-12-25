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

#include "audio_task_manager.h"

#include <linux/string.h>

#include <linux/slab.h>         /* needed by kmalloc */

#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#include <linux/io.h>
#include <linux/mutex.h>

#include <scp_ipi.h>
#ifdef CONFIG_MTK_DO /* TODO: check only once in one file */
#include <do.h>
#endif

#include "audio_log.h"
#include "audio_assert.h"

#include "audio_ipi_queue.h"
#include "audio_messenger_ipi.h"

/* using for filter ipi message*/
#include <audio_spkprotect_msg_id.h>

#ifdef CONFIG_MTK_DO /* with DO */
static DEFINE_MUTEX(audio_load_task_mutex);

#define DO_FEATURE_NAME_CALL "FEATURE_MTK_AURISYS_PHONE_CALL"
#define DO_FEATURE_NAME_MP3  "FEATURE_MTK_AUDIO_PLAYBACK_MP3"
#define DO_FEATURE_NAME_VOW  "FEATURE_MTK_VOW"

#define DO_SET_NAME_CALL     "AUDIO_CALL"
#define DO_SET_NAME_MP3_VOW  "AUDIO_MP3_VOW"
#endif


struct audio_task_t {
#ifdef CONFIG_MTK_DO /* with DO */
	char *feature_name;
	char *do_name;

	bool is_do_loaded;
	task_unloaded_t task_unloaded;
#endif
	struct ipi_queue_handler_t *ipi_queue_handler;
};

static struct audio_task_t g_audio_task_array[TASK_SCENE_SIZE];

static char *g_current_do_name;



#ifdef CONFIG_MTK_DO /* with DO */
static char *get_feature_name(const task_scene_t task_scene)
{
	/*
	 * feature name defined in:
	 *     repo: alps/vendor/mediatek/proprietary/tinysys/freertos/source
	 *     file: project/CM4_A/mt6797/platform/platform.mk
	 */
	char *feature_name = NULL;


	switch (task_scene) {
	case TASK_SCENE_PHONE_CALL:
		feature_name = DO_FEATURE_NAME_CALL;
		break;
	case TASK_SCENE_PLAYBACK_MP3:
		feature_name = DO_FEATURE_NAME_MP3;
		break;
	case TASK_SCENE_VOW:
		feature_name = DO_FEATURE_NAME_VOW;
		break;
	case TASK_SCENE_VOICE_ULTRASOUND:
	case TASK_SCENE_RECORD:
	case TASK_SCENE_VOIP:
	case TASK_SCENE_SPEAKER_PROTECTION:
	default: {
		pr_notice("%s not support task %d", __func__, task_scene);
		break;
	}
	}

	return feature_name;
}

static char *get_do_name(const task_scene_t task_scene)
{
#if 1
	/*
	 * DO name defined in:
	 *     repo: alps/vendor/mediatek/proprietary/tinysys/freertos/source
	 *     file: project/CM4_A/mt6797/platform/dos.mk
	 */
	char *do_name = NULL;


	switch (task_scene) {
	case TASK_SCENE_PHONE_CALL:
		do_name = DO_SET_NAME_CALL;
		break;
	case TASK_SCENE_PLAYBACK_MP3:
	case TASK_SCENE_VOW:
		do_name = DO_SET_NAME_MP3_VOW;
		break;
	case TASK_SCENE_VOICE_ULTRASOUND:
	case TASK_SCENE_RECORD:
	case TASK_SCENE_VOIP:
	case TASK_SCENE_SPEAKER_PROTECTION:
	default: {
		pr_notice("%s not support task %d", __func__, task_scene);
		break;
	}
	}

	return do_name;
#else
	char *feature_name = g_audio_task_array[task_scene].feature_name;
	struct do_list_node *all_do_info = mt_do_get_do_infos();

	struct do_list_node *do_node = NULL;
	struct do_list_node *feature_node = NULL;

	char *do_name = NULL;


	do_node = all_do_info;
	while (do_node != NULL) {
		feature_node = do_node->features;
		while (feature_node != NULL) {
			if (!strcmp(feature_node->name, feature_name)) {
				do_name = do_node->name;
				break;
			}
			feature_node = feature_node->next;
		}
		if (do_name != NULL)
			break;
		do_node = do_node->next;
	}


	if (do_name != NULL)
		pr_debug("get feature %s in DO set %s\n",
			 feature_name, do_name);

	return do_name;
#endif
}
#endif /* end of CONFIG_MTK_DO */






int audio_task_register_callback(
	const task_scene_t task_scene,
	recv_message_t  recv_message,
	task_unloaded_t task_unloaded)
{
	struct audio_task_t *task = &g_audio_task_array[task_scene];

#ifdef CONFIG_MTK_DO /* with DO */
	task->feature_name = get_feature_name(task_scene);
	AUD_ASSERT(task->feature_name != NULL);

	task->do_name = get_do_name(task_scene);
	AUD_ASSERT(task->do_name != NULL);

	task->is_do_loaded = false;

	task->task_unloaded = task_unloaded;

	/* with DO, create hanlder until do loaded */
	task->ipi_queue_handler = NULL;
#else
	/* without DO, create hanlder when registering */
	task->ipi_queue_handler = create_ipi_queue_handler(task_scene);
#endif

	audio_reg_recv_message(task_scene, recv_message);

	return 0;
}



#ifdef CONFIG_MTK_DO /* with DO */
static void load_target_tasks(char *target_do_name)
{
	struct audio_task_t *task = NULL;
	int i = 0;


	for (i = 0; i < TASK_SCENE_SIZE; i++) {
		task = &g_audio_task_array[i];

		if (task->do_name == NULL)
			continue;

		if (!strcmp(task->do_name, target_do_name)) {
			task->is_do_loaded = true;
			task->ipi_queue_handler = create_ipi_queue_handler(i);
		}
	}
}


static void unload_current_tasks(void)
{

	struct audio_task_t *task = NULL;
	int i = 0;

	for (i = 0; i < TASK_SCENE_SIZE; i++) {
		task = &g_audio_task_array[i];
		if (task->is_do_loaded == true) {
			task->is_do_loaded = false;

			/* notify user the task is going to be unloaded */
			if (task->task_unloaded != NULL)
				task->task_unloaded();

			disable_ipi_queue_handler(task->ipi_queue_handler);
			flush_ipi_queue_handler(task->ipi_queue_handler);
			destroy_ipi_queue_handler(task->ipi_queue_handler);
			task->ipi_queue_handler = NULL;
		}
	}
}
#endif


int audio_load_task(const task_scene_t task_scene)
{
#ifndef CONFIG_MTK_DO /* without DO, do nothing */
	return -1;
#else
	struct do_list_node *current_do = NULL;

	char *target_do_name = NULL;
	int retval = 0;

	pr_debug("%s(+), task_scene: %d", __func__, task_scene);

	mutex_lock(&audio_load_task_mutex);

	target_do_name = get_do_name(task_scene);
	AUD_ASSERT(target_do_name != NULL);

	/* already loaded, do nothing */
	if (g_current_do_name != NULL &&
	    !strcmp(target_do_name, g_current_do_name))
		goto audio_load_task_exit;

	/* unload current */
	if (g_current_do_name == NULL) { /* scp might load a default DO */
		current_do = mt_do_get_loaded_do(SCP_B);
		if (current_do != NULL &&
		    current_do->name != NULL &&
		    strcmp(target_do_name, current_do->name) != 0)
			mt_do_unload_do(current_do->name);
	} else { /* another DO is loaded, unload it */
		unload_current_tasks();
		mt_do_unload_do(g_current_do_name);
	}

	/* load target */
	mt_do_load_do(target_do_name);
	load_target_tasks(target_do_name);

	g_current_do_name = target_do_name;


audio_load_task_exit:

	mutex_unlock(&audio_load_task_mutex);

	pr_debug("%s(-), task_scene: %d\n", __func__, task_scene);
	return retval;
#endif /* end of CONFIG_MTK_DO */
}


static bool check_print_msg_info(const struct ipi_msg_t *p_ipi_msg)
{
	if (p_ipi_msg->task_scene == TASK_SCENE_SPEAKER_PROTECTION &&
	    p_ipi_msg->msg_id == SPK_PROTECT_DLCOPY)
		return false;
	else
		return true;
}


int audio_send_ipi_msg(
	struct ipi_msg_t *p_ipi_msg,
	uint8_t task_scene, /* task_scene_t */
	uint8_t msg_layer, /* audio_ipi_msg_layer_t */
	uint8_t data_type, /* audio_ipi_msg_data_t */
	uint8_t ack_type, /* audio_ipi_msg_ack_t */
	uint16_t msg_id,
	uint32_t param1,
	uint32_t param2,
	char    *data_buffer)
{
	struct ipi_queue_handler_t *handler = NULL;
	uint32_t ipi_msg_len = 0;

	if (p_ipi_msg == NULL) {
		pr_notice("%s(), p_ipi_msg = NULL, return\n", __func__);
		return -1;
	}

	memset_io(p_ipi_msg, 0, MAX_IPI_MSG_BUF_SIZE);

	p_ipi_msg->magic      = IPI_MSG_MAGIC_NUMBER;
	p_ipi_msg->task_scene = task_scene;
	p_ipi_msg->msg_layer  = msg_layer;
	p_ipi_msg->data_type  = data_type;
	p_ipi_msg->ack_type   = ack_type;
	p_ipi_msg->msg_id     = msg_id;
	p_ipi_msg->param1     = param1;
	p_ipi_msg->param2     = param2;

	if (p_ipi_msg->data_type == AUDIO_IPI_PAYLOAD) {
		AUD_ASSERT(data_buffer != NULL);
		AUD_ASSERT(p_ipi_msg->param1 <= MAX_IPI_MSG_PAYLOAD_SIZE);
		memcpy(p_ipi_msg->payload, data_buffer, p_ipi_msg->param1);
	} else if (p_ipi_msg->data_type == AUDIO_IPI_DMA) {
		AUD_ASSERT(data_buffer != NULL);
		p_ipi_msg->dma_addr = data_buffer;
	}

	ipi_msg_len = get_message_buf_size(p_ipi_msg);
	check_msg_format(p_ipi_msg, ipi_msg_len);

	/* if need atomic , direct call send_message_to_scp*/
	if (msg_layer == AUDIO_IPI_LAYER_KERNEL_TO_SCP_ATOMIC) {
		if (check_print_msg_info(p_ipi_msg) == true)
			print_msg_info(__func__, "p_ipi_msg", p_ipi_msg);
		return send_message_to_scp(p_ipi_msg);
	}

	handler = get_ipi_queue_handler(p_ipi_msg->task_scene);
	if (handler == NULL) {
		pr_notice("%s(), handler = NULL, return\n", __func__);
		return -1;
	}

	return send_message(handler, p_ipi_msg);
}


int audio_send_ipi_filled_msg(struct ipi_msg_t *p_ipi_msg)
{
	struct ipi_queue_handler_t *handler = NULL;

	handler = get_ipi_queue_handler(p_ipi_msg->task_scene);
	if (handler == NULL) {
		pr_notice("%s(), handler = NULL, return\n", __func__);
		return -1;
	}

	return send_message(handler, p_ipi_msg);
}


void audio_task_manager_init(void)
{
	g_current_do_name = NULL;

	memset(g_audio_task_array, 0, sizeof(g_audio_task_array));

	audio_messenger_ipi_init();

}


void audio_task_manager_deinit(void)
{
	struct audio_task_t *task = NULL;
	int i = 0;

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


