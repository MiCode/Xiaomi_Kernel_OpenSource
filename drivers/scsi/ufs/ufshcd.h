/*
 * Universal Flash Storage Host controller driver
 *
 * This code is based on drivers/scsi/ufs/ufshcd.h
 * Copyright (C) 2011-2013 Samsung India Software Operations
 * Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
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
#include <linux/hrtimer.h>
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
#include <linux/reset.h>
#include <linux/extcon.h>
#include "unipro.h"

#include <asm/irq.h>
#include <asm/byteorder.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_eh.h>

#include <linux/fault-inject.h>

#include "ufs.h"
#include "ufshci.h"

#define UFSHCD "ufshcd"
#define UFSHCD_DRIVER_VERSION "0.3"

#define UFS_BIT(x)	BIT(x)
#define UFS_MASK(x, y)	(x << ((y) % BITS_PER_LONG))

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

enum {
	/* errors which require the host controller reset for recovery */
	UFS_ERR_HIBERN8_EXIT,
	UFS_ERR_VOPS_SUSPEND,
	UFS_ERR_EH,
	UFS_ERR_CLEAR_PEND_XFER_TM,
	UFS_ERR_INT_FATAL_ERRORS,
	UFS_ERR_INT_UIC_ERROR,
	UFS_ERR_CRYPTO_ENGINE,

	/* other errors */
	UFS_ERR_HIBERN8_ENTER,
	UFS_ERR_RESUME,
	UFS_ERR_SUSPEND,
	UFS_ERR_LINKSTARTUP,
	UFS_ERR_POWER_MODE_CHANGE,
	UFS_ERR_TASK_ABORT,
	UFS_ERR_MAX,
};

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
 * @complete_time_stamp: time stamp for statistics
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
	ktime_t issue_time_stamp;
	ktime_t complete_time_stamp;

	bool req_abort_skip;
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

/**
 * struct ufs_hba_variant_ops - variant specific callbacks
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
 * @setup_task_mgmt: called before any task management request is issued
 *                  to set some things
 * @hibern8_notify: called around hibern8 enter/exit
 * @apply_dev_quirks: called to apply device specific quirks
 * @suspend: called during host controller PM callback
 * @resume: called during host controller PM callback
 * @full_reset:  called during link recovery for handling variant specific
 *		 implementations of resetting the hci
 * @update_sec_cfg: called to restore host controller secure configuration
 * @get_scale_down_gear: called to get the minimum supported gear to
 *			 scale down
 * @set_bus_vote: called to vote for the required bus bandwidth
 * @phy_initialization: used to initialize phys
 */
struct ufs_hba_variant_ops {
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
	int	(*full_reset)(struct ufs_hba *);
	void	(*dbg_register_dump)(struct ufs_hba *hba, bool no_sleep);
	int	(*update_sec_cfg)(struct ufs_hba *hba, bool restore_sec_cfg);
	u32	(*get_scale_down_gear)(struct ufs_hba *);
	int	(*set_bus_vote)(struct ufs_hba *, bool);
	int	(*phy_initialization)(struct ufs_hba *);
#ifdef CONFIG_DEBUG_FS
	void	(*add_debugfs)(struct ufs_hba *hba, struct dentry *root);
	void	(*remove_debugfs)(struct ufs_hba *hba);
#endif
};

/**
 * struct ufs_hba_crypto_variant_ops - variant specific crypto callbacks
 * @crypto_req_setup:	retreieve the necessary cryptographic arguments to setup
			a requests's transfer descriptor.
 * @crypto_engine_cfg_start: start configuring cryptographic engine
 *							 according to tag
 *							 parameter
 * @crypto_engine_cfg_end: end configuring cryptographic engine
 *						   according to tag parameter
 * @crypto_engine_reset: perform reset to the cryptographic engine
 * @crypto_engine_get_status: get errors status of the cryptographic engine
 */
struct ufs_hba_crypto_variant_ops {
	int	(*crypto_req_setup)(struct ufs_hba *, struct ufshcd_lrb *lrbp,
				    u8 *cc_index, bool *enable, u64 *dun);
	int	(*crypto_engine_cfg_start)(struct ufs_hba *, unsigned int);
	int	(*crypto_engine_cfg_end)(struct ufs_hba *, struct ufshcd_lrb *,
			struct request *);
	int	(*crypto_engine_reset)(struct ufs_hba *);
	int	(*crypto_engine_get_status)(struct ufs_hba *, u32 *);
};

/**
* struct ufs_hba_pm_qos_variant_ops - variant specific PM QoS callbacks
*/
struct ufs_hba_pm_qos_variant_ops {
	void		(*req_start)(struct ufs_hba *, struct request *);
	void		(*req_end)(struct ufs_hba *, struct request *, bool);
};

