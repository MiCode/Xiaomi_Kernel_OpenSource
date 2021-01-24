/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _IPA_ETH_H_
#define _IPA_ETH_H_

#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/ipc_logging.h>

#include <linux/netdevice.h>
#include <linux/netdev_features.h>

#include <linux/ipa.h>
#include <linux/msm_ipa.h>
#include <linux/msm_gsi.h>

/**
 * API Version    Changes
 * ---------------------------------------------------------------------------
 *           1    - Initial version
 *           2    - API separation for use by network and offload drivers
 *                - New ipa_eth_net_*() APIs offer safer interface for offload
 *                  drivers to call into ipa_eth_net_ops
 *                - ipa_eth_net_ops.request_channel() to accept additional
 *                  memory allocation params, including custom memory allocator
 *                  defined via struct ipa_eth_dma_allocator interface
 *                - probe() and remove() offload bus ops are replaced by pair()
 *                  and unpair() callbacks respectively
 *           3    - Added .save_regs() callback for network and offload drivers
 *           4    - Added ipa_eth_device_notify() interface for client drivers
 *                  to notify of various device events.
 *           5    - Removed ipa_eth_{gsi,uc}_iommu_*{} APIs that were used for
 *                  mapping memory to GSI and IPA uC IOMMU CBs.
 *           6    - Added ipa_eth_ep_deinit()
 *           7    - ipa_eth_net_ops.receive_skb() now accepts in_napi parameter
 *           8    - Added IPA rx/tx intf properties to ipa_eth_device
 */

#define IPA_ETH_API_VER 8

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
 * @IPA_ETH_CH_DIR_RX: Traffic flowing from device to IPA
 * @IPA_ETH_CH_DIR_TX: Traffic flowing from IPA to device
 */
enum ipa_eth_channel_dir {
	IPA_ETH_CH_DIR_RX,
	IPA_ETH_CH_DIR_TX,
};

#define IPA_ETH_DIR_RX IPA_ETH_CH_DIR_RX
#define IPA_ETH_DIR_TX IPA_ETH_CH_DIR_TX

/**
 * enum ipa_eth_offload_state - Offload state of an ethernet device
 * @IPA_ETH_OF_ST_DEINITED: No offload path resources are allocated
 * @IPA_ETH_OF_ST_INITED: Offload path resources are allocated, but not started
 * @IPA_ETH_OF_ST_STARTED: Offload path is started and ready to handle traffic
 * @IPA_ETH_OF_ST_ERROR: One or more offload path components are in error state
 * @IPA_ETH_OF_ST_RECOVERY: Offload path is attempting to recover from error
 */
enum ipa_eth_offload_state {
	IPA_ETH_OF_ST_DEINITED = 0,
	IPA_ETH_OF_ST_INITED,
	IPA_ETH_OF_ST_STARTED,
	IPA_ETH_OF_ST_ERROR,
	IPA_ETH_OF_ST_RECOVERY,
	IPA_ETH_OF_ST_MAX,
};

/**
 * enum ipa_eth_offload_state - States of the network interface
 * @IPA_ETH_IF_ST_UP: Network interface is up in software/Linux
 * @IPA_ETH_IF_ST_LOWER_UP: Network interface PHY link is up / cable connected
 */
enum ipa_eth_interface_states {
	IPA_ETH_IF_ST_UP,
	IPA_ETH_IF_ST_LOWER_UP,
	IPA_ETH_IF_ST_MAX,
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
 * ipa_eth_mem_it_t() - Callback used by the ipa_eth_dma_allocator.walk() as it
 *                      iterates through each contiguous chunk of memory
 * @eth_dev: Device to which the memory belong
 * @cmem: Contigous mapping. If cmem->paddr is NULL, it could be determined from
 *        ipa_eth_dma_allocator.paddr(cmem->vaddr)
 * @arg: Private argument passed to ipa_eth_dma_allocator.walk() that can hold
 *       necessary context required by the iterator
 *
 * See &struct ipa_eth_dma_allocator for more details.
 */
typedef int (*ipa_eth_mem_it_t)(struct ipa_eth_device *eth_dev,
		const struct ipa_eth_resource *cmem, void *arg);

/**
 * struct ipa_eth_dma_allocator - Custom DMA memory allocator interface
 * @name: Name of the allocator
 */
struct ipa_eth_dma_allocator {
	const char *name;

