/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "osal_typedef.h"
#include "stp_exp.h"
#include "wmt_exp.h"
#include "plat.h"

#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_interface.h"
#include "fm_stdlib.h"
#include "fm_patch.h"
#include "fm_utils.h"
#include "fm_link.h"
#include "fm_config.h"
#include "fm_cmd.h"

#include "mt6631_fm_reg.h"
#include "mt6631_fm_lib.h"

#define HQA_RETURN_ZERO_MAP 0
#define HQA_ZERO_DESENSE_MAP 0

/* #include "mach/mt_gpio.h" */

/* #define MT6631_FM_PATCH_PATH "/etc/firmware/mt6631/mt6631_fm_patch.bin" */
/* #define MT6631_FM_COEFF_PATH "/etc/firmware/mt6631/mt6631_fm_coeff.bin" */
/* #define MT6631_FM_HWCOEFF_PATH "/etc/firmware/mt6631/mt6631_fm_hwcoeff.bin" */
/* #define MT6631_FM_ROM_PATH "/etc/firmware/mt6631/mt6631_fm_rom.bin" */

static struct fm_patch_tbl mt6631_patch_tbl[5] = {
	{FM_ROM_V1, "mt6631_fm_v1_patch.bin", "mt6631_fm_v1_coeff.bin", NULL, NULL},
	{FM_ROM_V2, "mt6631_fm_v2_patch.bin", "mt6631_fm_v2_coeff.bin", NULL, NULL},
	{FM_ROM_V3, "mt6631_fm_v3_patch.bin", "mt6631_fm_v3_coeff.bin", NULL, NULL},
	{FM_ROM_V4, "mt6631_fm_v4_patch.bin", "mt6631_fm_v4_coeff.bin", NULL, NULL},
	{FM_ROM_V5, "mt6631_fm_v5_patch.bin", "mt6631_fm_v5_coeff.bin", NULL, NULL}
};

static struct fm_hw_info mt6631_hw_info = {
	.chip_id = 0x00006631,
	.eco_ver = 0x00000000,
	.rom_ver = 0x00000000,
	.patch_ver = 0x00000000,
	.reserve = 0x00000000,
};


static struct fm_callback *fm_cb_op;

/* static signed int Chip_Version = mt6631_E1; */

/* static bool rssi_th_set = false; */

#if 0				/* def CONFIG_MTK_FM_50KHZ_SUPPORT */
static struct fm_fifo *cqi_fifo;
#endif
static signed int mt6631_is_dese_chan(unsigned short freq);

#if 0
static signed int mt6631_mcu_dese(unsigned short freq, void *arg);
static signed int mt6631_gps_dese(unsigned short freq, void *arg);
static signed int mt6631_I2s_Setting(signed int onoff, signed int mode, signed int sample);
#endif
static unsigned short mt6631_chan_para_get(unsigned short freq);
static signed int mt6631_desense_check(unsigned short freq, signed int rssi);
static bool mt6631_TDD_chan_check(unsigned short freq);
static bool mt6631_SPI_hopping_check(unsigned short freq);
static signed int mt6631_soft_mute_tune(unsigned short freq, signed int *rssi, signed int *valid);

static bool mt6631_do_SPI_hopping_26M(void)
{
	struct fm_ext_interface *ei = &fm_wcn_ops.ei;
	signed int hwid = ei->get_hw_version();
	signed int ret = 0;
	unsigned int tem = 0, hw_ver_id = 0;

	/* switch SPI clock to 26MHz */
	if (ei->spi_clock_switch)
		ret = ei->spi_clock_switch(FM_SPI_SPEED_26M);
	else {
		ret = -1;
		WCN_DBG(FM_ERR | CHIP, "Clock switch cb is null\n");
	}
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "Switch SPI clock to 26MHz failed\n");

	ret = fm_host_reg_read(0x80021010, &hw_ver_id);
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "%s: read HW ver. failed\n", __func__);
	hw_ver_id = hw_ver_id >> 16;

	/* unlock 64M */
	if (hwid >= FM_CONNAC_1_2) {
		if (hw_ver_id == 0x1005 || hw_ver_id == 0x1002) {
			ret = fm_host_reg_read(0x80023008, &tem);
			if (ret)
				WCN_DBG(FM_ERR | CHIP, "%s: unlock 64M reg 0x880023008 failed\n", __func__);
			ret = fm_host_reg_write(0x80023008, tem & (~(0x1 << 21)));
			if (ret)
				WCN_DBG(FM_ERR | CHIP, "%s: unlock 64M failed\n", __func__);
		} else {
			ret = fm_host_reg_read(0x81021128, &tem);
			if (ret)
				WCN_DBG(FM_ERR | CHIP, "%s: unlock 64M reg 0x81021128 failed\n", __func__);
			ret = fm_host_reg_write(0x81021128, tem & ~0x1);
			if (ret)
				WCN_DBG(FM_ERR | CHIP, "%s: unlock 64M failed\n", __func__);
			ret = fm_host_reg_read(0x81024040, &tem);
			if (ret)
				WCN_DBG(FM_ERR | CHIP, "%s: unlock 64M reg 0x81024040 failed\n", __func__);
			ret = fm_host_reg_write(0x81024040, tem & ~0x3);
			if (ret)
				WCN_DBG(FM_ERR | CHIP, "%s: unlock 64M failed\n", __func__);
		}
	} else {
		ret = fm_host_reg_read(0x80026000, &tem);
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: unlock 64M reg 0x80026000 failed\n", __func__);
		ret = fm_host_reg_write(0x80026000, tem & (~(0x1 << 28)));
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: unlock 64M failed\n", __func__);
	}

	if (hwid <= FM_CONNAC_1_2) {
		/* Rlease TOP2/64M sleep */
		fm_host_reg_read(0x81021138, &tem);   /* Set 0x81021138[7] = 0x0 */
		tem = tem & 0xFFFFFF7F;
		ret = fm_host_reg_write(0x81021138, tem);
		if (ret) {
			WCN_DBG(FM_ALT | CHIP, "Rlease TOP2/64M sleep failed\n");
			return ret;
		}
		WCN_DBG(FM_DBG | CHIP, "Rlease TOP2/64M sleep\n");
	}

	return ret == 0;
}

static bool mt6631_do_SPI_hopping_64M(unsigned short freq)
{
	struct fm_ext_interface *ei = &fm_wcn_ops.ei;
	signed int hwid = ei->get_hw_version();
	signed int ret = 0;
	signed int i = 0;
	unsigned int tem = 0, hw_ver_id = 0;
	bool flag_spi_hopping = false;

	if (!mt6631_SPI_hopping_check(freq))
		return true;

	/* SPI hopping setting*/
	WCN_DBG(FM_NTC | CHIP,
		"%s: freq:%d is SPI hopping channel,turn on 64M PLL\n",
		__func__, freq);

	ret = fm_host_reg_read(0x80021010, &hw_ver_id);
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "%s: read HW ver. failed\n", __func__);
	WCN_DBG(FM_NTC | CHIP, "%s: HW ver. ID = 0x%08x\n", __func__, hw_ver_id);
	hw_ver_id = hw_ver_id >> 16;

	if (hwid <= FM_CONNAC_1_2) {
		/*Disable TOP2/64M sleep*/
		ret = fm_host_reg_read(0x81021138, &tem);
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: read 64M reg 0x81021138 failed\n", __func__);
		tem |= 0x00000080;
		ret = fm_host_reg_write(0x81021138, tem);
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: disable 64M sleep failed\n", __func__);
	}
	/* lock 64M */
	if (hwid >= FM_CONNAC_1_2) {
		if (hw_ver_id == 0x1005 || hw_ver_id == 0x1002) {
			ret = fm_host_reg_read(0x80023008, &tem);
			if (ret)
				WCN_DBG(FM_ERR | CHIP,
					"%s: lock 64M reg 0x80023008 failed\n", __func__);
			ret = fm_host_reg_write(0x80023008, tem | (0x1 << 21));
			if (ret)
				WCN_DBG(FM_ERR | CHIP,
					"%s: lock 64M failed\n", __func__);
		} else {
			ret = fm_host_reg_read(0x81021128, &tem);
			if (ret)
				WCN_DBG(FM_ERR | CHIP,
					"%s: read reg 0x81021128 failed\n", __func__);
			ret = fm_host_reg_write(0x81021128, tem | 0x1);
			if (ret)
				WCN_DBG(FM_ERR | CHIP,
					"%s: enable 'rf_spi_div_en' failed\n", __func__);
			ret = fm_host_reg_read(0x81024040, &tem);
			if (ret)
				WCN_DBG(FM_ERR | CHIP,
					"%s: lock 64M reg 0x81024040 failed\n", __func__);
			ret = fm_host_reg_write(0x81024040, tem | 0x3);
			if (ret)
				WCN_DBG(FM_ERR | CHIP,
					"%s: lock 64M failed\n", __func__);
		}
	} else {
		ret = fm_host_reg_read(0x80026000, &tem);
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: lock 64M reg 0x80026000 failed\n", __func__);
		ret = fm_host_reg_write(0x80026000, tem | (0x1 << 28));
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: lock 64M failed\n", __func__);
	}

	for (i = 0; i < 100; i++) { /*rd 0x8002110C until D27 ==1*/

		if (hwid >= FM_CONNAC_1_2)
			fm_host_reg_read(0x80021118, &tem);
		else
			fm_host_reg_read(0x8002110C, &tem);

		if (tem & 0x08000000) {
			WCN_DBG(FM_NTC | CHIP, "%s: POLLING PLL_RDY success !\n", __func__);
			if (ei->spi_clock_switch)
				flag_spi_hopping = ei->spi_clock_switch(FM_SPI_SPEED_64M) == 0;
			break;
		}
		fm_delayus(10);
	}
	if (false == flag_spi_hopping)
		WCN_DBG(FM_ERR | CHIP,
			"%s: Polling to read rd 0x8002110C[27] ==0x1 failed !\n",
			__func__);

	return flag_spi_hopping;
}

static signed int mt6631_pwron(signed int data)
{
	if (mtk_wcn_wmt_func_on(WMTDRV_TYPE_FM) == MTK_WCN_BOOL_FALSE) {
		WCN_DBG(FM_ERR | CHIP, "WMT turn on FM Fail!\n");
		return -FM_ELINK;
	}

	WCN_DBG(FM_NTC | CHIP, "WMT turn on FM OK!\n");
	return 0;
}

static signed int mt6631_pwroff(signed int data)
{
	if (mtk_wcn_wmt_func_off(WMTDRV_TYPE_FM) == MTK_WCN_BOOL_FALSE) {
		WCN_DBG(FM_ERR | CHIP, "WMT turn off FM Fail!\n");
		return -FM_ELINK;
	}

	WCN_DBG(FM_NTC | CHIP, "WMT turn off FM OK!\n");
	return 0;
}

