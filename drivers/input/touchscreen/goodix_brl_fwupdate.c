 /*
  * Goodix Touchscreen Driver
  * Copyright (C) 2020 - 2021 Goodix, Inc.
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
  */
#include "goodix_ts_core.h"

#define BUS_TYPE_SPI					1
#define BUS_TYPE_I2C					0

#define GOODIX_BUS_RETRY_TIMES			3

#define FW_HEADER_SIZE_BRA				256
#define FW_HEADER_SIZE					512
#define FW_SUBSYS_INFO_SIZE				10
#define FW_SUBSYS_INFO_OFFSET_BRA		36
#define FW_SUBSYS_INFO_OFFSET			42
#define FW_SUBSYS_MAX_NUM				47

#define ISP_MAX_BUFFERSIZE				4096

#define FW_PID_LEN						8
#define FW_VID_LEN						4
#define FLASH_CMD_LEN					11

#define FW_FILE_CHECKSUM_OFFSET			8
#define CONFIG_DATA_TYPE				4

#define ISP_RAM_ADDR_BRA				0x18400
#define ISP_RAM_ADDR_BRB				0x57000
#define ISP_RAM_ADDR_BRD				0x23800
#define HW_REG_CPU_RUN_FROM				0x10000
#define FLASH_CMD_REG_BRA				0x10400
#define FLASH_CMD_REG_BRB				0x13400
#define FLASH_CMD_REG_BRD				0x12400
#define HW_REG_ISP_BUFFER_BRA			0x10410
#define HW_REG_ISP_BUFFER_BRB			0x13410
#define HW_REG_ISP_BUFFER_BRD			0x12410
#define CONFIG_DATA_ADDR_BRA			0x3E000
#define CONFIG_DATA_ADDR_BRB			0x40000
#define CONFIG_DATA_ADDR_BRD			0x3E000
#define GOODIX_CFG_ID_ADDR_BRA			0x1006E
#define GOODIX_CFG_ID_ADDR_BRB			0x10076
#define GOODIX_CFG_ID_ADDR_BRD			0x10076

#define HOLD_CPU_REG_W					0x0002
#define HOLD_CPU_REG_R					0x2000
#define MISCTL_REG_BRA					0xD807
#define MISCTL_REG_BRB					0xD80B
#define MISCTL_REG_BRD					0xD804
#define ENABLE_MISCTL_BRA				0x08
#define ENABLE_MISCTL_BRB				0x40
#define ENABLE_MISCTL_BRD				0x20700000
#define ESD_KEY_REG						0xCC58
#define WATCH_DOG_REG_BRA				0xCC54
#define WATCH_DOG_REG_BRB				0xD054
#define WATCH_DOG_REG_BRD				0xD040

#define FLASH_CMD_TYPE_READ				0xAA
#define FLASH_CMD_TYPE_WRITE			0xBB
#define FLASH_CMD_ACK_CHK_PASS			0xEE
#define FLASH_CMD_ACK_CHK_ERROR			0x33
#define FLASH_CMD_ACK_IDLE				0x11
#define FLASH_CMD_W_STATUS_CHK_PASS		0x22
#define FLASH_CMD_W_STATUS_CHK_FAIL		0x33
#define FLASH_CMD_W_STATUS_ADDR_ERR		0x44
#define FLASH_CMD_W_STATUS_WRITE_ERR	0x55
#define FLASH_CMD_W_STATUS_WRITE_OK		0xEE

#define CHIP_TYPE_BRA					0x96
#define CHIP_TYPE_BRB					0x97
#define CHIP_TYPE_BRD					0x98


struct update_info_t {
	int header_size;
	int subsys_info_offset;
	u32 isp_ram_reg;
	u32 flash_cmd_reg;
	u32 isp_buffer_reg;
	u32 config_data_reg;
	u32 misctl_reg;
	u32 watch_dog_reg;
	u32 config_id_reg;
	u32 enable_misctl_val;
};

/* berlinA update into */
struct update_info_t update_bra = {
	FW_HEADER_SIZE_BRA,
	FW_SUBSYS_INFO_OFFSET_BRA,
	ISP_RAM_ADDR_BRA,
	FLASH_CMD_REG_BRA,
	HW_REG_ISP_BUFFER_BRA,
	CONFIG_DATA_ADDR_BRA,
	MISCTL_REG_BRA,
	WATCH_DOG_REG_BRA,
	GOODIX_CFG_ID_ADDR_BRA,
	ENABLE_MISCTL_BRA,
};

/* berlinB update info */
struct update_info_t update_brb = {
	FW_HEADER_SIZE,
	FW_SUBSYS_INFO_OFFSET,
	ISP_RAM_ADDR_BRB,
	FLASH_CMD_REG_BRB,
	HW_REG_ISP_BUFFER_BRB,
	CONFIG_DATA_ADDR_BRB,
	MISCTL_REG_BRB,
	WATCH_DOG_REG_BRB,
	GOODIX_CFG_ID_ADDR_BRB,
	ENABLE_MISCTL_BRB,
};

/* berlinD update info */
struct update_info_t update_brd = {
	FW_HEADER_SIZE,
	FW_SUBSYS_INFO_OFFSET,
	ISP_RAM_ADDR_BRD,
	FLASH_CMD_REG_BRD,
	HW_REG_ISP_BUFFER_BRD,
	CONFIG_DATA_ADDR_BRD,
	MISCTL_REG_BRD,
	WATCH_DOG_REG_BRD,
	GOODIX_CFG_ID_ADDR_BRD,
	ENABLE_MISCTL_BRD,
};

/**
 * fw_subsys_info - subsytem firmware information
 * @type: sybsystem type
 * @size: firmware size
 * @flash_addr: flash address
 * @data: firmware data
 */
struct fw_subsys_info {
	u8 type;
	u32 size;
	u32 flash_addr;
	const u8 *data;
};

