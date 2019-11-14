/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 * Copyright (C) 2019 XiaoMi, Inc.
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
 */

#include "ilitek.h"

#define PROTOCL_VER_NUM		7
static struct ilitek_protocol_info protocol_info[PROTOCL_VER_NUM] = {
	/* length -> fw, protocol, tp, key, panel, core, func, window, cdc, mp_info */
	[0] = {PROTOCOL_VER_500, 4, 4, 14, 30, 5, 5, 2, 8, 3, 8},
	[1] = {PROTOCOL_VER_510, 4, 3, 14, 30, 5, 5, 3, 8, 3, 8},
	[2] = {PROTOCOL_VER_520, 4, 4, 14, 30, 5, 5, 3, 8, 3, 8},
	[3] = {PROTOCOL_VER_530, 9, 4, 14, 30, 5, 5, 3, 8, 3, 8},
	[4] = {PROTOCOL_VER_540, 9, 4, 14, 30, 5, 5, 3, 8, 15, 8},
	[5] = {PROTOCOL_VER_550, 9, 4, 14, 30, 5, 5, 3, 8, 15, 14},
	[6] = {PROTOCOL_VER_560, 9, 4, 14, 30, 5, 5, 3, 8, 15, 14},
};

#define FUNC_CTRL_NUM	16
static struct ilitek_ic_func_ctrl func_ctrl[FUNC_CTRL_NUM] = {
	/* cmd[3] = cmd, func, ctrl */
	[0] = {"sense", {0x1, 0x1, 0x0}, 3},
	[1] = {"sleep", {0x1, 0x2, 0x0}, 3},
	[2] = {"glove", {0x1, 0x6, 0x0}, 3},
	[3] = {"stylus", {0x1, 0x7, 0x0}, 3},
	[4] = {"tp_scan_mode", {0x1, 0x8, 0x0}, 3},
	[5] = {"lpwg", {0x1, 0xA, 0x0}, 3},
	[6] = {"gesture", {0x1, 0xB, 0x3F}, 3},
	[7] = {"phone_cover", {0x1, 0xC, 0x0}, 3},
	[8] = {"finger_sense", {0x1, 0xF, 0x0}, 3},
	[9] = {"phone_cover_window", {0xE, 0x0, 0x0}, 3},
	[10] = {"proximity", {0x1, 0x10, 0x0}, 3},
	[11] = {"plug", {0x1, 0x11, 0x0}, 3},
	[12] = {"edge_palm", {0x1, 0x12, 0x0}, 3},
	[13] = {"lock_point", {0x1, 0x13, 0x0}, 3},
	[14] = {"active", {0x1, 0x14, 0x0}, 3},
	[15] = {"idle", {0x1, 0x19, 0x0}, 3},
};

#define CHIP_SUP_NUM		6
static u32 ic_sup_list[CHIP_SUP_NUM] = {
	[0] = ILI9881_CHIP,
	[1] = ILI9881H_AD,
	[2] = ILI9881H_AE,
	[3] = ILI7807_CHIP,
	[4] = ILI7807G_AA,
	[5] = ILI7807G_AB,
};

static int ilitek_tddi_ic_check_support(u32 pid, u16 id)
{
	int i = 0;

	for (i = 0; i < CHIP_SUP_NUM; i++) {
		if ((pid == ic_sup_list[i]) || (id == ic_sup_list[i]))
		break;
	}

	if (i >= CHIP_SUP_NUM) {
		ipio_err("ERROR, ILITEK CHIP (%x, %x) Not found !!\n", pid, id);
		return -EINVAL;
	}

	ipio_info("ILITEK CHIP (%x, %x) found.\n", pid, id);

	if (id == ILI9881_CHIP) {
		idev->chip->reset_key = 0x00019881;
		idev->chip->wtd_key = 0x9881;
		idev->chip->open_sp_formula = open_sp_formula_ili9881;
		idev->chip->hd_dma_check_crc_off = firmware_hd_dma_crc_off_ili9881;

		/*
		 * Since spi speed has been enabled previsouly whenever enter to ICE mode,
		 * we have to disable if find out the ic is ili9881.
		 */
		if (idev->spi_speed != NULL && idev->chip->spi_speed_ctrl) {
			idev->spi_speed(OFF);
			idev->chip->spi_speed_ctrl = DISABLE;
		}

		if (pid == ILI9881F_AA)
			idev->chip->no_bk_shift = RAWDATA_NO_BK_SHIFT_9881F;
		else
			idev->chip->no_bk_shift = RAWDATA_NO_BK_SHIFT_9881H;
	} else {
		idev->chip->reset_key = 0x00019878;
		idev->chip->wtd_key = 0x9878;
		idev->chip->open_sp_formula = open_sp_formula_ili7807;
		idev->chip->hd_dma_check_crc_off = firmware_hd_dma_crc_off_ili7807;
		idev->chip->no_bk_shift = RAWDATA_NO_BK_SHIFT_9881H;
	}

	idev->chip->max_count = 0x1FFFF;
	idev->chip->open_c_formula = open_c_formula;
	idev->chip->info_addr = INFO_HEX_START_ADDR_64K;
	idev->chip->info_from_hex = true;
	return 0;
}

