/* drivers/media/radio/rtc6226/radio-rtc6226-i2c.c
 *
 *  Driver for Richwave RTC6226 FM Tuner
 *
 *  Copyright (c) 2009 Samsung Electronics Co.Ltd
 *  Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *  Copyright (c) 2009 Tobias Lorenz <tobias.lorenz@gmx.net>
 *  Copyright (c) 2012 Hans de Goede <hdegoede@redhat.com>
 *  Copyright (c) 2018 LG Electronics, Inc.
 *  Copyright (c) 2018 Richwave Technology Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* kernel includes */
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/of_gpio.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include "radio-rtc6226.h"
#include <linux/workqueue.h>

static const struct of_device_id rtc6226_i2c_dt_ids[] = {
	{.compatible = "rtc6226"},
	{}
};

/* I2C Device ID List */
static const struct i2c_device_id rtc6226_i2c_id[] = {
    /* Generic Entry */
	{ "rtc6226", 0 },
	/* Terminating entry */
	{ }
};
MODULE_DEVICE_TABLE(i2c, rtc6226_i2c_id);


/**************************************************************************
 * Module Parameters
 **************************************************************************/

/* Radio Nr */
static int radio_nr = -1;
MODULE_PARM_DESC(radio_nr, "Radio Nr");

/* RDS buffer blocks */
static unsigned int rds_buf = 100;
MODULE_PARM_DESC(rds_buf, "RDS buffer entries: *100*");

enum rtc6226_ctrl_id {
	RTC6226_ID_CSR0_ENABLE,
	RTC6226_ID_CSR0_DISABLE,
	RTC6226_ID_DEVICEID,
	RTC6226_ID_CSR0_DIS_SMUTE,
	RTC6226_ID_CSR0_DIS_MUTE,
	RTC6226_ID_CSR0_DEEM,
	RTC6226_ID_CSR0_BLNDADJUST,
	RTC6226_ID_CSR0_VOLUME,
	RTC6226_ID_CSR0_BAND,
	RTC6226_ID_CSR0_CHSPACE,
	RTC6226_ID_CSR0_DIS_AGC,
	RTC6226_ID_CSR0_RDS_EN,
	RTC6226_ID_SEEK_CANCEL,
	RTC6226_ID_CSR0_SEEKRSSITH,
	RTC6226_ID_CSR0_OFSTH,
	RTC6226_ID_CSR0_QLTTH,
	RTC6226_ID_RSSI,
	RTC6226_ID_RDS_RDY,
	RTC6226_ID_STD,
	RTC6226_ID_SF,
	RTC6226_ID_RDS_SYNC,
	RTC6226_ID_SI,
};


/**************************************************************************
 * I2C Definitions
 **************************************************************************/
/* Write starts with the upper byte of register 0x02 */
#define WRITE_REG_NUM       3
#define WRITE_INDEX(i)      ((i + 0x02)%16)

/* Read starts with the upper byte of register 0x0a */
#define READ_REG_NUM        2
#define READ_INDEX(i)       ((i + RADIO_REGISTER_NUM - 0x0a) % READ_REG_NUM)

/*static*/
struct tasklet_struct my_tasklet;
/*
 * rtc6226_get_register - read register
 */
int rtc6226_get_register(struct rtc6226_device *radio, int regnr)
{
	u8 reg[1];
	u8 buf[READ_REG_NUM];
	struct i2c_msg msgs[2] = {
		{ radio->client->addr, 0, 1, reg },
		{ radio->client->addr, I2C_M_RD, sizeof(buf), buf },
	};

	reg[0] = (u8)(regnr);
	if (i2c_transfer(radio->client->adapter, msgs, 2) != 2)
		return -EIO;

	radio->registers[regnr] =
		(u16)(((buf[0] << 8) & 0xff00) | buf[1]);

	return 0;
}

/*
 * rtc6226_set_register - write register
 */
int rtc6226_set_register(struct rtc6226_device *radio, int regnr)
{
	u8 buf[WRITE_REG_NUM];
	struct i2c_msg msgs[1] = {
		{ radio->client->addr, 0, sizeof(u8) * WRITE_REG_NUM,
			(void *)buf },
	};

	buf[0] = (u8)(regnr);
	buf[1] = (u8)((radio->registers[(u8)(regnr) & 0xFF] >> 8) & 0xFF);
	buf[2] = (u8)(radio->registers[(u8)(regnr) & 0xFF] & 0xFF);

	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
		return -EIO;

	return 0;
}

