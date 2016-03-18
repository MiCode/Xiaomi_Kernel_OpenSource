/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include "wcd9xxx-common-v2.h"

#define WCD_USLEEP_RANGE 50

enum {
	DAC_GAIN_0DB = 0,
	DAC_GAIN_0P2DB,
	DAC_GAIN_0P4DB,
	DAC_GAIN_0P6DB,
	DAC_GAIN_0P8DB,
	DAC_GAIN_M0P2DB,
	DAC_GAIN_M0P4DB,
	DAC_GAIN_M0P6DB,
};

enum {
	VREF_FILT_R_0OHM = 0,
	VREF_FILT_R_25KOHM,
	VREF_FILT_R_50KOHM,
	VREF_FILT_R_100KOHM,
};

enum {
	DELTA_I_0MA,
	DELTA_I_10MA,
	DELTA_I_20MA,
	DELTA_I_30MA,
	DELTA_I_40MA,
	DELTA_I_50MA,
};

static void (*clsh_state_fp[NUM_CLSH_STATES_V2])(struct snd_soc_codec *,
					      struct wcd_clsh_cdc_data *,
					      u8 req_state, bool en, int mode);

static bool is_native_44_1_active(struct snd_soc_codec *codec)
{
	bool native_active = false;
	u8 native_clk, rx1_rate, rx2_rate;

	native_clk = snd_soc_read(codec,
				 WCD9XXX_CDC_CLK_RST_CTRL_MCLK_CONTROL);
	rx1_rate = snd_soc_read(codec, WCD9XXX_CDC_RX1_RX_PATH_CTL);
	rx2_rate = snd_soc_read(codec, WCD9XXX_CDC_RX2_RX_PATH_CTL);

	dev_dbg(codec->dev, "%s: native_clk %x rx1_rate= %x rx2_rate= %x",
		__func__, native_clk, rx1_rate, rx2_rate);

	if ((native_clk & 0x2) &&
	    ((rx1_rate & 0x0F) == 0x9 || (rx2_rate & 0x0F) == 0x9))
		native_active = true;

	return native_active;
}

static const char *mode_to_str(int mode)
{
	switch (mode) {
	case CLS_H_NORMAL:
		return "CLS_H_NORMAL";
	case CLS_H_HIFI:
		return "CLS_H_HIFI";
	case CLS_H_LP:
		return "CLS_H_LP";
	case CLS_AB:
		return "CLS_AB";
	default:
		return "CLS_H_INVALID";
	};
}

static const char *state_to_str(u8 state, char *buf, size_t buflen)
{
	int i;
	int cnt = 0;
	/*
	 * This array of strings should match with enum wcd_clsh_state_bit.
	 */
	const char *states[] = {
		"STATE_EAR",
		"STATE_HPH_L",
		"STATE_HPH_R",
		"STATE_LO",
	};

	if (state == WCD_CLSH_STATE_IDLE) {
		snprintf(buf, buflen, "[STATE_IDLE]");
		goto done;
	}

	buf[0] = '\0';
	for (i = 0; i < ARRAY_SIZE(states); i++) {
		if (!(state & (1 << i)))
			continue;
		cnt = snprintf(buf, buflen - cnt - 1, "%s%s%s", buf,
			       buf[0] == '\0' ? "[" : "|",
			       states[i]);
	}
	if (cnt > 0)
		strlcat(buf + cnt, "]", buflen);

done:
	if (buf[0] == '\0')
		snprintf(buf, buflen, "[STATE_UNKNOWN]");
	return buf;
}

static inline void
wcd_enable_clsh_block(struct snd_soc_codec *codec,
		      struct wcd_clsh_cdc_data *clsh_d, bool enable)
{
	if ((enable && ++clsh_d->clsh_users == 1) ||
	    (!enable && --clsh_d->clsh_users == 0))
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLSH_CRC, 0x01,
				    (u8) enable);
	if (clsh_d->clsh_users < 0)
		clsh_d->clsh_users = 0;
	dev_dbg(codec->dev, "%s: clsh_users %d, enable %d", __func__,
		clsh_d->clsh_users, enable);
}

static inline bool wcd_clsh_enable_status(struct snd_soc_codec *codec)
{
	return snd_soc_read(codec, WCD9XXX_A_CDC_CLSH_CRC) & 0x01;
}

static inline int wcd_clsh_get_int_mode(struct wcd_clsh_cdc_data *clsh_d,
					int clsh_state)
{
	int mode;

	if ((clsh_state != WCD_CLSH_STATE_EAR) &&
	    (clsh_state != WCD_CLSH_STATE_HPHL) &&
	    (clsh_state != WCD_CLSH_STATE_HPHR) &&
	    (clsh_state != WCD_CLSH_STATE_LO))
		mode = CLS_NONE;
	else
		mode = clsh_d->interpolator_modes[ffs(clsh_state)];

	return mode;
}

