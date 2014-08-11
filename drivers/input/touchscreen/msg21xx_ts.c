/*
 * MStar MSG21XX touchscreen driver
 *
 * Copyright (c) 2006-2012 MStar Semiconductor, Inc.
 *
 * Copyright (C) 2012 Bruce Ding <bruce.ding@mstarsemi.com>
 *
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <mach/gpio.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <asm/unistd.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/regulator/consumer.h>

#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#include <linux/input.h>
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
#include <linux/input/vir_ps.h>
#endif

/* Macro Definition*/

#define TOUCH_DRIVER_DEBUG 0
#if (TOUCH_DRIVER_DEBUG == 1)
#define DBG(fmt, arg...) pr_info(fmt, ##arg)
#else
#define DBG(fmt, arg...)
#endif

/* Constant Value & Variable Definition*/

static struct regulator *vdd;
static struct regulator *vcc_i2c;

#define MSTAR_VTG_MIN_UV	2800000
#define MSTAR_VTG_MAX_UV	3300000
#define MSTAR_I2C_VTG_MIN_UV	1800000
#define MSTAR_I2C_VTG_MAX_UV	1800000


#define TOUCH_SCREEN_X_MIN   (0)
#define TOUCH_SCREEN_Y_MIN   (0)

#define MAX_BUTTONS		4
#define FT_COORDS_ARR_SIZE	4


/*
 * Note.
 * Please do not change the below setting.
 */
#define TPD_WIDTH   (2048)
#define TPD_HEIGHT  (2048)

/*#define FIRMWARE_AUTOUPDATE*/
#ifdef FIRMWARE_AUTOUPDATE
enum {
	SWID_START = 1,
	SWID_TRULY = SWID_START,
	SWID_NULL,
};

static unsigned char MSG_FIRMWARE[1][33*1024] = { {
		#include "msg21xx_truly_update_bin.h"
	}
};
#endif

#define CONFIG_TP_HAVE_KEY

#define PINCTRL_STATE_ACTIVE	"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND	"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE	"pmx_ts_release"

#define SLAVE_I2C_ID_DBBUS		 (0xC4>>1)

#define DEMO_MODE_PACKET_LENGTH	(8)
#define MAX_TOUCH_NUM		   (2)

#define TP_PRINT
#ifdef TP_PRINT
static int tp_print_proc_read(void);
static void tp_print_create_entry(void);
#endif

static char *fw_version; /* customer firmware version*/
static unsigned short fw_version_major;
static unsigned short fw_version_minor;
static unsigned char temp[94][1024];
static unsigned int crc32_table[256];
static int FwDataCnt;
static unsigned char bFwUpdating;
static struct class *firmware_class;
static struct device *firmware_cmd_dev;

static struct i2c_client *i2c_client;

static u32 button_map[MAX_BUTTONS];

static u32 num_buttons;

struct msg21xx_ts_platform_data {
	const char *name;
	const char *fw_name;
	u32 irqflags;
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	u32 family_id;
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 panel_minx;
	u32 panel_miny;
	u32 panel_maxx;
	u32 panel_maxy;
	u32 group_id;
	u32 hard_rst_dly;
	u32 soft_rst_dly;
	u32 num_max_touches;
	bool fw_vkey_support;
	bool no_force_update;
	bool i2c_pull_up;
	bool ignore_id_check;
	int (*power_init) (bool);
	int (*power_on) (bool);
};

static struct msg21xx_ts_platform_data *pdata;

struct msg21xx_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	const struct msg21xx_ts_platform_data *pdata;
	struct regulator *vdd;
	struct regulator *vcc_i2c;
	bool loading_fw;
	u8 family_id;
	struct dentry *dir;
	u16 addr;
	bool suspended;
	char *ts_info;
	u8 *tch_data;
	u32 tch_data_len;
	u8 fw_ver[3];
	u8 fw_vendor_id;
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	struct pinctrl_state *pinctrl_state_release;
	struct mutex ts_mutex;
};

static struct msg21xx_ts_data *ts_data;

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static struct early_suspend mstar_ts_early_suspend;
#endif

#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
static unsigned char bEnableTpProximity;
static unsigned char bFaceClosingTp;
#endif

static struct mutex msg21xx_mutex;
static struct input_dev *input_dev;


/*Data Type Definition*/

struct touchPoint_t {
	unsigned short x;
	unsigned short y;
};

struct touchInfo_t {
	struct touchPoint_t point[MAX_TOUCH_NUM];
	unsigned char count;
	unsigned char keycode;
};

enum i2c_speed {
	I2C_SLOW = 0,
	I2C_NORMAL = 1, /* Enable erasing/writing for 10 msec. */
	I2C_FAST = 2,   /* Disable EWENB before 10 msec timeout. */
};

enum EMEM_TYPE_t {
	EMEM_ALL = 0,
	EMEM_MAIN,
	EMEM_INFO,
};

/* Function Definition*/

static unsigned int _CRC_doReflect(unsigned int ref, signed char ch)
{
	unsigned int value = 0;
	unsigned int i = 0;

	for (i = 1; i < (ch + 1); i++) {
		if (ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	}

	return value;
}

static unsigned int _CRC_getValue(unsigned int text, unsigned int prevCRC)
{
	unsigned int ulCRC = prevCRC;

	ulCRC = (ulCRC >> 8) ^ crc32_table[(ulCRC & 0xFF) ^ text];

	return ulCRC;
}

static void _CRC_initTable(void)
{
	unsigned int magic_number = 0x04c11db7;
	unsigned int i, j;

	for (i = 0; i <= 0xFF; i++) {
		crc32_table[i] = _CRC_doReflect(i, 8) << 24;
		for (j = 0; j < 8; j++)
			crc32_table[i] = (crc32_table[i] << 1) ^
				(crc32_table[i] & (0x80000000L) ?
					magic_number : 0);
		crc32_table[i] = _CRC_doReflect(crc32_table[i], 32);
	}
}

static void reset_hw(void)
{
	DBG("reset_hw()\n");

	gpio_direction_output(pdata->reset_gpio, 1);
	gpio_set_value_cansleep(pdata->reset_gpio, 0);
	msleep(100);	 /* Note that the RST must be in LOW 10ms at least */
	gpio_set_value_cansleep(pdata->reset_gpio, 1);
	/* Enable the interrupt service thread/routine for INT after 50ms */
	msleep(100);
}

static int read_i2c_seq(unsigned char addr, unsigned char *buf,
					unsigned short size)
{
	int rc = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = addr,
			.flags = I2C_M_RD, /* read flag*/
			.len = size,
			.buf = buf,
		},
	};

	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	if (i2c_client != NULL) {
		rc = i2c_transfer(i2c_client->adapter, msgs, 1);
		if (rc < 0)
			DBG("read_i2c_seq() error %d\n", rc);
	} else {
		DBG("i2c_client is NULL\n");
	}

	return rc;
}

static int write_i2c_seq(unsigned char addr, unsigned char *buf,
					unsigned short size)
{
	int rc = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = addr,
			/* if read flag is undefined,
			then it means write flag.*/
			.flags = 0,
			.len = size,
			.buf = buf,
		},
	};

	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	if (i2c_client != NULL) {
		rc = i2c_transfer(i2c_client->adapter, msgs, 1);
		if (rc < 0)
			DBG("write_i2c_seq() error %d\n", rc);
	} else {
		DBG("i2c_client is NULL\n");
	}

	return rc;
}

