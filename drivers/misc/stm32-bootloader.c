/*
 * Protocol used by STM32 bootloader
 *
 * This file is extracted from project stm32flash:
 *   stm32flash - Open Source ST STM32 flash program for *nix
 *   Copyright (C) 2010 Geoffrey McRae <geoff@spacevs.com>
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Modified by Antonio Borneo <borneo.antonio@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/i2c.h>

#include <asm/byteorder.h>

#include "stm32-fwu-i2c.h"
#include "stm32-bootloader.h"

/*
 * The length of reply for some commands is dependent on firmware version.
 * On byte oriented interfaces, like UART, we can read the lenght from the
 * first byte of the reply.
 * On frame oriented interfaces, like I2C, the lenght is part of the frame
 * so we can only guess the length.
 * Guessed lenght below is the value expected in first byte of frame.
 */
#define STM32_CMD_GLEN_GET	18
#define STM32_CMD_GLEN_GID	1

#define STM32_RESYNC_TIMEOUT_MS	5000

/* Bootloader commands */
#define STM32_CMD_GET	0x00	/* get the version and command supported */
#define STM32_CMD_GVR	0x01	/* get version and read protection status */
#define STM32_CMD_GID	0x02	/* get ID */
#define STM32_CMD_RM	0x11	/* read memory */
#define STM32_CMD_GO	0x21	/* go */
#define STM32_CMD_WM	0x31	/* write memory */
#define STM32_CMD_NS_WM	0x32	/* no-stretch write memory */
#define STM32_CMD_ER	0x43	/* erase */
#define STM32_CMD_EE	0x44	/* extended erase */
#define STM32_CMD_NS_ER	0x45	/* no-stretch erase */
#define STM32_CMD_WP	0x63	/* write protect */
#define STM32_CMD_NS_WP	0x64	/* no-stretch write protect */
#define STM32_CMD_UW	0x73	/* write unprotect */
#define STM32_CMD_NS_UW	0x74	/* no-stretch write unprotect */
#define STM32_CMD_RP	0x82	/* readout protect */
#define STM32_CMD_NS_RP	0x83	/* no-stretch readout protec */
#define STM32_CMD_UR	0x92	/* readout unprotect */
#define STM32_CMD_NS_UR	0x93	/* no-stretch readout unprotect */
#define STM32_CMD_CRC	0xA1	/* compute CRC */

#define STM32_CMD_INIT	0x7F	/* initialize UART */
#define STM32_CMD_ERR	0xFF	/* not a valid command */

/* Bootloader reply */
#define STM32_NACK	0x1F
#define STM32_BUSY	0x76
#define STM32_ACK	0x79

static int stm32_bl_getack(struct device *dev)
{
	struct stm32_i2c_platform_data *pdata = dev->platform_data;
	int ret;
	u8 buf[1];

	do {
		ret = pdata->recv(dev, buf, 1);
		if (ret < 0)
			return ret;
		if (ret == 1 && buf[0] != STM32_BUSY)
			break;
	} while (1);

	if (buf[0] == STM32_ACK)
		return 0;

	if (buf[0] == STM32_NACK) {
		dev_err(dev, "Got NACK\n");
		return STM32_NACK;
	}
	dev_err(dev, "Unexpected reply 0x%02x waiting ACK\n", buf[0]);
	return -EPROTO;
}

static int stm32_bl_send_frame(struct device *dev, u8 *data, size_t len)
{
	struct stm32_i2c_platform_data *pdata = dev->platform_data;
	int ret;

	ret = pdata->send(dev, data, len);
	if (ret < 0)
		return ret;

	return stm32_bl_getack(dev);
}

static int stm32_bl_send_command(struct device *dev, u8 cmd)
{
	u8 buf[2];

	buf[0] = cmd;
	buf[1] = cmd ^ 0xff;
	return stm32_bl_send_frame(dev, buf, 2);
}

