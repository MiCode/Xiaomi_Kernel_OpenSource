/* drivers/input/touchscreen/gt1x_update.c
 *
 * 2010 - 2014 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version: 1.0
 * Revision Record:
 *      V1.0:  first release. 2014/09/28.
 */
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/uaccess.h>
#ifdef CONFIG_GTP_REQUEST_FW_UPDATE
#include <linux/firmware.h>
#endif
#include "gt1x_config.h"
#include "include/gt1x_tpd_common.h"
#ifdef CONFIG_GTP_HEADER_FW_UPDATE
#include "gt1x_firmware.h"
#endif

#ifdef CONFIG_GTP_REQUEST_FW_UPDATE
#define GT1151_DEFAULT_FW              "gt1151_default_"
#endif
#undef CONFIG_GTP_FOPEN_FW_UPDATE

#define UPDATE_FILE_PATH_1          "/data/_goodix_update_.bin"
#define UPDATE_FILE_PATH_2          "/sdcard/_goodix_update_.bin"

#define CONFIG_FILE_PATH_1          "/data/_gt1x_config_.cfg"
#define CONFIG_FILE_PATH_2          "/sdcard/_gt1x_config_.cfg"

#define FOUND_FW_PATH_1              0x01
#define FOUND_FW_PATH_2              0x02
#define FOUND_CFG_PATH_1             0x04
#define FOUND_CFG_PATH_2             0x08

#define PACK_SIZE                    256

/*hardware register define*/
#define _bRW_MISCTL__SRAM_BANK       0x4048
#define _bRW_MISCTL__MEM_CD_EN       0x4049
#define _bRW_MISCTL__CACHE_EN        0x404B
#define _bRW_MISCTL__TMR0_EN         0x40B0
#define _rRW_MISCTL__SWRST_B0_       0x4180
#define _bWO_MISCTL__CPU_SWRST_PULSE 0x4184
#define _rRW_MISCTL__BOOTCTL_B0_     0x4190
#define _rRW_MISCTL__BOOT_OPT_B0_    0x4218
#define _rRW_MISCTL__BOOT_CTL_       0x5094
#define _bRW_MISCTL__DSP_MCU_PWR_    0x4010
#define _bRW_MISCTL__PATCH_AREA_EN_  0x404D

/*
 * 1.  firmware structure
 *    header: 128b
 *
 *   offset           size          content
 *   0                 4              firmware length
 *   4                 2              checksum
 *   6                 6              target MASK name
 *   12               3              target MASK version
 *   15               6              TP subsystem PID
 *   21               3              TP subsystem version
 *   24               1              subsystem count
 *   25               1              chip type
 *                                     0x91: GT1X,   0x92: GT2X
 *   26               6              reserved
 *   32               8              subsystem info[0]
 *   32               8              subsystem info[1]
 *   .....
 *   120             8              subsystem info[11]
 *
 *   body: followed header
 *
 *   128             N0              subsystem[0]
 *   128+N0       N1              subsystem[1]
 *   ....
 *
 * 2. subsystem info structure
 *   offset           size          content
 *   0                 1              subsystem type
 *   1                 2              subsystem length
 *   3                 2              stored address in flash
 *                                     addr = value * 256
 *   5                 3              reserved
 *
 */

#define FW_HEAD_SIZE                         128
#define FW_HEAD_SUBSYSTEM_INFO_SIZE          8
#define FW_HEAD_OFFSET_SUBSYSTEM_INFO_BASE   32

#define FW_SECTION_TYPE_SS51_ISP               0x01
#define FW_SECTION_TYPE_SS51_PATCH             0x02
#define FW_SECTION_TYPE_SS51_PATCH_OVERLAY     0x03
#define FW_SECTION_TYPE_DSP                    0x04
#define FW_SECTION_TYPE_HOTKNOT                0x05
#define FW_SECTION_TYPE_GESTURE                0x06
#define FW_SECTION_TYPE_GESTURE_OVERLAY        0x07
#define FW_SECTION_TYPE_FLASHLESS_FAST_POWER   0x08

#define UPDATE_TYPE_HEADER 0
#define UPDATE_TYPE_FILE   1

#define UPDATE_STATUS_IDLE     0
#define UPDATE_STATUS_RUNNING  1

struct fw_subsystem_info {
	int type;
	int length;
	u32 address;
	int offset;
};

#pragma pack(1)
struct fw_info {
	u32 length;
	u16 checksum;
	u8 target_mask[6];
	u8 target_mask_version[3];
	u8 pid[6];
	u8 version[3];
	u8 subsystem_count;
	u8 chip_type;
	u8 reserved[6];
	struct fw_subsystem_info subsystem[12];
};
#pragma pack()

struct fw_update_info update_info = {
	.status = UPDATE_STATUS_IDLE,
	.progress = 0,
	.max_progress = 9
};


/**
 * @return: return 0 if success, otherwise return a negative number
 *          which contains the error code.
 */
