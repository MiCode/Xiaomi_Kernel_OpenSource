/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
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

#include <asm/dma-iommu.h>
#include <linux/async.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/esoc_client.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/msm-bus.h>
#include <linux/msm_pcie.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/mhi.h>
#include "mhi_qcom.h"

struct arch_info {
	struct mhi_dev *mhi_dev;
	struct esoc_desc *esoc_client;
	struct esoc_client_hook esoc_ops;
	struct msm_bus_scale_pdata *msm_bus_pdata;
	u32 bus_client;
	struct msm_pcie_register_event pcie_reg_event;
	struct pci_saved_state *pcie_state;
	struct dma_iommu_mapping *mapping;
	async_cookie_t cookie;
	void *boot_ipc_log;
	void *tsync_ipc_log;
	struct mhi_device *boot_dev;
	struct notifier_block pm_notifier;
	struct completion pm_completion;
};

/* ipc log markings */
#define DLOG "Dev->Host: "
#define HLOG "Host: "

#define MHI_TSYNC_LOG_PAGES (2)

#ifdef CONFIG_MHI_DEBUG

#define MHI_IPC_LOG_PAGES (100)
#define MHI_CNTRL_LOG_PAGES (25)
enum MHI_DEBUG_LEVEL  mhi_ipc_log_lvl = MHI_MSG_LVL_VERBOSE;

#else

#define MHI_IPC_LOG_PAGES (10)
#define MHI_CNTRL_LOG_PAGES (5)
enum MHI_DEBUG_LEVEL  mhi_ipc_log_lvl = MHI_MSG_LVL_ERROR;

#endif

void mhi_reg_write_work(struct work_struct *w)
{
	struct mhi_controller *mhi_cntrl = container_of(w,
						struct mhi_controller,
						reg_write_work);
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct pci_dev *pci_dev = mhi_dev->pci_dev;
	struct reg_write_info *info =
				&mhi_cntrl->reg_write_q[mhi_cntrl->read_idx];

	if (!info->valid)
		return;

	if (!mhi_is_active(mhi_cntrl->mhi_dev))
		return;

	if (msm_pcie_prevent_l1(pci_dev))
		return;

	while (info->valid) {
		if (!mhi_is_active(mhi_cntrl->mhi_dev))
			break;

		writel_relaxed(info->val, info->reg_addr);
		info->valid = false;
		mhi_cntrl->read_idx =
				(mhi_cntrl->read_idx + 1) &
						(REG_WRITE_QUEUE_LEN - 1);
		info = &mhi_cntrl->reg_write_q[mhi_cntrl->read_idx];
	}

	msm_pcie_allow_l1(pci_dev);
}

static int mhi_arch_pm_notifier(struct notifier_block *nb,
				unsigned long event, void *unused)
{
	struct arch_info *arch_info =
		container_of(nb, struct arch_info, pm_notifier);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		reinit_completion(&arch_info->pm_completion);
		break;

	case PM_POST_SUSPEND:
		complete_all(&arch_info->pm_completion);
		break;
	}

	return NOTIFY_DONE;
}

static void mhi_arch_timesync_log(struct mhi_controller *mhi_cntrl,
				  u64 remote_time)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct arch_info *arch_info = mhi_dev->arch_info;

	if (remote_time != U64_MAX)
		ipc_log_string(arch_info->tsync_ipc_log, "%6llu.%06llu 0x%llx",
			       REMOTE_TICKS_TO_SEC(remote_time),
			       REMOTE_TIME_REMAINDER_US(remote_time),
			       remote_time);
}

static int mhi_arch_set_bus_request(struct mhi_controller *mhi_cntrl, int index)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct arch_info *arch_info = mhi_dev->arch_info;

	MHI_LOG("Setting bus request to index %d\n", index);

	if (arch_info->bus_client)
		return msm_bus_scale_client_update_request(
							arch_info->bus_client,
							index);

	/* default return success */
	return 0;
}