int stm32_bl_resync(struct device *dev)
{
	unsigned long deadline;
	int ret;

	deadline = jiffies + msecs_to_jiffies(STM32_RESYNC_TIMEOUT_MS);
	do {
		ret = stm32_bl_send_command(dev, STM32_CMD_ERR);
		if (ret == STM32_NACK)
			return 0;
		if (time_is_before_jiffies(deadline))
			return -ETIMEDOUT;
		msleep(20);
	} while (1);
}

static int stm_32_bl_cmdvarlen(struct device *dev, int cmd, int len, u8 *buf)
{
	struct stm32_i2c_platform_data *pdata = dev->platform_data;
	int ret;

	do {
		ret = stm32_bl_send_command(dev, cmd);
		if (ret)
			return ret;
		ret = pdata->recv(dev, buf, len + 2);
		if (ret < 0)
			return ret;
		if (ret && len != buf[0]) {
			/* incorrect guessed length; retry */
			len = buf[0];
			ret = stm32_bl_resync(dev);
			if (ret) {
				if (!pdata->hw_reset)
					return -EIO;
				ret = pdata->hw_reset(dev, true);
				if (ret)
					return ret;
			}
			continue;
		}
		if (ret != len + 2)
			return -EIO;

		ret = stm32_bl_getack(dev);
		if (ret == STM32_NACK)
			return -EIO;
		return ret;
	} while (1);
}

int stm32_bl_init(struct device *dev)
{
	struct stm32_i2c_platform_data *pdata = dev->platform_data;
	int ret, len, i;
	u32 chip_id;
	u8 buf[256 + 2], cmd;

	if (pdata->hw_reset) {
		ret = pdata->hw_reset(dev, true);
		if (ret)
			return ret;
	}

	/* Command Get BL Version */
	ret = stm32_bl_send_command(dev, STM32_CMD_GVR);
	if (ret < 0)
		return ret;
    pr_info("Send get bl version cmd\n");
	/* From AN, only UART bootloader returns 3 bytes */
	len = 1;
	ret = pdata->recv(dev, buf, len);
	if (ret < 0)
		return ret;
	ret = stm32_bl_getack(dev);
	if (ret)
		return ret;
	pdata->bootloader_ver = buf[0];
    pr_info("bootloader_ver = %d\n", pdata->bootloader_ver);
	/* Command GET */
	len = STM32_CMD_GLEN_GET;
	if (pdata->cmd_get_reply)
		for (i = 0 ; pdata->cmd_get_reply[i].length ; i++)
			if (pdata->bootloader_ver
			    == pdata->cmd_get_reply[i].version) {
				len = pdata->cmd_get_reply[i].length;
				break;
			}
	ret = stm_32_bl_cmdvarlen(dev, STM32_CMD_GET, len, buf);
	if (ret < 0)
		return ret;
	if (len != buf[0]) {
		len = buf[0];
		dev_info(dev, "Correct length of cmd GET is %d.\n", len);
	}
	if (pdata->bootloader_ver != buf[1])
		dev_info(dev,
			 "Received different bootloader version: gvr(0x%02x), get(0x%02x).\n",
			 pdata->bootloader_ver, buf[1]);
	for (i = 0 ; i < len ; i++) {
		cmd = buf[i + 2];
		switch (cmd) {
		case STM32_CMD_RM:
			pdata->cmd_rm = cmd;
			break;
		case STM32_CMD_GO:
			pdata->cmd_go = cmd;
			break;
		case STM32_CMD_WM:
		case STM32_CMD_NS_WM:
			/* prefer newer (higher code) command */
			pdata->cmd_wm = max(pdata->cmd_wm, cmd);
			break;
		case STM32_CMD_ER:
		case STM32_CMD_EE:
		case STM32_CMD_NS_ER:
			pdata->cmd_er = max(pdata->cmd_er, cmd);
			break;
		case STM32_CMD_WP:
		case STM32_CMD_NS_WP:
			pdata->cmd_wp = max(pdata->cmd_wp, cmd);
			break;
		case STM32_CMD_UW:
		case STM32_CMD_NS_UW:
			pdata->cmd_uw = max(pdata->cmd_uw, cmd);
			break;
		case STM32_CMD_RP:
		case STM32_CMD_NS_RP:
			pdata->cmd_rp = max(pdata->cmd_rp, cmd);
			break;
		case STM32_CMD_UR:
		case STM32_CMD_NS_UR:
			pdata->cmd_ur = max(pdata->cmd_ur, cmd);
			break;
		case STM32_CMD_CRC:
			pdata->cmd_crc = cmd;
			break;
		default:
			/* ignore it */
			break;
		}
	}

	/* Command GET chip ID */
	ret = stm_32_bl_cmdvarlen(dev, STM32_CMD_GID, STM32_CMD_GLEN_GID, buf);
	if (ret < 0)
		return ret;
	len = buf[0];
	if (len != STM32_CMD_GLEN_GID)
		dev_info(dev, "Correct length of cmd GET_ID is %d.\n", len);
	chip_id = 0;
	for (i = 0 ; i < 4 && i <= len ; i++)
		chip_id = (chip_id << 8) | buf[i + 1];
	pdata->chip_id = chip_id;

	return 0;
}

