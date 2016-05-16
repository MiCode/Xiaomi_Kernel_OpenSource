/* drivers/input/touchscreen/NVTtouch_205.c
 *
 * Copyright (C) 2010 - 2014 Novatek, Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Revision : V2 (2014/10/22)
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
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/device.h>

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/unistd.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

#include <linux/wakelock.h>

#include <linux/input/mt.h>
#include "NVTtouch_205.h"
#include <linux/of_gpio.h>

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>

#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#if  WT_ADD_CTP_INFO
#include <linux/hardware_info.h>
static char tp_string_version[40];
#endif

#if BOOT_UPDATE_FIRMWARE
#include "NVT_firmware_205.h"
static struct workqueue_struct *nvt_fwu_wq;
#endif

#if CTP_WT_TP_OPENSHORT_TEST
#include "NVTtouch_205_remap_mptool.h"

#define CTP_PARENT_PROC_NAME "touchscreen"
#define CTP_OPEN_PROC_NAME		"ctp_openshort_test"

uint8_t buffer[64] = {0x00};
int XNum;
int YNum;
typedef struct {
	unsigned short Tx;
	unsigned short Rx;
	signed short Flag;
} Mapping_t;
Mapping_t MappingTable[45][35];

int iRecord_AIN = 0;
unsigned char GridColor_Flag[40][40];

#define cWhite		0x00
#define cRed		0x01
#define cYellow		0x02
#define cBlue 		0x03
#define cPurople 	0x04
#define cTeal		0x05
char FAIL[2048];
char GPIOMSG[2048];

int iAccum;
int fCF;
int fCC;
unsigned char RecordResult[40*40];
int DiffTemp[5000];
unsigned short RawDataTmp[1000];
const int CF_table[8] = {222, 327, 428, 533, 632, 737, 838, 943};
const int CC_table[32] = {30, 57, 82, 109, 136, 163, 188, 215,
			237, 264, 289, 316, 343, 370 , 395, 422,
			856, 883, 908, 935, 962, 989, 1014, 1041,
			1063, 109 , 1115, 1142, 1169, 1196, 1221, 1248};
#define MaxStatisticsBuf 2000
int StatisticsNum[MaxStatisticsBuf];
long int StatisticsSum[MaxStatisticsBuf];
int StatisticsStep;
int Mutual_GoldenRatio[40][40];
int Mutual_Data[40*40];
int CM_Data[40*40];

#endif

struct nvt_ts_data *ts;

static struct workqueue_struct *nvt_wq;
struct nvt_platform_data *pdata;


#if defined(CONFIG_FB)
static void fb_notify_resume_work(struct work_struct *work);
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#endif


struct sprd_i2c_setup_data {
	unsigned i2c_bus;
	unsigned short i2c_address;
	int irq;
	char type[I2C_NAME_SIZE];
};

static int sprd_3rdparty_gpio_tp_rst;
static int sprd_3rdparty_gpio_tp_irq;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void nvt_ts_early_suspend(struct early_suspend *h);
static void nvt_ts_late_resume(struct early_suspend *h);
#endif


/*******************************************************
 I2C Read & Write
*******************************************************/
static int CTP_I2C_READ(struct i2c_client *client, uint8_t address, uint8_t *buf, uint8_t len)
{
	struct i2c_msg msgs[2];
	int ret = -1;
	int retries = 0;

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = address;
	msgs[0].len   = 1;
	msgs[0].buf   = &buf[0];

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = address;
	msgs[1].len   = len-1;
	msgs[1].buf   = &buf[1];

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
	}
	return ret;
}

void CTP_I2C_WRITE (struct i2c_client *client, uint8_t address, uint8_t *data, uint8_t len)
{
	struct i2c_msg msg;
	int ret = -1;
	int retries = 0;

	msg.flags = !I2C_M_RD;
	msg.addr  = address;
	msg.len   = len;
	msg.buf   = data;

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			break;
		retries++;
	}
	return;
}


/*******************************************************
  IC Reset using GPIO trigger
*******************************************************/
void nvt_hw_reset(void)
{
	gpio_direction_output(sprd_3rdparty_gpio_tp_rst, 1);
	msleep(20);
	gpio_direction_output(sprd_3rdparty_gpio_tp_rst, 0);
	msleep(10);
	gpio_direction_output(sprd_3rdparty_gpio_tp_rst, 1);
}


/*******************************************************
  Create Device Node (Proc Entry)
*******************************************************/
#if NVT_TOUCH_CTRL_DRIVER

#define DEVICE_NAME	"NVTflash"
ssize_t nvt_flash_write(struct file *file, const char __user *buff, size_t count, loff_t *offp)
{
	struct i2c_msg msgs[2];
	char *str;
	int ret = -1;
	int retries = 0;
	file->private_data = (uint8_t *)kmalloc(64, GFP_KERNEL);
	str = file->private_data;
	if (copy_from_user(str, buff, count))
		return -EFAULT;

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = str[0];
	msgs[0].len   = str[1];
	msgs[0].buf   = &str[2];


	if (str[0] == 0x70) {
		if (str[2] == 0x00 && str[3] == 0x5A) {
			nvt_hw_reset();
			return 1;
		}
	}

	while (retries < 20) {
		ret = i2c_transfer(ts->client->adapter, msgs, 1);
		if (ret == 1)
			break;
		else
			printk("%s error, retries=%d\n", __func__, retries);

		retries++;
	}
	return ret;
}

ssize_t nvt_flash_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	struct i2c_msg msgs[2];
	char *str;
	int ret = -1;
	int retries = 0;
	file->private_data = (uint8_t *)kmalloc(64, GFP_KERNEL);
	str = file->private_data;
	if (copy_from_user(str, buff, count))
		return -EFAULT;

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = str[0];
	msgs[0].len   = 1;
	msgs[0].buf   = &str[2];

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = str[0];
	msgs[1].len   = str[1]-1;
	msgs[1].buf   = &str[3];

	while (retries < 20) {
		ret = i2c_transfer(ts->client->adapter, msgs, 2);
		if (ret == 2)
			break;
		else
			printk("%s error, retries=%d\n", __func__, retries);

		retries++;
	}


	if (retries < 20) {
		if (copy_to_user(buff, str, count))
			return -EFAULT;
	}
	return ret;
}

int nvt_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev;

	dev = kmalloc(sizeof(struct nvt_flash_data), GFP_KERNEL);
	if (dev == NULL)
		return -ENOMEM;

	rwlock_init(&dev->lock);
	file->private_data = dev;

	return 0;
}

int nvt_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev = file->private_data;

	if (dev)
		kfree(dev);

	return 0;
}

struct file_operations nvt_flash_fops = {
	.owner = THIS_MODULE,
	.open = nvt_flash_open,
	.release = nvt_flash_close,
	.write = nvt_flash_write,
	.read = nvt_flash_read,
};
static int nvt_flash_init(void)
{

	proc_create(DEVICE_NAME, 0666, NULL, &nvt_flash_fops);

	printk("============================================================\n");
	printk("NVT_flash driver loaded\n");
	printk("============================================================\n");
	return 0;
}
#endif

