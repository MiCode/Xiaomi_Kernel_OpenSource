/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#define HDMI_SEC_TO_MS 1000
#define HDMI_MS_TO_US 1000
#define HDMI_SEC_TO_US (HDMI_SEC_TO_MS * HDMI_MS_TO_US)
#define HDMI_KHZ_TO_HZ 1000
#define HDMI_BUSY_WAIT_DELAY_US 100

static void sde_hdmi_hdcp2p2_ddc_clear_status(struct sde_hdmi *display)
{
	u32 reg_val;
	struct hdmi *hdmi;

	if (!display) {
		pr_err("invalid ddc ctrl\n");
		return;
	}
	hdmi = display->ctrl.ctrl;
	/* check for errors and clear status */
	reg_val = hdmi_read(hdmi, HDMI_HDCP2P2_DDC_STATUS);

	if (reg_val & BIT(4)) {
		pr_debug("ddc aborted\n");
		reg_val |= BIT(5);
	}

	if (reg_val & BIT(8)) {
		pr_debug("timed out\n");
		reg_val |= BIT(9);
	}

	if (reg_val & BIT(12)) {
		pr_debug("NACK0\n");
		reg_val |= BIT(13);
	}

	if (reg_val & BIT(14)) {
		pr_debug("NACK1\n");
		reg_val |= BIT(15);
	}

	hdmi_write(hdmi, HDMI_HDCP2P2_DDC_STATUS, reg_val);
}

static const char *sde_hdmi_hdr_sname(enum sde_hdmi_hdr_state hdr_state)
{
	switch (hdr_state) {
	case HDR_DISABLE: return "HDR_DISABLE";
	case HDR_ENABLE: return "HDR_ENABLE";
	case HDR_RESET: return "HDR_RESET";
	default: return "HDR_INVALID_STATE";
	}
}

static u8 sde_hdmi_infoframe_checksum(u8 *ptr, size_t size)
{
	u8 csum = 0;
	size_t i;

	/* compute checksum */
	for (i = 0; i < size; i++)
		csum += ptr[i];

	return 256 - csum;
}

u8 sde_hdmi_hdr_set_chksum(struct drm_msm_ext_panel_hdr_metadata *hdr_meta)
{
	u8 *buff;
	u8 *ptr;
	u32 length;
	u32 size;
	u32 chksum = 0;
	u32 const type_code = 0x87;
	u32 const version = 0x01;
	u32 const descriptor_id = 0x00;

	/* length of metadata is 26 bytes */
	length = 0x1a;
	/* add 4 bytes for the header */
	size = length + HDMI_INFOFRAME_HEADER_SIZE;

	buff = kzalloc(size, GFP_KERNEL);

	if (!buff) {
		SDE_ERROR("invalid buff\n");
		goto err_alloc;
	}

	ptr = buff;

	buff[0] = type_code;
	buff[1] = version;
	buff[2] = length;
	buff[3] = 0;
	/* start infoframe payload */
	buff += HDMI_INFOFRAME_HEADER_SIZE;

	buff[0] = hdr_meta->eotf;
	buff[1] = descriptor_id;

	buff[2] = hdr_meta->display_primaries_x[0] & 0xff;
	buff[3] = hdr_meta->display_primaries_x[0] >> 8;

	buff[4] = hdr_meta->display_primaries_x[1] & 0xff;
	buff[5] = hdr_meta->display_primaries_x[1] >> 8;

	buff[6] = hdr_meta->display_primaries_x[2] & 0xff;
	buff[7] = hdr_meta->display_primaries_x[2] >> 8;

	buff[8] = hdr_meta->display_primaries_y[0] & 0xff;
	buff[9] = hdr_meta->display_primaries_y[0] >> 8;

	buff[10] = hdr_meta->display_primaries_y[1] & 0xff;
	buff[11] = hdr_meta->display_primaries_y[1] >> 8;

	buff[12] = hdr_meta->display_primaries_y[2] & 0xff;
	buff[13] = hdr_meta->display_primaries_y[2] >> 8;

	buff[14] = hdr_meta->white_point_x & 0xff;
	buff[15] = hdr_meta->white_point_x >> 8;
	buff[16] = hdr_meta->white_point_y & 0xff;
	buff[17] = hdr_meta->white_point_y >> 8;

	buff[18] = hdr_meta->max_luminance & 0xff;
	buff[19] = hdr_meta->max_luminance >> 8;

	buff[20] = hdr_meta->min_luminance & 0xff;
	buff[21] = hdr_meta->min_luminance >> 8;

	buff[22] = hdr_meta->max_content_light_level & 0xff;
	buff[23] = hdr_meta->max_content_light_level >> 8;

	buff[24] = hdr_meta->max_average_light_level & 0xff;
	buff[25] = hdr_meta->max_average_light_level >> 8;

	chksum = sde_hdmi_infoframe_checksum(ptr, size);

	kfree(ptr);

err_alloc:
	return chksum;
}

