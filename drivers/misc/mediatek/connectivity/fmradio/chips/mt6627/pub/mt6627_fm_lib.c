/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "osal_typedef.h"
#include "stp_exp.h"
#include "wmt_exp.h"

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

#include "mt6627_fm_reg.h"
#include "mt6627_fm_lib.h"

/* #include "mach/mt_gpio.h" */

/* #define MT6627_FM_PATCH_PATH "/etc/firmware/mt6627/mt6627_fm_patch.bin" */
/* #define MT6627_FM_COEFF_PATH "/etc/firmware/mt6627/mt6627_fm_coeff.bin" */
/* #define MT6627_FM_HWCOEFF_PATH "/etc/firmware/mt6627/mt6627_fm_hwcoeff.bin" */
/* #define MT6627_FM_ROM_PATH "/etc/firmware/mt6627/mt6627_fm_rom.bin" */

static struct fm_patch_tbl mt6627_patch_tbl[5] = {
	{FM_ROM_V1, "mt6627_fm_v1_patch.bin", "mt6627_fm_v1_coeff.bin", NULL, NULL},
	{FM_ROM_V2, "mt6627_fm_v2_patch.bin", "mt6627_fm_v2_coeff.bin", NULL, NULL},
	{FM_ROM_V3, "mt6627_fm_v3_patch.bin", "mt6627_fm_v3_coeff.bin", NULL, NULL},
	{FM_ROM_V4, "mt6627_fm_v4_patch.bin", "mt6627_fm_v4_coeff.bin", NULL, NULL},
	{FM_ROM_V5, "mt6627_fm_v5_patch.bin", "mt6627_fm_v5_coeff.bin", NULL, NULL}
};

static struct fm_hw_info mt6627_hw_info = {
	.chip_id = 0x00006627,
	.eco_ver = 0x00000000,
	.rom_ver = 0x00000000,
	.patch_ver = 0x00000000,
	.reserve = 0x00000000,
};

static struct fm_callback *fm_cb_op;
/* static signed int Chip_Version = mt6627_E1; */

/* static bool rssi_th_set = false; */

#if 0				/* def CONFIG_MTK_FM_50KHZ_SUPPORT */
static struct fm_fifo *cqi_fifo;
#endif
static signed int mt6627_is_dese_chan(unsigned short freq);
static bool mt6627_I2S_hopping_check(unsigned short freq);

#if 0
static signed int mt6627_mcu_dese(unsigned short freq, void *arg);
static signed int mt6627_gps_dese(unsigned short freq, void *arg);
static signed int mt6627_I2s_Setting(signed int onoff, signed int mode, signed int sample);
#endif
static unsigned short mt6627_chan_para_get(unsigned short freq);
static signed int mt6627_desense_check(unsigned short freq, signed int rssi);
static bool mt6627_TDD_chan_check(unsigned short freq);
static signed int mt6627_soft_mute_tune(unsigned short freq, signed int *rssi, signed int *valid);
static signed int mt6627_pwron(signed int data)
{
	if (mtk_wcn_wmt_func_on(WMTDRV_TYPE_FM) == MTK_WCN_BOOL_FALSE) {
		WCN_DBG(FM_ERR | CHIP, "WMT turn on FM Fail!\n");
		return -FM_ELINK;
	}

	WCN_DBG(FM_NTC | CHIP, "WMT turn on FM OK!\n");
	return 0;
}

static signed int mt6627_pwroff(signed int data)
{
	if (mtk_wcn_wmt_func_off(WMTDRV_TYPE_FM) == MTK_WCN_BOOL_FALSE) {
		WCN_DBG(FM_ERR | CHIP, "WMT turn off FM Fail!\n");
		return -FM_ELINK;
	}

	WCN_DBG(FM_NTC | CHIP, "WMT turn off FM OK!\n");
	return 0;
}

#if 0
static signed int mt6627_top_set_bits(unsigned short addr, unsigned int bits, unsigned int mask)
{
	signed int ret = 0;
	unsigned int val;

	ret = fm_top_reg_read(addr, &val);

	if (ret)
		return ret;

	val = ((val & (mask)) | bits);
	ret = fm_top_reg_write(addr, val);

	return ret;
}
#endif

#if 0
static signed int mt6627_DSP_write(unsigned short addr, unsigned short val)
{
	fm_reg_write(0xE2, addr);
	fm_reg_write(0xE3, val);
	fm_reg_write(0xE1, 0x0002);
	return 0;
}
static signed int mt6627_DSP_read(unsigned short addr, unsigned short *val)
{
	signed int ret = -1;

	fm_reg_write(0xE2, addr);
	fm_reg_write(0xE1, 0x0001);
	ret = fm_reg_read(0xE4, val);
	return ret;
}
#endif

static unsigned short mt6627_get_chipid(void)
{
	return 0x6627;
}

/*  MT6627_SetAntennaType - set Antenna type
 *  @type - 1,Short Antenna;  0, Long Antenna
 */
static signed int mt6627_SetAntennaType(signed int type)
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

static signed int mt6627_GetAntennaType(void)
{
	unsigned short dataRead = 0;

	fm_reg_read(FM_MAIN_CG2_CTRL, &dataRead);
	WCN_DBG(FM_DBG | CHIP, "get ana type: %s\n", (dataRead & ANTENNA_TYPE) ? "short" : "long");

	if (dataRead & ANTENNA_TYPE)
		return FM_ANA_SHORT;	/* short antenna */
	else
		return FM_ANA_LONG;	/* long antenna */
}

static signed int mt6627_Mute(bool mute)
{
	signed int ret = 0;
	unsigned short dataRead = 0;

	WCN_DBG(FM_DBG | CHIP, "set %s\n", mute ? "mute" : "unmute");
	/* fm_reg_read(FM_MAIN_CTRL, &dataRead); */
	fm_reg_read(0x9C, &dataRead);

	/* fm_top_reg_write(0x0050,0x00000007); */
	if (mute == 1)
		ret = fm_reg_write(0x9C, (dataRead & 0xFFFC) | 0x0003);
	else
		ret = fm_reg_write(0x9C, (dataRead & 0xFFFC));

	/* fm_top_reg_write(0x0050,0x0000000F); */

	return ret;
}

