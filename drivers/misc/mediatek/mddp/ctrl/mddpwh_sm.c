/*
 * mddpwh_sm.c - MDDPWH (WiFi Hotspot) state machine.
 *
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/types.h>

#include "mddp_ctrl.h"
#include "mddp_filter.h"

#include "mddp_dev.h"
#include "mddp_if.h"
#include "mddp_ipc.h"
#include "mddp_sm.h"
#include "mddp_wifi_def.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Global variables.
//------------------------------------------------------------------------------
static struct wfpm_deactivate_md_func_rsp_t deact_rsp_metadata_s;

//------------------------------------------------------------------------------
// Private variables.
//------------------------------------------------------------------------------
static struct mddp_md_cfg_t mddpwh_md_cfg_s = {
	AP_MOD_WIFI, /* ipc_ap_mod_id */
	MD_MOD_WFPM, /*ipc_md_mod_id */
};

//------------------------------------------------------------------------------
// Private helper macro.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// External functions.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Public functions - MDDPWH (WiFi) state machine functions
//------------------------------------------------------------------------------
void mddpwh_sm_enable(struct mddp_app_t *app)
{
	struct mddp_md_msg_t                   *md_msg;
	struct wfpm_enable_md_func_req_t       *enable_req;
	struct wfpm_smem_info_t                *smem_info;
	uint32_t                                smem_num;

	// 1. Send ENABLE to WiFi
	app->drv_hdlr.change_state(MDDP_STATE_ENABLING);

	if (wfpm_ipc_get_smem_list((void **)&smem_info, &smem_num)) {
		pr_notice("%s: Failed to get smem info!\n", __func__);
		smem_num = 0;
	}

	// 2. Send ENABLE to MD
	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) +
			 sizeof(struct wfpm_enable_md_func_req_t) +
			smem_num * sizeof(struct wfpm_smem_info_t), GFP_ATOMIC);

	if (unlikely(!md_msg)) {
		pr_notice("%s: Failed to alloc md_msg bug!\n", __func__);
		WARN_ON(1);
		return;
	}

	md_msg->msg_id = IPC_MSG_ID_WFPM_ENABLE_MD_FAST_PATH_REQ;
	md_msg->data_len = sizeof(enable_req) +
		smem_num * sizeof(struct wfpm_smem_info_t);
	enable_req = (struct wfpm_enable_md_func_req_t *)&(md_msg->data);
	enable_req->mode = WFPM_FUNC_MODE_TETHER;
	enable_req->version = __MDDP_VERSION__;
	enable_req->smem_num = smem_num;

	memcpy(&(enable_req->smem_info), smem_info,
			smem_num * sizeof(struct wfpm_smem_info_t));
	mddp_ipc_send_md(app, md_msg, -1);
}

void mddpwh_sm_rsp_enable_ok(struct mddp_app_t *app)
{
	struct mddp_dev_rsp_enable_t            enable;

	// 1. Send RSP to WiFi
	app->drv_hdlr.change_state(app->state);

	// 2. Send RSP to upper module.
	mddp_dev_response(app->type, MDDP_CMCMD_ENABLE_RSP,
			true, (uint8_t *)&enable, sizeof(enable));
}

void mddpwh_sm_rsp_enable_fail(struct mddp_app_t *app)
{
	struct mddp_dev_rsp_enable_t    enable;

	// 1. Send RSP to WiFi
	app->drv_hdlr.change_state(app->state);

	// 2. Send RSP to upper module.
	mddp_dev_response(app->type, MDDP_CMCMD_ENABLE_RSP,
			false, (uint8_t *)&enable, sizeof(enable));
}

