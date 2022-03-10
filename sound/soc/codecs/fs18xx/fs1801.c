// SPDX-License-Identifier: GPL-2.0

/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2019-01-29 File created.
 */

#if defined(CONFIG_FSM_FS1801)
#include "fsm_public.h"
#include "fs1801_reg_bf.h"

#define FS1801_PRESET_EQ_LEN  (0x0069)
#define FS1801_RS2RL_RATIO    (2300)
#define FS1801_OTP_COUNT_MAX  (15)
#define FS1801_EXCER_RAM_ADDR (0xD2)

const static fsm_pll_config_t g_fs1801_pll_tbl[] = {
	/* bclk,    0xC1,   0xC2,   0xC3 */
	{  256000, 0x01A0, 0x0180, 0x0001 }, //  8000*16*2
	{  512000, 0x01A0, 0x0180, 0x0002 }, // 16000*16*2 &  8000*32*2
	{ 1024000, 0x0260, 0x0120, 0x0003 }, //            & 16000*32*2
	{ 1024032, 0x0260, 0x0120, 0x0003 }, // 32000*16*2+32
	{ 1411200, 0x01A0, 0x0100, 0x0004 }, // 44100*16*2
	{ 1536000, 0x0260, 0x0100, 0x0004 }, // 48000*16*2
	{ 2048032, 0x0260, 0x0120, 0x0006 }, //            & 32000*32*2+32
	{ 2822400, 0x01A0, 0x0100, 0x0008 }, //            & 44100*32*2
	{ 3072000, 0x0260, 0x0100, 0x0008 }, //            & 48000*32*2
};

const static struct fsm_bpcoef g_fs1801_bpcoef_table[] = {
	{  200, { 0x00181B7B, 0x001801DA, 0xffe7e485, 0x0007C629, 0xFFE8349E} },
	{  250, { 0x001817ED, 0x001801DA, 0xffe7e813, 0x0007CEEF, 0xFFE82DB5} },
	{  300, { 0x001815DA, 0x001801DA, 0xffe7edda, 0x0007D06F, 0xFFE829D8} },
	{  350, { 0x0018168A, 0x001801DA, 0xffe7e96a, 0x0007C908, 0xFFE82F78} },
	{  400, { 0x00181489, 0x001801DA, 0xffe7eb77, 0x0007CBF4, 0xFFE82B7D} },
	{  450, { 0x00181225, 0x001801DA, 0xffe7eddb, 0x0007C804, 0xFFE82625} },
	{  500, { 0x001817EE, 0x001801DA, 0xffe7e816, 0x0007C3FC, 0xFFE82DB3} },
	{  550, { 0x0018152A, 0x001801DA, 0xffe7eaca, 0x0007C0D8, 0xFFE8283B} },
	{  600, { 0x00181226, 0x001801DA, 0xffe7edde, 0x0007BF3F, 0xFFE82620} },
	{  650, { 0x00181479, 0x001801DA, 0xffe7eb87, 0x0007B6E7, 0xFFE82A92} },
	{  700, { 0x0018156D, 0x001801DA, 0xffe7ea93, 0x0007B585, 0xFFE828B5} },
	{  750, { 0x00181223, 0x001801DA, 0xffe7eddd, 0x0007B111, 0xFFE8262E} },
	{  800, { 0x00181491, 0x001801DA, 0xffe7eb6f, 0x0007A919, 0xFFE82B42} },
	{  850, { 0x0018154B, 0x001801DA, 0xffe7eab5, 0x0007A5AC, 0xFFE828FE} },
	{  900, { 0x0018122C, 0x001801DA, 0xffe7edd0, 0x00079E4B, 0xFFE82637} },
	{  950, { 0x001814D5, 0x001801DA, 0xffe7eb2b, 0x00079728, 0xFFE82BC5} },
	{ 1000, { 0x001815AF, 0x001801DA, 0xffe7ea51, 0x00079082, 0xFFE82936} },
	{ 1050, { 0x00181228, 0x001801DA, 0xffe7edd4, 0x00078AE0, 0xFFE8263F} },
	{ 1100, { 0x00181538, 0x001801DA, 0xffe7eac4, 0x00078039, 0xFFE8281F} },
	{ 1150, { 0x00181585, 0x001801DA, 0xffe7ea7b, 0x00077AC8, 0xFFE8291A} },
	{ 1200, { 0x00181237, 0x001801DA, 0xffe7edc9, 0x0007721D, 0xFFE82601} },
	{ 1250, { 0x00181564, 0x001801DA, 0xffe7ea98, 0x0007687F, 0xFFE828A4} },
	{ 1300, { 0x00181596, 0x001801DA, 0xffe7ea6e, 0x00076073, 0xFFE82940} },
	{ 1350, { 0x0018123D, 0x001801DA, 0xffe7edc3, 0x000758E1, 0xFFE8260A} },
	{ 1400, { 0x0018157B, 0x001801DA, 0xffe7ea85, 0x00074F9B, 0xFFE82899} },
	{ 1450, { 0x001815E6, 0x001801DA, 0xffe7ea1e, 0x000744F8, 0xFFE829A3} },
	{ 1500, { 0x0018123B, 0x001801DA, 0xffe7edc5, 0x00073A42, 0xFFE82619} },
	{ 1550, { 0x0018155D, 0x001801DA, 0xffe7eaa3, 0x00072E67, 0xFFE828CA} },
	{ 1600, { 0x001815F7, 0x001801DA, 0xffe7ea09, 0x00072459, 0xFFE82981} },
	{ 1650, { 0x00181200, 0x001801DA, 0xffe7edfc, 0x00071B3B, 0xFFE8266C} },
	{ 1700, { 0x001815AB, 0x001801DA, 0xffe7ea55, 0x00070FFA, 0xFFE82939} },
	{ 1750, { 0x001815C4, 0x001801DA, 0xffe7ea38, 0x00070315, 0xFFE829E4} },
	{ 1800, { 0x00181209, 0x001801DA, 0xffe7edf7, 0x0006F6C3, 0xFFE8267D} },
	{ 1850, { 0x00181586, 0x001801DA, 0xffe7ea7e, 0x0006E8B4, 0xFFE82960} },
	{ 1900, { 0x001815CA, 0x001801DA, 0xffe7ea2a, 0x0006DCD0, 0xFFE829F8} },
	{ 1950, { 0x00181210, 0x001801DA, 0xffe7edec, 0x0006D19E, 0xFFE8264C} },
	{ 2000, { 0x00181593, 0x001801DA, 0xffe7ea6d, 0x0006C076, 0xFFE82949} },
};

