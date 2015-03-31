/*
 * Copyright (C) 2011-2015 XiaoMi, Inc.
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
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/fts_ts.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/power_supply.h>
#include <linux/of_gpio.h>
#include "fts_ts.h"

#define DRIVER_FTS		"fts_dummy"
#define FTS_MAJOR		0x0A
#define FTS_MINOR		0xF3

#define FTS_TS_DRV_NAME	"fts"
#define FTS_TS_DRV_VERSION	"1108"

#define FTS_FIFO_MAX		32
#define FTS_EVENT_SIZE		8

#define X_AXIS_MAX		1080
#define X_AXIS_MIN		0
#define Y_AXIS_MAX		1920
#define Y_AXIS_MIN		0

#define FTS_PANEL_BIEL		0x02
#define FTS_PANEL_WINTEK	0x04

//register address
#define FTS_ID0		0x39
#define FTS_ID1		0x80

#define READ_STATUS		0x84
#define READ_ONE_EVENT		0x85
#define READ_ALL_EVENT		0x86

// command
#define SLEEPIN		0x90
#define SLEEPOUT		0x91
#define SENSEOFF		0x92
#define SENSEON		0x93
#define SELF_SENSEOFF		0x94
#define SELF_SENSEON		0x95
#define PROXIMITY_ON		0x97
#define GESTURE_ON		0x9D
#define MS_HOVER_OFF		0x9E
#define MS_HOVER_ON		0x9F
#define SYS_RESET		0xA0
#define FLUSHBUFFER		0xA1
#define FORCECALIBRATION		0xA2
#define CX_TUNNING		0xA3
#define SELF_TUNING		0xA4
#define ITO_TEST			0xA7
#define CHARGER_IN		0xA8
#define DUMP_PANEL_INFO		0xAA
#define CHARGER_OUT		0xAB
#define HOVER_ON		0x95
#define SS_BUTTON_ON		0x9B
#define SAVE_CX_TUNE		0xFC
#define SAVE_SETTING_TO_FLASH	0xFB

#define FW_CMD_BURN_MAIN_BLOCK		0xF2
#define FW_CMD_BURN_INFO_BLOCK		0xFB
#define FW_CMD_ERASE_MAIN_BLOCK		0xF3

#define EVENTID_NO_EVENT		0x00
#define EVENTID_ENTER_POINTER		0x03
#define EVENTID_LEAVE_POINTER		0x04
#define EVENTID_MOTION_POINTER		0x05
#define EVENTID_HOVER_ENTER_POINTER		0x07
#define EVENTID_HOVER_LEAVE_POINTER		0x08
#define EVENTID_HOVER_MOTION_POINTER		0x09
#define EVENTID_ERROR		0x0F
#define EVENTID_CONTROLLER_READY		0x10
#define EVENTID_SLEEPOUT_CONTROLLER_READY		0x11
#define EVENTID_PROXIMITY_ENTER		0x0B
#define EVENTID_PROXIMITY_LEAVE		0x0C
#define EVENTID_BUTTON		0x0E
#define EVENTID_3D_GESTURE		0x20

#define FW_MAIN_BLOCK		0x01
#define FW_INFO_BLOCK		0x02

#define INT_ENABLE		0x41
#define INT_DISABLE		0x00

#define MAIN_BLOCK_ADDR		0x00000000
#define MAIN_BLOCK_SIZE		(64 * 1024)
#define INFO_BLOCK_ADDR		0x0000F800
#define INFO_BLOCK_SIZE		(2 * 1024)

#define FINGER_MAX		10

#define WRITE_CHUNK_SIZE		(2 * 1024) - 16

#define GESTURE_LEFT_TO_RIGHT		0x1
#define GESTURE_RIGHT_TO_LEFT		0x2
#define GESTURE_TOP_TO_BOTTOM		0x3
#define GESTURE_BOTTOM_TO_TOP		0x4

#define FW_VERSION_INFO	0x0
#define CFG_VERSION_INFO	0x1
#define AFE_INFO_IN_CONFIG	0x2
#define AFE_INFO_IN_CX		0x3

#define FTS_INPUT_EVENT_SENSITIVE_MODE_OFF		0
#define FTS_INPUT_EVENT_SENSITIVE_MODE_ON		1

struct fts_data {
	struct device *dev;
	struct fts_ts_platform_data *pdata;
	struct input_dev *input_dev;
	int  irq;
	bool dbgdump;
	int dumval;
	struct mutex lock;
	bool enabled;
	const struct fts_bus_ops *bops;
	unsigned char ID_Indx[FINGER_MAX];
	int touch_is_pressed;
	unsigned char hover_on;
	u8 test_result;
	bool charger_plug_in;
	u8 sensitive_mode;
	struct notifier_block power_supply_notifier;
	struct work_struct switch_mode_work;
	int lcd_id;
	int module_id;
	int current_index;
};

static int fts_get_panel_id(struct fts_data *fts)
{

	fts->lcd_id = 0;
	return 0;
}

static int fts_send_block(struct fts_data *fts,
				void *buf, int len)
{
	int ret;
	ret = fts->bops->send(fts->dev, buf, len);
	if (ret >= 0)
		return 0;
	else
		return ret;
}

static int fts_recv_block(struct fts_data *fts,
				void *wbuf, int wlen,
				void *rbuf, int rlen)
{
	int ret;
	ret = fts->bops->recv(fts->dev, wbuf, wlen, rbuf, rlen);
	if (ret >= 0)
		return 0;
	else
		return ret;
}

static void fts_delay(unsigned int ms)
{
	if (ms < 20)
		mdelay(ms);
	else
		msleep(ms);
}

#define GOLDEN_ERR	0
#define RAW_DATA_ERR	1
static void fts_report_auto_tune_error(struct fts_data *fts,
					unsigned char cmd, unsigned char val,
					unsigned char type)
{
	int i;

	char *mut_golden_array[] = {
			"error in CX1\n",
			"error in CX2\n",
			"error in CX1 and CX2\n",
	};
	char *self_golden_array [] = {
			"error in FIX1\n",
			"error in SIX1\n",
			"error in FCX2\n",
			"error in SCX2\n",
	};
	char *self_raw_array [] = {
			"raw equals 4095\n",
			"raw equals 0\n",
			"self raw out of window\n",
			"self sense NMAX overflow\n",
	};


	if (cmd == CX_TUNNING) {
		if (type == GOLDEN_ERR) {
			for (i = 0; i < sizeof(mut_golden_array); i++) {
				if (val == i + 1) {
					dev_info(fts->dev, mut_golden_array[i]);
					break;
				}
			}
		} else if (type == RAW_DATA_ERR)
			dev_info(fts->dev, "err for mutual raw!\n");
	} else if (cmd == SELF_TUNING) {
		if (type == GOLDEN_ERR) {
			for (i = 0; i < sizeof(self_golden_array); i++) {
				if (val & (1 << i))
					dev_info(fts->dev, self_golden_array[i]);
			}
		} else if (type == RAW_DATA_ERR) {
			for (i = 0; i < sizeof(self_raw_array); i++) {
				if (val & (1 << i))
					dev_info(fts->dev, self_raw_array[i]);
			}
		}
	}
}

static int fts_wait_cmd_ready(struct fts_data *fts, unsigned char cmd)
{
	int ret;
	int i;
	unsigned char data[8];
	unsigned char regAdd;
	int timeout = 500;
	unsigned val = 0;

	for (i = 0; i < timeout; i++) {
		regAdd = READ_ONE_EVENT;
		ret = fts_recv_block(fts, &regAdd, 1, data, FTS_EVENT_SIZE);
		if (fts->dbgdump)
			dev_info(fts->dev, "data recevice = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
				data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
		switch(cmd) {
			case SLEEPOUT:
				val = 0x05;
				break;
			case CX_TUNNING:
				val = 0x01;
				break;
			case SELF_TUNING:
				val = 0x02;
				break;
			default:
				return -EINVAL;
		}
		if ((data[0] == 0x16) && (data[1] == val)) {
			if (cmd == SELF_TUNING || cmd == CX_TUNNING) {
				dev_info(fts->dev, "read data = 0x%x 0x%x 0x%x, 0x%x 0x%x 0x%x\n",
				data[2], data[3], data[4], data[5], data[6], data[7]);
				if (data[2] != 0 || data[3] != 0) {
					dev_info(fts->dev, "self tuning failed, retry!\n");
					if (data[2] != 0)
						fts_report_auto_tune_error(fts, cmd,
								data[2], GOLDEN_ERR);
					if (data[3] != 0)
						fts_report_auto_tune_error(fts, cmd,
								data[3], RAW_DATA_ERR);
					//return -EAGAIN;
					return 0;
				}
			}

			dev_info(fts->dev, "Wait for cmd 0x%x ready!\n", cmd);
			return 0;
		}
		msleep(10);
	}

	dev_err(fts->dev, "Wait for cmd 0x%x timeout!\n", cmd);
	return -ETIMEDOUT;
}

static int fts_command(struct fts_data *fts, unsigned char cmd)
{
	unsigned char regAdd;
	int ret = 0;
	bool need_wait = false;

	if (cmd == SLEEPOUT ||
		cmd == CX_TUNNING || cmd == SELF_TUNING)
		need_wait = true;

	regAdd = cmd;
	ret = fts_send_block(fts, &regAdd, 1);
	if (ret)
		dev_err(fts->dev, "fts_command 0x%x failed\n", cmd);

	if (need_wait)
		ret = fts_wait_cmd_ready(fts, cmd);

	return ret;
}

static int fts_get_module_id(struct fts_data *fts)
{
	unsigned char regAdd[4];
	unsigned char val[8];
	int ret;
	int time_out = 100;
	int i;

	regAdd[0] = 0xB2;
	regAdd[1] = 0x07;
	regAdd[2] = 0xEA;
	regAdd[3] = 0x08;

	ret = fts_send_block(fts, regAdd, sizeof(regAdd));
	if (ret) {
		dev_err(fts->dev, "send block failed\n");
		return -EINVAL;
	}

	for (i = 0; i < time_out; i++) {
		regAdd[0] = 0x85;
		ret = fts_recv_block(fts, &regAdd[0], 1, val, 8);
		if (ret) {
			fts->module_id = FTS_PANEL_BIEL;
			dev_err(fts->dev, "recv block failed\n");
			return -EINVAL;
		}
		if (val[0] == 0x12 && val[1] == 0x07 && val[2] == 0xEA)
			break;
		msleep(10);
	}

	pr_info("panel info = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		val[0], val[1], val[2], val[3], val[4], val[5], val[6], val[7]);
	fts->module_id = val[3];

	ret = fts_command(fts, FLUSHBUFFER);

	return ret;
}

static void fts_get_config_index(struct fts_data *fts)
{
	int error, i;
	struct fts_ts_platform_data *pdata = fts->dev->platform_data;

	error = fts_get_panel_id(fts);
	if (error)
		dev_warn(fts->dev, "Failed to get panel id, just give default setting!\n");

	error = fts_get_module_id(fts);
	if (error)
		dev_warn(fts->dev, "Failed to get module id, just give default setting!\n");

	for (i = 0; i < pdata->config_array_size; i++) {
		if (fts->lcd_id == pdata->config_info[i].lcd_id &&
			fts->module_id == pdata->config_info[i].module_id) {
			dev_info(fts->dev, "Choose config index = %d\n", i);
			fts->current_index = i;
			break;
		}
	}
}

static int fts_system_reset(struct fts_data *fts)
{
	unsigned char regAdd[4] = {0xB6, 0x00, 0x23, 0x01};
	int ret = 0;

	dev_info(fts->dev, "FTS system reset\n");
	ret = fts_send_block(fts, regAdd, sizeof(regAdd));
	if (ret)
		dev_err(fts->dev, "fts_system_reset failed\n");

	fts_delay(10);
	return ret;
}

static int fts_interrupt_set(struct fts_data *fts, int enable)
{
	unsigned char regAdd[4] = {0};
	int ret = 0;

	regAdd[0] = 0xB6;
	regAdd[1] = 0x00;
	regAdd[2] = 0x1C;
	regAdd[3] = (unsigned char)enable;

	if (enable)
		dev_info(fts->dev, "FTS interrupt enable\n");
	else
		dev_info(fts->dev, "FTS interrupt disable\n");

	ret = fts_send_block(fts, regAdd, sizeof(regAdd));
	if (ret)
		dev_err(fts->dev, "interrupt set failed\n");

	return ret;
}

int fw_flash_cmd(struct fts_data *fts, char block_type)
{
	int ret = 0;
	unsigned char regAdd = 0;

	if ((block_type == FW_MAIN_BLOCK) || (block_type == FW_INFO_BLOCK)) {
		regAdd = FW_CMD_BURN_MAIN_BLOCK;
		ret = fts_send_block(fts, &regAdd, 1);
		if (ret) {
			dev_err(fts->dev, "fts send block error when flash!\n");
			return ret;
		}
	}
	else
		dev_err(fts->dev, "Block Type error \n");

	if (block_type == FW_INFO_BLOCK) {
		regAdd = FW_CMD_BURN_INFO_BLOCK;
		ret = fts_send_block(fts, &regAdd, 1);
	}

	if (ret)
		dev_err(fts->dev, "fw_write_cmd error \n");
	else {
		dev_info(fts->dev, "\n Flash burning starts...\n");
		fts_delay(3000);
	}

	return ret;
}

int fw_erase(struct fts_data *fts, char block_type)
{
	int ret = 0;
	unsigned char regAdd = 0;

	if ((block_type == FW_MAIN_BLOCK) || (block_type == FW_INFO_BLOCK)) {
		regAdd = FW_CMD_ERASE_MAIN_BLOCK;
		ret = fts_send_block(fts, &regAdd, 1);
		if (ret) {
			dev_err(fts->dev, "fts send block error when erase!\n");
			return ret;
		}
	}
	else
		dev_err(fts->dev, "Block Type error \n");

	if (ret)
		dev_err(fts->dev, "fw_erase error \n");
	else {
		dev_info(fts->dev, "\n Flash Erase\n");
		fts_delay(3000);
	}

	return ret;
}

unsigned char fw_load_ram(struct fts_data *fts, unsigned int addr,
                          unsigned int maxsize, unsigned char *pData,
                          int size)
{
	unsigned int writeAddr = 0;
	int i = 0;
	int j = 0;
	unsigned char* byteWork1;
	unsigned char regAdd[3] = {
		0xB3, 0xB1, 0
	};

	int ret = 0;

	dev_info(fts->dev, "FTS fw_load_ram called\n");
	dev_info(fts->dev, "FTS addr = %d, maxsize = %d, size = %d\n", addr, maxsize, size);

	byteWork1 = kmalloc(WRITE_CHUNK_SIZE + 3, GFP_KERNEL);
	if (byteWork1 == NULL) {
		dev_err(fts->dev, "unable to malloc enough mem!\n");
		return -ENOMEM;
	}

	while ((j < maxsize) && (j < size))
	{
		writeAddr = addr + j;

		regAdd[0] = 0xB3;
		regAdd[1] = (writeAddr >> 24) & 0xFF;
		regAdd[2] = (writeAddr >> 16) & 0xFF;
		ret = fts_send_block(fts, &regAdd[0], 3);
		if (ret) {
			dev_err(fts->dev, "fw_load_ram send block B3 error!\n");
			goto end;
		}

		byteWork1[0] = 0xB1;
		byteWork1[1] = (writeAddr >> 8) & 0xFF;
		byteWork1[2] = writeAddr & 0xFF;

		i = 0;
		while ((j < maxsize) && (i < WRITE_CHUNK_SIZE)) {
			byteWork1[i + 3] = pData[j];
			i++;
			j++;
		}
		ret = fts_send_block(fts, &byteWork1[0], WRITE_CHUNK_SIZE + 3);
		if (ret) {
			dev_err(fts->dev, "fw_load_ram send block B1 error!\n");
			goto end;
		}
	}

	dev_info(fts->dev, "FTS Done %d Bytes\n", j);

end:
	kfree(byteWork1);
	return ret;
}

int fw_unlock(struct fts_data *fts)
{
	int ret = 0;
	unsigned char regAdd[4] = {
		0xF7, 0x74, 0x45, 0x0
	};

	ret = fts_send_block(fts, &regAdd[0], 4);

	if (ret)
		dev_err(fts->dev, "FTS erase error \n");
	else {
		dev_info(fts->dev, "\n FTS Flash Unlocked\n");
		fts_delay(3000);
	}

	return ret;
}

static int fts_parsing_memh(struct fts_data *fts, unsigned char **pOut,
                            int *pOutsize,
                            char block_type)
{
	int pos = 0;
	const struct firmware *fw = NULL;
	unsigned char *pData = NULL;
	struct fts_ts_platform_data *pdata = fts->dev->platform_data;
	int index = fts->current_index;

	const char *fwName;
	int ret = 0;

	if (block_type == FW_MAIN_BLOCK) {
		fwName = pdata->config_info[index].firmware_name;
	}
	else if (block_type == FW_INFO_BLOCK) {
		fwName = pdata->config_info[index].config_name;
	}
	else {
		dev_err(fts->dev, "FTS wrong block_type\n");
		return -EINVAL;
	}

	dev_info(fts->dev, "FTS fts_parsing_memh fileName = %s\n", fwName);

	ret = request_firmware(&fw, fwName, fts->dev);
	if (ret) {
		dev_err(fts->dev, "Failed to request firmware!\n");
		return ret;
	}

	dev_info(fts->dev, "fts_upgrade filp_open success\n");
	pData = (unsigned char *)kmalloc(MAIN_BLOCK_SIZE * 2, GFP_KERNEL);
	if (pData == NULL) {
		dev_info(fts->dev, "FTS kmalloc failed\n");
		goto end;
	}

	pos = 0;

	for (pos = 0; pos < fw->size; pos++)
		pData[pos] = fw->data[pos];

	*pOut = pData;
	*pOutsize = (int)fw->size;

end:
	release_firmware(fw);
	return ret;
}

static int fts_upgrade(struct fts_data *fts, char block_type)
{
	unsigned char *pData = NULL;
	int datasize = 0;
	int ret;
	int blkAddr = 0;
	int blkSize = 0;

	ret = fts_parsing_memh(fts, &pData, &datasize, block_type);
	if (ret)
		return ret;

	dev_info(fts->dev, "FTS %d KB \n", datasize / 1024);

	if ((pData == NULL) || (datasize == 0)) {
		dev_info(fts->dev, "fts_upgrade file read error\n");
		return -EINVAL;
	}

	ret = fw_unlock(fts);
	if (ret)
		goto end;

	if (block_type == FW_MAIN_BLOCK) {
		blkAddr = MAIN_BLOCK_ADDR;
		blkSize = MAIN_BLOCK_SIZE;
	}
	else if (block_type == FW_INFO_BLOCK) {
		blkAddr = INFO_BLOCK_ADDR;
		blkSize = INFO_BLOCK_SIZE;
	}

	ret = fw_load_ram(fts, blkAddr, blkSize, pData, datasize);
	if (ret)
		goto end;

	ret = fw_erase(fts, block_type);
	if (ret)
		goto end;

	ret = fw_flash_cmd(fts, block_type);

end:
	kfree(pData);
	return ret;
}

static u16 fts_get_firmware_info(struct fts_data *fts, int flag)
{
	unsigned char regAdd[4] = {0};
	unsigned char val[8] = {0};
	int ret = 0;
	int dummy_count = 2;
	int i;
	u16 chk_val;
	int len = 0;

	/* here we should do 2 dummy read to flush the redundant events*/
	/* or we could not get the version correctly.*/
	for (i = 0; i < dummy_count; i++) {
		regAdd[0] = 0x85;
		ret = fts_recv_block(fts, regAdd, 1, val, 8);
		if (ret) {
			dev_err(fts->dev, "recv block failed\n");
			return -EINVAL;
		}
	}

	if (flag == CFG_VERSION_INFO) {
		regAdd[0] = 0xB2;
		regAdd[1] = 0x00;
		regAdd[2] = 0x01;
		regAdd[3] = 0x01;
		len = 4;
	} else if (flag == AFE_INFO_IN_CONFIG) {
		regAdd[0] = 0xB2;
		regAdd[1] = 0x07;
		regAdd[2] = 0xFB;
		regAdd[3] = 0x01;
		len = 4;
	} else if (flag == AFE_INFO_IN_CX) {
		regAdd[0] = 0xB2;
		regAdd[1] = 0x17;
		regAdd[2] = 0xFB;
		regAdd[3] = 0x01;
		len = 4;
	} else if (flag == FW_VERSION_INFO) {
		regAdd[0] = 0xB6;
		regAdd[1] = 0x00;
		regAdd[2] = 0x07;
		len = 3;
	}

	if (flag == FW_VERSION_INFO) {
		ret = fts_recv_block(fts, regAdd, len, val, 8);
		if (ret) {
			dev_err(fts->dev, "Failed to read chip info!\n");
			return -EINVAL;
		}
	}

	else {
		ret = fts_send_block(fts, regAdd, len);
		if (ret) {
			dev_err(fts->dev, "send block failed\n");
			return -EINVAL;
		}

		regAdd[0] = 0x85;
		ret = fts_recv_block(fts, regAdd, 1, val, 8);
		if (ret) {
			dev_err(fts->dev, "recv block failed\n");
			return -EINVAL;
		}
	}

	pr_info("%02x %02x %02x %02x %02x %02x %02x %02x \n",
		val[0], val[1], val[2], val[3], val[4],
		val[5], val[6], val[7]);

	if (flag != FW_VERSION_INFO)
		chk_val = (u16)val[3];
	else
		chk_val = (val[4] << 8) | val[5];

	return chk_val;
}

