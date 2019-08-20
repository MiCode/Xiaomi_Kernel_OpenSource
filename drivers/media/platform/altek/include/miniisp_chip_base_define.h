/*
 * File: miniisp_chip_base_define.c.h
 * Description: Mini ISP ChipBase Define Code
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 *  2017/10/18; Louis Wang; Initial version
 */

/*
 * This file is part of al6100.
 *
 * al6100 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * al6100 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTIBILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License version 2 for
 * more details.
 *
 * You should have received a copy of the General Public License version 2
 * along with al6100. If not, see https://www.gnu.org/licenses/gpl-2.0.html.
 */


#ifndef _MINIISP_CHIP_BASE_DEFINE_H_
#define _MINIISP_CHIP_BASE_DEFINE_H_

/******Include File******/


/******Public Constant Definition******/
#define CHIP_ID_ADDR 0xffef0020
#define SPIE_DUMMY_BYTE_ADDR 0xFFF82000

#define INTERRUPT_STATUS_REGISTER_ADDR 0xffef00b4
#define INTERRUPT_ENABLE_BIT_REGISTER_ADDR 0xffef00b0

#define clk_gen_dump_field \
{\
0xFFE80040, 0xFFE8004C,\
0xFFE80060, 0xFFE80064,\
0xFFE80080, 0xFFE80084,\
0xFFE800A0, 0xFFE800A4,\
0xFFE800C0, 0xFFE800C4,\
0xFFE800E0, 0xFFE800E4,\
0xFFE80100, 0xFFE80104,\
0xFFE80120, 0xFFE80124,\
0xFFE80140, 0xFFE80144,\
0xFFE80160, 0xFFE80164,\
0xFFE80180, 0xFFE80184,\
0xFFE801A0, 0xFFE801A8,\
0xFFE801C8, 0xFFE801C8,\
0xFFE80200, 0xFFE80204,\
0xFFE80240, 0xFFE80244,\
0xFFE80280, 0xFFE80284,\
0xFFE80340, 0xFFE80344,\
0xFFE80380, 0xFFE80384,\
0xFFE80400, 0xFFE80404,\
0xFFE80440, 0xFFE80444,\
0xFFE80460, 0xFFE80464,\
0xFFE80484, 0xFFE80484,\
0xFFE804A4, 0xFFE804A4,\
0xFFE804C4, 0xFFE804C4,\
0xFFE804E4, 0xFFE804E4,\
0xFFE80500, 0xFFE80504,\
0xFFE80540, 0xFFE80548,\
0xFFE80580, 0xFFE80588,\
0xFFE805C0, 0xFFE805C8,\
0xFFE80620, 0xFFE80630,\
0xFFE80680, 0xFFE80684,\
0xFFE806E0, 0xFFE806E4,\
0xFFE80720, 0xFFE80724,\
0xFFE807C0, 0xFFE807C4,\
0xFFE80804, 0xFFE80804,\
0xFFE80814, 0xFFE80814,\
0xFFE80824, 0xFFE80824,\
0xFFE80840, 0xFFE80844,\
0xFFE80884, 0xFFE80884,\
0xFFE80900, 0xFFE80904,\
0xFFE80944, 0xFFE80944,\
0xFFE80B00, 0xFFE80B04,\
0xFFE80C00, 0xFFE80C04,\
0xFFE80C40, 0xFFE80C44,\
0xFFE81000, 0xFFE81004,\
0xFFE81080, 0xFFE81080,\
0xFFE81100, 0xFFE81114,\
0xFFE81120, 0xFFE81134,\
0xFFE81140, 0xFFE81154,\
0xFFE81200, 0xFFE8122C,\
0xFFE8F000, 0xFFE8F000,\
}

#define mipi_tx_phy_if_0_dump_field \
{\
0xFFED1000, 0xFFED1048,\
0xFFED1100, 0xFFED1104,\
}

#define mipi_tx_phy_if_1_dump_field \
{\
0xFFED6000, 0xFFED6048,\
0xFFED6100, 0xFFED6104,\
}

#define gen_reg_dump_field \
{\
0xffef0000, 0xffef0008,\
0xffef0020,	0xffef0044,\
0xffef0054,	0xffef005C,\
0xffef0070,	0xffef00a8,\
0xffef00b0,	0xffef00b4,\
0xffef00f0,	0xffef00f8,\
0xffef0100,	0xffef0100,\
0xffef0208,	0xffef0208,\
0xffef0240,	0xffef0240,\
0xffef0248,	0xffef0248,\
0xffef0250,	0xffef0258,\
0xffef0280,	0xffef0284,\
0xffef0410,	0xffef0414,\
0xffef0500,	0xffef0500,\
0xffef0f00,	0xffef0f00,\
}

