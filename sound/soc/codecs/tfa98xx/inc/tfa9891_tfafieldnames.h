/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef TFA_INC_TFA9891_TFAFIELDNAMES_H_
#define TFA_INC_TFA9891_TFAFIELDNAMES_H_
#define TFA9891_I2CVERSION 13
#define TFA9891_NAMETABLE                                                      \
	static struct TfaBfName Tfa9891DatasheetNames[] = {                    \
		{ 0x0, "VDDS" },	   { 0x10, "PLLS" },                   \
		{ 0x20, "OTDS" },	  { 0x30, "OVDS" },                   \
		{ 0x40, "UVDS" },	  { 0x50, "OCDS" },                   \
		{ 0x60, "CLKS" },	  { 0x70, "CLIPS" },                  \
		{ 0x80, "MTPB" },	  { 0x90, "DCCS" },                   \
		{ 0xa0, "SPKS" },	  { 0xb0, "ACS" },                    \
		{ 0xc0, "SWS" },	   { 0xd0, "WDS" },                    \
		{ 0xe0, "AMPS" },	  { 0xf0, "AREFS" },                  \
		{ 0x109, "BATS" },	 { 0x208, "TEMPS" },                 \
		{ 0x307, "REV" },	  { 0x402, "I2SF" },                  \
		{ 0x431, "CHS12" },	{ 0x450, "CHS3" },                  \
		{ 0x461, "CHSA" },	 { 0x481, "I2SDOC" },                \
		{ 0x4a0, "DISP" },	 { 0x4b0, "I2SDOE" },                \
		{ 0x4c3, "I2SSR" },	{ 0x501, "BSSCR" },                 \
		{ 0x523, "BSST" },	 { 0x561, "BSSRL" },                 \
		{ 0x582, "BSSRR" },	{ 0x5b1, "BSSHY" },                 \
		{ 0x5e0, "BSSR" },	 { 0x5f0, "BSSBY" },                 \
		{ 0x600, "DPSA" },	 { 0x613, "AMPSL" },                 \
		{ 0x650, "CFSM" },	 { 0x670, "BSSS" },                  \
		{ 0x687, "VOL" },	  { 0x702, "DCVO" },                  \
		{ 0x732, "DCMCC" },	{ 0x7a0, "DCIE" },                  \
		{ 0x7b0, "DCSR" },	 { 0x800, "TROS" },                  \
		{ 0x818, "EXTTS" },	{ 0x900, "PWDN" },                  \
		{ 0x910, "I2CR" },	 { 0x920, "CFE" },                   \
		{ 0x930, "AMPE" },	 { 0x940, "DCA" },                   \
		{ 0x950, "SBSL" },	 { 0x960, "AMPC" },                  \
		{ 0x970, "DCDIS" },	{ 0x980, "PSDR" },                  \
		{ 0x991, "DCCV" },	 { 0x9b1, "CCFD" },                  \
		{ 0x9d0, "ISEL" },	 { 0x9e0, "IPLL" },                  \
		{ 0xa02, "DOLS" },	 { 0xa32, "DORS" },                  \
		{ 0xa62, "SPKL" },	 { 0xa91, "SPKR" },                  \
		{ 0xab3, "DCFG" },	 { 0xb07, "MTPK" },                  \
		{ 0xf00, "VDDD" },	 { 0xf10, "OTDD" },                  \
		{ 0xf20, "OVDD" },	 { 0xf30, "UVDD" },                  \
		{ 0xf40, "OCDD" },	 { 0xf50, "CLKD" },                  \
		{ 0xf60, "DCCD" },	 { 0xf70, "SPKD" },                  \
		{ 0xf80, "WDD" },	  { 0xfe0, "INT" },                   \
		{ 0xff0, "INTP" },	 { 0x1000, "PDMSEL" },               \
		{ 0x1010, "I2SMOUTEN" },   { 0x1021, "PDMORSEL" },             \
		{ 0x1041, "PDMOLSEL" },    { 0x1061, "PADSEL" },               \
		{ 0x1100, "PDMOSDEN" },    { 0x1110, "PDMOSDCF" },             \
		{ 0x1140, "SAAMEN" },      { 0x1150, "SAAMLPEN" },             \
		{ 0x1160, "PDMOINTEN" },   { 0x1203, "PDMORG1" },              \
		{ 0x1243, "PDMORG2" },     { 0x1303, "PDMOLG1" },              \
		{ 0x1343, "PDMOLG2" },     { 0x2202, "SAAMGAIN" },             \
		{ 0x2250, "SAAMPGACTRL" }, { 0x2500, "PLLCCOSEL" },            \
		{ 0x4600, "CSBYPGC" },     { 0x4900, "CLIP" },                 \
		{ 0x4910, "CLIP2" },       { 0x62b0, "CIMTP" },                \
		{ 0x7000, "RST" },	 { 0x7011, "DMEM" },                 \
		{ 0x7030, "AIF" },	 { 0x7040, "CFINT" },                \
		{ 0x7087, "REQ" },	 { 0x710f, "MADD" },                 \
		{ 0x720f, "MEMA" },	{ 0x7307, "ERR" },                  \
		{ 0x7387, "ACK" },	 { 0x8000, "MTPOTC" },               \
		{ 0x8010, "MTPEX" },       { 0x8045, "SWPROFIL" },             \
		{ 0x80a5, "SWVSTEP" },     { 0xffff, "Unknown bitfield enum" } \
	}
