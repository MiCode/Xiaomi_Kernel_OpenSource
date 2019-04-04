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

#ifndef _IPA_ETH_H_
#define _IPA_ETH_H_

#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>

#include <linux/netdevice.h>
#include <linux/netdev_features.h>

#include <linux/msm_ipa.h>
#include <linux/msm_gsi.h>

/**
 * enum ipa_eth_dev_features - Features supported by an ethernet device or
 *                             driver that may be requested via offload APIs
 * @IPA_ETH_DEV_F_L2_CSUM_BIT: Ethernet L2 checksum offload
 * @IPA_ETH_DEV_F_L3_CSUM_BIT: Ethernet L2 checksum offload
 * @IPA_ETH_DEV_F_TCP_CSUM_BIT: TCP checksum offload
 * @IPA_ETH_DEV_F_UDP_CSUM_BIT: UDP checksum offload
 * @IPA_ETH_DEV_F_LSO_BIT: Large Send Offload / TCP Segmentation Offload
 * @IPA_ETH_DEV_F_LRO_BIT: Large Receive Offload / Receive Side Coalescing
 * @IPA_ETH_DEV_F_VLAN_BIT: VLAN Offload
 * @IPA_ETH_DEV_F_MODC_BIT: Counter based event moderation
 * @IPA_ETH_DEV_F_MODT_BIT: Timer based event moderation
 *
 * Ethernet hardware features represented as bit numbers below are used in
 * IPA_ETH_DEV_F_* feature flags that are to be used in offload APIs.
 */
enum ipa_eth_dev_features {
	IPA_ETH_DEV_F_L2_CSUM_BIT,
	IPA_ETH_DEV_F_L3_CSUM_BIT,
	IPA_ETH_DEV_F_TCP_CSUM_BIT,
	IPA_ETH_DEV_F_UDP_CSUM_BIT,
	IPA_ETH_DEV_F_LSO_BIT,
	IPA_ETH_DEV_F_LRO_BIT,
	IPA_ETH_DEV_F_VLAN_BIT,
	IPA_ETH_DEV_F_MODC_BIT,
	IPA_ETH_DEV_F_MODT_BIT,
};

#define ipa_eth_dev_f(f) BIT(IPA_ETH_DEV_F_##f##_BIT)

#define IPA_ETH_DEV_F_L2_CSUM  ipa_eth_dev_f(L2_CSUM)
#define IPA_ETH_DEV_F_L3_CSUM  ipa_eth_dev_f(L3_CSUM)
#define IPA_ETH_DEV_F_TCP_CSUM ipa_eth_dev_f(TCP_CSUM)
#define IPA_ETH_DEV_F_UDP_CSUM ipa_eth_dev_f(UDP_CSUM)
#define IPA_ETH_DEV_F_LSO      ipa_eth_dev_f(LSO)
#define IPA_ETH_DEV_F_LRO      ipa_eth_dev_f(LRO)
#define IPA_ETH_DEV_F_VLAN     ipa_eth_dev_f(VLAN)
#define IPA_ETH_DEV_F_MODC     ipa_eth_dev_f(MODC)
#define IPA_ETH_DEV_F_MODT     ipa_eth_dev_f(MODT)

/**
 * enum ipa_eth_dev_events - Events supported by an ethernet device that may be
 *                           requested via offload APIs
 * @IPA_ETH_DEV_EV_RX_INT_BIT: Rx interrupt
 * @IPA_ETH_DEV_EV_TX_INT_BIT: Tx interrupt
 * @IPA_ETH_DEV_EV_RX_PTR_BIT: Rx (head) pointer write-back
 * @IPA_ETH_DEV_EV_TX_PTR_BIT: Tx (head) pointer write-back
 */
enum ipa_eth_dev_events {
	IPA_ETH_DEV_EV_RX_INT_BIT,
	IPA_ETH_DEV_EV_TX_INT_BIT,
	IPA_ETH_DEV_EV_RX_PTR_BIT,
	IPA_ETH_DEV_EV_TX_PTR_BIT
};

#define ipa_eth_dev_ev(ev) BIT(IPA_ETH_DEV_EV_##ev##_BIT)

#define IPA_ETH_DEV_EV_RX_INT ipa_eth_dev_ev(RX_INT)
#define IPA_ETH_DEV_EV_TX_INT ipa_eth_dev_ev(TX_INT)
#define IPA_ETH_DEV_EV_RX_PTR ipa_eth_dev_ev(RX_PTR)
#define IPA_ETH_DEV_EV_TX_PTR ipa_eth_dev_ev(TX_PTR)

