/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/

#include "adf_hdmi_audio_if.h"
#include "hdmi_pipe.h"

/*
 * Audio register range 0x69000 to 0x69114
 */
#define IS_HDMI_AUDIO_REG(reg) ((reg >= AUD_CONFIG) && \
				(reg <= AUD_HDMIW_INFOFR))

static struct hdmi_pipe  *pipe;

#if 0
static void hdmi_suspend_work(struct work_struct *work)
{
	struct android_hdmi_priv *hdmi_priv =
		container_of(work, struct android_hdmi_priv, suspend_wq);
	struct drm_device *dev = hdmi_priv->dev;

	android_hdmi_suspend_display(dev);
}

/*
 * return whether HDMI audio device is busy.
*/
bool mid_hdmi_audio_is_busy(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	int hdmi_audio_busy = 0;
	hdmi_audio_event_t hdmi_audio_event;

	if (hdmi_state == 0) {
		/* HDMI is not connected, assuming audio device is idle. */
		return false;
	}

	if (dev_priv->had_interface) {
		hdmi_audio_event.type = HAD_EVENT_QUERY_IS_AUDIO_BUSY;
		hdmi_audio_busy = dev_priv->had_interface->query(
			dev_priv->had_pvt_data,
			hdmi_audio_event);
		return hdmi_audio_busy != 0;
	}
	return false;
}

/*
 * return whether HDMI audio device is suspended.
*/
bool mid_hdmi_audio_suspend(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	hdmi_audio_event_t hdmi_audio_event;
	int ret = 0;

	PSB_DEBUG_ENTRY("%s: hdmi_state %d", __func__, hdmi_state);
	if (hdmi_state == 0) {
		/* HDMI is not connected,
		*assuming audio device is suspended already.
		*/
		return true;
	}

	if (dev_priv->had_interface) {
		hdmi_audio_event.type = 0;
		ret = dev_priv->had_interface->suspend(
						dev_priv->had_pvt_data,
						hdmi_audio_event);
		return (ret == 0) ? true : false;
	}
	return true;
}

void mid_hdmi_audio_resume(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	PSB_DEBUG_ENTRY("%s: hdmi_state %d", __func__, hdmi_state);
	if (hdmi_state == 0) {
		/* HDMI is not connected,
		*  there is no need to resume audio device.
		*/
		return;
	}

	if (dev_priv->had_interface)
		dev_priv->had_interface->resume(dev_priv->had_pvt_data);
}
#endif

static void adf_audio_hotplug_work(struct work_struct *work)
{
	pr_debug("%s\n", __func__);
	if (atomic_read(&pipe->hpd_ctx.is_connected))
		adf_hdmi_audio_signal_event(HAD_EVENT_HOT_PLUG);
}

static void adf_audio_bufferdone_work(struct work_struct *work)
{
	pr_debug("%s\n", __func__);
	if (atomic_read(&pipe->hpd_ctx.is_connected))
		adf_hdmi_audio_signal_event(HAD_EVENT_AUDIO_BUFFER_DONE);
}

static void adf_audio_underrun_work(struct work_struct *work)
{
	pr_debug("%s\n", __func__);
	if (atomic_read(&pipe->hpd_ctx.is_connected))
		adf_hdmi_audio_signal_event(HAD_EVENT_AUDIO_BUFFER_UNDERRUN);
}

void adf_hdmi_audio_signal_event(
						enum had_event_type event)
{
	pr_debug("%s: event type= %d\n", __func__, event);
	if (pipe->audio.callbacks)
		(pipe->audio.callbacks)
				(event, pipe->audio.had_pvt_data);
}

void adf_hdmi_audio_init(struct hdmi_pipe *hdmi_pipe)
{
	pipe = hdmi_pipe;
	INIT_WORK(&pipe->audio.hdmi_audio_work, adf_audio_hotplug_work);
	INIT_WORK(&pipe->audio.hdmi_bufferdone_work, adf_audio_bufferdone_work);
	INIT_WORK(&pipe->audio.hdmi_underrun_work, adf_audio_underrun_work);
}

/**
 * mid_hdmi_audio_get_caps:
 * used to return the HDMI audio capabilities.
 * e.g. resolution, frame rate.
 */
