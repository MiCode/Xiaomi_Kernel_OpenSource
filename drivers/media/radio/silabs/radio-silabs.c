/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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


#define DRIVER_NAME "radio-silabs"
#define DRIVER_CARD "Silabs FM Radio Receiver"
#define DRIVER_DESC "Driver for Silabs FM Radio receiver"

#include <linux/version.h>
#include <linux/init.h>         /* Initdata                     */
#include <linux/delay.h>        /* udelay                       */
#include <linux/uaccess.h>      /* copy to/from user            */
#include <linux/kfifo.h>        /* lock free circular buffer    */
#include <linux/param.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

/* kernel includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/atomic.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include "radio-silabs.h"
#include "radio-silabs-transport.h"

struct silabs_fm_device {
	struct video_device *videodev;
	/* driver management */
	atomic_t users;
	struct device *dev;
	unsigned int chipID;
	/* To send commands*/
	u8 write_buf[WRITE_REG_NUM];
	/* TO read events, data*/
	u8 read_buf[READ_REG_NUM];
	/*RDS buffers + Radio event buffer*/
	struct kfifo data_buf[SILABS_FM_BUF_MAX];
	struct silabs_fm_recv_conf_req recv_conf;
	struct completion sync_req_done;
	/* for the first tune, we need to set properties for digital audio. */
	u8 first_tune;
	int tune_req;
	/* 1 if tune is pending, 2 if seek is pending, 0 otherwise.*/
	u8 seek_tune_status;
	/* command that is being sent to chip. */
	u8 cmd;
	u8 antenna;
	u8 g_search_mode;
	unsigned int mode;
	/* regional settings */
	enum silabs_region_t region;
	/* power mode */
	int lp_mode;
	int handle_irq;
	/* global lock */
	struct mutex lock;
	/* buffer locks*/
	spinlock_t buf_lock[SILABS_FM_BUF_MAX];
	/* work queue */
	struct workqueue_struct *wqueue;
	struct workqueue_struct *wqueue_scan;
	struct delayed_work work;
	struct delayed_work work_scan;
	/* wait queue for blocking event read */
	wait_queue_head_t event_queue;
	/* wait queue for raw rds read */
	wait_queue_head_t read_queue;
	int irq;
	int tuned_freq_khz;
	int dwell_time;
	int search_on;
};

static struct silabs_fm_device *g_radio;
static int silabs_fm_request_irq(struct silabs_fm_device *radio);
static int tune(struct silabs_fm_device *radio, u32 freq);
static int silabs_seek(struct silabs_fm_device *radio, int dir, int wrap);
static int cancel_seek(struct silabs_fm_device *radio);
static void silabs_fm_q_event(struct silabs_fm_device *radio,
				enum silabs_evt_t event);

static bool is_enable_rx_possible(struct silabs_fm_device *radio)
{
	bool retval = true;

	if (radio->mode == FM_OFF || radio->mode == FM_RECV)
		retval = false;

	return retval;
}

static int read_cts_bit(struct silabs_fm_device *radio)
{
	int retval = 1, i = 0;

	for (i = 0; i < CTS_RETRY_COUNT; i++) {
		memset(radio->read_buf, 0, READ_REG_NUM);

		retval = silabs_fm_i2c_read(radio->read_buf, READ_REG_NUM);

		if (retval < 0) {
			FMDERR("%s: failure reading the response, error %d\n",
					__func__, retval);
			continue;
		} else
			FMDBG("%s: successfully read the response from soc\n",
					__func__);

		if (radio->read_buf[0] & ERR_BIT_MASK) {
			FMDERR("%s: error bit set\n", __func__);
			switch (radio->read_buf[1]) {
			case BAD_CMD:
				FMDERR("%s: cmd %d, error BAD_CMD\n",
						__func__, radio->cmd);
				break;
			case BAD_ARG1:
				FMDERR("%s: cmd %d, error BAD_ARG1\n",
						__func__, radio->cmd);
				break;
			case BAD_ARG2:
				FMDERR("%s: cmd %d, error BAD_ARG2\n",
						__func__, radio->cmd);
				break;
			case BAD_ARG3:
				FMDERR("%s: cmd %d, error BAD_ARG3\n",
						__func__, radio->cmd);
				break;
			case BAD_ARG4:
				FMDERR("%s: cmd %d, error BAD_ARG4\n",
						__func__, radio->cmd);
				break;
			case BAD_ARG5:
				FMDERR("%s: cmd %d, error BAD_ARG5\n",
						__func__, radio->cmd);
			case BAD_ARG6:
				FMDERR("%s: cmd %d, error BAD_ARG6\n",
						__func__, radio->cmd);
				break;
			case BAD_ARG7:
				FMDERR("%s: cmd %d, error BAD_ARG7\n",
						__func__, radio->cmd);
				break;
			case BAD_PROP:
				FMDERR("%s: cmd %d, error BAD_PROP\n",
						__func__, radio->cmd);
				break;
			case BAD_BOOT_MODE:
				FMDERR("%s:cmd %d,err BAD_BOOT_MODE\n",
						__func__, radio->cmd);
				break;
			default:
				FMDERR("%s: cmd %d, unknown error\n",
						__func__, radio->cmd);
				break;
			}
			retval = -EINVAL;
			goto bad_cmd_arg;

		}

		if (radio->read_buf[0] & CTS_INT_BIT_MASK) {
			FMDERR("In %s, CTS bit is set\n", __func__);
			break;
		}
		/* Give some time if the chip is not done with processing
		 * previous command.
		 */
		msleep(100);
	}

	FMDERR("In %s, status byte is %x\n",  __func__, radio->read_buf[0]);

bad_cmd_arg:
	return retval;
}

static int send_cmd(struct silabs_fm_device *radio, u8 total_len)
{
	int retval = 0;

	if (unlikely(radio == NULL)) {
		FMDERR(":radio is null");
		return -EINVAL;
	}

	retval = silabs_fm_i2c_write(radio->write_buf, total_len);

	if (retval > 0)	{
		FMDBG("In %s, successfully written command %x to soc\n",
			  __func__, radio->write_buf[0]);
	} else {
		FMDERR("In %s, error %d writing command %d to soc\n",
			   __func__, retval, radio->write_buf[1]);
	}

	retval = read_cts_bit(radio);

	return retval;
}

static int get_property(struct silabs_fm_device *radio, u16 prop, u16 *pvalue)
{
	int retval = 0;

	mutex_lock(&radio->lock);
	memset(radio->write_buf, 0, WRITE_REG_NUM);

	/* track command that is being sent to chip. */
	radio->cmd = GET_PROPERTY_CMD;
	radio->write_buf[0] = GET_PROPERTY_CMD;
	/* reserved, always write 0 */
	radio->write_buf[1] = 0;
	/* property high byte */
	radio->write_buf[2] = HIGH_BYTE_16BIT(prop);
	/* property low byte */
	radio->write_buf[3] = LOW_BYTE_16BIT(prop);

	FMDBG("in %s, radio->write_buf[2] is %x\n",
					__func__, radio->write_buf[2]);
	FMDBG("in %s, radio->write_buf[3] is %x\n",
					__func__, radio->write_buf[3]);

	retval = send_cmd(radio, GET_PROP_CMD_LEN);
	if (retval < 0)
		FMDERR("In %s, error getting property %d\n", __func__, prop);
	else
		*pvalue = (radio->read_buf[2] << 8) + radio->read_buf[3];

	mutex_unlock(&radio->lock);

	return retval;
}