int ilitek_ice_mode_bit_mask_write(u32 addr, u32 mask, u32 value)
{
	int ret = 0;
	u32 data = 0;

	if (ilitek_ice_mode_read(addr, &data, sizeof(u32)) < 0) {
		ipio_err("Read data error\n");
		return -EINVAL;
	}

	data &= (~mask);
	data |= (value & mask);

	ipio_debug("mask value data = %x\n", data);

	ret = ilitek_ice_mode_write(addr, data, sizeof(u32));
	if (ret < 0)
		ipio_err("Failed to re-write data in ICE mode, ret = %d\n", ret);

	return ret;
}

int ilitek_ice_mode_write(u32 addr, u32 data, int len)
{
	int ret = 0, i;
	u8 txbuf[64] = {0};

	if (!atomic_read(&idev->ice_stat)) {
		ipio_err("ice mode not enabled\n");
		return -EINVAL;
	}

	txbuf[0] = 0x25;
	txbuf[1] = (char)((addr & 0x000000FF) >> 0);
	txbuf[2] = (char)((addr & 0x0000FF00) >> 8);
	txbuf[3] = (char)((addr & 0x00FF0000) >> 16);

	for (i = 0; i < len; i++)
		txbuf[i + 4] = (char)(data >> (8 * i));

	ret = idev->write(txbuf, len + 4);
	if (ret < 0)
		ipio_err("Failed to write data in ice mode, ret = %d\n", ret);

	return ret;
}

int ilitek_ice_mode_read(u32 addr, u32 *data, int len)
{
	int ret = 0;
	u8 *rxbuf = NULL;
	u8 txbuf[4] = {0};

	if (!atomic_read(&idev->ice_stat)) {
		ipio_err("ice mode not enabled\n");
		return -EINVAL;
	}

	txbuf[0] = 0x25;
	txbuf[1] = (char)((addr & 0x000000FF) >> 0);
	txbuf[2] = (char)((addr & 0x0000FF00) >> 8);
	txbuf[3] = (char)((addr & 0x00FF0000) >> 16);

	ret = idev->write(txbuf, 4);
	if (ret < 0)
		goto out;

	rxbuf = kcalloc(len, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(rxbuf)) {
		ipio_err("Failed to allocate rxbuf, %ld\n", PTR_ERR(rxbuf));
		ret = -ENOMEM;
		goto out;
	}

	ret = idev->read(rxbuf, len);
	if (ret < 0)
		goto out;

	if (len == sizeof(u8))
		*data = rxbuf[0];
	else
		*data = (rxbuf[0] | rxbuf[1] << 8 | rxbuf[2] << 16 | rxbuf[3] << 24);

out:
	if (ret < 0)
		ipio_err("Failed to read data in ice mode, ret = %d\n", ret);

	ipio_kfree((void **)&rxbuf);
	return ret;
}

int ilitek_ice_mode_ctrl(bool enable, bool mcu)
{
	int ret = 0, retry = 3;
	u8 cmd_open[4] = {0x25, 0x62, 0x10, 0x18};
	u8 cmd_close[4] = {0x1B, 0x62, 0x10, 0x18};
	u32 pid;

	ipio_info("%s ICE mode, mcu on = %d\n", (enable ? "Enable" : "Disable"), mcu);

	if (enable) {
		if (atomic_read(&idev->ice_stat)) {
			ipio_info("ice mode already enabled\n");
			return 0;
		}

		if (mcu)
			cmd_open[0] = 0x1F;

		atomic_set(&idev->ice_stat, ENABLE);

		do {
			if (idev->write(cmd_open, sizeof(cmd_open)) < 0)
				ipio_err("write ice mode cmd error\n");

			if (idev->spi_speed != NULL && idev->chip->spi_speed_ctrl)
				idev->spi_speed(ON);

			/* Read chip id to ensure that ice mode is enabled successfully */
			if (ilitek_ice_mode_read(idev->chip->pid_addr, &pid, sizeof(u32)) < 0)
				ipio_err("Read pid error\n");

			if (ilitek_tddi_ic_check_support(pid, pid >> 16) == 0)
				break;
		} while (--retry > 0);

		if (retry <= 0) {
			ipio_err("Enter to ICE Mode failed !!\n");
			atomic_set(&idev->ice_stat, DISABLE);
			ret = -1;
			goto out;
		}

		/* Patch to resolve the issue of i2c nack after exit to ice mode */
		if (ilitek_ice_mode_write(0x47002, 0x00, 1) < 0)
			ipio_err("Write 0x0 at 0x47002 failed\n");
	} else {
		if (!atomic_read(&idev->ice_stat)) {
			ipio_debug("ice mode already disabled\n");
			return 0;
		}

		ret = idev->write(cmd_close, sizeof(cmd_close));
		if (ret < 0)
			ipio_err("Exit to ICE Mode failed !!\n");
		atomic_set(&idev->ice_stat, DISABLE);
	}
out:
	return ret;
}

