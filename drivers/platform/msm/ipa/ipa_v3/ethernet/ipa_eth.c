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

#include <linux/module.h>
#include <linux/timer.h>

#include "ipa_eth_i.h"

unsigned long ipa_eth_state;

static LIST_HEAD(ipa_eth_devices);
static DEFINE_MUTEX(ipa_eth_devices_lock);

bool ipa_eth_noauto = IPA_ETH_NOAUTO_DEFAULT;
module_param(ipa_eth_noauto, bool, 0444);
MODULE_PARM_DESC(ipa_eth_noauto,
	"Disable automatic offload initialization of interfaces");

static struct workqueue_struct *ipa_eth_wq;

bool ipa_eth_is_ready(void)
{
	return test_bit(IPA_ETH_ST_READY, &ipa_eth_state);
}

bool ipa_eth_all_ready(void)
{
	return ipa_eth_is_ready() &&
		test_bit(IPA_ETH_ST_UC_READY, &ipa_eth_state) &&
		test_bit(IPA_ETH_ST_IPA_READY, &ipa_eth_state);
}

static inline bool present(struct ipa_eth_device *eth_dev)
{
	return
		!test_bit(IPA_ETH_DEV_F_REMOVING, &eth_dev->flags);
}

static inline bool reachable(struct ipa_eth_device *eth_dev)
{
	return
		present(eth_dev) &&
		!test_bit(IPA_ETH_DEV_F_RESETTING, &eth_dev->flags);
}

static inline bool offloadable(struct ipa_eth_device *eth_dev)
{
	return
		ipa_eth_all_ready() &&
		reachable(eth_dev) &&
		!test_bit(IPA_ETH_DEV_F_UNPAIRING, &eth_dev->flags);
}

static inline bool initable(struct ipa_eth_device *eth_dev)
{
	return
		offloadable(eth_dev) &&
		test_bit(IPA_ETH_IF_ST_UP, &eth_dev->if_state) &&
		eth_dev->init;
}

static inline bool startable(struct ipa_eth_device *eth_dev)
{
	return
		initable(eth_dev) &&
		test_bit(IPA_ETH_IF_ST_LOWER_UP, &eth_dev->if_state) &&
		eth_dev->start;
}

