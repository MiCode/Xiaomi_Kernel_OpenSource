/*
 * Copyright (C) 2010 - 2017 Novatek, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * $Revision: 23175 $
 * $Date: 2018-02-12 16:26:21 +0800 (周一, 12 二月 2018) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/input/mt.h>
#include <linux/wakelock.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#include "nt36xxx.h"
#if NVT_TOUCH_ESD_PROTECT
#include <linux/jiffies.h>
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#ifdef CONFIG_KERNEL_CUSTOM_FACTORY
#include "../lct_tp_work.h"
#endif
#include "../lct_tp_gesture.h"
#include "../lct_tp_grip_area.h"

/* add verify LCD by wanghan start */
extern char g_lcd_id[128];
/* add verify LCD by wanghan end */
/* add touchpad information by wanghan start */
extern int lct_nvt_tp_info_node_init(void);
/* add touchpad information by wanghan end */
/* add resume work by wanghan start */
static struct work_struct g_resume_work;
/* add resume work by wanghan end */

#if NVT_TOUCH_ESD_PROTECT
static struct delayed_work nvt_esd_check_work;
static struct workqueue_struct *nvt_esd_check_wq;
static unsigned long irq_timer = 0;
uint8_t esd_check = false;
uint8_t esd_retry = 0;
uint8_t esd_retry_max = 5;
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if NVT_TOUCH_EXT_PROC
extern int32_t nvt_extra_proc_init(void);
#endif

#if NVT_TOUCH_MP
extern int32_t nvt_mp_proc_init(void);
#endif

struct nvt_ts_data *ts;

//F7A Novatek touch usb pulgin work
struct g_nvt_data {
	bool valid;
	bool usb_plugin;
	struct work_struct nvt_usb_plugin_work;
};
struct g_nvt_data g_nvt = {0};
EXPORT_SYMBOL(g_nvt);

static struct workqueue_struct *nvt_wq;

#if BOOT_UPDATE_FIRMWARE
static struct workqueue_struct *nvt_fwu_wq;
extern void Boot_Update_Firmware(struct work_struct *work);
#endif

#ifdef CONFIG_KERNEL_CUSTOM_FACTORY
static int lct_tp_work_node_callback(bool flag);
#endif
static int lct_tp_gesture_node_callback(bool flag);
static int lct_tp_get_screen_angle_callback(void);
static int lct_tp_set_screen_angle_callback(unsigned int angle);

/* add resume work by wanghan start */
static void do_nvt_ts_resume_work(struct work_struct *work);
/* add resume work by wanghan end */

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void nvt_ts_early_suspend(struct early_suspend *h);
static void nvt_ts_late_resume(struct early_suspend *h);
#endif

#if TOUCH_KEY_NUM > 0
const uint16_t touch_key_array[TOUCH_KEY_NUM] = {
	KEY_BACK,
	KEY_HOME,
	KEY_MENU
};
#endif

#if WAKEUP_GESTURE
const uint16_t gesture_key_array[] = {
	KEY_WAKEUP,  //GESTURE_WORD_C
	KEY_WAKEUP,  //GESTURE_WORD_W
	KEY_WAKEUP,  //GESTURE_WORD_V
	KEY_WAKEUP,  //GESTURE_DOUBLE_CLICK
	KEY_WAKEUP,  //GESTURE_WORD_Z
	KEY_WAKEUP,  //GESTURE_WORD_M
	KEY_WAKEUP,  //GESTURE_WORD_O
	KEY_WAKEUP,  //GESTURE_WORD_e
	KEY_WAKEUP,  //GESTURE_WORD_S
	KEY_WAKEUP,  //GESTURE_SLIDE_UP
	KEY_WAKEUP,  //GESTURE_SLIDE_DOWN
	KEY_WAKEUP,  //GESTURE_SLIDE_LEFT
	KEY_WAKEUP,  //GESTURE_SLIDE_RIGHT
};

bool enable_gesture_mode = false; // for gesture
EXPORT_SYMBOL(enable_gesture_mode);
bool delay_gesture = false;
bool suspend_state = false;
#define WAKEUP_OFF 4
#define WAKEUP_ON 5
#define ENABLE_TOUCH_SZIE 0//disable touch size report

int nvt_gesture_switch(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	LOG_ENTRY();
	if (type == EV_SYN && code == SYN_CONFIG)
	{
		if (suspend_state)
		{
			if ((value != WAKEUP_OFF) || enable_gesture_mode)
			{
				delay_gesture = true;
			}
		}
		NVT_LOG("choose the gesture mode yes or not\n");
		if(value == WAKEUP_OFF){
			NVT_LOG("disable gesture mode\n");
			enable_gesture_mode = false;
		}else if(value == WAKEUP_ON){
			NVT_LOG("enable gesture mode\n");
			enable_gesture_mode  = true;
		}
	}
	LOG_DONE();
	return 0;
}

#endif

static uint8_t bTouchIsAwake = 0;

/*******************************************************
Description:
Novatek touchscreen i2c read function.

return:
Executive outcomes. 2---succeed. -5---I/O error
 *******************************************************/
int32_t CTP_I2C_READ(struct i2c_client *client, uint16_t address, uint8_t *buf, uint16_t len)
{
	struct i2c_msg msgs[2];
	int32_t ret = -1;
	int32_t retries = 0;

	LOG_ENTRY();
	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = address;
	msgs[0].len   = 1;
	msgs[0].buf   = &buf[0];

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = address;
	msgs[1].len   = len - 1;
	msgs[1].buf   = &buf[1];

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)	break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	LOG_DONE();
	return ret;
}

/*******************************************************
Description:
Novatek touchscreen i2c write function.

return:
Executive outcomes. 1---succeed. -5---I/O error
 *******************************************************/
int32_t CTP_I2C_WRITE(struct i2c_client *client, uint16_t address, uint8_t *buf, uint16_t len)
{
	struct i2c_msg msg;
	int32_t ret = -1;
	int32_t retries = 0;

	LOG_ENTRY();
	msg.flags = !I2C_M_RD;
	msg.addr  = address;
	msg.len   = len;
	msg.buf   = buf;

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)	break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	LOG_DONE();
	return ret;
}


/*******************************************************
Description:
Novatek touchscreen reset MCU then into idle mode
function.

return:
n.a.
 *******************************************************/
void nvt_sw_reset_idle(void)
{
	uint8_t buf[4]={0};

	LOG_ENTRY();
	//---write i2c cmds to reset idle---
	buf[0]=0x00;
	buf[1]=0xA5;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);

	msleep(15);
	LOG_DONE();
}

/*******************************************************
Description:
Novatek touchscreen reset MCU (boot) function.

return:
n.a.
 *******************************************************/
void nvt_bootloader_reset(void)
{
	uint8_t buf[8] = {0};

	LOG_ENTRY();
	//---write i2c cmds to reset---
	buf[0] = 0x00;
	buf[1] = 0x69;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);

	// need 35ms delay after bootloader reset
	msleep(35);
	LOG_DONE();
}

/*******************************************************
Description:
Novatek touchscreen clear FW status function.

return:
Executive outcomes. 0---succeed. -1---fail.
 *******************************************************/
