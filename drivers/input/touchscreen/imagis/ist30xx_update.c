/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2015 XiaoMi, Inc.
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
#include <asm/unaligned.h>
#include <linux/stat.h>
#include <linux/module.h>

#include "ist30xx.h"
#include "ist30xx_update.h"
#include "ist30xx_tracking.h"

#if IST30XX_DEBUG
#include "ist30xx_misc.h"
#endif

extern void ist30xx_disable_irq(struct ist30xx_data *data);
extern void ist30xx_enable_irq(struct ist30xx_data *data);


int ist30xx_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
			 int msg_num, u8 *cmd_buf)
{
	int ret = 0;
	int idx = msg_num - 1;
	int size = msgs[idx].len;
	u8 *msg_buf = NULL;
	u8 *pbuf = NULL;
	int trans_size, max_size = 0;

	if (msg_num == WRITE_CMD_MSG_LEN)
		max_size = I2C_MAX_WRITE_SIZE;
	else if (msg_num == READ_CMD_MSG_LEN)
		max_size = I2C_MAX_READ_SIZE;

	if (unlikely(max_size == 0)) {
		tsp_err("%s() : transaction size(%d)\n", __func__, max_size);
		return -EINVAL;
	}

	if (msg_num == WRITE_CMD_MSG_LEN) {
		msg_buf = kmalloc(max_size + IST30XX_ADDR_LEN, GFP_KERNEL);
		if (!msg_buf)
			return -ENOMEM;
		memcpy(msg_buf, cmd_buf, IST30XX_ADDR_LEN);
		pbuf = msgs[idx].buf;
	}

	while (size > 0) {
		trans_size = (size >= max_size ? max_size : size);

		msgs[idx].len = trans_size;
		if (msg_num == WRITE_CMD_MSG_LEN) {
			memcpy(&msg_buf[IST30XX_ADDR_LEN], pbuf, trans_size);
			msgs[idx].buf = msg_buf;
			msgs[idx].len += IST30XX_ADDR_LEN;
		}
		ret = i2c_transfer(adap, msgs, msg_num);
		if (unlikely(ret != msg_num)) {
			tsp_err("%s() : i2c_transfer failed(%d), num=%d\n",
				__func__, ret, msg_num);
			break;
		}

		if (msg_num == WRITE_CMD_MSG_LEN)
			pbuf += trans_size;
		else
			msgs[idx].buf += trans_size;

		size -= trans_size;
	}

	if (msg_num == WRITE_CMD_MSG_LEN)
		kfree(msg_buf);

	return ret;
}

int ist30xx_read_buf(struct i2c_client *client, u32 cmd, u32 *buf, u16 len)
{
	int ret, i;
	u32 le_reg = cpu_to_be32(cmd);

	struct i2c_msg msg[READ_CMD_MSG_LEN] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = IST30XX_ADDR_LEN,
			.buf = (u8 *)&le_reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len * IST30XX_DATA_LEN,
			.buf = (u8 *)buf,
		},
	};

	ret = ist30xx_i2c_transfer(client->adapter, msg, READ_CMD_MSG_LEN, NULL);
	if (unlikely(ret != READ_CMD_MSG_LEN))
		return -EIO;

	for (i = 0; i < len; i++)
		buf[i] = cpu_to_be32(buf[i]);

	return 0;
}

int ist30xx_write_buf(struct i2c_client *client, u32 cmd, u32 *buf, u16 len)
{
	int i;
	int ret;
	struct i2c_msg msg;
	u8 cmd_buf[IST30XX_ADDR_LEN];
	u8 msg_buf[IST30XX_DATA_LEN * (len + 1)];

	put_unaligned_be32(cmd, cmd_buf);

	if (likely(len > 0)) {
		for (i = 0; i < len; i++)
			put_unaligned_be32(buf[i], msg_buf + (i * IST30XX_DATA_LEN));
	} else {
		/* then add dummy data(4byte) */
		put_unaligned_be32(0, msg_buf);
		len = 1;
	}

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = IST30XX_DATA_LEN * len;
	msg.buf = msg_buf;

	ret = ist30xx_i2c_transfer(client->adapter, &msg, WRITE_CMD_MSG_LEN, cmd_buf);
	if (unlikely(ret != WRITE_CMD_MSG_LEN))
		return -EIO;

	return 0;
}

int ist30xxb_burst_read(struct i2c_client *client, u32 addr,
			u32 *buf32, int len)
{
	int ret = 0;
	int i;
	int max_len = I2C_DA_MAX_READ_SIZE / IST30XX_DATA_LEN;

	addr |= IST30XXB_BURST_ACCESS;

	for (i = 0; i < len; i += max_len) {
		if (len < max_len) max_len = len;

		ret = ist30xx_read_buf(client, addr, buf32, max_len);
		if (unlikely(ret)) {
			tsp_err("Burst fail, addr: %x\n", __func__, addr);
			return ret;
		}

		addr += max_len * IST30XX_DATA_LEN;
		buf32 += max_len;
	}

	return 0;
}

#define IST30XXB_ISP_READ       (1)
#define IST30XXB_ISP_WRITE      (2)
#define IST30XXB_ISP_ERASE_ALL  (3)
#define IST30XXB_ISP_ERASE_PAGE (4)

int ist30xxb_isp_reset(struct ist30xx_data *data)
{
	int ret = 0;

#if (IMAGIS_TSP_IC == IMAGIS_IST3038)
	ret = ist30xx_write_cmd(data->client, IST30XXB_REG_EEPMODE, 0x180);
	if (unlikely(ret)) return ret;
#else
	ret = ist30xx_write_cmd(data->client, IST30XXB_REG_EEPPWRCTRL, 0xB200);
	if (unlikely(ret)) return ret;

	ret = ist30xx_write_cmd(data->client, IST30XXB_REG_EEPPWRCTRL, 0x9200);
	if (unlikely(ret)) return ret;
#endif
	msleep(1);

	return ret;
}

int ist30xxb_isp_enable(struct ist30xx_data *data, bool enable)
{
	int ret = 0;

	if (enable) {
		ret = ist30xx_write_cmd(data->client, IST30XXB_REG_EEPISPEN, 0x1);
		if (ret) return ret;

		ret = ist30xx_write_cmd(data->client, IST30XXB_REG_LDOOSC, 0x74C8);
		if (ret) return ret;

		ret = ist30xx_write_cmd(data->client, IST30XXB_REG_CLKDIV, 0x3);
		if (ret) return ret;

		msleep(5);
	} else {
		ret = ist30xx_write_cmd(data->client, IST30XXB_REG_EEPISPEN, 0x0);
		if (ret) return ret;

		msleep(1);
	}

	return 0;
}