static void mhi_arch_pci_link_state_cb(struct msm_pcie_notify *notify)
{
	struct mhi_controller *mhi_cntrl = notify->data;
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct pci_dev *pci_dev = mhi_dev->pci_dev;

	switch (notify->event) {
	case MSM_PCIE_EVENT_WAKEUP:
		MHI_CNTRL_LOG("Received PCIE_WAKE signal\n");

		/* bring link out of d3cold */
		if (mhi_dev->powered_on) {
			pm_runtime_get(&pci_dev->dev);
			pm_runtime_put_noidle(&pci_dev->dev);
		}
		break;
	case MSM_PCIE_EVENT_L1SS_TIMEOUT:
		MHI_VERB("Received PCIE_L1SS_TIMEOUT signal\n");

		pm_runtime_mark_last_busy(&pci_dev->dev);
		pm_request_autosuspend(&pci_dev->dev);
		break;
	default:
		MHI_CNTRL_LOG("Unhandled event 0x%x\n", notify->event);
	}
}

static int mhi_arch_esoc_ops_power_on(void *priv, unsigned int flags)
{
	struct mhi_controller *mhi_cntrl = priv;
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct pci_dev *pci_dev = mhi_dev->pci_dev;
	int ret;

	mutex_lock(&mhi_cntrl->pm_mutex);
	if (mhi_dev->powered_on) {
		MHI_CNTRL_LOG("MHI still in active state\n");
		mutex_unlock(&mhi_cntrl->pm_mutex);
		return 0;
	}

	MHI_CNTRL_LOG("Enter: mdm_crashed:%d\n", flags & ESOC_HOOK_MDM_CRASH);

	/* reset rpm state */
	pm_runtime_set_active(&pci_dev->dev);
	pm_runtime_enable(&pci_dev->dev);
	mutex_unlock(&mhi_cntrl->pm_mutex);
	pm_runtime_forbid(&pci_dev->dev);
	ret = pm_runtime_get_sync(&pci_dev->dev);
	if (ret < 0) {
		MHI_CNTRL_ERR("Error with rpm resume, ret:%d\n", ret);
		return ret;
	}

	/* re-start the link & recover default cfg states */
	ret = msm_pcie_pm_control(MSM_PCIE_RESUME, pci_dev->bus->number,
				  pci_dev, NULL, 0);
	if (ret) {
		MHI_CNTRL_ERR("Failed to resume pcie bus ret %d\n", ret);
		return ret;
	}

	mhi_dev->mdm_state = (flags & ESOC_HOOK_MDM_CRASH);
	ret = mhi_pci_probe(pci_dev, NULL);
	if (ret)
		mhi_dev->powered_on = false;

	return ret;
}

static void mhi_arch_link_off(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct pci_dev *pci_dev = mhi_dev->pci_dev;

	MHI_CNTRL_LOG("Entered\n");

	pci_set_power_state(pci_dev, PCI_D3hot);

	/* release the resources */
	msm_pcie_pm_control(MSM_PCIE_SUSPEND, mhi_cntrl->bus, pci_dev, NULL, 0);
	mhi_arch_set_bus_request(mhi_cntrl, 0);

	MHI_CNTRL_LOG("Exited\n");
}

static void mhi_arch_esoc_ops_power_off(void *priv, unsigned int flags)
{
	struct mhi_controller *mhi_cntrl = priv;
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	bool mdm_state = (flags & ESOC_HOOK_MDM_CRASH);
	struct arch_info *arch_info = mhi_dev->arch_info;
	struct pci_dev *pci_dev = mhi_dev->pci_dev;

	MHI_CNTRL_LOG("Enter: mdm_crashed:%d\n", mdm_state);

	/*
	 * Abort system suspend if system is preparing to go to suspend
	 * by grabbing wake source.
	 * If system is suspended, wait for pm notifier callback to notify
	 * that resume has occurred with PM_POST_SUSPEND event.
	 */
	pm_stay_awake(&mhi_cntrl->mhi_dev->dev);
	wait_for_completion(&arch_info->pm_completion);

	/* if link is in drv suspend, wake it up */
	pm_runtime_get_sync(&pci_dev->dev);

	mutex_lock(&mhi_cntrl->pm_mutex);
	if (!mhi_dev->powered_on) {
		MHI_CNTRL_LOG("Not in active state\n");
		mutex_unlock(&mhi_cntrl->pm_mutex);
		pm_runtime_put_noidle(&pci_dev->dev);
		return;
	}
	mhi_dev->powered_on = false;
	mutex_unlock(&mhi_cntrl->pm_mutex);

	pm_runtime_put_noidle(&pci_dev->dev);

	MHI_CNTRL_LOG("Triggering shutdown process\n");
	mhi_power_down(mhi_cntrl, !mdm_state);

	/* turn the link off */
	mhi_deinit_pci_dev(mhi_cntrl);
	mhi_arch_link_off(mhi_cntrl);

	/* wait for boot monitor to exit */
	async_synchronize_cookie(arch_info->cookie + 1);

	mhi_arch_iommu_deinit(mhi_cntrl);
	mhi_arch_pcie_deinit(mhi_cntrl);

	pm_relax(&mhi_cntrl->mhi_dev->dev);
}

