/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2016 XiaoMi, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/stat.h>
#include <asm/unaligned.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/err.h>

#include "ist30xxc.h"
#include "ist30xxc_update.h"
#include "ist30xxc_tracking.h"

#if IST30XX_INTERNAL_BIN
#include "./firmware/IST3038C1_FW_A12_BOE.h"
#if (IMAGIS_TSP_IC < IMAGIS_IST3038C)
#if IST30XX_MULTIPLE_TSP
#endif
#endif
#endif

#define TAGS_PARSE_OK		(0)

int ist30xx_isp_read_burst(struct i2c_client *client, u32 addr,
			   u32 *buf32, u16 len)
{
	int ret = 0;
	int i;
	u16 max_len = I2C_MAX_READ_SIZE / IST30XX_DATA_LEN;
	u16 remain_len = len;

	for (i = 0; i < len; i += max_len) {
		if (remain_len < max_len)
			max_len = remain_len;
		ret = ist30xx_read_buf(client, addr, buf32, max_len);
		if (unlikely(ret)) {
			tsp_err("Burst fail, addr: %x\n", __func__, addr);
			return ret;
		}

		buf32 += max_len;
		remain_len -= max_len;
	}

	return 0;
}

int ist30xx_isp_write_burst(struct i2c_client *client, u32 addr,
			    u32 *buf32, u16 len)
{
	int ret = 0;
	int i;
	u16 max_len = I2C_MAX_WRITE_SIZE / IST30XX_DATA_LEN;
	u16 remain_len = len;

	for (i = 0; i < len; i += max_len) {
		if (remain_len < max_len)
			max_len = remain_len;
		ret = ist30xx_write_buf(client, addr, buf32, max_len);
		if (unlikely(ret)) {
			tsp_err("Burst fail, addr: %x\n", addr);
			return ret;
		}

		buf32 += max_len;
		remain_len -= max_len;
	}

	return 0;
}

#define IST30XX_ISP_READ_TOTAL_S    (0x01)
#define IST30XX_ISP_READ_TOTAL_B    (0x11)
#define IST30XX_ISP_READ_MAIN_S     (0x02)
#define IST30XX_ISP_READ_MAIN_B     (0x12)
#define IST30XX_ISP_READ_INFO_S     (0x03)
#define IST30XX_ISP_READ_INFO_B     (0x13)
#define IST30XX_ISP_PROG_TOTAL_S    (0x04)
#define IST30XX_ISP_PROG_TOTAL_B    (0x14)
#define IST30XX_ISP_PROG_MAIN_S     (0x05)
#define IST30XX_ISP_PROG_MAIN_B     (0x15)
#define IST30XX_ISP_PROG_INFO_S     (0x06)
#define IST30XX_ISP_PROG_INFO_B     (0x16)
#define IST30XX_ISP_ERASE_BLOCK     (0x07)
#define IST30XX_ISP_ERASE_SECTOR    (0x08)
#define IST30XX_ISP_ERASE_PAGE      (0x09)
#define IST30XX_ISP_ERASE_INFO      (0x0A)
#define IST30XX_ISP_READ_TOTAL_CRC  (0x1B)
#define IST30XX_ISP_READ_MAIN_CRC   (0x1C)
#define IST30XX_ISP_READ_INFO_CRC   (0x1D)
int ist30xxc_isp_enable(struct ist30xx_data *data, bool enable)
{
	int ret = 0;

	if (enable) {
		ret = ist30xx_write_cmd(data, IST30XX_FLASH_ISPEN, 0xDE01);
		if (unlikely(ret))
			return ret;

#if (IMAGIS_TSP_IC < IMAGIS_IST3038C)
		ret = ist30xx_write_cmd(data, IST30XX_FLASH_MODE, 0x4100);
		if (unlikely(ret))
			return ret;

		ret = ist30xx_write_cmd(data, IST30XX_FLASH_TEST_MODE1, 0x38);
		if (unlikely(ret))
			return ret;
#endif
	} else {
		ret = ist30xx_write_cmd(data, IST30XX_FLASH_ISPEN, 0x2);
		if (unlikely(ret))
			return ret;
	}

	msleep(1);

	return ret;
}

int ist30xxc_isp_mode(struct ist30xx_data *data, int mode)
{
	int ret = 0;
	u32 val = 0;

	switch (mode) {
	case IST30XX_ISP_READ_TOTAL_S:
		val = 0x8090;
		break;
	case IST30XX_ISP_READ_TOTAL_B:
	case IST30XX_ISP_READ_TOTAL_CRC:
		val = 0x8190;
		break;
	case IST30XX_ISP_READ_MAIN_S:
		val = 0x0090;
		break;
	case IST30XX_ISP_READ_MAIN_B:
	case IST30XX_ISP_READ_MAIN_CRC:
		val = 0x0190;
		break;
	case IST30XX_ISP_READ_INFO_S:
		val = 0x0098;
		break;
	case IST30XX_ISP_READ_INFO_B:
	case IST30XX_ISP_READ_INFO_CRC:
		val = 0x0198;
		break;
	case IST30XX_ISP_PROG_TOTAL_S:
		val = 0x8050;
		break;
	case IST30XX_ISP_PROG_TOTAL_B:
		val = 0x8150;
		break;
	case IST30XX_ISP_PROG_MAIN_S:
		val = 0x0050;
		break;
	case IST30XX_ISP_PROG_MAIN_B:
		val = 0x0150;
		break;
	case IST30XX_ISP_PROG_INFO_S:
		val = 0x0058;
		break;
	case IST30XX_ISP_PROG_INFO_B:
		val = 0x0158;
		break;
	case IST30XX_ISP_ERASE_BLOCK:
		val = 0x0031;
		break;
	case IST30XX_ISP_ERASE_SECTOR:
		val = 0x0032;
		break;
	case IST30XX_ISP_ERASE_PAGE:
		val = 0x0030;
		break;
	case IST30XX_ISP_ERASE_INFO:
		val = 0x0038;
		break;
	default:
		tsp_err("ISP fail, unknown mode\n");
		return -EINVAL;
	}

#if (IMAGIS_TSP_IC > IMAGIS_IST3032C)
	val &= ~(0x8000);
#endif

	ret = ist30xx_write_cmd(data, IST30XX_FLASH_MODE, val);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XX_FLASH_MODE\n");
		return ret;
	}

	return 0;
}

int ist30xxc_isp_erase(struct ist30xx_data *data, int mode, u32 index)
{
	int ret = 0;

	tsp_info("%s\n", __func__);

	ret = ist30xxc_isp_mode(data, mode);
	if (unlikely(ret))
		return ret;

	ret = ist30xx_write_cmd(data, IST30XX_FLASH_DIN, index);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XX_FLASH_DIN\n");
		return ret;
	}

	msleep(50);

	return ret;
}

