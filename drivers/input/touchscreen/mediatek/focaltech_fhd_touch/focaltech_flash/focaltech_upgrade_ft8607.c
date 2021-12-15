/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

/*****************************************************************************
 *
 * File Name: focaltech_upgrade_ft8607.c
 *
 * Author: Focaltech Driver Team
 *
 * Created: 2016-08-15
 *
 * Abstract:
 *
 * Reference:
 *
 *****************************************************************************/

/*****************************************************************************
 * 1.Included header files
 *****************************************************************************/
#include "../focaltech_flash.h"

/*****************************************************************************
 * Global variable or extern global variabls/functions
 *****************************************************************************/
u8 pb_file_ft8607[] = {
#include "../include/pramboot/FT8607_Pramboot_V0.7_20180725.i"
};

/*****************************************************************************
 * Private constant and macro definitions using #define
 *****************************************************************************/
#define MAX_BANK_DATA 0x80
#define MAX_GAMMA_LEN 0x180
#define LIC_BANK_LEN_OFF 0
#define LIC_BANK_HEAD_LEN 0
#define LIC_BANK_START_ADDR 0x02
/* calculate lcd init code checksum */
static unsigned short cal_lcdinitcode_checksum(u8 *ptr, int initcode_length)
{
	u32 checksum = 0;
	u8 *p_block_addr;
	u16 block_len;
	u16 param_num;
	int i;
	int index = 2;

	while (index < initcode_length - 6) {
		p_block_addr = ptr + index;
		block_len = ((p_block_addr[0] << 8) + p_block_addr[1]) * 2;
		param_num = block_len - 4;

		checksum += (param_num * (param_num - 1) / 2);

		checksum += p_block_addr[3];

		for (i = 0; i < param_num; ++i)
			checksum += p_block_addr[4 + i];


		checksum &= 0xffff;

		index += block_len;
	}

	return checksum;
}

static int print_data(u8 *buf, u32 len)
{
	int i = 0;
	int n = 0;
	u8 *p = NULL;

	p = kzalloc(len * 4, GFP_KERNEL);
	for (i = 0; i < len; i++)
		n += snprintf(p + n, PAGE_SIZE, "%02x ", buf[i]);


	FTS_DEBUG("%s", p);

	kfree(p);
	return 0;
}

static int find_bank(u8 *initcode, u16 bank_start_addr, u8 *bank_sign,
		     u16 *bank_pos)
{
	u16 pos = bank_start_addr;
	u16 initcode_len = 0;

	initcode_len = ((u16)(((u16)initcode[0]) << 8) + initcode[1]) * 2;
	if ((initcode_len >= FTS_MAX_LEN_SECTOR) ||
	    (initcode_len <= FTS_MIN_LEN)) {
		FTS_ERROR("host lcd init code len(%x) is too large",
			  initcode_len);
		return -EINVAL;
	}
	FTS_INFO("lic saddr:%x, len:%d", pos, initcode_len);

	while (pos < initcode_len) {
		if ((initcode[pos] == bank_sign[0]) &&
		    (initcode[pos + 1] == bank_sign[1]) &&
		    (initcode[pos + 2] == bank_sign[2]) &&
		    (initcode[pos + 3] == bank_sign[3])) {
			FTS_INFO("bank(%x %x %x %x) find", bank_sign[0],
				 bank_sign[1], bank_sign[2], bank_sign[3]);
			*bank_pos = pos;
			return 0;
		}
		pos += (((u16)initcode[pos + LIC_BANK_LEN_OFF] << 8) +
				initcode[pos + LIC_BANK_LEN_OFF + 1]) *
				       2 +
			       LIC_BANK_HEAD_LEN;

	}

	return -ENODATA;
}

