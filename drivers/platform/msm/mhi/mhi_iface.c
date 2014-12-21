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


#include <linux/pci.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/msm-bus.h>
#include <linux/delay.h>
#include <linux/debugfs.h>

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
	{ MHI_PCIE_VENDOR_ID, MHI_PCIE_DEVICE_ID_9640,
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
	u32 i = 0;
	u32 retry_count = 0;
	struct pci_dev *pcie_device = NULL;

	if (NULL == mhi_pcie_dev)
		return -EINVAL;
	pcie_device = mhi_pcie_dev->pcie_device;

	ret_val = mhi_init_pcie_device(mhi_pcie_dev);
	if (0 != ret_val) {
		mhi_log(MHI_MSG_CRITICAL,
				"Failed to initialize pcie device, ret %d\n",
				ret_val);
		return -ENODEV;
	}
	ret_val = mhi_init_device_ctxt(mhi_pcie_dev,
					&mhi_pcie_dev->mhi_ctxt);
	if (MHI_STATUS_SUCCESS != ret_val) {
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
	mhi_pcie_dev->mhi_ctxt.bus_scale_table =
				msm_bus_cl_get_pdata(mhi_pcie_dev->plat_dev);
	mhi_pcie_dev->mhi_ctxt.bus_client =
		msm_bus_scale_register_client(
				mhi_pcie_dev->mhi_ctxt.bus_scale_table);
	if (!mhi_pcie_dev->mhi_ctxt.bus_client) {
		mhi_log(MHI_MSG_CRITICAL,
			"Could not register for bus control ret: %d.\n",
			mhi_pcie_dev->mhi_ctxt.bus_client);
	} else {
		ret_val = mhi_set_bus_request(&mhi_pcie_dev->mhi_ctxt, 1);
		if (ret_val)
			mhi_log(MHI_MSG_CRITICAL,
				"Could not set bus frequency ret: %d\n",
				ret_val);
	}

	device_disable_async_suspend(&pcie_device->dev);
	ret_val = pci_enable_msi_block(pcie_device, MAX_NR_MSI + 1);
	if (0 != ret_val) {
		mhi_log(MHI_MSG_ERROR,
			"Failed to enable MSIs for pcie dev ret_val %d.\n",
			ret_val);
		goto msi_config_err;
	}
	for (i = 0; i < MAX_NR_MSI; ++i) {
		ret_val = request_irq(pcie_device->irq + i,
					mhi_msi_handlr,
					IRQF_NO_SUSPEND,
					"mhi_drv",
					(void *)&pcie_device->dev);
		if (ret_val) {
			mhi_log(MHI_MSG_ERROR,
				"Failed to register handler for MSI.\n");
			goto msi_config_err;
		}
	}
	mhi_pcie_dev->core.irq_base = pcie_device->irq;
	mhi_log(MHI_MSG_VERBOSE,
		"Setting IRQ Base to 0x%x\n", mhi_pcie_dev->core.irq_base);
	mhi_pcie_dev->core.max_nr_msis = MAX_NR_MSI;
	do  {
		ret_val = mhi_init_gpios(mhi_pcie_dev);
		switch (ret_val) {
		case -EPROBE_DEFER:
			mhi_log(MHI_MSG_VERBOSE,
				"DT requested probe defer, wait and retry\n");
			break;
		case 0:
			break;
		default:
			mhi_log(MHI_MSG_CRITICAL,
				"Could not get gpio from struct device tree!\n");
			goto msi_config_err;
		}
		retry_count++;
	} while ((retry_count < DT_WAIT_RETRIES) && (ret_val == -EPROBE_DEFER));
	ret_val = mhi_init_pm_sysfs(&pcie_device->dev);
	if (ret_val != 0) {
		mhi_log(MHI_MSG_ERROR, "Failed to setup sysfs.\n");
		goto sysfs_config_err;
	}
	if (!mhi_init_debugfs(&mhi_pcie_dev->mhi_ctxt))
		mhi_log(MHI_MSG_ERROR, "Failed to init debugfs.\n");

	mhi_pcie_dev->mhi_ctxt.mmio_addr = mhi_pcie_dev->core.bar0_base;
	pcie_device->dev.platform_data = &mhi_pcie_dev->mhi_ctxt;
	if (mhi_pcie_dev->mhi_ctxt.base_state == STATE_TRANSITION_BHI) {
		ret_val = bhi_probe(mhi_pcie_dev);
		if (ret_val) {
			mhi_log(MHI_MSG_ERROR, "Failed to initialize BHI.\n");
			goto mhi_state_transition_error;
		}
	}
	if (MHI_STATUS_SUCCESS != mhi_reg_notifiers(&mhi_pcie_dev->mhi_ctxt)) {
		mhi_log(MHI_MSG_ERROR, "Failed to register for notifiers\n");
		return MHI_STATUS_ERROR;
	}
	mhi_log(MHI_MSG_INFO,
			"Finished all driver probing returning ret_val %d.\n",
			ret_val);
	return ret_val;

mhi_state_transition_error:
	if (MHI_STATUS_SUCCESS != mhi_clean_init_stage(&mhi_pcie_dev->mhi_ctxt,
				MHI_INIT_ERROR_STAGE_UNWIND_ALL))
		mhi_log(MHI_MSG_ERROR, "Could not clean up context\n");
	mhi_rem_pm_sysfs(&pcie_device->dev);
sysfs_config_err:
	gpio_free(mhi_pcie_dev->core.device_wake_gpio);
	for (; i >= 0; --i)
		free_irq(pcie_device->irq + i, &pcie_device->dev);
	debugfs_remove_recursive(mhi_pcie_dev->mhi_ctxt.mhi_parent_folder);
msi_config_err:
	pci_disable_msi(pcie_device);
	pci_disable_device(pcie_device);
	return ret_val;
}

static struct pci_driver mhi_pcie_driver = {
	.name = "mhi_pcie_drv",
	.id_table = mhi_pcie_device_id,
	.probe = mhi_pci_probe,
	.suspend = mhi_pci_suspend,
	.resume = mhi_pci_resume,
};

static int mhi_pci_probe(struct pci_dev *pcie_device,
		const struct pci_device_id *mhi_device_id)
{
	int ret_val = 0;
	struct mhi_pcie_dev_info *mhi_pcie_dev = NULL;
	struct platform_device *plat_dev;
	u32 nr_dev = mhi_devices.nr_of_devices;

	mhi_log(MHI_MSG_INFO, "Entering.\n");
	mhi_pcie_dev = &mhi_devices.device_list[mhi_devices.nr_of_devices];
	if (mhi_devices.nr_of_devices + 1 > MHI_MAX_SUPPORTED_DEVICES) {
		mhi_log(MHI_MSG_ERROR, "Error: Too many devices\n");
		return -ENOMEM;
	}

	mhi_devices.nr_of_devices++;
	plat_dev = mhi_devices.device_list[nr_dev].plat_dev;
	pcie_device->dev.of_node = plat_dev->dev.of_node;
	mhi_pcie_dev->pcie_device = pcie_device;
	mhi_pcie_dev->mhi_pcie_driver = &mhi_pcie_driver;
	mhi_pcie_dev->mhi_pci_link_event.events =
			(MSM_PCIE_EVENT_LINKDOWN | MSM_PCIE_EVENT_LINKUP |
			 MSM_PCIE_EVENT_WAKEUP);
	mhi_pcie_dev->mhi_pci_link_event.user = pcie_device;
	mhi_pcie_dev->mhi_pci_link_event.callback = mhi_link_state_cb;
	mhi_pcie_dev->mhi_pci_link_event.notify.data = mhi_pcie_dev;
	ret_val = msm_pcie_register_event(&mhi_pcie_dev->mhi_pci_link_event);
	if (ret_val)
		mhi_log(MHI_MSG_ERROR,
			"Failed to register for link notifications %d.\n",
			ret_val);
	return ret_val;
}

static int mhi_plat_probe(struct platform_device *pdev)
{
	u32 nr_dev = mhi_devices.nr_of_devices;
	mhi_log(MHI_MSG_INFO, "Entered\n");
	mhi_devices.device_list[nr_dev].plat_dev = pdev;
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
		MHI_PCIE_DEVICE_ID_9640,
		mhi_msm_fixup);


module_exit(mhi_exit);
module_init(mhi_init);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("MHI_CORE");
MODULE_DESCRIPTION("MHI Host Driver");