static void mhi_arch_esoc_ops_mdm_error(void *priv)
{
	struct mhi_controller *mhi_cntrl = priv;

	MHI_CNTRL_LOG("Enter: mdm asserted\n");

	/* transition MHI state into error state */
	mhi_control_error(mhi_cntrl);
}

static void mhi_bl_dl_cb(struct mhi_device *mhi_device,
			 struct mhi_result *mhi_result)
{
	struct mhi_controller *mhi_cntrl = mhi_device->mhi_cntrl;
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct arch_info *arch_info = mhi_dev->arch_info;
	char *buf = mhi_result->buf_addr;
	char *token, *delim = "\n";

	/* force a null at last character */
	buf[mhi_result->bytes_xferd - 1] = 0;

	if (mhi_result->bytes_xferd >= MAX_MSG_SIZE) {
		do {
			token = strsep((char **)&buf, delim);
			if (token)
				ipc_log_string(arch_info->boot_ipc_log, "%s %s",
					       DLOG, token);
		} while (token);
	} else {
		ipc_log_string(arch_info->boot_ipc_log, "%s %s", DLOG, buf);
	}
}

static void mhi_bl_dummy_cb(struct mhi_device *mhi_dev,
			    struct mhi_result *mhi_result)
{
}

static void mhi_bl_remove(struct mhi_device *mhi_device)
{
	struct mhi_controller *mhi_cntrl = mhi_device->mhi_cntrl;
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct arch_info *arch_info = mhi_dev->arch_info;

	arch_info->boot_dev = NULL;
	ipc_log_string(arch_info->boot_ipc_log,
		       HLOG "Received Remove notif.\n");
}

static void mhi_boot_monitor(void *data, async_cookie_t cookie)
{
	struct mhi_controller *mhi_cntrl = data;
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct arch_info *arch_info = mhi_dev->arch_info;
	/* 15 sec timeout for booting device */
	const u32 timeout = msecs_to_jiffies(15000);

	/* wait for device to enter boot stage */
	wait_event_timeout(mhi_cntrl->state_event, mhi_cntrl->ee == MHI_EE_AMSS
			   || mhi_cntrl->ee == MHI_EE_DISABLE_TRANSITION
			   || mhi_cntrl->power_down,
			   timeout);

	ipc_log_string(arch_info->boot_ipc_log, HLOG "Device current ee = %s\n",
		       TO_MHI_EXEC_STR(mhi_cntrl->ee));
}

int mhi_arch_power_up(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct arch_info *arch_info = mhi_dev->arch_info;

	/* start a boot monitor if not in crashed state */
	if (!mhi_dev->mdm_state)
		arch_info->cookie = async_schedule(mhi_boot_monitor, mhi_cntrl);

	return 0;
}

void mhi_arch_mission_mode_enter(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct arch_info *arch_info = mhi_dev->arch_info;
	struct mhi_device *boot_dev = arch_info->boot_dev;

	/* disable boot logger channel */
	if (boot_dev)
		mhi_unprepare_from_transfer(boot_dev);

	pm_runtime_allow(&mhi_dev->pci_dev->dev);
}

