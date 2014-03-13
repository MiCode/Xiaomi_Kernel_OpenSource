/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/mfd/wcd9xxx/core.h>
#include <sound/cpe_cmi.h>
#include <sound/soc.h>
#include <linux/mfd/wcd9xxx/wcd9330_registers.h>
#include "wcd_cpe_services.h"
#include "wcd_cmi_api.h"

#define CPE_MSG_BUFFER_SIZE 132
#define CPE_NO_SERVICE 0

#define CMI_DRIVER_SUPPORTED_VERSION 0
#define CMI_API_SUCCESS 0
#define CMI_MSG_TRANSPORT (0x0002)

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
#define CHUNK_SIZE 16

static const struct cpe_svc_hw_cfg cpe_svc_tomtom_info = {
	TOMTOM_A_SVASS_SPE_DRAM_SIZE,
	TOMTOM_A_SVASS_SPE_DRAM_OFFSET,
	TOMTOM_A_SVASS_SPE_IRAM_SIZE,
	TOMTOM_A_SVASS_SPE_IRAM_OFFSET,
	TOMTOM_A_SVASS_SPE_INBOX_SIZE,
	TOMTOM_A_SVASS_SPE_OUTBOX_SIZE
};

enum cpe_state {
	CPE_STATE_UNINITIALIZED,
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
	CPE_SS_IDLE,
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
	CPE_CMD_KILL_THREAD,
	CPE_CMD_BOOT,
	CPE_CMD_BOOT_INITIALIZE,
	CPE_CMD_BOOT_COMPLETE,
	CPE_CMD_SEND_MSG,
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
	CPE_LAB_CFG_SB
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
	enum cpe_state state;
	enum cpe_substate substate;
	struct list_head client_list;
	bool (*cpe_process_command)
			(struct cpe_command_node *command_node);
	enum cpe_svc_result (*cpe_cmd_validate)
				(const struct cpe_info *i,
				 enum cpe_command command);
	enum cpe_svc_result (*cpe_start_notification)
			     (const struct cpe_info *i);
	u32 initialized;
	struct cpe_svc_tgt_abstraction *tgt;
	void *pending;
	void *data;
};

struct cpe_svc_tgt_abstraction {
	enum cpe_svc_result (*tgt_boot) (int debug_mode);

	u32 (*tgt_cpar_init_done) (void);

	u32 (*tgt_is_active) (void);

	enum cpe_svc_result (*tgt_reset) (void);

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
	u8 *inbox;
	u8 *outbox;
};

static enum cpe_svc_result cpe_tgt_tomtom_init(
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
	struct list_head list;
};

static struct cpe_info *cpe_default_handle;
static void (*cpe_irq_control_callback)(u32 enable);
static u32 cpe_msg_buffer;
static struct mutex cpe_api_mutex;

static enum cpe_svc_result
cpe_is_command_valid(const struct cpe_info *t_info,
	enum cpe_command command);

static void *cdc_priv;

static int cpe_register_read(u32 reg, u8 *val)
{
	*(val) = snd_soc_read(cdc_priv, reg);
	return 0;
}

static enum cpe_svc_result cpe_update_bits(u32 reg,
		u32 mask, u32 value)
{
	int ret = 0;
	ret = snd_soc_update_bits(cdc_priv, reg,
				  mask, value);
	if (ret < 0)
		return CPE_SVC_FAILED;

	return CPE_SVC_SUCCESS;
}

static int cpe_register_write(u32 reg, u32 val)
{
	int ret = 0;

	if (reg != TOMTOM_A_SVASS_MEM_BANK)
		pr_debug("%s: reg = 0x%x, value = 0x%x\n",
			  __func__, reg, val);

	ret = snd_soc_write(cdc_priv, reg, val);
	if (ret < 0)
		return CPE_SVC_FAILED;

	return CPE_SVC_SUCCESS;
}