int32_t nvt_clear_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 20;

	LOG_ENTRY();
	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		buf[0] = 0xFF;
		buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
		buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

		if (buf[1] == 0x00)
			break;

		msleep(10);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		LOG_DONE();
		return -1;
	} else {
		LOG_DONE();
		return 0;
	}
}

/*******************************************************
Description:
Novatek touchscreen check FW status function.

return:
Executive outcomes. 0---succeed. -1---failed.
 *******************************************************/
int32_t nvt_check_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;

	LOG_ENTRY();
	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		buf[0] = 0xFF;
		buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
		buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		msleep(10);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		LOG_DONE();
		return -1;
	} else {
		LOG_DONE();
		return 0;
	}
}

/*******************************************************
Description:
Novatek touchscreen check FW reset state function.

return:
Executive outcomes. 0---succeed. -1---failed.
 *******************************************************/
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;

	LOG_ENTRY();
	while (1) {
		msleep(10);

		//---read reset state---
		buf[0] = EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 6);

		if ((buf[1] >= check_reset_state) && (buf[1] <= RESET_STATE_MAX)) {
			ret = 0;
			break;
		}

		retry++;
		if(unlikely(retry > 100)) {
			NVT_ERR("error, retry=%d, buf[1]=0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", retry, buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}
	}

	LOG_DONE();
	return ret;
}

/*******************************************************
Description:
Novatek touchscreen get novatek project id information
function.

return:
Executive outcomes. 0---success. -1---fail.
 *******************************************************/
int32_t nvt_read_pid(void)
{
	uint8_t buf[3] = {0};
	int32_t ret = 0;

	LOG_ENTRY();
	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---read project id---
	buf[0] = EVENT_MAP_PROJECTID;
	buf[1] = 0x00;
	buf[2] = 0x00;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 3);

	ts->nvt_pid = (buf[2] << 8) + buf[1];

	NVT_LOG("PID=%04X\n", ts->nvt_pid);

	LOG_DONE();
	return ret;
}

/*******************************************************
Description:
Novatek touchscreen get firmware related information
function.

return:
Executive outcomes. 0---success. -1---fail.
 *******************************************************/
int32_t nvt_get_fw_info(void)
{
	uint8_t buf[64] = {0};
	uint32_t retry_count = 0;
	int32_t ret = 0;

	LOG_ENTRY();
info_retry:
	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---read fw info---
	buf[0] = EVENT_MAP_FWINFO;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 17);
	ts->fw_ver = buf[1];
	ts->x_num = buf[3];
	ts->y_num = buf[4];
	ts->abs_x_max = (uint16_t)((buf[5] << 8) | buf[6]);
	ts->abs_y_max = (uint16_t)((buf[7] << 8) | buf[8]);
	ts->max_button_num = buf[11];

	//---clear x_num, y_num if fw info is broken---
	if ((buf[1] + buf[2]) != 0xFF) {
		NVT_ERR("FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", buf[1], buf[2]);
		ts->fw_ver = 0;
		ts->x_num = 18;
		ts->y_num = 32;
		ts->abs_x_max = TOUCH_DEFAULT_MAX_WIDTH;
		ts->abs_y_max = TOUCH_DEFAULT_MAX_HEIGHT;
		ts->max_button_num = TOUCH_KEY_NUM;

		if(retry_count < 3) {
			retry_count++;
			NVT_ERR("retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			NVT_ERR("Set default fw_ver=%d, x_num=%d, y_num=%d, \
					abs_x_max=%d, abs_y_max=%d, max_button_num=%d!\n",
					ts->fw_ver, ts->x_num, ts->y_num,
					ts->abs_x_max, ts->abs_y_max, ts->max_button_num);
			ret = -1;
		}
	} else {
		ret = 0;
	}
	//---Get Novatek PID---
	nvt_read_pid();

	LOG_DONE();
	return ret;
}

/*******************************************************
  Create Device Node (Proc Entry)
 *******************************************************/
#if NVT_TOUCH_PROC
static struct proc_dir_entry *NVT_proc_entry;
#define DEVICE_NAME	"NVTflash"

/*******************************************************
Description:
Novatek touchscreen /proc/NVTflash read function.

return:
Executive outcomes. 2---succeed. -5,-14---failed.
 *******************************************************/
static ssize_t nvt_flash_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	uint8_t str[68] = {0};
	int32_t ret = -1;
	int32_t retries = 0;
	int8_t i2c_wr = 0;

	LOG_ENTRY();
	if (count > sizeof(str)) {
		NVT_ERR("error count=%zu\n", count);
		return -EFAULT;
	}

	if (copy_from_user(str, buff, count)) {
		NVT_ERR("copy from user error\n");
		return -EFAULT;
	}

#if NVT_TOUCH_ESD_PROTECT
	/*
	 * stop esd check work to avoid case that 0x77 report righ after here to enable esd check again
	 * finally lead to trigger esd recovery bootloader reset
	 */
	cancel_delayed_work_sync(&nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	i2c_wr = str[0] >> 7;

	if (i2c_wr == 0) {	//I2C write
		while (retries < 20) {
			ret = CTP_I2C_WRITE(ts->client, (str[0] & 0x7F), &str[2], str[1]);
			if (ret == 1)
				break;
			else
				NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			return -EIO;
		}

		return ret;
	} else if (i2c_wr == 1) {	//I2C read
		while (retries < 20) {
			ret = CTP_I2C_READ(ts->client, (str[0] & 0x7F), &str[2], str[1]);
			if (ret == 2)
				break;
			else
				NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		// copy buff to user if i2c transfer
		if (retries < 20) {
			if (copy_to_user(buff, str, count))
				return -EFAULT;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			return -EIO;
		}

		return ret;
	} else {
		NVT_ERR("Call error, str[0]=%d\n", str[0]);
		return -EFAULT;
	}
}

/*******************************************************
Description:
Novatek touchscreen /proc/NVTflash open function.

return:
Executive outcomes. 0---succeed. -12---failed.
 *******************************************************/
static int32_t nvt_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev;

	LOG_ENTRY();
	dev = kmalloc(sizeof(struct nvt_flash_data), GFP_KERNEL);
	if (dev == NULL) {
		NVT_ERR("Failed to allocate memory for nvt flash data\n");
		return -ENOMEM;
	}

	rwlock_init(&dev->lock);
	file->private_data = dev;

	LOG_DONE();
	return 0;
}

/*******************************************************
Description:
Novatek touchscreen /proc/NVTflash close function.

return:
Executive outcomes. 0---succeed.
 *******************************************************/
static int32_t nvt_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev = file->private_data;

	LOG_ENTRY();
	if (dev)
		kfree(dev);

	LOG_DONE();
	return 0;
}

static const struct file_operations nvt_flash_fops = {
	.owner = THIS_MODULE,
	.open = nvt_flash_open,
	.release = nvt_flash_close,
	.read = nvt_flash_read,
};

/*******************************************************
Description:
Novatek touchscreen /proc/NVTflash initial function.

return:
Executive outcomes. 0---succeed. -12---failed.
 *******************************************************/
