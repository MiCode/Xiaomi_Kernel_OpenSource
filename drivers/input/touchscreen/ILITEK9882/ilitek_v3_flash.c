/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "ilitek_v3.h"

#define UPDATE_PASS		0
#define UPDATE_FAIL		-1
#define TIMEOUT_SECTOR		500
#define TIMEOUT_PAGE		3500
#define TIMEOUT_PROGRAM		10

static struct touch_fw_data {
	u8 block_number;
	u32 start_addr;
	u32 end_addr;
	u32 new_fw_cb;
	int delay_after_upgrade;
	bool isCRC;
	bool isboot;
	bool is80k;
	int hex_tag;
} tfd;

static struct flash_block_info {
	char *name;
	u32 start;
	u32 end;
	u32 len;
	u32 mem_start;
	u32 fix_mem_start;
	u8 mode;
} fbi[FW_BLOCK_INFO_NUM];

static u8 fw_flash_tmp[K + 7];
static u8 *pfw;
static u8 *CTPM_FW;

static u32 HexToDec(char *phex, s32 len)
{
	u32 ret = 0, temp = 0, i;
	s32 shift = (len - 1) * 4;

	for (i = 0; i < len; shift -= 4, i++) {
		if ((phex[i] >= '0') && (phex[i] <= '9'))
			temp = phex[i] - '0';
		else if ((phex[i] >= 'a') && (phex[i] <= 'f'))
			temp = (phex[i] - 'a') + 10;
		else if ((phex[i] >= 'A') && (phex[i] <= 'F'))
			temp = (phex[i] - 'A') + 10;

		ret |= (temp << shift);
	}
	return ret;
}

static int CalculateCRC32(u32 start_addr, u32 len, u8 *pfw)
{
	int i = 0, j = 0;
	int crc_poly = 0x04C11DB7;
	int tmp_crc = 0xFFFFFFFF;

	for (i = start_addr; i < start_addr + len; i++) {
		tmp_crc ^= (pfw[i] << 24);

		for (j = 0; j < 8; j++) {
			if ((tmp_crc & 0x80000000) != 0)
				tmp_crc = tmp_crc << 1 ^ crc_poly;
			else
				tmp_crc = tmp_crc << 1;
		}
	}
	return tmp_crc;
}

static int ilitek_tddi_fw_iram_read(u8 *buf, u32 start, int len)
{
	int limit = 4*K;
	int addr = 0, loop = 0, tmp_len = len, cnt = 0;
	u8 cmd[4] = {0};

	if (!buf) {
		ILI_ERR("buf is null\n");
		return -ENOMEM;
	}

	if (len % limit)
		loop = (len / limit) + 1;
	else
		loop = len / limit;

	for (cnt = 0, addr = start; cnt < loop; cnt++, addr += limit) {
		if (tmp_len > limit)
			tmp_len = limit;

		cmd[0] = 0x25;
		cmd[3] = (char)((addr & 0x00FF0000) >> 16);
		cmd[2] = (char)((addr & 0x0000FF00) >> 8);
		cmd[1] = (char)((addr & 0x000000FF));

		if (ilits->wrapper(cmd, 4, NULL, 0, OFF, OFF) < 0) {
			ILI_ERR("Failed to write iram data\n");
			return -ENODEV;
		}

		if (ilits->wrapper(NULL, 0, buf + cnt * limit, tmp_len, OFF, OFF) < 0) {
			ILI_ERR("Failed to Read iram data\n");
			return -ENODEV;
		}

		tmp_len = len - cnt * limit;
		ilits->fw_update_stat = ((len - tmp_len) * 100) / len;
		ILI_DBG("Reading iram data .... %d%c", ilits->fw_update_stat, '%');
	}
	return 0;
}

int ili_fw_dump_iram_data(u32 start, u32 end, bool save, bool mcu)
{
	struct file *f = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	int i, ret, len;
	u8 *fw_buf = NULL;

	if (ili_ice_mode_ctrl(ENABLE, mcu) < 0) {
		ILI_ERR("Enable ice mode failed\n");
		ret = -1;
		goto out;
	}

	len = end - start + 1;

	if (len > MAX_HEX_FILE_SIZE) {
		ILI_ERR("len is larger than buffer, abort\n");
		ret = -EINVAL;
		goto out;
	}

	fw_buf = kzalloc(MAX_HEX_FILE_SIZE, GFP_KERNEL);
	if (ERR_ALLOC_MEM(fw_buf)) {
		ILI_ERR("Failed to allocate update_buf\n");
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < MAX_HEX_FILE_SIZE; i++)
		fw_buf[i] = 0xFF;

	ret = ilitek_tddi_fw_iram_read(fw_buf, start, len);
	if (ret < 0)
		goto out;

	f = filp_open(DUMP_IRAM_PATH, O_WRONLY | O_CREAT | O_TRUNC, 644);
	if (ERR_ALLOC_MEM(f)) {
		ILI_ERR("Failed to open the file at %ld.\n", PTR_ERR(f));
		ret = -1;
		goto out;
	}

	old_fs = get_fs();
	set_fs(get_ds());
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(f, fw_buf, len, &pos);
	set_fs(old_fs);
	filp_close(f, NULL);
	ILI_INFO("Save iram data to %s\n", DUMP_IRAM_PATH);

out:
	ili_ice_mode_ctrl(DISABLE, mcu);
	ILI_INFO("Dump IRAM %s\n", (ret < 0) ? "FAIL" : "SUCCESS");
	ipio_kfree((void **)&fw_buf);
	return ret;
}

static int ilitek_tddi_flash_poll_busy(int timer)
{
	int ret = UPDATE_PASS, retry = timer;
	u8 cmd = 0x5;
	u32 temp = 0;

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
		ILI_ERR("Pull cs low failed\n");

	if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, cmd, 1) < 0)
		ILI_ERR("Write 0x5 cmd failed\n");

	do {
		if (ili_ice_mode_write(FLASH2_ADDR, 0xFF, 1) < 0)
			ILI_ERR("Write dummy failed\n");

		mdelay(1);

		if (ili_ice_mode_read(FLASH4_ADDR, &temp, sizeof(u8)) < 0)
			ILI_ERR("Read flash busy error\n");

		if ((temp & 0x3) == 0)
			break;
	} while (--retry >= 0);

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
		ILI_ERR("Pull cs high failed\n");

	if (retry <= 0) {
		ILI_ERR("Flash polling busy timeout ! tmp = %x\n", temp);
		ret = UPDATE_FAIL;
	}

	return ret;
}

