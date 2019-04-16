/* Copyright (c) 2015-2018 The Linux Foundation. All rights reserved.
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

#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/ipa.h>
#include <linux/ipa_usb.h>
#include <linux/rndis_ipa.h>
#include <linux/ecm_ipa.h>
#include "../ipa_v3/ipa_i.h"
#include "../ipa_rm_i.h"

#define IPA_USB_RM_TIMEOUT_MSEC 10000
#define IPA_USB_DEV_READY_TIMEOUT_MSEC 10000

#define IPA_HOLB_TMR_EN 0x1

/* GSI channels weights */
#define IPA_USB_DL_CHAN_LOW_WEIGHT 0x5
#define IPA_USB_UL_CHAN_LOW_WEIGHT 0x4

#define IPA_USB_MAX_MSG_LEN 4096

#define IPA_USB_DRV_NAME "ipa_usb"

#define IPA_USB_DBG(fmt, args...) \
	do { \
		pr_debug(IPA_USB_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPA_USB_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_USB_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_USB_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(IPA_USB_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_USB_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_USB_ERR(fmt, args...) \
	do { \
		pr_err(IPA_USB_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPA_USB_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_USB_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_USB_INFO(fmt, args...) \
	do { \
		pr_info(IPA_USB_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPA_USB_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_USB_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

enum ipa_usb_direction {
	IPA_USB_DIR_UL,
	IPA_USB_DIR_DL,
};

struct ipa_usb_xdci_connect_params_internal {
	enum ipa_usb_max_usb_packet_size max_pkt_size;
	u32 ipa_to_usb_clnt_hdl;
	u8 ipa_to_usb_xferrscidx;
	bool ipa_to_usb_xferrscidx_valid;
	u32 usb_to_ipa_clnt_hdl;
	u8 usb_to_ipa_xferrscidx;
	bool usb_to_ipa_xferrscidx_valid;
	enum ipa_usb_teth_prot teth_prot;
	struct ipa_usb_teth_prot_params teth_prot_params;
	u32 max_supported_bandwidth_mbps;
};

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
	enum ipa3_usb_cons_state cons_state;
	/* consumer was requested*/
	bool cons_requested;
	/* consumer was requested and released before it was granted*/
	bool cons_requested_released;
};

struct ipa3_usb_pm_context {
	struct ipa_pm_register_params reg_params;
	struct work_struct *remote_wakeup_work;
	u32 hdl;
};

enum ipa3_usb_state {
	IPA_USB_INVALID,
	IPA_USB_INITIALIZED,
	IPA_USB_CONNECTED,
	IPA_USB_STOPPED,
	IPA_USB_SUSPEND_REQUESTED,
	IPA_USB_SUSPENDED,
	IPA_USB_SUSPENDED_NO_RWAKEUP,
	IPA_USB_RESUME_IN_PROGRESS
};

enum ipa3_usb_transport_type {
	IPA_USB_TRANSPORT_TETH,
	IPA_USB_TRANSPORT_DPL,
	IPA_USB_TRANSPORT_MAX
};

/* Get transport type from tethering protocol */
#define IPA3_USB_GET_TTYPE(__teth_prot) \
	(((__teth_prot) == IPA_USB_DIAG) ? \
	IPA_USB_TRANSPORT_DPL : IPA_USB_TRANSPORT_TETH)

/* Does the given transport type is DPL? */
#define IPA3_USB_IS_TTYPE_DPL(__ttype) \
	((__ttype) == IPA_USB_TRANSPORT_DPL)

struct ipa3_usb_teth_prot_conn_params {
	u32 usb_to_ipa_clnt_hdl;
	u32 ipa_to_usb_clnt_hdl;
	struct ipa_usb_teth_prot_params params;
};

/**
 * Transport type - could be either data tethering or DPL
 * Each transport has it's own RM resources and statuses
 */
struct ipa3_usb_transport_type_ctx {
	struct ipa3_usb_rm_context rm_ctx;
	struct ipa3_usb_pm_context pm_ctx;
	int (*ipa_usb_notify_cb)(enum ipa_usb_notify_event, void *user_data);
	void *user_data;
	enum ipa3_usb_state state;
	bool rwakeup_pending;
	struct ipa_usb_xdci_chan_params ul_ch_params;
	struct ipa_usb_xdci_chan_params dl_ch_params;
	struct ipa3_usb_teth_prot_conn_params teth_conn_params;
};

struct ipa3_usb_smmu_reg_map {
	int cnt;
	phys_addr_t addr;
};

struct ipa3_usb_context {
	struct ipa3_usb_teth_prot_context
		teth_prot_ctx[IPA_USB_MAX_TETH_PROT_SIZE];
	int num_init_prot; /* without dpl */
	struct teth_bridge_init_params teth_bridge_params;
	struct completion dev_ready_comp;
	u32 qmi_req_id;
	spinlock_t state_lock;
	bool dl_data_pending;
	struct workqueue_struct *wq;
	struct mutex general_mutex;
	struct ipa3_usb_transport_type_ctx
		ttype_ctx[IPA_USB_TRANSPORT_MAX];
	struct dentry *dfile_state_info;
	struct dentry *dent;
	struct ipa3_usb_smmu_reg_map smmu_reg_map;
};

enum ipa3_usb_op {
	IPA_USB_OP_INIT_TETH_PROT,
	IPA_USB_OP_REQUEST_CHANNEL,
	IPA_USB_OP_CONNECT,
	IPA_USB_OP_DISCONNECT,
	IPA_USB_OP_RELEASE_CHANNEL,
	IPA_USB_OP_DEINIT_TETH_PROT,
	IPA_USB_OP_SUSPEND,
	IPA_USB_OP_SUSPEND_NO_RWAKEUP,
	IPA_USB_OP_RESUME
};

struct ipa3_usb_status_dbg_info {
	const char *teth_state;
	const char *dpl_state;
	int num_init_prot;
	const char *inited_prots[IPA_USB_MAX_TETH_PROT_SIZE];
	const char *teth_connected_prot;
	const char *dpl_connected_prot;
	const char *teth_cons_state;
	const char *dpl_cons_state;
};

static void ipa3_usb_wq_notify_remote_wakeup(struct work_struct *work);
static void ipa3_usb_wq_dpl_notify_remote_wakeup(struct work_struct *work);
static DECLARE_WORK(ipa3_usb_notify_remote_wakeup_work,
	ipa3_usb_wq_notify_remote_wakeup);
static DECLARE_WORK(ipa3_usb_dpl_notify_remote_wakeup_work,
	ipa3_usb_wq_dpl_notify_remote_wakeup);

struct ipa3_usb_context *ipa3_usb_ctx;

static char *ipa3_usb_op_to_string(enum ipa3_usb_op op)
{
	switch (op) {
	case IPA_USB_OP_INIT_TETH_PROT:
		return "IPA_USB_OP_INIT_TETH_PROT";
	case IPA_USB_OP_REQUEST_CHANNEL:
		return "IPA_USB_OP_REQUEST_CHANNEL";
	case IPA_USB_OP_CONNECT:
		return "IPA_USB_OP_CONNECT";
	case IPA_USB_OP_DISCONNECT:
		return "IPA_USB_OP_DISCONNECT";
	case IPA_USB_OP_RELEASE_CHANNEL:
		return "IPA_USB_OP_RELEASE_CHANNEL";
	case IPA_USB_OP_DEINIT_TETH_PROT:
		return "IPA_USB_OP_DEINIT_TETH_PROT";
	case IPA_USB_OP_SUSPEND:
		return "IPA_USB_OP_SUSPEND";
	case IPA_USB_OP_SUSPEND_NO_RWAKEUP:
		return "IPA_USB_OP_SUSPEND_NO_RWAKEUP";
	case IPA_USB_OP_RESUME:
		return "IPA_USB_OP_RESUME";
	}

	return "UNSUPPORTED";
}

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
	case IPA_USB_SUSPENDED:
		return "IPA_USB_SUSPENDED";
	case IPA_USB_SUSPENDED_NO_RWAKEUP:
		return "IPA_USB_SUSPENDED_NO_RWAKEUP";
	case IPA_USB_RESUME_IN_PROGRESS:
		return "IPA_USB_RESUME_IN_PROGRESS";
	}

	return "UNSUPPORTED";
}

static char *ipa3_usb_notify_event_to_string(enum ipa_usb_notify_event event)
{
	switch (event) {
	case IPA_USB_DEVICE_READY:
		return "IPA_USB_DEVICE_READY";
	case IPA_USB_REMOTE_WAKEUP:
		return "IPA_USB_REMOTE_WAKEUP";
	case IPA_USB_SUSPEND_COMPLETED:
		return "IPA_USB_SUSPEND_COMPLETED";
	}

	return "UNSUPPORTED";
}

static bool ipa3_usb_get_teth_port_state(void)
{
	if (ipa3_usb_ctx == NULL)
		return false;
	if (ipa3_usb_ctx->ttype_ctx[IPA_USB_TRANSPORT_TETH].state ==
					IPA_USB_CONNECTED)
		return true;
	else
		return false;
}

static bool ipa3_usb_set_state(enum ipa3_usb_state new_state, bool err_permit,
	enum ipa3_usb_transport_type ttype)
{
	unsigned long flags;
	int state_legal = false;
	enum ipa3_usb_state state;
	bool rwakeup_pending;
	struct ipa3_usb_rm_context *rm_ctx;

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	state = ipa3_usb_ctx->ttype_ctx[ttype].state;
	rwakeup_pending = ipa3_usb_ctx->ttype_ctx[ttype].rwakeup_pending;
	switch (new_state) {
	case IPA_USB_INVALID:
		if (state == IPA_USB_INITIALIZED)
			state_legal = true;
		break;
	case IPA_USB_INITIALIZED:
		if (state == IPA_USB_STOPPED || state == IPA_USB_INVALID ||
			((!IPA3_USB_IS_TTYPE_DPL(ttype)) &&
			(state == IPA_USB_INITIALIZED)))
			state_legal = true;
		break;
	case IPA_USB_CONNECTED:
		if (state == IPA_USB_INITIALIZED ||
			state == IPA_USB_STOPPED ||
			state == IPA_USB_RESUME_IN_PROGRESS ||
			state == IPA_USB_SUSPENDED_NO_RWAKEUP ||
			/*
			 * In case of failure during suspend request
			 * handling, state is reverted to connected.
			 */
			(err_permit && state == IPA_USB_SUSPEND_REQUESTED))
			state_legal = true;
		break;
	case IPA_USB_STOPPED:
		if (state == IPA_USB_CONNECTED ||
			state == IPA_USB_SUSPENDED ||
			state == IPA_USB_SUSPENDED_NO_RWAKEUP)
			state_legal = true;
		break;
	case IPA_USB_SUSPEND_REQUESTED:
		if (state == IPA_USB_CONNECTED)
			state_legal = true;
		break;
	case IPA_USB_SUSPENDED:
		if (state == IPA_USB_SUSPEND_REQUESTED ||
			/*
			 * In case of failure during resume, state is reverted
			 * to original, which could be suspended. Allow it
			 */
			(err_permit && state == IPA_USB_RESUME_IN_PROGRESS)) {
			state_legal = true;
			rwakeup_pending = false;
		}
		break;
	case IPA_USB_SUSPENDED_NO_RWAKEUP:
		if (state == IPA_USB_CONNECTED)
			state_legal = true;
		break;
	case IPA_USB_RESUME_IN_PROGRESS:
		if (state == IPA_USB_SUSPENDED)
			state_legal = true;
		break;
	default:
		state_legal = false;
		break;

	}
	if (state_legal) {
		if (state != new_state) {
			IPA_USB_DBG("ipa_usb %s state changed %s -> %s\n",
				IPA3_USB_IS_TTYPE_DPL(ttype) ? "DPL" : "",
				ipa3_usb_state_to_string(state),
				ipa3_usb_state_to_string(new_state));
			ipa3_usb_ctx->ttype_ctx[ttype].state = new_state;
			ipa3_usb_ctx->ttype_ctx[ttype].rwakeup_pending =
				rwakeup_pending;
		}
	} else {
		IPA_USB_ERR("invalid state change %s -> %s\n",
			ipa3_usb_state_to_string(state),
			ipa3_usb_state_to_string(new_state));
	}

	if (!ipa_pm_is_used() &&
		state_legal && (new_state == IPA_USB_CONNECTED)) {
		rm_ctx = &ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx;
		if ((rm_ctx->cons_state == IPA_USB_CONS_GRANTED) ||
			rm_ctx->cons_requested_released) {
			rm_ctx->cons_requested = false;
			rm_ctx->cons_requested_released =
			false;
		}
		/* Notify RM that consumer is granted */
		if (rm_ctx->cons_requested) {
			ipa_rm_notify_completion(
				IPA_RM_RESOURCE_GRANTED,
				rm_ctx->cons_params.name);
			rm_ctx->cons_state = IPA_USB_CONS_GRANTED;
			rm_ctx->cons_requested = false;
		}
	}

	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
	return state_legal;
}

static bool ipa3_usb_check_legal_op(enum ipa3_usb_op op,
	enum ipa3_usb_transport_type ttype)
{
	unsigned long flags;
	bool is_legal = false;
	enum ipa3_usb_state state;
	bool is_dpl;

	if (ipa3_usb_ctx == NULL) {
		IPA_USB_ERR("ipa_usb_ctx is not initialized!\n");
		return false;
	}

	is_dpl = IPA3_USB_IS_TTYPE_DPL(ttype);

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	state = ipa3_usb_ctx->ttype_ctx[ttype].state;
	switch (op) {
	case IPA_USB_OP_INIT_TETH_PROT:
		if (state == IPA_USB_INVALID ||
			(!is_dpl && state == IPA_USB_INITIALIZED))
			is_legal = true;
		break;
	case IPA_USB_OP_REQUEST_CHANNEL:
		if (state == IPA_USB_INITIALIZED)
			is_legal = true;
		break;
	case IPA_USB_OP_CONNECT:
		if (state == IPA_USB_INITIALIZED || state == IPA_USB_STOPPED)
			is_legal = true;
		break;
	case IPA_USB_OP_DISCONNECT:
		if  (state == IPA_USB_CONNECTED ||
			state == IPA_USB_SUSPENDED ||
			state == IPA_USB_SUSPENDED_NO_RWAKEUP)
			is_legal = true;
		break;
	case IPA_USB_OP_RELEASE_CHANNEL:
		/* when releasing 1st channel state will be changed already */
		if (state == IPA_USB_STOPPED ||
			(!is_dpl && state == IPA_USB_INITIALIZED))
			is_legal = true;
		break;
	case IPA_USB_OP_DEINIT_TETH_PROT:
		/*
		 * For data tethering we should allow deinit an inited protocol
		 * always. E.g. rmnet is inited and rndis is connected.
		 * USB can deinit rmnet first and then disconnect rndis
		 * on cable disconnect.
		 */
		if (!is_dpl || state == IPA_USB_INITIALIZED)
			is_legal = true;
		break;
	case IPA_USB_OP_SUSPEND:
		if (state == IPA_USB_CONNECTED)
			is_legal = true;
		break;
	case IPA_USB_OP_SUSPEND_NO_RWAKEUP:
		if (state == IPA_USB_CONNECTED)
			is_legal = true;
		break;
	case IPA_USB_OP_RESUME:
		if (state == IPA_USB_SUSPENDED ||
			state == IPA_USB_SUSPENDED_NO_RWAKEUP)
			is_legal = true;
		break;
	default:
		is_legal = false;
		break;
	}

	if (!is_legal) {
		IPA_USB_ERR("Illegal %s operation: state=%s operation=%s\n",
			is_dpl ? "DPL" : "",
			ipa3_usb_state_to_string(state),
			ipa3_usb_op_to_string(op));
	}

	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
	return is_legal;
}

static void ipa3_usb_notify_do(enum ipa3_usb_transport_type ttype,
	enum ipa_usb_notify_event event)
{
	int (*cb)(enum ipa_usb_notify_event, void *user_data);
	void *user_data;
	int res;

	IPA_USB_DBG("Trying to notify USB with %s\n",
		ipa3_usb_notify_event_to_string(event));

	cb = ipa3_usb_ctx->ttype_ctx[ttype].ipa_usb_notify_cb;
	user_data = ipa3_usb_ctx->ttype_ctx[ttype].user_data;

	if (cb) {
		res = cb(event, user_data);
		IPA_USB_DBG("Notified USB with %s. is_dpl=%d result=%d\n",
			ipa3_usb_notify_event_to_string(event),
			IPA3_USB_IS_TTYPE_DPL(ttype), res);
	}
}

/*
 * This call-back is called from ECM or RNDIS drivers.
 * Both drivers are data tethering drivers and not DPL
 */
void ipa3_usb_device_ready_notify_cb(void)
{
	IPA_USB_DBG_LOW("entry\n");
	ipa3_usb_notify_do(IPA_USB_TRANSPORT_TETH,
		IPA_USB_DEVICE_READY);
	IPA_USB_DBG_LOW("exit\n");
}

static void ipa3_usb_prod_notify_cb_do(enum ipa_rm_event event,
		enum  ipa3_usb_transport_type ttype)
{
	struct ipa3_usb_rm_context *rm_ctx;

	IPA_USB_DBG_LOW("entry\n");

	rm_ctx = &ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx;

	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		IPA_USB_DBG(":%s granted\n",
			ipa_rm_resource_str(rm_ctx->prod_params.name));
		complete_all(&rm_ctx->prod_comp);
		break;
	case IPA_RM_RESOURCE_RELEASED:
		IPA_USB_DBG(":%s released\n",
			ipa_rm_resource_str(rm_ctx->prod_params.name));
		complete_all(&rm_ctx->prod_comp);
		break;
	}
	IPA_USB_DBG_LOW("exit\n");
}

static void ipa3_usb_prod_notify_cb(void *user_data, enum ipa_rm_event event,
			     unsigned long data)
{
	ipa3_usb_prod_notify_cb_do(event, IPA_USB_TRANSPORT_TETH);
}

static void ipa3_usb_dpl_dummy_prod_notify_cb(void *user_data,
		enum ipa_rm_event event, unsigned long data)
{
	ipa3_usb_prod_notify_cb_do(event, IPA_USB_TRANSPORT_TETH);
}

static void ipa3_usb_wq_notify_remote_wakeup(struct work_struct *work)
{
	bool rwakeup_pending;
	unsigned long flags;
	enum ipa3_usb_transport_type ttype =
		IPA_USB_TRANSPORT_TETH;

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	rwakeup_pending =
		ipa3_usb_ctx->ttype_ctx[ttype].rwakeup_pending;
	if (!rwakeup_pending) {
		rwakeup_pending = true;
		ipa3_usb_notify_do(ttype,
			IPA_USB_REMOTE_WAKEUP);
	}
	ipa3_usb_ctx->ttype_ctx[ttype].rwakeup_pending =
		rwakeup_pending;
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
}

static void ipa3_usb_wq_dpl_notify_remote_wakeup(struct work_struct *work)
{
	bool rwakeup_pending;
	unsigned long flags;
	enum ipa3_usb_transport_type ttype =
		IPA_USB_TRANSPORT_DPL;

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	rwakeup_pending =
		ipa3_usb_ctx->ttype_ctx[ttype].rwakeup_pending;
	if (!rwakeup_pending) {
		rwakeup_pending = true;
		ipa3_usb_notify_do(ttype,
			IPA_USB_REMOTE_WAKEUP);
	}
	ipa3_usb_ctx->ttype_ctx[ttype].rwakeup_pending =
		rwakeup_pending;
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
}

static int ipa3_usb_cons_request_resource_cb_do(
	enum ipa3_usb_transport_type ttype,
	struct work_struct *remote_wakeup_work)
{
	struct ipa3_usb_rm_context *rm_ctx;
	unsigned long flags;
	int result;

	IPA_USB_DBG_LOW("entry\n");
	rm_ctx = &ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx;
	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	IPA_USB_DBG("state is %s\n",
			ipa3_usb_state_to_string(
				ipa3_usb_ctx->ttype_ctx[ttype].state));
	switch (ipa3_usb_ctx->ttype_ctx[ttype].state) {
	case IPA_USB_CONNECTED:
	case IPA_USB_SUSPENDED_NO_RWAKEUP:
		rm_ctx->cons_state = IPA_USB_CONS_GRANTED;
		result = 0;
		break;
	case IPA_USB_SUSPEND_REQUESTED:
		rm_ctx->cons_requested = true;
		if (rm_ctx->cons_state == IPA_USB_CONS_GRANTED)
			result = 0;
		else
			result = -EINPROGRESS;
		break;
	case IPA_USB_SUSPENDED:
		if (!rm_ctx->cons_requested) {
			rm_ctx->cons_requested = true;
			queue_work(ipa3_usb_ctx->wq, remote_wakeup_work);
		}
		result = -EINPROGRESS;
		break;
	default:
		rm_ctx->cons_requested = true;
		result = -EINPROGRESS;
		break;
	}
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
	IPA_USB_DBG_LOW("exit with %d\n", result);
	return result;
}

static int ipa3_usb_cons_request_resource_cb(void)
{
	return ipa3_usb_cons_request_resource_cb_do(IPA_USB_TRANSPORT_TETH,
		&ipa3_usb_notify_remote_wakeup_work);
}

static int ipa3_usb_dpl_cons_request_resource_cb(void)
{
	return ipa3_usb_cons_request_resource_cb_do(IPA_USB_TRANSPORT_DPL,
		&ipa3_usb_dpl_notify_remote_wakeup_work);
}

static int ipa3_usb_cons_release_resource_cb_do(
	enum ipa3_usb_transport_type ttype)
{
	unsigned long flags;
	struct ipa3_usb_rm_context *rm_ctx;

	IPA_USB_DBG_LOW("entry\n");
	rm_ctx = &ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx;
	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	IPA_USB_DBG("state is %s\n",
			ipa3_usb_state_to_string(
			ipa3_usb_ctx->ttype_ctx[ttype].state));
	switch (ipa3_usb_ctx->ttype_ctx[ttype].state) {
	case IPA_USB_SUSPENDED:
		/* Proceed with the suspend if no DL/DPL data */
		if (rm_ctx->cons_requested)
			rm_ctx->cons_requested_released = true;
		break;
	case IPA_USB_SUSPEND_REQUESTED:
		if (rm_ctx->cons_requested)
			rm_ctx->cons_requested_released = true;
		break;
	case IPA_USB_STOPPED:
	case IPA_USB_RESUME_IN_PROGRESS:
	case IPA_USB_SUSPENDED_NO_RWAKEUP:
		if (rm_ctx->cons_requested)
			rm_ctx->cons_requested = false;
		break;
	case IPA_USB_CONNECTED:
	case IPA_USB_INITIALIZED:
		break;
	default:
		IPA_USB_ERR("received cons_release_cb in bad state: %s!\n",
			ipa3_usb_state_to_string(
				ipa3_usb_ctx->ttype_ctx[ttype].state));
		WARN_ON(1);
		break;
	}

	rm_ctx->cons_state = IPA_USB_CONS_RELEASED;
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
	IPA_USB_DBG_LOW("exit\n");
	return 0;
}

static int ipa3_usb_cons_release_resource_cb(void)
{
	return ipa3_usb_cons_release_resource_cb_do(IPA_USB_TRANSPORT_TETH);
}

static int ipa3_usb_dpl_cons_release_resource_cb(void)
{
	return ipa3_usb_cons_release_resource_cb_do(IPA_USB_TRANSPORT_DPL);
}

static void ipa3_usb_pm_cb(void *p, enum ipa_pm_cb_event event)
{
	struct ipa3_usb_transport_type_ctx *ttype_ctx =
		(struct ipa3_usb_transport_type_ctx *)p;
	unsigned long flags;

	IPA_USB_DBG_LOW("entry\n");

	if (event != IPA_PM_REQUEST_WAKEUP) {
		IPA_USB_ERR("Unexpected event %d\n", event);
		WARN_ON(1);
		return;
	}

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	IPA_USB_DBG("state is %s\n",
		ipa3_usb_state_to_string(ttype_ctx->state));
	if (ttype_ctx->state == IPA_USB_SUSPENDED)
		queue_work(ipa3_usb_ctx->wq,
			ttype_ctx->pm_ctx.remote_wakeup_work);
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
	IPA_USB_DBG_LOW("exit\n");
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
		return "dpl";
	default:
		break;
	}

	return "unsupported";
}

static char *ipa3_usb_teth_bridge_prot_to_string(
	enum ipa_usb_teth_prot teth_prot)
{
	switch (teth_prot) {
	case IPA_USB_RMNET:
		return "rmnet";
	case IPA_USB_MBIM:
		return "mbim";
	default:
		break;
	}

	return "unsupported";
}

static int ipa3_usb_init_teth_bridge(void)
{
	int result;

	result = teth_bridge_init(&ipa3_usb_ctx->teth_bridge_params);
	if (result) {
		IPA_USB_ERR("Failed to initialize teth_bridge\n");
		return result;
	}

	return 0;
}

static int ipa3_usb_register_pm(enum ipa3_usb_transport_type ttype)
{
	struct ipa3_usb_transport_type_ctx *ttype_ctx =
		&ipa3_usb_ctx->ttype_ctx[ttype];
	int result;

	/* there is one PM resource for teth and one for DPL */
	if (!IPA3_USB_IS_TTYPE_DPL(ttype) && ipa3_usb_ctx->num_init_prot > 0)
		return 0;

	memset(&ttype_ctx->pm_ctx.reg_params, 0,
		sizeof(ttype_ctx->pm_ctx.reg_params));
	ttype_ctx->pm_ctx.reg_params.name = (ttype == IPA_USB_TRANSPORT_DPL) ?
				"USB DPL" : "USB";
	ttype_ctx->pm_ctx.reg_params.callback = ipa3_usb_pm_cb;
	ttype_ctx->pm_ctx.reg_params.user_data = ttype_ctx;
	ttype_ctx->pm_ctx.reg_params.group = IPA_PM_GROUP_DEFAULT;

	result = ipa_pm_register(&ttype_ctx->pm_ctx.reg_params,
		&ttype_ctx->pm_ctx.hdl);
	if (result) {
		IPA_USB_ERR("fail to register with PM %d\n", result);
		goto fail_pm_reg;
	}

	result = ipa_pm_associate_ipa_cons_to_client(ttype_ctx->pm_ctx.hdl,
		(ttype == IPA_USB_TRANSPORT_DPL) ?
		IPA_CLIENT_USB_DPL_CONS : IPA_CLIENT_USB_CONS);
	if (result) {
		IPA_USB_ERR("fail to associate cons with PM %d\n", result);
		goto fail_pm_cons;
	}

	return 0;

fail_pm_cons:
	ipa_pm_deregister(ttype_ctx->pm_ctx.hdl);
fail_pm_reg:
	memset(&ttype_ctx->pm_ctx.reg_params, 0,
		sizeof(ttype_ctx->pm_ctx.reg_params));
	return result;
}

static int ipa3_usb_deregister_pm(enum ipa3_usb_transport_type ttype)
{
	struct ipa3_usb_pm_context *pm_ctx =
		&ipa3_usb_ctx->ttype_ctx[ttype].pm_ctx;
	int result;

	result = ipa_pm_deregister(pm_ctx->hdl);
	if (result)
		return result;

	memset(&pm_ctx->reg_params, 0, sizeof(pm_ctx->reg_params));
	return 0;
}

static int ipa3_usb_create_rm_resources(enum ipa3_usb_transport_type ttype)
{
	struct ipa3_usb_rm_context *rm_ctx;
	int result = -EFAULT;
	bool created = false;

	rm_ctx = &ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx;

	/* create PROD */
	if (!rm_ctx->prod_valid) {
		rm_ctx->prod_params.name = IPA3_USB_IS_TTYPE_DPL(ttype) ?
			IPA_RM_RESOURCE_USB_DPL_DUMMY_PROD :
			IPA_RM_RESOURCE_USB_PROD;
		rm_ctx->prod_params.floor_voltage = IPA_VOLTAGE_SVS2;
		rm_ctx->prod_params.reg_params.user_data = NULL;
		rm_ctx->prod_params.reg_params.notify_cb =
			IPA3_USB_IS_TTYPE_DPL(ttype) ?
			ipa3_usb_dpl_dummy_prod_notify_cb :
			ipa3_usb_prod_notify_cb;
		rm_ctx->prod_params.request_resource = NULL;
		rm_ctx->prod_params.release_resource = NULL;
		result = ipa_rm_create_resource(&rm_ctx->prod_params);
		if (result) {
			IPA_USB_ERR("Failed to create %s RM resource\n",
				ipa_rm_resource_str(rm_ctx->prod_params.name));
			return result;
		}
		rm_ctx->prod_valid = true;
		created = true;
		IPA_USB_DBG("Created %s RM resource\n",
			ipa_rm_resource_str(rm_ctx->prod_params.name));
	}

	/* Create CONS */
	if (!rm_ctx->cons_valid) {
		rm_ctx->cons_params.name = IPA3_USB_IS_TTYPE_DPL(ttype) ?
			IPA_RM_RESOURCE_USB_DPL_CONS :
			IPA_RM_RESOURCE_USB_CONS;
		rm_ctx->cons_params.floor_voltage = IPA_VOLTAGE_SVS2;
		rm_ctx->cons_params.reg_params.user_data = NULL;
		rm_ctx->cons_params.reg_params.notify_cb = NULL;
		rm_ctx->cons_params.request_resource =
			IPA3_USB_IS_TTYPE_DPL(ttype) ?
			ipa3_usb_dpl_cons_request_resource_cb :
			ipa3_usb_cons_request_resource_cb;
		rm_ctx->cons_params.release_resource =
			IPA3_USB_IS_TTYPE_DPL(ttype) ?
			ipa3_usb_dpl_cons_release_resource_cb :
			ipa3_usb_cons_release_resource_cb;
		result = ipa_rm_create_resource(&rm_ctx->cons_params);
		if (result) {
			IPA_USB_ERR("Failed to create %s RM resource\n",
				ipa_rm_resource_str(rm_ctx->cons_params.name));
			goto create_cons_rsc_fail;
		}
		rm_ctx->cons_valid = true;
		IPA_USB_DBG("Created %s RM resource\n",
			ipa_rm_resource_str(rm_ctx->cons_params.name));
	}

	return 0;

create_cons_rsc_fail:
	if (created) {
		rm_ctx->prod_valid = false;
		ipa_rm_delete_resource(rm_ctx->prod_params.name);
	}
	return result;
}

int ipa_usb_init_teth_prot(enum ipa_usb_teth_prot teth_prot,
			   struct ipa_usb_teth_params *teth_params,
			   int (*ipa_usb_notify_cb)(enum ipa_usb_notify_event,
			   void *),
			   void *user_data)
{
	int result = -EFAULT;
	enum ipa3_usb_transport_type ttype;
	struct ipa3_usb_teth_prot_context *teth_prot_ptr;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG_LOW("entry\n");
	if (teth_prot < 0 || teth_prot >= IPA_USB_MAX_TETH_PROT_SIZE ||
		((teth_prot == IPA_USB_RNDIS || teth_prot == IPA_USB_ECM) &&
		teth_params == NULL) || ipa_usb_notify_cb == NULL ||
		user_data == NULL) {
		IPA_USB_ERR("bad parameters\n");
		result = -EINVAL;
		goto bad_params;
	}

	ttype = IPA3_USB_GET_TTYPE(teth_prot);

	if (!ipa3_usb_check_legal_op(IPA_USB_OP_INIT_TETH_PROT, ttype)) {
		IPA_USB_ERR("Illegal operation\n");
		result = -EPERM;
		goto bad_params;
	}

	/* Create IPA RM USB resources */
	teth_prot_ptr = &ipa3_usb_ctx->teth_prot_ctx[teth_prot];
	if (ipa_pm_is_used())
		result = ipa3_usb_register_pm(ttype);
	else
		result = ipa3_usb_create_rm_resources(ttype);
	if (result) {
		IPA_USB_ERR("Failed creating IPA RM USB resources\n");
		goto bad_params;
	}

	if (!ipa3_usb_ctx->ttype_ctx[ttype].ipa_usb_notify_cb) {
		ipa3_usb_ctx->ttype_ctx[ttype].ipa_usb_notify_cb =
			ipa_usb_notify_cb;
	} else if (!IPA3_USB_IS_TTYPE_DPL(ttype)) {
		if (ipa3_usb_ctx->ttype_ctx[ttype].ipa_usb_notify_cb !=
			ipa_usb_notify_cb) {
			IPA_USB_ERR("Got different notify_cb\n");
			result = -EINVAL;
			goto bad_params;
		}
	} else {
		IPA_USB_ERR("Already has dpl_notify_cb\n");
		result = -EINVAL;
		goto bad_params;
	}

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
		if (teth_prot == IPA_USB_RNDIS) {
			struct ipa_usb_init_params *rndis_ptr =
				&teth_prot_ptr->teth_prot_params.rndis;

			rndis_ptr->device_ready_notify =
				ipa3_usb_device_ready_notify_cb;
			memcpy(rndis_ptr->host_ethaddr,
				teth_params->host_ethaddr,
				sizeof(teth_params->host_ethaddr));
			memcpy(rndis_ptr->device_ethaddr,
				teth_params->device_ethaddr,
				sizeof(teth_params->device_ethaddr));

			result = rndis_ipa_init(rndis_ptr);
			if (result) {
				IPA_USB_ERR("Failed to initialize %s\n",
					ipa3_usb_teth_prot_to_string(
					teth_prot));
				goto teth_prot_init_fail;
			}
		} else {
			struct ecm_ipa_params *ecm_ptr =
				&teth_prot_ptr->teth_prot_params.ecm;

			ecm_ptr->device_ready_notify =
				ipa3_usb_device_ready_notify_cb;
			memcpy(ecm_ptr->host_ethaddr,
				teth_params->host_ethaddr,
				sizeof(teth_params->host_ethaddr));
			memcpy(ecm_ptr->device_ethaddr,
				teth_params->device_ethaddr,
				sizeof(teth_params->device_ethaddr));

			result = ecm_ipa_init(ecm_ptr);
			if (result) {
				IPA_USB_ERR("Failed to initialize %s\n",
					ipa3_usb_teth_prot_to_string(
					teth_prot));
				goto teth_prot_init_fail;
			}
		}
		teth_prot_ptr->state =
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
			goto teth_prot_init_fail;
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].state =
			IPA_USB_TETH_PROT_INITIALIZED;
		ipa3_usb_ctx->num_init_prot++;
		IPA_USB_DBG("initialized %s %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot),
			ipa3_usb_teth_bridge_prot_to_string(teth_prot));
		break;
	case IPA_USB_DIAG:
		if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state !=
			IPA_USB_TETH_PROT_INVALID) {
			IPA_USB_DBG("DPL already initialized\n");
			result = -EPERM;
			goto bad_params;
		}
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].user_data = user_data;
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].state =
			IPA_USB_TETH_PROT_INITIALIZED;
		IPA_USB_DBG("initialized DPL\n");
		break;
	default:
		IPA_USB_ERR("unexpected tethering protocol\n");
		result = -EINVAL;
		goto bad_params;
	}

	if (!ipa3_usb_set_state(IPA_USB_INITIALIZED, false, ttype))
		IPA_USB_ERR("failed to change state to initialized\n");

	IPA_USB_DBG_LOW("exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

teth_prot_init_fail:
	if ((IPA3_USB_IS_TTYPE_DPL(ttype))
		|| (ipa3_usb_ctx->num_init_prot == 0)) {
		if (ipa_pm_is_used()) {
			ipa3_usb_deregister_pm(ttype);
		} else {
			ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx.prod_valid =
				false;
			ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx.cons_valid =
				false;
			ipa_rm_delete_resource(
			ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx.prod_params.name);
			ipa_rm_delete_resource(
			ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx.cons_params.name);
		}
	}
bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;
}
EXPORT_SYMBOL(ipa_usb_init_teth_prot);

