/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include "linux/ipa.h"
#include "linux/rndis_ipa.h"
#include "linux/ecm_ipa.h"
#include "ipa_i.h"
#include <linux/mutex.h>

#define IPA_USB_RM_TIMEOUT_MSEC 10000
#define IPA_USB_DEV_READY_TIMEOUT_MSEC 10000

#define IPA_HOLB_TMR_EN 0x1

/* GSI channels weights */
#define IPA_USB_DL_CHAN_LOW_WEIGHT 0x5
#define IPA_USB_UL_CHAN_LOW_WEIGHT 0x4

#define IPA_USB_DRV_NAME "ipa_usb"
#define IPA_USB_DBG(fmt, args...) \
	pr_debug(IPA_USB_DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)
#define IPA_USB_ERR(fmt, args...) \
	pr_err(IPA_USB_DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)

typedef void (*ipa_usb_callback)(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data);

enum ipa3_usb_teth_prot_state {
	IPA_USB_TETH_PROT_INITIALIZED,
	IPA_USB_TETH_PROT_CONNECTED,
	IPA_USB_TETH_PROT_INVALID
};

struct ipa3_usb_teth_prot_context {
	union {
		struct ipa_usb_init_params rndis;
		struct ecm_ipa_params ecm;
		struct teth_bridge_init_params teth_bridge;
	} teth_prot_params;
	enum ipa3_usb_teth_prot_state state;
	void *user_data;
};

enum ipa3_usb_cons_state {
	IPA_USB_CONS_GRANTED,
	IPA_USB_CONS_RELEASED
};

struct ipa3_usb_rm_context {
	struct ipa_rm_create_params prod_params;
	struct ipa_rm_create_params cons_params;
	bool prod_valid;
	bool cons_valid;
	struct completion prod_comp;
	struct completion cons_comp;
	enum ipa3_usb_cons_state cons_state;
	/* consumer was requested*/
	bool cons_requested;
	/* consumer was requested and released before it was granted*/
	bool cons_requested_released;
};

enum ipa3_usb_state {
	IPA_USB_INVALID,
	IPA_USB_INITIALIZED,
	IPA_USB_CONNECTED,
	IPA_USB_STOPPED,
	IPA_USB_SUSPEND_REQUESTED,
	IPA_USB_SUSPEND_IN_PROGRESS,
	IPA_USB_SUSPENDED,
	IPA_USB_RESUME_IN_PROGRESS
};

struct finish_suspend_work_context {
	struct work_struct work;
	u32 dl_clnt_hdl;
	u32 ul_clnt_hdl;
};

struct ipa3_usb_context {
	struct ipa3_usb_teth_prot_context
		teth_prot_ctx[IPA_USB_MAX_TETH_PROT_SIZE];
	int num_init_prot;
	enum ipa3_usb_teth_prot_state teth_bridge_state;
	struct teth_bridge_init_params teth_bridge_params;
	struct completion dev_ready_comp;
	struct ipa3_usb_rm_context rm_ctx;
	int (*ipa_usb_notify_cb)(enum ipa_usb_notify_event, void *user_data);
	void *user_data;
	void *diag_user_data;
	u32 qmi_req_id;
	enum ipa3_usb_state state;
	enum ipa3_usb_state diag_state;
	spinlock_t state_lock;
	bool dl_data_pending;
	struct workqueue_struct *wq;
	struct finish_suspend_work_context finish_suspend_work;
	struct mutex general_mutex;
};

enum ipa3_usb_op {
	IPA_USB_INIT_TETH_PROT,
	IPA_USB_REQUEST_CHANNEL,
	IPA_USB_CONNECT,
	IPA_USB_DISCONNECT,
	IPA_USB_RELEASE_CHANNEL,
	IPA_USB_DEINIT_TETH_PROT,
	IPA_USB_SUSPEND,
	IPA_USB_RESUME
};

static void ipa3_usb_wq_notify_remote_wakeup(struct work_struct *work);
static void ipa3_usb_wq_notify_suspend_completed(struct work_struct *work);
static DECLARE_WORK(ipa3_usb_notify_remote_wakeup_work,
	ipa3_usb_wq_notify_remote_wakeup);
static DECLARE_WORK(ipa3_usb_notify_suspend_completed_work,
	ipa3_usb_wq_notify_suspend_completed);

struct ipa3_usb_context *ipa3_usb_ctx;

static char *ipa3_usb_state_to_string(enum ipa3_usb_state state)
{
	switch (state) {
	case IPA_USB_INVALID:
		return "IPA_USB_INVALID";
	case IPA_USB_INITIALIZED:
		return "IPA_USB_INITIALIZED";
	case IPA_USB_CONNECTED:
		return "IPA_USB_CONNECTED";
	case IPA_USB_STOPPED:
		return "IPA_USB_STOPPED";
	case IPA_USB_SUSPEND_REQUESTED:
		return "IPA_USB_SUSPEND_REQUESTED";
	case IPA_USB_SUSPEND_IN_PROGRESS:
		return "IPA_USB_SUSPEND_IN_PROGRESS";
	case IPA_USB_SUSPENDED:
		return "IPA_USB_SUSPENDED";
	case IPA_USB_RESUME_IN_PROGRESS:
		return "IPA_USB_RESUME_IN_PROGRESS";
	default:
		return "UNSUPPORTED";
	}

	return NULL;
}

static bool ipa3_usb_set_state(enum ipa3_usb_state new_state)
{
	unsigned long flags;
	int state_legal = false;

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	switch (new_state) {
	case IPA_USB_INVALID:
		if (ipa3_usb_ctx->state == IPA_USB_INITIALIZED)
			state_legal = true;
		break;
	case IPA_USB_INITIALIZED:
		if (ipa3_usb_ctx->state == IPA_USB_STOPPED ||
			ipa3_usb_ctx->state == IPA_USB_INVALID ||
			ipa3_usb_ctx->state == IPA_USB_INITIALIZED)
			state_legal = true;
		break;
	case IPA_USB_CONNECTED:
		if (ipa3_usb_ctx->state == IPA_USB_INITIALIZED ||
			ipa3_usb_ctx->state == IPA_USB_STOPPED ||
			ipa3_usb_ctx->state == IPA_USB_RESUME_IN_PROGRESS)
			state_legal = true;
		if (ipa3_usb_ctx->rm_ctx.cons_state == IPA_USB_CONS_GRANTED ||
			ipa3_usb_ctx->rm_ctx.cons_requested_released) {
			ipa3_usb_ctx->rm_ctx.cons_requested = false;
			ipa3_usb_ctx->rm_ctx.cons_requested_released = false;
		}
		/* Notify RM that consumer is granted */
		if (ipa3_usb_ctx->rm_ctx.cons_requested) {
			ipa3_rm_notify_completion(IPA_RM_RESOURCE_GRANTED,
				ipa3_usb_ctx->rm_ctx.cons_params.name);
			ipa3_usb_ctx->rm_ctx.cons_state = IPA_USB_CONS_GRANTED;
			ipa3_usb_ctx->rm_ctx.cons_requested = false;
		}
		break;
	case IPA_USB_STOPPED:
		if (ipa3_usb_ctx->state == IPA_USB_SUSPEND_IN_PROGRESS ||
			ipa3_usb_ctx->state == IPA_USB_CONNECTED ||
			ipa3_usb_ctx->state == IPA_USB_SUSPENDED)
			state_legal = true;
		break;
	case IPA_USB_SUSPEND_REQUESTED:
		if (ipa3_usb_ctx->state == IPA_USB_CONNECTED)
			state_legal = true;
		break;
	case IPA_USB_SUSPEND_IN_PROGRESS:
		if (ipa3_usb_ctx->state == IPA_USB_SUSPEND_REQUESTED)
			state_legal = true;
		break;
	case IPA_USB_SUSPENDED:
		if (ipa3_usb_ctx->state == IPA_USB_SUSPEND_REQUESTED ||
			ipa3_usb_ctx->state == IPA_USB_SUSPEND_IN_PROGRESS)
			state_legal = true;
		break;
	case IPA_USB_RESUME_IN_PROGRESS:
		if (ipa3_usb_ctx->state == IPA_USB_SUSPEND_IN_PROGRESS ||
			ipa3_usb_ctx->state == IPA_USB_SUSPENDED)
			state_legal = true;
		break;
	default:
		state_legal = false;
		break;
	}
	if (state_legal) {
		if (ipa3_usb_ctx->state != new_state) {
			IPA_USB_DBG("ipa_usb state changed %s -> %s\n",
				ipa3_usb_state_to_string(ipa3_usb_ctx->state),
				ipa3_usb_state_to_string(new_state));
			ipa3_usb_ctx->state = new_state;
		}
	} else {
		IPA_USB_ERR("invalid state change %s -> %s\n",
			ipa3_usb_state_to_string(ipa3_usb_ctx->state),
			ipa3_usb_state_to_string(new_state));
	}
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
	return state_legal;
}

