/*
 * Reference Driver for NextInput Sensor
 *
 * The GPL Deliverables are provided to Licensee under the terms
 * of the GNU General Public License version 2 (the "GPL") and
 * any use of such GPL Deliverables shall comply with the terms
 * and conditions of the GPL. A copy of the GPL is available
 * in the license txt file accompanying the Deliverables and
 * at http://www.gnu.org/licenses/gpl.txt
 *
 * Copyright (C) NextInput, Inc.
 * Copyright (C) 2019 XiaoMi, Inc.
 * All rights reserved
 *
 * 1. Redistribution in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 2. Neither the name of NextInput nor the names of the contributors
 *    may be used to endorse or promote products derived from
 *    the software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES INCLUDING BUT
 * NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <asm/uaccess.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/firmware.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/ctype.h>
#include "../touchscreen/fts/fts.h"
#include "ni_af3018.h"

static int num_clients;
static int adcraw_mode;
static struct workqueue_struct *ni_af3018_wq;
#ifdef NI_MCU
#include "ni_mcu.c"
#endif
static ssize_t ni_af3018_force_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t ni_af3018_force_mode_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t ni_af3018_force_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t ni_af3018_baseline_show(struct device *dev, struct device_attribute *attr, char *buf);
#ifdef DEVICE_INTERRUPT
static ssize_t ni_af3018_interrupt_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
#endif
static ssize_t ni_af3018_threshold_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t ni_af3018_register_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t ni_af3018_device_id_show(struct device *dev, struct device_attribute *attr, char *buf);

static struct device_attribute ni_af3018_device_attrs[] = {
#ifdef NI_MCU
	__ATTR(firmware,      S_IRUGO | NI_WRITE_PERMISSIONS, ni_af3018_fw_show,    ni_af3018_fw_store),
#endif
	__ATTR(force,         S_IRUGO,                        ni_af3018_force_show,      NULL),
	__ATTR(device_id,         S_IRUGO,                      ni_af3018_device_id_show,      NULL),
	__ATTR(force_mode,    S_IRUGO | NI_WRITE_PERMISSIONS, ni_af3018_force_mode_show, ni_af3018_force_mode_store),
	__ATTR(baseline,      S_IRUGO,                        ni_af3018_baseline_show,   NULL),
#ifdef DEVICE_INTERRUPT
	__ATTR(interrupt,               NI_WRITE_PERMISSIONS, NULL,                      ni_af3018_interrupt_store),
#endif
	__ATTR(threshold,               NI_WRITE_PERMISSIONS, NULL,                      ni_af3018_threshold_store),
	__ATTR(register,      S_IRUGO,                        ni_af3018_register_show,   NULL),
#ifdef NI_MCU
#ifdef HOST_DEBUGGER
	__ATTR(dump_trace,    S_IRUGO,                        ni_af3018_dump_trace,      NULL),
#endif
#endif
};

static ssize_t ni_af3018_device_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 databuffer = 0;
	u8 devid = 0;
	u8 rev = 0;
	int ret = 0;
	struct ni_af3018_ts_data *ts = dev_get_drvdata(dev);
	LOGI("%s\n", __func__);
#if DEVID_REG != REV_REG
#error "Assumption about DEVID_REG and REV_REG invalid"
#endif

	if (unlikely(ni_af3018_i2c_read(ts->client, DEVID_REG, sizeof(databuffer), &databuffer) < 0)) {
		LOGE("DEVID_REG read fail\n");
		ret = snprintf(buf, PAGE_SIZE,  "%s", "i2c_read error\n");
	} else {
		devid = (databuffer & DEVID_MSK) >> DEVID_POS;
		rev   = (databuffer & REV_MSK)   >> REV_POS;
		LOGI("%s: IC DEVID %d, REV %d\n", __func__, devid, rev);
		ret = snprintf(buf, PAGE_SIZE,  "DEVID:%d,REV:%d\n", devid, rev);
	}

	return ret;
}

static ssize_t ni_af3018_force_show(struct device *dev,  struct device_attribute *attr, char *buf)
{
	u8 data_buffer_out[NUM_SENSORS * OEM_ADCOUT_LEN];
	int ret = 0;
	u8  i;
	s16 sample;
	struct ni_af3018_ts_data *ts = dev_get_drvdata(dev);
#ifdef NI_MCU

	if (ts->fw_info.fw_upgrade.bootloadermode == true)
		return 0;

#endif

	if (unlikely(ni_af3018_i2c_read(ts->client, ADCOUT_REG, sizeof(data_buffer_out), data_buffer_out) < 0)) {
		ret = snprintf(buf, PAGE_SIZE,  "%s", "i2c_read error\n");
	} else {
		ret = snprintf(buf, PAGE_SIZE,  "%d ", NUM_SENSORS);

		for (i = 0; i < NUM_SENSORS; i++) {
			sample = (((u16) data_buffer_out[(i * OEM_ADCOUT_LEN) + 0]) << ADCOUT_SHIFT) + (data_buffer_out[(i * OEM_ADCOUT_LEN) + 1]  >> ADCOUT_SHIFT);
#ifdef AFE_REV1

			if (adcraw_mode) {
				if (sample & 0x800) {
					sample = -(sample & 0x7ff);
				}
			} else
#endif
			if (sample & 0x800) {
				sample |= ((-1) & ~0xfff);
			}
			ret += snprintf(buf + ret, PAGE_SIZE, "%d ", sample);
		}

		ret += snprintf(buf + ret, PAGE_SIZE, "%s", "\n");
	}

	return ret;
}

static ssize_t ni_af3018_force_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 mode;
	int ret = 0;
	struct ni_af3018_ts_data *ts = dev_get_drvdata(dev);
#ifdef NI_MCU

	if (ts->fw_info.fw_upgrade.bootloadermode == true)
		return 0;

#endif

	if (unlikely(ni_af3018_i2c_read(ts->client, ADCRAW_REG, sizeof(mode), &mode) < 0)) {
		ret = snprintf(buf, PAGE_SIZE,  "%s", "i2c_read error\n");
	} else {
		ret = snprintf(buf, PAGE_SIZE,  "%d\n", (mode & ADCRAW_MSK) >> ADCRAW_POS);
	}

	return ret;
}

static ssize_t ni_af3018_force_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int mode;
	struct ni_af3018_ts_data *ts = dev_get_drvdata(dev);
#ifdef NI_MCU

	if (ts->fw_info.fw_upgrade.bootloadermode == true)
		return count;

#endif

	if ((sscanf(buf, "%d", &mode) != 1) || (mode < 0) || (mode > 1)) {
		LOGE("Invalid force mode\n");
		return count;
	}

	if (unlikely(ni_af3018_i2c_modify_byte(ts->client, ADCRAW_REG, mode << ADCRAW_POS, ADCRAW_MSK) < 0)) {
		LOGE("Force mode not changed");
	}

	adcraw_mode = mode;
	return count;
}

static ssize_t ni_af3018_baseline_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data_buffer_out[NUM_SENSORS * OEM_ADCOUT_LEN];
	int ret = 0;
	struct ni_af3018_ts_data *ts = dev_get_drvdata(dev);
#ifdef NI_MCU

	if (ts->fw_info.fw_upgrade.bootloadermode == true)
		return 0;

#endif

	if (unlikely(ni_af3018_i2c_read(ts->client, BASELINE_REG, sizeof(data_buffer_out), data_buffer_out) < 0)) {
		ret = snprintf(buf, PAGE_SIZE,  "%s", "i2c_read error\n");
	} else {
		u8  i;
		s16 baseline;
		ret = snprintf(buf, PAGE_SIZE,  "%d ", NUM_SENSORS);

		for (i = 0; i < NUM_SENSORS; i++) {
			baseline = (((u16) data_buffer_out[(i * OEM_ADCOUT_LEN) + 0]) << ADCOUT_SHIFT) + (data_buffer_out[(i * OEM_ADCOUT_LEN) + 1]  >> ADCOUT_SHIFT);
#ifdef AFE_REV1

			if (baseline & 0x800) {
				baseline = -(baseline & 0x7ff);
			}

#else

			if (baseline & 0x800) {
				baseline |= ((-1) & ~0xfff);
			}

#endif
			ret += snprintf(buf + ret, PAGE_SIZE, "%d ", baseline);
		}

		ret += snprintf(buf + ret, PAGE_SIZE, "%s", "\n");
	}

	return ret;
}

#ifdef DEVICE_INTERRUPT
static ssize_t ni_af3018_interrupt_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t count)
{
	int interrupt;
	struct ni_af3018_ts_data *ts = dev_get_drvdata(dev);
#ifdef NI_MCU

	if (ts->fw_info.fw_upgrade.bootloadermode == true)
		return count;

#endif

	if ((sscanf(buf, "%d", &interrupt) != 1) || (interrupt < 0) || (interrupt > 1)) {
		LOGE("Invalid interrupt\n");
		return count;
	}

	if (unlikely(ni_af3018_i2c_modify_array(ts->client, INTREN_REG, interrupt << INTREN_POS,
						INTREN_MSK, 2) < 0)) {
		LOGE("Error modifying interrupts\n");
	}

	return count;
}
#endif

static ssize_t ni_af3018_threshold_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	const char *pbuf = buf;
	u32 autocal;
	u32 interrupt;
	u8 data_buffer_out[OEM_ADCOUT_LEN];
	struct ni_af3018_ts_data *ts = dev_get_drvdata(dev);

#ifdef NI_MCU
	if (ts->fw_info.fw_upgrade.bootloadermode == true)
		return count;
#endif
	if (sscanf(pbuf, "%d %d", &autocal, &interrupt) != 2) {
		LOGE("Invalid input\n");
		return count;
	} else
		LOGI("%s, autocal:%d, interrupt:%d\n", __func__, autocal, interrupt);
	/* read, modify, write autocal thresholds */
	if (unlikely(ni_af3018_i2c_read(ts->client, AUTOCAL_REG,
					sizeof(data_buffer_out), data_buffer_out) < 0)) {
		LOGE("Error reading autocal thresholds\n");
		return count;
	}

	data_buffer_out[0] = (u8) (autocal >> AUTOCAL_SHIFT);
	data_buffer_out[1] = (u8) (data_buffer_out[1] & 0x0f) | (u8) (autocal << AUTOCAL_SHIFT);


	if (unlikely(ni_af3018_i2c_write(ts->client, AUTOCAL_REG,
					 sizeof(data_buffer_out), data_buffer_out) < 0)) {
		LOGE("Error writing autocal thresholds\n");
		return count;
	}

	/* read, modify, write interrupt thresholds */
	if (unlikely(ni_af3018_i2c_read(ts->client, INTRTHRSLD_REG,
					sizeof(data_buffer_out), data_buffer_out) < 0)) {
		LOGE("Error reading interrupt thresholds\n");
		return count;
	}

	data_buffer_out[0] = (u8) (interrupt >> INTRTHRSLD_SHIFT);
	data_buffer_out[1] = (u8) (data_buffer_out[1] & 0x0f) | (u8) (interrupt << INTRTHRSLD_SHIFT);

	if (unlikely(ni_af3018_i2c_write(ts->client, INTRTHRSLD_REG,
					 sizeof(data_buffer_out), data_buffer_out) < 0)) {
		LOGE("Error writing interrupt thresholds\n");
		return count;
	}

	return count;
}

