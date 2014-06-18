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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/i2c.h>

#include "psb_powermgmt.h"

#include "hdmi_hdcp.h"
#include "hdmi_edid.h"

enum hdcp_event {
	HDCP_ENABLE = 1,
	HDCP_RESET,
	HDCP_RI_CHECK,
	HDCP_REPEATER_CHECK,
	HDCP_REPEATER_WDT_EXPIRED,
	HDCP_SET_POWER_SAVE_STATUS,
	HDCP_SET_HPD_STATUS,
	HDCP_SET_DPMS_STATUS
};

struct hdcp_rx_bcaps_t {
	union {
		uint8_t value;
		struct {
			uint8_t fast_reauthentication:1;
			uint8_t b1_1_features:1;
			uint8_t reserved:2;
			uint8_t fast_transfer:1;
			uint8_t ksv_fifo_ready:1;
			uint8_t is_repeater:1;
			uint8_t hdmi_reserved:1;
		};
	};
};

struct hdcp_rx_bstatus_t {
	union {
		uint16_t value;
		struct {
			uint16_t device_count:7;
			uint16_t max_devs_exceeded:1;
			uint16_t depth:3;
			uint16_t max_cascade_exceeded:1;
			uint16_t hdmi_mode:1;
			uint16_t reserved2:1;
			uint16_t rsvd:2;
		};
	};
};
/* = = = = = = = = = = = = = = = = == = = = = = = = = = = = = = = = = = = = = */
/*!  \brief Our local context.
 */

typedef struct hdcp_context_s {
	unsigned int auth_id;
	bool	is_enabled;/*if the hdcp context is enabled.*/
	bool is_hdmi; /* HDMI or DVI*/

	/*authentication stages enabled flag*/
	bool	is_phase1_enabled;
	bool	is_phase2_enabled;
	bool	is_phase3_valid;
	bool	previous_phase1_status;

	/*stage 3 related members*/
	bool	wdt_expired;
	bool    force_reset;

	/* time delay (msec) to re-try Ri check */
	unsigned int ri_retry;
	/* phase 3 ri-check interval based on mode*/
	unsigned int	ri_check_interval;
	/* upper bound of ri-check interval */
	unsigned int	ri_check_interval_upper;
	/* lower bound of ri-check interval */
	unsigned int	ri_check_interval_lower;
	/*Ri frame count in HDCP status register when doing previous Ri check*/
	unsigned int	prev_ri_frm_cnt_status;
	/* time interval (msec) of video refresh. */
	unsigned int	video_refresh_interval;
	/* the bksv from the down stream machine */
	uint8_t bksv[HDCP_KSV_SIZE];

	/* single-thread workqueue handling HDCP events */
	struct workqueue_struct *hdcp_event_handler;

	struct hdmi_pipe *pipe;/*the hdmi pipe.*/
} hdcp_context;

/*the event message for HDCP*/
typedef struct hdcp_event_msg_s {
	struct delayed_work dwork;
	int msg;/*the event ID of HDCP*/
	void *data;/*the data of HDCP event*/
} hdcp_event_msg;

static hdcp_context *g_pstHdcpContext; /* global context. */

static int hdcp_ddc_read(uint8_t offset, uint8_t *buffer, int size);
static int hdcp_ddc_write(uint8_t offset, uint8_t *buffer, int size);
static int hdmi_ddc_read_write(bool read,
							uint8_t i2c_addr,
							uint8_t offset,
							uint8_t *buffer,
							int size);

static bool hdcp_validate_ksv(uint8_t *ksv, uint32_t size);
static bool hdcp_get_aksv(uint8_t *aksv, uint32_t size);

static bool hdcp_enable_condition_ready(void);
static void hdcp_task_event_handler(struct work_struct *work);

static bool hdcp_wq_start(void);
static bool hdcp_wq_ri_check(void);
static bool hdcp_wq_repeater_authentication(void);
static void hdcp_wq_reset(void);

static bool hdcp_stage1_authentication(bool *is_repeater);
static bool hdcp_stage2_start_repeater_authentication(void);
static bool hdcp_stage3_schedule_ri_check(bool first_check);
static bool hdcp_stage3_ri_check(void);

static bool hdcp_rep_check(bool first);
static bool hdcp_rep_watch_dog(void);

static bool wq_send_message_delayed(int msg, void *msg_data,
				   unsigned long delay);
static bool wq_send_message(int msg, void *msg_data);

/**
 * Description: this function reads & write data to i2c hdcp device
 * @read        is read or wirte.
 * @i2c_addr  HDCP_PRIMARY_I2C_ADDR
 * @offset	offset address on hdcp device
 * @buffer	buffer to store data
 * @size	size of buffer to be read
 *
 * Returns:	true on success else false
 */
static int hdmi_ddc_read_write(bool read,
			uint8_t i2c_addr,
			uint8_t offset,
			uint8_t *buffer,
			int size)
{
	struct i2c_adapter *adapter = i2c_get_adapter(HDMI_I2C_ADAPTER_NUM);
	struct i2c_msg msgs[2];
	int num_of_msgs = 0;
	uint8_t wr_buffer[HDMI_MAX_DDC_WRITE_SIZE];

	/* Use one i2c message to write and two to read as some
	 * monitors don't handle two write messages properly
	*/
	if (read) {
		msgs[0].addr   = i2c_addr,
		msgs[0].flags  = 0,
		msgs[0].len    = 1,
		msgs[0].buf    = &offset,

		msgs[1].addr   = i2c_addr,
		msgs[1].flags  = ((read) ? I2C_M_RD : 0),
		msgs[1].len    = size,
		msgs[1].buf    = buffer,

		num_of_msgs = 2;
	} else {
		BUG_ON(size + 1 > HDMI_MAX_DDC_WRITE_SIZE);

		wr_buffer[0] = offset;
		memcpy(&wr_buffer[1], buffer, size);

		msgs[0].addr   = i2c_addr,
		msgs[0].flags  = 0,
		msgs[0].len    = size + 1,
		msgs[0].buf    = wr_buffer,

		num_of_msgs = 1;
	}

	if (adapter != NULL && i2c_transfer(adapter, msgs, num_of_msgs) ==
								num_of_msgs)
		return 1;

	return 0;
}

