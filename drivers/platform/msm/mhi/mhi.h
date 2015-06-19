/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#ifndef _H_MHI
#define _H_MHI

#include "mhi_macros.h"
#include <linux/msm_mhi.h>
#include <linux/types.h>
#include <linux/pm.h>
#include <linux/completion.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <mach/msm_pcie.h>
#include <linux/sched.h>

extern struct mhi_pcie_devices mhi_devices;

enum MHI_DEBUG_LEVEL {
	MHI_MSG_VERBOSE = 0x1,
	MHI_MSG_INFO = 0x2,
	MHI_MSG_DBG = 0x4,
	MHI_MSG_WARNING = 0x8,
	MHI_MSG_ERROR = 0x10,
	MHI_MSG_CRITICAL = 0x20,
	MHI_MSG_reserved = 0x80000000
};

struct pcie_core_info {
	u32 dev_id;
	u32 manufact_id;
	u32 mhi_ver;
	void __iomem *bar0_base;
	void __iomem *bar0_end;
	void __iomem *bar2_base;
	void __iomem *bar2_end;
	u32 device_wake_gpio;
	u32 irq_base;
	u32 max_nr_msis;
	struct pci_saved_state *pcie_state;
};

struct bhi_ctxt_t {
	void __iomem *bhi_base;
	void *image_loc;
	dma_addr_t phy_image_loc;
	size_t image_size;
	void *unaligned_image_loc;
	dev_t bhi_dev;
	struct cdev cdev;
	struct class *bhi_class;
	struct device *dev;
};

enum MHI_CHAN_TYPE {
	MHI_INVALID = 0x0,
	MHI_OUT = 0x1,
	MHI_IN = 0x2,
	MHI_CHAN_TYPE_reserved = 0x80000000
};

enum MHI_CHAN_STATE {
	MHI_CHAN_STATE_DISABLED = 0x0,
	MHI_CHAN_STATE_ENABLED = 0x1,
	MHI_CHAN_STATE_RUNNING = 0x2,
	MHI_CHAN_STATE_SUSPENDED = 0x3,
	MHI_CHAN_STATE_STOP = 0x4,
	MHI_CHAN_STATE_ERROR = 0x5,
	MHI_CHAN_STATE_LIMIT = 0x6,
	MHI_CHAN_STATE_reserved = 0x80000000
};

enum MHI_RING_TYPE {
	MHI_RING_TYPE_CMD_RING = 0x0,
	MHI_RING_TYPE_XFER_RING = 0x1,
	MHI_RING_TYPE_EVENT_RING = 0x2,
	MHI_RING_TYPE_MAX = 0x4,
	MHI_RING_reserved = 0x80000000
};

enum MHI_CHAIN {
	MHI_TRE_CHAIN_OFF = 0x0,
	MHI_TRE_CHAIN_ON = 0x1,
	MHI_TRE_CHAIN_LIMIT = 0x2,
	MHI_TRE_CHAIN_reserved = 0x80000000
};

enum MHI_EVENT_RING_STATE {
	MHI_EVENT_RING_UINIT = 0x0,
	MHI_EVENT_RING_INIT = 0x1,
	MHI_EVENT_RING_reserved = 0x80000000
};

enum MHI_STATE {
	MHI_STATE_RESET = 0x0,
	MHI_STATE_READY = 0x1,
	MHI_STATE_M0 = 0x2,
	MHI_STATE_M1 = 0x3,
	MHI_STATE_M2 = 0x4,
	MHI_STATE_M3 = 0x5,
	MHI_STATE_BHI  = 0x7,
	MHI_STATE_SYS_ERR  = 0x8,
	MHI_STATE_LIMIT = 0x9,
	MHI_STATE_reserved = 0x80000000
};

struct __packed mhi_event_ctxt {
	u32 mhi_intmodt;
	u32 mhi_event_er_type;
	u32 mhi_msi_vector;
	u64 mhi_event_ring_base_addr;
	u64 mhi_event_ring_len;
	u64 mhi_event_read_ptr;
	u64 mhi_event_write_ptr;
};

struct __packed mhi_chan_ctxt {
	enum MHI_CHAN_STATE mhi_chan_state;
	enum MHI_CHAN_TYPE mhi_chan_type;
	u32 mhi_event_ring_index;
	u64 mhi_trb_ring_base_addr;
	u64 mhi_trb_ring_len;
	u64 mhi_trb_read_ptr;
	u64 mhi_trb_write_ptr;
};

