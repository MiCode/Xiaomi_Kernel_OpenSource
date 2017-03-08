/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_RSC_H_
#define _SDE_RSC_H_

#include <linux/kernel.h>
#include <linux/sde_io_util.h>

#include <soc/qcom/tcs.h>
#include "sde_power_handle.h"

#define SDE_RSC_COMPATIBLE "disp_rscc"

#define MAX_RSC_CLIENT_NAME_LEN 128

/* primary display rsc index */
#define SDE_RSC_INDEX		0

/* rsc index max count */
#define MAX_RSC_COUNT		5

struct sde_rsc_priv;

/**
 * rsc_mode_req: sde rsc mode request information
 * MODE_READ: read vsync status
 * MODE0_UPDATE: mode0 status , this should be 0x0
 * MODE1_UPDATE: mode1 status , this should be 0x1
 * MODE2_UPDATE: mode2 status , this should be 0x2
 */
enum rsc_mode_req {
	MODE_READ,
	MODE0_UPDATE = 0x1,
	MODE1_UPDATE = 0x2,
	MODE2_UPDATE = 0x3,
};

/**
 * rsc_vsync_req: sde rsc vsync request information
 * VSYNC_READ: read vsync status
 * VSYNC_ENABLE: enable rsc wrapper vsync status
 * VSYNC_DISABLE: disable rsc wrapper vsync status
 */
enum rsc_vsync_req {
	VSYNC_READ,
	VSYNC_ENABLE,
	VSYNC_DISABLE,
};

/**
 * sde_rsc_state: sde rsc state information
 * SDE_RSC_MODE_IDLE: A client requests for idle state when there is no
 *                    pixel or cmd transfer expected. An idle vote from
 *                    all clients lead to power collapse state.
 * SDE_RSC_MODE_CMD:  A client requests for cmd state when it wants to
 *                    enable the solver mode.
 * SDE_RSC_MODE_CMD_UPDATE: A clients requests for cmd_update state when
 *                    it wants to update the backoff time during solver
 *                    enable state. Inline-rotation is one good example
 *                    use case. It increases the prefill lines by 128 lines.
 * SDE_RSC_MODE_VID:  A client requests for vid state it wants to avoid
 *                    solver enable because client is fetching data from
 *                    continuously.
 */
enum sde_rsc_state {
	SDE_RSC_IDLE_STATE,
	SDE_RSC_CMD_STATE,
	SDE_RSC_CMD_UPDATE_STATE,
	SDE_RSC_VID_STATE,
};

/**
 * struct sde_rsc_client: stores the rsc client for sde driver
 * @name:	name of the client
 * @current_state:   current client state
 * @crtc_id:		crtc_id associated with this rsc client.
 * @rsc_index:	rsc index of a client - only index "0" valid.
 * @list:	list to attach power handle master list
 */
struct sde_rsc_client {
	char name[MAX_RSC_CLIENT_NAME_LEN];
	short current_state;
	int crtc_id;
	u32 rsc_index;
	struct list_head list;
};

/**
 * struct sde_rsc_hw_ops - sde resource state coordinator hardware ops
 * @init:			Initialize the sequencer, solver, qtimer,
				etc. hardware blocks on RSC.
 * @tcs_wait:			Waits for TCS block OK to allow sending a
 *				TCS command.
 * @hw_vsync:			Enables the vsync on RSC block.
 * @tcs_use_ok:			set TCS set to high to allow RSC to use it.
 * @mode2_entry:		Request to entry mode2 when all clients are
 *                              requesting power collapse.
 * @mode2_exit:			Request to exit mode2 when one of the client
 *                              is requesting against the power collapse
 * @is_amc_mode:		Check current amc mode status
 * @state_update:		Enable/override the solver based on rsc state
 *                              status (command/video)
 * @mode_show:			shows current mode status, mode0/1/2
 * @debug_show:			Show current debug status.
 */

