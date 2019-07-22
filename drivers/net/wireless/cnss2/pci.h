/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved. */

#ifndef _CNSS_PCI_H
#define _CNSS_PCI_H

#include <asm/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/mhi.h>
#include <linux/msm_pcie.h>
#include <linux/pci.h>

#include "main.h"

enum cnss_mhi_state {
	CNSS_MHI_INIT,
	CNSS_MHI_DEINIT,
	CNSS_MHI_POWER_ON,
	CNSS_MHI_POWER_OFF,
	CNSS_MHI_FORCE_POWER_OFF,
	CNSS_MHI_SUSPEND,
	CNSS_MHI_RESUME,
	CNSS_MHI_TRIGGER_RDDM,
	CNSS_MHI_RDDM,
	CNSS_MHI_RDDM_DONE,
};

enum pci_link_status {
	PCI_GEN1,
	PCI_GEN2,
	PCI_DEF,
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

struct cnss_pci_reg {
	char *name;
	u32 offset;
};

struct cnss_pci_debug_reg {
	u32 offset;
	u32 val;
};

struct cnss_pci_data {
	struct pci_dev *pci_dev;
	struct cnss_plat_data *plat_priv;
	const struct pci_device_id *pci_device_id;
	u32 device_id;
	u16 revision_id;
	struct cnss_wlan_driver *driver_ops;
	u8 pci_link_state;
	u8 pci_link_down_ind;
	struct pci_saved_state *saved_state;
	struct pci_saved_state *default_state;
	struct msm_pcie_register_event msm_pci_event;
	atomic_t auto_suspended;
	atomic_t drv_connected;
	u8 drv_connected_last;
	u16 def_link_speed;
	u16 def_link_width;
	u8 monitor_wake_intr;
	struct iommu_domain *iommu_domain;
	u8 smmu_s1_enable;
	dma_addr_t smmu_iova_start;
	size_t smmu_iova_len;
	dma_addr_t smmu_iova_ipa_start;
	size_t smmu_iova_ipa_len;
	void __iomem *bar;
	struct cnss_msi_config *msi_config;
	u32 msi_ep_base_data;
	struct mhi_controller *mhi_ctrl;
	unsigned long mhi_state;
	u32 remap_window;
	struct timer_list dev_rddm_timer;
	struct delayed_work time_sync_work;
	u8 disable_pc;
	struct cnss_pci_debug_reg *debug_reg;
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

static inline void cnss_pci_set_drv_connected(void *bus_priv, int val)
{
	struct cnss_pci_data *pci_priv = bus_priv;

	atomic_set(&pci_priv->drv_connected, val);
}

static inline int cnss_pci_get_drv_connected(void *bus_priv)
{
	struct cnss_pci_data *pci_priv = bus_priv;

	return atomic_read(&pci_priv->drv_connected);
}

int cnss_suspend_pci_link(struct cnss_pci_data *pci_priv);
int cnss_resume_pci_link(struct cnss_pci_data *pci_priv);
int cnss_pci_init(struct cnss_plat_data *plat_priv);
void cnss_pci_deinit(struct cnss_plat_data *plat_priv);
int cnss_pci_alloc_fw_mem(struct cnss_pci_data *pci_priv);
int cnss_pci_alloc_qdss_mem(struct cnss_pci_data *pci_priv);
void cnss_pci_free_qdss_mem(struct cnss_pci_data *pci_priv);
int cnss_pci_load_m3(struct cnss_pci_data *pci_priv);
int cnss_pci_set_mhi_state(struct cnss_pci_data *pci_priv,
			   enum cnss_mhi_state state);
int cnss_pci_start_mhi(struct cnss_pci_data *pci_priv);
void cnss_pci_stop_mhi(struct cnss_pci_data *pci_priv);
void cnss_pci_collect_dump_info(struct cnss_pci_data *pci_priv, bool in_panic);
void cnss_pci_clear_dump_info(struct cnss_pci_data *pci_priv);
u32 cnss_pci_get_wake_msi(struct cnss_pci_data *pci_priv);
int cnss_pci_force_fw_assert_hdlr(struct cnss_pci_data *pci_priv);
void cnss_pci_fw_boot_timeout_hdlr(struct cnss_pci_data *pci_priv);
int cnss_pci_call_driver_probe(struct cnss_pci_data *pci_priv);
int cnss_pci_call_driver_remove(struct cnss_pci_data *pci_priv);
int cnss_pci_dev_powerup(struct cnss_pci_data *pci_priv);
int cnss_pci_dev_shutdown(struct cnss_pci_data *pci_priv);
int cnss_pci_dev_crash_shutdown(struct cnss_pci_data *pci_priv);
int cnss_pci_dev_ramdump(struct cnss_pci_data *pci_priv);
int cnss_pci_register_driver_hdlr(struct cnss_pci_data *pci_priv, void *data);
int cnss_pci_unregister_driver_hdlr(struct cnss_pci_data *pci_priv);
int cnss_pci_call_driver_modem_status(struct cnss_pci_data *pci_priv,
				      int modem_current_status);
void cnss_pci_pm_runtime_show_usage_count(struct cnss_pci_data *pci_priv);
int cnss_pci_pm_request_resume(struct cnss_pci_data *pci_priv);
int cnss_pci_pm_runtime_resume(struct cnss_pci_data *pci_priv);
int cnss_pci_pm_runtime_get(struct cnss_pci_data *pci_priv);
int cnss_pci_pm_runtime_get_sync(struct cnss_pci_data *pci_priv);
void cnss_pci_pm_runtime_get_noresume(struct cnss_pci_data *pci_priv);
int cnss_pci_pm_runtime_put_autosuspend(struct cnss_pci_data *pci_priv);
void cnss_pci_pm_runtime_put_noidle(struct cnss_pci_data *pci_priv);
void cnss_pci_pm_runtime_mark_last_busy(struct cnss_pci_data *pci_priv);
int cnss_pci_update_status(struct cnss_pci_data *pci_priv,
			   enum cnss_driver_status status);

#endif /* _CNSS_PCI_H */