static int set_property(struct silabs_fm_device *radio, u16 prop, u16 value)
{
	int retval = 0;

	mutex_lock(&radio->lock);

	memset(radio->write_buf, 0, WRITE_REG_NUM);

	/* track command that is being sent to chip. */
	radio->cmd = SET_PROPERTY_CMD;
	radio->write_buf[0] = SET_PROPERTY_CMD;
	/* reserved, always write 0 */
	radio->write_buf[1] = 0;
	/* property high byte */
	radio->write_buf[2] = HIGH_BYTE_16BIT(prop);
	/* property low byte */
	radio->write_buf[3] = LOW_BYTE_16BIT(prop);

	/* value high byte */
	radio->write_buf[4] = HIGH_BYTE_16BIT(value);
	/* value low byte */
	radio->write_buf[5] = LOW_BYTE_16BIT(value);

	retval = send_cmd(radio, SET_PROP_CMD_LEN);
	if (retval < 0)
		FMDERR("In %s, error setting property %d\n", __func__, prop);

	mutex_unlock(&radio->lock);

	return retval;
}

static void silabs_scan(struct work_struct *work)
{
	struct silabs_fm_device *radio;
	int current_freq_khz;
	u8 valid;
	u8 bltf;
	u32 temp_freq_khz;
	int retval = 0;

	FMDBG("+%s, getting radio handle from work struct\n", __func__);

	radio = g_radio;

	if (unlikely(radio == NULL)) {
		FMDERR(":radio is null");
		return;
	}

	current_freq_khz = radio->tuned_freq_khz;
	FMDBG("current freq is %d\n", current_freq_khz);

	radio->seek_tune_status = SCAN_PENDING;
	/* tune to lowest freq of the band */
	retval = tune(radio, radio->recv_conf.band_low_limit * TUNE_STEP_SIZE);
	if (retval < 0) {
		FMDERR("%s: Tune to lower band limit failed with error %d\n",
			__func__, retval);
		goto seek_tune_fail;
	}

	/* wait for tune to complete. */
	if (!wait_for_completion_timeout(&radio->sync_req_done,
				msecs_to_jiffies(WAIT_TIMEOUT_MSEC)))
		FMDERR("In %s, didn't receive STC for tune\n", __func__);
	else
		FMDBG("In %s, received STC for tune\n", __func__);
	while (1) {
		silabs_seek(radio, SRCH_DIR_UP, 0);
		if (retval < 0) {
			FMDERR("Scan operation failed with error %d\n", retval);
			goto seek_tune_fail;
		}
		/* wait for seek to complete */
		if (!wait_for_completion_timeout(&radio->sync_req_done,
					msecs_to_jiffies(WAIT_TIMEOUT_MSEC)))
			FMDERR("%s: didn't receive STC for seek\n", __func__);
		else
			FMDBG("%s: received STC for seek\n", __func__);

		mutex_lock(&radio->lock);
		memset(radio->write_buf, 0, WRITE_REG_NUM);

		radio->cmd = FM_TUNE_STATUS_CMD;

		radio->write_buf[0] = FM_TUNE_STATUS_CMD;
		radio->write_buf[1] = 0;

		retval = send_cmd(radio, TUNE_STATUS_CMD_LEN);
		if (retval < 0)	{
			FMDERR("%s: FM_TUNE_STATUS_CMD failed with error %d\n",
					__func__, retval);
		}

		valid = radio->read_buf[1] & 0x01;
		bltf = radio->read_buf[1] & 0x80;

		temp_freq_khz = ((u32)(radio->read_buf[2] << 8) +
					radio->read_buf[3])*
					TUNE_STEP_SIZE;
		mutex_unlock(&radio->lock);
		FMDBG("In %s, freq is %d\n", __func__, temp_freq_khz);

		if (valid) {
			FMDBG("val bit set, posting SILABS_EVT_TUNE_SUCC\n");
			silabs_fm_q_event(radio, SILABS_EVT_TUNE_SUCC);
		}

		if (bltf) {
			FMDBG("bltf bit is set\n");
			break;
		}
		/* sleep for dwell period */
		msleep(radio->dwell_time * 1000);

		/* need to queue the event when the seek completes */
		silabs_fm_q_event(radio, SILABS_EVT_SCAN_NEXT);
	}
seek_tune_fail:
	/* tune to original frequency */
	retval = tune(radio, current_freq_khz);
	if (retval < 0)
		FMDERR("%s: Tune to orig freq failed with error %d\n",
			__func__, retval);
	else {
		if (!wait_for_completion_timeout(&radio->sync_req_done,
					msecs_to_jiffies(WAIT_TIMEOUT_MSEC)))
			FMDERR("%s: didn't receive STC for tune\n", __func__);
		else
			FMDBG("%s: received STC for tune\n", __func__);
	}
	silabs_fm_q_event(radio, SILABS_EVT_SEEK_COMPLETE);
	radio->seek_tune_status = NO_SEEK_TUNE_PENDING;
}

static int silabs_search(struct silabs_fm_device *radio, bool on)
{
	int retval = 0;
	int saved_val;
	int current_freq_khz;

	saved_val = radio->search_on;
	radio->search_on = on;
	current_freq_khz = radio->tuned_freq_khz;

	if (on) {
		g_radio = radio;
		FMDBG("%s: Queuing the work onto scan work q\n", __func__);
		queue_delayed_work(radio->wqueue_scan, &radio->work_scan,
					msecs_to_jiffies(SILABS_DELAY_MSEC));
	} else {
		cancel_seek(radio);
		silabs_fm_q_event(radio, SILABS_EVT_SEEK_COMPLETE);
	}

	if (retval < 0)
		radio->search_on = saved_val;
	return retval;
}

/* to enable, disable interrupts.*/
static int configure_interrupts(struct silabs_fm_device *radio, u8 val)
{
	int retval = 0;
	u16 prop_val = 0;

	switch (val) {
	case DISABLE_ALL_INTERRUPTS:
		prop_val = 0;
		retval = set_property(radio, GPO_IEN_PROP, prop_val);
		if (retval < 0)
			FMDERR("In %s, error disabling interrupts\n", __func__);
		break;
	case ENABLE_STC_RDS_INTERRUPTS:
		/* enable interrupts. */
		prop_val = RDS_INT_BIT_MASK | STC_INT_BIT_MASK;

		retval = set_property(radio, GPO_IEN_PROP, prop_val);
		if (retval < 0)
			FMDERR("In %s, error enabling interrupts\n", __func__);
		break;
	case ENABLE_STC_INTERRUPTS:
		/* enable STC interrupts only. */
		prop_val = STC_INT_BIT_MASK;

		retval = set_property(radio, GPO_IEN_PROP, prop_val);
		if (retval < 0)
			FMDERR("In %s, error enabling interrupts\n", __func__);
		break;
	default:
		FMDERR("%s: invalid value %u\n", __func__, val);
		retval = -EINVAL;
		break;
	}

	return retval;
}