/**
 * enum ipa_eth_channel_dir - Direction of a ring / channel
 * @IPA_ETH_DIR_RX: Traffic flowing from device to IPA
 * @IPA_ETH_DIR_TX: Traffic flowing from IPA to device
 * @IPA_ETH_DIR_BI: Bi-direction traffic flow
 */
enum ipa_eth_channel_dir {
	IPA_ETH_DIR_RX,
	IPA_ETH_DIR_TX,
	IPA_ETH_DIR_BI
};

/**
 * enum ipa_eth_state - Offload state of an ethernet device
 * @IPA_ETH_ST_DEINITED: No offload path resources are allocated
 * @IPA_ETH_ST_INITED: Offload path resources are allocated, but not started
 * @IPA_ETH_ST_STARTED: Offload path is started and ready to handle traffic
 * @IPA_ETH_ST_ERROR: One or more offload path components are in error state
 * @IPA_ETH_ST_RECOVERY: Offload path is attempting to recover from error
 */
enum ipa_eth_state {
	IPA_ETH_ST_DEINITED = 0,
	IPA_ETH_ST_INITED,
	IPA_ETH_ST_STARTED,
	IPA_ETH_ST_ERROR,
	IPA_ETH_ST_RECOVERY,
};

struct ipa_eth_device;
struct ipa_eth_net_driver;

/**
 * struct ipa_eth_resource - Memory based resource info
 * @size: Size of the memory accessible
 * @vaddr: Kernel mapped address of the resource
 * @daddr: DMA address of the resource
 * @paddr: Physical address of the resource
 */
struct ipa_eth_resource {
	size_t size;

	void *vaddr;
	dma_addr_t daddr;
	phys_addr_t paddr;

};

/**
 * struct ipa_eth_channel - Represents a network device channel
 * @nd_priv: Private field for use by network driver
 * @events: Events supported by the channel
 * @features: Features enabled in the channel
 * @direction: Channel direction
 * @queue: Network device queue/ring number
 * @desc_mem: Descriptor ring memory
 * @buff_mem: Buffer memory pointed to by descriptors
 * @od_priv: Private field for use by offload driver
 * @eth_dev: Associated ipa_eth_device
 * @ipa_client: IPA client type enum to be used for the channel
 * @ipa_ep_num: IPA endpoint number configured for the client type
 * @process_skb: Callback to be called for processing IPA exception
 *               packets received from the IPA endpoint
 * @ipa_priv: Private field to be used by offload subsystem
 * @exception_total: Total number of exception packets received
 * @exception_drops: Total number of dropped exception packets
 * @exception_loopback: Total number of exception packets looped back
 *                      into IPA
 */
struct ipa_eth_channel {
	/* fields managed by network driver */
	void *nd_priv;

	unsigned long events;
	unsigned long features;
	enum ipa_eth_channel_dir direction;

	int queue;
	struct ipa_eth_resource desc_mem;
	struct ipa_eth_resource buff_mem;

	/* fields managed by offload driver */
	void *od_priv;
	struct ipa_eth_device *eth_dev;

	enum ipa_client_type ipa_client;
	int ipa_ep_num;

	int (*process_skb)(struct ipa_eth_channel *ch, struct sk_buff *skb);

	/* fields managed by offload subsystem */
	void *ipa_priv;

	u64 exception_total;
	u64 exception_drops;
	u64 exception_loopback;
};

#define IPA_ETH_CH_IS_RX(ch) ((ch)->direction != IPA_ETH_DIR_TX)
#define IPA_ETH_CH_IS_TX(ch) ((ch)->direction != IPA_ETH_DIR_RX)

/**
 * struct ipa_eth_device - Represents an ethernet device
 * @net_dev: Netdev registered by the network driver
 * @nd_priv: Private field for use by network driver
 * @ch_rx: Rx channel allocated for the offload path
 * @ch_tx: Tx channel allocated for the offload path
 * @od_priv: Private field for use by offload driver
 * @device_list: Entry in the global offload device list
 * @bus_device_list: Entry in the per-bus offload device list
 * @state: Offload state of the device
 * @dev: Pointer to struct device
 * @nd: IPA offload net driver associated with the device
 * @od: IPA offload driver that is managing the device
 * @netdevice_nb: Notifier block for receiving netdev events from the
 *                network device (to monitor link state changes)
 * @init: Allowed to initialize offload path for the device
 * @start: Allowed to start offload data path for the device
 * @link_up: Carrier is detected by the PHY (link is active)
 * @pm_handle: IPA PM client handle for the device
 * @bus_priv: Private field for use by offload subsystem bus layer
 * @ipa_priv: Private field for use by offload subsystem
 * @debugfs: Debugfs root for the device
 */
