/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2018-2021, The Linux Foundation. All rights reserved. */

#ifndef _MHI_H_
#define _MHI_H_

#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>

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
	MHI_CB_FW_FALLBACK_IMG,
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

#define MHI_VOTE_BUS BIT(0) /* do not disable the bus */
#define MHI_VOTE_DEVICE BIT(1) /* prevent mhi device from entering lpm */

/**
 * struct mhi_link_info - bw requirement
 * target_link_speed - as defined by TLS bits in LinkControl reg
 * target_link_width - as defined by NLW bits in LinkStatus reg
 * sequence_num - used by device to track bw requests sent to host
 * last_response - used by host to cache response to the last bw switch request
 */
struct mhi_link_info {
	unsigned int target_link_speed;
	unsigned int target_link_width;
	int sequence_num;
	u32 last_response;
};

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

/* rddm header info */

#define MAX_RDDM_TABLE_SIZE 6

/**
 * struct rddm_table_info - rddm table info
 * @base_address - Start offset of the file
 * @actual_phys_address - phys addr offset of file
 * @size - size of file
 * @description - file description
 * @file_name - name of file
 */
struct rddm_table_info {
	u64 base_address;
	u64 actual_phys_address;
	u64 size;
	char description[20];
	char file_name[20];
};

/**
 * struct rddm_header - rddm header
 * @version - header ver
 * @header_size - size of header
 * @rddm_table_info - array of rddm table info
 */
struct rddm_header {
	u32 version;
	u32 header_size;
	struct rddm_table_info table_info[MAX_RDDM_TABLE_SIZE];
};

/**
 * struct file_info - keeping track of file info while traversing the rddm
 * table header
 * @file_offset - current file offset
 * @seg_idx - mhi buf seg array index
 * @rem_seg_len - remaining length of the segment containing current file
 */
struct file_info {
	u8 *file_offset;
	u32 file_size;
	u32 seg_idx;
	u32 rem_seg_len;
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
 * @img_pre_alloc: allocate rddm and fbc image buffers one time
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
 * @reset: Controller specific reset function (optional)
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
	unsigned int len;
	void __iomem *regs;
	void __iomem *bhi;
	void __iomem *bhie;
	void __iomem *wake_db;
	void __iomem *tsync_db;
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
	const char *fw_image_fallback;
	const char *edl_image;

	/* mhi host manages downloading entire fbc images */
	bool fbc_download;
	bool rddm_supported;
	size_t rddm_size;
	size_t sbl_size;
	size_t seg_len;
	u32 session_id;
	u32 sequence_id;
	u32 bhie_offset;

	bool img_pre_alloc;
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
	struct list_head sp_ev_rings; /* special purpose event rings */

	/* cmd rings */
	struct mhi_cmd *mhi_cmd;

	/* mhi context (shared with device) */
	struct mhi_ctxt *mhi_ctxt;

	u32 timeout_ms;
	u32 m2_timeout_ms; /* wait time for host to continue suspend after m2 */

	/* caller should grab pm_mutex for suspend/resume operations */
	struct mutex pm_mutex;
	struct mutex tsync_mutex;
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
	bool ignore_override;
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
	struct work_struct special_work;
	struct workqueue_struct *wq;

	wait_queue_head_t state_event;

	/* shadow functions */
	void (*status_cb)(struct mhi_controller *mhi_cntrl, void *priv,
			  enum MHI_CB reason);
	int (*link_status)(struct mhi_controller *mhi_cntrl, void *priv);
	void (*wake_get)(struct mhi_controller *mhi_cntrl, bool override);
	void (*wake_put)(struct mhi_controller *mhi_cntrl, bool override);
	void (*wake_toggle)(struct mhi_controller *mhi_cntrl);
	int (*runtime_get)(struct mhi_controller *mhi_cntrl, void *priv);
	void (*runtime_put)(struct mhi_controller *mhi_cntrl, void *priv);
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
	void (*reset)(struct mhi_controller *mhi_cntrl);

	/* channel to control DTR messaging */
	struct mhi_device *dtr_dev;

	/* bounce buffer settings */
	bool bounce_buf;
	size_t buffer_len;