static int fs1801_i2c_reset(fsm_dev_t *fsm_dev)
{
	uint16_t val;
	int ret;
	int i;

	fsm_dev->acc_count = 0;
	for (i = 0; i < FSM_I2C_RETRY; i++) {
		fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), 0x0002);
		fsm_reg_read(fsm_dev, REG(FSM_SYSCTRL), NULL); // dummy read
		fsm_delay_ms(15); // 15ms
		ret = fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), 0x0001);
		 // check init finish flag
		ret |= fsm_reg_multiread(fsm_dev, REG(FSM_CHIPINI), &val);
		if ((val == 0x0003) || (val == 0x0300)) {
			break;
		}
	}
	if (i == FSM_I2C_RETRY) {
		pr_addr(err, "retry timeout");
		ret = -ETIMEDOUT;
	}

	return ret;
}

static int fs1801_config_pll(fsm_dev_t *fsm_dev, bool on)
{
	fsm_config_t *cfg = fsm_get_config();
	int idx;
	int ret;

	if (!fsm_dev || !cfg) {
		return -EINVAL;
	}
	// config pll need disable pll firstly
	ret = fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL4), 0);
	fsm_delay_ms(1);
	if (!on) {
		// disable pll
		return ret;
	}

	for (idx = 0; idx < ARRAY_SIZE(g_fs1801_pll_tbl); idx++) {
		if (g_fs1801_pll_tbl[idx].bclk == cfg->i2s_bclk) {
			break;
		}
	}
	pr_addr(debug, "bclk[%d]: %d", idx, cfg->i2s_bclk);
	if (idx >= ARRAY_SIZE(g_fs1801_pll_tbl)) {
		pr_addr(err, "Not found bclk: %d, rate: %d",
				cfg->i2s_bclk, cfg->i2s_srate);
		return -EINVAL;
	}

	ret |= fsm_access_key(fsm_dev, 1);
	if (cfg->i2s_srate == 32000) {
		ret |= fsm_reg_write(fsm_dev, REG(FSM_ANACTRL), 0x0101);
	} else {
		ret |= fsm_reg_write(fsm_dev, REG(FSM_ANACTRL), 0x0100);
	}
	ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL1), g_fs1801_pll_tbl[idx].c1);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL2), g_fs1801_pll_tbl[idx].c2);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL3), g_fs1801_pll_tbl[idx].c3);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL4), 0x000F);
	ret |= fsm_access_key(fsm_dev, 0);

	FSM_FUNC_EXIT(ret);
	return ret;
}

