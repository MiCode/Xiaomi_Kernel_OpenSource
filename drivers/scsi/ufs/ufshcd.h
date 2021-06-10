/*
 * Universal Flash Storage Host controller driver
 *
 * This code is based on drivers/scsi/ufs/ufshcd.h
 * Copyright (C) 2011-2013 Samsung India Software Operations
 * Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
 *
 * Authors:
 *	Santosh Yaraganavi <santosh.sy@samsung.com>
 *	Vinayak Holikatti <h.vinayak@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * See the COPYING file in the top-level directory or visit
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program is provided "AS IS" and "WITH ALL FAULTS" and
 * without warranty of any kind. You are solely responsible for
 * determining the appropriateness of using and distributing
 * the program and assume all risks associated with your exercise
 * of rights with respect to the program, including but not limited
 * to infringement of third party rights, the risks and costs of
 * program errors, damage to or loss of data, programs or equipment,
 * and unavailability or interruption of operations. Under no
 * circumstances will the contributor of this Program be liable for
 * any damages of any kind arising from your use or distribution of
 * this program.
 */

#ifndef _UFSHCD_H
#define _UFSHCD_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/regulator/consumer.h>
#include "unipro.h"

#include <asm/irq.h>
#include <asm/byteorder.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_eh.h>

#include "ufs.h"
#include "ufshci.h"
#if defined(CONFIG_UFSFEATURE)
#include "ufsfeature.h"
#endif
#if defined(CONFIG_SCSI_SKHPB)
#include "ufshpb_skh.h"
#endif

/* MTK PATCH */
#include <linux/rpmb.h>

#define UFSHCD "ufshcd"
#define UFSHCD_DRIVER_VERSION "0.2"

struct ufs_hba;

enum dev_cmd_type {
	DEV_CMD_TYPE_NOP		= 0x0,
	DEV_CMD_TYPE_QUERY		= 0x1,
};

/**
 * struct uic_command - UIC command structure
 * @command: UIC command
 * @argument1: UIC command argument 1
 * @argument2: UIC command argument 2
 * @argument3: UIC command argument 3
 * @cmd_active: Indicate if UIC command is outstanding
 * @result: UIC command result
 * @done: UIC command completion
 */
struct uic_command {
	u32 command;
	u32 argument1;
	u32 argument2;
	u32 argument3;
	int cmd_active;
	int result;
	struct completion done;
};

/* Used to differentiate the power management options */
enum ufs_pm_op {
	UFS_RUNTIME_PM,
	UFS_SYSTEM_PM,
	UFS_SHUTDOWN_PM,
};

#define ufshcd_is_runtime_pm(op) ((op) == UFS_RUNTIME_PM)
#define ufshcd_is_system_pm(op) ((op) == UFS_SYSTEM_PM)
#define ufshcd_is_shutdown_pm(op) ((op) == UFS_SHUTDOWN_PM)

/* Host <-> Device UniPro Link state */
enum uic_link_state {
	UIC_LINK_OFF_STATE	= 0, /* Link powered down or disabled */
	UIC_LINK_ACTIVE_STATE	= 1, /* Link is in Fast/Slow/Sleep state */
	UIC_LINK_HIBERN8_STATE	= 2, /* Link is in Hibernate state */
};

#define ufshcd_is_link_off(hba) ((hba)->uic_link_state == UIC_LINK_OFF_STATE)
#define ufshcd_is_link_active(hba) ((hba)->uic_link_state == \
				    UIC_LINK_ACTIVE_STATE)
#define ufshcd_is_link_hibern8(hba) ((hba)->uic_link_state == \
				    UIC_LINK_HIBERN8_STATE)
#define ufshcd_set_link_off(hba) ((hba)->uic_link_state = UIC_LINK_OFF_STATE)
#define ufshcd_set_link_active(hba) ((hba)->uic_link_state = \
				    UIC_LINK_ACTIVE_STATE)
#define ufshcd_set_link_hibern8(hba) ((hba)->uic_link_state = \
				    UIC_LINK_HIBERN8_STATE)

/*
 * UFS Power management levels.
 * Each level is in increasing order of power savings.
 */
enum ufs_pm_level {
	UFS_PM_LVL_0, /* UFS_ACTIVE_PWR_MODE, UIC_LINK_ACTIVE_STATE */
	UFS_PM_LVL_1, /* UFS_ACTIVE_PWR_MODE, UIC_LINK_HIBERN8_STATE */
	UFS_PM_LVL_2, /* UFS_SLEEP_PWR_MODE, UIC_LINK_ACTIVE_STATE */
	UFS_PM_LVL_3, /* UFS_SLEEP_PWR_MODE, UIC_LINK_HIBERN8_STATE */
	UFS_PM_LVL_4, /* UFS_POWERDOWN_PWR_MODE, UIC_LINK_HIBERN8_STATE */
	UFS_PM_LVL_5, /* UFS_POWERDOWN_PWR_MODE, UIC_LINK_OFF_STATE */
	UFS_PM_LVL_MAX
};

struct ufs_pm_lvl_states {
	enum ufs_dev_pwr_mode dev_state;
	enum uic_link_state link_state;
};

/**
 * struct ufshcd_lrb - local reference block
 * @utr_descriptor_ptr: UTRD address of the command
 * @ucd_req_ptr: UCD address of the command
 * @ucd_rsp_ptr: Response UPIU address for this command
 * @ucd_prdt_ptr: PRDT address of the command
 * @utrd_dma_addr: UTRD dma address for debug
 * @ucd_prdt_dma_addr: PRDT dma address for debug
 * @ucd_rsp_dma_addr: UPIU response dma address for debug
 * @ucd_req_dma_addr: UPIU request dma address for debug
 * @cmd: pointer to SCSI command
 * @sense_buffer: pointer to sense buffer address of the SCSI command
 * @sense_bufflen: Length of the sense buffer
 * @scsi_status: SCSI status of the command
 * @command_type: SCSI, UFS, Query.
 * @task_tag: Task tag of the command
 * @lun: LUN of the command
 * @intr_cmd: Interrupt command (doesn't participate in interrupt aggregation)
 * @issue_time_stamp: time stamp for debug purposes
 * @crypto_enable: whether or not the request needs inline crypto operations
 * @crypto_key_slot: the key slot to use for inline crypto
 * @data_unit_num: the data unit number for the first block for inline crypto
 * @req_abort_skip: skip request abort task flag
 */
struct ufshcd_lrb {
	struct utp_transfer_req_desc *utr_descriptor_ptr;
	struct utp_upiu_req *ucd_req_ptr;
	struct utp_upiu_rsp *ucd_rsp_ptr;
	struct ufshcd_sg_entry *ucd_prdt_ptr;

