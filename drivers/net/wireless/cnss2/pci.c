/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <linux/firmware.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>

#include "main.h"
#include "debug.h"
#include "pci.h"

#define PCI_LINK_UP			1
#define PCI_LINK_DOWN			0

#define SAVE_PCI_CONFIG_SPACE		1
#define RESTORE_PCI_CONFIG_SPACE	0

#define PM_OPTIONS_DEFAULT		0
#define PM_OPTIONS_LINK_DOWN \
	(MSM_PCIE_CONFIG_NO_CFG_RESTORE | MSM_PCIE_CONFIG_LINKDOWN)

#define PCI_BAR_NUM			0

#ifdef CONFIG_ARM_LPAE
#define PCI_DMA_MASK			64
#else
#define PCI_DMA_MASK			32
#endif

#define MHI_NODE_NAME			"qcom,mhi"

#define MAX_M3_FILE_NAME_LENGTH		13
#define DEFAULT_M3_FILE_NAME		"m3.bin"

static DEFINE_SPINLOCK(pci_link_down_lock);

static unsigned int pci_link_down_panic;
module_param(pci_link_down_panic, uint, 0600);
MODULE_PARM_DESC(pci_link_down_panic,
		 "Trigger kernel panic when PCI link down is detected");

static bool fbc_bypass;
#ifdef CONFIG_CNSS2_DEBUG
module_param(fbc_bypass, bool, 0600);
MODULE_PARM_DESC(fbc_bypass,
		 "Bypass firmware download when loading WLAN driver");
#endif

static int cnss_set_pci_config_space(struct cnss_pci_data *pci_priv, bool save)
{
	int ret = 0;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	bool link_down_or_recovery;

	if (!plat_priv)
		return -ENODEV;

	link_down_or_recovery = pci_priv->pci_link_down_ind ||
		(test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state));

	if (save) {
		if (link_down_or_recovery) {
			pci_priv->saved_state = NULL;
		} else {
			pci_save_state(pci_dev);
			pci_priv->saved_state = pci_store_saved_state(pci_dev);
		}
	} else {
		if (link_down_or_recovery) {
			ret = msm_pcie_recover_config(pci_dev);
			if (ret) {
				cnss_pr_err("Failed to recover PCI config space, err = %d\n",
					    ret);
				return ret;
			}
		} else if (pci_priv->saved_state) {
			pci_load_and_free_saved_state(pci_dev,
						      &pci_priv->saved_state);
			pci_restore_state(pci_dev);
		}
	}

	return 0;
}

static int cnss_set_pci_link(struct cnss_pci_data *pci_priv, bool link_up)
{
	int ret = 0;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	bool link_down_or_recovery;

	if (!plat_priv)
		return -ENODEV;

	link_down_or_recovery = pci_priv->pci_link_down_ind ||
		(test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state));

	ret = msm_pcie_pm_control(link_up ? MSM_PCIE_RESUME :
				  MSM_PCIE_SUSPEND,
				  pci_dev->bus->number,
				  pci_dev, NULL,
				  link_down_or_recovery ?
				  PM_OPTIONS_LINK_DOWN :
				  PM_OPTIONS_DEFAULT);
	if (ret) {
		cnss_pr_err("Failed to %s PCI link with %s option, err = %d\n",
			    link_up ? "resume" : "suspend",
			    link_down_or_recovery ? "link down" : "default",
			    ret);
		return ret;
	}

	return 0;
}

int cnss_suspend_pci_link(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv)
		return -ENODEV;

	cnss_pr_dbg("Suspending PCI link\n");
	if (!pci_priv->pci_link_state) {
		cnss_pr_info("PCI link is already suspended!\n");
		goto out;
	}

	ret = cnss_set_pci_config_space(pci_priv, SAVE_PCI_CONFIG_SPACE);
	if (ret)
		goto out;

	pci_disable_device(pci_priv->pci_dev);

	ret = pci_set_power_state(pci_priv->pci_dev, PCI_D3hot);
	if (ret)
		cnss_pr_err("Failed to set D3Hot, err =  %d\n", ret);

	ret = cnss_set_pci_link(pci_priv, PCI_LINK_DOWN);
	if (ret)
		goto out;

	pci_priv->pci_link_state = PCI_LINK_DOWN;

	return 0;
out:
	return ret;
}

int cnss_resume_pci_link(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv)
		return -ENODEV;

	cnss_pr_dbg("Resuming PCI link\n");
	if (pci_priv->pci_link_state) {
		cnss_pr_info("PCI link is already resumed!\n");
		goto out;
	}

	ret = cnss_set_pci_link(pci_priv, PCI_LINK_UP);
	if (ret)
		goto out;

	pci_priv->pci_link_state = PCI_LINK_UP;

	ret = pci_enable_device(pci_priv->pci_dev);
	if (ret) {
		cnss_pr_err("Failed to enable PCI device, err = %d\n", ret);
		goto out;
	}

	ret = cnss_set_pci_config_space(pci_priv, RESTORE_PCI_CONFIG_SPACE);
	if (ret)
		goto out;

	pci_set_master(pci_priv->pci_dev);

	if (pci_priv->pci_link_down_ind)
		pci_priv->pci_link_down_ind = false;

	return 0;
out:
	return ret;
}

int cnss_pci_link_down(struct device *dev)
{
	unsigned long flags;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL!\n");
		return -EINVAL;
	}

	if (pci_link_down_panic)
		panic("cnss: PCI link is down!\n");

	spin_lock_irqsave(&pci_link_down_lock, flags);
	if (pci_priv->pci_link_down_ind) {
		cnss_pr_dbg("PCI link down recovery is in progress, ignore!\n");
		spin_unlock_irqrestore(&pci_link_down_lock, flags);
		return -EINVAL;
	}
	pci_priv->pci_link_down_ind = true;
	spin_unlock_irqrestore(&pci_link_down_lock, flags);

	cnss_pr_err("PCI link down is detected by host driver, schedule recovery!\n");

	cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_NOTIFY_LINK_ERROR);
	cnss_schedule_recovery(dev, CNSS_REASON_LINK_DOWN);

	return 0;
}
EXPORT_SYMBOL(cnss_pci_link_down);