struct ipa_eth_device {
	/* fields managed by the network driver */
	struct net_device *net_dev;
	void *nd_priv;

	/* fields managed by offload driver */
	struct ipa_eth_channel *ch_rx;
	struct ipa_eth_channel *ch_tx;
	void *od_priv;

	/* fields managed by offload subsystem */
	struct list_head device_list;
	struct list_head bus_device_list;

	enum ipa_eth_state state;

	struct device *dev;
	struct ipa_eth_net_driver *nd;
	struct ipa_eth_offload_driver *od;

	struct notifier_block netdevice_nb;

	bool init;
	bool start;
	bool link_up;

	u32 pm_handle;

	void *bus_priv;
	void *ipa_priv;
	struct dentry *debugfs;
};

/**
 * struct ipa_eth_net_ops - Network device operations required for IPA offload
 */
struct ipa_eth_net_ops {
	/**
	 * .open_device() - Initialize the network device if needed before
	 *                  channels and events are configured.
	 * @eth_dev: Device to initialize
	 *
	 * Typically this is API is functionally equivalent to .ndo_open()
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*open_device)(struct ipa_eth_device *eth_dev);

	/**
	 * .close_device() Deinitialize the device if required
	 * @eth_dev: Device to deinitialize
	 *
	 * This is called when the offload data path is deinitialized and the
	 * offload subsystem do not see any offload drivers registered to
	 * handle the device anymore. Typically the device is maintained in
	 * the initialized state until both Linux and IPA data paths are
	 * deinitialized.
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	void (*close_device)(struct ipa_eth_device *eth_dev);

	/**
	 * .request_channel() - Allocate a channel/ring for IPA offload data
	 *                      path to use
	 * @eth_dev: Device from which to allocate channel
	 * @dir: Requested channel direction
	 * @events: Device events requested for the channel. Value is zero or
	 *            more IPA_ETH_DEV_EV_* flags.
	 * @features: Device featured requested for the channel. Value is zero
	 *            or more IPA_ETH_DEV_F_* feature flags.
	 *
	 * Arguments @dir, @features and @events are used to inform the network
	 * driver about the capabilities of the offload subsystem/driver. The
	 * API implementation may choose to allocate a channel with only a
	 * subset of capablities enabled. Caller of this API need to check the
	 * corresponding values in returned the channel and proceed only if the
	 * allocated capability set is acceptable for data path operation.
	 *
	 * The allocated channel is expected to be in disabled state with no
	 * events allocated or enabled.
	 *
	 * Return: Channel object pointer, or NULL if the channel allocation
	 *         failed
	 */
	struct ipa_eth_channel * (*request_channel)(
		struct ipa_eth_device *eth_dev, enum ipa_eth_channel_dir dir,
		unsigned long events, unsigned long features);

	/**
	 * .release_channel() - Free a channel/ring previously allocated using
	 *                      .request_channel()
	 * @ch: Channel to be freed
	 */
	void (*release_channel)(struct ipa_eth_channel *ch);