#define mipi_slvds_rx_phy_if_0_dump_field \
{\
0xfff91000, 0xfff91008,\
0xfff91010, 0xfff91040,\
0xfff9104c,	0xfff9104c,\
0xfff91068,	0xfff91078,\
0xfff91084,	0xfff91084,\
0xfff9108c,	0xfff9108c,\
0xfff91094,	0xfff910c8,\
0xfff91100,	0xfff9114c,\
}

#define mipi_slvds_rx_phy_if_1_dump_field \
{\
0xfff94000,	0xfff94008,\
0xfff94010,	0xfff94040,\
0xfff9404c,	0xfff9404c,\
0xfff94068,	0xfff94078,\
0xfff94084,	0xfff94084,\
0xfff9408c,	0xfff9408c,\
0xfff94094,	0xfff940c8,\
0xfff94100,	0xfff9414c,\
}

#define ppi_bridge_a_0_dump_field \
{\
0xfff97000, 0xfff97040,\
0xfff970a0, 0xfff970ac,\
0xfff970f0,	0xfff970fc,\
0xfff97110,	0xfff9716c,\
0xfff972a0,	0xfff972a4,\
0xfff97300,	0xfff97314,\
}

#define ppi_bridge_a_1_dump_field \
{\
0xfff98000,	0xfff98040,\
0xfff980a0,	0xfff980ac,\
0xfff980f0,	0xfff980fc,\
0xfff98110,	0xfff9816c,\
0xfff982a0,	0xfff982a4,\
0xfff98300,	0xfff98314,\
}

#define ppi_bridge_a_1_dump_field \
{\
0xfff98000,	0xfff98040,\
0xfff980a0,	0xfff980ac,\
0xfff980f0,	0xfff980fc,\
0xfff98110,	0xfff9816c,\
0xfff982a0,	0xfff982a4,\
0xfff98300,	0xfff98314,\
}

#define tx_top_out_mux_a_0_dump_field \
{\
0xffecb000,	0xffecb004,\
0xffecb010,	0xffecb014,\
0xffecb020,	0xffecb050,\
0xffecb100,	0xffecb13c,\
0xffecb200,	0xffecb220,\
0xffecb300,	0xffecb32c,\
0xffecbf00,	0xffecbf00,\
}

#define tx_top_out_mux_a_1_dump_field \
{\
0xffecc000,	0xffecc004,\
0xffecc010,	0xffecc014,\
0xffecc020,	0xffecc050,\
0xffecc100, 0xffecc13c,\
0xffecc200, 0xffecc220,\
0xffecc300,	0xffecc32c,\
0xffeccf00, 0xffeccf00,\
}

#define tx_line_merge_21_a_0_dump_field \
{\
0xffec1000,	0xffec1010,\
0xffec1020,	0xffec1024,\
0xffec1030,	0xffec1034,\
0xffec1040,	0xffec1044,\
0xffec104c,	0xffec105c,\
0xffec1070,	0xffec1074,\
0xffec108c,	0xffec108c,\
0xffec1100,	0xffec1124,\
0xffec1140,	0xffec1144,\
0xffec1150,	0xffec1174,\
0xffec1180,	0xffec1184,\
0xffec1300,	0xffec1300,\
0xffec1310,	0xffec1320,\
0xffec1330,	0xffec135c,\
0xffec1400,	0xffec1404,\
0xffec1410,	0xffec145c,\
0xffec1470,	0xffec1484,\
0xffec1490,	0xffec14b0,\
0xffec1a00,	0xffec1a00,\
0xffec1a10,	0xffec1a34,\
0xffec1a40,	0xffec1a84,\
0xffec1a90,	0xffec1aac,\
0xffec1ac0,	0xffec1ac4,\
0xffec1ad0,	0xffec1adc,\
0xffec1b00,	0xffec1b08,\
0xffec1b10,	0xffec1b14,\
0xffec1b20,	0xffec1b54,\
0xffec1b68,	0xffec1b98,\
0xffec1f00,	0xffec1f00,\
}

#define tx_line_merge_21_b_0_dump_field \
{\
0xffec2000,	0xffec2010,\
0xffec2020,	0xffec2024,\
0xffec2030,	0xffec2034,\
0xffec2040,	0xffec2044,\
0xffec204c,	0xffec205c,\
0xffec2070,	0xffec2074,\
0xffec208c,	0xffec208c,\
0xffec2100,	0xffec2124,\
0xffec2140,	0xffec2144,\
0xffec2150,	0xffec2174,\
0xffec2180,	0xffec2184,\
0xffec2300,	0xffec2300,\
0xffec2310,	0xffec2320,\
0xffec2330,	0xffec235c,\
0xffec2400,	0xffec2404,\
0xffec2410,	0xffec245c,\
0xffec2470,	0xffec2484,\
0xffec2490,	0xffec24b0,\
0xffec2a00,	0xffec2a00,\
0xffec2a10,	0xffec2a34,\
0xffec2a40,	0xffec2a84,\
0xffec2a90,	0xffec2aac,\
0xffec2ac0,	0xffec2ac4,\
0xffec2ad0,	0xffec2adc,\
0xffec2b00,	0xffec2b08,\
0xffec2b10,	0xffec2b14,\
0xffec2b20,	0xffec2b54,\
0xffec2b68,	0xffec2b98,\
0xffec2f00,	0xffec2f00,\
}

