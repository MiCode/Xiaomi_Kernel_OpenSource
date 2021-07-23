/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _TFA9896_TFAFIELDNAMES_H
#define _TFA9896_TFAFIELDNAMES_H
#define TFA9896_I2CVERSION 16
enum nxpTFA9896BfEnumList {
	TFA9896_BF_VDDS = 0x0000,
	TFA9896_BF_PLLS = 0x0010,
	TFA9896_BF_OTDS = 0x0020,
	TFA9896_BF_OVDS = 0x0030,
	TFA9896_BF_UVDS = 0x0040,
	TFA9896_BF_OCDS = 0x0050,
	TFA9896_BF_CLKS = 0x0060,
	TFA9896_BF_CLIPS = 0x0070,
	TFA9896_BF_MTPB = 0x0080,
	TFA9896_BF_NOCLK = 0x0090,
	TFA9896_BF_SPKS = 0x00a0,
	TFA9896_BF_ACS = 0x00b0,
	TFA9896_BF_SWS = 0x00c0,
	TFA9896_BF_WDS = 0x00d0,
	TFA9896_BF_AMPS = 0x00e0,
	TFA9896_BF_AREFS = 0x00f0,
	TFA9896_BF_BATS = 0x0109,
	TFA9896_BF_TEMPS = 0x0208,
	TFA9896_BF_REV = 0x030f,
	TFA9896_BF_RCV = 0x0420,
	TFA9896_BF_CHS12 = 0x0431,
	TFA9896_BF_INPLVL = 0x0450,
	TFA9896_BF_CHSA = 0x0461,
	TFA9896_BF_AUDFS = 0x04c3,
	TFA9896_BF_BSSCR = 0x0501,
	TFA9896_BF_BSST = 0x0523,
	TFA9896_BF_BSSRL = 0x0561,
	TFA9896_BF_BSSRR = 0x0582,
	TFA9896_BF_BSSHY = 0x05b1,
	TFA9896_BF_BSSR = 0x05e0,
	TFA9896_BF_BSSBY = 0x05f0,
	TFA9896_BF_DPSA = 0x0600,
	TFA9896_BF_ATTEN = 0x0613,
	TFA9896_BF_CFSM = 0x0650,
	TFA9896_BF_BSSS = 0x0670,
	TFA9896_BF_VOL = 0x0687,
	TFA9896_BF_DCVO2 = 0x0702,
	TFA9896_BF_DCMCC = 0x0733,
	TFA9896_BF_DCVO1 = 0x0772,
	TFA9896_BF_DCIE = 0x07a0,
	TFA9896_BF_DCSR = 0x07b0,
	TFA9896_BF_DCPAVG = 0x07c0,
	TFA9896_BF_DCPWM = 0x07d0,
	TFA9896_BF_TROS = 0x0800,
	TFA9896_BF_EXTTS = 0x0818,
	TFA9896_BF_PWDN = 0x0900,
	TFA9896_BF_I2CR = 0x0910,
	TFA9896_BF_CFE = 0x0920,
	TFA9896_BF_AMPE = 0x0930,
	TFA9896_BF_DCA = 0x0940,
	TFA9896_BF_SBSL = 0x0950,
	TFA9896_BF_AMPC = 0x0960,
	TFA9896_BF_DCDIS = 0x0970,
	TFA9896_BF_PSDR = 0x0980,
	TFA9896_BF_INTPAD = 0x09c1,
	TFA9896_BF_IPLL = 0x09e0,
	TFA9896_BF_DCTRIP = 0x0a04,
	TFA9896_BF_DCHOLD = 0x0a54,
	TFA9896_BF_MTPK = 0x0b07,
	TFA9896_BF_CVFDLY = 0x0c25,
	TFA9896_BF_OPENMTP = 0x0ec0,
	TFA9896_BF_TDMPRF = 0x1011,
	TFA9896_BF_TDMEN = 0x1030,
	TFA9896_BF_TDMCKINV = 0x1040,
	TFA9896_BF_TDMFSLN = 0x1053,
	TFA9896_BF_TDMFSPOL = 0x1090,
	TFA9896_BF_TDMSAMSZ = 0x10a4,
	TFA9896_BF_TDMSLOTS = 0x1103,
	TFA9896_BF_TDMSLLN = 0x1144,
	TFA9896_BF_TDMBRMG = 0x1194,
	TFA9896_BF_TDMDDEL = 0x11e0,
	TFA9896_BF_TDMDADJ = 0x11f0,
	TFA9896_BF_TDMTXFRM = 0x1201,
	TFA9896_BF_TDMUUS0 = 0x1221,
	TFA9896_BF_TDMUUS1 = 0x1241,
	TFA9896_BF_TDMSI0EN = 0x1270,
	TFA9896_BF_TDMSI1EN = 0x1280,
	TFA9896_BF_TDMSI2EN = 0x1290,
	TFA9896_BF_TDMSO0EN = 0x12a0,
	TFA9896_BF_TDMSO1EN = 0x12b0,
	TFA9896_BF_TDMSO2EN = 0x12c0,
	TFA9896_BF_TDMSI0IO = 0x12d0,
	TFA9896_BF_TDMSI1IO = 0x12e0,
	TFA9896_BF_TDMSI2IO = 0x12f0,
	TFA9896_BF_TDMSO0IO = 0x1300,
	TFA9896_BF_TDMSO1IO = 0x1310,
	TFA9896_BF_TDMSO2IO = 0x1320,
	TFA9896_BF_TDMSI0SL = 0x1333,
	TFA9896_BF_TDMSI1SL = 0x1373,
	TFA9896_BF_TDMSI2SL = 0x13b3,
	TFA9896_BF_TDMSO0SL = 0x1403,
	TFA9896_BF_TDMSO1SL = 0x1443,
	TFA9896_BF_TDMSO2SL = 0x1483,
	TFA9896_BF_NBCK = 0x14c3,
	TFA9896_BF_INTOVDDS = 0x2000,
	TFA9896_BF_INTOPLLS = 0x2010,
	TFA9896_BF_INTOOTDS = 0x2020,
	TFA9896_BF_INTOOVDS = 0x2030,
	TFA9896_BF_INTOUVDS = 0x2040,
	TFA9896_BF_INTOOCDS = 0x2050,
	TFA9896_BF_INTOCLKS = 0x2060,
	TFA9896_BF_INTOCLIPS = 0x2070,
	TFA9896_BF_INTOMTPB = 0x2080,
	TFA9896_BF_INTONOCLK = 0x2090,
	TFA9896_BF_INTOSPKS = 0x20a0,
	TFA9896_BF_INTOACS = 0x20b0,
	TFA9896_BF_INTOSWS = 0x20c0,
	TFA9896_BF_INTOWDS = 0x20d0,
	TFA9896_BF_INTOAMPS = 0x20e0,
	TFA9896_BF_INTOAREFS = 0x20f0,
	TFA9896_BF_INTOERR = 0x2200,
	TFA9896_BF_INTOACK = 0x2210,
	TFA9896_BF_INTIVDDS = 0x2300,
	TFA9896_BF_INTIPLLS = 0x2310,
	TFA9896_BF_INTIOTDS = 0x2320,
	TFA9896_BF_INTIOVDS = 0x2330,
	TFA9896_BF_INTIUVDS = 0x2340,
	TFA9896_BF_INTIOCDS = 0x2350,
	TFA9896_BF_INTICLKS = 0x2360,
	TFA9896_BF_INTICLIPS = 0x2370,
	TFA9896_BF_INTIMTPB = 0x2380,
	TFA9896_BF_INTINOCLK = 0x2390,
	TFA9896_BF_INTISPKS = 0x23a0,
	TFA9896_BF_INTIACS = 0x23b0,
	TFA9896_BF_INTISWS = 0x23c0,
	TFA9896_BF_INTIWDS = 0x23d0,
	TFA9896_BF_INTIAMPS = 0x23e0,
	TFA9896_BF_INTIAREFS = 0x23f0,
	TFA9896_BF_INTIERR = 0x2500,
	TFA9896_BF_INTIACK = 0x2510,
	TFA9896_BF_INTENVDDS = 0x2600,
	TFA9896_BF_INTENPLLS = 0x2610,
	TFA9896_BF_INTENOTDS = 0x2620,
	TFA9896_BF_INTENOVDS = 0x2630,
	TFA9896_BF_INTENUVDS = 0x2640,
	TFA9896_BF_INTENOCDS = 0x2650,
	TFA9896_BF_INTENCLKS = 0x2660,
	TFA9896_BF_INTENCLIPS = 0x2670,
	TFA9896_BF_INTENMTPB = 0x2680,
	TFA9896_BF_INTENNOCLK = 0x2690,
	TFA9896_BF_INTENSPKS = 0x26a0,
	TFA9896_BF_INTENACS = 0x26b0,
	TFA9896_BF_INTENSWS = 0x26c0,
	TFA9896_BF_INTENWDS = 0x26d0,
	TFA9896_BF_INTENAMPS = 0x26e0,
	TFA9896_BF_INTENAREFS = 0x26f0,
	TFA9896_BF_INTENERR = 0x2800,
	TFA9896_BF_INTENACK = 0x2810,
	TFA9896_BF_INTPOLVDDS = 0x2900,
	TFA9896_BF_INTPOLPLLS = 0x2910,
	TFA9896_BF_INTPOLOTDS = 0x2920,
	TFA9896_BF_INTPOLOVDS = 0x2930,
	TFA9896_BF_INTPOLUVDS = 0x2940,
	TFA9896_BF_INTPOLOCDS = 0x2950,
	TFA9896_BF_INTPOLCLKS = 0x2960,
	TFA9896_BF_INTPOLCLIPS = 0x2970,
	TFA9896_BF_INTPOLMTPB = 0x2980,
	TFA9896_BF_INTPOLNOCLK = 0x2990,
	TFA9896_BF_INTPOLSPKS = 0x29a0,
	TFA9896_BF_INTPOLACS = 0x29b0,
	TFA9896_BF_INTPOLSWS = 0x29c0,
	TFA9896_BF_INTPOLWDS = 0x29d0,
	TFA9896_BF_INTPOLAMPS = 0x29e0,
	TFA9896_BF_INTPOLAREFS = 0x29f0,
	TFA9896_BF_INTPOLERR = 0x2b00,
	TFA9896_BF_INTPOLACK = 0x2b10,
	TFA9896_BF_CLIP = 0x4900,
	TFA9896_BF_CIMTP = 0x62b0,
	TFA9896_BF_RST = 0x7000,
	TFA9896_BF_DMEM = 0x7011,
	TFA9896_BF_AIF = 0x7030,
	TFA9896_BF_CFINT = 0x7040,
	TFA9896_BF_REQ = 0x7087,
	TFA9896_BF_MADD = 0x710f,
	TFA9896_BF_MEMA = 0x720f,
	TFA9896_BF_ERR = 0x7307,
	TFA9896_BF_ACK = 0x7387,
	TFA9896_BF_MTPOTC = 0x8000,
	TFA9896_BF_MTPEX = 0x8010,
};
#define TFA9896_NAMETABLE                                                     \
	static struct TfaBfName Tfa9896DatasheetNames[] = {                   \
		{ 0x0, "VDDS" },	  { 0x10, "PLLS" },                   \
		{ 0x20, "OTDS" },	 { 0x30, "OVDS" },                   \
		{ 0x40, "UVDS" },	 { 0x50, "OCDS" },                   \
		{ 0x60, "CLKS" },	 { 0x70, "CLIPS" },                  \
		{ 0x80, "MTPB" },	 { 0x90, "NOCLK" },                  \
		{ 0xa0, "SPKS" },	 { 0xb0, "ACS" },                    \
		{ 0xc0, "SWS" },	  { 0xd0, "WDS" },                    \
		{ 0xe0, "AMPS" },	 { 0xf0, "AREFS" },                  \
		{ 0x109, "BATS" },	{ 0x208, "TEMPS" },                 \
		{ 0x30f, "REV" },	 { 0x420, "RCV" },                   \
		{ 0x431, "CHS12" },       { 0x450, "INPLVL" },                \
		{ 0x461, "CHSA" },	{ 0x4c3, "AUDFS" },                 \
		{ 0x501, "BSSCR" },       { 0x523, "BSST" },                  \
		{ 0x561, "BSSRL" },       { 0x582, "BSSRR" },                 \
		{ 0x5b1, "BSSHY" },       { 0x5e0, "BSSR" },                  \
		{ 0x5f0, "BSSBY" },       { 0x600, "DPSA" },                  \
		{ 0x613, "ATTEN" },       { 0x650, "CFSM" },                  \
		{ 0x670, "BSSS" },	{ 0x687, "VOL" },                   \
		{ 0x702, "DCVO2" },       { 0x733, "DCMCC" },                 \
		{ 0x772, "DCVO1" },       { 0x7a0, "DCIE" },                  \
		{ 0x7b0, "DCSR" },	{ 0x7c0, "DCPAVG" },                \
		{ 0x7d0, "DCPWM" },       { 0x800, "TROS" },                  \
		{ 0x818, "EXTTS" },       { 0x900, "PWDN" },                  \
		{ 0x910, "I2CR" },	{ 0x920, "CFE" },                   \
		{ 0x930, "AMPE" },	{ 0x940, "DCA" },                   \
		{ 0x950, "SBSL" },	{ 0x960, "AMPC" },                  \
		{ 0x970, "DCDIS" },       { 0x980, "PSDR" },                  \
		{ 0x9c1, "INTPAD" },      { 0x9e0, "IPLL" },                  \
		{ 0xa04, "DCTRIP" },      { 0xa54, "DCHOLD" },                \
		{ 0xb07, "MTPK" },	{ 0xc25, "CVFDLY" },                \
		{ 0xec0, "OPENMTP" },     { 0x1011, "TDMPRF" },               \
		{ 0x1030, "TDMEN" },      { 0x1040, "TDMCKINV" },             \
		{ 0x1053, "TDMFSLN" },    { 0x1090, "TDMFSPOL" },             \
		{ 0x10a4, "TDMSAMSZ" },   { 0x1103, "TDMSLOTS" },             \
		{ 0x1144, "TDMSLLN" },    { 0x1194, "TDMBRMG" },              \
		{ 0x11e0, "TDMDDEL" },    { 0x11f0, "TDMDADJ" },              \
		{ 0x1201, "TDMTXFRM" },   { 0x1221, "TDMUUS0" },              \
		{ 0x1241, "TDMUUS1" },    { 0x1270, "TDMSI0EN" },             \
		{ 0x1280, "TDMSI1EN" },   { 0x1290, "TDMSI2EN" },             \
		{ 0x12a0, "TDMSO0EN" },   { 0x12b0, "TDMSO1EN" },             \
		{ 0x12c0, "TDMSO2EN" },   { 0x12d0, "TDMSI0IO" },             \
		{ 0x12e0, "TDMSI1IO" },   { 0x12f0, "TDMSI2IO" },             \
		{ 0x1300, "TDMSO0IO" },   { 0x1310, "TDMSO1IO" },             \
		{ 0x1320, "TDMSO2IO" },   { 0x1333, "TDMSI0SL" },             \
		{ 0x1373, "TDMSI1SL" },   { 0x13b3, "TDMSI2SL" },             \
		{ 0x1403, "TDMSO0SL" },   { 0x1443, "TDMSO1SL" },             \
		{ 0x1483, "TDMSO2SL" },   { 0x14c3, "NBCK" },                 \
		{ 0x2000, "INTOVDDS" },   { 0x2010, "INTOPLLS" },             \
		{ 0x2020, "INTOOTDS" },   { 0x2030, "INTOOVDS" },             \
		{ 0x2040, "INTOUVDS" },   { 0x2050, "INTOOCDS" },             \
		{ 0x2060, "INTOCLKS" },   { 0x2070, "INTOCLIPS" },            \
		{ 0x2080, "INTOMTPB" },   { 0x2090, "INTONOCLK" },            \
		{ 0x20a0, "INTOSPKS" },   { 0x20b0, "INTOACS" },              \
		{ 0x20c0, "INTOSWS" },    { 0x20d0, "INTOWDS" },              \
		{ 0x20e0, "INTOAMPS" },   { 0x20f0, "INTOAREFS" },            \
		{ 0x2200, "INTOERR" },    { 0x2210, "INTOACK" },              \
		{ 0x2300, "INTIVDDS" },   { 0x2310, "INTIPLLS" },             \
		{ 0x2320, "INTIOTDS" },   { 0x2330, "INTIOVDS" },             \
		{ 0x2340, "INTIUVDS" },   { 0x2350, "INTIOCDS" },             \
		{ 0x2360, "INTICLKS" },   { 0x2370, "INTICLIPS" },            \
		{ 0x2380, "INTIMTPB" },   { 0x2390, "INTINOCLK" },            \
		{ 0x23a0, "INTISPKS" },   { 0x23b0, "INTIACS" },              \
		{ 0x23c0, "INTISWS" },    { 0x23d0, "INTIWDS" },              \
		{ 0x23e0, "INTIAMPS" },   { 0x23f0, "INTIAREFS" },            \
		{ 0x2500, "INTIERR" },    { 0x2510, "INTIACK" },              \
		{ 0x2600, "INTENVDDS" },  { 0x2610, "INTENPLLS" },            \
		{ 0x2620, "INTENOTDS" },  { 0x2630, "INTENOVDS" },            \
		{ 0x2640, "INTENUVDS" },  { 0x2650, "INTENOCDS" },            \
		{ 0x2660, "INTENCLKS" },  { 0x2670, "INTENCLIPS" },           \
		{ 0x2680, "INTENMTPB" },  { 0x2690, "INTENNOCLK" },           \
		{ 0x26a0, "INTENSPKS" },  { 0x26b0, "INTENACS" },             \
		{ 0x26c0, "INTENSWS" },   { 0x26d0, "INTENWDS" },             \
		{ 0x26e0, "INTENAMPS" },  { 0x26f0, "INTENAREFS" },           \
		{ 0x2800, "INTENERR" },   { 0x2810, "INTENACK" },             \
		{ 0x2900, "INTPOLVDDS" }, { 0x2910, "INTPOLPLLS" },           \
		{ 0x2920, "INTPOLOTDS" }, { 0x2930, "INTPOLOVDS" },           \
		{ 0x2940, "INTPOLUVDS" }, { 0x2950, "INTPOLOCDS" },           \
		{ 0x2960, "INTPOLCLKS" }, { 0x2970, "INTPOLCLIPS" },          \
		{ 0x2980, "INTPOLMTPB" }, { 0x2990, "INTPOLNOCLK" },          \
		{ 0x29a0, "INTPOLSPKS" }, { 0x29b0, "INTPOLACS" },            \
		{ 0x29c0, "INTPOLSWS" },  { 0x29d0, "INTPOLWDS" },            \
		{ 0x29e0, "INTPOLAMPS" }, { 0x29f0, "INTPOLAREFS" },          \
		{ 0x2b00, "INTPOLERR" },  { 0x2b10, "INTPOLACK" },            \
		{ 0x4900, "CLIP" },       { 0x62b0, "CIMTP" },                \
		{ 0x7000, "RST" },	{ 0x7011, "DMEM" },                 \
		{ 0x7030, "AIF" },	{ 0x7040, "CFINT" },                \
		{ 0x7087, "REQ" },	{ 0x710f, "MADD" },                 \
		{ 0x720f, "MEMA" },       { 0x7307, "ERR" },                  \
		{ 0x7387, "ACK" },	{ 0x8000, "MTPOTC" },               \
		{ 0x8010, "MTPEX" },      { 0x8045, "SWPROFIL" },             \
		{ 0x80a5, "SWVSTEP" },    { 0xffff, "Unknown bitfield enum" } \
	}
