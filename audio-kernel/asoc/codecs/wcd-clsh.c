/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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
#include <asoc/wcd9xxx_registers.h>
#include "wcd-clsh.h"

#define WCD_USLEEP_RANGE 50

static void (*clsh_state_fp[NUM_CLSH_STATES])(struct snd_soc_codec *,
					      struct wcd_clsh_cdc_info *,
					      u8 req_state, bool en, int mode);

static const char *mode_to_str(int mode)
{
	switch (mode) {
	case CLS_H_NORMAL:
		return WCD_CLSH_STRINGIFY(CLS_H_NORMAL);
	case CLS_H_HIFI:
		return WCD_CLSH_STRINGIFY(CLS_H_HIFI);
	case CLS_H_LOHIFI:
		return WCD_CLSH_STRINGIFY(CLS_H_LOHIFI);
	case CLS_H_LP:
		return WCD_CLSH_STRINGIFY(CLS_H_LP);
	case CLS_H_ULP:
		return WCD_CLSH_STRINGIFY(CLS_H_ULP);
	case CLS_AB:
		return WCD_CLSH_STRINGIFY(CLS_AB);
	case CLS_AB_HIFI:
		return WCD_CLSH_STRINGIFY(CLS_AB_HIFI);
	default:
		return WCD_CLSH_STRINGIFY(CLS_H_INVALID);
	};
}