static int cpe_register_write_repeat(u32 reg, u8 *ptr, u32 to_write)
{
	struct snd_soc_codec *codec = cdc_priv;
	struct wcd9xxx *wcd9xxx = codec->control_data;
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

static void cpe_cmd_received(struct cpe_info *t_info)
{
	struct cpe_command_node *node = NULL;
	bool check_next = true;
	if (!t_info) {
		pr_err("%s: Invalid thread info\n",
			__func__);
		return;
	}

	while (!list_empty(&t_info->main_queue)) {
		if (!check_next)
			break;
		node = list_first_entry(&t_info->main_queue,
					struct cpe_command_node, list);
		if (!node)
			break;
		list_del(&node->list);
		check_next = t_info->cpe_process_command(node);
		if (check_next)
			kfree(node);
		else
			list_add(&node->list, &(t_info->main_queue));
	}
}

static int cpe_worker_thread(void *context)
{
	struct cpe_info *t_info = (struct cpe_info *)context;

	if (t_info->cpe_start_notification)
		t_info->cpe_start_notification(t_info);
	else
		pr_debug("%s: no start notification\n",
			 __func__);

	while (!kthread_should_stop()) {
		wait_for_completion(&t_info->cmd_complete);
		cpe_cmd_received(t_info);
		INIT_COMPLETION(t_info->cmd_complete);
	};

	return 0;
}

static void cpe_create_worker_thread(struct cpe_info *t_info)
{
	INIT_LIST_HEAD(&t_info->main_queue);
	init_completion(&t_info->cmd_complete);
	t_info->thread_handler = kthread_run(cpe_worker_thread,
		(void *)t_info, "cpe-worker-thread");
}

static void cpe_cleanup_worker_thread(struct cpe_info *t_info)
{
	if (t_info->thread_handler != NULL)
		kthread_stop(t_info->thread_handler);

	t_info->thread_handler = NULL;
}

static enum cpe_svc_result
cpe_send_cmd_to_thread(struct cpe_info *t_info,
	enum cpe_command command, void *data)
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
	list_add_tail(&(cmd->list), &(t_info->main_queue));

	complete(&t_info->cmd_complete);
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

	if (client->notif.notification)
		client->notif.notification(payload);

	if (client->notif.cmi_notification)
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

	list_for_each_entry(n, &t_info->client_list, list)
		cpe_notify_client(n, payload);
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
	n->name = name;
	list_add_tail(&n->list, &t_info->client_list);
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
	struct cpe_svc_notification notif;
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
	notif.payload = payload;

	list_for_each_entry(n, &t_info->client_list, list) {
		if ((n->mask & CPE_SVC_CMI_MSG) && n->service == service)
			cpe_notify_client(n, &notif);
	}
}

static void cpe_toggle_irq_notification(struct cpe_info *t_info, u32 value)
{
	if (cpe_irq_control_callback)
		cpe_irq_control_callback(value);
}