static void fts_get_chip_info(struct fts_data *fts, unsigned char *chip_info, int len)
{
	int ret;
	unsigned char regAdd[3] = {0xB6, 0x00, 0x07};

	ret = fts_recv_block(fts, regAdd, 3, chip_info, len);
	if (ret) {
		dev_err(fts->dev, "Failed to read chip info!\n");
		return;
	}

	dev_info(fts->dev, "FTS %02X%02X%02X ", regAdd[0], regAdd[1], regAdd[2]);
	print_hex_dump(KERN_DEBUG, "value is:", DUMP_PREFIX_NONE, 16, 1,
			       chip_info, len, false);
}

static bool fts_check_upgrade_fw_cfg(struct fts_data *fts, u16 fw_version,
					u8 cfg_version, bool force_update)
{
	int retval;
	struct fts_ts_platform_data *pdata = fts->pdata;
	const struct firmware *fw_entry_cfg = NULL;
	const struct firmware *fw_entry_fw = NULL;
	u8 cfg_ver_new;
	u16 fw_ver_new;
	int index = fts->current_index;

	retval = request_firmware(&fw_entry_cfg, pdata->config_info[index].config_name, fts->dev);
	if (retval) {
		dev_err(fts->dev, "request config failed\n");
		return false;
	}
	retval = request_firmware(&fw_entry_fw, pdata->config_info[index].firmware_name, fts->dev);
	if (retval) {
		dev_err(fts->dev, "request firmware failed\n");
		return false;
	}

	cfg_ver_new = fw_entry_cfg->data[1];
	fw_ver_new = (fw_entry_fw->data[192] << 8) | (fw_entry_fw->data[193]);
	release_firmware(fw_entry_cfg);
	release_firmware(fw_entry_fw);

	dev_info(fts->dev, "in chip: cfg_ver = 0x%x, fw ver = 0x%x\n",
		cfg_version, fw_version);
	dev_info(fts->dev, "new: cfg_ver = 0x%x, fw ver = 0x%x\n",
		cfg_ver_new, fw_ver_new);


	if (cfg_ver_new != cfg_version ||
		fw_ver_new != fw_version ||
		force_update) {
		unsigned char chip_info[9];
		int retry_time = 5;
		int i = 0;
		/* not equal version ,update */
update:
		if (fw_ver_new != fw_version) {
			retval = fts_upgrade(fts, FW_MAIN_BLOCK);
			if (retval) {
				dev_err(fts->dev, "upgrade main block failed\n");
				return false;
			}
		}

		retval = fts_upgrade(fts, FW_INFO_BLOCK);
		if (retval) {
			dev_err(fts->dev, "upgrade info block failed\n");
			return false;
		}

		fts_get_chip_info(fts, chip_info, sizeof(chip_info));
		if (chip_info[3] == 0 && chip_info[4] == 0 && chip_info[5] == 0 &&
			chip_info[6] == 0 && chip_info[7] == 0 && chip_info[8] == 0) {
			if (i < retry_time) {
				i++;
				goto update;
			} else
				dev_err(fts->dev, "Failed to flash within 5 times!\n");
		}
		return true;
	} else
		dev_info(fts->dev, "version equal, keep the current fw and cfg!\n");

	return false;
}

