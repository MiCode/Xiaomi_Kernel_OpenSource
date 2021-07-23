/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _TFA9874_TFAFIELDNAMES_H
#define _TFA9874_TFAFIELDNAMES_H
#define TFA9874_I2CVERSION 1.16
enum nxpTfa9874BfEnumList {
	TFA9874_BF_PWDN = 0x0000,
	TFA9874_BF_I2CR = 0x0010,
	TFA9874_BF_AMPE = 0x0030,
	TFA9874_BF_DCA = 0x0040,
	TFA9874_BF_INTP = 0x0071,
	TFA9874_BF_BYPOCP = 0x00b0,
	TFA9874_BF_TSTOCP = 0x00c0,
	TFA9874_BF_MANSCONF = 0x0120,
	TFA9874_BF_MANAOOSC = 0x0140,
	TFA9874_BF_MUTETO = 0x01d0,
	TFA9874_BF_OPENMTP = 0x01e0,
	TFA9874_BF_AUDFS = 0x0203,
	TFA9874_BF_INPLEV = 0x0240,
	TFA9874_BF_FRACTDEL = 0x0255,
	TFA9874_BF_REV = 0x030f,
	TFA9874_BF_REFCKEXT = 0x0401,
	TFA9874_BF_REFCKSEL = 0x0420,
	TFA9874_BF_SSFAIME = 0x05c0,
	TFA9874_BF_AMPOCRT = 0x0802,
	TFA9874_BF_VDDS = 0x1000,
	TFA9874_BF_DCOCPOK = 0x1010,
	TFA9874_BF_OTDS = 0x1020,
	TFA9874_BF_OCDS = 0x1030,
	TFA9874_BF_UVDS = 0x1040,
	TFA9874_BF_MANALARM = 0x1050,
	TFA9874_BF_TDMERR = 0x1060,
	TFA9874_BF_NOCLK = 0x1070,
	TFA9874_BF_DCIL = 0x1100,
	TFA9874_BF_DCDCA = 0x1110,
	TFA9874_BF_DCHVBAT = 0x1130,
	TFA9874_BF_DCH114 = 0x1140,
	TFA9874_BF_DCH107 = 0x1150,
	TFA9874_BF_PLLS = 0x1160,
	TFA9874_BF_CLKS = 0x1170,
	TFA9874_BF_TDMLUTER = 0x1180,
	TFA9874_BF_TDMSTAT = 0x1192,
	TFA9874_BF_MTPB = 0x11c0,
	TFA9874_BF_SWS = 0x11d0,
	TFA9874_BF_AMPS = 0x11e0,
	TFA9874_BF_AREFS = 0x11f0,
	TFA9874_BF_OCPOAP = 0x1300,
	TFA9874_BF_OCPOAN = 0x1310,
	TFA9874_BF_OCPOBP = 0x1320,
	TFA9874_BF_OCPOBN = 0x1330,
	TFA9874_BF_OVDS = 0x1380,
	TFA9874_BF_CLIPS = 0x1390,
	TFA9874_BF_ADCCR = 0x13a0,
	TFA9874_BF_MANWAIT1 = 0x13c0,
	TFA9874_BF_MANMUTE = 0x13e0,
	TFA9874_BF_MANOPER = 0x13f0,
	TFA9874_BF_CLKOOR = 0x1420,
	TFA9874_BF_MANSTATE = 0x1433,
	TFA9874_BF_DCMODE = 0x1471,
	TFA9874_BF_BATS = 0x1509,
	TFA9874_BF_TEMPS = 0x1608,
	TFA9874_BF_VDDPS = 0x1709,
	TFA9874_BF_TDME = 0x2040,
	TFA9874_BF_TDMMODE = 0x2050,
	TFA9874_BF_TDMCLINV = 0x2060,
	TFA9874_BF_TDMFSLN = 0x2073,
	TFA9874_BF_TDMFSPOL = 0x20b0,
	TFA9874_BF_TDMNBCK = 0x20c3,
	TFA9874_BF_TDMSLOTS = 0x2103,
	TFA9874_BF_TDMSLLN = 0x2144,
	TFA9874_BF_TDMBRMG = 0x2194,
	TFA9874_BF_TDMDEL = 0x21e0,
	TFA9874_BF_TDMADJ = 0x21f0,
	TFA9874_BF_TDMOOMP = 0x2201,
	TFA9874_BF_TDMSSIZE = 0x2224,
	TFA9874_BF_TDMTXDFO = 0x2271,
	TFA9874_BF_TDMTXUS0 = 0x2291,
	TFA9874_BF_TDMSPKE = 0x2300,
	TFA9874_BF_TDMDCE = 0x2310,
	TFA9874_BF_TDMCSE = 0x2330,
	TFA9874_BF_TDMVSE = 0x2340,
	TFA9874_BF_TDMSPKS = 0x2603,
	TFA9874_BF_TDMDCS = 0x2643,
	TFA9874_BF_TDMCSS = 0x26c3,
	TFA9874_BF_TDMVSS = 0x2703,
	TFA9874_BF_ISTVDDS = 0x4000,
	TFA9874_BF_ISTBSTOC = 0x4010,
	TFA9874_BF_ISTOTDS = 0x4020,
	TFA9874_BF_ISTOCPR = 0x4030,
	TFA9874_BF_ISTUVDS = 0x4040,
	TFA9874_BF_ISTMANALARM = 0x4050,
	TFA9874_BF_ISTTDMER = 0x4060,
	TFA9874_BF_ISTNOCLK = 0x4070,
	TFA9874_BF_ICLVDDS = 0x4400,
	TFA9874_BF_ICLBSTOC = 0x4410,
	TFA9874_BF_ICLOTDS = 0x4420,
	TFA9874_BF_ICLOCPR = 0x4430,
	TFA9874_BF_ICLUVDS = 0x4440,
	TFA9874_BF_ICLMANALARM = 0x4450,
	TFA9874_BF_ICLTDMER = 0x4460,
	TFA9874_BF_ICLNOCLK = 0x4470,
	TFA9874_BF_IEVDDS = 0x4800,
	TFA9874_BF_IEBSTOC = 0x4810,
	TFA9874_BF_IEOTDS = 0x4820,
	TFA9874_BF_IEOCPR = 0x4830,
	TFA9874_BF_IEUVDS = 0x4840,
	TFA9874_BF_IEMANALARM = 0x4850,
	TFA9874_BF_IETDMER = 0x4860,
	TFA9874_BF_IENOCLK = 0x4870,
	TFA9874_BF_IPOVDDS = 0x4c00,
	TFA9874_BF_IPOBSTOC = 0x4c10,
	TFA9874_BF_IPOOTDS = 0x4c20,
	TFA9874_BF_IPOOCPR = 0x4c30,
	TFA9874_BF_IPOUVDS = 0x4c40,
	TFA9874_BF_IPOMANALARM = 0x4c50,
	TFA9874_BF_IPOTDMER = 0x4c60,
	TFA9874_BF_IPONOCLK = 0x4c70,
	TFA9874_BF_BSSCR = 0x5001,
	TFA9874_BF_BSST = 0x5023,
	TFA9874_BF_BSSRL = 0x5061,
	TFA9874_BF_VBATFLTL = 0x5080,
	TFA9874_BF_BSSR = 0x50e0,
	TFA9874_BF_BSSBY = 0x50f0,
	TFA9874_BF_BSSS = 0x5100,
	TFA9874_BF_HPFBYP = 0x5150,
	TFA9874_BF_DPSA = 0x5170,
	TFA9874_BF_CLIPCTRL = 0x5222,
	TFA9874_BF_AMPGAIN = 0x5257,
	TFA9874_BF_SLOPEE = 0x52d0,
	TFA9874_BF_SLOPESET = 0x52e0,
	TFA9874_BF_TDMDCG = 0x6123,
	TFA9874_BF_TDMSPKG = 0x6163,
	TFA9874_BF_LNMODE = 0x62e1,
	TFA9874_BF_LPM1MODE = 0x64e1,
	TFA9874_BF_TDMSRCMAP = 0x6802,
	TFA9874_BF_TDMSRCAS = 0x6831,
	TFA9874_BF_TDMSRCBS = 0x6851,
	TFA9874_BF_TDMSRCACLIP = 0x6871,
	TFA9874_BF_TDMSRCBCLIP = 0x6891,
	TFA9874_BF_LP1 = 0x6e10,
	TFA9874_BF_LA = 0x6e20,
	TFA9874_BF_VDDPH = 0x6e30,
	TFA9874_BF_DELCURCOMP = 0x6f02,
	TFA9874_BF_SIGCURCOMP = 0x6f40,
	TFA9874_BF_ENCURCOMP = 0x6f50,
	TFA9874_BF_LVLCLPPWM = 0x6f72,
	TFA9874_BF_DCMCC = 0x7033,
	TFA9874_BF_DCCV = 0x7071,
	TFA9874_BF_DCIE = 0x7090,
	TFA9874_BF_DCSR = 0x70a0,
	TFA9874_BF_DCDIS = 0x70e0,
	TFA9874_BF_DCPWM = 0x70f0,
	TFA9874_BF_DCTRACK = 0x7430,
	TFA9874_BF_DCTRIP = 0x7444,
	TFA9874_BF_DCHOLD = 0x7494,
	TFA9874_BF_DCINT = 0x74e0,
	TFA9874_BF_DCTRIP2 = 0x7534,
	TFA9874_BF_DCTRIPT = 0x7584,
	TFA9874_BF_DCTRIPHYSTE = 0x75f0,
	TFA9874_BF_DCVOF = 0x7635,
	TFA9874_BF_DCVOS = 0x7695,
	TFA9874_BF_MTPK = 0xa107,
	TFA9874_BF_KEY1LOCKED = 0xa200,
	TFA9874_BF_KEY2LOCKED = 0xa210,
	TFA9874_BF_CIMTP = 0xa360,
	TFA9874_BF_MTPRDMSB = 0xa50f,
	TFA9874_BF_MTPRDLSB = 0xa60f,
	TFA9874_BF_EXTTS = 0xb108,
	TFA9874_BF_TROS = 0xb190,
	TFA9874_BF_SWPROFIL = 0xee0f,
	TFA9874_BF_SWVSTEP = 0xef0f,
	TFA9874_BF_MTPOTC = 0xf000,
	TFA9874_BF_MTPEX = 0xf010,
	TFA9874_BF_DCMCCAPI = 0xf020,
	TFA9874_BF_DCMCCSB = 0xf030,
	TFA9874_BF_USERDEF = 0xf042,
	TFA9874_BF_CUSTINFO = 0xf078,
	TFA9874_BF_R25C = 0xf50f,
};
#define TFA9874_NAMETABLE                              \
	static struct TfaBfName Tfa9874DatasheetNames[] = { \
		{ 0x0, "PWDN" },                       \
		{ 0x10, "I2CR" },                      \
		{ 0x30, "AMPE" },                      \
		{ 0x40, "DCA" },                       \
		{ 0x71, "INTP" },                      \
		{ 0xb0, "BYPOCP" },                    \
		{ 0xc0, "TSTOCP" },                    \
		{ 0x120, "MANSCONF" },                 \
		{ 0x140, "MANAOOSC" },                 \
		{ 0x1d0, "MUTETO" },                   \
		{ 0x1e0, "OPENMTP" },                  \
		{ 0x203, "AUDFS" },                    \
		{ 0x240, "INPLEV" },                   \
		{ 0x255, "FRACTDEL" },                 \
		{ 0x30f, "REV" },                      \
		{ 0x401, "REFCKEXT" },                 \
		{ 0x420, "REFCKSEL" },                 \
		{ 0x5c0, "SSFAIME" },                  \
		{ 0x802, "AMPOCRT" },                  \
		{ 0x1000, "VDDS" },                    \
		{ 0x1010, "DCOCPOK" },                 \
		{ 0x1020, "OTDS" },                    \
		{ 0x1030, "OCDS" },                    \
		{ 0x1040, "UVDS" },                    \
		{ 0x1050, "MANALARM" },                \
		{ 0x1060, "TDMERR" },                  \
		{ 0x1070, "NOCLK" },                   \
		{ 0x1100, "DCIL" },                    \
		{ 0x1110, "DCDCA" },                   \
		{ 0x1130, "DCHVBAT" },                 \
		{ 0x1140, "DCH114" },                  \
		{ 0x1150, "DCH107" },                  \
		{ 0x1160, "PLLS" },                    \
		{ 0x1170, "CLKS" },                    \
		{ 0x1180, "TDMLUTER" },                \
		{ 0x1192, "TDMSTAT" },                 \
		{ 0x11c0, "MTPB" },                    \
		{ 0x11d0, "SWS" },                     \
		{ 0x11e0, "AMPS" },                    \
		{ 0x11f0, "AREFS" },                   \
		{ 0x1300, "OCPOAP" },                  \
		{ 0x1310, "OCPOAN" },                  \
		{ 0x1320, "OCPOBP" },                  \
		{ 0x1330, "OCPOBN" },                  \
		{ 0x1380, "OVDS" },                    \
		{ 0x1390, "CLIPS" },                   \
		{ 0x13a0, "ADCCR" },                   \
		{ 0x13c0, "MANWAIT1" },                \
		{ 0x13e0, "MANMUTE" },                 \
		{ 0x13f0, "MANOPER" },                 \
		{ 0x1420, "CLKOOR" },                  \
		{ 0x1433, "MANSTATE" },                \
		{ 0x1471, "DCMODE" },                  \
		{ 0x1509, "BATS" },                    \
		{ 0x1608, "TEMPS" },                   \
		{ 0x1709, "VDDPS" },                   \
		{ 0x2040, "TDME" },                    \
		{ 0x2050, "TDMMODE" },                 \
		{ 0x2060, "TDMCLINV" },                \
		{ 0x2073, "TDMFSLN" },                 \
		{ 0x20b0, "TDMFSPOL" },                \
		{ 0x20c3, "TDMNBCK" },                 \
		{ 0x2103, "TDMSLOTS" },                \
		{ 0x2144, "TDMSLLN" },                 \
		{ 0x2194, "TDMBRMG" },                 \
		{ 0x21e0, "TDMDEL" },                  \
		{ 0x21f0, "TDMADJ" },                  \
		{ 0x2201, "TDMOOMP" },                 \
		{ 0x2224, "TDMSSIZE" },                \
		{ 0x2271, "TDMTXDFO" },                \
		{ 0x2291, "TDMTXUS0" },                \
		{ 0x2300, "TDMSPKE" },                 \
		{ 0x2310, "TDMDCE" },                  \
		{ 0x2330, "TDMCSE" },                  \
		{ 0x2340, "TDMVSE" },                  \
		{ 0x2603, "TDMSPKS" },                 \
		{ 0x2643, "TDMDCS" },                  \
		{ 0x26c3, "TDMCSS" },                  \
		{ 0x2703, "TDMVSS" },                  \
		{ 0x4000, "ISTVDDS" },                 \
		{ 0x4010, "ISTBSTOC" },                \
		{ 0x4020, "ISTOTDS" },                 \
		{ 0x4030, "ISTOCPR" },                 \
		{ 0x4040, "ISTUVDS" },                 \
		{ 0x4050, "ISTMANALARM" },             \
		{ 0x4060, "ISTTDMER" },                \
		{ 0x4070, "ISTNOCLK" },                \
		{ 0x4400, "ICLVDDS" },                 \
		{ 0x4410, "ICLBSTOC" },                \
		{ 0x4420, "ICLOTDS" },                 \
		{ 0x4430, "ICLOCPR" },                 \
		{ 0x4440, "ICLUVDS" },                 \
		{ 0x4450, "ICLMANALARM" },             \
		{ 0x4460, "ICLTDMER" },                \
		{ 0x4470, "ICLNOCLK" },                \
		{ 0x4800, "IEVDDS" },                  \
		{ 0x4810, "IEBSTOC" },                 \
		{ 0x4820, "IEOTDS" },                  \
		{ 0x4830, "IEOCPR" },                  \
		{ 0x4840, "IEUVDS" },                  \
		{ 0x4850, "IEMANALARM" },              \
		{ 0x4860, "IETDMER" },                 \
		{ 0x4870, "IENOCLK" },                 \
		{ 0x4c00, "IPOVDDS" },                 \
		{ 0x4c10, "IPOBSTOC" },                \
		{ 0x4c20, "IPOOTDS" },                 \
		{ 0x4c30, "IPOOCPR" },                 \
		{ 0x4c40, "IPOUVDS" },                 \
		{ 0x4c50, "IPOMANALARM" },             \
		{ 0x4c60, "IPOTDMER" },                \
		{ 0x4c70, "IPONOCLK" },                \
		{ 0x5001, "BSSCR" },                   \
		{ 0x5023, "BSST" },                    \
		{ 0x5061, "BSSRL" },                   \
		{ 0x5080, "VBATFLTL" },                \
		{ 0x50e0, "BSSR" },                    \
		{ 0x50f0, "BSSBY" },                   \
		{ 0x5100, "BSSS" },                    \
		{ 0x5150, "HPFBYP" },                  \
		{ 0x5170, "DPSA" },                    \
		{ 0x5222, "CLIPCTRL" },                \
		{ 0x5257, "AMPGAIN" },                 \
		{ 0x52d0, "SLOPEE" },                  \
		{ 0x52e0, "SLOPESET" },                \
		{ 0x6123, "TDMDCG" },                  \
		{ 0x6163, "TDMSPKG" },                 \
		{ 0x62e1, "LNMODE" },                  \
		{ 0x64e1, "LPM1MODE" },                \
		{ 0x6802, "TDMSRCMAP" },               \
		{ 0x6831, "TDMSRCAS" },                \
		{ 0x6851, "TDMSRCBS" },                \
		{ 0x6871, "TDMSRCACLIP" },             \
		{ 0x6891, "TDMSRCBCLIP" },             \
		{ 0x6e10, "LP1" },                     \
		{ 0x6e20, "LA" },                      \
		{ 0x6e30, "VDDPH" },                   \
		{ 0x6f02, "DELCURCOMP" },              \
		{ 0x6f40, "SIGCURCOMP" },              \
		{ 0x6f50, "ENCURCOMP" },               \
		{ 0x6f72, "LVLCLPPWM" },               \
		{ 0x7033, "DCMCC" },                   \
		{ 0x7071, "DCCV" },                    \
		{ 0x7090, "DCIE" },                    \
		{ 0x70a0, "DCSR" },                    \
		{ 0x70e0, "DCDIS" },                   \
		{ 0x70f0, "DCPWM" },                   \
		{ 0x7430, "DCTRACK" },                 \
		{ 0x7444, "DCTRIP" },                  \
		{ 0x7494, "DCHOLD" },                  \
		{ 0x74e0, "DCINT" },                   \
		{ 0x7534, "DCTRIP2" },                 \
		{ 0x7584, "DCTRIPT" },                 \
		{ 0x75f0, "DCTRIPHYSTE" },             \
		{ 0x7635, "DCVOF" },                   \
		{ 0x7695, "DCVOS" },                   \
		{ 0xa107, "MTPK" },                    \
		{ 0xa200, "KEY1LOCKED" },              \
		{ 0xa210, "KEY2LOCKED" },              \
		{ 0xa360, "CIMTP" },                   \
		{ 0xa50f, "MTPRDMSB" },                \
		{ 0xa60f, "MTPRDLSB" },                \
		{ 0xb108, "EXTTS" },                   \
		{ 0xb190, "TROS" },                    \
		{ 0xee0f, "SWPROFIL" },                \
		{ 0xef0f, "SWVSTEP" },                 \
		{ 0xf000, "MTPOTC" },                  \
		{ 0xf010, "MTPEX" },                   \
		{ 0xf020, "DCMCCAPI" },                \
		{ 0xf030, "DCMCCSB" },                 \
		{ 0xf042, "USERDEF" },                 \
		{ 0xf078, "CUSTINFO" },                \
		{ 0xf50f, "R25C" },                    \
		{ 0xffff, "Unknown bitfield enum" }    \
	}
