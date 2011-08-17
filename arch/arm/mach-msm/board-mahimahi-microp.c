/* board-mahimahi-microp.c
 * Copyright (C) 2009 Google.
 * Copyright (C) 2009 HTC Corporation.
 *
 * The Microp on mahimahi is an i2c device that supports
 * the following functions
 *   - LEDs (Green, Amber, Jogball backlight)
 *   - Lightsensor
 *   - Headset remotekeys
 *   - G-sensor
 *   - Interrupts
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <asm/uaccess.h>
#include <linux/wakelock.h>
#include <asm/mach-types.h>
#include <mach/htc_pwrsink.h>
#include <linux/earlysuspend.h>
#include <linux/bma150.h>
#include <linux/lightsensor.h>
#include <asm/mach/mmc.h>
#include <mach/htc_35mm_jack.h>
#include <asm/setup.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/jiffies.h>

#include "board-mahimahi.h"


#define MICROP_I2C_NAME "mahimahi-microp"

#define MICROP_LSENSOR_ADC_CHAN		6
#define MICROP_REMOTE_KEY_ADC_CHAN	7

#define MICROP_I2C_WCMD_MISC				0x20
#define MICROP_I2C_WCMD_SPI_EN				0x21
#define MICROP_I2C_WCMD_AUTO_BL_CTL			0x23
#define MICROP_I2C_RCMD_SPI_BL_STATUS			0x24
#define MICROP_I2C_WCMD_BUTTONS_LED_CTRL		0x25
#define MICROP_I2C_RCMD_VERSION				0x30
#define MICROP_I2C_WCMD_ADC_TABLE			0x42
#define MICROP_I2C_WCMD_LED_MODE			0x53
#define MICROP_I2C_RCMD_GREEN_LED_REMAIN_TIME		0x54
#define MICROP_I2C_RCMD_AMBER_RED_LED_REMAIN_TIME	0x55
#define MICROP_I2C_RCMD_BLUE_LED_REMAIN_TIME		0x57
#define MICROP_I2C_WCMD_JOGBALL_LED_MODE		0x5A
#define MICROP_I2C_RCMD_JOGBALL_LED_REMAIN_TIME		0x5B
#define MICROP_I2C_WCMD_JOGBALL_LED_PWM_SET		0x5C
#define MICROP_I2C_WCMD_JOGBALL_LED_PERIOD_SET		0x5D
#define MICROP_I2C_WCMD_READ_ADC_VALUE_REQ		0x60
#define MICROP_I2C_RCMD_ADC_VALUE			0x62
#define MICROP_I2C_WCMD_REMOTEKEY_TABLE			0x63
#define MICROP_I2C_WCMD_LCM_REGISTER			0x70
#define MICROP_I2C_WCMD_GSENSOR_REG			0x73
#define MICROP_I2C_WCMD_GSENSOR_REG_DATA_REQ		0x74
#define MICROP_I2C_RCMD_GSENSOR_REG_DATA		0x75
#define MICROP_I2C_WCMD_GSENSOR_DATA_REQ		0x76
#define MICROP_I2C_RCMD_GSENSOR_X_DATA			0x77
#define MICROP_I2C_RCMD_GSENSOR_Y_DATA			0x78
#define MICROP_I2C_RCMD_GSENSOR_Z_DATA			0x79
#define MICROP_I2C_RCMD_GSENSOR_DATA			0x7A
#define MICROP_I2C_WCMD_OJ_REG				0x7B
#define MICROP_I2C_WCMD_OJ_REG_DATA_REQ			0x7C
#define MICROP_I2C_RCMD_OJ_REG_DATA			0x7D
#define MICROP_I2C_WCMD_OJ_POS_DATA_REQ			0x7E
#define MICROP_I2C_RCMD_OJ_POS_DATA			0x7F
#define MICROP_I2C_WCMD_GPI_INT_CTL_EN			0x80
#define MICROP_I2C_WCMD_GPI_INT_CTL_DIS			0x81
#define MICROP_I2C_RCMD_GPI_INT_STATUS			0x82
#define MICROP_I2C_RCMD_GPI_STATUS			0x83
#define MICROP_I2C_WCMD_GPI_INT_STATUS_CLR		0x84
#define MICROP_I2C_RCMD_GPI_INT_SETTING			0x85
#define MICROP_I2C_RCMD_REMOTE_KEYCODE			0x87
#define MICROP_I2C_WCMD_REMOTE_KEY_DEBN_TIME		0x88
#define MICROP_I2C_WCMD_REMOTE_PLUG_DEBN_TIME		0x89
#define MICROP_I2C_WCMD_SIMCARD_DEBN_TIME		0x8A
#define MICROP_I2C_WCMD_GPO_LED_STATUS_EN		0x90
#define MICROP_I2C_WCMD_GPO_LED_STATUS_DIS		0x91

#define IRQ_GSENSOR	(1<<10)
#define IRQ_LSENSOR  	(1<<9)
#define IRQ_REMOTEKEY	(1<<7)
#define IRQ_HEADSETIN	(1<<2)
#define IRQ_SDCARD	(1<<0)

#define READ_GPI_STATE_HPIN	(1<<2)
#define READ_GPI_STATE_SDCARD	(1<<0)

#define ALS_CALIBRATE_MODE  147

/* Check pattern, to check if ALS has been calibrated */
#define ALS_CALIBRATED	0x6DA5

/* delay for deferred light sensor read */
#define LS_READ_DELAY   (HZ/2)

/*#define DEBUG_BMA150  */
#ifdef DEBUG_BMA150
/* Debug logging of accelleration data */
#define GSENSOR_LOG_MAX 2048  /* needs to be power of 2 */
#define GSENSOR_LOG_MASK (GSENSOR_LOG_MAX - 1)

struct gsensor_log {
	ktime_t timestamp;
	short x;
	short y;
	short z;
};

static DEFINE_MUTEX(gsensor_log_lock);
static struct gsensor_log gsensor_log[GSENSOR_LOG_MAX];
static unsigned gsensor_log_head;
static unsigned gsensor_log_tail;

void gsensor_log_status(ktime_t time, short x, short y, short z)
{
	unsigned n;
	mutex_lock(&gsensor_log_lock);
	n = gsensor_log_head;
	gsensor_log[n].timestamp = time;
	gsensor_log[n].x = x;
	gsensor_log[n].y = y;
	gsensor_log[n].z = z;
	n = (n + 1) & GSENSOR_LOG_MASK;
	if (n == gsensor_log_tail)
		gsensor_log_tail = (gsensor_log_tail + 1) & GSENSOR_LOG_MASK;
	gsensor_log_head = n;
	mutex_unlock(&gsensor_log_lock);
}

static int gsensor_log_print(struct seq_file *sf, void *private)
{
	unsigned n;

	mutex_lock(&gsensor_log_lock);
	seq_printf(sf, "timestamp                  X      Y      Z\n");
	for (n = gsensor_log_tail;
	     n != gsensor_log_head;
	     n = (n + 1) & GSENSOR_LOG_MASK) {
		seq_printf(sf, "%10d.%010d %6d %6d %6d\n",
			   gsensor_log[n].timestamp.tv.sec,
			   gsensor_log[n].timestamp.tv.nsec,
			   gsensor_log[n].x, gsensor_log[n].y,
			   gsensor_log[n].z);
	}
	mutex_unlock(&gsensor_log_lock);
	return 0;
}

static int gsensor_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, gsensor_log_print, NULL);
}