/**
 *  firmware_summary
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
 * @bus_type: 0 represent I2C, 1 for SPI
 * @subsys: sybsystem info
 */
#pragma pack(1)
struct  firmware_summary {
	u32 size;
	u32 checksum;
	u8 hw_pid[6];
	u8 hw_vid[3];
	u8 fw_pid[FW_PID_LEN];
	u8 fw_vid[FW_VID_LEN];
	u8 subsys_num;
	u8 chip_type;
	u8 protocol_ver;
	u8 bus_type;
	u8 flash_protect;
	// u8 reserved[8];
	struct fw_subsys_info subsys[FW_SUBSYS_MAX_NUM];
};
#pragma pack()

/**
 * firmware_data - firmware data structure
 * @fw_summary: firmware information
 * @firmware: firmware data structure
 */
struct firmware_data {
	struct  firmware_summary fw_summary;
	const struct firmware *firmware;
};

struct config_data {
	u8 *data;
	int size;
};

#pragma pack(1)
struct goodix_flash_cmd {
	union {
		struct {
			u8 status;
			u8 ack;
			u8 len;
			u8 cmd;
			u8 fw_type;
			u16 fw_len;
			u32 fw_addr;
			//u16 checksum;
		};
		u8 buf[16];
	};
};
#pragma pack()

enum update_status {
	UPSTA_NOTWORK = 0,
	UPSTA_PREPARING,
	UPSTA_UPDATING,
	UPSTA_SUCCESS,
	UPSTA_FAILED
};

enum compare_status {
	COMPARE_EQUAL = 0,
	COMPARE_NOCODE,
	COMPARE_PIDMISMATCH,
	COMPARE_FW_NOTEQUAL,
	COMPARE_CFG_NOTEQUAL,
};

/**
 * fw_update_ctrl - structure used to control the
 *  firmware update process
 * @initialized: struct init state
 * @mode: indicate weather reflash config or not, fw data source,
 *        and run on block mode or not.
 * @status: update status
 * @progress: indicate the progress of update
 * @fw_data: firmware data
 * @fw_name: firmware name
 * @attr_fwimage: sysfs bin attrs, for storing fw image
 * @fw_data_src: firmware data source form sysfs, request or head file
 * @kobj: pointer to the sysfs kobject
 */
struct fw_update_ctrl {
	struct mutex mutex;
	int initialized;
	char fw_name[GOODIX_MAX_STR_LABLE_LEN];
	int mode;
	enum update_status status;
	int spend_time;

	struct firmware_data fw_data;
	struct goodix_ic_config *ic_config;
	struct goodix_ts_core *core_data;
	struct update_info_t *update_info;

	struct bin_attribute attr_fwimage;
	struct kobject *kobj;
};
static struct fw_update_ctrl goodix_fw_update_ctrl;

static int goodix_fw_update_reset(int delay)
{
	struct goodix_ts_hw_ops *hw_ops;

	hw_ops = goodix_fw_update_ctrl.core_data->hw_ops;
	return hw_ops->reset(goodix_fw_update_ctrl.core_data, delay);
}

static int get_fw_version_info(struct goodix_fw_version *fw_version)
{
	struct goodix_ts_hw_ops *hw_ops =
		goodix_fw_update_ctrl.core_data->hw_ops;

	return hw_ops->read_version(goodix_fw_update_ctrl.core_data,
				fw_version);
}

static int goodix_reg_write(unsigned int addr,
			unsigned char *data, unsigned int len)
{
	struct goodix_ts_hw_ops *hw_ops =
			goodix_fw_update_ctrl.core_data->hw_ops;

	return hw_ops->write(goodix_fw_update_ctrl.core_data,
			addr, data, len);
}

static int goodix_reg_read(unsigned int addr,
			unsigned char *data, unsigned int len)
{
	struct goodix_ts_hw_ops *hw_ops =
			goodix_fw_update_ctrl.core_data->hw_ops;

	return hw_ops->read(goodix_fw_update_ctrl.core_data,
			addr, data, len);
}

/**
 * goodix_parse_firmware - parse firmware header information
 *	and subsystem information from firmware data buffer
 *
 * @fw_data: firmware struct, contains firmware header info
 *	and firmware data.
 * return: 0 - OK, < 0 - error
 */
/* sizeof(length) + sizeof(checksum) */

