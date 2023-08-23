/* Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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

#include <linux/suspend.h>

#define IPA_ETH_NET_DRIVER
#define IPA_ETH_OFFLOAD_DRIVER
#include <linux/ipa_eth.h>

#include "ipa_eth_debugfs.h"

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

/* Time to remain awake after a suspend abort due to NIC activity */
#define IPA_ETH_WAKE_TIME_MS 500

/* Time for NIC HW to settle down (ex. receive link interrupt) after a resume */
#define IPA_ETH_RESUME_SETTLE_MS 2000

#define IPA_ETH_PFDEV (ipa3_ctx ? ipa3_ctx->pdev : NULL)
#define IPA_ETH_SUBSYS "ipa_eth"

#define IPA_ETH_NET_DEVICE_MAX_EVENTS (NETDEV_CHANGE_TX_QUEUE_LEN + 1)
#define IPA_ETH_PM_NOTIFIER_MAX_EVENTS (PM_POST_RESTORE + 1)

enum ipa_eth_states {
	IPA_ETH_ST_READY,
	IPA_ETH_ST_UC_READY,
	IPA_ETH_ST_IPA_READY,
	IPA_ETH_ST_MAX,
};

enum ipa_eth_dev_flags {
	IPA_ETH_DEV_F_REMOVING,
	IPA_ETH_DEV_F_UNPAIRING,
	IPA_ETH_DEV_F_RESETTING,
	IPA_ETH_DEV_F_COUNT,
};

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

struct ipa_eth_upper_device {
	struct list_head upper_list;

	struct net_device *net_dev;
	struct ipa_eth_device *eth_dev;

	bool up; /* interface is up */
	bool registered; /* registered with IPA */
};

struct ipa_eth_device_private {
	struct ipa_eth_device *eth_dev;

	struct list_head upper_devices;

	unsigned long assume_active;
	struct rtnl_link_stats64 last_rtnl_stats;

	struct notifier_block pm_nb;
	struct notifier_block panic_nb;
};

#define eth_dev_priv(eth_dev) \
		((struct ipa_eth_device_private *)((eth_dev)->ipa_priv))

struct ipa_eth_bus {
	struct list_head bus_list;

	struct bus_type *bus;

	int (*register_driver)(struct ipa_eth_net_driver *nd);
	void (*unregister_driver)(struct ipa_eth_net_driver *nd);

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

extern unsigned long ipa_eth_state;
extern bool ipa_eth_noauto;
extern bool ipa_eth_ipc_logdbg;

bool ipa_eth_is_ready(void);
bool ipa_eth_all_ready(void);

static inline void ipa_eth_dev_assume_active_ms(
	struct ipa_eth_device *eth_dev,
	unsigned int msec)
{
	eth_dev_priv(eth_dev)->assume_active +=
			DIV_ROUND_UP(msec, IPA_ETH_WAKE_TIME_MS);
	pm_system_wakeup();
}

static inline void ipa_eth_dev_assume_active_inc(
	struct ipa_eth_device *eth_dev,
	unsigned int count)
{
	eth_dev_priv(eth_dev)->assume_active += count;
	pm_system_wakeup();
}

static inline void ipa_eth_dev_assume_active_dec(
	struct ipa_eth_device *eth_dev,
	unsigned int count)
{
	if (eth_dev_priv(eth_dev)->assume_active > count)
		eth_dev_priv(eth_dev)->assume_active -= count;
	else
		eth_dev_priv(eth_dev)->assume_active = 0;
}

struct ipa_eth_device *ipa_eth_alloc_device(
	struct device *dev,
	struct ipa_eth_net_driver *nd);
void ipa_eth_free_device(struct ipa_eth_device *eth_dev);
int ipa_eth_register_device(struct ipa_eth_device *eth_dev);
void ipa_eth_unregister_device(struct ipa_eth_device *eth_dev);

void ipa_eth_device_refresh_sync(struct ipa_eth_device *eth_dev);
void ipa_eth_device_refresh_sched(struct ipa_eth_device *eth_dev);
void ipa_eth_global_refresh_sched(void);

int ipa_eth_pci_modinit(void);
void ipa_eth_pci_modexit(void);

int ipa_eth_bus_modinit(void);
void ipa_eth_bus_modexit(void);

int ipa_eth_bus_register_driver(struct ipa_eth_net_driver *nd);
void ipa_eth_bus_unregister_driver(struct ipa_eth_net_driver *nd);

int ipa_eth_bus_enable_pc(struct ipa_eth_device *eth_dev);
int ipa_eth_bus_disable_pc(struct ipa_eth_device *eth_dev);

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
int ipa_eth_offload_prepare_reset(struct ipa_eth_device *eth_dev, void *data);
int ipa_eth_offload_complete_reset(struct ipa_eth_device *eth_dev, void *data);

bool ipa_eth_net_check_active(struct ipa_eth_device *eth_dev);

int ipa_eth_net_register_driver(struct ipa_eth_net_driver *nd);
void ipa_eth_net_unregister_driver(struct ipa_eth_net_driver *nd);
int ipa_eth_net_register_upper(struct ipa_eth_device *eth_dev);
int ipa_eth_net_unregister_upper(struct ipa_eth_device *eth_dev);

int ipa_eth_net_open_device(struct ipa_eth_device *eth_dev);
void ipa_eth_net_close_device(struct ipa_eth_device *eth_dev);

int ipa_eth_net_save_regs(struct ipa_eth_device *eth_dev);

int ipa_eth_ep_init_headers(struct ipa_eth_device *eth_dev);
int ipa_eth_ep_deinit_headers(struct ipa_eth_device *eth_dev);

int ipa_eth_ep_register_interface(struct ipa_eth_device *eth_dev);
int ipa_eth_ep_unregister_interface(struct ipa_eth_device *eth_dev);

int ipa_eth_ep_register_upper_interface(
	struct ipa_eth_upper_device *upper_eth_dev);
int ipa_eth_ep_unregister_upper_interface(
	struct ipa_eth_upper_device *upper_eth_dev);

void ipa_eth_ep_init_ctx(struct ipa_eth_channel *ch, bool vlan_mode);
void ipa_eth_ep_deinit_ctx(struct ipa_eth_channel *ch);

int ipa_eth_pm_register(struct ipa_eth_device *eth_dev);
int ipa_eth_pm_unregister(struct ipa_eth_device *eth_dev);

int ipa_eth_pm_activate(struct ipa_eth_device *eth_dev);
int ipa_eth_pm_deactivate(struct ipa_eth_device *eth_dev);

int ipa_eth_pm_vote_bw(struct ipa_eth_device *eth_dev);

int ipa_eth_uc_stats_init(struct ipa_eth_device *eth_dev);
int ipa_eth_uc_stats_deinit(struct ipa_eth_device *eth_dev);
int ipa_eth_uc_stats_start(struct ipa_eth_device *eth_dev);
int ipa_eth_uc_stats_stop(struct ipa_eth_device *eth_dev);

/* ipa_eth_utils.c APIs */

const char *ipa_eth_device_event_name(enum ipa_eth_device_event event);
const char *ipa_eth_net_device_event_name(unsigned long event);
const char *ipa_eth_pm_notifier_event_name(unsigned long event);

int ipa_eth_send_msg_connect(struct net_device *net_dev);
int ipa_eth_send_msg_disconnect(struct net_device *net_dev);

void *ipa_eth_get_ipc_logbuf(void);
void *ipa_eth_get_ipc_logbuf_dbg(void);

int ipa_eth_ipc_log_init(void);
void ipa_eth_ipc_log_cleanup(void);

#endif // _IPA_ETH_I_H_
