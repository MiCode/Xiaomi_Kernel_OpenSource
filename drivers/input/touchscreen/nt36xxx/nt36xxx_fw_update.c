// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 47247 $
 * $Date: 2019-07-10 10:41:36 +0800 (Wed, 10 Jul 2019) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#if !IS_ENABLED(CONFIG_TOUCHSCREEN_NT36XXX_SPI) /* TOUCHSCREEN_NT36XXX I2C */

#include <linux/firmware.h>

#include "nt36xxx.h"

#if BOOT_UPDATE_FIRMWARE

#define SIZE_4KB 4096
#define FLASH_SECTOR_SIZE SIZE_4KB
#define SIZE_64KB 65536
#define BLOCK_64KB_NUM 4
#define FW_BIN_VER_OFFSET (fw_need_write_size - SIZE_4KB)
#define FW_BIN_VER_BAR_OFFSET (FW_BIN_VER_OFFSET + 1)

#define NVT_FLASH_END_FLAG_LEN 3
#define NVT_FLASH_END_FLAG_ADDR (fw_need_write_size - NVT_FLASH_END_FLAG_LEN)

const struct firmware *fw_entry = NULL;
static size_t fw_need_write_size = 0;

static int32_t nvt_get_fw_need_write_size(const struct firmware *fw_entry)
{
	int32_t i = 0;
	int32_t total_sectors_to_check = 0;

	total_sectors_to_check = fw_entry->size / FLASH_SECTOR_SIZE;
	/* printk("total_sectors_to_check = %d\n", total_sectors_to_check); */

	for (i = total_sectors_to_check; i > 0; i--) {
		/* printk("current end flag address checked = 0x%X\n", i * FLASH_SECTOR_SIZE - NVT_FLASH_END_FLAG_LEN); */
		/* check if there is end flag "NVT" at the end of this sector */
		if (memcmp(&fw_entry->data[i * FLASH_SECTOR_SIZE - NVT_FLASH_END_FLAG_LEN], "NVT", NVT_FLASH_END_FLAG_LEN) == 0) {
			fw_need_write_size = i * FLASH_SECTOR_SIZE;
			NVT_LOG("fw_need_write_size = %zu(0x%zx)\n", fw_need_write_size, fw_need_write_size);
			return 0;
		}
	}

	NVT_ERR("end flag \"NVT\" not found!\n");
	return -1;
}

