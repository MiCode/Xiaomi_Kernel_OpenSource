/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Created on 12 Dec 2014
 * Author: Shashank Sharma
 */
#include <linux/types.h>
#include <intel_adf.h>
#include <intel_adf_interface.h>
#include <core/intel_dc_config.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/vlv_dc_hw.h>
#include <core/vlv/vlv_pipe.h>
#include <core/common/dsi/dsi_pipe.h>

struct intel_adf_context *context_from_hp_work(struct work_struct *work)
{
	return container_of(work, struct intel_adf_context, hotplug_work);
}

void intel_adf_shortpulse_work_function(struct work_struct *work)
{
	return;
}

void intel_adf_hotplug_work_function(struct work_struct *work)
{
	return;
}

int intel_adf_handle_events(struct intel_pipe *pipe, u32 events)
{
	int ret = 0;
	struct vlv_pipeline *pipeline = to_vlv_pipeline(pipe->pipeline);
	struct vlv_pm *pm = &pipeline->pm;

	ret = vlv_pm_flush_values(pm, events);
	if (ret)
		pr_err("ADF: %s: PM flush failed\n", __func__);

	/* Todo: Increment pipe vsync counter here */
	pr_debug("ADF: %s\n", __func__);
	return ret;
}

u32 intel_adf_set_pipe_event(struct intel_pipe *pipe, u32 event, bool enabled)
{
	u32 err = 0;
	u32 value = 0;
	u32 pipestat;

	struct vlv_pipe *vlv_pipe = &((to_vlv_pipeline(pipe->pipeline))->pipe);

	if ((enabled == false) && (event == INTEL_PIPE_EVENT_VSYNC)) {
		pr_debug("ADF: %s: Not allowing VSYNC OFF\n", __func__);
		return 0;
	}

	pr_debug("ADF: %s:\n", __func__);

	switch (event) {
	case INTEL_PIPE_EVENT_SPRITE2_FLIP:
		pipestat = SPRITE2_FLIP_DONE_EN;
		break;
	case INTEL_PIPE_EVENT_SPRITE1_FLIP:
		pipestat = SPRITE1_FLIP_DONE_EN;
		break;
	case INTEL_PIPE_EVENT_PRIMARY_FLIP:
		pipestat = PLANE_FLIP_DONE_EN;
		break;
	case INTEL_PIPE_EVENT_VSYNC:
		pipestat = VSYNC_EN;
		break;
	case INTEL_PIPE_EVENT_DPST:
		pipestat = DPST_EVENT_EN;
		break;
	default:
		err = -EINVAL;
		break;
	}

	if (err)
		goto out;

	value = REG_READ(vlv_pipe->status_offset);

	if (enabled)
		/* Enable interrupts */
		REG_WRITE(vlv_pipe->status_offset, value | pipestat);
	else
		/* Disable interrupts */
		REG_WRITE(vlv_pipe->status_offset, value & (~pipestat));
out:
	return err;
}


int intel_adf_set_event(struct intel_pipe *pipe, u16 event, bool enabled)
{
	u32 ret = 0;
	pr_debug("ADF: %s\n", __func__);

	ret = intel_adf_set_pipe_event(pipe, event, enabled);
	if (ret)
		pr_err("ADF: %s: Failed to set pipe events\n", __func__);

	return ret;
}