static int read_3gamma(u8 **gamma, u16 *len)
{
	int ret = 0;
	int i = 0;
	int packet_num = 0;
	int packet_len = 0;
	int remainder = 0;
	u8 cmd[4] = {0};
	u32 addr = 0x01D000;
	u8 gamma_header[0x20] = {0};
	u16 gamma_len = 0;
	u16 gamma_len_n = 0;
	u8 *pgamma = NULL;
	int j = 0;
	u8 gamma_ecc = 0;

	cmd[0] = 0x05;
	cmd[1] = 0x80;
	cmd[2] = 0x01;
	ret = fts_write(cmd, 3);
	if (ret < 0) {
		FTS_ERROR("get flash type and clock fail !");
		return ret;
	}

	cmd[0] = 0x03;
	cmd[1] = (u8)(addr >> 16);
	cmd[2] = (u8)(addr >> 8);
	cmd[3] = (u8)addr;
	ret = fts_write(cmd, 4);
	msleep(20);

	ret = fts_read(NULL, 0, gamma_header, 0x20);
	if (ret < 0) {
		FTS_ERROR("read 3-gamma header fail");
		return ret;
	}

	gamma_len = (u16)((u16)gamma_header[0] << 8) + gamma_header[1];
	gamma_len_n = (u16)((u16)gamma_header[2] << 8) + gamma_header[3];

	if ((gamma_len + gamma_len_n) != 0xFFFF) {
		FTS_INFO("gamma length check fail:%x %x", gamma_len, gamma_len);
		return -EIO;
	}

	if ((gamma_header[4] + gamma_header[5]) != 0xFF) {
		FTS_INFO("gamma ecc check fail:%x %x", gamma_header[4],
			 gamma_header[5]);
		return -EIO;
	}

	if (gamma_len > MAX_GAMMA_LEN) {
		FTS_ERROR("gamma data len(%d) is too long", gamma_len);
		return -EINVAL;
	}

	*gamma = kzalloc(MAX_GAMMA_LEN, GFP_KERNEL);
	if (*gamma == NULL) {
		FTS_ERROR("malloc gamma memory fail");
		return -ENOMEM;
	}
	pgamma = *gamma;

	packet_num = gamma_len / 256;
	packet_len = 256;
	remainder = gamma_len % 256;
	if (remainder)
		packet_num++;
	FTS_INFO("3-gamma len:%d", gamma_len);
	cmd[0] = 0x03;
	addr += 0x20;
	for (i = 0; i < packet_num; i++) {
		addr += i * 256;
		cmd[1] = (u8)(addr >> 16);
		cmd[2] = (u8)(addr >> 8);
		cmd[3] = (u8)addr;
		if ((i == packet_num - 1) && remainder)
			packet_len = remainder;
		ret = fts_write(cmd, 4);
		msleep(20);

		ret = fts_read(NULL, 0, pgamma + i * 256, packet_len);
		if (ret < 0) {
			FTS_ERROR("read 3-gamma data fail");
			return ret;
		}
	}

	/*  ecc */
	for (j = 0; j < gamma_len; j++)
		gamma_ecc ^= pgamma[j];

	FTS_INFO("back_3gamma_ecc: 0x%x, 0x%x", gamma_ecc, gamma_header[0x04]);
	if (gamma_ecc != gamma_header[0x04]) {
		FTS_ERROR("back gamma ecc check fail:%x %x", gamma_ecc,
			  gamma_header[0x04]);
		return -EIO;
	}

	*len = gamma_len;

	FTS_DEBUG("read 3-gamma data:");
	print_data(*gamma, gamma_len);

	return 0;
}

static int replace_3gamma(u8 *initcode, u8 *gamma, u16 gamma_len)
{
	int ret = 0;
	u16 gamma_pos = 0;
	int i = 0;
	u8 gamma_bank[4] = {0x00, 0x20, 0x82, 0xD1};
	u16 bank_addr = 0;
	u16 bank_saddr = LIC_BANK_START_ADDR;
	u16 bank_len = 0;

	FTS_FUNC_ENTER();
	/* bank Gamma */
	for (i = 0; i < 6; i++) {
		ret = find_bank(initcode, bank_saddr, gamma_bank, &bank_addr);
		if (ret < 0) {
			FTS_ERROR("find gamma bank fail");
			goto find_gamma_bank_err;
		}
		bank_len = (((u16)initcode[bank_addr + LIC_BANK_LEN_OFF] << 8) +
			    initcode[bank_addr + LIC_BANK_LEN_OFF + 1]) *
				   2 +
			   LIC_BANK_HEAD_LEN;
		memcpy(initcode + bank_addr, gamma + gamma_pos, bank_len);

		bank_saddr = bank_addr;
		gamma_pos += bank_len;
		gamma_bank[3] += 0x01;
	}

	FTS_DEBUG("replace 3-gamma data:");
	print_data(initcode + 0x468, gamma_len);

	FTS_FUNC_EXIT();
	return 0;

find_gamma_bank_err:
	FTS_INFO("3-gamma bank(%02x %02x %02x %02x) not find", gamma[gamma_pos],
		 gamma[gamma_pos + 1], gamma[gamma_pos + 2],
		 gamma[gamma_pos + 3]);
	return -ENODATA;
}

