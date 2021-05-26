/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define TFA9890_I2CVERSION 34
#define TFA9890_NAMETABLE                                                  \
	static struct TfaBfName Tfa9890DatasheetNames[] = {                \
		{ 0x402, "I2SF" },     { 0x431, "CHS12" },                 \
		{ 0x450, "CHS3" },     { 0x461, "CHSA" },                  \
		{ 0x481, "I2SDOC" },   { 0x4a0, "DISP" },                  \
		{ 0x4b0, "I2SDOE" },   { 0x4c3, "I2SSR" },                 \
		{ 0x732, "DCMCC" },    { 0x9c0, "CCFD" },                  \
		{ 0x9d0, "ISEL" },     { 0xa02, "DOLS" },                  \
		{ 0xa32, "DORS" },     { 0xa62, "SPKL" },                  \
		{ 0xa91, "SPKR" },     { 0xab3, "DCFG" },                  \
		{ 0xf00, "VDDD" },     { 0xf10, "OTDD" },                  \
		{ 0xf20, "OVDD" },     { 0xf30, "UVDD" },                  \
		{ 0xf40, "OCDD" },     { 0xf50, "CLKD" },                  \
		{ 0xf60, "DCCD" },     { 0xf70, "SPKD" },                  \
		{ 0xf80, "WDD" },      { 0xf90, "LCLK" },                  \
		{ 0xfe0, "INT" },      { 0xff0, "INTP" },                  \
		{ 0x8f0f, "VERSION" }, { 0xffff, "Unknown bitfield enum" } \
	}
#define TFA9890_BITNAMETABLE                             \
	static struct TfaBfName Tfa9890BitNames[] = {    \
		{ 0x402, "i2s_seti" },                   \
		{ 0x431, "chan_sel1" },                  \
		{ 0x450, "lr_sw_i2si2" },                \
		{ 0x461, "input_sel" },                  \
		{ 0x481, "datao_sel" },                  \
		{ 0x4a0, "disable_idp" },                \
		{ 0x4b0, "enbl_datao" },                 \
		{ 0x4c3, "i2s_fs" },                     \
		{ 0x732, "ctrl_bstcur" },                \
		{ 0x9c0, "sel_cf_clk" },                 \
		{ 0x9d0, "intf_sel" },                   \
		{ 0xa02, "sel_i2so_l" },                 \
		{ 0xa32, "sel_i2so_r" },                 \
		{ 0xa62, "ctrl_spkr_coil" },             \
		{ 0xa91, "ctrl_spr_res" },               \
		{ 0xab3, "ctrl_dcdc_spkr_i_comp_gain" }, \
		{ 0xaf0, "ctrl_dcdc_spkr_i_comp_sign" }, \
		{ 0xf00, "flag_por_mask" },              \
		{ 0xf10, "flag_otpok_mask" },            \
		{ 0xf20, "flag_ovpok_mask" },            \
		{ 0xf30, "flag_uvpok_mask" },            \
		{ 0xf40, "flag_ocp_alarm_mask" },        \
		{ 0xf50, "flag_clocks_stable_mask" },    \
		{ 0xf60, "flag_pwrokbst_mask" },         \
		{ 0xf70, "flag_cf_speakererror_mask" },  \
		{ 0xf80, "flag_watchdog_reset_mask" },   \
		{ 0xf90, "flag_lost_clk_mask" },         \
		{ 0xfe0, "enable_interrupt" },           \
		{ 0xff0, "invert_int_polarity" },        \
		{ 0x4700, "switch_fb" },                 \
		{ 0x4713, "se_hyst" },                   \
		{ 0x4754, "se_level" },                  \
		{ 0x47a5, "ktemp" },                     \
		{ 0x8f0f, "production_data6" },          \
		{ 0xffff, "Unknown bitfield enum" }      \
	}