/*******************************************************
Description:
	Novatek touchscreen request update firmware function.

return:
	Executive outcomes. 0---succeed. -1,-22---failed.
*******************************************************/
int32_t update_firmware_request(char *filename)
{
	int32_t ret = 0;

	if (NULL == filename) {
		return -1;
	}

	NVT_LOG("filename is %s\n", filename);

	ret = request_firmware_nowarn(&fw_entry, filename, &ts->client->dev);
	if (ret) {
		NVT_ERR("firmware load failed, ret=%d\n", ret);
		return ret;
	}

	// check FW need to write size
	if (nvt_get_fw_need_write_size(fw_entry)) {
		NVT_ERR("get fw need to write size fail!\n");
		return -EINVAL;
	}

	// check if FW version add FW version bar equals 0xFF
	if (*(fw_entry->data + FW_BIN_VER_OFFSET) + *(fw_entry->data + FW_BIN_VER_BAR_OFFSET) != 0xFF) {
		NVT_ERR("bin file FW_VER + FW_VER_BAR should be 0xFF!\n");
		NVT_ERR("FW_VER=0x%02X, FW_VER_BAR=0x%02X\n", *(fw_entry->data+FW_BIN_VER_OFFSET), *(fw_entry->data+FW_BIN_VER_BAR_OFFSET));
		return -EINVAL;
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen release update firmware function.

return:
	n.a.
*******************************************************/
void update_firmware_release(void)
{
	if (fw_entry) {
		release_firmware(fw_entry);
	}
	fw_entry=NULL;
}

/*******************************************************
Description:
	Novatek touchscreen check firmware version function.

return:
	Executive outcomes. 0---need update. 1---need not
	update.
*******************************************************/
int32_t Check_FW_Ver(void)
{
	uint8_t buf[16] = {0};
	int32_t ret = 0;

	//write i2c index to EVENT BUF ADDR
	ret = nvt_set_page(I2C_BLDR_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);
	if (ret < 0) {
		NVT_ERR("i2c write error!(%d)\n", ret);
		return ret;
	}

	//read Firmware Version
	buf[0] = EVENT_MAP_FWINFO;
	buf[1] = 0x00;
	buf[2] = 0x00;
	ret = CTP_I2C_READ(ts->client, I2C_BLDR_Address, buf, 3);
	if (ret < 0) {
		NVT_ERR("i2c read error!(%d)\n", ret);
		return ret;
	}

	NVT_LOG("IC FW Ver = 0x%02X, FW Ver Bar = 0x%02X\n", buf[1], buf[2]);
	NVT_LOG("Bin FW Ver = 0x%02X, FW ver Bar = 0x%02X\n",
			fw_entry->data[FW_BIN_VER_OFFSET], fw_entry->data[FW_BIN_VER_BAR_OFFSET]);

	// check IC FW_VER + FW_VER_BAR equals 0xFF or not, need to update if not
	if ((buf[1] + buf[2]) != 0xFF) {
		NVT_ERR("IC FW_VER + FW_VER_BAR not equals to 0xFF!\n");
		return 0;
	}

	// compare IC and binary FW version
	if (buf[1] > fw_entry->data[FW_BIN_VER_OFFSET])
		return 1;
	else
		return 0;
}

/*******************************************************
Description:
	Novatek touchscreen resume from deep power down function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
int32_t Resume_PD(void)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;

	// Resume Command
	buf[0] = 0x00;
	buf[1] = 0xAB;
	ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		NVT_ERR("Write Enable error!!(%d)\n", ret);
		return ret;
	}

	// Check 0xAA (Resume Command)
	retry = 0;
	while(1) {
		msleep(1);
		buf[0] = 0x00;
		buf[1] = 0x00;
		ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			NVT_ERR("Check 0xAA (Resume Command) error!!(%d)\n", ret);
			return ret;
		}
		if (buf[1] == 0xAA) {
			break;
		}
		retry++;
		if (unlikely(retry > 20)) {
			NVT_ERR("Check 0xAA (Resume Command) error!! status=0x%02X\n", buf[1]);
			return -1;
		}
	}
	msleep(10);

	NVT_LOG("Resume PD OK\n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen check firmware checksum function.

return:
	Executive outcomes. 0---checksum not match.
	1---checksum match. -1--- checksum read failed.
*******************************************************/
int32_t Check_CheckSum(void)
{
	uint8_t buf[64] = {0};
	uint32_t XDATA_Addr = ts->mmap->READ_FLASH_CHECKSUM_ADDR;
	int32_t ret = 0;
	int32_t i = 0;
	int32_t k = 0;
	uint16_t WR_Filechksum[BLOCK_64KB_NUM] = {0};
	uint16_t RD_Filechksum[BLOCK_64KB_NUM] = {0};
	size_t len_in_blk = 0;
	int32_t retry = 0;

	if (Resume_PD()) {
		NVT_ERR("Resume PD error!!\n");
		return -1;
	}

	for (i = 0; i < BLOCK_64KB_NUM; i++) {
		if (fw_need_write_size > (i * SIZE_64KB)) {
			// Calculate WR_Filechksum of each 64KB block
			len_in_blk = min(fw_need_write_size - i * SIZE_64KB, (size_t)SIZE_64KB);
			WR_Filechksum[i] = i + 0x00 + 0x00 + (((len_in_blk - 1) >> 8) & 0xFF) + ((len_in_blk - 1) & 0xFF);
			for (k = 0; k < len_in_blk; k++) {
				WR_Filechksum[i] += fw_entry->data[k + i * SIZE_64KB];
			}
			WR_Filechksum[i] = 65535 - WR_Filechksum[i] + 1;

			// Fast Read Command
			buf[0] = 0x00;
			buf[1] = 0x07;
			buf[2] = i;
			buf[3] = 0x00;
			buf[4] = 0x00;
			buf[5] = ((len_in_blk - 1) >> 8) & 0xFF;
			buf[6] = (len_in_blk - 1) & 0xFF;
			ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 7);
			if (ret < 0) {
				NVT_ERR("Fast Read Command error!!(%d)\n", ret);
				return ret;
			}
			// Check 0xAA (Fast Read Command)
			retry = 0;
			while (1) {
				msleep(80);
				buf[0] = 0x00;
				buf[1] = 0x00;
				ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
				if (ret < 0) {
					NVT_ERR("Check 0xAA (Fast Read Command) error!!(%d)\n", ret);
					return ret;
				}
				if (buf[1] == 0xAA) {
					break;
				}
				retry++;
				if (unlikely(retry > 5)) {
					NVT_ERR("Check 0xAA (Fast Read Command) failed, buf[1]=0x%02X, retry=%d\n", buf[1], retry);
					return -1;
				}
			}
			// Read Checksum (write addr high byte & middle byte)
			ret = nvt_set_page(I2C_BLDR_Address, XDATA_Addr);
			if (ret < 0) {
				NVT_ERR("Read Checksum (write addr high byte & middle byte) error!!(%d)\n", ret);
				return ret;
			}
			// Read Checksum
			buf[0] = (XDATA_Addr) & 0xFF;
			buf[1] = 0x00;
			buf[2] = 0x00;
			ret = CTP_I2C_READ(ts->client, I2C_BLDR_Address, buf, 3);
			if (ret < 0) {
				NVT_ERR("Read Checksum error!!(%d)\n", ret);
				return ret;
			}

			RD_Filechksum[i] = (uint16_t)((buf[2] << 8) | buf[1]);
			if (WR_Filechksum[i] != RD_Filechksum[i]) {
				NVT_ERR("RD_Filechksum[%d]=0x%04X, WR_Filechksum[%d]=0x%04X\n", i, RD_Filechksum[i], i, WR_Filechksum[i]);
				NVT_ERR("firmware checksum not match!!\n");
				return 0;
			}
		}
	}

	NVT_LOG("firmware checksum match\n");
	return 1;
}

/*******************************************************
Description:
	Novatek touchscreen initial bootloader and flash
	block function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
int32_t Init_BootLoader(void)
{
	uint8_t buf[64] = {0};
	int32_t ret = 0;
	int32_t retry = 0;

	// SW Reset & Idle
	nvt_sw_reset_idle();

	// Initiate Flash Block
	buf[0] = 0x00;
	buf[1] = 0x00;
	buf[2] = I2C_FW_Address;
	ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 3);
	if (ret < 0) {
		NVT_ERR("Inittial Flash Block error!!(%d)\n", ret);
		return ret;
	}

	// Check 0xAA (Initiate Flash Block)
	retry = 0;
	while(1) {
		msleep(1);
		buf[0] = 0x00;
		buf[1] = 0x00;
		ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			NVT_ERR("Check 0xAA (Inittial Flash Block) error!!(%d)\n", ret);
			return ret;
		}
		if (buf[1] == 0xAA) {
			break;
		}
		retry++;
		if (unlikely(retry > 20)) {
			NVT_ERR("Check 0xAA (Inittial Flash Block) error!! status=0x%02X\n", buf[1]);
			return -1;
		}
	}

	NVT_LOG("Init OK \n");
	msleep(20);

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen erase flash sectors function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
int32_t Erase_Flash(void)
{
	uint8_t buf[64] = {0};
	int32_t ret = 0;
	int32_t count = 0;
	int32_t i = 0;
	int32_t Flash_Address = 0;
	int32_t retry = 0;

	// Write Enable
	buf[0] = 0x00;
	buf[1] = 0x06;
	ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		NVT_ERR("Write Enable (for Write Status Register) error!!(%d)\n", ret);
		return ret;
	}
	// Check 0xAA (Write Enable)
	retry = 0;
	while (1) {
		msleep(1);
		buf[0] = 0x00;
		buf[1] = 0x00;
		ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			NVT_ERR("Check 0xAA (Write Enable for Write Status Register) error!!(%d)\n", ret);
			return ret;
		}
		if (buf[1] == 0xAA) {
			break;
		}
		retry++;
		if (unlikely(retry > 20)) {
			NVT_ERR("Check 0xAA (Write Enable for Write Status Register) error!! status=0x%02X\n", buf[1]);
			return -1;
		}
	}

	// Write Status Register
	buf[0] = 0x00;
	buf[1] = 0x01;
	buf[2] = 0x00;
	ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 3);
	if (ret < 0) {
		NVT_ERR("Write Status Register error!!(%d)\n", ret);
		return ret;
	}
	// Check 0xAA (Write Status Register)
	retry = 0;
	while (1) {
		msleep(1);
		buf[0] = 0x00;
		buf[1] = 0x00;
		ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			NVT_ERR("Check 0xAA (Write Status Register) error!!(%d)\n", ret);
			return ret;
		}
		if (buf[1] == 0xAA) {
			break;
		}
		retry++;
		if (unlikely(retry > 20)) {
			NVT_ERR("Check 0xAA (Write Status Register) error!! status=0x%02X\n", buf[1]);
			return -1;
		}
	}

	// Read Status
	retry = 0;
	while (1) {
		msleep(5);
		buf[0] = 0x00;
		buf[1] = 0x05;
		ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			NVT_ERR("Read Status (for Write Status Register) error!!(%d)\n", ret);
			return ret;
		}

		// Check 0xAA (Read Status)
		buf[0] = 0x00;
		buf[1] = 0x00;
		buf[2] = 0x00;
		ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 3);
		if (ret < 0) {
			NVT_ERR("Check 0xAA (Read Status for Write Status Register) error!!(%d)\n", ret);
			return ret;
		}
		if ((buf[1] == 0xAA) && (buf[2] == 0x00)) {
			break;
		}
		retry++;
		if (unlikely(retry > 100)) {
			NVT_ERR("Check 0xAA (Read Status for Write Status Register) failed, buf[1]=0x%02X, buf[2]=0x%02X, retry=%d\n", buf[1], buf[2], retry);
			return -1;
		}
	}

	if (fw_need_write_size % FLASH_SECTOR_SIZE)
		count = fw_need_write_size / FLASH_SECTOR_SIZE + 1;
	else
		count = fw_need_write_size / FLASH_SECTOR_SIZE;

	for(i = 0; i < count; i++) {
		// Write Enable
		buf[0] = 0x00;
		buf[1] = 0x06;
		ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			NVT_ERR("Write Enable error!!(%d,%d)\n", ret, i);
			return ret;
		}
		// Check 0xAA (Write Enable)
		retry = 0;
		while (1) {
			msleep(1);
			buf[0] = 0x00;
			buf[1] = 0x00;
			ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				NVT_ERR("Check 0xAA (Write Enable) error!!(%d,%d)\n", ret, i);
				return ret;
			}
			if (buf[1] == 0xAA) {
				break;
			}
			retry++;
			if (unlikely(retry > 20)) {
				NVT_ERR("Check 0xAA (Write Enable) error!! status=0x%02X\n", buf[1]);
				return -1;
			}
		}

		Flash_Address = i * FLASH_SECTOR_SIZE;

		// Sector Erase
		buf[0] = 0x00;
		buf[1] = 0x20;    // Command : Sector Erase
		buf[2] = ((Flash_Address >> 16) & 0xFF);
		buf[3] = ((Flash_Address >> 8) & 0xFF);
		buf[4] = (Flash_Address & 0xFF);
		ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 5);
		if (ret < 0) {
			NVT_ERR("Sector Erase error!!(%d,%d)\n", ret, i);
			return ret;
		}
		// Check 0xAA (Sector Erase)
		retry = 0;
		while (1) {
			msleep(1);
			buf[0] = 0x00;
			buf[1] = 0x00;
			ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				NVT_ERR("Check 0xAA (Sector Erase) error!!(%d,%d)\n", ret, i);
				return ret;
			}
			if (buf[1] == 0xAA) {
				break;
			}
			retry++;
			if (unlikely(retry > 20)) {
				NVT_ERR("Check 0xAA (Sector Erase) failed, buf[1]=0x%02X, retry=%d\n", buf[1], retry);
				return -1;
			}
		}

		// Read Status
		retry = 0;
		while (1) {
			msleep(5);
			buf[0] = 0x00;
			buf[1] = 0x05;
			ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				NVT_ERR("Read Status error!!(%d,%d)\n", ret, i);
				return ret;
			}

			// Check 0xAA (Read Status)
			buf[0] = 0x00;
			buf[1] = 0x00;
			buf[2] = 0x00;
			ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 3);
			if (ret < 0) {
				NVT_ERR("Check 0xAA (Read Status) error!!(%d,%d)\n", ret, i);
				return ret;
			}
			if ((buf[1] == 0xAA) && (buf[2] == 0x00)) {
				break;
			}
			retry++;
			if (unlikely(retry > 100)) {
				NVT_ERR("Check 0xAA (Read Status) failed, buf[1]=0x%02X, buf[2]=0x%02X, retry=%d\n", buf[1], buf[2], retry);
				return -1;
			}
		}
	}

	NVT_LOG("Erase OK \n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen write flash sectors function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
int32_t Write_Flash(void)
{
	uint8_t buf[64] = {0};
	uint32_t XDATA_Addr = ts->mmap->RW_FLASH_DATA_ADDR;
	uint32_t Flash_Address = 0;
	int32_t i = 0, j = 0, k = 0;
	uint8_t tmpvalue = 0;
	int32_t count = 0;
	int32_t ret = 0;
	int32_t retry = 0;
	int32_t percent = 0;
	int32_t previous_percent = -1;

	// change I2C buffer index
	ret = nvt_set_page(I2C_BLDR_Address, XDATA_Addr);
	if (ret < 0) {
		NVT_ERR("change I2C buffer index error!!(%d)\n", ret);
		return ret;
	}

	if (fw_need_write_size % 256)
		count = fw_need_write_size / 256 + 1;
	else
		count = fw_need_write_size / 256;

	for (i = 0; i < count; i++) {
		Flash_Address = i * 256;

		// Write Enable
		buf[0] = 0x00;
		buf[1] = 0x06;
		ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			NVT_ERR("Write Enable error!!(%d)\n", ret);
			return ret;
		}
		// Check 0xAA (Write Enable)
		retry = 0;
		while (1) {
			udelay(100);
			buf[0] = 0x00;
			buf[1] = 0x00;
			ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				NVT_ERR("Check 0xAA (Write Enable) error!!(%d,%d)\n", ret, i);
				return ret;
			}
			if (buf[1] == 0xAA) {
				break;
			}
			retry++;
			if (unlikely(retry > 20)) {
				NVT_ERR("Check 0xAA (Write Enable) error!! status=0x%02X\n", buf[1]);
				return -1;
			}
		}

		// Write Page : 256 bytes
		for (j = 0; j < min(fw_need_write_size - i * 256, (size_t)256); j += 32) {
			buf[0] = (XDATA_Addr + j) & 0xFF;
			for (k = 0; k < 32; k++) {
				buf[1 + k] = fw_entry->data[Flash_Address + j + k];
			}
			ret = CTP_I2C_WRITE(ts->client, I2C_BLDR_Address, buf, 33);
			if (ret < 0) {
				NVT_ERR("Write Page error!!(%d), j=%d\n", ret, j);
				return ret;
			}
		}
		if (fw_need_write_size - Flash_Address >= 256)
			tmpvalue=(Flash_Address >> 16) + ((Flash_Address >> 8) & 0xFF) + (Flash_Address & 0xFF) + 0x00 + (255);
		else
			tmpvalue=(Flash_Address >> 16) + ((Flash_Address >> 8) & 0xFF) + (Flash_Address & 0xFF) + 0x00 + (fw_need_write_size - Flash_Address - 1);

		for (k = 0; k < min(fw_need_write_size - Flash_Address, (size_t)256); k++)
			tmpvalue += fw_entry->data[Flash_Address + k];

		tmpvalue = 255 - tmpvalue + 1;

		// Page Program
		buf[0] = 0x00;
		buf[1] = 0x02;
		buf[2] = ((Flash_Address >> 16) & 0xFF);
		buf[3] = ((Flash_Address >> 8) & 0xFF);
		buf[4] = (Flash_Address & 0xFF);
		buf[5] = 0x00;
		buf[6] = min(fw_need_write_size - Flash_Address, (size_t)256) - 1;
		buf[7] = tmpvalue;
		ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 8);
		if (ret < 0) {
			NVT_ERR("Page Program error!!(%d), i=%d\n", ret, i);
			return ret;
		}
		// Check 0xAA (Page Program)
		retry = 0;
		while (1) {
			msleep(1);
			buf[0] = 0x00;
			buf[1] = 0x00;
			ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				NVT_ERR("Page Program error!!(%d)\n", ret);
				return ret;
			}
			if (buf[1] == 0xAA || buf[1] == 0xEA) {
				break;
			}
			retry++;
			if (unlikely(retry > 20)) {
				NVT_ERR("Check 0xAA (Page Program) failed, buf[1]=0x%02X, retry=%d\n", buf[1], retry);
				return -1;
			}
		}
		if (buf[1] == 0xEA) {
			NVT_ERR("Page Program error!! i=%d\n", i);
			return -3;
		}

		// Read Status
		retry = 0;
		while (1) {
			msleep(5);
			buf[0] = 0x00;
			buf[1] = 0x05;
			ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				NVT_ERR("Read Status error!!(%d)\n", ret);
				return ret;
			}

			// Check 0xAA (Read Status)
			buf[0] = 0x00;
			buf[1] = 0x00;
			buf[2] = 0x00;
			ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 3);
			if (ret < 0) {
				NVT_ERR("Check 0xAA (Read Status) error!!(%d)\n", ret);
				return ret;
			}
			if (((buf[1] == 0xAA) && (buf[2] == 0x00)) || (buf[1] == 0xEA)) {
				break;
			}
			retry++;
			if (unlikely(retry > 100)) {
				NVT_ERR("Check 0xAA (Read Status) failed, buf[1]=0x%02X, buf[2]=0x%02X, retry=%d\n", buf[1], buf[2], retry);
				return -1;
			}
		}
		if (buf[1] == 0xEA) {
			NVT_ERR("Page Program error!! i=%d\n", i);
			return -4;
		}

		percent = ((i + 1) * 100) / count;
		if (((percent % 10) == 0) && (percent != previous_percent)) {
			NVT_LOG("Programming...%2d%%\n", percent);
			previous_percent = percent;
		}
	}

	NVT_LOG("Program OK         \n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen verify checksum of written
	flash function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
int32_t Verify_Flash(void)
{
	uint8_t buf[64] = {0};
	uint32_t XDATA_Addr = ts->mmap->READ_FLASH_CHECKSUM_ADDR;
	int32_t ret = 0;
	int32_t i = 0;
	int32_t k = 0;
	uint16_t WR_Filechksum[BLOCK_64KB_NUM] = {0};
	uint16_t RD_Filechksum[BLOCK_64KB_NUM] = {0};
	size_t len_in_blk = 0;
	int32_t retry = 0;

	for (i = 0; i < BLOCK_64KB_NUM; i++) {
		if (fw_need_write_size > (i * SIZE_64KB)) {
			// Calculate WR_Filechksum of each 64KB block
			len_in_blk = min(fw_need_write_size - i * SIZE_64KB, (size_t)SIZE_64KB);
			WR_Filechksum[i] = i + 0x00 + 0x00 + (((len_in_blk - 1) >> 8) & 0xFF) + ((len_in_blk - 1) & 0xFF);
			for (k = 0; k < len_in_blk; k++) {
				WR_Filechksum[i] += fw_entry->data[k + i * SIZE_64KB];
			}
			WR_Filechksum[i] = 65535 - WR_Filechksum[i] + 1;

			// Fast Read Command
			buf[0] = 0x00;
			buf[1] = 0x07;
			buf[2] = i;
			buf[3] = 0x00;
			buf[4] = 0x00;
			buf[5] = ((len_in_blk - 1) >> 8) & 0xFF;
			buf[6] = (len_in_blk - 1) & 0xFF;
			ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 7);
			if (ret < 0) {
				NVT_ERR("Fast Read Command error!!(%d)\n", ret);
				return ret;
			}
			// Check 0xAA (Fast Read Command)
			retry = 0;
			while (1) {
				msleep(80);
				buf[0] = 0x00;
				buf[1] = 0x00;
				ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
				if (ret < 0) {
					NVT_ERR("Check 0xAA (Fast Read Command) error!!(%d)\n", ret);
					return ret;
				}
				if (buf[1] == 0xAA) {
					break;
				}
				retry++;
				if (unlikely(retry > 5)) {
					NVT_ERR("Check 0xAA (Fast Read Command) failed, buf[1]=0x%02X, retry=%d\n", buf[1], retry);
					return -1;
				}
			}
			// Read Checksum (write addr high byte & middle byte)
			ret = nvt_set_page(I2C_BLDR_Address, XDATA_Addr);
			if (ret < 0) {
				NVT_ERR("Read Checksum (write addr high byte & middle byte) error!!(%d)\n", ret);
				return ret;
			}
			// Read Checksum
			buf[0] = (XDATA_Addr) & 0xFF;
			buf[1] = 0x00;
			buf[2] = 0x00;
			ret = CTP_I2C_READ(ts->client, I2C_BLDR_Address, buf, 3);
			if (ret < 0) {
				NVT_ERR("Read Checksum error!!(%d)\n", ret);
				return ret;
			}

			RD_Filechksum[i] = (uint16_t)((buf[2] << 8) | buf[1]);
			if (WR_Filechksum[i] != RD_Filechksum[i]) {
				NVT_ERR("Verify Fail%d!!\n", i);
				NVT_ERR("RD_Filechksum[%d]=0x%04X, WR_Filechksum[%d]=0x%04X\n", i, RD_Filechksum[i], i, WR_Filechksum[i]);
				return -1;
			}
		}
	}

	NVT_LOG("Verify OK \n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen update firmware function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
int32_t Update_Firmware(void)
{
	int32_t ret = 0;

	//---Stop CRC check to prevent IC auto reboot---
	nvt_stop_crc_reboot();

	// Step 1 : initial bootloader
	ret = Init_BootLoader();
	if (ret) {
		return ret;
	}

	// Step 2 : Resume PD
	ret = Resume_PD();
	if (ret) {
		return ret;
	}

	// Step 3 : Erase
	ret = Erase_Flash();
	if (ret) {
		return ret;
	}

	// Step 4 : Program
	ret = Write_Flash();
	if (ret) {
		return ret;
	}

	// Step 5 : Verify
	ret = Verify_Flash();
	if (ret) {
		return ret;
	}

	//Step 6 : Bootloader Reset
	nvt_bootloader_reset();
	nvt_check_fw_reset_state(RESET_STATE_INIT);
	nvt_get_fw_info();

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen check flash end flag function.

return:
	Executive outcomes. 0---succeed. 1,negative---failed.
*******************************************************/
int32_t nvt_check_flash_end_flag(void)
{
	uint8_t buf[8] = {0};
	uint8_t nvt_end_flag[NVT_FLASH_END_FLAG_LEN + 1] = {0};
	int32_t ret = 0;

	// Step 1 : initial bootloader
	ret = Init_BootLoader();
	if (ret) {
		return ret;
	}

	// Step 2 : Resume PD
	ret = Resume_PD();
	if (ret) {
		return ret;
	}

	// Step 3 : unlock
	buf[0] = 0x00;
	buf[1] = 0x35;
	ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		NVT_ERR("write unlock error!!(%d)\n", ret);
		return ret;
	}
	msleep(10);

	//Step 4 : Flash Read Command
	buf[0] = 0x00;
	buf[1] = 0x03;
	buf[2] = (NVT_FLASH_END_FLAG_ADDR >> 16) & 0xFF; //Addr_H
	buf[3] = (NVT_FLASH_END_FLAG_ADDR >> 8) & 0xFF; //Addr_M
	buf[4] = NVT_FLASH_END_FLAG_ADDR & 0xFF; //Addr_L
	buf[5] = (NVT_FLASH_END_FLAG_LEN >> 8) & 0xFF; //Len_H
	buf[6] = NVT_FLASH_END_FLAG_LEN & 0xFF; //Len_L
	ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 7);
	if (ret < 0) {
		NVT_ERR("write Read Command error!!(%d)\n", ret);
		return ret;
	}
	msleep(10);

	// Check 0xAA (Read Command)
	buf[0] = 0x00;
	buf[1] = 0x00;
	ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		NVT_ERR("Check 0xAA (Read Command) error!!(%d)\n", ret);
		return ret;
	}
	if (buf[1] != 0xAA) {
		NVT_ERR("Check 0xAA (Read Command) error!! status=0x%02X\n", buf[1]);
		return -1;
	}

	msleep(10);

	//Step 5 : Read Flash Data
	ret = nvt_set_page(I2C_BLDR_Address, ts->mmap->READ_FLASH_CHECKSUM_ADDR);
	if (ret < 0) {
		NVT_ERR("change index error!! (%d)\n", ret);
		return ret;
	}
	msleep(10);

	// Read Back
	buf[0] = ts->mmap->READ_FLASH_CHECKSUM_ADDR & 0xFF;
	ret = CTP_I2C_READ(ts->client, I2C_BLDR_Address, buf, 6);
	if (ret < 0) {
		NVT_ERR("Read Back error!! (%d)\n", ret);
		return ret;
	}

	//buf[3:5] => NVT End Flag
	strlcpy(nvt_end_flag, &buf[3], NVT_FLASH_END_FLAG_LEN);
	NVT_LOG("nvt_end_flag=%s (%02X %02X %02X)\n", nvt_end_flag, buf[3], buf[4], buf[5]);

	if (memcmp(nvt_end_flag, "NVT", NVT_FLASH_END_FLAG_LEN) == 0) {
		return 0;
	} else {
		NVT_ERR("\"NVT\" end flag not found!\n");
		return 1;
	}
}