static int fts_sense_control(struct fts_data *fts, int selfon)
{
	int error;
	u8 ms_hover_val;
	u8 self_sense_val;

	if (selfon) {
		ms_hover_val = MS_HOVER_OFF;
		self_sense_val = SELF_SENSEON;
		fts->hover_on = 1;
	} else {
		ms_hover_val = MS_HOVER_ON;
		self_sense_val = SELF_SENSEOFF;
		fts->hover_on = 0;
	}

	error = fts_command(fts, ms_hover_val);
	if (error) {
		dev_err(fts->dev, "Failed to mutual sense on/off\n");
		return error;
	}

	error = fts_command(fts, self_sense_val);
	if (error) {
		dev_err(fts->dev, "Failed to self sense on/off\n");
		return error;
	}

	error = fts_command(fts, FORCECALIBRATION);
	if (error) {
		dev_err(fts->dev, "Failed to do force calib\n");
	}

	return error;
}

static int fts_save_auto_tune_status(struct fts_data *fts)
{
	int ret;

	ret = fts_command(fts, SAVE_CX_TUNE);
	if (ret) {
		dev_err(fts->dev,
			"fts_save_auto_tune_status command save_cx_tune error!\n");
		return ret;
	}

	ret = fts_command(fts, SAVE_SETTING_TO_FLASH);
	if (ret) {
		dev_err(fts->dev,
			"fts_save_auto_tune_status command save_setting_to_flash error!\n");
		return ret;
	}

	return 0;
}