/*******************************************************
  Auto Update FW in Probe
*******************************************************/
#if BOOT_UPDATE_FIRMWARE
int Check_FW_Ver(void)
{
	uint8_t I2C_Buf[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	I2C_Buf[0] = 0x78;
	CTP_I2C_READ(ts->client, 0x01, I2C_Buf, 2);
	dev_info(&ts->client->dev, "IC FW Ver = %d\n", I2C_Buf[1]);
	dev_info(&ts->client->dev, "Bin FW Ver = %d\n", BUFFER_DATA[0x7F00]);
	printk("IC FW Ver = %d\n", I2C_Buf[1]);
	printk("Bin FW Ver = %d\n", BUFFER_DATA[0x7F00]);
	if (I2C_Buf[1] > BUFFER_DATA[0x7F00])
		return 1;
	else
		return 0;
}

int Check_CheckSum(void)
{
	uint8_t I2C_Buf[64];
	uint8_t buf2[64];
	int i, j, k, Retry_Counter = 0;
	int addr = 0;
	uint8_t addrH, addrL;
	unsigned short RD_Filechksum, WR_Filechksum;

	WR_Filechksum = 0;

	I2C_Buf[0] = 0x00;
	I2C_Buf[1] = 0x5A;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, I2C_Buf, 2);

	msleep(1000);

	I2C_Buf[0] = 0xFF;
	I2C_Buf[1] = 0x3F;
	I2C_Buf[2] = 0xE8;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, I2C_Buf, 3);

	I2C_Buf[0] = 0x00;
	I2C_Buf[1] = 0xEA;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, I2C_Buf, 2);

	addr = 0;
	for (i = 0; i < (BUFFER_LENGTH)/128; i++) {
		for (j = 0; j < 16; j++) {
			unsigned char tmp = 0;
			addrH = addr>>8;
			addrL = addr&0xFF;
			for (k = 0; k < 8; k++) {
				tmp += BUFFER_DATA[i * 128 + j * 8 + k];
			}
			tmp = tmp + addrH + addrL + 8;
			tmp = (255-tmp) + 1;
			WR_Filechksum += tmp;
			addr += 8;
		}
	}

	msleep(800);

	do {
		msleep(10);
		I2C_Buf[0] = 0xFF;
		I2C_Buf[1] = 0x3F;
		I2C_Buf[2] = 0xF8;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, I2C_Buf, 3);

		buf2[0] = 0x00;
		buf2[1] = 0x00;
		buf2[2] = 0x00;
		buf2[3] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf2, 4);

		Retry_Counter++;
		msleep(10);

	} while ((Retry_Counter < 20) && (buf2[1] != 0xAA));

	if (buf2[1] == 0xAA) {
		RD_Filechksum = (buf2[2]<<8) + buf2[3];
		if (RD_Filechksum == WR_Filechksum) {
			dev_info(&ts->client->dev, "%s : firmware checksum match.\n", __func__);
			return 1;
		} else {
			dev_info(&ts->client->dev, "%s : firmware checksum not match!!\n", __func__);
			return 0;
		}
	} else {
		dev_info(&ts->client->dev, "%s : read firmware checksum timeout!!\n", __func__);
		return -EPERM;
	}
}

void Update_Firmware(void)
{
	uint8_t I2C_Buf[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int i = 0;
	int j = 0;
	unsigned int Flash_Address = 0;
	unsigned int Row_Address = 0;
	uint8_t CheckSum[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	struct i2c_client *client = ts->client;
	int ret;

	I2C_Buf[0] = 0x00;
	I2C_Buf[1] = 0xA5;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, I2C_Buf, 2);

	msleep(2);

	I2C_Buf[0] = 0x00;
	I2C_Buf[1] = 0x00;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, I2C_Buf, 2);

	msleep(20);

	I2C_Buf[0] = 0x00;
	CTP_I2C_READ(ts->client, I2C_HW_Address, I2C_Buf, 2);
	if (I2C_Buf[1] != 0xAA) {
		dev_info(&client->dev, "Program: init get status(0x%2X) error.", I2C_Buf[1]);
		return;
	}
	dev_info(&client->dev, "Program: init get status(0x%2X) success.", I2C_Buf[1]);

	I2C_Buf[0] = 0x00;
	I2C_Buf[1] = 0x66;
	I2C_Buf[2] = 0x00;
	I2C_Buf[3] = 0x0E;
	I2C_Buf[4] = 0x01;
	I2C_Buf[5] = 0xB4;
	I2C_Buf[6] = 0x3D;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, I2C_Buf, 7);

	while (1) {
		msleep(1);
		CTP_I2C_READ(ts->client, I2C_HW_Address, I2C_Buf, 2);
		if (I2C_Buf[1] == 0xAA)
			break;
	}

	I2C_Buf[0] = 0x00;
	I2C_Buf[1] = 0x66;
	I2C_Buf[2] = 0x00;
	I2C_Buf[3] = 0x0F;
	I2C_Buf[4] = 0x01;
	I2C_Buf[5] = 0xEF;
	I2C_Buf[6] = 0x01;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, I2C_Buf, 7);

	while (1) {
		msleep(1);
		CTP_I2C_READ(ts->client, I2C_HW_Address, I2C_Buf, 2);
		if (I2C_Buf[1]==0xAA)
			break;
	}


	for (i = 0; i < BUFFER_LENGTH/4096; i++) {
		Row_Address = i * 4096;

		I2C_Buf[0] = 0x00;
		I2C_Buf[1] = 0x33;
		I2C_Buf[2] = (uint8_t)((Row_Address & 0xFF00) >> 8);
		I2C_Buf[3] = (uint8_t)(Row_Address & 0x00FF);
		CTP_I2C_WRITE(ts->client, I2C_HW_Address, I2C_Buf, 4);
		msleep(15);

		CTP_I2C_READ(ts->client, I2C_HW_Address, I2C_Buf, 2);

		if (I2C_Buf[1] != 0xAA) {
			dev_info(&client->dev, "Program: erase(0x%02X) error.", I2C_Buf[1]);
			return;
		}
	}

	dev_info(&client->dev, "Program: erase(0x%02X) success.", I2C_Buf[1]);

	Flash_Address = 0;

	dev_info(&client->dev, "Program: write begin, please wait...");

	for (j = 0; j < BUFFER_LENGTH/128; j++) {
		Flash_Address = (j)*128;

		for (i = 0; i < 16 ; i++, Flash_Address += 8) {

			I2C_Buf[0] = 0x00;
			I2C_Buf[1] = 0x55;
			I2C_Buf[2] = (uint8_t)(Flash_Address  >> 8);
			I2C_Buf[3] = (uint8_t)(Flash_Address & 0xFF);
			I2C_Buf[4] = 0x08;
			I2C_Buf[6] = BUFFER_DATA[Flash_Address + 0];
			I2C_Buf[7] = BUFFER_DATA[Flash_Address + 1];
			I2C_Buf[8] = BUFFER_DATA[Flash_Address + 2];
			I2C_Buf[9] = BUFFER_DATA[Flash_Address + 3];
			I2C_Buf[10] = BUFFER_DATA[Flash_Address + 4];
			I2C_Buf[11] = BUFFER_DATA[Flash_Address + 5];
			I2C_Buf[12] = BUFFER_DATA[Flash_Address + 6];
			I2C_Buf[13] = BUFFER_DATA[Flash_Address + 7];

			CheckSum[i] = ~(I2C_Buf[2] + I2C_Buf[3] + I2C_Buf[4] + I2C_Buf[6] + I2C_Buf[7] +
						  I2C_Buf[8] + I2C_Buf[9] + I2C_Buf[10] + I2C_Buf[11] + I2C_Buf[12] +
						  I2C_Buf[13]) + 1;

			I2C_Buf[5] = CheckSum[i];
			CTP_I2C_WRITE(ts->client, I2C_HW_Address, I2C_Buf, 14);
		}
		msleep(10);


		I2C_Buf[0] = 0x00;
		while (1) {
			CTP_I2C_READ(ts->client, I2C_HW_Address, I2C_Buf, 2);
			if (I2C_Buf[1]==0xAA)
				break;
		}
	}

	dev_info(&client->dev, "Program: Verify begin, please wait...");
	ret = Check_CheckSum();
	if (ret == 1)
		dev_info(&client->dev, "Program: Verify Pass!");
	else if (ret == 0)
		dev_info(&client->dev, "Program: Verify NG!");
	else if (ret == -1)
		dev_info(&client->dev, "Program: Verify FW not return!");

	nvt_hw_reset();

	msleep(500);
	dev_info(&client->dev, "Program: END");
}