void mddpwh_sm_disable(struct mddp_app_t *app)
{
	struct mddp_md_msg_t                   *md_msg;
	struct wfpm_md_fast_path_common_req_t  *disable_req;

	// 1. Send DISABLE to WiFi
	app->drv_hdlr.change_state(MDDP_STATE_DISABLING);

	// 2. Send DISABLE to MD
	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) + sizeof(disable_req),
			GFP_ATOMIC);
	if (unlikely(!md_msg)) {
		pr_notice("%s: Failed to alloc md_msg bug!\n", __func__);
		WARN_ON(1);
		return;
	}

	disable_req = (struct wfpm_md_fast_path_common_req_t *)&(md_msg->data);
	disable_req->mode = WFPM_FUNC_MODE_TETHER;

	md_msg->msg_id = IPC_MSG_ID_WFPM_DISABLE_MD_FAST_PATH_REQ;
	md_msg->data_len = sizeof(struct wfpm_md_fast_path_common_req_t);
	mddp_ipc_send_md(app, md_msg, -1);
}

void mddpwh_sm_rsp_disable(struct mddp_app_t *app)
{
	// 1. Send RSP to WiFi
	app->drv_hdlr.change_state(app->state);

	// 2. NO NEED to send RSP to upper module.

}

void mddpwh_sm_act(struct mddp_app_t *app)
{
	struct mddp_md_msg_t                 *md_msg;
	struct wfpm_activate_md_func_req_t   *act_req;

	// 1. Register filter model
	mddp_f_dev_add_wan_dev(app->ap_cfg.ul_dev_name);
	mddp_f_dev_add_lan_dev(app->ap_cfg.dl_dev_name, 0);

	// 2. Send ACTIVATING to WiFi
	app->drv_hdlr.change_state(MDDP_STATE_ACTIVATING);

	// 3. Send ACTIVATING to MD
	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) +
		sizeof(struct wfpm_activate_md_func_req_t), GFP_ATOMIC);

	if (unlikely(!md_msg)) {
		pr_notice("%s: Failed to alloc md_msg bug!\n", __func__);
		WARN_ON(1);
		return;
	}

	act_req = (struct wfpm_activate_md_func_req_t *)&(md_msg->data);
	act_req->mode = WFPM_FUNC_MODE_TETHER;

	md_msg->msg_id = IPC_MSG_ID_WFPM_ACTIVATE_MD_FAST_PATH_REQ;
	md_msg->data_len = sizeof(struct wfpm_activate_md_func_req_t);
	mddp_ipc_send_md(app, md_msg, -1);
}

void mddpwh_sm_rsp_act_ok(struct mddp_app_t *app)
{
	struct mddp_dev_rsp_act_t       act;

	// 1. Send RSP to WiFi
	app->drv_hdlr.change_state(app->state);

	// 2. Send RSP to upper module.
	mddp_dev_response(app->type, MDDP_CMCMD_ACT_RSP,
			true, (uint8_t *)&act, sizeof(act));
}

void mddpwh_sm_rsp_act_fail(struct mddp_app_t *app)
{
	struct mddp_dev_rsp_act_t       act;

	// 1. Send RSP to WiFi
	app->drv_hdlr.change_state(app->state);

	// 2. Send RSP to upper module.
	mddp_dev_response(app->type, MDDP_CMCMD_ACT_RSP,
			false, (uint8_t *)&act, sizeof(act));
}

void mddpwh_sm_deact(struct mddp_app_t *app)
{
	struct mddp_md_msg_t                 *md_msg;
	struct wfpm_activate_md_func_req_t   *deact_req;

	// 1. Send ACTIVATING to WiFi
	app->drv_hdlr.change_state(MDDP_STATE_DEACTIVATING);

	// 2. Send ACTIVATING to MD
	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) +
		sizeof(struct wfpm_activate_md_func_req_t), GFP_ATOMIC);

	if (unlikely(!md_msg)) {
		pr_notice("%s: Failed to alloc md_msg bug!\n", __func__);
		WARN_ON(1);
		return;
	}

	deact_req = (struct wfpm_activate_md_func_req_t *)&(md_msg->data);
	deact_req->mode = WFPM_FUNC_MODE_TETHER;

	md_msg->msg_id = IPC_MSG_ID_WFPM_DEACTIVATE_MD_FAST_PATH_REQ;
	md_msg->data_len = sizeof(struct wfpm_activate_md_func_req_t);
	mddp_ipc_send_md(app, md_msg, -1);
}