static inline void wcd_clsh_set_int_mode(struct wcd_clsh_cdc_data *clsh_d,
					int clsh_state, int mode)
{
	if ((clsh_state != WCD_CLSH_STATE_EAR) &&
	    (clsh_state != WCD_CLSH_STATE_HPHL) &&
	    (clsh_state != WCD_CLSH_STATE_HPHR) &&
	    (clsh_state != WCD_CLSH_STATE_LO))
		return;

	clsh_d->interpolator_modes[ffs(clsh_state)] = mode;
}

static inline void wcd_clsh_set_buck_mode(struct snd_soc_codec *codec,
					  int mode)
{
	if (mode == CLS_H_HIFI)
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    0x08, 0x08); /* set to HIFI */
	else
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    0x08, 0x00); /* set to default */
}

static inline void wcd_clsh_set_flyback_mode(struct snd_soc_codec *codec,
					     int mode)
{
	if (mode == CLS_H_HIFI)
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    0x04, 0x04); /* set to HIFI */
	else
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    0x04, 0x00); /* set to Default */
}

static void wcd_clsh_buck_ctrl(struct snd_soc_codec *codec,
			       struct wcd_clsh_cdc_data *clsh_d,
			       int mode,
			       bool enable)
{
	/* enable/disable buck */
	if ((enable && (++clsh_d->buck_users == 1)) ||
	   (!enable && (--clsh_d->buck_users == 0)))
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    (1 << 7), (enable << 7));
	dev_dbg(codec->dev, "%s: buck_users %d, enable %d, mode: %s",
		__func__, clsh_d->buck_users, enable, mode_to_str(mode));
	/*
	 * 500us sleep is required after buck enable/disable
	 * as per HW requirement
	 */
	usleep_range(500, 500 + WCD_USLEEP_RANGE);
}

static void wcd_clsh_flyback_ctrl(struct snd_soc_codec *codec,
				  struct wcd_clsh_cdc_data *clsh_d,
				  int mode,
				  bool enable)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	struct wcd9xxx_reg_val bulk_reg[2];
	u8 vneg[] = {0x00, 0x40};

	/* enable/disable flyback */
	if ((enable && (++clsh_d->flyback_users == 1)) ||
	   (!enable && (--clsh_d->flyback_users == 0))) {
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    (1 << 6), (enable << 6));
		/* 100usec delay is needed as per HW requirement */
		usleep_range(100, 110);
		if (enable && (TASHA_IS_1_1(wcd9xxx->version))) {
			wcd_clsh_set_flyback_mode(codec, CLS_H_HIFI);
			snd_soc_update_bits(codec, WCD9XXX_FLYBACK_EN,
					    0x60, 0x40);
			snd_soc_update_bits(codec, WCD9XXX_FLYBACK_EN,
					    0x10, 0x10);
			vneg[0] = snd_soc_read(codec,
					       WCD9XXX_A_ANA_RX_SUPPLIES);
			vneg[0] &= ~(0x40);
			vneg[1] = vneg[0] | 0x40;
			bulk_reg[0].reg = WCD9XXX_A_ANA_RX_SUPPLIES;
			bulk_reg[0].buf = &vneg[0];
			bulk_reg[0].bytes = 1;
			bulk_reg[1].reg = WCD9XXX_A_ANA_RX_SUPPLIES;
			bulk_reg[1].buf = &vneg[1];
			bulk_reg[1].bytes = 1;
			/* 500usec delay is needed as per HW requirement */
			usleep_range(500, 510);
			wcd9xxx_slim_bulk_write(wcd9xxx, bulk_reg, 2,
						false);
			snd_soc_update_bits(codec, WCD9XXX_FLYBACK_EN,
					    0x10, 0x00);
			wcd_clsh_set_flyback_mode(codec, mode);
		}

	}
	dev_dbg(codec->dev, "%s: flyback_users %d, enable %d, mode: %s",
		__func__, clsh_d->flyback_users, enable, mode_to_str(mode));
	/*
	 * 500us sleep is required after flyback enable/disable
	 * as per HW requirement
	 */
	usleep_range(500, 500 + WCD_USLEEP_RANGE);
}

