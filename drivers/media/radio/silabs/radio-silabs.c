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
#include <linux/videodev2.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/atomic.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include "radio-silabs.h"

struct silabs_fm_device {
	struct i2c_client *client;
	struct pwm_device *pwm;
	bool is_len_gpio_valid;
	struct fm_power_vreg_data *dreg;
	struct fm_power_vreg_data *areg;
	int reset_gpio;
	int int_gpio;
	int status_gpio;
	struct pinctrl *fm_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
	struct video_device *videodev;
	/* driver management */
	atomic_t users;
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
	bool is_search_cancelled;
	unsigned int mode;
	/* regional settings */
	enum silabs_region_t region;
	/* power mode */
	bool lp_mode;
	int handle_irq;
	/* global lock */
	struct mutex lock;
	/* buffer locks*/
	spinlock_t buf_lock[SILABS_FM_BUF_MAX];
	/* work queue */
	struct workqueue_struct *wqueue;
	struct workqueue_struct *wqueue_scan;
	struct workqueue_struct *wqueue_af;
	struct workqueue_struct *wqueue_rds;
	struct work_struct rds_worker;
	struct delayed_work work;
	struct delayed_work work_scan;
	struct delayed_work work_af;
	/* wait queue for blocking event read */
	wait_queue_head_t event_queue;
	/* wait queue for raw rds read */
	wait_queue_head_t read_queue;
	int irq;
	int status_irq;
	int tuned_freq_khz;
	int dwell_time_sec;
	u16 pi; /* PI of tuned channel */
	u8 pty; /* programe type of the tuned channel */
	u16 block[NO_OF_RDS_BLKS];
	u8 rt_display[MAX_RT_LEN];   /* RT that will be displayed */
	u8 rt_tmp0[MAX_RT_LEN]; /* high probability RT */
	u8 rt_tmp1[MAX_RT_LEN]; /* low probability RT */
	u8 rt_cnt[MAX_RT_LEN];  /* high probability RT's hit count */
	u8 rt_flag;          /* A/B flag of RT */
	bool valid_rt_flg;     /* validity of A/B flag */
	u8 ps_display[MAX_PS_LEN];    /* PS that will be displayed */
	u8 ps_tmp0[MAX_PS_LEN]; /* high probability PS */
	u8 ps_tmp1[MAX_PS_LEN]; /* low probability PS */
	u8 ps_cnt[MAX_PS_LEN];  /* high probability PS's hit count */
	u8 rt_plus_carrier;
	u8 ert_carrier;
	u8 ert_buf[MAX_ERT_LEN];
	u8 ert_len;
	u8 c_byt_pair_index;
	u8 utf_8_flag;
	u8 rt_ert_flag;
	u8 formatting_dir;
	bool is_af_jump_enabled;
	bool is_af_tune_in_progress;
	u8 af_rssi_th; /* allowed rssi is 0-127 */
	u8 rssi_th; /* 0 - 127 */
	u8 sinr_th; /* 0 - 127 */
	u8 rds_fifo_cnt; /* 0 - 25 */
	struct silabs_af_info af_info1;
	struct silabs_af_info af_info2;
};

static int silabs_fm_request_irq(struct silabs_fm_device *radio);
static int tune(struct silabs_fm_device *radio, u32 freq);
static int silabs_seek(struct silabs_fm_device *radio, int dir, int wrap);
static int cancel_seek(struct silabs_fm_device *radio);
static int configure_interrupts(struct silabs_fm_device *radio, u8 val);
static void silabs_fm_q_event(struct silabs_fm_device *radio,
				enum silabs_evt_t event);

static bool is_valid_rssi(int rssi)
{
	if ((rssi >= MIN_RSSI) &&
		(rssi <= MAX_RSSI))
		return true;
	else
		return false;
}

static bool is_valid_sinr(int sinr)
{
	if ((sinr >= MIN_SNR) &&
		(sinr <= MAX_SNR))
		return true;
	else
		return false;
}

static bool is_valid_rds_fifo_cnt(int cnt)
{
	if ((cnt >= MIN_RDS_FIFO_CNT) &&
		(cnt <= MAX_RDS_FIFO_CNT))
		return true;
	else
		return false;
}

static int silabs_fm_i2c_read(struct silabs_fm_device *radio, u8 len)
{
	int i = 0, retval = 0;
	struct i2c_msg msgs[1];

	msgs[0].addr = radio->client->addr;
	msgs[0].len = len;
	msgs[0].flags = I2C_M_RD;
	msgs[0].buf = (u8 *)radio->read_buf;

	for (i = 0; i < 2; i++) {
		retval = i2c_transfer(radio->client->adapter, msgs, 1);
		if (retval == 1)
			break;
	}

	return retval;
}

static int silabs_fm_i2c_write(struct silabs_fm_device *radio, u8 len)
{
	struct i2c_msg msgs[1];
	int i = 0, retval = 0;

	msgs[0].addr = radio->client->addr;
	msgs[0].len = len;
	msgs[0].flags = 0;
	msgs[0].buf = (u8 *)radio->write_buf;

	for (i = 0; i < 2; i++) {
		retval = i2c_transfer(radio->client->adapter, msgs, 1);
		if (retval == 1)
			break;
	}

	return retval;
}

static int silabs_fm_pinctrl_select(struct silabs_fm_device *radio, bool on)
{
	struct pinctrl_state *pins_state;
	int ret;

	pins_state = on ? radio->gpio_state_active
			: radio->gpio_state_suspend;

	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(radio->fm_pinctrl, pins_state);
		if (ret) {
			FMDERR("%s: cannot set pin state\n", __func__);
			return ret;
		}
	} else {
		FMDERR("%s: not a valid %s pin state\n", __func__,
				on ? "pmx_fm_active" : "pmx_fm_suspend");
	}

	return 0;
}

static int fm_configure_gpios(struct silabs_fm_device *radio, bool on)
{
	int rc = 0;
	int fm_reset_gpio = radio->reset_gpio;
	int fm_int_gpio = radio->int_gpio;
	int fm_status_gpio = radio->status_gpio;

	if (on) {
		/*
		 * Turn ON sequence
		 * GPO1/status gpio configuration.
		 * Keep the GPO1 to high till device comes out of reset.
		 */
		if (fm_status_gpio > 0) {
			FMDERR("status gpio is provided, setting it to high\n");
			rc = gpio_direction_output(fm_status_gpio, 1);
			if (rc) {
				FMDERR("unable to set gpio %d direction(%d)\n",
				fm_status_gpio, rc);
				return rc;
			}
			/* Wait for the value to take effect on gpio. */
			msleep(100);
		}

		/*
		 * GPO2/Interrupt gpio configuration.
		 * Keep the GPO2 to low till device comes out of reset.
		 */
		rc = gpio_direction_output(fm_int_gpio, 0);
		if (rc) {
			FMDERR("unable to set the gpio %d direction(%d)\n",
			fm_int_gpio, rc);
			return rc;
		}
		/* Wait for the value to take effect on gpio. */
		msleep(100);

		/*
		 * Reset pin configuration.
		 * write "0'' to make sure the chip is in reset.
		 */
		rc = gpio_direction_output(fm_reset_gpio, 0);
		if (rc) {
			FMDERR("Unable to set direction\n");
			return rc;
		}
		/* Wait for the value to take effect on gpio. */
		msleep(100);
		/* write "1" to bring the chip out of reset.*/
		rc = gpio_direction_output(fm_reset_gpio, 1);
		if (rc) {
			FMDERR("Unable to set direction\n");
			return rc;
		}
		/* Wait for the value to take effect on gpio. */
		msleep(100);

		rc = gpio_direction_input(fm_int_gpio);
		if (rc) {
			FMDERR("unable to set the gpio %d direction(%d)\n",
						fm_int_gpio, rc);
			return rc;
		}
		/* Wait for the value to take effect on gpio. */
		msleep(100);

		if (fm_status_gpio > 0) {
			FMDERR("setting status gpio as input\n");
			rc = gpio_direction_input(fm_status_gpio);
			if (rc) {
				FMDERR("unable to set gpio %d direction(%d)\n",
				fm_status_gpio, rc);
				return rc;
			}
			/* Wait for the value to take effect on gpio. */
			msleep(100);
		}


	} else {
		/*Turn OFF sequence */
		gpio_set_value(fm_reset_gpio, 0);

		rc = gpio_direction_input(fm_reset_gpio);
		if (rc)
			FMDERR("Unable to set direction\n");
		/* Wait for some time for the value to take effect. */
		msleep(100);
		if (fm_status_gpio > 0) {
			rc = gpio_direction_input(fm_status_gpio);
			if (rc)
				FMDERR("Unable to set dir for status gpio\n");
			msleep(100);
		}
	}
	return rc;
}

static int silabs_fm_areg_cfg(struct silabs_fm_device *radio, bool on)
{
	int rc = 0;
	struct fm_power_vreg_data *vreg;

	vreg = radio->areg;
	if (!vreg) {
		FMDERR("In %s, areg is NULL\n", __func__);
		return rc;
	}
	if (on) {
		FMDBG("vreg is : %s", vreg->name);
		if (vreg->set_voltage_sup) {
			rc = regulator_set_voltage(vreg->reg,
						vreg->low_vol_level,
						vreg->high_vol_level);
			if (rc < 0) {
				FMDERR("set_vol(%s) fail %d\n", vreg->name, rc);
				return rc;
			}
		}
		rc = regulator_enable(vreg->reg);
		if (rc < 0) {
			FMDERR("reg enable(%s) failed.rc=%d\n", vreg->name, rc);
			if (vreg->set_voltage_sup) {
				regulator_set_voltage(vreg->reg,
						0,
						vreg->high_vol_level);
			}
			return rc;
		}
		vreg->is_enabled = true;

	} else {
		rc = regulator_disable(vreg->reg);
		if (rc < 0) {
			FMDERR("reg disable(%s) fail rc=%d\n", vreg->name, rc);
			return rc;
		}
		vreg->is_enabled = false;

		if (vreg->set_voltage_sup) {
			/* Set the min voltage to 0 */
			rc = regulator_set_voltage(vreg->reg,
						0,
						vreg->high_vol_level);
			if (rc < 0) {
				FMDERR("set_vol(%s) fail %d\n", vreg->name, rc);
				return rc;
			}
		}
	}
	return rc;
}