static  int mhi_arch_pcie_scale_bw(struct mhi_controller *mhi_cntrl,
				   struct pci_dev *pci_dev,
				   struct mhi_link_info *link_info)
{
	int ret;

	ret = msm_pcie_set_link_bandwidth(pci_dev, link_info->target_link_speed,
					  link_info->target_link_width);
	if (ret)
		return ret;

	/* do a bus scale vote based on gen speeds */
	mhi_arch_set_bus_request(mhi_cntrl, link_info->target_link_speed);

	MHI_LOG("BW changed to speed:0x%x width:0x%x\n",
		link_info->target_link_speed,
		link_info->target_link_width);

	return 0;
}

static int mhi_arch_bw_scale(struct mhi_controller *mhi_cntrl,
			     struct mhi_link_info *link_info)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct pci_dev *pci_dev = mhi_dev->pci_dev;

	return mhi_arch_pcie_scale_bw(mhi_cntrl, pci_dev, link_info);
}

static int mhi_bl_probe(struct mhi_device *mhi_device,
			const struct mhi_device_id *id)
{
	char node_name[32];
	struct mhi_controller *mhi_cntrl = mhi_device->mhi_cntrl;
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct arch_info *arch_info = mhi_dev->arch_info;

	snprintf(node_name, sizeof(node_name), "mhi_bl_%04x_%02u.%02u.%02u",
		 mhi_device->dev_id, mhi_device->domain, mhi_device->bus,
		 mhi_device->slot);

	arch_info->boot_dev = mhi_device;
	arch_info->boot_ipc_log = ipc_log_context_create(MHI_CNTRL_LOG_PAGES,
							 node_name, 0);
	ipc_log_string(arch_info->boot_ipc_log, HLOG
		       "Entered SBL, Session ID:0x%x\n", mhi_cntrl->session_id);

	return 0;
}

static const struct mhi_device_id mhi_bl_match_table[] = {
	{ .chan = "BL" },
	{},
};

static struct mhi_driver mhi_bl_driver = {
	.id_table = mhi_bl_match_table,
	.remove = mhi_bl_remove,
	.probe = mhi_bl_probe,
	.ul_xfer_cb = mhi_bl_dummy_cb,
	.dl_xfer_cb = mhi_bl_dl_cb,
	.driver = {
		.name = "MHI_BL",
		.owner = THIS_MODULE,
	},
};