static bool ipa3_usb_set_diag_state(enum ipa3_usb_state new_state)
{
	unsigned long flags;
	int state_legal = false;

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	IPA_USB_DBG("ipa3_usb_set_diag_state: current diag state = %s\n",
		ipa3_usb_state_to_string(ipa3_usb_ctx->diag_state));
	switch (new_state) {
	case IPA_USB_INVALID:
		if (ipa3_usb_ctx->diag_state == IPA_USB_INITIALIZED)
			state_legal = true;
		break;
	case IPA_USB_INITIALIZED:
		if (ipa3_usb_ctx->diag_state == IPA_USB_INVALID ||
			ipa3_usb_ctx->diag_state == IPA_USB_STOPPED ||
			ipa3_usb_ctx->diag_state == IPA_USB_INITIALIZED)
			state_legal = true;
		break;
	case IPA_USB_CONNECTED:
		if (ipa3_usb_ctx->diag_state == IPA_USB_INITIALIZED ||
			ipa3_usb_ctx->diag_state == IPA_USB_STOPPED)
			state_legal = true;
		break;
	case IPA_USB_STOPPED:
		if (ipa3_usb_ctx->diag_state == IPA_USB_CONNECTED)
			state_legal = true;
		break;
	default:
		state_legal = false;
		break;
	}
	if (state_legal) {
		if (ipa3_usb_ctx->diag_state != new_state) {
			IPA_USB_DBG("DIAG state changed %s -> %s\n",
				ipa3_usb_state_to_string(
					ipa3_usb_ctx->diag_state),
				ipa3_usb_state_to_string(new_state));
			ipa3_usb_ctx->diag_state = new_state;
		}
	} else {
		IPA_USB_ERR("invalid DIAG state change %s -> %s\n",
			ipa3_usb_state_to_string(ipa3_usb_ctx->diag_state),
			ipa3_usb_state_to_string(new_state));
	}
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
	return state_legal;
}

static bool ipa3_usb_check_legal_op(enum ipa3_usb_op op, bool is_diag)
{
	unsigned long flags;
	bool is_legal = false;

	if (ipa3_usb_ctx == NULL) {
		IPA_USB_ERR("ipa_usb_ctx is not initialized!\n");
		return false;
	}

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	switch (op) {
	case IPA_USB_INIT_TETH_PROT:
		if (!is_diag && (ipa3_usb_ctx->state == IPA_USB_INVALID ||
			ipa3_usb_ctx->state == IPA_USB_INITIALIZED))
			is_legal = true;
		if (is_diag && ipa3_usb_ctx->diag_state == IPA_USB_INVALID)
			is_legal = true;
		break;
	case IPA_USB_REQUEST_CHANNEL:
		if (!is_diag && ipa3_usb_ctx->state == IPA_USB_INITIALIZED)
			is_legal = true;
		if (is_diag && ipa3_usb_ctx->diag_state == IPA_USB_INITIALIZED)
			is_legal = true;
		break;
	case IPA_USB_CONNECT:
		if (!is_diag && (ipa3_usb_ctx->state == IPA_USB_INITIALIZED ||
			ipa3_usb_ctx->state == IPA_USB_STOPPED))
			is_legal = true;
		if (is_diag && ipa3_usb_ctx->state == IPA_USB_CONNECTED &&
			(ipa3_usb_ctx->diag_state == IPA_USB_INITIALIZED
			|| ipa3_usb_ctx->diag_state == IPA_USB_STOPPED))
			is_legal = true;
		break;
	case IPA_USB_DISCONNECT:
		if (!is_diag && (ipa3_usb_ctx->state == IPA_USB_CONNECTED ||
			ipa3_usb_ctx->state == IPA_USB_SUSPEND_IN_PROGRESS ||
			ipa3_usb_ctx->state == IPA_USB_SUSPENDED))
			is_legal = true;
		if (is_diag &&
			ipa3_usb_ctx->diag_state == IPA_USB_CONNECTED)
			is_legal = true;
		break;
	case IPA_USB_RELEASE_CHANNEL:
		if (!is_diag && (ipa3_usb_ctx->state == IPA_USB_STOPPED ||
			ipa3_usb_ctx->state == IPA_USB_INITIALIZED))
			is_legal = true;
		if (is_diag &&
			(ipa3_usb_ctx->diag_state == IPA_USB_STOPPED ||
			ipa3_usb_ctx->diag_state == IPA_USB_INITIALIZED))
			is_legal = true;
		break;
	case IPA_USB_DEINIT_TETH_PROT:
		if (!is_diag && ipa3_usb_ctx->state == IPA_USB_INITIALIZED)
			is_legal = true;
		if (is_diag && ipa3_usb_ctx->diag_state == IPA_USB_INITIALIZED)
			is_legal = true;
		break;
	case IPA_USB_SUSPEND:
		if (ipa3_usb_ctx->state == IPA_USB_CONNECTED &&
			ipa3_usb_ctx->diag_state != IPA_USB_CONNECTED)
			is_legal = true;
		break;
	case IPA_USB_RESUME:
		if (ipa3_usb_ctx->state == IPA_USB_SUSPENDED ||
			ipa3_usb_ctx->state == IPA_USB_SUSPEND_IN_PROGRESS)
			is_legal = true;
		break;
	default:
		is_legal = false;
		break;
	}

	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
	return is_legal;
}

int ipa3_usb_init(void)
{
	int i;
	unsigned long flags;

	IPA_USB_DBG("ipa3_usb_init: entry\n");
	ipa3_usb_ctx = kzalloc(sizeof(struct ipa3_usb_context), GFP_KERNEL);
	if (ipa3_usb_ctx == NULL) {
		IPA_USB_ERR("failed to allocate memory\n");
		return -EFAULT;
	}

	memset(ipa3_usb_ctx, 0, sizeof(struct ipa3_usb_context));

	for (i = 0; i < IPA_USB_MAX_TETH_PROT_SIZE; i++)
		ipa3_usb_ctx->teth_prot_ctx[i].state =
			IPA_USB_TETH_PROT_INVALID;
	ipa3_usb_ctx->num_init_prot = 0;
	ipa3_usb_ctx->teth_bridge_state = IPA_USB_TETH_PROT_INVALID;
	init_completion(&ipa3_usb_ctx->dev_ready_comp);

	ipa3_usb_ctx->rm_ctx.prod_valid = false;
	ipa3_usb_ctx->rm_ctx.cons_valid = false;
	init_completion(&ipa3_usb_ctx->rm_ctx.prod_comp);
	init_completion(&ipa3_usb_ctx->rm_ctx.cons_comp);

	ipa3_usb_ctx->qmi_req_id = 0;
	spin_lock_init(&ipa3_usb_ctx->state_lock);
	ipa3_usb_ctx->dl_data_pending = false;
	mutex_init(&ipa3_usb_ctx->general_mutex);
	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	ipa3_usb_ctx->state = IPA_USB_INVALID;
	ipa3_usb_ctx->diag_state = IPA_USB_INVALID;
	ipa3_usb_ctx->rm_ctx.cons_state = IPA_USB_CONS_RELEASED;
	ipa3_usb_ctx->user_data = NULL;
	ipa3_usb_ctx->diag_user_data = NULL;
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);

	ipa3_usb_ctx->wq = create_singlethread_workqueue("ipa_usb_wq");
	if (!ipa3_usb_ctx->wq) {
		IPA_USB_ERR("failed to create workqueue\n");
		kfree(ipa3_usb_ctx);
		return -EFAULT;
	}

	IPA_USB_DBG("ipa3_usb_init: exit\n");

	return 0;
}

static void ipa3_usb_notify_device_ready(void *user_data)
{
	if (ipa3_usb_ctx->ipa_usb_notify_cb) {
		ipa3_usb_ctx->ipa_usb_notify_cb(IPA_USB_DEVICE_READY,
			user_data);
		IPA_USB_DBG("invoked device_ready CB\n");
	}
}

void ipa3_usb_device_ready_notify_cb(void)
{
	IPA_USB_DBG("ipa3_usb_device_ready_notify_cb: entry\n");
	ipa3_usb_notify_device_ready(ipa3_usb_ctx->user_data);
	IPA_USB_DBG("ipa3_usb_device_ready_notify_cb: exit\n");
}

void ipa3_usb_prod_notify_cb(void *user_data, enum ipa_rm_event event,
			     unsigned long data)
{
	IPA_USB_DBG("ipa3_usb_prod_notify_cb: entry\n");
	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		IPA_USB_DBG(":USB_PROD granted\n");
		complete_all(&ipa3_usb_ctx->rm_ctx.prod_comp);
		break;
	case IPA_RM_RESOURCE_RELEASED:
		IPA_USB_DBG(":USB_PROD released\n");
		complete_all(&ipa3_usb_ctx->rm_ctx.prod_comp);
		break;
	}
	IPA_USB_DBG("ipa3_usb_prod_notify_cb: exit\n");
}

static void ipa3_usb_wq_notify_remote_wakeup(struct work_struct *work)
{
	ipa3_usb_ctx->ipa_usb_notify_cb(IPA_USB_REMOTE_WAKEUP,
		ipa3_usb_ctx->user_data);
	IPA_USB_DBG("invoked remote wakeup event\n");
}

static void ipa3_usb_wq_notify_suspend_completed(struct work_struct *work)
{
	ipa3_usb_ctx->ipa_usb_notify_cb(IPA_USB_SUSPEND_COMPLETED,
		ipa3_usb_ctx->user_data);
	IPA_USB_DBG("invoked suspend completed event\n");
}

static void ipa3_usb_wq_finish_suspend_work(struct work_struct *work)
{
	struct finish_suspend_work_context *finish_suspend_work_ctx;
	unsigned long flags;
	int result = -EFAULT;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG("ipa3_usb_wq_finish_suspend_work: entry\n");
	finish_suspend_work_ctx = container_of(work,
		struct finish_suspend_work_context, work);

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	if (ipa3_usb_ctx->state != IPA_USB_SUSPEND_IN_PROGRESS) {
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
		mutex_unlock(&ipa3_usb_ctx->general_mutex);
		return;
	}
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);

	/* Stop DL channel */
	result = ipa3_stop_gsi_channel(finish_suspend_work_ctx->dl_clnt_hdl);
	if (result) {
		IPAERR("Error stopping DL channel: %d, resuming channel\n",
			result);
		ipa3_xdci_resume(finish_suspend_work_ctx->ul_clnt_hdl,
			finish_suspend_work_ctx->dl_clnt_hdl);
		/* Change ipa_usb state back to CONNECTED */
		if (!ipa3_usb_set_state(IPA_USB_CONNECTED))
			IPA_USB_ERR("failed to change state to connected\n");
		queue_work(ipa3_usb_ctx->wq,
			&ipa3_usb_notify_remote_wakeup_work);
		mutex_unlock(&ipa3_usb_ctx->general_mutex);
		return;
	}

	/* Change ipa_usb state to SUSPENDED */
	if (!ipa3_usb_set_state(IPA_USB_SUSPENDED))
		IPA_USB_ERR("failed to change state to suspended\n");

	queue_work(ipa3_usb_ctx->wq,
		&ipa3_usb_notify_suspend_completed_work);

	IPA_USB_DBG("ipa3_usb_wq_finish_suspend_work: exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
}

