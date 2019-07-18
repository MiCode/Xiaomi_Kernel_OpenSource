/*
 * Goodix GTX5 Firmware Update Driver.
 *
 * Copyright (C) 2015 - 2016 Goodix, Inc.
 * Copyright (C) 2019 XiaoMi, Inc.
 * Authors:  Yulong Cai <caiyulong@goodix.com>
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
 */
#include "goodix_ts_core.h"
#include "goodix_cfg_bin.h"
#include "goodix_default_fw.h"
/* COMMON PART - START */
#define TS_DEFAULT_FIRMWARE			"goodix_gt9886_fw_f2.bin"

#define FW_HEADER_SIZE				256
#define FW_SUBSYS_INFO_SIZE			8
#define FW_SUBSYS_INFO_OFFSET		32
#define FW_SUBSYS_MAX_NUM			28

#define ISP_MAX_BUFFERSIZE			(1024 * 4)

#define HW_REG_CPU_CTRL				0x2180
#define HW_REG_DSP_MCU_POWER		0x2010
#define HW_REG_RESET				0x2184
#define HW_REG_SCRAMBLE				0x2218
#define HW_REG_BANK_SELECT			0x2048
#define HW_REG_ACCESS_PATCH0		0x204D
#define HW_REG_EC_SRM_START			0x204F
#define HW_REG_CPU_RUN_FROM			0x4506 /* for nor_L is 0x4006 */
#define HW_REG_ISP_RUN_FLAG			0x6006
#define HW_REG_ISP_ADDR				0xC000
#define HW_REG_ISP_BUFFER			0x6100
#define HW_REG_SUBSYS_TYPE			0x6020
#define HW_REG_FLASH_FLAG			0x6022
#define HW_REG_CACHE			    0x204B
#define HW_REG_ESD_KEY		        0x2318
#define HW_REG_WTD_TIMER			0x20B0

#define CPU_CTRL_PENDING			0x00
#define CPU_CTRL_RUNNING			0x01

#define ISP_STAT_IDLE				0xFF
#define ISP_STAT_READY				0xAA
#define ISP_STAT_WRITING			0xAA
#define ISP_FLASH_SUCCESS			0xBB
#define ISP_FLASH_ERROR				0xCC
#define ISP_FLASH_CHECK_ERROR		0xDD
#define ISP_CMD_PREPARE				0x55
#define ISP_CMD_FLASH				0xAA

#define TS_CHECK_ISP_STATE_RETRY_TIMES     200
#define TS_READ_FLASH_STATE_RETRY_TIMES    200

/*0: Header update
 *1: request firmware update*/
atomic_t fw_update_mode = ATOMIC_INIT(1);

/**
 * fw_subsys_info - subsytem firmware infomation
 * @type: sybsystem type
 * @size: firmware size
 * @flash_addr: flash address
 * @data: firmware data
 */
struct fw_subsys_info {
	u8 type;
	u32 size;
	u16 flash_addr;
	const u8 *data;
};

#pragma pack(1)
/**
 * firmware_info
 * @size: fw total length
 * @checksum: checksum of fw
 * @hw_pid: mask pid string
 * @hw_pid: mask vid code
 * @fw_pid: fw pid string
 * @fw_vid: fw vid code
 * @subsys_num: number of fw subsystem
 * @chip_type: chip type
 * @protocol_ver: firmware packing
 *   protocol version
 * @subsys: sybsystem info
 */
struct firmware_info {
	u32 size;
	u16 checksum;
	u8 hw_pid[6];
	u8 hw_vid[3];
	u8 fw_pid[8];
	u8 fw_vid[4];
	u8 subsys_num;
	u8 chip_type;
	u8 protocol_ver;
	u8 reserved[2];
	struct fw_subsys_info subsys[FW_SUBSYS_MAX_NUM];
};

#pragma pack()

/**
 * firmware_data - firmware data structure
 * @fw_info: firmware infomation
 * @firmware: firmware data structure
 */
struct firmware_data {
	struct firmware_info fw_info;
	const struct firmware *firmware;
};

enum update_status {
	UPSTA_NOTWORK = 0,
	UPSTA_PREPARING,
	UPSTA_UPDATING,
	UPSTA_ABORT,
	UPSTA_SUCCESS,
	UPSTA_FAILED
};

/**
 * fw_update_ctrl - sturcture used to control the
 *  firmware update process
 * @status: update status
 * @progress: indicate the progress of update
 * @allow_reset: control the reset callback
 * @allow_irq: control the irq callback
 * @allow_suspend: control the suspend callback
 * @allow_resume: allow resume callback
 * @fw_data: firmware data
 * @ts_dev: touch device
 * @fw_name: firmware name
 * @attr_fwimage: sysfs bin attrs, for storing fw image
 * @fw_from_sysfs: whether the firmware image is loadind
 *		from sysfs
 */
struct fw_update_ctrl {
	enum update_status  status;
	unsigned int progress;
	bool force_update;

	bool allow_reset;
	bool allow_irq;
	bool allow_suspend;
	bool allow_resume;

	struct firmware_data fw_data;
	struct goodix_ts_device *ts_dev;
	struct goodix_ts_core *core_data;

	char fw_name[32];
	struct bin_attribute attr_fwimage;
	bool fw_from_sysfs;
};

/**
 * goodix_parse_firmware - parse firmware header infomation
 *	and subsystem infomation from firmware data buffer
 *
 * @fw_data: firmware struct, contains firmware header info
 *	and firmware data.
 * return: 0 - OK, < 0 - error
 */
