/* Copyright (c) 2010-2016 The Linux Foundation. All rights reserved.
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
#include <linux/iopoll.h>
#include <soc/qcom/scm.h>
#include <linux/hdcp_qseecom.h>
#include "mdss_hdcp.h"
#include "mdss_fb.h"
#include "mdss_dp_util.h"
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

#define HDCP_INT_CLR (isr->auth_success_ack | isr->auth_fail_ack | \
			isr->auth_fail_info_ack | isr->tx_req_ack | \
			isr->encryption_ready_ack | \
			isr->encryption_not_ready_ack | isr->tx_req_done_ack)

#define HDCP_INT_EN (isr->auth_success_mask | isr->auth_fail_mask | \
			isr->encryption_ready_mask | \
			isr->encryption_not_ready_mask)

#define HDCP_POLL_SLEEP_US   (20 * 1000)
#define HDCP_POLL_TIMEOUT_US (HDCP_POLL_SLEEP_US * 100)

#define reg_set_data(x) \
	(hdcp_ctrl->init_data.sec_access ? reg_set->sec_data##x : \
		reg_set->data##x)

struct hdcp_sink_addr {
	char *name;
	u32 addr;
	u32 len;
};

struct hdcp_1x_reg_data {
	u32 reg_id;
	struct hdcp_sink_addr *sink;
};

struct hdcp_sink_addr_map {
	/* addresses to read from sink */
	struct hdcp_sink_addr bcaps;
	struct hdcp_sink_addr bksv;
	struct hdcp_sink_addr r0;
	struct hdcp_sink_addr bstatus;
	struct hdcp_sink_addr ksv_fifo;
	struct hdcp_sink_addr v_h0;
	struct hdcp_sink_addr v_h1;
	struct hdcp_sink_addr v_h2;
	struct hdcp_sink_addr v_h3;
	struct hdcp_sink_addr v_h4;

	/* addresses to write to sink */
	struct hdcp_sink_addr an;
	struct hdcp_sink_addr aksv;
	struct hdcp_sink_addr rep;
};

struct hdcp_int_set {
	/* interrupt register */
	u32 int_reg;

	/* interrupt enable/disable masks */
	u32 auth_success_mask;
	u32 auth_fail_mask;
	u32 encryption_ready_mask;
	u32 encryption_not_ready_mask;
	u32 tx_req_mask;
	u32 tx_req_done_mask;

	/* interrupt acknowledgment */
	u32 auth_success_ack;
	u32 auth_fail_ack;
	u32 auth_fail_info_ack;
	u32 encryption_ready_ack;
	u32 encryption_not_ready_ack;
	u32 tx_req_ack;
	u32 tx_req_done_ack;

	/* interrupt status */
	u32 auth_success_int;
	u32 auth_fail_int;
	u32 encryption_ready;
	u32 encryption_not_ready;
	u32 tx_req_int;
	u32 tx_req_done_int;
};

struct hdcp_reg_set {
	u32 status;
	u32 keys_offset;
	u32 r0_offset;
	u32 v_offset;
	u32 ctrl;
	u32 aksv_lsb;
	u32 aksv_msb;
	u32 entropy_ctrl0;
	u32 entropy_ctrl1;
	u32 sha_ctrl;
	u32 sec_sha_ctrl;
	u32 sha_status;

	u32 data0;
	u32 data1;
	u32 data2_0;
	u32 data3;
	u32 data4;
	u32 data5;
	u32 data6;
	u32 data7;
	u32 data8;
	u32 data9;
	u32 data10;
	u32 data11;
	u32 data12;

	u32 sec_data0;
	u32 sec_data1;
	u32 sec_data7;
	u32 sec_data8;
	u32 sec_data9;
	u32 sec_data10;
	u32 sec_data11;
	u32 sec_data12;

	u32 reset;
};

#define HDCP_REG_SET_CLIENT_HDMI \
	{HDMI_HDCP_LINK0_STATUS, 28, 24, 20, HDMI_HDCP_CTRL, \
	 HDMI_HDCP_SW_LOWER_AKSV, HDMI_HDCP_SW_UPPER_AKSV, \
	 HDMI_HDCP_ENTROPY_CTRL0, HDMI_HDCP_ENTROPY_CTRL1, \
	 HDMI_HDCP_SHA_CTRL, HDCP_SEC_TZ_HV_HLOS_HDCP_SHA_CTRL, \
	 HDMI_HDCP_SHA_STATUS, HDMI_HDCP_RCVPORT_DATA0, \
	 HDMI_HDCP_RCVPORT_DATA1, HDMI_HDCP_RCVPORT_DATA2_0, \
	 HDMI_HDCP_RCVPORT_DATA3, HDMI_HDCP_RCVPORT_DATA4, \
	 HDMI_HDCP_RCVPORT_DATA5, HDMI_HDCP_RCVPORT_DATA6, \
	 HDMI_HDCP_RCVPORT_DATA7, HDMI_HDCP_RCVPORT_DATA8, \
	 HDMI_HDCP_RCVPORT_DATA9, HDMI_HDCP_RCVPORT_DATA10, \
	 HDMI_HDCP_RCVPORT_DATA11, HDMI_HDCP_RCVPORT_DATA12, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA0, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA1, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA7, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA8, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA9, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA10, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA11, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA12, \
	 HDMI_HDCP_RESET}

#define HDCP_REG_SET_CLIENT_DP \
	{DP_HDCP_STATUS, 16, 14, 13, DP_HDCP_CTRL, \
	 DP_HDCP_SW_LOWER_AKSV, DP_HDCP_SW_UPPER_AKSV, \
	 DP_HDCP_ENTROPY_CTRL0, DP_HDCP_ENTROPY_CTRL1, \
	 0, HDCP_SEC_DP_TZ_HV_HLOS_HDCP_SHA_CTRL, \
	 DP_HDCP_SHA_STATUS, 0, 0, DP_HDCP_RCVPORT_DATA2_0, \
	 DP_HDCP_RCVPORT_DATA3, DP_HDCP_RCVPORT_DATA4, \
	 DP_HDCP_RCVPORT_DATA5, DP_HDCP_RCVPORT_DATA6, \
	 0, 0, 0, 0, 0, 0, \
	 HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA0, \
	 HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA1, \
	 HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA7, \
	 HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA8, \
	 HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA9, \
	 HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA10, \
	 HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA11, \
	 HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA12, 0}

#define HDCP_HDMI_SINK_ADDR_MAP \
	{{"bcaps", 0x40, 1}, {"bksv", 0x00, 5}, {"r0'", 0x08, 2}, \
	 {"bstatus", 0x41, 2}, {"ksv-fifo", 0x43, 0}, {"v_h0", 0x20, 4}, \
	 {"v_h1", 0x24, 4}, {"v_h2", 0x28, 4}, {"v_h3", 0x2c, 4}, \
	 {"v_h4", 0x30, 4}, {"an", 0x18, 8}, {"aksv", 0x10, 5}, \
	 {"repeater", 0x00, 0} }