	/**
	 * .paddr() - Convert a virtual address previously allocated by this
	 *            ipa_eth_dma_allocator, to physical address
	 * @eth_dev: Device which owns the memory
	 * @vaddr: Kernel mapped address of the resource
	 *
	 * Return: Physical address of @vaddr
	 */
	phys_addr_t (*paddr)(struct ipa_eth_device *eth_dev,
		const void *vaddr);

	/**
	 * .alloc() - Allocates DMA memory for a given device
	 * @eth_dev: Device which will own the memory. Note that @eth_dev->dev
	 *           should be a valid struct device pointer.
	 * @size: Minimum number of bytes to allocate
	 * @gfp: Kernel GFP_* flags to use, if the allocator supports it
	 * @mem: Memory info if the allocation was successful. @mem->paddr may
	 *       be NUll or not valid, use ipa_eth_dma_allocator.paddr() to get
	 *       the correct physical address for a page of virtual address.
	 *
	 * This API callback is typically called by the network driver to
	 * allocate memory for descriptor rings, buffers, etc. Any memory
	 * allocated by this callback should be traversable by the .remap()
	 * API callback implementation.
	 *
	 * Return: 0 on success, non-zero otherwise
	 */
	int (*alloc)(struct ipa_eth_device *eth_dev, size_t size, gfp_t gfp,
		struct ipa_eth_resource *mem);

	/**
	 * .free() - Free a memory previously allocated using .alloc() API
	 * @eth_dev: Device which owns the memory
	 * @mem: Memory info
	 *
	 * This API callback is typically called by the network driver to free
	 * memory resources associated with descriptor rings, buffers, etc. once
	 * their associated hardware queues are stopped.
	 */
	void (*free)(struct ipa_eth_device *eth_dev,
		struct ipa_eth_resource *mem);

