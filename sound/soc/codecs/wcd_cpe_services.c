/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <sound/cpe_cmi.h>
#include <sound/soc.h>
#include <linux/mfd/wcd9xxx/wcd9330_registers.h>
#include <linux/mfd/wcd9335/registers.h>
#include "wcd_cpe_services.h"
#include "wcd_cmi_api.h"

#define CPE_MSG_BUFFER_SIZE 132
#define CPE_NO_SERVICE 0

#define CMI_DRIVER_SUPPORTED_VERSION 0
#define CMI_API_SUCCESS 0
#define CMI_MSG_TRANSPORT (0x0002)
#define CPE_SVC_INACTIVE_STATE_RETRIES_MAX 10

#define TOMTOM_A_SVASS_SPE_DRAM_OFFSET				0x50000
#define TOMTOM_A_SVASS_SPE_DRAM_SIZE				0x30000
#define TOMTOM_A_SVASS_SPE_IRAM_OFFSET				0x80000
#define TOMTOM_A_SVASS_SPE_IRAM_SIZE				0xC000
#define TOMTOM_A_SVASS_SPE_INBOX_SIZE				12
#define TOMTOM_A_SVASS_SPE_OUTBOX_SIZE				12

#define MEM_ACCESS_NONE_VAL			0x0
#define MEM_ACCESS_IRAM_VAL			0x1
#define MEM_ACCESS_DRAM_VAL			0x2
#define LISTEN_CTL_SPE_VAL			0x0
#define LISTEN_CTL_MSM_VAL			0x1

#define TOMTOM_A_SVASS_SPE_INBOX(N)	(TOMTOM_A_SVASS_SPE_INBOX_0 + (N))
#define TOMTOM_A_SVASS_SPE_OUTBOX(N)	(TOMTOM_A_SVASS_SPE_OUTBOX_0 + (N))

#define WCD9335_CPE_SS_SPE_DRAM_OFFSET		0x48000
#define WCD9335_CPE_SS_SPE_DRAM_SIZE		0x34000
#define WCD9335_CPE_SS_SPE_IRAM_OFFSET		0x80000
#define WCD9335_CPE_SS_SPE_IRAM_SIZE		0x20000

#define WCD9335_CPE_SS_SPE_INBOX_SIZE		16
#define WCD9335_CPE_SS_SPE_OUTBOX_SIZE		16
#define WCD9335_CPE_SS_SPE_MEM_BANK_SIZ		16

#define WCD9335_CPE_SS_SPE_INBOX1(N)	(WCD9335_CPE_SS_INBOX1_0 + (N))
#define WCD9335_CPE_SS_SPE_OUTBOX1(N)	(WCD9335_CPE_SS_OUTBOX1_0 + (N))
#define WCD9335_CPE_SS_MEM_BANK(N)	(WCD9335_CPE_SS_MEM_BANK_0 + (N))

#define CHUNK_SIZE 16

#define CPE_SVC_GRAB_LOCK(lock, name)		\
{						\
	pr_debug("%s: %s lock acquire\n",	\
		 __func__, name);		\
	mutex_lock(lock);			\
}

#define CPE_SVC_REL_LOCK(lock, name)		\
{						\
	pr_debug("%s: %s lock release\n",	\
		 __func__, name);		\
	mutex_unlock(lock);			\
}

static const struct cpe_svc_hw_cfg cpe_svc_tomtom_info = {
	TOMTOM_A_SVASS_SPE_DRAM_SIZE,
	TOMTOM_A_SVASS_SPE_DRAM_OFFSET,
	TOMTOM_A_SVASS_SPE_IRAM_SIZE,
	TOMTOM_A_SVASS_SPE_IRAM_OFFSET,
	TOMTOM_A_SVASS_SPE_INBOX_SIZE,
	TOMTOM_A_SVASS_SPE_OUTBOX_SIZE
};

static const struct cpe_svc_hw_cfg cpe_svc_wcd9335_info = {
	WCD9335_CPE_SS_SPE_DRAM_SIZE,
	WCD9335_CPE_SS_SPE_DRAM_OFFSET,
	WCD9335_CPE_SS_SPE_IRAM_SIZE,
	WCD9335_CPE_SS_SPE_IRAM_OFFSET,
	WCD9335_CPE_SS_SPE_INBOX_SIZE,
	WCD9335_CPE_SS_SPE_OUTBOX_SIZE
};

enum cpe_state {
	CPE_STATE_UNINITIALIZED = 0,
	CPE_STATE_INITIALIZED,
	CPE_STATE_IDLE,
	CPE_STATE_DOWNLOADING,
	CPE_STATE_BOOTING,
	CPE_STATE_SENDING_MSG,
	CPE_STATE_OFFLINE,
	CPE_STATE_BUFFERING,
	CPE_STATE_BUFFERING_CANCELLED
};

enum cpe_substate {
	CPE_SS_IDLE = 0,
	CPE_SS_MSG_REQUEST_ACCESS,
	CPE_SS_MSG_SEND_INBOX,
	CPE_SS_MSG_SENT,
	CPE_SS_DL_DOWNLOADING,
	CPE_SS_DL_COMPLETED,
	CPE_SS_BOOT,
	CPE_SS_BOOT_INIT,
	CPE_SS_ONLINE
};

enum cpe_command {
	CPE_CMD_KILL_THREAD = 0,
	CPE_CMD_BOOT,
	CPE_CMD_BOOT_INITIALIZE,
	CPE_CMD_BOOT_COMPLETE,
	CPE_CMD_SEND_MSG,
	CPE_CMD_SEND_TRANS_MSG,
	CPE_CMD_SEND_MSG_COMPLETE,
	CPE_CMD_PROCESS_IRQ,
	CPE_CMD_RAMDUMP,
	CPE_CMD_DL_SEGMENT,
	CPE_CMD_SHUTDOWN,
	CPE_CMD_RESET,
	CPE_CMD_DEINITIALIZE,
	CPE_CMD_READ,
	CPE_CMD_ENABLE_LAB,
	CPE_CMD_DISABLE_LAB,
	CPE_CMD_SWAP_BUFFER,
	CPE_LAB_CFG_SB,
	CPE_CMD_CANCEL_MEMACCESS,
	CPE_CMD_PROC_INCOMING_MSG,
	CPE_CMD_FTM_TEST,
};

enum cpe_process_result {
	CPE_PROC_SUCCESS = 0,
	CPE_PROC_FAILED,
	CPE_PROC_KILLED,
	CPE_PROC_QUEUED,
};

struct cpe_command_node {
	enum cpe_command command;
	enum cpe_svc_result result;
	void *data;
	struct list_head list;
};

struct cpe_info {
	struct list_head main_queue;
	struct completion cmd_complete;
	void *thread_handler;
	bool stop_thread;
	struct mutex msg_lock;
	enum cpe_state state;
	enum cpe_substate substate;
	struct list_head client_list;
	enum cpe_process_result (*cpe_process_command)
			(struct cpe_command_node *command_node);
	enum cpe_svc_result (*cpe_cmd_validate)
				(const struct cpe_info *i,
				 enum cpe_command command);
	enum cpe_svc_result (*cpe_start_notification)
			     (struct cpe_info *i);
	u32 initialized;
	struct cpe_svc_tgt_abstraction *tgt;
	void *pending;
	void *data;
	void *client_context;
	u32 codec_id;
	struct work_struct clk_plan_work;
	struct completion core_svc_cmd_compl;
};

struct cpe_tgt_waiti_info {
	u8 tgt_waiti_size;
	u8 *tgt_waiti_data;
};

struct cpe_svc_tgt_abstraction {
	enum cpe_svc_result (*tgt_boot) (int debug_mode);

	u32 (*tgt_cpar_init_done) (void);

	u32 (*tgt_is_active) (void);

	enum cpe_svc_result (*tgt_reset) (void);

	enum cpe_svc_result (*tgt_stop)(void);

	enum cpe_svc_result (*tgt_read_mailbox)
				(u8 *buffer, size_t size);

	enum cpe_svc_result (*tgt_write_mailbox)
				(u8 *buffer, size_t size);

	enum cpe_svc_result (*tgt_read_ram)
				(struct cpe_info *c,
				 struct cpe_svc_mem_segment *data);

	enum cpe_svc_result (*tgt_write_ram)
				(struct cpe_info *c,
				const struct cpe_svc_mem_segment *data);

	enum cpe_svc_result (*tgt_route_notification)
				(enum cpe_svc_module module,
				 enum cpe_svc_route_dest dest);

	enum cpe_svc_result (*tgt_set_debug_mode) (u32 enable);
	const struct cpe_svc_hw_cfg *(*tgt_get_cpe_info) (void);
	enum cpe_svc_result (*tgt_deinit)
				(struct cpe_svc_tgt_abstraction *param);
	enum cpe_svc_result (*tgt_voice_tx_lab)
				(bool);
	u8 *inbox;
	u8 *outbox;
	struct cpe_tgt_waiti_info *tgt_waiti_info;
};

static enum cpe_svc_result cpe_tgt_tomtom_init(
	struct cpe_svc_codec_info_v1 *codec_info,
	struct cpe_svc_tgt_abstraction *param);

static enum cpe_svc_result cpe_tgt_wcd9335_init(
	struct cpe_svc_codec_info_v1 *codec_info,
	struct cpe_svc_tgt_abstraction *param);

struct cpe_send_msg {
	u8 *payload;
	u32 isobm;
	u32 address;
	size_t size;
};

struct cpe_read_handle {
	void *registration;
	struct cpe_info t_info;
	struct list_head buffers;
	void *config;
};

struct generic_notification {
	void (*notification)
		(const struct cpe_svc_notification *parameter);
	void (*cmi_notification)
		(const struct cmi_api_notification *parameter);
};

struct cpe_notif_node {
	struct generic_notification notif;
	u32 mask;
	u32 service;
	const struct cpe_info *context;
	const char *name;
	u32 disabled;
	struct list_head list;
};

struct cpe_priv {
	struct cpe_info *cpe_default_handle;
	void (*cpe_irq_control_callback)(u32 enable);
	void (*cpe_query_freq_plans_cb)
		(void *cdc_priv,
		 struct cpe_svc_cfg_clk_plan *clk_freq);
	void (*cpe_change_freq_plan_cb)(void *cdc_priv,
			u32 clk_freq);
	u32 cpe_msg_buffer;
	void *cpe_cmi_handle;
	struct mutex cpe_api_mutex;
	struct mutex cpe_svc_lock;
	struct cpe_svc_boot_event cpe_debug_vector;
	void *cdc_priv;
};

static struct cpe_priv cpe_d;

static enum cpe_svc_result __cpe_svc_shutdown(void *cpe_handle);

static enum cpe_svc_result cpe_is_command_valid(
		const struct cpe_info *t_info,
		enum cpe_command command);

static int cpe_register_read(u32 reg, u8 *val)
{
	*(val) = snd_soc_read(cpe_d.cdc_priv, reg);
	return 0;
}

static enum cpe_svc_result cpe_update_bits(u32 reg,
		u32 mask, u32 value)
{
	int ret = 0;
	ret = snd_soc_update_bits(cpe_d.cdc_priv, reg,
				  mask, value);
	if (ret < 0)
		return CPE_SVC_FAILED;

	return CPE_SVC_SUCCESS;
}

static int cpe_register_write(u32 reg, u32 val)
{
	int ret = 0;

	if (reg != TOMTOM_A_SVASS_MEM_BANK &&
	    reg != WCD9335_CPE_SS_MEM_BANK_0)
		pr_debug("%s: reg = 0x%x, value = 0x%x\n",
			  __func__, reg, val);

	ret = snd_soc_write(cpe_d.cdc_priv, reg, val);
	if (ret < 0)
		return CPE_SVC_FAILED;

	return CPE_SVC_SUCCESS;
}

static int cpe_register_write_repeat(u32 reg, u8 *ptr, u32 to_write)
{
	struct snd_soc_codec *codec = cpe_d.cdc_priv;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	int ret = 0;

	ret = wcd9xxx_slim_write_repeat(wcd9xxx, reg, to_write, ptr);
	if (ret != 0)
		pr_err("%s: slim_write_repeat failed\n", __func__);

	if (ret < 0)
		return CPE_SVC_FAILED;

	return CPE_SVC_SUCCESS;
}

static bool cpe_register_read_autoinc_supported(void)
{
	return true;
}