	dma_addr_t utrd_dma_addr;
	dma_addr_t ucd_req_dma_addr;
	dma_addr_t ucd_rsp_dma_addr;
	dma_addr_t ucd_prdt_dma_addr;

	struct scsi_cmnd *cmd;
	u8 *sense_buffer;
	unsigned int sense_bufflen;
	int scsi_status;

	int command_type;
	int task_tag;
	u8 lun; /* UPIU LUN id field is only 8-bit wide */
	bool intr_cmd;
	bool crypto_enable;
	u8 crypto_key_slot;
	u64 data_unit_num;

	/*
	 * Use sched_clock instead of ktime_get to align with
	 * kernel log timestamp and command history.
	 */
	/* MTK PATCH */
	u64 issue_time_stamp;
	u64 complete_time_stamp; /* only in memory dump */
	bool req_abort_skip;

#if defined(CONFIG_UFSFEATURE) && defined(CONFIG_UFSHPB)
	int hpb_ctx_id;
#endif
};

/**
 * struct ufs_query - holds relevant data structures for query request
 * @request: request upiu and function
 * @descriptor: buffer for sending/receiving descriptor
 * @response: response upiu and response
 */
struct ufs_query {
	struct ufs_query_req request;
	u8 *descriptor;
	struct ufs_query_res response;
};

/**
 * struct ufs_dev_cmd - all assosiated fields with device management commands
 * @type: device management command type - Query, NOP OUT
 * @lock: lock to allow one command at a time
 * @complete: internal commands completion
 * @tag_wq: wait queue until free command slot is available
 */
struct ufs_dev_cmd {
	enum dev_cmd_type type;
	struct mutex lock;
	struct completion *complete;
	wait_queue_head_t tag_wq;
	struct ufs_query query;
};

struct ufs_desc_size {
	int dev_desc;
	int pwr_desc;
	int geom_desc;
	int interc_desc;
	int unit_desc;
	int conf_desc;
};

/**
 * struct ufs_clk_info - UFS clock related info
 * @list: list headed by hba->clk_list_head
 * @clk: clock node
 * @name: clock name
 * @max_freq: maximum frequency supported by the clock
 * @min_freq: min frequency that can be used for clock scaling
 * @curr_freq: indicates the current frequency that it is set to
 * @enabled: variable to check against multiple enable/disable
 */
struct ufs_clk_info {
	struct list_head list;
	struct clk *clk;
	const char *name;
	u32 max_freq;
	u32 min_freq;
	u32 curr_freq;
	bool enabled;
};

enum ufs_notify_change_status {
	PRE_CHANGE,
	POST_CHANGE,
};

struct ufs_pa_layer_attr {
	u32 gear_rx;
	u32 gear_tx;
	u32 lane_rx;
	u32 lane_tx;
	u32 pwr_rx;
	u32 pwr_tx;
	u32 hs_rate;
};

struct ufs_pwr_mode_info {
	bool is_valid;
	struct ufs_pa_layer_attr info;
};

/* MTK PATCH */
enum ufs_scsi_dev_cfg {
	UFS_SCSI_DEV_SLAVE_ALLOC,
	UFS_SCSI_DEV_SLAVE_CONFIGURE,
	UFS_SCSI_DEV_SLAVE_DESTROY
};

#define UFS_RESCTL_CMD_SEND        BIT(0)
#define UFS_RESCTL_CMD_COMP        BIT(1)

union ufs_crypto_cfg_entry;

/**
 * struct ufs_hba_variant_ops - variant specific callbacks
 * @name: variant name
 * @init: called when the driver is initialized
 * @exit: called to cleanup everything done in init
 * @get_ufs_hci_version: called to get UFS HCI version
 * @clk_scale_notify: notifies that clks are scaled up/down
 * @setup_clocks: called before touching any of the controller registers
 * @setup_regulators: called before accessing the host controller
 * @hce_enable_notify: called before and after HCE enable bit is set to allow
 *                     variant specific Uni-Pro initialization.
 * @link_startup_notify: called before and after Link startup is carried out
 *                       to allow variant specific Uni-Pro initialization.
 * @pwr_change_notify: called before and after a power mode change
 *			is carried out to allow vendor spesific capabilities
 *			to be set.
 * @setup_xfer_req: called before any transfer request is issued
 *                  to set some things
 * @complete_xfer_req: called after any transfer request is completed
 *                  to release some things
 * @setup_task_mgmt: called before any task management request is issued
 *                  to set some things
 * @hibern8_notify: called around hibern8 enter/exit
 * @apply_dev_quirks: called to apply device specific quirks
 * @suspend: called during host controller PM callback
 * @resume: called during host controller PM callback
 * @dbg_register_dump: used to dump controller debug information
 * @phy_initialization: used to initialize phys
 * @device_reset: called to issue a reset pulse on the UFS device
 * @program_key: program an inline encryption key into a keyslot
 */
struct ufs_hba_variant_ops {
	const char *name;
	int	(*init)(struct ufs_hba *);
	void    (*exit)(struct ufs_hba *);
	u32	(*get_ufs_hci_version)(struct ufs_hba *);
	int	(*clk_scale_notify)(struct ufs_hba *, bool,
				    enum ufs_notify_change_status);
	int	(*setup_clocks)(struct ufs_hba *, bool,
				enum ufs_notify_change_status);
	int     (*setup_regulators)(struct ufs_hba *, bool);
	int	(*hce_enable_notify)(struct ufs_hba *,
				     enum ufs_notify_change_status);
	int	(*link_startup_notify)(struct ufs_hba *,
				       enum ufs_notify_change_status);
	int	(*pwr_change_notify)(struct ufs_hba *,
					enum ufs_notify_change_status status,
					struct ufs_pa_layer_attr *,
					struct ufs_pa_layer_attr *);
	void	(*setup_xfer_req)(struct ufs_hba *, int, bool);
	void	(*setup_task_mgmt)(struct ufs_hba *, int, u8);
	void    (*hibern8_notify)(struct ufs_hba *, enum uic_cmd_dme,
					enum ufs_notify_change_status);
	int	(*apply_dev_quirks)(struct ufs_hba *);
	int     (*suspend)(struct ufs_hba *, enum ufs_pm_op);
	int     (*resume)(struct ufs_hba *, enum ufs_pm_op);
	void	(*dbg_register_dump)(struct ufs_hba *hba);
	int	(*phy_initialization)(struct ufs_hba *);
	void	(*device_reset)(struct ufs_hba *hba);

	/*
	 * MTK PATCH: Control AH8
	 *   1: Enable AH8
	 *   0: Disable AH8.
	 */
	void    (*auto_hibern8)(struct ufs_hba *, bool);