static void wcd_clsh_set_gain_path(struct snd_soc_codec *codec,
				   int mode)
{
	u8 val;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);

	if (!TASHA_IS_2_0(wcd9xxx->version))
		return;

	switch (mode) {
	case CLS_H_NORMAL:
	case CLS_AB:
		val = 0x00;
		break;
	case CLS_H_HIFI:
		val = 0x02;
		break;
	case CLS_H_LP:
		val = 0x01;
		break;
	default:
		return;
	};
	snd_soc_update_bits(codec, WCD9XXX_HPH_L_EN, 0xC0, (val << 6));
	snd_soc_update_bits(codec, WCD9XXX_HPH_R_EN, 0xC0, (val << 6));
}

static void wcd_clsh_set_hph_mode(struct snd_soc_codec *codec,
				  int mode)
{
	u8 val;
	u8 gain;
	u8 res_val = VREF_FILT_R_0OHM;
	u8 ipeak = DELTA_I_50MA;

	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);

	switch (mode) {
	case CLS_H_NORMAL:
		res_val = VREF_FILT_R_50KOHM;
		val = 0x00;
		gain = DAC_GAIN_0DB;
		ipeak = DELTA_I_50MA;
		break;
	case CLS_AB:
		val = 0x00;
		gain = DAC_GAIN_0DB;
		ipeak = DELTA_I_50MA;
		break;
	case CLS_H_HIFI:
		val = 0x08;
		gain = DAC_GAIN_M0P2DB;
		ipeak = DELTA_I_50MA;
		break;
	case CLS_H_LP:
		val = 0x04;
		ipeak = DELTA_I_30MA;
		break;
	default:
		return;
	};

	snd_soc_update_bits(codec, WCD9XXX_A_ANA_HPH, 0x0C, val);
	if (TASHA_IS_2_0(wcd9xxx->version)) {
		snd_soc_update_bits(codec, WCD9XXX_CLASSH_CTRL_VCL_2,
				    0x30, (res_val << 4));
		if (mode != CLS_H_LP)
			snd_soc_update_bits(codec, WCD9XXX_HPH_REFBUFF_UHQA_CTL,
					    0x07, gain);
		snd_soc_update_bits(codec, WCD9XXX_CLASSH_CTRL_CCL_1,
				    0xF0, (ipeak << 4));
	}
}

static void wcd_clsh_set_flyback_current(struct snd_soc_codec *codec, int mode)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);

	if (!TASHA_IS_2_0(wcd9xxx->version))
		return;

	snd_soc_update_bits(codec, WCD9XXX_RX_BIAS_FLYB_BUFF, 0x0F, 0x0A);
	snd_soc_update_bits(codec, WCD9XXX_RX_BIAS_FLYB_BUFF, 0xF0, 0xA0);
	/* Sleep needed to avoid click and pop as per HW requirement */
	usleep_range(100, 110);
}

static void wcd_clsh_set_buck_regulator_mode(struct snd_soc_codec *codec,
					     int mode)
{
	snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
			    0x02, 0x00);
}

static void wcd_clsh_state_lo(struct snd_soc_codec *codec,
			      struct wcd_clsh_cdc_data *clsh_d,
			      u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");

	if (mode != CLS_AB) {
		dev_err(codec->dev, "%s: LO cannot be in this mode: %d\n",
			__func__, mode);
		return;
	}

	if (is_enable) {
		wcd_clsh_set_buck_regulator_mode(codec, mode);
		wcd_clsh_set_buck_mode(codec, mode);
		wcd_clsh_set_flyback_mode(codec, mode);
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_flyback_current(codec, mode);
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, true);
	} else {
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, false);
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, false);
		wcd_clsh_set_flyback_mode(codec, CLS_H_NORMAL);
		wcd_clsh_set_buck_mode(codec, CLS_H_NORMAL);
		wcd_clsh_set_buck_regulator_mode(codec, CLS_H_NORMAL);
	}
}