int ipa3_usb_cons_request_resource_cb(void)
{
	unsigned long flags;
	int result;

	IPA_USB_DBG("ipa3_usb_cons_request_resource_cb: entry\n");
	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	switch (ipa3_usb_ctx->state) {
	case IPA_USB_CONNECTED:
		ipa3_usb_ctx->rm_ctx.cons_state = IPA_USB_CONS_GRANTED;
		result = 0;
		break;
	case IPA_USB_SUSPEND_REQUESTED:
		ipa3_usb_ctx->rm_ctx.cons_requested = true;
		if (ipa3_usb_ctx->rm_ctx.cons_state == IPA_USB_CONS_GRANTED)
			result = 0;
		else
			result = -EINPROGRESS;
		break;
	case IPA_USB_SUSPEND_IN_PROGRESS:
		ipa3_usb_ctx->rm_ctx.cons_requested = true;
		queue_work(ipa3_usb_ctx->wq,
			&ipa3_usb_notify_remote_wakeup_work);
		result = 0;
		break;
	case IPA_USB_SUSPENDED:
		ipa3_usb_ctx->rm_ctx.cons_requested = true;
		queue_work(ipa3_usb_ctx->wq,
			&ipa3_usb_notify_remote_wakeup_work);
		result = -EINPROGRESS;
		break;
	default:
		ipa3_usb_ctx->rm_ctx.cons_requested = true;
		result = -EINPROGRESS;
		break;
	}
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
	IPA_USB_DBG("ipa3_usb_cons_request_resource_cb: exit with %d\n",
		result);
	return result;
}

int ipa3_usb_cons_release_resource_cb(void)
{
	unsigned long flags;

	IPA_USB_DBG("ipa3_usb_cons_release_resource_cb: entry\n");
	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	switch (ipa3_usb_ctx->state) {
	case IPA_USB_SUSPEND_IN_PROGRESS:
		if (ipa3_usb_ctx->rm_ctx.cons_requested)
			ipa3_usb_ctx->rm_ctx.cons_requested_released = true;
		else {
			queue_work(ipa3_usb_ctx->wq,
				&ipa3_usb_ctx->finish_suspend_work.work);
		}
		break;
	case IPA_USB_SUSPEND_REQUESTED:
		if (ipa3_usb_ctx->rm_ctx.cons_requested)
			ipa3_usb_ctx->rm_ctx.cons_requested_released = true;
		break;
	case IPA_USB_STOPPED:
	case IPA_USB_RESUME_IN_PROGRESS:
		if (ipa3_usb_ctx->rm_ctx.cons_requested)
			ipa3_usb_ctx->rm_ctx.cons_requested = false;
		break;
	case IPA_USB_CONNECTED:
		break;
	default:
		IPA_USB_ERR("received cons_release_cb in bad state: %s!\n",
			ipa3_usb_state_to_string(ipa3_usb_ctx->state));
		WARN_ON(1);
		break;
	}

	ipa3_usb_ctx->rm_ctx.cons_state = IPA_USB_CONS_RELEASED;
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
	IPA_USB_DBG("ipa3_usb_cons_release_resource_cb: exit\n");
	return 0;
}

static char *ipa3_usb_teth_prot_to_string(enum ipa_usb_teth_prot teth_prot)
{
	switch (teth_prot) {
	case IPA_USB_RNDIS:
		return "rndis_ipa";
	case IPA_USB_ECM:
		return "ecm_ipa";
	case IPA_USB_RMNET:
	case IPA_USB_MBIM:
		return "teth_bridge";
	case IPA_USB_DIAG:
		return "diag";
	default:
		return "unsupported";
	}

	return NULL;
}

static int ipa3_usb_init_teth_bridge(void)
{
	int result;

	if (ipa3_usb_ctx->teth_bridge_state != IPA_USB_TETH_PROT_INVALID)
		return 0;

	result = teth_bridge_init(&ipa3_usb_ctx->teth_bridge_params);
	if (result) {
		IPA_USB_ERR("Failed to initialize teth_bridge.\n");
		return result;
	}
	ipa3_usb_ctx->teth_bridge_state = IPA_USB_TETH_PROT_INITIALIZED;

	return 0;
}

int ipa3_usb_init_teth_prot(enum ipa_usb_teth_prot teth_prot,
			   struct ipa_usb_teth_params *teth_params,
			   int (*ipa_usb_notify_cb)(enum ipa_usb_notify_event,
			   void *),
			   void *user_data)
{
	int result = -EFAULT;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG("ipa3_usb_init_teth_prot: entry\n");
	if (teth_prot > IPA_USB_MAX_TETH_PROT_SIZE ||
		((teth_prot == IPA_USB_RNDIS || teth_prot == IPA_USB_ECM) &&
		teth_params == NULL) || ipa_usb_notify_cb == NULL ||
		user_data == NULL) {
		IPA_USB_ERR("bad parameters.\n");
		result = -EINVAL;
		goto bad_params;
	}

	if (!ipa3_usb_check_legal_op(IPA_USB_INIT_TETH_PROT,
		(teth_prot == IPA_USB_DIAG))) {
		IPA_USB_ERR("Illegal operation.\n");
		result = -EPERM;
		goto bad_params;
	}

	/* Create IPA RM USB resources */
	if (!ipa3_usb_ctx->rm_ctx.prod_valid) {
		ipa3_usb_ctx->rm_ctx.prod_params.name =
			IPA_RM_RESOURCE_USB_PROD;
		ipa3_usb_ctx->rm_ctx.prod_params.floor_voltage =
			IPA_VOLTAGE_SVS;
		ipa3_usb_ctx->rm_ctx.prod_params.reg_params.user_data = NULL;
		ipa3_usb_ctx->rm_ctx.prod_params.reg_params.notify_cb =
			ipa3_usb_prod_notify_cb;
		ipa3_usb_ctx->rm_ctx.prod_params.request_resource = NULL;
		ipa3_usb_ctx->rm_ctx.prod_params.release_resource = NULL;
		result = ipa_rm_create_resource(
			&ipa3_usb_ctx->rm_ctx.prod_params);
		if (result) {
			IPA_USB_ERR("Failed to create USB_PROD RM resource.\n");
			goto bad_params;
		}
		ipa3_usb_ctx->rm_ctx.prod_valid = true;
		IPA_USB_DBG("Created USB_PROD RM resource.\n");
	}

	if (!ipa3_usb_ctx->rm_ctx.cons_valid) {
		ipa3_usb_ctx->rm_ctx.cons_params.name =
			IPA_RM_RESOURCE_USB_CONS;
		ipa3_usb_ctx->rm_ctx.cons_params.floor_voltage =
			IPA_VOLTAGE_SVS;
		ipa3_usb_ctx->rm_ctx.cons_params.reg_params.user_data = NULL;
		ipa3_usb_ctx->rm_ctx.cons_params.reg_params.notify_cb = NULL;
		ipa3_usb_ctx->rm_ctx.cons_params.request_resource =
			ipa3_usb_cons_request_resource_cb;
		ipa3_usb_ctx->rm_ctx.cons_params.release_resource =
			ipa3_usb_cons_release_resource_cb;
		result = ipa_rm_create_resource(
			&ipa3_usb_ctx->rm_ctx.cons_params);
		if (result) {
			IPA_USB_ERR("Failed to create USB_CONS RM resource.\n");
			goto create_cons_rsc_fail;
		}
		ipa3_usb_ctx->rm_ctx.cons_valid = true;
		IPA_USB_DBG("Created USB_CONS RM resource.\n");
	}
	ipa3_usb_ctx->ipa_usb_notify_cb = ipa_usb_notify_cb;

	/* Initialize tethering protocol */
	switch (teth_prot) {
	case IPA_USB_RNDIS:
	case IPA_USB_ECM:
		if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state !=
			IPA_USB_TETH_PROT_INVALID) {
			IPA_USB_DBG("%s already initialized\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			result = -EPERM;
			goto bad_params;
		}
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].user_data = user_data;
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].
			teth_prot_params.rndis.device_ready_notify =
			ipa3_usb_device_ready_notify_cb;
		memcpy(ipa3_usb_ctx->teth_prot_ctx[teth_prot].
			teth_prot_params.rndis.host_ethaddr,
			teth_params->host_ethaddr,
			sizeof(teth_params->host_ethaddr));
		memcpy(ipa3_usb_ctx->teth_prot_ctx[teth_prot].
			teth_prot_params.rndis.device_ethaddr,
			teth_params->device_ethaddr,
			sizeof(teth_params->device_ethaddr));
		if (teth_prot == IPA_USB_RNDIS) {
			result = rndis_ipa_init(&ipa3_usb_ctx->
				teth_prot_ctx[teth_prot].
				teth_prot_params.rndis);
			if (result) {
				IPA_USB_ERR("Failed to initialize %s.\n",
					ipa3_usb_teth_prot_to_string(
					teth_prot));
				goto init_rndis_ipa_fail;
			}
		} else {
			result = ecm_ipa_init(&ipa3_usb_ctx->
				teth_prot_ctx[teth_prot].teth_prot_params.ecm);
			if (result) {
				IPA_USB_ERR("Failed to initialize %s.\n",
					ipa3_usb_teth_prot_to_string(
					teth_prot));
				goto init_rndis_ipa_fail;
			}
		}
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].state =
			IPA_USB_TETH_PROT_INITIALIZED;
		ipa3_usb_ctx->num_init_prot++;
		IPA_USB_DBG("initialized %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	case IPA_USB_RMNET:
	case IPA_USB_MBIM:
		if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state !=
			IPA_USB_TETH_PROT_INVALID) {
			IPA_USB_DBG("%s already initialized\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			result = -EPERM;
			goto bad_params;
		}
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].user_data = user_data;
		result = ipa3_usb_init_teth_bridge();
		if (result)
			goto init_rndis_ipa_fail;
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].state =
			IPA_USB_TETH_PROT_INITIALIZED;
		ipa3_usb_ctx->num_init_prot++;
		IPA_USB_DBG("initialized %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	case IPA_USB_DIAG:
		if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state !=
			IPA_USB_TETH_PROT_INVALID) {
			IPA_USB_DBG("%s already initialized\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			result = -EPERM;
			goto bad_params;
		}
		ipa3_usb_ctx->diag_user_data = user_data;
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].state =
			IPA_USB_TETH_PROT_INITIALIZED;
		ipa3_usb_ctx->num_init_prot++;
		IPA_USB_DBG("initialized %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	default:
		IPA_USB_ERR("unexpected tethering protocol\n");
		result = -EINVAL;
		goto bad_params;
	}

	if (teth_prot != IPA_USB_DIAG) {
		if (!ipa3_usb_set_state(IPA_USB_INITIALIZED))
			IPA_USB_ERR("failed to change state to initialized\n");
	} else {
		if (!ipa3_usb_set_diag_state(IPA_USB_INITIALIZED))
			IPA_USB_ERR("failed to change diag state to init\n");
	}

	IPA_USB_DBG("ipa3_usb_init_teth_prot: exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

init_rndis_ipa_fail:
	if (ipa3_usb_ctx->num_init_prot == 0)
		ipa_rm_delete_resource(ipa3_usb_ctx->rm_ctx.cons_params.name);
create_cons_rsc_fail:
	if (ipa3_usb_ctx->num_init_prot == 0)
		ipa_rm_delete_resource(ipa3_usb_ctx->rm_ctx.prod_params.name);
bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;
}