static ssize_t ni_af3018_register_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data_buffer_out[LAST_REG - FIRST_REG + 1];
	int ret = 0;
	u8 i;
	struct ni_af3018_ts_data *ts = dev_get_drvdata(dev);
#ifdef NI_MCU

	if (ts->fw_info.fw_upgrade.bootloadermode == true)
		return 0;

#endif

	if (unlikely(ni_af3018_i2c_read(ts->client, FIRST_REG, sizeof(data_buffer_out), data_buffer_out) < 0)) {
		ret = snprintf(buf, PAGE_SIZE,  "%s", "i2c_read error\n");
	} else {
		for (i = 0; i < sizeof(data_buffer_out); i++) {
			ret += snprintf(buf + ret, PAGE_SIZE, "%02x ", data_buffer_out[i]);

			if ((i & 0x3) == 0x3) {
				ret += snprintf(buf + ret, PAGE_SIZE, "%s", " ");
			}
		}

		ret += snprintf(buf + ret, PAGE_SIZE, "%s", "\n");
	}

	return ret;
}

#if defined(INPUT_DEVICE) && defined(DEVICE_INTERRUPT)
static void ni_af3018_abs_input_report(struct ni_af3018_ts_data *ts, const ktime_t timestamp)
{
	int i;
#ifdef EVENT_SYN
	input_event(ts->input_dev, EV_SYN, SYN_TIME_SEC, ktime_to_timespec(timestamp).tv_sec);
	input_event(ts->input_dev, EV_SYN, SYN_TIME_NSEC, ktime_to_timespec(timestamp).tv_nsec);
#endif

	for (i = 0; i < NUM_SENSORS; i++) {
#ifdef CONFIG_TOUCHSCREEN_ST_CORE

		if (fts_is_infod()) {
#endif
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, ts->force[i]);
			input_sync(ts->input_dev);
#ifdef CONFIG_TOUCHSCREEN_ST_CORE
		}

#endif
	}
}
#endif