int stm32_bl_read_memory(struct device *dev, uint32_t address, u8 *data,
			 size_t len)
{
	struct stm32_i2c_platform_data *pdata = dev->platform_data;
	int ret;
	u8 buf[5];

	if ((len == 0) || (len > STM32_MAX_BUFFER) || !data ||
	    !IS_ALIGNED(address, 4))
		return -EINVAL;

	ret = stm32_bl_send_command(dev, pdata->cmd_rm);
	if (ret)
		return ret;

	buf[0] = (address >> 24) & 0xff;
	buf[1] = (address >> 16) & 0xff;
	buf[2] = (address >> 8) & 0xff;
	buf[3] = address & 0xff;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];
	ret = stm32_bl_send_frame(dev, buf, 5);
	if (ret)
		return ret;

	ret = stm32_bl_send_command(dev, len - 1);
	if (ret)
		return ret;

	return pdata->recv(dev, data, len);
}

int stm32_bl_write_memory(struct device *dev, uint32_t address, const u8 *data,
			  size_t len)
{
	struct stm32_i2c_platform_data *pdata = dev->platform_data;
	int ret, i, checksum;
	size_t aligned_len;
	u8 buf[STM32_MAX_BUFFER + 2], cmd;

	if ((len == 0) || (len > STM32_MAX_BUFFER) || !data ||
	    !IS_ALIGNED(address, 4))
		return -EINVAL;

	cmd = pdata->cmd_wm;
	if (cmd == 0)
		return -EOPNOTSUPP;

	ret = stm32_bl_send_command(dev, cmd);
	if (ret)
		return ret;

	buf[0] = (address >> 24) & 0xff;
	buf[1] = (address >> 16) & 0xff;
	buf[2] = (address >> 8) & 0xff;
	buf[3] = address & 0xff;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];
	ret = stm32_bl_send_frame(dev, buf, 5);
	if (ret)
		return ret;

	aligned_len = ALIGN(len, 4);
	buf[0] = aligned_len - 1;
	checksum = aligned_len - 1;
	for (i = 0 ; i < len ; i++) {
		buf[i + 1] = data[i];
		checksum ^= data[i];
	}
	for (i = len ; i < aligned_len ; i++) {
		buf[i + 1] = 0xff;
		checksum ^= 0xff;
	}
	buf[aligned_len + 1] = checksum;

	return stm32_bl_send_frame(dev, buf, aligned_len + 2);
}