	/* supports time sync feature */
	struct mhi_timesync *mhi_tsync;
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
	bool initiate_mhi_reset;
	void *priv_data;
	void *log_buf;
	void *cntrl_log_buf;
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
	int (*ul_xfer)(struct mhi_device *mhi_dev, struct mhi_chan *mhi_chan,
		       void *buf, size_t len, enum MHI_FLAGS flags);
	int (*dl_xfer)(struct mhi_device *mhi_dev, struct mhi_chan *mhi_chan,
		       void *buf, size_t size, enum MHI_FLAGS flags);
	void (*status_cb)(struct mhi_device *mhi_dev, enum MHI_CB reason);
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
	int (*probe)(struct mhi_device *mhi_dev,
		     const struct mhi_device_id *id);
	void (*remove)(struct mhi_device *mhi_dev);
	void (*ul_xfer_cb)(struct mhi_device *mhi_dev, struct mhi_result *res);
	void (*dl_xfer_cb)(struct mhi_device *mhi_dev, struct mhi_result *res);
	void (*status_cb)(struct mhi_device *mhi_dev, enum MHI_CB mhi_cb);
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
 * mhi_device_get_sync_atomic - Asserts device_wait and moves device to M0
 * @mhi_dev: Device associated with the channels
 * @timeout_us: timeout, in micro-seconds
 * @in_panic: If requested while kernel is in panic state and no ISRs expected
 *
 * The device_wake is asserted to keep device in M0 or bring it to M0.
 * If device is not in M0 state, then this function will wait for device to
 * move to M0, until @timeout_us elapses.
 * However, if device's M1 state-change event races with this function
 * then there is a possiblity of device moving from M0 to M2 and back
 * to M0. That can't be avoided as host must transition device from M1 to M2
 * as per the spec.
 * Clients can ignore that transition after this function returns as the device
 * is expected to immediately  move from M2 to M0 as wake is asserted and
 * wouldn't enter low power state.
 * If in_panic boolean is set, no ISRs are expected, hence this API will have to
 * resort to reading the MHI status register and poll on M0 state change.
 *
 * Returns:
 * 0 if operation was successful (however, M0 -> M2 -> M0 is possible later) as
 * mentioned above.
 * -ETIMEDOUT is device faled to move to M0 before @timeout_us elapsed
 * -EIO if the MHI state is one of the ERROR states.
 */
int mhi_device_get_sync_atomic(struct mhi_device *mhi_dev,
			       int timeout_us,
			       bool in_panic);

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
 * mhi_pause_transfer - Pause the current transfe
 * Moves both UL and DL channels to STOP state to halt
 * pending transfers.
 * @mhi_dev: Device associated with the channels
 */
int mhi_pause_transfer(struct mhi_device *mhi_dev);

/**
 * mhi_resume_transfer - resume current transfer
 * Moves both UL and DL channels to START state to
 * resume transfer.
 * @mhi_dev: Device associated with the channels
 */
int mhi_resume_transfer(struct mhi_device *mhi_dev);

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
 * mhi_force_reset - does host reset request to collect device side dumps
 * for debugging purpose
 * @mhi_cntrl: MHI controller
 */
int mhi_force_reset(struct mhi_controller *mhi_cntrl);

/**
 * mhi_scan_rddm_cookie - Look for supplied cookie value in the BHI debug
 * registers set by device to indicate rddm readiness for debugging purposes.
 * @mhi_cntrl: MHI controller
 * @cookie: cookie/pattern value to match
 */
bool mhi_scan_rddm_cookie(struct mhi_controller *mhi_cntrl, u32 cookie);

/**
 * mhi_force_rddm_mode - Force external device into rddm mode
 * to collect device ramdump. This is useful if host driver assert
 * and we need to see device state as well.
 * @mhi_cntrl: MHI controller
 */
int mhi_force_rddm_mode(struct mhi_controller *mhi_cntrl);

/**
 * mhi_dump_sfr - Print SFR string from RDDM table.
 * @mhi_cntrl: MHI controller
 */
void mhi_dump_sfr(struct mhi_controller *mhi_cntrl);

/**
 * mhi_get_remote_time - Get external modem time relative to host time
 * Trigger event to capture modem time, also capture host time so client
 * can do a relative drift comparision.
 * Recommended only tsync device calls this method and do not call this
 * from atomic context
 * @mhi_dev: Device associated with the channels
 * @sequence:unique sequence id track event
 * @cb_func: callback function to call back
 */
int mhi_get_remote_time(struct mhi_device *mhi_dev,
			u32 sequence,
			void (*cb_func)(struct mhi_device *mhi_dev,
					u32 sequence,
					u64 local_time,
					u64 remote_time));

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
 * mhi_get_remote_time - Get external modem time relative to host time
 * Trigger event to capture modem time, also capture host time so client
 * can do a relative drift comparision.
 * Recommended only tsync device calls this method and do not call this
 * from atomic context
 * @mhi_dev: Device associated with the channels
 * @sequence:unique sequence id track event
 * @cb_func: callback function to call back
 */
int mhi_get_remote_time(struct mhi_device *mhi_dev,
			u32 sequence,
			void (*cb_func)(struct mhi_device *mhi_dev,
					u32 sequence,
					u64 local_time,
					u64 remote_time));

/**
 * mhi_get_exec_env - Return execution environment of the device
 * @mhi_cntrl: MHI controller
 */
enum mhi_ee mhi_get_exec_env(struct mhi_controller *mhi_cntrl);

/**
 * mhi_get_mhi_state - Return MHI state of device
 * @mhi_cntrl: MHI controller
 */
enum mhi_dev_state mhi_get_mhi_state(struct mhi_controller *mhi_cntrl);

/**
 * mhi_soc_reset - Trigger a device reset. This can be used as a last resort
 *			to reset and recover a device.
 * @mhi_cntrl: MHI controller
 */
void mhi_soc_reset(struct mhi_controller *mhi_cntrl);

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
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_VERBOSE) \
			pr_dbg("[D][%s] " fmt, __func__, ##__VA_ARGS__);\
} while (0)