int ist30xxc_isp_program(struct ist30xx_data *data, u32 addr, int mode,
			 const u32 *buf32, int len)
{
	int ret = 0;

	tsp_info("%s\n", __func__);

	ret = ist30xxc_isp_mode(data, mode);
	if (unlikely(ret))
		return ret;

	ret = ist30xx_write_cmd(data, IST30XX_FLASH_ADDR, addr);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XX_FLASH_ADDR\n");
		return ret;
	}

	if (mode & 0x10)
		ret = ist30xx_isp_write_burst(data->client, IST30XX_FLASH_DIN,
					      (u32 *) buf32, len);
	else
		ret =
		    ist30xx_write_buf(data->client, IST30XX_FLASH_DIN,
				      (u32 *) buf32, len);

	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XX_FLASH_DIN\n");
		return ret;
	}

	return ret;
}

int ist30xxc_isp_read(struct ist30xx_data *data, u32 addr, int mode,
		      u32 *buf32, int len)
{
	int ret = 0;

	/* IST30xxB ISP read mode */
	ret = ist30xxc_isp_mode(data, mode);
	if (unlikely(ret))
		return ret;

	ret = ist30xx_write_cmd(data, IST30XX_FLASH_ADDR, addr);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XX_FLASH_ADDR\n");
		return ret;
	}

	if (mode & 0x10)
		ret =
		    ist30xx_isp_read_burst(data->client, IST30XX_FLASH_DOUT,
					   buf32, len);
	else
		ret =
		    ist30xx_read_buf(data->client, IST30XX_FLASH_DOUT, buf32,
				     len);

	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XX_FLASH_DOUT\n");
		return ret;
	}

	return 0;
}

int ist30xxc_cmd_read_chksum(struct ist30xx_data *data, int mode,
			     u32 start_addr, u32 end_addr, u32 *chksum)
{
	int ret = 0;
	u32 val = (1 << 28) | (1 << 25) | (1 << 24) | (1 << 20) | (1 << 16);

	val |= (end_addr / IST30XX_ADDR_LEN) - 1;

	ret = ist30xxc_isp_mode(data, mode);
	if (unlikely(ret))
		return ret;

	ret = ist30xx_write_cmd(data, IST30XX_FLASH_ADDR, start_addr);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XX_FLASH_ADDR (%x)\n", val);
		return ret;
	}

	ret = ist30xx_write_cmd(data, IST30XX_FLASH_AUTO_READ, val);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XX_FLASH_AUTO_READ (%x)\n", val);
		return ret;
	}

	msleep(100);

	ret = ist30xx_read_reg(data->client, IST30XX_FLASH_CRC, chksum);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XX_FLASH_CRC (%x)\n", chksum);
		return ret;
	}

	return 0;
}

int ist30xxc_read_chksum(struct ist30xx_data *data, u32 *chksum)
{
	int ret = 0;
	u32 start_addr, end_addr;

	start_addr = data->tags.fw_addr;
	end_addr = data->tags.sensor_addr + data->tags.sensor_size;
	ret =
	    ist30xxc_cmd_read_chksum(data, IST30XX_ISP_READ_TOTAL_CRC,
				     start_addr, end_addr, chksum);
	if (unlikely(ret))
		return ret;

	tsp_info("chksum: %x(%x~%x)\n", *chksum, start_addr, end_addr);

	return 0;
}

int ist30xxc_read_chksum_all(struct ist30xx_data *data, u32 *chksum)
{
	int ret = 0;
	u32 start_addr, end_addr;

	start_addr = IST30XX_FLASH_BASE_ADDR;
	end_addr = IST30XX_FLASH_BASE_ADDR + IST30XX_FLASH_TOTAL_SIZE;
	ret =
	    ist30xxc_cmd_read_chksum(data, IST30XX_ISP_READ_TOTAL_CRC,
				     start_addr, end_addr, chksum);
	if (unlikely(ret))
		return ret;

	tsp_info("chksum: %x(%x~%x)\n", *chksum, start_addr, end_addr);

	return 0;
}

int ist30xxc_isp_info_read(struct ist30xx_data *data, u32 addr, u32 *buf32,
			   u32 len)
{
	int ret = 0;
	int retry = IST30XX_MAX_RETRY_CNT;

isp_info_read_retry:
	if (retry-- == 0)
		goto isp_info_read_end;

	ist30xx_reset(data, true);

	/* IST30xxB ISP enable */
	ret = ist30xxc_isp_enable(data, true);
	if (unlikely(ret))
		goto isp_info_read_retry;

	ret = ist30xxc_isp_read(data, addr, IST30XX_ISP_READ_INFO_B,
				buf32, len);
	if (unlikely(ret))
		goto isp_info_read_retry;

isp_info_read_end:
	/* IST30xxC ISP disable */
	ist30xxc_isp_enable(data, false);
	ist30xx_reset(data, false);
	return ret;
}

int ist30xxc_isp_fw_read(struct ist30xx_data *data, u32 *buf32)
{
	int ret = 0;
	int i;
	int len;
	u32 addr = IST30XX_FLASH_BASE_ADDR;

	ist30xx_reset(data, true);

	/* IST30xxB ISP enable */
	ret = ist30xxc_isp_enable(data, true);
	if (unlikely(ret))
		return ret;

#if I2C_BURST_MODE
	for (i = 0; i < IST30XX_FLASH_TOTAL_SIZE; i += I2C_MAX_READ_SIZE) {
		len = I2C_MAX_READ_SIZE / IST30XX_DATA_LEN;
		if ((IST30XX_FLASH_TOTAL_SIZE - i) < I2C_MAX_READ_SIZE)
			len = (IST30XX_FLASH_TOTAL_SIZE - i) / IST30XX_DATA_LEN;

		ret = ist30xxc_isp_read(data, addr, IST30XX_ISP_READ_TOTAL_B,
					buf32, len);
		if (unlikely(ret))
			goto isp_fw_read_end;

		addr += len;
		buf32 += len;
	}
#else
	for (i = 0; i < IST30XX_FLASH_TOTAL_SIZE; i += IST30XX_DATA_LEN) {
		ret = ist30xxc_isp_read(data, addr, IST30XX_ISP_READ_TOTAL_S,
					buf32, 1);
		if (unlikely(ret))
			goto isp_fw_read_end;

		addr++;
		buf32++;
	}
#endif
isp_fw_read_end:
	/* IST30xxC ISP disable */
	ist30xxc_isp_enable(data, false);
	ist30xx_reset(data, false);
	return ret;
}