void mddpwh_sm_rsp_deact(struct mddp_app_t *app)
{
	struct mddp_dev_rsp_deact_t     deact;

	// 1. Register filter model
	mddp_f_dev_del_wan_dev(app->ap_cfg.ul_dev_name);
	mddp_f_dev_del_lan_dev(app->ap_cfg.dl_dev_name);

	// 2. Send RSP to WiFi
	app->drv_hdlr.change_state(app->state);

	// 3. Send RSP to upper module.
	mddp_dev_response(app->type, MDDP_CMCMD_DEACT_RSP,
			true, (uint8_t *)&deact, sizeof(deact));
}

//------------------------------------------------------------------------------
// MDDPWH State machine.
//------------------------------------------------------------------------------
/* TODO: add enable stage1 for USB Tethering enable earlier than Wifi Hotspot */
static struct mddp_sm_entry_t mddpwh_uninit_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_FUNC_ENABLE,  MDDP_STATE_ENABLING,     mddpwh_sm_enable},
{MDDP_EVT_DUMMY,        MDDP_STATE_UNINIT,       NULL} /* End of SM. */
};

static struct mddp_sm_entry_t mddpwh_enabling_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_MD_RSP_OK,    MDDP_STATE_DEACTIVATED,  mddpwh_sm_rsp_enable_ok},
{MDDP_EVT_MD_RSP_FAIL,  MDDP_STATE_UNINIT,       mddpwh_sm_rsp_enable_fail},
{MDDP_EVT_DUMMY,        MDDP_STATE_ENABLING,     NULL} /* End of SM. */
};

static struct mddp_sm_entry_t mddpwh_disabling_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_MD_RSP_OK,    MDDP_STATE_UNINIT,       mddpwh_sm_rsp_disable},
{MDDP_EVT_MD_RSP_FAIL,  MDDP_STATE_UNINIT,       mddpwh_sm_rsp_disable},
{MDDP_EVT_DUMMY,        MDDP_STATE_DISABLING,    NULL} /* End of SM. */
};

static struct mddp_sm_entry_t mddpwh_deactivated_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_FUNC_ACT,     MDDP_STATE_ACTIVATING,   mddpwh_sm_act},
{MDDP_EVT_DUMMY,        MDDP_STATE_DEACTIVATED,  NULL} /* End of SM. */
};

static struct mddp_sm_entry_t mddpwh_activating_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_FUNC_DEACT,   MDDP_STATE_DEACTIVATING, mddpwh_sm_deact},
{MDDP_EVT_MD_RSP_OK,    MDDP_STATE_ACTIVATED,    mddpwh_sm_rsp_act_ok},
{MDDP_EVT_MD_RSP_FAIL,  MDDP_STATE_DEACTIVATED,  mddpwh_sm_rsp_act_fail},
{MDDP_EVT_DUMMY,        MDDP_STATE_ACTIVATING,   NULL} /* End of SM. */
};

static struct mddp_sm_entry_t mddpwh_activated_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_FUNC_DEACT,   MDDP_STATE_DEACTIVATING, mddpwh_sm_deact},
{MDDP_EVT_FUNC_DISABLE, MDDP_STATE_DISABLING,    mddpwh_sm_disable},
{MDDP_EVT_DUMMY,        MDDP_STATE_ACTIVATED,    NULL} /* End of SM. */
};

static struct mddp_sm_entry_t mddpwh_deactivating_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_FUNC_ACT,     MDDP_STATE_ACTIVATING,   mddpwh_sm_act},
{MDDP_EVT_MD_RSP_OK,    MDDP_STATE_DEACTIVATED,  mddpwh_sm_rsp_deact},
{MDDP_EVT_MD_RSP_FAIL,  MDDP_STATE_DEACTIVATED,  mddpwh_sm_rsp_deact},
{MDDP_EVT_DUMMY,        MDDP_STATE_DEACTIVATING, NULL} /* End of SM. */
};

