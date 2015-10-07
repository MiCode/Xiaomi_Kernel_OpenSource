/*
 * MStar MSG21XX touchscreen driver
 *
 * Copyright (c) 2006-2012 MStar Semiconductor, Inc.
 *
 * Copyright (C) 2012 Bruce Ding <bruce.ding@mstarsemi.com>
 *
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/regulator/consumer.h>

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
#include <linux/input/vir_ps.h>
#endif

/* Constant Value & Variable Definition*/

#define MSTAR_VTG_MIN_UV	2800000
#define MSTAR_VTG_MAX_UV	3300000
#define MSTAR_I2C_VTG_MIN_UV	1800000
#define MSTAR_I2C_VTG_MAX_UV	1800000

#define MAX_BUTTONS		4
#define FT_COORDS_ARR_SIZE	4
#define MSTAR_FW_NAME_MAX_LEN	50

#define MSTAR_CHIPTOP_REGISTER_BANK	0x1E
#define MSTAR_CHIPTOP_REGISTER_ICTYPE 0xCC
#define MSTAR_INIT_SW_ID 0x7FF
#define MSTAR_DEBUG_DIR_NAME "ts_debug"

#define MSG_FW_FILE_MAJOR_VERSION(x) \
	(((x)->data[0x7f4f] << 8) + ((x)->data[0x7f4e]))

#define MSG_FW_FILE_MINOR_VERSION(x) \
	(((x)->data[0x7f51] << 8) + ((x)->data[0x7f50]))

/*
 * Note.
 * Please do not change the below setting.
 */
#define TPD_WIDTH   (2048)
#define TPD_HEIGHT  (2048)

#define PINCTRL_STATE_ACTIVE	"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND	"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE	"pmx_ts_release"

#define SLAVE_I2C_ID_DBBUS		 (0xC4>>1)

#define DEMO_MODE_PACKET_LENGTH	(8)

#define TP_PRINT

/*store the frimware binary data*/
static unsigned char fw_bin_data[94][1024];
static unsigned int crc32_table[256];

static unsigned short fw_file_major, fw_file_minor;
static unsigned short main_sw_id = MSTAR_INIT_SW_ID;
static unsigned short info_sw_id = MSTAR_INIT_SW_ID;
static unsigned int bin_conf_crc32;

struct msg21xx_ts_platform_data {
	const char *name;
	char fw_name[MSTAR_FW_NAME_MAX_LEN];
	u8 fw_version_major;
	u8 fw_version_minor;
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 panel_minx;
	u32 panel_miny;
	u32 panel_maxx;
	u32 panel_maxy;
	u32 num_max_touches;
	u8 ic_type;
	u32 button_map[MAX_BUTTONS];
	u32 num_buttons;
	u32 hard_reset_delay_ms;
	u32 post_hard_reset_delay_ms;
	bool updating_fw;
};

/* Touch Data Type Definition */
struct touchPoint_t {
	unsigned short x;
	unsigned short y;
};

struct touchInfo_t {
	struct touchPoint_t *point;
	unsigned char count;
	unsigned char keycode;
};

struct msg21xx_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct msg21xx_ts_platform_data *pdata;
	struct regulator *vdd;
	struct regulator *vcc_i2c;
	bool suspended;
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#endif
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	struct pinctrl_state *pinctrl_state_release;
	struct mutex ts_mutex;
	struct touchInfo_t info;
};

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data);
#endif

#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
static unsigned char bEnableTpProximity;
static unsigned char bFaceClosingTp;
#endif

#ifdef TP_PRINT
static int tp_print_proc_read(struct msg21xx_ts_data *ts_data);
static void tp_print_create_entry(struct msg21xx_ts_data *ts_data);
#endif

static void _ReadBinConfig(struct msg21xx_ts_data *ts_data);
static unsigned int _CalMainCRC32(struct msg21xx_ts_data *ts_data);

static struct mutex msg21xx_mutex;

enum EMEM_TYPE_t {
	EMEM_ALL = 0,
	EMEM_MAIN,
	EMEM_INFO,
};

/* Function Definition */

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

static void msg21xx_reset_hw(struct msg21xx_ts_platform_data *pdata)
{
	unsigned int delay;

	gpio_direction_output(pdata->reset_gpio, 1);
	gpio_set_value_cansleep(pdata->reset_gpio, 0);
	/* Note that the RST must be in LOW 10ms at least */
	delay = pdata->hard_reset_delay_ms * 1000;
	usleep_range(delay, delay + 1);
	gpio_set_value_cansleep(pdata->reset_gpio, 1);
	/* Enable the interrupt service thread/routine for INT after 50ms */
	delay = pdata->post_hard_reset_delay_ms * 1000;
	usleep_range(delay, delay + 1);
}

static int read_i2c_seq(struct msg21xx_ts_data *ts_data, unsigned char addr,
			unsigned char *buf, unsigned short size)
{
	int rc = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = addr,
			.flags = I2C_M_RD, /* read flag */
			.len = size,
			.buf = buf,
		},
	};

	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	if (ts_data->client != NULL) {
		rc = i2c_transfer(ts_data->client->adapter, msgs, 1);
		if (rc < 0)
			dev_err(&ts_data->client->dev,
				"%s error %d\n", __func__, rc);
	} else {
		dev_err(&ts_data->client->dev, "ts_data->client is NULL\n");
	}

	return rc;
}

static int write_i2c_seq(struct msg21xx_ts_data *ts_data, unsigned char addr,
			unsigned char *buf, unsigned short size)
{
	int rc = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = addr,
			/*
			 * if read flag is undefined,
			 * then it means write flag.
			 */
			.flags = 0,
			.len = size,
			.buf = buf,
		},
	};

	/*
	 * If everything went ok (i.e. 1 msg transmitted), return #bytes
	 * transmitted, else error code.
	 */
	if (ts_data->client != NULL) {
		rc = i2c_transfer(ts_data->client->adapter, msgs, 1);
		if (rc < 0)
			dev_err(&ts_data->client->dev,
				"%s error %d\n", __func__, rc);
	} else {
		dev_err(&ts_data->client->dev, "ts_data->client is NULL\n");
	}

	return rc;
}

static unsigned short read_reg(struct msg21xx_ts_data *ts_data,
					unsigned char bank, unsigned char addr)
{
	unsigned char tx_data[3] = {0x10, bank, addr};
	unsigned char rx_data[2] = {0};

	write_i2c_seq(ts_data, SLAVE_I2C_ID_DBBUS, tx_data, sizeof(tx_data));
	read_i2c_seq(ts_data, SLAVE_I2C_ID_DBBUS, rx_data, sizeof(rx_data));

	return rx_data[1] << 8 | rx_data[0];
}

static void write_reg(struct msg21xx_ts_data *ts_data, unsigned char bank,
				unsigned char addr,
						unsigned short data)
{
	unsigned char tx_data[5] = {0x10, bank, addr, data & 0xFF, data >> 8};

	write_i2c_seq(ts_data, SLAVE_I2C_ID_DBBUS, tx_data, sizeof(tx_data));
}

static void write_reg_8bit(struct msg21xx_ts_data *ts_data, unsigned char bank,
				unsigned char addr,
						unsigned char data)
{
	unsigned char tx_data[4] = {0x10, bank, addr, data};

	write_i2c_seq(ts_data, SLAVE_I2C_ID_DBBUS, tx_data, sizeof(tx_data));
}