/**
 * sde_hdmi_dump_regs - utility to dump HDMI regs
 * @hdmi_display: Pointer to private display handle
 * Return : void
 */

void sde_hdmi_dump_regs(void *hdmi_display)
{
	struct sde_hdmi *display = (struct sde_hdmi *)hdmi_display;
	struct hdmi *hdmi;
	int i;
	u32 addr_off = 0;
	u32 len = 0;

	if (!display) {
		pr_err("invalid input\n");
		return;
	}

	hdmi = display->ctrl.ctrl;

	if (!hdmi) {
		pr_err("invalid input\n");
		return;
	}

	if (!hdmi->power_on || !display->connected) {
		SDE_ERROR("HDMI display is not ready\n");
		return;
	}

	len = hdmi->mmio_len;

	if (len % 16)
		len += 16;
	len /= 16;

	pr_info("HDMI CORE regs\n");
	for (i = 0; i < len; i++) {
		u32 x0, x4, x8, xc;

		x0 = hdmi_read(hdmi, addr_off+0x0);
		x4 = hdmi_read(hdmi, addr_off+0x4);
		x8 = hdmi_read(hdmi, addr_off+0x8);
		xc = hdmi_read(hdmi, addr_off+0xc);

		pr_info("%08x : %08x %08x %08x %08x\n", addr_off, x0, x4, x8,
				xc);

		addr_off += 16;
	}
}