/**
 * Description: this function reads data from downstream i2c hdcp device
 *
 * @offset	offset address on hdcp device
 * @buffer	buffer to store data
 * @size	size of buffer to be read
 *
 * Returns:	true on success else false
 */
static int hdcp_ddc_read(uint8_t offset, uint8_t *buffer, int size)
{
	if (hdcp_enable_condition_ready() == true ||
		(g_pstHdcpContext->is_enabled == true &&
		 offset == HDCP_RX_BKSV_ADDR))
		return hdmi_ddc_read_write(true, HDCP_PRIMARY_I2C_ADDR,
			offset, buffer, size);
	return false;
}

/**
 * Description: this function writes data to downstream i2c hdcp device
 *
 * @offset	offset address on hdcp device
 * @buffer	data to be written
 * @size	size of data to be written
 *
 * Returns:	true on success else false
 */
static int hdcp_ddc_write(uint8_t offset, uint8_t *buffer, int size)
{
	if (hdcp_enable_condition_ready() == true)
		return hdmi_ddc_read_write(false,	 HDCP_PRIMARY_I2C_ADDR,
			offset, buffer, size);
	return false;
}

/**
 * Description: this function validates a ksv value
 *		1. 20 1's & 20 o's
 *		2. SRM check: check for revoked keys
 *
 * @ksv		ksv value
 * @size	size of the ksv
 *
 * Returns:	true if valid else false
 */
static bool hdcp_validate_ksv(uint8_t *ksv, uint32_t size)
{
	int i = 0, count = 0;
	uint8_t temp = 0;
	bool ret = false;
	if (ksv != NULL  && size == HDCP_KSV_SIZE) {
		count = 0;
		for (i = 0; i < 5; i++) {
			temp = ksv[i];
			while (temp) {
				temp &= (temp-1);
				count++;
			}
		}
		if (count == HDCP_KSV_HAMMING_WT)
			ret = true;
	}
	return ret;
}

/**
 * Description: this function reads aksv from local hdcp tx device
 *
 * @aksv	buffer to store aksv
 * @size	size of the aksv buffer
 *
 * Returns:	true on success else false
 */
static bool hdcp_get_aksv(uint8_t *aksv, uint32_t size)
{
	bool ret = false;
	if (mofd_hdcp_get_aksv(aksv, HDCP_KSV_SIZE) == true) {
		if (hdcp_validate_ksv(aksv, size) == true)
			ret = true;
	}
	return ret;
}

/**
 * Description: this function reads bksv from downstream device
 *
 * @bksv	buffer to store bksv
 * @size	size of the bksv buffer
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_bksv(uint8_t *bksv, uint32_t size)
{
	bool ret = false;
	if (bksv != NULL  && size == HDCP_KSV_SIZE) {
		if (hdcp_ddc_read(HDCP_RX_BKSV_ADDR,
				bksv, HDCP_KSV_SIZE) == true) {
			if (hdcp_validate_ksv(bksv, size) == true)
				ret = true;
		}
	}
	return ret;
}

/**
 * Description: this function reads all ksv's from downstream repeater
 *
 * @ksv_list	buffer to store ksv list
 * @size	size of the ksv_list to read into the buffer
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_rx_ksv_list(uint8_t *ksv_list, uint32_t size)
{
	bool ret = false;
	if (ksv_list != NULL && size) {
		if (hdcp_ddc_read(HDCP_RX_KSV_FIFO_ADDR, ksv_list, size) ==
		    true)
			ret = true;
	}
	return ret;
}

/**
 * Description: this function reads bcaps from downstream device
 *
 * @bcaps	buffer to store bcaps
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_bcaps(uint8_t *bcaps)
{
	bool ret = false;
	if (bcaps != NULL) {
		if (hdcp_ddc_read(HDCP_RX_BCAPS_ADDR,
				bcaps, HDCP_RX_BCAPS_SIZE) == true)
			ret = true;
	}
	return ret;
}

/**
 * Description: this function reads bstatus from downstream device
 *
 * @bstatus	buffer to store bstatus
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_bstatus(uint16_t *bstatus)
{
	bool ret = false;
	if (bstatus != NULL) {
		if (hdcp_ddc_read(HDCP_RX_BSTATUS_ADDR,
			(uint8_t *)bstatus, HDCP_RX_BSTATUS_SIZE) == true)
			ret = true;
	}
	return ret;
}
/**
 * Description: this function reads ri from downstream device
 *
 * @rx_ri	buffer to store ri
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_rx_ri(uint16_t *rx_ri)
{
	bool ret = false;
	if (rx_ri != NULL) {
		if (hdcp_ddc_read(HDCP_RX_RI_ADDR,
				(uint8_t *)rx_ri, HDCP_RI_SIZE) == true)
			ret = true;
	}
	return ret;
}

/**
 * Description: this function reads r0 from downstream device
 *
 * @rx_r0	buffer to store r0
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_rx_r0(uint16_t *rx_r0)
{
	return hdcp_read_rx_ri(rx_r0);
}
/**
 * Description: this function reads sha1 value from downstream device
 *
 * @v		buffer to store the sha1 value
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_rx_v(uint8_t *v)
{
	bool ret = false;
	uint8_t *buf = v;
	uint8_t offset = HDCP_RX_V_H0_ADDR;

	if (v != NULL) {
		for (; offset <= HDCP_RX_V_H4_ADDR; offset += 4) {
			if (hdcp_ddc_read(offset, buf, 4) == false) {
				pr_debug("hdcp: read rx v failure\n");
				break;
			}
			buf += 4;
		}
		if (offset > HDCP_RX_V_H4_ADDR)
			ret = true;
	}
	return ret;
}

/**
 * Description: this function sends an aksv to downstream device
 *
 * @an		AN value to send
 * @an_size	size of an
 * @aksv	AKSV value to send
 * @aksv_size	size of aksv
 *
 * Returns:	true on success else false
 */