static void cpe_command_cleanup(struct cpe_command_node *command_node)
{
	switch (command_node->command) {
	case CPE_CMD_SEND_MSG:
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

static void cpe_send_msg_to_inbox(struct cpe_info *t_info,
		struct cpe_send_msg *msg)
{
	size_t bytes = 0;
	size_t inbox_size =
		t_info->tgt->tgt_get_cpe_info()->inbox_size;
	struct cmi_hdr *hdr;

	memset(t_info->tgt->inbox, 0, inbox_size);
	hdr = CMI_GET_HEADER(t_info->tgt->inbox);
	CMI_HDR_SET_SESSION(hdr, 1);
	CMI_HDR_SET_SERVICE(hdr, CMI_CPE_CORE_SERVICE_ID);
	CMI_HDR_SET_VERSION(hdr, CMI_DRIVER_SUPPORTED_VERSION);
	CMI_HDR_SET_OBM(hdr, CMI_OBM_FLAG_IN_BAND);

	switch (t_info->substate) {
	case CPE_SS_BOOT_INIT: {
		struct cmi_core_svc_cmd_shared_mem_alloc *m;
		hdr = CMI_GET_HEADER(t_info->tgt->inbox);
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

	case CPE_SS_DL_COMPLETED:
	case CPE_SS_MSG_SEND_INBOX:
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
	case CPE_SS_MSG_REQUEST_ACCESS:
		hdr = CMI_GET_HEADER(t_info->tgt->inbox);
		CMI_HDR_SET_OPCODE(hdr,
			CPE_CORE_SVC_CMD_DRAM_ACCESS_REQ);
		CMI_HDR_SET_PAYLOAD_SIZE(hdr, 0);
		pr_debug("%s: Creating DRAM acces request message\n",
			 __func__);
		break;

	default:
		pr_err("%s: unhandled substate %d\n",
			__func__, t_info->substate);
		break;
	}

	pr_debug("%s: sending message to cpe inbox\n",
		  __func__);
	bytes = sizeof(struct cmi_hdr);
	hdr = CMI_GET_HEADER(t_info->tgt->inbox);
	bytes += CMI_HDR_GET_PAYLOAD_SIZE(hdr);
	t_info->tgt->tgt_write_mailbox(t_info->tgt->inbox, bytes);
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

	mutex_lock(&cpe_api_mutex);
	switch (irq) {
	case CPE_IRQ_OUTBOX_IRQ:
		size = t_info->tgt->tgt_get_cpe_info()->outbox_size;
		t_info->tgt->tgt_read_mailbox(t_info->tgt->outbox, size);
		break;

	case CPE_IRQ_MEM_ACCESS_ERROR:
		err_irq = true;
		t_info->state = CPE_STATE_OFFLINE;
		t_info->substate = CPE_SS_IDLE;
		break;

	case CPE_IRQ_WDOG_BITE:
		err_irq = true;
		mutex_unlock(&cpe_api_mutex);
		cpe_svc_shutdown(t_info);
		mutex_lock(&cpe_api_mutex);
		break;

	case CPE_IRQ_WCO_WDOG_INT:
	case CPE_IRQ_FLL_LOCK_LOST:
	default:
		err_irq = true;
		break;
	}

	if (err_irq) {
		pr_err("%s: CPE error IRQ %u occured\n",
			__func__, irq);
		mutex_unlock(&cpe_api_mutex);
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
			t_info->substate = CPE_SS_MSG_SEND_INBOX;
			cpe_send_msg_to_inbox(t_info, m);
			break;

		case CPE_SS_MSG_SEND_INBOX:
			temp_node.command = CPE_CMD_SEND_MSG_COMPLETE;
			temp_node.data = m->payload;
			t_info->substate = CPE_SS_MSG_SENT;
			t_info->cpe_process_command(&temp_node);
			kfree(m);
			t_info->pending = NULL;
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
		cpe_notify_cmi_client(t_info,
			t_info->tgt->outbox, CPE_SVC_SUCCESS);
		break;

	default:
		pr_debug("%s: unhandled state %d\n",
			 __func__, t_info->state);
		break;
	}

	mutex_unlock(&cpe_api_mutex);
}


static void broacast_boot_failed(void)
{
	struct cpe_info *t_info = cpe_default_handle;
	struct cpe_svc_notification payload;

	payload.event = CPE_SVC_BOOT_FAILED;
	payload.result = CPE_SVC_FAILED;
	payload.payload = NULL;
	cpe_broadcast_notification(t_info, &payload);
}

static enum cpe_svc_result broadcast_boot_event(
		const struct cpe_info *t_info)
{
	struct cpe_svc_notification payload;

	payload.event = CPE_SVC_ONLINE;
	payload.result = CPE_SVC_SUCCESS;
	payload.payload = cdc_priv;
	cpe_broadcast_notification(t_info, &payload);

	return CPE_SVC_SUCCESS;
}

static bool cpe_mt_process_cmd(struct cpe_command_node *command_node)
{
	bool rc = true;
	struct cpe_info *t_info = cpe_default_handle;
	struct cpe_svc_notification payload;
	struct cmi_core_svc_event_system_boot *ev_boot;
	struct cmi_core_svc_cmdrsp_shared_mem_alloc *rsp_shmem_alloc;
	enum cpe_svc_result cpe_rc;
	struct cpe_send_msg *m;
	struct cmi_hdr *hdr;
	u32 size;
	u8 service = 0;

	if (!t_info || !command_node) {
		pr_err("%s: Invalid handle/command node\n",
			__func__);
		return false;
	}

	pr_debug("%s: cmd = %u\n", __func__, command_node->command);

	cpe_rc = cpe_is_command_valid(t_info, command_node->command);

	if (cpe_rc != CPE_SVC_SUCCESS) {
		pr_err("%s: Invalid command %d, err = %d\n",
			__func__, command_node->command, cpe_rc);
		return false;
	}

	switch (command_node->command) {

	case CPE_CMD_BOOT_INITIALIZE:
		if (CMI_GET_OPCODE(t_info->tgt->outbox) !=
			CPE_CORE_SVC_EVENT_SYSTEM_BOOT) {
			broacast_boot_failed();
			break;
		}

		ev_boot = (struct cmi_core_svc_event_system_boot *)
			CMI_GET_PAYLOAD(t_info->tgt->outbox);

		if (ev_boot->status != CPE_BOOT_SUCCESS) {
			pr_err("%s: cpe boot failed, status = %d\n",
				__func__, ev_boot->status);
			broacast_boot_failed();
			break;
		}

		t_info->substate = CPE_SS_BOOT_INIT;
		cpe_send_msg_to_inbox(t_info, NULL);
		break;

	case CPE_CMD_BOOT_COMPLETE:
		if (CMI_GET_OPCODE(t_info->tgt->outbox) !=
			CPE_CORE_SVC_CMDRSP_SHARED_MEM_ALLOC) {
			broacast_boot_failed();
			break;
		}

		rsp_shmem_alloc =
			(struct cmi_core_svc_cmdrsp_shared_mem_alloc *)
				CMI_GET_PAYLOAD(t_info->tgt->outbox);
		cpe_msg_buffer = rsp_shmem_alloc->addr;

		if (cpe_msg_buffer == 0) {
			pr_err("%s: Invalid cpe buffer for message\n",
				__func__);
			broacast_boot_failed();
		} else {
			pr_debug("%s: boot complete\n", __func__);
			cpe_create_worker_thread(t_info);
		}

		t_info->state = CPE_STATE_IDLE;
		t_info->substate = CPE_SS_IDLE;
		break;

	case CPE_CMD_SEND_MSG:
		m = (struct cpe_send_msg *)command_node->data;
		size = m->size;
		t_info->state = CPE_STATE_SENDING_MSG;

		if (size <= t_info->tgt->tgt_get_cpe_info()->inbox_size) {
			t_info->substate = CPE_SS_MSG_SEND_INBOX;
			pr_debug("%s: msg fits mailbox, size %u\n",
				 __func__, size);
		} else if (size < CPE_MSG_BUFFER_SIZE) {
			t_info->substate = CPE_SS_MSG_REQUEST_ACCESS;
			m->address = cpe_msg_buffer;
			pr_debug("%s: msg is inband\n", __func__);
		} else {
			pr_err("%s: Wrong msg size %u\n",
			       __func__, size);
			t_info->state = CPE_STATE_IDLE;
			cpe_command_cleanup(command_node);
			return CPE_SVC_FAILED;
		}

		t_info->pending = m;
		cpe_send_msg_to_inbox(t_info, m);
		break;

	case CPE_CMD_SEND_MSG_COMPLETE:
		hdr = CMI_GET_HEADER(t_info->tgt->outbox);
		service = CMI_HDR_GET_SERVICE(hdr);
		pr_debug("%s: msg send success, notifying clients\n",
			 __func__);
		cpe_notify_cmi_client(t_info,
			t_info->tgt->outbox, CPE_SVC_SUCCESS);
		cpe_command_cleanup(command_node);
		t_info->state = CPE_STATE_IDLE;
		break;

	case CPE_CMD_KILL_THREAD:

		if (t_info->pending) {
			struct cpe_send_msg *m =
				(struct cpe_send_msg *)t_info->pending;
			cpe_notify_cmi_client(t_info, m->payload,
				CPE_SVC_SHUTTING_DOWN);
			kfree(t_info->pending);
			t_info->pending = NULL;
		}

		cpe_command_cleanup(command_node);
		kfree(command_node);
		payload.result = CPE_SVC_SHUTTING_DOWN;
		payload.event = CPE_SVC_OFFLINE;
		payload.payload = cdc_priv;
		cpe_broadcast_notification(t_info, &payload);
		t_info->state = CPE_STATE_OFFLINE;
		t_info->substate = CPE_SS_IDLE;
		cpe_cleanup_worker_thread(t_info);
		break;

	default:
		pr_err("%s: unhandled cpe cmd = %d\n",
			__func__, command_node->command);
		break;
	}

	return rc;
}

static enum cpe_svc_result cpe_mt_validate_cmd(
		const struct cpe_info *t_info,
		enum cpe_command command)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;

	if (t_info && t_info->initialized == false) {
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
		case CPE_CMD_PROCESS_IRQ:
		case CPE_CMD_KILL_THREAD:
		case CPE_CMD_DEINITIALIZE:
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
		default:
			rc = CPE_SVC_NOT_READY;
			break;
		}
		break;

	case CPE_STATE_IDLE:
	case CPE_STATE_SENDING_MSG:
		switch (command) {
		case CPE_CMD_SEND_MSG:
		case CPE_CMD_SEND_MSG_COMPLETE:
		case CPE_CMD_PROCESS_IRQ:
		case CPE_CMD_SHUTDOWN:
		case CPE_CMD_KILL_THREAD:
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

	if (cpe_default_handle && cpe_default_handle->initialized == true)
		return (void *)cpe_default_handle;

	if (!cpe_default_handle) {
		cpe_default_handle = kzalloc(sizeof(struct cpe_info),
					     GFP_KERNEL);
		if (!cpe_default_handle) {
			pr_err("%s: no memory for cpe handle, size = %zu\n",
				__func__, sizeof(struct cpe_info));
			goto err_register;
		}

		memset(cpe_default_handle, 0,
		       sizeof(struct cpe_info));
	}

	t_info = cpe_default_handle;

	INIT_LIST_HEAD(&t_info->client_list);
	cdc_priv = context;

	t_info->tgt = kzalloc(sizeof(struct cpe_svc_tgt_abstraction),
			      GFP_KERNEL);
	if (!t_info->tgt) {
		pr_err("%s: target allocation failed, size = %zu\n",
			__func__,
			sizeof(struct cpe_svc_tgt_abstraction));
		goto err_tgt_alloc;
	}

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
	cpe_irq_control_callback = irq_control_callback;
	t_info->cpe_process_command = cpe_mt_process_cmd;
	t_info->cpe_cmd_validate = cpe_mt_validate_cmd;
	t_info->cpe_start_notification = broadcast_boot_event;
	mutex_init(&cpe_api_mutex);
	pr_debug("%s: cpe services initialized\n", __func__);
	t_info->state = CPE_STATE_INITIALIZED;
	t_info->initialized = true;

	return t_info;

err_tgt_init:
	kfree(t_info->tgt);

err_tgt_alloc:
	kfree(cpe_default_handle);
	cpe_default_handle = NULL;

err_register:
	return NULL;
}

enum cpe_svc_result cpe_svc_deinitialize(void *cpe_handle)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	if (!t_info)
		t_info = cpe_default_handle;

	rc = cpe_is_command_valid(t_info, CPE_CMD_DEINITIALIZE);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: Invalid command %d\n",
			__func__, CPE_CMD_DEINITIALIZE);
		return rc;
	}

	if (cpe_default_handle == t_info)
		cpe_default_handle = NULL;

	t_info->tgt->tgt_deinit(t_info->tgt);
	kfree(t_info->tgt);
	kfree(t_info);
	mutex_destroy(&cpe_api_mutex);

	return rc;
}