static struct file_operations gsensor_log_fops = {
	.open = gsensor_log_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif /* def DEBUG_BMA150 */

static int microp_headset_has_mic(void);
static int microp_enable_headset_plug_event(void);
static int microp_enable_key_event(void);
static int microp_disable_key_event(void);

static struct h35mm_platform_data mahimahi_h35mm_data = {
	.plug_event_enable = microp_enable_headset_plug_event,
	.headset_has_mic = microp_headset_has_mic,
	.key_event_enable = microp_enable_key_event,
	.key_event_disable = microp_disable_key_event,
};

static struct platform_device mahimahi_h35mm = {
	.name           = "htc_headset",
	.id                     = -1,
	.dev            = {
		.platform_data  = &mahimahi_h35mm_data,
	},
};

enum led_type {
	GREEN_LED,
	AMBER_LED,
	RED_LED,
	BLUE_LED,
	JOGBALL_LED,
	BUTTONS_LED,
	NUM_LEDS,
};

static uint16_t lsensor_adc_table[10] = {
	0x000, 0x001, 0x00F, 0x01E, 0x03C, 0x121, 0x190, 0x2BA, 0x26E, 0x3FF
};

static uint16_t remote_key_adc_table[6] = {
	0, 33, 43, 110, 129, 220
};

static uint32_t golden_adc = 0xC0;
static uint32_t als_kadc;

static struct wake_lock microp_i2c_wakelock;

static struct i2c_client *private_microp_client;

struct microp_int_pin {
	uint16_t int_gsensor;
	uint16_t int_lsensor;
	uint16_t int_reset;
	uint16_t int_simcard;
	uint16_t int_hpin;
	uint16_t int_remotekey;
};

struct microp_led_data {
	int type;
	struct led_classdev ldev;
	struct mutex led_data_mutex;
	struct work_struct brightness_work;
	spinlock_t brightness_lock;
	enum led_brightness brightness;
	uint8_t mode;
	uint8_t blink;
};

struct microp_i2c_work {
	struct work_struct work;
	struct i2c_client *client;
	int (*intr_debounce)(uint8_t *pin_status);
	void (*intr_function)(uint8_t *pin_status);
};

struct microp_i2c_client_data {
	struct microp_led_data leds[NUM_LEDS];
	uint16_t version;
	struct microp_i2c_work work;
	struct delayed_work hpin_debounce_work;
	struct delayed_work ls_read_work;
	struct early_suspend early_suspend;
	uint8_t enable_early_suspend;
	uint8_t enable_reset_button;
	int microp_is_suspend;
	int auto_backlight_enabled;
	uint8_t light_sensor_enabled;
	uint8_t force_light_sensor_read;
	uint8_t button_led_value;
	int headset_is_in;
	int is_hpin_pin_stable;
	struct input_dev *ls_input_dev;
	uint32_t als_kadc;
	uint32_t als_gadc;
	uint8_t als_calibrating;
};

static char *hex2string(uint8_t *data, int len)
{
	static char buf[101];
	int i;

	i = (sizeof(buf) - 1) / 4;
	if (len > i)
		len = i;

	for (i = 0; i < len; i++)
		sprintf(buf + i * 4, "[%02X]", data[i]);

	return buf;
}

#define I2C_READ_RETRY_TIMES  10
#define I2C_WRITE_RETRY_TIMES 10

static int i2c_read_block(struct i2c_client *client, uint8_t addr,
	uint8_t *data, int length)
{
	int retry;
	int ret;
	struct i2c_msg msgs[] = {
	{
		.addr = client->addr,
		.flags = 0,
		.len = 1,
		.buf = &addr,
	},
	{
		.addr = client->addr,
		.flags = I2C_M_RD,
		.len = length,
		.buf = data,
	}
	};

	mdelay(1);
	for (retry = 0; retry <= I2C_READ_RETRY_TIMES; retry++) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2) {
			dev_dbg(&client->dev, "R [%02X] = %s\n", addr,
					hex2string(data, length));
			return 0;
		}
		msleep(10);
	}

	dev_err(&client->dev, "i2c_read_block retry over %d\n",
			I2C_READ_RETRY_TIMES);
	return -EIO;
}

#define MICROP_I2C_WRITE_BLOCK_SIZE 21
static int i2c_write_block(struct i2c_client *client, uint8_t addr,
	uint8_t *data, int length)
{
	int retry;
	uint8_t buf[MICROP_I2C_WRITE_BLOCK_SIZE];
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length + 1,
			.buf = buf,
		}
	};

	dev_dbg(&client->dev, "W [%02X] = %s\n", addr,
			hex2string(data, length));

	if (length + 1 > MICROP_I2C_WRITE_BLOCK_SIZE) {
		dev_err(&client->dev, "i2c_write_block length too long\n");
		return -E2BIG;
	}

	buf[0] = addr;
	memcpy((void *)&buf[1], (void *)data, length);

	mdelay(1);
	for (retry = 0; retry <= I2C_WRITE_RETRY_TIMES; retry++) {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret == 1)
			return 0;
		msleep(10);
	}
	dev_err(&client->dev, "i2c_write_block retry over %d\n",
			I2C_WRITE_RETRY_TIMES);
	return -EIO;
}

static int microp_read_adc(uint8_t channel, uint16_t *value)
{
	struct i2c_client *client;
	int ret;
	uint8_t cmd[2], data[2];

	client = private_microp_client;
	cmd[0] = 0;
	cmd[1] = channel;
	ret = i2c_write_block(client, MICROP_I2C_WCMD_READ_ADC_VALUE_REQ,
			      cmd, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: request adc fail\n", __func__);
		return -EIO;
	}

	ret = i2c_read_block(client, MICROP_I2C_RCMD_ADC_VALUE, data, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: read adc fail\n", __func__);
		return -EIO;
	}
	*value = data[0] << 8 | data[1];
	return 0;
}

static int microp_read_gpi_status(struct i2c_client *client, uint16_t *status)
{
	uint8_t data[2];
	int ret;

	ret = i2c_read_block(client, MICROP_I2C_RCMD_GPI_STATUS, data, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: read failed\n", __func__);
		return -EIO;
	}
	*status = (data[0] << 8) | data[1];
	return 0;
}

static int microp_interrupt_enable(struct i2c_client *client,
				   uint16_t interrupt_mask)
{
	uint8_t data[2];
	int ret = -1;

	data[0] = interrupt_mask >> 8;
	data[1] = interrupt_mask & 0xFF;
	ret = i2c_write_block(client, MICROP_I2C_WCMD_GPI_INT_CTL_EN, data, 2);

	if (ret < 0)
		dev_err(&client->dev, "%s: enable 0x%x interrupt failed\n",
			__func__, interrupt_mask);
	return ret;
}

static int microp_interrupt_disable(struct i2c_client *client,
				    uint16_t interrupt_mask)
{
	uint8_t data[2];
	int ret = -1;

	data[0] = interrupt_mask >> 8;
	data[1] = interrupt_mask & 0xFF;
	ret = i2c_write_block(client, MICROP_I2C_WCMD_GPI_INT_CTL_DIS, data, 2);

	if (ret < 0)
		dev_err(&client->dev, "%s: disable 0x%x interrupt failed\n",
			__func__, interrupt_mask);
	return ret;
}


/*
 * SD slot card-detect support
 */
static unsigned int sdslot_cd = 0;
static void (*sdslot_status_cb)(int card_present, void *dev_id);
static void *sdslot_mmc_dev;

int mahimahi_microp_sdslot_status_register(
		void (*cb)(int card_present, void *dev_id),
		void *dev_id)
{
	if (sdslot_status_cb)
		return -EBUSY;
	sdslot_status_cb = cb;
	sdslot_mmc_dev = dev_id;
	return 0;
}

unsigned int mahimahi_microp_sdslot_status(struct device *dev)
{
	return sdslot_cd;
}

static void mahimahi_microp_sdslot_update_status(int status)
{
	sdslot_cd = !(status & READ_GPI_STATE_SDCARD);
	if (sdslot_status_cb)
		sdslot_status_cb(sdslot_cd, sdslot_mmc_dev);
}

/*
 *Headset Support
*/
static void hpin_debounce_do_work(struct work_struct *work)
{
	uint16_t gpi_status = 0;
	struct microp_i2c_client_data *cdata;
	int insert = 0;
	struct i2c_client *client;

	client = private_microp_client;
	cdata = i2c_get_clientdata(client);

	microp_read_gpi_status(client, &gpi_status);
	insert = (gpi_status & READ_GPI_STATE_HPIN) ? 0 : 1;
	if (insert != cdata->headset_is_in) {
		cdata->headset_is_in = insert;
		pr_debug("headset %s\n", insert ? "inserted" : "removed");
		htc_35mm_jack_plug_event(cdata->headset_is_in,
					 &cdata->is_hpin_pin_stable);
	}
}

static int microp_enable_headset_plug_event(void)
{
	int ret;
	struct i2c_client *client;
	struct microp_i2c_client_data *cdata;
	uint16_t stat;

	client = private_microp_client;
	cdata = i2c_get_clientdata(client);

	/* enable microp interrupt to detect changes */
	ret = microp_interrupt_enable(client, IRQ_HEADSETIN);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to enable irqs\n",
			__func__);
		return 0;
	}
	/* see if headset state has changed */
	microp_read_gpi_status(client, &stat);
	stat = !(stat & READ_GPI_STATE_HPIN);
	if(cdata->headset_is_in != stat) {
		cdata->headset_is_in = stat;
		pr_debug("Headset state changed\n");
		htc_35mm_jack_plug_event(stat, &cdata->is_hpin_pin_stable);
	}

	return 1;
}