/* Called under msgq locked context */
static void cpe_cmd_received(struct cpe_info *t_info)
{
	struct cpe_command_node *node = NULL;
	enum cpe_process_result proc_rc = CPE_PROC_SUCCESS;
	if (!t_info) {
		pr_err("%s: Invalid thread info\n",
			__func__);
		return;
	}

	while (!list_empty(&t_info->main_queue)) {
		if (proc_rc != CPE_PROC_SUCCESS)
			break;
		node = list_first_entry(&t_info->main_queue,
					struct cpe_command_node, list);
		if (!node)
			break;
		list_del(&node->list);
		proc_rc = t_info->cpe_process_command(node);
		pr_debug("%s: process command return %d\n",
			 __func__, proc_rc);

		switch (proc_rc) {
		case CPE_PROC_SUCCESS:
			kfree(node);
			break;
		case CPE_PROC_FAILED:
			kfree(node);
			pr_err("%s: cmd failed\n", __func__);
			break;
		case CPE_PROC_KILLED:
			break;
		default:
			list_add(&node->list, &(t_info->main_queue));

		}
	}
}

static int cpe_worker_thread(void *context)
{
	struct cpe_info *t_info = (struct cpe_info *)context;

	while (!kthread_should_stop()) {
		wait_for_completion(&t_info->cmd_complete);

		CPE_SVC_GRAB_LOCK(&t_info->msg_lock, "msg_lock");
		cpe_cmd_received(t_info);
		INIT_COMPLETION(t_info->cmd_complete);
		if (t_info->stop_thread)
			goto unlock_and_exit;
		CPE_SVC_REL_LOCK(&t_info->msg_lock, "msg_lock");
	};

	pr_debug("%s: thread exited\n", __func__);
	return 0;

unlock_and_exit:
	pr_debug("%s: thread stopped\n", __func__);
	CPE_SVC_REL_LOCK(&t_info->msg_lock, "msg_lock");

	return 0;
}

static void cpe_create_worker_thread(struct cpe_info *t_info)
{
	INIT_LIST_HEAD(&t_info->main_queue);
	init_completion(&t_info->cmd_complete);
	t_info->stop_thread = false;
	t_info->thread_handler = kthread_run(cpe_worker_thread,
		(void *)t_info, "cpe-worker-thread");
	pr_debug("%s: Created new worker thread\n",
		 __func__);
}

static void cpe_cleanup_worker_thread(struct cpe_info *t_info)
{
	if (!t_info->thread_handler) {
		pr_err("%s: thread not created\n", __func__);
		return;
	}

	/*
	 * Wake up the command handler in case
	 * it is waiting for an command to be processed.
	 */
	CPE_SVC_GRAB_LOCK(&t_info->msg_lock, "msg_lock");
	t_info->stop_thread = true;
	complete(&t_info->cmd_complete);
	CPE_SVC_REL_LOCK(&t_info->msg_lock, "msg_lock");

	kthread_stop(t_info->thread_handler);

	t_info->thread_handler = NULL;
}

static enum cpe_svc_result
cpe_send_cmd_to_thread(struct cpe_info *t_info,
	enum cpe_command command, void *data,
	bool high_prio)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_command_node *cmd = NULL;

	rc = cpe_is_command_valid(t_info, command);
	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: Invalid command %d\n",
			__func__, command);
		return rc;
	}

	cmd = kzalloc(sizeof(struct cpe_command_node),
		      GFP_ATOMIC);
	if (!cmd) {
		pr_err("%s: No memory for cmd node, size = %zu\n",
			__func__, sizeof(struct cpe_command_node));
		return CPE_SVC_NO_MEMORY;
	}

	cmd->command = command;
	cmd->data = data;

	CPE_SVC_GRAB_LOCK(&t_info->msg_lock, "msg_lock");
	if (high_prio)
		list_add(&(cmd->list),
			 &(t_info->main_queue));
	else
		list_add_tail(&(cmd->list),
			      &(t_info->main_queue));
	complete(&t_info->cmd_complete);
	CPE_SVC_REL_LOCK(&t_info->msg_lock, "msg_lock");

	return rc;
}

static enum cpe_svc_result cpe_change_state(
	struct cpe_info *t_info,
	enum cpe_state state, enum cpe_substate ss)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;

	if (!t_info)
		t_info = cpe_d.cpe_default_handle;

	t_info->state = state;
	t_info->substate = ss;

	pr_debug("%s: current state: %d,%d, new_state: %d,%d\n",
		 __func__, t_info->state, t_info->substate,
		 state, ss);

	return rc;
}

static enum cpe_svc_result
cpe_is_command_valid(const struct cpe_info *t_info,
		enum cpe_command command)
{
	enum cpe_svc_result rc = CPE_SVC_INVALID_HANDLE;
	if (t_info && t_info->cpe_cmd_validate)
		rc = t_info->cpe_cmd_validate(t_info, command);
	else
		pr_err("%s: invalid handle or callback\n",
			__func__);
	return rc;
}

static void cpe_notify_client(struct cpe_notif_node *client,
		struct cpe_svc_notification *payload)
{
	if (!client || !payload) {
		pr_err("%s: invalid client or payload\n",
			__func__);
		return;
	}

	if (!(client->mask & payload->event)) {
		pr_debug("%s: client mask 0x%x not registered for event 0x%x\n",
			 __func__, client->mask, payload->event);
		return;
	}

	if (client->notif.notification && !client->disabled)
		client->notif.notification(payload);

	if ((client->mask & CPE_SVC_CMI_MSG) &&
	     client->notif.cmi_notification)
		client->notif.cmi_notification(
			(const struct cmi_api_notification *)payload);
}

static void cpe_broadcast_notification(const struct cpe_info *t_info,
		struct cpe_svc_notification *payload)
{
	struct cpe_notif_node *n = NULL;

	if (!t_info || !payload) {
		pr_err("%s: invalid handle\n", __func__);
		return;
	}

	pr_debug("%s: notify clients, event = %d\n",
		 __func__, payload->event);
	payload->private_data = cpe_d.cdc_priv;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_svc_lock, "cpe_svc");
	list_for_each_entry(n, &t_info->client_list, list) {
		if (!(n->mask & CPE_SVC_CMI_MSG)) {
			cpe_notify_client(n, payload);
		}
	}
	CPE_SVC_REL_LOCK(&cpe_d.cpe_svc_lock, "cpe_svc");
}

static void *cpe_register_generic(struct cpe_info *t_info,
		void notification_callback(
			const struct cpe_svc_notification *parameter),
		void cmi_callback(
			const struct cmi_api_notification *parameter),
		u32 mask, u32 service, const char *name)
{
	struct cpe_notif_node *n = NULL;

	n = kzalloc(sizeof(struct cpe_notif_node),
		    GFP_KERNEL);
	if (!n) {
		pr_err("%s: No memory for notification, size = %zu\n",
			__func__, sizeof(struct cpe_notif_node));
		return NULL;
	}
	n->mask = mask;
	n->service = service;
	n->notif.notification = notification_callback;
	n->notif.cmi_notification = cmi_callback;
	n->context = t_info;
	n->disabled = false;
	n->name = name;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_svc_lock, "cpe_svc");
	/* Make sure CPE core service is first */
	if (service == CMI_CPE_CORE_SERVICE_ID)
		list_add(&n->list, &t_info->client_list);
	else
		list_add_tail(&n->list, &t_info->client_list);
	CPE_SVC_REL_LOCK(&cpe_d.cpe_svc_lock, "cpe_svc");

	return n;
}

static enum cpe_svc_result cpe_deregister_generic(struct cpe_info *t_info,
		void *reg_handle)
{
	struct cpe_notif_node *n = (struct cpe_notif_node *)reg_handle;

	if (!t_info || !reg_handle) {
		pr_err("%s: invalid handle\n", __func__);
		return CPE_SVC_INVALID_HANDLE;
	}

	list_del(&(n->list));
	kfree(reg_handle);

	return CPE_SVC_SUCCESS;
}

static enum cpe_svc_result cpe_svc_tgt_init(struct cpe_svc_codec_info_v1 *i,
		struct cpe_svc_tgt_abstraction *abs)
{
	if (!i || !abs) {
		pr_err("%s: Incorrect information provided\n",
			__func__);
		return CPE_SVC_FAILED;
	}

	switch (i->id) {
	case CPE_SVC_CODEC_TOMTOM:
		return cpe_tgt_tomtom_init(i, abs);
	case CPE_SVC_CODEC_WCD9335:
		return cpe_tgt_wcd9335_init(i, abs);
	default:
		pr_err("%s: Codec type %d not supported\n",
			__func__, i->id);
		return CPE_SVC_FAILED;
	}

	return CPE_SVC_SUCCESS;
}

static void cpe_notify_cmi_client(struct cpe_info *t_info, u8 *payload,
		enum cpe_svc_result result)
{
	struct cpe_notif_node *n = NULL;
	struct cmi_api_notification notif;
	struct cmi_hdr *hdr;
	u8 service = 0;

	if (!t_info || !payload) {
		pr_err("%s: invalid payload/handle\n",
			__func__);
		return;
	}

	hdr = CMI_GET_HEADER(payload);
	service = CMI_HDR_GET_SERVICE(hdr);

	notif.event = CPE_SVC_CMI_MSG;
	notif.result = result;
	notif.message = payload;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_svc_lock, "cpe_svc");
	list_for_each_entry(n, &t_info->client_list, list) {

		if ((n->mask & CPE_SVC_CMI_MSG) &&
		    n->service == service &&
		    n->notif.cmi_notification) {
			n->notif.cmi_notification(&notif);
			break;
		}
	}
	CPE_SVC_REL_LOCK(&cpe_d.cpe_svc_lock, "cpe_svc");
}

static void cpe_toggle_irq_notification(struct cpe_info *t_info, u32 value)
{
	if (cpe_d.cpe_irq_control_callback)
		cpe_d.cpe_irq_control_callback(value);
}

static void cpe_command_cleanup(struct cpe_command_node *command_node)
{
	switch (command_node->command) {
	case CPE_CMD_SEND_MSG:
	case CPE_CMD_SEND_TRANS_MSG:
	case CPE_CMD_SEND_MSG_COMPLETE:
	case CPE_CMD_SHUTDOWN:
	case CPE_CMD_READ:
		kfree(command_node->data);
		command_node->data = NULL;
		break;
	default:
		pr_err("%s: unhandled command\n",
			__func__);
		break;
	}
}

static enum cpe_svc_result cpe_send_msg_to_inbox(
		struct cpe_info *t_info, u32 opcode,
		struct cpe_send_msg *msg)
{
	size_t bytes = 0;
	size_t inbox_size =
		t_info->tgt->tgt_get_cpe_info()->inbox_size;
	struct cmi_hdr *hdr;
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;

	memset(t_info->tgt->inbox, 0, inbox_size);
	hdr = CMI_GET_HEADER(t_info->tgt->inbox);
	CMI_HDR_SET_SESSION(hdr, 1);
	CMI_HDR_SET_SERVICE(hdr, CMI_CPE_CORE_SERVICE_ID);
	CMI_HDR_SET_VERSION(hdr, CMI_DRIVER_SUPPORTED_VERSION);
	CMI_HDR_SET_OBM(hdr, CMI_OBM_FLAG_IN_BAND);