void Boot_Auto_Update_Firmware(void)
{
	int ret = 0;

	ret = Check_CheckSum();

	nvt_hw_reset();
	msleep(500);

	if (ret == -1) {
		Update_Firmware();
	} else if (ret == 0 && (Check_FW_Ver() == 0)) {
		Update_Firmware();
	}

}

void Boot_Update_Firmware(struct work_struct *work)
{
	struct i2c_client *client = ts->client;
	int ret = 0;

	ret = Check_CheckSum();

	nvt_hw_reset();
	msleep(500);

	if (ret == -1) {
		Update_Firmware();
	} else if (ret == 0 && (Check_FW_Ver() == 0)) {
		dev_info(&client->dev, "%s : firmware version not match.\n", __func__);
		Update_Firmware();
	}

}
#endif


#if defined(CONFIG_TOUCHSCREEN_WITH_PROXIMITY)
static char prox_enable;

static ssize_t proximity_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	dev_info(&ts->client->dev, "%s\n", __func__);
	return sprintf(buf, "%d\n", prox_enable);
}

static ssize_t proximity_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long on_off = simple_strtoul(buf, NULL, 10);
	uint8_t buf1[2] = {0};

	on_off = (on_off > 0) ? 1 : 0;

	if (prox_enable != on_off) {
		if (on_off == 0) {

			dev_info(&ts->client->dev, "Disable touch proximity.\n");
			buf1[0] = 0xA4;
			buf1[1] = 0x00;
			CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf1, 2);
		} else {
			dev_info(&ts->client->dev, "Enable touch proximity.\n");
			buf1[0] = 0xA4;
			buf1[1] = 0x01;
			CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf1, 2);
		}

		prox_enable = on_off;
	}

	return size;
}

static DEVICE_ATTR(enable, 0777, proximity_enable_show, proximity_enable_store);

static struct attribute *proximity_attributes[] = {
	&dev_attr_enable.attr,
	NULL
};

static const struct attribute_group proximity_attr_group = {
	.attrs = proximity_attributes,
};

static void nvt_ts_proximity_report(uint8_t prox_status)
{
	switch (prox_status) {
	case 0x01:
		dev_info(&ts->client->dev, "Proximity is near.\n");
		input_report_abs(ts->input_dev, ABS_DISTANCE, 0);
		input_sync(ts->input_dev);
		break;
	case 0x03:
		dev_info(&ts->client->dev, "Proximity is far.\n");
		input_report_abs(ts->input_dev, ABS_DISTANCE, 1);
		input_sync(ts->input_dev);
		break;

	default:
		break;
	}
}
#endif

#if WAKEUP_GESTURE
#define GESTURE_WORD_C			12
#define GESTURE_WORD_W			13
#define GESTURE_WORD_V			14
#define GESTURE_DOUBLE_CLICK	15
#define GESTURE_WORD_Z			16
#define GESTURE_WORD_M			17
#define GESTURE_WORD_O			18
#define GESTURE_WORD_e			19
#define GESTURE_WORD_S			20
#define GESTURE_SLIDE_UP		21
#define GESTURE_SLIDE_DOWN		22
#define GESTURE_SLIDE_LEFT		23
#define GESTURE_SLIDE_RIGHT		24

static struct wake_lock gestrue_wakelock;

static unsigned char bTouchIsAwake = 1;
void nvt_ts_wakeup_gesture_report(unsigned char gesture_id)
{
	struct i2c_client *client = ts->client;
	unsigned int keycode = 0;

	switch (gesture_id) {
	case GESTURE_WORD_C:
		dev_info(&client->dev, "Gesture : Word-C.\n");
		keycode = gesture_key_array[0];
		break;
	case GESTURE_WORD_W:
		dev_info(&client->dev, "Gesture : Word-W.\n");
		keycode = gesture_key_array[1];
		break;
	case GESTURE_WORD_V:
		dev_info(&client->dev, "Gesture : Word-V.\n");
		keycode = gesture_key_array[2];
		break;
	case GESTURE_DOUBLE_CLICK:
		dev_info(&client->dev, "Gesture : Double Click.\n");
		keycode = gesture_key_array[3];
		break;
	case GESTURE_WORD_Z:
		dev_info(&client->dev, "Gesture : Word-Z.\n");
		keycode = gesture_key_array[4];
		break;
	case GESTURE_WORD_M:
		dev_info(&client->dev, "Gesture : Word-M.\n");
		keycode = gesture_key_array[5];
		break;
	case GESTURE_WORD_O:
		dev_info(&client->dev, "Gesture : Word-O.\n");
		keycode = gesture_key_array[6];
		break;
	case GESTURE_WORD_e:
		dev_info(&client->dev, "Gesture : Word-e.\n");
		keycode = gesture_key_array[7];
		break;
	case GESTURE_WORD_S:
		dev_info(&client->dev, "Gesture : Word-S.\n");
		keycode = gesture_key_array[8];
		break;
	case GESTURE_SLIDE_UP:
		dev_info(&client->dev, "Gesture : Slide UP.\n");
		keycode = gesture_key_array[9];
		break;
	case GESTURE_SLIDE_DOWN:
		dev_info(&client->dev, "Gesture : Slide DOWN.\n");
		keycode = gesture_key_array[10];
		break;
	case GESTURE_SLIDE_LEFT:
		dev_info(&client->dev, "Gesture : Slide LEFT.\n");
		keycode = gesture_key_array[11];
		break;
	case GESTURE_SLIDE_RIGHT:
		dev_info(&client->dev, "Gesture : Slide RIGHT.\n");
		keycode = gesture_key_array[12];
		break;
	default:
		dev_info(&client->dev, "Wrong Gesture!! ID=%d\n", gesture_id);
		break;
	}

	if (keycode > 0) {
		input_report_key(ts->input_dev, keycode, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, keycode, 0);
		input_sync(ts->input_dev);
	}
	msleep(250);
}
#endif


uint8_t nvt_read_firmware_version(void)
{
	uint8_t I2C_Buf[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	I2C_Buf[0] = 0x78;
	CTP_I2C_READ(ts->client, 0x01, I2C_Buf, 2);
	dev_info(&ts->client->dev, "IC FW Ver = %d\n", I2C_Buf[1]);
	printk("IC FW Ver = %d\n", I2C_Buf[1]);
	return I2C_Buf[1];
}

#if CTP_WT_TP_OPENSHORT_TEST
void nvt_sw_reset(void)
{
	buffer[0] = 0x00;
	buffer[1] = 0x5A;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, buffer, 2);
}

void nvt_sw_reset_idle(void)
{
	buffer[0] = 0x00;
	buffer[1] = 0xA5;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, buffer, 2);
}

int GPIOShort_AllCheck(void)
{
	unsigned short TimeoutCnt0, TimeoutCnt1;

	TimeoutCnt1 = 0;

Short_AllCheck:
	TimeoutCnt0 = 0;

	buffer[0] = 0xFF;
	buffer[1] = 0x3F;
	buffer[2] = 0xE8;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
	msleep(1);

	buffer[0] = 0x00;
	buffer[1] = 0xC5;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 2);
	msleep(100);

	while(1) {
		buffer[0] = 0xFF;
		buffer[1] = 0x3F;
		buffer[2] = 0xE9;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
		msleep(1);

		buffer[0] = 0x00;
		buffer[1] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buffer, 8);
		if (buffer[1] == 0xBB)
			break;
		msleep(1);
		TimeoutCnt0++;
		{
			TimeoutCnt1++;
			if (TimeoutCnt1 > 3)
				return 1;
			else
				goto Short_AllCheck;
		}
	}
	return 0;
}

