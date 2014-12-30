/*
 * Copyright (C) 2011 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/input.h>
#include <linux/input/ft5x06_ts.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/power_supply.h>
#include <linux/input/mt.h>
#include "ft5x06_ts.h"

//register address
#define FT5X0X_REG_DEVIDE_MODE	0x00
#define FT5X0X_REG_ROW_ADDR		0x01
#define FT5X0X_REG_TD_STATUS		0x02
#define FT5X0X_REG_START_SCAN		0x02
#define FT5X0X_REG_TOUCH_START	0x03
#define FT5X0X_REG_VOLTAGE		0x05
#define FT5X0X_REG_CHIP_ID		0xA3
#define FT5X0X_ID_G_PMODE			0xA5
#define FT5x0x_REG_FW_VER			0xA6
#define FT5x0x_ID_G_FT5201ID		0xA8
#define FT5X0X_NOISE_FILTER		0x8B
#define FT5x0x_REG_POINT_RATE		0x88
#define FT5X0X_REG_THGROUP		0x80
#define FT5X0X_REG_RESET			0xFC

#define FT5X0X_DEVICE_MODE_NORMAL	0x00
#define FT5X0X_DEVICE_MODE_TEST	0x40
#define FT5X0X_DEVICE_START_CALIB	0x04
#define FT5X0X_DEVICE_SAVE_RESULT	0x05

#define FT5X0X_POWER_ACTIVE             0x00
#define FT5X0X_POWER_MONITOR            0x01
#define FT5X0X_POWER_HIBERNATE          0x03


/* ft5x0x register list */
#define FT5X0X_TOUCH_LENGTH		6

#define FT5X0X_TOUCH_XH			0x00 /* offset from each touch */
#define FT5X0X_TOUCH_XL			0x01
#define FT5X0X_TOUCH_YH			0x02
#define FT5X0X_TOUCH_YL			0x03
#define FT5X0X_TOUCH_PRESSURE		0x04
#define FT5X0X_TOUCH_SIZE		0x05

/* ft5x0x bit field definition */
#define FT5X0X_MODE_NORMAL		0x00
#define FT5X0X_MODE_SYSINFO		0x10
#define FT5X0X_MODE_TEST		0x40
#define FT5X0X_MODE_MASK		0x70

#define FT5X0X_EVENT_DOWN		0x00
#define FT5X0X_EVENT_UP			0x40
#define FT5X0X_EVENT_CONTACT		0x80
#define FT5X0X_EVENT_MASK		0xc0


/* ft5x0x firmware upgrade definition */
#define FT5X0X_FIRMWARE_TAIL		-8 /* base on the end of firmware */
#define FT5X0X_FIRMWARE_VERION		-2
#define FT5X0X_PACKET_HEADER		6
#define FT5X0X_PACKET_LENGTH		128

/* ft5x0x absolute value */
#define FT5X0X_MAX_FINGER		0x0A
#define FT5X0X_MAX_SIZE			0xff
#define FT5X0X_MAX_PRESSURE		0xff

#define NOISE_FILTER_DELAY	HZ

#define FT_VTG_MIN_UV		2600000
#define FT_VTG_MAX_UV		3300000
#define FT_I2C_VTG_MIN_UV	1800000
#define FT_I2C_VTG_MAX_UV	1800000

/*upgrade config of FT5316*/
#define FT5316_UPGRADE_AA_DELAY 		50
#define FT5316_UPGRADE_55_DELAY 		40
#define FT5316_UPGRADE_ID_1		0x79
#define FT5316_UPGRADE_ID_2		0x07
#define FT5316_UPGRADE_READID_DELAY 		1

/*upgrade config of FT5X36*/
#define FT5X36_UPGRADE_AA_DELAY 		30
#define FT5X36_UPGRADE_55_DELAY 		30
#define FT5X36_UPGRADE_ID_1		0x79
#define FT5X36_UPGRADE_ID_2		0x11
#define FT5X36_UPGRADE_READID_DELAY 		10

#define FT5316_CHIP_ID		0x0A
#define FT5X36_CHIP_ID		0x14

#define FT5316_PROJ_ID		"FTS0000x000"
#define FT5X36_PROJ_ID		"FTS0000P000"

#define FT5X0X_UPGRADE_LOOP	3

#define	BL_VERSION_LZ4		0
#define	BL_VERSION_Z7		1
#define	BL_VERSION_GZF	2

struct upgrade_info {
	u16	delay_aa;	/*delay of write FT_UPGRADE_AA*/
	u16	delay_55;	/*delay of write FT_UPGRADE_55*/
	u8	upgrade_id_1;	/*upgrade id 1*/
	u8	upgrade_id_2;	/*upgrade id 2*/
	u16	delay_readid;	/*delay of read id*/
};

struct ft5x06_packet {
	u8  magic1;
	u8  magic2;
	u16 offset;
	u16 length;
	u8  payload[FT5X0X_PACKET_LENGTH];
};

struct ft5x06_finger {
	int x, y;
	int size;
	int pressure;
	bool detect;
};

struct ft5x06_tracker {
	int x, y;
	bool detect;
	bool moving;
	unsigned long jiffies;
};

struct ft5x06_data {
	struct mutex mutex;
	struct device *dev;
	struct input_dev *input;
	struct kobject *vkeys_dir;
	struct kobj_attribute vkeys_attr;
	struct notifier_block power_supply_notifier;
	struct regulator *vdd;
	struct regulator *vcc_i2c;
	const struct ft5x06_bus_ops *bops;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct ft5x06_tracker tracker[FT5X0X_MAX_FINGER];
	int  irq;
	bool dbgdump;
	unsigned int test_result;
	bool in_suspend;
	struct delayed_work noise_filter_delayed_work;
	u8 chip_id;
	u8 is_usb_plug_in;
};

static int ft5x06_recv_byte(struct ft5x06_data *ft5x06, u8 len, ...)
{
	int error;
	va_list varg;
	u8 i, buf[len];

	error = ft5x06->bops->recv(ft5x06->dev, buf, len);
	if (error)
		return error;

	va_start(varg, len);
	for (i = 0; i < len; i++)
		*va_arg(varg, u8 *) = buf[i];
	va_end(varg);

	return 0;
}

static int ft5x06_send_block(struct ft5x06_data *ft5x06,
				const void *buf, int len)
{
	return ft5x06->bops->send(ft5x06->dev, buf, len);
}

static int ft5x06_send_byte(struct ft5x06_data *ft5x06, u8 len, ...)
{
	va_list varg;
	u8 i, buf[len];

	va_start(varg, len);
	for (i = 0; i < len; i++)
		buf[i] = va_arg(varg, int); /* u8 promote to int */
	va_end(varg);

	return ft5x06_send_block(ft5x06, buf, len);
}

static int ft5x06_read_block(struct ft5x06_data *ft5x06,
				u8 addr, void *buf, u8 len)
{
	return ft5x06->bops->read(ft5x06->dev, addr, buf, len);
}

static int ft5x06_read_byte(struct ft5x06_data *ft5x06, u8 addr, u8 *data)
{
	return ft5x06_read_block(ft5x06, addr, data, sizeof(*data));
}

static int ft5x06_write_byte(struct ft5x06_data *ft5x06, u8 addr, u8 data)
{
	return ft5x06->bops->write(ft5x06->dev, addr, &data, sizeof(data));
}


static void ft5x06_charger_state_changed(struct ft5x06_data *ft5x06)
{
	u8 is_usb_exist;

	is_usb_exist = power_supply_is_system_supplied();
	if (is_usb_exist != ft5x06->is_usb_plug_in) {
		ft5x06->is_usb_plug_in = is_usb_exist;
		dev_info(ft5x06->dev, "Power state changed, set noise filter to 0x%x\n", is_usb_exist);
		mutex_lock(&ft5x06->mutex);
		ft5x06_write_byte(ft5x06, FT5X0X_NOISE_FILTER, is_usb_exist);
		mutex_unlock(&ft5x06->mutex);
	}
}

static void ft5x06_noise_filter_delayed_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ft5x06_data *ft5x06 = container_of(delayed_work, struct ft5x06_data, noise_filter_delayed_work);

	dev_info(ft5x06->dev, "ft5x06_noise_filter_delayed_work called\n");
	ft5x06_charger_state_changed(ft5x06);
}

static int ft5x06_power_supply_event(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct ft5x06_data *ft5x06 = container_of(nb, struct ft5x06_data, power_supply_notifier);

	if (ft5x06->dbgdump)
		dev_info(ft5x06->dev, "Power_supply_event\n");
	if (!ft5x06->in_suspend)
		ft5x06_charger_state_changed(ft5x06);
	else if (ft5x06->dbgdump)
		dev_info(ft5x06->dev, "Don't response to power supply event in suspend mode!\n");

	return 0;
}