static int cnss_pci_init_smmu(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct device *dev;
	struct dma_iommu_mapping *mapping;
	int disable_htw = 1;
	int atomic_ctx = 1;

	dev = &pci_priv->pci_dev->dev;

	mapping = arm_iommu_create_mapping(&platform_bus_type,
					   pci_priv->smmu_iova_start,
					   pci_priv->smmu_iova_len);
	if (IS_ERR(mapping)) {
		ret = PTR_ERR(mapping);
		cnss_pr_err("Failed to create SMMU mapping, err = %d\n", ret);
		goto out;
	}

	ret = iommu_domain_set_attr(mapping->domain,
				    DOMAIN_ATTR_COHERENT_HTW_DISABLE,
				    &disable_htw);
	if (ret) {
		cnss_pr_err("Failed to set SMMU disable_htw attribute, err = %d\n",
			    ret);
		goto release_mapping;
	}

	ret = iommu_domain_set_attr(mapping->domain,
				    DOMAIN_ATTR_ATOMIC,
				    &atomic_ctx);
	if (ret) {
		pr_err("Failed to set SMMU atomic_ctx attribute, err = %d\n",
		       ret);
		goto release_mapping;
	}

	ret = arm_iommu_attach_device(dev, mapping);
	if (ret) {
		pr_err("Failed to attach SMMU device, err = %d\n", ret);
		goto release_mapping;
	}

	pci_priv->smmu_mapping = mapping;

	return ret;
release_mapping:
	arm_iommu_release_mapping(mapping);
out:
	return ret;
}

static void cnss_pci_deinit_smmu(struct cnss_pci_data *pci_priv)
{
	arm_iommu_detach_device(&pci_priv->pci_dev->dev);
	arm_iommu_release_mapping(pci_priv->smmu_mapping);

	pci_priv->smmu_mapping = NULL;
}

static void cnss_pci_event_cb(struct msm_pcie_notify *notify)
{
	unsigned long flags;
	struct pci_dev *pci_dev;
	struct cnss_pci_data *pci_priv;

	if (!notify)
		return;

	pci_dev = notify->user;
	if (!pci_dev)
		return;

	pci_priv = cnss_get_pci_priv(pci_dev);
	if (!pci_priv)
		return;

	switch (notify->event) {
	case MSM_PCIE_EVENT_LINKDOWN:
		if (pci_link_down_panic)
			panic("cnss: PCI link is down!\n");

		spin_lock_irqsave(&pci_link_down_lock, flags);
		if (pci_priv->pci_link_down_ind) {
			cnss_pr_dbg("PCI link down recovery is in progress, ignore!\n");
			spin_unlock_irqrestore(&pci_link_down_lock, flags);
			return;
		}
		pci_priv->pci_link_down_ind = true;
		spin_unlock_irqrestore(&pci_link_down_lock, flags);

		cnss_pr_err("PCI link down, schedule recovery!\n");
		cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_NOTIFY_LINK_ERROR);
		if (pci_dev->device == QCA6174_DEVICE_ID)
			disable_irq(pci_dev->irq);
		cnss_schedule_recovery(&pci_dev->dev, CNSS_REASON_LINK_DOWN);
		break;
	case MSM_PCIE_EVENT_WAKEUP:
		if (cnss_pci_get_monitor_wake_intr(pci_priv) &&
		    cnss_pci_get_auto_suspended(pci_priv)) {
			cnss_pci_set_monitor_wake_intr(pci_priv, false);
			pm_request_resume(&pci_dev->dev);
		}
		break;
	default:
		cnss_pr_err("Received invalid PCI event: %d\n", notify->event);
	}
}

static int cnss_reg_pci_event(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct msm_pcie_register_event *pci_event;

	pci_event = &pci_priv->msm_pci_event;
	pci_event->events = MSM_PCIE_EVENT_LINKDOWN |
		MSM_PCIE_EVENT_WAKEUP;
	pci_event->user = pci_priv->pci_dev;
	pci_event->mode = MSM_PCIE_TRIGGER_CALLBACK;
	pci_event->callback = cnss_pci_event_cb;
	pci_event->options = MSM_PCIE_CONFIG_NO_RECOVERY;

	ret = msm_pcie_register_event(pci_event);
	if (ret)
		cnss_pr_err("Failed to register MSM PCI event, err = %d\n",
			    ret);

	return ret;
}

static void cnss_dereg_pci_event(struct cnss_pci_data *pci_priv)
{
	msm_pcie_deregister_event(&pci_priv->msm_pci_event);
}

static int cnss_pci_suspend(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct cnss_wlan_driver *driver_ops;

	pm_message_t state = { .event = PM_EVENT_SUSPEND };

	if (!pci_priv)
		goto out;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		goto out;

	driver_ops = plat_priv->driver_ops;
	if (driver_ops && driver_ops->suspend) {
		ret = driver_ops->suspend(pci_dev, state);
		if (ret) {
			cnss_pr_err("Failed to suspend host driver, err = %d\n",
				    ret);
			ret = -EAGAIN;
			goto out;
		}
	}

	if (pci_priv->pci_link_state) {
		ret = cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_SUSPEND);
		if (ret) {
			if (driver_ops && driver_ops->resume)
				driver_ops->resume(pci_dev);
			ret = -EAGAIN;
			goto out;
		}

		cnss_set_pci_config_space(pci_priv,
					  SAVE_PCI_CONFIG_SPACE);
		pci_disable_device(pci_dev);

		ret = pci_set_power_state(pci_dev, PCI_D3hot);
		if (ret)
			cnss_pr_err("Failed to set D3Hot, err = %d\n",
				    ret);
	}

	cnss_pci_set_monitor_wake_intr(pci_priv, false);

	return 0;

out:
	return ret;
}