static int fts_do_auto_tuning(struct fts_data *fts)
{
	int ret, i;
	int max_retry_time = 10;

	dev_info(fts->dev, "Do auto tuning work!\n");
	ret = fts_command(fts, CX_TUNNING);
	if (ret)
		return ret;

	for (i = 0; i < max_retry_time; i++) {
		ret = fts_command(fts, SELF_TUNING);
		if (ret) {
			if (ret == -EAGAIN) {
				msleep(1000);
				continue;
			}
			else
				return ret;
		}
		break;
	}

	ret = fts_command(fts, FORCECALIBRATION);
	if (ret)
		return ret;

	ret = fts_save_auto_tune_status(fts);

	return ret;
}

void fts_charger_state_changed(struct fts_data *fts)
{
	int ret;
	u8 is_usb_exist;
	is_usb_exist = power_supply_is_system_supplied();

	if (is_usb_exist) {
		if (!fts->charger_plug_in) {
			dev_info(fts->dev, "Power state changed, set to charge in\n");
			ret = fts_command(fts, CHARGER_IN);
			if (ret)
				dev_err(fts->dev, "Failed to do charge in!\n");
			fts->charger_plug_in = true;
		} else
			dev_info(fts->dev, "in: No state changed, just ignore!\n");
	} else {
		if (fts->charger_plug_in) {
			dev_info(fts->dev, "Power state changed, set to charge out\n");
			ret = fts_command(fts, CHARGER_OUT);
			if (ret)
				dev_err(fts->dev, "Failed to do charge out!\n");
			fts->charger_plug_in = false;
		} else
			dev_info(fts->dev, "out: No state changed, just ignore!\n");
	}
}

static int fts_init(struct fts_data *fts, bool force_tuning)
{
	unsigned char val[3];
	int ret;
	u8 cfg_version;
	u16 fw_version;
	u8 afe_info_in_config, afe_info_in_cx;
	bool is_update = false;
	bool force_update = false;

	dev_info(fts->dev, "fts_init called\n");

	//TS Chip ID
	fts_get_chip_info(fts, val, sizeof(val));
	if (val[1] != FTS_ID0 || val[2] != FTS_ID1)
		return -EINVAL;

reset:
	ret = fts_system_reset(fts);
	if (ret)
		goto FAILED_END;

	ret = fts_command(fts, SLEEPOUT);
	if (ret) {
		dev_warn(fts->dev,
			"no response for sleep out, Maybe fw corruption, fw udpate!\n");
		force_update = true;
	}

	fts_get_config_index(fts);

	if (!is_update) {
		cfg_version = (u8)fts_get_firmware_info(fts, CFG_VERSION_INFO);
		fw_version = fts_get_firmware_info(fts, FW_VERSION_INFO);
		if (cfg_version >= 0 && fw_version >= 0) {
			is_update = fts_check_upgrade_fw_cfg(fts,
					fw_version, cfg_version, force_update);
			if (is_update)
				goto reset;
		}
	}

	afe_info_in_config = fts_get_firmware_info(fts, AFE_INFO_IN_CONFIG);
	afe_info_in_cx = fts_get_firmware_info(fts, AFE_INFO_IN_CX);
	if (force_tuning || (afe_info_in_config != afe_info_in_cx)) {
		ret = fts_do_auto_tuning(fts);
		if (ret)
			goto FAILED_END;
	}

	ret = fts_command(fts, SENSEON);
	if (ret)
		goto FAILED_END;

	ret = fts_command(fts, MS_HOVER_OFF);
	if (ret)
		goto FAILED_END;

	ret = fts_command(fts, SS_BUTTON_ON);
	if (ret)
		goto FAILED_END;

	ret = fts_command(fts, FLUSHBUFFER);
	if (ret)
		goto FAILED_END;

	ret = fts_command(fts, FORCECALIBRATION);
	if (ret)
		goto FAILED_END;

	ret = fts_interrupt_set(fts, INT_ENABLE);
	if (ret)
		goto FAILED_END;

FAILED_END:
	return ret;
}

static void fts_controller_ready(struct fts_data *fts)
{
	unsigned char touch_id = 0;

	for (touch_id = 0; touch_id < FINGER_MAX; touch_id++)
		fts->ID_Indx[touch_id] = 0;

	input_sync(fts->input_dev);
}

static void fts_sleepout_controller_ready(struct fts_data *fts)
{
	dev_info(fts->dev, "FTS sleepout controller ready\n");
}

static void fts_error_handler(struct fts_data *fts, unsigned char *data)
{
	dev_info(fts->dev, "FTS error %02X %02X %02X %02X %02X %02X %02X %02X \n",
		data[0], data[1], data[2], data[3],
		data[4], data[5], data[6], data[7]);
}

static void fts_unknown_event_handler(struct fts_data *fts, unsigned char *data)
{
	dev_info(fts->dev, "FTS event %02X %02X %02X %02X %02X %02X %02X %02X \n",
		data[0], data[1], data[2], data[3],
		data[4], data[5], data[6], data[7]);
}