	/*
	 * MTK PATCH:
	 * Resource control
	 */
	void	(*res_ctrl)(struct ufs_hba *hba, unsigned int op);

	/*
	 * MTK PATCH: Lock for deepidle or SODI.
	 *   1: Lock. Deepidle or SODI is NOT allowed after locked.
	 *   0: Unlock. Deepidle or SODI is allowed after unlocked.
	 */
	void	(*deepidle_lock)(struct ufs_hba *, bool);

	/*
	 * MTK PATCH: SCSI device slave alloc/configure/destroy.
	 */
	int     (*scsi_dev_cfg)(struct scsi_device *, enum ufs_scsi_dev_cfg);
	int	(*program_key)(struct ufs_hba *hba,
			       const union ufs_crypto_cfg_entry *cfg, int slot);
	void	(*abort_handler)(struct ufs_hba *hba, int tag, char *file,
				 int line);
};

struct keyslot_mgmt_ll_ops;
struct ufs_hba_crypto_variant_ops {
	void (*setup_rq_keyslot_manager)(struct ufs_hba *hba,
					 struct request_queue *q);
	void (*destroy_rq_keyslot_manager)(struct ufs_hba *hba,
					   struct request_queue *q);
	int (*hba_init_crypto)(struct ufs_hba *hba,
			       const struct keyslot_mgmt_ll_ops *ksm_ops);
	void (*enable)(struct ufs_hba *hba);
	void (*disable)(struct ufs_hba *hba);
	int (*suspend)(struct ufs_hba *hba, enum ufs_pm_op pm_op);
	int (*resume)(struct ufs_hba *hba, enum ufs_pm_op pm_op);
	int (*debug)(struct ufs_hba *hba);
	int (*prepare_lrbp_crypto)(struct ufs_hba *hba,
				   struct scsi_cmnd *cmd,
				   struct ufshcd_lrb *lrbp);
	int (*map_sg_crypto)(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
	int (*complete_lrbp_crypto)(struct ufs_hba *hba,
				    struct scsi_cmnd *cmd,
				    struct ufshcd_lrb *lrbp);
	void *priv;
	void *crypto_DO_NOT_USE[8];
};

/* clock gating state  */
enum clk_gating_state {
	CLKS_OFF,
	CLKS_ON,
	REQ_CLKS_OFF,
	REQ_CLKS_ON,
};

/**
 * struct ufs_clk_gating - UFS clock gating related info
 * @gate_work: worker to turn off clocks after some delay as specified in
 * delay_ms
 * @ungate_work: worker to turn on clocks that will be used in case of
 * interrupt context
 * @state: the current clocks state
 * @delay_ms: gating delay in ms
 * @is_suspended: clk gating is suspended when set to 1 which can be used
 * during suspend/resume
 * @delay_attr: sysfs attribute to control delay_attr
 * @enable_attr: sysfs attribute to enable/disable clock gating
 * @is_enabled: Indicates the current status of clock gating
 * @active_reqs: number of requests that are pending and should be waited for
 * completion before gating clocks.
 */
struct ufs_clk_gating {
	struct delayed_work gate_work;
	struct work_struct ungate_work;
	enum clk_gating_state state;
	unsigned long delay_ms;
	bool is_suspended;
	struct device_attribute delay_attr;
	struct device_attribute enable_attr;
	bool is_enabled;
	int active_reqs;
	struct workqueue_struct *clk_gating_workq;
};

struct ufs_saved_pwr_info {
	struct ufs_pa_layer_attr info;
	bool is_valid;
};

/**
 * struct ufs_clk_scaling - UFS clock scaling related data
 * @active_reqs: number of requests that are pending. If this is zero when
 * devfreq ->target() function is called then schedule "suspend_work" to
 * suspend devfreq.
 * @tot_busy_t: Total busy time in current polling window
 * @window_start_t: Start time (in jiffies) of the current polling window
 * @busy_start_t: Start time of current busy period
 * @enable_attr: sysfs attribute to enable/disable clock scaling
 * @saved_pwr_info: UFS power mode may also be changed during scaling and this
 * one keeps track of previous power mode.
 * @workq: workqueue to schedule devfreq suspend/resume work
 * @suspend_work: worker to suspend devfreq
 * @resume_work: worker to resume devfreq
 * @is_allowed: tracks if scaling is currently allowed or not
 * @is_busy_started: tracks if busy period has started or not
 * @is_suspended: tracks if devfreq is suspended or not
 */
struct ufs_clk_scaling {
	int active_reqs;
	unsigned long tot_busy_t;
	unsigned long window_start_t;
	ktime_t busy_start_t;
	struct device_attribute enable_attr;
	struct ufs_saved_pwr_info saved_pwr_info;
	struct workqueue_struct *workq;
	struct work_struct suspend_work;
	struct work_struct resume_work;
	bool is_allowed;
	bool is_busy_started;
	bool is_suspended;
};

/**
 * struct ufs_init_prefetch - contains data that is pre-fetched once during
 * initialization
 * @icc_level: icc level which was read during initialization
 */
struct ufs_init_prefetch {
	u32 icc_level;
};

#define UFS_ERR_REG_HIST_LENGTH 8
/**
 * struct ufs_err_reg_hist - keeps history of uic errors
 * @pos: index to indicate cyclic buffer position
 * @reg: cyclic buffer for registers value
 * @tstamp: cyclic buffer for time stamp
 */
struct ufs_err_reg_hist {
	int pos;
	u32 reg[UFS_ERR_REG_HIST_LENGTH];
	ktime_t tstamp[UFS_ERR_REG_HIST_LENGTH];
};

/**
 * struct ufs_stats - keeps usage/err statistics
 * @hibern8_exit_cnt: Counter to keep track of number of exits,
 *		reset this after link-startup.
 * @last_hibern8_exit_tstamp: Set time after the hibern8 exit.
 *		Clear after the first successful command completion.
 * @pa_err: tracks pa-uic errors
 * @dl_err: tracks dl-uic errors
 * @nl_err: tracks nl-uic errors
 * @tl_err: tracks tl-uic errors
 * @dme_err: tracks dme errors
 * @auto_hibern8_err: tracks auto-hibernate errors
 * @fatal_err: tracks fatal errors
 * @linkup_err: tracks link-startup errors
 * @resume_err: tracks resume errors
 * @suspend_err: tracks suspend errors
 * @dev_reset: tracks device reset events
 * @host_reset: tracks host reset events
 * @task_abort: tracks task abort events
 */
struct ufs_stats {
	u32 hibern8_exit_cnt;
	ktime_t last_hibern8_exit_tstamp;