void ipa3_usb_gsi_evt_err_cb(struct gsi_evt_err_notify *notify)
{
	IPA_USB_DBG("ipa3_usb_gsi_evt_err_cb: entry\n");
	if (!notify)
		return;
	IPA_USB_ERR("Received event error %d, description: %d\n",
		notify->evt_id, notify->err_desc);
	IPA_USB_DBG("ipa3_usb_gsi_evt_err_cb: exit\n");
}

void ipa3_usb_gsi_chan_err_cb(struct gsi_chan_err_notify *notify)
{
	IPA_USB_DBG("ipa3_usb_gsi_chan_err_cb: entry\n");
	if (!notify)
		return;
	IPA_USB_ERR("Received channel error %d, description: %d\n",
		notify->evt_id, notify->err_desc);
	IPA_USB_DBG("ipa3_usb_gsi_chan_err_cb: exit\n");
}

static bool ipa3_usb_check_chan_params(struct ipa_usb_xdci_chan_params *params)
{
	IPA_USB_DBG("gevntcount_low_addr = %x\n", params->gevntcount_low_addr);
	IPA_USB_DBG("gevntcount_hi_addr = %x\n", params->gevntcount_hi_addr);
	IPA_USB_DBG("dir = %d\n", params->dir);
	IPA_USB_DBG("xfer_ring_len = %d\n", params->xfer_ring_len);
	IPA_USB_DBG("xfer_ring_base_addr = %llx\n",
		params->xfer_ring_base_addr);
	IPA_USB_DBG("last_trb_addr = %x\n",
		params->xfer_scratch.last_trb_addr);
	IPA_USB_DBG("const_buffer_size = %d\n",
		params->xfer_scratch.const_buffer_size);
	IPA_USB_DBG("depcmd_low_addr = %x\n",
		params->xfer_scratch.depcmd_low_addr);
	IPA_USB_DBG("depcmd_hi_addr = %x\n",
		params->xfer_scratch.depcmd_hi_addr);

	if (params->client >= IPA_CLIENT_MAX  ||
		params->teth_prot > IPA_USB_MAX_TETH_PROT_SIZE ||
		params->xfer_ring_len % GSI_CHAN_RE_SIZE_16B ||
		params->xfer_scratch.const_buffer_size < 1 ||
		params->xfer_scratch.const_buffer_size > 31) {
		return false;
	}
	switch (params->teth_prot) {
	case IPA_USB_RNDIS:
	case IPA_USB_ECM:
		if (ipa3_usb_ctx->teth_prot_ctx[params->teth_prot].state ==
			IPA_USB_TETH_PROT_INVALID) {
			IPA_USB_ERR("%s is not initialized\n",
				ipa3_usb_teth_prot_to_string(
				params->teth_prot));
			return false;
		}
		break;
	case IPA_USB_RMNET:
	case IPA_USB_MBIM:
		if (ipa3_usb_ctx->teth_prot_ctx[params->teth_prot].state ==
			IPA_USB_TETH_PROT_INVALID) {
			IPA_USB_ERR("%s is not initialized\n",
				ipa3_usb_teth_prot_to_string(
				params->teth_prot));
			return false;
		}
		break;
	case IPA_USB_DIAG:
		if (ipa3_usb_ctx->teth_prot_ctx[params->teth_prot].state ==
			IPA_USB_TETH_PROT_INVALID) {
			IPA_USB_ERR("%s is not initialized\n",
				ipa3_usb_teth_prot_to_string(
				params->teth_prot));
			return false;
		}
		if (!IPA_CLIENT_IS_CONS(params->client)) {
			IPA_USB_ERR("DIAG supports only DL channel\n");
			return false;
		}
		break;
	default:
		break;
	}
	return true;
}

int ipa3_usb_request_xdci_channel(struct ipa_usb_xdci_chan_params *params,
				 struct ipa_req_chan_out_params *out_params)
{
	int result = -EFAULT;
	struct ipa_request_gsi_channel_params chan_params;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG("ipa3_usb_request_xdci_channel: entry\n");
	if (params == NULL || out_params == NULL ||
		!ipa3_usb_check_chan_params(params)) {
		IPA_USB_ERR("bad parameters.\n");
		result = -EINVAL;
		goto bad_params;
	}

	if (!ipa3_usb_check_legal_op(IPA_USB_REQUEST_CHANNEL,
		(params->teth_prot == IPA_USB_DIAG))) {
		IPA_USB_ERR("Illegal operation.\n");
		result = -EPERM;
		goto bad_params;
	}