struct __packed mhi_cmd_ctxt {
	u32 mhi_cmd_ctxt_reserved1;
	u32 mhi_cmd_ctxt_reserved2;
	u32 mhi_cmd_ctxt_reserved3;
	u64 mhi_cmd_ring_base_addr;
	u64 mhi_cmd_ring_len;
	u64 mhi_cmd_ring_read_ptr;
	u64 mhi_cmd_ring_write_ptr;
};

enum MHI_COMMAND {
	MHI_COMMAND_NOOP = 0x0,
	MHI_COMMAND_RESET_CHAN = 0x1,
	MHI_COMMAND_STOP_CHAN = 0x2,
	MHI_COMMAND_START_CHAN = 0x3,
	MHI_COMMAND_RESUME_CHAN = 0x4,
	MHI_COMMAND_MAX_NR = 0x5,
	MHI_COMMAND_reserved = 0x80000000
};

enum MHI_PKT_TYPE {
	MHI_PKT_TYPE_RESERVED = 0x0,
	MHI_PKT_TYPE_NOOP_CMD = 0x1,
	MHI_PKT_TYPE_TRANSFER = 0x2,
	MHI_PKT_TYPE_RESET_CHAN_CMD = 0x10,
	MHI_PKT_TYPE_STOP_CHAN_CMD = 0x11,
	MHI_PKT_TYPE_START_CHAN_CMD = 0x12,
	MHI_PKT_TYPE_RESET_CHAN_DEFER_CMD = 0x1F,
	MHI_PKT_TYPE_STATE_CHANGE_EVENT = 0x20,
	MHI_PKT_TYPE_CMD_COMPLETION_EVENT = 0x21,
	MHI_PKT_TYPE_TX_EVENT = 0x22,
	MHI_PKT_TYPE_EE_EVENT = 0x40,
	MHI_PKT_TYPE_SYS_ERR_EVENT = 0xFF,
};

struct __packed mhi_tx_pkt {
	u64 buffer_ptr;
	u32 buf_len;
	u32 info;
};

struct __packed mhi_noop_tx_pkt {
	u64 reserved1;
	u32 reserved2;
	u32 info;
};

struct __packed mhi_noop_cmd_pkt {
	u64 reserved1;
	u32 reserved2;
	u32 info;
};

struct __packed mhi_reset_chan_cmd_pkt {
	u32 reserved1;
	u32 reserved2;
	u32 reserved3;
	u32 info;
};

struct __packed mhi_ee_state_change_event {
	u64 reserved1;
	u32 exec_env;
	u32 info;
};

struct __packed mhi_xfer_event_pkt {
	u64 xfer_ptr;
	u32 xfer_details;
	u32 info;
};

struct __packed mhi_cmd_complete_event_pkt {
	u64 ptr;
	u32 code;
	u32 info;
};

struct __packed mhi_state_change_event_pkt {
	u64 reserved1;
	u32 state;
	u32 info;
};

union __packed mhi_xfer_pkt {
	struct mhi_tx_pkt data_tx_pkt;
	struct mhi_noop_tx_pkt noop_tx_pkt;
	struct mhi_tx_pkt type;
};

union __packed mhi_cmd_pkt {
	struct mhi_reset_chan_cmd_pkt reset_cmd_pkt;
	struct mhi_noop_cmd_pkt noop_cmd_pkt;
	struct mhi_noop_cmd_pkt type;
};

union __packed mhi_event_pkt {
	struct mhi_xfer_event_pkt xfer_event_pkt;
	struct mhi_cmd_complete_event_pkt cmd_complete_event_pkt;
	struct mhi_state_change_event_pkt state_change_event_pkt;
	struct mhi_ee_state_change_event ee_event_pkt;
	struct mhi_xfer_event_pkt type;
};

enum MHI_EVENT_CCS {
	MHI_EVENT_CC_INVALID = 0x0,
	MHI_EVENT_CC_SUCCESS = 0x1,
	MHI_EVENT_CC_EOT = 0x2,
	MHI_EVENT_CC_OVERFLOW = 0x3,
	MHI_EVENT_CC_EOB = 0x4,
	MHI_EVENT_CC_OOB = 0x5,
	MHI_EVENT_CC_DB_MODE = 0x6,
	MHI_EVENT_CC_UNDEFINED_ERR = 0x10,
	MHI_EVENT_CC_RING_EL_ERR = 0x11,
};

