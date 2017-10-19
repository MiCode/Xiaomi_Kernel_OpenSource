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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/io.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/iopoll.h>
#include <linux/hdcp_qseecom.h>
#include "sde_hdcp.h"
#include "sde_hdmi_util.h"
#include "video/msm_hdmi_hdcp_mgr.h"

#define SDE_HDCP_STATE_NAME (sde_hdcp_state_name(hdcp->hdcp_state))

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

#define HDCP_INT_CLR (isr->auth_success_ack | isr->auth_fail_ack | \
			isr->auth_fail_info_ack | isr->tx_req_ack | \
			isr->encryption_ready_ack | \
			isr->encryption_not_ready_ack | isr->tx_req_done_ack)

#define HDCP_INT_EN (isr->auth_success_mask | isr->auth_fail_mask | \
			isr->encryption_ready_mask | \
			isr->encryption_not_ready_mask)

#define HDCP_POLL_SLEEP_US   (20 * 1000)
#define HDCP_POLL_TIMEOUT_US (HDCP_POLL_SLEEP_US * 100)

#define sde_hdcp_1x_state(x) (hdcp->hdcp_state == x)

struct sde_hdcp_sink_addr {
	char *name;
	u32 addr;
	u32 len;
};

struct sde_hdcp_1x_reg_data {
	u32 reg_id;
	struct sde_hdcp_sink_addr *sink;
};

struct sde_hdcp_skaddr_map {
	/* addresses to read from sink */
	struct sde_hdcp_sink_addr bcaps;
	struct sde_hdcp_sink_addr bksv;
	struct sde_hdcp_sink_addr r0;
	struct sde_hdcp_sink_addr bstatus;
	struct sde_hdcp_sink_addr cp_irq_status;
	struct sde_hdcp_sink_addr ksv_fifo;
	struct sde_hdcp_sink_addr v_h0;
	struct sde_hdcp_sink_addr v_h1;
	struct sde_hdcp_sink_addr v_h2;
	struct sde_hdcp_sink_addr v_h3;
	struct sde_hdcp_sink_addr v_h4;

	/* addresses to write to sink */
	struct sde_hdcp_sink_addr an;
	struct sde_hdcp_sink_addr aksv;
	struct sde_hdcp_sink_addr ainfo;
};

struct sde_hdcp_int_set {
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

struct sde_hdcp_reg_set {
	u32 status;
	u32 keys_offset;
	u32 r0_offset;
	u32 v_offset;
	u32 ctrl;
	u32 aksv_lsb;
	u32 aksv_msb;
	u32 entropy_ctrl0;
	u32 entropy_ctrl1;
	u32 sec_sha_ctrl;
	u32 sec_sha_data;
	u32 sha_status;

	u32 data2_0;
	u32 data3;
	u32 data4;
	u32 data5;
	u32 data6;

	u32 sec_data0;
	u32 sec_data1;
	u32 sec_data7;
	u32 sec_data8;
	u32 sec_data9;
	u32 sec_data10;
	u32 sec_data11;
	u32 sec_data12;

	u32 reset;
	u32 reset_bit;

