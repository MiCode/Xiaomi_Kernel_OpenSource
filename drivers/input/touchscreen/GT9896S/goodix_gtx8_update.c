/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "goodix_ts_core.h"
#include "goodix_cfg_bin.h"
#include "goodix_default_fw.h"
/* COMMON PART - START */

#define FW_HEADER_SIZE				256
#define FW_SUBSYS_INFO_SIZE			8
#define FW_SUBSYS_INFO_OFFSET			32
#define FW_SUBSYS_MAX_NUM			28

#define ISP_MAX_BUFFERSIZE			(1024 * 4)

#define HW_REG_CPU_CTRL				0x2180
#define HW_REG_DSP_MCU_POWER			0x2010
#define HW_REG_RESET				0x2184
#define HW_REG_SCRAMBLE				0x2218
#define HW_REG_BANK_SELECT			0x2048
#define HW_REG_ACCESS_PATCH0			0x204D
#define HW_REG_EC_SRM_START			0x204F
#define HW_REG_GIO_YS				0x2014
#define HW_REG_ESD_KEY_EN			0x2318
#define HW_REG_ESD_KEY_DIS			0x2324
#define HW_REG_CPU_RUN_FROM			0x4506 /* for nor_L is 0x4006 */
#define HW_REG_CPU_RUN_FROM_YS			0x4000
#define HW_REG_ISP_RUN_FLAG			0x6006
#define HW_REG_ISP_ADDR				0xC000
#define HW_REG_ISP_BUFFER			0x6100
#define HW_REG_SUBSYS_TYPE			0x6020
#define HW_REG_FLASH_FLAG			0x6022
#define HW_REG_CACHE				0x204B
#define HW_REG_ESD_KEY		                0x2318
#define HW_REG_WTD_TIMER			0x20B0

#define FLASH_ADDR_CONFIG_DATA			0x1E000
#define FLASH_SUBSYS_TYPE_CONFIG		0x03

#define CPU_CTRL_PENDING			0x00
#define CPU_CTRL_RUNNING			0x01

#define ISP_STAT_IDLE				0xFF
#define ISP_STAT_READY				0xAA
#define ISP_STAT_WRITING			0xAA
#define ISP_FLASH_SUCCESS			0xBB
#define ISP_FLASH_ERROR				0xCC
#define ISP_FLASH_CHECK_ERROR			0xDD
#define ISP_CMD_PREPARE				0x55
#define ISP_CMD_FLASH				0xAA

#define TS_CHECK_ISP_STATE_RETRY_TIMES     200
#define TS_READ_FLASH_STATE_RETRY_TIMES    200

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
 * @initialized: struct init state
 * @mode: indicate weather reflash config or not, fw data source,
 *        and run on block mode or not.
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
 * @fw_data_src: firmware data source form sysfs, request or head file
 */
struct fw_update_ctrl {
	struct mutex mutex;
	int initialized;
	int mode;
	enum update_status  status;
	unsigned int progress;

	bool allow_reset;
	bool allow_irq;
	bool allow_suspend;
	bool allow_resume;

	struct firmware_data fw_data;
	struct gt9896s_ts_device *ts_dev;
	struct gt9896s_ts_core *core_data;

	char fw_name[64];
	struct bin_attribute attr_fwimage;
};
static struct fw_update_ctrl gt9896s_fw_update_ctrl;
/**
 * gt9896s_parse_firmware - parse firmware header infomation
 *	and subsystem infomation from firmware data buffer
 *
 * @fw_data: firmware struct, contains firmware header info
 *	and firmware data.
 * return: 0 - OK, < 0 - error
 */