int sde_hdmi_ddc_hdcp2p2_isr(void *hdmi_display)
{
	struct sde_hdmi_tx_hdcp2p2_ddc_data *data;
	u32 intr0, intr2, intr5;
	u32 msg_size;
	int rc = 0;
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;
	struct sde_hdmi *display = (struct sde_hdmi *)hdmi_display;
	struct hdmi *hdmi;

	ddc_ctrl = &display->ddc_ctrl;
	data = &ddc_ctrl->sde_hdcp2p2_ddc_data;
	hdmi = display->ctrl.ctrl;

	if (!hdmi) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	intr0 = hdmi_read(hdmi, HDMI_DDC_INT_CTRL0);
	intr2 = hdmi_read(hdmi, HDMI_HDCP_INT_CTRL2);
	intr5 = hdmi_read(hdmi, HDMI_DDC_INT_CTRL5);

	pr_debug("intr0: 0x%x, intr2: 0x%x, intr5: 0x%x\n",
			 intr0, intr2, intr5);

	/* check if encryption is enabled */
	if (intr2 & BIT(0)) {
		/*
		 * ack encryption ready interrupt.
		 * disable encryption ready interrupt.
		 * enable encryption not ready interrupt.
		 */
		intr2 &= ~BIT(2);
		intr2 |= BIT(1) | BIT(6);

		pr_info("HDCP 2.2 Encryption enabled\n");
		data->encryption_ready = true;
	}

	/* check if encryption is disabled */
	if (intr2 & BIT(4)) {
		/*
		 * ack encryption not ready interrupt.
		 * disable encryption not ready interrupt.
		 * enable encryption ready interrupt.
		 */
		intr2  &= ~BIT(6);
		intr2  |= BIT(5) | BIT(2);

		pr_info("HDCP 2.2 Encryption disabled\n");
		data->encryption_ready = false;
	}

	hdmi_write(hdmi, HDMI_HDCP_INT_CTRL2, intr2);

	/* get the message size bits 29:20 */
	msg_size = (intr0 & (0x3FF << 20)) >> 20;

	if (msg_size) {
		/* ack and disable message size interrupt */
		intr0 |= BIT(30);
		intr0 &= ~BIT(31);

		data->message_size = msg_size;
	}

	/* check and disable ready interrupt */
	if (intr0 & BIT(16)) {
	/* ack ready/not ready interrupt */
		intr0 |= BIT(17);
		intr0 &= ~BIT(18);
		pr_debug("got ready interrupt\n");
		data->ready = true;
	}

	/* check for reauth req interrupt */
	if (intr0 & BIT(12)) {
		/* ack and disable reauth req interrupt */
		intr0 |= BIT(13);
		intr0 &= ~BIT(14);
		pr_err("got reauth interrupt\n");
		data->reauth_req = true;
	}

	/* check for ddc fail interrupt */
	if (intr0 & BIT(8)) {
		/* ack ddc fail interrupt */
		intr0 |= BIT(9);
		pr_err("got ddc fail interrupt\n");
		data->ddc_max_retries_fail = true;
	}

	/* check for ddc done interrupt */
	if (intr0 & BIT(4)) {
		/* ack ddc done interrupt */
		intr0 |= BIT(5);
		pr_debug("got ddc done interrupt\n");
		data->ddc_done = true;
	}

	/* check for ddc read req interrupt */
	if (intr0 & BIT(0)) {
		/* ack read req interrupt */
		intr0 |= BIT(1);

		data->ddc_read_req = true;
	}

	hdmi_write(hdmi, HDMI_DDC_INT_CTRL0, intr0);

	if (intr5 & BIT(0)) {
		pr_err("RXSTATUS_DDC_REQ_TIMEOUT\n");

		/* ack and disable timeout interrupt */
		intr5 |= BIT(1);
		intr5 &= ~BIT(2);

		data->ddc_timeout = true;
	}
	hdmi_write(hdmi, HDMI_DDC_INT_CTRL5, intr5);

	if (data->message_size || data->ready || data->reauth_req) {
		if (data->wait) {
			complete(&ddc_ctrl->rx_status_done);
		} else if (data->link_cb && data->link_data) {
			data->link_cb(data->link_data);
		} else {
			pr_err("new msg/reauth not handled\n");
			rc = -EINVAL;
		}
	}

	sde_hdmi_hdcp2p2_ddc_clear_status(display);

	return rc;
}

int sde_hdmi_ddc_scrambling_isr(void *hdmi_display)
{

	bool scrambler_timer_off = false;
	u32 intr2, intr5;
	struct sde_hdmi *display = (struct sde_hdmi *)hdmi_display;
	struct hdmi *hdmi;


	hdmi = display->ctrl.ctrl;

	if (!hdmi) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	intr2 = hdmi_read(hdmi, HDMI_DDC_INT_CTRL2);
	intr5 = hdmi_read(hdmi, HDMI_DDC_INT_CTRL5);

	pr_debug("intr2: 0x%x, intr5: 0x%x\n", intr2, intr5);

	if (intr2 & BIT(12)) {
		pr_err("SCRAMBLER_STATUS_NOT\n");

		intr2 |= BIT(14);
		scrambler_timer_off = true;
	}

	if (intr2 & BIT(8)) {
		pr_err("SCRAMBLER_STATUS_DDC_FAILED\n");

		intr2 |= BIT(9);

		scrambler_timer_off = true;
	}
	hdmi_write(hdmi, HDMI_DDC_INT_CTRL2, intr2);

	if (intr5 & BIT(8)) {
		pr_err("SCRAMBLER_STATUS_DDC_REQ_TIMEOUT\n");
		intr5 |= BIT(9);
		intr5 &= ~BIT(10);
		scrambler_timer_off = true;
	}
	hdmi_write(hdmi, HDMI_DDC_INT_CTRL5, intr5);

	if (scrambler_timer_off)
		_sde_hdmi_scrambler_ddc_disable((void *)display);

	return 0;
}

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

	/* Check if encryption was enabled */
	if (hdmi_ctrl->hdmi_tx_major_version <= HDMI_TX_VERSION_3) {
		reg_val = hdmi_read(hdmi, HDMI_HDCP_CTRL2);
		if ((reg_val & BIT(0)) && (reg_val & BIT(1)))
			goto end;

		if (hdmi_read(hdmi, HDMI_CTRL) & BIT(2))
			goto end;
	} else {
		reg_val = hdmi_read(hdmi, HDMI_HDCP_STATUS);
		if (reg_val)
			goto end;
	}

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

