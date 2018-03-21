/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2010-2017, Focaltech Ltd. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

/*****************************************************************************
*
* File Name: focaltech_upgrade_ft8006.c
*
* Author:    fupeipei
*
* Created:    2016-08-15
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "../focaltech_core.h"

#if (FTS_CHIP_TYPE == _FT8006)
#include "../focaltech_flash.h"
#include "focaltech_upgrade_common.h"

/*****************************************************************************
* Static variables
*****************************************************************************/
#if FTS_AUTO_UPGRADE_FOR_LCD_CFG_EN
#define APP_OFFSET                  0x5000
#define APP_FILE_MAX_SIZE           (116 * 1024)
#else
#define APP_OFFSET                  0x0
#define APP_FILE_MAX_SIZE           (96 * 1024)
#endif

#define APP_FILE_MIN_SIZE           (8)
#define APP_FILE_VER_MAPPING        (0x10E + APP_OFFSET)
#define APP_FILE_VENDORID_MAPPING   (0x10C + APP_OFFSET)
#define APP_FILE_CHIPID_MAPPING     (0x11E + APP_OFFSET)
#define CONFIG_START_ADDR           (0xF80)
#define CONFIG_START_ADDR_LEN       (0x80)
#define CONFIG_VENDOR_ID_OFFSET     (0x04)
#define CONFIG_PROJECT_ID_OFFSET    (0x20)
#define CONFIG_VENDOR_ID_ADDR       (CONFIG_START_ADDR+CONFIG_VENDOR_ID_OFFSET)
#define CONFIG_PROJECT_ID_ADDR      (CONFIG_START_ADDR+CONFIG_PROJECT_ID_OFFSET)
#define LCD_CFG_MAX_SIZE            (4 * 1024)
#define LCD_CFG_MIN_SIZE            (8)

#define TP_WHITE_LOCKDOWN	"44353102D1003100"
#define TP_BLACK_LOCKDOWN	"44353202D1003100"

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
static int ft8006m_ctpm_get_i_file(struct i2c_client *client, int fw_valid);
static int fts_ctpm_get_app_i_file_ver(void);
static int fts_ctpm_get_app_bin_file_ver(char *firmware_name);
static int ft8006m_ctpm_fw_upgrade_with_app_i_file(struct i2c_client *client);
static int ft8006m_ctpm_fw_upgrade_with_app_bin_file(struct i2c_client *client, char *firmware_name);
static int ft8006m_ctpm_fw_upgrade_with_lcd_cfg_i_file(struct i2c_client *client);
static int fts_get_host_lic_ver(void);
static int Ft8006m_Read_Lockdown_From_Boot(struct i2c_client *client);


struct fts_upgrade_fun ft8006m_updatefun = {
	.get_i_file = ft8006m_ctpm_get_i_file,
	.get_app_bin_file_ver = fts_ctpm_get_app_bin_file_ver,
	.get_app_i_file_ver = fts_ctpm_get_app_i_file_ver,
	.upgrade_with_app_i_file = ft8006m_ctpm_fw_upgrade_with_app_i_file,
	.upgrade_with_app_bin_file = ft8006m_ctpm_fw_upgrade_with_app_bin_file,
	.get_hlic_ver = fts_get_host_lic_ver,
	.upgrade_with_lcd_cfg_i_file = ft8006m_ctpm_fw_upgrade_with_lcd_cfg_i_file,

	.upgrade_with_lcd_cfg_bin_file = NULL,
};

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
#if (FTS_GET_VENDOR_ID_NUM != 0)
/************************************************************************
* Name: fts_ctpm_get_vendor_id_flash
* Brief:
* Input:
* Output:
* Return:
***********************************************************************/
static int fts_ctpm_get_vendor_id_flash(struct i2c_client *client, u8 *vendor_id)
{
	bool inbootloader = false;
	u8 rw_buf[10];
	int i_ret;

	ft8006m_ctpm_i2c_hid2std(client);

	i_ret = fts_ctpm_start_fw_upgrade(client);
	if (i_ret < 0) {
		 FTS_ERROR("[UPGRADE]: send upgrade cmd to FW error!!");
		 return i_ret;
	}

	/*Enter upgrade mode*/
	ft8006m_ctpm_i2c_hid2std(client);
	msleep(10);

	inbootloader = ft8006m_ctpm_check_run_state(client, FTS_RUN_IN_BOOTLOADER);
	if (!inbootloader) {
		 FTS_ERROR("[UPGRADE]: not run in bootloader, upgrade fail!!");
		 return -EIO;
	}

	/*read vendor id*/
	rw_buf[0] = 0x03;
	rw_buf[1] = 0x00;
	rw_buf[2] = (u8)(CONFIG_VENDOR_ID_ADDR >> 8);
	rw_buf[3] = (u8)(CONFIG_VENDOR_ID_ADDR);
	i_ret = ft8006m_i2c_write(client, rw_buf, 4);
	msleep(10); /* must wait, otherwise read vendor id wrong */
	i_ret = ft8006m_i2c_read(client, NULL, 0, vendor_id, 1);
	if (i_ret < 0) {
		 return -EIO;
	}
	FTS_DEBUG("Vendor ID from Flash:%x", *vendor_id);
	return 0;
}
#endif