/*
 * rtc6226_set_register - write register
 */
int rtc6226_set_serial_registers(struct rtc6226_device *radio,
	u16 *data, int regnr)
{
	u8 buf[WRITE_REG_NUM];
	struct i2c_msg msgs[1] = {
		{ radio->client->addr, 0, sizeof(u8) * WRITE_REG_NUM,
			(void *)buf },
	};

	buf[0] = (u8)(regnr);
	buf[1] = (u8)((data[0] >> 8) & 0xFF);
	buf[2] = (u8)(data[0] & 0xFF);

	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
		return -EIO;

	return 0;
}

/**************************************************************************
 * General Driver Functions - ENTIRE REGISTERS
 **************************************************************************/
/*
 * rtc6226_get_all_registers - read entire registers
 */
/* changed from static */
int rtc6226_get_all_registers(struct rtc6226_device *radio)
{
	int i;
	int err;
	u8 reg[1] = {0x00};
	u8 buf[RADIO_REGISTER_NUM];
	struct i2c_msg msgs1[1] = {
		{ radio->client->addr, 0, 1, reg},
	};
	struct i2c_msg msgs[1] = {
		{ radio->client->addr, I2C_M_RD, sizeof(buf), buf },
	};

	if (i2c_transfer(radio->client->adapter, msgs1, 1) != 1)
		return -EIO;

	err = i2c_transfer(radio->client->adapter, msgs, 1);

	if (err < 0)
		return -EIO;

	for (i = 0; i < 16; i++)
		radio->registers[i] =
			(u16)(((buf[i*2] << 8) & 0xff00) | buf[i*2+1]);

	return 0;
}

/*
 * rtc6226_vidioc_querycap - query device capabilities
 */
int rtc6226_vidioc_querycap(struct file *file, void *priv,
	struct v4l2_capability *capability)
{
	FMDBG("%s enter\n", __func__);
	strlcpy(capability->driver, DRIVER_NAME, sizeof(capability->driver));
	strlcpy(capability->card, DRIVER_CARD, sizeof(capability->card));
	capability->device_caps = V4L2_CAP_HW_FREQ_SEEK | V4L2_CAP_READWRITE |
		V4L2_CAP_TUNER | V4L2_CAP_RADIO | V4L2_CAP_RDS_CAPTURE;
	capability->capabilities = capability->device_caps |
		V4L2_CAP_DEVICE_CAPS;

	return 0;
}

/*
 * rtc6226_i2c_interrupt - interrupt handler
 */
