/* drivers/input/touchscreen/raydium_wt030/raydium_fw_update.c
 *
 * Raydium TouchScreen driver.
 *
 * Copyright (c) 2010  Raydium tech Ltd.
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

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/device.h>
#include <asm/traps.h>
#include <linux/firmware.h>
#include "raydium_driver.h"
#if defined(FW_MAPPING_EN)
#include "rad_fw_image_21.h"
#endif

#ifdef FW_UPDATE_EN

#ifdef ENABLE_FLASHLOG_BACKUP
unsigned char raydium_turn_on_flash_2X(struct i2c_client *client)
{
	unsigned int u32_read = 0;
	unsigned char u8_buf[4];

	/*Turn on Flash*/
	memset(u8_buf, 0,  sizeof(u8_buf));
	if (raydium_i2c_pda_write(client, 0x50000624, u8_buf, 4) == ERROR)
		return ERROR;

	u8_buf[0] = 0x20;
	if (raydium_i2c_pda_write(client, 0x50000914, u8_buf, 4) == ERROR)
		return ERROR;

	return SUCCESS;
}

unsigned char  raydium_read_fpc_flash(struct i2c_client *client,
			unsigned int u32_addr, unsigned char *u8_r_data)
{
	unsigned int u32_read;
	unsigned char u8_buf[4];

	pr_debug("[touch]%s\n", __func__);

	if (raydium_i2c_pda_read(client, 0x40000000, u8_buf, 4) == ERROR)
		return ERROR;

	u8_buf[0] |= 0x40;
	if (raydium_i2c_pda_write(client, 0x40000000, u8_buf, 4) == ERROR)
		return ERROR;

	memset(u8_buf, 0,  sizeof(u8_buf));
	if (raydium_i2c_pda_write(client, 0x50000624, u8_buf, 4) == ERROR)
		return ERROR;

	u8_buf[0] =  (u32_addr & 0x000000FF);
	u8_buf[1] = ((u32_addr & 0x0000FF00) >> 8);
	u8_buf[2] = ((u32_addr & 0x00FF0000) >> 16);
	u8_buf[3] = ((u32_addr & 0xFF000000) >> 24);

	if (raydium_i2c_pda_write(client, 0x50000910, u8_buf, 4) == ERROR)
		return ERROR;

	memset(u8_buf, 0,  sizeof(u8_buf));
	u8_buf[0] = 0x40;

	if (raydium_i2c_pda_write(client, 0x50000914, u8_buf, 4) == ERROR)
		return ERROR;

	usleep_range(950, 1050);

	if (raydium_i2c_pda_read(client, 0x5000093C, u8_r_data, 4) == ERROR)
		return ERROR;

	return SUCCESS;
}

unsigned char raydium_read_flash_log(void)
{
	unsigned char u8_buf[4];
	unsigned int u32_readbuf;
	unsigned char u8_ret = 0;
	unsigned char u8_logcount = 0;
	unsigned int u32_temp = 0;
	unsigned char u8_i = 0, u8_j = 0;

	raydium_i2c_pda2_set_page(g_raydium_ts->client,
		g_raydium_ts->is_suspend, RAYDIUM_PDA2_2_PDA);
	g_u8_i2c_mode = PDA_MODE;
	pr_debug("[touch]Disable PDA2_MODE\n");

	if ((g_raydium_ts->id  & 0x2000) != 0)
		raydium_turn_on_flash_2X(g_raydium_ts->client);

	for (u8_i = 0; u8_i < 4; u8_i++) {
		u32_temp = (0x9000 +  (u8_i*4));
		raydium_read_fpc_flash(g_raydium_ts->client, u32_temp,
						&u32_readbuf);
		if (u8_i == 0 && u32_readbuf == 0xFFFFFFFF) {
			pr_err("[touch]Raydium flash no log\n");
			return FAIL;
		}
		pr_debug("[touch]Raydium flash 0x%x = 0x%x\n",
			u32_temp, u32_readbuf);
		u32_readbuf = u32_readbuf & (~u32_readbuf + 1);
		pr_debug("[touch]Raydium flash reverse = 0x%x\n",
			u32_readbuf);
		u32_temp = 1;
		u8_j = 0;
		while (u32_readbuf != u32_temp) {
			u8_j++;
			u32_temp <<= 1;
			if (u8_j == 32)
				break;
		}
		if (u8_i == 0) {
			if ((u8_j > 0) && (u8_j < 32)) {
				u8_logcount = u8_i*32 + u8_j;
				pr_debug("[touch]logcount = Log%d\n",
						(u8_logcount - 1));
				break;
			}
		} else {
			if (u8_j < 32) {
				u8_logcount = u8_i*32 + u8_j;
				pr_debug("[touch]logcount = Log%d\n",
						(u8_logcount - 1));
				break;
			}
		}
	}

	if (u8_logcount != 0) {
		u32_temp = (0x9014 + (u8_logcount-1) * 48);
		raydium_read_fpc_flash(g_raydium_ts->client, u32_temp, u8_buf);
		pr_info("[touch]Rad log fw version 0x%x.0x%x.0x%x.0x%x\n",
				u8_buf[0], u8_buf[1], u8_buf[2], u8_buf[3]);
		if ((g_raydium_ts->id  & 0x2000) != 0)
			g_raydium_ts->id = 0x2000 | ((u8_buf[0] & 0xF) << 8)
						| u8_buf[1];

		return SUCCESS;
	}
	return FAIL;
}
#endif
unsigned char raydium_mem_table_init(unsigned short u16_id)
{
	unsigned int u8_ret = 0;

	pr_info("[touch]Raydium table init 0x%x\n", u16_id);

	if ((u16_id & 0x2000) != 0) {
		g_rad_boot_image = kzalloc(RAD_BOOT_2X_SIZE, GFP_KERNEL);
		g_rad_init_image = kzalloc(RAD_INIT_2X_SIZE, GFP_KERNEL);
		g_rad_fw_image = kzalloc(RAD_FW_2X_SIZE, GFP_KERNEL);
		g_rad_para_image = kzalloc(RAD_PARA_2X_SIZE + 4, GFP_KERNEL);
		g_rad_testfw_image = kzalloc(RAD_TESTFW_2X_SIZE, GFP_KERNEL);
		g_rad_testpara_image = kzalloc(RAD_PARA_2X_SIZE + 4,
								GFP_KERNEL);
		g_u8_table_init = SUCCESS;
		u8_ret = SUCCESS;
	}

	return u8_ret;
}