/************************************************************************
* Name: fts_ft5x46_get_i_file
* Brief: get .i file
* Input:
* Output:
* Return: 0   - ok
*         <0 - fail
***********************************************************************/
static int ft8006m_ctpm_get_i_file(struct i2c_client *client, int fw_valid)
{
	int ret = 0;
	struct fts_ts_data *data;
	data = i2c_get_clientdata(client);

#if (FTS_GET_VENDOR_ID_NUM != 0)
	u8 vendor_id = 0;

	if (fw_valid) {
		 ret = ft8006m_i2c_read_reg(client, FTS_REG_VENDOR_ID, &vendor_id);
	} else
		 ret = fts_ctpm_get_vendor_id_flash(client, &vendor_id);
	if (ret < 0) {
		 FTS_ERROR("Get upgrade file fail because of Vendor ID wrong");
		 return ret;
	}
	FTS_INFO("[UPGRADE] vendor id in tp=%x", vendor_id);
	FTS_INFO("[UPGRADE] vendor id in driver:%x, FTS_VENDOR_ID:%02x %02x %02x",
			 vendor_id, FTS_VENDOR_1_ID, FTS_VENDOR_2_ID, FTS_VENDOR_3_ID);

	ret = 0;
	switch (vendor_id) {
#if (FTS_GET_VENDOR_ID_NUM >= 1)
	case FTS_VENDOR_1_ID:
		 ft8006m_g_fw_file = FT8006M_CTPM_FW;
		 ft8006m_g_fw_len = ft8006m_getsize(FW_SIZE);
		 FTS_DEBUG("[UPGRADE]FW FILE:FT8006M_CTPM_FW, SIZE:%x", ft8006m_g_fw_len);
		 break;
#endif
#if (FTS_GET_VENDOR_ID_NUM >= 2)
	case FTS_VENDOR_2_ID:
		 ft8006m_g_fw_file = FT8006M_FT8006M_CTPM_FW2;
		 ft8006m_g_fw_len = ft8006m_getsize(FW2_SIZE);
		 FTS_DEBUG("[UPGRADE]FW FILE:FT8006M_FT8006M_CTPM_FW2, SIZE:%x", ft8006m_g_fw_len);
		 break;
#endif
#if (FTS_GET_VENDOR_ID_NUM >= 3)
	case FTS_VENDOR_3_ID:
		 ft8006m_g_fw_file = FT8006M_FT8006M_CTPM_FW3;
		 ft8006m_g_fw_len = ft8006m_getsize(FW3_SIZE);
		 FTS_DEBUG("[UPGRADE]FW FILE:FT8006M_FT8006M_CTPM_FW3, SIZE:%x", ft8006m_g_fw_len);
		 break;
#endif
	default:
		 FTS_ERROR("[UPGRADE]Vendor ID check fail, get fw file fail");
		 ret = -EIO;
		 break;
	}
#else
	/* (FTS_GET_VENDOR_ID_NUM == 0) */
	if (!strncmp(data->lockdown_info, TP_WHITE_LOCKDOWN, 16)) {
		FTS_DEBUG(" TP color is WHITE\n");
		ret = 1;
	} else if (!strncmp(data->lockdown_info, TP_BLACK_LOCKDOWN, 16)) {
		FTS_DEBUG("TP color is BLACK\n");
		ret = 2;
	} else{
		ret = Ft8006m_Read_Lockdown_From_Boot(client);
	}
	FTS_DEBUG("ret = %d\n", ret);

	if (ret == 1) {
	   ft8006m_g_fw_file = FT8006M_CTPM_FW_WHITE;
	} else if (ret == 2) {
	   ft8006m_g_fw_file = FT8006M_CTPM_FW_BLACK;
	} else{
	  ft8006m_g_fw_file = NULL;
	   FTS_DEBUG("[UPGRADE] request tp FW fail!!!\n");
	}
	ft8006m_g_fw_len = ft8006m_getsize(FW_SIZE);
	FTS_DEBUG("[UPGRADE]FW FILE:FT8006M_CTPM_FW, SIZE:%x", ft8006m_g_fw_len);
#endif

	return ret?0:1;
}

/************************************************************************
* Name: fts_ctpm_get_app_bin_file_ver
* Brief:  get .i file version
* Input: no
* Output: no
* Return: fw version
***********************************************************************/
static int fts_ctpm_get_app_bin_file_ver(char *firmware_name)
{
	u8 *pbt_buf = NULL;
	int fwsize = 0;
	int fw_ver = 0;

	FTS_FUNC_ENTER();

	fwsize = ft8006m_GetFirmwareSize(firmware_name);
	if (fwsize < APP_FILE_MIN_SIZE || fwsize > APP_FILE_MAX_SIZE) {
		 FTS_ERROR("[UPGRADE]: FW length(%x) error", fwsize);
		 return -EIO;
	}

	pbt_buf = (unsigned char *)kmalloc(fwsize + 1, GFP_KERNEL);
	if (ft8006m_ReadFirmware(firmware_name, pbt_buf)) {
		 FTS_ERROR("[UPGRADE]: request_firmware failed!!");
		 kfree(pbt_buf);
		 return -EIO;
	}

	fw_ver = pbt_buf[APP_FILE_VER_MAPPING];

	kfree(pbt_buf);
	FTS_FUNC_EXIT();

	return fw_ver;
}

/************************************************************************
* Name: fts_ctpm_get_app_i_file_ver
* Brief:  get .i file version
* Input: no
* Output: no
* Return: fw version
***********************************************************************/
static int fts_ctpm_get_app_i_file_ver(void)
{
	int fwsize = ft8006m_g_fw_len;

	if (fwsize < APP_FILE_MIN_SIZE || fwsize > APP_FILE_MAX_SIZE) {
		 FTS_ERROR("[UPGRADE]: FW length(%x) error", fwsize);
		 return 0;
	}

	return ft8006m_g_fw_file[APP_FILE_VER_MAPPING];
}

/* read host lcd init code ver
* return 0 if host lcd init code is valid, otherwise return error code
*/
static int fts_get_host_lic_ver(void)
{
	u8 *hlic_buf = ft8006m_g_fw_file;
	u32 hlic_len = 0;
	u8 hlic_ver[2] = { 0 };
	u32 upgfile_len = ft8006m_g_fw_len;

	if (upgfile_len < 4096) {
		 FTS_ERROR("upgrade file len fail");
		 return -EINVAL;
	}
	hlic_len = (u32)(((u32)hlic_buf[2]) << 8) + hlic_buf[3];
	FTS_DEBUG("host lcd init code len:%x", hlic_len);
	if (hlic_len >= upgfile_len) {
		 FTS_ERROR("host lcd init code len is too large");
		 return -EINVAL;
	}

	hlic_ver[0] = hlic_buf[hlic_len];
	hlic_ver[1] = hlic_buf[hlic_len + 1];

	FTS_DEBUG("host lcd init code ver:%x %x", hlic_ver[0], hlic_ver[1]);
	if (0xFF != (hlic_ver[0] + hlic_ver[1])) {
		 FTS_ERROR("host lcd init code version check fail");
		 return -EINVAL;
	}

	return hlic_ver[0];
}


#define MAX_BANK_DATA       0x80
#define MAX_GAMMA_LEN       0x180
int gamma_analog[] = { 0x003A, 0x85, 0x00, 0x00, 0x2C, 0x2B };
int gamma_digital1[] = { 0x0355, 0x8D, 0x00, 0x00, 0x80, 0x80 };
int gamma_digital2[] = { 0x03d9, 0x8D, 0x80, 0x00, 0x14, 0x13 };
int gamma_enable[] = { 0x040d, 0x91, 0x80, 0x00, 0x19, 0x01 };
union short_bits{
	u16 dshort;
	struct bits{
		 u16 bit0:1;
		 u16 bit1:1;
		 u16 bit2:1;
		 u16 bit3:1;
		 u16 bit4:1;
		 u16 bit5:1;
		 u16 bit6:1;
		 u16 bit7:1;
		 u16 bit8:1;
		 u16 bit9:1;
		 u16 bit10:1;
		 u16 bit11:1;
		 u16 bit12:1;
		 u16 bit13:1;
		 u16 bit14:1;
		 u16 bit15:1;
	} bits;
};