static bool hdcp_send_an_aksv(uint8_t *an, uint8_t an_size,
			uint8_t *aksv, uint8_t aksv_size)
{
	bool ret = false;
	if (an != NULL && an_size == HDCP_AN_SIZE &&
	   aksv != NULL  && aksv_size == HDCP_KSV_SIZE) {
		if (hdcp_ddc_write(HDCP_RX_AN_ADDR, an, HDCP_AN_SIZE) ==
			true) {
			/* wait 20ms for i2c write for An to complete */
			/* msleep(20); */
			if (hdcp_ddc_write(HDCP_RX_AKSV_ADDR, aksv,
					HDCP_KSV_SIZE) == true)
				ret = true;
		}
	}
	return ret;
}

/**
 * Description: verify conditions necessary for re-authentication and
 *		enable HDCP if favourable
 *
 * Returns:	none
 */
static void hdcp_retry_enable(void)
{
	int msg = HDCP_ENABLE;
	if (hdcp_enable_condition_ready() == true &&
		g_pstHdcpContext->is_phase1_enabled == false) {
		wq_send_message_delayed(msg, NULL, 30);
		pr_debug("hdcp: retry enable\n");
	}
}

/**
 * Description: this function verifies all conditions to enable hdcp
 *
 * Returns:	true if hdcp can be enabled else false
 */
static bool hdcp_enable_condition_ready(void)
{
	int is_connected =
		atomic_read(&(g_pstHdcpContext->pipe->hpd_ctx.is_connected));
	if (g_pstHdcpContext != NULL &&
	    g_pstHdcpContext->is_enabled == true &&
	    (bool)is_connected == true)
		return true;

	if (g_pstHdcpContext == NULL) {
		pr_err("[%s,%d]hdcp: hdcp_context is NULL\n",
		       __FILE__, __LINE__);
	} else {
		pr_err("[%s,%d]hdcp: condition not ready, enabled %d\n",
			__FILE__, __LINE__,
			g_pstHdcpContext->is_enabled);
	}

	return false;
}

/**
 * Description: Main function that initiates all stages of HDCP authentication
 *
 * Returns:	true on succesful authentication else false
 */
static bool hdcp_wq_start(void)
{
	bool is_repeater = false;
	hdcp_context *ctx = g_pstHdcpContext;

	if (ctx == NULL) {
		pr_debug("[%s,%d]hdcp: context was not initialized.\n",
			 __FILE__, __LINE__);
		return false;
	}

	/* Make sure TMDS is available
	 * Remove this delay since HWC already has the delay
	 */

	/* Increment Auth Check Counter */
	ctx->auth_id++;

	/* Check HDCP Status */
	if (mofd_hdcp_is_ready() == false) {
		pr_err("hdcp: hdcp is not ready\n");
		return false;
	}


	/* start 1st stage of hdcp authentication */
	if (hdcp_stage1_authentication(&is_repeater) == false) {
		pr_debug("hdcp: stage 1 authentication fails\n");
		return false;
	}

	pr_debug("hdcp: initial authentication completed, repeater:%d\n",
		is_repeater);

	/* Branch Repeater Mode Authentication */
	if (is_repeater == true)
		if (hdcp_stage2_start_repeater_authentication() == false)
			return false;

	/* Initiate phase3_valid with true status */
	g_pstHdcpContext->is_phase3_valid = true;
	/* Branch Periodic Ri Check */
	pr_debug("hdcp: starting periodic Ri check\n");

	/* Schedule Ri check after 2 sec*/
	if (hdcp_stage3_schedule_ri_check(false) == false) {
		pr_err("hdcp: fail to schedule Ri check\n");
		return false;
	}

	return true;
}
/* Based on hardware Ri frame count, adjust ri_check_interval.
 * Also, make sure Ri check happens right after Ri frame count
 * becomes multiples of 128.
 *  */
