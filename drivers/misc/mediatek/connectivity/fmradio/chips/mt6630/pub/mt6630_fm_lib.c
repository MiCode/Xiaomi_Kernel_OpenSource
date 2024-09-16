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

#include "mt6630_fm_reg.h"
#include "mt6630_fm_lib.h"

static struct fm_patch_tbl mt6630_patch_tbl[5] = {
	{FM_ROM_V1, "mt6630_fm_v1_patch.bin", "mt6630_fm_v1_coeff.bin", NULL, NULL},
	{FM_ROM_V2, "mt6630_fm_v2_patch.bin", "mt6630_fm_v2_coeff.bin", NULL, NULL},
	{FM_ROM_V3, "mt6630_fm_v3_patch.bin", "mt6630_fm_v3_coeff.bin", NULL, NULL},
	{FM_ROM_V4, "mt6630_fm_v4_patch.bin", "mt6630_fm_v4_coeff.bin", NULL, NULL},
	{FM_ROM_V5, "mt6630_fm_v5_patch.bin", "mt6630_fm_v5_coeff.bin", NULL, NULL},
};

static struct fm_patch_tbl mt6630_patch_tbl_tx[5] = {
	{FM_ROM_V1, "mt6630_fm_v1_patch_tx.bin", "mt6630_fm_v1_coeff_tx.bin", NULL, NULL},
	{FM_ROM_V2, "mt6630_fm_v2_patch_tx.bin", "mt6630_fm_v2_coeff_tx.bin", NULL, NULL},
	{FM_ROM_V3, "mt6630_fm_v3_patch_tx.bin", "mt6630_fm_v3_coeff_tx.bin", NULL, NULL},
	{FM_ROM_V4, "mt6630_fm_v4_patch_tx.bin", "mt6630_fm_v4_coeff_tx.bin", NULL, NULL},
	{FM_ROM_V5, "mt6630_fm_v5_patch_tx.bin", "mt6630_fm_v5_coeff_tx.bin", NULL, NULL},
};

static struct fm_hw_info mt6630_hw_info = {
	.chip_id = 0x00006630,
	.eco_ver = 0x00000000,
	.rom_ver = 0x00000000,
	.patch_ver = 0x00000000,
	.reserve = 0x00000000,
};

static struct fm_callback *fm_cb_op;
static unsigned char fm_packaging = 1;	/*0:QFN,1:WLCSP */
static unsigned int fm_sant_flag;	/* 1,Short Antenna;  0, Long Antenna */
static signed int mt6630_is_dese_chan(unsigned short freq);
#if 0
static signed int mt6630_mcu_dese(unsigned short freq, void *arg);
#endif
static signed int mt6630_gps_dese(unsigned short freq, void *arg);

static signed int mt6630_I2s_Setting(signed int onoff, signed int mode, signed int sample);
static unsigned short mt6630_chan_para_get(unsigned short freq);
static signed int mt6630_desense_check(unsigned short freq, signed int rssi);
static bool mt6630_TDD_chan_check(unsigned short freq);
static signed int mt6630_soft_mute_tune(unsigned short freq, signed int *rssi, signed int *valid);
static signed int mt6630_pwron(signed int data)
{
	if (mtk_wcn_wmt_func_on(WMTDRV_TYPE_FM) == MTK_WCN_BOOL_FALSE) {
		WCN_DBG(FM_ERR | CHIP, "WMT turn on FM Fail!\n");
		return -FM_ELINK;
	}

	WCN_DBG(FM_NTC | CHIP, "WMT turn on FM OK!\n");
	return 0;
}

static signed int mt6630_pwroff(signed int data)
{
	if (mtk_wcn_wmt_func_off(WMTDRV_TYPE_FM) == MTK_WCN_BOOL_FALSE) {
		WCN_DBG(FM_ERR | CHIP, "WMT turn off FM Fail!\n");
		return -FM_ELINK;
	}

	WCN_DBG(FM_NTC | CHIP, "WMT turn off FM OK!\n");
	return 0;
}

static unsigned short mt6630_get_chipid(void)
{
	return 0x6630;
}

/*  MT6630_SetAntennaType - set Antenna type
 *  @type - 1,Short Antenna;  0, Long Antenna
 */
static signed int mt6630_SetAntennaType(signed int type)
{
	unsigned short dataRead = 0;

	WCN_DBG(FM_NTC | CHIP, "set ana to %s\n", type ? "short" : "long");
	if (fm_packaging == 0) {
		fm_sant_flag = type;
	} else {
		fm_reg_read(FM_MAIN_CG2_CTRL, &dataRead);

		if (type)
			dataRead |= ANTENNA_TYPE;
		else
			dataRead &= (~ANTENNA_TYPE);

		fm_reg_write(FM_MAIN_CG2_CTRL, dataRead);
	}
	return 0;
}

static signed int mt6630_GetAntennaType(void)
{
	unsigned short dataRead = 0;

	if (fm_packaging == 0)
		return fm_sant_flag;

	fm_reg_read(FM_MAIN_CG2_CTRL, &dataRead);
	WCN_DBG(FM_NTC | CHIP, "get ana type: %s\n", (dataRead & ANTENNA_TYPE) ? "short" : "long");

	if (dataRead & ANTENNA_TYPE)
		return FM_ANA_SHORT;	/* short antenna */
	else
		return FM_ANA_LONG;	/* long antenna */
}

static signed int mt6630_Mute(bool mute)
{
	signed int ret = 0;
	unsigned short dataRead = 0;

	WCN_DBG(FM_NTC | CHIP, "set %s\n", mute ? "mute" : "unmute");
	fm_reg_read(FM_MAIN_CTRL, &dataRead);

	if (mute == 1)
		ret = fm_reg_write(FM_MAIN_CTRL, (dataRead & 0xFFDF) | 0x0020);
	else
		ret = fm_reg_write(FM_MAIN_CTRL, (dataRead & 0xFFDF));

	return ret;
}

signed int mt6630_rampdown_reg_op(unsigned char *buf, signed int buf_size)
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
 * mt6630_rampdown - f/w will wait for STC_DONE interrupt
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6630_rampdown(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6630_rampdown_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_RAMPDOWN_OPCODE, pkt_size);
}

static signed int mt6630_RampDown(void)
{
	signed int ret = 0;
	unsigned short pkt_size;
	/* unsigned short tmp; */

	WCN_DBG(FM_NTC | CHIP, "ramp down\n");

	ret = fm_reg_write(FM_MAIN_EXTINTRMASK, 0x0000);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down write FM_MAIN_EXTINTRMASK failed\n");
		return ret;
	}

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6630_rampdown(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_RAMPDOWN, SW_RETRY_CNT, RAMPDOWN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down failed\n");
		return ret;
	}

	ret = fm_reg_write(FM_MAIN_EXTINTRMASK, 0x0021);
	if (ret)
		WCN_DBG(FM_ERR | CHIP, "ramp down write FM_MAIN_EXTINTRMASK failed\n");

	return ret;
}