/* calculate lcd init code ecc */
static int cal_lcdinitcode_ecc(u8 *buf, u16 *ecc_val)
{
	u32 bank_crc_en = 0;
	u8 bank_data[MAX_BANK_DATA] = { 0 };
	u16 bank_len = 0;
	u16 bank_addr = 0;
	u32 bank_num = 0;
	u16 file_len = 0;
	u16 pos = 0;
	int i = 0;
	union short_bits ecc;
	union short_bits ecc_last;
	union short_bits temp_byte;
	u8 bank_mapping[] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9,
		 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x18,
		 0x19, 0x1A, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x22, 0x23, 0x24};
	u8 banknum_8006 = 0;

	ecc.dshort = 0;
	ecc_last.dshort = 0;
	temp_byte.dshort = 0;

	file_len = (u16)(((u16)buf[2] << 8) + buf[3]);
	bank_crc_en = (u32)(((u32)buf[9] << 24) + ((u32)buf[8] << 16) +\
		 ((u32)buf[7] << 8) + (u32)buf[6]);
	FTS_INFO("lcd init code len=%x bank en=%x", file_len, bank_crc_en);

	pos = 0x0A;
	while (pos < file_len) {
		 bank_addr = (u16)(((u16)buf[pos + 0] << 8) + buf[pos + 1]);
		 bank_len = (u16)(((u16)buf[pos + 2] << 8) + buf[pos + 3]);
		 FTS_INFO("bank pos=%x bank_addr=%x bank_len=%x", pos, bank_addr, bank_len);
		 if (bank_len > MAX_BANK_DATA)
			return -EINVAL;
		 memset(bank_data, 0, MAX_BANK_DATA);
		 memcpy(bank_data, buf + pos + 4, bank_len);

		 bank_num = (bank_addr - 0x8000)/MAX_BANK_DATA;
		 FTS_INFO("actual mipi bank number = %x", bank_num);
		 for (i = 0; i < sizeof(bank_mapping)/sizeof(u8); i++) {
			if (bank_num == bank_mapping[i]) {
				banknum_8006 = i;
				break;
			}
		 }
		 if (i >= sizeof(bank_mapping)/sizeof(u8)) {
			FTS_INFO("actual mipi bank(%d) not find in bank mapping, need jump", bank_num);
		 } else{
			FTS_INFO("8006 bank number = %d", banknum_8006);
			if ((bank_crc_en >> banknum_8006) & 0x01) {
				for (i = 0; i < MAX_BANK_DATA; i++) {
					temp_byte.dshort = (u16)bank_data[i];
					if (i == 0)
						FTS_INFO("data0=%x, %d %d %d %d %d %d %d %d", temp_byte.dshort, temp_byte.bits.bit0,
							temp_byte.bits.bit1, temp_byte.bits.bit2, temp_byte.bits.bit3, temp_byte.bits.bit4,
							temp_byte.bits.bit5, temp_byte.bits.bit6, temp_byte.bits.bit7);

			ecc.bits.bit0 = ecc_last.bits.bit8 ^ ecc_last.bits.bit9 ^ ecc_last.bits.bit10 ^ ecc_last.bits.bit11
				^ ecc_last.bits.bit12 ^ ecc_last.bits.bit13 ^ ecc_last.bits.bit14 ^ ecc_last.bits.bit15
				^ temp_byte.bits.bit0 ^ temp_byte.bits.bit1 ^ temp_byte.bits.bit2 ^ temp_byte.bits.bit3
				^ temp_byte.bits.bit4 ^ temp_byte.bits.bit5 ^ temp_byte.bits.bit6 ^ temp_byte.bits.bit7;

			ecc.bits.bit1 = ecc_last.bits.bit9 ^ ecc_last.bits.bit10 ^ ecc_last.bits.bit11 ^ ecc_last.bits.bit12
				^ ecc_last.bits.bit13 ^ ecc_last.bits.bit14 ^ ecc_last.bits.bit15
				^ temp_byte.bits.bit1 ^ temp_byte.bits.bit2 ^ temp_byte.bits.bit3 ^ temp_byte.bits.bit4
				^ temp_byte.bits.bit5 ^ temp_byte.bits.bit6 ^ temp_byte.bits.bit7;

			ecc.bits.bit2 = ecc_last.bits.bit8 ^ ecc_last.bits.bit9 ^ temp_byte.bits.bit0 ^ temp_byte.bits.bit1;

			ecc.bits.bit3 = ecc_last.bits.bit9 ^ ecc_last.bits.bit10 ^ temp_byte.bits.bit1 ^ temp_byte.bits.bit2;

			ecc.bits.bit4 = ecc_last.bits.bit10 ^ ecc_last.bits.bit11 ^ temp_byte.bits.bit2 ^ temp_byte.bits.bit3;

			ecc.bits.bit5 = ecc_last.bits.bit11 ^ ecc_last.bits.bit12 ^ temp_byte.bits.bit3 ^ temp_byte.bits.bit4;

			ecc.bits.bit6 = ecc_last.bits.bit12 ^ ecc_last.bits.bit13 ^ temp_byte.bits.bit4 ^ temp_byte.bits.bit5;

			ecc.bits.bit7 = ecc_last.bits.bit13 ^ ecc_last.bits.bit14 ^ temp_byte.bits.bit5 ^ temp_byte.bits.bit6;

			ecc.bits.bit8 = ecc_last.bits.bit0 ^ ecc_last.bits.bit14 ^ ecc_last.bits.bit15 ^ temp_byte.bits.bit6 ^ temp_byte.bits.bit7;

			ecc.bits.bit9 = ecc_last.bits.bit1 ^ ecc_last.bits.bit15 ^ temp_byte.bits.bit7;

			ecc.bits.bit10 = ecc_last.bits.bit2;

			ecc.bits.bit11 = ecc_last.bits.bit3;

			ecc.bits.bit12 = ecc_last.bits.bit4;

			ecc.bits.bit13 = ecc_last.bits.bit5;

			ecc.bits.bit14 = ecc_last.bits.bit6;

			ecc.bits.bit15 = ecc_last.bits.bit7 ^ ecc_last.bits.bit8 ^ ecc_last.bits.bit9 ^ ecc_last.bits.bit10
				^ ecc_last.bits.bit11 ^ ecc_last.bits.bit12 ^ ecc_last.bits.bit13 ^ ecc_last.bits.bit14 ^ ecc_last.bits.bit15
				^ temp_byte.bits.bit0 ^ temp_byte.bits.bit1 ^ temp_byte.bits.bit2 ^ temp_byte.bits.bit3
				^ temp_byte.bits.bit4 ^ temp_byte.bits.bit5 ^ temp_byte.bits.bit6 ^ temp_byte.bits.bit7;

			ecc_last.dshort = ecc.dshort;
				}
			}
		 }
		 pos += bank_len + 4;
	}

	*ecc_val = ecc.dshort;
	FTS_INFO("");
	return 0;
}

