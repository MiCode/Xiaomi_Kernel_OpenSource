/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
#ifndef _MHI_H_
#define _MHI_H_

struct mhi_chan;
struct mhi_event;
struct mhi_ctxt;
struct mhi_cmd;
struct image_info;
struct bhi_vec_entry;
struct mhi_timesync;
struct mhi_buf_info;
struct mhi_sfr_info;

#define REG_WRITE_QUEUE_LEN 1024

/**
 * enum MHI_CB - MHI callback
 * @MHI_CB_IDLE: MHI entered idle state
 * @MHI_CB_PENDING_DATA: New data available for client to process
 * @MHI_CB_DTR_SIGNAL: DTR signaling update
 * @MHI_CB_LPM_ENTER: MHI host entered low power mode
 * @MHI_CB_LPM_EXIT: MHI host about to exit low power mode
 * @MHI_CB_EE_RDDM: MHI device entered RDDM execution enviornment
 * @MHI_CB_EE_MISSION_MODE: MHI device entered Mission Mode ee
 * @MHI_CB_SYS_ERROR: MHI device enter error state (may recover)
 * @MHI_CB_FATAL_ERROR: MHI device entered fatal error
 */
enum MHI_CB {
	MHI_CB_IDLE,
	MHI_CB_PENDING_DATA,
	MHI_CB_DTR_SIGNAL,
	MHI_CB_LPM_ENTER,
	MHI_CB_LPM_EXIT,
	MHI_CB_EE_RDDM,
	MHI_CB_EE_MISSION_MODE,
	MHI_CB_SYS_ERROR,
	MHI_CB_FATAL_ERROR,
};

/**
 * enum MHI_DEBUG_LEVEL - various debugging level
 */
enum MHI_DEBUG_LEVEL {
	MHI_MSG_LVL_VERBOSE,
	MHI_MSG_LVL_INFO,
	MHI_MSG_LVL_ERROR,
	MHI_MSG_LVL_CRITICAL,
	MHI_MSG_LVL_MASK_ALL,
	MHI_MSG_LVL_MAX,
};

/**
 * enum MHI_FLAGS - Transfer flags
 * @MHI_EOB: End of buffer for bulk transfer
 * @MHI_EOT: End of transfer
 * @MHI_CHAIN: Linked transfer
 */
enum MHI_FLAGS {
	MHI_EOB,
	MHI_EOT,
	MHI_CHAIN,
};

/**
 * enum mhi_device_type - Device types
 * @MHI_XFER_TYPE: Handles data transfer
 * @MHI_TIMESYNC_TYPE: Use for timesync feature
 * @MHI_CONTROLLER_TYPE: Control device
 */
enum mhi_device_type {
	MHI_XFER_TYPE,
	MHI_TIMESYNC_TYPE,
	MHI_CONTROLLER_TYPE,
};

/**
 * enum mhi_ee - device current execution enviornment
 * @MHI_EE_PBL - device in PBL
 * @MHI_EE_SBL - device in SBL
 * @MHI_EE_AMSS - device in mission mode (firmware fully loaded)
 * @MHI_EE_RDDM - device in ram dump collection mode
 * @MHI_EE_WFW - device in WLAN firmware mode
 * @MHI_EE_PTHRU - device in PBL but configured in pass thru mode
 * @MHI_EE_EDL - device in emergency download mode
 */
enum mhi_ee {
	MHI_EE_PBL,
	MHI_EE_SBL,
	MHI_EE_AMSS,
	MHI_EE_RDDM,
	MHI_EE_WFW,
	MHI_EE_PTHRU,
	MHI_EE_EDL,
	MHI_EE_MAX_SUPPORTED = MHI_EE_EDL,
	MHI_EE_DISABLE_TRANSITION, /* local EE, not related to mhi spec */
	MHI_EE_NOT_SUPPORTED,
	MHI_EE_MAX,
};

/**
 * enum mhi_dev_state - device current MHI state
 */
enum mhi_dev_state {
	MHI_STATE_RESET = 0x0,
	MHI_STATE_READY = 0x1,
	MHI_STATE_M0 = 0x2,
	MHI_STATE_M1 = 0x3,
	MHI_STATE_M2 = 0x4,
	MHI_STATE_M3 = 0x5,
	MHI_STATE_M3_FAST = 0x6,
	MHI_STATE_BHI  = 0x7,
	MHI_STATE_SYS_ERR  = 0xFF,
	MHI_STATE_MAX,
};

