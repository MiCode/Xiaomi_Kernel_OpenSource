/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/pm_runtime.h>
#include <mhi_sys.h>
#include <mhi.h>
#include <mhi_bhi.h>
#include <mhi_hwio.h>

#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>

#include <linux/esoc_client.h>

static int mhi_ssr_notify_cb(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct mhi_device_ctxt *mhi_dev_ctxt =
		container_of(nb, struct mhi_device_ctxt, mhi_ssr_nb);
	switch (action) {
	case SUBSYS_BEFORE_POWERUP:
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Received Subsystem event BEFORE_POWERUP\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Received Subsystem event AFTER_POWERUP\n");
		break;
	case SUBSYS_POWERUP_FAILURE:
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Received Subsystem event POWERUP_FAILURE\n");
		break;
	case SUBSYS_BEFORE_SHUTDOWN:
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Received Subsystem event BEFORE_SHUTDOWN\n");
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Received Subsystem event AFTER_SHUTDOWN\n");
		break;
	case SUBSYS_RAMDUMP_NOTIFICATION:
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Received Subsystem event RAMDUMP\n");
		break;
	default:
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Received ESOC notifcation %d, NOT handling\n",
			(int)action);
		break;
	}
	return NOTIFY_OK;
}

int mhi_esoc_register(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int ret_val = 0;
	struct device_node *np;
	struct device *dev = &mhi_dev_ctxt->pcie_device->dev;

	np = dev->of_node;
	mhi_dev_ctxt->esoc_handle = devm_register_esoc_client(dev, "mdm");
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"Of table of pcie struct device property is dev->of_node %p\n",
		np);
	if (IS_ERR_OR_NULL(mhi_dev_ctxt->esoc_handle)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Failed to register for SSR, ret %lx\n",
			(uintptr_t)mhi_dev_ctxt->esoc_handle);
		return -EIO;
	}
	mhi_dev_ctxt->mhi_ssr_nb.notifier_call = mhi_ssr_notify_cb;
	mhi_dev_ctxt->esoc_ssr_handle = subsys_notif_register_notifier(
					mhi_dev_ctxt->esoc_handle->name,
					&mhi_dev_ctxt->mhi_ssr_nb);
	if (IS_ERR_OR_NULL(mhi_dev_ctxt->esoc_ssr_handle)) {
		ret_val = PTR_RET(mhi_dev_ctxt->esoc_ssr_handle);
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Can't find esoc desc ret 0x%lx\n",
			(uintptr_t)mhi_dev_ctxt->esoc_ssr_handle);
	}

	return ret_val;
}

void mhi_notify_client(struct mhi_client_handle *client_handle,
		       enum MHI_CB_REASON reason)
{
	struct mhi_cb_info cb_info = {0};
	struct mhi_result result = {0};
	struct mhi_client_config *client_config;

	cb_info.result = NULL;
	cb_info.cb_reason = reason;

	if (client_handle == NULL)
		return;

	client_config = client_handle->client_config;

	if (client_config->client_info.mhi_client_cb) {
		result.user_data = client_config->user_data;
		cb_info.chan = client_config->chan_info.chan_nr;
		cb_info.result = &result;
		mhi_log(client_config->mhi_dev_ctxt, MHI_MSG_INFO,
			"Calling back for chan %d, reason %d\n",
			cb_info.chan,
			reason);
		client_config->client_info.mhi_client_cb(&cb_info);
	}
}

void mhi_notify_clients(struct mhi_device_ctxt *mhi_dev_ctxt,
					enum MHI_CB_REASON reason)
{
	int i;
	struct mhi_client_handle *client_handle = NULL;

	for (i = 0; i < MHI_MAX_CHANNELS; ++i) {
		if (VALID_CHAN_NR(i)) {
			client_handle = mhi_dev_ctxt->client_handle_list[i];
			mhi_notify_client(client_handle, reason);
		}
	}
}

int set_mhi_base_state(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 pcie_word_val = 0;
	int r = 0;

	mhi_dev_ctxt->bhi_ctxt.bhi_base = mhi_dev_ctxt->core.bar0_base;
	pcie_word_val = mhi_reg_read(mhi_dev_ctxt->bhi_ctxt.bhi_base, BHIOFF);

	/* confirm it's a valid reading */
	if (unlikely(pcie_word_val == U32_MAX)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Invalid BHI Offset:0x%x\n", pcie_word_val);
		return -EIO;
	}
	mhi_dev_ctxt->bhi_ctxt.bhi_base += pcie_word_val;
	pcie_word_val = mhi_reg_read(mhi_dev_ctxt->bhi_ctxt.bhi_base,
				     BHI_EXECENV);
	mhi_dev_ctxt->dev_exec_env = pcie_word_val;
	if (pcie_word_val == MHI_EXEC_ENV_AMSS) {
		mhi_dev_ctxt->base_state = STATE_TRANSITION_RESET;
	} else if (pcie_word_val == MHI_EXEC_ENV_PBL) {
		mhi_dev_ctxt->base_state = STATE_TRANSITION_BHI;
	} else {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Invalid EXEC_ENV: 0x%x\n",
			pcie_word_val);
		r = -EIO;
	}
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"EXEC_ENV: %d Base state %d\n",
		pcie_word_val, mhi_dev_ctxt->base_state);
	return r;
}

void mhi_link_state_cb(struct msm_pcie_notify *notify)
{
	struct mhi_device_ctxt *mhi_dev_ctxt = NULL;

	if (!notify || !notify->data) {
		pr_err("%s: incomplete handle received\n", __func__);
		return;
	}

	mhi_dev_ctxt = notify->data;
	switch (notify->event) {
	case MSM_PCIE_EVENT_LINKDOWN:
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Received MSM_PCIE_EVENT_LINKDOWN\n");
		break;
	case MSM_PCIE_EVENT_LINKUP:
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Received MSM_PCIE_EVENT_LINKUP\n");
		mhi_dev_ctxt->counters.link_up_cntr++;
		break;
	case MSM_PCIE_EVENT_WAKEUP:
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Received MSM_PCIE_EVENT_WAKE\n");
		__pm_stay_awake(&mhi_dev_ctxt->w_lock);
		__pm_relax(&mhi_dev_ctxt->w_lock);

		if (mhi_dev_ctxt->flags.mhi_initialized) {
			mhi_dev_ctxt->runtime_get(mhi_dev_ctxt);
			mhi_dev_ctxt->runtime_put(mhi_dev_ctxt);
		}
		break;
	default:
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Received bad link event\n");
		return;
		}
}

int init_mhi_base_state(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int r = 0;

	r = mhi_init_state_transition(mhi_dev_ctxt, mhi_dev_ctxt->base_state);
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Failed to start state change event, to %d\n",
			mhi_dev_ctxt->base_state);
	}
	return r;
}
