/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/* ************************** */
/* Include Header File */
/* ************************** */
/* #define		OISINI */

/* #include      "Main.h" */
/* #include      "Cmd.h" */
#include "Ois.h"
#include "OisDef.h"
#include "OisFil.h"

unsigned short UsCntXof; /* OPTICAL Center Xvalue */
unsigned short UsCntYof; /* OPTICAL Center Yvalue */
unsigned char UcPwmMod;  /* PWM MODE */
unsigned char UcCvrCod;  /* CverCode */

const struct STFILREG CsFilReg[] = {{0x0111, 0x00}, /*00,0111 */
				   {0x0113, 0x00}, /*00,0113 */
				   {0x0114, 0x00}, /*00,0114 */
				   {0x0172, 0x00}, /*00,0172 */
				   {0x01E3, 0x00}, /*00,01E3 */
				   {0x01E4, 0x00}, /*00,01E4 */
				   {0xFFFF, 0xFF} };

/* 32bit */
const struct STFILRAM CsFilRam[] = {
	{0x1000, 0x3F800000}, /*3F800000,1000,0dB,invert=0 */
	{0x1001, 0x3F800000}, /*3F800000,1001,0dB,invert=0 */
	{0x1002, 0x00000000}, /*00000000,1002,Cutoff,invert=0 */
	{0x1003, 0x3F800000}, /*3F800000,1003,0dB,invert=0 */
	{0x1004, 0x3828A700}, /*3828A700,1004,LPF,0.3Hz,0dB,fs/1,invert=0 */
	{0x1005, 0x3828A700}, /*3828A700,1005,LPF,0.3Hz,0dB,fs/1,invert=0 */
	{0x1006, 0x3F7FFAC0}, /*3F7FFAC0,1006,LPF,0.3Hz,0dB,fs/1,invert=0 */
	{0x1007, 0x3F800000}, /*3F800000,1007,0dB,invert=0 */
	{0x1008, 0xBF800000}, /*BF800000,1008,0dB,invert=1 */
	{0x1009, 0x3F800000}, /*3F800000,1009,0dB,invert=0 */
	{0x100A, 0x3F800000}, /*3F800000,100A,0dB,invert=0 */
	{0x100B, 0x3F800000}, /*3F800000,100B,0dB,invert=0 */
	{0x100C, 0x3F800000}, /*3F800000,100C,0dB,invert=0 */
	{0x100E, 0x3F800000}, /*3F800000,100E,0dB,invert=0 */
	{0x1010, 0x3DA2AD80}, /*3DA2AD80,1010 */
	{0x1011, 0x00000000}, /*00000000,1011,Free,fs/1,invert=0 */
	{0x1012, 0x3F7FFE00}, /*3F7FFE00,1012,Free,fs/1,invert=0 */
	{0x1013,
	0x3FB26DC0}, /*3FB26DC0,1013,HBF,50Hz,150Hz,3dB,fs/1,invert=0 */
	{0x1014,
	0xBFB00DC0}, /*BFB00DC0,1014,HBF,50Hz,150Hz,3dB,fs/1,invert=0 */
	{0x1015,
	0x3F75E8C0}, /*3F75E8C0,1015,HBF,50Hz,150Hz,3dB,fs/1,invert=0 */
	{0x1016,
	0x3F1B2780}, /*3F1B2780,1016,LBF,0.2Hz,0.33Hz,0dB,fs/1,invert=0 */
	{0x1017,
	0xBF1B2400}, /*BF1B2400,1017,LBF,0.2Hz,0.33Hz,0dB,fs/1,invert=0 */
	{0x1018,
	0x3F7FFC80}, /*3F7FFC80,1018,LBF,0.2Hz,0.33Hz,0dB,fs/1,invert=0 */
	{0x1019, 0x3F800000}, /*3F800000,1019,Through,0dB,fs/1,invert=0 */
	{0x101A, 0x00000000}, /*00000000,101A,Through,0dB,fs/1,invert=0 */
	{0x101B, 0x00000000}, /*00000000,101B,Through,0dB,fs/1,invert=0 */
	{0x101C, 0x3F800000}, /*3F800000,101C,0dB,invert=0 */
	{0x101D, 0x00000000}, /*00000000,101D,Cutoff,invert=0 */
	{0x101E, 0x3F800000}, /*3F800000,101E,0dB,invert=0 */
	{0x1020, 0x3F800000}, /*3F800000,1020,0dB,invert=0 */
	{0x1021, 0x3F800000}, /*3F800000,1021,0dB,invert=0 */
	{0x1022, 0x3F800000}, /*3F800000,1022,0dB,invert=0 */
	{0x1023, 0x3F800000}, /*3F800000,1023,Through,0dB,fs/1,invert=0 */
	{0x1024, 0x00000000}, /*00000000,1024,Through,0dB,fs/1,invert=0 */
	{0x1025, 0x00000000}, /*00000000,1025,Through,0dB,fs/1,invert=0 */
	{0x1026, 0x00000000}, /*00000000,1026,Through,0dB,fs/1,invert=0 */
	{0x1027, 0x00000000}, /*00000000,1027,Through,0dB,fs/1,invert=0 */
	{0x1030, 0x3F800000}, /*3F800000,1030,Through,0dB,fs/1,invert=0 */
	{0x1031, 0x00000000}, /*00000000,1031,Through,0dB,fs/1,invert=0 */
	{0x1032, 0x00000000}, /*00000000,1032,Through,0dB,fs/1,invert=0 */
	{0x1033, 0x3F800000}, /*3F800000,1033,Through,0dB,fs/1,invert=0 */
	{0x1034, 0x00000000}, /*00000000,1034,Through,0dB,fs/1,invert=0 */
	{0x1035, 0x00000000}, /*00000000,1035,Through,0dB,fs/1,invert=0 */
	{0x1036, 0x3F800000}, /*3F800000,1036,Through,0dB,fs/1,invert=0 */
	{0x1037, 0x00000000}, /*00000000,1037,Through,0dB,fs/1,invert=0 */
	{0x1038, 0x00000000}, /*00000000,1038,Through,0dB,fs/1,invert=0 */
	{0x1039, 0x3F800000}, /*3F800000,1039,Through,0dB,fs/1,invert=0 */
	{0x103A, 0x00000000}, /*00000000,103A,Through,0dB,fs/1,invert=0 */
	{0x103B, 0x00000000}, /*00000000,103B,Through,0dB,fs/1,invert=0 */
	{0x103C, 0x3F800000}, /*3F800000,103C,Through,0dB,fs/1,invert=0 */
	{0x103D, 0x00000000}, /*00000000,103D,Through,0dB,fs/1,invert=0 */
	{0x103E, 0x00000000}, /*00000000,103E,Through,0dB,fs/1,invert=0 */
	{0x1043, 0x39D2BD40}, /*39D2BD40,1043,LPF,3Hz,0dB,fs/1,invert=0 */
	{0x1044, 0x39D2BD40}, /*39D2BD40,1044,LPF,3Hz,0dB,fs/1,invert=0 */
	{0x1045, 0x3F7FCB40}, /*3F7FCB40,1045,LPF,3Hz,0dB,fs/1,invert=0 */
	{0x1046, 0x388C8A40}, /*388C8A40,1046,LPF,0.5Hz,0dB,fs/1,invert=0 */
	{0x1047, 0x388C8A40}, /*388C8A40,1047,LPF,0.5Hz,0dB,fs/1,invert=0 */
	{0x1048, 0x3F7FF740}, /*3F7FF740,1048,LPF,0.5Hz,0dB,fs/1,invert=0 */
	{0x1049, 0x390C87C0}, /*390C87C0,1049,LPF,1Hz,0dB,fs/1,invert=0 */
	{0x104A, 0x390C87C0}, /*390C87C0,104A,LPF,1Hz,0dB,fs/1,invert=0 */
	{0x104B, 0x3F7FEE80}, /*3F7FEE80,104B,LPF,1Hz,0dB,fs/1,invert=0 */
	{0x104C, 0x398C8300}, /*398C8300,104C,LPF,2Hz,0dB,fs/1,invert=0 */
	{0x104D, 0x398C8300}, /*398C8300,104D,LPF,2Hz,0dB,fs/1,invert=0 */
	{0x104E, 0x3F7FDCC0}, /*3F7FDCC0,104E,LPF,2Hz,0dB,fs/1,invert=0 */
	{0x1053, 0x3F800000}, /*3F800000,1053,Through,0dB,fs/1,invert=0 */
	{0x1054, 0x00000000}, /*00000000,1054,Through,0dB,fs/1,invert=0 */
	{0x1055, 0x00000000}, /*00000000,1055,Through,0dB,fs/1,invert=0 */
	{0x1056, 0x3F800000}, /*3F800000,1056,Through,0dB,fs/1,invert=0 */
	{0x1057, 0x00000000}, /*00000000,1057,Through,0dB,fs/1,invert=0 */
	{0x1058, 0x00000000}, /*00000000,1058,Through,0dB,fs/1,invert=0 */
	{0x1059, 0x3F800000}, /*3F800000,1059,Through,0dB,fs/1,invert=0 */
	{0x105A, 0x00000000}, /*00000000,105A,Through,0dB,fs/1,invert=0 */
	{0x105B, 0x00000000}, /*00000000,105B,Through,0dB,fs/1,invert=0 */
	{0x105C, 0x3F800000}, /*3F800000,105C,Through,0dB,fs/1,invert=0 */
	{0x105D, 0x00000000}, /*00000000,105D,Through,0dB,fs/1,invert=0 */
	{0x105E, 0x00000000}, /*00000000,105E,Through,0dB,fs/1,invert=0 */
	{0x1063, 0x3F800000}, /*3F800000,1063,0dB,invert=0 */
	{0x1066, 0x3F800000}, /*3F800000,1066,0dB,invert=0 */
	{0x1069, 0x3F800000}, /*3F800000,1069,0dB,invert=0 */
	{0x106C, 0x3F800000}, /*3F800000,106C,0dB,invert=0 */
	{0x1073, 0x00000000}, /*00000000,1073,Cutoff,invert=0 */
	{0x1076, 0x3F800000}, /*3F800000,1076,0dB,invert=0 */
	{0x1079, 0x3F800000}, /*3F800000,1079,0dB,invert=0 */
	{0x107C, 0x3F800000}, /*3F800000,107C,0dB,invert=0 */
	{0x1083, 0x38D1B700}, /*38D1B700,1083,-80dB,invert=0 */
	{0x1086, 0x00000000}, /*00000000,1086,Cutoff,invert=0 */
	{0x1089, 0x00000000}, /*00000000,1089,Cutoff,invert=0 */
	{0x108C, 0x00000000}, /*00000000,108C,Cutoff,invert=0 */
	{0x1093, 0x00000000}, /*00000000,1093,Cutoff,invert=0 */
	{0x1098, 0x3F800000}, /*3F800000,1098,0dB,invert=0 */
	{0x1099, 0x3F800000}, /*3F800000,1099,0dB,invert=0 */
	{0x109A, 0x3F800000}, /*3F800000,109A,0dB,invert=0 */
	{0x10A1, 0x3C58B440}, /*3C58B440,10A1,LPF,100Hz,0dB,fs/1,invert=0 */
	{0x10A2, 0x3C58B440}, /*3C58B440,10A2,LPF,100Hz,0dB,fs/1,invert=0 */
	{0x10A3, 0x3F793A40}, /*3F793A40,10A3,LPF,100Hz,0dB,fs/1,invert=0 */
	{0x10A4, 0x3C58B440}, /*3C58B440,10A4,LPF,100Hz,0dB,fs/1,invert=0 */
	{0x10A5, 0x3C58B440}, /*3C58B440,10A5,LPF,100Hz,0dB,fs/1,invert=0 */
	{0x10A6, 0x3F793A40}, /*3F793A40,10A6,LPF,100Hz,0dB,fs/1,invert=0 */
	{0x10A7, 0x3F800000}, /*3F800000,10A7,Through,0dB,fs/1,invert=0 */
	{0x10A8, 0x00000000}, /*00000000,10A8,Through,0dB,fs/1,invert=0 */
	{0x10A9, 0x00000000}, /*00000000,10A9,Through,0dB,fs/1,invert=0 */
	{0x10AA, 0x00000000}, /*00000000,10AA,Cutoff,invert=0 */
	{0x10AB, 0x3BDA2580}, /*3BDA2580,10AB,LPF,50Hz,0dB,fs/1,invert=0 */
	{0x10AC, 0x3BDA2580}, /*3BDA2580,10AC,LPF,50Hz,0dB,fs/1,invert=0 */
	{0x10AD, 0x3F7C9780}, /*3F7C9780,10AD,LPF,50Hz,0dB,fs/1,invert=0 */
	{0x10B0, 0x3DD17800}, /*3DD17800,10B0,LPF,850Hz,0dB,fs/1,invert=0 */
	{0x10B1, 0x3DD17800}, /*3DD17800,10B1,LPF,850Hz,0dB,fs/1,invert=0 */
	{0x10B2, 0x3F4BA200}, /*3F4BA200,10B2,LPF,850Hz,0dB,fs/1,invert=0 */
	{0x10B3, 0x3F800000}, /*3F800000,10B3,0dB,invert=0 */
	{0x10B4, 0x00000000}, /*00000000,10B4,Cutoff,invert=0 */
	{0x10B5, 0x00000000}, /*00000000,10B5,Cutoff,invert=0 */
	{0x10B6, 0x3F353C00}, /*3F353C00,10B6,-3dB,invert=0 */
	{0x10B8, 0x3F800000}, /*3F800000,10B8,0dB,invert=0 */
	{0x10B9, 0x00000000}, /*00000000,10B9,Cutoff,invert=0 */
	{0x10C0,
	0x3F915680}, /*3F915680,10C0,HBF,80Hz,900Hz,2dB,fs/1,invert=0 */
	{0x10C1,
	0xBF8E4100}, /*BF8E4100,10C1,HBF,80Hz,900Hz,2dB,fs/1,invert=0 */
	{0x10C2,
	0x3F48E240}, /*3F48E240,10C2,HBF,80Hz,900Hz,2dB,fs/1,invert=0 */
	{0x10C3,
	0x3FA04B40}, /*3FA04B40,10C3,HBF,100Hz,140Hz,2dB,fs/1,invert=0 */
	{0x10C4,
	0xBF9C0DC0}, /*BF9C0DC0,10C4,HBF,100Hz,140Hz,2dB,fs/1,invert=0 */
	{0x10C5,
	0x3F7691C0}, /*3F7691C0,10C5,HBF,100Hz,140Hz,2dB,fs/1,invert=0 */
	{0x10C6, 0x3D506F00}, /*3D506F00,10C6,LPF,400Hz,0dB,fs/1,invert=0 */
	{0x10C7, 0x3D506F00}, /*3D506F00,10C7,LPF,400Hz,0dB,fs/1,invert=0 */
	{0x10C8, 0x3F65F240}, /*3F65F240,10C8,LPF,400Hz,0dB,fs/1,invert=0 */
	{0x10C9, 0x3C208400}, /*3C208400,10C9,LPF,1.3Hz,35dB,fs/1,invert=0 */
	{0x10CA, 0x3C208400}, /*3C208400,10CA,LPF,1.3Hz,35dB,fs/1,invert=0 */
	{0x10CB, 0x3F7FE940}, /*3F7FE940,10CB,LPF,1.3Hz,35dB,fs/1,invert=0 */
	{0x10CC,
	0x3E196280}, /*3E196280,10CC,LBF,15Hz,40Hz,-8dB,fs/1,invert=0 */
	{0x10CD,
	0xBE17BF80}, /*BE17BF80,10CD,LBF,15Hz,40Hz,-8dB,fs/1,invert=0 */
	{0x10CE,
	0x3F7EF900}, /*3F7EF900,10CE,LBF,15Hz,40Hz,-8dB,fs/1,invert=0 */
	{0x10D0, 0x3FFF64C0}, /*3FFF64C0,10D0,6dB,invert=0 */
	{0x10D1, 0x00000000}, /*00000000,10D1,Cutoff,invert=0 */
	{0x10D2, 0x3F800000}, /*3F800000,10D2,0dB,invert=0 */
	{0x10D3, 0x3F800000}, /*3F800000,10D3,0dB,invert=0 */
	{0x10D4, 0x3F800000}, /*3F800000,10D4,0dB,invert=0 */
	{0x10D5, 0x3F800000}, /*3F800000,10D5,0dB,invert=0 */
	{0x10D7, 0x41618840}, /*41618840,10D7,LPF,6000Hz,30dB,fs/1,invert=0 */
	{0x10D8, 0x41618840}, /*41618840,10D8,LPF,6000Hz,30dB,fs/1,invert=0 */
	{0x10D9, 0x3DDE3840}, /*3DDE3840,10D9,LPF,6000Hz,30dB,fs/1,invert=0 */
	{0x10DA,
	0x3F672280}, /*3F672280,10DA,PKF,1000Hz,-10dB,3,fs/1,invert=0 */
	{0x10DB,
	0xBFD3E1C0}, /*BFD3E1C0,10DB,PKF,1000Hz,-10dB,3,fs/1,invert=0 */
	{0x10DC,
	0x3FD3E1C0}, /*3FD3E1C0,10DC,PKF,1000Hz,-10dB,3,fs/1,invert=0 */
	{0x10DD,
	0x3F5022C0}, /*3F5022C0,10DD,PKF,1000Hz,-10dB,3,fs/1,invert=0 */
	{0x10DE,
	0xBF374540}, /*BF374540,10DE,PKF,1000Hz,-10dB,3,fs/1,invert=0 */
	{0x10E0, 0x3F800000}, /*3F800000,10E0,Through,0dB,fs/1,invert=0 */
	{0x10E1, 0x00000000}, /*00000000,10E1,Through,0dB,fs/1,invert=0 */
	{0x10E2, 0x00000000}, /*00000000,10E2,Through,0dB,fs/1,invert=0 */
	{0x10E3, 0x00000000}, /*00000000,10E3,Through,0dB,fs/1,invert=0 */
	{0x10E4, 0x00000000}, /*00000000,10E4,Through,0dB,fs/1,invert=0 */
	{0x10E5, 0x3F800000}, /*3F800000,10E5,0dB,invert=0 */
	{0x10E8, 0x3F800000}, /*3F800000,10E8,0dB,invert=0 */
	{0x10E9, 0x00000000}, /*00000000,10E9,Cutoff,invert=0 */
	{0x10EA, 0x00000000}, /*00000000,10EA,Cutoff,invert=0 */
	{0x10EB, 0x00000000}, /*00000000,10EB,Cutoff,invert=0 */
	{0x10F0, 0x3F800000}, /*3F800000,10F0,Through,0dB,fs/1,invert=0 */
	{0x10F1, 0x00000000}, /*00000000,10F1,Through,0dB,fs/1,invert=0 */
	{0x10F2, 0x00000000}, /*00000000,10F2,Through,0dB,fs/1,invert=0 */
	{0x10F3, 0x00000000}, /*00000000,10F3,Through,0dB,fs/1,invert=0 */
	{0x10F4, 0x00000000}, /*00000000,10F4,Through,0dB,fs/1,invert=0 */
	{0x10F5, 0x3F800000}, /*3F800000,10F5,Through,0dB,fs/1,invert=0 */
	{0x10F6, 0x00000000}, /*00000000,10F6,Through,0dB,fs/1,invert=0 */
	{0x10F7, 0x00000000}, /*00000000,10F7,Through,0dB,fs/1,invert=0 */
	{0x10F8, 0x00000000}, /*00000000,10F8,Through,0dB,fs/1,invert=0 */
	{0x10F9, 0x00000000}, /*00000000,10F9,Through,0dB,fs/1,invert=0 */
	{0x1100, 0x3F800000}, /*3F800000,1100,0dB,invert=0 */
	{0x1101, 0x3F800000}, /*3F800000,1101,0dB,invert=0 */
	{0x1102, 0x00000000}, /*00000000,1102,Cutoff,invert=0 */
	{0x1103, 0x3F800000}, /*3F800000,1103,0dB,invert=0 */
	{0x1104, 0x3828A700}, /*3828A700,1104,LPF,0.3Hz,0dB,fs/1,invert=0 */
	{0x1105, 0x3828A700}, /*3828A700,1105,LPF,0.3Hz,0dB,fs/1,invert=0 */
	{0x1106, 0x3F7FFAC0}, /*3F7FFAC0,1106,LPF,0.3Hz,0dB,fs/1,invert=0 */
	{0x1107, 0x3F800000}, /*3F800000,1107,0dB,invert=0 */
	{0x1108, 0xBF800000}, /*BF800000,1108,0dB,invert=1 */
	{0x1109, 0x3F800000}, /*3F800000,1109,0dB,invert=0 */
	{0x110A, 0x3F800000}, /*3F800000,110A,0dB,invert=0 */
	{0x110B, 0x3F800000}, /*3F800000,110B,0dB,invert=0 */
	{0x110C, 0x3F800000}, /*3F800000,110C,0dB,invert=0 */
	{0x110E, 0x3F800000}, /*3F800000,110E,0dB,invert=0 */
	{0x1110, 0x3DA2AD80}, /*3DA2AD80,1110 */
	{0x1111, 0x00000000}, /*00000000,1111,Free,fs/1,invert=0 */
	{0x1112, 0x3F7FFE00}, /*3F7FFE00,1112,Free,fs/1,invert=0 */
	{0x1113,
	0x3FB26DC0}, /*3FB26DC0,1113,HBF,50Hz,150Hz,3dB,fs/1,invert=0 */
	{0x1114,
	0xBFB00DC0}, /*BFB00DC0,1114,HBF,50Hz,150Hz,3dB,fs/1,invert=0 */
	{0x1115,
	0x3F75E8C0}, /*3F75E8C0,1115,HBF,50Hz,150Hz,3dB,fs/1,invert=0 */
	{0x1116,
	0x3F1B2780}, /*3F1B2780,1116,LBF,0.2Hz,0.33Hz,0dB,fs/1,invert=0 */
	{0x1117,
	0xBF1B2400}, /*BF1B2400,1117,LBF,0.2Hz,0.33Hz,0dB,fs/1,invert=0 */
	{0x1118,
	0x3F7FFC80}, /*3F7FFC80,1118,LBF,0.2Hz,0.33Hz,0dB,fs/1,invert=0 */
	{0x1119, 0x3F800000}, /*3F800000,1119,Through,0dB,fs/1,invert=0 */
	{0x111A, 0x00000000}, /*00000000,111A,Through,0dB,fs/1,invert=0 */
	{0x111B, 0x00000000}, /*00000000,111B,Through,0dB,fs/1,invert=0 */
	{0x111C, 0x3F800000}, /*3F800000,111C,0dB,invert=0 */
	{0x111D, 0x00000000}, /*00000000,111D,Cutoff,invert=0 */
	{0x111E, 0x3F800000}, /*3F800000,111E,0dB,invert=0 */
	{0x1120, 0x3F800000}, /*3F800000,1120,0dB,invert=0 */
	{0x1121, 0x3F800000}, /*3F800000,1121,0dB,invert=0 */
	{0x1122, 0x3F800000}, /*3F800000,1122,0dB,invert=0 */
	{0x1123, 0x3F800000}, /*3F800000,1123,Through,0dB,fs/1,invert=0 */
	{0x1124, 0x00000000}, /*00000000,1124,Through,0dB,fs/1,invert=0 */
	{0x1125, 0x00000000}, /*00000000,1125,Through,0dB,fs/1,invert=0 */
	{0x1126, 0x00000000}, /*00000000,1126,Through,0dB,fs/1,invert=0 */
	{0x1127, 0x00000000}, /*00000000,1127,Through,0dB,fs/1,invert=0 */
	{0x1130, 0x3F800000}, /*3F800000,1130,Through,0dB,fs/1,invert=0 */
	{0x1131, 0x00000000}, /*00000000,1131,Through,0dB,fs/1,invert=0 */
	{0x1132, 0x00000000}, /*00000000,1132,Through,0dB,fs/1,invert=0 */
	{0x1133, 0x3F800000}, /*3F800000,1133,Through,0dB,fs/1,invert=0 */
	{0x1134, 0x00000000}, /*00000000,1134,Through,0dB,fs/1,invert=0 */
	{0x1135, 0x00000000}, /*00000000,1135,Through,0dB,fs/1,invert=0 */
	{0x1136, 0x3F800000}, /*3F800000,1136,Through,0dB,fs/1,invert=0 */
	{0x1137, 0x00000000}, /*00000000,1137,Through,0dB,fs/1,invert=0 */
	{0x1138, 0x00000000}, /*00000000,1138,Through,0dB,fs/1,invert=0 */
	{0x1139, 0x3F800000}, /*3F800000,1139,Through,0dB,fs/1,invert=0 */
	{0x113A, 0x00000000}, /*00000000,113A,Through,0dB,fs/1,invert=0 */
	{0x113B, 0x00000000}, /*00000000,113B,Through,0dB,fs/1,invert=0 */
	{0x113C, 0x3F800000}, /*3F800000,113C,Through,0dB,fs/1,invert=0 */
	{0x113D, 0x00000000}, /*00000000,113D,Through,0dB,fs/1,invert=0 */
	{0x113E, 0x00000000}, /*00000000,113E,Through,0dB,fs/1,invert=0 */
	{0x1143, 0x39D2BD40}, /*39D2BD40,1143,LPF,3Hz,0dB,fs/1,invert=0 */
	{0x1144, 0x39D2BD40}, /*39D2BD40,1144,LPF,3Hz,0dB,fs/1,invert=0 */
	{0x1145, 0x3F7FCB40}, /*3F7FCB40,1145,LPF,3Hz,0dB,fs/1,invert=0 */
	{0x1146, 0x388C8A40}, /*388C8A40,1146,LPF,0.5Hz,0dB,fs/1,invert=0 */
	{0x1147, 0x388C8A40}, /*388C8A40,1147,LPF,0.5Hz,0dB,fs/1,invert=0 */
	{0x1148, 0x3F7FF740}, /*3F7FF740,1148,LPF,0.5Hz,0dB,fs/1,invert=0 */
	{0x1149, 0x390C87C0}, /*390C87C0,1149,LPF,1Hz,0dB,fs/1,invert=0 */
	{0x114A, 0x390C87C0}, /*390C87C0,114A,LPF,1Hz,0dB,fs/1,invert=0 */
	{0x114B, 0x3F7FEE80}, /*3F7FEE80,114B,LPF,1Hz,0dB,fs/1,invert=0 */
	{0x114C, 0x398C8300}, /*398C8300,114C,LPF,2Hz,0dB,fs/1,invert=0 */
	{0x114D, 0x398C8300}, /*398C8300,114D,LPF,2Hz,0dB,fs/1,invert=0 */
	{0x114E, 0x3F7FDCC0}, /*3F7FDCC0,114E,LPF,2Hz,0dB,fs/1,invert=0 */
	{0x1153, 0x3F800000}, /*3F800000,1153,Through,0dB,fs/1,invert=0 */
	{0x1154, 0x00000000}, /*00000000,1154,Through,0dB,fs/1,invert=0 */
	{0x1155, 0x00000000}, /*00000000,1155,Through,0dB,fs/1,invert=0 */
	{0x1156, 0x3F800000}, /*3F800000,1156,Through,0dB,fs/1,invert=0 */
	{0x1157, 0x00000000}, /*00000000,1157,Through,0dB,fs/1,invert=0 */
	{0x1158, 0x00000000}, /*00000000,1158,Through,0dB,fs/1,invert=0 */
	{0x1159, 0x3F800000}, /*3F800000,1159,Through,0dB,fs/1,invert=0 */
	{0x115A, 0x00000000}, /*00000000,115A,Through,0dB,fs/1,invert=0 */
	{0x115B, 0x00000000}, /*00000000,115B,Through,0dB,fs/1,invert=0 */
	{0x115C, 0x3F800000}, /*3F800000,115C,Through,0dB,fs/1,invert=0 */
	{0x115D, 0x00000000}, /*00000000,115D,Through,0dB,fs/1,invert=0 */
	{0x115E, 0x00000000}, /*00000000,115E,Through,0dB,fs/1,invert=0 */
	{0x1163, 0x3F800000}, /*3F800000,1163,0dB,invert=0 */
	{0x1166, 0x3F800000}, /*3F800000,1166,0dB,invert=0 */
	{0x1169, 0x3F800000}, /*3F800000,1169,0dB,invert=0 */
	{0x116C, 0x3F800000}, /*3F800000,116C,0dB,invert=0 */
	{0x1173, 0x00000000}, /*00000000,1173,Cutoff,invert=0 */
	{0x1176, 0x3F800000}, /*3F800000,1176,0dB,invert=0 */
	{0x1179, 0x3F800000}, /*3F800000,1179,0dB,invert=0 */
	{0x117C, 0x3F800000}, /*3F800000,117C,0dB,invert=0 */
	{0x1183, 0x38D1B700}, /*38D1B700,1183,-80dB,invert=0 */
	{0x1186, 0x00000000}, /*00000000,1186,Cutoff,invert=0 */
	{0x1189, 0x00000000}, /*00000000,1189,Cutoff,invert=0 */
	{0x118C, 0x00000000}, /*00000000,118C,Cutoff,invert=0 */
	{0x1193, 0x00000000}, /*00000000,1193,Cutoff,invert=0 */
	{0x1198, 0x3F800000}, /*3F800000,1198,0dB,invert=0 */
	{0x1199, 0x3F800000}, /*3F800000,1199,0dB,invert=0 */
	{0x119A, 0x3F800000}, /*3F800000,119A,0dB,invert=0 */
	{0x11A1, 0x3C58B440}, /*3C58B440,11A1,LPF,100Hz,0dB,fs/1,invert=0 */
	{0x11A2, 0x3C58B440}, /*3C58B440,11A2,LPF,100Hz,0dB,fs/1,invert=0 */
	{0x11A3, 0x3F793A40}, /*3F793A40,11A3,LPF,100Hz,0dB,fs/1,invert=0 */
	{0x11A4, 0x3C58B440}, /*3C58B440,11A4,LPF,100Hz,0dB,fs/1,invert=0 */
	{0x11A5, 0x3C58B440}, /*3C58B440,11A5,LPF,100Hz,0dB,fs/1,invert=0 */
	{0x11A6, 0x3F793A40}, /*3F793A40,11A6,LPF,100Hz,0dB,fs/1,invert=0 */
	{0x11A7, 0x3F800000}, /*3F800000,11A7,Through,0dB,fs/1,invert=0 */
	{0x11A8, 0x00000000}, /*00000000,11A8,Through,0dB,fs/1,invert=0 */
	{0x11A9, 0x00000000}, /*00000000,11A9,Through,0dB,fs/1,invert=0 */
	{0x11AA, 0x00000000}, /*00000000,11AA,Cutoff,invert=0 */
	{0x11AB, 0x3BDA2580}, /*3BDA2580,11AB,LPF,50Hz,0dB,fs/1,invert=0 */
	{0x11AC, 0x3BDA2580}, /*3BDA2580,11AC,LPF,50Hz,0dB,fs/1,invert=0 */
	{0x11AD, 0x3F7C9780}, /*3F7C9780,11AD,LPF,50Hz,0dB,fs/1,invert=0 */
	{0x11B0, 0x3DD17800}, /*3DD17800,11B0,LPF,850Hz,0dB,fs/1,invert=0 */
	{0x11B1, 0x3DD17800}, /*3DD17800,11B1,LPF,850Hz,0dB,fs/1,invert=0 */
	{0x11B2, 0x3F4BA200}, /*3F4BA200,11B2,LPF,850Hz,0dB,fs/1,invert=0 */
	{0x11B3, 0x3F800000}, /*3F800000,11B3,0dB,invert=0 */
	{0x11B4, 0x00000000}, /*00000000,11B4,Cutoff,invert=0 */
	{0x11B5, 0x00000000}, /*00000000,11B5,Cutoff,invert=0 */
	{0x11B6, 0x3F353C00}, /*3F353C00,11B6,-3dB,invert=0 */
	{0x11B8, 0x3F800000}, /*3F800000,11B8,0dB,invert=0 */
	{0x11B9, 0x00000000}, /*00000000,11B9,Cutoff,invert=0 */
	{0x11C0,
	0x3F915680}, /*3F915680,11C0,HBF,80Hz,900Hz,2dB,fs/1,invert=0 */
	{0x11C1,
	0xBF8E4100}, /*BF8E4100,11C1,HBF,80Hz,900Hz,2dB,fs/1,invert=0 */
	{0x11C2,
	0x3F48E240}, /*3F48E240,11C2,HBF,80Hz,900Hz,2dB,fs/1,invert=0 */
	{0x11C3,
	0x3FA04B40}, /*3FA04B40,11C3,HBF,100Hz,140Hz,2dB,fs/1,invert=0 */
	{0x11C4,
	0xBF9C0DC0}, /*BF9C0DC0,11C4,HBF,100Hz,140Hz,2dB,fs/1,invert=0 */
	{0x11C5,
	0x3F7691C0}, /*3F7691C0,11C5,HBF,100Hz,140Hz,2dB,fs/1,invert=0 */
	{0x11C6, 0x3D506F00}, /*3D506F00,11C6,LPF,400Hz,0dB,fs/1,invert=0 */
	{0x11C7, 0x3D506F00}, /*3D506F00,11C7,LPF,400Hz,0dB,fs/1,invert=0 */
	{0x11C8, 0x3F65F240}, /*3F65F240,11C8,LPF,400Hz,0dB,fs/1,invert=0 */
	{0x11C9, 0x3C208400}, /*3C208400,11C9,LPF,1.3Hz,35dB,fs/1,invert=0 */
	{0x11CA, 0x3C208400}, /*3C208400,11CA,LPF,1.3Hz,35dB,fs/1,invert=0 */
	{0x11CB, 0x3F7FE940}, /*3F7FE940,11CB,LPF,1.3Hz,35dB,fs/1,invert=0 */
	{0x11CC,
	0x3E196280}, /*3E196280,11CC,LBF,15Hz,40Hz,-8dB,fs/1,invert=0 */
	{0x11CD,
	0xBE17BF80}, /*BE17BF80,11CD,LBF,15Hz,40Hz,-8dB,fs/1,invert=0 */
	{0x11CE,
	0x3F7EF900}, /*3F7EF900,11CE,LBF,15Hz,40Hz,-8dB,fs/1,invert=0 */
	{0x11D0, 0x3FFF64C0}, /*3FFF64C0,11D0,6dB,invert=0 */
	{0x11D1, 0x00000000}, /*00000000,11D1,Cutoff,invert=0 */
	{0x11D2, 0x3F800000}, /*3F800000,11D2,0dB,invert=0 */
	{0x11D3, 0x3F800000}, /*3F800000,11D3,0dB,invert=0 */
	{0x11D4, 0x3F800000}, /*3F800000,11D4,0dB,invert=0 */
	{0x11D5, 0x3F800000}, /*3F800000,11D5,0dB,invert=0 */
	{0x11D7, 0x41618840}, /*41618840,11D7,LPF,6000Hz,30dB,fs/1,invert=0 */
	{0x11D8, 0x41618840}, /*41618840,11D8,LPF,6000Hz,30dB,fs/1,invert=0 */
	{0x11D9, 0x3DDE3840}, /*3DDE3840,11D9,LPF,6000Hz,30dB,fs/1,invert=0 */
	{0x11DA,
	0x3F672280}, /*3F672280,11DA,PKF,1000Hz,-10dB,3,fs/1,invert=0 */
	{0x11DB,
	0xBFD3E1C0}, /*BFD3E1C0,11DB,PKF,1000Hz,-10dB,3,fs/1,invert=0 */
	{0x11DC,
	0x3FD3E1C0}, /*3FD3E1C0,11DC,PKF,1000Hz,-10dB,3,fs/1,invert=0 */
	{0x11DD,
	0x3F5022C0}, /*3F5022C0,11DD,PKF,1000Hz,-10dB,3,fs/1,invert=0 */
	{0x11DE,
	0xBF374540}, /*BF374540,11DE,PKF,1000Hz,-10dB,3,fs/1,invert=0 */
	{0x11E0, 0x3F800000}, /*3F800000,11E0,Through,0dB,fs/1,invert=0 */
	{0x11E1, 0x00000000}, /*00000000,11E1,Through,0dB,fs/1,invert=0 */
	{0x11E2, 0x00000000}, /*00000000,11E2,Through,0dB,fs/1,invert=0 */
	{0x11E3, 0x00000000}, /*00000000,11E3,Through,0dB,fs/1,invert=0 */
	{0x11E4, 0x00000000}, /*00000000,11E4,Through,0dB,fs/1,invert=0 */
	{0x11E5, 0x3F800000}, /*3F800000,11E5,0dB,invert=0 */
	{0x11E8, 0x3F800000}, /*3F800000,11E8,0dB,invert=0 */
	{0x11E9, 0x00000000}, /*00000000,11E9,Cutoff,invert=0 */
	{0x11EA, 0x00000000}, /*00000000,11EA,Cutoff,invert=0 */
	{0x11EB, 0x00000000}, /*00000000,11EB,Cutoff,invert=0 */
	{0x11F0, 0x3F800000}, /*3F800000,11F0,Through,0dB,fs/1,invert=0 */
	{0x11F1, 0x00000000}, /*00000000,11F1,Through,0dB,fs/1,invert=0 */
	{0x11F2, 0x00000000}, /*00000000,11F2,Through,0dB,fs/1,invert=0 */
	{0x11F3, 0x00000000}, /*00000000,11F3,Through,0dB,fs/1,invert=0 */
	{0x11F4, 0x00000000}, /*00000000,11F4,Through,0dB,fs/1,invert=0 */
	{0x11F5, 0x3F800000}, /*3F800000,11F5,Through,0dB,fs/1,invert=0 */
	{0x11F6, 0x00000000}, /*00000000,11F6,Through,0dB,fs/1,invert=0 */
	{0x11F7, 0x00000000}, /*00000000,11F7,Through,0dB,fs/1,invert=0 */
	{0x11F8, 0x00000000}, /*00000000,11F8,Through,0dB,fs/1,invert=0 */
	{0x11F9, 0x00000000}, /*00000000,11F9,Through,0dB,fs/1,invert=0 */
	{0x1200, 0x00000000}, /*00000000,1200,Cutoff,invert=0 */
	{0x1201, 0x3F800000}, /*3F800000,1201,0dB,invert=0 */
	{0x1202, 0x3F800000}, /*3F800000,1202,0dB,invert=0 */
	{0x1203, 0x3F800000}, /*3F800000,1203,0dB,invert=0 */
	{0x1204, 0x3F800000}, /*3F800000,1204,Through,0dB,fs/1,invert=0 */
	{0x1205, 0x00000000}, /*00000000,1205,Through,0dB,fs/1,invert=0 */
	{0x1206, 0x00000000}, /*00000000,1206,Through,0dB,fs/1,invert=0 */
	{0x1207, 0x3F800000}, /*3F800000,1207,Through,0dB,fs/1,invert=0 */
	{0x1208, 0x00000000}, /*00000000,1208,Through,0dB,fs/1,invert=0 */
	{0x1209, 0x00000000}, /*00000000,1209,Through,0dB,fs/1,invert=0 */
	{0x120A, 0x3F800000}, /*3F800000,120A,Through,0dB,fs/1,invert=0 */
	{0x120B, 0x00000000}, /*00000000,120B,Through,0dB,fs/1,invert=0 */
	{0x120C, 0x00000000}, /*00000000,120C,Through,0dB,fs/1,invert=0 */
	{0x120D, 0x3F800000}, /*3F800000,120D,Through,0dB,fs/1,invert=0 */
	{0x120E, 0x00000000}, /*00000000,120E,Through,0dB,fs/1,invert=0 */
	{0x120F, 0x00000000}, /*00000000,120F,Through,0dB,fs/1,invert=0 */
	{0x1210, 0x3F800000}, /*3F800000,1210,Through,0dB,fs/1,invert=0 */
	{0x1211, 0x00000000}, /*00000000,1211,Through,0dB,fs/1,invert=0 */
	{0x1212, 0x00000000}, /*00000000,1212,Through,0dB,fs/1,invert=0 */
	{0x1213, 0x3F800000}, /*3F800000,1213,0dB,invert=0 */
	{0x1214, 0x3F800000}, /*3F800000,1214,0dB,invert=0 */
	{0x1215, 0x3F800000}, /*3F800000,1215,0dB,invert=0 */
	{0x1216, 0x3F800000}, /*3F800000,1216,0dB,invert=0 */
	{0x1217, 0x3F800000}, /*3F800000,1217,0dB,invert=0 */
	{0x1218, 0x00000000}, /*00000000,1218,Cutoff,fs/1,invert=0 */
	{0x1219, 0x00000000}, /*00000000,1219,Cutoff,fs/1,invert=0 */
	{0x121A, 0x00000000}, /*00000000,121A,Cutoff,fs/1,invert=0 */
	{0x121B, 0x00000000}, /*00000000,121B,Cutoff,fs/1,invert=0 */
	{0x121C, 0x00000000}, /*00000000,121C,Cutoff,fs/1,invert=0 */
	{0x121D, 0x3F800000}, /*3F800000,121D,0dB,invert=0 */
	{0x121E, 0x3F800000}, /*3F800000,121E,0dB,invert=0 */
	{0x121F, 0x3F800000}, /*3F800000,121F,0dB,invert=0 */
	{0x1235, 0x3F800000}, /*3F800000,1235,0dB,invert=0 */
	{0x1236, 0x3F800000}, /*3F800000,1236,0dB,invert=0 */
	{0x1237, 0x3F800000}, /*3F800000,1237,0dB,invert=0 */
	{0x1238, 0x3F800000}, /*3F800000,1238,0dB,invert=0 */
	{0xFFFF, 0xFFFFFFFF} };