	u32 repeater;
};

#define HDCP_REG_SET_CLIENT_HDMI \
	{HDMI_HDCP_LINK0_STATUS, 28, 24, 20, HDMI_HDCP_CTRL, \
	 HDMI_HDCP_SW_LOWER_AKSV, HDMI_HDCP_SW_UPPER_AKSV, \
	 HDMI_HDCP_ENTROPY_CTRL0, HDMI_HDCP_ENTROPY_CTRL1, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_SHA_CTRL, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_SHA_DATA, \
	 HDMI_HDCP_SHA_STATUS, HDMI_HDCP_RCVPORT_DATA2_0, \
	 HDMI_HDCP_RCVPORT_DATA3, HDMI_HDCP_RCVPORT_DATA4, \
	 HDMI_HDCP_RCVPORT_DATA5, HDMI_HDCP_RCVPORT_DATA6, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA0, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA1, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA7, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA8, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA9, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA10, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA11, \
	 HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA12, \
	 HDMI_HDCP_RESET, BIT(0), BIT(6)}

/* To do for DP */
#define HDCP_REG_SET_CLIENT_DP \
	{0}

#define HDCP_HDMI_SINK_ADDR_MAP \
	{{"bcaps", 0x40, 1}, {"bksv", 0x00, 5}, {"r0'", 0x08, 2}, \
	 {"bstatus", 0x41, 2}, {"??", 0x0, 0}, {"ksv-fifo", 0x43, 0}, \
	 {"v_h0", 0x20, 4}, {"v_h1", 0x24, 4}, {"v_h2", 0x28, 4}, \
	 {"v_h3", 0x2c, 4}, {"v_h4", 0x30, 4}, {"an", 0x18, 8}, \
	 {"aksv", 0x10, 5}, {"ainfo", 0x00, 0},}

#define HDCP_DP_SINK_ADDR_MAP \
	{{"bcaps", 0x68028, 1}, {"bksv", 0x68000, 5}, {"r0'", 0x68005, 2}, \
	 {"binfo", 0x6802A, 2}, {"cp_irq_status", 0x68029, 1}, \
	 {"ksv-fifo", 0x6802C, 0}, {"v_h0", 0x68014, 4}, {"v_h1", 0x68018, 4}, \
	 {"v_h2", 0x6801C, 4}, {"v_h3", 0x68020, 4}, {"v_h4", 0x68024, 4}, \
	 {"an", 0x6800C, 8}, {"aksv", 0x68007, 5}, {"ainfo", 0x6803B, 1} }

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

struct sde_hdcp_1x {
	u8 bcaps;
	u32 tp_msgid;
	u32 an_0, an_1, aksv_0, aksv_1;
	bool sink_r0_ready;
	bool reauth;
	bool ksv_ready;
	enum sde_hdcp_states hdcp_state;
	struct HDCP_V2V1_MSG_TOPOLOGY cached_tp;
	struct HDCP_V2V1_MSG_TOPOLOGY current_tp;
	struct delayed_work hdcp_auth_work;
	struct completion r0_checked;
	struct completion sink_r0_available;
	struct sde_hdcp_init_data init_data;
	struct sde_hdcp_ops *ops;
	struct sde_hdcp_reg_set reg_set;
	struct sde_hdcp_int_set int_set;
	struct sde_hdcp_skaddr_map sink_addr;
	struct workqueue_struct *workq;
};

const char *sde_hdcp_state_name(enum sde_hdcp_states hdcp_state)
{
	switch (hdcp_state) {
	case HDCP_STATE_INACTIVE:	return "HDCP_STATE_INACTIVE";
	case HDCP_STATE_AUTHENTICATING:	return "HDCP_STATE_AUTHENTICATING";
	case HDCP_STATE_AUTHENTICATED:	return "HDCP_STATE_AUTHENTICATED";
	case HDCP_STATE_AUTH_FAIL:	return "HDCP_STATE_AUTH_FAIL";
	default:			return "???";
	}
}

static int sde_hdcp_1x_count_one(u8 *array, u8 len)
{
	int i, j, count = 0;

	for (i = 0; i < len; i++)
		for (j = 0; j < 8; j++)
			count += (((array[i] >> j) & 0x1) ? 1 : 0);
	return count;
}

static void reset_hdcp_ddc_failures(struct sde_hdcp_1x *hdcp)
{
	int hdcp_ddc_ctrl1_reg;
	int hdcp_ddc_status;
	int failure;
	int nack0;
	struct dss_io_data *io;

	if (!hdcp || !hdcp->init_data.core_io) {
		pr_err("invalid input\n");
		return;
	}

	io = hdcp->init_data.core_io;

	/* Check for any DDC transfer failures */
	hdcp_ddc_status = DSS_REG_R(io, HDMI_HDCP_DDC_STATUS);
	failure = (hdcp_ddc_status >> 16) & BIT(0);
	nack0 = (hdcp_ddc_status >> 14) & BIT(0);
	SDE_HDCP_DEBUG("%s: HDCP_DDC_STATUS=0x%x, FAIL=%d, NACK0=%d\n",
		SDE_HDCP_STATE_NAME, hdcp_ddc_status, failure, nack0);

	if (failure) {
		/*
		 * Indicates that the last HDCP HW DDC transfer failed.
		 * This occurs when a transfer is attempted with HDCP DDC
		 * disabled (HDCP_DDC_DISABLE=1) or the number of retries
		 * matches HDCP_DDC_RETRY_CNT.
		 * Failure occurred,  let's clear it.
		 */
		SDE_HDCP_DEBUG("%s: DDC failure HDCP_DDC_STATUS=0x%08x\n",
			 SDE_HDCP_STATE_NAME, hdcp_ddc_status);

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
			SDE_HDCP_DEBUG("%s: HDCP DDC Failure cleared\n",
				SDE_HDCP_STATE_NAME);
		else
			SDE_ERROR("%s: Unable to clear HDCP DDC Failure",
				SDE_HDCP_STATE_NAME);

		/* Re-Enable HDCP DDC */
		DSS_REG_W(io, HDMI_HDCP_DDC_CTRL_0, 0);
	}

	if (nack0) {
		SDE_HDCP_DEBUG("%s: Before: HDMI_DDC_SW_STATUS=0x%08x\n",
			SDE_HDCP_STATE_NAME, DSS_REG_R(io, HDMI_DDC_SW_STATUS));
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
		SDE_HDCP_DEBUG("%s: After: HDMI_DDC_SW_STATUS=0x%08x\n",
			SDE_HDCP_STATE_NAME, DSS_REG_R(io, HDMI_DDC_SW_STATUS));
	}

	hdcp_ddc_status = DSS_REG_R(io, HDMI_HDCP_DDC_STATUS);

	failure = (hdcp_ddc_status >> 16) & BIT(0);
	nack0 = (hdcp_ddc_status >> 14) & BIT(0);
	SDE_HDCP_DEBUG("%s: On Exit: HDCP_DDC_STATUS=0x%x, FAIL=%d, NACK0=%d\n",
		SDE_HDCP_STATE_NAME, hdcp_ddc_status, failure, nack0);
} /* reset_hdcp_ddc_failures */

static void sde_hdcp_1x_hw_ddc_clean(struct sde_hdcp_1x *hdcp)
{
	struct dss_io_data *io = NULL;
	u32 hdcp_ddc_status, ddc_hw_status;
	u32 ddc_xfer_done, ddc_xfer_req;
	u32 ddc_hw_req, ddc_hw_not_idle;
	bool ddc_hw_not_ready, xfer_not_done, hw_not_done;
	u32 timeout_count;

	if (!hdcp || !hdcp->init_data.core_io) {
		pr_err("invalid input\n");
		return;
	}

	io = hdcp->init_data.core_io;
	if (!io->base) {
		pr_err("core io not inititalized\n");
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

		SDE_HDCP_DEBUG("%s: timeout count(%d): ddc hw%sready\n",
			SDE_HDCP_STATE_NAME, timeout_count,
				ddc_hw_not_ready ? " not " : " ");
		SDE_HDCP_DEBUG("hdcp_ddc_status[0x%x], ddc_hw_status[0x%x]\n",
				hdcp_ddc_status, ddc_hw_status);
		if (ddc_hw_not_ready)
			msleep(20);
		} while (ddc_hw_not_ready && --timeout_count);
} /* hdcp_1x_hw_ddc_clean */

static int sde_hdcp_1x_load_keys(void *input)
{
	int rc = 0;
	bool use_sw_keys = false;
	u32 reg_val;
	u32 ksv_lsb_addr, ksv_msb_addr;
	u32 aksv_lsb, aksv_msb;
	u8 aksv[5];
	struct dss_io_data *io;
	struct dss_io_data *qfprom_io;
	struct sde_hdcp_1x *hdcp = input;
	struct sde_hdcp_reg_set *reg_set;

	if (!hdcp || !hdcp->init_data.core_io ||
		!hdcp->init_data.qfprom_io) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	if (!sde_hdcp_1x_state(HDCP_STATE_INACTIVE) &&
	    !sde_hdcp_1x_state(HDCP_STATE_AUTH_FAIL)) {
		pr_err("%s: invalid state. returning\n",
			SDE_HDCP_STATE_NAME);
		rc = -EINVAL;
		goto end;
	}

	io = hdcp->init_data.core_io;
	qfprom_io = hdcp->init_data.qfprom_io;
	reg_set = &hdcp->reg_set;

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
			pr_err("setting hdcp SW keys failed\n");
			rc = -EINVAL;
			goto end;
		}
	} else {
		/* Fetch aksv from QFPROM, this info should be public. */
		ksv_lsb_addr = HDCP_KSV_LSB;
		ksv_msb_addr = HDCP_KSV_MSB;

		if (hdcp->init_data.sec_access) {
			ksv_lsb_addr += HDCP_KSV_VERSION_4_OFFSET;
			ksv_msb_addr += HDCP_KSV_VERSION_4_OFFSET;
		}

		aksv_lsb = DSS_REG_R(qfprom_io, ksv_lsb_addr);
		aksv_msb = DSS_REG_R(qfprom_io, ksv_msb_addr);
	}

	SDE_HDCP_DEBUG("%s: AKSV=%02x%08x\n", SDE_HDCP_STATE_NAME,
		aksv_msb, aksv_lsb);

	aksv[0] =  aksv_lsb        & 0xFF;
	aksv[1] = (aksv_lsb >> 8)  & 0xFF;
	aksv[2] = (aksv_lsb >> 16) & 0xFF;
	aksv[3] = (aksv_lsb >> 24) & 0xFF;
	aksv[4] =  aksv_msb        & 0xFF;

	/* check there are 20 ones in AKSV */
	if (sde_hdcp_1x_count_one(aksv, 5) != 20) {
		pr_err("AKSV bit count failed\n");
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

	hdcp->hdcp_state = HDCP_STATE_AUTHENTICATING;
end:
	return rc;
}

