/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef _DSI_CLK_H_
#define _DSI_CLK_H_

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/clk.h>
#include "sde_power_handle.h"

#define MAX_STRING_LEN 32
#define MAX_DSI_CTRL 2

enum dsi_clk_state {
	DSI_CLK_OFF,
	DSI_CLK_ON,
	DSI_CLK_EARLY_GATE,
};

enum clk_req_client {
	DSI_CLK_REQ_MDP_CLIENT = 0,
	DSI_CLK_REQ_DSI_CLIENT,
};

enum dsi_link_clk_type {
	DSI_LINK_ESC_CLK,
	DSI_LINK_BYTE_CLK,
	DSI_LINK_PIX_CLK,
	DSI_LINK_BYTE_INTF_CLK,
	DSI_LINK_CLK_MAX,
};

enum dsi_clk_type {
	DSI_CORE_CLK = BIT(0),
	DSI_LINK_CLK = BIT(1),
	DSI_ALL_CLKS = (BIT(0) | BIT(1)),
	DSI_CLKS_MAX = BIT(2),
};

struct dsi_clk_ctrl_info {
	enum dsi_clk_type clk_type;
	enum dsi_clk_state clk_state;
	enum clk_req_client client;
};

struct clk_ctrl_cb {
	void *priv;
	int (*dsi_clk_cb)(void *priv, struct dsi_clk_ctrl_info clk_ctrl_info);
};

/**
 * struct dsi_core_clk_info - Core clock information for DSI hardware
 * @mdp_core_clk:        Handle to MDP core clock.
 * @iface_clk:           Handle to MDP interface clock.
 * @core_mmss_clk:       Handle to MMSS core clock.
 * @bus_clk:             Handle to bus clock.
 * @mnoc_clk:            Handle to MMSS NOC clock.
 * @dsi_core_client:	 Pointer to SDE power client
 * @phandle:             Pointer to SDE power handle
 */
struct dsi_core_clk_info {
	struct clk *mdp_core_clk;
	struct clk *iface_clk;
	struct clk *core_mmss_clk;
	struct clk *bus_clk;
	struct clk *mnoc_clk;
	struct sde_power_client *dsi_core_client;
	struct sde_power_handle *phandle;
};

/**
 * struct dsi_link_clk_info - Link clock information for DSI hardware.
 * @byte_clk:        Handle to DSI byte clock.
 * @pixel_clk:       Handle to DSI pixel clock.
 * @esc_clk:         Handle to DSI escape clock.
 * @byte_intf_clk:   Handle to DSI byte intf. clock.
 */
struct dsi_link_clk_info {
	struct clk *byte_clk;
	struct clk *pixel_clk;
	struct clk *esc_clk;
	struct clk *byte_intf_clk;
};

/**
 * struct link_clk_freq - Clock frequency information for Link clocks
 * @byte_clk_rate:   Frequency of DSI byte clock in KHz.
 * @pixel_clk_rate:  Frequency of DSI pixel clock in KHz.
 * @esc_clk_rate:    Frequency of DSI escape clock in KHz.
 */
struct link_clk_freq {
	u32 byte_clk_rate;
	u32 pix_clk_rate;
	u32 esc_clk_rate;
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
			       enum dsi_clk_type clk_type,
			       enum dsi_clk_state new_state);

/**
 * typedef *post_clockoff_cb() - Callback after clock is turned off
 * @priv: private data pointer.
 * @clk_type: clock which was turned off.
 * @curr_state: current state for the clock.
 *
 * @return: error code.
 */
typedef int (*post_clockoff_cb)(void *priv,
				enum dsi_clk_type clk_type,
				enum dsi_clk_state curr_state);

/**
 * typedef *post_clockon_cb() - Callback after clock is turned on
 * @priv: private data pointer.
 * @clk_type: clock which was turned on.
 * @curr_state: current state for the clock.
 *
 * @return: error code.
 */
typedef int (*post_clockon_cb)(void *priv,
			       enum dsi_clk_type clk_type,
			       enum dsi_clk_state curr_state);

/**
 * typedef *pre_clockon_cb() - Callback before clock is turned on
 * @priv: private data pointer.
 * @clk_type: clock which is being turned on.
 * @new_state: next state for the clock.
 *
 * @return: error code.
 */
typedef int (*pre_clockon_cb)(void *priv,
			      enum dsi_clk_type clk_type,
			      enum dsi_clk_state new_state);