static unsigned short mt6631_get_chipid(void)
{
	return 0x6631;
}

/*  MT6631_SetAntennaType - set Antenna type
 *  @type - 1, Short Antenna;  0, Long Antenna
 */
static signed int mt6631_SetAntennaType(signed int type)
{
	unsigned short dataRead = 0;

	WCN_DBG(FM_DBG | CHIP, "set ana to %s\n", type ? "short" : "long");
	fm_reg_read(FM_MAIN_CG2_CTRL, &dataRead);

	if (type)
		dataRead |= ANTENNA_TYPE;
	else
		dataRead &= (~ANTENNA_TYPE);

	fm_reg_write(FM_MAIN_CG2_CTRL, dataRead);

	return 0;
}

static signed int mt6631_GetAntennaType(void)
{
	unsigned short dataRead = 0;

	fm_reg_read(FM_MAIN_CG2_CTRL, &dataRead);
	WCN_DBG(FM_DBG | CHIP, "get ana type: %s\n", (dataRead & ANTENNA_TYPE) ? "short" : "long");

	if (dataRead & ANTENNA_TYPE)
		return FM_ANA_SHORT;	/* short antenna */
	else
		return FM_ANA_LONG;	/* long antenna */
}

static signed int mt6631_Mute(bool mute)
{
	signed int ret = 0;
	unsigned short dataRead = 0;

	WCN_DBG(FM_DBG | CHIP, "set %s\n", mute ? "mute" : "unmute");
	/* fm_reg_read(FM_MAIN_CTRL, &dataRead); */
	fm_reg_read(0x9C, &dataRead);

	/* fm_top_reg_write(0x0050, 0x00000007); */
	if (mute == 1)
		ret = fm_reg_write(0x9C, (dataRead & 0xFFFC) | 0x0003);
	else
		ret = fm_reg_write(0x9C, (dataRead & 0xFFFC));

	/* fm_top_reg_write(0x0050, 0x0000000F); */

	return ret;
}

static signed int mt6631_rampdown_reg_op(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 4;

	if (buf == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid pointer\n", __func__);
		return -1;
	}
	if (buf_size < TX_BUF_SIZE) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid buf size(%d)\n", __func__, buf_size);
		return -2;
	}

	/* Clear DSP state */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, 0xFFF0, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	/* Set DSP ramp down state */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, 0xFFFF, RAMP_DOWN, &buf[pkt_size], buf_size - pkt_size);
	/* @Wait for STC_DONE interrupt@ */
	pkt_size += fm_bop_rd_until(FM_MAIN_INTR, FM_INTR_STC_DONE, FM_INTR_STC_DONE, &buf[pkt_size],
				    buf_size - pkt_size);
	/* Clear DSP ramp down state */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, (~RAMP_DOWN), 0x0000, &buf[pkt_size], buf_size - pkt_size);
	/* Write 1 clear the STC_DONE interrupt status flag */
	pkt_size += fm_bop_modify(FM_MAIN_INTR, 0xFFFF, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);

	return pkt_size - 4;
}

void mt8168_spi_hopping_clock_switch(bool clk_64M)
{
	signed int ret = 0;
	unsigned int reg_val = 0;
	bool flag_spi_hopping = false;
	unsigned int i = 0;

	if (clk_64M == true) {
		/*Disable TOP2/64M sleep*/
		ret = fm_host_reg_read(0x81021138, &reg_val);
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: read 64M reg 0x81021138 failed\n", __func__);
		reg_val |= (0x1 << 7);
		ret = fm_host_reg_write(0x81021138, reg_val);
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: disable 64M sleep failed\n", __func__);

		/* lock 64M */
		ret = fm_host_reg_read(0x80023008, &reg_val);
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: lock 64M reg 0x80023008 failed\n", __func__);
		ret = fm_host_reg_write(0x80023008, reg_val | (0x1 << 21));
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: lock 64M failed\n", __func__);

		for (i = 0; i < 100; i++) { /*rd 0x80021118 until D27 ==1*/

			ret = fm_host_reg_read(0x80021118, &reg_val);

			if (reg_val & 0x08000000) {
				flag_spi_hopping = true;
				WCN_DBG(FM_NTC | CHIP, "%s: POLLING PLL_RDY success !\n", __func__);
				/* switch SPI clock to 64MHz */
				ret = fm_host_reg_read(0x81026004, &reg_val); /* wr 0x81026004[0] 0x1   D0 */
				reg_val |= 0x00000001;
				ret = fm_host_reg_write(0x81026004, reg_val);
				break;
			}
			fm_delayus(10);
		}
		if (false == flag_spi_hopping)
			WCN_DBG(FM_ERR | CHIP, "%s: Polling to read rd 0x80021118[27] == 0x1 failed !\n", __func__);

		flag_spi_hopping = false;
		for (i = 0; i < 100; i++) { /*rd 0x8002110C until D27 ==1*/
			ret = fm_host_reg_read(0x81026004, &reg_val);
			if ((reg_val & 0x00000018) == 0x00000010) {
				flag_spi_hopping = true;
				WCN_DBG(FM_NTC | CHIP, "%s: POLLING switch command success !\n", __func__);
				/* switch SPI clock to 64MHz */
				ret = fm_host_reg_read(0x81026004, &reg_val); /* wr 0x81026004[0] 0x1   D0 */
				reg_val |= 0x00000004;
				ret = fm_host_reg_write(0x81026004, reg_val);
				break;
			}
			fm_delayus(10);
		}
		if (false == flag_spi_hopping)
			WCN_DBG(FM_ERR | CHIP, "%s: Polling to read rd 0x80021118[4:3] == 0x2 failed !\n", __func__);
	} else {
		/*set next_cap_en*/
		ret = fm_host_reg_read(0x81026004, &reg_val); /* wr 0x81026004[2] 0x0   D0 */
		reg_val &= 0xFFFFFFFB;
		ret = fm_host_reg_write(0x81026004, reg_val);

		/*switch back to 26M*/
		ret = fm_host_reg_read(0x81026004, &reg_val); /* wr 0x81026004[0] 0x0   D0 */
		reg_val &= 0xFFFFFFFE;
		ret = fm_host_reg_write(0x81026004, reg_val);

		flag_spi_hopping = false;
		for (i = 0; i < 100; i++) {
			ret = fm_host_reg_read(0x81026004, &reg_val);
			if ((reg_val & 0x00000018) == 0x8) {
				flag_spi_hopping = true;
				WCN_DBG(FM_NTC | CHIP, "%s: POLLING switch command success !\n", __func__);

				/* turn off 64M PLL request */
				ret = fm_host_reg_read(0x80023008, &reg_val);
				if (ret)
					WCN_DBG(FM_ERR | CHIP, "%s: turn off 0x80023008 failed\n", __func__);
				ret = fm_host_reg_write(0x80023008, reg_val & (~(0x1 << 21)));
				if (ret)
					WCN_DBG(FM_ERR | CHIP, "%s: lock 64M failed\n", __func__);
				break;
			}
			fm_delayus(10);
		}
		if (false == flag_spi_hopping)
			WCN_DBG(FM_ERR | CHIP, "%s: Polling to read rd 0x80021118[4:3] == 0x2 failed !\n", __func__);

		ret = fm_host_reg_read(0x81021138, &reg_val);
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: read 64M reg 0x81021138 failed\n", __func__);
		reg_val &= ~(0x1 << 7);
		ret = fm_host_reg_write(0x81021138, reg_val);
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: enable 64M sleep failed\n", __func__);
	}
}

/*
 * mt6631_rampdown - f/w will wait for STC_DONE interrupt
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6631_rampdown(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6631_rampdown_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_RAMPDOWN_OPCODE, pkt_size);
}

/* FMSYS Ramp Down Sequence*/
static signed int mt6631_RampDown(void)
{
	signed int ret = 0;
	unsigned short pkt_size;
	signed int projectid = fm_cb_op->projectid_get();
	/* unsigned short tmp; */

	WCN_DBG(FM_DBG | CHIP, "ramp down\n");

	/* unlock 64M */
	if (projectid == 0x8168)
		mt8168_spi_hopping_clock_switch(false);
	else
		mt6631_do_SPI_hopping_26M();

	/* A0.0 Host control RF register */
	ret = fm_set_bits(0x60, 0x0007, 0xFFF0);  /*Set 0x60 [D3:D0] = 0x7*/
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down HOST control rf: Set 0x60 [D3:D0] = 0x7 failed\n");
		return ret;
	}

	/* A0.1 Update FM ADPLL fast tracking mode gain */
	ret = fm_set_bits(0x0F, 0x0000, 0xF800);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down ADPLL gainA/B: Set 0xFH [D10:D0] = 0x000 failed\n");
		return ret;
	}

	/* A0.2 Host control RF register */
	ret = fm_set_bits(0x60, 0x000F, 0xFFF0);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down Host control RF registerwr top 0x60 failed\n");
		return ret;
	}
	/*Clear dsp state*/
	ret = fm_set_bits(0x63, 0x0000, 0xFFF0);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down Host control RF registerwr top 0x63 failed\n");
		return ret;
	}
	/* Set DSP ramp down state*/
	ret = fm_set_bits(0x63, 0x0010, 0xFFEF);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down Host control RF registerwr top 0x63 failed\n");
		return ret;
	}

	ret = fm_reg_write(FM_MAIN_INTRMASK, 0x0000);
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "ramp down clean FM_MAIN_INTRMASK failed\n");

	ret = fm_reg_write(FM_MAIN_EXTINTRMASK, 0x0000);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down clean FM_MAIN_EXTINTRMASK failed\n");
		return ret;
	}

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6631_rampdown(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_RAMPDOWN, SW_RETRY_CNT, RAMPDOWN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down failed\n");
		return ret;
	}

	ret = fm_reg_write(FM_MAIN_EXTINTRMASK, 0x0021);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down wr FM_MAIN_EXTINTRMASK failed\n");
		return ret;
	}

	ret = fm_reg_write(FM_MAIN_INTRMASK, 0x0021);
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "ramp down wr FM_MAIN_INTRMASK failed\n");

#if 0
	fm_delayms(1);
	WCN_DBG(FM_DBG | CHIP, "ramp down delay 1ms\n");

	/* A1.1. Disable aon_osc_clk_cg */
	ret = fm_host_reg_write(0x81024064, 0x00000004);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " Disable aon_osc_clk_cg failed\n");
		return ret;
	}
	/* A1.1. Disable FMAUD trigger */
	ret = fm_host_reg_write(0x81024058, 0x88800000);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "Disable FMAUD trigger failed\n");
		return ret;
	}

	/* A1.1. issue fmsys memory powr down */
	ret = fm_host_reg_write(0x81024054, 0x00000180);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " Issue fmsys memory powr down failed\n");
		return ret;
	}