#define tx_line_merge_21_c_0_dump_field \
{\
0xffec5000,	0xffec5010,\
0xffec5020,	0xffec5024,\
0xffec5030,	0xffec5034,\
0xffec5040,	0xffec5044,\
0xffec504c,	0xffec505c,\
0xffec5070,	0xffec5074,\
0xffec508c,	0xffec508c,\
0xffec5100,	0xffec5124,\
0xffec5140,	0xffec5144,\
0xffec5150,	0xffec5174,\
0xffec5180,	0xffec5184,\
0xffec5300,	0xffec5300,\
0xffec5310,	0xffec5320,\
0xffec5330,	0xffec535c,\
0xffec5400,	0xffec5404,\
0xffec5410,	0xffec545c,\
0xffec5470,	0xffec5484,\
0xffec5490,	0xffec54b0,\
0xffec5a00,	0xffec5a00,\
0xffec5a10,	0xffec5a34,\
0xffec5a40,	0xffec5a84,\
0xffec5a90,	0xffec5aac,\
0xffec5ac0,	0xffec5ac4,\
0xffec5ad0,	0xffec5adc,\
0xffec5b00,	0xffec5b08,\
0xffec5b10,	0xffec5b14,\
0xffec5b20,	0xffec5b54,\
0xffec5b68,	0xffec5b98,\
0xffec5f00,	0xffec5f00,\
}

#define tx_line_merge_21_d_0_dump_field \
{\
0xffec6000,	0xffec6010,\
0xffec6020,	0xffec6024,\
0xffec6030,	0xffec6034,\
0xffec6040,	0xffec6044,\
0xffec604c,	0xffec605c,\
0xffec6070,	0xffec6074,\
0xffec608c,	0xffec608c,\
0xffec6100,	0xffec6124,\
0xffec6140,	0xffec6144,\
0xffec6150,	0xffec6174,\
0xffec6180,	0xffec6184,\
0xffec6300,	0xffec6300,\
0xffec6310,	0xffec6320,\
0xffec6330,	0xffec635c,\
0xffec6400,	0xffec6404,\
0xffec6410,	0xffec645c,\
0xffec6470,	0xffec6484,\
0xffec6490,	0xffec64b0,\
0xffec6a00,	0xffec6a00,\
0xffec6a10,	0xffec6a34,\
0xffec6a40,	0xffec6a84,\
0xffec6a90,	0xffec6aac,\
0xffec6ac0,	0xffec6ac4,\
0xffec6ad0,	0xffec6adc,\
0xffec6b00,	0xffec6b08,\
0xffec6b10,	0xffec6b14,\
0xffec6b20,	0xffec6b54,\
0xffec6b68,	0xffec6b98,\
0xffec6f00,	0xffec6f00,\
}

#define mipi_csi2_tx_0_dump_field \
{\
0xFFED0000,	0xFFED0008,\
0xFFED0044,	0xFFED0044,\
0xFFED0058,	0xFFED0068,\
0xFFED0090,	0xFFED00AC,\
0xFFED00BC,	0xFFED00C8,\
0xFFED00D4,	0xFFED00EC,\
0xFFED0100,	0xFFED0138,\
0xFFED0300,	0xFFED0358,\
0xFFED038C,	0xFFED038C,\
0xFFED0360,	0xFFED03B4,\
0xFFED03BC,	0xFFED03D0,\
0xFFED0FF0,	0xFFED0FF0,\
}

#define mipi_csi2_tx_1_dump_field \
{\
0xFFED5000,	0xFFED5008,\
0xFFED5044,	0xFFED5044,\
0xFFED5058,	0xFFED5068,\
0xFFED5090,	0xFFED50AC,\
0xFFED50BC,	0xFFED50C8,\
0xFFED50D4,	0xFFED50EC,\
0xFFED5100,	0xFFED5138,\
0xFFED5300,	0xFFED5358,\
0xFFED538C,	0xFFED538C,\
0xFFED5360,	0xFFED53B4,\
0xFFED53BC,	0xFFED53D0,\
0xFFED5FF0,	0xFFED5FF0,\
}

#define gen_reg_depth_top_dump_field \
{\
0xfffa0000,	0xfffa0004,\
0xfffa0050,	0xfffa0050,\
0xfffa0100,	0xfffa0104,\
0xfffa0128,	0xfffa0128,\
0xfffa0200,	0xfffa0234,\
0xfffa0f00,	0xfffa0f00,\
}