static int goodix_parse_firmware(struct firmware_data *fw_data)
{
	const struct firmware *firmware;
	struct  firmware_summary *fw_summary;
	unsigned int i, fw_offset, info_offset;
	u32 checksum;
	int ic_type =
		goodix_fw_update_ctrl.core_data->bus->ic_type;
	int subsys_info_offset =
		goodix_fw_update_ctrl.update_info->subsys_info_offset;
	int header_size =
		goodix_fw_update_ctrl.update_info->header_size;
	int r = 0;

	fw_summary = &fw_data->fw_summary;

	/* copy firmware head info */
	firmware = fw_data->firmware;
	if (firmware->size < subsys_info_offset) {
		ts_err("Invalid firmware size:%zu", firmware->size);
		r = -EINVAL;
		goto err_size;
	}
	memcpy(fw_summary, firmware->data, sizeof(*fw_summary));

	/* check firmware size */
	fw_summary->size = le32_to_cpu(fw_summary->size);
	if (firmware->size != fw_summary->size + FW_FILE_CHECKSUM_OFFSET) {
		ts_err("Bad firmware, size not match, %zu != %d",
			firmware->size, fw_summary->size + 6);
		r = -EINVAL;
		goto err_size;
	}

	for (i = FW_FILE_CHECKSUM_OFFSET, checksum = 0;
	     i < firmware->size; i += 2)
		checksum += firmware->data[i] + (firmware->data[i+1] << 8);

	/* byte order change, and check */
	fw_summary->checksum = le32_to_cpu(fw_summary->checksum);
	if (checksum != fw_summary->checksum) {
		ts_err("Bad firmware, cheksum error");
		r = -EINVAL;
		goto err_size;
	}

	if (fw_summary->subsys_num > FW_SUBSYS_MAX_NUM) {
		ts_err("Bad firmware, invalid subsys num: %d",
		       fw_summary->subsys_num);
		r = -EINVAL;
		goto err_size;
	}

	/* parse subsystem info */
	fw_offset = header_size;
	for (i = 0; i < fw_summary->subsys_num; i++) {
		info_offset = subsys_info_offset +
					i * FW_SUBSYS_INFO_SIZE;

		fw_summary->subsys[i].type = firmware->data[info_offset];
		fw_summary->subsys[i].size =
		    le32_to_cpup((__le32 *)&firmware->data[info_offset + 1]);

		fw_summary->subsys[i].flash_addr =
		    le32_to_cpup((__le32 *)&firmware->data[info_offset + 5]);
		if (fw_offset > firmware->size) {
			ts_err("Sybsys offset exceed Firmware size");
			goto err_size;
		}

		fw_summary->subsys[i].data = firmware->data + fw_offset;
		fw_offset += fw_summary->subsys[i].size;
	}

	ts_info("Firmware package protocol: V%u", fw_summary->protocol_ver);
	ts_info("Firmware PID:GT%s", fw_summary->fw_pid);
	ts_info("Firmware VID:%*ph", 4, fw_summary->fw_vid);
	ts_info("Firmware chip type:0x%02X", fw_summary->chip_type);
	ts_info("Firmware bus type:%s",
		(fw_summary->bus_type & BUS_TYPE_SPI) ? "SPI" : "I2C");
	ts_info("Firmware size:%u", fw_summary->size);
	ts_info("Firmware subsystem num:%u", fw_summary->subsys_num);

	for (i = 0; i < fw_summary->subsys_num; i++) {
		ts_debug("------------------------------------------");
		ts_debug("Index:%d", i);
		ts_debug("Subsystem type:%02X", fw_summary->subsys[i].type);
		ts_debug("Subsystem size:%u", fw_summary->subsys[i].size);
		ts_debug("Subsystem flash_addr:%08X",
				fw_summary->subsys[i].flash_addr);
		ts_debug("Subsystem Ptr:%p", fw_summary->subsys[i].data);
	}

	if (fw_summary->chip_type == CHIP_TYPE_BRA &&
		ic_type != IC_TYPE_BERLIN_A) {
		ts_err("ic type mismatch!");
		r = -EINVAL;
	} else if (fw_summary->chip_type == CHIP_TYPE_BRB &&
		ic_type != IC_TYPE_BERLIN_B) {
		ts_err("ic type mismatch!");
		r = -EINVAL;
	} else if (fw_summary->chip_type == CHIP_TYPE_BRD &&
		ic_type != IC_TYPE_BERLIN_D) {
		ts_err("ic type mismatch!");
		r = -EINVAL;
	}

err_size:
	return r;
}

/**
 * goodix_fw_version_compare - compare the active version with
 * firmware file version.
 * @fwu_ctrl: firmware information to be compared
 * return: 0 equal, < 0 unequal
 */