static void ni_af3018_release_interrupt_func(struct work_struct *work_release_interrupt)
{
	struct ni_af3018_ts_data *ts = container_of(work_release_interrupt, struct ni_af3018_ts_data, work_release_interrupt);
	int i = 0;

	LOGI("%s\n", __func__);
	if (ni_af3018_ts_get_data(ts->client) == 0) {
		for (i = 0; i < NUM_SENSORS; i++) {
			LOGI("%s, force:%d, release_threshold:%d\n", __func__, ts->force[i], ts->release_threshold[i]);
			if (ts->force[i] < ts->release_threshold[i]) {
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
				input_sync(ts->input_dev);
			}
		}
#ifdef CONFIG_TOUCHSCREEN_ST_CORE
	fts_senseoff_without_cal();
#endif
	}
}

#ifdef DEVICE_INTERRUPT
static void ni_af3018_recover_func(struct work_struct *work_recover)
{
	struct ni_af3018_ts_data *ts = container_of(work_recover,
				       struct ni_af3018_ts_data,
				       work_recover);
	LOGI("%s\n", __func__);
	disable_irq(ts->client->irq);

	if (ts->curr_pwr_state == POWER_ON) {
		ni_af3018_ic_init(ts);
		enable_irq(ts->client->irq);
	}

	enable_irq(ts->client->irq);
}
#endif