unsigned char raydium_mem_table_setting(void)
{
	unsigned char u8_ret = SUCCESS;
#ifdef ENABLE_FLASHLOG_BACKUP
	unsigned char u8_buf[4];
	static unsigned char u8_readflash;
#endif

	pr_info("[touch]Raydium ID is 0x%x\n", g_raydium_ts->id);
	switch (g_raydium_ts->id) {
	case RAD_20:
		memcpy(g_rad_boot_image, u8_rad_boot_20, RAD_BOOT_2X_SIZE);
		memcpy(g_rad_init_image, u8_rad_init_20, RAD_INIT_2X_SIZE);
		memcpy(g_rad_fw_image, u8_rad_fw_20, RAD_FW_2X_SIZE);
		memcpy(g_rad_testfw_image, u8_rad_testfw_20, RAD_FW_2X_SIZE);
		memcpy(g_rad_testfw_image + RAD_FW_2X_SIZE,
			u8_rad_testpara_20, RAD_PARA_2X_SIZE + 4);
		if (g_rad_boot_image[0x82] >= 4) {
			memcpy(g_rad_para_image,
				u8_rad_para_20,
				RAD_PARA_2X_SIZE + 4);
			memcpy(g_rad_testpara_image,
				u8_rad_testpara_20,
				RAD_PARA_2X_SIZE + 4);
		} else {
			memcpy(g_rad_para_image,
				u8_rad_para_20,
				RAD_PARA_2X_SIZE);
			memcpy(g_rad_testpara_image,
				u8_rad_testpara_20,
				RAD_PARA_2X_SIZE);
		}
		break;
#if defined(FW_MAPPING_EN)
	case RAD_21:
		memcpy(g_rad_boot_image, u8_rad_boot_21, RAD_BOOT_2X_SIZE);
		memcpy(g_rad_init_image, u8_rad_init_21, RAD_INIT_2X_SIZE);
		memcpy(g_rad_fw_image, u8_rad_fw_21, RAD_FW_2X_SIZE);
		memcpy(g_rad_testfw_image, u8_rad_testfw_21, RAD_FW_2X_SIZE);
		memcpy(g_rad_testfw_image + RAD_FW_2X_SIZE,
			u8_rad_testpara_21,
			RAD_PARA_2X_SIZE + 4);
		if (g_rad_boot_image[0x82] >= 4) {
			memcpy(g_rad_para_image,
				u8_rad_para_21,
				RAD_PARA_2X_SIZE + 4);
			memcpy(g_rad_testpara_image,
				u8_rad_testpara_21,
				RAD_PARA_2X_SIZE + 4);
		} else {
			memcpy(g_rad_para_image,
				u8_rad_para_21,
				RAD_PARA_2X_SIZE);
			memcpy(g_rad_testpara_image,
				u8_rad_testpara_21,
				RAD_PARA_2X_SIZE);
		}
		break;
#endif
	default:
		pr_info("[touch]mapping ic setting use default fw\n");
#ifdef ENABLE_FLASHLOG_BACKUP
		if (!u8_readflash) {
			u8_ret = raydium_read_flash_log();
			u8_readflash = true;

			raydium_i2c_pda_read(g_raydium_ts->client,
					RAD_PDA2_CTRL_CMD,
					u8_buf,
					4);
			u8_buf[0] |= RAD_ENABLE_PDA2 | RAD_ENABLE_SI2;
			raydium_i2c_pda_write(g_raydium_ts->client,
					RAD_PDA2_CTRL_CMD,
					u8_buf,
					4);
			raydium_i2c_pda_set_address(0x50000628, DISABLE);

			g_u8_i2c_mode = PDA2_MODE;
			pr_debug("[touch]Enable PDA2_MODE\n");
			raydium_mem_table_setting();
		} else {
			if ((g_raydium_ts->id & 0x2000) != 0) {
				memcpy(g_rad_boot_image,
					u8_rad_boot_20,
					RAD_BOOT_2X_SIZE);
				memcpy(g_rad_init_image,
					u8_rad_init_20,
					RAD_INIT_2X_SIZE);
				memcpy(g_rad_fw_image,
					u8_rad_fw_20,
					RAD_FW_2X_SIZE);
				memcpy(g_rad_testfw_image,
					u8_rad_testfw_20,
					RAD_FW_2X_SIZE);
				memcpy(g_rad_testfw_image + RAD_FW_2X_SIZE,
					u8_rad_testpara_20,
					RAD_PARA_2X_SIZE + 4);
				if (g_rad_boot_image[0x82] >= 4) {
					memcpy(g_rad_para_image,
						u8_rad_para_20,
						RAD_PARA_2X_SIZE + 4);
					memcpy(g_rad_testpara_image,
						u8_rad_testpara_20,
						RAD_PARA_2X_SIZE + 4);
				} else {
					memcpy(g_rad_para_image,
						u8_rad_para_20,
						RAD_PARA_2X_SIZE);
					memcpy(g_rad_testpara_image,
						u8_rad_testpara_20,
						RAD_PARA_2X_SIZE);
				}
				g_raydium_ts->id = RAD_20;
			}
		}
		u8_ret = SUCCESS;
		break;
#else
		if ((g_raydium_ts->id & 0x2000) != 0) {
			memcpy(g_rad_boot_image,
				u8_rad_boot_20,
				RAD_BOOT_2X_SIZE);
			memcpy(g_rad_init_image,
				u8_rad_init_20,
				RAD_INIT_2X_SIZE);
			memcpy(g_rad_fw_image,
				u8_rad_fw_20,
				RAD_FW_2X_SIZE);
			memcpy(g_rad_testfw_image,
				u8_rad_testfw_20,
				RAD_FW_2X_SIZE);
			memcpy(g_rad_testfw_image + RAD_FW_2X_SIZE,
				u8_rad_testpara_20,
				RAD_PARA_2X_SIZE + 4);
			if (g_rad_boot_image[0x82] >= 4) {
				memcpy(g_rad_para_image,
					u8_rad_para_20,
					RAD_PARA_2X_SIZE + 4);
				memcpy(g_rad_testpara_image,
					u8_rad_testpara_20,
					RAD_PARA_2X_SIZE + 4);
			} else {
				memcpy(g_rad_para_image,
					u8_rad_para_20,
					RAD_PARA_2X_SIZE);
				memcpy(g_rad_testpara_image,
					u8_rad_testpara_20,
					RAD_PARA_2X_SIZE);
			}
			g_raydium_ts->id = RAD_20;
		}
		u8_ret = SUCCESS;
		break;
#endif
	}

	g_u8_table_setting = 0;
	return u8_ret;
}

unsigned char raydium_id_init(unsigned char u8_type)
{
	unsigned int u8_ret = 0;

	switch (u8_type) {
	case 0:
		g_raydium_ts->id = RAD_20;
		u8_ret = SUCCESS;
		break;
#if defined(FW_MAPPING_EN)
	case 1:
		g_raydium_ts->id = RAD_21;
		u8_ret = SUCCESS;
		break;
#endif
	}


	return u8_ret;
}

static unsigned int bits_reverse(unsigned int u32_num, unsigned int bit_num)
{
	unsigned int reverse = 0, u32_i;

	for (u32_i = 0; u32_i < bit_num; u32_i++) {
		if (u32_num & (1 << u32_i))
			reverse |= 1 << ((bit_num - 1) - u32_i);
	}
	return reverse;
}