s32 gt1x_check_fs_mounted(char *path_name)
{
	struct path root_path;
	struct path path;
	s32 err;

	err = kern_path("/", LOOKUP_FOLLOW, &root_path);
	if (err)
		return ERROR_PATH;

	err = kern_path(path_name, LOOKUP_FOLLOW, &path);
	if (err) {
		err = ERROR_PATH;
		goto check_fs_fail;
	}

	if (path.mnt->mnt_sb == root_path.mnt->mnt_sb)
		err = ERROR_PATH;
	else
		err = 0;

	path_put(&path);
 check_fs_fail:
	path_put(&root_path);
	return err;
}

int gt1x_i2c_write_with_readback(u16 addr, u8 *buffer, int length)
{
	u8 buf[100];
	int ret = gt1x_i2c_write(addr, buffer, length);

	if (ret)
		return ret;
	ret = gt1x_i2c_read(addr, buf, length);
	if (ret)
		return ret;
	if (memcmp(buf, buffer, length))
		return ERROR_CHECK;
	return 0;
}

#define getU32(a) ((u32)getUint((u8 *)(a), 4))
#define getU16(a) ((u16)getUint((u8 *)(a), 2))
u32 getUint(u8 *buffer, int len)
{
	u32 num = 0;
	int i;

	for (i = 0; i < len; i++) {
		num <<= 8;
		num += buffer[i];
	}
	return num;
}
int gt1x_auto_update_proc(void *data)
{

#ifdef CONFIG_GTP_HEADER_FW_UPDATE
	GTP_INFO("Start auto update thread...");
	gt1x_update_firmware(NULL);
#elif defined(CONFIG_GTP_REQUEST_FW_UPDATE)
	GTP_INFO("Start auto update thread...");
	gt1x_update_firmware(NULL);
#endif
	gt1x_auto_update_done();
	return 0;
}


void gt1x_enter_update_mode(void)
{
#ifdef CONFIG_GTP_ESD_PROTECT
	gt1x_esd_switch(SWITCH_OFF);
#endif
	gt1x_irq_disable();
}

int gt1x_update_prepare(char *filename)
{
	int ret = 0;
	int retry = 5;
#ifdef CONFIG_GTP_REQUEST_FW_UPDATE
	const struct firmware *fw_entry;
	char buf[64];
#endif

	if (filename == NULL) {
#ifdef CONFIG_GTP_REQUEST_FW_UPDATE
		update_info.fw_name = NULL;
		update_info.update_type = UPDATE_TYPE_HEADER;

		sprintf(buf, "%s%s.img",
			GT1151_DEFAULT_FW,
			CONFIG_GT1151_FIRMWARE);
		GTP_INFO("Request default firmware version: %s\n", buf);
		ret = request_firmware(&fw_entry, buf, &gt1x_i2c_client->dev);
		if (ret) {
			GTP_ERROR("load %s fail, error: %d\n",
				GT1151_DEFAULT_FW, ret);
			return ret;
		}
		GTP_INFO("firmware size: 0x%x\n", (unsigned int)fw_entry->size);

		update_info.fw_data =
			(u8 *)devm_kzalloc(&gt1x_i2c_client->dev,
						fw_entry->size, GFP_KERNEL);
		if (!update_info.fw_data) {
			GTP_ERROR("Alloca memory fail\n");
			release_firmware(fw_entry);
			return ERROR_MEM;
		}
		memcpy(update_info.fw_data, fw_entry->data, fw_entry->size);
		update_info.fw_length = fw_entry->size;
		release_firmware(fw_entry);
#elif defined(CONFIG_GTP_HEADER_FW_UPDATE)
		update_info.fw_data = gt1x_default_FW;
		update_info.fw_length = sizeof(gt1x_default_FW);
#else
		GTP_ERROR("No Fw in .h file!");
		return ERROR_FW;
#endif
	}

	while (retry > 0) {
		retry--;
		update_info.firmware =
			kzalloc(sizeof(struct fw_info), GFP_KERNEL);
		if (update_info.firmware == NULL) {
			GTP_INFO("Alloc %d bytes memory fail.",
				(int)(sizeof(struct fw_info)));
			continue;
		} else {
			GTP_INFO("Alloc %d bytes memory success.",
				(int)(sizeof(struct fw_info)));
			break;
		}
	}
	if (retry <= 0) {
		ret = ERROR_RETRY;
		goto gt1x_update_pre_fail1;
	}

	retry = 5;
	while (retry > 0) {
		retry--;
		update_info.buffer = kzalloc(1024 * 4, GFP_KERNEL);
		if (update_info.buffer == NULL) {
			GTP_ERROR("Alloc %d bytes memory fail.", 1024 * 4);
			continue;
		} else {
			GTP_INFO("Alloc %d bytes memory success.", 1024 * 4);
			break;
		}
	}
	if (retry <= 0) {
		ret = ERROR_RETRY;
		goto gt1x_update_pre_fail0;
	}

	return 0;

 gt1x_update_pre_fail0:
	kfree(update_info.firmware);
 gt1x_update_pre_fail1:
	return ret;
}
/**
 * @return: return a pointer pointed at the content of firmware
 *          if success, otherwise return NULL.
 */