static bool hdcp_wq_ri_check(void)
{
	#define RI_FRAME_WAIT_LIMIT 150

	hdcp_context *ctx = g_pstHdcpContext;
	uint32_t prev_ri_frm_cnt_status = ctx->prev_ri_frm_cnt_status;
	uint8_t  ri_frm_cnt_status;
	int32_t  ri_frm_cnt;
	int32_t  adj;  /* Adjustment of ri_check_interval in msec */
	uint32_t cnt_ri_wait = 0;
	bool     ret = false;


	/* Query hardware Ri frame counter.
	 * This value is used to adjust ri_check_interval
	 * */
	mofd_hdcp_get_ri_frame_count(&ri_frm_cnt_status);
	/* (frm_cnt_ri - prev_frm_cnt_ri) is expected to be 128. If not,
	 * we have to compensate the time difference, which is caused by async
	 * behavior of CPU clock, scheduler and HDMI clock. If hardware can
	 * provide interrupt signal for Ri check, then this compensation work
	 * can be avoided.
	 * Hardcode "256" is because hardware Ri frame counter is 8 bits.
	 * Hardcode "128" is based on HDCP spec.
	* */
	ri_frm_cnt = ri_frm_cnt_status >= prev_ri_frm_cnt_status      ?
		ri_frm_cnt_status - prev_ri_frm_cnt_status       :
		256 - prev_ri_frm_cnt_status + ri_frm_cnt_status;
	pr_debug("current ri_frm_cnt = %d, previous ri_frm_cnt = %d\n",
			  ri_frm_cnt_status, prev_ri_frm_cnt_status);

	/* Compute adjustment of ri_check_interval*/
	adj = (128 - ri_frm_cnt) * ctx->video_refresh_interval;

	/* Adjust ri_check_interval */
	/* adj<0:  Ri check speed is slower than HDMI clock speed
	 * adj>0:  Ri check speed is faster than HDMI clock speed
	 * */
	pr_debug("adjustment of ri_check_interval  = %d (ms)\n", adj);
	ctx->ri_check_interval += adj;
	if (ctx->ri_check_interval > ctx->ri_check_interval_upper)
		ctx->ri_check_interval = ctx->ri_check_interval_upper;

	if (ctx->ri_check_interval < ctx->ri_check_interval_lower)
		ctx->ri_check_interval = ctx->ri_check_interval_lower;

	pr_debug("ri_check_interval=%d(ms)\n", ctx->ri_check_interval);

	/* Update prev_ri_frm_cnt_status*/
	ctx->prev_ri_frm_cnt_status = ri_frm_cnt_status;

	/* Queue next Ri check task with new ri_check_interval*/
	ret = hdcp_stage3_schedule_ri_check(false);
	if (!ret)
		goto exit;

	/* Now, check if ri_frm_cnt_status is multiples of 128.
	 * If we are too fast, wait for frame 128 (or a few frames after
	 * frame 128) to happen to make sure Ri' is ready.
	 * Why using hardcode "64"? : if ri_frm_cnt_status is close to
	 * multiples of 128 (ie, ri_frm_cnt_status % 128 > 64), we keep waiting.
	 * Otherwise if ri_frm_cnt_status just passes 128
	 * (ie, ri_frm_cnt_status % 128 < 64) we continue.
	 * Note the assumption here is this thread is scheduled to run at least
	 * once in one 64-frame period.
	 *
	 * RI_FRAME_WAIT_LIMIT is in case HW stops updating ri frame
	 * count and causes infinite looping.
	*/
	while ((ri_frm_cnt_status % 128 >= 64) &&
			(cnt_ri_wait < RI_FRAME_WAIT_LIMIT)) {
		msleep(ctx->video_refresh_interval);
		mofd_hdcp_get_ri_frame_count(&ri_frm_cnt_status);
		cnt_ri_wait++;
		pr_debug("current Ri frame count = %d\n", ri_frm_cnt_status);
	}

	if (RI_FRAME_WAIT_LIMIT == cnt_ri_wait) {
		ret = false;
		goto exit;
	}

	/* Match Ri with Ri'*/
	ret = hdcp_stage3_ri_check();

exit:
	return ret;
}

/**
 * Description: this function performs repeater authentication i.e. 2nd
 *		stage HDCP authentication
 *
 * Returns:	true if repeater authentication is in progress or succesful
 *		else false. If in progress repeater authentication would be
 *		rescheduled
 */
static bool hdcp_wq_repeater_authentication(void)
{
	uint8_t *rep_ksv_list = NULL;
	uint32_t rep_prime_v[HDCP_V_H_SIZE] = {0};
	struct hdcp_rx_bstatus_t bstatus;
	struct hdcp_rx_bcaps_t bcaps;
	bool ret = false;

	/* Repeater Authentication */
	if (hdcp_enable_condition_ready() == false ||
	    g_pstHdcpContext->is_phase1_enabled == false ||
	    g_pstHdcpContext->wdt_expired == true) {
		pr_debug("hdcp: stage2 auth condition not ready\n");
		return false;
	}

	/* Read BCAPS */
	if (hdcp_read_bcaps(&bcaps.value) == false)
		return false;

	if (!bcaps.is_repeater)
		return false;

	/* Check if fifo ready */
	if (!bcaps.ksv_fifo_ready) {
		/* not ready: reschedule but return true */
		pr_debug("hdcp: rescheduling repeater auth\n");
		hdcp_rep_check(false);
		return true;
	}

	/* Read BSTATUS */
	if (hdcp_read_bstatus(&bstatus.value) == false)
		return false;

	/* Check validity of repeater depth & device count */
	if (bstatus.max_devs_exceeded)
		return false;

	if (bstatus.max_cascade_exceeded)
		return false;

	if (0 == bstatus.device_count)
		return true;

	if (bstatus.device_count > HDCP_MAX_DEVICES)
		return false;

	/* allocate memory for ksv_list */
	rep_ksv_list = kzalloc(bstatus.device_count * HDCP_KSV_SIZE,
				GFP_KERNEL);
	if (!rep_ksv_list) {
		pr_debug("hdcp: rep ksv list alloc failure\n");
		return false;
	}

	/* Read ksv list from repeater */
	if (hdcp_read_rx_ksv_list(rep_ksv_list,
				  bstatus.device_count * HDCP_KSV_SIZE)
				  == false) {
		pr_debug("hdcp: rep ksv list read failure\n");
		goto exit;
	}

	/* TODO: SRM check */

	/* Compute tx sha1 (V) */
	if (mofd_hdcp_compute_tx_v(rep_ksv_list,
				   bstatus.device_count, bstatus.value) ==
				   false) {
		pr_debug("hdcp: rep compute tx v failure\n");
		goto exit;
	}

	/* Read rx sha1 (V') */
	if (hdcp_read_rx_v((uint8_t *)rep_prime_v) == false) {
		pr_debug("hdcp: rep read rx v failure\n");
		goto exit;
	}

	/* Verify SHA1 tx(V) = rx(V') */
	if (mofd_hdcp_compare_v(rep_prime_v) == false) {
		pr_debug("hdcp: rep compare v failure\n");
		goto exit;
	}

	pr_debug("hdcp: repeater auth success\n");
	g_pstHdcpContext->is_phase2_enabled = true;
	ret = true;

exit:
	kfree(rep_ksv_list);
	return ret;
}