/* calculate lcd init code checksum */
static unsigned short cal_lcdinitcode_checksum(u8 *ptr , int length)
{

	u16 cFcs = 0;
	int i, j;

	if (length%2) {
		return 0xFFFF;
	}

	for (i = 0; i < length; i += 2) {
		cFcs ^= ((ptr[i] << 8) + ptr[i+1]);
		for (j = 0; j < 16; j++) {
			if (cFcs & 1) {
				cFcs = (unsigned short)((cFcs >> 1) ^ ((1 << 15) + (1 << 10) + (1 << 3)));
			} else {
				cFcs >>= 1;
			}
		}
	}
	return cFcs;
}

static int print_data(u8 *buf, u32 len)
{
	int i = 0;
	int n = 0;
	u8 *p = NULL;

	p = kmalloc(len*4, GFP_KERNEL);
	memset(p, 0, len*4);

	for (i = 0; i < len; i++) {
		 n += sprintf(p + n, "%02x ", buf[i]);
	}

	FTS_DEBUG("%s", p);

	kfree(p);
	return 0;
}

static int read_3gamma(struct i2c_client *client, u8 **gamma, u16 *len)
{
	int ret = 0;
	int i = 0;
	int packet_num = 0;
	int packet_len = 0;
	int remainder = 0;
	u8 cmd[4] = { 0 };
	u32 addr = 0x01D000;
	u8 gamma_header[0x20] = { 0 };
	u16 gamma_len = 0;
	u16 gamma_len_n = 0;
	u16 pos = 0;
	bool gamma_has_enable = false;
	u8 *pgamma = NULL;
	int j = 0;
	u8 gamma_ecc = 0;

	cmd[0] = 0x03;
	cmd[1] = (u8)(addr >> 16);
	cmd[2] = (u8)(addr >> 8);
	cmd[3] = (u8)addr;
	ret = ft8006m_i2c_write(client, cmd, 4);
	msleep(10);
	ret = ft8006m_i2c_read(client, NULL, 0, gamma_header, 0x20);
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
		 FTS_INFO("gamma ecc check fail:%x %x", gamma_header[4], gamma_header[5]);
		 return -EIO;
	}

	if (gamma_len > MAX_GAMMA_LEN) {
		 FTS_ERROR("gamma data len(%d) is too long", gamma_len);
		 return -EINVAL;
	}

	*gamma = kmalloc(MAX_GAMMA_LEN, GFP_KERNEL);
	if (NULL == *gamma) {
		 FTS_ERROR("malloc gamma memory fail");
		 return -ENOMEM;
	}
	pgamma = *gamma;

	packet_num = gamma_len/256;
	packet_len = 256;
	remainder = gamma_len%256;
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
		 ret = ft8006m_i2c_write(client, cmd, 4);
		 msleep(10);
	ret = ft8006m_i2c_read(client, NULL, 0, pgamma + i*256, packet_len);
		if (ret < 0) {
		FTS_ERROR("read 3-gamma data fail");
		return ret;
		 }
	}



	for (j = 0; j < gamma_len; j++) {
	gamma_ecc ^= pgamma[j];
	}
	FTS_INFO("back_3gamma_ecc: 0x%x, 0x%x", gamma_ecc, gamma_header[0x04]);
	if (gamma_ecc != gamma_header[0x04]) {
	FTS_ERROR("back gamma ecc check fail:%x %x", gamma_ecc, gamma_header[0x04]);
	return -EIO;
	}


	/* check last byte is 91 80 00 19 01 */
	pos = gamma_len - 5;
	if ((gamma_enable[1] == pgamma[pos]) && (gamma_enable[2] == pgamma[pos+1])
		 && (gamma_enable[3] == pgamma[pos+2]) && (gamma_enable[4] == pgamma[pos+3])) {
		 gamma_has_enable = true;
	}

	if (false == gamma_has_enable) {
		 FTS_INFO("3-gamma has no gamma enable info");
		 pgamma[gamma_len++] = gamma_enable[1];
		 pgamma[gamma_len++] = gamma_enable[2];
		 pgamma[gamma_len++] = gamma_enable[3];
		 pgamma[gamma_len++] = gamma_enable[4];
		 pgamma[gamma_len++] = gamma_enable[5];
	}

	*len = gamma_len;

	FTS_DEBUG("read 3-gamma data:");
	print_data(*gamma, gamma_len);

	return 0;
}

static int replace_3gamma(u8 *initcode, u8 *gamma, u16 gamma_len)
{
	u16 gamma_pos = 0;

	/* Analog Gamma */
	if ((initcode[gamma_analog[0]] == gamma[gamma_pos])
		 && (initcode[gamma_analog[0] + 1] == gamma[gamma_pos + 1])) {
		 memcpy(initcode + gamma_analog[0] + 4 , gamma + gamma_pos + 4, gamma_analog[5]);
		 gamma_pos += gamma_analog[5] + 4;
	} else
		 goto find_gamma_bank_err;

	/* Digital1 Gamma */
	if ((initcode[gamma_digital1[0]] == gamma[gamma_pos])
		 && (initcode[gamma_digital1[0] + 1] == gamma[gamma_pos + 1])) {
		 memcpy(initcode + gamma_digital1[0] + 4 , gamma + gamma_pos + 4, gamma_digital1[5]);
		 gamma_pos += gamma_digital1[5] + 4;
	} else
		 goto find_gamma_bank_err;

	/* Digital2 Gamma */
	if ((initcode[gamma_digital2[0]] == gamma[gamma_pos])
		 && (initcode[gamma_digital2[0] + 1] == gamma[gamma_pos + 1])) {
		 memcpy(initcode + gamma_digital2[0] + 4 , gamma + gamma_pos + 4, gamma_digital2[5]);
		 gamma_pos += gamma_digital2[5] + 4;
	} else
		 goto find_gamma_bank_err;

	/* enable Gamma */
	if ((initcode[gamma_enable[0]] == gamma[gamma_pos])
		 && (initcode[gamma_enable[0] + 1] == gamma[gamma_pos + 1])) {
		 if (gamma[gamma_pos + 4])
			initcode[gamma_enable[0] + 4 + 15] |= 0x01;
		 else
			initcode[gamma_enable[0] + 4 + 15] &= 0xFE;
		 gamma_pos += 1 + 4;
	} else
		 goto find_gamma_bank_err;

	FTS_DEBUG("replace 3-gamma data:");
	print_data(initcode, 1100);

	return 0;

find_gamma_bank_err:
	FTS_INFO("3-gamma bank(%02x %02x) not find",
		 gamma[gamma_pos], gamma[gamma_pos+1]);
	return -ENODATA;
}