static int read_replace_3gamma(u8 *buf)
{
	int ret = 0;
	u16 initcode_checksum = 0;
	u8 *gamma = NULL;
	u16 gamma_len = 0;
	u16 hlic_len = 0;

	FTS_FUNC_ENTER();

	ret = read_3gamma(&gamma, &gamma_len);
	if (ret < 0) {
		FTS_INFO("no valid 3-gamma data, not replace");
		kfree(gamma);
		return 0;
	}

	ret = replace_3gamma(buf, gamma, gamma_len);
	if (ret < 0) {
		FTS_ERROR("replace 3-gamma fail");
		kfree(gamma);
		return ret;
	}

	hlic_len = ((u16)(((u16)buf[0]) << 8) + buf[1]) * 2;
	if ((hlic_len >= FTS_MAX_LEN_SECTOR) || (hlic_len <= FTS_MIN_LEN)) {
		FTS_ERROR("host lcd init code len(%x) is too large", hlic_len);
		return -EINVAL;
	}
	initcode_checksum = cal_lcdinitcode_checksum(buf, hlic_len);
	FTS_INFO("lcd init code calc checksum:0x%04x", initcode_checksum);
	buf[hlic_len - 2] = (u8)(initcode_checksum >> 8);
	buf[hlic_len - 1] = (u8)(initcode_checksum);

	FTS_FUNC_EXIT();

	kfree(gamma);
	return 0;
}

static int check_initial_code_valid(u8 *buf)
{
	u16 initcode_checksum = 0;
	u16 buf_checksum = 0;
	u16 hlic_len = 0;

	hlic_len = ((u16)(((u16)buf[0]) << 8) + buf[1]) * 2;
	if ((hlic_len >= FTS_MAX_LEN_SECTOR) || (hlic_len <= FTS_MIN_LEN)) {
		FTS_ERROR("host lcd init code len(%x) is too large", hlic_len);
		return -EINVAL;
	}

	initcode_checksum = cal_lcdinitcode_checksum(buf, hlic_len);
	buf_checksum = ((u16)((u16)buf[hlic_len - 2] << 8) + buf[hlic_len - 1]);
	FTS_INFO("lcd init code calc checksum:0x%04x,0x%04x", initcode_checksum,
		 buf_checksum);
	if (initcode_checksum != buf_checksum) {
		FTS_ERROR("Initial Code checksum fail");
		return -EINVAL;
	}
	return 0;
}

