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


#include <linux/pci.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/msm-bus.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/err.h>

#define CREATE_TRACE_POINTS
#include "mhi_trace.h"

#include "mhi_sys.h"
#include "mhi.h"
#include "mhi_macros.h"
#include "mhi_hwio.h"
#include "mhi_bhi.h"

struct mhi_pcie_devices mhi_devices;

static int mhi_pci_probe(struct pci_dev *pcie_device,
		const struct pci_device_id *mhi_device_id);
static int __exit mhi_plat_remove(struct platform_device *pdev);
void *mhi_ipc_log;

static DEFINE_PCI_DEVICE_TABLE(mhi_pcie_device_id) = {
	{ MHI_PCIE_VENDOR_ID, MHI_PCIE_DEVICE_ID_9x35,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ MHI_PCIE_VENDOR_ID, MHI_PCIE_DEVICE_ID_ZIRC,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ MHI_PCIE_VENDOR_ID, MHI_PCIE_DEVICE_ID_9x55,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ 0, },
};

static const struct of_device_id mhi_plat_match[] = {
	{
		.compatible = "qcom,mhi",
	},
	{},
};

static void mhi_msm_fixup(struct pci_dev *pcie_device)
{
	if (pcie_device->class == PCI_CLASS_NOT_DEFINED) {
		mhi_log(MHI_MSG_INFO, "Setting msm pcie class\n");
		pcie_device->class = PCI_CLASS_STORAGE_SCSI;
	}
}