int _sde_hdmi_get_timeout_in_hysnc(void *hdmi_display, u32 timeout_ms)
{
	struct sde_hdmi *display = (struct sde_hdmi *)hdmi_display;
	struct drm_display_mode mode = display->mode;
	/*
	 * pixel clock  = h_total * v_total * fps
	 * 1 sec = pixel clock number of pixels are transmitted.
	 * time taken by one line (h_total) = 1s / (v_total * fps).
	 * lines for give time = (time_ms * 1000) / (1000000 / (v_total * fps))
	 *                     = (time_ms * clock) / h_total
	 */

	return (timeout_ms * mode.clock / mode.htotal);
}

static void sde_hdmi_hdcp2p2_ddc_reset(struct sde_hdmi *hdmi_ctrl)
{
	u32 reg_val;
	struct hdmi *hdmi = hdmi_ctrl->ctrl.ctrl;

	if (!hdmi) {
		pr_err("Invalid parameters\n");
		return;
	}

	/*
	 * Clear acks for DDC_REQ, DDC_DONE, DDC_FAILED, RXSTATUS_READY,
	 * RXSTATUS_MSG_SIZE
	 */
	reg_val = BIT(30) | BIT(17) | BIT(13) | BIT(9) | BIT(5) | BIT(1);
	hdmi_write(hdmi, HDMI_DDC_INT_CTRL0, reg_val);
	/* Reset DDC timers */
	reg_val = BIT(0) | hdmi_read(hdmi, HDMI_HDCP2P2_DDC_CTRL);
	hdmi_write(hdmi, HDMI_HDCP2P2_DDC_CTRL, reg_val);
	reg_val = hdmi_read(hdmi, HDMI_HDCP2P2_DDC_CTRL);
	reg_val &= ~BIT(0);
	hdmi_write(hdmi, HDMI_HDCP2P2_DDC_CTRL, reg_val);
}

void sde_hdmi_hdcp2p2_ddc_disable(void *hdmi_display)
{
	struct sde_hdmi *display = (struct sde_hdmi *)hdmi_display;
	u32 reg_val;
	struct hdmi *hdmi = display->ctrl.ctrl;

	if (!hdmi) {
		pr_err("Invalid parameters\n");
		return;
	}

	sde_hdmi_hdcp2p2_ddc_reset(display);

	/* Disable HW DDC access to RxStatus register */
	reg_val = hdmi_read(hdmi, HDMI_HW_DDC_CTRL);
	reg_val &= ~(BIT(1) | BIT(0));

	hdmi_write(hdmi, HDMI_HW_DDC_CTRL, reg_val);
}

static void _sde_hdmi_scrambler_ddc_reset(struct hdmi *hdmi)
{
	u32 reg_val;

	/* clear ack and disable interrupts */
	reg_val = BIT(14) | BIT(9) | BIT(5) | BIT(1);
	hdmi_write(hdmi, REG_HDMI_DDC_INT_CTRL2, reg_val);

	/* Reset DDC timers */
	reg_val = BIT(0) | hdmi_read(hdmi, REG_HDMI_SCRAMBLER_STATUS_DDC_CTRL);
	hdmi_write(hdmi, REG_HDMI_SCRAMBLER_STATUS_DDC_CTRL, reg_val);

	reg_val = hdmi_read(hdmi, REG_HDMI_SCRAMBLER_STATUS_DDC_CTRL);
	reg_val &= ~BIT(0);
	hdmi_write(hdmi, REG_HDMI_SCRAMBLER_STATUS_DDC_CTRL, reg_val);
}