static int cnss_pci_resume(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		goto out;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		goto out;

	if (pci_priv->pci_link_down_ind)
		goto out;

	ret = pci_enable_device(pci_dev);
	if (ret)
		cnss_pr_err("Failed to enable PCI device, err = %d\n", ret);

	if (pci_priv->saved_state)
		cnss_set_pci_config_space(pci_priv,
					  RESTORE_PCI_CONFIG_SPACE);

	pci_set_master(pci_dev);
	cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_RESUME);

	driver_ops = plat_priv->driver_ops;
	if (driver_ops && driver_ops->resume) {
		ret = driver_ops->resume(pci_dev);
		if (ret)
			cnss_pr_err("Failed to resume host driver, err = %d\n",
				    ret);
	}

	return 0;

out:
	return ret;
}

static int cnss_pci_suspend_noirq(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		goto out;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		goto out;

	driver_ops = plat_priv->driver_ops;
	if (driver_ops && driver_ops->suspend_noirq)
		ret = driver_ops->suspend_noirq(pci_dev);

out:
	return ret;
}

static int cnss_pci_resume_noirq(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		goto out;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		goto out;

	driver_ops = plat_priv->driver_ops;
	if (driver_ops && driver_ops->resume_noirq &&
	    !pci_priv->pci_link_down_ind)
		ret = driver_ops->resume_noirq(pci_dev);

out:
	return ret;
}

static int cnss_pci_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		return -EAGAIN;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -EAGAIN;

	if (pci_priv->pci_link_down_ind) {
		cnss_pr_dbg("PCI link down recovery is in progress!\n");
		return -EAGAIN;
	}

	cnss_pr_dbg("Runtime suspend start\n");

	driver_ops = plat_priv->driver_ops;
	if (driver_ops && driver_ops->runtime_ops &&
	    driver_ops->runtime_ops->runtime_suspend)
		ret = driver_ops->runtime_ops->runtime_suspend(pci_dev);

	cnss_pr_info("Runtime suspend status: %d\n", ret);

	return ret;
}

static int cnss_pci_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		return -EAGAIN;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -EAGAIN;

	if (pci_priv->pci_link_down_ind) {
		cnss_pr_dbg("PCI link down recovery is in progress!\n");
		return -EAGAIN;
	}

	cnss_pr_dbg("Runtime resume start\n");

	driver_ops = plat_priv->driver_ops;
	if (driver_ops && driver_ops->runtime_ops &&
	    driver_ops->runtime_ops->runtime_resume)
		ret = driver_ops->runtime_ops->runtime_resume(pci_dev);

	cnss_pr_info("Runtime resume status: %d\n", ret);

	return ret;
}

static int cnss_pci_runtime_idle(struct device *dev)
{
	cnss_pr_dbg("Runtime idle\n");

	pm_request_autosuspend(dev);

	return -EBUSY;
}

int cnss_wlan_pm_control(struct device *dev, bool vote)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_pci_data *pci_priv;
	struct pci_dev *pci_dev;

	if (!plat_priv)
		return -ENODEV;

	pci_priv = plat_priv->bus_priv;
	if (!pci_priv)
		return -ENODEV;

	pci_dev = pci_priv->pci_dev;

	return msm_pcie_pm_control(vote ? MSM_PCIE_DISABLE_PC :
				   MSM_PCIE_ENABLE_PC,
				   pci_dev->bus->number, pci_dev,
				   NULL, PM_OPTIONS_DEFAULT);
}
EXPORT_SYMBOL(cnss_wlan_pm_control);

int cnss_auto_suspend(struct device *dev)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct pci_dev *pci_dev;
	struct cnss_pci_data *pci_priv;
	struct cnss_bus_bw_info *bus_bw_info;

	if (!plat_priv)
		return -ENODEV;

	pci_priv = plat_priv->bus_priv;
	if (!pci_priv)
		return -ENODEV;

	pci_dev = pci_priv->pci_dev;

	if (pci_priv->pci_link_state) {
		if (cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_SUSPEND)) {
			ret = -EAGAIN;
			goto out;
		}

		cnss_set_pci_config_space(pci_priv, SAVE_PCI_CONFIG_SPACE);
		pci_disable_device(pci_dev);

		ret = pci_set_power_state(pci_dev, PCI_D3hot);
		if (ret)
			cnss_pr_err("Failed to set D3Hot, err =  %d\n", ret);

		cnss_pr_dbg("Suspending PCI link\n");
		if (cnss_set_pci_link(pci_priv, PCI_LINK_DOWN)) {
			cnss_pr_err("Failed to suspend PCI link!\n");
			ret = -EAGAIN;
			goto resume_mhi;
		}

		pci_priv->pci_link_state = PCI_LINK_DOWN;
	}

	cnss_pci_set_auto_suspended(pci_priv, 1);
	cnss_pci_set_monitor_wake_intr(pci_priv, true);

	bus_bw_info = &plat_priv->bus_bw_info;
	msm_bus_scale_client_update_request(bus_bw_info->bus_client,
					    CNSS_BUS_WIDTH_NONE);

	return 0;

resume_mhi:
	if (pci_enable_device(pci_dev))
		cnss_pr_err("Failed to enable PCI device!\n");
	cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_RESUME);
out:
	return ret;
}
EXPORT_SYMBOL(cnss_auto_suspend);

int cnss_auto_resume(struct device *dev)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct pci_dev *pci_dev;
	struct cnss_pci_data *pci_priv;
	struct cnss_bus_bw_info *bus_bw_info;

	if (!plat_priv)
		return -ENODEV;

	pci_priv = plat_priv->bus_priv;
	if (!pci_priv)
		return -ENODEV;

	pci_dev = pci_priv->pci_dev;
	if (!pci_priv->pci_link_state) {
		cnss_pr_dbg("Resuming PCI link\n");
		if (cnss_set_pci_link(pci_priv, PCI_LINK_UP)) {
			cnss_pr_err("Failed to resume PCI link!\n");
			ret = -EAGAIN;
			goto out;
		}
		pci_priv->pci_link_state = PCI_LINK_UP;

		ret = pci_enable_device(pci_dev);
		if (ret)
			cnss_pr_err("Failed to enable PCI device, err = %d\n",
				    ret);

		cnss_set_pci_config_space(pci_priv, RESTORE_PCI_CONFIG_SPACE);
		pci_set_master(pci_dev);
		cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_RESUME);
	}

	cnss_pci_set_auto_suspended(pci_priv, 0);

	bus_bw_info = &plat_priv->bus_bw_info;
	msm_bus_scale_client_update_request(bus_bw_info->bus_client,
					    bus_bw_info->current_bw_vote);