static int silabs_fm_dreg_cfg(struct silabs_fm_device *radio, bool on)
{
	int rc = 0;
	struct fm_power_vreg_data *vreg;

	vreg = radio->dreg;
	if (!vreg) {
		FMDERR("In %s, dreg is NULL\n", __func__);
		return rc;
	}

	if (on) {
		FMDBG("vreg is : %s", vreg->name);
		if (vreg->set_voltage_sup) {
			rc = regulator_set_voltage(vreg->reg,
						vreg->low_vol_level,
						vreg->high_vol_level);
			if (rc < 0) {
				FMDERR("set_vol(%s) fail %d\n", vreg->name, rc);
				return rc;
			}
		}

		rc = regulator_enable(vreg->reg);
		if (rc < 0) {
			FMDERR("reg enable(%s) failed.rc=%d\n", vreg->name, rc);
			if (vreg->set_voltage_sup) {
				regulator_set_voltage(vreg->reg,
						0,
						vreg->high_vol_level);
			}
			return rc;
		}
			vreg->is_enabled = true;
	} else {
		rc = regulator_disable(vreg->reg);
		if (rc < 0) {
			FMDERR("reg disable(%s) fail. rc=%d\n", vreg->name, rc);
			return rc;
		}
		vreg->is_enabled = false;

		if (vreg->set_voltage_sup) {
			/* Set the min voltage to 0 */
			rc = regulator_set_voltage(vreg->reg,
						0,
						vreg->high_vol_level);
			if (rc < 0) {
				FMDERR("set_vol(%s) fail %d\n", vreg->name, rc);
				return rc;
			}
		}
	}
	return rc;
}

static int silabs_fm_power_cfg(struct silabs_fm_device *radio, bool on)
{
	int rc = 0;

	if (on) {
		/* Turn ON sequence */
		rc = silabs_fm_dreg_cfg(radio, on);
		if (rc < 0) {
			FMDERR("In %s, dreg cfg failed %x\n", __func__, rc);
			return rc;
		}
		rc = silabs_fm_areg_cfg(radio, on);
		if (rc < 0) {
			FMDERR("In %s, areg cfg failed %x\n", __func__, rc);
			silabs_fm_dreg_cfg(radio, false);
			return rc;
		}
		/* If pinctrl is supported, select active state */
		if (radio->fm_pinctrl) {
			rc = silabs_fm_pinctrl_select(radio, true);
			if (rc)
				FMDERR("%s: error setting active pin state\n",
								__func__);
		}

		rc = fm_configure_gpios(radio, on);
		if (rc < 0) {
			FMDERR("fm_power gpio config failed\n");
			silabs_fm_dreg_cfg(radio, false);
			silabs_fm_areg_cfg(radio, false);
			return rc;
		}
	} else {
		/* Turn OFF sequence */
		rc = fm_configure_gpios(radio, on);
		if (rc < 0)
			FMDERR("fm_power gpio config failed");

		/* If pinctrl is supported, select suspend state */
		if (radio->fm_pinctrl) {
			rc = silabs_fm_pinctrl_select(radio, false);
			if (rc)
				FMDERR("%s: error setting suspend pin state\n",
								__func__);
		}
		rc = silabs_fm_dreg_cfg(radio, on);
		if (rc < 0)
			FMDERR("In %s, dreg cfg failed %x\n", __func__, rc);
		rc = silabs_fm_areg_cfg(radio, on);
		if (rc < 0)
			FMDERR("In %s, areg cfg failed %x\n", __func__, rc);
	}
	return rc;
}

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

		retval = silabs_fm_i2c_read(radio, READ_REG_NUM);

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
			FMDBG("In %s, CTS bit is set\n", __func__);
			break;
		}
		/*
		 * Give some time if the chip is not done with processing
		 * previous command.
		 */
		msleep(100);
	}

	FMDBG("In %s, status byte is %x\n",  __func__, radio->read_buf[0]);

bad_cmd_arg:
	return retval;
}

static int send_cmd(struct silabs_fm_device *radio, u8 total_len)
{
	int retval = 0;

	retval = silabs_fm_i2c_write(radio, total_len);

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
	radio = container_of(work, struct silabs_fm_device, work_scan.work);

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
		/* If scan is cancelled or FM is not ON, break */
		if (radio->is_search_cancelled == true) {
			FMDBG("%s: scan cancelled\n", __func__);
			goto seek_cancelled;
		} else if (radio->mode != FM_RECV) {
			FMDERR("%s: FM is not in proper state\n", __func__);
			return;
		}

		retval = silabs_seek(radio, SRCH_DIR_UP, WRAP_DISABLE);
		if (retval < 0) {
			FMDERR("Scan operation failed with error %d\n", retval);
			goto seek_tune_fail;
		}
		/* wait for seek to complete */
		if (!wait_for_completion_timeout(&radio->sync_req_done,
					msecs_to_jiffies(WAIT_TIMEOUT_MSEC))) {
			FMDERR("%s: didn't receive STC for seek\n", __func__);
			/* FM is not correct state or scan is cancelled */
			continue;
		} else
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

		valid = radio->read_buf[1] & VALID_MASK;
		bltf = radio->read_buf[1] & BLTF_MASK;

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
		/*
		 * If scan is cancelled or FM is not ON, break ASAP so that we
		 * don't need to sleep for dwell time.
		 */
		if (radio->is_search_cancelled == true) {
			FMDBG("%s: scan cancelled\n", __func__);
			goto seek_cancelled;
		} else if (radio->mode != FM_RECV) {
			FMDERR("%s: FM is not in proper state\n", __func__);
			return;
		}

		/* sleep for dwell period */
		msleep(radio->dwell_time_sec * 1000);

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
seek_cancelled:
	silabs_fm_q_event(radio, SILABS_EVT_SEEK_COMPLETE);
	radio->seek_tune_status = NO_SEEK_TUNE_PENDING;
}

static void silabs_search(struct silabs_fm_device *radio, bool on)
{
	int current_freq_khz;

	current_freq_khz = radio->tuned_freq_khz;

	if (on) {
		FMDBG("%s: Queuing the work onto scan work q\n", __func__);
		queue_delayed_work(radio->wqueue_scan, &radio->work_scan,
					msecs_to_jiffies(SILABS_DELAY_MSEC));
	} else {
		cancel_seek(radio);
		silabs_fm_q_event(radio, SILABS_EVT_SEEK_COMPLETE);
	}
}

static void get_rds_status(struct silabs_fm_device *radio)
{
	int retval = 0;

	mutex_lock(&radio->lock);
	memset(radio->write_buf, 0, WRITE_REG_NUM);
	radio->cmd = FM_RDS_STATUS_CMD;
	radio->write_buf[0] = FM_RDS_STATUS_CMD;
	radio->write_buf[1] |= FM_RDS_STATUS_IN_INTACK;

	retval = send_cmd(radio, RDS_CMD_LEN);
	if (retval < 0) {
		FMDERR("In %s, Get RDS failed %d\n", __func__, retval);
		mutex_unlock(&radio->lock);
		return;
	}

	memset(radio->read_buf, 0, sizeof(radio->read_buf));

	retval = silabs_fm_i2c_read(radio, RDS_RSP_LEN);

	if (retval < 0) {
		FMDERR("In %s, failed to read the resp from soc %d\n",
							__func__, retval);
		mutex_unlock(&radio->lock);
		return;
	} else {
		FMDBG("In %s, successfully read the response from soc\n",
								__func__);
	}

	radio->block[0] = ((u16)radio->read_buf[MSB_OF_BLK_0] << 8) |
					(u16)radio->read_buf[LSB_OF_BLK_0];
	radio->block[1] = ((u16)radio->read_buf[MSB_OF_BLK_1] << 8) |
					(u16)radio->read_buf[LSB_OF_BLK_1];
	radio->block[2] = ((u16)radio->read_buf[MSB_OF_BLK_2] << 8) |
					(u16)radio->read_buf[LSB_OF_BLK_2];
	radio->block[3] = ((u16)radio->read_buf[MSB_OF_BLK_3] << 8) |
					(u16)radio->read_buf[LSB_OF_BLK_3];
	mutex_unlock(&radio->lock);
}

static void pi_handler(struct silabs_fm_device *radio, u16 current_pi)
{
	if (radio->pi != current_pi) {
		FMDBG("PI code of radio->block[0] = %x\n", current_pi);
		radio->pi = current_pi;
	} else {
		FMDBG(" Received same PI code\n");
	}
}

static void pty_handler(struct silabs_fm_device *radio, u8 current_pty)
{
	if (radio->pty != current_pty) {
		FMDBG("PTY code of radio->block[1] = %x\n", current_pty);
		radio->pty = current_pty;
	} else {
		FMDBG("PTY repeated\n");
	}
}

static void update_ps(struct silabs_fm_device *radio, u8 addr, u8 ps)
{
	u8 i;
	bool ps_txt_chg = false;
	bool ps_cmplt = true;
	u8 *data;
	struct kfifo *data_b;

	if (radio->ps_tmp0[addr] == ps) {
		if (radio->ps_cnt[addr] < PS_VALIDATE_LIMIT) {
			radio->ps_cnt[addr]++;
		} else {
			radio->ps_cnt[addr] = PS_VALIDATE_LIMIT;
			radio->ps_tmp1[addr] = ps;
		}
	} else if (radio->ps_tmp1[addr] == ps) {
		if (radio->ps_cnt[addr] >= PS_VALIDATE_LIMIT) {
			ps_txt_chg = true;
			radio->ps_cnt[addr] = PS_VALIDATE_LIMIT + 1;
		} else {
			radio->ps_cnt[addr] = PS_VALIDATE_LIMIT;
		}
		radio->ps_tmp1[addr] = radio->ps_tmp0[addr];
		radio->ps_tmp0[addr] = ps;
	} else if (!radio->ps_cnt[addr]) {
		radio->ps_tmp0[addr] = ps;
		radio->ps_cnt[addr] = 1;
	} else {
		radio->ps_tmp1[addr] = ps;
	}

	if (ps_txt_chg) {
		for (i = 0; i < MAX_PS_LEN; i++) {
			if (radio->ps_cnt[i] > 1)
				radio->ps_cnt[i]--;
		}
	}

	for (i = 0; i < MAX_PS_LEN; i++) {
		if (radio->ps_cnt[i] < PS_VALIDATE_LIMIT) {
			ps_cmplt = false;
			return;
		}
	}

	if (ps_cmplt) {
		for (i = 0; (i < MAX_PS_LEN) &&
			(radio->ps_display[i] == radio->ps_tmp0[i]); i++)
				;
		if (i == MAX_PS_LEN) {
			FMDBG("Same PS string repeated\n");
			return;
		}

		for (i = 0; i < MAX_PS_LEN; i++)
			radio->ps_display[i] = radio->ps_tmp0[i];

		data = kmalloc(PS_EVT_DATA_LEN, GFP_ATOMIC);
		if (data != NULL) {
			data[0] = NO_OF_PS;
			data[1] = radio->pty;
			data[2] = (radio->pi >> 8) & 0xFF;
			data[3] = (radio->pi & 0xFF);
			data[4] = 0;
			memcpy(data + OFFSET_OF_PS,
					radio->ps_tmp0, MAX_PS_LEN);
			data_b = &radio->data_buf[SILABS_FM_BUF_PS_RDS];
			kfifo_in_locked(data_b, data, PS_EVT_DATA_LEN,
					&radio->buf_lock[SILABS_FM_BUF_PS_RDS]);
			FMDBG("Q the PS event\n");
			silabs_fm_q_event(radio, SILABS_EVT_NEW_PS_RDS);
			kfree(data);
		} else {
			FMDERR("Memory allocation failed for PTY\n");
		}
	}
}