#if 0
static signed int mt6627_set_RSSITh(unsigned short TH_long, unsigned short TH_short)
{
	fm_reg_write(0xE2, 0x3072);
	fm_reg_write(0xE3, TH_long);
	fm_reg_write(0xE1, 0x0002);

	fm_delayms(1);
	fm_reg_write(0xE2, 0x307A);
	fm_reg_write(0xE3, TH_short);
	fm_reg_write(0xE1, 0x0002);

	WCN_DBG(FM_DBG | CHIP, "RSSI TH, long:0x%04x, short:0x%04x", TH_long, TH_short);
	return 0;
}

static signed int mt6627_set_SMGTh(signed int ver, unsigned short TH_smg)
{
	if (mt6627_E1 == ver) {
		fm_reg_write(0xE2, 0x321E);
		fm_reg_write(0xE3, TH_smg);
		fm_reg_write(0xE1, 0x0002);
	} else {
		fm_reg_write(0xE2, 0x3218);
		fm_reg_write(0xE3, TH_smg);
		fm_reg_write(0xE1, 0x0002);
	}

	WCN_DBG(FM_DBG | CHIP, "Soft-mute gain TH %d\n", (int)TH_smg);
	return 0;
}
#endif

static signed int mt6627_pwrup_clock_on_reg_op(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 4;
	unsigned short de_emphasis;
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
	pkt_size += fm_bop_top_write(0xA10, 0xFFFFFFFF, &buf[pkt_size], buf_size - pkt_size);	/* wr a10 ffffffff */
	/* 3,enable MTCMOS */
	pkt_size += fm_bop_top_write(0x60, 0x00000030, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 30 */
	pkt_size += fm_bop_top_write(0x60, 0x00000035, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 35 */
	pkt_size += fm_bop_top_rd_until(0x60, 0x0000000A, 0xA, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_top_write(0x60, 0x00000015, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 15 */
	pkt_size += fm_bop_top_write(0x60, 0x00000005, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 5 */
	pkt_size += fm_bop_udelay(10, &buf[pkt_size], buf_size - pkt_size);	/* delay 10us */
	pkt_size += fm_bop_top_write(0x60, 0x00000045, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 45 */
	/* 4,set CSPI fm slave dummy count */
	pkt_size += fm_bop_top_write(0x68, 0x0000003F, &buf[pkt_size], buf_size - pkt_size);	/* wr 68 3F */

	/* a1 enable digital OSC */
	pkt_size += fm_bop_top_write(0x50, 0x00000001, &buf[pkt_size], buf_size - pkt_size);	/* wr 50 1 */
	pkt_size += fm_bop_udelay(3000, &buf[pkt_size], buf_size - pkt_size);	/* delay 3ms */
	/* a3 set OSC clock output to fm */
	pkt_size += fm_bop_top_write(0x50, 0x00000003, &buf[pkt_size], buf_size - pkt_size);	/* wr 50 3 */
	/* a4 release HW clock gating */
	pkt_size += fm_bop_top_write(0x50, 0x00000007, &buf[pkt_size], buf_size - pkt_size);	/* wr 50 7 */
	/* set I2S current driving */
	pkt_size += fm_bop_top_write(0x000, 0x00000000, &buf[pkt_size], buf_size - pkt_size);	/* wr  0 0 */
	/* a5 enable DSP auto clock gating */
	pkt_size += fm_bop_write(0x70, 0x0040, &buf[pkt_size], buf_size - pkt_size);	/* wr 70 0040 */
	/* a7 deemphasis setting */
	pkt_size += fm_bop_modify(0x61, ~DE_EMPHASIS, (de_emphasis << 12), &buf[pkt_size], buf_size - pkt_size);

	/* pkt_size += fm_bop_modify(0x60, OSC_FREQ_MASK, (osc_freq << 4), &buf[pkt_size], buf_size - pkt_size); */

	return pkt_size - 4;
}
/*
 * mt6627_pwrup_clock_on - Wholechip FM Power Up: step 1, FM Digital Clock enable
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6627_pwrup_clock_on(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6627_pwrup_clock_on_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_ENABLE_OPCODE, pkt_size);
}

static signed int mt6627_pwrup_digital_init_reg_op(unsigned char *buf, signed int buf_size)
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

	/* FM RF&ADPLL divider setting */
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

	/* FM Digital Init: fm_rgf_maincon */
	/* E4 */
	pkt_size += fm_bop_write(0x6A, 0x0021, &buf[pkt_size], buf_size - pkt_size);	/* wr 6A 0021 */
	pkt_size += fm_bop_write(0x6B, 0x0021, &buf[pkt_size], buf_size - pkt_size);	/* wr 6B 0021 */
	/* E5 */
	pkt_size += fm_bop_top_write(0x50, 0x0000000F, &buf[pkt_size], buf_size - pkt_size);	/* wr 50 f */
	/* E6 */
	pkt_size += fm_bop_modify(0x61, 0xFFFD, 0x0002, &buf[pkt_size], buf_size - pkt_size);	/* wr 61 D1=1 */
	/* E7 */
	pkt_size += fm_bop_modify(0x61, 0xFFFE, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 61 D0=0 */
	/* E8 */
	pkt_size += fm_bop_udelay(100000, &buf[pkt_size], buf_size - pkt_size);	/* delay 100ms */
	/* E9 */
	pkt_size += fm_bop_rd_until(0x64, 0x001F, 0x0002, &buf[pkt_size], buf_size - pkt_size);	/* Poll 64[0~4] = 2 */

	return pkt_size - 4;
}

/*
 * mt6627_pwrup_digital_init - Wholechip FM Power Up: step 4, FM Digital Init: fm_rgf_maincon
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6627_pwrup_digital_init(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6627_pwrup_digital_init_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_ENABLE_OPCODE, pkt_size);
}

static signed int mt6627_pwrup_fine_tune_reg_op(unsigned char *buf, signed int buf_size)
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
	pkt_size += fm_bop_top_write(0x50, 0x00000007, &buf[pkt_size], buf_size - pkt_size);
	/* F2 fine tune RF setting */
	pkt_size += fm_bop_write(0x01, 0xBEE8, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x03, 0xF6ED, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x15, 0x0D80, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x16, 0x0068, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x17, 0x092A, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x34, 0x807F, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x35, 0x311E, &buf[pkt_size], buf_size - pkt_size);
	/* F1 set DSP control RF register */
	pkt_size += fm_bop_top_write(0x50, 0x0000000F, &buf[pkt_size], buf_size - pkt_size);

	return pkt_size - 4;
}

/*
 * mt6627_pwrup_fine_tune - Wholechip FM Power Up: step 5, FM RF fine tune setting
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6627_pwrup_fine_tune(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6627_pwrup_fine_tune_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_ENABLE_OPCODE, pkt_size);
}

static signed int mt6627_pwrdown_reg_op(unsigned char *buf, signed int buf_size)
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
	pkt_size += fm_bop_top_write(0x50, 0x330F, &buf[pkt_size], buf_size - pkt_size);
	/* B1:Reset ASIP */
	pkt_size += fm_bop_write(0x61, 0x0001, &buf[pkt_size], buf_size - pkt_size);
	/* B2:digital core + digital rgf reset */
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	/* B3:Disable all clock */
	pkt_size += fm_bop_top_write(0x50, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	/* B4:Reset rgfrf */
	pkt_size += fm_bop_top_write(0x50, 0x4000, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_top_write(0x50, 0x0000, &buf[pkt_size], buf_size - pkt_size);
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
 * mt6627_pwrdown - Wholechip FM Power down: Digital Modem Power Down
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6627_pwrdown(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6627_pwrdown_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_ENABLE_OPCODE, pkt_size);
}

static signed int mt6627_tune_reg_op(unsigned char *buf, signed int buf_size, unsigned short freq,
					unsigned short chan_para)
{
	signed int pkt_size = 4;

	WCN_DBG(FM_ALT | CHIP, "%s enter mt6627_tune function\n", __func__);

	if (buf == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid pointer\n", __func__);
		return -1;
	}
	if (buf_size < TX_BUF_SIZE) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid buf size(%d)\n", __func__, buf_size);
		return -2;
	}

	/* Set desired channel & channel parameter */
#ifdef FM_TUNE_USE_POLL
	pkt_size += fm_bop_write(0x6A, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x6B, 0x0000, &buf[pkt_size], buf_size - pkt_size);
#endif
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, 0xFFF8, TUNE, &buf[pkt_size], buf_size - pkt_size);
	/* Wait for STC_DONE interrupt */
#ifdef FM_TUNE_USE_POLL
	pkt_size += fm_bop_rd_until(FM_MAIN_INTR, FM_INTR_STC_DONE, FM_INTR_STC_DONE, &buf[pkt_size],
				    buf_size - pkt_size);
	/* Write 1 clear the STC_DONE interrupt status flag */
	pkt_size += fm_bop_modify(FM_MAIN_INTR, 0xFFFF, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);
#endif

	WCN_DBG(FM_ALT | CHIP, "%s leave mt6627_tune function\n", __func__);

	return pkt_size - 4;
}

/*
 * mt6627_tune - execute tune action,
 * @buf - target buf
 * @buf_size - buffer size
 * @freq - 760 ~ 1080, 100KHz unit
 * return package size
 */
static signed int mt6627_tune(unsigned char *buf, signed int buf_size, unsigned short freq,
				unsigned short chan_para)
{
	signed int pkt_size = 0;

	pkt_size = mt6627_tune_reg_op(buf, buf_size, freq, chan_para);
	return fm_op_seq_combine_cmd(buf, FM_TUNE_OPCODE, pkt_size);
}

static signed int mt6627_rampdown_reg_op(unsigned char *buf, signed int buf_size)
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
/*
 * mt6627_rampdown - f/w will wait for STC_DONE interrupt
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6627_rampdown(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6627_rampdown_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_RAMPDOWN_OPCODE, pkt_size);
}

static signed int mt6627_RampDown(void)
{
	signed int ret = 0;
	unsigned short pkt_size;
	/* unsigned short tmp; */

	WCN_DBG(FM_DBG | CHIP, "ramp down\n");
	/* pwer up sequence 0425 */
	ret = fm_top_reg_write(0x0050, 0x00000007);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down wr top 0x50 failed\n");
		return ret;
	}

	ret = fm_set_bits(0x0F, 0x0000, 0xF800);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down wr 0x0f failed\n");
		return ret;
	}

	ret = fm_top_reg_write(0x0050, 0x0000000F);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down wr top 0x50 failed\n");
		return ret;
	}

	/* fm_reg_read(FM_MAIN_INTRMASK, &tmp); */
	ret = fm_reg_write(FM_MAIN_INTRMASK, 0x0000);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down wr FM_MAIN_INTRMASK failed\n");
		return ret;
	}

	ret = fm_reg_write(FM_MAIN_EXTINTRMASK, 0x0000);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down wr FM_MAIN_EXTINTRMASK failed\n");
		return ret;
	}

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6627_rampdown(cmd_buf, TX_BUF_SIZE);
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

	return ret;
}