	/* uic specific errors */
	struct ufs_err_reg_hist pa_err;
	struct ufs_err_reg_hist dl_err;
	struct ufs_err_reg_hist nl_err;
	struct ufs_err_reg_hist tl_err;
	struct ufs_err_reg_hist dme_err;

	/* fatal errors */
	struct ufs_err_reg_hist auto_hibern8_err;
	struct ufs_err_reg_hist fatal_err;
	struct ufs_err_reg_hist link_startup_err;
	struct ufs_err_reg_hist resume_err;
	struct ufs_err_reg_hist suspend_err;

	/* abnormal events */
	struct ufs_err_reg_hist dev_reset;
	struct ufs_err_reg_hist host_reset;
	struct ufs_err_reg_hist sw_reset;
	struct ufs_err_reg_hist task_abort;
	struct ufs_err_reg_hist ocs_err_status;

	/* performance warning */
	struct ufs_err_reg_hist perf_warn;
};

/* MTK PATCH UFS Host Controller debug print bitmask */
#define UFSHCD_DBG_PRINT_CLK_FREQ_EN		UFS_BIT(0)
#define UFSHCD_DBG_PRINT_UIC_ERR_HIST_EN	UFS_BIT(1)
#define UFSHCD_DBG_PRINT_HOST_REGS_EN		UFS_BIT(2)
#define UFSHCD_DBG_PRINT_TRS_EN			UFS_BIT(3)
#define UFSHCD_DBG_PRINT_TMRS_EN		UFS_BIT(4)
#define UFSHCD_DBG_PRINT_PWR_EN			UFS_BIT(5)
#define UFSHCD_DBG_PRINT_HOST_STATE_EN		UFS_BIT(6)
#define UFSHCD_DBG_PRINT_ABORT_CMD_EN		UFS_BIT(7)
#define UFSHCD_DBG_PRINT_QCMD_EN		UFS_BIT(8)

#define UFSHCD_DBG_PRINT_ALL						   \
		(UFSHCD_DBG_PRINT_CLK_FREQ_EN		|		   \
		 UFSHCD_DBG_PRINT_UIC_ERR_HIST_EN	|		   \
		 UFSHCD_DBG_PRINT_HOST_REGS_EN | UFSHCD_DBG_PRINT_TRS_EN | \
		 UFSHCD_DBG_PRINT_TMRS_EN | UFSHCD_DBG_PRINT_PWR_EN |	   \
		 UFSHCD_DBG_PRINT_HOST_STATE_EN |	   \
		 UFSHCD_DBG_PRINT_ABORT_CMD_EN)

enum ufs_crypto_state {
	UFS_CRYPTO_HW_FDE             = (1 << 0),
	UFS_CRYPTO_HW_FDE_ENCRYPTED   = (1 << 1),
	UFS_CRYPTO_HW_FBE             = (1 << 2),
	UFS_CRYPTO_HW_FBE_ENCRYPTED   = (1 << 3),
};

/**
 * struct ufs_hba - per adapter private structure
 * @mmio_base: UFSHCI base register address
 * @ucdl_base_addr: UFS Command Descriptor base address
 * @utrdl_base_addr: UTP Transfer Request Descriptor base address
 * @utmrdl_base_addr: UTP Task Management Descriptor base address
 * @ucdl_dma_addr: UFS Command Descriptor DMA address
 * @utrdl_dma_addr: UTRDL DMA address
 * @utmrdl_dma_addr: UTMRDL DMA address
 * @host: Scsi_Host instance of the driver
 * @dev: device handle
 * @sdev_ufs_rpmb: reference to RPMB device W-LU
 * @lrb: local reference block
 * @lrb_in_use: lrb in use
 * @outstanding_tasks: Bits representing outstanding task requests
 * @outstanding_reqs: Bits representing outstanding transfer requests
 * @capabilities: UFS Controller Capabilities
 * @nutrs: Transfer Request Queue depth supported by controller
 * @nutmrs: Task Management Queue depth supported by controller
 * @ufs_version: UFS Version to which controller complies
 * @vops: pointer to variant specific operations
 * @priv: pointer to variant specific private data
 * @sg_entry_size: size of struct ufshcd_sg_entry (may include variant fields)
 * @irq: Irq number of the controller
 * @active_uic_cmd: handle of active UIC command
 * @uic_cmd_mutex: mutex for uic command
 * @tm_wq: wait queue for task management
 * @tm_tag_wq: wait queue for free task management slots
 * @tm_slots_in_use: bit map of task management request slots in use
 * @pwr_done: completion for power mode change
 * @tm_condition: condition variable for task management
 * @ufshcd_state: UFSHCD states
 * @eh_flags: Error handling flags
 * @intr_mask: Interrupt Mask Bits
 * @ee_ctrl_mask: Exception event control mask
 * @is_powered: flag to check if HBA is powered
 * @is_init_prefetch: flag to check if data was pre-fetched in initialization
 * @init_prefetch_data: data pre-fetched during initialization
 * @eh_work: Worker to handle UFS errors that require s/w attention
 * @eeh_work: Worker to handle exception events
 * @errors: HBA errors
 * @uic_error: UFS interconnect layer error status
 * @saved_err: sticky error mask
 * @saved_uic_err: sticky UIC error mask
 * @silence_err_logs: flag to silence error logs
 * @dev_cmd: ufs device management command information
 * @last_dme_cmd_tstamp: time stamp of the last completed DME command
 * @auto_bkops_enabled: to track whether bkops is enabled in device
 * @vreg_info: UFS device voltage regulator information
 * @clk_list_head: UFS host controller clocks list node head
 * @pwr_info: holds current power mode
 * @max_pwr_info: keeps the device max valid pwm
 * @desc_size: descriptor sizes reported by device
 * @urgent_bkops_lvl: keeps track of urgent bkops level for device
 * @is_urgent_bkops_lvl_checked: keeps track if the urgent bkops level for
 *  device is known or not.
 * @scsi_block_reqs_cnt: reference counting for scsi block requests
 * @crypto_capabilities: Content of crypto capabilities register (0x100)
 * @crypto_cap_array: Array of crypto capabilities
 * @crypto_cfg_register: Start of the crypto cfg array
 * @ksm: the keyslot manager tied to this hba
 */
struct ufs_hba {
	void __iomem *mmio_base;

	/* Virtual memory reference */
	struct utp_transfer_cmd_desc *ucdl_base_addr;
	struct utp_transfer_req_desc *utrdl_base_addr;
	struct utp_task_req_desc *utmrdl_base_addr;

	/* DMA memory reference */
	dma_addr_t ucdl_dma_addr;
	dma_addr_t utrdl_dma_addr;
	dma_addr_t utmrdl_dma_addr;