void ili_flash_clear_dma(void)
{
	if (ili_ice_mode_bit_mask_write(FLASH0_ADDR, FLASH0_reg_preclk_sel, (2 << 16)) < 0)
		ILI_ERR("Write %lu at %x failed\n", FLASH0_reg_preclk_sel, FLASH0_ADDR);

	if (ili_ice_mode_bit_mask_write(FLASH4_ADDR, FLASH4_reg_flash_dma_trigger_en, (0 << 24)) < 0)
		ILI_ERR("Write %lu at %x failed\n", FLASH4_reg_flash_dma_trigger_en, FLASH4_ADDR);

	if (ili_ice_mode_bit_mask_write(FLASH0_ADDR, FLASH0_reg_rx_dual, (0 << 24)) < 0)
		ILI_ERR("Write %lu at %x failed\n", FLASH0_reg_rx_dual, FLASH0_ADDR);

	if (ili_ice_mode_write(FLASH3_reg_rcv_cnt, 0x00, 1) < 0)
		ILI_ERR("Write 0x0 at %x failed\n", FLASH3_reg_rcv_cnt);

	if (ili_ice_mode_write(FLASH4_reg_rcv_data, 0xFF, 1) < 0)
		ILI_ERR("Write 0xFF at %x failed\n", FLASH4_reg_rcv_data);
}

static int ilitek_tddi_flash_read_int_flag(void)
{
	int retry = 100;
	u32 data = 0;

	do {
		if (ili_ice_mode_read(INTR1_ADDR & BIT(25), &data, sizeof(u32)) < 0)
			ILI_ERR("Read flash int flag error\n");

		ILI_DBG("int flag = %x\n", data);
		if (data)
			break;
		mdelay(2);
	} while (--retry >= 0);

	if (retry <= 0) {
		ILI_ERR("Read Flash INT flag timeout !, flag = 0x%x\n", data);
		return -1;
	}
	return 0;
}

void ili_flash_dma_write(u32 start, u32 end, u32 len)
{
	if (ili_ice_mode_bit_mask_write(FLASH0_ADDR, FLASH0_reg_preclk_sel, 1 << 16) < 0)
		ILI_ERR("Write %lu at %x failed\n", FLASH0_reg_preclk_sel, FLASH0_ADDR);

	if (ili_ice_mode_write(FLASH0_reg_flash_csb, 0x00, 1) < 0)
		ILI_ERR("Pull cs low failed\n");

	if (ili_ice_mode_write(FLASH1_reg_flash_key1, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_reg_tx_data, 0x0b, 1) < 0)
		ILI_ERR("Write 0x0b at %x failed\n", FLASH2_reg_tx_data);

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write 0xb timeout \n");
		return;
	}

	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, (1 << 25)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR1_reg_flash_int_flag, INTR1_ADDR);

	if (ili_ice_mode_write(FLASH2_reg_tx_data, (start & 0xFF0000) >> 16, 1) < 0)
		ILI_ERR("Write %x at %x failed\n", (start & 0xFF0000), FLASH2_reg_tx_data);

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write addr1 timeout\n");
		return;
	}

	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, (1 << 25)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR1_reg_flash_int_flag, INTR1_ADDR);

	if (ili_ice_mode_write(FLASH2_reg_tx_data, (start & 0x00FF00) >> 8, 1) < 0)
		ILI_ERR("Write %x at %x failed\n", (start & 0x00FF00) >> 8, FLASH2_reg_tx_data);

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write addr2 timeout\n");
		return;
	}

	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, (1 << 25)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR1_reg_flash_int_flag, INTR1_ADDR);

	if (ili_ice_mode_write(FLASH2_reg_tx_data, (start & 0x0000FF), 1) < 0)
		ILI_ERR("Write %x at %x failed\n", (start & 0x0000FF), FLASH2_reg_tx_data);

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write addr3 timeout\n");
		return;
	}

	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, (1 << 25)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR1_reg_flash_int_flag, INTR1_ADDR);

	if (ili_ice_mode_bit_mask_write(FLASH0_ADDR, FLASH0_reg_rx_dual, 0 << 24) < 0)
		ILI_ERR("Write %lu at %x failed\n", FLASH0_reg_rx_dual, FLASH0_ADDR);

	if (ili_ice_mode_write(FLASH2_reg_tx_data, 0x00, 1) < 0)
		ILI_ERR("Write dummy failed\n");

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write dummy timeout\n");
		return;
	}

	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, (1 << 25)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR1_reg_flash_int_flag, INTR1_ADDR);

	if (ili_ice_mode_write(FLASH3_reg_rcv_cnt, len, 4) < 0)
		ILI_ERR("Write length failed\n");
}

static void ilitek_tddi_flash_write_enable(void)
{
	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
		ILI_ERR("Pull CS low failed\n");

	if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, 0x6, 1) < 0)
		ILI_ERR("Write 0x6 failed\n");

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
		ILI_ERR("Pull CS high failed\n");
}