struct mhi_ring {
	void *base;
	void *wp;
	void *rp;
	void *ack_rp;
	uintptr_t len;
	uintptr_t el_size;
	u32 overwrite_en;
	enum MHI_CHAN_TYPE dir;
};

enum MHI_CMD_STATUS {
	MHI_CMD_NOT_PENDING = 0x0,
	MHI_CMD_PENDING = 0x1,
	MHI_CMD_RESET_PENDING = 0x2,
	MHI_CMD_RESERVED = 0x80000000
};

enum MHI_EVENT_RING_TYPE {
	MHI_EVENT_RING_TYPE_INVALID = 0x0,
	MHI_EVENT_RING_TYPE_VALID = 0x1,
	MHI_EVENT_RING_TYPE_reserved = 0x80000000
};

enum MHI_INIT_ERROR_STAGE {
	MHI_INIT_ERROR_STAGE_UNWIND_ALL = 0x1,
	MHI_INIT_ERROR_STAGE_DEVICE_CTRL = 0x2,
	MHI_INIT_ERROR_STAGE_THREADS = 0x3,
	MHI_INIT_ERROR_STAGE_EVENTS = 0x4,
	MHI_INIT_ERROR_STAGE_MEM_ZONES = 0x5,
	MHI_INIT_ERROR_STAGE_SYNC = 0x6,
	MHI_INIT_ERROR_STAGE_THREAD_QUEUES = 0x7,
	MHI_INIT_ERROR_TIMERS = 0x8,
	MHI_INIT_ERROR_STAGE_RESERVED = 0x80000000
};

enum STATE_TRANSITION {
	STATE_TRANSITION_RESET = 0x0,
	STATE_TRANSITION_READY = 0x1,
	STATE_TRANSITION_M0 = 0x2,
	STATE_TRANSITION_M1 = 0x3,
	STATE_TRANSITION_M2 = 0x4,
	STATE_TRANSITION_M3 = 0x5,
	STATE_TRANSITION_BHI = 0x6,
	STATE_TRANSITION_SBL = 0x7,
	STATE_TRANSITION_AMSS = 0x8,
	STATE_TRANSITION_LINK_DOWN = 0x9,
	STATE_TRANSITION_WAKE = 0xA,
	STATE_TRANSITION_SYS_ERR = 0xFF,
	STATE_TRANSITION_reserved = 0x80000000
};

enum MHI_EXEC_ENV {
	MHI_EXEC_ENV_PBL = 0x0,
	MHI_EXEC_ENV_SBL = 0x1,
	MHI_EXEC_ENV_AMSS = 0x2,
	MHI_EXEC_ENV_reserved = 0x80000000
};

struct mhi_chan_info {
	u32 chan_nr;
	u32 max_desc;
	u32 ev_ring;
	u32 flags;
};

struct mhi_client_handle {
	struct mhi_chan_info chan_info;
	struct mhi_device_ctxt *mhi_dev_ctxt;
	struct mhi_client_info_t client_info;
	struct completion chan_reset_complete;
	struct completion chan_open_complete;
	void *user_data;
	struct mhi_result result;
	u32 device_index;
	u32 msi_vec;
	u32 intmod_t;
	u32 pkt_count;
	int magic;
	int chan_status;
	int event_ring_index;
};

enum MHI_EVENT_POLLING {
	MHI_EVENT_POLLING_DISABLED = 0x0,
	MHI_EVENT_POLLING_ENABLED = 0x1,
	MHI_EVENT_POLLING_reserved = 0x80000000
};

enum MHI_TYPE_EVENT_RING {
	MHI_ER_DATA_TYPE = 0x1,
	MHI_ER_CTRL_TYPE = 0x2,
	MHI_ER_TYPE_RESERVED = 0x80000000
};

struct mhi_state_work_queue {
	spinlock_t *q_lock;
	struct mhi_ring q_info;
	u32 queue_full_cntr;
	enum STATE_TRANSITION buf[MHI_WORK_Q_MAX_SIZE];
};

struct mhi_control_seg {
	union mhi_cmd_pkt cmd_trb_list[NR_OF_CMD_RINGS][CMD_EL_PER_RING + 1];
	struct mhi_cmd_ctxt mhi_cmd_ctxt_list[NR_OF_CMD_RINGS];
	struct mhi_chan_ctxt mhi_cc_list[MHI_MAX_CHANNELS];
	struct mhi_event_ctxt *mhi_ec_list;
	u32 padding;
};

