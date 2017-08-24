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

struct mhi_device_driver *mhi_device_drv;

static int mhi_pci_probe(struct pci_dev *pcie_device,
		const struct pci_device_id *mhi_device_id);
static int __exit mhi_plat_remove(struct platform_device *pdev);

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
		pcie_device->class = PCI_CLASS_STORAGE_SCSI;
	}
}

int mhi_ctxt_init(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	int ret_val = 0;
	u32 j = 0;

	ret_val = mhi_init_device_ctxt(mhi_dev_ctxt);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Failed to initialize main MHI ctxt ret %d\n", ret_val);
		return ret_val;
	}

	for (j = 0; j < mhi_dev_ctxt->mmio_info.nr_event_rings; j++) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
			"MSI_number = %d, event ring number = %d\n",
			mhi_dev_ctxt->ev_ring_props[j].msi_vec, j);

		/* outside of requested irq boundary */
		if (mhi_dev_ctxt->core.max_nr_msis <=
		    mhi_dev_ctxt->ev_ring_props[j].msi_vec) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
				"max msi supported:%d request:%d ev:%d\n",
				mhi_dev_ctxt->core.max_nr_msis,
				mhi_dev_ctxt->ev_ring_props[j].msi_vec,
				j);
			goto irq_error;
		}
		ret_val = request_irq(mhi_dev_ctxt->core.irq_base +
				mhi_dev_ctxt->ev_ring_props[j].msi_vec,
				mhi_dev_ctxt->ev_ring_props[j].mhi_handler_ptr,
				IRQF_NO_SUSPEND,
				"mhi_drv",
				(void *)mhi_dev_ctxt);
		if (ret_val) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
				"Failed to register handler for MSI ret_val = %d\n",
				ret_val);
			goto irq_error;
		}
	}

	mhi_dev_ctxt->mmio_info.mmio_addr = mhi_dev_ctxt->core.bar0_base;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "exit\n");
	return 0;

