/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define TFA9887_I2CVERSION 34
#define TFA9895_I2CVERSION 34
#define TFA9887_NAMETABLE                                                \
	static struct TfaBfName Tfa9887DatasheetNames[] = {                   \
		{ 0x402, "I2SF" },   { 0x431, "CHS12" },                 \
		{ 0x450, "CHS3" },   { 0x461, "CHSA" },                  \
		{ 0x4b0, "I2SDOE" }, { 0x4c3, "I2SSR" },                 \
		{ 0x500, "BSSBY" },  { 0x511, "BSSCR" },                 \
		{ 0x532, "BSST" },   { 0x5f0, "I2SDOC" },                \
		{ 0xa02, "DOLS" },   { 0xa32, "DORS" },                  \
		{ 0xa62, "SPKL" },   { 0xa91, "SPKR" },                  \
		{ 0xab3, "DCFG" },   { 0x4134, "PWMDEL" },               \
		{ 0x4180, "PWMSH" }, { 0x4190, "PWMRE" },                \
		{ 0x48e1, "TCC" },   { 0xffff, "Unknown bitfield enum" } \
	}
#define TFA9887_BITNAMETABLE                             \
	static struct TfaBfName Tfa9887BitNames[] = {         \
		{ 0x402, "i2s_seti" },                   \
		{ 0x431, "chan_sel1" },                  \
		{ 0x450, "lr_sw_i2si2" },                \
		{ 0x461, "input_sel" },                  \
		{ 0x4b0, "enbl_datao" },                 \
		{ 0x4c3, "i2s_fs" },                     \
		{ 0x500, "bypass_clipper" },             \
		{ 0x511, "vbat_prot_attacktime[1:0]" },  \
		{ 0x532, "vbat_prot_thlevel[2:0]" },     \
		{ 0x5d0, "reset_min_vbat" },             \
		{ 0x5f0, "datao_sel" },                  \
		{ 0xa02, "sel_i2so_l" },                 \
		{ 0xa32, "sel_i2so_r" },                 \
		{ 0xa62, "ctrl_spkr_coil" },             \
		{ 0xa91, "ctrl_spr_res" },               \
		{ 0xab3, "ctrl_dcdc_spkr_i_comp_gain" }, \
		{ 0xaf0, "ctrl_dcdc_spkr_i_comp_sign" }, \
		{ 0x4100, "bypass_hp" },                 \
		{ 0x4110, "hard_mute" },                 \
		{ 0x4120, "soft_mute" },                 \
		{ 0x4134, "PWM_Delay[4:0]" },            \
		{ 0x4180, "PWM_Shape" },                 \
		{ 0x4190, "PWM_BitLength" },             \
		{ 0x4800, "ctrl_negin" },                \
		{ 0x4810, "ctrl_cs_sein" },              \
		{ 0x4820, "ctrl_coincidencecs" },        \
		{ 0x4876, "delay_se_neg[6:0]" },         \
		{ 0x48e1, "ctrl_cs_ttrack[1:0]" },       \
		{ 0xffff, "Unknown bitfield enum" }      \
	}