void *cpe_svc_register(void *cpe_handle,
		void (*notification_callback)
			(const struct cpe_svc_notification *parameter),
		u32 mask, const char *name)
{
	if (!cpe_default_handle) {
		cpe_default_handle = kzalloc(sizeof(struct cpe_info),
					     GFP_KERNEL);
		if (!cpe_default_handle) {
			pr_err("%s: no_mem for cpe handle, sz = %zu\n",
				__func__, sizeof(struct cpe_info));
			return NULL;
		}

		memset(cpe_default_handle, 0,
			sizeof(struct cpe_info));
	}

	if (!cpe_handle)
		cpe_handle = cpe_default_handle;

	return cpe_register_generic((struct cpe_info *)cpe_handle,
		notification_callback,
		NULL,
		mask, CPE_NO_SERVICE, name);
}

enum cpe_svc_result cpe_svc_deregister(void *cpe_handle, void *reg_handle)
{
	if (!cpe_handle)
		cpe_handle = cpe_default_handle;

	return cpe_deregister_generic((struct cpe_info *)cpe_handle,
		reg_handle);
}

enum cpe_svc_result cpe_svc_download_segment(void *cpe_handle,
	const struct cpe_svc_mem_segment *segment)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	if (!t_info)
		t_info = cpe_default_handle;

	rc = cpe_is_command_valid(t_info, CPE_CMD_DL_SEGMENT);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: cmd validation fail, cmd = %d\n",
			__func__, CPE_CMD_DL_SEGMENT);
		return rc;
	}

	mutex_lock(&cpe_api_mutex);
	cpe_toggle_irq_notification(t_info, false);
	t_info->state = CPE_STATE_DOWNLOADING;
	t_info->substate = CPE_SS_DL_DOWNLOADING;
	rc = t_info->tgt->tgt_write_ram(t_info, segment);
	cpe_toggle_irq_notification(t_info, true);
	mutex_unlock(&cpe_api_mutex);

	return rc;
}

