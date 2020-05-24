/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2015-2016, 2018, 2020, The Linux Foundation. All rights reserved. */

#ifndef _MDSS_DSI_CLK_H_
#define _MDSS_DSI_CLK_H_

#include <linux/mdss_io_util.h>
#include <linux/list.h>

#define DSI_CLK_NAME_LEN 20

#define MDSS_DSI_CLK_UPDATE_CLK_RATE_AT_ON 0x1

enum mdss_dsi_clk_state {
	MDSS_DSI_CLK_OFF,
	MDSS_DSI_CLK_ON,
	MDSS_DSI_CLK_EARLY_GATE,
};

enum dsi_clk_req_client {
	DSI_CLK_REQ_MDP_CLIENT = 0,
	DSI_CLK_REQ_DSI_CLIENT,
};

enum mdss_dsi_link_clk_type {
	MDSS_DSI_LINK_ESC_CLK,
	MDSS_DSI_LINK_BYTE_CLK,
	MDSS_DSI_LINK_PIX_CLK,
	MDSS_DSI_LINK_BYTE_INTF_CLK,
	MDSS_DSI_LINK_CLK_MAX,
};

enum mdss_dsi_clk_type {
	MDSS_DSI_CORE_CLK = BIT(0),
	MDSS_DSI_LINK_CLK = BIT(1),
	MDSS_DSI_ALL_CLKS = (BIT(0) | BIT(1)),
	MDSS_DSI_CLKS_MAX = BIT(2),
};

/**
 * typedef *pre_clockoff_cb() - Callback before clock is turned off
 * @priv: private data pointer.
 * @clk_type: clock which is being turned off.
 * @new_state: next state for the clock.
 *
 * @return: error code.
 */
typedef int (*pre_clockoff_cb)(void *priv,
			       enum mdss_dsi_clk_type clk_type,
			       enum mdss_dsi_clk_state new_state);

/**
 * typedef *post_clockoff_cb() - Callback after clock is turned off
 * @priv: private data pointer.
 * @clk_type: clock which was turned off.
 * @curr_state: current state for the clock.
 *
 * @return: error code.
 */
typedef int (*post_clockoff_cb)(void *priv,
				enum mdss_dsi_clk_type clk_type,
				enum mdss_dsi_clk_state curr_state);

/**
 * typedef *post_clockon_cb() - Callback after clock is turned on
 * @priv: private data pointer.
 * @clk_type: clock which was turned on.
 * @curr_state: current state for the clock.
 *
 * @return: error code.
 */
typedef int (*post_clockon_cb)(void *priv,
			       enum mdss_dsi_clk_type clk_type,
			       enum mdss_dsi_clk_state curr_state);

/**
 * typedef *pre_clockon_cb() - Callback before clock is turned on
 * @priv: private data pointer.
 * @clk_type: clock which is being turned on.
 * @new_state: next state for the clock.
 *
 * @return: error code.
 */
typedef int (*pre_clockon_cb)(void *priv,
			      enum mdss_dsi_clk_type clk_type,
			      enum mdss_dsi_clk_state new_state);

struct mdss_dsi_core_clk_info {
	struct clk *mdp_core_clk;
	struct clk *mnoc_clk;
	struct clk *ahb_clk;
	struct clk *axi_clk;
	struct clk *mmss_misc_ahb_clk;
};

struct mdss_dsi_link_clk_info {
	struct clk *esc_clk;
	struct clk *byte_clk;
	struct clk *pixel_clk;
	struct clk *byte_intf_clk;
};

struct dsi_panel_clk_ctrl {
	enum mdss_dsi_clk_state state;
	enum dsi_clk_req_client client;
};

/**
 * struct mdss_dsi_clk_info - clock information to initialize manager
 * @name: name for the clocks to identify debug logs.
 * @core_clks: core clock information.
 * @link_clks: link clock information.
 * @pre_clkoff_cb: callback before a clock is turned off.
 * @post_clkoff_cb: callback after a clock is turned off.
 * @pre_clkon_cb: callback before a clock is turned on.
 * @post_clkon_cb: callback after a clock is turned on.
 * @priv_data: pointer to private data passed to callbacks.
 */