static void display_rt(struct silabs_fm_device *radio)
{
	u8 len = 0, i = 0;
	u8 *data;
	struct kfifo *data_b;
	bool rt_cmplt = true;

	for (i = 0; i < MAX_RT_LEN; i++) {
		if (radio->rt_cnt[i] < RT_VALIDATE_LIMIT) {
			rt_cmplt = false;
			return;
		}
		if (radio->rt_tmp0[i] == END_OF_RT)
			break;
	}

	if (rt_cmplt) {
		while ((len < MAX_RT_LEN) && (radio->rt_tmp0[len] != END_OF_RT))
			len++;

		for (i = 0; (i < len) &&
			(radio->rt_display[i] == radio->rt_tmp0[i]); i++)
				;
		if (i == len) {
			FMDBG("Same RT string repeated\n");
			return;
		}
		for (i = 0; i < len; i++)
			radio->rt_display[i] = radio->rt_tmp0[i];
		data = kmalloc(len + OFFSET_OF_RT, GFP_ATOMIC);
		if (data != NULL) {
			data[0] = len; /* len of RT */
			data[1] = radio->pty;
			data[2] = (radio->pi >> 8) & 0xFF;
			data[3] = (radio->pi & 0xFF);
			data[4] = radio->rt_flag;
			memcpy(data + OFFSET_OF_RT, radio->rt_display, len);
			data_b = &radio->data_buf[SILABS_FM_BUF_RT_RDS];
			kfifo_in_locked(data_b, data, OFFSET_OF_RT + len,
				&radio->buf_lock[SILABS_FM_BUF_RT_RDS]);
			FMDBG("Q the RT event\n");
			silabs_fm_q_event(radio, SILABS_EVT_NEW_RT_RDS);
			kfree(data);
		} else {
			FMDERR("Memory allocation failed for PTY\n");
		}
	}
}

static void rt_handler(struct silabs_fm_device *radio, u8 ab_flg,
					u8 cnt, u8 addr, u8 *rt)
{
	u8 i;
	bool rt_txt_chg = 0;

	if (ab_flg != radio->rt_flag && radio->valid_rt_flg) {
		for (i = 0; i < sizeof(radio->rt_cnt); i++) {
			if (!radio->rt_tmp0[i]) {
				radio->rt_tmp0[i] = ' ';
				radio->rt_cnt[i]++;
			}
		}
		memset(radio->rt_cnt, 0, sizeof(radio->rt_cnt));
		memset(radio->rt_tmp0, 0, sizeof(radio->rt_tmp0));
		memset(radio->rt_tmp1, 0, sizeof(radio->rt_tmp1));
	}

	radio->rt_flag = ab_flg;
	radio->valid_rt_flg = true;

	for (i = 0; i < cnt; i++) {
		if (radio->rt_tmp0[addr+i] == rt[i]) {
			if (radio->rt_cnt[addr+i] < RT_VALIDATE_LIMIT) {
				radio->rt_cnt[addr+i]++;
			} else {
				radio->rt_cnt[addr+i] = RT_VALIDATE_LIMIT;
				radio->rt_tmp1[addr+i] = rt[i];
			}
		} else if (radio->rt_tmp1[addr+i] == rt[i]) {
			if (radio->rt_cnt[addr+i] >= RT_VALIDATE_LIMIT) {
				rt_txt_chg = true;
				radio->rt_cnt[addr+i] = RT_VALIDATE_LIMIT + 1;
			} else {
				radio->rt_cnt[addr+i] = RT_VALIDATE_LIMIT;
			}
			radio->rt_tmp1[addr+i] = radio->rt_tmp0[addr+i];
			radio->rt_tmp0[addr+i] = rt[i];
		} else if (!radio->rt_cnt[addr+i]) {
			radio->rt_tmp0[addr+i] = rt[i];
			radio->rt_cnt[addr+i] = 1;
		} else {
			radio->rt_tmp1[addr+i] = rt[i];
		}
	}

	if (rt_txt_chg) {
		for (i = 0; i < MAX_RT_LEN; i++) {
			if (radio->rt_cnt[i] > 1)
				radio->rt_cnt[i]--;
		}
	}
	display_rt(radio);
}

static void silabs_ev_ert(struct silabs_fm_device *radio)
{
	u8 *data = NULL;
	struct kfifo *data_b;

	if (radio->ert_len <= 0)
		return;

	data = kmalloc((radio->ert_len + ERT_OFFSET), GFP_ATOMIC);
	if (data != NULL) {
		data[0] = radio->ert_len;
		data[1] = radio->utf_8_flag;
		data[2] = radio->formatting_dir;
		memcpy((data + ERT_OFFSET), radio->ert_buf, radio->ert_len);
		data_b = &radio->data_buf[SILABS_FM_BUF_ERT];
		kfifo_in_locked(data_b, data, (radio->ert_len + ERT_OFFSET),
				&radio->buf_lock[SILABS_FM_BUF_ERT]);
		silabs_fm_q_event(radio, SILABS_EVT_NEW_ERT);
		kfree(data);
	}
}

static void silabs_buff_ert(struct silabs_fm_device *radio)
{
	int i;
	u16 info_byte = 0;
	u8 byte_pair_index;

	byte_pair_index = radio->block[1] & APP_GRP_typ_MASK;
	if (byte_pair_index == 0) {
		radio->c_byt_pair_index = 0;
		radio->ert_len = 0;
	}
	FMDBG("c_byt_pair_index = %x\n", radio->c_byt_pair_index);
	if (radio->c_byt_pair_index == byte_pair_index) {
		for (i = 2; i <= 3; i++) {
			info_byte = radio->block[i];
			FMDBG("info_byte = %x\n", info_byte);
			FMDBG("ert_len = %x\n", radio->ert_len);
			if (radio->ert_len > (MAX_ERT_LEN - 2))
				return;
			radio->ert_buf[radio->ert_len] = radio->block[i] >> 8;
			radio->ert_buf[radio->ert_len + 1] =
							radio->block[i] & 0xFF;
			radio->ert_len += ERT_CNT_PER_BLK;
			FMDBG("utf_8_flag = %d\n", radio->utf_8_flag);
			if ((radio->utf_8_flag == 0) &&
					(info_byte == END_OF_RT)) {
				radio->ert_len -= ERT_CNT_PER_BLK;
				break;
			} else if ((radio->utf_8_flag == 1) &&
					(radio->block[i] >> 8 == END_OF_RT)) {
				info_byte = END_OF_RT;
				radio->ert_len -= ERT_CNT_PER_BLK;
				break;
			} else if ((radio->utf_8_flag == 1) &&
					((radio->block[i] & 0xFF)
						 == END_OF_RT)) {
				info_byte = END_OF_RT;
				radio->ert_len--;
				break;
			}
		}
		if ((byte_pair_index == MAX_ERT_SEGMENT) ||
			(info_byte == END_OF_RT)) {
			silabs_ev_ert(radio);
			radio->c_byt_pair_index = 0;
			radio->ert_len = 0;
		}
		radio->c_byt_pair_index++;
	} else {
		radio->ert_len = 0;
		radio->c_byt_pair_index = 0;
	}
}


static void silabs_rt_plus(struct silabs_fm_device *radio)
{
	u8 tag_type1, tag_type2;
	u8 *data = NULL;
	int len = 0;
	u16 grp_typ;
	struct kfifo *data_b;

	grp_typ = radio->block[1] & APP_GRP_typ_MASK;
	/*
	 *right most 3 bits of Lsb of block 2
	 * and left most 3 bits of Msb of block 3
	 */
	tag_type1 = (((grp_typ & TAG1_MSB_MASK) << TAG1_MSB_OFFSET) |
			 (radio->block[2] >> TAG1_LSB_OFFSET));
	/*
	 *right most 1 bit of lsb of 3rd block
	 * and left most 5 bits of Msb of 4th block
	 */
	tag_type2 = (((radio->block[2] & TAG2_MSB_MASK)
			 << TAG2_MSB_OFFSET) |
			 (radio->block[2] >> TAG2_LSB_OFFSET));

	if (tag_type1 != DUMMY_CLASS)
		len += RT_PLUS_LEN_1_TAG;
	if (tag_type2 != DUMMY_CLASS)
		len += RT_PLUS_LEN_1_TAG;

	if (len != 0) {
		len += RT_PLUS_OFFSET;
		data = kmalloc(len, GFP_ATOMIC);
	} else {
		FMDERR("%s:Len is zero\n", __func__);
		return;
	}
	if (data != NULL) {
		data[0] = len;
		len = RT_ERT_FLAG_OFFSET;
		data[len++] = radio->rt_ert_flag;
		if (tag_type1 != DUMMY_CLASS) {
			data[len++] = tag_type1;
			/*
			 *start position of tag1
			 *right most 5 bits of msb of 3rd block
			 *and left most bit of lsb of 3rd block
			 */
			 data[len++] = (radio->block[2] >> TAG1_POS_LSB_OFFSET)
							& TAG1_POS_MSB_MASK;
			/*
			 *length of tag1
			 *left most 6 bits of lsb of 3rd block
			 */
			data[len++] = (radio->block[2] >> TAG1_LEN_OFFSET) &
								TAG1_LEN_MASK;
		}
		if (tag_type2 != DUMMY_CLASS) {
			data[len++] = tag_type2;
			/*
			 *start position of tag2
			 *right most 3 bit of msb of 4th block
			 *and left most 3 bits of lsb of 4th block
			 */
			data[len++] = (radio->block[3] >> TAG2_POS_LSB_OFFSET) &
							TAG2_POS_MSB_MASK;
			/*
			 *length of tag2
			 *right most 5 bits of lsb of 4th block
			 */
			data[len++] = radio->block[3] & TAG2_LEN_MASK;
		}
		data_b = &radio->data_buf[SILABS_FM_BUF_RT_PLUS];
		kfifo_in_locked(data_b, data, len,
				&radio->buf_lock[SILABS_FM_BUF_RT_PLUS]);
		silabs_fm_q_event(radio, SILABS_EVT_NEW_RT_PLUS);
		kfree(data);
	} else {
		FMDERR("%s:memory allocation failed\n", __func__);
	}
}