static int fs1801_config_i2s(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	uint16_t i2sctrl;
	uint16_t i2ssr;
	int ret;

	if (!fsm_dev || !cfg) {
		return -EINVAL;
	}
	i2ssr = fsm_get_srate_bits(fsm_dev, cfg->i2s_srate);
	if (i2ssr < 0) {
		pr_addr(err, "unsupport srate:%d", cfg->i2s_srate);
		return -EINVAL;
	}
	ret = fsm_reg_read(fsm_dev, REG(FSM_I2SCTRL), &i2sctrl);
	set_bf_val(&i2sctrl, FSM_I2SSR, i2ssr);
	set_bf_val(&i2sctrl, FSM_I2SF, FSM_FMT_I2S);
	pr_addr(debug, "srate:%d, val:%04X", cfg->i2s_srate, i2sctrl);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_I2SCTRL), i2sctrl);
	ret |= fsm_swap_channel(fsm_dev, cfg->next_angle);

	FSM_FUNC_EXIT(ret);
	return ret;
}

static int fs1801_store_otp(fsm_dev_t *fsm_dev, uint8_t valOTP)
{
	uint16_t sysctrl;
	uint16_t pllctrl4;
	uint16_t bstctrl;
	uint16_t otprdata;
	int count;
	int delta;
	int re25_otp;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	ret = fsm_access_key(fsm_dev, 1);
	// power on device and ClassD off
	ret |= fsm_reg_read(fsm_dev, REG(FSM_SYSCTRL), &sysctrl);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), 0x0000);
	ret |= fsm_reg_read(fsm_dev, REG(FSM_PLLCTRL4), &pllctrl4);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL4), 0x000F);
	ret |= fsm_reg_multiread(fsm_dev, REG(FSM_BSTCTRL), &bstctrl);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_BSTCTRL), 0x0000);

	ret |= fsm_reg_write(fsm_dev, REG(FSM_OTPADDR), 0x0010);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_OTPCMD), 0x0000);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_OTPCMD), 0x0001);

	do {
		ret |= fsm_wait_stable(fsm_dev, FSM_WAIT_OTP_READY);
		if (ret) {
			pr_addr(err, "wait OTP ready fail:%d", ret);
			break;
		}
		ret = fsm_reg_multiread(fsm_dev, REG(FSM_OTPRDATA), &otprdata);
		fsm_parse_otp(fsm_dev, otprdata, &re25_otp, &count);
		pr_addr(info, "re25 old:%d, new:%d", re25_otp, fsm_dev->re25);
		delta = abs(re25_otp - fsm_dev->re25);
		if (delta < (FSM_MAGNIF(fsm_dev->spkr) / 20)) {
			pr_addr(info, "not need to update otp, delta:%d", delta);
			break;
		}
		if (count >= fsm_dev->compat.otp_max_count) {
			pr_addr(err, "count exceeds max:%d", count);
			ret = -EINVAL;
			break;
		}
		ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL4), 0x000B);
		// enable boost and follow mode
		ret |= fsm_reg_write(fsm_dev, REG(FSM_BSTCTRL), 0x0008);
		fsm_wait_stable(fsm_dev, FSM_WAIT_BOOST_SSEND);
		ret |= fsm_reg_write(fsm_dev, REG(FSM_OTPADDR), 0x0010);
		ret |= fsm_reg_write(fsm_dev, REG(FSM_OTPWDATA), (uint16_t)valOTP);
		ret |= fsm_reg_write(fsm_dev, REG(FSM_OTPCMD), 0x0000);
		ret |= fsm_reg_write(fsm_dev, REG(FSM_OTPCMD), 0x2000);
		ret |= fsm_wait_stable(fsm_dev, FSM_WAIT_BOOST_SSEND);
		if (ret) {
			pr_addr(err, "wait boost ready failed");
			break;
		}
		ret = fsm_reg_write(fsm_dev, REG(FSM_OTPCMD), 0x2002);
		ret |= fsm_wait_stable(fsm_dev, FSM_WAIT_OTP_READY);
		if (ret) {
			pr_addr(err, "wait OTP ready failed");
			break;
		}
		ret = fsm_reg_write(fsm_dev, REG(FSM_OTPCMD), 0x0000);
		fsm_delay_ms(1);
		// disable boost and discharge
		ret |= fsm_reg_write(fsm_dev, REG(FSM_BSTCTRL), 0x0001);
		fsm_delay_ms(5);
		ret |= fsm_reg_write(fsm_dev, REG(FSM_BSTCTRL), 0x0000);
		// read back otp info
		ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL4), 0x000A);
		ret |= fsm_reg_write(fsm_dev, REG(FSM_BSTCTRL), 0x0000);
		ret |= fsm_reg_write(fsm_dev, REG(FSM_OTPADDR), 0x0010);
		ret |= fsm_reg_write(fsm_dev, REG(FSM_OTPCMD), 0x0000);
		ret |= fsm_reg_write(fsm_dev, REG(FSM_OTPCMD), 0x0001);
		ret |= fsm_wait_stable(fsm_dev, FSM_WAIT_OTP_READY);
		if (ret) {
			pr_addr(err, "wait OTP ready failed");
			break;
		}
		ret = fsm_reg_multiread(fsm_dev, REG(FSM_OTPRDATA), &otprdata);
		if (HIGH8(otprdata) != valOTP) {
			pr_addr(err, "read back failed:%04X(expect:%04X)",
					otprdata, valOTP);
			ret = -EINVAL;
			break;
		}
		fsm_parse_otp(fsm_dev, otprdata, &re25_otp, &count);
		fsm_dev->cal_count = count;
		pr_addr(info, "read back count:%d", count);
	} while (0);

	ret |= fsm_reg_write(fsm_dev, REG(FSM_BSTCTRL), bstctrl);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_PLLCTRL4), pllctrl4);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), sysctrl);
	ret |= fsm_access_key(fsm_dev, 0);

	FSM_FUNC_EXIT(ret);
	return ret;
}