static const char *state_to_str(u8 state, char *buf, size_t buflen)
{
	int i;
	int cnt = 0;
	/*
	 * This array of strings should match with enum wcd_clsh_state_bit.
	 */
	static const char *const states[] = {
		"STATE_EAR",
		"STATE_HPH_L",
		"STATE_HPH_R",
		"STATE_AUX",
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

static inline int wcd_clsh_get_int_mode(struct wcd_clsh_cdc_info *clsh_d,
					int clsh_state)
{
	int mode;

	if ((clsh_state != WCD_CLSH_STATE_EAR) &&
	    (clsh_state != WCD_CLSH_STATE_HPHL) &&
	    (clsh_state != WCD_CLSH_STATE_HPHR) &&
	    (clsh_state != WCD_CLSH_STATE_AUX))
		mode = CLS_NONE;
	else
		mode = clsh_d->interpolator_modes[ffs(clsh_state)];

	return mode;
}

static inline void wcd_clsh_set_int_mode(struct wcd_clsh_cdc_info *clsh_d,
					int clsh_state, int mode)
{
	if ((clsh_state != WCD_CLSH_STATE_EAR) &&
	    (clsh_state != WCD_CLSH_STATE_HPHL) &&
	    (clsh_state != WCD_CLSH_STATE_HPHR) &&
	    (clsh_state != WCD_CLSH_STATE_AUX))
		return;

	clsh_d->interpolator_modes[ffs(clsh_state)] = mode;
}

static inline void wcd_clsh_set_buck_mode(struct snd_soc_codec *codec,
					  int mode)
{
	if (mode == CLS_H_HIFI || mode == CLS_H_LOHIFI ||
	    mode == CLS_AB_HIFI)
		snd_soc_update_bits(codec, WCD9XXX_ANA_RX_SUPPLIES,
				    0x08, 0x08); /* set to HIFI */
	else
		snd_soc_update_bits(codec, WCD9XXX_ANA_RX_SUPPLIES,
				    0x08, 0x00); /* set to default */
}

static inline void wcd_clsh_set_flyback_mode(struct snd_soc_codec *codec,
					     int mode)
{
	if (mode == CLS_H_HIFI || mode == CLS_H_LOHIFI ||
	    mode == CLS_AB_HIFI) {
		snd_soc_update_bits(codec, WCD9XXX_ANA_RX_SUPPLIES,
				    0x04, 0x04);
		snd_soc_update_bits(codec, WCD9XXX_FLYBACK_VNEG_CTRL_4,
				    0xF0, 0x80);
	} else {
		snd_soc_update_bits(codec, WCD9XXX_ANA_RX_SUPPLIES,
				    0x04, 0x00); /* set to Default */
		snd_soc_update_bits(codec, WCD9XXX_FLYBACK_VNEG_CTRL_4,
				    0xF0, 0x70);
	}
}

static inline void wcd_clsh_force_iq_ctl(struct snd_soc_codec *codec,
					 int mode, bool enable)
{
	if (enable) {
		snd_soc_update_bits(codec, WCD9XXX_FLYBACK_VNEGDAC_CTRL_2,
				    0xE0, 0xA0);
		/* 100usec delay is needed as per HW requirement */
		usleep_range(100, 110);
		snd_soc_update_bits(codec, WCD9XXX_CLASSH_MODE_3,
				    0x02, 0x02);
		snd_soc_update_bits(codec, WCD9XXX_CLASSH_MODE_2,
				    0xFF, 0x1C);
		if (mode == CLS_H_LOHIFI) {
			snd_soc_update_bits(codec, WCD9XXX_HPH_NEW_INT_PA_MISC2,
					    0x20, 0x20);
			snd_soc_update_bits(codec, WCD9XXX_RX_BIAS_HPH_LOWPOWER,
					    0xF0, 0xC0);
			snd_soc_update_bits(codec, WCD9XXX_HPH_PA_CTL1,
					    0x0E, 0x02);
		}
	} else {
		snd_soc_update_bits(codec, WCD9XXX_HPH_NEW_INT_PA_MISC2,
				    0x20, 0x00);
		snd_soc_update_bits(codec, WCD9XXX_RX_BIAS_HPH_LOWPOWER,
				    0xF0, 0x80);
		snd_soc_update_bits(codec, WCD9XXX_HPH_PA_CTL1,
				    0x0E, 0x06);
	}
}

static void wcd_clsh_buck_ctrl(struct snd_soc_codec *codec,
			       struct wcd_clsh_cdc_info *clsh_d,
			       int mode,
			       bool enable)
{
	/* enable/disable buck */
	if ((enable && (++clsh_d->buck_users == 1)) ||
	   (!enable && (--clsh_d->buck_users == 0))) {
		snd_soc_update_bits(codec, WCD9XXX_ANA_RX_SUPPLIES,
				    (1 << 7), (enable << 7));
		/*
		 * 500us sleep is required after buck enable/disable
		 * as per HW requirement
		 */
		usleep_range(500, 510);
		if (mode == CLS_H_LOHIFI || mode == CLS_H_ULP ||
			mode == CLS_H_HIFI || mode == CLS_H_LP)
			snd_soc_update_bits(codec, WCD9XXX_CLASSH_MODE_3,
					    0x02, 0x00);

		snd_soc_update_bits(codec, WCD9XXX_CLASSH_MODE_2, 0xFF, 0x3A);
		/* 500usec delay is needed as per HW requirement */
		usleep_range(500, 500 + WCD_USLEEP_RANGE);
	}
	dev_dbg(codec->dev, "%s: buck_users %d, enable %d, mode: %s\n",
		__func__, clsh_d->buck_users, enable, mode_to_str(mode));
}

static void wcd_clsh_flyback_ctrl(struct snd_soc_codec *codec,
				  struct wcd_clsh_cdc_info *clsh_d,
				  int mode,
				  bool enable)
{
	/* enable/disable flyback */
	if ((enable && (++clsh_d->flyback_users == 1)) ||
	   (!enable && (--clsh_d->flyback_users == 0))) {
		snd_soc_update_bits(codec, WCD9XXX_FLYBACK_VNEG_CTRL_1,
				    0xE0, 0xE0);
		snd_soc_update_bits(codec, WCD9XXX_ANA_RX_SUPPLIES,
				    (1 << 6), (enable << 6));
		/*
		 * 100us sleep is required after flyback enable/disable
		 * as per HW requirement
		 */
		usleep_range(100, 110);
		snd_soc_update_bits(codec, WCD9XXX_FLYBACK_VNEGDAC_CTRL_2,
			    0xE0, 0xE0);
		/* 500usec delay is needed as per HW requirement */
		usleep_range(500, 500 + WCD_USLEEP_RANGE);
	}
	dev_dbg(codec->dev, "%s: flyback_users %d, enable %d, mode: %s\n",
		__func__, clsh_d->flyback_users, enable, mode_to_str(mode));
}

static void wcd_clsh_set_hph_mode(struct snd_soc_codec *codec,
				  int mode)
{
	u8 val = 0;

	switch (mode) {
	case CLS_H_NORMAL:
		val = 0x00;
		break;
	case CLS_AB:
	case CLS_H_ULP:
		val = 0x0C;
		break;
	case CLS_AB_HIFI:
	case CLS_H_HIFI:
		val = 0x08;
		break;
	case CLS_H_LP:
	case CLS_H_LOHIFI:
		val = 0x04;
		break;
	default:
		dev_err(codec->dev, "%s:Invalid mode %d\n", __func__, mode);
		return;
	};

	snd_soc_update_bits(codec, WCD9XXX_ANA_HPH, 0x0C, val);
}

static void wcd_clsh_set_flyback_current(struct snd_soc_codec *codec, int mode)
{

	snd_soc_update_bits(codec, WCD9XXX_RX_BIAS_FLYB_BUFF, 0x0F, 0x0A);
	snd_soc_update_bits(codec, WCD9XXX_RX_BIAS_FLYB_BUFF, 0xF0, 0xA0);
	/* Sleep needed to avoid click and pop as per HW requirement */
	usleep_range(100, 110);
}

static void wcd_clsh_set_buck_regulator_mode(struct snd_soc_codec *codec,
					     int mode)
{
	snd_soc_update_bits(codec, WCD9XXX_ANA_RX_SUPPLIES,
			    0x02, 0x00);
}

static void wcd_clsh_state_ear_aux(struct snd_soc_codec *codec,
				  struct wcd_clsh_cdc_info *clsh_d,
				  u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");
}

static void wcd_clsh_state_hph_aux(struct snd_soc_codec *codec,
				  struct wcd_clsh_cdc_info *clsh_d,
				  u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");
}

static void wcd_clsh_state_hph_ear(struct snd_soc_codec *codec,
				  struct wcd_clsh_cdc_info *clsh_d,
				  u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");
}

static void wcd_clsh_state_hph_st(struct snd_soc_codec *codec,
				  struct wcd_clsh_cdc_info *clsh_d,
				  u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");
}

static void wcd_clsh_state_hph_r(struct snd_soc_codec *codec,
				 struct wcd_clsh_cdc_info *clsh_d,
				 u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");

	if (mode == CLS_H_NORMAL) {
		dev_dbg(codec->dev, "%s: Normal mode not applicable for hph_r\n",
			__func__);
		return;
	}

	if (is_enable) {
		wcd_clsh_set_buck_regulator_mode(codec, mode);
		wcd_clsh_set_flyback_mode(codec, mode);
		wcd_clsh_force_iq_ctl(codec, mode, true);
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_flyback_current(codec, mode);
		wcd_clsh_set_buck_mode(codec, mode);
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_hph_mode(codec, mode);
	} else {
		wcd_clsh_set_hph_mode(codec, CLS_H_NORMAL);

		/* buck and flyback set to default mode and disable */
		wcd_clsh_flyback_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_buck_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_force_iq_ctl(codec, CLS_H_NORMAL, false);
		wcd_clsh_set_flyback_mode(codec, CLS_H_NORMAL);
		wcd_clsh_set_buck_mode(codec, CLS_H_NORMAL);
	}
}

static void wcd_clsh_state_hph_l(struct snd_soc_codec *codec,
				 struct wcd_clsh_cdc_info *clsh_d,
				 u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");

	if (mode == CLS_H_NORMAL) {
		dev_dbg(codec->dev, "%s: Normal mode not applicable for hph_l\n",
			__func__);
		return;
	}

	if (is_enable) {
		wcd_clsh_set_buck_regulator_mode(codec, mode);
		wcd_clsh_set_flyback_mode(codec, mode);
		wcd_clsh_force_iq_ctl(codec, mode, true);
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_flyback_current(codec, mode);
		wcd_clsh_set_buck_mode(codec, mode);
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_hph_mode(codec, mode);
	} else {
		wcd_clsh_set_hph_mode(codec, CLS_H_NORMAL);

		/* set buck and flyback to Default Mode */
		wcd_clsh_flyback_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_buck_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_force_iq_ctl(codec, CLS_H_NORMAL, false);
		wcd_clsh_set_flyback_mode(codec, CLS_H_NORMAL);
		wcd_clsh_set_buck_mode(codec, CLS_H_NORMAL);
	}
}

static void wcd_clsh_state_aux(struct snd_soc_codec *codec,
			      struct wcd_clsh_cdc_info *clsh_d,
			      u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");

	if (is_enable) {
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
	}
}

static void wcd_clsh_state_ear(struct snd_soc_codec *codec,
		struct wcd_clsh_cdc_info *clsh_d,
		u8 req_state, bool is_enable, int mode)
{
	dev_dbg(codec->dev, "%s: mode: %s, %s\n", __func__, mode_to_str(mode),
		is_enable ? "enable" : "disable");

	if (is_enable) {
		wcd_clsh_set_buck_regulator_mode(codec, mode);
		wcd_clsh_set_flyback_mode(codec, mode);
		wcd_clsh_force_iq_ctl(codec, mode, true);
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_flyback_current(codec, mode);
		wcd_clsh_set_buck_mode(codec, mode);
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_hph_mode(codec, mode);
	} else {
		wcd_clsh_set_hph_mode(codec, CLS_H_NORMAL);

		/* set buck and flyback to Default Mode */
		wcd_clsh_flyback_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_buck_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_force_iq_ctl(codec, CLS_H_NORMAL, false);
		wcd_clsh_set_flyback_mode(codec, CLS_H_NORMAL);
		wcd_clsh_set_buck_mode(codec, CLS_H_NORMAL);
	}
}

static void wcd_clsh_state_err(struct snd_soc_codec *codec,
		struct wcd_clsh_cdc_info *clsh_d,
		u8 req_state, bool is_enable, int mode)
{
	char msg[128];

	dev_err(codec->dev,
		"%s Wrong request for class H state machine requested to %s %s\n",
		__func__, is_enable ? "enable" : "disable",
		state_to_str(req_state, msg, sizeof(msg)));
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
	case WCD_CLSH_STATE_AUX:
	case WCD_CLSH_STATE_HPHL_AUX:
	case WCD_CLSH_STATE_HPHR_AUX:
	case WCD_CLSH_STATE_HPH_ST_AUX:
	case WCD_CLSH_STATE_EAR_AUX:
		return true;
	default:
		return false;
	};
}

/*
 * Function: wcd_cls_h_fsm
 * Params: codec, cdc_clsh_d, req_state, req_type, clsh_event
 * Description:
 * This function handles PRE DAC and POST DAC conditions of different devices
 * and updates class H configuration of different combination of devices
 * based on validity of their states. cdc_clsh_d will contain current
 * class h state information
 */
void wcd_cls_h_fsm(struct snd_soc_codec *codec,
		struct wcd_clsh_cdc_info *cdc_clsh_d,
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
		if (new_state < NUM_CLSH_STATES) {
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
EXPORT_SYMBOL(wcd_cls_h_fsm);

/*
 * wcd_cls_h_init: Called to init clsh info
 *
 * @clsh: pointer for clsh state information.
 */
void wcd_cls_h_init(struct wcd_clsh_cdc_info *clsh)
{
	int i;

	clsh->state = WCD_CLSH_STATE_IDLE;

	for (i = 0; i < NUM_CLSH_STATES; i++)
		clsh_state_fp[i] = wcd_clsh_state_err;

	clsh_state_fp[WCD_CLSH_STATE_EAR] = wcd_clsh_state_ear;
	clsh_state_fp[WCD_CLSH_STATE_HPHL] = wcd_clsh_state_hph_l;
	clsh_state_fp[WCD_CLSH_STATE_HPHR] = wcd_clsh_state_hph_r;
	clsh_state_fp[WCD_CLSH_STATE_HPH_ST] = wcd_clsh_state_hph_st;
	clsh_state_fp[WCD_CLSH_STATE_AUX] = wcd_clsh_state_aux;
	clsh_state_fp[WCD_CLSH_STATE_HPHL_AUX] = wcd_clsh_state_hph_aux;
	clsh_state_fp[WCD_CLSH_STATE_HPHR_AUX] = wcd_clsh_state_hph_aux;
	clsh_state_fp[WCD_CLSH_STATE_HPH_ST_AUX] =
						wcd_clsh_state_hph_aux;
	clsh_state_fp[WCD_CLSH_STATE_EAR_AUX] = wcd_clsh_state_ear_aux;
	clsh_state_fp[WCD_CLSH_STATE_HPHL_EAR] = wcd_clsh_state_hph_ear;
	clsh_state_fp[WCD_CLSH_STATE_HPHR_EAR] = wcd_clsh_state_hph_ear;
	clsh_state_fp[WCD_CLSH_STATE_HPH_ST_EAR] = wcd_clsh_state_hph_ear;
	/* Set interpolaotr modes to NONE */
	wcd_clsh_set_int_mode(clsh, WCD_CLSH_STATE_EAR, CLS_NONE);
	wcd_clsh_set_int_mode(clsh, WCD_CLSH_STATE_HPHL, CLS_NONE);
	wcd_clsh_set_int_mode(clsh, WCD_CLSH_STATE_HPHR, CLS_NONE);
	wcd_clsh_set_int_mode(clsh, WCD_CLSH_STATE_AUX, CLS_NONE);
	clsh->flyback_users = 0;
	clsh->buck_users = 0;
}
EXPORT_SYMBOL(wcd_cls_h_init);

MODULE_DESCRIPTION("WCD Class-H Driver");
MODULE_LICENSE("GPL v2");