/**
 * Description: this function resets hdcp state machine to initial state
 *
 * Returns:	none
 */
static void hdcp_wq_reset(void)
{
	pr_debug("hdcp: reset\n");

	/* blank TV screen */

	/* Stop HDCP */
	if (g_pstHdcpContext->is_phase1_enabled == true ||
	    g_pstHdcpContext->force_reset == true) {
		pr_debug("hdcp: off state\n");
		mofd_hdcp_disable();
		g_pstHdcpContext->force_reset = false;
	}

	g_pstHdcpContext->is_phase1_enabled = false;
	g_pstHdcpContext->is_phase2_enabled = false;
	g_pstHdcpContext->is_phase3_valid   = false;
	g_pstHdcpContext->prev_ri_frm_cnt_status = 0;
	memset(g_pstHdcpContext->bksv, 0, HDCP_KSV_SIZE);
}

/**
 * Description: this function performs 1st stage HDCP authentication i.e.
 *		exchanging keys and r0 match
 *
 * @is_repeater	variable to return type of downstream device, i.e. repeater
 *		or not
 *
 * Returns:	true on successful authentication else false
 */
static bool hdcp_stage1_authentication(bool *is_repeater)
{
	uint8_t bksv[HDCP_KSV_SIZE], aksv[HDCP_KSV_SIZE], an[HDCP_AN_SIZE];
	struct hdcp_rx_bstatus_t bstatus;
	struct hdcp_rx_bcaps_t bcaps;
	uint8_t retry = 0;
	uint16_t rx_r0 = 0;
	hdcp_context *ctx = g_pstHdcpContext;

	/* Wait (up to 2s) for HDMI sink to be in HDMI mode */
	retry = 40;
	if (ctx->is_hdmi) {
		while (retry--) {
			if (hdcp_read_bstatus(&bstatus.value) == false) {
				pr_err("hdcp: failed to read bstatus\n");
				return false;
			}
			if (bstatus.hdmi_mode)
				break;
			msleep(50);
			pr_debug("hdcp: waiting for sink to be in HDMI mode\n");
		}
	}

	if (retry == 0)
		pr_err("hdcp: sink is not in HDMI mode\n");

	pr_debug("hdcp: bstatus: %04x\n", bstatus.value);

	/* Read BKSV */
	if (hdcp_read_bksv(bksv, HDCP_KSV_SIZE) == false) {
		pr_err("hdcp: failed to read bksv\n");
		return false;
	}
	pr_debug("hdcp: bksv: %02x%02x%02x%02x%02x\n",
		bksv[0], bksv[1], bksv[2], bksv[3], bksv[4]);

	memcpy(ctx->bksv, bksv, HDCP_KSV_SIZE);

	/* Read An */
	if (mofd_hdcp_get_an(an, HDCP_AN_SIZE) == false) {
		pr_err("hdcp: failed to get an\n");
		return false;
	}
	pr_debug("hdcp: an: %02x%02x%02x%02x%02x%02x%02x%02x\n",
		an[0], an[1], an[2], an[3], an[4], an[5], an[6], an[7]);

	/* Read AKSV */
	if (hdcp_get_aksv(aksv, HDCP_KSV_SIZE) == false) {
		pr_err("hdcp: failed to get aksv\n");
		return false;
	}
	pr_debug("hdcp: aksv: %02x%02x%02x%02x%02x\n",
			aksv[0], aksv[1], aksv[2], aksv[3], aksv[4]);

	/* Write An AKSV to Downstream Rx */
	if (hdcp_send_an_aksv(an, HDCP_AN_SIZE, aksv, HDCP_KSV_SIZE)
						== false) {
		pr_err("hdcp: failed to send an and aksv\n");
		return false;
	}
	pr_debug("hdcp: sent an aksv\n");

	/* Read BKSV */
	if (hdcp_read_bksv(bksv, HDCP_KSV_SIZE) == false) {
		pr_err("hdcp: failed to read bksv\n");
		return false;
	}
	pr_debug("hdcp: bksv: %02x%02x%02x%02x%02x\n",
			bksv[0], bksv[1], bksv[2], bksv[3], bksv[4]);

	/* Read BCAPS */
	if (hdcp_read_bcaps(&bcaps.value) == false) {
		pr_err("hdcp: failed to read bcaps\n");
		return false;
	}
	pr_debug("hdcp: bcaps: %x\n", bcaps.value);


	/* Update repeater present status */
	*is_repeater = bcaps.is_repeater;

	/* Set Repeater Bit */
	if (mofd_hdcp_set_repeater(bcaps.is_repeater) == false) {
		pr_err("hdcp: failed to set repeater bit\n");
		return false;
	}

	/* Write BKSV to Self (hdcp tx) */
	if (mofd_hdcp_set_bksv(bksv) == false) {
		pr_err("hdcp: failed to write bksv to self\n");
		return false;
	}

	pr_debug("hdcp: set repeater & bksv\n");

	/* Start Authentication i.e. computations using hdcp keys */
	if (mofd_hdcp_start_authentication() == false) {
		pr_err("hdcp: failed to start authentication\n");
		return false;
	}

	pr_debug("hdcp: auth started\n");

	/* Wait for 120ms before reading R0' */
	msleep(120);

	/* Check if R0 Ready in hdcp tx */
	retry = 20;
	do {
		if (mofd_hdcp_is_r0_ready() == true)
			break;
		usleep_range(5000, 5500);
		retry--;
	} while (retry);

	if (retry == 0 && mofd_hdcp_is_r0_ready() == false) {
		pr_err("hdcp: R0 is not ready\n");
		return false;
	}

	pr_debug("hdcp: tx_r0 ready\n");

	/* Read Ro' from Receiver hdcp rx */
	if (hdcp_read_rx_r0(&rx_r0) == false) {
		pr_err("hdcp: failed to read R0 from receiver\n");
		return false;
	}

	pr_debug("hdcp: rx_r0 = %04x\n", rx_r0);

	/* Check if R0 Matches */
	if (mofd_hdcp_does_ri_match(rx_r0) == false) {
		pr_err("hdcp: R0 does not match\n");
		return false;
	}
	pr_debug("hdcp: R0 matched\n");

	/* Enable Encryption & Check status */
	if (mofd_hdcp_enable_encryption() == false) {
		pr_err("hdcp: failed to enable encryption\n");
		return false;
	}
	pr_debug("hdcp: encryption enabled\n");

	ctx->is_phase1_enabled = true;

	return true;
}