u32 ili_fw_read_hw_crc(u32 start, u32 end, u32 *flash_crc)
{
	int retry = 100;
	u32 busy = 0;
	u32 write_len = end;

	if (write_len > ilits->chip->max_count) {
		ILI_ERR("The length (%x) written into firmware is greater than max count (%x)\n",
			write_len, ilits->chip->max_count);
		return -1;
	}

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
		ILI_ERR("Pull CS low failed\n");

	if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, 0x3b, 1) < 0)
		ILI_ERR("Write 0x3b failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, (start & 0xFF0000) >> 16, 1) < 0)
		ILI_ERR("Write address failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, (start & 0x00FF00) >> 8, 1) < 0)
		ILI_ERR("Write address failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, (start & 0x0000FF), 1) < 0)
		ILI_ERR("Write address failed\n");

	if (ili_ice_mode_write(0x041003, 0x01, 1) < 0)
		ILI_ERR("Write enable Dio_Rx_dual failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, 0xFF, 1) < 0)
		ILI_ERR("Write dummy failed\n");

	if (ili_ice_mode_write(0x04100C, write_len, 3) < 0)
		ILI_ERR("Write Set Receive count failed\n");

	if (ili_ice_mode_write(0x048007, 0x02, 1) < 0)
		ILI_ERR("Write clearing int flag failed\n");

	if (ili_ice_mode_write(0x041016, 0x00, 1) < 0)
		ILI_ERR("Write 0x0 at 0x041016 failed\n");

	if (ili_ice_mode_write(0x041016, 0x01, 1) < 0)
		ILI_ERR("Write Checksum_En failed\n");

	if (ili_ice_mode_write(FLASH4_ADDR, 0xFF, 1) < 0)
		ILI_ERR("Write start to receive failed\n");

	do {
		if (ili_ice_mode_read(0x048007, &busy, sizeof(u8)) < 0)
			ILI_ERR("Read busy error\n");

		ILI_DBG("busy = %x\n", busy);
		if (((busy >> 1) & 0x01) == 0x01)
			break;
		mdelay(2);
	} while (--retry >= 0);

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
		ILI_ERR("Write CS high failed\n");

	if (retry <= 0) {
		ILI_ERR("Read HW CRC timeout !, busy = 0x%x\n", busy);
		return -1;
	}

	if (ili_ice_mode_write(0x041003, 0x0, 1) < 0)
		ILI_ERR("Write disable dio_Rx_dual failed\n");

	if (ili_ice_mode_read(0x04101C, flash_crc, sizeof(u32)) < 0) {
		ILI_ERR("Read hw crc error\n");
		return -1;
	}

	return 0;
}

static int ilitek_tddi_fw_read_flash_data(u32 start, u32 end, u8 *data, int len)
{
	u32 i, j, index = 0;
	u32 tmp;

	if (end - start > len) {
		ILI_ERR("the length (%d) reading crc is over than len(%d)\n", end - start, len);
		return -1;
	}

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
		ILI_ERR("Write cs low failed\n");

	if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, 0x03, 1) < 0)
		ILI_ERR("Write 0x3 failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, (start & 0xFF0000) >> 16, 1) < 0)
		ILI_ERR("Write address failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, (start & 0x00FF00) >> 8, 1) < 0)
		ILI_ERR("Write address failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, (start & 0x0000FF), 1) < 0)
		ILI_ERR("Write address failed\n");

	for (i = start, j = 0; i <= end; i++, j++) {
		if (ili_ice_mode_write(FLASH2_ADDR, 0xFF, 1) < 0)
			ILI_ERR("Write dummy failed\n");

		if (ili_ice_mode_read(FLASH4_ADDR, &tmp, sizeof(u8)) < 0)
			ILI_ERR("Read flash data error!\n");

		data[index] = tmp;
		index++;
		ilits->fw_update_stat = (j * 100) / len;
		ILI_DBG("Reading flash data .... %d%c", ilits->fw_update_stat, '%');
	}

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
		ILI_ERR("Write cs high failed\n");

	return 0;
}

int ili_fw_dump_flash_data(u32 start, u32 end, bool user, bool mcu)
{
	struct file *f = NULL;
	u8 *buf = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	u32 start_addr, end_addr;
	int ret, length;

	f = filp_open(DUMP_FLASH_PATH, O_WRONLY | O_CREAT | O_TRUNC, 644);
	if (ERR_ALLOC_MEM(f)) {
		ILI_ERR("Failed to open the file at %ld.\n", PTR_ERR(f));
		ret = -1;
		goto out;
	}

	ret = ili_ice_mode_ctrl(ENABLE, mcu);
	if (ret < 0)
		goto out;

	if (user) {
		start_addr = 0x0;
		end_addr = 0x1FFFF;
	} else {
		start_addr = start;
		end_addr = end;
	}

	length = end_addr - start_addr + 1;
	ILI_INFO("len = %d\n", length);

	buf = vmalloc(length * sizeof(u8));
	if (ERR_ALLOC_MEM(buf)) {
		ILI_ERR("Failed to allocate buf memory, %ld\n", PTR_ERR(buf));
		filp_close(f, NULL);
		ret = -1;
		goto out;
	}

	ret = ilitek_tddi_fw_read_flash_data(start_addr, end_addr, buf, length);
	if (ret < 0)
		goto out;

	old_fs = get_fs();
	set_fs(get_ds());
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(f, buf, length, &pos);
	set_fs(old_fs);
	filp_close(f, NULL);
	ipio_vfree((void **)&buf);

out:
	ili_ice_mode_ctrl(DISABLE, mcu);
	ILI_INFO("Dump flash %s\n", (ret < 0) ? "FAIL" : "SUCCESS");
	return ret;
}

static void ilitek_tddi_flash_protect(bool enable)
{
	ILI_INFO("%s flash protection\n", enable ? "Enable" : "Disable");

	ilitek_tddi_flash_write_enable();

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
		ILI_ERR("Write cs low failed\n");

	if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, 0x1, 1) < 0)
		ILI_ERR("Write 0x1 failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, 0x0, 1) < 0)
		ILI_ERR("Write 0x0 failed\n");

	switch (ilits->flash_mid) {
	case 0xEF:
		if (ilits->flash_devid == 0x6012 || ilits->flash_devid == 0x6011) {
			if (enable) {
				if (ili_ice_mode_write(FLASH2_ADDR, 0x7E, 1) < 0)
					ILI_ERR("Write 0x7E at %x failed\n", FLASH2_ADDR);
			} else {
				if (ili_ice_mode_write(FLASH2_ADDR, 0x0, 1) < 0)
					ILI_ERR("Write 0x0 at %x failed\n", FLASH2_ADDR);
			}
		}
		break;
	case 0xC8:
		if (ilits->flash_devid == 0x6012 || ilits->flash_devid == 0x6013) {
			if (enable) {
				if (ili_ice_mode_write(FLASH2_ADDR, 0x7A, 1) < 0)
					ILI_ERR("Write 0x7A at %x failed\n", FLASH2_ADDR);
			} else {
				if (ili_ice_mode_write(FLASH2_ADDR, 0x0, 1) < 0)
					ILI_ERR("Write 0x0 at %x failed\n", FLASH2_ADDR);
			}

		}
		break;
	default:
		ILI_ERR("Can't find flash id(0x%x), ignore protection\n", ilits->flash_mid);
		break;
	}

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
		ILI_ERR("Write cs high failed\n");
}

static int ilitek_fw_check_ddi_chunk(u8 *pfw)
{
	int ret = 0;
	u8 cmd[7] = {0x0};
	u32 start = 0x0, end = 0x0, len = 0x0;
	bool hex_info = ilits->info_from_hex;

	if (ili_ic_get_fw_ver() < 0) {
		ILI_INFO("Failed to get fw ver, reading flash by dma instead\n");
		goto dma;
	}

	if (!fbi[DDI].start || !fbi[DDI].end)
		return UPDATE_PASS;

	/* To make sure if flash has data. */
	if (hex_info)
		ilits->info_from_hex = DISABLE;

	/* Command for getting ddi info from firmware. */
	cmd[0] = CMD_GET_FLASH_DATA;		//cmd
	cmd[1] = DDI_RSV_BK_SIZE & 0xFF;	//len L byte
	cmd[2] = (DDI_RSV_BK_SIZE >> 8) & 0xFF;	//len H byte

	/* Read customer reserved block */
	start = DDI_RSV_BK_ST_ADDR;
	cmd[3] = (start & 0xFF);       	//addr L byte
	cmd[4] = (start >> 8) & 0xFF ;  //addr M byte
	cmd[5] = (start >> 16) & 0xFF; 	//addr H byte
	cmd[6] = ili_calc_packet_checksum(cmd, sizeof(cmd) - 1);
	ILI_INFO("Read customer reserved block: start addr = 0x%X, len = 0x%X ", start, DDI_RSV_BK_SIZE);
	if (ilits->wrapper(cmd, sizeof(cmd), fw_flash_tmp, sizeof(fw_flash_tmp), OFF, ON) < 0) {
		ILI_ERR("Failed to read customer reserved block data from fw\n");
		goto dma;
	}
	ipio_memcpy(&pfw[start], fw_flash_tmp + 6, DDI_RSV_BK_SIZE, MAX_HEX_FILE_SIZE - start);

	/* Read mp data block */
	start = DDI_RSV_BK_END_ADDR - DDI_RSV_BK_SIZE + 1;
	cmd[3] = (start & 0xFF);       	//addr L byte
	cmd[4] = (start >> 8) & 0xFF ;  //addr M byte
	cmd[5] = (start >> 16) & 0xFF; 	//addr H byte
	cmd[6] = ili_calc_packet_checksum(cmd, sizeof(cmd) - 1);
	ILI_INFO("Read mp data block: start addr = 0x%X, len = 0x%X ", start, DDI_RSV_BK_SIZE);
	if (ilits->wrapper(cmd, sizeof(cmd), fw_flash_tmp, sizeof(fw_flash_tmp), OFF, ON) < 0) {
		ILI_ERR("Failed to read mp data block from fw\n");
		goto dma;
	}
	ipio_memcpy(&pfw[start], fw_flash_tmp + 6, DDI_RSV_BK_SIZE, MAX_HEX_FILE_SIZE - start);

	goto out;

dma:
	ILI_INFO("Obtain block data from dma\n");

	ili_ice_mode_ctrl(ENABLE, OFF);

	/* Read customer reserved block */
	start = DDI_RSV_BK_ST_ADDR;
	end = DDI_RSV_BK_ST_ADDR + DDI_RSV_BK_SIZE - 1;
	len = DDI_RSV_BK_SIZE;
	if (ilitek_tddi_fw_read_flash_data(start, end, fw_flash_tmp, len) < 0) {
		ILI_ERR("Failed to read customer reserved block data from dma\\n");
		ret = UPDATE_FAIL;
		goto out;
	}
	ipio_memcpy(&pfw[start], fw_flash_tmp + 6, DDI_RSV_BK_SIZE, MAX_HEX_FILE_SIZE - start);

	/* Read mp data block */
	start = DDI_RSV_BK_END_ADDR - DDI_RSV_BK_SIZE + 1;
	end = DDI_RSV_BK_END_ADDR;
	len = DDI_RSV_BK_SIZE;
	if (ilitek_tddi_fw_read_flash_data(start, end, fw_flash_tmp, len) < 0) {
		ILI_ERR("Failed to read mp data block from dma\n");
		ret = UPDATE_FAIL;
		goto out;
	}
	ipio_memcpy(&pfw[start], fw_flash_tmp + 6, DDI_RSV_BK_SIZE, MAX_HEX_FILE_SIZE - start);

	ili_ice_mode_ctrl(DISABLE, OFF);
out:
	/*
	 * Since there're two blocks combined with DDI chunk togeter,
	 * we need to extend the addresses where are going to upgrade.
	 */
	fbi[DDI].start = DDI_RSV_BK_ST_ADDR;
	fbi[DDI].end = DDI_RSV_BK_END_ADDR;
	tfd.end_addr = DDI_RSV_BK_END_ADDR;

	if (hex_info)
		ilits->info_from_hex = ENABLE;

	atomic_set(&ilits->cmd_int_check, DISABLE);
	return ret;
}

static int ilitek_tddi_flash_fw_crc_check(u8 *pfw, bool check_fw_ver)
{
	int i, len = 0, crc_byte_len = 4;
	u8 flash_crc[4] = {0};
	u32 start_addr = 0, end_addr = 0;
	u32 hw_crc = 0;
	u32 flash_crc_cb = 0, hex_crc = 0;

	/* Check Flash and HW CRC */
	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		start_addr = fbi[i].start;
		end_addr = fbi[i].end;

		/* Invaild end address */
		if (end_addr == 0)
			continue;

		if (ilitek_tddi_fw_read_flash_data(end_addr - crc_byte_len + 1, end_addr,
					flash_crc, sizeof(flash_crc)) < 0) {
			ILI_ERR("Read Flash failed\n");
			return UPDATE_FAIL;
		}

		flash_crc_cb = flash_crc[0] << 24 | flash_crc[1] << 16 | flash_crc[2] << 8 | flash_crc[3];

		if (ili_fw_read_hw_crc(start_addr, end_addr - start_addr - crc_byte_len + 1, &hw_crc) < 0) {
			ILI_ERR("Read HW CRC failed\n");
			return UPDATE_FAIL;
		}

		ILI_INFO("Block = %d, HW CRC = 0x%06x, Flash CRC = 0x%06x\n", i, hw_crc, flash_crc_cb);

		/* Compare Flash CRC with HW CRC */
		if (flash_crc_cb != hw_crc) {
			ILI_INFO("HW and Flash CRC not matched\n");
			return UPDATE_FAIL;
		}
		memset(flash_crc, 0, sizeof(flash_crc));
	}

	if (check_fw_ver) {
		/* Check FW version */
		ILI_INFO("New FW ver = 0x%x, Current FW ver = 0x%x\n", tfd.new_fw_cb, ilits->chip->fw_ver);
		if (tfd.new_fw_cb != ilits->chip->fw_ver) {
			ILI_INFO("FW version not matched\n");
			return UPDATE_FAIL;
		}
	}


	/* Check Hex and HW CRC */
	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].end == 0)
			continue;

		start_addr = fbi[i].start;
		end_addr = fbi[i].end;

		len = fbi[i].end - fbi[i].start + 1 - 4;
		hex_crc = CalculateCRC32(fbi[i].start, len, pfw);

		if (ili_fw_read_hw_crc(start_addr, end_addr - start_addr - crc_byte_len + 1, &hw_crc) < 0) {
			ILI_ERR("Read HW CRC failed\n");
			return UPDATE_FAIL;
		}

		ILI_INFO("Block = %d, HW CRC = 0x%06x, Hex CRC = 0x%06x\n", i, hw_crc, hex_crc);
		if (hex_crc != hw_crc) {
			ILI_ERR("Hex and HW CRC not matched\n");
			return UPDATE_FAIL;
		}
	}

	ILI_INFO("Flash FW is the same as targe file FW\n");
	return UPDATE_PASS;
}

