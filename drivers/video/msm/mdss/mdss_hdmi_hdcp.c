/* Copyright (c) 2010-2014 The Linux Foundation. All rights reserved.
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

#include <linux/io.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <soc/qcom/scm.h>

#include "mdss_hdmi_hdcp.h"
#include "video/msm_hdmi_hdcp_mgr.h"

#define HDCP_STATE_NAME (hdcp_state_name(hdcp_ctrl->hdcp_state))

/* HDCP Keys state based on HDMI_HDCP_LINK0_STATUS:KEYS_STATE */
#define HDCP_KEYS_STATE_NO_KEYS		0
#define HDCP_KEYS_STATE_NOT_CHECKED	1
#define HDCP_KEYS_STATE_CHECKING	2
#define HDCP_KEYS_STATE_VALID		3
#define HDCP_KEYS_STATE_AKSV_NOT_VALID	4
#define HDCP_KEYS_STATE_CHKSUM_MISMATCH	5
#define HDCP_KEYS_STATE_PROD_AKSV	6
#define HDCP_KEYS_STATE_RESERVED	7

#define TZ_HDCP_CMD_ID 0x00004401
#define HDCP_REG_ENABLE 0x01
#define HDCP_REG_DISABLE 0x00

#define HDCP_INT_CLR (BIT(1) | BIT(5) | BIT(7) | BIT(9) | BIT(13))

struct hdmi_hdcp_reg_data {
	u32 reg_id;
	u32 off;
	char *name;
	u32 reg_val;
};

struct hdmi_hdcp_ctrl {
	u32 auth_retries;
	u32 tp_msgid;
	u32 tz_hdcp;
	enum hdmi_hdcp_state hdcp_state;
	struct HDCP_V2V1_MSG_TOPOLOGY cached_tp;
	struct HDCP_V2V1_MSG_TOPOLOGY current_tp;
	struct delayed_work hdcp_auth_work;
	struct work_struct hdcp_int_work;
	struct completion r0_checked;
	struct hdmi_hdcp_init_data init_data;
};

const char *hdcp_state_name(enum hdmi_hdcp_state hdcp_state)
{
	switch (hdcp_state) {
	case HDCP_STATE_INACTIVE:	return "HDCP_STATE_INACTIVE";
	case HDCP_STATE_AUTHENTICATING:	return "HDCP_STATE_AUTHENTICATING";
	case HDCP_STATE_AUTHENTICATED:	return "HDCP_STATE_AUTHENTICATED";
	case HDCP_STATE_AUTH_FAIL:	return "HDCP_STATE_AUTH_FAIL";
	default:			return "???";
	}
} /* hdcp_state_name */

static int hdmi_hdcp_count_one(u8 *array, u8 len)
{
	int i, j, count = 0;
	for (i = 0; i < len; i++)
		for (j = 0; j < 8; j++)
			count += (((array[i] >> j) & 0x1) ? 1 : 0);
	return count;
} /* hdmi_hdcp_count_one */

static void reset_hdcp_ddc_failures(struct hdmi_hdcp_ctrl *hdcp_ctrl)
{
	int hdcp_ddc_ctrl1_reg;
	int hdcp_ddc_status;
	int failure;
	int nack0;
	struct dss_io_data *io;

	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = hdcp_ctrl->init_data.core_io;

	/* Check for any DDC transfer failures */
	hdcp_ddc_status = DSS_REG_R(io, HDMI_HDCP_DDC_STATUS);
	failure = (hdcp_ddc_status >> 16) & 0x1;
	nack0 = (hdcp_ddc_status >> 14) & 0x1;
	DEV_DBG("%s: %s: On Entry: HDCP_DDC_STATUS=0x%x, FAIL=%d, NACK0=%d\n",
		__func__, HDCP_STATE_NAME, hdcp_ddc_status, failure, nack0);

	if (failure == 0x1) {
		/*
		 * Indicates that the last HDCP HW DDC transfer failed.
		 * This occurs when a transfer is attempted with HDCP DDC
		 * disabled (HDCP_DDC_DISABLE=1) or the number of retries
		 * matches HDCP_DDC_RETRY_CNT.
		 * Failure occured,  let's clear it.
		 */
		DEV_DBG("%s: %s: DDC failure detected.HDCP_DDC_STATUS=0x%08x\n",
			 __func__, HDCP_STATE_NAME, hdcp_ddc_status);

		/* First, Disable DDC */
		DSS_REG_W(io, HDMI_HDCP_DDC_CTRL_0, BIT(0));

		/* ACK the Failure to Clear it */
		hdcp_ddc_ctrl1_reg = DSS_REG_R(io, HDMI_HDCP_DDC_CTRL_1);
		DSS_REG_W(io, HDMI_HDCP_DDC_CTRL_1,
			hdcp_ddc_ctrl1_reg | BIT(0));

		/* Check if the FAILURE got Cleared */
		hdcp_ddc_status = DSS_REG_R(io, HDMI_HDCP_DDC_STATUS);
		hdcp_ddc_status = (hdcp_ddc_status >> 16) & BIT(0);
		if (hdcp_ddc_status == 0x0)
			DEV_DBG("%s: %s: HDCP DDC Failure cleared\n", __func__,
				HDCP_STATE_NAME);
		else
			DEV_WARN("%s: %s: Unable to clear HDCP DDC Failure",
				__func__, HDCP_STATE_NAME);

		/* Re-Enable HDCP DDC */
		DSS_REG_W(io, HDMI_HDCP_DDC_CTRL_0, 0);
	}

	if (nack0 == 0x1) {
		DEV_DBG("%s: %s: Before: HDMI_DDC_SW_STATUS=0x%08x\n", __func__,
			HDCP_STATE_NAME, DSS_REG_R(io, HDMI_DDC_SW_STATUS));
		/* Reset HDMI DDC software status */
		DSS_REG_W_ND(io, HDMI_DDC_CTRL,
			DSS_REG_R(io, HDMI_DDC_CTRL) | BIT(3));
		msleep(20);
		DSS_REG_W_ND(io, HDMI_DDC_CTRL,
			DSS_REG_R(io, HDMI_DDC_CTRL) & ~(BIT(3)));

		/* Reset HDMI DDC Controller */
		DSS_REG_W_ND(io, HDMI_DDC_CTRL,
			DSS_REG_R(io, HDMI_DDC_CTRL) | BIT(1));
		msleep(20);
		DSS_REG_W_ND(io, HDMI_DDC_CTRL,
			DSS_REG_R(io, HDMI_DDC_CTRL) & ~BIT(1));
		DEV_DBG("%s: %s: After: HDMI_DDC_SW_STATUS=0x%08x\n", __func__,
			HDCP_STATE_NAME, DSS_REG_R(io, HDMI_DDC_SW_STATUS));
	}

	hdcp_ddc_status = DSS_REG_R(io, HDMI_HDCP_DDC_STATUS);

	failure = (hdcp_ddc_status >> 16) & BIT(0);
	nack0 = (hdcp_ddc_status >> 14) & BIT(0);
	DEV_DBG("%s: %s: On Exit: HDCP_DDC_STATUS=0x%x, FAIL=%d, NACK0=%d\n",
		__func__, HDCP_STATE_NAME, hdcp_ddc_status, failure, nack0);
} /* reset_hdcp_ddc_failures */