static int gt9896s_parse_firmware(struct firmware_data *fw_data)
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
	 * by u16 checksum
	 */
	for (i = 6, checksum = 0; i < firmware->size; i++)
		checksum += firmware->data[i];

	/* byte order change, and check */
	fw_info->checksum = be16_to_cpu(fw_info->checksum);
	if (checksum != fw_info->checksum) {
		ts_err("Bad firmware, cheksum error %x(file) != %x(cal)",
			fw_info->checksum, checksum);
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
	ts_info("Fimware VID:%02X%02X%02X%02x", fw_info->fw_vid[0],
				fw_info->fw_vid[1], fw_info->fw_vid[2], fw_info->fw_vid[3]);
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
 * gt9896s_check_update - compare the version of firmware running in
 *  touch device with the version getting from the firmware file.
 * @fw_info: firmware infomation to be compared
 * return: 0 no need do update,
 *         otherwise need do update
 */
static int gt9896s_check_update(struct gt9896s_ts_device *dev,
		const struct firmware_info *fw_info)
{
	struct gt9896s_ts_version fw_ver;
	int ret = -EINVAL;

	/* read version from chip, if we got invalid
	 * firmware version, maybe fimware in flash is
	 * incorrect, so we need to update firmware
	 */
	ret = dev->hw_ops->read_version(dev, &fw_ver);
	if (ret) {
		ts_info("failed get active pid");
		return -EINVAL;
	}

	if (fw_ver.valid) {
		// should we compare PID before fw update?
		// if fw patch demage the PID may unmatch but
		// we should de update to recover it.
		// TODO skip PID check
		/*if (memcmp(fw_ver.pid, fw_info->fw_pid, dev->reg.pid_len)) {
			ts_err("Product ID is not match %s != %s",
				fw_ver.pid, fw_info->fw_pid);
			return -EPERM;
		}*/

		ret = memcmp(fw_ver.vid, fw_info->fw_vid, dev->reg.vid_len);
		if (ret == 0) {
			ts_info("FW version is equal to the IC's");
			return 0;
		} else if (ret > 0) {
			ts_info("Warning: fw version is lower the IC's");
		}
	} /* else invalid firmware, update firmware */

	ts_info("Firmware needs to be updated");
	return ret;
}

/**
 * gt9896s_reg_write_confirm - write register and confirm the value
 *  in the register.
 * @dev: pointer to touch device
 * @addr: register address
 * @data: pointer to data buffer
 * @len: data length
 * return: 0 write success and confirm ok
 *		   < 0 failed
 */
static int gt9896s_reg_write_confirm(struct gt9896s_ts_device *dev,
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
			ts_info("data[0]:0x%02x, data[1]:0x%02x,"
				"read cfm[0]:0x%02x, cfm[1]:0x%02x",
				data[0], data[1], cfm[0], cfm[1]);
			dev->hw_ops->read_trans(dev, 0x6022, cfm, 2);
			ts_info("read 0x6022 data[0]:0x%02x, data[1]:0x%02x",
				cfm[0], cfm[1]);
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

static inline int gt9896s_reg_write(struct gt9896s_ts_device *dev,
		unsigned int addr, unsigned char *data, unsigned int len)
{
	return dev->hw_ops->write_trans(dev, addr, data, len);
}

static inline int gt9896s_reg_read(struct gt9896s_ts_device *dev,
		unsigned int addr, unsigned char *data, unsigned int len)
{
	return dev->hw_ops->read_trans(dev, addr, data, len);
}

/**
 * gt9896s_load_isp - load ISP program to deivce ram
 * @dev: pointer to touch device
 * @fw_data: firmware data
 * return 0 ok, <0 error
 */
static int gt9896s_load_isp(struct gt9896s_ts_device *ts_dev,
			   struct firmware_data *fw_data)
{
	struct fw_subsys_info *fw_isp;
	u8 reg_val[8] = {0x00};
	int r;
	int i;

	fw_isp = &fw_data->fw_info.subsys[0];

	ts_info("Loading ISP start");
	reg_val[0] = 0x00;
	r = gt9896s_reg_write(ts_dev, HW_REG_BANK_SELECT,
				     reg_val, 1);
	if (r < 0) {
		ts_err("Failed to select bank0");
		return r;
	}
	ts_debug("Success select bank0, Set 0x%x -->0x00", HW_REG_BANK_SELECT);

	reg_val[0] = 0x01;
	r = gt9896s_reg_write(ts_dev, HW_REG_ACCESS_PATCH0,
				     reg_val, 1);
	if (r < 0) {
		ts_err("Failed to enable patch0 access");
		return r;
	}
	ts_debug("Success select bank0, Set 0x%x -->0x01", HW_REG_ACCESS_PATCH0);

	r = gt9896s_reg_write_confirm(ts_dev, HW_REG_ISP_ADDR,
				     (u8 *)fw_isp->data, fw_isp->size);
	if (r < 0) {
		ts_err("Loading ISP error");
		return r;
	}

	ts_debug("Success send ISP data to IC");

	reg_val[0] = 0x00;
	r = gt9896s_reg_write_confirm(ts_dev, HW_REG_ACCESS_PATCH0,
				     reg_val, 1);
	if (r < 0) {
		ts_err("Failed to disable patch0 access");
		return r;
	}
	ts_debug("Success forbit bank0 accedd, set 0x%x -->0x00",
		 HW_REG_ACCESS_PATCH0);

	reg_val[0] = 0x00;
	reg_val[1] = 0x00;
	r = gt9896s_reg_write(ts_dev, HW_REG_ISP_RUN_FLAG, reg_val, 2);
	if (r < 0) {
		ts_err("Failed to clear 0x%x", HW_REG_ISP_RUN_FLAG);
		return r;
	}
	ts_debug("Success clear 0x%x", HW_REG_ISP_RUN_FLAG);

	memset(reg_val, 0x55, 8);
	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE ||
		ts_dev->ic_type == IC_TYPE_YELLOWSTONE_SPI)
		r = gt9896s_reg_write(ts_dev, HW_REG_CPU_RUN_FROM_YS,
				reg_val, 8);
	else
		r = gt9896s_reg_write(ts_dev, HW_REG_CPU_RUN_FROM,
				reg_val, 8);
	if (r < 0) {
		ts_err("Failed set backdoor flag");
		return r;
	}
	ts_info("Success write [8]0x55 to backdoor");

	reg_val[0] = 0x00;
	r = gt9896s_reg_write(ts_dev, HW_REG_CPU_CTRL,
				     reg_val, 1);
	if (r < 0) {
		ts_err("Failed to run isp");
		return r;
	}
	ts_debug("Success run isp, set 0x%x-->0x00", HW_REG_CPU_CTRL);

	/* check isp work state */
	for (i = 0; i < TS_CHECK_ISP_STATE_RETRY_TIMES; i++) {
		msleep(10);
		r = gt9896s_reg_read(ts_dev, HW_REG_ISP_RUN_FLAG,
				    reg_val, 2);
		if (r < 0 || (reg_val[0] == 0xAA && reg_val[1] == 0xBB))
			break;
	}
	if (reg_val[0] == 0xAA && reg_val[1] == 0xBB) {
		ts_info("ISP working OK");
		return 0;
	}
	ts_err("ISP not work,0x%x=0x%x, 0x%x=0x%x",
		HW_REG_ISP_RUN_FLAG, reg_val[0],
		HW_REG_ISP_RUN_FLAG + 1, reg_val[1]);
	return -EFAULT;
}

int gt9896s_reset_ic_init(struct gt9896s_ts_device *ts_dev);
/**
 * gt9896s_update_prepare - update prepare, loading ISP program
 *  and make sure the ISP is running.
 * @fwu_ctrl: pointer to fimrware control structure
 * return: 0 ok, <0 error
 */
static int gt9896s_update_prepare(struct fw_update_ctrl *fwu_ctrl)
{
	struct gt9896s_ts_device *ts_dev = fwu_ctrl->ts_dev;
	u8 reg_val[4] = { 0x00 };
	u8 temp_buf[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	int retry = 20;
	int r;

	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE ||
		ts_dev->ic_type == IC_TYPE_YELLOWSTONE_SPI)
		ts_dev->hw_ops->write(ts_dev, HW_REG_CPU_RUN_FROM_YS,
				      temp_buf, 8);
	else
		ts_dev->hw_ops->write(ts_dev, HW_REG_CPU_RUN_FROM,
				      temp_buf, 8);

	/*reset IC*/
	fwu_ctrl->allow_reset = true;
	ts_info("firmware update, reset");
	gpio_direction_output(ts_dev->board_data.reset_gpio, 0);
	udelay(2000);
	gpio_direction_output(ts_dev->board_data.reset_gpio, 1);
	usleep_range(10000, 11000);
	fwu_ctrl->allow_reset = false;

	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE_SPI) {
		r = gt9896s_reset_ic_init(ts_dev);
		if (r < 0) {
			ts_err("Failed to reset ic init!");
			return -EINVAL;
		}
	} else if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE) {
		retry = 100;
		do {
			reg_val[0] = 0x00;
			r = gt9896s_reg_write_confirm(ts_dev, HW_REG_GIO_YS,
						     reg_val, 1);
			if (r < 0)
				ts_info("Failed to remove GIO hold flag, retry %d", retry);
			else
				break;
		} while (--retry);
		if (!retry) {
			ts_err("Failed to remove GIO flag");
			return -EINVAL;
		}
	}

	retry = 20;
	for (retry = 0; retry < 20; retry++) {
		if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE ||
			ts_dev->ic_type == IC_TYPE_YELLOWSTONE_SPI) {
			reg_val[0] = 0x00;
			r = gt9896s_reg_write(ts_dev, HW_REG_ESD_KEY_DIS,
					reg_val, 1);
			if (r < 0) {
				ts_info("failed dis esd key");
				continue;
			}
			reg_val[0] = 0x95;
			r = gt9896s_reg_write(ts_dev, HW_REG_ESD_KEY_EN,
				reg_val, 1);
			if (r < 0) {
				ts_info("failed open esd key");
				continue;
			}
		}
		reg_val[0] = 0x24;
		r = gt9896s_reg_write_confirm(ts_dev, HW_REG_CPU_CTRL,
					     reg_val, 1);
		if (r < 0) {
			ts_info("Failed to hold ss51, retry");
			msleep(20);
		} else {
			break;
		}
	}
	if (retry >= 20) {
		ts_err("Failed hold ss51,return =%d", r);
		return -EINVAL;
	}
	ts_debug("Success hold ss51");

	/* enable DSP & MCU power */
	reg_val[0] = 0x00;
	r = gt9896s_reg_write_confirm(ts_dev, HW_REG_DSP_MCU_POWER, reg_val, 1);
	if (r < 0) {
		ts_err("Failed enable DSP&MCU power");
		return r;
	}
	ts_debug("Success enabled DSP&MCU power,set 0x%x-->0x00",
		 HW_REG_DSP_MCU_POWER);

	/* disable watchdog timer */
	reg_val[0] = 0x00;
	r = gt9896s_reg_write(ts_dev, HW_REG_CACHE, reg_val, 1);
	if (r < 0) {
		ts_err("Failed to clear cache");
		return r;
	}
	ts_debug("Success clear cache");

	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE ||
		ts_dev->ic_type == IC_TYPE_YELLOWSTONE_SPI) {
		reg_val[0] = 0x00;
		r = gt9896s_reg_write(ts_dev, HW_REG_WTD_TIMER, reg_val, 1);
	} else {
		reg_val[0] = 0x95;
		r = gt9896s_reg_write(ts_dev, HW_REG_ESD_KEY, reg_val, 1);
		reg_val[0] = 0x00;
		r |= gt9896s_reg_write(ts_dev, HW_REG_WTD_TIMER, reg_val, 1);

		reg_val[0] = 0x27;
		r |= gt9896s_reg_write(ts_dev, HW_REG_ESD_KEY, reg_val, 1);
	}
	if (r < 0) {
		ts_err("Failed to disable watchdog");
		return r;
	}
	ts_info("Success disable watchdog");

	/* set scramble */
	reg_val[0] = 0x00;
	r = gt9896s_reg_write(ts_dev, HW_REG_SCRAMBLE, reg_val, 1);
	if (r < 0) {
		ts_err("Failed to set scramble");
		return r;
	}
	ts_debug("Succcess set scramble");

	/* load ISP code and run form isp */
	r = gt9896s_load_isp(ts_dev, &fwu_ctrl->fw_data);
	if (r < 0)
		ts_err("Failed lode and run isp");

	return r;
}