#ifdef CONFIG_TOUCHSCREEN_FT5X06_CALIBRATE
static int ft5x06_auto_calib(struct ft5x06_data *ft5x06)
{
	int error;
	u8 val1;
	int i;
	msleep(200);
	error = ft5x06_write_byte(ft5x06, /* enter factory mode */
				FT5X0X_REG_DEVIDE_MODE, FT5X0X_DEVICE_MODE_TEST);
	if (error)
		return error;
	msleep(100);

	error = ft5x06_write_byte(ft5x06, /* start calibration */
				FT5X0X_REG_START_SCAN, FT5X0X_DEVICE_START_CALIB);
	if (error)
		return error;
	msleep(300);

	for (i = 0; i < 100; i++) {
		error = ft5x06_read_byte(ft5x06, FT5X0X_REG_DEVIDE_MODE, &val1);
		if (error)
			return error;
		if ((val1&0x70) == 0) /* return to normal mode? */
			break;
		msleep(200); /* not yet, wait and try again later */
	}
	dev_info(ft5x06->dev, "[FTS] calibration OK.\n");

	msleep(300); /* enter factory mode again */
	error = ft5x06_write_byte(ft5x06, FT5X0X_REG_DEVIDE_MODE, FT5X0X_DEVICE_MODE_TEST);
	if (error)
		return error;
	msleep(100);

	error = ft5x06_write_byte(ft5x06, /* save calibration result */
				FT5X0X_REG_START_SCAN, FT5X0X_DEVICE_SAVE_RESULT);
	if (error)
		return error;
	msleep(300);

	error = ft5x06_write_byte(ft5x06, /* return to normal mode */
				FT5X0X_REG_DEVIDE_MODE, FT5X0X_DEVICE_MODE_NORMAL);
	if (error)
		return error;
	msleep(300);
	dev_info(ft5x06->dev, "[FTS] Save calib result OK.\n");

	return 0;
}
#endif

static void fts_get_upgrade_info(struct ft5x06_data *ft5x06, struct upgrade_info *ui)
{
	switch(ft5x06->chip_id) {
		case FT5316_CHIP_ID:
			ui->delay_55 = FT5316_UPGRADE_55_DELAY;
			ui->delay_aa = FT5316_UPGRADE_AA_DELAY;
			ui->upgrade_id_1 = FT5316_UPGRADE_ID_1;
			ui->upgrade_id_2 = FT5316_UPGRADE_ID_2;
			ui->delay_readid = FT5316_UPGRADE_READID_DELAY;
			break;
		case FT5X36_CHIP_ID:
		default:
			ui->delay_55 = FT5X36_UPGRADE_55_DELAY;
			ui->delay_aa = FT5X36_UPGRADE_AA_DELAY;
			ui->upgrade_id_1 = FT5X36_UPGRADE_ID_1;
			ui->upgrade_id_2 = FT5X36_UPGRADE_ID_2;
			ui->delay_readid = FT5X36_UPGRADE_READID_DELAY;
			break;
	}
}

static u8 reset_delay[] = {
	30, 33, 36, 39, 42, 45, 27, 24, 11, 18, 15
};

static u8 ft5x06_get_factory_id(struct ft5x06_data *ft5x06)
{
	int error = 0;
	int i, j;
	u8 reg_val[2], val1, val2;
	u8 ft5336_bootloader_ver;
	struct ft5x06_packet packet;
	u8 vid;

	for (i = 0, error = -1; i < sizeof(reset_delay) && error; i++) {
		/* step 1: reset device */
		error = ft5x06_write_byte(ft5x06, FT5X0X_REG_RESET, 0xaa);
		if (error)
			continue;
		msleep(50);

		error = ft5x06_write_byte(ft5x06, FT5X0X_REG_RESET, 0x55);
		if (error)
			continue;
		msleep(reset_delay[i]);
		dev_info(ft5x06->dev, "[FTS] step1: Reset device.\n");

		/* step 2: enter upgrade mode */
		for (j = 0; j < 10; j++) {
			error = ft5x06_send_byte(ft5x06, 2, 0x55, 0xaa);
			msleep(5);
			if (!error)
				break;
		}
		if (error)
			continue;

		dev_info(ft5x06->dev, "[FTS] step2: Enter upgrade mode.\n");

		/* step 4: check device id */
		error = ft5x06_send_byte(ft5x06, 4, 0x90, 0x00, 0x00, 0x00);
		if (error) {
			dev_err(ft5x06->dev, "Failed to send 90 00 00 00\n");
			continue;
		}

		error = ft5x06_recv_byte(ft5x06, 2, &val1, &val2);
		if (error) {
			dev_err(ft5x06->dev, "Failed to receive device id\n");
			continue;
		}

		pr_info("read id = 0x%x, 0x%x\n", val1, val2);

		dev_info(ft5x06->dev, "[FTS] step3: Check device id.\n");

		if (val1 == FT5316_UPGRADE_ID_1 && val2 == FT5316_UPGRADE_ID_2) {
			ft5x06->chip_id = FT5316_CHIP_ID;
			break;
		} else if (val1 == FT5X36_UPGRADE_ID_1 && val2 == FT5X36_UPGRADE_ID_2) {
			ft5x06->chip_id = FT5X36_CHIP_ID;
			break;
		}
	}

	if (error)
		return error;

	error = ft5x06_send_byte(ft5x06, 1, 0xCD);
	if (error)
		return error;
	error = ft5x06_recv_byte(ft5x06, 1, &reg_val[0]);
	if (error)
		return error;
	dev_info(ft5x06->dev, "bootloader version = 0x%x\n", reg_val[0]);
	if (ft5x06->chip_id == FT5X36_CHIP_ID) {
		if (reg_val[0] <= 4)
			ft5336_bootloader_ver = BL_VERSION_LZ4;
		else if (reg_val[0] == 7)
			ft5336_bootloader_ver = BL_VERSION_Z7;
		else if (reg_val[0] >= 0x0f)
			ft5336_bootloader_ver = BL_VERSION_GZF;
	} else
		ft5336_bootloader_ver = BL_VERSION_LZ4;

	packet.magic1 = 0x03;
	packet.magic2 = 0x00;

	if (ft5336_bootloader_ver == BL_VERSION_Z7 ||
		ft5336_bootloader_ver == BL_VERSION_GZF)
		packet.offset = cpu_to_be16(0x07b4 + j);
	else
		packet.offset = cpu_to_be16(0x7820 + j);
	error = ft5x06_send_block(ft5x06, &packet,
				FT5X0X_PACKET_HEADER);
	if (error)
		return error;
	error = ft5x06_recv_byte(ft5x06, 1, &vid);
	if (error)
		return error;

	error = ft5x06_send_byte(ft5x06, 1, 0x07);
	if (error)
		return error;
	msleep(200);

	return vid;
}

static int ft5x06_load_firmware(struct ft5x06_data *ft5x06,
		struct ft5x06_firmware_data *firmware, bool *upgraded)
{
	struct ft5x06_packet packet;
	int i, j, length, error = 0;
	u8 val1, val2, vid,  ecc = 0, id;
#ifdef CONFIG_TOUCHSCREEN_FT5X06_CALIBRATE
	const int max_calib_time = 3;
	bool calib_ok = false;
#endif
	bool is_5336_fwsize_30 = false;
	u8 ft5336_bootloader_ver;
	struct upgrade_info ui;
	const struct firmware *fw;
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;

	/* step 0a: check and init argument */
	if (upgraded)
		*upgraded = false;

	if (firmware == NULL)
		return 0;

	/* step 0b: find the right firmware for touch screen */
	error = ft5x06_read_byte(ft5x06, FT5x0x_ID_G_FT5201ID, &vid);
	if (error)
		return error;
	dev_info(ft5x06->dev, "firmware vendor is %02x\n", vid);
	id = vid;
	if (vid == FT5x0x_ID_G_FT5201ID ||
		vid == 0 || ft5x06->chip_id == 0) {
		vid = ft5x06_get_factory_id(ft5x06);
		dev_err(ft5x06->dev, "firmware corruption, read real factory id = 0x%x!\n", vid);
	}

	for (i = 0; i < pdata->cfg_size; i++, firmware++) {
		if (vid == firmware->vendor) {
			if (ft5x06->dev->of_node)
				if(ft5x06->chip_id == firmware->chip) {
					dev_info(ft5x06->dev, "chip id = 0x%x, found it!\n",
						ft5x06->chip_id);
					break;
				}
				else
					continue;
			else
				break;
		}
	}

	if (!ft5x06->dev->of_node && firmware->size == 0) {
		dev_err(ft5x06->dev, "unknown touch screen vendor, failed!\n");
		return -ENOENT;
	} else if (ft5x06->dev->of_node && (i == pdata->cfg_size)) {
		dev_err(ft5x06->dev, "Failed to find matched config!\n");
		return -ENOENT;
	}

	if (firmware->size == 0 && ft5x06->dev->of_node) {
		dev_info(ft5x06->dev, "firmware name = %s\n", firmware->fwname);
		error = request_firmware(&fw, firmware->fwname, ft5x06->dev);
		if (!error) {
			firmware->data = kmalloc((int)fw->size, GFP_KERNEL);
			if (!firmware->data) {
				dev_err(ft5x06->dev, "Failed to allocate firmware!\n");
				return -ENOMEM;
			} else {
				memcpy(firmware->data, fw->data, (int)fw->size);
				firmware->size = (int)fw->size;
			}
		}
	}

	fts_get_upgrade_info(ft5x06, &ui);
	if (firmware->data[firmware->size - 12] == 30)
		is_5336_fwsize_30 = true;
	else
		is_5336_fwsize_30 = false;

	/* step 1: check firmware id is different */
	error = ft5x06_read_byte(ft5x06, FT5x0x_REG_FW_VER, &id);
	if (error)
		return error;
	dev_info(ft5x06->dev, "firmware version is %02x\n", id);

	if (id == firmware->data[firmware->size+FT5X0X_FIRMWARE_VERION])
		return 0;
	dev_info(ft5x06->dev, "upgrade firmware to %02x\n",
		firmware->data[firmware->size+FT5X0X_FIRMWARE_VERION]);
	dev_info(ft5x06->dev, "[FTS] step1: check fw id\n");