int mhi_arch_pcie_init(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct arch_info *arch_info = mhi_dev->arch_info;
	struct mhi_link_info *cur_link_info;
	char node[32];
	int ret;
	u16 linkstat;

	if (!arch_info) {
		struct msm_pcie_register_event *reg_event;

		arch_info = devm_kzalloc(&mhi_dev->pci_dev->dev,
					 sizeof(*arch_info), GFP_KERNEL);
		if (!arch_info)
			return -ENOMEM;

		mhi_dev->arch_info = arch_info;
		arch_info->mhi_dev = mhi_dev;

		snprintf(node, sizeof(node), "mhi_%04x_%02u.%02u.%02u",
			 mhi_cntrl->dev_id, mhi_cntrl->domain, mhi_cntrl->bus,
			 mhi_cntrl->slot);
		mhi_cntrl->log_buf = ipc_log_context_create(MHI_IPC_LOG_PAGES,
							    node, 0);
		mhi_cntrl->log_lvl = mhi_ipc_log_lvl;

		snprintf(node, sizeof(node), "mhi_cntrl_%04x_%02u.%02u.%02u",
			 mhi_cntrl->dev_id, mhi_cntrl->domain, mhi_cntrl->bus,
			 mhi_cntrl->slot);
		mhi_cntrl->cntrl_log_buf = ipc_log_context_create(
						MHI_CNTRL_LOG_PAGES, node, 0);

		snprintf(node, sizeof(node), "mhi_tsync_%04x_%02u.%02u.%02u",
			 mhi_cntrl->dev_id, mhi_cntrl->domain, mhi_cntrl->bus,
			 mhi_cntrl->slot);
		arch_info->tsync_ipc_log = ipc_log_context_create(
					   MHI_TSYNC_LOG_PAGES, node, 0);
		if (arch_info->tsync_ipc_log)
			mhi_cntrl->tsync_log = mhi_arch_timesync_log;

		/* register for bus scale if defined */
		arch_info->msm_bus_pdata = msm_bus_cl_get_pdata_from_dev(
							&mhi_dev->pci_dev->dev);
		if (arch_info->msm_bus_pdata) {
			arch_info->bus_client =
				msm_bus_scale_register_client(
						arch_info->msm_bus_pdata);
			if (!arch_info->bus_client)
				return -EINVAL;
		}

		/* register with pcie rc for WAKE# or link state events */
		reg_event = &arch_info->pcie_reg_event;
		reg_event->events = mhi_dev->allow_m1 ?
			(MSM_PCIE_EVENT_WAKEUP) :
			(MSM_PCIE_EVENT_WAKEUP | MSM_PCIE_EVENT_L1SS_TIMEOUT);

		reg_event->user = mhi_dev->pci_dev;
		reg_event->callback = mhi_arch_pci_link_state_cb;
		reg_event->notify.data = mhi_cntrl;
		ret = msm_pcie_register_event(reg_event);
		if (ret)
			MHI_CNTRL_ERR(
				"Failed to reg. for link up notification\n");

		init_completion(&arch_info->pm_completion);

		/* register PM notifier to get post resume events */
		arch_info->pm_notifier.notifier_call = mhi_arch_pm_notifier;
		register_pm_notifier(&arch_info->pm_notifier);

		/*
		 * Mark as completed at initial boot-up to allow ESOC power on
		 * callback to proceed if system has not gone to suspend
		 */
		complete_all(&arch_info->pm_completion);

		arch_info->esoc_client = devm_register_esoc_client(
						&mhi_dev->pci_dev->dev, "mdm");
		if (IS_ERR_OR_NULL(arch_info->esoc_client)) {
			MHI_CNTRL_ERR("Failed to register esoc client\n");
		} else {
			/* register for power on/off hooks */
			struct esoc_client_hook *esoc_ops =
				&arch_info->esoc_ops;

			esoc_ops->priv = mhi_cntrl;
			esoc_ops->prio = ESOC_MHI_HOOK;
			esoc_ops->esoc_link_power_on =
				mhi_arch_esoc_ops_power_on;
			esoc_ops->esoc_link_power_off =
				mhi_arch_esoc_ops_power_off;
			esoc_ops->esoc_link_mdm_crash =
				mhi_arch_esoc_ops_mdm_error;

			ret = esoc_register_client_hook(arch_info->esoc_client,
							esoc_ops);
			if (ret)
				MHI_CNTRL_ERR("Failed to register esoc ops\n");
		}

		/*
		 * MHI host driver has full autonomy to manage power state.
		 * Disable all automatic power collapse features
		 */
		msm_pcie_pm_control(MSM_PCIE_DISABLE_PC, mhi_cntrl->bus,
				    mhi_dev->pci_dev, NULL, 0);
		mhi_dev->pci_dev->no_d3hot = true;

		mhi_cntrl->bw_scale = mhi_arch_bw_scale;

		mhi_driver_register(&mhi_bl_driver);
	}

	/* store the current bw info */
	ret = pcie_capability_read_word(mhi_dev->pci_dev,
					PCI_EXP_LNKSTA, &linkstat);
	if (ret)
		return ret;

	cur_link_info = &mhi_cntrl->mhi_link_info;
	cur_link_info->target_link_speed = linkstat & PCI_EXP_LNKSTA_CLS;
	cur_link_info->target_link_width = (linkstat & PCI_EXP_LNKSTA_NLW) >>
					    PCI_EXP_LNKSTA_NLW_SHIFT;

	return mhi_arch_set_bus_request(mhi_cntrl,
					cur_link_info->target_link_speed);
}

void mhi_arch_pcie_deinit(struct mhi_controller *mhi_cntrl)
{
	mhi_arch_set_bus_request(mhi_cntrl, 0);
}