static unsigned short read_reg(unsigned char bank, unsigned char addr)
{
	unsigned char tx_data[3] = {0x10, bank, addr};
	unsigned char rx_data[2] = {0};

	write_i2c_seq(SLAVE_I2C_ID_DBBUS, &tx_data[0], 3);
	read_i2c_seq(SLAVE_I2C_ID_DBBUS, &rx_data[0], 2);

	return rx_data[1] << 8 | rx_data[0];
}

static void write_reg(unsigned char bank, unsigned char addr,
						unsigned short data)
{
	unsigned char tx_data[5] = {0x10, bank, addr, data & 0xFF, data >> 8};
	write_i2c_seq(SLAVE_I2C_ID_DBBUS, &tx_data[0], 5);
}

static void write_reg_8bit(unsigned char bank, unsigned char addr,
						unsigned char data)
{
	unsigned char tx_data[4] = {0x10, bank, addr, data};
	write_i2c_seq(SLAVE_I2C_ID_DBBUS, &tx_data[0], 4);
}

static void dbbusDWIICEnterSerialDebugMode(void)
{
	unsigned char data[5];

	/* Enter the Serial Debug Mode*/
	data[0] = 0x53;
	data[1] = 0x45;
	data[2] = 0x52;
	data[3] = 0x44;
	data[4] = 0x42;

	write_i2c_seq(SLAVE_I2C_ID_DBBUS, data, 5);
}

static void dbbusDWIICStopMCU(void)
{
	unsigned char data[1];

	/* Stop the MCU*/
	data[0] = 0x37;

	write_i2c_seq(SLAVE_I2C_ID_DBBUS, data, 1);
}

static void dbbusDWIICIICUseBus(void)
{
	unsigned char data[1];

	/* IIC Use Bus*/
	data[0] = 0x35;

	write_i2c_seq(SLAVE_I2C_ID_DBBUS, data, 1);
}

static void dbbusDWIICIICReshape(void)
{
	unsigned char data[1];

	/* IIC Re-shape*/
	data[0] = 0x71;

	write_i2c_seq(SLAVE_I2C_ID_DBBUS, data, 1);
}

static unsigned char get_ic_type(void)
{
	unsigned char ic_type = 0;

	reset_hw();
	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();
	msleep(300);

	/* stop mcu*/
	write_reg_8bit(0x0F, 0xE6, 0x01);
	/* disable watch dog*/
	write_reg(0x3C, 0x60, 0xAA55);
	/* get ic type*/
	ic_type = (0xff)&(read_reg(0x1E, 0xCC));

	if (ic_type != 1		/*msg2133*/
		&& ic_type != 2	 /*msg21xxA*/
		&& ic_type !=  3)   /*msg26xxM*/ {
		ic_type = 0;
	}

	reset_hw();

	return ic_type;
}

static int get_customer_firmware_version(void)
{
	unsigned char dbbus_tx_data[3] = {0};
	unsigned char dbbus_rx_data[4] = {0};
	int ret = 0;

	DBG("get_customer_firmware_version()\n");

	dbbus_tx_data[0] = 0x53;
	dbbus_tx_data[1] = 0x00;
	dbbus_tx_data[2] = 0x2A;
	mutex_lock(&msg21xx_mutex);
	write_i2c_seq(ts_data->client->addr, &dbbus_tx_data[0], 3);
	read_i2c_seq(ts_data->client->addr, &dbbus_rx_data[0], 4);
	mutex_unlock(&msg21xx_mutex);
	fw_version_major = (dbbus_rx_data[1]<<8) + dbbus_rx_data[0];
	fw_version_minor = (dbbus_rx_data[3]<<8) + dbbus_rx_data[2];

	DBG("*** major = %d ***\n", fw_version_major);
	DBG("*** minor = %d ***\n", fw_version_minor);

	if (fw_version == NULL)
		fw_version = kzalloc(sizeof(char), GFP_KERNEL);

	snprintf(fw_version, sizeof(char) - 1, "%03d%03d",
				fw_version_major, fw_version_minor);


	return ret;
}

static int firmware_erase_c33(enum EMEM_TYPE_t emem_type)
{
	/* stop mcu*/
	write_reg(0x0F, 0xE6, 0x0001);

	/*disable watch dog*/
	write_reg_8bit(0x3C, 0x60, 0x55);
	write_reg_8bit(0x3C, 0x61, 0xAA);

	/* set PROGRAM password*/
	write_reg_8bit(0x16, 0x1A, 0xBA);
	write_reg_8bit(0x16, 0x1B, 0xAB);

	write_reg_8bit(0x16, 0x18, 0x80);

	if (emem_type == EMEM_ALL)
		write_reg_8bit(0x16, 0x08, 0x10);

	write_reg_8bit(0x16, 0x18, 0x40);
	msleep(20);

	/* clear pce*/
	write_reg_8bit(0x16, 0x18, 0x80);

	/* erase trigger*/
	if (emem_type == EMEM_MAIN)
		write_reg_8bit(0x16, 0x0E, 0x04); /*erase main*/
	else
		write_reg_8bit(0x16, 0x0E, 0x08); /*erase all block*/

	return 1;
}

static ssize_t firmware_update_c33(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size,
						enum EMEM_TYPE_t emem_type) {
	unsigned int i, j;
	unsigned int crc_main, crc_main_tp;
	unsigned int crc_info, crc_info_tp;
	unsigned short reg_data = 0;
	int update_pass = 1;

	crc_main = 0xffffffff;
	crc_info = 0xffffffff;

	reset_hw();
	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();
	msleep(300);

	/*erase main*/
	firmware_erase_c33(EMEM_MAIN);
	msleep(1000);

	reset_hw();
	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();
	msleep(300);

	/*
	 * Program
	*/

	/*polling 0x3CE4 is 0x1C70*/
	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		do {
			reg_data = read_reg(0x3C, 0xE4);
		} while (reg_data != 0x1C70);
	}

	switch (emem_type) {
	case EMEM_ALL:
		write_reg(0x3C, 0xE4, 0xE38F);  /* for all-blocks*/
		break;
	case EMEM_MAIN:
		write_reg(0x3C, 0xE4, 0x7731);  /* for main block*/
		break;
	case EMEM_INFO:
		write_reg(0x3C, 0xE4, 0x7731);  /* for info block*/

		write_reg_8bit(0x0F, 0xE6, 0x01);

		write_reg_8bit(0x3C, 0xE4, 0xC5);
		write_reg_8bit(0x3C, 0xE5, 0x78);

		write_reg_8bit(0x1E, 0x04, 0x9F);
		write_reg_8bit(0x1E, 0x05, 0x82);

		write_reg_8bit(0x0F, 0xE6, 0x00);
		msleep(100);
		break;
	}

	/* polling 0x3CE4 is 0x2F43*/
	do {
		reg_data = read_reg(0x3C, 0xE4);
	} while (reg_data != 0x2F43);

	/* calculate CRC 32*/
	_CRC_initTable();

	/* total  32 KB : 2 byte per R/W */
	for (i = 0; i < 32; i++) {
		if (i == 31) {
			temp[i][1014] = 0x5A;
			temp[i][1015] = 0xA5;

			for (j = 0; j < 1016; j++)
				crc_main = _CRC_getValue(temp[i][j], crc_main);
		} else {
			for (j = 0; j < 1024; j++)
				crc_main = _CRC_getValue(temp[i][j], crc_main);
		}

		for (j = 0; j < 8; j++)
			write_i2c_seq(ts_data->client->addr,
						&temp[i][j*128], 128);
		msleep(100);

		/* polling 0x3CE4 is 0xD0BC*/
		do {
			reg_data = read_reg(0x3C, 0xE4);
		} while (reg_data != 0xD0BC);

		write_reg(0x3C, 0xE4, 0x2F43);
	}

	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		/* write file done and check crc*/
		write_reg(0x3C, 0xE4, 0x1380);
	}
	msleep(20);

	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		/* polling 0x3CE4 is 0x9432*/
		do {
			reg_data = read_reg(0x3C, 0xE4);
		} while (reg_data != 0x9432);
	}

	crc_main = crc_main ^ 0xffffffff;
	crc_info = crc_info ^ 0xffffffff;

	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		/* CRC Main from TP*/
		crc_main_tp = read_reg(0x3C, 0x80);
		crc_main_tp = (crc_main_tp << 16) | read_reg(0x3C, 0x82);

		/* CRC Info from TP*/
		crc_info_tp = read_reg(0x3C, 0xA0);
		crc_info_tp = (crc_info_tp << 16) | read_reg(0x3C, 0xA2);
	}

	update_pass = 1;
	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		if (crc_main_tp != crc_main)
			update_pass = 0;
	}

	if (!update_pass) {
		DBG("update_C33 failed\n");
		reset_hw();
		FwDataCnt = 0;
		return 0;
	}

	DBG("update_C33 OK\n");
	reset_hw();
	FwDataCnt = 0;
	return size;
}
#ifdef FIRMWARE_AUTOUPDATE
static unsigned short main_sw_id = 0x7FF, info_sw_id = 0x7FF;
static unsigned int bin_conf_crc32;