char *GPIO2AIN2TxRx(unsigned char GPIO)
{
	unsigned char  ii, jj;
	if (AIN[GPIO] == -1) {
		if (!iRecord_AIN)
			sprintf(FAIL, "UnusedPin, ");
		else
			sprintf(FAIL, "UnusedPin%d[AIN%d], ", (AIN[GPIO]-32), (GPIO));
	} else
	if (AIN[GPIO] >= 32) {
		if (!iRecord_AIN)
			sprintf(FAIL, "Rx%d, ", (AIN[GPIO]-32));
		else
			sprintf(FAIL, "Rx%d[AIN%d], ", (AIN[GPIO]-32), (GPIO));

		for (jj = 0; jj < YNum; jj++) {
			for (ii = 0; ii < XNum; ii++) {
				if (MappingTable[jj][ii].Rx == (AIN[GPIO]-32)) {
					GridColor_Flag[ii][jj] = cPurople;
				}
			}
		}

	} else {
		if (!iRecord_AIN)
			sprintf(FAIL, "Tx%d, ", (AIN[GPIO]));
		else
			sprintf(FAIL, "Tx%d[AIN%d], ", (AIN[GPIO]), (GPIO));

		for (jj = 0; jj < YNum; jj++) {
			for (ii = 0; ii < XNum; ii++) {
				if (MappingTable[jj][ii].Tx == (AIN[GPIO])) {
					GridColor_Flag[ii][jj] = cPurople;
				}
			}
		}
	}
	return FAIL;

}

static ssize_t nvt_sysfs_sensor_short_test_show(void)
{
	unsigned char Port;
	unsigned char GPIO_ShortList[48];
	unsigned char GPIO_ShortCnt = 0;
	int i, j;
	int ret = 0;

	memset(GPIO_ShortList, 0xff, sizeof(GPIO_ShortList));
	GPIO_ShortCnt = 0;

	CTP_INFO("nvt_read_firmware_version");
	nvt_read_firmware_version();

	memset(GPIOMSG, 0, 2048);

	nvt_hw_reset();
	msleep(500);

	buffer[0] = 0xFF;
	buffer[1] = 0x3F;
	buffer[2] = 0xF4;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
	msleep(1);

	buffer[0] = 0x00;
	buffer[1] = 255;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 2);
	msleep(1);

	CTP_INFO("GPIOShort_AllCheck");
	if (GPIOShort_AllCheck() > 0) {
		sprintf(GPIOMSG, "FW is Timeout!");
		return 0;
	}
	for (i = 0; i < 6; i++) {

		if (buffer[i + 2] > 0) {
			Port = buffer[i+2];
			for (j = 0; j < 8; j++) {
				unsigned char TestPin;
				TestPin = (0x01 << j);
				if ((Port & TestPin) > 0) {
					unsigned char FailAIN;
					FailAIN = (i * 8) + j;
					if (AIN[FailAIN] == -1) {
						continue;
					} else {
						GPIO_ShortList[GPIO_ShortCnt] = FailAIN;
						GPIO_ShortCnt++;
					}

				}

			}
		}
	}

	ret = 0;

	sprintf(GPIOMSG, "{");
	for (j = 0; j < GPIO_ShortCnt; j++) {
		i = GPIO_ShortList[j];
		if (i >= 0xff) {
			continue;
		} else
		if (AIN[i] == -1) {
			continue;
		}

		sprintf(GPIOMSG, "%s%s", GPIOMSG, GPIO2AIN2TxRx(i));
		ret++;
	}

	if (ret <= 1) {
		sprintf(GPIOMSG, "%sGND,}, ", GPIOMSG);
	} else {
		sprintf(GPIOMSG, "%s}, ", GPIOMSG);
	}

	nvt_hw_reset();
	if (ret > 0) {
			  CTP_INFO("Self Open Test FAIL!");
		return 0;
	} else {
			 CTP_INFO("Self Open Test PASS!");
		return 1;
	}
}

int EnterTestMode(void)
{
	int TimeoutCnt0 = 0, TimeoutCnt1 = 0;

lEnterTestMode:
	buffer[0] = 0xFF;
	buffer[1] = 0x3F;
	buffer[2] = 0xE8;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
	msleep(1);

	buffer[0] = 0x00;
	buffer[1] = 0xCC;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 2);
	msleep(500);

	while(1) {
		buffer[0] = 0xFF;
		buffer[1] = 0x3F;
		buffer[2] = 0xE9;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
		msleep(1);

		buffer[0] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buffer, 2);
		if (buffer[1] == 0xBB) {
			break;
		} else {
			msleep(1);
			TimeoutCnt0++;
			if (TimeoutCnt0 > 100) {
				TimeoutCnt1++;
				TimeoutCnt0 = 0;
				if (TimeoutCnt1 > 3)
					return 1;
				else
					goto lEnterTestMode;
			}
		}
	}

	return 0;
}

void LeaveTestMode(void)
{
	unsigned char buffer[64] = {0};
	buffer[0] = 0xFF;
	buffer[1] = 0x3F;
	buffer[2] = 0xE9;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
	msleep(1);

	buffer[0] = 0x00;
	buffer[1] = 0xAA;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 2);
	msleep(1);
}

void GetTP_CMParameter(void)
{
	nvt_hw_reset();
	msleep(500);

	buffer[0] = 0xFF;
	buffer[1] = 0x3F;
	buffer[2] = 0xB7;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
	msleep(1);

	buffer[0] = 0x00;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buffer, 25);

	iAccum =  buffer[1];
	fCF = CF_table[buffer[9] & 0x07];
	fCC = CC_table[((buffer[9] & 0xF8) >> 4) | (((buffer[9] & 0xF8) & 0x08) << 1)];

	LeaveTestMode();
}

int CheckFWStatus(void)
{
	int i;

	for (i = 0; i<100; i++) {
		msleep(1);
		buffer[0] = 0xFF;
		buffer[1] = 0x3D;
		buffer[2] = 0xFB;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
		msleep(1);

		buffer[0] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buffer, 2);

		if (buffer[1] == 0xAA)
			break;
		msleep(10);
	}

	if (i == 100) {
		return -EPERM;
	} else
		return 0;
}

void ReadRaw_NT11205(void)
{
	unsigned int startAddr;
	int i, j, k, m;
	int bytecount, sec_ct, Residual_1, sec_ct2, Residual_2, temp_cnt, offsetAddr;

	temp_cnt = 0;
	bytecount = XNum*YNum*2;
	sec_ct = bytecount/244;
	Residual_1 = bytecount%244;
	sec_ct2 = Residual_1/61;
	Residual_2 = Residual_1%61;
	startAddr = 0x0800;

	for (m = 0; m < sec_ct; m++) {
		offsetAddr = 0;
		buffer[0] = 0xFF;
		buffer[1] = startAddr>>8;
		buffer[2] = startAddr&0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
		msleep(1);

		for (k = 0; k < 4; k++) {
			buffer[0] = offsetAddr&0xFF;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buffer, 62);
			offsetAddr += 61;
			for (i = 0; i < 61; i++) {
				DiffTemp[temp_cnt++] = (int)buffer[1 + i];
			}
		}
		startAddr += offsetAddr;
	}

	if (Residual_1 > 0) {
		offsetAddr = 0;
		buffer[0] = 0xFF;
		buffer[1] = startAddr>>8;
		buffer[2] = startAddr&0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
		msleep(1);

		for (k = 0; k < sec_ct2; k++) {
			buffer[0] = offsetAddr&0xFF;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buffer, 62);
			offsetAddr += 61;
			for (i = 0; i < 61; i++) {
				DiffTemp[temp_cnt++] = (int)buffer[1 + i];
			}
		}
		startAddr += offsetAddr;
		if (Residual_2 > 0) {
			offsetAddr = 0;
			buffer[0] = 0xFF;
			buffer[1] = startAddr>>8;
			buffer[2] = startAddr&0xFF;
			CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
			msleep(1);

			buffer[0]=offsetAddr&0xFF;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buffer, Residual_2+1);
			for (i = 0; i < Residual_2; i++) {
				DiffTemp[temp_cnt++] = (int)buffer[1 + i];
			}
		}
	}

	for (j = 0; j < YNum; j++) {
		for (i = 0; i < XNum; i++) {
			Mutual_Data[j*XNum + i] = (unsigned short)((DiffTemp[2*(j*XNum + i)]<<8)|DiffTemp[2*(j*XNum + i) + 1]);
		}
	}
}

