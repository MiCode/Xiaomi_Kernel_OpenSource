/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/iopoll.h>
#include <linux/types.h>
#include <linux/switch.h>
#include <linux/gcd.h>

#include "drm_edid.h"
#include "sde_kms.h"
#include "sde_hdmi.h"
#include "sde_hdmi_regs.h"
#include "hdmi.h"

static int sde_hdmi_ddc_read_retry(struct sde_hdmi *display)
{
	int status;
	int busy_wait_us;
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;
	struct sde_hdmi_tx_ddc_data *ddc_data;
	struct hdmi *hdmi;

	if (!display) {
		SDE_ERROR("invalid input\n");
		return -EINVAL;
	}

	hdmi = display->ctrl.ctrl;
	ddc_ctrl = &display->ddc_ctrl;
	ddc_data = &ddc_ctrl->ddc_data;

	if (!ddc_data) {
		SDE_ERROR("invalid input\n");
		return -EINVAL;
	}

	if (!ddc_data->data_buf) {
		status = -EINVAL;
		SDE_ERROR("%s: invalid buf\n", ddc_data->what);
		goto error;
	}

	if (ddc_data->retry < 0) {
		SDE_ERROR("invalid no. of retries %d\n", ddc_data->retry);
		status = -EINVAL;
		goto error;
	}

	do {
		if (ddc_data->hard_timeout) {
			HDMI_UTIL_DEBUG("using hard_timeout %dms\n",
					 ddc_data->hard_timeout);

			busy_wait_us = ddc_data->hard_timeout * HDMI_MS_TO_US;
			hdmi->use_hard_timeout = true;
			hdmi->busy_wait_us = busy_wait_us;
		}

		/* Calling upstream ddc read method */
		status = hdmi_ddc_read(hdmi, ddc_data->dev_addr,
			ddc_data->offset,
			ddc_data->data_buf, ddc_data->request_len,
			false);

		if (ddc_data->hard_timeout)
			ddc_data->timeout_left = hdmi->timeout_count;


		if (ddc_data->hard_timeout && !hdmi->timeout_count) {
			HDMI_UTIL_DEBUG("%s: timedout\n", ddc_data->what);
			status = -ETIMEDOUT;
		}

	} while (status && ddc_data->retry--);

	if (status) {
		HDMI_UTIL_ERROR("%s: failed status = %d\n",
						ddc_data->what, status);
		goto error;
	}

	HDMI_UTIL_DEBUG("%s: success\n",  ddc_data->what);

error:
	return status;
} /* sde_hdmi_ddc_read_retry */

int sde_hdmi_ddc_read(void *cb_data)
{
	int rc = 0;
	int retry;
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;
	struct sde_hdmi_tx_ddc_data *ddc_data;
	struct sde_hdmi *display = (struct sde_hdmi *)cb_data;

	if (!display) {
		SDE_ERROR("invalid ddc ctrl\n");
		return -EINVAL;
	}

	ddc_ctrl = &display->ddc_ctrl;
	ddc_data = &ddc_ctrl->ddc_data;
	retry = ddc_data->retry;

	rc = sde_hdmi_ddc_read_retry(display);
	if (!rc)
		return rc;

	if (ddc_data->retry_align) {
		ddc_data->retry = retry;

		ddc_data->request_len = 32 * ((ddc_data->data_len + 31) / 32);
		rc = sde_hdmi_ddc_read_retry(display);
	}

	return rc;
} /* hdmi_ddc_read */

int sde_hdmi_ddc_write(void *cb_data)
{
	int status;
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;
	struct sde_hdmi_tx_ddc_data *ddc_data;
	int busy_wait_us;
	struct hdmi *hdmi;
	struct sde_hdmi *display = (struct sde_hdmi *)cb_data;

	if (!display) {
		SDE_ERROR("invalid input\n");
		return -EINVAL;
	}

	hdmi = display->ctrl.ctrl;
	ddc_ctrl = &display->ddc_ctrl;

	ddc_data = &ddc_ctrl->ddc_data;

	if (!ddc_data) {
		SDE_ERROR("invalid input\n");
		return -EINVAL;
	}

	if (!ddc_data->data_buf) {
		status = -EINVAL;
		SDE_ERROR("%s: invalid buf\n", ddc_data->what);
		goto error;
	}

	if (ddc_data->retry < 0) {
		SDE_ERROR("invalid no. of retries %d\n", ddc_data->retry);
		status = -EINVAL;
		goto error;
	}

	do {
		if (ddc_data->hard_timeout) {
			busy_wait_us = ddc_data->hard_timeout * HDMI_MS_TO_US;
			hdmi->use_hard_timeout = true;
			hdmi->busy_wait_us = busy_wait_us;
		}

		status = hdmi_ddc_write(hdmi,
			ddc_data->dev_addr, ddc_data->offset,
			ddc_data->data_buf, ddc_data->data_len,
			false);

		if (ddc_data->hard_timeout)
			ddc_data->timeout_left = hdmi->timeout_count;

		if (ddc_data->hard_timeout && !hdmi->timeout_count) {
			HDMI_UTIL_ERROR("%s timout\n",  ddc_data->what);
			status = -ETIMEDOUT;
		}

	} while (status && ddc_data->retry--);

	if (status) {
		HDMI_UTIL_ERROR("%s: failed status = %d\n",
						ddc_data->what, status);
		goto error;
	}

	HDMI_UTIL_DEBUG("%s: success\n", ddc_data->what);
error:
	return status;
} /* hdmi_ddc_write */