static int read_replace_3gamma(struct i2c_client *client, u8 *buf)
{
	int ret = 0;
	u16 initcode_ecc = 0;
	u16 initcode_checksum = 0;
	u8 *gamma = NULL;
	u16 gamma_len = 0;

	FTS_FUNC_ENTER();

	ret = read_3gamma(client, &gamma, &gamma_len);
	if (ret < 0) {
		 FTS_INFO("no vaid 3-gamma data, not replace");
		 return 0;
	}

	ret = replace_3gamma(buf, gamma, gamma_len);
	if (ret < 0) {
		 FTS_ERROR("replace 3-gamma fail");
		 kfree(gamma);
		 return ret;
	}

	ret = cal_lcdinitcode_ecc(buf, &initcode_ecc);
	if (ret < 0) {
		 FTS_ERROR("lcd init code ecc calculate fail");
		 kfree(gamma);
		 return ret;
	}
	FTS_INFO("lcd init code cal ecc:%04x", initcode_ecc);
	buf[4] = (u8)(initcode_ecc >> 8);
	buf[5] = (u8)(initcode_ecc);
	buf[0x43d] = (u8)(initcode_ecc >> 8);
	buf[0x43c] = (u8)(initcode_ecc);

	initcode_checksum = cal_lcdinitcode_checksum(buf + 2, 0x43e - 2);
	FTS_INFO("lcd init code calc checksum:%04x", initcode_checksum);
	buf[0] = (u8)(initcode_checksum >> 8);
	buf[1] = (u8)(initcode_checksum);

	FTS_FUNC_EXIT();

	kfree(gamma);
	return 0;
}


int check_initial_code_valid(struct i2c_client *client, u8 *buf)
{
	int ret = 0;
	u16 initcode_ecc = 0;
	u16 initcode_checksum = 0;

	initcode_checksum = cal_lcdinitcode_checksum(buf + 2, 0x43e - 2);
	FTS_INFO("lcd init code calc checksum:%04x", initcode_checksum);
	if (initcode_checksum != ((u16)((u16)buf[0] << 8) + buf[1])) {
		 FTS_ERROR("Initial Code checksum fail");
		 return -EINVAL;
	}

	ret = cal_lcdinitcode_ecc(buf, &initcode_ecc);
	if (ret < 0) {
		 FTS_ERROR("lcd init code ecc calculate fail");
		 return ret;
	}
	FTS_INFO("lcd init code cal ecc:%04x", initcode_ecc);
	if (initcode_ecc != ((u16)((u16)buf[4] << 8) + buf[5])) {
		 FTS_ERROR("Initial Code ecc check fail");
		 return -EINVAL;
	}

	return 0;
}

static int Ft8006m_Read_Lockdown_From_Boot(struct i2c_client *client)
{
	u8 buf[16] =  {0};
	u8 rbuf[4] = {0};
	u32 i = 0;
	u8 auc_i2c_write_buf[10];
	int i_ret = 0;
	int ret = 0;
	bool inbootloader = false;

	ft8006m_ctpm_i2c_hid2std(client);

	i_ret = ft8006m_ctpm_start_fw_upgrade(client);
	if (i_ret < 0) {
		 FTS_ERROR("[UPGRADE]: send upgrade cmd to FW error!!");
		 return i_ret;
	}

	/*Enter upgrade mode*/
	ft8006m_ctpm_i2c_hid2std(client);
	msleep(10);

	inbootloader = ft8006m_ctpm_check_run_state(client, FTS_RUN_IN_BOOTLOADER);
	if (!inbootloader) {
		 FTS_ERROR("[UPGRADE]: not run in bootloader, upgrade fail!!");
		 return -EIO;
	}

		  rbuf[0] = 0x03;
	rbuf[1] = 0x00;
		  for (i = 0; i < 30; i++) {
			rbuf[2] = 0x0f;
			rbuf[3] = 0xa0;

				  i_ret = ft8006m_i2c_write(client, rbuf, 4);
				   if (i_ret < 0) {
							FTS_ERROR("[FTS] Step 4: read lcm id from flash error when i2c write, i_ret = %d\n", i_ret);
							continue;
				   }

		 msleep(10);

	        i_ret = ft8006m_i2c_read(client, rbuf, 0, buf, 16);
				if (i_ret < 0) {
							FTS_ERROR("[FTS] Step 4: read lcm id from flash error when i2c write, i_ret = %d\n", i_ret);
							continue;
				   }
	      }

	FTS_DEBUG("%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c", \
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8],
			buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
	FTS_DEBUG("[UPGRADE]: reset the new FW!!");
	auc_i2c_write_buf[0] = FTS_REG_RESET_FW;
	ft8006m_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(1000);

	ft8006m_ctpm_i2c_hid2std(client);
if (!strncmp(buf, TP_WHITE_LOCKDOWN, 16)) {
	FTS_DEBUG(" BOOT status:TP color is WHITE\n");
	ret = 1;
} else if (!strncmp(buf, TP_BLACK_LOCKDOWN, 16)) {
	FTS_DEBUG("BOOT status:TP color is BLACK\n");
	ret = 2;
}

	return ret;
}