/**
 * Description: this function initiates repeater authentication
 *
 * Returns:	true on success else false
 */
static bool hdcp_stage2_start_repeater_authentication(void)
{
	pr_debug("hdcp: initiating repeater check\n");
	g_pstHdcpContext->wdt_expired = false;
	if (hdcp_rep_check(true) == true) {
		if (hdcp_rep_watch_dog() == true)
			return true;
		else
			pr_debug("hdcp: failed to start repeater wdt\n");
	} else
		pr_debug("hdcp: failed to start repeater check\n");
	return false;
}

/**
 * Description:	this function schedules ri check
 *
 * @first_check	whether its the first time, interval is varied if first time
 *
 * Returns:	true on success else false
 */
static bool hdcp_stage3_schedule_ri_check(bool first_check)
{
	enum hdcp_event msg = HDCP_RI_CHECK;
	unsigned int *msg_data = kmalloc(sizeof(unsigned int), GFP_KERNEL);
	/* Do the first check immediately while adding some randomness  */
	int ri_check_interval = (first_check) ? (20 + (jiffies % 10)) :
		g_pstHdcpContext->ri_check_interval;
	if (msg_data != NULL) {
		*msg_data = g_pstHdcpContext->auth_id;
		return wq_send_message_delayed(msg,
					(void *)msg_data, ri_check_interval);
	}
	return false;
}

/**
 * Description: this function performs ri match
 *
 * Returns:	true on match else false
 */
static bool hdcp_stage3_ri_check(void)
{
	uint16_t rx_ri = 0;

	if (hdcp_enable_condition_ready() == false ||
	    g_pstHdcpContext->is_phase1_enabled == false)
		return false;

	if (hdcp_read_rx_ri(&rx_ri) == true) {
		if (mofd_hdcp_does_ri_match(rx_ri) == true)
			/* pr_debug("hdcp: Ri Matches %04x\n", rx_ri);*/
			return true;

		/* If first Ri check fails,we re-check it after ri_retry (msec).
		 * This is because some receivers do not immediately have valid
		 * Ri' at frame 128.
		 * */
		pr_debug("re-check Ri after %d (msec)\n",
				g_pstHdcpContext->ri_retry);

		msleep(g_pstHdcpContext->ri_retry);
		if (hdcp_read_rx_ri(&rx_ri) == true)
			if (mofd_hdcp_does_ri_match(rx_ri) == true)
				return true;
	}

	/* ri check failed update phase3 status */
	g_pstHdcpContext->is_phase3_valid = false;

	pr_debug("hdcp: error!!!  Ri check failed %x\n", rx_ri);
	return false;
}

/**
 * Description: this function schedules repeater authentication
 *
 * @first	whether its first time schedule or not, delay for check is
 *		varied between first and subsequent times
 *
 * Returns:	true on success else false
 */
static bool hdcp_rep_check(bool first)
{
	int msg = HDCP_REPEATER_CHECK;
	int delay = (first) ? 50 : 100;
	unsigned int *auth_id = kmalloc(sizeof(unsigned int), GFP_KERNEL);
	if (auth_id != NULL) {
		*auth_id = g_pstHdcpContext->auth_id;
		return wq_send_message_delayed(msg, (void *)auth_id, delay);
	} else
		pr_debug("hdcp: %s failed to alloc mem\n", __func__);
	return false;
}

/**
 * Description: this function creates a watch dog timer for repeater auth
 *
 * Returns:	true on success else false
 */