u8 *gt1x_get_fw_data(u32 offset, int length)
{
	return &update_info.fw_data[offset];
}

int gt1x_check_firmware(void)
{
	u16 checksum;
	u16 checksum_in_header;
	u8 *p;
	struct fw_info *firmware;
	int i;
	int offset;

	/*compare file length with the length field in the firmware header*/
	if (update_info.fw_length < FW_HEAD_SIZE) {
		GTP_ERROR("Bad firmware!(file length: %d)",
			update_info.fw_length);
		return ERROR_CHECK;
	}
	p = gt1x_get_fw_data(0, 6);
	if (p == NULL)
		return ERROR_FW;
	if (getU32(p) + 6 != update_info.fw_length) {
		GTP_ERROR("Bad firmware!(file length: %d, header define: %d)",
			update_info.fw_length, getU32(p));
		return ERROR_CHECK;
	}
	/*check firmware's checksum*/
	checksum_in_header = getU16(&p[4]);
	checksum = 0;
	for (i = 6; i < update_info.fw_length; i++) {
		p = gt1x_get_fw_data(i, 1);
		if (p == NULL)
			return ERROR_FW;
		checksum += p[0];
	}

	if (checksum != checksum_in_header) {
		GTP_ERROR("Bad firmware!(checksum=0x%04X, header= 0x%04X)",
			checksum, checksum_in_header);
		return ERROR_CHECK;
	}
	/*parse firmware*/
	p = gt1x_get_fw_data(0, FW_HEAD_SIZE);
	if (p == NULL)
		return ERROR_FW;
	memcpy((u8 *) update_info.firmware, p, FW_HEAD_SIZE - 8 * 12);
	update_info.firmware->pid[5] = 0;

	p = &p[FW_HEAD_OFFSET_SUBSYSTEM_INFO_BASE];
	firmware = update_info.firmware;
	offset = FW_HEAD_SIZE;
	for (i = 0; i < firmware->subsystem_count; i++) {
		firmware->subsystem[i].type =
			p[i * FW_HEAD_SUBSYSTEM_INFO_SIZE];
		firmware->subsystem[i].length =
			getU16(&p[i * FW_HEAD_SUBSYSTEM_INFO_SIZE + 1]);
		firmware->subsystem[i].address =
			getU16(&p[i * FW_HEAD_SUBSYSTEM_INFO_SIZE + 3]) * 256;
		firmware->subsystem[i].offset = offset;
		offset += firmware->subsystem[i].length;
	}

	/*print update information*/
	GTP_INFO("Update type: %s",
		update_info.update_type == UPDATE_TYPE_HEADER ?
			"Header" : "File");
	GTP_INFO("Firmware length: %d",
		update_info.fw_length);
	GTP_INFO("Firmware product: GT%s",
		update_info.firmware->pid);
	GTP_INFO("Firmware patch: %02X%02X%02X",
		update_info.firmware->version[0],
		update_info.firmware->version[1],
		update_info.firmware->version[2]);
	GTP_INFO("Firmware chip: 0x%02X",
		update_info.firmware->chip_type);
	GTP_INFO("Subsystem count: %d",
		update_info.firmware->subsystem_count);
	for (i = 0; i < update_info.firmware->subsystem_count; i++) {
		GTP_INFO("------------------------------------------");
		GTP_INFO("Subsystem: %d", i);
		GTP_INFO("Type: %d",
			update_info.firmware->subsystem[i].type);
		GTP_INFO("Length: %d",
			update_info.firmware->subsystem[i].length);
		GTP_INFO("Address: 0x%08X",
			update_info.firmware->subsystem[i].address);
		GTP_INFO("Offset: %d",
			update_info.firmware->subsystem[i].offset);
	}

	return 0;
}