void IniSet(void)
{
	/* Command Execute Process Initial */
	IniCmd();
	/* Clock Setting */
	IniClk();
	/* I/O Port Initial Setting */
	IniIop();
	/* DigitalGyro Initial Setting */
	IniDgy();
	/* Monitor & Other Initial Setting */
	IniMon();
	/* Servo Initial Setting */
	IniSrv();
	/* Gyro Filter Initial Setting */
	IniGyr();
	/* Gyro Filter Initial Setting */
	IniFil();
	/* Adjust Fix Value Setting */
	IniAdj();
}

void IniSetAf(void)
{
	/* Command Execute Process Initial */
	IniCmd();
	/* Clock Setting */
	IniClk();
	/* AF Initial Setting */
	IniAf();
}

void IniClk(void)
{
	ChkCvr(); /* Read Cver */

	/*OSC Enables */
	UcOscAdjFlg = 0; /* Osc adj flag */

#ifdef DEF_SET
	/*OSC ENABLE */
	RegWriteA_LC898122AF(OSCSTOP, 0x00);  /* 0x0256 */
	RegWriteA_LC898122AF(OSCSET, 0x90);   /* 0x0257       OSC ini */
	RegWriteA_LC898122AF(OSCCNTEN, 0x00); /* 0x0258       OSC Cnt disable */
#endif
	/*Clock Enables */
	RegWriteA_LC898122AF(CLKON, 0x1F); /* 0x020B */

#ifdef USE_EXTCLK_ALL
	RegWriteA_LC898122AF(CLKSEL, 0x07); /* 0x020C       All */
#else
#ifdef USE_EXTCLK_PWM
	RegWriteA_LC898122AF(CLKSEL, 0x01); /* 0x020C       only PWM */
#else
#ifdef DEF_SET
	RegWriteA_LC898122AF(CLKSEL, 0x00); /* 0x020C */
#endif
#endif
#endif

#ifdef USE_EXTCLK_ALL			     /* 24MHz */
	RegWriteA_LC898122AF(PWMDIV, 0x00);   /* 0x0210       24MHz/1 */
	RegWriteA_LC898122AF(SRVDIV, 0x00);   /* 0x0211       24MHz/1 */
	RegWriteA_LC898122AF(GIFDIV, 0x02);   /* 0x0212       24MHz/2 = 12MHz */
	RegWriteA_LC898122AF(AFPWMDIV, 0x00); /* 0x0213       24MHz/1 = 24MHz */
	RegWriteA_LC898122AF(OPAFDIV, 0x02);  /* 0x0214       24MHz/2 = 12MHz */
#else
#ifdef DEF_SET
	RegWriteA_LC898122AF(PWMDIV, 0x00); /* 0x0210       48MHz/1 */
	RegWriteA_LC898122AF(SRVDIV, 0x00); /* 0x0211       48MHz/1 */
	RegWriteA_LC898122AF(GIFDIV, 0x03); /* 0x0212       48MHz/3 = 16MHz */
#ifdef AF_PWMMODE
	RegWriteA_LC898122AF(AFPWMDIV, 0x00); /* 0x0213       48MHz/1 */
#else
	RegWriteA_LC898122AF(AFPWMDIV, 0x02); /* 0x0213       48MHz/2 = 24MHz */
#endif
	RegWriteA_LC898122AF(OPAFDIV, 0x04);  /* 0x0214       48MHz/4 = 12MHz */
#endif
#endif
}

