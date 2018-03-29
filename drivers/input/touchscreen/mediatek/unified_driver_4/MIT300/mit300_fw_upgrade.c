/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * mit300_fw_upgrade.c : Firmware update functions for MIT300
 *
 */

#define LGTP_MODULE "[MIT300]"

#include <lgtp_common.h>
#include <lgtp_common_driver.h>
#include <lgtp_platform_api_i2c.h>
#include <lgtp_platform_api_misc.h>
#include <lgtp_device_mit300.h>


#define MIP_BIN_TAIL_MARK		{0x4D, 0x42, 0x54, 0x01}	/* M B T 0x01 */
#define MIP_BIN_TAIL_SIZE		64

/* Firmware update */
#define MIP_FW_MAX_SECT_NUM		4
#define MIP_FW_UPDATE_DEBUG		0	/* 0 (default) or 1 */
#define CHIP_NAME				"MIT300"
#define CHIP_FW_CODE			"T3H0"

/* ISC Info */
#define ISC_PAGE_SIZE			128

/* ISC Command */
#define ISC_CMD_ENTER			{0xFB, 0x4A, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00}
#define ISC_CMD_READ_STATUS		{0xFB, 0x4A, 0x36, 0xC2, 0x00, 0x00, 0x00, 0x00}
#define ISC_CMD_ERASE_PAGE		{0xFB, 0x4A, 0x00, 0x8F, 0x00, 0x00, 0x00, 0x00}
#define ISC_CMD_PROGRAM_PAGE	{0xFB, 0x4A, 0x00, 0x54, 0x00, 0x00, 0x00, 0x00}
#define ISC_CMD_READ_PAGE		{0xFB, 0x4A, 0x00, 0xC2, 0x00, 0x00, 0x00, 0x00}
#define ISC_CMD_EXIT			{0xFB, 0x4A, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00}

/* ISC Status */
#define ISC_STATUS_BUSY			0x96
#define ISC_STATUS_DONE			0xAD

/* Firmware binary tail info */
struct mip_bin_tail {
	u8 tail_mark[4];
	char chip_name[4];
	u32 bin_start_addr;
	u32 bin_length;

	u16 ver_boot;
	u16 ver_core;
	u16 ver_app;
	u16 ver_param;
	u8 boot_start;
	u8 boot_end;
	u8 core_start;
	u8 core_end;
	u8 app_start;
	u8 app_end;
	u8 param_start;
	u8 param_end;

	u8 checksum_type;
	u8 hw_category;
	u16 param_id;
	u32 param_length;
	u32 build_date;
	u32 build_time;

	u32 reserved1;
	u32 reserved2;
	u16 reserved3;
	u16 tail_size;
	u32 crc;
} __packed;


/**
* Read ISC status
*/
static int mip_isc_read_status(struct melfas_ts_data *ts)
{
	struct i2c_client *client = ts->client;
	u8 cmd[8] = ISC_CMD_READ_STATUS;
	u8 result = 0;
	int cnt = 100;
	int ret = 0;

	do {
		if (Mit300_I2C_Read(client, cmd, 8, &result, 1)) {
			TOUCH_ERR("Mit300_I2C_Read failed\n");
			return -1;
		}

		if (result == ISC_STATUS_DONE) {
			ret = 0;
			break;
		} else if (result == ISC_STATUS_BUSY) {
			ret = -1;
			usleep_range(1000, 1100);
		} else {
			TOUCH_ERR("wrong value [0x%02X]\n", result);
			ret = -1;
			usleep_range(1000, 1100);
		}
	} while (--cnt);

	if (!cnt) {
		TOUCH_ERR("count overflow - cnt [%d] status [0x%02X]\n", cnt, result);
		goto ERROR;
	}
	/* TOUCH_LOG("mip_isc_read_status [DONE]\n"); */

	return ret;

ERROR:
	return ret;
}