int gt1x_update_judge(void)
{
	int ret;
	u8 reg_val[1];
	u8 retry = 3;
	struct gt1x_version_info ver_info;
	struct gt1x_version_info fw_ver_info;

	fw_ver_info.mask_id =
		(update_info.firmware->target_mask_version[0] << 16) |
		(update_info.firmware->target_mask_version[1] << 8) |
		(update_info.firmware->target_mask_version[2]);
	fw_ver_info.patch_id = (update_info.firmware->version[0] << 16) |
		(update_info.firmware->version[1] << 8) |
		(update_info.firmware->version[2]);
	memcpy(fw_ver_info.product_id, update_info.firmware->pid, 4);
	fw_ver_info.product_id[4] = 0;

	/* check register 0x41E4 */
	do {
		ret = gt1x_i2c_read_dbl_check(0x41E4, reg_val, 1);
		if (ret != 0) {	/* read reg failed */
			gt1x_reset_guitar();
		} else {
			break;
		}
	}
	while (--retry)
		;
	if (reg_val[0] != 0xBE) {
		GTP_ERROR("check 0x41E4 failed, force update!!!");
		return 0;
	}

	ret = gt1x_read_version(&ver_info);
	if (ret < 0) {
		GTP_ERROR("Get IC's version info failed, force update!!!");
		return 0;
	}

	if (memcmp(fw_ver_info.product_id, ver_info.product_id, 4)) {
		GTP_ERROR("Product id is not match!");
		return ERROR_CHECK;
	}
	if ((fw_ver_info.mask_id & 0xFFFFFF00) !=
		(ver_info.mask_id & 0xFFFFFF00)) {
		GTP_ERROR("Mask id is not match!");
		return ERROR_CHECK;
	}
	if (fw_ver_info.patch_id <= ver_info.patch_id) {
		GTP_ERROR("The version of the fw is not high than the IC's!");
		return ERROR_CHECK;
	}
	return 0;
}

u16 gt1x_calc_checksum(u8 *fw, u32 length)
{
	u32 i = 0;
	u32 checksum = 0;

	for (i = 0; i < length; i += 2) {
		checksum += (((int)fw[i]) << 8);
		checksum += fw[i + 1];
	}
	checksum &= 0xFFFF;
	return checksum;
}

int gt1x_recall_check(u8 *chk_src, u16 start_addr, u16 chk_length)
{
	u8 rd_buf[PACK_SIZE];
	s32 ret = 0;
	u16 len = 0;
	u32 compared_length = 0;

	while (chk_length > 0) {
		len = (chk_length > PACK_SIZE ? PACK_SIZE : chk_length);

		ret = gt1x_i2c_read(start_addr + compared_length, rd_buf, len);
		if (ret) {
			GTP_ERROR("recall i2c error,exit!");
			return ret;
		}

		if (memcmp(rd_buf, &chk_src[compared_length], len)) {
			GTP_ERROR("Recall frame not equal(addr: 0x%04X)",
				start_addr + compared_length);
			GTP_DEBUG("chk_src array:");
			GTP_DEBUG_ARRAY(&chk_src[compared_length], len);
			GTP_DEBUG("recall array:");
			GTP_DEBUG_ARRAY(rd_buf, len);
			return ERROR_CHECK;
		}

		chk_length -= len;
		compared_length += len;
	}

	GTP_DEBUG("Recall check %d bytes(address: 0x%04X) success.",
		compared_length, start_addr);
	return 0;
}

int gt1x_run_ss51_isp(u8 *ss51_isp, int length)
{
	int ret;
	u8 buffer[10];

	ret = gt1x_hold_ss51_dsp();
	if (ret)
		return ret;
	/*select bank4*/
	buffer[0] = 0x04;
	ret = gt1x_i2c_write_with_readback(
		_bRW_MISCTL__SRAM_BANK, buffer, 1);
	if (ret) {
		GTP_ERROR("select bank4 fail.");
		return ret;
	}
	/*enable patch area access*/
	buffer[0] = 0x01;
	ret = gt1x_i2c_write_with_readback(
		_bRW_MISCTL__PATCH_AREA_EN_, buffer, 1);
	if (ret) {
		GTP_ERROR("enable patch area access fail!");
		return ret;
	}

	GTP_INFO("ss51_isp length: %d, checksum: 0x%04X",
		length, gt1x_calc_checksum(ss51_isp, length));
	/*load ss51 isp*/
	ret = gt1x_i2c_write(0xC000, ss51_isp, length);
	if (ret) {
		GTP_ERROR("load ss51 isp fail!");
		return ret;
	}
	/*recall compare*/
	ret = gt1x_recall_check(ss51_isp, 0xC000, length);
	if (ret) {
		GTP_ERROR("recall check ss51 isp fail!");
		return ret;
	}

	memset(buffer, 0xAA, 10);
	ret = gt1x_i2c_write_with_readback(0x8140, buffer, 10);

	/*disable patch area access*/
	buffer[0] = 0x00;
	ret = gt1x_i2c_write_with_readback(
		_bRW_MISCTL__PATCH_AREA_EN_, buffer, 1);
	if (ret) {
		GTP_ERROR("disable patch area access fail!");
		return ret;
	}
	/*set 0x8006*/
	memset(buffer, 0x55, 8);
	ret = gt1x_i2c_write_with_readback(0x8006, buffer, 8);
	if (ret) {
		GTP_ERROR("set 0x8006[0~7] 0x55 fail!");
		return ret;
	}
	/*release ss51*/
	buffer[0] = 0x08;
	ret = gt1x_i2c_write_with_readback(
		_rRW_MISCTL__SWRST_B0_, buffer, 1);
	if (ret) {
		GTP_ERROR("release ss51 fail!");
		return ret;
	}

	msleep(100);
	/*check run state*/
	ret = gt1x_i2c_read(0x8006, buffer, 2);
	if (ret) {
		GTP_ERROR("read 0x8006 fail!");
		return ret;
	}
	if (!(buffer[0] == 0xAA && buffer[1] == 0xBB)) {
		GTP_ERROR("ERROR: isp is not running! 0x8006: %02X %02X",
				buffer[0], buffer[1]);
		return ERROR_CHECK;
	}

	return 0;
}