static unsigned char fts_event_handler_type_b(struct fts_data *fts,
	unsigned char *data, unsigned left_event)
{
	struct fts_ts_platform_data *pdata = fts->pdata;
	unsigned char event_num = 0;
	unsigned char num_touches = 0;
	unsigned char touch_id = 0, event_id = 0;
	unsigned char last_left_event = 0;
	int x = 0, y = 0, z = 0;
	unsigned char button_status;
	static unsigned char button_status_prev = 0;
	unsigned char gesture;
	int i;

	for (event_num = 0; event_num < left_event; event_num++) {
		last_left_event = data[7 + event_num * FTS_EVENT_SIZE] & 0x0F;
		num_touches = (data[1 + event_num * FTS_EVENT_SIZE] & 0xF0) >> 4;
		touch_id = data[1 + event_num * FTS_EVENT_SIZE] & 0x0F;
		event_id = data[event_num * FTS_EVENT_SIZE] & 0xFF;

		if (fts->dbgdump)
			dev_info(fts->dev, "event_id = 0x%x\n", event_id);

		switch (event_id) {
			case EVENTID_NO_EVENT:
				break;
			case EVENTID_ENTER_POINTER:
			case EVENTID_MOTION_POINTER:
			case EVENTID_PROXIMITY_ENTER:
			case EVENTID_HOVER_ENTER_POINTER:
			case EVENTID_HOVER_MOTION_POINTER:
				fts->ID_Indx[touch_id] = 1;
				if (event_id == EVENTID_PROXIMITY_ENTER) {
					x = X_AXIS_MAX >> 1;
					y = Y_AXIS_MAX >> 1;
					z = pdata->prox_z;
				} else {
					x = ((data[4 + event_num * 8] & 0xF0) >> 4) |
						((data[2 + event_num * 8]) << 4);
					y = (data[4 + event_num * 8] & 0x0F) |
						((data[3 + event_num * 8]) << 4);
					z = data[ 5 + event_num * 8] & 0x3F;
					if (z < pdata->z_min)
						z = pdata->z_min;
				}

				if (x == X_AXIS_MAX)
					x--;
				if (y == Y_AXIS_MAX)
					y--;

				input_mt_slot(fts->input_dev, touch_id);
				input_mt_report_slot_state(fts->input_dev, MT_TOOL_FINGER, 1);
				input_report_abs(fts->input_dev, ABS_MT_TRACKING_ID, touch_id);
				input_report_abs(fts->input_dev, ABS_MT_POSITION_X, x);
				input_report_abs(fts->input_dev, ABS_MT_POSITION_Y, y);
				if (event_id == EVENTID_HOVER_ENTER_POINTER ||
					event_id == EVENTID_HOVER_MOTION_POINTER)
					input_report_abs(fts->input_dev, ABS_MT_DISTANCE, 1);
				else
					input_report_abs(fts->input_dev, ABS_MT_DISTANCE, 0);

				if (event_id == EVENTID_ENTER_POINTER ||
					event_id == EVENTID_MOTION_POINTER) {
					input_report_abs(fts->input_dev, ABS_MT_TOUCH_MAJOR, z);
					input_report_abs(fts->input_dev, ABS_MT_PRESSURE, z);
					if (fts->dbgdump)
						dev_info(fts->dev, "FTS Pointer ID[%d] X[%3d] Y[%3d] Z[%d]\n",
							touch_id, x, y, z);
				} else {
					input_report_abs(fts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
					input_report_abs(fts->input_dev, ABS_MT_PRESSURE, 0);
					if (fts->dbgdump)
						dev_info(fts->dev, "FTS ID[%d] X[%3d] Y[%3d] Z[%d]\n",
							touch_id, x, y, z);
				}
				break;

			case EVENTID_LEAVE_POINTER:
			case EVENTID_HOVER_LEAVE_POINTER:
			case EVENTID_PROXIMITY_LEAVE:
				fts->ID_Indx[touch_id] = 0;
				input_mt_slot(fts->input_dev, touch_id);
				input_mt_report_slot_state(fts->input_dev, MT_TOOL_FINGER, 0);
				input_report_abs(fts->input_dev, ABS_MT_TRACKING_ID, -1);
				if (fts->dbgdump)
					dev_info(fts->dev, "FTS ID[%d] release\n", touch_id);
				break;

			case EVENTID_CONTROLLER_READY:
				fts_controller_ready(fts);
				break;

			case EVENTID_SLEEPOUT_CONTROLLER_READY:
				fts_sleepout_controller_ready(fts);
				break;

			case EVENTID_ERROR:
				fts_error_handler(fts, &data[event_num * FTS_EVENT_SIZE]);
				button_status_prev = 0;
				break;

			case EVENTID_BUTTON:
				if (fts->pdata->is_mutual_key)
					button_status = data[2 + event_num * FTS_EVENT_SIZE] & 0x0F;
				else
					button_status = data[1 + event_num * FTS_EVENT_SIZE] & 0x0F;
				if (fts->dbgdump)
					dev_info(fts->dev, "button_status = 0x%x\n", button_status);
				for (i = 0; i < fts->pdata->key_num; i++) {
					if(!(button_status_prev & (1 << i)) && (button_status & (1 << i)))
						input_event(fts->input_dev, EV_KEY, fts->pdata->key_codes[i], 1);
					if ((button_status_prev & (1 << i)) && !(button_status & (1 << i)))
						input_event(fts->input_dev, EV_KEY, fts->pdata->key_codes[i], 0);
				}

				button_status_prev = button_status;
				break;

			case EVENTID_3D_GESTURE:
				gesture = data[1 + event_num * FTS_EVENT_SIZE] & 0x0F;
				switch(gesture & 0x7) {
					case GESTURE_LEFT_TO_RIGHT:
						if (fts->dbgdump)
							dev_info(fts->dev, "GESTURE_LEFT_TO_RIGHT\n");
						break;
					case GESTURE_RIGHT_TO_LEFT:
						if (fts->dbgdump)
							dev_info(fts->dev, "GESTURE_RIGHT_TO_LEFT\n");
						break;
					case GESTURE_TOP_TO_BOTTOM:
						if (fts->dbgdump)
							dev_info(fts->dev, "GESTURE_TOP_TO_BOTTOM\n");
						break;
					case GESTURE_BOTTOM_TO_TOP:
						if (fts->dbgdump)
							dev_info(fts->dev, "GESTURE_BOTTOM_TO_TOP\n");
						break;
					default:
						break;
				}
				break;
			default:
				fts_unknown_event_handler(fts, &data[event_num * FTS_EVENT_SIZE]);
				break;
		}
	}

	fts->touch_is_pressed = 0;
	for (touch_id = 0; touch_id < FINGER_MAX; touch_id++) {
		if (fts->ID_Indx[touch_id])
			fts->touch_is_pressed++;
	}

	input_sync(fts->input_dev);

	return last_left_event;
}


static irqreturn_t fts_interrupt(int irq, void *dev_id)
{
	struct fts_data *fts = dev_id;
	unsigned char data[FTS_EVENT_SIZE * FTS_FIFO_MAX];
	int ret;
	unsigned char regAdd;
	unsigned char first_left_event = 0;

	memset(data, 0x0, FTS_EVENT_SIZE);
	regAdd = READ_ONE_EVENT;
	mutex_lock(&fts->input_dev->mutex);

	ret = fts_recv_block(fts, &regAdd, 1, data, FTS_EVENT_SIZE);
	if (ret) {
		dev_info(fts->dev, "interrupt recv block error\n");
		goto end;
	}

	first_left_event = 0;
	first_left_event = fts_event_handler_type_b(fts, data, 1);
	if (first_left_event > 0) {
		memset(data, 0x0, FTS_EVENT_SIZE * first_left_event);
		regAdd = READ_ALL_EVENT;
		ret = fts_recv_block(fts, &regAdd, 1, data, first_left_event * FTS_EVENT_SIZE);
		if (ret) {
			dev_info(fts->dev, "interrupt recv block error (all)\n");
			goto end;
		}

		fts_event_handler_type_b(fts, data, first_left_event);
	}

end:
	mutex_unlock(&fts->input_dev->mutex);
	return IRQ_HANDLED;
}

#ifdef CONFIG_PM_SLEEP
int fts_suspend(struct fts_data *fts)
{
	struct input_dev *input_dev = fts->input_dev;
	int ret;
	int i;

	mutex_lock(&input_dev->mutex);
	ret = fts_interrupt_set(fts, INT_DISABLE);
	if (ret)
		goto end;

	ret = fts_command(fts, FLUSHBUFFER);
	if (ret)
		goto end;

	ret = fts_command(fts, SLEEPIN);
	if (ret)
		goto end;

	for (i = 0; i < FINGER_MAX; i++)
		fts->ID_Indx[i] = 0;

	fts->touch_is_pressed = 0;

	for (i = 0; i < FINGER_MAX; i++) {
		input_mt_slot(input_dev, i);
		input_report_abs(input_dev, ABS_MT_TRACKING_ID, -1);
	}

	input_sync(input_dev);
	fts->enabled = false;

end:
	mutex_unlock(&input_dev->mutex);
	return ret;
}

int fts_resume(struct fts_data *fts)
{
	struct input_dev *input_dev = fts->input_dev;
	int ret;
	int i = 0;
	int count = 10;

	if (fts->enabled)
		return 0;

	mutex_lock(&input_dev->mutex);
retry:
	ret = fts_command(fts, SLEEPOUT);
	if (ret) {
		dev_info(fts->dev, "sleep out timeout, retry i = %d !\n", i);
		fts_system_reset(fts);
		i ++;
		if (i < count)
			goto retry;
	}

	ret = fts_command(fts, SENSEON);
	if (ret)
		goto END;

	ret = fts_command(fts, SS_BUTTON_ON);
	if (ret)
		goto END;

	if (fts->sensitive_mode) {
		ret = fts_command(fts, MS_HOVER_ON);
		if (ret)
			goto END;
	} else {
		ret = fts_command(fts, MS_HOVER_OFF);
		if (ret)
			goto END;
	}

	ret = fts_command(fts, FORCECALIBRATION);
	if (ret)
		goto END;

	ret = fts_interrupt_set(fts, INT_ENABLE);
	if (ret)
		goto END;

	fts->enabled = true;

	fts_charger_state_changed(fts);
END:
	mutex_unlock(&input_dev->mutex);
	return ret;
}
#endif

static int fts_input_enable(struct input_dev *in_dev)
{
	int error = 0;

#ifdef CONFIG_PM
	struct fts_data *fts = input_get_drvdata(in_dev);

	error = fts_resume(fts);
	if (error)
		dev_err(fts->dev, "%s: failed\n", __func__);
#endif

	return error;
}

static int fts_input_disable(struct input_dev *in_dev)
{
	int error = 0;

#ifdef CONFIG_PM
	struct fts_data *fts = input_get_drvdata(in_dev);

	error = fts_suspend(fts);
	if (error)
		dev_err(fts->dev, "%s: failed\n", __func__);
#endif

	return error;
}

static ssize_t fts_dbgdump_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fts_data *fts = dev_get_drvdata(dev);
	int count;

	mutex_lock(&fts->input_dev->mutex);
	count = sprintf(buf, "%d\n", fts->dbgdump);
	mutex_unlock(&fts->input_dev->mutex);

	return count;
}

static ssize_t  fts_dbgdump_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct  fts_data * fts = dev_get_drvdata(dev);
	unsigned long dbgdump;
	int error;

	mutex_lock(& fts->input_dev->mutex);
	error = strict_strtoul(buf, 0, &dbgdump);
	if (!error)
		 fts->dbgdump = dbgdump;

	mutex_unlock(&fts->input_dev->mutex);

	return error ? : count;
}

static ssize_t fts_dumval_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fts_data *fts = dev_get_drvdata(dev);
	int count;

	mutex_lock(&fts->input_dev->mutex);
	count = sprintf(buf, "%d\n", fts->dumval);
	mutex_unlock(&fts->input_dev->mutex);

	return count;
}

static ssize_t  fts_dumval_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct  fts_data * fts = dev_get_drvdata(dev);
	unsigned long dumval;
	int error;

	mutex_lock(&fts->input_dev->mutex);
	error = strict_strtoul(buf, 0, &dumval);
	if (!error)
		 fts->dumval = (int)dumval;
	mutex_unlock(&fts->input_dev->mutex);

	return error ? : count;
}

static ssize_t fts_updatefw_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fts_data *fts = dev_get_drvdata(dev);
	int count;
	u8 version;

	mutex_lock(&fts->input_dev->mutex);
	version = fts_get_firmware_info(fts, FW_VERSION_INFO);
	count = sprintf(buf, "%d\n", (int)version);
	mutex_unlock(&fts->input_dev->mutex);

	return count;
}

