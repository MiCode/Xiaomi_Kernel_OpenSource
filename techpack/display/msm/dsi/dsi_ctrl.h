/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DSI_CTRL_H_
#define _DSI_CTRL_H_

#include <linux/debugfs.h>

#include "dsi_defs.h"
#include "dsi_ctrl_hw.h"
#include "dsi_clk.h"
#include "dsi_pwr.h"
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
 * @DSI_CTRL_CMD_FETCH_MEMORY:     Fetch command from memory through AXI bus
 *				   and transfer it.
 * @DSI_CTRL_CMD_LAST_COMMAND:     Trigger the DMA cmd transfer if this is last
 *				   command in the batch.
 * @DSI_CTRL_CMD_NON_EMBEDDED_MODE:Transfer cmd packets in non embedded mode.
 * @DSI_CTRL_CMD_CUSTOM_DMA_SCHED: Use the dma scheduling line number defined in
 *				   display panel dtsi file instead of default.
 * @DSI_CTRL_CMD_ASYNC_WAIT: Command flag to indicate that the wait for done
 *			for this command is asynchronous and must be queued.
 */
#define DSI_CTRL_CMD_READ             0x1
#define DSI_CTRL_CMD_BROADCAST        0x2
#define DSI_CTRL_CMD_BROADCAST_MASTER 0x4
#define DSI_CTRL_CMD_DEFER_TRIGGER    0x8
#define DSI_CTRL_CMD_FIFO_STORE       0x10
#define DSI_CTRL_CMD_FETCH_MEMORY     0x20
#define DSI_CTRL_CMD_LAST_COMMAND     0x40
#define DSI_CTRL_CMD_NON_EMBEDDED_MODE 0x80
#define DSI_CTRL_CMD_CUSTOM_DMA_SCHED  0x100
#define DSI_CTRL_CMD_ASYNC_WAIT 0x200

/* DSI embedded mode fifo size
 * If the command is greater than 256 bytes it is sent in non-embedded mode.
 */
#define DSI_EMBEDDED_MODE_DMA_MAX_SIZE_BYTES 256

/* max size supported for dsi cmd transfer using TPG */
#define DSI_CTRL_MAX_CMD_FIFO_STORE_SIZE 64

/**
 * enum dsi_power_state - defines power states for dsi controller.
 * @DSI_CTRL_POWER_VREG_OFF:    Digital and analog supplies for DSI controller
				turned off
 * @DSI_CTRL_POWER_VREG_ON:     Digital and analog supplies for DSI controller
 * @DSI_CTRL_POWER_MAX:         Maximum value.
 */