int ist30xxc_isp_fw_update(struct ist30xx_data *data, const u8 *buf)
{
#if !(I2C_BURST_MODE)
	int i;
#endif
	int ret = 0;
	u32 addr = IST30XX_FLASH_BASE_ADDR;

	tsp_info("%s\n", __func__);

	ist30xx_reset(data, true);

	/* IST30xxC ISP enable */
	ret = ist30xxc_isp_enable(data, true);
	if (unlikely(ret))
		goto isp_fw_update_end;

	/* IST30xxC ISP erase */
	ret = ist30xxc_isp_erase(data, IST30XX_ISP_ERASE_BLOCK, 0);
	if (unlikely(ret))
		goto isp_fw_update_end;

#if (IST30XX_FLASH_INFO_SIZE > 0)
	ret = ist30xxc_isp_erase(data, IST30XX_ISP_ERASE_INFO, 0);
	if (unlikely(ret))
		goto isp_fw_update_end;
#endif

	ist30xx_reset(data, true);

	/* IST30xxC ISP enable */
	ret = ist30xxc_isp_enable(data, true);
	if (unlikely(ret))
		goto isp_fw_update_end;

#if I2C_BURST_MODE
	/* IST30xxC ISP write burst */
	ret =
	    ist30xxc_isp_program(data, addr, IST30XX_ISP_PROG_TOTAL_B,
				 (u32 *) buf,
				 IST30XX_FLASH_TOTAL_SIZE / IST30XX_DATA_LEN);
	if (unlikely(ret))
		goto isp_fw_update_end;
#else
	/* IST30xxC ISP write single */
	for (i = 0; i < IST30XX_FLASH_TOTAL_SIZE; i += IST30XX_DATA_LEN) {
		ret = ist30xxc_isp_program(data, addr, IST30XX_ISP_PROG_TOTAL_S,
					   (u32 *) buf, 1);
		if (unlikely(ret))
			goto isp_fw_update_end;

		addr++;
		buf += IST30XX_DATA_LEN;
	}
#endif

isp_fw_update_end:
	/* IST30xxC ISP disable */
	ist30xxc_isp_enable(data, false);
	ist30xx_reset(data, false);
	return ret;
}

u32 ist30xx_parse_ver(struct ist30xx_data *data, int flag, const u8 *buf)
{
	u32 ver = 0;
	u32 *buf32 = (u32 *) buf;

	if (flag == FLAG_MAIN)
		ver = (u32) buf32[(data->tags.flag_addr + 0x1FC) >> 2];
	else if (flag == FLAG_TEST)
		ver = (u32) buf32[(data->tags.flag_addr + 0x1F4) >> 2];
	else if (flag == FLAG_FW)
		ver = (u32) (buf32[(data->tags.cfg_addr + 0x4) >> 2] & 0xFFFF);
	else if (flag == FLAG_CORE)
		ver = (u32) buf32[(data->tags.flag_addr + 0x1F0) >> 2];
	else
		tsp_warn("Parsing ver's flag is not corrent!\n");

	return ver;
}

int calib_ms_delay = CALIB_WAIT_TIME;
int ist30xx_calib_wait(struct ist30xx_data *data)
{
	int cnt = calib_ms_delay;

	data->status.calib_msg = 0;
	while (cnt-- > 0) {
		msleep(100);

		if (data->status.calib_msg) {
			tsp_info
			    ("Calibration status : %d, Max raw gap : %d - (%08x)\n",
			     CALIB_TO_STATUS(data->status.calib_msg),
			     CALIB_TO_GAP(data->status.calib_msg),
			     data->status.calib_msg);
			if (CALIB_TO_STATUS(data->status.calib_msg) == 0)
				return 0;
			else
				return -EAGAIN;
		}
	}
	tsp_warn("Calibration time out\n");

	return -EPERM;
}

int ist30xx_calibrate(struct ist30xx_data *data, int wait_cnt)
{
	int ret = -ENOEXEC;

	tsp_info("*** Calibrate %ds ***\n", calib_ms_delay / 10);

	data->status.update = 1;
	ist30xx_disable_irq(data);
	while (1) {
		ret = ist30xx_cmd_calibrate(data);
		if (unlikely(ret))
			continue;

		ist30xx_enable_irq(data);
		ret = ist30xx_calib_wait(data);
		if (likely(!ret))
			break;

		ist30xx_disable_irq(data);

		if (--wait_cnt == 0)
			break;

		ist30xx_reset(data, false);
	}

	ist30xx_disable_irq(data);
	ist30xx_reset(data, false);
	data->status.update = 2;
	ist30xx_enable_irq(data);

	return ret;
}

int ist30xx_parse_tags(struct ist30xx_data *data, const u8 *buf,
		       const u32 size)
{
	int ret = -EPERM;
	struct ist30xx_tags *tags;

	tags =
	    (struct ist30xx_tags *)(&buf[size - sizeof(struct ist30xx_tags)]);

	if (!strncmp(tags->magic1, IST30XX_TAG_MAGIC, sizeof(tags->magic1))
	    && !strncmp(tags->magic2, IST30XX_TAG_MAGIC, sizeof(tags->magic2))) {
		data->tags = *tags;

		data->tags.fw_addr -= data->tags.rom_base;
		data->tags.cfg_addr -= data->tags.rom_base;
		data->tags.sensor_addr -= data->tags.rom_base;
		data->tags.cp_addr -= data->tags.rom_base;
		data->tags.flag_addr -= data->tags.rom_base;

		data->fw.index = data->tags.fw_addr;
		data->fw.size = tags->flag_addr - tags->fw_addr +
		    tags->flag_size;
		data->fw.chksum = tags->chksum;

		tsp_verb("Tagts magic1: %s, magic2: %s\n",
			 data->tags.magic1, data->tags.magic2);
		tsp_verb(" rom: %x\n", data->tags.rom_base);
		tsp_verb(" ram: %x\n", data->tags.ram_base);
		tsp_verb(" fw: %x(%x)\n", data->tags.fw_addr,
			 data->tags.fw_size);
		tsp_verb(" cfg: %x(%x)\n", data->tags.cfg_addr,
			 data->tags.cfg_size);
		tsp_verb(" sensor: %x(%x)\n", data->tags.sensor_addr,
			 data->tags.sensor_size);
		tsp_verb(" cp: %x(%x)\n", data->tags.cp_addr,
			 data->tags.cp_size);
		tsp_verb(" flag: %x(%x)\n", data->tags.flag_addr,
			 data->tags.flag_size);
		tsp_verb(" zvalue: %x\n", data->tags.zvalue_base);
		tsp_verb(" algo: %x\n", data->tags.algo_base);
		tsp_verb(" raw: %x\n", data->tags.raw_base);
		tsp_verb(" filter: %x\n", data->tags.filter_base);
		tsp_verb(" chksum: %x\n", data->tags.chksum);
		tsp_verb(" chksum_all: %x\n", data->tags.chksum_all);
		tsp_verb(" build time: %04d/%02d/%02d (%02d:%02d:%02d)\n",
			 data->tags.year, data->tags.month, data->tags.day,
			 data->tags.hour, data->tags.min, data->tags.sec);

		ret = 0;
	}

	return ret;
}

int ist30xx_get_update_info(struct ist30xx_data *data, const u8 *buf,
			    const u32 size)
{
	int ret;

	ret = ist30xx_parse_tags(data, buf, size);
	if (unlikely(ret != TAGS_PARSE_OK))
		tsp_warn("Cannot find tags of F/W\n");

	return ret;
}

#define TSP_INFO_SWAP_XY    (1 << 0)
#define TSP_INFO_FLIP_X     (1 << 1)
#define TSP_INFO_FLIP_Y     (1 << 2)
u32 ist30xx_info_cal_crc(u32 *buf)
{
	int i;
	u32 chksum32 = 0;

	for (i = 0; i < IST30XX_MAX_CMD_SIZE - 1; i++)
		chksum32 += *buf++;

	return chksum32;
}

