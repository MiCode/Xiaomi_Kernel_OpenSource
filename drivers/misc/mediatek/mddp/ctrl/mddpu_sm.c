/*
 * mddpu_sm.c - MDDPU (USB) state machine.
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
#include "mddp_usb_def.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Global variables.
//------------------------------------------------------------------------------
static struct ufpm_deactivate_md_func_rsp_t deact_rsp_metadata_s;

//------------------------------------------------------------------------------
// Private variables.
//------------------------------------------------------------------------------
static struct mddp_md_cfg_t mddpu_md_cfg_s = {
	AP_MOD_USB, /* ipc_ap_mod_id */
	MD_MOD_UFPM, /*ipc_md_mod_id */
};

//------------------------------------------------------------------------------
// Private helper macro.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Public functions - MDDPU (USB) state machine functions
//------------------------------------------------------------------------------
void mddpu_sm_enable(struct mddp_app_t *app)
{
	struct mddp_md_msg_t                   *md_msg;
	struct ufpm_enable_md_func_req_t        enable_req;
	struct ccci_emi_info                    emi_info;
	uint32_t                                usb_buf_len = 0;
	int32_t                                 ret;

	// 1. Send ENABLE to USB
	ret = rndis_mddpu_change_state(MDDP_STATE_ENABLING, NULL, &usb_buf_len);
	if (ret < 0 || usb_buf_len != 0) {
		pr_notice("%s: rndis_mddpu_change_state fail, ret=%d, usb_len=%d\n",
				__func__, ret, usb_buf_len);
		return;
	}

	// 2. Send ENABLE to MD
	ret = ccci_get_emi_info(0, &emi_info);
	if (ret < 0) {
		pr_notice("%s: ccci_get_emi_info fail, ret=%d\n",
				__func__, ret);
		return;
	}

	memset(&enable_req, 0, sizeof(enable_req));
	enable_req.mode = UFPM_FUNC_MODE_TETHER;
	enable_req.version = __MDDP_VERSION__;
	enable_req.mpuInfo.apUsbDomainId = emi_info.ap_domain_id;
	enable_req.mpuInfo.mdCldmaDomainId = emi_info.md_domain_id;
	enable_req.mpuInfo.memBank0BaseAddr = emi_info.ap_view_bank0_base;
	enable_req.mpuInfo.memBank0Size = emi_info.bank0_size;
	enable_req.mpuInfo.memBank4BaseAddr = emi_info.ap_view_bank4_base;
	enable_req.mpuInfo.memBank4Size = emi_info.bank4_size;

	pr_info("%s: MDDP version=%d\n",
			__func__, enable_req.version);
	pr_info("%s: memBank0BaseAddr=0x%llx\n",
			__func__, enable_req.mpuInfo.memBank0BaseAddr);
	pr_info("%s: memBank0Size=0x%llx\n",
			__func__, enable_req.mpuInfo.memBank0Size);
	pr_info("%s: memBank4BaseAddr=0x%llx\n",
			__func__, enable_req.mpuInfo.memBank4BaseAddr);
	pr_info("%s: memBank4Size=0x%llx\n",
			__func__, enable_req.mpuInfo.memBank4Size);

	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) + sizeof(enable_req),
			GFP_ATOMIC);
	if (unlikely(!md_msg)) {
		pr_notice("%s: failed to alloc md_msg bug!\n", __func__);
		WARN_ON(1);
		return;
	}

	md_msg->msg_id = IPC_MSG_ID_UFPM_ENABLE_MD_FAST_PATH_REQ;
	md_msg->data_len = sizeof(enable_req);
	memcpy(md_msg->data, &enable_req, sizeof(enable_req));
	mddp_ipc_send_md(app, md_msg, -1);
}

void mddpu_sm_rsp_enable_ok(struct mddp_app_t *app)
{
	uint32_t                                usb_buf_len = 0;
	struct mddp_dev_rsp_enable_t            enable;
	int32_t                                 ret;

	// 1. Send RSP to USB
	ret = rndis_mddpu_change_state(app->state, NULL, &usb_buf_len);
	if (ret < 0 || usb_buf_len != 0) {
		pr_notice("%s: rndis_mddpu_change_state fail, ret=%d, usb_len=%d\n",
				__func__, ret, usb_buf_len);
		return;
	}

	// 2. Send RSP to upper module.
	mddp_dev_response(app->type, MDDP_CMCMD_ENABLE_RSP,
			true, (uint8_t *)&enable, sizeof(enable));
}