#endif

	return ret;
}

static signed int mt6631_get_rom_version(void)
{
	unsigned short flag_Romcode = 0;
	unsigned short nRomVersion = 0;
#define ROM_CODE_READY 0x0001
	signed int ret = 0;

	/* A1.1 DSP rom code version request enable --- set 0x61 b15=1 */
	fm_set_bits(0x61, 0x8000, 0x7FFF);

	/* A1.2 Release ASIP reset --- set 0x61 b1=1 */
	fm_set_bits(0x61, 0x0002, 0xFFFD);

	/* A1.3 Enable ASIP power --- set 0x61 b0=0 */
	fm_set_bits(0x61, 0x0000, 0xFFFE);

	/* A1.4 Wait until DSP code version ready --- wait loop 1ms */
	do {
		fm_delayus(1000);
		ret = fm_reg_read(0x84, &flag_Romcode);
		/* ret=-4 means signal got when control FM. usually get sig 9 to kill FM process. */
		/* now cancel FM power up sequence is recommended. */
		if (ret)
			return ret;

		WCN_DBG(FM_DBG | CHIP, "ROM_CODE_READY flag 0x84=%x\n", flag_Romcode);
	} while (flag_Romcode != ROM_CODE_READY);


	/* A1.5 Read FM DSP code version --- rd 0x83[15:8] */
	fm_reg_read(0x83, &nRomVersion);
	nRomVersion = (nRomVersion >> 8);

	/* A1.6 DSP rom code version request disable --- set 0x61 b15=0 */
	fm_set_bits(0x61, 0x0000, 0x7FFF);

	/* A1.7 Reset ASIP --- set 0x61[1:0] = 1 */
	fm_set_bits(0x61, 0x0001, 0xFFFC);

	return (signed int) nRomVersion;
}

static signed int mt6631_pwrup_clock_on_reg_op(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 4;
	unsigned short de_emphasis;
	signed int hwid = fm_wcn_ops.ei.get_hw_version();
	/* unsigned short osc_freq; */

	if (buf == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid pointer\n", __func__);
		return -1;
	}
	if (buf_size < TX_BUF_SIZE) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid buf size(%d)\n", __func__, buf_size);
		return -2;
	}

	de_emphasis = fm_config.rx_cfg.deemphasis;
	de_emphasis &= 0x0001;	/* rang 0~1 */
	/* 2,turn on top clock */
	pkt_size += fm_bop_top_write(0xA10, 0xFFFFFFFF, &buf[pkt_size], buf_size - pkt_size);
	/* wr top cr a10 ffffffff */

	/* 3,enable MTCMOS */
	pkt_size += fm_bop_top_write(0x60, 0x00000030, &buf[pkt_size], buf_size - pkt_size);
	/* wr top 60 30 */

	pkt_size += fm_bop_top_write(0x60, 0x00000035, &buf[pkt_size], buf_size - pkt_size);
	/* wr top 60 35 */
	pkt_size += fm_bop_udelay(10, &buf[pkt_size], buf_size - pkt_size);
	/* delay 10us */
	pkt_size += fm_bop_top_write(0x60, 0x00000015, &buf[pkt_size], buf_size - pkt_size);
	/* wr top 60 15 */
	pkt_size += fm_bop_top_write(0x60, 0x00000005, &buf[pkt_size], buf_size - pkt_size);
	/* wr top 60 5 */

	pkt_size += fm_bop_udelay(10, &buf[pkt_size], buf_size - pkt_size);
	/* delay 10us */
	pkt_size += fm_bop_top_write(0x60, 0x00000045, &buf[pkt_size], buf_size - pkt_size);
	/* wr top 60 45 */

	/* 4,set comspi fm slave dumy count	*/
	if (hwid >= FM_CONNAC_1_5)
		pkt_size += fm_bop_write(0x7f, 0x801f, &buf[pkt_size], buf_size - pkt_size);	/* wr 7f 801f */
	else
		pkt_size += fm_bop_write(0x7f, 0x800f, &buf[pkt_size], buf_size - pkt_size);	/* wr 7f 800f */

	/* A. FM digital clock enable */
	/* A1. Enable digital OSC */
	if (hwid <= FM_CONNAC_1_2)
		pkt_size += fm_bop_write(0x60, 0x00000001, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 1 */

	/* A2. Wait 3ms */
	pkt_size += fm_bop_udelay(3000, &buf[pkt_size], buf_size - pkt_size);

	/* A3. Set OSC clock output to FM */
	if (hwid <= FM_CONNAC_1_2)
		pkt_size += fm_bop_write(0x60, 0x00000003, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 3 */
	/* A4. Release HW clock gating*/
	pkt_size += fm_bop_write(0x60, 0x00000007, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 7 */
	/* Enable DSP auto clock gating */
	pkt_size += fm_bop_write(0x70, 0x0040, &buf[pkt_size], buf_size - pkt_size);	/* wr 70 0040 */
	/* A7. Deemphasis setting: Set 0 for 50us, Set 1 for 75us */
	pkt_size += fm_bop_modify(0x61, ~DE_EMPHASIS, (de_emphasis << 12), &buf[pkt_size], buf_size - pkt_size);

	return pkt_size - 4;
}
/*
 * mt6631_pwrup_clock_on - Wholechip FM Power Up: step 1, FM Digital Clock enable
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6631_pwrup_clock_on(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6631_pwrup_clock_on_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_ENABLE_OPCODE, pkt_size);
}

static signed int mt6631_pwrup_digital_init_reg_op(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 4;

	if (buf == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid pointer\n", __func__);
		return -1;
	}
	if (buf_size < TX_BUF_SIZE) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid buf size(%d)\n", __func__, buf_size);
		return -2;
	}

	/* Part D   FM RF&ADPLL divider setting */

	/* D2.1 set cell mode */
	/* wr 30 D3:D2 00:FDD(default),01:both.10: TDD, 11 FDD */
	/* pkt_size += fm_bop_modify(0x30, 0xFFF3, 0x0000, &buf[pkt_size], buf_size - pkt_size); */

	/* D2.2 set ADPLL divider */
	pkt_size += fm_bop_write(0x21, 0xE000, &buf[pkt_size], buf_size - pkt_size);	/* wr 21 E000 */
	/* D2.3 set SDM coeff0_H */
	pkt_size += fm_bop_write(0xD8, 0x03F0, &buf[pkt_size], buf_size - pkt_size);	/* wr D8 0x03F0 */
	/* D2.4 set SDM coeff0_L */
	pkt_size += fm_bop_write(0xD9, 0x3F04, &buf[pkt_size], buf_size - pkt_size);	/* wr D9 0x3F04 */
	/* D2.5 set SDM coeff1_H */
	pkt_size += fm_bop_write(0xDA, 0x0014, &buf[pkt_size], buf_size - pkt_size);	/* wr DA 0x0014 */
	/* D2.6 set SDM coeff1_L */
	pkt_size += fm_bop_write(0xDB, 0x2A38, &buf[pkt_size], buf_size - pkt_size);	/* wr DB 0x2A38 */
	/* D2.7 set 26M clock */
	pkt_size += fm_bop_write(0x23, 0x4000, &buf[pkt_size], buf_size - pkt_size);	/* wr 23 4000 */

	/* Part E: FM Digital Init: fm_rgf_maincon */

	/* E4. Set appropriate interrupt mask behavior as desired */
	/* Enable stc_done_mask, Enable rgf_rds_mask*/
	pkt_size += fm_bop_write(0x6A, 0x0021, &buf[pkt_size], buf_size - pkt_size);	/* wr 6A 0021 */
	pkt_size += fm_bop_write(0x6B, 0x0021, &buf[pkt_size], buf_size - pkt_size);	/* wr 6B 0021 */

	/* E5. Enable hw auto control */
	pkt_size += fm_bop_write(0x60, 0x0000000F, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 f */

	/* E6. Release ASIP reset */
	pkt_size += fm_bop_modify(0x61, 0xFFFD, 0x0002, &buf[pkt_size], buf_size - pkt_size);	/* wr 61 D1=1 */
	/* E7. Enable ASIP power */
	pkt_size += fm_bop_modify(0x61, 0xFFFE, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 61 D0=0 */

	/* E8 */
	pkt_size += fm_bop_udelay(100000, &buf[pkt_size], buf_size - pkt_size);	/* delay 100ms */
	/* E9. Check HW initial complete */
	pkt_size += fm_bop_rd_until(0x64, 0x001F, 0x0002, &buf[pkt_size], buf_size - pkt_size);	/* Poll 64[0~4] = 2 */

	return pkt_size - 4;
}

/*
 * mt6631_pwrup_digital_init - Wholechip FM Power Up: step 4, FM Digital Init: fm_rgf_maincon
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6631_pwrup_digital_init(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6631_pwrup_digital_init_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_ENABLE_OPCODE, pkt_size);
}

static signed int mt6631_pwrup_fine_tune_reg_op(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 4;

	if (buf == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid pointer\n", __func__);
		return -1;
	}
	if (buf_size < TX_BUF_SIZE) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid buf size(%d)\n", __func__, buf_size);
		return -2;
	}

	/* F1 set host control RF register */
	pkt_size += fm_bop_write(0x60, 0x00000007, &buf[pkt_size], buf_size - pkt_size);
	/* F2 fine tune RF setting */
	pkt_size += fm_bop_write(0x01, 0xBEE8, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x03, 0xF6ED, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x15, 0x0D80, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x16, 0x0068, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x17, 0x092A, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x34, 0x807F, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x35, 0x311E, &buf[pkt_size], buf_size - pkt_size);
	/* F3 set DSP control RF register */
	pkt_size += fm_bop_write(0x60, 0x0000000F, &buf[pkt_size], buf_size - pkt_size);

	return pkt_size - 4;
}

/*
 * mt6631_pwrup_fine_tune - Wholechip FM Power Up: step 5, FM RF fine tune setting
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6631_pwrup_fine_tune(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6631_pwrup_fine_tune_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_ENABLE_OPCODE, pkt_size);
}

static signed int mt6631_pwrdown_reg_op(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 4;

	if (buf == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid pointer\n", __func__);
		return -1;
	}
	if (buf_size < TX_BUF_SIZE) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid buf size(%d)\n", __func__, buf_size);
		return -2;
	}

	/* A1:set audio output I2S Tx mode: */
	pkt_size += fm_bop_modify(0x9B, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);

	/* B0:Disable HW clock control */
	pkt_size += fm_bop_write(0x60, 0x330F, &buf[pkt_size], buf_size - pkt_size);
	/* B1:Reset ASIP : Set 0x61,  [D1 = 0, D0=1] */
	pkt_size += fm_bop_modify(0x61, 0xFFFD, 0x0001, &buf[pkt_size], buf_size - pkt_size);

	/* B2:digital core + digital rgf reset */
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);

	/* B3:Disable all clock */
	pkt_size += fm_bop_write(0x60, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	/* B4:Reset rgfrf */
	pkt_size += fm_bop_write(0x60, 0x4000, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x60, 0x0000, &buf[pkt_size], buf_size - pkt_size);

	/* MTCMOS power off */
	/* C0:disable MTCMOS */
	pkt_size += fm_bop_top_write(0x60, 0x0005, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_top_write(0x60, 0x0015, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_top_write(0x60, 0x0035, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_top_write(0x60, 0x0030, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_top_rd_until(0x60, 0x0000000A, 0x0, &buf[pkt_size], buf_size - pkt_size);

	return pkt_size - 4;
}
/*
 * mt6631_pwrdown - Wholechip FM Power down: Digital Modem Power Down
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6631_pwrdown(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6631_pwrdown_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_ENABLE_OPCODE, pkt_size);
}

static signed int mt6631_tune_reg_op(unsigned char *buf, signed int buf_size, unsigned short freq,
					unsigned short chan_para)
{
	signed int pkt_size = 4;

	WCN_DBG(FM_ALT | CHIP, "%s enter mt6631_tune function\n", __func__);

	if (buf == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid pointer\n", __func__);
		return -1;
	}
	if (buf_size < TX_BUF_SIZE) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid buf size(%d)\n", __func__, buf_size);
		return -2;
	}

	/* A2 Enable hardware controlled tuning sequence */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, 0xFFF8, TUNE, &buf[pkt_size], buf_size - pkt_size);/*Set 0x63 D0=1*/
	/* Wait for STC_DONE interrupt */

#ifdef FM_TUNE_USE_POLL
	/* A3 Wait for STC_DONE interrupt */
	/* A4 Wait for STC_DONE interrupt status flag */
	pkt_size += fm_bop_rd_until(FM_MAIN_INTR, FM_INTR_STC_DONE,
								FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);
	/* A6 Write 1 clear the STC_DONE interrupt status flag */
	pkt_size += fm_bop_modify(FM_MAIN_INTR, 0xFFFF, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);
#endif

	pkt_size += fm_bop_udelay(100000, &buf[pkt_size], buf_size - pkt_size);
	WCN_DBG(FM_ALT | CHIP, "mt6631_tune delay 100 ms wait 0x69 to change\n");

	WCN_DBG(FM_ALT | CHIP, "%s leave mt6631_tune function\n", __func__);

	return pkt_size - 4;
}

/*
 * mt6631_tune - execute tune action,
 * @buf - target buf
 * @buf_size - buffer size
 * @freq - 760 ~ 1080, 100KHz unit
 * return package size
 */
static signed int mt6631_tune(unsigned char *buf, signed int buf_size, unsigned short freq,
				unsigned short chan_para)
{
	signed int pkt_size = 0;

	pkt_size = mt6631_tune_reg_op(buf, buf_size, freq, chan_para);
	return fm_op_seq_combine_cmd(buf, FM_TUNE_OPCODE, pkt_size);
}

/*
 * mt6631_pwrup_DSP_download - execute dsp/coeff patch dl action,
 * @patch_tbl - current chip patch table
 * return patch dl ok or not
 */
static signed int mt6631_pwrup_DSP_download(struct fm_patch_tbl *patch_tbl)
{
#define PATCH_BUF_SIZE (4096*6)
	signed int ret = 0;
	signed int patch_len = 0;
	unsigned char *dsp_buf = NULL;
	unsigned short tmp_reg = 0;

	mt6631_hw_info.eco_ver = (signed int) mtk_wcn_wmt_ic_info_get(1);
	WCN_DBG(FM_DBG | CHIP, "ECO version:0x%08x\n", mt6631_hw_info.eco_ver);

	/*  Wholechip FM Power Up: step 3, get mt6631 DSP ROM version */
	ret = mt6631_get_rom_version();
	if (ret >= 0) {
		mt6631_hw_info.rom_ver = ret;
		WCN_DBG(FM_NTC | CHIP, "%s ROM version: v%d\n", __func__, mt6631_hw_info.rom_ver);
	} else {
		WCN_DBG(FM_ERR | CHIP, "get ROM version failed\n");
		if (ret == -4)
			WCN_DBG(FM_ERR | CHIP, "signal got when control FM, usually get sig 9 to kill FM process.\n");
			/* now cancel FM power up sequence is recommended. */
		goto out;
	}

	/* Wholechip FM Power Up: step 4 download patch */
	dsp_buf = fm_vmalloc(PATCH_BUF_SIZE);
	if (!dsp_buf) {
		WCN_DBG(FM_ALT | CHIP, "-ENOMEM\n");
		return -ENOMEM;
	}

	patch_len = fm_get_patch_path(mt6631_hw_info.rom_ver, dsp_buf, PATCH_BUF_SIZE, patch_tbl);
	if (patch_len <= 0) {
		WCN_DBG(FM_ALT | CHIP, " fm_get_patch_path failed\n");
		ret = patch_len;
		goto out;
	}

	ret = fm_download_patch((const unsigned char *)dsp_buf, patch_len, IMG_PATCH);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " DL DSPpatch failed\n");
		goto out;
	}

	patch_len = fm_get_coeff_path(mt6631_hw_info.rom_ver, dsp_buf, PATCH_BUF_SIZE, patch_tbl);
	if (patch_len <= 0) {
		WCN_DBG(FM_ALT | CHIP, " fm_get_coeff_path failed\n");
		ret = patch_len;
		goto out;
	}

	mt6631_hw_info.rom_ver += 1;

	tmp_reg = dsp_buf[38] | (dsp_buf[39] << 8);	/* to be confirmed */
	mt6631_hw_info.patch_ver = (signed int) tmp_reg;
	WCN_DBG(FM_NTC | CHIP, "Patch version: 0x%08x\n", mt6631_hw_info.patch_ver);

	ret = fm_download_patch((const unsigned char *)dsp_buf, patch_len, IMG_COEFFICIENT);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " Download DSP coefficient failed\n");
		goto out;
	}

	/* Download HWACC coefficient */
	fm_reg_write(0x92, 0x0000);
	fm_reg_write(0x90, 0x0040); /* Reset download control  */
	fm_reg_write(0x90, 0x0000); /* Disable memory control from host*/