static int32_t nvt_flash_proc_init(void)
{
	LOG_ENTRY();
	NVT_proc_entry = proc_create(DEVICE_NAME, 0444, NULL,&nvt_flash_fops);
	if (NVT_proc_entry == NULL) {
		NVT_ERR("Failed!\n");
		return -ENOMEM;
	} else {
		NVT_LOG("Succeeded!\n");
	}

	NVT_LOG("============================================================\n");
	NVT_LOG("Create /proc/NVTflash\n");
	NVT_LOG("============================================================\n");

	LOG_DONE();
	return 0;
}
#endif

#if WAKEUP_GESTURE
#define GESTURE_WORD_C          12
#define GESTURE_WORD_W          13
#define GESTURE_WORD_V          14
#define GESTURE_DOUBLE_CLICK    15
#define GESTURE_WORD_Z          16
#define GESTURE_WORD_M          17
#define GESTURE_WORD_O          18
#define GESTURE_WORD_e          19
#define GESTURE_WORD_S          20
#define GESTURE_SLIDE_UP        21
#define GESTURE_SLIDE_DOWN      22
#define GESTURE_SLIDE_LEFT      23
#define GESTURE_SLIDE_RIGHT     24
/* customized gesture id */
#define DATA_PROTOCOL           30

/* function page definition */
#define FUNCPAGE_GESTURE         1

static struct wake_lock gestrue_wakelock;

/*******************************************************
Description:
Novatek touchscreen wake up gesture key report function.

return:
n.a.
 *******************************************************/
void nvt_ts_wakeup_gesture_report(uint8_t gesture_id, uint8_t *data)
{
	uint32_t keycode = 0;
	uint8_t func_type = data[2];
	uint8_t func_id = data[3];

	LOG_ENTRY();
	/* support fw specifal data protocol */
	if ((gesture_id == DATA_PROTOCOL) && (func_type == FUNCPAGE_GESTURE)) {
		gesture_id = func_id;
	} else if (gesture_id > DATA_PROTOCOL) {
		NVT_ERR("gesture_id %d is invalid, func_type=%d, func_id=%d\n", gesture_id, func_type, func_id);
		return;
	}

	NVT_LOG("gesture_id = %d\n", gesture_id);

	switch (gesture_id) {
	case GESTURE_WORD_C:
		NVT_LOG("Gesture : Word-C.\n");
		keycode = gesture_key_array[0];
		break;
	case GESTURE_WORD_W:
		NVT_LOG("Gesture : Word-W.\n");
		keycode = gesture_key_array[1];
		break;
	case GESTURE_WORD_V:
		NVT_LOG("Gesture : Word-V.\n");
		keycode = gesture_key_array[2];
		break;
	case GESTURE_DOUBLE_CLICK:
		NVT_LOG("Gesture : Double Click.\n");
		keycode = gesture_key_array[3];
		break;
	case GESTURE_WORD_Z:
		NVT_LOG("Gesture : Word-Z.\n");
		keycode = gesture_key_array[4];
		break;
	case GESTURE_WORD_M:
		NVT_LOG("Gesture : Word-M.\n");
		keycode = gesture_key_array[5];
		break;
	case GESTURE_WORD_O:
		NVT_LOG("Gesture : Word-O.\n");
		keycode = gesture_key_array[6];
		break;
	case GESTURE_WORD_e:
		NVT_LOG("Gesture : Word-e.\n");
		keycode = gesture_key_array[7];
		break;
	case GESTURE_WORD_S:
		NVT_LOG("Gesture : Word-S.\n");
		keycode = gesture_key_array[8];
		break;
	case GESTURE_SLIDE_UP:
		NVT_LOG("Gesture : Slide UP.\n");
		keycode = gesture_key_array[9];
		break;
	case GESTURE_SLIDE_DOWN:
		NVT_LOG("Gesture : Slide DOWN.\n");
		keycode = gesture_key_array[10];
		break;
	case GESTURE_SLIDE_LEFT:
		NVT_LOG("Gesture : Slide LEFT.\n");
		keycode = gesture_key_array[11];
		break;
	case GESTURE_SLIDE_RIGHT:
		NVT_LOG("Gesture : Slide RIGHT.\n");
		keycode = gesture_key_array[12];
		break;
	default:
		break;
	}

	if (keycode > 0) {
		input_report_key(ts->input_dev, keycode, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, keycode, 0);
		input_sync(ts->input_dev);
	}
	LOG_DONE();
}
#endif

/*******************************************************
Description:
Novatek touchscreen parse device tree function.

return:
n.a.
 *******************************************************/
#ifdef CONFIG_OF
static void nvt_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;

	LOG_ENTRY();
#if NVT_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = of_get_named_gpio_flags(np, "novatek,reset-gpio", 0, &ts->reset_flags);
	NVT_LOG("novatek,reset-gpio=%d\n", ts->reset_gpio);
#endif
	ts->irq_gpio = of_get_named_gpio_flags(np, "novatek,irq-gpio", 0, &ts->irq_flags);
	NVT_LOG("novatek,irq-gpio=%d\n", ts->irq_gpio);

	LOG_DONE();
}
#else
static void nvt_parse_dt(struct device *dev)
{
	LOG_ENTRY();
#if NVT_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = NVTTOUCH_RST_PIN;
#endif
	ts->irq_gpio = NVTTOUCH_INT_PIN;
	LOG_DONE();
}
#endif

/*******************************************************
Description:
Novatek touchscreen config and request gpio

return:
Executive outcomes. 0---succeed. not 0---failed.
 *******************************************************/
static int nvt_gpio_config(struct nvt_ts_data *ts)
{
	int32_t ret = 0;

	LOG_ENTRY();
#if NVT_TOUCH_SUPPORT_HW_RST
	/* request RST-pin (Output/High) */
	if (gpio_is_valid(ts->reset_gpio)) {
		ret = gpio_request_one(ts->reset_gpio, GPIOF_OUT_INIT_HIGH, "NVT-tp-rst");
		if (ret) {
			NVT_ERR("Failed to request NVT-tp-rst GPIO\n");
			goto err_request_reset_gpio;
		}
	}
#endif

	/* request INT-pin (Input) */
	if (gpio_is_valid(ts->irq_gpio)) {
		ret = gpio_request_one(ts->irq_gpio, GPIOF_IN, "NVT-int");
		if (ret) {
			NVT_ERR("Failed to request NVT-int GPIO\n");
			goto err_request_irq_gpio;
		}
	}

	return ret;

err_request_irq_gpio:
#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_free(ts->reset_gpio);
err_request_reset_gpio:
#endif
	LOG_DONE();
	return ret;
}

#if NVT_TOUCH_ESD_PROTECT
void nvt_esd_check_enable(uint8_t enable)
{
	LOG_ENTRY();
	/* enable/disable esd check flag */
	esd_check = enable;
	/* update interrupt timer */
	irq_timer = jiffies;
	/* clear esd_retry counter, if protect function is enabled */
	esd_retry = enable ? 0 : esd_retry;
	LOG_DONE();
}