static void wcd_clsh_state_hph_ear(struct snd_soc_codec *codec,
				   struct wcd_clsh_cdc_data *clsh_d,
				   u8 req_state, bool is_enable, int mode)
{
	int hph_mode;

	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");

	if (is_enable) {
		if (req_state == WCD_CLSH_STATE_EAR) {
			/* If HPH is running in CLS-AB when
			 * EAR comes, let it continue to run
			 * in Class-AB, no need to enable Class-H
			 * for EAR.
			 */
			if (clsh_d->state & WCD_CLSH_STATE_HPHL)
				hph_mode = wcd_clsh_get_int_mode(clsh_d,
						WCD_CLSH_STATE_HPHL);
			else if (clsh_d->state & WCD_CLSH_STATE_HPHR)
				hph_mode = wcd_clsh_get_int_mode(clsh_d,
						WCD_CLSH_STATE_HPHR);
			else
				return;
			if (hph_mode != CLS_AB && !is_native_44_1_active(codec))
				snd_soc_update_bits(codec,
						WCD9XXX_A_CDC_RX0_RX_PATH_CFG0,
						0x40, 0x40);
		}

		if (is_native_44_1_active(codec)) {
			snd_soc_write(codec, WCD9XXX_CDC_CLSH_HPH_V_PA, 0x39);
			snd_soc_update_bits(codec,
					WCD9XXX_CDC_RX0_RX_PATH_SEC0,
					0x03, 0x00);
			if ((req_state == WCD_CLSH_STATE_HPHL) ||
			    (req_state == WCD_CLSH_STATE_HPHR))
				snd_soc_update_bits(codec,
						WCD9XXX_A_CDC_RX0_RX_PATH_CFG0,
						0x40, 0x00);
		}

		if (req_state == WCD_CLSH_STATE_HPHL)
			snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX1_RX_PATH_CFG0,
					    0x40, 0x40);
		if (req_state == WCD_CLSH_STATE_HPHR)
			snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX2_RX_PATH_CFG0,
					    0x40, 0x40);
		if ((req_state == WCD_CLSH_STATE_HPHL) ||
		    (req_state == WCD_CLSH_STATE_HPHR)) {
			wcd_clsh_set_gain_path(codec, mode);
			wcd_clsh_set_flyback_mode(codec, mode);
			wcd_clsh_set_buck_mode(codec, mode);
		}
	} else {
		if (req_state == WCD_CLSH_STATE_EAR) {
			/*
			 * If EAR goes away, disable EAR Channel Enable
			 * if HPH running in Class-H otherwise
			 * and if HPH requested mode is CLS_AB then
			 * no need to disable EAR channel enable bit.
			 */
		       if (wcd_clsh_enable_status(codec))
				snd_soc_update_bits(codec,
						WCD9XXX_A_CDC_RX0_RX_PATH_CFG0,
						0x40, 0x00);
		}

		if (is_native_44_1_active(codec)) {
			snd_soc_write(codec, WCD9XXX_CDC_CLSH_HPH_V_PA, 0x1C);
			snd_soc_update_bits(codec,
					WCD9XXX_CDC_RX0_RX_PATH_SEC0,
					0x03, 0x01);
			if (((clsh_d->state & WCD_CLSH_STATE_HPH_ST)
				  != WCD_CLSH_STATE_HPH_ST) &&
			    ((req_state == WCD_CLSH_STATE_HPHL) ||
			     (req_state == WCD_CLSH_STATE_HPHR)))
				snd_soc_update_bits(codec,
						WCD9XXX_A_CDC_RX0_RX_PATH_CFG0,
						0x40, 0x40);
		}

		if (req_state == WCD_CLSH_STATE_HPHL)
			snd_soc_update_bits(codec,
					WCD9XXX_A_CDC_RX1_RX_PATH_CFG0,
					0x40, 0x00);
		if (req_state == WCD_CLSH_STATE_HPHR)
			snd_soc_update_bits(codec,
					WCD9XXX_A_CDC_RX2_RX_PATH_CFG0,
					0x40, 0x00);
		if ((req_state & WCD_CLSH_STATE_HPH_ST) &&
		    !wcd_clsh_enable_status(codec)) {
			/* If Class-H is not enabled when HPH is turned
			 * off, enable it as EAR is in progress
			 */
			wcd_enable_clsh_block(codec, clsh_d, true);
			snd_soc_update_bits(codec,
					WCD9XXX_A_CDC_RX0_RX_PATH_CFG0,
					0x40, 0x40);
			wcd_clsh_set_flyback_mode(codec, CLS_H_NORMAL);
			wcd_clsh_set_buck_mode(codec, CLS_H_NORMAL);
		}
	}
}

static void wcd_clsh_state_ear_lo(struct snd_soc_codec *codec,
				  struct wcd_clsh_cdc_data *clsh_d,
				  u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");

	if (is_enable && (req_state == WCD_CLSH_STATE_LO)) {
		wcd_clsh_set_buck_regulator_mode(codec, CLS_AB);
	} else {
		if (req_state == WCD_CLSH_STATE_EAR)
			goto end;

		/* LO powerdown.
		 * If EAR Class-H is already enabled, just
		 * turn on regulator other enable Class-H
		 * configuration
		 */
		if (wcd_clsh_enable_status(codec)) {
			wcd_clsh_set_buck_regulator_mode(codec,
					CLS_H_NORMAL);
			goto end;
		}
		wcd_enable_clsh_block(codec, clsh_d, true);
		snd_soc_update_bits(codec,
				WCD9XXX_A_CDC_RX0_RX_PATH_CFG0,
				0x40, 0x40);
		wcd_clsh_set_buck_regulator_mode(codec,
				CLS_H_NORMAL);
		wcd_clsh_set_buck_mode(codec, mode);
		wcd_clsh_set_flyback_mode(codec, mode);
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, true);
	}
