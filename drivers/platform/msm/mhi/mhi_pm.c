/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/msm_mhi.h>
#include <linux/workqueue.h>
#include <linux/pm.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>

#include "mhi_sys.h"
#include "mhi.h"
#include "mhi_hwio.h"

/* Write only sysfs attributes */
static DEVICE_ATTR(MHI_M3, S_IWUSR, NULL, sysfs_init_m3);
static DEVICE_ATTR(MHI_M0, S_IWUSR, NULL, sysfs_init_m0);

/* Read only sysfs attributes */

static struct attribute *mhi_attributes[] = {
	&dev_attr_MHI_M3.attr,
	&dev_attr_MHI_M0.attr,
	NULL,
};

static struct attribute_group mhi_attribute_group = {
	.attrs = mhi_attributes,
};

int mhi_pci_suspend(struct pci_dev *pcie_dev, pm_message_t state)
{
	int r = 0;
	struct mhi_device_ctxt *mhi_dev_ctxt = pcie_dev->dev.platform_data;

	if (NULL == mhi_dev_ctxt)
		return -EINVAL;
	mhi_log(MHI_MSG_INFO, "Entered, sys state %d, MHI state %d\n",
			state.event, mhi_dev_ctxt->mhi_state);
	atomic_set(&mhi_dev_ctxt->flags.pending_resume, 1);
	r = cancel_work_sync(&mhi_dev_ctxt->m0_work);
	if (!r) {
		atomic_set(&mhi_dev_ctxt->flags.m0_work_enabled, 0);
		mhi_log(MHI_MSG_INFO, "M0 work cancelled\n");
	}

	r = mhi_initiate_m3(mhi_dev_ctxt);

	if (!r)
		return r;

	atomic_set(&mhi_dev_ctxt->flags.pending_resume, 0);
	mhi_log(MHI_MSG_ERROR, "Failing suspend sequence ret: %d\n",
						r);
	return r;
}

int mhi_pci_resume(struct pci_dev *pcie_dev)
{
	int r = 0;
	struct mhi_device_ctxt *mhi_dev_ctxt = pcie_dev->dev.platform_data;
	r = mhi_initiate_m0(mhi_dev_ctxt);
	if (r)
		goto exit;
	r = wait_event_interruptible_timeout(*mhi_dev_ctxt->M0_event,
			mhi_dev_ctxt->mhi_state == MHI_STATE_M0 ||
			mhi_dev_ctxt->mhi_state == MHI_STATE_M1,
			msecs_to_jiffies(MHI_MAX_SUSPEND_TIMEOUT));
	switch (r) {
	case 0:
		mhi_log(MHI_MSG_CRITICAL,
			"Timeout: No M0 event after %d ms\n",
			MHI_MAX_SUSPEND_TIMEOUT);
		mhi_dev_ctxt->counters.m0_event_timeouts++;
		r = -ETIME;
		break;
	case -ERESTARTSYS:
		mhi_log(MHI_MSG_CRITICAL,
			"Going Down...\n");
		break;
	default:
		mhi_log(MHI_MSG_INFO,
			"Wait complete state: %d\n", mhi_dev_ctxt->mhi_state);
		r = 0;
	}
exit:
	atomic_set(&mhi_dev_ctxt->flags.pending_resume, 0);
	return r;
}

enum hrtimer_restart mhi_initiate_m1(struct hrtimer *timer)
{
	int ret_val = 0;
	unsigned long flags;
	ktime_t curr_time, timer_inc;
	struct mhi_device_ctxt *mhi_dev_ctxt = container_of(timer,
						struct mhi_device_ctxt,
						m1_timer);
	write_lock_irqsave(&mhi_dev_ctxt->xfer_lock, flags);

	/*
	 * We will allow M1 if no data is pending, the current
	 * state is M0 and no M3 transition is pending
	 */
	if ((0 == atomic_read(&mhi_dev_ctxt->flags.data_pending)) &&
			(MHI_STATE_M1 == mhi_dev_ctxt->mhi_state ||
			 MHI_STATE_M0 == mhi_dev_ctxt->mhi_state) &&
			(0 == mhi_dev_ctxt->flags.pending_M3) &&
			mhi_dev_ctxt->flags.mhi_initialized &&
			(0 == atomic_read(
			&mhi_dev_ctxt->counters.outbound_acks))) {
		mhi_dev_ctxt->mhi_state = MHI_STATE_M1;
		ret_val = mhi_deassert_device_wake(mhi_dev_ctxt);
		mhi_dev_ctxt->counters.m0_m1++;
		if (ret_val)
			mhi_log(MHI_MSG_ERROR,
				"Could not set DEVICE WAKE GPIO LOW\n");
	}
	write_unlock_irqrestore(&mhi_dev_ctxt->xfer_lock, flags);
	if (mhi_dev_ctxt->mhi_state == MHI_STATE_M0 ||
	    mhi_dev_ctxt->mhi_state == MHI_STATE_M1 ||
	    mhi_dev_ctxt->mhi_state == MHI_STATE_READY) {
		curr_time = ktime_get();
		timer_inc = ktime_set(0, MHI_M1_ENTRY_DELAY_MS * 1E6L);
		hrtimer_forward(timer, curr_time, timer_inc);
		return HRTIMER_RESTART;
	}
	return HRTIMER_NORESTART;
}