int ist30xx_tsp_update_info(struct ist30xx_data *data)
{
	int ret = 0;
	u32 chksum;
	u32 info[IST30XX_MAX_CMD_SIZE];
	u32 tsp_lcd, tsp_swap, tsp_scr, tsp_gtx, tsp_ch;
	u32 tkey_info0, tkey_info1, tkey_info2;
	u32 finger_info, baseline, threshold;
	u32 debugging_info;
	TSP_INFO *tsp = &data->tsp_info;
#if IST30XX_USE_KEY
	TKEY_INFO *tkey = &data->tkey_info;
#endif

	ret = ist30xx_cmd_hold(data, 1);
	if (unlikely(ret))
		return ret;

	ret = ist30xx_burst_read(data->client,
				 IST30XX_DA_ADDR(eHCOM_GET_CHIP_ID), &info[0],
				 IST30XX_MAX_CMD_SIZE, true);
	if (unlikely(ret))
		return ret;

	ret = ist30xx_cmd_hold(data, 0);
	if (unlikely(ret)) {
		ist30xx_reset(data, false);
		return ret;
	}

	ret = ist30xx_read_cmd(data, IST30XX_REG_CHIPID, &data->chip_id);
	if (unlikely(ret))
		return ret;

	if ((info[IST30XX_CMD_VALUE(eHCOM_GET_CHIP_ID)] != data->chip_id) ||
	    (info[IST30XX_CMD_VALUE(eHCOM_GET_CHIP_ID)] == 0) ||
	    (info[IST30XX_CMD_VALUE(eHCOM_GET_CHIP_ID)] == 0xFFFF))
		return -EINVAL;

	chksum = ist30xx_info_cal_crc((u32 *) info);
	if (chksum != info[IST30XX_MAX_CMD_SIZE - 1]) {
		tsp_err("info checksum : %08X, %08X\n",
			chksum, info[IST30XX_MAX_CMD_SIZE - 1]);
		return -EINVAL;
	}

	tsp_lcd = info[IST30XX_CMD_VALUE(eHCOM_GET_LCD_INFO)];
	tsp_ch = info[IST30XX_CMD_VALUE(eHCOM_GET_TSP_INFO)];
	tkey_info0 = info[IST30XX_CMD_VALUE(eHCOM_GET_KEY_INFO_0)];
	tkey_info1 = info[IST30XX_CMD_VALUE(eHCOM_GET_KEY_INFO_1)];
	tkey_info2 = info[IST30XX_CMD_VALUE(eHCOM_GET_KEY_INFO_2)];
	tsp_scr = info[IST30XX_CMD_VALUE(eHCOM_GET_SCR_INFO)];
	tsp_gtx = info[IST30XX_CMD_VALUE(eHCOM_GET_GTX_INFO)];
	tsp_swap = info[IST30XX_CMD_VALUE(eHCOM_GET_SWAP_INFO)];
	finger_info = info[IST30XX_CMD_VALUE(eHCOM_GET_FINGER_INFO)];
	baseline = info[IST30XX_CMD_VALUE(eHCOM_GET_BASELINE)];
	threshold = info[IST30XX_CMD_VALUE(eHCOM_GET_TOUCH_TH)];
	debugging_info = info[IST30XX_CMD_VALUE(eHCOM_GET_DBG_INFO_BASE)];

	data->debugging_addr = debugging_info & 0x00FFFFFF;
	data->debugging_size = (debugging_info >> 24) & 0xFF;

	tsp->ch_num.rx = (tsp_ch >> 16) & 0xFFFF;
	tsp->ch_num.tx = tsp_ch & 0xFFFF;

	tsp->node.len = tsp->ch_num.tx * tsp->ch_num.rx;

	tsp->gtx.num = (tsp_gtx >> 24) & 0xFF;
	tsp->gtx.ch_num[0] = (tsp_gtx >> 16) & 0xFF;
	tsp->gtx.ch_num[1] = (tsp_gtx >> 8) & 0xFF;
	tsp->gtx.ch_num[2] = 0xFF;
	tsp->gtx.ch_num[3] = 0xFF;

	tsp->finger_num = finger_info;
	tsp->dir.swap_xy = (tsp_swap & TSP_INFO_SWAP_XY ? true : false);
	tsp->dir.flip_x = (tsp_swap & TSP_INFO_FLIP_X ? true : false);
	tsp->dir.flip_y = (tsp_swap & TSP_INFO_FLIP_Y ? true : false);

	tsp->baseline = baseline & 0xFFFF;

	tsp->screen.rx = (tsp_scr >> 16) & 0xFFFF;
	tsp->screen.tx = tsp_scr & 0xFFFF;

	if (tsp->dir.swap_xy) {
		tsp->width = tsp_lcd & 0xFFFF;
		tsp->height = (tsp_lcd >> 16) & 0xFFFF;
	} else {
		tsp->width = (tsp_lcd >> 16) & 0xFFFF;
		tsp->height = tsp_lcd & 0xFFFF;
	}

#if IST30XX_USE_KEY
	tkey->enable = (((tkey_info0 >> 24) & 0xFF) ? true : false);
	tkey->key_num = (tkey_info0 >> 16) & 0xFF;
	tkey->ch_num[0].tx = tkey_info0 & 0xFF;
	tkey->ch_num[0].rx = (tkey_info0 >> 8) & 0xFF;
	tkey->ch_num[1].tx = (tkey_info1 >> 16) & 0xFF;
	tkey->ch_num[1].rx = (tkey_info1 >> 24) & 0xFF;
	tkey->ch_num[2].tx = tkey_info1 & 0xFF;
	tkey->ch_num[2].rx = (tkey_info1 >> 8) & 0xFF;

	tkey->baseline = (baseline >> 16) & 0xFFFF;
#endif

	return ret;
}