end:
	return;
}

static void wcd_clsh_state_hph_lo(struct snd_soc_codec *codec,
				  struct wcd_clsh_cdc_data *clsh_d,
				  u8 req_state, bool is_enable, int mode)
{
	int hph_mode;

	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");

	if (is_enable) {
		/*
		 * If requested state is LO, put regulator
		 * in class-AB or if requested state is HPH,
		 * which means LO is already enabled, keep
		 * the regulator config the same at class-AB
		 * and just set the power modes for flyback
		 * and buck.
		 */
		if (req_state == WCD_CLSH_STATE_LO)
			wcd_clsh_set_buck_regulator_mode(codec, CLS_AB);
		else {
			if (!wcd_clsh_enable_status(codec)) {
				wcd_enable_clsh_block(codec, clsh_d, true);
				snd_soc_update_bits(codec,
						WCD9XXX_A_CDC_CLSH_K1_MSB,
						0x0F, 0x00);
				snd_soc_update_bits(codec,
						WCD9XXX_A_CDC_CLSH_K1_LSB,
						0xFF, 0xC0);
				wcd_clsh_set_flyback_mode(codec, mode);
				wcd_clsh_set_buck_mode(codec, mode);
				wcd_clsh_set_hph_mode(codec, mode);
				wcd_clsh_set_gain_path(codec, mode);
			} else {
				dev_dbg(codec->dev, "%s:clsh is already enabled\n",
					__func__);
			}
			if (req_state == WCD_CLSH_STATE_HPHL)
				snd_soc_update_bits(codec,
					WCD9XXX_A_CDC_RX1_RX_PATH_CFG0,
					0x40, 0x40);
			if (req_state == WCD_CLSH_STATE_HPHR)
				snd_soc_update_bits(codec,
					WCD9XXX_A_CDC_RX2_RX_PATH_CFG0,
					0x40, 0x40);
		}
	} else {
		if ((req_state == WCD_CLSH_STATE_HPHL) ||
		    (req_state == WCD_CLSH_STATE_HPHR)) {
			if (req_state == WCD_CLSH_STATE_HPHL)
				snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX1_RX_PATH_CFG0,
					    0x40, 0x00);
			if (req_state == WCD_CLSH_STATE_HPHR)
				snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX2_RX_PATH_CFG0,
					    0x40, 0x00);
			/*
			 * If HPH is powering down first, then disable clsh,
			 * set the buck/flyback mode to default and keep the
			 * regulator at Class-AB
			 */
			if ((clsh_d->state & WCD_CLSH_STATE_HPH_ST)
				!= WCD_CLSH_STATE_HPH_ST) {
				wcd_enable_clsh_block(codec, clsh_d, false);
				wcd_clsh_set_flyback_mode(codec, CLS_H_NORMAL);
				wcd_clsh_set_buck_mode(codec, CLS_H_NORMAL);
			}
		} else {
			/* LO powerdown.
			 * If HPH mode also is CLS-AB, no need
			 * to turn-on class-H, otherwise enable
			 * Class-H configuration.
			 */
			if (clsh_d->state & WCD_CLSH_STATE_HPHL)
				hph_mode = wcd_clsh_get_int_mode(clsh_d,
						WCD_CLSH_STATE_HPHL);
			else if (clsh_d->state & WCD_CLSH_STATE_HPHR)
				hph_mode = wcd_clsh_get_int_mode(clsh_d,
						WCD_CLSH_STATE_HPHR);
			else
				return;
			dev_dbg(codec->dev, "%s: hph_mode = %d\n", __func__,
				hph_mode);

			if ((hph_mode == CLS_AB) ||
			   (hph_mode == CLS_NONE))
				goto end;

			/*
			 * If Class-H is already enabled (HPH ON and then
			 * LO ON), no need to turn on again, just set the
			 * regulator mode.
			 */
			if (wcd_clsh_enable_status(codec)) {
				wcd_clsh_set_buck_regulator_mode(codec,
								 hph_mode);
				goto end;
			} else {
				dev_dbg(codec->dev, "%s: clsh is not enabled\n",
					__func__);
			}

			wcd_enable_clsh_block(codec, clsh_d, true);
			snd_soc_update_bits(codec,
					WCD9XXX_A_CDC_CLSH_K1_MSB,
					0x0F, 0x00);
			snd_soc_update_bits(codec,
					WCD9XXX_A_CDC_CLSH_K1_LSB,
					0xFF, 0xC0);
			wcd_clsh_set_buck_regulator_mode(codec,
							 hph_mode);
			if (clsh_d->state & WCD_CLSH_STATE_HPHL)
				snd_soc_update_bits(codec,
						WCD9XXX_A_CDC_RX1_RX_PATH_CFG0,
						0x40, 0x40);
			if (clsh_d->state & WCD_CLSH_STATE_HPHR)
				snd_soc_update_bits(codec,
						WCD9XXX_A_CDC_RX2_RX_PATH_CFG0,
						0x40, 0x40);
			wcd_clsh_set_hph_mode(codec, hph_mode);
		}
	}
