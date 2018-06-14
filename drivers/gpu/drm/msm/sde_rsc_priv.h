/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _SDE_RSC_PRIV_H_
#define _SDE_RSC_PRIV_H_

#include <linux/kernel.h>
#include <linux/sde_io_util.h>
#include <linux/sde_rsc.h>

#include <soc/qcom/tcs.h>
#include "sde_power_handle.h"

#define SDE_RSC_COMPATIBLE "disp_rscc"

#define MAX_RSC_COUNT		5

#define ALL_MODES_DISABLED	0x0
#define ONLY_MODE_0_ENABLED	0x1
#define ONLY_MODE_0_1_ENABLED	0x3
#define ALL_MODES_ENABLED	0x7

#define MAX_COUNT_SIZE_SUPPORTED	128

#define SDE_RSC_REV_1			0x1
#define SDE_RSC_REV_2			0x2

struct sde_rsc_priv;

/**
 * rsc_mode_req: sde rsc mode request information
 * MODE_READ: read vsync status
 * MODE_UPDATE: mode timeslot update
 *            0x0: all modes are disabled.
 *            0x1: Mode-0 is enabled and other two modes are disabled.
 *            0x3: Mode-0 & Mode-1 are enabled and mode-2 is disabled.
 *            0x7: all modes are enabled.
 */
enum rsc_mode_req {
	MODE_READ,
	MODE_UPDATE = 0x1,
};

/**
 * rsc_vsync_req: sde rsc vsync request information
 * VSYNC_READ: read vsync status
 * VSYNC_READ_VSYNC0: read value vsync0 timestamp (cast to int from u32)
 * VSYNC_ENABLE: enable rsc wrapper vsync status
 * VSYNC_DISABLE: disable rsc wrapper vsync status
 */
enum rsc_vsync_req {
	VSYNC_READ,
	VSYNC_READ_VSYNC0,
	VSYNC_ENABLE,
	VSYNC_DISABLE,
};

/**
 * struct sde_rsc_hw_ops - sde resource state coordinator hardware ops
 * @init:			Initialize the sequencer, solver, qtimer,
				etc. hardware blocks on RSC.
 * @timer_update:		update the static wrapper time and pdc/rsc
				backoff time.
 * @tcs_wait:			Waits for TCS block OK to allow sending a
 *				TCS command.
 * @hw_vsync:			Enables the vsync on RSC block.
 * @tcs_use_ok:			set TCS set to high to allow RSC to use it.
 * @is_amc_mode:		Check current amc mode status
 * @debug_dump:			dump debug bus registers or enable debug bus
 * @state_update:		Enable/override the solver based on rsc state
 *                              status (command/video)
 * @mode_show:			shows current mode status, mode0/1/2
 * @debug_show:			Show current debug status.
 */

struct sde_rsc_hw_ops {
	int (*init)(struct sde_rsc_priv *rsc);
	int (*timer_update)(struct sde_rsc_priv *rsc);
	int (*tcs_wait)(struct sde_rsc_priv *rsc);
	int (*hw_vsync)(struct sde_rsc_priv *rsc, enum rsc_vsync_req request,
		char *buffer, int buffer_size, u32 mode);
	int (*tcs_use_ok)(struct sde_rsc_priv *rsc);
	bool (*is_amc_mode)(struct sde_rsc_priv *rsc);
	void (*debug_dump)(struct sde_rsc_priv *rsc, u32 mux_sel);
	int (*state_update)(struct sde_rsc_priv *rsc, enum sde_rsc_state state);
	int (*debug_show)(struct seq_file *s, struct sde_rsc_priv *rsc);
	int (*mode_ctrl)(struct sde_rsc_priv *rsc, enum rsc_mode_req request,
		char *buffer, int buffer_size, u32 mode);
};