struct mhi_counters {
	u32 m0_m1;
	u32 m1_m0;
	u32 m1_m2;
	u32 m2_m0;
	u32 m0_m3;
	u32 m3_m0;
	u32 m1_m3;
	u32 mhi_reset_cntr;
	u32 mhi_ready_cntr;
	u32 m3_event_timeouts;
	u32 m0_event_timeouts;
	u32 msi_disable_cntr;
	u32 msi_enable_cntr;
	u32 nr_irq_migrations;
	u32 *msi_counter;
	u32 *ev_counter;
	atomic_t outbound_acks;
	u32 chan_pkts_xferd[MHI_MAX_CHANNELS];
};

struct mhi_flags {
	u32 mhi_initialized;
	u32 pending_M3;
	u32 pending_M0;
	u32 link_up;
	u32 kill_threads;
	atomic_t data_pending;
	atomic_t events_pending;
	atomic_t pending_resume;
	atomic_t pending_ssr;
	atomic_t pending_powerup;
	atomic_t m2_transition;
	int stop_threads;
	atomic_t device_wake;
	u32 ssr;
	u32 ev_thread_stopped;
	u32 st_thread_stopped;
	u32 uldl_enabled;
	u32 db_mode[MHI_MAX_CHANNELS];
};

struct mhi_wait_queues {
	wait_queue_head_t *mhi_event_wq;
	wait_queue_head_t *state_change_event;
	wait_queue_head_t *m0_event;
	wait_queue_head_t *m3_event;
	wait_queue_head_t *bhi_event;
};

struct dev_mmio_info {
	void __iomem *mmio_addr;
	void __iomem *chan_db_addr;
	void __iomem *event_db_addr;
	void __iomem *cmd_db_addr;
	u64 mmio_len;
	u32 nr_event_rings;
	dma_addr_t dma_ev_ctxt; /* Bus address of ECABAP*/
	void *dma_ev_rings;
};

struct mhi_device_ctxt {
	enum MHI_STATE mhi_state;
	enum MHI_EXEC_ENV dev_exec_env;

	struct mhi_pcie_dev_info *dev_info;
	struct pcie_core_info *dev_props;
	struct mhi_control_seg *mhi_ctrl_seg;
	struct mhi_meminfo *mhi_ctrl_seg_info;

	struct mhi_ring mhi_local_chan_ctxt[MHI_MAX_CHANNELS];
	struct mhi_ring *mhi_local_event_ctxt;
	struct mhi_ring mhi_local_cmd_ctxt[NR_OF_CMD_RINGS];

	struct mutex *mhi_chan_mutex;
	struct mutex mhi_link_state;
	spinlock_t *mhi_ev_spinlock_list;
	struct mutex *mhi_cmd_mutex_list;
	struct mhi_client_handle *client_handle_list[MHI_MAX_CHANNELS];
	struct mhi_event_ring_cfg *ev_ring_props;
	struct task_struct *event_thread_handle;
	struct task_struct *st_thread_handle;
	struct mhi_wait_queues mhi_ev_wq;
	struct dev_mmio_info mmio_info;

	u32 mhi_chan_db_order[MHI_MAX_CHANNELS];
	u32 mhi_ev_db_order[MHI_MAX_CHANNELS];
	spinlock_t *db_write_lock;

	struct mhi_state_work_queue state_change_work_item_list;
	enum MHI_CMD_STATUS mhi_chan_pend_cmd_ack[MHI_MAX_CHANNELS];

	u32 cmd_ring_order;
	struct mhi_counters counters;
	struct mhi_flags flags;

	u32 device_wake_asserted;

	rwlock_t xfer_lock;
	struct hrtimer m1_timer;
	ktime_t m1_timeout;

	struct esoc_desc *esoc_handle;
	void *esoc_ssr_handle;

	u32 bus_client;
	struct msm_bus_scale_pdata *bus_scale_table;
	struct notifier_block mhi_cpu_notifier;

	unsigned long esoc_notif;
	enum STATE_TRANSITION base_state;
	atomic_t outbound_acks;
	struct mutex pm_lock;
	struct wakeup_source w_lock;

	int enable_lpm;
	char *chan_info;
	struct dentry *mhi_parent_folder;
};

struct mhi_pcie_dev_info {
	struct pcie_core_info core;
	struct mhi_device_ctxt mhi_ctxt;
	struct msm_pcie_register_event mhi_pci_link_event;
	struct pci_dev *pcie_device;
	struct pci_driver *mhi_pcie_driver;
	struct bhi_ctxt_t bhi_ctxt;
	struct platform_device *plat_dev;
	u32 link_down_cntr;
	u32 link_up_cntr;
};