	for (i = 0, error = -1; i < sizeof(reset_delay) && error; i++) {
		/* step 2: reset device */
		error = ft5x06_write_byte(ft5x06, FT5X0X_REG_RESET, 0xaa);
		if (error)
			continue;
		msleep(ui.delay_aa);

		error = ft5x06_write_byte(ft5x06, FT5X0X_REG_RESET, 0x55);
		if (error)
			continue;
		msleep(reset_delay[i]);
		dev_info(ft5x06->dev, "[FTS] step2: Reset device.\n");

		/* step 3: enter upgrade mode */
		for (j = 0; j < 10; j++) {
			error = ft5x06_send_byte(ft5x06, 2, 0x55, 0xaa);
			msleep(5);
			if (!error)
				break;
		}
		if (error)
			continue;

		dev_info(ft5x06->dev, "[FTS] step3: Enter upgrade mode.\n");

		/* step 4: check device id */
		error = ft5x06_send_byte(ft5x06, 4, 0x90, 0x00, 0x00, 0x00);
		if (error) {
			dev_err(ft5x06->dev, "Failed to send 90 00 00 00\n");
			continue;
		}

		error = ft5x06_recv_byte(ft5x06, 2, &val1, &val2);
		if (error) {
			dev_err(ft5x06->dev, "Failed to receive device id\n");
			continue;
		}

		pr_info("read id = 0x%x, 0x%x, id = 0x%x, 0x%x\n", val1, val2,
			ui.upgrade_id_1, ui.upgrade_id_2);
		if (val1 != ui.upgrade_id_1 || val2 != ui.upgrade_id_2)
			error = -ENODEV;

		dev_info(ft5x06->dev, "[FTS] step4: Check device id.\n");
	}

	if (error) /* check the final result */
		return error;

	error = ft5x06_send_byte(ft5x06, 1, 0xcd);
	if (error)
		return error;
	error = ft5x06_recv_byte(ft5x06, 1, &val1);
	if (error)
		return error;
	dev_info(ft5x06->dev, "[FTS] bootloader version is 0x%x\n", val1);
	if (ft5x06->chip_id == FT5X36_CHIP_ID) {
		if (val1 <= 4)
			ft5336_bootloader_ver = BL_VERSION_LZ4;
		else if (val1 == 7)
			ft5336_bootloader_ver = BL_VERSION_Z7;
		else if (val1 >= 0x0f)
			ft5336_bootloader_ver = BL_VERSION_GZF;
	} else
		ft5336_bootloader_ver = BL_VERSION_LZ4;

	/* step 5: erase device */
	error = ft5x06_send_byte(ft5x06, 1, 0x61);
	if (error)
		return error;
	msleep(1500);
	if (is_5336_fwsize_30) {
		error = ft5x06_send_byte(ft5x06, 1, 0x63);
		if (error)
			return error;
		msleep(50);
	}
	dev_info(ft5x06->dev, "[FTS] step5: Erase device.\n");

	/* step 6: flash firmware to device */
	if (ft5336_bootloader_ver == BL_VERSION_LZ4 ||
		ft5336_bootloader_ver == BL_VERSION_Z7)
		firmware->size -= 8;
	else if (ft5336_bootloader_ver == BL_VERSION_GZF)
		firmware->size -= 14;

	packet.magic1 = 0xbf;
	packet.magic2 = 0x00;
	/* step 6a: send data in 128 bytes chunk each time */
	for (i = 0; i < firmware->size; i += length) {
		length = min(FT5X0X_PACKET_LENGTH,
				firmware->size - i);

		packet.offset = cpu_to_be16(i);
		packet.length = cpu_to_be16(length);

		for (j = 0; j < length; j++) {
			packet.payload[j] = firmware->data[i+j];
			ecc ^= firmware->data[i+j];
		}

		error = ft5x06_send_block(ft5x06, &packet,
					FT5X0X_PACKET_HEADER+length);
		if (error)
			return error;

		msleep(FT5X0X_PACKET_LENGTH/6 + 1);
	}
	dev_info(ft5x06->dev, "[FTS] step6a: Send data in 128 bytes chunk each time.\n");

	/* step 6b: send one byte each time for last six bytes */
	if (ft5336_bootloader_ver == BL_VERSION_LZ4 ||
		ft5336_bootloader_ver == BL_VERSION_Z7) {
		for (j = 0; i < firmware->size + 6; i++, j++) {
			if (ft5336_bootloader_ver== BL_VERSION_Z7 &&
				ft5x06->chip_id == FT5X36_CHIP_ID)
				packet.offset = cpu_to_be16(0x7bfa+j);
			else if (ft5336_bootloader_ver == BL_VERSION_LZ4)
				packet.offset = cpu_to_be16(0x6ffa+j);
			packet.length = cpu_to_be16(1);

			packet.payload[0] = firmware->data[i];
			ecc ^= firmware->data[i];

			error = ft5x06_send_block(ft5x06, &packet,
						FT5X0X_PACKET_HEADER+1);
			if (error)
				return error;

			msleep(20);
		}
	}
	else if (ft5336_bootloader_ver == BL_VERSION_GZF) {
		for (j = 0; i < firmware->size + 12; i++, j++) {
			if(is_5336_fwsize_30 &&
				ft5x06->chip_id == FT5X36_CHIP_ID)
				packet.offset = cpu_to_be16(0x7ff4+j);
			else if (ft5x06->chip_id == FT5X36_CHIP_ID)
				packet.offset = cpu_to_be16(0x7bf4+j);
			packet.length = cpu_to_be16(1);

			packet.payload[0] = firmware->data[i];
			ecc ^= firmware->data[i];

			error = ft5x06_send_block(ft5x06, &packet,
						FT5X0X_PACKET_HEADER+1);
			if (error)
				return error;

			msleep(20);
		}
	}
	dev_info(ft5x06->dev, "[FTS] step6b: Send one byte each time for last six bytes.\n");

	/* step 7: verify checksum */
	error = ft5x06_send_byte(ft5x06, 1, 0xcc);
	if (error)
		return error;

	error = ft5x06_recv_byte(ft5x06, 1, &val1);
	if (error)
		return error;

	if (val1 != ecc)
		return -ERANGE;
	dev_info(ft5x06->dev, "[FTS] step7:Verify checksum.\n");

	/* step 8: reset to new firmware */
	error = ft5x06_send_byte(ft5x06, 1, 0x07);
	if (error)
		return error;
	msleep(300);
	dev_info(ft5x06->dev, "[FTS] step8: Reset to new firmware.\n");

#ifdef CONFIG_TOUCHSCREEN_FT5X06_CALIBRATE
	/* step 9: calibrate the reference value */
	for (i = 0; i < max_calib_time; i++) {
		error = ft5x06_auto_calib(ft5x06);
		if (!error) {
			calib_ok = true;
			dev_info(ft5x06->dev, "[FTS] step9: Calibrate the ref value successfully.\n");
			break;
		}
	}
	if (!calib_ok) {
		dev_info(ft5x06->dev, "[FTS] step9: Calibrate the ref value failed.\n");
		return error;
	}
#endif

	if (upgraded)
		*upgraded = true;

	return 0;
}

static int ft5x06_collect_finger(struct ft5x06_data *ft5x06,
				struct ft5x06_finger *finger, int count)
{
	u8 number, buf[256];
	int i, error;

	error = ft5x06_read_byte(ft5x06, FT5X0X_REG_TD_STATUS, &number);
	if (error)
		return error;
	number &= 0x0f;

	if (number > FT5X0X_MAX_FINGER)
		number = FT5X0X_MAX_FINGER;

	error = ft5x06_read_block(ft5x06, FT5X0X_REG_TOUCH_START,
				buf, FT5X0X_TOUCH_LENGTH*number);
	if (error)
		return error;

	/* clear the finger buffer */
	memset(finger, 0, sizeof(*finger)*count);

	for (i = 0; i < number; i++) {
		u8 xh = buf[FT5X0X_TOUCH_LENGTH*i+FT5X0X_TOUCH_XH];
		u8 xl = buf[FT5X0X_TOUCH_LENGTH*i+FT5X0X_TOUCH_XL];
		u8 yh = buf[FT5X0X_TOUCH_LENGTH*i+FT5X0X_TOUCH_YH];
		u8 yl = buf[FT5X0X_TOUCH_LENGTH*i+FT5X0X_TOUCH_YL];

		u8 size     = buf[FT5X0X_TOUCH_LENGTH*i+FT5X0X_TOUCH_SIZE];
		u8 pressure = buf[FT5X0X_TOUCH_LENGTH*i+FT5X0X_TOUCH_PRESSURE];

		u8 id = (yh&0xf0)>>4;
		if (id >= FT5X0X_MAX_FINGER)
			id = FT5X0X_MAX_FINGER - 1;

		finger[id].x        = ((xh&0x0f)<<8)|xl;
		finger[id].y        = ((yh&0x0f)<<8)|yl;
		finger[id].size     = size;
		finger[id].pressure = pressure;
		finger[id].detect   = (xh&FT5X0X_EVENT_MASK) != FT5X0X_EVENT_UP;

		if (ft5x06->dbgdump)
			dev_info(ft5x06->dev,
				"fig(%02u): %d %04d %04d %03d %03d\n", id,
				finger[i].detect, finger[i].x, finger[i].y,
				finger[i].pressure, finger[i].size);
	}

	return 0;
}

static void ft5x06_apply_filter(struct ft5x06_data *ft5x06,
				struct ft5x06_finger *finger, int count)
{
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;
	int i;