out:
	return ret;
}
EXPORT_SYMBOL(cnss_auto_resume);

int cnss_pm_request_resume(struct cnss_pci_data *pci_priv)
{
	struct pci_dev *pci_dev;

	if (!pci_priv)
		return -ENODEV;

	pci_dev = pci_priv->pci_dev;
	if (!pci_dev)
		return -ENODEV;

	return pm_request_resume(&pci_dev->dev);
}

int cnss_pci_alloc_fw_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *fw_mem = &plat_priv->fw_mem;

	if (!fw_mem->va && fw_mem->size) {
		fw_mem->va = dma_alloc_coherent(&pci_priv->pci_dev->dev,
						fw_mem->size, &fw_mem->pa,
						GFP_KERNEL);
		if (!fw_mem->va) {
			cnss_pr_err("Failed to allocate memory for FW, size: 0x%zx\n",
				    fw_mem->size);
			fw_mem->size = 0;

			return -ENOMEM;
		}
	}

	return 0;
}

static void cnss_pci_free_fw_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *fw_mem = &plat_priv->fw_mem;

	if (fw_mem->va && fw_mem->size) {
		cnss_pr_dbg("Freeing memory for FW, va: 0x%pK, pa: %pa, size: 0x%zx\n",
			    fw_mem->va, &fw_mem->pa, fw_mem->size);
		dma_free_coherent(&pci_priv->pci_dev->dev, fw_mem->size,
				  fw_mem->va, fw_mem->pa);
		fw_mem->va = NULL;
		fw_mem->pa = 0;
		fw_mem->size = 0;
	}
}

int cnss_pci_load_m3(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *m3_mem = &plat_priv->m3_mem;
	char filename[MAX_M3_FILE_NAME_LENGTH];
	const struct firmware *fw_entry;
	int ret = 0;

	if (!m3_mem->va && !m3_mem->size) {
		snprintf(filename, sizeof(filename), DEFAULT_M3_FILE_NAME);

		ret = request_firmware(&fw_entry, filename,
				       &pci_priv->pci_dev->dev);
		if (ret) {
			cnss_pr_err("Failed to load M3 image: %s\n", filename);
			return ret;
		}

		m3_mem->va = dma_alloc_coherent(&pci_priv->pci_dev->dev,
						fw_entry->size, &m3_mem->pa,
						GFP_KERNEL);
		if (!m3_mem->va) {
			cnss_pr_err("Failed to allocate memory for M3, size: 0x%zx\n",
				    fw_entry->size);
			release_firmware(fw_entry);
			return -ENOMEM;
		}

		memcpy(m3_mem->va, fw_entry->data, fw_entry->size);
		m3_mem->size = fw_entry->size;
		release_firmware(fw_entry);
	}

	return 0;
}

static void cnss_pci_free_m3_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *m3_mem = &plat_priv->m3_mem;

	if (m3_mem->va && m3_mem->size) {
		cnss_pr_dbg("Freeing memory for M3, va: 0x%pK, pa: %pa, size: 0x%zx\n",
			    m3_mem->va, &m3_mem->pa, m3_mem->size);
		dma_free_coherent(&pci_priv->pci_dev->dev, m3_mem->size,
				  m3_mem->va, m3_mem->pa);
	}

	m3_mem->va = NULL;
	m3_mem->pa = 0;
	m3_mem->size = 0;
}

int cnss_pci_get_bar_info(struct cnss_pci_data *pci_priv, void __iomem **va,
			  phys_addr_t *pa)
{
	if (!pci_priv)
		return -ENODEV;

	*va = pci_priv->bar;
	*pa = pci_resource_start(pci_priv->pci_dev, PCI_BAR_NUM);

	return 0;
}

static struct cnss_msi_config msi_config = {
	.total_vectors = 32,
	.total_users = 4,
	.users = (struct cnss_msi_user[]) {
		{ .name = "MHI", .num_vectors = 2, .base_vector = 0 },
		{ .name = "CE", .num_vectors = 11, .base_vector = 2 },
		{ .name = "WAKE", .num_vectors = 1, .base_vector = 13 },
		{ .name = "DP", .num_vectors = 18, .base_vector = 14 },
	},
};

static int cnss_pci_get_msi_assignment(struct cnss_pci_data *pci_priv)
{
	pci_priv->msi_config = &msi_config;

	return 0;
}

static int cnss_pci_enable_msi(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	int num_vectors;
	struct cnss_msi_config *msi_config;
	struct msi_desc *msi_desc;

	ret = cnss_pci_get_msi_assignment(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to get MSI assignment, err = %d\n", ret);
		goto out;
	}

	msi_config = pci_priv->msi_config;
	if (!msi_config) {
		cnss_pr_err("msi_config is NULL!\n");
		ret = -EINVAL;
		goto out;
	}

	num_vectors = pci_enable_msi_range(pci_dev,
					   msi_config->total_vectors,
					   msi_config->total_vectors);
	if (num_vectors != msi_config->total_vectors) {
		cnss_pr_err("Failed to get enough MSI vectors (%d), available vectors = %d",
			    msi_config->total_vectors, num_vectors);
		ret = -EINVAL;
		goto reset_msi_config;
	}

	msi_desc = irq_get_msi_desc(pci_dev->irq);
	if (!msi_desc) {
		cnss_pr_err("msi_desc is NULL!\n");
		ret = -EINVAL;
		goto disable_msi;
	}

	pci_priv->msi_ep_base_data = msi_desc->msg.data;
	if (!pci_priv->msi_ep_base_data) {
		cnss_pr_err("Got 0 MSI base data!\n");
		CNSS_ASSERT(0);
	}

	cnss_pr_dbg("MSI base data is %d\n", pci_priv->msi_ep_base_data);

	return 0;

disable_msi:
	pci_disable_msi(pci_priv->pci_dev);
reset_msi_config:
	pci_priv->msi_config = NULL;
out:
	return ret;
}