/**
* Command : Erase Page
*/
static int mip_isc_erase_page(struct melfas_ts_data *ts, int offset)
{
	struct i2c_client *client = ts->client;
	u8 write_buf[8] = ISC_CMD_ERASE_PAGE;

	write_buf[4] = (u8) ((offset >> 24) & 0xFF);
	write_buf[5] = (u8) ((offset >> 16) & 0xFF);
	write_buf[6] = (u8) ((offset >> 8) & 0xFF);
	write_buf[7] = (u8) (offset & 0xFF);

	if (Mit300_I2C_Write(client, write_buf, 8)) {
		TOUCH_ERR("Mit300_I2C_Write failed\n");
		goto ERROR;
	}

	if (mip_isc_read_status(ts) != 0)
		goto ERROR;

	TOUCH_LOG("mip_isc_erase_page [DONE] - Offset [0x%04X]\n", offset);

	return 0;

ERROR:
	return -1;
}

/**
* Command : Program Page
*/
static int mip_isc_program_page(struct melfas_ts_data *ts, int offset, const u8 *data, int length)
{
	u8 write_buf[8 + ISC_PAGE_SIZE] = ISC_CMD_PROGRAM_PAGE;

	/* TOUCH_LOG("mip_isc_program_page [START]\n"); */

	if (length > ISC_PAGE_SIZE) {
		TOUCH_ERR("page length overflow\n");
		goto ERROR;
	}

	write_buf[4] = (u8) ((offset >> 24) & 0xFF);
	write_buf[5] = (u8) ((offset >> 16) & 0xFF);
	write_buf[6] = (u8) ((offset >> 8) & 0xFF);
	write_buf[7] = (u8) (offset & 0xFF);

	memcpy(&write_buf[8], data, length);

	if (Mit300_I2C_Write(ts->client, write_buf, (length + 8))) {
		TOUCH_ERR("Mit300_I2C_Write failed\n");
		goto ERROR;
	}

	if (mip_isc_read_status(ts) != 0)
		goto ERROR;
	/* TOUCH_LOG("mip_isc_program_page [DONE] - Offset[0x%04X] Length[%d]\n", offset, length); */

	return 0;

ERROR:
	return -1;
}

/**
* Command : Enter ISC
*/
static int mip_isc_enter(struct melfas_ts_data *ts)
{
	u8 write_buf[8] = ISC_CMD_ENTER;

	if (Mit300_I2C_Write(ts->client, write_buf, 8)) {
		TOUCH_ERR("Mit300_I2C_Write failed\n");
		goto ERROR;
	}

	if (mip_isc_read_status(ts) != 0)
		goto ERROR;

	TOUCH_LOG("mip_isc_enter [DONE]\n");

	return 0;

ERROR:
	return -1;
}

/**
* Command : Exit ISC
*/
static int mip_isc_exit(struct melfas_ts_data *ts)
{
	u8 write_buf[8] = ISC_CMD_EXIT;

	TOUCH_LOG("mip_isc_exit [START]\n");

	if (Mit300_I2C_Write(ts->client, write_buf, 8)) {
		TOUCH_ERR("Mit300_I2C_Write failed\n");
		goto ERROR;
	}

	TOUCH_LOG("mip_isc_exit [DONE]\n");

	return 0;

ERROR:
	return -1;
}

/**
* Read chip firmware version
*/
int mip_get_fw_version(struct melfas_ts_data *ts, u8 *ver_buf)
{
	u8 rbuf[8];
	u8 wbuf[2];
	int i;

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_VERSION_BOOT;
	if (Mit300_I2C_Read(ts->client, wbuf, 2, rbuf, 8)) {
		goto ERROR;
	};

	for (i = 0; i < MIP_FW_MAX_SECT_NUM; i++) {
		ver_buf[0 + i * 2] = rbuf[1 + i * 2];
		ver_buf[1 + i * 2] = rbuf[0 + i * 2];
	}

	return 0;

ERROR:
	memset(ver_buf, 0xFF, sizeof(ver_buf));

	TOUCH_ERR("mip_get_fw_version\n");
	return 1;
}