	for (i = 0; i < count; i++) {
		if (!finger[i].detect) /* finger release */
			ft5x06->tracker[i].detect = false;
		else if (!ft5x06->tracker[i].detect) { /* initial touch */
			ft5x06->tracker[i].x = finger[i].x;
			ft5x06->tracker[i].y = finger[i].y;
			ft5x06->tracker[i].detect  = true;
			ft5x06->tracker[i].moving  = false;
			ft5x06->tracker[i].jiffies = jiffies;
		} else { /* the rest report until finger lift */
			unsigned long landed_jiffies;
			int delta_x, delta_y, threshold;

			landed_jiffies  = ft5x06->tracker[i].jiffies;
			landed_jiffies += pdata->landing_jiffies;

			/* no significant movement yet */
			if (!ft5x06->tracker[i].moving) {
				/* use the big threshold for landing period */
				if (time_before(jiffies, landed_jiffies))
					threshold = pdata->landing_threshold;
				else /* use the middle jitter threshold */
					threshold = pdata->staying_threshold;
			} else { /* use the small threshold during movement */
				threshold = pdata->moving_threshold;
			}

			delta_x = finger[i].x - ft5x06->tracker[i].x;
			delta_y = finger[i].y - ft5x06->tracker[i].y;

			delta_x *= delta_x;
			delta_y *= delta_y;

			/* use the saved value for small change */
			if (delta_x + delta_y <= threshold * threshold)
			{
				finger[i].x = ft5x06->tracker[i].x;
				finger[i].y = ft5x06->tracker[i].y;
			} else {/* save new location */
				ft5x06->tracker[i].x = finger[i].x;
				ft5x06->tracker[i].y = finger[i].y;
				ft5x06->tracker[i].moving = true;
			}
		}
	}
}

static void ft5x06_report_touchevent(struct ft5x06_data *ft5x06,
				struct ft5x06_finger *finger, int count)
{
#ifndef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
	bool mt_sync_sent = false;
#endif
	int i;

	for (i = 0; i < count; i++) {
#ifdef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
		input_mt_slot(ft5x06->input, i);
#endif
		if (!finger[i].detect) {
#ifdef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
			input_mt_report_slot_state(ft5x06->input, MT_TOOL_FINGER, 0);
			input_report_abs(ft5x06->input, ABS_MT_TRACKING_ID, -1);
#endif
			continue;
		}

#ifdef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
		input_mt_report_slot_state(ft5x06->input, MT_TOOL_FINGER, 1);
#endif
		input_report_abs(ft5x06->input, ABS_MT_TRACKING_ID, i);
		input_report_abs(ft5x06->input, ABS_MT_POSITION_X ,
			max(1, finger[i].x)); /* for fruit ninja */
		input_report_abs(ft5x06->input, ABS_MT_POSITION_Y ,
			max(1, finger[i].y)); /* for fruit ninja */
		input_report_abs(ft5x06->input, ABS_MT_TOUCH_MAJOR,
			max(1, finger[i].pressure));
		input_report_abs(ft5x06->input, ABS_MT_WIDTH_MAJOR,
			max(1, finger[i].size));
#ifndef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
		input_mt_sync(ft5x06->input);
		mt_sync_sent = true;
#endif
		if (ft5x06->dbgdump)
			dev_info(ft5x06->dev,
				"tch(%02d): %04d %04d %03d %03d\n",
				i, finger[i].x, finger[i].y,
				finger[i].pressure, finger[i].size);
	}
#ifndef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
	if (!mt_sync_sent) {
		input_mt_sync(ft5x06->input);
		if (ft5x06->dbgdump)
			dev_info(ft5x06->dev, "tch(xx): no touch contact\n");
	}
#endif

	input_sync(ft5x06->input);
}

static irqreturn_t ft5x06_interrupt(int irq, void *dev_id)
{
	struct ft5x06_finger finger[FT5X0X_MAX_FINGER];
	struct ft5x06_data *ft5x06 = dev_id;
	int error;

	mutex_lock(&ft5x06->mutex);
	error = ft5x06_collect_finger(ft5x06, finger, FT5X0X_MAX_FINGER);
	if (error >= 0) {
		ft5x06_apply_filter(ft5x06, finger, FT5X0X_MAX_FINGER);
		ft5x06_report_touchevent(ft5x06, finger, FT5X0X_MAX_FINGER);
	} else
		dev_err(ft5x06->dev, "fail to collect finger(%d)\n", error);
	mutex_unlock(&ft5x06->mutex);

	return IRQ_HANDLED;
}

int ft5x06_suspend(struct ft5x06_data *ft5x06)
{
	int error = 0;

	disable_irq(ft5x06->irq);
	mutex_lock(&ft5x06->mutex);
	memset(ft5x06->tracker, 0, sizeof(ft5x06->tracker));

	ft5x06->in_suspend = true;
	cancel_delayed_work_sync(&ft5x06->noise_filter_delayed_work);
	error = ft5x06_write_byte(ft5x06,
			FT5X0X_ID_G_PMODE, FT5X0X_POWER_HIBERNATE);

	mutex_unlock(&ft5x06->mutex);

	return error;
}
EXPORT_SYMBOL_GPL(ft5x06_suspend);

int ft5x06_resume(struct ft5x06_data *ft5x06)
{
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;

	mutex_lock(&ft5x06->mutex);

	/* reset device */
	gpio_set_value_cansleep(pdata->reset_gpio, 0);
	msleep(20);
	gpio_set_value_cansleep(pdata->reset_gpio, 1);
	msleep(50);

	schedule_delayed_work(&ft5x06->noise_filter_delayed_work,
				NOISE_FILTER_DELAY);
	ft5x06->in_suspend = false;
	mutex_unlock(&ft5x06->mutex);
	enable_irq(ft5x06->irq);

	return 0;
}
EXPORT_SYMBOL_GPL(ft5x06_resume);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft5x06_early_suspend(struct early_suspend *h)
{
	struct ft5x06_data *ft5x06 = container_of(h,
					struct ft5x06_data, early_suspend);
	ft5x06_suspend(ft5x06);
}

static void ft5x06_early_resume(struct early_suspend *h)
{
	struct ft5x06_data *ft5x06 = container_of(h,
					struct ft5x06_data, early_suspend);
	ft5x06_resume(ft5x06);
}
#else
static int ft5x06_input_disable(struct input_dev *in_dev)
{
	struct ft5x06_data *ft5x06 = input_get_drvdata(in_dev);

	pr_info("ft5x06 disable!\n");
	ft5x06_suspend(ft5x06);

	return 0;
}

static int ft5x06_input_enable(struct input_dev *in_dev)
{
	struct ft5x06_data *ft5x06 = input_get_drvdata(in_dev);

	pr_info("ft5x06 enable!\n");
	ft5x06_resume(ft5x06);

	return 0;
}
#endif

static ssize_t ft5x06_vkeys_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct ft5x06_data *ft5x06 =
		container_of(attr, struct ft5x06_data, vkeys_attr);
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;
	const struct ft5x06_keypad_data *keypad;
	int i, count = 0;

	for (i = 0; i < pdata->cfg_size; i++) {
		if (ft5x06->chip_id == pdata->keypad[i].chip) {
			keypad = &pdata->keypad[i];
		}
	}
	for (i = 0; keypad && i < keypad->length; i++) {
		int width  = keypad->button[i].width;
		int height = keypad->button[i].height;
		int midx   = keypad->button[i].left+width/2;
		int midy   = keypad->button[i].top+height/2;

		count += snprintf(buf+count, PAGE_SIZE-count,
				"0x%02x:%d:%d:%d:%d:%d:",
				EV_KEY, keypad->keymap[i],
				midx, midy, width, height);
	}

	count -= 1; /* remove the last colon */
	count += snprintf(buf+count, PAGE_SIZE-count, "\n");
	return count;
}

static ssize_t ft5x06_object_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	static struct {
		u8 addr;
		const char *fmt;
	} reg_list[] = {
		/* threshold setting */
		{0x80, "THGROUP          %3d\n"  },
		{0x81, "THPEAK           %3d\n"  },
		{0x82, "THCAL            %3d\n"  },
		{0x83, "THWATER          %3d\n"  },
		{0x84, "THTEMP           %3d\n"  },
		{0x85, "THDIFF           %3d\n"  },
		{0xae, "THBAREA          %3d\n"  },
		/* mode setting */
		{0x86, "CTRL              %02x\n"},
		{0xa0, "AUTOCLB           %02x\n"},
		{0xa4, "MODE              %02x\n"},
		{0xa5, "PMODE             %02x\n"},
		{0xa7, "STATE             %02x\n"},
		{0xa9, "ERR               %02x\n"},
		/* timer setting */
		{0x87, "TIME2MONITOR     %3d\n"  },
		{0x88, "PERIODACTIVE     %3d\n"  },
		{0x89, "PERIODMONITOR    %3d\n"  },
		/* version info */
		{0xa1, "LIBVERH           %02x\n"},
		{0xa2, "LIBVERL           %02x\n"},
		{0xa3, "CIPHER            %02x\n"},
		{0xa6, "FIRMID            %02x\n"},
		{0xa8, "FT5201ID          %02x\n"},
		{/* end of the list */},
	};

	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	int i, error, count = 0;
	u8 val;

	mutex_lock(&ft5x06->mutex);
	for (i = 0; reg_list[i].addr != 0; i++) {
		error = ft5x06_read_byte(ft5x06, reg_list[i].addr, &val);
		if (error)
			break;

		count += snprintf(buf+count, PAGE_SIZE-count,
				reg_list[i].fmt, val);
	}
	mutex_unlock(&ft5x06->mutex);

	return error ? : count;
}