static int goodix_parse_firmware(struct firmware_data *fw_data)
{
	const struct firmware *firmware;
	struct firmware_info *fw_info;
	unsigned int i, fw_offset, info_offset;
	u16 checksum;
	int r = 0;

	if (!fw_data || !fw_data->firmware) {
		ts_err("Invalid firmware data");
		return -EINVAL;
	}
	fw_info = &fw_data->fw_info;

	/* copy firmware head info */
	firmware = fw_data->firmware;
	if (firmware->size < FW_SUBSYS_INFO_OFFSET) {
		ts_err("Invalid firmware size:%zu", firmware->size);
		r = -EINVAL;
		goto err_size;
	}
	memcpy(fw_info, firmware->data, FW_SUBSYS_INFO_OFFSET);

	/* check firmware size */
	fw_info->size = be32_to_cpu(fw_info->size);
	if (firmware->size != fw_info->size + 6) {
		ts_err("Bad firmware, size not match");
		r = -EINVAL;
		goto err_size;
	}

	/* calculate checksum, note: sum of bytes, but check
	 * by u16 checksum */
	for (i = 6, checksum = 0; i < firmware->size; i++)
		checksum += firmware->data[i];

	/* byte order change, and check */
	fw_info->checksum = be16_to_cpu(fw_info->checksum);
	if (checksum != fw_info->checksum) {
		ts_err("Bad firmware, cheksum error");
		r = -EINVAL;
		goto err_size;
	}

	if (fw_info->subsys_num > FW_SUBSYS_MAX_NUM) {
		ts_err("Bad firmware, invalid subsys num: %d",
		       fw_info->subsys_num);
		r = -EINVAL;
		goto err_size;
	}

	/* parse subsystem info */
	fw_offset = FW_HEADER_SIZE;
	for (i = 0; i < fw_info->subsys_num; i++) {
		info_offset = FW_SUBSYS_INFO_OFFSET +
					i * FW_SUBSYS_INFO_SIZE;

		fw_info->subsys[i].type = firmware->data[info_offset];
		fw_info->subsys[i].size =
				be32_to_cpup((__be32 *)&firmware->data[info_offset + 1]);
		fw_info->subsys[i].flash_addr =
				be16_to_cpup((__be16 *)&firmware->data[info_offset + 5]);

		if (fw_offset > firmware->size) {
			ts_err("Sybsys offset exceed Firmware size");
			goto err_size;
		}

		fw_info->subsys[i].data = firmware->data + fw_offset;
		fw_offset += fw_info->subsys[i].size;
	}

	ts_info("Firmware package protocol: V%u", fw_info->protocol_ver);
	ts_info("Fimware PID:GT%s", fw_info->fw_pid);
	ts_info("Fimware VID:%02X%02X%02X", fw_info->fw_vid[0],
				fw_info->fw_vid[1], fw_info->fw_vid[2]);
	ts_info("Firmware chip type:%02X", fw_info->chip_type);
	ts_info("Firmware size:%u", fw_info->size);
	ts_info("Firmware subsystem num:%u", fw_info->subsys_num);
#ifdef CONFIG_GOODIX_DEBUG
	for (i = 0; i < fw_info->subsys_num; i++) {
		ts_debug("------------------------------------------");
		ts_debug("Index:%d", i);
		ts_debug("Subsystem type:%02X", fw_info->subsys[i].type);
		ts_debug("Subsystem size:%u", fw_info->subsys[i].size);
		ts_debug("Subsystem flash_addr:%08X", fw_info->subsys[i].flash_addr);
		ts_debug("Subsystem Ptr:%p", fw_info->subsys[i].data);
	}
	ts_debug("------------------------------------------");
#endif

err_size:
	return r;
}

/**
 * goodix_check_update - compare the version of firmware running in
 *  touch device with the version getting from the firmware file.
 * @fw_info: firmware infomation to be compared
 * return: 0 firmware in the touch device needs to be updated
 *			< 0 no need to update firmware
 */
static int goodix_check_update(struct goodix_ts_device *dev,
		const struct firmware_info *fw_info)
{
	struct goodix_ts_version fw_ver;
	/*u8 fwimg_cid;*/
	int r = 0;
	int res = 0;

	/* read version from chip, if we got invalid
	 * firmware version, maybe fimware in flash is
	 * incorrect, so we need to update firmware */
	r = dev->hw_ops->read_version(dev, &fw_ver);
	if (r == -EBUS)
		return r;

	if (fw_ver.valid) {
		if (memcmp(fw_ver.pid, fw_info->fw_pid, dev->reg.pid_len)) {
			ts_err("Product ID is not match");
			return -EPERM;
		}

		/*fwimg_cid = fw_info->fw_vid[0];*/
		res = memcmp(fw_ver.vid, fw_info->fw_vid, dev->reg.vid_len);
		if (res == 0) {
			ts_err("FW version is equal to the IC's");
			return -EPERM;
		} else if (res > 0) {
			ts_info("Warning: fw version is lower the IC's");
		}
	} /* else invalid firmware, update firmware */

	ts_info("Firmware needs to be updated");
	return 0;
}

/**
 * goodix_reg_write_confirm - write register and confirm the value
 *  in the register.
 * @dev: pointer to touch device
 * @addr: register address
 * @data: pointer to data buffer
 * @len: data length
 * return: 0 write success and confirm ok
 *		   < 0 failed
 */
static int goodix_reg_write_confirm(struct goodix_ts_device *dev,
		unsigned int addr, unsigned char *data, unsigned int len)
{
	u8 *cfm, cfm_buf[32];
	int r, i;

	if (len > sizeof(cfm_buf)) {
		cfm = kzalloc(len, GFP_KERNEL);
		if (!cfm) {
			ts_err("Mem alloc failed");
			return -ENOMEM;
		}
	} else {
		cfm = &cfm_buf[0];
	}

	for (i = 0; i < GOODIX_BUS_RETRY_TIMES; i++) {
		r = dev->hw_ops->write_trans(dev, addr, data, len);
		if (r < 0)
			goto exit;

		r = dev->hw_ops->read_trans(dev, addr, cfm, len);
		if (r < 0)
			goto exit;

		if (memcmp(data, cfm, len)) {
			r = -EMEMCMP;
			continue;
		} else {
			r = 0;
			break;
		}
	}

exit:
	if (cfm != &cfm_buf[0])
		kfree(cfm);
	return r;
}

static inline int goodix_reg_write(struct goodix_ts_device *dev,
		unsigned int addr, unsigned char *data, unsigned int len)
{
	return dev->hw_ops->write_trans(dev, addr, data, len);
}

static inline int goodix_reg_read(struct goodix_ts_device *dev,
		unsigned int addr, unsigned char *data, unsigned int len)
{
	return dev->hw_ops->read_trans(dev, addr, data, len);
}