static int sde_hdcp_1x_read(struct sde_hdcp_1x *hdcp,
			  struct sde_hdcp_sink_addr *sink,
			  u8 *buf, bool realign)
{
	u32 rc = 0;
	struct sde_hdmi_tx_ddc_data *ddc_data;
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;

	if (hdcp->init_data.client_id == HDCP_CLIENT_HDMI) {
		reset_hdcp_ddc_failures(hdcp);

		ddc_ctrl = hdcp->init_data.ddc_ctrl;
		ddc_data = &ddc_ctrl->ddc_data;
		if (!ddc_data) {
			SDE_ERROR("invalid ddc data\n");
			return -EINVAL;
		}
		memset(ddc_data, 0, sizeof(*ddc_data));
		ddc_data->dev_addr = 0x74;
		ddc_data->offset = sink->addr;
		ddc_data->data_buf = buf;
		ddc_data->data_len = sink->len;
		ddc_data->request_len = sink->len;
		ddc_data->retry = 5;
		ddc_data->what = sink->name;
		ddc_data->retry_align = realign;

		rc = sde_hdmi_ddc_read((void *)hdcp->init_data.cb_data);
		if (rc)
			SDE_ERROR("%s: %s read failed\n",
				SDE_HDCP_STATE_NAME, sink->name);
	} else if (hdcp->init_data.client_id == HDCP_CLIENT_DP) {
		/* To-do DP APIs go here */
	}

	return rc;
}

static int sde_hdcp_1x_write(struct sde_hdcp_1x *hdcp,
			   struct sde_hdcp_sink_addr *sink, u8 *buf)
{
	int rc = 0;
	struct sde_hdmi_tx_ddc_data *ddc_data;
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;

	if (hdcp->init_data.client_id == HDCP_CLIENT_HDMI) {
		ddc_ctrl = hdcp->init_data.ddc_ctrl;
		ddc_data = &ddc_ctrl->ddc_data;

		if (!ddc_data) {
			SDE_ERROR("invalid ddc data\n");
			return -EINVAL;
		}
		memset(ddc_data, 0, sizeof(*ddc_data));

		ddc_data->dev_addr = 0x74;
		ddc_data->offset = sink->addr;
		ddc_data->data_buf = buf;
		ddc_data->data_len = sink->len;
		ddc_data->what = sink->name;

		rc = sde_hdmi_ddc_write((void *)hdcp->init_data.cb_data);
		if (rc)
			SDE_ERROR("%s: %s write failed\n",
				   SDE_HDCP_STATE_NAME, sink->name);
	} else if (hdcp->init_data.client_id == HDCP_CLIENT_DP) {
		/* To-do DP APIs go here */
	}

	return rc;
}

static void sde_hdcp_1x_enable_interrupts(struct sde_hdcp_1x *hdcp)
{
	u32 intr_reg;
	struct dss_io_data *io;
	struct sde_hdcp_int_set *isr;

	io = hdcp->init_data.core_io;
	isr = &hdcp->int_set;

	intr_reg = DSS_REG_R(io, isr->int_reg);

	intr_reg |= HDCP_INT_CLR | HDCP_INT_EN;

	DSS_REG_W(io, isr->int_reg, intr_reg);
}

static int sde_hdcp_1x_read_bcaps(struct sde_hdcp_1x *hdcp)
{
	int rc;
	struct sde_hdcp_reg_set *reg_set = &hdcp->reg_set;
	struct dss_io_data *hdcp_io  = hdcp->init_data.hdcp_io;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	rc = sde_hdcp_1x_read(hdcp, &hdcp->sink_addr.bcaps,
		&hdcp->bcaps, false);
	if (IS_ERR_VALUE(rc)) {
		pr_err("error reading bcaps\n");
		goto error;
	}

	SDE_HDCP_DEBUG("bcaps read: 0x%x\n", hdcp->bcaps);

	hdcp->current_tp.ds_type = hdcp->bcaps & reg_set->repeater ?
			DS_REPEATER : DS_RECEIVER;

	SDE_HDCP_DEBUG("ds: %s\n", hdcp->current_tp.ds_type == DS_REPEATER ?
			"repeater" : "receiver");

	/* Write BCAPS to the hardware */
	DSS_REG_W(hdcp_io, reg_set->sec_data12, hdcp->bcaps);
error:
	return rc;
}

static int sde_hdcp_1x_wait_for_hw_ready(struct sde_hdcp_1x *hdcp)
{
	int rc;
	u32 link0_status;
	struct sde_hdcp_reg_set *reg_set = &hdcp->reg_set;
	struct dss_io_data *io = hdcp->init_data.core_io;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	/* Wait for HDCP keys to be checked and validated */
	rc = readl_poll_timeout(io->base + reg_set->status, link0_status,
				((link0_status >> reg_set->keys_offset) & 0x7)
					== HDCP_KEYS_STATE_VALID ||
				!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (IS_ERR_VALUE(rc)) {
		pr_err("key not ready\n");
		goto error;
	}

	/*
	 * 1.1_Features turned off by default.
	 * No need to write AInfo since 1.1_Features is disabled.
	 */
	DSS_REG_W(io, reg_set->data4, 0);

	/* Wait for An0 and An1 bit to be ready */
	rc = readl_poll_timeout(io->base + reg_set->status, link0_status,
				(link0_status & (BIT(8) | BIT(9))) ||
				!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (IS_ERR_VALUE(rc)) {
		pr_err("An not ready\n");
		goto error;
	}

	/* As per hardware recommendations, wait before reading An */
	msleep(20);
error:
	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING))
		rc = -EINVAL;

	return rc;
}

static int sde_hdcp_1x_send_an_aksv_to_sink(struct sde_hdcp_1x *hdcp)
{
	int rc;
	u8 an[8], aksv[5];

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	an[0] =  hdcp->an_0        & 0xFF;
	an[1] = (hdcp->an_0 >> 8)  & 0xFF;
	an[2] = (hdcp->an_0 >> 16) & 0xFF;
	an[3] = (hdcp->an_0 >> 24) & 0xFF;
	an[4] =  hdcp->an_1        & 0xFF;
	an[5] = (hdcp->an_1 >> 8)  & 0xFF;
	an[6] = (hdcp->an_1 >> 16) & 0xFF;
	an[7] = (hdcp->an_1 >> 24) & 0xFF;

	SDE_HDCP_DEBUG("an read: 0x%2x%2x%2x%2x%2x%2x%2x%2x\n",
		an[7], an[6], an[5], an[4], an[3], an[2], an[1], an[0]);

	rc = sde_hdcp_1x_write(hdcp, &hdcp->sink_addr.an, an);
	if (IS_ERR_VALUE(rc)) {
		pr_err("error writing an to sink\n");
		goto error;
	}

	/* Copy An and AKSV to byte arrays for transmission */
	aksv[0] =  hdcp->aksv_0        & 0xFF;
	aksv[1] = (hdcp->aksv_0 >> 8)  & 0xFF;
	aksv[2] = (hdcp->aksv_0 >> 16) & 0xFF;
	aksv[3] = (hdcp->aksv_0 >> 24) & 0xFF;
	aksv[4] =  hdcp->aksv_1        & 0xFF;

	SDE_HDCP_DEBUG("aksv read: 0x%2x%2x%2x%2x%2x\n",
		aksv[4], aksv[3], aksv[2], aksv[1], aksv[0]);

	rc = sde_hdcp_1x_write(hdcp, &hdcp->sink_addr.aksv, aksv);
	if (IS_ERR_VALUE(rc)) {
		pr_err("error writing aksv to sink\n");
		goto error;
	}
error:
	return rc;
}