	struct Scsi_Host *host;
	struct device *dev;
	/*
	 * This field is to keep a reference to "scsi_device" corresponding to
	 * "UFS device" W-LU.
	 */
	struct scsi_device *sdev_ufs_device;

	/*
	 * MTK PATCH: RPMB device
	 */
	struct scsi_device *sdev_ufs_rpmb;
	struct rpmb_dev *rawdev_ufs_rpmb;

	enum ufs_dev_pwr_mode curr_dev_pwr_mode;
	enum uic_link_state uic_link_state;
	/* Desired UFS power management level during runtime PM */
	enum ufs_pm_level rpm_lvl;
	/* Desired UFS power management level during system PM */
	enum ufs_pm_level spm_lvl;
	struct device_attribute rpm_lvl_attr;
	struct device_attribute spm_lvl_attr;
	int pm_op_in_progress;

	/* Auto-Hibernate Idle Timer register value */
	u32 ahit;

	struct ufshcd_lrb *lrb;
	unsigned long lrb_in_use;

	unsigned long outstanding_tasks;
	unsigned long outstanding_reqs;

	u32 capabilities;
	int nutrs;
	int nutmrs;
	u32 ufs_version;
	struct ufs_hba_variant_ops *vops;
	void *priv;
	const struct ufs_hba_crypto_variant_ops *crypto_vops;
	size_t sg_entry_size;
	unsigned int irq;
	bool is_irq_enabled;

	/* Interrupt aggregation support is broken */
	#define UFSHCD_QUIRK_BROKEN_INTR_AGGR			UFS_BIT(0)

	/*
	 * delay before each dme command is required as the unipro
	 * layer has shown instabilities
	 */
	#define UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS		UFS_BIT(1)

	/*
	 * If UFS host controller is having issue in processing LCC (Line
	 * Control Command) coming from device then enable this quirk.
	 * When this quirk is enabled, host controller driver should disable
	 * the LCC transmission on UFS device (by clearing TX_LCC_ENABLE
	 * attribute of device to 0).
	 */
	#define UFSHCD_QUIRK_BROKEN_LCC				UFS_BIT(2)

	/*
	 * The attribute PA_RXHSUNTERMCAP specifies whether or not the
	 * inbound Link supports unterminated line in HS mode. Setting this
	 * attribute to 1 fixes moving to HS gear.
	 */
	#define UFSHCD_QUIRK_BROKEN_PA_RXHSUNTERMCAP		UFS_BIT(3)

	/*
	 * This quirk needs to be enabled if the host contoller only allows
	 * accessing the peer dme attributes in AUTO mode (FAST AUTO or
	 * SLOW AUTO).
	 */
	#define UFSHCD_QUIRK_DME_PEER_ACCESS_AUTO_MODE		UFS_BIT(4)

	/*
	 * This quirk needs to be enabled if the host contoller doesn't
	 * advertise the correct version in UFS_VER register. If this quirk
	 * is enabled, standard UFS host driver will call the vendor specific
	 * ops (get_ufs_hci_version) to get the correct version.
	 */
	#define UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION		UFS_BIT(5)

	/*
	 * This quirk needs to be enabled if the host contoller regards
	 * resolution of the values of PRDTO and PRDTL in UTRD as byte.
	 */
	#define UFSHCD_QUIRK_PRDT_BYTE_GRAN			UFS_BIT(7)

	/*
	 * MTK PATCH
	 * This quirk needs to be enabled if device requires hw reset
	 * if linkup is failed after retries.
	 */
	#define UFSHCD_QUIRK_UFS_HCI_DEV_RST_FOR_LINKUP_FAIL	UFS_BIT(8)

	/*
	 * MTK PATCH
	 * This quirk needs to be enabled if host needs vendor-specific reset
	 * flow.
	 */
	#define UFSHCD_QUIRK_UFS_HCI_VENDOR_HOST_RST		UFS_BIT(9)

	/*
	 * MTK PATCH
	 * This quirk needs to be enabled if host needs manually disable ah8
	 * before ringing any doorbell slots.
	 */
	#define UFSHCD_QUIRK_UFS_HCI_DISABLE_AH8_BEFORE_RDB	UFS_BIT(10)

    /*
	 * This quirk needs to be enabled if the host controller advertises
	 * inline encryption support but it doesn't work correctly.
	 */
	#define UFSHCD_QUIRK_BROKEN_CRYPTO			UFS_BIT(11)

	unsigned int quirks;	/* Deviations from standard UFSHCI spec. */

	/* Device deviations from standard UFS device spec. */
	unsigned int dev_quirks;

	wait_queue_head_t tm_wq;
	wait_queue_head_t tm_tag_wq;
	unsigned long tm_condition;
	unsigned long tm_slots_in_use;

	struct uic_command *active_uic_cmd;
	struct mutex uic_cmd_mutex;
	struct completion *uic_async_done;

	u32 ufshcd_state;
	u32 eh_flags;
	u32 intr_mask;
	u16 ee_ctrl_mask;
	u16 hba_enable_delay_us;
	bool is_powered;
	bool is_init_prefetch;
	struct ufs_init_prefetch init_prefetch_data;

	/* Work Queues */
	struct work_struct eh_work;
	struct work_struct eeh_work;
	struct work_struct rls_work;

	/* HBA Errors */
	u32 errors;
	u32 uic_error;
	u32 saved_err;
	u32 saved_uic_err;
	struct ufs_stats ufs_stats;
	bool silence_err_logs;

	/* Device management request data */
	struct ufs_dev_cmd dev_cmd;
	ktime_t last_dme_cmd_tstamp;

	/* Keeps information of the UFS device connected to this host */
	struct ufs_dev_info dev_info;
	bool auto_bkops_enabled;
	struct ufs_vreg_info vreg_info;
	struct list_head clk_list_head;

	bool wlun_dev_clr_ua;

	/* Number of requests aborts */
	int req_abort_count;

	/* Number of lanes available (1 or 2) for Rx/Tx */
	u32 lanes_per_direction;
	struct ufs_pa_layer_attr pwr_info;
	struct ufs_pwr_mode_info max_pwr_info;