/**
 * struct mhi_link_info - bw requirement
 * target_link_speed - as defined by TLS bits in LinkControl reg
 * target_link_width - as defined by NLW bits in LinkStatus reg
 * sequence_num - used by device to track bw requests sent to host
 */
struct mhi_link_info {
	unsigned int target_link_speed;
	unsigned int target_link_width;
	int sequence_num;
};

#define MHI_VOTE_BUS BIT(0) /* do not disable the bus */
#define MHI_VOTE_DEVICE BIT(1) /* prevent mhi device from entering lpm */

/**
 * struct image_info - firmware and rddm table table
 * @mhi_buf - Contain device firmware and rddm table
 * @entries - # of entries in table
 */
struct image_info {
	struct mhi_buf *mhi_buf;
	struct bhi_vec_entry *bhi_vec;
	u32 entries;
};

/**
 * struct reg_write_info - offload reg write info
 * @reg_addr - register address
 * @val - value to be written to register
 * @chan - channel number
 * @valid - entry is valid or not
 */
struct reg_write_info {
	void __iomem *reg_addr;
	u32 val;
	bool valid;
};

/**
 * struct mhi_controller - Master controller structure for external modem
 * @dev: Device associated with this controller
 * @of_node: DT that has MHI configuration information
 * @regs: Points to base of MHI MMIO register space
 * @bhi: Points to base of MHI BHI register space
 * @bhie: Points to base of MHI BHIe register space
 * @wake_db: MHI WAKE doorbell register address
 * @dev_id: PCIe device id of the external device
 * @domain: PCIe domain the device connected to
 * @bus: PCIe bus the device assigned to
 * @slot: PCIe slot for the modem
 * @iova_start: IOMMU starting address for data
 * @iova_stop: IOMMU stop address for data
 * @fw_image: Firmware image name for normal booting
 * @edl_image: Firmware image name for emergency download mode
 * @fbc_download: MHI host needs to do complete image transfer
 * @rddm_size: RAM dump size that host should allocate for debugging purpose
 * @sbl_size: SBL image size
 * @seg_len: BHIe vector size
 * @fbc_image: Points to firmware image buffer
 * @rddm_image: Points to RAM dump buffer
 * @max_chan: Maximum number of channels controller support
 * @mhi_chan: Points to channel configuration table
 * @lpm_chans: List of channels that require LPM notifications
 * @total_ev_rings: Total # of event rings allocated
 * @hw_ev_rings: Number of hardware event rings
 * @sw_ev_rings: Number of software event rings
 * @msi_required: Number of msi required to operate
 * @msi_allocated: Number of msi allocated by bus master
 * @irq: base irq # to request
 * @mhi_event: MHI event ring configurations table
 * @mhi_cmd: MHI command ring configurations table
 * @mhi_ctxt: MHI device context, shared memory between host and device
 * @timeout_ms: Timeout in ms for state transitions
 * @pm_state: Power management state
 * @ee: MHI device execution environment
 * @dev_state: MHI STATE
 * @mhi_link_info: requested link bandwidth by device
 * @status_cb: CB function to notify various power states to but master
 * @link_status: Query link status in case of abnormal value read from device
 * @runtime_get: Async runtime resume function
 * @runtimet_put: Release votes
 * @time_get: Return host time in us
 * @lpm_disable: Request controller to disable link level low power modes
 * @lpm_enable: Controller may enable link level low power modes again
 * @priv_data: Points to bus master's private data
 */
struct mhi_controller {
	struct list_head node;
	struct mhi_device *mhi_dev;

	/* device node for iommu ops */
	struct device *dev;
	struct device_node *of_node;

	/* mmio base */
	phys_addr_t base_addr;
	void __iomem *regs;
	void __iomem *bhi;
	void __iomem *bhie;
	void __iomem *wake_db;
	void __iomem *bw_scale_db;

	/* device topology */
	u32 dev_id;
	u32 domain;
	u32 bus;
	u32 slot;
	u32 family_number;
	u32 device_number;
	u32 major_version;
	u32 minor_version;

	/* addressing window */
	dma_addr_t iova_start;
	dma_addr_t iova_stop;

	/* fw images */
	const char *fw_image;
	const char *edl_image;

