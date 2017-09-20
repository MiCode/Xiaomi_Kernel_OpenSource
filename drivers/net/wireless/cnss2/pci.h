/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef _CNSS_PCI_H
#define _CNSS_PCI_H

#include <asm/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/msm_mhi.h>
#include <linux/msm_pcie.h>
#include <linux/pci.h>

#include "main.h"

#define QCA6174_VENDOR_ID		0x168C
#define QCA6174_DEVICE_ID		0x003E
#define QCA6174_REV_ID_OFFSET		0x08
#define QCA6174_REV3_VERSION		0x5020000
#define QCA6174_REV3_2_VERSION		0x5030000
#define QCA6290_VENDOR_ID		0x17CB
#define QCA6290_DEVICE_ID		0x1100
#define QCA6290_EMULATION_VENDOR_ID	0x168C
#define QCA6290_EMULATION_DEVICE_ID	0xABCD

enum cnss_mhi_state {
	CNSS_MHI_INIT,
	CNSS_MHI_DEINIT,
	CNSS_MHI_SUSPEND,
	CNSS_MHI_RESUME,
	CNSS_MHI_POWER_OFF,
	CNSS_MHI_POWER_ON,
	CNSS_MHI_TRIGGER_RDDM,
	CNSS_MHI_RDDM,
	CNSS_MHI_RDDM_KERNEL_PANIC,
	CNSS_MHI_NOTIFY_LINK_ERROR,
};

struct cnss_msi_user {
	char *name;
	int num_vectors;
	u32 base_vector;
};

struct cnss_msi_config {
	int total_vectors;
	int total_users;
	struct cnss_msi_user *users;
};

struct cnss_pci_data {
	struct pci_dev *pci_dev;
	struct cnss_plat_data *plat_priv;
	const struct pci_device_id *pci_device_id;
	u32 device_id;
	u16 revision_id;
	bool pci_link_state;
	bool pci_link_down_ind;
	struct pci_saved_state *saved_state;
	struct msm_pcie_register_event msm_pci_event;
	atomic_t auto_suspended;
	bool monitor_wake_intr;
	struct dma_iommu_mapping *smmu_mapping;
	dma_addr_t smmu_iova_start;
	size_t smmu_iova_len;
	void __iomem *bar;
	struct cnss_msi_config *msi_config;
	u32 msi_ep_base_data;
	struct mhi_device mhi_dev;
	unsigned long mhi_state;
};

static inline void cnss_set_pci_priv(struct pci_dev *pci_dev, void *data)
{
	pci_set_drvdata(pci_dev, data);
}

static inline struct cnss_pci_data *cnss_get_pci_priv(struct pci_dev *pci_dev)
{
	return pci_get_drvdata(pci_dev);
}

static inline struct cnss_plat_data *cnss_pci_priv_to_plat_priv(void *bus_priv)
{
	struct cnss_pci_data *pci_priv = bus_priv;

	return pci_priv->plat_priv;
}

static inline void cnss_pci_set_monitor_wake_intr(void *bus_priv, bool val)
{
	struct cnss_pci_data *pci_priv = bus_priv;

	pci_priv->monitor_wake_intr = val;
}

static inline bool cnss_pci_get_monitor_wake_intr(void *bus_priv)
{
	struct cnss_pci_data *pci_priv = bus_priv;

	return pci_priv->monitor_wake_intr;
}

static inline void cnss_pci_set_auto_suspended(void *bus_priv, int val)
{
	struct cnss_pci_data *pci_priv = bus_priv;

	atomic_set(&pci_priv->auto_suspended, val);
}

static inline int cnss_pci_get_auto_suspended(void *bus_priv)
{
	struct cnss_pci_data *pci_priv = bus_priv;

	return atomic_read(&pci_priv->auto_suspended);
}

int cnss_suspend_pci_link(struct cnss_pci_data *pci_priv);
int cnss_resume_pci_link(struct cnss_pci_data *pci_priv);
int cnss_pci_init(struct cnss_plat_data *plat_priv);
void cnss_pci_deinit(struct cnss_plat_data *plat_priv);
int cnss_pci_alloc_fw_mem(struct cnss_pci_data *pci_priv);
int cnss_pci_load_m3(struct cnss_pci_data *pci_priv);
int cnss_pci_get_bar_info(struct cnss_pci_data *pci_priv, void __iomem **va,
			  phys_addr_t *pa);
int cnss_pci_set_mhi_state(struct cnss_pci_data *pci_priv,
			   enum cnss_mhi_state state);
int cnss_pci_start_mhi(struct cnss_pci_data *pci_priv);
void cnss_pci_stop_mhi(struct cnss_pci_data *pci_priv);
void cnss_pci_collect_dump_info(struct cnss_pci_data *pci_priv);
void cnss_pci_clear_dump_info(struct cnss_pci_data *pci_priv);
int cnss_pm_request_resume(struct cnss_pci_data *pci_priv);

#endif /* _CNSS_PCI_H */