static int microp_headset_detect_mic(void)
{
	uint16_t data;

	microp_read_adc(MICROP_REMOTE_KEY_ADC_CHAN, &data);
	if (data >= 200)
		return 1;
	else
		return 0;
}

static int microp_headset_has_mic(void)
{
	int mic1 = -1;
	int mic2 = -1;
	int count = 0;

	mic2 = microp_headset_detect_mic();

	/* debounce the detection wait until 2 consecutive read are equal */
	while ((mic1 != mic2) && (count < 10)) {
		mic1 = mic2;
		msleep(600);
		mic2 = microp_headset_detect_mic();
		count++;
	}

	pr_info("%s: microphone (%d) %s\n", __func__, count,
		mic1 ? "present" : "not present");

	return mic1;
}

static int microp_enable_key_event(void)
{
	int ret;
	struct i2c_client *client;

	client = private_microp_client;

	if (!is_cdma_version(system_rev))
		gpio_set_value(MAHIMAHI_GPIO_35MM_KEY_INT_SHUTDOWN, 1);

	/* turn on  key interrupt */
	/* enable microp interrupt to detect changes */
	ret = microp_interrupt_enable(client, IRQ_REMOTEKEY);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to enable irqs\n",
			__func__);
		return ret;
	}
	return 0;
}

static int microp_disable_key_event(void)
{
	int ret;
	struct i2c_client *client;

	client = private_microp_client;

	/* shutdown key interrupt */
	if (!is_cdma_version(system_rev))
		gpio_set_value(MAHIMAHI_GPIO_35MM_KEY_INT_SHUTDOWN, 0);

	/* disable microp interrupt to detect changes */
	ret = microp_interrupt_disable(client, IRQ_REMOTEKEY);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to disable irqs\n",
			__func__);
		return ret;
	}
	return 0;
}

static int get_remote_keycode(int *keycode)
{
	struct i2c_client *client = private_microp_client;
	int ret;
	uint8_t data[2];

	ret = i2c_read_block(client, MICROP_I2C_RCMD_REMOTE_KEYCODE, data, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: read remote keycode fail\n",
			 __func__);
		return -EIO;
	}
	pr_debug("%s: key = 0x%x\n", __func__, data[1]);
	if (!data[1]) {
		*keycode = 0;
		return 1;		/* no keycode */
	} else {
		*keycode = data[1];
	}
	return 0;
}

static ssize_t microp_i2c_remotekey_adc_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct i2c_client *client;
	uint16_t value;
	int i, button = 0;
	int ret;

	client = to_i2c_client(dev);

	microp_read_adc(MICROP_REMOTE_KEY_ADC_CHAN, &value);

	for (i = 0; i < 3; i++) {
		if ((value >= remote_key_adc_table[2 * i]) &&
		    (value <= remote_key_adc_table[2 * i + 1])) {
			button = i + 1;
		}

	}

	ret = sprintf(buf, "Remote Key[0x%03X] => button %d\n",
		      value, button);

	return ret;
}

static DEVICE_ATTR(key_adc, 0644, microp_i2c_remotekey_adc_show, NULL);

/*
 * LED support
*/
static int microp_i2c_write_led_mode(struct i2c_client *client,
				struct led_classdev *led_cdev,
				uint8_t mode, uint16_t off_timer)
{
	struct microp_i2c_client_data *cdata;
	struct microp_led_data *ldata;
	uint8_t data[7];
	int ret;

	cdata = i2c_get_clientdata(client);
	ldata = container_of(led_cdev, struct microp_led_data, ldev);


	if (ldata->type == GREEN_LED) {
		data[0] = 0x01;
		data[1] = mode;
		data[2] = off_timer >> 8;
		data[3] = off_timer & 0xFF;
		data[4] = 0x00;
		data[5] = 0x00;
		data[6] = 0x00;
	} else if (ldata->type == AMBER_LED) {
		data[0] = 0x02;
		data[1] = 0x00;
		data[2] = 0x00;
		data[3] = 0x00;
		data[4] = mode;
		data[5] = off_timer >> 8;
		data[6] = off_timer & 0xFF;
	} else if (ldata->type == RED_LED) {
		data[0] = 0x02;
		data[1] = 0x00;
		data[2] = 0x00;
		data[3] = 0x00;
		data[4] = mode? 5: 0;
		data[5] = off_timer >> 8;
		data[6] = off_timer & 0xFF;
	} else if (ldata->type == BLUE_LED) {
		data[0] = 0x04;
		data[1] = mode;
		data[2] = off_timer >> 8;
		data[3] = off_timer & 0xFF;
		data[4] = 0x00;
		data[5] = 0x00;
		data[6] = 0x00;
	}

	ret = i2c_write_block(client, MICROP_I2C_WCMD_LED_MODE, data, 7);
	if (ret == 0) {
		mutex_lock(&ldata->led_data_mutex);
		if (mode > 1)
			ldata->blink = mode;
		else
			ldata->mode = mode;
		mutex_unlock(&ldata->led_data_mutex);
	}
	return ret;
}

static ssize_t microp_i2c_led_blink_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev;
	struct microp_led_data *ldata;
	int ret;

	led_cdev = (struct led_classdev *)dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct microp_led_data, ldev);

	mutex_lock(&ldata->led_data_mutex);
	ret = sprintf(buf, "%d\n", ldata->blink ? ldata->blink - 1 : 0);
	mutex_unlock(&ldata->led_data_mutex);

	return ret;
}

static ssize_t microp_i2c_led_blink_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct led_classdev *led_cdev;
	struct microp_led_data *ldata;
	struct i2c_client *client;
	int val, ret;
	uint8_t mode;

	val = -1;
	sscanf(buf, "%u", &val);

	led_cdev = (struct led_classdev *)dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct microp_led_data, ldev);
	client = to_i2c_client(dev->parent);

	mutex_lock(&ldata->led_data_mutex);
	switch (val) {
	case 0: /* stop flashing */
		mode = ldata->mode;
		ldata->blink = 0;
		break;
	case 1:
	case 2:
	case 3:
		mode = val + 1;
		break;

	default:
		mutex_unlock(&ldata->led_data_mutex);
		return -EINVAL;
	}
	mutex_unlock(&ldata->led_data_mutex);

	ret = microp_i2c_write_led_mode(client, led_cdev, mode, 0xffff);
	if (ret)
		dev_err(&client->dev, "%s set blink failed\n", led_cdev->name);

	return count;
}

static DEVICE_ATTR(blink, 0644, microp_i2c_led_blink_show,
				microp_i2c_led_blink_store);

static ssize_t microp_i2c_led_off_timer_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct microp_i2c_client_data *cdata;
	struct led_classdev *led_cdev;
	struct microp_led_data *ldata;
	struct i2c_client *client;
	uint8_t data[2];
	int ret, offtime;


	led_cdev = (struct led_classdev *)dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct microp_led_data, ldev);
	client = to_i2c_client(dev->parent);
	cdata = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "Getting %s remaining time\n", led_cdev->name);

	if (ldata->type == GREEN_LED) {
		ret = i2c_read_block(client,
				MICROP_I2C_RCMD_GREEN_LED_REMAIN_TIME, data, 2);
	} else if (ldata->type == AMBER_LED) {
		ret = i2c_read_block(client,
				MICROP_I2C_RCMD_AMBER_RED_LED_REMAIN_TIME,
				data, 2);
	} else if (ldata->type == RED_LED) {
		ret = i2c_read_block(client,
				MICROP_I2C_RCMD_AMBER_RED_LED_REMAIN_TIME,
				data, 2);
	} else if (ldata->type == BLUE_LED) {
		ret = i2c_read_block(client,
				MICROP_I2C_RCMD_BLUE_LED_REMAIN_TIME, data, 2);
	} else {
		dev_err(&client->dev, "Unknown led %s\n", ldata->ldev.name);
		return -EINVAL;
	}

	if (ret) {
		dev_err(&client->dev,
			"%s get off_timer failed\n", led_cdev->name);
	}
	offtime = (int)((data[1] | data[0] << 8) * 2);

	ret = sprintf(buf, "Time remains %d:%d\n", offtime / 60, offtime % 60);
	return ret;
}