static signed int mt6627_get_rom_version(void)
{
	unsigned short tmp = 0;
	signed int ret = 0;

	/* DSP rom code version request enable --- set 0x61 b15=1 */
	fm_set_bits(0x61, 0x8000, 0x7FFF);

	/* Release ASIP reset --- set 0x61 b1=1 */
	fm_set_bits(0x61, 0x0002, 0xFFFD);

	/* Enable ASIP power --- set 0x61 b0=0 */
	fm_set_bits(0x61, 0x0000, 0xFFFE);

	/* Wait DSP code version ready --- wait 1ms */
	do {
		fm_delayus(1000);
		ret = fm_reg_read(0x84, &tmp);
		/* ret=-4 means signal got when control FM. usually get sig 9 to kill FM process. */
		/* now cancel FM power up sequence is recommended. */
		if (ret)
			return ret;

		WCN_DBG(FM_DBG | CHIP, "0x84=%x\n", tmp);
	} while (tmp != 0x0001);

	/* Get FM DSP code version --- rd 0x83[15:8] */
	fm_reg_read(0x83, &tmp);
	tmp = (tmp >> 8);

	/* DSP rom code version request disable --- set 0x61 b15=0 */
	fm_set_bits(0x61, 0x0000, 0x7FFF);

	/* Reset ASIP --- set 0x61[1:0] = 1 */
	fm_set_bits(0x61, 0x0001, 0xFFFC);

	/* WCN_DBG(FM_NTC | CHIP, "ROM version: v%d\n", (signed int)tmp); */
	return (signed int) tmp;
}