	memset(&chan_params, 0, sizeof(struct ipa_request_gsi_channel_params));
	memcpy(&chan_params.ipa_ep_cfg, &params->ipa_ep_cfg,
		sizeof(struct ipa_ep_cfg));
	chan_params.client = params->client;
	switch (params->teth_prot) {
	case IPA_USB_RNDIS:
		chan_params.priv = ipa3_usb_ctx->teth_prot_ctx[IPA_USB_RNDIS].
			teth_prot_params.rndis.private;
		if (params->dir == GSI_CHAN_DIR_FROM_GSI)
			chan_params.notify =
				ipa3_usb_ctx->teth_prot_ctx[IPA_USB_RNDIS].
				teth_prot_params.rndis.ipa_tx_notify;
		else
			chan_params.notify =
				ipa3_usb_ctx->teth_prot_ctx[IPA_USB_RNDIS].
				teth_prot_params.rndis.ipa_rx_notify;
		chan_params.skip_ep_cfg =
			ipa3_usb_ctx->teth_prot_ctx[IPA_USB_RNDIS].
			teth_prot_params.rndis.skip_ep_cfg;
		break;
	case IPA_USB_ECM:
		chan_params.priv = ipa3_usb_ctx->teth_prot_ctx[IPA_USB_ECM].
			teth_prot_params.ecm.private;
		if (params->dir == GSI_CHAN_DIR_FROM_GSI)
			chan_params.notify =
				ipa3_usb_ctx->teth_prot_ctx[IPA_USB_ECM].
				teth_prot_params.ecm.ecm_ipa_tx_dp_notify;
		else
			chan_params.notify =
				ipa3_usb_ctx->teth_prot_ctx[IPA_USB_ECM].
				teth_prot_params.ecm.ecm_ipa_rx_dp_notify;
		chan_params.skip_ep_cfg =
			ipa3_usb_ctx->teth_prot_ctx[IPA_USB_ECM].
			teth_prot_params.ecm.skip_ep_cfg;
		break;
	case IPA_USB_RMNET:
	case IPA_USB_MBIM:
		chan_params.priv =
			ipa3_usb_ctx->teth_bridge_params.private_data;
		chan_params.notify =
			ipa3_usb_ctx->teth_bridge_params.usb_notify_cb;
		chan_params.skip_ep_cfg =
			ipa3_usb_ctx->teth_bridge_params.skip_ep_cfg;
		break;
	case IPA_USB_DIAG:
		chan_params.priv = NULL;
		chan_params.notify = NULL;
		chan_params.skip_ep_cfg = true;
		break;
	default:
		break;
	}
	chan_params.keep_ipa_awake = params->keep_ipa_awake;
	chan_params.evt_ring_params.intf = GSI_EVT_CHTYPE_XDCI_EV;
	chan_params.evt_ring_params.intr = GSI_INTR_IRQ;
	chan_params.evt_ring_params.re_size = GSI_EVT_RING_RE_SIZE_16B;
	chan_params.evt_ring_params.ring_len = params->xfer_ring_len -
		chan_params.evt_ring_params.re_size;
	chan_params.evt_ring_params.ring_base_addr =
		params->xfer_ring_base_addr;
	chan_params.evt_ring_params.ring_base_vaddr = NULL;
	chan_params.evt_ring_params.int_modt = 0;
	chan_params.evt_ring_params.int_modt = 0;
	chan_params.evt_ring_params.intvec = 0;
	chan_params.evt_ring_params.msi_addr = 0;
	chan_params.evt_ring_params.rp_update_addr = 0;
	chan_params.evt_ring_params.exclusive = true;
	chan_params.evt_ring_params.err_cb = ipa3_usb_gsi_evt_err_cb;
	chan_params.evt_ring_params.user_data = NULL;
	chan_params.evt_scratch.xdci.gevntcount_low_addr =
		params->gevntcount_low_addr;
	chan_params.evt_scratch.xdci.gevntcount_hi_addr =
		params->gevntcount_hi_addr;
	chan_params.chan_params.prot = GSI_CHAN_PROT_XDCI;
	chan_params.chan_params.dir = params->dir;
	/* chan_id is set in ipa3_request_gsi_channel() */
	chan_params.chan_params.re_size = GSI_CHAN_RE_SIZE_16B;
	chan_params.chan_params.ring_len = params->xfer_ring_len;
	chan_params.chan_params.ring_base_addr =
		params->xfer_ring_base_addr;
	chan_params.chan_params.ring_base_vaddr = NULL;
	chan_params.chan_params.use_db_eng = GSI_CHAN_DIRECT_MODE;
	chan_params.chan_params.max_prefetch = GSI_ONE_PREFETCH_SEG;
	if (params->dir == GSI_CHAN_DIR_FROM_GSI)
		chan_params.chan_params.low_weight =
			IPA_USB_DL_CHAN_LOW_WEIGHT;
	else
		chan_params.chan_params.low_weight =
			IPA_USB_UL_CHAN_LOW_WEIGHT;
	chan_params.chan_params.xfer_cb = NULL;
	chan_params.chan_params.err_cb = ipa3_usb_gsi_chan_err_cb;
	chan_params.chan_params.chan_user_data = NULL;
	chan_params.chan_scratch.xdci.last_trb_addr =
		params->xfer_scratch.last_trb_addr;
	/* xferrscidx will be updated later */
	chan_params.chan_scratch.xdci.xferrscidx = 0;
	chan_params.chan_scratch.xdci.const_buffer_size =
		params->xfer_scratch.const_buffer_size;
	chan_params.chan_scratch.xdci.depcmd_low_addr =
		params->xfer_scratch.depcmd_low_addr;
	chan_params.chan_scratch.xdci.depcmd_hi_addr =
		params->xfer_scratch.depcmd_hi_addr;
	/* max_outstanding_tre is set in ipa3_request_gsi_channel() */
	result = ipa3_request_gsi_channel(&chan_params, out_params);
	if (result) {
		IPA_USB_ERR("failed to allocate GSI channel\n");
		mutex_unlock(&ipa3_usb_ctx->general_mutex);
		return result;
	}

	IPA_USB_DBG("ipa3_usb_request_xdci_channel: exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;
}

static int ipa3_usb_request_prod(void)
{
	int result;

	init_completion(&ipa3_usb_ctx->rm_ctx.prod_comp);
	IPA_USB_DBG("requesting USB_PROD\n");
	result = ipa3_rm_request_resource(
		ipa3_usb_ctx->rm_ctx.prod_params.name);
	if (result) {
		if (result != -EINPROGRESS) {
			IPA_USB_ERR("failed to request USB_PROD: %d\n", result);
			return result;
		}
		result = wait_for_completion_timeout(
			&ipa3_usb_ctx->rm_ctx.prod_comp,
			msecs_to_jiffies(IPA_USB_RM_TIMEOUT_MSEC));
		if (result == 0) {
			IPA_USB_ERR("timeout request USB_PROD\n");
			return -ETIME;
		}
	}

	IPA_USB_DBG("USB_PROD granted\n");
	return 0;
}

static int ipa3_usb_release_prod(void)
{
	int result;

	init_completion(&ipa3_usb_ctx->rm_ctx.prod_comp);
	IPA_USB_DBG("releasing USB_PROD\n");
	result = ipa_rm_release_resource(ipa3_usb_ctx->rm_ctx.prod_params.name);
	if (result) {
		if (result != -EINPROGRESS) {
			IPA_USB_ERR("failed to release USB_PROD: %d\n", result);
			return result;
		}
		result = wait_for_completion_timeout(
			&ipa3_usb_ctx->rm_ctx.prod_comp,
			msecs_to_jiffies(IPA_USB_RM_TIMEOUT_MSEC));
		if (result == 0) {
			IPA_USB_ERR("timeout release USB_PROD\n");
			return -ETIME;
		}
	}

	IPA_USB_DBG("USB_PROD released\n");
	return 0;
}

static bool ipa3_usb_check_connect_params(
	struct ipa_usb_xdci_connect_params *params)
{
	IPA_USB_DBG("ul xferrscidx = %d\n", params->usb_to_ipa_xferrscidx);
	IPA_USB_DBG("dl xferrscidx = %d\n", params->ipa_to_usb_xferrscidx);
	IPA_USB_DBG("max_supported_bandwidth_mbps = %d\n",
		params->max_supported_bandwidth_mbps);

	if (params->max_pkt_size < IPA_USB_HIGH_SPEED_512B  ||
		params->max_pkt_size > IPA_USB_SUPER_SPEED_1024B  ||
		params->ipa_to_usb_xferrscidx < 0 ||
		params->ipa_to_usb_xferrscidx > 127 ||
		(params->teth_prot != IPA_USB_DIAG &&
		(params->usb_to_ipa_xferrscidx < 0 ||
		params->usb_to_ipa_xferrscidx > 127)) ||
		params->teth_prot > IPA_USB_MAX_TETH_PROT_SIZE) {
		return false;
	}

	switch (params->teth_prot) {
	case IPA_USB_RNDIS:
	case IPA_USB_ECM:
	case IPA_USB_DIAG:
		if (ipa3_usb_ctx->teth_prot_ctx[params->teth_prot].state ==
			IPA_USB_TETH_PROT_INVALID) {
			IPA_USB_ERR("%s is not initialized\n",
				ipa3_usb_teth_prot_to_string(
				params->teth_prot));
			return false;
		}
		break;
	case IPA_USB_RMNET:
	case IPA_USB_MBIM:
		return true;
	default:
		break;
	}
	return true;
}

static int ipa3_usb_connect_teth_bridge(
	struct teth_bridge_connect_params *params)
{
	int result;

	if (ipa3_usb_ctx->teth_bridge_state != IPA_USB_TETH_PROT_INITIALIZED)
		return 0;

	result = teth_bridge_connect(params);
	if (result) {
		IPA_USB_ERR("failed to connect teth_bridge.\n");
		ipa3_usb_ctx->user_data = NULL;
		return result;
	}
	ipa3_usb_ctx->teth_bridge_state = IPA_USB_TETH_PROT_CONNECTED;

	return 0;
}

static int ipa3_usb_connect_teth_prot(
	struct ipa_usb_xdci_connect_params *params)
{
	int result;
	struct teth_bridge_connect_params teth_bridge_params;