static uint8_t nvt_fw_recovery(uint8_t *point_data)
{
	uint8_t i = 0;
	uint8_t detected = true;

	LOG_ENTRY();
	/* check pattern */
	for (i=1 ; i<7 ; i++) {
		if (point_data[i] != 0x77) {
			detected = false;
			break;
		}
	}

	LOG_DONE();
	return detected;
}

static void nvt_esd_check_func(struct work_struct *work)
{
	unsigned int timer = jiffies_to_msecs(jiffies - irq_timer);

	LOG_ENTRY();
	//NVT_ERR("esd_check = %d (retry %d/%d)\n", esd_check, esd_retry, esd_retry_max);	//DEBUG

	if (esd_retry >= esd_retry_max)
		nvt_esd_check_enable(false);

	if ((timer > NVT_TOUCH_ESD_CHECK_PERIOD) && esd_check) {
		NVT_ERR("do ESD recovery, timer = %d, retry = %d\n", timer, esd_retry);
		/* do esd recovery, bootloader reset */
		nvt_bootloader_reset();
		/* update interrupt timer */
		irq_timer = jiffies;
		/* update esd_retry counter */
		esd_retry++;
	}

	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
	LOG_DONE();
}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#define POINT_DATA_LEN 65
/*******************************************************
Description:
Novatek touchscreen work function.

return:
n.a.
 *******************************************************/
static void nvt_ts_work_func(struct work_struct *work)
{
	int32_t ret = -1;
	uint8_t point_data[POINT_DATA_LEN + 1] = {0};
	uint32_t position = 0;
	uint32_t input_x = 0;
	uint32_t input_y = 0;
	uint32_t input_w = 0;
	uint32_t input_p = 0;
	uint8_t input_id = 0;
#if MT_PROTOCOL_B
	uint8_t press_id[TOUCH_MAX_FINGER_NUM] = {0};
#endif /* MT_PROTOCOL_B */
	int32_t i = 0;
	int32_t finger_cnt = 0;

	LOG_ENTRY();
	mutex_lock(&ts->lock);

	ret = CTP_I2C_READ(ts->client, I2C_FW_Address, point_data, POINT_DATA_LEN + 1);
	if (ret < 0) {
		NVT_ERR("CTP_I2C_READ failed.(%d)\n", ret);
		goto XFER_ERROR;
	}
	/*
	//--- dump I2C buf ---
	for (i = 0; i < 10; i++) {
	printk("%02X %02X %02X %02X %02X %02X  ", point_data[1+i*6], point_data[2+i*6], point_data[3+i*6], point_data[4+i*6], point_data[5+i*6], point_data[6+i*6]);
	}
	printk("\n");
	*/

#if NVT_TOUCH_ESD_PROTECT
	if (nvt_fw_recovery(point_data)) {
		nvt_esd_check_enable(true);
		goto XFER_ERROR;
	}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if WAKEUP_GESTURE
	if (bTouchIsAwake == 0) {
		input_id = (uint8_t)(point_data[1] >> 3);
		nvt_ts_wakeup_gesture_report(input_id, point_data);
		enable_irq(ts->client->irq);
		mutex_unlock(&ts->lock);
		return;
	}
#endif

	finger_cnt = 0;

	for (i = 0; i < ts->max_touch_num; i++) {
		position = 1 + 6 * i;
		input_id = (uint8_t)(point_data[position + 0] >> 3);
		if ((input_id == 0) || (input_id > ts->max_touch_num))
			continue;

		if (((point_data[position] & 0x07) == 0x01) || ((point_data[position] & 0x07) == 0x02)) {	//finger down (enter & moving)
#if NVT_TOUCH_ESD_PROTECT
			/* update interrupt timer */
			irq_timer = jiffies;
#endif /* #if NVT_TOUCH_ESD_PROTECT */
			input_x = (uint32_t)(point_data[position + 1] << 4) + (uint32_t) (point_data[position + 3] >> 4);
			input_y = (uint32_t)(point_data[position + 2] << 4) + (uint32_t) (point_data[position + 3] & 0x0F);
			if ((input_x < 0) || (input_y < 0))
				continue;
			if ((input_x > ts->abs_x_max) || (input_y > ts->abs_y_max))
				continue;
			input_w = (uint32_t)(point_data[position + 4]);
			if (input_w == 0)
				input_w = 1;
			if (i < 2) {
				input_p = (uint32_t)(point_data[position + 5]) + (uint32_t)(point_data[i + 63] << 8);
				if (input_p > TOUCH_FORCE_NUM)
					input_p = TOUCH_FORCE_NUM;
			} else {
				input_p = (uint32_t)(point_data[position + 5]);
			}
			if (input_p == 0)
				input_p = 1;

#if MT_PROTOCOL_B
			press_id[input_id - 1] = 1;
			input_mt_slot(ts->input_dev, input_id - 1);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
#else /* MT_PROTOCOL_B */
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, input_id - 1);
			input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif /* MT_PROTOCOL_B */

			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, input_x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, input_y);
#if ENABLE_TOUCH_SZIE
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, input_w);
#endif
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, input_p);

#if MT_PROTOCOL_B
#else /* MT_PROTOCOL_B */
			input_mt_sync(ts->input_dev);
#endif /* MT_PROTOCOL_B */

			finger_cnt++;
		}
	}

#if MT_PROTOCOL_B
	for (i = 0; i < ts->max_touch_num; i++) {
		if (press_id[i] != 1) {
			input_mt_slot(ts->input_dev, i);
#if ENABLE_TOUCH_SZIE
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
#endif
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
		}
	}

	input_report_key(ts->input_dev, BTN_TOUCH, (finger_cnt > 0));
#else /* MT_PROTOCOL_B */
	if (finger_cnt == 0) {
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
		input_mt_sync(ts->input_dev);
	}
#endif /* MT_PROTOCOL_B */

#if TOUCH_KEY_NUM > 0
	if (point_data[61] == 0xF8) {
#if NVT_TOUCH_ESD_PROTECT
		/* update interrupt timer */
		irq_timer = jiffies;
#endif /* #if NVT_TOUCH_ESD_PROTECT */
		for (i = 0; i < ts->max_button_num; i++) {
			input_report_key(ts->input_dev, touch_key_array[i], ((point_data[62] >> i) & 0x01));
		}
	} else {
		for (i = 0; i < ts->max_button_num; i++) {
			input_report_key(ts->input_dev, touch_key_array[i], 0);
		}
	}
#endif

	input_sync(ts->input_dev);

XFER_ERROR:
	enable_irq(ts->client->irq);

	mutex_unlock(&ts->lock);
	LOG_DONE();
}

/*******************************************************
Description:
Novatek touchscreen usb plugin work function.

return:
n.a.
 *******************************************************/
static void nvt_ts_usb_plugin_work_func(struct work_struct *work)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	LOG_ENTRY();
	if ( !bTouchIsAwake || (ts->touch_state != TOUCH_STATE_WORKING) ) {
		NVT_ERR("tp is suspended or flashing, can not to set\n");
		return;
	}

	NVT_LOG("++\n");
	mutex_lock(&ts->lock);
	NVT_LOG("usb_plugin = %d\n", g_nvt.usb_plugin);

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	ret = CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto exit;
	}

	buf[0] = EVENT_MAP_HOST_CMD;
	if (g_nvt.usb_plugin)
		buf[1] = 0x53;// power plug ac on
	else
		buf[1] = 0x51;// power plug off

	ret = CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
	if (ret < 0) {
		NVT_ERR("Write pwr plug switch command fail!\n");
		goto exit;
	}