void IniIop(void)
{
#ifdef DEF_SET
	/*set IOP direction */
	RegWriteA_LC898122AF(P0LEV, 0x00);
	RegWriteA_LC898122AF(P0DIR, 0x00);
	/*set pull up/down */
	RegWriteA_LC898122AF(P0PON, 0x0F);
	RegWriteA_LC898122AF(P0PUD, 0x0F);
#endif
/*select IOP signal */
#ifdef USE_3WIRE_DGYRO
	RegWriteA_LC898122AF(IOP1SEL, 0x02); /* 0x0231       IOP1 : IOP1 */
#else
	RegWriteA_LC898122AF(
		IOP1SEL,
		0x00); /* 0x0231       IOP1 : DGDATAIN (ATT:0236h[0]=1) */
#endif
#ifdef DEF_SET
	RegWriteA_LC898122AF(IOP0SEL, 0x02); /* 0x0230       IOP0 : IOP0 */
	RegWriteA_LC898122AF(IOP2SEL, 0x02); /* 0x0232       IOP2 : IOP2 */
	RegWriteA_LC898122AF(IOP3SEL, 0x00); /* 0x0233       IOP3 : DGDATAOUT */
	RegWriteA_LC898122AF(IOP4SEL, 0x00); /* 0x0234       IOP4 : DGSCLK */
	RegWriteA_LC898122AF(IOP5SEL, 0x00); /* 0x0235       IOP5 : DGSSB */
	RegWriteA_LC898122AF(DGINSEL,
			    0x00); /* 0x0236       DGDATAIN 0:IOP1 1:IOP2 */
	RegWriteA_LC898122AF(I2CSEL,
			    0x00); /* 0x0248       I2C noise reduction ON */
	RegWriteA_LC898122AF(DLMODE, 0x00); /* 0x0249       Download OFF */
#endif
}