static ssize_t  fts_updatefw_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct  fts_data * fts = dev_get_drvdata(dev);
	unsigned long val;
	int error;

	mutex_lock(&fts->input_dev->mutex);
	error = strict_strtoul(buf, 0, &val);
	if (!error) {
		if (val == 1) {
			dev_info(dev, "Start to update firmware\n");
			fts_check_upgrade_fw_cfg(fts, 0, 0, true);
			dev_info(dev, "End to update firmware\n");
			error = fts_init(fts, true);
		}
	}
	mutex_unlock(&fts->input_dev->mutex);

	return error ? : count;
}

static ssize_t fts_hover_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fts_data *fts = dev_get_drvdata(dev);
	int count;

	mutex_lock(&fts->input_dev->mutex);
	count = sprintf(buf, "%d\n", (int)fts->hover_on);
	mutex_unlock(&fts->input_dev->mutex);

	return count;
}

static ssize_t  fts_hover_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct  fts_data * fts = dev_get_drvdata(dev);
	unsigned long val;
	int error;

	mutex_lock(&fts->input_dev->mutex);
	error = strict_strtoul(buf, 0, &val);
	if (!error)
		fts_sense_control(fts, (int)val);

	mutex_unlock(&fts->input_dev->mutex);
	return error ? : count;
}

static ssize_t fts_auto_tune_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fts_data *fts = dev_get_drvdata(dev);
	int count;
	u8 afe_info_in_config, afe_info_in_cx;

	mutex_lock(&fts->input_dev->mutex);
	afe_info_in_config = fts_get_firmware_info(fts, AFE_INFO_IN_CONFIG);
	afe_info_in_cx = fts_get_firmware_info(fts, AFE_INFO_IN_CX);
	count = sprintf(buf, "%d\n",
			(int)(afe_info_in_config == afe_info_in_cx));
	mutex_unlock(&fts->input_dev->mutex);

	return count;
}

static ssize_t  fts_auto_tune_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct  fts_data * fts = dev_get_drvdata(dev);
	unsigned long val;
	int error;

	mutex_lock(&fts->input_dev->mutex);
	error = strict_strtoul(buf, 0, &val);
	if (!error) {
		if (val == 1) {
			dev_info(dev, "Start to do auto tuning\n");
			error = fts_init(fts, true);
			dev_info(dev, "End auto tuning\n");
		}
	}
	mutex_unlock(&fts->input_dev->mutex);

	return error ? : count;
}

static int fts_do_self_test(struct fts_data *fts)
{
	int cur_error;
	int error;
	int i,j;
	int left_msg;
	int time_out = 100;
	unsigned char reg = READ_ONE_EVENT;
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char reg_360[4] =
		{0xB0, 0x03, 0x60, 0xFE};
	unsigned char pass_result [] =
		{0x0F, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	unsigned char pre_list [] =
		{SYS_RESET, FLUSHBUFFER, SLEEPOUT};

	cur_error = fts_interrupt_set(fts, INT_DISABLE);
	if (cur_error) {
		dev_err(fts->dev, "Failed to disable interrupt!\n");
		return cur_error;
	}

	for (i = 0; i < sizeof(pre_list); i++) {
		cur_error = fts_send_block(fts, &pre_list[i], 1);
		if (cur_error) {
			dev_err(fts->dev, "Failed to do cmd 0x%x\n", pre_list[i]);
			goto end;
		}
		msleep(10);
	}

	cur_error = fts_send_block(fts, &reg_360[0], 4);
	if (cur_error) {
		dev_err(fts->dev, "Failed to write 0x360 register!\n");
		goto end;
	}
	msleep(10);

	cur_error = fts_command(fts, ITO_TEST);
	if (cur_error) {
		dev_err(fts->dev, "Failed to do ito test!\n");
		goto end;
	}

	j = 0;
	while(1) {
		cur_error = fts_recv_block(fts, &reg, 1, data, FTS_EVENT_SIZE);
		if (cur_error) {
			dev_err(fts->dev, "Failed to receive one event!\n");
			goto end;
		}
		if (data[0] == 0x0F && data[1] == 0x05)
			break;
		msleep(10);
		if (j++ > time_out) {
			dev_err(fts->dev, "Wait self-test timeout !\n");
			cur_error = -ETIMEDOUT;
			goto end;
		}
	}

	left_msg = data[7] + 1;
	for (i = 0; i < left_msg; i++) {
		dev_info(fts->dev, "data = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
		if (memcmp(data, pass_result, sizeof(data) != 0)) {
			fts->test_result = 0;
			break;
		}

		cur_error = fts_recv_block(fts, &reg, 1, data, FTS_EVENT_SIZE);
		if (cur_error) {
			dev_err(fts->dev, "Failed to receive one event!\n");
			goto end;
		}
		msleep(10);
	}

	if (i == left_msg)
		fts->test_result = 1;
end:
	error = cur_error;
	gpio_set_value(fts->pdata->power_gpio, 0);
	msleep(20);
	gpio_set_value(fts->pdata->power_gpio, 1);
	cur_error = fts_init(fts, false);

	return error;
}

static ssize_t fts_self_test_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fts_data *fts = dev_get_drvdata(dev);
	int count;

	mutex_lock(&fts->input_dev->mutex);
	count = sprintf(buf, "%d\n", (int)fts->test_result);
	mutex_unlock(&fts->input_dev->mutex);

	return count;
}

static ssize_t  fts_self_test_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct  fts_data * fts = dev_get_drvdata(dev);
	unsigned long val;
	int error;

	mutex_lock(&fts->input_dev->mutex);
	error = strict_strtoul(buf, 0, &val);
	if (!error) {
		if (val == 1) {
			dev_info(dev, "Start to do self test\n");
			error = fts_do_self_test(fts);
			dev_info(dev, "End self test\n");
		}
	}
	mutex_unlock(&fts->input_dev->mutex);

	return error ? : count;
}

static int fts_sensitive_mode_switch(struct fts_data *fts, bool mode_on)
{
	int error;

	mutex_lock(&fts->input_dev->mutex);

	if (mode_on)
		error = fts_command(fts, MS_HOVER_ON);
	else
		error = fts_command(fts, MS_HOVER_OFF);

	if (error)
		goto end;

	error = fts_command(fts, FORCECALIBRATION);
	if (error)
		dev_err(fts->dev, "Failed to do force calibration!\n");

end:
	mutex_unlock(&fts->input_dev->mutex);
	return error;
}

static ssize_t fts_sensitive_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fts_data *fts = dev_get_drvdata(dev);
	int count;

	count = sprintf(buf, "%d\n", (int)fts->sensitive_mode);

	return count;
}

static ssize_t  fts_sensitive_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct  fts_data * fts = dev_get_drvdata(dev);
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 0, &val);
	if (!error) {
		if (val == 1) {
			error = fts_sensitive_mode_switch(fts, true);
			if (error)
				dev_err(dev, "Failed to open sensitive mode!\n");
			fts->sensitive_mode = 1;
		} else if (val == 0) {
			error = fts_sensitive_mode_switch(fts, false);
			if (error)
				dev_err(dev, "Failed to close sensitive mode!\n");
			fts->sensitive_mode = 0;
		}
	}

	return error ? : count;
}

static ssize_t  fts_chip_reset_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct  fts_data * fts = dev_get_drvdata(dev);
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 0, &val);
	if (!error) {
		if (val == 1) {
			gpio_set_value(fts->pdata->power_gpio, 0);
			msleep(20);
			gpio_set_value(fts->pdata->power_gpio, 1);
			error = fts_init(fts, false);
		}
	}

	return error ? : count;
}

static void fts_get_noise_level(struct  fts_data * fts, unsigned char freq, int count)
{
	int error, i;
	unsigned char regAdd[5] = {0xB0, 0x07, 0x9E, 0x00, 0x00};
	unsigned char buf[6] = {0xB3, 0x00, 0x10, 0xB1, 0x31, 0x76};
	unsigned char nlevel[4] = {0};
	unsigned int total = 0;

	fts_interrupt_set(fts, INT_DISABLE);
	error = fts_command(fts, SLEEPIN);
	if (error) {
		dev_err(fts->dev, "Failed in sleepin!\n");
		goto end;
	}
	regAdd[3] = freq;

	error = fts_send_block(fts, regAdd, sizeof(regAdd));
	if (error) {
		dev_err(fts->dev, "Failed in wrting T cycle!\n");
		goto end;
	}

	error = fts_command(fts, SLEEPOUT);
	if (error) {
		dev_err(fts->dev, "Failed to sleep out!\n");
		goto end;
	}

	error = fts_command(fts, CX_TUNNING);
	if (error) {
		dev_err(fts->dev, "Failed to do mut-tuning!\n");
		goto end;
	}

	error = fts_command(fts, SELF_TUNING);
	if (error) {
		dev_err(fts->dev, "Failed to do self-tuning!\n");
		goto end;
	}

	error = fts_command(fts, SENSEON);
	if (error) {
		dev_err(fts->dev, "Failed to set sense on!\n");
		goto end;
	}

	error = fts_command(fts, FORCECALIBRATION);
	if (error) {
		dev_err(fts->dev, "Failed to do force calib!\n");
		goto end;
	}

	msleep(2000);

	for (i = 0; i < count; i++) {
		error = fts_send_block(fts, buf, 3);
		if (error) {
			dev_err(fts->dev, "Failed to send block!\n");
			goto end;
		}

		error = fts_recv_block(fts, &buf[3], 3, nlevel, 2);
		if (error) {
			dev_err(fts->dev, "Failed in receive block!\n");
			goto end;
		}
		total += (int)nlevel[1];
		dev_info(fts->dev, "nlevel = 0x%x, 0x%x, 0x%x, 0x%x\n", nlevel[0], nlevel[1], nlevel[2], nlevel[3]);
	}

	total /= count;
	dev_info(fts->dev, "average = 0x%x\n", (unsigned char)total);

end:
	fts_interrupt_set(fts, INT_ENABLE);
}