static int get_int_status(struct silabs_fm_device *radio)
{
	int retval = 0;

	mutex_lock(&radio->lock);

	memset(radio->write_buf, 0, WRITE_REG_NUM);

	/* track command that is being sent to chip.*/
	radio->cmd = GET_INT_STATUS_CMD;
	radio->write_buf[0] = GET_INT_STATUS_CMD;

	retval = send_cmd(radio, GET_INT_STATUS_CMD_LEN);

	if (retval < 0)
		FMDERR("%s: get_int_status failed with error %d\n",
				__func__, retval);

	mutex_unlock(&radio->lock);

	return retval;
}

static int initialize_recv(struct silabs_fm_device *radio)
{
	int retval = 0;

	retval = set_property(radio, FM_SEEK_TUNE_SNR_THRESHOLD_PROP, 2);
	if (retval < 0)	{
		FMDERR("%s: FM_SEEK_TUNE_SNR_THRESHOLD_PROP fail error %d\n",
				__func__, retval);
		goto set_prop_fail;
	}

	retval = set_property(radio, FM_SEEK_TUNE_RSSI_THRESHOLD_PROP, 7);
	if (retval < 0)	{
		FMDERR("%s: FM_SEEK_TUNE_RSSI_THRESHOLD_PROP fail error %d\n",
				__func__, retval);
		goto set_prop_fail;
	}

set_prop_fail:
	return retval;

}

static int enable(struct silabs_fm_device *radio)
{
	int retval = 0;

	retval = read_cts_bit(radio);

	if (retval < 0)
		return retval;

	mutex_lock(&radio->lock);

	memset(radio->write_buf, 0, WRITE_REG_NUM);

	/* track command that is being sent to chip.*/
	radio->cmd = POWER_UP_CMD;
	radio->write_buf[0] = POWER_UP_CMD;
	radio->write_buf[1] = ENABLE_GPO2_INT_MASK;

	radio->write_buf[2] = AUDIO_OPMODE_DIGITAL;

	retval = send_cmd(radio, POWER_UP_CMD_LEN);

	if (retval < 0) {
		FMDERR("%s: enable failed with error %d\n", __func__, retval);
		mutex_unlock(&radio->lock);
		goto send_cmd_fail;
	}

	mutex_unlock(&radio->lock);

	/* enable interrupts */
	retval = configure_interrupts(radio, ENABLE_STC_RDS_INTERRUPTS);
	if (retval < 0)
		FMDERR("In %s, configure_interrupts failed with error %d\n",
				__func__, retval);

	/* initialize with default configuration */
	retval = initialize_recv(radio);
	if (retval >= 0) {
		if (radio->mode == FM_RECV_TURNING_ON) {
			FMDBG("In %s, posting SILABS_EVT_RADIO_READY event\n",
				__func__);
			silabs_fm_q_event(radio, SILABS_EVT_RADIO_READY);
			radio->mode = FM_RECV;
		}
	}
send_cmd_fail:
	return retval;

}

static int disable(struct silabs_fm_device *radio)
{
	int retval = 0;

	mutex_lock(&radio->lock);

	memset(radio->write_buf, 0, WRITE_REG_NUM);

	/* track command that is being sent to chip. */
	radio->cmd = POWER_DOWN_CMD;
	radio->write_buf[0] = POWER_DOWN_CMD;

	retval = send_cmd(radio, POWER_DOWN_CMD_LEN);
	if (retval < 0)
		FMDERR("%s: disable failed with error %d\n",
			__func__, retval);

	mutex_unlock(&radio->lock);

	if (radio->mode == FM_TURNING_OFF || radio->mode == FM_RECV) {
		FMDBG("%s: posting SILABS_EVT_RADIO_DISABLED event\n",
			__func__);
		silabs_fm_q_event(radio, SILABS_EVT_RADIO_DISABLED);
		radio->mode = FM_OFF;
	}

	return retval;
}

static int set_chan_spacing(struct silabs_fm_device *radio, u16 spacing)
{
	int retval = 0;
	u16 prop_val = 0;

	if (spacing == 0)
		prop_val = FM_RX_SPACE_200KHZ;
	else if (spacing == 1)
		prop_val = FM_RX_SPACE_100KHZ;
	else if (spacing == 2)
		prop_val = FM_RX_SPACE_50KHZ;

	retval = set_property(radio, FM_SEEK_FREQ_SPACING_PROP, prop_val);
	if (retval < 0)
		FMDERR("In %s, error setting channel spacing\n", __func__);
	else
		radio->recv_conf.ch_spacing = spacing;

	return retval;

}

static int set_emphasis(struct silabs_fm_device *radio, u16 emp)
{
	int retval = 0;
	u16 prop_val = 0;

	if (emp == 0)
		prop_val = FM_RX_EMP75;
	else if (emp == 1)
		prop_val = FM_RX_EMP50;

	retval = set_property(radio, FM_DEEMPHASIS_PROP, prop_val);
	if (retval < 0)
		FMDERR("In %s, error setting emphasis\n", __func__);
	else
		radio->recv_conf.emphasis = emp;

	return retval;

}

static int tune(struct silabs_fm_device *radio, u32 freq_khz)
{
	int retval = 0;
	u16 freq_16bit = (u16)(freq_khz/TUNE_STEP_SIZE);

	FMDBG("In %s, freq is %d\n", __func__, freq_khz);

	/*
	 * when we are tuning for the first time, we must set digital audio
	 * properties.
	 */
	if (radio->first_tune) {
		/* I2S mode, rising edge */
		retval = set_property(radio, DIGITAL_OUTPUT_FORMAT_PROP, 0);
		if (retval < 0)	{
			FMDERR("%s: set output format prop failed, error %d\n",
					__func__, retval);
			goto set_prop_fail;
		}

		/* 48khz sample rate */
		retval = set_property(radio,
					DIGITAL_OUTPUT_SAMPLE_RATE_PROP,
					0xBB80);
		if (retval < 0)	{
			FMDERR("%s: set sample rate prop failed, error %d\n",
					__func__, retval);
			goto set_prop_fail;
		}
		radio->first_tune = false;
	}

	mutex_lock(&radio->lock);

	memset(radio->write_buf, 0, WRITE_REG_NUM);

	/* track command that is being sent to chip.*/
	radio->cmd = FM_TUNE_FREQ_CMD;

	radio->write_buf[0] = FM_TUNE_FREQ_CMD;
	/* reserved */
	radio->write_buf[1] = 0;
	/* freq high byte */
	radio->write_buf[2] = HIGH_BYTE_16BIT(freq_16bit);
	/* freq low byte */
	radio->write_buf[3] = LOW_BYTE_16BIT(freq_16bit);
	radio->write_buf[4] = 0;

	FMDBG("In %s, radio->write_buf[2] %x, radio->write_buf[3]%x\n",
		__func__, radio->write_buf[2], radio->write_buf[3]);

	retval = send_cmd(radio, TUNE_FREQ_CMD_LEN);
	if (retval < 0)
		FMDERR("In %s, tune failed with error %d\n", __func__, retval);

	mutex_unlock(&radio->lock);

set_prop_fail:
	return retval;
}