/**
 * struct ufs_hba_variant - variant specific parameters
 * @name: variant name
 */
struct ufs_hba_variant {
	struct device				*dev;
	const char				*name;
	struct ufs_hba_variant_ops		*vops;
	struct ufs_hba_crypto_variant_ops	*crypto_vops;
	struct ufs_hba_pm_qos_variant_ops	*pm_qos_vops;
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
 * @gate_hrtimer: hrtimer to invoke @gate_work after some delay as
 * specified in @delay_ms
 * @gate_work: worker to turn off clocks
 * @ungate_work: worker to turn on clocks that will be used in case of
 * interrupt context
 * @state: the current clocks state
 * @delay_ms: current gating delay in ms
 * @delay_ms_pwr_save: gating delay (in ms) in power save mode
 * @delay_ms_perf: gating delay (in ms) in performance mode
 * @is_suspended: clk gating is suspended when set to 1 which can be used
 * during suspend/resume
 * @delay_attr: sysfs attribute to control delay_ms if clock scaling is disabled
 * @delay_pwr_save_attr: sysfs attribute to control delay_ms_pwr_save
 * @delay_perf_attr: sysfs attribute to control delay_ms_perf
 * @enable_attr: sysfs attribute to enable/disable clock gating
 * @is_enabled: Indicates the current status of clock gating
 * @active_reqs: number of requests that are pending and should be waited for
 * completion before gating clocks.
 */
struct ufs_clk_gating {
	struct hrtimer gate_hrtimer;
	struct work_struct gate_work;
	struct work_struct ungate_work;
	enum clk_gating_state state;
	unsigned long delay_ms;
	unsigned long delay_ms_pwr_save;
	unsigned long delay_ms_perf;
	bool is_suspended;
	struct device_attribute delay_attr;
	struct device_attribute delay_pwr_save_attr;
	struct device_attribute delay_perf_attr;
	struct device_attribute enable_attr;
	bool is_enabled;
	int active_reqs;
	struct workqueue_struct *clk_gating_workq;
};

struct ufs_saved_pwr_info {
	struct ufs_pa_layer_attr info;
	bool is_valid;
};

/* Hibern8 state  */
enum ufshcd_hibern8_on_idle_state {
	HIBERN8_ENTERED,
	HIBERN8_EXITED,
	REQ_HIBERN8_ENTER,
	REQ_HIBERN8_EXIT,
	AUTO_HIBERN8,
};

/**
 * struct ufs_hibern8_on_idle - UFS Hibern8 on idle related data
 * @enter_work: worker to put UFS link in hibern8 after some delay as
 * specified in delay_ms
 * @exit_work: worker to bring UFS link out of hibern8
 * @state: the current hibern8 state
 * @delay_ms: hibern8 enter delay in ms
 * @is_suspended: hibern8 enter is suspended when set to 1 which can be used
 * during suspend/resume
 * @active_reqs: number of requests that are pending and should be waited for
 * completion before scheduling delayed "enter_work".
 * @delay_attr: sysfs attribute to control delay_attr
 * @enable_attr: sysfs attribute to enable/disable hibern8 on idle
 * @is_enabled: Indicates the current status of hibern8
 */
struct ufs_hibern8_on_idle {
	struct delayed_work enter_work;
	struct work_struct exit_work;
	enum ufshcd_hibern8_on_idle_state state;
	unsigned long delay_ms;
	bool is_suspended;
	int active_reqs;
	struct device_attribute delay_attr;
	struct device_attribute enable_attr;
	bool is_enabled;
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
 * @is_scaled_up: tracks if we are currently scaled up or scaled down
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
	bool is_scaled_up;
};

#define UIC_ERR_REG_HIST_LENGTH 20
/**
 * struct ufs_uic_err_reg_hist - keeps history of uic errors
 * @pos: index to indicate cyclic buffer position
 * @reg: cyclic buffer for registers value
 * @tstamp: cyclic buffer for time stamp
 */
struct ufs_uic_err_reg_hist {
	int pos;
	u32 reg[UIC_ERR_REG_HIST_LENGTH];
	ktime_t tstamp[UIC_ERR_REG_HIST_LENGTH];
};

#ifdef CONFIG_DEBUG_FS
struct debugfs_files {
	struct dentry *debugfs_root;
	struct dentry *stats_folder;
	struct dentry *tag_stats;
	struct dentry *err_stats;
	struct dentry *show_hba;
	struct dentry *host_regs;
	struct dentry *dump_dev_desc;
	struct dentry *power_mode;
	struct dentry *dme_local_read;
	struct dentry *dme_peer_read;
	struct dentry *dbg_print_en;
	struct dentry *req_stats;
	struct dentry *query_stats;
	u32 dme_local_attr_id;
	u32 dme_peer_attr_id;
	struct dentry *reset_controller;
	struct dentry *err_state;
	bool err_occurred;
#ifdef CONFIG_UFS_FAULT_INJECTION
	struct dentry *err_inj_scenario;
	struct dentry *err_inj_stats;
	u32 err_inj_scenario_mask;
	struct fault_attr fail_attr;
#endif
};

/* tag stats statistics types */
enum ts_types {
	TS_NOT_SUPPORTED	= -1,
	TS_TAG			= 0,
	TS_READ			= 1,
	TS_WRITE		= 2,
	TS_URGENT_READ		= 3,
	TS_URGENT_WRITE		= 4,
	TS_FLUSH		= 5,
	TS_NUM_STATS		= 6,
};

/**
 * struct ufshcd_req_stat - statistics for request handling times (in usec)
 * @min: shortest time measured
 * @max: longest time measured
 * @sum: sum of all the handling times measured (used for average calculation)
 * @count: number of measurements taken
 */
struct ufshcd_req_stat {
	u64 min;
	u64 max;
	u64 sum;
	u64 count;
};
#endif

enum ufshcd_ctx {
	QUEUE_CMD,
	ERR_HNDLR_WORK,
	H8_EXIT_WORK,
	UIC_CMD_SEND,
	PWRCTL_CMD_SEND,
	TM_CMD_SEND,
	XFR_REQ_COMPL,
	CLK_SCALE_WORK,
	DBGFS_CFG_PWR_MODE,
};

struct ufshcd_clk_ctx {
	ktime_t ts;
	enum ufshcd_ctx ctx;
};

/**
 * struct ufs_stats - keeps usage/err statistics
 * @enabled: enable tag stats for debugfs
 * @tag_stats: pointer to tag statistic counters
 * @q_depth: current amount of busy slots
 * @err_stats: counters to keep track of various errors
 * @req_stats: request handling time statistics per request type
 * @query_stats_arr: array that holds query statistics
 * @hibern8_exit_cnt: Counter to keep track of number of exits,
 *		reset this after link-startup.
 * @last_hibern8_exit_tstamp: Set time after the hibern8 exit.
 *		Clear after the first successful command completion.
 * @pa_err: tracks pa-uic errors
 * @dl_err: tracks dl-uic errors
 * @nl_err: tracks nl-uic errors
 * @tl_err: tracks tl-uic errors
 * @dme_err: tracks dme errors
 */
struct ufs_stats {
#ifdef CONFIG_DEBUG_FS
	bool enabled;
	u64 **tag_stats;
	int q_depth;
	int err_stats[UFS_ERR_MAX];
	struct ufshcd_req_stat req_stats[TS_NUM_STATS];
	int query_stats_arr[UPIU_QUERY_OPCODE_MAX][MAX_QUERY_IDN];

#endif
	u32 last_intr_status;
	ktime_t last_intr_ts;
	struct ufshcd_clk_ctx clk_hold;
	struct ufshcd_clk_ctx clk_rel;
	u32 hibern8_exit_cnt;
	ktime_t last_hibern8_exit_tstamp;
	u32 power_mode_change_cnt;
	struct ufs_uic_err_reg_hist pa_err;
	struct ufs_uic_err_reg_hist dl_err;
	struct ufs_uic_err_reg_hist nl_err;
	struct ufs_uic_err_reg_hist tl_err;
	struct ufs_uic_err_reg_hist dme_err;
	u32 pa_err_cnt_total;
	u32 pa_err_cnt[UFS_EC_PA_MAX];
	u32 dl_err_cnt_total;
	u32 dl_err_cnt[UFS_EC_DL_MAX];
	u32 dme_err_cnt;
};

/* UFS Host Controller debug print bitmask */
#define UFSHCD_DBG_PRINT_CLK_FREQ_EN		UFS_BIT(0)
#define UFSHCD_DBG_PRINT_UIC_ERR_HIST_EN	UFS_BIT(1)
#define UFSHCD_DBG_PRINT_HOST_REGS_EN		UFS_BIT(2)
#define UFSHCD_DBG_PRINT_TRS_EN			UFS_BIT(3)
#define UFSHCD_DBG_PRINT_TMRS_EN		UFS_BIT(4)
#define UFSHCD_DBG_PRINT_PWR_EN			UFS_BIT(5)
#define UFSHCD_DBG_PRINT_HOST_STATE_EN		UFS_BIT(6)

#define UFSHCD_DBG_PRINT_ALL						   \
		(UFSHCD_DBG_PRINT_CLK_FREQ_EN		|		   \
		 UFSHCD_DBG_PRINT_UIC_ERR_HIST_EN	|		   \
		 UFSHCD_DBG_PRINT_HOST_REGS_EN | UFSHCD_DBG_PRINT_TRS_EN | \
		 UFSHCD_DBG_PRINT_TMRS_EN | UFSHCD_DBG_PRINT_PWR_EN |	   \
		 UFSHCD_DBG_PRINT_HOST_STATE_EN)

struct ufshcd_cmd_log_entry {
	char *str;	/* context like "send", "complete" */
	char *cmd_type;	/* "scsi", "query", "nop", "dme" */
	u8 lun;
	u8 cmd_id;
	sector_t lba;
	int transfer_len;
	u8 idn;		/* used only for query idn */
	u32 doorbell;
	u32 outstanding_reqs;
	u32 seq_num;
	unsigned int tag;
	ktime_t tstamp;
};

struct ufshcd_cmd_log {
	struct ufshcd_cmd_log_entry *entries;
	int pos;
	u32 seq_num;
};

/* UFS card state - hotplug state */
enum ufshcd_card_state {
	UFS_CARD_STATE_UNKNOWN	= 0,
	UFS_CARD_STATE_ONLINE	= 1,
	UFS_CARD_STATE_OFFLINE	= 2,
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
 * @lrb: local reference block
 * @lrb_in_use: lrb in use
 * @outstanding_tasks: Bits representing outstanding task requests
 * @outstanding_reqs: Bits representing outstanding transfer requests
 * @capabilities: UFS Controller Capabilities
 * @nutrs: Transfer Request Queue depth supported by controller
 * @nutmrs: Task Management Queue depth supported by controller
 * @ufs_version: UFS Version to which controller complies
 * @var: pointer to variant specific data
 * @priv: pointer to variant specific private data
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
 * @eh_work: Worker to handle UFS errors that require s/w attention
 * @eeh_work: Worker to handle exception events
 * @errors: HBA errors
 * @uic_error: UFS interconnect layer error status
 * @saved_err: sticky error mask
 * @saved_uic_err: sticky UIC error mask
 * @dev_cmd: ufs device management command information
 * @last_dme_cmd_tstamp: time stamp of the last completed DME command
 * @auto_bkops_enabled: to track whether bkops is enabled in device
 * @ufs_stats: ufshcd statistics to be used via debugfs
 * @debugfs_files: debugfs files associated with the ufs stats
 * @ufshcd_dbg_print: Bitmask for enabling debug prints
 * @extcon: pointer to external connector device
 * @card_detect_nb: card detector notifier registered with @extcon
 * @card_detect_work: work to exectute the card detect function
 * @card_state: card state event, enum ufshcd_card_state defines possible states
 * @vreg_info: UFS device voltage regulator information
 * @clk_list_head: UFS host controller clocks list node head
 * @pwr_info: holds current power mode
 * @max_pwr_info: keeps the device max valid pwm
 * @hibern8_on_idle: UFS Hibern8 on idle related data
 * @desc_size: descriptor sizes reported by device
 * @urgent_bkops_lvl: keeps track of urgent bkops level for device
 * @is_urgent_bkops_lvl_checked: keeps track if the urgent bkops level for
 *  device is known or not.
 * @scsi_block_reqs_cnt: reference counting for scsi block requests
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