	/**
	 * .enable_channel() - Enable a channel, allowing data to flow
	 *
	 * @ch: Channel to be enabled
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*enable_channel)(struct ipa_eth_channel *ch);

	/**
	 * .disable_channel() - Disable a channel, stopping the data flow
	 *
	 * @ch: Channel to be disabled
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*disable_channel)(struct ipa_eth_channel *ch);

	/**
	 * .request_event() - Allocate an event for a channel
	 *
	 * @ch: Channel for which event need to be allocated
	 * @event: Event to be allocated
	 * @addr: Address to which the event need to be reported
	 * @data: Data value to be associated with the event
	 *
	 * The allocated event is expected to be unmoderated.
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*request_event)(struct ipa_eth_channel *ch, unsigned long event,
		phys_addr_t addr, u64 data);

	/**
	 * .release_event() - Deallocate a channel event
	 *
	 * @ch: Channel for which event need to be deallocated
	 * @event: Event to be deallocated
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	void (*release_event)(struct ipa_eth_channel *ch, unsigned long event);

	/**
	 * .enable_event() - Enable a channel event
	 *
	 * @ch: Channel for which event need to be enabled
	 * @event: Event to be enabled
	 *
	 * This API is called when IPA or GIC is ready to receive events from
	 * the network device.
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*enable_event)(struct ipa_eth_channel *ch, unsigned long event);

	/**
	 * .disable_event() - Disable a channel event
	 *
	 * @ch: Channel for which event need to be disabled
	 * @event: Event to be disabled
	 *
	 * Once this API is called, events may no more be reported to IPA/GIC
	 * although they may still be queued in the device for later delivery.
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*disable_event)(struct ipa_eth_channel *ch, unsigned long event);

	/**
	 * .moderate_event() - Moderate a channel event
	 *
	 * @ch: Channel for which event need to be disabled
	 * @event: Event to be disabled
	 * @min_count: Min threshold for counter based moderation
	 * @max_count: Max threshold for counter based moderation
	 * @min_usecs: Min microseconds for timer based moderation
	 * @max_usecs: Max microseconds for timer based moderation
	 *
	 * This API enables event moderation when supported by the device. A
	 * value of 0 in either of the @max_* arguments would disable moderation
	 * of that specific type. If both types of moderation are enabled, the
	 * event is triggered with either of them expires.
	 *
	 * It is expected from the device/driver to make sure there is always a
	 * default min_X value for each moderation type such that there would
	 * be no data stalls or queue overflow when a moderation target is not
	 * reached.
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*moderate_event)(struct ipa_eth_channel *ch, unsigned long event,
		u64 min_count, u64 max_count,
		u64 min_usecs, u64 max_usecs);

	/**
	 * .receive_skb() - Receive an skb from IPA and push it to Linux network
	 *                  stack
	 * @eth_dev: Device to which the skb need to belong
	 * @skb: Skb to be provided to Linux network stack
	 *
	 * When a network packet received by the IPA connected device queue can
	 * not be routed within IPA, it will be sent to Linux as an exception
	 * skb. Offload subsystem receives such packets and forwards to this API
	 * to be provided to Linux network stack to perform the necessary packet
	 * routing/filtering in the software path.
	 *
	 * Network packets received by this API is expected to emerge from the
	 * device's Linux network interface as it would have, had the packet
	 * arrived directly to the Linux connected queues.
	 *
	 * Return: 0 on success, negative errno otherwise. On error, skb is NOT
	 * expected to have been freed.
	 */
	int (*receive_skb)(struct ipa_eth_device *eth_dev,
		struct sk_buff *skb);

	/**
	 * .transmit_skb() - Transmit an skb given IPA
	 * @eth_dev: Device through which the packet need to be transmitted
	 * @skb: Skb to be transmitted
	 *
	 * Return: 0 on success, negative errno otherwise. On error, skb is NOT
	 * expected to have been freed.
	 */
	int (*transmit_skb)(struct ipa_eth_device *eth_dev,
		struct sk_buff *skb);
};

/**
 * struct ipa_eth_net_driver - Network driver to be registered with IPA offload
 *                             subsystem
 * @name: Name of the network driver
 * @driver: Pointer to the device_driver object embedded within a PCI/plaform
 *          driver object used by the network driver
 * @events: Events supported by the network device
 * @features: Capabilities of the network device
 * @bus: Pointer to the bus object. Must use &pci_bus_type for PCI and
 *       &platform_bus_type for platform drivers. This property is used by the
 *       offload subsystem to deduce the network driver's original pci_driver
 *       or plaform_driver object from the @driver argument during driver
 *       registration.
 *       Beyond registration, the bus type is also used to deduce a pci_dev or
 *       platform_device object from a `struct device` pointer.
 * @ops: Network device operations
 * @debugfs: Debugfs directory for the device
 */
struct ipa_eth_net_driver {
	const char *name;
	struct device_driver *driver;

	unsigned long events;
	unsigned long features;

	struct bus_type *bus;
	struct ipa_eth_net_ops *ops;

	struct dentry *debugfs;
};

int ipa_eth_register_net_driver(struct ipa_eth_net_driver *nd);
void ipa_eth_unregister_net_driver(struct ipa_eth_net_driver *nd);

/**
 * struct ipa_eth_bus_ops - Offload driver callbacks for bus operations
 *
 * These APIS are implemented by the offload driver for receiving bus level
 * notifications like probe, remove, suspend, resume, etc.
 */