end:
	return;
}

static void wcd_clsh_state_hph_st(struct snd_soc_codec *codec,
				  struct wcd_clsh_cdc_data *clsh_d,
				  u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");

	if (mode == CLS_AB)
		return;

	if (is_enable) {
		if (req_state == WCD_CLSH_STATE_HPHL)
			snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX1_RX_PATH_CFG0,
					    0x40, 0x40);
		if (req_state == WCD_CLSH_STATE_HPHR)
			snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX2_RX_PATH_CFG0,
					    0x40, 0x40);
	} else {
		if (req_state == WCD_CLSH_STATE_HPHL)
			snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX1_RX_PATH_CFG0,
					    0x40, 0x00);
		if (req_state == WCD_CLSH_STATE_HPHR)
			snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX2_RX_PATH_CFG0,
					    0x40, 0x00);
	}
}

static void wcd_clsh_state_hph_r(struct snd_soc_codec *codec,
				 struct wcd_clsh_cdc_data *clsh_d,
				 u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");

	if (mode == CLS_H_NORMAL) {
		dev_err(codec->dev, "%s: Normal mode not applicable for hph_r\n",
			__func__);
		return;
	}

	if (is_enable) {
		if (mode != CLS_AB) {
			wcd_enable_clsh_block(codec, clsh_d, true);
			/*
			 * These K1 values depend on the Headphone Impedance
			 * For now it is assumed to be 16 ohm
			 */
			snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLSH_K1_MSB,
					    0x0F, 0x00);
			snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLSH_K1_LSB,
					    0xFF, 0xC0);
			snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX2_RX_PATH_CFG0,
					    0x40, 0x40);
		}
		wcd_clsh_set_buck_regulator_mode(codec, mode);
		wcd_clsh_set_flyback_mode(codec, mode);
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_flyback_current(codec, mode);
		wcd_clsh_set_buck_mode(codec, mode);
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_hph_mode(codec, mode);
		wcd_clsh_set_gain_path(codec, mode);
	} else {
		wcd_clsh_set_hph_mode(codec, CLS_H_NORMAL);

		if (mode != CLS_AB) {
			snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX2_RX_PATH_CFG0,
					    0x40, 0x00);
			wcd_enable_clsh_block(codec, clsh_d, false);
		}
		/* buck and flyback set to default mode and disable */
		wcd_clsh_buck_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_flyback_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_set_flyback_mode(codec, CLS_H_NORMAL);
		wcd_clsh_set_buck_mode(codec, CLS_H_NORMAL);
		wcd_clsh_set_buck_regulator_mode(codec, CLS_H_NORMAL);
	}
}

static void wcd_clsh_state_hph_l(struct snd_soc_codec *codec,
				 struct wcd_clsh_cdc_data *clsh_d,
				 u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");

	if (mode == CLS_H_NORMAL) {
		dev_err(codec->dev, "%s: Normal mode not applicable for hph_l\n",
			__func__);
		return;
	}

	if (is_enable) {
		if (mode != CLS_AB) {
			wcd_enable_clsh_block(codec, clsh_d, true);
			/*
			 * These K1 values depend on the Headphone Impedance
			 * For now it is assumed to be 16 ohm
			 */
			snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLSH_K1_MSB,
					    0x0F, 0x00);
			snd_soc_update_bits(codec, WCD9XXX_A_CDC_CLSH_K1_LSB,
					    0xFF, 0xC0);
			snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX1_RX_PATH_CFG0,
					    0x40, 0x40);
		}
		wcd_clsh_set_buck_regulator_mode(codec, mode);
		wcd_clsh_set_flyback_mode(codec, mode);
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_flyback_current(codec, mode);
		wcd_clsh_set_buck_mode(codec, mode);
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_hph_mode(codec, mode);
		wcd_clsh_set_gain_path(codec, mode);
	} else {
		wcd_clsh_set_hph_mode(codec, CLS_H_NORMAL);

		if (mode != CLS_AB) {
			snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX1_RX_PATH_CFG0,
					    0x40, 0x00);
			wcd_enable_clsh_block(codec, clsh_d, false);
		}
		/* set buck and flyback to Default Mode */
		wcd_clsh_buck_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_flyback_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_set_flyback_mode(codec, CLS_H_NORMAL);
		wcd_clsh_set_buck_mode(codec, CLS_H_NORMAL);
		wcd_clsh_set_buck_regulator_mode(codec, CLS_H_NORMAL);
	}
}