static struct dma_iommu_mapping *mhi_arch_create_iommu_mapping(
					struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	dma_addr_t base;
	size_t size;

	/*
	 * If S1_BYPASS enabled then iommu space is not used, however framework
	 * still require clients to create a mapping space before attaching. So
	 * set to smallest size required by iommu framework.
	 */
	if (mhi_dev->smmu_cfg & MHI_SMMU_S1_BYPASS) {
		base = 0;
		size = PAGE_SIZE;
	} else {
		base = mhi_dev->iova_start;
		size = (mhi_dev->iova_stop - base) + 1;
	}

	MHI_CNTRL_LOG("Create iommu mapping of base:%pad size:%zu\n",
			&base, size);
	return arm_iommu_create_mapping(&pci_bus_type, base, size);
}

int mhi_arch_iommu_init(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct arch_info *arch_info = mhi_dev->arch_info;
	u32 smmu_config = mhi_dev->smmu_cfg;
	struct dma_iommu_mapping *mapping = NULL;
	int ret;

	if (smmu_config) {
		mapping = mhi_arch_create_iommu_mapping(mhi_cntrl);
		if (IS_ERR(mapping)) {
			MHI_ERR("Failed to create iommu mapping\n");
			return PTR_ERR(mapping);
		}
	}

	if (smmu_config & MHI_SMMU_S1_BYPASS) {
		int s1_bypass = 1;

		ret = iommu_domain_set_attr(mapping->domain,
					    DOMAIN_ATTR_S1_BYPASS, &s1_bypass);
		if (ret) {
			MHI_CNTRL_ERR("Failed to set attribute S1_BYPASS\n");
			goto release_mapping;
		}
	}

	if (smmu_config & MHI_SMMU_FAST) {
		int fast_map = 1;

		ret = iommu_domain_set_attr(mapping->domain, DOMAIN_ATTR_FAST,
					    &fast_map);
		if (ret) {
			MHI_CNTRL_ERR("Failed to set attribute FAST_MAP\n");
			goto release_mapping;
		}
	}

	if (smmu_config & MHI_SMMU_ATOMIC) {
		int atomic = 1;

		ret = iommu_domain_set_attr(mapping->domain, DOMAIN_ATTR_ATOMIC,
					    &atomic);
		if (ret) {
			MHI_CNTRL_ERR("Failed to set attribute ATOMIC\n");
			goto release_mapping;
		}
	}

	if (smmu_config & MHI_SMMU_FORCE_COHERENT) {
		int force_coherent = 1;

		ret = iommu_domain_set_attr(mapping->domain,
					DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT,
					&force_coherent);
		if (ret) {
			MHI_CNTRL_ERR(
				"Failed to set attribute FORCE_COHERENT\n");
			goto release_mapping;
		}
	}

	if (smmu_config) {
		ret = arm_iommu_attach_device(&mhi_dev->pci_dev->dev, mapping);

		if (ret) {
			MHI_CNTRL_ERR("Error attach device, ret:%d\n", ret);
			goto release_mapping;
		}
		arch_info->mapping = mapping;
	}

	mhi_cntrl->dev = &mhi_dev->pci_dev->dev;

	ret = dma_set_mask_and_coherent(mhi_cntrl->dev, DMA_BIT_MASK(64));
	if (ret) {
		MHI_CNTRL_ERR("Error setting dma mask, ret:%d\n", ret);
		goto release_device;
	}

	return 0;

release_device:
	arm_iommu_detach_device(mhi_cntrl->dev);

release_mapping:
	arm_iommu_release_mapping(mapping);

	return ret;
}

void mhi_arch_iommu_deinit(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct arch_info *arch_info = mhi_dev->arch_info;
	struct dma_iommu_mapping *mapping = arch_info->mapping;

	if (mapping) {
		arm_iommu_detach_device(mhi_cntrl->dev);
		arm_iommu_release_mapping(mapping);
	}
	arch_info->mapping = NULL;
	mhi_cntrl->dev = NULL;
}