u32 intel_adf_get_port_events(struct intel_pipe *intel_pipe)
{
	bool found = false;
	u32 val;
	u32 events = 0;
	u32 hp_type = 0;
	u32 hp_status = REG_READ(PORT_HOTPLUG_STAT);
	enum port port;

	/*
	 * Allow hot plug status check only if the pipe has registered
	 * for a hotplug interrupt
	 */
	if (!intel_pipe->hp_reqd)
		return 0;

	port = vlv_get_connected_port(intel_pipe);

	if (port == PORT_INVALID)
		return 0;

	/* Hot pluggable ports are PORT_B and above */
	if (hp_status && port) {
		pr_debug("ADF: %s: Hotplug status = 0x%x\n",
			__func__, hp_status);

		/*
		 * Detect port hotplug:
		 * First check if there is a hotplug.
		 * Consider long pulse for display detection
		 * Short pulse as other interrupts
		 */
		val = PORT_HOTPLUG_INT_STATUS(port);
		if (hp_status & val) {
			hp_type = PORT_HOTPLUG_LONG_PULSE(port);
			if (hp_status & hp_type) {
				events |= INTEL_PORT_EVENT_HOTPLUG_DISPLAY;
				pr_info("ADF: Display hot plug, long pulse\n");
			} else {
				events |= INTEL_PORT_EVENT_SHORT;
				pr_info("ADF: Display hot plug, short pulse\n");
			}
			found = true;
		}

		/* Aux */
		val = DP_AUX_CHANNEL_INT_STATUS_G4X(port);
		if (hp_status & val) {
			events |= INTEL_PORT_EVENT_AUX;
			pr_debug("ADF: Aux interrupt\n");
			found = true;
		}

		/* HDCP Audio */
		val = PORT_AUDIO_HDCP_REQUEST(port);
		if (hp_status & val) {
			events |= INTEL_PORT_EVENT_AUDIO;
			pr_debug("ADF: Audio HDCP interrupt\n");
			found = true;
		}

		/*
		 * Clear hotplug interrupts, only if found one on this port
		 * This can belong to some other pipe coming next
		 */
		if (found) {
			REG_WRITE(PORT_HOTPLUG_STAT, hp_status);
			REG_POSTING_READ(PORT_HOTPLUG_STAT);
			pr_info("ADF: %s: hp_stat=0x%x\n", __func__,
				REG_POSTING_READ(PORT_HOTPLUG_STAT));
		}
	}
	return events;
}

u32 intel_adf_get_pipe_events(struct intel_pipe *pipe)
{
	u8 pipe_id = pipe->base.idx;
	u32 value = 0;
	u32 events = 0;
	u32 pipestat = REG_READ(PIPESTAT(pipe_id));

	pr_debug("ADF: %s: PIPESTAT = 0x%x\n", __func__, pipestat);

	/* FIFO under run */
	if (pipestat & FIFO_UNDERRUN_STAT) {
		events |= INTEL_PIPE_EVENT_UNDERRUN;
		value |= FIFO_UNDERRUN_STAT;
	}

	/* Sprite B Flip done interrupt */
	if (pipestat & SPRITE2_FLIP_DONE_STAT) {
		events |= INTEL_PIPE_EVENT_SPRITE2_FLIP;
		value |= SPRITE2_FLIP_DONE_STAT;

		/* program the pre-calculated ddl value */
		if (pipe->regs.sp2_ddl) {
			REG_WRITE_BITS(VLV_DDL(pipe_id), pipe->regs.sp2_ddl,
					pipe->regs.sp2_ddl_mask);
			pipe->regs.sp2_ddl = 0;
		}
	}

	/* Sprite A Flip done interrupt */
	if (pipestat & SPRITE1_FLIP_DONE_STAT) {
		events |= INTEL_PIPE_EVENT_SPRITE1_FLIP;
		value |= SPRITE2_FLIP_DONE_STAT;

		/* program the pre-calculated ddl value */
		if (pipe->regs.sp1_ddl) {
			REG_WRITE_BITS(VLV_DDL(pipe_id), pipe->regs.sp1_ddl,
					pipe->regs.sp1_ddl_mask);
			pipe->regs.sp1_ddl = 0;
		}
	}

	/* Plane A Flip done interrupt */
	if (pipestat & PLANE_FLIP_DONE_STAT) {
		events |= INTEL_PIPE_EVENT_PRIMARY_FLIP;
		value |= PLANE_FLIP_DONE_STAT;

		/* program the pre-calculated ddl value */
		if (pipe->regs.pri_ddl) {
			REG_WRITE_BITS(VLV_DDL(pipe_id), pipe->regs.pri_ddl,
					pipe->regs.pri_ddl_mask);
			pipe->regs.pri_ddl = 0;
		}
	}

	/* Vsync interrupt */
	if (pipestat & VSYNC_STAT) {
		events |= INTEL_PIPE_EVENT_VSYNC;
		value |= VSYNC_STAT;
	}

	/* DPST event */
	if (pipestat & DPST_EVENT_STAT) {
		events |= INTEL_PIPE_EVENT_DPST;
		value |= DPST_EVENT_STAT;
	}

	/* Clear the 1st level interrupt. */
	REG_WRITE(PIPESTAT(pipe_id), pipestat | value);

	return events;
}

