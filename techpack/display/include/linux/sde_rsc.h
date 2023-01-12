/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_RSC_H_
#define _SDE_RSC_H_

#include <linux/kernel.h>

/* primary display rsc index */
#define SDE_RSC_INDEX		0

#define MAX_RSC_CLIENT_NAME_LEN 128
#define NUM_RSC_PROFILING_COUNTERS 3

/* DRM Object IDs are numbered excluding 0, use 0 to indicate invalid CRTC */
#define SDE_RSC_INVALID_CRTC_ID 0

/**
 * event will be triggered before sde core power collapse,
 * mdss gdsc is still on
 */
#define SDE_RSC_EVENT_PRE_CORE_PC 0x1
/**
 * event will be triggered after sde core collapse complete,
 * mdss gdsc is off now
 */
#define SDE_RSC_EVENT_POST_CORE_PC 0x2
/**
 * event will be triggered before restoring the sde core from power collapse,
 * mdss gdsc is still off
 */
#define SDE_RSC_EVENT_PRE_CORE_RESTORE 0x4
/**
 * event will be triggered after restoring the sde core from power collapse,
 * mdss gdsc is on now
 */
#define SDE_RSC_EVENT_POST_CORE_RESTORE 0x8
/**
 * event attached with solver state enabled
 * all clients in clk_state or cmd_state
 */
#define SDE_RSC_EVENT_SOLVER_ENABLED 0x10
/**
 * event attached with solver state disabled
 * one of the client requested for vid state
 */
#define SDE_RSC_EVENT_SOLVER_DISABLED 0x20

/**
 * sde_rsc_client_type: sde rsc client type information
 * SDE_RSC_PRIMARY_DISP_CLIENT:	A primary display client which can request
 *				vid or cmd state switch.
 * SDE_RSC_EXTERNAL_DISPLAY_CLIENT:An external display client which can
 *                              request only clk state switch.
 * SDE_RSC_CLK_CLIENT:		A clk client request for only rsc clocks
 *				enabled and mode_2 exit state.
 */
enum sde_rsc_client_type {
	SDE_RSC_PRIMARY_DISP_CLIENT,
	SDE_RSC_EXTERNAL_DISP_CLIENT,
	SDE_RSC_CLK_CLIENT,
	SDE_RSC_INVALID_CLIENT,
};

/**
 * sde_rsc_state: sde rsc state information
 * SDE_RSC_IDLE_STATE: A client requests for idle state when there is no
 *                    pixel or cmd transfer expected. An idle vote from
 *                    all clients lead to power collapse state.
 * SDE_RSC_CLK_STATE:  A client requests for clk state when it wants to
 *                    only avoid mode-2 entry/exit. For ex: V4L2 driver,
 *                    sde power handle, etc.
 * SDE_RSC_CMD_STATE:  A client requests for cmd state when it wants to
 *                    enable the solver mode.
 * SDE_RSC_VID_STATE:  A client requests for vid state it wants to avoid
 *                    solver enable because client is fetching data from
 *                    continuously.
 */
enum sde_rsc_state {
	SDE_RSC_IDLE_STATE,
	SDE_RSC_CLK_STATE,
	SDE_RSC_CMD_STATE,
	SDE_RSC_VID_STATE,
};

/**
 * struct sde_rsc_client: stores the rsc client for sde driver
 * @name:	name of the client
 * @current_state:   current client state
 * @crtc_id:		crtc_id associated with this rsc client.
 * @rsc_index:	rsc index of a client - only index "0" valid.
 * @id:		Index of client. It will be assigned during client_create call
 * @client_type: check sde_rsc_client_type information
 * @list:	list to attach client master list
 */
struct sde_rsc_client {
	char name[MAX_RSC_CLIENT_NAME_LEN];
	short current_state;
	int crtc_id;
	u32 rsc_index;
	u32 id;
	enum sde_rsc_client_type client_type;
	struct list_head list;
};

/**
 * struct sde_rsc_event: local event registration entry structure
 * @cb_func:	Pointer to desired callback function
 * @usr:	User pointer to pass to callback on event trigger
 * @rsc_index:	rsc index of a client - only index "0" valid.
 * @event_type:	refer comments in event_register
 * @list:	list to attach event master list
 */
