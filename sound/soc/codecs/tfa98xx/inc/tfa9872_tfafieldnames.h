/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _TFA9872_TFAFIELDNAMES_H
#define _TFA9872_TFAFIELDNAMES_H
#define TFA9872_I2CVERSION_N1A 26
#define TFA9872_I2CVERSION_N1B 29
#define TFA9872_I2CVERSION_N1B2 25
enum nxpTfa9872BfEnumList {
	TFA9872_BF_PWDN = 0x0000,
	TFA9872_BF_I2CR = 0x0010,
	TFA9872_BF_AMPE = 0x0030,
	TFA9872_BF_DCA = 0x0040,
	TFA9872_BF_INTP = 0x0071,
	TFA9872_BF_BYPOCP = 0x00b0,
	TFA9872_BF_TSTOCP = 0x00c0,
	TFA9872_BF_MANSCONF = 0x0120,
	TFA9872_BF_MANAOOSC = 0x0140,
	TFA9872_BF_MUTETO = 0x01d0,
	TFA9872_BF_RCVNS = 0x01e0,
	TFA9872_BF_AUDFS = 0x0203,
	TFA9872_BF_INPLEV = 0x0240,
	TFA9872_BF_FRACTDEL = 0x0255,
	TFA9872_BF_BYPHVBF = 0x02b0,
	TFA9872_BF_REV = 0x030f,
	TFA9872_BF_REFCKEXT = 0x0401,
	TFA9872_BF_REFCKSEL = 0x0420,
	TFA9872_BF_SSE = 0x0510,
	TFA9872_BF_VSE = 0x0530,
	TFA9872_BF_CSE = 0x0550,
	TFA9872_BF_SSPDME = 0x0560,
	TFA9872_BF_PGAE = 0x0580,
	TFA9872_BF_SSTDME = 0x0590,
	TFA9872_BF_SSPBSTE = 0x05a0,
	TFA9872_BF_SSADCE = 0x05b0,
	TFA9872_BF_SSFAIME = 0x05c0,
	TFA9872_BF_STGAIN = 0x0d18,
	TFA9872_BF_STSMUTE = 0x0da0,
	TFA9872_BF_ST1C = 0x0db0,
	TFA9872_BF_VDDS = 0x1000,
	TFA9872_BF_PLLS = 0x1010,
	TFA9872_BF_OTDS = 0x1020,
	TFA9872_BF_OVDS = 0x1030,
	TFA9872_BF_UVDS = 0x1040,
	TFA9872_BF_CLKS = 0x1050,
	TFA9872_BF_MTPB = 0x1060,
	TFA9872_BF_NOCLK = 0x1070,
	TFA9872_BF_SWS = 0x10a0,
	TFA9872_BF_AMPS = 0x10c0,
	TFA9872_BF_AREFS = 0x10d0,
	TFA9872_BF_ADCCR = 0x10e0,
	TFA9872_BF_DCIL = 0x1100,
	TFA9872_BF_DCDCA = 0x1110,
	TFA9872_BF_DCOCPOK = 0x1120,
	TFA9872_BF_DCHVBAT = 0x1140,
	TFA9872_BF_DCH114 = 0x1150,
	TFA9872_BF_DCH107 = 0x1160,
	TFA9872_BF_STMUTEB = 0x1170,
	TFA9872_BF_STMUTE = 0x1180,
	TFA9872_BF_TDMLUTER = 0x1190,
	TFA9872_BF_TDMSTAT = 0x11a2,
	TFA9872_BF_TDMERR = 0x11d0,
	TFA9872_BF_OCPOAP = 0x1300,
	TFA9872_BF_OCPOAN = 0x1310,
	TFA9872_BF_OCPOBP = 0x1320,
	TFA9872_BF_OCPOBN = 0x1330,
	TFA9872_BF_CLIPAH = 0x1340,
	TFA9872_BF_CLIPAL = 0x1350,
	TFA9872_BF_CLIPBH = 0x1360,
	TFA9872_BF_CLIPBL = 0x1370,
	TFA9872_BF_OCDS = 0x1380,
	TFA9872_BF_CLIPS = 0x1390,
	TFA9872_BF_OCPOKMC = 0x13a0,
	TFA9872_BF_MANALARM = 0x13b0,
	TFA9872_BF_MANWAIT1 = 0x13c0,
	TFA9872_BF_MANMUTE = 0x13e0,
	TFA9872_BF_MANOPER = 0x13f0,
	TFA9872_BF_CLKOOR = 0x1420,
	TFA9872_BF_MANSTATE = 0x1433,
	TFA9872_BF_DCMODE = 0x1471,
	TFA9872_BF_BATS = 0x1509,
	TFA9872_BF_TEMPS = 0x1608,
	TFA9872_BF_VDDPS = 0x1709,
	TFA9872_BF_TDME = 0x2040,
	TFA9872_BF_TDMMODE = 0x2050,
	TFA9872_BF_TDMCLINV = 0x2060,
	TFA9872_BF_TDMFSLN = 0x2073,
	TFA9872_BF_TDMFSPOL = 0x20b0,
	TFA9872_BF_TDMNBCK = 0x20c3,
	TFA9872_BF_TDMSLOTS = 0x2103,
	TFA9872_BF_TDMSLLN = 0x2144,
	TFA9872_BF_TDMBRMG = 0x2194,
	TFA9872_BF_TDMDEL = 0x21e0,
	TFA9872_BF_TDMADJ = 0x21f0,
	TFA9872_BF_TDMOOMP = 0x2201,
	TFA9872_BF_TDMSSIZE = 0x2224,
	TFA9872_BF_TDMTXDFO = 0x2271,
	TFA9872_BF_TDMTXUS0 = 0x2291,
	TFA9872_BF_TDMSPKE = 0x2300,
	TFA9872_BF_TDMDCE = 0x2310,
	TFA9872_BF_TDMCSE = 0x2330,
	TFA9872_BF_TDMVSE = 0x2340,
	TFA9872_BF_TDMSPKS = 0x2603,
	TFA9872_BF_TDMDCS = 0x2643,
	TFA9872_BF_TDMCSS = 0x26c3,
	TFA9872_BF_TDMVSS = 0x2703,
	TFA9872_BF_PDMSTSEL = 0x3111,
	TFA9872_BF_ISTVDDS = 0x4000,
	TFA9872_BF_ISTPLLS = 0x4010,
	TFA9872_BF_ISTOTDS = 0x4020,
	TFA9872_BF_ISTOVDS = 0x4030,
	TFA9872_BF_ISTUVDS = 0x4040,
	TFA9872_BF_ISTCLKS = 0x4050,
	TFA9872_BF_ISTMTPB = 0x4060,
	TFA9872_BF_ISTNOCLK = 0x4070,
	TFA9872_BF_ISTSWS = 0x40a0,
	TFA9872_BF_ISTAMPS = 0x40c0,
	TFA9872_BF_ISTAREFS = 0x40d0,
	TFA9872_BF_ISTADCCR = 0x40e0,
	TFA9872_BF_ISTBSTCU = 0x4100,
	TFA9872_BF_ISTBSTHI = 0x4110,
	TFA9872_BF_ISTBSTOC = 0x4120,
	TFA9872_BF_ISTBSTPKCUR = 0x4130,
	TFA9872_BF_ISTBSTVC = 0x4140,
	TFA9872_BF_ISTBST86 = 0x4150,
	TFA9872_BF_ISTBST93 = 0x4160,
	TFA9872_BF_ISTOCPR = 0x4190,
	TFA9872_BF_ISTMWSRC = 0x41a0,
	TFA9872_BF_ISTMWSMU = 0x41c0,
	TFA9872_BF_ISTCLKOOR = 0x41f0,
	TFA9872_BF_ISTTDMER = 0x4200,
	TFA9872_BF_ISTCLPR = 0x4220,
	TFA9872_BF_ISTLP0 = 0x4240,
	TFA9872_BF_ISTLP1 = 0x4250,
	TFA9872_BF_ISTLA = 0x4260,
	TFA9872_BF_ISTVDDPH = 0x4270,
	TFA9872_BF_ICLVDDS = 0x4400,
	TFA9872_BF_ICLPLLS = 0x4410,
	TFA9872_BF_ICLOTDS = 0x4420,
	TFA9872_BF_ICLOVDS = 0x4430,
	TFA9872_BF_ICLUVDS = 0x4440,
	TFA9872_BF_ICLCLKS = 0x4450,
	TFA9872_BF_ICLMTPB = 0x4460,
	TFA9872_BF_ICLNOCLK = 0x4470,
	TFA9872_BF_ICLSWS = 0x44a0,
	TFA9872_BF_ICLAMPS = 0x44c0,
	TFA9872_BF_ICLAREFS = 0x44d0,
	TFA9872_BF_ICLADCCR = 0x44e0,
	TFA9872_BF_ICLBSTCU = 0x4500,
	TFA9872_BF_ICLBSTHI = 0x4510,
	TFA9872_BF_ICLBSTOC = 0x4520,
	TFA9872_BF_ICLBSTPC = 0x4530,
	TFA9872_BF_ICLBSTVC = 0x4540,
	TFA9872_BF_ICLBST86 = 0x4550,
	TFA9872_BF_ICLBST93 = 0x4560,
	TFA9872_BF_ICLOCPR = 0x4590,
	TFA9872_BF_ICLMWSRC = 0x45a0,
	TFA9872_BF_ICLMWSMU = 0x45c0,
	TFA9872_BF_ICLCLKOOR = 0x45f0,
	TFA9872_BF_ICLTDMER = 0x4600,
	TFA9872_BF_ICLCLPR = 0x4620,
	TFA9872_BF_ICLLP0 = 0x4640,
	TFA9872_BF_ICLLP1 = 0x4650,
	TFA9872_BF_ICLLA = 0x4660,
	TFA9872_BF_ICLVDDPH = 0x4670,
	TFA9872_BF_IEVDDS = 0x4800,
	TFA9872_BF_IEPLLS = 0x4810,
	TFA9872_BF_IEOTDS = 0x4820,
	TFA9872_BF_IEOVDS = 0x4830,
	TFA9872_BF_IEUVDS = 0x4840,
	TFA9872_BF_IECLKS = 0x4850,
	TFA9872_BF_IEMTPB = 0x4860,
	TFA9872_BF_IENOCLK = 0x4870,
	TFA9872_BF_IESWS = 0x48a0,
	TFA9872_BF_IEAMPS = 0x48c0,
	TFA9872_BF_IEAREFS = 0x48d0,
	TFA9872_BF_IEADCCR = 0x48e0,
	TFA9872_BF_IEBSTCU = 0x4900,
	TFA9872_BF_IEBSTHI = 0x4910,
	TFA9872_BF_IEBSTOC = 0x4920,
	TFA9872_BF_IEBSTPC = 0x4930,
	TFA9872_BF_IEBSTVC = 0x4940,
	TFA9872_BF_IEBST86 = 0x4950,
	TFA9872_BF_IEBST93 = 0x4960,
	TFA9872_BF_IEOCPR = 0x4990,
	TFA9872_BF_IEMWSRC = 0x49a0,
	TFA9872_BF_IEMWSMU = 0x49c0,
	TFA9872_BF_IECLKOOR = 0x49f0,
	TFA9872_BF_IETDMER = 0x4a00,
	TFA9872_BF_IECLPR = 0x4a20,
	TFA9872_BF_IELP0 = 0x4a40,
	TFA9872_BF_IELP1 = 0x4a50,
	TFA9872_BF_IELA = 0x4a60,
	TFA9872_BF_IEVDDPH = 0x4a70,
	TFA9872_BF_IPOVDDS = 0x4c00,
	TFA9872_BF_IPOPLLS = 0x4c10,
	TFA9872_BF_IPOOTDS = 0x4c20,
	TFA9872_BF_IPOOVDS = 0x4c30,
	TFA9872_BF_IPOUVDS = 0x4c40,
	TFA9872_BF_IPOCLKS = 0x4c50,
	TFA9872_BF_IPOMTPB = 0x4c60,
	TFA9872_BF_IPONOCLK = 0x4c70,
	TFA9872_BF_IPOSWS = 0x4ca0,
	TFA9872_BF_IPOAMPS = 0x4cc0,
	TFA9872_BF_IPOAREFS = 0x4cd0,
	TFA9872_BF_IPOADCCR = 0x4ce0,
	TFA9872_BF_IPOBSTCU = 0x4d00,
	TFA9872_BF_IPOBSTHI = 0x4d10,
	TFA9872_BF_IPOBSTOC = 0x4d20,
	TFA9872_BF_IPOBSTPC = 0x4d30,
	TFA9872_BF_IPOBSTVC = 0x4d40,
	TFA9872_BF_IPOBST86 = 0x4d50,
	TFA9872_BF_IPOBST93 = 0x4d60,
	TFA9872_BF_IPOOCPR = 0x4d90,
	TFA9872_BF_IPOMWSRC = 0x4da0,
	TFA9872_BF_IPOMWSMU = 0x4dc0,
	TFA9872_BF_IPCLKOOR = 0x4df0,
	TFA9872_BF_IPOTDMER = 0x4e00,
	TFA9872_BF_IPOCLPR = 0x4e20,
	TFA9872_BF_IPOLP0 = 0x4e40,
	TFA9872_BF_IPOLP1 = 0x4e50,
	TFA9872_BF_IPOLA = 0x4e60,
	TFA9872_BF_IPOVDDPH = 0x4e70,
	TFA9872_BF_BSSCR = 0x5001,
	TFA9872_BF_BSST = 0x5023,
	TFA9872_BF_BSSRL = 0x5061,
	TFA9872_BF_BSSR = 0x50e0,
	TFA9872_BF_BSSBY = 0x50f0,
	TFA9872_BF_BSSS = 0x5100,
	TFA9872_BF_INTSMUTE = 0x5110,
	TFA9872_BF_HPFBYP = 0x5150,
	TFA9872_BF_DPSA = 0x5170,
	TFA9872_BF_CLIPCTRL = 0x5222,
	TFA9872_BF_AMPGAIN = 0x5257,
	TFA9872_BF_SLOPEE = 0x52d0,
	TFA9872_BF_SLOPESET = 0x52e0,
	TFA9872_BF_PGAGAIN = 0x6081,
	TFA9872_BF_PGALPE = 0x60b0,
	TFA9872_BF_LPM0BYP = 0x6110,
	TFA9872_BF_TDMDCG = 0x6123,
	TFA9872_BF_TDMSPKG = 0x6163,
	TFA9872_BF_STIDLEEN = 0x61b0,
	TFA9872_BF_LNMODE = 0x62e1,
	TFA9872_BF_LPM1MODE = 0x64e1,
	TFA9872_BF_LPM1DIS = 0x65c0,
	TFA9872_BF_TDMSRCMAP = 0x6801,
	TFA9872_BF_TDMSRCAS = 0x6821,
	TFA9872_BF_TDMSRCBS = 0x6841,
	TFA9872_BF_ANCSEL = 0x6881,
	TFA9872_BF_ANC1C = 0x68a0,
	TFA9872_BF_SAMMODE = 0x6901,
	TFA9872_BF_SAMSEL = 0x6920,
	TFA9872_BF_PDMOSELH = 0x6931,
	TFA9872_BF_PDMOSELL = 0x6951,
	TFA9872_BF_SAMOSEL = 0x6970,
	TFA9872_BF_LP0 = 0x6e00,
	TFA9872_BF_LP1 = 0x6e10,
	TFA9872_BF_LA = 0x6e20,
	TFA9872_BF_VDDPH = 0x6e30,
	TFA9872_BF_DELCURCOMP = 0x6f02,
	TFA9872_BF_SIGCURCOMP = 0x6f40,
	TFA9872_BF_ENCURCOMP = 0x6f50,
	TFA9872_BF_SELCLPPWM = 0x6f60,
	TFA9872_BF_LVLCLPPWM = 0x6f72,
	TFA9872_BF_DCVOS = 0x7002,
	TFA9872_BF_DCMCC = 0x7033,
	TFA9872_BF_DCCV = 0x7071,
	TFA9872_BF_DCIE = 0x7090,
	TFA9872_BF_DCSR = 0x70a0,
	TFA9872_BF_DCDIS = 0x70e0,
	TFA9872_BF_DCPWM = 0x70f0,
	TFA9872_BF_DCVOF = 0x7402,
	TFA9872_BF_DCTRACK = 0x7430,
	TFA9872_BF_DCTRIP = 0x7444,
	TFA9872_BF_DCHOLD = 0x7494,
	TFA9872_BF_DCTRIP2 = 0x7534,
	TFA9872_BF_DCTRIPT = 0x7584,
	TFA9872_BF_MTPK = 0xa107,
	TFA9872_BF_KEY1LOCKED = 0xa200,
	TFA9872_BF_KEY2LOCKED = 0xa210,
	TFA9872_BF_CMTPI = 0xa350,
	TFA9872_BF_CIMTP = 0xa360,
	TFA9872_BF_MTPRDMSB = 0xa50f,
	TFA9872_BF_MTPRDLSB = 0xa60f,
	TFA9872_BF_EXTTS = 0xb108,
	TFA9872_BF_TROS = 0xb190,
	TFA9872_BF_SWPROFIL = 0xee0f,
	TFA9872_BF_SWVSTEP = 0xef0f,
	TFA9872_BF_MTPOTC = 0xf000,
	TFA9872_BF_MTPEX = 0xf010,
	TFA9872_BF_DCMCCAPI = 0xf020,
	TFA9872_BF_DCMCCSB = 0xf030,
	TFA9872_BF_USERDEF = 0xf042,
	TFA9872_BF_CUSTINFO = 0xf078,
	TFA9872_BF_R25C = 0xf50f,
};
#define TFA9872_NAMETABLE                                   \
	static struct TfaBfName Tfa9872DatasheetNames[] = { \
		{ 0x0, "PWDN" },                            \
		{ 0x10, "I2CR" },                           \
		{ 0x30, "AMPE" },                           \
		{ 0x40, "DCA" },                            \
		{ 0x71, "INTP" },                           \
		{ 0xb0, "BYPOCP" },                         \
		{ 0xc0, "TSTOCP" },                         \
		{ 0x120, "MANSCONF" },                      \
		{ 0x140, "MANAOOSC" },                      \
		{ 0x1d0, "MUTETO" },                        \
		{ 0x1e0, "RCVNS" },                         \
		{ 0x203, "AUDFS" },                         \
		{ 0x240, "INPLEV" },                        \
		{ 0x255, "FRACTDEL" },                      \
		{ 0x2b0, "BYPHVBF" },                       \
		{ 0x30f, "REV" },                           \
		{ 0x401, "REFCKEXT" },                      \
		{ 0x420, "REFCKSEL" },                      \
		{ 0x510, "SSE" },                           \
		{ 0x530, "VSE" },                           \
		{ 0x550, "CSE" },                           \
		{ 0x560, "SSPDME" },                        \
		{ 0x580, "PGAE" },                          \
		{ 0x590, "SSTDME" },                        \
		{ 0x5a0, "SSPBSTE" },                       \
		{ 0x5b0, "SSADCE" },                        \
		{ 0x5c0, "SSFAIME" },                       \
		{ 0xd18, "STGAIN" },                        \
		{ 0xda0, "STSMUTE" },                       \
		{ 0xdb0, "ST1C" },                          \
		{ 0x1000, "VDDS" },                         \
		{ 0x1010, "PLLS" },                         \
		{ 0x1020, "OTDS" },                         \
		{ 0x1030, "OVDS" },                         \
		{ 0x1040, "UVDS" },                         \
		{ 0x1050, "CLKS" },                         \
		{ 0x1060, "MTPB" },                         \
		{ 0x1070, "NOCLK" },                        \
		{ 0x10a0, "SWS" },                          \
		{ 0x10c0, "AMPS" },                         \
		{ 0x10d0, "AREFS" },                        \
		{ 0x10e0, "ADCCR" },                        \
		{ 0x1100, "DCIL" },                         \
		{ 0x1110, "DCDCA" },                        \
		{ 0x1120, "DCOCPOK" },                      \
		{ 0x1140, "DCHVBAT" },                      \
		{ 0x1150, "DCH114" },                       \
		{ 0x1160, "DCH107" },                       \
		{ 0x1170, "STMUTEB" },                      \
		{ 0x1180, "STMUTE" },                       \
		{ 0x1190, "TDMLUTER" },                     \
		{ 0x11a2, "TDMSTAT" },                      \
		{ 0x11d0, "TDMERR" },                       \
		{ 0x1300, "OCPOAP" },                       \
		{ 0x1310, "OCPOAN" },                       \
		{ 0x1320, "OCPOBP" },                       \
		{ 0x1330, "OCPOBN" },                       \
		{ 0x1340, "CLIPAH" },                       \
		{ 0x1350, "CLIPAL" },                       \
		{ 0x1360, "CLIPBH" },                       \
		{ 0x1370, "CLIPBL" },                       \
		{ 0x1380, "OCDS" },                         \
		{ 0x1390, "CLIPS" },                        \
		{ 0x13a0, "OCPOKMC" },                      \
		{ 0x13b0, "MANALARM" },                     \
		{ 0x13c0, "MANWAIT1" },                     \
		{ 0x13e0, "MANMUTE" },                      \
		{ 0x13f0, "MANOPER" },                      \
		{ 0x1420, "CLKOOR" },                       \
		{ 0x1433, "MANSTATE" },                     \
		{ 0x1471, "DCMODE" },                       \
		{ 0x1509, "BATS" },                         \
		{ 0x1608, "TEMPS" },                        \
		{ 0x1709, "VDDPS" },                        \
		{ 0x2040, "TDME" },                         \
		{ 0x2050, "TDMMODE" },                      \
		{ 0x2060, "TDMCLINV" },                     \
		{ 0x2073, "TDMFSLN" },                      \
		{ 0x20b0, "TDMFSPOL" },                     \
		{ 0x20c3, "TDMNBCK" },                      \
		{ 0x2103, "TDMSLOTS" },                     \
		{ 0x2144, "TDMSLLN" },                      \
		{ 0x2194, "TDMBRMG" },                      \
		{ 0x21e0, "TDMDEL" },                       \
		{ 0x21f0, "TDMADJ" },                       \
		{ 0x2201, "TDMOOMP" },                      \
		{ 0x2224, "TDMSSIZE" },                     \
		{ 0x2271, "TDMTXDFO" },                     \
		{ 0x2291, "TDMTXUS0" },                     \
		{ 0x2300, "TDMSPKE" },                      \
		{ 0x2310, "TDMDCE" },                       \
		{ 0x2330, "TDMCSE" },                       \
		{ 0x2340, "TDMVSE" },                       \
		{ 0x2603, "TDMSPKS" },                      \
		{ 0x2643, "TDMDCS" },                       \
		{ 0x26c3, "TDMCSS" },                       \
		{ 0x2703, "TDMVSS" },                       \
		{ 0x3111, "PDMSTSEL" },                     \
		{ 0x4000, "ISTVDDS" },                      \
		{ 0x4010, "ISTPLLS" },                      \
		{ 0x4020, "ISTOTDS" },                      \
		{ 0x4030, "ISTOVDS" },                      \
		{ 0x4040, "ISTUVDS" },                      \
		{ 0x4050, "ISTCLKS" },                      \
		{ 0x4060, "ISTMTPB" },                      \
		{ 0x4070, "ISTNOCLK" },                     \
		{ 0x40a0, "ISTSWS" },                       \
		{ 0x40c0, "ISTAMPS" },                      \
		{ 0x40d0, "ISTAREFS" },                     \
		{ 0x40e0, "ISTADCCR" },                     \
		{ 0x4100, "ISTBSTCU" },                     \
		{ 0x4110, "ISTBSTHI" },                     \
		{ 0x4120, "ISTBSTOC" },                     \
		{ 0x4130, "ISTBSTPKCUR" },                  \
		{ 0x4140, "ISTBSTVC" },                     \
		{ 0x4150, "ISTBST86" },                     \
		{ 0x4160, "ISTBST93" },                     \
		{ 0x4190, "ISTOCPR" },                      \
		{ 0x41a0, "ISTMWSRC" },                     \
		{ 0x41c0, "ISTMWSMU" },                     \
		{ 0x41f0, "ISTCLKOOR" },                    \
		{ 0x4200, "ISTTDMER" },                     \
		{ 0x4220, "ISTCLPR" },                      \
		{ 0x4240, "ISTLP0" },                       \
		{ 0x4250, "ISTLP1" },                       \
		{ 0x4260, "ISTLA" },                        \
		{ 0x4270, "ISTVDDPH" },                     \
		{ 0x4400, "ICLVDDS" },                      \
		{ 0x4410, "ICLPLLS" },                      \
		{ 0x4420, "ICLOTDS" },                      \
		{ 0x4430, "ICLOVDS" },                      \
		{ 0x4440, "ICLUVDS" },                      \
		{ 0x4450, "ICLCLKS" },                      \
		{ 0x4460, "ICLMTPB" },                      \
		{ 0x4470, "ICLNOCLK" },                     \
		{ 0x44a0, "ICLSWS" },                       \
		{ 0x44c0, "ICLAMPS" },                      \
		{ 0x44d0, "ICLAREFS" },                     \
		{ 0x44e0, "ICLADCCR" },                     \
		{ 0x4500, "ICLBSTCU" },                     \
		{ 0x4510, "ICLBSTHI" },                     \
		{ 0x4520, "ICLBSTOC" },                     \
		{ 0x4530, "ICLBSTPC" },                     \
		{ 0x4540, "ICLBSTVC" },                     \
		{ 0x4550, "ICLBST86" },                     \
		{ 0x4560, "ICLBST93" },                     \
		{ 0x4590, "ICLOCPR" },                      \
		{ 0x45a0, "ICLMWSRC" },                     \
		{ 0x45c0, "ICLMWSMU" },                     \
		{ 0x45f0, "ICLCLKOOR" },                    \
		{ 0x4600, "ICLTDMER" },                     \
		{ 0x4620, "ICLCLPR" },                      \
		{ 0x4640, "ICLLP0" },                       \
		{ 0x4650, "ICLLP1" },                       \
		{ 0x4660, "ICLLA" },                        \
		{ 0x4670, "ICLVDDPH" },                     \
		{ 0x4800, "IEVDDS" },                       \
		{ 0x4810, "IEPLLS" },                       \
		{ 0x4820, "IEOTDS" },                       \
		{ 0x4830, "IEOVDS" },                       \
		{ 0x4840, "IEUVDS" },                       \
		{ 0x4850, "IECLKS" },                       \
		{ 0x4860, "IEMTPB" },                       \
		{ 0x4870, "IENOCLK" },                      \
		{ 0x48a0, "IESWS" },                        \
		{ 0x48c0, "IEAMPS" },                       \
		{ 0x48d0, "IEAREFS" },                      \
		{ 0x48e0, "IEADCCR" },                      \
		{ 0x4900, "IEBSTCU" },                      \
		{ 0x4910, "IEBSTHI" },                      \
		{ 0x4920, "IEBSTOC" },                      \
		{ 0x4930, "IEBSTPC" },                      \
		{ 0x4940, "IEBSTVC" },                      \
		{ 0x4950, "IEBST86" },                      \
		{ 0x4960, "IEBST93" },                      \
		{ 0x4990, "IEOCPR" },                       \
		{ 0x49a0, "IEMWSRC" },                      \
		{ 0x49c0, "IEMWSMU" },                      \
		{ 0x49f0, "IECLKOOR" },                     \
		{ 0x4a00, "IETDMER" },                      \
		{ 0x4a20, "IECLPR" },                       \
		{ 0x4a40, "IELP0" },                        \
		{ 0x4a50, "IELP1" },                        \
		{ 0x4a60, "IELA" },                         \
		{ 0x4a70, "IEVDDPH" },                      \
		{ 0x4c00, "IPOVDDS" },                      \
		{ 0x4c10, "IPOPLLS" },                      \
		{ 0x4c20, "IPOOTDS" },                      \
		{ 0x4c30, "IPOOVDS" },                      \
		{ 0x4c40, "IPOUVDS" },                      \
		{ 0x4c50, "IPOCLKS" },                      \
		{ 0x4c60, "IPOMTPB" },                      \
		{ 0x4c70, "IPONOCLK" },                     \
		{ 0x4ca0, "IPOSWS" },                       \
		{ 0x4cc0, "IPOAMPS" },                      \
		{ 0x4cd0, "IPOAREFS" },                     \
		{ 0x4ce0, "IPOADCCR" },                     \
		{ 0x4d00, "IPOBSTCU" },                     \
		{ 0x4d10, "IPOBSTHI" },                     \
		{ 0x4d20, "IPOBSTOC" },                     \
		{ 0x4d30, "IPOBSTPC" },                     \
		{ 0x4d40, "IPOBSTVC" },                     \
		{ 0x4d50, "IPOBST86" },                     \
		{ 0x4d60, "IPOBST93" },                     \
		{ 0x4d90, "IPOOCPR" },                      \
		{ 0x4da0, "IPOMWSRC" },                     \
		{ 0x4dc0, "IPOMWSMU" },                     \
		{ 0x4df0, "IPCLKOOR" },                     \
		{ 0x4e00, "IPOTDMER" },                     \
		{ 0x4e20, "IPOCLPR" },                      \
		{ 0x4e40, "IPOLP0" },                       \
		{ 0x4e50, "IPOLP1" },                       \
		{ 0x4e60, "IPOLA" },                        \
		{ 0x4e70, "IPOVDDPH" },                     \
		{ 0x5001, "BSSCR" },                        \
		{ 0x5023, "BSST" },                         \
		{ 0x5061, "BSSRL" },                        \
		{ 0x50e0, "BSSR" },                         \
		{ 0x50f0, "BSSBY" },                        \
		{ 0x5100, "BSSS" },                         \
		{ 0x5110, "INTSMUTE" },                     \
		{ 0x5150, "HPFBYP" },                       \
		{ 0x5170, "DPSA" },                         \
		{ 0x5222, "CLIPCTRL" },                     \
		{ 0x5257, "AMPGAIN" },                      \
		{ 0x52d0, "SLOPEE" },                       \
		{ 0x52e0, "SLOPESET" },                     \
		{ 0x6081, "PGAGAIN" },                      \
		{ 0x60b0, "PGALPE" },                       \
		{ 0x6110, "LPM0BYP" },                      \
		{ 0x6123, "TDMDCG" },                       \
		{ 0x6163, "TDMSPKG" },                      \
		{ 0x61b0, "STIDLEEN" },                     \
		{ 0x62e1, "LNMODE" },                       \
		{ 0x64e1, "LPM1MODE" },                     \
		{ 0x65c0, "LPM1DIS" },                      \
		{ 0x6801, "TDMSRCMAP" },                    \
		{ 0x6821, "TDMSRCAS" },                     \
		{ 0x6841, "TDMSRCBS" },                     \
		{ 0x6881, "ANCSEL" },                       \
		{ 0x68a0, "ANC1C" },                        \
		{ 0x6901, "SAMMODE" },                      \
		{ 0x6920, "SAMSEL" },                       \
		{ 0x6931, "PDMOSELH" },                     \
		{ 0x6951, "PDMOSELL" },                     \
		{ 0x6970, "SAMOSEL" },                      \
		{ 0x6e00, "LP0" },                          \
		{ 0x6e10, "LP1" },                          \
		{ 0x6e20, "LA" },                           \
		{ 0x6e30, "VDDPH" },                        \
		{ 0x6f02, "DELCURCOMP" },                   \
		{ 0x6f40, "SIGCURCOMP" },                   \
		{ 0x6f50, "ENCURCOMP" },                    \
		{ 0x6f60, "SELCLPPWM" },                    \
		{ 0x6f72, "LVLCLPPWM" },                    \
		{ 0x7002, "DCVOS" },                        \
		{ 0x7033, "DCMCC" },                        \
		{ 0x7071, "DCCV" },                         \
		{ 0x7090, "DCIE" },                         \
		{ 0x70a0, "DCSR" },                         \
		{ 0x70e0, "DCDIS" },                        \
		{ 0x70f0, "DCPWM" },                        \
		{ 0x7402, "DCVOF" },                        \
		{ 0x7430, "DCTRACK" },                      \
		{ 0x7444, "DCTRIP" },                       \
		{ 0x7494, "DCHOLD" },                       \
		{ 0x7534, "DCTRIP2" },                      \
		{ 0x7584, "DCTRIPT" },                      \
		{ 0xa107, "MTPK" },                         \
		{ 0xa200, "KEY1LOCKED" },                   \
		{ 0xa210, "KEY2LOCKED" },                   \
		{ 0xa350, "CMTPI" },                        \
		{ 0xa360, "CIMTP" },                        \
		{ 0xa50f, "MTPRDMSB" },                     \
		{ 0xa60f, "MTPRDLSB" },                     \
		{ 0xb108, "EXTTS" },                        \
		{ 0xb190, "TROS" },                         \
		{ 0xee0f, "SWPROFIL" },                     \
		{ 0xef0f, "SWVSTEP" },                      \
		{ 0xf000, "MTPOTC" },                       \
		{ 0xf010, "MTPEX" },                        \
		{ 0xf020, "DCMCCAPI" },                     \
		{ 0xf030, "DCMCCSB" },                      \
		{ 0xf042, "USERDEF" },                      \
		{ 0xf078, "CUSTINFO" },                     \
		{ 0xf50f, "R25C" },                         \
		{ 0xffff, "Unknown bitfield enum" }         \
	}