int ilitek_tddi_ic_watch_dog_ctrl(bool write, bool enable)
{
	int timeout = 50, ret = 0;

	if (!atomic_read(&idev->ice_stat)) {
		ipio_err("ice mode wasn't enabled\n");
		return -EINVAL;
	}

	if (idev->chip->wdt_addr <= 0 || idev->chip->id <= 0) {
		ipio_err("WDT/CHIP ID is invalid\n");
		return -EINVAL;
	}

	/* FW will automatiacally disable WDT in I2C */
	if (idev->wtd_ctrl == OFF) {
		ipio_info("WDT ctrl is off, do nothing\n");
		return 0;
	}

	if (!write) {
		if (ilitek_ice_mode_read(idev->chip->wdt_addr, &ret, sizeof(u8)) < 0) {
			ipio_err("Read wdt error\n");
			return -EINVAL;
		}
		ipio_debug("Read WDT: %s\n", (ret ? "ON" : "OFF"));
		return ret;
	}

	ipio_debug("%s WDT, key = %x\n", (enable ? "Enable" : "Disable"), idev->chip->wtd_key);

	if (enable) {
		if (ilitek_ice_mode_write(idev->chip->wdt_addr, 1, 1) < 0)
			ipio_err("Wrie WDT key failed\n");
	} else {
		/* need delay 300us to wait fw relaod code after stop mcu. */
		udelay(300);
		if (ilitek_ice_mode_write(idev->chip->wdt_addr, (idev->chip->wtd_key & 0xff), 1) < 0)
			ipio_err("Write WDT key failed\n");
		if (ilitek_ice_mode_write(idev->chip->wdt_addr, (idev->chip->wtd_key >> 8), 1) < 0)
			ipio_err("Write WDT key failed\n");
	}

	while (timeout > 0) {
		udelay(40);
		if (ilitek_ice_mode_read(TDDI_WDT_ACTIVE_ADDR, &ret, sizeof(u8)) < 0)
			ipio_err("Read wdt active error\n");

		ipio_debug("ret = %x\n", ret);
		if (enable) {
			if (ret == TDDI_WDT_ON)
				break;
		} else {
			if (ret == TDDI_WDT_OFF)
				break;

			/* If WDT can't be disabled, try to command and wait to see */
			if (ilitek_ice_mode_write(idev->chip->wdt_addr, 0x00, 1) < 0)
				ipio_err("Write 0x0 at %x\n", idev->chip->wdt_addr);
			if (ilitek_ice_mode_write(idev->chip->wdt_addr, 0x98, 1) < 0)
				ipio_err("Write 0x98 at %x\n", idev->chip->wdt_addr);
		}
		timeout--;
		mdelay(5);
	}

	if (timeout <= 0) {
		ipio_err("WDT turn on/off timeout !, ret = %x, pc = 0x%x\n",
				ret, ilitek_tddi_ic_get_pc_counter());
		return -EINVAL;
	}

	if (enable) {
		ipio_debug("WDT turn on succeed\n");
	} else {
		ipio_debug("WDT turn off succeed\n");
		if (ilitek_ice_mode_write(idev->chip->wdt_addr, 0, 1) < 0)
			ipio_err("Write turn off cmd failed\n");
	}
	return 0;
}

int ilitek_tddi_ic_func_ctrl(const char *name, int ctrl)
{
	int i = 0, ret;

	for (i = 0; i < FUNC_CTRL_NUM; i++) {
		if (strncmp(name, func_ctrl[i].name, strlen(name)) == 0) {
			if (strlen(name) != strlen(func_ctrl[i].name))
				continue;
			break;
		}
	}

	if (i >= FUNC_CTRL_NUM) {
		ipio_err("Not found function ctrl, %s\n", name);
		ret = -1;
		goto out;
	}

	if (idev->protocol->ver == PROTOCOL_VER_500) {
		ipio_err("Non support function ctrl with protocol v5.0\n");
		ret = -1;
		goto out;
	}

	if (idev->protocol->ver >= PROTOCOL_VER_560) {
		if (strncmp(func_ctrl[i].name, "gesture", strlen("gesture")) == 0 ||
			strncmp(func_ctrl[i].name, "phone_cover_window", strlen("phone_cover_window")) == 0) {
			ipio_debug("Non support %s function ctrl\n", func_ctrl[i].name);
			ret = -1;
			goto out;
		}
	}

	func_ctrl[i].cmd[2] = ctrl;

	ipio_info("func = %s, len = %d, cmd = 0x%x, 0%x, 0x%x\n", func_ctrl[i].name, func_ctrl[i].len,
		func_ctrl[i].cmd[0], func_ctrl[i].cmd[1], func_ctrl[i].cmd[2]);

	ret = idev->write(func_ctrl[i].cmd, func_ctrl[i].len);
	if (ret < 0)
		ipio_err("Write TP function failed\n");

out:
	return ret;
}

int ilitek_tddi_ic_code_reset(void)
{
	int ret;
	bool ice = atomic_read(&idev->ice_stat);

	if (!ice)
		if (ilitek_ice_mode_ctrl(ENABLE, OFF) < 0)
			ipio_err("Enable ice mode failed before code reset\n");

	ret = ilitek_ice_mode_write(0x40040, 0xAE, 1);
	if (ret < 0)
		ipio_err("ic code reset failed\n");

	if (!ice)
		if (ilitek_ice_mode_ctrl(DISABLE, OFF) < 0)
			ipio_err("Enable ice mode failed after code reset\n");
	return ret;
}

