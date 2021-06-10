/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _TFA9912_TFAFIELDNAMES_H
#define _TFA9912_TFAFIELDNAMES_H
#define TFA9912_I2CVERSION 1.43
enum nxpTfa9912BfEnumList {
	TFA9912_BF_PWDN = 0x0000,
	TFA9912_BF_I2CR = 0x0010,
	TFA9912_BF_CFE = 0x0020,
	TFA9912_BF_AMPE = 0x0030,
	TFA9912_BF_DCA = 0x0040,
	TFA9912_BF_SBSL = 0x0050,
	TFA9912_BF_AMPC = 0x0060,
	TFA9912_BF_INTP = 0x0071,
	TFA9912_BF_FSSSEL = 0x0090,
	TFA9912_BF_BYPOCP = 0x00b0,
	TFA9912_BF_TSTOCP = 0x00c0,
	TFA9912_BF_AMPINSEL = 0x0101,
	TFA9912_BF_MANSCONF = 0x0120,
	TFA9912_BF_MANCOLD = 0x0130,
	TFA9912_BF_MANAOOSC = 0x0140,
	TFA9912_BF_MANROBOD = 0x0150,
	TFA9912_BF_BODE = 0x0160,
	TFA9912_BF_BODHYS = 0x0170,
	TFA9912_BF_BODFILT = 0x0181,
	TFA9912_BF_BODTHLVL = 0x01a1,
	TFA9912_BF_MUTETO = 0x01d0,
	TFA9912_BF_RCVNS = 0x01e0,
	TFA9912_BF_MANWDE = 0x01f0,
	TFA9912_BF_AUDFS = 0x0203,
	TFA9912_BF_INPLEV = 0x0240,
	TFA9912_BF_FRACTDEL = 0x0255,
	TFA9912_BF_BYPHVBF = 0x02b0,
	TFA9912_BF_TDMC = 0x02c0,
	TFA9912_BF_ENBLADC10 = 0x02e0,
	TFA9912_BF_REV = 0x030f,
	TFA9912_BF_REFCKEXT = 0x0401,
	TFA9912_BF_REFCKSEL = 0x0420,
	TFA9912_BF_ENCFCKSEL = 0x0430,
	TFA9912_BF_CFCKSEL = 0x0441,
	TFA9912_BF_TDMINFSEL = 0x0460,
	TFA9912_BF_DISBLAUTOCLKSEL = 0x0470,
	TFA9912_BF_SELCLKSRC = 0x0480,
	TFA9912_BF_SELTIMSRC = 0x0490,
	TFA9912_BF_SSLEFTE = 0x0500,
	TFA9912_BF_SPKSSEN = 0x0510,
	TFA9912_BF_VSLEFTE = 0x0520,
	TFA9912_BF_VSRIGHTE = 0x0530,
	TFA9912_BF_CSLEFTE = 0x0540,
	TFA9912_BF_CSRIGHTE = 0x0550,
	TFA9912_BF_SSPDME = 0x0560,
	TFA9912_BF_PGALE = 0x0570,
	TFA9912_BF_PGARE = 0x0580,
	TFA9912_BF_SSTDME = 0x0590,
	TFA9912_BF_SSPBSTE = 0x05a0,
	TFA9912_BF_SSADCE = 0x05b0,
	TFA9912_BF_SSFAIME = 0x05c0,
	TFA9912_BF_SSCFTIME = 0x05d0,
	TFA9912_BF_SSCFWDTE = 0x05e0,
	TFA9912_BF_FAIMVBGOVRRL = 0x05f0,
	TFA9912_BF_SAMSPKSEL = 0x0600,
	TFA9912_BF_PDM2IISEN = 0x0610,
	TFA9912_BF_TAPRSTBYPASS = 0x0620,
	TFA9912_BF_CARDECISEL0 = 0x0631,
	TFA9912_BF_CARDECISEL1 = 0x0651,
	TFA9912_BF_TAPDECSEL = 0x0670,
	TFA9912_BF_COMPCOUNT = 0x0680,
	TFA9912_BF_STARTUPMODE = 0x0691,
	TFA9912_BF_AUTOTAP = 0x06b0,
	TFA9912_BF_COMPINITIME = 0x06c1,
	TFA9912_BF_ANAPINITIME = 0x06e1,
	TFA9912_BF_CCHKTH = 0x0707,
	TFA9912_BF_CCHKTL = 0x0787,
	TFA9912_BF_AMPOCRT = 0x0802,
	TFA9912_BF_AMPTCRR = 0x0832,
	TFA9912_BF_STGS = 0x0d00,
	TFA9912_BF_STGAIN = 0x0d18,
	TFA9912_BF_STSMUTE = 0x0da0,
	TFA9912_BF_ST1C = 0x0db0,
	TFA9912_BF_CMFBEL = 0x0e80,
	TFA9912_BF_VDDS = 0x1000,
	TFA9912_BF_PLLS = 0x1010,
	TFA9912_BF_OTDS = 0x1020,
	TFA9912_BF_OVDS = 0x1030,
	TFA9912_BF_UVDS = 0x1040,
	TFA9912_BF_CLKS = 0x1050,
	TFA9912_BF_MTPB = 0x1060,
	TFA9912_BF_NOCLK = 0x1070,
	TFA9912_BF_ACS = 0x1090,
	TFA9912_BF_SWS = 0x10a0,
	TFA9912_BF_WDS = 0x10b0,
	TFA9912_BF_AMPS = 0x10c0,
	TFA9912_BF_AREFS = 0x10d0,
	TFA9912_BF_ADCCR = 0x10e0,
	TFA9912_BF_BODNOK = 0x10f0,
	TFA9912_BF_DCIL = 0x1100,
	TFA9912_BF_DCDCA = 0x1110,
	TFA9912_BF_DCOCPOK = 0x1120,
	TFA9912_BF_DCPEAKCUR = 0x1130,
	TFA9912_BF_DCHVBAT = 0x1140,
	TFA9912_BF_DCH114 = 0x1150,
	TFA9912_BF_DCH107 = 0x1160,
	TFA9912_BF_STMUTEB = 0x1170,
	TFA9912_BF_STMUTE = 0x1180,
	TFA9912_BF_TDMLUTER = 0x1190,
	TFA9912_BF_TDMSTAT = 0x11a2,
	TFA9912_BF_TDMERR = 0x11d0,
	TFA9912_BF_HAPTIC = 0x11e0,
	TFA9912_BF_OCPOAP = 0x1300,
	TFA9912_BF_OCPOAN = 0x1310,
	TFA9912_BF_OCPOBP = 0x1320,
	TFA9912_BF_OCPOBN = 0x1330,
	TFA9912_BF_CLIPAH = 0x1340,
	TFA9912_BF_CLIPAL = 0x1350,
	TFA9912_BF_CLIPBH = 0x1360,
	TFA9912_BF_CLIPBL = 0x1370,
	TFA9912_BF_OCDS = 0x1380,
	TFA9912_BF_CLIPS = 0x1390,
	TFA9912_BF_TCMPTRG = 0x13a0,
	TFA9912_BF_TAPDET = 0x13b0,
	TFA9912_BF_MANWAIT1 = 0x13c0,
	TFA9912_BF_MANWAIT2 = 0x13d0,
	TFA9912_BF_MANMUTE = 0x13e0,
	TFA9912_BF_MANOPER = 0x13f0,
	TFA9912_BF_SPKSL = 0x1400,
	TFA9912_BF_SPKS = 0x1410,
	TFA9912_BF_CLKOOR = 0x1420,
	TFA9912_BF_MANSTATE = 0x1433,
	TFA9912_BF_DCMODE = 0x1471,
	TFA9912_BF_DSPCLKSRC = 0x1490,
	TFA9912_BF_STARTUPMODSTAT = 0x14a1,
	TFA9912_BF_TSPMSTATE = 0x14c3,
	TFA9912_BF_BATS = 0x1509,
	TFA9912_BF_TEMPS = 0x1608,
	TFA9912_BF_VDDPS = 0x1709,
	TFA9912_BF_DCILCF = 0x17a0,
	TFA9912_BF_TDMUC = 0x2000,
	TFA9912_BF_DIO4SEL = 0x2011,
	TFA9912_BF_TDME = 0x2040,
	TFA9912_BF_TDMMODE = 0x2050,
	TFA9912_BF_TDMCLINV = 0x2060,
	TFA9912_BF_TDMFSLN = 0x2073,
	TFA9912_BF_TDMFSPOL = 0x20b0,
	TFA9912_BF_TDMNBCK = 0x20c3,
	TFA9912_BF_TDMSLOTS = 0x2103,
	TFA9912_BF_TDMSLLN = 0x2144,
	TFA9912_BF_TDMBRMG = 0x2194,
	TFA9912_BF_TDMDEL = 0x21e0,
	TFA9912_BF_TDMADJ = 0x21f0,
	TFA9912_BF_TDMOOMP = 0x2201,
	TFA9912_BF_TDMSSIZE = 0x2224,
	TFA9912_BF_TDMTXDFO = 0x2271,
	TFA9912_BF_TDMTXUS0 = 0x2291,
	TFA9912_BF_TDMTXUS1 = 0x22b1,
	TFA9912_BF_TDMTXUS2 = 0x22d1,
	TFA9912_BF_TDMGIE = 0x2300,
	TFA9912_BF_TDMDCE = 0x2310,
	TFA9912_BF_TDMSPKE = 0x2320,
	TFA9912_BF_TDMCSE = 0x2330,
	TFA9912_BF_TDMVSE = 0x2340,
	TFA9912_BF_TDMGOE = 0x2350,
	TFA9912_BF_TDMCF2E = 0x2360,
	TFA9912_BF_TDMCF3E = 0x2370,
	TFA9912_BF_TDMCFE = 0x2380,
	TFA9912_BF_TDMES6 = 0x2390,
	TFA9912_BF_TDMES7 = 0x23a0,
	TFA9912_BF_TDMCF4E = 0x23b0,
	TFA9912_BF_TDMPD1E = 0x23c0,
	TFA9912_BF_TDMPD2E = 0x23d0,
	TFA9912_BF_TDMGIN = 0x2401,
	TFA9912_BF_TDMLIO = 0x2421,
	TFA9912_BF_TDMRIO = 0x2441,
	TFA9912_BF_TDMCSIO = 0x2461,
	TFA9912_BF_TDMVSIO = 0x2481,
	TFA9912_BF_TDMGOIO = 0x24a1,
	TFA9912_BF_TDMCFIO2 = 0x24c1,
	TFA9912_BF_TDMCFIO3 = 0x24e1,
	TFA9912_BF_TDMCFIO = 0x2501,
	TFA9912_BF_TDMLPB6 = 0x2521,
	TFA9912_BF_TDMLPB7 = 0x2541,
	TFA9912_BF_TDMGS = 0x2603,
	TFA9912_BF_TDMDCS = 0x2643,
	TFA9912_BF_TDMSPKS = 0x2683,
	TFA9912_BF_TDMCSS = 0x26c3,
	TFA9912_BF_TDMVSS = 0x2703,
	TFA9912_BF_TDMCGOS = 0x2743,
	TFA9912_BF_TDMCF2S = 0x2783,
	TFA9912_BF_TDMCF3S = 0x27c3,
	TFA9912_BF_TDMCFS = 0x2803,
	TFA9912_BF_TDMEDAT6S = 0x2843,
	TFA9912_BF_TDMEDAT7S = 0x2883,
	TFA9912_BF_TDMTXUS3 = 0x2901,
	TFA9912_BF_PDMSM = 0x3100,
	TFA9912_BF_PDMSTSEL = 0x3110,
	TFA9912_BF_PDMSTENBL = 0x3120,
	TFA9912_BF_PDMLSEL = 0x3130,
	TFA9912_BF_PDMRSEL = 0x3140,
	TFA9912_BF_MICVDDE = 0x3150,
	TFA9912_BF_PDMCLRAT = 0x3201,
	TFA9912_BF_PDMGAIN = 0x3223,
	TFA9912_BF_PDMOSEL = 0x3263,
	TFA9912_BF_SELCFHAPD = 0x32a0,
	TFA9912_BF_ISTVDDS = 0x4000,
	TFA9912_BF_ISTPLLS = 0x4010,
	TFA9912_BF_ISTOTDS = 0x4020,
	TFA9912_BF_ISTOVDS = 0x4030,
	TFA9912_BF_ISTUVDS = 0x4040,
	TFA9912_BF_ISTCLKS = 0x4050,
	TFA9912_BF_ISTMTPB = 0x4060,
	TFA9912_BF_ISTNOCLK = 0x4070,
	TFA9912_BF_ISTSPKS = 0x4080,
	TFA9912_BF_ISTACS = 0x4090,
	TFA9912_BF_ISTSWS = 0x40a0,
	TFA9912_BF_ISTWDS = 0x40b0,
	TFA9912_BF_ISTAMPS = 0x40c0,
	TFA9912_BF_ISTAREFS = 0x40d0,
	TFA9912_BF_ISTADCCR = 0x40e0,
	TFA9912_BF_ISTBODNOK = 0x40f0,
	TFA9912_BF_ISTBSTCU = 0x4100,
	TFA9912_BF_ISTBSTHI = 0x4110,
	TFA9912_BF_ISTBSTOC = 0x4120,
	TFA9912_BF_ISTBSTPKCUR = 0x4130,
	TFA9912_BF_ISTBSTVC = 0x4140,
	TFA9912_BF_ISTBST86 = 0x4150,
	TFA9912_BF_ISTBST93 = 0x4160,
	TFA9912_BF_ISTRCVLD = 0x4170,
	TFA9912_BF_ISTOCPL = 0x4180,
	TFA9912_BF_ISTOCPR = 0x4190,
	TFA9912_BF_ISTMWSRC = 0x41a0,
	TFA9912_BF_ISTMWCFC = 0x41b0,
	TFA9912_BF_ISTMWSMU = 0x41c0,
	TFA9912_BF_ISTCFMER = 0x41d0,
	TFA9912_BF_ISTCFMAC = 0x41e0,
	TFA9912_BF_ISTCLKOOR = 0x41f0,
	TFA9912_BF_ISTTDMER = 0x4200,
	TFA9912_BF_ISTCLPL = 0x4210,
	TFA9912_BF_ISTCLPR = 0x4220,
	TFA9912_BF_ISTOCPM = 0x4230,
	TFA9912_BF_ISTLP1 = 0x4250,
	TFA9912_BF_ISTLA = 0x4260,
	TFA9912_BF_ISTVDDP = 0x4270,
	TFA9912_BF_ISTTAPDET = 0x4280,
	TFA9912_BF_ISTAUDMOD = 0x4290,
	TFA9912_BF_ISTSAMMOD = 0x42a0,
	TFA9912_BF_ISTTAPMOD = 0x42b0,
	TFA9912_BF_ISTTAPTRG = 0x42c0,
	TFA9912_BF_ICLVDDS = 0x4400,
	TFA9912_BF_ICLPLLS = 0x4410,
	TFA9912_BF_ICLOTDS = 0x4420,
	TFA9912_BF_ICLOVDS = 0x4430,
	TFA9912_BF_ICLUVDS = 0x4440,
	TFA9912_BF_ICLCLKS = 0x4450,
	TFA9912_BF_ICLMTPB = 0x4460,
	TFA9912_BF_ICLNOCLK = 0x4470,
	TFA9912_BF_ICLSPKS = 0x4480,
	TFA9912_BF_ICLACS = 0x4490,
	TFA9912_BF_ICLSWS = 0x44a0,
	TFA9912_BF_ICLWDS = 0x44b0,
	TFA9912_BF_ICLAMPS = 0x44c0,
	TFA9912_BF_ICLAREFS = 0x44d0,
	TFA9912_BF_ICLADCCR = 0x44e0,
	TFA9912_BF_ICLBODNOK = 0x44f0,
	TFA9912_BF_ICLBSTCU = 0x4500,
	TFA9912_BF_ICLBSTHI = 0x4510,
	TFA9912_BF_ICLBSTOC = 0x4520,
	TFA9912_BF_ICLBSTPC = 0x4530,
	TFA9912_BF_ICLBSTVC = 0x4540,
	TFA9912_BF_ICLBST86 = 0x4550,
	TFA9912_BF_ICLBST93 = 0x4560,
	TFA9912_BF_ICLRCVLD = 0x4570,
	TFA9912_BF_ICLOCPL = 0x4580,
	TFA9912_BF_ICLOCPR = 0x4590,
	TFA9912_BF_ICLMWSRC = 0x45a0,
	TFA9912_BF_ICLMWCFC = 0x45b0,
	TFA9912_BF_ICLMWSMU = 0x45c0,
	TFA9912_BF_ICLCFMER = 0x45d0,
	TFA9912_BF_ICLCFMAC = 0x45e0,
	TFA9912_BF_ICLCLKOOR = 0x45f0,
	TFA9912_BF_ICLTDMER = 0x4600,
	TFA9912_BF_ICLCLPL = 0x4610,
	TFA9912_BF_ICLCLP = 0x4620,
	TFA9912_BF_ICLOCPM = 0x4630,
	TFA9912_BF_ICLLP1 = 0x4650,
	TFA9912_BF_ICLLA = 0x4660,
	TFA9912_BF_ICLVDDP = 0x4670,
	TFA9912_BF_ICLTAPDET = 0x4680,
	TFA9912_BF_ICLAUDMOD = 0x4690,
	TFA9912_BF_ICLSAMMOD = 0x46a0,
	TFA9912_BF_ICLTAPMOD = 0x46b0,
	TFA9912_BF_ICLTAPTRG = 0x46c0,
	TFA9912_BF_IEVDDS = 0x4800,
	TFA9912_BF_IEPLLS = 0x4810,
	TFA9912_BF_IEOTDS = 0x4820,
	TFA9912_BF_IEOVDS = 0x4830,
	TFA9912_BF_IEUVDS = 0x4840,
	TFA9912_BF_IECLKS = 0x4850,
	TFA9912_BF_IEMTPB = 0x4860,
	TFA9912_BF_IENOCLK = 0x4870,
	TFA9912_BF_IESPKS = 0x4880,
	TFA9912_BF_IEACS = 0x4890,
	TFA9912_BF_IESWS = 0x48a0,
	TFA9912_BF_IEWDS = 0x48b0,
	TFA9912_BF_IEAMPS = 0x48c0,
	TFA9912_BF_IEAREFS = 0x48d0,
	TFA9912_BF_IEADCCR = 0x48e0,
	TFA9912_BF_IEBODNOK = 0x48f0,
	TFA9912_BF_IEBSTCU = 0x4900,
	TFA9912_BF_IEBSTHI = 0x4910,
	TFA9912_BF_IEBSTOC = 0x4920,
	TFA9912_BF_IEBSTPC = 0x4930,
	TFA9912_BF_IEBSTVC = 0x4940,
	TFA9912_BF_IEBST86 = 0x4950,
	TFA9912_BF_IEBST93 = 0x4960,
	TFA9912_BF_IERCVLD = 0x4970,
	TFA9912_BF_IEOCPL = 0x4980,
	TFA9912_BF_IEOCPR = 0x4990,
	TFA9912_BF_IEMWSRC = 0x49a0,
	TFA9912_BF_IEMWCFC = 0x49b0,
	TFA9912_BF_IEMWSMU = 0x49c0,
	TFA9912_BF_IECFMER = 0x49d0,
	TFA9912_BF_IECFMAC = 0x49e0,
	TFA9912_BF_IECLKOOR = 0x49f0,
	TFA9912_BF_IETDMER = 0x4a00,
	TFA9912_BF_IECLPL = 0x4a10,
	TFA9912_BF_IECLPR = 0x4a20,
	TFA9912_BF_IEOCPM1 = 0x4a30,
	TFA9912_BF_IELP1 = 0x4a50,
	TFA9912_BF_IELA = 0x4a60,
	TFA9912_BF_IEVDDP = 0x4a70,
	TFA9912_BF_IETAPDET = 0x4a80,
	TFA9912_BF_IEAUDMOD = 0x4a90,
	TFA9912_BF_IESAMMOD = 0x4aa0,
	TFA9912_BF_IETAPMOD = 0x4ab0,
	TFA9912_BF_IETAPTRG = 0x4ac0,
	TFA9912_BF_IPOVDDS = 0x4c00,
	TFA9912_BF_IPOPLLS = 0x4c10,
	TFA9912_BF_IPOOTDS = 0x4c20,
	TFA9912_BF_IPOOVDS = 0x4c30,
	TFA9912_BF_IPOUVDS = 0x4c40,
	TFA9912_BF_IPOCLKS = 0x4c50,
	TFA9912_BF_IPOMTPB = 0x4c60,
	TFA9912_BF_IPONOCLK = 0x4c70,
	TFA9912_BF_IPOSPKS = 0x4c80,
	TFA9912_BF_IPOACS = 0x4c90,
	TFA9912_BF_IPOSWS = 0x4ca0,
	TFA9912_BF_IPOWDS = 0x4cb0,
	TFA9912_BF_IPOAMPS = 0x4cc0,
	TFA9912_BF_IPOAREFS = 0x4cd0,
	TFA9912_BF_IPOADCCR = 0x4ce0,
	TFA9912_BF_IPOBODNOK = 0x4cf0,
	TFA9912_BF_IPOBSTCU = 0x4d00,
	TFA9912_BF_IPOBSTHI = 0x4d10,
	TFA9912_BF_IPOBSTOC = 0x4d20,
	TFA9912_BF_IPOBSTPC = 0x4d30,
	TFA9912_BF_IPOBSTVC = 0x4d40,
	TFA9912_BF_IPOBST86 = 0x4d50,
	TFA9912_BF_IPOBST93 = 0x4d60,
	TFA9912_BF_IPORCVLD = 0x4d70,
	TFA9912_BF_IPOOCPL = 0x4d80,
	TFA9912_BF_IPOOCPR = 0x4d90,
	TFA9912_BF_IPOMWSRC = 0x4da0,
	TFA9912_BF_IPOMWCFC = 0x4db0,
	TFA9912_BF_IPOMWSMU = 0x4dc0,
	TFA9912_BF_IPOCFMER = 0x4dd0,
	TFA9912_BF_IPOCFMAC = 0x4de0,
	TFA9912_BF_IPOCLKOOR = 0x4df0,
	TFA9912_BF_IPOTDMER = 0x4e00,
	TFA9912_BF_IPOCLPL = 0x4e10,
	TFA9912_BF_IPOCLPR = 0x4e20,
	TFA9912_BF_IPOOCPM = 0x4e30,
	TFA9912_BF_IPOLP1 = 0x4e50,
	TFA9912_BF_IPOLA = 0x4e60,
	TFA9912_BF_IPOVDDP = 0x4e70,
	TFA9912_BF_IPOLTAPDET = 0x4e80,
	TFA9912_BF_IPOLAUDMOD = 0x4e90,
	TFA9912_BF_IPOLSAMMOD = 0x4ea0,
	TFA9912_BF_IPOLTAPMOD = 0x4eb0,
	TFA9912_BF_IPOLTAPTRG = 0x4ec0,
	TFA9912_BF_BSSCR = 0x5001,
	TFA9912_BF_BSST = 0x5023,
	TFA9912_BF_BSSRL = 0x5061,
	TFA9912_BF_BSSRR = 0x5082,
	TFA9912_BF_BSSHY = 0x50b1,
	TFA9912_BF_BSSAC = 0x50d0,
	TFA9912_BF_BSSR = 0x50e0,
	TFA9912_BF_BSSBY = 0x50f0,
	TFA9912_BF_BSSS = 0x5100,
	TFA9912_BF_INTSMUTE = 0x5110,
	TFA9912_BF_CFSML = 0x5120,
	TFA9912_BF_CFSM = 0x5130,
	TFA9912_BF_HPFBYPL = 0x5140,
	TFA9912_BF_HPFBYP = 0x5150,
	TFA9912_BF_DPSAL = 0x5160,
	TFA9912_BF_DPSA = 0x5170,
	TFA9912_BF_VOL = 0x5187,
	TFA9912_BF_HNDSFRCV = 0x5200,
	TFA9912_BF_CLIPCTRL = 0x5222,
	TFA9912_BF_AMPGAIN = 0x5257,
	TFA9912_BF_SLOPEE = 0x52d0,
	TFA9912_BF_SLOPESET = 0x52e0,
	TFA9912_BF_CFTAPPAT = 0x5c07,
	TFA9912_BF_TAPDBGINFO = 0x5c83,
	TFA9912_BF_TATPSTAT1 = 0x5d0f,
	TFA9912_BF_TCOMPTHR = 0x5f03,
	TFA9912_BF_PGAGAIN = 0x6081,
	TFA9912_BF_TDMSPKG = 0x6123,
	TFA9912_BF_LPM1LVL = 0x6505,
	TFA9912_BF_LPM1HLD = 0x6565,
	TFA9912_BF_LPM1DIS = 0x65c0,
	TFA9912_BF_DCDIS = 0x6630,
	TFA9912_BF_TDMSRCMAP = 0x6801,
	TFA9912_BF_TDMSRCAS = 0x6821,
	TFA9912_BF_TDMSRCBS = 0x6841,
	TFA9912_BF_ANC1C = 0x68a0,
	TFA9912_BF_SAMMODE = 0x6901,
	TFA9912_BF_DCMCC = 0x7033,
	TFA9912_BF_DCCV = 0x7071,
	TFA9912_BF_DCIE = 0x7090,
	TFA9912_BF_DCSR = 0x70a0,
	TFA9912_BF_DCINSEL = 0x70c1,
	TFA9912_BF_DCPWM = 0x70f0,
	TFA9912_BF_DCTRIP = 0x7504,
	TFA9912_BF_DCTRIP2 = 0x7554,
	TFA9912_BF_DCTRIPT = 0x75a4,
	TFA9912_BF_DCVOF = 0x7635,
	TFA9912_BF_DCVOS = 0x7695,
	TFA9912_BF_RST = 0x9000,
	TFA9912_BF_DMEM = 0x9011,
	TFA9912_BF_AIF = 0x9030,
	TFA9912_BF_CFINT = 0x9040,
	TFA9912_BF_CFCGATE = 0x9050,
	TFA9912_BF_REQCMD = 0x9080,
	TFA9912_BF_REQRST = 0x9090,
	TFA9912_BF_REQMIPS = 0x90a0,
	TFA9912_BF_REQMUTED = 0x90b0,
	TFA9912_BF_REQVOL = 0x90c0,
	TFA9912_BF_REQDMG = 0x90d0,
	TFA9912_BF_REQCAL = 0x90e0,
	TFA9912_BF_REQRSV = 0x90f0,
	TFA9912_BF_MADD = 0x910f,
	TFA9912_BF_MEMA = 0x920f,
	TFA9912_BF_ERR = 0x9307,
	TFA9912_BF_ACKCMD = 0x9380,
	TFA9912_BF_ACKRST = 0x9390,
	TFA9912_BF_ACKMIPS = 0x93a0,
	TFA9912_BF_ACKMUTED = 0x93b0,
	TFA9912_BF_ACKVOL = 0x93c0,
	TFA9912_BF_ACKDMG = 0x93d0,
	TFA9912_BF_ACKCAL = 0x93e0,
	TFA9912_BF_ACKRSV = 0x93f0,
	TFA9912_BF_MTPK = 0xa107,
	TFA9912_BF_KEY1LOCKED = 0xa200,
	TFA9912_BF_KEY2LOCKED = 0xa210,
	TFA9912_BF_CIMTP = 0xa360,
	TFA9912_BF_MTPRDMSB = 0xa50f,
	TFA9912_BF_MTPRDLSB = 0xa60f,
	TFA9912_BF_EXTTS = 0xb108,
	TFA9912_BF_TROS = 0xb190,
	TFA9912_BF_SWPROFIL = 0xee0f,
	TFA9912_BF_SWVSTEP = 0xef0f,
	TFA9912_BF_MTPOTC = 0xf000,
	TFA9912_BF_MTPEX = 0xf010,
	TFA9912_BF_DCMCCAPI = 0xf020,
	TFA9912_BF_DCMCCSB = 0xf030,
	TFA9912_BF_DCMCCCL = 0xf042,
	TFA9912_BF_USERDEF = 0xf078,
	TFA9912_BF_R25C = 0xf40f,
};
#define TFA9912_NAMETABLE                                   \
	static struct TfaBfName Tfa9912DatasheetNames[] = { \
		{ 0x0, "PWDN" },                            \
		{ 0x10, "I2CR" },                           \
		{ 0x20, "CFE" },                            \
		{ 0x30, "AMPE" },                           \
		{ 0x40, "DCA" },                            \
		{ 0x50, "SBSL" },                           \
		{ 0x60, "AMPC" },                           \
		{ 0x71, "INTP" },                           \
		{ 0x90, "FSSSEL" },                         \
		{ 0xb0, "BYPOCP" },                         \
		{ 0xc0, "TSTOCP" },                         \
		{ 0x101, "AMPINSEL" },                      \
		{ 0x120, "MANSCONF" },                      \
		{ 0x130, "MANCOLD" },                       \
		{ 0x140, "MANAOOSC" },                      \
		{ 0x150, "MANROBOD" },                      \
		{ 0x160, "BODE" },                          \
		{ 0x170, "BODHYS" },                        \
		{ 0x181, "BODFILT" },                       \
		{ 0x1a1, "BODTHLVL" },                      \
		{ 0x1d0, "MUTETO" },                        \
		{ 0x1e0, "RCVNS" },                         \
		{ 0x1f0, "MANWDE" },                        \
		{ 0x203, "AUDFS" },                         \
		{ 0x240, "INPLEV" },                        \
		{ 0x255, "FRACTDEL" },                      \
		{ 0x2b0, "BYPHVBF" },                       \
		{ 0x2c0, "TDMC" },                          \
		{ 0x2e0, "ENBLADC10" },                     \
		{ 0x30f, "REV" },                           \
		{ 0x401, "REFCKEXT" },                      \
		{ 0x420, "REFCKSEL" },                      \
		{ 0x430, "ENCFCKSEL" },                     \
		{ 0x441, "CFCKSEL" },                       \
		{ 0x460, "TDMINFSEL" },                     \
		{ 0x470, "DISBLAUTOCLKSEL" },               \
		{ 0x480, "SELCLKSRC" },                     \
		{ 0x490, "SELTIMSRC" },                     \
		{ 0x500, "SSLEFTE" },                       \
		{ 0x510, "SPKSSEN" },                       \
		{ 0x520, "VSLEFTE" },                       \
		{ 0x530, "VSRIGHTE" },                      \
		{ 0x540, "CSLEFTE" },                       \
		{ 0x550, "CSRIGHTE" },                      \
		{ 0x560, "SSPDME" },                        \
		{ 0x570, "PGALE" },                         \
		{ 0x580, "PGARE" },                         \
		{ 0x590, "SSTDME" },                        \
		{ 0x5a0, "SSPBSTE" },                       \
		{ 0x5b0, "SSADCE" },                        \
		{ 0x5c0, "SSFAIME" },                       \
		{ 0x5d0, "SSCFTIME" },                      \
		{ 0x5e0, "SSCFWDTE" },                      \
		{ 0x5f0, "FAIMVBGOVRRL" },                  \
		{ 0x600, "SAMSPKSEL" },                     \
		{ 0x610, "PDM2IISEN" },                     \
		{ 0x620, "TAPRSTBYPASS" },                  \
		{ 0x631, "CARDECISEL0" },                   \
		{ 0x651, "CARDECISEL1" },                   \
		{ 0x670, "TAPDECSEL" },                     \
		{ 0x680, "COMPCOUNT" },                     \
		{ 0x691, "STARTUPMODE" },                   \
		{ 0x6b0, "AUTOTAP" },                       \
		{ 0x6c1, "COMPINITIME" },                   \
		{ 0x6e1, "ANAPINITIME" },                   \
		{ 0x707, "CCHKTH" },                        \
		{ 0x787, "CCHKTL" },                        \
		{ 0x802, "AMPOCRT" },                       \
		{ 0x832, "AMPTCRR" },                       \
		{ 0xd00, "STGS" },                          \
		{ 0xd18, "STGAIN" },                        \
		{ 0xda0, "STSMUTE" },                       \
		{ 0xdb0, "ST1C" },                          \
		{ 0xe80, "CMFBEL" },                        \
		{ 0x1000, "VDDS" },                         \
		{ 0x1010, "PLLS" },                         \
		{ 0x1020, "OTDS" },                         \
		{ 0x1030, "OVDS" },                         \
		{ 0x1040, "UVDS" },                         \
		{ 0x1050, "CLKS" },                         \
		{ 0x1060, "MTPB" },                         \
		{ 0x1070, "NOCLK" },                        \
		{ 0x1090, "ACS" },                          \
		{ 0x10a0, "SWS" },                          \
		{ 0x10b0, "WDS" },                          \
		{ 0x10c0, "AMPS" },                         \
		{ 0x10d0, "AREFS" },                        \
		{ 0x10e0, "ADCCR" },                        \
		{ 0x10f0, "BODNOK" },                       \
		{ 0x1100, "DCIL" },                         \
		{ 0x1110, "DCDCA" },                        \
		{ 0x1120, "DCOCPOK" },                      \
		{ 0x1130, "DCPEAKCUR" },                    \
		{ 0x1140, "DCHVBAT" },                      \
		{ 0x1150, "DCH114" },                       \
		{ 0x1160, "DCH107" },                       \
		{ 0x1170, "STMUTEB" },                      \
		{ 0x1180, "STMUTE" },                       \
		{ 0x1190, "TDMLUTER" },                     \
		{ 0x11a2, "TDMSTAT" },                      \
		{ 0x11d0, "TDMERR" },                       \
		{ 0x11e0, "HAPTIC" },                       \
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
		{ 0x13a0, "TCMPTRG" },                      \
		{ 0x13b0, "TAPDET" },                       \
		{ 0x13c0, "MANWAIT1" },                     \
		{ 0x13d0, "MANWAIT2" },                     \
		{ 0x13e0, "MANMUTE" },                      \
		{ 0x13f0, "MANOPER" },                      \
		{ 0x1400, "SPKSL" },                        \
		{ 0x1410, "SPKS" },                         \
		{ 0x1420, "CLKOOR" },                       \
		{ 0x1433, "MANSTATE" },                     \
		{ 0x1471, "DCMODE" },                       \
		{ 0x1490, "DSPCLKSRC" },                    \
		{ 0x14a1, "STARTUPMODSTAT" },               \
		{ 0x14c3, "TSPMSTATE" },                    \
		{ 0x1509, "BATS" },                         \
		{ 0x1608, "TEMPS" },                        \
		{ 0x1709, "VDDPS" },                        \
		{ 0x17a0, "DCILCF" },                       \
		{ 0x2000, "TDMUC" },                        \
		{ 0x2011, "DIO4SEL" },                      \
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
		{ 0x22b1, "TDMTXUS1" },                     \
		{ 0x22d1, "TDMTXUS2" },                     \
		{ 0x2300, "TDMGIE" },                       \
		{ 0x2310, "TDMDCE" },                       \
		{ 0x2320, "TDMSPKE" },                      \
		{ 0x2330, "TDMCSE" },                       \
		{ 0x2340, "TDMVSE" },                       \
		{ 0x2350, "TDMGOE" },                       \
		{ 0x2360, "TDMCF2E" },                      \
		{ 0x2370, "TDMCF3E" },                      \
		{ 0x2380, "TDMCFE" },                       \
		{ 0x2390, "TDMES6" },                       \
		{ 0x23a0, "TDMES7" },                       \
		{ 0x23b0, "TDMCF4E" },                      \
		{ 0x23c0, "TDMPD1E" },                      \
		{ 0x23d0, "TDMPD2E" },                      \
		{ 0x2401, "TDMGIN" },                       \
		{ 0x2421, "TDMLIO" },                       \
		{ 0x2441, "TDMRIO" },                       \
		{ 0x2461, "TDMCSIO" },                      \
		{ 0x2481, "TDMVSIO" },                      \
		{ 0x24a1, "TDMGOIO" },                      \
		{ 0x24c1, "TDMCFIO2" },                     \
		{ 0x24e1, "TDMCFIO3" },                     \
		{ 0x2501, "TDMCFIO" },                      \
		{ 0x2521, "TDMLPB6" },                      \
		{ 0x2541, "TDMLPB7" },                      \
		{ 0x2603, "TDMGS" },                        \
		{ 0x2643, "TDMDCS" },                       \
		{ 0x2683, "TDMSPKS" },                      \
		{ 0x26c3, "TDMCSS" },                       \
		{ 0x2703, "TDMVSS" },                       \
		{ 0x2743, "TDMCGOS" },                      \
		{ 0x2783, "TDMCF2S" },                      \
		{ 0x27c3, "TDMCF3S" },                      \
		{ 0x2803, "TDMCFS" },                       \
		{ 0x2843, "TDMEDAT6S" },                    \
		{ 0x2883, "TDMEDAT7S" },                    \
		{ 0x2901, "TDMTXUS3" },                     \
		{ 0x3100, "PDMSM" },                        \
		{ 0x3110, "PDMSTSEL" },                     \
		{ 0x3120, "PDMSTENBL" },                    \
		{ 0x3130, "PDMLSEL" },                      \
		{ 0x3140, "PDMRSEL" },                      \
		{ 0x3150, "MICVDDE" },                      \
		{ 0x3201, "PDMCLRAT" },                     \
		{ 0x3223, "PDMGAIN" },                      \
		{ 0x3263, "PDMOSEL" },                      \
		{ 0x32a0, "SELCFHAPD" },                    \
		{ 0x4000, "ISTVDDS" },                      \
		{ 0x4010, "ISTPLLS" },                      \
		{ 0x4020, "ISTOTDS" },                      \
		{ 0x4030, "ISTOVDS" },                      \
		{ 0x4040, "ISTUVDS" },                      \
		{ 0x4050, "ISTCLKS" },                      \
		{ 0x4060, "ISTMTPB" },                      \
		{ 0x4070, "ISTNOCLK" },                     \
		{ 0x4080, "ISTSPKS" },                      \
		{ 0x4090, "ISTACS" },                       \
		{ 0x40a0, "ISTSWS" },                       \
		{ 0x40b0, "ISTWDS" },                       \
		{ 0x40c0, "ISTAMPS" },                      \
		{ 0x40d0, "ISTAREFS" },                     \
		{ 0x40e0, "ISTADCCR" },                     \
		{ 0x40f0, "ISTBODNOK" },                    \
		{ 0x4100, "ISTBSTCU" },                     \
		{ 0x4110, "ISTBSTHI" },                     \
		{ 0x4120, "ISTBSTOC" },                     \
		{ 0x4130, "ISTBSTPKCUR" },                  \
		{ 0x4140, "ISTBSTVC" },                     \
		{ 0x4150, "ISTBST86" },                     \
		{ 0x4160, "ISTBST93" },                     \
		{ 0x4170, "ISTRCVLD" },                     \
		{ 0x4180, "ISTOCPL" },                      \
		{ 0x4190, "ISTOCPR" },                      \
		{ 0x41a0, "ISTMWSRC" },                     \
		{ 0x41b0, "ISTMWCFC" },                     \
		{ 0x41c0, "ISTMWSMU" },                     \
		{ 0x41d0, "ISTCFMER" },                     \
		{ 0x41e0, "ISTCFMAC" },                     \
		{ 0x41f0, "ISTCLKOOR" },                    \
		{ 0x4200, "ISTTDMER" },                     \
		{ 0x4210, "ISTCLPL" },                      \
		{ 0x4220, "ISTCLPR" },                      \
		{ 0x4230, "ISTOCPM" },                      \
		{ 0x4250, "ISTLP1" },                       \
		{ 0x4260, "ISTLA" },                        \
		{ 0x4270, "ISTVDDP" },                      \
		{ 0x4280, "ISTTAPDET" },                    \
		{ 0x4290, "ISTAUDMOD" },                    \
		{ 0x42a0, "ISTSAMMOD" },                    \
		{ 0x42b0, "ISTTAPMOD" },                    \
		{ 0x42c0, "ISTTAPTRG" },                    \
		{ 0x4400, "ICLVDDS" },                      \
		{ 0x4410, "ICLPLLS" },                      \
		{ 0x4420, "ICLOTDS" },                      \
		{ 0x4430, "ICLOVDS" },                      \
		{ 0x4440, "ICLUVDS" },                      \
		{ 0x4450, "ICLCLKS" },                      \
		{ 0x4460, "ICLMTPB" },                      \
		{ 0x4470, "ICLNOCLK" },                     \
		{ 0x4480, "ICLSPKS" },                      \
		{ 0x4490, "ICLACS" },                       \
		{ 0x44a0, "ICLSWS" },                       \
		{ 0x44b0, "ICLWDS" },                       \
		{ 0x44c0, "ICLAMPS" },                      \
		{ 0x44d0, "ICLAREFS" },                     \
		{ 0x44e0, "ICLADCCR" },                     \
		{ 0x44f0, "ICLBODNOK" },                    \
		{ 0x4500, "ICLBSTCU" },                     \
		{ 0x4510, "ICLBSTHI" },                     \
		{ 0x4520, "ICLBSTOC" },                     \
		{ 0x4530, "ICLBSTPC" },                     \
		{ 0x4540, "ICLBSTVC" },                     \
		{ 0x4550, "ICLBST86" },                     \
		{ 0x4560, "ICLBST93" },                     \
		{ 0x4570, "ICLRCVLD" },                     \
		{ 0x4580, "ICLOCPL" },                      \
		{ 0x4590, "ICLOCPR" },                      \
		{ 0x45a0, "ICLMWSRC" },                     \
		{ 0x45b0, "ICLMWCFC" },                     \
		{ 0x45c0, "ICLMWSMU" },                     \
		{ 0x45d0, "ICLCFMER" },                     \
		{ 0x45e0, "ICLCFMAC" },                     \
		{ 0x45f0, "ICLCLKOOR" },                    \
		{ 0x4600, "ICLTDMER" },                     \
		{ 0x4610, "ICLCLPL" },                      \
		{ 0x4620, "ICLCLP" },                       \
		{ 0x4630, "ICLOCPM" },                      \
		{ 0x4650, "ICLLP1" },                       \
		{ 0x4660, "ICLLA" },                        \
		{ 0x4670, "ICLVDDP" },                      \
		{ 0x4680, "ICLTAPDET" },                    \
		{ 0x4690, "ICLAUDMOD" },                    \
		{ 0x46a0, "ICLSAMMOD" },                    \
		{ 0x46b0, "ICLTAPMOD" },                    \
		{ 0x46c0, "ICLTAPTRG" },                    \
		{ 0x4800, "IEVDDS" },                       \
		{ 0x4810, "IEPLLS" },                       \
		{ 0x4820, "IEOTDS" },                       \
		{ 0x4830, "IEOVDS" },                       \
		{ 0x4840, "IEUVDS" },                       \
		{ 0x4850, "IECLKS" },                       \
		{ 0x4860, "IEMTPB" },                       \
		{ 0x4870, "IENOCLK" },                      \
		{ 0x4880, "IESPKS" },                       \
		{ 0x4890, "IEACS" },                        \
		{ 0x48a0, "IESWS" },                        \
		{ 0x48b0, "IEWDS" },                        \
		{ 0x48c0, "IEAMPS" },                       \
		{ 0x48d0, "IEAREFS" },                      \
		{ 0x48e0, "IEADCCR" },                      \
		{ 0x48f0, "IEBODNOK" },                     \
		{ 0x4900, "IEBSTCU" },                      \
		{ 0x4910, "IEBSTHI" },                      \
		{ 0x4920, "IEBSTOC" },                      \
		{ 0x4930, "IEBSTPC" },                      \
		{ 0x4940, "IEBSTVC" },                      \
		{ 0x4950, "IEBST86" },                      \
		{ 0x4960, "IEBST93" },                      \
		{ 0x4970, "IERCVLD" },                      \
		{ 0x4980, "IEOCPL" },                       \
		{ 0x4990, "IEOCPR" },                       \
		{ 0x49a0, "IEMWSRC" },                      \
		{ 0x49b0, "IEMWCFC" },                      \
		{ 0x49c0, "IEMWSMU" },                      \
		{ 0x49d0, "IECFMER" },                      \
		{ 0x49e0, "IECFMAC" },                      \
		{ 0x49f0, "IECLKOOR" },                     \
		{ 0x4a00, "IETDMER" },                      \
		{ 0x4a10, "IECLPL" },                       \
		{ 0x4a20, "IECLPR" },                       \
		{ 0x4a30, "IEOCPM1" },                      \
		{ 0x4a50, "IELP1" },                        \
		{ 0x4a60, "IELA" },                         \
		{ 0x4a70, "IEVDDP" },                       \
		{ 0x4a80, "IETAPDET" },                     \
		{ 0x4a90, "IEAUDMOD" },                     \
		{ 0x4aa0, "IESAMMOD" },                     \
		{ 0x4ab0, "IETAPMOD" },                     \
		{ 0x4ac0, "IETAPTRG" },                     \
		{ 0x4c00, "IPOVDDS" },                      \
		{ 0x4c10, "IPOPLLS" },                      \
		{ 0x4c20, "IPOOTDS" },                      \
		{ 0x4c30, "IPOOVDS" },                      \
		{ 0x4c40, "IPOUVDS" },                      \
		{ 0x4c50, "IPOCLKS" },                      \
		{ 0x4c60, "IPOMTPB" },                      \
		{ 0x4c70, "IPONOCLK" },                     \
		{ 0x4c80, "IPOSPKS" },                      \
		{ 0x4c90, "IPOACS" },                       \
		{ 0x4ca0, "IPOSWS" },                       \
		{ 0x4cb0, "IPOWDS" },                       \
		{ 0x4cc0, "IPOAMPS" },                      \
		{ 0x4cd0, "IPOAREFS" },                     \
		{ 0x4ce0, "IPOADCCR" },                     \
		{ 0x4cf0, "IPOBODNOK" },                    \
		{ 0x4d00, "IPOBSTCU" },                     \
		{ 0x4d10, "IPOBSTHI" },                     \
		{ 0x4d20, "IPOBSTOC" },                     \
		{ 0x4d30, "IPOBSTPC" },                     \
		{ 0x4d40, "IPOBSTVC" },                     \
		{ 0x4d50, "IPOBST86" },                     \
		{ 0x4d60, "IPOBST93" },                     \
		{ 0x4d70, "IPORCVLD" },                     \
		{ 0x4d80, "IPOOCPL" },                      \
		{ 0x4d90, "IPOOCPR" },                      \
		{ 0x4da0, "IPOMWSRC" },                     \
		{ 0x4db0, "IPOMWCFC" },                     \
		{ 0x4dc0, "IPOMWSMU" },                     \
		{ 0x4dd0, "IPOCFMER" },                     \
		{ 0x4de0, "IPOCFMAC" },                     \
		{ 0x4df0, "IPOCLKOOR" },                    \
		{ 0x4e00, "IPOTDMER" },                     \
		{ 0x4e10, "IPOCLPL" },                      \
		{ 0x4e20, "IPOCLPR" },                      \
		{ 0x4e30, "IPOOCPM" },                      \
		{ 0x4e50, "IPOLP1" },                       \
		{ 0x4e60, "IPOLA" },                        \
		{ 0x4e70, "IPOVDDP" },                      \
		{ 0x4e80, "IPOLTAPDET" },                   \
		{ 0x4e90, "IPOLAUDMOD" },                   \
		{ 0x4ea0, "IPOLSAMMOD" },                   \
		{ 0x4eb0, "IPOLTAPMOD" },                   \
		{ 0x4ec0, "IPOLTAPTRG" },                   \
		{ 0x5001, "BSSCR" },                        \
		{ 0x5023, "BSST" },                         \
		{ 0x5061, "BSSRL" },                        \
		{ 0x5082, "BSSRR" },                        \
		{ 0x50b1, "BSSHY" },                        \
		{ 0x50d0, "BSSAC" },                        \
		{ 0x50e0, "BSSR" },                         \
		{ 0x50f0, "BSSBY" },                        \
		{ 0x5100, "BSSS" },                         \
		{ 0x5110, "INTSMUTE" },                     \
		{ 0x5120, "CFSML" },                        \
		{ 0x5130, "CFSM" },                         \
		{ 0x5140, "HPFBYPL" },                      \
		{ 0x5150, "HPFBYP" },                       \
		{ 0x5160, "DPSAL" },                        \
		{ 0x5170, "DPSA" },                         \
		{ 0x5187, "VOL" },                          \
		{ 0x5200, "HNDSFRCV" },                     \
		{ 0x5222, "CLIPCTRL" },                     \
		{ 0x5257, "AMPGAIN" },                      \
		{ 0x52d0, "SLOPEE" },                       \
		{ 0x52e0, "SLOPESET" },                     \
		{ 0x5c07, "CFTAPPAT" },                     \
		{ 0x5c83, "TAPDBGINFO" },                   \
		{ 0x5d0f, "TATPSTAT1" },                    \
		{ 0x5f03, "TCOMPTHR" },                     \
		{ 0x6081, "PGAGAIN" },                      \
		{ 0x6123, "TDMSPKG" },                      \
		{ 0x6505, "LPM1LVL" },                      \
		{ 0x6565, "LPM1HLD" },                      \
		{ 0x65c0, "LPM1DIS" },                      \
		{ 0x6630, "DCDIS" },                        \
		{ 0x6801, "TDMSRCMAP" },                    \
		{ 0x6821, "TDMSRCAS" },                     \
		{ 0x6841, "TDMSRCBS" },                     \
		{ 0x68a0, "ANC1C" },                        \
		{ 0x6901, "SAMMODE" },                      \
		{ 0x7033, "DCMCC" },                        \
		{ 0x7071, "DCCV" },                         \
		{ 0x7090, "DCIE" },                         \
		{ 0x70a0, "DCSR" },                         \
		{ 0x70c1, "DCINSEL" },                      \
		{ 0x70f0, "DCPWM" },                        \
		{ 0x7504, "DCTRIP" },                       \
		{ 0x7554, "DCTRIP2" },                      \
		{ 0x75a4, "DCTRIPT" },                      \
		{ 0x7635, "DCVOF" },                        \
		{ 0x7695, "DCVOS" },                        \
		{ 0x9000, "RST" },                          \
		{ 0x9011, "DMEM" },                         \
		{ 0x9030, "AIF" },                          \
		{ 0x9040, "CFINT" },                        \
		{ 0x9050, "CFCGATE" },                      \
		{ 0x9080, "REQCMD" },                       \
		{ 0x9090, "REQRST" },                       \
		{ 0x90a0, "REQMIPS" },                      \
		{ 0x90b0, "REQMUTED" },                     \
		{ 0x90c0, "REQVOL" },                       \
		{ 0x90d0, "REQDMG" },                       \
		{ 0x90e0, "REQCAL" },                       \
		{ 0x90f0, "REQRSV" },                       \
		{ 0x910f, "MADD" },                         \
		{ 0x920f, "MEMA" },                         \
		{ 0x9307, "ERR" },                          \
		{ 0x9380, "ACKCMD" },                       \
		{ 0x9390, "ACKRST" },                       \
		{ 0x93a0, "ACKMIPS" },                      \
		{ 0x93b0, "ACKMUTED" },                     \
		{ 0x93c0, "ACKVOL" },                       \
		{ 0x93d0, "ACKDMG" },                       \
		{ 0x93e0, "ACKCAL" },                       \
		{ 0x93f0, "ACKRSV" },                       \
		{ 0xa107, "MTPK" },                         \
		{ 0xa200, "KEY1LOCKED" },                   \
		{ 0xa210, "KEY2LOCKED" },                   \
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
		{ 0xf042, "DCMCCCL" },                      \
		{ 0xf078, "USERDEF" },                      \
		{ 0xf40f, "R25C" },                         \
		{ 0xffff, "Unknown bitfield enum" }         \
	}