void mddpu_sm_rsp_enable_fail(struct mddp_app_t *app)
{
	uint32_t                        usb_buf_len = 0;
	struct mddp_dev_rsp_enable_t    enable;
	int32_t                         ret;

	// 1. Send RSP to USB
	ret = rndis_mddpu_change_state(app->state, NULL, &usb_buf_len);
	if (ret < 0 || usb_buf_len != 0) {
		pr_notice("%s: rndis_mddpu_change_state fail, ret=%d, usb_len=%d\n",
				__func__, ret, usb_buf_len);
		return;
	}

	// 2. Send RSP to upper module.
	mddp_dev_response(app->type, MDDP_CMCMD_ENABLE_RSP,
			false, (uint8_t *)&enable, sizeof(enable));
}

void mddpu_sm_disable(struct mddp_app_t *app)
{
	struct mddp_md_msg_t                   *md_msg;
	struct ufpm_md_fast_path_common_req_t   disable_req;
	uint32_t                                usb_buf_len = 0;
	int32_t                                 ret;

	// 1. Send DISABLE to USB
	ret = rndis_mddpu_change_state(MDDP_STATE_DISABLING,
			NULL, &usb_buf_len);

	if (ret < 0 || usb_buf_len != 0) {
		pr_notice("%s: rndis_mddpu_change_state fail, ret=%d, usb_len=%d\n",
				__func__, ret, usb_buf_len);
		return;
	}

	// 2. Send DISABLE to MD
	memset(&disable_req, 0, sizeof(disable_req));
	disable_req.mode = UFPM_FUNC_MODE_TETHER;

	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) + sizeof(disable_req),
			GFP_ATOMIC);
	if (unlikely(!md_msg)) {
		pr_notice("%s: failed to alloc md_msg bug!\n", __func__);
		WARN_ON(1);
		return;
	}

	md_msg->msg_id = IPC_MSG_ID_UFPM_DISABLE_MD_FAST_PATH_REQ;
	md_msg->data_len = sizeof(disable_req);
	memcpy(md_msg->data, &disable_req, sizeof(disable_req));
	mddp_ipc_send_md(app, md_msg, -1);
}

void mddpu_sm_rsp_disable(struct mddp_app_t *app)
{
	uint32_t                        usb_buf_len = 0;
	int32_t                         ret;

	// 1. Send RSP to USB
	ret = rndis_mddpu_change_state(app->state, NULL, &usb_buf_len);
	if (ret < 0 || usb_buf_len != 0) {
		pr_notice("%s: rndis_mddpu_change_state fail, ret=%d, usb_len=%d\n",
				__func__, ret, usb_buf_len);
		return;
	}

	// 2. NO NEED to send RSP to upper module.

}

void mddpu_sm_act(struct mddp_app_t *app)
{
	struct mddp_md_msg_t   *md_msg;
	uint8_t                 usb_buf[MAX_USB_RET_BUF_SZ];
	uint32_t                usb_buf_len = MAX_USB_RET_BUF_SZ;
	int32_t                 ret;

	// 1. Register filter model
	mddp_f_dev_add_wan_dev(app->ap_cfg.ul_dev_name);
	mddp_f_dev_add_lan_dev(app->ap_cfg.dl_dev_name, 0);

	// 2. Send ACTIVATING to USB
	ret = rndis_mddpu_change_state(MDDP_STATE_ACTIVATING,
			usb_buf, &usb_buf_len);

	if (ret < 0 ||
		usb_buf_len != sizeof(struct ufpm_activate_md_func_req_t)) {
		pr_notice(
		"%s: Failed to change state, ret=%d, usb_len=%u, sz=%lu!\n",
			__func__, ret, usb_buf_len,
			sizeof(struct ufpm_activate_md_func_req_t));
		return;
	}

	// 3. Send ACTIVATING to MD
	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) + usb_buf_len,
			GFP_ATOMIC);
	if (unlikely(!md_msg)) {
		pr_notice("%s: failed to alloc md_msg bug!\n", __func__);
		WARN_ON(1);
		return;
	}

	md_msg->msg_id = IPC_MSG_ID_UFPM_ACTIVATE_MD_FAST_PATH_REQ;
	md_msg->data_len = usb_buf_len;
	memcpy(md_msg->data, &usb_buf, usb_buf_len);
	mddp_ipc_send_md(app, md_msg, -1);
}

void mddpu_sm_rsp_act_ok(struct mddp_app_t *app)
{
	uint32_t                        usb_buf_len = 0;
	struct mddp_dev_rsp_act_t       act;
	int32_t                         ret;

	// 1. Send RSP to USB
	ret = rndis_mddpu_change_state(app->state, NULL, &usb_buf_len);
	if (ret < 0 || usb_buf_len != 0) {
		pr_notice(
		"%s: RNDIS failed to change state, ret=%d, usb_len=%d!\n",
			__func__, ret, usb_buf_len);
		return;
	}

	// 2. Send RSP to upper module.
	mddp_dev_response(app->type, MDDP_CMCMD_ACT_RSP,
			true, (uint8_t *)&act, sizeof(act));
}