static int silabs_seek(struct silabs_fm_device *radio, int dir, int wrap)
{
	int retval = 0;

	mutex_lock(&radio->lock);

	memset(radio->write_buf, 0, WRITE_REG_NUM);

	/* track command that is being sent to chip. */
	radio->cmd = FM_SEEK_START_CMD;

	radio->write_buf[0] = FM_SEEK_START_CMD;
	if (wrap)
		radio->write_buf[1] = SEEK_WRAP_MASK;

	if (dir == SRCH_DIR_UP)
		radio->write_buf[1] |= SEEK_UP_MASK;

	retval = send_cmd(radio, SEEK_CMD_LEN);
	if (retval < 0)
		FMDERR("In %s, seek failed with error %d\n", __func__, retval);

	mutex_unlock(&radio->lock);
	return retval;
}


static int cancel_seek(struct silabs_fm_device *radio)
{
	int retval = 0;

	mutex_lock(&radio->lock);

	memset(radio->write_buf, 0, WRITE_REG_NUM);

	/* track command that is being sent to chip. */
	radio->cmd = FM_TUNE_STATUS_CMD;

	radio->write_buf[0] = FM_TUNE_STATUS_CMD;
	radio->write_buf[1] = CANCEL_SEEK_MASK;

	retval = send_cmd(radio, TUNE_STATUS_CMD_LEN);
	if (retval < 0)
		FMDERR("%s: cancel_seek failed, error %d\n", __func__, retval);

	mutex_unlock(&radio->lock);

	return retval;

}

static void silabs_fm_q_event(struct silabs_fm_device *radio,
				enum silabs_evt_t event)
{

	struct kfifo *data_b;
	unsigned char evt = event;

	data_b = &radio->data_buf[SILABS_FM_BUF_EVENTS];

	FMDBG("updating event_q with event %x\n", event);
	if (kfifo_in_locked(data_b,
				&evt,
				1,
				&radio->buf_lock[SILABS_FM_BUF_EVENTS]))
		wake_up_interruptible(&radio->event_queue);
}

static void silabs_interrupts_handler(struct silabs_fm_device *radio)
{
	int retval = 0;

	if (unlikely(radio == NULL)) {
			FMDERR("%s:radio is null", __func__);
			return;
	}

	FMDBG("%s: ISR fired for cmd %x, reading status bytes\n",
		__func__, radio->cmd);

	/* Get int status to know which interrupt is this(STC/RDS/etc) */
	retval = get_int_status(radio);

	if (retval < 0) {
		FMDERR("%s: failure reading the resp from soc with error %d\n",
			  __func__, retval);
		return;
	}
	FMDBG("%s: successfully read the resp from soc, status byte is %x\n",
		  __func__, radio->read_buf[0]);


	if (radio->read_buf[0] & STC_INT_BIT_MASK) {
		FMDBG("%s: STC bit set for cmd %x\n", __func__, radio->cmd);
		if (radio->seek_tune_status == TUNE_PENDING) {
			FMDBG("In %s, posting SILABS_EVT_TUNE_SUCC event\n",
				__func__);
			silabs_fm_q_event(radio, SILABS_EVT_TUNE_SUCC);
			radio->seek_tune_status = NO_SEEK_TUNE_PENDING;

		} else if (radio->seek_tune_status == SEEK_PENDING) {
			FMDBG("%s: posting SILABS_EVT_SEEK_COMPLETE event\n",
				__func__);
			silabs_fm_q_event(radio, SILABS_EVT_SEEK_COMPLETE);
			/* post tune comp evt since seek results in a tune.*/
			FMDBG("%s: posting SILABS_EVT_TUNE_SUCC\n",
				__func__);
			silabs_fm_q_event(radio, SILABS_EVT_TUNE_SUCC);
			radio->seek_tune_status = NO_SEEK_TUNE_PENDING;

		} else if (radio->seek_tune_status == SCAN_PENDING) {
			/*
			 * when scan is pending and STC int is set, signal
			 * so that scan can proceed
			 */
			FMDBG("In %s, signalling scan thread\n", __func__);
			complete(&radio->sync_req_done);
		}

		return;
	}

	return;
}

static void read_int_stat(struct work_struct *work)
{
	struct silabs_fm_device *radio;

	radio = container_of(work, struct silabs_fm_device, work.work);

	silabs_interrupts_handler(radio);
}

static void silabs_fm_disable_irq(struct silabs_fm_device *radio)
{
	int irq;

	irq = radio->irq;
	disable_irq_wake(irq);
	free_irq(irq, radio);
	cancel_delayed_work_sync(&radio->work);
	flush_workqueue(radio->wqueue);
}

static irqreturn_t silabs_fm_isr(int irq, void *dev_id)
{
	struct silabs_fm_device *radio = dev_id;
	/*
	 * The call to queue_delayed_work ensures that a minimum delay
	 * (in jiffies) passes before the work is actually executed. The return
	 * value from the function is nonzero if the work_struct was actually
	 * added to queue (otherwise, it may have already been there and will
	 * not be added a second time).
	 */

	queue_delayed_work(radio->wqueue, &radio->work,
				msecs_to_jiffies(SILABS_DELAY_MSEC));

	return IRQ_HANDLED;
}

static int silabs_fm_request_irq(struct silabs_fm_device *radio)
{
	int retval;
	int irq;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	irq = radio->irq;

	/*
	 * Use request_any_context_irq, So that it might work for nested or
	 * nested interrupts.
	 */
	retval = request_any_context_irq(irq, silabs_fm_isr,
				IRQ_TYPE_EDGE_FALLING, "fm interrupt", radio);
	if (retval < 0) {
		FMDERR("Couldn't acquire FM gpio %d\n", irq);
		return retval;
	} else {
		FMDBG("FM GPIO %d registered\n", irq);
	}
	retval = enable_irq_wake(irq);
	if (retval < 0) {
		FMDERR("Could not enable FM interrupt\n ");
		free_irq(irq , radio);
	}
	return retval;
}

static int silabs_fm_fops_open(struct file *file)
{
	struct silabs_fm_device *radio = video_get_drvdata(video_devdata(file));
	int retval = -ENODEV;
	int gpio_num = -1;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&radio->work, read_int_stat);
	INIT_DELAYED_WORK(&radio->work_scan, silabs_scan);

	init_completion(&radio->sync_req_done);
	if (!atomic_dec_and_test(&radio->users)) {
		FMDBG("%s: Device already in use. Try again later", __func__);
		atomic_inc(&radio->users);
		return -EBUSY;
	}

	/* initial gpio pin config & Power up */
	retval = silabs_fm_power_cfg(1);
	if (retval) {
		FMDERR("%s: failed config gpio & pmic\n", __func__);
		goto open_err_setup;
	}
	gpio_num = get_int_gpio_number();
	radio->irq = gpio_to_irq(gpio_num);

	if (radio->irq < 0) {
		FMDERR("%s: gpio_to_irq returned %d\n", __func__, radio->irq);
		goto open_err_req_irq;
	}

	FMDBG("irq number is = %d\n", radio->irq);
	/* enable irq */
	retval = silabs_fm_request_irq(radio);
	if (retval < 0) {
		FMDERR("%s: failed to request irq\n", __func__);
		goto open_err_req_irq;
	}

	radio->handle_irq = 0;
	radio->first_tune = true;
	return 0;