#define TFA9896_BITNAMETABLE                                     \
	static struct TfaBfName Tfa9896BitNames[] = {            \
		{ 0x0, "flag_por" },                             \
		{ 0x10, "flag_pll_lock" },                       \
		{ 0x20, "flag_otpok" },                          \
		{ 0x30, "flag_ovpok" },                          \
		{ 0x40, "flag_uvpok" },                          \
		{ 0x50, "flag_ocp_alarm" },                      \
		{ 0x60, "flag_clocks_stable" },                  \
		{ 0x70, "flag_clip" },                           \
		{ 0x80, "mtp_busy" },                            \
		{ 0x90, "flag_lost_clk" },                       \
		{ 0xa0, "flag_cf_speakererror" },                \
		{ 0xb0, "flag_cold_started" },                   \
		{ 0xc0, "flag_engage" },                         \
		{ 0xd0, "flag_watchdog_reset" },                 \
		{ 0xe0, "flag_enbl_amp" },                       \
		{ 0xf0, "flag_enbl_ref" },                       \
		{ 0x109, "bat_adc" },                            \
		{ 0x208, "temp_adc" },                           \
		{ 0x30f, "device_rev" },                         \
		{ 0x420, "ctrl_rcv" },                           \
		{ 0x431, "chan_sel" },                           \
		{ 0x450, "input_level" },                        \
		{ 0x461, "vamp_sel" },                           \
		{ 0x4c3, "audio_fs" },                           \
		{ 0x501, "vbat_prot_attacktime" },               \
		{ 0x523, "vbat_prot_thlevel" },                  \
		{ 0x561, "vbat_prot_max_reduct" },               \
		{ 0x582, "vbat_prot_release_t" },                \
		{ 0x5b1, "vbat_prot_hysterese" },                \
		{ 0x5d0, "reset_min_vbat" },                     \
		{ 0x5e0, "sel_vbat" },                           \
		{ 0x5f0, "bypass_clipper" },                     \
		{ 0x600, "dpsa" },                               \
		{ 0x613, "ctrl_att" },                           \
		{ 0x650, "cf_mute" },                            \
		{ 0x670, "batsense_steepness" },                 \
		{ 0x687, "vol" },                                \
		{ 0x702, "scnd_boost_voltage" },                 \
		{ 0x733, "boost_cur" },                          \
		{ 0x772, "frst_boost_voltage" },                 \
		{ 0x7a0, "boost_intel" },                        \
		{ 0x7b0, "boost_speed" },                        \
		{ 0x7c0, "boost_peak2avg" },                     \
		{ 0x7d0, "dcdc_pwmonly" },                       \
		{ 0x7e0, "ignore_flag_voutcomp86" },             \
		{ 0x800, "ext_temp_sel" },                       \
		{ 0x818, "ext_temp" },                           \
		{ 0x900, "powerdown" },                          \
		{ 0x910, "reset" },                              \
		{ 0x920, "enbl_coolflux" },                      \
		{ 0x930, "enbl_amplifier" },                     \
		{ 0x940, "enbl_boost" },                         \
		{ 0x950, "coolflux_configured" },                \
		{ 0x960, "sel_enbl_amplifier" },                 \
		{ 0x970, "dcdcoff_mode" },                       \
		{ 0x980, "iddqtest" },                           \
		{ 0x9c1, "int_pad_io" },                         \
		{ 0x9e0, "sel_fs_bck" },                         \
		{ 0x9f0, "sel_scl_cf_clock" },                   \
		{ 0xa04, "boost_trip_lvl" },                     \
		{ 0xa54, "boost_hold_time" },                    \
		{ 0xaa1, "bst_slpcmplvl" },                      \
		{ 0xb07, "mtpkey2" },                            \
		{ 0xc00, "enbl_volt_sense" },                    \
		{ 0xc10, "vsense_pwm_sel" },                     \
		{ 0xc25, "vi_frac_delay" },                      \
		{ 0xc80, "sel_voltsense_out" },                  \
		{ 0xc90, "vsense_bypass_avg" },                  \
		{ 0xd05, "cf_frac_delay" },                      \
		{ 0xe00, "bypass_dcdc_curr_prot" },              \
		{ 0xe10, "bypass_ocp" },                         \
		{ 0xe20, "ocptest" },                            \
		{ 0xe80, "disable_clock_sh_prot" },              \
		{ 0xe92, "reserve_reg_15_09" },                  \
		{ 0xec0, "unprotect_mtp" },                      \
		{ 0xed2, "reserve_reg_15_13" },                  \
		{ 0xf00, "dcdc_pfm20khz_limit" },                \
		{ 0xf11, "dcdc_ctrl_maxzercnt" },                \
		{ 0xf36, "dcdc_vbat_delta_detect" },             \
		{ 0xfa0, "dcdc_ignore_vbat" },                   \
		{ 0x1011, "tdm_usecase" },                       \
		{ 0x1030, "tdm_enable" },                        \
		{ 0x1040, "tdm_clk_inversion" },                 \
		{ 0x1053, "tdm_fs_ws_length" },                  \
		{ 0x1090, "tdm_fs_ws_polarity" },                \
		{ 0x10a4, "tdm_sample_size" },                   \
		{ 0x1103, "tdm_nb_of_slots" },                   \
		{ 0x1144, "tdm_slot_length" },                   \
		{ 0x1194, "tdm_bits_remaining" },                \
		{ 0x11e0, "tdm_data_delay" },                    \
		{ 0x11f0, "tdm_data_adjustment" },               \
		{ 0x1201, "tdm_txdata_format" },                 \
		{ 0x1221, "tdm_txdata_format_unused_slot_sd0" }, \
		{ 0x1241, "tdm_txdata_format_unused_slot_sd1" }, \
		{ 0x1270, "tdm_sink0_enable" },                  \
		{ 0x1280, "tdm_sink1_enable" },                  \
		{ 0x1290, "tdm_sink2_enable" },                  \
		{ 0x12a0, "tdm_source0_enable" },                \
		{ 0x12b0, "tdm_source1_enable" },                \
		{ 0x12c0, "tdm_source2_enable" },                \
		{ 0x12d0, "tdm_sink0_io" },                      \
		{ 0x12e0, "tdm_sink1_io" },                      \
		{ 0x12f0, "tdm_sink2_io" },                      \
		{ 0x1300, "tdm_source0_io" },                    \
		{ 0x1310, "tdm_source1_io" },                    \
		{ 0x1320, "tdm_source2_io" },                    \
		{ 0x1333, "tdm_sink0_slot" },                    \
		{ 0x1373, "tdm_sink1_slot" },                    \
		{ 0x13b3, "tdm_sink2_slot" },                    \
		{ 0x1403, "tdm_source0_slot" },                  \
		{ 0x1443, "tdm_source1_slot" },                  \
		{ 0x1483, "tdm_source2_slot" },                  \
		{ 0x14c3, "tdm_nbck" },                          \
		{ 0x1500, "flag_tdm_lut_error" },                \
		{ 0x1512, "flag_tdm_status" },                   \
		{ 0x1540, "flag_tdm_error" },                    \
		{ 0x1551, "status_bst_mode" },                   \
		{ 0x2000, "flag_por_int_out" },                  \
		{ 0x2010, "flag_pll_lock_int_out" },             \
		{ 0x2020, "flag_otpok_int_out" },                \
		{ 0x2030, "flag_ovpok_int_out" },                \
		{ 0x2040, "flag_uvpok_int_out" },                \
		{ 0x2050, "flag_ocp_alarm_int_out" },            \
		{ 0x2060, "flag_clocks_stable_int_out" },        \
		{ 0x2070, "flag_clip_int_out" },                 \
		{ 0x2080, "mtp_busy_int_out" },                  \
		{ 0x2090, "flag_lost_clk_int_out" },             \
		{ 0x20a0, "flag_cf_speakererror_int_out" },      \
		{ 0x20b0, "flag_cold_started_int_out" },         \
		{ 0x20c0, "flag_engage_int_out" },               \
		{ 0x20d0, "flag_watchdog_reset_int_out" },       \
		{ 0x20e0, "flag_enbl_amp_int_out" },             \
		{ 0x20f0, "flag_enbl_ref_int_out" },             \
		{ 0x2100, "flag_voutcomp_int_out" },             \
		{ 0x2110, "flag_voutcomp93_int_out" },           \
		{ 0x2120, "flag_voutcomp86_int_out" },           \
		{ 0x2130, "flag_hiz_int_out" },                  \
		{ 0x2140, "flag_ocpokbst_int_out" },             \
		{ 0x2150, "flag_peakcur_int_out" },              \
		{ 0x2160, "flag_ocpokap_int_out" },              \
		{ 0x2170, "flag_ocpokan_int_out" },              \
		{ 0x2180, "flag_ocpokbp_int_out" },              \
		{ 0x2190, "flag_ocpokbn_int_out" },              \
		{ 0x21a0, "flag_adc10_ready_int_out" },          \
		{ 0x21b0, "flag_clipa_high_int_out" },           \
		{ 0x21c0, "flag_clipa_low_int_out" },            \
		{ 0x21d0, "flag_clipb_high_int_out" },           \
		{ 0x21e0, "flag_clipb_low_int_out" },            \
		{ 0x21f0, "flag_tdm_error_int_out" },            \
		{ 0x2200, "flag_cfma_err_int_out" },             \
		{ 0x2210, "flag_cfma_ack_int_out" },             \
		{ 0x2300, "flag_por_int_in" },                   \
		{ 0x2310, "flag_pll_lock_int_in" },              \
		{ 0x2320, "flag_otpok_int_in" },                 \
		{ 0x2330, "flag_ovpok_int_in" },                 \
		{ 0x2340, "flag_uvpok_int_in" },                 \
		{ 0x2350, "flag_ocp_alarm_int_in" },             \
		{ 0x2360, "flag_clocks_stable_int_in" },         \
		{ 0x2370, "flag_clip_int_in" },                  \
		{ 0x2380, "mtp_busy_int_in" },                   \
		{ 0x2390, "flag_lost_clk_int_in" },              \
		{ 0x23a0, "flag_cf_speakererror_int_in" },       \
		{ 0x23b0, "flag_cold_started_int_in" },          \
		{ 0x23c0, "flag_engage_int_in" },                \
		{ 0x23d0, "flag_watchdog_reset_int_in" },        \
		{ 0x23e0, "flag_enbl_amp_int_in" },              \
		{ 0x23f0, "flag_enbl_ref_int_in" },              \
		{ 0x2400, "flag_voutcomp_int_in" },              \
		{ 0x2410, "flag_voutcomp93_int_in" },            \
		{ 0x2420, "flag_voutcomp86_int_in" },            \
		{ 0x2430, "flag_hiz_int_in" },                   \
		{ 0x2440, "flag_ocpokbst_int_in" },              \
		{ 0x2450, "flag_peakcur_int_in" },               \
		{ 0x2460, "flag_ocpokap_int_in" },               \
		{ 0x2470, "flag_ocpokan_int_in" },               \
		{ 0x2480, "flag_ocpokbp_int_in" },               \
		{ 0x2490, "flag_ocpokbn_int_in" },               \
		{ 0x24a0, "flag_adc10_ready_int_in" },           \
		{ 0x24b0, "flag_clipa_high_int_in" },            \
		{ 0x24c0, "flag_clipa_low_int_in" },             \
		{ 0x24d0, "flag_clipb_high_int_in" },            \
		{ 0x24e0, "flag_clipb_low_int_in" },             \
		{ 0x24f0, "flag_tdm_error_int_in" },             \
		{ 0x2500, "flag_cfma_err_int_in" },              \
		{ 0x2510, "flag_cfma_ack_int_in" },              \
		{ 0x2600, "flag_por_int_enable" },               \
		{ 0x2610, "flag_pll_lock_int_enable" },          \
		{ 0x2620, "flag_otpok_int_enable" },             \
		{ 0x2630, "flag_ovpok_int_enable" },             \
		{ 0x2640, "flag_uvpok_int_enable" },             \
		{ 0x2650, "flag_ocp_alarm_int_enable" },         \
		{ 0x2660, "flag_clocks_stable_int_enable" },     \
		{ 0x2670, "flag_clip_int_enable" },              \
		{ 0x2680, "mtp_busy_int_enable" },               \
		{ 0x2690, "flag_lost_clk_int_enable" },          \
		{ 0x26a0, "flag_cf_speakererror_int_enable" },   \
		{ 0x26b0, "flag_cold_started_int_enable" },      \
		{ 0x26c0, "flag_engage_int_enable" },            \
		{ 0x26d0, "flag_watchdog_reset_int_enable" },    \
		{ 0x26e0, "flag_enbl_amp_int_enable" },          \
		{ 0x26f0, "flag_enbl_ref_int_enable" },          \
		{ 0x2700, "flag_voutcomp_int_enable" },          \
		{ 0x2710, "flag_voutcomp93_int_enable" },        \
		{ 0x2720, "flag_voutcomp86_int_enable" },        \
		{ 0x2730, "flag_hiz_int_enable" },               \
		{ 0x2740, "flag_ocpokbst_int_enable" },          \
		{ 0x2750, "flag_peakcur_int_enable" },           \
		{ 0x2760, "flag_ocpokap_int_enable" },           \
		{ 0x2770, "flag_ocpokan_int_enable" },           \
		{ 0x2780, "flag_ocpokbp_int_enable" },           \
		{ 0x2790, "flag_ocpokbn_int_enable" },           \
		{ 0x27a0, "flag_adc10_ready_int_enable" },       \
		{ 0x27b0, "flag_clipa_high_int_enable" },        \
		{ 0x27c0, "flag_clipa_low_int_enable" },         \
		{ 0x27d0, "flag_clipb_high_int_enable" },        \
		{ 0x27e0, "flag_clipb_low_int_enable" },         \
		{ 0x27f0, "flag_tdm_error_int_enable" },         \
		{ 0x2800, "flag_cfma_err_int_enable" },          \
		{ 0x2810, "flag_cfma_ack_int_enable" },          \
		{ 0x2900, "flag_por_int_pol" },                  \
		{ 0x2910, "flag_pll_lock_int_pol" },             \
		{ 0x2920, "flag_otpok_int_pol" },                \
		{ 0x2930, "flag_ovpok_int_pol" },                \
		{ 0x2940, "flag_uvpok_int_pol" },                \
		{ 0x2950, "flag_ocp_alarm_int_pol" },            \
		{ 0x2960, "flag_clocks_stable_int_pol" },        \
		{ 0x2970, "flag_clip_int_pol" },                 \
		{ 0x2980, "mtp_busy_int_pol" },                  \
		{ 0x2990, "flag_lost_clk_int_pol" },             \
		{ 0x29a0, "flag_cf_speakererror_int_pol" },      \
		{ 0x29b0, "flag_cold_started_int_pol" },         \
		{ 0x29c0, "flag_engage_int_pol" },               \
		{ 0x29d0, "flag_watchdog_reset_int_pol" },       \
		{ 0x29e0, "flag_enbl_amp_int_pol" },             \
		{ 0x29f0, "flag_enbl_ref_int_pol" },             \
		{ 0x2a00, "flag_voutcomp_int_pol" },             \
		{ 0x2a10, "flag_voutcomp93_int_pol" },           \
		{ 0x2a20, "flag_voutcomp86_int_pol" },           \
		{ 0x2a30, "flag_hiz_int_pol" },                  \
		{ 0x2a40, "flag_ocpokbst_int_pol" },             \
		{ 0x2a50, "flag_peakcur_int_pol" },              \
		{ 0x2a60, "flag_ocpokap_int_pol" },              \
		{ 0x2a70, "flag_ocpokan_int_pol" },              \
		{ 0x2a80, "flag_ocpokbp_int_pol" },              \
		{ 0x2a90, "flag_ocpokbn_int_pol" },              \
		{ 0x2aa0, "flag_adc10_ready_int_pol" },          \
		{ 0x2ab0, "flag_clipa_high_int_pol" },           \
		{ 0x2ac0, "flag_clipa_low_int_pol" },            \
		{ 0x2ad0, "flag_clipb_high_int_pol" },           \
		{ 0x2ae0, "flag_clipb_low_int_pol" },            \
		{ 0x2af0, "flag_tdm_error_int_pol" },            \
		{ 0x2b00, "flag_cfma_err_int_pol" },             \
		{ 0x2b10, "flag_cfma_ack_int_pol" },             \
		{ 0x3000, "flag_voutcomp" },                     \
		{ 0x3010, "flag_voutcomp93" },                   \
		{ 0x3020, "flag_voutcomp86" },                   \
		{ 0x3030, "flag_hiz" },                          \
		{ 0x3040, "flag_ocpokbst" },                     \
		{ 0x3050, "flag_peakcur" },                      \
		{ 0x3060, "flag_ocpokap" },                      \
		{ 0x3070, "flag_ocpokan" },                      \
		{ 0x3080, "flag_ocpokbp" },                      \
		{ 0x3090, "flag_ocpokbn" },                      \
		{ 0x30a0, "flag_adc10_ready" },                  \
		{ 0x30b0, "flag_clipa_high" },                   \
		{ 0x30c0, "flag_clipa_low" },                    \
		{ 0x30d0, "flag_clipb_high" },                   \
		{ 0x30e0, "flag_clipb_low" },                    \
		{ 0x310f, "mtp_man_data_out" },                  \
		{ 0x3200, "key01_locked" },                      \
		{ 0x3210, "key02_locked" },                      \
		{ 0x3225, "mtp_ecc_tcout" },                     \
		{ 0x3280, "mtpctrl_valid_test_rd" },             \
		{ 0x3290, "mtpctrl_valid_test_wr" },             \
		{ 0x32a0, "flag_in_alarm_state" },               \
		{ 0x32b0, "mtp_ecc_err2" },                      \
		{ 0x32c0, "mtp_ecc_err1" },                      \
		{ 0x32d0, "mtp_mtp_hvf" },                       \
		{ 0x32f0, "mtp_zero_check_fail" },               \
		{ 0x3309, "data_adc10_tempbat" },                \
		{ 0x400f, "hid_code" },                          \
		{ 0x4100, "bypass_hp" },                         \
		{ 0x4110, "hard_mute" },                         \
		{ 0x4120, "soft_mute" },                         \
		{ 0x4134, "pwm_delay" },                         \
		{ 0x4180, "pwm_shape" },                         \
		{ 0x4190, "pwm_bitlength" },                     \
		{ 0x4203, "drive" },                             \
		{ 0x4240, "reclock_pwm" },                       \
		{ 0x4250, "reclock_voltsense" },                 \
		{ 0x4281, "dpsalevel" },                         \
		{ 0x42a1, "dpsa_release" },                      \
		{ 0x42c0, "coincidence" },                       \
		{ 0x42d0, "kickback" },                          \
		{ 0x4306, "drivebst" },                          \
		{ 0x4370, "boost_alg" },                         \
		{ 0x4381, "boost_loopgain" },                    \
		{ 0x43a0, "ocptestbst" },                        \
		{ 0x43d0, "test_abistfft_enbl" },                \
		{ 0x43e0, "bst_dcmbst" },                        \
		{ 0x43f0, "test_bcontrol" },                     \
		{ 0x4400, "reversebst" },                        \
		{ 0x4410, "sensetest" },                         \
		{ 0x4420, "enbl_engagebst" },                    \
		{ 0x4470, "enbl_slopecur" },                     \
		{ 0x4480, "enbl_voutcomp" },                     \
		{ 0x4490, "enbl_voutcomp93" },                   \
		{ 0x44a0, "enbl_voutcomp86" },                   \
		{ 0x44b0, "enbl_hizcom" },                       \
		{ 0x44c0, "enbl_peakcur" },                      \
		{ 0x44d0, "bypass_ovpglitch" },                  \
		{ 0x44e0, "enbl_windac" },                       \
		{ 0x44f0, "enbl_powerbst" },                     \
		{ 0x4507, "ocp_thr" },                           \
		{ 0x4580, "bypass_glitchfilter" },               \
		{ 0x4590, "bypass_ovp" },                        \
		{ 0x45a0, "bypass_uvp" },                        \
		{ 0x45b0, "bypass_otp" },                        \
		{ 0x45d0, "bypass_ocpcounter" },                 \
		{ 0x45e0, "bypass_lost_clk" },                   \
		{ 0x45f0, "vpalarm" },                           \
		{ 0x4600, "bypass_gc" },                         \
		{ 0x4610, "cs_gain_control" },                   \
		{ 0x4627, "cs_gain" },                           \
		{ 0x46a0, "bypass_lp" },                         \
		{ 0x46b0, "bypass_pwmcounter" },                 \
		{ 0x46c0, "cs_negfixed" },                       \
		{ 0x46d2, "cs_neghyst" },                        \
		{ 0x4700, "switch_fb" },                         \
		{ 0x4713, "se_hyst" },                           \
		{ 0x4754, "se_level" },                          \
		{ 0x47a5, "ktemp" },                             \
		{ 0x4800, "cs_negin" },                          \
		{ 0x4810, "cs_sein" },                           \
		{ 0x4820, "cs_coincidence" },                    \
		{ 0x4830, "iddqtestbst" },                       \
		{ 0x4840, "coincidencebst" },                    \
		{ 0x4876, "delay_se_neg" },                      \
		{ 0x48e1, "cs_ttrack" },                         \
		{ 0x4900, "bypass_clip" },                       \
		{ 0x4920, "cf_cgate_off" },                      \
		{ 0x4940, "clipfast" },                          \
		{ 0x4950, "cs_8ohm" },                           \
		{ 0x4974, "delay_clock_sh" },                    \
		{ 0x49c0, "inv_clksh" },                         \
		{ 0x49d0, "inv_neg" },                           \
		{ 0x49e0, "inv_se" },                            \
		{ 0x49f0, "setse" },                             \
		{ 0x4a12, "adc10_sel" },                         \
		{ 0x4a60, "adc10_reset" },                       \
		{ 0x4a81, "adc10_test" },                        \
		{ 0x4aa0, "bypass_lp_vbat" },                    \
		{ 0x4ae0, "dc_offset" },                         \
		{ 0x4af0, "tsense_hibias" },                     \
		{ 0x4b00, "adc13_iset" },                        \
		{ 0x4b14, "adc13_gain" },                        \
		{ 0x4b61, "adc13_slowdel" },                     \
		{ 0x4b83, "adc13_offset" },                      \
		{ 0x4bc0, "adc13_bsoinv" },                      \
		{ 0x4bd0, "adc13_resonator_enable" },            \
		{ 0x4be0, "testmicadc" },                        \
		{ 0x4c0f, "abist_offset" },                      \
		{ 0x4d05, "windac" },                            \
		{ 0x4dc3, "pwm_dcc_cnt" },                       \
		{ 0x4e04, "slopecur" },                          \
		{ 0x4e50, "ctrl_dem" },                          \
		{ 0x4ed0, "enbl_pwm_dcc" },                      \
		{ 0x4f00, "bst_bypass_bstcur" },                 \
		{ 0x4f10, "bst_bypass_bstfoldback" },            \
		{ 0x4f20, "bst_ctrl_azbst" },                    \
		{ 0x5007, "gain" },                              \
		{ 0x5081, "sourceb" },                           \
		{ 0x50a1, "sourcea" },                           \
		{ 0x50c1, "sourcebst" },                         \
		{ 0x50e0, "tdm_enable_loopback" },               \
		{ 0x5104, "pulselengthbst" },                    \
		{ 0x5150, "bypasslatchbst" },                    \
		{ 0x5160, "invertbst" },                         \
		{ 0x5174, "pulselength" },                       \
		{ 0x51c0, "bypasslatch" },                       \
		{ 0x51d0, "invertb" },                           \
		{ 0x51e0, "inverta" },                           \
		{ 0x51f0, "bypass_ctrlloop" },                   \
		{ 0x5210, "test_rdsona" },                       \
		{ 0x5220, "test_rdsonb" },                       \
		{ 0x5230, "test_rdsonbst" },                     \
		{ 0x5240, "test_cvia" },                         \
		{ 0x5250, "test_cvib" },                         \
		{ 0x5260, "test_cvibst" },                       \
		{ 0x5306, "digimuxa_sel" },                      \
		{ 0x5376, "digimuxb_sel" },                      \
		{ 0x5400, "hs_mode" },                           \
		{ 0x5412, "test_parametric_io" },                \
		{ 0x5440, "enbl_ringo" },                        \
		{ 0x5456, "digimuxc_sel" },                      \
		{ 0x54c0, "dio_ehs" },                           \
		{ 0x54d0, "gainio_ehs" },                        \
		{ 0x550d, "enbl_amp" },                          \
		{ 0x5600, "use_direct_ctrls" },                  \
		{ 0x5610, "rst_datapath" },                      \
		{ 0x5620, "rst_cgu" },                           \
		{ 0x5637, "enbl_ref" },                          \
		{ 0x56b0, "enbl_engage" },                       \
		{ 0x56c0, "use_direct_clk_ctrl" },               \
		{ 0x56d0, "use_direct_pll_ctrl" },               \
		{ 0x5707, "anamux" },                            \
		{ 0x57e0, "otptest" },                           \
		{ 0x57f0, "reverse" },                           \
		{ 0x5813, "pll_selr" },                          \
		{ 0x5854, "pll_selp" },                          \
		{ 0x58a5, "pll_seli" },                          \
		{ 0x5950, "pll_mdec_msb" },                      \
		{ 0x5960, "pll_ndec_msb" },                      \
		{ 0x5970, "pll_frm" },                           \
		{ 0x5980, "pll_directi" },                       \
		{ 0x5990, "pll_directo" },                       \
		{ 0x59a0, "enbl_pll" },                          \
		{ 0x59f0, "pll_bypass" },                        \
		{ 0x5a0f, "tsig_freq" },                         \
		{ 0x5b02, "tsig_freq_msb" },                     \
		{ 0x5b30, "inject_tsig" },                       \
		{ 0x5b44, "adc10_prog_sample" },                 \
		{ 0x5c0f, "pll_mdec" },                          \
		{ 0x5d06, "pll_pdec" },                          \
		{ 0x5d78, "pll_ndec" },                          \
		{ 0x6007, "mtpkey1" },                           \
		{ 0x6185, "mtp_ecc_tcin" },                      \
		{ 0x6203, "mtp_man_address_in" },                \
		{ 0x6260, "mtp_ecc_eeb" },                       \
		{ 0x6270, "mtp_ecc_ecb" },                       \
		{ 0x6280, "man_copy_mtp_to_iic" },               \
		{ 0x6290, "man_copy_iic_to_mtp" },               \
		{ 0x62a0, "auto_copy_mtp_to_iic" },              \
		{ 0x62b0, "auto_copy_iic_to_mtp" },              \
		{ 0x62d2, "mtp_speed_mode" },                    \
		{ 0x6340, "mtp_direct_enable" },                 \
		{ 0x6350, "mtp_direct_wr" },                     \
		{ 0x6360, "mtp_direct_rd" },                     \
		{ 0x6370, "mtp_direct_rst" },                    \
		{ 0x6380, "mtp_direct_ers" },                    \
		{ 0x6390, "mtp_direct_prg" },                    \
		{ 0x63a0, "mtp_direct_epp" },                    \
		{ 0x63b4, "mtp_direct_test" },                   \
		{ 0x640f, "mtp_man_data_in" },                   \
		{ 0x7000, "cf_rst_dsp" },                        \
		{ 0x7011, "cf_dmem" },                           \
		{ 0x7030, "cf_aif" },                            \
		{ 0x7040, "cf_int" },                            \
		{ 0x7087, "cf_req" },                            \
		{ 0x710f, "cf_madd" },                           \
		{ 0x720f, "cf_mema" },                           \
		{ 0x7307, "cf_err" },                            \
		{ 0x7387, "cf_ack" },                            \
		{ 0x8000, "calibration_onetime" },               \
		{ 0x8010, "calibr_ron_done" },                   \
		{ 0x8105, "calibr_vout_offset" },                \
		{ 0x8163, "calibr_delta_gain" },                 \
		{ 0x81a5, "calibr_offs_amp" },                   \
		{ 0x8207, "calibr_gain_cs" },                    \
		{ 0x8284, "calibr_temp_offset" },                \
		{ 0x82d2, "calibr_temp_gain" },                  \
		{ 0x830f, "calibr_ron" },                        \
		{ 0x8505, "type_bits_hw" },                      \
		{ 0x8601, "type_bits_1_0_sw" },                  \
		{ 0x8681, "type_bits_9_8_sw" },                  \
		{ 0x870f, "type_bits2_sw" },                     \
		{ 0x8806, "htol_iic_addr" },                     \
		{ 0x8870, "htol_iic_addr_en" },                  \
		{ 0x8881, "ctrl_ovp_response" },                 \
		{ 0x88a0, "disable_ovp_alarm_state" },           \
		{ 0x88b0, "enbl_stretch_ovp" },                  \
		{ 0x88c0, "cf_debug_mode" },                     \
		{ 0x8a0f, "production_data1" },                  \
		{ 0x8b0f, "production_data2" },                  \
		{ 0x8c0f, "production_data3" },                  \
		{ 0x8d0f, "production_data4" },                  \
		{ 0x8e0f, "production_data5" },                  \
		{ 0x8f0f, "production_data6" },                  \
		{ 0xffff, "Unknown bitfield enum" }              \
	}