#if 0
#define MAX_MASK_BUF_SIZE (16*1024)
static int goodix_load_mask(struct goodix_ts_device *ts_dev)
{
	const struct firmware *mask_fw;
	const u8 *mask_name = "goodix_mask.bin";
	u8 reg_val[10] = {0};
	u32 total_size = 0, data_size = 0, offset = 0;
	int r, i;
	int index;

	ts_debug("Start load mask");
	r = request_firmware(&mask_fw, mask_name, ts_dev->dev);
	if (r < 0) {
		ts_err("Firmware image [%s] not available,errno:%d", mask_name, r);
		return r;
	} else {
		ts_info("Firmware image [%s] is ready, size = %zu", mask_name,
			mask_fw->size);
	}

	/* enable AHB access */
	reg_val[0] = 0x01;
	r = goodix_reg_write(ts_dev, 0x2049, reg_val, 1);
	if (r) {
		ts_err("Failed enbale AHB access");
		goto mask_exit;
	}
	ts_debug("Success enable AHB access, Set 0x2049 --> 0x01");

	/* switch to bank4 */
	reg_val[0] = 0x04;
	r = goodix_reg_write(ts_dev, 0x2048, reg_val, 1);
	if (r) {
		ts_err("Failed switch to bank4");
		goto mask_exit;
	}
	ts_debug("Success switch to bank4, Set 0x2048 -->0x04");

	total_size = mask_fw->size;
	offset = 0;
	index = 1;
	while (total_size > 0) {
		data_size = total_size > MAX_MASK_BUF_SIZE ?
			MAX_MASK_BUF_SIZE : total_size;
		ts_info("Flash firmware to %08x,size:%u bytes",
			0xC000 + offset, data_size);

		for (i = 0; i < 3; i++) {
			r = goodix_reg_write_confirm(ts_dev, 0xC000,
				(u8 *)mask_fw->data + offset, data_size);
			if (!r)
				break;
			else {
				ts_info("Failed write mask data retry..");
				msleep(20);
			}
		}
		if (r) {
			ts_err("Failed send mask");
			goto mask_exit;
		}
		offset += data_size;
		total_size -= data_size;
		/* switch to bank5 */
		if (index == 1) {
			reg_val[0] = 0x05;
			r = goodix_reg_write(ts_dev, 0x2048, reg_val, 1);
			if (r) {
				ts_err("Failed switch to bank5");
				goto mask_exit;
			}
			ts_debug("Success switch to bank5, Set 0x2048-->0x05");
		}
		index++;
	}
	/* disable AHB access */
	reg_val[0] = 0x00;
	r = goodix_reg_write(ts_dev, 0x2049, reg_val, 1);
	if (r) {
		ts_err("Failed disbale AHB access");
		goto mask_exit;
	}
	ts_debug("Success diable AHB access, Set 0x2049-->0x00");
	ts_info("Success loak mask");
mask_exit:
	release_firmware(mask_fw);
	return r;
}
#endif

/**
 * goodix_load_isp - load ISP program to deivce ram
 * @dev: pointer to touch device
 * @fw_data: firmware data
 * return 0 ok, <0 error
 */
static int goodix_load_isp(struct goodix_ts_device *ts_dev,
			   struct firmware_data *fw_data)
{
	struct fw_subsys_info *fw_isp;
	u8 reg_val[8] = {0x00};
	int r;
	int i;

	fw_isp = &fw_data->fw_info.subsys[0];

	ts_info("Loading ISP start");
	/* select bank0 */
	reg_val[0] = 0x00;
	r = goodix_reg_write(ts_dev, HW_REG_BANK_SELECT,
				     reg_val, 1);
	if (r < 0) {
		ts_err("Failed to select bank0");
		return r;
	}
	ts_debug("Success select bank0, Set 0x%x -->0x00", HW_REG_BANK_SELECT);

	/* enable bank0 access */
	reg_val[0] = 0x01;
	r = goodix_reg_write(ts_dev, HW_REG_ACCESS_PATCH0,
				     reg_val, 1);
	if (r < 0) {
		ts_err("Failed to enable patch0 access");
		return r;
	}
	ts_debug("Success select bank0, Set 0x%x -->0x01", HW_REG_ACCESS_PATCH0);

	r = goodix_reg_write_confirm(ts_dev, HW_REG_ISP_ADDR,
				     (u8 *)fw_isp->data, fw_isp->size);
	if (r < 0) {
		ts_err("Loading ISP error");
		return r;
	}

	ts_debug("Success send ISP data to IC");


	/* forbid patch access */
	reg_val[0] = 0x00;
	r = goodix_reg_write_confirm(ts_dev, HW_REG_ACCESS_PATCH0,
				     reg_val, 1);
	if (r < 0) {
		ts_err("Failed to disable patch0 access");
		return r;
	}
	ts_debug("Success forbit bank0 accedd, set 0x%x -->0x00", HW_REG_ACCESS_PATCH0);

	/*clear 0x6006*/
	reg_val[0] = 0x00;
	reg_val[1] = 0x00;
	r = goodix_reg_write(ts_dev, HW_REG_ISP_RUN_FLAG,
			reg_val, 2);
	if (r < 0) {
		ts_err("Failed to clear 0x%x", HW_REG_ISP_RUN_FLAG);
		return r;
	}
	ts_debug("Success clear 0x%x", HW_REG_ISP_RUN_FLAG);

	/* TODO: change address 0xBDE6 set backdoor flag HW_REG_CPU_RUN_FROM */
	memset(reg_val, 0x55, 8);
	r = goodix_reg_write(ts_dev, HW_REG_CPU_RUN_FROM,
				     reg_val, 8);
	if (r < 0) {
		ts_err("Failed set backdoor flag");
		return r;
	}
	ts_debug("Success write [8]0x55 to 0x%x", HW_REG_CPU_RUN_FROM);

	/* Emulation code SRAM start */
	/*reg_val[0] = 0x01;
	r = goodix_reg_write_confirm(ts_dev, HW_REG_EC_SRM_START,
				     reg_val, 1);
	if (r < 0) {
		ts_err("Failed to set CPU Emulation Code SRM start");
		return r;
	}
	ts_debug("Success set CPU Emulation code start, set 0x%x-->0x01",
		 HW_REG_EC_SRM_START);*/

	/* TODO: change reg_val 0x08---> 0x00 release ss51 */
	reg_val[0] = 0x00;
	r = goodix_reg_write(ts_dev, HW_REG_CPU_CTRL,
				     reg_val, 1);
	if (r < 0) {
		ts_err("Failed to run isp");
		return r;
	}
	ts_debug("Success run isp, set 0x%x-->0x00", HW_REG_CPU_CTRL);

	/* check isp work state */
	for (i = 0; i < TS_CHECK_ISP_STATE_RETRY_TIMES; i++) {
		r = goodix_reg_read(ts_dev, HW_REG_ISP_RUN_FLAG,
				    reg_val, 2);
		if (r < 0 || (reg_val[0] == 0xAA && reg_val[1] == 0xBB))
			break;
		usleep_range(5000, 5100);
	}
	if (reg_val[0] == 0xAA && reg_val[1] == 0xBB) {
		ts_info("ISP working OK");
		return 0;
	} else {
		ts_err("ISP not work,0x%x=0x%x, 0x%x=0x%x",
			HW_REG_ISP_RUN_FLAG, reg_val[0],
			HW_REG_ISP_RUN_FLAG + 1, reg_val[1]);
		return -EFAULT;
	}
}

/**
 * goodix_update_prepare - update prepare, loading ISP program
 *  and make sure the ISP is running.
 * @fwu_ctrl: pointer to fimrware control structure
 * return: 0 ok, <0 error
 */