int ist30xxb_isp_mode(struct ist30xx_data *data, int mode)
{
	int ret = 0;
	u32 val = 0;

	switch (mode) {
	case IST30XXB_ISP_READ:
		val = 0x1C0;
		break;
	case IST30XXB_ISP_WRITE:
		val = 0x1A8;
		break;
	case IST30XXB_ISP_ERASE_ALL:
		val = 0x1A7;
		break;
	case IST30XXB_ISP_ERASE_PAGE:
		val = 0x1A3;
		break;
	default:
		tsp_err("ISP fail, unknown mode\n");
		return -EINVAL;
	}

	ret = ist30xx_write_cmd(data->client, IST30XXB_REG_EEPMODE, val);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XXB_REG_EEPMODE\n");
		return ret;
	}

	return 0;
}

int ist30xxb_isp_erase(struct i2c_client *client, int mode, u32 addr)
{
	int ret = 0;
	u32 val = 0x1A0;
	u8 buf[EEPROM_PAGE_SIZE] = { 0, };
	struct ist30xx_data *data = i2c_get_clientdata(client);

	ret = ist30xxb_isp_mode(data, mode);
	if (unlikely(ret)) return ret;

	ret = ist30xx_write_cmd(client, IST30XXB_REG_EEPADDR, addr);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XXB_REG_EEPADDR\n");
		return ret;
	}

	val = (EEPROM_PAGE_SIZE / IST30XX_DATA_LEN);
	ret = ist30xx_write_buf(client, IST30XXB_REG_EEPWDAT, (u32 *)buf, val);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XXB_REG_EEPWDAT\n");
		return ret;
	}

	msleep(30);

	ist30xxb_isp_reset(data);

	return 0;
}

int ist30xxb_isp_write(struct i2c_client *client, u32 addr,
		       const u32 *buf32, int len)
{
	int ret = 0;

	ret = ist30xx_write_cmd(client, IST30XXB_REG_EEPADDR, addr);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XXB_REG_EEPADDR\n");
		return ret;
	}

	ret = ist30xx_write_buf(client, IST30XXB_REG_EEPWDAT, (u32 *)buf32, len);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XXB_REG_EEPWDAT\n");
		return ret;
	}

	return 0;
}

int ist30xxb_isp_read(struct i2c_client *client, u32 addr,
		      u32 *buf32, int len)
{
	int ret = 0;
	int i;
	int max_len = I2C_MAX_READ_SIZE / IST30XX_DATA_LEN;
	struct ist30xx_data *data = i2c_get_clientdata(client);

	for (i = 0; i < len; i += max_len) {
		if (len < max_len) max_len = len;

		/* IST30xxB ISP read mode */
		ret = ist30xxb_isp_mode(data, IST30XXB_ISP_READ);
		if (unlikely(ret)) return ret;

		ret = ist30xx_write_cmd(client, IST30XXB_REG_EEPADDR, addr);
		if (unlikely(ret)) {
			tsp_err("ISP fail, IST30XXB_REG_EEPADDR\n");
			return ret;
		}

		ret = ist30xx_read_buf(client, IST30XXB_REG_EEPRDAT, buf32, max_len);
		if (unlikely(ret)) {
			tsp_err("ISP fail, IST30XXB_REG_EEPWDAT\n");
			return ret;
		}

		addr += max_len * IST30XX_DATA_LEN;
		buf32 += max_len;
	}

	return 0;
}

int ist30xxb_cmd_read_chksum(struct i2c_client *client,
			     u32 start_addr, u32 end_addr, u32 *chksum)
{
	int ret = 0;
	u32 val = (1 << 31); // Chkecksum enable

	val |= start_addr;
	val |= (end_addr / IST30XX_DATA_LEN - 1) << 16;

	ret = ist30xx_write_cmd(client, IST30XXB_REG_CHKSMOD, val);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XXB_REG_CHKSMOD (%x)\n", val);
		return ret;
	}

	msleep(10);

	ret = ist30xx_read_cmd(client, IST30XXB_REG_CHKSDAT, chksum);
	if (unlikely(ret)) {
		tsp_err("ISP fail, IST30XXB_REG_CHKSDAT (%x)\n", chksum);
		return ret;
	}

	tsp_verb("chksum: %x (%x~%x)\n", *chksum, start_addr, end_addr);

	return 0;
}

int ist30xxb_read_chksum(struct i2c_client *client, u32 *chksum)
{
	int ret = 0;
	u32 start_addr, end_addr;
	u32 chksum1 = 0, chksum2 = 0;
	struct ist30xx_data *data = i2c_get_clientdata(client);

	start_addr = 0;
	end_addr = data->tags.flag_addr;
	ret = ist30xxb_cmd_read_chksum(client, start_addr, end_addr, &chksum1);
	if (unlikely(ret)) return ret;

	start_addr = data->tags.flag_addr + data->tags.flag_size;
	end_addr = data->tags.sensor2_addr + data->tags.sensor2_size;
	ret = ist30xxb_cmd_read_chksum(client, start_addr, end_addr, &chksum2);
	if (unlikely(ret)) return ret;

	*chksum = chksum1 | (chksum2 << 16);

	tsp_info("Chksum: %x(%x~%x, %x~%x)\n", *chksum, 0, data->tags.flag_addr,
		 start_addr, end_addr);

	return 0;
}


int ist30xxb_isp_fw_read(struct i2c_client *client, u32 *buf32)
{
	int ret = 0;
	int i;

	u16 addr = EEPROM_BASE_ADDR;
	int len = EEPROM_PAGE_SIZE / IST30XX_DATA_LEN;
	struct ist30xx_data *data = i2c_get_clientdata(client);

	ist30xx_reset(data, true);

	/* IST30xxB ISP enable */
	ret = ist30xxb_isp_enable(data, true);
	if (unlikely(ret)) return ret;

	for (i = 0; i < IST30XX_EEPROM_SIZE; i += EEPROM_PAGE_SIZE) {
		ret = ist30xxb_isp_read(client, addr, buf32, len);
		if (unlikely(ret)) goto isp_fw_read_end;

		addr += EEPROM_PAGE_SIZE;
		buf32 += len;
	}

isp_fw_read_end:
	/* IST30xxB ISP disable */
	ist30xxb_isp_enable(data, false);
	return ret;
}