struct sde_rsc_event {
	void (*cb_func)(uint32_t event_type, void *usr);
	void *usr;
	u32 rsc_index;
	uint32_t event_type;
	struct list_head list;
};

/**
 * struct sde_rsc_cmd_config: provides panel configuration to rsc
 * when client is command mode. It is not required to set it during
 * video mode.
 *
 * @fps:	panel te interval
 * @vtotal:	current vertical total (height + vbp + vfp)
 * @jitter_numer: panel jitter numerator value. This config causes rsc/solver
 *                early before te. Default is 0.8% jitter.
 * @jitter_denom: panel jitter denominator.
 * @prefill_lines:	max prefill lines based on panel
 */
struct sde_rsc_cmd_config {
	u32 fps;
	u32 vtotal;
	u32 jitter_numer;
	u32 jitter_denom;
	u32 prefill_lines;
};

#if IS_ENABLED(CONFIG_DRM_SDE_RSC)
/**
 * sde_rsc_client_create() - create the client for sde rsc.
 * Different displays like DSI, HDMI, DP, WB, etc should call this
 * api to register their vote for rpmh. They still need to vote for
 * power handle to get the clocks.

 * @rsc_index:   A client will be created on this RSC. As of now only
 *               SDE_RSC_INDEX is valid rsc index.
 * @name:	 Caller needs to provide some valid string to identify
 *               the client. "primary", "dp", "hdmi" are suggested name.
 * @client_type: check client_type enum for information
 * @vsync_source: This parameter is only valid for primary display. It provides
 *               vsync source information
 *
 * Return: client node pointer.
 */
struct sde_rsc_client *sde_rsc_client_create(u32 rsc_index, char *name,
	enum sde_rsc_client_type client_type, u32 vsync_source);

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
 * Video mode, cmd mode and clk state are supported as modes. A client need to
 * set this property during panel time. A switching client can set the
 * property to change the state
 *
 * @client:	 Client pointer provided by sde_rsc_client_create().
 * @state:	 Client state - video/cmd
 * @config:	 fps, vtotal, porches, etc configuration for command mode
 *               panel
 * @crtc_id:	 current client's crtc id
 * @wait_vblank_crtc_id:	Output parameter. If set to non-zero, rsc hw
 *				state update requires a wait for one vblank on
 *				the primary crtc. In that case, this output
 *				param will be set to the crtc on which to wait.
 *				If SDE_RSC_INVALID_CRTC_ID, no wait necessary
 *
 * Return: error code.
 */
int sde_rsc_client_state_update(struct sde_rsc_client *client,
	enum sde_rsc_state state,
	struct sde_rsc_cmd_config *config, int crtc_id,
	int *wait_vblank_crtc_id);

/**
 * sde_rsc_client_get_vsync_refcount() - returns the status of the vsync
 * refcount, to signal if the client needs to reset the refcounting logic
 * @client:	 Client pointer provided by sde_rsc_client_create().
 *
 * Return: true if the state update has completed.
 */
int sde_rsc_client_get_vsync_refcount(
		struct sde_rsc_client *caller_client);

/**
 * sde_rsc_client_reset_vsync_refcount() - reduces the refcounting
 * logic that waits for the vsync.
 * @client:	 Client pointer provided by sde_rsc_client_create().
 *
 * Return: true if the state update has completed.
 */
int sde_rsc_client_reset_vsync_refcount(
		struct sde_rsc_client *caller_client);

/**
 * sde_rsc_client_is_state_update_complete() - check if state update is complete
 * RSC state transition is not complete until HW receives VBLANK signal. This
 * function checks RSC HW to determine whether that signal has been received.
 * @client:	 Client pointer provided by sde_rsc_client_create().
 *
 * Return: true if the state update has completed.
 */
bool sde_rsc_client_is_state_update_complete(
		struct sde_rsc_client *caller_client);

/**
 * sde_rsc_client_vote() - stores ab/ib vote for rsc client
 *
 * @client:	 Client pointer provided by sde_rsc_client_create().
 * @bus_id:	 data bus identifier
 * @ab:		 aggregated bandwidth vote from client.
 * @ib:		 instant bandwidth vote from client.
 *
 * Return: error code.
 */
