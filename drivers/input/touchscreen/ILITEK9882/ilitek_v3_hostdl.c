/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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

static int calc_hw_dma_crc(u32 start_addr, u32 block_size)
{
	int count = 50;
	u32 busy = 0;

	if (ilits->chip->dma_reset) {
		ILI_DBG("operate dma reset in reg after tp reset\n");
		if (ili_ice_mode_write(0x40040, 0x00800000, 4) < 0)
			ILI_ERR("Failed to open DMA reset\n");
		if (ili_ice_mode_write(0x40040, 0x00000000, 4) < 0)
			ILI_ERR("Failed to close DMA reset\n");
	}
	/* dma1 src1 address */
	if (ili_ice_mode_write(0x072104, start_addr, 4) < 0)
		ILI_ERR("Write dma1 src1 address failed\n");
	/* dma1 src1 format */
	if (ili_ice_mode_write(0x072108, 0x80000001, 4) < 0)
		ILI_ERR("Write dma1 src1 format failed\n");
	/* dma1 dest address */
	if (ili_ice_mode_write(0x072114, 0x0002725C, 4) < 0)
		ILI_ERR("Write dma1 src1 format failed\n");
	/* dma1 dest format */
	if (ili_ice_mode_write(0x072118, 0x80000000, 4) < 0)
		ILI_ERR("Write dma1 dest format failed\n");
	/* Block size*/
	if (ili_ice_mode_write(0x07211C, block_size, 4) < 0)
		ILI_ERR("Write block size (%d) failed\n", block_size);
	/* crc off */
	if (ili_ice_mode_write(0x041016, 0x00, 1) < 0)
		ILI_INFO("Write crc of failed\n");
	/* dma crc */
	if (ili_ice_mode_write(0x041017, 0x03, 1) < 0)
		ILI_ERR("Write dma 1 crc failed\n");
	/* crc on */
	if (ili_ice_mode_write(0x041016, 0x01, 1) < 0)
		ILI_ERR("Write crc on failed\n");
	/* Dma1 stop */
	if (ili_ice_mode_write(0x072100, 0x02000000, 4) < 0)
		ILI_ERR("Write dma1 stop failed\n");
	/* clr int */
	if (ili_ice_mode_write(0x048006, 0x2, 1) < 0)
		ILI_ERR("Write clr int failed\n");
	/* Dma1 start */
	if (ili_ice_mode_write(0x072100, 0x01000000, 4) < 0)
		ILI_ERR("Write dma1 start failed\n");

	/* Polling BIT0 */
	while (count > 0) {
		mdelay(1);
		if (ili_ice_mode_read(0x048006, &busy, sizeof(u8)) < 0)
			ILI_ERR("Read busy error\n");
		ILI_DBG("busy = %x\n", busy);
		if ((busy & 0x02) == 2)
			break;
		count--;
	}

	if (count <= 0) {
		ILI_ERR("BIT0 is busy\n");
		return -1;
	}

	if (ili_ice_mode_read(0x04101C, &busy, sizeof(u32)) < 0) {
		ILI_ERR("Read dma crc error\n");
		return -1;
	}
	return busy;
}

static int ilitek_tddi_fw_iram_read(u8 *buf, u32 start, int len)
{
	int limit = SPI_RX_BUF_SIZE;
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
		ILI_INFO("Reading iram data .... %d%c", ilits->fw_update_stat, '%');
	}
	return 0;
}