static int sde_hdcp_1x_read_an_aksv_from_hw(struct sde_hdcp_1x *hdcp)
{
	struct dss_io_data *io = hdcp->init_data.core_io;
	struct sde_hdcp_reg_set *reg_set = &hdcp->reg_set;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	hdcp->an_0 = DSS_REG_R(io, reg_set->data5);
	if (hdcp->init_data.client_id == HDCP_CLIENT_DP) {
		udelay(1);
		hdcp->an_0 = DSS_REG_R(io, reg_set->data5);
	}

	hdcp->an_1 = DSS_REG_R(io, reg_set->data6);
	if (hdcp->init_data.client_id == HDCP_CLIENT_DP) {
		udelay(1);
		hdcp->an_1 = DSS_REG_R(io, reg_set->data6);
	}

	/* Read AKSV */
	hdcp->aksv_0 = DSS_REG_R(io, reg_set->data3);
	hdcp->aksv_1 = DSS_REG_R(io, reg_set->data4);

	return 0;
}

static int sde_hdcp_1x_get_bksv_from_sink(struct sde_hdcp_1x *hdcp)
{
	int rc;
	u8 *bksv = hdcp->current_tp.bksv;
	u32 link0_bksv_0, link0_bksv_1;
	struct sde_hdcp_reg_set *reg_set = &hdcp->reg_set;
	struct dss_io_data *hdcp_io  = hdcp->init_data.hdcp_io;

	rc = sde_hdcp_1x_read(hdcp, &hdcp->sink_addr.bksv, bksv, false);
	if (IS_ERR_VALUE(rc)) {
		pr_err("error reading bksv from sink\n");
		goto error;
	}

	SDE_HDCP_DEBUG("bksv read: 0x%2x%2x%2x%2x%2x\n",
		bksv[4], bksv[3], bksv[2], bksv[1], bksv[0]);

	/* check there are 20 ones in BKSV */
	if (sde_hdcp_1x_count_one(bksv, 5) != 20) {
		pr_err("%s: BKSV doesn't have 20 1's and 20 0's\n",
			SDE_HDCP_STATE_NAME);
		rc = -EINVAL;
		goto error;
	}

	link0_bksv_0 = bksv[3];
	link0_bksv_0 = (link0_bksv_0 << 8) | bksv[2];
	link0_bksv_0 = (link0_bksv_0 << 8) | bksv[1];
	link0_bksv_0 = (link0_bksv_0 << 8) | bksv[0];
	link0_bksv_1 = bksv[4];

	DSS_REG_W(hdcp_io, reg_set->sec_data0, link0_bksv_0);
	DSS_REG_W(hdcp_io, reg_set->sec_data1, link0_bksv_1);
error:
	return rc;
}

static void sde_hdcp_1x_enable_sink_irq_hpd(struct sde_hdcp_1x *hdcp)
{
	int rc;
	u8 const required_major = 1, required_minor = 2;
	u8 sink_major = 0, sink_minor = 0;
	u8 enable_hpd_irq = 0x1;
	u16 version;

	if (hdcp->init_data.client_id == HDCP_CLIENT_HDMI)
		return;

	version = *hdcp->init_data.version;
	sink_major = (version >> 4) & 0x0f;
	sink_minor = version & 0x0f;

	if ((sink_minor < required_minor) || (sink_major < required_major) ||
		(hdcp->current_tp.ds_type != DS_REPEATER)) {
		pr_debug("sink irq hpd not enabled\n");
		return;
	}

	rc = sde_hdcp_1x_write(hdcp, &hdcp->sink_addr.ainfo, &enable_hpd_irq);
	if (IS_ERR_VALUE(rc))
		SDE_HDCP_DEBUG("error writing ainfo to sink\n");
}

static int sde_hdcp_1x_verify_r0(struct sde_hdcp_1x *hdcp)
{
	int rc, r0_retry = 3;
	u8 buf[2];
	u32 link0_status, timeout_count;
	u32 const r0_read_delay_us = 1;
	u32 const r0_read_timeout_us = r0_read_delay_us * 10;
	struct sde_hdcp_reg_set *reg_set = &hdcp->reg_set;
	struct dss_io_data *io = hdcp->init_data.core_io;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	/* Wait for HDCP R0 computation to be completed */
	rc = readl_poll_timeout(io->base + reg_set->status, link0_status,
				(link0_status & BIT(reg_set->r0_offset)) ||
				!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (IS_ERR_VALUE(rc)) {
		pr_err("R0 not ready\n");
		goto error;
	}

	/*
	 * HDCP Compliace Test case 1A-01:
	 * Wait here at least 100ms before reading R0'
	 */
	if (hdcp->init_data.client_id == HDCP_CLIENT_HDMI) {
		msleep(100);
	} else {
		if (!hdcp->sink_r0_ready) {
			reinit_completion(&hdcp->sink_r0_available);
			timeout_count = wait_for_completion_timeout(
				&hdcp->sink_r0_available, HZ / 2);

			if (hdcp->reauth) {
				pr_err("sink R0 not ready\n");
				rc = -EINVAL;
				goto error;
			}
		}
	}

	do {
		memset(buf, 0, sizeof(buf));

		rc = sde_hdcp_1x_read(hdcp, &hdcp->sink_addr.r0,
			buf, false);
		if (IS_ERR_VALUE(rc)) {
			pr_err("error reading R0' from sink\n");
			goto error;
		}

		SDE_HDCP_DEBUG("sink R0'read: %2x%2x\n", buf[1], buf[0]);

		DSS_REG_W(io, reg_set->data2_0, (((u32)buf[1]) << 8) | buf[0]);

		rc = readl_poll_timeout(io->base + reg_set->status,
			link0_status, (link0_status & BIT(12)) ||
			!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING),
			r0_read_delay_us, r0_read_timeout_us);
	} while (rc && --r0_retry);
error:
	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING))
		rc = -EINVAL;

	return rc;
}

static int sde_hdcp_1x_authentication_part1(struct sde_hdcp_1x *hdcp)
{
	int rc;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	sde_hdcp_1x_enable_interrupts(hdcp);

	rc = sde_hdcp_1x_read_bcaps(hdcp);
	if (rc)
		goto error;

	rc = sde_hdcp_1x_wait_for_hw_ready(hdcp);
	if (rc)
		goto error;

	rc = sde_hdcp_1x_read_an_aksv_from_hw(hdcp);
	if (rc)
		goto error;

	rc = sde_hdcp_1x_get_bksv_from_sink(hdcp);
	if (rc)
		goto error;

	rc = sde_hdcp_1x_send_an_aksv_to_sink(hdcp);
	if (rc)
		goto error;

	sde_hdcp_1x_enable_sink_irq_hpd(hdcp);

	rc = sde_hdcp_1x_verify_r0(hdcp);
	if (rc)
		goto error;

	pr_info("SUCCESSFUL\n");

	return 0;
error:
	pr_err("%s: FAILED\n", SDE_HDCP_STATE_NAME);

	return rc;
}