#define TFA9872_BITNAMETABLE                                           \
	static struct TfaBfName Tfa9872BitNames[] = {                  \
		{ 0x0, "powerdown" },                                  \
		{ 0x10, "reset" },                                     \
		{ 0x30, "enbl_amplifier" },                            \
		{ 0x40, "enbl_boost" },                                \
		{ 0x71, "int_pad_io" },                                \
		{ 0xb0, "bypass_ocp" },                                \
		{ 0xc0, "test_ocp" },                                  \
		{ 0x120, "src_set_configured" },                       \
		{ 0x140, "enbl_osc1m_auto_off" },                      \
		{ 0x1d0, "disable_mute_time_out" },                    \
		{ 0x203, "audio_fs" },                                 \
		{ 0x240, "input_level" },                              \
		{ 0x255, "cs_frac_delay" },                            \
		{ 0x2b0, "bypass_hvbat_filter" },                      \
		{ 0x2d0, "sel_hysteresis" },                           \
		{ 0x30f, "device_rev" },                               \
		{ 0x401, "pll_clkin_sel" },                            \
		{ 0x420, "pll_clkin_sel_osc" },                        \
		{ 0x510, "enbl_spkr_ss" },                             \
		{ 0x530, "enbl_volsense" },                            \
		{ 0x550, "enbl_cursense" },                            \
		{ 0x560, "enbl_pdm_ss" },                              \
		{ 0x580, "enbl_pga_chop" },                            \
		{ 0x590, "enbl_tdm_ss" },                              \
		{ 0x5a0, "enbl_bst_ss" },                              \
		{ 0x5b0, "enbl_adc_ss" },                              \
		{ 0x5c0, "enbl_faim_ss" },                             \
		{ 0xd18, "side_tone_gain" },                           \
		{ 0xda0, "mute_side_tone" },                           \
		{ 0xdb0, "side_tone_1scomplement" },                   \
		{ 0xe07, "ctrl_digtoana" },                            \
		{ 0xf0f, "hidden_code" },                              \
		{ 0x1000, "flag_por" },                                \
		{ 0x1010, "flag_pll_lock" },                           \
		{ 0x1020, "flag_otpok" },                              \
		{ 0x1030, "flag_ovpok" },                              \
		{ 0x1040, "flag_uvpok" },                              \
		{ 0x1050, "flag_clocks_stable" },                      \
		{ 0x1060, "flag_mtp_busy" },                           \
		{ 0x1070, "flag_lost_clk" },                           \
		{ 0x10a0, "flag_engage" },                             \
		{ 0x10c0, "flag_enbl_amp" },                           \
		{ 0x10d0, "flag_enbl_ref" },                           \
		{ 0x10e0, "flag_adc10_ready" },                        \
		{ 0x1100, "flag_bst_bstcur" },                         \
		{ 0x1110, "flag_bst_hiz" },                            \
		{ 0x1120, "flag_bst_ocpok" },                          \
		{ 0x1130, "flag_bst_peakcur" },                        \
		{ 0x1140, "flag_bst_voutcomp" },                       \
		{ 0x1150, "flag_bst_voutcomp86" },                     \
		{ 0x1160, "flag_bst_voutcomp93" },                     \
		{ 0x1170, "flag_soft_mute_busy" },                     \
		{ 0x1180, "flag_soft_mute_state" },                    \
		{ 0x1190, "flag_tdm_lut_error" },                      \
		{ 0x11a2, "flag_tdm_status" },                         \
		{ 0x11d0, "flag_tdm_error" },                          \
		{ 0x1300, "flag_ocpokap" },                            \
		{ 0x1310, "flag_ocpokan" },                            \
		{ 0x1320, "flag_ocpokbp" },                            \
		{ 0x1330, "flag_ocpokbn" },                            \
		{ 0x1340, "flag_clipa_high" },                         \
		{ 0x1350, "flag_clipa_low" },                          \
		{ 0x1360, "flag_clipb_high" },                         \
		{ 0x1370, "flag_clipb_low" },                          \
		{ 0x1380, "flag_ocp_alarm" },                          \
		{ 0x1390, "flag_clip" },                               \
		{ 0x13b0, "flag_man_alarm_state" },                    \
		{ 0x13c0, "flag_man_wait_src_settings" },              \
		{ 0x13e0, "flag_man_start_mute_audio" },               \
		{ 0x13f0, "flag_man_operating_state" },                \
		{ 0x1420, "flag_clk_out_of_range" },                   \
		{ 0x1433, "man_state" },                               \
		{ 0x1471, "status_bst_mode" },                         \
		{ 0x1509, "bat_adc" },                                 \
		{ 0x1608, "temp_adc" },                                \
		{ 0x1709, "vddp_adc" },                                \
		{ 0x2040, "tdm_enable" },                              \
		{ 0x2050, "tdm_mode" },                                \
		{ 0x2060, "tdm_clk_inversion" },                       \
		{ 0x2073, "tdm_fs_ws_length" },                        \
		{ 0x20b0, "tdm_fs_ws_polarity" },                      \
		{ 0x20c3, "tdm_nbck" },                                \
		{ 0x2103, "tdm_nb_of_slots" },                         \
		{ 0x2144, "tdm_slot_length" },                         \
		{ 0x2194, "tdm_bits_remaining" },                      \
		{ 0x21e0, "tdm_data_delay" },                          \
		{ 0x21f0, "tdm_data_adjustment" },                     \
		{ 0x2201, "tdm_audio_sample_compression" },            \
		{ 0x2224, "tdm_sample_size" },                         \
		{ 0x2271, "tdm_txdata_format" },                       \
		{ 0x2291, "tdm_txdata_format_unused_slot_sd0" },       \
		{ 0x2300, "tdm_sink0_enable" },                        \
		{ 0x2310, "tdm_sink1_enable" },                        \
		{ 0x2330, "tdm_source0_enable" },                      \
		{ 0x2340, "tdm_source1_enable" },                      \
		{ 0x2603, "tdm_sink0_slot" },                          \
		{ 0x2643, "tdm_sink1_slot" },                          \
		{ 0x26c3, "tdm_source0_slot" },                        \
		{ 0x2703, "tdm_source1_slot" },                        \
		{ 0x3111, "pdm_side_tone_sel" },                       \
		{ 0x3201, "pdm_nbck" },                                \
		{ 0x4000, "int_out_flag_por" },                        \
		{ 0x4010, "int_out_flag_pll_lock" },                   \
		{ 0x4020, "int_out_flag_otpok" },                      \
		{ 0x4030, "int_out_flag_ovpok" },                      \
		{ 0x4040, "int_out_flag_uvpok" },                      \
		{ 0x4050, "int_out_flag_clocks_stable" },              \
		{ 0x4060, "int_out_flag_mtp_busy" },                   \
		{ 0x4070, "int_out_flag_lost_clk" },                   \
		{ 0x40a0, "int_out_flag_engage" },                     \
		{ 0x40c0, "int_out_flag_enbl_amp" },                   \
		{ 0x40d0, "int_out_flag_enbl_ref" },                   \
		{ 0x40e0, "int_out_flag_adc10_ready" },                \
		{ 0x4100, "int_out_flag_bst_bstcur" },                 \
		{ 0x4110, "int_out_flag_bst_hiz" },                    \
		{ 0x4120, "int_out_flag_bst_ocpok" },                  \
		{ 0x4130, "int_out_flag_bst_peakcur" },                \
		{ 0x4140, "int_out_flag_bst_voutcomp" },               \
		{ 0x4150, "int_out_flag_bst_voutcomp86" },             \
		{ 0x4160, "int_out_flag_bst_voutcomp93" },             \
		{ 0x4190, "int_out_flag_ocp_alarm" },                  \
		{ 0x41a0, "int_out_flag_man_wait_src_settings" },      \
		{ 0x41c0, "int_out_flag_man_start_mute_audio" },       \
		{ 0x41f0, "int_out_flag_clk_out_of_range" },           \
		{ 0x4200, "int_out_flag_tdm_error" },                  \
		{ 0x4220, "int_out_flag_clip" },                       \
		{ 0x4240, "int_out_flag_lp_detect_mode0" },            \
		{ 0x4250, "int_out_flag_lp_detect_mode1" },            \
		{ 0x4260, "int_out_flag_low_amplitude" },              \
		{ 0x4270, "int_out_flag_vddp_gt_vbat" },               \
		{ 0x4400, "int_in_flag_por" },                         \
		{ 0x4410, "int_in_flag_pll_lock" },                    \
		{ 0x4420, "int_in_flag_otpok" },                       \
		{ 0x4430, "int_in_flag_ovpok" },                       \
		{ 0x4440, "int_in_flag_uvpok" },                       \
		{ 0x4450, "int_in_flag_clocks_stable" },               \
		{ 0x4460, "int_in_flag_mtp_busy" },                    \
		{ 0x4470, "int_in_flag_lost_clk" },                    \
		{ 0x44a0, "int_in_flag_engage" },                      \
		{ 0x44c0, "int_in_flag_enbl_amp" },                    \
		{ 0x44d0, "int_in_flag_enbl_ref" },                    \
		{ 0x44e0, "int_in_flag_adc10_ready" },                 \
		{ 0x4500, "int_in_flag_bst_bstcur" },                  \
		{ 0x4510, "int_in_flag_bst_hiz" },                     \
		{ 0x4520, "int_in_flag_bst_ocpok" },                   \
		{ 0x4530, "int_in_flag_bst_peakcur" },                 \
		{ 0x4540, "int_in_flag_bst_voutcomp" },                \
		{ 0x4550, "int_in_flag_bst_voutcomp86" },              \
		{ 0x4560, "int_in_flag_bst_voutcomp93" },              \
		{ 0x4590, "int_in_flag_ocp_alarm" },                   \
		{ 0x45a0, "int_in_flag_man_wait_src_settings" },       \
		{ 0x45c0, "int_in_flag_man_start_mute_audio" },        \
		{ 0x45f0, "int_in_flag_clk_out_of_range" },            \
		{ 0x4600, "int_in_flag_tdm_error" },                   \
		{ 0x4620, "int_in_flag_clip" },                        \
		{ 0x4640, "int_in_flag_lp_detect_mode0" },             \
		{ 0x4650, "int_in_flag_lp_detect_mode1" },             \
		{ 0x4660, "int_in_flag_low_amplitude" },               \
		{ 0x4670, "int_in_flag_vddp_gt_vbat" },                \
		{ 0x4800, "int_enable_flag_por" },                     \
		{ 0x4810, "int_enable_flag_pll_lock" },                \
		{ 0x4820, "int_enable_flag_otpok" },                   \
		{ 0x4830, "int_enable_flag_ovpok" },                   \
		{ 0x4840, "int_enable_flag_uvpok" },                   \
		{ 0x4850, "int_enable_flag_clocks_stable" },           \
		{ 0x4860, "int_enable_flag_mtp_busy" },                \
		{ 0x4870, "int_enable_flag_lost_clk" },                \
		{ 0x48a0, "int_enable_flag_engage" },                  \
		{ 0x48c0, "int_enable_flag_enbl_amp" },                \
		{ 0x48d0, "int_enable_flag_enbl_ref" },                \
		{ 0x48e0, "int_enable_flag_adc10_ready" },             \
		{ 0x4900, "int_enable_flag_bst_bstcur" },              \
		{ 0x4910, "int_enable_flag_bst_hiz" },                 \
		{ 0x4920, "int_enable_flag_bst_ocpok" },               \
		{ 0x4930, "int_enable_flag_bst_peakcur" },             \
		{ 0x4940, "int_enable_flag_bst_voutcomp" },            \
		{ 0x4950, "int_enable_flag_bst_voutcomp86" },          \
		{ 0x4960, "int_enable_flag_bst_voutcomp93" },          \
		{ 0x4990, "int_enable_flag_ocp_alarm" },               \
		{ 0x49a0, "int_enable_flag_man_wait_src_settings" },   \
		{ 0x49c0, "int_enable_flag_man_start_mute_audio" },    \
		{ 0x49f0, "int_enable_flag_clk_out_of_range" },        \
		{ 0x4a00, "int_enable_flag_tdm_error" },               \
		{ 0x4a20, "int_enable_flag_clip" },                    \
		{ 0x4a40, "int_enable_flag_lp_detect_mode0" },         \
		{ 0x4a50, "int_enable_flag_lp_detect_mode1" },         \
		{ 0x4a60, "int_enable_flag_low_amplitude" },           \
		{ 0x4a70, "int_enable_flag_vddp_gt_vbat" },            \
		{ 0x4c00, "int_polarity_flag_por" },                   \
		{ 0x4c10, "int_polarity_flag_pll_lock" },              \
		{ 0x4c20, "int_polarity_flag_otpok" },                 \
		{ 0x4c30, "int_polarity_flag_ovpok" },                 \
		{ 0x4c40, "int_polarity_flag_uvpok" },                 \
		{ 0x4c50, "int_polarity_flag_clocks_stable" },         \
		{ 0x4c60, "int_polarity_flag_mtp_busy" },              \
		{ 0x4c70, "int_polarity_flag_lost_clk" },              \
		{ 0x4ca0, "int_polarity_flag_engage" },                \
		{ 0x4cc0, "int_polarity_flag_enbl_amp" },              \
		{ 0x4cd0, "int_polarity_flag_enbl_ref" },              \
		{ 0x4ce0, "int_polarity_flag_adc10_ready" },           \
		{ 0x4d00, "int_polarity_flag_bst_bstcur" },            \
		{ 0x4d10, "int_polarity_flag_bst_hiz" },               \
		{ 0x4d20, "int_polarity_flag_bst_ocpok" },             \
		{ 0x4d30, "int_polarity_flag_bst_peakcur" },           \
		{ 0x4d40, "int_polarity_flag_bst_voutcomp" },          \
		{ 0x4d50, "int_polarity_flag_bst_voutcomp86" },        \
		{ 0x4d60, "int_polarity_flag_bst_voutcomp93" },        \
		{ 0x4d90, "int_polarity_flag_ocp_alarm" },             \
		{ 0x4da0, "int_polarity_flag_man_wait_src_settings" }, \
		{ 0x4dc0, "int_polarity_flag_man_start_mute_audio" },  \
		{ 0x4df0, "int_polarity_flag_clk_out_of_range" },      \
		{ 0x4e00, "int_polarity_flag_tdm_error" },             \
		{ 0x4e20, "int_polarity_flag_clip" },                  \
		{ 0x4e40, "int_polarity_flag_lp_detect_mode0" },       \
		{ 0x4e50, "int_polarity_flag_lp_detect_mode1" },       \
		{ 0x4e60, "int_polarity_flag_low_amplitude" },         \
		{ 0x4e70, "int_polarity_flag_vddp_gt_vbat" },          \
		{ 0x5001, "vbat_prot_attack_time" },                   \
		{ 0x5023, "vbat_prot_thlevel" },                       \
		{ 0x5061, "vbat_prot_max_reduct" },                    \
		{ 0x50d0, "rst_min_vbat" },                            \
		{ 0x50e0, "sel_vbat" },                                \
		{ 0x50f0, "bypass_clipper" },                          \
		{ 0x5100, "batsense_steepness" },                      \
		{ 0x5110, "soft_mute" },                               \
		{ 0x5150, "bypass_hp" },                               \
		{ 0x5170, "enbl_dpsa" },                               \
		{ 0x5222, "ctrl_cc" },                                 \
		{ 0x5257, "gain" },                                    \
		{ 0x52d0, "ctrl_slopectrl" },                          \
		{ 0x52e0, "ctrl_slope" },                              \
		{ 0x5301, "dpsa_level" },                              \
		{ 0x5321, "dpsa_release" },                            \
		{ 0x5340, "clipfast" },                                \
		{ 0x5350, "bypass_lp" },                               \
		{ 0x5400, "first_order_mode" },                        \
		{ 0x5410, "bypass_ctrlloop" },                         \
		{ 0x5420, "fb_hz" },                                   \
		{ 0x5430, "icomp_engage" },                            \
		{ 0x5440, "ctrl_kickback" },                           \
		{ 0x5450, "icomp_engage_overrule" },                   \
		{ 0x5503, "ctrl_dem" },                                \
		{ 0x5543, "ctrl_dem_mismatch" },                       \
		{ 0x5582, "dpsa_drive" },                              \
		{ 0x570a, "enbl_amp" },                                \
		{ 0x57b0, "enbl_engage" },                             \
		{ 0x57c0, "enbl_engage_pst" },                         \
		{ 0x5810, "hard_mute" },                               \
		{ 0x5820, "pwm_shape" },                               \
		{ 0x5844, "pwm_delay" },                               \
		{ 0x5890, "reclock_pwm" },                             \
		{ 0x58a0, "reclock_voltsense" },                       \
		{ 0x58c0, "enbl_pwm_phase_shift" },                    \
		{ 0x6081, "pga_gain_set" },                            \
		{ 0x60b0, "pga_lowpass_enable" },                      \
		{ 0x60c0, "pga_pwr_enable" },                          \
		{ 0x60d0, "pga_switch_enable" },                       \
		{ 0x60e0, "pga_switch_aux_enable" },                   \
		{ 0x6100, "force_idle" },                              \
		{ 0x6110, "bypass_idle" },                             \
		{ 0x6123, "ctrl_attl" },                               \
		{ 0x6163, "ctrl_attr" },                               \
		{ 0x61a0, "idle_cnt" },                                \
		{ 0x61b0, "enbl_idle_ch1" },                           \
		{ 0x6265, "zero_lvl" },                                \
		{ 0x62c1, "ctrl_fb_classd" },                          \
		{ 0x62e1, "lownoisegain_mode" },                       \
		{ 0x6305, "threshold_lvl" },                           \
		{ 0x6365, "hold_time" },                               \
		{ 0x6405, "lpm1_cal_offset" },                         \
		{ 0x6465, "lpm1_zero_lvl" },                           \
		{ 0x64e1, "lpm1_mode" },                               \
		{ 0x6505, "lpm1_threshold_lvl" },                      \
		{ 0x6565, "lpm1_hold_time" },                          \
		{ 0x65c0, "disable_low_power_mode" },                  \
		{ 0x6600, "dcdc_pfm20khz_limit" },                     \
		{ 0x6611, "dcdc_ctrl_maxzercnt" },                     \
		{ 0x6656, "dcdc_vbat_delta_detect" },                  \
		{ 0x66c0, "dcdc_ignore_vbat" },                        \
		{ 0x6700, "enbl_minion" },                             \
		{ 0x6713, "vth_vddpvbat" },                            \
		{ 0x6750, "lpen_vddpvbat" },                           \
		{ 0x6801, "tdm_source_mapping" },                      \
		{ 0x6821, "tdm_sourcea_frame_sel" },                   \
		{ 0x6841, "tdm_sourceb_frame_sel" },                   \
		{ 0x6881, "pdm_anc_sel" },                             \
		{ 0x68a0, "anc_1scomplement" },                        \
		{ 0x6901, "sam_mode" },                                \
		{ 0x6920, "sam_src" },                                 \
		{ 0x6931, "pdmdat_h_sel" },                            \
		{ 0x6951, "pdmdat_l_sel" },                            \
		{ 0x6970, "sam_spkr_sel" },                            \
		{ 0x6a02, "rst_min_vbat_delay" },                      \
		{ 0x6b00, "disable_auto_engage" },                     \
		{ 0x6b10, "sel_tdm_data_valid" },                      \
		{ 0x6c02, "ns_hp2ln_criterion" },                      \
		{ 0x6c32, "ns_ln2hp_criterion" },                      \
		{ 0x6c69, "spare_out" },                               \
		{ 0x6d0f, "spare_in" },                                \
		{ 0x6e00, "flag_lp_detect_mode0" },                    \
		{ 0x6e10, "flag_lp_detect_mode1" },                    \
		{ 0x6e20, "flag_low_amplitude" },                      \
		{ 0x6e30, "flag_vddp_gt_vbat" },                       \
		{ 0x6f02, "cursense_comp_delay" },                     \
		{ 0x6f40, "cursense_comp_sign" },                      \
		{ 0x6f50, "enbl_cursense_comp" },                      \
		{ 0x6f60, "sel_clip_pwms" },                           \
		{ 0x6f72, "pwms_clip_lvl" },                           \
		{ 0x7002, "scnd_boost_voltage" },                      \
		{ 0x7033, "boost_cur" },                               \
		{ 0x7071, "bst_slpcmplvl" },                           \
		{ 0x7090, "boost_intel" },                             \
		{ 0x70a0, "boost_speed" },                             \
		{ 0x70e0, "dcdcoff_mode" },                            \
		{ 0x70f0, "dcdc_pwmonly" },                            \
		{ 0x7104, "bst_drive" },                               \
		{ 0x7151, "bst_scalecur" },                            \
		{ 0x7174, "bst_slopecur" },                            \
		{ 0x71c1, "bst_slope" },                               \
		{ 0x71e0, "bst_bypass_bstcur" },                       \
		{ 0x71f0, "bst_bypass_bstfoldback" },                  \
		{ 0x7200, "enbl_bst_engage" },                         \
		{ 0x7210, "enbl_bst_hizcom" },                         \
		{ 0x7220, "enbl_bst_peak2avg" },                       \
		{ 0x7230, "enbl_bst_peakcur" },                        \
		{ 0x7240, "enbl_bst_power" },                          \
		{ 0x7250, "enbl_bst_slopecur" },                       \
		{ 0x7260, "enbl_bst_voutcomp" },                       \
		{ 0x7270, "enbl_bst_voutcomp86" },                     \
		{ 0x7280, "enbl_bst_voutcomp93" },                     \
		{ 0x7290, "enbl_bst_windac" },                         \
		{ 0x72a5, "bst_windac" },                              \
		{ 0x7300, "boost_alg" },                               \
		{ 0x7311, "boost_loopgain" },                          \
		{ 0x7331, "bst_freq" },                                \
		{ 0x7402, "frst_boost_voltage" },                      \
		{ 0x7430, "boost_track" },                             \
		{ 0x7444, "boost_trip_lvl_1st" },                      \
		{ 0x7494, "boost_hold_time" },                         \
		{ 0x74e0, "sel_dcdc_envelope_8fs" },                   \
		{ 0x74f0, "ignore_flag_voutcomp86" },                  \
		{ 0x7502, "track_decay" },                             \
		{ 0x7534, "boost_trip_lvl_2nd" },                      \
		{ 0x7584, "boost_trip_lvl_track" },                    \
		{ 0x7620, "pga_test_ldo_bypass" },                     \
		{ 0x8001, "sel_clk_cs" },                              \
		{ 0x8021, "micadc_speed" },                            \
		{ 0x8050, "cs_gain_control" },                         \
		{ 0x8060, "cs_bypass_gc" },                            \
		{ 0x8087, "cs_gain" },                                 \
		{ 0x8200, "enbl_cmfb" },                               \
		{ 0x8210, "invertpwm" },                               \
		{ 0x8222, "cmfb_gain" },                               \
		{ 0x8254, "cmfb_offset" },                             \
		{ 0x82a0, "cs_sam_set" },                              \
		{ 0x8305, "cs_ktemp" },                                \
		{ 0x8400, "cs_adc_bsoinv" },                           \
		{ 0x8421, "cs_adc_hifreq" },                           \
		{ 0x8440, "cs_adc_nortz" },                            \
		{ 0x8453, "cs_adc_offset" },                           \
		{ 0x8490, "cs_adc_slowdel" },                          \
		{ 0x84a4, "cs_adc_gain" },                             \
		{ 0x8500, "cs_resonator_enable" },                     \
		{ 0x8510, "cs_classd_tran_skip" },                     \
		{ 0x8530, "cs_inn_short" },                            \
		{ 0x8540, "cs_inp_short" },                            \
		{ 0x8550, "cs_ldo_bypass" },                           \
		{ 0x8560, "cs_ldo_pulldown" },                         \
		{ 0x8574, "cs_ldo_voset" },                            \
		{ 0x8700, "enbl_cs_adc" },                             \
		{ 0x8710, "enbl_cs_inn1" },                            \
		{ 0x8720, "enbl_cs_inn2" },                            \
		{ 0x8730, "enbl_cs_inp1" },                            \
		{ 0x8740, "enbl_cs_inp2" },                            \
		{ 0x8750, "enbl_cs_ldo" },                             \
		{ 0x8760, "enbl_cs_nofloating_n" },                    \
		{ 0x8770, "enbl_cs_nofloating_p" },                    \
		{ 0x8780, "enbl_cs_vbatldo" },                         \
		{ 0x8800, "volsense_pwm_sel" },                        \
		{ 0x8810, "vol_cur_sense_dc_offset" },                 \
		{ 0xa007, "mtpkey1" },                                 \
		{ 0xa107, "mtpkey2" },                                 \
		{ 0xa200, "key01_locked" },                            \
		{ 0xa210, "key02_locked" },                            \
		{ 0xa302, "mtp_man_address_in" },                      \
		{ 0xa330, "man_copy_mtp_to_iic" },                     \
		{ 0xa340, "man_copy_iic_to_mtp" },                     \
		{ 0xa350, "auto_copy_mtp_to_iic" },                    \
		{ 0xa360, "auto_copy_iic_to_mtp" },                    \
		{ 0xa400, "faim_set_clkws" },                          \
		{ 0xa410, "faim_sel_evenrows" },                       \
		{ 0xa420, "faim_sel_oddrows" },                        \
		{ 0xa430, "faim_program_only" },                       \
		{ 0xa440, "faim_erase_only" },                         \
		{ 0xa50f, "mtp_man_data_out_msb" },                    \
		{ 0xa60f, "mtp_man_data_out_lsb" },                    \
		{ 0xa70f, "mtp_man_data_in_msb" },                     \
		{ 0xa80f, "mtp_man_data_in_lsb" },                     \
		{ 0xb010, "bypass_ocpcounter" },                       \
		{ 0xb020, "bypass_glitchfilter" },                     \
		{ 0xb030, "bypass_ovp" },                              \
		{ 0xb040, "bypass_uvp" },                              \
		{ 0xb050, "bypass_otp" },                              \
		{ 0xb060, "bypass_lost_clk" },                         \
		{ 0xb070, "ctrl_vpalarm" },                            \
		{ 0xb087, "ocp_threshold" },                           \
		{ 0xb108, "ext_temp" },                                \
		{ 0xb190, "ext_temp_sel" },                            \
		{ 0xc000, "use_direct_ctrls" },                        \
		{ 0xc010, "rst_datapath" },                            \
		{ 0xc020, "rst_cgu" },                                 \
		{ 0xc038, "enbl_ref" },                                \
		{ 0xc0d0, "enbl_ringo" },                              \
		{ 0xc0e0, "use_direct_clk_ctrl" },                     \
		{ 0xc0f0, "use_direct_pll_ctrl" },                     \
		{ 0xc100, "enbl_tsense" },                             \
		{ 0xc110, "tsense_hibias" },                           \
		{ 0xc120, "enbl_flag_vbg" },                           \
		{ 0xc20f, "abist_offset" },                            \
		{ 0xc300, "bypasslatch" },                             \
		{ 0xc311, "sourcea" },                                 \
		{ 0xc331, "sourceb" },                                 \
		{ 0xc350, "inverta" },                                 \
		{ 0xc360, "invertb" },                                 \
		{ 0xc374, "pulselength" },                             \
		{ 0xc3c0, "tdm_enable_loopback" },                     \
		{ 0xc400, "bst_bypasslatch" },                         \
		{ 0xc411, "bst_source" },                              \
		{ 0xc430, "bst_invertb" },                             \
		{ 0xc444, "bst_pulselength" },                         \
		{ 0xc490, "test_bst_ctrlsthv" },                       \
		{ 0xc4a0, "test_bst_iddq" },                           \
		{ 0xc4b0, "test_bst_rdson" },                          \
		{ 0xc4c0, "test_bst_cvi" },                            \
		{ 0xc4d0, "test_bst_ocp" },                            \
		{ 0xc4e0, "test_bst_sense" },                          \
		{ 0xc500, "test_cvi" },                                \
		{ 0xc510, "test_discrete" },                           \
		{ 0xc520, "test_iddq" },                               \
		{ 0xc540, "test_rdson" },                              \
		{ 0xc550, "test_sdelta" },                             \
		{ 0xc570, "test_enbl_cs" },                            \
		{ 0xc5b0, "pga_test_enable" },                         \
		{ 0xc5c0, "pga_test_offset_enable" },                  \
		{ 0xc5d0, "pga_test_shortinput_enable" },              \
		{ 0xc600, "enbl_pwm_dcc" },                            \
		{ 0xc613, "pwm_dcc_cnt" },                             \
		{ 0xc650, "enbl_ldo_stress" },                         \
		{ 0xc707, "digimuxa_sel" },                            \
		{ 0xc787, "digimuxb_sel" },                            \
		{ 0xc807, "digimuxc_sel" },                            \
		{ 0xc981, "int_ehs" },                                 \
		{ 0xc9c0, "hs_mode" },                                 \
		{ 0xca00, "enbl_anamux1" },                            \
		{ 0xca10, "enbl_anamux2" },                            \
		{ 0xca20, "enbl_anamux3" },                            \
		{ 0xca30, "enbl_anamux4" },                            \
		{ 0xca74, "anamux1" },                                 \
		{ 0xcb04, "anamux2" },                                 \
		{ 0xcb54, "anamux3" },                                 \
		{ 0xcba4, "anamux4" },                                 \
		{ 0xcd05, "pll_seli" },                                \
		{ 0xcd64, "pll_selp" },                                \
		{ 0xcdb3, "pll_selr" },                                \
		{ 0xcdf0, "pll_frm" },                                 \
		{ 0xce09, "pll_ndec" },                                \
		{ 0xcea0, "pll_mdec_msb" },                            \
		{ 0xceb0, "enbl_pll" },                                \
		{ 0xcec0, "enbl_osc" },                                \
		{ 0xced0, "pll_bypass" },                              \
		{ 0xcee0, "pll_directi" },                             \
		{ 0xcef0, "pll_directo" },                             \
		{ 0xcf0f, "pll_mdec_lsb" },                            \
		{ 0xd006, "pll_pdec" },                                \
		{ 0xd10f, "tsig_freq_lsb" },                           \
		{ 0xd202, "tsig_freq_msb" },                           \
		{ 0xd230, "inject_tsig" },                             \
		{ 0xd283, "tsig_gain" },                               \
		{ 0xd300, "adc10_reset" },                             \
		{ 0xd311, "adc10_test" },                              \
		{ 0xd332, "adc10_sel" },                               \
		{ 0xd364, "adc10_prog_sample" },                       \
		{ 0xd3b0, "adc10_enbl" },                              \
		{ 0xd3c0, "bypass_lp_vbat" },                          \
		{ 0xd409, "data_adc10_tempbat" },                      \
		{ 0xd507, "ctrl_digtoana_hidden" },                    \
		{ 0xd580, "enbl_clk_out_of_range" },                   \
		{ 0xd621, "clkdiv_audio_sel" },                        \
		{ 0xd641, "clkdiv_muxa_sel" },                         \
		{ 0xd661, "clkdiv_muxb_sel" },                         \
		{ 0xd701, "pdmdat_ehs" },                              \
		{ 0xd721, "datao_ehs" },                               \
		{ 0xd740, "bck_ehs" },                                 \
		{ 0xd750, "datai_ehs" },                               \
		{ 0xd760, "pdmclk_ehs" },                              \
		{ 0xd800, "source_in_testmode" },                      \
		{ 0xd810, "gainatt_feedback" },                        \
		{ 0xd822, "test_parametric_io" },                      \
		{ 0xd850, "ctrl_bst_clk_lp1" },                        \
		{ 0xd861, "test_spare_out1" },                         \
		{ 0xd880, "bst_dcmbst" },                              \
		{ 0xd890, "pdm_loopback" },                            \
		{ 0xd8a1, "force_pga_clock" },                         \
		{ 0xd8c3, "test_spare_out2" },                         \
		{ 0xee0f, "sw_profile" },                              \
		{ 0xef0f, "sw_vstep" },                                \
		{ 0xf000, "calibration_onetime" },                     \
		{ 0xf010, "calibr_ron_done" },                         \
		{ 0xf020, "calibr_dcdc_api_calibrate" },               \
		{ 0xf030, "calibr_dcdc_delta_sign" },                  \
		{ 0xf042, "calibr_dcdc_delta" },                       \
		{ 0xf078, "calibr_speaker_info" },                     \
		{ 0xf105, "calibr_vout_offset" },                      \
		{ 0xf163, "spare_mpt1_9_6" },                          \
		{ 0xf1a5, "spare_mpt1_15_10" },                        \
		{ 0xf203, "calibr_gain" },                             \
		{ 0xf245, "calibr_offset" },                           \
		{ 0xf2a3, "spare_mpt2_13_10" },                        \
		{ 0xf307, "spare_mpt3_7_0" },                          \
		{ 0xf387, "calibr_gain_cs" },                          \
		{ 0xf40f, "spare_mtp4_15_0" },                         \
		{ 0xf50f, "calibr_R25C_R" },                           \
		{ 0xf606, "spare_mpt6_6_0" },                          \
		{ 0xf686, "spare_mpt6_14_8" },                         \
		{ 0xf706, "ctrl_offset_a" },                           \
		{ 0xf786, "ctrl_offset_b" },                           \
		{ 0xf806, "htol_iic_addr" },                           \
		{ 0xf870, "htol_iic_addr_en" },                        \
		{ 0xf884, "calibr_temp_offset" },                      \
		{ 0xf8d2, "calibr_temp_gain" },                        \
		{ 0xf900, "mtp_lock_dcdcoff_mode" },                   \
		{ 0xf910, "disable_sam_mode" },                        \
		{ 0xf920, "mtp_lock_bypass_clipper" },                 \
		{ 0xf930, "mtp_lock_max_dcdc_voltage" },               \
		{ 0xf943, "calibr_vbg_trim" },                         \
		{ 0xf980, "mtp_enbl_amp_in_state_alarm" },             \
		{ 0xf990, "mtp_enbl_pwm_delay_clock_gating" },         \
		{ 0xf9a0, "mtp_enbl_ocp_clock_gating" },               \
		{ 0xf9b0, "mtp_gate_cgu_clock_for_test" },             \
		{ 0xf9c3, "spare_mtp9_15_12" },                        \
		{ 0xfa0f, "mtpdataA" },                                \
		{ 0xfb0f, "mtpdataB" },                                \
		{ 0xfc0f, "mtpdataC" },                                \
		{ 0xfd0f, "mtpdataD" },                                \
		{ 0xfe0f, "mtpdataE" },                                \
		{ 0xff07, "calibr_osc_delta_ndiv" },                   \
		{ 0xffff, "Unknown bitfield enum" }                    \
	}
