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

#include "ilitek.h"

#define PROTOCL_VER_NUM		8
static struct ilitek_protocol_info protocol_info[PROTOCL_VER_NUM] = {
	/* length -> fw, protocol, tp, key, panel, core, func, window, cdc, mp_info */
	[0] = {PROTOCOL_VER_500, 4, 4, 14, 30, 5, 5, 2, 8, 3, 8},
	[1] = {PROTOCOL_VER_510, 4, 3, 14, 30, 5, 5, 3, 8, 3, 8},
	[2] = {PROTOCOL_VER_520, 4, 4, 14, 30, 5, 5, 3, 8, 3, 8},
	[3] = {PROTOCOL_VER_530, 9, 4, 14, 30, 5, 5, 3, 8, 3, 8},
	[4] = {PROTOCOL_VER_540, 9, 4, 14, 30, 5, 5, 3, 8, 15, 8},
	[5] = {PROTOCOL_VER_550, 9, 4, 14, 30, 5, 5, 3, 8, 15, 14},
	[6] = {PROTOCOL_VER_560, 9, 4, 14, 30, 5, 5, 3, 8, 15, 14},
	[7] = {PROTOCOL_VER_570, 9, 4, 14, 30, 5, 5, 3, 8, 15, 14},
};

#define FUNC_CTRL_NUM	16
static struct ilitek_ic_func_ctrl func_ctrl[FUNC_CTRL_NUM] = {
	/* cmd[3] = cmd, func, ctrl */
	// enble 0:disable, 1: enable, 2: N/A
	[0] = {"sense", {0x1, 0x1, 0x0}, 3, 0x0, 2, 0xFF},
	[1] = {"sleep", {0x1, 0x2, 0x0}, 3, 0x0, 2, 0xFF},
	[2] = {"glove", {0x1, 0x6, 0x0}, 3, 0x0, 0, 0xFF},
	[3] = {"stylus", {0x1, 0x7, 0x0}, 3, 0x0, 0, 0xFF},
	[4] = {"lpwg", {0x1, 0xA, 0x0}, 3, 0x0, 2, 0xFF},
	[5] = {"proximity", {0x1, 0x10, 0x0}, 3, 0x0, 2, 0xFF},
	[6] = {"plug", {0x1, 0x11, 0x0}, 3, 0x1, 0, 0xFF},
	[7] = {"edge_palm", {0x1, 0x12, 0x0}, 3, 0x1, 0, 0xFF},
	[8] = {"lock_point", {0x1, 0x13, 0x0}, 3, 0x0, 0, 0xFF},
	[9] = {"active", {0x1, 0x14, 0x0}, 3, 0x0, 2, 0xFF},
	[10] = {"idle", {0x1, 0x19, 0x0}, 3, 0x1, 0, 0xFF},
	[11] = {"gesture_demo_en", {0x1, 0x16, 0x0}, 3, 0x0, 0, 0xFF},
	[12] = {"tp_recore", {0x1, 0x18, 0x0}, 3, 0x0, 2, 0xFF},
	[13] = {"knock_en", {0x1, 0xA, 0x8, 0x03, 0x0, 0x0}, 6, 0xFF, 0, 0xFF},
	[14] = {"ear_phone", {0x1, 0x17, 0x0}, 3, 0x0, 0, 0xFF},
	[15] = {"knuckle", {0x1, 0xF, 0x0}, 3, 0x0, 0, 0xFF},
};

#ifdef ROI
int ilitek_config_knuckle_roi_ctrl(cmd_types cmd)
{
	int ret = 0;
	switch(cmd) {
	case CMD_DISABLE:
		ipio_debug("knuckle roi disbale\n");
		ret = ilitek_tddi_ic_func_ctrl("knuckle", cmd, NULL, 0);
		break;
	case CMD_ENABLE:
		ipio_debug("knuckle roi enable\n");
		ret = ilitek_tddi_ic_func_ctrl("knuckle", cmd, NULL, 0);
		break;
	case CMD_STATUS:
		ipio_debug("knuckle roi read status\n");
		ret = ilitek_tddi_ic_func_ctrl("knuckle", cmd, NULL, 0);
		break;
	case CMD_ROI_DATA:
		ipio_debug("knuckle roi read data\n");
		ret = ilitek_tddi_ic_func_ctrl("knuckle", cmd, NULL, 0);
		break;
	default:
		ipio_err("knuckle roi unknown cmd: %d\n", cmd);
		return -EINVAL;
	}
	return ret;
}

int ilitek_config_get_knuckle_roi_status(void)
{
	int ret = 0;
	u8 roi_status = 0;

	ret = ilitek_config_knuckle_roi_ctrl(CMD_STATUS);
	if (ret) {
		ipio_err("config to read roi status failed\n");
		return ret;
	}
    	// ret = ilitek_i2c_read(&roi_status, 1);
	msleep(1);
	ret = idev->wrapper(NULL, 0, &roi_status, sizeof(roi_status), OFF, OFF);
	if (ret) {
		ipio_err("read data failed\n");
		return ret;
	}

	ipio_info("roi %s\n", roi_status ? "enable" : "disable");

	return roi_status;
}
#endif