int mhi_ctxt_init(struct mhi_pcie_dev_info *mhi_pcie_dev)
{
	int ret_val = 0;
	u32 i = 0, j = 0;
	u32 requested_msi_number = 32, actual_msi_number = 0;
	struct mhi_device_ctxt *mhi_dev_ctxt = NULL;
	struct pci_dev *pcie_device = NULL;

	if (NULL == mhi_pcie_dev)
		return -EINVAL;
	pcie_device = mhi_pcie_dev->pcie_device;

	ret_val = mhi_init_pcie_device(mhi_pcie_dev);
	if (ret_val) {
		mhi_log(MHI_MSG_CRITICAL,
				"Failed to initialize pcie device, ret %d\n",
				ret_val);
		return -ENODEV;
	}
	ret_val = mhi_init_device_ctxt(mhi_pcie_dev, &mhi_pcie_dev->mhi_ctxt);
	if (ret_val) {
		mhi_log(MHI_MSG_CRITICAL,
			"Failed to initialize main MHI ctxt ret %d\n",
			ret_val);
		goto msi_config_err;
	}
	ret_val = mhi_esoc_register(&mhi_pcie_dev->mhi_ctxt);
	if (ret_val) {
		mhi_log(MHI_MSG_ERROR,
				"Failed to register with esoc ret %d.\n",
				ret_val);
	}

	device_disable_async_suspend(&pcie_device->dev);
	ret_val = pci_enable_msi_range(pcie_device, 1, requested_msi_number);
	if (IS_ERR_VALUE(ret_val)) {
		mhi_log(MHI_MSG_ERROR,
			"Failed to enable MSIs for pcie dev ret_val %d.\n",
			ret_val);
		goto msi_config_err;
	} else if (ret_val) {
		mhi_log(MHI_MSG_INFO,
			"Hrmmm, got fewer MSIs than we requested. Requested %d, got %d.\n",
			requested_msi_number, ret_val);
		actual_msi_number = ret_val;
	} else {
		mhi_log(MHI_MSG_VERBOSE,
			"Got all requested MSIs, moving on\n");
	}
	mhi_dev_ctxt = &mhi_pcie_dev->mhi_ctxt;

	for (j = 0; j < mhi_dev_ctxt->mmio_info.nr_event_rings; j++) {
		mhi_log(MHI_MSG_VERBOSE,
				"MSI_number = %d, event ring number = %d\n",
				mhi_dev_ctxt->ev_ring_props[j].msi_vec, j);

		ret_val = request_irq(pcie_device->irq +
				mhi_dev_ctxt->ev_ring_props[j].msi_vec,
				mhi_dev_ctxt->ev_ring_props[j].mhi_handler_ptr,
				IRQF_NO_SUSPEND,
				"mhi_drv",
				(void *)&pcie_device->dev);
		if (ret_val) {
			mhi_log(MHI_MSG_ERROR,
			   "Failed to register handler for MSI ret_val = %d\n",
			   ret_val);
			goto msi_config_err;
		}
	}
	mhi_pcie_dev->core.irq_base = pcie_device->irq;
	mhi_log(MHI_MSG_VERBOSE,
		"Setting IRQ Base to 0x%x\n", mhi_pcie_dev->core.irq_base);
	mhi_pcie_dev->core.max_nr_msis = requested_msi_number;
	ret_val = mhi_init_pm_sysfs(&pcie_device->dev);
	if (ret_val) {
		mhi_log(MHI_MSG_ERROR, "Failed to setup sysfs ret %d\n",
								ret_val);
		goto sysfs_config_err;
	}
	if (!mhi_init_debugfs(&mhi_pcie_dev->mhi_ctxt))
		mhi_log(MHI_MSG_ERROR, "Failed to init debugfs.\n");

	mhi_pcie_dev->mhi_ctxt.mmio_info.mmio_addr =
						mhi_pcie_dev->core.bar0_base;
	pcie_device->dev.platform_data = &mhi_pcie_dev->mhi_ctxt;
	mhi_pcie_dev->mhi_ctxt.dev_info->plat_dev->dev.platform_data =
						&mhi_pcie_dev->mhi_ctxt;
	ret_val = mhi_reg_notifiers(&mhi_pcie_dev->mhi_ctxt);
	if (ret_val) {
		mhi_log(MHI_MSG_ERROR, "Failed to register for notifiers\n");
		goto mhi_state_transition_error;
	}
	mhi_log(MHI_MSG_INFO,
			"Finished all driver probing returning ret_val %d.\n",
			ret_val);
	return ret_val;

mhi_state_transition_error:
	kfree(mhi_dev_ctxt->state_change_work_item_list.q_lock);
	kfree(mhi_dev_ctxt->mhi_ev_wq.mhi_event_wq);
	kfree(mhi_dev_ctxt->mhi_ev_wq.state_change_event);
	kfree(mhi_dev_ctxt->mhi_ev_wq.m0_event);
	kfree(mhi_dev_ctxt->mhi_ev_wq.m3_event);
	kfree(mhi_dev_ctxt->mhi_ev_wq.bhi_event);
	dma_free_coherent(&mhi_dev_ctxt->dev_info->plat_dev->dev,
		   mhi_dev_ctxt->dev_space.dev_mem_len,
		   mhi_dev_ctxt->dev_space.dev_mem_start,
		   mhi_dev_ctxt->dev_space.dma_dev_mem_start);

	kfree(mhi_dev_ctxt->ev_ring_props);
	mhi_rem_pm_sysfs(&pcie_device->dev);
sysfs_config_err:
	for (; i >= 0; --i)
		free_irq(pcie_device->irq + i, &pcie_device->dev);
	debugfs_remove_recursive(mhi_pcie_dev->mhi_ctxt.mhi_parent_folder);
msi_config_err:
	pci_disable_device(pcie_device);
	return ret_val;
}

static const struct dev_pm_ops pm_ops = {
	SET_RUNTIME_PM_OPS(mhi_runtime_suspend,
			   mhi_runtime_resume,
			   mhi_runtime_idle)
	SET_SYSTEM_SLEEP_PM_OPS(mhi_pci_suspend, mhi_pci_resume)
};

static struct pci_driver mhi_pcie_driver = {
	.name = "mhi_pcie_drv",
	.id_table = mhi_pcie_device_id,
	.probe = mhi_pci_probe,
	.driver = {
		.pm = &pm_ops
	}
};