static int ipa_eth_init_device(struct ipa_eth_device *eth_dev)
{
	int rc;

	if (eth_dev->of_state == IPA_ETH_OF_ST_INITED)
		return 0;

	if (eth_dev->of_state != IPA_ETH_OF_ST_DEINITED)
		return -EFAULT;

	rc = ipa_eth_ep_init_headers(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to init EP headers");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_pm_register(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to register with IPA PM");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_offload_init(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to init offload");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_uc_stats_init(eth_dev);
	if (rc)
		ipa_eth_dev_err(eth_dev,
			"Failed to init uC stats monitor, continuing.");

	ipa_eth_dev_log(eth_dev, "Initialized device");

	eth_dev->of_state = IPA_ETH_OF_ST_INITED;

	return 0;
}

static int ipa_eth_deinit_device(struct ipa_eth_device *eth_dev)
{
	int rc;

	if (eth_dev->of_state == IPA_ETH_OF_ST_DEINITED)
		return 0;

	if (eth_dev->of_state != IPA_ETH_OF_ST_INITED)
		return -EFAULT;

	rc = ipa_eth_uc_stats_deinit(eth_dev);
	if (rc)
		ipa_eth_dev_err(eth_dev,
			"Failed to deinit uC stats monitor, continuing.");

	rc = ipa_eth_offload_deinit(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to deinit offload");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_pm_unregister(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to unregister with IPA PM");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_ep_deinit_headers(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to deinit EP headers");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	ipa_eth_dev_log(eth_dev, "Deinitialized device");

	eth_dev->of_state = IPA_ETH_OF_ST_DEINITED;

	return 0;
}

static void ipa_eth_free_msg(void *buff, u32 len, u32 type) {}

static int ipa_eth_start_device(struct ipa_eth_device *eth_dev)
{
	int rc;
	struct ipa_msg_meta msg_meta;
	struct ipa_ecm_msg ecm_msg;

	if (eth_dev->of_state == IPA_ETH_OF_ST_STARTED)
		return 0;

	if (eth_dev->of_state != IPA_ETH_OF_ST_INITED)
		return -EFAULT;

	rc = ipa_eth_pm_activate(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to activate device PM");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_offload_start(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to start offload");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_bus_disable_pc(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to disable bus power collapse");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	/* We cannot register the interface during offload init phase because
	 * it will cause IPACM to install IPA filter rules when it receives a
	 * link-up netdev event, even if offload path is not started and/or no
	 * ECM_CONNECT event is received from the driver. Since installing IPA
	 * filter rules while offload path is stopped can cause DL data stall,
	 * register the interface only after the offload path is started.
	 */
	rc = ipa_eth_ep_register_interface(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to register EP interface");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_uc_stats_start(eth_dev);
	if (rc)
		ipa_eth_dev_err(eth_dev,
			"Failed to start uC stats monitor, continuing.");

	ipa_eth_dev_log(eth_dev, "Started device");

	eth_dev->of_state = IPA_ETH_OF_ST_STARTED;

	return 0;
}

static int ipa_eth_stop_device(struct ipa_eth_device *eth_dev)
{
	int rc;
	struct ipa_msg_meta msg_meta;
	struct ipa_ecm_msg ecm_msg;

	memset(&msg_meta, 0, sizeof(msg_meta));
	memset(&ecm_msg, 0, sizeof(ecm_msg));

	ecm_msg.ifindex = eth_dev->net_dev->ifindex;
	strlcpy(ecm_msg.name, eth_dev->net_dev->name, IPA_RESOURCE_NAME_MAX);

	msg_meta.msg_type = ECM_DISCONNECT;
	msg_meta.msg_len = sizeof(struct ipa_ecm_msg);
	(void) ipa_send_msg(&msg_meta, &ecm_msg, ipa_eth_free_msg);

	if (eth_dev->of_state == IPA_ETH_OF_ST_DEINITED)
		return 0;

	if (eth_dev->of_state != IPA_ETH_OF_ST_STARTED)
		return -EFAULT;

	rc = ipa_eth_uc_stats_stop(eth_dev);
	if (rc)
		ipa_eth_dev_err(eth_dev,
			"Failed to stop uC stats monitor, continuing.");

	rc = ipa_eth_ep_unregister_interface(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to unregister IPA interface");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_bus_enable_pc(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to enable bus power collapse");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_offload_stop(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to stop offload");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_pm_deactivate(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to deactivate device PM");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	ipa_eth_dev_log(eth_dev, "Stopped device");

	eth_dev->of_state = IPA_ETH_OF_ST_INITED;

	return 0;
}

static void ipa_eth_device_refresh(struct ipa_eth_device *eth_dev)
{
	ipa_eth_dev_log(eth_dev, "Refreshing offload state for device");

	if (!ipa_eth_offload_device_paired(eth_dev)) {
		ipa_eth_dev_log(eth_dev, "Device is not paired. Skipping.");
		return;
	}

	if (eth_dev->of_state == IPA_ETH_OF_ST_ERROR) {
		ipa_eth_dev_err(eth_dev,
				"Device in ERROR state, skipping refresh");
		return;
	}

	if (initable(eth_dev)) {
		if (eth_dev->of_state == IPA_ETH_OF_ST_DEINITED) {
			IPA_ACTIVE_CLIENTS_INC_SIMPLE();
			(void) ipa_eth_init_device(eth_dev);
			IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

			if (eth_dev->of_state != IPA_ETH_OF_ST_INITED) {
				ipa_eth_dev_err(eth_dev,
						"Failed to init device");
				return;
			}
		}
	}

	if (startable(eth_dev)) {
		IPA_ACTIVE_CLIENTS_INC_SIMPLE();
		(void) ipa_eth_start_device(eth_dev);
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

		if (eth_dev->of_state != IPA_ETH_OF_ST_STARTED) {
			ipa_eth_dev_err(eth_dev, "Failed to start device");
			return;
		}

		if (ipa_eth_net_register_upper(eth_dev)) {
			ipa_eth_dev_err(eth_dev,
				"Failed to register upper interfaces");
			eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		}

		if (ipa_eth_pm_vote_bw(eth_dev))
			ipa_eth_dev_err(eth_dev,
					"Failed to vote for required BW");
	} else {
		ipa_eth_dev_log(eth_dev, "Start is disallowed for the device");

		if (ipa_eth_net_unregister_upper(eth_dev)) {
			ipa_eth_dev_err(eth_dev,
				"Failed to unregister upper interfaces");
			eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		}

		if (eth_dev->of_state == IPA_ETH_OF_ST_STARTED) {
			IPA_ACTIVE_CLIENTS_INC_SIMPLE();
			ipa_eth_stop_device(eth_dev);
			IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

			if (eth_dev->of_state != IPA_ETH_OF_ST_INITED) {
				ipa_eth_dev_err(eth_dev,
						"Failed to stop device");
				return;
			}
		}
	}

	if (!initable(eth_dev)) {
		ipa_eth_dev_log(eth_dev, "Init is disallowed for the device");

		IPA_ACTIVE_CLIENTS_INC_SIMPLE();
		ipa_eth_deinit_device(eth_dev);
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

		if (eth_dev->of_state != IPA_ETH_OF_ST_DEINITED) {
			ipa_eth_dev_err(eth_dev, "Failed to deinit device");
			return;
		}
	}
}

static void ipa_eth_device_refresh_work(struct work_struct *work)
{
	struct ipa_eth_device *eth_dev = container_of(work,
				struct ipa_eth_device, refresh);

	ipa_eth_device_refresh(eth_dev);
}

void ipa_eth_device_refresh_sched(struct ipa_eth_device *eth_dev)
{
	queue_work(ipa_eth_wq, &eth_dev->refresh);
}

void ipa_eth_device_refresh_sync(struct ipa_eth_device *eth_dev)
{
	ipa_eth_device_refresh_sched(eth_dev);
	flush_work(&eth_dev->refresh);
}

static void ipa_eth_global_refresh_work(struct work_struct *work)
{
	struct ipa_eth_device *eth_dev;

	ipa_eth_log("Performing global refresh");

	mutex_lock(&ipa_eth_devices_lock);

	if (ipa_eth_all_ready()) {
		list_for_each_entry(eth_dev, &ipa_eth_devices, device_list) {
			ipa_eth_device_refresh_sched(eth_dev);
		}
	}

	mutex_unlock(&ipa_eth_devices_lock);
}

static DECLARE_WORK(ipa_eth_global_refresh, ipa_eth_global_refresh_work);

void ipa_eth_global_refresh_sched(void)
{
	queue_work(ipa_eth_wq, &ipa_eth_global_refresh);
}

static void ipa_eth_global_refresh_sync(void)
{
	ipa_eth_global_refresh_sched();
	flush_workqueue(ipa_eth_wq);
}

static int ipa_eth_device_prepare_reset(
	struct ipa_eth_device *eth_dev, void *data)
{
	int rc = 0;

	/* Set the bit so that any in-progress operation can possibly
	 * return early.
	 */
	set_bit(IPA_ETH_DEV_F_RESETTING, &eth_dev->flags);

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	rc = ipa_eth_offload_prepare_reset(eth_dev, data);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return rc;
}

static int ipa_eth_device_complete_reset(
	struct ipa_eth_device *eth_dev, void *data)
{
	int rc = 0;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	rc = ipa_eth_offload_complete_reset(eth_dev, data);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	/* Clear the flag before unlocking the mutex so that blocked threads
	 * can resume with the updated value.
	 */
	clear_bit(IPA_ETH_DEV_F_RESETTING, &eth_dev->flags);

	return rc;
}

static int ipa_eth_device_event_add_macsec(
	struct ipa_eth_device *eth_dev, void *data)
{
	struct net_device *net_dev = data;

	ipa_eth_dev_log(eth_dev,
		"Network device added MACSec interface %s", net_dev->name);

	return 0;
}

static int ipa_eth_device_event_del_macsec(
	struct ipa_eth_device *eth_dev, void *data)
{
	struct net_device *net_dev = data;

	ipa_eth_dev_log(eth_dev,
		"Network device removed MACSec interface %s", net_dev->name);

	return 0;
}

/**
 * ipa_eth_device_notify() - Notifies a device event to the offload sub-system
 * @eth_dev: Device for which the event is generated
 * @event: Device event
 * @data: Event specific data, if required
 *
 * Return: 0 on success, non-zero otherwise
 */
int ipa_eth_device_notify(struct ipa_eth_device *eth_dev,
	enum ipa_eth_device_event event, void *data)
{
	int rc = -EINVAL;

	ipa_eth_dev_log(eth_dev,
		"Received notificaiton %s", ipa_eth_device_event_name(event));

	switch (event) {
	case IPA_ETH_DEV_RESET_PREPARE:
		rc = ipa_eth_device_prepare_reset(eth_dev, data);
		break;
	case IPA_ETH_DEV_RESET_COMPLETE:
		rc = ipa_eth_device_complete_reset(eth_dev, data);
		break;
	case IPA_ETH_DEV_ADD_MACSEC_IF:
		rc = ipa_eth_device_event_add_macsec(eth_dev, data);
		break;
	case IPA_ETH_DEV_DEL_MACSEC_IF:
		rc = ipa_eth_device_event_del_macsec(eth_dev, data);
		break;
	default:
		ipa_eth_dev_log(eth_dev, "Unknown event %d", event);
		break;
	}

	if (rc)
		ipa_eth_dev_err(eth_dev, "Failed to handle notification");

	return rc;
}
EXPORT_SYMBOL(ipa_eth_device_notify);

static void ipa_eth_dev_start_timer_cb(unsigned long data)
{
	struct ipa_eth_device *eth_dev = (struct ipa_eth_device *)data;

	ipa_eth_dev_log(eth_dev, "Start timer has fired");

	/* Do not start offload if user disabled timer in between */
	if (present(eth_dev) && eth_dev->start_on_timeout) {
		eth_dev->start = true;

		ipa_eth_device_refresh_sched(eth_dev);
	}
}

static int ipa_eth_uc_ready_cb(struct notifier_block *nb,
	unsigned long action, void *data)
{
	ipa_eth_log("IPA uC is ready");

	set_bit(IPA_ETH_ST_UC_READY, &ipa_eth_state);
	ipa_eth_global_refresh_sched();

	return NOTIFY_OK;
}

static struct notifier_block uc_ready_cb = {
	.notifier_call = ipa_eth_uc_ready_cb,
};

static void ipa_eth_ipa_ready_cb(void *data)
{
	ipa_eth_log("IPA is ready");

	set_bit(IPA_ETH_ST_IPA_READY, &ipa_eth_state);
	ipa_eth_global_refresh_sched();
}

static int ipa_eth_panic_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct ipa_eth_device_private *ipa_priv = container_of(nb,
				struct ipa_eth_device_private, panic_nb);
	struct ipa_eth_device *eth_dev = ipa_priv->eth_dev;

	if (!reachable(eth_dev))
		return -ENODEV;

	ipa_eth_net_save_regs(eth_dev);
	ipa_eth_offload_save_regs(eth_dev);

	return NOTIFY_DONE;
}

/* During a system suspend, suspend-prepare callbacks are called first by the
 * PM framework before freezing processes. This gives us an early opportunity
 * to abort the suspend and reduces the chances for device resumes at a later
 * stage.
 */
static int ipa_eth_pm_notifier_event_suspend_prepare(
	struct ipa_eth_device *eth_dev)
{
	/* We look for any ethernet rx activity since previous attempt to
	 * suspend, and if such an activity is found, we hold a wake lock
	 * for WAKE_TIME_MS time. Any Rx packets received beyond this point
	 * should cause a wake up due to the Rx interrupt. In rare cases
	 * where Rx interrupt was already processed and NAPI poll is yet to
	 * complete, we perform a second check in the suspend late handler
	 * and reverts the device suspension by aborting the system suspend.
	 */
	if (ipa_eth_net_check_active(eth_dev)) {
		pr_info("%s: %s is active, preventing suspend for %u ms",
				IPA_ETH_SUBSYS, eth_dev->net_dev->name,
				IPA_ETH_WAKE_TIME_MS);
		pm_wakeup_dev_event(eth_dev->dev, IPA_ETH_WAKE_TIME_MS, false);
		return NOTIFY_BAD;
	}

	return NOTIFY_OK;
}

static int ipa_eth_pm_notifier_cb(struct notifier_block *nb,
	unsigned long pm_event, void *unused)
{
	struct ipa_eth_device_private *ipa_priv = container_of(nb,
				struct ipa_eth_device_private, pm_nb);
	struct ipa_eth_device *eth_dev = ipa_priv->eth_dev;

	ipa_eth_dbg("PM notifier called for event %s (0x%04lx)",
			ipa_eth_pm_notifier_event_name(pm_event), pm_event);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		return ipa_eth_pm_notifier_event_suspend_prepare(eth_dev);
	default:
		break;
	}

	return NOTIFY_DONE;
}

/*
 * ipa_eth_alloc_device() - Allocate an ipa_eth_device structure and initialize
 *                          all common fields
 * @dev: struct device pointer of the net device
 * @nd: Offload sub-system net driver structure
 *
 * This API is meant to be called by the bus layer (ex. ipa_eth_pci.c) to
 * allocate an ipa_eth_device object, initialize it with bus specific values
 * and register the discovered device using ipa_eth_register_device() API.
 *
 * Return: 0 on success, non-zero otherwise
 */
struct ipa_eth_device *ipa_eth_alloc_device(
	struct device *dev,
	struct ipa_eth_net_driver *nd)
{
	struct ipa_eth_device *eth_dev;
	struct ipa_eth_device_private *ipa_priv;

	if (!dev || !nd) {
		ipa_eth_err("Invalid device or net driver");
		return NULL;
	}

	eth_dev = kzalloc(sizeof(*eth_dev), GFP_KERNEL);
	if (!eth_dev)
		return NULL;

	ipa_priv = kzalloc(sizeof(*ipa_priv), GFP_KERNEL);
	if (!ipa_priv) {
		kfree(eth_dev);
		return NULL;
	}

	eth_dev->dev = dev;
	eth_dev->nd = nd;

	eth_dev->of_state = IPA_ETH_OF_ST_DEINITED;
	eth_dev->pm_handle = IPA_PM_MAX_CLIENTS;
	INIT_WORK(&eth_dev->refresh, ipa_eth_device_refresh_work);

	INIT_LIST_HEAD(&eth_dev->rx_channels);
	INIT_LIST_HEAD(&eth_dev->tx_channels);

	init_timer(&eth_dev->start_timer);

	eth_dev->start_timer.function = ipa_eth_dev_start_timer_cb;
	eth_dev->start_timer.data = (unsigned long)eth_dev;

	eth_dev->init = eth_dev->start = !ipa_eth_noauto;

	/* Initialize private data */

	ipa_priv->eth_dev = eth_dev;

	INIT_LIST_HEAD(&ipa_priv->upper_devices);

	ipa_priv->pm_nb.notifier_call = ipa_eth_pm_notifier_cb;
	ipa_priv->panic_nb.notifier_call = ipa_eth_panic_notifier;

	eth_dev->ipa_priv = ipa_priv;

	return eth_dev;
}

/*
 * ipa_eth_free_device() - Free an ipa_eth_device object previously allocated
 *                         using ipa_eth_alloc_device()
 * @eth_dev: ipa_eth_device object to be freed
 */
void ipa_eth_free_device(struct ipa_eth_device *eth_dev)
{
	kzfree(eth_dev->ipa_priv);
	kzfree(eth_dev);
}

/*
 * ipa_eth_register_device() - Register a new device with offload sub-system
 * @eth_dev: Eth device to register
 *
 * This API is meant to be called by the bus layer (ex. ipa_eth_pci.c) to
 * register an ipa_eth_device object linked to a newly discovered device. The
 * @eth_dev object is expected to be allocated using ipa_eth_alloc_device() API.
 *
 * Return: 0 on success, non-zero otherwise
 */
int ipa_eth_register_device(struct ipa_eth_device *eth_dev)
{
	int rc;
	struct ipa_eth_device_private *ipa_priv = eth_dev->ipa_priv;

	ipa_eth_dev_log(eth_dev, "Registering new device");

	(void) atomic_notifier_chain_register(
			&panic_notifier_list, &ipa_priv->panic_nb);

	rc = ipa_eth_net_open_device(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to open network device");
		return rc;
	}

	/* Add new device to the devices list before pairing with offload
	 * driver in order to synchronize with offload driver deregistration.
	 */
	mutex_lock(&ipa_eth_devices_lock);
	list_add(&eth_dev->device_list, &ipa_eth_devices);
	mutex_unlock(&ipa_eth_devices_lock);

	(void) register_pm_notifier(&ipa_priv->pm_nb);

	ipa_eth_dev_log(eth_dev, "Registered new device");

	rc = ipa_eth_offload_pair_device(eth_dev);
	if (rc)
		ipa_eth_dev_log(eth_dev, "Failed to pair device. Deferring.");

	ipa_eth_debugfs_add_device(eth_dev);

	return 0;
}

/*
 * ipa_eth_unpair_device() - Unpair a device from its offload driver
 * @eth_dev: Device to unpair
 *
 * It is safe to call this function in parallel from ipa_eth_unregister_device()
 * and ipa_eth_unregister_offload_driver() as the necessary locking is placed
 * in ipa_eth_offload_unpair_device().
 *
 * Return: 0 on success, non-zero otherwise
 */

static void ipa_eth_unpair_device(struct ipa_eth_device *eth_dev)
{
	ipa_eth_dev_log(eth_dev, "Unpairing device");

	/* Set UNPAIRING flag to prevent offload init or start */
	set_bit(IPA_ETH_DEV_F_UNPAIRING, &eth_dev->flags);

	/* Wait for next refresh to tear down offload path */
	ipa_eth_device_refresh_sync(eth_dev);

	ipa_eth_offload_unpair_device(eth_dev);

	clear_bit(IPA_ETH_DEV_F_UNPAIRING, &eth_dev->flags);

	ipa_eth_dev_log(eth_dev, "Unpaired device");
}

/*
 * ipa_eth_unregister_device() - Unregister a device from the offload sub-system
 *
 * This API is meant to be called by the bus layer when a device is removed from
 * the system. The @eth_dev object itself need to be freed separately by calling
 * the ipa_eth_free_device() API.
 */
void ipa_eth_unregister_device(struct ipa_eth_device *eth_dev)
{
	struct ipa_eth_device_private *ipa_priv = eth_dev->ipa_priv;

	ipa_eth_dev_log(eth_dev, "Unregistering device");

	/* Set REMOVING flag so that device refreshes do not happen */
	set_bit(IPA_ETH_DEV_F_REMOVING, &eth_dev->flags);

	/* Remove debugfs node to prevent any new 'start_timer' to be
	 * be started.
	 */
	ipa_eth_debugfs_remove_device(eth_dev);

	del_timer_sync(&eth_dev->start_timer);

	/* Unpair the device before removing from devices list so that
	 * unregister_offload_driver() does not skip this device.
	 */
	ipa_eth_unpair_device(eth_dev);

	(void) unregister_pm_notifier(&ipa_priv->pm_nb);

	/* Remove from devices list so that no new global refreshes are
	 * scheduled.
	 */
	mutex_lock(&ipa_eth_devices_lock);
	list_del(&eth_dev->device_list);
	mutex_unlock(&ipa_eth_devices_lock);

	/* Closing device from ipa_eth_net should prevent further events to be
	 * received from either network driver via ipa_eth_device_notify() or
	 * the network sub-system via netdevice notifier.
	 */
	ipa_eth_net_close_device(eth_dev);

	/* No more refresh work would be queued beyond this point. Flush out
	 * any pending refresh work items.
	 */
	cancel_work_sync(&eth_dev->refresh);

	/* By this time we would have processed all pending events for the
	 * device and removed/disabled all event sources.
	 */
	clear_bit(IPA_ETH_DEV_F_REMOVING, &eth_dev->flags);

	(void) atomic_notifier_chain_unregister(
			&panic_notifier_list, &ipa_priv->panic_nb);

	ipa_eth_dev_log(eth_dev, "Device unregistered");
}

/**
 * ipa_eth_register_net_driver() - Register a network driver with the offload
 *                                 subsystem
 * @nd: Network driver to register
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_register_net_driver(struct ipa_eth_net_driver *nd)
{
	return ipa_eth_net_register_driver(nd);
}
EXPORT_SYMBOL(ipa_eth_register_net_driver);

/**
 * ipa_eth_unregister_net_driver() - Unregister a network driver
 * @nd: Network driver to unregister
 */
void ipa_eth_unregister_net_driver(struct ipa_eth_net_driver *nd)
{
	ipa_eth_net_unregister_driver(nd);
}
EXPORT_SYMBOL(ipa_eth_unregister_net_driver);

/**
 * ipa_eth_register_offload_driver - Register an offload driver with the offload
 *                                   subsystem
 * @nd: Offload driver to register
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_register_offload_driver(struct ipa_eth_offload_driver *od)
{
	int rc;
	struct ipa_eth_device *eth_dev;

	rc = ipa_eth_offload_register_driver(od);
	if (rc) {
		ipa_eth_err("Failed to register offload driver %s", od->name);
		return rc;
	}

	ipa_eth_log("Registered offload driver %s", od->name);

	mutex_lock(&ipa_eth_devices_lock);

	list_for_each_entry(eth_dev, &ipa_eth_devices, device_list) {
		if (!ipa_eth_offload_pair_device(eth_dev))
			ipa_eth_device_refresh_sched(eth_dev);
	}

	mutex_unlock(&ipa_eth_devices_lock);

	return 0;
}
EXPORT_SYMBOL(ipa_eth_register_offload_driver);

/**
 * ipa_eth_unregister_offload_driver() - Unregister an offload driver
 * @nd: Offload driver to unregister
 */
void ipa_eth_unregister_offload_driver(struct ipa_eth_offload_driver *od)
{
	struct ipa_eth_device *eth_dev;

	/* Unregister offload driver first from ipa_eth_offload so that any
	 * new device registration will not be able to pair with the driver.
	 */
	ipa_eth_offload_unregister_driver(od);

	mutex_lock(&ipa_eth_devices_lock);

	list_for_each_entry(eth_dev, &ipa_eth_devices, device_list)
		if (eth_dev->od == od)
			ipa_eth_unpair_device(eth_dev);

	mutex_unlock(&ipa_eth_devices_lock);
}
EXPORT_SYMBOL(ipa_eth_unregister_offload_driver);

int ipa_eth_init(void)
{
	int rc;
	unsigned int wq_flags = WQ_UNBOUND | WQ_MEM_RECLAIM;

	/* Freeze the workqueue so that a refresh will not happen while the
	 * device is suspended as the suspend operation itself can generate
	 * Netlink events.
	 */
	wq_flags |= WQ_FREEZABLE;

	rc = ipa_eth_ipc_log_init();
	if (rc) {
		ipa_eth_err("Failed to initialize IPC logging");
		goto err_ipclog;
	}

	ipa_eth_dbg("Initializing IPA Ethernet Offload Sub-System");

	ipa_eth_wq = alloc_workqueue("ipa_eth", wq_flags, 0);
	if (!ipa_eth_wq) {
		ipa_eth_err("Failed to alloc workqueue");
		goto err_wq;
	}

	rc = ipa_eth_bus_modinit();
	if (rc) {
		ipa_eth_err("Failed to initialize bus");
		goto err_bus;
	}

	rc = ipa_eth_debugfs_init();
	if (rc) {
		ipa_eth_err("Failed to initialize debugfs");
		goto err_dbgfs;
	}

	rc = ipa3_uc_register_ready_cb(&uc_ready_cb);
	if (rc) {
		ipa_eth_err("Failed to register for uC ready cb");
		goto err_uc;
	}

	/* Register for IPA ready cb in the end since there is no
	 * mechanism to unregister it.
	 */
	rc = ipa_register_ipa_ready_cb(ipa_eth_ipa_ready_cb, NULL);
	if (rc == -EEXIST) {
		set_bit(IPA_ETH_ST_IPA_READY, &ipa_eth_state);
	} else if (rc) {
		ipa_eth_err("Failed to register for IPA ready cb");
		goto err_ipa;
	}

	set_bit(IPA_ETH_ST_READY, &ipa_eth_state);

	ipa_eth_log("Offload sub-system init is complete");

	ipa_eth_global_refresh_sched();

	return 0;

err_ipa:
	ipa3_uc_unregister_ready_cb(&uc_ready_cb);
err_uc:
	ipa_eth_debugfs_cleanup();
err_dbgfs:
	ipa_eth_bus_modexit();
err_bus:
	destroy_workqueue(ipa_eth_wq);
	ipa_eth_wq = NULL;
err_wq:
	ipa_eth_ipc_log_cleanup();
err_ipclog:
	return rc;
}

void ipa_eth_exit(void)
{
	ipa_eth_dbg("De-initializing IPA Ethernet Offload Sub-System");

	/* Clear READY bit so that all active offload paths can start
	 * deinitializing.
	 */
	clear_bit(IPA_ETH_ST_READY, &ipa_eth_state);

	/* IPA ready CB can not be unregistered. But since ipa_eth_exit() is
	 * only called when IPA driver itself is deinitialized, we do not
	 * expect the IPA ready CB to happen beyond this point.
	 */

	ipa3_uc_unregister_ready_cb(&uc_ready_cb);

	ipa_eth_debugfs_cleanup();

	/* Wait for all offload paths to deinit. Although the chances for any
	 * such path to exist is quite low when IPA is a platform device, we
	 * still need to make sure we wait here before destroying the work
	 * queue.
	 */
	ipa_eth_global_refresh_sync();

	ipa_eth_bus_modexit();

	destroy_workqueue(ipa_eth_wq);
	ipa_eth_wq = NULL;

	ipa_eth_ipc_log_cleanup();
}