static int fs1801_update_otctrl(fsm_dev_t *fsm_dev)
{
	uint16_t otppg1w0;
	uint16_t otctrl;
	int new_val;
	int offset;
	int ret;

	ret = fsm_access_key(fsm_dev, 1);
	ret |= fsm_reg_read(fsm_dev, REG(FSM_OTPPG1W0), &otppg1w0);
	ret |= fsm_reg_read(fsm_dev, REG(FSM_OTCTRL), &otctrl);
	offset = (otppg1w0 & 0x007F);
	if ((otppg1w0 & 0x0080) != 0) {
		offset = (-1 * offset);
	}
	do {
		new_val = offset + LOW8(otctrl);
		if (new_val < 0 || new_val > 0xFF)
			break;
		otctrl = ((otctrl & 0xFF00) | new_val);
		new_val = offset + HIGH8(otctrl);
		if (new_val < 0 || new_val > 0xFF)
			break;
		otctrl = ((otctrl & 0x00FF) | (new_val << 8));
		ret |= fsm_reg_write(fsm_dev, REG(FSM_OTCTRL), otctrl);
	} while (0);
	ret |= fsm_access_key(fsm_dev, 0);

	return ret;
}

int fs1801_reg_init(fsm_dev_t *fsm_dev)
{
	int ret;

	if (!fsm_dev) {
		return -EINVAL;
	}
	ret = fs1801_i2c_reset(fsm_dev);
	ret |= fsm_reg_write(fsm_dev, 0xC4, 0x000A);
	fsm_delay_ms(5); // 5ms
	ret |= fsm_reg_write(fsm_dev, 0x06, 0x0000);
	ret |= fsm_access_key(fsm_dev, 1);
	ret |= fsm_reg_write(fsm_dev, 0xD3, 0x0100);
	ret |= fsm_reg_write(fsm_dev, 0xC0, 0x15C0);
	ret |= fsm_reg_write(fsm_dev, 0xC4, 0x000F);
	ret |= fsm_reg_write(fsm_dev, 0xAE, 0x0210);
	ret |= fsm_reg_write(fsm_dev, 0xB9, 0xFFFF);
	ret |= fsm_reg_write(fsm_dev, 0x09, 0x0000);
	ret |= fsm_reg_write(fsm_dev, 0xCD, 0x2004);
	ret |= fsm_reg_write(fsm_dev, 0xA1, 0x1C92);
	ret |= fs1801_update_otctrl(fsm_dev);
	ret |= fsm_access_key(fsm_dev, 0);
	fsm_dev->errcode = ret;

	return ret;
}