	enum ufs_dev_pwr_mode curr_dev_pwr_mode;
	enum uic_link_state uic_link_state;
	/* Desired UFS power management level during runtime PM */
	int rpm_lvl;
	/* Desired UFS power management level during system PM */
	int spm_lvl;
	struct device_attribute rpm_lvl_attr;
	struct device_attribute spm_lvl_attr;
	int pm_op_in_progress;

	struct ufshcd_lrb *lrb;
	unsigned long lrb_in_use;

	unsigned long outstanding_tasks;
	unsigned long outstanding_reqs;

	u32 capabilities;
	int nutrs;
	int nutmrs;
	u32 ufs_version;
	struct ufs_hba_variant *var;
	void *priv;
	unsigned int irq;
	bool is_irq_enabled;
	bool crash_on_err;

	u32 dev_ref_clk_gating_wait;
	u32 dev_ref_clk_freq;

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

	/* HIBERN8 support is broken */
	#define UFSHCD_QUIRK_BROKEN_HIBERN8			UFS_BIT(8)

	/*
	 * UFS controller version register (VER) wrongly advertise the version
	 * as v1.0 though controller implementation is as per UFSHCI v1.1
	 * specification.
	 */
	#define UFSHCD_QUIRK_BROKEN_VER_REG_1_1			UFS_BIT(9)