static void hdmi_hdcp_hw_ddc_clean(struct hdmi_hdcp_ctrl *hdcp_ctrl)
{
	struct dss_io_data *io = NULL;
	u32 hdcp_ddc_status, ddc_hw_status;
	u32 ddc_xfer_done, ddc_xfer_req, ddc_hw_done;
	u32 ddc_hw_not_ready;
	u32 timeout_count;

	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = hdcp_ctrl->init_data.core_io;
	if (!io->base) {
			DEV_ERR("%s: core io not inititalized\n", __func__);
			return;
	}

	if (DSS_REG_R(io, HDMI_DDC_HW_STATUS) != 0) {
		/* Wait to be clean on DDC HW engine */
		timeout_count = 100;
		do {
			hdcp_ddc_status = DSS_REG_R(io, HDMI_HDCP_DDC_STATUS);
			ddc_hw_status = DSS_REG_R(io, HDMI_DDC_HW_STATUS);
			ddc_xfer_done = hdcp_ddc_status & BIT(10);
			ddc_xfer_req = hdcp_ddc_status & BIT(4);
			ddc_hw_done = ddc_hw_status & BIT(3);
			ddc_hw_not_ready = !ddc_xfer_done ||
				ddc_xfer_req || !ddc_hw_done;

			DEV_DBG("%s: %s: timeout count(%d):ddc hw%sready\n",
				__func__, HDCP_STATE_NAME, timeout_count,
					ddc_hw_not_ready ? " not " : " ");
			DEV_DBG("hdcp_ddc_status[0x%x], ddc_hw_status[0x%x]\n",
					hdcp_ddc_status, ddc_hw_status);
			if (ddc_hw_not_ready)
				msleep(20);
			} while (ddc_hw_not_ready && --timeout_count);
	}
} /* hdmi_hdcp_hw_ddc_clean */