enum tfa9872_irq {
	tfa9872_irq_stvdds = 0,
	tfa9872_irq_stplls = 1,
	tfa9872_irq_stotds = 2,
	tfa9872_irq_stovds = 3,
	tfa9872_irq_stuvds = 4,
	tfa9872_irq_stclks = 5,
	tfa9872_irq_stmtpb = 6,
	tfa9872_irq_stnoclk = 7,
	tfa9872_irq_stsws = 10,
	tfa9872_irq_stamps = 12,
	tfa9872_irq_starefs = 13,
	tfa9872_irq_stadccr = 14,
	tfa9872_irq_stbstcu = 16,
	tfa9872_irq_stbsthi = 17,
	tfa9872_irq_stbstoc = 18,
	tfa9872_irq_stbstpkcur = 19,
	tfa9872_irq_stbstvc = 20,
	tfa9872_irq_stbst86 = 21,
	tfa9872_irq_stbst93 = 22,
	tfa9872_irq_stocpr = 25,
	tfa9872_irq_stmwsrc = 26,
	tfa9872_irq_stmwsmu = 28,
	tfa9872_irq_stclkoor = 31,
	tfa9872_irq_sttdmer = 32,
	tfa9872_irq_stclpr = 34,
	tfa9872_irq_stlp0 = 36,
	tfa9872_irq_stlp1 = 37,
	tfa9872_irq_stla = 38,
	tfa9872_irq_stvddph = 39,
	tfa9872_irq_max = 40,
	tfa9872_irq_all = -1
};
#define TFA9872_IRQ_NAMETABLE                                               \
	static struct TfaIrqName Tfa9872IrqNames[] = {                      \
		{ 0, "STVDDS" },   { 1, "STPLLS" },      { 2, "STOTDS" },   \
		{ 3, "STOVDS" },   { 4, "STUVDS" },      { 5, "STCLKS" },   \
		{ 6, "STMTPB" },   { 7, "STNOCLK" },     { 8, "8" },        \
		{ 9, "9" },	{ 10, "STSWS" },      { 11, "11" },      \
		{ 12, "STAMPS" },  { 13, "STAREFS" },    { 14, "STADCCR" }, \
		{ 15, "15" },      { 16, "STBSTCU" },    { 17, "STBSTHI" }, \
		{ 18, "STBSTOC" }, { 19, "STBSTPKCUR" }, { 20, "STBSTVC" }, \
		{ 21, "STBST86" }, { 22, "STBST93" },    { 23, "23" },      \
		{ 24, "24" },      { 25, "STOCPR" },     { 26, "STMWSRC" }, \
		{ 27, "27" },      { 28, "STMWSMU" },    { 29, "29" },      \
		{ 30, "30" },      { 31, "STCLKOOR" },   { 32, "STTDMER" }, \
		{ 33, "33" },      { 34, "STCLPR" },     { 35, "35" },      \
		{ 36, "STLP0" },   { 37, "STLP1" },      { 38, "STLA" },    \
		{ 39, "STVDDPH" }, { 40, "40" },                            \
	}
#endif