	IPA_USB_DBG("ipa3_usb_connect_teth_prot: connecting protocol = %d\n",
		params->teth_prot);
	switch (params->teth_prot) {
	case IPA_USB_RNDIS:
		if (ipa3_usb_ctx->teth_prot_ctx[IPA_USB_RNDIS].state ==
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s is already connected.\n",
				ipa3_usb_teth_prot_to_string(
				params->teth_prot));
			break;
		}
		ipa3_usb_ctx->user_data =
			ipa3_usb_ctx->teth_prot_ctx[IPA_USB_RNDIS].user_data;
		result = rndis_ipa_pipe_connect_notify(
			params->usb_to_ipa_clnt_hdl,
			params->ipa_to_usb_clnt_hdl,
			params->teth_prot_params.max_xfer_size_bytes_to_dev,
			params->teth_prot_params.max_packet_number_to_dev,
			params->teth_prot_params.max_xfer_size_bytes_to_host,
			ipa3_usb_ctx->teth_prot_ctx[IPA_USB_RNDIS].
			teth_prot_params.rndis.private);
		if (result) {
			IPA_USB_ERR("failed to connect %s.\n",
				ipa3_usb_teth_prot_to_string(
				params->teth_prot));
			ipa3_usb_ctx->user_data = NULL;
			return result;
		}
		ipa3_usb_ctx->teth_prot_ctx[IPA_USB_RNDIS].state =
			IPA_USB_TETH_PROT_CONNECTED;
		IPA_USB_DBG("%s is connected.\n",
			ipa3_usb_teth_prot_to_string(
			params->teth_prot));
		break;
	case IPA_USB_ECM:
		if (ipa3_usb_ctx->teth_prot_ctx[IPA_USB_ECM].state ==
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s is already connected.\n",
				ipa3_usb_teth_prot_to_string(
				params->teth_prot));
			break;
		}
		ipa3_usb_ctx->user_data =
			ipa3_usb_ctx->teth_prot_ctx[IPA_USB_ECM].user_data;
		result = ecm_ipa_connect(params->usb_to_ipa_clnt_hdl,
			params->ipa_to_usb_clnt_hdl,
			ipa3_usb_ctx->teth_prot_ctx[IPA_USB_ECM].
			teth_prot_params.ecm.private);
		if (result) {
			IPA_USB_ERR("failed to connect %s.\n",
				ipa3_usb_teth_prot_to_string(
				params->teth_prot));
			ipa3_usb_ctx->user_data = NULL;
			return result;
		}
		ipa3_usb_ctx->teth_prot_ctx[IPA_USB_ECM].state =
			IPA_USB_TETH_PROT_CONNECTED;
		IPA_USB_DBG("%s is connected.\n",
			ipa3_usb_teth_prot_to_string(
			params->teth_prot));
		break;
	case IPA_USB_RMNET:
	case IPA_USB_MBIM:
		if (ipa3_usb_ctx->teth_prot_ctx[params->teth_prot].state ==
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s is already connected.\n",
				ipa3_usb_teth_prot_to_string(
				params->teth_prot));
			break;
		}
		result = ipa3_usb_init_teth_bridge();
		if (result)
				return result;

		ipa3_usb_ctx->user_data =
			ipa3_usb_ctx->teth_prot_ctx[params->teth_prot].
			user_data;
		teth_bridge_params.ipa_usb_pipe_hdl =
			params->ipa_to_usb_clnt_hdl;
		teth_bridge_params.usb_ipa_pipe_hdl =
			params->usb_to_ipa_clnt_hdl;
		teth_bridge_params.tethering_mode = TETH_TETHERING_MODE_RMNET;
		teth_bridge_params.client_type = IPA_CLIENT_USB_PROD;
		result = ipa3_usb_connect_teth_bridge(&teth_bridge_params);
		if (result)
			return result;
		ipa3_usb_ctx->teth_prot_ctx[params->teth_prot].state =
			IPA_USB_TETH_PROT_CONNECTED;
		ipa3_usb_notify_device_ready(ipa3_usb_ctx->user_data);
		IPA_USB_DBG("%s is connected.\n",
			ipa3_usb_teth_prot_to_string(
			params->teth_prot));
		break;
	case IPA_USB_DIAG:
		if (ipa3_usb_ctx->teth_prot_ctx[IPA_USB_DIAG].state ==
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s is already connected.\n",
				ipa3_usb_teth_prot_to_string(
				params->teth_prot));
			return -EPERM;
		}
		ipa3_usb_ctx->teth_prot_ctx[IPA_USB_DIAG].state =
			IPA_USB_TETH_PROT_CONNECTED;
		ipa3_usb_notify_device_ready(ipa3_usb_ctx->diag_user_data);
		IPA_USB_DBG("%s is connected.\n",
			ipa3_usb_teth_prot_to_string(
			params->teth_prot));
		break;
	default:
		break;
	}

	return 0;
}

static int ipa3_usb_disconnect_teth_bridge(void)
{
	int result;

	if (ipa3_usb_ctx->teth_bridge_state != IPA_USB_TETH_PROT_CONNECTED)
		return 0;

	result = teth_bridge_disconnect(IPA_CLIENT_USB_PROD);
	if (result) {
		IPA_USB_ERR("failed to disconnect teth_bridge.\n");
		return result;
	}
	ipa3_usb_ctx->teth_bridge_state = IPA_USB_TETH_PROT_INVALID;

	return 0;
}

static int ipa3_usb_disconnect_teth_prot(enum ipa_usb_teth_prot teth_prot)
{
	int result = 0;

	switch (teth_prot) {
	case IPA_USB_RNDIS:
	case IPA_USB_ECM:
		if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state !=
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s is not connected.\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			return -EPERM;
		}
		if (teth_prot == IPA_USB_RNDIS) {
			result = rndis_ipa_pipe_disconnect_notify(
				ipa3_usb_ctx->teth_prot_ctx[teth_prot].
				teth_prot_params.rndis.private);
		} else {
			result = ecm_ipa_disconnect(
				ipa3_usb_ctx->teth_prot_ctx[teth_prot].
				teth_prot_params.ecm.private);
		}
		if (result) {
			IPA_USB_ERR("failed to disconnect %s.\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			break;
		}
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].state =
			IPA_USB_TETH_PROT_INITIALIZED;
		IPA_USB_DBG("disconnected %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	case IPA_USB_RMNET:
	case IPA_USB_MBIM:
		if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state !=
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s is not connected.\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			return -EPERM;
		}
		result = ipa3_usb_disconnect_teth_bridge();
		if (result)
			break;
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].state =
			IPA_USB_TETH_PROT_INITIALIZED;
		IPA_USB_DBG("disconnected %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	case IPA_USB_DIAG:
		if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state !=
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s is not connected.\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			return -EPERM;
		}
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].state =
			IPA_USB_TETH_PROT_INITIALIZED;
		IPA_USB_DBG("disconnected %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	default:
		break;
	}

	ipa3_usb_ctx->user_data = NULL;
	return result;
}

static ipa_usb_callback ipa3_usb_get_client_notify_cb(
	enum ipa_usb_teth_prot teth_prot, bool is_tx)
{
	switch (teth_prot) {
	case IPA_USB_RNDIS:
		if (is_tx)
			return ipa3_usb_ctx->teth_prot_ctx[IPA_USB_RNDIS].
				teth_prot_params.rndis.ipa_tx_notify;
		else
			return ipa3_usb_ctx->teth_prot_ctx[IPA_USB_RNDIS].
				teth_prot_params.rndis.ipa_rx_notify;
	case IPA_USB_ECM:
		if (is_tx)
			return ipa3_usb_ctx->teth_prot_ctx[IPA_USB_ECM].
				teth_prot_params.ecm.ecm_ipa_tx_dp_notify;
		else
			return ipa3_usb_ctx->teth_prot_ctx[IPA_USB_ECM].
				teth_prot_params.ecm.ecm_ipa_rx_dp_notify;
	case IPA_USB_RMNET:
	case IPA_USB_MBIM:
		return ipa3_usb_ctx->teth_bridge_params.usb_notify_cb;
	case IPA_USB_DIAG:
		return NULL;
	default:
		return NULL;
	}

	return NULL;
}

static int ipa3_usb_xdci_connect_diag(
	struct ipa_usb_xdci_connect_params *params)
{
	int result = -EFAULT;

	/* Start DIAG channel */
	result = ipa3_xdci_connect(params->ipa_to_usb_clnt_hdl,
		params->ipa_to_usb_xferrscidx,
		params->ipa_to_usb_xferrscidx_valid,
		ipa3_usb_get_client_notify_cb(IPA_USB_DIAG, true));
	if (result) {
		IPA_USB_ERR("failed to connect DIAG channel.\n");
		return result;
	}

	result = ipa3_usb_connect_teth_prot(params);
	if (result)
		goto connect_teth_prot_fail;

	/* Change diag state to CONNECTED */
	if (!ipa3_usb_set_diag_state(IPA_USB_CONNECTED)) {
		IPA_USB_ERR("failed to change diag state to connected\n");
		goto state_change_diag_connected_fail;
	}

	return 0;

state_change_diag_connected_fail:
	ipa3_usb_disconnect_teth_prot(params->teth_prot);
connect_teth_prot_fail:
	ipa3_xdci_disconnect(params->ipa_to_usb_clnt_hdl, false, -1);
	ipa3_reset_gsi_channel(params->ipa_to_usb_clnt_hdl);
	ipa3_reset_gsi_event_ring(params->ipa_to_usb_clnt_hdl);
	return result;
}