int stm32_bl_mass_erase(struct device *dev)
{
	struct stm32_i2c_platform_data *pdata = dev->platform_data;
	int ret;
	u8 buf[3], cmd;

	cmd = pdata->cmd_er;
	if (cmd == 0)
		return -EOPNOTSUPP;

	ret = stm32_bl_send_command(dev, cmd);
	if (ret)
		return ret;

	if (cmd == STM32_CMD_ER)
		return stm32_bl_send_command(dev, 0xff);

	buf[0] = 0xff;
	buf[1] = 0xff;
	buf[2] = 0x00;
	/* FIXME: Bootloader v.1.0 has to be handled separately */
	if (pdata->bootloader_ver > 0x10)
		return stm32_bl_send_frame(dev, buf, 3);

	/*
	 * just send command, wait, dont't read ACK but resync
	 * if something goes wrong, we will notice at verify
	 */
	ret = pdata->send(dev, buf, 3);
	if (ret < 0)
		return ret;

	msleep(10000);

	return stm32_bl_resync(dev);
}

int stm32_bl_go(struct device *dev, uint32_t address)
{
	struct stm32_i2c_platform_data *pdata = dev->platform_data;
	int ret;
	u8 buf[5], cmd;

	cmd = pdata->cmd_go;
	if (cmd == 0)
		return -EOPNOTSUPP;

	ret = stm32_bl_send_command(dev, cmd);
	if (ret)
		return ret;

	buf[0] = (address >> 24) & 0xff;
	buf[1] = (address >> 16) & 0xff;
	buf[2] = (address >> 8) & 0xff;
	buf[3] = address & 0xff;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];
	return stm32_bl_send_frame(dev, buf, 5);
}

int stm32_bl_crc_memory(struct device *dev, uint32_t address, size_t len,
			uint32_t *crc)
{
	struct stm32_i2c_platform_data *pdata = dev->platform_data;
	int ret;
	u8 buf[5], cmd;

	cmd = pdata->cmd_crc;
	if (cmd == 0)
		return -EOPNOTSUPP;

	ret = stm32_bl_send_command(dev, cmd);
	if (ret)
		return ret;

	buf[0] = (address >> 24) & 0xff;
	buf[1] = (address >> 16) & 0xff;
	buf[2] = (address >> 8) & 0xff;
	buf[3] = address & 0xff;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];
	ret = stm32_bl_send_frame(dev, buf, 5);
	if (ret)
		return ret;

	buf[0] = (len >> 24) & 0xff;
	buf[1] = (len >> 16) & 0xff;
	buf[2] = (len >> 8) & 0xff;
	buf[3] = len & 0xff;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];
	ret = stm32_bl_send_frame(dev, buf, 5);
	if (ret)
		return ret;

	ret = stm32_bl_getack(dev);
	if (ret)
		return ret;

	ret = pdata->recv(dev, buf, 5);
	if (ret < 0)
		return ret;

	if (buf[4] != (buf[0] ^ buf[1] ^ buf[2] ^ buf[3]))
		return -EPROTO;

	*crc = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
	return 0;
}

#define CRCPOLY_BE      0x04c11db7
#define CRC_MSBMASK     0x80000000
#define CRC_INIT_VALUE  0xFFFFFFFF
uint32_t stm32_bl_sw_crc(uint32_t crc, const u8 *buf, unsigned int len)
{
	int i;
	uint32_t data;

	while (len) {
		data = *buf++;
		data |= ((len > 1) ? *buf++ : 0xff) << 8;
		data |= ((len > 2) ? *buf++ : 0xff) << 16;
		data |= ((len > 3) ? *buf++ : 0xff) << 24;
		len = len > 3 ? len - 4 : 0;

		crc ^= data;

		for (i = 0; i < 32; i++)
			if (crc & CRC_MSBMASK)
				crc = (crc << 1) ^ CRCPOLY_BE;
			else
				crc = (crc << 1);
	}
	return crc;
}