	/* UFSHC advertises 64-bit not supported even though it supports */
	#define UFSHCD_QUIRK_BROKEN_CAP_64_BIT_0		UFS_BIT(10)

	/*
	 * If LCC (Line Control Command) are having issue on the host
	 * controller then enable this quirk. Note that connected UFS device
	 * should also have workaround to not expect LCC commands from host.
	 */
	#define UFSHCD_BROKEN_LCC				UFS_BIT(11)

	/*
	 * If UFS device is having issue in processing LCC (Line Control
	 * Command) coming from UFS host controller then enable this quirk.
	 * When this quirk is enabled, host controller driver should disable
	 * the LCC transmission on UFS host controller (by clearing
	 * TX_LCC_ENABLE attribute of host to 0).
	 */
	#define UFSHCD_BROKEN_LCC_PROCESSING_ON_DEVICE		UFS_BIT(12)

	#define UFSHCD_BROKEN_LCC_PROCESSING_ON_HOST		UFS_BIT(13)

	#define UFSHCD_QUIRK_DME_PEER_GET_FAST_MODE		UFS_BIT(14)

	/* Auto hibern8 support is broken */
	#define UFSHCD_QUIRK_BROKEN_AUTO_HIBERN8		UFS_BIT(15)

	unsigned int quirks;	/* Deviations from standard UFSHCI spec. */

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
	bool is_powered;