void sde_hdmi_ctrl_cfg(struct hdmi *hdmi, bool power_on)
{
	uint32_t ctrl = 0;
	unsigned long flags;

	spin_lock_irqsave(&hdmi->reg_lock, flags);
	ctrl = hdmi_read(hdmi, REG_HDMI_CTRL);

	if (power_on)
		ctrl |= HDMI_CTRL_ENABLE;
	else
		ctrl &= ~HDMI_CTRL_ENABLE;

	hdmi_write(hdmi, REG_HDMI_CTRL, ctrl);
	spin_unlock_irqrestore(&hdmi->reg_lock, flags);

	HDMI_UTIL_DEBUG("HDMI Core: %s, HDMI_CTRL=0x%08x\n",
			power_on ? "Enable" : "Disable", ctrl);
}

static void sde_hdmi_clear_pkt_send(struct hdmi *hdmi)
{
	uint32_t reg_val;

	/* Clear audio sample send */
	reg_val = hdmi_read(hdmi, HDMI_AUDIO_PKT_CTRL);
	reg_val &= ~BIT(0);
	hdmi_write(hdmi, HDMI_AUDIO_PKT_CTRL, reg_val);

	/* Clear sending VBI ctrl packets */
	reg_val = hdmi_read(hdmi, HDMI_VBI_PKT_CTRL);
	reg_val &= ~(BIT(4) | BIT(8) | BIT(12));
	hdmi_write(hdmi, HDMI_VBI_PKT_CTRL, reg_val);

	/* Clear sending infoframe packets */
	reg_val = hdmi_read(hdmi, HDMI_INFOFRAME_CTRL0);
	reg_val &= ~(BIT(0) | BIT(4) | BIT(8) | BIT(12)
				 | BIT(15) | BIT(19));
	hdmi_write(hdmi, HDMI_INFOFRAME_CTRL0, reg_val);

	/* Clear sending general ctrl packets */
	reg_val = hdmi_read(hdmi, HDMI_GEN_PKT_CTRL);
	reg_val &= ~(BIT(0) | BIT(4));
	hdmi_write(hdmi, HDMI_GEN_PKT_CTRL, reg_val);
}

void sde_hdmi_ctrl_reset(struct hdmi *hdmi)
{
	uint32_t reg_val;

	/* Assert HDMI CTRL SW reset */
	reg_val = hdmi_read(hdmi, HDMI_CTRL_SW_RESET);
	reg_val |= BIT(0);
	hdmi_write(hdmi, HDMI_CTRL_SW_RESET, reg_val);

	/* disable the controller and put to known state */
	sde_hdmi_ctrl_cfg(hdmi, 0);

	/* disable the audio engine */
	reg_val = hdmi_read(hdmi, HDMI_AUDIO_CFG);
	reg_val &= ~BIT(0);
	hdmi_write(hdmi, HDMI_AUDIO_CFG, reg_val);

	/* clear sending packets to sink */
	sde_hdmi_clear_pkt_send(hdmi);

	/* De-assert HDMI CTRL SW reset */
	reg_val = hdmi_read(hdmi, HDMI_CTRL_SW_RESET);
	reg_val &= ~BIT(0);
	hdmi_write(hdmi, HDMI_CTRL_SW_RESET, reg_val);
}