	switch (opcode) {
	case CPE_CORE_SVC_CMD_SHARED_MEM_ALLOC: {
		struct cmi_core_svc_cmd_shared_mem_alloc *m;
		CMI_HDR_SET_OPCODE(hdr,
			CPE_CORE_SVC_CMD_SHARED_MEM_ALLOC);
		CMI_HDR_SET_PAYLOAD_SIZE(hdr,
			sizeof(struct cmi_core_svc_cmd_shared_mem_alloc));
		m = (struct cmi_core_svc_cmd_shared_mem_alloc *)
			CMI_GET_PAYLOAD(t_info->tgt->inbox);
		m->size = CPE_MSG_BUFFER_SIZE;
		pr_debug("send shared mem alloc msg to cpe inbox\n");
		}
		break;
	case CPE_CORE_SVC_CMD_DRAM_ACCESS_REQ:
		CMI_HDR_SET_OPCODE(hdr,
			CPE_CORE_SVC_CMD_DRAM_ACCESS_REQ);
		CMI_HDR_SET_PAYLOAD_SIZE(hdr, 0);
		pr_debug("%s: Creating DRAM acces request msg\n",
			 __func__);
		break;

	case CPE_CMI_BASIC_RSP_OPCODE: {
		struct cmi_basic_rsp_result *rsp;
		CMI_HDR_SET_OPCODE(hdr,
			       CPE_CMI_BASIC_RSP_OPCODE);
		CMI_HDR_SET_PAYLOAD_SIZE(hdr,
			sizeof(struct cmi_basic_rsp_result));
		rsp = (struct cmi_basic_rsp_result *)
				CMI_GET_PAYLOAD(t_info->tgt->inbox);
		rsp->status = 0;
		pr_debug("%s: send basic response\n", __func__);
		}
		break;

	default:
		if (msg->address != 0) {
			struct cmi_msg_transport *m = NULL;
			struct cpe_svc_mem_segment mem_seg;

			mem_seg.type = CPE_SVC_DATA_MEM;
			if (msg->isobm) {
				struct cmi_obm *obm = (struct cmi_obm *)
				CMI_GET_PAYLOAD(msg->payload);
				mem_seg.cpe_addr = obm->mem_handle;
				mem_seg.data = (u8 *)obm->data_ptr.kvaddr;
				mem_seg.size = obm->size;
				t_info->tgt->tgt_write_ram(t_info, &mem_seg);
			}

			mem_seg.cpe_addr = msg->address;
			mem_seg.data = msg->payload;
			mem_seg.size = msg->size;
			t_info->tgt->tgt_write_ram(t_info, &mem_seg);

			hdr = CMI_GET_HEADER(t_info->tgt->inbox);
			CMI_HDR_SET_OPCODE(hdr, CMI_MSG_TRANSPORT);
			m = (struct cmi_msg_transport *)
				CMI_GET_PAYLOAD(t_info->tgt->inbox);
			m->addr = msg->address;
			m->size = msg->size;
			CMI_HDR_SET_PAYLOAD_SIZE(hdr,
				sizeof(struct cmi_msg_transport));
		} else {
			memcpy(t_info->tgt->inbox, msg->payload,
			       msg->size);
		}

		break;
	}

	pr_debug("%s: sending message to cpe inbox\n",
		  __func__);
	bytes = sizeof(struct cmi_hdr);
	hdr = CMI_GET_HEADER(t_info->tgt->inbox);
	bytes += CMI_HDR_GET_PAYLOAD_SIZE(hdr);
	rc = t_info->tgt->tgt_write_mailbox(t_info->tgt->inbox, bytes);

	return rc;
}

static bool cpe_is_cmd_clk_req(void *cmd)
{
	struct cmi_hdr *hdr;

	hdr = CMI_GET_HEADER(cmd);

	if ((CMI_HDR_GET_SERVICE(hdr) ==
	    CMI_CPE_CORE_SERVICE_ID)) {
		if (CMI_GET_OPCODE(cmd) ==
		    CPE_CORE_SVC_CMD_CLK_FREQ_REQUEST)
			return true;
	}

	return false;
}

static enum cpe_svc_result cpe_process_clk_change_req(
		struct cpe_info *t_info)
{
	struct cmi_core_svc_cmd_clk_freq_request *req;
	req = (struct cmi_core_svc_cmd_clk_freq_request *)
			CMI_GET_PAYLOAD(t_info->tgt->outbox);

	if (!cpe_d.cpe_change_freq_plan_cb) {
		pr_err("%s: No support for clk freq change\n",
			__func__);
		return CPE_SVC_FAILED;
	}

	cpe_d.cpe_change_freq_plan_cb(cpe_d.cdc_priv,
				      req->clk_freq);

	/*send a basic response*/
	cpe_send_msg_to_inbox(t_info,
		CPE_CMI_BASIC_RSP_OPCODE, NULL);

	return CPE_SVC_SUCCESS;
}

static void cpe_process_irq_int(u32 irq,
		struct cpe_info *t_info)
{
	struct cpe_command_node temp_node;
	struct cpe_send_msg *m;
	u8 size = 0;
	bool err_irq = false;

	pr_debug("%s: irq = %u\n", __func__, irq);

	if (!t_info) {
		pr_err("%s: Invalid handle\n",
			__func__);
		return;
	}

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	switch (irq) {
	case CPE_IRQ_OUTBOX_IRQ:
		size = t_info->tgt->tgt_get_cpe_info()->outbox_size;
		t_info->tgt->tgt_read_mailbox(t_info->tgt->outbox, size);
		break;

	case CPE_IRQ_MEM_ACCESS_ERROR:
		err_irq = true;
		cpe_change_state(t_info, CPE_STATE_OFFLINE, CPE_SS_IDLE);
		break;

	case CPE_IRQ_WDOG_BITE:
	case CPE_IRQ_RCO_WDOG_INT:
		err_irq = true;
		__cpe_svc_shutdown(t_info);
		break;

	case CPE_IRQ_FLL_LOCK_LOST:
	default:
		err_irq = true;
		break;
	}

	if (err_irq) {
		pr_err("%s: CPE error IRQ %u occured\n",
			__func__, irq);
		CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
		return;
	}

	switch (t_info->state) {
	case CPE_STATE_BOOTING:

		switch (t_info->substate) {
		case CPE_SS_BOOT:
			temp_node.command = CPE_CMD_BOOT_INITIALIZE;
			temp_node.result = CPE_SVC_SUCCESS;
			t_info->substate = CPE_SS_BOOT_INIT;
			t_info->cpe_process_command(&temp_node);
			break;

		case CPE_SS_BOOT_INIT:
			temp_node.command = CPE_CMD_BOOT_COMPLETE;
			temp_node.result = CPE_SVC_SUCCESS;
			t_info->substate = CPE_SS_ONLINE;
			t_info->cpe_process_command(&temp_node);
			break;

		default:
			pr_debug("%s: unhandled substate %d for state %d\n",
				 __func__, t_info->state, t_info->substate);
			break;
		}
		break;

	case CPE_STATE_SENDING_MSG:
		m = (struct cpe_send_msg *)t_info->pending;

		switch (t_info->substate) {
		case CPE_SS_MSG_REQUEST_ACCESS:
			cpe_send_cmd_to_thread(t_info,
				CPE_CMD_SEND_TRANS_MSG, m, true);
			break;

		case CPE_SS_MSG_SEND_INBOX:
			if (cpe_is_cmd_clk_req(t_info->tgt->outbox))
				cpe_process_clk_change_req(t_info);
			else
				cpe_send_cmd_to_thread(t_info,
					CPE_CMD_SEND_MSG_COMPLETE, m, true);
			break;

		default:
			pr_debug("%s: unhandled substate %d for state %d\n",
				 __func__, t_info->state, t_info->substate);
			break;
		}
		break;

	case CPE_STATE_IDLE:
		pr_debug("%s: Message received, notifying client\n",
			 __func__);
		temp_node.command = CPE_CMD_PROC_INCOMING_MSG;
		temp_node.data = NULL;
		t_info->cpe_process_command(&temp_node);
		break;

	default:
		pr_debug("%s: unhandled state %d\n",
			 __func__, t_info->state);
		break;
	}

	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
}


static void broacast_boot_failed(void)
{
	struct cpe_info *t_info = cpe_d.cpe_default_handle;
	struct cpe_svc_notification payload;

	payload.event = CPE_SVC_BOOT_FAILED;
	payload.result = CPE_SVC_FAILED;
	payload.payload = NULL;
	if (t_info)
		payload.private_data =
			t_info->client_context;
	cpe_broadcast_notification(t_info, &payload);
}

static enum cpe_svc_result broadcast_boot_event(
		struct cpe_info *t_info)
{
	struct cpe_svc_notification payload;

	payload.event = CPE_SVC_ONLINE;
	payload.result = CPE_SVC_SUCCESS;
	payload.payload = NULL;
	if (t_info)
		payload.private_data =
			t_info->client_context;
	cpe_broadcast_notification(t_info, &payload);

	return CPE_SVC_SUCCESS;
}

static enum cpe_process_result cpe_boot_initialize(struct cpe_info *t_info,
	enum cpe_svc_result *cpe_rc)
{
	enum cpe_process_result rc = CPE_SVC_FAILED;
	struct cpe_svc_notification payload;
	struct cmi_core_svc_event_system_boot *p = NULL;

	if (CMI_GET_OPCODE(t_info->tgt->outbox) !=
		CPE_CORE_SVC_EVENT_SYSTEM_BOOT) {
		broacast_boot_failed();
		return rc;
	}

	p = (struct cmi_core_svc_event_system_boot *)
		CMI_GET_PAYLOAD(t_info->tgt->outbox);
	if (p->status != CPE_BOOT_SUCCESS) {
		pr_err("%s: cpe boot failed, status = %d\n",
			__func__, p->status);
		broacast_boot_failed();
		return rc;
	}

	/* boot was successful */
	if (p->version ==
	    CPE_CORE_VERSION_SYSTEM_BOOT_EVENT) {
		cpe_d.cpe_debug_vector.debug_address =
				p->sfr_buff_address;
		cpe_d.cpe_debug_vector.debug_buffer_size =
				p->sfr_buff_size;
		cpe_d.cpe_debug_vector.status = p->status;
		payload.event = CPE_SVC_BOOT;
		payload.result = CPE_SVC_SUCCESS;
		payload.payload = (void *)&cpe_d.cpe_debug_vector;
		payload.private_data = t_info->client_context;
		cpe_broadcast_notification(t_info, &payload);
	}
	cpe_change_state(t_info, CPE_STATE_BOOTING,
			 CPE_SS_BOOT_INIT);
	(*cpe_rc) = cpe_send_msg_to_inbox(t_info,
			CPE_CORE_SVC_CMD_SHARED_MEM_ALLOC, NULL);
	rc = CPE_PROC_SUCCESS;
	return rc;
}

static void cpe_svc_core_cmi_handler(
		const struct cmi_api_notification *parameter)
{
	struct cmi_hdr *hdr;

	pr_debug("%s: event = %d\n",
		 __func__, parameter->event);

	if (!parameter)
		return;

	if (parameter->event != CMI_API_MSG)
		return;

	hdr = (struct cmi_hdr *) parameter->message;

	if (hdr->opcode == CPE_CMI_BASIC_RSP_OPCODE) {
		struct cmi_basic_rsp_result *result;

		result = (struct cmi_basic_rsp_result *)
			((u8 *)parameter->message) + (sizeof(*hdr));
		if (result->status)
			pr_err("%s: error response, error code = %u\n",
				__func__, result->status);
		complete(&cpe_d.cpe_default_handle->core_svc_cmd_compl);
	}
}

static void cpe_clk_plan_work(struct work_struct *work)
{
	struct cpe_info *t_info = NULL;
	size_t size = 0;
	struct cpe_svc_cfg_clk_plan plan;
	u8 *cmi_msg;
	struct cmi_hdr *hdr;
	int rc;

	t_info = container_of(work, struct cpe_info, clk_plan_work);
	if (!t_info) {
		pr_err("%s: Invalid handle for cpe_info\n",
			__func__);
		return;
	}

	/* Register the core service */
	cpe_d.cpe_cmi_handle = cmi_register(
					cpe_svc_core_cmi_handler,
					CMI_CPE_CORE_SERVICE_ID);

	/* send the clk plan command */
	if (!cpe_d.cpe_query_freq_plans_cb) {
		pr_err("%s: No support for querying clk plans\n",
			__func__);
		return;
	}

	cpe_d.cpe_query_freq_plans_cb(cpe_d.cdc_priv, &plan);
	size = sizeof(plan.current_clk_feq) +
		sizeof(plan.num_clk_freqs);
	size += plan.num_clk_freqs *
		  sizeof(plan.clk_freqs[0]);
	cmi_msg = kzalloc(size + sizeof(struct cmi_hdr),
			  GFP_KERNEL);
	if (!cmi_msg) {
		pr_err("%s: no memory for cmi_msg\n",
			__func__);
		return;
	}

	hdr = (struct cmi_hdr *) cmi_msg;
	CMI_HDR_SET_OPCODE(hdr,
			   CPE_CORE_SVC_CMD_CFG_CLK_PLAN);
	CMI_HDR_SET_SERVICE(hdr, CMI_CPE_CORE_SERVICE_ID);
		CMI_HDR_SET_SESSION(hdr, 1);
	CMI_HDR_SET_VERSION(hdr, CMI_DRIVER_SUPPORTED_VERSION);
	CMI_HDR_SET_PAYLOAD_SIZE(hdr, size);
	memcpy(CMI_GET_PAYLOAD(cmi_msg), &plan,
	       size);
	cmi_send_msg(cmi_msg);

	/* Wait for clk plan command to complete */
	rc = wait_for_completion_timeout(&t_info->core_svc_cmd_compl,
					 (10 * HZ));
	if (!rc) {
		pr_err("%s: clk plan cmd timed out\n",
			__func__);
		goto cmd_fail;
	}

	/* clk plan cmd is successful, send start notification */
	if (t_info->cpe_start_notification)
		t_info->cpe_start_notification(t_info);
	else
		pr_err("%s: no start notification\n",
			 __func__);

cmd_fail:
	kfree(cmi_msg);
	cmi_deregister(cpe_d.cpe_cmi_handle);
}