#define TFA9912_BITNAMETABLE                                           \
	static struct TfaBfName Tfa9912BitNames[] = {                  \
		{ 0x0, "powerdown" },                                  \
		{ 0x10, "reset" },                                     \
		{ 0x20, "enbl_coolflux" },                             \
		{ 0x30, "enbl_amplifier" },                            \
		{ 0x40, "enbl_boost" },                                \
		{ 0x50, "coolflux_configured" },                       \
		{ 0x60, "sel_enbl_amplifier" },                        \
		{ 0x71, "int_pad_io" },                                \
		{ 0x90, "fs_pulse_sel" },                              \
		{ 0xb0, "bypass_ocp" },                                \
		{ 0xc0, "test_ocp" },                                  \
		{ 0x101, "vamp_sel" },                                 \
		{ 0x120, "src_set_configured" },                       \
		{ 0x130, "execute_cold_start" },                       \
		{ 0x140, "enbl_fro8m_auto_off" },                      \
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
		{ 0x2c0, "tdm_tfa9872_compatible" },                   \
		{ 0x2d0, "sel_hysteresis" },                           \
		{ 0x2e0, "enbl_adc10" },                               \
		{ 0x30f, "device_rev" },                               \
		{ 0x401, "pll_clkin_sel" },                            \
		{ 0x420, "pll_clkin_sel_osc" },                        \
		{ 0x430, "cf_clock_scaling" },                         \
		{ 0x441, "sel_cf_clock" },                             \
		{ 0x460, "tdm_intf_sel" },                             \
		{ 0x470, "disable_auto_sel_refclk" },                  \
		{ 0x480, "sel_clk_src" },                              \
		{ 0x490, "wdt_tim_clk_src" },                          \
		{ 0x510, "enbl_spkr_ss" },                             \
		{ 0x530, "enbl_volsense" },                            \
		{ 0x550, "enbl_cursense" },                            \
		{ 0x560, "enbl_pdm_ss" },                              \
		{ 0x580, "enbl_pga_chop" },                            \
		{ 0x590, "enbl_tdm_ss" },                              \
		{ 0x5a0, "enbl_bst_ss" },                              \
		{ 0x5b0, "enbl_adc10_ss" },                            \
		{ 0x5c0, "enbl_faim_ss" },                             \
		{ 0x5d0, "enbl_tim_clk" },                             \
		{ 0x5e0, "enbl_wdt_clk" },                             \
		{ 0x5f0, "faim_enable_vbg" },                          \
		{ 0x600, "aux_spkr_sel" },                             \
		{ 0x620, "bypass_tapdec_reset" },                      \
		{ 0x631, "car_dec_in_sel0" },                          \
		{ 0x651, "car_dec_in_sel1" },                          \
		{ 0x670, "tapdec_sel" },                               \
		{ 0x680, "comp_count" },                               \
		{ 0x691, "startup_mode" },                             \
		{ 0x6b0, "enable_auto_tap_switching" },                \
		{ 0x6c1, "comp_init_time" },                           \
		{ 0x6e1, "ana_init_time" },                            \
		{ 0x707, "clkchk_th_hi" },                             \
		{ 0x787, "clkchk_th_lo" },                             \
		{ 0x802, "ctrl_on2off_criterion" },                    \
		{ 0x832, "ctrl_on2tap_criterion" },                    \
		{ 0xd00, "side_tone_gain_sel" },                       \
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
		{ 0x13a0, "flag_tap_comp_trig" },                      \
		{ 0x13b0, "flag_cf_tapdetected" },                     \
		{ 0x13c0, "flag_man_wait_src_settings" },              \
		{ 0x13d0, "flag_man_wait_cf_config" },                 \
		{ 0x13e0, "flag_man_start_mute_audio" },               \
		{ 0x1410, "flag_cf_speakererror" },                    \
		{ 0x1420, "flag_clk_out_of_range" },                   \
		{ 0x1433, "man_state" },                               \
		{ 0x1471, "status_bst_mode" },                         \
		{ 0x1490, "man_dsp_clk_src" },                         \
		{ 0x14a1, "man_startup_mode" },                        \
		{ 0x14c3, "tap_machine_state" },                       \
		{ 0x1509, "bat_adc" },                                 \
		{ 0x1608, "temp_adc" },                                \
		{ 0x1709, "vddp_adc" },                                \
		{ 0x17a0, "flag_bst_bstcur_cf" },                      \
		{ 0x2000, "tdm_usecase" },                             \
		{ 0x2011, "dio4_input_sel" },                          \
		{ 0x2040, "tdm_enable" },                              \
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
		{ 0x2901, "tdm_txdata_format_unused_slot_sd3" },       \
		{ 0x3100, "pdm_mode" },                                \
		{ 0x3110, "pdm_input_sel" },                           \
		{ 0x3120, "enbl_pdm_side_tone" },                      \
		{ 0x3201, "pdm_nbck" },                                \
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
		{ 0x4190, "int_out_flag_ocp_alarm" },                  \
		{ 0x41a0, "int_out_flag_man_wait_src_settings" },      \
		{ 0x41b0, "int_out_flag_man_wait_cf_config" },         \
		{ 0x41c0, "int_out_flag_man_start_mute_audio" },       \
		{ 0x41d0, "int_out_flag_cfma_err" },                   \
		{ 0x41e0, "int_out_flag_cfma_ack" },                   \
		{ 0x41f0, "int_out_flag_clk_out_of_range" },           \
		{ 0x4200, "int_out_flag_tdm_error" },                  \
		{ 0x4220, "int_out_flag_clip" },                       \
		{ 0x4250, "int_out_flag_lp_detect_mode1" },            \
		{ 0x4260, "int_out_flag_low_amplitude" },              \
		{ 0x4270, "int_out_flag_vddp_gt_vbat" },               \
		{ 0x4280, "int_out_newtap" },                          \
		{ 0x4290, "int_out_audiomodeactive" },                 \
		{ 0x42a0, "int_out_sammodeactive" },                   \
		{ 0x42b0, "int_out_tapmodeactive" },                   \
		{ 0x42c0, "int_out_flag_tap_comp_trig" },              \
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
		{ 0x4590, "int_in_flag_ocp_alarm" },                   \
		{ 0x45a0, "int_in_flag_man_wait_src_settings" },       \
		{ 0x45b0, "int_in_flag_man_wait_cf_config" },          \
		{ 0x45c0, "int_in_flag_man_start_mute_audio" },        \
		{ 0x45d0, "int_in_flag_cfma_err" },                    \
		{ 0x45e0, "int_in_flag_cfma_ack" },                    \
		{ 0x45f0, "int_in_flag_clk_out_of_range" },            \
		{ 0x4600, "int_in_flag_tdm_error" },                   \
		{ 0x4620, "int_in_flag_clip" },                        \
		{ 0x4650, "int_in_flag_lp_detect_mode1" },             \
		{ 0x4660, "int_in_flag_low_amplitude" },               \
		{ 0x4670, "int_in_flag_vddp_gt_vbat" },                \
		{ 0x4680, "int_in_newtap" },                           \
		{ 0x4690, "int_in_audiomodeactive" },                  \
		{ 0x46a0, "int_in_sammodeactive" },                    \
		{ 0x46b0, "int_in_tapmodeactive" },                    \
		{ 0x46c0, "int_in_flag_tap_comp_trig" },               \
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
		{ 0x4990, "int_enable_flag_ocp_alarm" },               \
		{ 0x49a0, "int_enable_flag_man_wait_src_settings" },   \
		{ 0x49b0, "int_enable_flag_man_wait_cf_config" },      \
		{ 0x49c0, "int_enable_flag_man_start_mute_audio" },    \
		{ 0x49d0, "int_enable_flag_cfma_err" },                \
		{ 0x49e0, "int_enable_flag_cfma_ack" },                \
		{ 0x49f0, "int_enable_flag_clk_out_of_range" },        \
		{ 0x4a00, "int_enable_flag_tdm_error" },               \
		{ 0x4a20, "int_enable_flag_clip" },                    \
		{ 0x4a50, "int_enable_flag_lp_detect_mode1" },         \
		{ 0x4a60, "int_enable_flag_low_amplitude" },           \
		{ 0x4a70, "int_enable_flag_vddp_gt_vbat" },            \
		{ 0x4a80, "int_enable_newtap" },                       \
		{ 0x4a90, "int_enable_audiomodeactive" },              \
		{ 0x4aa0, "int_enable_sammodeactive" },                \
		{ 0x4ab0, "int_enable_tapmodeactive" },                \
		{ 0x4ac0, "int_enable_flag_tap_comp_trig" },           \
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
		{ 0x4d90, "int_polarity_flag_ocp_alarm" },             \
		{ 0x4da0, "int_polarity_flag_man_wait_src_settings" }, \
		{ 0x4db0, "int_polarity_flag_man_wait_cf_config" },    \
		{ 0x4dc0, "int_polarity_flag_man_start_mute_audio" },  \
		{ 0x4dd0, "int_polarity_flag_cfma_err" },              \
		{ 0x4de0, "int_polarity_flag_cfma_ack" },              \
		{ 0x4df0, "int_polarity_flag_clk_out_of_range" },      \
		{ 0x4e00, "int_polarity_flag_tdm_error" },             \
		{ 0x4e20, "int_polarity_flag_clip" },                  \
		{ 0x4e50, "int_polarity_flag_lp_detect_mode1" },       \
		{ 0x4e60, "int_polarity_flag_low_amplitude" },         \
		{ 0x4e70, "int_polarity_flag_vddp_gt_vbat" },          \
		{ 0x4e80, "int_polarity_newtap" },                     \
		{ 0x4e90, "int_polarity_audiomodeactive" },            \
		{ 0x4ea0, "int_polarity_sammodeactive" },              \
		{ 0x4eb0, "int_polarity_tapmodeactive" },              \
		{ 0x4ec0, "int_polarity_flag_tap_comp_trig" },         \
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
		{ 0x5130, "cf_mute" },                                 \
		{ 0x5150, "bypass_hp" },                               \
		{ 0x5170, "enbl_dpsa" },                               \
		{ 0x5187, "cf_volume" },                               \
		{ 0x5222, "ctrl_cc" },                                 \
		{ 0x5257, "gain" },                                    \
		{ 0x52d0, "ctrl_slopectrl" },                          \
		{ 0x52e0, "ctrl_slope" },                              \
		{ 0x5301, "dpsa_level" },                              \
		{ 0x5321, "dpsa_release" },                            \
		{ 0x5340, "clipfast" },                                \
		{ 0x5350, "bypass_lp" },                               \
		{ 0x5360, "enbl_low_latency" },                        \
		{ 0x5400, "first_order_mode" },                        \
		{ 0x5410, "bypass_ctrlloop" },                         \
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
		{ 0x5c07, "flag_cf_tap_pattern" },                     \
		{ 0x5c83, "tap_debug_info" },                          \
		{ 0x5d0f, "tap_status_1" },                            \
		{ 0x5f03, "tap_comp_threshold" },                      \
		{ 0x6081, "pga_gain_set" },                            \
		{ 0x60b0, "pga_lowpass_enable" },                      \
		{ 0x60c0, "pga_pwr_enable" },                          \
		{ 0x60d0, "pga_switch_enable" },                       \
		{ 0x60e0, "pga_switch_aux_enable" },                   \
		{ 0x6123, "ctrl_att" },                                \
		{ 0x6265, "zero_lvl" },                                \
		{ 0x62c1, "ctrl_fb_resistor" },                        \
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
		{ 0x6630, "dcdcoff_mode" },                            \
		{ 0x6656, "dcdc_vbat_delta_detect" },                  \
		{ 0x66c0, "dcdc_ignore_vbat" },                        \
		{ 0x6700, "enbl_minion" },                             \
		{ 0x6713, "vth_vddpvbat" },                            \
		{ 0x6750, "lpen_vddpvbat" },                           \
		{ 0x6761, "ctrl_rfb" },                                \
		{ 0x6801, "tdm_source_mapping" },                      \
		{ 0x6821, "tdm_sourcea_frame_sel" },                   \
		{ 0x6841, "tdm_sourceb_frame_sel" },                   \
		{ 0x6901, "sam_mode" },                                \
		{ 0x6931, "pdmdat_h_sel" },                            \
		{ 0x6951, "pdmdat_l_sel" },                            \
		{ 0x6970, "cs_sam_set" },                              \
		{ 0x6980, "cs_adc_nortz" },                            \
		{ 0x6990, "sam_spkr_sel" },                            \
		{ 0x6b00, "disable_engage" },                          \
		{ 0x6c02, "ns_hp2ln_criterion" },                      \
		{ 0x6c32, "ns_ln2hp_criterion" },                      \
		{ 0x6c60, "sel_clip_pwms" },                           \
		{ 0x6c72, "pwms_clip_lvl" },                           \
		{ 0x6ca5, "spare_out" },                               \
		{ 0x6d0f, "spare_in" },                                \
		{ 0x6e10, "flag_lp_detect_mode1" },                    \
		{ 0x6e20, "flag_low_amplitude" },                      \
		{ 0x6e30, "flag_vddp_gt_vbat" },                       \
		{ 0x7033, "boost_cur" },                               \
		{ 0x7071, "bst_slpcmplvl" },                           \
		{ 0x7090, "boost_intel" },                             \
		{ 0x70a0, "boost_speed" },                             \
		{ 0x70c1, "dcdc_sel" },                                \
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
		{ 0x7430, "boost_track" },                             \
		{ 0x7494, "boost_hold_time" },                         \
		{ 0x74e0, "sel_dcdc_envelope_8fs" },                   \
		{ 0x74f0, "ignore_flag_voutcomp86" },                  \
		{ 0x7504, "boost_trip_lvl_1st" },                      \
		{ 0x7554, "boost_trip_lvl_2nd" },                      \
		{ 0x75a4, "boost_trip_lvl_track" },                    \
		{ 0x7602, "track_decay" },                             \
		{ 0x7635, "frst_boost_voltage" },                      \
		{ 0x7695, "scnd_boost_voltage" },                      \
		{ 0x7720, "pga_test_ldo_bypass" },                     \
		{ 0x8001, "sel_clk_cs" },                              \
		{ 0x8021, "micadc_speed" },                            \
		{ 0x8050, "cs_gain_control" },                         \
		{ 0x8060, "cs_bypass_gc" },                            \
		{ 0x8087, "cs_gain" },                                 \
		{ 0x8200, "enbl_cmfb" },                               \
		{ 0x8210, "invertpwm" },                               \
		{ 0x8222, "cmfb_gain" },                               \
		{ 0x8256, "cmfb_offset" },                             \
		{ 0x8305, "cs_ktemp" },                                \
		{ 0x8364, "cs_ktemp2" },                               \
		{ 0x8400, "cs_adc_bsoinv" },                           \
		{ 0x8421, "cs_adc_hifreq" },                           \
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
		{ 0x8902, "cursense_comp_delay" },                     \
		{ 0x8930, "cursense_comp_sign" },                      \
		{ 0x8940, "enbl_cursense_comp" },                      \
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
		{ 0x9380, "cf_ack_cmd" },                              \
		{ 0x9390, "cf_ack_reset" },                            \
		{ 0x93a0, "cf_ack_mips" },                             \
		{ 0x93b0, "cf_ack_mute_ready" },                       \
		{ 0x93c0, "cf_ack_volume_ready" },                     \
		{ 0x93d0, "cf_ack_damage" },                           \
		{ 0x93e0, "cf_ack_calibrate_ready" },                  \
		{ 0x93f0, "cf_ack_reserved" },                         \
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
		{ 0xc130, "tap_comp_enable" },                         \
		{ 0xc140, "tap_comp_switch_enable" },                  \
		{ 0xc150, "tap_comp_switch_aux_enable" },              \
		{ 0xc161, "tap_comp_test_enable" },                    \
		{ 0xc180, "curdist_enable" },                          \
		{ 0xc190, "vbg2i_enbl" },                              \
		{ 0xc1a0, "bg_filt_bypass_enbl" },                     \
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
		{ 0xc560, "bypass_fro8" },                             \
		{ 0xc570, "test_enbl_cs" },                            \
		{ 0xc5b0, "pga_test_enable" },                         \
		{ 0xc5c0, "pga_test_offset_enable" },                  \
		{ 0xc5d0, "pga_test_shortinput_enable" },              \
		{ 0xc600, "enbl_pwm_dcc" },                            \
		{ 0xc613, "pwm_dcc_cnt" },                             \
		{ 0xc650, "enbl_ldo_stress" },                         \
		{ 0xc660, "enbl_powerswitch" },                        \
		{ 0xc707, "digimuxa_sel" },                            \
		{ 0xc787, "digimuxb_sel" },                            \
		{ 0xc807, "digimuxc_sel" },                            \
		{ 0xc901, "dio1_ehs" },                                \
		{ 0xc921, "dio2_ehs" },                                \
		{ 0xc941, "dio3_ehs" },                                \
		{ 0xc961, "dio4_ehs" },                                \
		{ 0xc981, "spdmo_ehs" },                               \
		{ 0xc9a1, "tdo_ehs" },                                 \
		{ 0xc9c0, "int_ehs" },                                 \
		{ 0xc9d0, "pdmclk_ehs" },                              \
		{ 0xc9e0, "fs2_ehs" },                                 \
		{ 0xc9f0, "hs_mode" },                                 \
		{ 0xca00, "enbl_anamux1" },                            \
		{ 0xca10, "enbl_anamux2" },                            \
		{ 0xca20, "enbl_anamux3" },                            \
		{ 0xca30, "enbl_anamux4" },                            \
		{ 0xca74, "anamux1" },                                 \
		{ 0xcb04, "anamux2" },                                 \
		{ 0xcb54, "anamux3" },                                 \
		{ 0xcba4, "anamux4" },                                 \
		{ 0xcc05, "pll_seli_lbw" },                            \
		{ 0xcc64, "pll_selp_lbw" },                            \
		{ 0xccb3, "pll_selr_lbw" },                            \
		{ 0xccf0, "sel_user_pll_bw" },                         \
		{ 0xcdf0, "pll_frm" },                                 \
		{ 0xce09, "pll_ndec" },                                \
		{ 0xcea0, "pll_mdec_msb" },                            \
		{ 0xceb0, "enbl_pll" },                                \
		{ 0xcec0, "enbl_fro8" },                               \
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
		{ 0xd580, "enbl_clk_range_chk" },                      \
		{ 0xd601, "clkdiv_dsp_sel" },                          \
		{ 0xd621, "clkdiv_audio_sel" },                        \
		{ 0xd641, "clkdiv_muxa_sel" },                         \
		{ 0xd661, "clkdiv_muxb_sel" },                         \
		{ 0xd681, "dsp_tap_clk" },                             \
		{ 0xd6a1, "sel_wdt_clk" },                             \
		{ 0xd6c1, "sel_tim_clk" },                             \
		{ 0xd700, "ads1_ehs" },                                \
		{ 0xd710, "ads2_ehs" },                                \
		{ 0xd822, "test_parametric_io" },                      \
		{ 0xd850, "ctrl_bst_clk_lp1" },                        \
		{ 0xd861, "test_spare_out1" },                         \
		{ 0xd880, "bst_dcmbst" },                              \
		{ 0xd8a1, "force_pga_clock" },                         \
		{ 0xd8c3, "test_spare_out2" },                         \
		{ 0xd900, "overrules_usercase" },                      \
		{ 0xd910, "ovr_switch_ref" },                          \
		{ 0xd920, "ovr_enbl_pll" },                            \
		{ 0xd930, "ovr_switch_amp" },                          \
		{ 0xd940, "ovr_enbl_clk_cs" },                         \
		{ 0xd951, "ovr_sel_clk_cs" },                          \
		{ 0xd970, "ovr_switch_cs" },                           \
		{ 0xd980, "ovr_enbl_csvs_ss" },                        \
		{ 0xd990, "ovr_enbl_comp" },                           \
		{ 0xed00, "enbl_fro8cal" },                            \
		{ 0xed10, "start_fro8_calibration" },                  \
		{ 0xed20, "fro8_calibration_done" },                   \
		{ 0xed45, "fro8_auto_trim_val" },                      \
		{ 0xee0f, "sw_profile" },                              \
		{ 0xef0f, "sw_vstep" },                                \
		{ 0xf000, "calibration_onetime" },                     \
		{ 0xf010, "calibr_ron_done" },                         \
		{ 0xf020, "calibr_dcdc_api_calibrate" },               \
		{ 0xf030, "calibr_dcdc_delta_sign" },                  \
		{ 0xf042, "calibr_dcdc_delta" },                       \
		{ 0xf078, "calibr_speaker_info" },                     \
		{ 0xf105, "calibr_vout_offset" },                      \
		{ 0xf163, "spare_mtp1_9_6" },                          \
		{ 0xf1a5, "spare_mtp1_15_10" },                        \
		{ 0xf203, "calibr_gain" },                             \
		{ 0xf245, "calibr_offset" },                           \
		{ 0xf2a3, "spare_mtp2_13_10" },                        \
		{ 0xf307, "spare_mtp3_7_0" },                          \
		{ 0xf387, "calibr_gain_cs" },                          \
		{ 0xf40f, "calibr_R25C" },                             \
		{ 0xf50f, "spare_mtp5_15_0" },                         \
		{ 0xf600, "mtp_lock_enbl_coolflux" },                  \
		{ 0xf610, "mtp_pwm_delay_enbl_clk_auto_gating" },      \
		{ 0xf620, "mtp_ocp_enbl_clk_auto_gating" },            \
		{ 0xf630, "mtp_disable_clk_a_gating" },                \
		{ 0xf642, "spare_mtp6_6_3" },                          \
		{ 0xf686, "spare_mtp6_14_8" },                         \
		{ 0xf706, "ctrl_offset_a" },                           \
		{ 0xf786, "ctrl_offset_b" },                           \
		{ 0xf806, "htol_iic_addr" },                           \
		{ 0xf870, "htol_iic_addr_en" },                        \
		{ 0xf884, "calibr_temp_offset" },                      \
		{ 0xf8d2, "calibr_temp_gain" },                        \
		{ 0xf910, "disable_sam_mode" },                        \
		{ 0xf920, "mtp_lock_bypass_clipper" },                 \
		{ 0xf930, "mtp_lock_max_dcdc_voltage" },               \
		{ 0xf943, "calibr_vbg_trim" },                         \
		{ 0xf987, "type_bits_fw" },                            \
		{ 0xfa0f, "mtpdataA" },                                \
		{ 0xfb0f, "mtpdataB" },                                \
		{ 0xfc0f, "mtpdataC" },                                \
		{ 0xfd0f, "mtpdataD" },                                \
		{ 0xfe0f, "mtpdataE" },                                \
		{ 0xff05, "fro8_trim" },                               \
		{ 0xff61, "fro8_short_nwell_r" },                      \
		{ 0xff81, "fro8_boost_i" },                            \
		{ 0xffff, "Unknown bitfield enum" }                    \
	}
