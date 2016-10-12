/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

	switch (action) {
	case SUBSYS_BEFORE_POWERUP:
		mhi_log(MHI_MSG_INFO,
			"Received Subsystem event BEFORE_POWERUP\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		mhi_log(MHI_MSG_INFO,
			"Received Subsystem event AFTER_POWERUP\n");
		break;
	case SUBSYS_POWERUP_FAILURE:
		mhi_log(MHI_MSG_INFO,
			"Received Subsystem event POWERUP_FAILURE\n");
		break;
	case SUBSYS_BEFORE_SHUTDOWN:
		mhi_log(MHI_MSG_INFO,
			"Received Subsystem event BEFORE_SHUTDOWN\n");
		mhi_log(MHI_MSG_INFO,
			"Not notifying clients\n");
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		mhi_log(MHI_MSG_INFO,
			"Received Subsystem event AFTER_SHUTDOWN\n");
		mhi_log(MHI_MSG_INFO,
			"Not notifying clients\n");
		break;
	case SUBSYS_RAMDUMP_NOTIFICATION:
		mhi_log(MHI_MSG_INFO,
			"Received Subsystem event RAMDUMP\n");
		mhi_log(MHI_MSG_INFO,
			"Not notifying clients\n");
		break;
	default:
		mhi_log(MHI_MSG_INFO,
			"Received ESOC notifcation %d, NOT handling\n",
			(int)action);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block mhi_ssr_nb = {
	.notifier_call = mhi_ssr_notify_cb,
};

int mhi_esoc_register(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int ret_val = 0;
	struct device_node *np;
	struct pci_driver *mhi_driver;
	struct device *dev = &mhi_dev_ctxt->dev_info->pcie_device->dev;

	mhi_driver = mhi_dev_ctxt->dev_info->mhi_pcie_driver;
	np = dev->of_node;
	mhi_dev_ctxt->esoc_handle = devm_register_esoc_client(dev, "mdm");
	mhi_log(MHI_MSG_VERBOSE,
		"Of table of pcie struct device property is dev->of_node %p\n",
		np);
	if (IS_ERR_OR_NULL(mhi_dev_ctxt->esoc_handle)) {
		mhi_log(MHI_MSG_CRITICAL,
			"Failed to register for SSR, ret %lx\n",
			(uintptr_t)mhi_dev_ctxt->esoc_handle);
		return -EIO;
	}

	mhi_dev_ctxt->esoc_ssr_handle = subsys_notif_register_notifier(
					mhi_dev_ctxt->esoc_handle->name,
					&mhi_ssr_nb);
	if (IS_ERR_OR_NULL(mhi_dev_ctxt->esoc_ssr_handle)) {
		ret_val = PTR_RET(mhi_dev_ctxt->esoc_ssr_handle);
		mhi_log(MHI_MSG_CRITICAL,
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

	cb_info.result = NULL;
	cb_info.cb_reason = reason;

	if (NULL != client_handle &&
	    NULL != client_handle->client_info.mhi_client_cb) {
		result.user_data = client_handle->user_data;
		cb_info.chan = client_handle->chan_info.chan_nr;
		cb_info.result = &result;
		mhi_log(MHI_MSG_INFO, "Calling back for chan %d, reason %d\n",
					cb_info.chan, reason);
		client_handle->client_info.mhi_client_cb(&cb_info);
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

int set_mhi_base_state(struct mhi_pcie_dev_info *mhi_pcie_dev)
{
	u32 pcie_word_val = 0;
	int r = 0;
	struct mhi_device_ctxt *mhi_dev_ctxt = &mhi_pcie_dev->mhi_ctxt;

	mhi_pcie_dev->bhi_ctxt.bhi_base = mhi_pcie_dev->core.bar0_base;
	pcie_word_val = mhi_reg_read(mhi_pcie_dev->bhi_ctxt.bhi_base, BHIOFF);
	mhi_pcie_dev->bhi_ctxt.bhi_base += pcie_word_val;
	pcie_word_val = mhi_reg_read(mhi_pcie_dev->bhi_ctxt.bhi_base,
				     BHI_EXECENV);
	mhi_dev_ctxt->dev_exec_env = pcie_word_val;
	if (pcie_word_val == MHI_EXEC_ENV_AMSS) {
		mhi_dev_ctxt->base_state = STATE_TRANSITION_RESET;
	} else if (pcie_word_val == MHI_EXEC_ENV_PBL) {
		mhi_dev_ctxt->base_state = STATE_TRANSITION_BHI;
	} else {
		mhi_log(MHI_MSG_ERROR, "Invalid EXEC_ENV: 0x%x\n",
			pcie_word_val);
		r = -EIO;
	}
	mhi_log(MHI_MSG_INFO, "EXEC_ENV: %d Base state %d\n",
		pcie_word_val, mhi_dev_ctxt->base_state);
	return r;
}

void mhi_link_state_cb(struct msm_pcie_notify *notify)
{

	struct mhi_pcie_dev_info *mhi_pcie_dev;
	struct mhi_device_ctxt *mhi_dev_ctxt = NULL;

	if (NULL == notify || NULL == notify->data) {
		mhi_log(MHI_MSG_CRITICAL,
		"Incomplete handle received\n");
		return;
	}

	mhi_pcie_dev = notify->data;
	mhi_dev_ctxt = &mhi_pcie_dev->mhi_ctxt;
	switch (notify->event) {
	case MSM_PCIE_EVENT_LINKDOWN:
		mhi_log(MHI_MSG_INFO, "Received MSM_PCIE_EVENT_LINKDOWN\n");
		break;
	case MSM_PCIE_EVENT_LINKUP:
		mhi_log(MHI_MSG_INFO,
			"Received MSM_PCIE_EVENT_LINKUP\n");
		mhi_pcie_dev->link_up_cntr++;
		break;
	case MSM_PCIE_EVENT_WAKEUP:
		mhi_log(MHI_MSG_INFO,
			"Received MSM_PCIE_EVENT_WAKE\n");
		__pm_stay_awake(&mhi_dev_ctxt->w_lock);
		__pm_relax(&mhi_dev_ctxt->w_lock);

		if (mhi_dev_ctxt->flags.mhi_initialized) {
			pm_runtime_get(&mhi_dev_ctxt->
				       dev_info->pcie_device->dev);
			pm_runtime_mark_last_busy(&mhi_dev_ctxt->
						  dev_info->pcie_device->dev);
			pm_runtime_put_noidle(&mhi_dev_ctxt->
					      dev_info->pcie_device->dev);
		}
		break;
	default:
		mhi_log(MHI_MSG_INFO,
			"Received bad link event\n");
		return;
		}
}

int init_mhi_base_state(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int r = 0;

	r = mhi_init_state_transition(mhi_dev_ctxt, mhi_dev_ctxt->base_state);
	if (r) {
		mhi_log(MHI_MSG_CRITICAL,
		"Failed to start state change event, to %d\n",
		mhi_dev_ctxt->base_state);
	}
	return r;
}