static int goodix_update_prepare(struct fw_update_ctrl *fwu_ctrl)
{
	struct goodix_ts_device *ts_dev = fwu_ctrl->ts_dev;
	u8 reg_val[4] = { 0x00 };
	u8 temp_buf[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	int retry = 20;
	int r;

	ts_dev->hw_ops->write(ts_dev, HW_REG_CPU_RUN_FROM, temp_buf, 8);

	/*reset IC*/
	fwu_ctrl->allow_reset = true;
	ts_info("normandy firmware update, reset");
	gpio_direction_output(ts_dev->board_data->reset_gpio, 0);
	udelay(2000);
	gpio_direction_output(ts_dev->board_data->reset_gpio, 1);
	usleep_range(10000, 11000);
	fwu_ctrl->allow_reset = false;


	retry = 20;
	do {
		/*r = goodix_reg_read(ts_dev, addr + index, temp_buf, rlen);
		if (r < 0) {
			ts_err("Failed read back addr 0x%x", addr + index);
			goto err_out;
		}*/
		reg_val[0] = 0x24;
		r = goodix_reg_write_confirm(ts_dev, HW_REG_CPU_CTRL, reg_val, 1);
		if (r < 0) {
			ts_info("Failed to hold ss51, retry");
			msleep(20);
		} else {
			break;
		}
	} while (--retry);
	if (!retry) {
		ts_err("Failed hold ss51,return =%d", r);
		return -EINVAL;
	}
	ts_debug("Success hold ss51");

	/* enable DSP & MCU power */
	reg_val[0] = 0x00;
	r = goodix_reg_write_confirm(ts_dev, HW_REG_DSP_MCU_POWER, reg_val, 1);
	if (r < 0) {
		ts_err("Failed enable DSP&MCU power");
		return r;
	}
	ts_debug("Success enabled DSP&MCU power,set 0x%x-->0x00",
		 HW_REG_DSP_MCU_POWER);

	/* disable watchdog timer */
	reg_val[0] = 0x00;
	r = goodix_reg_write(ts_dev, HW_REG_CACHE, reg_val, 1);
	if (r < 0) {
		ts_err("Failed to clear cache");
		return r;
	}
	ts_debug("Success clear cache");

	reg_val[0] = 0x95;
	r = goodix_reg_write(ts_dev, HW_REG_ESD_KEY, reg_val, 1);
	reg_val[0] = 0x00;
	r |= goodix_reg_write(ts_dev, HW_REG_WTD_TIMER, reg_val, 1);

	reg_val[0] = 0x27;
	r |= goodix_reg_write(ts_dev, HW_REG_ESD_KEY, reg_val, 1);
	if (r < 0) {
		ts_err("Failed to disable watchdog");
		return r;
	}
	ts_debug("Success disable watchdog");

	/* soft reset */
	/*reg_val[0] = 0x01;
	r = goodix_reg_write(ts_dev, HW_REG_RESET, reg_val, 1);
	if (r < 0) {
		ts_err("Soft reset falied");
		return r;
	}
	ts_debug("Success soft reset");*/

	/* set scramble */
	reg_val[0] = 0x00;
	r = goodix_reg_write(ts_dev, HW_REG_SCRAMBLE, reg_val, 1);
	if (r < 0) {
		ts_err("Failed to set scramble");
		return r;
	}
	ts_debug("Succcess set scramble");

	/* load mask for emulation IC */
	/*r = goodix_load_mask(ts_dev);
	if (r < 0) {
		ts_err("Failed load mask");
		return r;
	}
	ts_debug("Success load mask");*/

	/* load ISP code and run form isp */
	r = goodix_load_isp(ts_dev, &fwu_ctrl->fw_data);
	if (r < 0)
		ts_err("Failed lode and run isp");

	return r;
}

/**
 * goodix_format_fw_packet - formate one flash packet
 * @pkt: target firmware packet
 * @flash_addr: flash address
 * @size: packet size
 * @data: packet data
 */
static int goodix_format_fw_packet(u8 *pkt, u32 flash_addr,
				   u16 len, const u8 *data)
{
	u16 checksum;
	if (!pkt || !data)
		return -EINVAL;

	/*
	 * checksum rule:sum of data in one format is equal to zero
	 * data format: byte/le16/be16/le32/be32/le64/be64
	 */
	pkt[0] = (len >> 8) & 0xff;
	pkt[1] = len & 0xff;
	pkt[2] = (flash_addr >> 16) & 0xff; /* u16 >> 16bit seems nosense but really important */
	pkt[3] = (flash_addr >> 8) & 0xff;
	memcpy(&pkt[4], data, len);
	checksum = checksum_be16(pkt, len + 4);
	checksum = 0 - checksum;
	pkt[len + 4] = (checksum >> 8) & 0xff;
	pkt[len + 5] = checksum & 0xff;
	return 0;
}

/*static int goodix_read_back_check(struct goodix_ts_device *ts_dev, u16 addr,
				  u32 len, const char *dst)
{
	int r;
	int index = 0;
	int rlen = 0;
	int need_compare_len = len;
	char *temp_buf;

	temp_buf = kzalloc(0x1006, GFP_KERNEL);
	if (!temp_buf) {
		ts_err("Alloc memory");
		return -ENOMEM;
	}

	while (need_compare_len > 0) {
		rlen = need_compare_len > 0x1006 ? 0x1006 : need_compare_len;
		r = goodix_reg_read(ts_dev, addr + index, temp_buf, rlen);
		if (r < 0) {
			ts_err("Failed read back addr 0x%x", addr + index);
			goto err_out;
		}

		if (memcmp(temp_buf, dst + index, rlen)) {
			ts_err("Read back compare failed, len = %d", rlen);
			//goto err_out;
		}

		index += rlen;
		need_compare_len -= rlen;
	}
	r = 0;
err_out:
	kfree(temp_buf);
	return r;
}*/
/**
 * goodix_send_fw_packet - send one firmware packet to ISP
 * @dev: target touch device
 * @pkt: firmware packet
 * return：0 ok, <0 error
 */
static int goodix_send_fw_packet(struct goodix_ts_device *dev, u8 type,
				 u8 *pkt, u32 len)
{
	u8 reg_val[4];
	int r, i;

	if (!pkt)
		return -EINVAL;

	r = goodix_reg_write_confirm(dev, HW_REG_ISP_BUFFER, pkt, len);
	if (r < 0) {
		ts_err("Failed to write firmware packet");
		return r;
	}

	reg_val[0] = 0;
	reg_val[1] = 0;
	/* clear flash flag 0X6022 */
	r = goodix_reg_write_confirm(dev, HW_REG_FLASH_FLAG, reg_val, 2);
	if (r < 0) {
		ts_err("Faile to clear flash flag");
		return r;
	}

	/* write subsystem type 0X8020*/
	reg_val[0] = type;
	reg_val[1] = type;
	r = goodix_reg_write_confirm(dev, HW_REG_SUBSYS_TYPE, reg_val, 2);
	if (r < 0) {
		ts_err("Failed write subsystem type to IC");
		return r;
	}

	for (i = 0; i < TS_READ_FLASH_STATE_RETRY_TIMES; i++) {
		r = goodix_reg_read(dev, HW_REG_FLASH_FLAG, reg_val, 2);
		if (r < 0) {
			ts_err("Failed read flash state");
			return r;
		}

		/* flash haven't end */
		if (reg_val[0] == ISP_STAT_WRITING && reg_val[1] == ISP_STAT_WRITING) {
			ts_debug("Flash not ending...");
			usleep_range(55000, 56000);
			continue;
		}
		if (reg_val[0] == ISP_FLASH_SUCCESS && reg_val[1] == ISP_FLASH_SUCCESS) {
			/* read twice to confirm the result */
			r = goodix_reg_read(dev, HW_REG_FLASH_FLAG, reg_val, 2);
			if (!r && reg_val[0] == ISP_FLASH_SUCCESS && reg_val[1] == ISP_FLASH_SUCCESS) {
				/*r = goodix_read_back_check(dev, 0x8104,
					len - 6, (char*)pkt + 4);
				if (r) {
					ts_err("Read back compare failed");
				} else {
					ts_info("Read back compare OK");
				}*/
				ts_info("Flash subsystem ok");
				return 0;
			}
		}
		if (reg_val[0] == ISP_FLASH_ERROR && reg_val[1] == ISP_FLASH_ERROR) {
			ts_err(" Flash subsystem failed");
			return -EAGAIN;
		}
		if (reg_val[0] == ISP_FLASH_CHECK_ERROR) {
			ts_err("Subsystem checksum err");
			return -EAGAIN;
		}

		usleep_range(250, 260);
	}

	ts_err("Wait for flash end timeout, 0x6022= %x %x",
			reg_val[0], reg_val[1]);
	return -EAGAIN;
}




/**
 * goodix_flash_subsystem - flash subsystem firmware,
 *  Main flow of flashing firmware.
 *	Each firmware subsystem is divided into several
 *	packets, the max size of packet is limited to
 *	@{ISP_MAX_BUFFERSIZE}
 * @dev: pointer to touch device
 * @subsys: subsystem infomation
 * return: 0 ok, < 0 error
 */
static int goodix_flash_subsystem(struct goodix_ts_device *dev,
				struct fw_subsys_info *subsys)
{
	u16 data_size, offset;
	u32 total_size;
	u32 subsys_base_addr = subsys->flash_addr << 8;
	u8 *fw_packet;
	int r = 0, i;

	/*
	 * if bus(i2c/spi) error occued, then exit, we will do
	 * hardware reset and re-prepare ISP and then retry
	 * flashing
	 */
	total_size = subsys->size;
	fw_packet = kzalloc(ISP_MAX_BUFFERSIZE + 6, GFP_KERNEL);
	if (!fw_packet) {
		ts_err("Failed alloc memory");
		return -EINVAL;
	}

	offset = 0;
	while (total_size > 0) {
		data_size = total_size > ISP_MAX_BUFFERSIZE ?
				ISP_MAX_BUFFERSIZE : total_size;
		ts_info("Flash firmware to %08x,size:%u bytes",
			subsys_base_addr + offset, data_size);

		/* format one firmware packet */
		r = goodix_format_fw_packet(fw_packet, subsys_base_addr + offset,
				data_size, &subsys->data[offset]);
		if (r < 0) {
			ts_err("Invalid packet params");
			goto exit;
		}

		/* send one firmware packet, retry 3 time if send failed */
		for (i = 0; i < 3; i++) {
			r = goodix_send_fw_packet(dev, subsys->type,
						  fw_packet, data_size + 6);
			if (!r)
				break;
		}
		if (r) {
			ts_err("Failed flash subsystem");
			goto exit;
		}
		offset += data_size;
		total_size -= data_size;
	} /* end while */

exit:
	kfree(fw_packet);
	return r;
}



/**
 * goodix_flash_firmware - flash firmware
 * @dev: pointer to touch device
 * @fw_data: firmware data
 * return: 0 ok, < 0 error
 */
static int goodix_flash_firmware(struct goodix_ts_device *dev,
				struct firmware_data *fw_data)
{
	struct fw_update_ctrl *fw_ctrl;
	struct firmware_info  *fw_info;
	struct fw_subsys_info *fw_x;
	int retry = GOODIX_BUS_RETRY_TIMES;
	int i, r = 0, fw_num, prog_step;

	/* start from subsystem 1,
	 * subsystem 0 is the ISP program */
	fw_ctrl = container_of(fw_data, struct fw_update_ctrl, fw_data);
	fw_info = &fw_data->fw_info;
	fw_num = fw_info->subsys_num;

	/* we have 80% work here */
	prog_step = 80 / (fw_num - 1);

	for (i = 1; i < fw_num && retry;) {
		ts_info("--- Start to flash subsystem[%d] ---", i);
		fw_x = &fw_info->subsys[i];
		r = goodix_flash_subsystem(dev, fw_x);
		if (r == 0) {
			ts_info("--- End flash subsystem[%d]: OK ---", i);
			fw_ctrl->progress += prog_step;
			i++;
		} else if (r == -EAGAIN) {
			retry--;
			ts_err("--- End flash subsystem%d: Fail, errno:%d, retry:%d ---",
				i, r, GOODIX_BUS_RETRY_TIMES - retry);
		} else if (r < 0) { /* bus error */
			ts_err("--- End flash subsystem%d: Fatal error:%d exit ---",
				i, r);
			goto exit_flash;
		}
	}

/*------Following is debug code---------*/
/*	debug_buf = kzalloc(4096, GFP_KERNEL);
	if (!debug_buf) {
		ts_err("Failed alloc memory");
		goto exit_flash;
	}

	temp[0] = 0x10;
	temp[1] = 0x00;
	temp[2] = 0x00;
	temp[3] = 0x00;
	r = goodix_reg_write(dev, 0x8100, temp, 4);
	temp[0] = 0;
	temp[1] = 0;
	r |= goodix_reg_write(dev, 0x8022, temp, 2);
	temp[0] = 0xAA;
	temp[1] = 0xAA;
	r |= goodix_reg_write(dev, 0x8020, temp, 2);
	if (r) {
		ts_err("Faild send read command");
		goto exit_debug;
	}

	r = goodix_reg_read(dev, 0x8100, debug_buf, 4086);
	if (!r) {
		ts_info("success read 4096bytes");
		ts_info("data is: %*ph", 128, debug_buf);
	} else {
		ts_err("Failed read 0x8100,4096 bytes");
	}

exit_debug:
	kfree(debug_buf);*/
/*-------------------------------------*/



exit_flash:
	return r;
}

/**
 * goodix_update_finish - update finished, free resource
 *  and reset flags---
 * @fwu_ctrl: pointer to fw_update_ctrl structrue
 * return: 0 ok, < 0 error
 */
static int goodix_update_finish(struct goodix_ts_device *ts_dev,
				struct fw_update_ctrl *fwu_ctrl)
{
	u8 reg_val[8] = {0};
	int r = 0;

	/* hold ss51 */
	reg_val[0] = 0x24;
	r = goodix_reg_write(ts_dev, HW_REG_CPU_CTRL,
				     reg_val, 1);
	if (r < 0)
		ts_err("Failed to hold ss51");

	/* clear back door flag */
	memset(reg_val, 0, sizeof(reg_val));
	r = goodix_reg_write(ts_dev, HW_REG_CPU_RUN_FROM, reg_val, 8);
	if (r) {
		ts_err("Failed set CPU run from normal firmware");
		return r;
	}

	/* release ss51 */
	reg_val[0] = 0x00;
	r = goodix_reg_write(ts_dev, HW_REG_CPU_CTRL, reg_val, 1);
	if (r < 0)
		ts_err("Failed to run ss51");

	/*reset*/
	r = ts_dev->hw_ops->reset(ts_dev);

	return r;
}


/**
 * goodix_fw_update_proc - firmware update process, the entry of
 *  firmware update flow
 * @fwu_ctrl: firmware control
 * return: 0 ok, < 0 error
 */
int goodix_fw_update_proc(struct fw_update_ctrl *fwu_ctrl)
{
#define FW_UPDATE_RETRY		2
	int retry0 = FW_UPDATE_RETRY;
	int retry1 = FW_UPDATE_RETRY;
	int r = 0;

	if (fwu_ctrl->status == UPSTA_PREPARING ||
			fwu_ctrl->status == UPSTA_UPDATING) {
		ts_err("Firmware update already in progress");
		return 0;
	}

	fwu_ctrl->progress = 0;
	fwu_ctrl->status = UPSTA_PREPARING;

	r = goodix_parse_firmware(&fwu_ctrl->fw_data);
	if (r < 0) {
		fwu_ctrl->status = UPSTA_ABORT;
		goto err_parse_fw;
	}

	/* TODO: set force update flag*/
	/*fwu_ctrl->force_update = true;*/
	fwu_ctrl->progress = 10;
	if (fwu_ctrl->force_update == false) {
		r = goodix_check_update(fwu_ctrl->ts_dev,
					&fwu_ctrl->fw_data.fw_info);
		if (r < 0) {
			fwu_ctrl->status = UPSTA_ABORT;
			goto err_check_update;
		}
	}

start_update:
	fwu_ctrl->progress = 20;
	fwu_ctrl->status = UPSTA_UPDATING; /* show upgrading status */
	r = goodix_update_prepare(fwu_ctrl);
	if ((r == -EBUS || r == -EAGAIN) && --retry0 > 0) {
		ts_err("Bus error, retry prepare ISP:%d",
				FW_UPDATE_RETRY - retry0);
		goto start_update;
	} else if (r < 0) {
		ts_err("Failed to prepare ISP, exit update:%d", r);
		fwu_ctrl->status = UPSTA_FAILED;
		goto err_fw_prepare;
	}

	/* progress: 20%~100% */
	r = goodix_flash_firmware(fwu_ctrl->ts_dev, &fwu_ctrl->fw_data);
	if ((r == -EBUS || r == -ETIMEOUT) && --retry1 > 0) {
		/* we will retry[twice] if returns bus error[i2c/spi]
		 * we will do hardware reset and re-prepare ISP and then retry
		 * flashing
		 */
		ts_err("Bus error, retry firmware update:%d",
				FW_UPDATE_RETRY - retry1);
		goto start_update;
	} else if (r < 0) {
		ts_err("Fatal error, exit update:%d", r);
		fwu_ctrl->status = UPSTA_FAILED;
		goto err_fw_flash;
	}

	fwu_ctrl->status = UPSTA_SUCCESS;

err_fw_flash:
err_fw_prepare:
	goodix_update_finish(fwu_ctrl->ts_dev, fwu_ctrl);
err_check_update:
err_parse_fw:
	if (fwu_ctrl->status == UPSTA_SUCCESS)
		ts_info("Firmware update successfully");
	else if (fwu_ctrl->status == UPSTA_FAILED)
		ts_err("Firmware update failed");

	fwu_ctrl->progress = 100; /* 100% */

/*	msleep(800);
	if (fwu_ctrl->ts_dev->hw_ops->send_config(fwu_ctrl->ts_dev,
						  fwu_ctrl->ts_dev->normal_cfg))
		ts_err("Failed send config");
	else
		ts_info("Send config success");
	msleep(200);
*/
	return r;
}
/* COMMON PART - END */

static struct goodix_ext_module goodix_fwu_module;

/**
 * goodix_request_firmware - request firmware data from user space
 *
 * @fw_data: firmware struct, contains firmware header info
 *	and firmware data pointer.
 * return: 0 - OK, < 0 - error
 */
static int goodix_request_firmware(struct firmware_data *fw_data,
				const char *name)
{
	struct fw_update_ctrl *fw_ctrl =
		container_of(fw_data, struct fw_update_ctrl, fw_data);
	struct device *dev = fw_ctrl->ts_dev->dev;
	int r;

	ts_info("Request firmware image [%s]", name);
	r = request_firmware(&fw_data->firmware, name, dev);
	if (r < 0)
		ts_err("Firmware image [%s] not available,errno:%d", name, r);
	else
		ts_info("Firmware image [%s] is ready", name);
	return r;
}

/**
 * relase firmware resources
 *
 */
static inline void goodix_release_firmware(struct firmware_data *fw_data)
{
	if (fw_data->firmware) {
		release_firmware(fw_data->firmware);
		fw_data->firmware = NULL;
	}
}

static int goodix_fw_update_thread(void *data)
{
	struct fw_update_ctrl *fwu_ctrl = data;
	struct firmware *temp_firmware = NULL;
	static DEFINE_MUTEX(fwu_lock);
	int r;

	if (!fwu_ctrl) {
		ts_err("Invaid thread params");
		goodix_unregister_ext_module(&goodix_fwu_module);
		return 0;
	}

	mutex_lock(&fwu_lock);
	if (!fwu_ctrl->fw_from_sysfs) {
		if (atomic_read(&fw_update_mode) == 0) {
			ts_info("Firmware header update starts");
			temp_firmware = kzalloc(sizeof(struct firmware), GFP_KERNEL);
			if (!temp_firmware) {
				ts_err("Failed to allocate memory for firmware");
				goto out;
			}
			temp_firmware->size = sizeof(goodix_default_fw);
			temp_firmware->data = goodix_default_fw;
			fwu_ctrl->fw_data.firmware = temp_firmware;
		} else if (atomic_read(&fw_update_mode) == 1) {
			ts_info("Firmware request update starts");
			r = goodix_request_firmware(&fwu_ctrl->fw_data,
							fwu_ctrl->fw_name);
			if (r < 0) {
				fwu_ctrl->status = UPSTA_ABORT;
				fwu_ctrl->progress = 100;
				goto out;
			}
		}
	} else {
		if (!fwu_ctrl->fw_data.firmware) {
			ts_err("Invalid firmware from sysfs");
			fwu_ctrl->status = UPSTA_ABORT;
			fwu_ctrl->progress = 100;
			goto out;
		}
	}

	/* DONT allow reset/irq/suspend/resume during update */
	fwu_ctrl->allow_irq = false;
	fwu_ctrl->allow_suspend = false;
	fwu_ctrl->allow_resume = false;
	fwu_ctrl->allow_reset = false;
	goodix_ts_blocking_notify(NOTIFY_FWUPDATE_START, NULL);

	/* ready to update */
	goodix_fw_update_proc(fwu_ctrl);

	goodix_ts_blocking_notify(NOTIFY_FWUPDATE_END, NULL);
	fwu_ctrl->allow_reset = true;
	fwu_ctrl->allow_irq = true;
	fwu_ctrl->allow_suspend = true;
	fwu_ctrl->allow_resume = true;

	/* clean */
	if (!fwu_ctrl->fw_from_sysfs) {
		if (atomic_read(&fw_update_mode) == 0) {
			kfree(fwu_ctrl->fw_data.firmware);
			fwu_ctrl->fw_data.firmware = NULL;
			temp_firmware = NULL;
		} else if (atomic_read(&fw_update_mode) == 1) {
			goodix_release_firmware(&fwu_ctrl->fw_data);
		}
	} else {
		fwu_ctrl->fw_from_sysfs = false;
		vfree(fwu_ctrl->fw_data.firmware);
		fwu_ctrl->fw_data.firmware = NULL;
		if (atomic_read(&fw_update_mode) == 0)
			temp_firmware = NULL;
	}

	/*parse cfg_group.bin*/
	if (!fwu_ctrl->core_data->cfg_group_parsed)
		goodix_cfg_bin_proc(fwu_ctrl->core_data);

out:
	goodix_unregister_ext_module(&goodix_fwu_module);
	mutex_unlock(&fwu_lock);
	atomic_set(&fw_update_mode, 0);
	return 0;
}
/* sysfs attributes */
static ssize_t goodix_sysfs_update_en_store(
		struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	int val = 0, r;

	r = sscanf(buf, "%d", &val);
	if (r < 0)
		return r;

	if (r) {
		atomic_set(&fw_update_mode, 1);
		if (goodix_register_ext_module(&goodix_fwu_module))
			return -EIO;
	}

	return count;
}

static ssize_t goodix_sysfs_update_progress_show(
		struct goodix_ext_module *module,
		char *buf)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	return scnprintf(buf, PAGE_SIZE, "%d\n", fw_ctrl->progress);
}