struct sde_rsc_hw_ops {
	int (*init)(struct sde_rsc_priv *rsc);
	int (*tcs_wait)(struct sde_rsc_priv *rsc);
	int (*hw_vsync)(struct sde_rsc_priv *rsc, enum rsc_vsync_req request,
		char *buffer, int buffer_size, u32 mode);
	int (*tcs_use_ok)(struct sde_rsc_priv *rsc);
	int (*mode2_entry)(struct sde_rsc_priv *rsc);
	int (*mode2_exit)(struct sde_rsc_priv *rsc);
	bool (*is_amc_mode)(struct sde_rsc_priv *rsc);
	int (*state_update)(struct sde_rsc_priv *rsc, enum sde_rsc_state state);
	int (*debug_show)(struct seq_file *s, struct sde_rsc_priv *rsc);
	int (*mode_ctrl)(struct sde_rsc_priv *rsc, enum rsc_mode_req request,
		char *buffer, int buffer_size, bool mode);
};

/**
 * struct sde_rsc_cmd_config: provides panel configuration to rsc
 * when client is command mode. It is not required to set it during
 * video mode.
 *
 * @fps:	panel te interval
 * @vtotal:	current vertical total (height + vbp + vfp)
 * @jitter:	panel can set the jitter to wake up rsc/solver early
 *              This value causes mdp core to exit certain mode
 *              early. Default is 10% jitter
 * @prefill_lines:	max prefill lines based on panel
 */
struct sde_rsc_cmd_config {
	u32 fps;
	u32 vtotal;
	u32 jitter;
	u32 prefill_lines;
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
 *
 * @drv_io:		sde drv io data mapping
 * @wrapper_io:		wrapper io data mapping
 *
 * @client_list:	current rsc client list handle
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
 * master_drm:		Primary client waits for vsync on this drm object based
 *			on crtc id
 */
struct sde_rsc_priv {
	u32 version;
	struct sde_power_handle phandle;
	struct sde_power_client *pclient;
	struct regulator *fs;

	struct dss_io_data drv_io;
	struct dss_io_data wrapper_io;

	struct list_head client_list;
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

	struct drm_device *master_drm;
};

/**
 * sde_rsc_client_create() - create the client for sde rsc.
 * Different displays like DSI, HDMI, DP, WB, etc should call this
 * api to register their vote for rpmh. They still need to vote for
 * power handle to get the clocks.

 * @rsc_index:   A client will be created on this RSC. As of now only
 *               SDE_RSC_INDEX is valid rsc index.
 * @name:	 Caller needs to provide some valid string to identify
 *               the client. "primary", "dp", "hdmi" are suggested name.
 * @is_primary:	 Caller needs to provide information if client is primary
 *               or not. Primary client votes will be redirected to
 *               display rsc.
 * @config:	 fps, vtotal, porches, etc configuration for command mode
 *               panel
 *
 * Return: client node pointer.
 */
struct sde_rsc_client *sde_rsc_client_create(u32 rsc_index, char *name,
		bool is_primary_display);

/**
 * sde_rsc_client_destroy() - Destroy the sde rsc client.
 *
 * @client:	 Client pointer provided by sde_rsc_client_create().
 *
 * Return: none
 */
void sde_rsc_client_destroy(struct sde_rsc_client *client);

/**
 * sde_rsc_client_state_update() - rsc client state update
 * Video mode and command mode are supported as modes. A client need to
 * set this property during panel time. A switching client can set the
 * property to change the state
 *
 * @client:	 Client pointer provided by sde_rsc_client_create().
 * @state:	 Client state - video/cmd
 * @config:	 fps, vtotal, porches, etc configuration for command mode
 *               panel
 * @crtc_id:	 current client's crtc id
 *
 * Return: error code.
 */
int sde_rsc_client_state_update(struct sde_rsc_client *client,
	enum sde_rsc_state state,
	struct sde_rsc_cmd_config *config, int crtc_id);

/**
 * sde_rsc_client_vote() - ab/ib vote from rsc client
 *
 * @client:	 Client pointer provided by sde_rsc_client_create().
 * @ab:		 aggregated bandwidth vote from client.
 * @ib:		 instant bandwidth vote from client.
 *
 * Return: error code.
 */
int sde_rsc_client_vote(struct sde_rsc_client *caller_client,
	u64 ab_vote, u64 ib_vote);

/**
 * sde_rsc_hw_register() - register hardware API
 *
 * @client:	 Client pointer provided by sde_rsc_client_create().
 *
 * Return: error code.
 */
int sde_rsc_hw_register(struct sde_rsc_priv *rsc);


#endif /* _SDE_RSC_H_ */