struct mddp_sm_entry_t *mddpwh_state_machines_s[MDDP_STATE_CNT] = {
	mddpwh_uninit_state_machine_s, /* UNINIT */
	mddpwh_enabling_state_machine_s, /* ENABLING */
	mddpwh_deactivated_state_machine_s, /* DEACTIVATED */
	mddpwh_activating_state_machine_s, /* ACTIVATING */
	mddpwh_activated_state_machine_s, /* ACTIVATED */
	mddpwh_deactivating_state_machine_s, /* DEACTIVATING */
	mddpwh_disabling_state_machine_s, /* DISABLING */
};

//------------------------------------------------------------------------------
// Public functions.
//------------------------------------------------------------------------------
int32_t mddpwh_wfpm_msg_hdlr(struct ipc_ilm *ilm)
{
	struct mddp_app_t                      *mdu;
	struct mddp_ilm_common_rsp_t           *rsp;
	struct wfpm_enable_md_func_rsp_t       *enable_rsp;

	//pr_info("%s: Handle ilm from WFPM.\n", __func__);

	rsp = (struct mddp_ilm_common_rsp_t *) ilm->local_para_ptr;
	if (unlikely(rsp->rsp.mode != WFPM_FUNC_MODE_TETHER)) {
		pr_notice("%s: Wrong mode(%d)!\n",
				__func__, rsp->rsp.mode);
		return -EINVAL;
	}

	mdu = mddp_get_app_inst(MDDP_APP_TYPE_WH);

	switch (ilm->msg_id) {
	case IPC_MSG_ID_WFPM_ENABLE_MD_FAST_PATH_RSP:
		enable_rsp = (struct wfpm_enable_md_func_rsp_t *)
			&ilm->local_para_ptr->data[0];
	pr_info("%s: set (%u), (%u),  MD version(%u), (%u).\n",
		__func__, enable_rsp->mode, enable_rsp->result,
		enable_rsp->version, enable_rsp->reserved);
		mddp_set_md_version(enable_rsp->version);

		if (rsp->rsp.result) {
			/* ENABLE OK. */
			pr_info("%s: ENABLE RSP OK, result(%d).\n",
					__func__, rsp->rsp.result);
			mddp_sm_set_state_by_md_rsp(mdu,
				MDDP_STATE_ENABLING, true);
		} else {
			/* ENABLE FAIL. */
			pr_notice("%s: ENABLE RSP FAIL, result(%d)!\n",
					__func__, rsp->rsp.result);
			mddp_sm_set_state_by_md_rsp(mdu,
				MDDP_STATE_ENABLING, false);
		}
		break;

	case IPC_MSG_ID_WFPM_DISABLE_MD_FAST_PATH_RSP:
		if (rsp->rsp.result) {
			/* DISABLE OK. */
			pr_info("%s: DISABLE RSP OK, result(%d).\n",
					__func__, rsp->rsp.result);
			mddp_sm_set_state_by_md_rsp(mdu,
				MDDP_STATE_DISABLING, true);
		} else {
			/* DISABLE FAIL. */
			pr_notice("%s: DISABLE RSP FAIL, result(%d)!\n",
					__func__, rsp->rsp.result);
			mddp_sm_set_state_by_md_rsp(mdu,
				MDDP_STATE_DISABLING, false);
		}

		break;

	case IPC_MSG_ID_WFPM_ACTIVATE_MD_FAST_PATH_RSP:
		if (rsp->rsp.result) {
			/* ACT OK. */
			pr_info("%s: ACT RSP OK, result(%d).\n",
					__func__, rsp->rsp.result);
			mddp_sm_set_state_by_md_rsp(mdu,
				MDDP_STATE_ACTIVATING, true);
		} else {
			/* ACT FAIL. */
			pr_notice("%s: ACT RSP FAIL, result(%d)!\n",
					__func__, rsp->rsp.result);
			mddp_sm_set_state_by_md_rsp(mdu,
				MDDP_STATE_ACTIVATING, false);
		}
		break;

	case IPC_MSG_ID_WFPM_DEACTIVATE_MD_FAST_PATH_RSP:
		if (rsp->rsp.result) {
			/* DEACT OK. */
			pr_info("%s: DEACT RSP OK, result(%d)\n",
					__func__, rsp->rsp.result);
			mddp_sm_set_state_by_md_rsp(mdu,
				MDDP_STATE_DEACTIVATING, true);

			memcpy(&deact_rsp_metadata_s,
					&((struct local_para *)rsp)->data[0],
					sizeof(deact_rsp_metadata_s));
		} else {
			/* DEACT FAIL. */
			pr_notice("%s: DEACT RSP FAIL, result(%d)\n",
					__func__, rsp->rsp.result);
			mddp_sm_set_state_by_md_rsp(mdu,
				MDDP_STATE_DEACTIVATING, false);

			memcpy(&deact_rsp_metadata_s,
					&((struct local_para *)rsp)->data[0],
					sizeof(deact_rsp_metadata_s));
		}

		break;

	default:
		pr_notice("%s: Unsupported RSP MSG_ID[%d] from WFPM.\n",
					__func__, ilm->msg_id);
		break;
	}

	// <TJ_TODO_2> MSG_ID_L4C_WFPM_DEACTIVATE_MD_FAST_PATH_REQ
	// from ATCI for MD power off flight mode

	return 0;
}

