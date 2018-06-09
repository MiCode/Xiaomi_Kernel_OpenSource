/*
 * Copyright (c) 2015-2016, 2018 The Linux Foundation. All rights reserved.
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

#ifndef _DSI_CTRL_H_
#define _DSI_CTRL_H_

#include <linux/debugfs.h>

#include "dsi_defs.h"
#include "dsi_ctrl_hw.h"
#include "dsi_clk_pwr.h"
#include "drm_mipi_dsi.h"

/*
 * DSI Command transfer modifiers
 * @DSI_CTRL_CMD_READ:             The current transfer involves reading data.
 * @DSI_CTRL_CMD_BROADCAST:        The current transfer needs to be done in
 *				   broadcast mode to multiple slaves.
 * @DSI_CTRL_CMD_BROADCAST_MASTER: This controller is the master and the slaves
 *				   sync to this trigger.
 * @DSI_CTRL_CMD_DEFER_TRIGGER:    Defer the command trigger to later.
 * @DSI_CTRL_CMD_FIFO_STORE:       Use FIFO for command transfer in place of
 *				   reading data from memory.
 */
#define DSI_CTRL_CMD_READ             0x1
#define DSI_CTRL_CMD_BROADCAST        0x2
#define DSI_CTRL_CMD_BROADCAST_MASTER 0x4
#define DSI_CTRL_CMD_DEFER_TRIGGER    0x8
#define DSI_CTRL_CMD_FIFO_STORE       0x10

/**
 * enum dsi_power_state - defines power states for dsi controller.
 * @DSI_CTRL_POWER_OFF:         DSI controller is powered down.
 * @DSI_CTRL_POWER_VREG_ON:     Digital and analog supplies for DSI controller
 *				are powered on.
 * @DSI_CTRL_POWER_CORE_CLK_ON: DSI core clocks for register access are enabled.
 * @DSI_CTRL_POWER_LINK_CLK_ON: DSI link clocks for link transfer are enabled.
 * @DSI_CTRL_POWER_MAX:         Maximum value.
 */
enum dsi_power_state {
	DSI_CTRL_POWER_OFF = 0,
	DSI_CTRL_POWER_VREG_ON,
	DSI_CTRL_POWER_CORE_CLK_ON,
	DSI_CTRL_POWER_LINK_CLK_ON,
	DSI_CTRL_POWER_MAX,
};

/**
 * enum dsi_engine_state - define engine status for dsi controller.
 * @DSI_CTRL_ENGINE_OFF:  Engine is turned off.
 * @DSI_CTRL_ENGINE_ON:   Engine is turned on.
 * @DSI_CTRL_ENGINE_MAX:  Maximum value.
 */
enum dsi_engine_state {
	DSI_CTRL_ENGINE_OFF = 0,
	DSI_CTRL_ENGINE_ON,
	DSI_CTRL_ENGINE_MAX,
};

/**
 * struct dsi_ctrl_power_info - digital and analog power supplies for dsi host
 * @digital:  Digital power supply required to turn on DSI controller hardware.
 * @host_pwr: Analog power supplies required to turn on DSI controller hardware.
 *            Even though DSI controller it self does not require an analog
 *            power supply, supplies required for PLL can be defined here to
 *            allow proper control over these supplies.
 */
struct dsi_ctrl_power_info {
	struct dsi_regulator_info digital;
	struct dsi_regulator_info host_pwr;
};

/**
 * struct dsi_ctrl_clk_info - clock information for DSI controller
 * @core_clks:          Core clocks needed to access DSI controller registers.
 * @link_clks:          Link clocks required to transmit data over DSI link.
 * @rcg_clks:           Root clock generation clocks generated in MMSS_CC. The
 *			output of the PLL is set as parent for these root
 *			clocks. These clocks are specific to controller
 *			instance.
 * @mux_clks:           Mux clocks used for Dynamic refresh feature.
 * @ext_clks:           External byte/pixel clocks from the MMSS block. These
 *			clocks are set as parent to rcg clocks.
 * @pll_op_clks:        TODO:
 * @shadow_clks:        TODO:
 */
struct dsi_ctrl_clk_info {
	/* Clocks parsed from DT */
	struct dsi_core_clk_info core_clks;
	struct dsi_link_clk_info link_clks;
	struct dsi_clk_link_set rcg_clks;

	/* Clocks set by DSI Manager */
	struct dsi_clk_link_set mux_clks;
	struct dsi_clk_link_set ext_clks;
	struct dsi_clk_link_set pll_op_clks;
	struct dsi_clk_link_set shadow_clks;
};

/**
 * struct dsi_ctrl_bus_scale_info - Bus scale info for msm-bus bandwidth voting
 * @bus_scale_table:        Bus scale voting usecases.
 * @bus_handle:             Handle used for voting bandwidth.
 * @refcount:               reference count.
 */