out:
	if (dsp_buf)
		fm_vfree(dsp_buf);
	return ret;
}
static void mt6631_show_reg(void)
{
	unsigned int host_reg[3] = {0};
	unsigned int debug_reg1[3] = {0};
	unsigned short debug_reg2[3] = {0};

	fm_host_reg_read(0x81024030, &host_reg[0]);
	fm_host_reg_read(0x81021234, &host_reg[1]);
	fm_host_reg_read(0x81021138, &host_reg[2]);
	WCN_DBG(FM_ALT | CHIP,
		"host read 0x81024030 = 0x%08x, 0x81021234 = 0x%08x, 0x81021138 = 0x%08x\n",
		host_reg[0], host_reg[1], host_reg[2]);

	fm_top_reg_read(0x00c0, &debug_reg1[0]);
	fm_top_reg_read(0x00c8, &debug_reg1[1]);
	fm_top_reg_read(0x0060, &debug_reg1[2]);
	fm_reg_read(0x7f, &debug_reg2[0]);
	fm_reg_read(0x62, &debug_reg2[1]);
	fm_reg_read(0x60, &debug_reg2[2]);
	WCN_DBG(FM_ALT | CHIP,
		"top cr 0xc0 = 0x%08x, 0xc8 = 0x%08x, 0x60 = 0x%08x, fmreg 0x7f = 0x%08x, 0x62 = 0x%08x, 0x60 = 0x%08x\n",
		debug_reg1[0], debug_reg1[1], debug_reg1[2], debug_reg2[0], debug_reg2[1], debug_reg2[2]);
}

static signed int mt6631_PowerUp(unsigned short *chip_id, unsigned short *device_id)
{
	signed int ret = 0;
	unsigned short pkt_size;
	unsigned short tmp_reg = 0;
	unsigned int tem = 0;
	unsigned int host_reg = 0;
	signed int hwid = fm_wcn_ops.ei.get_hw_version();

	if (chip_id == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (device_id == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	WCN_DBG(FM_DBG | CHIP, "pwr on seq......\n");

	/* Wholechip FM Power Up: step 1, set common SPI parameter */
	if (hwid >= FM_CONNAC_1_5)
		ret = fm_host_reg_write(0x8102600C, 0x0000801F);
	else
		ret = fm_host_reg_write(0x8102600C, 0x0000800F);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwrup set CSPI failed\n");
		return ret;
	}

	if (hwid >= FM_CONNAC_1_0) {
		/* Set top_clk_en_adie to trigger sleep controller before FM power on */
		if (hwid >= FM_CONNAC_1_5) {
			ret = fm_host_reg_write(0x81021500, 0x00000003);
			if (ret) {
				WCN_DBG(FM_ALT | CHIP, " pwrup set top_clk_en_adie failed\n");
				return ret;
			}
		} else {
			fm_host_reg_read(0x81021500, &tem);   /* Set 0x81021500[1] = 0x1 */
			tem = tem | 0x00000002;
			fm_host_reg_write(0x81021500, tem);
		}

		/* Disable 26M crystal sleep */
		fm_host_reg_read(0x81021200, &tem);   /* Set 0x81021200[23] = 0x1 */
		tem = tem | 0x00800000;
		fm_host_reg_write(0x81021200, tem);
	} else {
		/* Set top_clk_en_adie to trigger sleep controller before FM power on */
		fm_host_reg_read(0x81024030, &tem);   /* Set 0x81024030[1] = 0x1 */
		tem = tem | 0x00000002;
		fm_host_reg_write(0x81024030, tem);

		/* Disable 26M crystal sleep */
		fm_host_reg_read(0x81021234, &tem);   /* Set 0x81021234[7] = 0x1 */
		tem = tem | 0x00000080;
		fm_host_reg_write(0x81021234, tem);
	}

	/* turn on RG_TOP_BGLDO */
	ret = fm_top_reg_read(0x00c0, &host_reg);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "power up read top 0xc0 failed\n");
		return ret;
	}
	ret = fm_top_reg_write(0x00c0, host_reg | (0x3 << 27));
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "power up write top 0xc0 failed\n");
		return ret;
	}

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;

	pkt_size = mt6631_pwrup_clock_on(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6631_pwrup_clock_on failed\n");
		return ret;
	}

	/* Wholechip FM Power Up: step 2, read HW version */
	mt6631_show_reg();
	fm_reg_read(0x62, &tmp_reg);
	/* chip_id = tmp_reg; */
	if (tmp_reg == 0x6631)
		*chip_id = 0x6631;
	*device_id = tmp_reg;
	mt6631_hw_info.chip_id = (signed int) tmp_reg;
	WCN_DBG(FM_DBG | CHIP, "chip_id:0x%04x\n", tmp_reg);

	if ((mt6631_hw_info.chip_id != 0x6631)) {
		mt6631_show_reg();
		WCN_DBG(FM_NTC | CHIP, "fm sys error, reset hw, chip_id = 0x%08x\n", mt6631_hw_info.chip_id);
		return -FM_EFW;

	}
	/* Wholechip FM Power Up: step 3, patch download */
	ret = mt6631_pwrup_DSP_download(mt6631_patch_tbl);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "mt6631_pwrup_DSP_download failed\n");
		return ret;
	}

	/* Wholechip FM Power Up: step 4, FM Digital Init: fm_rgf_maincon */
	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6631_pwrup_digital_init(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6631_pwrup_digital_init failed\n");
		return ret;
	}

	/* Wholechip FM Power Up: step 5, FM RF fine tune setting */
	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6631_pwrup_fine_tune(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6631_pwrup_fine_tune failed\n");
		return ret;
	}

	/* Enable connsys FM 2 wire RX */
	fm_reg_write(0x9B, 0xF9AB);                /* G2: Set audio output i2s TX mode */
	fm_host_reg_write(0x81024064, 0x00000014); /* G3: Enable aon_osc_clk_cg */
	/* G4: Enable FMAUD trigger, 20170119 */
	if (hwid >= FM_CONNAC_1_5)
		fm_host_reg_write(0x81024058, 0x888100C3);
	else
		fm_host_reg_write(0x81024058, 0x888100F3);
	fm_host_reg_write(0x81024054, 0x00000100); /* G5: Release fmsys memory power down*/

	WCN_DBG(FM_NTC | CHIP, "pwr on seq ok\n");

	return ret;
}