int ilitek_tddi_ic_whole_reset(void)
{
	ipio_info("ic whole reset key = 0x%x, edge_delay = %d\n",
		idev->chip->reset_key, idev->rst_edge_delay);

	if (ilitek_ice_mode_write(idev->chip->reset_key,
				idev->chip->reset_addr,
				sizeof(u32)) < 0) {
		ipio_err("ic whole reset failed\n");
		return -EINVAL;
	}
	msleep(idev->rst_edge_delay);
	return 0;
}

static void ilitek_tddi_ic_wr_pack(int packet)
{
	int retry = 100;
	u32 reg_data = 0;

	while (retry--) {
		if (ilitek_ice_mode_read(0x73010, &reg_data, sizeof(u8)) < 0)
			ipio_err("Read 0x73010 error\n");

		if ((reg_data & 0x02) == 0) {
			ipio_info("check ok 0x73010 read 0x%X retry = %d\n", reg_data, retry);
			break;
		}
		mdelay(10);
	}

	if (retry <= 0)
		ipio_info("check 0x73010 error read 0x%X\n", reg_data);

	if (ilitek_ice_mode_write(0x73000, packet, 4) < 0)
		ipio_err("Write %x at 0x73000\n", packet);
}

static u32 ilitek_tddi_ic_rd_pack(int packet)
{
	int retry = 100;
	u32 reg_data = 0;

	ilitek_tddi_ic_wr_pack(packet);

	while (retry--) {
		if (ilitek_ice_mode_read(0x4800A, &reg_data, sizeof(u8)) < 0)
			ipio_err("Read 0x4800A error\n");

		if ((reg_data & 0x02) == 0x02) {
			ipio_info("check  ok 0x4800A read 0x%X retry = %d\n", reg_data, retry);
			break;
		}
		mdelay(10);
	}
	if (retry <= 0)
		ipio_info("check 0x4800A error read 0x%X\n", reg_data);

	if (ilitek_ice_mode_write(0x4800A, 0x02, 1) < 0)
		ipio_err("Write 0x2 at 0x4800A\n");

	if (ilitek_ice_mode_read(0x73016, &reg_data, sizeof(u8)) < 0)
		ipio_err("Read 0x73016 error\n");

	return reg_data;
}

void ilitek_tddi_ic_set_ddi_reg_onepage(u8 page, u8 reg, u8 data)
{
	int wdt;
	u32 setpage = 0x1FFFFF00 | page;
	u32 setreg = 0x1F000100 | (reg << 16) | data;
	bool ice = atomic_read(&idev->ice_stat);

	ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);

	ipio_debug("setpage =  0x%X setreg = 0x%X\n", setpage, setreg);

	if (!ice)
		if (ilitek_ice_mode_ctrl(ENABLE, OFF) < 0)
			ipio_err("Enable ice mode failed before writing ddi reg\n");

	wdt = ilitek_tddi_ic_watch_dog_ctrl(ILI_READ, DISABLE);
	if (wdt)
		if (ilitek_tddi_ic_watch_dog_ctrl(ILI_WRITE, DISABLE) < 0)
			ipio_err("Disable WDT failed before writing ddi reg\n");

	/*TDI_WR_KEY*/
	ilitek_tddi_ic_wr_pack(0x1FFF9527);
	/*Switch to Page*/
	ilitek_tddi_ic_wr_pack(setpage);
	/* Page*/
	ilitek_tddi_ic_wr_pack(setreg);
	/*TDI_WR_KEY OFF*/
	ilitek_tddi_ic_wr_pack(0x1FFF9500);

	if (wdt)
		if (ilitek_tddi_ic_watch_dog_ctrl(ILI_WRITE, ENABLE) < 0)
			ipio_err("Enable WDT failed after writing ddi reg\n");

	if (!ice)
		if (ilitek_ice_mode_ctrl(DISABLE, OFF) < 0)
			ipio_err("Disable ice mode failed after writing ddi reg\n");

	ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);
}

void ilitek_tddi_ic_get_ddi_reg_onepage(u8 page, u8 reg)
{
	int wdt;
	u32 reg_data = 0;
	u32 setpage = 0x1FFFFF00 | page;
	u32 setreg = 0x2F000100 | (reg << 16);
	bool ice = atomic_read(&idev->ice_stat);

	ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);

	ipio_debug("setpage = 0x%X setreg = 0x%X\n", setpage, setreg);

	if (!ice)
		if (ilitek_ice_mode_ctrl(ENABLE, OFF) < 0)
			ipio_err("Enable ice mode failed before reading ddi reg\n");

	wdt = ilitek_tddi_ic_watch_dog_ctrl(ILI_READ, DISABLE);
	if (wdt)
		if (ilitek_tddi_ic_watch_dog_ctrl(ILI_WRITE, DISABLE) < 0)
			ipio_err("Disable WDT failed before reading ddi reg\n");

	/*TDI_WR_KEY*/
	ilitek_tddi_ic_wr_pack(0x1FFF9527);
	/*Set Read Page reg*/
	ilitek_tddi_ic_wr_pack(setpage);

	/*TDI_RD_KEY*/
	ilitek_tddi_ic_wr_pack(0x1FFF9487);
	/*( *( __IO uint8 *)	(0x4800A) ) =0x2*/
	if (ilitek_ice_mode_write(0x4800A, 0x02, 1) < 0)
		ipio_err("Write 0x2 at 0x4800A\n");

	reg_data = ilitek_tddi_ic_rd_pack(setreg);
	ipio_debug("check page = 0x%X, reg = 0x%X, read 0x%X\n", page, reg, reg_data);
	idev->chip->read_reg_data = reg_data;

	/*TDI_RD_KEY OFF*/
	ilitek_tddi_ic_wr_pack(0x1FFF9400);
	/*TDI_WR_KEY OFF*/
	ilitek_tddi_ic_wr_pack(0x1FFF9500);

	if (wdt)
		if (ilitek_tddi_ic_watch_dog_ctrl(ILI_WRITE, ENABLE) < 0)
			ipio_err("Enable WDT failed after reading ddi reg\n");

	if (!ice)
		if (ilitek_ice_mode_ctrl(DISABLE, OFF) < 0)
			ipio_err("Disable ice mode failed after reading ddi reg\n");

	ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);
}