static void silabs_raw_rds_handler(struct silabs_fm_device *radio)
{
	u16 aid, app_grp_typ;

	aid = radio->block[3];
	app_grp_typ = radio->block[1] & APP_GRP_typ_MASK;
	FMDBG("app_grp_typ = %x\n", app_grp_typ);
	FMDBG("AID = %x", aid);

	switch (aid) {
	case ERT_AID:
		radio->utf_8_flag = (radio->block[2] & 1);
		radio->formatting_dir = EXTRACT_BIT(radio->block[2],
							ERT_FORMAT_DIR_BIT);
		if (radio->ert_carrier != app_grp_typ) {
			silabs_fm_q_event(radio, SILABS_EVT_NEW_ODA);
			radio->ert_carrier = app_grp_typ;
		}
		break;
	case RT_PLUS_AID:
		/*Extract 5th bit of MSB (b7b6b5b4b3b2b1b0)*/
		radio->rt_ert_flag = EXTRACT_BIT(radio->block[2],
				 RT_ERT_FLAG_BIT);
		if (radio->rt_plus_carrier != app_grp_typ) {
			silabs_fm_q_event(radio, SILABS_EVT_NEW_ODA);
			radio->rt_plus_carrier = app_grp_typ;
		}
		break;
	default:
		FMDBG("Not handling the AID of %x\n", aid);
		break;
	}
}

static int set_hard_mute(struct silabs_fm_device *radio, bool val)
{
	int retval = 0;

	if (val == true) {
		retval = set_property(radio, RX_HARD_MUTE_PROP, HARD_MUTE_MASK);

		if (retval < 0)
			FMDERR("%s: set_hard_mute failed with error %d\n",
				__func__, retval);
	} else {
		retval = set_property(radio, RX_HARD_MUTE_PROP, 0);

		if (retval < 0)
			FMDERR("%s: set_hard_mute failed with error %d\n",
				__func__, retval);
	}

	return retval;
}

static int get_rssi(struct silabs_fm_device *radio, u8 *prssi)
{
	int retval = 0;

	mutex_lock(&radio->lock);

	memset(radio->write_buf, 0, WRITE_REG_NUM);

	/* track command that is being sent to chip.*/
	radio->cmd = FM_RSQ_STATUS_CMD;
	radio->write_buf[0] = FM_RSQ_STATUS_CMD;
	radio->write_buf[1] = 1;

	retval = send_cmd(radio, RSQ_STATUS_CMD_LEN);

	if (retval < 0)
		FMDERR("%s: get_rsq_status failed with error %d\n",
				__func__, retval);

	FMDBG("%s: rssi is %d\n", __func__, radio->read_buf[4]);
	*prssi = radio->read_buf[4];
	mutex_unlock(&radio->lock);

	return retval;
}

static bool is_valid_freq(struct silabs_fm_device *radio, u32 freq)
{
	u32 band_low_limit = radio->recv_conf.band_low_limit * TUNE_STEP_SIZE;
	u32 band_high_limit = radio->recv_conf.band_high_limit * TUNE_STEP_SIZE;
	u8 spacing;

	if (radio->recv_conf.ch_spacing == 0)
		spacing = CH_SPACING_200;
	else if (radio->recv_conf.ch_spacing == 1)
		spacing = CH_SPACING_100;
	else if (radio->recv_conf.ch_spacing == 2)
		spacing = CH_SPACING_50;

	if ((freq >= band_low_limit) &&
		(freq <= band_high_limit) &&
		((freq - band_low_limit) % spacing == 0))
		return true;

	return false;
}

static bool is_new_freq(struct silabs_fm_device *radio, u32 freq)
{
	u8 i = 0;

	for (i = 0; i < radio->af_info2.size; i++) {
		if (freq == radio->af_info2.af_list[i])
			return false;
	}

	return true;
}

static bool is_different_af_list(struct silabs_fm_device *radio)
{
	u8 i = 0, j = 0;
	u32 freq;

	if (radio->af_info1.orig_freq_khz != radio->af_info2.orig_freq_khz)
		return true;

	/* freq is same, check if the AFs are same. */
	for (i = 0; i < radio->af_info1.size; i++) {
		freq = radio->af_info1.af_list[i];
		for (j = 0; j < radio->af_info2.size; j++) {
			if (freq == radio->af_info2.af_list[j])
				break;
		}

		/* freq is not there in list2 i.e list1, list2 are different.*/
		if (j == radio->af_info2.size)
			return true;
	}

	return false;
}

static void reset_af_info(struct silabs_fm_device *radio)
{
	radio->af_info1.inval_freq_cnt = 0;
	radio->af_info1.cnt = 0;
	radio->af_info1.index = 0;
	radio->af_info1.size = 0;
	radio->af_info1.orig_freq_khz = 0;
	memset(radio->af_info1.af_list, 0, sizeof(radio->af_info1.af_list));

	radio->af_info2.inval_freq_cnt = 0;
	radio->af_info2.cnt = 0;
	radio->af_info2.index = 0;
	radio->af_info2.size = 0;
	radio->af_info2.orig_freq_khz = 0;
	memset(radio->af_info2.af_list, 0, sizeof(radio->af_info2.af_list));
}

static void update_af_list(struct silabs_fm_device *radio)
{
	bool retval;
	u8 i = 0;
	u8 af_data = radio->block[2] >> 8;
	u32 af_freq_khz;

	struct kfifo *buff;
	struct af_list_ev ev;
	spinlock_t lock = radio->buf_lock[SILABS_FM_BUF_AF_LIST];

	for (; i < NO_OF_AF_IN_GRP; i++, af_data = radio->block[2] & 0xFF) {

		if (af_data >= MIN_AF_CNT_CODE && af_data <= MAX_AF_CNT_CODE) {

			FMDBG("%s: resetting af info, freq %u, pi %u\n",
				__func__, radio->tuned_freq_khz, radio->pi);
			radio->af_info2.inval_freq_cnt = 0;
			radio->af_info2.cnt = 0;
			radio->af_info2.index = 0;
			radio->af_info2.size = 0;
			radio->af_info2.orig_freq_khz = 0;
			memset(radio->af_info2.af_list,
				0,
				sizeof(radio->af_info2.af_list));

			/* AF count. */
			radio->af_info2.cnt = af_data - NO_AF_CNT_CODE;
			radio->af_info2.orig_freq_khz = radio->tuned_freq_khz;
			radio->af_info2.pi = radio->pi;

			FMDBG("%s: current freq is %u, AF cnt is %u\n",
			__func__, radio->tuned_freq_khz, radio->af_info2.cnt);

		} else if (af_data >= MIN_AF_FREQ_CODE &&
				af_data <= MAX_AF_FREQ_CODE &&
				radio->af_info2.orig_freq_khz != 0 &&
				radio->af_info2.size < MAX_NO_OF_AF) {

			af_freq_khz = SCALE_AF_CODE_TO_FREQ_KHZ(af_data);
			retval = is_valid_freq(radio, af_freq_khz);
			if (retval == false) {
				FMDBG("%s: Invalid AF\n", __func__);
				radio->af_info2.inval_freq_cnt++;
				continue;
			}

			retval = is_new_freq(radio, af_freq_khz);
			if (retval == false) {
				FMDBG("%s: Duplicate AF\n", __func__);
				radio->af_info2.inval_freq_cnt++;
				continue;
			}

			/* update the AF list */
			radio->af_info2.af_list[radio->af_info2.size++] =
								af_freq_khz;
			FMDBG("%s: AF is %u\n", __func__, af_freq_khz);
			if ((radio->af_info2.size +
				radio->af_info2.inval_freq_cnt ==
				radio->af_info2.cnt) &&
				is_different_af_list(radio)) {

				/* Copy the list to af_info1. */
				radio->af_info1.cnt = radio->af_info2.cnt;
				radio->af_info1.size = radio->af_info2.size;
				radio->af_info1.pi = radio->af_info2.pi;
				radio->af_info1.orig_freq_khz =
					radio->af_info2.orig_freq_khz;
				memset(radio->af_info1.af_list,
					0,
					sizeof(radio->af_info1.af_list));

				memcpy(radio->af_info1.af_list,
					radio->af_info2.af_list,
					sizeof(radio->af_info2.af_list));

				/* AF list changed, post it to user space */
				memset(&ev, 0, sizeof(struct af_list_ev));

				ev.tune_freq_khz =
						radio->af_info1.orig_freq_khz;
				ev.pi_code = radio->pi;
				ev.af_size = radio->af_info1.size;

				memcpy(&ev.af_list[0],
					radio->af_info1.af_list,
					GET_AF_LIST_LEN(ev.af_size));

				buff = &radio->data_buf[SILABS_FM_BUF_AF_LIST];
				kfifo_in_locked(buff,
						(u8 *)&ev,
						GET_AF_EVT_LEN(ev.af_size),
						&lock);

				FMDBG("%s: posting AF list evt, curr freq %u\n",
					__func__, ev.tune_freq_khz);

				silabs_fm_q_event(radio,
						SILABS_EVT_NEW_AF_LIST);
			}
		}
	}
}