/*******************************************************
Description:
	Novatek touchscreen update firmware when booting
	function.

return:
	n.a.
*******************************************************/
void Boot_Update_Firmware(struct work_struct *work)
{
	int32_t ret = 0;

	char firmware_name[256] = "";

	snprintf(firmware_name, sizeof(firmware_name),
			BOOT_UPDATE_FIRMWARE_NAME);

	// request bin file in "/etc/firmware"
	ret = update_firmware_request(firmware_name);
	if (ret) {
		NVT_ERR("update_firmware_request failed. (%d)\n", ret);
		return;
	}

	mutex_lock(&ts->lock);

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_sw_reset_idle();

	ret = Check_CheckSum();

	if (ret < 0) {	// read firmware checksum failed
		NVT_ERR("read firmware checksum failed\n");
		Update_Firmware();
	} else if ((ret == 0) && (Check_FW_Ver() == 0)) { // (fw checksum not match) && (bin fw version >= ic fw version)
		NVT_LOG("firmware version not match\n");
		Update_Firmware();
	} else if (nvt_check_flash_end_flag()) {
		NVT_LOG("check flash end flag failed\n");
		Update_Firmware();
	} else {
		// Bootloader Reset
		nvt_bootloader_reset();
		ret = nvt_check_fw_reset_state(RESET_STATE_INIT);
		if (ret) {
			NVT_LOG("check fw reset state failed\n");
			Update_Firmware();
		}
	}

	mutex_unlock(&ts->lock);

	update_firmware_release();
}
#endif /* BOOT_UPDATE_FIRMWARE */