#define gen_reg_dpc_top_dump_field \
{\
0xfff00000,	0xfff00004,\
0xfff00010,	0xfff00010,\
0xfff00100,	0xfff00104,\
0xfff00128,	0xfff00128,\
0xfff00f00,	0xfff00f00,\
}

#define gen_reg_hdr_top_dump_field \
{\
0xfff50000,	0xfff50004,\
0xfff50100,	0xfff50104,\
0xfff50128,	0xfff50128,\
0xfff50f00,	0xfff50f00,\
}

#define gen_reg_irp_top_dump_field \
{\
0xffec3000,	0xffec3004,\
0xffec3010,	0xffec3018,\
0xffec3100,	0xffec3104,\
0xffec3128,	0xffec312c,\
0xffec3f00,	0xffec3f00,\
}

#define gen_reg_standby_top_dump_field \
{\
0xffec4000,	0xffec4004,\
0xffec4010,	0xffec4020,\
0xffec4030,	0xffec4044,\
0xffec4100,	0xffec4104,\
0xffec4128,	0xffec4128,\
0xffec4f00,	0xffec4f00,\
}

#define gen_reg_tx_top_dump_field \
{\
0xffec0000,	0xffec0008,\
0xffec0010,	0xffec0018,\
0xffec0310,	0xffec0318,\
0xffec0f00,	0xffec0f00,\
}

#define id_det_a_0_dump_field \
{\
0xfff01000,	0xfff01038,\
0xfff01050,	0xfff0110c,\
0xfff01150,	0xfff01158,\
0xfff0117c,	0xfff011c8,\
0xfff011f4,	0xfff01208,\
0xfff012a0,	0xfff012b0,\
0xfff012dc,	0xfff012e4,\
}

#define id_det_a_1_dump_field \
{\
0xfff02000,	0xfff02038,\
0xfff02050,	0xfff0210c,\
0xfff02150,	0xfff02158,\
0xfff0217c,	0xfff021c8,\
0xfff021f4,	0xfff02208,\
0xfff022a0,	0xfff022b0,\
0xfff022dc,	0xfff022e4,\
}

#define bayer_binning_a_0_dump_field \
{\
0xfff54000,	0xfff54008,\
0xfff5401c,	0xfff54068,\
0xfff54070,	0xfff54094,\
}

#define bayer_binning_a_1_dump_field \
{\
0xfff55000,	0xfff55008,\
0xfff5501c,	0xfff55068,\
0xfff55070,	0xfff55094,\
}

#define bayer_scl_a_0_dump_field \
{\
0xfffa7000,	0xfffa70d0,\
0xfffa7100,	0xfffa7108,\
0xfffa7110,	0xfffa7118,\
0xfffa7200,	0xfffa7200,\
}

#define bayer_scl_a_1_dump_field \
{\
0xfffa8000,	0xfffa80d0,\
0xfffa8100,	0xfffa8108,\
0xfffa8110,	0xfffa8118,\
0xfffa8200,	0xfffa8200,\
}

#define rlb_a_0_dump_field \
{\
0xfff05000,	0xfff0500c,\
0xfff05018,	0xfff0501c,\
0xfff05028,	0xfff0502c,\
0xfff05058,	0xfff05098,\
0xfff050b0,	0xfff050b0,\
0xfff050b8,	0xfff050c8,\
0xfff050d4,	0xfff050e0,\
0xfff050f0,	0xfff050f0,\
0xfff05100,	0xfff05108,\
0xfff05110,	0xfff05110,\
0xfff05120,	0xfff05138,\
}

#define rlb_b_0_dump_field \
{\
0xfff06000,	0xfff0600c,\
0xfff06018,	0xfff0601c,\
0xfff06028,	0xfff0602c,\
0xfff06058,	0xfff06098,\
0xfff060b0,	0xfff060b0,\
0xfff060b8,	0xfff060c8,\
0xfff060d4,	0xfff060e0,\
0xfff060f0,	0xfff060f0,\
0xfff06100,	0xfff06108,\
0xfff06110,	0xfff06110,\
0xfff06120,	0xfff06138,\
}


#define mipi_csi2_rx_0_dump_field \
{\
0xfff92000,	0xfff92004,\
0xfff9200c,	0xfff9208c,\
0xfff92100,	0xfff92148,\
0xfff9215c,	0xfff9217c,\
0xfff92188,	0xfff9218c,\
0xfff92198,	0xfff921dc,\
0xfff921f8,	0xfff922d4,\
}

#define mipi_csi2_rx_1_dump_field \
{\
0xfff95000,	0xfff95004,\
0xfff9500c,	0xfff9508c,\
0xfff95100,	0xfff95148,\
0xfff9515c,	0xfff9517c,\
0xfff95188,	0xfff9518c,\
0xfff95198,	0xfff951dc,\
0xfff951f8,	0xfff952d4,\
}

