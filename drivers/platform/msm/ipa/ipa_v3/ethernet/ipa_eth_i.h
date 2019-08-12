/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#ifndef _IPA_ETH_I_H_
#define _IPA_ETH_I_H_

#define IPA_ETH_NET_DRIVER
#define IPA_ETH_OFFLOAD_DRIVER
#include <linux/ipa_eth.h>

#include "../ipa_i.h"

#ifdef CONFIG_IPA_ETH_NOAUTO
#define IPA_ETH_NOAUTO_DEFAULT true
#else
#define IPA_ETH_NOAUTO_DEFAULT false
#endif

#ifdef CONFIG_IPA_ETH_DEBUG
#define IPA_ETH_IPC_LOGDBG_DEFAULT true
#else
#define IPA_ETH_IPC_LOGDBG_DEFAULT false
#endif

#define IPA_ETH_PFDEV (ipa3_ctx ? ipa3_ctx->pdev : NULL)
#define IPA_ETH_SUBSYS "ipa_eth"

#define ipa_eth_err(fmt, args...) \
	do { \
		dev_err(IPA_ETH_PFDEV, \
			IPA_ETH_SUBSYS " %s:%d ERROR: " fmt "\n", \
			__func__, __LINE__, ## args); \
		ipa_eth_ipc_log("ERROR: " fmt, ## args); \
	} while (0)

#define ipa_eth_bug(fmt, args...) \
	do { \
		ipa_eth_err("BUG: " fmt, ## args); \
		dump_stack(); \
	} while (0)

#define ipa_eth_log(fmt, args...) \
	do { \
		dev_dbg(IPA_ETH_PFDEV, \
			IPA_ETH_SUBSYS " %s:%d " fmt "\n", \
			__func__, __LINE__, ## args); \
		ipa_eth_ipc_log(fmt, ## args); \
	} while (0)

#define ipa_eth_dbg(fmt, args...) \
	do { \
		dev_dbg(IPA_ETH_PFDEV, \
			IPA_ETH_SUBSYS " %s:%d " fmt "\n", \
			__func__, __LINE__, ## args); \
		ipa_eth_ipc_dbg("DEBUG: " fmt, ## args); \
	} while (0)

#define ipa_eth_dev_err(edev, fmt, args...) \
	ipa_eth_err("(%s) " fmt, ((edev && edev->net_dev) ? \
		edev->net_dev->name : "<unpaired>"), \
		## args)

#define ipa_eth_dev_bug(edev, fmt, args...) \
	ipa_eth_bug("(%s) " fmt, ((edev && edev->net_dev) ? \
		edev->net_dev->name : "<unpaired>"), \
		## args)

#define ipa_eth_dev_dbg(edev, fmt, args...) \
	ipa_eth_dbg("(%s) " fmt, ((edev && edev->net_dev) ? \
		edev->net_dev->name : "<unpaired>"), \
		## args)

#define ipa_eth_dev_log(edev, fmt, args...) \
	ipa_eth_log("(%s) " fmt, ((edev && edev->net_dev) ? \
		edev->net_dev->name : "<unpaired>"), \
		## args)

struct ipa_eth_bus {
	struct list_head bus_list;

	struct bus_type *bus;

	int (*register_net_driver)(struct ipa_eth_net_driver *nd);
	void (*unregister_net_driver)(struct ipa_eth_net_driver *nd);

	int (*enable_pc)(struct ipa_eth_device *eth_dev);
	int (*disable_pc)(struct ipa_eth_device *eth_dev);
};

extern struct ipa_eth_bus ipa_eth_pci_bus;

struct ipa_eth_cb_map_param {
	bool map;
	bool sym;
	int iommu_prot;
	enum dma_data_direction dma_dir;
	const struct ipa_smmu_cb_ctx *cb_ctx;
};

int ipa_eth_register_device(struct ipa_eth_device *eth_dev);
void ipa_eth_unregister_device(struct ipa_eth_device *eth_dev);

int ipa_eth_iommu_map(struct iommu_domain *domain,
	dma_addr_t daddr, void *addr, bool is_va,
	size_t size, int prot, bool split);
int ipa_eth_iommu_unmap(struct iommu_domain *domain,
	dma_addr_t daddr, size_t size, bool split);

int ipa_eth_pci_modinit(struct dentry *dbgfs_root);
void ipa_eth_pci_modexit(void);

int ipa_eth_bus_modinit(struct dentry *dbgfs_root);
void ipa_eth_bus_modexit(void);

int ipa_eth_bus_register_driver(struct ipa_eth_net_driver *nd);
void ipa_eth_bus_unregister_driver(struct ipa_eth_net_driver *nd);

int ipa_eth_bus_enable_pc(struct ipa_eth_device *eth_dev);
int ipa_eth_bus_disable_pc(struct ipa_eth_device *eth_dev);

int ipa_eth_offload_modinit(struct dentry *dbgfs_root);
void ipa_eth_offload_modexit(void);

int ipa_eth_offload_register_driver(struct ipa_eth_offload_driver *od);
void ipa_eth_offload_unregister_driver(struct ipa_eth_offload_driver *od);

static inline bool ipa_eth_offload_device_paired(struct ipa_eth_device *eth_dev)
{
	return eth_dev->od != NULL;
}

int ipa_eth_offload_pair_device(struct ipa_eth_device *eth_dev);
void ipa_eth_offload_unpair_device(struct ipa_eth_device *eth_dev);

int ipa_eth_offload_init(struct ipa_eth_device *eth_dev);
int ipa_eth_offload_deinit(struct ipa_eth_device *eth_dev);
int ipa_eth_offload_start(struct ipa_eth_device *eth_dev);
int ipa_eth_offload_stop(struct ipa_eth_device *eth_dev);

int ipa_eth_offload_save_regs(struct ipa_eth_device *eth_dev);

int ipa_eth_net_open_device(struct ipa_eth_device *eth_dev);
void ipa_eth_net_close_device(struct ipa_eth_device *eth_dev);

int ipa_eth_net_save_regs(struct ipa_eth_device *eth_dev);

int ipa_eth_ep_init_headers(struct ipa_eth_device *eth_dev);
int ipa_eth_ep_register_interface(struct ipa_eth_device *eth_dev);
int ipa_eth_ep_unregister_interface(struct ipa_eth_device *eth_dev);
void ipa_eth_ep_init_ctx(struct ipa_eth_channel *ch, bool vlan_mode);
void ipa_eth_ep_deinit_ctx(struct ipa_eth_channel *ch);

int ipa_eth_pm_register(struct ipa_eth_device *eth_dev);
int ipa_eth_pm_unregister(struct ipa_eth_device *eth_dev);

int ipa_eth_pm_activate(struct ipa_eth_device *eth_dev);
int ipa_eth_pm_deactivate(struct ipa_eth_device *eth_dev);

int ipa_eth_pm_vote_bw(struct ipa_eth_device *eth_dev);

#endif // _IPA_ETH_I_H_