enum dsi_power_state {
	DSI_CTRL_POWER_VREG_OFF = 0,
	DSI_CTRL_POWER_VREG_ON,
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
 * enum dsi_ctrl_driver_ops - controller driver ops
 */
enum dsi_ctrl_driver_ops {
	DSI_CTRL_OP_POWER_STATE_CHANGE,
	DSI_CTRL_OP_CMD_ENGINE,
	DSI_CTRL_OP_VID_ENGINE,
	DSI_CTRL_OP_HOST_ENGINE,
	DSI_CTRL_OP_CMD_TX,
	DSI_CTRL_OP_HOST_INIT,
	DSI_CTRL_OP_TPG,
	DSI_CTRL_OP_PHY_SW_RESET,
	DSI_CTRL_OP_ASYNC_TIMING,
	DSI_CTRL_OP_MAX
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
 * @hs_link_clks:       Clocks required to transmit high speed data over DSI
 * @lp_link_clks:       Clocks required to perform low power ops over DSI
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
	struct dsi_link_hs_clk_info hs_link_clks;
	struct dsi_link_lp_clk_info lp_link_clks;
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
 */
struct dsi_ctrl_bus_scale_info {
	struct msm_bus_scale_pdata *bus_scale_table;
	u32 bus_handle;
};

/**
 * struct dsi_ctrl_state_info - current driver state information
 * @power_state:        Status of power states on DSI controller.
 * @cmd_engine_state:   Status of DSI command engine.
 * @vid_engine_state:   Status of DSI video engine.
 * @controller_state:   Status of DSI Controller engine.
 * @host_initialized:	Boolean to indicate status of DSi host Initialization
 * @tpg_enabled:        Boolean to indicate whether tpg is enabled.
 */
struct dsi_ctrl_state_info {
	enum dsi_power_state power_state;
	enum dsi_engine_state cmd_engine_state;
	enum dsi_engine_state vid_engine_state;
	enum dsi_engine_state controller_state;
	bool host_initialized;
	bool tpg_enabled;
};

/**
 * struct dsi_ctrl_interrupts - define interrupt information
 * @irq_lock:            Spinlock for ISR handler.
 * @irq_num:             Linux interrupt number associated with device.
 * @irq_stat_mask:       Hardware mask of currently enabled interrupts.
 * @irq_stat_refcount:   Number of times each interrupt has been requested.
 * @irq_stat_cb:         Status IRQ callback definitions.
 * @irq_err_cb:          IRQ callback definition to handle DSI ERRORs.
 * @cmd_dma_done:          Completion signal for DSI_CMD_MODE_DMA_DONE interrupt
 * @vid_frame_done:        Completion signal for DSI_VIDEO_MODE_FRAME_DONE int.
 * @cmd_frame_done:        Completion signal for DSI_CMD_FRAME_DONE interrupt.
 */
struct dsi_ctrl_interrupts {
	spinlock_t irq_lock;
	int irq_num;
	uint32_t irq_stat_mask;
	int irq_stat_refcount[DSI_STATUS_INTERRUPT_COUNT];
	struct dsi_event_cb_info irq_stat_cb[DSI_STATUS_INTERRUPT_COUNT];
	struct dsi_event_cb_info irq_err_cb;

	struct completion cmd_dma_done;
	struct completion vid_frame_done;
	struct completion cmd_frame_done;
	struct completion bta_done;
};

/**
 * struct dsi_ctrl - DSI controller object
 * @pdev:                Pointer to platform device.
 * @cell_index:          Instance cell id.
 * @horiz_index:         Index in physical horizontal CTRL layout, 0 = leftmost
 * @name:                Name of the controller instance.
 * @refcount:            ref counter.
 * @ctrl_lock:           Mutex for hardware and object access.
 * @drm_dev:             Pointer to DRM device.
 * @version:             DSI controller version.
 * @hw:                  DSI controller hardware object.
 * @current_state:       Current driver and hardware state.
 * @clk_cb:		 Callback for DSI clock control.
 * @irq_info:            Interrupt information.
 * @recovery_cb:         Recovery call back to SDE.
 * @clk_info:            Clock information.
 * @clk_freq:            DSi Link clock frequency information.
 * @pwr_info:            Power information.
 * @axi_bus_info:        AXI bus information.
 * @host_config:         Current host configuration.
 * @mode_bounds:         Boundaries of the default mode ROI.
 *                       Origin is at top left of all CTRLs.
 * @roi:                 Partial update region of interest.
 *                       Origin is top left of this CTRL.
 * @tx_cmd_buf:          Tx command buffer.
 * @cmd_buffer_iova:     cmd buffer mapped address.
 * @cmd_buffer_size:     Size of command buffer.
 * @vaddr:               CPU virtual address of cmd buffer.
 * @secure_mode:         Indicates if secure-session is in progress
 * @esd_check_underway:  Indicates if esd status check is in progress
 * @dma_cmd_wait:	Work object waiting on DMA command transfer done.
 * @dma_cmd_workq:	Pointer to the workqueue of DMA command transfer done
 *				wait sequence.
 * @dma_wait_queued:	Indicates if any DMA command transfer wait work
 *				is queued.
 * @dma_irq_trig:		 Atomic state to indicate DMA done IRQ
 *				triggered.
 * @debugfs_root:        Root for debugfs entries.
 * @misr_enable:         Frame MISR enable/disable
 * @misr_cache:          Cached Frame MISR value
 * @frame_threshold_time_us: Frame threshold time in microseconds, where
 *		 	  dsi data lane will be idle i.e from pingpong done to
 *			  next TE for command mode.
 * @phy_isolation_enabled:    A boolean property allows to isolate the phy from
 *                          dsi controller and run only dsi controller.
 * @null_insertion_enabled:  A boolean property to allow dsi controller to
 *                           insert null packet.
 * @modeupdated:	  Boolean to send new roi if mode is updated.
 * @split_link_supported: Boolean to check if hw supports split link.
 */
struct dsi_ctrl {
	struct platform_device *pdev;
	u32 cell_index;
	u32 horiz_index;
	const char *name;
	u32 refcount;
	struct mutex ctrl_lock;
	struct drm_device *drm_dev;