irq_error:
	kfree(mhi_dev_ctxt->state_change_work_item_list.q_lock);
	kfree(mhi_dev_ctxt->mhi_ev_wq.m0_event);
	kfree(mhi_dev_ctxt->mhi_ev_wq.m3_event);
	kfree(mhi_dev_ctxt->mhi_ev_wq.bhi_event);
	dma_free_coherent(&mhi_dev_ctxt->plat_dev->dev,
		   mhi_dev_ctxt->dev_space.dev_mem_len,
		   mhi_dev_ctxt->dev_space.dev_mem_start,
		   mhi_dev_ctxt->dev_space.dma_dev_mem_start);

	kfree(mhi_dev_ctxt->ev_ring_props);
	for (j = j - 1; j >= 0; --j)
		free_irq(mhi_dev_ctxt->core.irq_base + j, NULL);

	return -EINVAL;
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
	struct platform_device *plat_dev;
	struct mhi_device_ctxt *mhi_dev_ctxt = NULL, *itr;
	u32 domain = pci_domain_nr(pcie_device->bus);
	u32 bus = pcie_device->bus->number;
	u32 dev_id = pcie_device->device;
	u32 slot = PCI_SLOT(pcie_device->devfn);
	unsigned long msi_requested, msi_required;
	struct msm_pcie_register_event *mhi_pci_link_event;
	struct pcie_core_info *core;
	int i;
	char node[32];

	/* Find correct device context based on bdf & dev_id */
	mutex_lock(&mhi_device_drv->lock);
	list_for_each_entry(itr, &mhi_device_drv->head, node) {
		core = &itr->core;
		if (core->domain == domain && core->bus == bus &&
		    (core->dev_id == PCI_ANY_ID || (core->dev_id == dev_id)) &&
		    core->slot == slot) {
			/* change default dev_id to actual dev_id */
			core->dev_id = dev_id;
			mhi_dev_ctxt = itr;
			break;
		}
	}
	mutex_unlock(&mhi_device_drv->lock);
	if (!mhi_dev_ctxt)
		return -EPROBE_DEFER;

	snprintf(node, sizeof(node), "mhi_%04x_%02u.%02u.%02u",
		 core->dev_id, core->domain, core->bus, core->slot);
	mhi_dev_ctxt->mhi_ipc_log =
		ipc_log_context_create(MHI_IPC_LOG_PAGES, node, 0);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Processing Domain:%02u Bus:%04u dev:0x%04x slot:%04u\n",
		domain, bus, dev_id, slot);

	ret_val = of_property_read_u32(mhi_dev_ctxt->plat_dev->dev.of_node,
				       "mhi-event-rings",
				       (u32 *)&msi_required);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Failed to pull ev ring info from DT, %d\n", ret_val);
		return ret_val;
	}

	plat_dev = mhi_dev_ctxt->plat_dev;
	pcie_device->dev.of_node = plat_dev->dev.of_node;
	mhi_dev_ctxt->mhi_pm_state = MHI_PM_DISABLE;
	INIT_WORK(&mhi_dev_ctxt->process_m1_worker, process_m1_transition);
	INIT_WORK(&mhi_dev_ctxt->st_thread_worker, mhi_state_change_worker);
	INIT_WORK(&mhi_dev_ctxt->process_sys_err_worker, mhi_sys_err_worker);
	mutex_init(&mhi_dev_ctxt->pm_lock);
	rwlock_init(&mhi_dev_ctxt->pm_xfer_lock);
	spin_lock_init(&mhi_dev_ctxt->dev_wake_lock);
	init_completion(&mhi_dev_ctxt->cmd_complete);
	mhi_dev_ctxt->flags.link_up = 1;

	/* Setup bus scale */
	mhi_dev_ctxt->bus_scale_table = msm_bus_cl_get_pdata(plat_dev);
	if (!mhi_dev_ctxt->bus_scale_table)
		return -ENODATA;
	mhi_dev_ctxt->bus_client = msm_bus_scale_register_client
		(mhi_dev_ctxt->bus_scale_table);
	if (!mhi_dev_ctxt->bus_client)
		return -EINVAL;
	mhi_set_bus_request(mhi_dev_ctxt, 1);

	mhi_dev_ctxt->pcie_device = pcie_device;

	mhi_pci_link_event = &mhi_dev_ctxt->mhi_pci_link_event;
	mhi_pci_link_event->events =
		(MSM_PCIE_EVENT_LINKDOWN | MSM_PCIE_EVENT_WAKEUP);
	mhi_pci_link_event->user = pcie_device;
	mhi_pci_link_event->callback = mhi_link_state_cb;
	mhi_pci_link_event->notify.data = mhi_dev_ctxt;
	ret_val = msm_pcie_register_event(mhi_pci_link_event);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to reg for link notifications %d\n", ret_val);
		return ret_val;
	}

	dev_set_drvdata(&pcie_device->dev, mhi_dev_ctxt);

	mhi_dev_ctxt->core.pci_master = true;
	ret_val = mhi_init_pcie_device(mhi_dev_ctxt);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt,
			MHI_MSG_CRITICAL,
			"Failed to initialize pcie device, ret %d\n",
			ret_val);
		return ret_val;
	}
	pci_set_master(pcie_device);
	device_disable_async_suspend(&pcie_device->dev);

	ret_val = mhi_esoc_register(mhi_dev_ctxt);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Failed to reg with esoc ret %d\n", ret_val);
	}

	/* # of MSI requested must be power of 2 */
	msi_requested = 1 << find_last_bit(&msi_required, 32);
	if (msi_requested < msi_required)
		msi_requested <<= 1;

	ret_val = pci_enable_msi_range(pcie_device, 1, msi_requested);
	if (IS_ERR_VALUE(ret_val) || (ret_val < msi_requested)) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to enable MSIs for pcie dev ret_val %d.\n",
			ret_val);
		return -EIO;
	}

	mhi_dev_ctxt->core.max_nr_msis = msi_requested;
	mhi_dev_ctxt->core.irq_base = pcie_device->irq;
	mhi_log(mhi_dev_ctxt, MHI_MSG_VERBOSE,
		"Setting IRQ Base to 0x%x\n", mhi_dev_ctxt->core.irq_base);

	/* Initialize MHI CNTXT */
	ret_val = mhi_ctxt_init(mhi_dev_ctxt);
	if (ret_val) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"MHI Initialization failed, ret %d\n", ret_val);
		goto deregister_pcie;
	}

	mhi_init_pm_sysfs(&pcie_device->dev);
	mhi_init_debugfs(mhi_dev_ctxt);
	mhi_reg_notifiers(mhi_dev_ctxt);

	/* setup shadow pm functions */
	mhi_dev_ctxt->assert_wake = mhi_assert_device_wake;
	mhi_dev_ctxt->deassert_wake = mhi_deassert_device_wake;
	mhi_dev_ctxt->runtime_get = mhi_master_mode_runtime_get;
	mhi_dev_ctxt->runtime_put = mhi_master_mode_runtime_put;

	mutex_lock(&mhi_dev_ctxt->pm_lock);
	write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	mhi_dev_ctxt->mhi_pm_state = MHI_PM_POR;
	write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);

	/* notify all registered clients we probed */
	for (i = 0; i < MHI_MAX_CHANNELS; i++) {
		struct mhi_client_handle *client_handle =
			mhi_dev_ctxt->client_handle_list[i];

		if (!client_handle)
			continue;
		client_handle->dev_id = core->dev_id;
		mhi_notify_client(client_handle, MHI_CB_MHI_PROBED);
	}
	write_lock_irq(&mhi_dev_ctxt->pm_xfer_lock);
	ret_val = set_mhi_base_state(mhi_dev_ctxt);
	write_unlock_irq(&mhi_dev_ctxt->pm_xfer_lock);

	if (ret_val) {
		mhi_log(mhi_dev_ctxt,
			MHI_MSG_ERROR,
			"Error Setting MHI Base State %d\n", ret_val);
		goto unlock_pm_lock;
	}

	if (mhi_dev_ctxt->base_state == STATE_TRANSITION_BHI) {
		ret_val = bhi_probe(mhi_dev_ctxt);
		if (ret_val) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
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
	msm_pcie_deregister_event(&mhi_dev_ctxt->mhi_pci_link_event);
	return ret_val;
}