	/**
	 * .walk() - Iterates over memory previously allocated by .alloc()
	 * @eth_dev: Device which owns the memory
	 * @mem: Allocated memory that need to be iterated over
	 * @it: Iterator that is invoked for each chunk (typically PAGE_SIZE)
	 *      of memory
	 * @arg: Private argument passed to @it for each invocation
	 *
	 * This API is expected to iterate over pieces of an allocated memory
	 * for typical purposes remapping to additional peripherals, and later
	 * unmapping from them. .walk() is responsible to invoke the iterator
	 * @it for each mappable region within an allocated space, which is
	 * typically page sized.
	 *
	 * Iteration is expected to stop either when the page walk completes or
	 * when any of the @it() invocations return failure. Iterator can add
	 * additional checks for memory alignments, addressability, etc. that
	 * can also fail. In any case, the API is expected to return the number
	 * of bytes (<= mem->size) from the given memory that was successfully
	 * iterated. Although the API may re-align memory regions while invoking
	 * the iterator, it should still return only the number of bytes of the
	 * original memory that was successfully iterated.
	 *
	 * .walk() API could be used for any purpose of iteration, not limited
	 * to memory mapping and unmapping. Implementation of the API should be
	 * flexible for generalized use-cases. Caller of this API should always
	 * check for the return value against @mem->size to make sure if the
	 * intended operation was successful, and explicitly revert any partial
	 * operation. Ex,
	 *
	 *   mapped_bytes = alctr->walk(dev, mem_region, iommu_mapper, map_ctx);
	 *   if (mapped_bytes != mem_region->size) {
	 *     struct ipa_eth_resource unmap_region = *mem_region;
	 *     unmap_region.size = mapped_bytes;
	 *     alctr->walk(dev, unmap_region, smmu_map, ctx);
	 *     return -ENOMEM;
	 *   }
	 *
	 * Return: the number of bytes in @mem that were successfully iterated
	 */
	size_t (*walk)(struct ipa_eth_device *eth_dev,
		const struct ipa_eth_resource *mem,
		ipa_eth_mem_it_t it, void *arg);
};

/**
 * enum ipa_eth_hw_type - Types of hardware used in an offload path
 * @IPA_ETH_HW_UC: IPA uC
 * @IPA_ETH_HW_GSI: GSI
 * @IPA_ETH_HW_IPA: IPA hardware/endpoint
 */
enum ipa_eth_hw_type {
	IPA_ETH_HW_UC,
	IPA_ETH_HW_GSI,
	IPA_ETH_HW_IPA,
	IPA_ETH_HW_MAX,
};

/**
 * struct ipa_eth_hw_map_param - Params for mapping memory to IPA hardware
 * @map: If true, perform mapping of memory to the given hardware
 * @sym: If true, performs symmetric mapping where IO virtual address (IOVA)
 *        used is the same as the one used on original device.
 * @read: Memory should be readable by hardware
 * @write: Memory should be writable by hardware
 */
struct ipa_eth_hw_map_param {
	bool map;
	bool sym;
	bool read;
	bool write;
};

/**
 * struct ipa_eth_desc_params - Params for allocating descriptor memory
 * @size: Size of each descriptor. This field is usually filled in by the
 *         network driver.
 * @count: Number of descriptors to be allocated
 * @allocator: DMA allocator to be used for IO memory allocation and mapping
 * @hw_map_params: Info on memory mapping requirements to IPA CBs
 */
struct ipa_eth_desc_params {
	size_t size;
	size_t count;
	struct ipa_eth_dma_allocator *allocator;
	struct ipa_eth_hw_map_param hw_map_params[IPA_ETH_HW_MAX];
};

/**
 * struct ipa_eth_buff_params - Params for allocating buffer memory
 * @size: Size of data buffer associated with a descriptor
 * @count: Number of buffers. This ield is usually filled in by the network
 *          driver and is typically the same value as descriptor count.
 * @allocator:  DMA allocator to be used for IO memory allocation and mapping
 * @maps: Info on memory mapping requirements to IPA CBs
 */
struct ipa_eth_buff_params {
	size_t size;
	size_t count;
	struct ipa_eth_dma_allocator *allocator;
	struct ipa_eth_hw_map_param hw_map_params[IPA_ETH_HW_MAX];
};

/**
 * struct ipa_eth_channel_mem_params - Params for various channel memory
 * @desc: Descriptor memory parameters
 * @buff: Buffer memory parameters
 */
struct ipa_eth_channel_mem_params {
	struct ipa_eth_desc_params desc;
	struct ipa_eth_buff_params buff;
};

/**
 * struct ipa_eth_channel_mem - Represents a piece of memory used by the
 *       network device channel for descriptrors, buffers, etc.
 * @mem_list_entry: list entry in either of ipa_eth_channel.desc_mem or
 *                   ipa_eth_channel.buff_mem
 * @mem: Memory mapping info as used by the network device/driver
 * @cb_mem: Memory mapping info as used by each of IPA context banks. When a
 *          mapping is present, cb_mem[cb_type].size would be non-zero.
 */
struct ipa_eth_channel_mem {
	struct list_head mem_list_entry;

	struct ipa_eth_resource mem;

/* private: for internal use by offload sub-system */
	struct ipa_eth_resource *cb_mem;
};

/**
 * struct ipa_eth_channel - Represents a network device channel
 * @channel_list: list entry in either of ipa_eth_device.rx_channels or
 *                ipa_eth_device.tx_channels
 * @nd_priv: Private field for use by network driver
 * @events: Events supported by the channel
 * @features: Features enabled in the channel
 * @direction: Channel direction
 * @mem_params: Channel memory params filled in collectively by Offload driver,
 *              Network driver and Offload sub-system
 * @queue: Network device queue/ring number
 * @desc_mem: Descriptor ring memory list
 * @buff_mem: Data buffer memory list
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
	struct list_head channel_list;

	/* fields managed by network driver */
	void *nd_priv;