int ipa3_usb_xdci_connect(struct ipa_usb_xdci_connect_params *params)
{
	int result = -EFAULT;
	struct ipa_rm_perf_profile profile;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG("ipa3_usb_xdci_connect: entry\n");
	if (params == NULL || !ipa3_usb_check_connect_params(params)) {
		IPA_USB_ERR("bad parameters.\n");
		result = -EINVAL;
		goto bad_params;
	}

	if (!ipa3_usb_check_legal_op(IPA_USB_CONNECT,
		(params->teth_prot == IPA_USB_DIAG))) {
		IPA_USB_ERR("Illegal operation.\n");
		result = -EPERM;
		goto bad_params;
	}

	if (params->teth_prot == IPA_USB_DIAG) {
		result = ipa3_usb_xdci_connect_diag(params);
		IPA_USB_DBG("ipa3_usb_xdci_connect: exit\n");
		mutex_unlock(&ipa3_usb_ctx->general_mutex);
		return result;
	}

	/* Set EE xDCI specific scratch */
	result = ipa3_set_usb_max_packet_size(params->max_pkt_size);
	if (result) {
		IPA_USB_ERR("failed setting xDCI EE scratch field\n");
		goto bad_params;
	}

	/* Set USB_PROD & USB_CONS perf profile */
	profile.max_supported_bandwidth_mbps =
		params->max_supported_bandwidth_mbps;
	result = ipa_rm_set_perf_profile(ipa3_usb_ctx->rm_ctx.prod_params.name,
			&profile);
	if (result) {
		IPA_USB_ERR("failed to set USB_PROD perf profile.\n");
		goto bad_params;
	}
	result = ipa_rm_set_perf_profile(ipa3_usb_ctx->rm_ctx.cons_params.name,
			&profile);
	if (result) {
		IPA_USB_ERR("failed to set USB_CONS perf profile.\n");
		goto bad_params;
	}

	/* Request USB_PROD */
	result = ipa3_usb_request_prod();
	if (result)
		goto bad_params;

	/* Start UL channel */
	result = ipa3_xdci_connect(params->usb_to_ipa_clnt_hdl,
		params->usb_to_ipa_xferrscidx,
		params->usb_to_ipa_xferrscidx_valid,
		ipa3_usb_get_client_notify_cb(params->teth_prot, false));
	if (result) {
		IPA_USB_ERR("failed to connect UL channel.\n");
		goto connect_ul_fail;
	}

	/* Start DL channel */
	result = ipa3_xdci_connect(params->ipa_to_usb_clnt_hdl,
		params->ipa_to_usb_xferrscidx,
		params->ipa_to_usb_xferrscidx_valid,
		ipa3_usb_get_client_notify_cb(params->teth_prot, true));
	if (result) {
		IPA_USB_ERR("failed to connect DL channel.\n");
		goto connect_dl_fail;
	}

	/* Connect tethering protocol */
	result = ipa3_usb_connect_teth_prot(params);
	if (result)
		goto connect_teth_prot_fail;

	/* Change ipa_usb state to CONNECTED */
	if (!ipa3_usb_set_state(IPA_USB_CONNECTED)) {
		IPA_USB_ERR("failed to change state to connected\n");
		goto state_change_connected_fail;
	}

	IPA_USB_DBG("ipa3_usb_xdci_connect: exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

state_change_connected_fail:
	ipa3_usb_disconnect_teth_prot(params->teth_prot);
connect_teth_prot_fail:
	ipa3_xdci_disconnect(params->ipa_to_usb_clnt_hdl, false, -1);
	ipa3_reset_gsi_channel(params->ipa_to_usb_clnt_hdl);
	ipa3_reset_gsi_event_ring(params->ipa_to_usb_clnt_hdl);
connect_dl_fail:
	ipa3_xdci_disconnect(params->usb_to_ipa_clnt_hdl, false, -1);
	ipa3_reset_gsi_channel(params->usb_to_ipa_clnt_hdl);
	ipa3_reset_gsi_event_ring(params->usb_to_ipa_clnt_hdl);
connect_ul_fail:
	ipa3_usb_release_prod();
bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;
}

static int ipa3_usb_xdci_disconnect_diag(u32 dl_clnt_hdl)
{
	int result = 0;

	/* Stop DIAG channel */
	result = ipa3_xdci_disconnect(dl_clnt_hdl, false, -1);
	if (result) {
		IPA_USB_ERR("failed to disconnect DIAG channel.\n");
		return result;
	}

	/* Reset DIAG channel */
	result = ipa3_reset_gsi_channel(dl_clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to reset DIAG channel.\n");
		return result;
	}

	/* Reset DIAG event ring */
	result = ipa3_reset_gsi_event_ring(dl_clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to reset DIAG event ring.\n");
		return result;
	}

	/* Disconnect DIAG */
	result = ipa3_usb_disconnect_teth_prot(IPA_USB_DIAG);
	if (result)
		return result;

	/* Change diag state to STOPPED */
	if (!ipa3_usb_set_diag_state(IPA_USB_STOPPED))
		IPA_USB_ERR("failed to change diag state to stopped\n");

	return 0;
}

static int ipa3_usb_check_disconnect_prot(enum ipa_usb_teth_prot teth_prot)
{
	if (teth_prot > IPA_USB_MAX_TETH_PROT_SIZE) {
		IPA_USB_ERR("bad parameter.\n");
		return -EFAULT;
	}

	switch (teth_prot) {
	case IPA_USB_RNDIS:
	case IPA_USB_ECM:
	case IPA_USB_DIAG:
		if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state !=
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_ERR("%s is not connected.\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			return -EFAULT;
		}
		break;
	case IPA_USB_RMNET:
	case IPA_USB_MBIM:
		if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state !=
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_ERR("%s is not connected.\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			return -EFAULT;
		}
		break;
	default:
		break;
	}
	return 0;
}

int ipa3_usb_xdci_disconnect(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
			    enum ipa_usb_teth_prot teth_prot)
{
	int result = 0;
	struct ipa_ep_cfg_holb holb_cfg;
	unsigned long flags;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG("ipa3_usb_xdci_disconnect: entry\n");
	if (ipa3_usb_check_disconnect_prot(teth_prot)) {
		result = -EINVAL;
		goto bad_params;
	}

	if (!ipa3_usb_check_legal_op(IPA_USB_DISCONNECT,
		(teth_prot == IPA_USB_DIAG))) {
		IPA_USB_ERR("Illegal operation.\n");
		result = -EPERM;
		goto bad_params;
	}

	if (teth_prot == IPA_USB_DIAG) {
		result = ipa3_usb_xdci_disconnect_diag(dl_clnt_hdl);
		IPA_USB_DBG("ipa3_usb_xdci_disconnect: exit\n");
		mutex_unlock(&ipa3_usb_ctx->general_mutex);
		return result;
	}

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	if (ipa3_usb_ctx->state != IPA_USB_SUSPEND_IN_PROGRESS &&
		ipa3_usb_ctx->state != IPA_USB_SUSPENDED) {
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
		/* Stop UL channel */
		result = ipa3_xdci_disconnect(ul_clnt_hdl,
			(teth_prot == IPA_USB_RMNET ||
			teth_prot == IPA_USB_MBIM),
			ipa3_usb_ctx->qmi_req_id);
		if (result) {
			IPA_USB_ERR("failed to disconnect UL channel.\n");
			goto bad_params;
		}
		if (teth_prot == IPA_USB_RMNET ||
			teth_prot == IPA_USB_MBIM)
			ipa3_usb_ctx->qmi_req_id++;
	} else
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	if (ipa3_usb_ctx->state != IPA_USB_SUSPENDED) {
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
		/* Stop DL channel */
		result = ipa3_xdci_disconnect(dl_clnt_hdl, false, -1);
		if (result) {
			IPA_USB_ERR("failed to disconnect DL channel.\n");
			goto bad_params;
		}
	} else
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	if (ipa3_usb_ctx->state == IPA_USB_SUSPENDED) {
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
		memset(&holb_cfg, 0, sizeof(holb_cfg));
		holb_cfg.en = IPA_HOLB_TMR_EN;
		holb_cfg.tmr_val = 0;
		ipa3_cfg_ep_holb(dl_clnt_hdl, &holb_cfg);
	} else
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);

	/* Reset UL channel */
	result = ipa3_reset_gsi_channel(ul_clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to reset UL channel.\n");
		goto bad_params;
	}

	/* Reset UL event ring */
	result = ipa3_reset_gsi_event_ring(ul_clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to reset UL event ring.\n");
		return result;
	}

	/* Reset DL channel */
	result = ipa3_reset_gsi_channel(dl_clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to reset DL channel.\n");
		goto bad_params;
	}

	/* Reset DL event ring */
	result = ipa3_reset_gsi_event_ring(dl_clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to reset DL event ring.\n");
		return result;
	}

	/* Disconnect tethering protocol */
	result = ipa3_usb_disconnect_teth_prot(teth_prot);
	if (result)
		goto bad_params;

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	if (ipa3_usb_ctx->state != IPA_USB_SUSPEND_IN_PROGRESS &&
		ipa3_usb_ctx->state != IPA_USB_SUSPENDED) {
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
		result = ipa3_usb_release_prod();
		if (result) {
			IPA_USB_ERR("failed to release USB_PROD.\n");
			goto bad_params;
		}
	} else
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);

	/* Change ipa_usb state to STOPPED */
	if (!ipa3_usb_set_state(IPA_USB_STOPPED))
		IPA_USB_ERR("failed to change state to stopped\n");

	IPA_USB_DBG("ipa3_usb_xdci_disconnect: exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;

}

static int ipa3_usb_release_diag_channel(u32 clnt_hdl)
{
	int result = 0;

	/* Release DIAG channel */
	result = ipa3_release_gsi_channel(clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to release DIAG channel.\n");
		return result;
	}

	/* Change ipa_usb_diag state to INITIALIZED */
	if (!ipa3_usb_set_diag_state(IPA_USB_INITIALIZED))
		IPA_USB_ERR("failed to change DIAG state to initialized\n");

	return 0;
}

int ipa3_usb_release_xdci_channel(u32 clnt_hdl,
	enum ipa_usb_teth_prot teth_prot)
{
	int result = 0;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG("ipa3_usb_release_xdci_channel: entry\n");
	if (teth_prot > IPA_USB_MAX_TETH_PROT_SIZE) {
		IPA_USB_ERR("bad parameter.\n");
		result = -EINVAL;
		goto bad_params;
	}

	if (!ipa3_usb_check_legal_op(IPA_USB_RELEASE_CHANNEL,
		(teth_prot == IPA_USB_DIAG))) {
		IPA_USB_ERR("Illegal operation.\n");
		result = -EPERM;
		goto bad_params;
	}

	if (teth_prot == IPA_USB_DIAG) {
		result = ipa3_usb_release_diag_channel(clnt_hdl);
		IPA_USB_DBG("ipa3_usb_release_xdci_channel: exit\n");
		mutex_unlock(&ipa3_usb_ctx->general_mutex);
		return result;
	}

	/* Release channel */
	result = ipa3_release_gsi_channel(clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to deallocate channel.\n");
		goto bad_params;
	}

	/* Change ipa_usb state to INITIALIZED */
	if (!ipa3_usb_set_state(IPA_USB_INITIALIZED))
		IPA_USB_ERR("failed to change state to initialized\n");