static void wcd_clsh_state_ear(struct snd_soc_codec *codec,
		struct wcd_clsh_cdc_data *clsh_d,
		u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");

	if (mode != CLS_H_NORMAL) {
		dev_err(codec->dev, "%s: mode: %s cannot be used for EAR\n",
			__func__, mode_to_str(mode));
		return;
	}

	if (is_enable) {
		wcd_enable_clsh_block(codec, clsh_d, true);
		snd_soc_update_bits(codec,
				    WCD9XXX_A_CDC_RX0_RX_PATH_CFG0,
				    0x40, 0x40);
		wcd_clsh_set_buck_mode(codec, mode);
		wcd_clsh_set_flyback_mode(codec, mode);
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_flyback_current(codec, mode);
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, true);
	} else {
		snd_soc_update_bits(codec,
				    WCD9XXX_A_CDC_RX0_RX_PATH_CFG0,
				    0x40, 0x00);
		wcd_enable_clsh_block(codec, clsh_d, false);
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, false);
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, false);
		wcd_clsh_set_flyback_mode(codec, CLS_H_NORMAL);
		wcd_clsh_set_buck_mode(codec, CLS_H_NORMAL);
	}
}

static void wcd_clsh_state_err(struct snd_soc_codec *codec,
		struct wcd_clsh_cdc_data *clsh_d,
		u8 req_state, bool is_enable, int mode)
{
	char msg[128];

	dev_err(codec->dev,
		"%s Wrong request for class H state machine requested to %s %s",
		__func__, is_enable ? "enable" : "disable",
		state_to_str(req_state, msg, sizeof(msg)));
	WARN_ON(1);
}

/*
 * Function: wcd_clsh_is_state_valid
 * Params: state
 * Description:
 * Provides information on valid states of Class H configuration
 */
static bool wcd_clsh_is_state_valid(u8 state)
{
	switch (state) {
	case WCD_CLSH_STATE_IDLE:
	case WCD_CLSH_STATE_EAR:
	case WCD_CLSH_STATE_HPHL:
	case WCD_CLSH_STATE_HPHR:
	case WCD_CLSH_STATE_HPH_ST:
	case WCD_CLSH_STATE_LO:
	case WCD_CLSH_STATE_HPHL_EAR:
	case WCD_CLSH_STATE_HPHR_EAR:
	case WCD_CLSH_STATE_HPH_ST_EAR:
	case WCD_CLSH_STATE_HPHL_LO:
	case WCD_CLSH_STATE_HPHR_LO:
	case WCD_CLSH_STATE_HPH_ST_LO:
		return true;
	default:
		return false;
	};
}

/*
 * Function: wcd_clsh_fsm
 * Params: codec, cdc_clsh_d, req_state, req_type, clsh_event
 * Description:
 * This function handles PRE DAC and POST DAC conditions of different devices
 * and updates class H configuration of different combination of devices
 * based on validity of their states. cdc_clsh_d will contain current
 * class h state information
 */