void RawDataToCM(void)
{
		int i, j;
		int kk = 0;
		int RepeatCnt = 0;
		int temp1;

AgainGetData:

		nvt_hw_reset();
		msleep(500);

		buffer[0] = 0xFF;
		buffer[1] = 0x3F;
		buffer[2] = 0xE8;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
		msleep(1);
		buffer[0] = 0x00;
		buffer[1] = 0xCF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 2);
		msleep(500);

		buffer[0] = 0xFF;
		buffer[1] = 0x3D;
		buffer[2] = 0xFC;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
		msleep(1);
		buffer[0] = 0x00;
		buffer[1] = 0xAA;
		buffer[2] = 0x5A;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buffer, 3);
		msleep(500);

		for (j = 0; j < YNum; j++) {
			for (i = 0; i < XNum; i++) {
				RawDataTmp[j*XNum + i] = Mutual_Data[j*XNum + i];
			}
		}

		if (CheckFWStatus() == -1)
			return;

		ReadRaw_NT11205();

		kk = 0;
		for (j = 0; j < YNum; j++) {
			for (i = 0; i < XNum; i++) {
				if (abs(RawDataTmp[j*XNum + i] - Mutual_Data[j*XNum + i] > 1000)) {
					kk++;
					if (kk > 2) {
						RepeatCnt++;
						goto AgainGetData;
					}
				}
			}
		}

		temp1 = (int)(((2*(long int)fCC*10/3)*10000/fCF));

		for (j = 0; j < YNum; j++) {
			for (i = 0; i < XNum; i++) {
				CM_Data[XNum*j + i] = (int)((((((((Mutual_Data[XNum*j + i]/((iAccum+1)/2))-1024)*24)+temp1)/2)*3/10)*fCF/100));
			}
		}
}

int Test_CaluateGRatioAndNormal(void)
{
	int i, j, k;
	long int tmpValue;
	long int MaxSum = 0;
	int MaxNum = 0, MaxIndex = 0;
	int Max = -9999999;
	int Min =  9999999;
	int offset;
	int Data;

	for (j = 0; j < YNum; j++) {
		for (i = 0; i < XNum; i++) {
			Data = CM_Data[j*XNum + i];
			if (Data == 0)
				Data = 1;
			Mutual_GoldenRatio[j][i] = Data - Mutual_AVG[j*XNum + i];
			Mutual_GoldenRatio[j][i] /= Data ;
		}
	}

	for (j = 0; j < YNum; j++) {
		for (i = 0; i < XNum; i++) {
				Mutual_GoldenRatio[j][i] *=  10000;
			}
	}

	for (j = 0; j < YNum; j++) {
		for (i = 0; i < XNum; i++) {
			if (Max < (int)Mutual_GoldenRatio[j][i])
				Max  =  (int)Mutual_GoldenRatio[j][i];
			if (Min > (int)Mutual_GoldenRatio[j][i])
				Min = (int)Mutual_GoldenRatio[j][i];
		}
	}

	offset = 0;
	if (Min < 0) {
		offset =  0 - Min ;
		offset += StatisticsStep;
		for (j = 0; j < YNum; j++) {
			for (i = 0; i < XNum; i++) {
				Mutual_GoldenRatio[j][i] += offset;
			}
		}
		Max += offset;
	}
	StatisticsStep = Max / MaxStatisticsBuf;
	StatisticsStep += 1;
	if (StatisticsStep < 0)
		return 1;

	memset(StatisticsSum, 0 , sizeof(long int)*MaxStatisticsBuf);
	memset(StatisticsNum, 0 , sizeof(int) * MaxStatisticsBuf);
	for (j = 0;  j < YNum; j++) {
		for (i = 0; i < XNum; i++) {
			tmpValue = (long int)Mutual_GoldenRatio[j][i];
			tmpValue /= StatisticsStep;
			StatisticsNum[tmpValue] += 2;
			StatisticsSum[tmpValue] += (2 * (long int)Mutual_GoldenRatio[j][i]);

			if ((tmpValue + 1) <  MaxStatisticsBuf) {
				StatisticsNum[tmpValue+1] += 1;
				StatisticsSum[tmpValue+1] += (long int)Mutual_GoldenRatio[j][i] ;
			}
			if ((tmpValue - 1) <  MaxStatisticsBuf) {
				StatisticsNum[tmpValue - 1] += 1;
				StatisticsSum[tmpValue - 1] += (long int)Mutual_GoldenRatio[j][i];
			}
		}
	}

	MaxNum = 0;
	for (k = 0; k < MaxStatisticsBuf; k++) {
		if (MaxNum < StatisticsNum[k]) {
			MaxSum = StatisticsSum[k];
			MaxNum = StatisticsNum[k];
			MaxIndex = k;
		}
	}

	if (MaxSum > 0) {
		tmpValue = (long int)(StatisticsSum[MaxIndex] / (long int)StatisticsNum[MaxIndex]) * 2;
		if ((MaxIndex+1) < (MaxStatisticsBuf))
		tmpValue += (long int)(StatisticsSum[MaxIndex+1] / (long int)StatisticsNum[MaxIndex+1]);
		if ((MaxIndex-1) >= 0)
		tmpValue += (long int)(StatisticsSum[MaxIndex-1] / (long int)StatisticsNum[MaxIndex-1]);

		if ((MaxIndex+1) < (MaxStatisticsBuf) && ((MaxIndex-1) >= 0))
		tmpValue /= 4;
		else
		tmpValue /= 3;
	} else {
		StatisticsSum[0] = 0;
		StatisticsNum[0] = 0;
		for (j = 0; j < YNum; j++) {
			for (i = 0; i < XNum; i++) {
				StatisticsSum[0] += (long int)Mutual_GoldenRatio[j][i];
				StatisticsNum[0]++;
			}
		}
		tmpValue = StatisticsSum[0] / StatisticsNum[0];
	}

	tmpValue -= offset;
	for (j = 0; j < YNum; j++) {
		for (i = 0; i < XNum;  i++) {
			Mutual_GoldenRatio[j][i] -= offset;

			Mutual_GoldenRatio[j][i] =  Mutual_GoldenRatio[j][i] - tmpValue;
			Mutual_GoldenRatio[j][i] =  Mutual_GoldenRatio[j][i] / 10000;
		}
	}

	return 0;
}