static signed int mt6631_PowerDown(void)
{
	signed int ret = 0;
	unsigned int tem = 0;
	unsigned short pkt_size;
	signed int hwid = fm_wcn_ops.ei.get_hw_version();
	/* unsigned int host_reg = 0; */

	WCN_DBG(FM_DBG | CHIP, "pwr down seq\n");

	/* A0.1. Disable aon_osc_clk_cg */
	ret = fm_host_reg_write(0x81024064, 0x00000004);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " Disable aon_osc_clk_cg failed\n");
		return ret;
	}
	/* A0.1. Disable FMAUD trigger */
	ret = fm_host_reg_write(0x81024058, 0x88800000);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " Disable FMAUD trigger failed\n");
		return ret;
	}

	/* A0.1. issue fmsys memory powr down */
	ret = fm_host_reg_write(0x81024054, 0x00000180);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " Issue fmsys memory powr down failed\n");
		return ret;
	}

	mt6631_do_SPI_hopping_26M();

	/* Enable 26M crystal sleep */
	if (hwid >= FM_CONNAC_1_0 && hwid <= FM_CONNAC_1_2) {
		WCN_DBG(FM_DBG | CHIP, "PowerDown: Enable 26M crystal sleep,Set 0x81021200[23] = 0x0\n");
		fm_host_reg_read(0x81021200, &tem);   /* Set 0x81021200[23] = 0x0 */
		tem = tem & 0xFF7FFFFF;
		fm_host_reg_write(0x81021200, tem);
	} else {
		WCN_DBG(FM_DBG | CHIP, "PowerDown: Enable 26M crystal sleep,Set 0x81021234[7] = 0x0\n");
		fm_host_reg_read(0x81021234, &tem);   /* Set 0x81021234[7] = 0x0 */
		tem = tem & 0xFFFFFF7F;
		fm_host_reg_write(0x81021234, tem);
	}

	if (ret)
		WCN_DBG(FM_ALT | CHIP, "PowerDown: Enable 26M crystal sleep,Set 0x81021234[7] = 0x0 failed\n");

	/* A0:set audio output I2X Rx mode: */
	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6631_pwrdown(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6631_pwrdown failed\n");
		return ret;
	}
	/* FIX_ME, disable ext interrupt */
	/* fm_reg_write(FM_MAIN_EXTINTRMASK, 0x00); */


	/* D0.  Clear top_clk_en_adie to indicate sleep controller after FM power off */
	/* ret = fm_host_reg_read(0x80101030, &host_reg);
	 * if (ret) {
	 *	WCN_DBG(FM_ALT | CHIP, " pwroff read 0x80100030 failed\n");
	 *	return ret;
	 * }
	 * ret = fm_host_reg_write(0x80101030, host_reg & (~(0x1 << 1)));
	 * if (ret) {
	 *	WCN_DBG(FM_ALT | CHIP, " pwroff disable top_ck_en_adie failed\n");
	 *	return ret;
	 * }
	 */
	return ret;
}

/* just for dgb */
#if 0
static void mt6631_bt_write(unsigned int addr, unsigned int val)
{
	unsigned int tem, i = 0;

	fm_host_reg_write(0x80103020, addr);
	fm_host_reg_write(0x80103024, val);
	fm_host_reg_read(0x80103000, &tem);
	while ((tem == 4) && (i < 1000)) {
		i++;
		fm_host_reg_read(0x80103000, &tem);
	}
}
#endif

static bool mt6631_SetFreq(unsigned short freq)
{
	signed int ret = 0;
	unsigned short pkt_size;
	unsigned short chan_para = 0;
	unsigned short freq_reg = 0;
	unsigned short tmp_reg[6] = {0};
	signed int projectid = fm_cb_op->projectid_get();

	fm_cb_op->cur_freq_set(freq);

#if 0
	/* MCU clock adjust if need */
	ret = mt6631_mcu_dese(freq, NULL);
	if (ret < 0)
		WCN_DBG(FM_ERR | MAIN, "mt6631_mcu_dese FAIL:%d\n", ret);

	WCN_DBG(FM_INF | MAIN, "MCU %d\n", ret);

	/* GPS clock adjust if need */
	ret = mt6631_gps_dese(freq, NULL);
	if (ret < 0)
		WCN_DBG(FM_ERR | MAIN, "mt6631_gps_dese FAIL:%d\n", ret);

	WCN_DBG(FM_INF | MAIN, "GPS %d\n", ret);
#endif

	/* A0. Host contrl RF register */
	ret = fm_set_bits(0x60, 0x0007, 0xFFF0);  /* Set 0x60 [D3:D0] = 0x07*/
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "%s: Host Control RF register 0x60 = 0x7 failed\n", __func__);

	/* A0.1 Update FM ADPLL fast tracking mode gain */
	ret = fm_set_bits(0x0F, 0x0455, 0xF800);
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "%s: Set FM ADPLL gainA/B=0x455 failed\n", __func__);

	/* A0.2 Set FMSYS cell mode */
	if (mt6631_TDD_chan_check(freq)) {
		ret = fm_set_bits(0x30, 0x0008, 0xFFF3);	/* use TDD solution */
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: freq[%d]: use TDD solution failed\n", __func__, freq);
	} else {
		ret = fm_set_bits(0x30, 0x0000, 0xFFF3);	/* default use FDD solution */
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "%s: freq[%d]: default use FDD solution failed\n", __func__, freq);
	}

	/* A0.3 Host control RF register */
	ret = fm_set_bits(0x60, 0x000F, 0xFFF0);	/* Set 0x60 [D3:D0] = 0x0F*/
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "%s: Set 0x60 [D3:D0] = 0x0F failed\n", __func__);

	/* A1 Get Channel parameter from map list*/

	chan_para = mt6631_chan_para_get(freq);
	WCN_DBG(FM_DBG | CHIP, "%s: %d chan para = %d\n", __func__, (signed int) freq, (signed int) chan_para);

	freq_reg = freq;
	if (fm_get_channel_space(freq_reg) == 0)
		freq_reg *= 10;

	freq_reg = (freq_reg - 6400) * 2 / 10;

	/*A1 Set rgfrf_chan = XXX*/
	ret = fm_set_bits(0x65, freq_reg, 0xFC00);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "%s: set rgfrf_chan = xxx = %d failed\n", __func__, freq_reg);
		return false;
	}

	ret = fm_set_bits(0x65, (chan_para << 12), 0x0FFF);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "set freq wr 0x65 failed\n");
		return false;
	}

	/* SPI hoppint setting*/
	if (mt6631_SPI_hopping_check(freq)) {

		WCN_DBG(FM_NTC | CHIP, "%s: freq:%d is SPI hopping channel,turn on 64M PLL\n", __func__, freq);

		if (projectid == 0x8168) {
			mt8168_spi_hopping_clock_switch(true);
		} else {
			if (!mt6631_do_SPI_hopping_64M(freq))
				WCN_DBG(FM_ERR | CHIP, "%s: spi hopping fail!\n", __func__);
			}
		}

	/* A0. Host contrl RF register */
	ret = fm_set_bits(0x60, 0x0007, 0xFFF0);  /* Set 0x60 [D3:D0] = 0x07*/
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "%s: Host Control RF register 0x60 = 0x7 failed\n", __func__);


	fm_reg_read(0x62, &tmp_reg[0]);
	fm_reg_read(0x64, &tmp_reg[1]);
	fm_reg_read(0x69, &tmp_reg[2]);
	fm_reg_read(0x6a, &tmp_reg[3]);
	fm_reg_read(0x6b, &tmp_reg[4]);
	fm_reg_read(0x9b, &tmp_reg[5]);

	WCN_DBG(FM_ALT | CHIP, "%s: Before tune--0x62 0x64 0x69 0x6a 0x6b 0x9b = %04x %04x %04x %04x %04x %04x\n",
			__func__,
			tmp_reg[0],
			tmp_reg[1],
			tmp_reg[2],
			tmp_reg[3],
			tmp_reg[4],
			tmp_reg[5]);

	/* A0.3 Host control RF register */
	ret = fm_set_bits(0x60, 0x000F, 0xFF00);	/* Set 0x60 [D3:D0] = 0x0F*/
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "%s: Set 0x60 [D3:D0] = 0x0F failed\n", __func__);

	if (FM_LOCK(cmd_buf_lock))
		return false;
	pkt_size = mt6631_tune(cmd_buf, TX_BUF_SIZE, freq, chan_para);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_TUNE | FLAG_TUNE_DONE, SW_RETRY_CNT, TUNE_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);


	/* A0. Host contrl RF register */
	ret = fm_set_bits(0x60, 0x0007, 0xFFF0);  /* Set 0x60 [D3:D0] = 0x07*/
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "%s: Host Control RF register 0x60 = 0x7 failed\n", __func__);

	memset(tmp_reg, 0, sizeof(tmp_reg[0])*6);

	fm_reg_read(0x62, &tmp_reg[0]);
	fm_reg_read(0x64, &tmp_reg[1]);
	fm_reg_read(0x69, &tmp_reg[2]);
	fm_reg_read(0x6a, &tmp_reg[3]);
	fm_reg_read(0x6b, &tmp_reg[4]);
	fm_reg_read(0x9b, &tmp_reg[5]);

	WCN_DBG(FM_ALT | CHIP, "%s: After tune--0x62 0x64 0x69 0x6a 0x6b 0x9b = %04x %04x %04x %04x %04x %04x\n",
			__func__,
			tmp_reg[0],
			tmp_reg[1],
			tmp_reg[2],
			tmp_reg[3],
			tmp_reg[4],
			tmp_reg[5]);

	/* A0.3 Host control RF register */
	ret = fm_set_bits(0x60, 0x000F, 0xFF00);	/* Set 0x60 [D3:D0] = 0x0F*/
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "%s: Set 0x60 [D3:D0] = 0x0F failed\n", __func__);

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "%s: mt6631_tune failed\n", __func__);
		return false;
	}

	WCN_DBG(FM_DBG | CHIP, "%s: set freq to %d ok\n", __func__, freq);