static ssize_t ft5x06_object_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	u8 addr, val;
	int error;

	mutex_lock(&ft5x06->mutex);
	if (sscanf(buf, "%hhx=%hhx", &addr, &val) == 2)
		error = ft5x06_write_byte(ft5x06, addr, val);
	else
		error = -EINVAL;
	mutex_unlock(&ft5x06->mutex);

	return error ? : count;
}

static ssize_t ft5x06_dbgdump_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	int count;

	mutex_lock(&ft5x06->mutex);
	count = sprintf(buf, "%d\n", ft5x06->dbgdump);
	mutex_unlock(&ft5x06->mutex);

	return count;
}

static ssize_t ft5x06_dbgdump_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	unsigned long dbgdump;
	int error;

	mutex_lock(&ft5x06->mutex);
	error = strict_strtoul(buf, 0, &dbgdump);
	if (!error)
		ft5x06->dbgdump = dbgdump;
	mutex_unlock(&ft5x06->mutex);

	return error ? : count;
}

static ssize_t ft5x06_updatefw_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	struct ft5x06_firmware_data firmware;
	const struct firmware *fw;
	bool upgraded;
	int error;

	error = request_firmware(&fw, "ft5x06.bin", dev);
	if (!error) {
		firmware.data = kmalloc((int)fw->size, GFP_KERNEL);
		memcpy(firmware.data, fw->data, (int)fw->size);
		firmware.size = fw->size;

		mutex_lock(&ft5x06->mutex);
		error = ft5x06_load_firmware(ft5x06, &firmware, &upgraded);
		mutex_unlock(&ft5x06->mutex);

		kfree(firmware.data);
		release_firmware(fw);
	}

	return error ? : count;
}

static ssize_t ft5x06_tpfwver_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	ssize_t num_read_chars = 0;
	u8 fwver = 0;
	int error;
	mutex_lock(&ft5x06->mutex);
	error = ft5x06_read_byte(ft5x06, FT5x0x_REG_FW_VER, &fwver);
	if (error)
		num_read_chars = snprintf(buf, PAGE_SIZE, "Get firmware version failed!\n");
	else
		num_read_chars = snprintf(buf, PAGE_SIZE, "%02X\n", fwver);
	mutex_unlock(&ft5x06->mutex);
	return num_read_chars;
}

static int ft5x06_enter_factory(struct ft5x06_data *ft5x06_ts)
{
	u8 reg_val;
	int error;

	error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE,
							FT5X0X_DEVICE_MODE_TEST);
	if (error)
		return -1;
	msleep(100);
	error = ft5x06_read_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE, &reg_val);
	if (error)
		return -1;
	if ((reg_val & 0x70) != FT5X0X_DEVICE_MODE_TEST) {
		dev_info(ft5x06_ts->dev, "ERROR: The Touch Panel was not put in Factory Mode.");
		return -1;
	}

	return 0;
}

static int ft5x06_enter_work(struct ft5x06_data *ft5x06_ts)
{
	u8 reg_val;
	int error;
	error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE,
							FT5X0X_DEVICE_MODE_NORMAL);
	if (error)
		return -1;
	msleep(100);
	error = ft5x06_read_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE, &reg_val);
	if (error)
		return -1;
	if ((reg_val & 0x70) != FT5X0X_DEVICE_MODE_NORMAL) {
		dev_info(ft5x06_ts->dev, "ERROR: The Touch Panel was not put in Normal Mode.\n");
		return -1;
	}

	return 0;
}

#define FT5x0x_TX_NUM		28
#define FT5x0x_RX_NUM   	16
static int ft5x06_get_rawData(struct ft5x06_data *ft5x06_ts,
					 u16 rawdata[][FT5x0x_RX_NUM])
{
	int ret_val = 0;
	int error;
	u8 val;
	int row_num = 0;
	u8 read_buffer[FT5x0x_RX_NUM * 2];
	int read_len;
	int i;

	error = ft5x06_read_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE, &val);
	if (error < 0) {
		dev_err(ft5x06_ts->dev, "ERROR: Read mode failed!\n");
		ret_val = -1;
		goto error_return;
	}
	val |= 0x80;
	error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE, val);
	if (error < 0) {
		dev_err(ft5x06_ts->dev, "ERROR: Write mode failed!\n");
		ret_val = -1;
		goto error_return;
	}
	msleep(20);
	error = ft5x06_read_byte(ft5x06_ts, FT5X0X_REG_DEVIDE_MODE, &val);
	if (error < 0) {
		dev_err(ft5x06_ts->dev, "ERROR: Read mode failed!\n");
		ret_val = -1;
		goto error_return;
	}
	if (0x00 != (val & 0x80)) {
		dev_err(ft5x06_ts->dev, "ERROR: Read mode failed!\n");
		ret_val = -1;
		goto error_return;
	}
	dev_info(ft5x06_ts->dev, "Read rawdata......\n");
	for (row_num = 0; row_num < FT5x0x_TX_NUM; row_num++) {
		memset(read_buffer, 0x00, (FT5x0x_RX_NUM * 2));
		error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_ROW_ADDR, row_num);
		if (error < 0) {
			dev_err(ft5x06_ts->dev, "ERROR: Write row addr failed!\n");
			ret_val = -1;
			goto error_return;
		}
		msleep(1);
		read_len = FT5x0x_RX_NUM * 2;
		error = ft5x06_write_byte(ft5x06_ts, 0x10, read_len);
		if (error < 0) {
			dev_err(ft5x06_ts->dev, "ERROR: Write len failed!\n");
			ret_val = -1;
			goto error_return;
		}
		error = ft5x06_read_block(ft5x06_ts, 0x10,
							read_buffer, FT5x0x_RX_NUM * 2);
		if (error < 0) {
			dev_err(ft5x06_ts->dev,
				"ERROR: Coule not read row %u data!\n", row_num);
			ret_val = -1;
			goto error_return;
		}
		for (i = 0; i < FT5x0x_RX_NUM; i++) {
			rawdata[row_num][i] = read_buffer[i<<1];
			rawdata[row_num][i] = rawdata[row_num][i] << 8;
			rawdata[row_num][i] |= read_buffer[(i<<1)+1];
		}
	}
error_return:
	return ret_val;
}

static int ft5x06_get_diffData(struct ft5x06_data *ft5x06_ts,
					 u16 diffdata[][FT5x0x_RX_NUM],
					 u16 *average)
{
	u16 after_rawdata[FT5x0x_TX_NUM][FT5x0x_RX_NUM];
	int error;
	int ret_val = 0;
	u8 reg_value;
	u8 orig_vol = 0;
	int i, j;
	unsigned int total = 0;
	struct ft5x06_ts_platform_data *pdata = ft5x06_ts->dev->platform_data;
	int tx_num = pdata->tx_num - 1;
	int rx_num = pdata->rx_num;

	/*get original voltage and change it to get new frame rawdata*/
	error = ft5x06_read_byte(ft5x06_ts, FT5X0X_REG_VOLTAGE, &reg_value);
	if (error < 0) {
		dev_err(ft5x06_ts->dev, "ERROR: Could not get voltage data!\n");
		goto error_return;
	} else
		orig_vol = reg_value;

	error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_VOLTAGE, 0);
	if (error < 0) {
		dev_err(ft5x06_ts->dev, "ERROR: Could not set voltage data to 0!\n");
		goto error_return;
	}

	for (i = 0; i < 3; i++) {
		error = ft5x06_get_rawData(ft5x06_ts, diffdata);
		if (error < 0) {
			dev_err(ft5x06_ts->dev, "ERROR: Could not get original raw data!\n");
			ret_val = error;
			goto error_return;
		}
	}

	reg_value = 2;

	dev_info(ft5x06_ts->dev, "original voltage: 0 changed voltage:%u\n",
		reg_value);

	error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_VOLTAGE, reg_value);
	if (error < 0) {
		dev_err(ft5x06_ts->dev, "ERROR: Could not set voltage data!\n");
		ret_val = error;
		goto error_return;
	}

	/* get raw data */
	for (i = 0; i < 3; i++) {
		error = ft5x06_get_rawData(ft5x06_ts, after_rawdata);
		if (error < 0) {
			dev_err(ft5x06_ts->dev, "ERROR: Could not get after raw data!\n");
			ret_val = error;
			goto error_voltage;
		}
	}

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if (after_rawdata[i][j] > diffdata[i][j])
				diffdata[i][j] = after_rawdata[i][j] - diffdata[i][j];
			else
				diffdata[i][j] = diffdata[i][j] - after_rawdata[i][j];

				total += diffdata[i][j];

			printk(KERN_CONT "%d ", diffdata[i][j]);
		}
		pr_info("total = %d\n", total);
	}

	*average = (u16)(total / (tx_num * rx_num));

error_voltage:
	error = ft5x06_write_byte(ft5x06_ts, FT5X0X_REG_VOLTAGE, orig_vol);
	if (error < 0) {
		ret_val = error;
		dev_err(ft5x06_ts->dev, "ERROR: Could not get voltage data!\n");
	}

error_return:
	return ret_val;

}

