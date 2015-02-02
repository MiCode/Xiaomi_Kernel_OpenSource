/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * Author: Akashdeep Sharma <akashdeep.sharma@intel.com>
 * Author: Shashank Sharma <shashank.sharma@intel.com>
 */

#include <drm/drmP.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/chv_dc_regs.h>
#include <core/vlv/vlv_hdmi_port.h>
#include <core/vlv/vlv_dc_config.h>

u32 vlv_hdmi_port_enable(struct vlv_hdmi_port *port)
{
	u32 temp;
	u32 enable_bits = SDVO_ENABLE | SDVO_AUDIO_ENABLE;

	pr_info("ADF: HDMI: %s\n", __func__);

	temp = REG_READ(port->control_reg);
	temp |= enable_bits;

	REG_WRITE(port->control_reg, temp);
	REG_POSTING_READ(port->control_reg);

	return 0;
}

u32 vlv_hdmi_port_prepare(struct vlv_hdmi_port *port, u32 val)
{
	u32 temp = REG_READ(port->control_reg);
	temp |= val;

	pr_info("ADF: HDMI: %s\n", __func__);
	REG_WRITE(port->control_reg, temp);
	REG_POSTING_READ(port->control_reg);

	return 0;
}

u32 vlv_hdmi_port_disable(struct vlv_hdmi_port *port)
{
	u32 temp;
	u32 enable_bits = SDVO_ENABLE | SDVO_AUDIO_ENABLE;

	pr_info("ADF: HDMI: %s\n", __func__);

	temp = REG_READ(port->control_reg);
	temp &= ~enable_bits;

	REG_WRITE(port->control_reg, temp);
	REG_POSTING_READ(port->control_reg);

	return 0;
}

void vlv_hdmi_port_enable_audio(struct vlv_hdmi_port *port)
{
	u32 temp;
	pr_info("ADF: HDMI: %s\n", __func__);

	temp = REG_READ(port->control_reg);
	if (temp & SDVO_ENABLE)
		temp |= SDVO_AUDIO_ENABLE;

	REG_WRITE(port->control_reg, temp);
	REG_READ(port->control_reg);
}

void vlv_hdmi_port_disable_audio(struct vlv_hdmi_port *port)
{
	u32 temp;
	pr_info("ADF: HDMI: %s\n", __func__);

	temp = REG_READ(port->control_reg) & ~SDVO_AUDIO_ENABLE;
	REG_WRITE(port->control_reg, temp);
	REG_READ(port->control_reg);
}

u32 vlv_hdmi_port_write_dip(struct vlv_hdmi_port *port, bool enable)
{
	u32 val = REG_READ(port->dip_ctrl);
	u32 dip_port = VIDEO_DIP_PORT(port->port_id);

	pr_info("ADF: HDMI: %s\n", __func__);

	val |= VIDEO_DIP_SELECT_AVI | VIDEO_DIP_FREQ_VSYNC;

	if (!enable) {
		if (!(val & VIDEO_DIP_ENABLE))
			return 0;
		val &= ~VIDEO_DIP_ENABLE;
		REG_WRITE(port->dip_ctrl, val);
		REG_POSTING_READ(port->dip_ctrl);
		return 0;
	}

	if (dip_port != (val & VIDEO_DIP_PORT_MASK)) {
		if (val & VIDEO_DIP_ENABLE) {
			val &= ~VIDEO_DIP_ENABLE;
			REG_WRITE(port->dip_ctrl, val);
			REG_POSTING_READ(port->dip_ctrl);
		}
		val &= ~VIDEO_DIP_PORT_MASK;
		val |= dip_port;
	}

	val |= VIDEO_DIP_ENABLE;
	val |= VIDEO_DIP_ENABLE_AVI | VIDEO_DIP_ENABLE_SPD;
	val &= ~(VIDEO_DIP_ENABLE_VENDOR |
		VIDEO_DIP_ENABLE_GAMUT | VIDEO_DIP_ENABLE_GCP);

	REG_WRITE(port->dip_ctrl, val);
	REG_POSTING_READ(port->dip_ctrl);

	return 0;
}

bool vlv_hdmi_port_init(struct vlv_hdmi_port *port, enum port enum_port,
		enum pipe pipe)
{
	pr_info("ADF: HDMI: %s\n", __func__);

	switch (enum_port) {
	case PORT_D:
		port->control_reg = CHV_PORTD_CTRL;
		port->adapter = intel_adf_get_gmbus_adapter(GMBUS_PORT_DPD_CHV);
		break;

	case PORT_C:
		port->control_reg = CHV_PORTC_CTRL;
		port->adapter = intel_adf_get_gmbus_adapter(GMBUS_PORT_DPC);
		break;

	case PORT_B:
		port->control_reg = CHV_PORTB_CTRL;
		port->adapter = intel_adf_get_gmbus_adapter(GMBUS_PORT_DPB);
		break;

	default:
		pr_err("ADF: HDMI: %s: Invalid port\n", __func__);
		return false;
	}

	port->dip_stat = VLV_AUD_CNTL_ST(pipe);
	port->dip_ctrl = VLV_TVIDEO_DIP_CTL(pipe);
	port->dip_data = VLV_TVIDEO_DIP_DATA(pipe);
	port->hpd_detect = CHV_HPD_STAT;
	port->hpd_ctrl = CHV_HPD_CTRL;
	port->port_id = enum_port;
	return true;
}

bool vlv_hdmi_port_destroy(struct vlv_hdmi_port *port)
{
	pr_info("ADF: HDMI: %s\n", __func__);
	return true;
}