	/* Work Queues */
	struct work_struct eh_work;
	struct work_struct eeh_work;
	struct work_struct rls_work;

	/* HBA Errors */
	u32 errors;
	u32 uic_error;
	u32 ce_error;	/* crypto engine errors */
	u32 saved_err;
	u32 saved_uic_err;
	u32 saved_ce_err;
	bool silence_err_logs;
	bool force_host_reset;
	bool auto_h8_err;
	struct ufs_stats ufs_stats;

	/* Device management request data */
	struct ufs_dev_cmd dev_cmd;
	ktime_t last_dme_cmd_tstamp;

	/* Keeps information of the UFS device connected to this host */
	struct ufs_dev_info dev_info;
	bool auto_bkops_enabled;

#ifdef CONFIG_DEBUG_FS
	struct debugfs_files debugfs_files;
#endif

	struct ufs_vreg_info vreg_info;
	struct list_head clk_list_head;

	bool wlun_dev_clr_ua;

	/* Number of requests aborts */
	int req_abort_count;

	/* Number of lanes available (1 or 2) for Rx/Tx */
	u32 lanes_per_direction;

	/* Gear limits */
	u32 limit_tx_hs_gear;
	u32 limit_rx_hs_gear;
	u32 limit_tx_pwm_gear;
	u32 limit_rx_pwm_gear;

	u32 scsi_cmd_timeout;

	/* Bitmask for enabling debug prints */
	u32 ufshcd_dbg_print;

	struct extcon_dev *extcon;
	struct notifier_block card_detect_nb;
	struct work_struct card_detect_work;
	atomic_t card_state;

	struct ufs_pa_layer_attr pwr_info;
	struct ufs_pwr_mode_info max_pwr_info;

	struct ufs_clk_gating clk_gating;
	struct ufs_hibern8_on_idle hibern8_on_idle;
	struct ufshcd_cmd_log cmd_log;

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
	/* Allow standalone Hibern8 enter on idle */
#define UFSHCD_CAP_HIBERN8_ENTER_ON_IDLE (1 << 5)

	/*
	 * This capability allows the device auto-bkops to be always enabled
	 * except during suspend (both runtime and suspend).
	 * Enabling this capability means that device will always be allowed
	 * to do background operation when it's active but it might degrade
	 * the performance of ongoing read/write operations.
	 */
#define UFSHCD_CAP_KEEP_AUTO_BKOPS_ENABLED_EXCEPT_SUSPEND (1 << 6)
	/*
	 * If host controller hardware can be power collapsed when UFS link is
	 * in hibern8 then enable this cap.
	 */
#define UFSHCD_CAP_POWER_COLLAPSE_DURING_HIBERN8 (1 << 7)

	struct devfreq *devfreq;
	struct ufs_clk_scaling clk_scaling;
	bool is_sys_suspended;

	enum bkops_status urgent_bkops_lvl;
	bool is_urgent_bkops_lvl_checked;

	/* sync b/w diff contexts */
	struct rw_semaphore lock;
	unsigned long shutdown_in_prog;

	/* If set, don't gate device ref_clk during clock gating */
	bool no_ref_clk_gating;

	int scsi_block_reqs_cnt;

	bool full_init_linereset;
	struct pinctrl *pctrl;

	struct reset_control *core_reset;

	struct ufs_desc_size desc_size;
	bool restore_needed;

	int latency_hist_enabled;
	struct io_latency_state io_lat_s;