/*
 * mt6627_pwrup_DSP_download - execute dsp/coeff patch dl action,
 * @patch_tbl - current chip patch table
 * return patch dl ok or not
 */
static signed int mt6627_pwrup_DSP_download(struct fm_patch_tbl *patch_tbl)
{
#define PATCH_BUF_SIZE (4096*6)
	signed int ret = 0;
	signed int patch_len = 0;
	unsigned char *dsp_buf = NULL;
	unsigned short tmp_reg = 0;

	mt6627_hw_info.eco_ver = (signed int) mtk_wcn_wmt_ic_info_get(1);
	WCN_DBG(FM_DBG | CHIP, "ECO version:0x%08x\n", mt6627_hw_info.eco_ver);

	/* get mt6627 DSP rom version */
	ret = mt6627_get_rom_version();
	if (ret >= 0) {
		mt6627_hw_info.rom_ver = ret;
		WCN_DBG(FM_DBG | CHIP, "ROM version: v%d\n", mt6627_hw_info.rom_ver);
	} else {
		WCN_DBG(FM_ERR | CHIP, "get ROM version failed\n");
		/* ret=-4 means signal got when control FM. usually get sig 9 to kill FM process. */
		/* now cancel FM power up sequence is recommended. */
		goto out;
	}

	/* Wholechip FM Power Up: step 3, download patch */
	dsp_buf = fm_vmalloc(PATCH_BUF_SIZE);
	if (!dsp_buf) {
		WCN_DBG(FM_ALT | CHIP, "-ENOMEM\n");
		return -ENOMEM;
	}

	patch_len = fm_get_patch_path(mt6627_hw_info.rom_ver, dsp_buf, PATCH_BUF_SIZE, patch_tbl);
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

	patch_len = fm_get_coeff_path(mt6627_hw_info.rom_ver, dsp_buf, PATCH_BUF_SIZE, patch_tbl);
	if (patch_len <= 0) {
		WCN_DBG(FM_ALT | CHIP, " fm_get_coeff_path failed\n");
		ret = patch_len;
		goto out;
	}

	mt6627_hw_info.rom_ver += 1;

	tmp_reg = dsp_buf[38] | (dsp_buf[39] << 8);	/* to be confirmed */
	mt6627_hw_info.patch_ver = (signed int) tmp_reg;
	WCN_DBG(FM_NTC | CHIP, "Patch version: 0x%08x\n", mt6627_hw_info.patch_ver);

	if (ret == 1) {
		dsp_buf[4] = 0x00;	/* if we found rom version undefined, we should disable patch */
		dsp_buf[5] = 0x00;
	}

	ret = fm_download_patch((const unsigned char *)dsp_buf, patch_len, IMG_COEFFICIENT);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " DL DSPcoeff failed\n");
		goto out;
	}
	fm_reg_write(0x92, 0x0000);	/* ? */
	fm_reg_write(0x90, 0x0040);
	fm_reg_write(0x90, 0x0000);

out:
	if (dsp_buf) {
		fm_vfree(dsp_buf);
		dsp_buf = NULL;
	}
	return ret;
}

static signed int mt6627_PowerUp(unsigned short *chip_id, unsigned short *device_id)
{
	signed int ret = 0;
	unsigned short pkt_size;
	unsigned short tmp_reg = 0;
#if	defined(MT6625_FM)
	unsigned int host_reg = 0;
#endif

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
	ret = fm_host_reg_write(0x8013000C, 0x0000801F);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwrup set CSPI failed\n");
		return ret;
	}
#if	defined(MT6625_FM)
	ret = fm_host_reg_read(0x80000224, &host_reg);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwrup read 0x80000224 failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x80000224, host_reg | (1 << 0));
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwrup conn_srcclkena enable failed\n");
		return ret;
	}

	ret = fm_host_reg_read(0x80000224, &host_reg);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwrup read 0x80000224 failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x80000224, host_reg | (1 << 16));
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwrup conn_srcclkena switch failed\n");
		return ret;
	}

	ret = fm_host_reg_read(0x80101030, &host_reg);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwrup read 0x80100030 failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x80101030, host_reg | (1 << 1));
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwrup enable top_ck_en_adie failed\n");
		return ret;
	}

	/* enable bgldo */
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
#endif
	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6627_pwrup_clock_on(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6627_pwrup_clock_on failed\n");
		return ret;
	}