/**
* Read chip firmware version for u16
*/
int mip_get_fw_version_u16(struct melfas_ts_data *ts, u16 *ver_buf_u16)
{
	u8 rbuf[8];
	int i;

	if (mip_get_fw_version(ts, rbuf))
		goto ERROR;

	for (i = 0; i < MIP_FW_MAX_SECT_NUM; i++)
		ver_buf_u16[i] = (rbuf[0 + i * 2] << 8) | rbuf[1 + i * 2];

	return 0;

ERROR:
	memset(ver_buf_u16, 0xFFFF, sizeof(ver_buf_u16));

	TOUCH_ERR("mip_get_fw_version_u16 failed!\n");
	return 1;
}

/**
* Flash chip firmware (main function)
*/
int mip_flash_fw(struct melfas_ts_data *ts, const u8 *fw_data, size_t fw_size, bool force,
		 bool section)
{
	struct mip_bin_tail *bin_info;
	int ret = 0;
	int offset = 0;
	int offset_start = 0;
	int bin_size = 0;
	u8 *bin_data;
	u16 tail_size = 0;
	u8 tail_mark[4] = MIP_BIN_TAIL_MARK;
	u16 ver_chip[MIP_FW_MAX_SECT_NUM];
	bool full_download = false;

	TOUCH_FUNC();

	/* Check tail size */
	tail_size = (fw_data[fw_size - 5] << 8) | fw_data[fw_size - 6];
	if (tail_size != MIP_BIN_TAIL_SIZE) {
		TOUCH_ERR("wrong tail size [%d]\n", tail_size);
		ret = fw_err_file_type;
		goto ERROR;
	}
	/* Check bin format */
	if (memcmp(&fw_data[fw_size - tail_size], tail_mark, 4)) {
		TOUCH_ERR("wrong tail mark\n");
		ret = fw_err_file_type;
		goto ERROR;
	}
	/* Read bin info */
	bin_info = (struct mip_bin_tail *)&fw_data[fw_size - tail_size];

	TOUCH_LOG("bin_info : bin_len[%d] hw_cat[0x%2X] date[%4X] time[%4X] tail_size[%d]\n",
		  bin_info->bin_length, bin_info->hw_category, bin_info->build_date,
		  bin_info->build_time, bin_info->tail_size);

#if MIP_FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MIP_DEVICE_NAME " Bin Info : ", DUMP_PREFIX_OFFSET, 16, 1,
		       bin_info, tail_size, false);