static int fs1801_f0_reg_init(fsm_dev_t *fsm_dev)
{
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	ret = fs1801_i2c_reset(fsm_dev);
	ret |= fsm_access_key(fsm_dev, 1);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_AUDIOCTRL), 0xFF00);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_DACCTRL), 0x0210);
	fs1801_config_i2s(fsm_dev);
	fs1801_config_pll(fsm_dev, true);
	// Make sure 0xC4 is 0x000F
	ret |= fsm_set_bf(fsm_dev, FSM_CHS12, 3);
	// ValD0 Bypass OT
	ret |= fsm_reg_write(fsm_dev, REG(FSM_ANACTRL), 0x0120);
	//DCR Bypass
	ret |= fsm_reg_write(fsm_dev, REG(FSM_AUXCFG), 0x1020);
	// Power up
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), 0x0000);
	// Boost control
	ret |= fsm_reg_write(fsm_dev, REG(FSM_BSTCTRL), 0xBF0E);
	// ret |= fsm_reg_read(fsm_dev, REG(FSM_STATUS), NULL);
	// DSP control
	ret |= fsm_reg_write(fsm_dev, REG(FSM_DSPCTRL), 0x1012);
	// ret |= fsm_reg_write(fsm_dev, REG(FSM_ACSCTRL), 0x9880);
	// ADC control, adc env, adc time
	ret |= fsm_reg_write(fsm_dev, REG(FSM_ADCCTRL), 0x0300);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_ADCENV), 0x9FFF);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_ADCTIME), 0x0038);
	// BFL, AGC
	ret |= fsm_reg_write(fsm_dev, REG(FSM_BFLCTRL), 0x0006);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_BFLSET), 0x009F);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_AGC), 0x00B7);
	ret |= fsm_reg_write(fsm_dev, REG(FSM_DRPARA), 0x0001);
	ret |= fsm_access_key(fsm_dev, 0);
	// TS control
	ret |= fsm_reg_write(fsm_dev, REG(FSM_TSCTRL), 0x162F);
	// wait stable: DACRUN=0
	ret |= fsm_wait_stable(fsm_dev, FSM_WAIT_AMP_OFF);
	if (ret) {
		pr_addr(err, "wait timeout");
		return ret;
	}

	return ret;
}

static int fs1801_f0_ram_init(fsm_dev_t *fsm_dev)
{
	int32_t coef_ad[COEF_LEN] = {0x00674612, 0x0098B9EC, 0x001801AD, 0x002794B3, 0x001801AD};
	uint8_t buf[sizeof(uint32_t)];
	int ret;
	int i;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	ret = fsm_access_key(fsm_dev, 1);
	// Set ADC coef(B0, B1)
	ret |= fsm_reg_write(fsm_dev, REG(FSM_ADCEQA), 0x0000);
	for (i = 0; i < COEF_LEN; i++) {
		convert_data_to_bytes(coef_ad[i], buf);
		ret |= fsm_burst_write(fsm_dev, REG(FSM_ADCEQWL), buf, sizeof(uint32_t));
	}
	for (i = 0; i < COEF_LEN; i++) {
		convert_data_to_bytes(coef_ad[i], buf);
		ret |= fsm_burst_write(fsm_dev, REG(FSM_ADCEQWL), buf, sizeof(uint32_t));
	}
	ret |= fsm_access_key(fsm_dev, 0);

	return ret;
}

int fs1801_pre_f0_test(fsm_dev_t *fsm_dev)
{
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	fsm_dev->f0 = 0;
	fsm_dev->state.f0_runing = true;
	if (fsm_dev->tdata) {
		fsm_dev->tdata->test_f0 = 0;
		memset(&fsm_dev->tdata->f0, 0, sizeof(struct f0_data));
	}
	ret = fs1801_f0_reg_init(fsm_dev);
	// make sure amp off here
	// ret |= fsm_reg_read(fsm_dev, REG(FSM_SYSCTRL), NULL);
	ret = fsm_access_key(fsm_dev, 1);
	ret |= fs1801_f0_ram_init(fsm_dev);
	// ADC on
	ret |= fsm_reg_write(fsm_dev, REG(FSM_ADCCTRL), 0x1300);
	ret |= fsm_access_key(fsm_dev, 0);
	fsm_dev->errcode = ret;

	FSM_ADDR_EXIT(ret);
	return ret;
}