	unsigned long events;
	unsigned long features;
	enum ipa_eth_channel_dir direction;
	struct ipa_eth_channel_mem_params mem_params;

	int queue;
	struct list_head desc_mem;
	struct list_head buff_mem;

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

#define IPA_ETH_CH_IS_RX(ch) ((ch)->direction == IPA_ETH_CH_DIR_RX)
#define IPA_ETH_CH_IS_TX(ch) ((ch)->direction == IPA_ETH_CH_DIR_TX)

/**
 * struct ipa_eth_device - Represents an ethernet device
 * @device_list: Entry in the global offload device list
 * @bus_device_list: Entry in the per-bus offload device list
 * @net_dev: Netdev registered by the network driver
 * @nd_priv: Private field for use by network driver
 * @od_priv: Private field for use by offload driver
 * @rx_channels: Rx channels allocated for the offload path
 * @tx_channels: Tx channels allocated for the offload path
 * @ipa_rx_intf: IPA properties Rx interface
 * @ipa_tx_intf: IPA properties Tx interface
 * @of_state: Offload state of the device
 * @dev: Pointer to struct device
 * @nd: IPA offload net driver associated with the device
 * @od: IPA offload driver that is managing the device
 * @netdevice_nb: Notifier block for receiving netdev events from the
 *                network device (to monitor link state changes)
 * @init: Allowed to initialize offload path for the device
 * @start: Allowed to start offload data path for the device
 * @start_on_wakeup: Allow start upon wake up by device
 * @start_on_resume: Allow start upon driver resume
 * @start_on_timeout: Timeout in milliseconds after which @start is enabled
 * @start_timer: Timer associated with @start_on_timer
 * @flags: Device flags
 * @if_state: Interface state - one or more bit numbers IPA_ETH_IF_ST_*
 * @pm_handle: IPA PM client handle for the device
 * @phdr_v4_handle: Partial header handle for IPv4
 * @phdr_v6_handle: Partial header handle for IPv6
 * @bus_priv: Private field for use by offload subsystem bus layer
 * @ipa_priv: Private field for use by offload subsystem
 * @debugfs: Debugfs root for the device
 * @refresh: Work struct used to perform device refresh
 */
struct ipa_eth_device {
	struct list_head device_list;
	struct list_head bus_device_list;

	/* fields managed by the network driver */
	struct net_device *net_dev;
	void *nd_priv;

	/* fields managed by offload driver */
	void *od_priv;

	/* fields managed by offload subsystem */
	struct list_head rx_channels;
	struct list_head tx_channels;

	struct ipa_rx_intf ipa_rx_intf;
	struct ipa_tx_intf ipa_tx_intf;

	enum ipa_eth_offload_state of_state;

	struct device *dev;
	struct ipa_eth_net_driver *nd;
	struct ipa_eth_offload_driver *od;

	struct notifier_block netdevice_nb;

	bool init;
	bool start;

	bool start_on_wakeup;
	bool start_on_resume;
	u32 start_on_timeout;
	struct timer_list start_timer;

	unsigned long flags;
	unsigned long if_state;

	u32 pm_handle;
	u32 phdr_v4_handle;
	u32 phdr_v6_handle;

	void *bus_priv;
	void *ipa_priv;
	struct dentry *debugfs;