int32_t mddpwh_drv_add_txd(struct mddpwh_txd_t *txd)
{
	struct mddp_md_msg_t    *md_msg;
	struct mddp_app_t       *app;

	// Send TXD to MD
	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);

	if (!app->is_config) {
		pr_notice("%s: app_type(MDDP_APP_TYPE_WH) not configured!\n",
		__func__);
		return -ENODEV;
	}

	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) +
	sizeof(struct mddpwh_txd_t) + txd->txd_length, GFP_ATOMIC);

	if (unlikely(!md_msg)) {
		pr_notice("%s: Failed to alloc md_msg bug!\n", __func__);
		WARN_ON(1);
		return -ENOMEM;
	}

	md_msg->msg_id = IPC_MSG_ID_WFPM_SEND_MD_TXD_NOTIFY;
	md_msg->data_len = sizeof(struct mddpwh_txd_t) + txd->txd_length;
	memcpy(md_msg->data, txd, md_msg->data_len);
	mddp_ipc_send_md(app, md_msg, -1);

	return 0;
}

int32_t mddpwh_drv_get_net_stat(struct mddpwh_net_stat_t *usage)
{
	// Use global variable to cache previous statistics,
	// and return delta value each call.

	static struct mddpwh_net_stat_t     cur_stats = {0};
	struct mddpwh_net_stat_t           *md_stats;
	uint8_t                             smem_attr;
	uint32_t                            smem_size;

	if (mddp_ipc_get_md_smem_by_id(MDDP_MD_SMEM_USER_WIFI_STATISTICS,
				(void **)&md_stats, &smem_attr, &smem_size)) {
		pr_notice("%s: Failed to get smem_id (%d)!\n",
				__func__, MDDP_MD_SMEM_USER_WIFI_STATISTICS);
		return -EINVAL;
	}
	#define DIFF_FROM_SMEM(x) (usage->x = \
	(md_stats->x > cur_stats.x) ? (md_stats->x - cur_stats.x) : 0)
	DIFF_FROM_SMEM(tx_packets);
	DIFF_FROM_SMEM(rx_packets);
	DIFF_FROM_SMEM(tx_bytes);
	DIFF_FROM_SMEM(rx_bytes);
	DIFF_FROM_SMEM(tx_errors);
	DIFF_FROM_SMEM(rx_errors);

	memcpy(&cur_stats, md_stats, sizeof(struct mddpwh_net_stat_t));

	return 0;
}