int gt1x_burn_subsystem(struct fw_subsystem_info *subsystem)
{
	int block_len;
	u16 checksum;
	int burn_len = 0;
	u16 cur_addr;
	u32 length = subsystem->length;
	u8 buffer[10];
	int ret;
	int wait_time;
	int burn_state;
	int retry = 5;
	u8 *fw;

	GTP_INFO("Subsystem: %d", subsystem->type);
	GTP_INFO("Length: %d", subsystem->length);
	GTP_INFO("Address: 0x%08X", subsystem->address);

	while (length > 0 && retry > 0) {
		retry--;

		block_len = length > 1024 * 4 ? 1024 * 4 : length;

		GTP_INFO("Burn block ==> length: %d, address: 0x%08X",
			block_len, subsystem->address + burn_len);
		fw = gt1x_get_fw_data(subsystem->offset + burn_len, block_len);
		if (fw == NULL)
			return ERROR_FW;
		cur_addr = ((subsystem->address + burn_len) >> 8);

		checksum = 0;
		checksum += block_len;
		checksum += cur_addr;
		checksum += gt1x_calc_checksum(fw, block_len);
		checksum = (0 - checksum);

		buffer[0] = ((block_len >> 8) & 0xFF);
		buffer[1] = (block_len & 0xFF);
		buffer[2] = ((cur_addr >> 8) & 0xFF);
		buffer[3] = (cur_addr & 0xFF);

		ret = gt1x_i2c_write_with_readback(0x8100, buffer, 4);
		if (ret) {
			GTP_ERROR("write length & address fail!");
			continue;
		}

		ret = gt1x_i2c_write(0x8100 + 4, fw, block_len);
		if (ret) {
			GTP_ERROR("write fw data fail!");
			continue;
		}

		ret = gt1x_recall_check(fw, 0x8100 + 4, block_len);
		if (ret)
			continue;
		buffer[0] = ((checksum >> 8) & 0xFF);
		buffer[1] = (checksum & 0xFF);
		ret = gt1x_i2c_write_with_readback(
			0x8100 + 4 + block_len, buffer, 2);
		if (ret) {
			GTP_ERROR("write checksum fail!");
			continue;
		}

		buffer[0] = 0;
		ret = gt1x_i2c_write_with_readback(0x8022, buffer, 1);
		if (ret) {
			GTP_ERROR("clear control flag fail!");
			continue;
		}

		buffer[0] = subsystem->type;
		buffer[1] = subsystem->type;
		ret = gt1x_i2c_write_with_readback(0x8020, buffer, 2);
		if (ret) {
			GTP_ERROR("write subsystem type fail!");
			continue;
		}
		burn_state = ERROR;
		wait_time = 200;
		while (wait_time > 0) {
			wait_time--;
			msleep(20);
			ret = gt1x_i2c_read(0x8022, buffer, 1);
			if (ret)
				continue;
			if (buffer[0] == 0xAA) {
				GTP_INFO("burning.....");
				continue;
			} else if (buffer[0] == 0xDD) {
				GTP_ERROR("checksum error!");
				break;
			} else if (buffer[0] == 0xBB) {
				GTP_INFO("burning success.");
				burn_state = 0;
				break;
			} else if (buffer[0] == 0xCC) {
				GTP_ERROR("burning failed!");
				break;
			}
			GTP_INFO("unknown state!(0x8022: 0x%02X)", buffer[0]);
		}

		if (!burn_state) {
			length -= block_len;
			burn_len += block_len;
			retry = 5;
		}
	}
	if (length == 0)
		return 0;
	else
		return ERROR_RETRY;
}