static bool hdcp_rep_watch_dog(void)
{
	int msg = HDCP_REPEATER_WDT_EXPIRED;
	unsigned int *auth_id = kmalloc(sizeof(unsigned int), GFP_KERNEL);
	if (auth_id != NULL) {
		*auth_id = g_pstHdcpContext->auth_id;
		/* set a watch dog timer for 5.2 secs, added additional
		   0.2 seconds to be safe */
		return wq_send_message_delayed(msg, (void *)auth_id, 5200);
	} else
		pr_debug("hdcp: %s failed to alloc mem\n", __func__);
	return false;
}

/**
 * Description: workqueue event handler to execute all hdcp tasks
 *
 * @work	work assigned from workqueue contains the task to be handled
 *
 * Returns:	none
 */
static void hdcp_task_event_handler(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	hdcp_event_msg *event = container_of(delayed_work,
					     hdcp_event_msg, dwork);
	int msg = 0;
	void *data = NULL;
	bool reset_hdcp = false;
	hdcp_context *ctx = g_pstHdcpContext;
	int is_connected =
		atomic_read(&(g_pstHdcpContext->pipe->hpd_ctx.is_connected));

	if (ctx == NULL || event == NULL)
		goto EXIT_HDCP_HANDLER;

	msg = event->msg;
	data = event->data;

	if (!is_connected) {
		pr_err("[%s,%d]hdcp error: hdmi pipe is not enabled.\n",
			__FILE__, __LINE__);
		goto EXIT_HDCP_HANDLER;
	}

	switch (msg) {
	case HDCP_ENABLE:
		if (hdcp_enable_condition_ready() == true &&
			ctx->is_phase1_enabled == false &&
			hdcp_wq_start() == false) {
			reset_hdcp = true;
			pr_debug("[%s,%d]hdcp: failed to start hdcp\n",
				__FILE__, __LINE__);
		}
		break;

	case HDCP_REPEATER_CHECK:
		pr_debug("hdcp: repeater check\n");
		if (data == NULL ||
			*(unsigned int *)data != ctx->auth_id)
			/*pr_debug("hdcp: auth count %d mismatch %d\n",
				*(unsigned int *)msg_data,
				hdcp_context->auth_id);*/
			break;

		if (hdcp_wq_repeater_authentication() == false)
			reset_hdcp = true;
		break;

	case HDCP_REPEATER_WDT_EXPIRED:
		if (data != NULL && *(unsigned int *)data == ctx->auth_id) {
			ctx->wdt_expired = true;
			if (!ctx->is_phase2_enabled && ctx->is_phase1_enabled)
				reset_hdcp = true;
		}
		break;
	case HDCP_RI_CHECK:
		if (data == NULL || *(unsigned int *)data != ctx->auth_id)
			break;

		/* Do phase 3 only if phase 1 was successful*/
		if (ctx->is_phase1_enabled == false)
			break;

		if (hdcp_wq_ri_check() == false)
			reset_hdcp = true;
		break;

	case HDCP_RESET:
		hdcp_wq_reset();
		break;
	default:
		break;
	}

	if (reset_hdcp == true) {
		msg = HDCP_RESET;
		wq_send_message(msg, NULL);
	} else
		/* if disabled retry HDCP authentication */
		hdcp_retry_enable();

EXIT_HDCP_HANDLER:
	if (data != NULL)
		kfree(data);
	if (event != NULL)
		kfree(event);

	return;
}

/**
 * Description: this function sends a message to the hdcp workqueue to be
 *		processed with a delay
 *
 * @msg		message type
 * @msg_data	any additional data accompanying the message
 * @delay	amount of delay for before the message gets processed
 *
 * Returns:	true if message was successfully queued else false
 */
static bool wq_send_message_delayed(int msg, void *msg_data,
				    unsigned long delay)
{
	hdcp_event_msg *pst_msg = NULL;
	pst_msg = kmalloc(sizeof(hdcp_event_msg), GFP_KERNEL);
	if (pst_msg == NULL) {
		kfree(msg_data);
		return false;
	}

	pst_msg->msg = msg;
	pst_msg->data = msg_data;

	INIT_DELAYED_WORK(&pst_msg->dwork, hdcp_task_event_handler);

	if (queue_delayed_work(g_pstHdcpContext->hdcp_event_handler,
		&(pst_msg->dwork),
		(unsigned long)(msecs_to_jiffies(delay))) != 0)
		return true;
	else
		pr_debug("hdcp: failed to add message to delayed wq\n");

	kfree(msg_data);

	return false;
}

/**
 * Description: this function sends a message to the hdcp workqueue to be
 *		processed without delay
 *
 * @msg		message type
 * @msg_data	any additional data accompanying the message
 *
 * Returns:	true if message was successfully queued else false
 */
static bool wq_send_message(int msg, void *msg_data)
{
	return wq_send_message_delayed(msg, msg_data, 0);
}
/**
 * Description: function to enable HDCP
 *
 * @hdmi_context handle hdmi_context
 * @refresh_rate vertical refresh rate of the video mode
 *
 * Returns:	true on success
 *		false on failure
 */