static int adf_hdmi_audio_get_caps(enum had_caps_list get_element,
		void *capabilities)
{

	switch (get_element) {
	case HAD_GET_ELD:
		pr_debug("%s: HAD_GET_ELD\n", __func__);
		memcpy(capabilities, &pipe->monitor.eld[7], sizeof(uint8_t));
		break;
	case HAD_GET_SAMPLING_FREQ:
	{
		pr_debug("%s: HAD_GET_SAMPLING_FREQ\n", __func__);
		memcpy(capabilities, &pipe->audio.tmds_clock, sizeof(uint32_t));
		break;
	}
	default:
		break;
	}

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

	switch (set_element) {
	case HAD_SET_ENABLE_AUDIO:
		pr_debug("%s: HAD_SET_ENABLE_AUDIO\n", __func__);
		break;
	case HAD_SET_DISABLE_AUDIO:
		pr_debug("%s: HAD_SET_DISABLE_AUDIO\n", __func__);
		break;
	case HAD_SET_ENABLE_AUDIO_INT:
		pr_debug("%s: HAD_SET_ENABLE_AUDIO_INT\n", __func__);
		break;
	case HAD_SET_DISABLE_AUDIO_INT:
		pr_debug("%s: HAD_SET_DISABLE_AUDIO_INT\n", __func__);
	default:
		break;
	}

	return ret;
}

/**
 * mid_hdmi_audio_write:
 * used to write into display controller HDMI audio registers.
 *
 */
static int adf_hdmi_audio_write(uint32_t reg, uint32_t val)
{
	pr_debug("%s: reg=%x, val=%x\n", __func__, reg, val);
	if (IS_HDMI_AUDIO_REG(reg)) {
		REG_WRITE(reg, val);
		return 0;
	} else
		return -EINVAL;
}

/**
 * mid_hdmi_audio_read:
 * used to get the register value read
 * from display controller HDMI audio registers.
 *
 */
static int adf_hdmi_audio_read(uint32_t reg, uint32_t *val)
{
	pr_debug("%s: reg=%x\n", __func__, reg);
	if (IS_HDMI_AUDIO_REG(reg)) {
		*val = REG_READ(reg);
		return 0;
	} else
		return -EINVAL;
}

/**
 * mid_hdmi_audio_rmw:
 * used to update the masked bits in display
 * controller HDMI audio registers .
 *
 */
static int adf_hdmi_audio_rmw(uint32_t reg,
				uint32_t val, uint32_t mask)
{
	u32 val_tmp;
	pr_debug("%s: reg=%x, val=%x\n", __func__, reg, val);
	if (IS_HDMI_AUDIO_REG(reg)) {
		val_tmp = (val & mask) | (REG_READ(reg) & ~mask);
		REG_WRITE(reg, val_tmp);
		return 0;
	} else
		return -EINVAL;
}

static struct  hdmi_audio_registers_ops hdmi_audio_reg_ops = {
	.hdmi_audio_read_register = adf_hdmi_audio_read,
	.hdmi_audio_write_register = adf_hdmi_audio_write,
	.hdmi_audio_read_modify = adf_hdmi_audio_rmw,
};

static struct hdmi_audio_query_set_ops hdmi_audio_get_set_ops = {
	.hdmi_audio_get_caps = adf_hdmi_audio_get_caps,
	.hdmi_audio_set_caps = adf_hdmi_audio_set_caps,
};

int adf_hdmi_audio_setup(
	had_event_call_back audio_callbacks,
	struct hdmi_audio_registers_ops *reg_ops,
	struct hdmi_audio_query_set_ops *query_ops)
{
	int ret = 0;
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

	pipe->audio.callbacks = audio_callbacks;
	return ret;
}
EXPORT_SYMBOL(adf_hdmi_audio_setup);

int adf_hdmi_audio_register(struct snd_intel_had_interface *driver,
							void *had_data)
{
	pr_debug("%s\n", __func__);
	pipe->audio.had_pvt_data = had_data;
	pipe->audio.had_interface = driver;

	if (pipe->monitor.is_hdmi == false)
		return 0;

	/* The Audio driver is loading now and we need to notify
	 * it if there is an HDMI device attached */
	schedule_work(&pipe->audio.hdmi_audio_work);
	return 0;
}
EXPORT_SYMBOL(adf_hdmi_audio_register);