static int sde_hdcp_1x_transfer_v_h(struct sde_hdcp_1x *hdcp)
{
	int rc = 0;
	struct dss_io_data *io = hdcp->init_data.hdcp_io;
	struct sde_hdcp_reg_set *reg_set = &hdcp->reg_set;
	struct sde_hdcp_1x_reg_data reg_data[]  = {
		{reg_set->sec_data7,  &hdcp->sink_addr.v_h0},
		{reg_set->sec_data8,  &hdcp->sink_addr.v_h1},
		{reg_set->sec_data9,  &hdcp->sink_addr.v_h2},
		{reg_set->sec_data10, &hdcp->sink_addr.v_h3},
		{reg_set->sec_data11, &hdcp->sink_addr.v_h4},
	};
	struct sde_hdcp_sink_addr sink = {"V", reg_data->sink->addr};
	u32 size = ARRAY_SIZE(reg_data);
	u8 buf[0xFF] = {0};
	u32 i = 0, len = 0;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	for (i = 0; i < size; i++) {
		struct sde_hdcp_1x_reg_data *rd = reg_data + i;

		len += rd->sink->len;
	}

	sink.len = len;

	rc = sde_hdcp_1x_read(hdcp, &sink, buf, false);
	if (IS_ERR_VALUE(rc)) {
		pr_err("error reading %s\n", sink.name);
		goto end;
	}


	for (i = 0; i < size; i++) {
		struct sde_hdcp_1x_reg_data *rd = reg_data + i;
		u32 reg_data;

		memcpy(&reg_data, buf + (sizeof(u32) * i), sizeof(u32));
		DSS_REG_W(io, rd->reg_id, reg_data);
	}
end:
	return rc;
}

static int sde_hdcp_1x_validate_downstream(struct sde_hdcp_1x *hdcp)
{
	int rc;
	u8 buf[2] = {0, 0};
	u8 device_count, depth;
	u8 max_cascade_exceeded, max_devs_exceeded;
	u16 bstatus;
	struct sde_hdcp_reg_set *reg_set = &hdcp->reg_set;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	rc = sde_hdcp_1x_read(hdcp, &hdcp->sink_addr.bstatus,
			buf, false);
	if (IS_ERR_VALUE(rc)) {
		pr_err("error reading bstatus\n");
		goto end;
	}

	bstatus = buf[1];
	bstatus = (bstatus << 8) | buf[0];

	device_count = bstatus & 0x7F;

	SDE_HDCP_DEBUG("device count %d\n", device_count);

	/* Cascaded repeater depth */
	depth = (bstatus >> 8) & 0x7;
	SDE_HDCP_DEBUG("depth %d\n", depth);

	/*
	 * HDCP Compliance 1B-05:
	 * Check if no. of devices connected to repeater
	 * exceed max_devices_connected from bit 7 of Bstatus.
	 */
	max_devs_exceeded = (bstatus & BIT(7)) >> 7;
	if (max_devs_exceeded == 0x01) {
		pr_err("no. of devs connected exceed max allowed\n");
		rc = -EINVAL;
		goto end;
	}

	/*
	 * HDCP Compliance 1B-06:
	 * Check if no. of cascade connected to repeater
	 * exceed max_cascade_connected from bit 11 of Bstatus.
	 */
	max_cascade_exceeded = (bstatus & BIT(11)) >> 11;
	if (max_cascade_exceeded == 0x01) {
		pr_err("no. of cascade connections exceed max allowed\n");
		rc = -EINVAL;
		goto end;
	}

	/* Update topology information */
	hdcp->current_tp.dev_count = device_count;
	hdcp->current_tp.max_cascade_exceeded = max_cascade_exceeded;
	hdcp->current_tp.max_dev_exceeded = max_devs_exceeded;
	hdcp->current_tp.depth = depth;

	DSS_REG_W(hdcp->init_data.hdcp_io,
		  reg_set->sec_data12, hdcp->bcaps | (bstatus << 8));
end:
	return rc;
}

static int sde_hdcp_1x_read_ksv_fifo(struct sde_hdcp_1x *hdcp)
{
	u32 ksv_read_retry = 20, ksv_bytes, rc = 0;
	u8 *ksv_fifo = hdcp->current_tp.ksv_list;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	memset(ksv_fifo, 0, sizeof(hdcp->current_tp.ksv_list));

	/* each KSV is 5 bytes long */
	ksv_bytes = 5 * hdcp->current_tp.dev_count;
	hdcp->sink_addr.ksv_fifo.len = ksv_bytes;

	while (ksv_bytes && --ksv_read_retry) {
		rc = sde_hdcp_1x_read(hdcp, &hdcp->sink_addr.ksv_fifo,
				ksv_fifo, true);
		if (IS_ERR_VALUE(rc))
			pr_err("could not read ksv fifo (%d)\n",
				ksv_read_retry);
		else
			break;
	}

	if (rc)
		pr_err("error reading ksv_fifo\n");

	return rc;
}