static int mhi_pci_probe(struct pci_dev *pcie_device,
			 const struct pci_device_id *mhi_device_id)
{
	int ret_val = 0;
	struct mhi_pcie_dev_info *mhi_pcie_dev = NULL;
	struct platform_device *plat_dev;
	struct mhi_device_ctxt *mhi_dev_ctxt;
	u32 nr_dev = mhi_devices.nr_of_devices;

	mhi_log(MHI_MSG_INFO, "Entering\n");
	mhi_pcie_dev = &mhi_devices.device_list[mhi_devices.nr_of_devices];
	if (mhi_devices.nr_of_devices + 1 > MHI_MAX_SUPPORTED_DEVICES) {
		mhi_log(MHI_MSG_ERROR, "Error: Too many devices\n");
		return -ENOMEM;
	}

	mhi_devices.nr_of_devices++;
	plat_dev = mhi_devices.device_list[nr_dev].plat_dev;
	pcie_device->dev.of_node = plat_dev->dev.of_node;
	mhi_dev_ctxt = &mhi_devices.device_list[nr_dev].mhi_ctxt;
	mhi_dev_ctxt->mhi_pm_state = MHI_PM_DISABLE;
	INIT_WORK(&mhi_dev_ctxt->process_m1_worker, process_m1_transition);
	mutex_init(&mhi_dev_ctxt->pm_lock);
	rwlock_init(&mhi_dev_ctxt->pm_xfer_lock);
	spin_lock_init(&mhi_dev_ctxt->dev_wake_lock);
	tasklet_init(&mhi_dev_ctxt->ev_task,
		     mhi_ctrl_ev_task,
		     (unsigned long)mhi_dev_ctxt);

	mhi_dev_ctxt->flags.link_up = 1;
	ret_val = mhi_set_bus_request(mhi_dev_ctxt, 1);
	mhi_pcie_dev->pcie_device = pcie_device;
	mhi_pcie_dev->mhi_pcie_driver = &mhi_pcie_driver;
	mhi_pcie_dev->mhi_pci_link_event.events =
			(MSM_PCIE_EVENT_LINKDOWN | MSM_PCIE_EVENT_WAKEUP);
	mhi_pcie_dev->mhi_pci_link_event.user = pcie_device;
	mhi_pcie_dev->mhi_pci_link_event.callback = mhi_link_state_cb;
	mhi_pcie_dev->mhi_pci_link_event.notify.data = mhi_pcie_dev;
	ret_val = msm_pcie_register_event(&mhi_pcie_dev->mhi_pci_link_event);
	if (ret_val) {
		mhi_log(MHI_MSG_ERROR,
			"Failed to register for link notifications %d.\n",
			ret_val);
		return ret_val;
	}

	/* Initialize MHI CNTXT */
	ret_val = mhi_ctxt_init(mhi_pcie_dev);
	if (ret_val) {
		mhi_log(MHI_MSG_ERROR,
			"MHI Initialization failed, ret %d\n",
			ret_val);
		goto deregister_pcie;
	}
	pci_set_master(mhi_pcie_dev->pcie_device);

	mutex_lock(&mhi_dev_ctxt->pm_lock);
	write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->mhi_pm_state = MHI_PM_POR;
	ret_val = set_mhi_base_state(mhi_pcie_dev);
	write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	if (ret_val) {
		mhi_log(MHI_MSG_ERROR,
			"Error Setting MHI Base State %d\n", ret_val);
		goto unlock_pm_lock;
	}

	if (mhi_dev_ctxt->base_state == STATE_TRANSITION_BHI) {
		ret_val = bhi_probe(mhi_pcie_dev);
		if (ret_val) {
			mhi_log(MHI_MSG_ERROR,
				"Error with bhi_probe ret:%d", ret_val);
			goto unlock_pm_lock;
		}
	}

	init_mhi_base_state(mhi_dev_ctxt);

	pm_runtime_set_autosuspend_delay(&pcie_device->dev,
					 MHI_RPM_AUTOSUSPEND_TMR_VAL_MS);
	pm_runtime_use_autosuspend(&pcie_device->dev);
	pm_suspend_ignore_children(&pcie_device->dev, true);

	/*
	 * pci framework will increment usage count (twice) before
	 * calling local device driver probe function.
	 * 1st pci.c pci_pm_init() calls pm_runtime_forbid
	 * 2nd pci-driver.c local_pci_probe calls pm_runtime_get_sync
	 * Framework expect pci device driver to call pm_runtime_put_noidle
	 * to decrement usage count after successful probe and
	 * and call pm_runtime_allow to enable runtime suspend.
	 * MHI will allow runtime after entering AMSS state.
	 */
	pm_runtime_mark_last_busy(&pcie_device->dev);
	pm_runtime_put_noidle(&pcie_device->dev);

	/*
	 * Keep the MHI state in Active (M0) state until AMSS because EP
	 * would error fatal if we try to enter M1 before entering
	 * AMSS state.
	 */
	read_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_assert_device_wake(mhi_dev_ctxt, false);
	read_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);

	mutex_unlock(&mhi_dev_ctxt->pm_lock);

	return 0;