static int hdmi_hdcp_authentication_part1(struct hdmi_hdcp_ctrl *hdcp_ctrl)
{
	int rc;
	u32 qfprom_aksv_lsb, qfprom_aksv_msb;
	u32 link0_aksv_0, link0_aksv_1;
	u32 link0_bksv_0, link0_bksv_1;
	u32 link0_an_0, link0_an_1;
	u32 timeout_count;
	bool is_match;
	bool stale_an = false;
	struct dss_io_data *io;
	u8 aksv[5], *bksv = NULL;
	u8 an[8];
	u8 bcaps;
	struct hdmi_tx_ddc_data ddc_data;
	u32 link0_status, an_ready, keys_state;
	u8 buf[0xFF];

	struct scm_hdcp_req scm_buf[SCM_HDCP_MAX_REG];
	u32 phy_addr;
	u32 ret  = 0;
	u32 resp = 0;

	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io ||
		!hdcp_ctrl->init_data.qfprom_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	phy_addr = hdcp_ctrl->init_data.phy_addr;

	if (HDCP_STATE_AUTHENTICATING != hdcp_ctrl->hdcp_state) {
		DEV_DBG("%s: %s: invalid state. returning\n", __func__,
			HDCP_STATE_NAME);
		rc = -EINVAL;
		goto error;
	}

	bksv = hdcp_ctrl->current_tp.bksv;

	io = hdcp_ctrl->init_data.core_io;

	/* Fetch aksv from QFPROM, this info should be public. */
	qfprom_aksv_lsb = DSS_REG_R(hdcp_ctrl->init_data.qfprom_io,
		HDCP_KSV_LSB);
	qfprom_aksv_msb = DSS_REG_R(hdcp_ctrl->init_data.qfprom_io,
		HDCP_KSV_MSB);

	aksv[0] =  qfprom_aksv_lsb        & 0xFF;
	aksv[1] = (qfprom_aksv_lsb >> 8)  & 0xFF;
	aksv[2] = (qfprom_aksv_lsb >> 16) & 0xFF;
	aksv[3] = (qfprom_aksv_lsb >> 24) & 0xFF;
	aksv[4] =  qfprom_aksv_msb        & 0xFF;

	/* check there are 20 ones in AKSV */
	if (hdmi_hdcp_count_one(aksv, 5) != 20) {
		DEV_ERR("%s: %s: AKSV QFPROM doesn't have 20 1's, 20 0's\n",
			__func__, HDCP_STATE_NAME);
		DEV_ERR("%s: %s: QFPROM AKSV chk failed (AKSV=%02x%08x)\n",
			__func__, HDCP_STATE_NAME, qfprom_aksv_msb,
			qfprom_aksv_lsb);
		rc = -EINVAL;
		goto error;
	}
	DEV_DBG("%s: %s: AKSV=%02x%08x\n", __func__, HDCP_STATE_NAME,
		qfprom_aksv_msb, qfprom_aksv_lsb);

	/*
	 * Write AKSV read from QFPROM to the HDCP registers.
	 * This step is needed for HDCP authentication and must be
	 * written before enabling HDCP.
	 */
	DSS_REG_W(io, HDMI_HDCP_SW_LOWER_AKSV, qfprom_aksv_lsb);
	DSS_REG_W(io, HDMI_HDCP_SW_UPPER_AKSV, qfprom_aksv_msb);

	/* Check to see if link0_Status has stale values for An ready bit */
	link0_status = DSS_REG_R(io, HDMI_HDCP_LINK0_STATUS);
	DEV_DBG("%s: %s: Before enabling cipher Link0_status=0x%08x\n",
		__func__, HDCP_STATE_NAME, link0_status);
	if (link0_status & (BIT(8) | BIT(9))) {
		DEV_DBG("%s: %s: An ready even before enabling HDCP\n",
		__func__, HDCP_STATE_NAME);
		stale_an = true;
	}

	/*
	 * Read BCAPS
	 * We need to first try to read an HDCP register on the sink to see if
	 * the sink is ready for HDCP authentication
	 */
	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = 0x74;
	ddc_data.offset = 0x40;
	ddc_data.data_buf = &bcaps;
	ddc_data.data_len = 1;
	ddc_data.request_len = 1;
	ddc_data.retry = 5;
	ddc_data.what = "Bcaps";
	ddc_data.no_align = true;
	rc = hdmi_ddc_read(hdcp_ctrl->init_data.ddc_ctrl, &ddc_data);
	if (rc) {
		DEV_ERR("%s: %s: BCAPS read failed\n", __func__,
			HDCP_STATE_NAME);
		goto error;
	}
	DEV_DBG("%s: %s: BCAPS=%02x\n", __func__, HDCP_STATE_NAME, bcaps);

	/* receiver (0), repeater (1) */
	hdcp_ctrl->current_tp.ds_type =
		(bcaps & BIT(6)) >> 6 ? DS_REPEATER : DS_RECEIVER;

	/*
	 * HDCP setup prior to enabling HDCP_CTRL.
	 * Setup seed values for random number An.
	 */
	DSS_REG_W(io, HDMI_HDCP_ENTROPY_CTRL0, 0xB1FFB0FF);
	DSS_REG_W(io, HDMI_HDCP_ENTROPY_CTRL1, 0xF00DFACE);

	/* Disable the RngCipher state */
	DSS_REG_W(io, HDMI_HDCP_DEBUG_CTRL,
		DSS_REG_R(io, HDMI_HDCP_DEBUG_CTRL) & ~(BIT(2)));
	DEV_DBG("%s: %s: HDCP_DEBUG_CTRL=0x%08x\n", __func__, HDCP_STATE_NAME,
		DSS_REG_R(io, HDMI_HDCP_DEBUG_CTRL));

	/* Ensure that all register writes are completed before
	 * enabling HDCP cipher
	 */
	wmb();

	/*
	 * Enable HDCP
	 * This needs to be done as early as possible in order for the
	 * hardware to make An available to read
	 */
	DSS_REG_W(io, HDMI_HDCP_CTRL, BIT(0));

	/* Clear any DDC failures from previous tries */
	reset_hdcp_ddc_failures(hdcp_ctrl);

	/* Write BCAPS to the hardware */

	if (hdcp_ctrl->tz_hdcp) {
		memset(scm_buf, 0x00, sizeof(scm_buf));

		/* Write BCAPS to hardware */
		scm_buf[0].addr = phy_addr + HDMI_HDCP_RCVPORT_DATA12;
		scm_buf[0].val  = bcaps;

		ret = scm_call(SCM_SVC_HDCP, SCM_CMD_HDCP, (void *) scm_buf,
			sizeof(scm_buf), (void *) &resp, sizeof(resp));

		if (ret || resp) {
			DEV_ERR("%s: error: scm_call ret = %d, resp = %d\n",
				__func__, ret, resp);
			rc = -EINVAL;
			goto error;
		}
	} else {
		DSS_REG_W(io, HDMI_HDCP_RCVPORT_DATA12, bcaps);
	}

	/*
	 * If we had stale values for the An ready bit, it should most
	 * likely be cleared now after enabling HDCP cipher
	 */
	link0_status = DSS_REG_R(io, HDMI_HDCP_LINK0_STATUS);
	DEV_DBG("%s: %s: After enabling HDCP Link0_Status=0x%08x\n",
		__func__, HDCP_STATE_NAME, link0_status);
	if (!(link0_status & (BIT(8) | BIT(9)))) {
		DEV_DBG("%s: %s: An not ready after enabling HDCP\n",
			__func__, HDCP_STATE_NAME);
		stale_an = false;
	}

	/* Wait for HDCP keys to be checked and validated */
	timeout_count = 100;
	keys_state = (link0_status >> 28) & 0x7;
	while ((keys_state != HDCP_KEYS_STATE_VALID) &&
		--timeout_count) {
		link0_status = DSS_REG_R(io, HDMI_HDCP_LINK0_STATUS);
		keys_state = (link0_status >> 28) & 0x7;
		DEV_DBG("%s: %s: Keys not ready(%d). s=%d\n, l0=%0x08x",
			__func__, HDCP_STATE_NAME, timeout_count,
			keys_state, link0_status);
		msleep(20);
	}

	if (!timeout_count) {
		DEV_ERR("%s: %s: Invalid Keys State: %d\n", __func__,
			HDCP_STATE_NAME, keys_state);
		rc = -EINVAL;
		goto error;
	}

	/*
	 * 1.1_Features turned off by default.
	 * No need to write AInfo since 1.1_Features is disabled.
	 */
	DSS_REG_W(io, HDMI_HDCP_RCVPORT_DATA4, 0);

	/* Wait for An0 and An1 bit to be ready */
	timeout_count = 100;
	do {
		link0_status = DSS_REG_R(io, HDMI_HDCP_LINK0_STATUS);
		an_ready = (link0_status & BIT(8)) && (link0_status & BIT(9));
		if (!an_ready) {
			DEV_DBG("%s: %s: An not ready(%d). l0_status=0x%08x\n",
				__func__, HDCP_STATE_NAME, timeout_count,
				link0_status);
			msleep(20);
		}
	} while (!an_ready && --timeout_count);

	if (!timeout_count) {
		rc = -ETIMEDOUT;
		DEV_ERR("%s: %s: timedout, An0=%ld, An1=%ld\n", __func__,
			HDCP_STATE_NAME, (link0_status & BIT(8)) >> 8,
			(link0_status & BIT(9)) >> 9);
		goto error;
	}

	/*
	 * In cases where An_ready bits had stale values, it would be
	 * better to delay reading of An to avoid any potential of this
	 * read being blocked
	 */
	if (stale_an) {
		msleep(200);
		stale_an = false;
	}

	/* Read An0 and An1 */
	link0_an_0 = DSS_REG_R(io, HDMI_HDCP_RCVPORT_DATA5);
	link0_an_1 = DSS_REG_R(io, HDMI_HDCP_RCVPORT_DATA6);

	/* Read AKSV */
	link0_aksv_0 = DSS_REG_R(io, HDMI_HDCP_RCVPORT_DATA3);
	link0_aksv_1 = DSS_REG_R(io, HDMI_HDCP_RCVPORT_DATA4);

	/* Copy An and AKSV to byte arrays for transmission */
	aksv[0] =  link0_aksv_0        & 0xFF;
	aksv[1] = (link0_aksv_0 >> 8)  & 0xFF;
	aksv[2] = (link0_aksv_0 >> 16) & 0xFF;
	aksv[3] = (link0_aksv_0 >> 24) & 0xFF;
	aksv[4] =  link0_aksv_1        & 0xFF;

	an[0] =  link0_an_0        & 0xFF;
	an[1] = (link0_an_0 >> 8)  & 0xFF;
	an[2] = (link0_an_0 >> 16) & 0xFF;
	an[3] = (link0_an_0 >> 24) & 0xFF;
	an[4] =  link0_an_1        & 0xFF;
	an[5] = (link0_an_1 >> 8)  & 0xFF;
	an[6] = (link0_an_1 >> 16) & 0xFF;
	an[7] = (link0_an_1 >> 24) & 0xFF;

	/* Write An to offset 0x18 */
	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = 0x74;
	ddc_data.offset = 0x18;
	ddc_data.data_buf = an;
	ddc_data.data_len = 8;
	ddc_data.what = "An";
	rc = hdmi_ddc_write(hdcp_ctrl->init_data.ddc_ctrl, &ddc_data);
	if (rc) {
		DEV_ERR("%s: %s: An write failed\n", __func__, HDCP_STATE_NAME);
		goto error;
	}

	/* Write AKSV to offset 0x10 */
	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = 0x74;
	ddc_data.offset = 0x10;
	ddc_data.data_buf = aksv;
	ddc_data.data_len = 5;
	ddc_data.what = "Aksv";
	rc = hdmi_ddc_write(hdcp_ctrl->init_data.ddc_ctrl, &ddc_data);
	if (rc) {
		DEV_ERR("%s: %s: AKSV write failed\n", __func__,
			HDCP_STATE_NAME);
		goto error;
	}
	DEV_DBG("%s: %s: Link0-AKSV=%02x%08x\n", __func__,
		HDCP_STATE_NAME, link0_aksv_1 & 0xFF, link0_aksv_0);

	/* Read BKSV at offset 0x00 */
	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = 0x74;
	ddc_data.offset = 0x00;
	ddc_data.data_buf = bksv;
	ddc_data.data_len = 5;
	ddc_data.request_len = 5;
	ddc_data.retry = 5;
	ddc_data.what = "Bksv";
	ddc_data.no_align = true;
	rc = hdmi_ddc_read(hdcp_ctrl->init_data.ddc_ctrl, &ddc_data);
	if (rc) {
		DEV_ERR("%s: %s: BKSV read failed\n", __func__,
			HDCP_STATE_NAME);
		goto error;
	}

	/* check there are 20 ones in BKSV */
	if (hdmi_hdcp_count_one(bksv, 5) != 20) {
		DEV_ERR("%s: %s: BKSV doesn't have 20 1's and 20 0's\n",
			__func__, HDCP_STATE_NAME);
		DEV_ERR("%s: %s: BKSV chk fail. BKSV=%02x%02x%02x%02x%02x\n",
			__func__, HDCP_STATE_NAME, bksv[4], bksv[3], bksv[2],
			bksv[1], bksv[0]);
		rc = -EINVAL;
		goto error;
	}

	link0_bksv_0 = bksv[3];
	link0_bksv_0 = (link0_bksv_0 << 8) | bksv[2];
	link0_bksv_0 = (link0_bksv_0 << 8) | bksv[1];
	link0_bksv_0 = (link0_bksv_0 << 8) | bksv[0];
	link0_bksv_1 = bksv[4];
	DEV_DBG("%s: %s: BKSV=%02x%08x\n", __func__, HDCP_STATE_NAME,
		link0_bksv_1, link0_bksv_0);

	/* Write BKSV read from sink to HDCP registers */
	if (hdcp_ctrl->tz_hdcp) {
		memset(scm_buf, 0x00, sizeof(scm_buf));

		scm_buf[0].addr = phy_addr + HDMI_HDCP_RCVPORT_DATA0;
		scm_buf[0].val  = link0_bksv_0;
		scm_buf[1].addr = phy_addr + HDMI_HDCP_RCVPORT_DATA1;
		scm_buf[1].val  = link0_bksv_1;

		ret = scm_call(SCM_SVC_HDCP, SCM_CMD_HDCP, (void *) scm_buf,
			sizeof(scm_buf), (void *) &resp, sizeof(resp));

		if (ret || resp) {
			DEV_ERR("%s: error: scm_call ret = %d, resp = %d\n",
				__func__, ret, resp);
			rc = -EINVAL;
			goto error;
		}
	} else {
		DSS_REG_W(io, HDMI_HDCP_RCVPORT_DATA0, link0_bksv_0);
		DSS_REG_W(io, HDMI_HDCP_RCVPORT_DATA1, link0_bksv_1);
	}

	/* Enable HDCP interrupts and ack/clear any stale interrupts */
	DSS_REG_W(io, HDMI_HDCP_INT_CTRL, 0xE6);

	/*
	 * HDCP Compliace Test case 1A-01:
	 * Wait here at least 100ms before reading R0'
	 */
	msleep(125);

	/* Read R0' at offset 0x08 */
	memset(buf, 0, sizeof(buf));
	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = 0x74;
	ddc_data.offset = 0x08;
	ddc_data.data_buf = buf;
	ddc_data.data_len = 2;
	ddc_data.request_len = 2;
	ddc_data.retry = 5;
	ddc_data.what = "R0'";
	ddc_data.no_align = true;
	rc = hdmi_ddc_read(hdcp_ctrl->init_data.ddc_ctrl, &ddc_data);
	if (rc) {
		DEV_ERR("%s: %s: R0' read failed\n", __func__, HDCP_STATE_NAME);
		goto error;
	}
	DEV_DBG("%s: %s: R0'=%02x%02x\n", __func__, HDCP_STATE_NAME,
		buf[1], buf[0]);

	/* Write R0' to HDCP registers and check to see if it is a match */
	INIT_COMPLETION(hdcp_ctrl->r0_checked);
	DSS_REG_W(io, HDMI_HDCP_RCVPORT_DATA2_0, (((u32)buf[1]) << 8) | buf[0]);
	timeout_count = wait_for_completion_timeout(
		&hdcp_ctrl->r0_checked, HZ*2);
	link0_status = DSS_REG_R(io, HDMI_HDCP_LINK0_STATUS);
	is_match = link0_status & BIT(12);
	if (!is_match) {
		DEV_DBG("%s: %s: Link0_Status=0x%08x\n", __func__,
			HDCP_STATE_NAME, link0_status);
		if (!timeout_count) {
			DEV_ERR("%s: %s: Timeout. No R0 mtch. R0'=%02x%02x\n",
				__func__, HDCP_STATE_NAME, buf[1], buf[0]);
			rc = -ETIMEDOUT;
			goto error;
		} else {
			DEV_ERR("%s: %s: R0 mismatch. R0'=%02x%02x\n", __func__,
				HDCP_STATE_NAME, buf[1], buf[0]);
			rc = -EINVAL;
			goto error;
		}
	} else {
		DEV_DBG("%s: %s: R0 matches\n", __func__, HDCP_STATE_NAME);
	}

error:
	if (rc) {
		DEV_ERR("%s: %s: Authentication Part I failed\n", __func__,
			HDCP_STATE_NAME);
	} else {
		/* Enable HDCP Encryption */
		DSS_REG_W(io, HDMI_HDCP_CTRL, BIT(0) | BIT(8));
		DEV_INFO("%s: %s: Authentication Part I successful\n",
			__func__, HDCP_STATE_NAME);
	}
	return rc;
} /* hdmi_hdcp_authentication_part1 */