static ssize_t nvt_sysfs_sensor_open_test_show(void)
{
	int i, j;
	int kk = 0;

	CTP_INFO("nvt_read_firmware_version");
	nvt_read_firmware_version();

	nvt_hw_reset();
	msleep(500);

	for (j = 0; j < YNum; j++) {
		for (i = 0; i < XNum; i++) {
			if (GridColor_Flag[i][j]  != cWhite)
			RecordResult[j*XNum + i] =  0x80;
			else
			RecordResult[j*XNum + i] =  0x00;
		}
	}

	CTP_INFO("Get RawData and To CM ");
	GetTP_CMParameter();
	RawDataToCM();

	printk("XNum=%d\n", XNum);
	printk("YNum=%d\n", YNum);
	printk("iAccum=%d\n", iAccum);
	printk("fCC=%d\n", fCC);
	printk("fCF=%d\n", fCF);

	printk("\nMutual_Data\n");
	for (j = 0; j < YNum; j++) {
		for (i = 0; i < XNum; i++) {
			printk("%6d, ", Mutual_Data[XNum*j + i]);
		}
		printk("\n");
	}
	printk("\n");


	printk("CM_Data\n");
	for (j = 0; j < YNum; j++) {
		for (i = 0; i < XNum; i++) {
			CM_Data[XNum*j + i] -= FPC_CM[XNum*j + i];
			printk("%6d, ", CM_Data[XNum*j + i]);
		}
		printk("\n");
	}
	printk("\n");


	for (j = 0; j < YNum;  j++) {
		for (i = 0; i < XNum ; i++) {
			if (CM_Data[j*XNum + i] == 0)
				 CM_Data[j*XNum + i] = 1;
			else if (CM_Data[j*XNum + i] < 0)
				 CM_Data[j*XNum + 1] = 1;
		}
	}


	for (j = 0; j < YNum; j++) {
		for (i = 0; i < XNum; i++) {
			kk = (int)((Mutual_AVG[j*XNum + i]*(1000 - iTolerance_S))/1000);
			if (CM_Data[j*XNum + i] < kk) {
				RecordResult[j*XNum + i] |= 1;
			}

			kk = (int)((Mutual_AVG[j*XNum + i]*(1000 + iPostiveTolerance))/1000);
			if (CM_Data[j*XNum + i] > kk) {
				RecordResult[j*XNum + i] |= 1;
			}
		}
	}

	CTP_INFO("Test_CaluateGRatioAndNormal");
	Test_CaluateGRatioAndNormal();

	for (j = 0; j < YNum; j++) {
		for (i = 0; i < XNum; i++) {
			if (Mutual_GoldenRatio[j][i]*1000 < iDiffLimitG)
				RecordResult[j*XNum + i] |=  0x02;
		}
	}

	for (j = 0; j < YNum; j++) {
		for (i = 0; i < XNum; i++)	{
			kk = 0;
			if ((RecordResult[j*XNum + i] & 0x01) > 0)
				kk++;
			if ((RecordResult[j*XNum + i] & 0x02) > 0)
				kk++;
		}
	}

	nvt_hw_reset();
	if (kk >= 1) {
		CTP_INFO("Self Open Test FAIL!");
		return 0;
	} else {
		CTP_INFO("Self Open Test PASS!");
		return 1;
	}
}

static ssize_t ctp_open_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t ctp_open_proc_write(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos);
static const struct file_operations ctp_open_procs_fops = {
	.write = ctp_open_proc_write,
	.read = ctp_open_proc_read,
	.owner = THIS_MODULE,
};


static ssize_t ctp_open_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	char *ptr = buf;

	u8 resultopen = 0;
	u8 resultclose = 0;
	u8 result = 0;

	if (*ppos) {
		CTP_INFO("tp test again return\n");
		return 0;
	}
	*ppos += count;

	CTP_INFO("shorttest read");
	resultclose = nvt_sysfs_sensor_short_test_show();

	CTP_INFO("opentest read");
	resultopen = nvt_sysfs_sensor_open_test_show();

	if (resultclose && resultopen) {
		result = 1;
	} else {
	   result = 0;
	}

	return  sprintf(ptr, "result=%d\n", result);
}


static ssize_t ctp_open_proc_write(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos)
{
	return -EPERM;
}


static void create_ctp_proc(void)
{
	struct proc_dir_entry *ctp_device_proc = NULL;
	struct proc_dir_entry *ctp_open_proc = NULL;

	ctp_device_proc = proc_mkdir(CTP_PARENT_PROC_NAME, NULL);
	if (ctp_device_proc == NULL) {
		CTP_ERROR("NVTtouch_205: create parent_proc fail\n");
		return;
	}

	ctp_open_proc = proc_create(CTP_OPEN_PROC_NAME, 0666, ctp_device_proc, &ctp_open_procs_fops);
	if (ctp_open_proc == NULL) {
		CTP_ERROR("NVTtouch_205: create open_proc fail\n");
	}


}
#endif

/*******************************************************
Description:
	Novatek touchscreen work function.

Parameter:
	ts:	i2c client private struct.

return:
	Executive outcomes.0---succeed.
*******************************************************/
static void nvt_ts_work_func(struct work_struct *work)
{

	struct i2c_client *client = ts->client;

	int ret = -1;
	uint8_t  point_data[(TOUCH_MAX_FINGER_NUM*6)+2+1] = {0};
	unsigned int position = 0;
	unsigned int input_x = 0;
	unsigned int input_y = 0;
	unsigned char input_w = 0;
	unsigned char input_id = 0;

	int i;
	int finger_cnt = 0;

	ret = CTP_I2C_READ(ts->client, I2C_FW_Address, point_data, ts->max_touch_num*6+2+1);

	if (ret < 0) {
		dev_info(&client->dev, "%s: CTP_I2C_READ failed.\n", __func__);
		goto XFER_ERROR;
	}

	input_id = (unsigned int)(point_data[1]>>3);

#if WAKEUP_GESTURE
	if (bTouchIsAwake == 0) {
		nvt_ts_wakeup_gesture_report(input_id);
		enable_irq(ts->client->irq);
		return;
	}
#endif

#if defined(CONFIG_TOUCHSCREEN_WITH_PROXIMITY)
	if (input_id == 30) {
		nvt_ts_proximity_report(point_data[1]&0x07);
	}
#endif


	for (i = 0; i < ts->max_touch_num; i++) {
		position = 1 + 6*i;
		input_id = (unsigned int)(point_data[position+0]>>3);

		if ((point_data[position]&0x07) == 0x03) {
			continue;
		} else if (((point_data[position]&0x07) == 0x01) || ((point_data[position]&0x07) == 0x02)) {
			input_x = (unsigned int)(point_data[position+1]<<4) + (unsigned int) (point_data[position+3]>>4);
			input_y = (unsigned int)(point_data[position+2]<<4) + (unsigned int) (point_data[position+3]&0x0f);
			input_w = (unsigned int)(point_data[position+4])+10;
			if (input_w > 255)
				input_w = 255;

			if ((input_x < 0) || (input_y < 0))
				continue;
			if ((input_x > ts->abs_x_max) || (input_y > ts->abs_y_max))
				continue;

			input_report_key(ts->input_dev, BTN_TOUCH, 1);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, input_x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, input_y);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, input_w);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, input_w);
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, input_id);

			input_mt_sync(ts->input_dev);

			finger_cnt++;
		}
	}

#if TOUCH_KEY_NUM > 0
	if (point_data[ts->max_touch_num*6+1] == 0xF8) {
		for (i = 0; i < TOUCH_KEY_NUM; i++) {
			input_report_key(ts->input_dev, touch_key_array[i], ((point_data[ts->max_touch_num*6+2]>>i)&(0x01)));
		}
	} else {

		if (!(((point_data[1]&0x07) == 0x01) || ((point_data[1]&0x07) == 0x02))) {
			for (i = 0; i < TOUCH_KEY_NUM; i++) {
				input_report_key(ts->input_dev, touch_key_array[i], 0);
			}
		}
	}
#endif

	if (finger_cnt == 0) {
		input_report_key(ts->input_dev, BTN_TOUCH, 0);

		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);

		input_mt_sync(ts->input_dev);
	}

	input_sync(ts->input_dev);