bool sde_hdmi_tx_is_hdcp_enabled(struct sde_hdmi *hdmi_ctrl)
{
	if (!hdmi_ctrl) {
		SDE_ERROR("%s: invalid input\n", __func__);
		return false;
	}

	return (hdmi_ctrl->hdcp14_present || hdmi_ctrl->hdcp22_present) &&
		hdmi_ctrl->hdcp_ops;
}

bool sde_hdmi_tx_is_encryption_set(struct sde_hdmi *hdmi_ctrl)
{
	bool enc_en = true;
	u32 reg_val;
	struct hdmi *hdmi;

	if (!hdmi_ctrl) {
		SDE_ERROR("%s: invalid input\n", __func__);
		goto end;
	}

	hdmi = hdmi_ctrl->ctrl.ctrl;

	reg_val = hdmi_read(hdmi, HDMI_HDCP_CTRL2);
	if ((reg_val & BIT(0)) && (reg_val & BIT(1)))
		goto end;

	if (hdmi_read(hdmi, HDMI_CTRL) & BIT(2))
		goto end;

	return false;

end:
	return enc_en;
} /* sde_hdmi_tx_is_encryption_set */

bool sde_hdmi_tx_is_stream_shareable(struct sde_hdmi *hdmi_ctrl)
{
	bool ret;

	if (!hdmi_ctrl) {
		SDE_ERROR("%s: invalid input\n", __func__);
		return false;
	}

	switch (hdmi_ctrl->enc_lvl) {
	case HDCP_STATE_AUTH_ENC_NONE:
		ret = true;
		break;
	case HDCP_STATE_AUTH_ENC_1X:
		ret = sde_hdmi_tx_is_hdcp_enabled(hdmi_ctrl) &&
				hdmi_ctrl->auth_state;
		break;
	case HDCP_STATE_AUTH_ENC_2P2:
		ret = hdmi_ctrl->hdcp22_present &&
			hdmi_ctrl->auth_state;
		break;
	default:
		ret = false;
	}

	return ret;
}

bool sde_hdmi_tx_is_panel_on(struct sde_hdmi *hdmi_ctrl)
{
	struct hdmi *hdmi;

	if (!hdmi_ctrl) {
		SDE_ERROR("%s: invalid input\n", __func__);
		return false;
	}

	hdmi = hdmi_ctrl->ctrl.ctrl;

	return hdmi_ctrl->connected && hdmi->power_on;
}

int sde_hdmi_config_avmute(struct hdmi *hdmi, bool set)
{
	u32 av_mute_status;
	bool av_pkt_en = false;

	if (!hdmi) {
		SDE_ERROR("invalid HDMI Ctrl\n");
		return -ENODEV;
	}

	av_mute_status = hdmi_read(hdmi, HDMI_GC);

	if (set) {
		if (!(av_mute_status & BIT(0))) {
			hdmi_write(hdmi, HDMI_GC, av_mute_status | BIT(0));
			av_pkt_en = true;
		}
	} else {
		if (av_mute_status & BIT(0)) {
			hdmi_write(hdmi, HDMI_GC, av_mute_status & ~BIT(0));
			av_pkt_en = true;
		}
	}

	/* Enable AV Mute tranmission here */
	if (av_pkt_en)
		hdmi_write(hdmi, HDMI_VBI_PKT_CTRL,
			hdmi_read(hdmi, HDMI_VBI_PKT_CTRL) | (BIT(4) & BIT(5)));

	pr_info("AVMUTE %s\n", set ? "set" : "cleared");

	return 0;
}