static void rtc6226_i2c_interrupt_handler(struct rtc6226_device *radio)
{
	unsigned char regnr;
	int retval = 0;
	unsigned short current_chan;

	FMDBG("%s enter\n", __func__);

	/* check Seek/Tune Complete */
	retval = rtc6226_get_register(radio, STATUS);
	if (retval < 0) {
		FMDERR("%s read fail to STATUS\n", __func__);
		goto end;
	}
	FMDBG("%s : STATUS=0x%4.4hx\n", __func__, radio->registers[STATUS]);

	retval = rtc6226_get_register(radio, RSSI);
	if (retval < 0) {
		FMDERR("%s read fail to RSSI\n", __func__);
		goto end;
	}
	FMDBG("%s : RSSI=0x%4.4hx\n", __func__, radio->registers[RSSI]);

	if (radio->registers[STATUS] & STATUS_STD) {
			/* stop seeking : clear STD*/
		radio->registers[SEEKCFG1] &= ~SEEKCFG1_CSR0_SEEK;
		retval = rtc6226_set_register(radio, SEEKCFG1);
		/*clear the status bit to allow another tune or seek*/
		current_chan = radio->registers[CHANNEL] & CHANNEL_CSR0_CH;
		radio->registers[CHANNEL] &= ~CHANNEL_CSR0_TUNE;
		retval = rtc6226_set_register(radio, CHANNEL);
		if (retval < 0)
			radio->registers[CHANNEL] = current_chan;
		rtc6226_reset_rds_data(radio);
		FMDBG("%s clear Seek/Tune bit\n", __func__);
		if (radio->seek_tune_status == SEEK_PENDING) {
			FMDBG("posting RTC6226_EVT_SEEK_COMPLETE event\n");
			rtc6226_q_event(radio, RTC6226_EVT_SEEK_COMPLETE);
			/* post tune comp evt since seek results in a tune.*/
			FMDBG("posting RICHWAVE_EVT_TUNE_SUCC event\n");
			rtc6226_q_event(radio, RTC6226_EVT_TUNE_SUCC);
			radio->seek_tune_status = NO_SEEK_TUNE_PENDING;
		} else if (radio->seek_tune_status == TUNE_PENDING) {
			FMDBG("posting RICHWAVE_EVT_TUNE_SUCC event\n");
			rtc6226_q_event(radio, RTC6226_EVT_TUNE_SUCC);
			radio->seek_tune_status = NO_SEEK_TUNE_PENDING;
		} else if (radio->seek_tune_status == SCAN_PENDING) {
			/* when scan is pending and STC int is set, signal
			 * so that scan can proceed
			 */
			FMDBG("In %s, signalling scan thread\n", __func__);
			complete(&radio->completion);
		}
		FMDBG("%s Seek/Tune done\n", __func__);
	} else {
		/* Check RDS data after tune/seek interrupt finished
		 * Update RDS registers
		 */
		for (regnr = 1; regnr < RDS_REGISTER_NUM; regnr++) {
			retval = rtc6226_get_register(radio, STATUS + regnr);
			if (retval < 0)
				goto end;
		}
		/* get rds blocks */
		if ((radio->registers[STATUS] & STATUS_RDS_RDY) == 0) {
			/* No RDS group ready, better luck next time */
			FMDERR("%s No RDS group ready\n", __func__);
			goto end;
		} else {
			/* avoid RDS interrupt lock disable_irq*/
			if ((radio->registers[SYSCFG] &
						SYSCFG_CSR0_RDS_EN) != 0) {
				FMDBG("%s start rds handler\n", __func__);
				schedule_work(&radio->rds_worker);
			}
		}
	}
end:
	FMDBG("%s exit :%d\n", __func__, retval);
}

static irqreturn_t rtc6226_isr(int irq, void *dev_id)
{
	struct rtc6226_device *radio = dev_id;
	/*
	 * The call to queue_delayed_work ensures that a minimum delay
	 * (in jiffies) passes before the work is actually executed. The return
	 * value from the function is nonzero if the work_struct was actually
	 * added to queue (otherwise, it may have already been there and will
	 * not be added a second time).
	 */

	queue_delayed_work(radio->wqueue, &radio->work,
				msecs_to_jiffies(10));

	return IRQ_HANDLED;
}

static void rtc6226_handler(struct work_struct *work)
{
	struct rtc6226_device *radio;

	radio = container_of(work, struct rtc6226_device, work.work);

	rtc6226_i2c_interrupt_handler(radio);
}

void rtc6226_disable_irq(struct rtc6226_device *radio)
{
	int irq;

	irq = radio->irq;
	disable_irq_wake(irq);
	free_irq(irq, radio);

	cancel_delayed_work_sync(&radio->work);
	flush_workqueue(radio->wqueue);

	cancel_work_sync(&radio->rds_worker);
	flush_workqueue(radio->wqueue_rds);
	cancel_delayed_work_sync(&radio->work_scan);
	flush_workqueue(radio->wqueue_scan);
}

int rtc6226_enable_irq(struct rtc6226_device *radio)
{
	int retval;
	int irq;

	retval = gpio_direction_input(radio->int_gpio);
	if (retval) {
		FMDERR("%s unable to set the gpio %d direction(%d)\n",
				__func__, radio->int_gpio, retval);
		return retval;
	}
	radio->irq = gpio_to_irq(radio->int_gpio);
	irq = radio->irq;

	if (radio->irq < 0) {
		FMDERR("%s: gpio_to_irq returned %d\n", __func__, radio->irq);
		goto open_err_req_irq;
	}

	FMDBG("%s irq number is = %d\n", __func__, radio->irq);

	retval = request_any_context_irq(radio->irq, rtc6226_isr,
			IRQF_TRIGGER_FALLING, DRIVER_NAME, radio);

	if (retval < 0) {
		FMDERR("%s Couldn't acquire FM gpio %d, retval:%d\n",
			 __func__, radio->irq, retval);
		goto open_err_req_irq;
	} else {
		FMDBG("%s FM GPIO %d registered\n", __func__, radio->irq);
	}
	retval = enable_irq_wake(irq);
	if (retval < 0) {
		FMDERR("Could not wake FM interrupt\n");
		free_irq(irq, radio);
	}
	return retval;

open_err_req_irq:
	rtc6226_disable_irq(radio);

	return retval;
}