static signed int mt6630_pwrup_clock_on_reg_op(unsigned char *buf, signed int buf_size)
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

	/* B1.1    Enable digital OSC */
	pkt_size += fm_bop_write(0x60, 0x0003, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 3 */
	pkt_size += fm_bop_udelay(100, &buf[pkt_size], buf_size - pkt_size);	/* delay 100us */
	/* B1.3    Release HW clock gating */
	pkt_size += fm_bop_write(0x60, 0x0007, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 7 */
	/* B1.4    Set FM long/short antenna:1: short_antenna  0: long antenna(default) */
	pkt_size += fm_bop_modify(0x61, 0xFFEF, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	/* B1.5    Set audio output mode (lineout/I2S) 0:lineout,  1:I2S */
	if (fm_config.aud_cfg.aud_path == FM_AUD_ANALOG)
		pkt_size += fm_bop_modify(0x61, 0xFF7F, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	else
		pkt_size += fm_bop_modify(0x61, 0xFF7F, 0x0080, &buf[pkt_size], buf_size - pkt_size);

	/* B1.6    Set deemphasis setting */
	pkt_size += fm_bop_modify(0x61, ~DE_EMPHASIS, (de_emphasis << 12), &buf[pkt_size], buf_size - pkt_size);

	/* pkt_size += fm_bop_modify(0x60, OSC_FREQ_MASK, (osc_freq << 4), &buf[pkt_size], buf_size - pkt_size); */

	return pkt_size - 4;
}

/*
 * mt6630_pwrup_clock_on - Wholechip FM Power Up: step 1, FM Digital Clock enable
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6630_pwrup_clock_on(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6630_pwrup_clock_on_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_ENABLE_OPCODE, pkt_size);
}

static signed int mt6630_pwrup_digital_init_reg_op(unsigned char *buf, signed int buf_size)
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

	/* update FM ADPLL fast tracking mode gain */
	pkt_size += fm_bop_modify(0xF, 0xF800, 0x0455, &buf[pkt_size], buf_size - pkt_size);
	/* F1.4    Set appropriate interrupt mask behavior as desired(RX) */
	/* pkt_size += fm_bop_write(0x6A, 0x0021, &buf[pkt_size], buf_size - pkt_size);//wr 6A 0021 */
	pkt_size += fm_bop_write(0x6B, 0x0021, &buf[pkt_size], buf_size - pkt_size);	/* wr 6B 0021 */
	/* F1.9    Enable HW auto control */
	pkt_size += fm_bop_write(0x60, 0x000F, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 f */
	/* F1.10   Release ASIP reset */
	pkt_size += fm_bop_modify(0x61, 0xFFFD, 0x0002, &buf[pkt_size], buf_size - pkt_size);	/* wr 61 D1=1 */
	/* F1.11   Enable ASIP power */
	pkt_size += fm_bop_modify(0x61, 0xFFFE, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 61 D0=0 */
	pkt_size += fm_bop_udelay(100000, &buf[pkt_size], buf_size - pkt_size);	/* delay 100ms */
	/* F1.13   Check HW intitial complete */
	pkt_size += fm_bop_rd_until(0x64, 0x001F, 0x0002, &buf[pkt_size], buf_size - pkt_size);	/* Poll 64[0~4] = 2 */

	return pkt_size - 4;
}

/*
 * mt6630_pwrup_digital_init - Wholechip FM Power Up: step 4, FM Digital Init: fm_rgf_maincon
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6630_pwrup_digital_init(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6630_pwrup_digital_init_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_ENABLE_OPCODE, pkt_size);
}


signed int mt6630_pwrdown_reg_op(unsigned char *buf, signed int buf_size)
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

	/* Disable HW clock control */
	pkt_size += fm_bop_write(0x60, 0x0107, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 107 */
	/* Reset ASIP */
	pkt_size += fm_bop_write(0x61, 0x0001, &buf[pkt_size], buf_size - pkt_size);	/* wr 61 0001 */
	/* digital core + digital rgf reset */
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 6E[0~2] 0 */
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 6E[0~2] 0 */
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 6E[0~2] 0 */
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 6E[0~2] 0 */
	/* Disable all clock */
	pkt_size += fm_bop_write(0x60, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 0000 */
	/* Reset rgfrf */
	pkt_size += fm_bop_write(0x60, 0x4000, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 4000 */
	pkt_size += fm_bop_write(0x60, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 0000 */

	return pkt_size - 4;
}

/*
 * mt6630_pwrdown - Wholechip FM Power down: Digital Modem Power Down
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6630_pwrdown(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6630_pwrdown_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_ENABLE_OPCODE, pkt_size);
}

signed int mt6630_tune_reg_op(unsigned char *buf, signed int buf_size, unsigned short freq,
				unsigned short chan_para)
{
	signed int pkt_size = 4;

	WCN_DBG(FM_ALT | CHIP, "%s enter mt6630_tune function\n", __func__);

	if (buf == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid pointer\n", __func__);
		return -1;
	}
	if (buf_size < TX_BUF_SIZE) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid buf size(%d)\n", __func__, buf_size);
		return -2;
	}

	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	freq = (freq - 6400) * 2 / 10;

	/* Set desired channel & channel parameter */
#ifdef FM_TUNE_USE_POLL
	pkt_size += fm_bop_write(0x6A, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x6B, 0x0000, &buf[pkt_size], buf_size - pkt_size);
#endif
	pkt_size += fm_bop_modify(FM_CHANNEL_SET, 0xFC00, freq, &buf[pkt_size], buf_size - pkt_size);
	/* channel para setting, D15~D12, D15: ATJ, D13: HL, D12: FA */
	pkt_size += fm_bop_modify(FM_CHANNEL_SET, 0x0FFF, (chan_para << 12), &buf[pkt_size], buf_size - pkt_size);
	/* Enable hardware controlled tuning sequence */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, 0xFFF8, TUNE, &buf[pkt_size], buf_size - pkt_size);
	/* Wait for STC_DONE interrupt */
#ifdef FM_TUNE_USE_POLL
	pkt_size += fm_bop_rd_until(FM_MAIN_INTR, FM_INTR_STC_DONE, FM_INTR_STC_DONE, &buf[pkt_size],
				    buf_size - pkt_size);
	/* Write 1 clear the STC_DONE interrupt status flag */
	pkt_size += fm_bop_modify(FM_MAIN_INTR, 0xFFFF, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);
#endif

	return pkt_size - 4;
}

/*
 * mt6630_tune - execute tune action,
 * @buf - target buf
 * @buf_size - buffer size
 * @freq - 760 ~ 1080, 100KHz unit
 * return package size
 */
static signed int mt6630_tune(unsigned char *buf, signed int buf_size, unsigned short freq,
				unsigned short chan_para)
{
	signed int pkt_size = 0;

	pkt_size = mt6630_tune_reg_op(buf, buf_size, freq, chan_para);
	return fm_op_seq_combine_cmd(buf, FM_TUNE_OPCODE, pkt_size);
}

static signed int mt6630_get_rom_version(void)
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
	WCN_DBG(FM_NTC | CHIP, "DSP ver=0x%x\n", tmp);
	tmp = (tmp >> 8);

	/* DSP rom code version request disable --- set 0x61 b15=0 */
	fm_set_bits(0x61, 0x0000, 0x7FFF);

	/* Reset ASIP --- set 0x61[1:0] = 1 */
	fm_set_bits(0x61, 0x0001, 0xFFFC);

	return (signed int) tmp;
}

static signed int mt6630_pwrup_top_setting(void)
{
	signed int ret = 0, value = 0;
	/* A0.1 Turn on FM buffer */
	ret = fm_host_reg_read(0x8102123c, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x8102123c rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x8102123c, value & 0xFFFFFFBF);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x8102123c wr failed\n");
		return ret;
	}
	/* A0.2 Set xtal no off when FM on */
	ret = fm_host_reg_read(0x81021134, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81021134 rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x81021134, value | 0x80);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81021134 wr failed\n");
		return ret;
	}
	/* A0.3 Set top off always on when FM on */
	ret = fm_host_reg_read(0x81020010, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81020010 rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x81020010, value & 0xFFFDFFFF);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81020010 wr failed\n");
		return ret;
	}
	/* A0.4 Always enable PALDO when FM on */
	ret = fm_host_reg_read(0x81021430, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81021430 rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x81021430, value | 0x80000000);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81021430 wr failed\n");
		return ret;
	}
	/* A0.5 */
	fm_delayus(240);

	/* A0.6 MTCMOS Control */
	ret = fm_host_reg_read(0x81020008, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81020008 rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x81020008, value | 0x00000030);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81020008 wr failed\n");
		return ret;
	}
	/* A0.7 */
	fm_delayus(20);

	/* A0.8 release power on reset */
	ret = fm_host_reg_read(0x81020008, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81020008 rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x81020008, value | 0x00000001);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81020008 wr failed\n");
		return ret;
	}
	/* A0.9 enable fspi_mas_bclk_ck */
	ret = fm_host_reg_read(0x80000108, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x80000108 rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x80000108, value | 0x00000100);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x80000108 wr failed\n");
		return ret;
	}
	return ret;
}

static signed int mt6630_pwrdown_top_setting(void)
{
	signed int ret = 0, value = 0;
	/* B0.1 disable fspi_mas_bclk_ck */
	ret = fm_host_reg_read(0x80000104, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x80000104 rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x80000104, value | 0x00000100);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x80000104 wr failed\n");
		return ret;
	}
	/* B0.2 set power off reset */
	ret = fm_host_reg_read(0x81020008, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81020008 rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x81020008, value & 0xFFFFFFFE);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81020008 wr failed\n");
		return ret;
	}
	/* B0.3 */
	fm_delayus(20);

	/* B0.4 disable MTCMOS & set Iso_en */
	ret = fm_host_reg_read(0x81020008, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81020008 rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x81020008, value & 0xFFFFFFEF);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81020008 wr failed\n");
		return ret;
	}
	/* B0.5 Turn off FM buffer */
	ret = fm_host_reg_read(0x8102123c, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x8102123c rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x8102123c, value | 0x40);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x8102123c wr failed\n");
		return ret;
	}
	/* B0.6 Clear xtal no off when FM off */
	ret = fm_host_reg_read(0x81021134, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81021134 rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x81021134, value & 0xFFFFFF7F);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81021134 wr failed\n");
		return ret;
	}
	/* B0.7 Clear top off always on when FM off */
	ret = fm_host_reg_read(0x81020010, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81020010 rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x81020010, value | 0x20000);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81020010 wr failed\n");
		return ret;
	}
	/* B0.9 Disable PALDO when FM off */
	ret = fm_host_reg_read(0x81021430, &value);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81021430 rd failed\n");
		return ret;
	}
	ret = fm_host_reg_write(0x81021430, value & 0x7FFFFFFF);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " 0x81021430 wr failed\n");
		return ret;
	}

	return ret;
}