#if 0
	/* ADPLL setting for dbg */
	fm_top_reg_write(0x0050, 0x00000007);
	fm_top_reg_write(0x0A08, 0xFFFFFFFF);
	mt6631_bt_write(0x82, 0x11);
	mt6631_bt_write(0x83, 0x11);
	mt6631_bt_write(0x84, 0x11);
	fm_top_reg_write(0x0040, 0x1C1C1C1C);
	fm_top_reg_write(0x0044, 0x1C1C1C1C);
	fm_reg_write(0x70, 0x0010);
	/*0x0806 DCO clk
	*0x0802 ref clk
	*0x0804 feedback clk
	*/
	fm_reg_write(0xE0, 0x0806);
#endif
	return true;
}

#if 0
/*
* mt6631_Seek
* @pFreq - IN/OUT parm, IN start freq/OUT seek valid freq
* @seekdir - 0:up, 1:down
* @space - 1:50KHz, 2:100KHz, 4:200KHz
* return true:seek success; false:seek failed
*/
static bool mt6631_Seek(unsigned short min_freq, unsigned short max_freq, unsigned short *pFreq,
			unsigned short seekdir, unsigned short space)
{
	signed int ret = 0;
	unsigned short pkt_size, temp;

	mt6631_RampDown();
	fm_reg_read(FM_MAIN_CTRL, &temp);
	mt6631_Mute(true);

	if (FM_LOCK(cmd_buf_lock))
		return false;
	pkt_size = mt6631_seek(cmd_buf, TX_BUF_SIZE, seekdir, space, max_freq, min_freq);
	ret =
		fm_cmd_tx(cmd_buf, pkt_size, FLAG_SEEK | FLAG_SEEK_DONE, SW_RETRY_CNT, SEEK_TIMEOUT,
			fm_get_read_result);
	FM_UNLOCK(cmd_buf_lock);

	if (!ret && fm_res) {
		*pFreq = fm_res->seek_result;
		/* fm_cb_op->cur_freq_set(*pFreq); */
	} else {
		WCN_DBG(FM_ALT | CHIP, "mt6631_seek failed\n");
		return ret;
	}

	/* get the result freq */
	WCN_DBG(FM_NTC | CHIP, "seek, result freq:%d\n", *pFreq);
	mt6631_RampDown();
	if ((temp & 0x0020) == 0)
		mt6631_Mute(false);

	return true;
}
#endif
#define FM_CQI_LOG_PATH "/mnt/sdcard/fmcqilog"

static signed int mt6631_full_cqi_get(signed int min_freq, signed int max_freq, signed int space, signed int cnt)
{
	signed int ret = 0;
	unsigned short pkt_size;
	unsigned short freq, orig_freq;
	signed int i, j, k;
	signed int space_val, max, min, num;
	struct mt6631_full_cqi *p_cqi;
	unsigned char *cqi_log_title = "Freq, RSSI, PAMD, PR, FPAMD, MR, ATDC, PRX, ATDEV, SMGain, DltaRSSI\n";
	unsigned char cqi_log_buf[100] = { 0 };
	signed int pos;
	unsigned char cqi_log_path[100] = { 0 };

	/* for soft-mute tune, and get cqi */
	freq = fm_cb_op->cur_freq_get();
	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	/* get cqi */
	orig_freq = freq;
	if (fm_get_channel_space(min_freq) == 0)
		min = min_freq * 10;
	else
		min = min_freq;

	if (fm_get_channel_space(max_freq) == 0)
		max = max_freq * 10;
	else
		max = max_freq;

	if (space == 0x0001)
		space_val = 5;	/* 50Khz */
	else if (space == 0x0002)
		space_val = 10;	/* 100Khz */
	else if (space == 0x0004)
		space_val = 20;	/* 200Khz */
	else
		space_val = 10;

	num = (max - min) / space_val + 1;	/* Eg, (8760 - 8750) / 10 + 1 = 2 */
	for (k = 0; (orig_freq == 10000) && (g_dbg_level == 0xffffffff) && (k < cnt); k++) {
		WCN_DBG(FM_NTC | CHIP, "cqi file:%d\n", k + 1);
		freq = min;
		pos = 0;
		fm_memcpy(cqi_log_path, FM_CQI_LOG_PATH, strlen(FM_CQI_LOG_PATH));
		if (sprintf(&cqi_log_path[strlen(FM_CQI_LOG_PATH)], "%d.txt", k + 1) < 0)
			WCN_DBG(FM_NTC | CHIP, "sprintf fail\n");
		fm_file_write(cqi_log_path, cqi_log_title, strlen(cqi_log_title), &pos);
		for (j = 0; j < num; j++) {
			if (FM_LOCK(cmd_buf_lock))
				return -FM_ELOCK;
			pkt_size = fm_full_cqi_req(cmd_buf, TX_BUF_SIZE, &freq, 1, 1);
			ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_SM_TUNE, SW_RETRY_CNT,
					SM_TUNE_TIMEOUT, fm_get_read_result);
			FM_UNLOCK(cmd_buf_lock);

			if (!ret && fm_res) {
				WCN_DBG(FM_NTC | CHIP, "smt cqi size %d\n", fm_res->cqi[0]);
				p_cqi = (struct mt6631_full_cqi *)&fm_res->cqi[2];
				for (i = 0; i < fm_res->cqi[1]; i++) {
					/* just for debug */
					WCN_DBG(FM_NTC | CHIP,
						"freq %d, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x\n",
						p_cqi[i].ch, p_cqi[i].rssi, p_cqi[i].pamd,
						p_cqi[i].pr, p_cqi[i].fpamd, p_cqi[i].mr,
						p_cqi[i].atdc, p_cqi[i].prx, p_cqi[i].atdev,
						p_cqi[i].smg, p_cqi[i].drssi);
					/* format to buffer */
					if (sprintf(cqi_log_buf,
							"%04d, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x,\n",
							p_cqi[i].ch, p_cqi[i].rssi, p_cqi[i].pamd,
							p_cqi[i].pr, p_cqi[i].fpamd, p_cqi[i].mr,
							p_cqi[i].atdc, p_cqi[i].prx, p_cqi[i].atdev,
							p_cqi[i].smg, p_cqi[i].drssi) < 0)
						WCN_DBG(FM_NTC | CHIP, "sprintf fail\n");
					/* write back to log file */
					fm_file_write(cqi_log_path, cqi_log_buf, strlen(cqi_log_buf), &pos);
				}
			} else {
				WCN_DBG(FM_ALT | CHIP, "smt get CQI failed\n");
				ret = -1;
			}
			freq += space_val;
		}
		fm_cb_op->cur_freq_set(0);	/* avoid run too much times */
	}

	return ret;
}

static unsigned short mt6631_read_dsp_reg(unsigned short addr)
{
	unsigned short regValue = 0;

	fm_reg_write(0x60, 0x07);
	fm_reg_write(0xE2, addr);
	fm_reg_write(0xE1, 0x01);
	fm_reg_read(0xE4, &regValue);
	fm_reg_write(0x60, 0x0F);

	return regValue;
}

static bool mt6631_is_valid_freq(unsigned short freq)
{
	int i = 0;
	bool valid = false;
	signed int RSSI = 0, PAMD = 0, MR = 0;
	unsigned int PRX = 0;
	unsigned short softmuteGainLvl = 0;
	unsigned short tmp_reg = 0;

	for (i = 0; i < 8; i++) {
		fm_reg_read(0x6C, &tmp_reg);
		RSSI += (((tmp_reg & 0x03FF) >= 512) ? ((tmp_reg & 0x03FF) - 1024) : (tmp_reg & 0x03FF)) >> 3;
		fm_reg_read(0xB4, &tmp_reg);
		PAMD += (((tmp_reg & 0x1FF) >= 256) ? ((tmp_reg & 0x01FF) - 512) : (tmp_reg & 0x01FF)) >> 3;
		tmp_reg = mt6631_read_dsp_reg(0x4E1);
		PRX += (tmp_reg & 0x00FF) >> 3;
		fm_reg_read(0xBD, &tmp_reg);
		MR += (((tmp_reg & 0x01FF) >= 256) ? ((tmp_reg & 0x01FF) - 512) : (tmp_reg & 0x01FF)) >> 3;
		tmp_reg = mt6631_read_dsp_reg(0x2C4);
		softmuteGainLvl += tmp_reg >> 3;
		fm_delayus(2250);
	}

	if ((fm_config.rx_cfg.long_ana_rssi_th <= RSSI)
		&& (fm_config.rx_cfg.pamd_th >= PAMD)
		&& (fm_config.rx_cfg.mr_th <= MR)
		&& (fm_config.rx_cfg.prx_th <= PRX)
		&& (fm_config.rx_cfg.smg_th <= softmuteGainLvl)) {
		valid = true;
	}

	WCN_DBG(FM_DBG | CHIP,
			"freq %d valid=%d, %d, %d, 0x%04x, 0x%04x, 0x%04x\n",
			freq, valid, RSSI, PAMD, PRX, MR, softmuteGainLvl);

	return valid;
}

/*
 * mt6631_GetCurRSSI - get current freq's RSSI value
 * RS=RSSI
 * If RS>511, then RSSI(dBm)= (RS-1024)/16*6
 *				else RSSI(dBm)= RS/16*6
 */