void ilitek_tddi_ic_check_otp_prog_mode(void)
{
	int retry = 5;
	u32 prog_mode, prog_done;

	if (!idev->do_otp_check)
		return;

	if (ilitek_ice_mode_ctrl(ENABLE, OFF) < 0) {
		ipio_err("enter ice mode failed in otp\n");
		return;
	}

	if (ilitek_tddi_ic_watch_dog_ctrl(ILI_WRITE, DISABLE) < 0) {
		ipio_err("disable WDT failed in otp\n");
		return;
	}

	do {
		if (ilitek_ice_mode_write(0x43008, 0x80, 1) < 0)
			ipio_err("Write 0x80 at 0x43008 failed\n");

		if (ilitek_ice_mode_write(0x43030, 0x0, 1) < 0)
			ipio_err("Write 0x0 at 0x43030 failed\n");

		if (ilitek_ice_mode_write(0x4300C, 0x4, 1) < 0)
			ipio_err("Write 0x4 at 0x4300C failed\n");

		/* Need accurate power sequence, do not change it to msleep */
		mdelay(1);

		if (ilitek_ice_mode_write(0x4300C, 0x4, 1) < 0)
			ipio_err("Write 0x4 at 0x4300C\n");

		if (ilitek_ice_mode_read(0x43030, &prog_done, sizeof(u8)) < 0)
			ipio_err("Read prog_done error\n");

		if (ilitek_ice_mode_read(0x43008, &prog_mode, sizeof(u8)) < 0)
			ipio_err("Read prog_mode error\n");

		ipio_debug("otp prog_mode = 0x%x, prog_done = 0x%x\n", prog_mode, prog_done);
		if (prog_done == 0x0 && prog_mode == 0x80)
			break;
	} while (--retry > 0);

	if (retry <= 0)
		ipio_err("OTP Program mode error!\n");
}

void ilitek_tddi_ic_spi_speed_ctrl(bool enable)
{
	ipio_debug("%s spi speed up\n", (enable ? "Enable" : "Disable"));

	if (enable) {
		if (ilitek_ice_mode_write(0x063820, 0x00000101, 4) < 0)
			ipio_err("Write 0x00000101 at 0x063820 failed\n");

		if (ilitek_ice_mode_write(0x042c34, 0x00000008, 4) < 0)
			ipio_err("Write 0x00000008 at 0x042c34 failed\n");

		if (ilitek_ice_mode_write(0x063820, 0x00000000, 4) < 0)
			ipio_err("Write 0x00000000 at 0x063820 failed\n");
	} else {
		if (ilitek_ice_mode_write(0x063820, 0x00000101, 4) < 0)
			ipio_err("Write 0x00000101 at 0x063820 failed\n");

		if (ilitek_ice_mode_write(0x042c34, 0x00000000, 4) < 0)
			ipio_err("Write 0x00000000 at 0x042c34 failed\n");

		if (ilitek_ice_mode_write(0x063820, 0x00000000, 4) < 0)
			ipio_err("Write 0x00000000 at 0x063820 failed\n");
	}
}

u32 ilitek_tddi_ic_get_pc_counter(void)
{
	bool ice = atomic_read(&idev->ice_stat);
	u32 pc = 0;

	if (!ice)
		if (ilitek_ice_mode_ctrl(ENABLE, OFF) < 0)
			ipio_err("Enable ice mode failed while reading pc counter\n");

	if (ilitek_ice_mode_read(idev->chip->pc_counter_addr, &pc, sizeof(u32)) < 0)
		ipio_err("Read pc conter error\n");

	ipio_info("pc counter = 0x%x\n", pc);

	if (!ice)
		if (ilitek_ice_mode_ctrl(DISABLE, OFF) < 0)
			ipio_err("Disable ice mode failed while reading pc counter\n");

	return pc;
}