static ssize_t ft5x06_rawdata_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	u16	rawdata[FT5x0x_TX_NUM][FT5x0x_RX_NUM];
	int error;
	int i = 0, j = 0;
	int num_read_chars = 0;
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;
	int tx_num = pdata->tx_num - 1;
	int rx_num = pdata->rx_num;

	mutex_lock(&ft5x06->mutex);

	disable_irq_nosync(ft5x06->irq);
	error = ft5x06_enter_factory(ft5x06);
	if (error < 0) {
		dev_err(ft5x06->dev, "ERROR: Could not enter factory mode!\n");
		goto end;
	}

	error = ft5x06_get_rawData(ft5x06, rawdata);
	if (error < 0)
		sprintf(buf, "%s", "Could not get rawdata\n");
	else {
		for (i = 0; i < tx_num; i++) {
			for (j = 0; j < rx_num; j++) {
				num_read_chars += sprintf(&buf[num_read_chars],
								"%u ", rawdata[i][j]);
			}
			buf[num_read_chars-1] = '\n';
		}
	}

	error = ft5x06_enter_work(ft5x06);
	if (error < 0)
		dev_err(ft5x06->dev, "ERROR: Could not enter work mode!\n");

end:
	enable_irq(ft5x06->irq);
	mutex_unlock(&ft5x06->mutex);
	return num_read_chars;
}

static ssize_t ft5x06_diffdata_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	u16	diffdata[FT5x0x_TX_NUM][FT5x0x_RX_NUM];
	int error;
	int i = 0, j = 0;
	int num_read_chars = 0;
	u16 average;
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;
	int tx_num = pdata->tx_num - 1;
	int rx_num = pdata->rx_num;

	mutex_lock(&ft5x06->mutex);
	disable_irq_nosync(ft5x06->irq);
	error = ft5x06_enter_factory(ft5x06);
	if (error < 0) {
		dev_err(ft5x06->dev, "ERROR: Could not enter factory mode!\n");
		goto end;
	}

	error = ft5x06_get_diffData(ft5x06, diffdata, &average);
	if (error < 0)
		sprintf(buf, "%s", "Could not get rawdata\n");
	else {
		for (i = 0; i < tx_num; i++) {
			for (j = 0; j < rx_num; j++) {
				num_read_chars += sprintf(&buf[num_read_chars],
								"%u ", diffdata[i][j]);
			}
			buf[num_read_chars-1] = '\n';
		}
	}

	error = ft5x06_enter_work(ft5x06);
	if (error < 0)
		dev_err(ft5x06->dev, "ERROR: Could not enter work mode!\n");

end:
	enable_irq(ft5x06->irq);
	mutex_unlock(&ft5x06->mutex);
	return num_read_chars;
}

unsigned int ft5x06_do_selftest(struct ft5x06_data *ft5x06)
{
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;
	u16 testdata[FT5x0x_TX_NUM][FT5x0x_RX_NUM];
	int i, j;
	int error;
	const struct ft5x06_keypad_data *keypad;
	u16 average;

	for (i = 0; i < pdata->cfg_size; i++) {
		if (ft5x06->chip_id == pdata->keypad[i].chip) {
			keypad = &pdata->keypad[i];
		}
	}

	/* 1. test raw data */
	error = ft5x06_get_rawData(ft5x06, testdata);
	if (error)
		return 0;

	for (i = 0; i < pdata->tx_num; i++) {
		if (i != pdata->tx_num - 1)  {
			for (j = 0; j < pdata->rx_num; j++) {
				if (testdata[i][j] < pdata->raw_min ||
					testdata[i][j] > pdata->raw_max) {
						return 0;
					}
			}
		} else {
			for (j = 0; j < keypad->length; j++) {
				if (testdata[i][keypad->key_pos[j]] < pdata->raw_min ||
					testdata[i][keypad->key_pos[j]]  > pdata->raw_max) {
					return 0;
				}
			}
		}
	}

	/* 2. test diff data */
	error = ft5x06_get_diffData(ft5x06, testdata, &average);
	if (error)
		return 0;
	for (i = 0; i < pdata->tx_num - 1; i++) {
		for (j = 0; j < pdata->rx_num; j++) {
			if ((testdata[i][j] < average * 13 / 20) ||
				(testdata[i][j] > average * 27 / 20)) {
				dev_info(ft5x06->dev, "Failed, testdata = %d, average = %d\n",
					testdata[i][j], average);
				return 0;
			}
		}
	}

	return 1;
}

static ssize_t ft5x06_selftest_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);
	int error;
	unsigned long val;

	error = strict_strtoul(buf, 0, &val);
	if (error )
		return error;
	if (val != 1)
		return -EINVAL;

	mutex_lock(&ft5x06->mutex);

	disable_irq_nosync(ft5x06->irq);
	error = ft5x06_enter_factory(ft5x06);
	if (error < 0) {
		dev_err(ft5x06->dev, "ERROR: Could not enter factory mode!\n");
		goto end;
	}

	ft5x06->test_result = ft5x06_do_selftest(ft5x06);

	error = ft5x06_enter_work(ft5x06);
	if (error < 0)
		dev_err(ft5x06->dev, "ERROR: Could not enter work mode!\n");

end:
	enable_irq(ft5x06->irq);
	mutex_unlock(&ft5x06->mutex);
	return count;
}

static ssize_t ft5x06_selftest_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_data *ft5x06 = dev_get_drvdata(dev);

	return sprintf(&buf[0], "%u\n", ft5x06->test_result);
}

/* sysfs */
static DEVICE_ATTR(tpfwver, 0644, ft5x06_tpfwver_show, NULL);
static DEVICE_ATTR(object, 0644, ft5x06_object_show, ft5x06_object_store);
static DEVICE_ATTR(dbgdump, 0644, ft5x06_dbgdump_show, ft5x06_dbgdump_store);
static DEVICE_ATTR(updatefw, 0200, NULL, ft5x06_updatefw_store);
static DEVICE_ATTR(rawdatashow, 0644, ft5x06_rawdata_show, NULL);
static DEVICE_ATTR(diffdatashow, 0644, ft5x06_diffdata_show, NULL);
static DEVICE_ATTR(selftest, 0644, ft5x06_selftest_show, ft5x06_selftest_store);

static struct attribute *ft5x06_attrs[] = {
	&dev_attr_tpfwver.attr,
	&dev_attr_object.attr,
	&dev_attr_dbgdump.attr,
	&dev_attr_updatefw.attr,
	&dev_attr_rawdatashow.attr,
	&dev_attr_diffdatashow.attr,
	&dev_attr_selftest.attr,
	NULL
};

static const struct attribute_group ft5x06_attr_group = {
	.attrs = ft5x06_attrs
};