static ssize_t microp_i2c_led_off_timer_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct led_classdev *led_cdev;
	struct microp_led_data *ldata;
	struct i2c_client *client;
	int min, sec, ret;
	uint16_t off_timer;

	min = -1;
	sec = -1;
	sscanf(buf, "%d %d", &min, &sec);

	if (min < 0 || min > 255)
		return -EINVAL;
	if (sec < 0 || sec > 255)
		return -EINVAL;

	led_cdev = (struct led_classdev *)dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct microp_led_data, ldev);
	client = to_i2c_client(dev->parent);

	dev_dbg(&client->dev, "Setting %s off_timer to %d min %d sec\n",
			led_cdev->name, min, sec);

	if (!min && !sec)
		off_timer = 0xFFFF;
	else
		off_timer = (min * 60 + sec) / 2;

	ret = microp_i2c_write_led_mode(client, led_cdev,
					ldata->mode, off_timer);
	if (ret) {
		dev_err(&client->dev,
			"%s set off_timer %d min %d sec failed\n",
			led_cdev->name, min, sec);
	}
	return count;
}

static DEVICE_ATTR(off_timer, 0644, microp_i2c_led_off_timer_show,
			microp_i2c_led_off_timer_store);

static ssize_t microp_i2c_jogball_color_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct led_classdev *led_cdev;
	struct microp_led_data *ldata;
	struct i2c_client *client;
	int rpwm, gpwm, bpwm, ret;
	uint8_t data[4];

	rpwm = -1;
	gpwm = -1;
	bpwm = -1;
	sscanf(buf, "%d %d %d", &rpwm, &gpwm, &bpwm);

	if (rpwm < 0 || rpwm > 255)
		return -EINVAL;
	if (gpwm < 0 || gpwm > 255)
		return -EINVAL;
	if (bpwm < 0 || bpwm > 255)
		return -EINVAL;

	led_cdev = (struct led_classdev *)dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct microp_led_data, ldev);
	client = to_i2c_client(dev->parent);

	dev_dbg(&client->dev, "Setting %s color to R=%d, G=%d, B=%d\n",
			led_cdev->name, rpwm, gpwm, bpwm);

	data[0] = rpwm;
	data[1] = gpwm;
	data[2] = bpwm;
	data[3] = 0x00;

	ret = i2c_write_block(client, MICROP_I2C_WCMD_JOGBALL_LED_PWM_SET,
			      data, 4);
	if (ret) {
		dev_err(&client->dev,
			"%s set color R=%d G=%d B=%d failed\n",
			led_cdev->name, rpwm, gpwm, bpwm);
	}
	return count;
}

static DEVICE_ATTR(color, 0644, NULL, microp_i2c_jogball_color_store);

static ssize_t microp_i2c_jogball_period_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct led_classdev *led_cdev;
	struct microp_led_data *ldata;
	struct i2c_client *client;
	int period = -1;
	int ret;
	uint8_t data[4];

	sscanf(buf, "%d", &period);

	if (period < 2 || period > 12)
		return -EINVAL;

	led_cdev = (struct led_classdev *)dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct microp_led_data, ldev);
	client = to_i2c_client(dev->parent);

	dev_info(&client->dev, "Setting Jogball flash period to %d\n", period);

	data[0] = 0x00;
	data[1] = period;

	ret = i2c_write_block(client, MICROP_I2C_WCMD_JOGBALL_LED_PERIOD_SET,
			      data, 2);
	if (ret) {
		dev_err(&client->dev, "%s set period=%d failed\n",
			led_cdev->name, period);
	}
	return count;
}

static DEVICE_ATTR(period, 0644, NULL, microp_i2c_jogball_period_store);

static void microp_brightness_set(struct led_classdev *led_cdev,
			       enum led_brightness brightness)
{
	unsigned long flags;
	struct i2c_client *client = to_i2c_client(led_cdev->dev->parent);
	struct microp_led_data *ldata =
		container_of(led_cdev, struct microp_led_data, ldev);

	dev_dbg(&client->dev, "Setting %s brightness current %d new %d\n",
			led_cdev->name, led_cdev->brightness, brightness);

	if (brightness > 255)
		brightness = 255;
	led_cdev->brightness = brightness;

	spin_lock_irqsave(&ldata->brightness_lock, flags);
	ldata->brightness = brightness;
	spin_unlock_irqrestore(&ldata->brightness_lock, flags);

	schedule_work(&ldata->brightness_work);
}

static void microp_led_brightness_set_work(struct work_struct *work)
{
	unsigned long flags;
	struct microp_led_data *ldata =
		container_of(work, struct microp_led_data, brightness_work);
	struct led_classdev *led_cdev = &ldata->ldev;

	struct i2c_client *client = to_i2c_client(led_cdev->dev->parent);

	enum led_brightness brightness;
	int ret;
	uint8_t mode;

	spin_lock_irqsave(&ldata->brightness_lock, flags);
	brightness = ldata->brightness;
	spin_unlock_irqrestore(&ldata->brightness_lock, flags);

	if (brightness)
		mode = 1;
	else
		mode = 0;

	ret = microp_i2c_write_led_mode(client, led_cdev, mode, 0xffff);
	if (ret) {
		dev_err(&client->dev,
			 "led_brightness_set failed to set mode\n");
	}
}

struct device_attribute *green_amber_attrs[] = {
	&dev_attr_blink,
	&dev_attr_off_timer,
};

struct device_attribute *jogball_attrs[] = {
	&dev_attr_color,
	&dev_attr_period,
};

static void microp_led_buttons_brightness_set_work(struct work_struct *work)
{

	unsigned long flags;
	struct microp_led_data *ldata =
		container_of(work, struct microp_led_data, brightness_work);
	struct led_classdev *led_cdev = &ldata->ldev;

	struct i2c_client *client = to_i2c_client(led_cdev->dev->parent);
	struct microp_i2c_client_data *cdata = i2c_get_clientdata(client);


	uint8_t data[4] = {0, 0, 0};
	int ret = 0;
	enum led_brightness brightness;
	uint8_t value;


	spin_lock_irqsave(&ldata->brightness_lock, flags);
	brightness = ldata->brightness;
	spin_unlock_irqrestore(&ldata->brightness_lock, flags);

	value = brightness >= 255 ? 0x20 : 0;

	/* avoid a flicker that can occur when writing the same value */
	if (cdata->button_led_value == value)
		return;
	cdata->button_led_value = value;

	/* in 40ms */
	data[0] = 0x05;
	/* duty cycle 0-255 */
	data[1] = value;
	/* bit2 == change brightness */
	data[3] = 0x04;

	ret = i2c_write_block(client, MICROP_I2C_WCMD_BUTTONS_LED_CTRL,
			      data, 4);
	if (ret < 0)
		dev_err(&client->dev, "%s failed on set buttons\n", __func__);
}

static void microp_led_jogball_brightness_set_work(struct work_struct *work)
{
	unsigned long flags;
	struct microp_led_data *ldata =
		container_of(work, struct microp_led_data, brightness_work);
	struct led_classdev *led_cdev = &ldata->ldev;

	struct i2c_client *client = to_i2c_client(led_cdev->dev->parent);
	uint8_t data[3] = {0, 0, 0};
	int ret = 0;
	enum led_brightness brightness;

	spin_lock_irqsave(&ldata->brightness_lock, flags);
	brightness = ldata->brightness;
	spin_unlock_irqrestore(&ldata->brightness_lock, flags);

	switch (brightness) {
	case 0:
		data[0] = 0;
		break;
	case 3:
		data[0] = 1;
		data[1] = data[2] = 0xFF;
		break;
	case 7:
		data[0] = 2;
		data[1] = 0;
		data[2] = 60;
		break;
	default:
		dev_warn(&client->dev, "%s: unknown value: %d\n",
			__func__, brightness);
		break;
	}
	ret = i2c_write_block(client, MICROP_I2C_WCMD_JOGBALL_LED_MODE,
			      data, 3);
	if (ret < 0)
		dev_err(&client->dev, "%s failed on set jogball mode:0x%2.2X\n",
				__func__, data[0]);
}

/*
 * Light Sensor Support
 */
static int microp_i2c_auto_backlight_mode(struct i2c_client *client,
					    uint8_t enabled)
{
	uint8_t data[2];
	int ret = 0;

	data[0] = 0;
	if (enabled)
		data[1] = 1;
	else
		data[1] = 0;

	ret = i2c_write_block(client, MICROP_I2C_WCMD_AUTO_BL_CTL, data, 2);
	if (ret != 0)
		pr_err("%s: set auto light sensor fail\n", __func__);

	return ret;
}