int ist30xx_get_tsp_info(struct ist30xx_data *data)
{
	int ret = 0;
	int retry = 3;
#if IST30XX_INTERNAL_BIN
	TSP_INFO *tsp = &data->tsp_info;
#if IST30XX_USE_KEY
	TKEY_INFO *tkey = &data->tkey_info;
#endif
	u8 *cfg_buf;
#endif

	while (retry--) {
		ret = ist30xx_tsp_update_info(data);
		if (ret == 0) {

			return ret;
		}
	}

#if IST30XX_INTERNAL_BIN
	cfg_buf = (u8 *) &data->fw.buf[data->tags.cfg_addr];

	tsp->ch_num.rx = (u8) cfg_buf[0x0C];
	tsp->ch_num.tx = (u8) cfg_buf[0x0D];
	tsp->screen.rx = (u8) cfg_buf[0x0E];
	tsp->screen.tx = (u8) cfg_buf[0x0F];
	tsp->node.len = tsp->ch_num.tx * tsp->ch_num.rx;

	tsp->gtx.num = (u8) cfg_buf[0x10];
	tsp->gtx.ch_num[0] = (u8) cfg_buf[0x11];
	tsp->gtx.ch_num[1] = (u8) cfg_buf[0x12];
	tsp->gtx.ch_num[2] = (u8) cfg_buf[0x13];
	tsp->gtx.ch_num[3] = (u8) cfg_buf[0x14];

	tsp->finger_num = (u8) cfg_buf[0x38];
	tsp->dir.swap_xy =
	    (bool) (cfg_buf[0x39] & TSP_INFO_SWAP_XY ? true : false);
	tsp->dir.flip_x =
	    (bool) (cfg_buf[0x39] & TSP_INFO_FLIP_X ? true : false);
	tsp->dir.flip_y =
	    (bool) (cfg_buf[0x39] & TSP_INFO_FLIP_Y ? true : false);

	tsp->baseline = (u16) ((cfg_buf[0x29] << 8) | cfg_buf[0x28]);

	if (tsp->dir.swap_xy) {
		tsp->width = (u16) ((cfg_buf[0x23] << 8) | cfg_buf[0x22]);
		tsp->height = (u16) ((cfg_buf[0x21] << 8) | cfg_buf[0x20]);
	} else {
		tsp->width = (u16) ((cfg_buf[0x21] << 8) | cfg_buf[0x20]);
		tsp->height = (u16) ((cfg_buf[0x23] << 8) | cfg_buf[0x22]);
	}

#if IST30XX_USE_KEY
	tkey->enable = (bool) (cfg_buf[0x15] & 1);
	tkey->key_num = (u8) cfg_buf[0x16];
	tkey->ch_num[0].tx = (u8) cfg_buf[0x17];
	tkey->ch_num[0].rx = (u8) cfg_buf[0x1B];
	tkey->ch_num[1].tx = (u8) cfg_buf[0x18];
	tkey->ch_num[1].rx = (u8) cfg_buf[0x1C];
	tkey->ch_num[2].tx = (u8) cfg_buf[0x19];
	tkey->ch_num[2].rx = (u8) cfg_buf[0x1D];

	tkey->baseline = (u16) ((cfg_buf[0x2B] << 8) | cfg_buf[0x2A]);
#endif
#endif

	return ret;
}

void ist30xx_print_info(struct ist30xx_data *data)
{
	TSP_INFO *tsp = &data->tsp_info;
	TKEY_INFO *tkey = &data->tkey_info;

	tsp_info("*** TSP/TKEY info ***\n");
	tsp_info("TSP info: \n");
	tsp_info(" finger num: %d\n", tsp->finger_num);
	tsp_info(" dir swap: %d, flip x: %d, y: %d\n",
		 tsp->dir.swap_xy, tsp->dir.flip_x, tsp->dir.flip_y);
	tsp_info(" baseline: %d\n", tsp->baseline);
	tsp_info(" ch_num tx: %d, rx: %d\n", tsp->ch_num.tx, tsp->ch_num.rx);
	tsp_info(" screen tx: %d, rx: %d\n", tsp->screen.tx, tsp->screen.rx);
	tsp_info(" width: %d, height: %d\n", tsp->width, tsp->height);
	tsp_info(" gtx num: %d, ch [1]: %d, [2]: %d, [3]: %d, [4]: %d\n",
		 tsp->gtx.num, tsp->gtx.ch_num[0], tsp->gtx.ch_num[1],
		 tsp->gtx.ch_num[2], tsp->gtx.ch_num[3]);
	tsp_info(" node len: %d\n", tsp->node.len);
	tsp_info("TKEY info: \n");
	tsp_info(" enable: %d, key num: %d\n", tkey->enable, tkey->key_num);
	tsp_info(" baseline : %d\n", tkey->baseline);
}

#define update_next_step(ret)   { if (unlikely(ret)) goto end; }
int ist30xx_fw_update(struct ist30xx_data *data, const u8 *buf, int size)
{
	int ret = 0;
	u32 chksum = 0;
	struct ist30xx_fw *fw = &data->fw;

	tsp_info("*** Firmware update ***\n");
	tsp_info(" main: %x, fw: %x, test: %x, core: %x(addr: 0x%x ~ 0x%x)\n",
		 fw->bin.main_ver, fw->bin.fw_ver, fw->bin.test_ver,
		 fw->bin.core_ver, fw->index, (fw->index + fw->size));

	data->status.update = 1;
	data->status.update_result = 0;

	ist30xx_disable_irq(data);

	ret = ist30xxc_isp_fw_update(data, buf);
	update_next_step(ret);

	ret = ist30xx_read_cmd(data, eHCOM_GET_CRC32, &chksum);
	if (unlikely((ret) || (chksum != fw->chksum))) {
		if (unlikely(ret))
			ist30xx_reset(data, false);

		goto end;
	}

	ret = ist30xx_get_ver_info(data);
	update_next_step(ret);

end:
	if (unlikely(ret)) {
		data->status.update_result = 1;
		tsp_warn("Firmware update Fail!, ret=%d\n", ret);
	} else if (unlikely(chksum != fw->chksum)) {
		data->status.update_result = 1;
		tsp_warn("Error CheckSum: %x(%x)\n", chksum, fw->chksum);
		ret = -ENOEXEC;
	}

	ist30xx_enable_irq(data);

	data->status.update = 2;

	return ret;
}

int ist30xx_fw_recovery(struct ist30xx_data *data)
{
	int ret = -EPERM;
	u8 *fw = data->fw.buf;
	int fw_size = data->fw.buf_size;

	ret = ist30xx_get_update_info(data, fw, fw_size);
	if (ret) {
		data->status.update_result = 1;
		return ret;
	}

	data->fw.bin.main_ver = ist30xx_parse_ver(data, FLAG_MAIN, fw);
	data->fw.bin.fw_ver = ist30xx_parse_ver(data, FLAG_FW, fw);
	data->fw.bin.test_ver = ist30xx_parse_ver(data, FLAG_TEST, fw);
	data->fw.bin.core_ver = ist30xx_parse_ver(data, FLAG_CORE, fw);

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_fw_update(data, fw, fw_size);
	if (ret == 0) {
		ist30xx_get_tsp_info(data);
		ist30xx_print_info(data);
		ist30xx_calibrate(data, 1);
	}
	mutex_unlock(&ist30xx_mutex);

	ist30xx_start(data);

	return ret;
}

#if IST30XX_INTERNAL_BIN
int ist30xx_check_fw(struct ist30xx_data *data)
{
	int ret;
	u32 chksum;

	ret = ist30xx_read_cmd(data, eHCOM_GET_CRC32, &chksum);
	if (unlikely(ret)) {
		ist30xx_reset(data, false);
		return ret;
	}

	if (unlikely(chksum != data->fw.chksum)) {
		tsp_warn("Checksum compare error, (IC: %08x, Bin: %08x)\n",
			 chksum, data->fw.chksum);
		return -EPERM;
	}

	return 0;
}