int fs1801_f0_test(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	const uint32_t *coef_acsbp = NULL;
	uint8_t buf[sizeof(uint32_t)];
	int freq;
	int ret;
	int i;

	if (fsm_dev == NULL || !cfg) {
		return -EINVAL;
	}

	if (!fsm_dev->state.f0_runing) {
		pr_addr(info, "invalid running state");
		return 0;
	}
	// CalculateBPCoef
	freq = cfg->test_freq;
	for (i = 0; i < ARRAY_SIZE(g_fs1801_bpcoef_table); i++) {
		if (freq == g_fs1801_bpcoef_table[i].freq) {
			coef_acsbp = g_fs1801_bpcoef_table[i].coef;
			break;
		}
	}
	if (!coef_acsbp) {
		pr_addr(err, "freq no matched: %d", freq);
		return -EINVAL;
	}
	// Keep amp off here.
	ret = fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), 0x0000);
	// wait stable
	ret = fsm_wait_stable(fsm_dev, FSM_WAIT_AMP_OFF);
	if (ret) {
		return ret;
	}
	// ACS EQ band 0 and band 1
	ret |= fsm_access_key(fsm_dev, 1);
	ret |= fsm_reg_write(fsm_dev, fsm_dev->compat.ACSEQA, 0x0000); // 0xA6
	for (i = 0; i < COEF_LEN; i++) {
		convert_data_to_bytes(coef_acsbp[i], buf);
		ret |= fsm_burst_write(fsm_dev, fsm_dev->compat.ACSEQWL, buf, sizeof(uint32_t));
	}
	for (i = 0; i < COEF_LEN; i++) {
		convert_data_to_bytes(coef_acsbp[i], buf);
		ret |= fsm_burst_write(fsm_dev, fsm_dev->compat.ACSEQWL, buf, sizeof(uint32_t));
	}
	ret |= fsm_access_key(fsm_dev, 0);
	// Amp on
	ret |= fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), 0x0008);
	// now need sleep 350ms, first time need 700ms

	FSM_ADDR_EXIT(ret);
	return ret;
}

int fs1801_post_f0_test(fsm_dev_t *fsm_dev)
{
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	if (!fsm_dev->state.f0_runing) {
		pr_addr(info, "invalid running state");
		return 0;
	}
	fsm_dev->state.f0_runing = false;
	ret = fsm_reg_write(fsm_dev, REG(FSM_SYSCTRL), 0x0001);

	return ret;
}

void fs1801_ops(fsm_dev_t *fsm_dev)
{
	if (!fsm_dev) {
		return;
	}
	fsm_dev->dev_ops.reg_init = fs1801_reg_init;
	fsm_dev->dev_ops.i2s_config = fs1801_config_i2s;
	fsm_dev->dev_ops.pll_config = fs1801_config_pll;
	fsm_dev->dev_ops.store_otp = fs1801_store_otp;
	fsm_dev->dev_ops.pre_f0_test = fs1801_pre_f0_test;
	fsm_dev->dev_ops.f0_test = fs1801_f0_test;
	fsm_dev->dev_ops.post_f0_test = fs1801_post_f0_test;
	fsm_dev->compat.preset_unit_len = FS1801_PRESET_EQ_LEN;
	fsm_dev->compat.addr_excer_ram = FS1801_EXCER_RAM_ADDR;
	fsm_dev->compat.otp_max_count = FS1801_OTP_COUNT_MAX;
	fsm_dev->compat.ACSEQA = REG(FS1801_DACEQA);
	fsm_dev->compat.ACSEQWL = REG(FS1801_DACEQWL);
	fsm_dev->compat.DACEQA = REG(FS1801_DACEQA);
	fsm_dev->compat.DACEQWL = REG(FS1801_DACEQWL);
	fsm_dev->compat.RS2RL_RATIO = FS1801_RS2RL_RATIO;
}
#endif