#else

#define MHI_VERB(fmt, ...)

#endif

#define MHI_CNTRL_LOG(fmt, ...) do {	\
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_INFO) \
			printk("%s[I][%s] " fmt, KERN_INFO, __func__, \
					##__VA_ARGS__); \
} while (0)

#define MHI_CNTRL_ERR(fmt, ...) do {	\
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_ERROR) \
			printk("%s[E][%s] " fmt, KERN_ERR, __func__, \
					##__VA_ARGS__); \
} while (0)

#define MHI_LOG(fmt, ...) do {	\
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_INFO) \
			printk("%s[I][%s] " fmt, KERN_INFO, __func__, \
					##__VA_ARGS__); \
} while (0)

#define MHI_ERR(fmt, ...) do {	\
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_ERROR) \
			printk("%s[E][%s] " fmt, KERN_ERR, __func__, \
					##__VA_ARGS__); \
} while (0)

#define MHI_CRITICAL(fmt, ...) do { \
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_CRITICAL) \
			printk("%s[C][%s] " fmt, KERN_ALERT, __func__, \
					##__VA_ARGS__); \
} while (0)

#else /* ARCH QCOM */

#include <linux/ipc_logging.h>

#ifdef CONFIG_MHI_DEBUG

#define MHI_VERB(fmt, ...) do { \
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_VERBOSE) \
			printk("%s[D][%s] " fmt, KERN_ERR, __func__, \
					##__VA_ARGS__); \
		if (mhi_cntrl->log_lvl <= MHI_MSG_LVL_VERBOSE) \
			ipc_log_string(mhi_cntrl->log_buf, "%s[D][%s] " fmt, \
				       "", __func__, ##__VA_ARGS__); \
} while (0)

#else

#define MHI_VERB(fmt, ...) do { \
		if (mhi_cntrl->log_lvl <= MHI_MSG_LVL_VERBOSE) \
			ipc_log_string(mhi_cntrl->log_buf, "%s[D][%s] " fmt, \
				       "", __func__, ##__VA_ARGS__); \
} while (0)

#endif

#define MHI_CNTRL_LOG(fmt, ...) do { \
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_INFO) \
			printk("%s[I][%s] " fmt, KERN_ERR, __func__, \
					##__VA_ARGS__); \
		ipc_log_string(mhi_cntrl->cntrl_log_buf, "[I][%s] " fmt, \
			       __func__, ##__VA_ARGS__); \
} while (0)

#define MHI_CNTRL_ERR(fmt, ...) do { \
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_ERROR) \
			printk("%s[E][%s] " fmt, KERN_ERR, __func__, \
					##__VA_ARGS__); \
		ipc_log_string(mhi_cntrl->cntrl_log_buf, "[E][%s] " fmt, \
			       __func__, ##__VA_ARGS__); \
} while (0)

#define MHI_LOG(fmt, ...) do {	\
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_INFO) \
			printk("%s[I][%s] " fmt, KERN_ERR, __func__, \
					##__VA_ARGS__); \
		if (mhi_cntrl->log_lvl <= MHI_MSG_LVL_INFO) \
			ipc_log_string(mhi_cntrl->log_buf, "%s[I][%s] " fmt, \
				       "", __func__, ##__VA_ARGS__); \
} while (0)

#define MHI_ERR(fmt, ...) do {	\
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_ERROR) \
			printk("%s[E][%s] " fmt, KERN_ERR, __func__, \
					##__VA_ARGS__); \
		if (mhi_cntrl->log_lvl <= MHI_MSG_LVL_ERROR) \
			ipc_log_string(mhi_cntrl->log_buf, "%s[E][%s] " fmt, \
				       "", __func__, ##__VA_ARGS__); \
} while (0)

#define MHI_CRITICAL(fmt, ...) do { \
		if (mhi_cntrl->klog_lvl <= MHI_MSG_LVL_CRITICAL) \
			printk("%s[C][%s] " fmt, KERN_ERR, __func__, \
					##__VA_ARGS__); \
		if (mhi_cntrl->log_lvl <= MHI_MSG_LVL_CRITICAL) \
			ipc_log_string(mhi_cntrl->log_buf, "%s[C][%s] " fmt, \
				       "", __func__, ##__VA_ARGS__); \
} while (0)

#endif

#endif /* _MHI_H_ */