	/* mhi host manages downloading entire fbc images */
	bool fbc_download;
	size_t rddm_size;
	size_t sbl_size;
	size_t seg_len;
	u32 session_id;
	u32 sequence_id;
	struct image_info *fbc_image;
	struct image_info *rddm_image;

	/* physical channel config data */
	u32 max_chan;
	struct mhi_chan *mhi_chan;
	struct list_head lpm_chans; /* these chan require lpm notification */

	/* physical event config data */
	u32 total_ev_rings;
	u32 hw_ev_rings;
	u32 sw_ev_rings;
	u32 msi_required;
	u32 msi_allocated;
	int *irq; /* interrupt table */
	struct mhi_event *mhi_event;
	struct list_head lp_ev_rings; /* low priority event rings */

	/* cmd rings */
	struct mhi_cmd *mhi_cmd;

	/* mhi context (shared with device) */
	struct mhi_ctxt *mhi_ctxt;

	u32 timeout_ms;

	/* caller should grab pm_mutex for suspend/resume operations */
	struct mutex pm_mutex;
	bool pre_init;
	rwlock_t pm_lock;
	u32 pm_state;
	u32 saved_pm_state; /* saved state during fast suspend */
	u32 db_access; /* db access only on these states */
	enum mhi_ee ee;
	u32 ee_table[MHI_EE_MAX]; /* ee conversion from dev to host */
	enum mhi_dev_state dev_state;
	enum mhi_dev_state saved_dev_state;
	bool wake_set;
	atomic_t dev_wake;
	atomic_t alloc_size;
	atomic_t pending_pkts;
	struct list_head transition_list;
	spinlock_t transition_lock;
	spinlock_t wlock;

	/* target bandwidth info */
	struct mhi_link_info mhi_link_info;

	/* debug counters */
	u32 M0, M2, M3, M3_FAST;

	/* worker for different state transitions */
	struct work_struct st_worker;
	struct work_struct fw_worker;
	struct work_struct syserr_worker;
	struct work_struct low_priority_worker;
	wait_queue_head_t state_event;

	/* shadow functions */
	void (*status_cb)(struct mhi_controller *, void *, enum MHI_CB);
	int (*link_status)(struct mhi_controller *, void *);
	void (*wake_get)(struct mhi_controller *, bool);
	void (*wake_put)(struct mhi_controller *, bool);
	void (*wake_toggle)(struct mhi_controller *mhi_cntrl);
	int (*runtime_get)(struct mhi_controller *, void *);
	void (*runtime_put)(struct mhi_controller *, void *);
	u64 (*time_get)(struct mhi_controller *mhi_cntrl, void *priv);
	int (*lpm_disable)(struct mhi_controller *mhi_cntrl, void *priv);
	int (*lpm_enable)(struct mhi_controller *mhi_cntrl, void *priv);
	int (*map_single)(struct mhi_controller *mhi_cntrl,
			  struct mhi_buf_info *buf);
	void (*unmap_single)(struct mhi_controller *mhi_cntrl,
			     struct mhi_buf_info *buf);
	void (*tsync_log)(struct mhi_controller *mhi_cntrl, u64 remote_time);
	int (*bw_scale)(struct mhi_controller *mhi_cntrl,
			struct mhi_link_info *link_info);
	void (*write_reg)(struct mhi_controller *mhi_cntrl, void __iomem *base,
			u32 offset, u32 val);

	/* channel to control DTR messaging */
	struct mhi_device *dtr_dev;

	/* bounce buffer settings */
	bool bounce_buf;
	size_t buffer_len;

	/* supports time sync feature */
	struct mhi_timesync *mhi_tsync;
	struct mhi_device *tsync_dev;
	u64 local_timer_freq;
	u64 remote_timer_freq;

	/* subsytem failure reason retrieval feature */
	struct mhi_sfr_info *mhi_sfr;
	size_t sfr_len;

	/* kernel log level */
	enum MHI_DEBUG_LEVEL klog_lvl;

	/* private log level controller driver to set */
	enum MHI_DEBUG_LEVEL log_lvl;

	/* controller specific data */
	const char *name;
	bool power_down;
	void *priv_data;
	void *log_buf;
	struct dentry *dentry;
	struct dentry *parent;

	/* for reg write offload */
	struct workqueue_struct *offload_wq;
	struct work_struct reg_write_work;
	struct reg_write_info *reg_write_q;
	atomic_t write_idx;
	u32 read_idx;
};