static ssize_t goodix_sysfs_update_result_show(
		struct goodix_ext_module *module,
		char *buf)
{
	char *result = NULL;
	struct fw_update_ctrl *fw_ctrl = module->priv_data;

	ts_info("result show");
	switch (fw_ctrl->status) {
	case UPSTA_NOTWORK:
		result = "notwork";
		break;
	case UPSTA_PREPARING:
		result = "preparing";
		break;
	case UPSTA_UPDATING:
		result = "upgrading";
		break;
	case UPSTA_ABORT:
		result = "abort";
		break;
	case UPSTA_SUCCESS:
		result = "success";
		break;
	case UPSTA_FAILED:
		result = "failed";
		break;
	default:
		break;
	}

	return scnprintf(buf, PAGE_SIZE, "%s\n", result);
}

static ssize_t goodix_sysfs_update_fwversion_show(
		struct goodix_ext_module *module,
		char *buf)
{
	struct goodix_ts_version fw_ver;
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	int r = 0;
	char str[5];

	/* read version from chip */
	r = fw_ctrl->ts_dev->hw_ops->read_version(fw_ctrl->ts_dev,
			&fw_ver);
	if (!r) {
		memcpy(str, fw_ver.pid, 4);
		str[4] = '\0';
		return scnprintf(buf, PAGE_SIZE, "PID:%s VID:%02x %02x %02x %02x SENSOR_ID:%d\n",
				str, fw_ver.vid[0], fw_ver.vid[1],
				fw_ver.vid[2], fw_ver.vid[3], fw_ver.sensor_id);
	}
	return 0;
}