	struct work_struct refresh;
};

/**
 * enum ipa_eth_device_event - Events related to device state
 * @IPA_ETH_DEV_RESET_PREPARE: Device is entering reset and is requesting
 *                             offload path to stop using the device
 * @IPA_ETH_DEV_RESET_COMPLETE: Device has completed resetting and is
 *                              requesting offload path to resume its operations
 * @IPA_ETH_DEV_ADD_MACSEC_IF: A MACSec interface is coming up
 * @IPA_ETH_DEV_DEL_MACSEC_IF: A MACSec interface is going down
 */
enum ipa_eth_device_event {
	IPA_ETH_DEV_RESET_PREPARE,
	IPA_ETH_DEV_RESET_COMPLETE,
	IPA_ETH_DEV_ADD_MACSEC_IF,
	IPA_ETH_DEV_DEL_MACSEC_IF,
	IPA_ETH_DEV_EVENT_COUNT,
};

int ipa_eth_device_notify(struct ipa_eth_device *eth_dev,
	enum ipa_eth_device_event event, void *data);

/**
 * struct ipa_eth_net_ops - Network device operations required for IPA offload
 */
struct ipa_eth_net_ops {
	/**
	 * .open_device() - Initialize the network device if needed and fill in
	 *                  @eth_dev->net_dev field
	 * @eth_dev: Device to initialize
	 *
	 * Some network perform complete device initialization only in driver
	 * .ndo_open(). Offload sub-system may try to use the network hardware
	 * for offload path even before .ndo_open() is called. .open_device() is
	 * expected to perform all device initialization that may be required
	 * to make the hardware functional for offload path, irrespective of
	 * whether .ndo_open() gets called.
	 *
	 * Once the device is initialized, .open_device() should initialize the
	 * @eth_dev->net_dev field with the driver struct net_device pointer
	 * before returning.
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
	 * .save_regs() - Save registers for debugging
	 * @eth_dev: Offloaded device
	 * @regs: if not NULL, write saved data address to the given pointer
	 * @size: if not NULL, write the size of saved data to the given pointer
	 *
	 * Return: 0 on success, errno otherwise.
	 */
	int (*save_regs)(struct ipa_eth_device *eth_dev,
		void **regs, size_t *size);
};

/**
 * struct ipa_eth_net_driver - Network driver to be registered with IPA offload
 *                             subsystem
 * @driver_list: Entry in the offload sub-system network driver list
 * @bus_driver_list: Entry in the bus specific driver list
 * @ops: Network device operations
 * @bus_priv: Bus private data
 * @name: Name of the network driver
 * @bus: Pointer to the bus object. Must use &pci_bus_type for PCI and
 *       &platform_bus_type for platform drivers. This property is used by the
 *       offload subsystem to deduce the network driver's original pci_driver
 *       or plaform_driver object from the @driver argument during driver
 *       registration.
 *       Beyond registration, the bus type is also used to deduce a pci_dev or
 *       platform_device object from a `struct device` pointer.
 * @driver: Pointer to the device_driver object embedded within a PCI/plaform
 *          driver object used by the network driver
 * @events: Events supported by the network device
 * @features: Capabilities of the network device
 * @debugfs: Debugfs directory for the device
 */
struct ipa_eth_net_driver {
	struct list_head driver_list;
	struct list_head bus_driver_list;

	struct ipa_eth_net_ops *ops;
	void *bus_priv;

	const char *name;
	struct bus_type *bus;
	struct device_driver *driver;

	unsigned long events;
	unsigned long features;

	struct dentry *debugfs;
};

int ipa_eth_register_net_driver(struct ipa_eth_net_driver *nd);
void ipa_eth_unregister_net_driver(struct ipa_eth_net_driver *nd);

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
	 * .pair() - Pair a new device, if compatible, with the offload driver
	 * @eth_dev: Device to pair with the offload driver
	 *
	 * This API is called by offload sub-system in order to pair an ethernet
	 * device with the offload driver. Typically this API is called soon
	 * after the device probe is completed and registered with the offload
	 * sub-system. When the API is called, offload driver is expected to
	 * check if the device is compatible with the driver and that the driver
	 * has enough offload resources for offloading the device to IPA.
	 *
	 * The API implementation can expect the eth_dev->dev to have been
	 * already initialized by the offload subsystem, from which a bus
	 * specific device pointer (pci_dev, platform_device, etc.) can be
	 * derived. The API is expected to perform at least the following:
	 *
	 * 1. Ensure the device is compatible with the offload driver. For
	 *    plaform drivers, the .compatible DT property could be used while
	 *    PCI IDs could be used for matching with a PCI device.
	 * 2. If multiple network devices of the same type can be managed by the
	 *    offload driver, it may attempt to pair @eth_dev with an available
	 *    resource set for a single instance.
	 *
	 * Return: 0 on success, negative errno otherwise
	 */
	int (*pair)(struct ipa_eth_device *eth_dev);