XFER_ERROR:
	enable_irq(ts->client->irq);
}

/*******************************************************
Description:
	External interrupt service routine.

Parameter:
	irq:	interrupt number.
	dev_id: private data pointer.

return:
	irq execute status.
*******************************************************/
static irqreturn_t nvt_ts_irq_handler(int irq, void *dev_id)
{


	disable_irq_nosync(ts->client->irq);

	#if WAKEUP_GESTURE
	if (bTouchIsAwake == 0)
		wake_lock_timeout(&gestrue_wakelock, msecs_to_jiffies(5000));
	#endif

	queue_work(nvt_wq, &ts->nvt_work);

	return IRQ_HANDLED;
}

static uint8_t nvt_ts_read_chipid(void)
{
	uint8_t buf[8] = {0};
	int retry = 0;

	for (retry = 5; retry >= 0; retry--) {

		buf[0] = 0x00;
		buf[1] = 0xA5;
		CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
		msleep(100);

		buf[0] = 0xFF;
		buf[1] = 0xF0;
		buf[2] = 0x00;
		CTP_I2C_WRITE(ts->client, 0x01, buf, 3);
		msleep(10);

		buf[0] = 0x00;
		buf[1] = 0x00;
		CTP_I2C_READ(ts->client, 0x01, buf, 3);

		if (buf[1] == 5)
			break;
	}

	dev_info(&ts->client->dev, "ChipID = %d.\n", buf[1]);

	return buf[1];

}

static int nvt_parse_dt(struct device *dev,
						   struct nvt_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;

	printk("nvt205 in parse_dt\n");
	pdata->name = "nvt205";
	rc = of_property_read_string(np, "nvt205, name", &pdata->name);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read name\n");
		return rc;
	}

	printk("nvt205in parse_dt, get reset GPIO\n");
	pdata->reset_gpio = of_get_named_gpio_flags(np, "nvt205, reset-gpio",
						0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
		return pdata->reset_gpio;

	printk("nvt205 in parse_dt, get irq GPIO\n");
	pdata->irq_gpio = of_get_named_gpio_flags(np, "nvt205, irq-gpio",
					  0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
		return pdata->irq_gpio;

	printk("nvt205 in parse_dt done, RST:%d, IRQ:%d\n", pdata->reset_gpio, pdata->irq_gpio);
	return 0;
}


/*******************************************************
Description:
	Novatek touchscreen
 function.

Parameter:
	client:	i2c device struct.
	id:device id.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int nvt_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	int retry = 0;
	int err;

	struct nvt_platform_data *pdata;
	uint8_t chip_id = 0;

	struct device_node *np = client->dev.of_node;
	ts = kmalloc(sizeof(struct nvt_ts_data), GFP_KERNEL);
	ts->client = client;
	i2c_set_clientdata(client, ts);
	if (!np) {
		goto  err_check_functionality_failed;
	}

   if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
							sizeof(struct nvt_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = nvt_parse_dt(&client->dev, pdata);
		if (ret) {
			dev_err(&client->dev, "DT parsing failed\n");
			return ret;
		}
	} else
		pdata = client->dev.platform_data;

		if (!pdata) {
		dev_err(&client->dev, "Invalid pdata\n");
		return -EINVAL;
		}

	sprd_3rdparty_gpio_tp_rst = pdata->reset_gpio;
	sprd_3rdparty_gpio_tp_irq = pdata->irq_gpio;
	if (gpio_request(pdata->irq_gpio, "ts_rst") < 0 || \
		gpio_request(pdata->reset_gpio, "ts_irq") < 0) {
		dev_err(&client->dev, "NT11205 gpio resource is requested by some tp.\n");
		goto  err_check_functionality_failed;
	}

	nvt_hw_reset();


	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c_check_functionality failed. (no I2C_FUNC_I2C)\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}


	chip_id = nvt_ts_read_chipid();
	nvt_hw_reset();
	msleep(500);

	if (chip_id == 5)
		dev_err(&ts->client->dev, "This IC is NT11205.\n");
	else {
	dev_info(&ts->client->dev, "This IC is not NT11205.\n");

	ret = -ENODEV;
		  goto err_check_functionality_failed;
	}


#if  WT_ADD_CTP_INFO
	 if (chip_id == 5) {
	   sprintf(tp_string_version, "NVTtouch, nt11205, fw_ver:%x", nvt_read_firmware_version());
	 } else {
	  sprintf(tp_string_version, "NVTtouch, unknwon, fw_ver:%x", nvt_read_firmware_version());
	 }
#endif


	nvt_wq = create_workqueue("nvt_wq");
	if (!nvt_wq) {
		dev_info(&client->dev, " nvt_wq create workqueue failed.\n");
		return -ENOMEM;
	}
	INIT_WORK(&ts->nvt_work, nvt_ts_work_func);


	ts->input_dev = input_allocate_device();

	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		dev_info(&client->dev, "allocate input device failed.\n");
		goto err_input_dev_alloc_failed;
	}


#if BOOT_UPDATE_FIRMWARE
#if NVT_UPDATE_BY_WORKQUEUE
	dev_info(&ts->client->dev, "Boot_Update_Firmware.\n");
	nvt_fwu_wq = create_singlethread_workqueue("nvt_fwu_wq");
	if (!nvt_fwu_wq) {
		dev_err(&client->dev, "nvt_fwu_wq create workqueue failed.\n");
		return -ENOMEM;
	}
	INIT_DELAYED_WORK(&ts->nvt_fwu_work, Boot_Update_Firmware);
	queue_delayed_work(nvt_fwu_wq, &ts->nvt_fwu_work, msecs_to_jiffies(4000));
#else
	dev_info(&ts->client->dev, "Boot_Auto_Update_Firmware.\n");
	Boot_Auto_Update_Firmware();
#endif
#endif

	ts->abs_x_max = TOUCH_MAX_WIDTH;
	ts->abs_y_max = TOUCH_MAX_HEIGHT;
	ts->max_touch_num = TOUCH_MAX_FINGER_NUM;

	#if TOUCH_KEY_NUM > 0
	ts->max_button_num = TOUCH_KEY_NUM;
	#endif


	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	ts->input_dev->absbit[0] = BIT(ABS_X) | BIT(ABS_Y) | BIT(ABS_PRESSURE);
	ts->input_dev->propbit[0] = BIT(INPUT_PROP_DIRECT);

	input_set_abs_params(ts->input_dev, ABS_X, 0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_Y, 0, ts->abs_y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, 255, 0, 0);

#if TOUCH_MAX_FINGER_NUM > 1
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->abs_y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->max_touch_num, 0, 0);
#endif

#if defined(CONFIG_TOUCHSCREEN_WITH_PROXIMITY)
	input_set_abs_params(ts->input_dev, ABS_DISTANCE, 0, 1, 0, 0);
#endif

#if TOUCH_KEY_NUM > 0
	for (retry = 0; retry < TOUCH_KEY_NUM; retry++) {
		input_set_capability(ts->input_dev, EV_KEY, touch_key_array[retry]);
	}
#endif

#if WAKEUP_GESTURE
	for (retry = 0; retry < (sizeof(gesture_key_array)/sizeof(gesture_key_array[0])); retry++) {
		input_set_capability(ts->input_dev, EV_KEY, gesture_key_array[retry]);
	}

	wake_lock_init(&gestrue_wakelock, WAKE_LOCK_SUSPEND, "poll-wake-lock");
#endif

	sprintf(ts->phys, "input/ts");
	ts->input_dev->name = NVT_TS_NAME;
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0x0205;
	ts->input_dev->id.product = 0x0001;



	ret = input_register_device(ts->input_dev);
	if (ret) {
		dev_info(&client->dev, "register input device (%s) failed. ret=%d\n", ts->input_dev->name, ret);
		goto err_input_register_device_failed;
	}

#if defined(CONFIG_TOUCHSCREEN_WITH_PROXIMITY)
	/*create device group in sysfs as user interface */
	ret = sysfs_create_group(&ts->input_dev->dev.kobj, &proximity_attr_group);
	if (ret) {
		dev_err(&client->dev, "ysfs_create_group() error\n");
		ret = -EINVAL;
		goto err_input_register_device_failed;
	}
#endif


	client->irq = gpio_to_irq(sprd_3rdparty_gpio_tp_irq);
	if (client->irq) {
		ret = request_irq(client->irq, nvt_ts_irq_handler, IRQ_TYPE_EDGE_FALLING | IRQF_NO_SUSPEND | IRQF_ONESHOT, client->name, ts);

		if (ret != 0) {
			dev_info(&client->dev, "request irq failed. ret=%d\n", ret);
			goto err_int_request_failed;
		} else {
			disable_irq(client->irq);
			dev_info(&client->dev, "request irq %d succeed.\n", client->irq);
		}
	}
	nvt_hw_reset();
 #if CTP_WT_TP_OPENSHORT_TEST
		create_ctp_proc();
 #endif



	#if NVT_TOUCH_CTRL_DRIVER
	ret = nvt_flash_init();
	if (ret != 0) {
		dev_info(&client->dev, "nvt_flash_init failed. ret=%d\n", ret);
		goto err_init_NVT_ts;
	}
	#endif


#if defined(CONFIG_FB)
		INIT_WORK(&ts->fb_notify_work, fb_notify_resume_work);
		ts->fb_notif.notifier_call = fb_notifier_callback;

		err = fb_register_client(&ts->fb_notif);

		if (err)
			dev_err(&client->dev, "Unable to register fb_notifier: %d\n",
					err);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = nvt_ts_early_suspend;
	ts->early_suspend.resume = nvt_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	dev_info(&client->dev, "%s finished.\n", __func__);
	enable_irq(client->irq);
	 printk("probe sucess\n");
	return 0;


err_init_NVT_ts:
	free_irq(client->irq, ts);
err_int_request_failed:
err_input_register_device_failed:
	input_unregister_device(ts->input_dev);
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:

err_check_functionality_failed:
	i2c_set_clientdata(client, NULL);
	kfree(ts);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen driver release function.

Parameter:
	client:	i2c device struct.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int nvt_ts_remove(struct i2c_client *client)
{
	 struct nvt_ts_data *ts = i2c_get_clientdata(client);

#if defined(CONFIG_FB)
		if (fb_unregister_client(&ts->fb_notif))
			dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif

	dev_notice(&client->dev, "removing driver...\n");

	free_irq(client->irq, ts);
	input_unregister_device(ts->input_dev);
	i2c_set_clientdata(client, NULL);
	kfree(ts);

	return 0;
}


#ifdef CONFIG_PM
static int nvt_ts_suspend(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);
	uint8_t buf[4] = {0};

	 CTP_INFO("nvt_ts_suspend in\n");
#if defined(CONFIG_TOUCHSCREEN_WITH_PROXIMITY)
	if (prox_enable)
		return 0;
#endif

#if WAKEUP_GESTURE
	bTouchIsAwake = 0;
	dev_info(&ts->client->dev, "Enable touch wakeup gesture.\n");


	buf[0] = 0x88;
	buf[1] = 0x55;
	buf[2] = 0xAA;
	buf[3] = 0xA6;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 4);
	enable_irq_wake(ts->client->irq);
	irq_set_irq_type(ts->client->irq, IRQF_TRIGGER_LOW | IRQF_NO_SUSPEND | IRQF_ONESHOT);