exit:
	mutex_unlock(&ts->lock);
	NVT_LOG("--\n");
	LOG_DONE();
}

/*******************************************************
Description:
External interrupt service routine.

return:
irq execute status.
 *******************************************************/
static irqreturn_t nvt_ts_irq_handler(int32_t irq, void *dev_id)
{
	LOG_ENTRY();
	disable_irq_nosync(ts->client->irq);

#if WAKEUP_GESTURE
	if (bTouchIsAwake == 0) {
		wake_lock_timeout(&gestrue_wakelock, msecs_to_jiffies(5000));
	}
#endif

	queue_work(nvt_wq, &ts->nvt_work);

	LOG_DONE();
	return IRQ_HANDLED;
}

/*******************************************************
Description:
Novatek touchscreen check and stop crc reboot loop.

return:
n.a.
 *******************************************************/
void nvt_stop_crc_reboot(void)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;

	LOG_ENTRY();
	//read dummy buffer to check CRC fail reboot is happening or not

	//---change I2C index to prevent geting 0xFF, but not 0xFC---
	buf[0] = 0xFF;
	buf[1] = 0x01;
	buf[2] = 0xF6;
	CTP_I2C_WRITE(ts->client, I2C_BLDR_Address, buf, 3);

	//---read to check if buf is 0xFC which means IC is in CRC reboot ---
	buf[0] = 0x4E;
	CTP_I2C_READ(ts->client, I2C_BLDR_Address, buf, 4);

	if ((buf[1] == 0xFC) ||
			((buf[1] == 0xFF) && (buf[2] == 0xFF) && (buf[3] == 0xFF))) {

		//IC is in CRC fail reboot loop, needs to be stopped!
		for (retry = 5; retry > 0; retry--) {

			//---write i2c cmds to reset idle : 1st---
			buf[0]=0x00;
			buf[1]=0xA5;
			CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);

			//---write i2c cmds to reset idle : 2rd---
			buf[0]=0x00;
			buf[1]=0xA5;
			CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
			msleep(1);

			//---clear CRC_ERR_FLAG---
			buf[0] = 0xFF;
			buf[1] = 0x03;
			buf[2] = 0xF1;
			CTP_I2C_WRITE(ts->client, I2C_BLDR_Address, buf, 3);

			buf[0] = 0x35;
			buf[1] = 0xA5;
			CTP_I2C_WRITE(ts->client, I2C_BLDR_Address, buf, 2);

			//---check CRC_ERR_FLAG---
			buf[0] = 0xFF;
			buf[1] = 0x03;
			buf[2] = 0xF1;
			CTP_I2C_WRITE(ts->client, I2C_BLDR_Address, buf, 3);

			buf[0] = 0x35;
			buf[1] = 0x00;
			CTP_I2C_READ(ts->client, I2C_BLDR_Address, buf, 2);

			if (buf[1] == 0xA5)
				break;
		}
		if (retry == 0)
			NVT_ERR("CRC auto reboot is not able to be stopped! buf[1]=0x%02X\n", buf[1]);
	}

	LOG_DONE();
	return;
}

/*******************************************************
Description:
Novatek touchscreen check chip version trim function.

return:
Executive outcomes. 0---NVT IC. -1---not NVT IC.
 *******************************************************/
static int8_t nvt_ts_check_chip_ver_trim(void)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;
	int32_t list = 0;
	int32_t i = 0;
	int32_t found_nvt_chip = 0;
	int32_t ret = -1;

	LOG_ENTRY();
	nvt_bootloader_reset(); // NOT in retry loop

	//---Check for 5 times---
	for (retry = 5; retry > 0; retry--) {
		nvt_sw_reset_idle();

		buf[0] = 0x00;
		buf[1] = 0x35;
		CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
		msleep(10);

		buf[0] = 0xFF;
		buf[1] = 0x01;
		buf[2] = 0xF6;
		CTP_I2C_WRITE(ts->client, I2C_BLDR_Address, buf, 3);

		buf[0] = 0x4E;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		CTP_I2C_READ(ts->client, I2C_BLDR_Address, buf, 7);
		NVT_LOG("buf[1]=0x%02X, buf[2]=0x%02X, buf[3]=0x%02X, buf[4]=0x%02X, buf[5]=0x%02X, buf[6]=0x%02X\n",
				buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

		//---Stop CRC check to prevent IC auto reboot---
		if ((buf[1] == 0xFC) ||
				((buf[1] == 0xFF) && (buf[2] == 0xFF) && (buf[3] == 0xFF))) {
			nvt_stop_crc_reboot();
			continue;
		}

		// compare read chip id on supported list
		for (list = 0; list < (sizeof(trim_id_table) / sizeof(struct nvt_ts_trim_id_table)); list++) {
			found_nvt_chip = 0;

			// compare each byte
			for (i = 0; i < NVT_ID_BYTE_MAX; i++) {
				if (trim_id_table[list].mask[i]) {
					if (buf[i + 1] != trim_id_table[list].id[i])
						break;
				}
			}

			if (i == NVT_ID_BYTE_MAX) {
				found_nvt_chip = 1;
			}

			if (found_nvt_chip) {
				NVT_LOG("This is NVT touch IC\n");
				ts->mmap = trim_id_table[list].mmap;
				ts->carrier_system = trim_id_table[list].carrier_system;
				ret = 0;
				goto out;
			} else {
				ts->mmap = NULL;
				ret = -1;
			}
		}

		msleep(10);
	}

out:
	LOG_DONE();
	return ret;
}

/*******************************************************
Description:
Novatek touchscreen driver probe function.

return:
Executive outcomes. 0---succeed. negative---failed
 *******************************************************/
static int32_t nvt_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int32_t ret = 0;
#if ((TOUCH_KEY_NUM > 0) || WAKEUP_GESTURE)
	int32_t retry = 0;