#define GOODIX_NOCODE "NOCODE"
static int goodix_fw_version_compare(struct fw_update_ctrl *fwu_ctrl)
{
	int ret = 0;
	struct goodix_fw_version fw_version;
	struct firmware_summary *fw_summary = &fwu_ctrl->fw_data.fw_summary;
	u32 config_id_reg = goodix_fw_update_ctrl.update_info->config_id_reg;
	u32 file_cfg_id;
	u32 ic_cfg_id;

	/* compare fw_version */
	ret = get_fw_version_info(&fw_version);
	if (ret)
		return -EINVAL;

	if (!memcmp(fw_version.rom_pid, GOODIX_NOCODE, 6) ||
		!memcmp(fw_version.patch_pid, GOODIX_NOCODE, 6)) {
		ts_info("there is no code in the chip");
		return COMPARE_NOCODE;
	}

	if (memcmp(fw_version.patch_pid, fw_summary->fw_pid, FW_PID_LEN)) {
		ts_err("Product ID mismatch:%s != %s",
			fw_version.patch_pid, fw_summary->fw_pid);
		return COMPARE_PIDMISMATCH;
	}

	ret = memcmp(fw_version.patch_vid, fw_summary->fw_vid, FW_VID_LEN);
	if (ret) {
		ts_info("active firmware version:%*ph", FW_VID_LEN,
				fw_version.patch_vid);
		ts_info("firmware file version: %*ph", FW_VID_LEN,
				fw_summary->fw_vid);
		return COMPARE_FW_NOTEQUAL;
	}
	ts_info("fw_version equal");

	/* compare config id */
	if (fwu_ctrl->ic_config && fwu_ctrl->ic_config->len > 0) {
		file_cfg_id =
			goodix_get_file_config_id(fwu_ctrl->ic_config->data);
		goodix_reg_read(config_id_reg,
			(u8 *)&ic_cfg_id, sizeof(ic_cfg_id));
		if (ic_cfg_id != file_cfg_id) {
			ts_info("ic_cfg_id:0x%x != file_cfg_id:0x%x",
				ic_cfg_id, file_cfg_id);
			return COMPARE_CFG_NOTEQUAL;
		}
		ts_info("config_id equal");
	}

	return COMPARE_EQUAL;
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
static int goodix_reg_write_confirm(unsigned int addr,
		unsigned char *data, unsigned int len)
{
	u8 *cfm = NULL;
	u8 cfm_buf[32];
	int r, i;

	if (len > sizeof(cfm_buf)) {
		cfm = kzalloc(len, GFP_KERNEL);
		if (!cfm)
			return -ENOMEM;
	} else {
		cfm = &cfm_buf[0];
	}

	for (i = 0; i < GOODIX_BUS_RETRY_TIMES; i++) {
		r = goodix_reg_write(addr, data, len);
		if (r < 0)
			goto exit;

		r = goodix_reg_read(addr, cfm, len);
		if (r < 0)
			goto exit;

		if (memcmp(data, cfm, len)) {
			r = -EINVAL;
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


/**
 * goodix_load_isp - load ISP program to device ram
 * @dev: pointer to touch device
 * @fw_data: firmware data
 * return 0 ok, <0 error
 */
static int goodix_load_isp(struct firmware_data *fw_data)
{
	struct goodix_fw_version isp_fw_version;
	struct fw_subsys_info *fw_isp;
	u32 isp_ram_reg = goodix_fw_update_ctrl.update_info->isp_ram_reg;
	u8 reg_val[8] = {0x00};
	int r;

	memset(&isp_fw_version, 0, sizeof(isp_fw_version));
	fw_isp = &fw_data->fw_summary.subsys[0];

	ts_info("Loading ISP start");
	r = goodix_reg_write_confirm(isp_ram_reg,
					(u8 *)fw_isp->data, fw_isp->size);
	if (r < 0) {
		ts_err("Loading ISP error");
		return r;
	}

	ts_info("Success send ISP data");

	/* SET BOOT OPTION TO 0X55 */
	memset(reg_val, 0x55, 8);
	r = goodix_reg_write_confirm(HW_REG_CPU_RUN_FROM, reg_val, 8);
	if (r < 0) {
		ts_err("Failed set REG_CPU_RUN_FROM flag");
		return r;
	}
	ts_info("Success write [8]0x55 to 0x%x", HW_REG_CPU_RUN_FROM);

	if (goodix_fw_update_reset(100))
		ts_err("reset abnormal");
	/*check isp state */
	if (get_fw_version_info(&isp_fw_version)) {
		ts_err("failed read isp version");
		return -2;
	}
	if (memcmp(&isp_fw_version.patch_pid[3], "ISP", 3)) {
		ts_err("patch id error %c%c%c != %s",
		isp_fw_version.patch_pid[3], isp_fw_version.patch_pid[4],
		isp_fw_version.patch_pid[5], "ISP");
		return -3;
	}
	ts_info("ISP running successfully");
	return 0;
}

/**
 * goodix_update_prepare - update prepare, loading ISP program
 *  and make sure the ISP is running.
 * @fwu_ctrl: pointer to fimrware control structure
 * return: 0 ok, <0 error
 */
static int goodix_update_prepare(struct fw_update_ctrl *fwu_ctrl)
{
	u32 misctl_reg = fwu_ctrl->update_info->misctl_reg;
	u32 watch_dog_reg = fwu_ctrl->update_info->watch_dog_reg;
	u32 enable_misctl_val = fwu_ctrl->update_info->enable_misctl_val;
	u8 reg_val[4] = {0};
	u8 temp_buf[64] = {0};
	int retry = 20;
	int r;

	/*reset IC*/
	ts_info("firmware update, reset");
	if (goodix_fw_update_reset(5))
		ts_err("reset abnormal");

	retry = 100;
	/* Hold cpu*/
	do {
		reg_val[0] = 0x01;
		reg_val[1] = 0x00;
		r = goodix_reg_write(HOLD_CPU_REG_W, reg_val, 2);
		r |= goodix_reg_read(HOLD_CPU_REG_R, &temp_buf[0], 4);
		r |= goodix_reg_read(HOLD_CPU_REG_R, &temp_buf[4], 4);
		r |= goodix_reg_read(HOLD_CPU_REG_R, &temp_buf[8], 4);
		if (!r && !memcmp(&temp_buf[0], &temp_buf[4], 4) &&
			!memcmp(&temp_buf[4], &temp_buf[8], 4) &&
			!memcmp(&temp_buf[0], &temp_buf[8], 4)) {
			break;
		}
		usleep_range(1000, 1100);
		ts_info("retry hold cpu %d", retry);
		ts_debug("data:%*ph", 12, temp_buf);
	} while (--retry);
	if (!retry) {
		ts_err("Failed to hold CPU, return =%d", r);
		return -1;
	}
	ts_info("Success hold CPU");

	/* enable misctl clock */
	if (fwu_ctrl->core_data->bus->ic_type == IC_TYPE_BERLIN_D)
		goodix_reg_write(misctl_reg, (u8 *)&enable_misctl_val, 4);
	else
		goodix_reg_write(misctl_reg, (u8 *)&enable_misctl_val, 1);
	ts_info("enbale misctl clock");

	if (fwu_ctrl->core_data->bus->ic_type == IC_TYPE_BERLIN_A) {
		/* open ESD_KEY */
		retry = 20;
		do {
			reg_val[0] = 0x95;
			r = goodix_reg_write(ESD_KEY_REG, reg_val, 1);
			r |= goodix_reg_read(ESD_KEY_REG, temp_buf, 1);
			if (!r && temp_buf[0] == 0x01)
				break;
			usleep_range(1000, 1100);
			ts_info("retry %d enable esd key, 0x%x",
				retry, temp_buf[0]);
		} while (--retry);
		if (!retry) {
			ts_err("Failed to enable esd key, return =%d", r);
			return -2;
		}
		ts_info("success enable esd key");
	}

	/* disable watch dog */
	reg_val[0] = 0x00;
	r = goodix_reg_write(watch_dog_reg, reg_val, 1);
	ts_info("disable watch dog");

	/* load ISP code and run form isp */
	r = goodix_load_isp(&fwu_ctrl->fw_data);
	if (r < 0)
		ts_err("Failed load and run isp");

	return r;
}

/*	goodix_send_flash_cmd: send command to read or write flash data
 *	@flash_cmd: command need to send.
 */
static int goodix_send_flash_cmd(struct goodix_flash_cmd *flash_cmd)
{
	int i, ret, retry;
	struct goodix_flash_cmd tmp_cmd;
	u32 flash_cmd_reg = goodix_fw_update_ctrl.update_info->flash_cmd_reg;

	ts_info("try send flash cmd:%*ph", (int)sizeof(flash_cmd->buf),
		flash_cmd->buf);
	memset(tmp_cmd.buf, 0, sizeof(tmp_cmd));
	ret = goodix_reg_write(flash_cmd_reg,
		flash_cmd->buf, sizeof(flash_cmd->buf));
	if (ret) {
		ts_err("failed send flash cmd %d", ret);
		return ret;
	}

	retry = 5;
	for (i = 0; i < retry; i++) {
		ret = goodix_reg_read(flash_cmd_reg,
			tmp_cmd.buf, sizeof(tmp_cmd.buf));
		if (!ret && tmp_cmd.ack == FLASH_CMD_ACK_CHK_PASS)
			break;
		usleep_range(5000, 5100);
		ts_info("flash cmd ack error retry %d, ack 0x%x, ret %d",
			i, tmp_cmd.ack, ret);
	}
	if (tmp_cmd.ack != FLASH_CMD_ACK_CHK_PASS) {
		ts_err("flash cmd ack error, ack 0x%x, ret %d",
			tmp_cmd.ack, ret);
		ts_err("data:%*ph", (int)sizeof(tmp_cmd.buf), tmp_cmd.buf);
		return -EINVAL;
	}
	ts_info("flash cmd ack check pass");

	msleep(80);
	retry = 20;
	for (i = 0; i < retry; i++) {
		ret = goodix_reg_read(flash_cmd_reg,
			tmp_cmd.buf, sizeof(tmp_cmd.buf));
		if (!ret && tmp_cmd.ack == FLASH_CMD_ACK_CHK_PASS &&
			tmp_cmd.status == FLASH_CMD_W_STATUS_WRITE_OK) {
			ts_info("flash status check pass");
			return 0;
		}

		ts_info("flash cmd status not ready, retry %d, ack 0x%x, status 0x%x, ret %d",
			i, tmp_cmd.ack, tmp_cmd.status, ret);
		msleep(20);
	}

	ts_err("flash cmd status error %d, ack 0x%x, status 0x%x, ret %d",
		i, tmp_cmd.ack, tmp_cmd.status, ret);
	if (ret) {
		ts_info("reason: bus or paltform error");
		return -EINVAL;
	}

	switch (tmp_cmd.status) {
	case FLASH_CMD_W_STATUS_CHK_PASS:
		ts_err("data check pass, but failed get follow-up results");
		return -EFAULT;
	case FLASH_CMD_W_STATUS_CHK_FAIL:
		ts_err("data check failed, please retry");
		return -EAGAIN;
	case FLASH_CMD_W_STATUS_ADDR_ERR:
		ts_err("flash target addr error, please check");
		return -EFAULT;
	case FLASH_CMD_W_STATUS_WRITE_ERR:
		ts_err("flash data write err, please retry");
		return -EAGAIN;
	default:
		ts_err("unknown status");
		return -EFAULT;
	}
}

static int goodix_flash_package(u8 subsys_type, u8 *pkg,
	u32 flash_addr, u16 pkg_len)
{
	int ret, retry;
	struct goodix_flash_cmd flash_cmd;
	u32 isp_buffer_reg = goodix_fw_update_ctrl.update_info->isp_buffer_reg;

	retry = 2;
	do {
		ret = goodix_reg_write(isp_buffer_reg, pkg, pkg_len);
		if (ret < 0) {
			ts_err("Failed to write firmware packet");
			return ret;
		}

		flash_cmd.status = 0;
		flash_cmd.ack = 0;
		flash_cmd.len = FLASH_CMD_LEN;
		flash_cmd.cmd = FLASH_CMD_TYPE_WRITE;
		flash_cmd.fw_type = subsys_type;
		flash_cmd.fw_len = cpu_to_le16(pkg_len);
		flash_cmd.fw_addr = cpu_to_le32(flash_addr);

		goodix_append_checksum(&(flash_cmd.buf[2]),
				9, CHECKSUM_MODE_U8_LE);

		ret = goodix_send_flash_cmd(&flash_cmd);
		if (!ret) {
			ts_info("success write package to 0x%x, len %d",
				flash_addr, pkg_len - 4);
			return 0;
		}
	} while (ret == -EAGAIN && --retry);

	return ret;
}

/**
 * goodix_flash_subsystem - flash subsystem firmware,
 *  Main flow of flashing firmware.
 *	Each firmware subsystem is divided into several
 *	packets, the max size of packet is limited to
 *	@{ISP_MAX_BUFFERSIZE}
 * @dev: pointer to touch device
 * @subsys: subsystem information
 * return: 0 ok, < 0 error
 */
static int goodix_flash_subsystem(struct fw_subsys_info *subsys)
{
	u32 data_size, offset;
	u32 total_size;
	//TODO: confirm flash addr ,<< 8??
	u32 subsys_base_addr = subsys->flash_addr;
	u8 *fw_packet = NULL;
	int r = 0;

	/*
	 * if bus(i2c/spi) error occued, then exit, we will do
	 * hardware reset and re-prepare ISP and then retry
	 * flashing
	 */
	total_size = subsys->size;
	fw_packet = kzalloc(ISP_MAX_BUFFERSIZE + 4, GFP_KERNEL);
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

		memcpy(fw_packet, &subsys->data[offset], data_size);
		/* set checksum for package data */
		goodix_append_checksum(fw_packet,
				data_size, CHECKSUM_MODE_U16_LE);

		r = goodix_flash_package(subsys->type, fw_packet,
				subsys_base_addr + offset, data_size + 4);
		if (r) {
			ts_err("failed flash to %08x,size:%u bytes",
			subsys_base_addr + offset, data_size);
			break;
		}
		offset += data_size;
		total_size -= data_size;
	} /* end while */

	kfree(fw_packet);
	return r;
}

/**
 * goodix_flash_firmware - flash firmware
 * @dev: pointer to touch device
 * @fw_data: firmware data
 * return: 0 ok, < 0 error
 */
static int goodix_flash_firmware(struct fw_update_ctrl *fw_ctrl)
{
	struct firmware_data *fw_data = &fw_ctrl->fw_data;
	struct  firmware_summary  *fw_summary;
	struct fw_subsys_info *fw_x;
	struct fw_subsys_info subsys_cfg = {0};
	u32 config_data_reg = fw_ctrl->update_info->config_data_reg;
	int retry = GOODIX_BUS_RETRY_TIMES;
	int i, r = 0, fw_num;

	/*	start from subsystem 1,
	 *	subsystem 0 is the ISP program
	 */

	fw_summary = &fw_data->fw_summary;
	fw_num = fw_summary->subsys_num;

	/* flash config data first if we have */
	if (fw_ctrl->ic_config && fw_ctrl->ic_config->len) {
		subsys_cfg.data = fw_ctrl->ic_config->data;
		subsys_cfg.size = fw_ctrl->ic_config->len;
		subsys_cfg.flash_addr = config_data_reg;
		subsys_cfg.type = CONFIG_DATA_TYPE;
		r = goodix_flash_subsystem(&subsys_cfg);
		if (r) {
			ts_err("failed flash config with ISP, %d", r);
			return r;
		}
		ts_info("success flash config with ISP");
	}

	for (i = 1; i < fw_num && retry;) {
		ts_info("--- Start to flash subsystem[%d] ---", i);
		fw_x = &fw_summary->subsys[i];
		r = goodix_flash_subsystem(fw_x);
		if (r == 0) {
			ts_info("--- End flash subsystem[%d]: OK ---", i);
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

exit_flash:
	return r;
}

/**
 * goodix_update_finish - update finished, FREE resource
 *  and reset flags---
 * @fwu_ctrl: pointer to fw_update_ctrl structrue
 * return: 0 ok, < 0 error
 */
static int goodix_update_finish(struct fw_update_ctrl *fwu_ctrl)
{
	int ret;

	if (goodix_fw_update_reset(100))
		ts_err("reset abnormal");
	ret = goodix_fw_version_compare(fwu_ctrl);
	if (ret == COMPARE_EQUAL || ret == COMPARE_CFG_NOTEQUAL)
		return 0;

	return -EINVAL;
}

/**
 * goodix_fw_update_proc - firmware update process, the entry of
 *  firmware update flow
 * @fwu_ctrl: firmware control
 * return: = 0 update ok, < 0 error or NO_NEED_UPDATE
 */
int goodix_fw_update_proc(struct fw_update_ctrl *fwu_ctrl)
{
#define FW_UPDATE_RETRY		2
	int retry0 = FW_UPDATE_RETRY;
	int retry1 = FW_UPDATE_RETRY;
	int ret = 0;

	ret = goodix_parse_firmware(&fwu_ctrl->fw_data);
	if (ret < 0)
		return ret;

	if (!(fwu_ctrl->mode & UPDATE_MODE_FORCE)) {
		ret = goodix_fw_version_compare(fwu_ctrl);
		if (!ret) {
			ts_info("firmware upgraded");
			return 0;
		} else
			ts_info("need to upgrade");
	}

start_update:
	fwu_ctrl->status = UPSTA_PREPARING;
	do {
		ret = goodix_update_prepare(fwu_ctrl);
		if (ret) {
			ts_err("failed prepare ISP, retry %d",
				FW_UPDATE_RETRY - retry0);
		}
	} while (ret && --retry0 > 0);
	if (ret) {
		ts_err("Failed to prepare ISP, exit update:%d", ret);
		goto err_fw_prepare;
	}

	/* progress: 20%~100% */
	fwu_ctrl->status = UPSTA_UPDATING;
	ret = goodix_flash_firmware(fwu_ctrl);
	if (ret < 0 && --retry1 > 0) {
		ts_err("Bus error, retry firmware update:%d",
				FW_UPDATE_RETRY - retry1);
		goto start_update;
	}
	if (ret)
		ts_err("flash fw data enter error, ret:%d", ret);
	else
		ts_info("flash fw data success, need check version");

err_fw_prepare:
	ret = goodix_update_finish(fwu_ctrl);
	if (!ret)
		ts_info("Firmware update successfully");
	else
		ts_err("Firmware update failed, ret:%d", ret);

	return ret;
}

/*
 * goodix_sysfs_update_en_store: start fw update manually
 * @buf: '1'[001] update in blocking mode with fwdata from sysfs
 *       '2'[010] update in blocking mode with fwdata from request
 *       '5'[101] update in unblocking mode with fwdata from sysfs
 *       '6'[110] update in unblocking mode with fwdata from request
 */
static ssize_t goodix_sysfs_update_en_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;
	int mode = 0;
	struct fw_update_ctrl *fw_ctrl = &goodix_fw_update_ctrl;

	if (!buf || count <= 0) {
		ts_err("invalid params");
		return -EINVAL;
	}
	if (!fw_ctrl || !fw_ctrl->initialized) {
		ts_err("fw module uninit");
		return -EINVAL;
	}

	ts_info("set update mode:0x%x", buf[0]);
	if (buf[0] == '1') {
		mode = UPDATE_MODE_FORCE | UPDATE_MODE_BLOCK |
			UPDATE_MODE_SRC_SYSFS;
	} else if (buf[0] == '2') {
		mode = UPDATE_MODE_FORCE | UPDATE_MODE_BLOCK |
			UPDATE_MODE_SRC_REQUEST;
	} else if (buf[0] == '5') {
		mode = UPDATE_MODE_FORCE | UPDATE_MODE_SRC_SYSFS;
	} else if (buf[0] == '6') {
		mode = UPDATE_MODE_FORCE | UPDATE_MODE_SRC_REQUEST;
	} else {
		ts_err("invalid update mode:0x%x", buf[0]);
		return -EINVAL;
	}

	ret = goodix_do_fw_update(NULL, mode);
	if (!ret) {
		ts_info("success do update work");
		return count;
	}
	ts_err("failed do fw update work");
	return -EINVAL;
}

static ssize_t goodix_sysfs_fwsize_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct fw_update_ctrl *fw_ctrl = &goodix_fw_update_ctrl;
	int r = -EINVAL;

	if (fw_ctrl && fw_ctrl->fw_data.firmware)
		r = snprintf(buf, PAGE_SIZE, "%zu\n",
				fw_ctrl->fw_data.firmware->size);
	return r;
}

static ssize_t goodix_sysfs_fwsize_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fw_update_ctrl *fw_ctrl = &goodix_fw_update_ctrl;
	struct firmware *fw;
	u8 **data;
	size_t size = 0;

	if (!fw_ctrl)
		return -EINVAL;

	if (sscanf(buf, "%zu", &size) < 0 || !size) {
		ts_err("Failed to get fwsize");
		return -EFAULT;
	}

	/* use vmalloc to alloc huge memory */
	fw = vmalloc(sizeof(*fw) + size);
	if (!fw)
		return -ENOMEM;
	mutex_lock(&fw_ctrl->mutex);
	memset(fw, 0x00, sizeof(*fw) + size);
	data = (u8 **)&fw->data;
	*data = (u8 *)fw + sizeof(struct firmware);
	fw->size = size;
	fw_ctrl->fw_data.firmware = fw;
	fw_ctrl->mode = UPDATE_MODE_SRC_SYSFS;
	mutex_unlock(&fw_ctrl->mutex);
	return count;
}

static ssize_t goodix_sysfs_fwimage_store(struct file *file,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t pos, size_t count)
{
	struct fw_update_ctrl *fw_ctrl = &goodix_fw_update_ctrl;
	struct firmware_data *fw_data;

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
	mutex_lock(&fw_ctrl->mutex);
	memcpy((u8 *)&fw_data->firmware->data[pos], buf, count);
	mutex_unlock(&fw_ctrl->mutex);
	return count;
}

/* return fw_update result */
static ssize_t goodix_sysfs_result_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct fw_update_ctrl *fw_ctrl = &goodix_fw_update_ctrl;
	char str[GOODIX_MAX_STR_LABLE_LEN] = {0};
	int r = -EINVAL;

	if (!fw_ctrl)
		return r;

	switch (fw_ctrl->status) {
	case UPSTA_PREPARING:
		sprintf(str, "preparing");
		break;
	case UPSTA_UPDATING:
		sprintf(str, "updating");
		break;
	case UPSTA_SUCCESS:
		sprintf(str, "success");
		break;
	case UPSTA_FAILED:
		sprintf(str, "failed");
		break;
	case UPSTA_NOTWORK:
	default:
		sprintf(str, "notwork");
		break;
	}

	r = snprintf(buf, PAGE_SIZE, "result:%s  spend_time:%dms\n",
			str, fw_ctrl->spend_time);

	return r;
}

static DEVICE_ATTR(update_en, 0220, NULL, goodix_sysfs_update_en_store);
static DEVICE_ATTR(fwsize, 0664, goodix_sysfs_fwsize_show,
		goodix_sysfs_fwsize_store);
static DEVICE_ATTR(result, 0664, goodix_sysfs_result_show, NULL);

static struct attribute *goodix_fwu_attrs[] = {
	&dev_attr_update_en.attr,
	&dev_attr_fwsize.attr,
	&dev_attr_result.attr
};

static int goodix_fw_sysfs_init(struct goodix_ts_core *core_data,
		struct fw_update_ctrl *fw_ctrl)
{
	int ret = 0, i;

	fw_ctrl->kobj = kobject_create_and_add("fwupdate",
					&core_data->pdev->dev.kobj);
	if (!fw_ctrl->kobj) {
		ts_err("failed create sub dir for fwupdate");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(goodix_fwu_attrs) && !ret; i++)
		ret = sysfs_create_file(fw_ctrl->kobj, goodix_fwu_attrs[i]);

	if (ret) {
		ts_err("failed create fwu sysfs files");
		while (--i >= 0)
			sysfs_remove_file(fw_ctrl->kobj, goodix_fwu_attrs[i]);

		kobject_put(fw_ctrl->kobj);
		return -EINVAL;
	}

	fw_ctrl->attr_fwimage.attr.name = "fwimage";
	fw_ctrl->attr_fwimage.attr.mode = 0666;
	fw_ctrl->attr_fwimage.size = 0;
	fw_ctrl->attr_fwimage.write = goodix_sysfs_fwimage_store;
	ret = sysfs_create_bin_file(fw_ctrl->kobj, &fw_ctrl->attr_fwimage);
	if (ret) {
		ts_err("failed create fwimage bin node, %d", ret);
		for (i = 0; i < ARRAY_SIZE(goodix_fwu_attrs); i++)
			sysfs_remove_file(fw_ctrl->kobj, goodix_fwu_attrs[i]);
		kobject_put(fw_ctrl->kobj);
	}

	return ret;
}

static void goodix_fw_sysfs_remove(void)
{
	struct fw_update_ctrl *fw_ctrl = &goodix_fw_update_ctrl;
	int i;

	sysfs_remove_bin_file(fw_ctrl->kobj, &fw_ctrl->attr_fwimage);

	for (i = 0; i < ARRAY_SIZE(goodix_fwu_attrs); i++)
		sysfs_remove_file(fw_ctrl->kobj,
				goodix_fwu_attrs[i]);

	kobject_put(fw_ctrl->kobj);
}


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
	struct device *dev = &(fw_ctrl->core_data->pdev->dev);
	int r;
	int retry = GOODIX_RETRY_3;

	ts_info("Request firmware image [%s]", name);

	while (retry--) {
		r = request_firmware(&fw_data->firmware, name, dev);
		if (!r)
			break;
		ts_info("get fw bin retry:[%d]", GOODIX_RETRY_3 - retry);
		msleep(200);
	}
	if (retry < 0) {
		ts_err("Firmware image [%s] not available,errno:%d", name, r);
		return r;
	}

	ts_info("Firmware image [%s] is ready", name);
	return 0;
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
	ktime_t start, end;
	int r = -EINVAL;

	start = ktime_get();
	fwu_ctrl->spend_time = 0;
	fwu_ctrl->status = UPSTA_NOTWORK;
	mutex_lock(&fwu_ctrl->mutex);

	if (fwu_ctrl->mode & UPDATE_MODE_SRC_REQUEST) {
		ts_info("Firmware request update starts");
		r = goodix_request_firmware(&fwu_ctrl->fw_data,
						fwu_ctrl->fw_name);
		if (r < 0)
			goto out;

	} else if (fwu_ctrl->mode & UPDATE_MODE_SRC_SYSFS) {
		if (!fwu_ctrl->fw_data.firmware) {
			ts_err("Invalid firmware from sysfs");
			r = -EINVAL;
			goto out;
		}
	} else {
		ts_err("unknown update mode 0x%x", fwu_ctrl->mode);
		r = -EINVAL;
		goto out;
	}

	ts_debug("notify update start");
	goodix_ts_blocking_notify(NOTIFY_FWUPDATE_START, NULL);

	/* ready to update */
	ts_debug("start update proc");
	r = goodix_fw_update_proc(fwu_ctrl);

	/* clean */
	if (fwu_ctrl->mode & UPDATE_MODE_SRC_HEAD) {
		kfree(fwu_ctrl->fw_data.firmware);
		fwu_ctrl->fw_data.firmware = NULL;
		temp_firmware = NULL;
	} else if (fwu_ctrl->mode & UPDATE_MODE_SRC_REQUEST) {
		goodix_release_firmware(&fwu_ctrl->fw_data);
	}
out:
	fwu_ctrl->mode = UPDATE_MODE_DEFAULT;
	mutex_unlock(&fwu_ctrl->mutex);

	if (r) {
		ts_err("fw update failed, %d", r);
		fwu_ctrl->status = UPSTA_FAILED;
		goodix_ts_blocking_notify(NOTIFY_FWUPDATE_FAILED, NULL);
	} else {
		ts_info("fw update success");
		fwu_ctrl->status = UPSTA_SUCCESS;
		goodix_ts_blocking_notify(NOTIFY_FWUPDATE_SUCCESS, NULL);
	}

	end = ktime_get();
	fwu_ctrl->spend_time = ktime_to_ms(ktime_sub(end, start));

	return r;
}