static int ilitek_tddi_fw_flash_program(u8 *pfw)
{
	u8 buf[512] = {0};
	u32 i = 0, j = 0, addr = 0, k = 0, recv_addr = 0;
	int page = ilits->program_page;
	bool skip = true;

	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].end == 0)
			continue;

		ILI_INFO("Block[%d]: Programing from (0x%x) to (0x%x), tfd.end_addr = 0x%x\n",
				i, fbi[i].start, fbi[i].end, tfd.end_addr);

		for (addr = fbi[i].start; addr < fbi[i].end; addr += page) {
			buf[0] = 0x25;
			buf[3] = 0x04;
			buf[2] = 0x10;
			buf[1] = 0x08;

			for (k = 0; k < page; k++) {
				if (addr + k <= tfd.end_addr)
					buf[4 + k] = pfw[addr + k];
				else
					buf[4 + k] = 0xFF;

				if (buf[4 + k] != 0xFF)
					skip = false;
			}

			if (skip) {
				if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
					ILI_ERR("Write cs high failed\n");
				return UPDATE_FAIL;
			}

			ilitek_tddi_flash_write_enable();

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
				ILI_ERR("Write cs low failed\n");

			if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
				ILI_ERR("Write key failed\n");

			if (ili_ice_mode_write(FLASH2_ADDR, 0x2, 1) < 0)
				ILI_ERR("Write 0x2 failed\n");

			recv_addr = ((addr & 0xFF0000) >> 16) | (addr & 0x00FF00) | ((addr & 0x0000FF) << 16);
			if (ili_ice_mode_write(FLASH2_ADDR, recv_addr, 3) < 0)
				ILI_ERR("Write address failed\n");

			if (ilits->wrapper(buf, page + 4, NULL, 0, OFF, OFF) < 0) {
				ILI_ERR("Failed to program data at start_addr = 0x%X, k = 0x%X, addr = 0x%x\n",
				addr, k, addr + k);
				if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
					ILI_ERR("Write cs high failed\n");
				return UPDATE_FAIL;
			}

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
				ILI_ERR("Write cs high failed\n");

			if (ilits->flash_mid == 0xEF) {
				mdelay(1);
			} else {
				if (ilitek_tddi_flash_poll_busy(TIMEOUT_PROGRAM) < 0)
					return UPDATE_FAIL;
			}
			if (ilits->fw_update_stat != ((j * 100) / tfd.end_addr)) {
				ilits->fw_update_stat = (j * 100) / tfd.end_addr;
				ILI_DBG("Program flahse data .... %d%c", ilits->fw_update_stat, '%');
			}
		}
	}
	return UPDATE_PASS;
}