enum TFA9896_irq {
	TFA9896_irq_vdds = 0,
	TFA9896_irq_plls = 1,
	TFA9896_irq_ds = 2,
	TFA9896_irq_vds = 3,
	TFA9896_irq_uvds = 4,
	TFA9896_irq_cds = 5,
	TFA9896_irq_clks = 6,
	TFA9896_irq_clips = 7,
	TFA9896_irq_mtpb = 8,
	TFA9896_irq_clk = 9,
	TFA9896_irq_spks = 10,
	TFA9896_irq_acs = 11,
	TFA9896_irq_sws = 12,
	TFA9896_irq_wds = 13,
	TFA9896_irq_amps = 14,
	TFA9896_irq_arefs = 15,
	TFA9896_irq_err = 32,
	TFA9896_irq_ack = 33,
	TFA9896_irq_max = 34,
	TFA9896_irq_all = -1
};
#define TFA9896_IRQ_NAMETABLE                                                  \
	static struct TfaIrqName TFA9896IrqNames[] = {                         \
		{ 0, "VDDS" }, { 1, "PLLS" }, { 2, "DS" },    { 3, "VDS" },    \
		{ 4, "UVDS" }, { 5, "CDS" },  { 6, "CLKS" },  { 7, "CLIPS" },  \
		{ 8, "MTPB" }, { 9, "CLK" },  { 10, "SPKS" }, { 11, "ACS" },   \
		{ 12, "SWS" }, { 13, "WDS" }, { 14, "AMPS" }, { 15, "AREFS" }, \
		{ 16, "16" },  { 17, "17" },  { 18, "18" },   { 19, "19" },    \
		{ 20, "20" },  { 21, "21" },  { 22, "22" },   { 23, "23" },    \
		{ 24, "24" },  { 25, "25" },  { 26, "26" },   { 27, "27" },    \
		{ 28, "28" },  { 29, "29" },  { 30, "30" },   { 31, "31" },    \
		{ 32, "ERR" }, { 33, "ACK" }, { 34, "34" },                    \
	}
#endif