static enum cpe_process_result cpe_boot_complete(
		struct cpe_info *t_info)
{
	struct cmi_core_svc_cmdrsp_shared_mem_alloc *p = NULL;

	if (CMI_GET_OPCODE(t_info->tgt->outbox) !=
		CPE_CORE_SVC_CMDRSP_SHARED_MEM_ALLOC) {
		broacast_boot_failed();
		return CPE_PROC_FAILED;
	}

	p = (struct cmi_core_svc_cmdrsp_shared_mem_alloc *)
		CMI_GET_PAYLOAD(t_info->tgt->outbox);
	cpe_d.cpe_msg_buffer = p->addr;

	if (cpe_d.cpe_msg_buffer == 0) {
		pr_err("%s: Invalid cpe buffer for message\n",
			__func__);
		broacast_boot_failed();
		return CPE_PROC_FAILED;
	}

	cpe_change_state(t_info, CPE_STATE_IDLE, CPE_SS_IDLE);
	cpe_create_worker_thread(t_info);

	if (t_info->codec_id != CPE_SVC_CODEC_TOMTOM) {
		schedule_work(&t_info->clk_plan_work);
	} else {
		if (t_info->cpe_start_notification)
			t_info->cpe_start_notification(t_info);
		else
			pr_err("%s: no start notification\n",
				__func__);
	}

	pr_debug("%s: boot complete\n", __func__);
	return CPE_SVC_SUCCESS;
}

static enum cpe_process_result cpe_process_send_msg(
	struct cpe_info *t_info,
	enum cpe_svc_result *cpe_rc,
	struct cpe_command_node *command_node)
{
	enum cpe_process_result rc = CPE_PROC_SUCCESS;
	struct cpe_send_msg *m =
		(struct cpe_send_msg *)command_node->data;
	u32 size = m->size;

	if (t_info->pending) {
		pr_debug("%s: message queued\n", __func__);
		*cpe_rc = CPE_SVC_SUCCESS;
		return CPE_PROC_QUEUED;
	}

	pr_debug("%s: Send CMI message, size = %u\n",
		 __func__, size);

	if (size <= t_info->tgt->tgt_get_cpe_info()->inbox_size) {
		pr_debug("%s: Msg fits mailbox, size %u\n",
			 __func__, size);
		cpe_change_state(t_info, CPE_STATE_SENDING_MSG,
			CPE_SS_MSG_SEND_INBOX);
		t_info->pending = m;
		*cpe_rc = cpe_send_msg_to_inbox(t_info, 0, m);
	} else if (size < CPE_MSG_BUFFER_SIZE) {
		m->address = cpe_d.cpe_msg_buffer;
		pr_debug("%s: Message req CMI mem access\n",
			 __func__);
		t_info->pending = m;
		cpe_change_state(t_info, CPE_STATE_SENDING_MSG,
			CPE_SS_MSG_REQUEST_ACCESS);
		*cpe_rc = cpe_send_msg_to_inbox(t_info,
			CPE_CORE_SVC_CMD_DRAM_ACCESS_REQ, m);
	} else {
		pr_debug("%s: Invalid msg size %u\n",
			 __func__, size);
		cpe_command_cleanup(command_node);
		rc = CPE_PROC_FAILED;
		cpe_change_state(t_info, CPE_STATE_IDLE,
			CPE_SS_IDLE);
	}

	return rc;
}

static enum cpe_process_result cpe_process_incoming(
		struct cpe_info *t_info)
{
	enum cpe_process_result rc = CPE_PROC_FAILED;
	struct cmi_hdr *hdr;

	hdr = CMI_GET_HEADER(t_info->tgt->outbox);

	if (CMI_HDR_GET_SERVICE(hdr) ==
	    CMI_CPE_CORE_SERVICE_ID) {
		pr_debug("%s: core service message received\n",
			 __func__);

		switch (CMI_GET_OPCODE(t_info->tgt->outbox)) {
		case CPE_CORE_SVC_CMD_CLK_FREQ_REQUEST:
			cpe_process_clk_change_req(t_info);
			rc = CPE_PROC_SUCCESS;
			break;
		case CMI_MSG_TRANSPORT:
			pr_debug("%s: transport msg received\n",
				 __func__);
			rc = CPE_PROC_SUCCESS;
			break;
		case CPE_CMI_BASIC_RSP_OPCODE:
			pr_debug("%s: received basic rsp\n",
				 __func__);
			rc = CPE_PROC_SUCCESS;
			break;
		default:
			pr_debug("%s: unknown message received\n",
				 __func__);
			break;
		}
	} else {
		/* if service id if for a CMI client, notify client */
		pr_debug("%s: Message received, notifying client\n",
			 __func__);
		cpe_notify_cmi_client(t_info,
			t_info->tgt->outbox, CPE_SVC_SUCCESS);
		rc = CPE_PROC_SUCCESS;
	}

	return rc;
}

static enum cpe_process_result cpe_process_kill_thread(
	struct cpe_info *t_info,
	struct cpe_command_node *command_node)
{
	struct cpe_svc_notification payload;

	cpe_d.cpe_msg_buffer = 0;
	payload.result = CPE_SVC_SHUTTING_DOWN;
	payload.event = CPE_SVC_OFFLINE;
	payload.payload = NULL;
	payload.private_data = t_info->client_context;
	/*
	 * Make state as offline before broadcasting
	 * the message to clients.
	 */
	cpe_change_state(t_info, CPE_STATE_OFFLINE,
			 CPE_SS_IDLE);
	cpe_broadcast_notification(t_info, &payload);

	return CPE_PROC_KILLED;
}

static enum cpe_process_result cpe_mt_process_cmd(
		struct cpe_command_node *command_node)
{
	struct cpe_info *t_info = cpe_d.cpe_default_handle;
	enum cpe_svc_result cpe_rc = CPE_SVC_SUCCESS;
	enum cpe_process_result rc = CPE_PROC_SUCCESS;
	struct cpe_send_msg *m;
	struct cmi_hdr *hdr;
	u8 service = 0;
	u8 retries = 0;

	if (!t_info || !command_node) {
		pr_err("%s: Invalid handle/command node\n",
			__func__);
		return CPE_PROC_FAILED;
	}

	pr_debug("%s: cmd = %u\n", __func__, command_node->command);

	cpe_rc = cpe_is_command_valid(t_info, command_node->command);

	if (cpe_rc != CPE_SVC_SUCCESS) {
		pr_err("%s: Invalid command %d, err = %d\n",
			__func__, command_node->command, cpe_rc);
		return CPE_PROC_FAILED;
	}

	switch (command_node->command) {

	case CPE_CMD_BOOT_INITIALIZE:
		rc = cpe_boot_initialize(t_info, &cpe_rc);
		break;

	case CPE_CMD_BOOT_COMPLETE:
		rc = cpe_boot_complete(t_info);
		break;

	case CPE_CMD_SEND_MSG:
		rc = cpe_process_send_msg(t_info, &cpe_rc,
					  command_node);
		break;

	case CPE_CMD_SEND_TRANS_MSG:
		m = (struct cpe_send_msg *)command_node->data;

		while (retries < CPE_SVC_INACTIVE_STATE_RETRIES_MAX) {
			if (t_info->tgt->tgt_is_active()) {
				++retries;
				/* Wait for CPE to be inactive */
				usleep_range(5000, 5100);
			} else {
				break;
			}
		}

		pr_debug("%s: cpe inactive after %d attempts\n",
			 __func__, retries);

		cpe_change_state(t_info, CPE_STATE_SENDING_MSG,
				CPE_SS_MSG_SEND_INBOX);
		rc = cpe_send_msg_to_inbox(t_info, 0, m);
		break;

	case CPE_CMD_SEND_MSG_COMPLETE:
		hdr = CMI_GET_HEADER(t_info->tgt->outbox);
		service = CMI_HDR_GET_SERVICE(hdr);
		pr_debug("%s: msg send success, notifying clients\n",
			 __func__);
		cpe_command_cleanup(command_node);
		t_info->pending = NULL;
		cpe_change_state(t_info,
				 CPE_STATE_IDLE, CPE_SS_IDLE);
		cpe_notify_cmi_client(t_info,
			t_info->tgt->outbox, CPE_SVC_SUCCESS);
		break;

	case CPE_CMD_PROC_INCOMING_MSG:
		rc = cpe_process_incoming(t_info);
		break;

	case CPE_CMD_KILL_THREAD:
		rc = cpe_process_kill_thread(t_info, command_node);
		break;

	default:
		pr_err("%s: unhandled cpe cmd = %d\n",
			__func__, command_node->command);
		break;
	}

	if (cpe_rc != CPE_SVC_SUCCESS) {
		pr_err("%s: failed to execute command\n", __func__);
		if (t_info->pending) {
			m = (struct cpe_send_msg *)t_info->pending;
			cpe_notify_cmi_client(t_info, m->payload,
					      CPE_SVC_FAILED);
			t_info->pending = NULL;
		}

		cpe_command_cleanup(command_node);
		rc = CPE_PROC_FAILED;
		cpe_change_state(t_info, CPE_STATE_IDLE,
			CPE_SS_IDLE);
	}

	return rc;
}

static enum cpe_svc_result cpe_mt_validate_cmd(
		const struct cpe_info *t_info,
		enum cpe_command command)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;

	if ((t_info == NULL) || t_info->initialized == false) {
		pr_err("%s: cpe service is not ready\n",
			__func__);
		return CPE_SVC_NOT_READY;
	}

	switch (t_info->state) {
	case CPE_STATE_UNINITIALIZED:
	case CPE_STATE_INITIALIZED:
		switch (command) {
		case CPE_CMD_RESET:
		case CPE_CMD_DL_SEGMENT:
		case CPE_CMD_RAMDUMP:
		case CPE_CMD_PROCESS_IRQ:
		case CPE_CMD_KILL_THREAD:
		case CPE_CMD_DEINITIALIZE:
		case CPE_CMD_FTM_TEST:
			rc = CPE_SVC_SUCCESS;
			break;
		default:
			rc = CPE_SVC_NOT_READY;
			break;
		}
		break;

	case CPE_STATE_DOWNLOADING:
		switch (command) {
		case CPE_CMD_RESET:
		case CPE_CMD_DL_SEGMENT:
		case CPE_CMD_BOOT:
		case CPE_CMD_FTM_TEST:
			rc = CPE_SVC_SUCCESS;
			break;
		default:
			rc = CPE_SVC_NOT_READY;
			break;
		}
		break;

	case CPE_STATE_BOOTING:
		switch (command) {
		case CPE_CMD_PROCESS_IRQ:
		case CPE_CMD_BOOT_INITIALIZE:
		case CPE_CMD_BOOT_COMPLETE:
		case CPE_CMD_SHUTDOWN:
			rc = CPE_SVC_SUCCESS;
			break;
		case CPE_CMD_FTM_TEST:
			rc = CPE_SVC_BUSY;
			break;
		default:
			rc = CPE_SVC_NOT_READY;
			break;
		}
		break;

	case CPE_STATE_IDLE:
		switch (command) {
		case CPE_CMD_SEND_MSG:
		case CPE_CMD_SEND_TRANS_MSG:
		case CPE_CMD_SEND_MSG_COMPLETE:
		case CPE_CMD_PROCESS_IRQ:
		case CPE_CMD_RESET:
		case CPE_CMD_SHUTDOWN:
		case CPE_CMD_KILL_THREAD:
		case CPE_CMD_PROC_INCOMING_MSG:
			rc = CPE_SVC_SUCCESS;
			break;
		case CPE_CMD_FTM_TEST:
			rc = CPE_SVC_BUSY;
			break;
		default:
			rc = CPE_SVC_FAILED;
			break;
		}
		break;

	case CPE_STATE_SENDING_MSG:
		switch (command) {
		case CPE_CMD_SEND_MSG:
		case CPE_CMD_SEND_TRANS_MSG:
		case CPE_CMD_SEND_MSG_COMPLETE:
		case CPE_CMD_PROCESS_IRQ:
		case CPE_CMD_SHUTDOWN:
		case CPE_CMD_KILL_THREAD:
		case CPE_CMD_PROC_INCOMING_MSG:
			rc = CPE_SVC_SUCCESS;
			break;
		case CPE_CMD_FTM_TEST:
			rc = CPE_SVC_BUSY;
			break;
		default:
			rc = CPE_SVC_FAILED;
			break;
		}
		break;

	case CPE_STATE_OFFLINE:
		switch (command) {
		case CPE_CMD_RESET:
		case CPE_CMD_RAMDUMP:
		case CPE_CMD_KILL_THREAD:
			rc = CPE_SVC_SUCCESS;
			break;
		default:
			rc = CPE_SVC_NOT_READY;
			break;
		}
		break;

	default:
		pr_debug("%s: unhandled state %d\n",
			 __func__, t_info->state);
		break;
	}

	if (rc != CPE_SVC_SUCCESS)
		pr_err("%s: invalid command %d, state = %d\n",
			__func__, command, t_info->state);
	return rc;
}