#endif

	LOG_ENTRY();
	NVT_LOG("start\n");

	ts = kzalloc(sizeof(struct nvt_ts_data), GFP_KERNEL);
	if (IS_ERR_OR_NULL(ts)) {
		NVT_ERR("failed to allocated memory for nvt ts data\n");
		return -ENOMEM;
	}

	ts->client = client;
	i2c_set_clientdata(client, ts);

	//---parse dts---
	nvt_parse_dt(&client->dev);

	//---request and config GPIOs---
	ret = nvt_gpio_config(ts);
	if (ret) {
		NVT_ERR("gpio config error!\n");
		goto err_gpio_config_failed;
	}

	//---check i2c func.---
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		NVT_ERR("i2c_check_functionality failed. (no I2C_FUNC_I2C)\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	// need 10ms delay after POR(power on reset)
	msleep(10);

	//---check chip version trim---
	ret = nvt_ts_check_chip_ver_trim();
	if (ret) {
		NVT_ERR("chip is not identified\n");
		ret = -EINVAL;
		goto err_chipvertrim_failed;
	}

	mutex_init(&ts->lock);
	mutex_init(&ts->pm_mutex);

	mutex_lock(&ts->lock);
	nvt_bootloader_reset();
	nvt_check_fw_reset_state(RESET_STATE_INIT);
	nvt_get_fw_info();
	mutex_unlock(&ts->lock);

	//---create workqueue---
	nvt_wq = create_workqueue("nvt_wq");
	if (!nvt_wq) {
		NVT_ERR("nvt_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_wq_failed;
	}
	INIT_WORK(&ts->nvt_work, nvt_ts_work_func);
	INIT_WORK(&g_nvt.nvt_usb_plugin_work, nvt_ts_usb_plugin_work_func);

	//---allocate input device---
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		NVT_ERR("allocate input device failed\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	ts->max_touch_num = TOUCH_MAX_FINGER_NUM;

#if TOUCH_KEY_NUM > 0
	ts->max_button_num = TOUCH_KEY_NUM;
#endif

	ts->int_trigger_type = INT_TRIGGER_TYPE;


	//---set input device info.---
	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	ts->input_dev->propbit[0] = BIT(INPUT_PROP_DIRECT);

#if MT_PROTOCOL_B
	input_mt_init_slots(ts->input_dev, ts->max_touch_num, 0);
#endif

	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, TOUCH_FORCE_NUM, 0, 0);    //pressure = TOUCH_FORCE_NUM

#if TOUCH_MAX_FINGER_NUM > 1
#if ENABLE_TOUCH_SZIE
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);    //area = 255
#endif
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->abs_x_max-1, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->abs_y_max-1, 0, 0);
#if MT_PROTOCOL_B
	// no need to set ABS_MT_TRACKING_ID, input_mt_init_slots() already set it
#else
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->max_touch_num, 0, 0);
#endif //MT_PROTOCOL_B
#endif //TOUCH_MAX_FINGER_NUM > 1

#if TOUCH_KEY_NUM > 0
	for (retry = 0; retry < ts->max_button_num; retry++) {
		input_set_capability(ts->input_dev, EV_KEY, touch_key_array[retry]);
	}
#endif

#if WAKEUP_GESTURE
	ts->input_dev->event =nvt_gesture_switch;
	for (retry = 0; retry < (sizeof(gesture_key_array) / sizeof(gesture_key_array[0])); retry++) {
		input_set_capability(ts->input_dev, EV_KEY, gesture_key_array[retry]);
	}
	wake_lock_init(&gestrue_wakelock, WAKE_LOCK_SUSPEND, "poll-wake-lock");
#endif

	sprintf(ts->phys, "input/ts");
	ts->input_dev->name = NVT_TS_NAME;
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_I2C;

	//---register input device---
	ret = input_register_device(ts->input_dev);
	if (ret) {
		NVT_ERR("register input device (%s) failed. ret=%d\n", ts->input_dev->name, ret);
		goto err_input_register_device_failed;
	}

	//---set int-pin & request irq---
	client->irq = gpio_to_irq(ts->irq_gpio);
	if (client->irq) {
		NVT_LOG("int_trigger_type=%d\n", ts->int_trigger_type);

#if WAKEUP_GESTURE
		ret = request_irq(client->irq, nvt_ts_irq_handler, ts->int_trigger_type | IRQF_NO_SUSPEND, client->name, ts);
#else
		ret = request_irq(client->irq, nvt_ts_irq_handler, ts->int_trigger_type, client->name, ts);
#endif
		if (ret != 0) {
			NVT_ERR("request irq failed. ret=%d\n", ret);
			goto err_int_request_failed;
		} else {
			disable_irq(client->irq);
			NVT_LOG("request irq %d succeed\n", client->irq);
		}
	}

#if BOOT_UPDATE_FIRMWARE
	nvt_fwu_wq = create_singlethread_workqueue("nvt_fwu_wq");
	if (!nvt_fwu_wq) {
		NVT_ERR("nvt_fwu_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_fwu_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->nvt_fwu_work, Boot_Update_Firmware);
	// please make sure boot update start after display reset(RESX) sequence
	queue_delayed_work(nvt_fwu_wq, &ts->nvt_fwu_work, msecs_to_jiffies(14000));
#endif

#if NVT_TOUCH_ESD_PROTECT
	INIT_DELAYED_WORK(&nvt_esd_check_work, nvt_esd_check_func);
	nvt_esd_check_wq = create_workqueue("nvt_esd_check_wq");
	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	//---set device node---

#ifdef CONFIG_KERNEL_CUSTOM_FACTORY
	ret = init_lct_tp_work(lct_tp_work_node_callback);
	if (ret < 0) {
		NVT_ERR("Failed to add /proc/tp_work node!\n");
	}
#endif

	ret = init_lct_tp_gesture(lct_tp_gesture_node_callback);
	if (ret < 0) {
		NVT_ERR("Failed to add /proc/tp_work node!\n");
	}

	ret = init_lct_tp_grip_area(lct_tp_set_screen_angle_callback, lct_tp_get_screen_angle_callback);
	if (ret < 0) {
		NVT_ERR("Failed to add /proc/tp_grip_area node!\n");
	}

	/* add touchpad information by wanghan start */
	lct_nvt_tp_info_node_init();
	/* add touchpad information by wanghan end */

#if NVT_TOUCH_PROC
	ret = nvt_flash_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt flash proc init failed. ret=%d\n", ret);
		goto err_init_NVT_ts;
	}
#endif

#if NVT_TOUCH_EXT_PROC
	ret = nvt_extra_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt extra proc init failed. ret=%d\n", ret);
		goto err_init_NVT_ts;
	}
#endif

#if NVT_TOUCH_MP
	ret = nvt_mp_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt mp proc init failed. ret=%d\n", ret);
		goto err_init_NVT_ts;
	}
#endif

#if defined(CONFIG_FB)
	ts->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts->fb_notif);
	if(ret) {
		NVT_ERR("register fb_notifier failed. ret=%d\n", ret);
		goto err_register_fb_notif_failed;
	}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = nvt_ts_early_suspend;
	ts->early_suspend.resume = nvt_ts_late_resume;
	ret = register_early_suspend(&ts->early_suspend);
	if(ret) {
		NVT_ERR("register early suspend failed. ret=%d\n", ret);
		goto err_register_early_suspend_failed;
	}
#endif

	/* add resume work by wanghan start */
	INIT_WORK(&g_resume_work, do_nvt_ts_resume_work);
	/* add resume work by wanghan end */

	bTouchIsAwake = 1;
	NVT_LOG("end\n");

	enable_irq(client->irq);

	//set novatek touch drver global value;
	g_nvt.valid = true;
	g_nvt.usb_plugin = false;

	LOG_DONE();
	return 0;

#if defined(CONFIG_FB)
err_register_fb_notif_failed:
#elif defined(CONFIG_HAS_EARLYSUSPEND)
err_register_early_suspend_failed:
#endif
#if (NVT_TOUCH_PROC || NVT_TOUCH_EXT_PROC || NVT_TOUCH_MP)
err_init_NVT_ts:
#endif
	free_irq(client->irq, ts);