static int rtc6226_fm_vio_reg_cfg(struct rtc6226_device *radio, bool on)
{
	int rc = 0;
	struct fm_power_vreg_data *vreg;

	vreg = radio->vioreg;
	if (!vreg) {
		FMDERR("In %s, vio reg is NULL\n", __func__);
		return rc;
	}
	if (on) {
		FMDBG("vreg is : %s\n", vreg->name);
		rc = regulator_set_voltage(vreg->reg,
					vreg->low_vol_level,
					vreg->high_vol_level);
		if (rc < 0) {
			FMDERR("set_vol(%s) fail %d\n", vreg->name, rc);
			return rc;
		}
		rc = regulator_enable(vreg->reg);
		if (rc < 0) {
			FMDERR("reg enable(%s) failed.rc=%d\n", vreg->name, rc);
				regulator_set_voltage(vreg->reg,
						0,
						vreg->high_vol_level);
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

		/* Set the min voltage to 0 */
		rc = regulator_set_voltage(vreg->reg,
					0,
					vreg->high_vol_level);
		if (rc < 0) {
			FMDERR("set_vol(%s) fail %d\n", vreg->name, rc);
			return rc;
		}
	}
	return rc;
}

static int rtc6226_fm_vdd_reg_cfg(struct rtc6226_device *radio, bool on)
{
	int rc = 0;
	struct fm_power_vreg_data *vreg;

	vreg = radio->vddreg;
	if (!vreg) {
		FMDERR("In %s, vdd reg is NULL\n", __func__);
		return rc;
	}

	if (on) {
		FMDBG("vreg is : %s\n", vreg->name);
		rc = regulator_set_voltage(vreg->reg,
					vreg->low_vol_level,
					vreg->high_vol_level);
		if (rc < 0) {
			FMDERR("set_vol(%s) fail %d\n", vreg->name, rc);
			return rc;
		}

		rc = regulator_enable(vreg->reg);
		if (rc < 0) {
			FMDERR("reg enable(%s) failed.rc=%d\n", vreg->name, rc);
			regulator_set_voltage(vreg->reg,
					0,
					vreg->high_vol_level);
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

			/* Set the min voltage to 0 */
		rc = regulator_set_voltage(vreg->reg,
					0,
					vreg->high_vol_level);
		if (rc < 0) {
			FMDERR("set_vol(%s) fail %d\n", vreg->name, rc);
			return rc;
		}
	}
	return rc;
}

static int rtc6226_fm_power_cfg(struct rtc6226_device *radio, bool powerflag)
{
	int rc = 0;

	if (powerflag) {
		/* Turn ON sequence */
		rc = rtc6226_fm_vdd_reg_cfg(radio, powerflag);
		if (rc < 0) {
			FMDERR("In %s, vdd reg cfg failed %x\n", __func__, rc);
			return rc;
		}
		rc = rtc6226_fm_vio_reg_cfg(radio, powerflag);
		if (rc < 0) {
			FMDERR("In %s, vio reg cfg failed %x\n", __func__, rc);
			rtc6226_fm_vdd_reg_cfg(radio, false);
			return rc;
		}
	} else {
		/* Turn OFF sequence */
		rc = rtc6226_fm_vdd_reg_cfg(radio, powerflag);
		if (rc < 0)
			FMDERR("In %s, vdd reg cfg failed %x\n", __func__, rc);
		rc = rtc6226_fm_vio_reg_cfg(radio, powerflag);
		if (rc < 0)
			FMDERR("In %s, vio reg cfg failed %x\n", __func__, rc);
	}
	return rc;
}
/*
 * rtc6226_fops_open - file open
 */
int rtc6226_fops_open(struct file *file)
{
	struct rtc6226_device *radio = video_drvdata(file);
	int retval = v4l2_fh_open(file);

	FMDBG("%s enter user num = %d\n", __func__, radio->users);
	if (retval) {
		FMDERR("%s fail to open v4l2\n", __func__);
		return retval;
	}

	if (radio->users == 0)
		radio->users++;
	else {
		FMDERR("Device already in use. Try again later\n");
		return -EBUSY;
	}

	INIT_DELAYED_WORK(&radio->work, rtc6226_handler);
	INIT_DELAYED_WORK(&radio->work_scan, rtc6226_scan);
	INIT_WORK(&radio->rds_worker, rtc6226_rds_handler);

	/* Power up  Supply voltage to VDD and VIO */
	retval = rtc6226_fm_power_cfg(radio, TURNING_ON);
	if (retval) {
		FMDERR("%s: failed to supply voltage\n", __func__);
		goto open_err_setup;
	}

	retval = rtc6226_enable_irq(radio);
	/* Wait for the value to take effect on gpio. */
	msleep(100);
	if (retval) {
		FMDERR("%s:enable irq failed\n", __func__);
		goto open_err_req_irq;
	}

	if (retval)
		v4l2_fh_release(file);
	return retval;

open_err_req_irq:
	rtc6226_fm_power_cfg(radio, TURNING_OFF);
open_err_setup:
	radio->users--;
	return retval;
}

/*
 * rtc6226_fops_release - file release
 */
int rtc6226_fops_release(struct file *file)
{
	struct rtc6226_device *radio = video_drvdata(file);
	int retval = 0;

	FMDBG("%s : Exit\n", __func__);
	if (v4l2_fh_is_singular_file(file)) {
		if (radio->mode != FM_OFF) {
			rtc6226_power_down(radio);
			radio->mode = FM_OFF;
		}
	}
	rtc6226_disable_irq(radio);
	radio->users--;
	retval = rtc6226_fm_power_cfg(radio, TURNING_OFF);
	if (retval < 0)
		FMDERR("%s: failed to apply voltage\n", __func__);
	return v4l2_fh_release(file);
}

static int rtc6226_parse_dt(struct device *dev,
			struct rtc6226_device *radio)
{
	int rc = 0;
	struct device_node *np = dev->of_node;

	radio->int_gpio = of_get_named_gpio(np, "fmint-gpio", 0);
	if (radio->int_gpio < 0) {
		FMDERR("%s int-gpio not provided in device tree\n", __func__);
		rc = radio->int_gpio;
		goto err_int_gpio;
	}

	rc = gpio_request(radio->int_gpio, "fm_int");
	if (rc) {
		FMDERR("%s unable to request gpio %d (%d)\n", __func__,
						radio->int_gpio, rc);
		goto err_int_gpio;
	}

	rc = gpio_direction_output(radio->int_gpio, 0);
	if (rc) {
		FMDERR("%s unable to set the gpio %d direction(%d)\n",
		__func__, radio->int_gpio, rc);
		goto err_int_gpio;
	}
		/* Wait for the value to take effect on gpio. */
	msleep(100);

	return rc;

err_int_gpio:
	gpio_free(radio->int_gpio);

	return rc;
}

static int rtc6226_pinctrl_init(struct rtc6226_device *radio)
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

static int rtc6226_dt_parse_vreg_info(struct device *dev,
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

/*
 * rtc6226_i2c_probe - probe for the device
 */
static int rtc6226_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct rtc6226_device *radio;
	struct v4l2_device *v4l2_dev;
	struct v4l2_ctrl_handler *hdl;
	struct regulator *vddvreg = NULL;
	struct regulator *viovreg = NULL;
	int retval = 0;
	int i = 0;
	int kfifo_alloc_rc = 0;

	/* struct v4l2_ctrl *ctrl; */
	/* need to add description "irq-fm" in dts */

	FMDBG("%s enter\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		retval = -ENODEV;
		return retval;
	}

	/*
	 * if voltage regulator is not ready yet, return the error
	 * if error is -EPROBE_DEFER to kernel then probe will be called at
	 * later point of time.
	 */
	viovreg = regulator_get(&client->dev, "vio");
	if (IS_ERR(viovreg)) {
		retval = PTR_ERR(viovreg);
		FMDERR("%s: regulator_get(vio) failed. retval=%d\n",
			__func__, retval);
		return retval;
	}

	vddvreg = regulator_get(&client->dev, "vdd");
	if (IS_ERR(vddvreg)) {
		retval = PTR_ERR(vddvreg);
		FMDERR("%s: regulator_get(vdd) failed. retval=%d\n",
			__func__, retval);
		regulator_put(viovreg);
		return retval;
	}

	/* private data allocation and initialization */
	radio = kzalloc(sizeof(struct rtc6226_device), GFP_KERNEL);
	if (!radio) {
		retval = -ENOMEM;
		regulator_put(viovreg);
		regulator_put(vddvreg);
		return retval;
	}

	v4l2_dev = &radio->v4l2_dev;
	retval = v4l2_device_register(&client->dev, v4l2_dev);
	if (retval < 0) {
		FMDERR("%s couldn't register v4l2_device\n", __func__);
		goto err_vreg;
	}

	FMDBG("v4l2_device_register successfully\n");
	hdl = &radio->ctrl_handler;

	radio->users = 0;
	radio->client = client;
	mutex_init(&radio->lock);
	init_completion(&radio->completion);

	retval = rtc6226_parse_dt(&client->dev, radio);
	if (retval) {
		FMDERR("%s: Parsing DT failed(%d)\n", __func__, retval);
		goto err_v4l2;
	}

	radio->vddreg = devm_kzalloc(&client->dev,
				sizeof(struct fm_power_vreg_data),
				GFP_KERNEL);
	if (!radio->vddreg) {
		FMDERR("%s: allocating memory for vdd vreg failed\n",
							__func__);
		retval = -ENOMEM;
		goto err_v4l2;
	}

	radio->vddreg->reg = vddvreg;
	radio->vddreg->name = "vdd";
	radio->vddreg->is_enabled = false;
	retval = rtc6226_dt_parse_vreg_info(&client->dev,
			radio->vddreg, "rtc6226,vdd-supply-voltage");
	if (retval < 0) {
		FMDERR("%s: parsing vdd-supply failed\n", __func__);
		goto err_v4l2;
	}

	radio->vioreg = devm_kzalloc(&client->dev,
				sizeof(struct fm_power_vreg_data),
				GFP_KERNEL);
	if (!radio->vioreg) {
		FMDERR("%s: allocating memory for vio vreg failed\n",
							__func__);
		retval = -ENOMEM;
		goto err_v4l2;
	}
	radio->vioreg->reg = viovreg;
	radio->vioreg->name = "vio";
	radio->vioreg->is_enabled = false;
	retval = rtc6226_dt_parse_vreg_info(&client->dev,
			radio->vioreg, "rtc6226,vio-supply-voltage");
	if (retval < 0) {
		FMDERR("%s: parsing vio-supply failed\n", __func__);
		goto err_v4l2;
	}
	/* Initialize pin control*/
	retval = rtc6226_pinctrl_init(radio);
	if (retval) {
		FMDERR("%s: rtc6226_pinctrl_init returned %d\n",
							__func__, retval);
		/* if pinctrl is not supported, -EINVAL is returned*/
		if (retval == -EINVAL)
			retval = 0;
	} else {
		FMDBG("%s rtc6226_pinctrl_init success\n", __func__);
	}

	memcpy(&radio->videodev, &rtc6226_viddev_template,
		sizeof(struct video_device));

	radio->videodev.v4l2_dev = v4l2_dev;
	radio->videodev.ioctl_ops = &rtc6226_ioctl_ops;
	video_set_drvdata(&radio->videodev, radio);

	/* rds buffer allocation */
	radio->buf_size = rds_buf * 3;
	radio->buffer = kmalloc(radio->buf_size, GFP_KERNEL);
	if (!radio->buffer) {
		retval = -EIO;
		goto err;
	}

	for (i = 0; i < RTC6226_FM_BUF_MAX; i++) {
		spin_lock_init(&radio->buf_lock[i]);

		kfifo_alloc_rc = kfifo_alloc(&radio->data_buf[i],
				STD_BUF_SIZE, GFP_KERNEL);

		if (kfifo_alloc_rc != 0) {
			FMDERR("%s: failed allocating buffers %d\n",
					__func__, kfifo_alloc_rc);
			retval = -ENOMEM;
			goto err_rds;
		}
	}
	radio->wqueue = NULL;
	radio->wqueue_scan = NULL;
	radio->wqueue_rds = NULL;
	radio->band = -1;

	/* rds buffer configuration */
	radio->wr_index = 0;
	radio->rd_index = 0;
	init_waitqueue_head(&radio->event_queue);
	init_waitqueue_head(&radio->read_queue);
	init_waitqueue_head(&rtc6226_wq);

	radio->wqueue  = create_singlethread_workqueue("fmradio");
	if (!radio->wqueue) {
		retval = -ENOMEM;
		goto err_rds;
	}

	radio->wqueue_scan  = create_singlethread_workqueue("fmradioscan");
	if (!radio->wqueue_scan) {
		retval = -ENOMEM;
		goto err_wqueue;
	}

	radio->wqueue_rds  = create_singlethread_workqueue("fmradiords");
	if (!radio->wqueue_rds) {
		retval = -ENOMEM;
		goto err_wqueue_scan;
	}

	/* register video device */
	retval = video_register_device(&radio->videodev, VFL_TYPE_RADIO,
		radio_nr);
	if (retval) {
		dev_info(&client->dev, "Could not register video device\n");
		goto err_all;
	}

	i2c_set_clientdata(client, radio);		/* move from below */
	FMDBG("%s exit\n", __func__);
	return 0;

err_all:
	destroy_workqueue(radio->wqueue_rds);
err_wqueue_scan:
	destroy_workqueue(radio->wqueue_scan);
err_wqueue:
	destroy_workqueue(radio->wqueue);
err_rds:
	kfree(radio->buffer);
err:
	video_device_release(&radio->videodev);
err_v4l2:
	v4l2_device_unregister(v4l2_dev);
err_vreg:
	if (radio && radio->vioreg && radio->vioreg->reg) {
		regulator_put(radio->vioreg->reg);
		devm_kfree(&client->dev, radio->vioreg);
	} else {
		regulator_put(viovreg);
	}
	if (radio && radio->vddreg && radio->vddreg->reg) {
		regulator_put(radio->vddreg->reg);
		devm_kfree(&client->dev, radio->vddreg);
	} else {
		regulator_put(vddvreg);
	}
	kfree(radio);
	return retval;
}

/*
 * rtc6226_i2c_remove - remove the device
 */
static int rtc6226_i2c_remove(struct i2c_client *client)
{
	struct rtc6226_device *radio = i2c_get_clientdata(client);

	free_irq(client->irq, radio);
	kfree(radio->buffer);
	video_device_release(&radio->videodev);
	v4l2_ctrl_handler_free(&radio->ctrl_handler);
	video_unregister_device(&radio->videodev);
	v4l2_device_unregister(&radio->v4l2_dev);
	kfree(radio);
	FMDBG("%s exit\n", __func__);

	return 0;
}

#ifdef CONFIG_PM
/*
 * rtc6226_i2c_suspend - suspend the device
 */
static int rtc6226_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rtc6226_device *radio = i2c_get_clientdata(client);

	FMDBG("%s %d\n", __func__, radio->client->addr);

	return 0;
}


/*
 * rtc6226_i2c_resume - resume the device
 */
static int rtc6226_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rtc6226_device *radio = i2c_get_clientdata(client);

	FMDBG("%s %d\n", __func__, radio->client->addr);

	return 0;
}

static SIMPLE_DEV_PM_OPS(rtc6226_i2c_pm, rtc6226_i2c_suspend,
						rtc6226_i2c_resume);
#endif


/*
 * rtc6226_i2c_driver - i2c driver interface
 */
struct i2c_driver rtc6226_i2c_driver = {
	.driver = {
		.name			= "rtc6226",
		.owner			= THIS_MODULE,
		.of_match_table = of_match_ptr(rtc6226_i2c_dt_ids),
#ifdef CONFIG_PM
		.pm				= &rtc6226_i2c_pm,
#endif
	},
	.probe				= rtc6226_i2c_probe,
	.remove				= rtc6226_i2c_remove,
	.id_table			= rtc6226_i2c_id,
};

/*
 * rtc6226_i2c_init
 */
int rtc6226_i2c_init(void)
{
	FMDBG(DRIVER_DESC ", Version " DRIVER_VERSION "\n");
	return i2c_add_driver(&rtc6226_i2c_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