unlock_pm_lock:
	mutex_unlock(&mhi_dev_ctxt->pm_lock);
deregister_pcie:
	msm_pcie_deregister_event(&mhi_pcie_dev->mhi_pci_link_event);
	return ret_val;
}

static int mhi_plat_probe(struct platform_device *pdev)
{
	u32 nr_dev = mhi_devices.nr_of_devices;
	struct mhi_device_ctxt *mhi_dev_ctxt;
	int r = 0;

	mhi_log(MHI_MSG_INFO, "Entered\n");
	mhi_dev_ctxt = &mhi_devices.device_list[nr_dev].mhi_ctxt;

	mhi_dev_ctxt->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (!mhi_dev_ctxt->bus_scale_table)
		return -ENODATA;
	mhi_dev_ctxt->bus_client = msm_bus_scale_register_client
		(mhi_dev_ctxt->bus_scale_table);
	if (!mhi_dev_ctxt->bus_client)
		return -EINVAL;

	mhi_devices.device_list[nr_dev].plat_dev = pdev;
	r = dma_set_mask(&pdev->dev, MHI_DMA_MASK);
	if (r)
		mhi_log(MHI_MSG_CRITICAL,
			"Failed to set mask for DMA ret %d\n", r);
	mhi_log(MHI_MSG_INFO, "Exited\n");
	return 0;
}

static struct platform_driver mhi_plat_driver = {
	.probe	= mhi_plat_probe,
	.remove	= mhi_plat_remove,
	.driver	= {
		.name		= "mhi",
		.owner		= THIS_MODULE,
		.of_match_table	= mhi_plat_match,
	},
};

static void __exit mhi_exit(void)
{
	ipc_log_context_destroy(mhi_ipc_log);
	pci_unregister_driver(&mhi_pcie_driver);
	platform_driver_unregister(&mhi_plat_driver);
}

static int __exit mhi_plat_remove(struct platform_device *pdev)
{
	platform_driver_unregister(&mhi_plat_driver);
	return 0;
}

static int __init mhi_init(void)
{
	int r;

	mhi_log(MHI_MSG_INFO, "Entered\n");
	r = platform_driver_register(&mhi_plat_driver);
	if (r) {
		mhi_log(MHI_MSG_INFO, "Failed to probe platform ret %d\n", r);
		return r;
	}
	r = pci_register_driver(&mhi_pcie_driver);
	if (r) {
		mhi_log(MHI_MSG_INFO,
				"Failed to register pcie drv ret %d\n", r);
		goto error;
	}
	mhi_ipc_log = ipc_log_context_create(MHI_IPC_LOG_PAGES, "mhi", 0);
	if (!mhi_ipc_log) {
		mhi_log(MHI_MSG_ERROR,
				"Failed to create IPC logging context\n");
	}
	mhi_log(MHI_MSG_INFO, "Exited\n");
	return 0;
error:
	pci_unregister_driver(&mhi_pcie_driver);
	return r;
}

DECLARE_PCI_FIXUP_HEADER(MHI_PCIE_VENDOR_ID,
		MHI_PCIE_DEVICE_ID_9x35,
		mhi_msm_fixup);

DECLARE_PCI_FIXUP_HEADER(MHI_PCIE_VENDOR_ID,
		MHI_PCIE_DEVICE_ID_9x55,
		mhi_msm_fixup);

DECLARE_PCI_FIXUP_HEADER(MHI_PCIE_VENDOR_ID,
		MHI_PCIE_DEVICE_ID_ZIRC,
		mhi_msm_fixup);


module_exit(mhi_exit);
module_init(mhi_init);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("MHI_CORE");
MODULE_DESCRIPTION("MHI Host Driver");