/**
 * struct mhi_device - mhi device structure associated bind to channel
 * @dev: Device associated with the channels
 * @mtu: Maximum # of bytes controller support
 * @ul_chan_id: MHI channel id for UL transfer
 * @dl_chan_id: MHI channel id for DL transfer
 * @tiocm: Device current terminal settings
 * @early_notif: This device needs an early notification in case of error
 * with external modem.
 * @dev_vote: Keep external device in active state
 * @bus_vote: Keep physical bus (pci, spi) in active state
 * @priv: Driver private data
 */
struct mhi_device {
	struct device dev;
	u32 dev_id;
	u32 domain;
	u32 bus;
	u32 slot;
	size_t mtu;
	int ul_chan_id;
	int dl_chan_id;
	int ul_event_id;
	int dl_event_id;
	u32 tiocm;
	bool early_notif;
	const struct mhi_device_id *id;
	const char *chan_name;
	struct mhi_controller *mhi_cntrl;
	struct mhi_chan *ul_chan;
	struct mhi_chan *dl_chan;
	atomic_t dev_vote;
	atomic_t bus_vote;
	enum mhi_device_type dev_type;
	void *priv_data;
	int (*ul_xfer)(struct mhi_device *, struct mhi_chan *, void *,
		       size_t, enum MHI_FLAGS);
	int (*dl_xfer)(struct mhi_device *, struct mhi_chan *, void *,
		       size_t, enum MHI_FLAGS);
	void (*status_cb)(struct mhi_device *, enum MHI_CB);
};

/**
 * struct mhi_result - Completed buffer information
 * @buf_addr: Address of data buffer
 * @dir: Channel direction
 * @bytes_xfer: # of bytes transferred
 * @transaction_status: Status of last trasnferred
 */
struct mhi_result {
	void *buf_addr;
	enum dma_data_direction dir;
	size_t bytes_xferd;
	int transaction_status;
};

/**
 * struct mhi_buf - Describes the buffer
 * @page: buffer as a page
 * @buf: cpu address for the buffer
 * @phys_addr: physical address of the buffer
 * @dma_addr: iommu address for the buffer
 * @skb: skb of ip packet
 * @len: # of bytes
 * @name: Buffer label, for offload channel configurations name must be:
 * ECA - Event context array data
 * CCA - Channel context array data
 */
struct mhi_buf {
	struct list_head node;
	struct page *page;
	void *buf;
	phys_addr_t phys_addr;
	dma_addr_t dma_addr;
	struct sk_buff *skb;
	size_t len;
	const char *name; /* ECA, CCA */
};

/**
 * struct mhi_driver - mhi driver information
 * @id_table: NULL terminated channel ID names
 * @ul_xfer_cb: UL data transfer callback
 * @dl_xfer_cb: DL data transfer callback
 * @status_cb: Asynchronous status callback
 */
struct mhi_driver {
	const struct mhi_device_id *id_table;
	int (*probe)(struct mhi_device *, const struct mhi_device_id *id);
	void (*remove)(struct mhi_device *);
	void (*ul_xfer_cb)(struct mhi_device *, struct mhi_result *);
	void (*dl_xfer_cb)(struct mhi_device *, struct mhi_result *);
	void (*status_cb)(struct mhi_device *, enum MHI_CB mhi_cb);
	struct device_driver driver;
};

#define to_mhi_driver(drv) container_of(drv, struct mhi_driver, driver)
#define to_mhi_device(dev) container_of(dev, struct mhi_device, dev)

static inline void mhi_device_set_devdata(struct mhi_device *mhi_dev,
					  void *priv)
{
	mhi_dev->priv_data = priv;
}

static inline void *mhi_device_get_devdata(struct mhi_device *mhi_dev)
{
	return mhi_dev->priv_data;
}

/**
 * mhi_queue_transfer - Queue a buffer to hardware
 * All transfers are asyncronous transfers
 * @mhi_dev: Device associated with the channels
 * @dir: Data direction
 * @buf: Data buffer (skb for hardware channels)
 * @len: Size in bytes
 * @mflags: Interrupt flags for the device
 */