static int lightsensor_enable(void)
{
	struct i2c_client *client;
	struct microp_i2c_client_data *cdata;
	int ret;

	client = private_microp_client;
	cdata = i2c_get_clientdata(client);

	if (cdata->microp_is_suspend) {
		pr_err("%s: abort, uP is going to suspend after #\n",
		       __func__);
		return -EIO;
	}

	disable_irq(client->irq);
	ret = microp_i2c_auto_backlight_mode(client, 1);
	if (ret < 0) {
		pr_err("%s: set auto light sensor fail\n", __func__);
		enable_irq(client->irq);
		return ret;
	}

	cdata->auto_backlight_enabled = 1;
	/* TEMPORARY HACK: schedule a deferred light sensor read
	 * to work around sensor manager race condition
	 */
	schedule_delayed_work(&cdata->ls_read_work, LS_READ_DELAY);
	schedule_work(&cdata->work.work);

	return 0;
}

static int lightsensor_disable(void)
{
	/* update trigger data when done */
	struct i2c_client *client;
	struct microp_i2c_client_data *cdata;
	int ret;

	client = private_microp_client;
	cdata = i2c_get_clientdata(client);

	if (cdata->microp_is_suspend) {
		pr_err("%s: abort, uP is going to suspend after #\n",
		       __func__);
		return -EIO;
	}

	cancel_delayed_work(&cdata->ls_read_work);

	ret = microp_i2c_auto_backlight_mode(client, 0);
	if (ret < 0)
		pr_err("%s: disable auto light sensor fail\n",
		       __func__);
	else
		cdata->auto_backlight_enabled = 0;
	return 0;
}

static int microp_lightsensor_read(uint16_t *adc_value,
					  uint8_t *adc_level)
{
	struct i2c_client *client;
	struct microp_i2c_client_data *cdata;
	uint8_t i;
	int ret;

	client = private_microp_client;
	cdata = i2c_get_clientdata(client);

	ret = microp_read_adc(MICROP_LSENSOR_ADC_CHAN, adc_value);
	if (ret != 0)
		return -1;

	if (*adc_value > 0x3FF) {
		pr_warning("%s: get wrong value: 0x%X\n",
			__func__, *adc_value);
		return -1;
	} else {
		if (!cdata->als_calibrating) {
			*adc_value = *adc_value
				* cdata->als_gadc / cdata->als_kadc;
			if (*adc_value > 0x3FF)
				*adc_value = 0x3FF;
		}

		*adc_level = ARRAY_SIZE(lsensor_adc_table) - 1;
		for (i = 0; i < ARRAY_SIZE(lsensor_adc_table); i++) {
			if (*adc_value <= lsensor_adc_table[i]) {
				*adc_level = i;
				break;
			}
		}
		pr_debug("%s: ADC value: 0x%X, level: %d #\n",
				__func__, *adc_value, *adc_level);
	}

	return 0;
}

static ssize_t microp_i2c_lightsensor_adc_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	uint8_t adc_level = 0;
	uint16_t adc_value = 0;
	int ret;

	ret = microp_lightsensor_read(&adc_value, &adc_level);

	ret = sprintf(buf, "ADC[0x%03X] => level %d\n", adc_value, adc_level);

	return ret;
}

static DEVICE_ATTR(ls_adc, 0644, microp_i2c_lightsensor_adc_show, NULL);

static ssize_t microp_i2c_ls_auto_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct i2c_client *client;
	uint8_t data[2] = {0, 0};
	int ret;

	client = to_i2c_client(dev);

	i2c_read_block(client, MICROP_I2C_RCMD_SPI_BL_STATUS, data, 2);
	ret = sprintf(buf, "Light sensor Auto = %d, SPI enable = %d\n",
			data[0], data[1]);

	return ret;
}

static ssize_t microp_i2c_ls_auto_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct i2c_client *client;
	struct microp_i2c_client_data *cdata;
	uint8_t enable = 0;
	int ls_auto;

	ls_auto = -1;
	sscanf(buf, "%d", &ls_auto);

	if (ls_auto != 0 && ls_auto != 1 && ls_auto != ALS_CALIBRATE_MODE)
		return -EINVAL;

	client = to_i2c_client(dev);
	cdata = i2c_get_clientdata(client);

	if (ls_auto) {
		enable = 1;
		cdata->als_calibrating = (ls_auto == ALS_CALIBRATE_MODE) ? 1 : 0;
		cdata->auto_backlight_enabled = 1;
	} else {
		enable = 0;
		cdata->als_calibrating = 0;
		cdata->auto_backlight_enabled = 0;
	}

	microp_i2c_auto_backlight_mode(client, enable);

	return count;
}

static DEVICE_ATTR(ls_auto, 0644,  microp_i2c_ls_auto_show,
			microp_i2c_ls_auto_store);

DEFINE_MUTEX(api_lock);
static int lightsensor_opened;

static int lightsensor_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	pr_debug("%s\n", __func__);
	mutex_lock(&api_lock);
	if (lightsensor_opened) {
		pr_err("%s: already opened\n", __func__);
		rc = -EBUSY;
	}
	lightsensor_opened = 1;
	mutex_unlock(&api_lock);
	return rc;
}

static int lightsensor_release(struct inode *inode, struct file *file)
{
	pr_debug("%s\n", __func__);
	mutex_lock(&api_lock);
	lightsensor_opened = 0;
	mutex_unlock(&api_lock);
	return 0;
}

static long lightsensor_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int rc, val;
	struct i2c_client *client;
	struct microp_i2c_client_data *cdata;

	mutex_lock(&api_lock);

	client = private_microp_client;
	cdata = i2c_get_clientdata(client);

	pr_debug("%s cmd %d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
	case LIGHTSENSOR_IOCTL_ENABLE:
		if (get_user(val, (unsigned long __user *)arg)) {
			rc = -EFAULT;
			break;
		}
		rc = val ? lightsensor_enable() : lightsensor_disable();
		break;
	case LIGHTSENSOR_IOCTL_GET_ENABLED:
		val = cdata->auto_backlight_enabled;
		pr_debug("%s enabled %d\n", __func__, val);
		rc = put_user(val, (unsigned long __user *)arg);
		break;
	default:
		pr_err("%s: invalid cmd %d\n", __func__, _IOC_NR(cmd));
		rc = -EINVAL;
	}

	mutex_unlock(&api_lock);
	return rc;
}

static struct file_operations lightsensor_fops = {
	.owner = THIS_MODULE,
	.open = lightsensor_open,
	.release = lightsensor_release,
	.unlocked_ioctl = lightsensor_ioctl
};

struct miscdevice lightsensor_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "lightsensor",
	.fops = &lightsensor_fops
};

/*
 * G-sensor
 */
static int microp_spi_enable(uint8_t on)
{
	struct i2c_client *client;
	int ret;

	client = private_microp_client;
	ret = i2c_write_block(client, MICROP_I2C_WCMD_SPI_EN, &on, 1);
	if (ret < 0) {
		dev_err(&client->dev,"%s: i2c_write_block fail\n", __func__);
		return ret;
	}
	msleep(10);
	return ret;
}

static int gsensor_read_reg(uint8_t reg, uint8_t *data)
{
	struct i2c_client *client;
	int ret;
	uint8_t tmp[2];

	client = private_microp_client;
	ret = i2c_write_block(client, MICROP_I2C_WCMD_GSENSOR_REG_DATA_REQ,
			      &reg, 1);
	if (ret < 0) {
		dev_err(&client->dev,"%s: i2c_write_block fail\n", __func__);
		return ret;
	}
	msleep(10);

	ret = i2c_read_block(client, MICROP_I2C_RCMD_GSENSOR_REG_DATA, tmp, 2);
	if (ret < 0) {
		dev_err(&client->dev,"%s: i2c_read_block fail\n", __func__);
		return ret;
	}
	*data = tmp[1];
	return ret;
}

static int gsensor_write_reg(uint8_t reg, uint8_t data)
{
	struct i2c_client *client;
	int ret;
	uint8_t tmp[2];

	client = private_microp_client;

	tmp[0] = reg;
	tmp[1] = data;
	ret = i2c_write_block(client, MICROP_I2C_WCMD_GSENSOR_REG, tmp, 2);
	if (ret < 0) {
		dev_err(&client->dev,"%s: i2c_write_block fail\n", __func__);
		return ret;
	}

	return ret;
}