static void *get_touch_handle(struct i2c_client *client)
{
	return i2c_get_clientdata(client);
}

static int ni_af3018_i2c_read(struct i2c_client *client, u8 reg, int len, u8 *buf)
{
#ifdef DEVICE_INTERRUPT
	struct ni_af3018_ts_data *ts = (struct ni_af3018_ts_data *)get_touch_handle(client);
#endif
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};
#ifdef DEVICE_INTERRUPT
	ts->enableInterrupt = 0;
#endif

	if (i2c_transfer(client->adapter, msgs, 2) < 0) {
		if (printk_ratelimit())
			LOGE("transfer error\n");

		return -EIO;
	}

#ifdef DEVICE_INTERRUPT
	ts->enableInterrupt = 1;
#endif
	return 0;
}

static int ni_af3018_i2c_write(struct i2c_client *client, u8 reg, int len, u8 *buf)
{
#ifdef DEVICE_INTERRUPT
	struct ni_af3018_ts_data *ts = (struct ni_af3018_ts_data *)get_touch_handle(client);
#endif
	u8 send_buf[len + 1];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = len + 1,
			.buf = send_buf,
		},
	};
#ifdef DEVICE_INTERRUPT
	ts->enableInterrupt = 0;
#endif
	send_buf[0] = (u8)reg;
	memcpy(&send_buf[1], buf, len);

	if (i2c_transfer(client->adapter, msgs, 1) < 0) {
		if (printk_ratelimit())
			LOGE("transfer error\n");

		return -EIO;
	}

#ifdef DEVICE_INTERRUPT
	ts->enableInterrupt = 1;
#endif
	return 0;
}

static int ni_af3018_i2c_write_byte(struct i2c_client *client, u8 reg, u8 data)
{
#ifdef DEVICE_INTERRUPT
	struct ni_af3018_ts_data *ts = (struct ni_af3018_ts_data *)get_touch_handle(client);
#endif
	u8 send_buf[2];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 2,
			.buf = send_buf,
		},
	};
#ifdef DEVICE_INTERRUPT
	ts->enableInterrupt = 0;
#endif
	send_buf[0] = (u8)reg;
	send_buf[1] = (u8)data;

	if (i2c_transfer(client->adapter, msgs, 1) < 0) {
		if (printk_ratelimit())
			LOGE("transfer error\n");

		return -EIO;
	}

#ifdef DEVICE_INTERRUPT
	ts->enableInterrupt = 1;
#endif
	return 0;
}

/* ni_af3018_i2c_modify_byte
*
* Read register, clear bits specified by 'mask', set any bits specified by 'data',
* then write back to register
*
*/
static int ni_af3018_i2c_modify_byte(struct i2c_client *client, u8 reg, u8 data, u8 mask)
{
	u8 buf;

	if (unlikely(ni_af3018_i2c_read(client, reg, sizeof(buf), &buf) < 0)) {
		return -EIO;
	}

	buf = (buf & ~mask) | data;

	if (unlikely(ni_af3018_i2c_write_byte(client, reg, buf) < 0)) {
		return -EIO;
	}

	return 0;
}

/* ni_af3018_i2c_modify_array
*
* Read NUM_SENSORS-length register array, and for each register spaced 'offset'
* bytes apart within this array, clear bits specified by 'mask', set any bits
* specified by 'data', then write back to register
*
*/
static int ni_af3018_i2c_modify_array(struct i2c_client *client, u8 reg, u8 data, u8 mask, u8 offset)
{
	int i;
	u8 buf[NUM_SENSORS * offset];

	if (unlikely(ni_af3018_i2c_read(client, reg, sizeof(buf), buf) < 0)) {
		return -EIO;
	}

	for (i = 0; i < NUM_SENSORS; i++) {
		buf[i * offset] = (buf[i * offset] & ~mask) | data;
	}

	if (unlikely(ni_af3018_i2c_write(client, reg, sizeof(buf), buf) < 0)) {
		return -EIO;
	}

	return 0;
}