static void dbbusDWIICEnterSerialDebugMode(struct msg21xx_ts_data *ts_data)
{
	unsigned char data[5];

	/* Enter the Serial Debug Mode */
	data[0] = 0x53;
	data[1] = 0x45;
	data[2] = 0x52;
	data[3] = 0x44;
	data[4] = 0x42;

	write_i2c_seq(ts_data, SLAVE_I2C_ID_DBBUS, data, sizeof(data));
}

static void dbbusDWIICStopMCU(struct msg21xx_ts_data *ts_data)
{
	unsigned char data[1];

	/* Stop the MCU */
	data[0] = 0x37;

	write_i2c_seq(ts_data, SLAVE_I2C_ID_DBBUS, data, sizeof(data));
}

static void dbbusDWIICIICUseBus(struct msg21xx_ts_data *ts_data)
{
	unsigned char data[1];

	/* IIC Use Bus */
	data[0] = 0x35;

	write_i2c_seq(ts_data, SLAVE_I2C_ID_DBBUS, data, sizeof(data));
}

static void dbbusDWIICIICReshape(struct msg21xx_ts_data *ts_data)
{
	unsigned char data[1];

	/* IIC Re-shape */
	data[0] = 0x71;

	write_i2c_seq(ts_data, SLAVE_I2C_ID_DBBUS, data, sizeof(data));
}

static unsigned char msg21xx_get_ic_type(struct msg21xx_ts_data *ts_data)
{
	unsigned char ic_type = 0;
	unsigned char bank;
	unsigned char addr;

	msg21xx_reset_hw(ts_data->pdata);
	dbbusDWIICEnterSerialDebugMode(ts_data);
	dbbusDWIICStopMCU(ts_data);
	dbbusDWIICIICUseBus(ts_data);
	dbbusDWIICIICReshape(ts_data);
	msleep(300);

	/* stop mcu */
	write_reg_8bit(ts_data, 0x0F, 0xE6, 0x01);
	/* disable watch dog */
	write_reg(ts_data, 0x3C, 0x60, 0xAA55);
	/* get ic type */
	bank = MSTAR_CHIPTOP_REGISTER_BANK;
	addr = MSTAR_CHIPTOP_REGISTER_ICTYPE;
	ic_type = (0xff)&(read_reg(ts_data, bank, addr));

	if (ic_type != ts_data->pdata->ic_type)
		ic_type = 0;

	msg21xx_reset_hw(ts_data->pdata);

	return ic_type;
}

static int msg21xx_read_firmware_id(struct msg21xx_ts_data *ts_data)
{
	unsigned char command[3] = { 0x53, 0x00, 0x2A};
	unsigned char response[4] = { 0 };

	mutex_lock(&msg21xx_mutex);
	write_i2c_seq(ts_data, ts_data->client->addr, command, sizeof(command));
	read_i2c_seq(ts_data, ts_data->client->addr, response,
				sizeof(response));
	mutex_unlock(&msg21xx_mutex);
	ts_data->pdata->fw_version_major = (response[1]<<8) + response[0];
	ts_data->pdata->fw_version_minor = (response[3]<<8) + response[2];

	dev_info(&ts_data->client->dev, "major num = %d, minor num = %d\n",
			ts_data->pdata->fw_version_major,
			ts_data->pdata->fw_version_minor);

	return 0;
}

static int firmware_erase_c33(struct msg21xx_ts_data *ts_data,
					enum EMEM_TYPE_t emem_type)
{
	/* stop mcu */
	write_reg(ts_data, 0x0F, 0xE6, 0x0001);

	/* disable watch dog */
	write_reg_8bit(ts_data, 0x3C, 0x60, 0x55);
	write_reg_8bit(ts_data, 0x3C, 0x61, 0xAA);

	/* set PROGRAM password */
	write_reg_8bit(ts_data, 0x16, 0x1A, 0xBA);
	write_reg_8bit(ts_data, 0x16, 0x1B, 0xAB);

	write_reg_8bit(ts_data, 0x16, 0x18, 0x80);

	if (emem_type == EMEM_ALL)
		write_reg_8bit(ts_data, 0x16, 0x08, 0x10);

	write_reg_8bit(ts_data, 0x16, 0x18, 0x40);
	msleep(20);

	/* clear pce */
	write_reg_8bit(ts_data, 0x16, 0x18, 0x80);

	/* erase trigger */
	if (emem_type == EMEM_MAIN)
		write_reg_8bit(ts_data, 0x16, 0x0E, 0x04); /* erase main */
	else
		write_reg_8bit(ts_data, 0x16, 0x0E, 0x08); /* erase all block */

	return 0;
}

static ssize_t firmware_update_c33(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size,
						enum EMEM_TYPE_t emem_type,
						bool isForce) {
	unsigned int i, j;
	unsigned int crc_main, crc_main_tp;
	unsigned int crc_info, crc_info_tp;
	unsigned short reg_data = 0;
	int update_pass = 1;
	bool fw_upgrade = false;
	struct msg21xx_ts_data *ts_data = dev_get_drvdata(dev);

	crc_main = 0xffffffff;
	crc_info = 0xffffffff;

	msg21xx_reset_hw(ts_data->pdata);

	msg21xx_read_firmware_id(ts_data);
	_ReadBinConfig(ts_data);
	if ((main_sw_id == info_sw_id) &&
		(_CalMainCRC32(ts_data) == bin_conf_crc32) &&
		(fw_file_major == ts_data->pdata->fw_version_major) &&
		(fw_file_minor > ts_data->pdata->fw_version_minor)) {
		fw_upgrade = true;
	}

	if (!fw_upgrade && !isForce) {
		dev_dbg(dev, "no need to update\n");
		msg21xx_reset_hw(ts_data->pdata);
		return size;
	}
	msg21xx_reset_hw(ts_data->pdata);
	msleep(300);

	dbbusDWIICEnterSerialDebugMode(ts_data);
	dbbusDWIICStopMCU(ts_data);
	dbbusDWIICIICUseBus(ts_data);
	dbbusDWIICIICReshape(ts_data);
	msleep(300);

	/* erase main */
	firmware_erase_c33(ts_data, EMEM_MAIN);
	msleep(1000);

	msg21xx_reset_hw(ts_data->pdata);
	dbbusDWIICEnterSerialDebugMode(ts_data);
	dbbusDWIICStopMCU(ts_data);
	dbbusDWIICIICUseBus(ts_data);
	dbbusDWIICIICReshape(ts_data);
	msleep(300);

	/*
	 * Program
	 */

	/* polling 0x3CE4 is 0x1C70 */
	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		do {
			reg_data = read_reg(ts_data, 0x3C, 0xE4);
		} while (reg_data != 0x1C70);
	}

	switch (emem_type) {
	case EMEM_ALL:
		write_reg(ts_data, 0x3C, 0xE4, 0xE38F);  /* for all-blocks */
		break;
	case EMEM_MAIN:
		write_reg(ts_data, 0x3C, 0xE4, 0x7731);  /* for main block */
		break;
	case EMEM_INFO:
		write_reg(ts_data, 0x3C, 0xE4, 0x7731);  /* for info block */

		write_reg_8bit(ts_data, 0x0F, 0xE6, 0x01);

		write_reg_8bit(ts_data, 0x3C, 0xE4, 0xC5);
		write_reg_8bit(ts_data, 0x3C, 0xE5, 0x78);

		write_reg_8bit(ts_data, MSTAR_CHIPTOP_REGISTER_BANK,
						0x04, 0x9F);
		write_reg_8bit(ts_data, MSTAR_CHIPTOP_REGISTER_BANK,
						0x05, 0x82);

		write_reg_8bit(ts_data, 0x0F, 0xE6, 0x00);
		msleep(100);
		break;
	}

	/* polling 0x3CE4 is 0x2F43 */
	do {
		reg_data = read_reg(ts_data, 0x3C, 0xE4);
	} while (reg_data != 0x2F43);

	/* calculate CRC 32 */
	_CRC_initTable();

	/* total  32 KB : 2 byte per R/W */
	for (i = 0; i < 32; i++) {
		if (i == 31) {
			fw_bin_data[i][1014] = 0x5A;
			fw_bin_data[i][1015] = 0xA5;

			for (j = 0; j < 1016; j++)
				crc_main = _CRC_getValue(fw_bin_data[i][j],
							crc_main);
		} else {
			for (j = 0; j < 1024; j++)
				crc_main = _CRC_getValue(fw_bin_data[i][j],
							crc_main);
		}

		for (j = 0; j < 8; j++)
			write_i2c_seq(ts_data, ts_data->client->addr,
						&fw_bin_data[i][j * 128], 128);
		msleep(100);

		/* polling 0x3CE4 is 0xD0BC */
		do {
			reg_data = read_reg(ts_data, 0x3C, 0xE4);
		} while (reg_data != 0xD0BC);

		write_reg(ts_data, 0x3C, 0xE4, 0x2F43);
	}

	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		/* write file done and check crc */
		write_reg(ts_data, 0x3C, 0xE4, 0x1380);
	}
	msleep(20);

	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		/* polling 0x3CE4 is 0x9432 */
		do {
			reg_data = read_reg(ts_data, 0x3C, 0xE4);
		} while (reg_data != 0x9432);
	}

	crc_main = crc_main ^ 0xffffffff;
	crc_info = crc_info ^ 0xffffffff;

	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		/* CRC Main from TP */
		crc_main_tp = read_reg(ts_data, 0x3C, 0x80);
		crc_main_tp = (crc_main_tp << 16) |
						read_reg(ts_data, 0x3C, 0x82);

		/* CRC Info from TP */
		crc_info_tp = read_reg(ts_data, 0x3C, 0xA0);
		crc_info_tp = (crc_info_tp << 16) |
						read_reg(ts_data, 0x3C, 0xA2);
	}

	update_pass = 1;
	if ((emem_type == EMEM_ALL) || (emem_type == EMEM_MAIN)) {
		if (crc_main_tp != crc_main)
			update_pass = 0;
	}

	if (!update_pass) {
		dev_err(dev, "update_C33 failed\n");
		msg21xx_reset_hw(ts_data->pdata);
		return 0;
	}

	dev_dbg(dev, "update_C33 OK\n");
	msg21xx_reset_hw(ts_data->pdata);
	return size;
}