/* #ifdef FM_DIGITAL_INPUT */
	/* mt6627_I2s_Setting(MT6627_I2S_ON, MT6627_I2S_MASTER, MT6627_I2S_44K); */
	/* mt_combo_audio_ctrl(COMBO_AUDIO_STATE_2); */
	/* mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)CMB_STUB_AIF_2); */
/* #endif */

	/* Wholechip FM Power Up: step 2, read HW version */
	fm_reg_read(0x62, &tmp_reg);
	/* *chip_id = tmp_reg; */
	if ((tmp_reg == 0x6625) || (tmp_reg == 0x6627))
		*chip_id = 0x6627;
	*device_id = tmp_reg;
	mt6627_hw_info.chip_id = (signed int) tmp_reg;
	WCN_DBG(FM_DBG | CHIP, "chip_id:0x%04x\n", tmp_reg);

	if ((mt6627_hw_info.chip_id != 0x6627) && (mt6627_hw_info.chip_id != 0x6625)) {
		WCN_DBG(FM_NTC | CHIP, "fm sys error, reset hw\n");
		return -FM_EFW;
	}

	/* Wholechip FM Power Up: step 3, patch download */
	ret = mt6627_pwrup_DSP_download(mt6627_patch_tbl);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "mt6627_pwrup_DSP_download failed\n");
		return ret;
	}

	/* Wholechip FM Power Up: step 4, FM Digital Init: fm_rgf_maincon */
	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6627_pwrup_digital_init(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6627_pwrup_digital_init failed\n");
		return ret;
	}
	/* Wholechip FM Power Up: step 5, FM RF fine tune setting */
	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6627_pwrup_fine_tune(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6627_pwrup_fine_tune failed\n");
		return ret;
	}
	/* enable connsys FM 2 wire RX */
	fm_reg_write(0x9B, 0xF9AB);
	fm_host_reg_write(0x80101054, 0x00003f35);

	WCN_DBG(FM_DBG | CHIP, "pwr on seq ok\n");

	return ret;
}

static signed int mt6627_PowerDown(void)
{
	signed int ret = 0;
	unsigned short pkt_size;
	unsigned short dataRead = 0;
	unsigned int tem = 0;
#if	defined(MT6625_FM)
	unsigned int host_reg = 0;

	WCN_DBG(FM_DBG | CHIP, "pwr down seq, but not clear top_clk_en_adie\n");
#endif

	WCN_DBG(FM_DBG | CHIP, "pwr down seq\n");
	/*SW work around for MCUFA issue.
	 *if interrupt happen before doing rampdown, DSP can't switch MCUFA back well.
	 * In case read interrupt, and clean if interrupt found before rampdown.
	 */
	fm_reg_read(FM_MAIN_INTR, &dataRead);

	if (dataRead & 0x1)
		fm_reg_write(FM_MAIN_INTR, dataRead);	/* clear status flag */

	/* mt6627_RampDown(); */

/* #ifdef FM_DIGITAL_INPUT */
/* mt6627_I2s_Setting(MT6627_I2S_OFF, MT6627_I2S_SLAVE, MT6627_I2S_44K); */
/* #endif */
	/* pwer up sequence 0425 */
	/* A0:set audio output I2X Rx mode: */
	fm_host_reg_read(0x80101054, &tem);
	tem = tem & 0xFFFF9FFF;
	fm_host_reg_write(0x80101054, tem);

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6627_pwrdown(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6627_pwrdown failed\n");
		return ret;
	}
	/* FIX_ME, disable ext interrupt */
	fm_reg_write(FM_MAIN_EXTINTRMASK, 0x00);

#if	defined(MT6625_FM)
	/* ret = fm_host_reg_read(0x80101030, &host_reg);
	 * if (ret) {
	 *	WCN_DBG(FM_ALT | CHIP, " pwroff read 0x80100030 failed\n");
	 *	return ret;
	 * }
	 * WCN_DBG(FM_DBG | CHIP, "read host reg 0x80101030=%x\n", host_reg);
	 * ret = fm_host_reg_write(0x80101030, host_reg & (~(0x1 << 1)));
	 * if (ret) {
	 *	WCN_DBG(FM_ALT | CHIP, " pwroff disable top_ck_en_adie failed\n");
	 *	return ret;
	 * }
	 */

	ret = fm_host_reg_read(0x80000224, &host_reg);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwroff read 0x80000224 failed\n");
		return ret;
	}
	WCN_DBG(FM_DBG | CHIP, "read host reg 0x80000224=%x\n", host_reg);
	ret = fm_host_reg_write(0x80000224, host_reg & (~(1 << 16)));
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwroff conn_srcclkena switch failed\n");
		return ret;
	}
#endif

/* rssi_th_set = false; */
	return ret;
}

/* just for dgb */
#if 0
static void mt6627_bt_write(unsigned int addr, unsigned int val)
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
static bool mt6627_SetFreq(unsigned short freq)
{
	signed int ret = 0;
	unsigned short pkt_size;
	unsigned short chan_para = 0;
	unsigned int reg_val = 0;
	unsigned short freq_reg = 0;

	fm_cb_op->cur_freq_set(freq);

#if 0
	/* MCU clock adjust if need */
	ret = mt6627_mcu_dese(freq, NULL);
	if (ret < 0)
		WCN_DBG(FM_ERR | MAIN, "mt6627_mcu_dese FAIL:%d\n", ret);

	WCN_DBG(FM_INF | MAIN, "MCU %d\n", ret);

	/* GPS clock adjust if need */
	ret = mt6627_gps_dese(freq, NULL);
	if (ret < 0)
		WCN_DBG(FM_ERR | MAIN, "mt6627_gps_dese FAIL:%d\n", ret);

	WCN_DBG(FM_INF | MAIN, "GPS %d\n", ret);
#endif
	/* pwer up sequence 0425 */
	ret = fm_top_reg_write(0x0050, 0x00000007);
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "set freq wr top 0x50 failed\n");

	ret = fm_set_bits(0x0F, 0x0455, 0xF800);
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "set freq wr 0x0f failed\n");

	if (mt6627_TDD_chan_check(freq)) {
		ret = fm_set_bits(0x30, 0x0008, 0xFFF3);	/* use TDD solution */
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "set freq wr 0x30 failed\n");
	} else {
		ret = fm_set_bits(0x30, 0x0000, 0xFFF3);	/* default use FDD solution */
		if (ret)
			WCN_DBG(FM_ERR | CHIP, "set freq wr 0x30 failed\n");
	}
	ret = fm_top_reg_write(0x0050, 0x0000000F);
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "set freq wr top 0x50 failed\n");