	enum dsi_ctrl_version version;
	struct dsi_ctrl_hw hw;

	/* Current state */
	struct dsi_ctrl_state_info current_state;
	struct clk_ctrl_cb clk_cb;

	struct dsi_ctrl_interrupts irq_info;
	struct dsi_event_cb_info recovery_cb;

	/* Clock and power states */
	struct dsi_ctrl_clk_info clk_info;
	struct link_clk_freq clk_freq;
	struct dsi_ctrl_power_info pwr_info;
	struct dsi_ctrl_bus_scale_info axi_bus_info;

	struct dsi_host_config host_config;
	struct dsi_rect mode_bounds;
	struct dsi_rect roi;

	/* Command tx and rx */
	struct drm_gem_object *tx_cmd_buf;
	u32 cmd_buffer_size;
	u32 cmd_buffer_iova;
	u32 cmd_len;
	void *vaddr;
	bool secure_mode;
	bool esd_check_underway;
	struct work_struct dma_cmd_wait;
	struct workqueue_struct *dma_cmd_workq;
	bool dma_wait_queued;
	atomic_t dma_irq_trig;

	/* Debug Information */
	struct dentry *debugfs_root;

	/* MISR */
	bool misr_enable;
	u32 misr_cache;

	u32 frame_threshold_time_us;

	/* Check for spurious interrupts */
	unsigned long jiffies_start;
	unsigned int error_interrupt_count;

