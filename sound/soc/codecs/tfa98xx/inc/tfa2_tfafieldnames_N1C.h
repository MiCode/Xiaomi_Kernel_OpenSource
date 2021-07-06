/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define TFA9888_I2CVERSION 18
enum nxpTfa2BfEnumList {
	TFA2_BF_PWDN = 0x0000,
	TFA2_BF_I2CR = 0x0010,
	TFA2_BF_CFE = 0x0020,
	TFA2_BF_AMPE = 0x0030,
	TFA2_BF_DCA = 0x0040,
	TFA2_BF_SBSL = 0x0050,
	TFA2_BF_AMPC = 0x0060,
	TFA2_BF_INTP = 0x0071,
	TFA2_BF_FSSSEL = 0x0091,
	TFA2_BF_BYPOCP = 0x00b0,
	TFA2_BF_TSTOCP = 0x00c0,
	TFA2_BF_AMPINSEL = 0x0101,
	TFA2_BF_MANSCONF = 0x0120,
	TFA2_BF_MANCOLD = 0x0130,
	TFA2_BF_MANAOOSC = 0x0140,
	TFA2_BF_MANROBOD = 0x0150,
	TFA2_BF_BODE = 0x0160,
	TFA2_BF_BODHYS = 0x0170,
	TFA2_BF_BODFILT = 0x0181,
	TFA2_BF_BODTHLVL = 0x01a1,
	TFA2_BF_MUTETO = 0x01d0,
	TFA2_BF_RCVNS = 0x01e0,
	TFA2_BF_MANWDE = 0x01f0,
	TFA2_BF_AUDFS = 0x0203,
	TFA2_BF_INPLEV = 0x0240,
	TFA2_BF_FRACTDEL = 0x0255,
	TFA2_BF_BYPHVBF = 0x02b0,
	TFA2_BF_LDOBYP = 0x02c0,
	TFA2_BF_REV = 0x030f,
	TFA2_BF_REFCKEXT = 0x0401,
	TFA2_BF_REFCKSEL = 0x0420,
	TFA2_BF_SSLEFTE = 0x0500,
	TFA2_BF_SSRIGHTE = 0x0510,
	TFA2_BF_VSLEFTE = 0x0520,
	TFA2_BF_VSRIGHTE = 0x0530,
	TFA2_BF_CSLEFTE = 0x0540,
	TFA2_BF_CSRIGHTE = 0x0550,
	TFA2_BF_SSPDME = 0x0560,
	TFA2_BF_STGAIN = 0x0d18,
	TFA2_BF_PDMSMUTE = 0x0da0,
	TFA2_BF_SWVSTEP = 0x0e06,
	TFA2_BF_VDDS = 0x1000,
	TFA2_BF_PLLS = 0x1010,
	TFA2_BF_OTDS = 0x1020,
	TFA2_BF_OVDS = 0x1030,
	TFA2_BF_UVDS = 0x1040,
	TFA2_BF_CLKS = 0x1050,
	TFA2_BF_MTPB = 0x1060,
	TFA2_BF_NOCLK = 0x1070,
	TFA2_BF_SPKS = 0x1080,
	TFA2_BF_ACS = 0x1090,
	TFA2_BF_SWS = 0x10a0,
	TFA2_BF_WDS = 0x10b0,
	TFA2_BF_AMPS = 0x10c0,
	TFA2_BF_AREFS = 0x10d0,
	TFA2_BF_ADCCR = 0x10e0,
	TFA2_BF_BODNOK = 0x10f0,
	TFA2_BF_DCIL = 0x1100,
	TFA2_BF_DCDCA = 0x1110,
	TFA2_BF_DCOCPOK = 0x1120,
	TFA2_BF_DCHVBAT = 0x1140,
	TFA2_BF_DCH114 = 0x1150,
	TFA2_BF_DCH107 = 0x1160,
	TFA2_BF_STMUTEB = 0x1170,
	TFA2_BF_STMUTE = 0x1180,
	TFA2_BF_TDMLUTER = 0x1190,
	TFA2_BF_TDMSTAT = 0x11a2,
	TFA2_BF_TDMERR = 0x11d0,
	TFA2_BF_HAPTIC = 0x11e0,
	TFA2_BF_OCPOAPL = 0x1200,
	TFA2_BF_OCPOANL = 0x1210,
	TFA2_BF_OCPOBPL = 0x1220,
	TFA2_BF_OCPOBNL = 0x1230,
	TFA2_BF_CLIPAHL = 0x1240,
	TFA2_BF_CLIPALL = 0x1250,
	TFA2_BF_CLIPBHL = 0x1260,
	TFA2_BF_CLIPBLL = 0x1270,
	TFA2_BF_OCPOAPRC = 0x1280,
	TFA2_BF_OCPOANRC = 0x1290,
	TFA2_BF_OCPOBPRC = 0x12a0,
	TFA2_BF_OCPOBNRC = 0x12b0,
	TFA2_BF_RCVLDOR = 0x12c0,
	TFA2_BF_RCVLDOBR = 0x12d0,
	TFA2_BF_OCDSL = 0x12e0,
	TFA2_BF_CLIPSL = 0x12f0,
	TFA2_BF_OCPOAPR = 0x1300,
	TFA2_BF_OCPOANR = 0x1310,
	TFA2_BF_OCPOBPR = 0x1320,
	TFA2_BF_OCPOBNR = 0x1330,
	TFA2_BF_CLIPAHR = 0x1340,
	TFA2_BF_CLIPALR = 0x1350,
	TFA2_BF_CLIPBHR = 0x1360,
	TFA2_BF_CLIPBLR = 0x1370,
	TFA2_BF_OCDSR = 0x1380,
	TFA2_BF_CLIPSR = 0x1390,
	TFA2_BF_OCPOKMC = 0x13a0,
	TFA2_BF_MANALARM = 0x13b0,
	TFA2_BF_MANWAIT1 = 0x13c0,
	TFA2_BF_MANWAIT2 = 0x13d0,
	TFA2_BF_MANMUTE = 0x13e0,
	TFA2_BF_MANOPER = 0x13f0,
	TFA2_BF_SPKSL = 0x1400,
	TFA2_BF_SPKSR = 0x1410,
	TFA2_BF_CLKOOR = 0x1420,
	TFA2_BF_MANSTATE = 0x1433,
	TFA2_BF_BATS = 0x1509,
	TFA2_BF_TEMPS = 0x1608,
	TFA2_BF_TDMUC = 0x2003,
	TFA2_BF_TDME = 0x2040,
	TFA2_BF_TDMMODE = 0x2050,
	TFA2_BF_TDMCLINV = 0x2060,
	TFA2_BF_TDMFSLN = 0x2073,
	TFA2_BF_TDMFSPOL = 0x20b0,
	TFA2_BF_TDMNBCK = 0x20c3,
	TFA2_BF_TDMSLOTS = 0x2103,
	TFA2_BF_TDMSLLN = 0x2144,
	TFA2_BF_TDMBRMG = 0x2194,
	TFA2_BF_TDMDEL = 0x21e0,
	TFA2_BF_TDMADJ = 0x21f0,
	TFA2_BF_TDMOOMP = 0x2201,
	TFA2_BF_TDMSSIZE = 0x2224,
	TFA2_BF_TDMTXDFO = 0x2271,
	TFA2_BF_TDMTXUS0 = 0x2291,
	TFA2_BF_TDMTXUS1 = 0x22b1,
	TFA2_BF_TDMTXUS2 = 0x22d1,
	TFA2_BF_TDMLE = 0x2310,
	TFA2_BF_TDMRE = 0x2320,
	TFA2_BF_TDMVSRE = 0x2340,
	TFA2_BF_TDMCSRE = 0x2350,
	TFA2_BF_TDMVSLE = 0x2360,
	TFA2_BF_TDMCSLE = 0x2370,
	TFA2_BF_TDMCFRE = 0x2380,
	TFA2_BF_TDMCFLE = 0x2390,
	TFA2_BF_TDMCF3E = 0x23a0,
	TFA2_BF_TDMCF4E = 0x23b0,
	TFA2_BF_TDMPD1E = 0x23c0,
	TFA2_BF_TDMPD2E = 0x23d0,
	TFA2_BF_TDMLIO = 0x2421,
	TFA2_BF_TDMRIO = 0x2441,
	TFA2_BF_TDMVSRIO = 0x2481,
	TFA2_BF_TDMCSRIO = 0x24a1,
	TFA2_BF_TDMVSLIO = 0x24c1,
	TFA2_BF_TDMCSLIO = 0x24e1,
	TFA2_BF_TDMCFRIO = 0x2501,
	TFA2_BF_TDMCFLIO = 0x2521,
	TFA2_BF_TDMCF3IO = 0x2541,
	TFA2_BF_TDMCF4IO = 0x2561,
	TFA2_BF_TDMPD1IO = 0x2581,
	TFA2_BF_TDMPD2IO = 0x25a1,
	TFA2_BF_TDMLS = 0x2643,
	TFA2_BF_TDMRS = 0x2683,
	TFA2_BF_TDMVSRS = 0x2703,
	TFA2_BF_TDMCSRS = 0x2743,
	TFA2_BF_TDMVSLS = 0x2783,
	TFA2_BF_TDMCSLS = 0x27c3,
	TFA2_BF_TDMCFRS = 0x2803,
	TFA2_BF_TDMCFLS = 0x2843,
	TFA2_BF_TDMCF3S = 0x2883,
	TFA2_BF_TDMCF4S = 0x28c3,
	TFA2_BF_TDMPD1S = 0x2903,
	TFA2_BF_TDMPD2S = 0x2943,
	TFA2_BF_PDMSM = 0x3100,
	TFA2_BF_PDMSTSEL = 0x3111,
	TFA2_BF_PDMLSEL = 0x3130,
	TFA2_BF_PDMRSEL = 0x3140,
	TFA2_BF_MICVDDE = 0x3150,
	TFA2_BF_PDMCLRAT = 0x3201,
	TFA2_BF_PDMGAIN = 0x3223,
	TFA2_BF_PDMOSEL = 0x3263,
	TFA2_BF_SELCFHAPD = 0x32a0,
	TFA2_BF_HAPTIME = 0x3307,
	TFA2_BF_HAPLEVEL = 0x3387,
	TFA2_BF_GPIODIN = 0x3403,
	TFA2_BF_GPIOCTRL = 0x3500,
	TFA2_BF_GPIOCONF = 0x3513,
	TFA2_BF_GPIODOUT = 0x3553,
	TFA2_BF_ISTVDDS = 0x4000,
	TFA2_BF_ISTPLLS = 0x4010,
	TFA2_BF_ISTOTDS = 0x4020,
	TFA2_BF_ISTOVDS = 0x4030,
	TFA2_BF_ISTUVDS = 0x4040,
	TFA2_BF_ISTCLKS = 0x4050,
	TFA2_BF_ISTMTPB = 0x4060,
	TFA2_BF_ISTNOCLK = 0x4070,
	TFA2_BF_ISTSPKS = 0x4080,
	TFA2_BF_ISTACS = 0x4090,
	TFA2_BF_ISTSWS = 0x40a0,
	TFA2_BF_ISTWDS = 0x40b0,
	TFA2_BF_ISTAMPS = 0x40c0,
	TFA2_BF_ISTAREFS = 0x40d0,
	TFA2_BF_ISTADCCR = 0x40e0,
	TFA2_BF_ISTBODNOK = 0x40f0,
	TFA2_BF_ISTBSTCU = 0x4100,
	TFA2_BF_ISTBSTHI = 0x4110,
	TFA2_BF_ISTBSTOC = 0x4120,
	TFA2_BF_ISTBSTPKCUR = 0x4130,
	TFA2_BF_ISTBSTVC = 0x4140,
	TFA2_BF_ISTBST86 = 0x4150,
	TFA2_BF_ISTBST93 = 0x4160,
	TFA2_BF_ISTRCVLD = 0x4170,
	TFA2_BF_ISTOCPL = 0x4180,
	TFA2_BF_ISTOCPR = 0x4190,
	TFA2_BF_ISTMWSRC = 0x41a0,
	TFA2_BF_ISTMWCFC = 0x41b0,
	TFA2_BF_ISTMWSMU = 0x41c0,
	TFA2_BF_ISTCFMER = 0x41d0,
	TFA2_BF_ISTCFMAC = 0x41e0,
	TFA2_BF_ISTCLKOOR = 0x41f0,
	TFA2_BF_ISTTDMER = 0x4200,
	TFA2_BF_ISTCLPL = 0x4210,
	TFA2_BF_ISTCLPR = 0x4220,
	TFA2_BF_ISTOCPM = 0x4230,
	TFA2_BF_ICLVDDS = 0x4400,
	TFA2_BF_ICLPLLS = 0x4410,
	TFA2_BF_ICLOTDS = 0x4420,
	TFA2_BF_ICLOVDS = 0x4430,
	TFA2_BF_ICLUVDS = 0x4440,
	TFA2_BF_ICLCLKS = 0x4450,
	TFA2_BF_ICLMTPB = 0x4460,
	TFA2_BF_ICLNOCLK = 0x4470,
	TFA2_BF_ICLSPKS = 0x4480,
	TFA2_BF_ICLACS = 0x4490,
	TFA2_BF_ICLSWS = 0x44a0,
	TFA2_BF_ICLWDS = 0x44b0,
	TFA2_BF_ICLAMPS = 0x44c0,
	TFA2_BF_ICLAREFS = 0x44d0,
	TFA2_BF_ICLADCCR = 0x44e0,
	TFA2_BF_ICLBODNOK = 0x44f0,
	TFA2_BF_ICLBSTCU = 0x4500,
	TFA2_BF_ICLBSTHI = 0x4510,
	TFA2_BF_ICLBSTOC = 0x4520,
	TFA2_BF_ICLBSTPC = 0x4530,
	TFA2_BF_ICLBSTVC = 0x4540,
	TFA2_BF_ICLBST86 = 0x4550,
	TFA2_BF_ICLBST93 = 0x4560,
	TFA2_BF_ICLRCVLD = 0x4570,
	TFA2_BF_ICLOCPL = 0x4580,
	TFA2_BF_ICLOCPR = 0x4590,
	TFA2_BF_ICLMWSRC = 0x45a0,
	TFA2_BF_ICLMWCFC = 0x45b0,
	TFA2_BF_ICLMWSMU = 0x45c0,
	TFA2_BF_ICLCFMER = 0x45d0,
	TFA2_BF_ICLCFMAC = 0x45e0,
	TFA2_BF_ICLCLKOOR = 0x45f0,
	TFA2_BF_ICLTDMER = 0x4600,
	TFA2_BF_ICLCLPL = 0x4610,
	TFA2_BF_ICLCLPR = 0x4620,
	TFA2_BF_ICLOCPM = 0x4630,
	TFA2_BF_IEVDDS = 0x4800,
	TFA2_BF_IEPLLS = 0x4810,
	TFA2_BF_IEOTDS = 0x4820,
	TFA2_BF_IEOVDS = 0x4830,
	TFA2_BF_IEUVDS = 0x4840,
	TFA2_BF_IECLKS = 0x4850,
	TFA2_BF_IEMTPB = 0x4860,
	TFA2_BF_IENOCLK = 0x4870,
	TFA2_BF_IESPKS = 0x4880,
	TFA2_BF_IEACS = 0x4890,
	TFA2_BF_IESWS = 0x48a0,
	TFA2_BF_IEWDS = 0x48b0,
	TFA2_BF_IEAMPS = 0x48c0,
	TFA2_BF_IEAREFS = 0x48d0,
	TFA2_BF_IEADCCR = 0x48e0,
	TFA2_BF_IEBODNOK = 0x48f0,
	TFA2_BF_IEBSTCU = 0x4900,
	TFA2_BF_IEBSTHI = 0x4910,
	TFA2_BF_IEBSTOC = 0x4920,
	TFA2_BF_IEBSTPC = 0x4930,
	TFA2_BF_IEBSTVC = 0x4940,
	TFA2_BF_IEBST86 = 0x4950,
	TFA2_BF_IEBST93 = 0x4960,
	TFA2_BF_IERCVLD = 0x4970,
	TFA2_BF_IEOCPL = 0x4980,
	TFA2_BF_IEOCPR = 0x4990,
	TFA2_BF_IEMWSRC = 0x49a0,
	TFA2_BF_IEMWCFC = 0x49b0,
	TFA2_BF_IEMWSMU = 0x49c0,
	TFA2_BF_IECFMER = 0x49d0,
	TFA2_BF_IECFMAC = 0x49e0,
	TFA2_BF_IECLKOOR = 0x49f0,
	TFA2_BF_IETDMER = 0x4a00,
	TFA2_BF_IECLPL = 0x4a10,
	TFA2_BF_IECLPR = 0x4a20,
	TFA2_BF_IEOCPM1 = 0x4a30,
	TFA2_BF_IPOVDDS = 0x4c00,
	TFA2_BF_IPOPLLS = 0x4c10,
	TFA2_BF_IPOOTDS = 0x4c20,
	TFA2_BF_IPOOVDS = 0x4c30,
	TFA2_BF_IPOUVDS = 0x4c40,
	TFA2_BF_IPOCLKS = 0x4c50,
	TFA2_BF_IPOMTPB = 0x4c60,
	TFA2_BF_IPONOCLK = 0x4c70,
	TFA2_BF_IPOSPKS = 0x4c80,
	TFA2_BF_IPOACS = 0x4c90,
	TFA2_BF_IPOSWS = 0x4ca0,
	TFA2_BF_IPOWDS = 0x4cb0,
	TFA2_BF_IPOAMPS = 0x4cc0,
	TFA2_BF_IPOAREFS = 0x4cd0,
	TFA2_BF_IPOADCCR = 0x4ce0,
	TFA2_BF_IPOBODNOK = 0x4cf0,
	TFA2_BF_IPOBSTCU = 0x4d00,
	TFA2_BF_IPOBSTHI = 0x4d10,
	TFA2_BF_IPOBSTOC = 0x4d20,
	TFA2_BF_IPOBSTPC = 0x4d30,
	TFA2_BF_IPOBSTVC = 0x4d40,
	TFA2_BF_IPOBST86 = 0x4d50,
	TFA2_BF_IPOBST93 = 0x4d60,
	TFA2_BF_IPORCVLD = 0x4d70,
	TFA2_BF_IPOOCPL = 0x4d80,
	TFA2_BF_IPOOCPR = 0x4d90,
	TFA2_BF_IPOMWSRC = 0x4da0,
	TFA2_BF_IPOMWCFC = 0x4db0,
	TFA2_BF_IPOMWSMU = 0x4dc0,
	TFA2_BF_IPOCFMER = 0x4dd0,
	TFA2_BF_IPOCFMAC = 0x4de0,
	TFA2_BF_IPCLKOOR = 0x4df0,
	TFA2_BF_IPOTDMER = 0x4e00,
	TFA2_BF_IPOCLPL = 0x4e10,
	TFA2_BF_IPOCLPR = 0x4e20,
	TFA2_BF_IPOOCPM = 0x4e30,
	TFA2_BF_BSSCR = 0x5001,
	TFA2_BF_BSST = 0x5023,
	TFA2_BF_BSSRL = 0x5061,
	TFA2_BF_BSSRR = 0x5082,
	TFA2_BF_BSSHY = 0x50b1,
	TFA2_BF_BSSR = 0x50e0,
	TFA2_BF_BSSBY = 0x50f0,
	TFA2_BF_BSSS = 0x5100,
	TFA2_BF_INTSMUTE = 0x5110,
	TFA2_BF_CFSML = 0x5120,
	TFA2_BF_CFSMR = 0x5130,
	TFA2_BF_HPFBYPL = 0x5140,
	TFA2_BF_HPFBYPR = 0x5150,
	TFA2_BF_DPSAL = 0x5160,
	TFA2_BF_DPSAR = 0x5170,
	TFA2_BF_VOL = 0x5187,
	TFA2_BF_HNDSFRCV = 0x5200,
	TFA2_BF_CLIPCTRL = 0x5222,
	TFA2_BF_AMPGAIN = 0x5257,
	TFA2_BF_SLOPEE = 0x52d0,
	TFA2_BF_SLOPESET = 0x52e1,
	TFA2_BF_VOLSEC = 0x5a07,
	TFA2_BF_SWPROFIL = 0x5a87,
	TFA2_BF_DCVO = 0x7002,
	TFA2_BF_DCMCC = 0x7033,
	TFA2_BF_DCCV = 0x7071,
	TFA2_BF_DCIE = 0x7090,
	TFA2_BF_DCSR = 0x70a0,
	TFA2_BF_DCSYNCP = 0x70b2,
	TFA2_BF_DCDIS = 0x70e0,
	TFA2_BF_RST = 0x9000,
	TFA2_BF_DMEM = 0x9011,
	TFA2_BF_AIF = 0x9030,
	TFA2_BF_CFINT = 0x9040,
	TFA2_BF_CFCGATE = 0x9050,
	TFA2_BF_REQ = 0x9087,
	TFA2_BF_REQCMD = 0x9080,
	TFA2_BF_REQRST = 0x9090,
	TFA2_BF_REQMIPS = 0x90a0,
	TFA2_BF_REQMUTED = 0x90b0,
	TFA2_BF_REQVOL = 0x90c0,
	TFA2_BF_REQDMG = 0x90d0,
	TFA2_BF_REQCAL = 0x90e0,
	TFA2_BF_REQRSV = 0x90f0,
	TFA2_BF_MADD = 0x910f,
	TFA2_BF_MEMA = 0x920f,
	TFA2_BF_ERR = 0x9307,
	TFA2_BF_ACK = 0x9387,
	TFA2_BF_ACKCMD = 0x9380,
	TFA2_BF_ACKRST = 0x9390,
	TFA2_BF_ACKMIPS = 0x93a0,
	TFA2_BF_ACKMUTED = 0x93b0,
	TFA2_BF_ACKVOL = 0x93c0,
	TFA2_BF_ACKDMG = 0x93d0,
	TFA2_BF_ACKCAL = 0x93e0,
	TFA2_BF_ACKRSV = 0x93f0,
	TFA2_BF_MTPK = 0xa107,
	TFA2_BF_KEY1LOCKED = 0xa200,
	TFA2_BF_KEY2LOCKED = 0xa210,
	TFA2_BF_CIMTP = 0xa360,
	TFA2_BF_MTPRDMSB = 0xa50f,
	TFA2_BF_MTPRDLSB = 0xa60f,
	TFA2_BF_EXTTS = 0xb108,
	TFA2_BF_TROS = 0xb190,
	TFA2_BF_MTPOTC = 0xf000,
	TFA2_BF_MTPEX = 0xf010,
	TFA2_BF_DCMCCAPI = 0xf020,
	TFA2_BF_DCMCCSB = 0xf030,
	TFA2_BF_USERDEF = 0xf042,
	TFA2_BF_R25CL = 0xf40f,
	TFA2_BF_R25CR = 0xf50f,
};
#define TFA2_NAMETABLE                                   \
	static struct TfaBfName Tfa2DatasheetNames[] = { \
		{ 0x0, "PWDN" },                         \
		{ 0x10, "I2CR" },                        \
		{ 0x20, "CFE" },                         \
		{ 0x30, "AMPE" },                        \
		{ 0x40, "DCA" },                         \
		{ 0x50, "SBSL" },                        \
		{ 0x60, "AMPC" },                        \
		{ 0x71, "INTP" },                        \
		{ 0x91, "FSSSEL" },                      \
		{ 0xb0, "BYPOCP" },                      \
		{ 0xc0, "TSTOCP" },                      \
		{ 0x101, "AMPINSEL" },                   \
		{ 0x120, "MANSCONF" },                   \
		{ 0x130, "MANCOLD" },                    \
		{ 0x140, "MANAOOSC" },                   \
		{ 0x150, "MANROBOD" },                   \
		{ 0x160, "BODE" },                       \
		{ 0x170, "BODHYS" },                     \
		{ 0x181, "BODFILT" },                    \
		{ 0x1a1, "BODTHLVL" },                   \
		{ 0x1d0, "MUTETO" },                     \
		{ 0x1e0, "RCVNS" },                      \
		{ 0x1f0, "MANWDE" },                     \
		{ 0x203, "AUDFS" },                      \
		{ 0x240, "INPLEV" },                     \
		{ 0x255, "FRACTDEL" },                   \
		{ 0x2b0, "BYPHVBF" },                    \
		{ 0x2c0, "LDOBYP" },                     \
		{ 0x30f, "REV" },                        \
		{ 0x401, "REFCKEXT" },                   \
		{ 0x420, "REFCKSEL" },                   \
		{ 0x500, "SSLEFTE" },                    \
		{ 0x510, "SSRIGHTE" },                   \
		{ 0x520, "VSLEFTE" },                    \
		{ 0x530, "VSRIGHTE" },                   \
		{ 0x540, "CSLEFTE" },                    \
		{ 0x550, "CSRIGHTE" },                   \
		{ 0x560, "SSPDME" },                     \
		{ 0xd18, "STGAIN" },                     \
		{ 0xda0, "PDMSMUTE" },                   \
		{ 0xe06, "SWVSTEP" },                    \
		{ 0x1000, "VDDS" },                      \
		{ 0x1010, "PLLS" },                      \
		{ 0x1020, "OTDS" },                      \
		{ 0x1030, "OVDS" },                      \
		{ 0x1040, "UVDS" },                      \
		{ 0x1050, "CLKS" },                      \
		{ 0x1060, "MTPB" },                      \
		{ 0x1070, "NOCLK" },                     \
		{ 0x1080, "SPKS" },                      \
		{ 0x1090, "ACS" },                       \
		{ 0x10a0, "SWS" },                       \
		{ 0x10b0, "WDS" },                       \
		{ 0x10c0, "AMPS" },                      \
		{ 0x10d0, "AREFS" },                     \
		{ 0x10e0, "ADCCR" },                     \
		{ 0x10f0, "BODNOK" },                    \
		{ 0x1100, "DCIL" },                      \
		{ 0x1110, "DCDCA" },                     \
		{ 0x1120, "DCOCPOK" },                   \
		{ 0x1140, "DCHVBAT" },                   \
		{ 0x1150, "DCH114" },                    \
		{ 0x1160, "DCH107" },                    \
		{ 0x1170, "STMUTEB" },                   \
		{ 0x1180, "STMUTE" },                    \
		{ 0x1190, "TDMLUTER" },                  \
		{ 0x11a2, "TDMSTAT" },                   \
		{ 0x11d0, "TDMERR" },                    \
		{ 0x11e0, "HAPTIC" },                    \
		{ 0x1200, "OCPOAPL" },                   \
		{ 0x1210, "OCPOANL" },                   \
		{ 0x1220, "OCPOBPL" },                   \
		{ 0x1230, "OCPOBNL" },                   \
		{ 0x1240, "CLIPAHL" },                   \
		{ 0x1250, "CLIPALL" },                   \
		{ 0x1260, "CLIPBHL" },                   \
		{ 0x1270, "CLIPBLL" },                   \
		{ 0x1280, "OCPOAPRC" },                  \
		{ 0x1290, "OCPOANRC" },                  \
		{ 0x12a0, "OCPOBPRC" },                  \
		{ 0x12b0, "OCPOBNRC" },                  \
		{ 0x12c0, "RCVLDOR" },                   \
		{ 0x12d0, "RCVLDOBR" },                  \
		{ 0x12e0, "OCDSL" },                     \
		{ 0x12f0, "CLIPSL" },                    \
		{ 0x1300, "OCPOAPR" },                   \
		{ 0x1310, "OCPOANR" },                   \
		{ 0x1320, "OCPOBPR" },                   \
		{ 0x1330, "OCPOBNR" },                   \
		{ 0x1340, "CLIPAHR" },                   \
		{ 0x1350, "CLIPALR" },                   \
		{ 0x1360, "CLIPBHR" },                   \
		{ 0x1370, "CLIPBLR" },                   \
		{ 0x1380, "OCDSR" },                     \
		{ 0x1390, "CLIPSR" },                    \
		{ 0x13a0, "OCPOKMC" },                   \
		{ 0x13b0, "MANALARM" },                  \
		{ 0x13c0, "MANWAIT1" },                  \
		{ 0x13d0, "MANWAIT2" },                  \
		{ 0x13e0, "MANMUTE" },                   \
		{ 0x13f0, "MANOPER" },                   \
		{ 0x1400, "SPKSL" },                     \
		{ 0x1410, "SPKSR" },                     \
		{ 0x1420, "CLKOOR" },                    \
		{ 0x1433, "MANSTATE" },                  \
		{ 0x1509, "BATS" },                      \
		{ 0x1608, "TEMPS" },                     \
		{ 0x2003, "TDMUC" },                     \
		{ 0x2040, "TDME" },                      \
		{ 0x2050, "TDMMODE" },                   \
		{ 0x2060, "TDMCLINV" },                  \
		{ 0x2073, "TDMFSLN" },                   \
		{ 0x20b0, "TDMFSPOL" },                  \
		{ 0x20c3, "TDMNBCK" },                   \
		{ 0x2103, "TDMSLOTS" },                  \
		{ 0x2144, "TDMSLLN" },                   \
		{ 0x2194, "TDMBRMG" },                   \
		{ 0x21e0, "TDMDEL" },                    \
		{ 0x21f0, "TDMADJ" },                    \
		{ 0x2201, "TDMOOMP" },                   \
		{ 0x2224, "TDMSSIZE" },                  \
		{ 0x2271, "TDMTXDFO" },                  \
		{ 0x2291, "TDMTXUS0" },                  \
		{ 0x22b1, "TDMTXUS1" },                  \
		{ 0x22d1, "TDMTXUS2" },                  \
		{ 0x2310, "TDMLE" },                     \
		{ 0x2320, "TDMRE" },                     \
		{ 0x2340, "TDMVSRE" },                   \
		{ 0x2350, "TDMCSRE" },                   \
		{ 0x2360, "TDMVSLE" },                   \
		{ 0x2370, "TDMCSLE" },                   \
		{ 0x2380, "TDMCFRE" },                   \
		{ 0x2390, "TDMCFLE" },                   \
		{ 0x23a0, "TDMCF3E" },                   \
		{ 0x23b0, "TDMCF4E" },                   \
		{ 0x23c0, "TDMPD1E" },                   \
		{ 0x23d0, "TDMPD2E" },                   \
		{ 0x2421, "TDMLIO" },                    \
		{ 0x2441, "TDMRIO" },                    \
		{ 0x2481, "TDMVSRIO" },                  \
		{ 0x24a1, "TDMCSRIO" },                  \
		{ 0x24c1, "TDMVSLIO" },                  \
		{ 0x24e1, "TDMCSLIO" },                  \
		{ 0x2501, "TDMCFRIO" },                  \
		{ 0x2521, "TDMCFLIO" },                  \
		{ 0x2541, "TDMCF3IO" },                  \
		{ 0x2561, "TDMCF4IO" },                  \
		{ 0x2581, "TDMPD1IO" },                  \
		{ 0x25a1, "TDMPD2IO" },                  \
		{ 0x2643, "TDMLS" },                     \
		{ 0x2683, "TDMRS" },                     \
		{ 0x2703, "TDMVSRS" },                   \
		{ 0x2743, "TDMCSRS" },                   \
		{ 0x2783, "TDMVSLS" },                   \
		{ 0x27c3, "TDMCSLS" },                   \
		{ 0x2803, "TDMCFRS" },                   \
		{ 0x2843, "TDMCFLS" },                   \
		{ 0x2883, "TDMCF3S" },                   \
		{ 0x28c3, "TDMCF4S" },                   \
		{ 0x2903, "TDMPD1S" },                   \
		{ 0x2943, "TDMPD2S" },                   \
		{ 0x3100, "PDMSM" },                     \
		{ 0x3111, "PDMSTSEL" },                  \
		{ 0x3130, "PDMLSEL" },                   \
		{ 0x3140, "PDMRSEL" },                   \
		{ 0x3150, "MICVDDE" },                   \
		{ 0x3201, "PDMCLRAT" },                  \
		{ 0x3223, "PDMGAIN" },                   \
		{ 0x3263, "PDMOSEL" },                   \
		{ 0x32a0, "SELCFHAPD" },                 \
		{ 0x3307, "HAPTIME" },                   \
		{ 0x3387, "HAPLEVEL" },                  \
		{ 0x3403, "GPIODIN" },                   \
		{ 0x3500, "GPIOCTRL" },                  \
		{ 0x3513, "GPIOCONF" },                  \
		{ 0x3553, "GPIODOUT" },                  \
		{ 0x4000, "ISTVDDS" },                   \
		{ 0x4010, "ISTPLLS" },                   \
		{ 0x4020, "ISTOTDS" },                   \
		{ 0x4030, "ISTOVDS" },                   \
		{ 0x4040, "ISTUVDS" },                   \
		{ 0x4050, "ISTCLKS" },                   \
		{ 0x4060, "ISTMTPB" },                   \
		{ 0x4070, "ISTNOCLK" },                  \
		{ 0x4080, "ISTSPKS" },                   \
		{ 0x4090, "ISTACS" },                    \
		{ 0x40a0, "ISTSWS" },                    \
		{ 0x40b0, "ISTWDS" },                    \
		{ 0x40c0, "ISTAMPS" },                   \
		{ 0x40d0, "ISTAREFS" },                  \
		{ 0x40e0, "ISTADCCR" },                  \
		{ 0x40f0, "ISTBODNOK" },                 \
		{ 0x4100, "ISTBSTCU" },                  \
		{ 0x4110, "ISTBSTHI" },                  \
		{ 0x4120, "ISTBSTOC" },                  \
		{ 0x4130, "ISTBSTPKCUR" },               \
		{ 0x4140, "ISTBSTVC" },                  \
		{ 0x4150, "ISTBST86" },                  \
		{ 0x4160, "ISTBST93" },                  \
		{ 0x4170, "ISTRCVLD" },                  \
		{ 0x4180, "ISTOCPL" },                   \
		{ 0x4190, "ISTOCPR" },                   \
		{ 0x41a0, "ISTMWSRC" },                  \
		{ 0x41b0, "ISTMWCFC" },                  \
		{ 0x41c0, "ISTMWSMU" },                  \
		{ 0x41d0, "ISTCFMER" },                  \
		{ 0x41e0, "ISTCFMAC" },                  \
		{ 0x41f0, "ISTCLKOOR" },                 \
		{ 0x4200, "ISTTDMER" },                  \
		{ 0x4210, "ISTCLPL" },                   \
		{ 0x4220, "ISTCLPR" },                   \
		{ 0x4230, "ISTOCPM" },                   \
		{ 0x4400, "ICLVDDS" },                   \
		{ 0x4410, "ICLPLLS" },                   \
		{ 0x4420, "ICLOTDS" },                   \
		{ 0x4430, "ICLOVDS" },                   \
		{ 0x4440, "ICLUVDS" },                   \
		{ 0x4450, "ICLCLKS" },                   \
		{ 0x4460, "ICLMTPB" },                   \
		{ 0x4470, "ICLNOCLK" },                  \
		{ 0x4480, "ICLSPKS" },                   \
		{ 0x4490, "ICLACS" },                    \
		{ 0x44a0, "ICLSWS" },                    \
		{ 0x44b0, "ICLWDS" },                    \
		{ 0x44c0, "ICLAMPS" },                   \
		{ 0x44d0, "ICLAREFS" },                  \
		{ 0x44e0, "ICLADCCR" },                  \
		{ 0x44f0, "ICLBODNOK" },                 \
		{ 0x4500, "ICLBSTCU" },                  \
		{ 0x4510, "ICLBSTHI" },                  \
		{ 0x4520, "ICLBSTOC" },                  \
		{ 0x4530, "ICLBSTPC" },                  \
		{ 0x4540, "ICLBSTVC" },                  \
		{ 0x4550, "ICLBST86" },                  \
		{ 0x4560, "ICLBST93" },                  \
		{ 0x4570, "ICLRCVLD" },                  \
		{ 0x4580, "ICLOCPL" },                   \
		{ 0x4590, "ICLOCPR" },                   \
		{ 0x45a0, "ICLMWSRC" },                  \
		{ 0x45b0, "ICLMWCFC" },                  \
		{ 0x45c0, "ICLMWSMU" },                  \
		{ 0x45d0, "ICLCFMER" },                  \
		{ 0x45e0, "ICLCFMAC" },                  \
		{ 0x45f0, "ICLCLKOOR" },                 \
		{ 0x4600, "ICLTDMER" },                  \
		{ 0x4610, "ICLCLPL" },                   \
		{ 0x4620, "ICLCLPR" },                   \
		{ 0x4630, "ICLOCPM" },                   \
		{ 0x4800, "IEVDDS" },                    \
		{ 0x4810, "IEPLLS" },                    \
		{ 0x4820, "IEOTDS" },                    \
		{ 0x4830, "IEOVDS" },                    \
		{ 0x4840, "IEUVDS" },                    \
		{ 0x4850, "IECLKS" },                    \
		{ 0x4860, "IEMTPB" },                    \
		{ 0x4870, "IENOCLK" },                   \
		{ 0x4880, "IESPKS" },                    \
		{ 0x4890, "IEACS" },                     \
		{ 0x48a0, "IESWS" },                     \
		{ 0x48b0, "IEWDS" },                     \
		{ 0x48c0, "IEAMPS" },                    \
		{ 0x48d0, "IEAREFS" },                   \
		{ 0x48e0, "IEADCCR" },                   \
		{ 0x48f0, "IEBODNOK" },                  \
		{ 0x4900, "IEBSTCU" },                   \
		{ 0x4910, "IEBSTHI" },                   \
		{ 0x4920, "IEBSTOC" },                   \
		{ 0x4930, "IEBSTPC" },                   \
		{ 0x4940, "IEBSTVC" },                   \
		{ 0x4950, "IEBST86" },                   \
		{ 0x4960, "IEBST93" },                   \
		{ 0x4970, "IERCVLD" },                   \
		{ 0x4980, "IEOCPL" },                    \
		{ 0x4990, "IEOCPR" },                    \
		{ 0x49a0, "IEMWSRC" },                   \
		{ 0x49b0, "IEMWCFC" },                   \
		{ 0x49c0, "IEMWSMU" },                   \
		{ 0x49d0, "IECFMER" },                   \
		{ 0x49e0, "IECFMAC" },                   \
		{ 0x49f0, "IECLKOOR" },                  \
		{ 0x4a00, "IETDMER" },                   \
		{ 0x4a10, "IECLPL" },                    \
		{ 0x4a20, "IECLPR" },                    \
		{ 0x4a30, "IEOCPM1" },                   \
		{ 0x4c00, "IPOVDDS" },                   \
		{ 0x4c10, "IPOPLLS" },                   \
		{ 0x4c20, "IPOOTDS" },                   \
		{ 0x4c30, "IPOOVDS" },                   \
		{ 0x4c40, "IPOUVDS" },                   \
		{ 0x4c50, "IPOCLKS" },                   \
		{ 0x4c60, "IPOMTPB" },                   \
		{ 0x4c70, "IPONOCLK" },                  \
		{ 0x4c80, "IPOSPKS" },                   \
		{ 0x4c90, "IPOACS" },                    \
		{ 0x4ca0, "IPOSWS" },                    \
		{ 0x4cb0, "IPOWDS" },                    \
		{ 0x4cc0, "IPOAMPS" },                   \
		{ 0x4cd0, "IPOAREFS" },                  \
		{ 0x4ce0, "IPOADCCR" },                  \
		{ 0x4cf0, "IPOBODNOK" },                 \
		{ 0x4d00, "IPOBSTCU" },                  \
		{ 0x4d10, "IPOBSTHI" },                  \
		{ 0x4d20, "IPOBSTOC" },                  \
		{ 0x4d30, "IPOBSTPC" },                  \
		{ 0x4d40, "IPOBSTVC" },                  \
		{ 0x4d50, "IPOBST86" },                  \
		{ 0x4d60, "IPOBST93" },                  \
		{ 0x4d70, "IPORCVLD" },                  \
		{ 0x4d80, "IPOOCPL" },                   \
		{ 0x4d90, "IPOOCPR" },                   \
		{ 0x4da0, "IPOMWSRC" },                  \
		{ 0x4db0, "IPOMWCFC" },                  \
		{ 0x4dc0, "IPOMWSMU" },                  \
		{ 0x4dd0, "IPOCFMER" },                  \
		{ 0x4de0, "IPOCFMAC" },                  \
		{ 0x4df0, "IPCLKOOR" },                  \
		{ 0x4e00, "IPOTDMER" },                  \
		{ 0x4e10, "IPOCLPL" },                   \
		{ 0x4e20, "IPOCLPR" },                   \
		{ 0x4e30, "IPOOCPM" },                   \
		{ 0x5001, "BSSCR" },                     \
		{ 0x5023, "BSST" },                      \
		{ 0x5061, "BSSRL" },                     \
		{ 0x5082, "BSSRR" },                     \
		{ 0x50b1, "BSSHY" },                     \
		{ 0x50e0, "BSSR" },                      \
		{ 0x50f0, "BSSBY" },                     \
		{ 0x5100, "BSSS" },                      \
		{ 0x5110, "INTSMUTE" },                  \
		{ 0x5120, "CFSML" },                     \
		{ 0x5130, "CFSMR" },                     \
		{ 0x5140, "HPFBYPL" },                   \
		{ 0x5150, "HPFBYPR" },                   \
		{ 0x5160, "DPSAL" },                     \
		{ 0x5170, "DPSAR" },                     \
		{ 0x5187, "VOL" },                       \
		{ 0x5200, "HNDSFRCV" },                  \
		{ 0x5222, "CLIPCTRL" },                  \
		{ 0x5257, "AMPGAIN" },                   \
		{ 0x52d0, "SLOPEE" },                    \
		{ 0x52e1, "SLOPESET" },                  \
		{ 0x5a07, "VOLSEC" },                    \
		{ 0x5a87, "SWPROFIL" },                  \
		{ 0x7002, "DCVO" },                      \
		{ 0x7033, "DCMCC" },                     \
		{ 0x7071, "DCCV" },                      \
		{ 0x7090, "DCIE" },                      \
		{ 0x70a0, "DCSR" },                      \
		{ 0x70b2, "DCSYNCP" },                   \
		{ 0x70e0, "DCDIS" },                     \
		{ 0x9000, "RST" },                       \
		{ 0x9011, "DMEM" },                      \
		{ 0x9030, "AIF" },                       \
		{ 0x9040, "CFINT" },                     \
		{ 0x9050, "CFCGATE" },                   \
		{ 0x9080, "REQCMD" },                    \
		{ 0x9090, "REQRST" },                    \
		{ 0x90a0, "REQMIPS" },                   \
		{ 0x90b0, "REQMUTED" },                  \
		{ 0x90c0, "REQVOL" },                    \
		{ 0x90d0, "REQDMG" },                    \
		{ 0x90e0, "REQCAL" },                    \
		{ 0x90f0, "REQRSV" },                    \
		{ 0x910f, "MADD" },                      \
		{ 0x920f, "MEMA" },                      \
		{ 0x9307, "ERR" },                       \
		{ 0x9387, "ACK" },                       \
		{ 0x9380, "ACKCMD" },                    \
		{ 0x9390, "ACKRST" },                    \
		{ 0x93a0, "ACKMIPS" },                   \
		{ 0x93b0, "ACKMUTED" },                  \
		{ 0x93c0, "ACKVOL" },                    \
		{ 0x93d0, "ACKDMG" },                    \
		{ 0x93e0, "ACKCAL" },                    \
		{ 0x93f0, "ACKRSV" },                    \
		{ 0xa107, "MTPK" },                      \
		{ 0xa200, "KEY1LOCKED" },                \
		{ 0xa210, "KEY2LOCKED" },                \
		{ 0xa360, "CIMTP" },                     \
		{ 0xa50f, "MTPRDMSB" },                  \
		{ 0xa60f, "MTPRDLSB" },                  \
		{ 0xb108, "EXTTS" },                     \
		{ 0xb190, "TROS" },                      \
		{ 0xf000, "MTPOTC" },                    \
		{ 0xf010, "MTPEX" },                     \
		{ 0xf020, "DCMCCAPI" },                  \
		{ 0xf030, "DCMCCSB" },                   \
		{ 0xf042, "USERDEF" },                   \
		{ 0xf40f, "R25CL" },                     \
		{ 0xf50f, "R25CR" },                     \
		{ 0xffff, "Unknown bitfield enum" }      \
	}