struct dsi_ctrl_bus_scale_info {
	struct msm_bus_scale_pdata *bus_scale_table;
	u32 bus_handle;
	u32 refcount;
};

/**
 * struct dsi_ctrl_state_info - current driver state information
 * @power_state:        Controller power state.
 * @cmd_engine_state:   Status of DSI command engine.
 * @vid_engine_state:   Status of DSI video engine.
 * @controller_state:   Status of DSI Controller engine.
 * @pwr_enabled:        Set to true, if voltage supplies are enabled.
 * @core_clk_enabled:   Set to true, if core clocks are enabled.
 * @lin_clk_enabled:    Set to true, if link clocks are enabled.
 * @ulps_enabled:       Set to true, if lanes are in ULPS state.
 * @clamp_enabled:      Set to true, if PHY output is clamped.
 * @clk_source_set:     Set to true, if parent is set for DSI link clocks.
 */
struct dsi_ctrl_state_info {
	enum dsi_power_state power_state;
	enum dsi_engine_state cmd_engine_state;
	enum dsi_engine_state vid_engine_state;
	enum dsi_engine_state controller_state;
	bool pwr_enabled;
	bool core_clk_enabled;
	bool link_clk_enabled;
	bool ulps_enabled;
	bool clamp_enabled;
	bool clk_source_set;
	bool host_initialized;
	bool tpg_enabled;
};

/**
 * struct dsi_ctrl_interrupts - define interrupt information
 * @irq:                   IRQ id for the DSI controller.
 * @intr_lock:             Spinlock to protect access to interrupt registers.
 * @interrupt_status:      Status interrupts which need to be serviced.
 * @error_status:          Error interurpts which need to be serviced.
 * @interrupts_enabled:    Status interrupts which are enabled.
 * @errors_enabled:        Error interrupts which are enabled.
 * @cmd_dma_done:          Completion signal for DSI_CMD_MODE_DMA_DONE interrupt
 * @vid_frame_done:        Completion signal for DSI_VIDEO_MODE_FRAME_DONE int.
 * @cmd_frame_done:        Completion signal for DSI_CMD_FRAME_DONE interrupt.
 * @interrupt_done_work:   Work item for servicing status interrupts.
 * @error_status_work:     Work item for servicing error interrupts.
 */
struct dsi_ctrl_interrupts {
	u32 irq;
	spinlock_t intr_lock; /* protects access to interrupt registers */
	u32 interrupt_status;
	u64 error_status;

	u32 interrupts_enabled;
	u64 errors_enabled;

	struct completion cmd_dma_done;
	struct completion vid_frame_done;
	struct completion cmd_frame_done;

	struct work_struct interrupt_done_work;
	struct work_struct error_status_work;
};

/**
 * struct dsi_ctrl - DSI controller object
 * @pdev:                Pointer to platform device.
 * @index:               Instance id.
 * @name:                Name of the controller instance.
 * @refcount:            ref counter.
 * @ctrl_lock:           Mutex for hardware and object access.
 * @drm_dev:             Pointer to DRM device.
 * @version:             DSI controller version.
 * @hw:                  DSI controller hardware object.
 * @current_state;       Current driver and hardware state.
 * @int_info:            Interrupt information.
 * @clk_info:            Clock information.
 * @pwr_info:            Power information.
 * @axi_bus_info:        AXI bus information.
 * @host_config:         Current host configuration.
 * @tx_cmd_buf:          Tx command buffer.
 * @cmd_buffer_size:     Size of command buffer.
 * @debugfs_root:        Root for debugfs entries.
 */
struct dsi_ctrl {
	struct platform_device *pdev;
	u32 index;
	const char *name;
	u32 refcount;
	struct mutex ctrl_lock;
	struct drm_device *drm_dev;

	enum dsi_ctrl_version version;
	struct dsi_ctrl_hw hw;

	/* Current state */
	struct dsi_ctrl_state_info current_state;

	struct dsi_ctrl_interrupts int_info;
	/* Clock and power states */
	struct dsi_ctrl_clk_info clk_info;
	struct dsi_ctrl_power_info pwr_info;
	struct dsi_ctrl_bus_scale_info axi_bus_info;

	struct dsi_host_config host_config;
	/* Command tx and rx */
	struct drm_gem_object *tx_cmd_buf;
	u32 cmd_buffer_size;

	/* Debug Information */
	struct dentry *debugfs_root;

};

/**
 * dsi_ctrl_get() - get a dsi_ctrl handle from an of_node
 * @of_node:    of_node of the DSI controller.
 *
 * Gets the DSI controller handle for the corresponding of_node. The ref count
 * is incremented to one and all subsequent gets will fail until the original
 * clients calls a put.
 *
 * Return: DSI Controller handle.
 */