enum cpe_svc_result cpe_svc_boot(void *cpe_handle, int debug_mode)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	if (!t_info)
		t_info = cpe_default_handle;

	rc = cpe_is_command_valid(t_info, CPE_CMD_BOOT);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: cmd validation fail, cmd = %d\n",
			__func__, CPE_CMD_BOOT);
		return rc;
	}

	mutex_lock(&cpe_api_mutex);

	if (rc == CPE_SVC_SUCCESS) {
		t_info->tgt->tgt_boot(debug_mode);
		t_info->state = CPE_STATE_BOOTING;
		t_info->substate = CPE_SS_BOOT;
		pr_debug("%s: cpe service booting\n",
			 __func__);
	}

	mutex_unlock(&cpe_api_mutex);
	return rc;
}

enum cpe_svc_result cpe_svc_process_irq(void *cpe_handle, u32 cpe_irq)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	if (!t_info)
		t_info = cpe_default_handle;

	cpe_toggle_irq_notification(t_info, false);
	cpe_process_irq_int(cpe_irq, t_info);
	cpe_toggle_irq_notification(t_info, true);

	return rc;
}

enum cpe_svc_result cpe_svc_route_notification(void *cpe_handle,
		enum cpe_svc_module module, enum cpe_svc_route_dest dest)
{
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	if (!t_info)
		t_info = cpe_default_handle;