#define HDCP_DP_SINK_ADDR_MAP \
	{{"bcaps", 0x68028, 1}, {"bksv", 0x68000, 5}, {"r0'", 0x68005, 2}, \
	 {"bstatus", 0x6802A, 2}, {"ksv-fifo", 0x6802A, 0}, \
	 {"v_h0", 0x68014, 4}, {"v_h1", 0x68018, 4}, {"v_h2", 0x6801C, 4}, \
	 {"v_h3", 0x68020, 4}, {"v_h4", 0x68024, 4}, {"an", 0x6800C, 8}, \
	 {"aksv", 0x68007, 5}, {"repeater", 0x68028, 1} }

#define HDCP_HDMI_INT_SET \
	{HDMI_HDCP_INT_CTRL, \
	 BIT(2), BIT(6), 0, 0, 0, 0, \
	 BIT(1), BIT(5), BIT(7), 0, 0, 0, 0, \
	 BIT(0), BIT(4), 0, 0, 0, 0}

#define HDCP_DP_INT_SET \
	{DP_INTR_STATUS2, \
	 BIT(17), BIT(20), BIT(24), BIT(27), 0, 0, \
	 BIT(16), BIT(19), BIT(21), BIT(23), BIT(26), 0, 0, \
	 BIT(15), BIT(18), BIT(22), BIT(25), 0, 0}

struct hdcp_1x_ctrl {
	u32 auth_retries;
	u32 tp_msgid;
	u32 tz_hdcp;
	enum hdcp_states hdcp_state;
	struct HDCP_V2V1_MSG_TOPOLOGY cached_tp;
	struct HDCP_V2V1_MSG_TOPOLOGY current_tp;
	struct delayed_work hdcp_auth_work;
	struct work_struct hdcp_int_work;
	struct completion r0_checked;
	struct hdcp_init_data init_data;
	struct hdcp_ops *ops;
	struct hdcp_reg_set reg_set;
	struct hdcp_int_set int_set;
	struct hdcp_sink_addr_map sink_addr;
};

const char *hdcp_state_name(enum hdcp_states hdcp_state)
{
	switch (hdcp_state) {
	case HDCP_STATE_INACTIVE:	return "HDCP_STATE_INACTIVE";
	case HDCP_STATE_AUTHENTICATING:	return "HDCP_STATE_AUTHENTICATING";
	case HDCP_STATE_AUTHENTICATED:	return "HDCP_STATE_AUTHENTICATED";
	case HDCP_STATE_AUTH_FAIL:	return "HDCP_STATE_AUTH_FAIL";
	default:			return "???";
	}
} /* hdcp_state_name */

static int hdcp_1x_count_one(u8 *array, u8 len)
{
	int i, j, count = 0;
	for (i = 0; i < len; i++)
		for (j = 0; j < 8; j++)
			count += (((array[i] >> j) & 0x1) ? 1 : 0);
	return count;
} /* hdcp_1x_count_one */

static void reset_hdcp_ddc_failures(struct hdcp_1x_ctrl *hdcp_ctrl)
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

static void hdcp_1x_hw_ddc_clean(struct hdcp_1x_ctrl *hdcp_ctrl)
{
	struct dss_io_data *io = NULL;
	u32 hdcp_ddc_status, ddc_hw_status;
	u32 ddc_xfer_done, ddc_xfer_req;
	u32 ddc_hw_req, ddc_hw_not_idle;
	bool ddc_hw_not_ready, xfer_not_done, hw_not_done;
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

	/* Wait to be clean on DDC HW engine */
	timeout_count = 100;
	do {
		hdcp_ddc_status = DSS_REG_R(io, HDMI_HDCP_DDC_STATUS);
		ddc_xfer_req    = hdcp_ddc_status & BIT(4);
		ddc_xfer_done   = hdcp_ddc_status & BIT(10);

		ddc_hw_status   = DSS_REG_R(io, HDMI_DDC_HW_STATUS);
		ddc_hw_req      = ddc_hw_status & BIT(16);
		ddc_hw_not_idle = ddc_hw_status & (BIT(0) | BIT(1));

		/* ddc transfer was requested but not completed */
		xfer_not_done = ddc_xfer_req && !ddc_xfer_done;

		/* ddc status is not idle or a hw request pending */
		hw_not_done = ddc_hw_not_idle || ddc_hw_req;

		ddc_hw_not_ready = xfer_not_done || hw_not_done;

		DEV_DBG("%s: %s: timeout count(%d): ddc hw%sready\n",
			__func__, HDCP_STATE_NAME, timeout_count,
				ddc_hw_not_ready ? " not " : " ");
		DEV_DBG("hdcp_ddc_status[0x%x], ddc_hw_status[0x%x]\n",
				hdcp_ddc_status, ddc_hw_status);
		if (ddc_hw_not_ready)
			msleep(20);
		} while (ddc_hw_not_ready && --timeout_count);
} /* hdcp_1x_hw_ddc_clean */

static int hdcp_scm_call(struct scm_hdcp_req *req, u32 *resp)
{
	int ret = 0;

	if (!is_scm_armv8()) {
		ret = scm_call(SCM_SVC_HDCP, SCM_CMD_HDCP, (void *) req,
			     SCM_HDCP_MAX_REG * sizeof(struct scm_hdcp_req),
			     &resp, sizeof(*resp));
	} else {
		struct scm_desc desc;

		desc.args[0] = req[0].addr;
		desc.args[1] = req[0].val;
		desc.args[2] = req[1].addr;
		desc.args[3] = req[1].val;
		desc.args[4] = req[2].addr;
		desc.args[5] = req[2].val;
		desc.args[6] = req[3].addr;
		desc.args[7] = req[3].val;
		desc.args[8] = req[4].addr;
		desc.args[9] = req[4].val;
		desc.arginfo = SCM_ARGS(10);

		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_HDCP, SCM_CMD_HDCP),
				&desc);
		*resp = desc.ret[0];
		if (ret)
			return ret;
	}

	return ret;
}