	/**
	 * .unpair() - Unpair a device from the offload driver
	 * @eth_dev: Device to unpair
	 *
	 * Unpairing the device from offload driver should make sure that all
	 * resources allocated for the device are freed and the data path is
	 * stopped and deinitialized, and device is readied for removal. The
	 * implementation should be able to handle plug-n-play devices by not
	 * assuming the device to be connected anymore to the bus/system when
	 * this API is called.
	 */
	void (*unpair)(struct ipa_eth_device *eth_dev);

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

	/**
	 * .save_regs() - Save registers for debugging
	 * @eth_dev: Offloaded device
	 * @regs: if not NULL, write saved data address to the given pointer
	 * @size: if not NULL, write the size of saved data to the given pointer
	 *
	 * Return: 0 on success, errno otherwise.
	 */
	int (*save_regs)(struct ipa_eth_device *eth_dev,
		void **regs, size_t *size);

	/**
	 * .prepare_reset() - Prepare offload path for netdev reset
	 * @eth_dev: Offloaded device
	 * @data: Private data the network driver has provided
	 *
	 * Return: 0 on success, errno otherwise.
	 */
	int (*prepare_reset)(struct ipa_eth_device *eth_dev, void *data);

	/**
	 * .complete_reset() - Netdev reset completed, offload path can resume
	 * @eth_dev: Offloaded device
	 * @data: Private data the network driver has provided
	 *
	 * Return: 0 on success, errno otherwise.
	 */
	int (*complete_reset)(struct ipa_eth_device *eth_dev, void *data);

};

/**
 * struct ipa_eth_offload_driver - Offload driver to be registered with the
 *                                 offload sub-system
 * @driver_list: Entry in the global offload driver list
 * @mutex: Mutex to protect offload driver struct
 * @name: Name of the offload driver
 * @bus: Supported network device bus type
 * @ops: Offload operations
 * @bus_ops: Bus level callbacks and operations for the offload driver
 * @debugfs: Debugfs directory for the offload driver
 */
struct ipa_eth_offload_driver {
	struct list_head driver_list;
	struct mutex mutex;

	const char *name;
	struct bus_type *bus;

	struct ipa_eth_offload_ops *ops;

	struct dentry *debugfs;
};

int ipa_eth_register_offload_driver(struct ipa_eth_offload_driver *od);
void ipa_eth_unregister_offload_driver(struct ipa_eth_offload_driver *od);

/* IPC logging interface */