#define TFA9891_BITNAMETABLE                               \
	static struct TfaBfName Tfa9891BitNames[] = {      \
		{ 0x0, "POR" },                            \
		{ 0x10, "PLL_LOCK" },                      \
		{ 0x20, "flag_otpok" },                    \
		{ 0x30, "flag_ovpok" },                    \
		{ 0x40, "flag_uvpok" },                    \
		{ 0x50, "flag_OCP_alarm" },                \
		{ 0x60, "flag_clocks_stable" },            \
		{ 0x70, "CLIP" },                          \
		{ 0x80, "mtp_busy" },                      \
		{ 0x90, "flag_pwrokbst" },                 \
		{ 0xa0, "flag_cf_speakererror" },          \
		{ 0xb0, "flag_cold_started" },             \
		{ 0xc0, "flag_engage" },                   \
		{ 0xd0, "flag_watchdog_reset" },           \
		{ 0xe0, "flag_enbl_amp" },                 \
		{ 0xf0, "flag_enbl_ref" },                 \
		{ 0x109, "bat_adc" },                      \
		{ 0x208, "temp_adc" },                     \
		{ 0x307, "rev_reg" },                      \
		{ 0x402, "i2s_seti" },                     \
		{ 0x431, "chan_sel1" },                    \
		{ 0x450, "lr_sw_i2si2" },                  \
		{ 0x461, "input_sel" },                    \
		{ 0x481, "datao_sel" },                    \
		{ 0x4a0, "disable_idp" },                  \
		{ 0x4b0, "enbl_datao" },                   \
		{ 0x4c3, "i2s_fs" },                       \
		{ 0x501, "vbat_prot_attacktime" },         \
		{ 0x523, "vbat_prot_thlevel" },            \
		{ 0x561, "vbat_prot_max_reduct" },         \
		{ 0x582, "vbat_prot_release_t" },          \
		{ 0x5b1, "vbat_prot_hysterese" },          \
		{ 0x5d0, "reset_min_vbat" },               \
		{ 0x5e0, "sel_vbat" },                     \
		{ 0x5f0, "bypass_clipper" },               \
		{ 0x600, "dpsa" },                         \
		{ 0x613, "ctrl_slope" },                   \
		{ 0x650, "cf_mute" },                      \
		{ 0x660, "sel_other_vamp" },               \
		{ 0x670, "ctrl_batsensesteepness" },       \
		{ 0x687, "vol" },                          \
		{ 0x702, "ctrl_bstvolt" },                 \
		{ 0x732, "ctrl_bstcur" },                  \
		{ 0x761, "ctrl_slopebst_1_0" },            \
		{ 0x781, "ctrl_slopebst_3_2" },            \
		{ 0x7a0, "boost_intel" },                  \
		{ 0x7b0, "boost_speed" },                  \
		{ 0x7c1, "ctrl_delay_comp_dcdc" },         \
		{ 0x7e0, "boost_input" },                  \
		{ 0x7f0, "ctrl_supplysense" },             \
		{ 0x800, "ext_temp_sel" },                 \
		{ 0x818, "ext_temp" },                     \
		{ 0x8a0, "ctrl_spk_coilpvp_bst" },         \
		{ 0x8b2, "ctrl_dcdc_synchronisation" },    \
		{ 0x8e0, "ctrl_cs_samplevalid" },          \
		{ 0x900, "PowerDown" },                    \
		{ 0x910, "reset" },                        \
		{ 0x920, "enbl_coolflux" },                \
		{ 0x930, "enbl_amplifier" },               \
		{ 0x940, "enbl_boost" },                   \
		{ 0x950, "cf_configured" },                \
		{ 0x960, "sel_enbl_amplifier" },           \
		{ 0x970, "dcdcoff_mode" },                 \
		{ 0x980, "cttr_iddqtest" },                \
		{ 0x991, "ctrl_coil_value" },              \
		{ 0x9b1, "ctrl_sel_cf_clock" },            \
		{ 0x9d0, "intf_sel" },                     \
		{ 0x9e0, "sel_ws_bck" },                   \
		{ 0xa02, "sel_i2so_l" },                   \
		{ 0xa32, "sel_i2so_r" },                   \
		{ 0xa62, "ctrl_spkr_coil" },               \
		{ 0xa91, "ctrl_spr_res" },                 \
		{ 0xab3, "ctrl_dcdc_spkr_i_comp_gain" },   \
		{ 0xaf0, "ctrl_dcdc_spkr_i_comp_sign" },   \
		{ 0xb07, "MTP_key2" },                     \
		{ 0xc0c, "clk_sync_delay" },               \
		{ 0xcf0, "enbl_clk_sync" },                \
		{ 0xd0c, "adc_sync_delay" },               \
		{ 0xdf0, "enable_adc_sync" },              \
		{ 0xe00, "bypass_dcdc_curr_prot" },        \
		{ 0xe24, "ctrl_digtoana6_2" },             \
		{ 0xe70, "switch_on_icomp" },              \
		{ 0xe87, "reserve_reg_1_7_0" },            \
		{ 0xf00, "flag_por_mask" },                \
		{ 0xf10, "flag_otpok_mask" },              \
		{ 0xf20, "flag_ovpok_mask" },              \
		{ 0xf30, "flag_uvpok_mask" },              \
		{ 0xf40, "flag_ocp_alarm_mask" },          \
		{ 0xf50, "flag_clocks_stable_mask" },      \
		{ 0xf60, "flag_pwrokbst_mask" },           \
		{ 0xf70, "flag_cf_speakererror_mask" },    \
		{ 0xf80, "flag_watchdog_reset_mask" },     \
		{ 0xf90, "flag_lost_clk_mask" },           \
		{ 0xfe0, "enable_interrupt" },             \
		{ 0xff0, "invert_int_polarity" },          \
		{ 0x1000, "pdm_i2s_input" },               \
		{ 0x1010, "I2S_master_ena" },              \
		{ 0x1021, "pdm_out_sel_r" },               \
		{ 0x1041, "pdm_out_sel_l" },               \
		{ 0x1061, "micdat_out_sel" },              \
		{ 0x1100, "secure_dly" },                  \
		{ 0x1110, "d_out_valid_rf_mux" },          \
		{ 0x1140, "Speak_As_Mic_en" },             \
		{ 0x1150, "speak_as_mic_lp_mode" },        \
		{ 0x1160, "pdm_out_rate" },                \
		{ 0x1203, "ds4_g1_r" },                    \
		{ 0x1243, "ds4_g2_r" },                    \
		{ 0x1303, "ds4_g1_l" },                    \
		{ 0x1343, "ds4_g2_l" },                    \
		{ 0x1400, "clk_secure_dly" },              \
		{ 0x1410, "data_secure_dly" },             \
		{ 0x2202, "Ctrl_saam_pga_gain" },          \
		{ 0x2250, "ctrl_saam_pga_src" },           \
		{ 0x2300, "flag_saam_spare" },             \
		{ 0x2400, "ctrl_saam_pga_tm" },            \
		{ 0x2500, "pll_fcco" },                    \
		{ 0x3000, "flag_hi_small" },               \
		{ 0x3010, "flag_hi_large" },               \
		{ 0x3020, "flag_lo_small" },               \
		{ 0x3030, "flag_lo_large" },               \
		{ 0x3040, "flag_voutcomp" },               \
		{ 0x3050, "flag_voutcomp93" },             \
		{ 0x3060, "flag_voutcomp86" },             \
		{ 0x3070, "flag_hiz" },                    \
		{ 0x3080, "flag_hi_peak" },                \
		{ 0x3090, "flag_ocpokbst" },               \
		{ 0x30a0, "flag_peakcur" },                \
		{ 0x30b0, "flag_ocpokap" },                \
		{ 0x30c0, "flag_ocpokan" },                \
		{ 0x30d0, "flag_ocpokbp" },                \
		{ 0x30e0, "flag_ocpokbn" },                \
		{ 0x30f0, "lost_clk" },                    \
		{ 0x310f, "mtp_man_data_out" },            \
		{ 0x3200, "key01_locked" },                \
		{ 0x3210, "key02_locked" },                \
		{ 0x3225, "mtp_ecc_tcout" },               \
		{ 0x3280, "mtpctrl_valid_test_rd" },       \
		{ 0x3290, "mtpctrl_valid_test_wr" },       \
		{ 0x32a0, "flag_in_alarm_state" },         \
		{ 0x32b0, "mtp_ecc_err2" },                \
		{ 0x32c0, "mtp_ecc_err1" },                \
		{ 0x32d0, "mtp_mtp_hvf" },                 \
		{ 0x32f0, "mtp_zero_check_fail" },         \
		{ 0x3300, "flag_adc10_ready" },            \
		{ 0x3310, "flag_clipa_high" },             \
		{ 0x3320, "flag_clipa_low" },              \
		{ 0x3330, "flag_clipb_high" },             \
		{ 0x3340, "flag_clipb_low" },              \
		{ 0x3359, "data_adc10_tempbat" },          \
		{ 0x33f0, "flag_vddd_comp_nok" },          \
		{ 0x400f, "hid_code" },                    \
		{ 0x4100, "bypass_hp" },                   \
		{ 0x4110, "hard_mute" },                   \
		{ 0x4120, "soft_mute" },                   \
		{ 0x4134, "PWM_Delay" },                   \
		{ 0x4180, "PWM_Shape" },                   \
		{ 0x4190, "PWM_BitLength" },               \
		{ 0x4207, "ctrl_drive" },                  \
		{ 0x4281, "dpsalevel" },                   \
		{ 0x42a1, "dpsa_release" },                \
		{ 0x42c0, "ctrl_coincidence" },            \
		{ 0x42d0, "ctrl_kickback" },               \
		{ 0x42e0, "ctrl_test_sdeltaoffset" },      \
		{ 0x42f0, "ctrl_test_sdeltaclk" },         \
		{ 0x4309, "ctrl_drivebst" },               \
		{ 0x43a0, "ctrl_ocptestbst" },             \
		{ 0x43c0, "enbl_hi_peak" },                \
		{ 0x43d0, "test_abistfft_enbl" },          \
		{ 0x43e0, "ctrl_sensetest_amp" },          \
		{ 0x43f0, "test_bcontrol" },               \
		{ 0x4400, "ctrl_reversebst" },             \
		{ 0x4410, "ctrl_sensetest" },              \
		{ 0x4420, "enbl_engagebst" },              \
		{ 0x4430, "enbl_hi_small" },               \
		{ 0x4440, "enbl_hi_large" },               \
		{ 0x4450, "enbl_lo_small" },               \
		{ 0x4460, "enbl_lo_large" },               \
		{ 0x4470, "enbl_slopecur" },               \
		{ 0x4480, "enbl_voutcomp" },               \
		{ 0x4490, "enbl_voutcomp93" },             \
		{ 0x44a0, "enbl_voutcomp86" },             \
		{ 0x44b0, "enbl_hizcom" },                 \
		{ 0x44c0, "enbl_pcdac" },                  \
		{ 0x44d0, "enbl_pccomp" },                 \
		{ 0x44e0, "enbl_windac" },                 \
		{ 0x44f0, "enbl_powerbst" },               \
		{ 0x4507, "ocp_thr" },                     \
		{ 0x4580, "bypass_glitchfilter" },         \
		{ 0x4590, "bypass_ovp" },                  \
		{ 0x45a0, "bypass_uvp" },                  \
		{ 0x45b0, "bypass_otp" },                  \
		{ 0x45c0, "bypass_ocp" },                  \
		{ 0x45d0, "bypass_ocpcounter" },           \
		{ 0x45e0, "bypass_lost_clk" },             \
		{ 0x45f0, "vpalarm" },                     \
		{ 0x4600, "bypass_gc" },                   \
		{ 0x4610, "cs_gain_control" },             \
		{ 0x4627, "cs_gain" },                     \
		{ 0x46a0, "bypass_lp" },                   \
		{ 0x46b0, "bypass_pwmcounter" },           \
		{ 0x46c0, "ctrl_cs_negfixed" },            \
		{ 0x46d2, "ctrl_cs_neghyst" },             \
		{ 0x4700, "switch_fb" },                   \
		{ 0x4713, "se_hyst" },                     \
		{ 0x4754, "se_level" },                    \
		{ 0x47a5, "ktemp" },                       \
		{ 0x4800, "ctrl_negin" },                  \
		{ 0x4810, "ctrl_cs_sein" },                \
		{ 0x4820, "ctrl_coincidencecs" },          \
		{ 0x4830, "ctrl_iddqtestbst" },            \
		{ 0x4840, "ctrl_coincidencebst" },         \
		{ 0x4851, "clock_sh_sel" },                \
		{ 0x4876, "delay_se_neg" },                \
		{ 0x48e1, "ctrl_cs_ttrack" },              \
		{ 0x4900, "ctrl_bypassclip" },             \
		{ 0x4910, "ctrl_bypassclip2" },            \
		{ 0x4920, "ctrl_clkgateCFoff" },           \
		{ 0x4930, "ctrl_testabst" },               \
		{ 0x4940, "ctrl_clipfast" },               \
		{ 0x4950, "ctrl_cs_8ohm" },                \
		{ 0x4960, "reserved" },                    \
		{ 0x4974, "delay_clock_sh" },              \
		{ 0x49c0, "inv_clksh" },                   \
		{ 0x49d0, "inv_neg" },                     \
		{ 0x49e0, "inv_se" },                      \
		{ 0x49f0, "setse" },                       \
		{ 0x4a12, "ctrl_adc10_sel" },              \
		{ 0x4a60, "ctrl_adc10_reset" },            \
		{ 0x4a81, "ctrl_adc10_test" },             \
		{ 0x4aa0, "ctrl_bypass_lp_vbat" },         \
		{ 0x4ae0, "ctrl_dc_offset" },              \
		{ 0x4af0, "ctrl_tsense_hibias" },          \
		{ 0x4b00, "ctrl_adc13_iset" },             \
		{ 0x4b14, "ctrl_adc13_gain" },             \
		{ 0x4b61, "ctrl_adc13_slowdel" },          \
		{ 0x4b83, "ctrl_adc13_offset" },           \
		{ 0x4bc0, "ctrl_adc13_bsoinv" },           \
		{ 0x4bd0, "ctrl_adc13_resonator_enable" }, \
		{ 0x4be0, "ctrl_testmicadc" },             \
		{ 0x4c0f, "ctrl_offset" },                 \
		{ 0x4d05, "ctrl_windac" },                 \
		{ 0x4d65, "ctrl_peakcur" },                \
		{ 0x4dc3, "pwm_dcc_cnt" },                 \
		{ 0x4e04, "ctrl_slopecur" },               \
		{ 0x4e53, "ctrl_dem" },                    \
		{ 0x4e93, "ctrl_demmismatch" },            \
		{ 0x4ed0, "enbl_pwm_dcc" },                \
		{ 0x5007, "gain" },                        \
		{ 0x5081, "ctrl_sourceb" },                \
		{ 0x50a1, "ctrl_sourcea" },                \
		{ 0x50c1, "ctrl_sourcebst" },              \
		{ 0x50e1, "ctrl_test_mono" },              \
		{ 0x5104, "pulselengthbst" },              \
		{ 0x5150, "ctrl_bypasslatchbst" },         \
		{ 0x5160, "invertbst" },                   \
		{ 0x5174, "pulselength" },                 \
		{ 0x51c0, "ctrl_bypasslatch" },            \
		{ 0x51d0, "invertb" },                     \
		{ 0x51e0, "inverta" },                     \
		{ 0x51f0, "ctrl_bypass_ctrlloop" },        \
		{ 0x5200, "ctrl_test_discrete" },          \
		{ 0x5210, "ctrl_test_rdsona" },            \
		{ 0x5220, "ctrl_test_rdsonb" },            \
		{ 0x5230, "ctrl_test_rdsonbst" },          \
		{ 0x5240, "ctrl_test_cvia" },              \
		{ 0x5250, "ctrl_test_cvib" },              \
		{ 0x5260, "ctrl_test_cvibst" },            \
		{ 0x5290, "test_bypass_pwmdiscretea" },    \
		{ 0x52a0, "test_bypass_pwmdiscreteb" },    \
		{ 0x52b0, "ctrl_clipc_forcehigh" },        \
		{ 0x52c0, "ctrl_clipc_forcelow" },         \
		{ 0x52d0, "ctrl_test_sdelta" },            \
		{ 0x52e0, "ctrl_test_swhvp" },             \
		{ 0x52f0, "test_gain_reduction" },         \
		{ 0x5303, "ctrl_digimux_out_test1" },      \
		{ 0x5343, "ctrl_digimux_out_test2" },      \
		{ 0x5383, "ctrl_digimux_out_data1" },      \
		{ 0x53c3, "ctrl_digimux_out_data3" },      \
		{ 0x5400, "hs_mode" },                     \
		{ 0x5412, "test_parametric_io" },          \
		{ 0x5440, "enbl_ringo" },                  \
		{ 0x5480, "ctrl_cliplevel" },              \
		{ 0x5491, "ctrl_anamux_sel" },             \
		{ 0x54b0, "test_vdddsw_dio" },             \
		{ 0x54c0, "ctrl_bypass_diosw_ovp" },       \
		{ 0x54d0, "test_vddd_sw" },                \
		{ 0x54e0, "test_vddd_sw_comp" },           \
		{ 0x550e, "enbl_amp" },                    \
		{ 0x55f0, "fr_fsp" },                      \
		{ 0x5600, "use_direct_ctrls" },            \
		{ 0x5610, "rst_datapath" },                \
		{ 0x5620, "rst_cgu" },                     \
		{ 0x5637, "enbl_ref" },                    \
		{ 0x56b0, "enbl_engage" },                 \
		{ 0x56c0, "use_direct_clk_ctrl" },         \
		{ 0x56d0, "use_direct_pll_ctrl" },         \
		{ 0x56e0, "use_direct_ctrls_2" },          \
		{ 0x5707, "ctrl_anamux_out_test1" },       \
		{ 0x5782, "ctrl_zero" },                   \
		{ 0x57b0, "enbl_ldo_stress" },             \
		{ 0x57c0, "ctrl_ocptest" },                \
		{ 0x57e0, "ctrl_otptest" },                \
		{ 0x57f0, "ctrl_reverse" },                \
		{ 0x5802, "pll_mdec_msb" },                \
		{ 0x5833, "pll_selr" },                    \
		{ 0x5874, "pll_selp" },                    \
		{ 0x58c3, "pll_seli" },                    \
		{ 0x5900, "pll_psel" },                    \
		{ 0x5910, "use_direct_pll_psel" },         \
		{ 0x5923, "nbck" },                        \
		{ 0x5960, "auto_nbck" },                   \
		{ 0x5970, "pll_frm" },                     \
		{ 0x5980, "pll_directi" },                 \
		{ 0x5990, "pll_directo" },                 \
		{ 0x59a0, "enbl_PLL" },                    \
		{ 0x59b0, "sel_clkout" },                  \
		{ 0x59e0, "fr_lost_clk" },                 \
		{ 0x59f0, "pll_bypass" },                  \
		{ 0x5a0f, "tsig_freq" },                   \
		{ 0x5b02, "tsig_freq_msb" },               \
		{ 0x5b30, "inject_tsig" },                 \
		{ 0x5b44, "ctrl_adc10_prog_sample" },      \
		{ 0x5c01, "pll_ndec_msb" },                \
		{ 0x5c2d, "pll_mdec" },                    \
		{ 0x5d06, "pll_pdec" },                    \
		{ 0x5d87, "pll_ndec" },                    \
		{ 0x5e00, "pdm_ch_sel_reg" },              \
		{ 0x5e10, "pdm_iis_rst_reg" },             \
		{ 0x5e20, "clk_src_sel_reg" },             \
		{ 0x5e70, "pdm_resync_bypass" },           \
		{ 0x6007, "MTP_key1" },                    \
		{ 0x6185, "mtp_ecc_tcin" },                \
		{ 0x6203, "mtp_man_address_in" },          \
		{ 0x6260, "mtp_ecc_eeb" },                 \
		{ 0x6270, "mtp_ecc_ecb" },                 \
		{ 0x6280, "man_copy_mtp_to_iic" },         \
		{ 0x6290, "man_copy_iic_to_mtp" },         \
		{ 0x62a0, "auto_copy_mtp_to_iic" },        \
		{ 0x62b0, "auto_copy_iic_to_mtp" },        \
		{ 0x62d2, "mtp_speed_mode" },              \
		{ 0x6340, "mtp_dircet_enable" },           \
		{ 0x6350, "mtp_direct_wr" },               \
		{ 0x6360, "mtp_direct_rd" },               \
		{ 0x6370, "mtp_direct_rst" },              \
		{ 0x6380, "mtp_direct_ers" },              \
		{ 0x6390, "mtp_direct_prg" },              \
		{ 0x63a0, "mtp_direct_epp" },              \
		{ 0x63b4, "mtp_direct_test" },             \
		{ 0x640f, "mtp_man_data_in" },             \
		{ 0x7000, "cf_rst_dsp" },                  \
		{ 0x7011, "cf_dmem" },                     \
		{ 0x7030, "cf_aif" },                      \
		{ 0x7040, "cf_int" },                      \
		{ 0x7087, "cf_req" },                      \
		{ 0x710f, "cf_madd" },                     \
		{ 0x720f, "cf_mema" },                     \
		{ 0x7307, "cf_err" },                      \
		{ 0x7387, "cf_ack" },                      \
		{ 0x8000, "calibration_onetime" },         \
		{ 0x8010, "calibr_ron_done" },             \
		{ 0x8105, "calibr_vout_offset" },          \
		{ 0x8163, "calibr_delta_gain" },           \
		{ 0x81a5, "calibr_offs_amp" },             \
		{ 0x8207, "calibr_gain_cs" },              \
		{ 0x8284, "calibr_temp_offset" },          \
		{ 0x82d2, "calibr_temp_gain" },            \
		{ 0x830f, "calibr_ron" },                  \
		{ 0x8406, "ctrl_offset_a" },               \
		{ 0x8486, "ctrl_offset_b" },               \
		{ 0x850f, "type_bits_HW" },                \
		{ 0x860f, "type_bits1_SW" },               \
		{ 0x870f, "type_bits2_SW" },               \
		{ 0x8a0f, "production_data1" },            \
		{ 0x8b0f, "production_data2" },            \
		{ 0x8c0f, "production_data3" },            \
		{ 0x8d0f, "production_data4" },            \
		{ 0x8e0f, "production_data5" },            \
		{ 0x8f0f, "production_data6" },            \
		{ 0xffff, "Unknown bitfield enum" }        \
	}
#endif