static int sde_hdcp_1x_write_ksv_fifo(struct sde_hdcp_1x *hdcp)
{
	int i, rc = 0;
	u8 *ksv_fifo = hdcp->current_tp.ksv_list;
	u32 ksv_bytes = hdcp->sink_addr.ksv_fifo.len;
	struct dss_io_data *io = hdcp->init_data.core_io;
	struct dss_io_data *sec_io = hdcp->init_data.hdcp_io;
	struct sde_hdcp_reg_set *reg_set = &hdcp->reg_set;
	u32 sha_status = 0, status;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	/* reset SHA Controller */
	DSS_REG_W(sec_io, reg_set->sec_sha_ctrl, 0x1);
	DSS_REG_W(sec_io, reg_set->sec_sha_ctrl, 0x0);

	for (i = 0; i < ksv_bytes - 1; i++) {
		/* Write KSV byte and do not set DONE bit[0] */
		DSS_REG_W_ND(sec_io, reg_set->sec_sha_data, ksv_fifo[i] << 16);

		/*
		 * Once 64 bytes have been written, we need to poll for
		 * HDCP_SHA_BLOCK_DONE before writing any further
		 */
		if (i && !((i + 1) % 64)) {
			rc = readl_poll_timeout(io->base + reg_set->sha_status,
				sha_status, (sha_status & BIT(0)) ||
				!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
			if (IS_ERR_VALUE(rc)) {
				pr_err("block not done\n");
				goto error;
			}
		}
	}

	/* Write l to DONE bit[0] */
	DSS_REG_W_ND(sec_io, reg_set->sec_sha_data,
		(ksv_fifo[ksv_bytes - 1] << 16) | 0x1);

	/* Now wait for HDCP_SHA_COMP_DONE */
	rc = readl_poll_timeout(io->base + reg_set->sha_status, sha_status,
				(sha_status & BIT(4)) ||
				!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (IS_ERR_VALUE(rc)) {
		pr_err("V computation not done\n");
		goto error;
	}

	/* Wait for V_MATCHES */
	rc = readl_poll_timeout(io->base + reg_set->status, status,
				(status & BIT(reg_set->v_offset)) ||
				!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (IS_ERR_VALUE(rc)) {
		pr_err("V mismatch\n");
		rc = -EINVAL;
	}
error:
	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING))
		rc = -EINVAL;

	return rc;
}

static int sde_hdcp_1x_wait_for_ksv_ready(struct sde_hdcp_1x *hdcp)
{
	int rc, timeout;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	/*
	 * Wait until READY bit is set in BCAPS, as per HDCP specifications
	 * maximum permitted time to check for READY bit is five seconds.
	 */
	rc = sde_hdcp_1x_read(hdcp, &hdcp->sink_addr.bcaps,
		&hdcp->bcaps, false);
	if (IS_ERR_VALUE(rc)) {
		pr_err("error reading bcaps\n");
		goto error;
	}

	if (hdcp->init_data.client_id == HDCP_CLIENT_HDMI) {
		timeout = 50;

		while (!(hdcp->bcaps & BIT(5)) && --timeout) {
			rc = sde_hdcp_1x_read(hdcp,
				&hdcp->sink_addr.bcaps,
				&hdcp->bcaps, false);
			if (IS_ERR_VALUE(rc) ||
			   !sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
				pr_err("error reading bcaps\n");
				goto error;
			}
			msleep(100);
		}
	} else {
		u8 cp_buf = 0;
		struct sde_hdcp_sink_addr *sink =
			&hdcp->sink_addr.cp_irq_status;

		timeout = jiffies_to_msecs(jiffies);

		while (1) {
			rc = sde_hdcp_1x_read(hdcp, sink, &cp_buf, false);
			if (rc)
				goto error;

			if (cp_buf & BIT(0))
				break;

			/* max timeout of 5 sec as per hdcp 1.x spec */
			if (abs(timeout - jiffies_to_msecs(jiffies)) > 5000) {
				timeout = 0;
				break;
			}

			if (hdcp->ksv_ready || hdcp->reauth ||
			    !sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING))
				break;

			/* re-read after a minimum delay */
			msleep(20);
		}
	}

	if (!timeout || hdcp->reauth ||
	    !sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("DS KSV not ready\n");
		rc = -EINVAL;
	} else {
		hdcp->ksv_ready = true;
	}
error:
	return rc;
}

static int sde_hdcp_1x_authentication_part2(struct sde_hdcp_1x *hdcp)
{
	int rc;
	int v_retry = 3;

	rc = sde_hdcp_1x_validate_downstream(hdcp);
	if (rc)
		goto error;

	rc = sde_hdcp_1x_read_ksv_fifo(hdcp);
	if (rc)
		goto error;

	do {
		rc = sde_hdcp_1x_transfer_v_h(hdcp);
		if (rc)
			goto error;

		/* do not proceed further if no device connected */
		if (!hdcp->current_tp.dev_count)
			goto error;

		rc = sde_hdcp_1x_write_ksv_fifo(hdcp);
	} while (--v_retry && rc);
error:
	if (rc) {
		pr_err("%s: FAILED\n", SDE_HDCP_STATE_NAME);
	} else {
		hdcp->hdcp_state = HDCP_STATE_AUTHENTICATED;

		pr_info("SUCCESSFUL\n");
	}

	return rc;
}

static void sde_hdcp_1x_cache_topology(struct sde_hdcp_1x *hdcp)
{
	if (!hdcp || !hdcp->init_data.core_io) {
		pr_err("invalid input\n");
		return;
	}

	memcpy((void *)&hdcp->cached_tp,
		(void *) &hdcp->current_tp,
		sizeof(hdcp->cached_tp));
	hdcp1_cache_repeater_topology((void *)&hdcp->cached_tp);
}

static void sde_hdcp_1x_notify_topology(void)
{
	hdcp1_notify_topology();
}

static void sde_hdcp_1x_update_auth_status(struct sde_hdcp_1x *hdcp)
{
	if (sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATED)) {
		sde_hdcp_1x_cache_topology(hdcp);
		sde_hdcp_1x_notify_topology();
	}

	if (hdcp->init_data.notify_status &&
	    !sde_hdcp_1x_state(HDCP_STATE_INACTIVE)) {
		hdcp->init_data.notify_status(
			hdcp->init_data.cb_data,
			hdcp->hdcp_state);
	}
}

static void sde_hdcp_1x_auth_work(struct work_struct *work)
{
	int rc;
	struct delayed_work *dw = to_delayed_work(work);
	struct sde_hdcp_1x *hdcp = container_of(dw,
		struct sde_hdcp_1x, hdcp_auth_work);
	struct dss_io_data *io;

	if (!hdcp) {
		pr_err("invalid input\n");
		return;
	}

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return;
	}

	hdcp->sink_r0_ready = false;
	hdcp->reauth = false;
	hdcp->ksv_ready = false;

	io = hdcp->init_data.core_io;
	/* Enabling Software DDC for HDMI and REF timer for DP */
	if (hdcp->init_data.client_id == HDCP_CLIENT_HDMI)
		DSS_REG_W_ND(io, HDMI_DDC_ARBITRATION, DSS_REG_R(io,
				HDMI_DDC_ARBITRATION) & ~(BIT(4)));
	else if (hdcp->init_data.client_id == HDCP_CLIENT_DP) {
		/* To do for DP */
	}

	/*
	 * program hw to enable encryption as soon as
	 * authentication is successful.
	 */
	hdcp1_set_enc(true);

	rc = sde_hdcp_1x_authentication_part1(hdcp);
	if (rc)
		goto end;

	if (hdcp->current_tp.ds_type == DS_REPEATER) {
		rc = sde_hdcp_1x_wait_for_ksv_ready(hdcp);
		if (rc)
			goto end;
	} else {
		hdcp->hdcp_state = HDCP_STATE_AUTHENTICATED;
		goto end;
	}

	hdcp->ksv_ready = false;

	rc = sde_hdcp_1x_authentication_part2(hdcp);
	if (rc)
		goto end;


end:
	if (rc && !sde_hdcp_1x_state(HDCP_STATE_INACTIVE))
		hdcp->hdcp_state = HDCP_STATE_AUTH_FAIL;

	/*
	 * Disabling software DDC before going into part3 to make sure
	 * there is no Arbitration between software and hardware for DDC
	 */
	if (hdcp->init_data.client_id == HDCP_CLIENT_HDMI)
		DSS_REG_W_ND(io, HDMI_DDC_ARBITRATION, DSS_REG_R(io,
				HDMI_DDC_ARBITRATION) | (BIT(4)));

	sde_hdcp_1x_update_auth_status(hdcp);
}