struct mhi_pcie_devices {
	struct mhi_pcie_dev_info device_list[MHI_MAX_SUPPORTED_DEVICES];
	s32 nr_of_devices;
};

struct mhi_event_ring_cfg {
	u32 nr_desc;
	u32 msi_vec;
	u32 intmod;
	u32 flags;
	enum MHI_EVENT_RING_STATE state;
	irqreturn_t (*mhi_handler_ptr)(int , void *);
};

irqreturn_t mhi_msi_ipa_handlr(int irq_number, void *dev_id);
enum MHI_STATUS mhi_reset_all_thread_queues(
					struct mhi_device_ctxt *mhi_dev_ctxt);
enum MHI_STATUS mhi_add_elements_to_event_rings(
				struct mhi_device_ctxt *mhi_dev_ctxt,
					enum STATE_TRANSITION new_state);
int get_nr_avail_ring_elements(struct mhi_ring *ring);
enum MHI_STATUS get_nr_enclosed_el(struct mhi_ring *ring, void *loc_1,
					void *loc_2, u32 *nr_el);
enum MHI_STATUS mhi_init_mmio(struct mhi_device_ctxt *mhi_dev_ctxt);
enum MHI_STATUS mhi_init_device_ctxt(struct mhi_pcie_dev_info *dev_info,
				struct mhi_device_ctxt *mhi_dev_ctxt);
enum MHI_STATUS mhi_init_local_event_ring(struct mhi_device_ctxt *mhi_dev_ctxt,
		u32 nr_ev_el, u32 event_ring_index);
/*Mhi Initialization functions */
enum MHI_STATUS mhi_clean_init_stage(struct mhi_device_ctxt *mhi_dev_ctxt,
				enum MHI_INIT_ERROR_STAGE cleanup_stage);
enum MHI_STATUS mhi_send_cmd(struct mhi_device_ctxt *dest_device,
			enum MHI_COMMAND which_cmd, u32 chan);
enum MHI_STATUS mhi_queue_tx_pkt(struct mhi_device_ctxt *mhi_dev_ctxt,
				enum MHI_CLIENT_CHANNEL chan,
				void *payload,
				size_t payload_size);
enum MHI_STATUS mhi_init_chan_ctxt(struct mhi_chan_ctxt *cc_list,
				   uintptr_t trb_list_phy,
				   uintptr_t trb_list_virt,
				   u64 el_per_ring,
				   enum MHI_CHAN_TYPE chan_type,
				   u32 event_ring,
				   struct mhi_ring *ring,
				   enum MHI_CHAN_STATE chan_state);
int mhi_populate_event_cfg(struct mhi_device_ctxt *mhi_dev_ctxt);
int mhi_get_event_ring_for_channel(struct mhi_device_ctxt *mhi_dev_ctxt,
					      u32 chan);
enum MHI_STATUS delete_element(struct mhi_ring *ring, void **rp,
			 void **wp, void **assigned_addr);
enum MHI_STATUS ctxt_add_element(struct mhi_ring *ring, void **assigned_addr);
enum MHI_STATUS ctxt_del_element(struct mhi_ring *ring, void **assigned_addr);
enum MHI_STATUS get_element_index(struct mhi_ring *ring, void *address,
							uintptr_t *index);
enum MHI_STATUS recycle_trb_and_ring(struct mhi_device_ctxt *mhi_dev_ctxt,
	struct mhi_ring *ring, enum MHI_RING_TYPE ring_type, u32 ring_index);
enum MHI_STATUS parse_xfer_event(struct mhi_device_ctxt *ctxt,
					union mhi_event_pkt *event);
enum MHI_EVENT_CCS get_cmd_pkt(union mhi_event_pkt *ev_pkt,
			       union mhi_cmd_pkt **cmd_pkt);
enum MHI_STATUS parse_cmd_event(struct mhi_device_ctxt *ctxt,
					union mhi_event_pkt *event);
int parse_event_thread(void *ctxt);
enum MHI_STATUS mhi_test_for_device_ready(
					struct mhi_device_ctxt *mhi_dev_ctxt);
enum MHI_STATUS mhi_test_for_device_reset(
					struct mhi_device_ctxt *mhi_dev_ctxt);