open_err_req_irq:
	silabs_fm_power_cfg(0);
open_err_setup:
	radio->handle_irq = 1;
	atomic_inc(&radio->users);
	return retval;
}

static int silabs_fm_fops_release(struct file *file)
{
	struct silabs_fm_device *radio = video_get_drvdata(video_devdata(file));
	int retval = 0;

	if (unlikely(radio == NULL))
		return -EINVAL;

	if (radio->mode == FM_RECV) {
		radio->mode = FM_OFF;
		retval = disable(radio);
		if (retval < 0)
			FMDERR("Err on disable FM %d\n", retval);
	}

	FMDBG("%s, Disabling the IRQs\n", __func__);
	/* disable irq */
	silabs_fm_disable_irq(radio);

	retval = silabs_fm_power_cfg(0);
	if (retval < 0)
		FMDERR("%s: failed to configure gpios\n", __func__);

	atomic_inc(&radio->users);

	return retval;
}

static struct v4l2_queryctrl silabs_fm_v4l2_queryctrl[] = {
	{
		.id	       = V4L2_CID_AUDIO_VOLUME,
		.type	       = V4L2_CTRL_TYPE_INTEGER,
		.name	       = "Volume",
		.minimum       = 0,
		.maximum       = 15,
		.step	       = 1,
		.default_value = 15,
	},
	{
		.id	       = V4L2_CID_AUDIO_BALANCE,
		.flags	       = V4L2_CTRL_FLAG_DISABLED,
	},
	{
		.id	       = V4L2_CID_AUDIO_BASS,
		.flags	       = V4L2_CTRL_FLAG_DISABLED,
	},
	{
		.id	       = V4L2_CID_AUDIO_TREBLE,
		.flags	       = V4L2_CTRL_FLAG_DISABLED,
	},
	{
		.id	       = V4L2_CID_AUDIO_MUTE,
		.type	       = V4L2_CTRL_TYPE_BOOLEAN,
		.name	       = "Mute",
		.minimum       = 0,
		.maximum       = 1,
		.step	       = 1,
		.default_value = 1,
	},
	{
		.id	       = V4L2_CID_AUDIO_LOUDNESS,
		.flags	       = V4L2_CTRL_FLAG_DISABLED,
	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_SRCHON,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "Search on/off",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 1,
		.default_value = 1,

	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_STATE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "radio 0ff/rx/tx/reset",
		.minimum       = 0,
		.maximum       = 3,
		.step          = 1,
		.default_value = 1,

	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_REGION,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "radio standard",
		.minimum       = 0,
		.maximum       = 2,
		.step          = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_SIGNAL_TH,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Signal Threshold",
		.minimum       = 0x80,
		.maximum       = 0x7F,
		.step          = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_EMPHASIS,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "Emphasis",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_RDS_STD,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "RDS standard",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_SPACING,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Channel spacing",
		.minimum       = 0,
		.maximum       = 2,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_RDSON,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "RDS on/off",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_RDSGROUP_MASK,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "RDS group mask",
		.minimum       = 0,
		.maximum       = 0xFFFFFFFF,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_RDSGROUP_PROC,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "RDS processing",
		.minimum       = 0,
		.maximum       = 0xFF,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_RDSD_BUF,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "RDS data groups to buffer",
		.minimum       = 1,
		.maximum       = 21,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_PSALL,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "pass all ps strings",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_LP_MODE,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "Low power mode",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 0,
	},
	{
		.id            = V4L2_CID_PRIVATE_SILABS_ANTENNA,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "headset/internal",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 0,
	},

};

static int silabs_fm_vidioc_querycap(struct file *file, void *priv,
		struct v4l2_capability *capability)
{
	struct silabs_fm_device *radio = video_get_drvdata(video_devdata(file));

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(capability == NULL)) {
		FMDERR("%s:capability is null", __func__);
		return -EINVAL;
	}

	strlcpy(capability->driver, DRIVER_NAME, sizeof(capability->driver));
	strlcpy(capability->card, DRIVER_CARD, sizeof(capability->card));
	snprintf(capability->bus_info, 4,  "I2C");
	capability->capabilities = V4L2_CAP_TUNER | V4L2_CAP_RADIO;

	return 0;
}

static int silabs_fm_vidioc_queryctrl(struct file *file, void *priv,
		struct v4l2_queryctrl *qc)
{
	unsigned char i;
	int retval = -EINVAL;

	if (unlikely(qc == NULL)) {
		FMDERR("%s:qc is null", __func__);
		return -EINVAL;
	}


	for (i = 0; i < ARRAY_SIZE(silabs_fm_v4l2_queryctrl); i++) {
		if (qc->id && qc->id == silabs_fm_v4l2_queryctrl[i].id) {
			memcpy(qc, &(silabs_fm_v4l2_queryctrl[i]),
				       sizeof(*qc));
			retval = 0;
			break;
		}
	}
	if (retval < 0)
		FMDERR("query conv4ltrol failed with %d\n", retval);

	return retval;
}

static int silabs_fm_vidioc_g_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct silabs_fm_device *radio = video_get_drvdata(video_devdata(file));
	int retval = 0;

	if (unlikely(radio == NULL)) {
		FMDERR(":radio is null");
		retval = -EINVAL;
		goto err_null_args;
	}

	if (ctrl == NULL) {
		FMDERR("%s, v4l2 ctrl is null\n", __func__);
		retval = -EINVAL;
		goto err_null_args;
	}

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_VOLUME:
		break;
	case V4L2_CID_AUDIO_MUTE:
		break;

	case V4L2_CID_PRIVATE_SILABS_RDSGROUP_PROC:
		ctrl->value = 0;
		retval = 0;
		break;
	default:
		retval = -EINVAL;
		break;
	}

err_null_args:
	if (retval > 0)
		retval = -EINVAL;
	if (retval < 0)
		FMDERR("get control failed with %d, id: %x\n",
			retval, ctrl->id);

	return retval;
}