static void silabs_af_tune(struct work_struct *work)
{
	struct silabs_fm_device *radio;
	int retval = 0;
	u8 rssi = 0;
	u32 freq = 0;

	radio = container_of(work, struct silabs_fm_device, work_af.work);

	if (radio->af_info1.size == 0) {
		FMDBG("%s: Empty AF list\n", __func__);
		radio->is_af_tune_in_progress = false;
		return;
	}

	/* Disable all other interrupts except STC */
	retval = configure_interrupts(radio, ENABLE_STC_INTERRUPTS);

	/* Mute until AF tuning finishes */
	retval = set_hard_mute(radio, true);

	while (1) {
		if (radio->mode != FM_RECV) {
			FMDERR("%s: Drv is not in proper state\n", __func__);
			goto end;
		}

		if (radio->seek_tune_status != NO_SEEK_TUNE_PENDING) {
			FMDBG("%s: manual tune, search issued\n", __func__);
			break;
		}

		if (radio->is_af_jump_enabled != true) {
			FMDBG("%s: AF jump is disabled\n", __func__);
			break;
		}

		/* If no more AFs left, tune to original frequency and break */
		if (radio->af_info1.index >= radio->af_info1.size) {
			FMDBG("%s: No more AFs, tuning to original freq %u\n",
				__func__, radio->af_info1.orig_freq_khz);

			freq = radio->af_info1.orig_freq_khz;

			retval = tune(radio, freq);
			if (retval < 0) {
				FMDERR("%s: tune failed, error %d\n",
					__func__, retval);
				goto err_tune_fail;
			}

			/* wait for tune to finish */
			if (!wait_for_completion_timeout(&radio->sync_req_done,
				msecs_to_jiffies(WAIT_TIMEOUT_MSEC))) {
				FMDERR("%s: didn't receive STC for tune\n",
					__func__);
				/* FM is not correct state */
				continue;
			} else
				FMDBG("%s: received STC for tune\n", __func__);

			goto err_tune_fail;
		}

		freq = radio->af_info1.af_list[radio->af_info1.index++];

		FMDBG("%s: tuning to freq %u\n", __func__, freq);

		retval = tune(radio, freq);
		if (retval < 0) {
			FMDERR("%s: tune failed, error %d\n",
				__func__, retval);
			goto err_tune_fail;
		}

		/* wait for tune to finish */
		if (!wait_for_completion_timeout(&radio->sync_req_done,
			msecs_to_jiffies(WAIT_TIMEOUT_MSEC))) {
			FMDERR("%s: didn't receive STC for tune\n",
				__func__);
			/* FM is not correct state */
			continue;
		} else
			FMDBG("%s: received STC for tune\n", __func__);

		retval = get_rssi(radio, &rssi);
		if (retval < 0) {
			FMDERR("%s: getting rssi failed\n", __func__);
			goto err_tune_fail;
		}

		if (rssi >= radio->af_rssi_th) {
			FMDBG("%s: found AF freq(%u) >= AF threshold\n",
				__func__, freq);
			/* Notify FM UI about the new freq */
			FMDBG("%s: posting TUNE_SUCC event\n", __func__);
			silabs_fm_q_event(radio, SILABS_EVT_TUNE_SUCC);

			break;
		}
	}

err_tune_fail:
	/*
	 * At this point, we are tuned to either original freq or AF with >=
	 * AF rssi threshold
	 */
	reset_af_info(radio);

	radio->is_af_tune_in_progress = false;

	/* Clear the stale RDS int bit. */
	get_rds_status(radio);
	retval = configure_interrupts(radio, ENABLE_STC_RDS_INTERRUPTS);

	/* Clear the stale RSQ int bit. */
	get_rssi(radio, &rssi);
	retval = configure_interrupts(radio, ENABLE_RSQ_INTERRUPTS);

end:
	/* Unmute */
	retval = set_hard_mute(radio, false);
	return;
}

/* When RDS interrupt is received, read and process RDS data. */
static void rds_handler(struct work_struct *worker)
{
	struct silabs_fm_device *radio;
	u8 rt_blks[NO_OF_RDS_BLKS];
	u8 grp_type, addr, ab_flg;

	radio = container_of(worker, struct silabs_fm_device, rds_worker);

	if (!radio) {
		FMDERR("%s:radio is null\n", __func__);
		return;
	}

	FMDBG("Entered rds_handler\n");

	get_rds_status(radio);

	pi_handler(radio, radio->block[0]);

	grp_type = radio->block[1] >> OFFSET_OF_GRP_TYP;

	FMDBG("grp_type = %d\n", grp_type);

	if (grp_type & 0x01)
		pi_handler(radio, radio->block[2]);

	pty_handler(radio, (radio->block[1] >> OFFSET_OF_PTY) & PTY_MASK);

	switch (grp_type) {
	case RDS_TYPE_0A:
		update_af_list(radio);
		/*  fall through */
	case RDS_TYPE_0B:
		addr = (radio->block[1] & PS_MASK) * NO_OF_CHARS_IN_EACH_ADD;
		FMDBG("RDS is PS\n");
		update_ps(radio, addr+0, radio->block[3] >> 8);
		update_ps(radio, addr+1, radio->block[3] & 0xff);
		break;
	case RDS_TYPE_2A:
		FMDBG("RDS is RT 2A group\n");
		rt_blks[0] = (u8)(radio->block[2] >> 8);
		rt_blks[1] = (u8)(radio->block[2] & 0xFF);
		rt_blks[2] = (u8)(radio->block[3] >> 8);
		rt_blks[3] = (u8)(radio->block[3] & 0xFF);
		addr = (radio->block[1] & 0xf) * 4;
		ab_flg = (radio->block[1] & 0x0010) >> 4;
		rt_handler(radio, ab_flg, CNT_FOR_2A_GRP_RT, addr, rt_blks);
		break;
	case RDS_TYPE_2B:
		FMDBG("RDS is RT 2B group\n");
		rt_blks[0] = (u8)(radio->block[3] >> 8);
		rt_blks[1] = (u8)(radio->block[3] & 0xFF);
		rt_blks[2] = 0;
		rt_blks[3] = 0;
		addr = (radio->block[1] & 0xf) * 2;
		ab_flg = (radio->block[1] & 0x0010) >> 4;
		radio->rt_tmp0[MAX_LEN_2B_GRP_RT] = END_OF_RT;
		radio->rt_tmp1[MAX_LEN_2B_GRP_RT] = END_OF_RT;
		radio->rt_cnt[MAX_LEN_2B_GRP_RT] = RT_VALIDATE_LIMIT;
		rt_handler(radio, ab_flg, CNT_FOR_2B_GRP_RT, addr, rt_blks);
		break;
	case RDS_TYPE_3A:
		FMDBG("RDS is 3A group\n");
		silabs_raw_rds_handler(radio);
		break;
	default:
		FMDERR("Not handling the group type %d\n", grp_type);
		break;
	}
	FMDBG("rt_plus_carrier = %x\n", radio->rt_plus_carrier);
	FMDBG("ert_carrier = %x\n", radio->ert_carrier);
	if (grp_type == radio->rt_plus_carrier)
		silabs_rt_plus(radio);
	else if (grp_type == radio->ert_carrier)
		silabs_buff_ert(radio);
	return;
}