int ist30xxb_isp_fw_update(struct i2c_client *client, const u8 *buf, u32 *chksum)
{
	int ret = 0;
	int i;
	u32 page_cnt = IST30XX_EEPROM_SIZE / EEPROM_PAGE_SIZE;
	u16 addr = EEPROM_BASE_ADDR;
	int len = EEPROM_PAGE_SIZE / IST30XX_DATA_LEN;
	struct ist30xx_data *data = i2c_get_clientdata(client);

	ist30xx_tracking(data, TRACK_CMD_FWUPDATE);

	/* IST30xxB ISP enable */
	ret = ist30xxb_isp_enable(data, true);

	ret = ist30xxb_isp_erase(client, IST30XXB_ISP_ERASE_ALL, 0);
	if (unlikely(ret)) goto isp_fw_update_end;

	for (i = 0; i < page_cnt; i++) {
		/* IST30xxB ISP write mode */
		ret = ist30xxb_isp_mode(data, IST30XXB_ISP_WRITE);
		if (unlikely(ret)) goto isp_fw_update_end;

		ret = ist30xxb_isp_write(client, addr, (u32 *)buf, len);
		if (unlikely(ret)) goto isp_fw_update_end;

		addr += EEPROM_PAGE_SIZE;
		buf += EEPROM_PAGE_SIZE;

		msleep(5);

		ist30xxb_isp_reset(data);
	}

isp_fw_update_end:
	/* IST30xxB ISP disable */
	ist30xxb_isp_enable(data, false);
	return ret;
}

int ist30xxb_isp_bootloader(struct i2c_client *client, const u8 *buf)
{
	int ret = 0;
	int i;
	u16 addr = EEPROM_BASE_ADDR;
	struct ist30xx_data *data = i2c_get_clientdata(client);
	u32 page_cnt = (data->fw.index) / EEPROM_PAGE_SIZE;
	int buf_cnt = EEPROM_PAGE_SIZE / IST30XX_DATA_LEN;


	tsp_info("*** Bootloader update (0x%x~0x%x) ***\n",
		 EEPROM_BASE_ADDR, data->fw.index);

	/* IST30xxB ISP enable */
	ret = ist30xxb_isp_enable(data, true);

	ret = ist30xxb_isp_erase(client, IST30XXB_ISP_ERASE_ALL, 0);
	if (unlikely(ret)) goto isp_bootldr_end;

	for (i = 0; i < page_cnt; i++) {
		/* IST30xxB ISP write mode */
		ret = ist30xxb_isp_mode(data, IST30XXB_ISP_WRITE);
		if (unlikely(ret)) goto isp_bootldr_end;

		ret = ist30xxb_isp_write(client, addr, (u32 *)buf, buf_cnt);
		if (unlikely(ret)) goto isp_bootldr_end;

		addr += EEPROM_PAGE_SIZE;
		buf += EEPROM_PAGE_SIZE;

		msleep(5);

		ist30xxb_isp_reset(data);
	}

isp_bootldr_end:
	/* IST30xxB ISP disable */
	ist30xxb_isp_enable(data, false);
	return ret;
}


int ist30xx_fw_write(struct i2c_client *client, const u8 *buf)
{
	int ret;
	int len;
	struct ist30xx_data *data = i2c_get_clientdata(client);
	u32 *buf32 = (u32 *)(buf + data->fw.index);
	u32 size = data->fw.size;

	if (unlikely(size < 0 || size > IST30XX_EEPROM_SIZE))
		return -ENOEXEC;

	while (size > 0) {
		len = (size >= EEPROM_PAGE_SIZE ? EEPROM_PAGE_SIZE : size);

		ret = ist30xx_write_buf(client, CMD_ENTER_FW_UPDATE, buf32, (len >> 2));
		if (unlikely(ret))
			return ret;

		buf32 += (len >> 2);
		size -= len;

		msleep(5);
	}
	return 0;
}


u32 ist30xx_parse_ver(struct ist30xx_data *data, int flag, const u8 *buf)
{
	u32 ver = 0;
	u32 *buf32 = (u32 *)buf;

	if (flag == FLAG_FW)
		ver = (u32)buf32[(data->tags.flag_addr + 4) >> 2];
	else if (flag == FLAG_SUB)
		ver = (u32)buf32[(data->tags.flag_addr + 8) >> 2];
	else if (flag == FLAG_PARAM)
		ver = (u32)(buf32[(data->tags.cfg_addr + 4) >> 2] & 0xFFFF);
	else
		tsp_warn("Parsing ver's flag is not corrent!\n");

	return ver;
}