static int silabs_fm_vidioc_s_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct silabs_fm_device *radio = video_get_drvdata(video_devdata(file));
	int retval = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(ctrl == NULL)) {
		FMDERR("%s:ctrl is null", __func__);
		return -EINVAL;
	}

	switch (ctrl->id) {
	case V4L2_CID_PRIVATE_SILABS_STATE:
		/* check if already on */
		if (ctrl->value == FM_RECV) {
			if (is_enable_rx_possible(radio) != 0) {
				FMDERR("%s: fm is not in proper state\n",
					 __func__);
				retval = -EINVAL;
				goto end;
			}
			radio->mode = FM_RECV_TURNING_ON;

			retval = enable(radio);
			if (retval < 0) {
				FMDERR("Error while enabling RECV FM %d\n",
					retval);
				radio->mode = FM_OFF;
				goto end;
			}
		} else if (ctrl->value == FM_OFF) {
			radio->mode = FM_TURNING_OFF;
			retval = disable(radio);
			if (retval < 0) {
				FMDERR("Err on disable recv FM %d\n", retval);
				radio->mode = FM_RECV;
				goto end;
			}
		}
		break;
	case V4L2_CID_PRIVATE_SILABS_SPACING:
		if (!is_valid_chan_spacing(ctrl->value)) {
			retval = -EINVAL;
			FMDERR("%s: channel spacing is not valid\n", __func__);
			goto end;
		}
		retval = set_chan_spacing(radio, (u16)ctrl->value);
		if (retval < 0) {
			FMDERR("Error in setting channel spacing\n");
			goto end;
		}
		break;
	case V4L2_CID_PRIVATE_SILABS_EMPHASIS:
		retval = set_emphasis(radio, (u16)ctrl->value);
		if (retval < 0) {
			FMDERR("Error in setting emphasis\n");
			goto end;
		}
		break;
	case V4L2_CID_PRIVATE_SILABS_ANTENNA:
		if (ctrl->value == 0 || ctrl->value == 1) {
			retval = set_property(radio,
						FM_ANTENNA_INPUT_PROP,
						ctrl->value);
			if (retval < 0)
				FMDERR("Setting antenna type failed\n");
			else
				radio->antenna = ctrl->value;
		} else	{
			retval = -EINVAL;
			FMDERR("%s: antenna type is not valid\n", __func__);
			goto end;
		}
		break;
	case V4L2_CID_PRIVATE_SILABS_SOFT_MUTE:
		retval = 0;
		break;
	case V4L2_CID_PRIVATE_SILABS_REGION:
	case V4L2_CID_PRIVATE_SILABS_SRCH_ALGORITHM:
	case V4L2_CID_PRIVATE_SILABS_SET_AUDIO_PATH:
		/*
		 * These private controls are place holders to keep the
		 * driver compatible with changes done in the frameworks
		 * which are specific to TAVARUA.
		 */
		retval = 0;
		break;
	case V4L2_CID_PRIVATE_SILABS_SRCHMODE:
		if (is_valid_srch_mode(ctrl->value)) {
			radio->g_search_mode = ctrl->value;
		} else {
			FMDERR("%s: srch mode is not valid\n", __func__);
			retval = -EINVAL;
			goto end;
		}
		break;
	case V4L2_CID_PRIVATE_SILABS_SCANDWELL:
		if ((ctrl->value >= 0) && (ctrl->value <= 0x0F)) {
			radio->dwell_time = ctrl->value;
		} else {
			FMDERR("%s: scandwell period is not valid\n", __func__);
			retval = -EINVAL;
		}
		break;
	case V4L2_CID_PRIVATE_SILABS_SRCHON:
		retval = silabs_search(radio, (bool)ctrl->value);
		break;
	case V4L2_CID_PRIVATE_SILABS_RDS_STD:
		return retval;
		break;
	case V4L2_CID_PRIVATE_SILABS_RDSON:
		return retval;
		break;
	case V4L2_CID_PRIVATE_SILABS_RDSGROUP_MASK:
		retval = set_property(radio, FM_RDS_INT_SOURCE_PROP, 0x07);
		break;
	case V4L2_CID_PRIVATE_SILABS_RDSD_BUF:
		retval = set_property(radio, FM_RDS_INT_FIFO_COUNT_PROP, 0x01);
		break;
	case V4L2_CID_PRIVATE_SILABS_RDSGROUP_PROC:
		/* Enabled all with uncorrectable */
		retval = set_property(radio, FM_RDS_CONFIG_PROP, 0xFF01);
		break;
	case V4L2_CID_PRIVATE_SILABS_LP_MODE:
		FMDBG("In %s, V4L2_CID_PRIVATE_SILABS_LP_MODE, val is %d\n",
			__func__, ctrl->value);
		if (ctrl->value)
			/* disable RDS interrupts */
			retval = configure_interrupts(radio,
					ENABLE_STC_INTERRUPTS);
		else
			/* enable RDS interrupts */
			retval = configure_interrupts(radio,
					ENABLE_STC_RDS_INTERRUPTS);
		if (retval < 0) {
			FMDERR("In %s, setting low power mode failed %d\n",
				__func__, retval);
			goto end;
		}
		break;
	default:
		retval = -EINVAL;
		break;
	}

	if (retval < 0)
		FMDERR("set control failed with %d, id:%x\n", retval, ctrl->id);

end:
	return retval;
}

static int silabs_fm_vidioc_s_tuner(struct file *file, void *priv,
	const struct v4l2_tuner *tuner)
{
	struct silabs_fm_device *radio = video_get_drvdata(video_devdata(file));
	int retval = 0;
	u16 prop_val = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(tuner == NULL)) {
		FMDERR("%s:tuner is null", __func__);
		return -EINVAL;
	}

	if (tuner->index > 0)
		return -EINVAL;

	FMDBG("In %s, setting top and bottom band limits\n", __func__);

	prop_val = (u16)((tuner->rangelow / TUNE_PARAM) / TUNE_STEP_SIZE);
	FMDBG("In %s, tuner->rangelow is %d, setting bottom band to %d\n",
		__func__, tuner->rangelow, prop_val);

	retval = set_property(radio, FM_SEEK_BAND_BOTTOM_PROP, prop_val);
	if (retval < 0)
		FMDERR("In %s, error %d setting lower limit freq\n",
			__func__, retval);
	else
		radio->recv_conf.band_low_limit = prop_val;

	prop_val = (u16)((tuner->rangehigh / TUNE_PARAM) / TUNE_STEP_SIZE);
	FMDBG("In %s, tuner->rangehigh is %d, setting top band to %d\n",
		__func__, tuner->rangehigh, prop_val);

	retval = set_property(radio, FM_SEEK_BAND_TOP_PROP, prop_val);
	if (retval < 0)
		FMDERR("In %s, error %d setting upper limit freq\n",
			__func__, retval);
	else
		radio->recv_conf.band_high_limit = prop_val;

	if (retval < 0)
		FMDERR(": set tuner failed with %d\n", retval);

	return retval;
}