int gt1x_read_flash(u32 addr, int length)
{
	int wait_time;
	int ret = 0;
	u8 buffer[4];
	u16 read_addr = (addr >> 8);

	GTP_INFO("Read flash: 0x%04X, length: %d", addr, length);

	buffer[0] = 0;
	ret = gt1x_i2c_write_with_readback(0x8022, buffer, 1);

	buffer[0] = ((length >> 8) & 0xFF);
	buffer[1] = (length & 0xFF);
	buffer[2] = ((read_addr >> 8) & 0xFF);
	buffer[3] = (read_addr & 0xFF);
	ret |= gt1x_i2c_write_with_readback(0x8100, buffer, 4);

	buffer[0] = 0xAA;
	buffer[1] = 0xAA;
	ret |= gt1x_i2c_write(0x8020, buffer, 2);
	if (ret) {
		GTP_ERROR("Error occurred.");
		return ret;
	}

	wait_time = 200;
	while (wait_time > 0) {
		wait_time--;
		msleep(20);
		ret = gt1x_i2c_read(0x8022, buffer, 1);
		if (ret)
			continue;
		if (buffer[0] == 0xBB) {
			GTP_INFO("Read success(addr: 0x%04X, length: %d)",
					addr, length);
			break;
		}
	}
	if (wait_time == 0) {
		GTP_ERROR("Read Flash FAIL!");
		return ERROR_RETRY;
	}
	return 0;
}

int gt1x_check_subsystem_in_flash(struct fw_subsystem_info *subsystem)
{
	int block_len;
	int checked_len = 0;
	u32 length = subsystem->length;
	int ret;
	int check_state = 0;
	int retry = 5;
	u8 *fw;

	GTP_INFO("Subsystem: %d", subsystem->type);
	GTP_INFO("Length: %d", subsystem->length);
	GTP_INFO("Address: 0x%08X", subsystem->address);

	while (length > 0) {
		block_len = length > 1024 * 4 ? 1024 * 4 : length;

		GTP_INFO("Check block ==> length: %d, address: 0x%08X",
			block_len, subsystem->address + checked_len);
		fw = gt1x_get_fw_data(
			subsystem->offset + checked_len,
			block_len);
		if (fw == NULL)
			return ERROR_FW;
		ret = gt1x_read_flash(
			subsystem->address + checked_len,
			block_len);
		if (ret)
			check_state |= ret;
		ret = gt1x_recall_check(fw, 0x8100, block_len);
		if (ret) {
			GTP_ERROR("Block in flash is broken!");
			check_state |= ret;
		}

		length -= block_len;
		checked_len += block_len;
		retry = 5;
	}
	if (check_state)
		GTP_ERROR("Subsystem in flash is broken!");
	else
		GTP_INFO("Subsystem in flash is correct!");
	return check_state;
}

void gt1x_update_cleanup(void)
{
	if (update_info.buffer != NULL) {
		kfree(update_info.buffer);
		update_info.buffer = NULL;
	}
	if (update_info.firmware != NULL) {
		kfree(update_info.firmware);
		update_info.firmware = NULL;
	}
}

int gt1x_update_firmware(char *filename)
{
	int i = 0;
	int ret = 0;
	u8 *p;

	if (update_info.status != UPDATE_STATUS_IDLE) {
		GTP_ERROR("Update process is running!");
		return ERROR;
	}
	update_info.status = UPDATE_STATUS_RUNNING;
	update_info.max_progress = 9;
	update_info.progress = 0;

	gt1x_enter_update_mode();

	ret = gt1x_update_prepare(filename);
	if (ret) {
		update_info.status = UPDATE_STATUS_IDLE;
		goto gt1x_update_exit;
	}
	update_info.progress = 1;

	ret = gt1x_check_firmware();
	if (ret)
		goto gt1x_update_exit;
	update_info.progress = 2;

	ret = gt1x_update_judge();
	if (ret)
		goto gt1x_update_exit;
	update_info.progress = 3;

	p = gt1x_get_fw_data(update_info.firmware->subsystem[0].offset,
				update_info.firmware->subsystem[0].length);
	if (p == NULL) {
		GTP_ERROR("get isp fail");
		ret = ERROR_FW;
		goto gt1x_update_exit;
	}
	update_info.progress = 4;

	ret = gt1x_run_ss51_isp(p, update_info.firmware->subsystem[0].length);
	if (ret) {
		GTP_ERROR("run isp fail");
		goto gt1x_update_exit;
	}
	update_info.progress = 5;
	msleep(800);

	for (i = 1; i < update_info.firmware->subsystem_count; i++) {
		GTP_INFO("subsystem: %d",
			update_info.firmware->subsystem[i].type);
		GTP_INFO("Length: %d",
			update_info.firmware->subsystem[i].length);
		GTP_INFO("Address: %d",
			update_info.firmware->subsystem[i].address);

		ret = gt1x_burn_subsystem(
			&(update_info.firmware->subsystem[i]));
		if (ret) {
			GTP_ERROR("burn subsystem fail!");
			goto gt1x_update_exit;
		}
	}
	update_info.progress = 6;

	gt1x_reset_guitar();

	p = gt1x_get_fw_data(update_info.firmware->subsystem[0].offset,
				update_info.firmware->subsystem[0].length);
	if (p == NULL) {
		GTP_ERROR("get isp fail");
		ret = ERROR_FW;
		goto gt1x_update_exit;
	}
	update_info.progress = 7;

	ret = gt1x_run_ss51_isp(p, update_info.firmware->subsystem[0].length);
	if (ret) {
		GTP_ERROR("run isp fail");
		goto gt1x_update_exit;
	}
	update_info.progress = 8;

	GTP_INFO("Reset guitar & check firmware in flash.");
	/*heck_state = SUCCESS;*/
	for (i = 1; i < update_info.firmware->subsystem_count; i++) {
		GTP_INFO("subsystem: %d",
			update_info.firmware->subsystem[i].type);
		GTP_INFO("Length: %d",
			update_info.firmware->subsystem[i].length);
		GTP_INFO("Address: %d",
			update_info.firmware->subsystem[i].address);

		ret = gt1x_check_subsystem_in_flash(
				&(update_info.firmware->subsystem[i]));
		if (ret)
			break;
	}
	update_info.progress = 9;

 gt1x_update_exit:
	gt1x_update_cleanup();
	gt1x_leave_update_mode();
	gt1x_read_version(NULL);

	update_info.status = UPDATE_STATUS_IDLE;
	if (ret) {
		GTP_ERROR("Update firmware failed!");
	} else if (gt1x_init_failed) {
		gt1x_read_version(&gt1x_version);
		gt1x_init_panel();
	}
	GTP_INFO("Update firmware succeefully!");
	return ret;
}