#define TFA9874_BITNAMETABLE                                     \
	static struct TfaBfName Tfa9874BitNames[] = {                 \
		{ 0x0, "powerdown" },                            \
		{ 0x10, "reset" },                               \
		{ 0x30, "enbl_amplifier" },                      \
		{ 0x40, "enbl_boost" },                          \
		{ 0x71, "int_pad_io" },                          \
		{ 0xb0, "bypass_ocp" },                          \
		{ 0xc0, "test_ocp" },                            \
		{ 0x120, "src_set_configured" },                 \
		{ 0x140, "enbl_osc1m_auto_off" },                \
		{ 0x1d0, "disable_mute_time_out" },              \
		{ 0x1e0, "unprotect_faim" },                     \
		{ 0x203, "audio_fs" },                           \
		{ 0x240, "input_level" },                        \
		{ 0x255, "cs_frac_delay" },                      \
		{ 0x2d0, "sel_hysteresis" },                     \
		{ 0x30f, "device_rev" },                         \
		{ 0x401, "pll_clkin_sel" },                      \
		{ 0x420, "pll_clkin_sel_osc" },                  \
		{ 0x5c0, "enbl_faim_ss" },                       \
		{ 0x802, "ctrl_on2off_criterion" },              \
		{ 0xe07, "ctrl_digtoana" },                      \
		{ 0xf0f, "hidden_code" },                        \
		{ 0x1000, "flag_por" },                          \
		{ 0x1010, "flag_bst_ocpok" },                    \
		{ 0x1020, "flag_otpok" },                        \
		{ 0x1030, "flag_ocp_alarm" },                    \
		{ 0x1040, "flag_uvpok" },                        \
		{ 0x1050, "flag_man_alarm_state" },              \
		{ 0x1060, "flag_tdm_error" },                    \
		{ 0x1070, "flag_lost_clk" },                     \
		{ 0x1100, "flag_bst_bstcur" },                   \
		{ 0x1110, "flag_bst_hiz" },                      \
		{ 0x1120, "flag_bst_peakcur" },                  \
		{ 0x1130, "flag_bst_voutcomp" },                 \
		{ 0x1140, "flag_bst_voutcomp86" },               \
		{ 0x1150, "flag_bst_voutcomp93" },               \
		{ 0x1160, "flag_pll_lock" },                     \
		{ 0x1170, "flag_clocks_stable" },                \
		{ 0x1180, "flag_tdm_lut_error" },                \
		{ 0x1192, "flag_tdm_status" },                   \
		{ 0x11c0, "flag_mtp_busy" },                     \
		{ 0x11d0, "flag_engage" },                       \
		{ 0x11e0, "flag_enbl_amp" },                     \
		{ 0x11f0, "flag_enbl_ref" },                     \
		{ 0x1300, "flag_ocpokap" },                      \
		{ 0x1310, "flag_ocpokan" },                      \
		{ 0x1320, "flag_ocpokbp" },                      \
		{ 0x1330, "flag_ocpokbn" },                      \
		{ 0x1380, "flag_ovpok" },                        \
		{ 0x1390, "flag_clip" },                         \
		{ 0x13a0, "flag_adc10_ready" },                  \
		{ 0x13c0, "flag_man_wait_src_settings" },        \
		{ 0x13e0, "flag_man_start_mute_audio" },         \
		{ 0x13f0, "flag_man_operating_state" },          \
		{ 0x1420, "flag_clk_out_of_range" },             \
		{ 0x1433, "man_state" },                         \
		{ 0x1471, "status_bst_mode" },                   \
		{ 0x1509, "bat_adc" },                           \
		{ 0x1608, "temp_adc" },                          \
		{ 0x1709, "vddp_adc" },                          \
		{ 0x2040, "tdm_enable" },                        \
		{ 0x2050, "tdm_mode" },                          \
		{ 0x2060, "tdm_clk_inversion" },                 \
		{ 0x2073, "tdm_fs_ws_length" },                  \
		{ 0x20b0, "tdm_fs_ws_polarity" },                \
		{ 0x20c3, "tdm_nbck" },                          \
		{ 0x2103, "tdm_nb_of_slots" },                   \
		{ 0x2144, "tdm_slot_length" },                   \
		{ 0x2194, "tdm_bits_remaining" },                \
		{ 0x21e0, "tdm_data_delay" },                    \
		{ 0x21f0, "tdm_data_adjustment" },               \
		{ 0x2201, "tdm_audio_sample_compression" },      \
		{ 0x2224, "tdm_sample_size" },                   \
		{ 0x2271, "tdm_txdata_format" },                 \
		{ 0x2291, "tdm_txdata_format_unused_slot_sd0" }, \
		{ 0x2300, "tdm_sink0_enable" },                  \
		{ 0x2310, "tdm_sink1_enable" },                  \
		{ 0x2330, "tdm_source0_enable" },                \
		{ 0x2340, "tdm_source1_enable" },                \
		{ 0x2603, "tdm_sink0_slot" },                    \
		{ 0x2643, "tdm_sink1_slot" },                    \
		{ 0x26c3, "tdm_source0_slot" },                  \
		{ 0x2703, "tdm_source1_slot" },                  \
		{ 0x4000, "int_out_flag_por" },                  \
		{ 0x4010, "int_out_flag_bst_ocpok" },            \
		{ 0x4020, "int_out_flag_otpok" },                \
		{ 0x4030, "int_out_flag_ocp_alarm" },            \
		{ 0x4040, "int_out_flag_uvpok" },                \
		{ 0x4050, "int_out_flag_man_alarm_state" },      \
		{ 0x4060, "int_out_flag_tdm_error" },            \
		{ 0x4070, "int_out_flag_lost_clk" },             \
		{ 0x4400, "int_in_flag_por" },                   \
		{ 0x4410, "int_in_flag_bst_ocpok" },             \
		{ 0x4420, "int_in_flag_otpok" },                 \
		{ 0x4430, "int_in_flag_ocp_alarm" },             \
		{ 0x4440, "int_in_flag_uvpok" },                 \
		{ 0x4450, "int_in_flag_man_alarm_state" },       \
		{ 0x4460, "int_in_flag_tdm_error" },             \
		{ 0x4470, "int_in_flag_lost_clk" },              \
		{ 0x4800, "int_enable_flag_por" },               \
		{ 0x4810, "int_enable_flag_bst_ocpok" },         \
		{ 0x4820, "int_enable_flag_otpok" },             \
		{ 0x4830, "int_enable_flag_ocp_alarm" },         \
		{ 0x4840, "int_enable_flag_uvpok" },             \
		{ 0x4850, "int_enable_flag_man_alarm_state" },   \
		{ 0x4860, "int_enable_flag_tdm_error" },         \
		{ 0x4870, "int_enable_flag_lost_clk" },          \
		{ 0x4c00, "int_polarity_flag_por" },             \
		{ 0x4c10, "int_polarity_flag_bst_ocpok" },       \
		{ 0x4c20, "int_polarity_flag_otpok" },           \
		{ 0x4c30, "int_polarity_flag_ocp_alarm" },       \
		{ 0x4c40, "int_polarity_flag_uvpok" },           \
		{ 0x4c50, "int_polarity_flag_man_alarm_state" }, \
		{ 0x4c60, "int_polarity_flag_tdm_error" },       \
		{ 0x4c70, "int_polarity_flag_lost_clk" },        \
		{ 0x5001, "vbat_prot_attack_time" },             \
		{ 0x5023, "vbat_prot_thlevel" },                 \
		{ 0x5061, "vbat_prot_max_reduct" },              \
		{ 0x5080, "vbat_flt_limit" },                    \
		{ 0x50d0, "rst_min_vbat" },                      \
		{ 0x50e0, "sel_vbat" },                          \
		{ 0x50f0, "bypass_clipper" },                    \
		{ 0x5100, "batsense_steepness" },                \
		{ 0x5150, "bypass_hp" },                         \
		{ 0x5170, "enbl_dpsa" },                         \
		{ 0x5222, "ctrl_cc" },                           \
		{ 0x5257, "gain" },                              \
		{ 0x52d0, "ctrl_slopectrl" },                    \
		{ 0x52e0, "ctrl_slope" },                        \
		{ 0x5301, "dpsa_level" },                        \
		{ 0x5321, "dpsa_release" },                      \
		{ 0x5340, "clipfast" },                          \
		{ 0x5350, "bypass_lp" },                         \
		{ 0x5400, "first_order_mode" },                  \
		{ 0x5410, "bypass_ctrlloop" },                   \
		{ 0x5430, "icomp_engage" },                      \
		{ 0x5440, "ctrl_kickback" },                     \
		{ 0x5450, "icomp_engage_overrule" },             \
		{ 0x5503, "ctrl_dem" },                          \
		{ 0x5543, "ctrl_dem_mismatch" },                 \
		{ 0x5582, "dpsa_drive" },                        \
		{ 0x5690, "sel_pwm_delay_src" },                 \
		{ 0x56a1, "enbl_odd_up_even_down" },             \
		{ 0x570a, "enbl_amp" },                          \
		{ 0x57b0, "enbl_engage" },                       \
		{ 0x57c0, "enbl_engage_pst" },                   \
		{ 0x5810, "hard_mute" },                         \
		{ 0x5820, "pwm_shape" },                         \
		{ 0x5844, "pwm_delay" },                         \
		{ 0x5890, "reclock_pwm" },                       \
		{ 0x58a0, "reclock_voltsense" },                 \
		{ 0x58c0, "enbl_pwm_phase_shift" },              \
		{ 0x6123, "ctrl_attl" },                         \
		{ 0x6163, "ctrl_attr" },                         \
		{ 0x6265, "zero_lvl" },                          \
		{ 0x62c1, "ctrl_fb_resistor" },                  \
		{ 0x62e1, "lownoisegain_mode" },                 \
		{ 0x6305, "threshold_lvl" },                     \
		{ 0x6365, "hold_time" },                         \
		{ 0x6405, "lpm1_cal_offset" },                   \
		{ 0x6465, "lpm1_zero_lvl" },                     \
		{ 0x64e1, "lpm1_mode" },                         \
		{ 0x6505, "lpm1_threshold_lvl" },                \
		{ 0x6565, "lpm1_hold_time" },                    \
		{ 0x65c0, "disable_low_power_mode" },            \
		{ 0x6600, "dcdc_pfm20khz_limit" },               \
		{ 0x6611, "dcdc_ctrl_maxzercnt" },               \
		{ 0x6656, "dcdc_vbat_delta_detect" },            \
		{ 0x66c0, "dcdc_ignore_vbat" },                  \
		{ 0x6700, "enbl_minion" },                       \
		{ 0x6713, "vth_vddpvbat" },                      \
		{ 0x6750, "lpen_vddpvbat" },                     \
		{ 0x6761, "ctrl_rfb" },                          \
		{ 0x6802, "tdm_source_mapping" },                \
		{ 0x6831, "tdm_sourcea_frame_sel" },             \
		{ 0x6851, "tdm_sourceb_frame_sel" },             \
		{ 0x6871, "tdm_source0_clip_sel" },              \
		{ 0x6891, "tdm_source1_clip_sel" },              \
		{ 0x6a02, "rst_min_vbat_delay" },                \
		{ 0x6b00, "disable_auto_engage" },               \
		{ 0x6b10, "disable_engage" },                    \
		{ 0x6c02, "ns_hp2ln_criterion" },                \
		{ 0x6c32, "ns_ln2hp_criterion" },                \
		{ 0x6c69, "spare_out" },                         \
		{ 0x6d0f, "spare_in" },                          \
		{ 0x6e10, "flag_lp_detect_mode1" },              \
		{ 0x6e20, "flag_low_amplitude" },                \
		{ 0x6e30, "flag_vddp_gt_vbat" },                 \
		{ 0x6f02, "cursense_comp_delay" },               \
		{ 0x6f40, "cursense_comp_sign" },                \
		{ 0x6f50, "enbl_cursense_comp" },                \
		{ 0x6f72, "pwms_clip_lvl" },                     \
		{ 0x7033, "boost_cur" },                         \
		{ 0x7071, "bst_slpcmplvl" },                     \
		{ 0x7090, "boost_intel" },                       \
		{ 0x70a0, "boost_speed" },                       \
		{ 0x70e0, "dcdcoff_mode" },                      \
		{ 0x70f0, "dcdc_pwmonly" },                      \
		{ 0x7104, "bst_drive" },                         \
		{ 0x7151, "bst_scalecur" },                      \
		{ 0x7174, "bst_slopecur" },                      \
		{ 0x71c1, "bst_slope" },                         \
		{ 0x71e0, "bst_bypass_bstcur" },                 \
		{ 0x71f0, "bst_bypass_bstfoldback" },            \
		{ 0x7200, "enbl_bst_engage" },                   \
		{ 0x7210, "enbl_bst_hizcom" },                   \
		{ 0x7220, "enbl_bst_peak2avg" },                 \
		{ 0x7230, "enbl_bst_peakcur" },                  \
		{ 0x7240, "enbl_bst_power" },                    \
		{ 0x7250, "enbl_bst_slopecur" },                 \
		{ 0x7260, "enbl_bst_voutcomp" },                 \
		{ 0x7270, "enbl_bst_voutcomp86" },               \
		{ 0x7280, "enbl_bst_voutcomp93" },               \
		{ 0x7290, "enbl_bst_windac" },                   \
		{ 0x72a5, "bst_windac" },                        \
		{ 0x7300, "boost_alg" },                         \
		{ 0x7311, "boost_loopgain" },                    \
		{ 0x7331, "bst_freq" },                          \
		{ 0x7360, "bst_use_new_zercur_detect" },         \
		{ 0x7430, "boost_track" },                       \
		{ 0x7444, "boost_trip_lvl_1st" },                \
		{ 0x7494, "boost_hold_time" },                   \
		{ 0x74e0, "sel_dcdc_envelope_8fs" },             \
		{ 0x74f0, "ignore_flag_voutcomp86" },            \
		{ 0x7534, "boost_trip_lvl_2nd" },                \
		{ 0x7584, "boost_trip_lvl_track" },              \
		{ 0x75f0, "enbl_trip_hyst" },                    \
		{ 0x7635, "frst_boost_voltage" },                \
		{ 0x7695, "scnd_boost_voltage" },                \
		{ 0x8001, "sel_clk_cs" },                        \
		{ 0x8021, "micadc_speed" },                      \
		{ 0x8050, "cs_gain_control" },                   \
		{ 0x8060, "cs_bypass_gc" },                      \
		{ 0x8087, "cs_gain" },                           \
		{ 0x8210, "invertpwm" },                         \
		{ 0x8305, "cs_ktemp" },                          \
		{ 0x8364, "cs_ktemp2" },                         \
		{ 0x8400, "cs_adc_bsoinv" },                     \
		{ 0x8421, "cs_adc_hifreq" },                     \
		{ 0x8440, "cs_adc_nortz" },                      \
		{ 0x8453, "cs_adc_offset" },                     \
		{ 0x8490, "cs_adc_slowdel" },                    \
		{ 0x84a4, "cs_adc_gain" },                       \
		{ 0x8500, "cs_resonator_enable" },               \
		{ 0x8510, "cs_classd_tran_skip" },               \
		{ 0x8530, "cs_inn_short" },                      \
		{ 0x8540, "cs_inp_short" },                      \
		{ 0x8550, "cs_ldo_bypass" },                     \
		{ 0x8560, "cs_ldo_pulldown" },                   \
		{ 0x8574, "cs_ldo_voset" },                      \
		{ 0x8700, "enbl_cs_adc" },                       \
		{ 0x8710, "enbl_cs_inn1" },                      \
		{ 0x8720, "enbl_cs_inn2" },                      \
		{ 0x8730, "enbl_cs_inp1" },                      \
		{ 0x8740, "enbl_cs_inp2" },                      \
		{ 0x8750, "enbl_cs_ldo" },                       \
		{ 0x8780, "enbl_cs_vbatldo" },                   \
		{ 0x8790, "enbl_dc_filter" },                    \
		{ 0x8801, "volsense_pwm_sel" },                  \
		{ 0x8850, "vs_gain_control" },                   \
		{ 0x8860, "vs_bypass_gc" },                      \
		{ 0x8870, "vs_igen_supply" },                    \
		{ 0x8887, "vs_gain" },                           \
		{ 0x8c00, "vs_adc_bsoinv" },                     \
		{ 0x8c40, "vs_adc_nortz" },                      \
		{ 0x8c90, "vs_adc_slowdel" },                    \
		{ 0x8d10, "vs_classd_tran_skip" },               \
		{ 0x8d30, "vs_inn_short" },                      \
		{ 0x8d40, "vs_inp_short" },                      \
		{ 0x8d50, "vs_ldo_bypass" },                     \
		{ 0x8d60, "vs_ldo_pulldown" },                   \
		{ 0x8d74, "vs_ldo_voset" },                      \
		{ 0x8f00, "enbl_vs_adc" },                       \
		{ 0x8f10, "enbl_vs_inn1" },                      \
		{ 0x8f20, "enbl_vs_inn2" },                      \
		{ 0x8f30, "enbl_vs_inp1" },                      \
		{ 0x8f40, "enbl_vs_inp2" },                      \
		{ 0x8f50, "enbl_vs_ldo" },                       \
		{ 0x8f80, "enbl_vs_vbatldo" },                   \
		{ 0xa007, "mtpkey1" },                           \
		{ 0xa107, "mtpkey2" },                           \
		{ 0xa200, "key01_locked" },                      \
		{ 0xa210, "key02_locked" },                      \
		{ 0xa302, "mtp_man_address_in" },                \
		{ 0xa330, "man_copy_mtp_to_iic" },               \
		{ 0xa340, "man_copy_iic_to_mtp" },               \
		{ 0xa350, "auto_copy_mtp_to_iic" },              \
		{ 0xa360, "auto_copy_iic_to_mtp" },              \
		{ 0xa400, "faim_set_clkws" },                    \
		{ 0xa410, "faim_sel_evenrows" },                 \
		{ 0xa420, "faim_sel_oddrows" },                  \
		{ 0xa430, "faim_program_only" },                 \
		{ 0xa440, "faim_erase_only" },                   \
		{ 0xa50f, "mtp_man_data_out_msb" },              \
		{ 0xa60f, "mtp_man_data_out_lsb" },              \
		{ 0xa70f, "mtp_man_data_in_msb" },               \
		{ 0xa80f, "mtp_man_data_in_lsb" },               \
		{ 0xb010, "bypass_ocpcounter" },                 \
		{ 0xb020, "bypass_glitchfilter" },               \
		{ 0xb030, "bypass_ovp" },                        \
		{ 0xb040, "bypass_uvp" },                        \
		{ 0xb050, "bypass_otp" },                        \
		{ 0xb060, "bypass_lost_clk" },                   \
		{ 0xb070, "ctrl_vpalarm" },                      \
		{ 0xb087, "ocp_threshold" },                     \
		{ 0xb108, "ext_temp" },                          \
		{ 0xb190, "ext_temp_sel" },                      \
		{ 0xc000, "use_direct_ctrls" },                  \
		{ 0xc010, "rst_datapath" },                      \
		{ 0xc020, "rst_cgu" },                           \
		{ 0xc038, "enbl_ref" },                          \
		{ 0xc0c0, "use_direct_vs_ctrls" },               \
		{ 0xc0d0, "enbl_ringo" },                        \
		{ 0xc0e0, "use_direct_clk_ctrl" },               \
		{ 0xc0f0, "use_direct_pll_ctrl" },               \
		{ 0xc100, "enbl_tsense" },                       \
		{ 0xc110, "tsense_hibias" },                     \
		{ 0xc120, "enbl_flag_vbg" },                     \
		{ 0xc20f, "abist_offset" },                      \
		{ 0xc300, "bypasslatch" },                       \
		{ 0xc311, "sourcea" },                           \
		{ 0xc331, "sourceb" },                           \
		{ 0xc350, "inverta" },                           \
		{ 0xc360, "invertb" },                           \
		{ 0xc374, "pulselength" },                       \
		{ 0xc3c0, "tdm_enable_loopback" },               \
		{ 0xc400, "bst_bypasslatch" },                   \
		{ 0xc411, "bst_source" },                        \
		{ 0xc430, "bst_invertb" },                       \
		{ 0xc444, "bst_pulselength" },                   \
		{ 0xc490, "test_bst_ctrlsthv" },                 \
		{ 0xc4a0, "test_bst_iddq" },                     \
		{ 0xc4b0, "test_bst_rdson" },                    \
		{ 0xc4c0, "test_bst_cvi" },                      \
		{ 0xc4d0, "test_bst_ocp" },                      \
		{ 0xc4e0, "test_bst_sense" },                    \
		{ 0xc500, "test_cvi" },                          \
		{ 0xc510, "test_discrete" },                     \
		{ 0xc520, "test_iddq" },                         \
		{ 0xc540, "test_rdson" },                        \
		{ 0xc550, "test_sdelta" },                       \
		{ 0xc570, "test_enbl_cs" },                      \
		{ 0xc580, "test_enbl_vs" },                      \
		{ 0xc600, "enbl_pwm_dcc" },                      \
		{ 0xc613, "pwm_dcc_cnt" },                       \
		{ 0xc650, "enbl_ldo_stress" },                   \
		{ 0xc707, "digimuxa_sel" },                      \
		{ 0xc787, "digimuxb_sel" },                      \
		{ 0xc807, "digimuxc_sel" },                      \
		{ 0xc981, "int_ehs" },                           \
		{ 0xc9c0, "hs_mode" },                           \
		{ 0xca00, "enbl_anamux1" },                      \
		{ 0xca10, "enbl_anamux2" },                      \
		{ 0xca20, "enbl_anamux3" },                      \
		{ 0xca30, "enbl_anamux4" },                      \
		{ 0xca74, "anamux1" },                           \
		{ 0xcb04, "anamux2" },                           \
		{ 0xcb53, "anamux3" },                           \
		{ 0xcba3, "anamux4" },                           \
		{ 0xcd05, "pll_seli" },                          \
		{ 0xcd64, "pll_selp" },                          \
		{ 0xcdb3, "pll_selr" },                          \
		{ 0xcdf0, "pll_frm" },                           \
		{ 0xce09, "pll_ndec" },                          \
		{ 0xcea0, "pll_mdec_msb" },                      \
		{ 0xceb0, "enbl_pll" },                          \
		{ 0xcec0, "enbl_osc" },                          \
		{ 0xced0, "pll_bypass" },                        \
		{ 0xcee0, "pll_directi" },                       \
		{ 0xcef0, "pll_directo" },                       \
		{ 0xcf0f, "pll_mdec_lsb" },                      \
		{ 0xd006, "pll_pdec" },                          \
		{ 0xd10f, "tsig_freq_lsb" },                     \
		{ 0xd202, "tsig_freq_msb" },                     \
		{ 0xd230, "inject_tsig" },                       \
		{ 0xd283, "tsig_gain" },                         \
		{ 0xd300, "adc10_reset" },                       \
		{ 0xd311, "adc10_test" },                        \
		{ 0xd332, "adc10_sel" },                         \
		{ 0xd364, "adc10_prog_sample" },                 \
		{ 0xd3b0, "adc10_enbl" },                        \
		{ 0xd3c0, "bypass_lp_vbat" },                    \
		{ 0xd409, "data_adc10_tempbat" },                \
		{ 0xd507, "ctrl_digtoana_hidden" },              \
		{ 0xd580, "enbl_clk_out_of_range" },             \
		{ 0xd621, "clkdiv_audio_sel" },                  \
		{ 0xd641, "clkdiv_muxa_sel" },                   \
		{ 0xd661, "clkdiv_muxb_sel" },                   \
		{ 0xd721, "datao_ehs" },                         \
		{ 0xd740, "bck_ehs" },                           \
		{ 0xd750, "datai_ehs" },                         \
		{ 0xd800, "source_in_testmode" },                \
		{ 0xd810, "gainatt_feedback" },                  \
		{ 0xd822, "test_parametric_io" },                \
		{ 0xd850, "ctrl_bst_clk_lp1" },                  \
		{ 0xd861, "test_spare_out1" },                   \
		{ 0xd880, "bst_dcmbst" },                        \
		{ 0xd8c3, "test_spare_out2" },                   \
		{ 0xee0f, "sw_profile" },                        \
		{ 0xef0f, "sw_vstep" },                          \
		{ 0xf000, "calibration_onetime" },               \
		{ 0xf010, "calibr_ron_done" },                   \
		{ 0xf020, "calibr_dcdc_api_calibrate" },         \
		{ 0xf030, "calibr_dcdc_delta_sign" },            \
		{ 0xf042, "calibr_dcdc_delta" },                 \
		{ 0xf078, "calibr_speaker_info" },               \
		{ 0xf105, "calibr_vout_offset" },                \
		{ 0xf169, "spare_mpt1_15_6" },                   \
		{ 0xf203, "calibr_gain" },                       \
		{ 0xf245, "calibr_offset" },                     \
		{ 0xf2a5, "spare_mtp2_15_10" },                  \
		{ 0xf307, "calibr_gain_vs" },                    \
		{ 0xf387, "calibr_gain_cs" },                    \
		{ 0xf407, "spare_mtp4_15_0" },                   \
		{ 0xf487, "vs_trim" },                           \
		{ 0xf50f, "calibr_R25C_R" },                     \
		{ 0xf60f, "spare_mpt6_6_0" },                    \
		{ 0xf706, "ctrl_offset_a" },                     \
		{ 0xf770, "spare_mtp7_07" },                     \
		{ 0xf786, "ctrl_offset_b" },                     \
		{ 0xf7f0, "spare_mtp7_15" },                     \
		{ 0xf806, "htol_iic_addr" },                     \
		{ 0xf870, "htol_iic_addr_en" },                  \
		{ 0xf884, "calibr_temp_offset" },                \
		{ 0xf8d2, "calibr_temp_gain" },                  \
		{ 0xf900, "mtp_lock_dcdcoff_mode" },             \
		{ 0xf910, "spare_mtp9_1" },                      \
		{ 0xf920, "mtp_lock_bypass_clipper" },           \
		{ 0xf930, "mtp_lock_max_dcdc_voltage" },         \
		{ 0xf943, "calibr_vbg_trim" },                   \
		{ 0xf980, "spare_mtp9_8" },                      \
		{ 0xf990, "mtp_enbl_pwm_delay_clock_gating" },   \
		{ 0xf9a0, "mtp_enbl_ocp_clock_gating" },         \
		{ 0xf9b0, "mtp_gate_cgu_clock_for_test" },       \
		{ 0xf9c0, "mtp_tdm_pad_sel" },                   \
		{ 0xf9d2, "spare_mtp9_15_12" },                  \
		{ 0xfa0f, "mtpdataA" },                          \
		{ 0xfb0f, "mtpdataB" },                          \
		{ 0xfc0f, "mtpdataC" },                          \
		{ 0xfd0f, "mtpdataD" },                          \
		{ 0xfe0f, "mtpdataE" },                          \
		{ 0xff07, "calibr_osc_delta_ndiv" },             \
		{ 0xff87, "spare_mtp7_15_08" },                  \
		{ 0xffff, "Unknown bitfield enum" }              \
	}
enum tfa9874_irq {
	tfa9874_irq_stvdds = 0,
	tfa9874_irq_stbstoc = 1,
	tfa9874_irq_stotds = 2,
	tfa9874_irq_stocpr = 3,
	tfa9874_irq_stuvds = 4,
	tfa9874_irq_stmanalarm = 5,
	tfa9874_irq_sttdmer = 6,
	tfa9874_irq_stnoclk = 7,
	tfa9874_irq_max = 8,
	tfa9874_irq_all = -1
};
#define TFA9874_IRQ_NAMETABLE                                            \
	static struct TfaIrqName Tfa9874IrqNames[] = {                        \
		{ 0, "STVDDS" },  { 1, "STBSTOC" }, { 2, "STOTDS" },     \
		{ 3, "STOCPR" },  { 4, "STUVDS" },  { 5, "STMANALARM" }, \
		{ 6, "STTDMER" }, { 7, "STNOCLK" }, { 8, "8" },          \
	}
#endif