#if BOOT_UPDATE_FIRMWARE
err_create_nvt_fwu_wq_failed:
#endif
err_int_request_failed:
err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
err_create_nvt_wq_failed:
	mutex_destroy(&ts->lock);
err_chipvertrim_failed:
err_check_functionality_failed:
	gpio_free(ts->irq_gpio);
err_gpio_config_failed:
	i2c_set_clientdata(client, NULL);
	kfree(ts);
	LOG_DONE();
	return ret;
}

/*******************************************************
Description:
Novatek touchscreen driver release function.

return:
Executive outcomes. 0---succeed.
 *******************************************************/
static int32_t nvt_ts_remove(struct i2c_client *client)
{
	//struct nvt_ts_data *ts = i2c_get_clientdata(client);

	LOG_ENTRY();
#if defined(CONFIG_FB)
	if (fb_unregister_client(&ts->fb_notif))
		NVT_ERR("Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif

	/* add resume work by wanghan start */
	cancel_work_sync(&g_resume_work);
	/* add resume work by wanghan end */

	mutex_destroy(&ts->lock);
	mutex_destroy(&ts->pm_mutex);

	NVT_LOG("Removing driver...\n");

	free_irq(client->irq, ts);
	input_unregister_device(ts->input_dev);
	i2c_set_clientdata(client, NULL);
	kfree(ts);

	LOG_DONE();
	return 0;
}

/*******************************************************
Description:
Novatek touchscreen driver suspend function.

return:
Executive outcomes. 0---succeed.
 *******************************************************/
static int32_t nvt_ts_suspend(struct device *dev)
{
	uint8_t buf[4] = {0};
#if MT_PROTOCOL_B
	uint32_t i = 0;
#endif

	LOG_ENTRY();
	if (!bTouchIsAwake) {
		NVT_LOG("Touch is already suspend\n");
		return 0;
	}

	mutex_lock(&ts->lock);

	NVT_LOG("start\n");

	bTouchIsAwake = 0;

#if NVT_TOUCH_ESD_PROTECT
	cancel_delayed_work_sync(&nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (enable_gesture_mode) { // Gesture Mode
		//---write i2c command to enter "wakeup gesture mode"---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x13;
#if 0 // Do not set 0xFF first, ToDo
		buf[2] = 0xFF;
		buf[3] = 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 4);
#else
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
#endif

		enable_irq_wake(ts->client->irq);

		NVT_LOG("Enabled touch wakeup gesture\n");
	} else { // Normal Mode
		disable_irq(ts->client->irq);

		//---write i2c command to enter "deep sleep mode"---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x11;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
	}

	/* release all touches */
#if MT_PROTOCOL_B
	for (i = 0; i < ts->max_touch_num; i++) {
		input_mt_slot(ts->input_dev, i);
#if ENABLE_TOUCH_SZIE
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
#endif
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
	}
#endif
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
#if !MT_PROTOCOL_B
	input_mt_sync(ts->input_dev);
#endif
	input_sync(ts->input_dev);

	msleep(50);

	mutex_unlock(&ts->lock);
	suspend_state = true;

	NVT_LOG("end\n");

	LOG_DONE();
	return 0;
}

/*******************************************************
Description:
Novatek touchscreen driver resume function.

return:
Executive outcomes. 0---succeed.
 *******************************************************/
static int32_t nvt_ts_resume(struct device *dev)
{
	LOG_ENTRY();
	if (bTouchIsAwake) {
		NVT_LOG("Touch is already resume\n");
		return 0;
	}

	mutex_lock(&ts->lock);

	NVT_LOG("start\n");

	// please make sure display reset(RESX) sequence and mipi dsi cmds sent before this
#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_set_value(ts->reset_gpio, 1);
#endif
	nvt_bootloader_reset();
	nvt_check_fw_reset_state(RESET_STATE_REK);

	if (delay_gesture) {
		enable_gesture_mode = !enable_gesture_mode;
	}

	if (!enable_gesture_mode) {
		enable_irq(ts->client->irq);
	}

	if (delay_gesture) {
		enable_gesture_mode = !enable_gesture_mode;
	}

#if NVT_TOUCH_ESD_PROTECT
	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	bTouchIsAwake = 1;

	mutex_unlock(&ts->lock);
	suspend_state = false;
	delay_gesture = false;

	if (g_nvt.usb_plugin)
		nvt_ts_usb_plugin_work_func(NULL);

	NVT_LOG("end\n");

	LOG_DONE();
	return 0;
}

/* add resume work by wanghan start */
static void do_nvt_ts_resume_work(struct work_struct *work)
{
	int ret = 0;
	LOG_ENTRY();
	mutex_lock(&ts->pm_mutex);
	ret = nvt_ts_resume(&ts->client->dev);
	if (ret < 0)
		NVT_ERR("nvt_ts_resume faild! ret=%d\n", ret);
	mutex_unlock(&ts->pm_mutex);
	LOG_DONE();
	return;
}
/* add resume work by wanghan end */

#ifdef CONFIG_KERNEL_CUSTOM_FACTORY
static int lct_tp_work_node_callback(bool flag)
{
	int ret = 0;

	LOG_ENTRY();
	if (enable_gesture_mode) {
		LOGV("ERROR: novatek gesture=%d!\n", enable_gesture_mode);
		return -1;
	}
	if (suspend_state) return 0;
	if (flag) {// enable(resume) tp
		suspend_state = true;
		ret = nvt_ts_resume(&ts->client->dev);
		if (ret < 0) NVT_ERR("nvt_ts_resume faild! ret=%d\n", ret);
	} else {// disbale(suspend) tp
		ret = nvt_ts_suspend(&ts->client->dev);
		if (ret < 0) NVT_ERR("nvt_ts_suspend faild! ret=%d\n", ret);
	}
	suspend_state = false;

	LOG_DONE();
	return ret;
}
#endif

static int lct_tp_gesture_node_callback(bool flag)
{
	int retval = 0;

	LOG_ENTRY();
	if (suspend_state) {
		LOGV("ERROR: TP is suspend!\n");
		return -1;
	}
	if(flag) {
		enable_gesture_mode = true;
		LOGV("enable gesture mode\n");
	} else {
		enable_gesture_mode = false;
		LOGV("disable gesture mode\n");
	}
	LOG_DONE();
	return retval;
}

static int lct_tp_get_screen_angle_callback(void)
{
	uint8_t tmp[8] = {0};
	int32_t ret = -EIO;
	uint8_t edge_reject_switch;

	if ( !bTouchIsAwake || (ts->touch_state != TOUCH_STATE_WORKING) ) {
		NVT_ERR("tp is suspended or flashing, can not to set\n");
		return ret;
	}

	NVT_LOG("++\n");

	mutex_lock(&ts->lock);
	// Why go to sleep? Ask to FAE.
	msleep(35);

	tmp[0] = 0xFF;
	tmp[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	tmp[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	ret = CTP_I2C_WRITE(ts->client, I2C_FW_Address, tmp, 3);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto out;
	}

	tmp[0] = 0x5C;
	tmp[1] = 0x00;
	ret = CTP_I2C_READ(ts->client, I2C_FW_Address, tmp, 2);
	if (ret < 0) {
		NVT_ERR("Read edge reject switch status fail!\n");
		goto out;
	}

	edge_reject_switch = ((tmp[1] >> 5) & 0x03);
	NVT_LOG("edge_reject_switch = %d\n", edge_reject_switch);
	ret = edge_reject_switch;

out:
	mutex_unlock(&ts->lock);
	NVT_LOG("--\n");
	return ret;
}
static int lct_tp_set_screen_angle_callback(unsigned int angle)
{
	uint8_t tmp[3];
	int ret = -EIO;

	if ( !bTouchIsAwake || (ts->touch_state != TOUCH_STATE_WORKING) ) {
		NVT_ERR("tp is suspended or flashing, can not to set\n");
		return ret;
	}

	NVT_LOG("++\n");

	mutex_lock(&ts->lock);

	tmp[0] = 0XFF;
	tmp[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	tmp[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	ret = CTP_I2C_WRITE(ts->client, I2C_FW_Address, tmp, 3);
	if (ret < 0) {
		NVT_LOG("i2c wite error!\n");
		goto out;
	}
	tmp[0] = EVENT_MAP_HOST_CMD;
	if (angle == 90) {
		tmp[1] = 0xBC;
	} else if (angle == 270) {
		tmp[1] = 0xBB;
	} else {
		tmp[1] = 0xBA;
	}
	ret = CTP_I2C_WRITE(ts->client, I2C_FW_Address, tmp, 2);
	if (ret < 0) {
		NVT_LOG("i2c read error!\n");
		goto out;
	}
	ret = 0;

out:
	mutex_unlock(&ts->lock);
	NVT_LOG("--\n");
	return ret;
}

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct nvt_ts_data *ts =
		container_of(self, struct nvt_ts_data, fb_notif);

	LOG_ENTRY();
	if (evdata && evdata->data && event == FB_EARLY_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_POWERDOWN) {
			NVT_LOG("touch suspend\n");
#ifdef CONFIG_KERNEL_CUSTOM_FACTORY
			if (!get_lct_tp_work_status()) {
				suspend_state = true;
				return 0;
			}
#endif
			flush_work(&g_resume_work);
			mutex_lock(&ts->pm_mutex);
			nvt_ts_suspend(&ts->client->dev);
			mutex_unlock(&ts->pm_mutex);
		}
	} else if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			NVT_LOG("touch resume\n");
#ifdef CONFIG_KERNEL_CUSTOM_FACTORY
			if (!get_lct_tp_work_status()) {
				suspend_state = false;
				return 0;
			}
#endif
			schedule_work(&g_resume_work);
		}
	}

	LOG_DONE();
	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
/*******************************************************
Description:
Novatek touchscreen driver early suspend function.

return:
n.a.
 *******************************************************/
static void nvt_ts_early_suspend(struct early_suspend *h)
{
	LOG_ENTRY();
	nvt_ts_suspend(ts->client, PMSG_SUSPEND);
	LOG_DONE();
}

/*******************************************************
Description:
Novatek touchscreen driver late resume function.

return:
n.a.
 *******************************************************/
static void nvt_ts_late_resume(struct early_suspend *h)
{
	LOG_ENTRY();
	nvt_ts_resume(ts->client);
	LOG_DONE();
}
#endif

#if 0
static const struct dev_pm_ops nvt_ts_dev_pm_ops = {
	.suspend = nvt_ts_suspend,
	.resume  = nvt_ts_resume,
};
#endif

static const struct i2c_device_id nvt_ts_id[] = {
	{ NVT_I2C_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static struct of_device_id nvt_match_table[] = {
	{ .compatible = "novatek,NVT-ts",},
	{ },
};
#endif
/*
   static struct i2c_board_info __initdata nvt_i2c_boardinfo[] = {
   {
   I2C_BOARD_INFO(NVT_I2C_NAME, I2C_FW_Address),
   },
   };
   */

static struct i2c_driver nvt_i2c_driver = {
	.probe		= nvt_ts_probe,
	.remove		= nvt_ts_remove,
	//	.suspend	= nvt_ts_suspend,
	//	.resume		= nvt_ts_resume,
	.id_table	= nvt_ts_id,
	.driver = {
		.name	= NVT_I2C_NAME,
		.owner	= THIS_MODULE,
#if 0
#ifdef CONFIG_PM
		.pm = &nvt_ts_dev_pm_ops,
#endif
#endif
#ifdef CONFIG_OF
		.of_match_table = nvt_match_table,
#endif
	},
};

//doesn't support charging in shutdown mode
#define DOES_NOT_SUPPORT_SHUTDOWN_CHARGING

#ifdef DOES_NOT_SUPPORT_SHUTDOWN_CHARGING
extern char *saved_command_line;
#endif

/*******************************************************
Description:
Driver Install function.

return:
Executive Outcomes. 0---succeed. not 0---failed.
 ********************************************************/
static int32_t __init nvt_driver_init(void)
{
	int32_t ret = 0;

	LOG_ENTRY();
	NVT_LOG("start\n");

	//set novatek touch drver global value;
	g_nvt.valid = false;

#ifdef DOES_NOT_SUPPORT_SHUTDOWN_CHARGING
	if (strstr(saved_command_line, "androidboot.mode=charger") != NULL) {
		LOGV("androidboot.mode=charger, TP doesn't support!\n");
		goto err;
	}
#endif

	//Check TP hardware
	if (IS_ERR_OR_NULL(g_lcd_id)){
		NVT_ERR("g_lcd_id is ERROR!\n");
		goto err;
	} else {
		if (strstr(g_lcd_id,"tianma nt36672a") != NULL) {
			NVT_LOG("TP info: [Vendor]tianma [IC]nt36672a\n");
		} else if (strstr(g_lcd_id,"shenchao nt36672a") != NULL) {
			NVT_LOG("TP info: [Vendor]shenchao [IC] nt36672a\n");
		} else {
			NVT_ERR("Touch IC is not nt36672a\n");
			goto err;
		}
	}

	//---add i2c driver---
	ret = i2c_add_driver(&nvt_i2c_driver);
	if (ret) {
		pr_err("%s: failed to add i2c driver", __func__);
		goto err;
	}

	pr_info("%s: finished\n", __func__);
	goto exit;

err:
	ret = -ENODEV;
exit:
	LOG_DONE();
	return ret;
}

/*******************************************************
Description:
Driver uninstall function.

return:
n.a.
 ********************************************************/
static void __exit nvt_driver_exit(void)
{
	LOG_ENTRY();
	NVT_LOG("exit tp driver ...\n");
	i2c_del_driver(&nvt_i2c_driver);

	if (nvt_wq)
		destroy_workqueue(nvt_wq);

#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq)
		destroy_workqueue(nvt_fwu_wq);
#endif

#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq)
		destroy_workqueue(nvt_esd_check_wq);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
	LOG_DONE();
}

//late_initcall(nvt_driver_init);
module_init(nvt_driver_init);
module_exit(nvt_driver_exit);

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL");