int ili_fw_dump_iram_data(u32 start, u32 end, bool save, bool mcu)
{
	struct file *f = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	int i, ret = 0;
	int len, tmp = debug_en;
	bool ice = atomic_read(&ilits->ice_stat);

	if (!ice) {
		ret = ili_ice_mode_ctrl(ENABLE, mcu);
		if (ret < 0) {
			ILI_ERR("Enable ice mode failed\n");
			return ret;
		}
	}

	len = end - start + 1;

	if (len > MAX_HEX_FILE_SIZE) {
		ILI_ERR("len is larger than buffer, abort\n");
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < MAX_HEX_FILE_SIZE; i++)
		ilits->update_buf[i] = 0xFF;

	ret = ilitek_tddi_fw_iram_read(ilits->update_buf, start, len);
	if (ret < 0) {
		ILI_ERR("Read IRAM data failed\n");
		goto out;
	}

	if (save) {
		f = filp_open(DUMP_IRAM_PATH, O_WRONLY | O_CREAT | O_TRUNC, 644);
		if (ERR_ALLOC_MEM(f)) {
			ILI_ERR("Failed to open the file at %ld.\n", PTR_ERR(f));
			ret = -ENOMEM;
			goto out;
		}

		old_fs = get_fs();
		set_fs(get_ds());
		set_fs(KERNEL_DS);
		pos = 0;
		vfs_write(f, ilits->update_buf, len, &pos);
		set_fs(old_fs);
		filp_close(f, NULL);
		ILI_INFO("Save iram data to %s\n", DUMP_IRAM_PATH);
	} else {
		debug_en = DEBUG_ALL;
		ili_dump_data(ilits->update_buf, 8, len, 0, "IRAM");
		debug_en = tmp;
	}

out:
	if (!ice) {
		if (ili_ice_mode_ctrl(DISABLE, mcu) < 0)
			ILI_ERR("Enable ice mode failed after code reset\n");
	}

	ILI_INFO("Dump IRAM %s\n", (ret < 0) ? "FAIL" : "SUCCESS");
	return ret;
}

static int ilitek_tddi_fw_iram_program(u32 start, u8 *w_buf, u32 w_len, u32 split_len)
{
	int i = 0, j = 0, addr = 0;
	u32 end = start + w_len;
	bool fix_4_alignment = false;

	if (split_len % 4 > 0)
		ILI_ERR("Since split_len must be four-aligned, it must be a multiple of four");

	if (split_len != 0) {
		for (addr = start, i = 0; addr < end; addr += split_len, i += split_len) {
			if ((addr + split_len) > end) {
				split_len = end - addr;
				if (split_len % 4 != 0)
					fix_4_alignment = true;
			}

			ilits->update_buf[0] = SPI_WRITE;
			ilits->update_buf[1] = 0x25;
			ilits->update_buf[2] = (char)((addr & 0x000000FF));
			ilits->update_buf[3] = (char)((addr & 0x0000FF00) >> 8);
			ilits->update_buf[4] = (char)((addr & 0x00FF0000) >> 16);

			for (j = 0; j < split_len; j++)
				ilits->update_buf[5 + j] = w_buf[i + j];

			if (fix_4_alignment) {
				ILI_INFO("org split_len = 0x%X\n", split_len);
				ILI_INFO("idev->update_buf[5 + 0x%X] = 0x%X\n", split_len - 4, ilits->update_buf[5 + split_len - 4]);
				ILI_INFO("idev->update_buf[5 + 0x%X] = 0x%X\n", split_len - 3, ilits->update_buf[5 + split_len - 3]);
				ILI_INFO("idev->update_buf[5 + 0x%X] = 0x%X\n", split_len - 2, ilits->update_buf[5 + split_len - 2]);
				ILI_INFO("idev->update_buf[5 + 0x%X] = 0x%X\n", split_len - 1, ilits->update_buf[5 + split_len - 1]);
				for (j = 0; j < (4 - (split_len % 4)); j++) {
					ilits->update_buf[5 + j + split_len] = 0xFF;
					ILI_INFO("idev->update_buf[5 + 0x%X] = 0x%X\n", j + split_len, ilits->update_buf[5 + j + split_len]);
				}

				ILI_INFO("split_len %% 4 = %d\n", split_len % 4);
				split_len = split_len + (4 - (split_len % 4));
				ILI_INFO("fix split_len = 0x%X\n", split_len);
			}
			if (ilits->spi_write_then_read(ilits->spi, ilits->update_buf, split_len + 5, NULL, 0)) {
				ILI_ERR("Failed to write data via SPI in host download (%x)\n", split_len + 5);
				return -EIO;
			}
			ilits->fw_update_stat = (i * 100) / w_len;
		}
	} else {
		for (i = 0; i < MAX_HEX_FILE_SIZE; i++)
			ilits->update_buf[i] = 0xFF;

		ilits->update_buf[0] = SPI_WRITE;
		ilits->update_buf[1] = 0x25;
		ilits->update_buf[2] = (char)((start & 0x000000FF));
		ilits->update_buf[3] = (char)((start & 0x0000FF00) >> 8);
		ilits->update_buf[4] = (char)((start & 0x00FF0000) >> 16);

		memcpy(&ilits->update_buf[5], w_buf, w_len);
		if (w_len % 4 != 0) {
			ILI_INFO("org w_len = %d\n", w_len);
			w_len = w_len + (4 - (w_len % 4));
			ILI_INFO("w_len = %d w_len %% 4 = %d\n", w_len, w_len % 4);
		}
		/* It must be supported by platforms that have the ability to transfer all data at once. */
		if (ilits->spi_write_then_read(ilits->spi, ilits->update_buf, w_len + 5, NULL, 0) < 0) {
			ILI_ERR("Failed to write data via SPI in host download (%x)\n", w_len + 5);
			return -EIO;
		}
	}
	return 0;
}