static ssize_t goodix_sysfs_fwsize_show(struct goodix_ext_module *module,
		char *buf)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	int r = -EINVAL;

	if (fw_ctrl && fw_ctrl->fw_data.firmware)
		r = snprintf(buf, PAGE_SIZE, "%zu\n",
				fw_ctrl->fw_data.firmware->size);
	return r;
}

static ssize_t goodix_sysfs_fwsize_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	struct firmware *fw;
	u8 **data;
	size_t size = 0;

	if (!fw_ctrl)
		return -EINVAL;

	if (sscanf(buf, "%zu", &size) < 0 || !size) {
		ts_err("Failed to get fwsize");
		return -EFAULT;
	}

	fw = vmalloc(sizeof(*fw) + size);
	if (fw == NULL) {
		ts_err("Failed to alloc memory,size:%zu",
				size + sizeof(*fw));
		return -ENOMEM;
	}

	memset(fw, 0x00, sizeof(*fw) + size);
	data = (u8 **)&fw->data;
	*data = (u8 *)fw + sizeof(struct firmware);
	fw->size = size;
	fw_ctrl->fw_data.firmware = fw;
	fw_ctrl->fw_from_sysfs = true;

	return count;
}

static ssize_t goodix_sysfs_fwimage_store(struct file *file,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t pos, size_t count)
{
	struct fw_update_ctrl *fw_ctrl;
	struct firmware_data *fw_data;

	fw_ctrl = container_of(attr, struct fw_update_ctrl,
			attr_fwimage);
	fw_data = &fw_ctrl->fw_data;

	if (!fw_data->firmware) {
		ts_err("Need set fw image size first");
		return -ENOMEM;
	}

	if (fw_data->firmware->size == 0) {
		ts_err("Invalid firmware size");
		return -EINVAL;
	}

	if (pos + count > fw_data->firmware->size)
		return -EFAULT;

	memcpy((u8 *)&fw_data->firmware->data[pos], buf, count);
	fw_ctrl->force_update = true;

	return count;
}