static ssize_t  fts_noise_level_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct  fts_data * fts = dev_get_drvdata(dev);
	int error;
	unsigned char freq;
	int cnt;

	if (sscanf(buf, "%hhu:%d", &freq, &cnt) == 2)
		fts_get_noise_level(fts, freq, cnt);
	else
		error = -EINVAL;

	return error ? : count;
}

static int fts_input_event(struct input_dev *dev,
		unsigned int type, unsigned int code, int value)
{
	struct  fts_data * fts = input_get_drvdata(dev);
	char buffer[16];

	if (type == EV_SYN && code == SYN_CONFIG) {
		if (fts->dbgdump) {
			dev_info(fts->dev,
				"event write value = %d \n", value);
		}
		sprintf(buffer, "%d", value);
		if (value == FTS_INPUT_EVENT_SENSITIVE_MODE_ON ||
			value == FTS_INPUT_EVENT_SENSITIVE_MODE_OFF) {
			fts->sensitive_mode = (u8)value;
			schedule_work(&fts->switch_mode_work);
		}
	}

	return 0;
}

static void fts_switch_mode_work(struct work_struct *work)
{
	struct  fts_data *fts = container_of(work, struct fts_data, switch_mode_work);

	fts_sensitive_mode_switch(fts, fts->sensitive_mode == FTS_INPUT_EVENT_SENSITIVE_MODE_ON);
}

static DEVICE_ATTR(dbgdump, 0644, fts_dbgdump_show, fts_dbgdump_store);
static DEVICE_ATTR(dumval, 0644, fts_dumval_show, fts_dumval_store);
static DEVICE_ATTR(updatefw, 0644, fts_updatefw_show, fts_updatefw_store);
static DEVICE_ATTR(hover, 0644, fts_hover_show, fts_hover_store);
static DEVICE_ATTR(auto_tune, 0644, fts_auto_tune_show, fts_auto_tune_store);
static DEVICE_ATTR(self_test, 0644, fts_self_test_show, fts_self_test_store);
static DEVICE_ATTR(sensitive_mode, 0644, fts_sensitive_mode_show, fts_sensitive_mode_store);
static DEVICE_ATTR(chip_reset, 0200, NULL, fts_chip_reset_store);
static DEVICE_ATTR(noise_level, 0200, NULL, fts_noise_level_store);

static struct attribute *fts_attrs[] = {
	&dev_attr_dbgdump.attr,
	&dev_attr_dumval.attr,
	&dev_attr_updatefw.attr,
	&dev_attr_hover.attr,
	&dev_attr_auto_tune.attr,
	&dev_attr_self_test.attr,
	&dev_attr_sensitive_mode.attr,
	&dev_attr_chip_reset.attr,
	&dev_attr_noise_level.attr,
	NULL
};

static const struct attribute_group fts_attr_group = {
	.attrs = fts_attrs
};

static int fts_power_supply_event(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct fts_data *fts = container_of(nb, struct fts_data, power_supply_notifier);

	mutex_lock(&fts->input_dev->mutex);

	if (fts->dbgdump)
		dev_info(fts->dev, "Power_supply_event\n");
	if (fts->enabled)
		fts_charger_state_changed(fts);
	else if (fts->dbgdump)
		dev_info(fts->dev, "Don't response to power supply event in suspend mode!\n");

	mutex_unlock(&fts->input_dev->mutex);
	return 0;
}

static void fts_dump_value(struct device *dev, struct fts_ts_platform_data *pdata)
{
	int i = 0;

	dev_info(dev, "reset gpio= %d\n", pdata->reset_gpio);
	dev_info(dev, "irq gpio= %d\n", pdata->irq_gpio);
	dev_info(dev, "power gpio= %d\n", pdata->power_gpio);
	dev_info(dev, "lcd gpio= %d\n", pdata->lcd_gpio);

	dev_info(dev, "x_min = %d\n", pdata->x_min);
	dev_info(dev, "x_max = %d\n", pdata->x_max);
	dev_info(dev, "y_min = %d\n", pdata->y_min);
	dev_info(dev, "y_max = %d\n", pdata->y_max);
	dev_info(dev, "z_min = %d\n", pdata->z_min);
	dev_info(dev, "z_max = %d\n", pdata->z_max);
	dev_info(dev, "z_prox = %d\n", pdata->prox_z);

	for (i = 0; i < pdata->key_num; i++)
		dev_info(dev, "key codes[%d] = %d\n", i, pdata->key_codes[i]);

	dev_info(dev, "is mutual key = %d\n", pdata->is_mutual_key);
	dev_info(dev, "touch info = %d\n", pdata->touch_info);

	for (i = 0; i < pdata->config_array_size; i++) {
		dev_info(dev, "lcd id = %d\n", pdata->config_info[i].lcd_id);
		dev_info(dev, "module id = %d\n", pdata->config_info[i].module_id);
		dev_info(dev, "config name = %s\n", pdata->config_info[i].config_name);
		dev_info(dev, "firmware name = %s\n", pdata->config_info[i].firmware_name);
	}
}