static unsigned int _CalMainCRC32(struct msg21xx_ts_data *ts_data)
{
	unsigned int ret = 0;
	unsigned short reg_data = 0;

	msg21xx_reset_hw(ts_data->pdata);

	dbbusDWIICEnterSerialDebugMode(ts_data);
	dbbusDWIICStopMCU(ts_data);
	dbbusDWIICIICUseBus(ts_data);
	dbbusDWIICIICReshape(ts_data);
	msleep(100);

	/* Stop MCU */
	write_reg(ts_data, 0x0F, 0xE6, 0x0001);

	/* Stop Watchdog */
	write_reg_8bit(ts_data, 0x3C, 0x60, 0x55);
	write_reg_8bit(ts_data, 0x3C, 0x61, 0xAA);

	/* cmd */
	write_reg(ts_data, 0x3C, 0xE4, 0xDF4C);
	write_reg(ts_data, MSTAR_CHIPTOP_REGISTER_BANK, 0x04, 0x7d60);
	/* TP SW reset */
	write_reg(ts_data, MSTAR_CHIPTOP_REGISTER_BANK, 0x04, 0x829F);

	/* MCU run */
	write_reg(ts_data, 0x0F, 0xE6, 0x0000);

	/* polling 0x3CE4 */
	do {
		reg_data = read_reg(ts_data, 0x3C, 0xE4);
	} while (reg_data != 0x9432);

	/* Cal CRC Main from TP */
	ret = read_reg(ts_data, 0x3C, 0x80);
	ret = (ret << 16) | read_reg(ts_data, 0x3C, 0x82);

	dev_dbg(&ts_data->client->dev,
			"[21xxA]:Current main crc32=0x%x\n", ret);
	return ret;
}

static void _ReadBinConfig(struct msg21xx_ts_data *ts_data)
{
	unsigned char dbbus_tx_data[5] = {0};
	unsigned char dbbus_rx_data[4] = {0};
	unsigned short reg_data = 0;

	msg21xx_reset_hw(ts_data->pdata);

	dbbusDWIICEnterSerialDebugMode(ts_data);
	dbbusDWIICStopMCU(ts_data);
	dbbusDWIICIICUseBus(ts_data);
	dbbusDWIICIICReshape(ts_data);
	msleep(100);

	/* Stop MCU */
	write_reg(ts_data, 0x0F, 0xE6, 0x0001);

	/* Stop Watchdog */
	write_reg_8bit(ts_data, 0x3C, 0x60, 0x55);
	write_reg_8bit(ts_data, 0x3C, 0x61, 0xAA);

	/* cmd */
	write_reg(ts_data, 0x3C, 0xE4, 0xA4AB);
	write_reg(ts_data, MSTAR_CHIPTOP_REGISTER_BANK, 0x04, 0x7d60);

	/* TP SW reset */
	write_reg(ts_data, MSTAR_CHIPTOP_REGISTER_BANK, 0x04, 0x829F);

	/* MCU run */
	write_reg(ts_data, 0x0F, 0xE6, 0x0000);

	/* polling 0x3CE4 */
	do {
		reg_data = read_reg(ts_data, 0x3C, 0xE4);
	} while (reg_data != 0x5B58);

	dbbus_tx_data[0] = 0x72;
	dbbus_tx_data[1] = 0x7F;
	dbbus_tx_data[2] = 0x55;
	dbbus_tx_data[3] = 0x00;
	dbbus_tx_data[4] = 0x04;
	write_i2c_seq(ts_data, ts_data->client->addr, &dbbus_tx_data[0], 5);
	read_i2c_seq(ts_data, ts_data->client->addr, &dbbus_rx_data[0], 4);
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
	write_i2c_seq(ts_data, ts_data->client->addr, &dbbus_tx_data[0], 5);
	read_i2c_seq(ts_data, ts_data->client->addr, &dbbus_rx_data[0], 4);
	bin_conf_crc32 = (dbbus_rx_data[0] << 24) |
			(dbbus_rx_data[1] << 16) |
			(dbbus_rx_data[2] << 8) |
			(dbbus_rx_data[3]);

	dbbus_tx_data[0] = 0x72;
	dbbus_tx_data[1] = 0x83;
	dbbus_tx_data[2] = 0x00;
	dbbus_tx_data[3] = 0x00;
	dbbus_tx_data[4] = 0x04;
	write_i2c_seq(ts_data, ts_data->client->addr, &dbbus_tx_data[0], 5);
	read_i2c_seq(ts_data, ts_data->client->addr, &dbbus_rx_data[0], 4);
	if ((dbbus_rx_data[0] >= 0x30 && dbbus_rx_data[0] <= 0x39)
		&& (dbbus_rx_data[1] >= 0x30 && dbbus_rx_data[1] <= 0x39)
		&& (dbbus_rx_data[2] >= 0x31 && dbbus_rx_data[2] <= 0x39)) {
		info_sw_id = (dbbus_rx_data[0] - 0x30) * 100 +
					(dbbus_rx_data[1] - 0x30) * 10 +
					(dbbus_rx_data[2] - 0x30);
	}

	dev_dbg(&ts_data->client->dev,
		"[21xxA]:main_sw_id = %d, info_sw_id = %d, bin_conf_crc32 = 0x%x\n",
		main_sw_id, info_sw_id, bin_conf_crc32);
}

