/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/delay.h>
#include <sound/soc.h>
#include "aqt1000-registers.h"
#include "aqt1000-clsh.h"

#define AQT_USLEEP_RANGE 50
#define MAX_IMPED_PARAMS 6

enum aqt_vref_dac_sel {
	VREF_N1P9V = 0,
	VREF_N1P86V,
	VREF_N181V,
	VREF_N1P74V,
	VREF_N1P7V,
	VREF_N0P9V,
	VREF_N1P576V,
	VREF_N1P827V,
};

enum aqt_vref_ctl {
	CONTROLLER = 0,
	I2C,
};

enum aqt_hd2_res_div_ctl {
	DISCONNECT = 0,
	P5_0P35,
	P75_0P68,
	P82_0P77,
	P9_0P87,
};

enum aqt_curr_bias_err_amp {
	I_0P25UA = 0,
	I_0P5UA,
	I_0P75UA,
	I_1UA,
	I_1P25UA,
	I_1P5UA,
	I_1P75UA,
	I_2UA,
};

static const struct aqt_reg_mask_val imped_table_aqt[][MAX_IMPED_PARAMS] = {
	{
		{AQT1000_CDC_RX1_RX_VOL_CTL, 0xff, 0xf2},
		{AQT1000_CDC_RX1_RX_VOL_MIX_CTL, 0xff, 0xf2},
		{AQT1000_CDC_RX1_RX_PATH_SEC1, 0x01, 0x00},
		{AQT1000_CDC_RX2_RX_VOL_CTL, 0xff, 0xf2},
		{AQT1000_CDC_RX2_RX_VOL_MIX_CTL, 0xff, 0xf2},
		{AQT1000_CDC_RX2_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{AQT1000_CDC_RX1_RX_VOL_CTL, 0xff, 0xf4},
		{AQT1000_CDC_RX1_RX_VOL_MIX_CTL, 0xff, 0xf4},
		{AQT1000_CDC_RX1_RX_PATH_SEC1, 0x01, 0x00},
		{AQT1000_CDC_RX2_RX_VOL_CTL, 0xff, 0xf4},
		{AQT1000_CDC_RX2_RX_VOL_MIX_CTL, 0xff, 0xf4},
		{AQT1000_CDC_RX2_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{AQT1000_CDC_RX1_RX_VOL_CTL, 0xff, 0xf7},
		{AQT1000_CDC_RX1_RX_VOL_MIX_CTL, 0xff, 0xf7},
		{AQT1000_CDC_RX1_RX_PATH_SEC1, 0x01, 0x01},
		{AQT1000_CDC_RX2_RX_VOL_CTL, 0xff, 0xf7},
		{AQT1000_CDC_RX2_RX_VOL_MIX_CTL, 0xff, 0xf7},
		{AQT1000_CDC_RX2_RX_PATH_SEC1, 0x01, 0x01},
	},
	{
		{AQT1000_CDC_RX1_RX_VOL_CTL, 0xff, 0xf9},
		{AQT1000_CDC_RX1_RX_VOL_MIX_CTL, 0xff, 0xf9},
		{AQT1000_CDC_RX1_RX_PATH_SEC1, 0x01, 0x00},
		{AQT1000_CDC_RX2_RX_VOL_CTL, 0xff, 0xf9},
		{AQT1000_CDC_RX2_RX_VOL_MIX_CTL, 0xff, 0xf9},
		{AQT1000_CDC_RX2_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{AQT1000_CDC_RX1_RX_VOL_CTL, 0xff, 0xfa},
		{AQT1000_CDC_RX1_RX_VOL_MIX_CTL, 0xff, 0xfa},
		{AQT1000_CDC_RX1_RX_PATH_SEC1, 0x01, 0x00},
		{AQT1000_CDC_RX2_RX_VOL_CTL, 0xff, 0xfa},
		{AQT1000_CDC_RX2_RX_VOL_MIX_CTL, 0xff, 0xfa},
		{AQT1000_CDC_RX2_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{AQT1000_CDC_RX1_RX_VOL_CTL, 0xff, 0xfb},
		{AQT1000_CDC_RX1_RX_VOL_MIX_CTL, 0xff, 0xfb},
		{AQT1000_CDC_RX1_RX_PATH_SEC1, 0x01, 0x00},
		{AQT1000_CDC_RX2_RX_VOL_CTL, 0xff, 0xfb},
		{AQT1000_CDC_RX2_RX_VOL_MIX_CTL, 0xff, 0xfb},
		{AQT1000_CDC_RX2_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{AQT1000_CDC_RX1_RX_VOL_CTL, 0xff, 0xfc},
		{AQT1000_CDC_RX1_RX_VOL_MIX_CTL, 0xff, 0xfc},
		{AQT1000_CDC_RX1_RX_PATH_SEC1, 0x01, 0x00},
		{AQT1000_CDC_RX2_RX_VOL_CTL, 0xff, 0xfc},
		{AQT1000_CDC_RX2_RX_VOL_MIX_CTL, 0xff, 0xfc},
		{AQT1000_CDC_RX2_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{AQT1000_CDC_RX1_RX_VOL_CTL, 0xff, 0xfd},
		{AQT1000_CDC_RX1_RX_VOL_MIX_CTL, 0xff, 0xfd},
		{AQT1000_CDC_RX1_RX_PATH_SEC1, 0x01, 0x00},
		{AQT1000_CDC_RX2_RX_VOL_CTL, 0xff, 0xfd},
		{AQT1000_CDC_RX2_RX_VOL_MIX_CTL, 0xff, 0xfd},
		{AQT1000_CDC_RX2_RX_PATH_SEC1, 0x01, 0x00},
	},
	{
		{AQT1000_CDC_RX1_RX_VOL_CTL, 0xff, 0xfd},
		{AQT1000_CDC_RX1_RX_VOL_MIX_CTL, 0xff, 0xfd},
		{AQT1000_CDC_RX1_RX_PATH_SEC1, 0x01, 0x01},
		{AQT1000_CDC_RX2_RX_VOL_CTL, 0xff, 0xfd},
		{AQT1000_CDC_RX2_RX_VOL_MIX_CTL, 0xff, 0xfd},
		{AQT1000_CDC_RX2_RX_PATH_SEC1, 0x01, 0x01},
	},
};

static const struct aqt_imped_val imped_index[] = {
	{4, 0},
	{5, 1},
	{6, 2},
	{7, 3},
	{8, 4},
	{9, 5},
	{10, 6},
	{11, 7},
	{12, 8},
	{13, 9},
};

static void (*clsh_state_fp[NUM_CLSH_STATES])(struct snd_soc_codec *,
					      struct aqt_clsh_cdc_data *,
					      u8 req_state, bool en, int mode);

static int get_impedance_index(int imped)
{
	int i = 0;

	if (imped < imped_index[i].imped_val) {
		pr_debug("%s, detected impedance is less than 4 Ohm\n",
				__func__);
		i = 0;
		goto ret;
	}
	if (imped >= imped_index[ARRAY_SIZE(imped_index) - 1].imped_val) {
		pr_debug("%s, detected impedance is greater than 12 Ohm\n",
				__func__);
		i = ARRAY_SIZE(imped_index) - 1;
		goto ret;
	}
	for (i = 0; i < ARRAY_SIZE(imped_index) - 1; i++) {
		if (imped >= imped_index[i].imped_val &&
			imped < imped_index[i + 1].imped_val)
			break;
	}
ret:
	pr_debug("%s: selected impedance index = %d\n",
			__func__, imped_index[i].index);
	return imped_index[i].index;
}

/*
 * Function: aqt_clsh_imped_config
 * Params: codec, imped, reset
 * Description:
 * This function updates HPHL and HPHR gain settings
 * according to the impedance value.
 */
void aqt_clsh_imped_config(struct snd_soc_codec *codec, int imped, bool reset)
{
	int i;
	int index = 0;
	int table_size;

	static const struct aqt_reg_mask_val
				(*imped_table_ptr)[MAX_IMPED_PARAMS];

	table_size = ARRAY_SIZE(imped_table_aqt);
	imped_table_ptr = imped_table_aqt;

	/* reset = 1, which means request is to reset the register values */
	if (reset) {
		for (i = 0; i < MAX_IMPED_PARAMS; i++)
			snd_soc_update_bits(codec,
				imped_table_ptr[index][i].reg,
				imped_table_ptr[index][i].mask, 0);
		return;
	}
	index = get_impedance_index(imped);
	if (index >= (ARRAY_SIZE(imped_index) - 1)) {
		pr_debug("%s, impedance not in range = %d\n", __func__, imped);
		return;
	}
	if (index >= table_size) {
		pr_debug("%s, impedance index not in range = %d\n", __func__,
			index);
		return;
	}
	for (i = 0; i < MAX_IMPED_PARAMS; i++)
		snd_soc_update_bits(codec,
				imped_table_ptr[index][i].reg,
				imped_table_ptr[index][i].mask,
				imped_table_ptr[index][i].val);
}
EXPORT_SYMBOL(aqt_clsh_imped_config);

static const char *mode_to_str(int mode)
{
	switch (mode) {
	case CLS_H_NORMAL:
		return "CLS_H_NORMAL";
	case CLS_H_HIFI:
		return "CLS_H_HIFI";
	case CLS_H_LOHIFI:
		return "CLS_H_LOHIFI";
	case CLS_H_LP:
		return "CLS_H_LP";
	case CLS_H_ULP:
		return "CLS_H_ULP";
	case CLS_AB:
		return "CLS_AB";
	case CLS_AB_HIFI:
		return "CLS_AB_HIFI";
	default:
		return "CLS_H_INVALID";
	};
}

static const char *const state_to_str[] = {
	[AQT_CLSH_STATE_IDLE] = "STATE_IDLE",
	[AQT_CLSH_STATE_HPHL] = "STATE_HPH_L",
	[AQT_CLSH_STATE_HPHR] = "STATE_HPH_R",
	[AQT_CLSH_STATE_HPH_ST] = "STATE_HPH_ST",
};

static inline void
aqt_enable_clsh_block(struct snd_soc_codec *codec,
		      struct aqt_clsh_cdc_data *clsh_d, bool enable)
{
	if ((enable && ++clsh_d->clsh_users == 1) ||
	    (!enable && --clsh_d->clsh_users == 0))
		snd_soc_update_bits(codec, AQT1000_CDC_CLSH_CRC, 0x01,
				    (u8) enable);
	if (clsh_d->clsh_users < 0)
		clsh_d->clsh_users = 0;
	dev_dbg(codec->dev, "%s: clsh_users %d, enable %d", __func__,
		clsh_d->clsh_users, enable);
}

static inline bool aqt_clsh_enable_status(struct snd_soc_codec *codec)
{
	return snd_soc_read(codec, AQT1000_CDC_CLSH_CRC) & 0x01;
}

static inline int aqt_clsh_get_int_mode(struct aqt_clsh_cdc_data *clsh_d,
					int clsh_state)
{
	int mode;

	if ((clsh_state != AQT_CLSH_STATE_HPHL) &&
	    (clsh_state != AQT_CLSH_STATE_HPHR))
		mode = CLS_NONE;
	else
		mode = clsh_d->interpolator_modes[ffs(clsh_state)];

	return mode;
}

static inline void aqt_clsh_set_int_mode(struct aqt_clsh_cdc_data *clsh_d,
					int clsh_state, int mode)
{
	if ((clsh_state != AQT_CLSH_STATE_HPHL) &&
	    (clsh_state != AQT_CLSH_STATE_HPHR))
		return;

	clsh_d->interpolator_modes[ffs(clsh_state)] = mode;
}

static inline void aqt_clsh_set_buck_mode(struct snd_soc_codec *codec,
					  int mode)
{
	if (mode == CLS_H_HIFI || mode == CLS_H_LOHIFI ||
	    mode == CLS_AB_HIFI || mode == CLS_AB)
		snd_soc_update_bits(codec, AQT1000_ANA_RX_SUPPLIES,
				    0x08, 0x08); /* set to HIFI */
	else
		snd_soc_update_bits(codec, AQT1000_ANA_RX_SUPPLIES,
				    0x08, 0x00); /* set to default */
}

static inline void aqt_clsh_set_flyback_mode(struct snd_soc_codec *codec,
					     int mode)
{
	if (mode == CLS_H_HIFI || mode == CLS_H_LOHIFI ||
	    mode == CLS_AB_HIFI || mode == CLS_AB)
		snd_soc_update_bits(codec, AQT1000_ANA_RX_SUPPLIES,
				    0x04, 0x04); /* set to HIFI */
	else
		snd_soc_update_bits(codec, AQT1000_ANA_RX_SUPPLIES,
				    0x04, 0x00); /* set to Default */
}

static inline void aqt_clsh_gm3_boost_disable(struct snd_soc_codec *codec,
					      int mode)
{
	if (mode == CLS_H_HIFI || mode == CLS_H_LOHIFI ||
	    mode == CLS_AB_HIFI || mode == CLS_AB) {
		snd_soc_update_bits(codec, AQT1000_HPH_CNP_WG_CTL,
				    0x80, 0x0); /* disable GM3 Boost */
		snd_soc_update_bits(codec, AQT1000_FLYBACK_VNEG_CTRL_4,
				    0xF0, 0x80);
	} else {
		snd_soc_update_bits(codec, AQT1000_HPH_CNP_WG_CTL,
				    0x80, 0x80); /* set to Default */
		snd_soc_update_bits(codec, AQT1000_FLYBACK_VNEG_CTRL_4,
				    0xF0, 0x70);
	}
}

static inline void aqt_clsh_flyback_dac_ctl(struct snd_soc_codec *codec,
					     int vref)
{
	snd_soc_update_bits(codec, AQT1000_FLYBACK_VNEGDAC_CTRL_2,
			    0xE0, (vref << 5));
}

static inline void aqt_clsh_mode_vref_ctl(struct snd_soc_codec *codec,
					   int vref_ctl)
{
	if (vref_ctl == I2C) {
		snd_soc_update_bits(codec, AQT1000_CLASSH_MODE_3, 0x02, 0x02);
		snd_soc_update_bits(codec, AQT1000_CLASSH_MODE_2, 0xFF, 0x1C);
	} else {
		snd_soc_update_bits(codec, AQT1000_CLASSH_MODE_2, 0xFF, 0x3A);
		snd_soc_update_bits(codec, AQT1000_CLASSH_MODE_3, 0x02, 0x00);
	}
}

static inline void aqt_clsh_buck_current_bias_ctl(struct snd_soc_codec *codec,
						  bool enable)
{
	if (enable) {
		snd_soc_update_bits(codec, AQT1000_BUCK_5V_IBIAS_CTL_4,
				    0x70, (I_2UA << 4));
		snd_soc_update_bits(codec, AQT1000_BUCK_5V_IBIAS_CTL_4,
				    0x07, I_0P25UA);
		snd_soc_update_bits(codec, AQT1000_BUCK_5V_CTRL_CCL_2,
				    0x3F, 0x3F);
	} else {
		snd_soc_update_bits(codec, AQT1000_BUCK_5V_IBIAS_CTL_4,
				    0x70, (I_1UA << 4));
		snd_soc_update_bits(codec, AQT1000_BUCK_5V_IBIAS_CTL_4,
				    0x07, I_1UA);
		snd_soc_update_bits(codec, AQT1000_BUCK_5V_CTRL_CCL_2,
				    0x3F, 0x20);
	}
}

static inline void aqt_clsh_rdac_hd2_ctl(struct snd_soc_codec *codec,
					 u8 hd2_div_ctl, u8 state)
{
	u16 reg = 0;

	if (state == AQT_CLSH_STATE_HPHL)
		reg = AQT1000_HPH_NEW_INT_RDAC_HD2_CTL_L;
	else if (state == AQT_CLSH_STATE_HPHR)
		reg = AQT1000_HPH_NEW_INT_RDAC_HD2_CTL_R;
	else
		dev_err(codec->dev, "%s: Invalid state: %d\n",
			__func__, state);
	if (!reg)
		snd_soc_update_bits(codec, reg, 0x0F, hd2_div_ctl);
}

static inline void aqt_clsh_force_iq_ctl(struct snd_soc_codec *codec,
					 int mode)
{
	if (mode == CLS_H_LOHIFI || mode == CLS_AB) {
		snd_soc_update_bits(codec, AQT1000_HPH_NEW_INT_PA_MISC2,
				    0x20, 0x20);
		snd_soc_update_bits(codec, AQT1000_RX_BIAS_HPH_LOWPOWER,
				    0xF0, 0xC0);
		snd_soc_update_bits(codec, AQT1000_HPH_PA_CTL1,
				    0x0E, 0x02);
	} else {

		snd_soc_update_bits(codec, AQT1000_HPH_NEW_INT_PA_MISC2,
				    0x20, 0x0);
		snd_soc_update_bits(codec, AQT1000_RX_BIAS_HPH_LOWPOWER,
				    0xF0, 0x80);
		snd_soc_update_bits(codec, AQT1000_HPH_PA_CTL1,
				    0x0E, 0x06);
	}
}

static void aqt_clsh_buck_ctrl(struct snd_soc_codec *codec,
			       struct aqt_clsh_cdc_data *clsh_d,
			       int mode,
			       bool enable)
{
	/* enable/disable buck */
	if ((enable && (++clsh_d->buck_users == 1)) ||
	   (!enable && (--clsh_d->buck_users == 0)))
		snd_soc_update_bits(codec, AQT1000_ANA_RX_SUPPLIES,
				    (1 << 7), (enable << 7));
	dev_dbg(codec->dev, "%s: buck_users %d, enable %d, mode: %s",
		__func__, clsh_d->buck_users, enable, mode_to_str(mode));
	/*
	 * 500us sleep is required after buck enable/disable
	 * as per HW requirement
	 */
	usleep_range(500, 500 + AQT_USLEEP_RANGE);
}

static void aqt_clsh_flyback_ctrl(struct snd_soc_codec *codec,
				  struct aqt_clsh_cdc_data *clsh_d,
				  int mode,
				  bool enable)
{
	/* enable/disable flyback */
	if ((enable && (++clsh_d->flyback_users == 1)) ||
	   (!enable && (--clsh_d->flyback_users == 0))) {
		snd_soc_update_bits(codec, AQT1000_ANA_RX_SUPPLIES,
				    (1 << 6), (enable << 6));
		/* 100usec delay is needed as per HW requirement */
		usleep_range(100, 110);
	}
	dev_dbg(codec->dev, "%s: flyback_users %d, enable %d, mode: %s",
		__func__, clsh_d->flyback_users, enable, mode_to_str(mode));
	/*
	 * 500us sleep is required after flyback enable/disable
	 * as per HW requirement
	 */
	usleep_range(500, 500 + AQT_USLEEP_RANGE);
}

static void aqt_clsh_set_hph_mode(struct snd_soc_codec *codec,
				  int mode)
{
	u8 val = 0;
	u8 gain = 0;
	u8 res_val = VREF_FILT_R_0OHM;
	u8 ipeak = DELTA_I_50MA;

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
	case CLS_AB_HIFI:
		val = 0x08;
		break;
	case CLS_H_HIFI:
		val = 0x08;
		gain = DAC_GAIN_M0P2DB;
		ipeak = DELTA_I_50MA;
		break;
	case CLS_H_LOHIFI:
		val = 0x00;
		break;
	case CLS_H_ULP:
		val = 0x0C;
		break;
	case CLS_H_LP:
		val = 0x04;
		ipeak = DELTA_I_30MA;
		break;
	default:
		return;
	};

	if (mode == CLS_H_LOHIFI || mode == CLS_AB)
		val = 0x04;

	snd_soc_update_bits(codec, AQT1000_ANA_HPH, 0x0C, val);
}

static void aqt_clsh_set_buck_regulator_mode(struct snd_soc_codec *codec,
					     int mode)
{
	snd_soc_update_bits(codec, AQT1000_ANA_RX_SUPPLIES,
			    0x02, 0x00);
}

static void aqt_clsh_state_hph_st(struct snd_soc_codec *codec,
				  struct aqt_clsh_cdc_data *clsh_d,
				  u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");

	if (mode == CLS_AB || mode == CLS_AB_HIFI)
		return;

	if (is_enable) {
		if (req_state == AQT_CLSH_STATE_HPHL)
			snd_soc_update_bits(codec,
					    AQT1000_CDC_RX1_RX_PATH_CFG0,
					    0x40, 0x40);
		if (req_state == AQT_CLSH_STATE_HPHR)
			snd_soc_update_bits(codec,
					    AQT1000_CDC_RX2_RX_PATH_CFG0,
					    0x40, 0x40);
	} else {
		if (req_state == AQT_CLSH_STATE_HPHL)
			snd_soc_update_bits(codec,
					    AQT1000_CDC_RX1_RX_PATH_CFG0,
					    0x40, 0x00);
		if (req_state == AQT_CLSH_STATE_HPHR)
			snd_soc_update_bits(codec,
					    AQT1000_CDC_RX2_RX_PATH_CFG0,
					    0x40, 0x00);
	}
}

static void aqt_clsh_state_hph_r(struct snd_soc_codec *codec,
				 struct aqt_clsh_cdc_data *clsh_d,
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
		if (mode != CLS_AB && mode != CLS_AB_HIFI) {
			aqt_enable_clsh_block(codec, clsh_d, true);
			/*
			 * These K1 values depend on the Headphone Impedance
			 * For now it is assumed to be 16 ohm
			 */
			snd_soc_update_bits(codec, AQT1000_CDC_CLSH_K1_MSB,
					    0x0F, 0x00);
			snd_soc_update_bits(codec, AQT1000_CDC_CLSH_K1_LSB,
					    0xFF, 0xC0);
			snd_soc_update_bits(codec,
					    AQT1000_CDC_RX2_RX_PATH_CFG0,
					    0x40, 0x40);
		}
		aqt_clsh_set_buck_regulator_mode(codec, mode);
		aqt_clsh_set_flyback_mode(codec, mode);
		aqt_clsh_gm3_boost_disable(codec, mode);
		aqt_clsh_flyback_dac_ctl(codec, VREF_N0P9V);
		aqt_clsh_mode_vref_ctl(codec, I2C);
		aqt_clsh_force_iq_ctl(codec, mode);
		aqt_clsh_rdac_hd2_ctl(codec, P82_0P77, req_state);
		aqt_clsh_flyback_ctrl(codec, clsh_d, mode, true);
		aqt_clsh_flyback_dac_ctl(codec, VREF_N1P827V);
		aqt_clsh_set_buck_mode(codec, mode);
		aqt_clsh_buck_ctrl(codec, clsh_d, mode, true);
		aqt_clsh_mode_vref_ctl(codec, CONTROLLER);
		aqt_clsh_buck_current_bias_ctl(codec, true);
		aqt_clsh_set_hph_mode(codec, mode);
	} else {
		aqt_clsh_set_hph_mode(codec, CLS_H_NORMAL);
		aqt_clsh_buck_current_bias_ctl(codec, false);

		if (mode != CLS_AB && mode != CLS_AB_HIFI) {
			snd_soc_update_bits(codec,
					    AQT1000_CDC_RX2_RX_PATH_CFG0,
					    0x40, 0x00);
			aqt_enable_clsh_block(codec, clsh_d, false);
		}
		/* buck and flyback set to default mode and disable */
		aqt_clsh_buck_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		aqt_clsh_flyback_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		aqt_clsh_rdac_hd2_ctl(codec, P5_0P35, req_state);
		aqt_clsh_force_iq_ctl(codec, CLS_H_NORMAL);
		aqt_clsh_gm3_boost_disable(codec, CLS_H_NORMAL);
		aqt_clsh_set_flyback_mode(codec, CLS_H_NORMAL);
		aqt_clsh_set_buck_mode(codec, CLS_H_NORMAL);
		aqt_clsh_set_buck_regulator_mode(codec, CLS_H_NORMAL);
	}
}

static void aqt_clsh_state_hph_l(struct snd_soc_codec *codec,
				 struct aqt_clsh_cdc_data *clsh_d,
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
		if (mode != CLS_AB && mode != CLS_AB_HIFI) {
			aqt_enable_clsh_block(codec, clsh_d, true);
			/*
			 * These K1 values depend on the Headphone Impedance
			 * For now it is assumed to be 16 ohm
			 */
			snd_soc_update_bits(codec, AQT1000_CDC_CLSH_K1_MSB,
					    0x0F, 0x00);
			snd_soc_update_bits(codec, AQT1000_CDC_CLSH_K1_LSB,
					    0xFF, 0xC0);
			snd_soc_update_bits(codec,
					    AQT1000_CDC_RX1_RX_PATH_CFG0,
					    0x40, 0x40);
		}
		aqt_clsh_set_buck_regulator_mode(codec, mode);
		aqt_clsh_set_flyback_mode(codec, mode);
		aqt_clsh_gm3_boost_disable(codec, mode);
		aqt_clsh_flyback_dac_ctl(codec, VREF_N0P9V);
		aqt_clsh_mode_vref_ctl(codec, I2C);
		aqt_clsh_force_iq_ctl(codec, mode);
		aqt_clsh_rdac_hd2_ctl(codec, P82_0P77, req_state);
		aqt_clsh_flyback_ctrl(codec, clsh_d, mode, true);
		aqt_clsh_flyback_dac_ctl(codec, VREF_N1P827V);
		aqt_clsh_set_buck_mode(codec, mode);
		aqt_clsh_buck_ctrl(codec, clsh_d, mode, true);
		aqt_clsh_mode_vref_ctl(codec, CONTROLLER);
		aqt_clsh_buck_current_bias_ctl(codec, true);
		aqt_clsh_set_hph_mode(codec, mode);
	} else {
		aqt_clsh_set_hph_mode(codec, CLS_H_NORMAL);
		aqt_clsh_buck_current_bias_ctl(codec, false);

		if (mode != CLS_AB && mode != CLS_AB_HIFI) {
			snd_soc_update_bits(codec,
					    AQT1000_CDC_RX1_RX_PATH_CFG0,
					    0x40, 0x00);
			aqt_enable_clsh_block(codec, clsh_d, false);
		}
		/* set buck and flyback to Default Mode */
		aqt_clsh_buck_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		aqt_clsh_flyback_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		aqt_clsh_rdac_hd2_ctl(codec, P5_0P35, req_state);
		aqt_clsh_force_iq_ctl(codec, CLS_H_NORMAL);
		aqt_clsh_gm3_boost_disable(codec, CLS_H_NORMAL);
		aqt_clsh_set_flyback_mode(codec, CLS_H_NORMAL);
		aqt_clsh_set_buck_mode(codec, CLS_H_NORMAL);
		aqt_clsh_set_buck_regulator_mode(codec, CLS_H_NORMAL);
	}
}

static void aqt_clsh_state_err(struct snd_soc_codec *codec,
		struct aqt_clsh_cdc_data *clsh_d,
		u8 req_state, bool is_enable, int mode)
{
	dev_err(codec->dev,
		"%s Wrong request for class H state machine requested to %s %s",
		__func__, is_enable ? "enable" : "disable",
		state_to_str[req_state]);
}

/*
 * Function: aqt_clsh_is_state_valid
 * Params: state
 * Description:
 * Provides information on valid states of Class H configuration
 */
static bool aqt_clsh_is_state_valid(u8 state)
{
	switch (state) {
	case AQT_CLSH_STATE_IDLE:
	case AQT_CLSH_STATE_HPHL:
	case AQT_CLSH_STATE_HPHR:
	case AQT_CLSH_STATE_HPH_ST:
		return true;
	default:
		return false;
	};
}

/*
 * Function: aqt_clsh_fsm
 * Params: codec, cdc_clsh_d, req_state, req_type, clsh_event
 * Description:
 * This function handles PRE DAC and POST DAC conditions of different devices
 * and updates class H configuration of different combination of devices
 * based on validity of their states. cdc_clsh_d will contain current
 * class h state information
 */
void aqt_clsh_fsm(struct snd_soc_codec *codec,
		struct aqt_clsh_cdc_data *cdc_clsh_d,
		u8 clsh_event, u8 req_state,
		int int_mode)
{
	u8 old_state, new_state;

	switch (clsh_event) {
	case AQT_CLSH_EVENT_PRE_DAC:
		old_state = cdc_clsh_d->state;
		new_state = old_state | req_state;

		if (!aqt_clsh_is_state_valid(new_state)) {
			dev_err(codec->dev,
				"%s: Class-H not a valid new state: %s\n",
				__func__, state_to_str[new_state]);
			return;
		}
		if (new_state == old_state) {
			dev_err(codec->dev,
				"%s: Class-H already in requested state: %s\n",
				__func__, state_to_str[new_state]);
			return;
		}
		cdc_clsh_d->state = new_state;
		aqt_clsh_set_int_mode(cdc_clsh_d, req_state, int_mode);
		(*clsh_state_fp[new_state]) (codec, cdc_clsh_d, req_state,
					     CLSH_REQ_ENABLE, int_mode);
		dev_dbg(codec->dev,
			"%s: ClassH state transition from %s to %s\n",
			__func__, state_to_str[old_state],
			state_to_str[cdc_clsh_d->state]);
		break;
	case AQT_CLSH_EVENT_POST_PA:
		old_state = cdc_clsh_d->state;
		new_state = old_state & (~req_state);
		if (new_state < NUM_CLSH_STATES) {
			if (!aqt_clsh_is_state_valid(old_state)) {
				dev_err(codec->dev,
					"%s:Invalid old state:%s\n",
					__func__, state_to_str[old_state]);
				return;
			}
			if (new_state == old_state) {
				dev_err(codec->dev,
					"%s: Class-H already in requested state: %s\n",
					__func__,state_to_str[new_state]);
				return;
			}
			(*clsh_state_fp[old_state]) (codec, cdc_clsh_d,
					req_state, CLSH_REQ_DISABLE,
					int_mode);
			cdc_clsh_d->state = new_state;
			aqt_clsh_set_int_mode(cdc_clsh_d, req_state, CLS_NONE);
			dev_dbg(codec->dev, "%s: ClassH state transition from %s to %s\n",
				__func__, state_to_str[old_state],
				state_to_str[cdc_clsh_d->state]);
		}
		break;
	};
}
EXPORT_SYMBOL(aqt_clsh_fsm);

/*
 * Function: aqt_clsh_get_clsh_state
 * Params: clsh
 * Description:
 * This function returns the state of the class H controller
 */
int aqt_clsh_get_clsh_state(struct aqt_clsh_cdc_data *clsh)
{
	return clsh->state;
}
EXPORT_SYMBOL(aqt_clsh_get_clsh_state);

/*
 * Function: aqt_clsh_init
 * Params: clsh
 * Description:
 * This function initializes the class H controller
 */
void aqt_clsh_init(struct aqt_clsh_cdc_data *clsh)
{
	int i;

	clsh->state = AQT_CLSH_STATE_IDLE;

	for (i = 0; i < NUM_CLSH_STATES; i++)
		clsh_state_fp[i] = aqt_clsh_state_err;

	clsh_state_fp[AQT_CLSH_STATE_HPHL] = aqt_clsh_state_hph_l;
	clsh_state_fp[AQT_CLSH_STATE_HPHR] = aqt_clsh_state_hph_r;
	clsh_state_fp[AQT_CLSH_STATE_HPH_ST] = aqt_clsh_state_hph_st;
	/* Set interpolator modes to NONE */
	aqt_clsh_set_int_mode(clsh, AQT_CLSH_STATE_HPHL, CLS_NONE);
	aqt_clsh_set_int_mode(clsh, AQT_CLSH_STATE_HPHR, CLS_NONE);
	clsh->flyback_users = 0;
	clsh->buck_users = 0;
	clsh->clsh_users = 0;
}
EXPORT_SYMBOL(aqt_clsh_init);