#else
	disable_irq(ts->client->irq);


	buf[0] = 0x88;
	buf[1] = 0x55;
	buf[2] = 0xAA;
	buf[3] = 0xA5;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 4);
#endif

	msleep(50);
		  CTP_INFO("nvt_ts_suspend out\n");
	return 0;
}

static int nvt_ts_resume(struct device *dev)
{
 struct nvt_ts_data *ts = dev_get_drvdata(dev);

  CTP_INFO("nvt_ts_resume in\n");
#if defined(CONFIG_TOUCHSCREEN_WITH_PROXIMITY)
	if (prox_enable)
		return 0;
#endif

#if WAKEUP_GESTURE
	irq_set_irq_type(ts->client->irq, IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND | IRQF_ONESHOT);
	nvt_hw_reset();
	bTouchIsAwake = 1;
#else
	nvt_hw_reset();
	enable_irq(ts->client->irq);
#endif
	  CTP_INFO("nvt_ts_resume out\n");
	return 0;
}

static const struct dev_pm_ops nvttouch_ts_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
	.suspend = nvt_ts_suspend,
	.resume = nvt_ts_resume,
#endif
};

#else
static int nvt_ts_suspend(struct device *dev)
{
	return 0;
}

static int nvt_ts_resume(struct device *dev)
{
	return 0;
}
#endif



#if defined(CONFIG_FB)
static void fb_notify_resume_work(struct work_struct *work)
{
	   struct nvt_ts_data *data = container_of(work, struct nvt_ts_data, fb_notify_work);
	   nvt_ts_resume(&data->client->dev);
}
static int fb_notifier_callback(struct notifier_block *self,
								unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct nvt_ts_data *data_ts = container_of(self, struct nvt_ts_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK &&
		data_ts && data_ts->client) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK)
		   schedule_work(&data_ts->fb_notify_work);
		 else if (*blank == FB_BLANK_POWERDOWN) {
			flush_work(&data_ts->fb_notify_work);
			nvt_ts_suspend(&data_ts->client->dev);
		}
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void nvt_ts_early_suspend(struct early_suspend *h)
{
	struct nvt_ts_data *ts;
	ts = container_of(h, struct nvt_ts_data, early_suspend);

	nvt_ts_suspend(&ts->client->dev);
}

static void nvt_ts_late_resume(struct early_suspend *h)
{
	struct nvt_ts_data *ts;
	ts = container_of(h, struct nvt_ts_data, early_suspend);

	nvt_ts_resume(&ts->client->dev);
}
#endif

static const struct i2c_device_id nvt_ts_id[] = {
	{NVT_I2C_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, nvt_ts_id);

static const struct of_device_id nvt_of_match[] = {
	   {.compatible = "sprd, NVTouch_205",},
	   {}
};
MODULE_DEVICE_TABLE(of, nvt_of_match);

static struct i2c_driver nvt_i2c_driver = {
	.probe		= nvt_ts_probe,
	.remove		= nvt_ts_remove,


	.id_table	= nvt_ts_id,
	.driver	= {
		.name	= NVT_I2C_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = nvt_of_match,
		#ifdef CONFIG_PM
		.pm = &nvttouch_ts_pm_ops,
		#endif

	},
};


/*******************************************************
Description:
	Driver Install function.
return:
	Executive Outcomes. 0---succeed.
********************************************************/
static int __init nvt_driver_init(void)
{
	int ret = -1;
	ret = i2c_add_driver(&nvt_i2c_driver);
	if (ret) {
		printk("i2c_add_driver failed!\n");
		return ret;
	}
	return ret;
}

/*******************************************************
Description:
	Driver uninstall function.
return:
	Executive Outcomes. 0---succeed.
********************************************************/
static void __exit nvt_driver_exit(void)
{
	i2c_del_driver(&nvt_i2c_driver);
	if (nvt_wq)
		destroy_workqueue(nvt_wq);

	#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq)
		destroy_workqueue(nvt_fwu_wq);
	#endif
}

late_initcall(nvt_driver_init);
module_exit(nvt_driver_exit);

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL");