int ilitek_tddi_ic_check_int_stat(void)
{
	int timer = 5000;

	/* From FW request, timeout should at least be 5 sec */
	while (--timer > 0) {
		if (atomic_read(&idev->mp_int_check) == DISABLE)
			break;
		mdelay(1);
	}

	if (timer > 0) {
		ipio_info("Interrupt for MP is active\n");
		return 0;
	}

	ipio_err("Error! Interrupt for MP isn't received\n");
	atomic_set(&idev->mp_int_check, DISABLE);
	return -EINVAL;
}

int ilitek_tddi_ic_check_busy(int count, int delay)
{
	u8 cmd[2] = {0};
	u8 busy = 0, rby = 0;

	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = P5_X_CDC_BUSY_STATE;

	if (idev->actual_tp_mode == P5_X_FW_DEMO_MODE)
		rby = 0x41;
	else if (idev->actual_tp_mode == P5_X_FW_TEST_MODE)
		rby = 0x51;
	else {
		ipio_err("Unknown TP mode (0x%x)\n", idev->actual_tp_mode);
		return -EINVAL;
	}

	ipio_debug("read byte = %x, delay = %d\n", rby, delay);

	do {
		if (idev->write(cmd, sizeof(cmd)) < 0)
			ipio_err("Write %x,%x failed\n", P5_X_READ_DATA_CTRL, P5_X_CDC_BUSY_STATE);
		if (idev->write(&cmd[1], sizeof(u8)) < 0)
			ipio_err("Write %x failed\n", P5_X_CDC_BUSY_STATE);
		if (idev->read(&busy, sizeof(u8)) < 0)
			ipio_err("Read check busy failed\n");

		ipio_debug("busy = 0x%x\n", busy);

		if (busy == rby) {
			ipio_info("Check busy free\n");
			return 0;
		}

		mdelay(delay);
	} while (--count > 0);

	ipio_err("Check busy (0x%x) timeout ! pc = 0x%x\n", busy,
		ilitek_tddi_ic_get_pc_counter());
	return -EINVAL;
}

int ilitek_tddi_ic_get_project_id(u8 *pdata, int size)
{
	int i;
	u32 tmp;

	if (!pdata) {
		ipio_err("pdata is null\n");
		return -ENOMEM;
	}

	mutex_lock(&idev->touch_mutex);

	ipio_info("Read size = %d\n", size);

		if (ilitek_ice_mode_ctrl(ENABLE, OFF) < 0)
			ipio_err("Enable ice mode failed while reading project id\n");

	if (ilitek_ice_mode_write(0x041000, 0x0, 1) < 0)
		ipio_err("Pull cs low failed\n");
	if (ilitek_ice_mode_write(0x041004, 0x66aa55, 3) < 0)
		ipio_err("Write key failed\n");

	if (ilitek_ice_mode_write(0x041008, 0x03, 1) < 0)
		ipio_err("Write 0x03 at 0x041008\n");

	if (ilitek_ice_mode_write(0x041008, (RESERVE_BLOCK_START_ADDR & 0xFF0000) >> 16, 1) < 0)
		ipio_err("Write address failed\n");
	if (ilitek_ice_mode_write(0x041008, (RESERVE_BLOCK_START_ADDR & 0x00FF00) >> 8, 1) < 0)
		ipio_err("Write address failed\n");
	if (ilitek_ice_mode_write(0x041008, (RESERVE_BLOCK_START_ADDR & 0x0000FF), 1) < 0)
		ipio_err("Write address failed\n");

	for (i = 0; i < size; i++) {
		if (ilitek_ice_mode_write(0x041008, 0xFF, 1) < 0)
			ipio_err("Write dummy failed\n");
		if (ilitek_ice_mode_read(0x41010, &tmp, sizeof(u8)) < 0)
			ipio_err("Read project id error\n");
		pdata[i] = tmp;
		ipio_debug("project_id[%d] = 0x%x\n", i, pdata[i]);
	}

	ilitek_tddi_flash_clear_dma();

	if (ilitek_ice_mode_write(0x041000, 0x1, 1) < 0)
		ipio_err("Pull cs high\n");

		if (ilitek_ice_mode_ctrl(DISABLE, OFF) < 0)
			ipio_err("Disable ice mode failed while reading project id\n");

	mutex_unlock(&idev->touch_mutex);
	return 0;
}

int ilitek_tddi_ic_get_core_ver(void)
{
	int ret = 0;
	u8 cmd[2] = {0};
	u8 buf[10] = {0};

	if (idev->chip->info_from_hex) {
		buf[1] = idev->chip->info[64];
		buf[2] = idev->chip->info[65];
		buf[3] = idev->chip->info[66];
		goto out;
	}

	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = P5_X_GET_CORE_VERSION;

	if (idev->write(cmd, sizeof(cmd)) < 0) {
		ipio_err("write core ver err\n");
		ret = -1;
		goto out;
	}

	if (idev->write(&cmd[1], sizeof(u8)) < 0) {
		ipio_err("write core ver err\n");
		ret = -1;
		goto out;
	}

	if (idev->read(buf, idev->protocol->core_ver_len) < 0) {
		ipio_err("i2c/spi read core ver err\n");
		ret = -1;
		goto out;
	}

	if (buf[0] != P5_X_GET_CORE_VERSION) {
		ipio_err("Invalid core ver\n");
		ret = -1;
	}

out:
	ipio_info("Core version = %d.%d.%d.%d\n", buf[1], buf[2], buf[3], buf[4]);
	idev->chip->core_ver = buf[1] << 24 | buf[2] << 16 | buf[3] << 8 | buf[4];
	return ret;
}