int calib_ms_delay = WAIT_CALIB_CNT;
int ist30xx_calib_wait(struct ist30xx_data *data)
{
	int cnt = calib_ms_delay;

	data->status.calib_msg = 0;
	while (cnt-- > 0) {
		msleep(100);

		if (data->status.calib_msg) {
			tsp_info("Calibration status : %d, Max raw gap : %d - (%08x)\n",
				 CALIB_TO_STATUS(data->status.calib_msg),
				 CALIB_TO_GAP(data->status.calib_msg),
				 data->status.calib_msg);

			if (CALIB_TO_OS_VALUE(data->status.calib_msg) == 0xFFFF)
				return 1;
			else if (CALIB_TO_STATUS(data->status.calib_msg) == 0)
				return 0;  // Calibrate success
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
	while (wait_cnt--) {
		ist30xx_disable_irq(data);
		ret = ist30xx_cmd_run_device(data->client, true);
		if (unlikely(ret)) continue;

		ret = ist30xx_cmd_calibrate(data->client);
		if (unlikely(ret)) continue;

		ist30xx_enable_irq(data);
		ret = ist30xx_calib_wait(data);
		if (likely(ret == 0)) break;
	}

	ist30xx_disable_irq(data);
	ist30xx_cmd_run_device(data->client, true);

	data->status.update = 2;

	ist30xx_enable_irq(data);

	return ret;
}


int ist30xx_parse_tags(struct ist30xx_data *data, const u8 *buf, const u32 size)
{
	int ret = -EPERM;

	data->ts_tags = (struct ist30xx_tags *)(&buf[size - sizeof(struct ist30xx_tags)]);

	if (!strncmp(data->ts_tags->magic1, IST30XX_TAG_MAGIC, sizeof(data->ts_tags->magic1))
	    && !strncmp(data->ts_tags->magic2, IST30XX_TAG_MAGIC, sizeof(data->ts_tags->magic2))) {
		data->fw.index = data->ts_tags->fw_addr;
		data->fw.size = data->ts_tags->flag_addr - data->ts_tags->fw_addr +
				data->ts_tags->flag_size;
		data->fw.chksum = data->ts_tags->chksum;
		data->tags = *data->ts_tags;

		ret = 0;
	}

	//tsp_verb("Tagts magic1: %s, magic2: %s\n",
	//	 data->ts_tags->magic1, data->ts_tags->magic2);
	tsp_verb(" fw: %x(%x)\n", data->ts_tags->fw_addr, data->ts_tags->fw_size);
	tsp_verb(" flag: %x(%x)\n", data->ts_tags->flag_addr, data->ts_tags->flag_size);
	tsp_verb(" cfg: %x(%x)\n", data->ts_tags->cfg_addr, data->ts_tags->cfg_size);
	tsp_verb(" sensor1: %x(%x)\n", data->ts_tags->sensor1_addr, data->ts_tags->sensor1_size);
	tsp_verb(" sensor2: %x(%x)\n", data->ts_tags->sensor2_addr, data->ts_tags->sensor2_size);
	tsp_verb(" sensor3: %x(%x)\n", data->ts_tags->sensor3_addr, data->ts_tags->sensor3_size);
	tsp_verb(" chksum: %x\n", data->ts_tags->chksum);

	return ret;
}

void ist30xx_get_update_info(struct ist30xx_data *data, const u8 *buf, const u32 size)
{
	int ret;

	ret = ist30xx_parse_tags(data, buf, size);
	if (unlikely(ret != TAGS_PARSE_OK))
		tsp_warn("Cannot find tags of F/W, make a tags by 'tagts.exe'\n");
}

#if (IST30XX_DEBUG) && (IST30XX_INTERNAL_BIN)
int ist30xx_get_tkey_info(struct ist30xx_data *data)
{
	int ret = 0;
	TSP_INFO *tsp = &data->tsp_info;
	TKEY_INFO *tkey = &data->tkey_info;
	u8 *cfg_buf;

	ist30xx_get_update_info(data, data->fw.buf, data->fw.buf_size);
	cfg_buf = (u8 *)&data->fw.buf[data->tags.cfg_addr];

	tkey->enable = (bool)(cfg_buf[0x321] & 1);
	tkey->axis_rx = (bool)((cfg_buf[0x321] >> 1) & 1);
	tkey->key_num = (u8)cfg_buf[0x322];
	tkey->ch_num[0] = (u8)cfg_buf[0x326];
	tkey->ch_num[1] = (u8)cfg_buf[0x327];
	tkey->ch_num[2] = (u8)cfg_buf[0x328];
	tkey->ch_num[3] = (u8)cfg_buf[0x329];
	tkey->ch_num[4] = (u8)cfg_buf[0x32A];

	if (tkey->axis_rx) {
		if (tsp->dir.swap_xy) tsp->height -= 1;
		else tsp->width -= 1;
	} else {
		if (tsp->dir.swap_xy) tsp->width -= 1;
		else tsp->height -= 1;
	}

	return ret;
}

#define TSP_INFO_SWAP_XY    (1 << 0)
#define TSP_INFO_FLIP_X     (1 << 1)
#define TSP_INFO_FLIP_Y     (1 << 2)
int ist30xx_get_tsp_info(struct ist30xx_data *data)
{
	int ret = 0;
	TSP_INFO *tsp = &data->tsp_info;
	u8 *cfg_buf, *sensor_buf;

	ist30xx_get_update_info(data, data->fw.buf, data->fw.buf_size);
	cfg_buf = (u8 *)&data->fw.buf[data->tags.cfg_addr];
	sensor_buf = (u8 *)&data->fw.buf[data->tags.sensor1_addr];

	tsp->finger_num = (u8)cfg_buf[0x304];
	tsp->dir.swap_xy = (bool)(cfg_buf[0x305] & TSP_INFO_SWAP_XY ? true : false);
	tsp->dir.flip_x = (bool)(cfg_buf[0x305] & TSP_INFO_FLIP_X ? true : false);
	tsp->dir.flip_y = (bool)(cfg_buf[0x305] & TSP_INFO_FLIP_Y ? true : false);

	tsp->ch_num.tx = (u8)sensor_buf[0x40];
	tsp->ch_num.rx = (u8)sensor_buf[0x41];

	tsp->node.len = tsp->ch_num.tx * tsp->ch_num.rx;
	tsp->height = (tsp->dir.swap_xy ? tsp->ch_num.rx : tsp->ch_num.tx);
	tsp->width = (tsp->dir.swap_xy ? tsp->ch_num.tx : tsp->ch_num.rx);

	return ret;
}
#endif // (IST30XX_DEBUG) && (IST30XX_INTERNAL_BIN)


#define update_next_step(ret)   { if (unlikely(ret)) goto end; }
int ist30xx_fw_update(struct i2c_client *client, const u8 *buf, int size, bool mode)
{
	int ret = 0;
	u32 chksum = 0;
	struct ist30xx_data *data = i2c_get_clientdata(client);
	struct ist30xx_fw *fw = &data->fw;

	tsp_info("*** Firmware update ***\n");
	tsp_info(" core: %x, param: %x, sub: %x (addr: 0x%x ~ 0x%x)\n",
		 data->fw_ver, data->param_ver, data->sub_ver,
		 fw->index, (fw->index + fw->size));

	data->status.update = 1;

	ist30xx_disable_irq(data);

	ist30xx_reset(data, true);

	if (mode) { /* ISP Mode */
		ret = ist30xxb_isp_fw_update(client, buf, &chksum);
		update_next_step(ret);
	} else { /* I2C SW Mode */
		ret = ist30xx_cmd_update(client, CMD_ENTER_FW_UPDATE);
		update_next_step(ret);

		ret = ist30xx_fw_write(client, buf);
		update_next_step(ret);
	}
	msleep(50);

	buf += IST30XX_EEPROM_SIZE;
	size -= IST30XX_EEPROM_SIZE;

	ret = ist30xx_cmd_run_device(client, true);
	update_next_step(ret);

	ret = ist30xx_read_cmd(client, CMD_GET_CHECKSUM, &chksum);
	if (unlikely((ret) || (chksum != fw->chksum)))
		goto end;

	ret = ist30xx_get_ver_info(data);
	update_next_step(ret);

end:
	if (unlikely(ret)) {
		tsp_warn("Firmware update Fail!, ret=%d\n", ret);
	} else if (unlikely(chksum != fw->chksum)) {
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

	ist30xx_get_update_info(data, fw, fw_size);
	data->fw_ver = ist30xx_parse_ver(data, FLAG_FW, fw);
	data->param_ver = ist30xx_parse_ver(data, FLAG_PARAM, fw);
	data->sub_ver = ist30xx_parse_ver(data, FLAG_SUB, fw);

	mutex_lock(&data->ist30xx_mutex);
	ret = ist30xx_fw_update(data->client, fw, fw_size, true);
	mutex_unlock(&data->ist30xx_mutex);

	ist30xx_calibrate(data, 1);

	ist30xx_init_touch_driver(data);

	return ret;
}


#define fw_update_next_step(ret)   { if (unlikely(ret)) \
				     { step = 1; goto end; } }
#define param_update_next_step(ret)   { if (unlikely(ret)) \
					{ step = 2; goto end; } }
int ist30xx_fw_param_update(struct i2c_client *client, const u8 *buf)
{
	int ret = 0;
	int step = 0;
	u16 len = 0;
	int size = IST30XX_EEPROM_SIZE;
	u32 chksum = 0, ver = 0;
	u32 *param = (u32 *)buf;
	struct ist30xx_data *data = i2c_get_clientdata(client);
	struct ist30xx_fw *fw = &data->fw;

	tsp_info("*** FW param update ***\n");
	tsp_info(" core: %x, param: %x, sub: %x (addr: 0x%x ~ 0x%x)\n",
		 data->fw_ver, data->param_ver, data->sub_ver,
		 fw->index, (fw->index + fw->size));

	data->status.update = 1;

	ist30xx_disable_irq(data);
	ist30xx_reset(data, true);

	/* FW update by SW */
	ret = ist30xx_cmd_update(client, CMD_ENTER_FW_UPDATE);
	fw_update_next_step(ret);

	ret = ist30xx_fw_write(client, buf);
	fw_update_next_step(ret);

	msleep(50);

	param = (u32 *)(buf + (fw->index + fw->size));
	size -= fw->size;

	ret = ist30xx_cmd_run_device(client, true);
	fw_update_next_step(ret);

	ret = ist30xx_read_cmd(client, CMD_GET_CHECKSUM, &chksum);
	if (unlikely((ret) || (chksum != fw->chksum)))
		goto end;

	ret = ist30xx_read_cmd(client, CMD_GET_FW_VER, &ver);
	update_next_step(ret);
	fw->prev_core_ver = fw->core_ver;
	fw->core_ver = ver;
	tsp_info("F/W core version : %x\n", fw->core_ver);

	/* update parameters */
	ret = ist30xx_cmd_update(client, CMD_ENTER_UPDATE);
	param_update_next_step(ret);

	/* config */
	len = (data->tags.cfg_size >> 2);
	ret = ist30xx_write_buf(client, CMD_UPDATE_CONFIG, param, len);
	param_update_next_step(ret);
	msleep(10);

	param += len;
	size -= (len << 2);

	/* sensor */
	len = ((data->tags.sensor1_size + data->tags.sensor2_size) >> 2);
	ret = ist30xx_write_buf(client, CMD_UPDATE_SENSOR, param, len);
	param_update_next_step(ret);
	msleep(10);

	param += len;
	size -= (len << 2);

	ret = ist30xx_cmd_update(client, CMD_EXIT_UPDATE);
	param_update_next_step(ret);
	msleep(120);

	ret = ist30xx_cmd_run_device(client, true);
	update_next_step(ret);

	ret = ist30xx_read_cmd(client, CMD_GET_PARAM_VER, &ver);
	update_next_step(ret);
	fw->prev_param_ver = fw->param_ver;
	fw->param_ver = ver;

	ret = ist30xx_read_cmd(client, CMD_GET_SUB_VER, &ver);
	update_next_step(ret);
	fw->sub_ver = ver;

	tsp_info("Param version : %x, (sub: %x)\n", fw->param_ver, fw->sub_ver);

end:
	if (unlikely(ret)) {
		if (unlikely(step == 1))
			tsp_warn("Firmware update Fail!, ret=%d", ret);
		else if (unlikely(step == 2))
			tsp_warn("Parameters update Fail!, ret=%d", ret);
	} else if (unlikely(chksum != fw->chksum)) {
		tsp_warn("Error CheckSum: %08x(%08x)\n", chksum, fw->chksum);
		ret = -ENOEXEC;
	}

	ist30xx_enable_irq(data);
	data->status.update = 2;

	return ret;
}


#if IST30XX_INTERNAL_BIN
int ist30xx_unload_firmware(struct ist30xx_data *data)
{
	struct ist30xx_fw *fw = &data->fw;

	fw->fw_tsp_index = -1;	/* Set firmware index to invalid */
	if (fw->buf != NULL) {
		kfree(fw->buf);
		fw->buf = NULL;
	}
	fw->buf_size = 0;

	return 0;
}

int ist30xx_load_firmware(struct ist30xx_data *data, int fw_index)
{
	int err;
	struct ist30xx_fw *tsp_fw = &data->fw;
	struct ist30xx_config_info* fw_info;
	const struct firmware *fw = NULL;

	fw_info = &data->pdata->config_array[fw_index];

	if ((fw_index < 0) || (fw_index > data->pdata->config_array_size)) {
		tsp_err("Invalid fw index.\n");
		return -EINVAL;
	}

	if (tsp_fw->fw_tsp_index == fw_index) {
		tsp_debug("Firmware %s is already loaded.\n", fw_info->fw_name);
		return 0;
	}

	if (tsp_fw->fw_tsp_index != -1) {
		/* Another firmware is already loaded into the memory,
		 * which should be unloaded firstly */
		ist30xx_unload_firmware(data);
	}

	err = request_firmware(&fw, fw_info->fw_name, &data->client->dev);
	if (unlikely(err)) {
		tsp_err("Cannot load firmware %s.\n", fw_info->fw_name);
		return -EEXIST;
	} else {
		tsp_fw->buf = (u8 *)kmalloc((int)fw->size, GFP_KERNEL);
		if (unlikely(!tsp_fw->buf)) {
			tsp_err("Error allocating memory for firmware.\n");
			release_firmware(fw);
			return -ENOMEM;
		}

		memcpy(tsp_fw->buf, fw->data, (int)fw->size);
		tsp_fw->buf_size = (u32)fw->size;
		tsp_fw->fw_tsp_index = fw_index;	/* Firmware index */
		tsp_info("Firmware %s loaded successfully.\n", fw_info->fw_name);
		release_firmware(fw);
	}

	return 0;
}

int ist30xx_check_fw(struct ist30xx_data *data, const u8 *buf)
{
	int ret;
	u32 chksum;

	ret = ist30xx_read_cmd(data->client, CMD_GET_CHECKSUM, &chksum);
	if (unlikely(ret)) return ret;

	if (unlikely(chksum != data->fw.chksum)) {
		tsp_warn("Checksum compare error, (IC: %08x, Bin: %08x)\n",
			 chksum, data->fw.chksum);
		return -EPERM;
	}

	return 0;
}

bool ist30xx_check_valid_vendor(struct ist30xx_data *data, u32 tsp_vendor)
{
	int i;
	struct ist30xx_config_info *info = data->pdata->config_array;

	for (i = 0; i < data->pdata->config_array_size; i++) {
		if (tsp_vendor == info[i].tsp_type)
			return true;
	}

	return false;
}

#if IST30XX_MULTIPLE_TSP
void ist30xx_set_tsp_fw(struct ist30xx_data *data)
{
	int i;
	struct ist30xx_fw *fw = &data->fw;
	struct ist30xx_config_info *info = data->pdata->config_array;

	for (i = 0; i < data->pdata->config_array_size; i++) {
		if (data->tsp_type == info[i].tsp_type)
			break;
	}

	/* If no corresponding tsp firmware is found, just use the default one */
	if (i >= data->pdata->config_array_size) {
		tsp_warn("No corresponding TSP firmware found, use the default one(%x)\n", data->tsp_type);
		i = 0;
	} else {
		tsp_info("TSP vendor: %s(%x)\n", info[i].tsp_name, data->tsp_type);
	}

	ist30xx_load_firmware(data, i);

	ist30xx_get_update_info(data, fw->buf, fw->buf_size);
	data->fw_ver = ist30xx_parse_ver(data, FLAG_FW, fw->buf);
	data->param_ver = ist30xx_parse_ver(data, FLAG_PARAM, fw->buf);
	data->sub_ver = ist30xx_parse_ver(data, FLAG_SUB, fw->buf);
}
#endif  // IST30XX_MULTIPLE_TSP


int ist30xx_check_auto_update(struct ist30xx_data *data)
{
	int ret = 0;
	int retry = IST30XX_FW_UPDATE_RETRY;
	u32 tsp_type = TSP_TYPE_UNKNOWN;
	u32 chksum;
	bool tsp_check = false;
	struct ist30xx_fw *fw = &data->fw;

	while (retry--) {
		ret = ist30xx_read_cmd(data->client,
				       CMD_GET_TSP_PANNEL_TYPE, &tsp_type);
		if (likely(ret == 0)) {
			if (likely(ist30xx_check_valid_vendor(data, tsp_type) == true))
				tsp_check = true;
			break;
		}
	}

	retry = IST30XX_FW_UPDATE_RETRY;

	if (unlikely(!tsp_check)) {
		while (retry--) {
			/* FW recovery */
			tsp_info("tsp type: %x\n", tsp_type);
			ret = ist30xxb_isp_bootloader(data->client, fw->buf);
			if (likely(ret == 0)) {
				ist30xx_reset(data, false);
				ret = ist30xx_read_cmd(data->client,
						       CMD_GET_TSP_PANNEL_TYPE, &tsp_type);
				tsp_info("tsp type: %x\n", tsp_type);
				if (likely(ret == 0)) // recovery OK
					if (ist30xx_check_valid_vendor(data, tsp_type) == true) {
						data->tsp_type = tsp_type;
#if IST30XX_MULTIPLE_TSP
						ist30xx_set_tsp_fw(data);
#endif
						break;
					}
			}

			if (retry == 0) return 1;  /* TSP is not connected */
		}
	}

	ist30xx_cmd_run_device(data->client, false);

	ist30xx_get_ver_info(data);

	if (likely((fw->param_ver > 0) && (fw->param_ver < 0xFFFFFFFF))) {
		if (unlikely(((fw->core_ver & MASK_FW_VER) != IST30XX_FW_VER3) &&
			     ((fw->core_ver & MASK_FW_VER) != IST30XX_FW_VER4)))
			goto fw_check_end;

		/*
		 *  fw->core_ver : FW core version in TSP IC
		 *  fw->param_ver : FW version if TSP IC
		 *  data->fw_ver : FW core version in FW Binary
		 *  data->param_ver : FW version in FW Binary
		 */
		tsp_info("Version compare IC: %x(%x), BIN: %x(%x)\n",
			 fw->param_ver, fw->core_ver, data->param_ver, data->fw_ver);

		/* If FW version is same, check FW checksum */
		if (likely((fw->core_ver == data->fw_ver) &&
			   (fw->param_ver == data->param_ver))) {
			ret = ist30xx_read_cmd(data->client, CMD_GET_CHECKSUM, &chksum);
			if (unlikely((ret) || (chksum != fw->chksum))) {
				tsp_warn("Checksum error, IC: %x, Bin: %x (ret: %d)\n",
					 chksum, fw->chksum, ret);
				goto fw_check_end;
			}

			/* FW update will not operate only if ver of binary is equal to the ver of IC */
			return 0;
		}
	}

fw_check_end:
	return -EAGAIN;
}

int ist30xx_auto_bin_update(struct ist30xx_data *data)
{
	int ret = 0;
	int retry = IST30XX_FW_UPDATE_RETRY;
	struct ist30xx_fw *fw = &data->fw;

	fw->fw_tsp_index = -1;	/* Set initial firmware index to invalid */

#if IST30XX_MULTIPLE_TSP
	ist30xx_set_tsp_fw(data);
#else
	/* If multiple tsp is not enabled, just use the default firmware */
	ist30xx_load_firmware(data, 0);

	ist30xx_get_update_info(data, fw->buf, fw->buf_size);
	data->fw_ver = ist30xx_parse_ver(data, FLAG_FW, fw->buf);
	data->param_ver = ist30xx_parse_ver(data, FLAG_PARAM, fw->buf);
	data->sub_ver = ist30xx_parse_ver(data, FLAG_SUB, fw->buf);
#endif

	tsp_info("IC: %x, Binary ver core: %x, param: %x, sub: %x\n",
		 data->chip_id, data->fw_ver, data->param_ver, data->sub_ver);

	mutex_lock(&data->ist30xx_mutex);
	ret = ist30xx_check_auto_update(data);
	mutex_unlock(&data->ist30xx_mutex);
	if (likely(ret >= 0))
		return ret;

update_bin:   // TSP is not ready / FW update
	tsp_info("Update version. param(core): %x(%x, %x) -> %x(%x, %x)\n",
		 fw->param_ver, fw->core_ver, fw->sub_ver,
		 data->param_ver, data->fw_ver, data->sub_ver);

	mutex_lock(&data->ist30xx_mutex);
	while (retry--) {
		ret = ist30xx_fw_update(data->client, fw->buf, fw->buf_size, true);
		if (unlikely(!ret)) break;
	}
	mutex_unlock(&data->ist30xx_mutex);

	if (unlikely(ret))
		return ret;

	if (unlikely(retry > 0 && ist30xx_check_fw(data, fw->buf)))
		goto update_bin;

	ist30xx_calibrate(data, IST30XX_FW_UPDATE_RETRY);

	return ret;
}
#endif // IST30XX_INTERNAL_BIN


/* sysfs: /sys/class/touch/firmware/boot */
ssize_t ist30xx_boot_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	int ret;
	int fw_size = 0;
	u8 *fw = NULL;
	char *tmp;
	const struct firmware *request_fw = NULL;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	unsigned long mode = simple_strtoul(buf, &tmp, 10);

	if (mode == MASK_UPDATE_ISP || mode == MASK_UPDATE_FW) {
		ret = request_firmware(&request_fw, IST30XXB_FW_NAME, &data->client->dev);
		if (ret) {
			tsp_warn("File not found, %s\n", IST30XXB_FW_NAME);
			return size;
		}
		fw = (u8 *)request_fw->data;
		fw_size = (u32)request_fw->size;
	} else {
#if IST30XX_INTERNAL_BIN
		fw = data->fw.buf;
		fw_size = data->fw.buf_size;
#else
		return size;
#endif          // IST30XX_INTERNAL_BIN
	}

	if (mode == 3) {
		tsp_info("EEPROM all erase test\n");
		ist30xx_disable_irq(data);
		mutex_lock(&data->ist30xx_mutex);

		ist30xxb_isp_enable(data, true);
		ist30xxb_isp_erase(data->client, IST30XXB_ISP_ERASE_ALL, 0);
		ist30xxb_isp_enable(data, false);

		mutex_unlock(&data->ist30xx_mutex);
		ist30xx_enable_irq(data);
	} else {
		ist30xx_get_update_info(data, fw, fw_size);
		data->fw_ver = ist30xx_parse_ver(data, FLAG_FW, fw);

		ist30xx_disable_irq(data);

		mutex_lock(&data->ist30xx_mutex);
		ist30xxb_isp_bootloader(data->client, fw);
		mutex_unlock(&data->ist30xx_mutex);

		if (mode == MASK_UPDATE_ISP || mode == MASK_UPDATE_FW)
			release_firmware(request_fw);
	}

	return size;
}

