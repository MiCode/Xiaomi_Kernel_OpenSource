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
 * Create on 21 Jan 2015
 * Author: Akashdeep Sharma <akashdeep.sharma@intel.com>
 */

#include <core/common/hdmi/gen_hdmi_audio.h>
#include <core/common/hdmi/gen_hdmi_pipe.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/chv_dc_regs.h>
#include <intel_adf.h>


/* Audio register range 0x65000 to 0x65FFF */
#define IS_HDMI_AUDIO_REG(reg) ((reg >= 0x65000) && (reg < 0x65FFF))

static struct hdmi_audio_priv hdmi_priv;
static struct hdmi_pipe *pipe;

void adf_hdmi_audio_signal_event(enum had_event_type event)
{
	pr_debug("ADF: HDMI:%s: event type= %d\n", __func__, event);
	if (hdmi_priv.callbacks) {
		(hdmi_priv.callbacks)
				(event, hdmi_priv.had_pvt_data);
	}
}

void adf_hdmi_audio_init(struct hdmi_pipe *hdmi_pipe)
{
	pr_debug("ADF: HDMI:%s\n", __func__);
	pipe = hdmi_pipe;
}

/**
 * mid_hdmi_audio_get_caps:
 * used to return the HDMI audio capabilities.
 * e.g. resolution, frame rate.
 */
static int adf_hdmi_audio_get_caps(enum had_caps_list get_element,
		void *capabilities)
{
	pr_debug("ADF: HDMI:%s\n", __func__);

	switch (get_element) {
	case HAD_GET_ELD:
		memcpy(capabilities, pipe->config.ctx.monitor->eld,
						HDMI_MAX_ELD_LENGTH);
		break;
	case HAD_GET_SAMPLING_FREQ:
		memcpy(capabilities, &(pipe->tmds_clock), sizeof(uint32_t));
		break;
	default:
		break;
	}

	return 0;
}

/**
 * hdmi_audio_get_register_base
 * used to get the current hdmi base address
 */
int adf_audio_get_register_base(uint32_t *reg_base)
{
	/*
	 * HDMI audio LPE register hardcoded to pipe C for CHV.
	 * Audio driver needs to know which pipe to route audio to
	 */
	u32 reg_address = ADF_HDMI_AUDIO_LPE_C_CONFIG;
	pr_debug("ADF: HDMI:%s\n", __func__);

	*reg_base = reg_address;
	return 0;
}

/**
 * mid_hdmi_audio_set_caps:
 * used to set the HDMI audio capabilities.
 * e.g. Audio INT.
 */
static int adf_hdmi_audio_set_caps(enum had_caps_list set_element,
		void *capabilties)
{
	int ret = 0;
	struct intel_pipeline *pipeline = pipe->base.pipeline;
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct vlv_hdmi_port *hdmi_port;
	hdmi_port = &disp->port.hdmi_port;

	pr_debug("ADF: HDMI:%s\n", __func__);

	switch (set_element) {
	case HAD_SET_ENABLE_AUDIO:
		vlv_hdmi_port_enable_audio(hdmi_port);
		break;
	case HAD_SET_DISABLE_AUDIO:
		vlv_hdmi_port_disable_audio(hdmi_port);
		break;
	case HAD_SET_ENABLE_AUDIO_INT:
		break;
	case HAD_SET_DISABLE_AUDIO_INT:
		break;
	default:
		break;
	}

	return ret;
}

/**
 * mid_hdmi_audio_write:
 * used to write into display controller HDMI audio registers.
 */
static int adf_hdmi_audio_write(uint32_t reg, uint32_t val)
{
	int ret = 0;
	pr_debug("ADF: HDMI:%s: reg=%x, val=%x\n", __func__, reg, val);

	if (IS_HDMI_AUDIO_REG(reg))
		REG_WRITE((CHV_DISPLAY_BASE + reg), val);
	else
		return -EINVAL;

	return ret;
}