static ssize_t firmware_update_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct msg21xx_ts_data *ts_data = dev_get_drvdata(dev);

	return snprintf(buf, 3, "%d\n", ts_data->pdata->updating_fw);
}

static ssize_t firmware_update_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t size)
{
	struct msg21xx_ts_data *ts_data = dev_get_drvdata(dev);

	ts_data->pdata->updating_fw = true;
	disable_irq(ts_data->client->irq);

	size = firmware_update_c33(dev, attr, buf, size, EMEM_MAIN, false);

	enable_irq(ts_data->client->irq);
	ts_data->pdata->updating_fw = false;

	return size;
}

static DEVICE_ATTR(update, (S_IRUGO | S_IWUSR),
					firmware_update_show,
					firmware_update_store);

static int prepare_fw_data(struct device *dev)
{
	int count;
	int i;
	int ret;
	const struct firmware *fw = NULL;
	struct msg21xx_ts_data *ts_data = dev_get_drvdata(dev);

	ret = request_firmware(&fw, ts_data->pdata->fw_name, dev);
	if (ret < 0) {
		dev_err(dev, "Request firmware failed - %s (%d)\n",
						ts_data->pdata->fw_name, ret);
		return ret;
	}

	count = fw->size / 1024;

	for (i = 0; i < count; i++)
		memcpy(fw_bin_data[i], fw->data + (i * 1024), 1024);

	fw_file_major = MSG_FW_FILE_MAJOR_VERSION(fw);
	fw_file_minor = MSG_FW_FILE_MINOR_VERSION(fw);
	dev_dbg(dev, "New firmware: %d.%d",
			fw_file_major, fw_file_minor);

	return fw->size;
}

static ssize_t firmware_update_smart_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t size)
{
	int ret;
	struct msg21xx_ts_data *ts_data = dev_get_drvdata(dev);

	ret = prepare_fw_data(dev);
	if (ret < 0) {
		dev_err(dev, "Request firmware failed -(%d)\n", ret);
		return ret;
	}
	ts_data->pdata->updating_fw = true;
	disable_irq(ts_data->client->irq);

	ret = firmware_update_c33(dev, attr, buf, size, EMEM_MAIN, false);
	if (ret == 0)
		dev_err(dev, "firmware_update_c33 ret = %d\n", ret);

	enable_irq(ts_data->client->irq);
	ts_data->pdata->updating_fw = false;

	return ret;
}

static ssize_t firmware_force_update_smart_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t size)
{
	int ret;
	struct msg21xx_ts_data *ts_data = dev_get_drvdata(dev);

	ret = prepare_fw_data(dev);
	if (ret < 0) {
		dev_err(dev, "Request firmware failed -(%d)\n", ret);
		return ret;
	}
	ts_data->pdata->updating_fw = true;
	disable_irq(ts_data->client->irq);

	ret = firmware_update_c33(dev, attr, buf, size, EMEM_MAIN, true);
	if (ret == 0)
		dev_err(dev, "firmware_update_c33 et = %d\n", ret);

	enable_irq(ts_data->client->irq);
	ts_data->pdata->updating_fw = false;

	return ret;
}

static DEVICE_ATTR(update_fw, (S_IRUGO | S_IWUSR),
					firmware_update_show,
					firmware_update_smart_store);

static DEVICE_ATTR(force_update_fw, (S_IRUGO | S_IWUSR),
					firmware_update_show,
					firmware_force_update_smart_store);

static ssize_t firmware_version_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct msg21xx_ts_data *ts_data = dev_get_drvdata(dev);

	msg21xx_read_firmware_id(ts_data);
	return snprintf(buf, sizeof(char) * 8, "%03d%03d\n",
			ts_data->pdata->fw_version_major,
			ts_data->pdata->fw_version_minor);
}

static DEVICE_ATTR(version, S_IRUGO,
					firmware_version_show,
					NULL);


static ssize_t msg21xx_fw_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct msg21xx_ts_data *ts_data = dev_get_drvdata(dev);

	return snprintf(buf, MSTAR_FW_NAME_MAX_LEN - 1,
				"%s\n", ts_data->pdata->fw_name);
}

static ssize_t msg21xx_fw_name_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct msg21xx_ts_data *ts_data = dev_get_drvdata(dev);

	if (size > MSTAR_FW_NAME_MAX_LEN - 1)
		return -EINVAL;

	strlcpy(ts_data->pdata->fw_name, buf, size);
	if (ts_data->pdata->fw_name[size - 1] == '\n')
		ts_data->pdata->fw_name[size - 1] = 0;

	return size;
}

static DEVICE_ATTR(fw_name, (S_IRUGO | S_IWUSR),
			msg21xx_fw_name_show, msg21xx_fw_name_store);

static ssize_t firmware_data_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t size)
{
	int count = size / 1024;
	int i;

	for (i = 0; i < count; i++)
		memcpy(fw_bin_data[i], buf + (i * 1024), 1024);

	if (buf != NULL)
		dev_dbg(dev, "buf[0] = %c\n", buf[0]);

	return size;
}

static DEVICE_ATTR(data, S_IWUSR, NULL, firmware_data_store);

static ssize_t tp_print_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct msg21xx_ts_data *ts_data = dev_get_drvdata(dev);

	tp_print_proc_read(ts_data);

	return snprintf(buf, 3, "%d\n", ts_data->suspended);
}

static ssize_t tp_print_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t size)
{
	return size;
}

static DEVICE_ATTR(tpp, (S_IRUGO | S_IWUSR),
				tp_print_show, tp_print_store);