static int ni_af3018_ts_get_data(struct i2c_client *client)
{
	int i = 0;
	struct ni_af3018_ts_data *ts = (struct ni_af3018_ts_data *)get_touch_handle(client);
	u8 data_buffer_out[NUM_SENSORS * OEM_ADCOUT_LEN];
#ifdef NI_MCU

	if (ts->fw_info.fw_upgrade.bootloadermode == true)
		return 0;

#endif

	if (unlikely(ni_af3018_i2c_read(client, ADCOUT_REG, sizeof(data_buffer_out), data_buffer_out) < 0)) {
		LOGE("ADCOUT_REG fail\n");
		return -EIO;
	}

	/* Sensor data */
	for (i = 0; i < NUM_SENSORS; i++) {
		ts->force[i] = (((u16) data_buffer_out[(i * OEM_ADCOUT_LEN) + 0]) << ADCOUT_SHIFT) + (data_buffer_out[(i * OEM_ADCOUT_LEN) + 1]  >> ADCOUT_SHIFT);
	}

	return 0;
}
static int ni_af3018_get_ic_info(struct ni_af3018_ts_data *ts)
{
#ifdef NI_MCU
	u8 databuffer[64];
	int i;
	LOGI("%s\n", __func__);
	memset(&ts->fw_info, 0, sizeof(struct ni_af3018_ts_fw_info));

	if (unlikely(ni_af3018_i2c_read(ts->client, NI_CMD_FW_VERSION, sizeof(databuffer), databuffer) < 0)) {
		LOGE("NI_CMD_FW_VERSION read fail\n");
		return -EIO;
	}

	ts->fw_info.fw_ver   = databuffer[0];
	ts->fw_info.fw_rev   = databuffer[1];
	ts->fw_info.fw_build = databuffer[2];

	for (i = 0; i < 11; i++)
		ts->fw_info.buildDate[i] = databuffer[3 + i];

	for (i = 0; i < 8; i++)
		ts->fw_info.buildTime[i] = databuffer[14 + i];

	snprintf(ts->fw_info.ic_fw_identifier, sizeof(ts->fw_info.ic_fw_identifier),
		 "FP %d.%d.%d",
		 ts->fw_info.fw_ver, ts->fw_info.fw_rev, ts->fw_info.fw_build);
	LOGI("%s: IC identifier[%s]\n", __func__, ts->fw_info.ic_fw_identifier);
#else
	u8 databuffer;
	u8 devid;
	u8 rev;
	LOGI("%s\n", __func__);
#if DEVID_REG != REV_REG
#error "Assumption about DEVID_REG and REV_REG invalid"
#endif

	if (unlikely
	    (ni_af3018_i2c_read
	     (ts->client, DEVID_REG, sizeof(databuffer), &databuffer) < 0)) {
		LOGE("DEVID_REG read fail\n");
		return -EIO;
	}

	devid = (databuffer & DEVID_MSK) >> DEVID_POS;
	rev   = (databuffer & REV_MSK)   >> REV_POS;
	LOGI("%s: IC DEVID %d, REV %d\n", __func__, devid, rev);
#endif
	return 0;
}

static int ni_af3018_parse_dt(struct device *dev, struct ni_af3018_platform_data *pdata)
{
#ifdef DEVICE_INTERRUPT
	int rc;
	struct device_node *np = dev->of_node;
#endif
	LOGV("%s\n", __func__);
#ifdef DEVICE_INTERRUPT
	rc = of_get_named_gpio_flags(np, "nif,irq-gpio", 0, &pdata->irq_gpio_flags);

	if (rc < 0) {
		LOGE("%s: Failed with error %d\n", __func__, rc);
		dev_err(dev, "Unable to get irq gpio\n");
		return rc;
	} else {
		LOGV("%s: RC %d\n", __func__, rc);
		pdata->irq_gpio = rc;
	}

#endif
	return 0;
}

#ifdef DEVICE_INTERRUPT
static int ni_af3018_work_pre_proc(struct ni_af3018_ts_data *ts)
{
	int ret = 0;

	if (gpio_get_value(ts->pdata->irq_gpio) != 0) {
		LOGI("%s: INT STATE HIGH\n", __func__);
		return -EINTR;
	}

	ret = ni_af3018_ts_get_data(ts->client);

	if (ret != 0) {
		LOGE("get data fail\n");
		return ret;
	}

	return 0;
}

static enum hrtimer_restart ni_af3018_release_timer(struct hrtimer *timer)
{
	struct ni_af3018_ts_data *ts = container_of(timer, struct ni_af3018_ts_data,
					release_timer);

	ts->released = 1;
	schedule_work(&ts->work_release_interrupt);

	return HRTIMER_NORESTART;
}

static irqreturn_t ni_af3018_irq_handler(int irq, void *dev_id)
{
	struct ni_af3018_ts_data *ts = (struct ni_af3018_ts_data *)dev_id;
#ifdef INPUT_DEVICE
	ktime_t timestamp = ktime_get();
#endif
	u8 buf;

	if (ts->suspend_flag) {
#ifdef INPUT_DEVICE
		/* wait for i2c wakeup */
		msleep(100);

		if (ts->enableInterrupt && (ni_af3018_work_pre_proc(ts) == 0)) {
			ni_af3018_abs_input_report(ts, timestamp);
			input_event(ts->input_dev, EV_KEY, KEY_POWER, 1);
			input_sync(ts->input_dev);
			input_event(ts->input_dev, EV_KEY, KEY_POWER, 0);
			input_sync(ts->input_dev);
		}

#endif
	} else {
		if (ts->enableInterrupt) {
			switch (ni_af3018_work_pre_proc(ts)) {
			case 0:
#ifdef CONFIG_TOUCHSCREEN_ST_CORE
				fts_senseon_without_cal();
#endif
#ifdef INPUT_DEVICE
				ni_af3018_abs_input_report(ts, timestamp);
#endif
				if (ts->released == 1)
					hrtimer_start(&ts->release_timer, ktime_set(0, 1000 * 1000 * 1000), HRTIMER_MODE_REL);
				if (!ts->released) {
					hrtimer_start_range_ns(&ts->release_timer, ns_to_ktime(1000 * 1000 * 1000), 0, HRTIMER_MODE_REL);
				}
				break;

			case -EIO:
				queue_work(ni_af3018_wq, &ts->work_recover);
				break;
			}
		}
	}

	/* acknowledge DIC interrupt */
	if (unlikely(ni_af3018_i2c_read(ts->client, INTR_REG, sizeof(buf), &buf) < 0)) {
		LOGE("Error reading INTR_REG\n");
	}

	ts->released = 0;
	return IRQ_HANDLED;
}
#endif