void IniDgy(void)
{
#ifdef USE_INVENSENSE
	unsigned char UcGrini;
#endif

/*************/
/*For ST gyro */
/*************/

/*Set SPI Type */
#ifdef USE_3WIRE_DGYRO
	RegWriteA_LC898122AF(SPIM, 0x00);
#else
	RegWriteA_LC898122AF(SPIM, 0x01);
#endif
	/* DGSPI4  0: 3-wire SPI, 1: 4-wire SPI */

	/*Set to Command Mode */
	RegWriteA_LC898122AF(GRSEL, 0x01);

	/*Digital Gyro Read settings */
	RegWriteA_LC898122AF(GRINI, 0x80);

#ifdef USE_INVENSENSE

	RegReadA_LC898122AF(GRINI, &UcGrini);
	RegWriteA_LC898122AF(GRINI, (UcGrini | SLOWMODE));

	RegWriteA_LC898122AF(GRADR0, 0x6A); /* 0x0283       Set I2C_DIS */
	RegWriteA_LC898122AF(GSETDT, 0x10); /* 0x028A       Set Write Data */
	RegWriteA_LC898122AF(GRACC, 0x10); /* 0x0282       Set Trigger ON */
	AccWit(0x10);

	RegWriteA_LC898122AF(GRADR0, 0x1B); /* 0x0283       Set GYRO_CONFIG */
	RegWriteA_LC898122AF(GSETDT,
			    (FS_SEL << 3)); /* 0x028A       Set Write Data */
	RegWriteA_LC898122AF(GRACC, 0x10); /* 0x0282       Set Trigger ON */
	AccWit(0x10); /* Digital Gyro busy wait                               */

	RegReadA_LC898122AF(GRINI, &UcGrini);
	RegWriteA_LC898122AF(GRINI, (UcGrini & ~SLOWMODE));
/* 0x0281       [ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ] */

#endif

	RegWriteA_LC898122AF(
		RDSEL,
		0x7C); /* 0x028B       RDSEL(Data1 and 2 for continuos mode) */

	GyOutSignal();
}

void IniMon(void)
{
	RegWriteA_LC898122AF(PWMMONA, 0x00); /* 0x0030       0:off */

	RegWriteA_LC898122AF(MONSELA, 0x5C); /* 0x0270       DLYMON1 */
	RegWriteA_LC898122AF(MONSELB, 0x5D); /* 0x0271       DLYMON2 */
	RegWriteA_LC898122AF(MONSELC, 0x00); /* 0x0272 */
	RegWriteA_LC898122AF(MONSELD, 0x00); /* 0x0273 */

	/* Monitor Circuit */
	RegWriteA_LC898122AF(WC_PINMON1,
			    0x00); /* 0x01C0               Filter Monitor */
	RegWriteA_LC898122AF(WC_PINMON2, 0x00); /* 0x01C1 */
	RegWriteA_LC898122AF(WC_PINMON3, 0x00); /* 0x01C2 */
	RegWriteA_LC898122AF(WC_PINMON4, 0x00); /* 0x01C3 */
	/* Delay Monitor */
	RegWriteA_LC898122AF(WC_DLYMON11, 0x04); /* 0x01C5 DlyMonAdd1[10:8] */
	RegWriteA_LC898122AF(WC_DLYMON10, 0x40); /* 0x01C4 DlyMonAdd1[ 7:0] */
	RegWriteA_LC898122AF(WC_DLYMON21, 0x04); /* 0x01C7 DlyMonAdd2[10:8] */
	RegWriteA_LC898122AF(WC_DLYMON20, 0xC0); /* 0x01C6 DlyMonAdd2[ 7:0] */
	RegWriteA_LC898122AF(WC_DLYMON31, 0x00); /* 0x01C9 DlyMonAdd3[10:8] */
	RegWriteA_LC898122AF(WC_DLYMON30, 0x00); /* 0x01C8 DlyMonAdd3[ 7:0] */
	RegWriteA_LC898122AF(WC_DLYMON41, 0x00); /* 0x01CB DlyMonAdd4[10:8] */
	RegWriteA_LC898122AF(WC_DLYMON40, 0x00); /* 0x01CA DlyMonAdd4[ 7:0] */

	/* Monitor */
	RegWriteA_LC898122AF(PWMMONA, 0x80); /* 0x0030       1:on */
 /**/}