void _sde_hdmi_scrambler_ddc_disable(void *hdmi_display)
{
	struct sde_hdmi *display = (struct sde_hdmi *)hdmi_display;
	u32 reg_val;

	struct hdmi *hdmi = display->ctrl.ctrl;

	if (!hdmi) {
		pr_err("Invalid parameters\n");
		return;
	}

	_sde_hdmi_scrambler_ddc_reset(hdmi);
	/* Disable HW DDC access to RxStatus register */
	reg_val = hdmi_read(hdmi, REG_HDMI_HW_DDC_CTRL);
	reg_val &= ~(BIT(8) | BIT(9));
	hdmi_write(hdmi, REG_HDMI_HW_DDC_CTRL, reg_val);
}

void sde_hdmi_ddc_config(void *hdmi_display)
{
	struct sde_hdmi *display = (struct sde_hdmi *)hdmi_display;
	struct hdmi *hdmi = display->ctrl.ctrl;
	uint32_t ddc_speed;

	if (!hdmi) {
		pr_err("Invalid parameters\n");
		return;
	}

	ddc_speed = hdmi_read(hdmi, REG_HDMI_DDC_SPEED);
	ddc_speed |= HDMI_DDC_SPEED_THRESHOLD(2);
	ddc_speed |= HDMI_DDC_SPEED_PRESCALE(12);

	hdmi_write(hdmi, REG_HDMI_DDC_SPEED,
			   ddc_speed);

	hdmi_write(hdmi, REG_HDMI_DDC_SETUP,
			   HDMI_DDC_SETUP_TIMEOUT(0xff));

	/* enable reference timer for 19us */
	hdmi_write(hdmi, REG_HDMI_DDC_REF,
			   HDMI_DDC_REF_REFTIMER_ENABLE |
			   HDMI_DDC_REF_REFTIMER(19));
}