static int hdcp_1x_load_keys(void *input)
{
	int rc = 0;
	bool use_sw_keys = false;
	u32 reg_val;
	u32 ksv_lsb_addr, ksv_msb_addr;
	u32 aksv_lsb, aksv_msb;
	u8 aksv[5];
	struct dss_io_data *io;
	struct dss_io_data *qfprom_io;
	struct hdcp_1x_ctrl *hdcp_ctrl = input;
	struct hdcp_reg_set *reg_set;

	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io ||
		!hdcp_ctrl->init_data.qfprom_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	if ((HDCP_STATE_INACTIVE != hdcp_ctrl->hdcp_state) &&
		(HDCP_STATE_AUTH_FAIL != hdcp_ctrl->hdcp_state)) {
		DEV_ERR("%s: %s: invalid state. returning\n", __func__,
			HDCP_STATE_NAME);
		rc = -EINVAL;
		goto end;
	}

	io = hdcp_ctrl->init_data.core_io;
	qfprom_io = hdcp_ctrl->init_data.qfprom_io;
	reg_set = &hdcp_ctrl->reg_set;

	/* On compatible hardware, use SW keys */
	reg_val = DSS_REG_R(qfprom_io, SEC_CTRL_HW_VERSION);
	if (reg_val >= HDCP_SEL_MIN_SEC_VERSION) {
		reg_val = DSS_REG_R(qfprom_io,
			QFPROM_RAW_FEAT_CONFIG_ROW0_MSB +
			QFPROM_RAW_VERSION_4);

		if (!(reg_val & BIT(23)))
			use_sw_keys = true;
	}

	if (use_sw_keys) {
		if (hdcp1_set_keys(&aksv_msb, &aksv_lsb)) {
			pr_err("%s: setting hdcp SW keys failed\n", __func__);
			rc = -EINVAL;
			goto end;
		}
	} else {
		/* Fetch aksv from QFPROM, this info should be public. */
		ksv_lsb_addr = HDCP_KSV_LSB;
		ksv_msb_addr = HDCP_KSV_MSB;

		if (hdcp_ctrl->init_data.sec_access) {
			ksv_lsb_addr += HDCP_KSV_VERSION_4_OFFSET;
			ksv_msb_addr += HDCP_KSV_VERSION_4_OFFSET;
		}

		aksv_lsb = DSS_REG_R(qfprom_io, ksv_lsb_addr);
		aksv_msb = DSS_REG_R(qfprom_io, ksv_msb_addr);
	}

	DEV_DBG("%s: %s: AKSV=%02x%08x\n", __func__, HDCP_STATE_NAME,
		aksv_msb, aksv_lsb);

	aksv[0] =  aksv_lsb        & 0xFF;
	aksv[1] = (aksv_lsb >> 8)  & 0xFF;
	aksv[2] = (aksv_lsb >> 16) & 0xFF;
	aksv[3] = (aksv_lsb >> 24) & 0xFF;
	aksv[4] =  aksv_msb        & 0xFF;

	/* check there are 20 ones in AKSV */
	if (hdcp_1x_count_one(aksv, 5) != 20) {
		DEV_ERR("%s: AKSV bit count failed\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	DSS_REG_W(io, reg_set->aksv_lsb, aksv_lsb);
	DSS_REG_W(io, reg_set->aksv_msb, aksv_msb);

	/* Setup seed values for random number An */
	DSS_REG_W(io, reg_set->entropy_ctrl0, 0xB1FFB0FF);
	DSS_REG_W(io, reg_set->entropy_ctrl1, 0xF00DFACE);

	/* make sure hw is programmed */
	wmb();

	/* enable hdcp engine */
	DSS_REG_W(io, reg_set->ctrl, 0x1);

	hdcp_ctrl->hdcp_state = HDCP_STATE_AUTHENTICATING;
end:
	return rc;
}

static int hdcp_1x_read(struct hdcp_1x_ctrl *hdcp_ctrl,
			  struct hdcp_sink_addr *sink,
			  u8 *buf, bool realign)
{
	u32 rc = 0;
	struct hdmi_tx_ddc_data ddc_data;

	if (hdcp_ctrl->init_data.client_id == HDCP_CLIENT_HDMI) {
		reset_hdcp_ddc_failures(hdcp_ctrl);

		memset(&ddc_data, 0, sizeof(ddc_data));
		ddc_data.dev_addr = 0x74;
		ddc_data.offset = sink->addr;
		ddc_data.data_buf = buf;
		ddc_data.data_len = sink->len;
		ddc_data.request_len = sink->len;
		ddc_data.retry = 5;
		ddc_data.what = sink->name;
		ddc_data.retry_align = realign;

		hdcp_ctrl->init_data.ddc_ctrl->ddc_data = ddc_data;

		rc = hdmi_ddc_read(hdcp_ctrl->init_data.ddc_ctrl);
		if (rc)
			DEV_ERR("%s: %s: %s read failed\n", __func__,
				HDCP_STATE_NAME, sink->name);
	} else if (IS_ENABLED(CONFIG_FB_MSM_MDSS_DP_PANEL) &&
		hdcp_ctrl->init_data.client_id == HDCP_CLIENT_DP) {
		struct edp_cmd cmd = {0};

		cmd.read = 1;
		cmd.addr = sink->addr;
		cmd.out_buf = buf;
		cmd.len = sink->len;

		rc = dp_aux_read(hdcp_ctrl->init_data.cb_data, &cmd);
		if (rc)
			DEV_ERR("%s: %s: %s read failed\n", __func__,
				HDCP_STATE_NAME, sink->name);
	}

	return rc;
}

static int hdcp_1x_write(struct hdcp_1x_ctrl *hdcp_ctrl,
			   struct hdcp_sink_addr *sink, u8 *buf)
{
	int rc = 0;
	struct hdmi_tx_ddc_data ddc_data;

	if (hdcp_ctrl->init_data.client_id == HDCP_CLIENT_HDMI) {
		memset(&ddc_data, 0, sizeof(ddc_data));

		ddc_data.dev_addr = 0x74;
		ddc_data.offset = sink->addr;
		ddc_data.data_buf = buf;
		ddc_data.data_len = sink->len;
		ddc_data.what = sink->name;
		hdcp_ctrl->init_data.ddc_ctrl->ddc_data = ddc_data;

		rc = hdmi_ddc_write(hdcp_ctrl->init_data.ddc_ctrl);
		if (rc)
			DEV_ERR("%s: %s: %s write failed\n", __func__,
				HDCP_STATE_NAME, sink->name);
	} else if (IS_ENABLED(CONFIG_FB_MSM_MDSS_DP_PANEL) &&
		hdcp_ctrl->init_data.client_id == HDCP_CLIENT_DP) {
		struct edp_cmd cmd = {0};

		cmd.addr = sink->addr;
		cmd.len = sink->len;
		cmd.datap = buf;

		rc = dp_aux_write(hdcp_ctrl->init_data.cb_data, &cmd);
		if (rc)
			DEV_ERR("%s: %s: %s read failed\n", __func__,
				HDCP_STATE_NAME, sink->name);
	}

	return rc;
}

static void hdcp_1x_enable_interrupts(struct hdcp_1x_ctrl *hdcp_ctrl)
{
	u32 intr_reg;
	struct dss_io_data *io;
	struct hdcp_int_set *isr;

	io = hdcp_ctrl->init_data.core_io;
	isr = &hdcp_ctrl->int_set;

	intr_reg = DSS_REG_R(io, isr->int_reg);

	intr_reg |= HDCP_INT_CLR | HDCP_INT_EN;

	DSS_REG_W(io, isr->int_reg, intr_reg);
}

static int hdcp_1x_authentication_part1(struct hdcp_1x_ctrl *hdcp_ctrl)
{
	int rc;
	u32 link0_aksv_0, link0_aksv_1;
	u32 link0_bksv_0, link0_bksv_1;
	u32 link0_an_0, link0_an_1;
	u32 timeout_count;
	bool is_match;
	struct dss_io_data *io;
	struct dss_io_data *hdcp_io;
	struct hdcp_reg_set *reg_set;
	u8 aksv[5], *bksv = NULL;
	u8 an[8];
	u8 bcaps = 0;
	u32 link0_status;
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
	bksv = hdcp_ctrl->current_tp.bksv;
	io = hdcp_ctrl->init_data.core_io;
	hdcp_io = hdcp_ctrl->init_data.hdcp_io;
	reg_set = &hdcp_ctrl->reg_set;

	if (HDCP_STATE_AUTHENTICATING != hdcp_ctrl->hdcp_state) {
		DEV_ERR("%s: %s: invalid state. returning\n", __func__,
			HDCP_STATE_NAME);
		rc = -EINVAL;
		goto error;
	}

	rc = hdcp_1x_read(hdcp_ctrl, &hdcp_ctrl->sink_addr.bcaps,
		&bcaps, false);
	if (IS_ERR_VALUE(rc)) {
		DEV_ERR("%s: error reading bcaps\n", __func__);
		goto error;
	}

	hdcp_1x_enable_interrupts(hdcp_ctrl);

	/* receiver (0), repeater (1) */
	hdcp_ctrl->current_tp.ds_type =
		(bcaps & BIT(6)) >> 6 ? DS_REPEATER : DS_RECEIVER;

	/* Write BCAPS to the hardware */
	if (hdcp_ctrl->tz_hdcp) {
		memset(scm_buf, 0x00, sizeof(scm_buf));

		scm_buf[0].addr = phy_addr + reg_set->data12;
		scm_buf[0].val  = bcaps;

		ret = hdcp_scm_call(scm_buf, &resp);
		if (ret || resp) {
			DEV_ERR("%s: error: scm_call ret = %d, resp = %d\n",
				__func__, ret, resp);
			rc = -EINVAL;
			goto error;
		}
	} else if (hdcp_ctrl->init_data.sec_access) {
		DSS_REG_W(hdcp_io, reg_set->sec_data12, bcaps);
	} else {
		DSS_REG_W(io, reg_set->data12, bcaps);
	}

	/* Wait for HDCP keys to be checked and validated */
	rc = readl_poll_timeout(io->base + reg_set->status, link0_status,
				((link0_status >> reg_set->keys_offset) & 0x7)
					== HDCP_KEYS_STATE_VALID,
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (IS_ERR_VALUE(rc)) {
		DEV_ERR("%s: key not ready\n", __func__);
		goto error;
	}

	/*
	 * 1.1_Features turned off by default.
	 * No need to write AInfo since 1.1_Features is disabled.
	 */
	DSS_REG_W(io, reg_set->data4, 0);

	/* Wait for An0 and An1 bit to be ready */
	rc = readl_poll_timeout(io->base + reg_set->status, link0_status,
				(link0_status & (BIT(8) | BIT(9))),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (IS_ERR_VALUE(rc)) {
		DEV_ERR("%s: An not ready\n", __func__);
		goto error;
	}

	/* As per hardware recommendations, wait before reading An */
	msleep(20);

	/*
	 * As per hardware recommendation, for DP, read AN0 and AN1 again
	 * with a delay of 1 micro second each.
	 */
	link0_an_0 = DSS_REG_R(io, reg_set->data5);
	if (hdcp_ctrl->init_data.client_id == HDCP_CLIENT_DP) {
		udelay(1);
		link0_an_0 = DSS_REG_R(io, reg_set->data5);
	}

	link0_an_1 = DSS_REG_R(io, reg_set->data6);
	if (hdcp_ctrl->init_data.client_id == HDCP_CLIENT_DP) {
		udelay(1);
		link0_an_1 = DSS_REG_R(io, reg_set->data6);
	}

	/* Read AKSV */
	link0_aksv_0 = DSS_REG_R(io, reg_set->data3);
	link0_aksv_1 = DSS_REG_R(io, reg_set->data4);

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

	rc = hdcp_1x_write(hdcp_ctrl, &hdcp_ctrl->sink_addr.an, an);
	if (IS_ERR_VALUE(rc)) {
		DEV_ERR("%s: error writing an to sink\n", __func__);
		goto error;
	}

	rc = hdcp_1x_write(hdcp_ctrl, &hdcp_ctrl->sink_addr.aksv, aksv);
	if (IS_ERR_VALUE(rc)) {
		DEV_ERR("%s: error writing aksv to sink\n", __func__);
		goto error;
	}

	rc = hdcp_1x_read(hdcp_ctrl, &hdcp_ctrl->sink_addr.bksv, bksv, false);
	if (IS_ERR_VALUE(rc)) {
		DEV_ERR("%s: error reading bksv from sink\n", __func__);
		goto error;
	}

	/* check there are 20 ones in BKSV */
	if (hdcp_1x_count_one(bksv, 5) != 20) {
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

	if (hdcp_ctrl->tz_hdcp) {
		memset(scm_buf, 0x00, sizeof(scm_buf));

		scm_buf[0].addr = phy_addr + reg_set->data0;
		scm_buf[0].val  = link0_bksv_0;
		scm_buf[1].addr = phy_addr + reg_set->data1;
		scm_buf[1].val  = link0_bksv_1;

		ret = hdcp_scm_call(scm_buf, &resp);

		if (ret || resp) {
			DEV_ERR("%s: error: scm_call ret = %d, resp = %d\n",
				__func__, ret, resp);
			rc = -EINVAL;
			goto error;
		}
	} else if (hdcp_ctrl->init_data.sec_access) {
		DSS_REG_W(hdcp_io, reg_set->sec_data0, link0_bksv_0);
		DSS_REG_W(hdcp_io, reg_set->sec_data1, link0_bksv_1);
	} else {
		DSS_REG_W(io, reg_set->data0, link0_bksv_0);
		DSS_REG_W(io, reg_set->data1, link0_bksv_1);
	}

	/*
	 * HDCP Compliace Test case 1A-01:
	 * Wait here at least 100ms before reading R0'
	 */
	msleep(125);

	/* Wait for HDCP R0 computation to be completed */
	rc = readl_poll_timeout(io->base + reg_set->status, link0_status,
				link0_status & BIT(reg_set->r0_offset),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (IS_ERR_VALUE(rc)) {
		DEV_ERR("%s: R0 not ready\n", __func__);
		goto error;
	}

	memset(buf, 0, sizeof(buf));
	rc = hdcp_1x_read(hdcp_ctrl, &hdcp_ctrl->sink_addr.r0, buf, false);
	if (IS_ERR_VALUE(rc)) {
		DEV_ERR("%s: error reading R0' from sink\n", __func__);
		goto error;
	}

	DEV_DBG("%s: %s: R0'=%02x%02x\n", __func__, HDCP_STATE_NAME,
		buf[1], buf[0]);

	/* Write R0' to HDCP registers and check to see if it is a match */
	reinit_completion(&hdcp_ctrl->r0_checked);
	DSS_REG_W(io, reg_set->data2_0, (((u32)buf[1]) << 8) | buf[0]);
	timeout_count = wait_for_completion_timeout(
		&hdcp_ctrl->r0_checked, HZ*2);
	link0_status = DSS_REG_R(io, reg_set->status);
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
	if (rc)
		DEV_ERR("%s: %s: Authentication Part I failed\n", __func__,
			hdcp_ctrl ? HDCP_STATE_NAME : "???");
	else
		DEV_INFO("%s: %s: Authentication Part I successful\n",
			__func__, HDCP_STATE_NAME);
	return rc;
} /* hdcp_1x_authentication_part1 */

static int hdcp_1x_set_v_h(struct hdcp_1x_ctrl *hdcp_ctrl,
			struct hdcp_1x_reg_data *rd, u8 *buf)
{
	int rc;
	struct dss_io_data *io;

	if (!hdcp_ctrl->tz_hdcp && hdcp_ctrl->init_data.sec_access)
		io = hdcp_ctrl->init_data.hdcp_io;
	else
		io = hdcp_ctrl->init_data.core_io;

	rc = hdcp_1x_read(hdcp_ctrl, rd->sink, buf, false);
	if (IS_ERR_VALUE(rc)) {
		DEV_ERR("%s: error reading %s\n", __func__, rd->sink->name);
		goto end;
	}

	DEV_DBG("%s: %s: %s: buf[0]=%x, buf[1]=%x, buf[2]=%x, buf[3]=%x\n",
		__func__, HDCP_STATE_NAME, rd->sink->name, buf[0], buf[1],
		buf[2], buf[3]);

	if (!hdcp_ctrl->tz_hdcp)
		DSS_REG_W(io, rd->reg_id,
			(buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0]));
end:
	return rc;
}

static int hdcp_1x_transfer_v_h(struct hdcp_1x_ctrl *hdcp_ctrl)
{
	int rc = 0;
	u8 buf[4];
	struct scm_hdcp_req scm_buf[SCM_HDCP_MAX_REG];
	u32 phy_addr;
	struct hdcp_reg_set *reg_set = &hdcp_ctrl->reg_set;
	struct hdcp_1x_reg_data reg_data[]  = {
		{reg_set_data(7),  &hdcp_ctrl->sink_addr.v_h0},
		{reg_set_data(8),  &hdcp_ctrl->sink_addr.v_h1},
		{reg_set_data(9),  &hdcp_ctrl->sink_addr.v_h2},
		{reg_set_data(10), &hdcp_ctrl->sink_addr.v_h3},
		{reg_set_data(11), &hdcp_ctrl->sink_addr.v_h4},
	};
	u32 size = ARRAY_SIZE(reg_data);
	u32 iter = 0, ret = 0, resp = 0;

	phy_addr = hdcp_ctrl->init_data.phy_addr;

	memset(scm_buf, 0x00, sizeof(scm_buf));

	for (iter = 0; iter < size; iter++) {
		struct hdcp_1x_reg_data *rd = reg_data + iter;

		memset(buf, 0, sizeof(buf));
		hdcp_1x_set_v_h(hdcp_ctrl, rd, buf);

		if (hdcp_ctrl->tz_hdcp) {
			u32 reg_val = buf[3] << 24 | buf[2] << 16 |
				buf[1] << 8 | buf[0];

			scm_buf[iter].addr = phy_addr + reg_data[iter].reg_id;
			scm_buf[iter].val  = reg_val;

			ret = hdcp_scm_call(scm_buf, &resp);
			if (ret || resp) {
				DEV_ERR("%s: scm err: ret=%d, resp=%d\n",
					__func__, ret, resp);
				rc = -EINVAL;
				goto error;
			}
		}
	}
error:
	return rc;
}

static int hdcp_1x_authentication_part2(struct hdcp_1x_ctrl *hdcp_ctrl)
{
	int rc, i;
	u32 timeout_count, down_stream_devices = 0;
	u32 repeater_cascade_depth = 0;
	u8 buf[0xFF];
	u8 *ksv_fifo = NULL;
	u8 bcaps = 0;
	u16 bstatus = 0, max_devs_exceeded = 0, max_cascade_exceeded = 0;
	u32 status = 0, sha_status = 0;
	u32 ksv_bytes;
	struct dss_io_data *io;
	struct hdcp_reg_set *reg_set;
	struct scm_hdcp_req scm_buf[SCM_HDCP_MAX_REG];
	u32 phy_addr;
	u32 ret  = 0;
	u32 resp = 0;
	u32 ksv_read_retry = 20;

	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	phy_addr = hdcp_ctrl->init_data.phy_addr;
	reg_set = &hdcp_ctrl->reg_set;

	if (HDCP_STATE_AUTHENTICATING != hdcp_ctrl->hdcp_state) {
		DEV_DBG("%s: %s: invalid state. returning\n", __func__,
			HDCP_STATE_NAME);
		rc = -EINVAL;
		goto error;
	}

	ksv_fifo = hdcp_ctrl->current_tp.ksv_list;

	io = hdcp_ctrl->init_data.core_io;

	memset(buf, 0, sizeof(buf));
	memset(ksv_fifo, 0, sizeof(hdcp_ctrl->current_tp.ksv_list));

	/*
	 * Wait until READY bit is set in BCAPS, as per HDCP specifications
	 * maximum permitted time to check for READY bit is five seconds.
	 */
	timeout_count = 50;
	do {
		rc = hdcp_1x_read(hdcp_ctrl, &hdcp_ctrl->sink_addr.bcaps,
				&bcaps, true);
		if (IS_ERR_VALUE(rc)) {
			DEV_ERR("%s: error reading bcaps\n", __func__);
			goto error;
		}
		msleep(100);
	} while (!(bcaps & BIT(5)) && --timeout_count);

	rc = hdcp_1x_read(hdcp_ctrl, &hdcp_ctrl->sink_addr.bstatus,
			buf, true);
	if (IS_ERR_VALUE(rc)) {
		DEV_ERR("%s: error reading bstatus\n", __func__);
		goto error;
	}

	bstatus = buf[1];
	bstatus = (bstatus << 8) | buf[0];

	if (hdcp_ctrl->tz_hdcp) {
		memset(scm_buf, 0x00, sizeof(scm_buf));

		/* Write BSTATUS and BCAPS to HDCP registers */
		scm_buf[0].addr = phy_addr + reg_set->data12;
		scm_buf[0].val  = bcaps | (bstatus << 8);

		ret = hdcp_scm_call(scm_buf, &resp);
		if (ret || resp) {
			DEV_ERR("%s: error: scm_call ret = %d, resp = %d\n",
				__func__, ret, resp);
			rc = -EINVAL;
			goto error;
		}
	} else if (hdcp_ctrl->init_data.sec_access) {
		DSS_REG_W(hdcp_ctrl->init_data.hdcp_io,
			  reg_set->sec_data12,
			  bcaps | (bstatus << 8));
	} else {
		DSS_REG_W(io, reg_set->data12, bcaps | (bstatus << 8));
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
	hdcp_ctrl->sink_addr.ksv_fifo.len = ksv_bytes;

	do {
		rc = hdcp_1x_read(hdcp_ctrl, &hdcp_ctrl->sink_addr.ksv_fifo,
				ksv_fifo, false);
		if (IS_ERR_VALUE(rc)) {
			DEV_DBG("%s: could not read ksv fifo (%d)\n",
				__func__, ksv_read_retry);
			/*
			 * HDCP Compliace Test case 1B-01:
			 * Wait here until all the ksv bytes have been
			 * read from the KSV FIFO register.
			 */
			msleep(25);

		}
	} while (rc && --ksv_read_retry);

	if (rc) {
		DEV_ERR("%s: error reading ksv_fifo\n", __func__);
		goto error;
	}

	rc = hdcp_1x_transfer_v_h(hdcp_ctrl);
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

		scm_buf[0].addr = phy_addr + reg_set->sha_ctrl;
		scm_buf[0].val  = HDCP_REG_ENABLE;
		scm_buf[1].addr = phy_addr + reg_set->sha_ctrl;
		scm_buf[1].val  = HDCP_REG_DISABLE;

		ret = hdcp_scm_call(scm_buf, &resp);
		if (ret || resp) {
			DEV_ERR("%s: error: scm_call ret = %d, resp = %d\n",
				__func__, ret, resp);
			rc = -EINVAL;
			goto error;
		}
	} else if (hdcp_ctrl->init_data.sec_access) {
		DSS_REG_W(hdcp_ctrl->init_data.hdcp_io,
				reg_set->sec_sha_ctrl,
				HDCP_REG_ENABLE);
		DSS_REG_W(hdcp_ctrl->init_data.hdcp_io,
				reg_set->sec_sha_ctrl,
				HDCP_REG_DISABLE);
	} else {
		DSS_REG_W(io, reg_set->sha_ctrl, HDCP_REG_ENABLE);
		DSS_REG_W(io, reg_set->sha_ctrl, HDCP_REG_DISABLE);
	}

	for (i = 0; i < ksv_bytes - 1; i++) {
		/* Write KSV byte and do not set DONE bit[0] */
		if (hdcp_ctrl->tz_hdcp) {
			memset(scm_buf, 0x00, sizeof(scm_buf));

			scm_buf[0].addr = phy_addr + reg_set->sha_ctrl;
			scm_buf[0].val  = ksv_fifo[i] << 16;

			ret = hdcp_scm_call(scm_buf, &resp);
			if (ret || resp) {
				DEV_ERR("%s: scm_call ret = %d, resp = %d\n",
					__func__, ret, resp);
				rc = -EINVAL;
				goto error;
			}
		} else if (hdcp_ctrl->init_data.sec_access) {
			DSS_REG_W_ND(hdcp_ctrl->init_data.hdcp_io,
					reg_set->sec_sha_ctrl,
					ksv_fifo[i] << 16);
		} else {
			DSS_REG_W_ND(io, reg_set->sha_ctrl, ksv_fifo[i] << 16);
		}

		/*
		 * Once 64 bytes have been written, we need to poll for
		 * HDCP_SHA_BLOCK_DONE before writing any further
		 */
		if (i && !((i + 1) % 64)) {
			rc = readl_poll_timeout(io->base + reg_set->sha_status,
						sha_status, sha_status & BIT(0),
						HDCP_POLL_SLEEP_US,
						HDCP_POLL_TIMEOUT_US);
			if (IS_ERR_VALUE(rc)) {
				DEV_ERR("%s: block not done\n", __func__);
				goto error;
			}
		}
	}

	/* Write l to DONE bit[0] */
	if (hdcp_ctrl->tz_hdcp) {
		memset(scm_buf, 0x00, sizeof(scm_buf));

		scm_buf[0].addr = phy_addr + reg_set->sha_ctrl;
		scm_buf[0].val  = (ksv_fifo[ksv_bytes - 1] << 16) | 0x1;

		ret = hdcp_scm_call(scm_buf, &resp);
		if (ret || resp) {
			DEV_ERR("%s: error: scm_call ret = %d, resp = %d\n",
				__func__, ret, resp);
			rc = -EINVAL;
			goto error;
		}
	} else if (hdcp_ctrl->init_data.sec_access) {
		DSS_REG_W_ND(hdcp_ctrl->init_data.hdcp_io,
				reg_set->sec_sha_ctrl,
				(ksv_fifo[ksv_bytes - 1] << 16) | 0x1);
	} else {
		DSS_REG_W_ND(io, reg_set->sha_ctrl,
			(ksv_fifo[ksv_bytes - 1] << 16) | 0x1);
	}

	/* Now wait for HDCP_SHA_COMP_DONE */
	rc = readl_poll_timeout(io->base + reg_set->sha_status, sha_status,
				sha_status & BIT(4),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (IS_ERR_VALUE(rc)) {
		DEV_ERR("%s: comp not done\n", __func__);
		goto error;
	}

	/* Wait for V_MATCHES */
	rc = readl_poll_timeout(io->base + reg_set->status, status,
				status & BIT(reg_set->v_offset),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (IS_ERR_VALUE(rc)) {
		DEV_ERR("%s: V not ready\n", __func__);
		goto error;
	}
error:
	if (rc)
		DEV_ERR("%s: %s: Authentication Part II failed\n", __func__,
			hdcp_ctrl ? HDCP_STATE_NAME : "???");
	else
		DEV_INFO("%s: %s: Authentication Part II successful\n",
			__func__, HDCP_STATE_NAME);

	if (!hdcp_ctrl) {
		DEV_ERR("%s: hdcp_ctrl null. Topology not updated\n",
			__func__);
		return rc;
	}
	/* Update topology information */
	hdcp_ctrl->current_tp.dev_count = down_stream_devices;
	hdcp_ctrl->current_tp.max_cascade_exceeded = max_cascade_exceeded;
	hdcp_ctrl->current_tp.max_dev_exceeded = max_devs_exceeded;
	hdcp_ctrl->current_tp.depth = repeater_cascade_depth;

	return rc;
} /* hdcp_1x_authentication_part2 */

static void hdcp_1x_cache_topology(struct hdcp_1x_ctrl *hdcp_ctrl)
{
	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	memcpy((void *)&hdcp_ctrl->cached_tp,
		(void *) &hdcp_ctrl->current_tp,
		sizeof(hdcp_ctrl->cached_tp));
}

static void hdcp_1x_notify_topology(struct hdcp_1x_ctrl *hdcp_ctrl)
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

static void hdcp_1x_int_work(struct work_struct *work)
{
	struct hdcp_1x_ctrl *hdcp_ctrl = container_of(work,
		struct hdcp_1x_ctrl, hdcp_int_work);

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
} /* hdcp_1x_int_work */

static void hdcp_1x_auth_work(struct work_struct *work)
{
	int rc;
	struct delayed_work *dw = to_delayed_work(work);
	struct hdcp_1x_ctrl *hdcp_ctrl = container_of(dw,
		struct hdcp_1x_ctrl, hdcp_auth_work);
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
	/* Enabling Software DDC for HDMI and REF timer for DP */
	if (hdcp_ctrl->init_data.client_id == HDCP_CLIENT_HDMI)
		DSS_REG_W_ND(io, HDMI_DDC_ARBITRATION, DSS_REG_R(io,
				HDMI_DDC_ARBITRATION) & ~(BIT(4)));
	else if (hdcp_ctrl->init_data.client_id == HDCP_CLIENT_DP)
		DSS_REG_W(io, DP_DP_HPD_REFTIMER, 0x10013);

	rc = hdcp_1x_authentication_part1(hdcp_ctrl);
	if (rc) {
		DEV_DBG("%s: %s: HDCP Auth Part I failed\n", __func__,
			HDCP_STATE_NAME);
		goto error;
	}

	if (hdcp_ctrl->current_tp.ds_type == DS_REPEATER) {
		rc = hdcp_1x_authentication_part2(hdcp_ctrl);
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
	if (hdcp_ctrl->init_data.client_id == HDCP_CLIENT_HDMI)
		DSS_REG_W_ND(io, HDMI_DDC_ARBITRATION, DSS_REG_R(io,
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
			hdcp_1x_cache_topology(hdcp_ctrl);
			hdcp_1x_notify_topology(hdcp_ctrl);
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
} /* hdcp_1x_auth_work */

int hdcp_1x_authenticate(void *input)
{
	struct hdcp_1x_ctrl *hdcp_ctrl = (struct hdcp_1x_ctrl *)input;

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

	if (!hdcp_1x_load_keys(input)) {
		flush_delayed_work(&hdcp_ctrl->hdcp_auth_work);

		queue_delayed_work(hdcp_ctrl->init_data.workq,
			&hdcp_ctrl->hdcp_auth_work, HZ/2);
	} else {
		flush_work(&hdcp_ctrl->hdcp_int_work);

		queue_work(hdcp_ctrl->init_data.workq,
			&hdcp_ctrl->hdcp_int_work);
	}

	return 0;
} /* hdcp_1x_authenticate */

int hdcp_1x_reauthenticate(void *input)
{
	struct hdcp_1x_ctrl *hdcp_ctrl = (struct hdcp_1x_ctrl *)input;
	struct dss_io_data *io;
	struct hdcp_reg_set *reg_set;
	struct hdcp_int_set *isr;
	u32 hdmi_hw_version;
	u32 ret = 0;

	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	io = hdcp_ctrl->init_data.core_io;
	reg_set = &hdcp_ctrl->reg_set;
	isr = &hdcp_ctrl->int_set;

	if (HDCP_STATE_AUTH_FAIL != hdcp_ctrl->hdcp_state) {
		DEV_DBG("%s: %s: invalid state. returning\n", __func__,
			HDCP_STATE_NAME);
		return 0;
	}

	if (hdcp_ctrl->init_data.client_id == HDCP_CLIENT_HDMI) {
		hdmi_hw_version = DSS_REG_R(io, HDMI_VERSION);
		if (hdmi_hw_version >= 0x30030000) {
			DSS_REG_W(io, HDMI_CTRL_SW_RESET, BIT(1));
			DSS_REG_W(io, HDMI_CTRL_SW_RESET, 0);
		}

		/* Wait to be clean on DDC HW engine */
		hdcp_1x_hw_ddc_clean(hdcp_ctrl);
	}

	/* Disable HDCP interrupts */
	DSS_REG_W(io, isr->int_reg, DSS_REG_R(io, isr->int_reg) & ~HDCP_INT_EN);

	if (reg_set->reset)
		DSS_REG_W(io, reg_set->reset, BIT(0));

	/* Disable encryption and disable the HDCP block */
	DSS_REG_W(io, reg_set->ctrl, 0);

	if (!hdcp_1x_load_keys(input))
		queue_delayed_work(hdcp_ctrl->init_data.workq,
			&hdcp_ctrl->hdcp_auth_work, HZ/2);
	else
		queue_work(hdcp_ctrl->init_data.workq,
			&hdcp_ctrl->hdcp_int_work);

	return ret;
} /* hdcp_1x_reauthenticate */

void hdcp_1x_off(void *input)
{
	struct hdcp_1x_ctrl *hdcp_ctrl = (struct hdcp_1x_ctrl *)input;
	struct dss_io_data *io;
	struct hdcp_reg_set *reg_set;
	struct hdcp_int_set *isr;
	int rc = 0;

	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = hdcp_ctrl->init_data.core_io;
	reg_set = &hdcp_ctrl->reg_set;
	isr = &hdcp_ctrl->int_set;

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
	DSS_REG_W(io, isr->int_reg,
		DSS_REG_R(io, isr->int_reg) & ~HDCP_INT_EN);
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

	if (reg_set->reset)
		DSS_REG_W(io, reg_set->reset, BIT(0));

	/* Disable encryption and disable the HDCP block */
	DSS_REG_W(io, reg_set->ctrl, 0);

	DEV_DBG("%s: %s: HDCP: Off\n", __func__, HDCP_STATE_NAME);
} /* hdcp_1x_off */

int hdcp_1x_isr(void *input)
{
	struct hdcp_1x_ctrl *hdcp_ctrl = (struct hdcp_1x_ctrl *)input;
	int rc = 0;
	struct dss_io_data *io;
	u32 hdcp_int_val;
	struct hdcp_reg_set *reg_set;
	struct hdcp_int_set *isr;

	if (!hdcp_ctrl || !hdcp_ctrl->init_data.core_io) {
		DEV_ERR("%s: invalid input\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	io = hdcp_ctrl->init_data.core_io;
	reg_set = &hdcp_ctrl->reg_set;
	isr = &hdcp_ctrl->int_set;

	hdcp_int_val = DSS_REG_R(io, isr->int_reg);

	/* Ignore HDCP interrupts if HDCP is disabled */
	if (HDCP_STATE_INACTIVE == hdcp_ctrl->hdcp_state) {
		DSS_REG_W(io, isr->int_reg, hdcp_int_val | HDCP_INT_CLR);
		return 0;
	}

	if (hdcp_int_val & isr->auth_success_int) {
		/* AUTH_SUCCESS_INT */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->auth_success_ack));
		DEV_INFO("%s: %s: AUTH_SUCCESS_INT received\n", __func__,
			HDCP_STATE_NAME);
		if (HDCP_STATE_AUTHENTICATING == hdcp_ctrl->hdcp_state)
			complete_all(&hdcp_ctrl->r0_checked);
	}

	if (hdcp_int_val & isr->auth_fail_int) {
		/* AUTH_FAIL_INT */
		u32 link_status = DSS_REG_R(io, reg_set->status);

		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->auth_fail_ack));
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
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->auth_fail_info_ack));
	}

	if (hdcp_int_val & isr->tx_req_int) {
		/* DDC_XFER_REQ_INT */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->tx_req_ack));
		DEV_INFO("%s: %s: DDC_XFER_REQ_INT received\n", __func__,
			HDCP_STATE_NAME);
	}

	if (hdcp_int_val & isr->tx_req_done_int) {
		/* DDC_XFER_DONE_INT */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->tx_req_done_ack));
		DEV_INFO("%s: %s: DDC_XFER_DONE received\n", __func__,
			HDCP_STATE_NAME);
	}

	if (hdcp_int_val & isr->encryption_ready) {
		/* Encryption enabled */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->encryption_ready_ack));
		DEV_INFO("%s: %s: encryption ready received\n", __func__,
			HDCP_STATE_NAME);
	}

	if (hdcp_int_val & isr->encryption_not_ready) {
		/* Encryption enabled */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->encryption_not_ready_ack));
		DEV_INFO("%s: %s: encryption not ready received\n", __func__,
			HDCP_STATE_NAME);
	}