/**
 * mid_hdmi_audio_read:
 * used to get the register value read
 * from display controller HDMI audio registers.
 */
static int adf_hdmi_audio_read(uint32_t reg, uint32_t *val)
{
	int ret = 0;
	pr_debug("ADF: HDMI:%s: reg=%x\n", __func__, reg);

	if (IS_HDMI_AUDIO_REG(reg))
		*val = REG_READ(CHV_DISPLAY_BASE + reg);
	else
		return -EINVAL;

	return ret;
}

/**
 * mid_hdmi_audio_rmw:
 * used to update the masked bits in display
 * controller HDMI audio registers .
 */
static int adf_hdmi_audio_rmw(uint32_t reg,
				uint32_t val, uint32_t mask)
{
	int ret = 0;
	u32 val_tmp;
	pr_debug("ADF: HDMI:%s: reg=%x, val=%x\n", __func__, reg, val);

	if (IS_HDMI_AUDIO_REG(reg)) {
		val_tmp = (val & mask) |
			(REG_READ(CHV_DISPLAY_BASE + reg) & ~mask);
		REG_WRITE((CHV_DISPLAY_BASE + reg), val_tmp);
	} else {
		return -EINVAL;
	}

	return ret;
}

static struct  hdmi_audio_registers_ops hdmi_audio_reg_ops = {
	.hdmi_audio_get_register_base = adf_audio_get_register_base,
	.hdmi_audio_read_register = adf_hdmi_audio_read,
	.hdmi_audio_write_register = adf_hdmi_audio_write,
	.hdmi_audio_read_modify = adf_hdmi_audio_rmw,
};

static struct hdmi_audio_query_set_ops hdmi_audio_get_set_ops = {
	.hdmi_audio_get_caps = adf_hdmi_audio_get_caps,
	.hdmi_audio_set_caps = adf_hdmi_audio_set_caps,
};

int adf_hdmi_audio_setup(
	void *callbacks,
	void *r_ops, void *q_ops)
{
	had_event_call_back audio_callbacks = callbacks;
	struct hdmi_audio_registers_ops *reg_ops = r_ops;
	struct hdmi_audio_query_set_ops *query_ops = q_ops;

	int ret = 0;

	pr_debug("ADF: HDMI:%s\n", __func__);

	reg_ops->hdmi_audio_get_register_base =
			(hdmi_audio_reg_ops.hdmi_audio_get_register_base);
	reg_ops->hdmi_audio_read_register =
			(hdmi_audio_reg_ops.hdmi_audio_read_register);
	reg_ops->hdmi_audio_write_register =
			(hdmi_audio_reg_ops.hdmi_audio_write_register);
	reg_ops->hdmi_audio_read_modify =
			(hdmi_audio_reg_ops.hdmi_audio_read_modify);
	query_ops->hdmi_audio_get_caps =
			hdmi_audio_get_set_ops.hdmi_audio_get_caps;
	query_ops->hdmi_audio_set_caps =
			hdmi_audio_get_set_ops.hdmi_audio_set_caps;

	hdmi_priv.callbacks = audio_callbacks;
	return ret;
}
EXPORT_SYMBOL(adf_hdmi_audio_setup);

int adf_hdmi_audio_register(
	void *drv,
	void *had_data)
{
	struct snd_intel_had_interface *driver = drv;

	pr_debug("ADF: HDMI:%s\n", __func__);

	hdmi_priv.had_pvt_data = had_data;
	hdmi_priv.had_interface = driver;

	if (pipe->config.ctx.monitor)
		if (pipe->config.ctx.monitor->is_hdmi == false)
			return 0;

	/*
	 * The Audio driver is loading now and we need to notify
	 * it if there is an HDMI device attached
	 */
	if (atomic_read(&pipe->config.ctx.connected))
		adf_hdmi_audio_signal_event(HAD_EVENT_HOT_PLUG);

	return 0;
}
EXPORT_SYMBOL(adf_hdmi_audio_register);