#ifdef CONFIG_OF
static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
	int ret;
	struct fts_config_info *info;
	struct device_node *temp, *np = dev->of_node;
	u32 temp_val;

	/* reset, irq, power gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "st,reset-gpio",
				0, &pdata->reset_gpio_flags);
	pdata->irq_gpio = of_get_named_gpio_flags(np, "st,irq-gpio",
				0, &pdata->irq_gpio_flags);
	pdata->power_gpio = of_get_named_gpio_flags(np, "st,power-gpio",
				0, &pdata->power_gpio_flags);
	pdata->lcd_gpio = of_get_named_gpio_flags(np, "st,lcd-gpio",
				0, &pdata->lcd_gpio_flags);
	ret = of_property_read_u32(np, "st,irqflags", &temp_val);
	if (ret) {
		dev_err(dev, "Unable to read irqflags id\n");
		return ret;
	} else
		pdata->irqflags = temp_val;

	ret = of_property_read_u32(np, "st,x-max", &pdata->x_max);
	if (ret)
		dev_err(dev, "Unable to read max-X\n");

	ret = of_property_read_u32(np, "st,x-min", &pdata->x_min);
	if (ret)
		dev_err(dev, "Unable to read min-X\n");

	ret = of_property_read_u32(np, "st,y-max", &pdata->y_max);
	if (ret)
		dev_err(dev, "Unable to read max-Y\n");

	ret = of_property_read_u32(np, "st,y-min", &pdata->y_min);
	if (ret)
		dev_err(dev, "Unable to read min-Y\n");

	ret = of_property_read_u32(np, "st,z-max", &pdata->z_max);
	if (ret)
		dev_err(dev, "Unable to read max-Z\n");

	ret = of_property_read_u32(np, "st,z-min", &pdata->z_min);
	if (ret)
		dev_err(dev, "Unable to read min-Z\n");

	ret = of_property_read_u32(np, "st,prox-z", &pdata->prox_z);
	if (ret)
		dev_err(dev, "Unable to read prox-Z\n");

	ret = of_property_read_u32(np, "st,key-num", &pdata->key_num);
	if (ret)
		dev_err(dev, "Unable to read key num\n");

	if (pdata->key_num != 0) {
		pdata->key_codes = devm_kzalloc(dev,
						sizeof(int) * pdata->key_num, GFP_KERNEL);
		if (!pdata->key_codes)
			return -ENOMEM;

		ret = of_property_read_u32_array(np, "st,key-codes",
							pdata->key_codes, pdata->key_num);
		if (ret) {
			dev_err(dev, "Unable to read key codes\n");
			return ret;
		}
	}

	ret = of_property_read_u32(np, "st,is-mutual-key", &temp_val);
	if (ret)
		dev_err(dev, "Unable to read is-mutual-key\n");
	else
		pdata->is_mutual_key = (bool)temp_val;

	ret = of_property_read_u32(np, "st,touch-info", &pdata->touch_info);
	if (ret)
		dev_err(dev, "Unable to read touch info\n");

	ret = of_property_read_u32(np, "st,config-array-size", &pdata->config_array_size);
	if (ret) {
		dev_err(dev, "Unable to get array size\n");
		return ret;
	}

	pdata->config_info = devm_kzalloc(dev, pdata->config_array_size *
					sizeof(struct fts_config_info), GFP_KERNEL);
	if (!pdata->config_info) {
		dev_err(dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	info = pdata->config_info;

	for_each_child_of_node(np, temp) {
		ret = of_property_read_u32(temp, "st,lcd-id", &info->lcd_id);
		if (ret) {
			dev_err(dev, "Unable to get lcd id\n");
			return ret;
		}

		ret = of_property_read_u32(temp, "st,module-id", &info->module_id);
		if (ret) {
			dev_err(dev, "Unable to get module id\n");
			return ret;
		}

		ret = of_property_read_string(temp, "st,config-name", &info->config_name);
		if (ret && (ret != -EINVAL)) {
			dev_err(dev, "Unable to read config name\n");
			return ret;
		}

		ret = of_property_read_string(temp, "st,firmware-name", &info->firmware_name);
		if (ret && (ret != -EINVAL)) {
			dev_err(dev, "Unable to read firmware name\n");
			return ret;
		}

		info++;
	}

	fts_dump_value(dev, pdata);

	return 0;
}
#else
static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
	return -ENODEV;
}
#endif

struct fts_data *fts_probe(struct device *dev,
				const struct fts_bus_ops *bops, int irq)
{
	int error;
	struct fts_data *fts;
	struct fts_ts_platform_data *pdata;
	int i;

	dev_info(dev, "FTS driver [12%s] %s %s\n",
		FTS_TS_DRV_VERSION, __DATE__, __TIME__);
	dev_info(dev, "FTS use protocol type B\n");

	/* check input argument */
	if (dev->of_node) {
		pdata = devm_kzalloc(dev,
			sizeof(struct fts_ts_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(dev, "Failed to allocate memory\n");
			error = -ENOMEM;
			goto err;
		}

		error = fts_parse_dt(dev, pdata);
		if (error)
			goto err;
		dev->platform_data = pdata;
	} else
		pdata = dev->platform_data;

	if (pdata == NULL) {
		dev_err(dev, "platform data doesn't exist\n");
		error = -EINVAL;
		goto err;
	}

	/* init platform stuff */
	if (gpio_is_valid(pdata->power_gpio)) {
		error = gpio_request(pdata->power_gpio, "fts_power_gpio");
		if (error < 0) {
			dev_err(dev, "power gpio request failed");
			goto err;
		}
		error = gpio_direction_output(pdata->power_gpio, 1);
		if (error < 0) {
			dev_err(dev, "set_direction for power gpio failed\n");
			goto free_power_gpio;
		}
	}

	if (gpio_is_valid(pdata->irq_gpio)) {
		error = gpio_request(pdata->irq_gpio, "fts_irq_gpio");
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
		error = gpio_request(pdata->reset_gpio, "fts_reset_gpio");
		if (error < 0) {
			dev_err(dev, "irq gpio request failed");
			goto free_irq_gpio;
		}
		error = gpio_direction_output(pdata->reset_gpio, 1);
		if (error < 0) {
			dev_err(dev, "set_direction for irq gpio failed\n");
			goto free_reset_gpio;
		}
	}

	/* alloc and init data object */
	fts = kzalloc(sizeof(struct fts_data), GFP_KERNEL);
	if (fts == NULL) {
		dev_err(dev, "fail to allocate data object\n");
		error = -ENOMEM;
		goto free_reset_gpio;
	}

	fts->dev  = dev;
	fts->irq  = irq;
	fts->bops = bops;
	fts->pdata = pdata;

	for (i = 0; i < FINGER_MAX; i++)
		fts->ID_Indx[i] = 0;

	fts->touch_is_pressed = 0;
	fts->enabled = false;

	mutex_init(&fts->lock);

	mutex_lock(&fts->lock);
	error = fts_init(fts, false);
	mutex_unlock(&fts->lock);
	if (error) {
		dev_err(dev, "FTS fts_init fail!!!\n");
		goto err_free_data;
	}

	fts->enabled = true;

	/* alloc and init input device */
	fts->input_dev = input_allocate_device();
	if (fts->input_dev == NULL) {
		dev_err(dev, "fail to allocate input device\n");
		error = -ENOMEM;
		goto err_free_data;
	}

	input_set_drvdata(fts->input_dev, fts);
	fts->input_dev->name       = FTS_TS_DRV_NAME;
	fts->input_dev->id.bustype = bops->bustype;
	fts->input_dev->id.vendor  = 0x0001; /* ST  */
	fts->input_dev->id.product = 0x0002; /* fts  */
	fts->input_dev->id.version = 0x0100; /* 1.0 */
	fts->input_dev->dev.parent = dev;
	fts->input_dev->enable = fts_input_enable;
	fts->input_dev->disable = fts_input_disable;
	fts->input_dev->event = fts_input_event;

	/* init touch parameter */
	input_mt_init_slots(fts->input_dev, FINGER_MAX);
	set_bit(ABS_MT_TOUCH_MAJOR, fts->input_dev->absbit);
	set_bit(ABS_MT_POSITION_X, fts->input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, fts->input_dev->absbit);
	set_bit(ABS_MT_PRESSURE, fts->input_dev->absbit);
	set_bit(INPUT_PROP_DIRECT, fts->input_dev->propbit);

	input_set_abs_params(fts->input_dev,
			     ABS_MT_POSITION_X, 0, pdata->x_max, 0, 0);
	input_set_abs_params(fts->input_dev,
			     ABS_MT_POSITION_Y, 0, pdata->y_max, 0, 0);
	input_set_abs_params(fts->input_dev,
			     ABS_MT_TOUCH_MAJOR, 0, pdata->z_max, 0, 0);
	input_set_abs_params(fts->input_dev,
			     ABS_MT_PRESSURE, 0, pdata->z_max, 0, 0);
	input_set_abs_params(fts->input_dev,
			     ABS_MT_TRACKING_ID, 0, 10, 0, 0);
	input_set_abs_params(fts->input_dev,
			     ABS_MT_DISTANCE, 0, 1, 0, 0);

	set_bit(EV_KEY, fts->input_dev->evbit);
	set_bit(EV_ABS, fts->input_dev->evbit);

	if (fts->pdata->key_codes) {
		for (i = 0; i < fts->pdata->key_num; i++) {
			if (fts->pdata->key_codes[i])
				input_set_capability(fts->input_dev, EV_KEY,
							fts->pdata->key_codes[i]);
		}
	}

	/* register input device */
	error = input_register_device(fts->input_dev);
	if (error) {
		dev_err(dev, "fail to register input device\n");
		goto err_free_input;
	}

	/* start interrupt process */
	error = request_threaded_irq(fts->irq, NULL, fts_interrupt,
				pdata->irqflags, "fts", fts);
	if (error) {
		dev_err(dev, "fail to request interrupt\n");
		goto err_unregister_input;
	}

	error = sysfs_create_group(&dev->kobj, &fts_attr_group);
	if (error) {
		dev_err(dev, "fail to export sysfs entires\n");
	}

	fts->power_supply_notifier.notifier_call = fts_power_supply_event;
	register_power_supply_notifier(&fts->power_supply_notifier);

	INIT_WORK(&fts->switch_mode_work, fts_switch_mode_work);

	return fts;


err_unregister_input:
	input_unregister_device(fts->input_dev);
	fts->input_dev = NULL;
err_free_input:
	input_free_device(fts->input_dev);
err_free_data:
	kfree(fts);
free_reset_gpio:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
free_irq_gpio:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_power:
	gpio_set_value(pdata->power_gpio, 0);
free_power_gpio:
	if (gpio_is_valid(pdata->power_gpio)) {
		gpio_free(pdata->power_gpio);
	}
err:
	return ERR_PTR(error);
}
EXPORT_SYMBOL_GPL(fts_probe);

void fts_remove(struct fts_data *fts)
{
	int ret;

	ret = fts_interrupt_set(fts, INT_DISABLE);
	if (ret)
		return;

	ret = fts_command(fts, FLUSHBUFFER);
	if (ret)
		return;

	cancel_work_sync(&fts->switch_mode_work);
	unregister_power_supply_notifier(&fts->power_supply_notifier);
	input_unregister_device(fts->input_dev);
	kfree(fts);
}
EXPORT_SYMBOL_GPL(fts_remove);

MODULE_AUTHOR("Zhang Bo <zhangbo_a@xiaomi.com>");
MODULE_DESCRIPTION("fts touchscreen input driver");
MODULE_LICENSE("GPL");