	struct ufs_clk_gating clk_gating;
	/* Control to enable/disable host capabilities */
	u32 caps;
	/* Allow dynamic clk gating */
#define UFSHCD_CAP_CLK_GATING	(1 << 0)
	/* Allow hiberb8 with clk gating */
#define UFSHCD_CAP_HIBERN8_WITH_CLK_GATING (1 << 1)
	/* Allow dynamic clk scaling */
#define UFSHCD_CAP_CLK_SCALING	(1 << 2)
	/* Allow auto bkops to enabled during runtime suspend */
#define UFSHCD_CAP_AUTO_BKOPS_SUSPEND (1 << 3)
	/*
	 * This capability allows host controller driver to use the UFS HCI's
	 * interrupt aggregation capability.
	 * CAUTION: Enabling this might reduce overall UFS throughput.
	 */
#define UFSHCD_CAP_INTR_AGGR (1 << 4)
	/*
	 * This capability allows the device auto-bkops to be always enabled
	 * except during suspend (both runtime and suspend).
	 * Enabling this capability means that device will always be allowed
	 * to do background operation when it's active but it might degrade
	 * the performance of ongoing read/write operations.
	 */
#define UFSHCD_CAP_KEEP_AUTO_BKOPS_ENABLED_EXCEPT_SUSPEND (1 << 5)
	/*
	 * This capability allows the host controller driver to use the
	 * inline crypto engine, if it is present
	 */
#define UFSHCD_CAP_CRYPTO (1 << 7)

	struct devfreq *devfreq;
	struct ufs_clk_scaling clk_scaling;
	bool is_sys_suspended;

	enum bkops_status urgent_bkops_lvl;
	bool is_urgent_bkops_lvl_checked;

	struct rw_semaphore clk_scaling_lock;
	struct ufs_desc_size desc_size;

	/* MTK PATCH */
	/* record vendor id for vendor-specific configurations */
	u32 manu_id;

	/* MTK PATCH */
	struct device_attribute rpm_info_attr;
	struct device_attribute spm_info_attr;

	/* MTK PATCH */
	/* crypto */
	/* hw-fde key index */
	int crypto_hwfde_key_idx;
	u32 crypto_feature;

	/* MTK PATCH */
	struct mutex rpmb_lock;

	struct ufs_dev_desc *card;

	atomic_t scsi_block_reqs_cnt;
	bool restore_needed;

	bool full_init_linereset;
	bool force_host_reset;
	bool invalid_resp_upiu;

#if defined(CONFIG_UFSFEATURE)
	struct ufsf_feature ufsf;
#endif

#if defined(CONFIG_SCSI_SKHPB)
	/* HPB support */
	u32 skhpb_feat;
	int skhpb_state;
	int skhpb_max_regions;
	struct delayed_work skhpb_init_work;
	bool issue_ioctl;
	struct skhpb_lu *skhpb_lup[UFS_UPIU_MAX_GENERAL_LUN];
	struct work_struct skhpb_eh_work;
	u32 skhpb_quirk;
	u8 hpb_control_mode;
#define SKHPB_U8_MAX 0xFF
	u8 skhpb_quicklist_lu_enable[UFS_UPIU_MAX_GENERAL_LUN];
#endif

#if defined(CONFIG_SCSI_SKHPB)
	struct scsi_device *sdev_ufs_lu[UFS_UPIU_MAX_GENERAL_LUN];
#endif

#ifdef CONFIG_SCSI_UFS_CRYPTO
	/* crypto */
	union ufs_crypto_capabilities crypto_capabilities;
	union ufs_crypto_cap_entry *crypto_cap_array;
	u32 crypto_cfg_register;
	struct keyslot_manager *ksm;
	void *crypto_DO_NOT_USE[8];
#endif /* CONFIG_SCSI_UFS_CRYPTO */
};

/* MTK PATCH */
#define ufshcd_set_reg_state(hba, s) ((hba)->vreg_info.state = (s))

/**
 * MTK PATCH
 * ufshcd_scsi_to_upiu_lun - maps scsi LUN to UPIU LUN
 * @scsi_lun: scsi LUN id
 *
 * Returns UPIU LUN id
 */
static inline u8 ufshcd_scsi_to_upiu_lun(unsigned int scsi_lun)
{
	if (scsi_is_wlun(scsi_lun))
		return (scsi_lun & UFS_UPIU_MAX_UNIT_NUM_ID)
			| UFS_UPIU_WLUN_ID;
	else
		return scsi_lun & UFS_UPIU_MAX_UNIT_NUM_ID;
}

/* Returns true if clocks can be gated. Otherwise false */
static inline bool ufshcd_is_clkgating_allowed(struct ufs_hba *hba)
{
	return hba->caps & UFSHCD_CAP_CLK_GATING;
}
static inline bool ufshcd_can_hibern8_during_gating(struct ufs_hba *hba)
{
	return hba->caps & UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;
}
static inline int ufshcd_is_clkscaling_supported(struct ufs_hba *hba)
{
	return hba->caps & UFSHCD_CAP_CLK_SCALING;
}
static inline bool ufshcd_can_autobkops_during_suspend(struct ufs_hba *hba)
{
	return hba->caps & UFSHCD_CAP_AUTO_BKOPS_SUSPEND;
}

static inline bool ufshcd_is_intr_aggr_allowed(struct ufs_hba *hba)
{
/* DWC UFS Core has the Interrupt aggregation feature but is not detectable*/
#ifndef CONFIG_SCSI_UFS_DWC
	if ((hba->caps & UFSHCD_CAP_INTR_AGGR) &&
	    !(hba->quirks & UFSHCD_QUIRK_BROKEN_INTR_AGGR))
		return true;
	else
		return false;
#else
return true;
#endif
}

static inline bool ufshcd_is_auto_hibern8_supported(struct ufs_hba *hba)
{
	return (hba->capabilities & MASK_AUTO_HIBERN8_SUPPORT);
}

#define ufshcd_writel(hba, val, reg)	\
	writel((val), (hba)->mmio_base + (reg))
#define ufshcd_readl(hba, reg)	\
	readl((hba)->mmio_base + (reg))

/**
 * ufshcd_rmwl - read modify write into a register
 * @hba - per adapter instance
 * @mask - mask to apply on read value
 * @val - actual value to write
 * @reg - register address
 */
static inline void ufshcd_rmwl(struct ufs_hba *hba, u32 mask, u32 val, u32 reg)
{
	u32 tmp;

	tmp = ufshcd_readl(hba, reg);
	tmp &= ~mask;
	tmp |= (val & mask);
	ufshcd_writel(hba, tmp, reg);
}

int ufshcd_alloc_host(struct device *, struct ufs_hba **);
void ufshcd_dealloc_host(struct ufs_hba *);
int ufshcd_init(struct ufs_hba * , void __iomem * , unsigned int);
void ufshcd_remove(struct ufs_hba *);
void ufshcd_delay_us(unsigned long us, unsigned long tolerance);
int ufshcd_wait_for_register(struct ufs_hba *hba, u32 reg, u32 mask,
				u32 val, unsigned long interval_us,
				unsigned long timeout_ms, bool can_sleep);

/**
 * MTK PATCH
 * Wrapper function for safely calling variant operations
 */
static inline void ufshcd_vops_auto_hibern8(struct ufs_hba *hba, bool enable)
{
	if (hba->vops && hba->vops->auto_hibern8)
		hba->vops->auto_hibern8(hba, enable);
}

/* MTK PATCH */
static inline void ufshcd_vops_res_ctrl(struct ufs_hba *hba, unsigned int op)
{
	if (hba->vops && hba->vops->res_ctrl)
		hba->vops->res_ctrl(hba, op);
}

/* MTK PATCH */
static inline void ufshcd_vops_deepidle_lock(struct ufs_hba *hba, bool lock)
{
	if (hba->vops && hba->vops->deepidle_lock)
		hba->vops->deepidle_lock(hba, lock);
}

/* MTK PATCH */
static inline void ufshcd_vops_scsi_dev_cfg(struct scsi_device *sdev,
	enum ufs_scsi_dev_cfg op)
{
	struct ufs_hba *hba = shost_priv(sdev->host);