static inline int mhi_queue_transfer(struct mhi_device *mhi_dev,
				     enum dma_data_direction dir,
				     void *buf,
				     size_t len,
				     enum MHI_FLAGS mflags)
{
	if (dir == DMA_TO_DEVICE)
		return mhi_dev->ul_xfer(mhi_dev, mhi_dev->ul_chan, buf, len,
					mflags);
	else
		return mhi_dev->dl_xfer(mhi_dev, mhi_dev->dl_chan, buf, len,
					mflags);
}

static inline void *mhi_controller_get_devdata(struct mhi_controller *mhi_cntrl)
{
	return mhi_cntrl->priv_data;
}

static inline void mhi_free_controller(struct mhi_controller *mhi_cntrl)
{
	kfree(mhi_cntrl);
}

/**
 * mhi_driver_register - Register driver with MHI framework
 * @mhi_drv: mhi_driver structure
 */
int mhi_driver_register(struct mhi_driver *mhi_drv);

/**
 * mhi_driver_unregister - Unregister a driver for mhi_devices
 * @mhi_drv: mhi_driver structure
 */
void mhi_driver_unregister(struct mhi_driver *mhi_drv);

/**
 * mhi_device_configure - configure ECA or CCA context
 * For offload channels that client manage, call this
 * function to configure channel context or event context
 * array associated with the channel
 * @mhi_div: Device associated with the channels
 * @dir: Direction of the channel
 * @mhi_buf: Configuration data
 * @elements: # of configuration elements
 */
int mhi_device_configure(struct mhi_device *mhi_div,
			 enum dma_data_direction dir,
			 struct mhi_buf *mhi_buf,
			 int elements);

/**
 * mhi_device_get - disable low power modes
 * Only disables lpm, does not immediately exit low power mode
 * if controller already in a low power mode
 * @mhi_dev: Device associated with the channels
 * @vote: requested vote (bus, device or both)
 */
void mhi_device_get(struct mhi_device *mhi_dev, int vote);

/**
 * mhi_device_get_sync - disable low power modes
 * Synchronously disable device & or bus low power, exit low power mode if
 * controller already in a low power state
 * @mhi_dev: Device associated with the channels
 * @vote: requested vote (bus, device or both)
 */
int mhi_device_get_sync(struct mhi_device *mhi_dev, int vote);

/**
 * mhi_device_put - re-enable low power modes
 * @mhi_dev: Device associated with the channels
 * @vote: vote to remove
 */
void mhi_device_put(struct mhi_device *mhi_dev, int vote);

/**
 * mhi_prepare_for_transfer - setup channel for data transfer
 * Moves both UL and DL channel from RESET to START state
 * @mhi_dev: Device associated with the channels
 */
int mhi_prepare_for_transfer(struct mhi_device *mhi_dev);

/**
 * mhi_unprepare_from_transfer -unprepare the channels
 * Moves both UL and DL channels to RESET state
 * @mhi_dev: Device associated with the channels
 */
void mhi_unprepare_from_transfer(struct mhi_device *mhi_dev);

/**
 * mhi_get_no_free_descriptors - Get transfer ring length
 * Get # of TD available to queue buffers
 * @mhi_dev: Device associated with the channels
 * @dir: Direction of the channel
 */
int mhi_get_no_free_descriptors(struct mhi_device *mhi_dev,
				enum dma_data_direction dir);

/**
 * mhi_poll - poll for any available data to consume
 * This is only applicable for DL direction
 * @mhi_dev: Device associated with the channels
 * @budget: In descriptors to service before returning
 */
int mhi_poll(struct mhi_device *mhi_dev, u32 budget);

/**
 * mhi_ioctl - user space IOCTL support for MHI channels
 * Native support for setting  TIOCM
 * @mhi_dev: Device associated with the channels
 * @cmd: IOCTL cmd
 * @arg: Optional parameter, iotcl cmd specific
 */
long mhi_ioctl(struct mhi_device *mhi_dev, unsigned int cmd, unsigned long arg);

/**
 * mhi_alloc_controller - Allocate mhi_controller structure
 * Allocate controller structure and additional data for controller
 * private data. You may get the private data pointer by calling
 * mhi_controller_get_devdata
 * @size: # of additional bytes to allocate
 */
struct mhi_controller *mhi_alloc_controller(size_t size);

/**
 * of_register_mhi_controller - Register MHI controller
 * Registers MHI controller with MHI bus framework. DT must be supported
 * @mhi_cntrl: MHI controller to register
 */
int of_register_mhi_controller(struct mhi_controller *mhi_cntrl);