/**
 * gt9896s_format_fw_packet - formate one flash packet
 * @pkt: target firmware packet
 * @flash_addr: flash address
 * @size: packet size
 * @data: packet data
 */
static int gt9896s_format_fw_packet(u8 *pkt, u32 flash_addr,
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
	/* u16 >> 16bit seems nosense but really important */
	pkt[2] = (flash_addr >> 16) & 0xff;
	pkt[3] = (flash_addr >> 8) & 0xff;
	memcpy(&pkt[4], data, len);
	checksum = checksum_be16(pkt, len + 4);
	checksum = 0 - checksum;
	pkt[len + 4] = (checksum >> 8) & 0xff;
	pkt[len + 5] = checksum & 0xff;
	return 0;
}

/**
 * gt9896s_send_fw_packet - send one firmware packet to ISP
 * @dev: target touch device
 * @pkt: firmware packet
 * return：0 ok, <0 error
 */
static int gt9896s_send_fw_packet(struct gt9896s_ts_device *dev, u8 type,
				 u8 *pkt, u32 len)
{
	u8 reg_val[4];
	int r, i;

	if (!pkt)
		return -EINVAL;

	ts_info("target fw subsys type:0x%x, len %d", type, len);
	r = gt9896s_reg_write_confirm(dev, HW_REG_ISP_BUFFER, pkt, len);
	if (r < 0) {
		ts_err("Failed to write firmware packet");
		return r;
	}

	reg_val[0] = 0;
	reg_val[1] = 0;
	/* clear flash flag 0X6022 */
	r = gt9896s_reg_write_confirm(dev, HW_REG_FLASH_FLAG, reg_val, 2);
	if (r < 0) {
		ts_err("Faile to clear flash flag");
		return r;
	}

	/* write subsystem type 0X8020*/
	reg_val[0] = type;
	reg_val[1] = type;
	r = gt9896s_reg_write_confirm(dev, HW_REG_SUBSYS_TYPE, reg_val, 2);
	if (r < 0) {
		ts_err("Failed write subsystem type to IC");
		return r;
	}

	for (i = 0; i < TS_READ_FLASH_STATE_RETRY_TIMES; i++) {
		r = gt9896s_reg_read(dev, HW_REG_FLASH_FLAG, reg_val, 2);
		if (r < 0) {
			ts_err("Failed read flash state");
			return r;
		}

		/* flash haven't end */
		if (reg_val[0] == ISP_STAT_WRITING &&
		    reg_val[1] == ISP_STAT_WRITING) {
			ts_debug("Flash not ending...");
			usleep_range(55000, 56000);
			continue;
		}
		if (reg_val[0] == ISP_FLASH_SUCCESS &&
		    reg_val[1] == ISP_FLASH_SUCCESS) {
			r = gt9896s_reg_read(dev, HW_REG_FLASH_FLAG, reg_val, 2);
			if (!r && reg_val[0] == ISP_FLASH_SUCCESS &&
			    reg_val[1] == ISP_FLASH_SUCCESS) {
				ts_info("Flash subsystem ok");
				return 0;
			}
		}
		if (reg_val[0] == ISP_FLASH_ERROR &&
			reg_val[1] == ISP_FLASH_ERROR) {
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
 * gt9896s_flash_subsystem - flash subsystem firmware,
 *  Main flow of flashing firmware.
 *	Each firmware subsystem is divided into several
 *	packets, the max size of packet is limited to
 *	@{ISP_MAX_BUFFERSIZE}
 * @dev: pointer to touch device
 * @subsys: subsystem infomation
 * return: 0 ok, < 0 error
 */
static int gt9896s_flash_subsystem(struct gt9896s_ts_device *dev,
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
		r = gt9896s_format_fw_packet(fw_packet, subsys_base_addr + offset,
				data_size, &subsys->data[offset]);
		if (r < 0) {
			ts_err("Invalid packet params");
			goto exit;
		}

		/* send one firmware packet, retry 3 time if send failed */
		for (i = 0; i < 3; i++) {
			r = gt9896s_send_fw_packet(dev, subsys->type,
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
 * gt9896s_flash_firmware - flash firmware
 * @dev: pointer to touch device
 * @fw_data: firmware data
 * return: 0 ok, < 0 error
 */
static int gt9896s_flash_firmware(struct gt9896s_ts_device *dev,
				struct firmware_data *fw_data)
{
	struct fw_update_ctrl *fw_ctrl;
	struct firmware_info  *fw_info;
	struct fw_subsys_info *fw_x;
	int retry = GOODIX_BUS_RETRY_TIMES;
	int i, r = 0, fw_num, prog_step;
	u8 *fw_packet = NULL;
	u8 *flash_cfg = NULL;

	/* start from subsystem 1,
	 * subsystem 0 is the ISP program
	 */
	fw_ctrl = container_of(fw_data, struct fw_update_ctrl, fw_data);
	fw_info = &fw_data->fw_info;
	fw_num = fw_info->subsys_num;

	/* we have 80% work here */
	prog_step = 80 / (fw_num - 1);

	for (i = 1; i < fw_num && retry;) {
		ts_info("--- Start to flash subsystem[%d] ---", i);
		fw_x = &fw_info->subsys[i];
		r = gt9896s_flash_subsystem(dev, fw_x);
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
			break;
		}
	}

	kfree(fw_packet);
	fw_packet = NULL;
	kfree(flash_cfg);
	flash_cfg = NULL;
	return r;
}

/**
 * gt9896s_update_finish - update finished, free resource
 *  and reset flags---
 * @fwu_ctrl: pointer to fw_update_ctrl structrue
 * return: 0 ok, < 0 error
 */
static int gt9896s_update_finish(struct gt9896s_ts_device *ts_dev,
				struct fw_update_ctrl *fwu_ctrl)
{
	u8 reg_val[8] = {0};
	int r = 0, i = 0;

	/* hold ss51 */
	reg_val[0] = 0x24;
	r = gt9896s_reg_write(ts_dev, HW_REG_CPU_CTRL,
				     reg_val, 1);
	if (r < 0)
		ts_err("Failed to hold ss51");

	/* clear back door flag */
	memset(reg_val, 0, sizeof(reg_val));
	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE ||
		ts_dev->ic_type == IC_TYPE_YELLOWSTONE_SPI)
		r = gt9896s_reg_write(ts_dev, HW_REG_CPU_RUN_FROM_YS,
				     reg_val, 8);
	else
		r = gt9896s_reg_write(ts_dev, HW_REG_CPU_RUN_FROM,
				     reg_val, 8);
	if (r) {
		ts_err("Failed set CPU run from normal firmware");
		return r;
	}

	/* release ss51 */
	reg_val[0] = 0x00;
	r = gt9896s_reg_write(ts_dev, HW_REG_CPU_CTRL, reg_val, 1);
	if (r < 0)
		ts_err("Failed to run ss51");

	/*reset*/
	gpio_direction_output(ts_dev->board_data.reset_gpio, 0);
	udelay(2000);
	gpio_direction_output(ts_dev->board_data.reset_gpio, 1);
	msleep(80);

	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE) {
		for (i = 0; i < 100; i++) {
			reg_val[0] = 0x00;
			r = gt9896s_reg_write_confirm(ts_dev, HW_REG_GIO_YS,
							reg_val, 1);
			if (!r)
				break;
			ts_info("failed set GIO flag, r %d", r);
		}
		if (i >= 100)
			ts_err("failed set GIO flag, %d", r);
	} else if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE_SPI) {
		r = gt9896s_reset_ic_init(ts_dev);
		if (r < 0)
			ts_err("Failed to reset ic init!");
	}

	return r;
}

static int gt9896s_flash_config(struct gt9896s_ts_device *ts_dev)
{
	int ret;
	u8 *fw_packet = NULL;
	u8 *temp_data = NULL;

	if (!ts_dev || !ts_dev->cfg_bin_state ||
	    !ts_dev->normal_cfg.initialized) {
		ts_err("no valid config data for flash");
		return -EINVAL;
	}

	temp_data = kzalloc(ISP_MAX_BUFFERSIZE, GFP_KERNEL);
	fw_packet = kzalloc(ISP_MAX_BUFFERSIZE + 6, GFP_KERNEL);
	if (!temp_data || !fw_packet) {
		ts_err("Failed alloc memory");
		ret = -EINVAL;
		goto exit;
	}

	memset(temp_data, 0xFF, ISP_MAX_BUFFERSIZE);
	ts_info("normal config length %d", ts_dev->normal_cfg.length);
	memcpy(temp_data, ts_dev->normal_cfg.data, ts_dev->normal_cfg.length);

	/* format one firmware packet */
	ret = gt9896s_format_fw_packet(fw_packet, FLASH_ADDR_CONFIG_DATA,
			ISP_MAX_BUFFERSIZE, temp_data);
	if (ret < 0) {
		ts_err("Invalid packet params");
		goto exit;
	}
	ts_debug("fw_pack:%*ph", 10, fw_packet);
	ts_info("try flash config");
	ret = gt9896s_send_fw_packet(ts_dev, FLASH_SUBSYS_TYPE_CONFIG,
				  fw_packet, ISP_MAX_BUFFERSIZE + 6);
	if (ret)
		ts_err("failed flash config, ret %d", ret);
	else
		ts_info("success flash config with isp");

exit:
	kfree(temp_data);
	kfree(fw_packet);
	return ret;
}

/**
 * gt9896s_fw_update_proc - firmware update process, the entry of
 *  firmware update flow
 * @fwu_ctrl: firmware control
 * return: 0 ok, < 0 error
 */
static int gt9896s_fw_update_proc(struct fw_update_ctrl *fwu_ctrl)
{
#define FW_UPDATE_RETRY		2
	int retry0 = FW_UPDATE_RETRY;
	int retry1 = FW_UPDATE_RETRY;
	int r = 0;

	if (fwu_ctrl->status == UPSTA_PREPARING ||
			fwu_ctrl->status == UPSTA_UPDATING) {
		ts_err("Firmware update already in progress");
		return -EINVAL;
	}

	fwu_ctrl->progress = 0;
	fwu_ctrl->status = UPSTA_PREPARING;

	r = gt9896s_parse_firmware(&fwu_ctrl->fw_data);
	if (r < 0) {
		fwu_ctrl->status = UPSTA_ABORT;
		goto err_parse_fw;
	}

	fwu_ctrl->progress = 10;
	if (!(fwu_ctrl->mode & UPDATE_MODE_FORCE)) {
		r = gt9896s_check_update(fwu_ctrl->ts_dev,
					&fwu_ctrl->fw_data.fw_info);
		if (!r) {
			fwu_ctrl->status = UPSTA_ABORT;
			ts_info("fw update skiped");
			goto err_check_update;
		}
	} else {
		ts_info("force update mode");
	}

start_update:
	fwu_ctrl->progress = 20;
	fwu_ctrl->status = UPSTA_UPDATING; /* show upgrading status */
	r = gt9896s_update_prepare(fwu_ctrl);
	if ((r == -EBUS || r == -EAGAIN) && --retry0 > 0) {
		ts_err("Bus error, retry prepare ISP:%d",
			FW_UPDATE_RETRY - retry0);
		goto start_update;
	} else if (r < 0) {
		ts_err("Failed to prepare ISP, exit update:%d", r);
		fwu_ctrl->status = UPSTA_FAILED;
		goto err_fw_prepare;
	}

	if (GOODIX_FLASH_CONFIG_WITH_ISP &&
	    fwu_ctrl->mode & UPDATE_MODE_FLASH_CFG) {
		ts_info("need flash config with isp");
		gt9896s_flash_config(fwu_ctrl->ts_dev);
	}

	/* progress: 20%~100% */
	r = gt9896s_flash_firmware(fwu_ctrl->ts_dev, &fwu_ctrl->fw_data);
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
	gt9896s_update_finish(fwu_ctrl->ts_dev, fwu_ctrl);
err_check_update:
err_parse_fw:
	if (fwu_ctrl->status == UPSTA_SUCCESS)
		ts_info("Firmware update successfully");
	else if (fwu_ctrl->status == UPSTA_FAILED)
		ts_err("Firmware update failed");

	fwu_ctrl->progress = 100; /* 100% */
	ts_info("fw update ret %d", r);
	return r;
}

static struct gt9896s_ext_module gt9896s_fwu_module;

/**
 * gt9896s_request_firmware - request firmware data from user space
 *
 * @fw_data: firmware struct, contains firmware header info
 *	and firmware data pointer.
 * return: 0 - OK, < 0 - error
 */
static int gt9896s_request_firmware(struct firmware_data *fw_data,
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
static inline void gt9896s_release_firmware(struct firmware_data *fw_data)
{
	if (fw_data->firmware) {
		release_firmware(fw_data->firmware);
		fw_data->firmware = NULL;
	}
}

static int gt9896s_fw_update_thread(void *data)
{
	struct fw_update_ctrl *fwu_ctrl = data;
	struct firmware *temp_firmware = NULL;
	int r = -EINVAL;
	struct fw_update_ctrl *fw_ctrl = &gt9896s_fw_update_ctrl;

	mutex_lock(&fwu_ctrl->mutex);

	if (fwu_ctrl->mode & UPDATE_MODE_SRC_HEAD) {
		ts_info("Firmware header update starts");
		temp_firmware = kzalloc(sizeof(struct firmware), GFP_KERNEL);
		if (!temp_firmware) {
			ts_err("Failed to allocate memory for firmware");
			goto out;
		}
		temp_firmware->size = sizeof(gt9896s_default_fw);
		temp_firmware->data = gt9896s_default_fw;
		fwu_ctrl->fw_data.firmware = temp_firmware;
	} else if (fwu_ctrl->mode & UPDATE_MODE_SRC_REQUEST) {
		ts_info("Firmware request update starts");
		r = gt9896s_request_firmware(&fwu_ctrl->fw_data,
						fwu_ctrl->fw_name);
		if (r < 0) {
			fwu_ctrl->status = UPSTA_ABORT;
			fwu_ctrl->progress = 100;
			goto out;
		}
	} else if (fwu_ctrl->mode & UPDATE_MODE_SRC_SYSFS) {
		if (!fwu_ctrl->fw_data.firmware) {
			ts_err("Invalid firmware from sysfs");
			fwu_ctrl->status = UPSTA_ABORT;
			fwu_ctrl->progress = 100;
			r = -EINVAL;
			goto out;
		}
	} else {
		ts_err("unknown update mode 0x%x", fwu_ctrl->mode);
		r = -EINVAL;
		goto out;
	}

	if (!fw_ctrl->initialized)
		gt9896s_register_ext_module(&gt9896s_fwu_module);

	/* DONT allow reset/irq/suspend/resume during update */
	fwu_ctrl->allow_irq = false;
	fwu_ctrl->allow_suspend = false;
	fwu_ctrl->allow_resume = false;
	fwu_ctrl->allow_reset = false;
	ts_debug("notify update start");
	gt9896s_ts_blocking_notify(NOTIFY_FWUPDATE_START, NULL);

	/* ready to update */
	ts_debug("start update proc");
	r = gt9896s_fw_update_proc(fwu_ctrl);

	fwu_ctrl->allow_reset = true;
	fwu_ctrl->allow_irq = true;
	fwu_ctrl->allow_suspend = true;
	fwu_ctrl->allow_resume = true;

	/* clean */
	if (fwu_ctrl->mode & UPDATE_MODE_SRC_HEAD) {
		kfree(fwu_ctrl->fw_data.firmware);
		fwu_ctrl->fw_data.firmware = NULL;
		temp_firmware = NULL;
	} else if (fwu_ctrl->mode & UPDATE_MODE_SRC_REQUEST) {
		gt9896s_release_firmware(&fwu_ctrl->fw_data);
	} else if (fwu_ctrl->mode & UPDATE_MODE_SRC_SYSFS) {
		vfree(fwu_ctrl->fw_data.firmware);
		fwu_ctrl->fw_data.firmware = NULL;
	}

out:
	fwu_ctrl->mode = UPDATE_MODE_DEFAULT;
	mutex_unlock(&fwu_ctrl->mutex);
	gt9896s_unregister_ext_module(&gt9896s_fwu_module);

	if (r) {
		ts_err("fw update failed, %d", r);
		gt9896s_ts_blocking_notify(NOTIFY_FWUPDATE_FAILED, NULL);
	} else {
		ts_info("fw update success");
		gt9896s_ts_blocking_notify(NOTIFY_FWUPDATE_SUCCESS, NULL);
	}
	return r;
}

/*
 * gt9896s_sysfs_update_en_store: start fw update manually
 * @buf: '1'[001] update in blocking mode with fwdata from sysfs
 *       '2'[010] update in blocking mode with fwdata from request
 *       '5'[101] update in unblocking mode with fwdata from sysfs
 *       '6'[110] update in unblocking mode with fwdata from request
 */
static ssize_t gt9896s_sysfs_update_en_store(
		struct gt9896s_ext_module *module,
		const char *buf, size_t count)
{
	int ret = 0;
	int mode = 0;
	struct fw_update_ctrl *fw_ctrl = module->priv_data;

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
		mode = UPDATE_MODE_FORCE|UPDATE_MODE_BLOCK|UPDATE_MODE_SRC_SYSFS;
	} else if (buf[0] == '2') {
		mode = UPDATE_MODE_FORCE|UPDATE_MODE_BLOCK|UPDATE_MODE_SRC_REQUEST;
	} else if (buf[0] == '5') {
		mode = UPDATE_MODE_FORCE|UPDATE_MODE_SRC_SYSFS;
	} else if (buf[0] == '6') {
		mode = UPDATE_MODE_FORCE|UPDATE_MODE_SRC_REQUEST;
	} else {
		ts_err("invalid update mode:0x%x", buf[0]);
		return -EINVAL;
	}

	ret = gt9896s_do_fw_update(mode);
	if (!ret) {
		ts_info("success start update work");
		return count;
	}
	ts_err("failed start fw update work");
	return -EINVAL;
}

static ssize_t gt9896s_sysfs_update_progress_show(
		struct gt9896s_ext_module *module,
		char *buf)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	return scnprintf(buf, PAGE_SIZE, "%d\n", fw_ctrl->progress);
}

static ssize_t gt9896s_sysfs_update_result_show(
		struct gt9896s_ext_module *module,
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

static ssize_t gt9896s_sysfs_update_fwversion_show(
		struct gt9896s_ext_module *module,
		char *buf)
{
	struct gt9896s_ts_version fw_ver;
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

static ssize_t gt9896s_sysfs_fwsize_show(struct gt9896s_ext_module *module,
		char *buf)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	int r = -EINVAL;

	if (fw_ctrl && fw_ctrl->fw_data.firmware)
		r = snprintf(buf, PAGE_SIZE, "%zu\n",
				fw_ctrl->fw_data.firmware->size);
	return r;
}

static ssize_t gt9896s_sysfs_fwsize_store(struct gt9896s_ext_module *module,
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

	/* use vmalloc to alloc huge memory */
	fw = vmalloc(sizeof(*fw) + size);
	if (fw == NULL) {
		ts_err("Failed to alloc memory,size:%zu",
				size + sizeof(*fw));
		return -ENOMEM;
	}
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

static ssize_t gt9896s_sysfs_fwimage_store(struct file *file,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t pos, size_t count)
{
	struct fw_update_ctrl *fw_ctrl;
	struct firmware_data *fw_data;

	fw_ctrl = container_of(attr, struct fw_update_ctrl,
			attr_fwimage);
	fw_data = &fw_ctrl->fw_data;

	if ((!fw_data->firmware) || (!buf)) {
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

/* this interface has ben deprecated */
static ssize_t gt9896s_sysfs_force_update_store(
		struct gt9896s_ext_module *module,
		const char *buf, size_t count)
{
	return count;
}

static struct gt9896s_ext_attribute gt9896s_fwu_attrs[] = {
	__EXTMOD_ATTR(update_en, 0220, NULL, gt9896s_sysfs_update_en_store),
	__EXTMOD_ATTR(progress, S_IRUGO, gt9896s_sysfs_update_progress_show, NULL),
	__EXTMOD_ATTR(result, S_IRUGO, gt9896s_sysfs_update_result_show, NULL),
	__EXTMOD_ATTR(fwversion, S_IRUGO,
			gt9896s_sysfs_update_fwversion_show, NULL),
	__EXTMOD_ATTR(fwsize, 0660, gt9896s_sysfs_fwsize_show,
			gt9896s_sysfs_fwsize_store),
	__EXTMOD_ATTR(force_update, 0220, NULL,
			gt9896s_sysfs_force_update_store),
};

static int gt9896s_fw_sysfs_init(struct gt9896s_ts_core *core_data,
		struct gt9896s_ext_module *module)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	struct kobj_type *ktype;
	int ret = 0, i;

	ktype = gt9896s_get_default_ktype();
	ret = kobject_init_and_add(&module->kobj,
			ktype,
			&core_data->pdev->dev.kobj,
			"fwupdate");
	if (ret) {
		ts_err("Create fwupdate sysfs node error!");
		goto exit_sysfs_init;
	}


	ret = 0;
	for (i = 0; i < ARRAY_SIZE(gt9896s_fwu_attrs) && !ret; i++)
		ret = sysfs_create_file(&module->kobj, &gt9896s_fwu_attrs[i].attr);
	if (ret) {
		ts_err("failed create fwu sysfs files");
		while (--i >= 0)
			sysfs_remove_file(&module->kobj, &gt9896s_fwu_attrs[i].attr);

		kobject_put(&module->kobj);
		ret = -EINVAL;
		goto exit_sysfs_init;
	}

	fw_ctrl->attr_fwimage.attr.name = "fwimage";
	fw_ctrl->attr_fwimage.attr.mode = 0660;
	fw_ctrl->attr_fwimage.size = 0;
	fw_ctrl->attr_fwimage.write = gt9896s_sysfs_fwimage_store;
	ret = sysfs_create_bin_file(&module->kobj,
			&fw_ctrl->attr_fwimage);

exit_sysfs_init:
	return ret;
}

static void gt9896s_fw_sysfs_remove(struct gt9896s_ext_module *module)
{
	struct fw_update_ctrl *fw_ctrl = module->priv_data;
	int i;

	sysfs_remove_bin_file(&module->kobj, &fw_ctrl->attr_fwimage);

	for (i = 0; i < ARRAY_SIZE(gt9896s_fwu_attrs); i++)
		sysfs_remove_file(&module->kobj,
				&gt9896s_fwu_attrs[i].attr);

	kobject_put(&module->kobj);
}

int gt9896s_do_fw_update(int mode)
{
	struct task_struct *fwu_thrd;
	struct fw_update_ctrl *fwu_ctrl = &gt9896s_fw_update_ctrl;
	int ret;

	if (!fwu_ctrl->initialized) {
		ts_err("fw mode uninit");
		return -EINVAL;
	}

	fwu_ctrl->mode = mode;
	ts_debug("fw update mode 0x%x", mode);
	if (fwu_ctrl->mode & UPDATE_MODE_BLOCK) {
		ret = gt9896s_fw_update_thread(fwu_ctrl);
		ts_info("fw update return %d", ret);
		return ret;
	} else {
		/* create and run update thread */
		fwu_thrd = kthread_run(gt9896s_fw_update_thread,
				fwu_ctrl, "gt9896s-fwu");
		if (IS_ERR_OR_NULL(fwu_thrd)) {
			ts_err("Failed to create update thread:%ld",
					PTR_ERR(fwu_thrd));
			return -EFAULT;
		}
		ts_info("success create fw update thread");
		return 0;
	}
}

static int gt9896s_fw_update_init(struct gt9896s_ts_core *core_data,
				struct gt9896s_ext_module *module)
{
	int ret = 0;
	struct gt9896s_ts_board_data *ts_bdata = board_data(core_data);

	if (gt9896s_fw_update_ctrl.initialized) {
		ts_info("no need reinit");
		return ret;
	}

	if (!core_data || !ts_bdata || !core_data->ts_dev) {
		ts_err("core_data && ts_dev cann't be null");
		return -ENODEV;
	}

	mutex_lock(&gt9896s_fw_update_ctrl.mutex);
	module->priv_data = &gt9896s_fw_update_ctrl;

	gt9896s_fw_update_ctrl.ts_dev = core_data->ts_dev;
	gt9896s_fw_update_ctrl.allow_reset = true;
	gt9896s_fw_update_ctrl.allow_irq = true;
	gt9896s_fw_update_ctrl.allow_suspend = true;
	gt9896s_fw_update_ctrl.allow_resume = true;
	gt9896s_fw_update_ctrl.core_data = core_data;
	gt9896s_fw_update_ctrl.mode = 0;
	/* find a valid firmware image name */
	if (ts_bdata && ts_bdata->fw_name)
		strlcpy(gt9896s_fw_update_ctrl.fw_name, ts_bdata->fw_name,
			sizeof(gt9896s_fw_update_ctrl.fw_name));
	else {
		if (ts_bdata->lcm_max_x == 1080 && ts_bdata->lcm_max_y == 2280) {
			ret = snprintf(gt9896s_fw_update_ctrl.fw_name,
						sizeof(gt9896s_fw_update_ctrl.fw_name),
						"%s%s_1080x2280.bin",
						TS_DEFAULT_FIRMWARE,
						gt9896s_firmware_buf);
		} else if (ts_bdata->lcm_max_x == 1080 && ts_bdata->lcm_max_y == 2300) {
			ret = snprintf(gt9896s_fw_update_ctrl.fw_name,
						sizeof(gt9896s_fw_update_ctrl.fw_name),
						"%s%s_1080x2300.bin",
						TS_DEFAULT_FIRMWARE,
						gt9896s_firmware_buf);
		} else {
			ret = snprintf(gt9896s_fw_update_ctrl.fw_name,
						sizeof(gt9896s_fw_update_ctrl.fw_name),
						"%s%s.bin",
						TS_DEFAULT_FIRMWARE,
						gt9896s_firmware_buf);
		}

		ts_info("firmware_bin_name %s!!!", gt9896s_fw_update_ctrl.fw_name);
		if (ret >= sizeof(gt9896s_fw_update_ctrl.fw_name))
			ts_err("get firmware_bin_name name FAILED!!!");
	}

	ret = gt9896s_fw_sysfs_init(core_data, module);
	if (ret) {
		ts_err("failed create fwupate sysfs node");
		goto err_out;
	}

	gt9896s_fw_update_ctrl.initialized = 1;
err_out:
	mutex_unlock(&gt9896s_fw_update_ctrl.mutex);
	return ret;
}

static int gt9896s_fw_update_exit(struct gt9896s_ts_core *core_data,
				struct gt9896s_ext_module *module)
{
	return 0;
}

static int gt9896s_fw_before_suspend(struct gt9896s_ts_core *core_data,
				struct gt9896s_ext_module *module)
{
	struct fw_update_ctrl *fwu_ctrl = module->priv_data;

	return fwu_ctrl->allow_suspend ?
				EVT_HANDLED : EVT_CANCEL_SUSPEND;
}

static int gt9896s_fw_before_resume(struct gt9896s_ts_core *core_data,
				struct gt9896s_ext_module *module)
{
	struct fw_update_ctrl *fwu_ctrl = module->priv_data;

	return fwu_ctrl->allow_resume ?
				EVT_HANDLED : EVT_CANCEL_RESUME;
}

static int gt9896s_fw_after_resume(struct gt9896s_ts_core *core_data,
				struct gt9896s_ext_module *module)
{
	return 0;
}

static int gt9896s_fw_irq_event(struct gt9896s_ts_core *core_data,
				struct gt9896s_ext_module *module)
{
	struct fw_update_ctrl *fwu_ctrl = module->priv_data;

	return fwu_ctrl->allow_irq ?
				EVT_HANDLED : EVT_CANCEL_IRQEVT;
}

static int gt9896s_fw_before_reset(struct gt9896s_ts_core *core_data,
				struct gt9896s_ext_module *module)
{
	struct fw_update_ctrl *fwu_ctrl = module->priv_data;

	return fwu_ctrl->allow_reset ?
				EVT_HANDLED : EVT_CANCEL_RESET;
}

static const struct gt9896s_ext_module_funcs gt9896s_ext_funcs = {
	.init = gt9896s_fw_update_init,
	.exit = gt9896s_fw_update_exit,
	.before_reset = gt9896s_fw_before_reset,
	.after_reset = NULL,
	.before_suspend = gt9896s_fw_before_suspend,
	.after_suspend = NULL,
	.before_resume = gt9896s_fw_before_resume,
	.after_resume = gt9896s_fw_after_resume,
	.irq_event = gt9896s_fw_irq_event,
};

static struct gt9896s_ext_module gt9896s_fwu_module = {
	.name = "gt9896s-fwu",
	.funcs = &gt9896s_ext_funcs,
	.priority = EXTMOD_PRIO_FWUPDATE,
};

static int __init gt9896s_fwu_module_init(void)
{
	ts_info("gt9896s_fwupdate_module_ini IN");
	mutex_init(&gt9896s_fw_update_ctrl.mutex);
	return gt9896s_register_ext_module(&gt9896s_fwu_module);
}

static void __exit gt9896s_fwu_module_exit(void)
{
	mutex_lock(&gt9896s_fw_update_ctrl.mutex);
	gt9896s_unregister_ext_module(&gt9896s_fwu_module);
	if (gt9896s_fw_update_ctrl.initialized) {
		gt9896s_fw_sysfs_remove(&gt9896s_fwu_module);
		gt9896s_fw_update_ctrl.initialized = 0;
	}
	mutex_lock(&gt9896s_fw_update_ctrl.mutex);
}

late_initcall(gt9896s_fwu_module_init);
module_exit(gt9896s_fwu_module_exit);

MODULE_DESCRIPTION("Goodix FWU Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