#define dg_ca_a_0_dump_field \
{\
0xfff8b000,	0xfff8b074,\
}

#define dg_mcc_a_0_dump_field \
{\
0xfff8a000, 0xfff8a024,\
0xfff8a0b0, 0xfff8a170,\
}

#define dp_top_a_0_dump_field \
{\
0xfff88000, 0xfff88090,\
0xfff88100, 0xfff88104,\
0xfff88200, 0xfff88240,\
0xfff88280, 0xfff88280,\
0xfff88300, 0xfff88300,\
}

#define lvhwirp_top_a_0_dump_field \
{\
0xfff30000, 0xfff30018,\
0xfff30100, 0xfff30110,\
0xfff30200, 0xfff30238,\
0xfff30400, 0xfff30418,\
0xfff31000, 0xfff311ec,\
0xfff31300, 0xfff31310,\
0xfff32000, 0xfff32ffc,\
0xfff33000, 0xfff33004,\
0xfff33100, 0xfff33188,\
0xfff33200, 0xfff34204,\
0xfff34300, 0xfff35ddc,\
0xfff36000, 0xfff37000,\
}

#define lvhwirp_top_b_0_dump_field \
{\
0xfff20000, 0xfff20018,\
0xfff20100, 0xfff20110,\
0xfff20200, 0xfff20238,\
0xfff20400, 0xfff20418,\
0xfff21000, 0xfff211ec,\
0xfff21300, 0xfff21310,\
0xfff22000, 0xfff22ffc,\
0xfff23000, 0xfff23004,\
0xfff23100, 0xfff23188,\
0xfff23200, 0xfff24204,\
0xfff24300, 0xfff25ddc,\
0xfff26000, 0xfff27000,\
}

#define lvlumanr_a_0_dump_field \
{\
0xfffa1000, 0xfffa1018,\
0xfffa1040, 0xfffa1098,\
0xfffa10A4, 0xfffa10C8,\
}

#define lvlumanr_a_1_dump_field \
{\
0xfffa2000, 0xfffa2018,\
0xfffa2040, 0xfffa2098,\
0xfffa20A4, 0xfffa20C8,\
}

#define lvsharp_a_0_dump_field \
{\
0xfffa5000, 0xfffa5028,\
0xfffa5034, 0xfffa5034,\
0xfffa5044, 0xfffa5060,\
0xfffa5070, 0xfffa5070,\
0xfffa50fc, 0xfffa51b0,\
0xfffa5208, 0xfffa521c,\
0xfffa5250, 0xfffa5264,\
0xfffa5270, 0xfffa5270,\
0xfffa5300, 0xfffa5308,\
}

#define lvsharp_a_1_dump_field \
{\
0xfffa6000, 0xfffa6028,\
0xfffa6034, 0xfffa6034,\
0xfffa6044, 0xfffa6060,\
0xfffa6070, 0xfffa6070,\
0xfffa60fc, 0xfffa61b0,\
0xfffa6208, 0xfffa621c,\
0xfffa6250, 0xfffa6264,\
0xfffa6270, 0xfffa6270,\
0xfffa6300, 0xfffa6308,\
}

#define rectify_a_0_dump_field \
{\
0xFFF8C000, 0xFFF8C008,\
0xFFF8C010, 0xFFF8C010,\
0xFFF8C024, 0xFFF8C024,\
0xFFF8C040, 0xFFF8C040,\
0xFFF8C050, 0xFFF8C050,\
0xFFF8C060, 0xFFF8C060,\
0xFFF8C080, 0xFFF8C084,\
0xFFF8C100, 0xFFF8C108,\
0xFFF8C110, 0xFFF8C110,\
0xFFF8C17C, 0xFFF8C1A8,\
0xFFF8C380, 0xFFF8C3A0,\
0xFFF8C400, 0xFFF8C408,\
0xFFF8C480, 0xFFF8C480,\
0xFFF8C50C, 0xFFF8C518,\
0xFFF8C540, 0xFFF8C540,\
0xFFF8C550, 0xFFF8C550,\
0xFFF8C5F0, 0xFFF8C5F0,\
0xFFF8C900, 0xFFF8C908,\
0xFFF8C910, 0xFFF8C910,\
0xFFF8C97C, 0xFFF8C9A8,\
0xFFF8CB80, 0xFFF8CBA0,\
0xFFF8CC00, 0xFFF8CC00,\
0xFFF8CC08, 0xFFF8CC08,\
0xFFF8CC80, 0xFFF8CC80,\
0xFFF8CD0C, 0xFFF8CD18,\
0xFFF8CD40, 0xFFF8CD40,\
0xFFF8CD50, 0xFFF8CD50,\
0xFFF8CDF0, 0xFFF8CDF0,\
0xFFF8CE04, 0xFFF8CE38,\
0xFFF8CE40, 0xFFF8CE48,\
0xFFF8CF00, 0xFFF8CFFC,\
}

