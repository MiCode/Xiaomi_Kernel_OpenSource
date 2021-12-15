/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _TFA9894_TFAFIELDNAMES_N2_H
#define _TFA9894_TFAFIELDNAMES_N2_H
#define TFA9894N2_I2CVERSION 25.0
enum nxpTfa9894N2BfEnumList {
	TFA9894N2_BF_PWDN = 0x0000,
	TFA9894N2_BF_I2CR = 0x0010,
	TFA9894N2_BF_CFE = 0x0020,
	TFA9894N2_BF_AMPE = 0x0030,
	TFA9894N2_BF_DCA = 0x0040,
	TFA9894N2_BF_SBSL = 0x0050,
	TFA9894N2_BF_AMPC = 0x0060,
	TFA9894N2_BF_INTP = 0x0071,
	TFA9894N2_BF_FSSSEL = 0x0090,
	TFA9894N2_BF_BYPOCP = 0x00a0,
	TFA9894N2_BF_TSTOCP = 0x00b0,
	TFA9894N2_BF_BSSS = 0x00c0,
	TFA9894N2_BF_HPFBYP = 0x00d0,
	TFA9894N2_BF_DPSA = 0x00e0,
	TFA9894N2_BF_AMPINSEL = 0x0101,
	TFA9894N2_BF_MANSCONF = 0x0120,
	TFA9894N2_BF_MANCOLD = 0x0130,
	TFA9894N2_BF_MANROBOD = 0x0140,
	TFA9894N2_BF_BODE = 0x0150,
	TFA9894N2_BF_BODHYS = 0x0160,
	TFA9894N2_BF_BODFILT = 0x0171,
	TFA9894N2_BF_BODTHLVL = 0x0191,
	TFA9894N2_BF_MUTETO = 0x01b0,
	TFA9894N2_BF_MANWDE = 0x01c0,
	TFA9894N2_BF_OPENMTP = 0x01e0,
	TFA9894N2_BF_FAIMVBGOVRRL = 0x01f0,
	TFA9894N2_BF_AUDFS = 0x0203,
	TFA9894N2_BF_INPLEV = 0x0240,
	TFA9894N2_BF_FRACTDEL = 0x0255,
	TFA9894N2_BF_TDMPRES = 0x02b1,
	TFA9894N2_BF_AMPOCRT = 0x02d2,
	TFA9894N2_BF_REV = 0x030f,
	TFA9894N2_BF_REFCKEXT = 0x0401,
	TFA9894N2_BF_REFCKSEL = 0x0420,
	TFA9894N2_BF_MCLKSEL = 0x0432,
	TFA9894N2_BF_MANAOOSC = 0x0460,
	TFA9894N2_BF_ACKCLDDIS = 0x0470,
	TFA9894N2_BF_FSSYNCEN = 0x0480,
	TFA9894N2_BF_CLKREFSYNCEN = 0x0490,
	TFA9894N2_BF_PLLSTUP = 0x04a0,
	TFA9894N2_BF_CGUSYNCDCG = 0x0500,
	TFA9894N2_BF_SPKSSEN = 0x0510,
	TFA9894N2_BF_MTPSSEN = 0x0520,
	TFA9894N2_BF_WDTCLKEN = 0x0530,
	TFA9894N2_BF_VDDS = 0x1000,
	TFA9894N2_BF_PLLS = 0x1010,
	TFA9894N2_BF_OTDS = 0x1020,
	TFA9894N2_BF_OVDS = 0x1030,
	TFA9894N2_BF_UVDS = 0x1040,
	TFA9894N2_BF_OCDS = 0x1050,
	TFA9894N2_BF_CLKS = 0x1060,
	TFA9894N2_BF_MTPB = 0x1070,
	TFA9894N2_BF_NOCLK = 0x1080,
	TFA9894N2_BF_ACS = 0x1090,
	TFA9894N2_BF_WDS = 0x10a0,
	TFA9894N2_BF_SWS = 0x10b0,
	TFA9894N2_BF_AMPS = 0x10c0,
	TFA9894N2_BF_AREFS = 0x10d0,
	TFA9894N2_BF_ADCCR = 0x10e0,
	TFA9894N2_BF_BODNOK = 0x10f0,
	TFA9894N2_BF_DCIL = 0x1100,
	TFA9894N2_BF_DCDCA = 0x1110,
	TFA9894N2_BF_DCOCPOK = 0x1120,
	TFA9894N2_BF_DCHVBAT = 0x1140,
	TFA9894N2_BF_DCH114 = 0x1150,
	TFA9894N2_BF_DCH107 = 0x1160,
	TFA9894N2_BF_SPKS = 0x1170,
	TFA9894N2_BF_CLKOOR = 0x1180,
	TFA9894N2_BF_MANALARM = 0x1190,
	TFA9894N2_BF_TDMERR = 0x11a0,
	TFA9894N2_BF_TDMLUTER = 0x11b0,
	TFA9894N2_BF_NOAUDCLK = 0x11c0,
	TFA9894N2_BF_OCPOAP = 0x1200,
	TFA9894N2_BF_OCPOAN = 0x1210,
	TFA9894N2_BF_OCPOBP = 0x1220,
	TFA9894N2_BF_OCPOBN = 0x1230,
	TFA9894N2_BF_CLIPS = 0x1240,
	TFA9894N2_BF_MANMUTE = 0x1250,
	TFA9894N2_BF_MANOPER = 0x1260,
	TFA9894N2_BF_LP1 = 0x1270,
	TFA9894N2_BF_LA = 0x1280,
	TFA9894N2_BF_VDDPH = 0x1290,
	TFA9894N2_BF_TDMSTAT = 0x1302,
	TFA9894N2_BF_MANSTATE = 0x1333,
	TFA9894N2_BF_DCMODE = 0x13b1,
	TFA9894N2_BF_BATS = 0x1509,
	TFA9894N2_BF_TEMPS = 0x1608,
	TFA9894N2_BF_VDDPS = 0x1709,
	TFA9894N2_BF_TDME = 0x2000,
	TFA9894N2_BF_TDMSPKE = 0x2010,
	TFA9894N2_BF_TDMDCE = 0x2020,
	TFA9894N2_BF_TDMCSE = 0x2030,
	TFA9894N2_BF_TDMVSE = 0x2040,
	TFA9894N2_BF_TDMCFE = 0x2050,
	TFA9894N2_BF_TDMCF2E = 0x2060,
	TFA9894N2_BF_TDMCLINV = 0x2070,
	TFA9894N2_BF_TDMFSPOL = 0x2080,
	TFA9894N2_BF_TDMDEL = 0x2090,
	TFA9894N2_BF_TDMADJ = 0x20a0,
	TFA9894N2_BF_TDMOOMP = 0x20b1,
	TFA9894N2_BF_TDMNBCK = 0x2103,
	TFA9894N2_BF_TDMFSLN = 0x2143,
	TFA9894N2_BF_TDMSLOTS = 0x2183,
	TFA9894N2_BF_TDMTXDFO = 0x21c1,
	TFA9894N2_BF_TDMTXUS0 = 0x21e1,
	TFA9894N2_BF_TDMSLLN = 0x2204,
	TFA9894N2_BF_TDMBRMG = 0x2254,
	TFA9894N2_BF_TDMSSIZE = 0x22a4,
	TFA9894N2_BF_TDMSPKS = 0x2303,
	TFA9894N2_BF_TDMDCS = 0x2343,
	TFA9894N2_BF_TDMCFSEL = 0x2381,
	TFA9894N2_BF_TDMCF2SEL = 0x23a1,
	TFA9894N2_BF_TDMCSS = 0x2403,
	TFA9894N2_BF_TDMVSS = 0x2443,
	TFA9894N2_BF_TDMCFS = 0x2483,
	TFA9894N2_BF_TDMCF2S = 0x24c3,
	TFA9894N2_BF_ISTVDDS = 0x4000,
	TFA9894N2_BF_ISTBSTOC = 0x4010,
	TFA9894N2_BF_ISTOTDS = 0x4020,
	TFA9894N2_BF_ISTOCPR = 0x4030,
	TFA9894N2_BF_ISTUVDS = 0x4040,
	TFA9894N2_BF_ISTMANALARM = 0x4050,
	TFA9894N2_BF_ISTTDMER = 0x4060,
	TFA9894N2_BF_ISTNOCLK = 0x4070,
	TFA9894N2_BF_ISTCFMER = 0x4080,
	TFA9894N2_BF_ISTCFMAC = 0x4090,
	TFA9894N2_BF_ISTSPKS = 0x40a0,
	TFA9894N2_BF_ISTACS = 0x40b0,
	TFA9894N2_BF_ISTWDS = 0x40c0,
	TFA9894N2_BF_ISTBODNOK = 0x40d0,
	TFA9894N2_BF_ISTLP1 = 0x40e0,
	TFA9894N2_BF_ISTCLKOOR = 0x40f0,
	TFA9894N2_BF_ICLVDDS = 0x4400,
	TFA9894N2_BF_ICLBSTOC = 0x4410,
	TFA9894N2_BF_ICLOTDS = 0x4420,
	TFA9894N2_BF_ICLOCPR = 0x4430,
	TFA9894N2_BF_ICLUVDS = 0x4440,
	TFA9894N2_BF_ICLMANALARM = 0x4450,
	TFA9894N2_BF_ICLTDMER = 0x4460,
	TFA9894N2_BF_ICLNOCLK = 0x4470,
	TFA9894N2_BF_ICLCFMER = 0x4480,
	TFA9894N2_BF_ICLCFMAC = 0x4490,
	TFA9894N2_BF_ICLSPKS = 0x44a0,
	TFA9894N2_BF_ICLACS = 0x44b0,
	TFA9894N2_BF_ICLWDS = 0x44c0,
	TFA9894N2_BF_ICLBODNOK = 0x44d0,
	TFA9894N2_BF_ICLLP1 = 0x44e0,
	TFA9894N2_BF_ICLCLKOOR = 0x44f0,
	TFA9894N2_BF_IEVDDS = 0x4800,
	TFA9894N2_BF_IEBSTOC = 0x4810,
	TFA9894N2_BF_IEOTDS = 0x4820,
	TFA9894N2_BF_IEOCPR = 0x4830,
	TFA9894N2_BF_IEUVDS = 0x4840,
	TFA9894N2_BF_IEMANALARM = 0x4850,
	TFA9894N2_BF_IETDMER = 0x4860,
	TFA9894N2_BF_IENOCLK = 0x4870,
	TFA9894N2_BF_IECFMER = 0x4880,
	TFA9894N2_BF_IECFMAC = 0x4890,
	TFA9894N2_BF_IESPKS = 0x48a0,
	TFA9894N2_BF_IEACS = 0x48b0,
	TFA9894N2_BF_IEWDS = 0x48c0,
	TFA9894N2_BF_IEBODNOK = 0x48d0,
	TFA9894N2_BF_IELP1 = 0x48e0,
	TFA9894N2_BF_IECLKOOR = 0x48f0,
	TFA9894N2_BF_IPOVDDS = 0x4c00,
	TFA9894N2_BF_IPOBSTOC = 0x4c10,
	TFA9894N2_BF_IPOOTDS = 0x4c20,
	TFA9894N2_BF_IPOOCPR = 0x4c30,
	TFA9894N2_BF_IPOUVDS = 0x4c40,
	TFA9894N2_BF_IPOMANALARM = 0x4c50,
	TFA9894N2_BF_IPOTDMER = 0x4c60,
	TFA9894N2_BF_IPONOCLK = 0x4c70,
	TFA9894N2_BF_IPOCFMER = 0x4c80,
	TFA9894N2_BF_IPOCFMAC = 0x4c90,
	TFA9894N2_BF_IPOSPKS = 0x4ca0,
	TFA9894N2_BF_IPOACS = 0x4cb0,
	TFA9894N2_BF_IPOWDS = 0x4cc0,
	TFA9894N2_BF_IPOBODNOK = 0x4cd0,
	TFA9894N2_BF_IPOLP1 = 0x4ce0,
	TFA9894N2_BF_IPOCLKOOR = 0x4cf0,
	TFA9894N2_BF_BSSCR = 0x5001,
	TFA9894N2_BF_BSST = 0x5023,
	TFA9894N2_BF_BSSRL = 0x5061,
	TFA9894N2_BF_BSSRR = 0x5082,
	TFA9894N2_BF_BSSHY = 0x50b1,
	TFA9894N2_BF_BSSR = 0x50e0,
	TFA9894N2_BF_BSSBY = 0x50f0,
	TFA9894N2_BF_CFSM = 0x5130,
	TFA9894N2_BF_VOL = 0x5187,
	TFA9894N2_BF_CLIPCTRL = 0x5202,
	TFA9894N2_BF_SLOPEE = 0x5230,
	TFA9894N2_BF_SLOPESET = 0x5240,
	TFA9894N2_BF_BYPDLYLINE = 0x5250,
	TFA9894N2_BF_AMPGAIN = 0x5287,
	TFA9894N2_BF_TDMDCG = 0x5703,
	TFA9894N2_BF_TDMSPKG = 0x5743,
	TFA9894N2_BF_DCINSEL = 0x5781,
	TFA9894N2_BF_LNMODE = 0x5881,
	TFA9894N2_BF_LPM1MODE = 0x5ac1,
	TFA9894N2_BF_TDMSRCMAP = 0x5d02,
	TFA9894N2_BF_TDMSRCAS = 0x5d31,
	TFA9894N2_BF_TDMSRCBS = 0x5d51,
	TFA9894N2_BF_TDMSRCACLIP = 0x5d71,
	TFA9894N2_BF_TDMSRCBCLIP = 0x5d91,
	TFA9894N2_BF_DELCURCOMP = 0x6102,
	TFA9894N2_BF_SIGCURCOMP = 0x6130,
	TFA9894N2_BF_ENCURCOMP = 0x6140,
	TFA9894N2_BF_LVLCLPPWM = 0x6152,
	TFA9894N2_BF_DCVOF = 0x7005,
	TFA9894N2_BF_DCVOS = 0x7065,
	TFA9894N2_BF_DCMCC = 0x70c3,
	TFA9894N2_BF_DCCV = 0x7101,
	TFA9894N2_BF_DCIE = 0x7120,
	TFA9894N2_BF_DCSR = 0x7130,
	TFA9894N2_BF_DCDIS = 0x7140,
	TFA9894N2_BF_DCPWM = 0x7150,
	TFA9894N2_BF_DCTRACK = 0x7160,
	TFA9894N2_BF_DCENVSEL = 0x7170,
	TFA9894N2_BF_OVSCTLVL = 0x7195,
	TFA9894N2_BF_DCTRIP = 0x7204,
	TFA9894N2_BF_DCTRIP2 = 0x7254,
	TFA9894N2_BF_DCTRIPT = 0x72a4,
	TFA9894N2_BF_DCTRIPHYSTE = 0x72f0,
	TFA9894N2_BF_DCHOLD = 0x7304,
	TFA9894N2_BF_RST = 0x9000,
	TFA9894N2_BF_DMEM = 0x9011,
	TFA9894N2_BF_AIF = 0x9030,
	TFA9894N2_BF_CFINT = 0x9040,
	TFA9894N2_BF_CFCGATE = 0x9050,
	TFA9894N2_BF_REQCMD = 0x9080,
	TFA9894N2_BF_REQRST = 0x9090,
	TFA9894N2_BF_REQMIPS = 0x90a0,
	TFA9894N2_BF_REQMUTED = 0x90b0,
	TFA9894N2_BF_REQVOL = 0x90c0,
	TFA9894N2_BF_REQDMG = 0x90d0,
	TFA9894N2_BF_REQCAL = 0x90e0,
	TFA9894N2_BF_REQRSV = 0x90f0,
	TFA9894N2_BF_MADD = 0x910f,
	TFA9894N2_BF_MEMA = 0x920f,
	TFA9894N2_BF_ERR = 0x9307,
	TFA9894N2_BF_ACKCMD = 0x9380,
	TFA9894N2_BF_ACKRST = 0x9390,
	TFA9894N2_BF_ACKMIPS = 0x93a0,
	TFA9894N2_BF_ACKMUTED = 0x93b0,
	TFA9894N2_BF_ACKVOL = 0x93c0,
	TFA9894N2_BF_ACKDMG = 0x93d0,
	TFA9894N2_BF_ACKCAL = 0x93e0,
	TFA9894N2_BF_ACKRSV = 0x93f0,
	TFA9894N2_BF_MTPK = 0xa107,
	TFA9894N2_BF_KEY1LOCKED = 0xa200,
	TFA9894N2_BF_KEY2LOCKED = 0xa210,
	TFA9894N2_BF_CMTPI = 0xa350,
	TFA9894N2_BF_CIMTP = 0xa360,
	TFA9894N2_BF_MTPRDMSB = 0xa50f,
	TFA9894N2_BF_MTPRDLSB = 0xa60f,
	TFA9894N2_BF_EXTTS = 0xb108,
	TFA9894N2_BF_TROS = 0xb190,
	TFA9894N2_BF_PLLINSELI = 0xca05,
	TFA9894N2_BF_PLLINSELP = 0xca64,
	TFA9894N2_BF_PLLINSELR = 0xcab3,
	TFA9894N2_BF_PLLNDEC = 0xcb09,
	TFA9894N2_BF_PLLMDECMSB = 0xcba0,
	TFA9894N2_BF_PLLBYPASS = 0xcbb0,
	TFA9894N2_BF_PLLDIRECTI = 0xcbc0,
	TFA9894N2_BF_PLLDIRECTO = 0xcbd0,
	TFA9894N2_BF_PLLFRMSTBL = 0xcbe0,
	TFA9894N2_BF_PLLFRM = 0xcbf0,
	TFA9894N2_BF_PLLMDECLSB = 0xcc0f,
	TFA9894N2_BF_PLLPDEC = 0xcd06,
	TFA9894N2_BF_DIRECTPLL = 0xcd70,
	TFA9894N2_BF_DIRECTCLK = 0xcd80,
	TFA9894N2_BF_PLLLIM = 0xcd90,
	TFA9894N2_BF_SWPROFIL = 0xe00f,
	TFA9894N2_BF_SWVSTEP = 0xe10f,
	TFA9894N2_BF_MTPOTC = 0xf000,
	TFA9894N2_BF_MTPEX = 0xf010,
	TFA9894N2_BF_DCMCCAPI = 0xf020,
	TFA9894N2_BF_DCMCCSB = 0xf030,
	TFA9894N2_BF_USERDEF = 0xf042,
	TFA9894N2_BF_CUSTINFO = 0xf078,
	TFA9894N2_BF_R25C = 0xf50f,
};
#define TFA9894N2_NAMETABLE                                                    \
	static struct TfaBfName Tfa9894N2DatasheetNames[] = {                  \
		{ 0x0, "PWDN" },	   { 0x10, "I2CR" },                   \
		{ 0x20, "CFE" },	   { 0x30, "AMPE" },                   \
		{ 0x40, "DCA" },	   { 0x50, "SBSL" },                   \
		{ 0x60, "AMPC" },	  { 0x71, "INTP" },                   \
		{ 0x90, "FSSSEL" },	{ 0xa0, "BYPOCP" },                 \
		{ 0xb0, "TSTOCP" },	{ 0xc0, "BSSS" },                   \
		{ 0xd0, "HPFBYP" },	{ 0xe0, "DPSA" },                   \
		{ 0x101, "AMPINSEL" },     { 0x120, "MANSCONF" },              \
		{ 0x130, "MANCOLD" },      { 0x140, "MANROBOD" },              \
		{ 0x150, "BODE" },	 { 0x160, "BODHYS" },                \
		{ 0x171, "BODFILT" },      { 0x191, "BODTHLVL" },              \
		{ 0x1b0, "MUTETO" },       { 0x1c0, "MANWDE" },                \
		{ 0x1e0, "OPENMTP" },      { 0x1f0, "FAIMVBGOVRRL" },          \
		{ 0x203, "AUDFS" },	{ 0x240, "INPLEV" },                \
		{ 0x255, "FRACTDEL" },     { 0x2b1, "TDMPRES" },               \
		{ 0x2d2, "AMPOCRT" },      { 0x30f, "REV" },                   \
		{ 0x401, "REFCKEXT" },     { 0x420, "REFCKSEL" },              \
		{ 0x432, "MCLKSEL" },      { 0x460, "MANAOOSC" },              \
		{ 0x470, "ACKCLDDIS" },    { 0x480, "FSSYNCEN" },              \
		{ 0x490, "CLKREFSYNCEN" }, { 0x4a0, "PLLSTUP" },               \
		{ 0x500, "CGUSYNCDCG" },   { 0x510, "SPKSSEN" },               \
		{ 0x520, "MTPSSEN" },      { 0x530, "WDTCLKEN" },              \
		{ 0x1000, "VDDS" },	{ 0x1010, "PLLS" },                 \
		{ 0x1020, "OTDS" },	{ 0x1030, "OVDS" },                 \
		{ 0x1040, "UVDS" },	{ 0x1050, "OCDS" },                 \
		{ 0x1060, "CLKS" },	{ 0x1070, "MTPB" },                 \
		{ 0x1080, "NOCLK" },       { 0x1090, "ACS" },                  \
		{ 0x10a0, "WDS" },	 { 0x10b0, "SWS" },                  \
		{ 0x10c0, "AMPS" },	{ 0x10d0, "AREFS" },                \
		{ 0x10e0, "ADCCR" },       { 0x10f0, "BODNOK" },               \
		{ 0x1100, "DCIL" },	{ 0x1110, "DCDCA" },                \
		{ 0x1120, "DCOCPOK" },     { 0x1140, "DCHVBAT" },              \
		{ 0x1150, "DCH114" },      { 0x1160, "DCH107" },               \
		{ 0x1170, "SPKS" },	{ 0x1180, "CLKOOR" },               \
		{ 0x1190, "MANALARM" },    { 0x11a0, "TDMERR" },               \
		{ 0x11b0, "TDMLUTER" },    { 0x11c0, "NOAUDCLK" },             \
		{ 0x1200, "OCPOAP" },      { 0x1210, "OCPOAN" },               \
		{ 0x1220, "OCPOBP" },      { 0x1230, "OCPOBN" },               \
		{ 0x1240, "CLIPS" },       { 0x1250, "MANMUTE" },              \
		{ 0x1260, "MANOPER" },     { 0x1270, "LP1" },                  \
		{ 0x1280, "LA" },	  { 0x1290, "VDDPH" },                \
		{ 0x1302, "TDMSTAT" },     { 0x1333, "MANSTATE" },             \
		{ 0x13b1, "DCMODE" },      { 0x1509, "BATS" },                 \
		{ 0x1608, "TEMPS" },       { 0x1709, "VDDPS" },                \
		{ 0x2000, "TDME" },	{ 0x2010, "TDMSPKE" },              \
		{ 0x2020, "TDMDCE" },      { 0x2030, "TDMCSE" },               \
		{ 0x2040, "TDMVSE" },      { 0x2050, "TDMCFE" },               \
		{ 0x2060, "TDMCF2E" },     { 0x2070, "TDMCLINV" },             \
		{ 0x2080, "TDMFSPOL" },    { 0x2090, "TDMDEL" },               \
		{ 0x20a0, "TDMADJ" },      { 0x20b1, "TDMOOMP" },              \
		{ 0x2103, "TDMNBCK" },     { 0x2143, "TDMFSLN" },              \
		{ 0x2183, "TDMSLOTS" },    { 0x21c1, "TDMTXDFO" },             \
		{ 0x21e1, "TDMTXUS0" },    { 0x2204, "TDMSLLN" },              \
		{ 0x2254, "TDMBRMG" },     { 0x22a4, "TDMSSIZE" },             \
		{ 0x2303, "TDMSPKS" },     { 0x2343, "TDMDCS" },               \
		{ 0x2381, "TDMCFSEL" },    { 0x23a1, "TDMCF2SEL" },            \
		{ 0x2403, "TDMCSS" },      { 0x2443, "TDMVSS" },               \
		{ 0x2483, "TDMCFS" },      { 0x24c3, "TDMCF2S" },              \
		{ 0x4000, "ISTVDDS" },     { 0x4010, "ISTBSTOC" },             \
		{ 0x4020, "ISTOTDS" },     { 0x4030, "ISTOCPR" },              \
		{ 0x4040, "ISTUVDS" },     { 0x4050, "ISTMANALARM" },          \
		{ 0x4060, "ISTTDMER" },    { 0x4070, "ISTNOCLK" },             \
		{ 0x4080, "ISTCFMER" },    { 0x4090, "ISTCFMAC" },             \
		{ 0x40a0, "ISTSPKS" },     { 0x40b0, "ISTACS" },               \
		{ 0x40c0, "ISTWDS" },      { 0x40d0, "ISTBODNOK" },            \
		{ 0x40e0, "ISTLP1" },      { 0x40f0, "ISTCLKOOR" },            \
		{ 0x4400, "ICLVDDS" },     { 0x4410, "ICLBSTOC" },             \
		{ 0x4420, "ICLOTDS" },     { 0x4430, "ICLOCPR" },              \
		{ 0x4440, "ICLUVDS" },     { 0x4450, "ICLMANALARM" },          \
		{ 0x4460, "ICLTDMER" },    { 0x4470, "ICLNOCLK" },             \
		{ 0x4480, "ICLCFMER" },    { 0x4490, "ICLCFMAC" },             \
		{ 0x44a0, "ICLSPKS" },     { 0x44b0, "ICLACS" },               \
		{ 0x44c0, "ICLWDS" },      { 0x44d0, "ICLBODNOK" },            \
		{ 0x44e0, "ICLLP1" },      { 0x44f0, "ICLCLKOOR" },            \
		{ 0x4800, "IEVDDS" },      { 0x4810, "IEBSTOC" },              \
		{ 0x4820, "IEOTDS" },      { 0x4830, "IEOCPR" },               \
		{ 0x4840, "IEUVDS" },      { 0x4850, "IEMANALARM" },           \
		{ 0x4860, "IETDMER" },     { 0x4870, "IENOCLK" },              \
		{ 0x4880, "IECFMER" },     { 0x4890, "IECFMAC" },              \
		{ 0x48a0, "IESPKS" },      { 0x48b0, "IEACS" },                \
		{ 0x48c0, "IEWDS" },       { 0x48d0, "IEBODNOK" },             \
		{ 0x48e0, "IELP1" },       { 0x48f0, "IECLKOOR" },             \
		{ 0x4c00, "IPOVDDS" },     { 0x4c10, "IPOBSTOC" },             \
		{ 0x4c20, "IPOOTDS" },     { 0x4c30, "IPOOCPR" },              \
		{ 0x4c40, "IPOUVDS" },     { 0x4c50, "IPOMANALARM" },          \
		{ 0x4c60, "IPOTDMER" },    { 0x4c70, "IPONOCLK" },             \
		{ 0x4c80, "IPOCFMER" },    { 0x4c90, "IPOCFMAC" },             \
		{ 0x4ca0, "IPOSPKS" },     { 0x4cb0, "IPOACS" },               \
		{ 0x4cc0, "IPOWDS" },      { 0x4cd0, "IPOBODNOK" },            \
		{ 0x4ce0, "IPOLP1" },      { 0x4cf0, "IPOCLKOOR" },            \
		{ 0x5001, "BSSCR" },       { 0x5023, "BSST" },                 \
		{ 0x5061, "BSSRL" },       { 0x5082, "BSSRR" },                \
		{ 0x50b1, "BSSHY" },       { 0x50e0, "BSSR" },                 \
		{ 0x50f0, "BSSBY" },       { 0x5130, "CFSM" },                 \
		{ 0x5187, "VOL" },	 { 0x5202, "CLIPCTRL" },             \
		{ 0x5230, "SLOPEE" },      { 0x5240, "SLOPESET" },             \
		{ 0x5250, "BYPDLYLINE" },  { 0x5287, "AMPGAIN" },              \
		{ 0x5703, "TDMDCG" },      { 0x5743, "TDMSPKG" },              \
		{ 0x5781, "DCINSEL" },     { 0x5881, "LNMODE" },               \
		{ 0x5ac1, "LPM1MODE" },    { 0x5d02, "TDMSRCMAP" },            \
		{ 0x5d31, "TDMSRCAS" },    { 0x5d51, "TDMSRCBS" },             \
		{ 0x5d71, "TDMSRCACLIP" }, { 0x5d91, "TDMSRCBCLIP" },          \
		{ 0x6102, "DELCURCOMP" },  { 0x6130, "SIGCURCOMP" },           \
		{ 0x6140, "ENCURCOMP" },   { 0x6152, "LVLCLPPWM" },            \
		{ 0x7005, "DCVOF" },       { 0x7065, "DCVOS" },                \
		{ 0x70c3, "DCMCC" },       { 0x7101, "DCCV" },                 \
		{ 0x7120, "DCIE" },	{ 0x7130, "DCSR" },                 \
		{ 0x7140, "DCDIS" },       { 0x7150, "DCPWM" },                \
		{ 0x7160, "DCTRACK" },     { 0x7170, "DCENVSEL" },             \
		{ 0x7195, "OVSCTLVL" },    { 0x7204, "DCTRIP" },               \
		{ 0x7254, "DCTRIP2" },     { 0x72a4, "DCTRIPT" },              \
		{ 0x72f0, "DCTRIPHYSTE" }, { 0x7304, "DCHOLD" },               \
		{ 0x9000, "RST" },	 { 0x9011, "DMEM" },                 \
		{ 0x9030, "AIF" },	 { 0x9040, "CFINT" },                \
		{ 0x9050, "CFCGATE" },     { 0x9080, "REQCMD" },               \
		{ 0x9090, "REQRST" },      { 0x90a0, "REQMIPS" },              \
		{ 0x90b0, "REQMUTED" },    { 0x90c0, "REQVOL" },               \
		{ 0x90d0, "REQDMG" },      { 0x90e0, "REQCAL" },               \
		{ 0x90f0, "REQRSV" },      { 0x910f, "MADD" },                 \
		{ 0x920f, "MEMA" },	{ 0x9307, "ERR" },                  \
		{ 0x9380, "ACKCMD" },      { 0x9390, "ACKRST" },               \
		{ 0x93a0, "ACKMIPS" },     { 0x93b0, "ACKMUTED" },             \
		{ 0x93c0, "ACKVOL" },      { 0x93d0, "ACKDMG" },               \
		{ 0x93e0, "ACKCAL" },      { 0x93f0, "ACKRSV" },               \
		{ 0xa107, "MTPK" },	{ 0xa200, "KEY1LOCKED" },           \
		{ 0xa210, "KEY2LOCKED" },  { 0xa350, "CMTPI" },                \
		{ 0xa360, "CIMTP" },       { 0xa50f, "MTPRDMSB" },             \
		{ 0xa60f, "MTPRDLSB" },    { 0xb108, "EXTTS" },                \
		{ 0xb190, "TROS" },	{ 0xca05, "PLLINSELI" },            \
		{ 0xca64, "PLLINSELP" },   { 0xcab3, "PLLINSELR" },            \
		{ 0xcb09, "PLLNDEC" },     { 0xcba0, "PLLMDECMSB" },           \
		{ 0xcbb0, "PLLBYPASS" },   { 0xcbc0, "PLLDIRECTI" },           \
		{ 0xcbd0, "PLLDIRECTO" },  { 0xcbe0, "PLLFRMSTBL" },           \
		{ 0xcbf0, "PLLFRM" },      { 0xcc0f, "PLLMDECLSB" },           \
		{ 0xcd06, "PLLPDEC" },     { 0xcd70, "DIRECTPLL" },            \
		{ 0xcd80, "DIRECTCLK" },   { 0xcd90, "PLLLIM" },               \
		{ 0xe00f, "SWPROFIL" },    { 0xe10f, "SWVSTEP" },              \
		{ 0xf000, "MTPOTC" },      { 0xf010, "MTPEX" },                \
		{ 0xf020, "DCMCCAPI" },    { 0xf030, "DCMCCSB" },              \
		{ 0xf042, "USERDEF" },     { 0xf078, "CUSTINFO" },             \
		{ 0xf50f, "R25C" },	{ 0xffff, "Unknown bitfield enum" } \
	}