int32_t mddpwh_drv_get_ap_rx_reorder_buf(
	struct mddpwh_ap_reorder_sync_table_t **ap_table)
{
	uint8_t      smem_attr;
	uint32_t     smem_size;

	if (mddp_ipc_get_md_smem_by_id(MDDP_MD_SMEM_USER_RX_REORDER_TO_MD,
				(void **)ap_table, &smem_attr, &smem_size)) {
		pr_notice("%s: Failed to get smem_id (%d)!\n",
				__func__, MDDP_MD_SMEM_USER_RX_REORDER_TO_MD);
		return -EINVAL;
	}

	return 0;
}

int32_t mddpwh_drv_get_md_rx_reorder_buf(
	struct mddpwh_md_reorder_sync_table_t **md_table)
{
	uint8_t      smem_attr;
	uint32_t     smem_size;

	if (mddp_ipc_get_md_smem_by_id(MDDP_MD_SMEM_USER_RX_REORDER_FROM_MD,
				(void **)md_table, &smem_attr, &smem_size)) {
		pr_notice("%s: Failed to get smem_id (%d)!\n",
				__func__, MDDP_MD_SMEM_USER_RX_REORDER_FROM_MD);
		return -EINVAL;
	}

	return 0;
}

int32_t mddpwh_drv_notify_info(
	struct mddpwh_drv_notify_info_t *wifi_notify)
{
	struct mddp_md_msg_t    *md_msg;
	struct mddp_app_t       *app;

	// Send WIFI Notify to MD
	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);

	if (!app->is_config) {
		pr_notice("%s: app_type(MDDP_APP_TYPE_WH) not configured!\n",
		__func__);
		return -ENODEV;
	}

	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) +
		sizeof(struct mddpwh_drv_notify_info_t) +
		wifi_notify->buf_len, GFP_ATOMIC);

	if (unlikely(!md_msg)) {
		pr_notice("%s: Failed to alloc md_msg bug!\n", __func__);
		WARN_ON(1);
		return -ENOMEM;
	}

	md_msg->msg_id = IPC_MSG_ID_WFPM_DRV_NOTIFY;
	md_msg->data_len = sizeof(struct mddpwh_drv_notify_info_t) +
		wifi_notify->buf_len;
	memcpy(md_msg->data, wifi_notify, md_msg->data_len);
	mddp_ipc_send_md(app, md_msg, -1);

	return 0;
}

int32_t mddpwh_drv_reg_callback(struct mddp_drv_handle_t *handle)
{
	struct mddpwh_drv_handle_t         *wh_handle;

	wh_handle = handle->wh_handle;

	wh_handle->add_txd = mddpwh_drv_add_txd;
	wh_handle->get_net_stat = mddpwh_drv_get_net_stat;
	wh_handle->get_ap_rx_reorder_buf = mddpwh_drv_get_ap_rx_reorder_buf;
	wh_handle->get_md_rx_reorder_buf = mddpwh_drv_get_md_rx_reorder_buf;
	wh_handle->notify_drv_info = mddpwh_drv_notify_info;

	return 0;
}

int32_t mddpwh_sm_init(struct mddp_app_t *app)
{
	memcpy(&app->state_machines,
		&mddpwh_state_machines_s,
		sizeof(mddpwh_state_machines_s));

	pr_info("%s: %p, %p\n",
		__func__, &(app->state_machines), &mddpwh_state_machines_s);
	mddp_dump_sm_table(app);

	app->md_recv_msg_hdlr = mddpwh_wfpm_msg_hdlr;
	app->reg_drv_callback = mddpwh_drv_reg_callback;
	memcpy(&app->md_cfg, &mddpwh_md_cfg_s, sizeof(struct mddp_md_cfg_t));
	app->is_config = 1;

	return 0;
}