void IniSrv(void)
{
	unsigned char UcStbb0;

	UcPwmMod = INIT_PWMMODE; /* Driver output mode */

	RegWriteA_LC898122AF(WC_EQON,
			     0x00); /* 0x0101               Filter Calcu */
	RegWriteA_LC898122AF(WC_RAMINITON, 0x00); /* 0x0102 */
	ClrGyr(0x0000, CLR_ALL_RAM);		  /* All Clear */

	RegWriteA_LC898122AF(WH_EQSWX, 0x02);
	RegWriteA_LC898122AF(WH_EQSWY, 0x02);

	RamAccFixMod(OFF); /* 32bit Float mode */

	/* Monitor Gain */
	RamWrite32A_LC898122AF(dm1g, 0x3F800000); /* 0x109A */
	RamWrite32A_LC898122AF(dm2g, 0x3F800000); /* 0x109B */
	RamWrite32A_LC898122AF(dm3g, 0x3F800000); /* 0x119A */
	RamWrite32A_LC898122AF(dm4g, 0x3F800000); /* 0x119B */

	/* Hall output limitter */
	RamWrite32A_LC898122AF(
		sxlmta1,
		0x3F800000); /* 0x10E6               Hall X output Limit */
	RamWrite32A_LC898122AF(
		sylmta1,
		0x3F800000); /* 0x11E6               Hall Y output Limit */

	/* Emargency Stop */
	RegWriteA_LC898122AF(
		WH_EMGSTPON,
		0x00); /* 0x0178               Emargency Stop OFF */
	RegWriteA_LC898122AF(WH_EMGSTPTMR,
			     0xFF); /* 0x017A 255*(16/23.4375kHz)=174ms */

	RamWrite32A_LC898122AF(sxemglev, 0x3F800000);
	RamWrite32A_LC898122AF(syemglev, 0x3F800000);

	/* Hall Servo smoothing */
	RegWriteA_LC898122AF(WH_SMTSRVON,
			     0x00); /* 0x017C               Smooth Servo OFF */
#ifdef USE_EXTCLK_ALL		    /* 24MHz */
	RegWriteA_LC898122AF(WH_SMTSRVSMP,
			     0x03); /* 0x017D 2.7ms=2^03/11.718kHz */
	RegWriteA_LC898122AF(WH_SMTTMR,
			     0x00); /* 0x017E 1.3ms=(0+1)*16/11.718kHz */
#else
	RegWriteA_LC898122AF(WH_SMTSRVSMP,
			     0x06); /* 0x017D 2.7ms=2^06/23.4375kHz */
	RegWriteA_LC898122AF(WH_SMTTMR,
			     0x01); /* 0x017E 1.3ms=(1+1)*16/23.4375kHz */
#endif

	RamWrite32A_LC898122AF(sxsmtav, 0xBC800000);
	RamWrite32A_LC898122AF(sysmtav, 0xBC800000);
	RamWrite32A_LC898122AF(sxsmtstp, 0x3AE90466);
	RamWrite32A_LC898122AF(sysmtstp, 0x3AE90466);

	/* High-dimensional correction  */
	RegWriteA_LC898122AF(WH_HOFCON,
			     0x11); /* 0x0174               OUT 3x3 */

	/* Front */
	RamWrite32A_LC898122AF(sxiexp3, A3_IEXP3);   /* 0x10BA */
	RamWrite32A_LC898122AF(sxiexp2, 0x00000000); /* 0x10BB */
	RamWrite32A_LC898122AF(sxiexp1, A1_IEXP1);   /* 0x10BC */
	RamWrite32A_LC898122AF(sxiexp0, 0x00000000); /* 0x10BD */
	RamWrite32A_LC898122AF(sxiexp, 0x3F800000);  /* 0x10BE */

	RamWrite32A_LC898122AF(syiexp3, A3_IEXP3);   /* 0x11BA */
	RamWrite32A_LC898122AF(syiexp2, 0x00000000); /* 0x11BB */
	RamWrite32A_LC898122AF(syiexp1, A1_IEXP1);   /* 0x11BC */
	RamWrite32A_LC898122AF(syiexp0, 0x00000000); /* 0x11BD */
	RamWrite32A_LC898122AF(syiexp, 0x3F800000);  /* 0x11BE */

	/* Back */
	RamWrite32A_LC898122AF(sxoexp3, A3_IEXP3);   /* 0x10FA */
	RamWrite32A_LC898122AF(sxoexp2, 0x00000000); /* 0x10FB */
	RamWrite32A_LC898122AF(sxoexp1, A1_IEXP1);   /* 0x10FC */
	RamWrite32A_LC898122AF(sxoexp0, 0x00000000); /* 0x10FD */
	RamWrite32A_LC898122AF(sxoexp, 0x3F800000);  /* 0x10FE */

	RamWrite32A_LC898122AF(syoexp3, A3_IEXP3);   /* 0x11FA */
	RamWrite32A_LC898122AF(syoexp2, 0x00000000); /* 0x11FB */
	RamWrite32A_LC898122AF(syoexp1, A1_IEXP1);   /* 0x11FC */
	RamWrite32A_LC898122AF(syoexp0, 0x00000000); /* 0x11FD */
	RamWrite32A_LC898122AF(syoexp, 0x3F800000);  /* 0x11FE */

 /* Sine wave */
#ifdef DEF_SET
	RegWriteA_LC898122AF(WC_SINON,
			     0x00); /* 0x0180               Sin Wave off */
	RegWriteA_LC898122AF(WC_SINFRQ0, 0x00); /* 0x0181 */
	RegWriteA_LC898122AF(WC_SINFRQ1, 0x60); /* 0x0182 */
	RegWriteA_LC898122AF(WC_SINPHSX, 0x00); /* 0x0183 */
	RegWriteA_LC898122AF(WC_SINPHSY, 0x20); /* 0x0184 */

	/* AD over sampling */
	RegWriteA_LC898122AF(WC_ADMODE,
			     0x06); /* 0x0188               AD Over Sampling */

	/* Measure mode */
	RegWriteA_LC898122AF(WC_MESMODE, 0x00); /* 0x0190 Measurement Mode */
	RegWriteA_LC898122AF(WC_MESSINMODE, 0x00); /* 0x0191 */
	RegWriteA_LC898122AF(WC_MESLOOP0, 0x08);   /* 0x0192 */
	RegWriteA_LC898122AF(WC_MESLOOP1, 0x02);   /* 0x0193 */
	RegWriteA_LC898122AF(WC_MES1ADD0, 0x00);   /* 0x0194 */
	RegWriteA_LC898122AF(WC_MES1ADD1, 0x00);   /* 0x0195 */
	RegWriteA_LC898122AF(WC_MES2ADD0, 0x00);   /* 0x0196 */
	RegWriteA_LC898122AF(WC_MES2ADD1, 0x00);   /* 0x0197 */
	RegWriteA_LC898122AF(WC_MESABS, 0x00);     /* 0x0198 */
	RegWriteA_LC898122AF(WC_MESWAIT, 0x00);    /* 0x0199 */

	/* auto measure */
	RegWriteA_LC898122AF(
		WC_AMJMODE,
		0x00); /* 0x01A0               Automatic measurement mode */

	RegWriteA_LC898122AF(WC_AMJLOOP0, 0x08); /* 0x01A2 Self-Aadjustment */
	RegWriteA_LC898122AF(WC_AMJLOOP1, 0x02); /* 0x01A3 */
	RegWriteA_LC898122AF(WC_AMJIDL0, 0x02);  /* 0x01A4 */
	RegWriteA_LC898122AF(WC_AMJIDL1, 0x00);  /* 0x01A5 */
	RegWriteA_LC898122AF(WC_AMJ1ADD0, 0x00); /* 0x01A6 */
	RegWriteA_LC898122AF(WC_AMJ1ADD1, 0x00); /* 0x01A7 */
	RegWriteA_LC898122AF(WC_AMJ2ADD0, 0x00); /* 0x01A8 */
	RegWriteA_LC898122AF(WC_AMJ2ADD1, 0x00); /* 0x01A9 */

	/* Data Pass */
	RegWriteA_LC898122AF(WC_DPI1ADD0,
			     0x00); /* 0x01B0               Data Pass */
	RegWriteA_LC898122AF(WC_DPI1ADD1, 0x00); /* 0x01B1 */
	RegWriteA_LC898122AF(WC_DPI2ADD0, 0x00); /* 0x01B2 */
	RegWriteA_LC898122AF(WC_DPI2ADD1, 0x00); /* 0x01B3 */
	RegWriteA_LC898122AF(WC_DPI3ADD0, 0x00); /* 0x01B4 */
	RegWriteA_LC898122AF(WC_DPI3ADD1, 0x00); /* 0x01B5 */
	RegWriteA_LC898122AF(WC_DPI4ADD0, 0x00); /* 0x01B6 */
	RegWriteA_LC898122AF(WC_DPI4ADD1, 0x00); /* 0x01B7 */
	RegWriteA_LC898122AF(WC_DPO1ADD0,
			     0x00); /* 0x01B8               Data Pass */
	RegWriteA_LC898122AF(WC_DPO1ADD1, 0x00); /* 0x01B9 */
	RegWriteA_LC898122AF(WC_DPO2ADD0, 0x00); /* 0x01BA */
	RegWriteA_LC898122AF(WC_DPO2ADD1, 0x00); /* 0x01BB */
	RegWriteA_LC898122AF(WC_DPO3ADD0, 0x00); /* 0x01BC */
	RegWriteA_LC898122AF(WC_DPO3ADD1, 0x00); /* 0x01BD */
	RegWriteA_LC898122AF(WC_DPO4ADD0, 0x00); /* 0x01BE */
	RegWriteA_LC898122AF(WC_DPO4ADD1, 0x00); /* 0x01BF */
	RegWriteA_LC898122AF(WC_DPON,
			     0x00); /* 0x0105               Data pass OFF */

	/* Interrupt Flag */
	RegWriteA_LC898122AF(WC_INTMSK,
			     0xFF); /* 0x01CE               All Mask */

#endif

	/* Ram Access */
	RamAccFixMod(OFF); /* 32bit float mode */

	/* PWM Signal Generate */
	DrvSw(OFF); /* 0x0070       Drvier Block Ena=0 */
	RegWriteA_LC898122AF(
		DRVFC2, 0x90); /* 0x0002       Slope 3, Dead Time = 30 ns */
	RegWriteA_LC898122AF(
		DRVSELX,
		0xFF); /* 0x0003       PWM X drv max current  DRVSELX[7:0] */
	RegWriteA_LC898122AF(
		DRVSELY,
		0xFF); /* 0x0004       PWM Y drv max current  DRVSELY[7:0] */

#ifdef PWM_BREAK
#ifdef PWM_CAREER_TEST
	RegWriteA_LC898122AF(PWMFC, 0x7C);
#else  /* PWM_CAREER_TEST */
	if (UcCvrCod == CVER122)
		RegWriteA_LC898122AF(PWMFC, 0x2D);
	else
		RegWriteA_LC898122AF(PWMFC, 0x3D);
#endif /* PWM_CAREER_TEST */
#else
	RegWriteA_LC898122AF(PWMFC, 0x21);
#endif

#ifdef USE_VH_SYNC
	RegWriteA_LC898122AF(STROBEFC,
			     0x80); /* 0x001C       外?入力Strobe信?の有効 */
	RegWriteA_LC898122AF(STROBEDLYX, 0x00); /* 0x001D       Delay */
	RegWriteA_LC898122AF(STROBEDLYY, 0x00); /* 0x001E       Delay */
#endif						/* USE_VH_SYNC */

	RegWriteA_LC898122AF(PWMA, 0x00); /* 0x0010       PWM X/Y standby */
	RegWriteA_LC898122AF(PWMDLYX,
			     0x04); /* 0x0012       X Phase Delay Setting */
	RegWriteA_LC898122AF(PWMDLYY,
			     0x04); /* 0x0013       Y Phase Delay Setting */

#ifdef DEF_SET
	RegWriteA_LC898122AF(DRVCH1SEL,
			     0x00); /* 0x0005       OUT1/OUT2       X axis */
	RegWriteA_LC898122AF(DRVCH2SEL,
			     0x00); /* 0x0006       OUT3/OUT4       Y axis */

	RegWriteA_LC898122AF(PWMDLYTIMX,
			     0x00); /* 0x0014               PWM Timing */
	RegWriteA_LC898122AF(PWMDLYTIMY,
			     0x00); /* 0x0015               PWM Timing */
#endif

	if (UcCvrCod == CVER122) {
#ifdef PWM_CAREER_TEST
		RegWriteA_LC898122AF(PWMPERIODY, 0xD0);
		RegWriteA_LC898122AF(PWMPERIODY2, 0xD0);
#else /* PWM_CAREER_TEST */
		RegWriteA_LC898122AF(PWMPERIODY, 0x00);
		RegWriteA_LC898122AF(PWMPERIODY2, 0x00);
#endif
	} else {
#ifdef PWM_CAREER_TEST
		RegWriteA_LC898122AF(
			PWMPERIODX,
			0xF2); /* 0x0018               PWM Carrier Freq */
		RegWriteA_LC898122AF(
			PWMPERIODX2,
			0x00); /* 0x0019               PWM Carrier Freq */
		RegWriteA_LC898122AF(
			PWMPERIODY,
			0xF2); /* 0x001A               PWM Carrier Freq */
		RegWriteA_LC898122AF(
			PWMPERIODY2,
			0x00); /* 0x001B               PWM Carrier Freq */
#else				/* PWM_CAREER_TEST */
		RegWriteA_LC898122AF(
			PWMPERIODX,
			0x00); /* 0x0018               PWM Carrier Freq */
		RegWriteA_LC898122AF(
			PWMPERIODX2,
			0x00); /* 0x0019               PWM Carrier Freq */
		RegWriteA_LC898122AF(
			PWMPERIODY,
			0x00); /* 0x001A               PWM Carrier Freq */
		RegWriteA_LC898122AF(
			PWMPERIODY2,
			0x00); /* 0x001B               PWM Carrier Freq */
#endif
	}

	/* Linear PWM circuit setting */
	RegWriteA_LC898122AF(CVA,
			     0xC0); /* 0x0020       Linear PWM mode enable */

	if (UcCvrCod == CVER122)
		RegWriteA_LC898122AF(CVFC, 0x22); /* 0x0021 */

	RegWriteA_LC898122AF(CVFC2, 0x80); /* 0x0022 */
	if (UcCvrCod == CVER122) {
		RegWriteA_LC898122AF(CVSMTHX,
				     0x00); /* 0x0023       smooth off */
		RegWriteA_LC898122AF(CVSMTHY,
				     0x00); /* 0x0024       smooth off */
	}

	RegReadA_LC898122AF(STBB0, &UcStbb0);

	UcStbb0 &= 0x80;
	RegWriteA_LC898122AF(STBB0, UcStbb0); /* 0x0250       OIS standby */
}