#define TFA2_BITNAMETABLE                                              \
	static struct TfaBfName Tfa2BitNames[] = {                     \
		{ 0x0, "powerdown" },                                  \
		{ 0x10, "reset" },                                     \
		{ 0x20, "enbl_coolflux" },                             \
		{ 0x30, "enbl_amplifier" },                            \
		{ 0x40, "enbl_boost" },                                \
		{ 0x50, "coolflux_configured" },                       \
		{ 0x60, "sel_enbl_amplifier" },                        \
		{ 0x71, "int_pad_io" },                                \
		{ 0x91, "fs_pulse_sel" },                              \
		{ 0xb0, "bypass_ocp" },                                \
		{ 0xc0, "test_ocp" },                                  \
		{ 0x101, "vamp_sel" },                                 \
		{ 0x120, "src_set_configured" },                       \
		{ 0x130, "execute_cold_start" },                       \
		{ 0x140, "enbl_osc1m_auto_off" },                      \
		{ 0x150, "man_enbl_brown_out" },                       \
		{ 0x160, "enbl_bod" },                                 \
		{ 0x170, "enbl_bod_hyst" },                            \
		{ 0x181, "bod_delay" },                                \
		{ 0x1a1, "bod_lvlsel" },                               \
		{ 0x1d0, "disable_mute_time_out" },                    \
		{ 0x1e0, "pwm_sel_rcv_ns" },                           \
		{ 0x1f0, "man_enbl_watchdog" },                        \
		{ 0x203, "audio_fs" },                                 \
		{ 0x240, "input_level" },                              \
		{ 0x255, "cs_frac_delay" },                            \
		{ 0x2b0, "bypass_hvbat_filter" },                      \
		{ 0x2c0, "ctrl_rcvldop_bypass" },                      \
		{ 0x30f, "device_rev" },                               \
		{ 0x401, "pll_clkin_sel" },                            \
		{ 0x420, "pll_clkin_sel_osc" },                        \
		{ 0x500, "enbl_spkr_ss_left" },                        \
		{ 0x510, "enbl_spkr_ss_right" },                       \
		{ 0x520, "enbl_volsense_left" },                       \
		{ 0x530, "enbl_volsense_right" },                      \
		{ 0x540, "enbl_cursense_left" },                       \
		{ 0x550, "enbl_cursense_right" },                      \
		{ 0x560, "enbl_pdm_ss" },                              \
		{ 0xd00, "side_tone_gain_sel" },                       \
		{ 0xd18, "side_tone_gain" },                           \
		{ 0xda0, "mute_side_tone" },                           \
		{ 0xe06, "ctrl_digtoana" },                            \
		{ 0xe70, "enbl_cmfb_left" },                           \
		{ 0xf0f, "hidden_code" },                              \
		{ 0x1000, "flag_por" },                                \
		{ 0x1010, "flag_pll_lock" },                           \
		{ 0x1020, "flag_otpok" },                              \
		{ 0x1030, "flag_ovpok" },                              \
		{ 0x1040, "flag_uvpok" },                              \
		{ 0x1050, "flag_clocks_stable" },                      \
		{ 0x1060, "flag_mtp_busy" },                           \
		{ 0x1070, "flag_lost_clk" },                           \
		{ 0x1080, "flag_cf_speakererror" },                    \
		{ 0x1090, "flag_cold_started" },                       \
		{ 0x10a0, "flag_engage" },                             \
		{ 0x10b0, "flag_watchdog_reset" },                     \
		{ 0x10c0, "flag_enbl_amp" },                           \
		{ 0x10d0, "flag_enbl_ref" },                           \
		{ 0x10e0, "flag_adc10_ready" },                        \
		{ 0x10f0, "flag_bod_vddd_nok" },                       \
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
		{ 0x11e0, "flag_haptic_busy" },                        \
		{ 0x1200, "flag_ocpokap_left" },                       \
		{ 0x1210, "flag_ocpokan_left" },                       \
		{ 0x1220, "flag_ocpokbp_left" },                       \
		{ 0x1230, "flag_ocpokbn_left" },                       \
		{ 0x1240, "flag_clipa_high_left" },                    \
		{ 0x1250, "flag_clipa_low_left" },                     \
		{ 0x1260, "flag_clipb_high_left" },                    \
		{ 0x1270, "flag_clipb_low_left" },                     \
		{ 0x1280, "flag_ocpokap_rcv" },                        \
		{ 0x1290, "flag_ocpokan_rcv" },                        \
		{ 0x12a0, "flag_ocpokbp_rcv" },                        \
		{ 0x12b0, "flag_ocpokbn_rcv" },                        \
		{ 0x12c0, "flag_rcvldop_ready" },                      \
		{ 0x12d0, "flag_rcvldop_bypassready" },                \
		{ 0x12e0, "flag_ocp_alarm_left" },                     \
		{ 0x12f0, "flag_clip_left" },                          \
		{ 0x1300, "flag_ocpokap_right" },                      \
		{ 0x1310, "flag_ocpokan_right" },                      \
		{ 0x1320, "flag_ocpokbp_right" },                      \
		{ 0x1330, "flag_ocpokbn_right" },                      \
		{ 0x1340, "flag_clipa_high_right" },                   \
		{ 0x1350, "flag_clipa_low_right" },                    \
		{ 0x1360, "flag_clipb_high_right" },                   \
		{ 0x1370, "flag_clipb_low_right" },                    \
		{ 0x1380, "flag_ocp_alarm_right" },                    \
		{ 0x1390, "flag_clip_right" },                         \
		{ 0x13a0, "flag_mic_ocpok" },                          \
		{ 0x13b0, "flag_man_alarm_state" },                    \
		{ 0x13c0, "flag_man_wait_src_settings" },              \
		{ 0x13d0, "flag_man_wait_cf_config" },                 \
		{ 0x13e0, "flag_man_start_mute_audio" },               \
		{ 0x13f0, "flag_man_operating_state" },                \
		{ 0x1400, "flag_cf_speakererror_left" },               \
		{ 0x1410, "flag_cf_speakererror_right" },              \
		{ 0x1420, "flag_clk_out_of_range" },                   \
		{ 0x1433, "man_state" },                               \
		{ 0x1509, "bat_adc" },                                 \
		{ 0x1608, "temp_adc" },                                \
		{ 0x2003, "tdm_usecase" },                             \
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
		{ 0x22b1, "tdm_txdata_format_unused_slot_sd1" },       \
		{ 0x22d1, "tdm_txdata_format_unused_slot_sd2" },       \
		{ 0x2300, "tdm_sink0_enable" },                        \
		{ 0x2310, "tdm_sink1_enable" },                        \
		{ 0x2320, "tdm_sink2_enable" },                        \
		{ 0x2330, "tdm_source0_enable" },                      \
		{ 0x2340, "tdm_source1_enable" },                      \
		{ 0x2350, "tdm_source2_enable" },                      \
		{ 0x2360, "tdm_source3_enable" },                      \
		{ 0x2370, "tdm_source4_enable" },                      \
		{ 0x2380, "tdm_source5_enable" },                      \
		{ 0x2390, "tdm_source6_enable" },                      \
		{ 0x23a0, "tdm_source7_enable" },                      \
		{ 0x23b0, "tdm_source8_enable" },                      \
		{ 0x23c0, "tdm_source9_enable" },                      \
		{ 0x23d0, "tdm_source10_enable" },                     \
		{ 0x2401, "tdm_sink0_io" },                            \
		{ 0x2421, "tdm_sink1_io" },                            \
		{ 0x2441, "tdm_sink2_io" },                            \
		{ 0x2461, "tdm_source0_io" },                          \
		{ 0x2481, "tdm_source1_io" },                          \
		{ 0x24a1, "tdm_source2_io" },                          \
		{ 0x24c1, "tdm_source3_io" },                          \
		{ 0x24e1, "tdm_source4_io" },                          \
		{ 0x2501, "tdm_source5_io" },                          \
		{ 0x2521, "tdm_source6_io" },                          \
		{ 0x2541, "tdm_source7_io" },                          \
		{ 0x2561, "tdm_source8_io" },                          \
		{ 0x2581, "tdm_source9_io" },                          \
		{ 0x25a1, "tdm_source10_io" },                         \
		{ 0x2603, "tdm_sink0_slot" },                          \
		{ 0x2643, "tdm_sink1_slot" },                          \
		{ 0x2683, "tdm_sink2_slot" },                          \
		{ 0x26c3, "tdm_source0_slot" },                        \
		{ 0x2703, "tdm_source1_slot" },                        \
		{ 0x2743, "tdm_source2_slot" },                        \
		{ 0x2783, "tdm_source3_slot" },                        \
		{ 0x27c3, "tdm_source4_slot" },                        \
		{ 0x2803, "tdm_source5_slot" },                        \
		{ 0x2843, "tdm_source6_slot" },                        \
		{ 0x2883, "tdm_source7_slot" },                        \
		{ 0x28c3, "tdm_source8_slot" },                        \
		{ 0x2903, "tdm_source9_slot" },                        \
		{ 0x2943, "tdm_source10_slot" },                       \
		{ 0x3100, "pdm_mode" },                                \
		{ 0x3111, "pdm_side_tone_sel" },                       \
		{ 0x3130, "pdm_left_sel" },                            \
		{ 0x3140, "pdm_right_sel" },                           \
		{ 0x3150, "enbl_micvdd" },                             \
		{ 0x3160, "bypass_micvdd_ocp" },                       \
		{ 0x3201, "pdm_nbck" },                                \
		{ 0x3223, "pdm_gain" },                                \
		{ 0x3263, "sel_pdm_out_data" },                        \
		{ 0x32a0, "sel_cf_haptic_data" },                      \
		{ 0x3307, "haptic_duration" },                         \
		{ 0x3387, "haptic_data" },                             \
		{ 0x3403, "gpio_datain" },                             \
		{ 0x3500, "gpio_ctrl" },                               \
		{ 0x3513, "gpio_dir" },                                \
		{ 0x3553, "gpio_dataout" },                            \
		{ 0x4000, "int_out_flag_por" },                        \
		{ 0x4010, "int_out_flag_pll_lock" },                   \
		{ 0x4020, "int_out_flag_otpok" },                      \
		{ 0x4030, "int_out_flag_ovpok" },                      \
		{ 0x4040, "int_out_flag_uvpok" },                      \
		{ 0x4050, "int_out_flag_clocks_stable" },              \
		{ 0x4060, "int_out_flag_mtp_busy" },                   \
		{ 0x4070, "int_out_flag_lost_clk" },                   \
		{ 0x4080, "int_out_flag_cf_speakererror" },            \
		{ 0x4090, "int_out_flag_cold_started" },               \
		{ 0x40a0, "int_out_flag_engage" },                     \
		{ 0x40b0, "int_out_flag_watchdog_reset" },             \
		{ 0x40c0, "int_out_flag_enbl_amp" },                   \
		{ 0x40d0, "int_out_flag_enbl_ref" },                   \
		{ 0x40e0, "int_out_flag_adc10_ready" },                \
		{ 0x40f0, "int_out_flag_bod_vddd_nok" },               \
		{ 0x4100, "int_out_flag_bst_bstcur" },                 \
		{ 0x4110, "int_out_flag_bst_hiz" },                    \
		{ 0x4120, "int_out_flag_bst_ocpok" },                  \
		{ 0x4130, "int_out_flag_bst_peakcur" },                \
		{ 0x4140, "int_out_flag_bst_voutcomp" },               \
		{ 0x4150, "int_out_flag_bst_voutcomp86" },             \
		{ 0x4160, "int_out_flag_bst_voutcomp93" },             \
		{ 0x4170, "int_out_flag_rcvldop_ready" },              \
		{ 0x4180, "int_out_flag_ocp_alarm_left" },             \
		{ 0x4190, "int_out_flag_ocp_alarm_right" },            \
		{ 0x41a0, "int_out_flag_man_wait_src_settings" },      \
		{ 0x41b0, "int_out_flag_man_wait_cf_config" },         \
		{ 0x41c0, "int_out_flag_man_start_mute_audio" },       \
		{ 0x41d0, "int_out_flag_cfma_err" },                   \
		{ 0x41e0, "int_out_flag_cfma_ack" },                   \
		{ 0x41f0, "int_out_flag_clk_out_of_range" },           \
		{ 0x4200, "int_out_flag_tdm_error" },                  \
		{ 0x4210, "int_out_flag_clip_left" },                  \
		{ 0x4220, "int_out_flag_clip_right" },                 \
		{ 0x4230, "int_out_flag_mic_ocpok" },                  \
		{ 0x4400, "int_in_flag_por" },                         \
		{ 0x4410, "int_in_flag_pll_lock" },                    \
		{ 0x4420, "int_in_flag_otpok" },                       \
		{ 0x4430, "int_in_flag_ovpok" },                       \
		{ 0x4440, "int_in_flag_uvpok" },                       \
		{ 0x4450, "int_in_flag_clocks_stable" },               \
		{ 0x4460, "int_in_flag_mtp_busy" },                    \
		{ 0x4470, "int_in_flag_lost_clk" },                    \
		{ 0x4480, "int_in_flag_cf_speakererror" },             \
		{ 0x4490, "int_in_flag_cold_started" },                \
		{ 0x44a0, "int_in_flag_engage" },                      \
		{ 0x44b0, "int_in_flag_watchdog_reset" },              \
		{ 0x44c0, "int_in_flag_enbl_amp" },                    \
		{ 0x44d0, "int_in_flag_enbl_ref" },                    \
		{ 0x44e0, "int_in_flag_adc10_ready" },                 \
		{ 0x44f0, "int_in_flag_bod_vddd_nok" },                \
		{ 0x4500, "int_in_flag_bst_bstcur" },                  \
		{ 0x4510, "int_in_flag_bst_hiz" },                     \
		{ 0x4520, "int_in_flag_bst_ocpok" },                   \
		{ 0x4530, "int_in_flag_bst_peakcur" },                 \
		{ 0x4540, "int_in_flag_bst_voutcomp" },                \
		{ 0x4550, "int_in_flag_bst_voutcomp86" },              \
		{ 0x4560, "int_in_flag_bst_voutcomp93" },              \
		{ 0x4570, "int_in_flag_rcvldop_ready" },               \
		{ 0x4580, "int_in_flag_ocp_alarm_left" },              \
		{ 0x4590, "int_in_flag_ocp_alarm_right" },             \
		{ 0x45a0, "int_in_flag_man_wait_src_settings" },       \
		{ 0x45b0, "int_in_flag_man_wait_cf_config" },          \
		{ 0x45c0, "int_in_flag_man_start_mute_audio" },        \
		{ 0x45d0, "int_in_flag_cfma_err" },                    \
		{ 0x45e0, "int_in_flag_cfma_ack" },                    \
		{ 0x45f0, "int_in_flag_clk_out_of_range" },            \
		{ 0x4600, "int_in_flag_tdm_error" },                   \
		{ 0x4610, "int_in_flag_clip_left" },                   \
		{ 0x4620, "int_in_flag_clip_right" },                  \
		{ 0x4630, "int_in_flag_mic_ocpok" },                   \
		{ 0x4800, "int_enable_flag_por" },                     \
		{ 0x4810, "int_enable_flag_pll_lock" },                \
		{ 0x4820, "int_enable_flag_otpok" },                   \
		{ 0x4830, "int_enable_flag_ovpok" },                   \
		{ 0x4840, "int_enable_flag_uvpok" },                   \
		{ 0x4850, "int_enable_flag_clocks_stable" },           \
		{ 0x4860, "int_enable_flag_mtp_busy" },                \
		{ 0x4870, "int_enable_flag_lost_clk" },                \
		{ 0x4880, "int_enable_flag_cf_speakererror" },         \
		{ 0x4890, "int_enable_flag_cold_started" },            \
		{ 0x48a0, "int_enable_flag_engage" },                  \
		{ 0x48b0, "int_enable_flag_watchdog_reset" },          \
		{ 0x48c0, "int_enable_flag_enbl_amp" },                \
		{ 0x48d0, "int_enable_flag_enbl_ref" },                \
		{ 0x48e0, "int_enable_flag_adc10_ready" },             \
		{ 0x48f0, "int_enable_flag_bod_vddd_nok" },            \
		{ 0x4900, "int_enable_flag_bst_bstcur" },              \
		{ 0x4910, "int_enable_flag_bst_hiz" },                 \
		{ 0x4920, "int_enable_flag_bst_ocpok" },               \
		{ 0x4930, "int_enable_flag_bst_peakcur" },             \
		{ 0x4940, "int_enable_flag_bst_voutcomp" },            \
		{ 0x4950, "int_enable_flag_bst_voutcomp86" },          \
		{ 0x4960, "int_enable_flag_bst_voutcomp93" },          \
		{ 0x4970, "int_enable_flag_rcvldop_ready" },           \
		{ 0x4980, "int_enable_flag_ocp_alarm_left" },          \
		{ 0x4990, "int_enable_flag_ocp_alarm_right" },         \
		{ 0x49a0, "int_enable_flag_man_wait_src_settings" },   \
		{ 0x49b0, "int_enable_flag_man_wait_cf_config" },      \
		{ 0x49c0, "int_enable_flag_man_start_mute_audio" },    \
		{ 0x49d0, "int_enable_flag_cfma_err" },                \
		{ 0x49e0, "int_enable_flag_cfma_ack" },                \
		{ 0x49f0, "int_enable_flag_clk_out_of_range" },        \
		{ 0x4a00, "int_enable_flag_tdm_error" },               \
		{ 0x4a10, "int_enable_flag_clip_left" },               \
		{ 0x4a20, "int_enable_flag_clip_right" },              \
		{ 0x4a30, "int_enable_flag_mic_ocpok" },               \
		{ 0x4c00, "int_polarity_flag_por" },                   \
		{ 0x4c10, "int_polarity_flag_pll_lock" },              \
		{ 0x4c20, "int_polarity_flag_otpok" },                 \
		{ 0x4c30, "int_polarity_flag_ovpok" },                 \
		{ 0x4c40, "int_polarity_flag_uvpok" },                 \
		{ 0x4c50, "int_polarity_flag_clocks_stable" },         \
		{ 0x4c60, "int_polarity_flag_mtp_busy" },              \
		{ 0x4c70, "int_polarity_flag_lost_clk" },              \
		{ 0x4c80, "int_polarity_flag_cf_speakererror" },       \
		{ 0x4c90, "int_polarity_flag_cold_started" },          \
		{ 0x4ca0, "int_polarity_flag_engage" },                \
		{ 0x4cb0, "int_polarity_flag_watchdog_reset" },        \
		{ 0x4cc0, "int_polarity_flag_enbl_amp" },              \
		{ 0x4cd0, "int_polarity_flag_enbl_ref" },              \
		{ 0x4ce0, "int_polarity_flag_adc10_ready" },           \
		{ 0x4cf0, "int_polarity_flag_bod_vddd_nok" },          \
		{ 0x4d00, "int_polarity_flag_bst_bstcur" },            \
		{ 0x4d10, "int_polarity_flag_bst_hiz" },               \
		{ 0x4d20, "int_polarity_flag_bst_ocpok" },             \
		{ 0x4d30, "int_polarity_flag_bst_peakcur" },           \
		{ 0x4d40, "int_polarity_flag_bst_voutcomp" },          \
		{ 0x4d50, "int_polarity_flag_bst_voutcomp86" },        \
		{ 0x4d60, "int_polarity_flag_bst_voutcomp93" },        \
		{ 0x4d70, "int_polarity_flag_rcvldop_ready" },         \
		{ 0x4d80, "int_polarity_flag_ocp_alarm_left" },        \
		{ 0x4d90, "int_polarity_flag_ocp_alarm_right" },       \
		{ 0x4da0, "int_polarity_flag_man_wait_src_settings" }, \
		{ 0x4db0, "int_polarity_flag_man_wait_cf_config" },    \
		{ 0x4dc0, "int_polarity_flag_man_start_mute_audio" },  \
		{ 0x4dd0, "int_polarity_flag_cfma_err" },              \
		{ 0x4de0, "int_polarity_flag_cfma_ack" },              \
		{ 0x4df0, "int_polarity_flag_clk_out_of_range" },      \
		{ 0x4e00, "int_polarity_flag_tdm_error" },             \
		{ 0x4e10, "int_polarity_flag_clip_left" },             \
		{ 0x4e20, "int_polarity_flag_clip_right" },            \
		{ 0x4e30, "int_polarity_flag_mic_ocpok" },             \
		{ 0x5001, "vbat_prot_attack_time" },                   \
		{ 0x5023, "vbat_prot_thlevel" },                       \
		{ 0x5061, "vbat_prot_max_reduct" },                    \
		{ 0x5082, "vbat_prot_release_time" },                  \
		{ 0x50b1, "vbat_prot_hysterese" },                     \
		{ 0x50d0, "rst_min_vbat" },                            \
		{ 0x50e0, "sel_vbat" },                                \
		{ 0x50f0, "bypass_clipper" },                          \
		{ 0x5100, "batsense_steepness" },                      \
		{ 0x5110, "soft_mute" },                               \
		{ 0x5120, "cf_mute_left" },                            \
		{ 0x5130, "cf_mute_right" },                           \
		{ 0x5140, "bypass_hp_left" },                          \
		{ 0x5150, "bypass_hp_right" },                         \
		{ 0x5160, "enbl_dpsa_left" },                          \
		{ 0x5170, "enbl_dpsa_right" },                         \
		{ 0x5187, "cf_volume" },                               \
		{ 0x5200, "ctrl_rcv" },                                \
		{ 0x5210, "ctrl_rcv_fb_100k" },                        \
		{ 0x5222, "ctrl_cc" },                                 \
		{ 0x5257, "gain" },                                    \
		{ 0x52d0, "ctrl_slopectrl" },                          \
		{ 0x52e1, "ctrl_slope" },                              \
		{ 0x5301, "dpsa_level" },                              \
		{ 0x5321, "dpsa_release" },                            \
		{ 0x5340, "clipfast" },                                \
		{ 0x5350, "bypass_lp" },                               \
		{ 0x5360, "enbl_low_latency" },                        \
		{ 0x5400, "first_order_mode" },                        \
		{ 0x5410, "bypass_ctrlloop" },                         \
		{ 0x5420, "fb_hz" },                                   \
		{ 0x5430, "icomp_engage" },                            \
		{ 0x5440, "ctrl_kickback" },                           \
		{ 0x5450, "icomp_engage_overrule" },                   \
		{ 0x5503, "ctrl_dem" },                                \
		{ 0x5543, "ctrl_dem_mismatch" },                       \
		{ 0x5581, "dpsa_drive" },                              \
		{ 0x560a, "enbl_amp_left" },                           \
		{ 0x56b0, "enbl_engage_left" },                        \
		{ 0x570a, "enbl_amp_right" },                          \
		{ 0x57b0, "enbl_engage_right" },                       \
		{ 0x5800, "hard_mute_left" },                          \
		{ 0x5810, "hard_mute_right" },                         \
		{ 0x5820, "pwm_shape" },                               \
		{ 0x5830, "pwm_bitlength" },                           \
		{ 0x5844, "pwm_delay" },                               \
		{ 0x5890, "reclock_pwm" },                             \
		{ 0x58a0, "reclock_voltsense" },                       \
		{ 0x58b0, "enbl_pwm_phase_shift_left" },               \
		{ 0x58c0, "enbl_pwm_phase_shift_right" },              \
		{ 0x5900, "ctrl_rcvldop_pulldown" },                   \
		{ 0x5910, "ctrl_rcvldop_test_comp" },                  \
		{ 0x5920, "ctrl_rcvldop_test_loadedldo" },             \
		{ 0x5930, "enbl_rcvldop" },                            \
		{ 0x5a07, "cf_volume_sec" },                           \
		{ 0x5a87, "sw_profile" },                              \
		{ 0x7002, "boost_volt" },                              \
		{ 0x7033, "boost_cur" },                               \
		{ 0x7071, "bst_coil_value" },                          \
		{ 0x7090, "boost_intel" },                             \
		{ 0x70a0, "boost_speed" },                             \
		{ 0x70b2, "dcdc_synchronisation" },                    \
		{ 0x70e0, "dcdcoff_mode" },                            \
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
		{ 0x7332, "bst_freq" },                                \
		{ 0x8001, "sel_clk_cs" },                              \
		{ 0x8021, "micadc_speed" },                            \
		{ 0x8040, "cs_dc_offset" },                            \
		{ 0x8050, "cs_gain_control" },                         \
		{ 0x8060, "cs_bypass_gc" },                            \
		{ 0x8087, "cs_gain" },                                 \
		{ 0x8110, "invertpwm_left" },                          \
		{ 0x8122, "cmfb_gain_left" },                          \
		{ 0x8154, "cmfb_offset_left" },                        \
		{ 0x8200, "enbl_cmfb_right" },                         \
		{ 0x8210, "invertpwm_right" },                         \
		{ 0x8222, "cmfb_gain_right" },                         \
		{ 0x8254, "cmfb_offset_right" },                       \
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
		{ 0x8600, "enbl_cs_adc_left" },                        \
		{ 0x8610, "enbl_cs_inn1_left" },                       \
		{ 0x8630, "enbl_cs_inp1_left" },                       \
		{ 0x8650, "enbl_cs_ldo_left" },                        \
		{ 0x8660, "enbl_cs_nofloating_n_left" },               \
		{ 0x8670, "enbl_cs_nofloating_p_left" },               \
		{ 0x8680, "enbl_cs_vbatldo_left" },                    \
		{ 0x8700, "enbl_cs_adc_right" },                       \
		{ 0x8710, "enbl_cs_inn1_right" },                      \
		{ 0x8730, "enbl_cs_inp1_right" },                      \
		{ 0x8750, "enbl_cs_ldo_right" },                       \
		{ 0x8760, "enbl_cs_nofloating_n_right" },              \
		{ 0x8770, "enbl_cs_nofloating_p_right" },              \
		{ 0x8780, "enbl_cs_vbatldo_right" },                   \
		{ 0x8800, "volsense_pwm_sel" },                        \
		{ 0x8810, "volsense_dc_offset" },                      \
		{ 0x9000, "cf_rst_dsp" },                              \
		{ 0x9011, "cf_dmem" },                                 \
		{ 0x9030, "cf_aif" },                                  \
		{ 0x9040, "cf_int" },                                  \
		{ 0x9050, "cf_cgate_off" },                            \
		{ 0x9080, "cf_req_cmd" },                              \
		{ 0x9090, "cf_req_reset" },                            \
		{ 0x90a0, "cf_req_mips" },                             \
		{ 0x90b0, "cf_req_mute_ready" },                       \
		{ 0x90c0, "cf_req_volume_ready" },                     \
		{ 0x90d0, "cf_req_damage" },                           \
		{ 0x90e0, "cf_req_calibrate_ready" },                  \
		{ 0x90f0, "cf_req_reserved" },                         \
		{ 0x910f, "cf_madd" },                                 \
		{ 0x920f, "cf_mema" },                                 \
		{ 0x9307, "cf_err" },                                  \
		{ 0x9387, "cf_ack" },                                  \
		{ 0x9380, "cf_ack_cmd" },                              \
		{ 0x9390, "cf_ack_reset" },                            \
		{ 0x93a0, "cf_ack_mips" },                             \
		{ 0x93b0, "cf_ack_mute_ready" },                       \
		{ 0x93c0, "cf_ack_volume_ready" },                     \
		{ 0x93d0, "cf_ack_damage" },                           \
		{ 0x93e0, "cf_ack_calibrate_ready" },                  \
		{ 0x93f0, "cf_ack_reserved" },                         \
		{ 0x980f, "ivt_addr0_msb" },                           \
		{ 0x990f, "ivt_addr0_lsb" },                           \
		{ 0x9a0f, "ivt_addr1_msb" },                           \
		{ 0x9b0f, "ivt_addr1_lsb" },                           \
		{ 0x9c0f, "ivt_addr2_msb" },                           \
		{ 0x9d0f, "ivt_addr2_lsb" },                           \
		{ 0x9e0f, "ivt_addr3_msb" },                           \
		{ 0x9f0f, "ivt_addr3_lsb" },                           \
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
		{ 0xc3d0, "test_abistfft_enbl" },                      \
		{ 0xc3e0, "test_pwr_switch" },                         \
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
		{ 0xc600, "enbl_pwm_dcc" },                            \
		{ 0xc613, "pwm_dcc_cnt" },                             \
		{ 0xc650, "enbl_ldo_stress" },                         \
		{ 0xc660, "bypass_diosw_ovp" },                        \
		{ 0xc670, "enbl_powerswitch" },                        \
		{ 0xc707, "digimuxa_sel" },                            \
		{ 0xc787, "digimuxb_sel" },                            \
		{ 0xc807, "digimuxc_sel" },                            \
		{ 0xc887, "digimuxd_sel" },                            \
		{ 0xc901, "dio1_ehs" },                                \
		{ 0xc921, "dio2_ehs" },                                \
		{ 0xc941, "gainio_ehs" },                              \
		{ 0xc961, "pdmo_ehs" },                                \
		{ 0xc981, "int_ehs" },                                 \
		{ 0xc9a1, "tdo_ehs" },                                 \
		{ 0xc9c0, "hs_mode" },                                 \
		{ 0xca00, "enbl_anamux1" },                            \
		{ 0xca10, "enbl_anamux2" },                            \
		{ 0xca20, "enbl_anamux3" },                            \
		{ 0xca30, "enbl_anamux4" },                            \
		{ 0xca40, "enbl_anamux5" },                            \
		{ 0xca50, "enbl_anamux6" },                            \
		{ 0xca60, "enbl_anamux7" },                            \
		{ 0xca74, "anamux1" },                                 \
		{ 0xcb04, "anamux2" },                                 \
		{ 0xcb54, "anamux3" },                                 \
		{ 0xcba4, "anamux4" },                                 \
		{ 0xcc04, "anamux5" },                                 \
		{ 0xcc54, "anamux6" },                                 \
		{ 0xcca4, "anamux7" },                                 \
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
		{ 0xd243, "tsig_gain_left" },                          \
		{ 0xd283, "tsig_gain_right" },                         \
		{ 0xd300, "adc10_reset" },                             \
		{ 0xd311, "adc10_test" },                              \
		{ 0xd332, "adc10_sel" },                               \
		{ 0xd364, "adc10_prog_sample" },                       \
		{ 0xd3b0, "adc10_enbl" },                              \
		{ 0xd3c0, "bypass_lp_vbat" },                          \
		{ 0xd409, "data_adc10_tempbat" },                      \
		{ 0xd506, "ctrl_digtoana_hidden" },                    \
		{ 0xd570, "enbl_clk_out_of_range" },                   \
		{ 0xf000, "calibration_onetime" },                     \
		{ 0xf010, "calibr_ron_done" },                         \
		{ 0xf020, "calibr_dcdc_api_calibrate" },               \
		{ 0xf030, "calibr_dcdc_delta_sign" },                  \
		{ 0xf042, "calibr_dcdc_delta" },                       \
		{ 0xf078, "calibr_speaker_info" },                     \
		{ 0xf105, "calibr_vout_offset" },                      \
		{ 0xf163, "calibr_gain_left" },                        \
		{ 0xf1a5, "calibr_offset_left" },                      \
		{ 0xf203, "calibr_gain_right" },                       \
		{ 0xf245, "calibr_offset_right" },                     \
		{ 0xf2a3, "calibr_rcvldop_trim" },                     \
		{ 0xf307, "calibr_gain_cs_left" },                     \
		{ 0xf387, "calibr_gain_cs_right" },                    \
		{ 0xf40f, "calibr_R25C_L" },                           \
		{ 0xf50f, "calibr_R25C_R" },                           \
		{ 0xf606, "ctrl_offset_a_left" },                      \
		{ 0xf686, "ctrl_offset_b_left" },                      \
		{ 0xf706, "ctrl_offset_a_right" },                     \
		{ 0xf786, "ctrl_offset_b_right" },                     \
		{ 0xf806, "htol_iic_addr" },                           \
		{ 0xf870, "htol_iic_addr_en" },                        \
		{ 0xf884, "calibr_temp_offset" },                      \
		{ 0xf8d2, "calibr_temp_gain" },                        \
		{ 0xf900, "mtp_lock_dcdcoff_mode" },                   \
		{ 0xf910, "mtp_lock_enbl_coolflux" },                  \
		{ 0xf920, "mtp_lock_bypass_clipper" },                 \
		{ 0xf930, "mtp_lock_max_dcdc_voltage" },               \
		{ 0xf943, "calibr_vbg_trim" },                         \
		{ 0xf987, "type_bits_fw" },                            \
		{ 0xfa0f, "mtpdataA" },                                \
		{ 0xfb0f, "mtpdataB" },                                \
		{ 0xfc0f, "mtpdataC" },                                \
		{ 0xfd0f, "mtpdataD" },                                \
		{ 0xfe0f, "mtpdataE" },                                \
		{ 0xff05, "calibr_osc_delta_ndiv" },                   \
		{ 0xffff, "Unknown bitfield enum" }                    \
	}