void *cpe_svc_initialize(
		void irq_control_callback(u32 enable),
		const void *codec_info, void *context)
{
	struct cpe_info *t_info = NULL;
	const struct cpe_svc_hw_cfg *cap = NULL;
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_svc_init_param *init_context =
		(struct cpe_svc_init_param *) context;
	void *client_context = NULL;

	if (cpe_d.cpe_default_handle &&
	    cpe_d.cpe_default_handle->initialized == true)
		return (void *)cpe_d.cpe_default_handle;
	cpe_d.cpe_query_freq_plans_cb = NULL;
	cpe_d.cpe_change_freq_plan_cb = NULL;

	if (context) {
		client_context = init_context->context;
		switch (init_context->version) {
		case CPE_SVC_INIT_PARAM_V1:
			cpe_d.cpe_query_freq_plans_cb =
				init_context->query_freq_plans_cb;
			cpe_d.cpe_change_freq_plan_cb =
				init_context->change_freq_plan_cb;
			break;
		default:
			break;
		}
	}

	if (!cpe_d.cpe_default_handle) {
		cpe_d.cpe_default_handle = kzalloc(sizeof(struct cpe_info),
					     GFP_KERNEL);
		if (!cpe_d.cpe_default_handle) {
			pr_err("%s: no memory for cpe handle, size = %zu\n",
				__func__, sizeof(struct cpe_info));
			goto err_register;
		}

		memset(cpe_d.cpe_default_handle, 0,
		       sizeof(struct cpe_info));
	}

	t_info = cpe_d.cpe_default_handle;
	t_info->client_context = client_context;

	INIT_LIST_HEAD(&t_info->client_list);
	cpe_d.cdc_priv = client_context;
	INIT_WORK(&t_info->clk_plan_work, cpe_clk_plan_work);
	init_completion(&t_info->core_svc_cmd_compl);

	t_info->tgt = kzalloc(sizeof(struct cpe_svc_tgt_abstraction),
			      GFP_KERNEL);
	if (!t_info->tgt) {
		pr_err("%s: target allocation failed, size = %zu\n",
			__func__,
			sizeof(struct cpe_svc_tgt_abstraction));
		goto err_tgt_alloc;
	}
	t_info->codec_id =
		((struct cpe_svc_codec_info_v1 *) codec_info)->id;

	rc = cpe_svc_tgt_init((struct cpe_svc_codec_info_v1 *)codec_info,
			t_info->tgt);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: target initialization failed, err = %d\n",
			__func__, rc);
		goto err_tgt_init;
	}

	cap = t_info->tgt->tgt_get_cpe_info();

	memset(t_info->tgt->outbox, 0, cap->outbox_size);
	memset(t_info->tgt->inbox, 0, cap->inbox_size);
	mutex_init(&t_info->msg_lock);
	cpe_d.cpe_irq_control_callback = irq_control_callback;
	t_info->cpe_process_command = cpe_mt_process_cmd;
	t_info->cpe_cmd_validate = cpe_mt_validate_cmd;
	t_info->cpe_start_notification = broadcast_boot_event;
	mutex_init(&cpe_d.cpe_api_mutex);
	mutex_init(&cpe_d.cpe_svc_lock);
	pr_debug("%s: cpe services initialized\n", __func__);
	t_info->state = CPE_STATE_INITIALIZED;
	t_info->initialized = true;

	return t_info;

err_tgt_init:
	kfree(t_info->tgt);

err_tgt_alloc:
	kfree(cpe_d.cpe_default_handle);
	cpe_d.cpe_default_handle = NULL;

err_register:
	return NULL;
}

enum cpe_svc_result cpe_svc_deinitialize(void *cpe_handle)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	if (!t_info)
		t_info = cpe_d.cpe_default_handle;

	rc = cpe_is_command_valid(t_info, CPE_CMD_DEINITIALIZE);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: Invalid command %d\n",
			__func__, CPE_CMD_DEINITIALIZE);
		return rc;
	}

	if (cpe_d.cpe_default_handle == t_info)
		cpe_d.cpe_default_handle = NULL;

	t_info->tgt->tgt_deinit(t_info->tgt);
	cpe_change_state(t_info, CPE_STATE_UNINITIALIZED,
			 CPE_SS_IDLE);
	mutex_destroy(&t_info->msg_lock);
	kfree(t_info->tgt);
	kfree(t_info);
	mutex_destroy(&cpe_d.cpe_api_mutex);
	mutex_destroy(&cpe_d.cpe_svc_lock);

	return rc;
}

void *cpe_svc_register(void *cpe_handle,
		void (*notification_callback)
			(const struct cpe_svc_notification *parameter),
		u32 mask, const char *name)
{
	void *reg_handle;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	if (!cpe_d.cpe_default_handle) {
		cpe_d.cpe_default_handle = kzalloc(sizeof(struct cpe_info),
					     GFP_KERNEL);
		if (!cpe_d.cpe_default_handle) {
			pr_err("%s: no_mem for cpe handle, sz = %zu\n",
				__func__, sizeof(struct cpe_info));
			CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
			return NULL;
		}

		memset(cpe_d.cpe_default_handle, 0,
			sizeof(struct cpe_info));
	}

	if (!cpe_handle)
		cpe_handle = cpe_d.cpe_default_handle;

	reg_handle = cpe_register_generic((struct cpe_info *)cpe_handle,
					   notification_callback,
					   NULL,
					   mask, CPE_NO_SERVICE, name);
	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");

	return reg_handle;
}

enum cpe_svc_result cpe_svc_deregister(void *cpe_handle, void *reg_handle)
{
	enum cpe_svc_result rc;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	if (!cpe_handle)
		cpe_handle = cpe_d.cpe_default_handle;

	rc = cpe_deregister_generic((struct cpe_info *)cpe_handle,
				    reg_handle);
	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");

	return rc;
}

enum cpe_svc_result cpe_svc_download_segment(void *cpe_handle,
	const struct cpe_svc_mem_segment *segment)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	if (!t_info)
		t_info = cpe_d.cpe_default_handle;

	rc = cpe_is_command_valid(t_info, CPE_CMD_DL_SEGMENT);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: cmd validation fail, cmd = %d\n",
			__func__, CPE_CMD_DL_SEGMENT);
		CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
		return rc;
	}

	cpe_toggle_irq_notification(t_info, false);
	t_info->state = CPE_STATE_DOWNLOADING;
	t_info->substate = CPE_SS_DL_DOWNLOADING;
	rc = t_info->tgt->tgt_write_ram(t_info, segment);
	cpe_toggle_irq_notification(t_info, true);
	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");

	return rc;
}

enum cpe_svc_result cpe_svc_boot(void *cpe_handle, int debug_mode)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	if (!t_info)
		t_info = cpe_d.cpe_default_handle;

	rc = cpe_is_command_valid(t_info, CPE_CMD_BOOT);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: cmd validation fail, cmd = %d\n",
			__func__, CPE_CMD_BOOT);
		CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
		return rc;
	}

	if (rc == CPE_SVC_SUCCESS) {
		t_info->tgt->tgt_boot(debug_mode);
		t_info->state = CPE_STATE_BOOTING;
		t_info->substate = CPE_SS_BOOT;
		pr_debug("%s: cpe service booting\n",
			 __func__);
	}

	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	return rc;
}

enum cpe_svc_result cpe_svc_process_irq(void *cpe_handle, u32 cpe_irq)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	if (!t_info)
		t_info = cpe_d.cpe_default_handle;

	cpe_toggle_irq_notification(t_info, false);
	cpe_process_irq_int(cpe_irq, t_info);
	cpe_toggle_irq_notification(t_info, true);

	return rc;
}

enum cpe_svc_result cpe_svc_route_notification(void *cpe_handle,
		enum cpe_svc_module module, enum cpe_svc_route_dest dest)
{
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;
	enum cpe_svc_result rc = CPE_SVC_NOT_READY;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	if (!t_info)
		t_info = cpe_d.cpe_default_handle;

	if (t_info->tgt)
		rc = t_info->tgt->tgt_route_notification(module, dest);

	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	return rc;
}

static enum cpe_svc_result __cpe_svc_shutdown(void *cpe_handle)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;
	struct cpe_command_node *n = NULL;
	struct cpe_command_node kill_cmd;

	if (!t_info)
		t_info = cpe_d.cpe_default_handle;

	rc = cpe_is_command_valid(t_info, CPE_CMD_SHUTDOWN);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: cmd validation fail, cmd = %d\n",
			__func__, CPE_CMD_SHUTDOWN);
		return rc;
	}

	while (!list_empty(&t_info->main_queue)) {
		n = list_first_entry(&t_info->main_queue,
				     struct cpe_command_node, list);

		if (n->command == CPE_CMD_SEND_MSG) {
			cpe_notify_cmi_client(t_info, (u8 *)n->data,
				CPE_SVC_SHUTTING_DOWN);
		}
		/*
		 * Since command cannot be processed,
		 * delete it from the list and perform cleanup
		 */
		list_del(&n->list);
		cpe_command_cleanup(n);
		kfree(n);
	}

	pr_debug("%s: cpe service OFFLINE state\n", __func__);

	t_info->state = CPE_STATE_OFFLINE;
	t_info->substate = CPE_SS_IDLE;

	memset(&kill_cmd, 0, sizeof(kill_cmd));
	kill_cmd.command = CPE_CMD_KILL_THREAD;

	if (t_info->pending) {
		struct cpe_send_msg *m =
			(struct cpe_send_msg *)t_info->pending;
		cpe_notify_cmi_client(t_info, m->payload,
			CPE_SVC_SHUTTING_DOWN);
		kfree(t_info->pending);
		t_info->pending = NULL;
	}

	cpe_cleanup_worker_thread(t_info);
	t_info->cpe_process_command(&kill_cmd);

	return rc;
}

enum cpe_svc_result cpe_svc_shutdown(void *cpe_handle)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	rc = __cpe_svc_shutdown(cpe_handle);
	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	return rc;
}

enum cpe_svc_result cpe_svc_reset(void *cpe_handle)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	if (!t_info)
		t_info = cpe_d.cpe_default_handle;

	rc = cpe_is_command_valid(t_info, CPE_CMD_RESET);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: cmd validation fail, cmd = %d\n",
			__func__, CPE_CMD_RESET);
		CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
		return rc;
	}

	if (t_info && t_info->tgt) {
		rc = t_info->tgt->tgt_reset();
		pr_debug("%s: cpe services in INITIALIZED state\n",
			 __func__);
		t_info->state = CPE_STATE_INITIALIZED;
		t_info->substate = CPE_SS_IDLE;
	}
	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");

	return rc;
}

enum cpe_svc_result cpe_svc_ramdump(void *cpe_handle,
		struct cpe_svc_mem_segment *buffer)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	if (!t_info)
		t_info = cpe_d.cpe_default_handle;

	rc = cpe_is_command_valid(t_info, CPE_CMD_RAMDUMP);
	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: cmd validation fail, cmd = %d\n",
			__func__, CPE_CMD_RAMDUMP);
		CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
		return rc;
	}

	if (t_info->tgt) {
		rc = t_info->tgt->tgt_read_ram(t_info, buffer);
	} else {
		pr_err("%s: cpe service not ready\n", __func__);
		rc = CPE_SVC_NOT_READY;
	}
	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");

	return rc;
}

enum cpe_svc_result cpe_svc_set_debug_mode(void *cpe_handle, u32 mode)
{
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;
	enum cpe_svc_result rc = CPE_SVC_INVALID_HANDLE;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	if (!t_info)
		t_info = cpe_d.cpe_default_handle;

	if (t_info->tgt)
		rc = t_info->tgt->tgt_set_debug_mode(mode);
	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");