bool ist30xx_check_valid_vendor(u32 tsp_vendor)
{
#if (IMAGIS_TSP_IC < IMAGIS_IST3038C)
	switch (tsp_vendor) {
	case TSP_TYPE_ALPS:
	case TSP_TYPE_EELY:
	case TSP_TYPE_TOP:
	case TSP_TYPE_MELFAS:
	case TSP_TYPE_ILJIN:
	case TSP_TYPE_SYNOPEX:
	case TSP_TYPE_SMAC:
	case TSP_TYPE_TOVIS:
	case TSP_TYPE_ELK:
	case TSP_TYPE_BOE_SLOC:
	case TSP_TYPE_CNI_GF1:
	case TSP_TYPE_OTHERS:
		return true;
	default:
		return false;
	}
#else
	if (tsp_vendor < TSP_TYPE_NO)
		return true;
#endif

	return false;
}

#if (IMAGIS_TSP_IC < IMAGIS_IST3038C)
#if IST30XX_MULTIPLE_TSP
void ist30xx_set_tsp_fw(struct ist30xx_data *data)
{
	char *str;
	struct ist30xx_fw *fw = &data->fw;

	switch (data->tsp_type) {
	case TSP_TYPE_BOE_SLOC:
		str = "BOE";
		fw->buf = (u8 *) ist30xxc_fw;
		fw->buf_size = sizeof(ist30xxc_fw);
		break;
	case TSP_TYPE_CNI_GF1:
		str = "CNI";
		fw->buf = (u8 *) ist30xxc_fw2;
		fw->buf_size = sizeof(ist30xxc_fw2);
		break;

	case TSP_TYPE_UNKNOWN:
	default:
		str = "Unknown";
		tsp_warn("Unknown TSP vendor(0x%x)\n", data->tsp_type);
		break;
	}
	tsp_info("TSP vendor : %s(%x)\n", str, data->tsp_type);
}
#endif
#endif
#define MAIN_VER_MASK           0xFF000000
int ist30xx_check_auto_update(struct ist30xx_data *data)
{
	int ret = 0;
	int retry = IST30XX_MAX_RETRY_CNT;
	u32 chip_id = 0;
	bool tsp_check = false;
	u32 chksum;
	struct ist30xx_fw *fw = &data->fw;

	while (retry--) {
		ret =
		    ist30xx_read_reg(data->client,
				     IST30XX_DA_ADDR(eHCOM_GET_CHIP_ID),
				     &chip_id);
		if (likely(ret == 0)) {
			if (likely(chip_id == IST30XX_CHIP_ID))
				tsp_check = true;

			break;
		}

		ist30xx_reset(data, false);
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				  (eHCOM_FW_HOLD << 16) | (1 & 0xFFFF));
		tsp_info("%s: set FW_HOLD\n", __func__);
	}
#if (IMAGIS_TSP_IC < IMAGIS_IST3038C)
	tsp_info("TSP vendor: %x\n", tsp_type);
#endif

	if (unlikely(!tsp_check))
		goto fw_check_end;

	ret = ist30xx_get_ver_info(data);
	if (ret) {
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				  (eHCOM_FW_HOLD << 16) | (1 & 0xFFFF));
		tsp_info("%s: set FW_HOLD\n", __func__);
	}

	if ((data->product_id == A12HD_PID) && (fw->cur.fw_ver == 0x0011))
		goto fw_check_end;

	if (likely((fw->cur.fw_ver > 0) && (fw->cur.fw_ver <= 0xFFFF))) {
		if (unlikely
		    (((fw->cur.main_ver & MAIN_VER_MASK) == MAIN_VER_MASK)
		     || ((fw->cur.main_ver & MAIN_VER_MASK) == 0)))
			goto fw_check_end;

		tsp_info("Version compare IC: %x(%x), BIN: %x(%x)\n",
			 fw->cur.fw_ver, fw->cur.main_ver, fw->bin.fw_ver,
			 fw->bin.main_ver);

		/* If FW version is same, check FW checksum */
		if (likely((fw->cur.main_ver == fw->bin.main_ver) &&
			   (fw->cur.fw_ver == fw->bin.fw_ver) &&
			   (fw->cur.test_ver == 0))) {
			ret = ist30xx_read_cmd(data, eHCOM_GET_CRC32, &chksum);
			if (unlikely((ret) || (chksum != fw->chksum))) {
				tsp_warn
				    ("Checksum error, IC: %x, Bin: %x (ret: %d)\n",
				     chksum, fw->chksum, ret);
				goto fw_check_end;
			}
		}

		/*
		 * fw->cur.main_ver : Main version in TSP IC
		 * fw->cur.fw_ver : FW version if TSP IC
		 * fw->bin.main_ver : Main version in FW Binary
		 * fw->bin.fw_ver : FW version in FW Binary
		 */
		/* If the ver of binary is higher than ver of IC, FW update operate. */

		if (likely((fw->cur.main_ver >= fw->bin.main_ver) &&
			   (fw->cur.fw_ver >= fw->bin.fw_ver)))
			return 0;
	}

fw_check_end:
	return -EAGAIN;
}

int ist30xx_auto_bin_update(struct ist30xx_data *data)
{
	int ret = 0;
	int retry = IST30XX_MAX_RETRY_CNT;
	struct ist30xx_fw *fw = &data->fw;

	if (data->product_id == A12HD_PID) {
		fw->buf = (u8 *) ist30xxc_fw1;
		fw->buf_size = sizeof(ist30xxc_fw1);
	} else {
		fw->buf = 0;
		fw->buf_size = 0;
		return 0;
	}

#if (IMAGIS_TSP_IC < IMAGIS_IST3038C)
#if IST30XX_MULTIPLE_TSP
	ist30xx_set_tsp_fw(data);
#endif
#endif

	ret = ist30xx_get_update_info(data, fw->buf, fw->buf_size);
	if (unlikely(ret))
		return 1;
	fw->bin.main_ver = ist30xx_parse_ver(data, FLAG_MAIN, fw->buf);
	fw->bin.fw_ver = ist30xx_parse_ver(data, FLAG_FW, fw->buf);
	fw->bin.test_ver = ist30xx_parse_ver(data, FLAG_TEST, fw->buf);
	fw->bin.core_ver = ist30xx_parse_ver(data, FLAG_CORE, fw->buf);

	tsp_info("IC: %x, Binary ver main: %x, fw: %x, test: %x, core: %x\n",
		 data->chip_id, fw->bin.main_ver, fw->bin.fw_ver,
		 fw->bin.test_ver, fw->bin.core_ver);

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_check_auto_update(data);
	mutex_unlock(&ist30xx_mutex);

	if (likely(ret >= 0))
		return ret;

update_bin:
	tsp_info
	    ("Update version. fw(main, test, core): %x(%x, %x, %x) -> %x(%x, %x, %x)\n",
	     fw->cur.fw_ver, fw->cur.main_ver, fw->cur.test_ver,
	     fw->cur.core_ver, fw->bin.fw_ver, fw->bin.main_ver,
	     fw->bin.test_ver, fw->bin.core_ver);

	mutex_lock(&ist30xx_mutex);
	while (retry--) {
		ret = ist30xx_fw_update(data, fw->buf, fw->buf_size);
		if (unlikely(!ret))
			break;
	}
	mutex_unlock(&ist30xx_mutex);

	if (unlikely(ret))
		goto end_update;

	if (unlikely(retry > 0 && ist30xx_check_fw(data)))
		goto update_bin;

	mutex_lock(&ist30xx_mutex);
	ist30xx_calibrate(data, IST30XX_MAX_RETRY_CNT);
	mutex_unlock(&ist30xx_mutex);

end_update:
	ist30xx_write_cmd(data, IST30XX_HIB_CMD,
			  (eHCOM_FW_HOLD << 16) | (1 & 0xFFFF));
	tsp_info("%s: set FW_HOLD\n", __func__);
	return ret;
}
#endif