static unsigned int rc_crc32(const char *buf, unsigned int u32_len,
			     unsigned int u32_crc)
{
	unsigned int u32_i;
	unsigned char u8_flash_byte, u8_current, u8_j;

	for (u32_i = 0; u32_i < u32_len; u32_i++) {
		u8_flash_byte = buf[u32_i];
		u8_current = (unsigned char)bits_reverse(u8_flash_byte, 8);
		for (u8_j = 0; u8_j < 8; u8_j++) {
			if ((u32_crc ^ u8_current) & 0x01)
				u32_crc = (u32_crc >> 1) ^ 0xedb88320;
			else
				u32_crc >>= 1;
			u8_current >>= 1;
		}
	}
	return u32_crc;
}

int wait_fw_state(struct i2c_client *client, unsigned int u32_addr,
			 unsigned int u32_state, unsigned long u32_delay_us,
			 unsigned short u16_retry)
{
	unsigned char u8_buf[4];
	unsigned int u32_read_data;
	unsigned int u32_min_delay_us = u32_delay_us - 500;
	unsigned int u32_max_delay_us = u32_delay_us + 500;

	do {
		if (raydium_i2c_pda_read(client, u32_addr, u8_buf, 4) == ERROR)
			return ERROR;

		memcpy(&u32_read_data, u8_buf, 4);
		u16_retry--;
		usleep_range(u32_min_delay_us, u32_max_delay_us);
	} while ((u32_read_data != u32_state) && (u16_retry != 0));

	if (u32_read_data != u32_state) {
		pr_err("[touch]confirm data error : 0x%x\n", u32_read_data);
		return ERROR;
	}

	return SUCCESS;
}

int raydium_do_software_reset(struct i2c_client *client)
{
	int i32_ret = SUCCESS;

	unsigned char u8_buf[4];

	/*SW reset*/
	g_u8_resetflag = true;
	memset(u8_buf, 0, sizeof(u8_buf));
	u8_buf[0] = 0x01;
	pr_info("[touch]SW reset\n");
	i32_ret = raydium_i2c_pda_write(client, 0x40000004, u8_buf, 4);
	if (i32_ret < 0)
		goto exit;

	if ((g_raydium_ts->id & 0x2000) != 0)
		msleep(25);
exit:
	return i32_ret;
}

static int raydium_check_fw_ready(struct i2c_client *client)
{
	int i32_ret = SUCCESS;
	unsigned int u32_retry = 400;
	unsigned char u8_buf[4];

	u8_buf[1] = 0;
	while (u8_buf[1] != 0x40 && u32_retry != 0) {
		i32_ret = raydium_i2c_pda_read(client, 0x50000918, u8_buf, 4);
		if (i32_ret < 0)
			goto exit;

		u32_retry--;
		usleep_range(4500, 5500);
	}

	if (u32_retry == 0) {
		pr_err("[touch]%s, FW not ready, retry error!\n", __func__);
		i32_ret = ERROR;
	} else {
		pr_info("[touch]%s, FW is ready!!\n", __func__);
		usleep_range(4500, 5500);
	}

exit:
	return i32_ret;
}