#else  /* TOUCHSCREEN_NT36XXX_SPI */

#include <linux/firmware.h>
#include <linux/gpio.h>

#include "nt36xxx.h"

#if NVT_SPI_BOOT_UPDATE_FIRMWARE

#define SIZE_4KB 4096
#define FLASH_SECTOR_SIZE SIZE_4KB
#define FW_BIN_VER_OFFSET (nvt_spi_fw_need_write_size - SIZE_4KB)
#define FW_BIN_VER_BAR_OFFSET (FW_BIN_VER_OFFSET + 1)
#define NVT_FLASH_END_FLAG_LEN 3
#define NVT_FLASH_END_FLAG_ADDR (nvt_spi_fw_need_write_size - NVT_FLASH_END_FLAG_LEN)

#define NVT_DUMP_PARTITION      (0)
#define NVT_DUMP_PARTITION_LEN  (1024)
#define NVT_DUMP_PARTITION_PATH "/data/local/tmp"

static ktime_t nvt_spi_start, nvt_spi_end;
static const struct firmware *nvt_spi_fw_entry;
static size_t nvt_spi_fw_need_write_size;
static uint8_t *nvt_spi_fwbuf;

struct nvt_spi_bin_map_t {
	char name[12];
	uint32_t BIN_addr;
	uint32_t SRAM_addr;
	uint32_t size;
	uint32_t crc;
};