static signed int mt6630_pwrup_DSP_download(struct fm_patch_tbl *patch_tbl)
{
#define PATCH_BUF_SIZE (4096*6)
	signed int ret = 0;
	signed int patch_len = 0;
	unsigned char *dsp_buf = NULL;
	unsigned short tmp_reg = 0;

	mt6630_hw_info.eco_ver = (signed int) mtk_wcn_wmt_ic_info_get(1);
	WCN_DBG(FM_NTC | CHIP, "ECO version:0x%08x\n", mt6630_hw_info.eco_ver);

	/* FM ROM code version request */
	ret = mt6630_get_rom_version();
	if (ret >= 0) {
		mt6630_hw_info.rom_ver = ret;
		WCN_DBG(FM_NTC | CHIP, "ROM version: v%d\n", mt6630_hw_info.rom_ver);
	} else {
		WCN_DBG(FM_ERR | CHIP, "get ROM version failed\n");
		/* ret=-4 means signal got when control FM. usually get sig 9 to kill FM process. */
		/* now cancel FM power up sequence is recommended. */
		return ret;
	}

	/* Wholechip FM Power Up: step 3, download patch */
	dsp_buf = fm_vmalloc(PATCH_BUF_SIZE);
	if (!dsp_buf) {
		WCN_DBG(FM_ERR | CHIP, "-ENOMEM\n");
		return -ENOMEM;
	}

	patch_len = fm_get_patch_path(mt6630_hw_info.rom_ver, dsp_buf, PATCH_BUF_SIZE, patch_tbl);
	if (patch_len <= 0) {
		WCN_DBG(FM_ALT | CHIP, " fm_get_patch_path failed\n");
		ret = patch_len;
		goto out;
	}

	ret = fm_download_patch((const unsigned char *)dsp_buf, patch_len, IMG_PATCH);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " DL DSPpatch failed\n");
		goto out;
	}

	patch_len = fm_get_coeff_path(mt6630_hw_info.rom_ver, dsp_buf, PATCH_BUF_SIZE, patch_tbl);
	if (patch_len <= 0) {
		WCN_DBG(FM_ALT | CHIP, " fm_get_coeff_path failed\n");
		ret = patch_len;
		goto out;
	}

	mt6630_hw_info.rom_ver += 1;

	tmp_reg = dsp_buf[38] | (dsp_buf[39] << 8);	/* to be confirmed */
	mt6630_hw_info.patch_ver = (signed int) tmp_reg;
	WCN_DBG(FM_NTC | CHIP, "Patch version: 0x%08x\n", mt6630_hw_info.patch_ver);

	if (ret == 1) {
		dsp_buf[4] = 0x00;	/* if we found rom version undefined, we should disable patch */
		dsp_buf[5] = 0x00;
	}

	ret = fm_download_patch((const unsigned char *)dsp_buf, patch_len, IMG_COEFFICIENT);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, " DL DSPcoeff failed\n");
		goto out;
	}
	fm_reg_write(0x90, 0x0040);
	fm_reg_write(0x90, 0x0000);
out:
	if (dsp_buf) {
		fm_vfree(dsp_buf);
		dsp_buf = NULL;
	}
	return ret;
}

static signed int mt6630_PowerUp(unsigned short *chip_id, unsigned short *device_id)
{
	signed int ret = 0, reg = 0;
	unsigned short pkt_size;
	unsigned short tmp_reg = 0;

	if (chip_id == NULL) {
		WCN_DBG(CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (device_id == NULL) {
		WCN_DBG(CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	WCN_DBG(FM_DBG | CHIP, "pwr on seq......\n");

	ret = fm_host_reg_read(0x80021010, &reg);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "packaging rd failed\n");
	} else {
		fm_packaging = (reg & 0x00008000) >> 15;
		WCN_DBG(FM_NTC | CHIP, "fm_packaging: %d\n", fm_packaging);
	}
	ret = mt6630_pwrup_top_setting();
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "mt6630_pwrup_top_setting failed\n");
		return ret;
	}
	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6630_pwrup_clock_on(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "mt6630_pwrup_clock_on failed\n");
		return ret;
	}
	/* read HW version */
	fm_reg_read(0x62, &tmp_reg);
	*chip_id = tmp_reg;
	*device_id = tmp_reg;
	mt6630_hw_info.chip_id = (signed int) tmp_reg;
	WCN_DBG(FM_NTC | CHIP, "chip_id:0x%04x\n", tmp_reg);

	if (mt6630_hw_info.chip_id != 0x6630) {
		WCN_DBG(FM_NTC | CHIP, "fm sys error!\n");
		return -FM_EPARA;
	}
	ret = mt6630_pwrup_DSP_download(mt6630_patch_tbl);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "mt6630_pwrup_DSP_download failed\n");
		return ret;
	}

	if ((fm_config.aud_cfg.aud_path == FM_AUD_MRGIF)
	    || (fm_config.aud_cfg.aud_path == FM_AUD_I2S)) {
		mt6630_I2s_Setting(FM_I2S_ON, fm_config.aud_cfg.i2s_info.mode,
				   fm_config.aud_cfg.i2s_info.rate);
		/* mt_combo_audio_ctrl(COMBO_AUDIO_STATE_2); */
		mtk_wcn_cmb_stub_audio_ctrl((enum CMB_STUB_AIF_X) CMB_STUB_AIF_2);
	}
	/* Wholechip FM Power Up: step 4, FM Digital Init: fm_rgf_maincon */
	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6630_pwrup_digital_init(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "mt6630_pwrup_digital_init failed\n");
		return ret;
	}

	WCN_DBG(FM_NTC | CHIP, "pwr on seq ok\n");

	return ret;
}

static signed int mt6630_PowerDown(void)
{
	signed int ret = 0;
	unsigned short pkt_size;
	unsigned short dataRead = 0;

	WCN_DBG(FM_DBG | CHIP, "pwr down seq\n");
	/*SW work around for MCUFA issue.
	 *if interrupt happen before doing rampdown, DSP can't switch MCUFA back well.
	 * In case read interrupt, and clean if interrupt found before rampdown.
	 */
	fm_reg_read(FM_MAIN_INTR, &dataRead);

	if (dataRead & 0x1)
		fm_reg_write(FM_MAIN_INTR, dataRead);	/* clear status flag */

	/* mt6630_RampDown(); */

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6630_pwrdown(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "mt6630_pwrdown failed\n");
		return ret;
	}

	ret = mt6630_pwrdown_top_setting();
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "mt6630_pwrdown_top_setting failed\n");
		return ret;
	}

	return ret;
}

/* just for dgb */
static bool mt6630_SetFreq(unsigned short freq)
{
	signed int ret = 0;
	unsigned short pkt_size;
	unsigned short chan_para = 0;

	fm_cb_op->cur_freq_set(freq);

#if 0
	/* MCU clock adjust if need */
	ret = mt6630_mcu_dese(freq, NULL);
	if (ret < 0)
		WCN_DBG(FM_ERR | MAIN, "mt6630_mcu_dese FAIL:%d\n", ret);

	WCN_DBG(FM_INF | MAIN, "MCU %d\n", ret);
#endif

	/* GPS clock adjust if need */
	ret = mt6630_gps_dese(freq, NULL);
	if (ret < 0)
		WCN_DBG(FM_ERR | MAIN, "mt6630_gps_dese FAIL:%d\n", ret);

	WCN_DBG(FM_INF | MAIN, "GPS %d\n", ret);

	ret = fm_reg_write(0x60, 0x0007);
	if (ret)
		WCN_DBG(FM_ALT | MAIN, "set freq write 0x60 fail\n");

	if (mt6630_TDD_chan_check(freq)) {
		ret = fm_set_bits(0x30, 0x0004, 0xFFF9);	/* use TDD solution */
		if (ret)
			WCN_DBG(FM_ALT | MAIN, "set freq write 0x30 fail\n");
	} else {
		ret = fm_set_bits(0x30, 0x0000, 0xFFF9);	/* default use FDD solution */
		if (ret)
			WCN_DBG(FM_ALT | MAIN, "set freq write 0x30 fail\n");
	}
	ret = fm_reg_write(0x60, 0x000F);
	if (ret)
		WCN_DBG(FM_ALT | MAIN, "set freq write 0x60 fail\n");

	chan_para = mt6630_chan_para_get(freq);
	WCN_DBG(FM_DBG | CHIP, "%d chan para = %d\n", (signed int) freq, (signed int) chan_para);

	if (FM_LOCK(cmd_buf_lock))
		return false;
	pkt_size = mt6630_tune(cmd_buf, TX_BUF_SIZE, freq, chan_para);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_TUNE | FLAG_TUNE_DONE, SW_RETRY_CNT, TUNE_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "mt6630_tune failed\n");
		return false;
	}

	WCN_DBG(FM_DBG | CHIP, "set freq to %d ok\n", freq);
	return true;
}

#define FM_CQI_LOG_PATH "/mnt/sdcard/fmcqilog"

static signed int mt6630_full_cqi_get(signed int min_freq, signed int max_freq, signed int space, signed int cnt)
{
	signed int ret = 0;
	unsigned short pkt_size;
	unsigned short freq, orig_freq;
	signed int i, j, k;
	signed int space_val, max, min, num;
	struct mt6630_full_cqi *p_cqi;
	unsigned char *cqi_log_title = "Freq, RSSI, PAMD, PR, FPAMD, MR, ATDC, PRX, ATDEV, SMGain, DltaRSSI\n";
	unsigned char cqi_log_buf[100] = { 0 };
	signed int pos;
	unsigned char cqi_log_path[100] = { 0 };

	WCN_DBG(FM_DBG | CHIP, "6630 cqi log start\n");
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
				p_cqi = (struct mt6630_full_cqi *)&fm_res->cqi[2];
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
				WCN_DBG(FM_ERR | CHIP, "smt get CQI failed\n");
				ret = -1;
			}
			freq += space_val;
		}
		fm_cb_op->cur_freq_set(0);	/* avoid run too much times */
	}
	WCN_DBG(FM_DBG | CHIP, "6630 cqi log done\n");

	return ret;
}

/*
 * mt6630_GetCurRSSI - get current freq's RSSI value
 * RS=RSSI
 * If RS>511, then RSSI(dBm)= (RS-1024)/16*6
 *				   else RSSI(dBm)= RS/16*6
 */