u32 buf32_eeprom[IST30XX_EEPROM_SIZE / IST30XX_DATA_LEN];
ssize_t ist30xx_fw_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int i;
	u8 *buf8 = (u8 *)buf32_eeprom;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->ist30xx_mutex);
	ist30xx_disable_irq(data);

	ist30xxb_isp_fw_read(data->client, buf32_eeprom);
	for (i = 0; i < IST30XX_EEPROM_SIZE; i += 16) {
		tsp_debug("%07x: %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x\n", i,
			  buf8[i], buf8[i + 1], buf8[i + 2], buf8[i + 3],
			  buf8[i + 4], buf8[i + 5], buf8[i + 6], buf8[i + 7],
			  buf8[i + 8], buf8[i + 9], buf8[i + 10], buf8[i + 11],
			  buf8[i + 12], buf8[i + 13], buf8[i + 14], buf8[i + 15]);
	}

	ist30xx_enable_irq(data);
	mutex_unlock(&data->ist30xx_mutex);

	return 0;
}

/* sysfs: /sys/class/touch/firmware/fw_only */
ssize_t ist30xx_fw_only_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int ret;
	bool isp_mode = false;
	int fw_size = 0;
	u8 *fw = NULL;
	char *tmp;
	const struct firmware *request_fw = NULL;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	unsigned long mode = simple_strtoul(buf, &tmp, 10);

	if (mode == MASK_UPDATE_ISP || mode == MASK_UPDATE_FW) {
		ret = request_firmware(&request_fw, IST30XXB_FW_NAME, &data->client->dev);
		if (ret) {
			tsp_warn("File not found, %s\n", IST30XXB_FW_NAME);
			return size;
		}
		fw = (u8 *)request_fw->data;
		fw_size = (u32)request_fw->size;
		isp_mode = (mode == MASK_UPDATE_ISP ? true : false);
	} else {
#if IST30XX_INTERNAL_BIN
		fw = data->fw.buf;
		fw_size = data->fw.buf_size;
#else
		return size;
#endif          // IST30XX_INTERNAL_BIN
	}

	ist30xx_get_update_info(data, fw, fw_size);
	data->fw_ver = ist30xx_parse_ver(data, FLAG_FW, fw);
	data->param_ver = ist30xx_parse_ver(data, FLAG_PARAM, fw);
	data->sub_ver = ist30xx_parse_ver(data, FLAG_SUB, fw);

	mutex_lock(&data->ist30xx_mutex);
	ist30xx_fw_update(data->client, fw, fw_size, isp_mode);
	mutex_unlock(&data->ist30xx_mutex);

	ist30xx_init_touch_driver(data);

	if (mode == MASK_UPDATE_ISP || mode == MASK_UPDATE_FW)
		release_firmware(request_fw);

	return size;
}