bool hdmi_hdcp_enable(struct hdmi_pipe *pipe)
{
	int  msg = HDCP_ENABLE;
	hdcp_context *ctx = g_pstHdcpContext;

	/* TODO: remove refresh cal here */

	struct drm_mode_modeinfo *prefer_mode = pipe->monitor.preferred_mode;

	u32 refresh_rate = mode_vrefresh(prefer_mode);
	if (refresh_rate == 0)
		return false;

	if (pipe == NULL || ctx == NULL)
		/* pipe and ctx is either not valid */
		return false;

	if (ctx->is_enabled == true) {
		pr_err("[%s,%d]hdcp: already enabled\n", __FILE__, __LINE__);
		return true;
	}

	do {/*set the hdcp context member value*/
		ctx->is_enabled = true;/*enable the hdcp context*/

		/* compute ri check interval based on refresh rate */
		if (refresh_rate) {
			/*compute msec time for 1 frame*/
			ctx->video_refresh_interval = 1000 / refresh_rate;
			/* compute msec time for 128 frames per HDCP spec */
			ctx->ri_check_interval = ((128 * 1000) / refresh_rate);
		} else {
			/*
			 * compute msec time for 1 frame,
			 * assuming refresh rate of 60
			 */
			ctx->video_refresh_interval = 1000 / 60;
			/* default to 128 frames @ 60 Hz */
			ctx->ri_check_interval = ((128 * 1000) / 60);
		}

		/* Set upper and lower bounds for ri_check_interval to
		 *  avoid dynamic adjustment to go wild.
		 *  Set adjustment range to 100ms, which is safe if HZ <=100.
		*/
		ctx->ri_check_interval_lower =	ctx->ri_check_interval - 100;
		ctx->ri_check_interval_upper =	ctx->ri_check_interval + 100;

		/* Init prev_ri_frm_cnt_status*/
		ctx->prev_ri_frm_cnt_status = 0;

		/*
		 * Set ri_retry
		 * Default to interval of 3 frames if can not read
		 * OTM_HDMI_ATTR_ID_HDCP_RI_RETRY
		 *
		 * 3 * hdcp_context->video_refresh_interval
		 */
		ctx->ri_retry = 40;

		ctx->is_hdmi = pipe->monitor.is_hdmi; /*check if */
	} while (0);

	/* send message and wait for 1st stage authentication to complete */
	if (wq_send_message(msg, NULL)) {
		/* on any failure is_required flag will be reset */
		while (ctx->is_enabled) {
			/* wait for phase1 to be enabled before
			 * returning from this function */
			if (ctx->is_phase1_enabled)
				return true;
			ulseep_range(1000, 1500);
		}
	}

	return false;
}

/**
 * Description: function to disable HDCP
 *
 * @hdmi_context handle hdmi_context
 *
 * Returns:	true on success
 *		false on failure
 */
bool hdmi_hdcp_disable(struct hdmi_pipe *pipe)
{
	int msg = HDCP_RESET;
	hdcp_context *ctx = g_pstHdcpContext;

	if (pipe == NULL ||  ctx == NULL)
		return false;

	if (ctx->is_enabled == false) {
		pr_debug("hdcp: already disabled\n");
		return true;
	}

	/* send reset message */
	wq_send_message(msg, NULL);

	/* Wait until hdcp is disabled.
	 * No need to wait for workqueue flushed since it may block for 2sec
	 * */
	while (g_pstHdcpContext->is_phase1_enabled)
		ulseep_range(1000, 1500);

	ctx->is_enabled = false;

	return true;
}

static void hdcp_reset_context(hdcp_context *ctx)
{
	if (ctx == NULL)
		return;

	/* this flag will be re-enabled by upper layers */
	ctx->is_enabled = false;
	ctx->is_phase1_enabled = false;
	ctx->is_phase2_enabled = false;
	ctx->is_phase3_valid = false;
	ctx->prev_ri_frm_cnt_status = 0;
	ctx->auth_id = 0; /* reset the auth id */

	memset(ctx->bksv, 0, HDCP_KSV_SIZE);
}

/**
 * Description: hdcp init function
 *
 * @hdmi_context handle hdmi_context
 * @ddc_rd_wr:	pointer to ddc read write function
 *
 * Returns:	true on success
 *		false on failure
 */
bool hdmi_hdcp_init(struct hdmi_pipe *pipe)
{
	hdcp_context *ctx = NULL;
	bool ret = true;
	if (pipe == NULL || g_pstHdcpContext != NULL) {
		pr_err("[%s,%d]syp code left hdcp: init error!!!\n",
			__FILE__, __LINE__);
		return false;
	}

	do {
		/* allocate hdcp context */
		ctx = kmalloc(sizeof(hdcp_context), GFP_KERNEL);
		if (ctx == NULL) {
			pr_err("[%s,%d]init error: allocation.\n",
				__FILE__, __LINE__);
			ret = false;
			break;
		}
		memset(ctx, 0, sizeof(hdcp_context));

		/* Create hdcp workqueue to handle all hdcp tasks.
		 * To avoid multiple threads created for multi-core CPU (eg CTP)
		 * use create_singlethread_workqueue.
		 */
		ctx->hdcp_event_handler = create_singlethread_workqueue(
							"HDCP_EVENT_WQ");
		if (ctx->hdcp_event_handler == NULL) {
			pr_err("[%s,%d]hdcp: init error: allocation.\n",
				__FILE__, __LINE__);
			ret = false;
			break;
		}
		hdcp_reset_context(ctx);

		/*save the hdmi pipe*/
		ctx->pipe = pipe;
	} while (0);

	if (ret == false) {
		pr_err("[%s,%d]syp code left:hdcp: init error: allocation.\n",
			__FILE__, __LINE__);
		goto EXIT_INIT;
	}

	g_pstHdcpContext = ctx;/*save the context to the global variable.*/


	/* perform any hardware initializations */
	if (mofd_hdcp_init() == true) {
		pr_err("[%s,%d]syp code left:hdcp: initialized\n",
			__FILE__, __LINE__);
		return true;
	}

EXIT_INIT:
	if (ctx != NULL) {/* Cleanup and exit */
		if (ctx->hdcp_event_handler != NULL)
			destroy_workqueue(ctx->hdcp_event_handler);

		kfree(ctx);
		ctx = NULL;
	}
	return false;
}