#ifdef GAIN_CONT
#define TRI_LEVEL 0x3A031280 /* 0.0005 */
#define TIMELOW 0x50	/* */
#define TIMEHGH 0x05	/* */
#ifdef USE_EXTCLK_ALL	/* 24MHz */
#define TIMEBSE 0x2F	/* 4.0ms */
#else
#define TIMEBSE 0x5D /* 3.96ms */
#endif
#define MONADR GXXFZ
#define GANADR gxadj
#define XMINGAIN 0x00000000
#define XMAXGAIN 0x3F800000
#define YMINGAIN 0x00000000
#define YMAXGAIN 0x3F800000
#define XSTEPUP 0x38D1B717 /* 0.0001        */
#define XSTEPDN 0xBD4CCCCD /* -0.05         */
#define YSTEPUP 0x38D1B717 /* 0.0001        */
#define YSTEPDN 0xBD4CCCCD /* -0.05         */
#endif

void IniGyr(void)
{

	/*Gyro Filter Setting */
	RegWriteA_LC898122AF(WG_EQSW, 0x03);

	/*Gyro Filter Down Sampling */

	RegWriteA_LC898122AF(WG_SHTON, 0x10);
 /* CmShtOpe[1:0] 00: シ?ッターOFF, 01: シ?ッターON, 1x:外?制御 */

#ifdef DEF_SET
	RegWriteA_LC898122AF(WG_SHTDLYTMR, 0x00); /* 0x0117 Shutter Delay */
	RegWriteA_LC898122AF(WG_GADSMP,
			     0x00); /* 0x011C               Sampling timing */
	RegWriteA_LC898122AF(WG_HCHR, 0x00);
	RegWriteA_LC898122AF(WG_LMT3MOD, 0x00);
	/* CmLmt3Mod       0: 通常?ミッター動作, 1: 円の半径?ミッター動作 */
	RegWriteA_LC898122AF(WG_VREFADD, 0x12);
#endif
	RegWriteA_LC898122AF(WG_SHTMOD, 0x06);

	/* Limiter */
	RamWrite32A_LC898122AF(gxlmt1H, GYRLMT1H); /* 0x1028 */
	RamWrite32A_LC898122AF(gylmt1H, GYRLMT1H); /* 0x1128 */

	RamWrite32A_LC898122AF(gxlmt3HS0, GYRLMT3_S1); /* 0x1029 */
	RamWrite32A_LC898122AF(gylmt3HS0, GYRLMT3_S1); /* 0x1129 */

	RamWrite32A_LC898122AF(gxlmt3HS1, GYRLMT3_S2); /* 0x102A */
	RamWrite32A_LC898122AF(gylmt3HS1, GYRLMT3_S2); /* 0x112A */

	RamWrite32A_LC898122AF(
		gylmt4HS0,
		GYRLMT4_S1); /* 0x112B        Y軸Limiter4 High?値0 */
	RamWrite32A_LC898122AF(
		gxlmt4HS0,
		GYRLMT4_S1); /* 0x102B        X軸Limiter4 High?値0 */

	RamWrite32A_LC898122AF(
		gxlmt4HS1,
		GYRLMT4_S2); /* 0x102C        X軸Limiter4 High?値1 */
	RamWrite32A_LC898122AF(
		gylmt4HS1,
		GYRLMT4_S2); /* 0x112C        Y軸Limiter4 High?値1 */

	/* Pan/Tilt parameter */
	RegWriteA_LC898122AF(WG_PANADDA,
			     0x12); /* 0x0130       GXH1Z2/GYH1Z2 Select */
	RegWriteA_LC898122AF(WG_PANADDB,
			     0x09); /* 0x0131       GXIZ/GYIZ Select */

	/* Threshold */
	RamWrite32A_LC898122AF(SttxHis, 0x00000000);  /* 0x1226 */
	RamWrite32A_LC898122AF(SttxaL, 0x00000000);   /* 0x109D */
	RamWrite32A_LC898122AF(SttxbL, 0x00000000);   /* 0x109E */
	RamWrite32A_LC898122AF(Sttx12aM, GYRA12_MID); /* 0x104F */
	RamWrite32A_LC898122AF(Sttx12aH, GYRA12_HGH); /* 0x105F */
	RamWrite32A_LC898122AF(Sttx12bM, GYRB12_MID); /* 0x106F */
	RamWrite32A_LC898122AF(Sttx12bH, GYRB12_HGH); /* 0x107F */
	RamWrite32A_LC898122AF(Sttx34aM, GYRA34_MID); /* 0x108F */
	RamWrite32A_LC898122AF(Sttx34aH, GYRA34_HGH); /* 0x109F */
	RamWrite32A_LC898122AF(Sttx34bM, GYRB34_MID); /* 0x10AF */
	RamWrite32A_LC898122AF(Sttx34bH, GYRB34_HGH); /* 0x10BF */
	RamWrite32A_LC898122AF(SttyaL, 0x00000000);   /* 0x119D */
	RamWrite32A_LC898122AF(SttybL, 0x00000000);   /* 0x119E */
	RamWrite32A_LC898122AF(Stty12aM, GYRA12_MID); /* 0x114F */
	RamWrite32A_LC898122AF(Stty12aH, GYRA12_HGH); /* 0x115F */
	RamWrite32A_LC898122AF(Stty12bM, GYRB12_MID); /* 0x116F */
	RamWrite32A_LC898122AF(Stty12bH, GYRB12_HGH); /* 0x117F */
	RamWrite32A_LC898122AF(Stty34aM, GYRA34_MID); /* 0x118F */
	RamWrite32A_LC898122AF(Stty34aH, GYRA34_HGH); /* 0x119F */
	RamWrite32A_LC898122AF(Stty34bM, GYRB34_MID); /* 0x11AF */
	RamWrite32A_LC898122AF(Stty34bH, GYRB34_HGH); /* 0x11BF */

	/* Pan level */
	RegWriteA_LC898122AF(WG_PANLEVABS, 0x00); /* 0x0133 */

	/* Average parameter are set IniAdj */

	/* Phase Transition Setting */
	/* State 2 -> 1 */
	RegWriteA_LC898122AF(WG_PANSTT21JUG0, 0x00); /* 0x0140 */
	RegWriteA_LC898122AF(WG_PANSTT21JUG1, 0x00); /* 0x0141 */
	/* State 3 -> 1 */
	RegWriteA_LC898122AF(WG_PANSTT31JUG0, 0x00); /* 0x0142 */
	RegWriteA_LC898122AF(WG_PANSTT31JUG1, 0x00); /* 0x0143 */
	/* State 4 -> 1 */
	RegWriteA_LC898122AF(WG_PANSTT41JUG0, 0x01); /* 0x0144 */
	RegWriteA_LC898122AF(WG_PANSTT41JUG1, 0x00); /* 0x0145 */
	/* State 1 -> 2 */
	RegWriteA_LC898122AF(WG_PANSTT12JUG0, 0x00); /* 0x0146 */
	RegWriteA_LC898122AF(WG_PANSTT12JUG1, 0x07); /* 0x0147 */
	/* State 1 -> 3 */
	RegWriteA_LC898122AF(WG_PANSTT13JUG0, 0x00); /* 0x0148 */
	RegWriteA_LC898122AF(WG_PANSTT13JUG1, 0x00); /* 0x0149 */
	/* State 2 -> 3 */
	RegWriteA_LC898122AF(WG_PANSTT23JUG0, 0x11); /* 0x014A */
	RegWriteA_LC898122AF(WG_PANSTT23JUG1, 0x00); /* 0x014B */
	/* State 4 -> 3 */
	RegWriteA_LC898122AF(WG_PANSTT43JUG0, 0x00); /* 0x014C */
	RegWriteA_LC898122AF(WG_PANSTT43JUG1, 0x00); /* 0x014D */
	/* State 3 -> 4 */
	RegWriteA_LC898122AF(WG_PANSTT34JUG0, 0x01); /* 0x014E */
	RegWriteA_LC898122AF(WG_PANSTT34JUG1, 0x00); /* 0x014F */
	/* State 2 -> 4 */
	RegWriteA_LC898122AF(WG_PANSTT24JUG0, 0x00); /* 0x0150 */
	RegWriteA_LC898122AF(WG_PANSTT24JUG1, 0x00); /* 0x0151 */
	/* State 4 -> 2 */
	RegWriteA_LC898122AF(WG_PANSTT42JUG0, 0x44); /* 0x0152 */
	RegWriteA_LC898122AF(WG_PANSTT42JUG1, 0x04); /* 0x0153 */

	/* State Timer */
	RegWriteA_LC898122AF(WG_PANSTT1LEVTMR, 0x00); /* 0x015B */
	RegWriteA_LC898122AF(WG_PANSTT2LEVTMR, 0x00); /* 0x015C */
	RegWriteA_LC898122AF(WG_PANSTT3LEVTMR, 0x00); /* 0x015D */
	RegWriteA_LC898122AF(WG_PANSTT4LEVTMR, 0x03); /* 0x015E */

	/* Control filter */
	RegWriteA_LC898122AF(WG_PANTRSON0,
			     0x11); /* 0x0132       USE I12/iSTP/Gain-Filter */

	/* State Setting */
	IniPtMovMod(OFF); /* Pan/Tilt setting (Still) */

	/* Hold */
	RegWriteA_LC898122AF(WG_PANSTTSETILHLD, 0x00); /* 0x015F */

	/* State2,4 Step Time Setting */
	RegWriteA_LC898122AF(WG_PANSTT2TMR0, 0x01); /* 0x013C */
	RegWriteA_LC898122AF(WG_PANSTT2TMR1, 0x00); /* 0x013D */
	RegWriteA_LC898122AF(WG_PANSTT4TMR0, 0x02); /* 0x013E */
	RegWriteA_LC898122AF(WG_PANSTT4TMR1, 0x07); /* 0x013F */

	RegWriteA_LC898122AF(WG_PANSTTXXXTH, 0x00); /* 0x015A */

#ifdef GAIN_CONT
	RamWrite32A_LC898122AF(gxlevlow, TRI_LEVEL); /* 0x10AE       Low Th */
	RamWrite32A_LC898122AF(gylevlow, TRI_LEVEL); /* 0x11AE       Low Th */
	RamWrite32A_LC898122AF(gxadjmin, XMINGAIN); /* 0x1094       Low gain */
	RamWrite32A_LC898122AF(gxadjmax, XMAXGAIN); /* 0x1095       Hgh gain */
	RamWrite32A_LC898122AF(gxadjdn, XSTEPDN);   /* 0x1096       -step */
	RamWrite32A_LC898122AF(gxadjup, XSTEPUP);   /* 0x1097       +step */
	RamWrite32A_LC898122AF(gyadjmin, YMINGAIN); /* 0x1194       Low gain */
	RamWrite32A_LC898122AF(gyadjmax, YMAXGAIN); /* 0x1195       Hgh gain */
	RamWrite32A_LC898122AF(gyadjdn, YSTEPDN);   /* 0x1196       -step */
	RamWrite32A_LC898122AF(gyadjup, YSTEPUP);   /* 0x1197       +step */

	RegWriteA_LC898122AF(
		WG_LEVADD,
		(unsigned char)MONADR); /* 0x0120       Input signal */
	RegWriteA_LC898122AF(WG_LEVTMR, TIMEBSE); /* 0x0123       Base Time */
	RegWriteA_LC898122AF(WG_LEVTMRLOW,
			     TIMELOW); /* 0x0121       X Low Time */
	RegWriteA_LC898122AF(WG_LEVTMRHGH,
			     TIMEHGH); /* 0x0122       X Hgh Time */
	RegWriteA_LC898122AF(
		WG_ADJGANADD,
		(unsigned char)GANADR); /* 0x0128       control address */
	RegWriteA_LC898122AF(WG_ADJGANGO, 0x00); /* 0x0108       manual off */

	/* exe function */
	/* AutoGainControlSw( OFF ) ;     */ /* Auto Gain Control Mode OFF */
	AutoGainControlSw(ON);		     /* Auto Gain Control Mode ON  */
#endif
}