void mddpu_sm_rsp_act_fail(struct mddp_app_t *app)
{
	uint32_t                        usb_buf_len = 0;
	struct mddp_dev_rsp_act_t       act;
	int32_t                         ret;

	// 1. Send RSP to USB
	ret = rndis_mddpu_change_state(app->state, NULL, &usb_buf_len);
	if (ret < 0 || usb_buf_len != 0) {
		pr_notice(
		"%s: RNDIS failed to change state, ret=%d, usb_len=%d\n",
			__func__, ret, usb_buf_len);
		return;
	}

	// 2. Send RSP to upper module.
	mddp_dev_response(app->type, MDDP_CMCMD_ACT_RSP,
			false, (uint8_t *)&act, sizeof(act));
}

void mddpu_sm_deact(struct mddp_app_t *app)
{
	struct mddp_md_msg_t   *md_msg;
	uint8_t                 usb_buf[MAX_USB_RET_BUF_SZ];
	uint32_t                usb_buf_len = MAX_USB_RET_BUF_SZ;
	int32_t                 ret;

	// 1. Send DEACTIVATING to USB
	ret = rndis_mddpu_change_state(MDDP_STATE_DEACTIVATING,
			usb_buf, &usb_buf_len);
	if (ret < 0 ||
		usb_buf_len != sizeof(struct ufpm_md_fast_path_common_req_t)) {
		pr_notice("%s: rndis_mddpu_change_state fail, ret=%d, usb_len=%d\n",
				__func__, ret, usb_buf_len);
		//return;
	}

	// 2. Send DEACTIVATING to MD
	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) + usb_buf_len,
			GFP_ATOMIC);
	if (unlikely(!md_msg)) {
		pr_notice("%s: failed to alloc md_msg bug!\n", __func__);
		WARN_ON(1);
		return;
	}

	md_msg->msg_id = IPC_MSG_ID_UFPM_DEACTIVATE_MD_FAST_PATH_REQ;
	md_msg->data_len = usb_buf_len;
	memcpy(md_msg->data, &usb_buf, usb_buf_len);
	mddp_ipc_send_md(app, md_msg, -1);
}

void mddpu_sm_rsp_deact(struct mddp_app_t *app)
{
	uint32_t                        usb_buf_len;
	struct mddp_dev_rsp_deact_t     deact;
	int32_t                         ret;

	// 1. Register filter model
	mddp_f_dev_del_wan_dev(app->ap_cfg.ul_dev_name);
	mddp_f_dev_del_lan_dev(app->ap_cfg.dl_dev_name);

	// 2. Send RSP to USB
	usb_buf_len = sizeof(deact_rsp_metadata_s);
	ret = rndis_mddpu_change_state(app->state,
			&deact_rsp_metadata_s,
			&usb_buf_len);
	if (ret < 0 || usb_buf_len != 0) {
		pr_notice("%s: rndis_mddpu_change_state fail, ret=%d, usb_len=%d\n",
				__func__, ret, usb_buf_len);
		return;
	}

	// 3. Send RSP to upper module.
	mddp_dev_response(app->type, MDDP_CMCMD_DEACT_RSP,
			true, (uint8_t *)&deact, sizeof(deact));
}

//------------------------------------------------------------------------------
// MDDPU State machine.
//------------------------------------------------------------------------------
static struct mddp_sm_entry_t mddpu_uninit_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_FUNC_ENABLE,  MDDP_STATE_ENABLING,     mddpu_sm_enable},
{MDDP_EVT_DUMMY,        MDDP_STATE_UNINIT,       NULL} /* End of SM. */
};

static struct mddp_sm_entry_t mddpu_enabling_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_MD_RSP_OK,    MDDP_STATE_DEACTIVATED,  mddpu_sm_rsp_enable_ok},
{MDDP_EVT_MD_RSP_FAIL,  MDDP_STATE_UNINIT,       mddpu_sm_rsp_enable_fail},
{MDDP_EVT_DUMMY,        MDDP_STATE_ENABLING,     NULL} /* End of SM. */
};

static struct mddp_sm_entry_t mddpu_disabling_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_MD_RSP_OK,    MDDP_STATE_UNINIT,       mddpu_sm_rsp_disable},
{MDDP_EVT_MD_RSP_FAIL,  MDDP_STATE_UNINIT,       mddpu_sm_rsp_disable},
{MDDP_EVT_DUMMY,        MDDP_STATE_DISABLING,    NULL} /* End of SM. */
};

static struct mddp_sm_entry_t mddpu_deactivated_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_FUNC_ACT,     MDDP_STATE_ACTIVATING,   mddpu_sm_act},
{MDDP_EVT_DUMMY,        MDDP_STATE_DEACTIVATED,  NULL} /* End of SM. */
};