static void cnss_pci_disable_msi(struct cnss_pci_data *pci_priv)
{
	pci_disable_msi(pci_priv->pci_dev);
}

int cnss_get_user_msi_assignment(struct device *dev, char *user_name,
				 int *num_vectors, u32 *user_base_data,
				 u32 *base_vector)
{
	struct cnss_pci_data *pci_priv = dev_get_drvdata(dev);
	struct cnss_msi_config *msi_config;
	int idx;

	if (!pci_priv)
		return -ENODEV;

	msi_config = pci_priv->msi_config;
	if (!msi_config) {
		cnss_pr_err("MSI is not supported.\n");
		return -EINVAL;
	}

	for (idx = 0; idx < msi_config->total_users; idx++) {
		if (strcmp(user_name, msi_config->users[idx].name) == 0) {
			*num_vectors = msi_config->users[idx].num_vectors;
			*user_base_data = msi_config->users[idx].base_vector
				+ pci_priv->msi_ep_base_data;
			*base_vector = msi_config->users[idx].base_vector;

			cnss_pr_dbg("Assign MSI to user: %s, num_vectors: %d, user_base_data: %u, base_vector: %u\n",
				    user_name, *num_vectors, *user_base_data,
				    *base_vector);

			return 0;
		}
	}

	cnss_pr_err("Failed to find MSI assignment for %s!\n", user_name);

	return -EINVAL;
}
EXPORT_SYMBOL(cnss_get_user_msi_assignment);

int cnss_get_msi_irq(struct device *dev, unsigned int vector)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);

	return pci_dev->irq + vector;
}
EXPORT_SYMBOL(cnss_get_msi_irq);

void cnss_get_msi_address(struct device *dev, u32 *msi_addr_low,
			  u32 *msi_addr_high)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);

	pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_ADDRESS_LO,
			      msi_addr_low);

	pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_ADDRESS_HI,
			      msi_addr_high);
}
EXPORT_SYMBOL(cnss_get_msi_address);

static int cnss_pci_enable_bus(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	u16 device_id;

	pci_read_config_word(pci_dev, PCI_DEVICE_ID, &device_id);
	if (device_id != pci_priv->pci_device_id->device)  {
		cnss_pr_err("PCI device ID mismatch, config ID: 0x%x, probe ID: 0x%x\n",
			    device_id, pci_priv->pci_device_id->device);
		ret = -EIO;
		goto out;
	}

	ret = pci_assign_resource(pci_dev, PCI_BAR_NUM);
	if (ret) {
		pr_err("Failed to assign PCI resource, err = %d\n", ret);
		goto out;
	}

	ret = pci_enable_device(pci_dev);
	if (ret) {
		cnss_pr_err("Failed to enable PCI device, err = %d\n", ret);
		goto out;
	}

	ret = pci_request_region(pci_dev, PCI_BAR_NUM, "cnss");
	if (ret) {
		cnss_pr_err("Failed to request PCI region, err = %d\n", ret);
		goto disable_device;
	}

	ret = pci_set_dma_mask(pci_dev, DMA_BIT_MASK(PCI_DMA_MASK));
	if (ret) {
		cnss_pr_err("Failed to set PCI DMA mask (%d), err = %d\n",
			    ret, PCI_DMA_MASK);
		goto release_region;
	}

	ret = pci_set_consistent_dma_mask(pci_dev, DMA_BIT_MASK(PCI_DMA_MASK));
	if (ret) {
		cnss_pr_err("Failed to set PCI consistent DMA mask (%d), err = %d\n",
			    ret, PCI_DMA_MASK);
		goto release_region;
	}

	pci_set_master(pci_dev);

	pci_priv->bar = pci_iomap(pci_dev, PCI_BAR_NUM, 0);
	if (!pci_priv->bar) {
		cnss_pr_err("Failed to do PCI IO map!\n");
		ret = -EIO;
		goto clear_master;
	}
	return 0;

clear_master:
	pci_clear_master(pci_dev);
release_region:
	pci_release_region(pci_dev, PCI_BAR_NUM);
disable_device:
	pci_disable_device(pci_dev);
out:
	return ret;
}

static void cnss_pci_disable_bus(struct cnss_pci_data *pci_priv)
{
	struct pci_dev *pci_dev = pci_priv->pci_dev;

	if (pci_priv->bar) {
		pci_iounmap(pci_dev, pci_priv->bar);
		pci_priv->bar = NULL;
	}

	pci_clear_master(pci_dev);
	pci_release_region(pci_dev, PCI_BAR_NUM);
	pci_disable_device(pci_dev);
}

static int cnss_mhi_pm_runtime_get(struct pci_dev *pci_dev)
{
	return pm_runtime_get(&pci_dev->dev);
}

static void cnss_mhi_pm_runtime_put_noidle(struct pci_dev *pci_dev)
{
	pm_runtime_put_noidle(&pci_dev->dev);
}

static char *cnss_mhi_state_to_str(enum cnss_mhi_state mhi_state)
{
	switch (mhi_state) {
	case CNSS_MHI_INIT:
		return "INIT";
	case CNSS_MHI_DEINIT:
		return "DEINIT";
	case CNSS_MHI_POWER_ON:
		return "POWER_ON";
	case CNSS_MHI_POWER_OFF:
		return "POWER_OFF";
	case CNSS_MHI_SUSPEND:
		return "SUSPEND";
	case CNSS_MHI_RESUME:
		return "RESUME";
	case CNSS_MHI_TRIGGER_RDDM:
		return "TRIGGER_RDDM";
	case CNSS_MHI_RDDM:
		return "RDDM";
	case CNSS_MHI_RDDM_KERNEL_PANIC:
		return "RDDM_KERNEL_PANIC";
	case CNSS_MHI_NOTIFY_LINK_ERROR:
		return "NOTIFY_LINK_ERROR";
	default:
		return "UNKNOWN";
	}
};