void ipa3_usb_gsi_evt_err_cb(struct gsi_evt_err_notify *notify)
{
	IPA_USB_DBG_LOW("entry\n");
	if (!notify)
		return;
	IPA_USB_ERR("Received event error %d, description: %d\n",
		notify->evt_id, notify->err_desc);
	IPA_USB_DBG_LOW("exit\n");
}

void ipa3_usb_gsi_chan_err_cb(struct gsi_chan_err_notify *notify)
{
	IPA_USB_DBG_LOW("entry\n");
	if (!notify)
		return;
	IPA_USB_ERR("Received channel error %d, description: %d\n",
		notify->evt_id, notify->err_desc);
	IPA_USB_DBG_LOW("exit\n");
}

static bool ipa3_usb_check_chan_params(struct ipa_usb_xdci_chan_params *params)
{
	IPA_USB_DBG_LOW("gevntcount_low_addr = %x\n",
			params->gevntcount_low_addr);
	IPA_USB_DBG_LOW("gevntcount_hi_addr = %x\n",
			params->gevntcount_hi_addr);
	IPA_USB_DBG_LOW("dir = %d\n", params->dir);
	IPA_USB_DBG_LOW("xfer_ring_len = %d\n", params->xfer_ring_len);
	IPA_USB_DBG_LOW("last_trb_addr_iova = %x\n",
		params->xfer_scratch.last_trb_addr_iova);
	IPA_USB_DBG_LOW("const_buffer_size = %d\n",
		params->xfer_scratch.const_buffer_size);
	IPA_USB_DBG_LOW("depcmd_low_addr = %x\n",
		params->xfer_scratch.depcmd_low_addr);
	IPA_USB_DBG_LOW("depcmd_hi_addr = %x\n",
		params->xfer_scratch.depcmd_hi_addr);

	if (params->client >= IPA_CLIENT_MAX  ||
		params->teth_prot < 0 ||
		params->teth_prot >= IPA_USB_MAX_TETH_PROT_SIZE ||
		params->xfer_ring_len % GSI_CHAN_RE_SIZE_16B ||
		params->xfer_scratch.const_buffer_size < 1 ||
		params->xfer_scratch.const_buffer_size > 31) {
		IPA_USB_ERR("Invalid params\n");
		return false;
	}
	switch (params->teth_prot) {
	case IPA_USB_DIAG:
		if (!IPA_CLIENT_IS_CONS(params->client)) {
			IPA_USB_ERR("DPL supports only DL channel\n");
			return false;
		}
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
				ipa3_usb_teth_bridge_prot_to_string(
				params->teth_prot));
			return false;
		}
		break;
	default:
		IPA_USB_ERR("Unknown tethering protocol (%d)\n",
			params->teth_prot);
		return false;
	}
	return true;
}