static int gsensor_read_acceleration(short *buf)
{
	struct i2c_client *client;
	int ret;
	uint8_t tmp[6];
	struct microp_i2c_client_data *cdata;

	client = private_microp_client;

	cdata = i2c_get_clientdata(client);

	tmp[0] = 1;
	ret = i2c_write_block(client, MICROP_I2C_WCMD_GSENSOR_DATA_REQ,
			      tmp, 1);
	if (ret < 0) {
		dev_err(&client->dev,"%s: i2c_write_block fail\n", __func__);
		return ret;
	}

	msleep(10);

	if (cdata->version <= 0x615) {
		/*
		 * Note the data is a 10bit signed value from the chip.
		*/
		ret = i2c_read_block(client, MICROP_I2C_RCMD_GSENSOR_X_DATA,
				     tmp, 2);
		if (ret < 0) {
			dev_err(&client->dev, "%s: i2c_read_block fail\n",
				__func__);
			return ret;
		}
		buf[0] = (short)(tmp[0] << 8 | tmp[1]);
		buf[0] >>= 6;

		ret = i2c_read_block(client, MICROP_I2C_RCMD_GSENSOR_Y_DATA,
				     tmp, 2);
		if (ret < 0) {
			dev_err(&client->dev, "%s: i2c_read_block fail\n",
				__func__);
			return ret;
		}
		buf[1] = (short)(tmp[0] << 8 | tmp[1]);
		buf[1] >>= 6;

		ret = i2c_read_block(client, MICROP_I2C_RCMD_GSENSOR_Z_DATA,
				     tmp, 2);
		if (ret < 0) {
			dev_err(&client->dev, "%s: i2c_read_block fail\n",
				__func__);
			return ret;
		}
		buf[2] = (short)(tmp[0] << 8 | tmp[1]);
		buf[2] >>= 6;
	} else {
		ret = i2c_read_block(client, MICROP_I2C_RCMD_GSENSOR_DATA,
				     tmp, 6);
		if (ret < 0) {
			dev_err(&client->dev, "%s: i2c_read_block fail\n",
				__func__);
			return ret;
		}
		buf[0] = (short)(tmp[0] << 8 | tmp[1]);
		buf[0] >>= 6;
		buf[1] = (short)(tmp[2] << 8 | tmp[3]);
		buf[1] >>= 6;
		buf[2] = (short)(tmp[4] << 8 | tmp[5]);
		buf[2] >>= 6;
	}

#ifdef DEBUG_BMA150
	/* Log this to debugfs */
	gsensor_log_status(ktime_get(), buf[0], buf[1], buf[2]);
#endif
	return 1;
}

static int gsensor_init_hw(void)
{
	uint8_t reg;
	int ret;

	pr_debug("%s\n", __func__);

	microp_spi_enable(1);

	ret = gsensor_read_reg(RANGE_BWIDTH_REG, &reg);
	if (ret < 0 )
		return -EIO;
	reg &= 0xe0;
	ret = gsensor_write_reg(RANGE_BWIDTH_REG, reg);
	if (ret < 0 )
		return -EIO;

	ret = gsensor_read_reg(SMB150_CONF2_REG, &reg);
	if (ret < 0 )
		return -EIO;
	reg |= (1 << 3);
	ret = gsensor_write_reg(SMB150_CONF2_REG, reg);

	return ret;
}

static int bma150_set_mode(char mode)
{
	uint8_t reg;
	int ret;

	pr_debug("%s mode = %d\n", __func__, mode);
	if (mode == BMA_MODE_NORMAL)
		microp_spi_enable(1);


	ret = gsensor_read_reg(SMB150_CTRL_REG, &reg);
	if (ret < 0 )
		return -EIO;
	reg = (reg & 0xfe) | mode;
	ret = gsensor_write_reg(SMB150_CTRL_REG, reg);

	if (mode == BMA_MODE_SLEEP)
		microp_spi_enable(0);

	return ret;
}
static int gsensor_read(uint8_t *data)
{
	int ret;
	uint8_t reg = data[0];

	ret = gsensor_read_reg(reg, &data[1]);
	pr_debug("%s reg = %x data = %x\n", __func__, reg, data[1]);
	return ret;
}

static int gsensor_write(uint8_t *data)
{
	int ret;
	uint8_t reg = data[0];

	pr_debug("%s reg = %x data = %x\n", __func__, reg, data[1]);
	ret = gsensor_write_reg(reg, data[1]);
	return ret;
}

static int bma150_open(struct inode *inode, struct file *file)
{
	pr_debug("%s\n", __func__);
	return nonseekable_open(inode, file);
}

static int bma150_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int bma150_ioctl(struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	char rwbuf[8];
	int ret = -1;
	short buf[8], temp;

	switch (cmd) {
	case BMA_IOCTL_READ:
	case BMA_IOCTL_WRITE:
	case BMA_IOCTL_SET_MODE:
		if (copy_from_user(&rwbuf, argp, sizeof(rwbuf)))
			return -EFAULT;
		break;
	case BMA_IOCTL_READ_ACCELERATION:
		if (copy_from_user(&buf, argp, sizeof(buf)))
			return -EFAULT;
		break;
	default:
		break;
	}

	switch (cmd) {
	case BMA_IOCTL_INIT:
		ret = gsensor_init_hw();
		if (ret < 0)
			return ret;
		break;

	case BMA_IOCTL_READ:
		if (rwbuf[0] < 1)
			return -EINVAL;
		ret = gsensor_read(rwbuf);
		if (ret < 0)
			return ret;
		break;
	case BMA_IOCTL_WRITE:
		if (rwbuf[0] < 2)
			return -EINVAL;
		ret = gsensor_write(rwbuf);
		if (ret < 0)
			return ret;
		break;
	case BMA_IOCTL_READ_ACCELERATION:
		ret = gsensor_read_acceleration(&buf[0]);
		if (ret < 0)
			return ret;
		break;
	case BMA_IOCTL_SET_MODE:
		bma150_set_mode(rwbuf[0]);
		break;
	case BMA_IOCTL_GET_INT:
		temp = 0;
		break;
	default:
		return -ENOTTY;
	}

	switch (cmd) {
	case BMA_IOCTL_READ:
		if (copy_to_user(argp, &rwbuf, sizeof(rwbuf)))
			return -EFAULT;
		break;
	case BMA_IOCTL_READ_ACCELERATION:
		if (copy_to_user(argp, &buf, sizeof(buf)))
			return -EFAULT;
		break;
	case BMA_IOCTL_GET_INT:
		if (copy_to_user(argp, &temp, sizeof(temp)))
			return -EFAULT;
		break;
	default:
		break;
	}

	return 0;
}

static struct file_operations bma_fops = {
	.owner = THIS_MODULE,
	.open = bma150_open,
	.release = bma150_release,
	.ioctl = bma150_ioctl,
};

static struct miscdevice spi_bma_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = BMA150_G_SENSOR_NAME,
	.fops = &bma_fops,
};

/*
 * Interrupt
 */
static irqreturn_t microp_i2c_intr_irq_handler(int irq, void *dev_id)
{
	struct i2c_client *client;
	struct microp_i2c_client_data *cdata;

	client = to_i2c_client(dev_id);
	cdata = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "intr_irq_handler\n");

	disable_irq_nosync(client->irq);
	schedule_work(&cdata->work.work);
	return IRQ_HANDLED;
}

static void microp_i2c_intr_work_func(struct work_struct *work)
{
	struct microp_i2c_work *up_work;
	struct i2c_client *client;
	struct microp_i2c_client_data *cdata;
	uint8_t data[3], adc_level;
	uint16_t intr_status = 0, adc_value, gpi_status = 0;
	int keycode = 0, ret = 0;

	up_work = container_of(work, struct microp_i2c_work, work);
	client = up_work->client;
	cdata = i2c_get_clientdata(client);

	ret = i2c_read_block(client, MICROP_I2C_RCMD_GPI_INT_STATUS, data, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: read interrupt status fail\n",
			 __func__);
	}

	intr_status = data[0]<<8 | data[1];
	ret = i2c_write_block(client, MICROP_I2C_WCMD_GPI_INT_STATUS_CLR,
			      data, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: clear interrupt status fail\n",
			 __func__);
	}
	pr_debug("intr_status=0x%02x\n", intr_status);

	if ((intr_status & IRQ_LSENSOR) || cdata->force_light_sensor_read) {
		ret = microp_lightsensor_read(&adc_value, &adc_level);
		if (cdata->force_light_sensor_read) {
			/* report an invalid value first to ensure we trigger an event
			 * when adc_level is zero.
			 */
			input_report_abs(cdata->ls_input_dev, ABS_MISC, -1);
			input_sync(cdata->ls_input_dev);
			cdata->force_light_sensor_read = 0;
		}
		input_report_abs(cdata->ls_input_dev, ABS_MISC, (int)adc_level);
		input_sync(cdata->ls_input_dev);
	}

	if (intr_status & IRQ_SDCARD) {
		microp_read_gpi_status(client, &gpi_status);
		mahimahi_microp_sdslot_update_status(gpi_status);
	}

	if (intr_status & IRQ_HEADSETIN) {
		cdata->is_hpin_pin_stable = 0;
		wake_lock_timeout(&microp_i2c_wakelock, 3*HZ);
		if (!cdata->headset_is_in)
			schedule_delayed_work(&cdata->hpin_debounce_work,
					msecs_to_jiffies(500));
		else
			schedule_delayed_work(&cdata->hpin_debounce_work,
					msecs_to_jiffies(300));
	}
	if (intr_status & IRQ_REMOTEKEY) {
		if ((get_remote_keycode(&keycode) == 0) &&
			(cdata->is_hpin_pin_stable)) {
			htc_35mm_key_event(keycode, &cdata->is_hpin_pin_stable);
		}
	}

	enable_irq(client->irq);
}