struct mdss_dsi_clk_info {
	char name[DSI_CLK_NAME_LEN];
	struct mdss_dsi_core_clk_info core_clks;
	struct mdss_dsi_link_clk_info link_clks;
	pre_clockoff_cb pre_clkoff_cb;
	post_clockoff_cb post_clkoff_cb;
	post_clockon_cb post_clkon_cb;
	pre_clockon_cb pre_clkon_cb;
	void *priv_data;
};

struct mdss_dsi_clk_client {
	char *client_name;
};

/**
 * mdss_dsi_clk_init() - Initializes clock manager
 * @info: Clock information to be managed by the clock manager.
 *
 * The Init API should be called during probe of the dsi driver. DSI driver
 * provides the clock handles to the core clocks and link clocks that will be
 * managed by the clock manager.
 *
 * returns handle or an error value.
 */
void *mdss_dsi_clk_init(struct mdss_dsi_clk_info *info);

/**
 * mdss_dsi_clk_deinit() - Deinitializes the clock manager
 * @mngr: handle returned by mdss_dsi_clk_init().
 *
 * Deinit will turn off all the clocks and release all the resources acquired
 * by mdss_dsi_clk_init().
 *
 * @return: error code.
 */
int mdss_dsi_clk_deinit(void *mngr);

/**
 * mdss_dsi_clk_register() - Register a client to control clock state
 * @mngr: handle returned by mdss_dsi_clk_init().
 * @client: client information.
 *
 * Register allows clients for DSI clock manager to acquire a handle which can
 * be used to request a specific clock state. The clock manager maintains a
 * reference count of the clock states requested by each client. Client has to
 * ensure that ON and OFF/EARLY_GATE calls are balanced properly.
 *
 * Requesting a particular clock state does not guarantee that physical clock
 * state. Physical clock state is determined by the states requested by all
 * clients.
 *
 * @return: handle or error code.
 */
void *mdss_dsi_clk_register(void *mngr, struct mdss_dsi_clk_client *client);

/**
 * mdss_dsi_clk_deregister() - Deregister a registered client.
 * @client: client handle returned by mdss_dsi_clk_register().
 *
 * Deregister releases all resources acquired by mdss_dsi_clk_register().
 *
 * @return: error code.
 */
int mdss_dsi_clk_deregister(void *client);

/**
 * mdss_dsi_clk_req_state() - Request a specific clock state
 * @client: client handle.
 * @clk: Type of clock requested (enum mdss_dsi_clk_type).
 * @state: clock state requested.
 * @index: controller index.
 *
 * This routine is used to request a new clock state for a specific clock. If
 * turning ON the clocks, this guarantees that clocks will be on before
 * returning. Valid state transitions are ON -> EARLY GATE, ON -> OFF,
 * EARLY GATE -> OFF, EARLY GATE -> ON and OFF -> ON.
 *
 * @return: error code.
 */
int mdss_dsi_clk_req_state(void *client, enum mdss_dsi_clk_type clk,
	enum mdss_dsi_clk_state state, u32 index);

/**
 * mdss_dsi_clk_set_link_rate() - set clock rate for link clocks
 * @client: client handle.
 * @clk: type of clock.
 * @rate: clock rate in Hz.
 * @flags: flags.
 *
 * This routine is used to request a specific clock rate. It supports an
 * additional flags argument which can change the behavior of the routine. If
 * MDSS_DSI_CLK_UPDATE_CLK_RATE_AT_ON flag is set, the routine caches the new
 * clock rate and applies it next time when the clock is turned on.
 *
 * @return: error code.
 */
int mdss_dsi_clk_set_link_rate(void *client, enum mdss_dsi_link_clk_type clk,
			       u32 rate, u32 flags);

/**
 * mdss_dsi_clk_force_toggle() - Turn off and turn on clocks
 * @client: client handle.
 * @clk: clock type.
 *
 * This routine has to be used in cases where clocks have to be toggled
 * irrespecitive of the refcount. This API bypasses the refcount and turns off
 * and turns on the clocks. This will fail if the clocks are in OFF state
 * already.
 *
 * @return:error code.
 */
int mdss_dsi_clk_force_toggle(void *client, u32 clk);

/**
 * is_dsi_clk_in_ecg_state() - Checks the current state of clocks
 * @client: client handle.
 *
 * This routine returns checks the clocks status for client and return
 * success code based on it.
 *
 * @return:true: if clocks are in ECG state
 *         false: for all other cases
 */
bool is_dsi_clk_in_ecg_state(void *client);
#endif /* _MDSS_DSI_CLK_H_ */