static int ipa3_usb_smmu_map_xdci_channel(
	struct ipa_usb_xdci_chan_params *params, bool map)
{
	int result;
	u32 gevntcount_r = rounddown(params->gevntcount_low_addr, PAGE_SIZE);
	u32 xfer_scratch_r =
		rounddown(params->xfer_scratch.depcmd_low_addr, PAGE_SIZE);

	if (gevntcount_r != xfer_scratch_r) {
		IPA_USB_ERR("No support more than 1 page map for USB regs\n");
		WARN_ON(1);
		return -EINVAL;
	}

	if (map) {
		if (ipa3_usb_ctx->smmu_reg_map.cnt == 0) {
			ipa3_usb_ctx->smmu_reg_map.addr = gevntcount_r;
			result = ipa3_smmu_map_peer_reg(
				ipa3_usb_ctx->smmu_reg_map.addr, true,
				IPA_SMMU_CB_AP);
			if (result) {
				IPA_USB_ERR("failed to map USB regs %d\n",
					result);
				return result;
			}
		} else {
			if (gevntcount_r != ipa3_usb_ctx->smmu_reg_map.addr) {
				IPA_USB_ERR(
					"No support for map different reg\n");
				return -EINVAL;
			}
		}
		ipa3_usb_ctx->smmu_reg_map.cnt++;
	} else {
		if (gevntcount_r != ipa3_usb_ctx->smmu_reg_map.addr) {
			IPA_USB_ERR(
				"No support for map different reg\n");
			return -EINVAL;
		}

		if (ipa3_usb_ctx->smmu_reg_map.cnt == 1) {
			result = ipa3_smmu_map_peer_reg(
				ipa3_usb_ctx->smmu_reg_map.addr, false,
				IPA_SMMU_CB_AP);
			if (result) {
				IPA_USB_ERR("failed to unmap USB regs %d\n",
					result);
				return result;
			}
		}
		ipa3_usb_ctx->smmu_reg_map.cnt--;
	}