#define READ_WRITE_V_H(off, name, reg, wr) \
do { \
	ddc_data.offset = (off); \
	memset(what, 0, sizeof(what)); \
	snprintf(what, sizeof(what), (name)); \
	rc = hdmi_ddc_read(hdcp_ctrl->init_data.ddc_ctrl, &ddc_data); \
	if (rc) { \
		DEV_ERR("%s: %s: Read %s failed\n", __func__, HDCP_STATE_NAME, \
			what); \
		goto error; \
	} \
	DEV_DBG("%s: %s: %s: buf[0]=%x, buf[1]=%x, buf[2]=%x, buf[3]=%x\n", \
			__func__, HDCP_STATE_NAME, what, buf[0], buf[1], \
			buf[2], buf[3]); \
	if (wr) { \
		DSS_REG_W(io, (reg), \
			(buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0])); \
	} \
} while (0);

static int hdmi_hdcp_transfer_v_h(struct hdmi_hdcp_ctrl *hdcp_ctrl)
{
	char what[20];
	int rc = 0;
	u8 buf[4];
	struct hdmi_tx_ddc_data ddc_data;
	struct dss_io_data *io;

	struct scm_hdcp_req scm_buf[SCM_HDCP_MAX_REG];
	u32 phy_addr;

	struct hdmi_hdcp_reg_data reg_data[]  = {
		{HDMI_HDCP_RCVPORT_DATA7,  0x20, "V' H0"},
		{HDMI_HDCP_RCVPORT_DATA8,  0x24, "V' H1"},
		{HDMI_HDCP_RCVPORT_DATA9,  0x28, "V' H2"},
		{HDMI_HDCP_RCVPORT_DATA10, 0x2C, "V' H3"},
		{HDMI_HDCP_RCVPORT_DATA11, 0x30, "V' H4"},
	};
	u32 size = sizeof(reg_data)/sizeof(reg_data[0]);
	u32 iter = 0;
	u32 ret  = 0;
	u32 resp = 0;

	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	phy_addr = hdcp_ctrl->init_data.phy_addr;

	io = hdcp_ctrl->init_data.core_io;
	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = 0x74;
	ddc_data.data_buf = buf;
	ddc_data.data_len = 4;
	ddc_data.request_len = 4;
	ddc_data.retry = 5;
	ddc_data.what = what;
	ddc_data.no_align = true;

	if (hdcp_ctrl->tz_hdcp) {
		memset(scm_buf, 0x00, sizeof(scm_buf));

		for (iter = 0; iter < size && iter < SCM_HDCP_MAX_REG; iter++) {
			struct hdmi_hdcp_reg_data *rd = reg_data + iter;

			READ_WRITE_V_H(rd->off, rd->name, 0, false);

			rd->reg_val = buf[3] << 24 | buf[2] << 16 |
				buf[1] << 8 | buf[0];

			scm_buf[iter].addr = phy_addr + reg_data[iter].reg_id;
			scm_buf[iter].val  = reg_data[iter].reg_val;
		}

		ret = scm_call(SCM_SVC_HDCP, SCM_CMD_HDCP, (void *) scm_buf,
			sizeof(scm_buf), (void *) &resp, sizeof(resp));

		if (ret || resp) {
			DEV_ERR("%s: error: scm_call ret = %d, resp = %d\n",
				__func__, ret, resp);
			rc = -EINVAL;
			goto error;
		}
	} else {
		/* Read V'.HO 4 Byte at offset 0x20 */
		READ_WRITE_V_H(0x20, "V' H0", HDMI_HDCP_RCVPORT_DATA7, true);

		/* Read V'.H1 4 Byte at offset 0x24 */
		READ_WRITE_V_H(0x24, "V' H1", HDMI_HDCP_RCVPORT_DATA8, true);

		/* Read V'.H2 4 Byte at offset 0x28 */
		READ_WRITE_V_H(0x28, "V' H2", HDMI_HDCP_RCVPORT_DATA9, true);

		/* Read V'.H3 4 Byte at offset 0x2C */
		READ_WRITE_V_H(0x2C, "V' H3", HDMI_HDCP_RCVPORT_DATA10, true);

		/* Read V'.H4 4 Byte at offset 0x30 */
		READ_WRITE_V_H(0x30, "V' H4", HDMI_HDCP_RCVPORT_DATA11, true);
	}

error:
	return rc;
}