static int sde_hdcp_1x_authenticate(void *input)
{
	struct sde_hdcp_1x *hdcp = (struct sde_hdcp_1x *)input;

	if (!hdcp) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	flush_delayed_work(&hdcp->hdcp_auth_work);

	if (!sde_hdcp_1x_state(HDCP_STATE_INACTIVE)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	if (!sde_hdcp_1x_load_keys(input)) {

		queue_delayed_work(hdcp->workq,
			&hdcp->hdcp_auth_work, HZ/2);
	} else {
		hdcp->hdcp_state = HDCP_STATE_AUTH_FAIL;
		sde_hdcp_1x_update_auth_status(hdcp);
	}

	return 0;
} /* hdcp_1x_authenticate */

static int sde_hdcp_1x_reauthenticate(void *input)
{
	struct sde_hdcp_1x *hdcp = (struct sde_hdcp_1x *)input;
	struct dss_io_data *io;
	struct sde_hdcp_reg_set *reg_set;
	struct sde_hdcp_int_set *isr;
	u32 hdmi_hw_version;
	u32 ret = 0, reg;

	if (!hdcp || !hdcp->init_data.core_io) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	io = hdcp->init_data.core_io;
	reg_set = &hdcp->reg_set;
	isr = &hdcp->int_set;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTH_FAIL)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	if (hdcp->init_data.client_id == HDCP_CLIENT_HDMI) {
		hdmi_hw_version = DSS_REG_R(io, HDMI_VERSION);
		if (hdmi_hw_version >= 0x30030000) {
			DSS_REG_W(io, HDMI_CTRL_SW_RESET, BIT(1));
			DSS_REG_W(io, HDMI_CTRL_SW_RESET, 0);
		}

		/* Wait to be clean on DDC HW engine */
		sde_hdcp_1x_hw_ddc_clean(hdcp);
	}

	/* Disable HDCP interrupts */
	DSS_REG_W(io, isr->int_reg, DSS_REG_R(io, isr->int_reg) & ~HDCP_INT_EN);

	reg = DSS_REG_R(io, reg_set->reset);
	DSS_REG_W(io, reg_set->reset, reg | reg_set->reset_bit);

	/* Disable encryption and disable the HDCP block */
	DSS_REG_W(io, reg_set->ctrl, 0);

	DSS_REG_W(io, reg_set->reset, reg & ~reg_set->reset_bit);

	hdcp->hdcp_state = HDCP_STATE_INACTIVE;
	sde_hdcp_1x_authenticate(hdcp);

	return ret;
} /* hdcp_1x_reauthenticate */

static void sde_hdcp_1x_off(void *input)
{
	struct sde_hdcp_1x *hdcp = (struct sde_hdcp_1x *)input;
	struct dss_io_data *io;
	struct sde_hdcp_reg_set *reg_set;
	struct sde_hdcp_int_set *isr;
	int rc = 0;
	u32 reg;

	if (!hdcp || !hdcp->init_data.core_io) {
		pr_err("invalid input\n");
		return;
	}

	io = hdcp->init_data.core_io;
	reg_set = &hdcp->reg_set;
	isr = &hdcp->int_set;

	if (sde_hdcp_1x_state(HDCP_STATE_INACTIVE)) {
		pr_err("invalid state\n");
		return;
	}

	/*
	 * Disable HDCP interrupts.
	 * Also, need to set the state to inactive here so that any ongoing
	 * reauth works will know that the HDCP session has been turned off.
	 */
	mutex_lock(hdcp->init_data.mutex);
	DSS_REG_W(io, isr->int_reg,
		DSS_REG_R(io, isr->int_reg) & ~HDCP_INT_EN);
	hdcp->hdcp_state = HDCP_STATE_INACTIVE;
	mutex_unlock(hdcp->init_data.mutex);

	/* complete any wait pending */
	complete_all(&hdcp->sink_r0_available);
	complete_all(&hdcp->r0_checked);
	/*
	 * Cancel any pending auth/reauth attempts.
	 * If one is ongoing, this will wait for it to finish.
	 * No more reauthentiaction attempts will be scheduled since we
	 * set the currect state to inactive.
	 */
	rc = cancel_delayed_work_sync(&hdcp->hdcp_auth_work);
	if (rc)
		SDE_HDCP_DEBUG("%s: Deleted hdcp auth work\n",
			SDE_HDCP_STATE_NAME);

	hdcp1_set_enc(false);

	reg = DSS_REG_R(io, reg_set->reset);
	DSS_REG_W(io, reg_set->reset, reg | reg_set->reset_bit);

	/* Disable encryption and disable the HDCP block */
	DSS_REG_W(io, reg_set->ctrl, 0);

	DSS_REG_W(io, reg_set->reset, reg & ~reg_set->reset_bit);

	hdcp->sink_r0_ready = false;

	SDE_HDCP_DEBUG("%s: HDCP: Off\n", SDE_HDCP_STATE_NAME);
} /* hdcp_1x_off */

static int sde_hdcp_1x_isr(void *input)
{
	struct sde_hdcp_1x *hdcp = (struct sde_hdcp_1x *)input;
	int rc = 0;
	struct dss_io_data *io;
	u32 hdcp_int_val;
	struct sde_hdcp_reg_set *reg_set;
	struct sde_hdcp_int_set *isr;

	if (!hdcp || !hdcp->init_data.core_io) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	io = hdcp->init_data.core_io;
	reg_set = &hdcp->reg_set;
	isr = &hdcp->int_set;

	hdcp_int_val = DSS_REG_R(io, isr->int_reg);

	/* Ignore HDCP interrupts if HDCP is disabled */
	if (sde_hdcp_1x_state(HDCP_STATE_INACTIVE)) {
		DSS_REG_W(io, isr->int_reg, hdcp_int_val | HDCP_INT_CLR);
		return 0;
	}

	if (hdcp_int_val & isr->auth_success_int) {
		/* AUTH_SUCCESS_INT */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->auth_success_ack));
		SDE_HDCP_DEBUG("%s: AUTH SUCCESS\n", SDE_HDCP_STATE_NAME);

		if (sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING))
			complete_all(&hdcp->r0_checked);
	}

	if (hdcp_int_val & isr->auth_fail_int) {
		/* AUTH_FAIL_INT */
		u32 link_status = DSS_REG_R(io, reg_set->status);

		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->auth_fail_ack));

		SDE_HDCP_DEBUG("%s: AUTH FAIL, LINK0_STATUS=0x%08x\n",
			SDE_HDCP_STATE_NAME, link_status);

		/* Clear AUTH_FAIL_INFO as well */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->auth_fail_info_ack));

		if (sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATED)) {
			hdcp->hdcp_state = HDCP_STATE_AUTH_FAIL;
			sde_hdcp_1x_update_auth_status(hdcp);
		} else if (sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
			complete_all(&hdcp->r0_checked);
		}

	}

	if (hdcp_int_val & isr->tx_req_int) {
		/* DDC_XFER_REQ_INT */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->tx_req_ack));
		SDE_HDCP_DEBUG("%s: DDC_XFER_REQ_INT received\n",
			SDE_HDCP_STATE_NAME);
	}

	if (hdcp_int_val & isr->tx_req_done_int) {
		/* DDC_XFER_DONE_INT */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->tx_req_done_ack));
		SDE_HDCP_DEBUG("%s: DDC_XFER_DONE received\n",
			SDE_HDCP_STATE_NAME);
	}

	if (hdcp_int_val & isr->encryption_ready) {
		/* Encryption enabled */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->encryption_ready_ack));
		SDE_HDCP_DEBUG("%s: encryption ready received\n",
			SDE_HDCP_STATE_NAME);
	}

	if (hdcp_int_val & isr->encryption_not_ready) {
		/* Encryption enabled */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->encryption_not_ready_ack));
		SDE_HDCP_DEBUG("%s: encryption not ready received\n",
			SDE_HDCP_STATE_NAME);
	}