/************************************************************************
* Name: fts_ctpm_fw_upgrade_use_buf
* Brief: fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
static int ft8006m_ctpm_fw_upgrade_use_buf(struct i2c_client *client, u8 *pbt_buf, u32 dw_lenth)
{
	u8 reg_val[4] = {0};
	u32 i = 0;
	u32 packet_number;
	u32 j = 0;
	u32 temp;
	u32 lenght;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 auc_i2c_write_buf[10];
	u8 upgrade_ecc;
	int i_ret = 0;
	bool inbootloader = false;

	ft8006m_ctpm_i2c_hid2std(client);

	i_ret = ft8006m_ctpm_start_fw_upgrade(client);
	if (i_ret < 0) {
		 FTS_ERROR("[UPGRADE]: send upgrade cmd to FW error!!");
		 return i_ret;
	}

	/*Enter upgrade mode*/
	ft8006m_ctpm_i2c_hid2std(client);
	msleep(10);

	inbootloader = ft8006m_ctpm_check_run_state(client, FTS_RUN_IN_BOOTLOADER);
	if (!inbootloader) {
		 FTS_ERROR("[UPGRADE]: not run in bootloader, upgrade fail!!");
		 return -EIO;
	}

	/*send upgrade type to reg 0x09: 0x0B: upgrade; 0x0A: download*/
	auc_i2c_write_buf[0] = 0x09;
	auc_i2c_write_buf[1] = 0x0B;
	ft8006m_i2c_write(client, auc_i2c_write_buf, 2);

	/*
	* All.bin <= 128K
	* APP.bin <= 94K
	* LCD_CFG <= 4K
	*/
	auc_i2c_write_buf[0] = 0xB0;
	auc_i2c_write_buf[1] = (u8) ((dw_lenth >> 16) & 0xFF);
	auc_i2c_write_buf[2] = (u8) ((dw_lenth >> 8) & 0xFF);
	auc_i2c_write_buf[3] = (u8) (dw_lenth & 0xFF);
	ft8006m_i2c_write(client, auc_i2c_write_buf, 4);


	/*erase the app erea in flash*/
	i_ret = ft8006m_ctpm_erase_flash(client);
	if (i_ret < 0) {
		 FTS_ERROR("[UPGRADE]: erase flash error!!");
		 return i_ret;
	}

	/*write FW to ctpm flash*/
	upgrade_ecc = 0;
	FTS_DEBUG("[UPGRADE]: write FW to ctpm flash!!");
	temp = 0;
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = FTS_FW_WRITE_CMD;

	for (j = 0; j < packet_number; j++) {
		 temp = 0x5000 + j * FTS_PACKET_LENGTH;
		 packet_buf[1] = (u8) (temp >> 16);
		 packet_buf[2] = (u8) (temp >> 8);
		 packet_buf[3] = (u8) temp;
		 lenght = FTS_PACKET_LENGTH;
		 packet_buf[4] = (u8) (lenght >> 8);
		 packet_buf[5] = (u8) lenght;
		 for (i = 0; i < FTS_PACKET_LENGTH; i++) {
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			upgrade_ecc ^= packet_buf[6 + i];
		 }
		 ft8006m_i2c_write(client, packet_buf, FTS_PACKET_LENGTH + 6);


		 for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			ft8006m_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

			if ((j + 0x1000 + (0x5000/FTS_PACKET_LENGTH)) == (((reg_val[0]) << 8) | reg_val[1])) {
				break;
			}

			if (i > 15) {
				msleep(1);
				FTS_DEBUG("[UPGRADE]: write flash: host : %x status : %x!!", (j + 0x1000 + (0x5000/FTS_PACKET_LENGTH)), (((reg_val[0]) << 8) | reg_val[1]));
			}

			ft8006m_ctpm_upgrade_delay(10000);
		 }
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0) {
		 temp = 0x5000 + packet_number * FTS_PACKET_LENGTH;
		 packet_buf[1] = (u8) (temp >> 16);
		 packet_buf[2] = (u8) (temp >> 8);
		 packet_buf[3] = (u8) temp;
		 temp = (dw_lenth) % FTS_PACKET_LENGTH;
		 packet_buf[4] = (u8) (temp >> 8);
		 packet_buf[5] = (u8) temp;
		 for (i = 0; i < temp; i++) {
			packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			upgrade_ecc ^= packet_buf[6 + i];
		 }
		 ft8006m_i2c_write(client, packet_buf, temp + 6);


		 for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			ft8006m_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

			if ((0x1000 + ((0x5000 + packet_number * FTS_PACKET_LENGTH)/((dw_lenth) % FTS_PACKET_LENGTH))) == (((reg_val[0]) << 8) | reg_val[1])) {
				break;
			}

			if (i > 15) {
				msleep(1);
				FTS_DEBUG("[UPGRADE]: write flash: host : %x status : %x!!", (j + 0x1000 + (0x5000/FTS_PACKET_LENGTH)), (((reg_val[0]) << 8) | reg_val[1]));
			}

			ft8006m_ctpm_upgrade_delay(10000);
		 }
	}

	msleep(50);

	/*********Step 6: read out checksum***********************/
	/*send the opration head */
	FTS_DEBUG("[UPGRADE]: read out checksum!!");
	auc_i2c_write_buf[0] = 0x64;
	ft8006m_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(300);

	temp = 0x5000;
	auc_i2c_write_buf[0] = 0x65;
	auc_i2c_write_buf[1] = (u8)(temp >> 16);
	auc_i2c_write_buf[2] = (u8)(temp >> 8);
	auc_i2c_write_buf[3] = (u8)(temp);
	temp = (64*1024-1);
	auc_i2c_write_buf[4] = (u8)(temp >> 8);
	auc_i2c_write_buf[5] = (u8)(temp);
	i_ret = ft8006m_i2c_write(client, auc_i2c_write_buf, 6);
	msleep(dw_lenth/256);

	temp = (0x5000+(64*1024-1));
	auc_i2c_write_buf[0] = 0x65;
	auc_i2c_write_buf[1] = (u8)(temp >> 16);
	auc_i2c_write_buf[2] = (u8)(temp >> 8);
	auc_i2c_write_buf[3] = (u8)(temp);
	temp = (dw_lenth-(64*1024-1));
	auc_i2c_write_buf[4] = (u8)(temp >> 8);
	auc_i2c_write_buf[5] = (u8)(temp);
	i_ret = ft8006m_i2c_write(client, auc_i2c_write_buf, 6);
	msleep(dw_lenth/256);

	for (i = 0; i < 100; i++) {
		 auc_i2c_write_buf[0] = 0x6a;
		 reg_val[0] = reg_val[1] = 0x00;
		 ft8006m_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

		 if (0xF0 == reg_val[0] && 0x55 == reg_val[1]) {
			FTS_DEBUG("[UPGRADE]: reg_val[0]=%02x reg_val[0]=%02x!!", reg_val[0], reg_val[1]);
			break;
		 }
		 msleep(1);

	}
	auc_i2c_write_buf[0] = 0x66;
	ft8006m_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != upgrade_ecc) {
		 FTS_ERROR("[UPGRADE]: ecc error! FW=%02x upgrade_ecc=%02x!!", reg_val[0], upgrade_ecc);
		 return -EIO;
	}
	FTS_DEBUG("[UPGRADE]: checksum %x %x!!", reg_val[0], upgrade_ecc);

	FTS_DEBUG("[UPGRADE]: reset the new FW!!");
	auc_i2c_write_buf[0] = FTS_REG_RESET_FW;
	ft8006m_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(1000);

	ft8006m_ctpm_i2c_hid2std(client);

	return 0;
}