int __gt1x_hold_ss51_dsp_20(void)
{
	int ret = -1;
	int retry = 0;
	u8 buf[1];
	int hold_times = 0;

	while (retry++ < 2000) {
		/*Hold ss51 & dsp*/
		buf[0] = 0x0C;
		ret = gt1x_i2c_write(_rRW_MISCTL__SWRST_B0_, buf, 1);
		if (ret) {
			GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
			continue;
		}
		/*Confirm hold*/
		buf[0] = 0x00;
		ret = gt1x_i2c_read(_rRW_MISCTL__SWRST_B0_, buf, 1);
		if (ret) {
			GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
			continue;
		}
		if (buf[0] == 0x0C) {
			if (hold_times++ < 20)
				continue;
			else
				break;
		}
		GTP_DEBUG("Hold ss51 & dsp confirm 0x4180 failed,value:%d",
				buf[0]);
	}
	if (retry >= 2000) {
		GTP_ERROR("Hold ss51&dsp failed!");
		return ERROR_RETRY;
	}

	GTP_INFO("Hold ss51&dsp successfully.");
	return 0;
}

int gt1x_hold_ss51_dsp(void)
{
	int ret = ERROR;
	u8 buffer[2];

	gt1x_select_addr();
	msleep(20);
	/*old ss51_dsp*/
	ret = __gt1x_hold_ss51_dsp_20();
	if (ret)
		return ret;
	/*enable dsp & mcu power*/
	buffer[0] = 0x00;
	ret = gt1x_i2c_write_with_readback(
			_bRW_MISCTL__DSP_MCU_PWR_,
			buffer, 1);
	if (ret) {
		GTP_ERROR("enabel dsp & mcu power fail!");
		return ret;
	}
	/*disable watchdog*/
	buffer[0] = 0x00;
	ret = gt1x_i2c_write_with_readback(_bRW_MISCTL__TMR0_EN, buffer, 1);
	if (ret) {
		GTP_ERROR("disable wdt fail!");
		return ret;
	}
	/*clear cache*/
	buffer[0] = 0x00;
	ret = gt1x_i2c_write_with_readback(_bRW_MISCTL__CACHE_EN, buffer, 1);
	if (ret) {
		GTP_ERROR("clear cache fail!");
		return ret;
	}
	/*soft reset*/
	buffer[0] = 0x01;
	ret = gt1x_i2c_write(_bWO_MISCTL__CPU_SWRST_PULSE, buffer, 1);
	if (ret) {
		GTP_ERROR("software reset fail!");
		return ret;
	}
	/*set scramble*/
	buffer[0] = 0x00;
	ret = gt1x_i2c_write_with_readback(
			_rRW_MISCTL__BOOT_OPT_B0_,
			buffer, 1);
	if (ret) {
		GTP_ERROR("set scramble fail!");
		return ret;
	}

	return 0;
}

void gt1x_leave_update_mode(void)
{
	GTP_DEBUG("[leave_update_mode]reset chip.");
	gt1x_reset_guitar();

#ifdef CONFIG_GTP_ESD_PROTECT
	gt1x_esd_switch(SWITCH_ON);
#endif
	gt1x_irq_enable();
}