int ilitek_i2c_spi_ice_wrapper(u8 *txbuf, u32 wlen, u8 *rxbuf, u32 rlen, bool slave_int_check, bool int_check)
{
	int ret = 0, operate = -1;
	u8 temp[2];

	if (wlen > 0) {
		if (ERR_ALLOC_MEM(txbuf)) {
			ipio_err("txbuf is null\n");
			return -ENOMEM;
		}
	}

	if (rlen > 0) {
		if (ERR_ALLOC_MEM(rxbuf)) {
			ipio_err("rxbuf is null\n");
			return -ENOMEM;
		}
	}

	if (wlen > 0 && rlen > 0)
		operate = RW_SYNC;
	else if (wlen > 0)
		operate = W_ONLY;
	else
		operate = R_ONLY;

	ipio_debug("wlen: %d, rlen: %d, slave_int_check: %d, int_check: %d\n", wlen, rlen, slave_int_check, int_check);

	idev->detect_int_stat = ilitek_tddi_ic_check_int_stat;

	switch (operate) {
	case RW_SYNC:
	case W_ONLY:
#if (PLL_CLK_WAKEUP_TP_RESUME == ENABLE)
		if(idev->pll_clk_wakeup == true) {
#else
		if ((idev->pll_clk_wakeup == true) && (idev->actual_tp_mode == P5_X_FW_GESTURE_MODE)) {
#endif
			ret = idev->pll_clk_wakeup_func(txbuf[0]);
			if (ret < 0)
				goto out;
		}

		if(operate == RW_SYNC) {
			temp[0] = P5_X_READ_DATA_CTRL;
			temp[1] = txbuf[0];

			ipio_debug("Pre cmd  0x%X, 0x%X\n", temp[0], temp[1]);
			if (idev->write(temp, sizeof(temp)) < 0) {
				ipio_err("write pre cmd fail\n");
				ret = -1;
				goto out;
			}

		}

		if (int_check)
			atomic_set(&idev->cmd_int_check, ENABLE);

		ret = idev->write(txbuf, wlen);
		if (ret < 0) {
			ipio_err("wrapper write error\n");
			goto out;
		}

		if (int_check) {
			if (idev->detect_int_stat() < 0) {
				ipio_err("ERROR! Check INT timeout\n");
				ret = -ETIME;
				goto out;
			}
		} else {
			msleep(1);
		}

		if (operate == W_ONLY)
			break;
	case R_ONLY:
		ret = idev->read(rxbuf, rlen);
		if (ret < 0) {
			ipio_err("wrapper read error\n");
			goto out;
		}

		ret = rlen;

		break;
	default:
		ipio_err("Unknown operation\n");
		ret = -EINVAL;
		break;
	}

out:
	if (int_check)
		atomic_set(&idev->cmd_int_check, DISABLE);

	return ret;
}

int ilitek_wrapper(u8 *txbuf, u32 wlen, u8 *rxbuf, u32 rlen, bool slave_int_check, bool int_check)
{
#if (TDDI_INTERFACE == BUS_SPI)
	if (idev->spi_mode == SPI_SLAVE_MODE)
		return ilitek_spi_slave_wrapper(txbuf, wlen, rxbuf, rlen, slave_int_check, int_check);
	else
#endif
		return ilitek_i2c_spi_ice_wrapper(txbuf, wlen, rxbuf, rlen, slave_int_check, int_check);
}

void ilitek_tddi_ic_setting(void)
{
	if (idev->chip->id == ILI9881_CHIP) {
		idev->chip->reset_key = 0x00019881;
		idev->chip->wtd_key = 0x9881;
		idev->chip->hd_dma_check_crc_off = firmware_hd_dma_crc_off_ili9881;
		idev->fix_ice = DISABLE;
		/*
		 * Since spi speed has been enabled previsouly whenever enter to ICE mode,
		 * we have to disable if find out the ic is ili9881.
		 */
		if (idev->spi_speed != NULL && idev->chip->spi_speed_ctrl) {
			idev->spi_speed(OFF);
			idev->chip->spi_speed_ctrl = DISABLE;
		}

		if (idev->chip->pid == ILI9881F_AA)
			idev->chip->no_bk_shift = RAWDATA_NO_BK_SHIFT_9881F;
		else
			idev->chip->no_bk_shift = RAWDATA_NO_BK_SHIFT_9881H;
	} else {
		idev->chip->reset_key = 0x00019878;
		idev->chip->wtd_key = 0x9878;
		idev->chip->hd_dma_check_crc_off = firmware_hd_dma_crc_off_ili7807;
		idev->chip->no_bk_shift = RAWDATA_NO_BK_SHIFT_9881H;

		if (idev->chip->pid != ILI7807G_AA && idev->chip->pid != ILI7807G_AB)
			idev->chip->spi_speed_ctrl = DISABLE;

		if (idev->chip->pid >= ILI7807G_AA && idev->chip->pid <= ILI7807G_AH) {
			idev->fix_ice = ENABLE;
		} else {
			idev->fix_ice = DISABLE;
		}
	}

	idev->chip->max_count = 0x1FFFF;
}

int ilitek_ice_mode_bit_mask_write(u32 addr, u32 mask, u32 value)
{
	int ret = 0;
	u32 data = 0;

	if (ilitek_ice_mode_read(addr, &data, sizeof(u32)) < 0) {
		ipio_err("Read data error\n");
		return -1;
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
		return -1;
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
		return -1;
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
	int ret = 0, retry = 3, ack = 0;
	u8 cmd_open[4] = {0x25, 0x62, 0x10, 0x18};
	u8 cmd_close[4] = {0x1B, 0x62, 0x10, 0x18};
	u32 pid;

	ipio_debug("%s ICE mode, mcu on = %d\n", (enable ? "Enable" : "Disable"), mcu);

	if (enable) {
		if (atomic_read(&idev->ice_stat)) {
			ipio_info("ice mode already enabled\n");
			return 0;
		}

		if (mcu)
			cmd_open[0] = 0x1F;

		atomic_set(&idev->ice_stat, ENABLE);

		if (idev->fix_ice) {
			do {
				if (idev->wrapper(cmd_open, sizeof(cmd_open), NULL, 0, OFF, OFF) < 0)
					ipio_err("write ice mode cmd error\n");

				idev->pll_clk_wakeup = false;

				if (idev->spi_speed != NULL && idev->chip->spi_speed_ctrl)
					idev->spi_speed(ON);

				/* Read chip id to ensure that ice mode is enabled successfully */
				if (ilitek_ice_mode_read(idev->chip->pid_addr, &pid, sizeof(u32)) < 0) {
					atomic_set(&idev->ice_stat, DISABLE);
					ipio_err("Read pid error\n");
				}

				if (pid == idev->chip->pid)
					break;

			} while (--retry > 0);

			if (retry <= 0) {
				ipio_err("Enter to ICE Mode failed !!\n");
				atomic_set(&idev->ice_stat, DISABLE);
				ret = -1;
				goto out;
			}
		} else {
			ret = idev->wrapper(cmd_open, sizeof(cmd_open), NULL, 0, OFF, OFF);
			if (ret < 0){
				ipio_err("Enter ice mode cmd error\n");
				atomic_set(&idev->ice_stat, DISABLE);
			}

			idev->pll_clk_wakeup = false;
		}
		ipio_debug("Enter to ICE Mode PASS !!\n");
		/* Patch to resolve the issue of i2c nack after exit to ice mode */
#if (TDDI_INTERFACE == BUS_I2C)
		if (ilitek_ice_mode_write(0x47002, 0x00, 1) < 0)
			ipio_err("Write 0x0 at 0x47002 failed\n");

		if (ilitek_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
			ipio_err("Write cs high failed\n");
#endif
	} else {
		if (!atomic_read(&idev->ice_stat)) {
			ipio_info("ice mode already disabled\n");
			return 0;
		}

		if (idev->fix_ice) {
			while (retry > 0) {
				if (idev->wrapper(cmd_close, sizeof(cmd_close), NULL, 0, OFF, OFF) < 0) {
					ipio_err("write ice mode cmd error\n");
					retry--;
					continue;
				}

				if (!idev->spi_ack)
					break;

				ack = idev->spi_ack();
				if (ack == SPI_ACK)
					break;

				usleep_range(1000, 1000);
				retry--;
			}

			if (retry <= 0) {
				ipio_err("Failed to exit ice mode\n");
				atomic_set(&idev->ice_stat, DISABLE);
				return -EIO;
			}
		} else {
			ret = idev->wrapper(cmd_close, sizeof(cmd_close), NULL, 0, OFF, OFF);
			if (ret < 0)
				ipio_err("Exit to ICE Mode failed !!\n");
		}
		idev->pll_clk_wakeup = true;
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
		return -1;
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
			return -1;
		}
		ipio_info("Read WDT: %s\n", (ret ? "ON" : "OFF"));
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
	}

	if (timeout <= 0) {
		ipio_err("WDT turn on/off timeout !, ret = %x\n", ret);
		ilitek_tddi_ic_get_pc_counter(0);
		return -EINVAL;
	}

	if (enable) {
		ipio_info("WDT turn on succeed\n");
	} else {
		ipio_info("WDT turn off succeed\n");
		if (ilitek_ice_mode_write(idev->chip->wdt_addr, 0, 1) < 0)
			ipio_err("Write turn off cmd failed\n");
	}
	return 0;
}

void ilitek_tddi_ic_edge_palm_para_init(void) {
#if ENABLE_EDGE_PALM_PARA
	int i;

	idev->edge_palm_para[0] = P5_X_EDGE_PLAM_CTRL_1;
	idev->edge_palm_para[1] = P5_X_EDGE_PALM_TUNING_PARA;
	idev->edge_palm_para[2] = (ZONE_A_W & 0xFF00) >> 8;
	idev->edge_palm_para[3] = ZONE_A_W & 0xFF;
	idev->edge_palm_para[4] = (ZONE_B_W & 0xFF00) >> 8;
	idev->edge_palm_para[5] = ZONE_B_W & 0xFF;
	idev->edge_palm_para[6] = (ZONE_A_ROTATION_W & 0xFF00) >> 8;
	idev->edge_palm_para[7] = ZONE_A_ROTATION_W & 0xFF;
	idev->edge_palm_para[8] = (ZONE_B_ROTATION_W & 0xFF00) >> 8;
	idev->edge_palm_para[9] = ZONE_B_ROTATION_W & 0xFF;
	idev->edge_palm_para[10] = (ZONE_C_WIDTH & 0xFF00) >> 8;
	idev->edge_palm_para[11] = ZONE_C_WIDTH & 0xFF;
	idev->edge_palm_para[12] = (ZONE_C_HEIGHT & 0xFF00) >> 8;
	idev->edge_palm_para[13] = ZONE_C_HEIGHT & 0xFF;
	idev->edge_palm_para[14] = (ZONE_C_HEIGHT_LITTLE & 0xFF00) >> 8;
	idev->edge_palm_para[15] = ZONE_C_HEIGHT_LITTLE & 0xFF;
	idev->edge_palm_para[16] = (ZONE_A_W & 0xFF00) >> 8;
	idev->edge_palm_para[17] = ZONE_A_W & 0xFF;
	idev->edge_palm_para[18] = (ZONE_B_W & 0xFF00) >> 8;
	idev->edge_palm_para[19] = ZONE_B_W & 0xFF;
	idev->edge_palm_para[20] = (ZONE_A_ROTATION_W & 0xFF00) >> 8;
	idev->edge_palm_para[21] = ZONE_A_ROTATION_W & 0xFF;
	idev->edge_palm_para[22] = (ZONE_B_ROTATION_W & 0xFF00) >> 8;
	idev->edge_palm_para[23] = ZONE_B_ROTATION_W & 0xFF;
	idev->edge_palm_para[24] = (ZONE_C_WIDTH & 0xFF00) >> 8;
	idev->edge_palm_para[25] = ZONE_C_WIDTH & 0xFF;
	idev->edge_palm_para[26] = (ZONE_C_HEIGHT & 0xFF00) >> 8;
	idev->edge_palm_para[27] = ZONE_C_HEIGHT & 0xFF;
	idev->edge_palm_para[28] = (ZONE_C_HEIGHT_LITTLE & 0xFF00) >> 8;
	idev->edge_palm_para[29] = ZONE_C_HEIGHT_LITTLE & 0xFF;
	idev->edge_palm_para[30] = ilitek_calc_packet_checksum(idev->edge_palm_para, P5_X_EDGE_PALM_PARA_LENGTH - 1);

	for (i = 0; i < P5_X_EDGE_PALM_PARA_LENGTH; i++) {
		ipio_debug("edge_palm_para[%d] = 0x%2x\n",i ,idev->edge_palm_para[i]);
	}
#endif
}
void ilitek_tddi_ic_send_edge_palm_para(void) {
#if ENABLE_EDGE_PALM_PARA
	int ret = 0;

	ipio_info("send edge palm para\n");

	ret = idev->wrapper(idev->edge_palm_para, P5_X_EDGE_PALM_PARA_LENGTH, NULL, 0, OFF, OFF);
	if (ret < 0)
		ipio_err("Write edge palm para function failed\n");
#endif
}


int ilitek_tddi_ic_func_ctrl(const char *name, int ctrl, u8 *rxbuf, u32 rlen)
{
	int i = 0, ret;

	for (i = 0; i < FUNC_CTRL_NUM; i++) {
		if (ipio_strcmp(name, func_ctrl[i].name) == 0) {
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
		if (ipio_strcmp(func_ctrl[i].name, "gesture") == 0 ||
			ipio_strcmp(func_ctrl[i].name, "phone_cover_window") == 0) {
			ipio_info("Non support %s function ctrl\n", func_ctrl[i].name);
			ret = -1;
			goto out;
		}
	}

	func_ctrl[i].cmd[2] = ctrl;

	ipio_info("func = %s, len = %d, cmd = 0x%x, 0%x, 0x%x\n", func_ctrl[i].name, func_ctrl[i].len,
		func_ctrl[i].cmd[0], func_ctrl[i].cmd[1], func_ctrl[i].cmd[2]);

	if (rxbuf == NULL || rlen == 0)
		ret = idev->wrapper(func_ctrl[i].cmd, func_ctrl[i].len, rxbuf, rlen, OFF, OFF);
	else
		ret = idev->wrapper(func_ctrl[i].cmd, func_ctrl[i].len, rxbuf, rlen, ON, OFF);

	if (ret < 0)
		ipio_err("Write TP function failed\n");

	if (func_ctrl[i].rec_state < 2) {
		if (ctrl == func_ctrl[i].def_cmd)
			func_ctrl[i].rec_state = DISABLE;
		else
			func_ctrl[i].rec_state = ENABLE;

		func_ctrl[i].rec_cmd = ctrl;
	}

	ipio_debug("record %s func cmd %d, rec_state %d\n", func_ctrl[i].name, func_ctrl[i].rec_cmd, func_ctrl[i].rec_state);

out:
	return ret;
}

void ilitek_tddi_ic_func_ctrl_reset(void) {
	int i = 0;

	ilitek_tddi_ic_send_edge_palm_para();

	for (i = 0; i < FUNC_CTRL_NUM; i++) {
		if (func_ctrl[i].rec_state == ENABLE) {
			ipio_debug("reset func ctrl %s, record status = %d, cmd = %d\n", func_ctrl[i].name,
				func_ctrl[i].rec_state, func_ctrl[i].rec_cmd);
			if (ilitek_tddi_ic_func_ctrl(func_ctrl[i].name, func_ctrl[i].rec_cmd, NULL, 0) < 0)
				ipio_err("reset ic func ctrl %s failed\n", func_ctrl[i].name);
		}
	}
}


int ilitek_tddi_ic_code_reset(bool mcu)
{
	int ret;
	bool ice = atomic_read(&idev->ice_stat);

	if (!ice)
		if (ilitek_ice_mode_ctrl(ENABLE, mcu) < 0)
			ipio_err("Enable ice mode failed before code reset\n");

	ret = ilitek_ice_mode_write(0x40040, 0xAE, 1);
	if (ret < 0)
		ipio_err("ic code reset failed\n");

	if (!ice)
		if (ilitek_ice_mode_ctrl(DISABLE, mcu) < 0)
			ipio_err("Enable ice mode failed after code reset\n");
	return ret;
}

int ilitek_tddi_ic_whole_reset(bool mcu)
{
	int ret = 0;
	bool ice = atomic_read(&idev->ice_stat);

	if (!ice)
		if (ilitek_ice_mode_ctrl(ENABLE, mcu) < 0)
			ipio_err("Enable ice mode failed before chip reset\n");

	ipio_info("ic whole reset key = 0x%x, edge_delay = %d\n",
		idev->chip->reset_key, idev->rst_edge_delay);

	ret = ilitek_ice_mode_write(idev->chip->reset_addr, idev->chip->reset_key, sizeof(u32));
	if (ret < 0) {
		ipio_err("ic whole reset failed\n");
		goto out;
	}

	/* Need accurate power sequence, do not change it to msleep */
	mdelay(idev->rst_edge_delay);

out:
	if (!ice)
		if (ilitek_ice_mode_ctrl(DISABLE, OFF) < 0)
			ipio_err("Enable ice mode failed after chip reset\n");

	return ret;
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

void ilitek_tddi_ic_set_ddi_reg_onepage(u8 page, u8 reg, u8 data, bool mcu)
{
	int wdt;
	u32 setpage = 0x1FFFFF00 | page;
	u32 setreg = 0x1F000100 | (reg << 16) | data;
	bool ice = atomic_read(&idev->ice_stat);

	ipio_info("setpage =  0x%X setreg = 0x%X\n", setpage, setreg);

	if (!ice)
		if (ilitek_ice_mode_ctrl(ENABLE, mcu) < 0)
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
		if (ilitek_ice_mode_ctrl(DISABLE, mcu) < 0)
			ipio_err("Disable ice mode failed after writing ddi reg\n");
}

void ilitek_tddi_ic_get_ddi_reg_onepage(u8 page, u8 reg, u8 *data, bool mcu)
{
	int wdt;
	u32 setpage = 0x1FFFFF00 | page;
	u32 setreg = 0x2F000100 | (reg << 16);
	bool ice = atomic_read(&idev->ice_stat);

	ipio_info("setpage = 0x%X setreg = 0x%X\n", setpage, setreg);

	if (!ice)
		if (ilitek_ice_mode_ctrl(ENABLE, mcu) < 0)
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

	*data = ilitek_tddi_ic_rd_pack(setreg);
	ipio_info("check page = 0x%X, reg = 0x%X, read 0x%X\n", page, reg, *data);

	/*TDI_RD_KEY OFF*/
	ilitek_tddi_ic_wr_pack(0x1FFF9400);
	/*TDI_WR_KEY OFF*/
	ilitek_tddi_ic_wr_pack(0x1FFF9500);

	if (wdt)
		if (ilitek_tddi_ic_watch_dog_ctrl(ILI_WRITE, ENABLE) < 0)
			ipio_err("Enable WDT failed after reading ddi reg\n");

	if (!ice)
		if (ilitek_ice_mode_ctrl(DISABLE, mcu) < 0)
			ipio_err("Disable ice mode failed after reading ddi reg\n");
}

void ilitek_tddi_ic_check_otp_prog_mode(bool mcu)
{
	int retry = 5;
	u32 prog_mode, prog_done;

	if (!idev->do_otp_check)
		return;

	if (ilitek_ice_mode_ctrl(ENABLE, mcu) < 0) {
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
		ipio_err("OTP Program mode error! (%x, %x)\n", prog_mode, prog_done);
}

void ilitek_tddi_ic_spi_speed_ctrl(bool enable)
{
	ipio_info("%s spi speed up\n", (enable ? "Enable" : "Disable"));

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

void ilitek_tddi_ic_get_pc_counter(int stat)
{
	bool ice = atomic_read(&idev->ice_stat);
	u32 pc = 0, pc_addr = idev->chip->pc_counter_addr;
	u32 latch = 0, latch_addr = idev->chip->pc_latch_addr;
	int ret = 0;

	ipio_debug("stat = %d\n", stat);

	if (!ice) {
		if (stat == DO_SPI_RECOVER || stat == DO_I2C_RECOVER)
			ret = ilitek_ice_mode_ctrl(ENABLE, OFF);
		else
			ret = ilitek_ice_mode_ctrl(ENABLE, ON);

		if (ret < 0)
			ipio_err("Enable ice mode failed while reading pc counter\n");
	}


	if (ilitek_ice_mode_read(pc_addr, &pc, sizeof(u32)) < 0)
		ipio_err("Read pc conter error\n");

	if (ilitek_ice_mode_read(latch_addr, &latch, sizeof(u32)) < 0)
		ipio_err("Read pc conter error\n");

	idev->fw_pc = pc;
	idev->fw_latch = latch;
	ipio_err("read counter (addr: 0x%x) = 0x%x, latch (addr: 0x%x) = 0x%x\n",
		pc_addr, idev->fw_pc, latch_addr, idev->fw_latch);

	/* Avoid screen abnormal. */
	if (stat == DO_SPI_RECOVER) {
		atomic_set(&idev->ice_stat, DISABLE);
		return;
	}

	if (!ice) {
		if (ilitek_ice_mode_ctrl(DISABLE, ON) < 0)
			ipio_err("Disable ice mode failed while reading pc counter\n");
	}
}

int ilitek_tddi_ic_check_int_stat(void)
{
	/* From FW request, timeout should at least be 3 sec */
	if (!wait_event_interruptible_timeout(idev->inq, !atomic_read(&idev->cmd_int_check), msecs_to_jiffies(idev->wait_int_timeout))) {
		ipio_err("Error! INT pulse no detected. Timeout = %d ms\n", idev->wait_int_timeout);
		atomic_set(&idev->cmd_int_check, DISABLE);
		return -1;
	}
	ipio_debug("MP INT detected.\n");
	return 0;
}

int ilitek_tddi_ic_check_busy(int count, int delay)
{
	u8 cmd = 0;
	u8 busy = 0, rby = 0;

	cmd = P5_X_CDC_BUSY_STATE;

	if (idev->actual_tp_mode == P5_X_FW_AP_MODE)
		rby = 0x41;
	else if (idev->actual_tp_mode == P5_X_FW_TEST_MODE)
		rby = 0x51;
	else {
		ipio_err("Unknown TP mode (0x%x)\n", idev->actual_tp_mode);
		return -EINVAL;
	}

	ipio_info("read byte = %x, delay = %d\n", rby, delay);

	do {
		if (idev->wrapper(&cmd, sizeof(cmd), &busy, sizeof(busy), ON, OFF) < 0)
			ipio_err("Read check busy failed\n");

		ipio_debug("busy = 0x%x\n", busy);

		if (busy == rby) {
			ipio_info("Check busy free\n");
			return 0;
		}

		mdelay(delay);
	} while (--count > 0);

	ipio_err("Check busy (0x%x) timeout !\n", busy);
	ilitek_tddi_ic_get_pc_counter(0);
	return -1;
}

int ilitek_tddi_ic_get_project_id(u8 *pdata, int size)
{
	int i;
	u32 tmp;
	bool ice = atomic_read(&idev->ice_stat);

	if (!pdata) {
		ipio_err("pdata is null\n");
		return -ENOMEM;
	}

	ipio_info("Read size = %d\n", size);

	if (!ice)
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
		ipio_info("project_id[%d] = 0x%x\n", i, pdata[i]);
	}

	ilitek_tddi_flash_clear_dma();

	if (ilitek_ice_mode_write(0x041000, 0x1, 1) < 0)
		ipio_err("Pull cs high\n");

	if (!ice)
		if (ilitek_ice_mode_ctrl(DISABLE, OFF) < 0)
			ipio_err("Disable ice mode failed while reading project id\n");

	return 0;
}

int ilitek_tddi_ic_get_core_ver(void)
{
	int i = 0, ret = 0;
	u8 cmd = 0;
	u8 buf[10] = {0};

	idev->protocol->core_ver_len = P5_X_CORE_VER_FOUR_LENGTH;

	if (idev->info_from_hex) {
		buf[1] = idev->fw_info[68];
		buf[2] = idev->fw_info[69];
		buf[3] = idev->fw_info[70];
		buf[4] = idev->fw_info[71];
		goto out;
	}

	do {
		if (i == 0) {
			cmd = P5_X_GET_CORE_VERSION_NEW;
		} else {
			cmd = P5_X_GET_CORE_VERSION;
			idev->protocol->core_ver_len = P5_X_CORE_VER_THREE_LENGTH;
		}

		memset(buf, 0, sizeof(buf));

		if (idev->wrapper(&cmd, sizeof(cmd), buf, idev->protocol->core_ver_len, ON, OFF) < 0)
			ipio_err("i2c/spi read core ver err\n");

		ipio_debug("header = 0x%x\n", buf[0]);

		if (buf[0] == P5_X_GET_CORE_VERSION ||
			buf[0] == P5_X_GET_CORE_VERSION_NEW)
			break;
	} while (++i < 2);

	if (buf[0] == P5_X_GET_CORE_VERSION)
		buf[4] = 0;

	if (i >= 2) {
		ipio_err("Invalid header (0x%x)\n", buf[0]);
		ret = -EINVAL;
	}

out:
	ipio_info("Core version = %d.%d.%d.%d\n", buf[1], buf[2], buf[3], buf[4]);
	idev->chip->core_ver = buf[1] << 24 | buf[2] << 16 | buf[3] << 8 | buf[4];
	return ret;
}

void ilitek_tddi_fw_uart_ctrl(u8 ctrl)
{
	u8 cmd[4] = {0};

	if (ctrl > 1) {
		ipio_info("Unknown cmd, ignore\n");
		return;
	}

	ipio_info("%s UART mode\n", ctrl ? "Enable" : "Disable");

	cmd[0] = P5_X_I2C_UART;
	cmd[1] = 0x3;
	cmd[2] = 0;
	cmd[3] = ctrl;

	if (idev->wrapper(cmd, sizeof(cmd), NULL, 0, OFF, OFF) < 0) {
		ipio_info("Write fw uart cmd failed\n");
		return;
	}

	idev->fw_uart_en = ctrl ? ENABLE : DISABLE;
}

int ilitek_tddi_ic_get_fw_ver(void)
{
	int ret = 0;
	u8 cmd = P5_X_GET_FW_VERSION;
	u8 buf[10] = {0};

	if (idev->info_from_hex) {
		buf[1] = idev->fw_info[48];
		buf[2] = idev->fw_info[49];
		buf[3] = idev->fw_info[50];
		buf[4] = idev->fw_info[51];
		buf[5] = idev->fw_mp_ver[0];
		buf[6] = idev->fw_mp_ver[1];
		buf[7] = idev->fw_mp_ver[2];
		buf[8] = idev->fw_mp_ver[3];
		goto out;
	}

	if (idev->wrapper(&cmd, sizeof(cmd), buf, idev->protocol->fw_ver_len, ON, OFF) < 0) {
		ipio_err("i2c/spi read firmware ver err\n");
		ret = -1;
		goto out;
	}

	if (buf[0] != P5_X_GET_FW_VERSION) {
		ipio_err("Invalid firmware ver\n");
		ret = -1;
	}

out:
	ipio_info("Firmware version = %d.%d.%d.%d\n", buf[1], buf[2], buf[3], buf[4]);
	ipio_info("Firmware MP version = %d.%d.%d.%d\n", buf[5], buf[6], buf[7], buf[8]);
	idev->chip->fw_ver = buf[1] << 24 | buf[2] << 16 | buf[3] << 8 | buf[4];
	idev->chip->fw_mp_ver = buf[5] << 24 | buf[6] << 16 | buf[7] << 8 | buf[8];
	return ret;
}

int ilitek_tddi_ic_get_panel_info(void)
{
	int ret = 0;
	u8 cmd = P5_X_GET_PANEL_INFORMATION;
	u8 buf[10] = {0};
	u8 len  = idev->protocol->panel_info_len;

	if (idev->info_from_hex && (idev->chip->core_ver >= CORE_VER_1410)) {
		buf[1] = idev->fw_info[16];
		buf[2] = idev->fw_info[17];
		buf[3] = idev->fw_info[18];
		buf[4] = idev->fw_info[19];
		idev->panel_wid = buf[2] << 8 | buf[1];
		idev->panel_hei = buf[4] << 8 | buf[3];
		idev->trans_xy = (idev->chip->core_ver >= CORE_VER_1430
			&& (idev->rib.nReportByPixel > 0)) ? ON : OFF;
		goto out;
	}

	len = (idev->chip->core_ver >= CORE_VER_1430) ? 6 : len;

	if (idev->wrapper(&cmd, sizeof(cmd), buf, len, ON, OFF) < 0)
		ipio_err("Read panel info error\n");

	if (buf[0] != P5_X_GET_PANEL_INFORMATION) {
		ipio_info("Invalid panel info, use default resolution\n");
		idev->panel_wid = TOUCH_SCREEN_X_MAX;
		idev->panel_hei = TOUCH_SCREEN_Y_MAX;
		idev->trans_xy = OFF;
	} else {
		idev->panel_wid = buf[1] << 8 | buf[2];
		idev->panel_hei = buf[3] << 8 | buf[4];
		idev->trans_xy = (idev->chip->core_ver >= CORE_VER_1430) ? buf[5] : OFF;
	}

out:
	ipio_info("Panel info: width = %d, height = %d\n", idev->panel_wid, idev->panel_hei);
	ipio_info("Transfer touch coordinate = %s\n", idev->trans_xy ? "ON" : "OFF");
	return ret;
}

int ilitek_tddi_ic_get_tp_info(void)
{
	int ret = 0;
	u8 cmd = 0;
	u8 buf[20] = {0};

	if (idev->info_from_hex  && (idev->chip->core_ver >= CORE_VER_1410)) {
		buf[1] = idev->fw_info[5];
		buf[2] = idev->fw_info[7];
		buf[3] = idev->fw_info[8];
		buf[4] = idev->fw_info[9];
		buf[5] = idev->fw_info[10];
		buf[6] = idev->fw_info[11];
		buf[7] = idev->fw_info[12];
		buf[8] = idev->fw_info[14];
		buf[11] = buf[7];
		buf[12] = buf[8];
		goto out;
	}

	cmd = P5_X_GET_TP_INFORMATION;

	if (idev->wrapper(&cmd, sizeof(cmd), buf, idev->protocol->tp_info_len, ON, OFF) < 0) {
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
	idev->min_x = buf[1];
	idev->min_y = buf[2];
	idev->max_x = buf[4] << 8 | buf[3];
	idev->max_y = buf[6] << 8 | buf[5];
	idev->xch_num = buf[7];
	idev->ych_num = buf[8];
	idev->stx = buf[11];
	idev->srx = buf[12];

	ipio_info("TP Info: min_x = %d, min_y = %d, max_x = %d, max_y = %d\n", idev->min_x, idev->min_y, idev->max_x, idev->max_y);
	ipio_info("TP Info: xch = %d, ych = %d, stx = %d, srx = %d\n", idev->xch_num, idev->ych_num, idev->stx, idev->srx);
	return ret;
}

static void ilitek_tddi_ic_check_protocol_ver(u32 pver)
{
	int i = 0;

	if (idev->protocol->ver == pver) {
		ipio_debug("same procotol version, do nothing\n");
		return;
	}

	for (i = 0; i < PROTOCL_VER_NUM - 1; i++) {
		if (protocol_info[i].ver == pver) {
			idev->protocol = &protocol_info[i];
			ipio_info("update protocol version = %x\n", idev->protocol->ver);
			return;
		}
	}

	ipio_err("Not found a correct protocol version in list, use newest version\n");
	idev->protocol = &protocol_info[PROTOCL_VER_NUM - 1];
}

int ilitek_tddi_ic_get_protocl_ver(void)
{
	int ret = 0;
	u8 cmd = P5_X_GET_PROTOCOL_VERSION;
	u8 buf[10] = {0};
	u32 ver;

	if (idev->info_from_hex) {
		buf[1] = idev->fw_info[72];
		buf[2] = idev->fw_info[73];
		buf[3] = idev->fw_info[74];
		goto out;
	}

	if (idev->wrapper(&cmd, sizeof(cmd), buf, idev->protocol->pro_ver_len, ON, OFF) < 0) {
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
	u32 pid = 0;

	if (!atomic_read(&idev->ice_stat)) {
		ipio_err("ice mode doesn't enable\n");
		return -1;
	}

	if (!pid) {
		if (ilitek_ice_mode_read(idev->chip->pid_addr, &pid, sizeof(u32)) < 0)
			ipio_err("Read chip id error\n");
	}
	if (ilitek_ice_mode_read(idev->chip->otp_addr, &idev->chip->otp_id, sizeof(u32)) < 0)
		ipio_err("Read otp id error\n");
	if (ilitek_ice_mode_read(idev->chip->ana_addr, &idev->chip->ana_id, sizeof(u32)) < 0)
		ipio_err("Read ana id error\n");

	if (pid == 0x98811117) {
		pid = (ILI9882_CHIP << 16)|(ILI_F << 8);
	}

	idev->chip->pid = pid;
	idev->chip->id = pid >> 16;
	idev->chip->type = (pid & 0x0000FF00) >> 8;
	idev->chip->ver = pid & 0xFF;
	idev->chip->otp_id &= 0xFF;
	idev->chip->ana_id &= 0xFF;

	ipio_info("CHIP: PID = %x\n",(idev->chip->pid >> 8));

	ilitek_tddi_ic_setting();
	return ret;
}

int ilitek_tddi_ic_dummy_check(bool mcu)
{
	int ret = 0, retry = 3;
	u32 wdata = 0xA55A5AA5;
	u32 rdata = 0;

	do {
		ret = ilitek_ice_mode_ctrl(ENABLE, mcu);
		if (ret < 0) {
			ipio_err("Failed to enable ice mode during ili_tddi_init\n");
			continue;
		}

		ret = ilitek_ice_mode_write(WDT9_DUMMY2, wdata, sizeof(u32));
		if (ret < 0) {
			ipio_err("Write dummy error\n");
			continue;
		}

		ret = ilitek_ice_mode_read(WDT9_DUMMY2, &rdata, sizeof(u32));
		if (ret < 0) {
			ipio_err("Read dummy error\n");
			continue;
		}

		if (rdata == wdata) {
			ipio_info("Ilitek IC check successe\n");
			ret = 0;
			break;
		}

		ipio_err("Dummy check incorrect, rdata = %x wdata = %x, retry = %d\n", rdata, wdata, retry);
	} while (--retry >= 0);

	if (retry < 0) {
		ipio_err("Ilitek IC check Fail\n");
		ret = -1;
	}

	return ret;
}

static struct ilitek_ic_info chip;

void ilitek_tddi_ic_init(void)
{
	chip.pid_addr =		   	TDDI_PID_ADDR;
	chip.wdt_addr =		   	TDDI_WDT_ADDR;
	chip.pc_counter_addr = 		TDDI_PC_COUNTER_ADDR;
	chip.pc_latch_addr =		TDDI_PC_LATCH_ADDR;
	chip.otp_addr =		   	TDDI_OTP_ID_ADDR;
	chip.ana_addr =		   	TDDI_ANA_ID_ADDR;
	chip.reset_addr =	   	TDDI_CHIP_RESET_ADDR;
	chip.spi_speed_ctrl =		ENABLE;

	idev->protocol = &protocol_info[PROTOCL_VER_NUM - 1];
	idev->chip = &chip;
}