static unsigned int _CalMainCRC32(void)
{
	unsigned int ret = 0;
	unsigned short reg_data = 0;

	reset_hw();

	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();
	msleep(100);

	/*Stop MCU*/
	write_reg(0x0F, 0xE6, 0x0001);

	/* Stop Watchdog*/
	write_reg_8bit(0x3C, 0x60, 0x55);
	write_reg_8bit(0x3C, 0x61, 0xAA);

	/*cmd*/
	write_reg(0x3C, 0xE4, 0xDF4C);
	write_reg(0x1E, 0x04, 0x7d60);
	/* TP SW reset*/
	write_reg(0x1E, 0x04, 0x829F);

	/*MCU run*/
	write_reg(0x0F, 0xE6, 0x0000);

	/*polling 0x3CE4*/
	do {
		reg_data = read_reg(0x3C, 0xE4);
	} while (reg_data != 0x9432);

	/* Cal CRC Main from TP*/
	ret = read_reg(0x3C, 0x80);
	ret = (ret << 16) | read_reg(0x3C, 0x82);

	DBG("[21xxA]:Current main crc32=0x%x\n", ret);
	return ret;
}

static void _ReadBinConfig(void)
{
	unsigned char dbbus_tx_data[5] = {0};
	unsigned char dbbus_rx_data[4] = {0};
	unsigned short reg_data = 0;

	reset_hw();

	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();
	msleep(100);

	/*Stop MCU*/
	write_reg(0x0F, 0xE6, 0x0001);

	/* Stop Watchdog*/
	write_reg_8bit(0x3C, 0x60, 0x55);
	write_reg_8bit(0x3C, 0x61, 0xAA);

	/*cmd*/
	write_reg(0x3C, 0xE4, 0xA4AB);
	write_reg(0x1E, 0x04, 0x7d60);

	/* TP SW reset*/
	write_reg(0x1E, 0x04, 0x829F);

	/*MCU run*/
	write_reg(0x0F, 0xE6, 0x0000);

    /*polling 0x3CE4*/
	do {
		reg_data = read_reg(0x3C, 0xE4);
	} while (reg_data != 0x5B58);

	dbbus_tx_data[0] = 0x72;
	dbbus_tx_data[1] = 0x7F;
	dbbus_tx_data[2] = 0x55;
	dbbus_tx_data[3] = 0x00;
	dbbus_tx_data[4] = 0x04;
	write_i2c_seq(ts_data->client->addr, &dbbus_tx_data[0], 5);
	read_i2c_seq(ts_data->client->addr, &dbbus_rx_data[0], 4);
	if ((dbbus_rx_data[0] >= 0x30 && dbbus_rx_data[0] <= 0x39)
		&& (dbbus_rx_data[1] >= 0x30 && dbbus_rx_data[1] <= 0x39)
		&& (dbbus_rx_data[2] >= 0x31 && dbbus_rx_data[2] <= 0x39)) {
		main_sw_id = (dbbus_rx_data[0] - 0x30) * 100 +
					(dbbus_rx_data[1] - 0x30) * 10 +
					(dbbus_rx_data[2] - 0x30);
	}

	dbbus_tx_data[0] = 0x72;
	dbbus_tx_data[1] = 0x7F;
	dbbus_tx_data[2] = 0xFC;
	dbbus_tx_data[3] = 0x00;
	dbbus_tx_data[4] = 0x04;
	write_i2c_seq(ts_data->client->addr, &dbbus_tx_data[0], 5);
	read_i2c_seq(ts_data->client->addr, &dbbus_rx_data[0], 4);
	bin_conf_crc32 = dbbus_rx_data[0];
	bin_conf_crc32 = (bin_conf_crc32<<8)|dbbus_rx_data[1];
	bin_conf_crc32 = (bin_conf_crc32<<8)|dbbus_rx_data[2];
	bin_conf_crc32 = (bin_conf_crc32<<8)|dbbus_rx_data[3];

	dbbus_tx_data[0] = 0x72;
	dbbus_tx_data[1] = 0x83;
	dbbus_tx_data[2] = 0x00;
	dbbus_tx_data[3] = 0x00;
	dbbus_tx_data[4] = 0x04;
	write_i2c_seq(ts_data->client->addr, &dbbus_tx_data[0], 5);
	read_i2c_seq(ts_data->client->addr, &dbbus_rx_data[0], 4);
	if ((dbbus_rx_data[0] >= 0x30 && dbbus_rx_data[0] <= 0x39)
		&& (dbbus_rx_data[1] >= 0x30 && dbbus_rx_data[1] <= 0x39)
		&& (dbbus_rx_data[2] >= 0x31 && dbbus_rx_data[2] <= 0x39)) {
		info_sw_id = (dbbus_rx_data[0] - 0x30) * 100 +
					(dbbus_rx_data[1] - 0x30) * 10 +
					(dbbus_rx_data[2] - 0x30);
	}

	DBG("[21xxA]:main_sw_id = %d, info_sw_id = %d, bin_conf_crc32=0x%x\n",
			main_sw_id, info_sw_id, bin_conf_crc32);
}

static int fwAutoUpdate(void *unused)
{
	int time = 0;
	ssize_t ret = 0;

	for (time = 0; time < 5; time++) {
		DBG("fwAutoUpdate time = %d\n", time);
		ret = firmware_update_c33(NULL, NULL, NULL, 1, EMEM_MAIN);
		if (ret == 1) {
			DBG("AUTO_UPDATE OK!!!");
			break;
		}
	}
	if (time == 5)
		DBG("AUTO_UPDATE failed!!!");
	enable_irq(ts_data->client->irq);
	return 0;
}
#endif

static ssize_t firmware_update_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	DBG("*** firmware_update_show() fw_version = %s ***\n", fw_version);

	return snprintf(buf, sizeof(char) - 1, "%s\n", fw_version);
}

static ssize_t firmware_update_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t size)
{
	bFwUpdating = 1;
	disable_irq(ts_data->client->irq);

	DBG("*** update fw size = %d ***\n", FwDataCnt);
	size = firmware_update_c33(dev, attr, buf, size, EMEM_MAIN);

	enable_irq(ts_data->client->irq);
	bFwUpdating = 0;

	return size;
}