static int ilitek_tddi_fw_iram_upgrade(u8 *pfw, bool mcu)
{
	int i, ret = UPDATE_PASS;
	u32 mode, crc, dma, iram_crc = 0;
	u8 *fw_ptr = NULL, crc_temp[4], crc_len = 4;
	bool iram_crc_err = false;

	if (!ilits->ddi_rest_done) {
		if (ilits->actual_tp_mode != P5_X_FW_GESTURE_MODE)
			ili_reset_ctrl(ilits->reset);

		ret = ili_ice_mode_ctrl(ENABLE, mcu);
		if (ret < 0)
			return -EFW_ICE_MODE;
	} else {
		/* Restore it if the wq of load_fw_ddi has been called. */
		ilits->ddi_rest_done = false;
	}

	/* Point to pfw with different addresses for getting its block data. */
	fw_ptr = pfw;
	if (ilits->actual_tp_mode == P5_X_FW_TEST_MODE) {

		mode = MP;
	} else if (ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE) {
		mode = GESTURE;
		crc_len = 0;
	} else {
		mode = AP;
	}

	/* Program data to iram acorrding to each block */
	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].mode == mode && fbi[i].len != 0) {
			ILI_DBG("Download %s code from hex 0x%x to IRAM 0x%x, len = 0x%x\n",
					fbi[i].name, fbi[i].start, fbi[i].mem_start, fbi[i].len);

#if SPI_DMA_TRANSFER_SPLIT
			if (ilitek_tddi_fw_iram_program(fbi[i].mem_start, (fw_ptr + fbi[i].start), fbi[i].len, SPI_UPGRADE_LEN) < 0)
				ILI_ERR("IRAM program failed\n");
#else
			if (ilitek_tddi_fw_iram_program(fbi[i].mem_start, (fw_ptr + fbi[i].start), fbi[i].len, 0) < 0)
				ILI_ERR("IRAM program failed\n");
#endif
			crc = CalculateCRC32(fbi[i].start, fbi[i].len - crc_len, fw_ptr);
			dma = calc_hw_dma_crc(fbi[i].mem_start, fbi[i].len - crc_len);

			if (mode != GESTURE) {
				ilitek_tddi_fw_iram_read(crc_temp, (fbi[i].mem_start + fbi[i].len - crc_len), sizeof(crc_temp));
				iram_crc = crc_temp[0] << 24 | crc_temp[1] << 16 | crc_temp[2] << 8 | crc_temp[3];
				if (iram_crc != dma)
					iram_crc_err = true;
			}

			ILI_INFO("%s CRC is %s hex(%x) : dma(%x) : iram(%x), calculation len is 0x%x\n",
				fbi[i].name, ((crc != dma) || (iram_crc_err)) ? "Invalid !" : "Correct !", crc, dma, iram_crc, fbi[i].len - crc_len);

			if ((crc != dma) || iram_crc_err) {
				ILI_ERR("CRC Error! print iram data with first 16 bytes\n");
				ili_fw_dump_iram_data(0x0, 0xF, false, OFF);
				return -EFW_CRC;
			}
		}
	}

	if (ilits->actual_tp_mode != P5_X_FW_GESTURE_MODE) {
		if (ili_reset_ctrl(TP_IC_CODE_RST) < 0) {
			ILI_ERR("TP Code reset failed during iram programming\n");
			ret = -EFW_REST;
			return ret;
		}
	}

	if (ili_ice_mode_ctrl(DISABLE, mcu) < 0) {
		ILI_ERR("Disable ice mode failed after code reset\n");
		ret = -EFW_ICE_MODE;
	}

	/* Waiting for fw ready sending first cmd */
	if (!ilits->info_from_hex || (ilits->chip->core_ver < CORE_VER_1410))
		mdelay(100);

	return ret;
}