int mhi_arch_link_suspend(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct arch_info *arch_info = mhi_dev->arch_info;
	struct pci_dev *pci_dev = mhi_dev->pci_dev;
	int ret = 0;

	MHI_LOG("Entered with suspend_mode:%s\n",
		TO_MHI_SUSPEND_MODE_STR(mhi_dev->suspend_mode));

	/* disable inactivity timer */
	if (!mhi_dev->allow_m1)
		msm_pcie_l1ss_timeout_disable(pci_dev);

	switch (mhi_dev->suspend_mode) {
	case MHI_DEFAULT_SUSPEND:
		pci_clear_master(pci_dev);
		ret = pci_save_state(mhi_dev->pci_dev);
		if (ret) {
			MHI_CNTRL_ERR("Failed with pci_save_state, ret:%d\n",
				      ret);
			goto exit_suspend;
		}

		arch_info->pcie_state = pci_store_saved_state(pci_dev);
		pci_disable_device(pci_dev);

		pci_set_power_state(pci_dev, PCI_D3hot);

		/* release the resources */
		msm_pcie_pm_control(MSM_PCIE_SUSPEND, mhi_cntrl->bus, pci_dev,
				    NULL, 0);
		mhi_arch_set_bus_request(mhi_cntrl, 0);
		break;
	case MHI_FAST_LINK_OFF:
	case MHI_ACTIVE_STATE:
	case MHI_FAST_LINK_ON:/* keeping link on do nothing */
	default:
		break;
	}

exit_suspend:
	if (ret && !mhi_dev->allow_m1)
		msm_pcie_l1ss_timeout_enable(pci_dev);

	MHI_LOG("Exited with ret:%d\n", ret);

	return ret;
}

static int __mhi_arch_link_resume(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct arch_info *arch_info = mhi_dev->arch_info;
	struct pci_dev *pci_dev = mhi_dev->pci_dev;
	struct mhi_link_info *cur_info = &mhi_cntrl->mhi_link_info;
	int ret;

	/* request bus scale voting based on higher gen speed */
	ret = mhi_arch_set_bus_request(mhi_cntrl,
				       cur_info->target_link_speed);
	if (ret)
		MHI_LOG("Could not set bus frequency, ret:%d\n", ret);

	ret = msm_pcie_pm_control(MSM_PCIE_RESUME, mhi_cntrl->bus, pci_dev,
				  NULL, 0);
	if (ret) {
		MHI_ERR("Link training failed, ret:%d\n", ret);
		return ret;
	}

	ret = pci_set_power_state(pci_dev, PCI_D0);
	if (ret) {
		MHI_ERR("Failed to set PCI_D0 state, ret:%d\n", ret);
		return ret;
	}

	ret = pci_enable_device(pci_dev);
	if (ret) {
		MHI_ERR("Failed to enable device, ret:%d\n", ret);
		return ret;
	}

	ret = pci_load_and_free_saved_state(pci_dev, &arch_info->pcie_state);
	if (ret)
		MHI_LOG("Failed to load saved cfg state\n");

	pci_restore_state(pci_dev);
	pci_set_master(pci_dev);

	return 0;
}

int mhi_arch_link_resume(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct pci_dev *pci_dev = mhi_dev->pci_dev;
	int ret = 0;

	MHI_LOG("Entered with suspend_mode:%s\n",
		TO_MHI_SUSPEND_MODE_STR(mhi_dev->suspend_mode));

	switch (mhi_dev->suspend_mode) {
	case MHI_DEFAULT_SUSPEND:
		ret = __mhi_arch_link_resume(mhi_cntrl);
		break;
	case MHI_FAST_LINK_OFF:
	case MHI_ACTIVE_STATE:
	case MHI_FAST_LINK_ON:
	default:
		break;
	}

	if (ret) {
		MHI_ERR("Link training failed, ret:%d\n", ret);
		return ret;
	}

	if (!mhi_dev->allow_m1)
		msm_pcie_l1ss_timeout_enable(pci_dev);

	MHI_LOG("Exited with ret:%d\n", ret);

	return ret;
}

int mhi_arch_link_lpm_disable(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);

	return msm_pcie_prevent_l1(mhi_dev->pci_dev);
}

int mhi_arch_link_lpm_enable(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);

	msm_pcie_allow_l1(mhi_dev->pci_dev);

	return 0;
}