static void ni_af3018_init_func(struct work_struct *work_init)
{
	struct ni_af3018_ts_data *ts = container_of(to_delayed_work(work_init), struct ni_af3018_ts_data, work_init);
	LOGI("%s\n", __func__);
#ifdef INPUT_DEVICE
	mutex_lock(&ts->input_dev->mutex);
#endif

	if (!ts->curr_resume_state) {
#ifdef INPUT_DEVICE
		mutex_unlock(&ts->input_dev->mutex);
#endif
		return;
	}

	/* Specific device initialization */
	ni_af3018_ic_init(ts);
#ifdef INPUT_DEVICE
	mutex_unlock(&ts->input_dev->mutex);
#endif
}

static int ni_af3018_ic_init(struct ni_af3018_ts_data *ts)
{
	int interrupt = DEFAULT_INT_THRE;
	u8 data_buffer_out[NUM_SENSORS * OEM_ADCOUT_LEN];

	LOGI("%s\n", __func__);
	if (unlikely(ts->ic_init_err_cnt >= MAX_RETRY_COUNT)) {
		LOGE("Init Failed: Irq-pin has some unknown problems\n");
		ts->ic_init_err_cnt = 0;
		return -EIO;
	}

	if (ni_af3018_init_panel(ts->client) < 0) {
		LOGE("specific device initialization fail\n");
		ts->ic_init_err_cnt++;
		queue_delayed_work(ni_af3018_wq, &ts->work_init, msecs_to_jiffies(10));
		return 0;
	}

	/* make devices active */
	if (unlikely(ni_af3018_i2c_modify_array(ts->client, EN_REG,
						1 << EN_POS, EN_MSK, 1) < 0)) {
		LOGE("EN_REG modify fail\n");
		return -EIO;
	}

#ifdef AFE_REV1
#define REG0_VALUE ((WAIT_8MS << WAIT_POS) | (ADCRAW_RAW << ADCRAW_POS) | (1 << EN_POS))

	if (unlikely(ni_af3018_i2c_write_byte(ts->client, 0, REG0_VALUE) < 0)) {
		LOGE("afe silicon: Reg 0 write fail\n");
		return -EIO;
	}

	adcraw_mode = 1;

	if (unlikely(ni_af3018_i2c_write_byte(ts->client, 1, 0x70) < 0)) {
		LOGE("afe silicon: Reg 1 write fail\n");
		return -EIO;
	}

	if (unlikely(ni_af3018_i2c_write_byte(ts->client, 0xb, 0x18) < 0)) {
		LOGE("afe silicon: Reg 0xb write fail\n");
		return -EIO;
	}

#endif
#ifdef DEVICE_INTERRUPT

	/* enable interrupts */
	if (unlikely(ni_af3018_i2c_modify_array(ts->client, INTREN_REG,
						1 << INTREN_POS, INTREN_MSK, 2) < 0)) {
		LOGE("INTREN_REG modify fail\n");
		return -EIO;
	}

	/* enable interrupt persist mode */
	if (unlikely(ni_af3018_i2c_modify_byte(ts->client, INTRPERSIST_REG,
					       INTRPERSIST_INF << INTRPERSIST_POS, INTRPERSIST_MSK) < 0)) {
		LOGE("INTRPERSIST_REG modify fail\n");
		return -EIO;
	}

	/* modify interrupt thresholds */
	if (unlikely(ni_af3018_i2c_read(ts->client, INTRTHRSLD_REG,
					sizeof(data_buffer_out), data_buffer_out) < 0)) {
		LOGE("Error reading interrupt thresholds\n");
	}

	data_buffer_out[0] = (u8) (interrupt >> INTRTHRSLD_SHIFT);
	data_buffer_out[1] = (u8) (data_buffer_out[1] & 0x0f) | (u8) (interrupt << INTRTHRSLD_SHIFT);

	if (unlikely(ni_af3018_i2c_write(ts->client, INTRTHRSLD_REG,
					 sizeof(data_buffer_out), data_buffer_out) < 0)) {
		LOGE("Error writing interrupt thresholds\n");
	}

#endif
	atomic_set(&ts->device_init, 1);
	ts->ic_init_err_cnt = 0;
	return 0;
}