	return rc;
}

const struct cpe_svc_hw_cfg *cpe_svc_get_hw_cfg(void *cpe_handle)
{
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	if (!t_info)
		t_info = cpe_d.cpe_default_handle;

	if (t_info->tgt)
		return t_info->tgt->tgt_get_cpe_info();

	return NULL;
}

void *cmi_register(
		void notification_callback(
			const struct cmi_api_notification *parameter),
		u32 service)
{
	void *reg_handle = NULL;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	reg_handle = cpe_register_generic(cpe_d.cpe_default_handle,
			NULL,
			notification_callback,
			(CPE_SVC_CMI_MSG | CPE_SVC_OFFLINE |
			 CPE_SVC_ONLINE),
			service,
			"CMI_CLIENT");
	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");

	return reg_handle;
}

enum cmi_api_result cmi_deregister(void *reg_handle)
{
	u32 clients = 0;
	struct cpe_notif_node *n = NULL;
	enum cmi_api_result rc = CMI_API_SUCCESS;
	struct cpe_svc_notification payload;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	rc = (enum cmi_api_result) cpe_deregister_generic(
		cpe_d.cpe_default_handle, reg_handle);

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_svc_lock, "cpe_svc");
	list_for_each_entry(n, &cpe_d.cpe_default_handle->client_list, list) {
		if (n->mask & CPE_SVC_CMI_MSG)
			clients++;
	}
	CPE_SVC_REL_LOCK(&cpe_d.cpe_svc_lock, "cpe_svc");

	if (clients == 0) {
		payload.event = CPE_SVC_CMI_CLIENTS_DEREG;
		payload.payload = NULL;
		payload.result = CPE_SVC_SUCCESS;
		cpe_broadcast_notification(cpe_d.cpe_default_handle, &payload);
	}

	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	return rc;
}

enum cmi_api_result cmi_send_msg(void *message)
{
	enum cmi_api_result rc = CMI_API_SUCCESS;
	struct cpe_send_msg *msg = NULL;
	struct cmi_hdr *hdr;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	hdr = CMI_GET_HEADER(message);
	msg = kzalloc(sizeof(struct cpe_send_msg),
		      GFP_ATOMIC);
	if (!msg) {
		pr_err("%s: no memory for cmi msg, sz = %zu\n",
			__func__, sizeof(struct cpe_send_msg));
		CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
		return CPE_SVC_NO_MEMORY;
	}

	if (CMI_HDR_GET_OBM_FLAG(hdr) == CMI_OBM_FLAG_OUT_BAND)
		msg->isobm = 1;
	else
		msg->isobm = 0;

	msg->size = sizeof(struct cmi_hdr) +
			CMI_HDR_GET_PAYLOAD_SIZE(hdr);

	msg->payload = kzalloc(msg->size, GFP_ATOMIC);
	if (!msg->payload) {
		pr_err("%s: no memory for cmi payload, sz = %zd\n",
			__func__, msg->size);
		kfree(msg);
		CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
		return CPE_SVC_NO_MEMORY;
	}

	msg->address = 0;
	memcpy((void *)msg->payload, message, msg->size);

	rc = (enum cmi_api_result) cpe_send_cmd_to_thread(
			cpe_d.cpe_default_handle,
			CPE_CMD_SEND_MSG,
			(void *)msg, false);

	if (rc != 0) {
		pr_err("%s: Failed to queue message\n", __func__);
		kfree(msg->payload);
		kfree(msg);
	}

	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	return rc;
}

enum cpe_svc_result cpe_svc_ftm_test(void *cpe_handle, u32 *status)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;
	struct cpe_svc_mem_segment backup_seg;
	struct cpe_svc_mem_segment waiti_seg;
	u8 *backup_data = NULL;

	CPE_SVC_GRAB_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	if (!t_info)
		t_info = cpe_d.cpe_default_handle;

	rc = cpe_is_command_valid(t_info, CPE_CMD_FTM_TEST);
	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: cmd validation fail, cmd = %d\n",
			__func__, CPE_CMD_FTM_TEST);
		goto fail_cmd;
	}

	if (t_info && t_info->tgt) {
		backup_data = kzalloc(
				t_info->tgt->tgt_waiti_info->tgt_waiti_size,
				GFP_KERNEL);

		/* CPE reset */
		rc = t_info->tgt->tgt_reset();
		if (rc != CPE_SVC_SUCCESS) {
			pr_err("%s: CPE reset fail! err = %d\n",
				__func__, rc);
			goto err_return;
		}

		/* Back up the 4 byte IRAM data first */
		backup_seg.type = CPE_SVC_INSTRUCTION_MEM;
		backup_seg.cpe_addr =
			t_info->tgt->tgt_get_cpe_info()->IRAM_offset;
		backup_seg.size = t_info->tgt->tgt_waiti_info->tgt_waiti_size;
		backup_seg.data = backup_data;

		pr_debug("%s: Backing up IRAM data from CPE\n",
			__func__);

		rc = t_info->tgt->tgt_read_ram(t_info, &backup_seg);
		if (rc != CPE_SVC_SUCCESS) {
			pr_err("%s: Fail to backup CPE IRAM data, err = %d\n",
				__func__, rc);
			goto err_return;
		}

		pr_debug("%s: Complete backing up IRAM data from CPE\n",
			__func__);

		/* Write the WAITI instruction data */
		waiti_seg.type = CPE_SVC_INSTRUCTION_MEM;
		waiti_seg.cpe_addr =
			t_info->tgt->tgt_get_cpe_info()->IRAM_offset;
		waiti_seg.size = t_info->tgt->tgt_waiti_info->tgt_waiti_size;
		waiti_seg.data = t_info->tgt->tgt_waiti_info->tgt_waiti_data;

		rc = t_info->tgt->tgt_write_ram(t_info, &waiti_seg);
		if (rc != CPE_SVC_SUCCESS) {
			pr_err("%s: Fail to write the WAITI data, err = %d\n",
				__func__, rc);
			goto restore_iram;
		}

		/* Boot up cpe to execute the WAITI instructions */
		rc = t_info->tgt->tgt_boot(1);
		if (rc != CPE_SVC_SUCCESS) {
			pr_err("%s: Fail to boot CPE, err = %d\n",
				__func__, rc);
			goto reset;
		}

		/*
		 * 1ms delay is suggested by the hw team to
		 * wait for cpe to boot up.
		 */
		usleep_range(1000, 1100);

		/* Check if the cpe init is done after executing the WAITI */
		*status = t_info->tgt->tgt_cpar_init_done();

reset:
		/* Set the cpe back to reset state */
		rc = t_info->tgt->tgt_reset();
		if (rc != CPE_SVC_SUCCESS) {
			pr_err("%s: CPE reset fail! err = %d\n",
				__func__, rc);
			goto restore_iram;
		}

restore_iram:
		/* Restore the IRAM 4 bytes data */
		rc = t_info->tgt->tgt_write_ram(t_info, &backup_seg);
		if (rc != CPE_SVC_SUCCESS) {
			pr_err("%s: Fail to restore the IRAM data, err = %d\n",
				__func__, rc);
			goto err_return;
		}
	}

err_return:
	kfree(backup_data);
fail_cmd:
	CPE_SVC_REL_LOCK(&cpe_d.cpe_api_mutex, "cpe_api");
	return rc;
}

static enum cpe_svc_result cpe_tgt_tomtom_boot(int debug_mode)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;

	if (!debug_mode)
		rc = cpe_update_bits(TOMTOM_A_SVASS_CPAR_WDOG_CFG,
				     0x3F, 0x31);
	else
		pr_info("%s: CPE in debug mode, WDOG disabled\n",
			__func__);

	rc = cpe_update_bits(TOMTOM_A_SVASS_CLKRST_CTL,
			     0x02, 0x00);
	rc = cpe_update_bits(TOMTOM_A_SVASS_CLKRST_CTL,
			     0x0C, 0x04);
	rc = cpe_update_bits(TOMTOM_A_SVASS_CPAR_CFG,
			     0x01, 0x01);

	return rc;
}

static u32 cpe_tgt_tomtom_is_cpar_init_done(void)
{
	u8 status = 0;
	cpe_register_read(TOMTOM_A_SVASS_STATUS, &status);
	return status & 0x01;
}

static u32 cpe_tgt_tomtom_is_active(void)
{
	u8 status = 0;
	cpe_register_read(TOMTOM_A_SVASS_STATUS, &status);
	return status & 0x04;
}

static enum cpe_svc_result cpe_tgt_tomtom_reset(void)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;

	rc = cpe_update_bits(TOMTOM_A_SVASS_CPAR_WDOG_CFG,
			     0x30, 0x00);

	rc = cpe_update_bits(TOMTOM_A_SVASS_CPAR_CFG,
			     0x01, 0x00);
	rc = cpe_update_bits(TOMTOM_A_MEM_LEAKAGE_CTL,
			     0x07, 0x03);
	rc = cpe_update_bits(TOMTOM_A_SVASS_CLKRST_CTL,
			     0x08, 0x08);
	rc = cpe_update_bits(TOMTOM_A_SVASS_CLKRST_CTL,
			     0x02, 0x02);
	return rc;
}

enum cpe_svc_result cpe_tgt_tomtom_voicetx(bool enable)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u8 val = 0;

	if (enable)
		val = 0x02;
	else
		val = 0x00;
	rc = cpe_update_bits(TOMTOM_A_SVASS_CFG,
			     0x02, val);
	val = 0;
	cpe_register_read(TOMTOM_A_SVASS_CFG, &val);
	return rc;
}

enum cpe_svc_result cpe_svc_toggle_lab(void *cpe_handle, bool enable)
{

	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	if (!t_info)
		t_info = cpe_d.cpe_default_handle;

	if (t_info->tgt)
		return t_info->tgt->tgt_voice_tx_lab(enable);
	else
		return CPE_SVC_INVALID_HANDLE;
}

static enum cpe_svc_result cpe_tgt_tomtom_read_mailbox(u8 *buffer,
	size_t size)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u32 cnt = 0;

	if (size >= TOMTOM_A_SVASS_SPE_OUTBOX_SIZE)
		size = TOMTOM_A_SVASS_SPE_OUTBOX_SIZE - 1;
	for (cnt = 0; (cnt < size) && (rc == CPE_SVC_SUCCESS); cnt++) {
		rc = cpe_register_read(TOMTOM_A_SVASS_SPE_OUTBOX(cnt),
			&(buffer[cnt]));
	}
	return rc;
}

static enum cpe_svc_result cpe_tgt_tomtom_write_mailbox(u8 *buffer,
	size_t size)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u32 cnt = 0;

	if (size >= TOMTOM_A_SVASS_SPE_INBOX_SIZE)
		size = TOMTOM_A_SVASS_SPE_INBOX_SIZE - 1;
	for (cnt = 0; (cnt < size) && (rc == CPE_SVC_SUCCESS); cnt++) {
		rc = cpe_register_write(TOMTOM_A_SVASS_SPE_INBOX(cnt),
			buffer[cnt]);
	}

	if (rc == CPE_SVC_SUCCESS)
		rc = cpe_register_write(TOMTOM_A_SVASS_SPE_INBOX_TRG, 1);

	return rc;
}

static enum cpe_svc_result cpe_get_mem_addr(struct cpe_info *t_info,
		const struct cpe_svc_mem_segment *mem_seg,
		u32 *addr, u8 *mem)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u32 offset, mem_sz, address;
	u8 mem_type;

	switch (mem_seg->type) {

	case CPE_SVC_DATA_MEM:
		mem_type = MEM_ACCESS_DRAM_VAL;
		offset = TOMTOM_A_SVASS_SPE_DRAM_OFFSET;
		mem_sz = TOMTOM_A_SVASS_SPE_DRAM_SIZE;
		break;

	case CPE_SVC_INSTRUCTION_MEM:
		mem_type = MEM_ACCESS_IRAM_VAL;
		offset = TOMTOM_A_SVASS_SPE_IRAM_OFFSET;
		mem_sz = TOMTOM_A_SVASS_SPE_IRAM_SIZE;
		break;

	default:
		pr_err("%s: Invalid mem type = %u\n",
			__func__, mem_seg->type);
		return CPE_SVC_INVALID_HANDLE;
	}

	if (mem_seg->cpe_addr < offset) {
		pr_err("%s: Invalid addr %x for mem type %u\n",
			__func__, mem_seg->cpe_addr, mem_type);
			return CPE_SVC_INVALID_HANDLE;
	}

	address = mem_seg->cpe_addr - offset;
	if (address + mem_seg->size > mem_sz) {
		pr_err("%s: wrong size %zu, start adress %x, mem_type %u\n",
			__func__, mem_seg->size, address, mem_type);
		return CPE_SVC_INVALID_HANDLE;
	}

	(*addr) = address;
	(*mem) = mem_type;

	return rc;
}