struct dsi_ctrl *dsi_ctrl_get(struct device_node *of_node);

/**
 * dsi_ctrl_put() - releases a dsi controller handle.
 * @dsi_ctrl:       DSI controller handle.
 *
 * Releases the DSI controller. Driver will clean up all resources and puts back
 * the DSI controller into reset state.
 */
void dsi_ctrl_put(struct dsi_ctrl *dsi_ctrl);

/**
 * dsi_ctrl_drv_init() - initialize dsi controller driver.
 * @dsi_ctrl:      DSI controller handle.
 * @parent:        Parent directory for debug fs.
 *
 * Initializes DSI controller driver. Driver should be initialized after
 * dsi_ctrl_get() succeeds.
 *
 * Return: error code.
 */
int dsi_ctrl_drv_init(struct dsi_ctrl *dsi_ctrl, struct dentry *parent);

/**
 * dsi_ctrl_drv_deinit() - de-initializes dsi controller driver
 * @dsi_ctrl:      DSI controller handle.
 *
 * Releases all resources acquired by dsi_ctrl_drv_init().
 *
 * Return: error code.
 */
int dsi_ctrl_drv_deinit(struct dsi_ctrl *dsi_ctrl);

/**
 * dsi_ctrl_validate_timing() - validate a video timing configuration
 * @dsi_ctrl:       DSI controller handle.
 * @timing:         Pointer to timing data.
 *
 * Driver will validate if the timing configuration is supported on the
 * controller hardware.
 *
 * Return: error code if timing is not supported.
 */
int dsi_ctrl_validate_timing(struct dsi_ctrl *dsi_ctrl,
			     struct dsi_mode_info *timing);

/**
 * dsi_ctrl_update_host_config() - update dsi host configuration
 * @dsi_ctrl:          DSI controller handle.
 * @config:            DSI host configuration.
 * @flags:             dsi_mode_flags modifying the behavior
 *
 * Updates driver with new Host configuration to use for host initialization.
 * This function call will only update the software context. The stored
 * configuration information will be used when the host is initialized.
 *
 * Return: error code.
 */
int dsi_ctrl_update_host_config(struct dsi_ctrl *dsi_ctrl,
				struct dsi_host_config *config,
				int flags);

/**
 * dsi_ctrl_async_timing_update() - update only controller timing
 * @dsi_ctrl:          DSI controller handle.
 * @timing:            New DSI timing info
 *
 * Updates host timing values to asynchronously transition to new timing
 * For example, to update the porch values in a seamless/dynamic fps switch.
 *
 * Return: error code.
 */
int dsi_ctrl_async_timing_update(struct dsi_ctrl *dsi_ctrl,
		struct dsi_mode_info *timing);

/**
 * dsi_ctrl_phy_sw_reset() - perform a PHY software reset
 * @dsi_ctrl:         DSI controller handle.
 *
 * Performs a PHY software reset on the DSI controller. Reset should be done
 * when the controller power state is DSI_CTRL_POWER_CORE_CLK_ON and the PHY is
 * not enabled.
 *
 * This function will fail if driver is in any other state.
 *
 * Return: error code.
 */
int dsi_ctrl_phy_sw_reset(struct dsi_ctrl *dsi_ctrl);

/**
 * dsi_ctrl_host_init() - Initialize DSI host hardware.
 * @dsi_ctrl:        DSI controller handle.
 * @cont_splash_enabled:    Flag for DSI splash enabled in bootloader.
 *
 * Initializes DSI controller hardware with host configuration provided by
 * dsi_ctrl_update_host_config(). Initialization can be performed only during
 * DSI_CTRL_POWER_CORE_CLK_ON state and after the PHY SW reset has been
 * performed.
 *
 * Return: error code.
 */
int dsi_ctrl_host_init(struct dsi_ctrl *dsi_ctrl, bool cont_splash_enabled);

/**
 * dsi_ctrl_host_deinit() - De-Initialize DSI host hardware.
 * @dsi_ctrl:        DSI controller handle.
 *
 * De-initializes DSI controller hardware. It can be performed only during
 * DSI_CTRL_POWER_CORE_CLK_ON state after LINK clocks have been turned off.
 *
 * Return: error code.
 */
int dsi_ctrl_host_deinit(struct dsi_ctrl *dsi_ctrl);

/**
 * dsi_ctrl_set_tpg_state() - enable/disable test pattern on the controller
 * @dsi_ctrl:          DSI controller handle.
 * @on:                enable/disable test pattern.
 *
 * Test pattern can be enabled only after Video engine (for video mode panels)
 * or command engine (for cmd mode panels) is enabled.
 *
 * Return: error code.
 */