static char ilk_info_summary[80] = "";
int ilitek_tddi_ic_get_fw_ver(void)
{
	int ret = 0;
	u8 cmd[2] = {0};
	u8 buf[10] = {0};

	if (idev->chip->info_from_hex) {
		buf[1] = idev->chip->info[44];
		buf[2] = idev->chip->info[45];
		buf[3] = idev->chip->info[46];
		buf[4] = idev->chip->info[47];
		goto out;
	}

	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = P5_X_GET_FW_VERSION;

	if (idev->write(cmd, sizeof(cmd)) < 0) {
		ipio_err("write firmware ver err\n");
		ret = -1;
		goto out;
	}

	if (idev->write(&cmd[1], sizeof(u8)) < 0) {
		ipio_err("write firmware ver err\n");
		ret = -1;
		goto out;
	}

	if (idev->read(buf, idev->protocol->fw_ver_len) < 0) {
		ipio_err("i2c/spi read firmware ver err\n");
		ret = -1;
		goto out;
	}

	if (buf[0] != P5_X_GET_FW_VERSION) {
		ipio_err("Invalid firmware ver\n");
		ret = -1;
	}

out:
	ipio_debug("Firmware version = %d.%d.%d.%d\n", buf[1], buf[2], buf[3], buf[4]);
	idev->chip->fw_ver = buf[1] << 24 | buf[2] << 16 | buf[3] << 8 | buf[4];

	snprintf(ilk_info_summary, sizeof(ilk_info_summary), "%s:%d.%d.%d.%d\n", ILK_VENDOR_INFO, buf[1], buf[2], buf[3], buf[4]);
	ipio_info("%s", ilk_info_summary);
	hq_regiser_hw_info(HWID_CTP, ilk_info_summary);

	return ret;
}

int ilitek_tddi_ic_get_panel_info(void)
{
	int ret = 0;
	u8 cmd = P5_X_GET_PANEL_INFORMATION;
	u8 buf[10] = {0};

	if (idev->chip->info_from_hex) {
		buf[1] = idev->chip->info[12];
		buf[2] = idev->chip->info[13];
		buf[3] = idev->chip->info[14];
		buf[4] = idev->chip->info[15];
		goto out;
	}

	ret = idev->write(&cmd, sizeof(u8));
	if (ret < 0) {
		ipio_err("Write panel info error\n");
		goto out;
	}

	ret = idev->read(buf, idev->protocol->panel_info_len);
	if (ret < 0)
		ipio_err("Read panel info error\n");

out:
	if (buf[0] != P5_X_GET_PANEL_INFORMATION) {
		idev->panel_wid = TOUCH_SCREEN_X_MAX;
		idev->panel_hei = TOUCH_SCREEN_Y_MAX;
	} else {
		idev->panel_wid = buf[1] << 8 | buf[2];
		idev->panel_hei = buf[3] << 8 | buf[4];
	}

	ipio_info("Panel info: width = %d, height = %d\n", idev->panel_wid, idev->panel_hei);
	return ret;
}

int ilitek_tddi_ic_get_spi_panel_info(void)
{
	int ret = 0;
	u8 cmd = P5_X_GET_PANEL_INFORMATION;
	u8 buf[10] = {0};

	ret = idev->write(&cmd, sizeof(u8));
	if (ret < 0) {
		ipio_err("Write panel info error\n");
		goto out;
	}

	ret = idev->read(buf, idev->protocol->panel_info_len);
	if (ret < 0)
		ipio_err("Read panel info error\n");

out:
	if (buf[0] != P5_X_GET_PANEL_INFORMATION) {
		idev->panel_wid = TOUCH_SCREEN_X_MAX;
		idev->panel_hei = TOUCH_SCREEN_Y_MAX;
	} else {
		idev->panel_wid = buf[1] << 8 | buf[2];
		idev->panel_hei = buf[3] << 8 | buf[4];
	}

	ipio_info("Panel info: width = %d, height = %d\n", idev->panel_wid, idev->panel_hei);
	return ret;
}

int ilitek_tddi_ic_get_tp_info(void)
{
	int ret = 0;
	u8 cmd[2] = {0};
	u8 buf[20] = {0};

	if (idev->chip->info_from_hex) {
		buf[1] = idev->chip->info[1];
		buf[2] = idev->chip->info[3];
		buf[3] = idev->chip->info[4];
		buf[4] = idev->chip->info[5];
		buf[5] = idev->chip->info[6];
		buf[6] = idev->chip->info[7];
		buf[7] = idev->chip->info[8];
		buf[8] = idev->chip->info[10];
		buf[11] = buf[7];
		buf[12] = buf[8];
		goto out;
	}

	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = P5_X_GET_TP_INFORMATION;

	if (idev->write(cmd, sizeof(cmd)) < 0) {
		ipio_err("Write tp info error\n");
		ret = -1;
		goto out;
	}

	if (idev->write(&cmd[1], sizeof(u8)) < 0) {
		ipio_err("Write tp info error\n");
		ret = -1;
		goto out;
	}

	if (idev->read(buf, idev->protocol->tp_info_len) < 0) {
		ipio_err("Read tp info error\n");
		ret = -1;
		goto out;
	}

	if (buf[0] != P5_X_GET_TP_INFORMATION) {
		ipio_err("Invalid tp info\n");
		ret = -1;
		goto out;
	}

out:
	idev->min_x = 0;
	idev->min_y = 0;
	idev->max_x = 2047;
	idev->max_y = 2047;
	idev->xch_num = 18;
	idev->ych_num = 32;
	idev->stx = 18;
	idev->srx = 32;


	ipio_info("TP Info: xch = %d, ych = %d, stx = %d, srx = %d, max_x = %d, max_y = %d\n\n", idev->xch_num, idev->ych_num, idev->stx, idev->srx, idev->max_x, idev->max_y);
	return ret;
}