void IniFil(void)
{
	unsigned short UsAryId;

	/* Filter Registor Parameter Setting */
	UsAryId = 0;
	while (CsFilReg[UsAryId].UsRegAdd != 0xFFFF) {
		RegWriteA_LC898122AF(CsFilReg[UsAryId].UsRegAdd,
				     CsFilReg[UsAryId].UcRegDat);
		UsAryId++;
	}

	/* Filter Ram Parameter Setting */
	UsAryId = 0;
	while (CsFilRam[UsAryId].UsRamAdd != 0xFFFF) {
		RamWrite32A_LC898122AF(CsFilRam[UsAryId].UsRamAdd,
					CsFilRam[UsAryId].UlRamDat);
		UsAryId++;
	}
}

void IniAdj(void)
{
	RegWriteA_LC898122AF(WC_RAMACCXY,
			     0x00); /* 0x018D       Filter copy off */

	IniPtAve(); /* Average setting */

	/* OIS */
	RegWriteA_LC898122AF(CMSDAC0,
			     BIAS_CUR_OIS); /* 0x0251       Hall Dac電流 */
	RegWriteA_LC898122AF(OPGSEL0,
			     AMP_GAIN_X); /* 0x0253       Hall amp Gain X */
	RegWriteA_LC898122AF(OPGSEL1,
			     AMP_GAIN_Y); /* 0x0254       Hall amp Gain Y */
	/* AF */
	RegWriteA_LC898122AF(CMSDAC1,
			     BIAS_CUR_AF); /* 0x0252       Hall Dac電流 */
	RegWriteA_LC898122AF(OPGSEL2,
			     AMP_GAIN_AF); /* 0x0255       Hall amp Gain AF */

	RegWriteA_LC898122AF(OSCSET, OSC_INI); /* 0x0257       OSC ini */

	/* adjusted value */
	RegWriteA_LC898122AF(
		IZAH,
		DGYRO_OFST_XH); /* 0x02A0               Set Offset High byte */
	RegWriteA_LC898122AF(
		IZAL,
		DGYRO_OFST_XL); /* 0x02A1               Set Offset Low byte */
	RegWriteA_LC898122AF(
		IZBH,
		DGYRO_OFST_YH); /* 0x02A2               Set Offset High byte */
	RegWriteA_LC898122AF(
		IZBL,
		DGYRO_OFST_YL); /* 0x02A3               Set Offset Low byte */

	/* Ram Access */
	RamAccFixMod(ON); /* 16bit Fix mode */

	/* OIS adjusted parameter */
	RamWriteA_LC898122AF(DAXHLO, DAHLXO_INI); /* 0x1479 */
	RamWriteA_LC898122AF(DAXHLB, DAHLXB_INI); /* 0x147A */
	RamWriteA_LC898122AF(DAYHLO, DAHLYO_INI); /* 0x14F9 */
	RamWriteA_LC898122AF(DAYHLB, DAHLYB_INI); /* 0x14FA */
	RamWriteA_LC898122AF(OFF0Z, HXOFF0Z_INI); /* 0x1450 */
	RamWriteA_LC898122AF(OFF1Z, HYOFF1Z_INI); /* 0x14D0 */
	RamWriteA_LC898122AF(sxg, SXGAIN_INI);    /* 0x10D3 */
	RamWriteA_LC898122AF(syg, SYGAIN_INI);    /* 0x11D3 */
	/* UsCntXof = OPTCEN_X ;         */ /* Clear Optical center X value */
	/* UsCntYof = OPTCEN_Y ;         */ /* Clear Optical center Y value */
	/* RamWriteA_LC898122AF( SXOFFZ1,             UsCntXof ) ; // 0x1461 */
	/* RamWriteA_LC898122AF( SYOFFZ1,             UsCntYof ) ; // 0x14E1 */

	/* AF adjusted parameter */
	RamWriteA_LC898122AF(DAZHLO, DAHLZO_INI); /* 0x1529 */
	RamWriteA_LC898122AF(DAZHLB, DAHLZB_INI); /* 0x152A */

	/* Ram Access */
	RamAccFixMod(OFF); /* 32bit Float mode */

	RamWrite32A_LC898122AF(
		gxzoom,
		GXGAIN_INI); /* 0x1020 Gyro X axis Gain adjusted value */
	RamWrite32A_LC898122AF(
		gyzoom,
		GYGAIN_INI); /* 0x1120 Gyro Y axis Gain adjusted value */

	RamWrite32A_LC898122AF(sxq, SXQ_INI);
	RamWrite32A_LC898122AF(syq, SYQ_INI);

	if (GXHY_GYHX) { /* GX -> HY , GY -> HX */
		RamWrite32A_LC898122AF(sxgx, 0x00000000); /* 0x10B8 */
		RamWrite32A_LC898122AF(sxgy, 0x3F800000); /* 0x10B9 */

		RamWrite32A_LC898122AF(sygy, 0x00000000); /* 0x11B8 */
		RamWrite32A_LC898122AF(sygx, 0x3F800000); /* 0x11B9 */
	}

	SetZsp(0); /* Zoom coefficient Initial Setting */

	RegWriteA_LC898122AF(PWMA, 0xC0); /* 0x0010               PWM enable */

	RegWriteA_LC898122AF(STBB0, 0xDF);

	RegWriteA_LC898122AF(WC_EQSW, 0x02);     /* 0x01E0 */
	RegWriteA_LC898122AF(WC_MESLOOP1, 0x02); /* 0x0193 */
	RegWriteA_LC898122AF(WC_MESLOOP0, 0x00); /* 0x0192 */
	RegWriteA_LC898122AF(WC_AMJLOOP1, 0x02); /* 0x01A3 */
	RegWriteA_LC898122AF(WC_AMJLOOP0, 0x00); /* 0x01A2 */

	SetPanTiltMode(OFF); /* Pan/Tilt OFF */

	SetGcf(0); /* DI initial value */
#ifdef H1COEF_CHANGER
	SetH1cMod(ACTMODE); /* Lvl Change Active mode */
#endif

	DrvSw(ON); /* 0x0001               Driver Mode setting */

	RegWriteA_LC898122AF(WC_EQON, 0x01); /* 0x0101       Filter ON */
}

void IniCmd(void)
{

	MemClr((unsigned char *)&StAdjPar,
		sizeof(struct stAdjPar)); /* Adjust Parameter Clear */
}

void BsyWit(unsigned short UsTrgAdr, unsigned char UcTrgDat)
{
	unsigned char UcFlgVal;

	RegWriteA_LC898122AF(UsTrgAdr,
			     UcTrgDat); /* Trigger Register Setting */

	UcFlgVal = 1;

	while (UcFlgVal) {

		RegReadA_LC898122AF(UsTrgAdr, &UcFlgVal);
		UcFlgVal &= (UcTrgDat & 0x0F);
	};
}

void MemClr(unsigned char *NcTgtPtr, unsigned short UsClrSiz)
{
	unsigned short UsClrIdx;

	for (UsClrIdx = 0; UsClrIdx < UsClrSiz; UsClrIdx++) {
		*NcTgtPtr = 0;
		NcTgtPtr++;
	}
}

void GyOutSignal(void)
{

	RegWriteA_LC898122AF(GRADR0, GYROX_INI);
	RegWriteA_LC898122AF(GRADR1, GYROY_INI);

	/*Start OIS Reading */
	RegWriteA_LC898122AF(GRSEL, 0x02);
}

void GyOutSignalCont(void)
{

	/*Start OIS Reading */
	RegWriteA_LC898122AF(GRSEL, 0x04);
}

#ifdef STANDBY_MODE

void AccWit(unsigned char UcTrgDat)
{
	unsigned char UcFlgVal;

	UcFlgVal = 1;

	while (UcFlgVal) {
		RegReadA_LC898122AF(GRACC, &UcFlgVal); /* 0x0282 */
		UcFlgVal &= UcTrgDat;
	};
}