static int ft5x06_power_on(struct ft5x06_data *data, bool on)
{
	int rc;

	if (!on)
		goto power_off;

	rc = regulator_enable(data->vdd);
	if (rc) {
		dev_err(data->dev,
			"Regulator vdd enable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(data->vcc_i2c);
	if (rc) {
		dev_err(data->dev,
			"Regulator vcc_i2c enable failed rc=%d\n", rc);
		regulator_disable(data->vdd);
	}

	return rc;

power_off:
	rc = regulator_disable(data->vdd);
	if (rc) {
		dev_err(data->dev,
			"Regulator vdd disable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_disable(data->vcc_i2c);
	if (rc) {
		dev_err(data->dev,
			"Regulator vcc_i2c disable failed rc=%d\n", rc);
		regulator_enable(data->vdd);
	}

	return rc;
}

static int ft5x06_power_init(struct ft5x06_data *data, bool on)
{
	int rc;

	if (!on)
		goto pwr_deinit;

	data->vdd = regulator_get(data->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		rc = PTR_ERR(data->vdd);
		dev_err(data->dev,
			"Regulator get failed vdd rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(data->vdd) > 0) {
		rc = regulator_set_voltage(data->vdd, FT_VTG_MIN_UV,
					   FT_VTG_MAX_UV);
		if (rc) {
			dev_err(data->dev,
				"Regulator set_vtg failed vdd rc=%d\n", rc);
			goto reg_vdd_put;
		}
	}

	data->vcc_i2c = regulator_get(data->dev, "vcc_i2c");
	if (IS_ERR(data->vcc_i2c)) {
		rc = PTR_ERR(data->vcc_i2c);
		dev_err(data->dev,
			"Regulator get failed vcc_i2c rc=%d\n", rc);
		goto reg_vdd_set_vtg;
	}

	if (regulator_count_voltages(data->vcc_i2c) > 0) {
		rc = regulator_set_voltage(data->vcc_i2c, FT_I2C_VTG_MIN_UV,
					   FT_I2C_VTG_MAX_UV);
		if (rc) {
			dev_err(data->dev,
			"Regulator set_vtg failed vcc_i2c rc=%d\n", rc);
			goto reg_vcc_i2c_put;
		}
	}

	return 0;

reg_vcc_i2c_put:
	regulator_put(data->vcc_i2c);
reg_vdd_set_vtg:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, FT_VTG_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;

pwr_deinit:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, FT_VTG_MAX_UV);

	regulator_put(data->vdd);

	if (regulator_count_voltages(data->vcc_i2c) > 0)
		regulator_set_voltage(data->vcc_i2c, 0, FT_I2C_VTG_MAX_UV);

	regulator_put(data->vcc_i2c);
	return 0;
}

static void ft5x06_dt_dump(struct device *dev,
			struct ft5x06_ts_platform_data *pdata)
{
	int key_num;
	int i, j;

	pr_info("i2c-pull-up = %d\n", (int)pdata->i2c_pull_up);
	pr_info("reset gpio = %d\n", (int)pdata->reset_gpio);
	pr_info("irq gpio = %d\n", (int)pdata->irq_gpio);
	pr_info("x_max = %d\n", (int)pdata->x_max);
	pr_info("y_max = %d\n", (int)pdata->y_max);
	pr_info("z_max = %d\n", (int)pdata->z_max);
	pr_info("w_max = %d\n", (int)pdata->w_max);
	pr_info("landing_jiffies = %d\n", (int)pdata->landing_jiffies);
	pr_info("landing_threshold = %d\n", (int)pdata->landing_threshold);
	pr_info("staying_threshold = %d\n", (int)pdata->staying_threshold);
	pr_info("tx num = %d\n", pdata->tx_num);
	pr_info("rx num = %d\n", pdata->rx_num);
	pr_info("raw min = %d\n", (int)pdata->raw_min);
	pr_info("raw max = %d\n", (int)pdata->raw_max);
	for (j = 0; j < pdata->cfg_size; j++) {
		key_num = pdata->keypad[j].length;
		pr_info("key_num = %d\n", key_num);
		for (i = 0; i < key_num; i++) {
			pr_info("keymap[%d] = %d\n", i, pdata->keypad[j].keymap[i]);
			pr_info("keypos[%d] = %d\n", i, pdata->keypad[j].key_pos[i]);
			pr_info("key[%d]: %d %d %d %d\n", i,
					pdata->keypad[j].button[i].left,
					pdata->keypad[j].button[i].top,
					pdata->keypad[j].button[i].width,
					pdata->keypad[j].button[i].height);
		}
	}

	pr_info("firmare 0 chip = 0x%x, vendor = 0x%x name = %s\n",
			pdata->firmware[0].chip,
			pdata->firmware[0].vendor,
			pdata->firmware[0].fwname);
	pr_info("firmare 1 chip = 0x%x, vendor = 0x%x name = %s\n",
			pdata->firmware[1].chip,
			pdata->firmware[1].vendor,
			pdata->firmware[1].fwname);
}

static int ft5x06_parse_dt(struct device *dev,
			struct ft5x06_ts_platform_data *pdata)
{
	int rc, i, j;
	struct device_node *np = dev->of_node;
	u32 temp_val, num_buttons, num_fw;
	u32 rect[12];
	u32 keymap[3], keypos[3];
	struct device_node *sub_np;

	pdata->i2c_pull_up = of_property_read_bool(np,
						"ft5x06_i2c,i2c-pull-up");
	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "ft5x06_i2c,reset-gpio",
				0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
		return pdata->reset_gpio;

	pdata->irq_gpio = of_get_named_gpio_flags(np, "ft5x06_i2c,irq-gpio",
				0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
		return pdata->irq_gpio;

	rc = of_property_read_u32(np, "ft5x06_i2c,x-max", &pdata->x_max);
	if (rc) {
		dev_err(dev, "can't read x-max\n");
		return rc;
	}
	rc = of_property_read_u32(np, "ft5x06_i2c,y-max", &pdata->y_max);
	if (rc) {
		dev_err(dev, "can't read y-max\n");
		return rc;
	}
	rc = of_property_read_u32(np, "ft5x06_i2c,z-max", &pdata->z_max);
	if (rc) {
		dev_err(dev, "can't read z-max\n");
		return rc;
	}
	rc = of_property_read_u32(np, "ft5x06_i2c,w-max", &pdata->w_max);
	if (rc) {
		dev_err(dev, "can't read w-max\n");
		return rc;
	}

	rc = of_property_read_u32(np, "ft5x06_i2c,landing-jiffies", &temp_val);
	if (rc) {
		dev_err(dev, "can't read landing-jiffies\n");
		return rc;
	} else
		pdata->landing_jiffies = (unsigned long)temp_val;
	rc = of_property_read_u32(np, "ft5x06_i2c,landing-threshold", &pdata->landing_threshold);
	if (rc) {
		dev_err(dev, "can't read landing-threshold\n");
		return rc;
	}
	rc = of_property_read_u32(np, "ft5x06_i2c,staying-threshold", &pdata->staying_threshold);
	if (rc) {
		dev_err(dev, "can't read staying-threshold\n");
		return rc;
	}

	rc = of_property_read_u32(np, "ft5x06_i2c,tx-num", &pdata->tx_num);
	if (rc) {
		dev_err(dev, "can't read tx-num\n");
		return rc;
	}
	rc = of_property_read_u32(np, "ft5x06_i2c,rx-num", &pdata->rx_num);
	if (rc) {
		dev_err(dev, "can't read rx-num\n");
		return rc;
	}

	rc = of_property_read_u32(np, "ft5x06_i2c,raw-min", &temp_val);
	if (rc) {
		dev_err(dev, "can't read raw-min\n");
		return rc;
	} else
		pdata->raw_min = (u16)temp_val;
	rc = of_property_read_u32(np, "ft5x06_i2c,raw-max", &temp_val);
	if (rc) {
		dev_err(dev, "can't read raw-max\n");
		return rc;
	} else
		pdata->raw_max= (u16)temp_val;

	rc = of_property_read_u32(np, "ft5x06_i2c,firmware-array-size", &num_fw);
	if (rc) {
		dev_err(dev, "can't get firmware-array-size\n");
		return rc;
	}

	pr_info("num_fw = %d\n", num_fw);

	pdata->firmware = kmalloc(sizeof(struct ft5x06_firmware_data) * (num_fw + 1),
				GFP_KERNEL);
	if (pdata->firmware == NULL)
		return -ENOMEM;
	pdata->keypad = kmalloc(sizeof(struct ft5x06_keypad_data) * num_fw, GFP_KERNEL);
	if (pdata->keypad == NULL)
		return -ENOMEM;

	pdata->cfg_size = num_fw;
	j = 0;
	for_each_child_of_node(np, sub_np) {
		rc = of_property_read_u32(sub_np, "ft5x06_i2c,key-length", &temp_val);
		if (rc) {
			dev_err(dev, "can't read key-length\n");
			return rc;
		} else
			pdata->keypad[j].length = temp_val;
		num_buttons = pdata->keypad[j].length;

		pdata->keypad[j].keymap = devm_kzalloc(dev,
						sizeof(unsigned int) * num_buttons, GFP_KERNEL);
		if (pdata->keypad[j].keymap == NULL)
			return -ENOMEM;
		pdata->keypad[j].key_pos = devm_kzalloc(dev,
						sizeof(unsigned int) * num_buttons, GFP_KERNEL);
		if (pdata->keypad[j].key_pos == NULL)
			return -ENOMEM;
		pdata->keypad[j].button = devm_kzalloc(dev,
						sizeof(struct ft5x06_rect) * num_buttons, GFP_KERNEL);
		if (pdata->keypad[j].button == NULL)
			return -ENOMEM;

		rc = of_property_read_u32_array(sub_np, "ft5x06_i2c,key-map",
						keymap, num_buttons);
		if (rc) {
			dev_err(dev, "can't get key-map\n");
			return rc;
		}
		rc = of_property_read_u32_array(sub_np, "ft5x06_i2c,key-pos",
						keypos, num_buttons);
		if (rc) {
			dev_err(dev, "can't get key-pos\n");
			return rc;
		}
		rc = of_property_read_u32_array(sub_np, "ft5x06_i2c,key-menu", rect, 4);
		if (rc) {
			dev_err(dev, "can't get key-menu\n");
			return rc;
		}
		rc = of_property_read_u32_array(sub_np, "ft5x06_i2c,key-home", rect + 4, 4);
		if (rc) {
			dev_err(dev, "can't get key-home\n");
			return rc;
		}
		rc = of_property_read_u32_array(sub_np, "ft5x06_i2c,key-back", rect + 8, 4);
		if (rc) {
			dev_err(dev, "can't get key-back\n");
			return rc;
		}

		for (i = 0; i < num_buttons; i++) {
			pdata->keypad[j].keymap[i] = keymap[i];
			pdata->keypad[j].key_pos[i] = keypos[i];
			pdata->keypad[j].button[i].left = rect[i*4];
			pdata->keypad[j].button[i].top = rect[i*4+1];
			pdata->keypad[j].button[i].width = rect[i*4+2];
			pdata->keypad[j].button[i].height = rect[i*4+3];
		}

		rc = of_property_read_u32(sub_np, "ft5x06_i2c,chip", &temp_val);
		if (rc) {
			dev_err(dev, "can't get chip id\n");
			return rc;
		} else
			pdata->firmware[j].chip = (u8)temp_val;
		pdata->keypad[j].chip = (u8)temp_val;
		rc = of_property_read_u32(sub_np, "ft5x06_i2c,vendor", &temp_val);
		if (rc) {
			dev_err(dev, "can't get vendor id\n");
			return rc;
		} else
			pdata->firmware[j].vendor = (u8)temp_val;
		rc = of_property_read_string(sub_np, "ft5x06_i2c,fw-name",
						&pdata->firmware[j].fwname);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev, "can't read fw-name\n");
			return rc;
		}
		pdata->firmware[j].size = 0;
		j ++;
	}

	ft5x06_dt_dump(dev, pdata);

	return 0;
}

struct ft5x06_data *ft5x06_probe(struct device *dev,
				const struct ft5x06_bus_ops *bops)
{
	int error;
	struct ft5x06_data *ft5x06;
	struct ft5x06_ts_platform_data *pdata;

	/* check input argument */
	if (dev->of_node) {
		pdata = devm_kzalloc(dev,
				sizeof(struct ft5x06_ts_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(dev, "Failed to allocate memory!\n");
			return ERR_PTR(-ENOMEM);
		}

		error = ft5x06_parse_dt(dev, pdata);
		if (error)
			goto err;
	} else
		pdata = dev->platform_data;
	if (pdata == NULL) {
		dev_err(dev, "platform data doesn't exist\n");
		error = -EINVAL;
		goto err;
	}

	/* alloc and init data object */
	ft5x06 = kzalloc(sizeof(struct ft5x06_data), GFP_KERNEL);
	if (ft5x06 == NULL) {
		dev_err(dev, "fail to allocate data object\n");
		error = -ENOMEM;
		goto err;
	}
	ft5x06->dev  = dev;
	ft5x06->irq  = gpio_to_irq(pdata->irq_gpio);
	ft5x06->bops = bops;
	if (dev->of_node)
		ft5x06->dev->platform_data = pdata;

	/* init platform stuff */
	if (pdata->power_init) {
		error = pdata->power_init(true);
		if (error) {
			dev_err(dev, "fail to power_init platform (pdata)\n");
			goto err_free_data;
		}
	} else {
		error = ft5x06_power_init(ft5x06, true);
		if (error) {
			dev_err(dev, "fail to power_init platform\n");
			goto err_free_data;
		}
	}

	if (pdata->power_on) {
		error = pdata->power_on(true);
		if (error) {
			dev_err(dev, "fail to power on (pdata)!\n");
			goto err_power_init;
		}
	} else {
		error = ft5x06_power_on(ft5x06, true);
		if (error) {
			dev_err(dev, "fail to power on\n");
			goto err_power_init;
		}
	}

	if (gpio_is_valid(pdata->irq_gpio)) {
		error = gpio_request(pdata->irq_gpio, "ft5x06_irq_gpio");
		if (error < 0) {
			dev_err(dev, "irq gpio request failed");
			goto err_power;
		}
		error = gpio_direction_input(pdata->irq_gpio);
		if (error < 0) {
			dev_err(dev, "set_direction for irq gpio failed\n");
			goto free_irq_gpio;
		}
	}

	if (gpio_is_valid(pdata->reset_gpio)) {
		error = gpio_request(pdata->reset_gpio, "ft5x06_reset_gpio");
		if (error < 0) {
			dev_err(dev, "irq gpio request failed");
			goto free_irq_gpio;
		}
		error = gpio_direction_output(pdata->reset_gpio, 0);
		if (error < 0) {
			dev_err(dev, "set_direction for irq gpio failed\n");
			goto free_reset_gpio;
		}
		msleep(100);
		gpio_set_value_cansleep(pdata->reset_gpio, 1);
	}

	msleep(100);
	mutex_init(&ft5x06->mutex);

	/* alloc and init input device */
	ft5x06->input = input_allocate_device();
	if (ft5x06->input == NULL) {
		dev_err(dev, "fail to allocate input device\n");
		error = -ENOMEM;
		goto free_reset_gpio;
	}

	input_set_drvdata(ft5x06->input, ft5x06);
	ft5x06->input->name       = "ft5x06";
	ft5x06->input->id.bustype = bops->bustype;
	ft5x06->input->id.vendor  = 0x4654; /* FocalTech */
	ft5x06->input->id.product = 0x5000; /* ft5x0x    */
	ft5x06->input->id.version = 0x0100; /* 1.0       */
	ft5x06->input->dev.parent = dev;

	/* init touch parameter */
#ifdef CONFIG_TOUCHSCREEN_FT5X06_TYPEB
	input_mt_init_slots(ft5x06->input, FT5X0X_MAX_FINGER);
#endif
	set_bit(ABS_MT_TOUCH_MAJOR, ft5x06->input->absbit);
	set_bit(ABS_MT_POSITION_X, ft5x06->input->absbit);
	set_bit(ABS_MT_POSITION_Y, ft5x06->input->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, ft5x06->input->absbit);
	set_bit(INPUT_PROP_DIRECT, ft5x06->input->propbit);

	input_set_abs_params(ft5x06->input,
			     ABS_MT_POSITION_X, 0, pdata->x_max, 0, 0);
	input_set_abs_params(ft5x06->input,
			     ABS_MT_POSITION_Y, 0, pdata->y_max, 0, 0);
	input_set_abs_params(ft5x06->input,
			     ABS_MT_TOUCH_MAJOR, 0, pdata->z_max, 0, 0);
	input_set_abs_params(ft5x06->input,
			     ABS_MT_WIDTH_MAJOR, 0, pdata->w_max, 0, 0);
	input_set_abs_params(ft5x06->input,
			     ABS_MT_TRACKING_ID, 0, 10, 0, 0);

	set_bit(EV_KEY, ft5x06->input->evbit);
	set_bit(EV_ABS, ft5x06->input->evbit);

	error = ft5x06_read_byte(ft5x06, FT5X0X_REG_CHIP_ID, &ft5x06->chip_id);
	if (error) {
		dev_err(dev, "failed to read chip id\n");
		goto err_free_input;
	}

	error = ft5x06_load_firmware(ft5x06, pdata->firmware, NULL);
	if (error) {
		dev_err(dev, "fail to load firmware\n");
		goto err_free_input;
	}

	ft5x06->input->enable = ft5x06_input_enable;
	ft5x06->input->disable = ft5x06_input_disable;
	ft5x06->input->enabled = true;
	/* register input device */
	error = input_register_device(ft5x06->input);
	if (error) {
		dev_err(dev, "fail to register input device\n");
		goto err_free_input;
	}

	ft5x06->input->phys =
		kobject_get_path(&ft5x06->input->dev.kobj, GFP_KERNEL);
	if (ft5x06->input->phys == NULL) {
		dev_err(dev, "fail to get input device path\n");
		error = -ENOMEM;
		goto err_unregister_input;
	}

	/* start interrupt process */
	error = request_threaded_irq(ft5x06->irq, NULL, ft5x06_interrupt,
				IRQF_TRIGGER_FALLING, "ft5x06", ft5x06);
	if (error) {
		dev_err(dev, "fail to request interrupt\n");
		goto err_free_phys;
	}

	/* export sysfs entries */
	ft5x06->vkeys_dir = kobject_create_and_add("board_properties", NULL);
	if (ft5x06->vkeys_dir == NULL) {
		error = -ENOMEM;
		dev_err(dev, "fail to create board_properties entry\n");
		goto err_free_irq;
	}

	sysfs_attr_init(&ft5x06->vkeys_attr.attr);
	ft5x06->vkeys_attr.attr.name = "virtualkeys.ft5x06";
	ft5x06->vkeys_attr.attr.mode = (S_IRUSR|S_IRGRP|S_IROTH);
	ft5x06->vkeys_attr.show      = ft5x06_vkeys_show;

	error = sysfs_create_file(ft5x06->vkeys_dir, &ft5x06->vkeys_attr.attr);
	if (error) {
		dev_err(dev, "fail to create virtualkeys entry\n");
		goto err_put_vkeys;
	}

	error = sysfs_create_group(&dev->kobj, &ft5x06_attr_group);
	if (error) {
		dev_err(dev, "fail to export sysfs entires\n");
		goto err_put_vkeys;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ft5x06->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN+1;
	ft5x06->early_suspend.suspend = ft5x06_early_suspend;
	ft5x06->early_suspend.resume  = ft5x06_early_resume;
	register_early_suspend(&ft5x06->early_suspend);
#endif

	ft5x06->power_supply_notifier.notifier_call = ft5x06_power_supply_event;
	register_power_supply_notifier(&ft5x06->power_supply_notifier);

	INIT_DELAYED_WORK(&ft5x06->noise_filter_delayed_work,
				ft5x06_noise_filter_delayed_work);
	return ft5x06;

err_put_vkeys:
	kobject_put(ft5x06->vkeys_dir);
err_free_irq:
	free_irq(ft5x06->irq, ft5x06);
err_free_phys:
	kfree(ft5x06->input->phys);
err_unregister_input:
	input_unregister_device(ft5x06->input);
	ft5x06->input = NULL;
err_free_input:
	input_free_device(ft5x06->input);
free_reset_gpio:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
free_irq_gpio:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_power:
	if (pdata->power_on)
		pdata->power_on(false);
	//else
	//	ft5x06_power_on(ft5x06, false);
err_power_init:
	if (pdata->power_init)
		pdata->power_init(false);
	//else
	//	ft5x06_power_init(ft5x06, false);
err_free_data:
	kfree(ft5x06);
err:
	return ERR_PTR(error);
}
EXPORT_SYMBOL_GPL(ft5x06_probe);

void ft5x06_remove(struct ft5x06_data *ft5x06)
{
	struct ft5x06_ts_platform_data *pdata = ft5x06->dev->platform_data;

	cancel_delayed_work_sync(&ft5x06->noise_filter_delayed_work);
	unregister_power_supply_notifier(&ft5x06->power_supply_notifier);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ft5x06->early_suspend);
#endif
	sysfs_remove_group(&ft5x06->dev->kobj, &ft5x06_attr_group);
	kobject_put(ft5x06->vkeys_dir);
	free_irq(ft5x06->irq, ft5x06);
	kfree(ft5x06->input->phys);
	input_unregister_device(ft5x06->input);
	kfree(ft5x06);
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
	if (pdata->power_on)
		pdata->power_on(false);
	if (pdata->power_init)
		pdata->power_init(false);
}
EXPORT_SYMBOL_GPL(ft5x06_remove);

MODULE_AUTHOR("Zhang Bo <zhangbo_a@xiaomi.com>");
MODULE_DESCRIPTION("ft5x0x touchscreen input driver");
MODULE_LICENSE("GPL");