#define MAX_FILE_PATH   255
const u8 fwbuf[IST30XX_FLASH_TOTAL_SIZE + sizeof(struct ist30xx_tags)];
/* sysfs: /sys/class/touch/firmware/firmware */
ssize_t ist30xx_fw_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t size)
{
	int ret;
	int fw_size = 0;
	u8 *fw = NULL;
	mm_segment_t old_fs = { 0 };
	struct file *fp = NULL;
	long fsize = 0, nread = 0;
	char fw_path[MAX_FILE_PATH];
	const struct firmware *request_fw = NULL;
	int mode = 0;
	int calib = 1;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	sscanf(buf, "%d %d", &mode, &calib);

	switch (mode) {
	case MASK_UPDATE_INTERNAL:
#if IST30XX_INTERNAL_BIN
		fw = data->fw.buf;
		fw_size = data->fw.buf_size;
#else
		data->status.update_result = 1;
		tsp_warn("Not support internal bin!!\n");
		return size;
#endif
		break;

	case MASK_UPDATE_FW:
		ret = request_firmware(&request_fw, IST30XX_FW_NAME,
				       &data->client->dev);
		if (ret) {
			data->status.update_result = 1;
			tsp_warn("File not found, %s\n", IST30XX_FW_NAME);
			return size;
		}

		fw = (u8 *) request_fw->data;
		fw_size = (u32) request_fw->size;
		tsp_info("firmware is loaded!!\n");
		break;

	case MASK_UPDATE_SDCARD:
		old_fs = get_fs();
		set_fs(get_ds());

		snprintf(fw_path, MAX_FILE_PATH, "/sdcard/%s", IST30XX_FW_NAME);
		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			data->status.update_result = 1;
			goto err_file_open;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;

		if (sizeof(fwbuf) != fsize) {
			data->status.update_result = 1;
			tsp_info("mismatch fw size\n");
			goto err_fw_size;
		}

		nread = vfs_read(fp, (char __user *)fwbuf, fsize, &fp->f_pos);
		if (nread != fsize) {
			data->status.update_result = 1;
			tsp_info("mismatch fw size\n");
			goto err_fw_size;
		}

		fw = (u8 *) fwbuf;
		fw_size = (u32) fsize;

		filp_close(fp, current->files);
		tsp_info("firmware is loaded!!\n");
		break;

	case MASK_UPDATE_ERASE:
		tsp_info("EEPROM all erase!!\n");

		mutex_lock(&ist30xx_mutex);
		ist30xx_disable_irq(data);
		ist30xx_reset(data, true);
		ist30xxc_isp_enable(data, true);
		ist30xxc_isp_erase(data, IST30XX_ISP_ERASE_BLOCK, 0);
#if (IST30XX_FLASH_INFO_SIZE > 0)
		ist30xxc_isp_erase(data, IST30XX_ISP_ERASE_INFO, 0);
#endif
		ist30xxc_isp_enable(data, false);
		ist30xx_reset(data, false);
		ist30xx_start(data);
		ist30xx_enable_irq(data);
		mutex_unlock(&ist30xx_mutex);

	default:
		return size;
	}

	ret = ist30xx_get_update_info(data, fw, fw_size);
	if (ret) {
		data->status.update_result = 1;
		goto err_get_info;
	}

	data->fw.bin.main_ver = ist30xx_parse_ver(data, FLAG_MAIN, fw);
	data->fw.bin.fw_ver = ist30xx_parse_ver(data, FLAG_FW, fw);
	data->fw.bin.test_ver = ist30xx_parse_ver(data, FLAG_TEST, fw);
	data->fw.bin.core_ver = ist30xx_parse_ver(data, FLAG_CORE, fw);

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_fw_update(data, fw, fw_size);
	if (ret == 0) {
		ist30xx_get_tsp_info(data);
		ist30xx_print_info(data);
		if (calib)
			ist30xx_calibrate(data, 1);
	}
	mutex_unlock(&ist30xx_mutex);

	ist30xx_start(data);

err_get_info:
	if (request_fw != NULL)
		release_firmware(request_fw);

	if (fp) {
	      err_fw_size:
		filp_close(fp, NULL);
	      err_file_open:
		set_fs(old_fs);
	}

	return size;
}

/* sysfs: /sys/class/touch/firmware/fw_sdcard */
ssize_t ist30xx_fw_sdcard_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int ret = 0;
	mm_segment_t old_fs = { 0 };
	struct file *fp = NULL;
	long fsize = 0, nread = 0;
	char fw_path[MAX_FILE_PATH];
	struct ist30xx_data *data = dev_get_drvdata(dev);

	old_fs = get_fs();
	set_fs(get_ds());

	snprintf(fw_path, MAX_FILE_PATH, "/sdcard/%s", IST30XX_FW_NAME);
	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		data->status.update_result = 1;
		goto err_file_open;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	if (sizeof(fwbuf) != fsize) {
		data->status.update_result = 1;
		tsp_info("mismatch fw size\n");
		goto err_fw_size;
	}

	nread = vfs_read(fp, (char __user *)fwbuf, fsize, &fp->f_pos);
	if (nread != fsize) {
		data->status.update_result = 1;
		tsp_info("mismatch fw size\n");
		goto err_fw_size;
	}

	filp_close(fp, current->files);
	tsp_info("firmware is loaded!!\n");

	ret = ist30xx_get_update_info(data, fwbuf, fsize);
	if (ret) {
		data->status.update_result = 1;
		goto err_get_info;
	}

	data->fw.bin.main_ver = ist30xx_parse_ver(data, FLAG_MAIN, fwbuf);
	data->fw.bin.fw_ver = ist30xx_parse_ver(data, FLAG_FW, fwbuf);
	data->fw.bin.test_ver = ist30xx_parse_ver(data, FLAG_TEST, fwbuf);
	data->fw.bin.core_ver = ist30xx_parse_ver(data, FLAG_CORE, fwbuf);

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_fw_update(data, fwbuf, fsize);
	if (ret == 0) {
		ist30xx_get_tsp_info(data);
		ist30xx_print_info(data);
	}
	mutex_unlock(&ist30xx_mutex);

	ist30xx_start(data);

err_get_info:
err_fw_size:
	filp_close(fp, NULL);
err_file_open:
	set_fs(old_fs);

	return 0;
}