void SelectGySleep(unsigned char UcSelMode)
{
#ifdef USE_INVENSENSE
	unsigned char UcRamIni;
	unsigned char UcGrini;

	if (UcSelMode == ON) {
		RegWriteA_LC898122AF(WC_EQON,
				     0x00); /* 0x0101       Equalizer OFF */
		RegWriteA_LC898122AF(GRSEL,
				     0x01); /* 0x0280       Set Command Mode */

		RegReadA_LC898122AF(GRINI, &UcGrini);

		RegWriteA_LC898122AF(GRINI, (UcGrini | SLOWMODE));

		RegWriteA_LC898122AF(
			GRADR0, 0x6B); /* 0x0283       Set Write Command */
		RegWriteA_LC898122AF(GRACC, 0x01);
		AccWit(0x01); /* Digital Gyro busy wait */
		RegReadA_LC898122AF(GRDAT0H, &UcRamIni); /* 0x0290 */

		UcRamIni |= 0x40; /* Set Sleep bit */
#ifdef GYROSTBY
		UcRamIni &= ~0x01; /* Clear PLL bit(internal oscillator */
#endif

		RegWriteA_LC898122AF(
			GRADR0, 0x6B); /* 0x0283       Set Write Command */
		RegWriteA_LC898122AF(
			GSETDT,
			UcRamIni); /* 0x028A       Set Write Data(Sleep ON) */
		RegWriteA_LC898122AF(GRACC,
				     0x10); /* 0x0282       Set Trigger ON */
		AccWit(0x10); /* Digital Gyro busy wait */

#ifdef GYROSTBY
		RegWriteA_LC898122AF(
			GRADR0, 0x6C); /* 0x0283       Set Write Command */
		RegWriteA_LC898122AF(
			GSETDT,
			0x07); /* 0x028A       Set Write Data(STBY ON) */
		RegWriteA_LC898122AF(GRACC,
				     0x10); /* 0x0282       Set Trigger ON */
		AccWit(0x10); /* Digital Gyro busy wait */
#endif
	} else {
#ifdef GYROSTBY
		RegWriteA_LC898122AF(
			GRADR0, 0x6C); /* 0x0283       Set Write Command */
		RegWriteA_LC898122AF(
			GSETDT,
			0x00); /* 0x028A       Set Write Data(STBY OFF) */
		RegWriteA_LC898122AF(GRACC,
				     0x10); /* 0x0282       Set Trigger ON */
		AccWit(0x10); /* Digital Gyro busy wait */
#endif
		RegWriteA_LC898122AF(GRADR0, 0x6B);
		RegWriteA_LC898122AF(GRACC, 0x01);
		AccWit(0x01); /* Digital Gyro busy wait */
		RegReadA_LC898122AF(GRDAT0H, &UcRamIni); /* 0x0290 */

		UcRamIni &= ~0x40; /* Clear Sleep bit */
#ifdef GYROSTBY
		UcRamIni |= 0x01; /* Set PLL bit */
#endif

		RegWriteA_LC898122AF(
			GSETDT,
			UcRamIni); /* 0x028A       Set Write Data(Sleep OFF) */
		RegWriteA_LC898122AF(GRACC,
				     0x10); /* 0x0282       Set Trigger ON */
		AccWit(0x10); /* Digital Gyro busy wait */

		RegReadA_LC898122AF(GRINI, &UcGrini);

		RegWriteA_LC898122AF(GRINI, (UcGrini & ~SLOWMODE));

		GyOutSignal(); /* Select Gyro output signal */

		WitTim_LC898122AF(50); /* 50ms wait */

		RegWriteA_LC898122AF(
			WC_EQON, 0x01); /* 0x0101       GYRO Equalizer ON */

		ClrGyr(0x007F, CLR_FRAM1); /* Gyro Delay RAM Clear */
	}
#else /* Panasonic */

	/* unsigned char   UcRamIni ; */

	if (UcSelMode == ON) {
		RegWriteA_LC898122AF(
			WC_EQON, 0x00); /* 0x0101       GYRO Equalizer OFF */
		RegWriteA_LC898122AF(GRSEL,
				     0x01); /* 0x0280       Set Command Mode */
		RegWriteA_LC898122AF(
			GRADR0, 0x4C); /* 0x0283       Set Write Command */
		RegWriteA_LC898122AF(
			GSETDT,
			0x02); /* 0x028A       Set Write Data(Sleep ON) */
		RegWriteA_LC898122AF(GRACC,
				     0x10); /* 0x0282       Set Trigger ON */
		AccWit(0x10); /* Digital Gyro busy wait */
	} else {
		RegWriteA_LC898122AF(
			GRADR0, 0x4C); /* 0x0283       Set Write Command */
		RegWriteA_LC898122AF(
			GSETDT,
			0x00); /* 0x028A       Set Write Data(Sleep OFF) */
		RegWriteA_LC898122AF(GRACC,
				     0x10); /* 0x0282       Set Trigger ON */
		AccWit(0x10); /* Digital Gyro busy wait */
		GyOutSignal(); /* Select Gyro output signal */

		WitTim_LC898122AF(50); /* 50ms wait */

		RegWriteA_LC898122AF(
			WC_EQON, 0x01);    /* 0x0101       GYRO Equalizer ON */
		ClrGyr(0x007F, CLR_FRAM1); /* Gyro Delay RAM Clear */
	}
#endif
}
#endif

#ifdef GAIN_CONT

void AutoGainControlSw(unsigned char UcModeSw)
{

	if (UcModeSw == OFF) {
		RegWriteA_LC898122AF(WG_ADJGANGXATO,
				     0xA0); /* 0x0129       X exe off */
		RegWriteA_LC898122AF(WG_ADJGANGYATO,
				     0xA0); /* 0x012A       Y exe off */
		RamWrite32A_LC898122AF(GANADR, XMAXGAIN); /* Gain Through */
		RamWrite32A_LC898122AF(GANADR | 0x0100,
					YMAXGAIN); /* Gain Through */
	} else {
		RegWriteA_LC898122AF(WG_ADJGANGXATO,
				     0xA3); /* 0x0129       X exe on */
		RegWriteA_LC898122AF(WG_ADJGANGYATO,
				     0xA3); /* 0x012A       Y exe on */
	}
}
#endif

void ClrGyr(unsigned short UsClrFil, unsigned char UcClrMod)
{
	unsigned char UcRamClr;

	/*Select Filter to clear */
	RegWriteA_LC898122AF(WC_RAMDLYMOD1, (unsigned char)(UsClrFil >> 8));
	RegWriteA_LC898122AF(
		WC_RAMDLYMOD0, (unsigned char)UsClrFil);

	/*Enable Clear */
	RegWriteA_LC898122AF(WC_RAMINITON,
			     UcClrMod);

	/*Check RAM Clear complete */
	do {
		RegReadA_LC898122AF(WC_RAMINITON, &UcRamClr);
		UcRamClr &= UcClrMod;
	} while (UcRamClr != 0x00);
}

void DrvSw(unsigned char UcDrvSw)
{
	if (UcDrvSw == ON) {
		if (UcPwmMod == PWMMOD_CVL) {
			RegWriteA_LC898122AF(
				DRVFC,
				0xF0);
		} else {
#ifdef PWM_BREAK
			RegWriteA_LC898122AF(
				DRVFC,
				0x00);
#else
			RegWriteA_LC898122AF(
				DRVFC,
				0xC0);
#endif
		}
	} else {
		if (UcPwmMod == PWMMOD_CVL) {
			RegWriteA_LC898122AF(
				DRVFC,
				0x30); /* 0x0001       Drvier Block Ena=0 */
		} else {
#ifdef PWM_BREAK
			RegWriteA_LC898122AF(
				DRVFC,
				0x00); /* 0x0001 Drv.MODE=0,Drv.BLK=0,MODE0B */
#else
			RegWriteA_LC898122AF(
				DRVFC,
				0x00); /* 0x0001       Drvier Block Ena=0 */
#endif
		}
	}
}

void AfDrvSw(unsigned char UcDrvSw)
{
	if (UcDrvSw == ON) {
#ifdef AF_PWMMODE
		RegWriteA_LC898122AF(DRVFCAF, 0x00);
#else
		RegWriteA_LC898122AF(DRVFCAF, 0x20);
#endif
		RegWriteA_LC898122AF(CCAAF, 0x80);
	} else {
		RegWriteA_LC898122AF(CCAAF, 0x00);
	}
}

void RamAccFixMod(unsigned char UcAccMod)
{
	switch (UcAccMod) {
	case OFF:
		RegWriteA_LC898122AF(WC_RAMACCMOD, 0x00);
		break;
	case ON:
		RegWriteA_LC898122AF(WC_RAMACCMOD, 0x31);
		break;
	}
}

void IniAf(void)
{
	unsigned char UcStbb0;

	AfDrvSw(OFF); /* AF Drvier Block Ena=0 */
#ifdef AF_PWMMODE
	RegWriteA_LC898122AF(DRVFCAF, 0x00);
#else
	RegWriteA_LC898122AF(DRVFCAF, 0x20);
#endif
	RegWriteA_LC898122AF(DRVFC3AF, 0x00);
	RegWriteA_LC898122AF(DRVFC4AF, 0x80); /* 0x0084       DOFSTDAF */
	RegWriteA_LC898122AF(PWMAAF, 0x00);   /* 0x0090       AF PWM standby */
	RegWriteA_LC898122AF(AFFC, 0x80);     /* 0x0088       OpenAF/-/- */
#ifdef AF_PWMMODE
	RegWriteA_LC898122AF(DRVFC2AF, 0x82); /* 0x0082       AF slope3 */
	RegWriteA_LC898122AF(DRVCH3SEL,
			     0x02); /* 0x0085       AF only IN1 control */
	RegWriteA_LC898122AF(PWMFCAF,
			     0x89); /* 0x0091       AF GND , Carrier , MODE1 */
	RegWriteA_LC898122AF(PWMPERIODAF,
			     0xA0); /* 0x0099       AF none-synchronism */
#else
	RegWriteA_LC898122AF(DRVFC2AF, 0x00); /* 0x0082       AF slope0 */
	RegWriteA_LC898122AF(DRVCH3SEL,
			     0x00); /* 0x0085       AF H bridge control */
	RegWriteA_LC898122AF(
		PWMFCAF, 0x01); /* 0x0091       AF VREF , Carrier , MODE1 */
	RegWriteA_LC898122AF(PWMPERIODAF,
			     0x20); /* 0x0099       AF none-synchronism */
#endif
	RegWriteA_LC898122AF(CCFCAF, 0x40); /* 0x00A1       GND/- */

	RegReadA_LC898122AF(STBB0, &UcStbb0);

	UcStbb0 &= 0x7F;
	RegWriteA_LC898122AF(STBB0, UcStbb0); /* 0x0250       OIS standby */
	RegWriteA_LC898122AF(STBB1, 0x00);    /* 0x0264       All standby */

	/* AF Initial setting */
	RegWriteA_LC898122AF(FSTMODE, FSTMODE_AF); /* 0x0302 */
	RamWriteA_LC898122AF(
		RWEXD1_L,
		RWEXD1_L_AF); /* 0x0396 - 0x0397 (Register continuos write) */
	RamWriteA_LC898122AF(
		RWEXD2_L,
		RWEXD2_L_AF); /* 0x0398 - 0x0399 (Register continuos write) */
	RamWriteA_LC898122AF(
		RWEXD3_L,
		RWEXD3_L_AF); /* 0x039A - 0x039B (Register continuos write) */
	RegWriteA_LC898122AF(FSTCTIME, FSTCTIME_AF); /* 0x0303 */
	RamWriteA_LC898122AF(
		TCODEH,
		0x0000); /* 0x0304 - 0x0305 (Register continuos write) */

#ifdef AF_PWMMODE
	RegWriteA_LC898122AF(PWMAAF, 0x80); /* 0x0090       AF PWM enable */
#endif

	UcStbb0 |= 0x80;
	RegWriteA_LC898122AF(STBB0, UcStbb0); /* 0x0250 */
	RegWriteA_LC898122AF(STBB1,
			     0x05);
	AfDrvSw(ON); /* AF Drvier Block Ena=1 */
}

void IniPtAve(void)
{
	RegWriteA_LC898122AF(WG_PANSTT1DWNSMP0, 0x00); /* 0x0134 */
	RegWriteA_LC898122AF(WG_PANSTT1DWNSMP1, 0x00); /* 0x0135 */
	RegWriteA_LC898122AF(WG_PANSTT2DWNSMP0, 0x90); /* 0x0136 400 */
	RegWriteA_LC898122AF(WG_PANSTT2DWNSMP1, 0x01); /* 0x0137 */
	RegWriteA_LC898122AF(WG_PANSTT3DWNSMP0, 0x64); /* 0x0138 100 */
	RegWriteA_LC898122AF(WG_PANSTT3DWNSMP1, 0x00); /* 0x0139 */
	RegWriteA_LC898122AF(WG_PANSTT4DWNSMP0, 0x00); /* 0x013A */
	RegWriteA_LC898122AF(WG_PANSTT4DWNSMP1, 0x00); /* 0x013B */

	RamWrite32A_LC898122AF(st1mean, 0x3f800000); /* 0x1235 */
	RamWrite32A_LC898122AF(st2mean, 0x3B23D700); /* 0x1236       1/400 */
	RamWrite32A_LC898122AF(st3mean, 0x3C23D700); /* 0x1237       1/100 */
	RamWrite32A_LC898122AF(st4mean, 0x3f800000); /* 0x1238 */
}

void IniPtMovMod(unsigned char UcPtMod)
{
	switch (UcPtMod) {
	case OFF:
		RegWriteA_LC898122AF(WG_PANSTTSETGYRO, 0x00); /* 0x0154 */
		RegWriteA_LC898122AF(WG_PANSTTSETGAIN, 0x54); /* 0x0155 */
		RegWriteA_LC898122AF(WG_PANSTTSETISTP, 0x14); /* 0x0156 */
		RegWriteA_LC898122AF(WG_PANSTTSETIFTR, 0x94); /* 0x0157 */
		RegWriteA_LC898122AF(WG_PANSTTSETLFTR, 0x00); /* 0x0158 */

		break;
	case ON:
		RegWriteA_LC898122AF(WG_PANSTTSETGYRO, 0x00); /* 0x0154 */
		RegWriteA_LC898122AF(WG_PANSTTSETGAIN, 0x00); /* 0x0155 */
		RegWriteA_LC898122AF(WG_PANSTTSETISTP, 0x14); /* 0x0156 */
		RegWriteA_LC898122AF(WG_PANSTTSETIFTR, 0x94); /* 0x0157 */
		RegWriteA_LC898122AF(WG_PANSTTSETLFTR, 0x00); /* 0x0158 */
		break;
	}
}

void ChkCvr(void)
{
	RegReadA_LC898122AF(CVER, &UcCvrCod);  /* 0x027E */
	RegWriteA_LC898122AF(MDLREG, MDL_VER); /* 0x00FF       Model */
	RegWriteA_LC898122AF(VRREG, FW_VER);   /* 0x02D0       Version */
}