/**
 * struct sde_rsc_timer_config: this is internal configuration between
 * rsc and rsc_hw API.
 *
 * @static_wakeup_time_ns:	wrapper backoff time in nano seconds
 * @rsc_backoff_time_ns:	rsc backoff time in nano seconds
 * @pdc_backoff_time_ns:	pdc backoff time in nano seconds
 * @rsc_mode_threshold_time_ns:	rsc mode threshold time in nano seconds
 * @rsc_time_slot_0_ns:		mode-0 time slot threshold in nano seconds
 * @rsc_time_slot_1_ns:		mode-1 time slot threshold in nano seconds
 * @rsc_time_slot_2_ns:		mode-2 time slot threshold in nano seconds
 */
struct sde_rsc_timer_config {
	u32 static_wakeup_time_ns;

	u32 rsc_backoff_time_ns;
	u32 pdc_backoff_time_ns;
	u32 rsc_mode_threshold_time_ns;
	u32 rsc_time_slot_0_ns;
	u32 rsc_time_slot_1_ns;
	u32 rsc_time_slot_2_ns;
};

/**
 * struct sde_rsc_priv: sde resource state coordinator(rsc) private handle
 * @version:		rsc sequence version
 * @phandle:		module power handle for clocks
 * @pclient:		module power client of phandle
 * @fs:			"MDSS GDSC" handle
 * @sw_fs_enabled:	track "MDSS GDSC" sw vote during probe
 *
 * @disp_rsc:		display rsc handle
 * @drv_io:		sde drv io data mapping
 * @wrapper_io:		wrapper io data mapping
 *
 * @client_list:	current rsc client list handle
 * @event_list:		current rsc event list handle
 * @client_lock:	current rsc client synchronization lock
 *
 * timer_config:	current rsc timer configuration
 * cmd_config:		current panel config
 * current_state:	current rsc state (video/command), solver
 *                      override/enabled.
 * debug_mode:		enables the logging for each register read/write
 * debugfs_root:	debugfs file system root node
 *
 * hw_ops:		sde rsc hardware operations
 * power_collapse:	if all clients are in IDLE state then it enters in
 *			mode2 state and enable the power collapse state
 * power_collapse_block:By default, rsc move to mode-2 if all clients are in
 *			invalid state. It can be blocked by this boolean entry.
 * primary_client:	A client which is allowed to make command state request
 *			and ab/ib vote on display rsc
 * single_tcs_execution_time: worst case time to execute one tcs vote
 *			(sleep/wake)
 * backoff_time_ns:	time to only wake tcs in any mode
 * mode_threshold_time_ns: time to wake TCS in mode-0, must be greater than
 *			backoff time
 * time_slot_0_ns:	time for sleep & wake TCS in mode-1
 * master_drm:		Primary client waits for vsync on this drm object based
 *			on crtc id
 * rsc_vsync_wait:   Refcount to indicate if we have to wait for the vsync.
 * rsc_vsync_waitq:   Queue to wait for the vsync.
 */
struct sde_rsc_priv {
	u32 version;
	struct sde_power_handle phandle;
	struct sde_power_client *pclient;
	struct regulator *fs;
	bool sw_fs_enabled;

	struct rpmh_client *disp_rsc;
	struct dss_io_data drv_io;
	struct dss_io_data wrapper_io;

	struct list_head client_list;
	struct list_head event_list;
	struct mutex client_lock;

	struct sde_rsc_timer_config timer_config;
	struct sde_rsc_cmd_config cmd_config;
	u32	current_state;

	u32 debug_mode;
	struct dentry *debugfs_root;

	struct sde_rsc_hw_ops hw_ops;
	bool power_collapse;
	bool power_collapse_block;
	struct sde_rsc_client *primary_client;

	u32 single_tcs_execution_time;
	u32 backoff_time_ns;
	u32 mode_threshold_time_ns;
	u32 time_slot_0_ns;

	struct drm_device *master_drm;
	atomic_t rsc_vsync_wait;
	wait_queue_head_t rsc_vsync_waitq;
};

/**
 * sde_rsc_hw_register() - register hardware API
 *
 * @client:	 Client pointer provided by sde_rsc_client_create().
 *
 * Return: error code.
 */
int sde_rsc_hw_register(struct sde_rsc_priv *rsc);

#endif /* _SDE_RSC_PRIV_H_ */