int sde_hdmi_hdcp2p2_read_rxstatus(void *hdmi_display)
{
	u32 reg_val;
	u32 intr_en_mask;
	u32 timeout;
	u32 timer;
	int rc = 0;
	int busy_wait_us;
	struct sde_hdmi_tx_hdcp2p2_ddc_data *data;
	struct sde_hdmi *display = (struct sde_hdmi *)hdmi_display;
	struct hdmi *hdmi = display->ctrl.ctrl;
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;
	u32 rem;

	if (!hdmi) {
		pr_err("Invalid ddc data\n");
		return -EINVAL;
	}

	ddc_ctrl = &display->ddc_ctrl;
	data = &ddc_ctrl->sde_hdcp2p2_ddc_data;
	if (!data) {
		pr_err("Invalid ddc data\n");
		return -EINVAL;
	}

	rc = ddc_clear_irq(hdmi);
	if (rc) {
		pr_err("DDC clear irq failed\n");
		return rc;
	}
	intr_en_mask = data->intr_mask;
	intr_en_mask |= BIT(HDCP2P2_RXSTATUS_DDC_FAILED_INTR_MASK);

	/* Disable short read for now, sinks don't support it */
	reg_val = hdmi_read(hdmi, HDMI_HDCP2P2_DDC_CTRL);
	reg_val |= BIT(4);
	hdmi_write(hdmi, HDMI_HDCP2P2_DDC_CTRL, reg_val);
	/*
	 * Setup the DDC timers for HDMI_HDCP2P2_DDC_TIMER_CTRL1 and
	 *  HDMI_HDCP2P2_DDC_TIMER_CTRL2.
	 * Following are the timers:
	 * 1. DDC_REQUEST_TIMER: Timeout in hsyncs in which to wait for the
	 *    HDCP 2.2 sink to respond to an RxStatus request
	 * 2. DDC_URGENT_TIMER: Time period in hsyncs to issue an urgent flag
	 *    when an RxStatus DDC request is made but not accepted by I2C
	 *    engine
	 * 3. DDC_TIMEOUT_TIMER: Timeout in hsyncs which starts counting when
	 *    a request is made and stops when it is accepted by DDC arbiter
	*/

	timeout = data->timeout_hsync;
	timer = data->periodic_timer_hsync;

	hdmi_write(hdmi, HDMI_HDCP2P2_DDC_TIMER_CTRL, timer);
	/* Set both urgent and hw-timeout fields to the same value */
	hdmi_write(hdmi, HDMI_HDCP2P2_DDC_TIMER_CTRL2,
			   (timeout << 16 | timeout));
	/* enable interrupts */
	reg_val = intr_en_mask;
	/* Clear interrupt status bits */
	reg_val |= intr_en_mask >> 1;

	hdmi_write(hdmi, HDMI_DDC_INT_CTRL0, reg_val);
	reg_val = hdmi_read(hdmi, HDMI_DDC_INT_CTRL5);
	/* clear and enable RxStatus read timeout */
	reg_val |= BIT(2) | BIT(1);

	hdmi_write(hdmi, HDMI_DDC_INT_CTRL5, reg_val);
	/*
	 * Enable hardware DDC access to RxStatus register
	 *
	 * HDMI_HW_DDC_CTRL:Bits 1:0 (RXSTATUS_DDC_ENABLE) read like this:
	 *
	 * 0 = disable HW controlled DDC access to RxStatus
	 * 1 = automatic on when HDCP 2.2 is authenticated and loop based on
	 * request timer (i.e. the hardware will loop automatically)
	 * 2 = force on and loop based on request timer (hardware will loop)
	 * 3 = enable by sw trigger and loop until interrupt is generated for
	 * RxStatus.reauth_req, RxStatus.ready or RxStatus.message_Size.
	 *
	 * Depending on the value of ddc_data::poll_sink, we make the decision
	 * to use either SW_TRIGGER(3) (poll_sink = false) which means that the
	 * hardware will poll sink and generate interrupt when sink responds,
	 * or use AUTOMATIC_LOOP(1) (poll_sink = true) which will poll the sink
	 * based on request timer
	 */

	reg_val = hdmi_read(hdmi, HDMI_HW_DDC_CTRL);
	reg_val &= ~(BIT(1) | BIT(0));

	busy_wait_us = data->timeout_ms * HDMI_MS_TO_US;

	/* read method: HDCP2P2_RXSTATUS_HW_DDC_SW_TRIGGER */
	reg_val |= BIT(1) | BIT(0);
	hdmi_write(hdmi, HDMI_HW_DDC_CTRL, reg_val);

	hdmi_write(hdmi, HDMI_HDCP2P2_DDC_SW_TRIGGER, 1);
	if (data->wait) {
		reinit_completion(&ddc_ctrl->rx_status_done);
		rem = wait_for_completion_timeout(&ddc_ctrl->rx_status_done,
		HZ);
		data->timeout_left = jiffies_to_msecs(rem);

		if (!data->timeout_left) {
			pr_err("sw ddc rxstatus timeout\n");
			rc = -ETIMEDOUT;
		}
		sde_hdmi_hdcp2p2_ddc_disable((void *)display);
	}
	return rc;
}

unsigned long sde_hdmi_calc_pixclk(unsigned long pixel_freq,
	u32 out_format, bool dc_enable)
{
	u32 rate_ratio = HDMI_RGB_24BPP_PCLK_TMDS_CH_RATE_RATIO;

	if (out_format & MSM_MODE_FLAG_COLOR_FORMAT_YCBCR420)
		rate_ratio = HDMI_YUV420_24BPP_PCLK_TMDS_CH_RATE_RATIO;

	pixel_freq /= rate_ratio;

	if (dc_enable)
		pixel_freq += pixel_freq >> 2;

	return pixel_freq;

}

bool sde_hdmi_validate_pixclk(struct drm_connector *connector,
	unsigned long pclk)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct sde_hdmi *display = (struct sde_hdmi *)c_conn->display;
	unsigned long max_pclk = display->max_pclk_khz * HDMI_KHZ_TO_HZ;

	if (connector->max_tmds_char)
		max_pclk = MIN(max_pclk,
			connector->max_tmds_char * HDMI_MHZ_TO_HZ);
	else if (connector->max_tmds_clock)
		max_pclk = MIN(max_pclk,
			connector->max_tmds_clock * HDMI_MHZ_TO_HZ);

	SDE_DEBUG("MAX PCLK = %ld, PCLK = %ld\n", max_pclk, pclk);

	return pclk < max_pclk;
}