static int ilitek_tddi_fw_flash_erase(bool mcu)
{
	int ret = 0;
	u32 i = 0, addr = 0, recv_addr = 0;
	bool bk_erase = false;
	bool ice = atomic_read(&ilits->ice_stat);

	if (!ice) {
		if (ili_ice_mode_ctrl(ENABLE, mcu) < 0)
			ILI_ERR("Enable ice mode failed while erasing flash\n");
	}

	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].end == 0)
			continue;

		ILI_INFO("Block[%d]: Erasing from (0x%x) to (0x%x) \n", i, fbi[i].start, fbi[i].end);

		for (addr = fbi[i].start; addr <= fbi[i].end; addr += ilits->flash_sector) {
			ilitek_tddi_flash_write_enable();

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
				ILI_ERR("Write cs low failed\n");

			if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
				ILI_ERR("Write key failed\n");

			if (addr == fbi[AP].start) {
				/* block erase with 64k bytes. */
				if (ili_ice_mode_write(FLASH2_ADDR, 0xD8, 1) < 0)
					ILI_ERR("Write 0xB at %x failed\n", FLASH2_ADDR);
				bk_erase = true;
			} else {
				/* sector erase with 4k bytes. */
				if (ili_ice_mode_write(FLASH2_ADDR, 0x20, 1) < 0)
					ILI_ERR("Write 0x20 at %x failed\n", FLASH2_ADDR);
			}

			recv_addr = ((addr & 0xFF0000) >> 16) | (addr & 0x00FF00) | ((addr & 0x0000FF) << 16);
			if (ili_ice_mode_write(FLASH2_ADDR, recv_addr, 3) < 0)
				ILI_ERR("Write address failed\n");

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
				ILI_ERR("Write cs high failed\n");

			/* Waitint for flash setting ready */
			mdelay(1);

			if (addr == fbi[AP].start)
				ret = ilitek_tddi_flash_poll_busy(TIMEOUT_PAGE);
			else
				ret = ilitek_tddi_flash_poll_busy(TIMEOUT_SECTOR);

			if (ret < 0)
				return UPDATE_FAIL;

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
				ILI_ERR("Write cs high failed\n");

			if (i == AP && tfd.is80k && bk_erase) {
				addr = 0x10000 - 0x1000;
				bk_erase = false;
			}
		}
	}
	if (!ice) {
		if (ili_ice_mode_ctrl(DISABLE, mcu) < 0)
			ILI_ERR("Disable ice mode failed after erase flash\n");
	}
	return UPDATE_PASS;
}

static int ilitek_fw_flash_upgrade(u8 *pfw, bool mcu)
{
	int ret = UPDATE_PASS;
	if (ili_reset_ctrl(ilits->reset) < 0) {
		ILI_ERR("TP reset failed during flash progam\n");
		return -EFW_REST;
	}

	ilitek_fw_check_ddi_chunk(pfw);

	ret = ili_ice_mode_ctrl(ENABLE, mcu);
	if (ret < 0)
		return -EFW_ICE_MODE;

	ret = ilitek_tddi_flash_fw_crc_check(pfw, true);
	if (ret == UPDATE_PASS) {
		if (ili_ice_mode_ctrl(DISABLE, mcu) < 0) {
			ILI_ERR("Disable ice mode failed, call reset instead\n");
			if (ili_reset_ctrl(ilits->reset) < 0) {
				ILI_ERR("TP reset failed during flash progam\n");
				return -EFW_REST;
			}
			return UPDATE_PASS;
		}
		return UPDATE_PASS;
	} else {
		ILI_INFO("Flash FW and target file FW are different, do upgrade\n");
	}

	ret = ilitek_tddi_fw_flash_erase(mcu);
	if (ret == UPDATE_FAIL)
		return -EFW_ERASE;

	ret = ilitek_tddi_fw_flash_program(pfw);
	if (ret == UPDATE_FAIL)
		return -EFW_PROGRAM;

	ret = ilitek_tddi_flash_fw_crc_check(pfw, false);
	if (ret == UPDATE_FAIL)
		return -EFW_CRC;

	/* We do have to reset chip in order to move new code from flash to iram. */
	if (ili_reset_ctrl(ilits->reset) < 0) {
		ILI_ERR("TP reset failed after flash progam\n");
		ret = -EFW_REST;
	}
	return ret;
}

static int ilitek_fw_calc_file_crc(u8 *pfw)
{
	int i;
	u32 ex_addr, data_crc, file_crc;

	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].end == 0)
			continue;
		ex_addr = fbi[i].end;
		data_crc = CalculateCRC32(fbi[i].start, fbi[i].len - 4, pfw);
		file_crc = pfw[ex_addr - 3] << 24 | pfw[ex_addr - 2] << 16 | pfw[ex_addr - 1] << 8 | pfw[ex_addr];
		ILI_DBG("data crc = %x, file crc = %x\n", data_crc, file_crc);
		if (data_crc != file_crc) {
			ILI_ERR("Content of fw file is broken. (%d, %x, %x)\n",
				i, data_crc, file_crc);
			return -1;
		}
	}

	ILI_INFO("Content of fw file is correct\n");
	return 0;
}