static void *cnss_pci_collect_dump_seg(struct cnss_pci_data *pci_priv,
				       enum mhi_rddm_segment type,
				       void *start_addr)
{
	int count;
	struct scatterlist *sg_list, *s;
	unsigned int i;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_dump_data *dump_data =
		&plat_priv->ramdump_info_v2.dump_data;
	struct cnss_dump_seg *dump_seg = start_addr;

	count = mhi_xfer_rddm(&pci_priv->mhi_dev, type, &sg_list);
	if (count <= 0 || !sg_list) {
		cnss_pr_err("Invalid dump_seg for type %u, count %u, sg_list %pK\n",
			    type, count, sg_list);
		return start_addr;
	}

	cnss_pr_dbg("Collect dump seg: type %u, nentries %d\n", type, count);

	for_each_sg(sg_list, s, count, i) {
		dump_seg->address = sg_dma_address(s);
		dump_seg->v_address = sg_virt(s);
		dump_seg->size = s->length;
		dump_seg->type = type;
		cnss_pr_dbg("seg-%d: address 0x%lx, v_address %pK, size 0x%lx\n",
			    i, dump_seg->address,
			    dump_seg->v_address, dump_seg->size);
		dump_seg++;
	}

	dump_data->nentries += count;

	return dump_seg;
}

void cnss_pci_collect_dump_info(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_dump_data *dump_data =
		&plat_priv->ramdump_info_v2.dump_data;
	void *start_addr, *end_addr;

	dump_data->nentries = 0;

	start_addr = plat_priv->ramdump_info_v2.dump_data_vaddr;
	end_addr = cnss_pci_collect_dump_seg(pci_priv,
					     MHI_RDDM_FW_SEGMENT, start_addr);

	start_addr = end_addr;
	end_addr = cnss_pci_collect_dump_seg(pci_priv,
					     MHI_RDDM_RD_SEGMENT, start_addr);

	if (dump_data->nentries > 0)
		plat_priv->ramdump_info_v2.dump_data_valid = true;
}

void cnss_pci_clear_dump_info(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	plat_priv->ramdump_info_v2.dump_data.nentries = 0;
	plat_priv->ramdump_info_v2.dump_data_valid = false;
}

static void cnss_mhi_notify_status(enum MHI_CB_REASON reason, void *priv)
{
	struct cnss_pci_data *pci_priv = priv;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	enum cnss_recovery_reason cnss_reason = CNSS_REASON_RDDM;

	if (!pci_priv)
		return;

	cnss_pr_dbg("MHI status cb is called with reason %d\n", reason);

	if (plat_priv->driver_ops && plat_priv->driver_ops->update_status)
		plat_priv->driver_ops->update_status(pci_priv->pci_dev,
						     CNSS_FW_DOWN);

	set_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);
	del_timer(&plat_priv->fw_boot_timer);

	if (reason == MHI_CB_SYS_ERROR)
		cnss_reason = CNSS_REASON_TIMEOUT;

	cnss_schedule_recovery(&pci_priv->pci_dev->dev,
			       cnss_reason);
}

static int cnss_pci_register_mhi(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	struct mhi_device *mhi_dev = &pci_priv->mhi_dev;

	mhi_dev->dev = &pci_priv->plat_priv->plat_dev->dev;
	mhi_dev->pci_dev = pci_dev;

	mhi_dev->resources[0].start = (resource_size_t)pci_priv->bar;
	mhi_dev->resources[0].end = (resource_size_t)pci_priv->bar +
		pci_resource_len(pci_dev, PCI_BAR_NUM);
	mhi_dev->resources[0].flags =
		pci_resource_flags(pci_dev, PCI_BAR_NUM);
	mhi_dev->resources[0].name = "BAR";
	cnss_pr_dbg("BAR start is %pa, BAR end is %pa\n",
		    &mhi_dev->resources[0].start, &mhi_dev->resources[0].end);

	if (!mhi_dev->resources[1].start) {
		mhi_dev->resources[1].start = pci_dev->irq;
		mhi_dev->resources[1].end = pci_dev->irq + 1;
		mhi_dev->resources[1].flags = IORESOURCE_IRQ;
		mhi_dev->resources[1].name = "IRQ";
	}
	cnss_pr_dbg("IRQ start is %pa, IRQ end is %pa\n",
		    &mhi_dev->resources[1].start, &mhi_dev->resources[1].end);

	mhi_dev->pm_runtime_get = cnss_mhi_pm_runtime_get;
	mhi_dev->pm_runtime_put_noidle = cnss_mhi_pm_runtime_put_noidle;

	mhi_dev->support_rddm = true;
	mhi_dev->rddm_size = pci_priv->plat_priv->ramdump_info_v2.ramdump_size;
	mhi_dev->status_cb = cnss_mhi_notify_status;

	ret = mhi_register_device(mhi_dev, MHI_NODE_NAME, pci_priv);
	if (ret) {
		cnss_pr_err("Failed to register as MHI device, err = %d\n",
			    ret);
		return ret;
	}

	return 0;
}

static void cnss_pci_unregister_mhi(struct cnss_pci_data *pci_priv)
{
}