	if (t_info->tgt)
		return t_info->tgt->tgt_route_notification(module, dest);

	return CPE_SVC_NOT_READY;
}

enum cpe_svc_result cpe_svc_shutdown(void *cpe_handle)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;
	struct cpe_command_node *n = NULL;

	if (!t_info)
		t_info = cpe_default_handle;

	rc = cpe_is_command_valid(t_info, CPE_CMD_SHUTDOWN);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: cmd validation fail, cmd = %d\n",
			__func__, CPE_CMD_SHUTDOWN);
		return rc;
	}

	mutex_lock(&cpe_api_mutex);

	while (!list_empty(&t_info->main_queue)) {
		n = list_first_entry(&t_info->main_queue,
				     struct cpe_command_node, list);

		if (n->command == CPE_CMD_SEND_MSG) {
			cpe_notify_cmi_client(t_info, (u8 *)n->data,
				CPE_SVC_SHUTTING_DOWN);
		}

		cpe_command_cleanup(n);
		kfree(n);
	}

	pr_debug("%s: cpe service OFFLINE state\n", __func__);

	t_info->state = CPE_STATE_OFFLINE;
	t_info->substate = CPE_SS_IDLE;

	rc = cpe_send_cmd_to_thread(t_info, CPE_CMD_KILL_THREAD, NULL);
	mutex_unlock(&cpe_api_mutex);

	return rc;
}

enum cpe_svc_result cpe_svc_reset(void *cpe_handle)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	if (!t_info)
		t_info = cpe_default_handle;

	rc = cpe_is_command_valid(t_info, CPE_CMD_RESET);

	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: cmd validation fail, cmd = %d\n",
			__func__, CPE_CMD_RESET);
		return rc;
	}

	if (t_info && t_info->tgt) {
		mutex_lock(&cpe_api_mutex);
		rc = t_info->tgt->tgt_reset();
		pr_debug("%s: cpe services in INITIALIZED state\n",
			 __func__);
		t_info->state = CPE_STATE_INITIALIZED;
		t_info->substate = CPE_SS_IDLE;
		mutex_unlock(&cpe_api_mutex);
	}

	return rc;
}

enum cpe_svc_result cpe_svc_ramdump(void *cpe_handle,
		struct cpe_svc_mem_segment *buffer)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	if (!t_info)
		t_info = cpe_default_handle;

	rc = cpe_is_command_valid(t_info, CPE_CMD_RAMDUMP);
	if (rc != CPE_SVC_SUCCESS) {
		pr_err("%s: cmd validation fail, cmd = %d\n",
			__func__, CPE_CMD_RAMDUMP);
		return rc;
	}

	if (t_info->tgt) {
		rc = t_info->tgt->tgt_read_ram(t_info, buffer);
	} else {
		pr_err("%s: cpe service not ready\n", __func__);
		return CPE_SVC_NOT_READY;
	}

	return rc;
}