	result = ipa3_smmu_map_peer_buff(params->xfer_ring_base_addr_iova,
		params->xfer_ring_len, map, params->sgt_xfer_rings,
		IPA_SMMU_CB_AP);
	if (result) {
		IPA_USB_ERR("failed to map Xfer ring %d\n", result);
		return result;
	}

	result = ipa3_smmu_map_peer_buff(params->data_buff_base_addr_iova,
		params->data_buff_base_len, map, params->sgt_data_buff,
		IPA_SMMU_CB_AP);
	if (result) {
		IPA_USB_ERR("failed to map TRBs buff %d\n", result);
		return result;
	}

	return 0;
}

static int ipa3_usb_request_xdci_channel(
	struct ipa_usb_xdci_chan_params *params,
	enum ipa_usb_direction dir,
	struct ipa_req_chan_out_params *out_params)
{
	int result = -EFAULT;
	struct ipa_request_gsi_channel_params chan_params;
	enum ipa3_usb_transport_type ttype;
	enum ipa_usb_teth_prot teth_prot;
	struct ipa_usb_init_params *rndis_ptr;
	struct ecm_ipa_params *ecm_ptr;
	struct ipa_usb_xdci_chan_params *xdci_ch_params;

	IPA_USB_DBG_LOW("entry\n");
	if (params == NULL || out_params == NULL ||
		!ipa3_usb_check_chan_params(params)) {
		IPA_USB_ERR("bad parameters\n");
		return -EINVAL;
	}

	ttype = IPA3_USB_GET_TTYPE(params->teth_prot);
	teth_prot = params->teth_prot;

	if (!ipa3_usb_check_legal_op(IPA_USB_OP_REQUEST_CHANNEL, ttype)) {
		IPA_USB_ERR("Illegal operation\n");
		return -EPERM;
	}

	rndis_ptr =
		&ipa3_usb_ctx->teth_prot_ctx[teth_prot].teth_prot_params.rndis;
	ecm_ptr =
		&ipa3_usb_ctx->teth_prot_ctx[teth_prot].teth_prot_params.ecm;

	memset(&chan_params, 0, sizeof(struct ipa_request_gsi_channel_params));
	memcpy(&chan_params.ipa_ep_cfg, &params->ipa_ep_cfg,
		sizeof(struct ipa_ep_cfg));
	chan_params.client = params->client;
	switch (params->teth_prot) {
	case IPA_USB_RNDIS:
		chan_params.priv = rndis_ptr->private;
		if (params->dir == GSI_CHAN_DIR_FROM_GSI)
			chan_params.notify = rndis_ptr->ipa_tx_notify;
		else
			chan_params.notify = rndis_ptr->ipa_rx_notify;
		chan_params.skip_ep_cfg = rndis_ptr->skip_ep_cfg;
		break;
	case IPA_USB_ECM:
		chan_params.priv = ecm_ptr->private;
		if (params->dir == GSI_CHAN_DIR_FROM_GSI)
			chan_params.notify = ecm_ptr->ecm_ipa_tx_dp_notify;
		else
			chan_params.notify = ecm_ptr->ecm_ipa_rx_dp_notify;
		chan_params.skip_ep_cfg = ecm_ptr->skip_ep_cfg;
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

	result = ipa3_usb_smmu_map_xdci_channel(params, true);
	if (result) {
		IPA_USB_ERR("failed to smmu map %d\n", result);
		return result;
	}

	/* store channel params for SMMU unmap */
	if (dir == IPA_USB_DIR_UL)
		xdci_ch_params = &ipa3_usb_ctx->ttype_ctx[ttype].ul_ch_params;
	else
		xdci_ch_params = &ipa3_usb_ctx->ttype_ctx[ttype].dl_ch_params;

	*xdci_ch_params = *params;
	result = ipa_smmu_store_sgt(
		&xdci_ch_params->sgt_xfer_rings,
		params->sgt_xfer_rings);
	if (result)
		return result;

	result = ipa_smmu_store_sgt(
		&xdci_ch_params->sgt_data_buff,
		params->sgt_data_buff);
	if (result) {
		ipa_smmu_free_sgt(&xdci_ch_params->sgt_xfer_rings);
		return result;
	}
	chan_params.keep_ipa_awake = params->keep_ipa_awake;
	chan_params.evt_ring_params.intf = GSI_EVT_CHTYPE_XDCI_EV;
	chan_params.evt_ring_params.intr = GSI_INTR_IRQ;
	chan_params.evt_ring_params.re_size = GSI_EVT_RING_RE_SIZE_16B;
	chan_params.evt_ring_params.ring_len = params->xfer_ring_len -
		chan_params.evt_ring_params.re_size;
	chan_params.evt_ring_params.ring_base_addr =
		params->xfer_ring_base_addr_iova;
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
		params->xfer_ring_base_addr_iova;
	chan_params.chan_params.ring_base_vaddr = NULL;
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		chan_params.chan_params.use_db_eng = GSI_CHAN_DIRECT_MODE;
	else
		chan_params.chan_params.use_db_eng = GSI_CHAN_DB_MODE;
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
		params->xfer_scratch.last_trb_addr_iova;
	/* xferrscidx will be updated later */
	chan_params.chan_scratch.xdci.xferrscidx = 0;
	chan_params.chan_scratch.xdci.const_buffer_size =
		params->xfer_scratch.const_buffer_size;
	chan_params.chan_scratch.xdci.depcmd_low_addr =
		params->xfer_scratch.depcmd_low_addr;
	chan_params.chan_scratch.xdci.depcmd_hi_addr =
		params->xfer_scratch.depcmd_hi_addr;

	/*
	 * Update scratch for MCS smart prefetch:
	 * Starting IPA4.5, smart prefetch implemented by H/W.
	 * At IPA 4.0/4.1/4.2, we do not use MCS smart prefetch
	 *  so keep the fields zero.
	 */
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		chan_params.chan_scratch.xdci.outstanding_threshold =
		((params->teth_prot == IPA_USB_MBIM) ? 1 : 2) *
		chan_params.chan_params.re_size;
	}
	/* max_outstanding_tre is set in ipa3_request_gsi_channel() */

	result = ipa3_request_gsi_channel(&chan_params, out_params);
	if (result) {
		IPA_USB_ERR("failed to allocate GSI channel\n");
		ipa3_usb_smmu_map_xdci_channel(params, false);
		return result;
	}

	IPA_USB_DBG_LOW("exit\n");
	return 0;
}

static int ipa3_usb_release_xdci_channel(u32 clnt_hdl,
	enum ipa_usb_direction dir,
	enum ipa3_usb_transport_type ttype)
{
	int result = 0;
	struct ipa_usb_xdci_chan_params *xdci_ch_params;

	IPA_USB_DBG_LOW("entry\n");
	if (ttype < 0 || ttype >= IPA_USB_TRANSPORT_MAX) {
		IPA_USB_ERR("bad parameter\n");
		return -EINVAL;
	}

	if (!ipa3_usb_check_legal_op(IPA_USB_OP_RELEASE_CHANNEL, ttype)) {
		IPA_USB_ERR("Illegal operation\n");
		return -EPERM;
	}

	/* Release channel */
	result = ipa3_release_gsi_channel(clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to deallocate channel\n");
		return result;
	}

	if (dir == IPA_USB_DIR_UL)
		xdci_ch_params = &ipa3_usb_ctx->ttype_ctx[ttype].ul_ch_params;
	else
		xdci_ch_params = &ipa3_usb_ctx->ttype_ctx[ttype].dl_ch_params;

	result = ipa3_usb_smmu_map_xdci_channel(xdci_ch_params, false);

	if (xdci_ch_params->sgt_xfer_rings != NULL)
		ipa_smmu_free_sgt(&xdci_ch_params->sgt_xfer_rings);
	if (xdci_ch_params->sgt_data_buff != NULL)
		ipa_smmu_free_sgt(&xdci_ch_params->sgt_data_buff);

	/* Change ipa_usb state to INITIALIZED */
	if (!ipa3_usb_set_state(IPA_USB_INITIALIZED, false, ttype))
		IPA_USB_ERR("failed to change state to initialized\n");

	IPA_USB_DBG_LOW("exit\n");
	return 0;
}

static int ipa3_usb_request_prod(enum ipa3_usb_transport_type ttype)
{
	int result;
	struct ipa3_usb_rm_context *rm_ctx;
	const char *rsrc_str;

	rm_ctx = &ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx;
	rsrc_str = ipa_rm_resource_str(rm_ctx->prod_params.name);

	IPA_USB_DBG_LOW("requesting %s\n", rsrc_str);
	init_completion(&rm_ctx->prod_comp);
	result = ipa_rm_request_resource(rm_ctx->prod_params.name);
	if (result) {
		if (result != -EINPROGRESS) {
			IPA_USB_ERR("failed to request %s: %d\n",
				rsrc_str, result);
			return result;
		}
		result = wait_for_completion_timeout(&rm_ctx->prod_comp,
				msecs_to_jiffies(IPA_USB_RM_TIMEOUT_MSEC));
		if (result == 0) {
			IPA_USB_ERR("timeout request %s\n", rsrc_str);
			return -ETIME;
		}
	}

	IPA_USB_DBG_LOW("%s granted\n", rsrc_str);
	return 0;
}

static int ipa3_usb_release_prod(enum ipa3_usb_transport_type ttype)
{
	int result;
	struct ipa3_usb_rm_context *rm_ctx;
	const char *rsrc_str;

	rm_ctx = &ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx;
	rsrc_str = ipa_rm_resource_str(rm_ctx->prod_params.name);

	IPA_USB_DBG_LOW("releasing %s\n", rsrc_str);

	init_completion(&rm_ctx->prod_comp);
	result = ipa_rm_release_resource(rm_ctx->prod_params.name);
	if (result) {
		if (result != -EINPROGRESS) {
			IPA_USB_ERR("failed to release %s: %d\n",
				rsrc_str, result);
			return result;
		}
		result = wait_for_completion_timeout(&rm_ctx->prod_comp,
			msecs_to_jiffies(IPA_USB_RM_TIMEOUT_MSEC));
		if (result == 0) {
			IPA_USB_ERR("timeout release %s\n", rsrc_str);
			return -ETIME;
		}
	}

	IPA_USB_DBG_LOW("%s released\n", rsrc_str);
	return 0;
}

static bool ipa3_usb_check_connect_params(
	struct ipa_usb_xdci_connect_params_internal *params)
{
	IPA_USB_DBG_LOW("ul xferrscidx = %d\n", params->usb_to_ipa_xferrscidx);
	IPA_USB_DBG_LOW("dl xferrscidx = %d\n", params->ipa_to_usb_xferrscidx);
	IPA_USB_DBG_LOW("max_supported_bandwidth_mbps = %d\n",
		params->max_supported_bandwidth_mbps);

	if (params->max_pkt_size < IPA_USB_FULL_SPEED_64B  ||
		params->max_pkt_size > IPA_USB_SUPER_SPEED_1024B  ||
		params->ipa_to_usb_xferrscidx > 127 ||
		(params->teth_prot != IPA_USB_DIAG &&
		(params->usb_to_ipa_xferrscidx > 127)) ||
		params->teth_prot < 0 ||
		params->teth_prot >= IPA_USB_MAX_TETH_PROT_SIZE) {
		IPA_USB_ERR("Invalid params\n");
		return false;
	}

	if (ipa3_usb_ctx->teth_prot_ctx[params->teth_prot].state ==
		IPA_USB_TETH_PROT_INVALID) {
		IPA_USB_ERR("%s is not initialized\n",
			ipa3_usb_teth_prot_to_string(
			params->teth_prot));
		return false;
	}

	return true;
}

static int ipa3_usb_connect_teth_bridge(
	struct teth_bridge_connect_params *params)
{
	int result;

	result = teth_bridge_connect(params);
	if (result) {
		IPA_USB_ERR("failed to connect teth_bridge (%s)\n",
			params->tethering_mode == TETH_TETHERING_MODE_RMNET ?
			"rmnet" : "mbim");
		return result;
	}

	return 0;
}