static int ilitek_fw_calc_file_crc(u8 *pfw)
{
	int i;
	u32 ex_addr, data_crc, file_crc;

	for (i = 0; i < ARRAY_SIZE(fbi); i++) {
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

static int ilitek_tddi_fw_update_block_info(u8 *pfw)
{
	u32 ges_area_section = 0, ges_info_addr = 0, ges_fw_start = 0, ges_fw_end = 0;
	u32 ap_end = 0, ap_len = 0;
	u32 fw_info_addr = 0, fw_mp_ver_addr = 0;

	if (tfd.hex_tag != BLOCK_TAG_AF) {
		ILI_ERR("HEX TAG is invalid (0x%X)\n", tfd.hex_tag);
		return -EINVAL;
	}

	fbi[AP].mem_start = (fbi[AP].fix_mem_start != INT_MAX) ? fbi[AP].fix_mem_start : 0;
	fbi[DATA].mem_start = (fbi[DATA].fix_mem_start != INT_MAX) ? fbi[DATA].fix_mem_start : DLM_START_ADDRESS;
	fbi[TUNING].mem_start = (fbi[TUNING].fix_mem_start != INT_MAX) ? fbi[TUNING].fix_mem_start :  fbi[DATA].mem_start + fbi[DATA].len;
	fbi[MP].mem_start = (fbi[MP].fix_mem_start != INT_MAX) ? fbi[MP].fix_mem_start : 0;
	fbi[GESTURE].mem_start = (fbi[GESTURE].fix_mem_start != INT_MAX) ? fbi[GESTURE].fix_mem_start :	0;
	fbi[TAG].mem_start = (fbi[TAG].fix_mem_start != INT_MAX) ? fbi[TAG].fix_mem_start : 0;
	fbi[PARA_BACKUP].mem_start = (fbi[PARA_BACKUP].fix_mem_start != INT_MAX) ? fbi[PARA_BACKUP].fix_mem_start : 0;
	fbi[DDI].mem_start = (fbi[DDI].fix_mem_start != INT_MAX) ? fbi[DDI].fix_mem_start : 0;

	/* Parsing gesture info form AP code */
	ges_info_addr = (fbi[AP].end + 1 - 60);
	ges_area_section = (pfw[ges_info_addr + 3] << 24) + (pfw[ges_info_addr + 2] << 16) + (pfw[ges_info_addr + 1] << 8) + pfw[ges_info_addr];
	fbi[GESTURE].mem_start = (pfw[ges_info_addr + 7] << 24) + (pfw[ges_info_addr + 6] << 16) + (pfw[ges_info_addr + 5] << 8) + pfw[ges_info_addr + 4];
	ap_end = (pfw[ges_info_addr + 11] << 24) + (pfw[ges_info_addr + 10] << 16) + (pfw[ges_info_addr + 9] << 8) + pfw[ges_info_addr + 8];

	if (ap_end != fbi[GESTURE].mem_start)
		ap_len = ap_end - fbi[GESTURE].mem_start + 1;

	ges_fw_start = (pfw[ges_info_addr + 15] << 24) + (pfw[ges_info_addr + 14] << 16) + (pfw[ges_info_addr + 13] << 8) + pfw[ges_info_addr + 12];
	ges_fw_end = (pfw[ges_info_addr + 19] << 24) + (pfw[ges_info_addr + 18] << 16) + (pfw[ges_info_addr + 17] << 8) + pfw[ges_info_addr + 16];

	if (ges_fw_end != ges_fw_start)
		fbi[GESTURE].len = ges_fw_end - ges_fw_start;

	/* update gesture address */
	fbi[GESTURE].start = ges_fw_start;

	ILI_INFO("==== Gesture loader info ====\n");
	ILI_INFO("gesture move to ap addr => start = 0x%x, ap_end = 0x%x, ap_len = 0x%x\n", fbi[GESTURE].mem_start, ap_end, ap_len);
	ILI_INFO("gesture hex addr => start = 0x%x, gesture_end = 0x%x, gesture_len = 0x%x\n", ges_fw_start, ges_fw_end, fbi[GESTURE].len);
	ILI_INFO("=============================\n");

	fbi[AP].name = "AP";
	fbi[DATA].name = "DATA";
	fbi[TUNING].name = "TUNING";
	fbi[MP].name = "MP";
	fbi[GESTURE].name = "GESTURE";
	fbi[TAG].name = "TAG";
	fbi[PARA_BACKUP].name = "PARA_BACKUP";
	fbi[DDI].name = "DDI";

	/* upgrade mode define */
	fbi[DATA].mode = fbi[AP].mode = fbi[TUNING].mode = AP;
	fbi[MP].mode = MP;
	fbi[GESTURE].mode = GESTURE;

	if (fbi[AP].end > (64*K))
		tfd.is80k = true;

	/* Copy fw info  */
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

	return 0;
}

static int ilitek_tddi_fw_ili_convert(u8 *pfw)
{
	int i, size, blk_num = 0, blk_map = 0, num;
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
			ILI_DBG("Block[%d]: start_addr = %x, end = %x, fix_mem_start = 0x%x\n", num, fbi[num].start,
								fbi[num].end, fbi[num].fix_mem_start);
			if (num == GESTURE)
				ilits->gesture_load_code = true;
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
			ILI_DBG("Block[%d]: start_addr = %x, end = %x", num, fbi[num].start, fbi[num].end);

			if (num == GESTURE)
				ilits->gesture_load_code = true;

			block++;
		} else if (type == BLOCK_TAG_B0 && tfd.hex_tag == BLOCK_TAG_AF) {
			num = HexToDec(&phex[i + 9 + 6], 2);

			if (num > (FW_BLOCK_INFO_NUM - 1)) {
				ILI_ERR("ERROR! block num is larger than its define (%d, %d)\n",
						num, FW_BLOCK_INFO_NUM - 1);
				return -EINVAL;
			}

			fbi[num].fix_mem_start = HexToDec(&phex[i + 9], 6);
			ILI_DBG("Tag 0xB0: change Block[%d] to addr = 0x%x\n", num, fbi[num].fix_mem_start);
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
			ILI_ERR("The size of file is invaild\n");
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
			ILI_ERR("Failed to open the file, %ld\n", PTR_ERR(f));
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
		ilits->gesture_load_code = false;

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
			 * hex files if they attempt to update via device node.
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

		if (ilitek_tddi_fw_update_block_info(pfw) < 0) {
			ret = -EFW_CONVERT_FILE;
			goto out;
		}

		if (ilits->chip->core_ver >= CORE_VER_1470 && ilits->rib.nIsHostDownload == 0) {
			ILI_ERR("hex file interface no match error\n");
			return -EFW_INTERFACE;
		}
	}

	do {
		ret = ilitek_tddi_fw_iram_upgrade(pfw, OFF);
		if (ret == UPDATE_PASS)
			break;

		ILI_ERR("Upgrade failed, do retry!\n");
	} while (--retry > 0);

	if (ret != UPDATE_PASS) {
		ILI_ERR("Failed to upgrade fw %d times, erasing iram\n", retry);
		if (ili_reset_ctrl(ilits->reset) < 0)
				ILI_ERR("TP reset failed while erasing data\n");
		ilits->xch_num = 0;
		ilits->ych_num = 0;
		return ret;
	}

out:
	ili_ic_get_core_ver();
	ili_ic_get_protocl_ver();
	ili_ic_get_fw_ver();
	ili_ic_get_tp_info();
	ili_ic_get_panel_info();
	ili_ic_func_ctrl_reset();
	return ret;
}

void ili_fw_read_flash_info(bool mcu)
{
    return;
}

void ili_flash_clear_dma(void)
{
    return;
}

void ili_flash_dma_write(u32 start, u32 end, u32 len)
{
    return;
}

int ili_fw_dump_flash_data(u32 start, u32 end, bool user, bool mcu)
{
    return 0;
}