static enum cpe_svc_result cpe_tgt_tomtom_read_RAM(struct cpe_info *t_info,
		struct cpe_svc_mem_segment *mem_seg)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u8 mem_reg_val = 0;
	u32 cnt = 0;
	bool autoinc;
	u8 mem = MEM_ACCESS_NONE_VAL;
	u32 addr = 0;
	u32 ptr_update = true;

	if (!mem_seg) {
		pr_err("%s: Invalid mem segment\n",
			__func__);
		return CPE_SVC_INVALID_HANDLE;
	}

	rc = cpe_get_mem_addr(t_info, mem_seg, &addr, &mem);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: Cannot obtain address, mem_type %u\n",
			__func__, mem_seg->type);
		return rc;
	}

	rc = cpe_register_write(TOMTOM_A_SVASS_MEM_CTL, 0);
	autoinc = cpe_register_read_autoinc_supported();
	if (autoinc)
		mem_reg_val |= 0x04;

	mem_reg_val |= 0x08;
	mem_reg_val |= mem;

	do {
		if (!autoinc || ptr_update) {
			rc = cpe_register_write(TOMTOM_A_SVASS_MEM_PTR0,
				(addr & 0xFF));
			rc = cpe_register_write(TOMTOM_A_SVASS_MEM_PTR1,
				((addr >> 8) & 0xFF));
			rc = cpe_register_write(TOMTOM_A_SVASS_MEM_PTR2,
				((addr >> 16) & 0xFF));

			rc = cpe_register_write(TOMTOM_A_SVASS_MEM_CTL,
						mem_reg_val);

			ptr_update = false;
		}
		rc = cpe_register_read(TOMTOM_A_SVASS_MEM_BANK,
			&mem_seg->data[cnt]);

		if (!autoinc)
			rc = cpe_register_write(TOMTOM_A_SVASS_MEM_CTL, 0);
	} while (++cnt < mem_seg->size);

	rc = cpe_register_write(TOMTOM_A_SVASS_MEM_CTL, 0);

	return rc;
}

static enum cpe_svc_result cpe_tgt_tomtom_write_RAM(struct cpe_info *t_info,
		const struct cpe_svc_mem_segment *mem_seg)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u8 mem_reg_val = 0;
	u8 mem = MEM_ACCESS_NONE_VAL;
	u32 addr = 0;
	u8 *temp_ptr = NULL;
	u32 temp_size = 0;
	bool autoinc;

	if (!mem_seg) {
		pr_err("%s: Invalid mem segment\n",
			__func__);
		return CPE_SVC_INVALID_HANDLE;
	}

	rc = cpe_get_mem_addr(t_info, mem_seg, &addr, &mem);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: Cannot obtain address, mem_type %u\n",
			__func__, mem_seg->type);
		return rc;
	}

	autoinc = cpe_register_read_autoinc_supported();
	if (autoinc)
		mem_reg_val |= 0x04;
	mem_reg_val |= mem;

	rc = cpe_update_bits(TOMTOM_A_SVASS_MEM_CTL,
			     0x0F, mem_reg_val);

	rc = cpe_register_write(TOMTOM_A_SVASS_MEM_PTR0,
				(addr & 0xFF));
	rc = cpe_register_write(TOMTOM_A_SVASS_MEM_PTR1,
				((addr >> 8) & 0xFF));

	rc = cpe_register_write(TOMTOM_A_SVASS_MEM_PTR2,
				((addr >> 16) & 0xFF));

	temp_size = 0;
	temp_ptr = mem_seg->data;

	while (temp_size <= mem_seg->size) {
		u32 to_write = (mem_seg->size >= temp_size+CHUNK_SIZE)
			? CHUNK_SIZE : (mem_seg->size-temp_size);

		if (t_info->state == CPE_STATE_OFFLINE) {
			pr_err("%s: CPE is offline\n", __func__);
			return CPE_SVC_FAILED;
		}

		cpe_register_write_repeat(TOMTOM_A_SVASS_MEM_BANK,
			temp_ptr, to_write);
		temp_size += CHUNK_SIZE;
		temp_ptr += CHUNK_SIZE;
	}

	rc = cpe_register_write(TOMTOM_A_SVASS_MEM_CTL, 0);
	return rc;
}

static enum cpe_svc_result cpe_tgt_tomtom_route_notification(
		enum cpe_svc_module module,
		enum cpe_svc_route_dest dest)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u8 ctl_reg_val = 0;

	switch (module) {
	case CPE_SVC_LISTEN_PROC:
		switch (dest) {
		case CPE_SVC_EXTERNAL:
			ctl_reg_val = LISTEN_CTL_MSM_VAL;
			break;
		case CPE_SVC_INTERNAL:
			ctl_reg_val = LISTEN_CTL_SPE_VAL;
			break;
		default:
			pr_err("%s: Invalid dest %d\n",
				__func__, dest);
			return CPE_SVC_FAILED;
		}

		rc = cpe_update_bits(TOMTOM_A_SVASS_CFG,
				     0x01, ctl_reg_val);
		break;
	default:
		pr_err("%s: Invalid module %d\n",
			__func__, module);
		rc = CPE_SVC_FAILED;
		break;
	}

	return rc;
}

static enum cpe_svc_result cpe_tgt_tomtom_set_debug_mode(u32 enable)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u8 dbg_reg_val = 0x00;
	if (enable)
		dbg_reg_val = 0x08;
	rc = cpe_update_bits(TOMTOM_A_SVASS_DEBUG,
			     0x08, dbg_reg_val);
	return rc;
}

static const struct cpe_svc_hw_cfg *cpe_tgt_tomtom_get_cpe_info(void)
{
	return &cpe_svc_tomtom_info;
}

static enum cpe_svc_result cpe_tgt_tomtom_deinit(
		struct cpe_svc_tgt_abstraction *param)
{
	kfree(param->inbox);
	param->inbox = NULL;
	kfree(param->outbox);
	param->outbox = NULL;
	memset(param, 0, sizeof(struct cpe_svc_tgt_abstraction));
	return CPE_SVC_SUCCESS;
}

static u8 cpe_tgt_tomtom_waiti_data[] = {0x00, 0x70, 0x00, 0x00};

static struct cpe_tgt_waiti_info cpe_tgt_tomtom_waiti_info = {
	.tgt_waiti_size = ARRAY_SIZE(cpe_tgt_tomtom_waiti_data),
	.tgt_waiti_data = cpe_tgt_tomtom_waiti_data,
};

static enum cpe_svc_result cpe_tgt_tomtom_init(
		struct cpe_svc_codec_info_v1 *codec_info,
		struct cpe_svc_tgt_abstraction *param)
{
	if (!codec_info)
		return CPE_SVC_INVALID_HANDLE;
	if (!param)
		return CPE_SVC_INVALID_HANDLE;

	if (codec_info->id == CPE_SVC_CODEC_TOMTOM) {
		param->tgt_boot      = cpe_tgt_tomtom_boot;
		param->tgt_cpar_init_done = cpe_tgt_tomtom_is_cpar_init_done;
		param->tgt_is_active = cpe_tgt_tomtom_is_active;
		param->tgt_reset = cpe_tgt_tomtom_reset;
		param->tgt_read_mailbox = cpe_tgt_tomtom_read_mailbox;
		param->tgt_write_mailbox = cpe_tgt_tomtom_write_mailbox;
		param->tgt_read_ram = cpe_tgt_tomtom_read_RAM;
		param->tgt_write_ram = cpe_tgt_tomtom_write_RAM;
		param->tgt_route_notification =
			cpe_tgt_tomtom_route_notification;
		param->tgt_set_debug_mode = cpe_tgt_tomtom_set_debug_mode;
		param->tgt_get_cpe_info = cpe_tgt_tomtom_get_cpe_info;
		param->tgt_deinit = cpe_tgt_tomtom_deinit;
		param->tgt_voice_tx_lab = cpe_tgt_tomtom_voicetx;
		param->tgt_waiti_info = &cpe_tgt_tomtom_waiti_info;

		param->inbox = kzalloc(TOMTOM_A_SVASS_SPE_INBOX_SIZE,
				       GFP_KERNEL);
		if (!param->inbox) {
			pr_err("%s: no memory for inbox, sz = %d\n",
				__func__, TOMTOM_A_SVASS_SPE_INBOX_SIZE);
			return CPE_SVC_NO_MEMORY;
		}

		param->outbox = kzalloc(TOMTOM_A_SVASS_SPE_OUTBOX_SIZE,
					GFP_KERNEL);
		if (!param->outbox) {
			kfree(param->inbox);
			pr_err("%s: no memory for inbox, sz = %d\n",
				__func__, TOMTOM_A_SVASS_SPE_OUTBOX_SIZE);
			return CPE_SVC_NO_MEMORY;
		}
	}

	return CPE_SVC_SUCCESS;
}

static enum cpe_svc_result cpe_tgt_wcd9335_boot(int debug_mode)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;

	if (!debug_mode)
		rc |= cpe_update_bits(
				WCD9335_CPE_SS_WDOG_CFG,
				0x3f, 0x31);
	else
		pr_info("%s: CPE in debug mode, WDOG disabled\n",
			__func__);

	rc |= cpe_register_write(WCD9335_CPE_SS_CPARMAD_BUFRDY_INT_PERIOD, 19);
	rc |= cpe_update_bits(WCD9335_CPE_SS_CPAR_CTL, 0x04, 0x00);
	rc |= cpe_update_bits(WCD9335_CPE_SS_CPAR_CTL, 0x02, 0x02);
	rc |= cpe_update_bits(WCD9335_CPE_SS_CPAR_CTL, 0x01, 0x01);

	if (unlikely(rc)) {
		pr_err("%s: Failed to boot, err = %d\n",
			__func__, rc);
		rc = CPE_SVC_FAILED;
	}

	return rc;
}

static u32 cpe_tgt_wcd9335_is_cpar_init_done(void)
{
	u8 temp = 0;
	cpe_register_read(WCD9335_CPE_SS_STATUS, &temp);
	return temp & 0x1;
}

static u32 cpe_tgt_wcd9335_is_active(void)
{
	u8 temp = 0;
	cpe_register_read(WCD9335_CPE_SS_STATUS, &temp);
	return temp & 0x4;
}

static enum cpe_svc_result cpe_tgt_wcd9335_reset(void)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;

	rc |= cpe_update_bits(WCD9335_CPE_SS_CPAR_CFG, 0x01, 0x00);

	rc |= cpe_register_write(
		WCD9335_CODEC_RPM_PWR_CPE_IRAM_SHUTDOWN, 0x00);
	rc |= cpe_register_write(
		WCD9335_CODEC_RPM_PWR_CPE_DRAM1_SHUTDOWN, 0x00);
	rc |= cpe_register_write(
		WCD9335_CODEC_RPM_PWR_CPE_DRAM0_SHUTDOWN_1, 0x00);
	rc |= cpe_register_write(
		WCD9335_CODEC_RPM_PWR_CPE_DRAM0_SHUTDOWN_2, 0x00);

	rc |= cpe_update_bits(WCD9335_CPE_SS_CPAR_CTL, 0x04, 0x04);

	if (unlikely(rc)) {
		pr_err("%s: failed to reset cpe, err = %d\n",
			__func__, rc);
		rc = CPE_SVC_FAILED;
	}

	return rc;
}

static enum cpe_svc_result cpe_tgt_wcd9335_read_mailbox(u8 *buffer,
	size_t size)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u32 cnt = 0;

	pr_debug("%s: size=%zd\n", __func__, size);

	if (size > WCD9335_CPE_SS_SPE_OUTBOX_SIZE)
		size = WCD9335_CPE_SS_SPE_OUTBOX_SIZE;

	for (cnt = 0; (cnt < size) && (rc == CPE_SVC_SUCCESS); cnt++)
		rc = cpe_register_read(WCD9335_CPE_SS_SPE_OUTBOX1(cnt),
				       &buffer[cnt]);

	rc = cpe_register_write(WCD9335_CPE_SS_OUTBOX1_ACK, 0x01);

	if (unlikely(rc)) {
		pr_err("%s: failed to ACK outbox, err = %d\n",
			__func__, rc);
		rc = CPE_SVC_FAILED;
	}

	return rc;
}