static int hdmi_hdcp_authentication_part2(struct hdmi_hdcp_ctrl *hdcp_ctrl)
{
	int rc, cnt, i;
	struct hdmi_tx_ddc_data ddc_data;
	u32 timeout_count, down_stream_devices = 0;
	u32 repeater_cascade_depth = 0;
	u8 buf[0xFF];
	u8 *ksv_fifo = NULL;
	u8 bcaps;
	u16 bstatus, max_devs_exceeded = 0, max_cascade_exceeded = 0;
	u32 link0_status;
	u32 ksv_bytes;
	struct dss_io_data *io;

	struct scm_hdcp_req scm_buf[SCM_HDCP_MAX_REG];
	u32 phy_addr;
	u32 ret  = 0;
	u32 resp = 0;

	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	phy_addr = hdcp_ctrl->init_data.phy_addr;

	if (HDCP_STATE_AUTHENTICATING != hdcp_ctrl->hdcp_state) {
		DEV_DBG("%s: %s: invalid state. returning\n", __func__,
			HDCP_STATE_NAME);
		rc = -EINVAL;
		goto error;
	}

	ksv_fifo = hdcp_ctrl->current_tp.ksv_list;

	io = hdcp_ctrl->init_data.core_io;

	memset(buf, 0, sizeof(buf));
	memset(ksv_fifo, 0,
		sizeof(hdcp_ctrl->current_tp.ksv_list));

	/*
	 * Wait until READY bit is set in BCAPS, as per HDCP specifications
	 * maximum permitted time to check for READY bit is five seconds.
	 */
	timeout_count = 50;
	do {
		timeout_count--;
		/* Read BCAPS at offset 0x40 */
		memset(&ddc_data, 0, sizeof(ddc_data));
		ddc_data.dev_addr = 0x74;
		ddc_data.offset = 0x40;
		ddc_data.data_buf = &bcaps;
		ddc_data.data_len = 1;
		ddc_data.request_len = 1;
		ddc_data.retry = 5;
		ddc_data.what = "Bcaps";
		ddc_data.no_align = false;
		rc = hdmi_ddc_read(hdcp_ctrl->init_data.ddc_ctrl, &ddc_data);
		if (rc) {
			DEV_ERR("%s: %s: BCAPS read failed\n", __func__,
				HDCP_STATE_NAME);
			goto error;
		}
		msleep(100);
	} while (!(bcaps & BIT(5)) && timeout_count);

	/* Read BSTATUS at offset 0x41 */
	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = 0x74;
	ddc_data.offset = 0x41;
	ddc_data.data_buf = buf;
	ddc_data.data_len = 2;
	ddc_data.request_len = 2;
	ddc_data.retry = 5;
	ddc_data.what = "Bstatuss";
	ddc_data.no_align = false;
	rc = hdmi_ddc_read(hdcp_ctrl->init_data.ddc_ctrl, &ddc_data);
	if (rc) {
		DEV_ERR("%s: %s: BSTATUS read failed\n", __func__,
			HDCP_STATE_NAME);
		goto error;
	}
	bstatus = buf[1];
	bstatus = (bstatus << 8) | buf[0];

	if (hdcp_ctrl->tz_hdcp) {
		memset(scm_buf, 0x00, sizeof(scm_buf));

		/* Write BSTATUS and BCAPS to HDCP registers */
		scm_buf[0].addr = phy_addr + HDMI_HDCP_RCVPORT_DATA12;
		scm_buf[0].val  = bcaps | (bstatus << 8);

		ret = scm_call(SCM_SVC_HDCP, SCM_CMD_HDCP, (void *) scm_buf,
			sizeof(scm_buf), (void *) &resp, sizeof(resp));

		if (ret || resp) {
			DEV_ERR("%s: error: scm_call ret = %d, resp = %d\n",
				__func__, ret, resp);
			rc = -EINVAL;
			goto error;
		}
	} else {
		DSS_REG_W(io, HDMI_HDCP_RCVPORT_DATA12, bcaps | (bstatus << 8));
	}

	down_stream_devices = bstatus & 0x7F;
	if (down_stream_devices == 0) {
		/*
		 * If no downstream devices are attached to the repeater
		 * then part II fails.
		 * todo: The other approach would be to continue PART II.
		 */
		DEV_ERR("%s: %s: No downstream devices\n", __func__,
			HDCP_STATE_NAME);
		rc = -EINVAL;
		goto error;
	}

	/* Cascaded repeater depth */
	repeater_cascade_depth = (bstatus >> 8) & 0x7;

	/*
	 * HDCP Compliance 1B-05:
	 * Check if no. of devices connected to repeater
	 * exceed max_devices_connected from bit 7 of Bstatus.
	 */
	max_devs_exceeded = (bstatus & BIT(7)) >> 7;
	if (max_devs_exceeded == 0x01) {
		DEV_ERR("%s: %s: no. of devs connected exceeds max allowed",
			__func__, HDCP_STATE_NAME);
		rc = -EINVAL;
		goto error;
	}

	/*
	 * HDCP Compliance 1B-06:
	 * Check if no. of cascade connected to repeater
	 * exceed max_cascade_connected from bit 11 of Bstatus.
	 */
	max_cascade_exceeded = (bstatus & BIT(11)) >> 11;
	if (max_cascade_exceeded == 0x01) {
		DEV_ERR("%s: %s: no. of cascade conn exceeds max allowed",
			__func__, HDCP_STATE_NAME);
		rc = -EINVAL;
		goto error;
	}

	/*
	 * Read KSV FIFO over DDC
	 * Key Slection vector FIFO Used to pull downstream KSVs
	 * from HDCP Repeaters.
	 * All bytes (DEVICE_COUNT * 5) must be read in a single,
	 * auto incrementing access.
	 * All bytes read as 0x00 for HDCP Receivers that are not
	 * HDCP Repeaters (REPEATER == 0).
	 */
	ksv_bytes = 5 * down_stream_devices;
	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = 0x74;
	ddc_data.offset = 0x43;
	ddc_data.data_buf = ksv_fifo;
	ddc_data.data_len = ksv_bytes;
	ddc_data.request_len = ksv_bytes;
	ddc_data.retry = 5;
	ddc_data.what = "KSV FIFO";
	ddc_data.no_align = true;
	cnt = 0;
	do {
		rc = hdmi_ddc_read(hdcp_ctrl->init_data.ddc_ctrl, &ddc_data);
		if (rc) {
			DEV_ERR("%s: %s: KSV FIFO read failed\n", __func__,
				HDCP_STATE_NAME);
			/*
			 * HDCP Compliace Test case 1B-01:
			 * Wait here until all the ksv bytes have been
			 * read from the KSV FIFO register.
			 */
			msleep(25);
		} else {
			break;
		}
		cnt++;
	} while (cnt != 20);

	if (cnt == 20)
		goto error;

	rc = hdmi_hdcp_transfer_v_h(hdcp_ctrl);
	if (rc)
		goto error;

	/*
	 * Write KSV FIFO to HDCP_SHA_DATA.
	 * This is done 1 byte at time starting with the LSB.
	 * On the very last byte write, the HDCP_SHA_DATA_DONE bit[0]
	 */

	/* First, reset SHA engine */
	/* Next, enable SHA engine, SEL=DIGA_HDCP */
	if (hdcp_ctrl->tz_hdcp) {
		memset(scm_buf, 0x00, sizeof(scm_buf));

		scm_buf[0].addr = phy_addr + HDMI_HDCP_SHA_CTRL;
		scm_buf[0].val  = HDCP_REG_ENABLE;
		scm_buf[1].addr = phy_addr + HDMI_HDCP_SHA_CTRL;
		scm_buf[1].val  = HDCP_REG_DISABLE;

		ret = scm_call(SCM_SVC_HDCP, SCM_CMD_HDCP, (void *) scm_buf,
			sizeof(scm_buf), (void *) &resp,
			sizeof(resp));

		if (ret || resp) {
			DEV_ERR("%s: error: scm_call ret = %d, resp = %d\n",
				__func__, ret, resp);
			rc = -EINVAL;
			goto error;
		}
	} else {
		DSS_REG_W(io, HDMI_HDCP_SHA_CTRL, HDCP_REG_ENABLE);
		DSS_REG_W(io, HDMI_HDCP_SHA_CTRL, HDCP_REG_DISABLE);
	}

	for (i = 0; i < ksv_bytes - 1; i++) {
		/* Write KSV byte and do not set DONE bit[0] */
		if (hdcp_ctrl->tz_hdcp) {
			memset(scm_buf, 0x00, sizeof(scm_buf));

			scm_buf[0].addr = phy_addr + HDMI_HDCP_SHA_DATA;
			scm_buf[0].val  = ksv_fifo[i] << 16;

			ret = scm_call(SCM_SVC_HDCP, SCM_CMD_HDCP,
				(void *)scm_buf, sizeof(scm_buf),
				(void *) &resp, sizeof(resp));

			if (ret || resp) {
				DEV_ERR("%s: scm_call ret = %d, resp = %d\n",
					__func__, ret, resp);
				rc = -EINVAL;
				goto error;
			}
		} else {
			DSS_REG_W_ND(io, HDMI_HDCP_SHA_DATA, ksv_fifo[i] << 16);
		}

		/*
		 * Once 64 bytes have been written, we need to poll for
		 * HDCP_SHA_BLOCK_DONE before writing any further
		 */
		if (i && !((i + 1) % 64)) {
			timeout_count = 100;
			while (!(DSS_REG_R(io, HDMI_HDCP_SHA_STATUS) & BIT(0))
				&& (--timeout_count)) {
				DEV_DBG("%s: %s: Wrote 64 bytes KVS FIFO\n",
					__func__, HDCP_STATE_NAME);
				DEV_DBG("%s: %s: HDCP_SHA_STATUS=%08x\n",
					__func__, HDCP_STATE_NAME,
					DSS_REG_R(io, HDMI_HDCP_SHA_STATUS));
				msleep(20);
			}
			if (!timeout_count) {
				rc = -ETIMEDOUT;
				DEV_ERR("%s: %s: Write KSV FIFO timedout",
					__func__, HDCP_STATE_NAME);
				goto error;
			}
		}

	}

	/* Write l to DONE bit[0] */
	if (hdcp_ctrl->tz_hdcp) {
		memset(scm_buf, 0x00, sizeof(scm_buf));

		scm_buf[0].addr = phy_addr + HDMI_HDCP_SHA_DATA;
		scm_buf[0].val  = (ksv_fifo[ksv_bytes - 1] << 16) | 0x1;

		ret = scm_call(SCM_SVC_HDCP, SCM_CMD_HDCP, (void *) scm_buf,
			sizeof(scm_buf), (void *) &resp, sizeof(resp));

		if (ret || resp) {
			DEV_ERR("%s: error: scm_call ret = %d, resp = %d\n",
				__func__, ret, resp);
			rc = -EINVAL;
			goto error;
		}
	} else {
		DSS_REG_W_ND(io, HDMI_HDCP_SHA_DATA,
			(ksv_fifo[ksv_bytes - 1] << 16) | 0x1);
	}

	/* Now wait for HDCP_SHA_COMP_DONE */
	timeout_count = 100;
	while ((0x10 != (DSS_REG_R(io, HDMI_HDCP_SHA_STATUS)
		& 0xFFFFFF10)) && --timeout_count)
		msleep(20);
	if (!timeout_count) {
		rc = -ETIMEDOUT;
		DEV_ERR("%s: %s: SHA computation timedout", __func__,
			HDCP_STATE_NAME);
		goto error;
	}

	/* Wait for V_MATCHES */
	timeout_count = 100;
	link0_status = DSS_REG_R(io, HDMI_HDCP_LINK0_STATUS);
	while (((link0_status & BIT(20)) != BIT(20)) && --timeout_count) {
		DEV_DBG("%s: %s: Waiting for V_MATCHES(%d). l0_status=0x%08x\n",
			__func__, HDCP_STATE_NAME, timeout_count, link0_status);
		msleep(20);
		link0_status = DSS_REG_R(io, HDMI_HDCP_LINK0_STATUS);
	}
	if (!timeout_count) {
		rc = -ETIMEDOUT;
		DEV_ERR("%s: %s: HDCP V Match timedout", __func__,
			HDCP_STATE_NAME);
		goto error;
	}

error:
	if (rc)
		DEV_ERR("%s: %s: Authentication Part II failed\n", __func__,
			HDCP_STATE_NAME);
	else
		DEV_INFO("%s: %s: Authentication Part II successful\n",
			__func__, HDCP_STATE_NAME);

	/* Update topology information */
	hdcp_ctrl->current_tp.dev_count = down_stream_devices;
	hdcp_ctrl->current_tp.max_cascade_exceeded = max_cascade_exceeded;
	hdcp_ctrl->current_tp.max_dev_exceeded = max_devs_exceeded;
	hdcp_ctrl->current_tp.depth = repeater_cascade_depth;

	return rc;
} /* hdmi_hdcp_authentication_part2 */