/* to enable, disable interrupts. */
static int configure_interrupts(struct silabs_fm_device *radio, u8 val)
{
	int retval = 0;
	u16 prop_val = 0;

	switch (val) {
	case DISABLE_ALL_INTERRUPTS:
		prop_val = 0;
		retval = set_property(radio, GPO_IEN_PROP, prop_val);
		if (retval < 0)
			FMDERR("%s: error disabling interrupts\n", __func__);
		break;
	case ENABLE_STC_RDS_INTERRUPTS:
		/* enable STC and RDS interrupts. */
		prop_val = RDS_INT_BIT_MASK | STC_INT_BIT_MASK;

		retval = set_property(radio, GPO_IEN_PROP, prop_val);
		if (retval < 0)
			FMDERR("%s: error enabling STC, RDS interrupts\n",
				__func__);
		break;
	case ENABLE_STC_INTERRUPTS:
		/* enable STC interrupts only. */
		prop_val = STC_INT_BIT_MASK;

		retval = set_property(radio, GPO_IEN_PROP, prop_val);
		if (retval < 0)
			FMDERR("%s: error enabling STC interrupts\n", __func__);
		break;
	case ENABLE_RDS_INTERRUPTS:
		/* enable RDS interrupts. */
		prop_val = RDS_INT_BIT_MASK | STC_INT_BIT_MASK;
		if (radio->is_af_jump_enabled)
			prop_val |= RSQ_INT_BIT_MASK;

		retval = set_property(radio, GPO_IEN_PROP, prop_val);
		if (retval < 0)
			FMDERR("%s: error enabling RDS interrupts\n",
				__func__);
		break;
	case DISABLE_RDS_INTERRUPTS:
		/* disable RDS interrupts. */
		prop_val = STC_INT_BIT_MASK;
		if (radio->is_af_jump_enabled)
			prop_val |= RSQ_INT_BIT_MASK;

		retval = set_property(radio, GPO_IEN_PROP, prop_val);
		if (retval < 0)
			FMDERR("%s: error disabling RDS interrupts\n",
				__func__);
		break;
	case ENABLE_RSQ_INTERRUPTS:
		/* enable RSQ interrupts. */
		prop_val = RSQ_INT_BIT_MASK | STC_INT_BIT_MASK;
		if (radio->lp_mode != true)
			prop_val |= RDS_INT_BIT_MASK;

		retval = set_property(radio, GPO_IEN_PROP, prop_val);
		if (retval < 0)
			FMDERR("%s: error enabling RSQ interrupts\n",
				__func__);
		break;
	case DISABLE_RSQ_INTERRUPTS:
		/* disable RSQ interrupts. */
		prop_val = STC_INT_BIT_MASK;
		if (radio->lp_mode != true)
			prop_val |= RDS_INT_BIT_MASK;

		retval = set_property(radio, GPO_IEN_PROP, prop_val);
		if (retval < 0)
			FMDERR("%s: error disabling RSQ interrupts\n",
				__func__);
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

static void reset_rds(struct silabs_fm_device *radio)
{
	/* reset PS bufferes */
	memset(radio->ps_display, 0, sizeof(radio->ps_display));
	memset(radio->ps_tmp0, 0, sizeof(radio->ps_tmp0));
	memset(radio->ps_cnt, 0, sizeof(radio->ps_cnt));

	/* reset RT buffers */
	memset(radio->rt_display, 0, sizeof(radio->rt_display));
	memset(radio->rt_tmp0, 0, sizeof(radio->rt_tmp0));
	memset(radio->rt_tmp1, 0, sizeof(radio->rt_tmp1));
	memset(radio->rt_cnt, 0, sizeof(radio->rt_cnt));
}

static int initialize_recv(struct silabs_fm_device *radio)
{
	int retval = 0;

	retval = set_property(radio,
				FM_SEEK_TUNE_SNR_THRESHOLD_PROP,
				DEFAULT_SNR_TH);
	if (retval < 0)	{
		FMDERR("%s: FM_SEEK_TUNE_SNR_THRESHOLD_PROP fail error %d\n",
				__func__, retval);
		goto set_prop_fail;
	}

	radio->sinr_th = DEFAULT_SNR_TH;

	retval = set_property(radio,
				FM_SEEK_TUNE_RSSI_THRESHOLD_PROP,
				DEFAULT_RSSI_TH);
	if (retval < 0)	{
		FMDERR("%s: FM_SEEK_TUNE_RSSI_THRESHOLD_PROP fail error %d\n",
				__func__, retval);
		goto set_prop_fail;
	}

	radio->rssi_th = DEFAULT_RSSI_TH;

	retval = set_property(radio,
				FM_RSQ_RSSI_LO_THRESHOLD_PROP,
				DEFAULT_AF_RSSI_LOW_TH);
	if (retval < 0)	{
		FMDERR("%s: FM_RSQ_RSSI_LO_THRESHOLD_PROP fail error %d\n",
				__func__, retval);
		goto set_prop_fail;
	}

	radio->af_rssi_th = DEFAULT_AF_RSSI_LOW_TH;

	retval = set_property(radio,
				FM_RSQ_INT_SOURCE_PROP,
				RSSI_LOW_TH_INT_BIT_MASK);
	if (retval < 0)	{
		FMDERR("%s: FM_RSQ_INT_SOURCE_PROP fail error %d\n",
				__func__, retval);
		goto set_prop_fail;
	}

	retval = set_property(radio,
				FM_RDS_INT_FIFO_COUNT_PROP,
				FIFO_CNT_16);
	if (retval < 0)	{
		FMDERR("%s: FM_RDS_INT_FIFO_COUNT_PROP fail error %d\n",
				__func__, retval);
		goto set_prop_fail;
	}

	radio->rds_fifo_cnt = FIFO_CNT_16;

set_prop_fail:
	return retval;

}

static void init_ssr(struct silabs_fm_device *radio)
{
	int retval = 0;

	mutex_lock(&radio->lock);

	memset(radio->write_buf, 0, WRITE_REG_NUM);
	/*
	 * Configure status gpio to low in active state. When chip is reset for
	 * some reason, status gpio becomes high since pull-up resistor is
	 * installed. No need to return error even if it fails, since normal
	 * FM functionality can still work fine.
	 */

	radio->cmd = GPIO_CTL_CMD;
	radio->write_buf[0] = GPIO_CTL_CMD;
	radio->write_buf[1] = GPIO1_OUTPUT_ENABLE_MASK;

	retval = send_cmd(radio, GPIO_CTL_CMD_LEN);

	if (retval < 0) {
		FMDERR("%s: setting Silabs gpio1 as op to chip fail, err %d\n",
			__func__, retval);
		goto end;
	}

	memset(radio->write_buf, 0, WRITE_REG_NUM);

	/* track command that is being sent to chip.*/
	radio->cmd = GPIO_SET_CMD;
	radio->write_buf[0] = GPIO_SET_CMD;
	radio->write_buf[1] = GPIO_OUTPUT_LOW_MASK;

	retval = send_cmd(radio, GPIO_SET_CMD_LEN);

	if (retval < 0)
		FMDERR("%s: setting gpios to low failed, error %d\n",
			__func__, retval);

end:
	mutex_unlock(&radio->lock);
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
	reset_rds(radio); /* Clear the existing RDS data */
	init_ssr(radio);
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
					SAMPLE_RATE_48_KHZ);
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
	radio->is_search_cancelled = true;

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

static int clear_stc_int(struct silabs_fm_device *radio)
{
	int retval = 0;

	mutex_lock(&radio->lock);

	memset(radio->write_buf, 0, WRITE_REG_NUM);

	/* track command that is being sent to chip. */
	radio->cmd = FM_TUNE_STATUS_CMD;

	radio->write_buf[0] = FM_TUNE_STATUS_CMD;
	radio->write_buf[1] = INTACK_MASK;

	retval = send_cmd(radio, TUNE_STATUS_CMD_LEN);
	if (retval < 0)
		FMDERR("%s: clear_stc_int fail, error %d\n", __func__, retval);

	mutex_unlock(&radio->lock);

	return retval;
}

static void silabs_interrupts_handler(struct silabs_fm_device *radio)
{
	int retval = 0;
	u8 rssi = 0;

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
		} else if (radio->is_af_tune_in_progress == true) {
			/*
			 * when AF tune is going on and STC int is set, signal
			 * so that AF tune can proceed.
			 */
			FMDBG("In %s, signalling AF tune thread\n", __func__);
			complete(&radio->sync_req_done);
		}
		/* clear the STC interrupt. */
		clear_stc_int(radio);
		reset_rds(radio); /* Clear the existing RDS data */
		return;
	}

	if (radio->read_buf[0] & RSQ_INT_BIT_MASK) {
		FMDBG("RSQ interrupt received, clearing the RSQ int bit\n");

		/* clear RSQ, RDS interrupt bits until AF tune is complete. */
		(void)get_rssi(radio, &rssi);
		(void)get_rds_status(radio);
		/* Don't process RSQ until AF tune is complete. */
		if (radio->is_af_tune_in_progress == true)
			return;

		if (radio->is_af_jump_enabled &&
			radio->af_info1.size != 0 &&
			rssi <= radio->af_rssi_th) {

			radio->is_af_tune_in_progress = true;
			FMDBG("%s: Queuing to AF work Q, freq %u, rssi %u\n",
					__func__, radio->tuned_freq_khz, rssi);
			queue_delayed_work(radio->wqueue_af, &radio->work_af,
					msecs_to_jiffies(SILABS_DELAY_MSEC));
		}
		return;
	}

	if (radio->read_buf[0] & RDS_INT_BIT_MASK) {
		FMDBG("RDS interrupt received\n");
		/* Don't process RDS until AF tune is complete. */
		if (radio->is_af_tune_in_progress == true) {
			/* clear RDS int bit and return */
			get_rds_status(radio);
			return;
		}
		schedule_work(&radio->rds_worker);
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

	irq = radio->status_irq;
	disable_irq_wake(irq);
	free_irq(irq, radio);

	cancel_work_sync(&radio->rds_worker);
	flush_workqueue(radio->wqueue_rds);
	cancel_delayed_work_sync(&radio->work);
	flush_workqueue(radio->wqueue);
	cancel_delayed_work_sync(&radio->work_scan);
	flush_workqueue(radio->wqueue_scan);
	cancel_delayed_work_sync(&radio->work_af);
	flush_workqueue(radio->wqueue_af);
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

static irqreturn_t silabs_fm_status_isr(int irq, void *dev_id)
{
	struct silabs_fm_device *radio = dev_id;

	if (radio->mode == FM_TURNING_OFF || radio->mode == FM_RECV) {
		FMDERR("%s: chip in bad state, posting DISABLED event\n",
			__func__);
		silabs_fm_q_event(radio, SILABS_EVT_RADIO_DISABLED);
		radio->mode = FM_OFF;
	}

	return IRQ_HANDLED;
}

static int silabs_fm_request_irq(struct silabs_fm_device *radio)
{
	int retval;
	int irq;

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
		return retval;
	}

	irq = radio->status_irq;

	retval = request_any_context_irq(irq, silabs_fm_status_isr,
				IRQ_TYPE_EDGE_RISING, "fm status interrupt",
				radio);
	if (retval < 0) {
		FMDERR("Couldn't acquire FM status gpio %d\n", irq);
		/* Do not error out for status int. FM can work without it. */
		return 0;
	} else {
		FMDBG("FM status GPIO %d registered\n", irq);
	}
	retval = enable_irq_wake(irq);
	if (retval < 0) {
		FMDERR("Could not enable FM status interrupt\n ");
		free_irq(irq , radio);
		/* Do not error out for status int. FM can work without it. */
		return 0;
	}

	return retval;
}

static int silabs_fm_fops_open(struct file *file)
{
	struct silabs_fm_device *radio = video_get_drvdata(video_devdata(file));
	int retval = -ENODEV;

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&radio->work, read_int_stat);
	INIT_DELAYED_WORK(&radio->work_scan, silabs_scan);
	INIT_DELAYED_WORK(&radio->work_af, silabs_af_tune);
	INIT_WORK(&radio->rds_worker, rds_handler);

	init_completion(&radio->sync_req_done);
	if (!atomic_dec_and_test(&radio->users)) {
		FMDBG("%s: Device already in use. Try again later", __func__);
		atomic_inc(&radio->users);
		return -EBUSY;
	}

	/* initial gpio pin config & Power up */
	retval = silabs_fm_power_cfg(radio, TURNING_ON);
	if (retval) {
		FMDERR("%s: failed config gpio & pmic\n", __func__);
		goto open_err_setup;
	}
	radio->irq = gpio_to_irq(radio->int_gpio);

	if (radio->irq < 0) {
		FMDERR("%s: gpio_to_irq returned %d\n", __func__, radio->irq);
		goto open_err_req_irq;
	}

	FMDBG("irq number is = %d\n", radio->irq);

	if (radio->status_gpio > 0) {
		radio->status_irq = gpio_to_irq(radio->status_gpio);

		if (radio->status_irq < 0) {
			FMDERR("%s: gpio_to_irq returned %d for status gpio\n",
				__func__, radio->irq);
			goto open_err_req_irq;
		}

		FMDBG("status irq number is = %d\n", radio->status_irq);
	}

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
	silabs_fm_power_cfg(radio, TURNING_OFF);
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

	retval = silabs_fm_power_cfg(radio, TURNING_OFF);
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
		return -EINVAL;
	}

	if (ctrl == NULL) {
		FMDERR("%s, v4l2 ctrl is null\n", __func__);
		return -EINVAL;
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
	case V4L2_CID_PRIVATE_SILABS_GET_SINR:

		mutex_lock(&radio->lock);
		radio->cmd = FM_TUNE_STATUS_CMD;

		radio->write_buf[0] = FM_TUNE_STATUS_CMD;
		radio->write_buf[1] = 0;

		retval = send_cmd(radio, TUNE_STATUS_CMD_LEN);
		if (retval < 0) {
			FMDERR("%s: FM_TUNE_STATUS_CMD failed with error %d\n",
					__func__, retval);
			mutex_unlock(&radio->lock);
			break;
		}

		/* sinr */
		ctrl->value = radio->read_buf[5];
		mutex_unlock(&radio->lock);
		FMDBG("%s: V4L2_CID_PRIVATE_SILABS_GET_SINR, val %d\n",
			__func__, ctrl->value);
		break;
	case V4L2_CID_PRIVATE_SILABS_SINR_THRESHOLD:
		FMDBG("%s: V4L2_CID_PRIVATE_SILABS_SINR_THRESHOLD, val %d\n",
			__func__, radio->sinr_th);

		ctrl->value = radio->sinr_th;
		break;
	case V4L2_CID_PRIVATE_SILABS_RSSI_TH:
		FMDBG("%s: V4L2_CID_PRIVATE_SILABS_RSSI_TH, val %d\n",
			__func__, radio->rssi_th);

		ctrl->value = radio->rssi_th;
		break;
	case V4L2_CID_PRIVATE_SILABS_AF_JUMP_RSSI_TH:
		FMDBG("%s: V4L2_CID_PRIVATE_SILABS_AF_JUMP_RSSI_TH, val %d\n",
			__func__, radio->af_rssi_th);

		ctrl->value = radio->af_rssi_th;
		break;

	case V4L2_CID_PRIVATE_SILABS_RDSD_BUF:
		FMDBG("%s: V4L2_CID_PRIVATE_SILABS_RDSD_BUF, val %d\n",
			__func__, radio->rds_fifo_cnt);

		ctrl->value = radio->rds_fifo_cnt;
		break;
	default:
		retval = -EINVAL;
		break;
	}

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
			retval = configure_interrupts(radio,
						DISABLE_ALL_INTERRUPTS);
			if (retval < 0)
				FMDERR("configure_interrupts failed %d\n",
				retval);
			flush_workqueue(radio->wqueue);
			cancel_work_sync(&radio->rds_worker);
			flush_workqueue(radio->wqueue_rds);
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
		if ((ctrl->value >= MIN_DWELL_TIME) &&
			(ctrl->value <= MAX_DWELL_TIME)) {
			radio->dwell_time_sec = ctrl->value;
		} else {
			FMDERR("%s: scandwell period is not valid\n", __func__);
			retval = -EINVAL;
		}
		break;
	case V4L2_CID_PRIVATE_SILABS_SRCHON:
		silabs_search(radio, (bool)ctrl->value);
		break;
	case V4L2_CID_PRIVATE_SILABS_RDS_STD:
		return retval;
		break;
	case V4L2_CID_PRIVATE_SILABS_RDSON:
		return retval;
		break;
	case V4L2_CID_PRIVATE_SILABS_RDSGROUP_MASK:
		retval = set_property(radio,
				FM_RDS_INT_SOURCE_PROP,
				RDS_INT_BIT);
		if (retval < 0) {
			FMDERR("In %s, FM_RDS_INT_SOURCE_PROP failed %d\n",
				__func__, retval);
			goto end;
		}
		break;
	case V4L2_CID_PRIVATE_SILABS_RDSD_BUF:
		if (is_valid_rds_fifo_cnt(ctrl->value)) {
			retval = set_property(radio,
					FM_RDS_INT_FIFO_COUNT_PROP,
					ctrl->value);
			if (retval < 0)	{
				FMDERR("%s: setting rds fifo cnt failed %d\n",
						__func__, retval);
				goto end;
			}

			radio->rds_fifo_cnt = ctrl->value;
		} else
			retval = -EINVAL;

		break;
	case V4L2_CID_PRIVATE_SILABS_RDSGROUP_PROC:
		/* Enabled all with uncorrectable */
		retval = set_property(radio,
				FM_RDS_CONFIG_PROP,
				UNCORRECTABLE_RDS_EN);
		if (retval < 0) {
			FMDERR("In %s, FM_RDS_CONFIG_PROP failed %d\n",
				__func__, retval);
			goto end;
		}
		break;
	case V4L2_CID_PRIVATE_SILABS_LP_MODE:
		FMDBG("In %s, V4L2_CID_PRIVATE_SILABS_LP_MODE, val is %d\n",
			__func__, ctrl->value);
		if (ctrl->value) {
			/* disable RDS interrupts */
			retval = configure_interrupts(radio,
					ENABLE_RDS_INTERRUPTS);
			radio->lp_mode = true;
		} else {
			/* enable RDS interrupts */
			retval = configure_interrupts(radio,
					DISABLE_RDS_INTERRUPTS);
			radio->lp_mode = false;
		}

		if (retval < 0) {
			FMDERR("In %s, setting low power mode failed %d\n",
				__func__, retval);
			goto end;
		}
		break;
	case V4L2_CID_PRIVATE_SILABS_AF_JUMP:
		FMDBG("%s: V4L2_CID_PRIVATE_SILABS_AF_JUMP, val is %d\n",
			__func__, ctrl->value);
		if (ctrl->value)
			/* enable RSQ interrupts */
			retval = configure_interrupts(radio,
					ENABLE_RSQ_INTERRUPTS);
		else
			/* disable RSQ interrupts */
			retval = configure_interrupts(radio,
					DISABLE_RSQ_INTERRUPTS);
		if (retval < 0) {
			FMDERR("%s: setting AF jump mode failed %d\n",
				__func__, retval);
			goto end;
		}
		/* Save the AF jump state */
		radio->is_af_jump_enabled = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_SILABS_SINR_THRESHOLD:
		FMDBG("%s: V4L2_CID_PRIVATE_SILABS_SINR_THRESHOLD, val is %d\n",
			__func__, ctrl->value);
		if (is_valid_sinr(ctrl->value)) {
			retval = set_property(radio,
						FM_SEEK_TUNE_SNR_THRESHOLD_PROP,
						ctrl->value);
			if (retval < 0)	{
				FMDERR("%s: setting sinr th failed, error %d\n",
						__func__, retval);
				goto end;
			}

			radio->sinr_th = ctrl->value;

		} else {
			retval = -EINVAL;
			FMDERR("%s: Invalid sinr\n", __func__);
		}
		break;
	case V4L2_CID_PRIVATE_SILABS_RSSI_TH:
		FMDBG("%s: V4L2_CID_PRIVATE_SILABS_RSSI_TH, val is %d\n",
			__func__, ctrl->value);
		if (is_valid_rssi(ctrl->value)) {
			retval = set_property(radio,
					FM_SEEK_TUNE_RSSI_THRESHOLD_PROP,
					ctrl->value);
			if (retval < 0)	{
				FMDERR("%s: setting rssi th failed, error %d\n",
						__func__, retval);
				goto end;
			}

			radio->rssi_th = ctrl->value;

		} else {
			retval = -EINVAL;
			FMDERR("%s: Invalid sinr\n", __func__);
		}
		break;
	case V4L2_CID_PRIVATE_SILABS_AF_JUMP_RSSI_TH:
		FMDBG("%s: V4L2_CID_PRIVATE_SILABS_AF_JUMP_RSSI_TH, val %d\n",
			__func__, ctrl->value);
		if (is_valid_rssi(ctrl->value)) {
			retval = set_property(radio,
						FM_RSQ_RSSI_LO_THRESHOLD_PROP,
						ctrl->value);
			if (retval < 0)	{
				FMDERR("%s: setting af rssi th failed err %d\n",
					__func__, retval);
				goto end;
			}

			radio->af_rssi_th = ctrl->value;

		} else {
			retval = -EINVAL;
			FMDERR("%s: Invalid sinr\n", __func__);
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
	radio->tuned_freq_khz = f * TUNE_STEP_SIZE;

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
	if (retval > 0) {
		radio->tuned_freq_khz = f;
		/* Clear AF list */
		reset_af_info(radio);
		cancel_delayed_work_sync(&radio->work_af);
		flush_workqueue(radio->wqueue_af);
	}

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

	radio->is_search_cancelled = false;

	if (radio->g_search_mode == 0) {
		/* seek */
		FMDBG("starting seek\n");

		radio->seek_tune_status = SEEK_PENDING;

		retval = silabs_seek(radio, dir, WRAP_ENABLE);

	} else if (radio->g_search_mode == 1) {
		/* scan */
		FMDBG("starting scan\n");

		silabs_search(radio, START_SCAN);

	} else {
		retval = -EINVAL;
		FMDERR("In %s, invalid search mode %d\n",
			__func__, radio->g_search_mode);
	}

	if (retval > 0) {
		/* Clear AF list */
		reset_af_info(radio);
		cancel_delayed_work_sync(&radio->work_af);
		flush_workqueue(radio->wqueue_af);
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
	int len = 0, retval = -1;

	if ((radio == NULL) || (buffer == NULL)) {
		FMDERR("radio/buffer is NULL\n");
		return -ENXIO;
	}
	buf_type = buffer->index;
	buf = (u8 *)buffer->m.userptr;
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

static int silabs_fm_pinctrl_init(struct silabs_fm_device *radio)
{
	int retval = 0;

	radio->fm_pinctrl = devm_pinctrl_get(&radio->client->dev);
	if (IS_ERR_OR_NULL(radio->fm_pinctrl)) {
		FMDERR("%s: target does not use pinctrl\n", __func__);
		retval = PTR_ERR(radio->fm_pinctrl);
		return retval;
	}

	radio->gpio_state_active =
			pinctrl_lookup_state(radio->fm_pinctrl,
						"pmx_fm_active");
	if (IS_ERR_OR_NULL(radio->gpio_state_active)) {
		FMDERR("%s: cannot get FM active state\n", __func__);
		retval = PTR_ERR(radio->gpio_state_active);
		goto err_active_state;
	}

	radio->gpio_state_suspend =
				pinctrl_lookup_state(radio->fm_pinctrl,
							"pmx_fm_suspend");
	if (IS_ERR_OR_NULL(radio->gpio_state_suspend)) {
		FMDERR("%s: cannot get FM suspend state\n", __func__);
		retval = PTR_ERR(radio->gpio_state_suspend);
		goto err_suspend_state;
	}

	return retval;

err_suspend_state:
	radio->gpio_state_suspend = 0;

err_active_state:
	radio->gpio_state_active = 0;

	return retval;
}

static int silabs_parse_dt(struct device *dev,
			struct silabs_fm_device *radio)
{
	int rc = 0;
	struct device_node *np = dev->of_node;

	radio->reset_gpio = of_get_named_gpio(np, "silabs,reset-gpio", 0);
	if (radio->reset_gpio < 0) {
		FMDERR("silabs-reset-gpio not provided in device tree");
		return radio->reset_gpio;
	}

	rc = gpio_request(radio->reset_gpio, "fm_rst_gpio_n");
	if (rc) {
		FMDERR("unable to request gpio %d (%d)\n",
					radio->reset_gpio, rc);
		return rc;
	}

	radio->int_gpio = of_get_named_gpio(np, "silabs,int-gpio", 0);
	if (radio->int_gpio < 0) {
		FMDERR("silabs-int-gpio not provided in device tree");
		rc = radio->int_gpio;
		goto err_int_gpio;
	}

	rc = gpio_request(radio->int_gpio, "silabs_fm_int_n");
	if (rc) {
		FMDERR("unable to request gpio %d (%d)\n",
						radio->int_gpio, rc);
		goto err_int_gpio;
	}

	radio->status_gpio = of_get_named_gpio(np, "silabs,status-gpio", 0);
	if (radio->status_gpio < 0) {
		FMDERR("silabs-status-gpio not provided in device tree");
	} else {
		rc = gpio_request(radio->status_gpio, "silabs_fm_stat_n");
		if (rc) {
			FMDERR("unable to request status gpio %d (%d)\n",
							radio->status_gpio, rc);
			goto err_status_gpio;
		}
	}
	return rc;

err_status_gpio:
	gpio_free(radio->int_gpio);
err_int_gpio:
	gpio_free(radio->reset_gpio);

	return rc;
}

static int silabs_dt_parse_vreg_info(struct device *dev,
			struct fm_power_vreg_data *vreg, const char *vreg_name)
{
	int ret = 0;
	u32 vol_suply[2];
	struct device_node *np = dev->of_node;

	ret = of_property_read_u32_array(np, vreg_name, vol_suply, 2);
	if (ret < 0) {
		FMDERR("Invalid property name\n");
		ret =  -EINVAL;
	} else {
		vreg->low_vol_level = vol_suply[0];
		vreg->high_vol_level = vol_suply[1];
	}
	return ret;
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

static int silabs_fm_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{

	struct silabs_fm_device *radio;
	struct regulator *vreg = NULL;
	int retval = 0;
	int i = 0;
	int kfifo_alloc_rc = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		FMDERR("%s: no support for i2c read/write byte data\n",
								__func__);
		return -EIO;
	}

	vreg = regulator_get(&client->dev, "va");

	if (IS_ERR(vreg)) {
		/*
		 * if analog voltage regulator, VA is not ready yet, return
		 * -EPROBE_DEFER to kernel so that probe will be called at
		 * later point of time.
		 */
		if (PTR_ERR(vreg) == -EPROBE_DEFER) {
			FMDERR("In %s, areg probe defer\n", __func__);
			return PTR_ERR(vreg);
		}
	}
	/* private data allocation */
	radio = kzalloc(sizeof(struct silabs_fm_device), GFP_KERNEL);
	if (!radio) {
		FMDERR("Memory not allocated for radio\n");
		retval = -ENOMEM;
		goto err_initial;
	}

	retval = silabs_parse_dt(&client->dev, radio);
	if (retval) {
		FMDERR("%s: Parsing DT failed(%d)", __func__, retval);
		regulator_put(vreg);
		kfree(radio);
		return retval;
	}

	radio->client = client;

	i2c_set_clientdata(client, radio);
	if (!IS_ERR(vreg)) {
		radio->areg = devm_kzalloc(&client->dev,
					sizeof(struct fm_power_vreg_data),
					GFP_KERNEL);
		if (!radio->areg) {
			FMDERR("%s: allocating memory for areg failed\n",
								__func__);
			regulator_put(vreg);
			kfree(radio);
			return -ENOMEM;
		}

		radio->areg->reg = vreg;
		radio->areg->name = "va";
		radio->areg->is_enabled = 0;
		retval = silabs_dt_parse_vreg_info(&client->dev,
				radio->areg, "silabs,va-supply-voltage");
		if (retval < 0) {
			FMDERR("%s: parsing va-supply failed\n", __func__);
			goto mem_alloc_fail;
		}
	}

	vreg = regulator_get(&client->dev, "vdd");

	if (IS_ERR(vreg)) {
		FMDERR("In %s, vdd supply is not provided\n", __func__);
	} else {
		radio->dreg = devm_kzalloc(&client->dev,
					sizeof(struct fm_power_vreg_data),
					GFP_KERNEL);
		if (!radio->dreg) {
			FMDERR("%s: allocating memory for dreg failed\n",
								__func__);
			retval = -ENOMEM;
			regulator_put(vreg);
			goto mem_alloc_fail;
		}

		radio->dreg->reg = vreg;
		radio->dreg->name = "vdd";
		radio->dreg->is_enabled = 0;
		retval = silabs_dt_parse_vreg_info(&client->dev,
				radio->dreg, "silabs,vdd-supply-voltage");
		if (retval < 0) {
			FMDERR("%s: parsing vdd-supply failed\n", __func__);
			goto err_dreg;
		}
	}

	/* Initialize pin control*/
	retval = silabs_fm_pinctrl_init(radio);
	if (retval) {
		FMDERR("%s: silabs_fm_pinctrl_init returned %d\n",
							__func__, retval);
		/* if pinctrl is not supported, -EINVAL is returned*/
		if (retval == -EINVAL)
			retval = 0;
	} else {
		FMDBG("silabs_fm_pinctrl_init success\n");
	}

	radio->wqueue = NULL;
	radio->wqueue_scan = NULL;
	radio->wqueue_af = NULL;
	radio->wqueue_rds = NULL;

	/* video device allocation */
	radio->videodev = video_device_alloc();
	if (!radio->videodev) {
		FMDERR("radio->videodev is NULL\n");
		goto err_dreg;
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

	/* radio initializes to normal mode */
	radio->lp_mode = 0;
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

	FMDBG("%s: creating work q for af\n", __func__);
	radio->wqueue_af  = create_singlethread_workqueue("sifmradioaf");

	if (!radio->wqueue_af) {
		retval = -ENOMEM;
		goto err_wqueue_af;
	}

	radio->wqueue_rds  = create_singlethread_workqueue("sifmradiords");

	if (!radio->wqueue_rds) {
		retval = -ENOMEM;
		goto err_wqueue_rds;
	}

	/* register video device */
	retval = video_register_device(radio->videodev,
			VFL_TYPE_RADIO,
			RADIO_NR);
	if (retval != 0) {
		FMDERR("Could not register video device\n");
		goto err_all;
	}
	return 0;

err_all:
	destroy_workqueue(radio->wqueue_rds);
err_wqueue_rds:
	destroy_workqueue(radio->wqueue_af);
err_wqueue_af:
	destroy_workqueue(radio->wqueue_scan);
err_wqueue_scan:
	destroy_workqueue(radio->wqueue);
err_fifo_alloc:
	for (i--; i >= 0; i--)
		kfifo_free(&radio->data_buf[i]);
	video_device_release(radio->videodev);
err_dreg:
	if (radio->dreg && radio->dreg->reg) {
		regulator_put(radio->dreg->reg);
		devm_kfree(&client->dev, radio->dreg);
	}
mem_alloc_fail:
	if (radio->areg && radio->areg->reg) {
		regulator_put(radio->areg->reg);
		devm_kfree(&client->dev, radio->areg);
	}
	kfree(radio);
err_initial:
	return retval;
}

static int silabs_fm_remove(struct i2c_client *client)
{
	int i;
	struct silabs_fm_device *radio = i2c_get_clientdata(client);

	if (unlikely(radio == NULL)) {
		FMDERR("%s:radio is null", __func__);
		return -EINVAL;
	}

	if (radio->dreg && radio->dreg->reg) {
		regulator_put(radio->dreg->reg);
		devm_kfree(&client->dev, radio->dreg);
	}

	if (radio->areg && radio->areg->reg) {
		regulator_put(radio->areg->reg);
		devm_kfree(&client->dev, radio->areg);
	}
	/* disable irq */
	destroy_workqueue(radio->wqueue);
	destroy_workqueue(radio->wqueue_scan);
	destroy_workqueue(radio->wqueue_af);
	destroy_workqueue(radio->wqueue_rds);

	video_unregister_device(radio->videodev);

	/* free internal buffers */
	for (i = 0; i < SILABS_FM_BUF_MAX; i++)
		kfifo_free(&radio->data_buf[i]);

	/* free state struct */
	kfree(radio);

	return 0;
}

static const struct i2c_device_id silabs_i2c_id[] = {
	{ DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, silabs_i2c_id);

static const struct of_device_id silabs_fm_match[] = {
	{.compatible = "silabs,si4705"},
	{}
};

static struct i2c_driver silabs_fm_driver = {
	.probe  = silabs_fm_probe,
	.driver = {
		.owner  = THIS_MODULE,
		.name   = "silabs-fm",
		.of_match_table = silabs_fm_match,
	},
	.remove  = silabs_fm_remove,
	.id_table       = silabs_i2c_id,
};


static int __init radio_module_init(void)
{
	return i2c_add_driver(&silabs_fm_driver);
}
module_init(radio_module_init);

static void __exit radio_module_exit(void)
{
	i2c_del_driver(&silabs_fm_driver);
}
module_exit(radio_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DRIVER_DESC);