static DEVICE_ATTR(update, (S_IRUGO | S_IWUSR),
					firmware_update_show,
					firmware_update_store);

static ssize_t firmware_version_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	DBG("*** firmware_version_show() fw_version = %s ***\n", fw_version);

	return snprintf(buf, sizeof(char) - 1, "%s\n", fw_version);
}

static ssize_t firmware_version_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t size)
{
	get_customer_firmware_version();

	DBG("*** firmware_version_store() fw_version = %s ***\n", fw_version);

	return size;
}

static DEVICE_ATTR(version, (S_IRUGO | S_IWUSR),
					firmware_version_show,
					firmware_version_store);

static ssize_t firmware_data_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	DBG("*** firmware_data_show() FwDataCnt = %d ***\n", FwDataCnt);

	return FwDataCnt;
}

static ssize_t firmware_data_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t size)
{
	int count = size / 1024;
	int i;

	for (i = 0; i < count; i++) {
		memcpy(temp[FwDataCnt], buf + (i * 1024), 1024);

		FwDataCnt++;
	}

	DBG("***FwDataCnt = %d ***\n", FwDataCnt);

	if (buf != NULL)
		DBG("*** buf[0] = %c ***\n", buf[0]);

	return size;
}

static DEVICE_ATTR(data, (S_IRUGO | S_IWUSR),
			firmware_data_show, firmware_data_store);

#ifdef TP_PRINT
static ssize_t tp_print_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	tp_print_proc_read();

	return snprintf(buf, sizeof(char) - 1, "%d\n", ts_data->suspended);
}

static ssize_t tp_print_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t size)
{
	DBG("*** tp_print_store() ***\n");

	return size;
}

static DEVICE_ATTR(tpp, (S_IRUGO | S_IWUSR),
				tp_print_show, tp_print_store);
#endif