/* sysfs: /sys/class/touch/firmware/firmware */
ssize_t ist30xx_fw_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t size)
{
	int ret;
	bool isp_mode = false;
	int fw_size = 0;
	u8 *fw = NULL;
	char *tmp;
	const struct firmware *request_fw = NULL;
	struct ist30xx_data *data = dev_get_drvdata(dev);
	unsigned long mode = simple_strtoul(buf, &tmp, 10);

	if (mode == MASK_UPDATE_ISP || mode == MASK_UPDATE_FW) {
		ret = request_firmware(&request_fw, IST30XXB_FW_NAME, &data->client->dev);
		if (ret) {
			tsp_warn("File not found, %s\n", IST30XXB_FW_NAME);
			return size;
		}
		fw = (u8 *)request_fw->data;
		fw_size = (u32)request_fw->size;
		isp_mode = (mode == MASK_UPDATE_ISP ? true : false);
	} else {
#if IST30XX_INTERNAL_BIN
		fw = data->fw.buf;
		fw_size = data->fw.buf_size;
#else
		return size;
#endif          // IST30XX_INTERNAL_BIN
	}

	ist30xx_get_update_info(data, fw, fw_size);
	data->fw_ver = ist30xx_parse_ver(data, FLAG_FW, fw);
	data->param_ver = ist30xx_parse_ver(data, FLAG_PARAM, fw);
	data->sub_ver = ist30xx_parse_ver(data, FLAG_SUB, fw);

	mutex_lock(&data->ist30xx_mutex);
	ist30xx_fw_update(data->client, fw, fw_size, isp_mode);
	mutex_unlock(&data->ist30xx_mutex);

	ist30xx_calibrate(data, 1);

	ist30xx_init_touch_driver(data);

	if (mode == MASK_UPDATE_ISP || mode == MASK_UPDATE_FW)
		release_firmware(request_fw);

	return size;
}