enum tfa9912_irq {
	tfa9912_irq_stvdds = 0,
	tfa9912_irq_stplls = 1,
	tfa9912_irq_stotds = 2,
	tfa9912_irq_stovds = 3,
	tfa9912_irq_stuvds = 4,
	tfa9912_irq_stclks = 5,
	tfa9912_irq_stmtpb = 6,
	tfa9912_irq_stnoclk = 7,
	tfa9912_irq_stspks = 8,
	tfa9912_irq_stacs = 9,
	tfa9912_irq_stsws = 10,
	tfa9912_irq_stwds = 11,
	tfa9912_irq_stamps = 12,
	tfa9912_irq_starefs = 13,
	tfa9912_irq_stadccr = 14,
	tfa9912_irq_stbodnok = 15,
	tfa9912_irq_stbstcu = 16,
	tfa9912_irq_stbsthi = 17,
	tfa9912_irq_stbstoc = 18,
	tfa9912_irq_stbstpkcur = 19,
	tfa9912_irq_stbstvc = 20,
	tfa9912_irq_stbst86 = 21,
	tfa9912_irq_stbst93 = 22,
	tfa9912_irq_strcvld = 23,
	tfa9912_irq_stocpl = 24,
	tfa9912_irq_stocpr = 25,
	tfa9912_irq_stmwsrc = 26,
	tfa9912_irq_stmwcfc = 27,
	tfa9912_irq_stmwsmu = 28,
	tfa9912_irq_stcfmer = 29,
	tfa9912_irq_stcfmac = 30,
	tfa9912_irq_stclkoor = 31,
	tfa9912_irq_sttdmer = 32,
	tfa9912_irq_stclpl = 33,
	tfa9912_irq_stclpr = 34,
	tfa9912_irq_stocpm = 35,
	tfa9912_irq_stlp1 = 37,
	tfa9912_irq_stla = 38,
	tfa9912_irq_stvddp = 39,
	tfa9912_irq_sttapdet = 40,
	tfa9912_irq_staudmod = 41,
	tfa9912_irq_stsammod = 42,
	tfa9912_irq_sttapmod = 43,
	tfa9912_irq_sttaptrg = 44,
	tfa9912_irq_max = 45,
	tfa9912_irq_all = -1
};
#define TFA9912_IRQ_NAMETABLE                                                 \
	static struct TfaIrqName Tfa9912IrqNames[] = {                        \
		{ 0, "STVDDS" },    { 1, "STPLLS" },      { 2, "STOTDS" },    \
		{ 3, "STOVDS" },    { 4, "STUVDS" },      { 5, "STCLKS" },    \
		{ 6, "STMTPB" },    { 7, "STNOCLK" },     { 8, "STSPKS" },    \
		{ 9, "STACS" },     { 10, "STSWS" },      { 11, "STWDS" },    \
		{ 12, "STAMPS" },   { 13, "STAREFS" },    { 14, "STADCCR" },  \
		{ 15, "STBODNOK" }, { 16, "STBSTCU" },    { 17, "STBSTHI" },  \
		{ 18, "STBSTOC" },  { 19, "STBSTPKCUR" }, { 20, "STBSTVC" },  \
		{ 21, "STBST86" },  { 22, "STBST93" },    { 23, "STRCVLD" },  \
		{ 24, "STOCPL" },   { 25, "STOCPR" },     { 26, "STMWSRC" },  \
		{ 27, "STMWCFC" },  { 28, "STMWSMU" },    { 29, "STCFMER" },  \
		{ 30, "STCFMAC" },  { 31, "STCLKOOR" },   { 32, "STTDMER" },  \
		{ 33, "STCLPL" },   { 34, "STCLPR" },     { 35, "STOCPM" },   \
		{ 36, "36" },       { 37, "STLP1" },      { 38, "STLA" },     \
		{ 39, "STVDDP" },   { 40, "STTAPDET" },   { 41, "STAUDMOD" }, \
		{ 42, "STSAMMOD" }, { 43, "STTAPMOD" },   { 44, "STTAPTRG" }, \
		{ 45, "45" },                                                 \
	}
#endif