int set_skip_load(struct i2c_client *client)
{
	int i32_ret = SUCCESS;
	unsigned int u32_retry_time = 1000;
	unsigned char u8_buf[4];

	/*Skip load*/
	memset(u8_buf, 0, sizeof(u8_buf));
	u8_buf[0] = 0x10;
	u8_buf[1] = 0x08;
	i32_ret = raydium_i2c_pda_write(client, 0x50000918, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;
	i32_ret = raydium_do_software_reset(client);
	if (i32_ret < 0)
		pr_err("[touch]%s, SW reset error!\n", __func__);

	i32_ret = wait_fw_state(client, 0x20000214, 0x82, 2000, u32_retry_time);
	if (i32_ret < 0)
		pr_err("[touch]%s, wait_fw_state error!\n", __func__);

exit_upgrade:
	return i32_ret;
}

/*check pram crc32*/
static int raydium_check_pram_crc_2X(struct i2c_client *client,
		unsigned int u32_addr,
		unsigned int u32_len)
{
	int i32_ret = SUCCESS;
	unsigned int u32_crc_addr = u32_addr + u32_len;
	unsigned int u32_end_addr = u32_crc_addr - 1;
	unsigned int u32_crc_result, u32_read_data;
	unsigned int u32_retry = 400;
	unsigned char u8_buf[4], u8_retry = 3;

	memset(u8_buf, 0, sizeof(u8_buf));
	u8_buf[0] = (unsigned char)(u32_addr & 0xFF);
	u8_buf[1] = (unsigned char)((u32_addr & 0xFF00) >> 8);
	u8_buf[2] = (unsigned char)(u32_end_addr & 0xFF);
	u8_buf[3] = (unsigned char)((u32_end_addr & 0xFF00) >> 8);

	i32_ret = raydium_i2c_pda_write(client, 0x50000974, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;
	i32_ret = raydium_i2c_pda_read(client, 0x5000094C, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;
	u8_buf[3] |= 0x81;
	i32_ret = raydium_i2c_pda_write(client, 0x5000094C, u8_buf, 4);

	while (u8_buf[3] != 0x80 && u32_retry != 0) {
		i32_ret = raydium_i2c_pda_read(client, 0x5000094C, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		u32_retry--;
		usleep_range(4500, 5500);
	}
	if (u32_retry == 0) {
		pr_err("[touch]%s, Cal CRC not ready, retry error!\n",
			__func__);
		i32_ret = ERROR;
	}

	i32_ret = raydium_i2c_pda_read(client, 0x50000978, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;
	memcpy(&u32_crc_result, u8_buf, 4);
	i32_ret = raydium_i2c_pda_read(client, u32_crc_addr, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;
	memcpy(&u32_read_data, u8_buf, 4);

	while (u32_read_data != u32_crc_result && u8_retry > 0) {
		i32_ret = raydium_i2c_pda_read(client, 0x50000978, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;
		memcpy(&u32_crc_result, u8_buf, 4);
		usleep_range(1500, 2500);
		u8_retry--;
	}
	if (u32_read_data != u32_crc_result) {
		pr_err("[touch]check pram crc fail!!\n");
		pr_err("[touch]u32_read_data 0x%x\n", u32_read_data);
		pr_err("[touch]u32_crc_result 0x%x\n", u32_crc_result);
		i32_ret = ERROR;
		goto exit_upgrade;
	} else if (u8_retry != 3) {
		pr_err("[touch]check pram crc pass!!\n");
		pr_err("[touch]u8_retry : %d\n", u8_retry);
		pr_err("[touch]u32_read_data 0x%x\n", u32_read_data);
		pr_err("[touch]u32_crc_result 0x%x\n", u32_crc_result);
		i32_ret = ERROR;
		goto exit_upgrade;
	}

exit_upgrade:
	return i32_ret;
}

/* upgrade firmware with image file */
static int raydium_write_to_pram_2X(struct i2c_client *client,
		unsigned int u32_fw_addr,
		unsigned char u8_type)
{
	int i32_ret = ERROR;
	unsigned int u32_fw_size = 0;
	unsigned char *p_u8_firmware_data = NULL;
	unsigned int u32_write_offset = 0;
	unsigned short u16_write_length = 0;

	switch (u8_type) {
	case RAYDIUM_INIT:
		u32_fw_size = 0x200;
		p_u8_firmware_data = g_rad_init_image;
		break;

	case RAYDIUM_PARA:
		u32_fw_size = 0x160;
		p_u8_firmware_data = g_rad_para_image;
		break;

	case RAYDIUM_FIRMWARE:
		u32_fw_size = 0x6200;
		p_u8_firmware_data = g_rad_fw_image;
		break;

	case RAYDIUM_BOOTLOADER:
		u32_fw_size = 0x800;
		p_u8_firmware_data = g_rad_boot_image;
		break;

	case RAYDIUM_TEST_FW:
		u32_fw_size = 0x6360;
		p_u8_firmware_data = g_rad_testfw_image;
		break;
	}

	u32_write_offset = 0;
	while (u32_write_offset < u32_fw_size) {
		if ((u32_write_offset + MAX_WRITE_PACKET_SIZE) < u32_fw_size)
			u16_write_length = MAX_WRITE_PACKET_SIZE;
		else
			u16_write_length =
			(unsigned short)(u32_fw_size - u32_write_offset);

		i32_ret = raydium_i2c_pda_write(
			      client,
			      (u32_fw_addr + u32_write_offset),
			      (p_u8_firmware_data + u32_write_offset),
			      u16_write_length);
		if (i32_ret < 0)
			goto exit_upgrate;

		u32_write_offset += (unsigned long)u16_write_length;
	}
	u32_fw_addr += u32_write_offset;

exit_upgrate:
	if (i32_ret < 0) {
		pr_err("[touch]upgrade failed\n");
		return i32_ret;
	}
	pr_info("[touch]upgrade success\n");
	return 0;
}

/* upgrade firmware with image file */
static int raydium_fw_upgrade_with_image(struct i2c_client *client,
		unsigned int u32_fw_addr,
		unsigned char u8_type)
{
	int i32_ret = ERROR;
	unsigned int u32_fw_size = 0;
	unsigned char *p_u8_firmware_data = NULL;
	unsigned int u32_write_offset = 0;
	unsigned short u16_write_length = 0;
	unsigned int u32_checksum = 0xFFFFFFFF;

	switch (u8_type) {
	case RAYDIUM_INIT:
		u32_fw_size = 0x1fc;
		p_u8_firmware_data = g_rad_init_image;
		break;
	case RAYDIUM_PARA:
		if ((g_raydium_ts->id & 0x2000) != 0)
			u32_fw_size = 0x158;
		p_u8_firmware_data = g_rad_para_image;
		break;
	case RAYDIUM_FIRMWARE:
		if ((g_raydium_ts->id & 0x2000) != 0)
			u32_fw_size = 0x61fc;
		p_u8_firmware_data = g_rad_fw_image;
		break;
	case RAYDIUM_BOOTLOADER:
		if ((g_raydium_ts->id & 0x2000) != 0)
			u32_fw_size = 0x7FC;
		p_u8_firmware_data = g_rad_boot_image;
		break;
	case RAYDIUM_TEST_FW:
		if ((g_raydium_ts->id & 0x2000) != 0)
			u32_fw_size = 0x635C;
		p_u8_firmware_data = g_rad_testfw_image;
		break;
	}

	pr_debug("[touch]CRC 0x%08X\n",
		*(unsigned int *)(p_u8_firmware_data + u32_fw_size));

	u32_checksum = rc_crc32(p_u8_firmware_data,
		u32_fw_size, u32_checksum);
	u32_checksum = bits_reverse(u32_checksum, 32);
	memcpy((p_u8_firmware_data + u32_fw_size), &u32_checksum, 4);
	pr_debug("[touch]CRC result 0x%08X\n", u32_checksum);
	u32_fw_size += 4;

	u32_write_offset = 0;
	while (u32_write_offset < u32_fw_size) {
		if ((u32_write_offset + MAX_WRITE_PACKET_SIZE) < u32_fw_size)
			u16_write_length = MAX_WRITE_PACKET_SIZE;
		else
			u16_write_length =
				(unsigned short)
				(u32_fw_size - u32_write_offset);

		i32_ret = raydium_i2c_pda_write(
			      client,
			      (u32_fw_addr + u32_write_offset),
			      (p_u8_firmware_data + u32_write_offset),
			      u16_write_length);
		if (i32_ret < 0)
			goto exit_upgrate;

		u32_write_offset += (unsigned long)u16_write_length;
	}
	u32_fw_addr += u32_write_offset;

exit_upgrate:
	if (i32_ret < 0) {
		pr_err("[touch]upgrade failed\n");
		return i32_ret;
	}
	pr_info("[touch]upgrade success\n");
	return 0;
}
static int raydium_boot_upgrade_2X(struct i2c_client *client)
{
	int i32_ret = SUCCESS;
	unsigned char u8_buf[4];

	/*set mcu hold*/
	memset(u8_buf, 0, sizeof(u8_buf));
	u8_buf[0] = 0x20;
	i32_ret = raydium_i2c_pda_write(client, 0x50000918, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;
	u8_buf[0] = 0x01;
	i32_ret = raydium_i2c_pda_write(client, 0x40000004, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;
	msleep(25);

	/*WRT boot-loader to PRAM first*/
	memset(u8_buf, 0, sizeof(u8_buf));
	i32_ret = raydium_i2c_pda_write(client, 0x50000900, u8_buf, 4);

	/*Sending bootloader*/
	i32_ret = raydium_write_to_pram_2X(client, 0x0000,
					    RAYDIUM_BOOTLOADER);
	if (i32_ret < 0)
		goto exit_upgrade;

	i32_ret = raydium_check_pram_crc_2X(client, 0x000, 0x7FC);
	if (i32_ret < 0)
		goto exit_upgrade;

	/*release mcu hold*/
	/*Skip load*/
	i32_ret = set_skip_load(client);
	if (i32_ret < 0)
		pr_err("[touch]%s, set skip_load error!\n", __func__);

exit_upgrade:
	return i32_ret;
}

/* Raydium fireware upgrade flow */
static int raydium_fw_upgrade_2X(struct i2c_client *client,
			      unsigned char u8_type,
			      unsigned char u8_check_crc)
{
	int i32_ret = 0;
	unsigned char u8_buf[4];
	unsigned short u16_retry = 1000;

	/*##### wait for boot-loader start #####*/
	pr_debug("[touch]Type is %x\n", u8_type);

	/*read Boot version*/
	if (raydium_i2c_pda_read(client, 0x80, u8_buf, 4) == ERROR)
		return ERROR;
	pr_debug("[touch]Boot version is %x\n", u8_buf[2]);

	if (u8_buf[2] >= 4) {
		if (u8_type != RAYDIUM_COMP) {
			/*set mcu hold*/
			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x20;
			i32_ret = raydium_i2c_pda_write(client,
						0x50000918,
						u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;
			u8_buf[0] = 0x01;
			i32_ret = raydium_i2c_pda_write(client,
						0x40000004,
						u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;
			msleep(25);
		}

		/*#start write data to PRAM*/
		if (u8_type == RAYDIUM_FIRMWARE) {
			/* unlock PRAM */
			u8_buf[0] = 0x27;
			i32_ret = raydium_i2c_pda_write(client,
						0x50000900,
						u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;
			i32_ret = raydium_write_to_pram_2X(client, 0x800,
							    RAYDIUM_FIRMWARE);
			if (i32_ret < 0)
				goto exit_upgrade;

			i32_ret = raydium_write_to_pram_2X(client, 0x6a00,
							    RAYDIUM_PARA);
			if (i32_ret < 0)
				goto exit_upgrade;

			i32_ret = raydium_check_pram_crc_2X(client, 0x800,
				0x635C);
			if (i32_ret < 0)
				goto exit_upgrade;

		} else if (u8_type == RAYDIUM_BOOTLOADER) {
			/* unlock PRAM */
			u8_buf[0] = 0x0E;
			i32_ret = raydium_i2c_pda_write(client, 0x50000900,
							u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;
			i32_ret = raydium_write_to_pram_2X(client, 0x0800,
							    RAYDIUM_BOOTLOADER);
			if (i32_ret < 0)
				goto exit_upgrade;
			i32_ret = raydium_write_to_pram_2X(client, 0x1000,
							    RAYDIUM_INIT);
			if (i32_ret < 0)
				goto exit_upgrade;

			i32_ret = raydium_check_pram_crc_2X(client, 0x800,
				0x7FC);
			if (i32_ret < 0)
				goto exit_upgrade;
			i32_ret = raydium_check_pram_crc_2X(client, 0x1000,
				0x1FC);
			if (i32_ret < 0)
				goto exit_upgrade;
		}

		if (u8_type != RAYDIUM_COMP) {
			/*release mcu hold*/
			/*Skip load*/
			i32_ret = set_skip_load(client);
			if (i32_ret < 0)
				pr_err("[touch]%s, set skip_load error!\n",
					__func__);
		}

		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0x01;
		i32_ret = raydium_i2c_pda_write(client, 0x20000204, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x20000208, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x2000020C, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0x01;
		i32_ret = raydium_i2c_pda_write(client, 0x20000218, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*#confirm in burn mode*/
		if (wait_fw_state(client, 0x20000214, 255,
				 2000, u16_retry) == ERROR) {
			pr_err("[touch]Error, confirm in burn mode\n");
			i32_ret = ERROR;
			goto exit_upgrade;
		}

		/*Clear BL_CRC*/
		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0x10;
		i32_ret = raydium_i2c_pda_write(client, 0x50000918, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = u8_type;
		i32_ret = raydium_i2c_pda_write(client, 0x50000904, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*#set PRAM length (at 'h5000_090C)*/
		if (u8_type == RAYDIUM_COMP) {
			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x60;
			u8_buf[1] = 0x6b;
			i32_ret = raydium_i2c_pda_write(client, 0x50000908,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x9c;
			u8_buf[1] = 0x02;
			i32_ret = raydium_i2c_pda_write(client, 0x5000090C,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

		} else if (u8_type == RAYDIUM_FIRMWARE) {
			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x00;
			u8_buf[1] = 0x08;
			i32_ret = raydium_i2c_pda_write(client, 0x50000908,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x5c;
			u8_buf[1] = 0x63;
			i32_ret = raydium_i2c_pda_write(client, 0x5000090C,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

		} else if (u8_type == RAYDIUM_BOOTLOADER) {
			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x00;
			u8_buf[1] = 0x08;
			i32_ret = raydium_i2c_pda_write(client, 0x50000908,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x00;
			u8_buf[1] = 0x0A;
			i32_ret = raydium_i2c_pda_write(client, 0x5000090C,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;
		}

		/*#set sync_data(0x20000200) = 0 as WRT data finish*/
		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x20000200, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*#wait for input unlock key*/
		if (wait_fw_state(client, 0x20000210, 168, 1000,
				 u16_retry) == ERROR) {
			pr_err("[touch]Error, wait for input unlock key\n");
			i32_ret = ERROR;
			goto exit_upgrade;
		}

		/*#unlock key*/
		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0xd7;
		i32_ret = raydium_i2c_pda_write(client, 0x50000938, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0xa5;
		i32_ret = raydium_i2c_pda_write(client, 0x50000934, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x50000934, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0xa5;
		i32_ret = raydium_i2c_pda_write(client, 0x50000934, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x50000938, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x50000624, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*#wrt return data as unlock value*/
		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0xa8;
		i32_ret = raydium_i2c_pda_write(client, 0x20000214, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*pr_debug("[touch]ready burn flash\n");*/

		/*#clr sync_data(0x20000200) = 0 as finish*/
		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x20000200, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/* wait erase/wrt finish
		 * confirm burning_state result (gu8I2CSyncData.burning_state =
		 * BURNING_WRT_FLASH_FINISH at 0x2000020C)
		 */
		if (wait_fw_state(client, 0x2000020c, 6, 2000,
				 u16_retry) == ERROR) {
			pr_err("[touch]Error, wait erase/wrt finish\n");
			i32_ret = ERROR;
			goto exit_upgrade;
		}
		pr_info("[touch]Burn flash ok\n");

		if (u8_check_crc) {
			memset(u8_buf, 0, sizeof(u8_buf));
			i32_ret = raydium_i2c_pda_write(client, 0x20000200,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

			/*#wait software reset finish*/
			msleep(25);

			/* wait sw reset finished 0x20000214 = 0x82 */
			if (wait_fw_state(client, 0x20000214, 130, 2000,
					 u16_retry) == ERROR) {
				pr_err("[touch]Error, wait sw reset finished\n");
				i32_ret = ERROR;
				goto exit_upgrade;
			}

			/*#set test_mode = 1*/
			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x01;
			i32_ret = raydium_i2c_pda_write(client, 0x20000218,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

			/*#wait crc check finish*/
			if (wait_fw_state(client, 0x20000208, 2,
					 2000, u16_retry)
					== ERROR) {
				pr_err("[touch]Error, wait crc check finish\n");
				i32_ret = ERROR;
				goto exit_upgrade;
			}

			/*#crc check pass 0x20000214 = 0x81*/
			if (wait_fw_state(client, 0x20000214, 0x81,
					 2000, u16_retry)
					== ERROR) {
				pr_err("[touch]Error, confirm crc result\n");
				i32_ret = ERROR;
				goto exit_upgrade;
			}
		}

	} else {
		/*#set main state as burning mode, normal init state*/
		/* #sync_data:200h
		 * main_state:204h
		 * normal_state:208h
		 * burning_state:20Ch
		 */
		/* #sync_data:210h
		 * cmd_type:210h
		 * ret_data:214h
		 * test_mode:218h
		 */
		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0x01;
		i32_ret = raydium_i2c_pda_write(client, 0x20000204, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x20000208, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x2000020C, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0x01;
		i32_ret = raydium_i2c_pda_write(client, 0x20000218, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*#confirm in burn mode*/
		if (wait_fw_state(client, 0x50000900, 63,
				 2000, u16_retry) == ERROR) {
			pr_err("[touch]Error, confirm in burn mode\n");
			i32_ret = ERROR;
			goto exit_upgrade;
		}

		/*Clear BL_CRC*/
		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0x10;
		i32_ret = raydium_i2c_pda_write(client, 0x50000918, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*
		 * #set PRAM type (at 0x50000904), wrt param code
		 * #write PRAM relative data
		 * #init_code:0x01,
		 * baseline:0x02
		 * COMP:0x04
		 * param:0x08
		 * FW:0x10
		 * bootloader:0x20
		 */
		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = u8_type;
		i32_ret = raydium_i2c_pda_write(client, 0x50000904, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*
		 * #set PRAM addr (at 'h5000_0908)
		 * #init_code:0x800
		 * Baseline:0xA00
		 * COMP:0xCD4
		 * para:0xF1C
		 * FW:0x1000
		 * BOOT:0x5000
		 *
		 * #set PRAM length (at 'h5000_090C)
		 */
		if (u8_type == RAYDIUM_INIT) {
			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x00;
			u8_buf[1] = 0x6e;
			i32_ret = raydium_i2c_pda_write(client, 0x50000908,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x00;
			u8_buf[1] = 0x02;
			i32_ret = raydium_i2c_pda_write(client, 0x5000090C,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

		} else if (u8_type == RAYDIUM_BASELINE) {
			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0xcc;
			u8_buf[1] = 0x6c;
			i32_ret = raydium_i2c_pda_write(client, 0x50000908,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x30;
			u8_buf[1] = 0x01;
			i32_ret = raydium_i2c_pda_write(client, 0x5000090C,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

		} else if (u8_type == RAYDIUM_COMP) {
			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x60;
			u8_buf[1] = 0x6b;
			i32_ret = raydium_i2c_pda_write(client, 0x50000908,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x9c;
			u8_buf[1] = 0x02;
			i32_ret = raydium_i2c_pda_write(client, 0x5000090C,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

		} else if (u8_type == RAYDIUM_PARA) {
			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x00;
			u8_buf[1] = 0x6a;
			i32_ret = raydium_i2c_pda_write(client, 0x50000908,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x5c;
			u8_buf[1] = 0x01;
			i32_ret = raydium_i2c_pda_write(client, 0x5000090C,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

		} else if (u8_type == RAYDIUM_FIRMWARE) {
			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x00;
			u8_buf[1] = 0x08;
			i32_ret = raydium_i2c_pda_write(client, 0x50000908,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x5c;
			u8_buf[1] = 0x63;
			i32_ret = raydium_i2c_pda_write(client, 0x5000090C,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

		} else if (u8_type == RAYDIUM_BOOTLOADER) {
			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x00;
			u8_buf[1] = 0x08;
			i32_ret = raydium_i2c_pda_write(client, 0x50000908,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x00;
			u8_buf[1] = 0x0A;
			i32_ret = raydium_i2c_pda_write(client, 0x5000090C,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;
		}

		/*#set sync_data(0x20000200) = 0 as WRT data finish*/
		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x20000200, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*#Wait bootloader check addr and PRAM unlock*/
		/*#Confirm ret_data at 0x20000214 is SET_ADDR_READY*/
		if (wait_fw_state(client, 0x20000214, 161,
				 2000, u16_retry) == ERROR) {
			pr_err("[touch]Error, SET_ADDR_READY\n");
			i32_ret = ERROR;
			goto exit_upgrade;
		}

		/*#Confirm cmd_type at 0x20000210 is WRT_PRAM_DATA*/
		if (wait_fw_state(client, 0x20000210, 163,
				 1000, u16_retry) == ERROR) {
			pr_err("[touch]Error, WRT_PRAM_DATA\n");
			i32_ret = ERROR;
			goto exit_upgrade;
		}

		/*#start write data to PRAM*/
		if (u8_type == RAYDIUM_INIT) {
			i32_ret = raydium_fw_upgrade_with_image(client, 0x6E00,
							    RAYDIUM_INIT);
			if (i32_ret < 0)
				goto exit_upgrade;
		}  else if (u8_type == RAYDIUM_PARA) {
			i32_ret = raydium_fw_upgrade_with_image(client, 0x6a00,
							    RAYDIUM_PARA);
			if (i32_ret < 0)
				goto exit_upgrade;
		} else if (u8_type == RAYDIUM_FIRMWARE) {
			i32_ret = raydium_fw_upgrade_with_image(client, 0x800,
							    RAYDIUM_FIRMWARE);
			if (i32_ret < 0)
				goto exit_upgrade;

			i32_ret = raydium_fw_upgrade_with_image(client, 0x6a00,
							    RAYDIUM_PARA);
			if (i32_ret < 0)
				goto exit_upgrade;


		} else if (u8_type == RAYDIUM_BOOTLOADER) {
			i32_ret = raydium_fw_upgrade_with_image(client, 0x0800,
							    RAYDIUM_BOOTLOADER);
			if (i32_ret < 0)
				goto exit_upgrade;
			i32_ret = raydium_fw_upgrade_with_image(client, 0x1000,
							    RAYDIUM_INIT);
			if (i32_ret < 0)
				goto exit_upgrade;
		}

		/*
		 *set sync_data(0x20000200) = 0 as WRT data finish
		 *bootloader check checksum
		 */
		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x20000200, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*
		 * wait(checksum okay) ACK cmd
		 * (gu8I2CSyncData.cmd_type=0xa5 at 0x20000210)
		 */
		if (wait_fw_state(client, 0x20000210,
				 165, 2000, u16_retry) == ERROR) {
			pr_err("[touch]Error, WRT_CHECKSUM_OK\n");
			i32_ret = ERROR;
			goto exit_upgrade;
		}

		/*#confirm ACK cmd result(ret_data=0xa5 at 0x20000214)*/
		if (wait_fw_state(client, 0x20000214,
				 165, 1000, u16_retry) == ERROR) {
			pr_err("[touch]Error, confirm ACK cmd result\n");
			i32_ret = ERROR;
			goto exit_upgrade;
		}

		/*
		 * set ACK return data = 0x5A
		 * adb shell "echo 20000210 1 A5 > /sys/bus/i2c/drivers/
		 * raydium_ts/1-0039 raydium_i2c_pda_access"
		 * above command can be ignored, due to previous while loop
		 * has check its value.
		 */
		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0xa5;
		i32_ret = raydium_i2c_pda_write(client, 0x20000210, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0x5a;
		i32_ret = raydium_i2c_pda_write(client, 0x20000214, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*#clr sync_data(0x20000200) = 0 as finish*/
		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x20000200, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*#wait for input unlock key*/
		if (wait_fw_state(client, 0x20000210,
				 168, 1000, u16_retry) == ERROR) {
			pr_err("[touch]Error, wait for input unlock key\n");
			i32_ret = ERROR;
			goto exit_upgrade;
		}

		/*#unlock key*/
		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0xd7;
		i32_ret = raydium_i2c_pda_write(client, 0x50000938, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0xa5;
		i32_ret = raydium_i2c_pda_write(client, 0x50000934, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x50000934, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0xa5;
		i32_ret = raydium_i2c_pda_write(client, 0x50000934, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x50000938, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x50000624, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*#wrt return data as unlock value*/
		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0xa8;
		i32_ret = raydium_i2c_pda_write(client, 0x20000214, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*pr_info("[touch]ready burn flash\n");*/

		/*#clr sync_data(0x20000200) = 0 as finish*/
		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x20000200, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/* wait erase/wrt finish
		 * confirm burning_state result (gu8I2CSyncData.burning_state =
		 * BURNING_WRT_FLASH_FINISH at 0x2000020C)
		 */
		if (wait_fw_state(client, 0x2000020c,
				 6, 2000, u16_retry) == ERROR) {
			dev_err(&g_raydium_ts->client->dev,
				"[touch]Error, wait erase/wrt finish\n");
			i32_ret = ERROR;
			goto exit_upgrade;
		}
		pr_info("[touch]Burn flash ok\n");


		if (u8_type == RAYDIUM_BOOTLOADER) {
			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x10;
			u8_buf[1] = 0x08;
			i32_ret = raydium_i2c_pda_write(client, 0x50000918,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;
		}

		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x20000200, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		/*#wait software reset finish*/
		msleep(25);

		/* wait sw reset finished 0x20000214 = 0x82 */
		if (wait_fw_state(client, 0x20000214,
				 130, 2000, u16_retry) == ERROR) {
			pr_err("[touch]Error, wait sw reset finished\n");
			i32_ret = ERROR;
			goto exit_upgrade;
		}

		if (u8_type == RAYDIUM_BASELINE || u8_type == RAYDIUM_COMP ||
		    u8_type == RAYDIUM_FIRMWARE || u8_check_crc == 1) {
			/*#set test_mode = 1*/
			memset(u8_buf, 0, sizeof(u8_buf));
			u8_buf[0] = 0x01;
			i32_ret = raydium_i2c_pda_write(client, 0x20000218,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;

			/*#wait crc check finish*/
			if (wait_fw_state(client, 0x20000208, 2,
					 2000, u16_retry) == ERROR) {
				pr_err("[touch]Error, wait crc check finish\n");
				i32_ret = ERROR;
				goto exit_upgrade;
			}

			/*#crc check pass 0x20000214 = 0x81*/
			if (wait_fw_state(client, 0x20000214, 0x81,
					 2000, u16_retry)
					== ERROR) {
				pr_err("[touch]Error, confirm crc result\n");
				i32_ret = ERROR;
				goto exit_upgrade;
			}
		}
	}

	/*#run to next step*/
		pr_info("[touch]Type 0x%x => Pass\n", u8_type);

	if (u8_check_crc) {
		/*#clr sync para*/
		memset(u8_buf, 0, sizeof(u8_buf));
		i32_ret = raydium_i2c_pda_write(client, 0x20000210, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		i32_ret = raydium_i2c_pda_write(client, 0x20000214, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		i32_ret = raydium_i2c_pda_write(client, 0x20000218, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;


		i32_ret = raydium_i2c_pda_write(client, 0x20000200, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		usleep_range(4500, 5500);
		raydium_i2c_pda_set_address(0x50000628, DISABLE);

		g_u8_i2c_mode = PDA2_MODE;

		pr_info("[touch]Burn FW finish!\n");
	}

exit_upgrade:
	return i32_ret;
}

int raydium_burn_fw(struct i2c_client *client)
{
	int i32_ret = 0;

	g_u8_resetflag = true;
	if ((g_raydium_ts->id & 0x2000) != 0) {
		pr_debug("[touch]start burn function!\n");
		i32_ret = raydium_boot_upgrade_2X(client);
		if (i32_ret < 0)
			goto exit_upgrade;

		i32_ret = raydium_fw_upgrade_2X(client, RAYDIUM_BOOTLOADER, 0);
		if (i32_ret < 0)
			goto exit_upgrade;

		i32_ret = raydium_fw_upgrade_2X(client, RAYDIUM_FIRMWARE, 1);
		if (i32_ret < 0)
			goto exit_upgrade;

	}

exit_upgrade:
	return i32_ret;
}

int raydium_fw_update_check(unsigned short u16_i2c_data)
{

	unsigned char u8_rbuffer[4];
	unsigned int u32_fw_version, u32_image_version;
	int i32_ret = ERROR;
	unsigned char u8_mode_change;

	mutex_lock(&g_raydium_ts->lock);
	i32_ret = raydium_i2c_pda2_set_page(g_raydium_ts->client,
				g_raydium_ts->is_suspend,
				RAYDIUM_PDA2_PAGE_0);
	if (i32_ret < 0)
		goto exit_error;

	i32_ret = raydium_i2c_pda2_read(g_raydium_ts->client,
				    RAYDIUM_PDA2_FW_VERSION_ADDR,
				    u8_rbuffer,
				    4);
	if (i32_ret < 0)
		goto exit_error;

	mutex_unlock(&g_raydium_ts->lock);

	u32_fw_version = (u8_rbuffer[0] << 24) | (u8_rbuffer[1] << 16) |
		(u8_rbuffer[2] << 8) | u8_rbuffer[3];
	pr_info("[touch]RAD FW ver 0x%.8x\n", u32_fw_version);

	g_raydium_ts->fw_version = u32_fw_version;

	g_raydium_ts->id = ((u16_i2c_data & 0xF) << 12) |
		((u8_rbuffer[0] & 0xF) << 8) | u8_rbuffer[1];

	raydium_mem_table_init(g_raydium_ts->id);
	if (raydium_mem_table_setting()) {

		u32_image_version = (g_rad_para_image[0x0004] << 24) |
			(g_rad_para_image[0x0005] << 16) |
			(g_rad_para_image[0x0006] << 8) |
			g_rad_para_image[0x0007];

		pr_info("[touch]RAD Image FW ver : 0x%x\n", u32_image_version);
	} else {
		pr_err("[touch]Mem setting failed, Stop fw upgrade!\n");
		return FAIL;
	}

	if (u32_fw_version != u32_image_version) {
		pr_info("[touch]FW need update.\n");
		raydium_irq_control(DISABLE);
		if ((g_u8_raydium_flag & ENG_MODE) == 0) {
			g_u8_raydium_flag |= ENG_MODE;
			u8_mode_change = 1;
		}
		i32_ret = raydium_burn_fw(g_raydium_ts->client);
		if (i32_ret < 0)
			pr_err("[touch]FW update fail:%d\n", i32_ret);

		if (u8_mode_change) {
			g_u8_raydium_flag &= ~ENG_MODE;
			u8_mode_change = 0;
		}
		raydium_irq_control(ENABLE);
		mutex_lock(&g_raydium_ts->lock);
		i32_ret = raydium_i2c_pda2_set_page(g_raydium_ts->client,
					g_raydium_ts->is_suspend,
					RAYDIUM_PDA2_PAGE_0);
		if (i32_ret < 0)
			goto exit_error;

		i32_ret = raydium_i2c_pda2_read(g_raydium_ts->client,
					    RAYDIUM_PDA2_FW_VERSION_ADDR,
					    u8_rbuffer,
					    4);
		if (i32_ret < 0)
			goto exit_error;

		mutex_unlock(&g_raydium_ts->lock);
		u32_fw_version = (u8_rbuffer[0] << 24) |
			     (u8_rbuffer[1] << 16) |
			     (u8_rbuffer[2] << 8) |
			     u8_rbuffer[3];
		pr_info("[touch]RAD FW ver is 0x%x\n",
			 u32_fw_version);
		g_raydium_ts->fw_version = u32_fw_version;
	} else
		pr_info("[touch]FW is the latest version.\n");

	return i32_ret;

exit_error:
	mutex_unlock(&g_raydium_ts->lock);
	return i32_ret;
}
int raydium_burn_comp(struct i2c_client *client)
{
	int i32_ret = FAIL;

	i32_ret = set_skip_load(client);
	if (i32_ret < 0)
		goto exit_upgrade;

	if ((g_raydium_ts->id & 0x2000) != 0) {
		i32_ret = raydium_fw_upgrade_2X(client, RAYDIUM_COMP, 1);
		if (i32_ret < 0)
			goto exit_upgrade;

	}

	i32_ret = SUCCESS;

exit_upgrade:
	return i32_ret;
}

int raydium_load_test_fw(struct i2c_client *client)
{
	int i32_ret = SUCCESS;
	unsigned char u8_buf[4];
	unsigned int u32_crc_result, u32_read_data;

	/*set mcu hold*/
	memset(u8_buf, 0, sizeof(u8_buf));
	u8_buf[0] = 0x20;
	raydium_i2c_pda_write(client, 0x50000918, u8_buf, 4);
	raydium_i2c_pda_read(client, 0x40000004, u8_buf, 4);
	u8_buf[0] |= 0x01;
	raydium_i2c_pda_write(client, 0x40000004, u8_buf, 4);
	msleep(25);

	i32_ret = raydium_i2c_pda_read(client, 0x40000000, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;

	u8_buf[3] |= 0x40;
	i32_ret = raydium_i2c_pda_write(client, 0x40000000, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;

	i32_ret = raydium_i2c_pda_read(client, 0x40000014, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;

	u8_buf[0] |= 0x04;
	u8_buf[1] |= 0x04;
	i32_ret = raydium_i2c_pda_write(client, 0x40000014, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;

	memset(u8_buf, 0, sizeof(u8_buf));
	pr_debug("[touch]Raydium WRT test_fw to PRAM\n");

	i32_ret = raydium_i2c_pda_write(client, 0x50000900, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;

	/*Sending test fw*/
	if ((g_raydium_ts->id & 0x2000) != 0) {
		i32_ret = raydium_fw_upgrade_with_image(client,
			0x800, RAYDIUM_TEST_FW);
		if (i32_ret < 0)
			goto exit_upgrade;
	}

	/*check pram crc data*/
	if ((g_raydium_ts->id & 0x2000) != 0) {
		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[1] = 0x08;
		u8_buf[2] = 0x5B;
		u8_buf[3] = 0x6B;
		i32_ret = raydium_i2c_pda_write(client, 0x50000974, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;
		i32_ret = raydium_i2c_pda_read(client, 0x5000094C, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;
		u8_buf[3] |= 0x81;
		i32_ret = raydium_i2c_pda_write(client, 0x5000094C, u8_buf, 4);
		usleep_range(9500, 10500);
		i32_ret = raydium_i2c_pda_read(client, 0x50000978, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;
		memcpy(&u32_crc_result, u8_buf, 4);
		i32_ret = raydium_i2c_pda_read(client, 0x6B5C, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;
		memcpy(&u32_read_data, u8_buf, 4);
		if (u32_read_data != u32_crc_result) {
			pr_err("[touch]check pram fw crc fail, result=0x%x\n",
					u32_crc_result);
			i32_ret = ERROR;
			goto exit_upgrade;
		}

		memset(u8_buf, 0, sizeof(u8_buf));
		u8_buf[0] = 0x60;
		u8_buf[1] = 0x6B;
		u8_buf[2] = 0xFB;
		u8_buf[3] = 0x6D;
		i32_ret = raydium_i2c_pda_write(client, 0x50000974, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;
		i32_ret = raydium_i2c_pda_read(client, 0x5000094C, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;
		u8_buf[3] |= 0x81;
		i32_ret = raydium_i2c_pda_write(client, 0x5000094C, u8_buf, 4);
		usleep_range(1000, 2000);
		i32_ret = raydium_i2c_pda_read(client, 0x50000978, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;
		memcpy(&u32_crc_result, u8_buf, 4);
		i32_ret = raydium_i2c_pda_read(client, 0x6DFC, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;
		memcpy(&u32_read_data, u8_buf, 4);
		if (u32_read_data != u32_crc_result) {
			pr_err("[touch]check pram CB crc fail, result=0x%x\n",
					u32_crc_result);
			i32_ret = ERROR;
			goto exit_upgrade;
		}

		i32_ret = raydium_i2c_pda_read(client, 0x80, u8_buf, 4);
		if (i32_ret < 0)
			goto exit_upgrade;

		if (u8_buf[2] > 2) {
			pr_err("[touch]bootloader version %x,!!\n", u8_buf[2]);
			memset(u8_buf, 0, sizeof(u8_buf));

			u8_buf[1] = 0x04;
			i32_ret = raydium_i2c_pda_write(client, 0x50000918,
							 u8_buf, 4);
			if (i32_ret < 0)
				goto exit_upgrade;
		}
	}

	/*Skip load*/
	pr_info("[touch]Raydium skip load\n");
	i32_ret = raydium_i2c_pda_read(client, 0x50000918, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;
	u8_buf[0] = 0x10;
	i32_ret = raydium_i2c_pda_write(client, 0x50000918, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;
	i32_ret = raydium_do_software_reset(client);
	if (i32_ret < 0)
		goto exit_upgrade;
	i32_ret = raydium_i2c_pda_read(client, 0x50000918, u8_buf, 4);
	if (i32_ret < 0)
		goto exit_upgrade;
	pr_debug("[touch]0x5000918 = 0x%x, 0x%x, 0x%x, 0x%x\n",
		u8_buf[0], u8_buf[1], u8_buf[2], u8_buf[3]);
	i32_ret = raydium_check_fw_ready(client);

exit_upgrade:
	return i32_ret;
}

#endif /* FW_UPDATE_EN */

MODULE_AUTHOR("Raydium");
MODULE_DESCRIPTION("Raydium TouchScreen driver");
MODULE_LICENSE("GPL");