static int silabs_fm_vidioc_g_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	int retval = 0;
	struct silabs_fm_device *radio = video_get_drvdata(video_devdata(file));

	if (unlikely(radio == NULL)) {
		FMDERR(":radio is null");
		return -EINVAL;
	}
	if (tuner == NULL) {
		FMDERR("%s, tuner is null\n", __func__);
		return -EINVAL;
	}
	if (tuner->index > 0) {
		FMDERR("Invalid Tuner Index");
		return -EINVAL;
	}

	mutex_lock(&radio->lock);

	memset(radio->write_buf, 0, WRITE_REG_NUM);

	/* track command that is being sent to chip. */
	radio->cmd = FM_TUNE_STATUS_CMD;

	radio->write_buf[0] = FM_TUNE_STATUS_CMD;
	radio->write_buf[1] = 0;

	retval = send_cmd(radio, TUNE_STATUS_CMD_LEN);
	if (retval < 0) {
		FMDERR("In %s, FM_TUNE_STATUS_CMD failed with error %d\n",
				__func__, retval);
		mutex_unlock(&radio->lock);
		goto get_prop_fail;
	}

	/* rssi */
	tuner->signal = radio->read_buf[4];
	mutex_unlock(&radio->lock);

	retval = get_property(radio,
				FM_SEEK_BAND_BOTTOM_PROP,
				&radio->recv_conf.band_low_limit);
	if (retval < 0) {
		FMDERR("%s: get FM_SEEK_BAND_BOTTOM_PROP failed, error %d\n",
				__func__, retval);
		goto get_prop_fail;
	}

	FMDBG("In %s, radio->recv_conf.band_low_limit is %d\n",
		__func__, radio->recv_conf.band_low_limit);
	retval = get_property(radio,
				FM_SEEK_BAND_TOP_PROP,
				&radio->recv_conf.band_high_limit);
	if (retval < 0) {
		FMDERR("In %s, get FM_SEEK_BAND_TOP_PROP failed, error %d\n",
				__func__, retval);
		goto get_prop_fail;
	}
	FMDBG("In %s, radio->recv_conf.band_high_limit is %d\n",
		__func__, radio->recv_conf.band_high_limit);

	tuner->type = V4L2_TUNER_RADIO;
	tuner->rangelow  =
		radio->recv_conf.band_low_limit * TUNE_STEP_SIZE * TUNE_PARAM;
	tuner->rangehigh =
		radio->recv_conf.band_high_limit * TUNE_STEP_SIZE * TUNE_PARAM;
	tuner->rxsubchans = V4L2_TUNER_SUB_MONO | V4L2_TUNER_SUB_STEREO;
	tuner->capability = V4L2_TUNER_CAP_LOW;

	tuner->audmode = 0;
	tuner->afc = 0;

get_prop_fail:
	return retval;
}

static int silabs_fm_vidioc_g_frequency(struct file *file, void *priv,
		struct v4l2_frequency *freq)
{
	struct silabs_fm_device *radio = video_get_drvdata(video_devdata(file));
	u32 f;
	u8 snr, rssi;
	int retval = 0;

	if (unlikely(radio == NULL)) {
		FMDERR(":radio is null");
		return -EINVAL;
	}

	if (freq == NULL) {
		FMDERR("%s, v4l2 freq is null\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&radio->lock);
	memset(radio->write_buf, 0, WRITE_REG_NUM);

	/* track command that is being sent to chip. */
	radio->cmd = FM_TUNE_STATUS_CMD;

	radio->write_buf[0] = FM_TUNE_STATUS_CMD;
	radio->write_buf[1] = 0;

	retval = send_cmd(radio, TUNE_STATUS_CMD_LEN);
	if (retval < 0) {
		FMDERR("In %s, get station freq cmd failed with error %d\n",
				__func__, retval);
		mutex_unlock(&radio->lock);
		goto send_cmd_fail;
	}

	f = (radio->read_buf[2] << 8) + radio->read_buf[3];
	freq->frequency = f * TUNE_PARAM * TUNE_STEP_SIZE;

	rssi = radio->read_buf[4];
	snr = radio->read_buf[5];
	mutex_unlock(&radio->lock);

	FMDBG("In %s, freq is %d, rssi %u, snr %u\n",
		__func__, f * TUNE_STEP_SIZE, rssi, snr);

send_cmd_fail:
	return retval;
}

static int silabs_fm_vidioc_s_frequency(struct file *file, void *priv,
					const struct v4l2_frequency *freq)
{
	struct silabs_fm_device *radio = video_get_drvdata(video_devdata(file));
	int retval = -1;
	u32 f = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(freq == NULL)) {
		FMDERR("%s:freq is null", __func__);
		return -EINVAL;
	}

	if (freq->type != V4L2_TUNER_RADIO)
		return -EINVAL;

	f = (freq->frequency)/TUNE_PARAM;

	FMDBG("Calling tune with freq %u\n", f);

	radio->seek_tune_status = TUNE_PENDING;

	retval = tune(radio, f);

	/* save the current frequency if tune is successful. */
	if (retval > 0)
		radio->tuned_freq_khz = f;

	return retval;
}

static int silabs_fm_vidioc_s_hw_freq_seek(struct file *file, void *priv,
					const struct v4l2_hw_freq_seek *seek)
{
	struct silabs_fm_device *radio = video_get_drvdata(video_devdata(file));
	int dir;
	int retval = 0;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (unlikely(seek == NULL)) {
		FMDERR("%s:seek is null", __func__);
		return -EINVAL;
	}

	if (seek->seek_upward)
		dir = SRCH_DIR_UP;
	else
		dir = SRCH_DIR_DOWN;

	if (radio->g_search_mode == 0) {
		/* seek */
		FMDBG("starting seek\n");

		radio->seek_tune_status = SEEK_PENDING;

		return silabs_seek(radio, dir, 1);

	} else if (radio->g_search_mode == 1) {
		/* scan */
		FMDBG("starting scan\n");

		return silabs_search(radio, 1);

	} else {
		retval = -EINVAL;
		FMDERR("In %s, invalid search mode %d\n",
			__func__, radio->g_search_mode);
	}

	return retval;
}

static int silabs_fm_vidioc_dqbuf(struct file *file, void *priv,
				struct v4l2_buffer *buffer)
{

	struct silabs_fm_device *radio = video_get_drvdata(video_devdata(file));
	enum silabs_buf_t buf_type = -1;
	u8 buf_fifo[STD_BUF_SIZE] = {0};
	struct kfifo *data_fifo = NULL;
	u8 *buf = NULL;
	unsigned int len = 0, retval = -1;

	if ((radio == NULL) || (buffer == NULL)) {
		FMDERR("radio/buffer is NULL\n");
		return -ENXIO;
	}
	buf_type = buffer->index;
	buf = (unsigned char *)buffer->m.userptr;
	len = buffer->length;
	FMDBG("%s: requesting buffer %d\n", __func__, buf_type);

	if ((buf_type < SILABS_FM_BUF_MAX) && (buf_type >= 0)) {
		data_fifo = &radio->data_buf[buf_type];
		if (buf_type == SILABS_FM_BUF_EVENTS) {
			if (wait_event_interruptible(radio->event_queue,
				kfifo_len(data_fifo)) < 0) {
				return -EINTR;
			}
		}
	} else {
		FMDERR("invalid buffer type\n");
		return -EINVAL;
	}
	if (len <= STD_BUF_SIZE) {
		buffer->bytesused = kfifo_out_locked(data_fifo, &buf_fifo[0],
					len, &radio->buf_lock[buf_type]);
	} else {
		FMDERR("kfifo_out_locked can not use len more than 128\n");
		return -EINVAL;
	}
	retval = copy_to_user(buf, &buf_fifo[0], buffer->bytesused);
	if (retval > 0) {
		FMDERR("Failed to copy %d bytes of data\n", retval);
		return -EAGAIN;
	}