#define ipa_eth_ipc_do_log(ipcbuf, fmt, args...) \
	do { \
		void *__buf = (ipcbuf); \
		ipc_log_string(__buf, " %s:%d " fmt "\n", \
			__func__, __LINE__, ## args); \
	} while (0)

#define ipa_eth_ipc_log(fmt, args...) \
	do { \
		void *ipa_eth_get_ipc_logbuf(void); \
		ipa_eth_ipc_do_log(ipa_eth_get_ipc_logbuf(), \
					fmt, ## args); \
	} while (0)

#define ipa_eth_ipc_dbg(fmt, args...) \
	do { \
		void *ipa_eth_get_ipc_logbuf_dbg(void); \
		ipa_eth_ipc_do_log(ipa_eth_get_ipc_logbuf_dbg(), \
					fmt, ## args); \
	} while (0)


/* New architecture prototypes */

typedef void (*ipa_eth_ready_cb)(void *user_data);
typedef u32 ipa_eth_hdl_t;

/**
 * struct ipa_eth_ready_cb - eth readiness parameters
 *
 * @notify: ipa_eth client ready callback notifier
 * @userdata: userdata for ipa_eth ready cb
 * @is_eth_ready: true if ipa_eth client is already ready
 */
struct ipa_eth_ready {
	ipa_eth_ready_cb notify;
	void *userdata;

	/* out params */
	bool is_eth_ready;
};

/**
 * enum ipa_eth_client_type - names for the various IPA
 * eth "clients".
 */
enum ipa_eth_client_type {
	IPA_ETH_CLIENT_AQC107,
	IPA_ETH_CLIENT_AQC113,
	IPA_ETH_CLIENT_RTK8111K,
	IPA_ETH_CLIENT_RTK8125B,
	IPA_ETH_CLIENT_NTN,
	IPA_ETH_CLIENT_EMAC,
	IPA_ETH_CLIENT_MAX,
};

/**
 * enum ipa_eth_pipe_traffic_type - traffic type for the various IPA
 * eth "pipes".
 */
enum ipa_eth_pipe_traffic_type {
	IPA_ETH_PIPE_BEST_EFFORT,
	IPA_ETH_PIPE_LOW_LATENCY,
	IPA_ETH_PIPE_TRAFFIC_TYPE_MAX,
};

/**
 * enum ipa_eth_pipe_direction - pipe direcitons for same
 * ethernet client.
 */
enum ipa_eth_pipe_direction {
	IPA_ETH_PIPE_DIR_TX,
	IPA_ETH_PIPE_DIR_RX,
	IPA_ETH_PIPE_DIR_MAX,
};

#define IPA_ETH_INST_ID_MAX (2)

/**
 * struct ipa_eth_aqc_setup_info - parameters for aqc ethernet
 * offloading
 *
 * @bar_addr: bar PA to access AQC register
 * @head_ptr_offs: head ptr offset
 * @aqc_ch: AQC ch number
 * @dest_tail_ptr_offs: tail ptr offset
 */
struct ipa_eth_aqc_setup_info {
	phys_addr_t bar_addr;
	phys_addr_t head_ptr_offs;
	u8 aqc_ch;
	phys_addr_t dest_tail_ptr_offs;
};


/**
 * struct ipa_eth_realtek_setup_info - parameters for realtek ethernet
 * offloading
 *
 * @bar_addr: bar PA to access RTK register
 * @bar_size: bar region size
 * @queue_number: Which RTK queue to check the status on
 * @dest_tail_ptr_offs: tail ptr offset
 */
struct ipa_eth_realtek_setup_info {
	phys_addr_t bar_addr;
	u32 bar_size;
	u8 queue_number;
	phys_addr_t dest_tail_ptr_offs;
};

/**
 * struct ipa_eth_buff_smmu_map -  IPA iova->pa SMMU mapping
 * @iova: virtual address of the data buffer
 * @pa: physical address of the data buffer
 */
struct ipa_eth_buff_smmu_map {
	dma_addr_t iova;
	phys_addr_t pa;
};

/**
 * struct  ipa_eth_pipe_setup_info - info needed for IPA setups
 * @is_transfer_ring_valid: if transfer ring is needed
 * @transfer_ring_base:  the base of the transfer ring
 * @transfer_ring_sgt: sgtable of transfer ring
 * @transfer_ring_size:  size of the transfer ring
 * @is_buffer_pool_valid: if buffer pool is needed
 * @buffer_pool_base_addr:  base of buffer pool address
 * @buffer_pool_base_sgt:  sgtable of buffer pool
 * @data_buff_list_size: number of buffers
 * @data_buff_list: array of data buffer list
 * @fix_buffer_size: buffer size
 * @notify:	callback for exception/embedded packets
 * @priv: priv for exception callback
 * @client_info: vendor specific pipe setup info
 * @db_pa: doorbell physical address
 * @db_val: doorbell value ethernet HW need to ring
 */
struct ipa_eth_pipe_setup_info {
	/* transfer ring info */
	bool is_transfer_ring_valid;
	dma_addr_t  transfer_ring_base;
	struct sg_table *transfer_ring_sgt;
	u32 transfer_ring_size;

	/* buffer pool info */
	bool is_buffer_pool_valid;
	dma_addr_t buffer_pool_base_addr;
	struct sg_table *buffer_pool_base_sgt;

	/* buffer info */
	u32 data_buff_list_size;
	struct ipa_eth_buff_smmu_map *data_buff_list;
	u32 fix_buffer_size;

	/* client notify cb */
	ipa_notify_cb notify;
	void *priv;

	/* vendor specific info */
	union {
		struct ipa_eth_aqc_setup_info aqc;
		struct ipa_eth_realtek_setup_info rtk;
	} client_info;

	/* output params */
	phys_addr_t db_pa;
	u32 db_val;
};

/**
 * struct  ipa_eth_client_pipe_info - ETH pipe/gsi related configuration
 * @link: link of ep for different client function on same ethernet HW
 * @dir: TX or RX direction
 * @info: tx/rx pipe setup info
 * @client_info: client the pipe belongs to
 * @pipe_hdl: output params, pipe handle
 */
struct ipa_eth_client_pipe_info {
	struct list_head link;
	enum ipa_eth_pipe_direction dir;
	struct ipa_eth_pipe_setup_info info;
	struct ipa_eth_client *client_info;

	/* output params */
	ipa_eth_hdl_t pipe_hdl;
};

/**
 * struct  ipa_eth_client - client info per traffic type
 * provided by offload client
 * @client_type: ethernet client type
 * @inst_id: instance id for dual NIC support
 * @traffic_type: traffic type
 * @pipe_list: list of pipes with same traffic type
 * @priv: private data for client
 */
struct ipa_eth_client {
	/* vendor driver */
	enum ipa_eth_client_type client_type;
	u8 inst_id;

	/* traffic type */
	enum ipa_eth_pipe_traffic_type traffic_type;
	struct list_head pipe_list;

	/* client specific priv data*/
	void *priv;
};

/**
 * struct  ipa_eth_perf_profile - To set BandWidth profile
 *
 * @max_supported_bw_mbps: maximum bandwidth needed (in Mbps)
 */
struct ipa_eth_perf_profile {
	u32 max_supported_bw_mbps;
};

/**
 * struct ipa_eth_hdr_info - Header to install on IPA HW
 *
 * @hdr: header to install on IPA HW
 * @hdr_len: length of header
 * @dst_mac_addr_offset: destination mac address offset
 * @hdr_type: layer two header type
 */
struct ipa_eth_hdr_info {
	u8 *hdr;
	u8 hdr_len;
	u8 dst_mac_addr_offset;
	enum ipa_hdr_l2_type hdr_type;
};

/**
 * struct ipa_eth_intf_info - parameters for ipa offload
 *	interface registration
 *
 * @netdev_name: network interface name
 * @hdr: hdr for ipv4/ipv6
 * @pipe_hdl_list_size: number of pipes prop needed for this interface
 * @pipe_hdl_list: array of pipes used for this interface
 */
struct ipa_eth_intf_info {
	const char *netdev_name;
	struct ipa_eth_hdr_info hdr[IPA_IP_MAX];

	/* tx/rx pipes for same netdev */
	int pipe_hdl_list_size;
	ipa_eth_hdl_t *pipe_hdl_list;
};

int ipa_eth_register_ready_cb(struct ipa_eth_ready *ready_info);
int ipa_eth_unregister_ready_cb(struct ipa_eth_ready *ready_info);
int ipa_eth_client_conn_pipes(struct ipa_eth_client *client);
int ipa_eth_client_disconn_pipes(struct ipa_eth_client *client);
int ipa_eth_client_reg_intf(struct ipa_eth_intf_info *intf);
int ipa_eth_client_unreg_intf(struct ipa_eth_intf_info *intf);
int ipa_eth_client_set_perf_profile(struct ipa_eth_client *client,
	struct ipa_eth_perf_profile *profile);
int ipa_eth_client_conn_evt(struct ipa_ecm_msg *msg);
int ipa_eth_client_disconn_evt(struct ipa_ecm_msg *msg);

#endif // _IPA_ETH_H_