#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
static void _msg_enable_proximity(void)
{
	unsigned char tx_data[4] = {0};

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

static int msg21xx_pinctrl_init(struct msg21xx_ts_data *ts_data)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	ts_data->ts_pinctrl = devm_pinctrl_get(&(ts_data->client->dev));
	if (IS_ERR_OR_NULL(ts_data->ts_pinctrl)) {
		retval = PTR_ERR(ts_data->ts_pinctrl);
		dev_dbg(&ts_data->client->dev,
			"Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	ts_data->pinctrl_state_active = pinctrl_lookup_state(
			ts_data->ts_pinctrl, PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(ts_data->pinctrl_state_active)) {
		retval = PTR_ERR(ts_data->pinctrl_state_active);
		dev_dbg(&ts_data->client->dev,
			"Can't lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	ts_data->pinctrl_state_suspend = pinctrl_lookup_state(
			ts_data->ts_pinctrl, PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(ts_data->pinctrl_state_suspend)) {
		retval = PTR_ERR(ts_data->pinctrl_state_suspend);
		dev_dbg(&ts_data->client->dev,
			"Can't lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	ts_data->pinctrl_state_release = pinctrl_lookup_state(
			ts_data->ts_pinctrl, PINCTRL_STATE_RELEASE);
	if (IS_ERR_OR_NULL(ts_data->pinctrl_state_release)) {
		retval = PTR_ERR(ts_data->pinctrl_state_release);
		dev_dbg(&ts_data->client->dev,
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
	int checksum = 0, i;

	for (i = 0; i < length; i++)
		checksum += msg[i];

	return (unsigned char)((-checksum) & 0xFF);
}

static int parse_info(struct msg21xx_ts_data *ts_data)
{
	unsigned char data[DEMO_MODE_PACKET_LENGTH] = {0};
	unsigned char checksum = 0;
	unsigned int x = 0, y = 0;
	unsigned int x2 = 0, y2 = 0;
	unsigned int delta_x = 0, delta_y = 0;

	mutex_lock(&msg21xx_mutex);
	read_i2c_seq(ts_data, ts_data->client->addr, &data[0],
				DEMO_MODE_PACKET_LENGTH);
	mutex_unlock(&msg21xx_mutex);
	checksum = calculate_checksum(&data[0], (DEMO_MODE_PACKET_LENGTH-1));
	dev_dbg(&ts_data->client->dev, "check sum: [%x] == [%x]?\n",
			data[DEMO_MODE_PACKET_LENGTH-1], checksum);

	if (data[DEMO_MODE_PACKET_LENGTH-1] != checksum) {
		dev_err(&ts_data->client->dev, "WRONG CHECKSUM\n");
		return -EINVAL;
	}

	if (data[0] != 0x52) {
		dev_err(&ts_data->client->dev, "WRONG HEADER\n");
		return -EINVAL;
	}

	ts_data->info.keycode = 0xFF;
	if ((data[1] == 0xFF) && (data[2] == 0xFF) &&
		(data[3] == 0xFF) && (data[4] == 0xFF) &&
		(data[6] == 0xFF)) {
		if ((data[5] == 0xFF) || (data[5] == 0)) {
			ts_data->info.keycode = 0xFF;
		} else if ((data[5] == 1) || (data[5] == 2) ||
				(data[5] == 4) || (data[5] == 8)) {
			ts_data->info.keycode = data[5] >> 1;

			dev_dbg(&ts_data->client->dev,
				"ts_data->info.keycode index %d\n",
				ts_data->info.keycode);
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
			dev_err(&ts_data->client->dev, "WRONG KEY\n");
			return -EINVAL;
		}
	} else {
		x = (((data[1] & 0xF0) << 4) | data[2]);
		y = (((data[1] & 0x0F) << 8) | data[3]);
		delta_x = (((data[4] & 0xF0) << 4) | data[5]);
		delta_y = (((data[4] & 0x0F) << 8) | data[6]);

		if ((delta_x == 0) && (delta_y == 0)) {
			ts_data->info.point[0].x =
				x * ts_data->pdata->x_max / TPD_WIDTH;
			ts_data->info.point[0].y =
				y * ts_data->pdata->y_max / TPD_HEIGHT;
			ts_data->info.count = 1;
		} else {
			if (delta_x > 2048)
				delta_x -= 4096;

			if (delta_y > 2048)
				delta_y -= 4096;

			x2 = (unsigned int)((signed short)x +
						(signed short)delta_x);
			y2 = (unsigned int)((signed short)y +
						(signed short)delta_y);
			ts_data->info.point[0].x =
				x * ts_data->pdata->x_max / TPD_WIDTH;
			ts_data->info.point[0].y =
				y * ts_data->pdata->y_max / TPD_HEIGHT;
			ts_data->info.point[1].x =
				x2 * ts_data->pdata->x_max / TPD_WIDTH;
			ts_data->info.point[1].y =
				y2 * ts_data->pdata->y_max / TPD_HEIGHT;
			ts_data->info.count = ts_data->pdata->num_max_touches;
		}
	}

	return 0;
}

static void touch_driver_touch_released(struct msg21xx_ts_data *ts_data)
{
	int i;

	for (i = 0; i < ts_data->pdata->num_max_touches; i++) {
		input_mt_slot(ts_data->input_dev, i);
		input_mt_report_slot_state(ts_data->input_dev,
						MT_TOOL_FINGER, 0);
	}

	input_report_key(ts_data->input_dev, BTN_TOUCH, 0);
	input_report_key(ts_data->input_dev, BTN_TOOL_FINGER, 0);
	input_sync(ts_data->input_dev);
}

/* read data through I2C then report data to input
sub-system when interrupt occurred  */
static irqreturn_t msg21xx_ts_interrupt(int irq, void *dev_id)
{
	int i = 0;
	static int last_keycode = 0xFF;
	static int last_count;
	struct msg21xx_ts_data *ts_data = dev_id;

	ts_data->info.count = 0;
	if (0 == parse_info(ts_data)) {
		if (ts_data->info.keycode != 0xFF) {   /* key touch pressed */
			if (ts_data->info.keycode <
					ts_data->pdata->num_buttons) {
				if (ts_data->info.keycode != last_keycode) {
					dev_dbg(&ts_data->client->dev,
						"key touch pressed");

					input_report_key(ts_data->input_dev,
							BTN_TOUCH, 1);
					input_report_key(ts_data->input_dev,
						ts_data->pdata->button_map[
						ts_data->info.keycode], 1);

					last_keycode = ts_data->info.keycode;
				} else {
					/* pass duplicate key-pressing */
					dev_dbg(&ts_data->client->dev,
						"REPEATED KEY\n");
				}
			} else {
				dev_dbg(&ts_data->client->dev, "WRONG KEY\n");
			}
		} else {  /* key touch released */
			if (last_keycode != 0xFF) {
				dev_dbg(&ts_data->client->dev, "key touch released");

				input_report_key(ts_data->input_dev,
						BTN_TOUCH, 0);
				input_report_key(ts_data->input_dev,
				ts_data->pdata->button_map[last_keycode],
				0);

				last_keycode = 0xFF;
			}
		}

		if (ts_data->info.count > 0)	{ /* point touch pressed */
			for (i = 0; i < ts_data->info.count; i++) {
				input_mt_slot(ts_data->input_dev, i);
				input_mt_report_slot_state(ts_data->input_dev,
					MT_TOOL_FINGER, 1);
				input_report_abs(ts_data->input_dev,
					ABS_MT_TOUCH_MAJOR, 1);
				input_report_abs(ts_data->input_dev,
					ABS_MT_POSITION_X,
					ts_data->info.point[i].x);
				input_report_abs(ts_data->input_dev,
					ABS_MT_POSITION_Y,
					ts_data->info.point[i].y);
			}
		}

		if (last_count > ts_data->info.count) {
			for (i = ts_data->info.count;
				i < ts_data->pdata->num_max_touches;
				i++) {
				input_mt_slot(ts_data->input_dev, i);
				input_mt_report_slot_state(ts_data->input_dev,
					MT_TOOL_FINGER, 0);
			}
		}
		last_count = ts_data->info.count;

		input_report_key(ts_data->input_dev, BTN_TOUCH,
						ts_data->info.count > 0);
		input_report_key(ts_data->input_dev, BTN_TOOL_FINGER,
						ts_data->info.count > 0);

		input_sync(ts_data->input_dev);
	}

	return IRQ_HANDLED;
}

static int msg21xx_ts_power_init(struct msg21xx_ts_data *ts_data, bool init)
{
	int rc;

	if (init) {
		ts_data->vdd = regulator_get(&ts_data->client->dev,
									"vdd");
		if (IS_ERR(ts_data->vdd)) {
			rc = PTR_ERR(ts_data->vdd);
			dev_err(&ts_data->client->dev,
				"Regulator get failed vdd rc=%d\n", rc);
			return rc;
		}

		if (regulator_count_voltages(ts_data->vdd) > 0) {
			rc = regulator_set_voltage(ts_data->vdd,
							MSTAR_VTG_MIN_UV,
							MSTAR_VTG_MAX_UV);
			if (rc) {
				dev_err(&ts_data->client->dev,
					"Regulator set_vtg failed vdd rc=%d\n",
					rc);
				goto reg_vdd_put;
			}
		}

		ts_data->vcc_i2c = regulator_get(&ts_data->client->dev,
								"vcc_i2c");
		if (IS_ERR(ts_data->vcc_i2c)) {
			rc = PTR_ERR(ts_data->vcc_i2c);
			dev_err(&ts_data->client->dev,
				"Regulator get failed vcc_i2c rc=%d\n", rc);
			goto reg_vdd_set_vtg;
		}

		if (regulator_count_voltages(ts_data->vcc_i2c) > 0) {
			rc = regulator_set_voltage(ts_data->vcc_i2c,
						MSTAR_I2C_VTG_MIN_UV,
						MSTAR_I2C_VTG_MAX_UV);
			if (rc) {
				dev_err(&ts_data->client->dev,
				"Regulator set_vtg failed vcc_i2c rc=%d\n", rc);
				goto reg_vcc_i2c_put;
			}
		}
	} else {
		if (regulator_count_voltages(ts_data->vdd) > 0)
			regulator_set_voltage(ts_data->vdd, 0,
							MSTAR_VTG_MAX_UV);

		regulator_put(ts_data->vdd);

		if (regulator_count_voltages(ts_data->vcc_i2c) > 0)
			regulator_set_voltage(ts_data->vcc_i2c, 0,
						MSTAR_I2C_VTG_MAX_UV);

		regulator_put(ts_data->vcc_i2c);
	}

	return 0;

reg_vcc_i2c_put:
	regulator_put(ts_data->vcc_i2c);
reg_vdd_set_vtg:
	if (regulator_count_voltages(ts_data->vdd) > 0)
		regulator_set_voltage(ts_data->vdd, 0, MSTAR_VTG_MAX_UV);
reg_vdd_put:
	regulator_put(ts_data->vdd);
	return rc;
}

static int msg21xx_ts_power_on(struct msg21xx_ts_data *ts_data, bool on)
{
	int rc;

	if (!on)
		goto power_off;

	rc = regulator_enable(ts_data->vdd);
	if (rc) {
		dev_err(&ts_data->client->dev,
			"Regulator vdd enable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(ts_data->vcc_i2c);
	if (rc) {
		dev_err(&ts_data->client->dev,
			"Regulator vcc_i2c enable failed rc=%d\n", rc);
		regulator_disable(ts_data->vdd);
	}

	return rc;

power_off:
	rc = regulator_disable(ts_data->vdd);
	if (rc) {
		dev_err(&ts_data->client->dev,
			"Regulator vdd disable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_disable(ts_data->vcc_i2c);
	if (rc) {
		dev_err(&ts_data->client->dev,
			"Regulator vcc_i2c disable failed rc=%d\n", rc);
		rc = regulator_enable(ts_data->vdd);
	}

	return rc;
}

static int msg21xx_ts_gpio_configure(struct msg21xx_ts_data *ts_data, bool on)
{
	int ret = 0;

	if (!on)
		goto pwr_deinit;

	if (gpio_is_valid(ts_data->pdata->irq_gpio)) {
		ret = gpio_request(ts_data->pdata->irq_gpio,
						"msg21xx_irq_gpio");
		if (ret) {
			dev_err(&ts_data->client->dev,
				"Failed to request GPIO[%d], %d\n",
				ts_data->pdata->irq_gpio, ret);
			goto err_irq_gpio_req;
		}
		ret = gpio_direction_input(ts_data->pdata->irq_gpio);
		if (ret) {
			dev_err(&ts_data->client->dev,
				"Failed to set direction for gpio[%d], %d\n",
				ts_data->pdata->irq_gpio, ret);
			goto err_irq_gpio_dir;
		}
		gpio_set_value_cansleep(ts_data->pdata->irq_gpio, 1);
	} else {
		dev_err(&ts_data->client->dev, "irq gpio not provided\n");
		goto err_irq_gpio_req;
	}

	if (gpio_is_valid(ts_data->pdata->reset_gpio)) {
		ret = gpio_request(ts_data->pdata->reset_gpio,
					"msg21xx_reset_gpio");
		if (ret) {
			dev_err(&ts_data->client->dev,
				"Failed to request GPIO[%d], %d\n",
				ts_data->pdata->reset_gpio, ret);
			goto err_reset_gpio_req;
		}

		/* power on TP */
		ret = gpio_direction_output(
					ts_data->pdata->reset_gpio, 1);
		if (ret) {
			dev_err(&ts_data->client->dev,
				"Failed to set direction for GPIO[%d], %d\n",
				ts_data->pdata->reset_gpio, ret);
			goto err_reset_gpio_dir;
		}
		msleep(100);
		gpio_set_value_cansleep(ts_data->pdata->reset_gpio, 0);
		msleep(20);
		gpio_set_value_cansleep(ts_data->pdata->reset_gpio, 1);
		msleep(200);
	} else {
		dev_err(&ts_data->client->dev, "reset gpio not provided\n");
		goto err_reset_gpio_req;
	}

	return 0;

err_reset_gpio_dir:
	if (gpio_is_valid(ts_data->pdata->reset_gpio))
		gpio_free(ts_data->pdata->irq_gpio);
err_reset_gpio_req:
err_irq_gpio_dir:
	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);
err_irq_gpio_req:
	return ret;

pwr_deinit:
	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);
	if (gpio_is_valid(ts_data->pdata->reset_gpio)) {
		gpio_set_value_cansleep(ts_data->pdata->reset_gpio, 0);
		ret = gpio_direction_input(ts_data->pdata->reset_gpio);
		if (ret)
			dev_err(&ts_data->client->dev,
				"Unable to set direction for gpio [%d]\n",
				ts_data->pdata->reset_gpio);
		gpio_free(ts_data->pdata->reset_gpio);
	}
	return 0;
}

#ifdef CONFIG_PM
static int msg21xx_ts_resume(struct device *dev)
{
	int retval;
	struct msg21xx_ts_data *ts_data = dev_get_drvdata(dev);

	if (!ts_data->suspended) {
		dev_info(dev, "msg21xx_ts already in resume\n");
		return 0;
	}

	mutex_lock(&ts_data->ts_mutex);

	retval = msg21xx_ts_power_on(ts_data, true);
	if (retval) {
		dev_err(dev, "msg21xx_ts power on failed");
		mutex_unlock(&ts_data->ts_mutex);
		return retval;
	}

	if (ts_data->ts_pinctrl) {
		retval = pinctrl_select_state(ts_data->ts_pinctrl,
				ts_data->pinctrl_state_active);
		if (retval < 0) {
			dev_err(dev, "Cannot get active pinctrl state\n");
			mutex_unlock(&ts_data->ts_mutex);
			return retval;
		}
	}

	retval = msg21xx_ts_gpio_configure(ts_data, true);
	if (retval) {
		dev_err(dev, "Failed to put gpios in active state %d",
				retval);
		mutex_unlock(&ts_data->ts_mutex);
		return retval;
	}

	enable_irq(ts_data->client->irq);
	ts_data->suspended = false;

	mutex_unlock(&ts_data->ts_mutex);

	return 0;
}

static int msg21xx_ts_suspend(struct device *dev)
{
	int retval;
	struct msg21xx_ts_data *ts_data = dev_get_drvdata(dev);

	if (ts_data->pdata->updating_fw) {
		dev_info(dev, "Firmware loading in progress\n");
		return 0;
	}

	if (ts_data->suspended) {
		dev_info(dev, "msg21xx_ts already in suspend\n");
		return 0;
	}

#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
	if (bEnableTpProximity) {
		dev_dbg(dev, "suspend bEnableTpProximity=%d\n",
				bEnableTpProximity);
		return 0;
	}
#endif

	mutex_lock(&ts_data->ts_mutex);

	disable_irq(ts_data->client->irq);

	touch_driver_touch_released(ts_data);

	if (ts_data->ts_pinctrl) {
		retval = pinctrl_select_state(ts_data->ts_pinctrl,
				ts_data->pinctrl_state_suspend);
		if (retval < 0) {
			dev_err(dev, "Cannot get idle pinctrl state %d\n",
				retval);
			mutex_unlock(&ts_data->ts_mutex);
			return retval;
		}
	}

	retval = msg21xx_ts_gpio_configure(ts_data, false);
	if (retval) {
		dev_err(dev, "Failed to put gpios in idle state %d",
				retval);
		mutex_unlock(&ts_data->ts_mutex);
		return retval;
	}

	retval = msg21xx_ts_power_on(ts_data, false);
	if (retval) {
		dev_err(dev, "msg21xx_ts power off failed");
		mutex_unlock(&ts_data->ts_mutex);
		return retval;
	}

	ts_data->suspended = true;

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

static int msg21xx_debug_suspend_set(void *_data, u64 val)
{
	struct msg21xx_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (val)
		msg21xx_ts_suspend(&data->client->dev);
	else
		msg21xx_ts_resume(&data->client->dev);

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

static int msg21xx_debug_suspend_get(void *_data, u64 *val)
{
	struct msg21xx_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);
	*val = data->suspended;
	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_suspend_fops, msg21xx_debug_suspend_get,
			msg21xx_debug_suspend_set, "%lld\n");


#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct msg21xx_ts_data *ts_data =
		container_of(self, struct msg21xx_ts_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK)
			msg21xx_ts_resume(&ts_data->client->dev);
		else if (*blank == FB_BLANK_POWERDOWN)
			msg21xx_ts_suspend(&ts_data->client->dev);
	}

	return 0;
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

	rc = of_property_read_u32(np, "mstar,hard-reset-delay-ms",
							&temp_val);
	if (!rc)
		pdata->hard_reset_delay_ms = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "mstar,post-hard-reset-delay-ms",
							&temp_val);
	if (!rc)
		pdata->post_hard_reset_delay_ms = temp_val;
	else
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

	rc = of_property_read_u32(np, "mstar,ic-type", &temp_val);
	if (rc && (rc != -EINVAL))
		return rc;

	pdata->ic_type = temp_val;

	rc = of_property_read_u32(np, "mstar,num-max-touches", &temp_val);
	if (!rc)
		pdata->num_max_touches = temp_val;
	else
		return rc;

	prop = of_find_property(np, "mstar,button-map", NULL);
	if (prop) {
		pdata->num_buttons = prop->length / sizeof(temp_val);
		if (pdata->num_buttons > MAX_BUTTONS)
			return -EINVAL;

		rc = of_property_read_u32_array(np,
			"mstar,button-map", pdata->button_map,
			pdata->num_buttons);
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

	int ret = 0, i;
	struct dentry *temp, *dir;
	struct input_dev *input_dev;
	struct msg21xx_ts_data *ts_data;
	struct msg21xx_ts_platform_data *pdata;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct msg21xx_ts_platform_data), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		ret = msg21xx_parse_dt(&client->dev, pdata);
		if (ret) {
			dev_err(&client->dev, "DT parsing failed\n");
			return ret;
		}
	} else
		pdata = client->dev.platform_data;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C not supported\n");
		return -ENODEV;
	}

	ts_data = devm_kzalloc(&client->dev,
			sizeof(struct msg21xx_ts_data), GFP_KERNEL);
	if (!ts_data)
		return -ENOMEM;

	ts_data->client = client;
	ts_data->info.point = devm_kzalloc(&client->dev,
		sizeof(struct touchPoint_t) * pdata->num_max_touches,
		GFP_KERNEL);
	if (!ts_data->info.point) {
		dev_err(&client->dev, "Not enough memory\n");
		return -ENOMEM;
	}

	/* allocate an input device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		ret = -ENOMEM;
		dev_err(&client->dev, "input device allocation failed\n");
		goto err_input_allocate_dev;
	}

	input_dev->name = client->name;
	input_dev->phys = "I2C";
	input_dev->dev.parent = &client->dev;
	input_dev->id.bustype = BUS_I2C;

	ts_data->input_dev = input_dev;
	ts_data->client = client;
	ts_data->pdata = pdata;

	input_set_drvdata(input_dev, ts_data);
	i2c_set_clientdata(client, ts_data);

	ret = msg21xx_ts_power_init(ts_data, true);
	if (ret) {
		dev_err(&client->dev, "Mstar power init failed\n");
		return ret;
	}

	ret = msg21xx_ts_power_on(ts_data, true);
	if (ret) {
		dev_err(&client->dev, "Mstar power on failed\n");
		goto exit_deinit_power;
	}

	ret = msg21xx_pinctrl_init(ts_data);
	if (!ret && ts_data->ts_pinctrl) {
		/*
		* Pinctrl handle is optional. If pinctrl handle is found
		* let pins to be configured in active state. If not
		* found continue further without error.
		*/
		ret = pinctrl_select_state(ts_data->ts_pinctrl,
				ts_data->pinctrl_state_active);
		if (ret < 0)
			dev_err(&client->dev,
				"Failed to select %s pinatate %d\n",
				PINCTRL_STATE_ACTIVE, ret);
	}

	ret = msg21xx_ts_gpio_configure(ts_data, true);
	if (ret) {
		dev_err(&client->dev, "Failed to configure gpio %d\n", ret);
		goto exit_gpio_config;
	}

	if (msg21xx_get_ic_type(ts_data) == 0) {
		dev_err(&client->dev, "The current IC is not Mstar\n");
		ret = -1;
		goto err_wrong_ic_type;
	}

	mutex_init(&msg21xx_mutex);
	mutex_init(&ts_data->ts_mutex);

	/* set the supported event type for input device */
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	for (i = 0; i < pdata->num_buttons; i++)
		input_set_capability(input_dev, EV_KEY, pdata->button_map[i]);

	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 2, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			0, pdata->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			0, pdata->y_max, 0, 0);
	ret = input_mt_init_slots(input_dev,  pdata->num_max_touches, 0);
	if (ret) {
		dev_err(&client->dev,
			"Error %d initialising slots\n", ret);
		goto err_free_mem;
	}

	/* register the input device to input sub-system */
	ret = input_register_device(input_dev);
	if (ret < 0) {
		dev_err(&client->dev,
			"Unable to register ms-touchscreen input device\n");
		goto err_input_reg_dev;
	}

	/* version */
	if (device_create_file(&client->dev, &dev_attr_version) < 0) {
		dev_err(&client->dev,
			"Failed to create device file(%s)!\n",
			dev_attr_version.attr.name);
		goto err_create_fw_ver_file;
	}
	/* update */
	if (device_create_file(&client->dev, &dev_attr_update) < 0) {
		dev_err(&client->dev,
			"Failed to create device file(%s)!\n",
			dev_attr_update.attr.name);
		goto err_create_fw_update_file;
	}
	/* data */
	if (device_create_file(&client->dev, &dev_attr_data) < 0) {
		dev_err(&client->dev,
			"Failed to create device file(%s)!\n",
			dev_attr_data.attr.name);
		goto err_create_fw_data_file;
	}
	/* fw name */
	if (device_create_file(&client->dev, &dev_attr_fw_name) < 0) {
		dev_err(&client->dev,
			"Failed to create device file(%s)!\n",
			dev_attr_fw_name.attr.name);
		goto err_create_fw_name_file;
	}
	/* smart fw update */
	if (device_create_file(&client->dev, &dev_attr_update_fw) < 0) {
		dev_err(&client->dev,
			"Failed to create device file(%s)!\n",
			dev_attr_update_fw.attr.name);
		goto err_create_update_fw_file;
	}
	/* smart fw force update */
	if (device_create_file(&client->dev,
					&dev_attr_force_update_fw) < 0) {
		dev_err(&client->dev,
			"Failed to create device file(%s)!\n",
			dev_attr_force_update_fw.attr.name);
		goto err_create_force_update_fw_file;
	}
	dir = debugfs_create_dir(MSTAR_DEBUG_DIR_NAME, NULL);
	temp = debugfs_create_file("suspend", S_IRUSR | S_IWUSR, dir,
					ts_data, &debug_suspend_fops);
	if (temp == NULL || IS_ERR(temp)) {
		dev_err(&client->dev,
			"debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		goto free_debug_dir;
	}

#ifdef TP_PRINT
	tp_print_create_entry(ts_data);
#endif

	ret = request_threaded_irq(client->irq, NULL,
				msg21xx_ts_interrupt,
				pdata->irq_gpio_flags | IRQF_ONESHOT,
				"msg21xx", ts_data);
	if (ret)
		goto err_req_irq;

	disable_irq(client->irq);

#if defined(CONFIG_FB)
	ts_data->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts_data->fb_notif);
#endif

#ifdef CONFIG_TOUCHSCREEN_PROXIMITY_SENSOR
	tsps_assist_register_callback("msg21xx", &tsps_msg21xx_enable,
				&tsps_msg21xx_data);
#endif

	dev_dbg(&client->dev, "mstar touch screen registered\n");
	enable_irq(client->irq);
	return 0;

err_req_irq:
	free_irq(client->irq, ts_data);
	device_remove_file(&client->dev, &dev_attr_data);
free_debug_dir:
	debugfs_remove_recursive(dir);
err_create_fw_data_file:
	device_remove_file(&client->dev, &dev_attr_update);
err_create_fw_update_file:
	device_remove_file(&client->dev, &dev_attr_version);
err_create_fw_name_file:
	device_remove_file(&client->dev, &dev_attr_fw_name);
err_create_update_fw_file:
	device_remove_file(&client->dev, &dev_attr_update_fw);
err_create_force_update_fw_file:
	device_remove_file(&client->dev, &dev_attr_force_update_fw);
err_create_fw_ver_file:
	input_unregister_device(input_dev);

err_input_reg_dev:
	input_free_device(input_dev);
	input_dev = NULL;
err_input_allocate_dev:
	mutex_destroy(&msg21xx_mutex);
	mutex_destroy(&ts_data->ts_mutex);

err_wrong_ic_type:
	msg21xx_ts_gpio_configure(ts_data, false);
exit_gpio_config:
	if (ts_data->ts_pinctrl) {
		if (IS_ERR_OR_NULL(ts_data->pinctrl_state_release)) {
			devm_pinctrl_put(ts_data->ts_pinctrl);
			ts_data->ts_pinctrl = NULL;
		} else {
			ret = pinctrl_select_state(ts_data->ts_pinctrl,
					ts_data->pinctrl_state_release);
			if (ret < 0)
				dev_err(&ts_data->client->dev,
					"Cannot get release pinctrl state\n");
		}
	}
	msg21xx_ts_power_on(ts_data, false);
exit_deinit_power:
	msg21xx_ts_power_init(ts_data, false);
err_free_mem:
	input_free_device(input_dev);

	return ret;
}

/* remove function is triggered when the input device is removed
from input sub-system */
static int touch_driver_remove(struct i2c_client *client)
{
	int retval = 0;
	struct msg21xx_ts_data *ts_data = i2c_get_clientdata(client);

	free_irq(ts_data->client->irq, ts_data);
	gpio_free(ts_data->pdata->irq_gpio);
	gpio_free(ts_data->pdata->reset_gpio);

	if (ts_data->ts_pinctrl) {
		if (IS_ERR_OR_NULL(ts_data->pinctrl_state_release)) {
			devm_pinctrl_put(ts_data->ts_pinctrl);
			ts_data->ts_pinctrl = NULL;
		} else {
			retval = pinctrl_select_state(ts_data->ts_pinctrl,
					ts_data->pinctrl_state_release);
			if (retval < 0)
				dev_err(&ts_data->client->dev,
					"Cannot get release pinctrl state\n");
		}
	}

	input_unregister_device(ts_data->input_dev);
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

module_i2c_driver(touch_device_driver);

#ifdef TP_PRINT
#include <linux/proc_fs.h>

static unsigned short InfoAddr = 0x0F, PoolAddr = 0x10, TransLen = 256;
static unsigned char row, units, cnt;

static int tp_print_proc_read(struct msg21xx_ts_data *ts_data)
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
				write_i2c_seq(ts_data, ts_data->client->addr,
							&dbbus_tx_data[0], 3);
				read_i2c_seq(ts_data, ts_data->client->addr,
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

static void tp_print_create_entry(struct msg21xx_ts_data *ts_data)
{
	unsigned char dbbus_tx_data[3] = {0};
	unsigned char dbbus_rx_data[8] = {0};

	dbbus_tx_data[0] = 0x53;
	dbbus_tx_data[1] = 0x00;
	dbbus_tx_data[2] = 0x58;
	mutex_lock(&msg21xx_mutex);
	write_i2c_seq(ts_data, ts_data->client->addr, &dbbus_tx_data[0], 3);
	read_i2c_seq(ts_data, ts_data->client->addr, &dbbus_rx_data[0], 4);
	mutex_unlock(&msg21xx_mutex);
	InfoAddr = (dbbus_rx_data[1]<<8) + dbbus_rx_data[0];
	PoolAddr = (dbbus_rx_data[3]<<8) + dbbus_rx_data[2];

	if ((InfoAddr != 0x0F) && (PoolAddr != 0x10)) {
		msleep(20);
		dbbus_tx_data[0] = 0x53;
		dbbus_tx_data[1] = (InfoAddr >> 8) & 0xFF;
		dbbus_tx_data[2] = InfoAddr & 0xFF;
		mutex_lock(&msg21xx_mutex);
		write_i2c_seq(ts_data, ts_data->client->addr,
						&dbbus_tx_data[0], 3);
		read_i2c_seq(ts_data, ts_data->client->addr,
						&dbbus_rx_data[0], 8);
		mutex_unlock(&msg21xx_mutex);

		units = dbbus_rx_data[0];
		row = dbbus_rx_data[1];
		cnt = dbbus_rx_data[2];
		TransLen = (dbbus_rx_data[7]<<8) + dbbus_rx_data[6];

		if (device_create_file(&ts_data->client->dev,
						&dev_attr_tpp) < 0)
			dev_err(&ts_data->client->dev, "Failed to create device file(%s)!\n",
					dev_attr_tpp.attr.name);
	}
}
#endif

MODULE_AUTHOR("MStar Semiconductor, Inc.");
MODULE_LICENSE("GPL v2");