#define rectify_b_0_dump_field \
{\
0xFFF8D000, 0xFFF8D008,\
0xFFF8D010, 0xFFF8D010,\
0xFFF8D024, 0xFFF8D024,\
0xFFF8D040, 0xFFF8D040,\
0xFFF8D050, 0xFFF8D050,\
0xFFF8D060, 0xFFF8D060,\
0xFFF8D080, 0xFFF8D084,\
0xFFF8D100, 0xFFF8D108,\
0xFFF8D110, 0xFFF8D110,\
0xFFF8D17C, 0xFFF8D1A8,\
0xFFF8D380, 0xFFF8D3A0,\
0xFFF8D400, 0xFFF8D408,\
0xFFF8D480, 0xFFF8D480,\
0xFFF8D50C, 0xFFF8D518,\
0xFFF8D540, 0xFFF8D540,\
0xFFF8D550, 0xFFF8D550,\
0xFFF8D5F0, 0xFFF8D5F0,\
0xFFF8D900, 0xFFF8D908,\
0xFFF8D910, 0xFFF8D910,\
0xFFF8D97C, 0xFFF8D9A8,\
0xFFF8DB80, 0xFFF8DBA0,\
0xFFF8DC00, 0xFFF8DC00,\
0xFFF8DC08, 0xFFF8DC08,\
0xFFF8DC80, 0xFFF8DC80,\
0xFFF8DD0C, 0xFFF8DD18,\
0xFFF8DD40, 0xFFF8DD40,\
0xFFF8DD50, 0xFFF8DD50,\
0xFFF8DDF0, 0xFFF8DDF0,\
0xFFF8DE04, 0xFFF8DE38,\
0xFFF8DE40, 0xFFF8DE48,\
0xFFF8DF00, 0xFFF8DFFC,\
}

#define hdr_ae_a_0_dump_field \
{\
	0xFFF59000, 0xFFF59450,\
}

#define hdr_ae_a_1_dump_field \
{\
	0xFFF5A000, 0xFFF5A450,\
}

#define DEPTH_FLOWMGR_INFO_VER 0xc
/* V11 */
typedef struct {
	/* General info */
	u8 ValidFlag;
	u8 Ver;
	u8 ALDU_MainVer;
	u8 ALDU_SubVer;

	struct {
		u8  ValidDepthFlowDebugInfo;
		u8  DepthFlowModeId;
		u16 refImg1_Width;
		u16 refImg1_Height;
		u16 refImg2_Width;
		u16 refImg2_Height;
		u16 refImg3_Width;
		u16 refImg3_Height;
		u16 DepthImgKernelSizeRatio;
		u16 DepthType;
		u16 DepthGroundType;
		u32 DephtProcTime;
		u32 BlendingProcTime;
		u32 DepthProcCnt;
		u8  FeatureFlag;
		u8  InvRECT_BypassLDC;
		u8 PerformanceFlag;
		u8 NormalKernalSizeMatch;
		u8 GroundKernalSizeMatch;
		u8 NormalKernelSizeIdx;
		u8 GroundKernelSizeIdx;
	} tDepthFlowDebugInfo;

	/* Packdata */
	struct {
		u8 ValidDepthPacadataDebugInfo;
		u8 ValidPackdata;
		u8 ValidDisparityToDistanceTable;
		u8 HighDistortionRate;
		u8 PackdataNormalIdx;
		u8 PackdataGroundIdx;
		u8 PackdataRGBIdx;
		u8 AbsoluteGlobalVal;
		u8 HwCoefficient;
		u16 HighDistortionIdx;
		u8 PackdataSource;
		u16 WOIMainXBase;
		u16 WOIMainYBase;
		u16 WOIMainXLength;
		u16 WOIMainYLength;
		u16 WOISubXBase;
		u16 WOISubYBase;
		u16 WOISubXLength;
		u16 WOISubYLength;
		s16 SubCamRectShift;
		u32 PackdataAddr;
		u32 PackdataChkSum;
		u32 IntrinsicK_Main[9];
		u32 AlDUErrCode;
		u8  reserve;
		u8 ConvertMatch;
		u8 ConvertCount;
		u8 NormalRectQPReadPingPongIdx;
	} tDepthPackdataDebugInfo;

	/* Detph utility */
	struct {
		u8  ValidDepthUtiDebugInfo;
		u8  ValidBlendingTable;
		u16 DistanceVal[3];
		u8  BlendingTableSource;
		u16 BlendingTableSize;
		u16 BlendingStartLine;
		u8  ValidQmerge;
		u8 QmergeBinCameraID;
		u8  DWUpdated;
	} tDepthUtiDebugInfo;
} DEPTHFLOWMGR_DEBUG_INFO;