/**
 * struct dsi_clk_info - clock information for DSI hardware.
 * @name:                    client name.
 * @c_clks[MAX_DSI_CTRL]     array of core clock configurations
 * @l_clks[MAX_DSI_CTRL]     array of link clock configurations
 * @bus_handle[MAX_DSI_CTRL] array of bus handles
 * @ctrl_index[MAX_DSI_CTRL] array of DSI controller indexes mapped
 *                           to core and link clock configurations
 * @pre_clkoff_cb            callback before clock is turned off
 * @post_clkoff_cb           callback after clock is turned off
 * @post_clkon_cb            callback after clock is turned on
 * @pre_clkon_cb             callback before clock is turned on
 * @priv_data                pointer to private data
 * @master_ndx               master DSI controller index
 * @dsi_ctrl_count           number of DSI controllers
 */
struct dsi_clk_info {
	char name[MAX_STRING_LEN];
	struct dsi_core_clk_info c_clks[MAX_DSI_CTRL];
	struct dsi_link_clk_info l_clks[MAX_DSI_CTRL];
	u32 bus_handle[MAX_DSI_CTRL];
	u32 ctrl_index[MAX_DSI_CTRL];
	pre_clockoff_cb pre_clkoff_cb;
	post_clockoff_cb post_clkoff_cb;
	post_clockon_cb post_clkon_cb;
	pre_clockon_cb pre_clkon_cb;
	void *priv_data;
	u32 master_ndx;
	u32 dsi_ctrl_count;
};

/**
 * struct dsi_clk_link_set - Pair of clock handles to describe link clocks
 * @byte_clk:     Handle to DSi byte clock.
 * @pixel_clk:    Handle to DSI pixel clock.
 */
struct dsi_clk_link_set {
	struct clk *byte_clk;
	struct clk *pixel_clk;
};

/**
 * dsi_display_clk_mngr_update_splash_status() - Update splash stattus
 * @clk_mngr:     Structure containing DSI clock information
 * @status:     Splash status
 */
void dsi_display_clk_mngr_update_splash_status(void *clk_mgr, bool status);

/**
 * dsi_display_clk_mgr_register() - Register DSI clock manager
 * @info:     Structure containing DSI clock information
 */
void *dsi_display_clk_mngr_register(struct dsi_clk_info *info);

/**
 * dsi_display_clk_mngr_deregister() - Deregister DSI clock manager
 * @clk_mngr:  DSI clock manager pointer
 */
int dsi_display_clk_mngr_deregister(void *clk_mngr);

/**
 * dsi_register_clk_handle() - Register clock handle with DSI clock manager
 * @clk_mngr:  DSI clock manager pointer
 * @client:     DSI clock client pointer.
 */
void *dsi_register_clk_handle(void *clk_mngr, char *client);

/**
 * dsi_deregister_clk_handle() - Deregister clock handle from DSI clock manager
 * @client:     DSI clock client pointer.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_deregister_clk_handle(void *client);

/**
 * dsi_display_clk_ctrl() - set frequencies for link clks
 * @handle:     Handle of desired DSI clock client.
 * @clk_type:   Clock which is being controlled.
 * @clk_state:  Desired state of clock
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_display_clk_ctrl(void *handle,
	enum dsi_clk_type clk_type, enum dsi_clk_state clk_state);

/**
 * dsi_clk_set_link_frequencies() - set frequencies for link clks
 * @client:     DSI clock client pointer.
 * @freq:       Structure containing link clock frequencies.
 * @index:      Index of the DSI controller.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_set_link_frequencies(void *client, struct link_clk_freq freq,
					u32 index);


/**
 * dsi_clk_set_pixel_clk_rate() - set frequency for pixel clock
 * @client:       DSI clock client pointer.
 * @pixel_clk: Pixel clock rate in Hz.
 * @index:      Index of the DSI controller.
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_set_pixel_clk_rate(void *client, u64 pixel_clk, u32 index);


/**
 * dsi_clk_set_byte_clk_rate() - set frequency for byte clock
 * @client:       DSI clock client pointer.
 * @byte_clk: Pixel clock rate in Hz.
 * @index:      Index of the DSI controller.
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_set_byte_clk_rate(void *client, u64 byte_clk, u32 index);

/**
 * dsi_clk_update_parent() - update parent clocks for specified clock
 * @parent:       link clock pair which are set as parent.
 * @child:        link clock pair whose parent has to be set.
 */
int dsi_clk_update_parent(struct dsi_clk_link_set *parent,
			  struct dsi_clk_link_set *child);
#endif /* _DSI_CLK_H_ */