static void ilitek_tddi_fw_update_block_info(u8 *pfw)
{
	u32 fw_info_addr = 0, fw_mp_ver_addr = 0;

	fbi[AP].name = "AP";
	fbi[DATA].name = "DATA";
	fbi[TUNING].name = "TUNING";
	fbi[MP].name = "MP";
	fbi[GESTURE].name = "GESTURE";

	/* upgrade mode define */
	fbi[DATA].mode = fbi[AP].mode = fbi[TUNING].mode = AP;
	fbi[MP].mode = MP;
	fbi[GESTURE].mode = GESTURE;

	if (fbi[AP].end > (64*K))
		tfd.is80k = true;

	/* Copy fw info */
	fw_info_addr = fbi[AP].end - INFO_HEX_ST_ADDR;
	ILI_INFO("Parsing hex info start addr = 0x%x\n", fw_info_addr);
	ipio_memcpy(ilits->fw_info, (pfw + fw_info_addr), sizeof(ilits->fw_info), sizeof(ilits->fw_info));

	/* copy fw mp ver */
	fw_mp_ver_addr = fbi[MP].end - INFO_MP_HEX_ADDR;
	ILI_INFO("Parsing hex mp ver addr = 0x%x\n", fw_mp_ver_addr);
	ipio_memcpy(ilits->fw_mp_ver, pfw + fw_mp_ver_addr, sizeof(ilits->fw_mp_ver), sizeof(ilits->fw_mp_ver));

	/* copy fw core ver */
	ilits->chip->core_ver = (ilits->fw_info[68] << 24) | (ilits->fw_info[69] << 16) |
			(ilits->fw_info[70] << 8) | ilits->fw_info[71];
	ILI_INFO("New FW Core version = %x\n", ilits->chip->core_ver);

	/* Get hex fw vers */
	tfd.new_fw_cb = (ilits->fw_info[48] << 24) | (ilits->fw_info[49] << 16) |
			(ilits->fw_info[50] << 8) | ilits->fw_info[51];

	/* Get hex report info block*/
	ipio_memcpy(&ilits->rib, ilits->fw_info, sizeof(ilits->rib), sizeof(ilits->rib));
	ILI_INFO("report_info_block : nReportByPixel = %d, nIsHostDownload = %d, nIsSPIICE = %d, nIsSPISLAVE = %d\n",
		ilits->rib.nReportByPixel, ilits->rib.nIsHostDownload, ilits->rib.nIsSPIICE, ilits->rib.nIsSPISLAVE);
	ILI_INFO("report_info_block : nIsI2C = %d, nReserved00 = %d, nReserved01 = %x, nReserved02 = %x,  nReserved03 = %x\n",
		ilits->rib.nIsI2C, ilits->rib.nReserved00, ilits->rib.nReserved01, ilits->rib.nReserved02, ilits->rib.nReserved03);

	/* Calculate update address */
	ILI_INFO("New FW ver = 0x%x\n", tfd.new_fw_cb);
	ILI_INFO("star_addr = 0x%06X, end_addr = 0x%06X, Block Num = %d\n", tfd.start_addr, tfd.end_addr, tfd.block_number);
}

static int ilitek_tddi_fw_ili_convert(u8 *pfw)
{
	int i, size, blk_num = 0, blk_map = 0, num = 0;
	int b0_addr = 0, b0_num = 0;

	if (ERR_ALLOC_MEM(ilits->md_fw_ili))
		return -ENOMEM;

	CTPM_FW = ilits->md_fw_ili;
	size = ilits->md_fw_ili_size;

	if (size < ILI_FILE_HEADER || size > MAX_HEX_FILE_SIZE) {
		ILI_ERR("size of ILI file is invalid\n");
		return -EINVAL;
	}

	/* Check if it's old version of ILI format. */
	if (CTPM_FW[22] == 0xFF && CTPM_FW[23] == 0xFF &&
		CTPM_FW[24] == 0xFF && CTPM_FW[25] == 0xFF) {
		ILI_ERR("Invaild ILI format, abort!\n");
		return -EINVAL;
	}

	blk_num = CTPM_FW[131];
	blk_map = (CTPM_FW[129] << 8) | CTPM_FW[130];
	ILI_INFO("Parsing ILI file, block num = %d, block mapping = %x\n", blk_num, blk_map);

	if (blk_num > (FW_BLOCK_INFO_NUM - 1) || !blk_num || !blk_map) {
		ILI_ERR("Number of block or block mapping is invalid, abort!\n");
		return -EINVAL;
	}

	memset(fbi, 0x0, sizeof(fbi));
	tfd.start_addr = 0;
	tfd.end_addr = 0;
	tfd.hex_tag = BLOCK_TAG_AF;

	/* Parsing block info */
	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		/* B0 tag */
		b0_addr = (CTPM_FW[4 + i * 4] << 16) | (CTPM_FW[5 + i * 4] << 8) | (CTPM_FW[6 + i * 4]);
		b0_num = CTPM_FW[7 + i * 4];
		if ((b0_num != 0) && (b0_addr != 0x000000))
			fbi[b0_num].fix_mem_start = b0_addr;

		/* AF tag */
		num = i + 1;
		if (((blk_map >> i) & 0x01) == 0x01) {
			fbi[num].start = (CTPM_FW[132 + i * 6] << 16) | (CTPM_FW[133 + i * 6] << 8) | CTPM_FW[134 + i * 6];
			fbi[num].end = (CTPM_FW[135 + i * 6] << 16) | (CTPM_FW[136 + i * 6] << 8) |  CTPM_FW[137 + i * 6];

			if (fbi[num].fix_mem_start == 0)
				fbi[num].fix_mem_start = INT_MAX;

			fbi[num].len = fbi[num].end - fbi[num].start + 1;
			ILI_INFO("Block[%d]: start_addr = %x, end = %x, fix_mem_start = 0x%x\n", num, fbi[num].start,
								fbi[num].end, fbi[num].fix_mem_start);
		}
	}

	memcpy(pfw, CTPM_FW + ILI_FILE_HEADER, size - ILI_FILE_HEADER);

	if (ilitek_fw_calc_file_crc(pfw) < 0)
		return -1;

	tfd.block_number = blk_num;
	tfd.end_addr = size - ILI_FILE_HEADER;
	return 0;
}