enum {
	RESOLUTION_16_9  = 0,
	RESOLUTION_16_10 = 1,
	RESOLUTION_4_3   = 2,
	RESOLUTION_OTHER = 3
};

typedef enum {
	DEPTHPACKDATA_SRC_NULL,
	DEPTHPACKDATA_SRC_CMD, /* receive packdata via command */
	DEPTHPACKDATA_SRC_OTP, /* receive packdata from OTP, EEPROM (ISP read via I2C) */
} DEPTHPACKDATA_SRC;

typedef enum {
	DEPTHBLENDING_SRC_NULL,
	DEPTHBLENDING_SRC_CMD,	/* AP send blending table */
	DEPTHBLENDING_SRC_ISP,	/* ISP calculate blending table */
} DEPTHBLENDING_SRC;

typedef struct {
	u16 MetaDataVer;
	u16 SCID;
	u8  RxSwap;
	u8  TxSwap;
	u8  SensorType;
	u8  HDRPPPipeIdx;
	u16 LPPWidth;
	u16 LPPHeight;
	u16 RPPWidth;
	u16 RPPHeight;
} CAPFLOWMGR_META_COMMON;

/**
@struct CAPFLOWMGR_META_PIPE
@brief Pipe paramters
*/
typedef struct {
	u32 FrameIndex;
	u16 SrcImgWidth;
	u16 SrcImgHeight;
	u32 ColorOrder;
	u32 ExpTime;
	s16 BV;
	u16 ISO;
	u16 AD_Gain;
	u16 AWB_RGain;
	u16 AWB_GGain;
	u16 AWB_BGain;

	u16 BlackOffset_R;
	u16 BlackOffset_G;
	u16 BlackOffset_B;
	u16 Crop_X;
	u16 Crop_Y;
	u16 Crop_Width;
	u16 Crop_Height;
	u16 ScalarWidth;
	u16 ScalarHeight;

	s16  VCM_macro;
	s16  VCM_infinity;
	s16  VCM_CurStep;
	u32 Depth_VCMStatus;
} CAPFLOWMGR_META_PIPE; /* For MetaData */

/**
@struct CAPFLOWMGR_META_DEPTH
@brief Depth paramters
*/
typedef struct {
	u32 DepthIndex;
	u32 ReferenceFrameIndex;
	u16 DepthWidth;
	u16 DepthHeight;
	u16 DepthType;
	s16 awDepth_VCMStep[2];
	u8  aucMediaData[34];
} CAPFLOWMGR_META_DEPTH; /* For MetaData */

/**
@struct CAPFLOWMGR_META_HDR
@brief Depth paramters
*/
typedef struct {
	u8  HDR_Type;
	u8  HDR_OutputMode;
	u8  ZZ_ColorOrder_R;
	u8  ZZ_ColorOrder_Gr;
	u8  ZZ_ColorOrder_B;
	u8  ExpRatio;
	u16 RDN_AdapParam;
	u8  Block_Operation_Mode;
	u8  PPInfo;
	u16 uw0EV_gain;
	s16  LongImage_0EV_Target;
	s16  ShortImage_0EV_Target;
	u16 GhostPrevent_low;
	u16 GhostPrevent_high;
	u16 HDR_AE_weighting_table_ID;
	u16 HDR_Compress_tone_ID;
} CAPFLOWMGR_META_HDR; /* For MetaData */

typedef struct {
	u16 SHDTableW;
	u16 SHDTableH;
	u16 SHDNoWOIOutWidth;
	u16 SHDNoWOIOutHeight;
	u16 SHD_WOI_X;
	u16 SHD_WOI_Y;
	u16 SHD_WOI_Width;
	u16 SHD_WOI_Height;
} CAPFLOWMGR_SHADING_DEBUG_INFO; /* For Metadata, check if it will be used in SK1 */

/**
@struct CAPFLOWMGR_PROJECTOR_STATUS
@brief Depth paramters
*/
typedef struct {
	u8 TurnOn;
	u8 Level;
} CAPFLOWMGR_PROJECTOR_STATUS;

typedef struct {
	u16 XBase;
	u16 YBase;
	u16 XLength;
	u16 YLength;
} DEPTHPACKDATA_WOI_INFO;

/**
@struct CAPFLOWMGR_PROJECTOR_STATUS
@brief Depth paramters
*/
typedef struct {
	u8 TurnOn;
} CAPFLOWMGR_GPIO_DEV;