static signed int mt6630_GetCurRSSI(signed int *pRSSI)
{
	unsigned short tmp_reg = 0;

	/* TODO: check reg */
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

static unsigned short mt6630_vol_tbl[16] = {
	0x0000, 0x0519, 0x066A, 0x0814,
	0x0A2B, 0x0CCD, 0x101D, 0x1449,
	0x198A, 0x2027, 0x287A, 0x32F5,
	0x4027, 0x50C3, 0x65AD, 0x7FFF
};

static signed int mt6630_SetVol(unsigned char vol)
{
	signed int ret = 0;

	/* TODO: check reg */
	vol = (vol > 15) ? 15 : vol;
	ret = fm_reg_write(0x7D, mt6630_vol_tbl[vol]);
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

static signed int mt6630_GetVol(unsigned char *pVol)
{
	int ret = 0;
	unsigned short tmp = 0;
	signed int i;

	if (pVol == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* TODO: check reg */
	ret = fm_reg_read(0x7D, &tmp);
	if (ret) {
		*pVol = 0;
		WCN_DBG(FM_ERR | CHIP, "Get vol Failed\n");
		return ret;
	}

	for (i = 0; i < 16; i++) {
		if (mt6630_vol_tbl[i] == tmp) {
			*pVol = i;
			break;
		}
	}

	WCN_DBG(FM_DBG | CHIP, "Get vol=%d OK\n", *pVol);
	return 0;
}

static signed int mt6630_dump_reg(void)
{
	signed int i;
	unsigned short TmpReg = 0;

	for (i = 0; i < 0xff; i++) {
		fm_reg_read(i, &TmpReg);
		WCN_DBG(FM_NTC | CHIP, "0x%02x=0x%04x\n", i, TmpReg);
	}
	return 0;
}

static bool mt6630_GetMonoStereo(unsigned short *pMonoStereo)
{
#define FM_BF_STEREO 0x1000
	unsigned short TmpReg = 0;

	/* TODO: check reg */
	if (pMonoStereo) {
		fm_reg_read(FM_RSSI_IND, &TmpReg);
		*pMonoStereo = (TmpReg & FM_BF_STEREO) >> 12;
	} else {
		WCN_DBG(FM_ERR | CHIP, "MonoStero: para err\n");
		return false;
	}

	WCN_DBG(FM_DBG | CHIP, "MonoStero:0x%04x\n", *pMonoStereo);
	return true;
}

static signed int mt6630_SetMonoStereo(signed int MonoStereo)
{
	signed int ret = 0;
#define FM_FORCE_MS 0x0008

	WCN_DBG(FM_DBG | CHIP, "set to %s\n", MonoStereo ? "mono" : "auto");
	/* TODO: check reg */

	fm_reg_write(0x60, 0x3007);

	if (MonoStereo)
		ret = fm_set_bits(0x75, FM_FORCE_MS, ~FM_FORCE_MS);
	else
		ret = fm_set_bits(0x75, 0x0000, ~FM_FORCE_MS);

	return ret;
}

static signed int mt6630_GetCapArray(signed int *ca)
{
	unsigned short dataRead = 0;
	unsigned short tmp = 0;

	/* TODO: check reg */
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
 * mt6630_GetCurPamd - get current freq's PAMD value
 * PA=PAMD
 * If PA>511 then PAMD(dB)=  (PA-1024)/16*6,
 *				else PAMD(dB)=PA/16*6
 */
static bool mt6630_GetCurPamd(unsigned short *pPamdLevl)
{
	unsigned short tmp_reg = 0;
	unsigned short dBvalue, valid_cnt = 0;
	int i, total = 0;

	for (i = 0; i < 8; i++) {
		/* TODO: check reg */
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

static signed int MT6630_FMOverBT(bool enable)
{
	signed int ret = 0;

	WCN_DBG(FM_NTC | CHIP, "+%s():\n", __func__);

	if (enable == true) {
		/* change I2S to slave mode and 48K sample rate */
		if (mt6630_I2s_Setting(FM_I2S_ON, FM_I2S_SLAVE, FM_I2S_48K))
			goto out;
		WCN_DBG(FM_NTC | CHIP, "set FM via BT controller\n");
	} else if (enable == false) {
		/* change I2S to master mode and 44.1K sample rate */
		if (mt6630_I2s_Setting(FM_I2S_ON, FM_I2S_MASTER, FM_I2S_44K))
			goto out;
		WCN_DBG(FM_NTC | CHIP, "set FM via Host\n");
	} else {
		WCN_DBG(FM_ERR | CHIP, "%s()\n", __func__);
		ret = -FM_EPARA;
		goto out;
	}
out:
	WCN_DBG(FM_NTC | CHIP, "-%s():[ret=%d]\n", __func__, ret);
	return ret;
}

/*
 * mt6630_I2s_Setting - set the I2S state on MT6630
 * @onoff - I2S on/off
 * @mode - I2S mode: Master or Slave
 *
 * Return:0, if success; error code, if failed
 */
static signed int mt6630_I2s_Setting(signed int onoff, signed int mode, signed int sample)
{
	unsigned short tmp_state = 0;
	unsigned short tmp_mode = 0;
	unsigned short tmp_sample = 0;
	signed int ret = 0;

	if (onoff == FM_I2S_ON) {
		tmp_state = 0x0003;	/* I2S enable and standard I2S mode, 0x9B D0,D1=1 */
		fm_config.aud_cfg.i2s_info.status = FM_I2S_ON;
	} else if (onoff == FM_I2S_OFF) {
		tmp_state = 0x0000;	/* I2S  off, 0x9B D0,D1=0 */
		fm_config.aud_cfg.i2s_info.status = FM_I2S_OFF;
	} else {
		WCN_DBG(FM_ERR | CHIP, "%s():[onoff=%d]\n", __func__, onoff);
		ret = -FM_EPARA;
		goto out;
	}

	if (mode == FM_I2S_MASTER) {
		tmp_mode = 0x0000;	/* 6630 as I2S master, set 0x9B D3=0 */
		fm_config.aud_cfg.i2s_info.mode = FM_I2S_MASTER;
	} else if (mode == FM_I2S_SLAVE) {
		tmp_mode = 0x0008;	/* 6630 as I2S slave, set 0x9B D3=1 */
		fm_config.aud_cfg.i2s_info.mode = FM_I2S_SLAVE;
	} else {
		WCN_DBG(FM_ERR | CHIP, "%s():[mode=%d]\n", __func__, mode);
		ret = -FM_EPARA;
		goto out;
	}

	if (sample == FM_I2S_32K) {
		tmp_sample = 0x0000;	/* 6630 I2S 32KHz sample rate, 0x5F D11~12 */
		fm_config.aud_cfg.i2s_info.rate = FM_I2S_32K;
	} else if (sample == FM_I2S_44K) {
		tmp_sample = 0x0800;	/* 6630 I2S 44.1KHz sample rate */
		fm_config.aud_cfg.i2s_info.rate = FM_I2S_44K;
	} else if (sample == FM_I2S_48K) {
		tmp_sample = 0x1000;	/* 6630 I2S 48KHz sample rate */
		fm_config.aud_cfg.i2s_info.rate = FM_I2S_48K;
	} else {
		WCN_DBG(FM_ERR | CHIP, "%s():[sample=%d]\n", __func__, sample);
		ret = -FM_EPARA;
		goto out;
	}

	ret = fm_reg_write(0x60, 0x7);
	if (ret)
		goto out;

	ret = fm_set_bits(0x5F, tmp_sample, 0xE7FF);
	if (ret)
		goto out;

	ret = fm_set_bits(0x9B, tmp_mode, 0xFFF7);
	if (ret)
		goto out;

	ret = fm_set_bits(0x9B, tmp_state, 0xFFFC);
	if (ret)
		goto out;

	/* F0.4    enable ft */
	ret = fm_set_bits(0x56, 0x1, 0xFFFE);
	if (ret)
		goto out;

	ret = fm_reg_write(0x60, 0xf);
	if (ret)
		goto out;

	WCN_DBG(FM_NTC | CHIP, "[onoff=%s][mode=%s][sample=%d](0)33KHz,(1)44.1KHz,(2)48KHz\n",
		   (onoff == FM_I2S_ON) ? "On" : "Off", (mode == FM_I2S_MASTER) ? "Master" : "Slave", sample);
out:
	return ret;
}

static signed int mt6630fm_get_audio_info(struct fm_audio_info_t *data)
{
	memcpy(data, &fm_config.aud_cfg, sizeof(struct fm_audio_info_t));
	return 0;
}

static signed int mt6630_i2s_info_get(signed int *ponoff, signed int *pmode, signed int *psample)
{
	*ponoff = fm_config.aud_cfg.i2s_info.status;
	*pmode = fm_config.aud_cfg.i2s_info.mode;
	*psample = fm_config.aud_cfg.i2s_info.rate;

	return 0;
}

static signed int mt6630_hw_info_get(struct fm_hw_info *req)
{
	if (req == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	req->chip_id = mt6630_hw_info.chip_id;
	req->eco_ver = mt6630_hw_info.eco_ver;
	req->patch_ver = mt6630_hw_info.patch_ver;
	req->rom_ver = mt6630_hw_info.rom_ver;

	return 0;
}

static signed int mt6630_pre_search(void)
{
	mt6630_RampDown();
	return 0;
}

static signed int mt6630_restore_search(void)
{
	mt6630_RampDown();
	return 0;
}

/*
*freq: 8750~10800
*valid: true-valid channel,false-invalid channel
*return: true- smt success, false-smt fail
*/
static signed int mt6630_soft_mute_tune(unsigned short freq, signed int *rssi, signed int *valid)
{
	signed int ret = 0;
	unsigned short pkt_size;
	/* unsigned short freq;//, orig_freq; */
	struct mt6630_full_cqi *p_cqi;
	signed int RSSI = 0, PAMD = 0, MR = 0, ATDC = 0;
	unsigned int PRX = 0, ATDEV = 0;
	unsigned short softmuteGainLvl = 0;

	ret = mt6630_chan_para_get(freq);
	if (ret == 2)
		ret = fm_set_bits(FM_CHANNEL_SET, 0x2000, 0x0FFF);	/* mdf HiLo */
	else
		ret = fm_set_bits(FM_CHANNEL_SET, 0x0000, 0x0FFF);	/* clear FA/HL/ATJ */
#if 0
	fm_reg_write(0x60, 0x0007);
	if (mt6630_TDD_chan_check(freq))
		fm_set_bits(0x30, 0x0004, 0xFFF9);	/* use TDD solution */
	else
		fm_set_bits(0x30, 0x0000, 0xFFF9);	/* default use FDD solution */
	fm_reg_write(0x60, 0x000F);
#endif
	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = fm_full_cqi_req(cmd_buf, TX_BUF_SIZE, &freq, 1, 1);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_SM_TUNE, SW_RETRY_CNT, SM_TUNE_TIMEOUT, fm_get_read_result);
	FM_UNLOCK(cmd_buf_lock);

	if (!ret && fm_res) {
		WCN_DBG(FM_DBG | CHIP, "smt cqi size %d\n", fm_res->cqi[0]);
		p_cqi = (struct mt6630_full_cqi *)&fm_res->cqi[2];

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
			"valid = %d, freq %d, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x\n",
			*valid, p_cqi->ch, p_cqi->rssi, p_cqi->pamd, p_cqi->pr, p_cqi->fpamd, p_cqi->mr,
			p_cqi->atdc, p_cqi->prx, p_cqi->atdev, p_cqi->smg, p_cqi->drssi);

		*rssi = RSSI;
	} else {
		WCN_DBG(FM_ERR | CHIP, "smt get CQI failed\n");
		return false;
	}
	return true;
}

/*
*parm:
*	parm.th_type: 0, RSSI. 1,desense RSSI. 2,SMG.
*	parm.th_val: threshold value
*/
static signed int mt6630_set_search_th(signed int idx, signed int val, signed int reserve)
{
	switch (idx) {
	case 0: {
		fm_config.rx_cfg.long_ana_rssi_th = val;
		WCN_DBG(FM_NTC | CHIP, "set rssi th =%d\n", val);
		break;
	}
	case 1: {
		fm_config.rx_cfg.desene_rssi_th = val;
		WCN_DBG(FM_NTC | CHIP, "set desense rssi th =%d\n", val);
		break;
	}
	case 2: {
		fm_config.rx_cfg.smg_th = val;
		WCN_DBG(FM_NTC | CHIP, "set smg th =%d\n", val);
		break;
	}
	default:
		break;
	}
	return 0;
}

#if 0
static const unsigned short mt6630_mcu_dese_list[] = {
	0			/* 7630, 7800, 7940, 8320, 9260, 9600, 9710, 9920, 10400, 10410 */
};

static const unsigned short mt6630_gps_dese_list[] = {
	0			/* 7850, 7860 */
};
#endif

static const signed char mt6630_chan_para_map[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6500~6595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6600~6695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,	/* 6700~6795 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6800~6895 */
	0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6900~6995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7000~7095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7100~7195 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7200~7295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7300~7395 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7400~7495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7500~7595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,	/* 7600~7695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7700~7795 */
	8, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7800~7895 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7900~7995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8000~8095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8100~8195 */
	0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8200~8295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,	/* 8300~8395 */
	0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8400~8495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8500~8595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8600~8695 */
	0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8700~8795 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8800~8895 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8900~8995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9000~9095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9100~9195 */
	0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9200~9295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9300~9395 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9400~9495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9500~9595 */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9600~9695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9700~9795 */
	0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9800~9895 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,	/* 9900~9995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10000~10095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10100~10195 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,	/* 10200~10295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0,	/* 10300~10395 */
	8, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10400~10495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0,	/* 10500~10595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10600~10695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0,	/* 10700~10795 */
	0			/* 10800 */
};

static const unsigned short mt6630_TDD_list[] = {
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

static const unsigned short mt6630_TDD_Mask[] = {
	0x0001, 0x0010, 0x0100, 0x1000
};

static const unsigned short mt6630_scan_dese_list[] = {
	7800, 9210, 9220, 9600, 9980, 10400, 10750, 10760
};

/* return value: 0, not a de-sense channel; 1, this is a de-sense channel; else error no */
static signed int mt6630_is_dese_chan(unsigned short freq)
{
	signed int size;

	/* return 0;//HQA only :skip desense channel check. */
	size = ARRAY_SIZE(mt6630_scan_dese_list);

	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	while (size) {
		if (mt6630_scan_dese_list[size - 1] == freq)
			return 1;

		size--;
	}

	return 0;
}

static bool mt6630_TDD_chan_check(unsigned short freq)
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
	if ((i / 4) >= ARRAY_SIZE(mt6630_TDD_list)) {
		WCN_DBG(FM_ERR | CHIP, "Freq index out of range(%d),max(%zd)\n",
			i / 4, ARRAY_SIZE(mt6630_TDD_list));
		return false;
	}

	if (mt6630_TDD_list[i / 4] & mt6630_TDD_Mask[i % 4]) {
		WCN_DBG(FM_DBG | CHIP, "Freq %d use TDD solution\n", freq);
		return true;
	} else
		return false;
}

/*  return value:
*1, is desense channel and rssi is less than threshold;
*0, not desense channel or it is but rssi is more than threshold.
*/
static signed int mt6630_desense_check(unsigned short freq, signed int rssi)
{
	if (mt6630_is_dese_chan(freq)) {
		if (rssi < fm_config.rx_cfg.desene_rssi_th)
			return 1;

		WCN_DBG(FM_DBG | CHIP, "desen_rssi %d th:%d\n", rssi, fm_config.rx_cfg.desene_rssi_th);
	}
	return 0;
}

/* get channel parameter, HL side/ FA / ATJ */
static unsigned short mt6630_chan_para_get(unsigned short freq)
{
	signed int pos, size;

	/* return 0;//for HQA only: skip FA/HL/ATJ */
	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	if (freq < 6500)
		return 0;

	pos = (freq - 6500) / 5;

	size = ARRAY_SIZE(mt6630_chan_para_map);

	pos = (pos > (size - 1)) ? (size - 1) : pos;

	return mt6630_chan_para_map[pos];
}

static signed int mt6630_gps_dese(unsigned short freq, void *arg)
{
	enum fm_gps_desense_t state = FM_GPS_DESE_DISABLE;

	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	WCN_DBG(FM_DBG | CHIP, "%s, [freq=%d]\n", __func__, (int)freq);

	if (state != FM_GPS_DESE_ENABLE) {
		if ((freq >= 7800) && (freq <= 8000))
			state = FM_GPS_DESE_ENABLE;
	}
	/* request 6630 GPS change clk */
	if (state == FM_GPS_DESE_DISABLE) {
		if (!mtk_wcn_wmt_dsns_ctrl(WMTDSNS_FM_GPS_DISABLE))
			return -1;

		return 0;
	}

	if (!mtk_wcn_wmt_dsns_ctrl(WMTDSNS_FM_GPS_ENABLE))
		return -1;

	return 1;
}

/******************************Tx function********************************************/

signed int mt6630_pwrup_clock_on_tx_reg_op(unsigned char *buf, signed int buf_size)
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

	/* B1.0    Enable digital OSC */
	pkt_size += fm_bop_write(0x60, 0x0003, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 3 */
	pkt_size += fm_bop_udelay(100, &buf[pkt_size], buf_size - pkt_size);	/* delay 100us */
	/* B1.2    Release HW clock gating */
	pkt_size += fm_bop_write(0x60, 0x0007, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 7 */
	if (fm_config.aud_cfg.aud_path == FM_AUD_ANALOG)
		pkt_size += fm_bop_modify(0x61, 0xFF7F, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	else
		pkt_size += fm_bop_modify(0x61, 0xFF7F, 0x0080, &buf[pkt_size], buf_size - pkt_size);

	/* B1.4    set TX mode: 0909 sequence */
	pkt_size += fm_bop_write(0xC7, 0x8286, &buf[pkt_size], buf_size - pkt_size);	/* wr C7 8286 */

	return pkt_size - 4;
}

/*
 * mt6630_pwrup_clock_on_tx - FM tx Digital Clock enable
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6630_pwrup_clock_on_tx(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6630_pwrup_clock_on_tx_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_ENABLE_OPCODE, pkt_size);
}

signed int mt6630_pwrup_tx_deviation_reg_op(unsigned char *buf, signed int buf_size)
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

	/* A1  switch to host control */
	pkt_size += fm_bop_write(0x60, 0x0007, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 0007 */
	/* set rgf_tx_beta_sum */
	pkt_size += fm_bop_write(0xCD, 0x72D2, &buf[pkt_size], buf_size - pkt_size);	/* wr CD 72D2 */
	/* set rgf_tx_beta_diff */
	pkt_size += fm_bop_write(0xCF, 0x787B, &buf[pkt_size], buf_size - pkt_size);	/* wr CF 787B */
	/* set rgf_tx_beta_rds */
	pkt_size += fm_bop_write(0xCE, 0x0785, &buf[pkt_size], buf_size - pkt_size);	/* wr CE 785 */
	/* set rgf_tx_beta_pilot */
	pkt_size += fm_bop_write(0xCC, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr CC 0 */
	/* set  rgf_phase_gen_rsh */
	pkt_size += fm_bop_modify(0xAD, 0xFFE8, 0x0001, &buf[pkt_size], buf_size - pkt_size);	/* wr AD D4 D2:D0=1 */
	/* set rgf_phase_gen_wb */
	pkt_size += fm_bop_modify(0xA8, 0xF000, 0x0F16, &buf[pkt_size], buf_size - pkt_size);	/* wr A8 D11:D0=F16 */
	/* set agc */
	pkt_size += fm_bop_modify(0xAE, 0xFC00, 0x020B, &buf[pkt_size], buf_size - pkt_size);	/* wr AE D9:D0=20B */
	/* set rgf_beta_fm */
	pkt_size += fm_bop_write(0xEE, 0x623D, &buf[pkt_size], buf_size - pkt_size);	/* wr EE 623D */
	/* switch to DSP control */
	pkt_size += fm_bop_write(0x60, 0x000F, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 000F */

	return pkt_size - 4;
}

/*
 * mt6630_pwrup_tx_deviation - default deviation (RDS off)
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
signed int mt6630_pwrup_tx_deviation(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6630_pwrup_tx_deviation_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, FM_ENABLE_OPCODE, pkt_size);
}

signed int mt6630_tx_rdson_deviation_reg_op(unsigned char *buf, signed int buf_size)
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

	/* A1  switch to host control */
	pkt_size += fm_bop_write(0x60, 0x0007, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 0007 */
	/* set rgf_tx_beta_sum */
	pkt_size += fm_bop_write(0xCD, 0x70E3, &buf[pkt_size], buf_size - pkt_size);	/* wr CD 70E3 */
	/* set rgf_tx_beta_diff */
	pkt_size += fm_bop_write(0xCF, 0x7675, &buf[pkt_size], buf_size - pkt_size);	/* wr CF 7675 */
	/* set rgf_tx_beta_rds:0909 sequence */
	pkt_size += fm_bop_write(0xCC, 0x0227, &buf[pkt_size], buf_size - pkt_size);	/* wr CC 227 */
	/* set rgf_tx_beta_pilot :0909 sequence */
	pkt_size += fm_bop_write(0xCE, 0x0764, &buf[pkt_size], buf_size - pkt_size);	/* wr CE 764 */
	/* set  rgf_phase_gen_rsh */
	pkt_size += fm_bop_modify(0xAD, 0xFFEF, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr AD D4 =0 */
	pkt_size += fm_bop_modify(0xAD, 0xFFF8, 0x0001, &buf[pkt_size], buf_size - pkt_size);	/* wr AD D2:D0=1 */
	/* set rgf_phase_gen_wb */
	pkt_size += fm_bop_modify(0xA8, 0xF000, 0x0222, &buf[pkt_size], buf_size - pkt_size);	/* wr A8 D11:D0=222 */
	/* set agc */
	pkt_size += fm_bop_modify(0xAE, 0xFC00, 0x0203, &buf[pkt_size], buf_size - pkt_size);	/* wr AE D9:D0=203 */
	/* set rgf_beta_fm */
	pkt_size += fm_bop_write(0xEE, 0x63EB, &buf[pkt_size], buf_size - pkt_size);	/* wr EE 63EB */
	/* switch to DSP control */
	pkt_size += fm_bop_write(0x60, 0x000F, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 000F */

	return pkt_size - 4;
}
/*
 * mt6630_tx_rdsoff_deviation -  deviation (RDS on)
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
static signed int mt6630_tx_rdson_deviation(unsigned char *buf, signed int buf_size)
{
	signed int pkt_size = 0;

	pkt_size = mt6630_tx_rdson_deviation_reg_op(buf, buf_size);
	return fm_op_seq_combine_cmd(buf, RDS_TX_OPCODE, pkt_size);
}

signed int mt6630_tune_tx_reg_op(unsigned char *buf, signed int buf_size, unsigned short freq,
					unsigned short chan_para)
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

	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	freq = (freq - 6400) * 2 / 10;
	/* Set desired channel & channel parameter */
#ifdef FM_TUNE_USE_POLL
	pkt_size += fm_bop_write(0x6B, 0x0000, &buf[pkt_size], buf_size - pkt_size);
#endif
	/* sequence 09/16:0x65 D12=1 for iq switch */
	pkt_size += fm_bop_modify(FM_CHANNEL_SET, 0xEC00, freq | 0x1000, &buf[pkt_size], buf_size - pkt_size);
	/* set 0x65[9:0] = 0x029e, => ((97.5 - 64) * 20) */
	/* set iq switch, D12 */
	/* pkt_size += fm_bop_modify(FM_CHANNEL_SET, 0x0FFF, (chan_para << 12), &buf[pkt_size], buf_size - pkt_size); */
	/* Enable hardware controlled tuning sequence */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, 0xFFF8, TUNE, &buf[pkt_size], buf_size - pkt_size);
	/* Wait for STC_DONE interrupt */
#ifdef FM_TUNE_USE_POLL
	pkt_size += fm_bop_rd_until(FM_MAIN_INTR, FM_INTR_STC_DONE, FM_INTR_STC_DONE, &buf[pkt_size],
				    buf_size - pkt_size);
	/* Write 1 clear the STC_DONE interrupt status flag */
	pkt_size += fm_bop_modify(FM_MAIN_INTR, 0xFFFF, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);
#endif

	return pkt_size - 4;
}


/*
 * mt6630_tune_tx - execute tx tune action,
 * @buf - target buf
 * @buf_size - buffer size
 * @freq - 760 ~ 1080, 100KHz unit
 * return package size
 */
static signed int mt6630_tune_tx(unsigned char *buf, signed int buf_size, unsigned short freq,
					unsigned short chan_para)
{
	signed int pkt_size = 0;

	pkt_size = mt6630_tune_tx_reg_op(buf, buf_size, freq, chan_para);
	return fm_op_seq_combine_cmd(buf, FM_TUNE_OPCODE, pkt_size);
}

signed int mt6630_rds_tx_reg_op(unsigned char *tx_buf, signed int tx_buf_size, unsigned short pi,
				unsigned short *ps, unsigned short *other_rds, unsigned char other_rds_cnt)
{
	signed int pkt_size = 4;
	signed int i = 0;

	if (tx_buf == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid pointer\n", __func__);
		return -1;
	}
	if (tx_buf_size < TX_BUF_SIZE) {
		WCN_DBG(FM_ERR | CHIP, "%s invalid buf size(%d)\n", __func__, tx_buf_size);
		return -2;
	}

	/* set repeat mode */
	pkt_size += fm_bop_modify(0x88, 0xFFFE, 0x0001, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);
	/* wr 88[0] = b'1, repeat mode */
	pkt_size += fm_bop_modify(0x88, 0xFFFB, 0x0004, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);
	/* wr 88[2] = b'1, PI_reg mode */
	pkt_size += fm_bop_write(0x8A, pi, &tx_buf[pkt_size], tx_buf_size - pkt_size);
	/* write PI to PI_reg */

	pkt_size += fm_bop_modify(0x88, 0xFFFD, 0x0002, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);
	/* wr 88[1] = b'1, addr from host */
	for (i = 0; i < 12; i++) {
		pkt_size += fm_bop_write(0x8B, (0x0063 + i), &tx_buf[pkt_size], tx_buf_size - pkt_size);
		/* 8B = mem_addr */
		pkt_size += fm_bop_write(0x8C, ps[i], &tx_buf[pkt_size], tx_buf_size - pkt_size);
		/* 8C = RDS Tx data */
	}
	pkt_size += fm_bop_modify(0x88, 0xFFFD, 0x0000, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);
	/* wr 88[1] = b'0, clear mem_addr */
	pkt_size += fm_bop_modify(0x88, 0xFFEF, 0x0010, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);
	/* wr 88[4] = b'1, switch to ps buf */
	/* work around: write at leat one group to normal buffer, otherwise ps buffer can be sent out. */
	pkt_size += fm_bop_write(0x8C, 0, &tx_buf[pkt_size], tx_buf_size - pkt_size);
	pkt_size += fm_bop_write(0x8C, 0, &tx_buf[pkt_size], tx_buf_size - pkt_size);
	pkt_size += fm_bop_write(0x8C, 0, &tx_buf[pkt_size], tx_buf_size - pkt_size);
	pkt_size += fm_bop_write(0x8C, 0, &tx_buf[pkt_size], tx_buf_size - pkt_size);
	pkt_size += fm_bop_modify(0x88, 0xFFDF, 0x0020, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);
	/* wr 88[5] = b'1,clear in_ptr */
	pkt_size += fm_bop_modify(0x88, 0xFFDF, 0x0000, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);
	/* wr 88[5] = b'0,clear in_ptr */

	return pkt_size - 4;
}

/*
*pi: pi code
*ps: block B,C,D
*other_rds: unused
*other_rds_cnt: unused
*/
static signed int mt6630_rds_tx(unsigned char *tx_buf, signed int tx_buf_size, unsigned short pi,
				unsigned short *ps, unsigned short *other_rds, unsigned char other_rds_cnt)
{
	signed int pkt_size = 0;

	pkt_size = mt6630_rds_tx_reg_op(tx_buf, tx_buf_size, pi, ps, other_rds, other_rds_cnt);
	return fm_op_seq_combine_cmd(tx_buf, RDS_TX_OPCODE, pkt_size);
}

static signed int mt6630_Tx_Support(signed int *sup)
{
	*sup = 1;
	return 0;
}

/*
*pi: pi code,
*ps: block B,C,D
*other_rds: NULL now
*other_rds_cnt:0 now
*/
static signed int MT6630_lib_Rds_Tx_adapter(unsigned short pi, unsigned short *ps, unsigned short *other_rds,
						unsigned char other_rds_cnt)
{
	signed int ret = 0;
	unsigned short pkt_size = 0;

	WCN_DBG(FM_NTC | RDSC,
		"+%s():PI=0x%04x, PS=0x%04x/0x%04x/0x%04x/0x%04x, other_rds_cnt=%d\n", __func__,
		pi, ps[0], ps[1], ps[2], ps[3], other_rds_cnt);
	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6630_rds_tx(cmd_buf, TX_BUF_SIZE, pi, ps, other_rds, other_rds_cnt);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_RDS_TX, SW_RETRY_CNT, RDS_TX_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

/*
*freq: 8750~10800
*valid: true-valid channel,false-invalid channel
*return: true- smt success, false-smt fail
*/
static signed int mt6630_soft_mute_tune_Tx(unsigned short freq, signed int *rssi, bool *valid)
{
	signed int ret = 0;
	unsigned short pkt_size;
	struct mt6630_full_cqi *p_cqi;
	signed int RSSI = 0, PAMD = 0, MR = 0, ATDC = 0;
	unsigned int PRX = 0, ATDEV = 0;
	unsigned short softmuteGainLvl = 0;

	ret = mt6630_chan_para_get(freq);
	if (ret == 2)
		ret = fm_set_bits(FM_CHANNEL_SET, 0x2000, 0x0FFF);	/* mdf HiLo */
	else
		ret = fm_set_bits(FM_CHANNEL_SET, 0x0000, 0x0FFF);	/* clear FA/HL/ATJ */

	fm_reg_write(0x60, 0x0007);
	if (mt6630_TDD_chan_check(freq))
		fm_set_bits(0x30, 0x0004, 0xFFF9);	/* use TDD solution */
	else
		fm_set_bits(0x30, 0x0000, 0xFFF9);	/* default use FDD solution */
	fm_reg_write(0x60, 0x000F);

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = fm_full_cqi_req(cmd_buf, TX_BUF_SIZE, &freq, 1, 1);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_SM_TUNE, SW_RETRY_CNT, SM_TUNE_TIMEOUT, fm_get_read_result);
	FM_UNLOCK(cmd_buf_lock);

	if (!ret && fm_res) {
		WCN_DBG(FM_NTC | CHIP, "smt cqi size %d\n", fm_res->cqi[0]);
		p_cqi = (struct mt6630_full_cqi *)&fm_res->cqi[2];
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
		if ((fm_config.tx_cfg.pamd_th < PAMD)
		    && (fm_config.tx_cfg.mr_th >= MR)
		    && (fm_config.tx_cfg.smg_th > softmuteGainLvl))
			*valid = true;
		else
			*valid = false;

		*rssi = RSSI;
	} else {
		WCN_DBG(FM_ERR | CHIP, "smt get CQI failed\n");
		return false;
	}
	WCN_DBG(FM_NTC | CHIP, "valid=%d\n", *valid);
	return true;
}

#define	TX_ABANDON_BAND_LOW1	7320
#define	TX_ABANDON_BAND_HIGH1	7450
#define	TX_ABANDON_BAND_LOW2	9760
#define	TX_ABANDON_BAND_HIGH2	9940
static signed int mt6630_TxScan(unsigned short min_freq, unsigned short max_freq, unsigned short *pFreq,
				unsigned short *pScanTBL, unsigned short *ScanTBLsize, unsigned short scandir,
				unsigned short space)
{
	signed int i = 0, ret = 0;
	unsigned short freq = *pFreq;
	unsigned short scan_cnt = *ScanTBLsize;
	unsigned short cnt = 0;
	signed int rssi = 0;
	signed int step;
	bool valid = false;
	signed int total_no = 0;

	WCN_DBG(FM_NTC | CHIP, "+%s():\n", __func__);

	if ((!pScanTBL) || (*ScanTBLsize < FM_TX_SCAN_MIN) || (*ScanTBLsize > FM_TX_SCAN_MAX)) {
		WCN_DBG(FM_ERR | CHIP, "invalid scan table\n");
		ret = -FM_EPARA;
		return 1;
	}
	if (fm_get_channel_space(freq) == 0)
		*pFreq *= 10;

	if (fm_get_channel_space(max_freq) == 0)
		max_freq *= 10;

	if (fm_get_channel_space(min_freq) == 0)
		min_freq *= 10;

	WCN_DBG(FM_NTC | CHIP,
		"[freq=%d], [max_freq=%d],[min_freq=%d],[scan BTL size=%d],[scandir=%d],[space=%d]\n",
		*pFreq, max_freq, min_freq, *ScanTBLsize, scandir, space);

	cnt = 0;
	if (space == FM_SPACE_200K)
		step = 20;
	else if (space == FM_SPACE_50K)
		step = 5;
	else
		step = 10;

	total_no = (max_freq - min_freq) / step + 1;
	if (scandir == FM_TX_SCAN_UP) {
		for (i = ((*pFreq - min_freq) / step); i < total_no; i++) {
			freq = min_freq + step * i;

			/* FM desense GPS */
			if ((freq >= TX_ABANDON_BAND_LOW1) && (freq <= TX_ABANDON_BAND_HIGH1)) {
				freq = TX_ABANDON_BAND_HIGH1 + 10;
				i = (freq - min_freq) / step;
			}

			if ((freq >= TX_ABANDON_BAND_LOW2) && (freq <= TX_ABANDON_BAND_HIGH2)) {
				freq = TX_ABANDON_BAND_HIGH2 + 10;
				i = (freq - min_freq) / step;
			}

			ret = mt6630_soft_mute_tune_Tx(freq, &rssi, &valid);
			if (ret == false) {
				WCN_DBG(FM_ERR | CHIP, "mt6630_soft_mute_tune_tx failed\n");
				return 1;
			}

			if (valid == true) {
				*(pScanTBL + cnt) = freq;	/* strore the valid empty channel */
				cnt++;
				WCN_DBG(FM_NTC | CHIP, "empty channel:[freq=%d] [cnt=%d]\n", freq, cnt);
			}
			if (cnt >= scan_cnt)
				break;
		}

		if (cnt < scan_cnt) {
			for (i = 0; i < ((*pFreq - min_freq) / step); i++) {
				freq = min_freq + step * i;

				/* FM desense GPS */
				if ((freq >= TX_ABANDON_BAND_LOW1) && (freq <= TX_ABANDON_BAND_HIGH1)) {
					freq = TX_ABANDON_BAND_HIGH1 + 10;
					i = (freq - min_freq) / step;
				}

				if ((freq >= TX_ABANDON_BAND_LOW2) && (freq <= TX_ABANDON_BAND_HIGH2)) {
					freq = TX_ABANDON_BAND_HIGH2 + 10;
					i = (freq - min_freq) / step;
				}

				if (i >= ((*pFreq - min_freq) / step))
					break;

				ret = mt6630_soft_mute_tune_Tx(freq, &rssi, &valid);
				if (ret == false) {
					WCN_DBG(FM_ERR | CHIP, "mt6630_soft_mute_tune failed\n");
					return 1;
				}

				if (valid == true) {
					*(pScanTBL + cnt) = freq;	/* strore the valid empty channel */
					cnt++;
					WCN_DBG(FM_NTC | CHIP, "empty channel:[freq=%d] [cnt=%d]\n", freq, cnt);
				}
				if (cnt >= scan_cnt)
					break;
			}
		}
	} else {
		for (i = ((*pFreq - min_freq) / step - 1); i >= 0; i--) {
			freq = min_freq + step * i;

			/* FM desense GPS */
			if ((freq >= TX_ABANDON_BAND_LOW1) && (freq <= TX_ABANDON_BAND_HIGH1)) {
				freq = TX_ABANDON_BAND_LOW1 - 10;
				i = (freq - min_freq) / step;
			}

			if ((freq >= TX_ABANDON_BAND_LOW2) && (freq <= TX_ABANDON_BAND_HIGH2)) {
				freq = TX_ABANDON_BAND_LOW2 - 10;
				i = (freq - min_freq) / step;
			}

			ret = mt6630_soft_mute_tune_Tx(freq, &rssi, &valid);
			if (ret == false) {
				WCN_DBG(FM_ERR | CHIP, "mt6630_soft_mute_tune failed\n");
				return 1;
			}

			if (valid == true) {
				*(pScanTBL + cnt) = freq;	/* strore the valid empty channel */
				cnt++;
				WCN_DBG(FM_NTC | CHIP, "empty channel:[freq=%d] [cnt=%d]\n", freq, cnt);
			}
			if (cnt >= scan_cnt)
				break;
		}
		if (cnt < scan_cnt) {
			for (i = (total_no - 1); i > ((*pFreq - min_freq) / step); i--) {
				freq = min_freq + step * i;

				/* FM desense GPS */
				if ((freq >= TX_ABANDON_BAND_LOW1) && (freq <= TX_ABANDON_BAND_HIGH1)) {
					freq = TX_ABANDON_BAND_LOW1 - 10;
					i = (freq - min_freq) / step;
				}

				if ((freq >= TX_ABANDON_BAND_LOW2) && (freq <= TX_ABANDON_BAND_HIGH2)) {
					freq = TX_ABANDON_BAND_LOW2 - 10;
					i = (freq - min_freq) / step;
				}

				if (i <= ((*pFreq - min_freq) / step))
					break;

				ret = mt6630_soft_mute_tune_Tx(freq, &rssi, &valid);
				if (ret == false) {
					WCN_DBG(FM_ERR | CHIP, "mt6630_soft_mute_tune failed\n");
					return 1;
				}

				if (valid == true) {
					*(pScanTBL + cnt) = freq;	/* strore the valid empty channel */
					cnt++;
					WCN_DBG(FM_NTC | CHIP, "empty channel:[freq=%d] [cnt=%d]\n", freq, cnt);
				}
				if (cnt >= scan_cnt)
					break;
			}
		}
	}

	*ScanTBLsize = cnt;
	WCN_DBG(FM_NTC | CHIP, "completed, [cnt=%d],[freq=%d]\n", cnt, freq);
	/* return 875~1080 */
	for (i = 0; i < cnt; i++) {
		if (fm_get_channel_space(*(pScanTBL + i)) == 1 && space != FM_SPACE_50K)
			*(pScanTBL + i) = *(pScanTBL + i) / 10;
	}
	WCN_DBG(FM_NTC | CHIP, "-%s():[ret=%d]\n", __func__, ret);
	return 0;
}

static signed int mt6630_PowerUpTx(void)
{
	signed int ret = 0;
	unsigned short pkt_size;
	unsigned short dataRead = 0;

	WCN_DBG(FM_NTC | CHIP, "pwr on Tx seq......\n");

	ret = mt6630_pwrup_top_setting();
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "mt6630_pwrup_top_setting failed\n");
		return ret;
	}
	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;

	pkt_size = mt6630_pwrup_clock_on_tx(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_pwrup_clock_on_tx failed\n");
		return ret;
	}

	fm_reg_read(0x62, &dataRead);
	WCN_DBG(FM_NTC | CHIP, "Tx on chipid=%x\n", dataRead);

	ret = mt6630_pwrup_DSP_download(mt6630_patch_tbl_tx);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "mt6630_pwrup_DSP_download failed\n");
		return ret;
	}

	if ((fm_config.aud_cfg.aud_path == FM_AUD_MRGIF)
	    || (fm_config.aud_cfg.aud_path == FM_AUD_I2S)) {
		mt6630_I2s_Setting(FM_I2S_ON, fm_config.aud_cfg.i2s_info.mode,
				   fm_config.aud_cfg.i2s_info.rate);
		/* mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)CMB_STUB_AIF_2);//no need to do? */
		WCN_DBG(FM_NTC | CHIP, "pwron set I2S on ok\n");
	}

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;

	pkt_size = mt6630_pwrup_digital_init(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_dig_init failed\n");
		return ret;
	}

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;

	pkt_size = mt6630_tx_rdson_deviation(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_RDS_TX, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_tx_rdson_deviation failed\n");
		return ret;
	}

	WCN_DBG(FM_DBG | CHIP, "pwr on tx seq ok\n");
	return ret;
}

static signed int mt6630_PowerDownTx(void)
{
	signed int ret = 0;

	ret = mt6630_PowerDown();
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "mt6630_PowerDownTx failed\n");
		return ret;
	}

	return ret;
}

static unsigned short mt6630_Hside_list_Tx[] = { 7720, 8045 };

static bool mt6630_HiSide_chan_check_Tx(unsigned short freq)
{
	/* signed int pos, size; */
	unsigned int i = 0, count = 0;

	/* return 0;//for HQA only: skip FA/HL/ATJ */
	if (fm_get_channel_space(freq) == 0)
		freq *= 10;

	if (freq < 6500)
		return false;

	count = ARRAY_SIZE(mt6630_Hside_list_Tx);
	for (i = 0; i < count; i++) {
		if (freq == mt6630_Hside_list_Tx[i])
			return true;
	}

	return false;
}

static bool MT6630_SetFreq_Tx(unsigned short freq)
{
	signed int ret = 0;
	unsigned short pkt_size;
	unsigned short chan_para = 0;
	unsigned short dataRead = 0;

	/* repeat tune due to audio noise workaround */
	fm_reg_read(0x63, &dataRead);
	fm_reg_read(0x61, &dataRead);
	fm_reg_write(0x63, 0x0);
	fm_reg_write(0x61, 0x81);
	fm_reg_write(0x61, 0x83);
	fm_reg_write(0x61, 0x82);
	/*fm_reg_write(0x69, 0x1);*/
	do {
		fm_reg_read(0x64, &dataRead);
		WCN_DBG(FM_DBG | CHIP, "dataRead = %d\n", dataRead);
	} while (dataRead != 2);

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;

	pkt_size = mt6630_tx_rdson_deviation(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_RDS_TX, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_tx_rdson_deviation failed\n");
		return ret;
	}
	/* repeat tune due to audio noise workaround end */

	ret = mt6630_RampDown();
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_RampDown failed\n");
		return ret;
	}

	if (true == mt6630_HiSide_chan_check_Tx(freq)) {
		WCN_DBG(FM_DBG | CHIP, "%d chan para = %d\n", (signed int) freq, (signed int) chan_para);
		ret = fm_set_bits(FM_CHANNEL_SET, 0x2000, 0x0FFF);	/* mdf HiLo */
	} else
		ret = fm_set_bits(FM_CHANNEL_SET, 0x0000, 0xEFFF);	/* clear HiLo */

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "fm_set_bits failed\n");
		return ret;
	}
	/* fm_cb_op->cur_freq_set(freq); */
	/* start tune */
	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = mt6630_tune_tx(cmd_buf, TX_BUF_SIZE, freq, 0);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_TUNE | FLAG_TUNE_DONE, SW_RETRY_CNT, TUNE_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_tune_tx failed\n");
		return ret;
	}

	WCN_DBG(FM_DBG | CHIP, "mt6630_tune_tx to %d ok\n", freq);

	return true;
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

	bi->pwron = mt6630_pwron;
	bi->pwroff = mt6630_pwroff;
	bi->chipid_get = mt6630_get_chipid;
	bi->mute = mt6630_Mute;
	bi->rampdown = mt6630_RampDown;
	bi->pwrupseq = mt6630_PowerUp;
	bi->pwrdownseq = mt6630_PowerDown;
	bi->setfreq = mt6630_SetFreq;
	/* bi->low_pwr_wa = MT6630fm_low_power_wa_default; */
	bi->i2s_set = mt6630_I2s_Setting;
	bi->rssiget = mt6630_GetCurRSSI;
	bi->volset = mt6630_SetVol;
	bi->volget = mt6630_GetVol;
	bi->dumpreg = mt6630_dump_reg;
	bi->msget = mt6630_GetMonoStereo;
	bi->msset = mt6630_SetMonoStereo;
	bi->pamdget = mt6630_GetCurPamd;
	/* bi->em = mt6630_em_test; */
	bi->anaswitch = mt6630_SetAntennaType;
	bi->anaget = mt6630_GetAntennaType;
	bi->caparray_get = mt6630_GetCapArray;
	bi->hwinfo_get = mt6630_hw_info_get;
	bi->fm_via_bt = MT6630_FMOverBT;
	bi->i2s_get = mt6630_i2s_info_get;
	bi->is_dese_chan = mt6630_is_dese_chan;
	bi->softmute_tune = mt6630_soft_mute_tune;
	bi->desense_check = mt6630_desense_check;
	bi->cqi_log = mt6630_full_cqi_get;
	bi->pre_search = mt6630_pre_search;
	bi->restore_search = mt6630_restore_search;
	bi->set_search_th = mt6630_set_search_th;
	bi->get_aud_info = mt6630fm_get_audio_info;
	/*****tx function****/
	bi->tx_support = mt6630_Tx_Support;
	bi->pwrupseq_tx = mt6630_PowerUpTx;
	bi->tune_tx = MT6630_SetFreq_Tx;
	bi->pwrdownseq_tx = mt6630_PowerDownTx;
	bi->tx_scan = mt6630_TxScan;
	/* need call fm link/cmd */
	bi->rds_tx_adapter = MT6630_lib_Rds_Tx_adapter;
	/* bi->tx_pwr_ctrl = MT6630_TX_PWR_CTRL; */
	/* bi->rtc_drift_ctrl = MT6630_RTC_Drift_CTRL; */
	/* bi->tx_desense_wifi = MT6630_TX_DESENSE; */

	cmd_buf_lock = fm_lock_create("30_cmd");
	ret = fm_lock_get(cmd_buf_lock);

	cmd_buf = fm_zalloc(TX_BUF_SIZE + 1);

	if (!cmd_buf) {
		WCN_DBG(FM_ERR | CHIP, "6630 fm lib alloc tx buf failed\n");
		ret = -1;
	}

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

	if (cmd_buf) {
		fm_free(cmd_buf);
		cmd_buf = NULL;
	}

	ret = fm_lock_put(cmd_buf_lock);
	fm_memset(bi, 0, sizeof(struct fm_basic_interface));
	return ret;
}