static int ni_af3018_init_panel(struct i2c_client *client)
{
	struct ni_af3018_ts_data *ts = (struct ni_af3018_ts_data *)get_touch_handle(client);
	LOGI("%s\n", __func__);

	if (!ts->is_probed)
		if (unlikely(ni_af3018_get_ic_info(ts) < 0))
			return -EIO;

	ts->is_probed = 1;
	return 0;
}

static int ni_af3018_free_wq(void)
{
	if (--num_clients <= 0) {
		if (ni_af3018_wq) {
			destroy_workqueue(ni_af3018_wq);
			ni_af3018_wq = NULL;
		}
	}

	return 0;
}

#ifdef CONFIG_PM
static int ni_af3018_pm_suspend(struct device *dev)
{
	struct ni_af3018_ts_data *ts = dev_get_drvdata(dev);
	LOGI("%s enable nif irq wake\n", __func__);
	enable_irq_wake(ts->client->irq);
	hrtimer_cancel(&ts->release_timer);
	cancel_work(&ts->work_release_interrupt);
	ts->suspend_flag = 1;
	return 0;
}

static int ni_af3018_pm_resume(struct device *dev)
{
	struct ni_af3018_ts_data *ts = dev_get_drvdata(dev);
	LOGI("%s disable nif irq wake\n", __func__);
	disable_irq_wake(ts->client->irq);
	ts->suspend_flag = 0;
	return 0;
}

static const struct dev_pm_ops ni_af3018_dev_pm_ops = {
	.suspend = ni_af3018_pm_suspend,
	.resume  = ni_af3018_pm_resume,
};
#endif