/************************************************************************
 * fts_ft8607_upgrade_mode -
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_ft8607_upgrade_mode(enum FW_FLASH_MODE mode, u8 *buf, u32 len)
{
	int ret = 0;
	u32 start_addr = 0;
	u8 cmd[4] = {0};
	u32 delay = 0;
	int ecc_in_host = 0;
	int ecc_in_tp = 0;

	if ((buf == NULL) || (len < FTS_MIN_LEN)) {
		FTS_ERROR("buffer/len(%x) is invalid", len);
		return -EINVAL;
	}

	/* enter into upgrade environment */
	ret = fts_fwupg_enter_into_boot();
	if (ret < 0) {
		FTS_ERROR("enter into pramboot/bootloader fail,ret=%d", ret);
		goto fw_reset;
	}

	cmd[0] = FTS_CMD_FLASH_MODE;
	cmd[1] = FLASH_MODE_UPGRADE_VALUE;
	start_addr = upgrade_func_ft8607.appoff;
	if (mode == FLASH_MODE_LIC) {
		/* lcd initial code upgrade */
		ret = read_replace_3gamma(buf);
		if (ret < 0) {
			FTS_ERROR(
				"replace 3-gamma fail, not upgrade lcd init code");
			goto fw_reset;
		}
		cmd[1] = FLASH_MODE_LIC_VALUE;
		start_addr = upgrade_func_ft8607.licoff;
	} else if (mode == FLASH_MODE_PARAM) {
		cmd[1] = FLASH_MODE_PARAM_VALUE;
		start_addr = upgrade_func_ft8607.paramcfgoff;
	}
	FTS_INFO("flash mode:0x%02x, start addr=0x%04x", cmd[1], start_addr);

	ret = fts_write(cmd, 2);
	if (ret < 0) {
		FTS_ERROR("upgrade mode(09) cmd write fail");
		goto fw_reset;
	}

	delay = FTS_ERASE_SECTOR_DELAY * (len / FTS_MAX_LEN_SECTOR);
	ret = fts_fwupg_erase(delay);
	if (ret < 0) {
		FTS_ERROR("erase cmd write fail");
		goto fw_reset;
	}

	/* write app */
	ecc_in_host = fts_flash_write_buf(start_addr, buf, len, 1);
	if (ecc_in_host < 0) {
		FTS_ERROR("lcd initial code write fail");
		goto fw_reset;
	}

	/* ecc */
	ecc_in_tp = fts_fwupg_ecc_cal(start_addr, len);
	if (ecc_in_tp < 0) {
		FTS_ERROR("ecc read fail");
		goto fw_reset;
	}

	FTS_INFO("ecc in tp:%x, host:%x", ecc_in_tp, ecc_in_host);
	if (ecc_in_tp != ecc_in_host) {
		FTS_ERROR("ecc check fail");
		goto fw_reset;
	}

	FTS_INFO("upgrade success, reset to normal boot");
	ret = fts_fwupg_reset_in_boot();
	if (ret < 0)
		FTS_ERROR("reset to normal boot fail");


	msleep(400);
	return 0;

fw_reset:
	return -EIO;
}

/*
 * fts_get_hlic_ver - read host lcd init code version
 *
 * return 0 if host lcd init code is valid, otherwise return error code
 */
static int fts_ft8607_get_hlic_ver(u8 *initcode)
{
	u8 *hlic_buf = initcode;
	u16 hlic_len = 0;
	u8 hlic_ver[2] = {0};

	hlic_len = ((u16)(((u16)hlic_buf[0]) << 8) + hlic_buf[1]) * 2;
	FTS_INFO("host lcd init code len:%x", hlic_len);
	if ((hlic_len >= FTS_MAX_LEN_SECTOR) || (hlic_len <= FTS_MIN_LEN)) {
		FTS_ERROR("host lcd init code len(%x) is too large", hlic_len);
		return -EINVAL;
	}

	hlic_ver[0] = hlic_buf[hlic_len];
	hlic_ver[1] = hlic_buf[hlic_len + 1];

	FTS_INFO("host lcd init code ver:%x %x", hlic_ver[0], hlic_ver[1]);
	if (0xFF != (hlic_ver[0] + hlic_ver[1])) {
		FTS_ERROR("host lcd init code version check fail");
		return -EINVAL;
	}

	return hlic_ver[0];
}