ssize_t ist30xx_fw_status(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int count;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	switch (data->status.update) {
	case 1:
		count = sprintf(buf, "Downloading\n");
		break;
	case 2:
		count = sprintf(buf, "Update success, ver %x(%x) -> %x(%x, %x), status : %d, gap : %d\n",
				data->fw.prev_param_ver, data->fw.prev_core_ver,
				data->fw.param_ver, data->fw.core_ver, data->fw.sub_ver,
				CALIB_TO_STATUS(data->status.calib_msg),
				CALIB_TO_GAP(data->status.calib_msg));
		break;
	default:
		count = sprintf(buf, "Pass\n");
	}

	return count;
}


/* sysfs: /sys/class/touch/firmware/fwparam */
ssize_t ist30xx_fwparam_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int ret;
	int fw_size = 0;
	u8 *fw = NULL;
	char *tmp;
	const struct firmware *request_fw = NULL;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	unsigned long mode = simple_strtoul(buf, &tmp, 10);

	if (mode == MASK_UPDATE_FW) {
		ret = request_firmware(&request_fw, IST30XXB_FW_NAME, &data->client->dev);
		if (unlikely(ret)) {
			tsp_warn("File not found, %s\n", IST30XXB_FW_NAME);
			return size;
		}
		fw = (u8 *)request_fw->data;
		fw_size = (u32)request_fw->size;
	} else {
#if IST30XX_INTERNAL_BIN
		fw = data->fw.buf;
		fw_size = data->fw.buf_size;
#else
		return size;
#endif          // IST30XX_INTERNAL_BIN
	}

	ist30xx_get_update_info(data, fw, fw_size);
	data->fw_ver = ist30xx_parse_ver(data, FLAG_FW, fw);
	data->param_ver = ist30xx_parse_ver(data, FLAG_PARAM, fw);

	mutex_lock(&data->ist30xx_mutex);
	ist30xx_fw_param_update(data->client, fw);
	mutex_unlock(&data->ist30xx_mutex);

	ist30xx_calibrate(data, 1);

	ist30xx_init_touch_driver(data);

	if (mode == MASK_UPDATE_FW)
		release_firmware(request_fw);

	return size;
}