/* if (fm_cb_op->chan_para_get) { */
	chan_para = mt6627_chan_para_get(freq);
	WCN_DBG(FM_DBG | CHIP, "%d chan para = %d\n", (signed int) freq, (signed int) chan_para);
/* } */

	freq_reg = freq;
	if (fm_get_channel_space(freq_reg) == 0)
		freq_reg *= 10;

	freq_reg = (freq_reg - 6400) * 2 / 10;
	ret = fm_set_bits(0x65, freq_reg, 0xFC00);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "set freq wr 0x65 failed\n");
		return false;
	}

	ret = fm_set_bits(0x65, (chan_para << 12), 0x0FFF);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "set freq wr 0x65 failed\n");
		return false;
	}

	/* enable connsys FM 2 wire RX */
	fm_reg_write(0x9B, 0xF9AB);
	fm_host_reg_write(0x80101054, 0x00003f35);

	if ((mt6627_hw_info.chip_id == 0x6625)
	    && ((mtk_wcn_wmt_chipid_query() == 0x6592) || (mtk_wcn_wmt_chipid_query() == 0x6752)
		|| (mtk_wcn_wmt_chipid_query() == 0x6755) || (mtk_wcn_wmt_chipid_query() == 0x6757)
		|| (mtk_wcn_wmt_chipid_query() == 0x6763) || (mtk_wcn_wmt_chipid_query() == 0x6739))) {
		if (mt6627_I2S_hopping_check(freq)) {
			/* set i2s TX desense mode */
			ret = fm_set_bits(0x9C, 0x80, 0xFFFF);
			if (ret)
				WCN_DBG(FM_ERR | CHIP, "set freq wr 0x9C failed\n");

			/* set i2s RX desense mode */
			ret = fm_host_reg_read(0x80101054, &reg_val);
			if (ret)
				WCN_DBG(FM_ERR | CHIP, "set freq rd 0x80101054 failed\n");

			reg_val |= 0x8000;
			ret = fm_host_reg_write(0x80101054, reg_val);
			if (ret)
				WCN_DBG(FM_ERR | CHIP, "set freq wr 0x80101054 failed\n");
		} else {
			ret = fm_set_bits(0x9C, 0x0, 0xFF7F);
			if (ret)
				WCN_DBG(FM_ERR | CHIP, "set freq wr 0x9C failed\n");

			ret = fm_host_reg_read(0x80101054, &reg_val);
			if (ret)
				WCN_DBG(FM_ERR | CHIP, "set freq rd 0x80101054 failed\n");

			reg_val &= 0x7FFF;
			ret = fm_host_reg_write(0x80101054, reg_val);
			if (ret)
				WCN_DBG(FM_ERR | CHIP, "set freq wr 0x80101054 failed\n");
		}
	}

	if (FM_LOCK(cmd_buf_lock))
		return false;
	pkt_size = mt6627_tune(cmd_buf, TX_BUF_SIZE, freq, chan_para);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_TUNE | FLAG_TUNE_DONE, SW_RETRY_CNT, TUNE_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6627_tune failed\n");
		return false;
	}

	WCN_DBG(FM_DBG | CHIP, "set freq to %d ok\n", freq);
#if 0
	/* ADPLL setting for dbg */
	fm_top_reg_write(0x0050, 0x00000007);
	fm_top_reg_write(0x0A08, 0xFFFFFFFF);
	mt6627_bt_write(0x82, 0x11);
	mt6627_bt_write(0x83, 0x11);
	mt6627_bt_write(0x84, 0x11);
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

#define FM_CQI_LOG_PATH "/mnt/sdcard/fmcqilog"

static signed int mt6627_full_cqi_get(signed int min_freq, signed int max_freq, signed int space, signed int cnt)
{
	signed int ret = 0;
	unsigned short pkt_size;
	unsigned short freq, orig_freq;
	signed int i, j, k;
	signed int space_val, max, min, num;
	struct mt6627_full_cqi *p_cqi;
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
				p_cqi = (struct mt6627_full_cqi *)&fm_res->cqi[2];
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

/*
 * mt6627_GetCurRSSI - get current freq's RSSI value
 * RS=RSSI
 * If RS>511, then RSSI(dBm)= (RS-1024)/16*6
 *				   else RSSI(dBm)= RS/16*6
 */
static signed int mt6627_GetCurRSSI(signed int *pRSSI)
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

static unsigned short mt6627_vol_tbl[16] = { 0x0000, 0x0519, 0x066A, 0x0814,
	0x0A2B, 0x0CCD, 0x101D, 0x1449,
	0x198A, 0x2027, 0x287A, 0x32F5,
	0x4027, 0x50C3, 0x65AD, 0x7FFF
};

static signed int mt6627_SetVol(unsigned char vol)
{
	signed int ret = 0;

	vol = (vol > 15) ? 15 : vol;
	ret = fm_reg_write(0x7D, mt6627_vol_tbl[vol]);
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

static signed int mt6627_GetVol(unsigned char *pVol)
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
		if (mt6627_vol_tbl[i] == tmp) {
			*pVol = i;
			break;
		}
	}

	WCN_DBG(FM_DBG | CHIP, "Get vol=%d OK\n", *pVol);
	return 0;
}