static void ls_read_do_work(struct work_struct *work)
{
	struct i2c_client *client = private_microp_client;
	struct microp_i2c_client_data *cdata = i2c_get_clientdata(client);

	/* force a light sensor reading */
	disable_irq(client->irq);
	cdata->force_light_sensor_read = 1;
	schedule_work(&cdata->work.work);
}

static int microp_function_initialize(struct i2c_client *client)
{
	struct microp_i2c_client_data *cdata;
	uint8_t data[20];
	uint16_t stat, interrupts = 0;
	int i;
	int ret;
	struct led_classdev *led_cdev;

	cdata = i2c_get_clientdata(client);

	/* Light Sensor */
	if (als_kadc >> 16 == ALS_CALIBRATED)
		cdata->als_kadc = als_kadc & 0xFFFF;
	else {
		cdata->als_kadc = 0;
		pr_info("%s: no ALS calibrated\n", __func__);
	}

	if (cdata->als_kadc && golden_adc) {
		cdata->als_kadc =
			(cdata->als_kadc > 0 && cdata->als_kadc < 0x400)
			? cdata->als_kadc : golden_adc;
		cdata->als_gadc =
			(golden_adc > 0)
			? golden_adc : cdata->als_kadc;
	} else {
		cdata->als_kadc = 1;
		cdata->als_gadc = 1;
	}
	pr_info("%s: als_kadc=0x%x, als_gadc=0x%x\n",
		__func__, cdata->als_kadc, cdata->als_gadc);

	for (i = 0; i < 10; i++) {
		data[i] = (uint8_t)(lsensor_adc_table[i]
			* cdata->als_kadc / cdata->als_gadc >> 8);
		data[i + 10] = (uint8_t)(lsensor_adc_table[i]
			* cdata->als_kadc / cdata->als_gadc);
	}
	ret = i2c_write_block(client, MICROP_I2C_WCMD_ADC_TABLE, data, 20);
	if (ret)
		goto exit;

	ret = gpio_request(MAHIMAHI_GPIO_LS_EN_N, "microp_i2c");
	if (ret < 0) {
		dev_err(&client->dev, "failed on request gpio ls_on\n");
		goto exit;
	}
	ret = gpio_direction_output(MAHIMAHI_GPIO_LS_EN_N, 0);
	if (ret < 0) {
		dev_err(&client->dev, "failed on gpio_direction_output"
				"ls_on\n");
		goto err_gpio_ls;
	}
	cdata->light_sensor_enabled = 1;

	/* Headset */
	for (i = 0; i < 6; i++) {
		data[i] = (uint8_t)(remote_key_adc_table[i] >> 8);
		data[i + 6] = (uint8_t)(remote_key_adc_table[i]);
	}
	ret = i2c_write_block(client,
		MICROP_I2C_WCMD_REMOTEKEY_TABLE, data, 12);
	if (ret)
		goto exit;

	INIT_DELAYED_WORK(
		&cdata->hpin_debounce_work, hpin_debounce_do_work);
	INIT_DELAYED_WORK(
		&cdata->ls_read_work, ls_read_do_work);

	/* SD Card */
	interrupts |= IRQ_SDCARD;

	/* set LED initial state */
	for (i = 0; i < BLUE_LED; i++) {
		led_cdev = &cdata->leds[i].ldev;
		microp_i2c_write_led_mode(client, led_cdev, 0, 0xffff);
	}

	/* enable the interrupts */
	ret = microp_interrupt_enable(client, interrupts);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to enable gpi irqs\n",
			__func__);
		goto err_irq_en;
	}

	microp_read_gpi_status(client, &stat);
	mahimahi_microp_sdslot_update_status(stat);

	return 0;

err_irq_en:
err_gpio_ls:
	gpio_free(MAHIMAHI_GPIO_LS_EN_N);
exit:
	return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void microp_early_suspend(struct early_suspend *h)
{
	struct microp_i2c_client_data *cdata;
	struct i2c_client *client = private_microp_client;
	int ret;

	if (!client) {
		pr_err("%s: dataset: client is empty\n", __func__);
		return;
	}
	cdata = i2c_get_clientdata(client);

	cdata->microp_is_suspend = 1;

	disable_irq(client->irq);
	ret = cancel_work_sync(&cdata->work.work);
	if (ret != 0) {
		enable_irq(client->irq);
	}

	if (cdata->auto_backlight_enabled)
		microp_i2c_auto_backlight_mode(client, 0);
	if (cdata->light_sensor_enabled == 1) {
		gpio_set_value(MAHIMAHI_GPIO_LS_EN_N, 1);
		cdata->light_sensor_enabled = 0;
	}
}

void microp_early_resume(struct early_suspend *h)
{
	struct i2c_client *client = private_microp_client;
	struct microp_i2c_client_data *cdata;

	if (!client) {
		pr_err("%s: dataset: client is empty\n", __func__);
		return;
	}
	cdata = i2c_get_clientdata(client);

	gpio_set_value(MAHIMAHI_GPIO_LS_EN_N, 0);
	cdata->light_sensor_enabled = 1;

	if (cdata->auto_backlight_enabled)
		microp_i2c_auto_backlight_mode(client, 1);

	cdata->microp_is_suspend = 0;
	enable_irq(client->irq);
}
#endif

static int microp_i2c_suspend(struct i2c_client *client,
	pm_message_t mesg)
{
	return 0;
}

static int microp_i2c_resume(struct i2c_client *client)
{
	return 0;
}

static struct {
	const char *name;
	void (*led_set_work)(struct work_struct *);
	struct device_attribute **attrs;
	int attr_cnt;
} microp_leds[] = {
	[GREEN_LED] = {
		.name		= "green",
		.led_set_work   = microp_led_brightness_set_work,
		.attrs		= green_amber_attrs,
		.attr_cnt	= ARRAY_SIZE(green_amber_attrs)
	},
	[AMBER_LED] = {
		.name		= "amber",
		.led_set_work   = microp_led_brightness_set_work,
		.attrs		= green_amber_attrs,
		.attr_cnt	= ARRAY_SIZE(green_amber_attrs)
	},
	[RED_LED] = {
		.name		= "red",
		.led_set_work   = microp_led_brightness_set_work,
		.attrs		= green_amber_attrs,
		.attr_cnt	= ARRAY_SIZE(green_amber_attrs)
	},
	[BLUE_LED] = {
		.name		= "blue",
		.led_set_work   = microp_led_brightness_set_work,
		.attrs		= green_amber_attrs,
		.attr_cnt	= ARRAY_SIZE(green_amber_attrs)
	},
	[JOGBALL_LED] = {
		.name		= "jogball-backlight",
		.led_set_work	= microp_led_jogball_brightness_set_work,
		.attrs		= jogball_attrs,
		.attr_cnt	= ARRAY_SIZE(jogball_attrs)
	},
	[BUTTONS_LED] = {
		.name		= "button-backlight",
		.led_set_work	= microp_led_buttons_brightness_set_work
	},
};