static int ipa3_usb_connect_dpl(void)
{
	int res = 0;

	if (ipa_pm_is_used())
		return 0;

	/*
	 * Add DPL dependency to RM dependency graph, first add_dependency call
	 * is sync in order to make sure the IPA clocks are up before we
	 * continue and notify the USB driver it may continue.
	 */
	res = ipa_rm_add_dependency_sync(IPA_RM_RESOURCE_USB_DPL_DUMMY_PROD,
				    IPA_RM_RESOURCE_Q6_CONS);
	if (res < 0) {
		IPA_USB_ERR("ipa_rm_add_dependency_sync() failed\n");
		return res;
	}

	/*
	 * this add_dependency call can't be sync since it will block until DPL
	 * status is connected (which can happen only later in the flow),
	 * the clocks are already up so the call doesn't need to block.
	 */
	res = ipa_rm_add_dependency(IPA_RM_RESOURCE_Q6_PROD,
				    IPA_RM_RESOURCE_USB_DPL_CONS);
	if (res < 0 && res != -EINPROGRESS) {
		IPA_USB_ERR("ipa_rm_add_dependency() failed\n");
		ipa_rm_delete_dependency(IPA_RM_RESOURCE_USB_DPL_DUMMY_PROD,
				IPA_RM_RESOURCE_Q6_CONS);
		return res;
	}

	return 0;
}