/************************************************************************
* Name: fts_ctpm_fw_upgrade_use_buf
* Brief: fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
static int ft8006m_ctpm_lcd_cfg_upgrade_use_buf(struct i2c_client  *client, u8 *pbt_buf, u32 dw_lenth)
{
	u8 reg_val[4] = {0};
	u32 i = 0;
	u32 packet_number;
	u32 j = 0;
	u32 temp;
	u32 lenght;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 auc_i2c_write_buf[10];
	u8 upgrade_ecc;
	int i_ret;

	ft8006m_ctpm_i2c_hid2std(client);

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		 /*write 0xaa to register FTS_RST_CMD_REG1 */
		 ft8006m_i2c_write_reg(client, FTS_RST_CMD_REG1, FTS_UPGRADE_AA);
		 msleep(10);

		/*write 0x55 to register FTS_RST_CMD_REG1*/
		 ft8006m_i2c_write_reg(client, FTS_RST_CMD_REG1, FTS_UPGRADE_55);
		 msleep(200);

		 /*Enter upgrade mode*/
		 ft8006m_ctpm_i2c_hid2std(client);

		 msleep(10);
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		 auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
		 i_ret = ft8006m_i2c_write(client, auc_i2c_write_buf, 2);
		 if (i_ret < 0) {
			FTS_ERROR("[UPGRADE]: failed writing  0x55 and 0xaa!!");
			continue;
		 }

		 /*check run in bootloader or not*/
		 msleep(1);
		auc_i2c_write_buf[0] = FTS_READ_ID_REG;
		 auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
		 reg_val[0] = reg_val[1] = 0x00;
		 ft8006m_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);

		 if (reg_val[0] == ft8006m_chip_types.bootloader_idh
			&& reg_val[1] == ft8006m_chip_types.bootloader_idl) {
			FTS_DEBUG("[UPGRADE]: read bootload id ok!! ID1 = 0x%x, ID2 = 0x%x!!", reg_val[0], reg_val[1]);
			break;
		 } else {
			FTS_ERROR("[UPGRADE]: read bootload id fail!! ID1 = 0x%x, ID2 = 0x%x!!", reg_val[0], reg_val[1]);
			continue;
		 }
	}

	if (i >= FTS_UPGRADE_LOOP)
		 return -EIO;


	i_ret = read_replace_3gamma(client, pbt_buf);
	if (i_ret < 0) {
		 FTS_ERROR("replace 3-gamma fail, not upgrade lcd init code");
		 return i_ret;
	}

	i_ret = check_initial_code_valid(client, pbt_buf);
	if (i_ret < 0) {
		 FTS_ERROR("initial code invalid, not upgrade lcd init code");
		 return i_ret;
	}

	/*send upgrade type to reg 0x09: 0x0B: upgrade; 0x0A: download*/
	auc_i2c_write_buf[0] = 0x09;
	auc_i2c_write_buf[1] = 0x0C;
	ft8006m_i2c_write(client, auc_i2c_write_buf, 2);

	/*Step 4:erase app and panel paramenter area*/
	FTS_DEBUG("[UPGRADE]: erase app and panel paramenter area!!");
	auc_i2c_write_buf[0] = FTS_ERASE_APP_REG;
	ft8006m_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(1000);

	for (i = 0; i < 15; i++) {
		 auc_i2c_write_buf[0] = 0x6a;
		 reg_val[0] = reg_val[1] = 0x00;
		 ft8006m_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);
		 if (0xF0 == reg_val[0] && 0xAA == reg_val[1]) {
			break;
		 }
		 msleep(50);
	}
	FTS_DEBUG("[UPGRADE]: erase app area reg_val[0] = %x reg_val[1] = %x!!", reg_val[0], reg_val[1]);

	auc_i2c_write_buf[0] = 0xB0;
	auc_i2c_write_buf[1] = 0;
	auc_i2c_write_buf[2] = (u8) ((dw_lenth >> 8) & 0xFF);
	auc_i2c_write_buf[3] = (u8) (dw_lenth & 0xFF);
	ft8006m_i2c_write(client, auc_i2c_write_buf, 4);

	/*write FW to ctpm flash*/
	upgrade_ecc = 0;
	FTS_DEBUG("[UPGRADE]: write FW to ctpm flash!!");
	temp = 0;
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = FTS_FW_WRITE_CMD;
	packet_buf[1] = 0;
	for (j = 0; j < packet_number; j++) {
		 temp = j * FTS_PACKET_LENGTH;
		 packet_buf[2] = (u8) (temp >> 8);
		 packet_buf[3] = (u8) temp;
		 lenght = FTS_PACKET_LENGTH;
		 packet_buf[4] = (u8) (lenght >> 8);
		 packet_buf[5] = (u8) lenght;
		 for (i = 0; i < FTS_PACKET_LENGTH; i++) {
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			upgrade_ecc ^= packet_buf[6 + i];
		 }
		 ft8006m_i2c_write(client, packet_buf, FTS_PACKET_LENGTH + 6);


		 for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			ft8006m_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

			if ((j + 0x1000) == (((reg_val[0]) << 8) | reg_val[1])) {
				break;
			}

			if (i > 15) {
				msleep(1);
				FTS_DEBUG("[UPGRADE]: write flash: host : %x status : %x!!", (j + 0x1000 + (0x5000/FTS_PACKET_LENGTH)), (((reg_val[0]) << 8) | reg_val[1]));
			}

			ft8006m_ctpm_upgrade_delay(10000);
		 }
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0) {
		 temp = packet_number * FTS_PACKET_LENGTH;
		 packet_buf[2] = (u8) (temp >> 8);
		 packet_buf[3] = (u8) temp;
		 temp = (dw_lenth) % FTS_PACKET_LENGTH;
		 packet_buf[4] = (u8) (temp >> 8);
		 packet_buf[5] = (u8) temp;
		 for (i = 0; i < temp; i++) {
			packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			upgrade_ecc ^= packet_buf[6 + i];
		 }
		 ft8006m_i2c_write(client, packet_buf, temp + 6);


		 for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			ft8006m_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

			if ((0x1000 + ((packet_number * FTS_PACKET_LENGTH)/((dw_lenth) % FTS_PACKET_LENGTH))) == (((reg_val[0]) << 8) | reg_val[1])) {
				break;
			}

			if (i > 15) {
				msleep(1);
				FTS_DEBUG("[UPGRADE]: write flash: host : %x status : %x!!", (j + 0x1000 + (0x5000/FTS_PACKET_LENGTH)), (((reg_val[0]) << 8) | reg_val[1]));
			}

			ft8006m_ctpm_upgrade_delay(10000);
		 }
	}

	msleep(50);

	/*********Step 6: read out checksum***********************/
	/*send the opration head */
	FTS_DEBUG("[UPGRADE]: read out checksum!!");
	auc_i2c_write_buf[0] = 0x64;
	ft8006m_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(300);

	temp = 0x00;
	auc_i2c_write_buf[0] = 0x65;
	auc_i2c_write_buf[1] = 0;
	auc_i2c_write_buf[2] = (u8)(temp >> 8);
	auc_i2c_write_buf[3] = (u8)(temp);
	temp = dw_lenth;
	auc_i2c_write_buf[4] = (u8)(temp >> 8);
	auc_i2c_write_buf[5] = (u8)(temp);
	i_ret = ft8006m_i2c_write(client, auc_i2c_write_buf, 6);
	msleep(dw_lenth/256);

	for (i = 0; i < 100; i++) {
		 auc_i2c_write_buf[0] = 0x6a;
		 reg_val[0] = reg_val[1] = 0x00;
		 ft8006m_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

		 if (0xF0 == reg_val[0] && 0x55 == reg_val[1]) {
			FTS_DEBUG("[UPGRADE]: reg_val[0]=%02x reg_val[0]=%02x!!", reg_val[0], reg_val[1]);
			break;
		 }
		 msleep(1);

	}
	auc_i2c_write_buf[0] = 0x66;
	ft8006m_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != upgrade_ecc) {
		 FTS_ERROR("[UPGRADE]: ecc error! FW=%02x upgrade_ecc=%02x!!", reg_val[0], upgrade_ecc);
		 return -EIO;
	}
	FTS_DEBUG("[UPGRADE]: checksum %x %x!!", reg_val[0], upgrade_ecc);

	FTS_DEBUG("[UPGRADE]: reset the new FW!!");
	auc_i2c_write_buf[0] = FTS_REG_RESET_FW;
	ft8006m_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(1000);

	ft8006m_ctpm_i2c_hid2std(client);

	return 0;
}
#endif
/************************************************************************
* Name: fts_ctpm_fw_upgrade_with_app_i_file
* Brief:  upgrade with *.i file
* Input: i2c info
* Output:
* Return: fail < 0
***********************************************************************/
static int ft8006m_ctpm_fw_upgrade_with_app_i_file(struct i2c_client *client)
{
	int i_ret = 0;
	u32 fw_len;
	u8 *fw_buf;

	FTS_INFO("[UPGRADE]**********start upgrade with app.i**********");

	fw_len = ft8006m_g_fw_len - APP_OFFSET;
	fw_buf = ft8006m_g_fw_file + APP_OFFSET;
	if (fw_len < APP_FILE_MIN_SIZE || fw_len > APP_FILE_MAX_SIZE) {
		 FTS_ERROR("[UPGRADE]: FW length(%x) error", fw_len);
		 return -EIO;
	}

	i_ret = ft8006m_ctpm_fw_upgrade_use_buf(client, fw_buf, fw_len);
	if (i_ret != 0) {
		 FTS_ERROR("[UPGRADE] upgrade app.i failed");
	} else {
		 FTS_INFO("[UPGRADE]: upgrade app.i succeed");
	}

	return i_ret;
}