#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
static void _msg_enable_proximity(void
{
	unsigned char tx_data[4] = {0};

	DBG("_msg_enable_proximity!");
	tx_data[0] = 0x52;
	tx_data[1] = 0x00;
	tx_data[2] = 0x47;
	tx_data[3] = 0xa0;
	mutex_lock(&msg21xx_mutex);
	write_i2c_seq(ts_data->client->addr, &tx_data[0], 4);
	mutex_unlock(&msg21xx_mutex);

	bEnableTpProximity = 1;
}

static void _msg_disable_proximity(void)
{
	unsigned char tx_data[4] = {0};

	DBG("_msg_disable_proximity!");
	tx_data[0] = 0x52;
	tx_data[1] = 0x00;
	tx_data[2] = 0x47;
	tx_data[3] = 0xa1;
	mutex_lock(&msg21xx_mutex);
	write_i2c_seq(ts_data->client->addr, &tx_data[0], 4);
	mutex_unlock(&msg21xx_mutex);

	bEnableTpProximity = 0;
	bFaceClosingTp = 0;
}

static void tsps_msg21xx_enable(int en)
{
	if (en)
		_msg_enable_proximity();
	else
		_msg_disable_proximity();
}

static int tsps_msg21xx_data(void)
{
	return bFaceClosingTp;
}
#endif

static int msg21xx_pinctrl_init(void)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	ts_data->ts_pinctrl = devm_pinctrl_get(&(i2c_client->dev));
	if (IS_ERR_OR_NULL(ts_data->ts_pinctrl)) {
		retval = PTR_ERR(ts_data->ts_pinctrl);
		dev_dbg(&i2c_client->dev,
			"Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	ts_data->pinctrl_state_active = pinctrl_lookup_state(
			ts_data->ts_pinctrl, PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(ts_data->pinctrl_state_active)) {
		retval = PTR_ERR(ts_data->pinctrl_state_active);
		dev_dbg(&i2c_client->dev,
			"Can't lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	ts_data->pinctrl_state_suspend = pinctrl_lookup_state(
			ts_data->ts_pinctrl, PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(ts_data->pinctrl_state_suspend)) {
		retval = PTR_ERR(ts_data->pinctrl_state_suspend);
		dev_dbg(&i2c_client->dev,
			"Can't lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	ts_data->pinctrl_state_release = pinctrl_lookup_state(
			ts_data->ts_pinctrl, PINCTRL_STATE_RELEASE);
	if (IS_ERR_OR_NULL(ts_data->pinctrl_state_release)) {
		retval = PTR_ERR(ts_data->pinctrl_state_release);
		dev_dbg(&i2c_client->dev,
			"Can't lookup %s pinstate %d\n",
			PINCTRL_STATE_RELEASE, retval);
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(ts_data->ts_pinctrl);
err_pinctrl_get:
	ts_data->ts_pinctrl = NULL;
	return retval;
}

static unsigned char calculate_checksum(unsigned char *msg, int length)
{
	int Checksum = 0, i;

	for (i = 0; i < length; i++)
		Checksum += msg[i];

	return (unsigned char)((-Checksum) & 0xFF);
}

static int parse_info(struct touchInfo_t *info)
{
	unsigned char data[DEMO_MODE_PACKET_LENGTH] = {0};
	unsigned char checksum = 0;
	unsigned int x = 0, y = 0;
	unsigned int x2 = 0, y2 = 0;
	unsigned int delta_x = 0, delta_y = 0;

	mutex_lock(&msg21xx_mutex);
	read_i2c_seq(ts_data->client->addr, &data[0], DEMO_MODE_PACKET_LENGTH);
	mutex_unlock(&msg21xx_mutex);
	checksum = calculate_checksum(&data[0], (DEMO_MODE_PACKET_LENGTH-1));
	DBG("check sum: [%x] == [%x]?\n",
			data[DEMO_MODE_PACKET_LENGTH-1], checksum);

	if (data[DEMO_MODE_PACKET_LENGTH-1] != checksum) {
		DBG("WRONG CHECKSUM\n");
		return -EINVAL;
	}

	if (data[0] != 0x52) {
		DBG("WRONG HEADER\n");
		return -EINVAL;
	}

	info->keycode = 0xFF;
	if ((data[1] == 0xFF) && (data[2] == 0xFF) &&
		(data[3] == 0xFF) && (data[4] == 0xFF) &&
		(data[6] == 0xFF)) {
		if ((data[5] == 0xFF) || (data[5] == 0)) {
			info->keycode = 0xFF;
		} else if ((data[5] == 1) || (data[5] == 2) ||
				(data[5] == 4) || (data[5] == 8)) {
			info->keycode = data[5] >> 1;

			DBG("info->keycode index %d\n", info->keycode);
		}
	#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
		else if (bEnableTpProximity && ((data[5] == 0x80) ||
					(data[5] == 0x40))) {
			if (data[5] == 0x80)
				bFaceClosingTp = 1;
			else if (data[5] == 0x40)
				bFaceClosingTp = 0;

			return -EINVAL;
		}
	#endif
		else {
			DBG("WRONG KEY\n");
			return -EINVAL;
		}
	} else {
		x = (((data[1] & 0xF0) << 4) | data[2]);
		y = (((data[1] & 0x0F) << 8) | data[3]);
		delta_x = (((data[4] & 0xF0) << 4) | data[5]);
		delta_y = (((data[4] & 0x0F) << 8) | data[6]);

		if ((delta_x == 0) && (delta_y == 0)) {
			info->point[0].x = x * pdata->x_max / TPD_WIDTH;
			info->point[0].y = y * pdata->y_max / TPD_HEIGHT;
			info->count = 1;
		} else {
			if (delta_x > 2048)
				delta_x -= 4096;

			if (delta_y > 2048)
				delta_y -= 4096;

			x2 = (unsigned int)((signed short)x +
						(signed short)delta_x);
			y2 = (unsigned int)((signed short)y +
						(signed short)delta_y);
			info->point[0].x = x * pdata->x_max / TPD_WIDTH;
			info->point[0].y = y * pdata->y_max / TPD_HEIGHT;
			info->point[1].x = x2 * pdata->x_max / TPD_WIDTH;
			info->point[1].y = y2 * pdata->y_max / TPD_HEIGHT;
			info->count = 2;
		}
	}

	return 0;
}

static void touch_driver_touch_released(void)
{
	int i;

	DBG("point touch released\n");

	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		input_mt_slot(input_dev, i);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}

	input_report_key(input_dev, BTN_TOUCH, 0);
	input_report_key(input_dev, BTN_TOOL_FINGER, 0);
	input_sync(input_dev);
}

/* read data through I2C then report data to input
sub-system when interrupt occurred */
static irqreturn_t msg21xx_ts_interrupt(int irq, void *dev_id)
{
	struct touchInfo_t info;
	int i = 0;
	static int last_keycode = 0xFF;
	static int last_count;

	DBG("touch_driver_do_work()\n");

	memset(&info, 0x0, sizeof(info));
	if (0 == parse_info(&info)) {
	#ifdef CONFIG_TP_HAVE_KEY
		if (info.keycode != 0xFF) {   /*key touch pressed*/
			if (info.keycode < num_buttons) {
				if (info.keycode != last_keycode) {
					DBG("key touch pressed");

					input_report_key(input_dev,
							BTN_TOUCH, 1);
					input_report_key(input_dev,
						button_map[info.keycode], 1);

					last_keycode = info.keycode;
				} else {
					/* pass duplicate key-pressing*/
					DBG("REPEATED KEY\n");
				}
			} else {
				DBG("WRONG KEY\n");
			}
		} else {  /*key touch released*/
			if (last_keycode != 0xFF) {
				DBG("key touch released");

				input_report_key(input_dev,
						BTN_TOUCH, 0);
				input_report_key(input_dev,
						button_map[last_keycode],
							0);

				last_keycode = 0xFF;
			}
		}
	#endif /*CONFIG_TP_HAVE_KEY*/

		if (info.count > 0)	{ /*point touch pressed*/
			for (i = 0; i < info.count; i++) {
				input_mt_slot(input_dev, i);
				input_mt_report_slot_state(input_dev,
					MT_TOOL_FINGER, 1);
				input_report_abs(input_dev,
					ABS_MT_TOUCH_MAJOR, 1);
				input_report_abs(input_dev,
					ABS_MT_POSITION_X,
					info.point[i].x);
				input_report_abs(input_dev,
					ABS_MT_POSITION_Y,
					info.point[i].y);
			}
			last_count = info.count;
		} else if (last_count > 0) { /*point touch released*/
			for (i = 0; i < last_count; i++) {
				input_mt_slot(input_dev, i);
				input_mt_report_slot_state(input_dev,
					MT_TOOL_FINGER, 0);
			}
			last_count = 0;
		}

		input_report_key(input_dev, BTN_TOUCH, info.count > 0);
		input_report_key(input_dev, BTN_TOOL_FINGER, info.count > 0);

		input_sync(input_dev);
	}

	return IRQ_HANDLED;
}


static int msg21xx_ts_power_init(void)
{
	int rc;

	vdd = regulator_get(&i2c_client->dev, "vdd");
	if (IS_ERR(vdd)) {
		rc = PTR_ERR(vdd);
		dev_err(&i2c_client->dev,
			"Regulator get failed vdd rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(vdd) > 0) {
		rc = regulator_set_voltage(vdd, MSTAR_VTG_MIN_UV,
					   MSTAR_VTG_MAX_UV);
		if (rc) {
			dev_err(&i2c_client->dev,
				"Regulator set_vtg failed vdd rc=%d\n", rc);
			goto reg_vdd_put;
		}
	}

	vcc_i2c = regulator_get(&i2c_client->dev, "vcc_i2c");
	if (IS_ERR(vcc_i2c)) {
		rc = PTR_ERR(vcc_i2c);
		dev_err(&i2c_client->dev,
			"Regulator get failed vcc_i2c rc=%d\n", rc);
		goto reg_vdd_set_vtg;
	}

	if (regulator_count_voltages(vcc_i2c) > 0) {
		rc = regulator_set_voltage(vcc_i2c, MSTAR_I2C_VTG_MIN_UV,
					   MSTAR_I2C_VTG_MAX_UV);
		if (rc) {
			dev_err(&i2c_client->dev,
			"Regulator set_vtg failed vcc_i2c rc=%d\n", rc);
			goto reg_vcc_i2c_put;
		}
	}

	return 0;

reg_vcc_i2c_put:
	regulator_put(vcc_i2c);
reg_vdd_set_vtg:
	if (regulator_count_voltages(vdd) > 0)
		regulator_set_voltage(vdd, 0, MSTAR_VTG_MAX_UV);
reg_vdd_put:
	regulator_put(vdd);
	return rc;
}


static int msg21xx_ts_power_deinit(void)
{
	if (regulator_count_voltages(vdd) > 0)
		regulator_set_voltage(vdd, 0, MSTAR_VTG_MAX_UV);

	regulator_put(vdd);

	if (regulator_count_voltages(vcc_i2c) > 0)
		regulator_set_voltage(vcc_i2c, 0, MSTAR_I2C_VTG_MAX_UV);

	regulator_put(vcc_i2c);
	return 0;
}

static int msg21xx_ts_power_on(void)
{
	int rc;

	DBG("*** %s ***\n", __func__);
	rc = regulator_enable(vdd);
	if (rc) {
		dev_err(&i2c_client->dev,
			"Regulator vdd enable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(vcc_i2c);
	if (rc) {
		dev_err(&i2c_client->dev,
			"Regulator vcc_i2c enable failed rc=%d\n", rc);
		regulator_disable(vdd);
	}

	return rc;
}

static int msg21xx_ts_power_off(void)
{
	int rc;
	DBG("*** %s ***\n", __func__);
	rc = regulator_disable(vdd);
	if (rc) {
		dev_err(&i2c_client->dev,
			"Regulator vdd disable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_disable(vcc_i2c);
	if (rc) {
		dev_err(&i2c_client->dev,
			"Regulator vcc_i2c disable failed rc=%d\n", rc);
		rc = regulator_enable(vdd);
	}

	return rc;
}

static int msg21xx_ts_gpio_configure(bool on)
{
	int ret = 0;

	if (on) {
		if (gpio_is_valid(pdata->irq_gpio)) {
			ret = gpio_request(pdata->irq_gpio, "msg21xx_irq_gpio");
			if (ret) {
				dev_err(&i2c_client->dev,
					"Failed to request GPIO[%d], %d\n",
					pdata->irq_gpio, ret);
				goto err_irq_gpio_req;
			}
			ret = gpio_direction_input(pdata->irq_gpio);
			if (ret) {
				dev_err(&i2c_client->dev,
					"Failed to set direction for gpio[%d], %d\n",
					pdata->irq_gpio, ret);
				goto err_irq_gpio_dir;
			}
			gpio_set_value_cansleep(pdata->irq_gpio, 1);
		} else {
			dev_err(&i2c_client->dev, "irq gpio not provided\n");
			goto err_irq_gpio_req;
		}

		if (gpio_is_valid(pdata->reset_gpio)) {
			ret = gpio_request(pdata->reset_gpio,
						"msg21xx_reset_gpio");
			if (ret) {
				dev_err(&i2c_client->dev,
					"Failed to request GPIO[%d], %d\n",
					pdata->reset_gpio, ret);
				goto err_reset_gpio_req;
			}

			/* power on TP*/
			ret = gpio_direction_output(pdata->reset_gpio, 1);
			if (ret) {
				dev_err(&i2c_client->dev,
					"Failed to set direction for GPIO[%d], %d\n",
					pdata->reset_gpio, ret);
				goto err_reset_gpio_dir;
			}
			msleep(100);
			gpio_set_value_cansleep(pdata->reset_gpio, 0);
			msleep(20);
			gpio_set_value_cansleep(pdata->reset_gpio, 1);
			msleep(200);
		} else {
			dev_err(&i2c_client->dev, "reset gpio not provided\n");
			goto err_reset_gpio_req;
		}

		return 0;
	} else {
		if (gpio_is_valid(pdata->irq_gpio))
			gpio_free(pdata->irq_gpio);
		if (gpio_is_valid(pdata->reset_gpio)) {
			gpio_set_value_cansleep(pdata->reset_gpio, 0);
			ret = gpio_direction_input(pdata->reset_gpio);
			if (ret)
				dev_err(&i2c_client->dev,
					"Unable to set direction for gpio [%d]\n",
					pdata->reset_gpio);
			gpio_free(pdata->reset_gpio);
		}
		return 0;
	}

err_reset_gpio_dir:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->irq_gpio);
err_reset_gpio_req:
err_irq_gpio_dir:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_irq_gpio_req:
	return ret;
}

#ifdef CONFIG_PM
static int msg21xx_ts_resume(struct device *dev)
{
	int retval;

	mutex_lock(&ts_data->ts_mutex);
	if (ts_data->suspended) {
		if (ts_data->ts_pinctrl) {
			retval = pinctrl_select_state(ts_data->ts_pinctrl,
					ts_data->pinctrl_state_active);
			if (retval < 0) {
				dev_err(dev, "Cannot get active pinctrl state\n");
				mutex_unlock(&ts_data->ts_mutex);
				return retval;
			}
		}

		retval = msg21xx_ts_gpio_configure(true);
		if (retval) {
			dev_err(dev, "Failed to put gpios in active state %d",
					retval);
			mutex_unlock(&ts_data->ts_mutex);
			return retval;
		}

		enable_irq(ts_data->client->irq);

		retval = msg21xx_ts_power_on();
		if (retval) {
			dev_err(dev, "msg21xx_ts power on failed");
			mutex_unlock(&ts_data->ts_mutex);
			return retval;
		}

		ts_data->suspended = 0;
	} else {
		dev_info(dev, "msg21xx_ts already in resume\n");
	}
	mutex_unlock(&ts_data->ts_mutex);

	return 0;
}

static int msg21xx_ts_suspend(struct device *dev)
{
	int retval;

	if (bFwUpdating) {
		DBG("suspend bFwUpdating=%d\n", bFwUpdating);
		return 0;
	}

#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
	if (bEnableTpProximity) {
		DBG("suspend bEnableTpProximity=%d\n", bEnableTpProximity);
		return 0;
	}
#endif

	mutex_lock(&ts_data->ts_mutex);
	if (ts_data->suspended == 0) {
		disable_irq(ts_data->client->irq);

		touch_driver_touch_released();

		retval = msg21xx_ts_power_off();
		if (retval) {
			dev_err(dev, "msg21xx_ts power off failed");
			mutex_unlock(&ts_data->ts_mutex);
			return retval;
		}

		if (ts_data->ts_pinctrl) {
			retval = pinctrl_select_state(ts_data->ts_pinctrl,
					ts_data->pinctrl_state_suspend);
			if (retval < 0) {
				dev_err(&i2c_client->dev, "Cannot get idle pinctrl state\n");
				mutex_unlock(&ts_data->ts_mutex);
				return retval;
			}
		}

		retval = msg21xx_ts_gpio_configure(false);
		if (retval) {
			dev_err(dev, "Failed to put gpios in idle state %d",
					retval);
			mutex_unlock(&ts_data->ts_mutex);
			return retval;
		}

		ts_data->suspended = 1;
	} else {
		dev_err(dev, "msg21xx_ts already in suspend\n");
	}
	mutex_unlock(&ts_data->ts_mutex);


	return 0;
}
#else
static int msg21xx_ts_resume(struct device *dev)
{
	return 0;
}
static int msg21xx_ts_suspend(struct device *dev)
{
	return 0;
}
#endif

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;

	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK)
			msg21xx_ts_resume(&i2c_client->dev);
		else if (*blank == FB_BLANK_POWERDOWN)
			msg21xx_ts_suspend(&i2c_client->dev);
	}

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void touch_driver_early_suspend(struct early_suspend *p)
{
	DBG("touch_driver_early_suspend()\n");

	if (bFwUpdating) {
		DBG("suspend bFwUpdating=%d\n", bFwUpdating);
		return;
	}

#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
	if (bEnableTpProximity) {
		DBG("suspend bEnableTpProximity=%d\n", bEnableTpProximity);
		return;
	}
#endif

	if (bTpInSuspend == 0) {
		disable_irq(ts_data->client->irq);
		gpio_set_value_cansleep(pdata->reset_gpio, 0);
	}


	if (msg21xx_ts_power_off())
		return;
	bTpInSuspend = 1;
}

static void touch_driver_early_resume(struct early_suspend *p)
{
	DBG("touch_driver_early_resume() bTpInSuspend=%d\n", bTpInSuspend);

	if (bTpInSuspend) {
		gpio_direction_output(pdata->reset_gpio, 1);
		msleep(20);
		gpio_set_value_cansleep(pdata->reset_gpio, 0);
		msleep(20);
		gpio_set_value_cansleep(pdata->reset_gpio, 1);
		msleep(200);

		touch_driver_touch_released();
		input_sync(input_dev);

		enable_irq(ts_data->client->irq);

		if (msg21xx_ts_power_on())
			return;
	}
	bTpInSuspend = 0;
}
#endif

static int msg21xx_get_dt_coords(struct device *dev, char *name,
				struct msg21xx_ts_platform_data *pdata)
{
	u32 coords[FT_COORDS_ARR_SIZE];
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size, rc;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != FT_COORDS_ARR_SIZE) {
		dev_err(dev, "invalid %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(np, name, coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read %s\n", name);
		return rc;
	}

	if (!strcmp(name, "mstar,panel-coords")) {
		pdata->panel_minx = coords[0];
		pdata->panel_miny = coords[1];
		pdata->panel_maxx = coords[2];
		pdata->panel_maxy = coords[3];
	} else if (!strcmp(name, "mstar,display-coords")) {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	} else {
		dev_err(dev, "unsupported property %s\n", name);
		return -EINVAL;
	}

	return 0;
}

static int msg21xx_parse_dt(struct device *dev,
			struct msg21xx_ts_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	struct property *prop;
	u32 temp_val;

	rc = msg21xx_get_dt_coords(dev, "mstar,panel-coords", pdata);
	if (rc && (rc != -EINVAL))
		return rc;

	rc = msg21xx_get_dt_coords(dev, "mstar,display-coords", pdata);
	if (rc)
		return rc;

	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "mstar,reset-gpio",
				0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
		return pdata->reset_gpio;

	pdata->irq_gpio = of_get_named_gpio_flags(np, "mstar,irq-gpio",
				0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
		return pdata->irq_gpio;


	prop = of_find_property(np, "mstar,button-map", NULL);
	if (prop) {
		num_buttons = prop->length / sizeof(temp_val);
		if (num_buttons > MAX_BUTTONS)
			return -EINVAL;

		rc = of_property_read_u32_array(np,
			"mstar,button-map", button_map,
			num_buttons);
		if (rc) {
			dev_err(dev, "Unable to read key codes\n");
			return rc;
		}
	}

	return 0;
}

/* probe function is used for matching and initializing input device */
static int msg21xx_ts_probe(struct i2c_client *client,
		const struct i2c_device_id *id) {
#ifdef FIRMWARE_AUTOUPDATE
	unsigned short update_bin_major = 0, update_bin_minor = 0;
	int i, update_flag = 0;
#endif
	int ret = 0;

	if (input_dev != NULL) {
		DBG("input device has found\n");
		return -EINVAL;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct msg21xx_ts_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = msg21xx_parse_dt(&client->dev, pdata);
		if (ret) {
			dev_err(&client->dev, "DT parsing failed\n");
			return ret;
		}
	} else
		pdata = client->dev.platform_data;

	ts_data = devm_kzalloc(&client->dev,
			sizeof(struct msg21xx_ts_data), GFP_KERNEL);
	if (!ts_data) {
		dev_err(&client->dev, "Not enough memory\n");
		return -ENOMEM;
	}

	DBG("*** %s ***\n", __func__);

	i2c_client = client;

	ret = msg21xx_ts_power_init();
	if (ret)
		dev_err(&client->dev, "Mstar power init failed\n");

	ret = msg21xx_ts_power_on();
	if (ret) {
		dev_err(&client->dev, "Mstar power on failed\n");
		goto exit_deinit_power;
	}

	ret = msg21xx_pinctrl_init();
	if (!ret && ts_data->ts_pinctrl) {
		ret = pinctrl_select_state(ts_data->ts_pinctrl,
				ts_data->pinctrl_state_active);
		if (ret < 0)
			goto exit_pinctrl_select;
	} else {
		goto exit_pinctrl_init;
	}

	ret = msg21xx_ts_gpio_configure(true);
	if (ret) {
		dev_err(&client->dev, "Failed to configure gpio %d\n", ret);
		goto exit_gpio_config;
	}

	if (0 == get_ic_type()) {
		pr_err("the currnet ic is not Mstar\n");
		ret = -1;
		goto err_wrong_ic_type;
	}

	mutex_init(&msg21xx_mutex);
	mutex_init(&ts_data->ts_mutex);

	/* allocate an input device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		ret = -ENOMEM;
		pr_err("*** input device allocation failed ***\n");
		goto err_input_allocate_dev;
	}

	input_dev->name = client->name;
	input_dev->phys = "I2C";
	input_dev->dev.parent = &client->dev;
	input_dev->id.bustype = BUS_I2C;

	/* set the supported event type for input device */
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	ts_data->input_dev = input_dev;
	ts_data->client = client;
	ts_data->pdata = pdata;

	input_set_drvdata(input_dev, ts_data);
	i2c_set_clientdata(client, ts_data);

#ifdef CONFIG_TP_HAVE_KEY
	{
		int i;
		for (i = 0; i < num_buttons; i++)
			input_set_capability(input_dev, EV_KEY, button_map[i]);
	}
#endif

	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
				0, 2, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			TOUCH_SCREEN_X_MIN, pdata->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			TOUCH_SCREEN_Y_MIN, pdata->y_max, 0, 0);
	input_mt_init_slots(input_dev, MAX_TOUCH_NUM, 0);

	/* register the input device to input sub-system */
	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("*** Unable to register ms-touchscreen input device ***\n");
		goto err_input_reg_dev;
	}

	/* set sysfs for firmware */
	firmware_class = class_create(THIS_MODULE, "ms-touchscreen-msg20xx");
	if (IS_ERR(firmware_class))
		pr_err("Failed to create class(firmware)!\n");

	firmware_cmd_dev = device_create(firmware_class, NULL, 0,
					NULL, "device");
	if (IS_ERR(firmware_cmd_dev))
		pr_err("Failed to create device(firmware_cmd_dev)!\n");

	/* version*/
	if (device_create_file(firmware_cmd_dev, &dev_attr_version) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_version.attr.name);
	/* update*/
	if (device_create_file(firmware_cmd_dev, &dev_attr_update) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_update.attr.name);
	/* data*/
	if (device_create_file(firmware_cmd_dev, &dev_attr_data) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_data.attr.name);

#ifdef TP_PRINT
	tp_print_create_entry();
#endif

	dev_set_drvdata(firmware_cmd_dev, NULL);

	ret = request_threaded_irq(client->irq, NULL,
				msg21xx_ts_interrupt,
				pdata->irq_gpio_flags | IRQF_ONESHOT,
				"msg21xx", ts_data);
	if (ret)
		goto err_req_irq;

	disable_irq(ts_data->client->irq);

#if defined(CONFIG_FB)
	ts_data->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts_data->fb_notif);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	mstar_ts_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	mstar_ts_early_suspend.suspend = touch_driver_early_suspend;
	mstar_ts_early_suspend.resume = touch_driver_early_resume;
	register_early_suspend(&mstar_ts_early_suspend);
#endif

#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
	tsps_assist_register_callback("msg21xx", &tsps_msg21xx_enable,
				&tsps_msg21xx_data);
#endif

#ifdef FIRMWARE_AUTOUPDATE
	get_customer_firmware_version();
	_ReadBinConfig();

	if (main_sw_id == info_sw_id) {
		if (_CalMainCRC32() == bin_conf_crc32) {
			if ((main_sw_id >= SWID_START) &&
						(main_sw_id < SWID_NULL)) {
				update_bin_major = (MSG_FIRMWARE
				[main_sw_id - SWID_START][0x7f4f] << 8)
				+ MSG_FIRMWARE[main_sw_id - SWID_START][0x7f4e];
				update_bin_minor = (MSG_FIRMWARE
				[main_sw_id - SWID_START][0x7f51] << 8)
				+ MSG_FIRMWARE[main_sw_id - SWID_START][0x7f50];

				/*check upgrading*/
				if ((update_bin_major == fw_version_major) &&
					(update_bin_minor > fw_version_minor)) {
					update_flag = 1;
				}
			}
		} else {
			if ((info_sw_id >= SWID_START) &&
				(info_sw_id < SWID_NULL)) {
				update_bin_major = (MSG_FIRMWARE
					[info_sw_id - SWID_START][0x7f4f] << 8)
					+ MSG_FIRMWARE
					[info_sw_id - SWID_START][0x7f4e];
				update_bin_minor = (MSG_FIRMWARE
					[info_sw_id - SWID_START][0x7f51] << 8)
					+ MSG_FIRMWARE
					[info_sw_id - SWID_START][0x7f50];
				update_flag = 1;
			}
		}
	} else {
		if ((info_sw_id >= SWID_START) && (info_sw_id < SWID_NULL)) {
			update_bin_major = (MSG_FIRMWARE
					[info_sw_id - SWID_START][0x7f4f] << 8)
					+ MSG_FIRMWARE
					[info_sw_id - SWID_START][0x7f4e];
			update_bin_minor = (MSG_FIRMWARE
					[info_sw_id - SWID_START][0x7f51] << 8)
					+ MSG_FIRMWARE
					[info_sw_id - SWID_START][0x7f50];
			update_flag = 1;
		}
	}

	if (update_flag == 1) {
		DBG("MSG21XX_fw_auto_update begin....\n");
		/*transfer data*/
		for (i = 0; i < 33; i++) {
			firmware_data_store(NULL, NULL,
			&(MSG_FIRMWARE[info_sw_id - SWID_START][i * 1024]),
			1024);
		}

		kthread_run(fwAutoUpdate, 0, "MSG21XX_fw_auto_update");
		DBG("*** mstar touch screen registered ***\n");
		return 0;
	}

	reset_hw();
#endif

	DBG("*** mstar touch screen registered ***\n");
	enable_irq(ts_data->client->irq);
	return 0;

err_req_irq:
	free_irq(ts_data->client->irq, input_dev);

err_input_reg_dev:
err_input_allocate_dev:
	mutex_destroy(&msg21xx_mutex);
	mutex_destroy(&ts_data->ts_mutex);
	input_unregister_device(input_dev);
	input_free_device(input_dev);
	input_dev = NULL;

err_wrong_ic_type:
	msg21xx_ts_gpio_configure(false);
exit_gpio_config:
exit_pinctrl_select:
	if (ts_data->ts_pinctrl) {
		if (IS_ERR_OR_NULL(ts_data->pinctrl_state_release)) {
			devm_pinctrl_put(ts_data->ts_pinctrl);
			ts_data->ts_pinctrl = NULL;
		} else {
			ret = pinctrl_select_state(ts_data->ts_pinctrl,
					ts_data->pinctrl_state_release);
			if (ret < 0)
				pr_err("Cannot get release pinctrl state\n");
		}
	}
exit_pinctrl_init:
	msg21xx_ts_power_off();
exit_deinit_power:
	msg21xx_ts_power_deinit();

	return ret;
}

/* remove function is triggered when the input device is removed
from input sub-system */
static int touch_driver_remove(struct i2c_client *client)
{
	int retval = 0;

	DBG("touch_driver_remove()\n");

	free_irq(ts_data->client->irq, input_dev);
	gpio_free(pdata->irq_gpio);
	gpio_free(pdata->reset_gpio);

	if (ts_data->ts_pinctrl) {
		if (IS_ERR_OR_NULL(ts_data->pinctrl_state_release)) {
			devm_pinctrl_put(ts_data->ts_pinctrl);
			ts_data->ts_pinctrl = NULL;
		} else {
			retval = pinctrl_select_state(ts_data->ts_pinctrl,
					ts_data->pinctrl_state_release);
			if (retval < 0)
				pr_err("Cannot get release pinctrl state\n");
		}
	}

	input_unregister_device(input_dev);
	mutex_destroy(&msg21xx_mutex);
	mutex_destroy(&ts_data->ts_mutex);

	return retval;
}

/* The I2C device list is used for matching I2C device
and I2C device driver. */
static const struct i2c_device_id touch_device_id[] = {
	{"msg21xx", 0},
	{}, /* should not omitted */
};

static struct of_device_id msg21xx_match_table[] = {
	{ .compatible = "mstar,msg21xx", },
	{ },
};

MODULE_DEVICE_TABLE(i2c, touch_device_id);

static struct i2c_driver touch_device_driver = {
	.driver = {
		.name = "ms-msg21xx",
		.owner = THIS_MODULE,
		.of_match_table = msg21xx_match_table,
	},
	.probe = msg21xx_ts_probe,
	.remove = touch_driver_remove,
	.id_table = touch_device_id,
};

static int __init touch_driver_init(void)
{
	int ret;

	/* register driver */
	ret = i2c_add_driver(&touch_device_driver);
	if (ret < 0) {
		DBG("add touch_device_driver i2c driver failed.\n");
		return -ENODEV;
	}
	DBG("add touch_device_driver i2c driver.\n");

	return ret;
}

static void __exit touch_driver_exit(void)
{
	DBG("remove touch_device_driver i2c driver.\n");

	i2c_del_driver(&touch_device_driver);
}

#ifdef TP_PRINT
#include <linux/proc_fs.h>

static unsigned short InfoAddr = 0x0F, PoolAddr = 0x10, TransLen = 256;
static unsigned char row, units, cnt;

static int tp_print_proc_read(void)
{
	unsigned short i, j;
	unsigned short left, offset = 0;
	unsigned char dbbus_tx_data[3] = {0};
	unsigned char u8Data;
	signed short s16Data;
	int s32Data;
	char *buf = NULL;

	left = cnt*row*units;
	if ((ts_data->suspended == 0) &&
				(InfoAddr != 0x0F) &&
				(PoolAddr != 0x10) &&
				(left > 0)) {
		buf = kmalloc(left, GFP_KERNEL);
		if (buf != NULL) {

			while (left > 0) {
				dbbus_tx_data[0] = 0x53;
				dbbus_tx_data[1] = ((PoolAddr + offset) >> 8)
									& 0xFF;
				dbbus_tx_data[2] = (PoolAddr + offset) & 0xFF;
				mutex_lock(&msg21xx_mutex);
				write_i2c_seq(ts_data->client->addr,
							&dbbus_tx_data[0], 3);
				read_i2c_seq(ts_data->client->addr,
					&buf[offset],
					left > TransLen ? TransLen : left);
				mutex_unlock(&msg21xx_mutex);

				if (left > TransLen) {
					left -= TransLen;
					offset += TransLen;
				} else {
					left = 0;
				}
			}

			for (i = 0; i < cnt; i++) {
				for (j = 0; j < row; j++) {
					if (units == 1) {
						u8Data = buf[i * row * units +
								j * units];
					} else if (units == 2) {
						s16Data = buf[i * row * units +
						j * units] +
						(buf[i * row * units +
						j * units + 1] << 8);
					} else if (units == 4) {
						s32Data = buf[i * row * units +
						j * units] +
						(buf[i * row * units +
						j * units + 1] << 8) +
						(buf[i * row * units +
						j * units + 2] << 16) +
						(buf[i * row * units +
						j * units + 3] << 24);
					}
				}
			}

			kfree(buf);
		}
	}

	return 0;
}

static void tp_print_create_entry(void)
{
	unsigned char dbbus_tx_data[3] = {0};
	unsigned char dbbus_rx_data[8] = {0};

	dbbus_tx_data[0] = 0x53;
	dbbus_tx_data[1] = 0x00;
	dbbus_tx_data[2] = 0x58;
	mutex_lock(&msg21xx_mutex);
	write_i2c_seq(ts_data->client->addr, &dbbus_tx_data[0], 3);
	read_i2c_seq(ts_data->client->addr, &dbbus_rx_data[0], 4);
	mutex_unlock(&msg21xx_mutex);
	InfoAddr = (dbbus_rx_data[1]<<8) + dbbus_rx_data[0];
	PoolAddr = (dbbus_rx_data[3]<<8) + dbbus_rx_data[2];

	if ((InfoAddr != 0x0F) && (PoolAddr != 0x10)) {
		msleep(20);
		dbbus_tx_data[0] = 0x53;
		dbbus_tx_data[1] = (InfoAddr >> 8) & 0xFF;
		dbbus_tx_data[2] = InfoAddr & 0xFF;
		mutex_lock(&msg21xx_mutex);
		write_i2c_seq(ts_data->client->addr, &dbbus_tx_data[0], 3);
		read_i2c_seq(ts_data->client->addr, &dbbus_rx_data[0], 8);
		mutex_unlock(&msg21xx_mutex);

		units = dbbus_rx_data[0];
		row = dbbus_rx_data[1];
		cnt = dbbus_rx_data[2];
		TransLen = (dbbus_rx_data[7]<<8) + dbbus_rx_data[6];

		if (device_create_file(firmware_cmd_dev, &dev_attr_tpp) < 0) {
			pr_err("Failed to create device file(%s)!\n",
					dev_attr_tpp.attr.name);
		}
	}
}
#endif


module_init(touch_driver_init);
module_exit(touch_driver_exit);
MODULE_AUTHOR("MStar Semiconductor, Inc.");
MODULE_LICENSE("GPL v2");