error:
	return rc;
}

void sde_hdcp_1x_deinit(void *input)
{
	struct sde_hdcp_1x *hdcp = (struct sde_hdcp_1x *)input;

	if (!hdcp) {
		pr_err("invalid input\n");
		return;
	}

	if (hdcp->workq)
		destroy_workqueue(hdcp->workq);

	kfree(hdcp);
} /* hdcp_1x_deinit */

static void sde_hdcp_1x_update_client_reg_set(struct sde_hdcp_1x *hdcp)
{

	if (hdcp->init_data.client_id == HDCP_CLIENT_HDMI) {
		struct sde_hdcp_reg_set reg_set = HDCP_REG_SET_CLIENT_HDMI;
		struct sde_hdcp_skaddr_map sink_addr = HDCP_HDMI_SINK_ADDR_MAP;
		struct sde_hdcp_int_set isr = HDCP_HDMI_INT_SET;

		hdcp->reg_set = reg_set;
		hdcp->sink_addr = sink_addr;
		hdcp->int_set = isr;
	} else if (hdcp->init_data.client_id == HDCP_CLIENT_DP) {
		/* TO DO for DP
		 * Will be filled later
		 */
	}
}

static bool sde_hdcp_1x_is_cp_irq_raised(struct sde_hdcp_1x *hdcp)
{
	int ret;
	u8 buf = 0;
	struct sde_hdcp_sink_addr sink = {"irq", 0x201, 1};

	ret = sde_hdcp_1x_read(hdcp, &sink, &buf, false);
	if (IS_ERR_VALUE(ret))
		pr_err("error reading irq_vector\n");

	return buf & BIT(2) ? true : false;
}

static void sde_hdcp_1x_clear_cp_irq(struct sde_hdcp_1x *hdcp)
{
	int ret;
	u8 buf = BIT(2);
	struct sde_hdcp_sink_addr sink = {"irq", 0x201, 1};

	ret = sde_hdcp_1x_write(hdcp, &sink, &buf);
	if (IS_ERR_VALUE(ret))
		pr_err("error clearing irq_vector\n");
}

static int sde_hdcp_1x_cp_irq(void *input)
{
	struct sde_hdcp_1x *hdcp = (struct sde_hdcp_1x *)input;
	u8 buf = 0;
	int ret;

	if (!hdcp) {
		pr_err("invalid input\n");
		goto irq_not_handled;
	}

	if (!sde_hdcp_1x_is_cp_irq_raised(hdcp)) {
		SDE_HDCP_DEBUG("cp_irq not raised\n");
		goto irq_not_handled;
	}

	ret = sde_hdcp_1x_read(hdcp, &hdcp->sink_addr.cp_irq_status,
			&buf, false);
	if (IS_ERR_VALUE(ret)) {
		pr_err("error reading cp_irq_status\n");
		goto irq_not_handled;
	}

	if ((buf & BIT(2)) || (buf & BIT(3))) {
		pr_err("%s\n",
			buf & BIT(2) ? "LINK_INTEGRITY_FAILURE" :
				"REAUTHENTICATION_REQUEST");

		hdcp->reauth = true;

		if (!sde_hdcp_1x_state(HDCP_STATE_INACTIVE))
			hdcp->hdcp_state = HDCP_STATE_AUTH_FAIL;

		complete_all(&hdcp->sink_r0_available);
		sde_hdcp_1x_update_auth_status(hdcp);
	} else if (buf & BIT(1)) {
		SDE_HDCP_DEBUG("R0' AVAILABLE\n");
		hdcp->sink_r0_ready = true;
		complete_all(&hdcp->sink_r0_available);
	} else if ((buf & BIT(0))) {
		SDE_HDCP_DEBUG("KSVs READY\n");

		hdcp->ksv_ready = true;
	} else {
		SDE_HDCP_DEBUG("spurious interrupt\n");
	}

	sde_hdcp_1x_clear_cp_irq(hdcp);
	return 0;

irq_not_handled:
	return -EINVAL;
}

void *sde_hdcp_1x_init(struct sde_hdcp_init_data *init_data)
{
	struct sde_hdcp_1x *hdcp = NULL;
	char name[20];
	static struct sde_hdcp_ops ops = {
		.isr = sde_hdcp_1x_isr,
		.cp_irq = sde_hdcp_1x_cp_irq,
		.reauthenticate = sde_hdcp_1x_reauthenticate,
		.authenticate = sde_hdcp_1x_authenticate,
		.off = sde_hdcp_1x_off
	};

	if (!init_data || !init_data->core_io || !init_data->qfprom_io ||
		!init_data->mutex || !init_data->notify_status ||
		!init_data->workq || !init_data->cb_data) {
		pr_err("invalid input\n");
		goto error;
	}

	if (init_data->sec_access && !init_data->hdcp_io) {
		pr_err("hdcp_io required\n");
		goto error;
	}

	hdcp = kzalloc(sizeof(*hdcp), GFP_KERNEL);
	if (!hdcp)
		goto error;

	hdcp->init_data = *init_data;
	hdcp->ops = &ops;

	snprintf(name, sizeof(name), "hdcp_1x_%d",
		hdcp->init_data.client_id);

	hdcp->workq = create_workqueue(name);
	if (!hdcp->workq) {
		pr_err("Error creating workqueue\n");
		kfree(hdcp);
		goto error;
	}

	sde_hdcp_1x_update_client_reg_set(hdcp);

	INIT_DELAYED_WORK(&hdcp->hdcp_auth_work, sde_hdcp_1x_auth_work);

	hdcp->hdcp_state = HDCP_STATE_INACTIVE;
	init_completion(&hdcp->r0_checked);
	init_completion(&hdcp->sink_r0_available);

	SDE_HDCP_DEBUG("HDCP module initialized. HDCP_STATE=%s\n",
		SDE_HDCP_STATE_NAME);

	return (void *)hdcp;

error:
	return NULL;
} /* hdcp_1x_init */

struct sde_hdcp_ops *sde_hdcp_1x_start(void *input)
{
	return ((struct sde_hdcp_1x *)input)->ops;
}