error:
	return rc;
} /* hdcp_1x_isr */

static struct hdcp_1x_ctrl *hdcp_1x_get_ctrl(struct device *dev)
{
	struct fb_info *fbi;
	struct msm_fb_data_type *mfd;
	struct mdss_panel_info *pinfo;

	if (!dev) {
		pr_err("invalid input\n");
		goto error;
	}

	fbi = dev_get_drvdata(dev);
	if (!fbi) {
		pr_err("invalid fbi\n");
		goto error;
	}

	mfd = fbi->par;
	if (!mfd) {
		pr_err("invalid mfd\n");
		goto error;
	}

	pinfo = mfd->panel_info;
	if (!pinfo) {
		pr_err("invalid pinfo\n");
		goto error;
	}

	return pinfo->hdcp_1x_data;

error:
	return NULL;
}
static ssize_t hdcp_1x_sysfs_rda_status(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdcp_1x_ctrl *hdcp_ctrl = hdcp_1x_get_ctrl(dev);

	if (!hdcp_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	mutex_lock(hdcp_ctrl->init_data.mutex);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", hdcp_ctrl->hdcp_state);
	DEV_DBG("%s: '%d'\n", __func__, hdcp_ctrl->hdcp_state);
	mutex_unlock(hdcp_ctrl->init_data.mutex);

	return ret;
} /* hdcp_1x_sysfs_rda_hdcp*/

static ssize_t hdcp_1x_sysfs_rda_tp(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct hdcp_1x_ctrl *hdcp_ctrl = hdcp_1x_get_ctrl(dev);

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
} /* hdcp_1x_sysfs_rda_tp*/

static ssize_t hdcp_1x_sysfs_wta_tp(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int msgid = 0;
	ssize_t ret = count;
	struct hdcp_1x_ctrl *hdcp_ctrl = hdcp_1x_get_ctrl(dev);

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

static DEVICE_ATTR(status, S_IRUGO, hdcp_1x_sysfs_rda_status, NULL);
static DEVICE_ATTR(tp, S_IRUGO | S_IWUSR, hdcp_1x_sysfs_rda_tp,
	hdcp_1x_sysfs_wta_tp);


static struct attribute *hdcp_1x_fs_attrs[] = {
	&dev_attr_status.attr,
	&dev_attr_tp.attr,
	NULL,
};

static struct attribute_group hdcp_1x_fs_attr_group = {
	.name = "hdcp",
	.attrs = hdcp_1x_fs_attrs,
};

void hdcp_1x_deinit(void *input)
{
	struct hdcp_1x_ctrl *hdcp_ctrl = (struct hdcp_1x_ctrl *)input;

	if (!hdcp_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	sysfs_remove_group(hdcp_ctrl->init_data.sysfs_kobj,
				&hdcp_1x_fs_attr_group);

	kfree(hdcp_ctrl);
} /* hdcp_1x_deinit */

static void hdcp_1x_update_client_reg_set(struct hdcp_1x_ctrl *hdcp_ctrl)
{
	if (hdcp_ctrl->init_data.client_id == HDCP_CLIENT_HDMI) {
		struct hdcp_reg_set reg_set = HDCP_REG_SET_CLIENT_HDMI;
		struct hdcp_sink_addr_map sink_addr = HDCP_HDMI_SINK_ADDR_MAP;
		struct hdcp_int_set isr = HDCP_HDMI_INT_SET;

		hdcp_ctrl->reg_set = reg_set;
		hdcp_ctrl->sink_addr = sink_addr;
		hdcp_ctrl->int_set = isr;
	} else if (hdcp_ctrl->init_data.client_id == HDCP_CLIENT_DP) {
		struct hdcp_reg_set reg_set = HDCP_REG_SET_CLIENT_DP;
		struct hdcp_sink_addr_map sink_addr = HDCP_DP_SINK_ADDR_MAP;
		struct hdcp_int_set isr = HDCP_DP_INT_SET;

		hdcp_ctrl->reg_set = reg_set;
		hdcp_ctrl->sink_addr = sink_addr;
		hdcp_ctrl->int_set = isr;
	}
}

void *hdcp_1x_init(struct hdcp_init_data *init_data)
{
	struct hdcp_1x_ctrl *hdcp_ctrl = NULL;
	int ret;
	static struct hdcp_ops ops = {
		.isr = hdcp_1x_isr,
		.reauthenticate = hdcp_1x_reauthenticate,
		.authenticate = hdcp_1x_authenticate,
		.off = hdcp_1x_off
	};

	if (!init_data || !init_data->core_io || !init_data->qfprom_io ||
		!init_data->mutex || !init_data->notify_status ||
		!init_data->workq || !init_data->cb_data) {
		DEV_ERR("%s: invalid input\n", __func__);
		goto error;
	}

	if (init_data->sec_access && !init_data->hdcp_io) {
		DEV_ERR("%s: hdcp_io required\n", __func__);
		goto error;
	}

	hdcp_ctrl = kzalloc(sizeof(*hdcp_ctrl), GFP_KERNEL);
	if (!hdcp_ctrl) {
		DEV_ERR("%s: Out of memory\n", __func__);
		goto error;
	}

	hdcp_ctrl->init_data = *init_data;
	hdcp_ctrl->ops = &ops;

	hdcp_1x_update_client_reg_set(hdcp_ctrl);

	if (sysfs_create_group(init_data->sysfs_kobj,
				&hdcp_1x_fs_attr_group)) {
		DEV_ERR("%s: hdcp sysfs group creation failed\n", __func__);
		goto error;
	}

	INIT_DELAYED_WORK(&hdcp_ctrl->hdcp_auth_work, hdcp_1x_auth_work);
	INIT_WORK(&hdcp_ctrl->hdcp_int_work, hdcp_1x_int_work);

	hdcp_ctrl->hdcp_state = HDCP_STATE_INACTIVE;
	init_completion(&hdcp_ctrl->r0_checked);

	if (!hdcp_ctrl->init_data.sec_access) {
		ret = scm_is_call_available(SCM_SVC_HDCP, SCM_CMD_HDCP);
		if (ret <= 0) {
			DEV_ERR("%s: secure hdcp service unavailable, ret = %d",
				 __func__, ret);
		} else {
			DEV_DBG("%s: tz_hdcp = 1\n", __func__);
			hdcp_ctrl->tz_hdcp = 1;
		}
	}

	DEV_DBG("%s: HDCP module initialized. HDCP_STATE=%s", __func__,
		HDCP_STATE_NAME);

error:
	return (void *)hdcp_ctrl;
} /* hdcp_1x_init */

struct hdcp_ops *hdcp_1x_start(void *input)
{
	return ((struct hdcp_1x_ctrl *)input)->ops;
}