/* sysfs: /sys/class/touch/firmware/viersion */
ssize_t ist30xx_fw_version(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	int count;
	struct ist30xx_data *data = dev_get_drvdata(dev);

	count = sprintf(buf, "ID: %x, f/w core: %x, param: %x, sub: %x\n",
			data->chip_id, data->fw.core_ver,
			data->fw.param_ver, data->fw.sub_ver);

#if IST30XX_INTERNAL_BIN
	{
		char msg[128];

		ist30xx_get_update_info(data, data->fw.buf, data->fw.buf_size);

		count += snprintf(msg, sizeof(msg),
				  " Header - f/w ver: %x, param: %x, sub: %x\r\n",
				  ist30xx_parse_ver(data, FLAG_FW, data->fw.buf),
				  ist30xx_parse_ver(data, FLAG_PARAM, data->fw.buf),
				  ist30xx_parse_ver(data, FLAG_SUB, data->fw.buf));
		strncat(buf, msg, sizeof(msg));
	}
#endif

	return count;
}


/* sysfs  */
static DEVICE_ATTR(fw_only, (S_IRUGO | S_IWUSR | S_IWGRP), ist30xx_fw_status, ist30xx_fw_only_store);
static DEVICE_ATTR(firmware, (S_IRUGO | S_IWUSR | S_IWGRP), ist30xx_fw_status, ist30xx_fw_store);
static DEVICE_ATTR(fwparam, (S_IRUGO | S_IWUSR | S_IWGRP), NULL, ist30xx_fwparam_store);
static DEVICE_ATTR(boot, (S_IRUGO | S_IWUSR | S_IWGRP), ist30xx_fw_show, ist30xx_boot_store);
static DEVICE_ATTR(version, S_IRUGO, ist30xx_fw_version, NULL);

static struct attribute *fw_attributes[] = {
	&dev_attr_fw_only.attr,
	&dev_attr_firmware.attr,
	&dev_attr_fwparam.attr,
	&dev_attr_boot.attr,
	&dev_attr_version.attr,
	NULL,
};

static struct attribute_group fw_attr_group = {
	.attrs	= fw_attributes,
};

int ist30xx_init_update_sysfs(struct ist30xx_data *data)
{
	/* /sys/class/touch */
	data->ist30xx_class = class_create(THIS_MODULE, "touch");

	/* /sys/class/touch/firmware */
	data->fw_dev = device_create(data->ist30xx_class, NULL, 0, data, "firmware");

	/* /sys/class/touch/firmware/... */
	if (unlikely(sysfs_create_group(&data->fw_dev->kobj,
					&fw_attr_group)))
		tsp_err("Failed to create sysfs group(%s)!\n", "firmware");

	data->status.update = 0;
	data->status.calib = 0;

	return 0;
}