static struct mddp_sm_entry_t mddpu_activating_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_FUNC_DEACT,   MDDP_STATE_DEACTIVATING, mddpu_sm_deact},
{MDDP_EVT_MD_RSP_OK,    MDDP_STATE_ACTIVATED,    mddpu_sm_rsp_act_ok},
{MDDP_EVT_MD_RSP_FAIL,  MDDP_STATE_DEACTIVATED,  mddpu_sm_rsp_act_fail},
{MDDP_EVT_DUMMY,        MDDP_STATE_ACTIVATING,   NULL} /* End of SM. */
};

static struct mddp_sm_entry_t mddpu_activated_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_FUNC_DEACT,   MDDP_STATE_DEACTIVATING, mddpu_sm_deact},
{MDDP_EVT_FUNC_DISABLE, MDDP_STATE_DISABLING,    mddpu_sm_disable},
{MDDP_EVT_DUMMY,        MDDP_STATE_ACTIVATED,    NULL} /* End of SM. */
};

static struct mddp_sm_entry_t mddpu_deactivating_state_machine_s[] = {
/* event                new_state                action */
{MDDP_EVT_FUNC_ACT,     MDDP_STATE_ACTIVATING,   mddpu_sm_act},
{MDDP_EVT_MD_RSP_OK,    MDDP_STATE_DEACTIVATED,  mddpu_sm_rsp_deact},
{MDDP_EVT_MD_RSP_FAIL,  MDDP_STATE_DEACTIVATED,  mddpu_sm_rsp_deact},
{MDDP_EVT_DUMMY,        MDDP_STATE_DEACTIVATING, NULL} /* End of SM. */
};

struct mddp_sm_entry_t *mddpu_state_machines_s[MDDP_STATE_CNT] = {
	mddpu_uninit_state_machine_s, /* UNINIT */
	mddpu_enabling_state_machine_s, /* ENABLING */
	mddpu_deactivated_state_machine_s, /* DEACTIVATED */
	mddpu_activating_state_machine_s, /* ACTIVATING */
	mddpu_activated_state_machine_s, /* ACTIVATED */
	mddpu_deactivating_state_machine_s, /* DEACTIVATING */
	mddpu_disabling_state_machine_s, /* DISABLING */
};

//------------------------------------------------------------------------------
// Public functions.
//------------------------------------------------------------------------------
int32_t mddpu_ufpm_msg_hdlr(struct ipc_ilm *ilm)
{
	struct mddp_app_t                      *mdu;
	struct mddp_ilm_common_rsp_t           *rsp;
	struct ufpm_enable_md_func_rsp_t       *enable_rsp;

	//pr_info("%s: Handle ilm from UFPM.\n", __func__);

	rsp = (struct mddp_ilm_common_rsp_t *) ilm->local_para_ptr;
	if (unlikely(rsp->rsp.mode != UFPM_FUNC_MODE_TETHER)) {
		pr_notice("%s: Wrong mode(%d)!\n",
				__func__, rsp->rsp.mode);
		return -EINVAL;
	}

	mdu = mddp_get_app_inst(MDDP_APP_TYPE_USB);

	switch (ilm->msg_id) {
	case IPC_MSG_ID_UFPM_ENABLE_MD_FAST_PATH_RSP:
		enable_rsp = (struct ufpm_enable_md_func_rsp_t *)
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

	case IPC_MSG_ID_UFPM_DISABLE_MD_FAST_PATH_RSP:
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

	case IPC_MSG_ID_UFPM_ACTIVATE_MD_FAST_PATH_RSP:
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

	case IPC_MSG_ID_UFPM_DEACTIVATE_MD_FAST_PATH_RSP:
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

	case IPC_MSG_ID_UFPM_SEND_MD_USB_EP0_RSP:
	case IPC_MSG_ID_UFPM_SEND_AP_USB_EP0_IND:
		/* USB event. Forward to USB driver directly. */
		rndis_mddpu_usb_event(ilm->msg_id,
				&((struct local_para *)rsp)->data[0],
				rsp->msg_len);

		break;

	default:
		pr_notice("%s: Unsupported RSP MSG_ID[%d] from UFPM.\n",
					__func__, ilm->msg_id);
		break;
	}

	// <TJ_TODO_2> MSG_ID_L4C_UFPM_DEACTIVATE_MD_FAST_PATH_REQ
	// from ATCI for MD power off flight mode

	return 0;
}

int32_t mddpu_sm_init(struct mddp_app_t *app)
{
	memcpy(&app->state_machines,
		&mddpu_state_machines_s,
		sizeof(mddpu_state_machines_s));

	mddp_dump_sm_table(app);

	app->md_recv_msg_hdlr = mddpu_ufpm_msg_hdlr;
	app->reg_drv_callback = NULL;
	memcpy(&app->md_cfg, &mddpu_md_cfg_s, sizeof(struct mddp_md_cfg_t));

	app->is_config = 1;
	return 0;
}