static signed int mt6627_dump_reg(void)
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
static bool mt6627_GetMonoStereo(unsigned short *pMonoStereo)
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

static signed int mt6627_SetMonoStereo(signed int MonoStereo)
{
	signed int ret = 0;

	WCN_DBG(FM_NTC | CHIP, "set to %s\n", MonoStereo ? "mono" : "auto");
	fm_top_reg_write(0x50, 0x0007);

	if (MonoStereo)	/*mono */
		ret = fm_set_bits(0x75, 0x0008, ~0x0008);
	else
		ret = fm_set_bits(0x75, 0x0000, ~0x0008);

	fm_top_reg_write(0x50, 0x000F);
	return ret;
}

static signed int mt6627_GetCapArray(signed int *ca)
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
 * mt6627_GetCurPamd - get current freq's PAMD value
 * PA=PAMD
 * If PA>511 then PAMD(dB)=  (PA-1024)/16*6,
 *				else PAMD(dB)=PA/16*6
 */
static bool mt6627_GetCurPamd(unsigned short *pPamdLevl)
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

static signed int mt6627_i2s_info_get(signed int *ponoff, signed int *pmode, signed int *psample)
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

static signed int mt6627fm_get_audio_info(struct fm_audio_info_t *data)
{
	memcpy(data, &fm_config.aud_cfg, sizeof(struct fm_audio_info_t));
	return 0;
}

static signed int mt6627_hw_info_get(struct fm_hw_info *req)
{
	if (req == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	req->chip_id = mt6627_hw_info.chip_id;
	req->eco_ver = mt6627_hw_info.eco_ver;
	req->patch_ver = mt6627_hw_info.patch_ver;
	req->rom_ver = mt6627_hw_info.rom_ver;

	return 0;
}

static signed int mt6627_pre_search(void)
{
	mt6627_RampDown();
	/* disable audio output I2S Rx mode */
	fm_host_reg_write(0x80101054, 0x00000000);
	/* disable audio output I2S Tx mode */
	fm_reg_write(0x9B, 0x0000);

	return 0;
}

static signed int mt6627_restore_search(void)
{
	mt6627_RampDown();
	/* set audio output I2S Tx mode */
	fm_reg_write(0x9B, 0xF9AB);
	/* set audio output I2S Rx mode */
	fm_host_reg_write(0x80101054, 0x00003f35);
	return 0;
}

static signed int mt6627_soft_mute_tune(unsigned short freq, signed int *rssi, signed int *valid)
{
	signed int ret = 0;
	unsigned short pkt_size;
	/* unsigned short freq;//, orig_freq; */
	struct mt6627_full_cqi *p_cqi;
	signed int RSSI = 0, PAMD = 0, MR = 0, ATDC = 0;
	unsigned int PRX = 0, ATDEV = 0;
	unsigned short softmuteGainLvl = 0;

	ret = mt6627_chan_para_get(freq);
	if (ret == 2)
		ret = fm_set_bits(FM_CHANNEL_SET, 0x2000, 0x0FFF);	/* mdf HiLo */
	else
		ret = fm_set_bits(FM_CHANNEL_SET, 0x0000, 0x0FFF);	/* clear FA/HL/ATJ */

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = fm_full_cqi_req(cmd_buf, TX_BUF_SIZE, &freq, 1, 1);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_SM_TUNE, SW_RETRY_CNT, SM_TUNE_TIMEOUT, fm_get_read_result);
	FM_UNLOCK(cmd_buf_lock);

	if (!ret && fm_res) {
		p_cqi = (struct mt6627_full_cqi *)&fm_res->cqi[2];

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
		WCN_DBG(FM_NTC | CHIP,
			"valid=%d, freq %d, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x\n",
			*valid, p_cqi->ch, p_cqi->rssi, p_cqi->pamd, p_cqi->pr, p_cqi->fpamd, p_cqi->mr,
			p_cqi->atdc, p_cqi->prx, p_cqi->atdev, p_cqi->smg, p_cqi->drssi);

		*rssi = RSSI;
	} else {
		WCN_DBG(FM_ALT | CHIP, "smt get CQI failed\n");
		return false;
	}
	return true;
}

static bool mt6627_em_test(unsigned short group_idx, unsigned short item_idx, unsigned int item_value)
{
	return true;
}

/*
*parm:
*	parm.th_type: 0, RSSI. 1,desense RSSI. 2,SMG.
*	parm.th_val: threshold value
*/
static signed int mt6627_set_search_th(signed int idx, signed int val, signed int reserve)
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

static signed int MT6627fm_low_power_wa_default(signed int fmon)
{
	return 0;
}

signed int fm_low_ops_register(struct fm_callback *cb, struct fm_basic_interface *bi)
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

	bi->pwron = mt6627_pwron;
	bi->pwroff = mt6627_pwroff;
	bi->chipid_get = mt6627_get_chipid;
	bi->mute = mt6627_Mute;
	bi->rampdown = mt6627_RampDown;
	bi->pwrupseq = mt6627_PowerUp;
	bi->pwrdownseq = mt6627_PowerDown;
	bi->setfreq = mt6627_SetFreq;
	bi->low_pwr_wa = MT6627fm_low_power_wa_default;
	bi->get_aud_info = mt6627fm_get_audio_info;
	bi->rssiget = mt6627_GetCurRSSI;
	bi->volset = mt6627_SetVol;
	bi->volget = mt6627_GetVol;
	bi->dumpreg = mt6627_dump_reg;
	bi->msget = mt6627_GetMonoStereo;
	bi->msset = mt6627_SetMonoStereo;
	bi->pamdget = mt6627_GetCurPamd;
	bi->em = mt6627_em_test;
	bi->anaswitch = mt6627_SetAntennaType;
	bi->anaget = mt6627_GetAntennaType;
	bi->caparray_get = mt6627_GetCapArray;
	bi->hwinfo_get = mt6627_hw_info_get;
	bi->i2s_get = mt6627_i2s_info_get;
	bi->is_dese_chan = mt6627_is_dese_chan;
	bi->softmute_tune = mt6627_soft_mute_tune;
	bi->desense_check = mt6627_desense_check;
	bi->cqi_log = mt6627_full_cqi_get;
	bi->pre_search = mt6627_pre_search;
	bi->restore_search = mt6627_restore_search;
	bi->set_search_th = mt6627_set_search_th;

	cmd_buf_lock = fm_lock_create("27_cmd");
	ret = fm_lock_get(cmd_buf_lock);

	cmd_buf = fm_zalloc(TX_BUF_SIZE + 1);

	if (!cmd_buf) {
		WCN_DBG(FM_ALT | CHIP, "6627 fm lib alloc tx buf failed\n");
		ret = -1;
	}