void read_reg(u16 addr, int len)
{
	int i;
	u8 buffer[16];
	int read_len = 0;
	int cur_len;

	GTP_INFO("Read address: 0x%04X, Length: %d", addr, len);
	while (len > 0) {
		cur_len = (len > 16 ? 16 : len);
		gt1x_i2c_read(addr + read_len, buffer, cur_len);
		GTP_INFO("<<GTP-INF>> ");
		for (i = 0; i < cur_len; i++)
			GTP_INFO("%02X ", buffer[i]);
		GTP_INFO("\n");
		len -= cur_len;
		read_len += cur_len;
	}
}
int gt1x_hold_ss51_dsp_no_reset(void)
{
	int ret = ERROR;
	u8 buffer[2];

	/*old ss51_dsp*/
	ret = __gt1x_hold_ss51_dsp_20();
	if (ret)
		return ret;
	/*enable dsp & mcu power*/
	buffer[0] = 0x00;
	ret = gt1x_i2c_write_with_readback(
			_bRW_MISCTL__DSP_MCU_PWR_,
			buffer, 1);
	if (ret) {
		GTP_ERROR("enabel dsp & mcu power fail!");
		return ret;
	}
	/*disable watchdog*/
	buffer[0] = 0x00;
	ret = gt1x_i2c_write_with_readback(_bRW_MISCTL__TMR0_EN, buffer, 1);
	if (ret) {
		GTP_ERROR("disable wdt fail!");
		return ret;
	}
	/*clear cache*/
	buffer[0] = 0x00;
	ret = gt1x_i2c_write_with_readback(_bRW_MISCTL__CACHE_EN, buffer, 1);
	if (ret) {
		GTP_ERROR("clear cache fail!");
		return ret;
	}
	/*t reset*/
	buffer[0] = 0x01;
	ret = gt1x_i2c_write(_bWO_MISCTL__CPU_SWRST_PULSE, buffer, 1);
	if (ret) {
		GTP_ERROR("software reset fail!");
		return ret;
	}
	/*et scramble*/
	buffer[0] = 0x00;
	ret = gt1x_i2c_write_with_readback(
			_rRW_MISCTL__BOOT_OPT_B0_,
			buffer, 1);
	if (ret) {
		GTP_ERROR("set scramble fail!");
		return ret;
	}

	return 0;
}

#define GT1X_LOAD_PACKET_SIZE (1024 * 2)

int gt1x_load_patch(u8 *patch, u32 patch_size, int offset, int bank_size)
{
	s32 loaded_length = 0;
	s32 len = 0;
	s32 ret = 0;
	u8 bank = 0, tmp;
	u16 address;

	GTP_INFO("size: %d, checksum: 0x%04X, position: 0x%04X, bank-size: %d",
		patch_size, gt1x_calc_checksum(patch, patch_size),
		0xC000 + offset, bank_size);
	while (loaded_length != patch_size) {
		if (loaded_length == 0 ||
			(loaded_length + offset) % bank_size == 0) {
			/*lect bank*/
			bank = 0x04 + (loaded_length + offset) / bank_size;
			ret = gt1x_i2c_write(_bRW_MISCTL__SRAM_BANK, &bank, 1);
			if (ret) {
				GTP_ERROR("select bank%d fail!", bank);
				return ret;
			}
			GTP_INFO("Select bank%d success.", bank);
			/*enable patch area access*/
			tmp = 0x01;
			ret = gt1x_i2c_write_with_readback(
				_bRW_MISCTL__PATCH_AREA_EN_ + bank - 4,
				&tmp, 1);
			if (ret) {
				GTP_ERROR("enable patch area access fail!");
				return ret;
			}
		}

		len =
		    patch_size - loaded_length > GT1X_LOAD_PACKET_SIZE ?
			GT1X_LOAD_PACKET_SIZE : patch_size - loaded_length;
		address = 0xC000 + (loaded_length + offset) % bank_size;

		ret = gt1x_i2c_write(address, &patch[loaded_length], len);
		if (ret) {
			GTP_ERROR("load 0x%04X, %dbytes fail!", address, len);
			return ret;
		}
		ret = gt1x_recall_check(&patch[loaded_length], address, len);
		if (ret) {
			GTP_ERROR("Recall check 0x%04X, %dbytes fail!",
				address, len);
			return ret;
		}
		GTP_INFO("load code 0x%04X, %dbytes success.", address, len);

		loaded_length += len;
	}

	return 0;
}

int gt1x_startup_patch(void)
{
	s32 ret = 0;
	u8 buffer[8] = { 0x55 };

	buffer[0] = 0x00;
	buffer[1] = 0x00;
	ret |= gt1x_i2c_write(_bRW_MISCTL__PATCH_AREA_EN_, buffer, 2);

	memset(buffer, 0x55, 8);
	ret |= gt1x_i2c_write(GTP_REG_FLASH_PASSBY, buffer, 8);
	ret |= gt1x_i2c_write(GTP_REG_VERSION, buffer, 5);

	buffer[0] = 0xAA;
	ret |= gt1x_i2c_write(GTP_REG_CMD, buffer, 1);
	ret |= gt1x_i2c_write(GTP_REG_ESD_CHECK, buffer, 1);

	buffer[0] = 0x00;
	ret |= gt1x_i2c_write(_rRW_MISCTL__SWRST_B0_, buffer, 1);

	msleep(200);

	return ret;
}