static signed int mt6631_GetCurRSSI(signed int *pRSSI)
{
	unsigned short tmp_reg = 0;

	fm_reg_read(FM_RSSI_IND, &tmp_reg);
	tmp_reg = tmp_reg & 0x03ff;

	if (pRSSI) {
		*pRSSI = (tmp_reg > 511) ? (((tmp_reg - 1024) * 6) >> 4) : ((tmp_reg * 6) >> 4);
		WCN_DBG(FM_DBG | CHIP, "rssi:%d, dBm:%d\n", tmp_reg, *pRSSI);
	} else {
		WCN_DBG(FM_ERR | CHIP, "get rssi para error\n");
		return -FM_EPARA;
	}

	return 0;
}

static unsigned short mt6631_vol_tbl[16] = { 0x0000, 0x0519, 0x066A, 0x0814,
	0x0A2B, 0x0CCD, 0x101D, 0x1449,
	0x198A, 0x2027, 0x287A, 0x32F5,
	0x4027, 0x50C3, 0x65AD, 0x7FFF
};

static signed int mt6631_SetVol(unsigned char vol)
{
	signed int ret = 0;

	vol = (vol > 15) ? 15 : vol;
	ret = fm_reg_write(0x7D, mt6631_vol_tbl[vol]);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "Set vol=%d Failed\n", vol);
		return ret;
	}
	WCN_DBG(FM_DBG | CHIP, "Set vol=%d OK\n", vol);


	if (vol == 10) {
		fm_print_cmd_fifo();	/* just for debug */
		fm_print_evt_fifo();
	}
	return 0;
}