void mhi_unregister_mhi_controller(struct mhi_controller *mhi_cntrl);

/**
 * mhi_bdf_to_controller - Look up a registered controller
 * Search for controller based on device identification
 * @domain: RC domain of the device
 * @bus: Bus device connected to
 * @slot: Slot device assigned to
 * @dev_id: Device Identification
 */
struct mhi_controller *mhi_bdf_to_controller(u32 domain, u32 bus, u32 slot,
					     u32 dev_id);

/**
 * mhi_prepare_for_power_up - Do pre-initialization before power up
 * This is optional, call this before power up if controller do not
 * want bus framework to automatically free any allocated memory during shutdown
 * process.
 * @mhi_cntrl: MHI controller
 */
int mhi_prepare_for_power_up(struct mhi_controller *mhi_cntrl);

/**
 * mhi_async_power_up - Starts MHI power up sequence
 * @mhi_cntrl: MHI controller
 */
int mhi_async_power_up(struct mhi_controller *mhi_cntrl);
int mhi_sync_power_up(struct mhi_controller *mhi_cntrl);

/**
 * mhi_power_down - Start MHI power down sequence
 * @mhi_cntrl: MHI controller
 * @graceful: link is still accessible, do a graceful shutdown process otherwise
 * we will shutdown host w/o putting device into RESET state
 */
void mhi_power_down(struct mhi_controller *mhi_cntrl, bool graceful);

/**
 * mhi_unprepare_after_powre_down - free any allocated memory for power up
 * @mhi_cntrl: MHI controller
 */
void mhi_unprepare_after_power_down(struct mhi_controller *mhi_cntrl);

/**
 * mhi_pm_suspend - Move MHI into a suspended state
 * Transition to MHI state M3 state from M0||M1||M2 state
 * @mhi_cntrl: MHI controller
 */
int mhi_pm_suspend(struct mhi_controller *mhi_cntrl);

/**
 * mhi_pm_fast_suspend - Move host into suspend state while keeping
 * the device in active state.
 * @mhi_cntrl: MHI controller
 * @notify_client: if true, clients will get a notification about lpm transition
 */
int mhi_pm_fast_suspend(struct mhi_controller *mhi_cntrl, bool notify_client);

/**
 * mhi_pm_resume - Resume MHI from suspended state
 * Transition to MHI state M0 state from M3 state
 * @mhi_cntrl: MHI controller
 */
int mhi_pm_resume(struct mhi_controller *mhi_cntrl);

/**
 * mhi_pm_fast_resume - Move host into resume state from fast suspend state
 * @mhi_cntrl: MHI controller
 * @notify_client: if true, clients will get a notification about lpm transition
 */
int mhi_pm_fast_resume(struct mhi_controller *mhi_cntrl, bool notify_client);

/**
 * mhi_download_rddm_img - Download ramdump image from device for
 * debugging purpose.
 * @mhi_cntrl: MHI controller
 * @in_panic: If we trying to capture image while in kernel panic
 */
int mhi_download_rddm_img(struct mhi_controller *mhi_cntrl, bool in_panic);

/**
 * mhi_force_rddm_mode - Force external device into rddm mode
 * to collect device ramdump. This is useful if host driver assert
 * and we need to see device state as well.
 * @mhi_cntrl: MHI controller
 */
int mhi_force_rddm_mode(struct mhi_controller *mhi_cntrl);

/**
 * mhi_get_remote_time_sync - Get external soc time relative to local soc time
 * using MMIO method.
 * @mhi_dev: Device associated with the channels
 * @t_host: Pointer to output local soc time
 * @t_dev: Pointer to output remote soc time
 */
int mhi_get_remote_time_sync(struct mhi_device *mhi_dev,
			     u64 *t_host,
			     u64 *t_dev);

/**
 * mhi_get_mhi_state - Return MHI state of device
 * @mhi_cntrl: MHI controller
 */
enum mhi_dev_state mhi_get_mhi_state(struct mhi_controller *mhi_cntrl);

/**
 * mhi_set_mhi_state - Set device state
 * @mhi_cntrl: MHI controller
 * @state: state to set
 */
void mhi_set_mhi_state(struct mhi_controller *mhi_cntrl,
		       enum mhi_dev_state state);


/**
 * mhi_is_active - helper function to determine if MHI in active state
 * @mhi_dev: client device
 */
