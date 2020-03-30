// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2010-2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[sde-hdcp1x] %s: " fmt, __func__

#include <linux/io.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/iopoll.h>
#include <linux/msm_hdcp.h>
#include <drm/drm_dp_helper.h>
#include "sde_hdcp.h"
#include "video/msm_hdmi_hdcp_mgr.h"
#include "dp/dp_reg.h"

#define SDE_HDCP_STATE_NAME (sde_hdcp_state_name(hdcp->hdcp_state))

/* QFPROM Registers for HDMI/HDCP */
#define QFPROM_RAW_FEAT_CONFIG_ROW0_LSB  (0x000000F8)
#define QFPROM_RAW_FEAT_CONFIG_ROW0_MSB  (0x000000FC)
#define QFPROM_RAW_VERSION_4             (0x000000A8)
#define SEC_CTRL_HW_VERSION              (0x00006000)
#define HDCP_KSV_LSB                     (0x000060D8)
#define HDCP_KSV_MSB                     (0x000060DC)
#define HDCP_KSV_VERSION_4_OFFSET        (0x00000014)

/* SEC_CTRL version that supports HDCP SEL */
#define HDCP_SEL_MIN_SEC_VERSION         (0x50010000)

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

struct sde_hdcp_sink_addr_map {
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
	{0}

#define HDCP_REG_SET_CLIENT_DP \
{DP_HDCP_STATUS, 16, 14, 13, DP_HDCP_CTRL, \
	DP_HDCP_SW_LOWER_AKSV, DP_HDCP_SW_UPPER_AKSV, \
	DP_HDCP_ENTROPY_CTRL0, DP_HDCP_ENTROPY_CTRL1, \
	HDCP_SEC_DP_TZ_HV_HLOS_HDCP_SHA_CTRL, \
	HDCP_SEC_DP_TZ_HV_HLOS_HDCP_SHA_DATA, \
	DP_HDCP_SHA_STATUS, DP_HDCP_RCVPORT_DATA2_0, \
	DP_HDCP_RCVPORT_DATA3, DP_HDCP_RCVPORT_DATA4, \
	DP_HDCP_RCVPORT_DATA5, DP_HDCP_RCVPORT_DATA6, \
	HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA0, \
	HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA1, \
	HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA7, \
	HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA8, \
	HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA9, \
	HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA10, \
	HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA11, \
	HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA12, \
	DP_SW_RESET, BIT(1), BIT(1)}

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
	{0}

#define HDCP_DP_INT_SET \
	{DP_INTR_STATUS2, \
	 BIT(17), BIT(20), BIT(24), BIT(27), 0, 0, \
	 BIT(16), BIT(19), BIT(21), BIT(23), BIT(26), 0, 0, \
	 BIT(15), BIT(18), BIT(22), BIT(25), 0, 0}

struct sde_hdcp_1x {
	u8 bcaps;
	u32 tp_msgid;
	u32 an_0, an_1, aksv_0, aksv_1;
	u32 aksv_msb, aksv_lsb;
	bool sink_r0_ready;
	bool reauth;
	bool ksv_ready;
	bool force_encryption;
	enum sde_hdcp_state hdcp_state;
	struct HDCP_V2V1_MSG_TOPOLOGY current_tp;
	struct delayed_work hdcp_auth_work;
	struct completion r0_checked;
	struct completion sink_r0_available;
	struct sde_hdcp_init_data init_data;
	struct sde_hdcp_ops *ops;
	struct sde_hdcp_reg_set reg_set;
	struct sde_hdcp_int_set int_set;
	struct sde_hdcp_sink_addr_map sink_addr;
	struct workqueue_struct *workq;
	void *hdcp1_handle;
};

static int sde_hdcp_1x_count_one(u8 *array, u8 len)
{
	int i, j, count = 0;

	for (i = 0; i < len; i++)
		for (j = 0; j < 8; j++)
			count += (((array[i] >> j) & 0x1) ? 1 : 0);
	return count;
}

static int sde_hdcp_1x_enable_hdcp_engine(void *input)
{
	int rc = 0;
	struct dss_io_data *dp_ahb;
	struct dss_io_data *dp_aux;
	struct dss_io_data *dp_link;
	struct sde_hdcp_1x *hdcp = input;
	struct sde_hdcp_reg_set *reg_set;

	if (!hdcp || !hdcp->init_data.dp_ahb ||
		!hdcp->init_data.dp_aux ||
		!hdcp->init_data.dp_link) {
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

	dp_ahb = hdcp->init_data.dp_ahb;
	dp_aux = hdcp->init_data.dp_aux;
	dp_link = hdcp->init_data.dp_link;
	reg_set = &hdcp->reg_set;

	DSS_REG_W(dp_aux, reg_set->aksv_lsb, hdcp->aksv_lsb);
	DSS_REG_W(dp_aux, reg_set->aksv_msb, hdcp->aksv_msb);

	/* Setup seed values for random number An */
	DSS_REG_W(dp_link, reg_set->entropy_ctrl0, 0xB1FFB0FF);
	DSS_REG_W(dp_link, reg_set->entropy_ctrl1, 0xF00DFACE);

	/* make sure hw is programmed */
	wmb();

	/* enable hdcp engine */
	DSS_REG_W(dp_ahb, reg_set->ctrl, 0x1);

	hdcp->hdcp_state = HDCP_STATE_AUTHENTICATING;
end:
	return rc;
}

static int sde_hdcp_1x_read(struct sde_hdcp_1x *hdcp,
			  struct sde_hdcp_sink_addr *sink,
			  u8 *buf, bool realign)
{
	int const max_size = 15;
	int rc = 0, read_size = 0, bytes_read = 0;

	if (hdcp->init_data.client_id == HDCP_CLIENT_DP) {
		int size = sink->len, offset = sink->addr;

		do {
			read_size = min(size, max_size);

			bytes_read = drm_dp_dpcd_read(hdcp->init_data.drm_aux,
					offset, buf, read_size);
			if (bytes_read != read_size) {
				pr_err("fail: offset(0x%x), size(0x%x), rc(0x%x)\n",
					offset, read_size, bytes_read);
				rc = -EIO;
				break;
			}

			buf += read_size;
			size -= read_size;

			if (!realign)
				offset += read_size;
		} while (size > 0);
	}

	return rc;
}

static int sde_hdcp_1x_write(struct sde_hdcp_1x *hdcp,
			   struct sde_hdcp_sink_addr *sink, u8 *buf)
{
	int const max_size = 16;
	int rc = 0, write_size = 0, bytes_written = 0;

	if (hdcp->init_data.client_id == HDCP_CLIENT_DP) {
		int size = sink->len, offset = sink->addr;

		do {
			write_size = min(size, max_size);

			bytes_written =
				drm_dp_dpcd_write(hdcp->init_data.drm_aux,
						offset, buf, write_size);
			if (bytes_written != write_size) {
				pr_err("fail: offset(0x%x), size(0x%x), rc(0x%x)\n",
					offset, write_size, bytes_written);
				rc = -EIO;
				break;
			}

			buf += write_size;
			offset += write_size;
			size -= write_size;
		} while (size > 0);
	}

	return rc;
}

static void sde_hdcp_1x_enable_interrupts(struct sde_hdcp_1x *hdcp)
{
	u32 intr_reg;
	struct dss_io_data *io;
	struct sde_hdcp_int_set *isr;

	io = hdcp->init_data.dp_ahb;
	isr = &hdcp->int_set;

	intr_reg = DSS_REG_R(io, isr->int_reg);

	intr_reg |= HDCP_INT_CLR | HDCP_INT_EN;

	DSS_REG_W(io, isr->int_reg, intr_reg);
}

static int sde_hdcp_1x_read_bcaps(struct sde_hdcp_1x *hdcp)
{
	int rc;
	struct sde_hdcp_reg_set *reg_set = &hdcp->reg_set;
	struct dss_io_data *hdcp_io = hdcp->init_data.hdcp_io;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	rc = sde_hdcp_1x_read(hdcp, &hdcp->sink_addr.bcaps,
		&hdcp->bcaps, false);
	if (rc) {
		pr_err("error reading bcaps\n");
		goto error;
	}

	pr_debug("bcaps read: 0x%x\n", hdcp->bcaps);

	hdcp->current_tp.ds_type = hdcp->bcaps & reg_set->repeater ?
			DS_REPEATER : DS_RECEIVER;

	pr_debug("ds: %s\n", hdcp->current_tp.ds_type == DS_REPEATER ?
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
	struct dss_io_data *dp_ahb = hdcp->init_data.dp_ahb;
	struct dss_io_data *dp_aux = hdcp->init_data.dp_aux;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	/* Wait for HDCP keys to be checked and validated */
	rc = readl_poll_timeout(dp_ahb->base + reg_set->status, link0_status,
				((link0_status >> reg_set->keys_offset) & 0x7)
					== HDCP_KEYS_STATE_VALID ||
				!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (rc) {
		pr_err("key not ready\n");
		goto error;
	}

	/*
	 * 1.1_Features turned off by default.
	 * No need to write AInfo since 1.1_Features is disabled.
	 */
	DSS_REG_W(dp_aux, reg_set->data4, 0);

	/* Wait for An0 and An1 bit to be ready */
	rc = readl_poll_timeout(dp_ahb->base + reg_set->status, link0_status,
				(link0_status & (BIT(8) | BIT(9))) ||
				!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (rc) {
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

	pr_debug("an read: 0x%2x%2x%2x%2x%2x%2x%2x%2x\n",
		an[7], an[6], an[5], an[4], an[3], an[2], an[1], an[0]);

	rc = sde_hdcp_1x_write(hdcp, &hdcp->sink_addr.an, an);
	if (rc) {
		pr_err("error writing an to sink\n");
		goto error;
	}

	/* Copy An and AKSV to byte arrays for transmission */
	aksv[0] =  hdcp->aksv_0        & 0xFF;
	aksv[1] = (hdcp->aksv_0 >> 8)  & 0xFF;
	aksv[2] = (hdcp->aksv_0 >> 16) & 0xFF;
	aksv[3] = (hdcp->aksv_0 >> 24) & 0xFF;
	aksv[4] =  hdcp->aksv_1        & 0xFF;

	pr_debug("aksv read: 0x%2x%2x%2x%2x%2x\n",
		aksv[4], aksv[3], aksv[2], aksv[1], aksv[0]);

	rc = sde_hdcp_1x_write(hdcp, &hdcp->sink_addr.aksv, aksv);
	if (rc) {
		pr_err("error writing aksv to sink\n");
		goto error;
	}
error:
	return rc;
}

static int sde_hdcp_1x_read_an_aksv_from_hw(struct sde_hdcp_1x *hdcp)
{
	struct dss_io_data *dp_ahb = hdcp->init_data.dp_ahb;
	struct dss_io_data *dp_aux = hdcp->init_data.dp_aux;
	struct sde_hdcp_reg_set *reg_set = &hdcp->reg_set;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	hdcp->an_0 = DSS_REG_R(dp_ahb, reg_set->data5);
	if (hdcp->init_data.client_id == HDCP_CLIENT_DP) {
		udelay(1);
		hdcp->an_0 = DSS_REG_R(dp_ahb, reg_set->data5);
	}

	hdcp->an_1 = DSS_REG_R(dp_ahb, reg_set->data6);
	if (hdcp->init_data.client_id == HDCP_CLIENT_DP) {
		udelay(1);
		hdcp->an_1 = DSS_REG_R(dp_ahb, reg_set->data6);
	}

	/* Read AKSV */
	hdcp->aksv_0 = DSS_REG_R(dp_aux, reg_set->data3);
	hdcp->aksv_1 = DSS_REG_R(dp_aux, reg_set->data4);

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
	if (rc) {
		pr_err("error reading bksv from sink\n");
		goto error;
	}

	pr_debug("bksv read: 0x%2x%2x%2x%2x%2x\n",
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
	u8 const required_major = 1, required_minor = 2;
	u8 sink_major = 0, sink_minor = 0;
	u8 enable_hpd_irq = 0x1;
	int rc;
	unsigned char revision = *hdcp->init_data.revision;

	sink_major = (revision >> 4) & 0x0f;
	sink_minor = revision & 0x0f;
	pr_debug("revision: %d.%d\n", sink_major, sink_minor);

	if ((sink_minor < required_minor) || (sink_major < required_major) ||
	  (hdcp->current_tp.ds_type != DS_REPEATER)) {
		pr_debug("sink irq hpd not enabled\n");
		return;
	}

	rc = sde_hdcp_1x_write(hdcp, &hdcp->sink_addr.ainfo, &enable_hpd_irq);
	if (rc)
		pr_debug("error writing ainfo to sink\n");
}

static int sde_hdcp_1x_verify_r0(struct sde_hdcp_1x *hdcp)
{
	int rc, r0_retry = 3;
	u8 buf[2];
	u32 link0_status, timeout_count;
	u32 const r0_read_delay_us = 1;
	u32 const r0_read_timeout_us = r0_read_delay_us * 10;
	struct sde_hdcp_reg_set *reg_set = &hdcp->reg_set;
	struct dss_io_data *io = hdcp->init_data.dp_ahb;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	/* Wait for HDCP R0 computation to be completed */
	rc = readl_poll_timeout(io->base + reg_set->status, link0_status,
				(link0_status & BIT(reg_set->r0_offset)) ||
				!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (rc) {
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
		if (rc) {
			pr_err("error reading R0' from sink\n");
			goto error;
		}

		pr_debug("sink R0'read: %2x%2x\n", buf[1], buf[0]);

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
	if (rc) {
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
	if (rc) {
		pr_err("error reading bstatus\n");
		goto end;
	}

	bstatus = buf[1];
	bstatus = (bstatus << 8) | buf[0];

	device_count = bstatus & 0x7F;

	pr_debug("device count %d\n", device_count);

	/* Cascaded repeater depth */
	depth = (bstatus >> 8) & 0x7;
	pr_debug("depth %d\n", depth);

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
		if (rc)
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
	struct dss_io_data *io = hdcp->init_data.dp_ahb;
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
			if (rc) {
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
	if (rc) {
		pr_err("V computation not done\n");
		goto error;
	}

	/* Wait for V_MATCHES */
	rc = readl_poll_timeout(io->base + reg_set->status, status,
				(status & BIT(reg_set->v_offset)) ||
				!sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING),
				HDCP_POLL_SLEEP_US, HDCP_POLL_TIMEOUT_US);
	if (rc) {
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
	if (rc) {
		pr_err("error reading bcaps\n");
		goto error;
	}

	if (hdcp->init_data.client_id == HDCP_CLIENT_HDMI) {
		timeout = 50;

		while (!(hdcp->bcaps & BIT(5)) && --timeout) {
			rc = sde_hdcp_1x_read(hdcp,
				&hdcp->sink_addr.bcaps,
				&hdcp->bcaps, false);
			if (rc ||
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

static void sde_hdcp_1x_update_auth_status(struct sde_hdcp_1x *hdcp)
{
	if (IS_ENABLED(CONFIG_HDCP_QSEECOM) &&
			sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATED)) {
		msm_hdcp_cache_repeater_topology(hdcp->init_data.msm_hdcp_dev,
						&hdcp->current_tp);
		msm_hdcp_notify_topology(hdcp->init_data.msm_hdcp_dev);
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
	if (hdcp->init_data.client_id == HDCP_CLIENT_DP) {
		io = hdcp->init_data.dp_aux;
		DSS_REG_W(io, DP_DP_HPD_REFTIMER, 0x10013);
	}

	/*
	 * Program h/w to enable encryption as soon as authentication is
	 * successful. This is applicable for HDMI sinks and HDCP 1.x compliance
	 * test cases.
	 */
	if (hdcp->init_data.client_id == HDCP_CLIENT_HDMI ||
			hdcp->force_encryption)
		hdcp1_set_enc(hdcp->hdcp1_handle, true);

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

	/*
	 * Disabling software DDC before going into part3 to make sure
	 * there is no Arbitration between software and hardware for DDC
	 */
end:
	if (rc && !sde_hdcp_1x_state(HDCP_STATE_INACTIVE))
		hdcp->hdcp_state = HDCP_STATE_AUTH_FAIL;

	sde_hdcp_1x_update_auth_status(hdcp);
}

static int sde_hdcp_1x_authenticate(void *input)
{
	struct sde_hdcp_1x *hdcp = (struct sde_hdcp_1x *)input;
	int rc = 0;

	if (!hdcp) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	flush_delayed_work(&hdcp->hdcp_auth_work);

	if (!sde_hdcp_1x_state(HDCP_STATE_INACTIVE)) {
		pr_err("invalid state\n");
		rc = -EINVAL;
		goto error;
	}

	rc = hdcp1_start(hdcp->hdcp1_handle, &hdcp->aksv_msb, &hdcp->aksv_lsb);
	if (rc) {
		pr_err("hdcp1_start failed (%d)\n", rc);
		goto error;
	}

	if (!sde_hdcp_1x_enable_hdcp_engine(input)) {

		queue_delayed_work(hdcp->workq,
			&hdcp->hdcp_auth_work, HZ/2);
	} else {
		hdcp->hdcp_state = HDCP_STATE_AUTH_FAIL;
		sde_hdcp_1x_update_auth_status(hdcp);
	}

error:
	return rc;
} /* hdcp_1x_authenticate */

static int sde_hdcp_1x_reauthenticate(void *input)
{
	struct sde_hdcp_1x *hdcp = (struct sde_hdcp_1x *)input;
	struct dss_io_data *io;
	struct sde_hdcp_reg_set *reg_set;
	struct sde_hdcp_int_set *isr;
	u32 reg;

	if (!hdcp || !hdcp->init_data.dp_ahb) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	io = hdcp->init_data.dp_ahb;
	reg_set = &hdcp->reg_set;
	isr = &hdcp->int_set;

	if (!sde_hdcp_1x_state(HDCP_STATE_AUTH_FAIL)) {
		pr_err("invalid state\n");
		return -EINVAL;
	}

	/* Disable HDCP interrupts */
	DSS_REG_W(io, isr->int_reg, DSS_REG_R(io, isr->int_reg) & ~HDCP_INT_EN);

	reg = DSS_REG_R(io, reg_set->reset);
	DSS_REG_W(io, reg_set->reset, reg | reg_set->reset_bit);

	/* Disable encryption and disable the HDCP block */
	DSS_REG_W(io, reg_set->ctrl, 0);

	DSS_REG_W(io, reg_set->reset, reg & ~reg_set->reset_bit);

	hdcp->hdcp_state = HDCP_STATE_INACTIVE;

	return sde_hdcp_1x_authenticate(hdcp);
} /* hdcp_1x_reauthenticate */

static void sde_hdcp_1x_off(void *input)
{
	struct sde_hdcp_1x *hdcp = (struct sde_hdcp_1x *)input;
	struct dss_io_data *io;
	struct sde_hdcp_reg_set *reg_set;
	struct sde_hdcp_int_set *isr;
	int rc = 0;
	u32 reg;

	if (!hdcp || !hdcp->init_data.dp_ahb) {
		pr_err("invalid input\n");
		return;
	}

	io = hdcp->init_data.dp_ahb;
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
	DSS_REG_W(io, isr->int_reg,
		DSS_REG_R(io, isr->int_reg) & ~HDCP_INT_EN);
	hdcp->hdcp_state = HDCP_STATE_INACTIVE;

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
		pr_debug("%s: Deleted hdcp auth work\n",
			SDE_HDCP_STATE_NAME);

	if (hdcp->init_data.client_id == HDCP_CLIENT_HDMI ||
			hdcp->force_encryption)
		hdcp1_set_enc(hdcp->hdcp1_handle, false);

	reg = DSS_REG_R(io, reg_set->reset);
	DSS_REG_W(io, reg_set->reset, reg | reg_set->reset_bit);

	/* Disable encryption and disable the HDCP block */
	DSS_REG_W(io, reg_set->ctrl, 0);

	DSS_REG_W(io, reg_set->reset, reg & ~reg_set->reset_bit);

	hdcp->sink_r0_ready = false;

	hdcp1_stop(hdcp->hdcp1_handle);

	pr_debug("%s: HDCP: Off\n", SDE_HDCP_STATE_NAME);
} /* hdcp_1x_off */

static int sde_hdcp_1x_isr(void *input)
{
	struct sde_hdcp_1x *hdcp = (struct sde_hdcp_1x *)input;
	int rc = 0;
	struct dss_io_data *io;
	u32 hdcp_int_val;
	struct sde_hdcp_reg_set *reg_set;
	struct sde_hdcp_int_set *isr;

	if (!hdcp || !hdcp->init_data.dp_ahb) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	io = hdcp->init_data.dp_ahb;
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
		pr_debug("%s: AUTH SUCCESS\n", SDE_HDCP_STATE_NAME);

		if (sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING))
			complete_all(&hdcp->r0_checked);
	}

	if (hdcp_int_val & isr->auth_fail_int) {
		/* AUTH_FAIL_INT */
		u32 link_status = DSS_REG_R(io, reg_set->status);

		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->auth_fail_ack));

		pr_debug("%s: AUTH FAIL, LINK0_STATUS=0x%08x\n",
			SDE_HDCP_STATE_NAME, link_status);

		if (sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATED)) {
			hdcp->hdcp_state = HDCP_STATE_AUTH_FAIL;
			sde_hdcp_1x_update_auth_status(hdcp);
		} else if (sde_hdcp_1x_state(HDCP_STATE_AUTHENTICATING)) {
			complete_all(&hdcp->r0_checked);
		}

		/* Clear AUTH_FAIL_INFO as well */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->auth_fail_info_ack));
	}

	if (hdcp_int_val & isr->tx_req_int) {
		/* DDC_XFER_REQ_INT */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->tx_req_ack));
		pr_debug("%s: DDC_XFER_REQ_INT received\n",
			SDE_HDCP_STATE_NAME);
	}

	if (hdcp_int_val & isr->tx_req_done_int) {
		/* DDC_XFER_DONE_INT */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->tx_req_done_ack));
		pr_debug("%s: DDC_XFER_DONE received\n",
			SDE_HDCP_STATE_NAME);
	}

	if (hdcp_int_val & isr->encryption_ready) {
		/* Encryption enabled */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->encryption_ready_ack));
		pr_debug("%s: encryption ready received\n",
			SDE_HDCP_STATE_NAME);
	}

	if (hdcp_int_val & isr->encryption_not_ready) {
		/* Encryption enabled */
		DSS_REG_W(io, isr->int_reg,
			(hdcp_int_val | isr->encryption_not_ready_ack));
		pr_debug("%s: encryption not ready received\n",
			SDE_HDCP_STATE_NAME);
	}

error:
	return rc;
}

static bool sde_hdcp_1x_feature_supported(void *input)
{
	struct sde_hdcp_1x *hdcp = (struct sde_hdcp_1x *)input;
	bool feature_supported = false;

	if (!hdcp) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	feature_supported = hdcp1_feature_supported(hdcp->hdcp1_handle);

	pr_debug("feature_supported = %d\n", feature_supported);

	return feature_supported;
}

static void sde_hdcp_1x_force_encryption(void *input, bool enable)
{
	struct sde_hdcp_1x *hdcp = (struct sde_hdcp_1x *)input;

	if (!hdcp) {
		pr_err("invalid input\n");
		return;
	}
	hdcp->force_encryption = enable;
	pr_info("force_encryption=%d\n", hdcp->force_encryption);
}

static bool sde_hdcp_1x_sink_support(void *input)
{
	return true;
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

	hdcp1_deinit(hdcp->hdcp1_handle);

	kfree(hdcp);
} /* hdcp_1x_deinit */

static void sde_hdcp_1x_update_client_reg_set(struct sde_hdcp_1x *hdcp)
{
	if (hdcp->init_data.client_id == HDCP_CLIENT_DP) {
		struct sde_hdcp_reg_set reg_set = HDCP_REG_SET_CLIENT_DP;
		struct sde_hdcp_sink_addr_map sink_addr = HDCP_DP_SINK_ADDR_MAP;
		struct sde_hdcp_int_set isr = HDCP_DP_INT_SET;

		hdcp->reg_set = reg_set;
		hdcp->sink_addr = sink_addr;
		hdcp->int_set = isr;
	}
}

static bool sde_hdcp_1x_is_cp_irq_raised(struct sde_hdcp_1x *hdcp)
{
	int ret;
	u8 buf = 0;
	struct sde_hdcp_sink_addr sink = {"irq", 0x201, 1};

	ret = sde_hdcp_1x_read(hdcp, &sink, &buf, false);
	if (ret)
		pr_err("error reading irq_vector\n");

	return buf & BIT(2) ? true : false;
}

static void sde_hdcp_1x_clear_cp_irq(struct sde_hdcp_1x *hdcp)
{
	int ret;
	u8 buf = BIT(2);
	struct sde_hdcp_sink_addr sink = {"irq", 0x201, 1};

	ret = sde_hdcp_1x_write(hdcp, &sink, &buf);
	if (ret)
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
		pr_debug("cp_irq not raised\n");
		goto irq_not_handled;
	}

	ret = sde_hdcp_1x_read(hdcp, &hdcp->sink_addr.cp_irq_status,
			&buf, false);
	if (ret) {
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
		pr_debug("R0' AVAILABLE\n");
		hdcp->sink_r0_ready = true;
		complete_all(&hdcp->sink_r0_available);
	} else if ((buf & BIT(0))) {
		pr_debug("KSVs READY\n");

		hdcp->ksv_ready = true;
	} else {
		pr_debug("spurious interrupt\n");
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
		.feature_supported = sde_hdcp_1x_feature_supported,
		.force_encryption = sde_hdcp_1x_force_encryption,
		.sink_support = sde_hdcp_1x_sink_support,
		.off = sde_hdcp_1x_off
	};

	if (!init_data || !init_data->notify_status ||
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
		goto workqueue_error;
	}

	hdcp->hdcp1_handle = hdcp1_init();
	if (!hdcp->hdcp1_handle) {
		pr_err("Error creating HDCP 1.x handle\n");
		goto hdcp1_handle_error;
	}

	sde_hdcp_1x_update_client_reg_set(hdcp);

	INIT_DELAYED_WORK(&hdcp->hdcp_auth_work, sde_hdcp_1x_auth_work);

	hdcp->hdcp_state = HDCP_STATE_INACTIVE;
	init_completion(&hdcp->r0_checked);
	init_completion(&hdcp->sink_r0_available);
	hdcp->force_encryption = false;

	pr_debug("HDCP module initialized. HDCP_STATE=%s\n",
		SDE_HDCP_STATE_NAME);

	return (void *)hdcp;
hdcp1_handle_error:
	destroy_workqueue(hdcp->workq);
workqueue_error:
	kfree(hdcp);
error:
	return NULL;
} /* hdcp_1x_init */

struct sde_hdcp_ops *sde_hdcp_1x_get(void *input)
{
	return ((struct sde_hdcp_1x *)input)->ops;
}