#define TFA9894N2_BITNAMETABLE                                    \
	static struct TfaBfName Tfa9894N2BitNames[] = {           \
		{ 0x0, "powerdown" },                             \
		{ 0x10, "reset" },                                \
		{ 0x20, "enbl_coolflux" },                        \
		{ 0x30, "enbl_amplifier" },                       \
		{ 0x40, "enbl_boost" },                           \
		{ 0x50, "coolflux_configured" },                  \
		{ 0x60, "sel_enbl_amplifier" },                   \
		{ 0x71, "int_pad_io" },                           \
		{ 0x90, "fs_pulse_sel" },                         \
		{ 0xa0, "bypass_ocp" },                           \
		{ 0xb0, "test_ocp" },                             \
		{ 0xc0, "batsense_steepness" },                   \
		{ 0xd0, "bypass_hp" },                            \
		{ 0xe0, "enbl_dpsa" },                            \
		{ 0xf0, "sel_hysteresis" },                       \
		{ 0x101, "vamp_sel1" },                           \
		{ 0x120, "src_set_configured" },                  \
		{ 0x130, "execute_cold_start" },                  \
		{ 0x140, "man_enbl_brown_out" },                  \
		{ 0x150, "bod_enbl" },                            \
		{ 0x160, "bod_hyst_enbl" },                       \
		{ 0x171, "bod_delay_set" },                       \
		{ 0x191, "bod_lvl_set" },                         \
		{ 0x1b0, "disable_mute_time_out" },               \
		{ 0x1c0, "man_enbl_watchdog" },                   \
		{ 0x1d0, "disable_engage" },                      \
		{ 0x1e0, "unprotect_faim" },                      \
		{ 0x1f0, "faim_enable_vbg" },                     \
		{ 0x203, "audio_fs" },                            \
		{ 0x240, "input_level" },                         \
		{ 0x255, "cs_frac_delay" },                       \
		{ 0x2b1, "use_tdm_presence" },                    \
		{ 0x2d2, "ctrl_on2off_criterion" },               \
		{ 0x30f, "device_rev" },                          \
		{ 0x401, "pll_clkin_sel" },                       \
		{ 0x420, "pll_clkin_sel_osc" },                   \
		{ 0x432, "mclk_sel" },                            \
		{ 0x460, "enbl_osc1m_auto_off" },                 \
		{ 0x470, "disable_auto_sel_refclk" },             \
		{ 0x480, "enbl_fs_sync" },                        \
		{ 0x490, "enbl_clkref_sync" },                    \
		{ 0x4a0, "pll_slow_startup" },                    \
		{ 0x500, "disable_cgu_sync_cgate" },              \
		{ 0x510, "enbl_spkr_ss" },                        \
		{ 0x520, "enbl_faim_ss" },                        \
		{ 0x530, "enbl_wdt_clk" },                        \
		{ 0xe07, "ctrl_digtoana" },                       \
		{ 0xf0f, "hidden_code" },                         \
		{ 0x1000, "flag_por" },                           \
		{ 0x1010, "flag_pll_lock" },                      \
		{ 0x1020, "flag_otpok" },                         \
		{ 0x1030, "flag_ovpok" },                         \
		{ 0x1040, "flag_uvpok" },                         \
		{ 0x1050, "flag_ocp_alarm" },                     \
		{ 0x1060, "flag_clocks_stable" },                 \
		{ 0x1070, "flag_mtp_busy" },                      \
		{ 0x1080, "flag_lost_clk" },                      \
		{ 0x1090, "flag_cold_started" },                  \
		{ 0x10a0, "flag_watchdog_reset" },                \
		{ 0x10b0, "flag_engage" },                        \
		{ 0x10c0, "flag_enbl_amp" },                      \
		{ 0x10d0, "flag_enbl_ref" },                      \
		{ 0x10e0, "flag_adc10_ready" },                   \
		{ 0x10f0, "flag_bod_vddd_nok" },                  \
		{ 0x1100, "flag_bst_bstcur" },                    \
		{ 0x1110, "flag_bst_hiz" },                       \
		{ 0x1120, "flag_bst_ocpok" },                     \
		{ 0x1130, "flag_bst_peakcur" },                   \
		{ 0x1140, "flag_bst_voutcomp" },                  \
		{ 0x1150, "flag_bst_voutcomp86" },                \
		{ 0x1160, "flag_bst_voutcomp93" },                \
		{ 0x1170, "flag_cf_speakererror" },               \
		{ 0x1180, "flag_clk_out_of_range" },              \
		{ 0x1190, "flag_man_alarm_state" },               \
		{ 0x11a0, "flag_tdm_error" },                     \
		{ 0x11b0, "flag_tdm_lut_error" },                 \
		{ 0x11c0, "flag_lost_audio_clk" },                \
		{ 0x1200, "flag_ocpokap" },                       \
		{ 0x1210, "flag_ocpokan" },                       \
		{ 0x1220, "flag_ocpokbp" },                       \
		{ 0x1230, "flag_ocpokbn" },                       \
		{ 0x1240, "flag_clip" },                          \
		{ 0x1250, "flag_man_start_mute_audio" },          \
		{ 0x1260, "flag_man_operating_state" },           \
		{ 0x1270, "flag_lp_detect_mode1" },               \
		{ 0x1280, "flag_low_amplitude" },                 \
		{ 0x1290, "flag_vddp_gt_vbat" },                  \
		{ 0x1302, "tdm_status" },                         \
		{ 0x1333, "man_state" },                          \
		{ 0x1373, "amp_ctrl_state" },                     \
		{ 0x13b1, "status_bst_mode" },                    \
		{ 0x1509, "bat_adc" },                            \
		{ 0x1608, "temp_adc" },                           \
		{ 0x1709, "vddp_adc" },                           \
		{ 0x2000, "tdm_enable" },                         \
		{ 0x2010, "tdm_sink0_enable" },                   \
		{ 0x2020, "tdm_sink1_enable" },                   \
		{ 0x2030, "tdm_source0_enable" },                 \
		{ 0x2040, "tdm_source1_enable" },                 \
		{ 0x2050, "tdm_source2_enable" },                 \
		{ 0x2060, "tdm_source3_enable" },                 \
		{ 0x2070, "tdm_clk_inversion" },                  \
		{ 0x2080, "tdm_fs_ws_polarity" },                 \
		{ 0x2090, "tdm_data_delay" },                     \
		{ 0x20a0, "tdm_data_adjustment" },                \
		{ 0x20b1, "tdm_audio_sample_compression" },       \
		{ 0x2103, "tdm_nbck" },                           \
		{ 0x2143, "tdm_fs_ws_length" },                   \
		{ 0x2183, "tdm_nb_of_slots" },                    \
		{ 0x21c1, "tdm_txdata_format" },                  \
		{ 0x21e1, "tdm_txdata_format_unused_slot" },      \
		{ 0x2204, "tdm_slot_length" },                    \
		{ 0x2254, "tdm_bits_remaining" },                 \
		{ 0x22a4, "tdm_sample_size" },                    \
		{ 0x2303, "tdm_sink0_slot" },                     \
		{ 0x2343, "tdm_sink1_slot" },                     \
		{ 0x2381, "tdm_source2_sel" },                    \
		{ 0x23a1, "tdm_source3_sel" },                    \
		{ 0x2403, "tdm_source0_slot" },                   \
		{ 0x2443, "tdm_source1_slot" },                   \
		{ 0x2483, "tdm_source2_slot" },                   \
		{ 0x24c3, "tdm_source3_slot" },                   \
		{ 0x4000, "int_out_flag_por" },                   \
		{ 0x4010, "int_out_flag_bst_ocpok" },             \
		{ 0x4020, "int_out_flag_otpok" },                 \
		{ 0x4030, "int_out_flag_ocp_alarm" },             \
		{ 0x4040, "int_out_flag_uvpok" },                 \
		{ 0x4050, "int_out_flag_man_alarm_state" },       \
		{ 0x4060, "int_out_flag_tdm_error" },             \
		{ 0x4070, "int_out_flag_lost_clk" },              \
		{ 0x4080, "int_out_flag_cfma_err" },              \
		{ 0x4090, "int_out_flag_cfma_ack" },              \
		{ 0x40a0, "int_out_flag_cf_speakererror" },       \
		{ 0x40b0, "int_out_flag_cold_started" },          \
		{ 0x40c0, "int_out_flag_watchdog_reset" },        \
		{ 0x40d0, "int_out_flag_bod_vddd_nok" },          \
		{ 0x40e0, "int_out_flag_lp_detect_mode1" },       \
		{ 0x40f0, "int_out_flag_clk_out_of_range" },      \
		{ 0x4400, "int_in_flag_por" },                    \
		{ 0x4410, "int_in_flag_bst_ocpok" },              \
		{ 0x4420, "int_in_flag_otpok" },                  \
		{ 0x4430, "int_in_flag_ocp_alarm" },              \
		{ 0x4440, "int_in_flag_uvpok" },                  \
		{ 0x4450, "int_in_flag_man_alarm_state" },        \
		{ 0x4460, "int_in_flag_tdm_error" },              \
		{ 0x4470, "int_in_flag_lost_clk" },               \
		{ 0x4480, "int_in_flag_cfma_err" },               \
		{ 0x4490, "int_in_flag_cfma_ack" },               \
		{ 0x44a0, "int_in_flag_cf_speakererror" },        \
		{ 0x44b0, "int_in_flag_cold_started" },           \
		{ 0x44c0, "int_in_flag_watchdog_reset" },         \
		{ 0x44d0, "int_in_flag_bod_vddd_nok" },           \
		{ 0x44e0, "int_in_flag_lp_detect_mode1" },        \
		{ 0x44f0, "int_in_flag_clk_out_of_range" },       \
		{ 0x4800, "int_enable_flag_por" },                \
		{ 0x4810, "int_enable_flag_bst_ocpok" },          \
		{ 0x4820, "int_enable_flag_otpok" },              \
		{ 0x4830, "int_enable_flag_ocp_alarm" },          \
		{ 0x4840, "int_enable_flag_uvpok" },              \
		{ 0x4850, "int_enable_flag_man_alarm_state" },    \
		{ 0x4860, "int_enable_flag_tdm_error" },          \
		{ 0x4870, "int_enable_flag_lost_clk" },           \
		{ 0x4880, "int_enable_flag_cfma_err" },           \
		{ 0x4890, "int_enable_flag_cfma_ack" },           \
		{ 0x48a0, "int_enable_flag_cf_speakererror" },    \
		{ 0x48b0, "int_enable_flag_cold_started" },       \
		{ 0x48c0, "int_enable_flag_watchdog_reset" },     \
		{ 0x48d0, "int_enable_flag_bod_vddd_nok" },       \
		{ 0x48e0, "int_enable_flag_lp_detect_mode1" },    \
		{ 0x48f0, "int_enable_flag_clk_out_of_range" },   \
		{ 0x4c00, "int_polarity_flag_por" },              \
		{ 0x4c10, "int_polarity_flag_bst_ocpok" },        \
		{ 0x4c20, "int_polarity_flag_otpok" },            \
		{ 0x4c30, "int_polarity_flag_ocp_alarm" },        \
		{ 0x4c40, "int_polarity_flag_uvpok" },            \
		{ 0x4c50, "int_polarity_flag_man_alarm_state" },  \
		{ 0x4c60, "int_polarity_flag_tdm_error" },        \
		{ 0x4c70, "int_polarity_flag_lost_clk" },         \
		{ 0x4c80, "int_polarity_flag_cfma_err" },         \
		{ 0x4c90, "int_polarity_flag_cfma_ack" },         \
		{ 0x4ca0, "int_polarity_flag_cf_speakererror" },  \
		{ 0x4cb0, "int_polarity_flag_cold_started" },     \
		{ 0x4cc0, "int_polarity_flag_watchdog_reset" },   \
		{ 0x4cd0, "int_polarity_flag_bod_vddd_nok" },     \
		{ 0x4ce0, "int_polarity_flag_lp_detect_mode1" },  \
		{ 0x4cf0, "int_polarity_flag_clk_out_of_range" }, \
		{ 0x5001, "vbat_prot_attack_time" },              \
		{ 0x5023, "vbat_prot_thlevel" },                  \
		{ 0x5061, "vbat_prot_max_reduct" },               \
		{ 0x5082, "vbat_prot_release_time" },             \
		{ 0x50b1, "vbat_prot_hysterese" },                \
		{ 0x50d0, "rst_min_vbat" },                       \
		{ 0x50e0, "sel_vbat" },                           \
		{ 0x50f0, "bypass_clipper" },                     \
		{ 0x5130, "cf_mute" },                            \
		{ 0x5187, "cf_volume" },                          \
		{ 0x5202, "ctrl_cc" },                            \
		{ 0x5230, "ctrl_slopectrl" },                     \
		{ 0x5240, "ctrl_slope" },                         \
		{ 0x5250, "bypass_dly_line" },                    \
		{ 0x5287, "gain" },                               \
		{ 0x5301, "dpsa_level" },                         \
		{ 0x5321, "dpsa_release" },                       \
		{ 0x5340, "clipfast" },                           \
		{ 0x5350, "bypass_lp" },                          \
		{ 0x5360, "first_order_mode" },                   \
		{ 0x5370, "icomp_engage" },                       \
		{ 0x5380, "ctrl_kickback" },                      \
		{ 0x5390, "icomp_engage_overrule" },              \
		{ 0x53a3, "ctrl_dem" },                           \
		{ 0x5400, "bypass_ctrlloop" },                    \
		{ 0x5413, "ctrl_dem_mismatch" },                  \
		{ 0x5452, "dpsa_drive" },                         \
		{ 0x550a, "enbl_amp" },                           \
		{ 0x55b0, "enbl_engage" },                        \
		{ 0x55c0, "enbl_engage_pst" },                    \
		{ 0x5600, "pwm_shape" },                          \
		{ 0x5614, "pwm_delay" },                          \
		{ 0x5660, "reclock_pwm" },                        \
		{ 0x5670, "reclock_voltsense" },                  \
		{ 0x5680, "enbl_pwm_phase_shift" },               \
		{ 0x5690, "sel_pwm_delay_src" },                  \
		{ 0x56a1, "enbl_odd_up_even_down" },              \
		{ 0x5703, "ctrl_att_dcdc" },                      \
		{ 0x5743, "ctrl_att_spkr" },                      \
		{ 0x5781, "vamp_sel2" },                          \
		{ 0x5805, "zero_lvl" },                           \
		{ 0x5861, "ctrl_fb_resistor" },                   \
		{ 0x5881, "lownoisegain_mode" },                  \
		{ 0x5905, "threshold_lvl" },                      \
		{ 0x5965, "hold_time" },                          \
		{ 0x5a05, "lpm1_cal_offset" },                    \
		{ 0x5a65, "lpm1_zero_lvl" },                      \
		{ 0x5ac1, "lpm1_mode" },                          \
		{ 0x5b05, "lpm1_threshold_lvl" },                 \
		{ 0x5b65, "lpm1_hold_time" },                     \
		{ 0x5bc0, "disable_low_power_mode" },             \
		{ 0x5c00, "enbl_minion" },                        \
		{ 0x5c13, "vth_vddpvbat" },                       \
		{ 0x5c50, "lpen_vddpvbat" },                      \
		{ 0x5c61, "ctrl_rfb" },                           \
		{ 0x5d02, "tdm_source_mapping" },                 \
		{ 0x5d31, "tdm_sourcea_frame_sel" },              \
		{ 0x5d51, "tdm_sourceb_frame_sel" },              \
		{ 0x5d71, "tdm_source0_clip_sel" },               \
		{ 0x5d91, "tdm_source1_clip_sel" },               \
		{ 0x5e02, "rst_min_vbat_delay" },                 \
		{ 0x5e30, "rst_min_vbat_sel" },                   \
		{ 0x5f00, "hard_mute" },                          \
		{ 0x5f12, "ns_hp2ln_criterion" },                 \
		{ 0x5f42, "ns_ln2hp_criterion" },                 \
		{ 0x5f78, "spare_out" },                          \
		{ 0x600f, "spare_in" },                           \
		{ 0x6102, "cursense_comp_delay" },                \
		{ 0x6130, "cursense_comp_sign" },                 \
		{ 0x6140, "enbl_cursense_comp" },                 \
		{ 0x6152, "pwms_clip_lvl" },                      \
		{ 0x7005, "frst_boost_voltage" },                 \
		{ 0x7065, "scnd_boost_voltage" },                 \
		{ 0x70c3, "boost_cur" },                          \
		{ 0x7101, "bst_slpcmplvl" },                      \
		{ 0x7120, "boost_intel" },                        \
		{ 0x7130, "boost_speed" },                        \
		{ 0x7140, "dcdcoff_mode" },                       \
		{ 0x7150, "dcdc_pwmonly" },                       \
		{ 0x7160, "boost_track" },                        \
		{ 0x7170, "sel_dcdc_envelope_8fs" },              \
		{ 0x7180, "ignore_flag_voutcomp86" },             \
		{ 0x7195, "overshoot_correction_lvl" },           \
		{ 0x7204, "boost_trip_lvl_1st" },                 \
		{ 0x7254, "boost_trip_lvl_2nd" },                 \
		{ 0x72a4, "boost_trip_lvl_track" },               \
		{ 0x72f0, "enbl_trip_hyst" },                     \
		{ 0x7304, "boost_hold_time" },                    \
		{ 0x7350, "dcdc_pfm20khz_limit" },                \
		{ 0x7361, "dcdc_ctrl_maxzercnt" },                \
		{ 0x7386, "dcdc_vbat_delta_detect" },             \
		{ 0x73f0, "dcdc_ignore_vbat" },                   \
		{ 0x7404, "bst_drive" },                          \
		{ 0x7451, "bst_scalecur" },                       \
		{ 0x7474, "bst_slopecur" },                       \
		{ 0x74c1, "bst_slope" },                          \
		{ 0x74e0, "bst_bypass_bstcur" },                  \
		{ 0x74f0, "bst_bypass_bstfoldback" },             \
		{ 0x7500, "enbl_bst_engage" },                    \
		{ 0x7510, "enbl_bst_hizcom" },                    \
		{ 0x7520, "enbl_bst_peakcur" },                   \
		{ 0x7530, "enbl_bst_power" },                     \
		{ 0x7540, "enbl_bst_slopecur" },                  \
		{ 0x7550, "enbl_bst_voutcomp" },                  \
		{ 0x7560, "enbl_bst_voutcomp86" },                \
		{ 0x7570, "enbl_bst_voutcomp93" },                \
		{ 0x7580, "enbl_bst_windac" },                    \
		{ 0x7595, "bst_windac" },                         \
		{ 0x7600, "boost_alg" },                          \
		{ 0x7611, "boost_loopgain" },                     \
		{ 0x7631, "bst_freq" },                           \
		{ 0x7650, "enbl_bst_peak2avg" },                  \
		{ 0x7660, "bst_use_new_zercur_detect" },          \
		{ 0x8001, "sel_clk_cs" },                         \
		{ 0x8021, "micadc_speed" },                       \
		{ 0x8040, "cs_gain_control" },                    \
		{ 0x8050, "cs_bypass_gc" },                       \
		{ 0x8060, "invertpwm" },                          \
		{ 0x8087, "cs_gain" },                            \
		{ 0x8105, "cs_ktemp" },                           \
		{ 0x8164, "cs_ktemp2" },                          \
		{ 0x81b0, "enbl_cs_adc" },                        \
		{ 0x81c0, "enbl_cs_inn1" },                       \
		{ 0x81d0, "enbl_cs_inn2" },                       \
		{ 0x81e0, "enbl_cs_inp1" },                       \
		{ 0x81f0, "enbl_cs_inp2" },                       \
		{ 0x8200, "enbl_cs_ldo" },                        \
		{ 0x8210, "enbl_cs_vbatldo" },                    \
		{ 0x8220, "cs_adc_bsoinv" },                      \
		{ 0x8231, "cs_adc_hifreq" },                      \
		{ 0x8250, "cs_adc_nortz" },                       \
		{ 0x8263, "cs_adc_offset" },                      \
		{ 0x82a0, "cs_adc_slowdel" },                     \
		{ 0x82b4, "cs_adc_gain" },                        \
		{ 0x8300, "cs_resonator_enable" },                \
		{ 0x8310, "cs_classd_tran_skip" },                \
		{ 0x8320, "cs_inn_short" },                       \
		{ 0x8330, "cs_inp_short" },                       \
		{ 0x8340, "cs_ldo_bypass" },                      \
		{ 0x8350, "cs_ldo_pulldown" },                    \
		{ 0x8364, "cs_ldo_voset" },                       \
		{ 0x8800, "ctrl_vs_igen_supply" },                \
		{ 0x8810, "ctrl_vs_force_div2" },                 \
		{ 0x8820, "enbl_dc_filter" },                     \
		{ 0x8901, "volsense_pwm_sel" },                   \
		{ 0x8920, "vs_gain_control" },                    \
		{ 0x8930, "vs_bypass_gc" },                       \
		{ 0x8940, "vs_adc_bsoinv" },                      \
		{ 0x8950, "vs_adc_nortz" },                       \
		{ 0x8960, "vs_adc_slowdel" },                     \
		{ 0x8970, "vs_classd_tran_skip" },                \
		{ 0x8987, "vs_gain" },                            \
		{ 0x8a00, "vs_inn_short" },                       \
		{ 0x8a10, "vs_inp_short" },                       \
		{ 0x8a20, "vs_ldo_bypass" },                      \
		{ 0x8a30, "vs_ldo_pulldown" },                    \
		{ 0x8a44, "vs_ldo_voset" },                       \
		{ 0x8a90, "enbl_vs_adc" },                        \
		{ 0x8aa0, "enbl_vs_inn1" },                       \
		{ 0x8ab0, "enbl_vs_inn2" },                       \
		{ 0x8ac0, "enbl_vs_inp1" },                       \
		{ 0x8ad0, "enbl_vs_inp2" },                       \
		{ 0x8ae0, "enbl_vs_ldo" },                        \
		{ 0x8af0, "enbl_vs_vbatldo" },                    \
		{ 0x9000, "cf_rst_dsp" },                         \
		{ 0x9011, "cf_dmem" },                            \
		{ 0x9030, "cf_aif" },                             \
		{ 0x9040, "cf_int" },                             \
		{ 0x9050, "cf_cgate_off" },                       \
		{ 0x9080, "cf_req_cmd" },                         \
		{ 0x9090, "cf_req_reset" },                       \
		{ 0x90a0, "cf_req_mips" },                        \
		{ 0x90b0, "cf_req_mute_ready" },                  \
		{ 0x90c0, "cf_req_volume_ready" },                \
		{ 0x90d0, "cf_req_damage" },                      \
		{ 0x90e0, "cf_req_calibrate_ready" },             \
		{ 0x90f0, "cf_req_reserved" },                    \
		{ 0x910f, "cf_madd" },                            \
		{ 0x920f, "cf_mema" },                            \
		{ 0x9307, "cf_err" },                             \
		{ 0x9380, "cf_ack_cmd" },                         \
		{ 0x9390, "cf_ack_reset" },                       \
		{ 0x93a0, "cf_ack_mips" },                        \
		{ 0x93b0, "cf_ack_mute_ready" },                  \
		{ 0x93c0, "cf_ack_volume_ready" },                \
		{ 0x93d0, "cf_ack_damage" },                      \
		{ 0x93e0, "cf_ack_calibrate_ready" },             \
		{ 0x93f0, "cf_ack_reserved" },                    \
		{ 0xa007, "mtpkey1" },                            \
		{ 0xa107, "mtpkey2" },                            \
		{ 0xa200, "key01_locked" },                       \
		{ 0xa210, "key02_locked" },                       \
		{ 0xa302, "mtp_man_address_in" },                 \
		{ 0xa330, "man_copy_mtp_to_iic" },                \
		{ 0xa340, "man_copy_iic_to_mtp" },                \
		{ 0xa350, "auto_copy_mtp_to_iic" },               \
		{ 0xa360, "auto_copy_iic_to_mtp" },               \
		{ 0xa400, "faim_set_clkws" },                     \
		{ 0xa410, "faim_sel_evenrows" },                  \
		{ 0xa420, "faim_sel_oddrows" },                   \
		{ 0xa430, "faim_program_only" },                  \
		{ 0xa440, "faim_erase_only" },                    \
		{ 0xa50f, "mtp_man_data_out_msb" },               \
		{ 0xa60f, "mtp_man_data_out_lsb" },               \
		{ 0xa70f, "mtp_man_data_in_msb" },                \
		{ 0xa80f, "mtp_man_data_in_lsb" },                \
		{ 0xb000, "bypass_ocpcounter" },                  \
		{ 0xb010, "bypass_glitchfilter" },                \
		{ 0xb020, "bypass_ovp" },                         \
		{ 0xb030, "bypass_uvp" },                         \
		{ 0xb040, "bypass_otp" },                         \
		{ 0xb050, "bypass_lost_clk" },                    \
		{ 0xb060, "ctrl_vpalarm" },                       \
		{ 0xb070, "disable_main_ctrl_change_prot" },      \
		{ 0xb087, "ocp_threshold" },                      \
		{ 0xb108, "ext_temp" },                           \
		{ 0xb190, "ext_temp_sel" },                       \
		{ 0xc000, "use_direct_ctrls" },                   \
		{ 0xc010, "rst_datapath" },                       \
		{ 0xc020, "rst_cgu" },                            \
		{ 0xc038, "enbl_ref" },                           \
		{ 0xc0c0, "use_direct_vs_ctrls" },                \
		{ 0xc0d0, "enbl_ringo" },                         \
		{ 0xc0e0, "enbl_pll" },                           \
		{ 0xc0f0, "enbl_osc" },                           \
		{ 0xc100, "enbl_tsense" },                        \
		{ 0xc110, "tsense_hibias" },                      \
		{ 0xc120, "enbl_flag_vbg" },                      \
		{ 0xc20f, "abist_offset" },                       \
		{ 0xc300, "bypasslatch" },                        \
		{ 0xc311, "sourcea" },                            \
		{ 0xc331, "sourceb" },                            \
		{ 0xc350, "inverta" },                            \
		{ 0xc360, "invertb" },                            \
		{ 0xc374, "pulselength" },                        \
		{ 0xc3d0, "test_abistfft_enbl" },                 \
		{ 0xc400, "bst_bypasslatch" },                    \
		{ 0xc411, "bst_source" },                         \
		{ 0xc430, "bst_invertb" },                        \
		{ 0xc444, "bst_pulselength" },                    \
		{ 0xc490, "test_bst_ctrlsthv" },                  \
		{ 0xc4a0, "test_bst_iddq" },                      \
		{ 0xc4b0, "test_bst_rdson" },                     \
		{ 0xc4c0, "test_bst_cvi" },                       \
		{ 0xc4d0, "test_bst_ocp" },                       \
		{ 0xc4e0, "test_bst_sense" },                     \
		{ 0xc500, "test_cvi" },                           \
		{ 0xc510, "test_discrete" },                      \
		{ 0xc520, "test_iddq" },                          \
		{ 0xc530, "test_rdson" },                         \
		{ 0xc540, "test_sdelta" },                        \
		{ 0xc550, "test_enbl_cs" },                       \
		{ 0xc560, "test_enbl_vs" },                       \
		{ 0xc570, "enbl_pwm_dcc" },                       \
		{ 0xc583, "pwm_dcc_cnt" },                        \
		{ 0xc5c0, "enbl_ldo_stress" },                    \
		{ 0xc607, "digimuxa_sel" },                       \
		{ 0xc687, "digimuxb_sel" },                       \
		{ 0xc707, "digimuxc_sel" },                       \
		{ 0xc800, "enbl_anamux1" },                       \
		{ 0xc810, "enbl_anamux2" },                       \
		{ 0xc820, "enbl_anamux3" },                       \
		{ 0xc830, "enbl_anamux4" },                       \
		{ 0xc844, "anamux1" },                            \
		{ 0xc894, "anamux2" },                            \
		{ 0xc903, "anamux3" },                            \
		{ 0xc943, "anamux4" },                            \
		{ 0xca05, "pll_inseli" },                         \
		{ 0xca64, "pll_inselp" },                         \
		{ 0xcab3, "pll_inselr" },                         \
		{ 0xcaf0, "pll_bandsel" },                        \
		{ 0xcb09, "pll_ndec" },                           \
		{ 0xcba0, "pll_mdec_msb" },                       \
		{ 0xcbb0, "pll_bypass" },                         \
		{ 0xcbc0, "pll_directi" },                        \
		{ 0xcbd0, "pll_directo" },                        \
		{ 0xcbe0, "pll_frm_clockstable" },                \
		{ 0xcbf0, "pll_frm" },                            \
		{ 0xcc0f, "pll_mdec_lsb" },                       \
		{ 0xcd06, "pll_pdec" },                           \
		{ 0xcd70, "use_direct_pll_ctrl" },                \
		{ 0xcd80, "use_direct_clk_ctrl" },                \
		{ 0xcd90, "pll_limup_off" },                      \
		{ 0xce0f, "tsig_freq_lsb" },                      \
		{ 0xcf02, "tsig_freq_msb" },                      \
		{ 0xcf33, "tsig_gain" },                          \
		{ 0xd000, "adc10_reset" },                        \
		{ 0xd011, "adc10_test" },                         \
		{ 0xd032, "adc10_sel" },                          \
		{ 0xd064, "adc10_prog_sample" },                  \
		{ 0xd0b0, "adc10_enbl" },                         \
		{ 0xd0c0, "bypass_lp_vbat" },                     \
		{ 0xd109, "data_adc10_tempbat" },                 \
		{ 0xd201, "clkdiv_audio_sel" },                   \
		{ 0xd301, "int_ehs" },                            \
		{ 0xd321, "datao_ehs" },                          \
		{ 0xd340, "hs_mode" },                            \
		{ 0xd407, "ctrl_digtoana_hidden" },               \
		{ 0xd480, "enbl_clk_out_of_range" },              \
		{ 0xd491, "sel_wdt_clk" },                        \
		{ 0xd4b0, "inject_tsig" },                        \
		{ 0xd500, "source_in_testmode" },                 \
		{ 0xd510, "gainatt_feedback" },                   \
		{ 0xd522, "test_parametric_io" },                 \
		{ 0xd550, "ctrl_bst_clk_lp1" },                   \
		{ 0xd561, "test_spare_out1" },                    \
		{ 0xd580, "bst_dcmbst" },                         \
		{ 0xd593, "test_spare_out2" },                    \
		{ 0xe00f, "sw_profile" },                         \
		{ 0xe10f, "sw_vstep" },                           \
		{ 0xf000, "calibration_onetime" },                \
		{ 0xf010, "calibr_ron_done" },                    \
		{ 0xf020, "calibr_dcdc_api_calibrate" },          \
		{ 0xf030, "calibr_dcdc_delta_sign" },             \
		{ 0xf042, "calibr_dcdc_delta" },                  \
		{ 0xf078, "calibr_speaker_info" },                \
		{ 0xf105, "calibr_vout_offset" },                 \
		{ 0xf163, "calibr_vbg_trim" },                    \
		{ 0xf203, "calibr_gain" },                        \
		{ 0xf245, "calibr_offset" },                      \
		{ 0xf307, "calibr_gain_vs" },                     \
		{ 0xf387, "calibr_gain_cs" },                     \
		{ 0xf40f, "mtpdata4" },                           \
		{ 0xf50f, "calibr_R25C" },                        \
		{ 0xf60f, "mtpdata6" },                           \
		{ 0xf706, "ctrl_offset_a" },                      \
		{ 0xf786, "ctrl_offset_b" },                      \
		{ 0xf806, "htol_iic_addr" },                      \
		{ 0xf870, "htol_iic_addr_en" },                   \
		{ 0xf884, "calibr_temp_offset" },                 \
		{ 0xf8d2, "calibr_temp_gain" },                   \
		{ 0xf900, "mtp_lock_dcdcoff_mode" },              \
		{ 0xf910, "mtp_lock_enbl_coolflux" },             \
		{ 0xf920, "mtp_lock_bypass_clipper" },            \
		{ 0xf930, "mtp_enbl_pwm_delay_clock_gating" },    \
		{ 0xf940, "mtp_enbl_ocp_clock_gating" },          \
		{ 0xf987, "type_bits_fw" },                       \
		{ 0xfa0f, "mtpdataA" },                           \
		{ 0xfb0f, "mtpdataB" },                           \
		{ 0xfc0f, "mtpdataC" },                           \
		{ 0xfd0f, "mtpdataD" },                           \
		{ 0xfe0f, "mtpdataE" },                           \
		{ 0xff07, "calibr_osc_delta_ndiv" },              \
		{ 0xffff, "Unknown bitfield enum" }               \
	}
#if 0
enum tfa9894_irq {
	tfa9894_irq_max = -1,
	tfa9894_irq_all = -1
};
#endif
#define TFA9894_IRQ_NAMETABLE static struct TfaIrqName Tfa9894IrqNames[] = {}
#endif