static int microp_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct microp_i2c_client_data *cdata;
	uint8_t data[6];
	int ret;
	int i;
	int j;

	private_microp_client = client;
	ret = i2c_read_block(client, MICROP_I2C_RCMD_VERSION, data, 2);
	if (ret || !(data[0] && data[1])) {
		ret = -ENODEV;
		dev_err(&client->dev, "failed on get microp version\n");
		goto err_exit;
	}
	dev_info(&client->dev, "microp version [%02X][%02X]\n",
		  data[0], data[1]);

	ret = gpio_request(MAHIMAHI_GPIO_UP_RESET_N, "microp_i2c_wm");
	if (ret < 0) {
		dev_err(&client->dev, "failed on request gpio reset\n");
		goto err_exit;
	}
	ret = gpio_direction_output(MAHIMAHI_GPIO_UP_RESET_N, 1);
	if (ret < 0) {
		dev_err(&client->dev,
			 "failed on gpio_direction_output reset\n");
		goto err_gpio_reset;
	}

	cdata = kzalloc(sizeof(struct microp_i2c_client_data), GFP_KERNEL);
	if (!cdata) {
		ret = -ENOMEM;
		dev_err(&client->dev, "failed on allocat cdata\n");
		goto err_cdata;
	}

	i2c_set_clientdata(client, cdata);
	cdata->version = data[0] << 8 | data[1];
	cdata->microp_is_suspend = 0;
	cdata->auto_backlight_enabled = 0;
	cdata->light_sensor_enabled = 0;

	wake_lock_init(&microp_i2c_wakelock, WAKE_LOCK_SUSPEND,
			 "microp_i2c_present");

	/* Light Sensor */
	ret = device_create_file(&client->dev, &dev_attr_ls_adc);
	ret = device_create_file(&client->dev, &dev_attr_ls_auto);
	cdata->ls_input_dev = input_allocate_device();
	if (!cdata->ls_input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		ret = -ENOMEM;
		goto err_request_input_dev;
	}
	cdata->ls_input_dev->name = "lightsensor-level";
	set_bit(EV_ABS, cdata->ls_input_dev->evbit);
	input_set_abs_params(cdata->ls_input_dev, ABS_MISC, 0, 9, 0, 0);

	ret = input_register_device(cdata->ls_input_dev);
	if (ret < 0) {
		dev_err(&client->dev, "%s: can not register input device\n",
				__func__);
		goto err_register_input_dev;
	}

	ret = misc_register(&lightsensor_misc);
	if (ret < 0) {
		dev_err(&client->dev, "%s: can not register misc device\n",
				__func__);
		goto err_register_misc_register;
	}

	/* LEDs */
	ret = 0;
	for (i = 0; i < ARRAY_SIZE(microp_leds) && !ret; ++i) {
		struct microp_led_data *ldata = &cdata->leds[i];

		ldata->type = i;
		ldata->ldev.name = microp_leds[i].name;
		ldata->ldev.brightness_set = microp_brightness_set;
		mutex_init(&ldata->led_data_mutex);
		INIT_WORK(&ldata->brightness_work, microp_leds[i].led_set_work);
		spin_lock_init(&ldata->brightness_lock);
		ret = led_classdev_register(&client->dev, &ldata->ldev);
		if (ret) {
			ldata->ldev.name = NULL;
			break;
		}

		for (j = 0; j < microp_leds[i].attr_cnt && !ret; ++j)
			ret = device_create_file(ldata->ldev.dev,
						 microp_leds[i].attrs[j]);
	}
	if (ret) {
		dev_err(&client->dev, "failed to add leds\n");
		goto err_add_leds;
	}

	/* Headset */
	cdata->headset_is_in = 0;
	cdata->is_hpin_pin_stable = 1;
	platform_device_register(&mahimahi_h35mm);

	ret = device_create_file(&client->dev, &dev_attr_key_adc);

	/* G-sensor */
	ret = misc_register(&spi_bma_device);
	if (ret < 0) {
		pr_err("%s: init bma150 misc_register fail\n",
				__func__);
		goto err_register_bma150;
	}
#ifdef DEBUG_BMA150
	debugfs_create_file("gsensor_log", 0444, NULL, NULL, &gsensor_log_fops);
#endif
	/* Setup IRQ handler */
	INIT_WORK(&cdata->work.work, microp_i2c_intr_work_func);
	cdata->work.client = client;

	ret = request_irq(client->irq,
			microp_i2c_intr_irq_handler,
			IRQF_TRIGGER_LOW,
			"microp_interrupt",
			&client->dev);
	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_intr;
	}
	ret = set_irq_wake(client->irq, 1);
	if (ret) {
		dev_err(&client->dev, "set_irq_wake failed\n");
		goto err_intr;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (cdata->enable_early_suspend) {
		cdata->early_suspend.level =
				EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
		cdata->early_suspend.suspend = microp_early_suspend;
		cdata->early_suspend.resume = microp_early_resume;
		register_early_suspend(&cdata->early_suspend);
	}
#endif

	ret = microp_function_initialize(client);
	if (ret) {
		dev_err(&client->dev, "failed on microp function initialize\n");
		goto err_fun_init;
	}

	return 0;

err_fun_init:
err_intr:
	misc_deregister(&spi_bma_device);

err_register_bma150:
	platform_device_unregister(&mahimahi_h35mm);
	device_remove_file(&client->dev, &dev_attr_key_adc);

err_add_leds:
	for (i = 0; i < ARRAY_SIZE(microp_leds); ++i) {
		if (!cdata->leds[i].ldev.name)
			continue;
		led_classdev_unregister(&cdata->leds[i].ldev);
		for (j = 0; j < microp_leds[i].attr_cnt; ++j)
			device_remove_file(cdata->leds[i].ldev.dev,
					   microp_leds[i].attrs[j]);
	}

	misc_deregister(&lightsensor_misc);

err_register_misc_register:
	input_unregister_device(cdata->ls_input_dev);

err_register_input_dev:
	input_free_device(cdata->ls_input_dev);

err_request_input_dev:
	wake_lock_destroy(&microp_i2c_wakelock);
	device_remove_file(&client->dev, &dev_attr_ls_adc);
	device_remove_file(&client->dev, &dev_attr_ls_auto);
	kfree(cdata);
	i2c_set_clientdata(client, NULL);

err_cdata:
err_gpio_reset:
	gpio_free(MAHIMAHI_GPIO_UP_RESET_N);
err_exit:
	return ret;
}

static int __devexit microp_i2c_remove(struct i2c_client *client)
{
	struct microp_i2c_client_data *cdata;
	int i;
	int j;

	cdata = i2c_get_clientdata(client);

	for (i = 0; i < ARRAY_SIZE(microp_leds); ++i) {
		struct microp_led_data *ldata = &cdata->leds[i];
		cancel_work_sync(&ldata->brightness_work);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (cdata->enable_early_suspend) {
		unregister_early_suspend(&cdata->early_suspend);
	}
#endif

	for (i = 0; i < ARRAY_SIZE(microp_leds); ++i) {
		if (!cdata->leds[i].ldev.name)
			continue;
		led_classdev_unregister(&cdata->leds[i].ldev);
		for (j = 0; j < microp_leds[i].attr_cnt; ++j)
			device_remove_file(cdata->leds[i].ldev.dev,
					   microp_leds[i].attrs[j]);
	}

	free_irq(client->irq, &client->dev);

	gpio_free(MAHIMAHI_GPIO_UP_RESET_N);

	misc_deregister(&lightsensor_misc);
	input_unregister_device(cdata->ls_input_dev);
	input_free_device(cdata->ls_input_dev);
	device_remove_file(&client->dev, &dev_attr_ls_adc);
	device_remove_file(&client->dev, &dev_attr_key_adc);
	device_remove_file(&client->dev, &dev_attr_ls_auto);

	platform_device_unregister(&mahimahi_h35mm);

	/* G-sensor */
	misc_deregister(&spi_bma_device);

	kfree(cdata);

	return 0;
}

#define ATAG_ALS	0x5441001b
static int __init parse_tag_als_kadc(const struct tag *tags)
{
	int found = 0;
	struct tag *t = (struct tag *)tags;

	for (; t->hdr.size; t = tag_next(t)) {
		if (t->hdr.tag == ATAG_ALS) {
			found = 1;
			break;
		}
	}

	if (found)
		als_kadc = t->u.revision.rev;
	pr_debug("%s: als_kadc = 0x%x\n", __func__, als_kadc);
	return 0;
}
__tagtable(ATAG_ALS, parse_tag_als_kadc);

static const struct i2c_device_id microp_i2c_id[] = {
	{ MICROP_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver microp_i2c_driver = {
	.driver = {
		   .name = MICROP_I2C_NAME,
		   },
	.id_table = microp_i2c_id,
	.probe = microp_i2c_probe,
	.suspend = microp_i2c_suspend,
	.resume = microp_i2c_resume,
	.remove = __devexit_p(microp_i2c_remove),
};


static int __init microp_i2c_init(void)
{
	return i2c_add_driver(&microp_i2c_driver);
}

static void __exit microp_i2c_exit(void)
{
	i2c_del_driver(&microp_i2c_driver);
}

module_init(microp_i2c_init);
module_exit(microp_i2c_exit);

MODULE_AUTHOR("Eric Olsen <eolsen@android.com>");
MODULE_DESCRIPTION("MicroP I2C driver");
MODULE_LICENSE("GPL");