int sde_rsc_client_vote(struct sde_rsc_client *caller_client,
	u32 bus_id, u64 ab_vote, u64 ib_vote);

/**
 * sde_rsc_register_event - register a callback function for an event
 * @rsc_index:   A client will be created on this RSC. As of now only
 *               SDE_RSC_INDEX is valid rsc index.
 * @event_type:  event type to register; client sets 0x3 if it wants
 *               to register for CORE_PC and CORE_RESTORE - both events.
 * @cb_func:     Pointer to desired callback function
 * @usr:         User pointer to pass to callback on event trigger
 * Returns: sde_rsc_event pointer on success
 */
struct sde_rsc_event *sde_rsc_register_event(int rsc_index, uint32_t event_type,
		void (*cb_func)(uint32_t event_type, void *usr), void *usr);

/**
 * sde_rsc_unregister_event - unregister callback for an event
 * @sde_rsc_event: event returned by sde_rsc_register_event
 */
void sde_rsc_unregister_event(struct sde_rsc_event *event);

/**
 * is_sde_rsc_available - check if display rsc available.
 * @rsc_index:   A client will be created on this RSC. As of now only
 *               SDE_RSC_INDEX is valid rsc index.
 * Returns: true if rsc is available; false in all other cases
 */
bool is_sde_rsc_available(int rsc_index);

/**
 * get_sde_rsc_current_state - gets the current state of sde rsc.
 * @rsc_index:   A client will be created on this RSC. As of now only
 *               SDE_RSC_INDEX is valid rsc index.
 * Returns: current state if rsc available; SDE_RSC_IDLE_STATE for
 *          all other cases
 */
enum sde_rsc_state get_sde_rsc_current_state(int rsc_index);

/**
 * sde_rsc_client_trigger_vote() - triggers ab/ib vote for rsc client
 *
 * @client:	 Client pointer provided by sde_rsc_client_create().
 * @delta_vote:  if bw vote is increased or decreased
 *
 * Return: error code.
 */
int sde_rsc_client_trigger_vote(struct sde_rsc_client *caller_client,
	bool delta_vote);

#else

static inline struct sde_rsc_client *sde_rsc_client_create(u32 rsc_index,
	char *name, enum sde_rsc_client_type client_type, u32 vsync_source)
{
	return NULL;
}

static inline void sde_rsc_client_destroy(struct sde_rsc_client *client)
{
}

static inline int sde_rsc_client_state_update(struct sde_rsc_client *client,
	enum sde_rsc_state state,
	struct sde_rsc_cmd_config *config, int crtc_id,
	int *wait_vblank_crtc_id)
{
	return 0;
}

static inline int sde_rsc_client_get_vsync_refcount(
		struct sde_rsc_client *caller_client)
{
	return 0;
}

static inline int sde_rsc_client_reset_vsync_refcount(
		struct sde_rsc_client *caller_client)
{
	return 0;
}

static inline bool sde_rsc_client_is_state_update_complete(
		struct sde_rsc_client *caller_client)
{
	return false;
}

static inline int sde_rsc_client_vote(struct sde_rsc_client *caller_client,
	u32 bus_id, u64 ab_vote, u64 ib_vote)
{
	return 0;
}

static inline struct sde_rsc_event *sde_rsc_register_event(int rsc_index,
		uint32_t event_type,
		void (*cb_func)(uint32_t event_type, void *usr), void *usr)
{
	return NULL;
}

static inline void sde_rsc_unregister_event(struct sde_rsc_event *event)
{
}

static inline bool is_sde_rsc_available(int rsc_index)
{
	return false;
}

static inline enum sde_rsc_state get_sde_rsc_current_state(int rsc_index)
{
	return SDE_RSC_IDLE_STATE;
}

static inline int sde_rsc_client_trigger_vote(
	struct sde_rsc_client *caller_client, bool delta_vote)
{
	return 0;
}
#endif /* CONFIG_DRM_SDE_RSC */

#endif /* _SDE_RSC_H_ */