struct ipa_eth_bus_ops {
	/**
	 * .probe() - Probe new device for offload ability
	 * @eth_dev: Device being probed
	 *
	 * This API is implemented by the offload driver and called immediately
	 * after the network driver (PCI/plaform) probe. Offload driver is
	 * expected to check if the device is compatible with the driver and
	 * that the driver has enough offload resources for offloading the
	 * device to IPA.
	 *
	 * The API implementation can expect the eth_dev->dev and eth_dev->nd
	 * to have been already initialized by the offload subsystem. The API
	 * is expected to perform at least the following:
	 *
	 * 1. Ensure the device is compatible with the offload driver. For
	 *    plaform drivers, the .compatible DT property could be used while
	 *    PCI IDs could be used for matching with a PCI device.
	 * 2. If multiple network devices of the same type can be managed by the
	 *    offload driver, it may attempt to pair @eth_dev with an available
	 *    resource set for a single instance.
	 * 3. Initialize the network device, typically by calling open_device()
	 *    callback provided by the network driver.
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*probe)(struct ipa_eth_device *eth_dev);

	/**
	 * .remove() - Handle device removal
	 * @eth_dev: Device being removed
	 *
	 * The API implementation should assume the device to be no more
	 * physically connected.
	 */
	void (*remove)(struct ipa_eth_device *eth_dev);

	/* dev_pm_ops go here */
};

/**
 * struct ipa_eth_offload_link_stats - Stats for each link within an
 *                                     offload data path
 * @valid: Stats are not available for the link
 * @events: Number of events reported to the link
 * @frames: Number of frames received by the link
 * @packets: Number of packets received by the link
 * @octets: Number of bytes received by the link
 */
struct ipa_eth_offload_link_stats {
	bool valid;
	u64 events;
	u64 frames;
	u64 packets;
	u64 octets;
};

struct ipa_eth_offload_ch_stats {
	struct ipa_eth_offload_link_stats ndev;
	struct ipa_eth_offload_link_stats host;
	struct ipa_eth_offload_link_stats uc;
	struct ipa_eth_offload_link_stats gsi;
	struct ipa_eth_offload_link_stats ipa;
};

struct ipa_eth_offload_stats {
	struct ipa_eth_offload_ch_stats rx;
	struct ipa_eth_offload_ch_stats tx;
};

/**
 * struct ipa_eth_offload_ops - Offload operations provided by an offload driver
 */
struct ipa_eth_offload_ops {
	/**
	 * .init_tx() - Initialize offload path in Tx direction
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*init_tx)(struct ipa_eth_device *eth_dev);

	/**
	 * .start_tx() - Start offload path in Tx direction
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*start_tx)(struct ipa_eth_device *eth_dev);

	/**
	 * .stop_tx() - Stop offload path in Tx direction
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*stop_tx)(struct ipa_eth_device *eth_dev);

	/**
	 * .deinit_tx() - Deinitialize offload path in Tx direction
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*deinit_tx)(struct ipa_eth_device *eth_dev);

	/**
	 * .init_rx() - Initialize offload path in Rx direction
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*init_rx)(struct ipa_eth_device *eth_dev);

	/**
	 * .start_rx() - Start offload path in Rx direction
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*start_rx)(struct ipa_eth_device *eth_dev);

	/**
	 * .stop_rx() - Stop offload path in Rx direction
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*stop_rx)(struct ipa_eth_device *eth_dev);

	/**
	 * .deinit_rx() - Deinitialize offload path in Rx direction
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*deinit_rx)(struct ipa_eth_device *eth_dev);

	/**
	 * .get_stats() - Get offload statistics
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*get_stats)(struct ipa_eth_device *eth_dev,
		struct ipa_eth_offload_stats *stats);

	/**
	 * .clear_stats() - Clear offload statistics
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*clear_stats)(struct ipa_eth_device *eth_dev);
};

/**
 * struct ipa_eth_offload_driver - Offload driver to be registered with the
 *                                 offload sub-system
 * @driver_list: Entry in the global offload driver list
 * @name: Name of the offload driver
 * @bus: Supported network device bus type
 * @ops: Offload operations
 * @bus_ops: Bus level callbacks and operations for the offload driver
 * @debugfs: Debugfs directory for the offload driver
 */
struct ipa_eth_offload_driver {
	struct list_head driver_list;