	bool reinit_g4_rate_A;
	bool force_g4;
};

static inline void ufshcd_mark_shutdown_ongoing(struct ufs_hba *hba)
{
	set_bit(0, &hba->shutdown_in_prog);
}

static inline bool ufshcd_is_shutdown_ongoing(struct ufs_hba *hba)
{
	return !!(test_bit(0, &hba->shutdown_in_prog));
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
static inline bool ufshcd_is_hibern8_on_idle_allowed(struct ufs_hba *hba)
{
	return hba->caps & UFSHCD_CAP_HIBERN8_ENTER_ON_IDLE;
}

static inline bool ufshcd_is_power_collapse_during_hibern8_allowed(
						struct ufs_hba *hba)
{
	return !!(hba->caps & UFSHCD_CAP_POWER_COLLAPSE_DURING_HIBERN8);
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
	return !!((hba->capabilities & MASK_AUTO_HIBERN8_SUPPORT) &&
		!(hba->quirks & UFSHCD_QUIRK_BROKEN_AUTO_HIBERN8));
}

static inline bool ufshcd_is_crypto_supported(struct ufs_hba *hba)
{
	return !!(hba->capabilities & MASK_CRYPTO_SUPPORT);
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
int ufshcd_wait_for_register(struct ufs_hba *hba, u32 reg, u32 mask,
				u32 val, unsigned long interval_us,
				unsigned long timeout_ms, bool can_sleep);
int ufshcd_uic_hibern8_enter(struct ufs_hba *hba);
int ufshcd_uic_hibern8_exit(struct ufs_hba *hba);

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
extern int ufshcd_scale_clks(struct ufs_hba *hba, bool scale_up);

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

/**
 * ufshcd_dme_rmw - get modify set a dme attribute
 * @hba - per adapter instance
 * @mask - mask to apply on read value
 * @val - actual value to write
 * @attr - dme attribute
 */
static inline int ufshcd_dme_rmw(struct ufs_hba *hba, u32 mask,
				 u32 val, u32 attr)
{
	u32 cfg = 0;
	int err = 0;

	err = ufshcd_dme_get(hba, UIC_ARG_MIB(attr), &cfg);
	if (err)
		goto out;

	cfg &= ~mask;
	cfg |= (val & mask);

	err = ufshcd_dme_set(hba, UIC_ARG_MIB(attr), cfg);

out:
	return err;
}

int ufshcd_read_device_desc(struct ufs_hba *hba, u8 *buf, u32 size);

static inline bool ufshcd_is_hs_mode(struct ufs_pa_layer_attr *pwr_info)
{
	return (pwr_info->pwr_rx == FAST_MODE ||
		pwr_info->pwr_rx == FASTAUTO_MODE) &&
		(pwr_info->pwr_tx == FAST_MODE ||
		pwr_info->pwr_tx == FASTAUTO_MODE);
}

static inline bool ufshcd_is_embedded_dev(struct ufs_hba *hba)
{
	if ((hba->dev_info.b_device_sub_class == UFS_DEV_EMBEDDED_BOOTABLE) ||
	    (hba->dev_info.b_device_sub_class == UFS_DEV_EMBEDDED_NON_BOOTABLE))
		return true;
	return false;
}

#ifdef CONFIG_DEBUG_FS
static inline void ufshcd_init_req_stats(struct ufs_hba *hba)
{
	memset(hba->ufs_stats.req_stats, 0, sizeof(hba->ufs_stats.req_stats));
}
#else
static inline void ufshcd_init_req_stats(struct ufs_hba *hba) {}
#endif

/* Expose Query-Request API */
int ufshcd_query_flag(struct ufs_hba *hba, enum query_opcode opcode,
	enum flag_idn idn, bool *flag_res);
int ufshcd_query_attr(struct ufs_hba *hba, enum query_opcode opcode,
	enum attr_idn idn, u8 index, u8 selector, u32 *attr_val);
int ufshcd_query_descriptor_retry(struct ufs_hba *hba, enum query_opcode opcode,
	enum desc_idn idn, u8 index, u8 selector, u8 *desc_buf, int *buf_len);

int ufshcd_hold(struct ufs_hba *hba, bool async);
void ufshcd_release(struct ufs_hba *hba, bool no_sched);
int ufshcd_wait_for_doorbell_clr(struct ufs_hba *hba, u64 wait_timeout_us);
int ufshcd_change_power_mode(struct ufs_hba *hba,
			     struct ufs_pa_layer_attr *pwr_mode);
void ufshcd_abort_outstanding_transfer_requests(struct ufs_hba *hba,
		int result);

int ufshcd_map_desc_id_to_length(struct ufs_hba *hba, enum desc_idn desc_id,
	int *desc_length);

u32 ufshcd_get_local_unipro_ver(struct ufs_hba *hba);

void ufshcd_scsi_block_requests(struct ufs_hba *hba);
void ufshcd_scsi_unblock_requests(struct ufs_hba *hba);

/* Wrapper functions for safely calling variant operations */
static inline const char *ufshcd_get_var_name(struct ufs_hba *hba)
{
	if (hba->var && hba->var->name)
		return hba->var->name;
	return "";
}

static inline int ufshcd_vops_init(struct ufs_hba *hba)
{
	if (hba->var && hba->var->vops && hba->var->vops->init)
		return hba->var->vops->init(hba);
	return 0;
}

static inline void ufshcd_vops_exit(struct ufs_hba *hba)
{
	if (hba->var && hba->var->vops && hba->var->vops->exit)
		hba->var->vops->exit(hba);
}

static inline u32 ufshcd_vops_get_ufs_hci_version(struct ufs_hba *hba)
{
	if (hba->var && hba->var->vops && hba->var->vops->get_ufs_hci_version)
		return hba->var->vops->get_ufs_hci_version(hba);
	return ufshcd_readl(hba, REG_UFS_VERSION);
}

static inline int ufshcd_vops_clk_scale_notify(struct ufs_hba *hba,
			bool up, enum ufs_notify_change_status status)
{
	if (hba->var && hba->var->vops && hba->var->vops->clk_scale_notify)
		return hba->var->vops->clk_scale_notify(hba, up, status);
	return 0;
}

static inline int ufshcd_vops_setup_clocks(struct ufs_hba *hba, bool on,
					enum ufs_notify_change_status status)
{
	if (hba->var && hba->var->vops && hba->var->vops->setup_clocks)
		return hba->var->vops->setup_clocks(hba, on, status);
	return 0;
}

static inline int ufshcd_vops_setup_regulators(struct ufs_hba *hba, bool status)
{
	if (hba->var && hba->var->vops && hba->var->vops->setup_regulators)
		return hba->var->vops->setup_regulators(hba, status);
	return 0;
}

static inline int ufshcd_vops_hce_enable_notify(struct ufs_hba *hba,
						bool status)
{
	if (hba->var && hba->var->vops && hba->var->vops->hce_enable_notify)
		hba->var->vops->hce_enable_notify(hba, status);
	return 0;
}
static inline int ufshcd_vops_link_startup_notify(struct ufs_hba *hba,
						bool status)
{
	if (hba->var && hba->var->vops && hba->var->vops->link_startup_notify)
		return hba->var->vops->link_startup_notify(hba, status);
	return 0;
}

static inline int ufshcd_vops_pwr_change_notify(struct ufs_hba *hba,
				  bool status,
				  struct ufs_pa_layer_attr *dev_max_params,
				  struct ufs_pa_layer_attr *dev_req_params)
{
	if (hba->var && hba->var->vops && hba->var->vops->pwr_change_notify)
		return hba->var->vops->pwr_change_notify(hba, status,
					dev_max_params, dev_req_params);

	return -ENOTSUPP;
}

static inline void ufshcd_vops_setup_xfer_req(struct ufs_hba *hba, int tag,
					bool is_scsi_cmd)
{
	if (hba->var && hba->var->vops && hba->var->vops->setup_xfer_req)
		return hba->var->vops->setup_xfer_req(hba, tag, is_scsi_cmd);
}

static inline void ufshcd_vops_setup_task_mgmt(struct ufs_hba *hba,
					int tag, u8 tm_function)
{
	if (hba->var->vops && hba->var->vops->setup_task_mgmt)
		return hba->var->vops->setup_task_mgmt(hba, tag, tm_function);
}

static inline void ufshcd_vops_hibern8_notify(struct ufs_hba *hba,
					enum uic_cmd_dme cmd,
					enum ufs_notify_change_status status)
{
	if (hba->var->vops && hba->var->vops->hibern8_notify)
		return hba->var->vops->hibern8_notify(hba, cmd, status);
}

static inline int ufshcd_vops_apply_dev_quirks(struct ufs_hba *hba)
{
	if (hba->var->vops && hba->var->vops->apply_dev_quirks)
		return hba->var->vops->apply_dev_quirks(hba);
	return 0;
}

static inline int ufshcd_vops_suspend(struct ufs_hba *hba, enum ufs_pm_op op)
{
	if (hba->var && hba->var->vops && hba->var->vops->suspend)
		return hba->var->vops->suspend(hba, op);
	return 0;
}

static inline int ufshcd_vops_resume(struct ufs_hba *hba, enum ufs_pm_op op)
{
	if (hba->var && hba->var->vops && hba->var->vops->resume)
		return hba->var->vops->resume(hba, op);
	return 0;
}

static inline int ufshcd_vops_full_reset(struct ufs_hba *hba)
{
	if (hba->var && hba->var->vops && hba->var->vops->full_reset)
		return hba->var->vops->full_reset(hba);
	return 0;
}

static inline void ufshcd_vops_dbg_register_dump(struct ufs_hba *hba,
						 bool no_sleep)
{
	if (hba->var && hba->var->vops && hba->var->vops->dbg_register_dump)
		hba->var->vops->dbg_register_dump(hba, no_sleep);
}

static inline int ufshcd_vops_update_sec_cfg(struct ufs_hba *hba,
						bool restore_sec_cfg)
{
	if (hba->var && hba->var->vops && hba->var->vops->update_sec_cfg)
		return hba->var->vops->update_sec_cfg(hba, restore_sec_cfg);
	return 0;
}

static inline u32 ufshcd_vops_get_scale_down_gear(struct ufs_hba *hba)
{
	if (hba->var && hba->var->vops && hba->var->vops->get_scale_down_gear)
		return hba->var->vops->get_scale_down_gear(hba);
	/* Default to lowest high speed gear */
	return UFS_HS_G1;
}

static inline int ufshcd_vops_set_bus_vote(struct ufs_hba *hba, bool on)
{
	if (hba->var && hba->var->vops && hba->var->vops->set_bus_vote)
		return hba->var->vops->set_bus_vote(hba, on);
	return 0;
}

#ifdef CONFIG_DEBUG_FS
static inline void ufshcd_vops_add_debugfs(struct ufs_hba *hba,
						struct dentry *root)
{
	if (hba->var && hba->var->vops && hba->var->vops->add_debugfs)
		hba->var->vops->add_debugfs(hba, root);
}

static inline void ufshcd_vops_remove_debugfs(struct ufs_hba *hba)
{
	if (hba->var && hba->var->vops && hba->var->vops->remove_debugfs)
		hba->var->vops->remove_debugfs(hba);
}
#else
static inline void ufshcd_vops_add_debugfs(struct ufs_hba *hba, struct dentry *)
{
}

static inline void ufshcd_vops_remove_debugfs(struct ufs_hba *hba)
{
}
#endif

static inline int ufshcd_vops_crypto_req_setup(struct ufs_hba *hba,
	struct ufshcd_lrb *lrbp, u8 *cc_index, bool *enable, u64 *dun)
{
	if (hba->var && hba->var->crypto_vops &&
		hba->var->crypto_vops->crypto_req_setup)
		return hba->var->crypto_vops->crypto_req_setup(hba, lrbp,
			cc_index, enable, dun);
	return 0;
}

static inline int ufshcd_vops_crypto_engine_cfg_start(struct ufs_hba *hba,
						unsigned int task_tag)
{
	if (hba->var && hba->var->crypto_vops &&
	    hba->var->crypto_vops->crypto_engine_cfg_start)
		return hba->var->crypto_vops->crypto_engine_cfg_start
				(hba, task_tag);
	return 0;
}

static inline int ufshcd_vops_crypto_engine_cfg_end(struct ufs_hba *hba,
						struct ufshcd_lrb *lrbp,
						struct request *req)
{
	if (hba->var && hba->var->crypto_vops &&
	    hba->var->crypto_vops->crypto_engine_cfg_end)
		return hba->var->crypto_vops->crypto_engine_cfg_end
				(hba, lrbp, req);
	return 0;
}

static inline int ufshcd_vops_crypto_engine_reset(struct ufs_hba *hba)
{
	if (hba->var && hba->var->crypto_vops &&
	    hba->var->crypto_vops->crypto_engine_reset)
		return hba->var->crypto_vops->crypto_engine_reset(hba);
	return 0;
}

static inline int ufshcd_vops_crypto_engine_get_status(struct ufs_hba *hba,
		u32 *status)

{
	if (hba->var && hba->var->crypto_vops &&
	    hba->var->crypto_vops->crypto_engine_get_status)
		return hba->var->crypto_vops->crypto_engine_get_status(hba,
			status);
	return 0;
}

static inline void ufshcd_vops_pm_qos_req_start(struct ufs_hba *hba,
		struct request *req)
{
	if (hba->var && hba->var->pm_qos_vops &&
		hba->var->pm_qos_vops->req_start)
		hba->var->pm_qos_vops->req_start(hba, req);
}

static inline void ufshcd_vops_pm_qos_req_end(struct ufs_hba *hba,
		struct request *req, bool lock)
{
	if (hba->var && hba->var->pm_qos_vops && hba->var->pm_qos_vops->req_end)
		hba->var->pm_qos_vops->req_end(hba, req, lock);
}

#endif /* End of Header */