static enum mhi_dev_ctrl cnss_to_mhi_dev_state(enum cnss_mhi_state state)
{
	switch (state) {
	case CNSS_MHI_INIT:
		return MHI_DEV_CTRL_INIT;
	case CNSS_MHI_DEINIT:
		return MHI_DEV_CTRL_DE_INIT;
	case CNSS_MHI_POWER_ON:
		return MHI_DEV_CTRL_POWER_ON;
	case CNSS_MHI_POWER_OFF:
		return MHI_DEV_CTRL_POWER_OFF;
	case CNSS_MHI_SUSPEND:
		return MHI_DEV_CTRL_SUSPEND;
	case CNSS_MHI_RESUME:
		return MHI_DEV_CTRL_RESUME;
	case CNSS_MHI_TRIGGER_RDDM:
		return MHI_DEV_CTRL_TRIGGER_RDDM;
	case CNSS_MHI_RDDM:
		return MHI_DEV_CTRL_RDDM;
	case CNSS_MHI_RDDM_KERNEL_PANIC:
		return MHI_DEV_CTRL_RDDM_KERNEL_PANIC;
	case CNSS_MHI_NOTIFY_LINK_ERROR:
		return MHI_DEV_CTRL_NOTIFY_LINK_ERROR;
	default:
		cnss_pr_err("Unknown CNSS MHI state (%d)\n", state);
		return -EINVAL;
	}
}