/************************************************************************
* Name: ft8006m_ctpm_fw_upgrade_with_lcd_cfg_i_file
* Brief:  upgrade with *.i file
* Input: i2c info
* Output: no
* Return: fail <0
***********************************************************************/
static int ft8006m_ctpm_fw_upgrade_with_lcd_cfg_i_file(struct i2c_client *client)
{
	int i_ret = 0;
	u32 fw_len;
	u8 *fw_buf;

	FTS_INFO("[UPGRADE]**********start upgrade with lcd init code**********");

	fw_len = ft8006m_g_fw_len;
	fw_buf = ft8006m_g_fw_file;
	if (fw_len < APP_FILE_MIN_SIZE || fw_len > APP_FILE_MAX_SIZE) {
		 FTS_ERROR("[UPGRADE]: FW length(%x) error", fw_len);
		 return -EIO;
	}

	/*FW upgrade*/
	i_ret = ft8006m_ctpm_lcd_cfg_upgrade_use_buf(client, fw_buf, 4096);
	if (i_ret != 0) {
		 FTS_ERROR("[UPGRADE] init code upgrade fail, ret=%d", i_ret);
	} else {
		 FTS_INFO("[UPGRADE] init code upgrade succeed");
	}

	return i_ret;
}

/************************************************************************
* Name: fts_ctpm_fw_upgrade_with_app_bin_file
* Brief: upgrade with *.bin file
* Input: i2c info, file name
* Output: no
* Return: success =0
***********************************************************************/
static int ft8006m_ctpm_fw_upgrade_with_app_bin_file(struct i2c_client *client, char *firmware_name)
{
	u8 *pbt_buf = NULL;
	int i_ret = 0;
	int fwsize = 0;

	FTS_INFO("[UPGRADE]**********start upgrade with app.bin**********");

	fwsize = ft8006m_GetFirmwareSize(firmware_name);
	if (fwsize < APP_FILE_MIN_SIZE || fwsize > APP_FILE_MAX_SIZE) {
		 FTS_ERROR("[UPGRADE]: app.bin length(%x) error, upgrade fail", fwsize);
		 return -EIO;
	}

	pbt_buf = (unsigned char *)kmalloc(fwsize + 1, GFP_KERNEL);
	if (NULL == pbt_buf) {
		 FTS_ERROR(" malloc pbt_buf failed ");
		 goto ERROR_BIN;
	}

	if (ft8006m_ReadFirmware(firmware_name, pbt_buf)) {
		 FTS_ERROR("[UPGRADE]: request_firmware failed!!");
		 goto ERROR_BIN;
	}
#if FTS_AUTO_UPGRADE_FOR_LCD_CFG_EN
	i_ret = ft8006m_ctpm_lcd_cfg_upgrade_use_buf(client, pbt_buf, 4096);
	i_ret = ft8006m_ctpm_fw_upgrade_use_buf(client, pbt_buf + APP_OFFSET, fwsize - APP_OFFSET);
#else
		 i_ret = ft8006m_ctpm_fw_upgrade_use_buf(client, pbt_buf, fwsize);
#endif
		 if (i_ret != 0) {
			FTS_ERROR("[UPGRADE]: upgrade app.bin failed");
			goto ERROR_BIN;
		 } else {
			FTS_INFO("[UPGRADE]: upgrade app.bin succeed");
		 }


	kfree(pbt_buf);
	return i_ret;
ERROR_BIN:
	kfree(pbt_buf);
	return -EIO;
}