	bool phy_isolation_enabled;
	bool null_insertion_enabled;
	bool modeupdated;
	bool split_link_supported;
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
 * @mode:              DSI host mode selected.
 * @flags:             dsi_mode_flags modifying the behavior
 * @clk_handle:        Clock handle for DSI clocks
 *
 * Updates driver with new Host configuration to use for host initialization.
 * This function call will only update the software context. The stored
 * configuration information will be used when the host is initialized.
 *
 * Return: error code.
 */
int dsi_ctrl_update_host_config(struct dsi_ctrl *dsi_ctrl,
				struct dsi_host_config *config,
				struct dsi_display_mode *mode, int flags,
				void *clk_handle);

/**
 * dsi_ctrl_timing_db_update() - update only controller Timing DB
 * @dsi_ctrl:          DSI controller handle.
 * @enable:            Enable/disable Timing DB register
 *
 * Update timing db register value during dfps usecases
 *
 * Return: error code.
 */
int dsi_ctrl_timing_db_update(struct dsi_ctrl *dsi_ctrl,
		bool enable);

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
 * dsi_ctrl_phy_reset_config() - Mask/unmask propagation of ahb reset signal
 *	to DSI PHY hardware.
 * @dsi_ctrl:        DSI controller handle.
 * @enable:			Mask/unmask the PHY reset signal.
 *
 * Return: error code.
 */
int dsi_ctrl_phy_reset_config(struct dsi_ctrl *dsi_ctrl, bool enable);

/**
 * dsi_ctrl_config_clk_gating() - Enable/Disable DSI PHY clk gating
 * @dsi_ctrl:        DSI controller handle.
 * @enable:          Enable/disable DSI PHY clk gating
 * @clk_selection:   clock selection for gating
 *
 * Return: error code.
 */
int dsi_ctrl_config_clk_gating(struct dsi_ctrl *dsi_ctrl, bool enable,
		 enum dsi_clk_gate_type clk_selection);

/**
 * dsi_ctrl_soft_reset() - perform a soft reset on DSI controller
 * @dsi_ctrl:         DSI controller handle.
 *
 * The video, command and controller engines will be disabled before the
 * reset is triggered. After, the engines will be re-enabled to the same state
 * as before the reset.
 *
 * If the reset is done while MDP timing engine is turned on, the video
 * engine should be re-enabled only during the vertical blanking time.
 *
 * Return: error code
 */
int dsi_ctrl_soft_reset(struct dsi_ctrl *dsi_ctrl);

/**
 * dsi_ctrl_host_timing_update - reinitialize host with new timing values
 * @dsi_ctrl:         DSI controller handle.
 *
 * Reinitialize DSI controller hardware with new display timing values
 * when resolution is switched dynamically.
 *
 * Return: error code
 */
int dsi_ctrl_host_timing_update(struct dsi_ctrl *dsi_ctrl);

/**
 * dsi_ctrl_host_init() - Initialize DSI host hardware.
 * @dsi_ctrl:        DSI controller handle.
 * @is_splash_enabled:       boolean signifying splash status.
 *
 * Initializes DSI controller hardware with host configuration provided by
 * dsi_ctrl_update_host_config(). Initialization can be performed only during
 * DSI_CTRL_POWER_CORE_CLK_ON state and after the PHY SW reset has been
 * performed.
 *
 * Return: error code.
 */
int dsi_ctrl_host_init(struct dsi_ctrl *dsi_ctrl, bool is_splash_enabled);

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
 * dsi_ctrl_set_ulps() - set ULPS state for DSI lanes.
 * @dsi_ctrl:		DSI controller handle.
 * @enable:		enable/disable ULPS.
 *
 * ULPS can be enabled/disabled after DSI host engine is turned on.
 *
 * Return: error code.
 */
int dsi_ctrl_set_ulps(struct dsi_ctrl *dsi_ctrl, bool enable);

/**
 * dsi_ctrl_timing_setup() - Setup DSI host config
 * @dsi_ctrl:        DSI controller handle.
 *
 * Initializes DSI controller hardware with host configuration provided by
 * dsi_ctrl_update_host_config(). This is called while setting up DSI host
 * through dsi_ctrl_setup() and after any ROI change.
 *
 * Also used to program the video mode timing values.
 *
 * Return: error code.
 */
int dsi_ctrl_timing_setup(struct dsi_ctrl *dsi_ctrl);

/**
 * dsi_ctrl_setup() - Setup DSI host hardware while coming out of idle screen.
 * @dsi_ctrl:        DSI controller handle.
 *
 * Initialization of DSI controller hardware with host configuration and
 * enabling required interrupts. Initialization can be performed only during
 * DSI_CTRL_POWER_CORE_CLK_ON state and after the PHY SW reset has been
 * performed.
 *
 * Return: error code.
 */
int dsi_ctrl_setup(struct dsi_ctrl *dsi_ctrl);

/**
 * dsi_ctrl_set_roi() - Set DSI controller's region of interest
 * @dsi_ctrl:        DSI controller handle.
 * @roi:             Region of interest rectangle, must be less than mode bounds
 * @changed:         Output parameter, set to true of the controller's ROI was
 *                   dirtied by setting the new ROI, and DCS cmd update needed
 *
 * Return: error code.
 */
int dsi_ctrl_set_roi(struct dsi_ctrl *dsi_ctrl, struct dsi_rect *roi,
		bool *changed);

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
			  u32 *flags);

/**
 * dsi_ctrl_cmd_tx_trigger() - Trigger a deferred command.
 * @dsi_ctrl:              DSI controller handle.
 * @flags:                 Modifiers.
 *
 * Return: error code.
 */
int dsi_ctrl_cmd_tx_trigger(struct dsi_ctrl *dsi_ctrl, u32 flags);

/**
 * dsi_ctrl_update_host_engine_state_for_cont_splash() - update engine
 *                                 states for cont splash usecase
 * @dsi_ctrl:              DSI controller handle.
 * @state:                 DSI engine state
 *
 * Return: error code.
 */
int dsi_ctrl_update_host_engine_state_for_cont_splash(struct dsi_ctrl *dsi_ctrl,
				enum dsi_engine_state state);

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
 * dsi_ctrl_validate_host_state() - validate DSI ctrl host state
 * @dsi_ctrl:            DSI Controller handle.
 *
 * Validate DSI cotroller host state
 *
 * Return: boolean indicating whether host is not initialized.
 */
bool dsi_ctrl_validate_host_state(struct dsi_ctrl *dsi_ctrl);

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
 * dsi_ctrl_clk_cb_register() - Register DSI controller clk control callback
 * @dsi_ctrl:         DSI controller handle.
 * @clk__cb:      Structure containing callback for clock control.
 *
 * Register call for DSI clock control
 *
 * Return: error code.
 */
int dsi_ctrl_clk_cb_register(struct dsi_ctrl *dsi_ctrl,
	struct clk_ctrl_cb *clk_cb);

/**
 * dsi_ctrl_set_clamp_state() - set clamp state for DSI phy
 * @dsi_ctrl:             DSI controller handle.
 * @enable:               enable/disable clamping.
 * @ulps_enabled:         ulps state.
 *
 * Clamps can be enabled/disabled while DSI controller is still turned on.
 *
 * Return: error code.
 */
int dsi_ctrl_set_clamp_state(struct dsi_ctrl *dsi_Ctrl,
		bool enable, bool ulps_enabled);

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
 * dsi_ctrl_enable_status_interrupt() - enable status interrupts
 * @dsi_ctrl:        DSI controller handle.
 * @intr_idx:        Index interrupt to disable.
 * @event_info:      Pointer to event callback definition
 */
void dsi_ctrl_enable_status_interrupt(struct dsi_ctrl *dsi_ctrl,
		uint32_t intr_idx, struct dsi_event_cb_info *event_info);

/**
 * dsi_ctrl_disable_status_interrupt() - disable status interrupts
 * @dsi_ctrl:        DSI controller handle.
 * @intr_idx:        Index interrupt to disable.
 */
void dsi_ctrl_disable_status_interrupt(
		struct dsi_ctrl *dsi_ctrl, uint32_t intr_idx);

/**
 * dsi_ctrl_setup_misr() - Setup frame MISR
 * @dsi_ctrl:              DSI controller handle.
 * @enable:                enable/disable MISR.
 * @frame_count:           Number of frames to accumulate MISR.
 *
 * Return: error code.
 */
int dsi_ctrl_setup_misr(struct dsi_ctrl *dsi_ctrl,
			bool enable,
			u32 frame_count);

/**
 * dsi_ctrl_collect_misr() - Read frame MISR
 * @dsi_ctrl:              DSI controller handle.
 *
 * Return: MISR value.
 */
u32 dsi_ctrl_collect_misr(struct dsi_ctrl *dsi_ctrl);

/**
 * dsi_ctrl_cache_misr - Cache frame MISR value
 * @dsi_ctrl:              DSI controller handle.
 */
void dsi_ctrl_cache_misr(struct dsi_ctrl *dsi_ctrl);

/**
 * dsi_ctrl_drv_register() - register platform driver for dsi controller
 */
void dsi_ctrl_drv_register(void);

/**
 * dsi_ctrl_drv_unregister() - unregister platform driver
 */
void dsi_ctrl_drv_unregister(void);

/**
 * dsi_ctrl_reset() - Reset DSI PHY CLK/DATA lane
 * @dsi_ctrl:        DSI controller handle.
 * @mask:	     Mask to indicate if CLK and/or DATA lane needs reset.
 */
int dsi_ctrl_reset(struct dsi_ctrl *dsi_ctrl, int mask);

/**
 * dsi_ctrl_get_hw_version() - read dsi controller hw revision
 * @dsi_ctrl:        DSI controller handle.
 */
int dsi_ctrl_get_hw_version(struct dsi_ctrl *dsi_ctrl);

/**
 * dsi_ctrl_vid_engine_en() - Control DSI video engine HW state
 * @dsi_ctrl:        DSI controller handle.
 * @on:		variable to control video engine ON/OFF.
 */
int dsi_ctrl_vid_engine_en(struct dsi_ctrl *dsi_ctrl, bool on);

/**
 * dsi_ctrl_setup_avr() - Set/Clear the AVR_SUPPORT_ENABLE bit
 * @dsi_ctrl:        DSI controller handle.
 * @enable:          variable to control AVR support ON/OFF.
 */
int dsi_ctrl_setup_avr(struct dsi_ctrl *dsi_ctrl, bool enable);

/**
 * @dsi_ctrl:        DSI controller handle.
 * cmd_len:	     Length of command.
 * flags:	     Config mode flags.
 */
void dsi_message_setup_tx_mode(struct dsi_ctrl *dsi_ctrl, u32 cmd_len,
		u32 *flags);

/**
 * @dsi_ctrl:        DSI controller handle.
 * cmd_len:	     Length of command.
 * flags:	     Config mode flags.
 */
int dsi_message_validate_tx_mode(struct dsi_ctrl *dsi_ctrl, u32 cmd_len,
		u32 *flags);

/**
 * dsi_ctrl_isr_configure() - API to register/deregister dsi isr
 * @dsi_ctrl:              DSI controller handle.
 * @enable:		   variable to control register/deregister isr
 */
void dsi_ctrl_isr_configure(struct dsi_ctrl *dsi_ctrl, bool enable);

/**
 * dsi_ctrl_mask_error_status_interrupts() - API to mask dsi ctrl error status
 *                                           interrupts
 * @dsi_ctrl:              DSI controller handle.
 * @idx:                   id indicating which interrupts to enable/disable.
 * @mask_enable:           boolean to enable/disable masking.
 */
void dsi_ctrl_mask_error_status_interrupts(struct dsi_ctrl *dsi_ctrl, u32 idx,
						bool mask_enable);

/**
 * dsi_ctrl_irq_update() - Put a irq vote to process DSI error
 *				interrupts at any time.
 * @dsi_ctrl:              DSI controller handle.
 * @enable:		   variable to control enable/disable irq line
 */
void dsi_ctrl_irq_update(struct dsi_ctrl *dsi_ctrl, bool enable);

/**
 * dsi_ctrl_get_host_engine_init_state() - Return host init state
 */
int dsi_ctrl_get_host_engine_init_state(struct dsi_ctrl *dsi_ctrl,
		bool *state);

/**
 * dsi_ctrl_wait_for_cmd_mode_mdp_idle() - Wait for command mode engine not to
 *				     be busy sending data from display engine.
 * @dsi_ctrl:                     DSI controller handle.
 */
int dsi_ctrl_wait_for_cmd_mode_mdp_idle(struct dsi_ctrl *dsi_ctrl);
/**
 * dsi_ctrl_update_host_state() - Set the host state
 */
int dsi_ctrl_update_host_state(struct dsi_ctrl *dsi_ctrl,
					enum dsi_ctrl_driver_ops op, bool en);

/**
 * dsi_ctrl_pixel_format_to_bpp() - returns number of bits per pxl
 */
int dsi_ctrl_pixel_format_to_bpp(enum dsi_pixel_format dst_format);

/**
 * dsi_ctrl_hs_req_sel() - API to enable continuous clk support through phy
 * @dsi_ctrl:			DSI controller handle.
 * @sel_phy:			Boolean to control whether to select phy or
 *				controller
 */
void dsi_ctrl_hs_req_sel(struct dsi_ctrl *dsi_ctrl, bool sel_phy);

/**
 * dsi_ctrl_set_continuous_clk() - API to set/unset force clock lane HS request.
 * @dsi_ctrl:                      DSI controller handle.
 * @enable:			   variable to control continuous clock.
 */
void dsi_ctrl_set_continuous_clk(struct dsi_ctrl *dsi_ctrl, bool enable);

/**
 * dsi_ctrl_wait4dynamic_refresh_done() - Poll for dynamic refresh done
 *					interrupt.
 * @dsi_ctrl:                      DSI controller handle.
 */
int dsi_ctrl_wait4dynamic_refresh_done(struct dsi_ctrl *ctrl);

/**
 * dsi_ctrl_mask_overflow() -	API to mask/unmask overflow errors.
 * @dsi_ctrl:			DSI controller handle.
 * @enable:			variable to control masking/unmasking.
 */
void dsi_ctrl_mask_overflow(struct dsi_ctrl *dsi_ctrl, bool enable);

/**
 * dsi_ctrl_clear_slave_dma_status -   API to clear slave DMA status
 * @dsi_ctrl:                   DSI controller handle.
 * @flags:                      Modifiers
 */
int dsi_ctrl_clear_slave_dma_status(struct dsi_ctrl *dsi_ctrl, u32 flags);
#endif /* _DSI_CTRL_H_ */