enum MHI_STATUS validate_ring_el_addr(struct mhi_ring *ring, uintptr_t addr);
enum MHI_STATUS validate_ev_el_addr(struct mhi_ring *ring, uintptr_t addr);
int mhi_state_change_thread(void *ctxt);
enum MHI_STATUS mhi_init_state_transition(struct mhi_device_ctxt *mhi_dev_ctxt,
					enum STATE_TRANSITION new_state);
enum MHI_STATUS mhi_wait_for_mdm(struct mhi_device_ctxt *mhi_dev_ctxt);
enum hrtimer_restart mhi_initiate_m1(struct hrtimer *timer);
int mhi_pci_suspend(struct pci_dev *dev, pm_message_t state);
int mhi_pci_resume(struct pci_dev *dev);
int mhi_init_pcie_device(struct mhi_pcie_dev_info *mhi_pcie_dev);
int mhi_init_gpios(struct mhi_pcie_dev_info *mhi_pcie_dev);
int mhi_init_pm_sysfs(struct device *dev);
void mhi_rem_pm_sysfs(struct device *dev);
void mhi_pci_remove(struct pci_dev *mhi_device);
int mhi_ctxt_init(struct mhi_pcie_dev_info *mhi_pcie_dev);
int mhi_get_chan_max_buffers(u32 chan);
int mhi_esoc_register(struct mhi_device_ctxt *mhi_dev_ctxt);
void mhi_link_state_cb(struct msm_pcie_notify *notify);
void mhi_notify_clients(struct mhi_device_ctxt *mhi_dev_ctxt,
						enum MHI_CB_REASON reason);
void mhi_notify_client(struct mhi_client_handle *client_handle,
		       enum MHI_CB_REASON reason);
int mhi_deassert_device_wake(struct mhi_device_ctxt *mhi_dev_ctxt);
int mhi_assert_device_wake(struct mhi_device_ctxt *mhi_dev_ctxt);
enum MHI_STATUS mhi_reg_notifiers(struct mhi_device_ctxt *mhi_dev_ctxt);
int mhi_cpu_notifier_cb(struct notifier_block *nfb, unsigned long action,
			void *hcpu);
enum MHI_STATUS init_mhi_base_state(struct mhi_device_ctxt *mhi_dev_ctxt);
enum MHI_STATUS mhi_turn_off_pcie_link(struct mhi_device_ctxt *mhi_dev_ctxt);
enum MHI_STATUS mhi_turn_on_pcie_link(struct mhi_device_ctxt *mhi_dev_ctxt);
int mhi_initiate_m0(struct mhi_device_ctxt *mhi_dev_ctxt);
int mhi_initiate_m3(struct mhi_device_ctxt *mhi_dev_ctxt);
int mhi_set_bus_request(struct mhi_device_ctxt *mhi_dev_ctxt,
					int index);
enum MHI_STATUS start_chan_sync(struct mhi_client_handle *client_handle);
void mhi_process_db(struct mhi_device_ctxt *mhi_dev_ctxt, void __iomem *io_addr,
		  uintptr_t io_offset, u32 val);
void mhi_reg_write_field(struct mhi_device_ctxt *mhi_dev_ctxt,
			 void __iomem *io_addr,
			 uintptr_t io_offset,
			 u32 mask, u32 shift, u32 val);
void mhi_reg_write(struct mhi_device_ctxt *mhi_dev_ctxt,
		   void __iomem *io_addr, uintptr_t io_offset, u32 val);
u32 mhi_reg_read(void __iomem *io_addr, uintptr_t io_offset);
u32 mhi_reg_read_field(void __iomem *io_addr, uintptr_t io_offset,
			 u32 mask, u32 shift);
void mhi_exit_m2(struct mhi_device_ctxt *mhi_dev_ctxt);
int mhi_runtime_suspend(struct device *dev);
int get_chan_props(struct mhi_device_ctxt *mhi_dev_ctxt, int chan,
		   struct mhi_chan_info *chan_info);
int mhi_runtime_resume(struct device *dev);
enum MHI_STATUS mhi_trigger_reset(struct mhi_device_ctxt *mhi_dev_ctxt);
int init_ev_rings(struct mhi_device_ctxt *mhi_dev_ctxt,
		  enum MHI_TYPE_EVENT_RING type);
void mhi_reset_ev_ctxt(struct mhi_device_ctxt *mhi_dev_ctxt,
				int index);
int init_event_ctxt_array(struct mhi_device_ctxt *mhi_dev_ctxt);
int create_ev_rings(struct mhi_device_ctxt *mhi_dev_ctxt);

#endif