static int cnss_pci_check_mhi_state_bit(struct cnss_pci_data *pci_priv,
					enum cnss_mhi_state mhi_state)
{
	switch (mhi_state) {
	case CNSS_MHI_INIT:
		if (!test_bit(CNSS_MHI_INIT, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_DEINIT:
	case CNSS_MHI_POWER_ON:
		if (test_bit(CNSS_MHI_INIT, &pci_priv->mhi_state) &&
		    !test_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_POWER_OFF:
	case CNSS_MHI_SUSPEND:
		if (test_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state) &&
		    !test_bit(CNSS_MHI_SUSPEND, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_RESUME:
		if (test_bit(CNSS_MHI_SUSPEND, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_TRIGGER_RDDM:
	case CNSS_MHI_RDDM:
	case CNSS_MHI_RDDM_KERNEL_PANIC:
	case CNSS_MHI_NOTIFY_LINK_ERROR:
		return 0;
	default:
		cnss_pr_err("Unhandled MHI state: %s(%d)\n",
			    cnss_mhi_state_to_str(mhi_state), mhi_state);
	}

	cnss_pr_err("Cannot set MHI state %s(%d) in current MHI state (0x%lx)\n",
		    cnss_mhi_state_to_str(mhi_state), mhi_state,
		    pci_priv->mhi_state);

	return -EINVAL;
}

static void cnss_pci_set_mhi_state_bit(struct cnss_pci_data *pci_priv,
				       enum cnss_mhi_state mhi_state)
{
	switch (mhi_state) {
	case CNSS_MHI_INIT:
		set_bit(CNSS_MHI_INIT, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_DEINIT:
		clear_bit(CNSS_MHI_INIT, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_POWER_ON:
		set_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_POWER_OFF:
		clear_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_SUSPEND:
		set_bit(CNSS_MHI_SUSPEND, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_RESUME:
		clear_bit(CNSS_MHI_SUSPEND, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_TRIGGER_RDDM:
	case CNSS_MHI_RDDM:
	case CNSS_MHI_RDDM_KERNEL_PANIC:
	case CNSS_MHI_NOTIFY_LINK_ERROR:
		break;
	default:
		cnss_pr_err("Unhandled MHI state (%d)\n", mhi_state);
	}
}

int cnss_pci_set_mhi_state(struct cnss_pci_data *pci_priv,
			   enum cnss_mhi_state mhi_state)
{
	int ret = 0;
	enum mhi_dev_ctrl mhi_dev_state = cnss_to_mhi_dev_state(mhi_state);

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL!\n");
		return -ENODEV;
	}

	if (pci_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (mhi_dev_state < 0) {
		cnss_pr_err("Invalid MHI DEV state (%d)\n", mhi_dev_state);
		return -EINVAL;
	}

	ret = cnss_pci_check_mhi_state_bit(pci_priv, mhi_state);
	if (ret)
		goto out;

	cnss_pr_dbg("Setting MHI state: %s(%d)\n",
		    cnss_mhi_state_to_str(mhi_state), mhi_state);
	ret = mhi_pm_control_device(&pci_priv->mhi_dev, mhi_dev_state);
	if (ret) {
		cnss_pr_err("Failed to set MHI state: %s(%d)\n",
			    cnss_mhi_state_to_str(mhi_state), mhi_state);
		goto out;
	}

	cnss_pci_set_mhi_state_bit(pci_priv, mhi_state);

out:
	return ret;
}

int cnss_pci_start_mhi(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL!\n");
		return -ENODEV;
	}

	if (fbc_bypass)
		return 0;

	ret = cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_INIT);
	if (ret)
		goto out;

	ret = cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_POWER_ON);
	if (ret)
		goto out;

	return 0;

out:
	return ret;
}

void cnss_pci_stop_mhi(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL!\n");
		return;
	}

	if (fbc_bypass)
		return;

	plat_priv = pci_priv->plat_priv;

	cnss_pci_set_mhi_state_bit(pci_priv, CNSS_MHI_RESUME);
	cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_POWER_OFF);

	if (plat_priv->ramdump_info_v2.dump_data_valid ||
	    test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state))
		return;

	cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_DEINIT);
}

static int cnss_pci_probe(struct pci_dev *pci_dev,
			  const struct pci_device_id *id)
{
	int ret = 0;
	struct cnss_pci_data *pci_priv;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct resource *res;

	cnss_pr_dbg("PCI is probing, vendor ID: 0x%x, device ID: 0x%x\n",
		    id->vendor, pci_dev->device);

	switch (pci_dev->device) {
	case QCA6290_EMULATION_DEVICE_ID:
	case QCA6290_DEVICE_ID:
		if (!mhi_is_device_ready(&plat_priv->plat_dev->dev,
					 MHI_NODE_NAME)) {
			cnss_pr_err("MHI driver is not ready, defer PCI probe!\n");
			ret = -EPROBE_DEFER;
			goto out;
		}
		break;
	default:
		break;
	}

	pci_priv = devm_kzalloc(&pci_dev->dev, sizeof(*pci_priv),
				GFP_KERNEL);
	if (!pci_priv) {
		ret = -ENOMEM;
		goto out;
	}

	pci_priv->pci_link_state = PCI_LINK_UP;
	pci_priv->plat_priv = plat_priv;
	pci_priv->pci_dev = pci_dev;
	pci_priv->pci_device_id = id;
	pci_priv->device_id = pci_dev->device;
	cnss_set_pci_priv(pci_dev, pci_priv);
	plat_priv->device_id = pci_dev->device;
	plat_priv->bus_priv = pci_priv;

	ret = cnss_register_subsys(plat_priv);
	if (ret)
		goto reset_ctx;

	ret = cnss_register_ramdump(plat_priv);
	if (ret)
		goto unregister_subsys;

	res = platform_get_resource_byname(plat_priv->plat_dev, IORESOURCE_MEM,
					   "smmu_iova_base");
	if (res) {
		pci_priv->smmu_iova_start = res->start;
		pci_priv->smmu_iova_len = resource_size(res);
		cnss_pr_dbg("smmu_iova_start: %pa, smmu_iova_len: %zu\n",
			    &pci_priv->smmu_iova_start,
			    pci_priv->smmu_iova_len);

		ret = cnss_pci_init_smmu(pci_priv);
		if (ret) {
			cnss_pr_err("Failed to init SMMU, err = %d\n", ret);
			goto unregister_ramdump;
		}
	}

	ret = cnss_reg_pci_event(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to register PCI event, err = %d\n", ret);
		goto deinit_smmu;
	}

	ret = cnss_pci_enable_bus(pci_priv);
	if (ret)
		goto dereg_pci_event;

	switch (pci_dev->device) {
	case QCA6174_DEVICE_ID:
		pci_read_config_word(pci_dev, QCA6174_REV_ID_OFFSET,
				     &pci_priv->revision_id);
		ret = cnss_suspend_pci_link(pci_priv);
		if (ret)
			cnss_pr_err("Failed to suspend PCI link, err = %d\n",
				    ret);
		cnss_power_off_device(plat_priv);
		break;
	case QCA6290_EMULATION_DEVICE_ID:
	case QCA6290_DEVICE_ID:
		ret = cnss_pci_enable_msi(pci_priv);
		if (ret)
			goto disable_bus;
		ret = cnss_pci_register_mhi(pci_priv);
		if (ret) {
			cnss_pci_disable_msi(pci_priv);
			goto disable_bus;
		}
		ret = cnss_suspend_pci_link(pci_priv);
		if (ret)
			cnss_pr_err("Failed to suspend PCI link, err = %d\n",
				    ret);
		cnss_power_off_device(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown PCI device found: 0x%x\n",
			    pci_dev->device);
		ret = -ENODEV;
		goto disable_bus;
	}

	return 0;

disable_bus:
	cnss_pci_disable_bus(pci_priv);
dereg_pci_event:
	cnss_dereg_pci_event(pci_priv);
deinit_smmu:
	if (pci_priv->smmu_mapping)
		cnss_pci_deinit_smmu(pci_priv);
unregister_ramdump:
	cnss_unregister_ramdump(plat_priv);
unregister_subsys:
	cnss_unregister_subsys(plat_priv);
reset_ctx:
	plat_priv->bus_priv = NULL;
out:
	return ret;
}

static void cnss_pci_remove(struct pci_dev *pci_dev)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv =
		cnss_bus_dev_to_plat_priv(&pci_dev->dev);

	cnss_pci_free_m3_mem(pci_priv);
	cnss_pci_free_fw_mem(pci_priv);

	switch (pci_dev->device) {
	case QCA6290_EMULATION_DEVICE_ID:
	case QCA6290_DEVICE_ID:
		cnss_pci_unregister_mhi(pci_priv);
		cnss_pci_disable_msi(pci_priv);
		break;
	default:
		break;
	}

	cnss_pci_disable_bus(pci_priv);
	cnss_dereg_pci_event(pci_priv);
	if (pci_priv->smmu_mapping)
		cnss_pci_deinit_smmu(pci_priv);
	cnss_unregister_ramdump(plat_priv);
	cnss_unregister_subsys(plat_priv);
	plat_priv->bus_priv = NULL;
}

static const struct pci_device_id cnss_pci_id_table[] = {
	{ QCA6174_VENDOR_ID, QCA6174_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ QCA6290_EMULATION_VENDOR_ID, QCA6290_EMULATION_DEVICE_ID,
	  PCI_ANY_ID, PCI_ANY_ID },
	{ QCA6290_VENDOR_ID, QCA6290_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, cnss_pci_id_table);

static const struct dev_pm_ops cnss_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cnss_pci_suspend, cnss_pci_resume)
	.suspend_noirq = cnss_pci_suspend_noirq,
	.resume_noirq = cnss_pci_resume_noirq,
	SET_RUNTIME_PM_OPS(cnss_pci_runtime_suspend, cnss_pci_runtime_resume,
			   cnss_pci_runtime_idle)
};

struct pci_driver cnss_pci_driver = {
	.name     = "cnss_pci",
	.id_table = cnss_pci_id_table,
	.probe    = cnss_pci_probe,
	.remove   = cnss_pci_remove,
	.driver = {
		.pm = &cnss_pm_ops,
	},
};

int cnss_pci_init(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct device *dev = &plat_priv->plat_dev->dev;
	u32 rc_num;

	ret = of_property_read_u32(dev->of_node, "qcom,wlan-rc-num", &rc_num);
	if (ret) {
		cnss_pr_err("Failed to find PCIe RC number, err = %d\n", ret);
		goto out;
	}

	ret = msm_pcie_enumerate(rc_num);
	if (ret) {
		cnss_pr_err("Failed to enable PCIe RC%x, err = %d\n",
			    rc_num, ret);
		goto out;
	}

	ret = pci_register_driver(&cnss_pci_driver);
	if (ret) {
		cnss_pr_err("Failed to register to PCI framework, err = %d\n",
			    ret);
		goto out;
	}

	return 0;
out:
	return ret;
}

void cnss_pci_deinit(struct cnss_plat_data *plat_priv)
{
	pci_unregister_driver(&cnss_pci_driver);
}