	IPA_USB_DBG("ipa3_usb_release_xdci_channel: exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;
}

int ipa3_usb_deinit_teth_prot(enum ipa_usb_teth_prot teth_prot)
{
	int result = -EFAULT;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG("ipa3_usb_deinit_teth_prot: entry\n");
	if (teth_prot > IPA_USB_MAX_TETH_PROT_SIZE) {
		IPA_USB_ERR("bad parameters.\n");
		result = -EINVAL;
		goto bad_params;
	}

	if (!ipa3_usb_check_legal_op(IPA_USB_DEINIT_TETH_PROT,
		(teth_prot == IPA_USB_DIAG))) {
		IPA_USB_ERR("Illegal operation.\n");
		result = -EPERM;
		goto bad_params;
	}

	/* Clean-up tethering protocol */
	switch (teth_prot) {
	case IPA_USB_RNDIS:
	case IPA_USB_ECM:
		if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state !=
			IPA_USB_TETH_PROT_INITIALIZED) {
			IPA_USB_ERR("%s is not initialized\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			result = -EINVAL;
			goto bad_params;
		}
		if (teth_prot == IPA_USB_RNDIS)
			rndis_ipa_cleanup(
				ipa3_usb_ctx->teth_prot_ctx[teth_prot].
				teth_prot_params.rndis.private);
		else
			ecm_ipa_cleanup(
				ipa3_usb_ctx->teth_prot_ctx[teth_prot].
				teth_prot_params.ecm.private);
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].user_data = NULL;
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].state =
			IPA_USB_TETH_PROT_INVALID;
		ipa3_usb_ctx->num_init_prot--;
		IPA_USB_DBG("deinitialized %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	case IPA_USB_RMNET:
	case IPA_USB_MBIM:
		if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state ==
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_ERR("%s is connected\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			result = -EINVAL;
			goto bad_params;
		}
		result = ipa3_usb_disconnect_teth_bridge();
		if (result)
			goto bad_params;
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].user_data =
			NULL;
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].state =
			IPA_USB_TETH_PROT_INVALID;
		ipa3_usb_ctx->num_init_prot--;
		IPA_USB_DBG("deinitialized %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	case IPA_USB_DIAG:
		if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state !=
			IPA_USB_TETH_PROT_INITIALIZED) {
			IPA_USB_ERR("%s is not initialized\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			result = -EINVAL;
			goto bad_params;
		}
		ipa3_usb_ctx->diag_user_data = NULL;
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].state =
			IPA_USB_TETH_PROT_INVALID;
		ipa3_usb_ctx->num_init_prot--;
		IPA_USB_DBG("deinitialized %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	default:
		IPA_USB_ERR("unexpected tethering protocol\n");
		result = -EINVAL;
		goto bad_params;
	}

	if (teth_prot == IPA_USB_DIAG &&
		!ipa3_usb_set_diag_state(IPA_USB_INVALID)) {
		IPA_USB_ERR("failed to change diag state to invalid\n");
	}
	if (ipa3_usb_ctx->num_init_prot == 0) {
		if (!ipa3_usb_set_state(IPA_USB_INVALID))
			IPA_USB_ERR("failed to change state to invalid\n");
		ipa_rm_delete_resource(ipa3_usb_ctx->rm_ctx.prod_params.name);
		ipa3_usb_ctx->rm_ctx.prod_valid = false;
		ipa_rm_delete_resource(ipa3_usb_ctx->rm_ctx.cons_params.name);
		ipa3_usb_ctx->rm_ctx.cons_valid = false;
		ipa3_usb_ctx->ipa_usb_notify_cb = NULL;
	}

	IPA_USB_DBG("ipa3_usb_deinit_teth_prot: exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;
}

int ipa3_usb_xdci_suspend(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot)
{
	int result = 0;
	unsigned long flags;
	enum ipa3_usb_cons_state curr_cons_state;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG("ipa3_usb_xdci_suspend: entry\n");
	if (teth_prot > IPA_USB_MAX_TETH_PROT_SIZE) {
		IPA_USB_ERR("bad parameters.\n");
		result = -EINVAL;
		goto bad_params;
	}

	if (!ipa3_usb_check_legal_op(IPA_USB_SUSPEND, false)) {
		IPA_USB_ERR("Illegal operation.\n");
		result = -EPERM;
		goto bad_params;
	}

	/* Change ipa_usb state to SUSPEND_REQUESTED */
	if (!ipa3_usb_set_state(IPA_USB_SUSPEND_REQUESTED)) {
		IPA_USB_ERR("failed to change state to suspend_requested\n");
		result = -EFAULT;
		goto bad_params;
	}

	/* Stop UL channel & suspend DL EP */
	result = ipa3_xdci_suspend(ul_clnt_hdl, dl_clnt_hdl,
		(teth_prot == IPA_USB_RMNET ||
		teth_prot == IPA_USB_MBIM),
		ipa3_usb_ctx->qmi_req_id);
	if (result) {
		IPA_USB_ERR("failed to suspend\n");
		goto suspend_fail;
	}
	if (teth_prot == IPA_USB_RMNET ||
		teth_prot == IPA_USB_MBIM)
		ipa3_usb_ctx->qmi_req_id++;

	result = ipa3_usb_release_prod();
	if (result) {
		IPA_USB_ERR("failed to release USB_PROD.\n");
		goto release_prod_fail;
	}

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	curr_cons_state = ipa3_usb_ctx->rm_ctx.cons_state;
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
	if (curr_cons_state == IPA_USB_CONS_GRANTED) {
		/* Change ipa_usb state to SUSPEND_IN_PROGRESS */
		if (!ipa3_usb_set_state(IPA_USB_SUSPEND_IN_PROGRESS))
			IPA_USB_ERR("fail set state to suspend_in_progress\n");
		/* Check if DL data pending */
		spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
		if (ipa3_usb_ctx->rm_ctx.cons_requested) {
			IPA_USB_DBG("DL data pending, invoke remote wakeup\n");
			queue_work(ipa3_usb_ctx->wq,
				&ipa3_usb_notify_remote_wakeup_work);
		}
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
		ipa3_usb_ctx->finish_suspend_work.dl_clnt_hdl = dl_clnt_hdl;
		ipa3_usb_ctx->finish_suspend_work.ul_clnt_hdl = ul_clnt_hdl;
		INIT_WORK(&ipa3_usb_ctx->finish_suspend_work.work,
			ipa3_usb_wq_finish_suspend_work);
		result = -EINPROGRESS;
		IPA_USB_DBG("ipa3_usb_xdci_suspend: exit\n");
		goto bad_params;
	}

	/* Stop DL channel */
	result = ipa3_stop_gsi_channel(dl_clnt_hdl);
	if (result) {
		IPAERR("Error stopping DL channel: %d\n", result);
		result = -EFAULT;
		goto release_prod_fail;
	}
	/* Change ipa_usb state to SUSPENDED */
	if (!ipa3_usb_set_state(IPA_USB_SUSPENDED))
		IPA_USB_ERR("failed to change state to suspended\n");
	/* Check if DL data pending */
	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	if (ipa3_usb_ctx->rm_ctx.cons_requested) {
		IPA_USB_DBG("DL data is pending, invoking remote wakeup\n");
		queue_work(ipa3_usb_ctx->wq,
			&ipa3_usb_notify_remote_wakeup_work);
	}
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);

	IPA_USB_DBG("ipa3_usb_xdci_suspend: exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

release_prod_fail:
	ipa3_xdci_resume(ul_clnt_hdl, dl_clnt_hdl);
suspend_fail:
	/* Change ipa_usb state back to CONNECTED */
	if (!ipa3_usb_set_state(IPA_USB_CONNECTED))
		IPA_USB_ERR("failed to change state to connected\n");
bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;
}

int ipa3_usb_xdci_resume(u32 ul_clnt_hdl, u32 dl_clnt_hdl)
{
	int result = -EFAULT;
	enum ipa3_usb_state prev_state;
	unsigned long flags;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG("ipa3_usb_xdci_resume: entry\n");

	if (!ipa3_usb_check_legal_op(IPA_USB_RESUME, false)) {
		IPA_USB_ERR("Illegal operation.\n");
		result = -EPERM;
		goto bad_params;
	}

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	prev_state = ipa3_usb_ctx->state;
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);

	/* Change ipa_usb state to RESUME_IN_PROGRESS */
	if (!ipa3_usb_set_state(IPA_USB_RESUME_IN_PROGRESS)) {
		IPA_USB_ERR("failed to change state to resume_in_progress\n");
		result = -EFAULT;
		goto bad_params;
	}

	/* Request USB_PROD */
	result = ipa3_usb_request_prod();
	if (result)
		goto prod_req_fail;

	/* Start UL channel */
	result = ipa3_start_gsi_channel(ul_clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to start UL channel.\n");
		goto start_ul_fail;
	}

	if (prev_state != IPA_USB_SUSPEND_IN_PROGRESS) {
		/* Start DL channel */
		result = ipa3_start_gsi_channel(dl_clnt_hdl);
		if (result) {
			IPA_USB_ERR("failed to start DL channel.\n");
			goto start_dl_fail;
		}
	}

	/* Change ipa_usb state to CONNECTED */
	if (!ipa3_usb_set_state(IPA_USB_CONNECTED)) {
		IPA_USB_ERR("failed to change state to connected\n");
		result = -EFAULT;
		goto state_change_connected_fail;
	}

	IPA_USB_DBG("ipa3_usb_xdci_resume: exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

state_change_connected_fail:
	if (prev_state != IPA_USB_SUSPEND_IN_PROGRESS) {
		result = ipa3_stop_gsi_channel(ul_clnt_hdl);
		if (result)
			IPAERR("Error stopping UL channel: %d\n", result);
	}
start_dl_fail:
	result = ipa3_stop_gsi_channel(ul_clnt_hdl);
	if (result)
		IPAERR("Error stopping UL channel: %d\n", result);
start_ul_fail:
	ipa3_usb_release_prod();
prod_req_fail:
	/* Change ipa_usb state back to prev_state */
	if (!ipa3_usb_set_state(prev_state))
		IPA_USB_ERR("failed to change state back to %s\n",
			ipa3_usb_state_to_string(prev_state));
bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;
}