static int ni_af3018_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ni_af3018_ts_data *ts;
	struct ni_af3018_platform_data *pdata;
	int ret = 0;
	u8 i2c_test = 0;
	int i;

	if (client->dev.of_node) {
		LOGV("%s: Allocating Memory... \n", __func__);
		pdata = devm_kzalloc(&client->dev,
				     sizeof(struct ni_af3018_platform_data),
				     GFP_KERNEL);

		if (!pdata) {
			LOGE("%s: Failed to allocate memory\n", __func__);
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = ni_af3018_parse_dt(&client->dev, pdata);

		if (ret) {
			LOGE("%s: Failed with error %d\n", __func__, ret);
			return ret;
		}
	} else {
		pdata = client->dev.platform_data;

		if (!pdata) {
			LOGE("%s: Failed with error %d\n", __func__, -ENODEV);
			return -ENODEV;
		}
	}

	LOGI("%s: Checking I2C Functionality\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		LOGE("i2c functionality check error\n");
		return -EPERM;
	}

	if (!ni_af3018_wq) {
		ni_af3018_wq = create_singlethread_workqueue("ni_af3018_wq");

		if (!ni_af3018_wq) {
			LOGE("create_singlethread_workqueue error\n");
			return -ENOMEM;
		}
	}

	ts = kzalloc(sizeof(struct ni_af3018_ts_data), GFP_KERNEL);

	if (!ts) {
		LOGE("Can not allocate memory\n");
		ret = -ENOMEM;
		goto err_kzalloc_failed;
	}

	ts->pdata = pdata;
	ts->ic_init_err_cnt = 0;
	ts->client = client;
	i2c_set_clientdata(client, ts);
	atomic_set(&ts->device_init, 0);
	ts->curr_resume_state = 1;
	ts->released = 1;
	for (i = 0; i < NUM_SENSORS; i++)
		ts->release_threshold[i] = OEM_RELEASE_THRESHOLD;
#ifdef NI_MCU
	LOGI("%s: Resetting MCU...\n", __func__);
	ni_af3018_i2c_write_byte(ts->client, NI_CMD_RESET, 0);
#endif
	msleep(BOOTING_DELAY);
	INIT_DELAYED_WORK(&ts->work_init, ni_af3018_init_func);
#ifdef NI_MCU
	INIT_WORK(&ts->work_fw_upgrade, ni_af3018_fw_upgrade_func);
#endif
#ifdef DEVICE_INTERRUPT
	INIT_WORK(&ts->work_recover, ni_af3018_recover_func);
	INIT_WORK(&ts->work_release_interrupt, ni_af3018_release_interrupt_func);
#endif
#ifdef INPUT_DEVICE
	ts->input_dev = input_allocate_device();

	if (ts->input_dev == NULL) {
		LOGE("Failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	ts->input_dev->name = DEVICE_NAME;
	ts->input_dev->id.vendor  = OEM_ID_VENDOR;
	ts->input_dev->id.product = OEM_ID_PRODUCT;
	ts->input_dev->id.version = OEM_ID_VERSION;
#ifdef EVENT_SYN
	set_bit(EV_SYN, ts->input_dev->evbit);
#endif
	set_bit(EV_KEY, ts->input_dev->evbit);
	input_set_capability(ts->input_dev, EV_KEY, KEY_POWER);
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 2048, 0, 0);
	ret = input_register_device(ts->input_dev);

	if (ret < 0) {
		LOGE("Unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	}

#endif
#ifdef DEVICE_INTERRUPT
	ts->enableInterrupt = 1;
	ret = gpio_request(ts->pdata->irq_gpio, "nif,irq-gpio");

	if (ret < 0) {
		LOGE("FAIL: irq-gpio gpio_request\n");
		goto err_int_gpio_request_failed;
	}

	gpio_direction_input(ts->pdata->irq_gpio);
	client->irq = gpio_to_irq(ts->pdata->irq_gpio);
	ret = request_threaded_irq(client->irq, NULL, ni_af3018_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   client->name, ts);

	if (ret < 0) {
		LOGE("request_irq failed\n");
		goto err_interrupt_failed;
	}
	hrtimer_init(&ts->release_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ts->release_timer.function = ni_af3018_release_timer;

#endif
	LOGI("%s: Attempting MCU I2C read \n", __func__);

	/* Add i2c check routine for booting in no touch panel/ic case */
	for (i = 0; i < MAX_RETRY_COUNT; i++) {
		if (unlikely(ni_af3018_i2c_read(ts->client, WAIT_REG, sizeof(i2c_test), &i2c_test) < 0)) {
			LOGE("MCU I2C read fail\n");

			if (i == MAX_RETRY_COUNT - 1) {
				LOGE("No MCU \n");
				ret = -EIO;
				goto err_ni_af3018_i2c_read_failed;
			}
		} else {
			LOGI("%s: MCU I2C read success \n", __func__);
			break;
		}
	}

	ni_af3018_ic_init(ts);

	/* Firmware Upgrade Check - use thread for booting time reduction
	queue_work(ni_af3018_wq, &ts->work_fw_upgrade);
	*/
	for (i = 0; i < ARRAY_SIZE(ni_af3018_device_attrs); i++) {
		ret = device_create_file(&client->dev, &ni_af3018_device_attrs[i]);

		if (ret)
			goto err_dev_create_file;
	}

	dev_set_drvdata(&client->dev, ts);
	num_clients++;
	LOGI("%s OK (%d)\n", __func__, ret);
	return 0;
err_dev_create_file:

	for (i = i - 1; i >= 0; i--) {
		device_remove_file(&ts->client->dev, &ni_af3018_device_attrs[i]);
	}

err_ni_af3018_i2c_read_failed:
#ifdef DEVICE_INTERRUPT
	free_irq(ts->client->irq, ts);
err_interrupt_failed:
	gpio_free(ts->pdata->irq_gpio);
err_int_gpio_request_failed:
#endif
#ifdef INPUT_DEVICE
	input_unregister_device(ts->input_dev);
err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
#endif
err_kzalloc_failed:
	kfree(ts);
	ni_af3018_free_wq();
	LOGI("%s error (%d)\n", __func__, ret);
	return ret;
}

static int ni_af3018_ts_remove(struct i2c_client *client)
{
	struct ni_af3018_ts_data *ts = i2c_get_clientdata(client);
	int i;
	LOGI("%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(ni_af3018_device_attrs); i++) {
		device_remove_file(&client->dev, &ni_af3018_device_attrs[i]);
	}

#ifdef DEVICE_INTERRUPT
	free_irq(client->irq, ts);
	gpio_free(ts->pdata->irq_gpio);
#endif
#ifdef INPUT_DEVICE
	input_unregister_device(ts->input_dev);
	input_free_device(ts->input_dev);
#endif
	kfree(ts);
	ni_af3018_free_wq();
	return 0;
}

static struct of_device_id ni_af3018_match_table[] = {
	{.compatible = DEVICE_TREE_NAME,},
	{},
};

static struct i2c_device_id ni_af3018_ts_id[] = {
	{DEVICE_NAME, 0},
	{},
};

static struct i2c_driver ni_af3018_ts_driver = {
	.probe = ni_af3018_ts_probe,
	.remove = ni_af3018_ts_remove,
	.id_table = ni_af3018_ts_id,
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ni_af3018_match_table,
#ifdef CONFIG_PM
		.pm = &ni_af3018_dev_pm_ops,
#endif
	},
};

static int __init ni_af3018_ts_init(void)
{
	LOGI("***NextInput driver __init!\n");
	return i2c_add_driver(&ni_af3018_ts_driver);
}

static void __exit ni_af3018_ts_exit(void)
{
	LOGI("***NextInput driver __exit!\n");
	i2c_del_driver(&ni_af3018_ts_driver);
	ni_af3018_free_wq();
}

module_init(ni_af3018_ts_init);
module_exit(ni_af3018_ts_exit);

MODULE_AUTHOR("NextInput Corporation");
MODULE_DESCRIPTION("NextInput ForceTouch Driver for AF3018");
MODULE_LICENSE("GPL");