static void ilitek_tddi_ic_check_protocol_ver(u32 pver)
{
	int i = 0;

	if (idev->protocol->ver == pver) {
		ipio_info("same procotol version, do nothing\n");
		return;
	}

	for (i = 0; i < PROTOCL_VER_NUM - 1; i++) {
		if (protocol_info[i].ver == pver) {
			idev->protocol = &protocol_info[i];
			ipio_info("update protocol version = %x\n", idev->protocol->ver);
			return;
		}
	}

	ipio_info("Not found a correct protocol version in list, use newest version\n");
	idev->protocol = &protocol_info[PROTOCL_VER_NUM - 1];
}

int ilitek_tddi_ic_get_protocl_ver(void)
{
	int ret = 0;
	u8 cmd[2] = {0};
	u8 buf[10] = {0};
	u32 ver;

	if (idev->chip->info_from_hex) {
		buf[1] = idev->chip->info[68];
		buf[2] = idev->chip->info[69];
		buf[3] = idev->chip->info[70];
		goto out;
	}

	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = P5_X_GET_PROTOCOL_VERSION;

	if (idev->write(cmd, sizeof(cmd)) < 0) {
		ipio_err("Write protocol version error\n");
		ret = -1;
		goto out;
	}

	if (idev->write(&cmd[1], sizeof(u8)) < 0) {
		ipio_err("Write protocol version error\n");
		ret = -1;
		goto out;
	}

	if (idev->read(buf, idev->protocol->pro_ver_len) < 0) {
		ipio_err("Read protocol version error\n");
		ret = -1;
		goto out;
	}

	if (buf[0] != P5_X_GET_PROTOCOL_VERSION) {
		ipio_err("Invalid protocol ver\n");
		ret = -1;
		goto out;
	}

out:
	ver = buf[1] << 16 | buf[2] << 8 | buf[3];

	ilitek_tddi_ic_check_protocol_ver(ver);

	ipio_info("Protocol version = %d.%d.%d\n", idev->protocol->ver >> 16,
		(idev->protocol->ver >> 8) & 0xFF, idev->protocol->ver & 0xFF);
	return ret;
}

int ilitek_tddi_ic_get_info(void)
{
	int ret = 0;

	if (!atomic_read(&idev->ice_stat)) {
		ipio_err("ice mode doesn't enable\n");
		return -EINVAL;
	}

	if (ilitek_ice_mode_read(idev->chip->pid_addr, &idev->chip->pid, sizeof(u32)) < 0)
		ipio_err("Read chip pid error\n");

	idev->chip->id = idev->chip->pid >> 16;
	idev->chip->type_hi = idev->chip->pid & 0x0000FF00;
	idev->chip->type_low = idev->chip->pid	& 0xFF;

	if (ilitek_ice_mode_read(idev->chip->otp_addr, &idev->chip->otp_id, sizeof(u32)) < 0)
		ipio_err("Read otp id error\n");
	if (ilitek_ice_mode_read(idev->chip->ana_addr, &idev->chip->ana_id, sizeof(u32)) < 0)
		ipio_err("Read ana id error\n");

	idev->chip->otp_id &= 0xFF;
	idev->chip->ana_id &= 0xFF;

	ipio_info("CHIP INFO: PID = %x, ID = %x, TYPE = %x, OTP = %x, ANA = %x\n",
		idev->chip->pid,
		idev->chip->id,
		((idev->chip->type_hi << 8) | idev->chip->type_low),
		idev->chip->otp_id,
		idev->chip->ana_id);

	ret = ilitek_tddi_ic_check_support(idev->chip->pid, idev->chip->id);
	return ret;
}

static struct ilitek_ic_info chip;

void ilitek_tddi_ic_init(void)
{
	chip.pid_addr =		   	TDDI_PID_ADDR;
	chip.wdt_addr =		   	TDDI_WDT_ADDR;
	chip.pc_counter_addr = 		TDDI_PC_COUNTER_ADDR;
	chip.otp_addr =		   	TDDI_OTP_ID_ADDR;
	chip.ana_addr =		   	TDDI_ANA_ID_ADDR;
	chip.reset_addr =	   	TDDI_CHIP_RESET_ADDR;
	chip.spi_speed_ctrl =		ENABLE;

	idev->protocol = &protocol_info[PROTOCL_VER_NUM - 1];
	idev->chip = &chip;
}