enum cpe_svc_result cpe_svc_set_debug_mode(void *cpe_handle, u32 mode)
{
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	if (!t_info)
		t_info = cpe_default_handle;

	if (t_info->tgt)
		return t_info->tgt->tgt_set_debug_mode(mode);

	else
		return CPE_SVC_INVALID_HANDLE;
}

const struct cpe_svc_hw_cfg *cpe_svc_get_hw_cfg(void *cpe_handle)
{
	struct cpe_info *t_info = (struct cpe_info *)cpe_handle;

	if (!t_info)
		t_info = cpe_default_handle;

	if (t_info->tgt)
		return t_info->tgt->tgt_get_cpe_info();
	else
		return NULL;
}

void *cmi_register(
		void notification_callback(
			const struct cmi_api_notification *parameter),
		u32 service)
{
	return cpe_register_generic(cpe_default_handle,
		NULL,
		notification_callback,
		CPE_SVC_CMI_MSG | CPE_SVC_OFFLINE | CPE_SVC_ONLINE,
		service,
		"CMI_CLIENT");
}

enum cmi_api_result cmi_deregister(void *reg_handle)
{
	u32 clients = 0;
	struct cpe_notif_node *n = NULL;
	enum cmi_api_result rc = CMI_API_SUCCESS;
	struct cpe_svc_notification payload;

	rc = (enum cmi_api_result) cpe_deregister_generic(
		cpe_default_handle, reg_handle);

	list_for_each_entry(n, &cpe_default_handle->client_list, list) {
		if (n->mask & CPE_SVC_CMI_MSG)
			clients++;
	}

	if (clients == 0) {
		payload.event = CPE_SVC_CMI_CLIENTS_DEREG;
		payload.payload = NULL;
		payload.result = CPE_SVC_SUCCESS;
		cpe_broadcast_notification(cpe_default_handle, &payload);
	}

	return rc;
}

enum cmi_api_result cmi_send_msg(void *message)
{
	enum cmi_api_result rc = CMI_API_SUCCESS;
	struct cpe_send_msg *msg = NULL;
	struct cmi_hdr *hdr;

	hdr = CMI_GET_HEADER(message);
	msg = kzalloc(sizeof(struct cpe_send_msg),
		      GFP_ATOMIC);
	if (!msg) {
		pr_err("%s: no memory for cmi msg, sz = %zu\n",
			__func__, sizeof(struct cpe_send_msg));
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
		pr_err("%s: no memory for cmi payload, sz = %d\n",
			__func__, msg->size);
		kfree(msg);
		return CPE_SVC_NO_MEMORY;
	}

	msg->address = 0;
	memcpy((void *)msg->payload, message, msg->size);

	rc = (enum cmi_api_result)cpe_send_cmd_to_thread(cpe_default_handle,
		CPE_CMD_SEND_MSG, (void *)msg);

	if (rc != 0) {
		pr_err("%s: Failed to queue message\n", __func__);
		kfree(msg->payload);
		kfree(msg);
	}

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
			     0x0C, 0x04);
	rc = cpe_update_bits(TOMTOM_A_SVASS_CPAR_CFG,
			     0x01, 0x01);

	return rc;
}

static u32 cpe_tgt_tomtom_is_cpar_init_done(void)
{
	u8 status = 0;
	cpe_register_read(TOMTOM_A_SVASS_STATUS, &status);
	status = status & 0x01;
	return status & 0x01;
}

static u32 cpe_tgt_tomtom_is_active(void)
{
	u8 status = 0;
	cpe_register_read(TOMTOM_A_SVASS_STATUS, &status);
	status = status & 0x04;
	return status;
}

static enum cpe_svc_result cpe_tgt_tomtom_reset(void)
{
	enum cpe_svc_result rc = CPE_SVC_SUCCESS;

	rc = cpe_update_bits(TOMTOM_A_SVASS_CPAR_CFG,
			     0x01, 0x00);
	rc = cpe_update_bits(TOMTOM_A_MEM_LEAKAGE_CTL,
			     0x07, 0x03);
	rc = cpe_update_bits(TOMTOM_A_SVASS_CLKRST_CTL,
			     0x08, 0x08);
	return rc;
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
		pr_err("%s: wrong size %u, start adress %x, mem_type %u\n",
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
		if (autoinc || ptr_update) {
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

MODULE_DESCRIPTION("WCD CPE Services");
MODULE_LICENSE("GPL v2");