int dsi_ctrl_set_tpg_state(struct dsi_ctrl *dsi_ctrl, bool on);

/**
 * dsi_ctrl_cmd_transfer() - Transfer commands on DSI link
 * @dsi_ctrl:             DSI controller handle.
 * @msg:                  Message to transfer on DSI link.
 * @flags:                Modifiers for message transfer.
 *
 * Command transfer can be done only when command engine is enabled. The
 * transfer API will until either the command transfer finishes or the timeout
 * value is reached. If the trigger is deferred, it will return without
 * triggering the transfer. Command parameters are programmed to hardware.
 *
 * Return: error code.
 */
int dsi_ctrl_cmd_transfer(struct dsi_ctrl *dsi_ctrl,
			  const struct mipi_dsi_msg *msg,
			  u32 flags);

/**
 * dsi_ctrl_cmd_tx_trigger() - Trigger a deferred command.
 * @dsi_ctrl:              DSI controller handle.
 * @flags:                 Modifiers.
 *
 * Return: error code.
 */
int dsi_ctrl_cmd_tx_trigger(struct dsi_ctrl *dsi_ctrl, u32 flags);

/**
 * dsi_ctrl_set_power_state() - set power state for dsi controller
 * @dsi_ctrl:          DSI controller handle.
 * @state:             Power state.
 *
 * Set power state for DSI controller. Power state can be changed only when
 * Controller, Video and Command engines are turned off.
 *
 * Return: error code.
 */
int dsi_ctrl_set_power_state(struct dsi_ctrl *dsi_ctrl,
			     enum dsi_power_state state);

/**
 * dsi_ctrl_set_cmd_engine_state() - set command engine state
 * @dsi_ctrl:            DSI Controller handle.
 * @state:               Engine state.
 *
 * Command engine state can be modified only when DSI controller power state is
 * set to DSI_CTRL_POWER_LINK_CLK_ON.
 *
 * Return: error code.
 */
int dsi_ctrl_set_cmd_engine_state(struct dsi_ctrl *dsi_ctrl,
				  enum dsi_engine_state state);

/**
 * dsi_ctrl_set_vid_engine_state() - set video engine state
 * @dsi_ctrl:            DSI Controller handle.
 * @state:               Engine state.
 *
 * Video engine state can be modified only when DSI controller power state is
 * set to DSI_CTRL_POWER_LINK_CLK_ON.
 *
 * Return: error code.
 */
int dsi_ctrl_set_vid_engine_state(struct dsi_ctrl *dsi_ctrl,
				  enum dsi_engine_state state);

/**
 * dsi_ctrl_set_host_engine_state() - set host engine state
 * @dsi_ctrl:            DSI Controller handle.
 * @state:               Engine state.
 *
 * Host engine state can be modified only when DSI controller power state is
 * set to DSI_CTRL_POWER_LINK_CLK_ON and cmd, video engines are disabled.
 *
 * Return: error code.
 */
int dsi_ctrl_set_host_engine_state(struct dsi_ctrl *dsi_ctrl,
				   enum dsi_engine_state state);

/**
 * dsi_ctrl_set_ulps() - set ULPS state for DSI lanes.
 * @dsi_ctrl:         DSI controller handle.
 * @enable:           enable/disable ULPS.
 *
 * ULPS can be enabled/disabled after DSI host engine is turned on.
 *
 * Return: error code.
 */
int dsi_ctrl_set_ulps(struct dsi_ctrl *dsi_ctrl, bool enable);

/**
 * dsi_ctrl_set_clamp_state() - set clamp state for DSI phy
 * @dsi_ctrl:             DSI controller handle.
 * @enable:               enable/disable clamping.
 *
 * Clamps can be enabled/disabled while DSI contoller is still turned on.
 *
 * Return: error code.
 */
int dsi_ctrl_set_clamp_state(struct dsi_ctrl *dsi_Ctrl, bool enable);

/**
 * dsi_ctrl_set_clock_source() - set clock source fpr dsi link clocks
 * @dsi_ctrl:        DSI controller handle.
 * @source_clks:     Source clocks for DSI link clocks.
 *
 * Clock source should be changed while link clocks are disabled.
 *
 * Return: error code.
 */
int dsi_ctrl_set_clock_source(struct dsi_ctrl *dsi_ctrl,
			      struct dsi_clk_link_set *source_clks);

/**
 * dsi_ctrl_drv_register() - register platform driver for dsi controller
 */
void dsi_ctrl_drv_register(void);

/**
 * dsi_ctrl_drv_unregister() - unregister platform driver
 */
void dsi_ctrl_drv_unregister(void);

#endif /* _DSI_CTRL_H_ */