static int ilitek_tddi_fw_hex_convert(u8 *phex, int size, u8 *pfw)
{
	int block = 0;
	u32 i = 0, j = 0, k = 0, num = 0;
	u32 len = 0, addr = 0, type = 0;
	u32 start_addr = 0x0, end_addr = 0x0, ex_addr = 0;
	u32 offset;

	memset(fbi, 0x0, sizeof(fbi));

	/* Parsing HEX file */
	for (; i < size;) {
		len = HexToDec(&phex[i + 1], 2);
		addr = HexToDec(&phex[i + 3], 4);
		type = HexToDec(&phex[i + 7], 2);

		if (type == 0x04) {
			ex_addr = HexToDec(&phex[i + 9], 4);
		} else if (type == 0x02) {
			ex_addr = HexToDec(&phex[i + 9], 4);
			ex_addr = ex_addr >> 12;
		} else if (type == BLOCK_TAG_AF) {
			/* insert block info extracted from hex */
			tfd.hex_tag = type;
			if (tfd.hex_tag == BLOCK_TAG_AF)
				num = HexToDec(&phex[i + 9 + 6 + 6], 2);
			else
				num = 0xFF;

			if (num > (FW_BLOCK_INFO_NUM - 1)) {
				ILI_ERR("ERROR! block num is larger than its define (%d, %d)\n",
						num, FW_BLOCK_INFO_NUM - 1);
				return -EINVAL;
			}

			fbi[num].start = HexToDec(&phex[i + 9], 6);
			fbi[num].end = HexToDec(&phex[i + 9 + 6], 6);
			fbi[num].fix_mem_start = INT_MAX;
			fbi[num].len = fbi[num].end - fbi[num].start + 1;
			ILI_INFO("Block[%d]: start_addr = %x, end = %x", num, fbi[num].start, fbi[num].end);

			block++;
		} else if (type == BLOCK_TAG_B0 && tfd.hex_tag == BLOCK_TAG_AF) {
			num = HexToDec(&phex[i + 9 + 6], 2);

			if (num > (FW_BLOCK_INFO_NUM - 1)) {
				ILI_ERR("ERROR! block num is larger than its define (%d, %d)\n",
						num, FW_BLOCK_INFO_NUM - 1);
				return -EINVAL;
			}

			fbi[num].fix_mem_start = HexToDec(&phex[i + 9], 6);
			ILI_INFO("Tag 0xB0: change Block[%d] to addr = 0x%x\n", num, fbi[num].fix_mem_start);
		}

		addr = addr + (ex_addr << 16);

		if (phex[i + 1 + 2 + 4 + 2 + (len * 2) + 2] == 0x0D)
			offset = 2;
		else
			offset = 1;

		if (addr > MAX_HEX_FILE_SIZE) {
			ILI_ERR("Invalid hex format %d\n", addr);
			return -1;
		}

		if (type == 0x00) {
			end_addr = addr + len;
			if (addr < start_addr)
				start_addr = addr;
			/* fill data */
			for (j = 0, k = 0; j < (len * 2); j += 2, k++)
				pfw[addr + k] = HexToDec(&phex[i + 9 + j], 2);
		}
		i += 1 + 2 + 4 + 2 + (len * 2) + 2 + offset;
	}

	if (ilitek_fw_calc_file_crc(pfw) < 0)
		return -1;

	tfd.start_addr = start_addr;
	tfd.end_addr = end_addr;
	tfd.block_number = block;
	return 0;
}

static int ilitek_tdd_fw_hex_open(u8 op, u8 *pfw)
{
	int ret = 0, fsize = 0;
	const struct firmware *fw = NULL;
	struct file *f = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;

	ILI_INFO("Open file method = %s, path = %s\n",
		op ? "FILP_OPEN" : "REQUEST_FIRMWARE",
		op ? ilits->md_fw_filp_path : ilits->md_fw_rq_path);

	switch (op) {
	case REQUEST_FIRMWARE:
		if (request_firmware(&fw, ilits->md_fw_rq_path, ilits->dev) < 0) {
			ILI_ERR("Request firmware failed, try again\n");
			if (request_firmware(&fw, ilits->md_fw_rq_path, ilits->dev) < 0) {
				ILI_ERR("Request firmware failed after retry\n");
				ret = -1;
				goto out;
			}
		}

		fsize = fw->size;
		ILI_INFO("fsize = %d\n", fsize);
		if (fsize <= 0) {
			ILI_ERR("The size of file is zero\n");
			release_firmware(fw);
			ret = -1;
			goto out;
		}

		ilits->tp_fw.size = 0;
		ilits->tp_fw.data = vmalloc(fsize);
		if (ERR_ALLOC_MEM(ilits->tp_fw.data)) {
			ILI_ERR("Failed to allocate tp_fw by vmalloc, try again\n");
			ilits->tp_fw.data = vmalloc(fsize);
			if (ERR_ALLOC_MEM(ilits->tp_fw.data)) {
				ILI_ERR("Failed to allocate tp_fw after retry\n");
				release_firmware(fw);
				ret = -ENOMEM;
				goto out;
			}
		}

		/* Copy fw data got from request_firmware to global */
		ipio_memcpy((u8 *)ilits->tp_fw.data, fw->data, fsize * sizeof(*fw->data), fsize);
		ilits->tp_fw.size = fsize;
		release_firmware(fw);
		break;
	case FILP_OPEN:
		f = filp_open(ilits->md_fw_filp_path, O_RDONLY, 0644);
		if (ERR_ALLOC_MEM(f)) {
			ILI_ERR("Failed to open the file at %ld\n", PTR_ERR(f));
			ret = -1;
			goto out;
		}

		fsize = f->f_inode->i_size;
		ILI_INFO("fsize = %d\n", fsize);
		if (fsize <= 0) {
			ILI_ERR("The size of file is invaild\n");
			filp_close(f, NULL);
			ret = -1;
			goto out;
		}

		ipio_vfree((void **)&(ilits->tp_fw.data));
		ilits->tp_fw.size = 0;
		ilits->tp_fw.data = vmalloc(fsize);
		if (ERR_ALLOC_MEM(ilits->tp_fw.data)) {
			ILI_ERR("Failed to allocate tp_fw by vmalloc, try again\n");
			ilits->tp_fw.data = vmalloc(fsize);
			if (ERR_ALLOC_MEM(ilits->tp_fw.data)) {
				ILI_ERR("Failed to allocate tp_fw after retry\n");
				filp_close(f, NULL);
				ret = -ENOMEM;
				goto out;
			}
		}

		/* ready to map user's memory to obtain data by reading files */
		old_fs = get_fs();
		set_fs(get_ds());
		set_fs(KERNEL_DS);
		pos = 0;
		vfs_read(f, (u8 *)ilits->tp_fw.data, fsize, &pos);
		set_fs(old_fs);
		filp_close(f, NULL);
		ilits->tp_fw.size = fsize;
		break;
	default:
		ILI_ERR("Unknown open file method, %d\n", op);
		break;
	}

	if (ERR_ALLOC_MEM(ilits->tp_fw.data) || ilits->tp_fw.size <= 0) {
		ILI_ERR("fw data/size is invaild\n");
		ret = -1;
		goto out;
	}

	/* Convert hex and copy data from tp_fw.data to pfw */
	if (ilitek_tddi_fw_hex_convert((u8 *)ilits->tp_fw.data, ilits->tp_fw.size, pfw) < 0) {
		ILI_ERR("Convert hex file failed\n");
		ret = -1;
	}

out:
	ipio_vfree((void **)&(ilits->tp_fw.data));
	return ret;
}