static enum cpe_svc_result cpe_tgt_wcd9335_write_mailbox(u8 *buffer,
	size_t size)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u32 cnt = 0;

	pr_debug("%s: size = %zd\n", __func__, size);
	if (size > WCD9335_CPE_SS_SPE_INBOX_SIZE)
		size = WCD9335_CPE_SS_SPE_INBOX_SIZE;
	for (cnt = 0; (cnt < size) && (rc == CPE_SVC_SUCCESS); cnt++) {
		rc |= cpe_register_write(WCD9335_CPE_SS_SPE_INBOX1(cnt),
			buffer[cnt]);
	}

	if (unlikely(rc)) {
		pr_err("%s: Error %d writing mailbox registers\n",
			__func__, rc);
		return rc;
	}

	rc = cpe_register_write(WCD9335_CPE_SS_INBOX1_TRG, 1);
	return rc;
}

static enum cpe_svc_result cpe_wcd9335_get_mem_addr(struct cpe_info *t_info,
		const struct cpe_svc_mem_segment *mem_seg,
		u32 *addr, u8 *mem)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u32 offset, mem_sz, address;
	u8 mem_type;

	switch (mem_seg->type) {
	case CPE_SVC_DATA_MEM:
		mem_type = MEM_ACCESS_DRAM_VAL;
		offset = WCD9335_CPE_SS_SPE_DRAM_OFFSET;
		mem_sz = WCD9335_CPE_SS_SPE_DRAM_SIZE;
		break;

	case CPE_SVC_INSTRUCTION_MEM:
		mem_type = MEM_ACCESS_IRAM_VAL;
		offset = WCD9335_CPE_SS_SPE_IRAM_OFFSET;
		mem_sz = WCD9335_CPE_SS_SPE_IRAM_SIZE;
		break;

	default:
		pr_err("%s: Invalid mem type = %u\n",
			__func__, mem_seg->type);
		return CPE_SVC_INVALID_HANDLE;
	}

	if (mem_seg->cpe_addr < offset) {
		pr_err("%s: Invalid addr %x for mem type %u\n",
			__func__, mem_seg->cpe_addr, mem_type);
		return CPE_SVC_INVALID_HANDLE;
	}

	address = mem_seg->cpe_addr - offset;
	if (address + mem_seg->size > mem_sz) {
		pr_err("%s: wrong size %zu, start adress %x, mem_type %u\n",
			__func__, mem_seg->size, address, mem_type);
		return CPE_SVC_INVALID_HANDLE;
	}

	(*addr) = address;
	(*mem) = mem_type;

	return rc;
}

static enum cpe_svc_result cpe_tgt_wcd9335_read_RAM(struct cpe_info *t_info,
		struct cpe_svc_mem_segment *mem_seg)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u8 temp = 0;
	u32 cnt = 0;
	u8 mem = 0x0;
	u32 addr = 0;
	u32 lastaddr = 0;
	u32 ptr_update = true;
	bool autoinc;

	if (!mem_seg) {
		pr_err("%s: Invalid buffer\n", __func__);
		return CPE_SVC_INVALID_HANDLE;
	}

	rc = cpe_wcd9335_get_mem_addr(t_info, mem_seg, &addr, &mem);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: Cannot obtain address, mem_type %u\n",
			__func__, mem_seg->type);
		return rc;
	}

	rc |= cpe_register_write(WCD9335_CPE_SS_MEM_CTRL, 0);
	autoinc = cpe_register_read_autoinc_supported();

	if (autoinc)
		temp = 0x18;
	else
		temp = 0x10;

	temp |= mem;

	lastaddr = ~addr;
	do {
		if (!autoinc || (ptr_update)) {
			/* write LSB only if modified */
			if ((lastaddr & 0xFF) != (addr & 0xFF))
				rc |= cpe_register_write(
						WCD9335_CPE_SS_MEM_PTR_0,
						(addr & 0xFF));
			/* write middle byte only if modified */
			if (((lastaddr >> 8) & 0xFF) != ((addr >> 8) & 0xFF))
				rc |= cpe_register_write(
						WCD9335_CPE_SS_MEM_PTR_1,
						((addr>>8) & 0xFF));
			/* write MSB only if modified */
			if (((lastaddr >> 16) & 0xFF) != ((addr >> 16) & 0xFF))
				rc |= cpe_register_write(
						WCD9335_CPE_SS_MEM_PTR_2,
						((addr>>16) & 0xFF));

			rc |= cpe_register_write(WCD9335_CPE_SS_MEM_CTRL, temp);
			lastaddr = addr;
			addr++;
			ptr_update = false;
		}

		rc |= cpe_register_read(WCD9335_CPE_SS_MEM_BANK_0,
				       &mem_seg->data[cnt]);

		if (!autoinc)
			rc |= cpe_register_write(WCD9335_CPE_SS_MEM_CTRL, 0);
	} while ((++cnt < mem_seg->size) ||
		 (rc != CPE_SVC_SUCCESS));

	rc |= cpe_register_write(WCD9335_CPE_SS_MEM_CTRL, 0);

	if (rc)
		pr_err("%s: Failed to read registers, err = %d\n",
			__func__, rc);

	return rc;
}

static enum cpe_svc_result cpe_tgt_wcd9335_write_RAM(struct cpe_info *t_info,
		const struct cpe_svc_mem_segment *mem_seg)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u8 mem_reg_val = 0;
	u8 mem = MEM_ACCESS_NONE_VAL;
	u32 addr = 0;
	u8 *temp_ptr = NULL;
	u32 temp_size = 0;
	bool autoinc;

	if (!mem_seg) {
		pr_err("%s: Invalid mem segment\n",
			__func__);
		return CPE_SVC_INVALID_HANDLE;
	}

	rc = cpe_wcd9335_get_mem_addr(t_info, mem_seg, &addr, &mem);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: Cannot obtain address, mem_type %u\n",
			__func__, mem_seg->type);
		return rc;
	}

	autoinc = cpe_register_read_autoinc_supported();
	if (autoinc)
		mem_reg_val = 0x18;
	else
		mem_reg_val = 0x10;

	mem_reg_val |= mem;

	rc = cpe_update_bits(WCD9335_CPE_SS_MEM_CTRL,
			     0x0F, mem_reg_val);

	rc = cpe_register_write(WCD9335_CPE_SS_MEM_PTR_0,
				(addr & 0xFF));
	rc = cpe_register_write(WCD9335_CPE_SS_MEM_PTR_1,
				((addr >> 8) & 0xFF));

	rc = cpe_register_write(WCD9335_CPE_SS_MEM_PTR_2,
				((addr >> 16) & 0xFF));

	temp_size = 0;
	temp_ptr = mem_seg->data;

	while (temp_size <= mem_seg->size) {
		u32 to_write = (mem_seg->size >= temp_size+CHUNK_SIZE)
			? CHUNK_SIZE : (mem_seg->size - temp_size);

		if (t_info->state == CPE_STATE_OFFLINE) {
			pr_err("%s: CPE is offline\n", __func__);
			return CPE_SVC_FAILED;
		}

		cpe_register_write_repeat(WCD9335_CPE_SS_MEM_BANK_0,
			temp_ptr, to_write);
		temp_size += CHUNK_SIZE;
		temp_ptr += CHUNK_SIZE;
	}

	rc = cpe_register_write(WCD9335_CPE_SS_MEM_CTRL, 0);

	if (rc)
		pr_err("%s: Failed to write registers, err = %d\n",
			__func__, rc);
	return rc;
}

static enum cpe_svc_result cpe_tgt_wcd9335_route_notification(
		enum cpe_svc_module module,
		enum cpe_svc_route_dest dest)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;

	pr_debug("%s: Module = %d, Destination = %d\n",
		 __func__, module, dest);

	switch (module) {
	case CPE_SVC_LISTEN_PROC:
		switch (dest) {
		case CPE_SVC_EXTERNAL:
			rc = cpe_update_bits(WCD9335_CPE_SS_CFG, 0x01, 0x01);
			break;
		case CPE_SVC_INTERNAL:
			rc = cpe_update_bits(WCD9335_CPE_SS_CFG, 0x01, 0x00);
			break;
		default:
			pr_err("%s: Invalid destination %d\n",
				__func__, dest);
			return CPE_SVC_FAILED;
		}
		break;
	default:
		pr_err("%s: Invalid module %d\n",
			__func__, module);
		rc = CPE_SVC_FAILED;
		break;
	}
	return rc;
}

static enum cpe_svc_result cpe_tgt_wcd9335_set_debug_mode(u32 enable)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;

	pr_debug("%s: enable = %s\n", __func__,
		 (enable) ? "true" : "false");

	return rc;
}

static const struct cpe_svc_hw_cfg *cpe_tgt_wcd9335_get_cpe_info(void)
{
	return &cpe_svc_wcd9335_info;
}

static enum cpe_svc_result
cpe_tgt_wcd9335_deinit(struct cpe_svc_tgt_abstraction *param)
{
	kfree(param->inbox);
	param->inbox = NULL;
	kfree(param->outbox);
	param->outbox = NULL;
	memset(param, 0, sizeof(struct cpe_svc_tgt_abstraction));

	return CPE_SVC_SUCCESS;
}

static enum cpe_svc_result
	cpe_tgt_wcd9335_voicetx(bool enable)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	u8 val = 0;

	pr_debug("%s: enable = %u\n", __func__, enable);
	if (enable)
		val = 0x02;
	else
		val = 0x00;

	rc = cpe_update_bits(WCD9335_CPE_SS_CFG, 0x02, val);
	val = 0;
	cpe_register_read(WCD9335_CPE_SS_CFG, &val);

	return rc;
}

static u8 cpe_tgt_wcd9335_waiti_data[] = {0x00, 0x70, 0x00, 0x00};

static struct cpe_tgt_waiti_info cpe_tgt_wcd9335_waiti_info = {
	.tgt_waiti_size = ARRAY_SIZE(cpe_tgt_wcd9335_waiti_data),
	.tgt_waiti_data = cpe_tgt_wcd9335_waiti_data,
};

static enum cpe_svc_result cpe_tgt_wcd9335_init(
		struct cpe_svc_codec_info_v1 *codec_info,
		struct cpe_svc_tgt_abstraction *param)
{
	if (!codec_info)
		return CPE_SVC_INVALID_HANDLE;
	if (!param)
		return CPE_SVC_INVALID_HANDLE;

	if (codec_info->id == CPE_SVC_CODEC_WCD9335) {
		param->tgt_boot = cpe_tgt_wcd9335_boot;
		param->tgt_cpar_init_done = cpe_tgt_wcd9335_is_cpar_init_done;
		param->tgt_is_active = cpe_tgt_wcd9335_is_active;
		param->tgt_reset = cpe_tgt_wcd9335_reset;
		param->tgt_read_mailbox = cpe_tgt_wcd9335_read_mailbox;
		param->tgt_write_mailbox = cpe_tgt_wcd9335_write_mailbox;
		param->tgt_read_ram = cpe_tgt_wcd9335_read_RAM;
		param->tgt_write_ram = cpe_tgt_wcd9335_write_RAM;
		param->tgt_route_notification =
			cpe_tgt_wcd9335_route_notification;
		param->tgt_set_debug_mode = cpe_tgt_wcd9335_set_debug_mode;
		param->tgt_get_cpe_info = cpe_tgt_wcd9335_get_cpe_info;
		param->tgt_deinit = cpe_tgt_wcd9335_deinit;
		param->tgt_voice_tx_lab = cpe_tgt_wcd9335_voicetx;
		param->tgt_waiti_info = &cpe_tgt_wcd9335_waiti_info;

		param->inbox = kzalloc(WCD9335_CPE_SS_SPE_INBOX_SIZE,
				       GFP_KERNEL);
		if (!param->inbox) {
			pr_err("%s: no memory for inbox, sz = %d\n",
				__func__, WCD9335_CPE_SS_SPE_INBOX_SIZE);
			return CPE_SVC_NO_MEMORY;
		}

		param->outbox = kzalloc(WCD9335_CPE_SS_SPE_OUTBOX_SIZE,
					GFP_KERNEL);
		if (!param->outbox) {
			kfree(param->inbox);
			pr_err("%s: no memory for inbox, sz = %d\n",
				__func__, WCD9335_CPE_SS_SPE_OUTBOX_SIZE);
			return CPE_SVC_NO_MEMORY;
		}
	}

	return CPE_SVC_SUCCESS;
}

MODULE_DESCRIPTION("WCD CPE Services");
MODULE_LICENSE("GPL v2");