static ssize_t goodix_sysfs_force_update_store(
		struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	int val = 0, r;

	r = sscanf(buf, "%d", &val);
	if (r < 0)
		return r;

	if (r)
		fw_ctrl->force_update = true;
	else
		fw_ctrl->force_update = false;

	return count;
}

static struct goodix_ext_attribute goodix_fwu_attrs[] = {
	__EXTMOD_ATTR(update_en, S_IWUGO, NULL, goodix_sysfs_update_en_store),
	__EXTMOD_ATTR(progress, S_IRUGO, goodix_sysfs_update_progress_show, NULL),
	__EXTMOD_ATTR(result, S_IRUGO, goodix_sysfs_update_result_show, NULL),
	__EXTMOD_ATTR(fwversion, S_IRUGO,
			goodix_sysfs_update_fwversion_show, NULL),
	__EXTMOD_ATTR(fwsize, S_IRUGO | S_IWUGO, goodix_sysfs_fwsize_show,
			goodix_sysfs_fwsize_store),
	__EXTMOD_ATTR(force_update, S_IWUGO, NULL,
			goodix_sysfs_force_update_store),
};

static int goodix_syfs_init(struct goodix_ts_core *core_data,
		struct goodix_ext_module *module)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	struct kobj_type *ktype;
	int ret = 0, i;

	ktype = goodix_get_default_ktype();
	ret = kobject_init_and_add(&module->kobj,
			ktype,
			&core_data->pdev->dev.kobj,
			"fwupdate");
	if (ret) {
		ts_err("Create fwupdate sysfs node error!");
		goto exit_sysfs_init;
	}

	for (i = 0; i < ARRAY_SIZE(goodix_fwu_attrs); i++) {
		if (sysfs_create_file(&module->kobj,
				&goodix_fwu_attrs[i].attr)) {
			ts_err("Create sysfs attr file error");
			kobject_put(&module->kobj);
			ret = -EINVAL;
			goto exit_sysfs_init;
		}
	}

	fw_ctrl->attr_fwimage.attr.name = "fwimage";
	fw_ctrl->attr_fwimage.attr.mode = S_IRUGO | S_IWUGO;
	fw_ctrl->attr_fwimage.size = 0;
	fw_ctrl->attr_fwimage.write = goodix_sysfs_fwimage_store;
	ret = sysfs_create_bin_file(&module->kobj,
			&fw_ctrl->attr_fwimage);