int intel_adf_get_events(struct intel_pipe *pipe, u32 *events)
{
	if (!pipe || !events) {
		pr_debug("ADF: %s Null input\n", __func__);
		return -EINVAL;
	}

	*events = 0;

	/* Check PIPE interrupts */
	*events |= intel_adf_get_pipe_events(pipe);

	/* Fixme: This should go to handle events */
	if (*events & INTEL_PIPE_EVENT_VSYNC) {
		if (++pipe->vsync_counter > VSYNC_COUNT_MAX_MASK)
			pipe->vsync_counter = 0;
	}

	/* Check PORT interrupts */
	*events |= intel_adf_get_port_events(pipe);

	pr_debug("ADF: %s\n", __func__);
	return 0;
}

u32 intel_adf_get_supported_events(struct intel_pipe *pipe)
{
	pr_debug("ADF: %s\n", __func__);

	/* Todo: Call ecoder's get event for any special request */
	return INTEL_PIPE_EVENT_VSYNC |
			INTEL_PORT_EVENT_HOTPLUG_DISPLAY |
				INTEL_PORT_EVENT_AUX |
					INTEL_PORT_EVENT_AUDIO;
}

static void intel_adf_hpd_reset(void)
{
	REG_WRITE(PORT_HOTPLUG_EN, 0);
	REG_WRITE(PORT_HOTPLUG_STAT, REG_READ(PORT_HOTPLUG_STAT));
}

void intel_adf_hpd_init(struct intel_adf_context *ctx)
{
	u8 count = 0;
	u32 hotplug_en = REG_READ(PORT_HOTPLUG_EN);
	struct intel_adf_interface *intf;
	struct intel_pipe *intel_pipe;
	enum port port;

	/* Clear old values */
	intel_adf_hpd_reset();

	/*
	 * Enable a hot plug interrupt only if an encoder has
	 * requested for it.
	 */
	while (count < ctx->n_intfs) {
		intf = &ctx->intfs[count++];
		intel_pipe = intf->pipe;
		if (intel_pipe->hp_reqd) {
			port = vlv_get_connected_port(intel_pipe);
			if (port < PORT_A || port > PORT_D)
				pr_err("ADF: %s Skipping invalid port on pipe(%d)\n",
					__func__, intel_pipe->base.idx);
			else {
				/*
				 * Extract the port on which encoder is
				 * connected and enable hot plug only on
				 * that port
				 */
				hotplug_en |= PORT_HOTPLUG_INT_EN(port);
				pr_info("ADF: %s: Selecting port %d for hotplug\n",
					__func__, port);
			}
		}
	}

	if (hotplug_en) {
		hotplug_en |= CRT_HOTPLUG_ACTIVATION_PERIOD_64;
		hotplug_en &= ~CRT_HOTPLUG_VOLTAGE_COMPARE_MASK;
		hotplug_en |= CRT_HOTPLUG_VOLTAGE_COMPARE_50;
		REG_WRITE(PORT_HOTPLUG_EN, hotplug_en);
	}
}