/* sysfs: /sys/class/touch/firmware/firmware */
ssize_t ist30xx_fw_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int count;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	switch (data->status.update) {
	case 1:
		count = sprintf(buf, "Downloading\n");
		break;
	case 2:
		if (data->status.update_result) {
			count = sprintf(buf, "Update fail\n");
		} else {
			count = sprintf(buf, "Update success, "
					"ver %x(%x, %x, %x)-> %x(%x, %x, %x), "
					"status : %d, gap : %d\n",
					data->fw.prev.fw_ver,
					data->fw.prev.main_ver,
					data->fw.prev.test_ver,
					data->fw.prev.core_ver,
					data->fw.cur.fw_ver,
					data->fw.cur.main_ver,
					data->fw.cur.test_ver,
					data->fw.cur.core_ver,
					CALIB_TO_STATUS(data->status.calib_msg),
					CALIB_TO_GAP(data->status.calib_msg));
		}
		break;
	default:
		if (data->status.update_result)
			count = sprintf(buf, "Update fail\n");
		else
			count = sprintf(buf, "Pass\n");
	}

	return count;
}

/* sysfs: /sys/class/touch/firmware/fw_read */
u32 buf32_flash[IST30XX_FLASH_TOTAL_SIZE / IST30XX_DATA_LEN];
ssize_t ist30xx_fw_read_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int i;
	int ret;
	int count;
	mm_segment_t old_fs = { 0 };
	struct file *fp = NULL;
	char fw_path[MAX_FILE_PATH];
	u8 *buf8 = (u8 *) buf32_flash;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);

	ret = ist30xxc_isp_fw_read(data, buf32_flash);
	if (ret) {
		count = sprintf(buf, "Fail\n");
		tsp_err("isp fw read fail\n");
		goto err_file_open;
	}

	for (i = 0; i < IST30XX_FLASH_TOTAL_SIZE; i += 16) {
		tsp_debug("%07x: %02x%02x %02x%02x %02x%02x %02x%02x "
			  "%02x%02x %02x%02x %02x%02x %02x%02x\n", i,
			  buf8[i], buf8[i + 1], buf8[i + 2], buf8[i + 3],
			  buf8[i + 4], buf8[i + 5], buf8[i + 6], buf8[i + 7],
			  buf8[i + 8], buf8[i + 9], buf8[i + 10], buf8[i + 11],
			  buf8[i + 12], buf8[i + 13], buf8[i + 14],
			  buf8[i + 15]);
	}

	old_fs = get_fs();
	set_fs(get_ds());

	snprintf(fw_path, MAX_FILE_PATH, "/sdcard/%s", IST30XX_BIN_NAME);
	fp = filp_open(fw_path, O_CREAT | O_WRONLY | O_TRUNC, 0);
	if (IS_ERR(fp)) {
		count = sprintf(buf, "Fail\n");
		goto err_file_open;
	}

	fp->f_op->write(fp, buf8, IST30XX_FLASH_TOTAL_SIZE, &fp->f_pos);
	fput(fp);

	filp_close(fp, NULL);
	set_fs(old_fs);

	count = sprintf(buf, "OK\n");

err_file_open:
	ist30xx_enable_irq(data);
	mutex_unlock(&ist30xx_mutex);

	ist30xx_start(data);

	return count;
}

/* sysfs: /sys/class/touch/firmware/version */
ssize_t ist30xx_fw_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int count;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	count = sprintf(buf, "ID: %x, main: %x, fw: %x, test: %x, core: %x\n",
			data->chip_id, data->fw.cur.main_ver,
			data->fw.cur.fw_ver, data->fw.cur.test_ver,
			data->fw.cur.core_ver);

#if IST30XX_INTERNAL_BIN
	{
		char msg[128];
		int ret = 0;

		ret =
		    ist30xx_get_update_info(data, data->fw.buf,
					    data->fw.buf_size);
		if (ret == 0) {
			count += sprintf(msg,
					 " Header - main: %x, fw: %x, test: %x, core: %x\n",
					 ist30xx_parse_ver(data, FLAG_MAIN,
							   data->fw.buf),
					 ist30xx_parse_ver(data, FLAG_FW,
							   data->fw.buf),
					 ist30xx_parse_ver(data, FLAG_TEST,
							   data->fw.buf),
					 ist30xx_parse_ver(data, FLAG_CORE,
							   data->fw.buf));
			strcat(buf, msg);
		}
	}
#endif

	return count;
}

#ifdef XIAOMI_PRODUCT
/* sysfs: /sys/class/touch/firmware/lockdown */
ssize_t ist30xx_lockdown_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	tsp_info("lockdown : %08X%08X\n", data->fw.cur.lockdown[0],
		 data->fw.cur.lockdown[1]);

	return sprintf(buf, "%08X%08X", data->fw.cur.lockdown[0],
		       data->fw.cur.lockdown[1]);
}

/* sysfs: /sys/class/touch/firmware/config */
ssize_t ist30xx_config_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	tsp_info("config : %08X%08X\n", data->fw.cur.config[0],
		 data->fw.cur.config[1]);

	return sprintf(buf, "%08X%08X", data->fw.cur.config[0],
		       data->fw.cur.config[1]);
}
#endif

/* sysfs  */
static DEVICE_ATTR(fw_read, 0664, ist30xx_fw_read_show, NULL);
static DEVICE_ATTR(firmware, 0664, ist30xx_fw_status_show, ist30xx_fw_store);
static DEVICE_ATTR(fw_sdcard, 0664, ist30xx_fw_sdcard_show, NULL);
static DEVICE_ATTR(version, 0664, ist30xx_fw_version_show, NULL);
#ifdef XIAOMI_PRODUCT
static DEVICE_ATTR(lockdown, 0664, ist30xx_lockdown_show, NULL);
static DEVICE_ATTR(config, 0664, ist30xx_config_show, NULL);
#endif

struct class *ist30xx_class;
struct device *ist30xx_fw_dev;

static struct attribute *fw_attributes[] = {
	&dev_attr_fw_read.attr,
	&dev_attr_firmware.attr,
	&dev_attr_fw_sdcard.attr,
	&dev_attr_version.attr,
#ifdef XIAOMI_PRODUCT
	&dev_attr_lockdown.attr,
	&dev_attr_config.attr,
#endif
	NULL,
};

static struct attribute_group fw_attr_group = {
	.attrs = fw_attributes,
};

int ist30xx_init_update_sysfs(struct ist30xx_data *data)
{
	/* /sys/class/touch */
	ist30xx_class = class_create(THIS_MODULE, "touch");

	/* /sys/class/touch/firmware */
	ist30xx_fw_dev =
	    device_create(ist30xx_class, NULL, 0, data, "firmware");

	/* /sys/class/touch/firmware/... */
	if (unlikely(sysfs_create_group(&ist30xx_fw_dev->kobj, &fw_attr_group)))
		tsp_err("Failed to create sysfs group(%s)!\n", "firmware");

	data->status.update = 0;
	data->status.calib = 0;
	data->status.update_result = 0;

	return 0;
}
