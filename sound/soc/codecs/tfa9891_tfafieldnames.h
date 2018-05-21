/*
 * tfa9891_tfafieldnames.h
 *
 *  Created on: Jul 16, 2015
 *      Author: wim
 */

#ifndef TFA_INC_TFA9891_TFAFIELDNAMES_H_
#define TFA_INC_TFA9891_TFAFIELDNAMES_H_

/** Filename: Tfa9891_TfaFieldnames.h
 *  This file was generated automatically on 07/16/15 at 15:00:02.
 *  Source file: TFA9891_I2C_list_V13.xls
 */

#define TFA9891_I2CVERSION    13.0


#define TFA9891_NAMETABLE static tfaBfName_t Tfa9891DatasheetNames[] = {\
	{ 0x0, "VDDS"},    /* POR                                               , */\
	{ 0x10, "PLLS"},    /* PLL                                               , */\
	{ 0x20, "OTDS"},    /* OTP                                               , */\
	{ 0x30, "OVDS"},    /* OVP                                               , */\
	{ 0x40, "UVDS"},    /* UVP                                               , */\
	{ 0x50, "OCDS"},    /* OCP                                               , */\
	{ 0x60, "CLKS"},    /* Clocks                                            , */\
	{ 0x70, "CLIPS"},    /* CLIP                                              , */\
	{ 0x80, "MTPB"},    /* MTP                                               , */\
	{ 0x90, "DCCS"},    /* BOOST                                             , */\
	{ 0xa0, "SPKS"},    /* Speaker                                           , */\
	{ 0xb0, "ACS"},    /* cold start flag                                   , */\
	{ 0xc0, "SWS"},    /* flag engage                                       , */\
	{ 0xd0, "WDS"},    /* flag watchdog reset                               , */\
	{ 0xe0, "AMPS"},    /* amplifier is enabled by manager                   , */\
	{ 0xf0, "AREFS"},    /* references are enabled by manager                 , */\
	{ 0x109, "BATS"},    /* Battery voltage readout; 0[V]..5.5[V]             , */\
	{ 0x208, "TEMPS"},    /* Temperature readout                               , */\
	{ 0x307, "REV"},    /* Device Revision                                   , */\
	{ 0x402, "I2SF"},    /* I2SFormat data 1 input                            , */\
	{ 0x431, "CHS12"},    /* ChannelSelection data1 input  (In CoolFlux)       , */\
	{ 0x450, "CHS3"},    /* Channel Selection data 2 input (coolflux input, the DCDC converter gets the other signal), */\
	{ 0x461, "CHSA"},    /* Input selection for amplifier                     , */\
	{ 0x481, "I2SDOC"},    /* Selection for I2S data out                        , */\
	{ 0x4a0, "DISP"},    /* idp protection                                    , */\
	{ 0x4b0, "I2SDOE"},    /* Enable data output                                , */\
	{ 0x4c3, "I2SSR"},    /* sample rate setting                               , */\
	{ 0x501, "BSSCR"},    /* ProtectionAttackTime                              , */\
	{ 0x523, "BSST"},    /* ProtectionThreshold                               , */\
	{ 0x561, "BSSRL"},    /* ProtectionMaximumReduction                        , */\
	{ 0x582, "BSSRR"},    /* Protection Release Timer                          , */\
	{ 0x5b1, "BSSHY"},    /* ProtectionHysterese                               , */\
	{ 0x5e0, "BSSR"},    /* battery voltage for I2C read out only             , */\
	{ 0x5f0, "BSSBY"},    /* bypass clipper battery protection                 , */\
	{ 0x600, "DPSA"},    /* Enable dynamic powerstage activation              , */\
	{ 0x613, "AMPSL"},    /* control slope                                     , */\
	{ 0x650, "CFSM"},    /* Soft mute in CoolFlux                             , */\
	{ 0x670, "BSSS"},    /* batsensesteepness                                 , */\
	{ 0x687, "VOL"},    /* volume control (in CoolFlux)                      , */\
	{ 0x702, "DCVO"},    /* Boost voltage                                     , */\
	{ 0x732, "DCMCC"},    /* Max boost coil current                            , */\
	{ 0x7a0, "DCIE"},    /* Adaptive boost mode                               , */\
	{ 0x7b0, "DCSR"},    /* Soft RampUp/Down mode for DCDC controller         , */\
	{ 0x800, "TROS"},    /* select external temperature also the ext_temp will be put on the temp read out , */\
	{ 0x818, "EXTTS"},    /* external temperature setting to be given by host  , */\
	{ 0x900, "PWDN"},    /* ON/OFF                                            , */\
	{ 0x910, "I2CR"},    /* I2CReset                                          , */\
	{ 0x920, "CFE"},    /* EnableCoolFlux                                    , */\
	{ 0x930, "AMPE"},    /* EnableAmplifier                                   , */\
	{ 0x940, "DCA"},    /* EnableBoost                                       , */\
	{ 0x950, "SBSL"},    /* Coolflux configured                               , */\
	{ 0x960, "AMPC"},    /* Selection on how AmplifierEnabling                , */\
	{ 0x970, "DCDIS"},    /* DCDC not connected                                , */\
	{ 0x980, "PSDR"},    /* Iddq test amplifier                               , */\
	{ 0x991, "DCCV"},    /* Coil Value                                        , */\
	{ 0x9b1, "CCFD"},    /* Selection CoolFluxClock                           , */\
	{ 0x9d0, "ISEL"},    /* Interface Selection                               , */\
	{ 0x9e0, "IPLL"},    /* selection input PLL for lock                      , */\
	{ 0xa02, "DOLS"},    /* Output selection dataout left channel             , */\
	{ 0xa32, "DORS"},    /* Output selection dataout right channel            , */\
	{ 0xa62, "SPKL"},    /* Selection speaker induction                       , */\
	{ 0xa91, "SPKR"},    /* Selection speaker impedance                       , */\
	{ 0xab3, "DCFG"},    /* DCDC speaker current compensation gain            , */\
	{ 0xb07, "MTPK"},    /* MTP KEY2 register                                 , */\
	{ 0xf00, "VDDD"},    /* mask flag_por for interupt generation             , */\
	{ 0xf10, "OTDD"},    /* mask flag_otpok for interupt generation           , */\
	{ 0xf20, "OVDD"},    /* mask flag_ovpok for interupt generation           , */\
	{ 0xf30, "UVDD"},    /* mask flag_uvpok for interupt generation           , */\
	{ 0xf40, "OCDD"},    /* mask flag_ocp_alarm for interupt generation       , */\
	{ 0xf50, "CLKD"},    /* mask flag_clocks_stable for interupt generation   , */\
	{ 0xf60, "DCCD"},    /* mask flag_pwrokbst for interupt generation        , */\
	{ 0xf70, "SPKD"},    /* mask flag_cf_speakererror for interupt generation , */\
	{ 0xf80, "WDD"},    /* mask flag_watchdog_reset for interupt generation  , */\
	{ 0xfe0, "INT"},    /* enabling interrupt                                , */\
	{ 0xff0, "INTP"},    /* Setting polarity interupt                         , */\
	{ 0x1000, "PDMSEL"},    /* Audio input interface mode                        , */\
	{ 0x1010, "I2SMOUTEN"},    /* I2S Master enable (CLK and WS pads)               , */\
	{ 0x1021, "PDMORSEL"},    /* PDM Output right channel source selection         , */\
	{ 0x1041, "PDMOLSEL"},    /* PDM Output Left/Mono channel source selection     , */\
	{ 0x1061, "PADSEL"},    /* Output interface mode and ball selection          , */\
	{ 0x1100, "PDMOSDEN"},    /* Secure delay Cell                                 , */\
	{ 0x1110, "PDMOSDCF"},    /* Rising Falling Resync control Mux                 , */\
	{ 0x1140, "SAAMEN"},    /* Speaker As a Mic feature ON/OFF                   , */\
	{ 0x1150, "SAAMLPEN"},    /* speaker_as_mic low power mode (only in PDM_out mode), */\
	{ 0x1160, "PDMOINTEN"},    /* PDM output interpolation ratio                    , */\
	{ 0x1203, "PDMORG1"},    /* PDM Interpolator Right Channel DS4 G1 Gain Value  , */\
	{ 0x1243, "PDMORG2"},    /* PDM Interpolator Right Channel DS4 G2 Gain Value  , */\
	{ 0x1303, "PDMOLG1"},    /* PDM Interpolator Left Channel DS4 G1 Gain Value   , */\
	{ 0x1343, "PDMOLG2"},    /* PDM Interpolator Left Channel DS4 G2 Gain Value   , */\
	{ 0x2202, "SAAMGAIN"},    /* pga gain                                          , */\
	{ 0x2250, "SAAMPGACTRL"},    /* 0 = active input common mode voltage source at the attenuator/PGA level, */\
	{ 0x2500, "PLLCCOSEL"},    /* pll cco frequency                                 , */\
	{ 0x4600, "CSBYPGC"},    /* bypass_gc, bypasses the CS gain correction        , */\
	{ 0x4900, "CLIP"},    /* Bypass clip control (function depending on digimux clip_x), */\
	{ 0x4910, "CLIP2"},    /* Bypass clip control (function depending on digimux clip_x), */\
	{ 0x62b0, "CIMTP"},    /* start copying all the data from i2cregs_mtp to mtp [Key 2 protected], */\
	{ 0x7000, "RST"},    /* Reset CoolFlux DSP                                , */\
	{ 0x7011, "DMEM"},    /* Target memory for access                          , */\
	{ 0x7030, "AIF"},    /* Autoincrement-flag for memory-address             , */\
	{ 0x7040, "CFINT"},    /* Interrupt CoolFlux DSP                            , */\
	{ 0x7087, "REQ"},    /* request for access (8 channels)                   , */\
	{ 0x710f, "MADD"},    /* memory-address to be accessed                     , */\
	{ 0x720f, "MEMA"},    /* activate memory access (24- or 32-bits data is written/read to/from memory, */\
	{ 0x7307, "ERR"},    /* cf error Flags                                    , */\
	{ 0x7387, "ACK"},    /* acknowledge of requests (8 channels")"            , */\
	{ 0x8000, "MTPOTC"},    /* Calibration schedule (key2 protected)             , */\
	{ 0x8010, "MTPEX"},    /* (key2 protected) calibration of Ron has been executed, */\
	{ 0x8045, "SWPROFIL" },\
	{ 0x80a5, "SWVSTEP" },\
	{ 0xffff, "Unknown bitfield enum" }   /* not found */\
};