static bool sde_hdmi_check_dc_clock(struct drm_connector *connector,
	struct drm_display_mode *mode, u32 format)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct sde_hdmi *display = (struct sde_hdmi *)c_conn->display;

	 u32 tmds_clk_with_dc = sde_hdmi_calc_pixclk(
					mode->clock * HDMI_KHZ_TO_HZ,
					format,
					true);

	return (display->dc_feature_supported &&
		 sde_hdmi_validate_pixclk(connector, tmds_clk_with_dc));
}

int sde_hdmi_sink_dc_support(struct drm_connector *connector,
	struct drm_display_mode *mode)
{
	int dc_format = 0;

	if ((mode->flags & DRM_MODE_FLAG_SUPPORTS_YUV) &&
	    (connector->display_info.edid_hdmi_dc_modes
	     & DRM_EDID_YCBCR420_DC_30))
		if (sde_hdmi_check_dc_clock(connector, mode,
				MSM_MODE_FLAG_COLOR_FORMAT_YCBCR420))
			dc_format |= MSM_MODE_FLAG_YUV420_DC_ENABLE;

	if ((mode->flags & DRM_MODE_FLAG_SUPPORTS_RGB) &&
	    (connector->display_info.edid_hdmi_dc_modes
	     & DRM_EDID_HDMI_DC_30))
		if (sde_hdmi_check_dc_clock(connector, mode,
				MSM_MODE_FLAG_COLOR_FORMAT_RGB444))
			dc_format |= MSM_MODE_FLAG_RGB444_DC_ENABLE;

	return dc_format;
}

u8 sde_hdmi_hdr_get_ops(u8 curr_state,
	u8 new_state)
{

	/** There could be 4 valid state transitions:
	 * 1. HDR_DISABLE -> HDR_ENABLE
	 *
	 * In this transition, we shall start sending
	 * HDR metadata with metadata from the HDR clip
	 *
	 * 2. HDR_ENABLE -> HDR_RESET
	 *
	 * In this transition, we will keep sending
	 * HDR metadata but with EOTF and metadata as 0
	 *
	 * 3. HDR_RESET -> HDR_ENABLE
	 *
	 * In this transition, we will start sending
	 * HDR metadata with metadata from the HDR clip
	 *
	 * 4. HDR_RESET -> HDR_DISABLE
	 *
	 * In this transition, we will stop sending
	 * metadata to the sink and clear PKT_CTRL register
	 * bits.
	 */

	if ((curr_state == HDR_DISABLE)
				&& (new_state == HDR_ENABLE)) {
		HDMI_UTIL_DEBUG("State changed %s ---> %s\n",
						sde_hdmi_hdr_sname(curr_state),
						sde_hdmi_hdr_sname(new_state));
		return HDR_SEND_INFO;
	} else if ((curr_state == HDR_ENABLE)
				&& (new_state == HDR_RESET)) {
		HDMI_UTIL_DEBUG("State changed %s ---> %s\n",
						sde_hdmi_hdr_sname(curr_state),
						sde_hdmi_hdr_sname(new_state));
		return HDR_SEND_INFO;
	} else if ((curr_state == HDR_RESET)
				&& (new_state == HDR_ENABLE)) {
		HDMI_UTIL_DEBUG("State changed %s ---> %s\n",
						sde_hdmi_hdr_sname(curr_state),
						sde_hdmi_hdr_sname(new_state));
		return HDR_SEND_INFO;
	} else if ((curr_state == HDR_RESET)
				&& (new_state == HDR_DISABLE)) {
		HDMI_UTIL_DEBUG("State changed %s ---> %s\n",
						sde_hdmi_hdr_sname(curr_state),
						sde_hdmi_hdr_sname(new_state));
		return HDR_CLEAR_INFO;
	}

	HDMI_UTIL_DEBUG("Unsupported OR no state change\n");
	return HDR_UNSUPPORTED_OP;
}