	if (hba->vops && hba->vops->scsi_dev_cfg)
		hba->vops->scsi_dev_cfg(sdev, op);
}

static inline void ufshcd_vops_abort_handler(struct ufs_hba *hba,
					     int tag, char *file, int line)
{
	if (hba->vops && hba->vops->abort_handler)
		hba->vops->abort_handler(hba, tag, file, line);
}

static inline void check_upiu_size(void)
{
	BUILD_BUG_ON(ALIGNED_UPIU_SIZE <
		GENERAL_UPIU_REQUEST_SIZE + QUERY_DESC_MAX_SIZE);
}

/**
 * ufshcd_set_variant - set variant specific data to the hba
 * @hba - per adapter instance
 * @variant - pointer to variant specific data
 */
static inline void ufshcd_set_variant(struct ufs_hba *hba, void *variant)
{
	BUG_ON(!hba);
	hba->priv = variant;
}

/**
 * ufshcd_get_variant - get variant specific data from the hba
 * @hba - per adapter instance
 */
static inline void *ufshcd_get_variant(struct ufs_hba *hba)
{
	BUG_ON(!hba);
	return hba->priv;
}
static inline bool ufshcd_keep_autobkops_enabled_except_suspend(
							struct ufs_hba *hba)
{
	return hba->caps & UFSHCD_CAP_KEEP_AUTO_BKOPS_ENABLED_EXCEPT_SUSPEND;
}

extern int ufshcd_runtime_suspend(struct ufs_hba *hba);
extern int ufshcd_runtime_resume(struct ufs_hba *hba);
extern int ufshcd_runtime_idle(struct ufs_hba *hba);
extern int ufshcd_system_suspend(struct ufs_hba *hba);
extern int ufshcd_system_resume(struct ufs_hba *hba);
extern int ufshcd_shutdown(struct ufs_hba *hba);
extern int ufshcd_dme_set_attr(struct ufs_hba *hba, u32 attr_sel,
			       u8 attr_set, u32 mib_val, u8 peer);
extern int ufshcd_dme_get_attr(struct ufs_hba *hba, u32 attr_sel,
			       u32 *mib_val, u8 peer);

/* UIC command interfaces for DME primitives */
#define DME_LOCAL	0
#define DME_PEER	1
#define ATTR_SET_NOR	0	/* NORMAL */
#define ATTR_SET_ST	1	/* STATIC */

static inline int ufshcd_dme_set(struct ufs_hba *hba, u32 attr_sel,
				 u32 mib_val)
{
	return ufshcd_dme_set_attr(hba, attr_sel, ATTR_SET_NOR,
				   mib_val, DME_LOCAL);
}

static inline int ufshcd_dme_st_set(struct ufs_hba *hba, u32 attr_sel,
				    u32 mib_val)
{
	return ufshcd_dme_set_attr(hba, attr_sel, ATTR_SET_ST,
				   mib_val, DME_LOCAL);
}

static inline int ufshcd_dme_peer_set(struct ufs_hba *hba, u32 attr_sel,
				      u32 mib_val)
{
	return ufshcd_dme_set_attr(hba, attr_sel, ATTR_SET_NOR,
				   mib_val, DME_PEER);
}

static inline int ufshcd_dme_peer_st_set(struct ufs_hba *hba, u32 attr_sel,
					 u32 mib_val)
{
	return ufshcd_dme_set_attr(hba, attr_sel, ATTR_SET_ST,
				   mib_val, DME_PEER);
}

static inline int ufshcd_dme_get(struct ufs_hba *hba,
				 u32 attr_sel, u32 *mib_val)
{
	return ufshcd_dme_get_attr(hba, attr_sel, mib_val, DME_LOCAL);
}

static inline int ufshcd_dme_peer_get(struct ufs_hba *hba,
				      u32 attr_sel, u32 *mib_val)
{
	return ufshcd_dme_get_attr(hba, attr_sel, mib_val, DME_PEER);
}

/* MTK PATCH */
extern struct ufs_pm_lvl_states ufs_pm_lvl_states[];
extern const int ufs_pm_lvl_states_size;

/* MTK PATCH */
int ufshcd_read_health_desc(struct ufs_hba *hba, u8 *buf, u32 size);

static inline bool ufshcd_is_hs_mode(struct ufs_pa_layer_attr *pwr_info)
{
	return (pwr_info->pwr_rx == FAST_MODE ||
		pwr_info->pwr_rx == FASTAUTO_MODE) &&
		(pwr_info->pwr_tx == FAST_MODE ||
		pwr_info->pwr_tx == FASTAUTO_MODE);
}

/* Expose Query-Request API */
int ufshcd_query_flag(struct ufs_hba *hba, enum query_opcode opcode,
	enum flag_idn idn, bool *flag_res);
int ufshcd_hold(struct ufs_hba *hba, bool async);
void ufshcd_release(struct ufs_hba *hba);
#if defined(CONFIG_UFSFEATURE)
int ufshcd_exec_dev_cmd(struct ufs_hba *hba,
			enum dev_cmd_type cmd_type, int timeout);
int ufshcd_comp_scsi_upiu(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
int ufshcd_map_sg(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
#endif

#if defined(CONFIG_SCSI_SKHPB)
int ufshcd_query_flag_retry(struct ufs_hba *hba,
	enum query_opcode opcode, enum flag_idn idn, bool *flag_res);
#endif
int ufshcd_map_desc_id_to_length(struct ufs_hba *hba, enum desc_idn desc_id,
	int *desc_length);

u32 ufshcd_get_local_unipro_ver(struct ufs_hba *hba);

/**
 * MTK PATCH:
 *
 * API prototypes for MTK vendor-specific usage.
 */
int ufshcd_clock_scaling_prepare(struct ufs_hba *hba);
void ufshcd_clock_scaling_unprepare(struct ufs_hba *hba);
void ufshcd_enable_intr(struct ufs_hba *hba, u32 intrs);
void ufshcd_disable_intr(struct ufs_hba *hba, u32 intrs);
int ufshcd_hba_enable(struct ufs_hba *hba);
int ufshcd_make_hba_operational(struct ufs_hba *hba);
void ufshcd_print_host_state(struct ufs_hba *hba,
	u32 mphy_info, struct seq_file *m, char **buff, unsigned long *size);
void ufshcd_print_all_err_hist(struct ufs_hba *hba,
	struct seq_file *m, char **buff, unsigned long *size);
int ufshcd_query_attr(struct ufs_hba *hba,
	enum query_opcode opcode,
	enum attr_idn idn,
	u8 index,
	u8 selector,
	u32 *attr_val);
int ufshcd_query_flag(struct ufs_hba *hba, enum query_opcode opcode,
	enum flag_idn idn, bool *flag_res);
int ufshcd_send_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd);
int ufshcd_check_hibern8_exit(struct ufs_hba *hba); /* MTK PATCH */
int ufshcd_uic_hibern8_exit(struct ufs_hba *hba);
int ufshcd_query_descriptor_retry(struct ufs_hba *hba,
	enum query_opcode opcode, enum desc_idn idn, u8 index,
	u8 selector, u8 *desc_buf, int *buf_len);
int ufshcd_rpmb_security_out(struct scsi_device *sdev,
			 struct rpmb_frame *frames, u32 cnt);
int ufshcd_rpmb_security_in(struct scsi_device *sdev,
			struct rpmb_frame *frames, u32 cnt);
void ufshcd_update_reg_hist(struct ufs_err_reg_hist *reg_hist,
			u32 reg);

/**
 * ufshcd_upiu_wlun_to_scsi_wlun - maps UPIU W-LUN id to SCSI W-LUN ID
 * @scsi_lun: UPIU W-LUN id
 *
 * Returns SCSI W-LUN id
 */
static inline u16 ufshcd_upiu_wlun_to_scsi_wlun(u8 upiu_wlun_id)
{
	return (upiu_wlun_id & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE;
}

/* Wrapper functions for safely calling variant operations */
static inline const char *ufshcd_get_var_name(struct ufs_hba *hba)
{
	if (hba->vops)
		return hba->vops->name;
	return "";
}

static inline int ufshcd_vops_init(struct ufs_hba *hba)
{
	if (hba->vops && hba->vops->init)
		return hba->vops->init(hba);

	return 0;
}

static inline void ufshcd_vops_exit(struct ufs_hba *hba)
{
	if (hba->vops && hba->vops->exit)
		return hba->vops->exit(hba);
}

static inline u32 ufshcd_vops_get_ufs_hci_version(struct ufs_hba *hba)
{
	if (hba->vops && hba->vops->get_ufs_hci_version)
		return hba->vops->get_ufs_hci_version(hba);

	return ufshcd_readl(hba, REG_UFS_VERSION);
}

static inline int ufshcd_vops_clk_scale_notify(struct ufs_hba *hba,
			bool up, enum ufs_notify_change_status status)
{
	if (hba->vops && hba->vops->clk_scale_notify)
		return hba->vops->clk_scale_notify(hba, up, status);
	return 0;
}

static inline int ufshcd_vops_setup_clocks(struct ufs_hba *hba, bool on,
					enum ufs_notify_change_status status)
{
	if (hba->vops && hba->vops->setup_clocks)
		return hba->vops->setup_clocks(hba, on, status);
	return 0;
}

static inline int ufshcd_vops_setup_regulators(struct ufs_hba *hba, bool status)
{
	if (hba->vops && hba->vops->setup_regulators)
		return hba->vops->setup_regulators(hba, status);

	return 0;
}

static inline int ufshcd_vops_hce_enable_notify(struct ufs_hba *hba,
						bool status)
{
	if (hba->vops && hba->vops->hce_enable_notify)
		return hba->vops->hce_enable_notify(hba, status);

	return 0;
}
static inline int ufshcd_vops_link_startup_notify(struct ufs_hba *hba,
						bool status)
{
	if (hba->vops && hba->vops->link_startup_notify)
		return hba->vops->link_startup_notify(hba, status);

	return 0;
}

static inline int ufshcd_vops_pwr_change_notify(struct ufs_hba *hba,
				  bool status,
				  struct ufs_pa_layer_attr *dev_max_params,
				  struct ufs_pa_layer_attr *dev_req_params)
{
	if (hba->vops && hba->vops->pwr_change_notify)
		return hba->vops->pwr_change_notify(hba, status,
					dev_max_params, dev_req_params);

	return -ENOTSUPP;
}

static inline void ufshcd_vops_setup_xfer_req(struct ufs_hba *hba, int tag,
					bool is_scsi_cmd)
{
	if (hba->vops && hba->vops->setup_xfer_req)
		return hba->vops->setup_xfer_req(hba, tag, is_scsi_cmd);
}

static inline void ufshcd_vops_setup_task_mgmt(struct ufs_hba *hba,
					int tag, u8 tm_function)
{
	if (hba->vops && hba->vops->setup_task_mgmt)
		return hba->vops->setup_task_mgmt(hba, tag, tm_function);
}

static inline void ufshcd_vops_hibern8_notify(struct ufs_hba *hba,
					enum uic_cmd_dme cmd,
					enum ufs_notify_change_status status)
{
	if (hba->vops && hba->vops->hibern8_notify)
		return hba->vops->hibern8_notify(hba, cmd, status);
}

static inline int ufshcd_vops_apply_dev_quirks(struct ufs_hba *hba)
{
	if (hba->vops && hba->vops->apply_dev_quirks)
		return hba->vops->apply_dev_quirks(hba);
	return 0;
}

static inline int ufshcd_vops_suspend(struct ufs_hba *hba, enum ufs_pm_op op)
{
	if (hba->vops && hba->vops->suspend)
		return hba->vops->suspend(hba, op);

	return 0;
}

static inline int ufshcd_vops_resume(struct ufs_hba *hba, enum ufs_pm_op op)
{
	if (hba->vops && hba->vops->resume)
		return hba->vops->resume(hba, op);

	return 0;
}

static inline void ufshcd_vops_dbg_register_dump(struct ufs_hba *hba)
{
	if (hba->vops && hba->vops->dbg_register_dump)
		hba->vops->dbg_register_dump(hba);
}

static inline void ufshcd_vops_device_reset(struct ufs_hba *hba)
{
	if (hba->vops && hba->vops->device_reset) {
		hba->vops->device_reset(hba);
		ufshcd_update_reg_hist(&hba->ufs_stats.dev_reset, 0);
	}
}
#endif /* End of Header */
