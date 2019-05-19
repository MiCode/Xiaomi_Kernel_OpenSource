/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/of_irq.h>
#include "hdmi.h"

void init_ddc(struct hdmi *hdmi)
{

	uint32_t ddc_speed;

	hdmi_write(hdmi, REG_HDMI_DDC_CTRL,
			HDMI_DDC_CTRL_SW_STATUS_RESET);
	hdmi_write(hdmi, REG_HDMI_DDC_CTRL,
			HDMI_DDC_CTRL_SOFT_RESET);

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

int ddc_clear_irq(struct hdmi *hdmi)
{
	struct hdmi_i2c_adapter *hdmi_i2c = to_hdmi_i2c_adapter(hdmi->i2c);
	struct drm_device *dev = hdmi->dev;
	uint32_t retry = 0xffff;
	uint32_t ddc_int_ctrl;

	do {
		--retry;

		hdmi_write(hdmi, REG_HDMI_DDC_INT_CTRL,
				HDMI_DDC_INT_CTRL_SW_DONE_ACK |
				HDMI_DDC_INT_CTRL_SW_DONE_MASK);

		ddc_int_ctrl = hdmi_read(hdmi, REG_HDMI_DDC_INT_CTRL);

	} while ((ddc_int_ctrl & HDMI_DDC_INT_CTRL_SW_DONE_INT) && retry);

	if (!retry) {
		dev_err(dev->dev, "timeout waiting for DDC\n");
		return -ETIMEDOUT;
	}

	hdmi_i2c->sw_done = false;

	return 0;
}

int hdmi_ddc_read(struct hdmi *hdmi, u16 addr, u8 offset,
u8 *data, u16 data_len, bool self_retry)
{
	int rc;
	int retry = 10;
	struct i2c_msg msgs[] = {
		{
			.addr	= addr >> 1,
			.flags	= 0,
			.len	= 1,
			.buf	= &offset,
		}, {
			.addr	= addr >> 1,
			.flags	= I2C_M_RD,
			.len	= data_len,
			.buf	= data,
		}
	};

	DBG("Start DDC read");
retry:
	rc = i2c_transfer(hdmi->i2c, msgs, 2);
	retry--;

	if (rc == 2)
		rc = 0;
	else if (self_retry && (retry > 0))
		goto retry;
	else
		rc = -EIO;

	DBG("End DDC read %d", rc);

	return rc;
}

#define HDCP_DDC_WRITE_MAX_BYTE_NUM 1024

int hdmi_ddc_write(struct hdmi *hdmi, u16 addr, u8 offset,
				   u8 *data, u16 data_len, bool self_retry)
{
	int rc;
	int retry = 10;
	u8 buf[HDCP_DDC_WRITE_MAX_BYTE_NUM];
	struct i2c_msg msgs[] = {
		{
			.addr	= addr >> 1,
			.flags	= 0,
			.len	= 1,
		}
	};

	pr_debug("TESTING ! REMOVE RETRY Start DDC write");
	if (data_len > (HDCP_DDC_WRITE_MAX_BYTE_NUM - 1)) {
		pr_err("%s: write size too big\n", __func__);
		return -ERANGE;
	}

	buf[0] = offset;
	memcpy(&buf[1], data, data_len);
	msgs[0].buf = buf;
	msgs[0].len = data_len + 1;
retry:
	rc = i2c_transfer(hdmi->i2c, msgs, 1);
	retry--;
	if (rc == 1)
		rc = 0;
	else if (self_retry && (retry > 0))
		goto retry;
	else
		rc = -EIO;

	DBG("End DDC write %d", rc);

	return rc;
}
