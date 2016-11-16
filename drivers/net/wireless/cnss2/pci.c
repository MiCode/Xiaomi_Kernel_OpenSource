/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>

#include "debug.h"
#include "pci.h"

#define PCI_LINK_UP			1
#define PCI_LINK_DOWN			0

#define SAVE_PCI_CONFIG_SPACE		1
#define RESTORE_PCI_CONFIG_SPACE	0

#define PM_OPTIONS_DEFAULT		0
#define PM_OPTIONS_LINK_DOWN \
	(MSM_PCIE_CONFIG_NO_CFG_RESTORE | MSM_PCIE_CONFIG_LINKDOWN)

static DEFINE_SPINLOCK(pci_link_down_lock);

static unsigned int pci_link_down_panic;
module_param(pci_link_down_panic, uint, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(pci_link_down_panic,
		 "Trigger kernel panic when PCI link down is detected");

static int cnss_set_pci_config_space(struct cnss_pci_data *pci_priv, bool save)
{
	int ret = 0;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	bool link_down_or_recovery;

	if (!plat_priv)
		return -ENODEV;

	link_down_or_recovery = pci_priv->pci_link_down_ind ||
		plat_priv->recovery_in_progress;

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
		}

		pci_restore_state(pci_dev);
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
		plat_priv->recovery_in_progress;

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

	if (!pci_priv->pci_link_state) {
		cnss_pr_info("PCI link is already suspended!\n");
		goto out;
	}

	ret = cnss_set_pci_config_space(pci_priv, SAVE_PCI_CONFIG_SPACE);
	if (ret)
		goto out;

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

	if (pci_priv->pci_link_state) {
		cnss_pr_info("PCI link is already resumed!\n");
		goto out;
	}

	ret = cnss_set_pci_link(pci_priv, PCI_LINK_UP);
	if (ret)
		goto out;

	pci_priv->pci_link_state = PCI_LINK_UP;

	ret = cnss_set_pci_config_space(pci_priv, RESTORE_PCI_CONFIG_SPACE);
	if (ret)
		goto out;

	return 0;
out:
	return ret;
}

void cnss_wlan_pci_link_down(void)
{
	unsigned long flags;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct cnss_pci_data *pci_priv;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return;
	}

	pci_priv = plat_priv->bus_priv;
	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL!\n");
		return;
	}

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

	cnss_pr_err("PCI link down is detected by host driver, schedule recovery!\n");
	cnss_schedule_recovery_work();
}
EXPORT_SYMBOL(cnss_wlan_pci_link_down);

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

	if (notify == NULL || notify->user)
		return;

	pci_dev = notify->user;
	if (!pci_dev)
		return;

	pci_priv = cnss_get_pci_priv(pci_dev);
	if (pci_priv)
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
		disable_irq(pci_dev->irq);
		cnss_schedule_recovery_work();
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
		if (pci_priv->pci_link_state)
			cnss_set_pci_config_space(pci_priv,
						  SAVE_PCI_CONFIG_SPACE);
	}

	cnss_pci_set_monitor_wake_intr(pci_priv, false);

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

	driver_ops = plat_priv->driver_ops;
	if (driver_ops && driver_ops->resume && !pci_priv->pci_link_down_ind) {
		if (pci_priv->saved_state)
			cnss_set_pci_config_space(pci_priv,
						  RESTORE_PCI_CONFIG_SPACE);
		ret = driver_ops->resume(pci_dev);
	}

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

int cnss_wlan_pm_control(bool vote)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
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

int cnss_auto_suspend(void)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
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
		cnss_set_pci_config_space(pci_priv, SAVE_PCI_CONFIG_SPACE);
		pci_disable_device(pci_dev);

		ret = pci_set_power_state(pci_dev, PCI_D3hot);
		if (ret)
			cnss_pr_err("Failed to set D3Hot, err =  %d\n", ret);
		if (cnss_set_pci_link(pci_priv, PCI_LINK_DOWN)) {
			cnss_pr_err("Failed to shutdown PCI link!\n");
			ret = -EAGAIN;
			goto out;
		}
	}

	pci_priv->pci_link_state = PCI_LINK_DOWN;
	cnss_pci_set_auto_suspended(pci_priv, 1);
	cnss_pci_set_monitor_wake_intr(pci_priv, true);

	bus_bw_info = &plat_priv->bus_bw_info;
	msm_bus_scale_client_update_request(bus_bw_info->bus_client,
					    CNSS_BUS_WIDTH_NONE);
out:
	return ret;
}
EXPORT_SYMBOL(cnss_auto_suspend);

int cnss_auto_resume(void)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
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
	}

	cnss_set_pci_config_space(pci_priv, RESTORE_PCI_CONFIG_SPACE);
	pci_set_master(pci_dev);
	cnss_pci_set_auto_suspended(pci_priv, 0);

	bus_bw_info = &plat_priv->bus_bw_info;
	msm_bus_scale_client_update_request(bus_bw_info->bus_client,
					    bus_bw_info->current_bw_vote);
out:
	return ret;
}
EXPORT_SYMBOL(cnss_auto_resume);

static int cnss_pci_probe(struct pci_dev *pci_dev,
			  const struct pci_device_id *id)
{
	int ret = 0;
	struct cnss_pci_data *pci_priv;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct resource *res;

	cnss_pr_dbg("PCI is probing, vendor ID: 0x%x, device ID: 0x%x\n",
		    id->vendor, pci_dev->device);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		ret = -ENODEV;
		goto out;
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
	plat_priv->bus_priv = pci_priv;
	pci_priv->pci_device_id = id;
	pci_priv->device_id = pci_dev->device;
	cnss_set_pci_priv(pci_dev, pci_priv);

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
			goto reset_ctx;
		}
	}

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
	case QCA6290_DEVICE_ID:
		break;
	default:
		cnss_pr_err("Unknown PCI device found: 0x%x\n",
			    pci_dev->device);
		ret = -ENODEV;
		goto deinit_smmu;
	}

	ret = cnss_reg_pci_event(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to register PCI event, err = %d\n", ret);
		goto deinit_smmu;
	}

	return 0;
deinit_smmu:
	if (pci_priv->smmu_mapping)
		cnss_pci_deinit_smmu(pci_priv);
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

	cnss_dereg_pci_event(pci_priv);
	if (pci_priv->smmu_mapping)
		cnss_pci_deinit_smmu(pci_priv);
	plat_priv->bus_priv = NULL;
}

static const struct pci_device_id cnss_pci_id_table[] = {
	{ QCA6174_VENDOR_ID, QCA6174_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ QCA6290_VENDOR_ID, QCA6290_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, cnss_pci_id_table);

static const struct dev_pm_ops cnss_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cnss_pci_suspend, cnss_pci_resume)
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