static int mhi_plat_probe(struct platform_device *pdev)
{
	int r = 0, len;
	struct mhi_device_ctxt *mhi_dev_ctxt;
	struct pcie_core_info *core;
	struct device_node *of_node = pdev->dev.of_node;
	u64 address_window[2];

	if (of_node == NULL)
		return -ENODEV;

	pdev->id = of_alias_get_id(of_node, "mhi");
	if (pdev->id < 0)
		return -ENODEV;

	mhi_dev_ctxt = devm_kzalloc(&pdev->dev,
				    sizeof(*mhi_dev_ctxt),
				    GFP_KERNEL);
	if (!mhi_dev_ctxt)
		return -ENOMEM;

	if (!of_find_property(of_node, "qcom,mhi-address-window", &len))
		return -ENODEV;

	if (len != sizeof(address_window))
		return -ENODEV;

	r = of_property_read_u64_array(of_node,
				       "qcom,mhi-address-window",
				       address_window,
				       sizeof(address_window) / sizeof(u64));
	if (r)
		return r;

	core = &mhi_dev_ctxt->core;
	r = of_property_read_u32(of_node, "qcom,pci-dev_id", &core->dev_id);
	if (r)
		core->dev_id = PCI_ANY_ID;

	r = of_property_read_u32(of_node, "qcom,pci-slot", &core->slot);
	if (r)
		return r;

	r = of_property_read_u32(of_node, "qcom,pci-domain", &core->domain);
	if (r)
		return r;

	r = of_property_read_u32(of_node, "qcom,pci-bus", &core->bus);
	if (r)
		return r;

	r = of_property_read_u32(of_node, "qcom,mhi-ready-timeout",
				 &mhi_dev_ctxt->poll_reset_timeout_ms);
	if (r)
		mhi_dev_ctxt->poll_reset_timeout_ms =
			MHI_READY_STATUS_TIMEOUT_MS;

	mhi_dev_ctxt->dev_space.start_win_addr = address_window[0];
	mhi_dev_ctxt->dev_space.end_win_addr = address_window[1];

	r = of_property_read_u32(of_node, "qcom,bhi-alignment",
				 &mhi_dev_ctxt->bhi_ctxt.alignment);
	if (r)
		mhi_dev_ctxt->bhi_ctxt.alignment = BHI_DEFAULT_ALIGNMENT;

	r = of_property_read_u32(of_node, "qcom,bhi-poll-timeout",
				 &mhi_dev_ctxt->bhi_ctxt.poll_timeout);
	if (r)
		mhi_dev_ctxt->bhi_ctxt.poll_timeout = BHI_POLL_TIMEOUT_MS;

	mhi_dev_ctxt->bhi_ctxt.manage_boot =
		of_property_read_bool(pdev->dev.of_node,
				      "qcom,mhi-manage-boot");
	if (mhi_dev_ctxt->bhi_ctxt.manage_boot) {
		struct bhi_ctxt_t *bhi_ctxt = &mhi_dev_ctxt->bhi_ctxt;
		struct firmware_info *fw_info = &bhi_ctxt->firmware_info;

		r = of_property_read_string(of_node, "qcom,mhi-fw-image",
					    &fw_info->fw_image);
		if (r) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
				"Error reading DT node 'qcom,mhi-fw-image'\n");
			return r;
		}
		r = of_property_read_u32(of_node, "qcom,mhi-max-sbl",
					 (u32 *)&fw_info->max_sbl_len);
		if (r) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
				"Error reading DT node 'qcom,mhi-max-sbl'\n");
			return r;
		}
		r = of_property_read_u32(of_node, "qcom,mhi-sg-size",
					 (u32 *)&fw_info->segment_size);
		if (r) {
			mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
				"Error reading DT node 'qcom,mhi-sg-size'\n");
			return r;
		}
		INIT_WORK(&bhi_ctxt->fw_load_work, bhi_firmware_download);
	}

	mhi_dev_ctxt->flags.bb_required =
		of_property_read_bool(pdev->dev.of_node,
				      "qcom,mhi-bb-required");

	mhi_dev_ctxt->plat_dev = pdev;
	platform_set_drvdata(pdev, mhi_dev_ctxt);

	r = dma_set_mask(&pdev->dev, MHI_DMA_MASK);
	if (r) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Failed to set mask for DMA ret %d\n", r);
		return r;
	}

	mhi_dev_ctxt->parent = mhi_device_drv->parent;
	mutex_lock(&mhi_device_drv->lock);
	list_add_tail(&mhi_dev_ctxt->node, &mhi_device_drv->head);
	mutex_unlock(&mhi_device_drv->lock);

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
	pci_unregister_driver(&mhi_pcie_driver);
	platform_driver_unregister(&mhi_plat_driver);
}