static struct nvt_spi_bin_map_t *nvt_spi_bin_map;

static int32_t nvt_spi_get_fw_need_write_size(const struct firmware *fw_entry)
{
	int32_t i = 0;
	int32_t total_sectors_to_check = 0;

	total_sectors_to_check = fw_entry->size / FLASH_SECTOR_SIZE;
	/* printk("total_sectors_to_check = %d\n", total_sectors_to_check); */

	for (i = total_sectors_to_check; i > 0; i--) {
		// printk("current end flag address checked = 0x%X\n",
		//			i * FLASH_SECTOR_SIZE - NVT_FLASH_END_FLAG_LEN);
		/* check if there is end flag "NVT" at the end of this sector */
		if (memcmp(&fw_entry->data[i * FLASH_SECTOR_SIZE - NVT_FLASH_END_FLAG_LEN],
			"NVT", NVT_FLASH_END_FLAG_LEN) == 0) {
			nvt_spi_fw_need_write_size = i * FLASH_SECTOR_SIZE;
			NVT_LOG("fw_need_write_size = %zu(0x%zx), NVT end flag\n",
				nvt_spi_fw_need_write_size, nvt_spi_fw_need_write_size);
			return 0;
		}

		/* check if there is end flag "MOD" at the end of this sector */
		if (memcmp(&fw_entry->data[i * FLASH_SECTOR_SIZE - NVT_FLASH_END_FLAG_LEN],
			"MOD", NVT_FLASH_END_FLAG_LEN) == 0) {
			nvt_spi_fw_need_write_size = i * FLASH_SECTOR_SIZE;
			NVT_LOG("fw_need_write_size = %zu(0x%zx), MOD end flag\n",
				nvt_spi_fw_need_write_size, nvt_spi_fw_need_write_size);
			return 0;
		}
	}

	NVT_ERR("end flag \"NVT\" \"MOD\" not found!\n");
	return -EINVAL;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen init variable and allocate buffer
 * for download firmware function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
static int32_t nvt_spi_download_init(void)
{
	uint8_t *buf;
	/* allocate buffer for transfer firmware */
	//NVT_LOG("NVT_TRANSFER_LEN = 0x%06X\n", NVT_SPI_TRANSFER_LEN);

	if (nvt_spi_fwbuf == NULL) {
		buf = kzalloc((NVT_SPI_TRANSFER_LEN + 1 + NVT_SPI_DUMMY_BYTES), GFP_KERNEL);
		if (buf == NULL) {
			NVT_ERR("kzalloc for fwbuf failed!\n");
			return -ENOMEM;
		}
		nvt_spi_fwbuf = buf;
	}

	return 0;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen checksum function. Calculate bin
 * file checksum for comparison.
 *
 * return:
 *	n.a.
 ******************************************************
 */
static uint32_t CheckSum(const u8 *data, size_t len)
{
	uint32_t i = 0;
	uint32_t checksum = 0;

	for (i = 0 ; i < len + 1; i++)
		checksum += data[i];

	checksum += len;
	checksum = ~checksum + 1;

	return checksum;
}

static uint32_t byte_to_word(const uint8_t *data)
{
	return data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen parsing bin header function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
static uint32_t nvt_spi_partition;
static uint8_t nvt_spi_ilm_dlm_num = 2;
static uint8_t nvt_spi_cascade_2nd_header_info;
static int32_t nvt_spi_bin_header_parser(const u8 *fwdata, size_t fwsize)
{
	uint32_t list = 0;
	uint32_t pos = 0x00;
	uint32_t end = 0x00;
	uint8_t info_sec_num = 0;
	uint8_t ovly_sec_num = 0;
	uint8_t ovly_info = 0;
	uint8_t find_bin_header = 0;
	struct nvt_spi_bin_map_t *bin_map = NULL;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	/* Find the header size */
	end = fwdata[0] + (fwdata[1] << 8) + (fwdata[2] << 16) + (fwdata[3] << 24);

	/* check cascade next header */
	nvt_spi_cascade_2nd_header_info = (fwdata[0x20] & 0x02) >> 1;
	NVT_LOG("cascade_2nd_header_info = %d\n", nvt_spi_cascade_2nd_header_info);

	if (nvt_spi_cascade_2nd_header_info) {
		pos = 0x30;	// info section start at 0x30 offset
		while (pos < (end / 2)) {
			info_sec_num++;
			pos += 0x10;	/* each header info is 16 bytes */
		}

		info_sec_num = info_sec_num + 1; //next header section
	} else {
		pos = 0x30;	// info section start at 0x30 offset
		while (pos < end) {
			info_sec_num++;
			pos += 0x10;	/* each header info is 16 bytes */
		}
	}

	/*
	 * Find the DLM OVLY section
	 * [0:3] Overlay Section Number
	 * [4]   Overlay Info
	 */
	ovly_info = (fwdata[0x28] & 0x10) >> 4;
	ovly_sec_num = (ovly_info) ? (fwdata[0x28] & 0x0F) : 0;

	/*
	 * calculate all partition number
	 * ilm_dlm_num (ILM & DLM) + ovly_sec_num + info_sec_num
	 */
	nvt_spi_partition = nvt_spi_ilm_dlm_num + ovly_sec_num + info_sec_num;
	NVT_LOG("ovly_info=%d, ilm_dlm_num=%d, ovly_sec_num=%d, info_sec_num=%d, partition=%d\n",
		ovly_info, nvt_spi_ilm_dlm_num, ovly_sec_num, info_sec_num, nvt_spi_partition);


	/* allocated memory for header info */
	bin_map = kzalloc((nvt_spi_partition+1) * sizeof(struct nvt_spi_bin_map_t), GFP_KERNEL);
	if (bin_map == NULL) {
		NVT_ERR("kzalloc for bin_map failed!\n");
		return -ENOMEM;
	}
	nvt_spi_bin_map = bin_map;

	for (list = 0; list < nvt_spi_partition; list++) {
		/*
		 * [1] parsing ILM & DLM header info
		 * BIN_addr : SRAM_addr : size (12-bytes)
		 * crc located at 0x18 & 0x1C
		 */
		if (list < nvt_spi_ilm_dlm_num) {
			bin_map[list].BIN_addr = byte_to_word(&fwdata[0 + list * 12]);
			bin_map[list].SRAM_addr = byte_to_word(&fwdata[4 + list * 12]);
			bin_map[list].size = byte_to_word(&fwdata[8 + list * 12]);
			if (ts->hw_crc)
				bin_map[list].crc = byte_to_word(&fwdata[0x18 + list * 4]);
			else { //ts->hw_crc
				if ((bin_map[list].BIN_addr + bin_map[list].size) < fwsize)
					bin_map[list].crc = CheckSum(
							&fwdata[bin_map[list].BIN_addr],
							bin_map[list].size);
				else {
					NVT_ERR("access range (0x%08X to 0x%08X) is too large!\n",
						bin_map[list].BIN_addr,
						bin_map[list].BIN_addr + bin_map[list].size);
					return -EINVAL;
				}
			} //ts->hw_crc
			if (list == 0)
				snprintf(bin_map[list].name, sizeof(bin_map[list].name), "ILM");
			else if (list == 1)
				snprintf(bin_map[list].name, sizeof(bin_map[list].name), "DLM");
		}

		/*
		 * [2] parsing others header info
		 * SRAM_addr : size : BIN_addr : crc (16-bytes)
		 */
		if ((list >= nvt_spi_ilm_dlm_num)
			&& (list < (nvt_spi_ilm_dlm_num + info_sec_num))) {

			if (find_bin_header == 0) {
				/* others partition located at 0x30 offset */
				pos = 0x30 + (0x10 * (list - nvt_spi_ilm_dlm_num));
			} else if (find_bin_header && nvt_spi_cascade_2nd_header_info) {
				/* cascade 2nd header info */
				pos = end - 0x10;
			}

			bin_map[list].SRAM_addr = byte_to_word(&fwdata[pos]);
			bin_map[list].size = byte_to_word(&fwdata[pos + 4]);
			bin_map[list].BIN_addr = byte_to_word(&fwdata[pos + 8]);
			if (ts->hw_crc)
				bin_map[list].crc = byte_to_word(&fwdata[pos + 12]);
			else { //ts->hw_crc
				if ((bin_map[list].BIN_addr + bin_map[list].size) < fwsize)
					bin_map[list].crc = CheckSum(
							&fwdata[bin_map[list].BIN_addr],
							bin_map[list].size);
				else {
					NVT_ERR("access range (0x%08X to 0x%08X) is too large!\n",
						bin_map[list].BIN_addr,
						bin_map[list].BIN_addr + bin_map[list].size);
					return -EINVAL;
				}
			} //ts->hw_crc
			/* detect header end to protect parser function */
			if ((bin_map[list].BIN_addr < end) && (bin_map[list].size != 0)) {
				snprintf(bin_map[list].name, sizeof(bin_map[list].name),
					"Header");
				find_bin_header = 1;
			} else
				snprintf(bin_map[list].name, sizeof(bin_map[list].name),
					"Info-%d", (list - nvt_spi_ilm_dlm_num));
		}

		/*
		 * [3] parsing overlay section header info
		 * SRAM_addr : size : BIN_addr : crc (16-bytes)
		 */
		if (list >= (nvt_spi_ilm_dlm_num + info_sec_num)) {
			/* overlay info located at DLM (list = 1) start addr */
			pos = bin_map[1].BIN_addr;
			pos += (0x10 * (list - nvt_spi_ilm_dlm_num - info_sec_num));

			bin_map[list].SRAM_addr = byte_to_word(&fwdata[pos]);
			bin_map[list].size = byte_to_word(&fwdata[pos + 4]);
			bin_map[list].BIN_addr = byte_to_word(&fwdata[pos + 8]);
			if (ts->hw_crc)
				bin_map[list].crc = byte_to_word(&fwdata[pos + 12]);
			else { //ts->hw_crc
				if ((bin_map[list].BIN_addr + bin_map[list].size) < fwsize)
					bin_map[list].crc = CheckSum(
								&fwdata[bin_map[list].BIN_addr],
								bin_map[list].size);
				else {
					NVT_ERR("access range (0x%08X to 0x%08X) is too large!\n",
						bin_map[list].BIN_addr,
						bin_map[list].BIN_addr + bin_map[list].size);
					return -EINVAL;
				}
			} //ts->hw_crc
			snprintf(bin_map[list].name, sizeof(bin_map[list].name),
					"Overlay-%d",
					(list - nvt_spi_ilm_dlm_num - info_sec_num));
		}

		/* BIN size error detect */
		if ((bin_map[list].BIN_addr + bin_map[list].size) > fwsize) {
			NVT_ERR("access range (0x%08X to 0x%08X) is larger than bin size!\n",
					bin_map[list].BIN_addr,
					bin_map[list].BIN_addr + bin_map[list].size);
			return -EINVAL;
		}

//		NVT_LOG("[%d][%s] SRAM (0x%08X), SIZE (0x%08X), BIN (0x%08X), CRC (0x%08X)\n",
//				list, bin_map[list].name,
//				bin_map[list].SRAM_addr, bin_map[list].size,
//				bin_map[list].BIN_addr, bin_map[list].crc);
	}

	return 0;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen release update firmware function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
static void nvt_spi_update_firmware_release(void)
{
	if (nvt_spi_fw_entry)
		release_firmware(nvt_spi_fw_entry);

	nvt_spi_fw_entry = NULL;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen request update firmware function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -1,-22---failed.
 ******************************************************
 */
static int32_t nvt_spi_update_firmware_request(char *filename)
{
	uint8_t retry = 0;
	int32_t ret = 0;
	struct nvt_spi_data_t *ts = nvt_spi_data;
	uint8_t ver;

	if (filename == NULL)
		return -ENOENT;

	while (1) {
		NVT_LOG("filename is %s\n", filename);

		ret = request_firmware(&nvt_spi_fw_entry, filename, &ts->client->dev);
		if (ret) {
			NVT_ERR("firmware load failed, ret=%d\n", ret);
			goto request_fail;
		}

		// check FW need to write size
		if (nvt_spi_get_fw_need_write_size(nvt_spi_fw_entry)) {
			NVT_ERR("get fw need to write size fail!\n");
			ret = -EINVAL;
			goto invalid;
		}

		// check if FW version add FW version bar equals 0xFF
		ver = *(nvt_spi_fw_entry->data + FW_BIN_VER_OFFSET);
		if (ver + *(nvt_spi_fw_entry->data + FW_BIN_VER_BAR_OFFSET) != 0xFF) {
			NVT_ERR("bin file FW_VER + FW_VER_BAR should be 0xFF!\n");
			NVT_ERR("FW_VER=0x%02X, FW_VER_BAR=0x%02X\n",
					*(nvt_spi_fw_entry->data+FW_BIN_VER_OFFSET),
					*(nvt_spi_fw_entry->data+FW_BIN_VER_BAR_OFFSET));
			ret = -ENOEXEC;
			goto invalid;
		}

		/* BIN Header Parser */
		ret = nvt_spi_bin_header_parser(nvt_spi_fw_entry->data, nvt_spi_fw_entry->size);
		if (ret) {
			NVT_ERR("bin header parser failed\n");
			goto invalid;
		} else
			break;

invalid:
		nvt_spi_update_firmware_release();
		if (!IS_ERR_OR_NULL(nvt_spi_bin_map)) {
			kfree(nvt_spi_bin_map);
			nvt_spi_bin_map = NULL;
		}

request_fail:
		retry++;
		if (unlikely(retry > 2)) {
			NVT_ERR("error, retry=%d\n", retry);
			break;
		}
	}

	return ret;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen write data to sram function.
 *
 * - fwdata   : The buffer is written
 * - SRAM_addr: The sram destination address
 * - size     : Number of data bytes in @fwdata being written
 * - BIN_addr : The transferred data offset of @fwdata
 *
 * return:
 *	Executive outcomes. 0---succeed. else---fail.
 *******************************************************
 */
static int32_t nvt_spi_write_sram(const u8 *fwdata,
		uint32_t SRAM_addr, uint32_t size, uint32_t BIN_addr)
{
	int32_t ret = 0;
	uint32_t i = 0;
	uint16_t len = 0;
	int32_t count = 0;

	if (size % NVT_SPI_TRANSFER_LEN)
		count = (size / NVT_SPI_TRANSFER_LEN) + 1;
	else
		count = (size / NVT_SPI_TRANSFER_LEN);

	for (i = 0 ; i < count ; i++) {
		len = (size < NVT_SPI_TRANSFER_LEN) ? size : NVT_SPI_TRANSFER_LEN;

		//---set xdata index to start address of SRAM---
		ret = nvt_spi_set_page(SRAM_addr);
		if (ret) {
			NVT_ERR("set page failed, ret = %d\n", ret);
			return ret;
		}

		//---write data into SRAM---
		nvt_spi_fwbuf[0] = SRAM_addr & 0x7F;	//offset
		memcpy(nvt_spi_fwbuf+1, &fwdata[BIN_addr], len);	//payload
		ret = nvt_spi_write(nvt_spi_fwbuf, len+1);
		if (ret) {
			NVT_ERR("write to sram failed, ret = %d\n", ret);
			return ret;
		}

		SRAM_addr += NVT_SPI_TRANSFER_LEN;
		BIN_addr += NVT_SPI_TRANSFER_LEN;
		size -= NVT_SPI_TRANSFER_LEN;
	}

	return ret;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen nvt_spi_write_firmware function to write
 *	firmware into each partition.
 *
 * return:
 *	n.a.
 ******************************************************
 */
static int32_t nvt_spi_write_firmware(const u8 *fwdata, size_t fwsize)
{
	uint32_t list = 0;
	char *name;
	uint32_t BIN_addr, SRAM_addr, size;
	int32_t ret = 0;

	memset(nvt_spi_fwbuf, 0, (NVT_SPI_TRANSFER_LEN+1));

	for (list = 0; list < nvt_spi_partition; list++) {
		/* initialize variable */
		SRAM_addr = nvt_spi_bin_map[list].SRAM_addr;
		size = nvt_spi_bin_map[list].size;
		BIN_addr = nvt_spi_bin_map[list].BIN_addr;
		name = nvt_spi_bin_map[list].name;

//		NVT_LOG("[%d][%s] SRAM (0x%08X), SIZE (0x%08X), BIN (0x%08X)\n",
//				list, name, SRAM_addr, size, BIN_addr);

		/* Check data size */
		if ((BIN_addr + size) > fwsize) {
			NVT_ERR("access range (0x%08X to 0x%08X) is larger than bin size!\n",
					BIN_addr, BIN_addr + size);
			ret = -EINVAL;
			goto out;
		}

		/* ignore reserved partition (Reserved Partition size is zero) */
		if (!size)
			continue;
		else
			size = size + 1;

		/* write data to SRAM */
		ret = nvt_spi_write_sram(fwdata, SRAM_addr, size, BIN_addr);
		if (ret) {
			NVT_ERR("sram program failed, ret = %d\n", ret);
			goto out;
		}
	}

out:
	return ret;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen check checksum function.
 * This function will compare file checksum and fw checksum.
 *
 * return:
 *	n.a.
 *******************************************************
 */
static int32_t nvt_spi_check_fw_checksum(void)
{
	uint32_t fw_checksum = 0;
	uint32_t len = nvt_spi_partition * 4;
	uint32_t list = 0;
	int32_t ret = 0;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	memset(nvt_spi_fwbuf, 0, (len+1));

	//---set xdata index to checksum---
	nvt_spi_set_page(ts->mmap->R_ILM_CHECKSUM_ADDR);

	/* read checksum */
	nvt_spi_fwbuf[0] = (ts->mmap->R_ILM_CHECKSUM_ADDR) & 0x7F;
	ret = nvt_spi_read(nvt_spi_fwbuf, len+1);
	if (ret) {
		NVT_ERR("Read fw checksum failed\n");
		return ret;
	}

	/*
	 * Compare each checksum from fw
	 * ILM + DLM + Overlay + Info
	 * nvt_spi_ilm_dlm_num (ILM & DLM) + ovly_sec_num + info_sec_num
	 */
	for (list = 0; list < nvt_spi_partition; list++) {
		fw_checksum = byte_to_word(&nvt_spi_fwbuf[1+list*4]);

		/* ignore reserved partition (Reserved Partition size is zero) */
		if (!nvt_spi_bin_map[list].size)
			continue;

		if (nvt_spi_bin_map[list].crc != fw_checksum) {
			NVT_ERR("[%d] BIN_checksum=0x%08X, FW_checksum=0x%08X\n",
				list, nvt_spi_bin_map[list].crc, fw_checksum);
			ret = -EIO;
		}
	}

	return ret;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen set bootload crc reg bank function.
 * This function will set hw crc reg before enable crc function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
static void nvt_spi_set_bld_crc_bank(uint32_t DES_ADDR, uint32_t SRAM_ADDR,
		uint32_t LENGTH_ADDR, uint32_t size,
		uint32_t G_CHECKSUM_ADDR, uint32_t crc)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	/* write destination address */
	nvt_spi_set_page(DES_ADDR);
	nvt_spi_fwbuf[0] = DES_ADDR & 0x7F;
	nvt_spi_fwbuf[1] = (SRAM_ADDR) & 0xFF;
	nvt_spi_fwbuf[2] = (SRAM_ADDR >> 8) & 0xFF;
	nvt_spi_fwbuf[3] = (SRAM_ADDR >> 16) & 0xFF;
	nvt_spi_write(nvt_spi_fwbuf, 4);

	/* write length */
	//nvt_spi_set_page(LENGTH_ADDR);
	nvt_spi_fwbuf[0] = LENGTH_ADDR & 0x7F;
	nvt_spi_fwbuf[1] = (size) & 0xFF;
	nvt_spi_fwbuf[2] = (size >> 8) & 0xFF;
	nvt_spi_fwbuf[3] = (size >> 16) & 0x01;
	if (ts->hw_crc == 1)
		nvt_spi_write(nvt_spi_fwbuf, 3);
	else if (ts->hw_crc > 1)
		nvt_spi_write(nvt_spi_fwbuf, 4);

	/* write golden dlm checksum */
	//nvt_spi_set_page(G_CHECKSUM_ADDR);
	nvt_spi_fwbuf[0] = G_CHECKSUM_ADDR & 0x7F;
	nvt_spi_fwbuf[1] = (crc) & 0xFF;
	nvt_spi_fwbuf[2] = (crc >> 8) & 0xFF;
	nvt_spi_fwbuf[3] = (crc >> 16) & 0xFF;
	nvt_spi_fwbuf[4] = (crc >> 24) & 0xFF;
	nvt_spi_write(nvt_spi_fwbuf, 5);
}

/*
 *********************************************************
 * Description:
 *	Novatek touchscreen set BLD hw crc function.
 * This function will set ILM and DLM crc information to register.
 *
 * return:
 *	n.a.
 *********************************************************
 */
static void nvt_spi_set_bld_hw_crc(void)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	/* [0] ILM */
	/* write register bank */
	nvt_spi_set_bld_crc_bank(ts->mmap->ILM_DES_ADDR, nvt_spi_bin_map[0].SRAM_addr,
			ts->mmap->ILM_LENGTH_ADDR, nvt_spi_bin_map[0].size,
			ts->mmap->G_ILM_CHECKSUM_ADDR, nvt_spi_bin_map[0].crc);

	/* [1] DLM */
	/* write register bank */
	nvt_spi_set_bld_crc_bank(ts->mmap->DLM_DES_ADDR, nvt_spi_bin_map[1].SRAM_addr,
			ts->mmap->DLM_LENGTH_ADDR, nvt_spi_bin_map[1].size,
			ts->mmap->G_DLM_CHECKSUM_ADDR, nvt_spi_bin_map[1].crc);
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen read BLD hw crc info function.
 * This function will check crc results from register.
 *
 * return:
 *	n.a.
 ******************************************************
 */
static void nvt_spi_read_bld_hw_crc(void)
{
	uint8_t buf[8] = {0};
	uint32_t g_crc = 0, r_crc = 0;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	/* CRC Flag */
	nvt_spi_set_page(ts->mmap->BLD_ILM_DLM_CRC_ADDR);
	buf[0] = ts->mmap->BLD_ILM_DLM_CRC_ADDR & 0x7F;
	buf[1] = 0x00;
	nvt_spi_read(buf, 2);
	NVT_ERR("crc_done = %d, ilm_crc_flag = %d, dlm_crc_flag = %d\n",
			(buf[1] >> 2) & 0x01, (buf[1] >> 0) & 0x01, (buf[1] >> 1) & 0x01);

	/* ILM CRC */
	nvt_spi_set_page(ts->mmap->G_ILM_CHECKSUM_ADDR);
	buf[0] = ts->mmap->G_ILM_CHECKSUM_ADDR & 0x7F;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;
	nvt_spi_read(buf, 5);
	g_crc = buf[1] | (buf[2] << 8) | (buf[3] << 16) | (buf[4] << 24);

	nvt_spi_set_page(ts->mmap->R_ILM_CHECKSUM_ADDR);
	buf[0] = ts->mmap->R_ILM_CHECKSUM_ADDR & 0x7F;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;
	nvt_spi_read(buf, 5);
	r_crc = buf[1] | (buf[2] << 8) | (buf[3] << 16) | (buf[4] << 24);

	NVT_ERR("ilm: bin crc = 0x%08X, golden = 0x%08X, result = 0x%08X\n",
			nvt_spi_bin_map[0].crc, g_crc, r_crc);

	/* DLM CRC */
	nvt_spi_set_page(ts->mmap->G_DLM_CHECKSUM_ADDR);
	buf[0] = ts->mmap->G_DLM_CHECKSUM_ADDR & 0x7F;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;
	nvt_spi_read(buf, 5);
	g_crc = buf[1] | (buf[2] << 8) | (buf[3] << 16) | (buf[4] << 24);

	nvt_spi_set_page(ts->mmap->R_DLM_CHECKSUM_ADDR);
	buf[0] = ts->mmap->R_DLM_CHECKSUM_ADDR & 0x7F;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;
	nvt_spi_read(buf, 5);
	r_crc = buf[1] | (buf[2] << 8) | (buf[3] << 16) | (buf[4] << 24);

	NVT_ERR("dlm: bin crc = 0x%08X, golden = 0x%08X, result = 0x%08X\n",
			nvt_spi_bin_map[1].crc, g_crc, r_crc);

}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen Download_Firmware with HW CRC
 * function. It's complete download firmware flow.
 *
 * return:
 *	Executive outcomes. 0---succeed. else---fail.
 ******************************************************
 */
static int32_t nvt_spi_download_firmware_hw_crc(void)
{
	uint8_t retry = 0;
	int32_t ret = 0;
	const struct firmware *fw = nvt_spi_fw_entry;

	nvt_spi_start = ktime_get();

	while (1) {
		/* bootloader reset to reset MCU */
		nvt_spi_bootloader_reset();

		/* set ilm & dlm reg bank */
		nvt_spi_set_bld_hw_crc();

		/* Start to write firmware process */
		if (nvt_spi_cascade_2nd_header_info) {
			/* for cascade */
			nvt_spi_tx_auto_copy_mode();

			ret = nvt_spi_write_firmware(fw->data, fw->size);
			if (ret) {
				NVT_ERR("Write_Firmware failed. (%d)\n", ret);
				goto fail;
			}

			ret = nvt_spi_check_spi_dma_tx_info();
			if (ret) {
				NVT_ERR("spi dma tx info failed. (%d)\n", ret);
				goto fail;
			}
		} else {
			ret = nvt_spi_write_firmware(fw->data, fw->size);
			if (ret) {
				NVT_ERR("Write_Firmware failed. (%d)\n", ret);
				goto fail;
			}
		}

#if NVT_DUMP_PARTITION
		ret = nvt_dump_partition();
		if (ret)
			NVT_ERR("nvt_dump_partition failed, ret = %d\n", ret);
#endif

		/* enable hw bld crc function */
		nvt_spi_bld_crc_enable();

		/* clear fw reset status & enable fw crc check */
		nvt_spi_fw_crc_enable();

		/* Set Boot Ready Bit */
		nvt_spi_boot_ready();

		ret = nvt_spi_check_fw_reset_state(NVT_SPI_RESET_STATE_INIT);
		if (ret) {
			NVT_ERR("nvt_check_fw_reset_state failed. (%d)\n", ret);
			goto fail;
		} else {
			break;
		}

fail:
		retry++;
		if (unlikely(retry > 2)) {
			NVT_ERR("error, retry=%d\n", retry);
			nvt_spi_read_bld_hw_crc();
			break;
		}
	}

	nvt_spi_end = ktime_get();

	return ret;
}

/*
 ********************************************************
 * Description:
 *	Novatek touchscreen Download_Firmware function. It's
 * complete download firmware flow.
 *
 * return:
 *	n.a.
 ******************************************************
 */
static int32_t nvt_spi_download_firmware(void)
{
	uint32_t addr;
	uint8_t retry = 0;
	int32_t ret = 0;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	nvt_spi_start = ktime_get();

	while (1) {
		/*
		 * Send eng reset cmd before download FW
		 * Keep TP_RESX low when send eng reset cmd
		 */
#if NVT_SPI_TOUCH_SUPPORT_HW_RST
		gpio_set_value(ts->reset_gpio, 0);
		mdelay(1);	//wait 1ms
#endif
		nvt_spi_eng_reset();
#if NVT_SPI_TOUCH_SUPPORT_HW_RST
		gpio_set_value(ts->reset_gpio, 1);
		mdelay(10);	//wait tRT2BRST after TP_RST
#endif
		nvt_spi_bootloader_reset();

		addr = ts->mmap->EVENT_BUF_ADDR;
		/* clear fw reset status */
		nvt_spi_write_addr(addr | NVT_SPI_EVENT_MAP_RESET_COMPLETE, 0x00);

		/* Start to write firmware process */
		ret = nvt_spi_write_firmware(nvt_spi_fw_entry->data, nvt_spi_fw_entry->size);
		if (ret) {
			NVT_ERR("Write_Firmware failed. (%d)\n", ret);
			goto fail;
		}

#if NVT_DUMP_PARTITION
		ret = nvt_dump_partition();
		if (ret)
			NVT_ERR("nvt_dump_partition failed, ret = %d\n", ret);
#endif

		/* Set Boot Ready Bit */
		nvt_spi_boot_ready();

		ret = nvt_spi_check_fw_reset_state(NVT_SPI_RESET_STATE_INIT);
		if (ret) {
			NVT_ERR("nvt_check_fw_reset_state failed. (%d)\n", ret);
			goto fail;
		}

		/* check fw checksum result */
		ret = nvt_spi_check_fw_checksum();
		if (ret) {
			NVT_ERR("firmware checksum not match, retry=%d\n", retry);
			goto fail;
		} else
			break;

fail:
		retry++;
		if (unlikely(retry > 2)) {
			NVT_ERR("error, retry=%d\n", retry);
			break;
		}
	}

	nvt_spi_end = ktime_get();

	return ret;
}

/*
 ******************************************************
 * Description:
 *	Novatek touchscreen update firmware main function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
int32_t nvt_spi_update_firmware(char *firmware_name)
{
	int32_t ret = 0;
	struct nvt_spi_data_t *ts = nvt_spi_data;

	// request bin file in "/etc/firmware"
	ret = nvt_spi_update_firmware_request(firmware_name);
	if (ret) {
		NVT_ERR("update_firmware_request failed. (%d)\n", ret);
		goto request_firmware_fail;
	}

	/* initial buffer and variable */
	ret = nvt_spi_download_init();
	if (ret) {
		NVT_ERR("Download Init failed. (%d)\n", ret);
		goto download_fail;
	}

	/* download firmware process */
	if (ts->hw_crc)
		ret = nvt_spi_download_firmware_hw_crc();
	else
		ret = nvt_spi_download_firmware();
	if (ret) {
		NVT_ERR("Download Firmware failed. (%d)\n", ret);
		goto download_fail;
	}

	NVT_LOG("Update firmware success! <%ld us>\n",
			(long) ktime_us_delta(nvt_spi_end, nvt_spi_start));

	/* Get FW Info */
	ret = nvt_spi_get_fw_info();
	if (ret)
		NVT_ERR("nvt_get_fw_info failed. (%d)\n", ret);

download_fail:
	if (!IS_ERR_OR_NULL(nvt_spi_bin_map)) {
		kfree(nvt_spi_bin_map);
		nvt_spi_bin_map = NULL;
	}

	nvt_spi_update_firmware_release();
request_firmware_fail:

	return ret;
}

/*
 *******************************************************
 * Description:
 *	Novatek touchscreen update firmware when booting
 *	function.
 *
 * return:
 *	n.a.
 ******************************************************
 */
void nvt_spi_update_firmware_work(struct work_struct *work)
{
	struct nvt_spi_data_t *ts = nvt_spi_data;

	mutex_lock(&ts->lock);
	nvt_spi_update_firmware(NVT_SPI_BOOT_UPDATE_FIRMWARE_NAME);
	mutex_unlock(&ts->lock);
}
#endif

#endif