/**
@struct CAPFLOWMGR_META_SWDEBUG
@brief Depth paramters
*/
typedef struct {
	u8 aucModuleByPass[12];  /* 12 */
	u16 DepthInWidth; /* 2 */
	u16 DepthInHeight; /* 2 */
	CAPFLOWMGR_SHADING_DEBUG_INFO tPipeSHDInfo[2]; /* 32 */
	DEPTHPACKDATA_WOI_INFO tPipeDepthCaliWOI[2]; /* 16 */
	u64  SysTimeus;   /* 8 */
	CAPFLOWMGR_PROJECTOR_STATUS tProjectorStatus[8]; /* 16 */
	CAPFLOWMGR_GPIO_DEV tGPIO_Device[2]; /* 2 */
	u8 Resv1[6];
	u64  IDD0_ISRTimeus;
	u64  IDD1_ISRTimeus;
	u8 aucResv2[16];
} CAPFLOWMGR_META_SWDEBUG; /* For MetaData */

/**
@struct CAPFLOWMGR_DEPTH
@brief Depth paramters
*/
typedef struct {
	u16  OriginalWidth;
	u16  OriginalHeight;
	u16  WOIWidth;         /* Sensor Crop */
	u16  WOIHeight;        /* Sensor Crop */
	u16  WOI_X;            /* Sensor Crop */
	u16  WOI_Y;            /* Sensor Crop */
	u16  uw2ndWOIWidth;      /* Internal Module Crop */
	u16  uw2ndWOIHeight;     /* Internal Module Crop */
	u16  uw2ndWOI_X;         /* Internal Module Crop */
	u16  uw2ndWOI_Y;         /* Internal Module Crop */
	u16  OutputWidth;
	u16  OutputHeight;
	u16  Output_X;
	u16  Output_Y;
} CAPFLOWMGR_DEPTH_WOI_INFO;

typedef struct {
	u32 Qmerge_Ver;
	u32 audTool_Ver[3];
	u32 audTuning_Ver[3];
	u8  aucVerifyDebug[2];
	u16 uw2PD_Info;
	u8  aucEngineEnable[8];
	s16  awCCM[2][9];
	u32 ExpRatio;
	u32 audParaAddr[2][80];
	u8  aucResv[32];
} CAPFLOWMGR_META_IQDEBUG; /* For Metadata, check if it will be used in SK1 */

/**
@struct CAPFLOWMGR_METADATA
@brief scenario paramters
*/
typedef struct {
	CAPFLOWMGR_META_COMMON tCommonInfo;
	CAPFLOWMGR_META_PIPE tPipe0Info;
	CAPFLOWMGR_META_PIPE tPipe1Info;
	CAPFLOWMGR_META_DEPTH tDpethInfo;
	CAPFLOWMGR_META_HDR tHDRInfo;
	u8 aucMetaData_reserved_0[12];
	CAPFLOWMGR_META_SWDEBUG tSWDebugInfo;
	CAPFLOWMGR_META_IQDEBUG tIQDebugInfo;
	u8 aucMetaData_reserved_1[920];
	u32 audHDRAEHistogram_Short[260];
	u32 audHDRAEHistogram_Long[260];
} CAPFLOWMGR_METADATA;

typedef enum {
	E_FEC_SENSOR_MODE_NORMAL,
	E_FEC_SENSOR_MODE_MASTER,
	E_FEC_SENSOR_MODE_SLAVE,
	E_FEC_SENSOR_MODE_MAX
} E_FEC_SENSOR_MODE;

typedef struct {
	u16 VTS;                  /* line_length_pck */
	u16 HTS;                  /* frame_length_line */
	u16 ImageWidth;           /* image width */
	u16 ImageHeight;          /* image height */
	u32 FrameTime;            /* frame time (us) */
	u32 FrameRate;            /* frame rate (3003 = 30.03fps) */
	u32 ExpTime;              /* expousre time (us) */
	u32 Gain;                 /* gain (150 = 1.5x) */
	E_FEC_SENSOR_MODE SensorMode; /* see E_FEC_SENSOR_MODE */
} FEC_SENSOR_REAL_INFO;

typedef enum {
	E_LED_PROJECTOR = 0,
	E_LED_FLOOD,
} E_LED_TYPE;


typedef struct{
	u8  Type;         /* 0 for projector, 1 for flood */
	u16 Level;        /* current sw level (0~255) */
	u16 Current;      /* sw level mapping to current (mA) */
	u16 MaxCurrent;   /* support maximum current (mA) */
	u16 ErrStatus;    /* driver IC status (if support) */
	u16 Temperature;  /* error occurs, and record temperature from thermal sensor */
	u32 errCode;      /* errCode, see ProjectorCtrl_Err.h */
} PROJECTOR_INFO;

typedef struct {
	char name[32];
	u32  addr;
	char mapping[32];
} GPIO;


/******Public Function Prototype******/
extern void mini_isp_chip_base_define_module_reg_dump(
				char *dest_path, char *module_name);

extern errcode mini_isp_chip_base_dump_bypass_mode_register(char *dest_path);

extern errcode mini_isp_chip_base_dump_normal_mode_register(char *dest_path);

extern errcode mini_isp_chip_base_dump_irp_and_depth_based_register(void);
#endif