static int ipa3_usb_connect_teth_prot(enum ipa_usb_teth_prot teth_prot)
{
	int result;
	struct teth_bridge_connect_params teth_bridge_params;
	struct ipa3_usb_teth_prot_conn_params *teth_conn_params;
	enum ipa3_usb_transport_type ttype;
	struct ipa3_usb_teth_prot_context *teth_prot_ptr =
		&ipa3_usb_ctx->teth_prot_ctx[teth_prot];

	IPA_USB_DBG("connecting protocol = %s\n",
		ipa3_usb_teth_prot_to_string(teth_prot));

	ttype = IPA3_USB_GET_TTYPE(teth_prot);

	teth_conn_params = &(ipa3_usb_ctx->ttype_ctx[ttype].teth_conn_params);

	switch (teth_prot) {
	case IPA_USB_RNDIS:
		if (teth_prot_ptr->state ==
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s is already connected\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			break;
		}
		ipa3_usb_ctx->ttype_ctx[ttype].user_data =
			teth_prot_ptr->user_data;
		result = rndis_ipa_pipe_connect_notify(
			teth_conn_params->usb_to_ipa_clnt_hdl,
			teth_conn_params->ipa_to_usb_clnt_hdl,
			teth_conn_params->params.max_xfer_size_bytes_to_dev,
			teth_conn_params->params.max_packet_number_to_dev,
			teth_conn_params->params.max_xfer_size_bytes_to_host,
			teth_prot_ptr->teth_prot_params.rndis.private);
		if (result) {
			IPA_USB_ERR("failed to connect %s\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			ipa3_usb_ctx->ttype_ctx[ttype].user_data = NULL;
			return result;
		}
		teth_prot_ptr->state =
			IPA_USB_TETH_PROT_CONNECTED;
		IPA_USB_DBG("%s is connected\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	case IPA_USB_ECM:
		if (teth_prot_ptr->state ==
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s is already connected\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			break;
		}
		ipa3_usb_ctx->ttype_ctx[ttype].user_data =
			teth_prot_ptr->user_data;
		result = ecm_ipa_connect(teth_conn_params->usb_to_ipa_clnt_hdl,
			teth_conn_params->ipa_to_usb_clnt_hdl,
			teth_prot_ptr->teth_prot_params.ecm.private);
		if (result) {
			IPA_USB_ERR("failed to connect %s\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			ipa3_usb_ctx->ttype_ctx[ttype].user_data = NULL;
			return result;
		}
		teth_prot_ptr->state =
			IPA_USB_TETH_PROT_CONNECTED;
		IPA_USB_DBG("%s is connected\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	case IPA_USB_RMNET:
	case IPA_USB_MBIM:
		if (teth_prot_ptr->state ==
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s is already connected\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			break;
		}
		result = ipa3_usb_init_teth_bridge();
		if (result)
			return result;

		ipa3_usb_ctx->ttype_ctx[ttype].user_data =
			teth_prot_ptr->user_data;
		teth_bridge_params.ipa_usb_pipe_hdl =
			teth_conn_params->ipa_to_usb_clnt_hdl;
		teth_bridge_params.usb_ipa_pipe_hdl =
			teth_conn_params->usb_to_ipa_clnt_hdl;
		teth_bridge_params.tethering_mode =
			(teth_prot == IPA_USB_RMNET) ?
			(TETH_TETHERING_MODE_RMNET):(TETH_TETHERING_MODE_MBIM);
		teth_bridge_params.client_type = IPA_CLIENT_USB_PROD;
		result = ipa3_usb_connect_teth_bridge(&teth_bridge_params);
		if (result) {
			ipa3_usb_ctx->ttype_ctx[ttype].user_data = NULL;
			return result;
		}
		ipa3_usb_ctx->teth_prot_ctx[teth_prot].state =
			IPA_USB_TETH_PROT_CONNECTED;
		ipa3_usb_notify_do(ttype, IPA_USB_DEVICE_READY);
		IPA_USB_DBG("%s (%s) is connected\n",
			ipa3_usb_teth_prot_to_string(teth_prot),
			ipa3_usb_teth_bridge_prot_to_string(teth_prot));
		break;
	case IPA_USB_DIAG:
		if (ipa3_usb_ctx->teth_prot_ctx[IPA_USB_DIAG].state ==
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s is already connected\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			break;
		}

		ipa3_usb_ctx->ttype_ctx[ttype].user_data =
			ipa3_usb_ctx->teth_prot_ctx[teth_prot].user_data;
		result = ipa3_usb_connect_dpl();
		if (result) {
			IPA_USB_ERR("Failed connecting DPL result=%d\n",
				result);
			ipa3_usb_ctx->ttype_ctx[ttype].user_data = NULL;
			return result;
		}
		ipa3_usb_ctx->teth_prot_ctx[IPA_USB_DIAG].state =
			IPA_USB_TETH_PROT_CONNECTED;
		ipa3_usb_notify_do(ttype, IPA_USB_DEVICE_READY);
		IPA_USB_DBG("%s is connected\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	default:
		IPA_USB_ERR("Invalid tethering protocol\n");
		return -EFAULT;
	}

	return 0;
}

static int ipa3_usb_disconnect_teth_bridge(void)
{
	int result;

	result = teth_bridge_disconnect(IPA_CLIENT_USB_PROD);
	if (result) {
		IPA_USB_ERR("failed to disconnect teth_bridge\n");
		return result;
	}

	return 0;
}

static int ipa3_usb_disconnect_dpl(void)
{
	int res;

	if (ipa_pm_is_used())
		return 0;

	/* Remove DPL RM dependency */
	res = ipa_rm_delete_dependency(IPA_RM_RESOURCE_USB_DPL_DUMMY_PROD,
				    IPA_RM_RESOURCE_Q6_CONS);
	if (res)
		IPA_USB_ERR("deleting DPL_DUMMY_PROD rsrc dependency fail\n");

	res = ipa_rm_delete_dependency(IPA_RM_RESOURCE_Q6_PROD,
				 IPA_RM_RESOURCE_USB_DPL_CONS);
	if (res)
		IPA_USB_ERR("deleting DPL_CONS rsrc dependencty fail\n");

	return 0;
}

static int ipa3_usb_disconnect_teth_prot(enum ipa_usb_teth_prot teth_prot)
{
	int result = 0;
	enum ipa3_usb_transport_type ttype;
	struct ipa3_usb_teth_prot_context *teth_prot_ptr =
		&ipa3_usb_ctx->teth_prot_ctx[teth_prot];

	ttype = IPA3_USB_GET_TTYPE(teth_prot);

	switch (teth_prot) {
	case IPA_USB_RNDIS:
	case IPA_USB_ECM:
		if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state !=
			IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s is not connected\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			return -EPERM;
		}
		if (teth_prot == IPA_USB_RNDIS) {
			result = rndis_ipa_pipe_disconnect_notify(
				teth_prot_ptr->teth_prot_params.rndis.private);
		} else {
			result = ecm_ipa_disconnect(
				teth_prot_ptr->teth_prot_params.ecm.private);
		}
		if (result) {
			IPA_USB_ERR("failed to disconnect %s\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			break;
		}
		teth_prot_ptr->state = IPA_USB_TETH_PROT_INITIALIZED;
		IPA_USB_DBG("disconnected %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	case IPA_USB_RMNET:
	case IPA_USB_MBIM:
		if (teth_prot_ptr->state != IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s (%s) is not connected\n",
				ipa3_usb_teth_prot_to_string(teth_prot),
				ipa3_usb_teth_bridge_prot_to_string(teth_prot));
			return -EPERM;
		}
		result = ipa3_usb_disconnect_teth_bridge();
		if (result)
			break;

		teth_prot_ptr->state = IPA_USB_TETH_PROT_INITIALIZED;
		IPA_USB_DBG("disconnected %s (%s)\n",
			ipa3_usb_teth_prot_to_string(teth_prot),
			ipa3_usb_teth_bridge_prot_to_string(teth_prot));
		break;
	case IPA_USB_DIAG:
		if (teth_prot_ptr->state != IPA_USB_TETH_PROT_CONNECTED) {
			IPA_USB_DBG("%s is not connected\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			return -EPERM;
		}
		result = ipa3_usb_disconnect_dpl();
		if (result)
			break;
		teth_prot_ptr->state = IPA_USB_TETH_PROT_INITIALIZED;
		IPA_USB_DBG("disconnected %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	default:
		break;
	}

	ipa3_usb_ctx->ttype_ctx[ttype].user_data = NULL;
	return result;
}

static int ipa3_usb_xdci_connect_internal(
	struct ipa_usb_xdci_connect_params_internal *params)
{
	int result = -EFAULT;
	struct ipa_rm_perf_profile profile;
	enum ipa3_usb_transport_type ttype;
	struct ipa3_usb_teth_prot_conn_params *teth_prot_ptr;
	struct ipa3_usb_rm_context *rm_ctx_ptr;
	struct ipa3_usb_transport_type_ctx *t_ctx_ptr;

	IPA_USB_DBG_LOW("entry\n");
	if (params == NULL || !ipa3_usb_check_connect_params(params)) {
		IPA_USB_ERR("bad parameters\n");
		return -EINVAL;
	}

	ttype = (params->teth_prot == IPA_USB_DIAG) ? IPA_USB_TRANSPORT_DPL :
		IPA_USB_TRANSPORT_TETH;

	if (!ipa3_usb_check_legal_op(IPA_USB_OP_CONNECT, ttype)) {
		IPA_USB_ERR("Illegal operation\n");
		return -EPERM;
	}

	teth_prot_ptr = &ipa3_usb_ctx->ttype_ctx[ttype].teth_conn_params;
	teth_prot_ptr->ipa_to_usb_clnt_hdl = params->ipa_to_usb_clnt_hdl;
	rm_ctx_ptr = &ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx;

	if (!IPA3_USB_IS_TTYPE_DPL(ttype))
		teth_prot_ptr->usb_to_ipa_clnt_hdl =
		params->usb_to_ipa_clnt_hdl;
	teth_prot_ptr->params = params->teth_prot_params;

	/* Set EE xDCI specific scratch */
	result = ipa3_set_usb_max_packet_size(params->max_pkt_size);
	if (result) {
		IPA_USB_ERR("failed setting xDCI EE scratch field\n");
		return result;
	}

	if (ipa_pm_is_used()) {
		/* perf profile is not set on  USB DPL pipe */
		if (ttype != IPA_USB_TRANSPORT_DPL) {
			result = ipa_pm_set_throughput(
				ipa3_usb_ctx->ttype_ctx[ttype].pm_ctx.hdl,
				params->max_supported_bandwidth_mbps);
			if (result) {
				IPA_USB_ERR("failed to set perf profile\n");
				return result;
			}
		}

		result = ipa_pm_activate_sync(
			ipa3_usb_ctx->ttype_ctx[ttype].pm_ctx.hdl);
		if (result) {
			IPA_USB_ERR("failed to activate pm\n");
			return result;
		}
	} else {
		/* Set RM PROD & CONS perf profile */
		profile.max_supported_bandwidth_mbps =
				params->max_supported_bandwidth_mbps;
		result = ipa_rm_set_perf_profile(
			ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx.prod_params.name,
			&profile);

		t_ctx_ptr = &ipa3_usb_ctx->ttype_ctx[ttype];

		if (result) {
			IPA_USB_ERR("failed to set %s perf profile\n",
				ipa_rm_resource_str(
					t_ctx_ptr->rm_ctx.prod_params.name));
			return result;
		}

		result = ipa_rm_set_perf_profile(
			ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx.cons_params.name,
			&profile);

		if (result) {
			IPA_USB_ERR("failed to set %s perf profile\n",
				ipa_rm_resource_str(
					t_ctx_ptr->rm_ctx.cons_params.name));
			return result;
		}

		/* Request PROD */
		result = ipa3_usb_request_prod(ttype);
		if (result)
			return result;
	}

	if (params->teth_prot != IPA_USB_DIAG) {
		/* Start UL channel */
		result = ipa3_xdci_start(params->usb_to_ipa_clnt_hdl,
			params->usb_to_ipa_xferrscidx,
			params->usb_to_ipa_xferrscidx_valid);
		if (result) {
			IPA_USB_ERR("failed to connect UL channel\n");
			goto connect_ul_fail;
		}
	}

	/* Start DL/DPL channel */
	result = ipa3_xdci_start(params->ipa_to_usb_clnt_hdl,
		params->ipa_to_usb_xferrscidx,
		params->ipa_to_usb_xferrscidx_valid);
	if (result) {
		IPA_USB_ERR("failed to connect DL/DPL channel\n");
		goto connect_dl_fail;
	}

	/* Connect tethering protocol */
	result = ipa3_usb_connect_teth_prot(params->teth_prot);
	if (result) {
		IPA_USB_ERR("failed to connect teth protocol\n");
		goto connect_teth_prot_fail;
	}

	if (!ipa3_usb_set_state(IPA_USB_CONNECTED, false, ttype)) {
		IPA_USB_ERR(
			"failed to change state to connected\n");
		goto state_change_connected_fail;
	}

	IPA_USB_DBG_LOW("exit\n");
	return 0;

state_change_connected_fail:
	ipa3_usb_disconnect_teth_prot(params->teth_prot);
connect_teth_prot_fail:
	ipa3_xdci_disconnect(params->ipa_to_usb_clnt_hdl, false, -1);
	ipa3_reset_gsi_channel(params->ipa_to_usb_clnt_hdl);
	ipa3_reset_gsi_event_ring(params->ipa_to_usb_clnt_hdl);
connect_dl_fail:
	if (params->teth_prot != IPA_USB_DIAG) {
		ipa3_xdci_disconnect(params->usb_to_ipa_clnt_hdl, false, -1);
		ipa3_reset_gsi_channel(params->usb_to_ipa_clnt_hdl);
		ipa3_reset_gsi_event_ring(params->usb_to_ipa_clnt_hdl);
	}
connect_ul_fail:
	if (ipa_pm_is_used())
		ipa_pm_deactivate_sync(
			ipa3_usb_ctx->ttype_ctx[ttype].pm_ctx.hdl);
	else
		ipa3_usb_release_prod(ttype);
	return result;
}

#ifdef CONFIG_DEBUG_FS
static char dbg_buff[IPA_USB_MAX_MSG_LEN];

static char *ipa3_usb_cons_state_to_string(enum ipa3_usb_cons_state state)
{
	switch (state) {
	case IPA_USB_CONS_GRANTED:
		return "CONS_GRANTED";
	case IPA_USB_CONS_RELEASED:
		return "CONS_RELEASED";
	}

	return "UNSUPPORTED";
}

static int ipa3_usb_get_status_dbg_info(struct ipa3_usb_status_dbg_info *status)
{
	int res;
	int i;
	unsigned long flags;
	struct ipa3_usb_rm_context *rm_ctx_ptr;

	IPA_USB_DBG_LOW("entry\n");

	if (ipa3_usb_ctx == NULL) {
		IPA_USB_ERR("IPA USB was not inited yet\n");
		return -EFAULT;
	}

	mutex_lock(&ipa3_usb_ctx->general_mutex);

	if (!status) {
		IPA_USB_ERR("Invalid input\n");
		res = -EINVAL;
		goto bail;
	}

	memset(status, 0, sizeof(struct ipa3_usb_status_dbg_info));

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	rm_ctx_ptr = &ipa3_usb_ctx->ttype_ctx[IPA_USB_TRANSPORT_TETH].rm_ctx;
	status->teth_state = ipa3_usb_state_to_string(
		ipa3_usb_ctx->ttype_ctx[IPA_USB_TRANSPORT_TETH].state);
	status->dpl_state = ipa3_usb_state_to_string(
		ipa3_usb_ctx->ttype_ctx[IPA_USB_TRANSPORT_DPL].state);
	if (rm_ctx_ptr->cons_valid)
		status->teth_cons_state = ipa3_usb_cons_state_to_string(
			rm_ctx_ptr->cons_state);
	rm_ctx_ptr = &ipa3_usb_ctx->ttype_ctx[IPA_USB_TRANSPORT_DPL].rm_ctx;
	if (rm_ctx_ptr->cons_valid)
		status->dpl_cons_state = ipa3_usb_cons_state_to_string(
			rm_ctx_ptr->cons_state);
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);

	for (i = 0 ; i < IPA_USB_MAX_TETH_PROT_SIZE ; i++) {
		if (ipa3_usb_ctx->teth_prot_ctx[i].state ==
			IPA_USB_TETH_PROT_INITIALIZED) {
			if ((i == IPA_USB_RMNET) || (i == IPA_USB_MBIM))
				status->inited_prots[status->num_init_prot++] =
					ipa3_usb_teth_bridge_prot_to_string(i);
			else
				status->inited_prots[status->num_init_prot++] =
					ipa3_usb_teth_prot_to_string(i);
		} else if (ipa3_usb_ctx->teth_prot_ctx[i].state ==
			IPA_USB_TETH_PROT_CONNECTED) {
			switch (i) {
			case IPA_USB_RMNET:
			case IPA_USB_MBIM:
				status->teth_connected_prot =
					ipa3_usb_teth_bridge_prot_to_string(i);
				break;
			case IPA_USB_DIAG:
				status->dpl_connected_prot =
					ipa3_usb_teth_prot_to_string(i);
				break;
			default:
				status->teth_connected_prot =
					ipa3_usb_teth_prot_to_string(i);
			}
		}
	}

	res = 0;
	IPA_USB_DBG_LOW("exit\n");
bail:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return res;
}

static ssize_t ipa3_read_usb_state_info(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct ipa3_usb_status_dbg_info status;
	int result;
	int nbytes;
	int cnt = 0;
	int i;

	result = ipa3_usb_get_status_dbg_info(&status);
	if (result) {
		nbytes = scnprintf(dbg_buff, IPA_USB_MAX_MSG_LEN,
				"Fail to read IPA USB status\n");
		cnt += nbytes;
	} else {
		nbytes = scnprintf(dbg_buff, IPA_USB_MAX_MSG_LEN,
			"Tethering Data State: %s\n"
			"DPL State: %s\n"
			"Protocols in Initialized State: ",
			status.teth_state,
			status.dpl_state);
		cnt += nbytes;

		for (i = 0 ; i < status.num_init_prot ; i++) {
			nbytes = scnprintf(dbg_buff + cnt,
					IPA_USB_MAX_MSG_LEN - cnt,
					"%s ", status.inited_prots[i]);
			cnt += nbytes;
		}
		nbytes = scnprintf(dbg_buff + cnt, IPA_USB_MAX_MSG_LEN - cnt,
				status.num_init_prot ? "\n" : "None\n");
		cnt += nbytes;

		nbytes = scnprintf(dbg_buff + cnt, IPA_USB_MAX_MSG_LEN - cnt,
				"Protocols in Connected State: ");
		cnt += nbytes;
		if (status.teth_connected_prot) {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_USB_MAX_MSG_LEN - cnt,
				"%s ", status.teth_connected_prot);
			cnt += nbytes;
		}
		if (status.dpl_connected_prot) {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_USB_MAX_MSG_LEN - cnt,
				"%s ", status.dpl_connected_prot);
			cnt += nbytes;
		}
		nbytes = scnprintf(dbg_buff + cnt, IPA_USB_MAX_MSG_LEN - cnt,
				(status.teth_connected_prot ||
				status.dpl_connected_prot) ? "\n" : "None\n");
		cnt += nbytes;

		nbytes = scnprintf(dbg_buff + cnt, IPA_USB_MAX_MSG_LEN - cnt,
				"USB Tethering Consumer State: %s\n",
				status.teth_cons_state ?
				status.teth_cons_state : "Invalid");
		cnt += nbytes;

		nbytes = scnprintf(dbg_buff + cnt, IPA_USB_MAX_MSG_LEN - cnt,
				"DPL Consumer State: %s\n",
				status.dpl_cons_state ? status.dpl_cons_state :
				"Invalid");
		cnt += nbytes;
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}

const struct file_operations ipa3_ipa_usb_ops = {
	.read = ipa3_read_usb_state_info,
};

static void ipa_usb_debugfs_init(void)
{
	const mode_t read_only_mode = 0444;

	ipa3_usb_ctx->dent = debugfs_create_dir("ipa_usb", 0);
	if (IS_ERR(ipa3_usb_ctx->dent)) {
		pr_err("fail to create folder in debug_fs\n");
		return;
	}

	ipa3_usb_ctx->dfile_state_info = debugfs_create_file("state_info",
			read_only_mode, ipa3_usb_ctx->dent, 0,
			&ipa3_ipa_usb_ops);
	if (!ipa3_usb_ctx->dfile_state_info ||
		IS_ERR(ipa3_usb_ctx->dfile_state_info)) {
		pr_err("failed to create file for state_info\n");
		goto fail;
	}

	return;

fail:
	debugfs_remove_recursive(ipa3_usb_ctx->dent);
	ipa3_usb_ctx->dent = NULL;
}

static void ipa_usb_debugfs_remove(void)
{
	if (IS_ERR(ipa3_usb_ctx->dent)) {
		IPA_USB_ERR("ipa_usb debugfs folder was not created\n");
		return;
	}

	debugfs_remove_recursive(ipa3_usb_ctx->dent);
}
#else /* CONFIG_DEBUG_FS */
static void ipa_usb_debugfs_init(void){}
static void ipa_usb_debugfs_remove(void){}
#endif /* CONFIG_DEBUG_FS */

static int ipa_usb_set_lock_unlock(bool is_lock)
{
	IPA_USB_DBG("entry\n");
	if (is_lock)
		mutex_lock(&ipa3_usb_ctx->general_mutex);
	else
		mutex_unlock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG("exit\n");

	return 0;
}



int ipa_usb_xdci_connect(struct ipa_usb_xdci_chan_params *ul_chan_params,
			 struct ipa_usb_xdci_chan_params *dl_chan_params,
			 struct ipa_req_chan_out_params *ul_out_params,
			 struct ipa_req_chan_out_params *dl_out_params,
			 struct ipa_usb_xdci_connect_params *connect_params)
{
	int result = -EFAULT;
	struct ipa_usb_xdci_connect_params_internal conn_params;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG_LOW("entry\n");
	if (connect_params == NULL || dl_chan_params == NULL ||
		dl_out_params == NULL ||
		(connect_params->teth_prot != IPA_USB_DIAG &&
		(ul_chan_params == NULL || ul_out_params == NULL))) {
		IPA_USB_ERR("bad parameters\n");
		result = -EINVAL;
		goto bad_params;
	}

	if (connect_params->teth_prot != IPA_USB_DIAG) {
		result = ipa3_usb_request_xdci_channel(ul_chan_params,
			IPA_USB_DIR_UL, ul_out_params);
		if (result) {
			IPA_USB_ERR("failed to allocate UL channel\n");
			goto bad_params;
		}
	}

	result = ipa3_usb_request_xdci_channel(dl_chan_params, IPA_USB_DIR_DL,
		dl_out_params);
	if (result) {
		IPA_USB_ERR("failed to allocate DL/DPL channel\n");
		goto alloc_dl_chan_fail;
	}

	memset(&conn_params, 0,
		sizeof(struct ipa_usb_xdci_connect_params_internal));
	conn_params.max_pkt_size = connect_params->max_pkt_size;
	conn_params.ipa_to_usb_clnt_hdl = dl_out_params->clnt_hdl;
	conn_params.ipa_to_usb_xferrscidx =
		connect_params->ipa_to_usb_xferrscidx;
	conn_params.ipa_to_usb_xferrscidx_valid =
		connect_params->ipa_to_usb_xferrscidx_valid;
	if (connect_params->teth_prot != IPA_USB_DIAG) {
		conn_params.usb_to_ipa_clnt_hdl = ul_out_params->clnt_hdl;
		conn_params.usb_to_ipa_xferrscidx =
			connect_params->usb_to_ipa_xferrscidx;
		conn_params.usb_to_ipa_xferrscidx_valid =
			connect_params->usb_to_ipa_xferrscidx_valid;
	}
	conn_params.teth_prot = connect_params->teth_prot;
	conn_params.teth_prot_params = connect_params->teth_prot_params;
	conn_params.max_supported_bandwidth_mbps =
		connect_params->max_supported_bandwidth_mbps;
	result = ipa3_usb_xdci_connect_internal(&conn_params);
	if (result) {
		IPA_USB_ERR("failed to connect\n");
		goto connect_fail;
	}

	/*
	 * Register for xdci lock/unlock callback with ipa core driver.
	 * As per use case, only register for IPA_CONS end point for now.
	 * If needed we can include the same for IPA_PROD ep.
	 * For IPA_USB_DIAG/DPL config there will not be any UL ep.
	 */
	if (connect_params->teth_prot != IPA_USB_DIAG)
		ipa3_register_client_callback(&ipa_usb_set_lock_unlock,
			&ipa3_usb_get_teth_port_state, ul_out_params->clnt_hdl);

	IPA_USB_DBG_LOW("exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

connect_fail:
	ipa3_usb_release_xdci_channel(dl_out_params->clnt_hdl, IPA_USB_DIR_DL,
		IPA3_USB_GET_TTYPE(dl_chan_params->teth_prot));
alloc_dl_chan_fail:
	if (connect_params->teth_prot != IPA_USB_DIAG)
		ipa3_usb_release_xdci_channel(ul_out_params->clnt_hdl,
			IPA_USB_DIR_UL,
			IPA3_USB_GET_TTYPE(ul_chan_params->teth_prot));
bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;
}
EXPORT_SYMBOL(ipa_usb_xdci_connect);

static int ipa3_usb_check_disconnect_prot(enum ipa_usb_teth_prot teth_prot)
{
	if (teth_prot < 0 || teth_prot >= IPA_USB_MAX_TETH_PROT_SIZE) {
		IPA_USB_ERR("bad parameter\n");
		return -EFAULT;
	}

	if (ipa3_usb_ctx->teth_prot_ctx[teth_prot].state !=
		IPA_USB_TETH_PROT_CONNECTED) {
		IPA_USB_ERR("%s is not connected\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		return -EFAULT;
	}

	return 0;
}

/* Assumes lock already acquired */
static int ipa_usb_xdci_dismiss_channels(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
				enum ipa_usb_teth_prot teth_prot)
{
	int result = 0;
	enum ipa3_usb_transport_type ttype;

	ttype = IPA3_USB_GET_TTYPE(teth_prot);

	IPA_USB_DBG_LOW("entry\n");

	/* Reset DL channel */
	result = ipa3_reset_gsi_channel(dl_clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to reset DL channel\n");
		return result;
	}

	/* Reset DL event ring */
	result = ipa3_reset_gsi_event_ring(dl_clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to reset DL event ring\n");
		return result;
	}

	if (!IPA3_USB_IS_TTYPE_DPL(ttype)) {
		ipa3_xdci_ep_delay_rm(ul_clnt_hdl); /* Remove ep_delay if set */
		/* Reset UL channel */
		result = ipa3_reset_gsi_channel(ul_clnt_hdl);
		if (result) {
			IPA_USB_ERR("failed to reset UL channel\n");
			return result;
		}

		/* Reset UL event ring */
		result = ipa3_reset_gsi_event_ring(ul_clnt_hdl);
		if (result) {
			IPA_USB_ERR("failed to reset UL event ring\n");
			return result;
		}
	}

	/*
	 * Deregister for xdci lock/unlock callback from ipa core driver.
	 * As per use case, only deregister for IPA_CONS end point for now.
	 * If needed we can include the same for IPA_PROD ep.
	 * For IPA_USB_DIAG/DPL config there will not be any UL config.
	 */
	if (!IPA3_USB_IS_TTYPE_DPL(ttype))
		ipa3_deregister_client_callback(ul_clnt_hdl);

	/* Change state to STOPPED */
	if (!ipa3_usb_set_state(IPA_USB_STOPPED, false, ttype))
		IPA_USB_ERR("failed to change state to stopped\n");

	if (!IPA3_USB_IS_TTYPE_DPL(ttype)) {
		result = ipa3_usb_release_xdci_channel(ul_clnt_hdl,
			IPA_USB_DIR_UL, ttype);
		if (result) {
			IPA_USB_ERR("failed to release UL channel\n");
			return result;
		}
	}

	result = ipa3_usb_release_xdci_channel(dl_clnt_hdl,
		IPA_USB_DIR_DL, ttype);
	if (result) {
		IPA_USB_ERR("failed to release DL channel\n");
		return result;
	}

	IPA_USB_DBG_LOW("exit\n");

	return 0;
}

int ipa_usb_xdci_disconnect(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
			    enum ipa_usb_teth_prot teth_prot)
{
	int result = 0;
	struct ipa_ep_cfg_holb holb_cfg;
	unsigned long flags;
	enum ipa3_usb_state orig_state;
	enum ipa3_usb_transport_type ttype;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG_LOW("entry\n");

	ttype = IPA3_USB_GET_TTYPE(teth_prot);

	if (!ipa3_usb_check_legal_op(IPA_USB_OP_DISCONNECT, ttype)) {
		IPA_USB_ERR("Illegal operation\n");
		result = -EPERM;
		goto bad_params;
	}

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	if (ipa3_usb_ctx->ttype_ctx[ttype].state ==
		IPA_USB_SUSPENDED_NO_RWAKEUP) {
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
		result = ipa_usb_xdci_dismiss_channels(ul_clnt_hdl, dl_clnt_hdl,
			teth_prot);
		mutex_unlock(&ipa3_usb_ctx->general_mutex);
		return result;
	}

	if (ipa3_usb_check_disconnect_prot(teth_prot)) {
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
		result = -EINVAL;
		goto bad_params;
	}

	if (ipa3_usb_ctx->ttype_ctx[ttype].state != IPA_USB_SUSPENDED) {
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
		/* Stop DL/DPL channel */
		result = ipa3_xdci_disconnect(dl_clnt_hdl, false, -1);
		if (result) {
			IPA_USB_ERR("failed to disconnect DL/DPL channel\n");
			goto bad_params;
		}
	} else {
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
		memset(&holb_cfg, 0, sizeof(holb_cfg));
		holb_cfg.en = IPA_HOLB_TMR_EN;
		holb_cfg.tmr_val = 0;
		ipa3_cfg_ep_holb(dl_clnt_hdl, &holb_cfg);
	}

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	orig_state = ipa3_usb_ctx->ttype_ctx[ttype].state;
	if (!IPA3_USB_IS_TTYPE_DPL(ttype)) {
		if (orig_state != IPA_USB_SUSPENDED) {
			spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock,
				flags);
			/* Stop UL channel */
			result = ipa3_xdci_disconnect(ul_clnt_hdl,
				true,
				ipa3_usb_ctx->qmi_req_id);
			if (result) {
				IPA_USB_ERR("failed disconnect UL channel\n");
				goto bad_params;
			}
			ipa3_usb_ctx->qmi_req_id++;
		} else
			spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock,
				flags);
	} else
		spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);

	result = ipa_usb_xdci_dismiss_channels(ul_clnt_hdl, dl_clnt_hdl,
			teth_prot);
	if (result)
		goto bad_params;

	/* Disconnect tethering protocol */
	result = ipa3_usb_disconnect_teth_prot(teth_prot);
	if (result)
		goto bad_params;

	if (orig_state != IPA_USB_SUSPENDED) {
		if (ipa_pm_is_used())
			result = ipa_pm_deactivate_sync(
				ipa3_usb_ctx->ttype_ctx[ttype].pm_ctx.hdl);
		else
			result = ipa3_usb_release_prod(ttype);
		if (result) {
			IPA_USB_ERR("failed to release PROD\n");
			goto bad_params;
		}
	}

	IPA_USB_DBG_LOW("exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;

}
EXPORT_SYMBOL(ipa_usb_xdci_disconnect);

int ipa_usb_deinit_teth_prot(enum ipa_usb_teth_prot teth_prot)
{
	int result = -EFAULT;
	enum ipa3_usb_transport_type ttype;
	struct ipa3_usb_teth_prot_context *teth_prot_ptr;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG_LOW("entry\n");
	if (teth_prot < 0 || teth_prot >= IPA_USB_MAX_TETH_PROT_SIZE) {
		IPA_USB_ERR("bad parameters\n");
		result = -EINVAL;
		goto bad_params;
	}

	ttype = IPA3_USB_GET_TTYPE(teth_prot);

	if (!ipa3_usb_check_legal_op(IPA_USB_OP_DEINIT_TETH_PROT, ttype)) {
		IPA_USB_ERR("Illegal operation\n");
		result = -EPERM;
		goto bad_params;
	}

	/* Clean-up tethering protocol */
	teth_prot_ptr = &ipa3_usb_ctx->teth_prot_ctx[teth_prot];

	switch (teth_prot) {
	case IPA_USB_RNDIS:
	case IPA_USB_ECM:
		if (teth_prot_ptr->state !=
			IPA_USB_TETH_PROT_INITIALIZED) {
			IPA_USB_ERR("%s is not initialized\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			result = -EINVAL;
			goto bad_params;
		}
		if (teth_prot == IPA_USB_RNDIS)
			rndis_ipa_cleanup(
				teth_prot_ptr->teth_prot_params.rndis.private);
		else
			ecm_ipa_cleanup(
				teth_prot_ptr->teth_prot_params.ecm.private);
		teth_prot_ptr->user_data = NULL;
		teth_prot_ptr->state = IPA_USB_TETH_PROT_INVALID;
		ipa3_usb_ctx->num_init_prot--;
		IPA_USB_DBG("deinitialized %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	case IPA_USB_RMNET:
	case IPA_USB_MBIM:
		if (teth_prot_ptr->state !=
			IPA_USB_TETH_PROT_INITIALIZED) {
			IPA_USB_ERR("%s (%s) is not initialized\n",
				ipa3_usb_teth_prot_to_string(teth_prot),
				ipa3_usb_teth_bridge_prot_to_string(teth_prot));
			result = -EINVAL;
			goto bad_params;
		}

		teth_prot_ptr->user_data = NULL;
		teth_prot_ptr->state = IPA_USB_TETH_PROT_INVALID;
		ipa3_usb_ctx->num_init_prot--;
		IPA_USB_DBG("deinitialized %s (%s)\n",
			ipa3_usb_teth_prot_to_string(teth_prot),
			ipa3_usb_teth_bridge_prot_to_string(teth_prot));
		break;
	case IPA_USB_DIAG:
		if (teth_prot_ptr->state !=
			IPA_USB_TETH_PROT_INITIALIZED) {
			IPA_USB_ERR("%s is not initialized\n",
				ipa3_usb_teth_prot_to_string(teth_prot));
			result = -EINVAL;
			goto bad_params;
		}
		teth_prot_ptr->user_data = NULL;
		teth_prot_ptr->state = IPA_USB_TETH_PROT_INVALID;
		IPA_USB_DBG("deinitialized %s\n",
			ipa3_usb_teth_prot_to_string(teth_prot));
		break;
	default:
		IPA_USB_ERR("unexpected tethering protocol\n");
		result = -EINVAL;
		goto bad_params;
	}

	if (IPA3_USB_IS_TTYPE_DPL(ttype) ||
		(ipa3_usb_ctx->num_init_prot == 0)) {
		if (!ipa3_usb_set_state(IPA_USB_INVALID, false, ttype))
			IPA_USB_ERR(
				"failed to change state to invalid\n");
		if (ipa_pm_is_used()) {
			ipa3_usb_deregister_pm(ttype);
			ipa3_usb_ctx->ttype_ctx[ttype].ipa_usb_notify_cb = NULL;
		} else {
			ipa_rm_delete_resource(
			ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx.prod_params.name);
			ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx.prod_valid =
				false;
			ipa_rm_delete_resource(
			ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx.cons_params.name);
			ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx.cons_valid =
				false;
			ipa3_usb_ctx->ttype_ctx[ttype].ipa_usb_notify_cb = NULL;
		}
	}

	IPA_USB_DBG_LOW("exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;
}
EXPORT_SYMBOL(ipa_usb_deinit_teth_prot);

/* Assumes lock already acquired */
static int ipa3_usb_suspend_no_remote_wakeup(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot)
{
	int result = 0;
	enum ipa3_usb_transport_type ttype;

	ttype = IPA3_USB_GET_TTYPE(teth_prot);

	if (!ipa3_usb_check_legal_op(IPA_USB_OP_SUSPEND_NO_RWAKEUP, ttype)) {
		IPA_USB_ERR("Illegal operation\n");
		result = -EPERM;
		goto fail_exit;
	}

	IPA_USB_DBG("Start suspend with no remote wakeup sequence: %s\n",
		IPA3_USB_IS_TTYPE_DPL(ttype) ?
		"DPL channel":"Data Tethering channels");

	if (ipa3_usb_check_disconnect_prot(teth_prot)) {
		result = -EINVAL;
		goto fail_exit;
	}

	/* Stop DL/DPL channel */
	result = ipa3_xdci_disconnect(dl_clnt_hdl, false, -1);
	if (result) {
		IPA_USB_ERR("failed to disconnect DL/DPL channel\n");
		goto fail_exit;
	}

	if (!IPA3_USB_IS_TTYPE_DPL(ttype)) {
		/* Stop UL channel */
		result = ipa3_xdci_disconnect(ul_clnt_hdl, true,
			ipa3_usb_ctx->qmi_req_id);
		if (result) {
			IPA_USB_ERR("failed disconnect UL channel\n");
			goto start_dl;
		}
		ipa3_usb_ctx->qmi_req_id++;
	}

	/* Disconnect tethering protocol */
	result = ipa3_usb_disconnect_teth_prot(teth_prot);
	if (result)
		goto start_ul;

	if (ipa_pm_is_used())
		result = ipa_pm_deactivate_sync(
			ipa3_usb_ctx->ttype_ctx[ttype].pm_ctx.hdl);
	else
		result = ipa3_usb_release_prod(ttype);
	if (result) {
		IPA_USB_ERR("failed to release PROD\n");
		goto connect_teth;
	}

	/* Change ipa_usb state to SUSPENDED_NO_RWAKEUP */
	if (!ipa3_usb_set_state(IPA_USB_SUSPENDED_NO_RWAKEUP, false, ttype))
		IPA_USB_ERR("failed to change state to suspend no rwakeup\n");

	IPA_USB_DBG_LOW("exit\n");
	return 0;

connect_teth:
	(void)ipa3_usb_connect_teth_prot(teth_prot);
start_ul:
	if (!IPA3_USB_IS_TTYPE_DPL(ttype))
		(void)ipa3_xdci_connect(ul_clnt_hdl);
start_dl:
	(void)ipa3_xdci_connect(dl_clnt_hdl);
fail_exit:
	return result;
}

int ipa_usb_xdci_suspend(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot, bool with_remote_wakeup)
{
	int result = 0;
	unsigned long flags;
	enum ipa3_usb_transport_type ttype;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG_LOW("entry\n");

	if (teth_prot < 0 || teth_prot >= IPA_USB_MAX_TETH_PROT_SIZE) {
		IPA_USB_ERR("bad parameters\n");
		result = -EINVAL;
		goto bad_params;
	}

	if (!with_remote_wakeup) {
		result = ipa3_usb_suspend_no_remote_wakeup(ul_clnt_hdl,
			dl_clnt_hdl, teth_prot);
		mutex_unlock(&ipa3_usb_ctx->general_mutex);
		return result;
	}

	ttype = IPA3_USB_GET_TTYPE(teth_prot);

	if (!ipa3_usb_check_legal_op(IPA_USB_OP_SUSPEND, ttype)) {
		IPA_USB_ERR("Illegal operation\n");
		result = -EPERM;
		goto bad_params;
	}

	IPA_USB_DBG("Start suspend sequence: %s\n",
		IPA3_USB_IS_TTYPE_DPL(ttype) ?
		"DPL channel":"Data Tethering channels");

	/* Change state to SUSPEND_REQUESTED */
	if (!ipa3_usb_set_state(IPA_USB_SUSPEND_REQUESTED, false, ttype)) {
		IPA_USB_ERR(
			"fail changing state to suspend_req\n");
		result = -EFAULT;
		goto bad_params;
	}

	/* Stop UL channel & suspend DL/DPL EP */
	result = ipa3_xdci_suspend(ul_clnt_hdl, dl_clnt_hdl,
		true,
		ipa3_usb_ctx->qmi_req_id, IPA3_USB_IS_TTYPE_DPL(ttype));
	if (result) {
		IPA_USB_ERR("failed to suspend\n");
		goto suspend_fail;
	}
	ipa3_usb_ctx->qmi_req_id++;

	if (ipa_pm_is_used())
		result = ipa_pm_deactivate_sync(
			ipa3_usb_ctx->ttype_ctx[ttype].pm_ctx.hdl);
	else
		result = ipa3_usb_release_prod(ttype);
	if (result) {
		IPA_USB_ERR("failed to release PROD\n");
		goto release_prod_fail;
	}

	/* Check if DL/DPL data pending */
	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	if (ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx.cons_state ==
		IPA_USB_CONS_GRANTED &&
		ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx.cons_requested) {

		IPA_USB_DBG("DL/DPL data pending, invoke remote wakeup\n");
		queue_work(ipa3_usb_ctx->wq,
			IPA3_USB_IS_TTYPE_DPL(ttype) ?
			&ipa3_usb_dpl_notify_remote_wakeup_work :
			&ipa3_usb_notify_remote_wakeup_work);
	}
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);

	/* Change state to SUSPENDED */
	if (!ipa3_usb_set_state(IPA_USB_SUSPENDED, false, ttype))
		IPA_USB_ERR("failed to change state to suspended\n");

	/* Check if DL/DPL data pending */
	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	if (ipa3_usb_ctx->ttype_ctx[ttype].rm_ctx.cons_requested) {
		IPA_USB_DBG_LOW(
			"DL/DPL data is pending, invoking remote wakeup\n");
		queue_work(ipa3_usb_ctx->wq, IPA3_USB_IS_TTYPE_DPL(ttype) ?
			&ipa3_usb_dpl_notify_remote_wakeup_work :
			&ipa3_usb_notify_remote_wakeup_work);
	}
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);

	IPA_USB_DBG_LOW("exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

release_prod_fail:
	ipa3_xdci_resume(ul_clnt_hdl, dl_clnt_hdl,
		IPA3_USB_IS_TTYPE_DPL(ttype));
suspend_fail:
	/* Change state back to CONNECTED */
	if (!ipa3_usb_set_state(IPA_USB_CONNECTED, true, ttype))
		IPA_USB_ERR("failed to change state back to connected\n");
bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;
}
EXPORT_SYMBOL(ipa_usb_xdci_suspend);

/* Assumes lock already acquired */
static int ipa3_usb_resume_no_remote_wakeup(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot)
{
	int result = -EFAULT;
	enum ipa3_usb_transport_type ttype;

	ttype = IPA3_USB_GET_TTYPE(teth_prot);

	IPA_USB_DBG("Start resume with no remote wakeup sequence: %s\n",
		IPA3_USB_IS_TTYPE_DPL(ttype) ?
		"DPL channel":"Data Tethering channels");

	/* Request USB_PROD */
	if (ipa_pm_is_used())
		result = ipa_pm_activate_sync(
			ipa3_usb_ctx->ttype_ctx[ttype].pm_ctx.hdl);
	else
		result = ipa3_usb_request_prod(ttype);
	if (result)
		goto fail_exit;

	/* Connect tethering protocol */
	result = ipa3_usb_connect_teth_prot(teth_prot);
	if (result) {
		IPA_USB_ERR("failed to connect teth protocol\n");
		goto release_prod;
	}

	if (!IPA3_USB_IS_TTYPE_DPL(ttype)) {
		/* Start UL channel */
		result = ipa3_xdci_connect(ul_clnt_hdl);
		if (result) {
			IPA_USB_ERR("failed to start UL channel\n");
			goto disconn_teth;
		}
	}

	/* Start DL/DPL channel */
	result = ipa3_xdci_connect(dl_clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to start DL/DPL channel\n");
		goto stop_ul;
	}

	/* Change state to CONNECTED */
	if (!ipa3_usb_set_state(IPA_USB_CONNECTED, false, ttype)) {
		IPA_USB_ERR("failed to change state to connected\n");
		result = -EFAULT;
		goto stop_dl;
	}

	return 0;

stop_dl:
	(void)ipa3_xdci_disconnect(dl_clnt_hdl, false, -1);
stop_ul:
	if (!IPA3_USB_IS_TTYPE_DPL(ttype)) {
		(void)ipa3_xdci_disconnect(ul_clnt_hdl, true,
			ipa3_usb_ctx->qmi_req_id);
		ipa3_usb_ctx->qmi_req_id++;
	}
disconn_teth:
	(void)ipa3_usb_disconnect_teth_prot(teth_prot);
release_prod:
	if (ipa_pm_is_used())
		(void)ipa_pm_deactivate_sync(
			ipa3_usb_ctx->ttype_ctx[ttype].pm_ctx.hdl);
	else
		(void)ipa3_usb_release_prod(ttype);
fail_exit:
	return result;
}

int ipa_usb_xdci_resume(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot)
{
	int result = -EFAULT;
	enum ipa3_usb_state prev_state;
	unsigned long flags;
	enum ipa3_usb_transport_type ttype;

	mutex_lock(&ipa3_usb_ctx->general_mutex);
	IPA_USB_DBG_LOW("entry\n");

	if (teth_prot < 0 || teth_prot >= IPA_USB_MAX_TETH_PROT_SIZE) {
		IPA_USB_ERR("bad parameters\n");
		result = -EINVAL;
		goto bad_params;
	}

	ttype = IPA3_USB_GET_TTYPE(teth_prot);

	if (!ipa3_usb_check_legal_op(IPA_USB_OP_RESUME, ttype)) {
		IPA_USB_ERR("Illegal operation\n");
		result = -EPERM;
		goto bad_params;
	}

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	prev_state = ipa3_usb_ctx->ttype_ctx[ttype].state;
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);
	if (prev_state == IPA_USB_SUSPENDED_NO_RWAKEUP) {
		result = ipa3_usb_resume_no_remote_wakeup(ul_clnt_hdl,
			dl_clnt_hdl, teth_prot);
		mutex_unlock(&ipa3_usb_ctx->general_mutex);
		return result;
	}

	IPA_USB_DBG("Start resume sequence: %s\n",
		IPA3_USB_IS_TTYPE_DPL(ttype) ?
		"DPL channel" : "Data Tethering channels");

	/* Change state to RESUME_IN_PROGRESS */
	if (!ipa3_usb_set_state(IPA_USB_RESUME_IN_PROGRESS, false, ttype)) {
		IPA_USB_ERR("failed to change state to resume_in_progress\n");
		result = -EFAULT;
		goto bad_params;
	}

	/* Request USB_PROD */
	if (ipa_pm_is_used())
		result = ipa_pm_activate_sync(
			ipa3_usb_ctx->ttype_ctx[ttype].pm_ctx.hdl);
	else
		result = ipa3_usb_request_prod(ttype);
	if (result)
		goto prod_req_fail;

	if (!IPA3_USB_IS_TTYPE_DPL(ttype)) {
		/* Start UL channel */
		result = ipa3_start_gsi_channel(ul_clnt_hdl);
		if (result) {
			IPA_USB_ERR("failed to start UL channel\n");
			goto start_ul_fail;
		}
	}

	/* Start DL/DPL channel */
	result = ipa3_start_gsi_channel(dl_clnt_hdl);
	if (result) {
		IPA_USB_ERR("failed to start DL/DPL channel\n");
		goto start_dl_fail;
	}

	/* Change state to CONNECTED */
	if (!ipa3_usb_set_state(IPA_USB_CONNECTED, false, ttype)) {
		IPA_USB_ERR("failed to change state to connected\n");
		result = -EFAULT;
		goto state_change_connected_fail;
	}

	IPA_USB_DBG_LOW("exit\n");
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return 0;

state_change_connected_fail:
	result = ipa3_stop_gsi_channel(dl_clnt_hdl);
	if (result)
		IPA_USB_ERR("Error stopping DL/DPL channel: %d\n",
			result);
start_dl_fail:
	if (!IPA3_USB_IS_TTYPE_DPL(ttype)) {
		result = ipa3_stop_gsi_channel(ul_clnt_hdl);
		if (result)
			IPA_USB_ERR("Error stopping UL channel: %d\n", result);
	}
start_ul_fail:
	if (ipa_pm_is_used())
		ipa_pm_deactivate_sync(
			ipa3_usb_ctx->ttype_ctx[ttype].pm_ctx.hdl);
	else
		ipa3_usb_release_prod(ttype);
prod_req_fail:
	/* Change state back to prev_state */
	if (!ipa3_usb_set_state(prev_state, true, ttype))
		IPA_USB_ERR("failed to change state back to %s\n",
			ipa3_usb_state_to_string(prev_state));
bad_params:
	mutex_unlock(&ipa3_usb_ctx->general_mutex);
	return result;
}
EXPORT_SYMBOL(ipa_usb_xdci_resume);

static int __init ipa3_usb_init(void)
{
	int i;
	unsigned long flags;
	int res;
	struct ipa3_usb_pm_context *pm_ctx;

	pr_debug("entry\n");
	ipa3_usb_ctx = kzalloc(sizeof(struct ipa3_usb_context), GFP_KERNEL);
	if (ipa3_usb_ctx == NULL) {
		pr_err(":ipa_usb init failed\n");
		return -ENOMEM;
	}
	memset(ipa3_usb_ctx, 0, sizeof(struct ipa3_usb_context));

	for (i = 0; i < IPA_USB_MAX_TETH_PROT_SIZE; i++)
		ipa3_usb_ctx->teth_prot_ctx[i].state =
			IPA_USB_TETH_PROT_INVALID;
	ipa3_usb_ctx->num_init_prot = 0;
	init_completion(&ipa3_usb_ctx->dev_ready_comp);
	ipa3_usb_ctx->qmi_req_id = 0;
	spin_lock_init(&ipa3_usb_ctx->state_lock);
	ipa3_usb_ctx->dl_data_pending = false;
	mutex_init(&ipa3_usb_ctx->general_mutex);

	/* init PM related members */
	pm_ctx = &ipa3_usb_ctx->ttype_ctx[IPA_USB_TRANSPORT_TETH].pm_ctx;
	pm_ctx->hdl = ~0;
	pm_ctx->remote_wakeup_work = &ipa3_usb_notify_remote_wakeup_work;
	pm_ctx = &ipa3_usb_ctx->ttype_ctx[IPA_USB_TRANSPORT_DPL].pm_ctx;
	pm_ctx->hdl = ~0;
	pm_ctx->remote_wakeup_work = &ipa3_usb_dpl_notify_remote_wakeup_work;

	for (i = 0; i < IPA_USB_TRANSPORT_MAX; i++) {
		ipa3_usb_ctx->ttype_ctx[i].rm_ctx.prod_valid = false;
		ipa3_usb_ctx->ttype_ctx[i].rm_ctx.cons_valid = false;
		init_completion(&ipa3_usb_ctx->ttype_ctx[i].rm_ctx.prod_comp);
		ipa3_usb_ctx->ttype_ctx[i].user_data = NULL;
	}

	spin_lock_irqsave(&ipa3_usb_ctx->state_lock, flags);
	for (i = 0; i < IPA_USB_TRANSPORT_MAX; i++) {
		ipa3_usb_ctx->ttype_ctx[i].state = IPA_USB_INVALID;
		ipa3_usb_ctx->ttype_ctx[i].rm_ctx.cons_state =
			IPA_USB_CONS_RELEASED;
	}
	spin_unlock_irqrestore(&ipa3_usb_ctx->state_lock, flags);

	ipa3_usb_ctx->wq = create_singlethread_workqueue("ipa_usb_wq");
	if (!ipa3_usb_ctx->wq) {
		pr_err("failed to create workqueue\n");
		res = -EFAULT;
		goto ipa_usb_workqueue_fail;
	}

	ipa_usb_debugfs_init();

	pr_info("exit: IPA_USB init success!\n");

	return 0;

ipa_usb_workqueue_fail:
	pr_err("init failed (%d)\n", -res);
	kfree(ipa3_usb_ctx);
	return res;
}

static void ipa3_usb_exit(void)
{
	IPA_USB_DBG_LOW("IPA_USB exit\n");
	ipa_usb_debugfs_remove();
	kfree(ipa3_usb_ctx);
}

arch_initcall(ipa3_usb_init);
module_exit(ipa3_usb_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA USB client driver");