static inline bool mhi_is_active(struct mhi_device *mhi_dev)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;

	return (mhi_cntrl->dev_state >= MHI_STATE_M0 &&
		mhi_cntrl->dev_state <= MHI_STATE_M3_FAST);
}

/**
 * mhi_control_error - MHI controller went into unrecoverable error state.
 * Will transition MHI into Linkdown state. Do not call from atomic
 * context.
 * @mhi_cntrl: MHI controller
 */
void mhi_control_error(struct mhi_controller *mhi_cntrl);

/**
 * mhi_debug_reg_dump - dump MHI registers for debug purpose
 * @mhi_cntrl: MHI controller
 */
void mhi_debug_reg_dump(struct mhi_controller *mhi_cntrl);

/**
 * mhi_get_restart_reason - retrieve the subsystem failure reason
 * @name: controller name
 */
char *mhi_get_restart_reason(const char *name);

#ifndef CONFIG_ARCH_QCOM

#ifdef CONFIG_MHI_DEBUG

#define MHI_VERB(fmt, ...) do { \
		if (mhi_cntrl->klog_lvl <= MHI_MSG_VERBOSE) \
			pr_dbg("[D][%s] " fmt, __func__, ##__VA_ARGS__);\
} while (0)

#else

#define MHI_VERB(fmt, ...)

#endif

#define MHI_LOG(fmt, ...) do {	\
		if (mhi_cntrl->klog_lvl <= MHI_MSG_INFO) \
			pr_info("[I][%s] " fmt, __func__, ##__VA_ARGS__);\
} while (0)

#define MHI_ERR(fmt, ...) do {	\
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_ERROR) \
			pr_err("[E][%s] " fmt, __func__, ##__VA_ARGS__); \
} while (0)

#define MHI_CRITICAL(fmt, ...) do { \
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_CRITICAL) \
			pr_alert("[C][%s] " fmt, __func__, ##__VA_ARGS__); \
} while (0)

#else /* ARCH QCOM */

#include <linux/ipc_logging.h>

#ifdef CONFIG_MHI_DEBUG

#define MHI_VERB(fmt, ...) do { \
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_VERBOSE) \
			pr_err("[D][%s] " fmt, __func__, ##__VA_ARGS__);\
		if (mhi_cntrl->log_buf && \
		    (mhi_cntrl->log_lvl <= MHI_MSG_LVL_VERBOSE)) \
			ipc_log_string(mhi_cntrl->log_buf, "[D][%s] " fmt, \
				       __func__, ##__VA_ARGS__); \
} while (0)

#else

#define MHI_VERB(fmt, ...) do { \
		if (mhi_cntrl->log_buf && \
		    (mhi_cntrl->log_lvl <= MHI_MSG_LVL_VERBOSE)) \
			ipc_log_string(mhi_cntrl->log_buf, "[D][%s] " fmt, \
				       __func__, ##__VA_ARGS__); \
} while (0)

#endif

#define MHI_LOG(fmt, ...) do {	\
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_INFO) \
			pr_err("[I][%s] " fmt, __func__, ##__VA_ARGS__);\
		if (mhi_cntrl->log_buf && \
		    (mhi_cntrl->log_lvl <= MHI_MSG_LVL_INFO)) \
			ipc_log_string(mhi_cntrl->log_buf, "[I][%s] " fmt, \
				       __func__, ##__VA_ARGS__); \
} while (0)

#define MHI_ERR(fmt, ...) do {	\
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_ERROR) \
			pr_err("[E][%s] " fmt, __func__, ##__VA_ARGS__); \
		if (mhi_cntrl->log_buf && \
		    (mhi_cntrl->log_lvl <= MHI_MSG_LVL_ERROR)) \
			ipc_log_string(mhi_cntrl->log_buf, "[E][%s] " fmt, \
				       __func__, ##__VA_ARGS__); \
} while (0)

#define MHI_CRITICAL(fmt, ...) do { \
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_CRITICAL) \
			pr_err("[C][%s] " fmt, __func__, ##__VA_ARGS__); \
		if (mhi_cntrl->log_buf && \
		    (mhi_cntrl->log_lvl <= MHI_MSG_LVL_CRITICAL)) \
			ipc_log_string(mhi_cntrl->log_buf, "[C][%s] " fmt, \
				       __func__, ##__VA_ARGS__); \
} while (0)

#endif

#endif /* _MHI_H_ */