static int __exit mhi_plat_remove(struct platform_device *pdev)
{
	struct mhi_device_ctxt *mhi_dev_ctxt = platform_get_drvdata(pdev);

	ipc_log_context_destroy(mhi_dev_ctxt->mhi_ipc_log);
	return 0;
}

static int __init mhi_init(void)
{
	int r = -EAGAIN;
	struct mhi_device_driver *mhi_dev_drv;

	mhi_dev_drv = kmalloc(sizeof(*mhi_dev_drv), GFP_KERNEL);
	if (mhi_dev_drv == NULL)
		return -ENOMEM;

	mutex_init(&mhi_dev_drv->lock);
	mutex_lock(&mhi_dev_drv->lock);
	INIT_LIST_HEAD(&mhi_dev_drv->head);
	mutex_unlock(&mhi_dev_drv->lock);
	mhi_dev_drv->mhi_bhi_class = class_create(THIS_MODULE, "bhi");
	if (IS_ERR(mhi_dev_drv->mhi_bhi_class)) {
		pr_err("Error creating mhi_bhi_class\n");
		goto class_error;
	}
	mhi_dev_drv->parent = debugfs_create_dir("mhi", NULL);
	mhi_device_drv = mhi_dev_drv;

	r = platform_driver_register(&mhi_plat_driver);
	if (r) {
		pr_err("%s: Failed to probe platform ret %d\n", __func__, r);
		goto platform_error;
	}
	r = pci_register_driver(&mhi_pcie_driver);
	if (r) {
		pr_err("%s: Failed to register pcie drv ret %d\n", __func__, r);
		goto error;
	}

	return 0;
error:
	platform_driver_unregister(&mhi_plat_driver);
platform_error:
	class_destroy(mhi_device_drv->mhi_bhi_class);

class_error:
	kfree(mhi_dev_drv);
	mhi_device_drv = NULL;
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
subsys_initcall(mhi_init);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("MHI_CORE");
MODULE_DESCRIPTION("MHI Host Driver");