int ili_fw_upgrade(int op)
{
	int i, ret = 0, retry = 3;

	if (!ilits->boot || ilits->force_fw_update || ERR_ALLOC_MEM(pfw)) {
		if (ERR_ALLOC_MEM(pfw)) {
			ipio_vfree((void **)&pfw);
			pfw = vmalloc(MAX_HEX_FILE_SIZE * sizeof(u8));
			if (ERR_ALLOC_MEM(pfw)) {
				ILI_ERR("Failed to allocate pfw memory, %ld\n", PTR_ERR(pfw));
				ipio_vfree((void **)&pfw);
				ret = -ENOMEM;
				goto out;
			}
		}

		for (i = 0; i < MAX_HEX_FILE_SIZE; i++)
			pfw[i] = 0xFF;

		if (ilitek_tdd_fw_hex_open(op, pfw) < 0) {
			ILI_ERR("Open hex file fail, try upgrade from ILI file\n");

			/*
			 * Users might not be aware of a broken hex file when recovering
			 * fw from ILI file. We should force them to check
			 * hex files they attempt to update via device node.
			 */
			if (ilits->node_update) {
				ILI_ERR("Ignore update from ILI file\n");
				ipio_vfree((void **)&pfw);
				return -EFW_CONVERT_FILE;
			}

			if (ilitek_tddi_fw_ili_convert(pfw) < 0) {
				ILI_ERR("Convert ILI file error\n");
				ret = -EFW_CONVERT_FILE;
				goto out;
			}
		}
		ilitek_tddi_fw_update_block_info(pfw);

		if (ilits->chip->core_ver >= CORE_VER_1470 && ilits->rib.nIsHostDownload == 1) {
			ILI_ERR("hex file interface no match error\n");
			ret = -EFW_INTERFACE;
			goto out;
		}
	}

	do {
		ret = ilitek_fw_flash_upgrade(pfw, OFF);
		if (ret == UPDATE_PASS)
			break;

		ILI_ERR("Upgrade failed, do retry!\n");
	} while (--retry > 0);

	if (ret != UPDATE_PASS) {
		ILI_ERR("Failed to upgrade fw %d times, erasing flash\n", retry);
		if (ilitek_tddi_fw_flash_erase(OFF) < 0)
			ILI_ERR("Failed to erase flash\n");
		if (ili_reset_ctrl(ilits->reset) < 0)
			ILI_ERR("TP reset failed after erase flash\n");
	}

out:
	if (ret < 0)
		ilits->info_from_hex = DISABLE;

	if (atomic_read(&ilits->ice_stat))
		ili_ice_mode_ctrl(DISABLE, OFF);

	ili_ic_get_core_ver();
	ili_ic_get_protocl_ver();
	ili_ic_get_fw_ver();
	ili_ic_get_tp_info();
	ili_ic_get_panel_info();
	ili_ic_func_ctrl_reset();

	if (!ilits->info_from_hex)
		ilits->info_from_hex = ENABLE;

	return ret;
}

struct flash_table {
	u16 mid;
	u16 dev_id;
	int mem_size;
	int program_page;
	int sector;
} flash_t[] = {
	[0] = {0x00, 0x0000, (256 * K), 256, (4 * K)}, /* Default */
	[1] = {0xEF, 0x6011, (128 * K), 256, (4 * K)}, /* W25Q10EW	*/
	[2] = {0xEF, 0x6012, (256 * K), 256, (4 * K)}, /* W25Q20EW	*/
	[3] = {0xC8, 0x6012, (256 * K), 256, (4 * K)}, /* GD25LQ20B */
	[4] = {0xC8, 0x6013, (512 * K), 256, (4 * K)}, /* GD25LQ40 */
	[5] = {0x85, 0x6013, (4 * M), 256, (4 * K)},
	[6] = {0xC2, 0x2812, (256 * K), 256, (4 * K)},
	[7] = {0x1C, 0x3812, (256 * K), 256, (4 * K)},
};

void ili_fw_read_flash_info(bool mcu)
{
	int i = 0;
	u8 buf[4] = {0};
	u8 cmd = 0x9F;
	u32 tmp = 0;
	u16 flash_id = 0, flash_mid = 0;
	bool ice = atomic_read(&ilits->ice_stat);

	if (!ice) {
		if (ili_ice_mode_ctrl(ENABLE, mcu) < 0)
			ILI_ERR("Enable ice mode failed while reading flash info\n");
	}

	if (ili_ice_mode_bit_mask_write(FLASH0_ADDR, FLASH0_reg_rx_dual, (0 << 24)) < 0)
		ILI_ERR("Write %lu at %x failed\n", FLASH0_reg_rx_dual, FLASH0_ADDR);

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
		ILI_ERR("Write cs low failed\n");

	if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, cmd, 1) < 0)
		ILI_ERR("Write 0x9F failed\n");

	for (i = 0; i < ARRAY_SIZE(buf); i++) {
		if (ili_ice_mode_write(FLASH2_ADDR, 0xFF, 1) < 0)
			ILI_ERR("Write dummy failed\n");

		if (ili_ice_mode_read(FLASH4_ADDR, &tmp, sizeof(u8)) < 0)
			ILI_ERR("Read flash info error\n");

		buf[i] = tmp;
	}

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
		ILI_ERR("Write cs high failed\n"); /* CS high */

	flash_mid = buf[0];
	flash_id = buf[1] << 8 | buf[2];

	for (i = 0; i < ARRAY_SIZE(flash_t); i++) {
		if (flash_mid == flash_t[i].mid && flash_id == flash_t[i].dev_id) {
			ilits->flash_mid = flash_t[i].mid;
			ilits->flash_devid = flash_t[i].dev_id;
			ilits->program_page = flash_t[i].program_page;
			ilits->flash_sector = flash_t[i].sector;
			break;
		}
	}

	if (i >= ARRAY_SIZE(flash_t)) {
		ILI_INFO("Not found flash id in tab, use default\n");
		ilits->flash_mid = flash_t[0].mid;
		ilits->flash_devid = flash_t[0].dev_id;
		ilits->program_page = flash_t[0].program_page;
		ilits->flash_sector = flash_t[0].sector;
	}

	ILI_INFO("Flash MID = %x, Flash DEV_ID = %x\n", ilits->flash_mid, ilits->flash_devid);
	ILI_INFO("Flash program page = %d\n", ilits->program_page);
	ILI_INFO("Flash sector = %d\n", ilits->flash_sector);

	ilitek_tddi_flash_protect(DISABLE);

	if (!ice) {
		if (ili_ice_mode_ctrl(DISABLE, mcu) < 0)
			ILI_ERR("Disable ice mode failed while reading flash info\n");
	}
}