#if 0				/* def CONFIG_MTK_FM_50KHZ_SUPPORT */
	cqi_fifo = fm_fifo_create("6628_cqi_fifo", sizeof(struct adapt_fm_cqi), 640);
	if (!cqi_fifo) {
		WCN_DBG(FM_ALT | CHIP, "6627 fm lib create cqi fifo failed\n");
		ret = -1;
	}
#endif

	return ret;
}

signed int fm_low_ops_unregister(struct fm_basic_interface *bi)
{
	signed int ret = 0;
	/* Basic functions. */
	if (bi == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
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
	return ret;
}

/* static struct fm_pub pub; */
/* static struct fm_pub_cb *pub_cb = &pub.pub_tbl; */

static const unsigned short mt6627_mcu_dese_list[] = {
	7630, 7800, 7940, 8320, 9260, 9600, 9710, 9920, 10400, 10410
};

static const unsigned short mt6627_gps_dese_list[] = {
	7850, 7860
};

static const signed char mt6627_chan_para_map[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,	/* 6500~6595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6600~6695 */
	2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0,	/* 6700~6795 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6800~6895 */
	0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6900~6995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7000~7095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,	/* 7100~7195 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0,	/* 7200~7295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7300~7395 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7400~7495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7500~7595 */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,	/* 7600~7695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7700~7795 */
	8, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7800~7895 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,	/* 7900~7995 */
	0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0,	/* 8000~8095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8100~8195 */
	0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8200~8295 */
	0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8300~8395 */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8400~8495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8500~8595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8600~8695 */
	0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8700~8795 */
	0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8800~8895 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8900~8995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9000~9095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9100~9195 */
	0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9200~9295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,	/* 9300~9395 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9400~9495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9500~9595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9600~9695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9700~9795 */
	0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9800~9895 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0,	/* 9900~9995 */
	0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10000~10095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10100~10195 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,	/* 10200~10295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10300~10395 */
	8, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10400~10495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10500~10595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10600~10695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0,	/* 10700~10795 */
	0			/* 10800 */
};

static const unsigned short mt6627_scan_dese_list[] = {
	6910, 7680, 7800, 9210, 9220, 9230, 9600, 9980, 9990, 10400, 10750, 10760
};

static const unsigned short mt6627_I2S_hopping_list[] = {
	6550, 6760, 6960, 6970, 7170, 7370, 7580, 7780, 7990, 8810, 9210, 9220, 10240
};

static const unsigned short mt6627_TDD_list[] = {
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
	0x0101, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9100~9195 */
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

static const unsigned short mt6627_TDD_Mask[] = {
	0x0001, 0x0010, 0x0100, 0x1000
};

/* return value: 0, not a de-sense channel; 1, this is a de-sense channel; else error no */
static signed int mt6627_is_dese_chan(unsigned short freq)
{
	signed int size;

	/* return 0;//HQA only :skip desense channel check. */
	size = ARRAY_SIZE(mt6627_scan_dese_list);

	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	while (size) {
		if (mt6627_scan_dese_list[size - 1] == freq)
			return 1;

		size--;
	}

	return 0;
}

/*  return value:
*1, is desense channel and rssi is less than threshold;
*0, not desense channel or it is but rssi is more than threshold.
*/
static signed int mt6627_desense_check(unsigned short freq, signed int rssi)
{
	if (mt6627_is_dese_chan(freq)) {
		if (rssi < fm_config.rx_cfg.desene_rssi_th)
			return 1;

		WCN_DBG(FM_DBG | CHIP, "desen_rssi %d th:%d\n", rssi, fm_config.rx_cfg.desene_rssi_th);
	}
	return 0;
}

static bool mt6627_TDD_chan_check(unsigned short freq)
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
	if ((i / 4) >= ARRAY_SIZE(mt6627_TDD_list)) {
		WCN_DBG(FM_ERR | CHIP, "Freq index out of range(%d),max(%zd)\n",
			i / 4, ARRAY_SIZE(mt6627_TDD_list));
		return false;
	}

	if (mt6627_TDD_list[i / 4] & mt6627_TDD_Mask[i % 4]) {
		WCN_DBG(FM_NTC | CHIP, "Freq %d use TDD solution\n", freq);
		return true;
	} else
		return false;
}

/* get channel parameter, HL side/ FA / ATJ */
static unsigned short mt6627_chan_para_get(unsigned short freq)
{
	signed int pos, size;

	/* return 0;//for HQA only: skip FA/HL/ATJ */
	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	if (freq < 6500)
		return 0;

	pos = (freq - 6500) / 5;

	size = ARRAY_SIZE(mt6627_chan_para_map);

	pos = (pos > (size - 1)) ? (size - 1) : pos;

	return mt6627_chan_para_map[pos];
}

static bool mt6627_I2S_hopping_check(unsigned short freq)
{
	signed int size;

	size = ARRAY_SIZE(mt6627_I2S_hopping_list);

	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	while (size) {
		if (mt6627_I2S_hopping_list[size - 1] == freq)
			return 1;
		size--;
	}

	return 0;
}