enum tfa2_irq {
	tfa2_irq_stvdds = 0,
	tfa2_irq_stplls = 1,
	tfa2_irq_stotds = 2,
	tfa2_irq_stovds = 3,
	tfa2_irq_stuvds = 4,
	tfa2_irq_stclks = 5,
	tfa2_irq_stmtpb = 6,
	tfa2_irq_stnoclk = 7,
	tfa2_irq_stspks = 8,
	tfa2_irq_stacs = 9,
	tfa2_irq_stsws = 10,
	tfa2_irq_stwds = 11,
	tfa2_irq_stamps = 12,
	tfa2_irq_starefs = 13,
	tfa2_irq_stadccr = 14,
	tfa2_irq_stbodnok = 15,
	tfa2_irq_stbstcu = 16,
	tfa2_irq_stbsthi = 17,
	tfa2_irq_stbstoc = 18,
	tfa2_irq_stbstpkcur = 19,
	tfa2_irq_stbstvc = 20,
	tfa2_irq_stbst86 = 21,
	tfa2_irq_stbst93 = 22,
	tfa2_irq_strcvld = 23,
	tfa2_irq_stocpl = 24,
	tfa2_irq_stocpr = 25,
	tfa2_irq_stmwsrc = 26,
	tfa2_irq_stmwcfc = 27,
	tfa2_irq_stmwsmu = 28,
	tfa2_irq_stcfmer = 29,
	tfa2_irq_stcfmac = 30,
	tfa2_irq_stclkoor = 31,
	tfa2_irq_sttdmer = 32,
	tfa2_irq_stclpl = 33,
	tfa2_irq_stclpr = 34,
	tfa2_irq_stocpm = 35,
	tfa2_irq_max = 36,
	tfa2_irq_all = -1
};
#define TFA2_IRQ_NAMETABLE                                                   \
	static struct TfaIrqName Tfa2IrqNames[] = {                          \
		{ 0, "STVDDS" },    { 1, "STPLLS" },      { 2, "STOTDS" },   \
		{ 3, "STOVDS" },    { 4, "STUVDS" },      { 5, "STCLKS" },   \
		{ 6, "STMTPB" },    { 7, "STNOCLK" },     { 8, "STSPKS" },   \
		{ 9, "STACS" },     { 10, "STSWS" },      { 11, "STWDS" },   \
		{ 12, "STAMPS" },   { 13, "STAREFS" },    { 14, "STADCCR" }, \
		{ 15, "STBODNOK" }, { 16, "STBSTCU" },    { 17, "STBSTHI" }, \
		{ 18, "STBSTOC" },  { 19, "STBSTPKCUR" }, { 20, "STBSTVC" }, \
		{ 21, "STBST86" },  { 22, "STBST93" },    { 23, "STRCVLD" }, \
		{ 24, "STOCPL" },   { 25, "STOCPR" },     { 26, "STMWSRC" }, \
		{ 27, "STMWCFC" },  { 28, "STMWSMU" },    { 29, "STCFMER" }, \
		{ 30, "STCFMAC" },  { 31, "STCLKOOR" },   { 32, "STTDMER" }, \
		{ 33, "STCLPL" },   { 34, "STCLPR" },     { 35, "STOCPM" },  \
		{ 36, "36" },                                                \
	}
