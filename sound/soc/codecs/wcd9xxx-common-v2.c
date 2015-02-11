/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include "wcd9xxx-common-v2.h"

#define WCD_USLEEP_RANGE 5

static void (*clsh_state_fp[NUM_CLSH_STATES])(struct snd_soc_codec *,
					      struct wcd_clsh_cdc_data *,
					      u8 req_state, bool en, int mode);

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
	dev_dbg(codec->dev, "%s: clsh_users %d, enable %d", __func__,
		clsh_d->clsh_users, enable);
}

static void wcd_clsh_buck_ctrl(struct snd_soc_codec *codec,
			       struct wcd_clsh_cdc_data *clsh_d,
			       int mode,
			       bool enable)
{
	/* set buck mode */
	if (mode == CLS_H_HIFI)
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    0x08, 0x08); /* set to HIFI */
	else
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    0x08, 0x00); /* set to default */

	/* enable/disable buck */
	if ((enable && (++clsh_d->buck_users == 1)) ||
	   (!enable && (--clsh_d->buck_users == 0)))
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    (1 << 7), (enable << 7));
	dev_dbg(codec->dev, "%s: buck_users %d, enable %d, mode: %s",
		__func__, clsh_d->buck_users, enable, mode_to_str(mode));
	/*
	 * 50us sleep is required after buck enable/disable
	 * as per HW requirement
	 */
	usleep_range(50, 50 + WCD_USLEEP_RANGE);
}

static void wcd_clsh_flyback_ctrl(struct snd_soc_codec *codec,
				  struct wcd_clsh_cdc_data *clsh_d,
				  int mode,
				  bool enable)
{
	/* set flyback mode */
	if (mode == CLS_H_HIFI)
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    0x04, 0x04); /* set to HIFI */
	else
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    0x04, 0x00); /* set to default */

	/* enable/disable flyback */
	if ((enable && (++clsh_d->flyback_users == 1)) ||
	   (!enable && (--clsh_d->flyback_users == 0)))
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    (1 << 6), (enable << 6));
	dev_dbg(codec->dev, "%s: flyback_users %d, enable %d, mode: %s",
		__func__, clsh_d->flyback_users, enable, mode_to_str(mode));
	/*
	 * 50us sleep is required after flyback enable/disable
	 * as per HW requirement
	 */
	usleep_range(50, 50 + WCD_USLEEP_RANGE);
}

static void wcd_clsh_set_hph_mode(struct snd_soc_codec *codec,
				  int mode)
{
	u8 val;

	switch (mode) {
	case CLS_H_NORMAL:
	case CLS_AB:
		val = 0x00;
		break;
	case CLS_H_HIFI:
		val = 0x08;
		break;
	case CLS_H_LP:
		val = 0x04;
		break;
	};

	snd_soc_update_bits(codec, WCD9XXX_A_ANA_HPH, 0x0C, val);
}

static void wcd_clsh_set_buck_regulator_mode(struct snd_soc_codec *codec,
					     int mode)
{
	if (mode == CLS_AB)
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    0x02, 0x02);
	else
		snd_soc_update_bits(codec, WCD9XXX_A_ANA_RX_SUPPLIES,
				    0x02, 0x00);
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
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_hph_mode(codec, mode);
	} else {
		wcd_clsh_set_hph_mode(codec, CLS_H_NORMAL);
		/* buck and flyback set to default mode and disable */
		wcd_clsh_buck_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_flyback_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_set_buck_regulator_mode(codec, CLS_H_NORMAL);

		if (mode != CLS_AB) {
			snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX2_RX_PATH_CFG0,
					    0x40, 0x00);
			wcd_enable_clsh_block(codec, clsh_d, false);
		}
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
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_set_hph_mode(codec, mode);
	} else {
		wcd_clsh_set_hph_mode(codec, CLS_H_NORMAL);
		/* set buck and flyback to Default Mode */
		wcd_clsh_buck_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_flyback_ctrl(codec, clsh_d, CLS_H_NORMAL, false);
		wcd_clsh_set_buck_regulator_mode(codec, CLS_H_NORMAL);

		if (mode != CLS_AB) {
			snd_soc_update_bits(codec,
					    WCD9XXX_A_CDC_RX1_RX_PATH_CFG0,
					    0x40, 0x00);
			wcd_enable_clsh_block(codec, clsh_d, false);
		}
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
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, true);
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, true);
	} else {
		wcd_clsh_buck_ctrl(codec, clsh_d, mode, false);
		wcd_clsh_flyback_ctrl(codec, clsh_d, mode, false);
		snd_soc_update_bits(codec,
				    WCD9XXX_A_CDC_RX0_RX_PATH_CFG0,
				    0x40, 0x00);
		wcd_enable_clsh_block(codec, clsh_d, false);
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
		(*clsh_state_fp[new_state]) (codec, cdc_clsh_d, req_state,
					     CLSH_REQ_ENABLE, int_mode);
		cdc_clsh_d->state = new_state;
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
			dev_dbg(codec->dev, "%s: ClassH state transition from %s to %s\n",
				__func__, state_to_str(old_state, msg0,
						       sizeof(msg0)),
				state_to_str(cdc_clsh_d->state, msg1,
					     sizeof(msg1)));
		}
		break;
	};
}

void wcd_clsh_init(struct wcd_clsh_cdc_data *clsh)
{
	int i;
	clsh->state = WCD_CLSH_STATE_IDLE;

	for (i = 0; i < NUM_CLSH_STATES; i++)
		clsh_state_fp[i] = wcd_clsh_state_err;

	clsh_state_fp[WCD_CLSH_STATE_EAR] = wcd_clsh_state_ear;
	clsh_state_fp[WCD_CLSH_STATE_HPHL] =
						wcd_clsh_state_hph_l;
	clsh_state_fp[WCD_CLSH_STATE_HPHR] =
						wcd_clsh_state_hph_r;
	clsh_state_fp[WCD_CLSH_STATE_HPH_ST] =
						wcd_clsh_state_hph_st;
}
EXPORT_SYMBOL(wcd_clsh_init);

MODULE_DESCRIPTION("WCD9XXX Common Driver");
MODULE_LICENSE("GPL v2");