exit_sysfs_init:
	return ret;
}

static int goodix_fw_update_init(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	struct task_struct *fwu_thrd;
	struct fw_update_ctrl *fwu_ctrl;
	static bool init_sysfs = true;

	if (!core_data->ts_dev)
		return -ENODEV;

	if (!module->priv_data) {
		module->priv_data = kzalloc(sizeof(struct fw_update_ctrl),
							GFP_KERNEL);
		if (!module->priv_data) {
			ts_err("Failed to alloc memory for fwu_ctrl");
			return -ENOMEM;
		}
	}
	fwu_ctrl = module->priv_data;
	fwu_ctrl->ts_dev = core_data->ts_dev;
	fwu_ctrl->allow_reset = true;
	fwu_ctrl->allow_irq = true;
	fwu_ctrl->allow_suspend = true;
	fwu_ctrl->allow_resume = true;
	fwu_ctrl->core_data = core_data;

	/* find a valid firmware image name */
	if (ts_bdata && ts_bdata->fw_name)
		strlcpy(fwu_ctrl->fw_name, ts_bdata->fw_name,
				sizeof(fwu_ctrl->fw_name));
	else
		strlcpy(fwu_ctrl->fw_name, TS_DEFAULT_FIRMWARE,
				sizeof(fwu_ctrl->fw_name));

	/* create sysfs interface */
	if (init_sysfs) {
		if (!goodix_syfs_init(core_data, module))
			init_sysfs = false;
	}

	/* create and run update thread */
	fwu_thrd = kthread_run(goodix_fw_update_thread,
			module->priv_data, "goodix-fwu");
	if (IS_ERR_OR_NULL(fwu_thrd)) {
		ts_err("Failed to create update thread:%ld",
				PTR_ERR(fwu_thrd));
		return -EFAULT;
	}

	return 0;
}

static int goodix_fw_update_exit(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	return 0;
}

static int goodix_fw_before_suspend(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	struct fw_update_ctrl *fwu_ctrl = module->priv_data;

	return fwu_ctrl->allow_suspend ?
				EVT_HANDLED : EVT_CANCEL_SUSPEND;
}

static int goodix_fw_before_resume(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	struct fw_update_ctrl *fwu_ctrl = module->priv_data;

	return fwu_ctrl->allow_resume ?
				EVT_HANDLED : EVT_CANCEL_RESUME;
}

static int goodix_fw_after_resume(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	return 0;
}

static int goodix_fw_irq_event(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	struct fw_update_ctrl *fwu_ctrl = module->priv_data;

	return fwu_ctrl->allow_irq ?
				EVT_HANDLED : EVT_CANCEL_IRQEVT;
}

static int goodix_fw_before_reset(struct goodix_ts_core *core_data,
				struct goodix_ext_module *module)
{
	struct fw_update_ctrl *fwu_ctrl = module->priv_data;

	return fwu_ctrl->allow_reset ?
				EVT_HANDLED : EVT_CANCEL_RESET;
}

static const struct goodix_ext_module_funcs goodix_ext_funcs = {
	.init = goodix_fw_update_init,
	.exit = goodix_fw_update_exit,
	.before_reset = goodix_fw_before_reset,
	.after_reset = NULL,
	.before_suspend = goodix_fw_before_suspend,
	.after_suspend = NULL,
	.before_resume = goodix_fw_before_resume,
	.after_resume = goodix_fw_after_resume,
	.irq_event = goodix_fw_irq_event,
};

static struct goodix_ext_module goodix_fwu_module = {
	.name = "goodix-fwu",
	.funcs = &goodix_ext_funcs,
	.priority = EXTMOD_PRIO_FWUPDATE,
};

static int __init goodix_fwu_module_init(void)
{
	return goodix_register_ext_module(&goodix_fwu_module);
}

static void __exit goodix_fwu_module_exit(void)
{
	return;
}

module_init(goodix_fwu_module_init);
module_exit(goodix_fwu_module_exit);

MODULE_DESCRIPTION("Goodix FWU Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