/************************************************************************
 * Name: fts_ft8607_upgrade
 * Brief:
 * Input:
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_ft8607_upgrade(u8 *buf, u32 len)
{
	int ret = 0;
	u8 *tmpbuf = NULL;
	u32 app_len = 0;

	FTS_INFO("fw app upgrade...");
	if (buf == NULL) {
		FTS_ERROR("fw buf is null");
		return -EINVAL;
	}

	if ((len < FTS_MIN_LEN) || (len > FTS_MAX_LEN_FILE)) {
		FTS_ERROR("fw buffer len(%x) fail", len);
		return -EINVAL;
	}

	app_len = len - upgrade_func_ft8607.appoff;
	tmpbuf = buf + upgrade_func_ft8607.appoff;
	ret = fts_ft8607_upgrade_mode(FLASH_MODE_APP, tmpbuf, app_len);
	if (ret < 0) {
		FTS_INFO("fw upgrade fail,reset to normal boot");
		if (fts_fwupg_reset_in_boot() < 0)
			FTS_ERROR("reset to normal boot fail");

		return ret;
	}

	return 0;
}

/************************************************************************
 * Name: fts_ft8607_lic_upgrade
 * Brief:
 * Input:
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_ft8607_lic_upgrade(u8 *buf, u32 len)
{
	int ret = 0;
	u8 *tmpbuf = NULL;
	u32 lic_len = 0;

	FTS_INFO("lcd initial code upgrade...");
	if (buf == NULL) {
		FTS_ERROR("lcd initial code buffer is null");
		return -EINVAL;
	}

	if ((len < FTS_MIN_LEN) || (len > FTS_MAX_LEN_FILE)) {
		FTS_ERROR("lcd initial code buffer len(%x) fail", len);
		return -EINVAL;
	}

	ret = check_initial_code_valid(buf);
	if (ret < 0) {
		FTS_ERROR("initial code invalid, not upgrade lcd init code");
		return -EINVAL;
	}



	lic_len = FTS_MAX_LEN_SECTOR;
	tmpbuf = kzalloc(lic_len, GFP_KERNEL);
	if (tmpbuf == NULL) {
		FTS_INFO("initial code buf malloc fail");
		return -EINVAL;
	}
	memcpy(tmpbuf, buf, lic_len);

	ret = fts_ft8607_upgrade_mode(FLASH_MODE_LIC, tmpbuf, lic_len);
	if (ret < 0) {
		FTS_INFO("lcd initial code upgrade fail,reset to normal boot");
		if (fts_fwupg_reset_in_boot() < 0)
			FTS_ERROR("reset to normal boot fail");


		kfree(tmpbuf);
		tmpbuf = NULL;

		return ret;
	}


	kfree(tmpbuf);
	tmpbuf = NULL;

	return 0;
}

/************************************************************************
 * Name: fts_ft8607_param_upgrade
 * Brief:
 * Input: buf - all.bin
 *        len - len of all.bin
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_ft8607_param_upgrade(u8 *buf, u32 len)
{
	int ret = 0;
	u8 *tmpbuf = NULL;
	u32 param_length = 0;

	FTS_INFO("parameter configure upgrade...");
	if (buf == NULL) {
		FTS_ERROR("fw file buffer is null");
		return -EINVAL;
	}

	if ((len < FTS_MIN_LEN) || (len > FTS_MAX_LEN_FILE)) {
		FTS_ERROR("fw file buffer len(%x) fail", len);
		return -EINVAL;
	}

	tmpbuf = buf + upgrade_func_ft8607.paramcfgoff;
	param_length = len - upgrade_func_ft8607.paramcfgoff;
	ret = fts_ft8607_upgrade_mode(FLASH_MODE_PARAM, tmpbuf, param_length);
	if (ret < 0) {
		FTS_INFO("fw upgrade fail,reset to normal boot");
		if (fts_fwupg_reset_in_boot() < 0)
			FTS_ERROR("reset to normal boot fail");

		return ret;
	}

	return 0;
}

struct upgrade_func upgrade_func_ft8607 = {
	.ctype = {0x09},
	.fwveroff = 0x110E,
	.fwcfgoff = 0x0780,
	.appoff = 0x1000,
	.licoff = 0x0000,
	.paramcfgoff = 0x11000,
	.paramcfgveroff = 0x11004,
	.pramboot_supported = true,
	.pramboot = pb_file_ft8607,
	.pb_length = sizeof(pb_file_ft8607),
	.hid_supported = false,
	.upgrade = fts_ft8607_upgrade,
	.get_hlic_ver = fts_ft8607_get_hlic_ver,
	.lic_upgrade = fts_ft8607_lic_upgrade,
	.param_upgrade = fts_ft8607_param_upgrade,
};