#endif

	/* Check chip code */
	if (memcmp(bin_info->chip_name, CHIP_FW_CODE, 4)) {
		TOUCH_ERR("F/W file is not for %s\n", CHIP_NAME);
		ret = fw_err_file_type;
		goto ERROR;
	}
	/* Check F/W version */
	TOUCH_LOG("F/W file version [0x%04X 0x%04X 0x%04X 0x%04X]\n", bin_info->ver_boot,
		  bin_info->ver_core, bin_info->ver_app, bin_info->ver_param);

	if (mip_get_fw_version_u16(ts, ver_chip)) {
		TOUCH_ERR("Unknown chip firmware version\n");
		ret = fw_err_download;
		goto ERROR;
	}
	if (ver_chip[0] != bin_info->ver_boot) {
		TOUCH_LOG("Full download\n");
		full_download = true;
	} else if ((ver_chip[1] != bin_info->ver_core) || (ver_chip[2] != bin_info->ver_app)
		   || (ver_chip[3] != bin_info->ver_param)) {
		TOUCH_LOG("Section download\n");
		full_download = false;
	} else {
		TOUCH_LOG("Chip firmware is already up-to-date\n");
	}
	/* Read bin data */
	bin_size = bin_info->bin_length;
	bin_data = kzalloc(sizeof(u8) * (bin_size), GFP_KERNEL);
	memcpy(bin_data, fw_data, bin_size);

	/* Enter ISC mode */
	TOUCH_LOG("Enter ISC mode\n");
	ret = mip_isc_enter(ts);
	if (ret != 0) {
		TOUCH_ERR("mip_isc_enter\n");
		ret = fw_err_download;
		goto ERROR;
	}

	if (full_download == true)
		offset_start = 0;
	else
		offset_start = bin_info->app_start * 1024;
	/* Erase first page */
	TOUCH_LOG("Erase first page : Offset[0x%04X]\n", offset_start);
	ret = mip_isc_erase_page(ts, offset_start);
	if (ret != 0) {
		TOUCH_ERR("mip_isc_erase_page\n");
		ret = fw_err_download;
		goto ERROR;
	}
	/* Program & Verify */
	TOUCH_LOG("Program & Verify\n");
	offset = bin_size - ISC_PAGE_SIZE;
	while (offset >= offset_start) {
		/* Program */
		if (mip_isc_program_page(ts, offset, &bin_data[offset], ISC_PAGE_SIZE)) {
			TOUCH_ERR("mip_isc_program_page : offset[0x%04X]\n", offset);
			ret = fw_err_download;
			goto ERROR;
		}

		offset -= ISC_PAGE_SIZE;
	}

	/* Exit ISC mode */
	mip_isc_exit(ts);

	/* Reset chip */
	TouchSetGpioReset(0);
	usleep_range(10000, 11000);
	TouchSetGpioReset(1);
	msleep(100);
	TOUCH_LOG("Device was reset\n");

	/* Check chip firmware version */
	if (mip_get_fw_version_u16(ts, ver_chip)) {
		TOUCH_ERR("Unknown chip firmware version\n");
		ret = fw_err_download;
		goto ERROR;
	} else {
		if ((ver_chip[0] == bin_info->ver_boot) && (ver_chip[1] == bin_info->ver_core)
		    && (ver_chip[2] == bin_info->ver_app) && (ver_chip[3] == bin_info->ver_param)) {
			TOUCH_LOG("Version check OK\n");
		} else {
			ret = fw_err_download;
			goto ERROR;
		}
	}

	goto EXIT;

ERROR:
	/* Reset chip */
	TouchSetGpioReset(0);
	usleep_range(10000, 11000);
	TouchSetGpioReset(1);
	msleep(100);
	TOUCH_LOG("Device was reset\n");

	TOUCH_ERR("mip_flash_fw failed!\n");
	return ret;

EXIT:

	return ret;
}

/**
* Get version of F/W bin file
*/
int mip_bin_fw_version(struct melfas_ts_data *ts, const u8 *fw_data, size_t fw_size, u8 *ver_buf)
{
	struct mip_bin_tail *bin_info;
	u16 tail_size = 0;
	u8 tail_mark[4] = MIP_BIN_TAIL_MARK;

	/* Check tail size */
	tail_size = (fw_data[fw_size - 5] << 8) | fw_data[fw_size - 6];
	if (tail_size != MIP_BIN_TAIL_SIZE) {
		TOUCH_ERR("wrong tail size [%d]\n", tail_size);
		goto ERROR;
	}
	/* Check bin format */
	if (memcmp(&fw_data[fw_size - tail_size], tail_mark, 4)) {
		TOUCH_ERR("wrong tail mark\n");
		goto ERROR;
	}
	/* Read bin info */
	bin_info = (struct mip_bin_tail *)&fw_data[fw_size - tail_size];

	/* F/W version */
	ver_buf[0] = (bin_info->ver_app >> 8) & 0xFF;
	ver_buf[1] = (bin_info->ver_app) & 0xFF;

	return 0;

ERROR:
	return 1;
}