static void hdmi_hdcp_cache_topology(struct hdmi_hdcp_ctrl *hdcp_ctrl)
{
	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	memcpy((void *)&hdcp_ctrl->cached_tp,
		(void *) &hdcp_ctrl->current_tp,
		sizeof(hdcp_ctrl->cached_tp));
}

static void hdmi_hdcp_notify_topology(struct hdmi_hdcp_ctrl *hdcp_ctrl)
{
	char a[16], b[16];
	char *envp[] = {
		[0] = "HDCP_MGR_EVENT=MSG_READY",
		[1] = a,
		[2] = b,
		NULL,
	};

	snprintf(envp[1], 16, "%d", (int)DOWN_CHECK_TOPOLOGY);
	snprintf(envp[2], 16, "%d", (int)HDCP_V1_TX);
	kobject_uevent_env(hdcp_ctrl->init_data.sysfs_kobj, KOBJ_CHANGE, envp);

	DEV_DBG("%s Event Sent: %s msgID = %s srcID = %s\n", __func__,
			envp[0], envp[1], envp[2]);
}

static void hdmi_hdcp_int_work(struct work_struct *work)
{
	struct hdmi_hdcp_ctrl *hdcp_ctrl = container_of(work,
		struct hdmi_hdcp_ctrl, hdcp_int_work);

	if (!hdcp_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	mutex_lock(hdcp_ctrl->init_data.mutex);
	hdcp_ctrl->hdcp_state = HDCP_STATE_AUTH_FAIL;
	mutex_unlock(hdcp_ctrl->init_data.mutex);

	if (hdcp_ctrl->init_data.notify_status) {
		hdcp_ctrl->init_data.notify_status(
			hdcp_ctrl->init_data.cb_data,
			hdcp_ctrl->hdcp_state);
	}
} /* hdmi_hdcp_int_work */

static void hdmi_hdcp_auth_work(struct work_struct *work)
{
	int rc;
	struct delayed_work *dw = to_delayed_work(work);
	struct hdmi_hdcp_ctrl *hdcp_ctrl = container_of(dw,
		struct hdmi_hdcp_ctrl, hdcp_auth_work);
	struct dss_io_data *io;

	if (!hdcp_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (HDCP_STATE_AUTHENTICATING != hdcp_ctrl->hdcp_state) {
		DEV_DBG("%s: %s: invalid state. returning\n", __func__,
			HDCP_STATE_NAME);
		return;
	}

	io = hdcp_ctrl->init_data.core_io;
	/* Enabling Software DDC */
	DSS_REG_W_ND(io, HDMI_DDC_ARBITRATION , DSS_REG_R(io,
				HDMI_DDC_ARBITRATION) & ~(BIT(4)));

	rc = hdmi_hdcp_authentication_part1(hdcp_ctrl);
	if (rc) {
		DEV_DBG("%s: %s: HDCP Auth Part I failed\n", __func__,
			HDCP_STATE_NAME);
		goto error;
	}

	if (hdcp_ctrl->current_tp.ds_type == DS_REPEATER) {
		rc = hdmi_hdcp_authentication_part2(hdcp_ctrl);
		if (rc) {
			DEV_DBG("%s: %s: HDCP Auth Part II failed\n", __func__,
				HDCP_STATE_NAME);
			goto error;
		}
	} else {
		DEV_INFO("%s: Downstream device is not a repeater\n", __func__);
	}
	/* Disabling software DDC before going into part3 to make sure
	 * there is no Arbitration between software and hardware for DDC */
	DSS_REG_W_ND(io, HDMI_DDC_ARBITRATION , DSS_REG_R(io,
				HDMI_DDC_ARBITRATION) | (BIT(4)));

error:
	/*
	 * Ensure that the state did not change during authentication.
	 * If it did, it means that deauthenticate/reauthenticate was
	 * called. In that case, this function need not notify HDMI Tx
	 * of the result
	 */
	mutex_lock(hdcp_ctrl->init_data.mutex);
	if (HDCP_STATE_AUTHENTICATING == hdcp_ctrl->hdcp_state) {
		if (rc) {
			hdcp_ctrl->hdcp_state = HDCP_STATE_AUTH_FAIL;
		} else {
			hdcp_ctrl->hdcp_state = HDCP_STATE_AUTHENTICATED;
			hdcp_ctrl->auth_retries = 0;
			hdmi_hdcp_cache_topology(hdcp_ctrl);
			hdmi_hdcp_notify_topology(hdcp_ctrl);
		}
		mutex_unlock(hdcp_ctrl->init_data.mutex);

		/* Notify HDMI Tx controller of the result */
		DEV_DBG("%s: %s: Notifying HDMI Tx of auth result\n",
			__func__, HDCP_STATE_NAME);
		if (hdcp_ctrl->init_data.notify_status) {
			hdcp_ctrl->init_data.notify_status(
				hdcp_ctrl->init_data.cb_data,
				hdcp_ctrl->hdcp_state);
		}
	} else {
		DEV_DBG("%s: %s: HDCP state changed during authentication\n",
			__func__, HDCP_STATE_NAME);
		mutex_unlock(hdcp_ctrl->init_data.mutex);
	}
	return;
} /* hdmi_hdcp_auth_work */

int hdmi_hdcp_authenticate(void *input)
{
	struct hdmi_hdcp_ctrl *hdcp_ctrl = (struct hdmi_hdcp_ctrl *)input;

	if (!hdcp_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (HDCP_STATE_INACTIVE != hdcp_ctrl->hdcp_state) {
		DEV_DBG("%s: %s: already active or activating. returning\n",
			__func__, HDCP_STATE_NAME);
		return 0;
	}

	DEV_DBG("%s: %s: Queuing work to start HDCP authentication", __func__,
		HDCP_STATE_NAME);
	mutex_lock(hdcp_ctrl->init_data.mutex);
	hdcp_ctrl->hdcp_state = HDCP_STATE_AUTHENTICATING;
	mutex_unlock(hdcp_ctrl->init_data.mutex);
	queue_delayed_work(hdcp_ctrl->init_data.workq,
		&hdcp_ctrl->hdcp_auth_work, 0);

	return 0;
} /* hdmi_hdcp_authenticate */

/*
 * Only retries defined times then abort current authenticating process
 * Send check_topology message to notify any hdcpmanager's client of non-
 * hdcp authenticated data link so the client can tear down any active secure
 * playback.
 * Reduce hdcp link to regular hdmi data link with hdcp disabled so any
 * un-secure like UI & menu still can be sent over HDMI and display.
 */
#define AUTH_RETRIES_TIME (30)
static int hdmi_msm_if_abort_reauth(struct hdmi_hdcp_ctrl *hdcp_ctrl)
{
	int ret = 0;

	if (!hdcp_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (++hdcp_ctrl->auth_retries == AUTH_RETRIES_TIME) {
		mutex_lock(hdcp_ctrl->init_data.mutex);
		hdcp_ctrl->hdcp_state = HDCP_STATE_INACTIVE;
		mutex_unlock(hdcp_ctrl->init_data.mutex);

		hdcp_ctrl->auth_retries = 0;
		ret = -ERANGE;
	}

	return ret;
}

int hdmi_hdcp_reauthenticate(void *input)
{
	struct hdmi_hdcp_ctrl *hdcp_ctrl = (struct hdmi_hdcp_ctrl *)input;
	struct dss_io_data *io;
	u32 ret = 0;

	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	io = hdcp_ctrl->init_data.core_io;

	if (HDCP_STATE_AUTH_FAIL != hdcp_ctrl->hdcp_state) {
		DEV_DBG("%s: %s: invalid state. returning\n", __func__,
			HDCP_STATE_NAME);
		return 0;
	}

	/*
	 * Disable HPD circuitry.
	 * This is needed to reset the HDCP cipher engine so that when we
	 * attempt a re-authentication, HW would clear the AN0_READY and
	 * AN1_READY bits in HDMI_HDCP_LINK0_STATUS register
	 */
	DSS_REG_W(io, HDMI_HPD_CTRL, DSS_REG_R(hdcp_ctrl->init_data.core_io,
		HDMI_HPD_CTRL) & ~BIT(28));

	/* Disable HDCP interrupts */
	DSS_REG_W(io, HDMI_HDCP_INT_CTRL, 0);

	DSS_REG_W(io, HDMI_HDCP_RESET, BIT(0));

	/* Wait to be clean on DDC HW engine */
	hdmi_hdcp_hw_ddc_clean(hdcp_ctrl);

	/* Disable encryption and disable the HDCP block */
	DSS_REG_W(io, HDMI_HDCP_CTRL, 0);

	/* Enable HPD circuitry */
	DSS_REG_W(hdcp_ctrl->init_data.core_io, HDMI_HPD_CTRL,
		DSS_REG_R(hdcp_ctrl->init_data.core_io,
		HDMI_HPD_CTRL) | BIT(28));

	ret = hdmi_msm_if_abort_reauth(hdcp_ctrl);

	if (ret) {
		DEV_ERR("%s: abort reauthentication!\n", __func__);
		return ret;
	}

	/* Restart authentication attempt */
	DEV_DBG("%s: %s: Scheduling work to start HDCP authentication",
		__func__, HDCP_STATE_NAME);
	mutex_lock(hdcp_ctrl->init_data.mutex);
	hdcp_ctrl->hdcp_state = HDCP_STATE_AUTHENTICATING;
	mutex_unlock(hdcp_ctrl->init_data.mutex);
	queue_delayed_work(hdcp_ctrl->init_data.workq,
		&hdcp_ctrl->hdcp_auth_work, HZ/2);

	return ret;
} /* hdmi_hdcp_reauthenticate */

void hdmi_hdcp_off(void *input)
{
	struct hdmi_hdcp_ctrl *hdcp_ctrl = (struct hdmi_hdcp_ctrl *)input;
	struct dss_io_data *io;
	int rc = 0;

	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = hdcp_ctrl->init_data.core_io;

	if (HDCP_STATE_INACTIVE == hdcp_ctrl->hdcp_state) {
		DEV_DBG("%s: %s: inactive. returning\n", __func__,
			HDCP_STATE_NAME);
		return;
	}

	/*
	 * Disable HDCP interrupts.
	 * Also, need to set the state to inactive here so that any ongoing
	 * reauth works will know that the HDCP session has been turned off.
	 */
	mutex_lock(hdcp_ctrl->init_data.mutex);
	DSS_REG_W(io, HDMI_HDCP_INT_CTRL, 0);
	hdcp_ctrl->hdcp_state = HDCP_STATE_INACTIVE;
	mutex_unlock(hdcp_ctrl->init_data.mutex);

	/*
	 * Cancel any pending auth/reauth attempts.
	 * If one is ongoing, this will wait for it to finish.
	 * No more reauthentiaction attempts will be scheduled since we
	 * set the currect state to inactive.
	 */
	rc = cancel_delayed_work_sync(&hdcp_ctrl->hdcp_auth_work);
	if (rc)
		DEV_DBG("%s: %s: Deleted hdcp auth work\n", __func__,
			HDCP_STATE_NAME);
	rc = cancel_work_sync(&hdcp_ctrl->hdcp_int_work);
	if (rc)
		DEV_DBG("%s: %s: Deleted hdcp int work\n", __func__,
			HDCP_STATE_NAME);

	DSS_REG_W(io, HDMI_HDCP_RESET, BIT(0));

	/* Disable encryption and disable the HDCP block */
	DSS_REG_W(io, HDMI_HDCP_CTRL, 0);

	DEV_DBG("%s: %s: HDCP: Off\n", __func__, HDCP_STATE_NAME);
} /* hdmi_hdcp_off */

int hdmi_hdcp_isr(void *input)
{
	struct hdmi_hdcp_ctrl *hdcp_ctrl = (struct hdmi_hdcp_ctrl *)input;
	int rc = 0;
	struct dss_io_data *io;
	u32 hdcp_int_val;

	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	io = hdcp_ctrl->init_data.core_io;

	hdcp_int_val = DSS_REG_R(io, HDMI_HDCP_INT_CTRL);

	/* Ignore HDCP interrupts if HDCP is disabled */
	if (HDCP_STATE_INACTIVE == hdcp_ctrl->hdcp_state) {
		DEV_DBG("%s: HDCP inactive. Just clear int and return.\n",
			__func__);
		DSS_REG_W(io, HDMI_HDCP_INT_CTRL, HDCP_INT_CLR);
		return 0;
	}

	if (hdcp_int_val & BIT(0)) {
		/* AUTH_SUCCESS_INT */
		DSS_REG_W(io, HDMI_HDCP_INT_CTRL, (hdcp_int_val | BIT(1)));
		DEV_INFO("%s: %s: AUTH_SUCCESS_INT received\n", __func__,
			HDCP_STATE_NAME);
		if (HDCP_STATE_AUTHENTICATING == hdcp_ctrl->hdcp_state)
			complete_all(&hdcp_ctrl->r0_checked);
	}

	if (hdcp_int_val & BIT(4)) {
		/* AUTH_FAIL_INT */
		u32 link_status = DSS_REG_R(io, HDMI_HDCP_LINK0_STATUS);
		DSS_REG_W(io, HDMI_HDCP_INT_CTRL, (hdcp_int_val | BIT(5)));
		DEV_INFO("%s: %s: AUTH_FAIL_INT rcvd, LINK0_STATUS=0x%08x\n",
			__func__, HDCP_STATE_NAME, link_status);
		if (HDCP_STATE_AUTHENTICATED == hdcp_ctrl->hdcp_state) {
			/* Inform HDMI Tx of the failure */
			queue_work(hdcp_ctrl->init_data.workq,
				&hdcp_ctrl->hdcp_int_work);
			/* todo: print debug log with auth fail reason */
		} else if (HDCP_STATE_AUTHENTICATING == hdcp_ctrl->hdcp_state) {
			complete_all(&hdcp_ctrl->r0_checked);
		}

		/* Clear AUTH_FAIL_INFO as well */
		DSS_REG_W(io, HDMI_HDCP_INT_CTRL, (hdcp_int_val | BIT(7)));
	}

	if (hdcp_int_val & BIT(8)) {
		/* DDC_XFER_REQ_INT */
		DSS_REG_W(io, HDMI_HDCP_INT_CTRL, (hdcp_int_val | BIT(9)));
		DEV_INFO("%s: %s: DDC_XFER_REQ_INT received\n", __func__,
			HDCP_STATE_NAME);
	}

	if (hdcp_int_val & BIT(12)) {
		/* DDC_XFER_DONE_INT */
		DSS_REG_W(io, HDMI_HDCP_INT_CTRL, (hdcp_int_val | BIT(13)));
		DEV_INFO("%s: %s: DDC_XFER_DONE received\n", __func__,
			HDCP_STATE_NAME);
	}

error:
	return rc;
} /* hdmi_hdcp_isr */

static ssize_t hdmi_hdcp_sysfs_rda_status(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_hdcp_ctrl *hdcp_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP);

	if (!hdcp_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	mutex_lock(hdcp_ctrl->init_data.mutex);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", hdcp_ctrl->hdcp_state);
	DEV_DBG("%s: '%d'\n", __func__, hdcp_ctrl->hdcp_state);
	mutex_unlock(hdcp_ctrl->init_data.mutex);

	return ret;
} /* hdmi_hdcp_sysfs_rda_hdcp*/

static ssize_t hdmi_hdcp_sysfs_rda_tp(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct hdmi_hdcp_ctrl *hdcp_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP);

	if (!hdcp_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	switch (hdcp_ctrl->tp_msgid) {
	case DOWN_CHECK_TOPOLOGY:
	case DOWN_REQUEST_TOPOLOGY:
		buf[MSG_ID_IDX]   = hdcp_ctrl->tp_msgid;
		buf[RET_CODE_IDX] = HDCP_AUTHED;
		ret = HEADER_LEN;

		memcpy(buf + HEADER_LEN, &hdcp_ctrl->cached_tp,
			sizeof(struct HDCP_V2V1_MSG_TOPOLOGY));

		ret += sizeof(struct HDCP_V2V1_MSG_TOPOLOGY);

		/* clear the flag once data is read back to user space*/
		hdcp_ctrl->tp_msgid = -1;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
} /* hdmi_hdcp_sysfs_rda_tp*/

static ssize_t hdmi_hdcp_sysfs_wta_tp(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int msgid = 0;
	ssize_t ret = count;
	struct hdmi_hdcp_ctrl *hdcp_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP);

	if (!hdcp_ctrl || !buf) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	msgid = buf[0];

	switch (msgid) {
	case DOWN_CHECK_TOPOLOGY:
	case DOWN_REQUEST_TOPOLOGY:
		hdcp_ctrl->tp_msgid = msgid;
		break;
	/* more cases added here */
	default:
		ret = -EINVAL;
	}

	return ret;
} /* hdmi_tx_sysfs_wta_hpd */

static DEVICE_ATTR(status, S_IRUGO, hdmi_hdcp_sysfs_rda_status, NULL);
static DEVICE_ATTR(tp, S_IRUGO | S_IWUSR, hdmi_hdcp_sysfs_rda_tp,
	hdmi_hdcp_sysfs_wta_tp);


static struct attribute *hdmi_hdcp_fs_attrs[] = {
	&dev_attr_status.attr,
	&dev_attr_tp.attr,
	NULL,
};

static struct attribute_group hdmi_hdcp_fs_attr_group = {
	.name = "hdcp",
	.attrs = hdmi_hdcp_fs_attrs,
};

void hdmi_hdcp_deinit(void *input)
{
	struct hdmi_hdcp_ctrl *hdcp_ctrl = (struct hdmi_hdcp_ctrl *)input;

	if (!hdcp_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	sysfs_remove_group(hdcp_ctrl->init_data.sysfs_kobj,
				&hdmi_hdcp_fs_attr_group);

	kfree(hdcp_ctrl);
} /* hdmi_hdcp_deinit */

void *hdmi_hdcp_init(struct hdmi_hdcp_init_data *init_data)
{
	struct hdmi_hdcp_ctrl *hdcp_ctrl = NULL;
	u32 scm_buf = TZ_HDCP_CMD_ID;
	u32 ret  = 0;
	u32 resp = 0;

	if (!init_data || !init_data->core_io || !init_data->qfprom_io ||
		!init_data->mutex || !init_data->ddc_ctrl ||
		!init_data->notify_status || !init_data->workq ||
		!init_data->cb_data) {
		DEV_ERR("%s: invalid input\n", __func__);
		goto error;
	}

	hdcp_ctrl = kzalloc(sizeof(*hdcp_ctrl), GFP_KERNEL);
	if (!hdcp_ctrl) {
		DEV_ERR("%s: Out of memory\n", __func__);
		goto error;
	}

	hdcp_ctrl->init_data = *init_data;

	if (sysfs_create_group(init_data->sysfs_kobj,
				&hdmi_hdcp_fs_attr_group)) {
		DEV_ERR("%s: hdcp sysfs group creation failed\n", __func__);
		goto error;
	}

	INIT_DELAYED_WORK(&hdcp_ctrl->hdcp_auth_work, hdmi_hdcp_auth_work);
	INIT_WORK(&hdcp_ctrl->hdcp_int_work, hdmi_hdcp_int_work);

	hdcp_ctrl->hdcp_state = HDCP_STATE_INACTIVE;
	init_completion(&hdcp_ctrl->r0_checked);

	ret = scm_call(SCM_SVC_INFO, SCM_CMD_HDCP, (void *) &scm_buf,
		sizeof(scm_buf), (void *) &resp, sizeof(resp));

	if (ret) {
		DEV_ERR("%s: error: scm_call ret = %d, resp = %d\n",
			__func__, ret, resp);
	} else {
		DEV_DBG("%s: tz_hdcp = %d\n", __func__, resp);
		hdcp_ctrl->tz_hdcp = resp;
	}

	DEV_DBG("%s: HDCP module initialized. HDCP_STATE=%s", __func__,
		HDCP_STATE_NAME);

error:
	return (void *)hdcp_ctrl;
} /* hdmi_hdcp_init */