	return retval;
}

static int silabs_fm_vidioc_g_fmt_type_private(struct file *file, void *priv,
						struct v4l2_format *f)
{
	return 0;

}

static const struct v4l2_ioctl_ops silabs_fm_ioctl_ops = {
	.vidioc_querycap              = silabs_fm_vidioc_querycap,
	.vidioc_queryctrl             = silabs_fm_vidioc_queryctrl,
	.vidioc_g_ctrl                = silabs_fm_vidioc_g_ctrl,
	.vidioc_s_ctrl                = silabs_fm_vidioc_s_ctrl,
	.vidioc_g_tuner               = silabs_fm_vidioc_g_tuner,
	.vidioc_s_tuner               = silabs_fm_vidioc_s_tuner,
	.vidioc_g_frequency           = silabs_fm_vidioc_g_frequency,
	.vidioc_s_frequency           = silabs_fm_vidioc_s_frequency,
	.vidioc_s_hw_freq_seek        = silabs_fm_vidioc_s_hw_freq_seek,
	.vidioc_dqbuf                 = silabs_fm_vidioc_dqbuf,
	.vidioc_g_fmt_type_private    = silabs_fm_vidioc_g_fmt_type_private,
};

static const struct v4l2_file_operations silabs_fm_fops = {
	.owner = THIS_MODULE,
	.ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = v4l2_compat_ioctl32,
#endif
	.open = silabs_fm_fops_open,
	.release = silabs_fm_fops_release,
};


static const struct video_device silabs_fm_viddev_template = {
	.fops                   = &silabs_fm_fops,
	.ioctl_ops              = &silabs_fm_ioctl_ops,
	.name                   = DRIVER_NAME,
	.release                = video_device_release,
};

static int silabs_fm_probe(struct platform_device *pdev)
{

	struct silabs_fm_device *radio;
	int retval = 0;
	int i = 0;
	int kfifo_alloc_rc = 0;

	if (unlikely(pdev == NULL)) {
		FMDERR("%s:pdev is null", __func__);
		return -EINVAL;
	}
	/* private data allocation */
	radio = kzalloc(sizeof(struct silabs_fm_device), GFP_KERNEL);
	if (!radio) {
		FMDERR("Memory not allocated for radio\n");
		retval = -ENOMEM;
		goto err_initial;
	}
	radio->dev = &pdev->dev;
	radio->wqueue = NULL;
	radio->wqueue_scan = NULL;

	/* video device allocation */
	radio->videodev = video_device_alloc();
	if (!radio->videodev) {
		FMDERR("radio->videodev is NULL\n");
		goto err_radio;
	}
	/* initial configuration */
	memcpy(radio->videodev, &silabs_fm_viddev_template,
	  sizeof(silabs_fm_viddev_template));

	/*allocate internal buffers for decoded rds and event buffer*/
	for (i = 0; i < SILABS_FM_BUF_MAX; i++) {
		spin_lock_init(&radio->buf_lock[i]);

		if (i == SILABS_FM_BUF_RAW_RDS)
			kfifo_alloc_rc = kfifo_alloc(&radio->data_buf[i],
				FM_RDS_BUF * 3, GFP_KERNEL);
		else if (i == SILABS_FM_BUF_RT_RDS)
			kfifo_alloc_rc = kfifo_alloc(&radio->data_buf[i],
				STD_BUF_SIZE * 2, GFP_KERNEL);
		else
			kfifo_alloc_rc = kfifo_alloc(&radio->data_buf[i],
				STD_BUF_SIZE, GFP_KERNEL);

		if (kfifo_alloc_rc != 0) {
			FMDERR("%s: failed allocating buffers %d\n",
				__func__, kfifo_alloc_rc);
			retval = -ENOMEM;
			goto err_fifo_alloc;
		}
	}
	/* initializing the device count  */
	atomic_set(&radio->users, 1);

	/* radio initializes to low power mode */
	radio->lp_mode = 1;
	radio->handle_irq = 1;
	/* init lock */
	mutex_init(&radio->lock);
	radio->tune_req = 0;
	radio->seek_tune_status = 0;
	init_completion(&radio->sync_req_done);
	/* initialize wait queue for event read */
	init_waitqueue_head(&radio->event_queue);
	/* initialize wait queue for raw rds read */
	init_waitqueue_head(&radio->read_queue);

	radio->dev = &pdev->dev;

	platform_set_drvdata(pdev, radio);
	video_set_drvdata(radio->videodev, radio);

	/*
	 * Start the worker thread for event handling and register read_int_stat
	 * as worker function
	 */
	radio->wqueue  = create_singlethread_workqueue("sifmradio");

	if (!radio->wqueue) {
		retval = -ENOMEM;
		goto err_fifo_alloc;
	}

	FMDBG("%s: creating work q for scan\n", __func__);
	radio->wqueue_scan  = create_singlethread_workqueue("sifmradioscan");

	if (!radio->wqueue_scan) {
		retval = -ENOMEM;
		goto err_wqueue_scan;
	}

	/* register video device */
	retval = video_register_device(radio->videodev,
			VFL_TYPE_RADIO,
			RADIO_NR);
	if (retval != 0) {
		FMDERR("Could not register video device\n");
		goto err_all;
	}
	g_radio = radio;
	return 0;

err_all:
	destroy_workqueue(radio->wqueue_scan);
err_wqueue_scan:
	destroy_workqueue(radio->wqueue);
err_fifo_alloc:
	for (i--; i >= 0; i--)
		kfifo_free(&radio->data_buf[i]);
	video_device_release(radio->videodev);
err_radio:
	kfree(radio);
err_initial:
	return retval;
}

static int silabs_fm_remove(struct platform_device *pdev)
{
	int i;
	struct silabs_fm_device *radio = platform_get_drvdata(pdev);

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	/* disable irq */
	destroy_workqueue(radio->wqueue);
	destroy_workqueue(radio->wqueue_scan);

	video_unregister_device(radio->videodev);

	/* free internal buffers */
	for (i = 0; i < SILABS_FM_BUF_MAX; i++)
		kfifo_free(&radio->data_buf[i]);

	/* free state struct */
	kfree(radio);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id silabs_fm_match[] = {
	{.compatible = "silabs,silabs-fm"},
	{}
};

static struct platform_driver silabs_fm_driver = {
	.probe  = silabs_fm_probe,
	.driver = {
		.owner  = THIS_MODULE,
		.name   = "silabs-fm",
		.of_match_table = silabs_fm_match,
	},
	.remove  = silabs_fm_remove,
};


static int __init radio_module_init(void)
{
	return platform_driver_register(&silabs_fm_driver);
}
module_init(radio_module_init);

static void __exit radio_module_exit(void)
{
	platform_driver_unregister(&silabs_fm_driver);
}
module_exit(radio_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DRIVER_DESC);