int goodix_do_fw_update(struct goodix_ic_config *ic_config, int mode)
{
	struct task_struct *fwu_thrd;
	struct fw_update_ctrl *fwu_ctrl = &goodix_fw_update_ctrl;
	int ret;

	if (!fwu_ctrl->initialized) {
		ts_err("fw mode uninit");
		return -EINVAL;
	}

	fwu_ctrl->mode = mode;
	fwu_ctrl->ic_config = ic_config;
	ts_debug("fw update mode 0x%x", mode);
	if (fwu_ctrl->mode & UPDATE_MODE_BLOCK) {
		ret = goodix_fw_update_thread(fwu_ctrl);
		ts_info("fw update return %d", ret);
		return ret;
	}
	/* create and run update thread */
	fwu_thrd = kthread_run(goodix_fw_update_thread,
			fwu_ctrl, "goodix-fwu");
	if (IS_ERR_OR_NULL(fwu_thrd)) {
		ts_err("Failed to create update thread:%ld",
				PTR_ERR(fwu_thrd));
		return -EFAULT;
	}
	ts_info("success create fw update thread");
	return 0;
}

int goodix_fw_update_init(struct goodix_ts_core *core_data)
{
	int ret;

	if (!core_data || !core_data->hw_ops) {
		ts_err("core_data && hw_ops cann't be null");
		return -ENODEV;
	}

	mutex_init(&goodix_fw_update_ctrl.mutex);
	goodix_fw_update_ctrl.core_data = core_data;
	goodix_fw_update_ctrl.mode = 0;

	strlcpy(goodix_fw_update_ctrl.fw_name, core_data->board_data.fw_name,
		sizeof(goodix_fw_update_ctrl.fw_name));

	ret = goodix_fw_sysfs_init(core_data, &goodix_fw_update_ctrl);
	if (ret) {
		ts_err("failed create fwupate sysfs node");
		return ret;
	}
	if (core_data->bus->ic_type == IC_TYPE_BERLIN_A)
		goodix_fw_update_ctrl.update_info = &update_bra;
	else if (core_data->bus->ic_type == IC_TYPE_BERLIN_B)
		goodix_fw_update_ctrl.update_info = &update_brb;
	else
		goodix_fw_update_ctrl.update_info = &update_brd;

	goodix_fw_update_ctrl.initialized = 1;
	return 0;
}

void goodix_fw_update_uninit(void)
{
	if (!goodix_fw_update_ctrl.initialized)
		return;

	mutex_lock(&goodix_fw_update_ctrl.mutex);
	goodix_fw_sysfs_remove();
	goodix_fw_update_ctrl.initialized = 0;
	mutex_unlock(&goodix_fw_update_ctrl.mutex);
	mutex_destroy(&goodix_fw_update_ctrl.mutex);
}