	const char *name;
	struct bus_type *bus;

	struct ipa_eth_offload_ops *ops;
	struct ipa_eth_bus_ops *bus_ops;

	struct dentry *debugfs;
};

int ipa_eth_register_offload_driver(struct ipa_eth_offload_driver *od);
void ipa_eth_unregister_offload_driver(struct ipa_eth_offload_driver *od);

int ipa_eth_ep_init(struct ipa_eth_channel *ch);
int ipa_eth_ep_start(struct ipa_eth_channel *ch);
int ipa_eth_ep_stop(struct ipa_eth_channel *ch);

int ipa_eth_gsi_alloc(struct ipa_eth_channel *ch,
	struct gsi_evt_ring_props *gsi_ev_props,
	union gsi_evt_scratch *gsi_ev_scratch,
	phys_addr_t *gsi_ev_db,
	struct gsi_chan_props *gsi_ch_props,
	union gsi_channel_scratch *gsi_ch_scratch,
	phys_addr_t *gsi_ch_db);
int ipa_eth_gsi_dealloc(struct ipa_eth_channel *ch);
int ipa_eth_gsi_ring_evtring(struct ipa_eth_channel *ch, u64 value);
int ipa_eth_gsi_ring_channel(struct ipa_eth_channel *ch, u64 value);
int ipa_eth_gsi_start(struct ipa_eth_channel *ch);
int ipa_eth_gsi_stop(struct ipa_eth_channel *ch);

int ipa_eth_gsi_iommu_pamap(dma_addr_t daddr, phys_addr_t paddr,
	size_t size, int prot, bool split);
int ipa_eth_gsi_iommu_vamap(dma_addr_t daddr, void *vaddr,
	size_t size, int prot, bool split);
int ipa_eth_gsi_iommu_unmap(dma_addr_t daddr, size_t size, bool split);

/* IPA uC interface for ethernet devices */

enum ipa_eth_uc_op {
	IPA_ETH_UC_OP_NOP         = 0,
	IPA_ETH_UC_OP_CH_SETUP    = 1,
	IPA_ETH_UC_OP_CH_TEARDOWN = 2,
	IPA_ETH_UC_OP_PER_INIT    = 3,
	IPA_ETH_UC_OP_PER_DEINIT  = 4,
	IPA_ETH_UC_OP_MAX,
};

enum ipa_eth_uc_resp {
	IPA_ETH_UC_RSP_SUCCESS                     = 0,
	IPA_ETH_UC_RSP_MAX_TX_CHANNELS             = 1,
	IPA_ETH_UC_RSP_TX_RING_OVERRUN_POSSIBILITY = 2,
	IPA_ETH_UC_RSP_TX_RING_SET_UP_FAILURE      = 3,
	IPA_ETH_UC_RSP_TX_RING_PARAMS_UNALIGNED    = 4,
	IPA_ETH_UC_RSP_UNKNOWN_TX_CHANNEL          = 5,
	IPA_ETH_UC_RSP_TX_INVALID_FSM_TRANSITION   = 6,
	IPA_ETH_UC_RSP_TX_FSM_TRANSITION_ERROR     = 7,
	IPA_ETH_UC_RSP_MAX_RX_CHANNELS             = 8,
	IPA_ETH_UC_RSP_RX_RING_PARAMS_UNALIGNED    = 9,
	IPA_ETH_UC_RSP_RX_RING_SET_UP_FAILURE      = 10,
	IPA_ETH_UC_RSP_UNKNOWN_RX_CHANNEL          = 11,
	IPA_ETH_UC_RSP_RX_INVALID_FSM_TRANSITION   = 12,
	IPA_ETH_UC_RSP_RX_FSM_TRANSITION_ERROR     = 13,
	IPA_ETH_UC_RSP_RX_RING_OVERRUN_POSSIBILITY = 14,
};

int ipa_eth_uc_send_cmd(enum ipa_eth_uc_op op, u32 protocol,
	const void *prot_data, size_t datasz);

int ipa_eth_uc_iommu_pamap(dma_addr_t daddr, phys_addr_t paddr,
	size_t size, int prot, bool split);
int ipa_eth_uc_iommu_vamap(dma_addr_t daddr, void *vaddr,
	size_t size, int prot, bool split);
int ipa_eth_uc_iommu_unmap(dma_addr_t daddr, size_t size, bool split);

#endif // _IPA_ETH_H_