void wcd_clsh_fsm(struct snd_soc_codec *codec,
		struct wcd_clsh_cdc_data *cdc_clsh_d,
		u8 clsh_event, u8 req_state,
		int int_mode)
{
	u8 old_state, new_state;
	char msg0[128], msg1[128];

	switch (clsh_event) {
	case WCD_CLSH_EVENT_PRE_DAC:
		old_state = cdc_clsh_d->state;
		new_state = old_state | req_state;

		if (!wcd_clsh_is_state_valid(new_state)) {
			dev_err(codec->dev,
				"%s: Class-H not a valid new state: %s\n",
				__func__,
				state_to_str(new_state, msg0, sizeof(msg0)));
			return;
		}
		if (new_state == old_state) {
			dev_err(codec->dev,
				"%s: Class-H already in requested state: %s\n",
				__func__,
				state_to_str(new_state, msg0, sizeof(msg0)));
			return;
		}
		cdc_clsh_d->state = new_state;
		wcd_clsh_set_int_mode(cdc_clsh_d, req_state, int_mode);
		(*clsh_state_fp[new_state]) (codec, cdc_clsh_d, req_state,
					     CLSH_REQ_ENABLE, int_mode);
		dev_dbg(codec->dev,
			"%s: ClassH state transition from %s to %s\n",
			__func__, state_to_str(old_state, msg0, sizeof(msg0)),
			state_to_str(cdc_clsh_d->state, msg1, sizeof(msg1)));
		break;
	case WCD_CLSH_EVENT_POST_PA:
		old_state = cdc_clsh_d->state;
		new_state = old_state & (~req_state);
		if (new_state < NUM_CLSH_STATES_V2) {
			if (!wcd_clsh_is_state_valid(old_state)) {
				dev_err(codec->dev,
					"%s:Invalid old state:%s\n",
					__func__,
					state_to_str(old_state, msg0,
						     sizeof(msg0)));
				return;
			}
			if (new_state == old_state) {
				dev_err(codec->dev,
					"%s: Class-H already in requested state: %s\n",
					__func__,
					state_to_str(new_state, msg0,
						     sizeof(msg0)));
				return;
			}
			(*clsh_state_fp[old_state]) (codec, cdc_clsh_d,
					req_state, CLSH_REQ_DISABLE,
					int_mode);
			cdc_clsh_d->state = new_state;
			wcd_clsh_set_int_mode(cdc_clsh_d, req_state, CLS_NONE);
			dev_dbg(codec->dev, "%s: ClassH state transition from %s to %s\n",
				__func__, state_to_str(old_state, msg0,
						       sizeof(msg0)),
				state_to_str(cdc_clsh_d->state, msg1,
					     sizeof(msg1)));
		}
		break;
	};
}

int wcd_clsh_get_clsh_state(struct wcd_clsh_cdc_data *clsh)
{
	return clsh->state;
}
EXPORT_SYMBOL(wcd_clsh_get_clsh_state);

void wcd_clsh_init(struct wcd_clsh_cdc_data *clsh)
{
	int i;
	clsh->state = WCD_CLSH_STATE_IDLE;

	for (i = 0; i < NUM_CLSH_STATES_V2; i++)
		clsh_state_fp[i] = wcd_clsh_state_err;

	clsh_state_fp[WCD_CLSH_STATE_EAR] = wcd_clsh_state_ear;
	clsh_state_fp[WCD_CLSH_STATE_HPHL] =
						wcd_clsh_state_hph_l;
	clsh_state_fp[WCD_CLSH_STATE_HPHR] =
						wcd_clsh_state_hph_r;
	clsh_state_fp[WCD_CLSH_STATE_HPH_ST] =
						wcd_clsh_state_hph_st;
	clsh_state_fp[WCD_CLSH_STATE_LO] = wcd_clsh_state_lo;
	clsh_state_fp[WCD_CLSH_STATE_HPHL_EAR] =
						wcd_clsh_state_hph_ear;
	clsh_state_fp[WCD_CLSH_STATE_HPHR_EAR] =
						wcd_clsh_state_hph_ear;
	clsh_state_fp[WCD_CLSH_STATE_HPH_ST_EAR] =
						wcd_clsh_state_hph_ear;
	clsh_state_fp[WCD_CLSH_STATE_HPHL_LO] = wcd_clsh_state_hph_lo;
	clsh_state_fp[WCD_CLSH_STATE_HPHR_LO] = wcd_clsh_state_hph_lo;
	clsh_state_fp[WCD_CLSH_STATE_HPH_ST_LO] =
						wcd_clsh_state_hph_lo;
	clsh_state_fp[WCD_CLSH_STATE_EAR_LO] = wcd_clsh_state_ear_lo;
	/* Set interpolaotr modes to NONE */
	wcd_clsh_set_int_mode(clsh, WCD_CLSH_STATE_EAR, CLS_NONE);
	wcd_clsh_set_int_mode(clsh, WCD_CLSH_STATE_HPHL, CLS_NONE);
	wcd_clsh_set_int_mode(clsh, WCD_CLSH_STATE_HPHR, CLS_NONE);
	wcd_clsh_set_int_mode(clsh, WCD_CLSH_STATE_LO, CLS_NONE);
	clsh->flyback_users = 0;
	clsh->buck_users = 0;
	clsh->clsh_users = 0;
}
EXPORT_SYMBOL(wcd_clsh_init);

MODULE_DESCRIPTION("WCD9XXX Common Driver");
MODULE_LICENSE("GPL v2");