#define TFA9891_BITNAMETABLE static tfaBfName_t Tfa9891BitNames[] = {\
	{ 0x0, "POR"},    /* POR                                               , */\
	{ 0x10, "PLL_LOCK"},    /* PLL                                               , */\
	{ 0x20, "flag_otpok"},    /* OTP                                               , */\
	{ 0x30, "flag_ovpok"},    /* OVP                                               , */\
	{ 0x40, "flag_uvpok"},    /* UVP                                               , */\
	{ 0x50, "flag_OCP_alarm"},    /* OCP                                               , */\
	{ 0x60, "flag_clocks_stable"},    /* Clocks                                            , */\
	{ 0x70, "CLIP"},    /* CLIP                                              , */\
	{ 0x80, "mtp_busy"},    /* MTP                                               , */\
	{ 0x90, "flag_pwrokbst"},    /* BOOST                                             , */\
	{ 0xa0, "flag_cf_speakererror"},    /* Speaker                                           , */\
	{ 0xb0, "flag_cold_started"},    /* cold start flag                                   , */\
	{ 0xc0, "flag_engage"},    /* flag engage                                       , */\
	{ 0xd0, "flag_watchdog_reset"},    /* flag watchdog reset                               , */\
	{ 0xe0, "flag_enbl_amp"},    /* amplifier is enabled by manager                   , */\
	{ 0xf0, "flag_enbl_ref"},    /* references are enabled by manager                 , */\
	{ 0x109, "bat_adc"},    /* Battery voltage readout; 0[V]..5.5[V]             , */\
	{ 0x208, "temp_adc"},    /* Temperature readout                               , */\
	{ 0x307, "rev_reg"},    /* Device Revision                                   , */\
	{ 0x402, "i2s_seti"},    /* I2SFormat data 1 input                            , */\
	{ 0x431, "chan_sel1"},    /* ChannelSelection data1 input  (In CoolFlux)       , */\
	{ 0x450, "lr_sw_i2si2"},    /* Channel Selection data 2 input (coolflux input, the DCDC converter gets the other signal), */\
	{ 0x461, "input_sel"},    /* Input selection for amplifier                     , */\
	{ 0x481, "datao_sel"},    /* Selection for I2S data out                        , */\
	{ 0x4a0, "disable_idp"},    /* idp protection                                    , */\
	{ 0x4b0, "enbl_datao"},    /* Enable data output                                , */\
	{ 0x4c3, "i2s_fs"},    /* sample rate setting                               , */\
	{ 0x501, "vbat_prot_attacktime"},    /* ProtectionAttackTime                              , */\
	{ 0x523, "vbat_prot_thlevel"},    /* ProtectionThreshold                               , */\
	{ 0x561, "vbat_prot_max_reduct"},    /* ProtectionMaximumReduction                        , */\
	{ 0x582, "vbat_prot_release_t"},    /* Protection Release Timer                          , */\
	{ 0x5b1, "vbat_prot_hysterese"},    /* ProtectionHysterese                               , */\
	{ 0x5d0, "reset_min_vbat"},    /* reset clipper                                     , */\
	{ 0x5e0, "sel_vbat"},    /* battery voltage for I2C read out only             , */\
	{ 0x5f0, "bypass_clipper"},    /* bypass clipper battery protection                 , */\
	{ 0x600, "dpsa"},    /* Enable dynamic powerstage activation              , */\
	{ 0x613, "ctrl_slope"},    /* control slope                                     , */\
	{ 0x650, "cf_mute"},    /* Soft mute in CoolFlux                             , */\
	{ 0x660, "sel_other_vamp"},    /* Input selection for the second channel of the DCDC inteligent mode detector, */\
	{ 0x670, "ctrl_batsensesteepness"},    /* batsensesteepness                                 , */\
	{ 0x687, "vol"},    /* volume control (in CoolFlux)                      , */\
	{ 0x702, "ctrl_bstvolt"},    /* Boost voltage                                     , */\
	{ 0x732, "ctrl_bstcur"},    /* Max boost coil current                            , */\
	{ 0x761, "ctrl_slopebst_1_0"},    /* Setting for the slope of the boost converter power stage, */\
	{ 0x781, "ctrl_slopebst_3_2"},    /* Setting for the part of the power transistor voltage to be used in peak current mode control, */\
	{ 0x7a0, "boost_intel"},    /* Adaptive boost mode                               , */\
	{ 0x7b0, "boost_speed"},    /* Soft RampUp/Down mode for DCDC controller         , */\
	{ 0x7c1, "ctrl_delay_comp_dcdc"},    /* delay compensation in current patg compared to delay in the audio path (relative) , */\
	{ 0x7e0, "boost_input"},    /* Selection intelligent boost detector input        , */\
	{ 0x7f0, "ctrl_supplysense"},    /* ADC10 input selection                             , */\
	{ 0x800, "ext_temp_sel"},    /* select external temperature also the ext_temp will be put on the temp read out , */\
	{ 0x818, "ext_temp"},    /* external temperature setting to be given by host  , */\
	{ 0x8a0, "ctrl_spk_coilpvp_bst"},    /* Peak voltage protection boost converter           , */\
	{ 0x8b2, "ctrl_dcdc_synchronisation"},    /* DCDC synchronisation off + 7 positions            , */\
	{ 0x8e0, "ctrl_cs_samplevalid"},    /* sample valid moment for CS in single sample moment mode, */\
	{ 0x900, "PowerDown"},    /* ON/OFF                                            , */\
	{ 0x910, "reset"},    /* I2CReset                                          , */\
	{ 0x920, "enbl_coolflux"},    /* EnableCoolFlux                                    , */\
	{ 0x930, "enbl_amplifier"},    /* EnableAmplifier                                   , */\
	{ 0x940, "enbl_boost"},    /* EnableBoost                                       , */\
	{ 0x950, "cf_configured"},    /* Coolflux configured                               , */\
	{ 0x960, "sel_enbl_amplifier"},    /* Selection on how AmplifierEnabling                , */\
	{ 0x970, "dcdcoff_mode"},    /* DCDC not connected                                , */\
	{ 0x980, "cttr_iddqtest"},    /* Iddq test amplifier                               , */\
	{ 0x991, "ctrl_coil_value"},    /* Coil Value                                        , */\
	{ 0x9b1, "ctrl_sel_cf_clock"},    /* Selection CoolFluxClock                           , */\
	{ 0x9d0, "intf_sel"},    /* Interface Selection                               , */\
	{ 0x9e0, "sel_ws_bck"},    /* selection input PLL for lock                      , */\
	{ 0xa02, "sel_i2so_l"},    /* Output selection dataout left channel             , */\
	{ 0xa32, "sel_i2so_r"},    /* Output selection dataout right channel            , */\
	{ 0xa62, "ctrl_spkr_coil"},    /* Selection speaker induction                       , */\
	{ 0xa91, "ctrl_spr_res"},    /* Selection speaker impedance                       , */\
	{ 0xab3, "ctrl_dcdc_spkr_i_comp_gain"},    /* DCDC speaker current compensation gain            , */\
	{ 0xaf0, "ctrl_dcdc_spkr_i_comp_sign"},    /* DCDC speaker current compensation sign            , */\
	{ 0xb07, "MTP_key2"},    /* MTP KEY2 register                                 , */\
	{ 0xc0c, "clk_sync_delay"},    /* Delay count for clock synchronisation             , */\
	{ 0xcf0, "enbl_clk_sync"},    /* Enable CGU clock synchronisation                  , */\
	{ 0xd0c, "adc_sync_delay"},    /* Delay count for ADC synchronisation               , */\
	{ 0xdf0, "enable_adc_sync"},    /* Enable ADC synchronisation                        , */\
	{ 0xe00, "bypass_dcdc_curr_prot"},    /* to switch off dcdc reduction with bat prot        , */\
	{ 0xe24, "ctrl_digtoana6_2"},    /* for extra connections digital to analog           , */\
	{ 0xe70, "switch_on_icomp"},    /* icomp dem switch                                  , */\
	{ 0xe87, "reserve_reg_1_7_0"},    /* reserved                                          , */\
	{ 0xf00, "flag_por_mask"},    /* mask flag_por for interupt generation             , */\
	{ 0xf10, "flag_otpok_mask"},    /* mask flag_otpok for interupt generation           , */\
	{ 0xf20, "flag_ovpok_mask"},    /* mask flag_ovpok for interupt generation           , */\
	{ 0xf30, "flag_uvpok_mask"},    /* mask flag_uvpok for interupt generation           , */\
	{ 0xf40, "flag_ocp_alarm_mask"},    /* mask flag_ocp_alarm for interupt generation       , */\
	{ 0xf50, "flag_clocks_stable_mask"},    /* mask flag_clocks_stable for interupt generation   , */\
	{ 0xf60, "flag_pwrokbst_mask"},    /* mask flag_pwrokbst for interupt generation        , */\
	{ 0xf70, "flag_cf_speakererror_mask"},    /* mask flag_cf_speakererror for interupt generation , */\
	{ 0xf80, "flag_watchdog_reset_mask"},    /* mask flag_watchdog_reset for interupt generation  , */\
	{ 0xf90, "flag_lost_clk_mask"},    /* mask flag_lost_clk for interupt generation        , */\
	{ 0xfe0, "enable_interrupt"},    /* enabling interrupt                                , */\
	{ 0xff0, "invert_int_polarity"},    /* Setting polarity interupt                         , */\
	{ 0x1000, "pdm_i2s_input"},    /* Audio input interface mode                        , */\
	{ 0x1010, "I2S_master_ena"},    /* I2S Master enable (CLK and WS pads)               , */\
	{ 0x1021, "pdm_out_sel_r"},    /* PDM Output right channel source selection         , */\
	{ 0x1041, "pdm_out_sel_l"},    /* PDM Output Left/Mono channel source selection     , */\
	{ 0x1061, "micdat_out_sel"},    /* Output interface mode and ball selection          , */\
	{ 0x1100, "secure_dly"},    /* Secure delay Cell                                 , */\
	{ 0x1110, "d_out_valid_rf_mux"},    /* Rising Falling Resync control Mux                 , */\
	{ 0x1140, "Speak_As_Mic_en"},    /* Speaker As a Mic feature ON/OFF                   , */\
	{ 0x1150, "speak_as_mic_lp_mode"},    /* speaker_as_mic low power mode (only in PDM_out mode), */\
	{ 0x1160, "pdm_out_rate"},    /* PDM output interpolation ratio                    , */\
	{ 0x1203, "ds4_g1_r"},    /* PDM Interpolator Right Channel DS4 G1 Gain Value  , */\
	{ 0x1243, "ds4_g2_r"},    /* PDM Interpolator Right Channel DS4 G2 Gain Value  , */\
	{ 0x1303, "ds4_g1_l"},    /* PDM Interpolator Left Channel DS4 G1 Gain Value   , */\
	{ 0x1343, "ds4_g2_l"},    /* PDM Interpolator Left Channel DS4 G2 Gain Value   , */\
	{ 0x1400, "clk_secure_dly"},    /* Secure delay Cell  on clock path                  , */\
	{ 0x1410, "data_secure_dly"},    /* Secure delay Cell  enable on PDM data path        , */\
	{ 0x2202, "Ctrl_saam_pga_gain"},    /* pga gain                                          , */\
	{ 0x2250, "ctrl_saam_pga_src"},    /* 0 = active input common mode voltage source at the attenuator/PGA level, */\
	{ 0x2300, "flag_saam_spare"},    /* spare flag                                        , */\
	{ 0x2400, "ctrl_saam_pga_tm"},    /* enables PGA test mode                             , */\
	{ 0x2500, "pll_fcco"},    /* pll cco frequency                                 , */\
	{ 0x3000, "flag_hi_small"},    /* positive small window dcdc converter              , */\
	{ 0x3010, "flag_hi_large"},    /* positive large window dcdc converter              , */\
	{ 0x3020, "flag_lo_small"},    /* negative small window dcdc converter              , */\
	{ 0x3030, "flag_lo_large"},    /* negative large window dcdc converter              , */\
	{ 0x3040, "flag_voutcomp"},    /* flag_voutcomp, indication Vset is larger than Vbat, */\
	{ 0x3050, "flag_voutcomp93"},    /* flag_voutcomp93, indication Vset is larger than 1.07* Vbat , */\
	{ 0x3060, "flag_voutcomp86"},    /* flag_voutcomp86, indication Vset is larger than 1.14* Vbat , */\
	{ 0x3070, "flag_hiz"},    /* flag_hiz, indication Vbst is larger than  Vbat    , */\
	{ 0x3080, "flag_hi_peak"},    /* flag_hi_peak, indication hi_peak                  , */\
	{ 0x3090, "flag_ocpokbst"},    /* flag_ocpokbst, indication no over current in boost converter pmos switch, */\
	{ 0x30a0, "flag_peakcur"},    /* flag_peakcur, indication current is max in dcdc converter, */\
	{ 0x30b0, "flag_ocpokap"},    /* flag_ocpokap, indication no over current in amplifier "a" pmos output stage, */\
	{ 0x30c0, "flag_ocpokan"},    /* flag_ocpokan, indication no over current in amplifier "a" nmos output stage, */\
	{ 0x30d0, "flag_ocpokbp"},    /* flag_ocpokbp, indication no over current in amplifier "b" pmos output stage, */\
	{ 0x30e0, "flag_ocpokbn"},    /* flag_ocpokbn, indication no over current in amplifier"b" nmos output stage, */\
	{ 0x30f0, "lost_clk"},    /* lost_clk, lost clock indication CGU               , */\
	{ 0x310f, "mtp_man_data_out"},    /* single word read from MTP (manual copy)           , */\
	{ 0x3200, "key01_locked"},    /* key01_locked, indication key 1 is locked          , */\
	{ 0x3210, "key02_locked"},    /* key02_locked, indication key 2 is locked          , */\
	{ 0x3225, "mtp_ecc_tcout"},    /* mtp_ecc_tcout                                     , */\
	{ 0x3280, "mtpctrl_valid_test_rd"},    /* mtp test readout for read                         , */\
	{ 0x3290, "mtpctrl_valid_test_wr"},    /* mtp test readout for write                        , */\
	{ 0x32a0, "flag_in_alarm_state"},    /* alarm state                                       , */\
	{ 0x32b0, "mtp_ecc_err2"},    /* two or more bit errors detected in MTP, can not reconstruct value, */\
	{ 0x32c0, "mtp_ecc_err1"},    /* one bit error detected in MTP, reconstructed value, */\
	{ 0x32d0, "mtp_mtp_hvf"},    /* high voltage ready flag for MTP                   , */\
	{ 0x32f0, "mtp_zero_check_fail"},    /* zero check failed (tbd) for MTP                   , */\
	{ 0x3300, "flag_adc10_ready"},    /* flag_adc10_ready, indication adc10 is ready       , */\
	{ 0x3310, "flag_clipa_high"},    /* flag_clipa_high, indication pmos amplifier "a" is clipping, */\
	{ 0x3320, "flag_clipa_low"},    /* flag_clipa_low, indication nmos amplifier "a" is clipping, */\
	{ 0x3330, "flag_clipb_high"},    /* flag_clipb_high, indication pmos amplifier "b" is clipping, */\
	{ 0x3340, "flag_clipb_low"},    /* flag_clipb_low, indication nmos amplifier "b" is clipping, */\
	{ 0x3359, "data_adc10_tempbat"},    /* adc 10 data output for testing                    , */\
	{ 0x33f0, "flag_vddd_comp_nok"},    /* power switch flag 2 for testing                   , */\
	{ 0x400f, "hid_code"},    /* hidden code                                       , */\
	{ 0x4100, "bypass_hp"},    /* Bypass_High Pass Filter                           , */\
	{ 0x4110, "hard_mute"},    /* Hard Mute                                         , */\
	{ 0x4120, "soft_mute"},    /* Soft Mute                                         , */\
	{ 0x4134, "PWM_Delay"},    /* PWM DelayBits to set the delay                    , */\
	{ 0x4180, "PWM_Shape"},    /* PWM Shape                                         , */\
	{ 0x4190, "PWM_BitLength"},    /* PWM Bitlength in noise shaper                     , */\
	{ 0x4207, "ctrl_drive"},    /* drive bits to select amount of power stages amplifier, */\
	{ 0x4281, "dpsalevel"},    /* DPSA Threshold level                              , */\
	{ 0x42a1, "dpsa_release"},    /* DPSA Release time                                 , */\
	{ 0x42c0, "ctrl_coincidence"},    /* Prevent simultaneously switching of output stage  , */\
	{ 0x42d0, "ctrl_kickback"},    /* Prevent double pulses of output stage             , */\
	{ 0x42e0, "ctrl_test_sdeltaoffset"},    /* ctrl_test_sdeltaoffset                            , */\
	{ 0x42f0, "ctrl_test_sdeltaclk"},    /* ctrl_test_sdeltaclk                               , */\
	{ 0x4309, "ctrl_drivebst"},    /* Drive bits to select the powertransistor sections boost converter, */\
	{ 0x43a0, "ctrl_ocptestbst"},    /* Boost OCP.                                        , */\
	{ 0x43c0, "enbl_hi_peak"},    /* enable for high peak comparator                   , */\
	{ 0x43d0, "test_abistfft_enbl"},    /* FFT coolflux                                      , */\
	{ 0x43e0, "ctrl_sensetest_amp"},    /* sensetest amplifier                               , */\
	{ 0x43f0, "test_bcontrol"},    /* test _bcontrol                                    , */\
	{ 0x4400, "ctrl_reversebst"},    /* OverCurrent Protection selection of power stage boost converter, */\
	{ 0x4410, "ctrl_sensetest"},    /* Test option for the sense NMOS in booster for current mode control., */\
	{ 0x4420, "enbl_engagebst"},    /* Enable power stage dcdc controller                , */\
	{ 0x4430, "enbl_hi_small"},    /* Enable bit of hi (small) comparator               , */\
	{ 0x4440, "enbl_hi_large"},    /* Enable bit of hi (large) comparator               , */\
	{ 0x4450, "enbl_lo_small"},    /* Enable bit of lo (small) comparator               , */\
	{ 0x4460, "enbl_lo_large"},    /* Enable bit of lo (large) comparator               , */\
	{ 0x4470, "enbl_slopecur"},    /* Enable bit of max-current dac                     , */\
	{ 0x4480, "enbl_voutcomp"},    /* Enable vout comparators                           , */\
	{ 0x4490, "enbl_voutcomp93"},    /* Enable vout-93 comparators                        , */\
	{ 0x44a0, "enbl_voutcomp86"},    /* Enable vout-86 comparators                        , */\
	{ 0x44b0, "enbl_hizcom"},    /* Enable hiz comparator                             , */\
	{ 0x44c0, "enbl_pcdac"},    /* Enable peak current dac                           , */\
	{ 0x44d0, "enbl_pccomp"},    /* Enable peak current comparator                    , */\
	{ 0x44e0, "enbl_windac"},    /* Enable window dac                                 , */\
	{ 0x44f0, "enbl_powerbst"},    /* Enable line of the powerstage                     , */\
	{ 0x4507, "ocp_thr"},    /* ocp_thr threshold level for OCP                   , */\
	{ 0x4580, "bypass_glitchfilter"},    /* Bypass glitchfilter                               , */\
	{ 0x4590, "bypass_ovp"},    /* Bypass OVP                                        , */\
	{ 0x45a0, "bypass_uvp"},    /* Bypass UVP                                        , */\
	{ 0x45b0, "bypass_otp"},    /* Bypass OTP                                        , */\
	{ 0x45c0, "bypass_ocp"},    /* Bypass OCP                                        , */\
	{ 0x45d0, "bypass_ocpcounter"},    /* BypassOCPCounter                                  , */\
	{ 0x45e0, "bypass_lost_clk"},    /* Bypasslost_clk detector                           , */\
	{ 0x45f0, "vpalarm"},    /* vpalarm (uvp ovp handling)                        , */\
	{ 0x4600, "bypass_gc"},    /* bypass_gc, bypasses the CS gain correction        , */\
	{ 0x4610, "cs_gain_control"},    /* gain control by means of MTP or i2c               , */\
	{ 0x4627, "cs_gain"},    /* + / - 128 steps in steps of 1/4 %  2's compliment , */\
	{ 0x46a0, "bypass_lp"},    /* bypass Low-Pass filter in temperature sensor      , */\
	{ 0x46b0, "bypass_pwmcounter"},    /* bypass_pwmcounter                                 , */\
	{ 0x46c0, "ctrl_cs_negfixed"},    /* does not switch to neg                            , */\
	{ 0x46d2, "ctrl_cs_neghyst"},    /* switches to neg depending on level                , */\
	{ 0x4700, "switch_fb"},    /* switch_fb                                         , */\
	{ 0x4713, "se_hyst"},    /* se_hyst                                           , */\
	{ 0x4754, "se_level"},    /* se_level                                          , */\
	{ 0x47a5, "ktemp"},    /* temperature compensation trimming                 , */\
	{ 0x4800, "ctrl_negin"},    /* negin                                             , */\
	{ 0x4810, "ctrl_cs_sein"},    /* cs_sein                                           , */\
	{ 0x4820, "ctrl_coincidencecs"},    /* Coincidence current sense                         , */\
	{ 0x4830, "ctrl_iddqtestbst"},    /* for iddq testing in powerstage of boost convertor , */\
	{ 0x4840, "ctrl_coincidencebst"},    /* Switch protection on to prevent simultaniously switching power stages bst and amp, */\
	{ 0x4851, "clock_sh_sel"},    /* Clock SH selection                                , */\
	{ 0x4876, "delay_se_neg"},    /* delay of se and neg                               , */\
	{ 0x48e1, "ctrl_cs_ttrack"},    /* sample & hold track time                          , */\
	{ 0x4900, "ctrl_bypassclip"},    /* Bypass clip control (function depending on digimux clip_x), */\
	{ 0x4910, "ctrl_bypassclip2"},    /* Bypass clip control (function depending on digimux clip_x), */\
	{ 0x4920, "ctrl_clkgateCFoff"},    /* to disable clock gating in the coolflux           , */\
	{ 0x4930, "ctrl_testabst"},    /* testabst                                          , */\
	{ 0x4940, "ctrl_clipfast"},    /* clock switch for battery protection clipper, it switches back to old frequency, */\
	{ 0x4950, "ctrl_cs_8ohm"},    /* 8 ohm mode for current sense (gain mode)          , */\
	{ 0x4960, "reserved"},    /* reserved                                          , */\
	{ 0x4974, "delay_clock_sh"},    /* delay_sh, tunes S7H delay                         , */\
	{ 0x49c0, "inv_clksh"},    /* Invert the sample/hold clock for current sense ADC, */\
	{ 0x49d0, "inv_neg"},    /* Invert neg signal                                 , */\
	{ 0x49e0, "inv_se"},    /* Invert se signal                                  , */\
	{ 0x49f0, "setse"},    /* switches between Single Ende and differentail mode, */\
	{ 0x4a12, "ctrl_adc10_sel"},    /* select the input to convert the 10b ADC           , */\
	{ 0x4a60, "ctrl_adc10_reset"},    /* Global asynchronous reset (active HIGH) 10 bit ADC, */\
	{ 0x4a81, "ctrl_adc10_test"},    /* Test mode selection signal 10 bit ADC             , */\
	{ 0x4aa0, "ctrl_bypass_lp_vbat"},    /* lp filter in batt sensor                          , */\
	{ 0x4ae0, "ctrl_dc_offset"},    /* switch offset control on/off, is decimator offset control, */\
	{ 0x4af0, "ctrl_tsense_hibias"},    /* bit to set the biasing in temp sensor to high     , */\
	{ 0x4b00, "ctrl_adc13_iset"},    /* Micadc Setting of current consumption. Debug use only, */\
	{ 0x4b14, "ctrl_adc13_gain"},    /* Micadc gain setting (2-compl)                     , */\
	{ 0x4b61, "ctrl_adc13_slowdel"},    /* Micadc Delay setting for internal clock. Debug use only, */\
	{ 0x4b83, "ctrl_adc13_offset"},    /* Micadc ADC offset setting                         , */\
	{ 0x4bc0, "ctrl_adc13_bsoinv"},    /* Micadc bit stream output invert mode for test     , */\
	{ 0x4bd0, "ctrl_adc13_resonator_enable"},    /* Micadc Give extra SNR with less stability. Debug use only, */\
	{ 0x4be0, "ctrl_testmicadc"},    /* Mux at input of MICADC for test purpose           , */\
	{ 0x4c0f, "ctrl_offset"},    /* offset control for ABIST testing                  , */\
	{ 0x4d05, "ctrl_windac"},    /* for testing direct control windac                 , */\
	{ 0x4d65, "ctrl_peakcur"},    /* Control peakcur                                   , */\
	{ 0x4dc3, "pwm_dcc_cnt"},    /* control pwm duty cycle when enbl_pwm_dcc is 1     , */\
	{ 0x4e04, "ctrl_slopecur"},    /* for testing direct control slopecur               , */\
	{ 0x4e53, "ctrl_dem"},    /* dyn element matching control, rest of codes are optional, */\
	{ 0x4e93, "ctrl_demmismatch"},    /* dyn element matching add offset                   , */\
	{ 0x4ed0, "enbl_pwm_dcc"},    /* to enable direct control of pwm duty cycle        , */\
	{ 0x5007, "gain"},    /* gain setting of the gain multiplier gain need to increase with factor 1.41 (3dB), */\
	{ 0x5081, "ctrl_sourceb"},    /* Set OUTB to                                       , */\
	{ 0x50a1, "ctrl_sourcea"},    /* Set OUTA to                                       , */\
	{ 0x50c1, "ctrl_sourcebst"},    /* Sets the source of the pwmbst output to boost converter input for testing, */\
	{ 0x50e1, "ctrl_test_mono"},    /* ABIST mode to add both amplifier halfs as stereo or one amplifier half as mono, */\
	{ 0x5104, "pulselengthbst"},    /* pulselength setting test input for boost converter , */\
	{ 0x5150, "ctrl_bypasslatchbst"},    /* bypass_latch in boost converter                   , */\
	{ 0x5160, "invertbst"},    /* invert pwmbst test signal                         , */\
	{ 0x5174, "pulselength"},    /* pulselength setting test input for amplifier      , */\
	{ 0x51c0, "ctrl_bypasslatch"},    /* bypass_latch in boost convert                     , */\
	{ 0x51d0, "invertb"},    /* invert pwmb test signal                           , */\
	{ 0x51e0, "inverta"},    /* invert pwma test signal                           , */\
	{ 0x51f0, "ctrl_bypass_ctrlloop"},    /* bypass_ctrlloop bypasses the control loop of the amplifier, */\
	{ 0x5200, "ctrl_test_discrete"},    /* tbd for rdson testing                             , */\
	{ 0x5210, "ctrl_test_rdsona"},    /* tbd for rdson testing                             , */\
	{ 0x5220, "ctrl_test_rdsonb"},    /* tbd for rdson testing                             , */\
	{ 0x5230, "ctrl_test_rdsonbst"},    /* tbd for rdson testing                             , */\
	{ 0x5240, "ctrl_test_cvia"},    /* tbd for rdson testing                             , */\
	{ 0x5250, "ctrl_test_cvib"},    /* tbd for rdson testing                             , */\
	{ 0x5260, "ctrl_test_cvibst"},    /* tbd for rdson testing                             , */\
	{ 0x5290, "test_bypass_pwmdiscretea"},    /* for testing ( ABIST)                              , */\
	{ 0x52a0, "test_bypass_pwmdiscreteb"},    /* for testing ( ABIST)                              , */\
	{ 0x52b0, "ctrl_clipc_forcehigh"},    /* test signal for clipcontrol                       , */\
	{ 0x52c0, "ctrl_clipc_forcelow"},    /* test signal for clipcontrol                       , */\
	{ 0x52d0, "ctrl_test_sdelta"},    /* for testing ( ABIST)                              , */\
	{ 0x52e0, "ctrl_test_swhvp"},    /* for testing ( ABIST)                              , */\
	{ 0x52f0, "test_gain_reduction"},    /* test gain reduction                               , */\
	{ 0x5303, "ctrl_digimux_out_test1"},    /* Digimux TEST1 out                                 , */\
	{ 0x5343, "ctrl_digimux_out_test2"},    /* Digimux TEST2 out. output flag_clipa_low depending on cntr_bypassclip setting, */\
	{ 0x5383, "ctrl_digimux_out_data1"},    /* Digimux DATA1 out (output flag_clipb_high depending on cntr_bypassclip setting), */\
	{ 0x53c3, "ctrl_digimux_out_data3"},    /* Digimux DATA3 out  (output flag_clipx_x depending on cntr_bypassclip setting), */\
	{ 0x5400, "hs_mode"},    /* hs_mode, high speed mode I2C bus                  , */\
	{ 0x5412, "test_parametric_io"},    /* test_parametric_io for testing pads               , */\
	{ 0x5440, "enbl_ringo"},    /* enbl_ringo, for test purpose to check with ringo  , */\
	{ 0x5480, "ctrl_cliplevel"},    /* Clip level                                        , */\
	{ 0x5491, "ctrl_anamux_sel"},    /* anamux selection                                  , */\
	{ 0x54b0, "test_vdddsw_dio"},    /* to overrule the power switches for memory         , */\
	{ 0x54c0, "ctrl_bypass_diosw_ovp"},    /* To disable the overvoltage protection of vddd_dio_sw, */\
	{ 0x54d0, "test_vddd_sw"},    /* test vdd sw                                       , */\
	{ 0x54e0, "test_vddd_sw_comp"},    /* test vdd sw comp                                  , */\
	{ 0x550e, "enbl_amp"},    /* enbl_amp for testing to enable all analoge blocks in amplifier, */\
	{ 0x55f0, "fr_fsp"},    /* extr free running clock mode for testing          , */\
	{ 0x5600, "use_direct_ctrls"},    /* use_direct_ctrls, to overrule several functions direct for testing, */\
	{ 0x5610, "rst_datapath"},    /* rst_datapath, datapath reset                      , */\
	{ 0x5620, "rst_cgu"},    /* rst_cgu, cgu reset                                , */\
	{ 0x5637, "enbl_ref"},    /* for testing to enable all analoge blocks in references, */\
	{ 0x56b0, "enbl_engage"},    /* Enable output stage amplifier                     , */\
	{ 0x56c0, "use_direct_clk_ctrl"},    /* use_direct_clk_ctrl, to overrule several functions direct for testing, */\
	{ 0x56d0, "use_direct_pll_ctrl"},    /* use_direct_pll_ctrl, to overrule several functions direct for test, */\
	{ 0x56e0, "use_direct_ctrls_2"},    /* use_direct_sourseamp_ctrls, to overrule several functions direct for testing, */\
	{ 0x5707, "ctrl_anamux_out_test1"},    /* Anamux control                                    , */\
	{ 0x5782, "ctrl_zero"},    /* Bandwith control feedbackloop                     , */\
	{ 0x57b0, "enbl_ldo_stress"},    /* LDO stress function frinch capacitors             , */\
	{ 0x57c0, "ctrl_ocptest"},    /* ctrl_ocptest, deactivates the over current protection in the power stages of the amplifier. The ocp flag signals stay active., */\
	{ 0x57e0, "ctrl_otptest"},    /* otptest, test mode otp amplifier                  , */\
	{ 0x57f0, "ctrl_reverse"},    /* CTRL revers                                       , */\
	{ 0x5802, "pll_mdec_msb"},    /* most significant bits pll_mdec                    , */\
	{ 0x5833, "pll_selr"},    /* pll_selr                                          , */\
	{ 0x5874, "pll_selp"},    /* pll_selp                                          , */\
	{ 0x58c3, "pll_seli"},    /* pll_seli                                          , */\
	{ 0x5900, "pll_psel"},    /* pll_psel                                          , */\
	{ 0x5910, "use_direct_pll_psel"},    /* use_direct_pll_psel                               , */\
	{ 0x5923, "nbck"},    /* NBCK                                              , */\
	{ 0x5960, "auto_nbck"},    /* AUTO_NBCK                                         , */\
	{ 0x5970, "pll_frm"},    /* pll_frm                                           , */\
	{ 0x5980, "pll_directi"},    /* pll_directi                                       , */\
	{ 0x5990, "pll_directo"},    /* pll_directo                                       , */\
	{ 0x59a0, "enbl_PLL"},    /* enbl_PLL                                          , */\
	{ 0x59b0, "sel_clkout"},    /* SEL_CLKOUT                                        , */\
	{ 0x59e0, "fr_lost_clk"},    /* fr_lost_clk                                       , */\
	{ 0x59f0, "pll_bypass"},    /* pll_bypass                                        , */\
	{ 0x5a0f, "tsig_freq"},    /* tsig_freq, internal sinus test generator, frequency control, */\
	{ 0x5b02, "tsig_freq_msb"},    /* select internal sinus test generator, frequency control msb bits, */\
	{ 0x5b30, "inject_tsig"},    /* inject_tsig, control bit to switch to internal sinus test generator, */\
	{ 0x5b44, "ctrl_adc10_prog_sample"},    /* control ADC10                                     , */\
	{ 0x5c01, "pll_ndec_msb"},    /* most significant bits of pll_ndec                 , */\
	{ 0x5c2d, "pll_mdec"},    /* bits 13..0 of pll_mdec                            , */\
	{ 0x5d06, "pll_pdec"},    /* pll_pdec                                          , */\
	{ 0x5d87, "pll_ndec"},    /* bits 7..0 of pll_ndec                             , */\
	{ 0x5e00, "pdm_ch_sel_reg"},    /* PDM channel selection                             , */\
	{ 0x5e10, "pdm_iis_rst_reg"},    /* PDM Interface reset                               , */\
	{ 0x5e20, "clk_src_sel_reg"},    /* WS  Source Selection                              , */\
	{ 0x5e70, "pdm_resync_bypass"},    /* PDM resynchronization bypass                      , */\
	{ 0x6007, "MTP_key1"},    /* MTP Key1                                          , */\
	{ 0x6185, "mtp_ecc_tcin"},    /* Mtp_ecc_tcin                                      , */\
	{ 0x6203, "mtp_man_address_in"},    /* address from i2cregs for writing one word single mtp, */\
	{ 0x6260, "mtp_ecc_eeb"},    /* enable code bit generation (active low!)          , */\
	{ 0x6270, "mtp_ecc_ecb"},    /* enable correction signal (active low!)            , */\
	{ 0x6280, "man_copy_mtp_to_iic"},    /* start copying single word from mtp to i2cregs_mtp , */\
	{ 0x6290, "man_copy_iic_to_mtp"},    /* start copying single word from i2cregs_mtp to mtp [Key 1 protected], */\
	{ 0x62a0, "auto_copy_mtp_to_iic"},    /* start copying all the data from mtp to i2cregs_mtp, */\
	{ 0x62b0, "auto_copy_iic_to_mtp"},    /* start copying all the data from i2cregs_mtp to mtp [Key 2 protected], */\
	{ 0x62d2, "mtp_speed_mode"},    /* Speed mode                                        , */\
	{ 0x6340, "mtp_dircet_enable"},    /* mtp_direct_enable (key1 protected)                , */\
	{ 0x6350, "mtp_direct_wr"},    /* mtp_direct_wr (key1 protected) direct value for mtp pin wr. To be enabled via iic2mtp_mtp_direct_enable, */\
	{ 0x6360, "mtp_direct_rd"},    /* mtp_direct_rd  (key1 protected) direct value for mtp pin rd. To be enabled via iic2mtp_mtp_direct_enable, */\
	{ 0x6370, "mtp_direct_rst"},    /* mtp_direct_rst  (key1 protected) direct value for mtp pin rst. To be enabled via iic2mtp_mtp_direct_enable, */\
	{ 0x6380, "mtp_direct_ers"},    /* mtp_direct_ers  (key1 protected) direct value for mtp pin ers. To be enabled via iic2mtp_mtp_direct_enable, */\
	{ 0x6390, "mtp_direct_prg"},    /* mtp_direct_prg  (key1 protected) direct value for mtp pin prg. To be enabled via iic2mtp_mtp_direct_enable, */\
	{ 0x63a0, "mtp_direct_epp"},    /* mtp_direct_epp  (key1 protected) direct value for mtp pin epp. To be enabled via iic2mtp_mtp_direct_enable, */\
	{ 0x63b4, "mtp_direct_test"},    /* mtp_direct_test  (key1 protected)                 , */\
	{ 0x640f, "mtp_man_data_in"},    /* single wordt be written to MTP (manual copy)      , */\
	{ 0x7000, "cf_rst_dsp"},    /* Reset CoolFlux DSP                                , */\
	{ 0x7011, "cf_dmem"},    /* Target memory for access                          , */\
	{ 0x7030, "cf_aif"},    /* Autoincrement-flag for memory-address             , */\
	{ 0x7040, "cf_int"},    /* Interrupt CoolFlux DSP                            , */\
	{ 0x7087, "cf_req"},    /* request for access (8 channels)                   , */\
	{ 0x710f, "cf_madd"},    /* memory-address to be accessed                     , */\
	{ 0x720f, "cf_mema"},    /* activate memory access (24- or 32-bits data is written/read to/from memory, */\
	{ 0x7307, "cf_err"},    /* cf error Flags                                    , */\
	{ 0x7387, "cf_ack"},    /* acknowledge of requests (8 channels")"            , */\
	{ 0x8000, "calibration_onetime"},    /* Calibration schedule (key2 protected)             , */\
	{ 0x8010, "calibr_ron_done"},    /* (key2 protected) calibration of Ron has been executed, */\
	{ 0x8105, "calibr_vout_offset"},    /* calibr_vout_offset (DCDCoffset) 2's compliment (key1 protected), */\
	{ 0x8163, "calibr_delta_gain"},    /* delta gain for vamp (alpha) 2's compliment (key1 protected), */\
	{ 0x81a5, "calibr_offs_amp"},    /* offset for vamp (Ampoffset) 2's compliment (key1 protected), */\
	{ 0x8207, "calibr_gain_cs"},    /* gain current sense (Imeasalpha) 2's compliment (key1 protected), */\
	{ 0x8284, "calibr_temp_offset"},    /* temperature offset 2's compliment (key1 protected), */\
	{ 0x82d2, "calibr_temp_gain"},    /* temperature gain 2's compliment (key1 protected)  , */\
	{ 0x830f, "calibr_ron"},    /* Ron resistance of coil (key1 protected)           , */\
	{ 0x8406, "ctrl_offset_a"},    /* Offset of amplifier level shifter                 , */\
	{ 0x8486, "ctrl_offset_b"},    /* Offset of amplifier level shifter                 , */\
	{ 0x850f, "type_bits_HW"},    /* HW Bits                                           , */\
	{ 0x860f, "type_bits1_SW"},    /* MTP-control SW1                                   , */\
	{ 0x870f, "type_bits2_SW"},    /* MTP-control SW2                                   , */\
	{ 0x8a0f, "production_data1"},    /* (key1 protected)                                  , */\
	{ 0x8b0f, "production_data2"},    /* (key1 protected)                                  , */\
	{ 0x8c0f, "production_data3"},    /* (key1 protected)                                  , */\
	{ 0x8d0f, "production_data4"},    /* (key1 protected)                                  , */\
	{ 0x8e0f, "production_data5"},    /* (key1 protected)                                  , */\
	{ 0x8f0f, "production_data6"},    /* (key1 protected)                                  , */\
	{ 0xffff, "Unknown bitfield enum" }    /* not found */\
};


#endif /* TFA_INC_TFA9891_TFAFIELDNAMES_H_ */