static signed int mt6631_GetVol(unsigned char *pVol)
{
	int ret = 0;
	unsigned short tmp = 0;
	signed int i;

	if (pVol == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	ret = fm_reg_read(0x7D, &tmp);
	if (ret) {
		*pVol = 0;
		WCN_DBG(FM_ERR | CHIP, "Get vol Failed\n");
		return ret;
	}

	for (i = 0; i < 16; i++) {
		if (mt6631_vol_tbl[i] == tmp) {
			*pVol = i;
			break;
		}
	}

	WCN_DBG(FM_DBG | CHIP, "Get vol=%d OK\n", *pVol);
	return 0;
}

static signed int mt6631_dump_reg(void)
{
	signed int i;
	unsigned short TmpReg = 0;

	for (i = 0; i < 0xff; i++) {
		fm_reg_read(i, &TmpReg);
		WCN_DBG(FM_NTC | CHIP, "0x%02x=0x%04x\n", i, TmpReg);
	}
	return 0;
}

/*0:mono, 1:stereo*/
static bool mt6631_GetMonoStereo(unsigned short *pMonoStereo)
{
#define FM_BF_STEREO 0x1000
	unsigned short TmpReg = 0;

	if (pMonoStereo) {
		fm_reg_read(FM_RSSI_IND, &TmpReg);
		*pMonoStereo = (TmpReg & FM_BF_STEREO) >> 12;
	} else {
		WCN_DBG(FM_ERR | CHIP, "MonoStero: para err\n");
		return false;
	}

	WCN_DBG(FM_NTC | CHIP, "Get MonoStero:0x%04x\n", *pMonoStereo);
	return true;
}

static signed int mt6631_SetMonoStereo(signed int MonoStereo)
{
	signed int ret = 0;

	WCN_DBG(FM_NTC | CHIP, "set to %s\n", MonoStereo ? "mono" : "auto");
	fm_reg_write(0x60, 0x0007);

	if (MonoStereo)	/*mono */
		ret = fm_set_bits(0x75, 0x0008, ~0x0008);
	else
		ret = fm_set_bits(0x75, 0x0000, ~0x0008);

	fm_reg_write(0x60, 0x000F);
	return ret;
}

static signed int mt6631_GetCapArray(signed int *ca)
{
	unsigned short dataRead = 0;
	unsigned short tmp = 0;

	if (ca == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	fm_reg_read(0x60, &tmp);
	fm_reg_write(0x60, tmp & 0xFFF7);	/* 0x60 D3=0 */

	fm_reg_read(0x26, &dataRead);
	*ca = dataRead;

	fm_reg_write(0x60, tmp);	/* 0x60 D3=1 */
	return 0;
}

/*
 * mt6631_GetCurPamd - get current freq's PAMD value
 * PA=PAMD
 * If PA>511 then PAMD(dB)=  (PA-1024)/16*6,
 *				else PAMD(dB)=PA/16*6
 */
static bool mt6631_GetCurPamd(unsigned short *pPamdLevl)
{
	unsigned short tmp_reg = 0;
	unsigned short dBvalue, valid_cnt = 0;
	int i, total = 0;

	for (i = 0; i < 8; i++) {
		if (fm_reg_read(FM_ADDR_PAMD, &tmp_reg)) {
			*pPamdLevl = 0;
			return false;
		}

		tmp_reg &= 0x03FF;
		dBvalue = (tmp_reg > 256) ? ((512 - tmp_reg) * 6 / 16) : 0;
		if (dBvalue != 0) {
			total += dBvalue;
			valid_cnt++;
			WCN_DBG(FM_DBG | CHIP, "[%d]PAMD=%d\n", i, dBvalue);
		}
		fm_delayms(3);
	}
	if (valid_cnt != 0)
		*pPamdLevl = total / valid_cnt;
	else
		*pPamdLevl = 0;

	WCN_DBG(FM_NTC | CHIP, "PAMD=%d\n", *pPamdLevl);
	return true;
}

static signed int mt6631_i2s_info_get(signed int *ponoff, signed int *pmode, signed int *psample)
{
	if (ponoff == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (pmode == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (psample == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	*ponoff = fm_config.aud_cfg.i2s_info.status;
	*pmode = fm_config.aud_cfg.i2s_info.mode;
	*psample = fm_config.aud_cfg.i2s_info.rate;

	return 0;
}

static signed int mt6631_get_audio_info(struct fm_audio_info_t *data)
{
	memcpy(data, &fm_config.aud_cfg, sizeof(struct fm_audio_info_t));
	return 0;
}

static signed int mt6631_hw_info_get(struct fm_hw_info *req)
{
	if (req == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	req->chip_id = mt6631_hw_info.chip_id;
	req->eco_ver = mt6631_hw_info.eco_ver;
	req->patch_ver = mt6631_hw_info.patch_ver;
	req->rom_ver = mt6631_hw_info.rom_ver;

	return 0;
}

static signed int mt6631_pre_search(void)
{
	mt6631_RampDown();
	/* disable audio output I2S Rx mode */
	fm_host_reg_write(0x80101054, 0x00000000);
	/* disable audio output I2S Tx mode */
	fm_reg_write(0x9B, 0x0000);

	return 0;
}

static signed int mt6631_restore_search(void)
{
	mt6631_RampDown();
	/* set audio output I2S Tx mode */
	fm_reg_write(0x9B, 0xF9AB);
	/* set audio output I2S Rx mode */
	fm_host_reg_write(0x80101054, 0x00003f35);
	return 0;
}

static signed int mt6631_soft_mute_tune(unsigned short freq, signed int *rssi, signed int *valid)
{
	signed int ret = 0;
	unsigned short pkt_size;
	struct mt6631_full_cqi *p_cqi;
	signed int RSSI = 0, PAMD = 0, MR = 0, ATDC = 0;
	unsigned int PRX = 0, ATDEV = 0;
	unsigned short softmuteGainLvl = 0;
	signed int projectid = fm_cb_op->projectid_get();

	ret = mt6631_chan_para_get(freq);
	if (ret == 2)
		ret = fm_set_bits(FM_CHANNEL_SET, 0x2000, 0x0FFF);	/* mdf HiLo */
	else
		ret = fm_set_bits(FM_CHANNEL_SET, 0x0000, 0x0FFF);	/* clear FA/HL/ATJ */

	/* SPI hoppint setting*/
	if (mt6631_SPI_hopping_check(freq)) {
		WCN_DBG(FM_NTC | CHIP, "%s: freq:%d is SPI hopping channel,turn on 64M PLL\n", __func__, freq);

		if (projectid == 0x8168) {
			mt8168_spi_hopping_clock_switch(true);
		} else {
			if (!mt6631_do_SPI_hopping_64M(freq))
				WCN_DBG(FM_ERR | CHIP, "%s: spi hopping fail!\n", __func__);
			}
		}

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = fm_full_cqi_req(cmd_buf, TX_BUF_SIZE, &freq, 1, 1);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_SM_TUNE, SW_RETRY_CNT, SM_TUNE_TIMEOUT, fm_get_read_result);
	FM_UNLOCK(cmd_buf_lock);

	if (!ret && fm_res) {
		p_cqi = (struct mt6631_full_cqi *)&fm_res->cqi[2];
		/* just for debug */
		WCN_DBG(FM_NTC | CHIP,
			"freq %d, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x\n",
			p_cqi->ch, p_cqi->rssi, p_cqi->pamd, p_cqi->pr, p_cqi->fpamd, p_cqi->mr,
			p_cqi->atdc, p_cqi->prx, p_cqi->atdev, p_cqi->smg, p_cqi->drssi);
		RSSI = ((p_cqi->rssi & 0x03FF) >= 512) ? ((p_cqi->rssi & 0x03FF) - 1024) : (p_cqi->rssi & 0x03FF);
		PAMD = ((p_cqi->pamd & 0x1FF) >= 256) ? ((p_cqi->pamd & 0x01FF) - 512) : (p_cqi->pamd & 0x01FF);
		MR = ((p_cqi->mr & 0x01FF) >= 256) ? ((p_cqi->mr & 0x01FF) - 512) : (p_cqi->mr & 0x01FF);
		ATDC = (p_cqi->atdc >= 32768) ? (65536 - p_cqi->atdc) : (p_cqi->atdc);
		if (ATDC < 0)
			ATDC = (~(ATDC)) - 1;	/* Get abs value of ATDC */

		PRX = (p_cqi->prx & 0x00FF);
		ATDEV = p_cqi->atdev;
		softmuteGainLvl = p_cqi->smg;
		/* check if the channel is valid according to each CQIs */
		if ((fm_config.rx_cfg.long_ana_rssi_th <= RSSI)
		    && (fm_config.rx_cfg.pamd_th >= PAMD)
		    && (fm_config.rx_cfg.atdc_th >= ATDC)
		    && (fm_config.rx_cfg.mr_th <= MR)
		    && (fm_config.rx_cfg.prx_th <= PRX)
		    && (ATDEV >= ATDC)	/* sync scan algorithm */
		    && (fm_config.rx_cfg.smg_th <= softmuteGainLvl)) {
			*valid = true;
		} else {
			*valid = false;
		}
		*rssi = RSSI;
	} else {
		WCN_DBG(FM_ALT | CHIP, "smt get CQI failed\n");
		return false;
	}
	WCN_DBG(FM_NTC | CHIP, "valid=%d\n", *valid);
	return true;
}

static bool mt6631_em_test(unsigned short group_idx, unsigned short item_idx, unsigned int item_value)
{
	return true;
}

/*
*parm:
*	parm.th_type: 0, RSSI. 1, desense RSSI. 2, SMG.
*	parm.th_val: threshold value
*/
static signed int mt6631_set_search_th(signed int idx, signed int val, signed int reserve)
{
	switch (idx) {
	case 0:	{
		fm_config.rx_cfg.long_ana_rssi_th = val;
		WCN_DBG(FM_NTC | CHIP, "set rssi th =%d\n", val);
		break;
	}
	case 1:	{
		fm_config.rx_cfg.desene_rssi_th = val;
		WCN_DBG(FM_NTC | CHIP, "set desense rssi th =%d\n", val);
		break;
	}
	case 2:	{
		fm_config.rx_cfg.smg_th = val;
		WCN_DBG(FM_NTC | CHIP, "set smg th =%d\n", val);
		break;
	}
	default:
		break;
	}
	return 0;
}

static signed int MT6631_low_power_wa_default(signed int fmon)
{
	return 0;
}

signed int mt6631_fm_low_ops_register(struct fm_callback *cb, struct fm_basic_interface *bi)
{
	signed int ret = 0;
	/* Basic functions. */

	if (bi == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,bi invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (cb->cur_freq_get == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,cb->cur_freq_get invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (cb->cur_freq_set == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,cb->cur_freq_set invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	fm_cb_op = cb;

	bi->pwron = mt6631_pwron;
	bi->pwroff = mt6631_pwroff;
	bi->chipid_get = mt6631_get_chipid;
	bi->mute = mt6631_Mute;
	bi->rampdown = mt6631_RampDown;
	bi->pwrupseq = mt6631_PowerUp;
	bi->pwrdownseq = mt6631_PowerDown;
	bi->setfreq = mt6631_SetFreq;
	bi->low_pwr_wa = MT6631_low_power_wa_default;
	bi->get_aud_info = mt6631_get_audio_info;
	bi->rssiget = mt6631_GetCurRSSI;
	bi->volset = mt6631_SetVol;
	bi->volget = mt6631_GetVol;
	bi->dumpreg = mt6631_dump_reg;
	bi->msget = mt6631_GetMonoStereo;
	bi->msset = mt6631_SetMonoStereo;
	bi->pamdget = mt6631_GetCurPamd;
	bi->em = mt6631_em_test;
	bi->anaswitch = mt6631_SetAntennaType;
	bi->anaget = mt6631_GetAntennaType;
	bi->caparray_get = mt6631_GetCapArray;
	bi->hwinfo_get = mt6631_hw_info_get;
	bi->i2s_get = mt6631_i2s_info_get;
	bi->is_dese_chan = mt6631_is_dese_chan;
	bi->softmute_tune = mt6631_soft_mute_tune;
	bi->desense_check = mt6631_desense_check;
	bi->cqi_log = mt6631_full_cqi_get;
	bi->pre_search = mt6631_pre_search;
	bi->restore_search = mt6631_restore_search;
	bi->set_search_th = mt6631_set_search_th;
	bi->is_valid_freq = mt6631_is_valid_freq;

	cmd_buf_lock = fm_lock_create("31_cmd");
	ret = fm_lock_get(cmd_buf_lock);

	cmd_buf = fm_zalloc(TX_BUF_SIZE + 1);

	if (!cmd_buf) {
		WCN_DBG(FM_ALT | CHIP, "6631 fm lib alloc tx buf failed\n");
		ret = -1;
	}
#if 0				/* def CONFIG_MTK_FM_50KHZ_SUPPORT */
	cqi_fifo = fm_fifo_create("6628_cqi_fifo", sizeof(struct adapt_fm_cqi), 640);
	if (!cqi_fifo) {
		WCN_DBG(FM_ALT | CHIP, "6631 fm lib create cqi fifo failed\n");
		ret = -1;
	}
#endif

	return ret;
}

signed int mt6631_fm_low_ops_unregister(struct fm_basic_interface *bi)
{
	signed int ret = 0;
	/* Basic functions. */
	if (bi == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,bi invalid pointer\n", __func__);
		return -FM_EPARA;
	}

#if 0				/* def CONFIG_MTK_FM_50KHZ_SUPPORT */
	fm_fifo_release(cqi_fifo);
#endif

	if (cmd_buf) {
		fm_free(cmd_buf);
		cmd_buf = NULL;
	}

	ret = fm_lock_put(cmd_buf_lock);
	fm_memset(bi, 0, sizeof(struct fm_basic_interface));

	fm_cb_op = NULL;

	return ret;
}

static const signed char mt6631_chan_para_map[] = {
 /* 0, X, 1, X, 2, X, 3, X, 4, X, 5, X, 6, X, 7, X, 8, X, 9, X*/
	0, 0, 2, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6500~6595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6600~6695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,	/* 6700~6795 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6800~6895 */
	0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6900~6995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7000~7095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7100~7195 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0,	/* 7200~7295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0,	/* 7300~7395 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,	/* 7400~7495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7500~7595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,	/* 7600~7695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7700~7795 */
	8, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7800~7895 */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7900~7995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8000~8095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8100~8195 */
	0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8200~8295 */
	0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,	/* 8300~8395 */
	0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8400~8495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8500~8595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8600~8695 */
	0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8700~8795 */
	0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8800~8895 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8900~8995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9000~9095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9100~9195 */
	0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9200~9295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9300~9395 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9400~9495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9500~9595 */
	3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9600~9695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9700~9795 */
	0, 0, 0, 0, 0, 0, 8, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9800~9895 */
	0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,	/* 9900~9995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10000~10095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10100~10195 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,	/* 10200~10295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,	/* 10300~10395 */
	8, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10400~10495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0,	/* 10500~10595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10600~10695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0,	/* 10700~10795 */
	0			/* 10800 */
};
static const unsigned short mt6631_scan_dese_list[] = {
	6910, 6920, 7680, 7800, 8450, 9210, 9220, 9230, 9590, 9600, 9830, 9900, 9980, 9990, 10400, 10750, 10760
};

static const unsigned short mt6631_SPI_hopping_list[] = {
	6510, 6520, 6530, 7780, 7790, 7800, 7810, 7820, 9090, 9100, 9110, 9120, 10380, 10390, 10400, 10410, 10420
};

static const unsigned short mt6631_I2S_hopping_list[] = {
	6550, 6760, 6960, 6970, 7170, 7370, 7580, 7780, 7990, 8810, 9210, 9220, 10240
};

static const unsigned short mt6631_TDD_list[] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6500~6595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6600~6695 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6700~6795 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6800~6895 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6900~6995 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7000~7095 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7100~7195 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7200~7295 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7300~7395 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7400~7495 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7500~7595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7600~7695 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7700~7795 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7800~7895 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7900~7995 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8000~8095 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8100~8195 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8200~8295 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8300~8395 */
	0x0101, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8400~8495 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8500~8595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8600~8695 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8700~8795 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8800~8895 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8900~8995 */
	0x0000, 0x0000, 0x0101, 0x0101, 0x0101,	/* 9000~9095 */
	0x0001, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9100~9195 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9200~9295 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9300~9395 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9400~9495 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9500~9595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9600~9695 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0100,	/* 9700~9795 */
	0x0101, 0x0101, 0x0101, 0x0101, 0x0101,	/* 9800~9895 */
	0x0101, 0x0101, 0x0001, 0x0000, 0x0000,	/* 9900~9995 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10000~10095 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10100~10195 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10200~10295 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10300~10395 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10400~10495 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10500~10595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0100,	/* 10600~10695 */
	0x0101, 0x0101, 0x0101, 0x0101, 0x0101,	/* 10700~10795 */
	0x0001			/* 10800 */
};

static const unsigned short mt6631_TDD_Mask[] = {
	0x0001, 0x0010, 0x0100, 0x1000
};

/* return value: 0, not a de-sense channel; 1, this is a de-sense channel; else error no */
static signed int mt6631_is_dese_chan(unsigned short freq)
{
	signed int size;

	if (1 == HQA_ZERO_DESENSE_MAP) /*HQA only :skip desense channel check. */
		return 0;

	size = ARRAY_SIZE(mt6631_scan_dese_list);

	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	while (size) {
		if (mt6631_scan_dese_list[size - 1] == freq)
			return 1;

		size--;
	}

	return 0;
}

/*  return value:
*1, is desense channel and rssi is less than threshold;
*0, not desense channel or it is but rssi is more than threshold.
*/
static signed int mt6631_desense_check(unsigned short freq, signed int rssi)
{
	if (mt6631_is_dese_chan(freq)) {
		if (rssi < fm_config.rx_cfg.desene_rssi_th)
			return 1;

		WCN_DBG(FM_DBG | CHIP, "desen_rssi %d th:%d\n", rssi, fm_config.rx_cfg.desene_rssi_th);
	}
	return 0;
}

static bool mt6631_TDD_chan_check(unsigned short freq)
{
	unsigned int i = 0;
	unsigned short freq_tmp = freq;
	signed int ret = 0;

	ret = fm_get_channel_space(freq_tmp);
	if (ret == 0)
		freq_tmp *= 10;
	else if (ret == -1)
		return false;

	i = (freq_tmp - 6500) / 5;
	if ((i / 4) >= ARRAY_SIZE(mt6631_TDD_list)) {
		WCN_DBG(FM_ERR | CHIP, "Freq index out of range(%d),max(%zd)\n",
			i / 4, ARRAY_SIZE(mt6631_TDD_list));
		return false;
	}

	if (mt6631_TDD_list[i / 4] & mt6631_TDD_Mask[i % 4]) {
		WCN_DBG(FM_DBG | CHIP, "Freq %d use TDD solution\n", freq);
		return true;
	} else
		return false;
}

/* get channel parameter, HL side/ FA / ATJ */
static unsigned short mt6631_chan_para_get(unsigned short freq)
{
	signed int pos, size;

	if (1 == HQA_RETURN_ZERO_MAP) {
		WCN_DBG(FM_NTC | CHIP, "HQA_RETURN_ZERO_CHAN mt6631_chan_para_map enabled!\n");
		return 0;
	}

	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	if (freq < 6500)
		return 0;

	pos = (freq - 6500) / 5;

	size = ARRAY_SIZE(mt6631_chan_para_map);

	pos = (pos > (size - 1)) ? (size - 1) : pos;

	return mt6631_chan_para_map[pos];
}


static bool mt6631_SPI_hopping_check(unsigned short freq)
{
	signed int size;

	size = ARRAY_SIZE(mt6631_SPI_hopping_list);

	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	while (size) {
		if (mt6631_SPI_hopping_list[size - 1] == freq)
			return 1;
		size--;
	}

	return 0;
}