int mhi_init_pm_sysfs(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &mhi_attribute_group);
}

void mhi_rem_pm_sysfs(struct device *dev)
{
	return sysfs_remove_group(&dev->kobj, &mhi_attribute_group);
}

ssize_t sysfs_init_m3(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	int r = 0;
	struct mhi_device_ctxt *mhi_dev_ctxt =
		&mhi_devices.device_list[0].mhi_ctxt;
	r = mhi_initiate_m3(mhi_dev_ctxt);
	if (r) {
		mhi_log(MHI_MSG_CRITICAL,
				"Failed to suspend %d\n", r);
		return r;
	}
	if (MHI_STATUS_SUCCESS != mhi_turn_off_pcie_link(mhi_dev_ctxt))
		mhi_log(MHI_MSG_CRITICAL,
				"Failed to turn off link\n");

	return count;
}

ssize_t sysfs_init_m0(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct mhi_device_ctxt *mhi_dev_ctxt =
		&mhi_devices.device_list[0].mhi_ctxt;
	if (MHI_STATUS_SUCCESS != mhi_turn_on_pcie_link(mhi_dev_ctxt)) {
		mhi_log(MHI_MSG_CRITICAL,
				"Failed to resume link\n");
		return count;
	}
	mhi_initiate_m0(mhi_dev_ctxt);
	mhi_log(MHI_MSG_CRITICAL,
			"Current mhi_state = 0x%x\n",
			mhi_dev_ctxt->mhi_state);

	return count;
}

enum MHI_STATUS mhi_turn_off_pcie_link(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int r;
	struct pci_dev *pcie_dev;
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	mhi_log(MHI_MSG_INFO, "Entered...\n");
	pcie_dev = mhi_dev_ctxt->dev_info->pcie_device;
	mutex_lock(&mhi_dev_ctxt->mhi_link_state);
	if (0 == mhi_dev_ctxt->flags.link_up) {
		mhi_log(MHI_MSG_CRITICAL,
			"Link already marked as down, nothing to do\n");
		goto exit;
	}
	/* Disable shadow to avoid restoring D3 hot struct device */
	r = msm_pcie_shadow_control(mhi_dev_ctxt->dev_info->pcie_device, 0);
	if (r)
		mhi_log(MHI_MSG_CRITICAL,
			"Failed to stop shadow config space: %d\n", r);

	r = pci_set_power_state(mhi_dev_ctxt->dev_info->pcie_device, PCI_D3hot);
	if (r) {
		mhi_log(MHI_MSG_CRITICAL,
			"Failed to set pcie power state to D3 hotret: %x\n", r);
		ret_val = MHI_STATUS_ERROR;
		goto exit;
	}
	r = msm_pcie_pm_control(MSM_PCIE_SUSPEND,
			mhi_dev_ctxt->dev_info->pcie_device->bus->number,
			mhi_dev_ctxt->dev_info->pcie_device,
			NULL,
			0);
	if (r)
		mhi_log(MHI_MSG_CRITICAL,
				"Failed to suspend pcie bus ret 0x%x\n", r);
	mhi_dev_ctxt->flags.link_up = 0;
exit:
	mutex_unlock(&mhi_dev_ctxt->mhi_link_state);
	mhi_log(MHI_MSG_INFO, "Exited...\n");
	return MHI_STATUS_SUCCESS;
}

enum MHI_STATUS mhi_turn_on_pcie_link(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int r = 0;
	struct pci_dev *pcie_dev;
	enum MHI_STATUS ret_val = MHI_STATUS_SUCCESS;
	pcie_dev = mhi_dev_ctxt->dev_info->pcie_device;

	mutex_lock(&mhi_dev_ctxt->mhi_link_state);
	mhi_log(MHI_MSG_INFO, "Entered...\n");
	if (mhi_dev_ctxt->flags.link_up)
		goto exit;
	r = msm_pcie_pm_control(MSM_PCIE_RESUME,
			mhi_dev_ctxt->dev_info->pcie_device->bus->number,
			mhi_dev_ctxt->dev_info->pcie_device,
			NULL, 0);
	if (r) {
		mhi_log(MHI_MSG_CRITICAL,
				"Failed to resume pcie bus ret %d\n", r);
		ret_val = MHI_STATUS_ERROR;
		goto exit;
	}

	atomic_dec(&mhi_dev_ctxt->flags.mhi_link_off);

	r = pci_set_power_state(mhi_dev_ctxt->dev_info->pcie_device,
				PCI_D0);
	if (r) {
		mhi_log(MHI_MSG_CRITICAL,
				"Failed to load stored state %d\n", r);
		ret_val = MHI_STATUS_ERROR;
		goto exit;
	}
	r = msm_pcie_recover_config(mhi_dev_ctxt->dev_info->pcie_device);
	if (r) {
		mhi_log(MHI_MSG_CRITICAL,
				"Failed to Recover config space ret: %d\n", r);
		ret_val = MHI_STATUS_ERROR;
		goto exit;
	}
	mhi_dev_ctxt->flags.link_up = 1;
exit:
	mutex_unlock(&mhi_dev_ctxt->mhi_link_state);
	mhi_log(MHI_MSG_INFO, "Exited...\n");
	return ret_val;
}

