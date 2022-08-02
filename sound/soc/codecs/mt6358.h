/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6358.h  --  mt6358 ALSA SoC audio codec driver
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: KaiChieh Chuang <kaichieh.chuang@mediatek.com>
 */

#ifndef _MT6358_H_
#define _MT6358_H_

/*************Register Bit Define*************/
#define MT6358_TOP0_ID                       0x0
#define MT6358_SMT_CON1                      0x30
#define MT6358_DRV_CON2                      0x3a
#define MT6358_DRV_CON3                      0x3c
#define MT6358_DRV_CON4                      0x3e
#define MT6358_GPIO_DIR0                     0x88
#define MT6358_GPIO_DIR0_SET                 0x8a
#define MT6358_GPIO_DIR0_CLR                 0x8c
#define MT6358_GPIO_MODE2                    0xd8
#define MT6358_GPIO_MODE2_SET                0xda
#define MT6358_GPIO_MODE2_CLR                0xdc
#define MT6358_GPIO_MODE3                    0xde
#define MT6358_GPIO_MODE3_SET                0xe0
#define MT6358_GPIO_MODE3_CLR                0xe2
#define MT6358_TOP_CKPDN_CON0                0x10c
#define MT6358_TOP_CKPDN_CON0_SET            0x10e
#define MT6358_TOP_CKPDN_CON0_CLR            0x110
#define MT6358_TOP_CKHWEN_CON0               0x12a
#define MT6358_TOP_CKHWEN_CON0_SET           0x12c
#define MT6358_TOP_CKHWEN_CON0_CLR           0x12e
#define MT6358_OTP_CON0                      0x38a
#define MT6358_OTP_CON1                      0x38c
#define MT6358_OTP_CON2                      0x38e
#define MT6358_OTP_CON3                      0x390
#define MT6358_OTP_CON4                      0x392
#define MT6358_OTP_CON5                      0x394
#define MT6358_OTP_CON6                      0x396
#define MT6358_OTP_CON7                      0x398
#define MT6358_OTP_CON8                      0x39a
#define MT6358_OTP_CON9                      0x39c
#define MT6358_OTP_CON10                     0x39e
#define MT6358_OTP_CON11                     0x3a0
#define MT6358_OTP_CON12                     0x3a2
#define MT6358_OTP_CON13                     0x3a4
#define MT6358_OTP_CON14                     0x3a6
#define MT6358_DCXO_CW00                     0x788
#define MT6358_DCXO_CW00_SET                 0x78a
#define MT6358_DCXO_CW00_CLR                 0x78c
#define MT6358_DCXO_CW01                     0x78e
#define MT6358_DCXO_CW02                     0x790
#define MT6358_DCXO_CW03                     0x792
#define MT6358_DCXO_CW04                     0x794
#define MT6358_DCXO_CW05                     0x796
#define MT6358_DCXO_CW06                     0x798
#define MT6358_DCXO_CW07                     0x79a
#define MT6358_DCXO_CW08                     0x79c
#define MT6358_DCXO_CW09                     0x79e
#define MT6358_DCXO_CW10                     0x7a0
#define MT6358_DCXO_CW11                     0x7a2
#define MT6358_DCXO_CW11_SET                 0x7a4
#define MT6358_DCXO_CW11_CLR                 0x7a6
#define MT6358_DCXO_CW12                     0x7a8
#define MT6358_DCXO_CW13                     0x7aa
#define MT6358_DCXO_CW14                     0x7ac
#define MT6358_DCXO_CW15                     0x7ae
#define MT6358_DCXO_CW16                     0x7b0
#define MT6358_DCXO_CW17                     0x7b2
#define MT6358_DCXO_CW18                     0x7b4
#define MT6358_DCXO_CW19                     0x7b6
#define MT6358_DCXO_CW20                     0x7b8
#define MT6358_DCXO_CW21                     0x7ba
#define MT6358_DCXO_CW22                     0x7bc
#define MT6358_DCXO_CW23                     0x7be
#define MT6358_DCXO_CW24                     0x7c0
#define MT6358_AUXADC_DSN_ID                 0x1000
#define MT6358_AUXADC_DSN_REV0               0x1002
#define MT6358_AUXADC_DSN_DBI                0x1004
#define MT6358_AUXADC_DSN_FPI                0x1006
#define MT6358_AUXADC_ANA_CON0               0x1008
#define MT6358_AUXADC_DIG_1_DSN_ID           0x1080
#define MT6358_AUXADC_DIG_1_DSN_REV0         0x1082
#define MT6358_AUXADC_DIG_1_DSN_DBI          0x1084
#define MT6358_AUXADC_DIG_1_DSN_DXI          0x1086
#define MT6358_AUXADC_ADC0                   0x1088
#define MT6358_AUXADC_ADC1                   0x108a
#define MT6358_AUXADC_ADC2                   0x108c
#define MT6358_AUXADC_ADC3                   0x108e
#define MT6358_AUXADC_ADC4                   0x1090
#define MT6358_AUXADC_ADC5                   0x1092
#define MT6358_AUXADC_ADC6                   0x1094
#define MT6358_AUXADC_ADC7                   0x1096
#define MT6358_AUXADC_ADC8                   0x1098
#define MT6358_AUXADC_ADC9                   0x109a
#define MT6358_AUXADC_ADC10                  0x109c
#define MT6358_AUXADC_ADC11                  0x109e
#define MT6358_AUXADC_ADC12                  0x10a0
#define MT6358_AUXADC_ADC13                  0x10a2
#define MT6358_AUXADC_ADC14                  0x10a4
#define MT6358_AUXADC_ADC15                  0x10a6
#define MT6358_AUXADC_ADC16                  0x10a8
#define MT6358_AUXADC_ADC17                  0x10aa
#define MT6358_AUXADC_ADC18                  0x10ac
#define MT6358_AUXADC_ADC19                  0x10ae
#define MT6358_AUXADC_ADC20                  0x10b0
#define MT6358_AUXADC_ADC21                  0x10b2
#define MT6358_AUXADC_ADC22                  0x10b4
#define MT6358_AUXADC_ADC23                  0x10b6
#define MT6358_AUXADC_ADC24                  0x10b8
#define MT6358_AUXADC_ADC25                  0x10ba
#define MT6358_AUXADC_ADC26                  0x10bc
#define MT6358_AUXADC_ADC27                  0x10be
#define MT6358_AUXADC_ADC28                  0x10c0
#define MT6358_AUXADC_ADC29                  0x10c2
#define MT6358_AUXADC_ADC30                  0x10c4
#define MT6358_AUXADC_ADC31                  0x10c6
#define MT6358_AUXADC_ADC32                  0x10c8
#define MT6358_AUXADC_ADC33                  0x10ca
#define MT6358_AUXADC_ADC34                  0x10cc
#define MT6358_AUXADC_ADC35                  0x10ce
#define MT6358_AUXADC_ADC36                  0x10d0
#define MT6358_AUXADC_ADC37                  0x10d2
#define MT6358_AUXADC_ADC38                  0x10d4
#define MT6358_AUXADC_ADC39                  0x10d6
#define MT6358_AUXADC_ADC40                  0x10d8
#define MT6358_AUXADC_STA0                   0x10da
#define MT6358_AUXADC_STA1                   0x10dc
#define MT6358_AUXADC_STA2                   0x10de
#define MT6358_AUXADC_DIG_2_DSN_ID           0x1100
#define MT6358_AUXADC_DIG_2_DSN_REV0         0x1102
#define MT6358_AUXADC_DIG_2_DSN_DBI          0x1104
#define MT6358_AUXADC_DIG_2_DSN_DXI          0x1106
#define MT6358_AUXADC_RQST0                  0x1108
#define MT6358_AUXADC_RQST1                  0x110a
#define MT6358_AUXADC_DIG_3_DSN_ID           0x1180
#define MT6358_AUXADC_DIG_3_DSN_REV0         0x1182
#define MT6358_AUXADC_DIG_3_DSN_DBI          0x1184
#define MT6358_AUXADC_DIG_3_DSN_DXI          0x1186
#define MT6358_AUXADC_CON0                   0x1188
#define MT6358_AUXADC_CON0_SET               0x118a
#define MT6358_AUXADC_CON0_CLR               0x118c
#define MT6358_AUXADC_CON1                   0x118e
#define MT6358_AUXADC_CON2                   0x1190
#define MT6358_AUXADC_CON3                   0x1192
#define MT6358_AUXADC_CON4                   0x1194
#define MT6358_AUXADC_CON5                   0x1196
#define MT6358_AUXADC_CON6                   0x1198
#define MT6358_AUXADC_CON7                   0x119a
#define MT6358_AUXADC_CON8                   0x119c
#define MT6358_AUXADC_CON9                   0x119e
#define MT6358_AUXADC_CON10                  0x11a0
#define MT6358_AUXADC_CON11                  0x11a2
#define MT6358_AUXADC_CON12                  0x11a4
#define MT6358_AUXADC_CON13                  0x11a6
#define MT6358_AUXADC_CON14                  0x11a8
#define MT6358_AUXADC_CON15                  0x11aa
#define MT6358_AUXADC_CON16                  0x11ac
#define MT6358_AUXADC_CON17                  0x11ae
#define MT6358_AUXADC_CON18                  0x11b0
#define MT6358_AUXADC_CON19                  0x11b2
#define MT6358_AUXADC_CON20                  0x11b4
#define MT6358_AUXADC_AUTORPT0               0x11b6
#define MT6358_AUXADC_ACCDET                 0x11b8
#define MT6358_AUXADC_DBG0                   0x11ba
#define MT6358_AUXADC_DIG_3_ELR_NUM          0x11bc
#define MT6358_AUXADC_DIG_3_ELR0             0x11be
#define MT6358_AUXADC_DIG_3_ELR1             0x11c0
#define MT6358_AUXADC_DIG_3_ELR2             0x11c2
#define MT6358_AUXADC_DIG_3_ELR3             0x11c4
#define MT6358_AUXADC_DIG_3_ELR4             0x11c6
#define MT6358_AUXADC_DIG_3_ELR5             0x11c8
#define MT6358_AUXADC_DIG_3_ELR6             0x11ca
#define MT6358_AUXADC_DIG_3_ELR7             0x11cc
#define MT6358_AUXADC_DIG_3_ELR8             0x11ce
#define MT6358_AUXADC_DIG_3_ELR9             0x11d0
#define MT6358_AUXADC_DIG_3_ELR10            0x11d2
#define MT6358_AUXADC_DIG_3_ELR11            0x11d4
#define MT6358_AUXADC_DIG_3_ELR12            0x11d6
#define MT6358_AUXADC_DIG_3_ELR13            0x11d8
#define MT6358_AUXADC_DIG_4_DSN_ID           0x1200
#define MT6358_AUXADC_DIG_4_DSN_REV0         0x1202
#define MT6358_AUXADC_DIG_4_DSN_DBI          0x1204
#define MT6358_AUXADC_DIG_4_DSN_DXI          0x1206
#define MT6358_AUXADC_IMP0                   0x1208
#define MT6358_AUXADC_IMP1                   0x120a
#define MT6358_AUXADC_IMP2                   0x120c
#define MT6358_AUXADC_LBAT0                  0x120e
#define MT6358_AUXADC_LBAT1                  0x1210
#define MT6358_AUXADC_LBAT2                  0x1212
#define MT6358_AUXADC_LBAT3                  0x1214
#define MT6358_AUXADC_LBAT4                  0x1216
#define MT6358_AUXADC_LBAT5                  0x1218
#define MT6358_AUXADC_LBAT6                  0x121a
#define MT6358_AUXADC_BAT_TEMP_0             0x121c
#define MT6358_AUXADC_BAT_TEMP_1             0x121e
#define MT6358_AUXADC_BAT_TEMP_2             0x1220
#define MT6358_AUXADC_BAT_TEMP_3             0x1222
#define MT6358_AUXADC_BAT_TEMP_4             0x1224
#define MT6358_AUXADC_BAT_TEMP_5             0x1226
#define MT6358_AUXADC_BAT_TEMP_6             0x1228
#define MT6358_AUXADC_BAT_TEMP_7             0x122a
#define MT6358_AUXADC_LBAT2_1                0x122c
#define MT6358_AUXADC_LBAT2_2                0x122e
#define MT6358_AUXADC_LBAT2_3                0x1230
#define MT6358_AUXADC_LBAT2_4                0x1232
#define MT6358_AUXADC_LBAT2_5                0x1234
#define MT6358_AUXADC_LBAT2_6                0x1236
#define MT6358_AUXADC_LBAT2_7                0x1238
#define MT6358_AUXADC_MDRT_0                 0x123a
#define MT6358_AUXADC_MDRT_1                 0x123c
#define MT6358_AUXADC_MDRT_2                 0x123e
#define MT6358_AUXADC_MDRT_3                 0x1240
#define MT6358_AUXADC_MDRT_4                 0x1242
#define MT6358_AUXADC_DCXO_MDRT_1            0x1244
#define MT6358_AUXADC_DCXO_MDRT_2            0x1246
#define MT6358_AUXADC_NAG_0                  0x1248
#define MT6358_AUXADC_NAG_1                  0x124a
#define MT6358_AUXADC_NAG_2                  0x124c
#define MT6358_AUXADC_NAG_3                  0x124e
#define MT6358_AUXADC_NAG_4                  0x1250
#define MT6358_AUXADC_NAG_5                  0x1252
#define MT6358_AUXADC_NAG_6                  0x1254
#define MT6358_AUXADC_NAG_7                  0x1256
#define MT6358_AUXADC_NAG_8                  0x1258
#define MT6358_AUXADC_NAG_9                  0x125a
#define MT6358_AUXADC_RSV_1                  0x125c
#define MT6358_AUXADC_PRI_NEW                0x125e
#define MT6358_AUXADC_DCM_CON                0x1260
#define MT6358_AUXADC_SPL_LIST_0             0x1262
#define MT6358_AUXADC_SPL_LIST_1             0x1264
#define MT6358_AUXADC_SPL_LIST_2             0x1266
#define MT6358_LDO_VAUD28_CON0               0x1ac4
#define MT6358_LDO_VUSB_OP_EN                0x1b32
#define MT6358_LDO_VUSB_OP_EN_SET            0x1b34
#define MT6358_LDO_VUSB_OP_EN_CLR            0x1b36
#define MT6358_AUD_TOP_ID                    0x2200
#define MT6358_AUD_TOP_REV0                  0x2202
#define MT6358_AUD_TOP_DBI                   0x2204
#define MT6358_AUD_TOP_DXI                   0x2206
#define MT6358_AUD_TOP_CKPDN_TPM0            0x2208
#define MT6358_AUD_TOP_CKPDN_TPM1            0x220a
#define MT6358_AUD_TOP_CKPDN_CON0            0x220c
#define MT6358_AUD_TOP_CKPDN_CON0_SET        0x220e
#define MT6358_AUD_TOP_CKPDN_CON0_CLR        0x2210
#define MT6358_AUD_TOP_CKSEL_CON0            0x2212
#define MT6358_AUD_TOP_CKSEL_CON0_SET        0x2214
#define MT6358_AUD_TOP_CKSEL_CON0_CLR        0x2216
#define MT6358_AUD_TOP_CKTST_CON0            0x2218
#define MT6358_AUD_TOP_CLK_HWEN_CON0         0x221a
#define MT6358_AUD_TOP_CLK_HWEN_CON0_SET     0x221c
#define MT6358_AUD_TOP_CLK_HWEN_CON0_CLR     0x221e
#define MT6358_AUD_TOP_RST_CON0              0x2220
#define MT6358_AUD_TOP_RST_CON0_SET          0x2222
#define MT6358_AUD_TOP_RST_CON0_CLR          0x2224
#define MT6358_AUD_TOP_RST_BANK_CON0         0x2226
#define MT6358_AUD_TOP_INT_CON0              0x2228
#define MT6358_AUD_TOP_INT_CON0_SET          0x222a
#define MT6358_AUD_TOP_INT_CON0_CLR          0x222c
#define MT6358_AUD_TOP_INT_MASK_CON0         0x222e
#define MT6358_AUD_TOP_INT_MASK_CON0_SET     0x2230
#define MT6358_AUD_TOP_INT_MASK_CON0_CLR     0x2232
#define MT6358_AUD_TOP_INT_STATUS0           0x2234
#define MT6358_AUD_TOP_INT_RAW_STATUS0       0x2236
#define MT6358_AUD_TOP_INT_MISC_CON0         0x2238
#define MT6358_AUDNCP_CLKDIV_CON0            0x223a
#define MT6358_AUDNCP_CLKDIV_CON1            0x223c
#define MT6358_AUDNCP_CLKDIV_CON2            0x223e
#define MT6358_AUDNCP_CLKDIV_CON3            0x2240
#define MT6358_AUDNCP_CLKDIV_CON4            0x2242
#define MT6358_AUD_TOP_MON_CON0              0x2244
#define MT6358_AUDIO_DIG_DSN_ID              0x2280
#define MT6358_AUDIO_DIG_DSN_REV0            0x2282
#define MT6358_AUDIO_DIG_DSN_DBI             0x2284
#define MT6358_AUDIO_DIG_DSN_DXI             0x2286
#define MT6358_AFE_UL_DL_CON0                0x2288
#define MT6358_AFE_DL_SRC2_CON0_L            0x228a
#define MT6358_AFE_UL_SRC_CON0_H             0x228c
#define MT6358_AFE_UL_SRC_CON0_L             0x228e
#define MT6358_AFE_TOP_CON0                  0x2290
#define MT6358_AUDIO_TOP_CON0                0x2292
#define MT6358_AFE_MON_DEBUG0                0x2294
#define MT6358_AFUNC_AUD_CON0                0x2296
#define MT6358_AFUNC_AUD_CON1                0x2298
#define MT6358_AFUNC_AUD_CON2                0x229a
#define MT6358_AFUNC_AUD_CON3                0x229c
#define MT6358_AFUNC_AUD_CON4                0x229e
#define MT6358_AFUNC_AUD_CON5                0x22a0
#define MT6358_AFUNC_AUD_CON6                0x22a2
#define MT6358_AFUNC_AUD_MON0                0x22a4
#define MT6358_AUDRC_TUNE_MON0               0x22a6
#define MT6358_AFE_ADDA_MTKAIF_FIFO_CFG0     0x22a8
#define MT6358_AFE_ADDA_MTKAIF_FIFO_LOG_MON1 0x22aa
#define MT6358_AFE_ADDA_MTKAIF_MON0          0x22ac
#define MT6358_AFE_ADDA_MTKAIF_MON1          0x22ae
#define MT6358_AFE_ADDA_MTKAIF_MON2          0x22b0
#define MT6358_AFE_ADDA_MTKAIF_MON3          0x22b2
#define MT6358_AFE_ADDA_MTKAIF_CFG0          0x22b4
#define MT6358_AFE_ADDA_MTKAIF_RX_CFG0       0x22b6
#define MT6358_AFE_ADDA_MTKAIF_RX_CFG1       0x22b8
#define MT6358_AFE_ADDA_MTKAIF_RX_CFG2       0x22ba
#define MT6358_AFE_ADDA_MTKAIF_RX_CFG3       0x22bc
#define MT6358_AFE_ADDA_MTKAIF_TX_CFG1       0x22be
#define MT6358_AFE_SGEN_CFG0                 0x22c0
#define MT6358_AFE_SGEN_CFG1                 0x22c2
#define MT6358_AFE_ADC_ASYNC_FIFO_CFG        0x22c4
#define MT6358_AFE_DCCLK_CFG0                0x22c6
#define MT6358_AFE_DCCLK_CFG1                0x22c8
#define MT6358_AUDIO_DIG_CFG                 0x22ca
#define MT6358_AFE_AUD_PAD_TOP               0x22cc
#define MT6358_AFE_AUD_PAD_TOP_MON           0x22ce
#define MT6358_AFE_AUD_PAD_TOP_MON1          0x22d0
#define MT6358_AFE_DL_NLE_CFG                0x22d2
#define MT6358_AFE_DL_NLE_MON                0x22d4
#define MT6358_AFE_CG_EN_MON                 0x22d6
#define MT6358_AUDIO_DIG_2ND_DSN_ID          0x2300
#define MT6358_AUDIO_DIG_2ND_DSN_REV0        0x2302
#define MT6358_AUDIO_DIG_2ND_DSN_DBI         0x2304
#define MT6358_AUDIO_DIG_2ND_DSN_DXI         0x2306
#define MT6358_AFE_PMIC_NEWIF_CFG3           0x2308
#define MT6358_AFE_VOW_TOP                   0x230a
#define MT6358_AFE_VOW_CFG0                  0x230c
#define MT6358_AFE_VOW_CFG1                  0x230e
#define MT6358_AFE_VOW_CFG2                  0x2310
#define MT6358_AFE_VOW_CFG3                  0x2312
#define MT6358_AFE_VOW_CFG4                  0x2314
#define MT6358_AFE_VOW_CFG5                  0x2316
#define MT6358_AFE_VOW_CFG6                  0x2318
#define MT6358_AFE_VOW_MON0                  0x231a
#define MT6358_AFE_VOW_MON1                  0x231c
#define MT6358_AFE_VOW_MON2                  0x231e
#define MT6358_AFE_VOW_MON3                  0x2320
#define MT6358_AFE_VOW_MON4                  0x2322
#define MT6358_AFE_VOW_MON5                  0x2324
#define MT6358_AFE_VOW_SN_INI_CFG            0x2326
#define MT6358_AFE_VOW_TGEN_CFG0             0x2328
#define MT6358_AFE_VOW_POSDIV_CFG0           0x232a
#define MT6358_AFE_VOW_HPF_CFG0              0x232c
#define MT6358_AFE_VOW_PERIODIC_CFG0         0x232e
#define MT6358_AFE_VOW_PERIODIC_CFG1         0x2330
#define MT6358_AFE_VOW_PERIODIC_CFG2         0x2332
#define MT6358_AFE_VOW_PERIODIC_CFG3         0x2334
#define MT6358_AFE_VOW_PERIODIC_CFG4         0x2336
#define MT6358_AFE_VOW_PERIODIC_CFG5         0x2338
#define MT6358_AFE_VOW_PERIODIC_CFG6         0x233a
#define MT6358_AFE_VOW_PERIODIC_CFG7         0x233c
#define MT6358_AFE_VOW_PERIODIC_CFG8         0x233e
#define MT6358_AFE_VOW_PERIODIC_CFG9         0x2340
#define MT6358_AFE_VOW_PERIODIC_CFG10        0x2342
#define MT6358_AFE_VOW_PERIODIC_CFG11        0x2344
#define MT6358_AFE_VOW_PERIODIC_CFG12        0x2346
#define MT6358_AFE_VOW_PERIODIC_CFG13        0x2348
#define MT6358_AFE_VOW_PERIODIC_CFG14        0x234a
#define MT6358_AFE_VOW_PERIODIC_CFG15        0x234c
#define MT6358_AFE_VOW_PERIODIC_CFG16        0x234e
#define MT6358_AFE_VOW_PERIODIC_CFG17        0x2350
#define MT6358_AFE_VOW_PERIODIC_CFG18        0x2352
#define MT6358_AFE_VOW_PERIODIC_CFG19        0x2354
#define MT6358_AFE_VOW_PERIODIC_CFG20        0x2356
#define MT6358_AFE_VOW_PERIODIC_CFG21        0x2358
#define MT6358_AFE_VOW_PERIODIC_CFG22        0x235a
#define MT6358_AFE_VOW_PERIODIC_CFG23        0x235c
#define MT6358_AFE_VOW_PERIODIC_MON0         0x235e
#define MT6358_AFE_VOW_PERIODIC_MON1         0x2360
#define MT6358_AUDENC_DSN_ID                 0x2380
#define MT6358_AUDENC_DSN_REV0               0x2382
#define MT6358_AUDENC_DSN_DBI                0x2384
#define MT6358_AUDENC_DSN_FPI                0x2386
#define MT6358_AUDENC_ANA_CON0               0x2388
#define MT6358_AUDENC_ANA_CON1               0x238a
#define MT6358_AUDENC_ANA_CON2               0x238c
#define MT6358_AUDENC_ANA_CON3               0x238e
#define MT6358_AUDENC_ANA_CON4               0x2390
#define MT6358_AUDENC_ANA_CON5               0x2392
#define MT6358_AUDENC_ANA_CON6               0x2394
#define MT6358_AUDENC_ANA_CON7               0x2396
#define MT6358_AUDENC_ANA_CON8               0x2398
#define MT6358_AUDENC_ANA_CON9               0x239a
#define MT6358_AUDENC_ANA_CON10              0x239c
#define MT6358_AUDENC_ANA_CON11              0x239e
#define MT6358_AUDENC_ANA_CON12              0x23a0
#define MT6358_AUDDEC_DSN_ID                 0x2400
#define MT6358_AUDDEC_DSN_REV0               0x2402
#define MT6358_AUDDEC_DSN_DBI                0x2404
#define MT6358_AUDDEC_DSN_FPI                0x2406
#define MT6358_AUDDEC_ANA_CON0               0x2408
#define MT6358_AUDDEC_ANA_CON1               0x240a
#define MT6358_AUDDEC_ANA_CON2               0x240c
#define MT6358_AUDDEC_ANA_CON3               0x240e
#define MT6358_AUDDEC_ANA_CON4               0x2410
#define MT6358_AUDDEC_ANA_CON5               0x2412
#define MT6358_AUDDEC_ANA_CON6               0x2414
#define MT6358_AUDDEC_ANA_CON7               0x2416
#define MT6358_AUDDEC_ANA_CON8               0x2418
#define MT6358_AUDDEC_ANA_CON9               0x241a
#define MT6358_AUDDEC_ANA_CON10              0x241c
#define MT6358_AUDDEC_ANA_CON11              0x241e
#define MT6358_AUDDEC_ANA_CON12              0x2420
#define MT6358_AUDDEC_ANA_CON13              0x2422
#define MT6358_AUDDEC_ANA_CON14              0x2424
#define MT6358_AUDDEC_ANA_CON15              0x2426
#define MT6358_AUDDEC_ELR_NUM                0x2428
#define MT6358_AUDDEC_ELR_0                  0x242a
#define MT6358_AUDZCD_DSN_ID                 0x2480
#define MT6358_AUDZCD_DSN_REV0               0x2482
#define MT6358_AUDZCD_DSN_DBI                0x2484
#define MT6358_AUDZCD_DSN_FPI                0x2486
#define MT6358_ZCD_CON0                      0x2488
#define MT6358_ZCD_CON1                      0x248a
#define MT6358_ZCD_CON2                      0x248c
#define MT6358_ZCD_CON3                      0x248e
#define MT6358_ZCD_CON4                      0x2490
#define MT6358_ZCD_CON5                      0x2492
#define MT6358_ACCDET_DSN_DIG_ID             0x2500
#define MT6358_ACCDET_DSN_DIG_REV0           0x2502
#define MT6358_ACCDET_DSN_DBI                0x2504
#define MT6358_ACCDET_DSN_FPI                0x2506
#define MT6358_ACCDET_CON0                   0x2508
#define MT6358_ACCDET_CON1                   0x250a
#define MT6358_ACCDET_CON2                   0x250c
#define MT6358_ACCDET_CON3                   0x250e
#define MT6358_ACCDET_CON4                   0x2510
#define MT6358_ACCDET_CON5                   0x2512
#define MT6358_ACCDET_CON6                   0x2514
#define MT6358_ACCDET_CON7                   0x2516
#define MT6358_ACCDET_CON8                   0x2518
#define MT6358_ACCDET_CON9                   0x251a
#define MT6358_ACCDET_CON10                  0x251c
#define MT6358_ACCDET_CON11                  0x251e
#define MT6358_ACCDET_CON12                  0x2520
#define MT6358_ACCDET_CON13                  0x2522
#define MT6358_ACCDET_CON14                  0x2524
#define MT6358_ACCDET_CON15                  0x2526
#define MT6358_ACCDET_CON16                  0x2528
#define MT6358_ACCDET_CON17                  0x252a
#define MT6358_ACCDET_CON18                  0x252c
#define MT6358_ACCDET_CON19                  0x252e
#define MT6358_ACCDET_CON20                  0x2530
#define MT6358_ACCDET_CON21                  0x2532
#define MT6358_ACCDET_CON22                  0x2534
#define MT6358_ACCDET_CON23                  0x2536
#define MT6358_ACCDET_CON24                  0x2538
#define MT6358_ACCDET_CON25                  0x253a
#define MT6358_ACCDET_CON26                  0x253c
#define MT6358_ACCDET_CON27                  0x253e
#define MT6358_ACCDET_CON28                  0x2540
#define TOP0_ANA_ID_ADDR                               \
	MT6358_TOP0_ID
#define TOP0_ANA_ID_SFT                                0
#define TOP0_ANA_ID_MASK                               0xFF
#define TOP0_ANA_ID_MASK_SFT                           (0xFF << 0)
#define AUXADC_RQST_CH0_ADDR                           \
	MT6358_AUXADC_RQST0
#define AUXADC_RQST_CH0_SFT                            0
#define AUXADC_RQST_CH0_MASK                           0x1
#define AUXADC_RQST_CH0_MASK_SFT                       (0x1 << 0)
#define AUXADC_ACCDET_ANASWCTRL_EN_ADDR                \
	MT6358_AUXADC_CON15
#define AUXADC_ACCDET_ANASWCTRL_EN_SFT                 6
#define AUXADC_ACCDET_ANASWCTRL_EN_MASK                0x1
#define AUXADC_ACCDET_ANASWCTRL_EN_MASK_SFT            (0x1 << 6)

#define AUXADC_ACCDET_AUTO_SPL_ADDR                     \
	MT6358_AUXADC_ACCDET
#define AUXADC_ACCDET_AUTO_SPL_SFT                      0
#define AUXADC_ACCDET_AUTO_SPL_MASK                     0x1
#define AUXADC_ACCDET_AUTO_SPL_MASK_SFT                 (0x1 << 0)
#define AUXADC_ACCDET_AUTO_RQST_CLR_ADDR                \
	MT6358_AUXADC_ACCDET
#define AUXADC_ACCDET_AUTO_RQST_CLR_SFT                 1
#define AUXADC_ACCDET_AUTO_RQST_CLR_MASK                0x1
#define AUXADC_ACCDET_AUTO_RQST_CLR_MASK_SFT            (0x1 << 1)
#define AUXADC_ACCDET_DIG1_RSV0_ADDR                    \
	MT6358_AUXADC_ACCDET
#define AUXADC_ACCDET_DIG1_RSV0_SFT                     2
#define AUXADC_ACCDET_DIG1_RSV0_MASK                    0x3F
#define AUXADC_ACCDET_DIG1_RSV0_MASK_SFT                (0x3F << 2)
#define AUXADC_ACCDET_DIG0_RSV0_ADDR                    \
	MT6358_AUXADC_ACCDET
#define AUXADC_ACCDET_DIG0_RSV0_SFT                     8
#define AUXADC_ACCDET_DIG0_RSV0_MASK                    0xFF
#define AUXADC_ACCDET_DIG0_RSV0_MASK_SFT                (0xFF << 8)

#define RG_ACCDET_CK_PDN_ADDR                           \
	MT6358_AUD_TOP_CKPDN_CON0
#define RG_ACCDET_CK_PDN_SFT                            0
#define RG_ACCDET_CK_PDN_MASK                           0x1
#define RG_ACCDET_CK_PDN_MASK_SFT                       (0x1 << 0)
#define RG_ACCDET_RST_ADDR                              \
	MT6358_AUD_TOP_RST_CON0
#define RG_ACCDET_RST_SFT                               1
#define RG_ACCDET_RST_MASK                              0x1
#define RG_ACCDET_RST_MASK_SFT                          (0x1 << 1)
#define BANK_ACCDET_SWRST_ADDR                          \
	MT6358_AUD_TOP_RST_BANK_CON0
#define BANK_ACCDET_SWRST_SFT                           0
#define BANK_ACCDET_SWRST_MASK                          0x1
#define BANK_ACCDET_SWRST_MASK_SFT                      (0x1 << 0)
#define RG_INT_EN_ACCDET_ADDR                           \
	MT6358_AUD_TOP_INT_CON0
#define RG_INT_EN_ACCDET_SFT                            5
#define RG_INT_EN_ACCDET_MASK                           0x1
#define RG_INT_EN_ACCDET_MASK_SFT                       (0x1 << 5)
#define RG_INT_EN_ACCDET_EINT0_ADDR                     \
	MT6358_AUD_TOP_INT_CON0
#define RG_INT_EN_ACCDET_EINT0_SFT                      6
#define RG_INT_EN_ACCDET_EINT0_MASK                     0x1
#define RG_INT_EN_ACCDET_EINT0_MASK_SFT                 (0x1 << 6)
#define RG_INT_EN_ACCDET_EINT1_ADDR                     \
	MT6358_AUD_TOP_INT_CON0
#define RG_INT_EN_ACCDET_EINT1_SFT                      7
#define RG_INT_EN_ACCDET_EINT1_MASK                     0x1
#define RG_INT_EN_ACCDET_EINT1_MASK_SFT                 (0x1 << 7)
#define RG_INT_MASK_ACCDET_ADDR                         \
	MT6358_AUD_TOP_INT_MASK_CON0
#define RG_INT_MASK_ACCDET_SFT                          5
#define RG_INT_MASK_ACCDET_MASK                         0x1
#define RG_INT_MASK_ACCDET_MASK_SFT                     (0x1 << 5)
#define RG_INT_MASK_ACCDET_EINT0_ADDR                   \
	MT6358_AUD_TOP_INT_MASK_CON0
#define RG_INT_MASK_ACCDET_EINT0_SFT                    6
#define RG_INT_MASK_ACCDET_EINT0_MASK                   0x1
#define RG_INT_MASK_ACCDET_EINT0_MASK_SFT               (0x1 << 6)
#define RG_INT_MASK_ACCDET_EINT1_ADDR                   \
	MT6358_AUD_TOP_INT_MASK_CON0
#define RG_INT_MASK_ACCDET_EINT1_SFT                    7
#define RG_INT_MASK_ACCDET_EINT1_MASK                   0x1
#define RG_INT_MASK_ACCDET_EINT1_MASK_SFT               (0x1 << 7)
#define RG_INT_STATUS_ACCDET_ADDR                       \
	MT6358_AUD_TOP_INT_STATUS0
#define RG_INT_STATUS_ACCDET_SFT                        5
#define RG_INT_STATUS_ACCDET_MASK                       0x1
#define RG_INT_STATUS_ACCDET_MASK_SFT                   (0x1 << 5)
#define RG_INT_STATUS_ACCDET_EINT0_ADDR                 \
	MT6358_AUD_TOP_INT_STATUS0
#define RG_INT_STATUS_ACCDET_EINT0_SFT                  6
#define RG_INT_STATUS_ACCDET_EINT0_MASK                 0x1
#define RG_INT_STATUS_ACCDET_EINT0_MASK_SFT             (0x1 << 6)
#define RG_INT_STATUS_ACCDET_EINT1_ADDR                 \
	MT6358_AUD_TOP_INT_STATUS0
#define RG_INT_STATUS_ACCDET_EINT1_SFT                  7
#define RG_INT_STATUS_ACCDET_EINT1_MASK                 0x1
#define RG_INT_STATUS_ACCDET_EINT1_MASK_SFT             (0x1 << 7)
#define RG_INT_RAW_STATUS_ACCDET_ADDR                   \
	MT6358_AUD_TOP_INT_RAW_STATUS0
#define RG_INT_RAW_STATUS_ACCDET_SFT                    5
#define RG_INT_RAW_STATUS_ACCDET_MASK                   0x1
#define RG_INT_RAW_STATUS_ACCDET_MASK_SFT               (0x1 << 5)
#define RG_INT_RAW_STATUS_ACCDET_EINT0_ADDR             \
	MT6358_AUD_TOP_INT_RAW_STATUS0
#define RG_INT_RAW_STATUS_ACCDET_EINT0_SFT              6
#define RG_INT_RAW_STATUS_ACCDET_EINT0_MASK             0x1
#define RG_INT_RAW_STATUS_ACCDET_EINT0_MASK_SFT         (0x1 << 6)
#define RG_INT_RAW_STATUS_ACCDET_EINT1_ADDR             \
	MT6358_AUD_TOP_INT_RAW_STATUS0
#define RG_INT_RAW_STATUS_ACCDET_EINT1_SFT              7
#define RG_INT_RAW_STATUS_ACCDET_EINT1_MASK             0x1
#define RG_INT_RAW_STATUS_ACCDET_EINT1_MASK_SFT         (0x1 << 7)

#define RG_AUDPREAMPLON_ADDR                            \
	MT6358_AUDENC_ANA_CON0
#define RG_AUDPREAMPLON_SFT                             0
#define RG_AUDPREAMPLON_MASK                            0x1
#define RG_AUDPREAMPLON_MASK_SFT                        (0x1 << 0)
#define RG_CLKSQ_EN_ADDR                                \
	MT6358_AUDENC_ANA_CON6
#define RG_CLKSQ_EN_SFT                                 0
#define RG_CLKSQ_EN_MASK                                0x1
#define RG_CLKSQ_EN_MASK_SFT                            (0x1 << 0)
#define RG_AUDSPARE_ADDR                                \
	MT6358_AUDENC_ANA_CON6
#define RG_AUDPWDBMICBIAS0_ADDR                         \
	MT6358_AUDENC_ANA_CON9
#define RG_AUDPWDBMICBIAS0_SFT                          0
#define RG_AUDPWDBMICBIAS0_MASK                         0x1
#define RG_AUDPWDBMICBIAS0_MASK_SFT                     (0x1 << 0)
#define RG_AUDPWDBMICBIAS1_ADDR                         \
	MT6358_AUDENC_ANA_CON10
#define RG_AUDPWDBMICBIAS1_SFT                          0
#define RG_AUDPWDBMICBIAS1_MASK                         0x1
#define RG_AUDPWDBMICBIAS1_MASK_SFT                     (0x1 << 0)
#define RG_AUDMICBIAS1BYPASSEN_ADDR                     \
	MT6358_AUDENC_ANA_CON10
#define RG_AUDMICBIAS1BYPASSEN_SFT                      1
#define RG_AUDMICBIAS1BYPASSEN_MASK                     0x1
#define RG_AUDMICBIAS1BYPASSEN_MASK_SFT                 (0x1 << 1)
#define RG_AUDMICBIAS1LOWPEN_ADDR                       \
	MT6358_AUDENC_ANA_CON10
#define RG_AUDMICBIAS1LOWPEN_SFT                        2
#define RG_AUDMICBIAS1LOWPEN_MASK                       0x1
#define RG_AUDMICBIAS1LOWPEN_MASK_SFT                   (0x1 << 2)
#define RG_AUDMICBIAS1VREF_ADDR                         \
	MT6358_AUDENC_ANA_CON10
#define RG_AUDMICBIAS1VREF_SFT                          4
#define RG_AUDMICBIAS1VREF_MASK                         0x7
#define RG_AUDMICBIAS1VREF_MASK_SFT                     (0x7 << 4)
#define RG_AUDMICBIAS1DCSW1PEN_ADDR                     \
	MT6358_AUDENC_ANA_CON10
#define RG_AUDMICBIAS1DCSW1PEN_SFT                      8
#define RG_AUDMICBIAS1DCSW1PEN_MASK                     0x1
#define RG_AUDMICBIAS1DCSW1PEN_MASK_SFT                 (0x1 << 8)
#define RG_AUDMICBIAS1DCSW1NEN_ADDR                     \
	MT6358_AUDENC_ANA_CON10
#define RG_AUDMICBIAS1DCSW1NEN_SFT                      9
#define RG_AUDMICBIAS1DCSW1NEN_MASK                     0x1
#define RG_AUDMICBIAS1DCSW1NEN_MASK_SFT                 (0x1 << 9)
#define RG_BANDGAPGEN_ADDR                              \
	MT6358_AUDENC_ANA_CON10
#define RG_BANDGAPGEN_SFT                               12
#define RG_BANDGAPGEN_MASK                              0x1
#define RG_BANDGAPGEN_MASK_SFT                          (0x1 << 12)
#define RG_MTEST_EN_ADDR                                \
	MT6358_AUDENC_ANA_CON10
#define RG_MTEST_EN_SFT                                 13
#define RG_MTEST_EN_MASK                                0x1
#define RG_MTEST_EN_MASK_SFT                            (0x1 << 13)
#define RG_MTEST_SEL_ADDR                               \
	MT6358_AUDENC_ANA_CON10
#define RG_MTEST_SEL_SFT                                14
#define RG_MTEST_SEL_MASK                               0x1
#define RG_MTEST_SEL_MASK_SFT                           (0x1 << 14)
#define RG_MTEST_CURRENT_ADDR                           \
	MT6358_AUDENC_ANA_CON10
#define RG_MTEST_CURRENT_SFT                            15
#define RG_MTEST_CURRENT_MASK                           0x1
#define RG_MTEST_CURRENT_MASK_SFT                       (0x1 << 15)
#define RG_AUDACCDETMICBIAS0PULLLOW_ADDR                \
	MT6358_AUDENC_ANA_CON11
#define RG_AUDACCDETMICBIAS0PULLLOW_SFT                 0
#define RG_AUDACCDETMICBIAS0PULLLOW_MASK                0x1
#define RG_AUDACCDETMICBIAS0PULLLOW_MASK_SFT            (0x1 << 0)
#define RG_AUDACCDETMICBIAS1PULLLOW_ADDR                \
	MT6358_AUDENC_ANA_CON11
#define RG_AUDACCDETMICBIAS1PULLLOW_SFT                 1
#define RG_AUDACCDETMICBIAS1PULLLOW_MASK                0x1
#define RG_AUDACCDETMICBIAS1PULLLOW_MASK_SFT            (0x1 << 1)
#define RG_AUDACCDETVIN1PULLLOW_ADDR                    \
	MT6358_AUDENC_ANA_CON11
#define RG_AUDACCDETVIN1PULLLOW_SFT                     2
#define RG_AUDACCDETVIN1PULLLOW_MASK                    0x1
#define RG_AUDACCDETVIN1PULLLOW_MASK_SFT                (0x1 << 2)
#define RG_AUDACCDETVTHACAL_ADDR                        \
	MT6358_AUDENC_ANA_CON11
#define RG_AUDACCDETVTHACAL_SFT                         4
#define RG_AUDACCDETVTHACAL_MASK                        0x1
#define RG_AUDACCDETVTHACAL_MASK_SFT                    (0x1 << 4)
#define RG_AUDACCDETVTHBCAL_ADDR                        \
	MT6358_AUDENC_ANA_CON11
#define RG_AUDACCDETVTHBCAL_SFT                         5
#define RG_AUDACCDETVTHBCAL_MASK                        0x1
#define RG_AUDACCDETVTHBCAL_MASK_SFT                    (0x1 << 5)
#define RG_AUDACCDETTVDET_ADDR                          \
	MT6358_AUDENC_ANA_CON11
#define RG_AUDACCDETTVDET_SFT                           6
#define RG_AUDACCDETTVDET_MASK                          0x1
#define RG_AUDACCDETTVDET_MASK_SFT                      (0x1 << 6)
#define RG_ACCDETSEL_ADDR                               \
	MT6358_AUDENC_ANA_CON11
#define RG_ACCDETSEL_SFT                                7
#define RG_ACCDETSEL_MASK                               0x1
#define RG_ACCDETSEL_MASK_SFT                           (0x1 << 7)
#define RG_SWBUFMODSEL_ADDR                             \
	MT6358_AUDENC_ANA_CON11
#define RG_SWBUFMODSEL_SFT                              8
#define RG_SWBUFMODSEL_MASK                             0x1
#define RG_SWBUFMODSEL_MASK_SFT                         (0x1 << 8)
#define RG_SWBUFSWEN_ADDR                               \
	MT6358_AUDENC_ANA_CON11
#define RG_SWBUFSWEN_SFT                                9
#define RG_SWBUFSWEN_MASK                               0x1
#define RG_SWBUFSWEN_MASK_SFT                           (0x1 << 9)
#define RG_EINTCOMPVTH_ADDR                             \
	MT6358_AUDENC_ANA_CON11
#define RG_EINTCOMPVTH_SFT                              10
#define RG_EINTCOMPVTH_MASK                             0x1
#define RG_EINTCOMPVTH_MASK_SFT                         (0x1 << 10)
#define RG_EINTCONFIGACCDET_ADDR                        \
	MT6358_AUDENC_ANA_CON11
#define RG_EINTCONFIGACCDET_SFT                         11
#define RG_EINTCONFIGACCDET_MASK                        0x1
#define RG_EINTCONFIGACCDET_MASK_SFT                    (0x1 << 11)
#define RG_EINTHIRENB_ADDR                              \
	MT6358_AUDENC_ANA_CON11
#define RG_EINTHIRENB_SFT                               12
#define RG_EINTHIRENB_MASK                              0x1
#define RG_EINTHIRENB_MASK_SFT                          (0x1 << 12)
#define RG_ACCDET2AUXRESBYPASS_ADDR                     \
	MT6358_AUDENC_ANA_CON11
#define RG_ACCDET2AUXRESBYPASS_SFT                      13
#define RG_ACCDET2AUXRESBYPASS_MASK                     0x1
#define RG_ACCDET2AUXRESBYPASS_MASK_SFT                 (0x1 << 13)
#define RG_ACCDET2AUXBUFFERBYPASS_ADDR                  \
	MT6358_AUDENC_ANA_CON11
#define RG_ACCDET2AUXBUFFERBYPASS_SFT                   14
#define RG_ACCDET2AUXBUFFERBYPASS_MASK                  0x1
#define RG_ACCDET2AUXBUFFERBYPASS_MASK_SFT              (0x1 << 14)
#define RG_ACCDET2AUXSWEN_ADDR                          \
	MT6358_AUDENC_ANA_CON11
#define RG_ACCDET2AUXSWEN_SFT                           15
#define RG_ACCDET2AUXSWEN_MASK                          0x1
#define RG_ACCDET2AUXSWEN_MASK_SFT                      (0x1 << 15)

#define ACCDET_ANA_ID_ADDR                              \
	MT6358_ACCDET_DSN_DIG_ID
#define ACCDET_ANA_ID_SFT                               0
#define ACCDET_ANA_ID_MASK                              0xFF
#define ACCDET_ANA_ID_MASK_SFT                          (0xFF << 0)
#define ACCDET_DIG_ID_ADDR                              \
	MT6358_ACCDET_DSN_DIG_ID
#define ACCDET_DIG_ID_SFT                               8
#define ACCDET_DIG_ID_MASK                              0xFF
#define ACCDET_DIG_ID_MASK_SFT                          (0xFF << 8)
#define ACCDET_ANA_MINOR_REV_ADDR                       \
	MT6358_ACCDET_DSN_DIG_REV0
#define ACCDET_ANA_MINOR_REV_SFT                        0
#define ACCDET_ANA_MINOR_REV_MASK                       0xF
#define ACCDET_ANA_MINOR_REV_MASK_SFT                   (0xF << 0)
#define ACCDET_ANA_MAJOR_REV_ADDR                       \
	MT6358_ACCDET_DSN_DIG_REV0
#define ACCDET_ANA_MAJOR_REV_SFT                        4
#define ACCDET_ANA_MAJOR_REV_MASK                       0xF
#define ACCDET_ANA_MAJOR_REV_MASK_SFT                   (0xF << 4)
#define ACCDET_DIG_MINOR_REV_ADDR                       \
	MT6358_ACCDET_DSN_DIG_REV0
#define ACCDET_DIG_MINOR_REV_SFT                        8
#define ACCDET_DIG_MINOR_REV_MASK                       0xF
#define ACCDET_DIG_MINOR_REV_MASK_SFT                   (0xF << 8)
#define ACCDET_DIG_MAJOR_REV_ADDR                       \
	MT6358_ACCDET_DSN_DIG_REV0
#define ACCDET_DIG_MAJOR_REV_SFT                        12
#define ACCDET_DIG_MAJOR_REV_MASK                       0xF
#define ACCDET_DIG_MAJOR_REV_MASK_SFT                   (0xF << 12)
#define ACCDET_DSN_CBS_ADDR                             \
	MT6358_ACCDET_DSN_DBI
#define ACCDET_DSN_CBS_SFT                              0
#define ACCDET_DSN_CBS_MASK                             0x3
#define ACCDET_DSN_CBS_MASK_SFT                         (0x3 << 0)
#define ACCDET_DSN_BIX_ADDR                             \
	MT6358_ACCDET_DSN_DBI
#define ACCDET_DSN_BIX_SFT                              2
#define ACCDET_DSN_BIX_MASK                             0x3
#define ACCDET_DSN_BIX_MASK_SFT                         (0x3 << 2)
#define ACCDET_ESP_ADDR                                 \
	MT6358_ACCDET_DSN_DBI
#define ACCDET_ESP_SFT                                  8
#define ACCDET_ESP_MASK                                 0xFF
#define ACCDET_ESP_MASK_SFT                             (0xFF << 8)
#define ACCDET_DSN_FPI_ADDR                             \
	MT6358_ACCDET_DSN_FPI
#define ACCDET_DSN_FPI_SFT                              0
#define ACCDET_DSN_FPI_MASK                             0xFF
#define ACCDET_DSN_FPI_MASK_SFT                         (0xFF << 0)
#define AUDACCDETAUXADCSWCTRL_ADDR                      \
	MT6358_ACCDET_CON0
#define AUDACCDETAUXADCSWCTRL_SFT                       10
#define AUDACCDETAUXADCSWCTRL_MASK                      0x1
#define AUDACCDETAUXADCSWCTRL_MASK_SFT                  (0x1 << 10)
#define AUDACCDETAUXADCSWCTRL_SEL_ADDR                  \
	MT6358_ACCDET_CON0
#define AUDACCDETAUXADCSWCTRL_SEL_SFT                   11
#define AUDACCDETAUXADCSWCTRL_SEL_MASK                  0x1
#define AUDACCDETAUXADCSWCTRL_SEL_MASK_SFT              (0x1 << 11)
#define RG_AUDACCDETRSV_ADDR                            \
	MT6358_ACCDET_CON0
#define RG_AUDACCDETRSV_SFT                             13
#define RG_AUDACCDETRSV_MASK                            0x3
#define RG_AUDACCDETRSV_MASK_SFT                        (0x3 << 13)
#define ACCDET_EN_ADDR                                  \
	MT6358_ACCDET_CON1
#define ACCDET_EN_SFT                                   0
#define ACCDET_EN_MASK                                  0x1
#define ACCDET_EN_MASK_SFT                              (0x1 << 0)
#define ACCDET_SEQ_INIT_ADDR                            \
	MT6358_ACCDET_CON1
#define ACCDET_SEQ_INIT_SFT                             1
#define ACCDET_SEQ_INIT_MASK                            0x1
#define ACCDET_SEQ_INIT_MASK_SFT                        (0x1 << 1)
#define ACCDET_EINT0_EN_ADDR                            \
	MT6358_ACCDET_CON1
#define ACCDET_EINT0_EN_SFT                             2
#define ACCDET_EINT0_EN_MASK                            0x1
#define ACCDET_EINT0_EN_MASK_SFT                        (0x1 << 2)
#define ACCDET_EINT0_SEQ_INIT_ADDR                      \
	MT6358_ACCDET_CON1
#define ACCDET_EINT0_SEQ_INIT_SFT                       3
#define ACCDET_EINT0_SEQ_INIT_MASK                      0x1
#define ACCDET_EINT0_SEQ_INIT_MASK_SFT                  (0x1 << 3)
#define ACCDET_EINT1_EN_ADDR                            \
	MT6358_ACCDET_CON1
#define ACCDET_EINT1_EN_SFT                             4
#define ACCDET_EINT1_EN_MASK                            0x1
#define ACCDET_EINT1_EN_MASK_SFT                        (0x1 << 4)
#define ACCDET_EINT1_SEQ_INIT_ADDR                      \
	MT6358_ACCDET_CON1
#define ACCDET_EINT1_SEQ_INIT_SFT                       5
#define ACCDET_EINT1_SEQ_INIT_MASK                      0x1
#define ACCDET_EINT1_SEQ_INIT_MASK_SFT                  (0x1 << 5)
#define ACCDET_ANASWCTRL_SEL_ADDR                       \
	MT6358_ACCDET_CON1
#define ACCDET_ANASWCTRL_SEL_SFT                        6
#define ACCDET_ANASWCTRL_SEL_MASK                       0x1
#define ACCDET_ANASWCTRL_SEL_MASK_SFT                   (0x1 << 6)
#define ACCDET_CMP_PWM_EN_ADDR                          \
	MT6358_ACCDET_CON2
#define ACCDET_CMP_PWM_EN_SFT                           0
#define ACCDET_CMP_PWM_EN_MASK                          0x1
#define ACCDET_CMP_PWM_EN_MASK_SFT                      (0x1 << 0)
#define ACCDET_VTH_PWM_EN_ADDR                          \
	MT6358_ACCDET_CON2
#define ACCDET_VTH_PWM_EN_SFT                           1
#define ACCDET_VTH_PWM_EN_MASK                          0x1
#define ACCDET_VTH_PWM_EN_MASK_SFT                      (0x1 << 1)
#define ACCDET_MBIAS_PWM_EN_ADDR                        \
	MT6358_ACCDET_CON2
#define ACCDET_MBIAS_PWM_EN_SFT                         2
#define ACCDET_MBIAS_PWM_EN_MASK                        0x1
#define ACCDET_MBIAS_PWM_EN_MASK_SFT                    (0x1 << 2)
#define ACCDET_EINT0_PWM_EN_ADDR                        \
	MT6358_ACCDET_CON2
#define ACCDET_EINT0_PWM_EN_SFT                         3
#define ACCDET_EINT0_PWM_EN_MASK                        0x1
#define ACCDET_EINT0_PWM_EN_MASK_SFT                    (0x1 << 3)
#define ACCDET_EINT1_PWM_EN_ADDR                        \
	MT6358_ACCDET_CON2
#define ACCDET_EINT1_PWM_EN_SFT                         4
#define ACCDET_EINT1_PWM_EN_MASK                        0x1
#define ACCDET_EINT1_PWM_EN_MASK_SFT                    (0x1 << 4)
#define ACCDET_CMP_PWM_IDLE_ADDR                        \
	MT6358_ACCDET_CON2
#define ACCDET_CMP_PWM_IDLE_SFT                         8
#define ACCDET_CMP_PWM_IDLE_MASK                        0x1
#define ACCDET_CMP_PWM_IDLE_MASK_SFT                    (0x1 << 8)
#define ACCDET_VTH_PWM_IDLE_ADDR                        \
	MT6358_ACCDET_CON2
#define ACCDET_VTH_PWM_IDLE_SFT                         9
#define ACCDET_VTH_PWM_IDLE_MASK                        0x1
#define ACCDET_VTH_PWM_IDLE_MASK_SFT                    (0x1 << 9)
#define ACCDET_MBIAS_PWM_IDLE_ADDR                      \
	MT6358_ACCDET_CON2
#define ACCDET_MBIAS_PWM_IDLE_SFT                       10
#define ACCDET_MBIAS_PWM_IDLE_MASK                      0x1
#define ACCDET_MBIAS_PWM_IDLE_MASK_SFT                  (0x1 << 10)
#define ACCDET_EINT0_PWM_IDLE_ADDR                      \
	MT6358_ACCDET_CON2
#define ACCDET_EINT0_PWM_IDLE_SFT                       11
#define ACCDET_EINT0_PWM_IDLE_MASK                      0x1
#define ACCDET_EINT0_PWM_IDLE_MASK_SFT                  (0x1 << 11)
#define ACCDET_EINT1_PWM_IDLE_ADDR                      \
	MT6358_ACCDET_CON2
#define ACCDET_EINT1_PWM_IDLE_SFT                       12
#define ACCDET_EINT1_PWM_IDLE_MASK                      0x1
#define ACCDET_EINT1_PWM_IDLE_MASK_SFT                  (0x1 << 12)
#define ACCDET_PWM_WIDTH_ADDR                           \
	MT6358_ACCDET_CON3
#define ACCDET_PWM_WIDTH_SFT                            0
#define ACCDET_PWM_WIDTH_MASK                           0xFFFF
#define ACCDET_PWM_WIDTH_MASK_SFT                       (0xFFFF << 0)
#define ACCDET_PWM_THRESH_ADDR                          \
	MT6358_ACCDET_CON4
#define ACCDET_PWM_THRESH_SFT                           0
#define ACCDET_PWM_THRESH_MASK                          0xFFFF
#define ACCDET_PWM_THRESH_MASK_SFT                      (0xFFFF << 0)
#define ACCDET_RISE_DELAY_ADDR                          \
	MT6358_ACCDET_CON5
#define ACCDET_RISE_DELAY_SFT                           0
#define ACCDET_RISE_DELAY_MASK                          0x7FFF
#define ACCDET_RISE_DELAY_MASK_SFT                      (0x7FFF << 0)
#define ACCDET_FALL_DELAY_ADDR                          \
	MT6358_ACCDET_CON5
#define ACCDET_FALL_DELAY_SFT                           15
#define ACCDET_FALL_DELAY_MASK                          0x1
#define ACCDET_FALL_DELAY_MASK_SFT                      (0x1 << 15)
#define ACCDET_DEBOUNCE0_ADDR                           \
	MT6358_ACCDET_CON6
#define ACCDET_DEBOUNCE0_SFT                            0
#define ACCDET_DEBOUNCE0_MASK                           0xFFFF
#define ACCDET_DEBOUNCE0_MASK_SFT                       (0xFFFF << 0)
#define ACCDET_DEBOUNCE1_ADDR                           \
	MT6358_ACCDET_CON7
#define ACCDET_DEBOUNCE1_SFT                            0
#define ACCDET_DEBOUNCE1_MASK                           0xFFFF
#define ACCDET_DEBOUNCE1_MASK_SFT                       (0xFFFF << 0)
#define ACCDET_DEBOUNCE2_ADDR                           \
	MT6358_ACCDET_CON8
#define ACCDET_DEBOUNCE2_SFT                            0
#define ACCDET_DEBOUNCE2_MASK                           0xFFFF
#define ACCDET_DEBOUNCE2_MASK_SFT                       (0xFFFF << 0)
#define ACCDET_DEBOUNCE3_ADDR                           \
	MT6358_ACCDET_CON9
#define ACCDET_DEBOUNCE3_SFT                            0
#define ACCDET_DEBOUNCE3_MASK                           0xFFFF
#define ACCDET_DEBOUNCE3_MASK_SFT                       (0xFFFF << 0)
#define ACCDET_DEBOUNCE4_ADDR                           \
	MT6358_ACCDET_CON10
#define ACCDET_DEBOUNCE4_SFT                            0
#define ACCDET_DEBOUNCE4_MASK                           0xFFFF
#define ACCDET_DEBOUNCE4_MASK_SFT                       (0xFFFF << 0)
#define ACCDET_IVAL_CUR_IN_ADDR                         \
	MT6358_ACCDET_CON11
#define ACCDET_IVAL_CUR_IN_SFT                          0
#define ACCDET_IVAL_CUR_IN_MASK                         0x3
#define ACCDET_IVAL_CUR_IN_MASK_SFT                     (0x3 << 0)
#define ACCDET_EINT0_IVAL_CUR_IN_ADDR                   \
	MT6358_ACCDET_CON11
#define ACCDET_EINT0_IVAL_CUR_IN_SFT                    2
#define ACCDET_EINT0_IVAL_CUR_IN_MASK                   0x1
#define ACCDET_EINT0_IVAL_CUR_IN_MASK_SFT               (0x1 << 2)
#define ACCDET_EINT1_IVAL_CUR_IN_ADDR                   \
	MT6358_ACCDET_CON11
#define ACCDET_EINT1_IVAL_CUR_IN_SFT                    3
#define ACCDET_EINT1_IVAL_CUR_IN_MASK                   0x1
#define ACCDET_EINT1_IVAL_CUR_IN_MASK_SFT               (0x1 << 3)
#define ACCDET_IVAL_SAM_IN_ADDR                         \
	MT6358_ACCDET_CON11
#define ACCDET_IVAL_SAM_IN_SFT                          4
#define ACCDET_IVAL_SAM_IN_MASK                         0x3
#define ACCDET_IVAL_SAM_IN_MASK_SFT                     (0x3 << 4)
#define ACCDET_EINT0_IVAL_SAM_IN_ADDR                   \
	MT6358_ACCDET_CON11
#define ACCDET_EINT0_IVAL_SAM_IN_SFT                    6
#define ACCDET_EINT0_IVAL_SAM_IN_MASK                   0x1
#define ACCDET_EINT0_IVAL_SAM_IN_MASK_SFT               (0x1 << 6)
#define ACCDET_EINT1_IVAL_SAM_IN_ADDR                   \
	MT6358_ACCDET_CON11
#define ACCDET_EINT1_IVAL_SAM_IN_SFT                    7
#define ACCDET_EINT1_IVAL_SAM_IN_MASK                   0x1
#define ACCDET_EINT1_IVAL_SAM_IN_MASK_SFT               (0x1 << 7)
#define ACCDET_IVAL_MEM_IN_ADDR                         \
	MT6358_ACCDET_CON11
#define ACCDET_IVAL_MEM_IN_SFT                          8
#define ACCDET_IVAL_MEM_IN_MASK                         0x3
#define ACCDET_IVAL_MEM_IN_MASK_SFT                     (0x3 << 8)
#define ACCDET_EINT0_IVAL_MEM_IN_ADDR                   \
	MT6358_ACCDET_CON11
#define ACCDET_EINT0_IVAL_MEM_IN_SFT                    10
#define ACCDET_EINT0_IVAL_MEM_IN_MASK                   0x1
#define ACCDET_EINT0_IVAL_MEM_IN_MASK_SFT               (0x1 << 10)
#define ACCDET_EINT1_IVAL_MEM_IN_ADDR                   \
	MT6358_ACCDET_CON11
#define ACCDET_EINT1_IVAL_MEM_IN_SFT                    11
#define ACCDET_EINT1_IVAL_MEM_IN_MASK                   0x1
#define ACCDET_EINT1_IVAL_MEM_IN_MASK_SFT               (0x1 << 11)
#define ACCDET_IVAL_SEL_ADDR                            \
	MT6358_ACCDET_CON11
#define ACCDET_IVAL_SEL_SFT                             13
#define ACCDET_IVAL_SEL_MASK                            0x1
#define ACCDET_IVAL_SEL_MASK_SFT                        (0x1 << 13)
#define ACCDET_EINT0_IVAL_SEL_ADDR                      \
	MT6358_ACCDET_CON11
#define ACCDET_EINT0_IVAL_SEL_SFT                       14
#define ACCDET_EINT0_IVAL_SEL_MASK                      0x1
#define ACCDET_EINT0_IVAL_SEL_MASK_SFT                  (0x1 << 14)
#define ACCDET_EINT1_IVAL_SEL_ADDR                      \
	MT6358_ACCDET_CON11
#define ACCDET_EINT1_IVAL_SEL_SFT                       15
#define ACCDET_EINT1_IVAL_SEL_MASK                      0x1
#define ACCDET_EINT1_IVAL_SEL_MASK_SFT                  (0x1 << 15)
#define ACCDET_IRQ_ADDR                                 \
	MT6358_ACCDET_CON12
#define ACCDET_IRQ_SFT                                  0
#define ACCDET_IRQ_MASK                                 0x1
#define ACCDET_IRQ_MASK_SFT                             (0x1 << 0)
#define ACCDET_EINT0_IRQ_ADDR                           \
	MT6358_ACCDET_CON12
#define ACCDET_EINT0_IRQ_SFT                            2
#define ACCDET_EINT0_IRQ_MASK                           0x1
#define ACCDET_EINT0_IRQ_MASK_SFT                       (0x1 << 2)
#define ACCDET_EINT1_IRQ_ADDR                           \
	MT6358_ACCDET_CON12
#define ACCDET_EINT1_IRQ_SFT                            3
#define ACCDET_EINT1_IRQ_MASK                           0x1
#define ACCDET_EINT1_IRQ_MASK_SFT                       (0x1 << 3)
#define ACCDET_IRQ_CLR_ADDR                             \
	MT6358_ACCDET_CON12
#define ACCDET_IRQ_CLR_SFT                              8
#define ACCDET_IRQ_CLR_MASK                             0x1
#define ACCDET_IRQ_CLR_MASK_SFT                         (0x1 << 8)
#define ACCDET_EINT0_IRQ_CLR_ADDR                       \
	MT6358_ACCDET_CON12
#define ACCDET_EINT0_IRQ_CLR_SFT                        10
#define ACCDET_EINT0_IRQ_CLR_MASK                       0x1
#define ACCDET_EINT0_IRQ_CLR_MASK_SFT                   (0x1 << 10)
#define ACCDET_EINT1_IRQ_CLR_ADDR                       \
	MT6358_ACCDET_CON12
#define ACCDET_EINT1_IRQ_CLR_SFT                        11
#define ACCDET_EINT1_IRQ_CLR_MASK                       0x1
#define ACCDET_EINT1_IRQ_CLR_MASK_SFT                   (0x1 << 11)
#define ACCDET_EINT0_IRQ_POLARITY_ADDR                  \
	MT6358_ACCDET_CON12
#define ACCDET_EINT0_IRQ_POLARITY_SFT                   14
#define ACCDET_EINT0_IRQ_POLARITY_MASK                  0x1
#define ACCDET_EINT0_IRQ_POLARITY_MASK_SFT              (0x1 << 14)
#define ACCDET_EINT1_IRQ_POLARITY_ADDR                  \
	MT6358_ACCDET_CON12
#define ACCDET_EINT1_IRQ_POLARITY_SFT                   15
#define ACCDET_EINT1_IRQ_POLARITY_MASK                  0x1
#define ACCDET_EINT1_IRQ_POLARITY_MASK_SFT              (0x1 << 15)
#define ACCDET_TEST_MODE0_ADDR                          \
	MT6358_ACCDET_CON13
#define ACCDET_TEST_MODE0_SFT                           0
#define ACCDET_TEST_MODE0_MASK                          0x1
#define ACCDET_TEST_MODE0_MASK_SFT                      (0x1 << 0)
#define ACCDET_CMP_SWSEL_ADDR                           \
	MT6358_ACCDET_CON13
#define ACCDET_CMP_SWSEL_SFT                            1
#define ACCDET_CMP_SWSEL_MASK                           0x1
#define ACCDET_CMP_SWSEL_MASK_SFT                       (0x1 << 1)
#define ACCDET_VTH_SWSEL_ADDR                           \
	MT6358_ACCDET_CON13
#define ACCDET_VTH_SWSEL_SFT                            2
#define ACCDET_VTH_SWSEL_MASK                           0x1
#define ACCDET_VTH_SWSEL_MASK_SFT                       (0x1 << 2)
#define ACCDET_MBIAS_SWSEL_ADDR                         \
	MT6358_ACCDET_CON13
#define ACCDET_MBIAS_SWSEL_SFT                          3
#define ACCDET_MBIAS_SWSEL_MASK                         0x1
#define ACCDET_MBIAS_SWSEL_MASK_SFT                     (0x1 << 3)
#define ACCDET_TEST_MODE4_ADDR                          \
	MT6358_ACCDET_CON13
#define ACCDET_TEST_MODE4_SFT                           4
#define ACCDET_TEST_MODE4_MASK                          0x1
#define ACCDET_TEST_MODE4_MASK_SFT                      (0x1 << 4)
#define ACCDET_TEST_MODE5_ADDR                          \
	MT6358_ACCDET_CON13
#define ACCDET_TEST_MODE5_SFT                           5
#define ACCDET_TEST_MODE5_MASK                          0x1
#define ACCDET_TEST_MODE5_MASK_SFT                      (0x1 << 5)
#define ACCDET_PWM_SEL_ADDR                             \
	MT6358_ACCDET_CON13
#define ACCDET_PWM_SEL_SFT                              6
#define ACCDET_PWM_SEL_MASK                             0x3
#define ACCDET_PWM_SEL_MASK_SFT                         (0x3 << 6)
#define ACCDET_IN_SW_ADDR                               \
	MT6358_ACCDET_CON13
#define ACCDET_IN_SW_SFT                                8
#define ACCDET_IN_SW_MASK                               0x3
#define ACCDET_IN_SW_MASK_SFT                           (0x3 << 8)
#define ACCDET_CMP_EN_SW_ADDR                           \
	MT6358_ACCDET_CON13
#define ACCDET_CMP_EN_SW_SFT                            12
#define ACCDET_CMP_EN_SW_MASK                           0x1
#define ACCDET_CMP_EN_SW_MASK_SFT                       (0x1 << 12)
#define ACCDET_VTH_EN_SW_ADDR                           \
	MT6358_ACCDET_CON13
#define ACCDET_VTH_EN_SW_SFT                            13
#define ACCDET_VTH_EN_SW_MASK                           0x1
#define ACCDET_VTH_EN_SW_MASK_SFT                       (0x1 << 13)
#define ACCDET_MBIAS_EN_SW_ADDR                         \
	MT6358_ACCDET_CON13
#define ACCDET_MBIAS_EN_SW_SFT                          14
#define ACCDET_MBIAS_EN_SW_MASK                         0x1
#define ACCDET_MBIAS_EN_SW_MASK_SFT                     (0x1 << 14)
#define ACCDET_PWM_EN_SW_ADDR                           \
	MT6358_ACCDET_CON13
#define ACCDET_PWM_EN_SW_SFT                            15
#define ACCDET_PWM_EN_SW_MASK                           0x1
#define ACCDET_PWM_EN_SW_MASK_SFT                       (0x1 << 15)
#define ACCDET_IN_ADDR                                  \
	MT6358_ACCDET_CON14
#define ACCDET_IN_SFT                                   0
#define ACCDET_IN_MASK                                  0x3
#define ACCDET_IN_MASK_SFT                              (0x3 << 0)
#define ACCDET_CUR_IN_ADDR                              \
	MT6358_ACCDET_CON14
#define ACCDET_CUR_IN_SFT                               2
#define ACCDET_CUR_IN_MASK                              0x3
#define ACCDET_CUR_IN_MASK_SFT                          (0x3 << 2)
#define ACCDET_SAM_IN_ADDR                              \
	MT6358_ACCDET_CON14
#define ACCDET_SAM_IN_SFT                               4
#define ACCDET_SAM_IN_MASK                              0x3
#define ACCDET_SAM_IN_MASK_SFT                          (0x3 << 4)
#define ACCDET_MEM_IN_ADDR                              \
	MT6358_ACCDET_CON14
#define ACCDET_MEM_IN_SFT                               6
#define ACCDET_MEM_IN_MASK                              0x3
#define ACCDET_MEM_IN_MASK_SFT                          (0x3 << 6)
#define ACCDET_STATE_ADDR                               \
	MT6358_ACCDET_CON14
#define ACCDET_STATE_SFT                                8
#define ACCDET_STATE_MASK                               0x7
#define ACCDET_STATE_MASK_SFT                           (0x7 << 8)
#define ACCDET_MBIAS_CLK_ADDR                           \
	MT6358_ACCDET_CON14
#define ACCDET_MBIAS_CLK_SFT                            12
#define ACCDET_MBIAS_CLK_MASK                           0x1
#define ACCDET_MBIAS_CLK_MASK_SFT                       (0x1 << 12)
#define ACCDET_VTH_CLK_ADDR                             \
	MT6358_ACCDET_CON14
#define ACCDET_VTH_CLK_SFT                              13
#define ACCDET_VTH_CLK_MASK                             0x1
#define ACCDET_VTH_CLK_MASK_SFT                         (0x1 << 13)
#define ACCDET_CMP_CLK_ADDR                             \
	MT6358_ACCDET_CON14
#define ACCDET_CMP_CLK_SFT                              14
#define ACCDET_CMP_CLK_MASK                             0x1
#define ACCDET_CMP_CLK_MASK_SFT                         (0x1 << 14)
#define DA_AUDACCDETAUXADCSWCTRL_ADDR                   \
	MT6358_ACCDET_CON14
#define DA_AUDACCDETAUXADCSWCTRL_SFT                    15
#define DA_AUDACCDETAUXADCSWCTRL_MASK                   0x1
#define DA_AUDACCDETAUXADCSWCTRL_MASK_SFT               (0x1 << 15)
#define ACCDET_EINT0_DEB_SEL_ADDR                       \
	MT6358_ACCDET_CON15
#define ACCDET_EINT0_DEB_SEL_SFT                        0
#define ACCDET_EINT0_DEB_SEL_MASK                       0x1
#define ACCDET_EINT0_DEB_SEL_MASK_SFT                   (0x1 << 0)
#define ACCDET_EINT0_DEBOUNCE_ADDR                      \
	MT6358_ACCDET_CON15
#define ACCDET_EINT0_DEBOUNCE_SFT                       3
#define ACCDET_EINT0_DEBOUNCE_MASK                      0xF
#define ACCDET_EINT0_DEBOUNCE_MASK_SFT                  (0xF << 3)
#define ACCDET_EINT0_PWM_THRESH_ADDR                    \
	MT6358_ACCDET_CON15
#define ACCDET_EINT0_PWM_THRESH_SFT                     8
#define ACCDET_EINT0_PWM_THRESH_MASK                    0x7
#define ACCDET_EINT0_PWM_THRESH_MASK_SFT                (0x7 << 8)
#define ACCDET_EINT0_PWM_WIDTH_ADDR                     \
	MT6358_ACCDET_CON15
#define ACCDET_EINT0_PWM_WIDTH_SFT                      12
#define ACCDET_EINT0_PWM_WIDTH_MASK                     0x3
#define ACCDET_EINT0_PWM_WIDTH_MASK_SFT                 (0x3 << 12)
#define ACCDET_EINT0_PWM_FALL_DELAY_ADDR                \
	MT6358_ACCDET_CON16
#define ACCDET_EINT0_PWM_FALL_DELAY_SFT                 5
#define ACCDET_EINT0_PWM_FALL_DELAY_MASK                0x1
#define ACCDET_EINT0_PWM_FALL_DELAY_MASK_SFT            (0x1 << 5)
#define ACCDET_EINT0_PWM_RISE_DELAY_ADDR                \
	MT6358_ACCDET_CON16
#define ACCDET_EINT0_PWM_RISE_DELAY_SFT                 6
#define ACCDET_EINT0_PWM_RISE_DELAY_MASK                0x3FF
#define ACCDET_EINT0_PWM_RISE_DELAY_MASK_SFT            (0x3FF << 6)
#define ACCDET_TEST_MODE11_ADDR                         \
	MT6358_ACCDET_CON17
#define ACCDET_TEST_MODE11_SFT                          5
#define ACCDET_TEST_MODE11_MASK                         0x1
#define ACCDET_TEST_MODE11_MASK_SFT                     (0x1 << 5)
#define ACCDET_TEST_MODE10_ADDR                         \
	MT6358_ACCDET_CON17
#define ACCDET_TEST_MODE10_SFT                          6
#define ACCDET_TEST_MODE10_MASK                         0x1
#define ACCDET_TEST_MODE10_MASK_SFT                     (0x1 << 6)
#define ACCDET_EINT0_CMPOUT_SW_ADDR                     \
	MT6358_ACCDET_CON17
#define ACCDET_EINT0_CMPOUT_SW_SFT                      7
#define ACCDET_EINT0_CMPOUT_SW_MASK                     0x1
#define ACCDET_EINT0_CMPOUT_SW_MASK_SFT                 (0x1 << 7)
#define ACCDET_EINT1_CMPOUT_SW_ADDR                     \
	MT6358_ACCDET_CON17
#define ACCDET_EINT1_CMPOUT_SW_SFT                      8
#define ACCDET_EINT1_CMPOUT_SW_MASK                     0x1
#define ACCDET_EINT1_CMPOUT_SW_MASK_SFT                 (0x1 << 8)
#define ACCDET_TEST_MODE9_ADDR                          \
	MT6358_ACCDET_CON17
#define ACCDET_TEST_MODE9_SFT                           9
#define ACCDET_TEST_MODE9_MASK                          0x1
#define ACCDET_TEST_MODE9_MASK_SFT                      (0x1 << 9)
#define ACCDET_TEST_MODE8_ADDR                          \
	MT6358_ACCDET_CON17
#define ACCDET_TEST_MODE8_SFT                           10
#define ACCDET_TEST_MODE8_MASK                          0x1
#define ACCDET_TEST_MODE8_MASK_SFT                      (0x1 << 10)
#define ACCDET_AUXADC_CTRL_SW_ADDR                      \
	MT6358_ACCDET_CON17
#define ACCDET_AUXADC_CTRL_SW_SFT                       11
#define ACCDET_AUXADC_CTRL_SW_MASK                      0x1
#define ACCDET_AUXADC_CTRL_SW_MASK_SFT                  (0x1 << 11)
#define ACCDET_TEST_MODE7_ADDR                          \
	MT6358_ACCDET_CON17
#define ACCDET_TEST_MODE7_SFT                           12
#define ACCDET_TEST_MODE7_MASK                          0x1
#define ACCDET_TEST_MODE7_MASK_SFT                      (0x1 << 12)
#define ACCDET_TEST_MODE6_ADDR                          \
	MT6358_ACCDET_CON17
#define ACCDET_TEST_MODE6_SFT                           13
#define ACCDET_TEST_MODE6_MASK                          0x1
#define ACCDET_TEST_MODE6_MASK_SFT                      (0x1 << 13)
#define ACCDET_EINT0_CMP_EN_SW_ADDR                     \
	MT6358_ACCDET_CON17
#define ACCDET_EINT0_CMP_EN_SW_SFT                      14
#define ACCDET_EINT0_CMP_EN_SW_MASK                     0x1
#define ACCDET_EINT0_CMP_EN_SW_MASK_SFT                 (0x1 << 14)
#define ACCDET_EINT1_CMP_EN_SW_ADDR                     \
	MT6358_ACCDET_CON17
#define ACCDET_EINT1_CMP_EN_SW_SFT                      15
#define ACCDET_EINT1_CMP_EN_SW_MASK                     0x1
#define ACCDET_EINT1_CMP_EN_SW_MASK_SFT                 (0x1 << 15)
#define ACCDET_EINT0_STATE_ADDR                         \
	MT6358_ACCDET_CON18
#define ACCDET_EINT0_STATE_SFT                          0
#define ACCDET_EINT0_STATE_MASK                         0x7
#define ACCDET_EINT0_STATE_MASK_SFT                     (0x7 << 0)
#define ACCDET_AUXADC_DEBOUNCE_END_ADDR                 \
	MT6358_ACCDET_CON18
#define ACCDET_AUXADC_DEBOUNCE_END_SFT                  3
#define ACCDET_AUXADC_DEBOUNCE_END_MASK                 0x1
#define ACCDET_AUXADC_DEBOUNCE_END_MASK_SFT             (0x1 << 3)
#define ACCDET_AUXADC_CONNECT_PRE_ADDR                  \
	MT6358_ACCDET_CON18
#define ACCDET_AUXADC_CONNECT_PRE_SFT                   4
#define ACCDET_AUXADC_CONNECT_PRE_MASK                  0x1
#define ACCDET_AUXADC_CONNECT_PRE_MASK_SFT              (0x1 << 4)
#define ACCDET_EINT0_CUR_IN_ADDR                        \
	MT6358_ACCDET_CON18
#define ACCDET_EINT0_CUR_IN_SFT                         8
#define ACCDET_EINT0_CUR_IN_MASK                        0x1
#define ACCDET_EINT0_CUR_IN_MASK_SFT                    (0x1 << 8)
#define ACCDET_EINT0_SAM_IN_ADDR                        \
	MT6358_ACCDET_CON18
#define ACCDET_EINT0_SAM_IN_SFT                         9
#define ACCDET_EINT0_SAM_IN_MASK                        0x1
#define ACCDET_EINT0_SAM_IN_MASK_SFT                    (0x1 << 9)
#define ACCDET_EINT0_MEM_IN_ADDR                        \
	MT6358_ACCDET_CON18
#define ACCDET_EINT0_MEM_IN_SFT                         10
#define ACCDET_EINT0_MEM_IN_MASK                        0x1
#define ACCDET_EINT0_MEM_IN_MASK_SFT                    (0x1 << 10)
#define AD_EINT0CMPOUT_ADDR                             \
	MT6358_ACCDET_CON18
#define AD_EINT0CMPOUT_SFT                              14
#define AD_EINT0CMPOUT_MASK                             0x1
#define AD_EINT0CMPOUT_MASK_SFT                         (0x1 << 14)
#define DA_NI_EINT0CMPEN_ADDR                           \
	MT6358_ACCDET_CON18
#define DA_NI_EINT0CMPEN_SFT                            15
#define DA_NI_EINT0CMPEN_MASK                           0x1
#define DA_NI_EINT0CMPEN_MASK_SFT                       (0x1 << 15)
#define ACCDET_CUR_DEB_ADDR                             \
	MT6358_ACCDET_CON19
#define ACCDET_CUR_DEB_SFT                              0
#define ACCDET_CUR_DEB_MASK                             0xFFFF
#define ACCDET_CUR_DEB_MASK_SFT                         (0xFFFF << 0)
#define ACCDET_EINT0_CUR_DEB_ADDR                       \
	MT6358_ACCDET_CON20
#define ACCDET_EINT0_CUR_DEB_SFT                        0
#define ACCDET_EINT0_CUR_DEB_MASK                       0x7FFF
#define ACCDET_EINT0_CUR_DEB_MASK_SFT                   (0x7FFF << 0)
#define ACCDET_MON_FLAG_EN_ADDR                         \
	MT6358_ACCDET_CON21
#define ACCDET_MON_FLAG_EN_SFT                          0
#define ACCDET_MON_FLAG_EN_MASK                         0x1
#define ACCDET_MON_FLAG_EN_MASK_SFT                     (0x1 << 0)
#define ACCDET_MON_FLAG_SEL_ADDR                        \
	MT6358_ACCDET_CON21
#define ACCDET_MON_FLAG_SEL_SFT                         4
#define ACCDET_MON_FLAG_SEL_MASK                        0xFF
#define ACCDET_MON_FLAG_SEL_MASK_SFT                    (0xFF << 4)
#define ACCDET_RSV_CON1_ADDR                            \
	MT6358_ACCDET_CON22
#define ACCDET_RSV_CON1_SFT                             0
#define ACCDET_RSV_CON1_MASK                            0xFFFF
#define ACCDET_RSV_CON1_MASK_SFT                        (0xFFFF << 0)
#define ACCDET_AUXADC_CONNECT_TIME_ADDR                 \
	MT6358_ACCDET_CON23
#define ACCDET_AUXADC_CONNECT_TIME_SFT                  0
#define ACCDET_AUXADC_CONNECT_TIME_MASK                 0xFFFF
#define ACCDET_AUXADC_CONNECT_TIME_MASK_SFT             (0xFFFF << 0)
#define ACCDET_HWEN_SEL_ADDR                            \
	MT6358_ACCDET_CON24
#define ACCDET_HWEN_SEL_SFT                             0
#define ACCDET_HWEN_SEL_MASK                            0x3
#define ACCDET_HWEN_SEL_MASK_SFT                        (0x3 << 0)
#define ACCDET_HWMODE_SEL_ADDR                          \
	MT6358_ACCDET_CON24
#define ACCDET_HWMODE_SEL_SFT                           2
#define ACCDET_HWMODE_SEL_MASK                          0x1
#define ACCDET_HWMODE_SEL_MASK_SFT                      (0x1 << 2)
#define ACCDET_EINT_DEB_OUT_DFF_ADDR                    \
	MT6358_ACCDET_CON24
#define ACCDET_EINT_DEB_OUT_DFF_SFT                     3
#define ACCDET_EINT_DEB_OUT_DFF_MASK                    0x1
#define ACCDET_EINT_DEB_OUT_DFF_MASK_SFT                (0x1 << 3)
#define ACCDET_FAST_DISCHARGE_ADDR                      \
	MT6358_ACCDET_CON24
#define ACCDET_FAST_DISCHARGE_SFT                       4
#define ACCDET_FAST_DISCHARGE_MASK                      0x1
#define ACCDET_FAST_DISCHARGE_MASK_SFT                  (0x1 << 4)
#define ACCDET_EINT0_REVERSE_ADDR                       \
	MT6358_ACCDET_CON24
#define ACCDET_EINT0_REVERSE_SFT                        14
#define ACCDET_EINT0_REVERSE_MASK                       0x1
#define ACCDET_EINT0_REVERSE_MASK_SFT                   (0x1 << 14)
#define ACCDET_EINT1_REVERSE_ADDR                       \
	MT6358_ACCDET_CON24
#define ACCDET_EINT1_REVERSE_SFT                        15
#define ACCDET_EINT1_REVERSE_MASK                       0x1
#define ACCDET_EINT1_REVERSE_MASK_SFT                   (0x1 << 15)
#define ACCDET_EINT1_DEB_SEL_ADDR                       \
	MT6358_ACCDET_CON25
#define ACCDET_EINT1_DEB_SEL_SFT                        0
#define ACCDET_EINT1_DEB_SEL_MASK                       0x1
#define ACCDET_EINT1_DEB_SEL_MASK_SFT                   (0x1 << 0)
#define ACCDET_EINT1_DEBOUNCE_ADDR                      \
	MT6358_ACCDET_CON25
#define ACCDET_EINT1_DEBOUNCE_SFT                       3
#define ACCDET_EINT1_DEBOUNCE_MASK                      0xF
#define ACCDET_EINT1_DEBOUNCE_MASK_SFT                  (0xF << 3)
#define ACCDET_EINT1_PWM_THRESH_ADDR                    \
	MT6358_ACCDET_CON25
#define ACCDET_EINT1_PWM_THRESH_SFT                     8
#define ACCDET_EINT1_PWM_THRESH_MASK                    0x7
#define ACCDET_EINT1_PWM_THRESH_MASK_SFT                (0x7 << 8)
#define ACCDET_EINT1_PWM_WIDTH_ADDR                     \
	MT6358_ACCDET_CON25
#define ACCDET_EINT1_PWM_WIDTH_SFT                      12
#define ACCDET_EINT1_PWM_WIDTH_MASK                     0x3
#define ACCDET_EINT1_PWM_WIDTH_MASK_SFT                 (0x3 << 12)
#define ACCDET_EINT1_PWM_FALL_DELAY_ADDR                \
	MT6358_ACCDET_CON26
#define ACCDET_EINT1_PWM_FALL_DELAY_SFT                 5
#define ACCDET_EINT1_PWM_FALL_DELAY_MASK                0x1
#define ACCDET_EINT1_PWM_FALL_DELAY_MASK_SFT            (0x1 << 5)
#define ACCDET_EINT1_PWM_RISE_DELAY_ADDR                \
	MT6358_ACCDET_CON26
#define ACCDET_EINT1_PWM_RISE_DELAY_SFT                 6
#define ACCDET_EINT1_PWM_RISE_DELAY_MASK                0x3FF
#define ACCDET_EINT1_PWM_RISE_DELAY_MASK_SFT            (0x3FF << 6)
#define ACCDET_EINT1_STATE_ADDR                         \
	MT6358_ACCDET_CON27
#define ACCDET_EINT1_STATE_SFT                          0
#define ACCDET_EINT1_STATE_MASK                         0x7
#define ACCDET_EINT1_STATE_MASK_SFT                     (0x7 << 0)
#define ACCDET_EINT1_CUR_IN_ADDR                        \
	MT6358_ACCDET_CON27
#define ACCDET_EINT1_CUR_IN_SFT                         8
#define ACCDET_EINT1_CUR_IN_MASK                        0x1
#define ACCDET_EINT1_CUR_IN_MASK_SFT                    (0x1 << 8)
#define ACCDET_EINT1_SAM_IN_ADDR                        \
	MT6358_ACCDET_CON27
#define ACCDET_EINT1_SAM_IN_SFT                         9
#define ACCDET_EINT1_SAM_IN_MASK                        0x1
#define ACCDET_EINT1_SAM_IN_MASK_SFT                    (0x1 << 9)
#define ACCDET_EINT1_MEM_IN_ADDR                        \
	MT6358_ACCDET_CON27
#define ACCDET_EINT1_MEM_IN_SFT                         10
#define ACCDET_EINT1_MEM_IN_MASK                        0x1
#define ACCDET_EINT1_MEM_IN_MASK_SFT                    (0x1 << 10)
#define AD_EINT1CMPOUT_ADDR                             \
	MT6358_ACCDET_CON27
#define AD_EINT1CMPOUT_SFT                              14
#define AD_EINT1CMPOUT_MASK                             0x1
#define AD_EINT1CMPOUT_MASK_SFT                         (0x1 << 14)
#define DA_NI_EINT1CMPEN_ADDR                           \
	MT6358_ACCDET_CON27
#define DA_NI_EINT1CMPEN_SFT                            15
#define DA_NI_EINT1CMPEN_MASK                           0x1
#define DA_NI_EINT1CMPEN_MASK_SFT                       (0x1 << 15)
#define ACCDET_EINT1_CUR_DEB_ADDR                       \
	MT6358_ACCDET_CON28
#define ACCDET_EINT1_CUR_DEB_SFT                        0
#define ACCDET_EINT1_CUR_DEB_MASK                       0x7FFF
#define ACCDET_EINT1_CUR_DEB_MASK_SFT                   (0x7FFF << 0)

#define RG_RTC32K_CK_PDN_ADDR                           \
	MT6358_TOP_CKPDN_CON0
#define RG_RTC32K_CK_PDN_SFT                            15
#define RG_RTC32K_CK_PDN_MASK                           0x1
#define RG_RTC32K_CK_PDN_MASK_SFT                       (0x1 << 15)
#define AUXADC_RQST_CH5_ADDR                            \
	MT6358_AUXADC_RQST0
#define AUXADC_RQST_CH5_SFT                             5
#define AUXADC_RQST_CH5_MASK                            0x1
#define AUXADC_RQST_CH5_MASK_SFT                        (0x1 << 5)
#define ACCDET_EINT0_IRQ_POLARITY_ADDR                  \
	MT6358_ACCDET_CON12
#define ACCDET_EINT0_IRQ_POLARITY_SFT                   14
#define ACCDET_EINT0_IRQ_POLARITY_MASK                  0x1
#define ACCDET_EINT0_IRQ_POLARITY_MASK_SFT              (0x1 << 14)
#define ACCDET_EINT1_IRQ_POLARITY_ADDR                  \
	MT6358_ACCDET_CON12
#define ACCDET_EINT1_IRQ_POLARITY_SFT                   15
#define ACCDET_EINT1_IRQ_POLARITY_MASK                  0x1
#define ACCDET_EINT1_IRQ_POLARITY_MASK_SFT              (0x1 << 15)

#define ACCDET_HWMODE_SEL_BIT		BIT(2)
#define ACCDET_FAST_DISCAHRGE		BIT(4)

/* AUDENC_ANA_CON6:  analog fast discharge*/
#define RG_AUDSPARE				(0x00A0)
#define RG_AUDSPARE_FSTDSCHRG_ANALOG_DIR_EN	BIT(5)
#define RG_AUDSPARE_FSTDSCHRG_IMPR_EN		BIT(6)

/* 0ms */
#define ACCDET_EINT1_DEB_BYPASS		(0x00<<3)
/* 0.12ms */
#define ACCDET_EINT1_DEB_OUT_012	(0x01<<3)
/* 32ms */
#define ACCDET_EINT1_DEB_IN_32		(0x0A<<3)
/* 64ms */
#define ACCDET_EINT1_DEB_IN_64		(0x0C<<3)
/* 256ms */
#define ACCDET_EINT1_DEB_IN_256		(0x0E<<3)
/* 512ms */
#define ACCDET_EINT1_DEB_512		(0x0F<<3)

/* ACCDET_CON15: accdet eint0 debounce, PWM width&thresh, etc.
 * bit0: ACCDET_EINT0_DEB_SEL, 1,debounce_multi_sync_path;0,from register
 */
#define ACCDET_EINT0_DEB_SEL		(0x01<<0)
/* 0ms */
#define ACCDET_EINT0_DEB_BYPASS		(0x00<<3)
/* 0.12ms */
#define ACCDET_EINT0_DEB_OUT_012	(0x01)
/* 32ms */
#define ACCDET_EINT0_DEB_IN_32		(0x0A)
/* 64ms */
#define ACCDET_EINT0_DEB_IN_64		(0x0C)
/* 256ms */
#define ACCDET_EINT0_DEB_IN_256		(0x0E)
/* 512ms */
#define ACCDET_EINT0_DEB_512		(0x0F)
#define ACCDET_EINT0_DEB_CLR		(0x0F)

/* AUDENC_ANA_CON10: */
#define RG_ACCDET_MODE_ANA11_MODE1	(0x0807)
#define RG_ACCDET_MODE_ANA11_MODE2	(0x0887)
#define RG_ACCDET_MODE_ANA11_MODE6	(0x0887)

#define ACCDET_CALI_MASK0		(0xFF)
#define ACCDET_CALI_MASK1		(0xFF<<8)
#define ACCDET_CALI_MASK2		(0xFF)
#define ACCDET_CALI_MASK3		(0xFF<<8)
#define ACCDET_CALI_MASK4		(0xFF)

#define ACCDET_EINT_IRQ_B2_B3		(0x03<<ACCDET_EINT0_IRQ_SFT)

/* ACCDET_CON25: RO, accdet FSM state,etc.*/
#define ACCDET_STATE_MEM_IN_OFFSET	(ACCDET_MEM_IN_SFT)
#define ACCDET_STATE_AB_MASK		(0x03)
#define ACCDET_STATE_AB_00		(0x00)
#define ACCDET_STATE_AB_01		(0x01)
#define ACCDET_STATE_AB_10		(0x02)
#define ACCDET_STATE_AB_11		(0x03)

#define ACCDET_EINT0_IVAL_B2_6_10	(0x0444)


/* The following are used for mt6359.c */

/* Reg bit define */
/* MT6358_DCXO_CW14 */
#define RG_XO_AUDIO_EN_M_SFT 13

/* MT6358_DCXO_CW13 */
#define RG_XO_VOW_EN_SFT 8

/* MT6358_AUD_TOP_CKPDN_CON0 */
#define RG_VOW13M_CK_PDN_SFT                              13
#define RG_VOW13M_CK_PDN_MASK                             0x1
#define RG_VOW13M_CK_PDN_MASK_SFT                         (0x1 << 13)
#define RG_VOW32K_CK_PDN_SFT                              12
#define RG_VOW32K_CK_PDN_MASK                             0x1
#define RG_VOW32K_CK_PDN_MASK_SFT                         (0x1 << 12)
#define RG_AUD_INTRP_CK_PDN_SFT                           8
#define RG_AUD_INTRP_CK_PDN_MASK                          0x1
#define RG_AUD_INTRP_CK_PDN_MASK_SFT                      (0x1 << 8)
#define RG_PAD_AUD_CLK_MISO_CK_PDN_SFT                    7
#define RG_PAD_AUD_CLK_MISO_CK_PDN_MASK                   0x1
#define RG_PAD_AUD_CLK_MISO_CK_PDN_MASK_SFT               (0x1 << 7)
#define RG_AUDNCP_CK_PDN_SFT                              6
#define RG_AUDNCP_CK_PDN_MASK                             0x1
#define RG_AUDNCP_CK_PDN_MASK_SFT                         (0x1 << 6)
#define RG_ZCD13M_CK_PDN_SFT                              5
#define RG_ZCD13M_CK_PDN_MASK                             0x1
#define RG_ZCD13M_CK_PDN_MASK_SFT                         (0x1 << 5)
#define RG_AUDIF_CK_PDN_SFT                               2
#define RG_AUDIF_CK_PDN_MASK                              0x1
#define RG_AUDIF_CK_PDN_MASK_SFT                          (0x1 << 2)
#define RG_AUD_CK_PDN_SFT                                 1
#define RG_AUD_CK_PDN_MASK                                0x1
#define RG_AUD_CK_PDN_MASK_SFT                            (0x1 << 1)
#define RG_ACCDET_CK_PDN_SFT                              0
#define RG_ACCDET_CK_PDN_MASK                             0x1
#define RG_ACCDET_CK_PDN_MASK_SFT                         (0x1 << 0)

/* MT6358_AUD_TOP_CKPDN_CON0_SET */
#define RG_AUD_TOP_CKPDN_CON0_SET_SFT                     0
#define RG_AUD_TOP_CKPDN_CON0_SET_MASK                    0x3fff
#define RG_AUD_TOP_CKPDN_CON0_SET_MASK_SFT                (0x3fff << 0)

/* MT6358_AUD_TOP_CKPDN_CON0_CLR */
#define RG_AUD_TOP_CKPDN_CON0_CLR_SFT                     0
#define RG_AUD_TOP_CKPDN_CON0_CLR_MASK                    0x3fff
#define RG_AUD_TOP_CKPDN_CON0_CLR_MASK_SFT                (0x3fff << 0)

/* MT6358_AUD_TOP_CKSEL_CON0 */
#define RG_AUDIF_CK_CKSEL_SFT                             3
#define RG_AUDIF_CK_CKSEL_MASK                            0x1
#define RG_AUDIF_CK_CKSEL_MASK_SFT                        (0x1 << 3)
#define RG_AUD_CK_CKSEL_SFT                               2
#define RG_AUD_CK_CKSEL_MASK                              0x1
#define RG_AUD_CK_CKSEL_MASK_SFT                          (0x1 << 2)

/* MT6358_AUD_TOP_CKSEL_CON0_SET */
#define RG_AUD_TOP_CKSEL_CON0_SET_SFT                     0
#define RG_AUD_TOP_CKSEL_CON0_SET_MASK                    0xf
#define RG_AUD_TOP_CKSEL_CON0_SET_MASK_SFT                (0xf << 0)

/* MT6358_AUD_TOP_CKSEL_CON0_CLR */
#define RG_AUD_TOP_CKSEL_CON0_CLR_SFT                     0
#define RG_AUD_TOP_CKSEL_CON0_CLR_MASK                    0xf
#define RG_AUD_TOP_CKSEL_CON0_CLR_MASK_SFT                (0xf << 0)

/* MT6358_AUD_TOP_CKTST_CON0 */
#define RG_VOW13M_CK_TSTSEL_SFT                           9
#define RG_VOW13M_CK_TSTSEL_MASK                          0x1
#define RG_VOW13M_CK_TSTSEL_MASK_SFT                      (0x1 << 9)
#define RG_VOW13M_CK_TST_DIS_SFT                          8
#define RG_VOW13M_CK_TST_DIS_MASK                         0x1
#define RG_VOW13M_CK_TST_DIS_MASK_SFT                     (0x1 << 8)
#define RG_AUD26M_CK_TSTSEL_SFT                           4
#define RG_AUD26M_CK_TSTSEL_MASK                          0x1
#define RG_AUD26M_CK_TSTSEL_MASK_SFT                      (0x1 << 4)
#define RG_AUDIF_CK_TSTSEL_SFT                            3
#define RG_AUDIF_CK_TSTSEL_MASK                           0x1
#define RG_AUDIF_CK_TSTSEL_MASK_SFT                       (0x1 << 3)
#define RG_AUD_CK_TSTSEL_SFT                              2
#define RG_AUD_CK_TSTSEL_MASK                             0x1
#define RG_AUD_CK_TSTSEL_MASK_SFT                         (0x1 << 2)
#define RG_AUD26M_CK_TST_DIS_SFT                          0
#define RG_AUD26M_CK_TST_DIS_MASK                         0x1
#define RG_AUD26M_CK_TST_DIS_MASK_SFT                     (0x1 << 0)

/* MT6358_AUD_TOP_CLK_HWEN_CON0 */
#define RG_AUD_INTRP_CK_PDN_HWEN_SFT                      0
#define RG_AUD_INTRP_CK_PDN_HWEN_MASK                     0x1
#define RG_AUD_INTRP_CK_PDN_HWEN_MASK_SFT                 (0x1 << 0)

/* MT6358_AUD_TOP_CLK_HWEN_CON0_SET */
#define RG_AUD_INTRP_CK_PND_HWEN_CON0_SET_SFT             0
#define RG_AUD_INTRP_CK_PND_HWEN_CON0_SET_MASK            0xffff
#define RG_AUD_INTRP_CK_PND_HWEN_CON0_SET_MASK_SFT        (0xffff << 0)

/* MT6358_AUD_TOP_CLK_HWEN_CON0_CLR */
#define RG_AUD_INTRP_CLK_PDN_HWEN_CON0_CLR_SFT            0
#define RG_AUD_INTRP_CLK_PDN_HWEN_CON0_CLR_MASK           0xffff
#define RG_AUD_INTRP_CLK_PDN_HWEN_CON0_CLR_MASK_SFT       (0xffff << 0)

/* MT6358_AUD_TOP_RST_CON0 */
#define RG_AUDNCP_RST_SFT                                 3
#define RG_AUDNCP_RST_MASK                                0x1
#define RG_AUDNCP_RST_MASK_SFT                            (0x1 << 3)
#define RG_ZCD_RST_SFT                                    2
#define RG_ZCD_RST_MASK                                   0x1
#define RG_ZCD_RST_MASK_SFT                               (0x1 << 2)
#define RG_ACCDET_RST_SFT                                 1
#define RG_ACCDET_RST_MASK                                0x1
#define RG_ACCDET_RST_MASK_SFT                            (0x1 << 1)
#define RG_AUDIO_RST_SFT                                  0
#define RG_AUDIO_RST_MASK                                 0x1
#define RG_AUDIO_RST_MASK_SFT                             (0x1 << 0)

/* MT6358_AUD_TOP_RST_CON0_SET */
#define RG_AUD_TOP_RST_CON0_SET_SFT                       0
#define RG_AUD_TOP_RST_CON0_SET_MASK                      0xf
#define RG_AUD_TOP_RST_CON0_SET_MASK_SFT                  (0xf << 0)

/* MT6358_AUD_TOP_RST_CON0_CLR */
#define RG_AUD_TOP_RST_CON0_CLR_SFT                       0
#define RG_AUD_TOP_RST_CON0_CLR_MASK                      0xf
#define RG_AUD_TOP_RST_CON0_CLR_MASK_SFT                  (0xf << 0)

/* MT6358_AUD_TOP_RST_BANK_CON0 */
#define BANK_AUDZCD_SWRST_SFT                             2
#define BANK_AUDZCD_SWRST_MASK                            0x1
#define BANK_AUDZCD_SWRST_MASK_SFT                        (0x1 << 2)
#define BANK_AUDIO_SWRST_SFT                              1
#define BANK_AUDIO_SWRST_MASK                             0x1
#define BANK_AUDIO_SWRST_MASK_SFT                         (0x1 << 1)
#define BANK_ACCDET_SWRST_SFT                             0
#define BANK_ACCDET_SWRST_MASK                            0x1
#define BANK_ACCDET_SWRST_MASK_SFT                        (0x1 << 0)

/* MT6358_AUD_TOP_INT_CON0 */
#define RG_INT_EN_AUDIO_SFT                               0
#define RG_INT_EN_AUDIO_MASK                              0x1
#define RG_INT_EN_AUDIO_MASK_SFT                          (0x1 << 0)
#define RG_INT_EN_ACCDET_SFT                              5
#define RG_INT_EN_ACCDET_MASK                             0x1
#define RG_INT_EN_ACCDET_MASK_SFT                         (0x1 << 5)
#define RG_INT_EN_ACCDET_EINT0_SFT                        6
#define RG_INT_EN_ACCDET_EINT0_MASK                       0x1
#define RG_INT_EN_ACCDET_EINT0_MASK_SFT                   (0x1 << 6)
#define RG_INT_EN_ACCDET_EINT1_SFT                        7
#define RG_INT_EN_ACCDET_EINT1_MASK                       0x1
#define RG_INT_EN_ACCDET_EINT1_MASK_SFT                   (0x1 << 7)

/* MT6358_AUD_TOP_INT_CON0_SET */
#define RG_AUD_INT_CON0_SET_SFT                           0
#define RG_AUD_INT_CON0_SET_MASK                          0xffff
#define RG_AUD_INT_CON0_SET_MASK_SFT                      (0xffff << 0)

/* MT6358_AUD_TOP_INT_CON0_CLR */
#define RG_AUD_INT_CON0_CLR_SFT                           0
#define RG_AUD_INT_CON0_CLR_MASK                          0xffff
#define RG_AUD_INT_CON0_CLR_MASK_SFT                      (0xffff << 0)

/* MT6358_AUD_TOP_INT_MASK_CON0 */
#define RG_INT_MASK_AUDIO_SFT                             0
#define RG_INT_MASK_AUDIO_MASK                            0x1
#define RG_INT_MASK_AUDIO_MASK_SFT                        (0x1 << 0)
#define RG_INT_MASK_ACCDET_SFT                            5
#define RG_INT_MASK_ACCDET_MASK                           0x1
#define RG_INT_MASK_ACCDET_MASK_SFT                       (0x1 << 5)
#define RG_INT_MASK_ACCDET_EINT0_SFT                      6
#define RG_INT_MASK_ACCDET_EINT0_MASK                     0x1
#define RG_INT_MASK_ACCDET_EINT0_MASK_SFT                 (0x1 << 6)
#define RG_INT_MASK_ACCDET_EINT1_SFT                      7
#define RG_INT_MASK_ACCDET_EINT1_MASK                     0x1
#define RG_INT_MASK_ACCDET_EINT1_MASK_SFT                 (0x1 << 7)

/* MT6358_AUD_TOP_INT_MASK_CON0_SET */
#define RG_AUD_INT_MASK_CON0_SET_SFT                      0
#define RG_AUD_INT_MASK_CON0_SET_MASK                     0xff
#define RG_AUD_INT_MASK_CON0_SET_MASK_SFT                 (0xff << 0)

/* MT6358_AUD_TOP_INT_MASK_CON0_CLR */
#define RG_AUD_INT_MASK_CON0_CLR_SFT                      0
#define RG_AUD_INT_MASK_CON0_CLR_MASK                     0xff
#define RG_AUD_INT_MASK_CON0_CLR_MASK_SFT                 (0xff << 0)

/* MT6358_AUD_TOP_INT_STATUS0 */
#define RG_INT_STATUS_AUDIO_SFT                           0
#define RG_INT_STATUS_AUDIO_MASK                          0x1
#define RG_INT_STATUS_AUDIO_MASK_SFT                      (0x1 << 0)
#define RG_INT_STATUS_ACCDET_SFT                          5
#define RG_INT_STATUS_ACCDET_MASK                         0x1
#define RG_INT_STATUS_ACCDET_MASK_SFT                     (0x1 << 5)
#define RG_INT_STATUS_ACCDET_EINT0_SFT                    6
#define RG_INT_STATUS_ACCDET_EINT0_MASK                   0x1
#define RG_INT_STATUS_ACCDET_EINT0_MASK_SFT               (0x1 << 6)
#define RG_INT_STATUS_ACCDET_EINT1_SFT                    7
#define RG_INT_STATUS_ACCDET_EINT1_MASK                   0x1
#define RG_INT_STATUS_ACCDET_EINT1_MASK_SFT               (0x1 << 7)

/* MT6358_AUD_TOP_INT_RAW_STATUS0 */
#define RG_INT_RAW_STATUS_AUDIO_SFT                       0
#define RG_INT_RAW_STATUS_AUDIO_MASK                      0x1
#define RG_INT_RAW_STATUS_AUDIO_MASK_SFT                  (0x1 << 0)
#define RG_INT_RAW_STATUS_ACCDET_SFT                      5
#define RG_INT_RAW_STATUS_ACCDET_MASK                     0x1
#define RG_INT_RAW_STATUS_ACCDET_MASK_SFT                 (0x1 << 5)
#define RG_INT_RAW_STATUS_ACCDET_EINT0_SFT                6
#define RG_INT_RAW_STATUS_ACCDET_EINT0_MASK               0x1
#define RG_INT_RAW_STATUS_ACCDET_EINT0_MASK_SFT           (0x1 << 6)
#define RG_INT_RAW_STATUS_ACCDET_EINT1_SFT                7
#define RG_INT_RAW_STATUS_ACCDET_EINT1_MASK               0x1
#define RG_INT_RAW_STATUS_ACCDET_EINT1_MASK_SFT           (0x1 << 7)

/* MT6358_AUD_TOP_INT_MISC_CON0 */
#define RG_AUD_TOP_INT_POLARITY_SFT                       0
#define RG_AUD_TOP_INT_POLARITY_MASK                      0x1
#define RG_AUD_TOP_INT_POLARITY_MASK_SFT                  (0x1 << 0)

/* MT6358_AUDNCP_CLKDIV_CON0 */
#define RG_DIVCKS_CHG_SFT                                 0
#define RG_DIVCKS_CHG_MASK                                0x1
#define RG_DIVCKS_CHG_MASK_SFT                            (0x1 << 0)

/* MT6358_AUDNCP_CLKDIV_CON1 */
#define RG_DIVCKS_ON_SFT                                  0
#define RG_DIVCKS_ON_MASK                                 0x1
#define RG_DIVCKS_ON_MASK_SFT                             (0x1 << 0)

/* MT6358_AUDNCP_CLKDIV_CON2 */
#define RG_DIVCKS_PRG_SFT                                 0
#define RG_DIVCKS_PRG_MASK                                0x1ff
#define RG_DIVCKS_PRG_MASK_SFT                            (0x1ff << 0)

/* MT6358_AUDNCP_CLKDIV_CON3 */
#define RG_DIVCKS_PWD_NCP_SFT                             0
#define RG_DIVCKS_PWD_NCP_MASK                            0x1
#define RG_DIVCKS_PWD_NCP_MASK_SFT                        (0x1 << 0)

/* MT6358_AUDNCP_CLKDIV_CON4 */
#define RG_DIVCKS_PWD_NCP_ST_SEL_SFT                      0
#define RG_DIVCKS_PWD_NCP_ST_SEL_MASK                     0x3
#define RG_DIVCKS_PWD_NCP_ST_SEL_MASK_SFT                 (0x3 << 0)

/* MT6358_AUD_TOP_MON_CON0 */
#define RG_AUD_TOP_MON_SEL_SFT                            0
#define RG_AUD_TOP_MON_SEL_MASK                           0x7
#define RG_AUD_TOP_MON_SEL_MASK_SFT                       (0x7 << 0)
#define RG_AUD_CLK_INT_MON_FLAG_SEL_SFT                   3
#define RG_AUD_CLK_INT_MON_FLAG_SEL_MASK                  0xff
#define RG_AUD_CLK_INT_MON_FLAG_SEL_MASK_SFT              (0xff << 3)
#define RG_AUD_CLK_INT_MON_FLAG_EN_SFT                    11
#define RG_AUD_CLK_INT_MON_FLAG_EN_MASK                   0x1
#define RG_AUD_CLK_INT_MON_FLAG_EN_MASK_SFT               (0x1 << 11)

/* MT6358_AUDIO_DIG_DSN_ID */
#define AUDIO_DIG_ANA_ID_SFT                              0
#define AUDIO_DIG_ANA_ID_MASK                             0xff
#define AUDIO_DIG_ANA_ID_MASK_SFT                         (0xff << 0)
#define AUDIO_DIG_DIG_ID_SFT                              8
#define AUDIO_DIG_DIG_ID_MASK                             0xff
#define AUDIO_DIG_DIG_ID_MASK_SFT                         (0xff << 8)

/* MT6358_AUDIO_DIG_DSN_REV0 */
#define AUDIO_DIG_ANA_MINOR_REV_SFT                       0
#define AUDIO_DIG_ANA_MINOR_REV_MASK                      0xf
#define AUDIO_DIG_ANA_MINOR_REV_MASK_SFT                  (0xf << 0)
#define AUDIO_DIG_ANA_MAJOR_REV_SFT                       4
#define AUDIO_DIG_ANA_MAJOR_REV_MASK                      0xf
#define AUDIO_DIG_ANA_MAJOR_REV_MASK_SFT                  (0xf << 4)
#define AUDIO_DIG_DIG_MINOR_REV_SFT                       8
#define AUDIO_DIG_DIG_MINOR_REV_MASK                      0xf
#define AUDIO_DIG_DIG_MINOR_REV_MASK_SFT                  (0xf << 8)
#define AUDIO_DIG_DIG_MAJOR_REV_SFT                       12
#define AUDIO_DIG_DIG_MAJOR_REV_MASK                      0xf
#define AUDIO_DIG_DIG_MAJOR_REV_MASK_SFT                  (0xf << 12)

/* MT6358_AUDIO_DIG_DSN_DBI */
#define AUDIO_DIG_DSN_CBS_SFT                             0
#define AUDIO_DIG_DSN_CBS_MASK                            0x3
#define AUDIO_DIG_DSN_CBS_MASK_SFT                        (0x3 << 0)
#define AUDIO_DIG_DSN_BIX_SFT                             2
#define AUDIO_DIG_DSN_BIX_MASK                            0x3
#define AUDIO_DIG_DSN_BIX_MASK_SFT                        (0x3 << 2)
#define AUDIO_DIG_ESP_SFT                                 8
#define AUDIO_DIG_ESP_MASK                                0xff
#define AUDIO_DIG_ESP_MASK_SFT                            (0xff << 8)

/* MT6358_AUDIO_DIG_DSN_DXI */
#define AUDIO_DIG_DSN_FPI_SFT                             0
#define AUDIO_DIG_DSN_FPI_MASK                            0xff
#define AUDIO_DIG_DSN_FPI_MASK_SFT                        (0xff << 0)

/* MT6358_AFE_UL_DL_CON0 */
#define AFE_UL_LR_SWAP_SFT                                15
#define AFE_UL_LR_SWAP_MASK                               0x1
#define AFE_UL_LR_SWAP_MASK_SFT                           (0x1 << 15)
#define AFE_DL_LR_SWAP_SFT                                14
#define AFE_DL_LR_SWAP_MASK                               0x1
#define AFE_DL_LR_SWAP_MASK_SFT                           (0x1 << 14)
#define AFE_ON_SFT                                        0
#define AFE_ON_MASK                                       0x1
#define AFE_ON_MASK_SFT                                   (0x1 << 0)

/* MT6358_AFE_DL_SRC2_CON0_L */
#define DL_2_SRC_ON_TMP_CTL_PRE_SFT                       0
#define DL_2_SRC_ON_TMP_CTL_PRE_MASK                      0x1
#define DL_2_SRC_ON_TMP_CTL_PRE_MASK_SFT                  (0x1 << 0)

/* MT6358_AFE_UL_SRC_CON0_H */
#define C_DIGMIC_PHASE_SEL_CH1_CTL_SFT                    11
#define C_DIGMIC_PHASE_SEL_CH1_CTL_MASK                   0x7
#define C_DIGMIC_PHASE_SEL_CH1_CTL_MASK_SFT               (0x7 << 11)
#define C_DIGMIC_PHASE_SEL_CH2_CTL_SFT                    8
#define C_DIGMIC_PHASE_SEL_CH2_CTL_MASK                   0x7
#define C_DIGMIC_PHASE_SEL_CH2_CTL_MASK_SFT               (0x7 << 8)
#define C_TWO_DIGITAL_MIC_CTL_SFT                         7
#define C_TWO_DIGITAL_MIC_CTL_MASK                        0x1
#define C_TWO_DIGITAL_MIC_CTL_MASK_SFT                    (0x1 << 7)

/* MT6358_AFE_UL_SRC_CON0_L */
#define DMIC_LOW_POWER_MODE_CTL_SFT                       14
#define DMIC_LOW_POWER_MODE_CTL_MASK                      0x3
#define DMIC_LOW_POWER_MODE_CTL_MASK_SFT                  (0x3 << 14)
#define DIGMIC_3P25M_1P625M_SEL_CTL_SFT                   5
#define DIGMIC_3P25M_1P625M_SEL_CTL_MASK                  0x1
#define DIGMIC_3P25M_1P625M_SEL_CTL_MASK_SFT              (0x1 << 5)
#define UL_LOOP_BACK_MODE_CTL_SFT                         2
#define UL_LOOP_BACK_MODE_CTL_MASK                        0x1
#define UL_LOOP_BACK_MODE_CTL_MASK_SFT                    (0x1 << 2)
#define UL_SDM_3_LEVEL_CTL_SFT                            1
#define UL_SDM_3_LEVEL_CTL_MASK                           0x1
#define UL_SDM_3_LEVEL_CTL_MASK_SFT                       (0x1 << 1)
#define UL_SRC_ON_TMP_CTL_SFT                             0
#define UL_SRC_ON_TMP_CTL_MASK                            0x1
#define UL_SRC_ON_TMP_CTL_MASK_SFT                        (0x1 << 0)

/* MT6358_AFE_TOP_CON0 */
#define MTKAIF_SINE_ON_SFT                                2
#define MTKAIF_SINE_ON_MASK                               0x1
#define MTKAIF_SINE_ON_MASK_SFT                           (0x1 << 2)
#define UL_SINE_ON_SFT                                    1
#define UL_SINE_ON_MASK                                   0x1
#define UL_SINE_ON_MASK_SFT                               (0x1 << 1)
#define DL_SINE_ON_SFT                                    0
#define DL_SINE_ON_MASK                                   0x1
#define DL_SINE_ON_MASK_SFT                               (0x1 << 0)

/* MT6358_AUDIO_TOP_CON0 */
#define PDN_AFE_CTL_SFT                                   7
#define PDN_AFE_CTL_MASK                                  0x1
#define PDN_AFE_CTL_MASK_SFT                              (0x1 << 7)
#define PDN_DAC_CTL_SFT                                   6
#define PDN_DAC_CTL_MASK                                  0x1
#define PDN_DAC_CTL_MASK_SFT                              (0x1 << 6)
#define PDN_ADC_CTL_SFT                                   5
#define PDN_ADC_CTL_MASK                                  0x1
#define PDN_ADC_CTL_MASK_SFT                              (0x1 << 5)
#define PDN_I2S_DL_CTL_SFT                                3
#define PDN_I2S_DL_CTL_MASK                               0x1
#define PDN_I2S_DL_CTL_MASK_SFT                           (0x1 << 3)
#define PWR_CLK_DIS_CTL_SFT                               2
#define PWR_CLK_DIS_CTL_MASK                              0x1
#define PWR_CLK_DIS_CTL_MASK_SFT                          (0x1 << 2)
#define PDN_AFE_TESTMODEL_CTL_SFT                         1
#define PDN_AFE_TESTMODEL_CTL_MASK                        0x1
#define PDN_AFE_TESTMODEL_CTL_MASK_SFT                    (0x1 << 1)
#define PDN_RESERVED_SFT                                  0
#define PDN_RESERVED_MASK                                 0x1
#define PDN_RESERVED_MASK_SFT                             (0x1 << 0)

/* MT6358_AFE_MON_DEBUG0 */
#define AUDIO_SYS_TOP_MON_SWAP_SFT                        14
#define AUDIO_SYS_TOP_MON_SWAP_MASK                       0x3
#define AUDIO_SYS_TOP_MON_SWAP_MASK_SFT                   (0x3 << 14)
#define AUDIO_SYS_TOP_MON_SEL_SFT                         8
#define AUDIO_SYS_TOP_MON_SEL_MASK                        0x1f
#define AUDIO_SYS_TOP_MON_SEL_MASK_SFT                    (0x1f << 8)
#define AFE_MON_SEL_SFT                                   0
#define AFE_MON_SEL_MASK                                  0xff
#define AFE_MON_SEL_MASK_SFT                              (0xff << 0)

/* MT6358_AFUNC_AUD_CON0 */
#define CCI_AUD_ANACK_SEL_SFT                             15
#define CCI_AUD_ANACK_SEL_MASK                            0x1
#define CCI_AUD_ANACK_SEL_MASK_SFT                        (0x1 << 15)
#define CCI_AUDIO_FIFO_WPTR_SFT                           12
#define CCI_AUDIO_FIFO_WPTR_MASK                          0x7
#define CCI_AUDIO_FIFO_WPTR_MASK_SFT                      (0x7 << 12)
#define CCI_SCRAMBLER_CG_EN_SFT                           11
#define CCI_SCRAMBLER_CG_EN_MASK                          0x1
#define CCI_SCRAMBLER_CG_EN_MASK_SFT                      (0x1 << 11)
#define CCI_LCH_INV_SFT                                   10
#define CCI_LCH_INV_MASK                                  0x1
#define CCI_LCH_INV_MASK_SFT                              (0x1 << 10)
#define CCI_RAND_EN_SFT                                   9
#define CCI_RAND_EN_MASK                                  0x1
#define CCI_RAND_EN_MASK_SFT                              (0x1 << 9)
#define CCI_SPLT_SCRMB_CLK_ON_SFT                         8
#define CCI_SPLT_SCRMB_CLK_ON_MASK                        0x1
#define CCI_SPLT_SCRMB_CLK_ON_MASK_SFT                    (0x1 << 8)
#define CCI_SPLT_SCRMB_ON_SFT                             7
#define CCI_SPLT_SCRMB_ON_MASK                            0x1
#define CCI_SPLT_SCRMB_ON_MASK_SFT                        (0x1 << 7)
#define CCI_AUD_IDAC_TEST_EN_SFT                          6
#define CCI_AUD_IDAC_TEST_EN_MASK                         0x1
#define CCI_AUD_IDAC_TEST_EN_MASK_SFT                     (0x1 << 6)
#define CCI_ZERO_PAD_DISABLE_SFT                          5
#define CCI_ZERO_PAD_DISABLE_MASK                         0x1
#define CCI_ZERO_PAD_DISABLE_MASK_SFT                     (0x1 << 5)
#define CCI_AUD_SPLIT_TEST_EN_SFT                         4
#define CCI_AUD_SPLIT_TEST_EN_MASK                        0x1
#define CCI_AUD_SPLIT_TEST_EN_MASK_SFT                    (0x1 << 4)
#define CCI_AUD_SDM_MUTEL_SFT                             3
#define CCI_AUD_SDM_MUTEL_MASK                            0x1
#define CCI_AUD_SDM_MUTEL_MASK_SFT                        (0x1 << 3)
#define CCI_AUD_SDM_MUTER_SFT                             2
#define CCI_AUD_SDM_MUTER_MASK                            0x1
#define CCI_AUD_SDM_MUTER_MASK_SFT                        (0x1 << 2)
#define CCI_AUD_SDM_7BIT_SEL_SFT                          1
#define CCI_AUD_SDM_7BIT_SEL_MASK                         0x1
#define CCI_AUD_SDM_7BIT_SEL_MASK_SFT                     (0x1 << 1)
#define CCI_SCRAMBLER_EN_SFT                              0
#define CCI_SCRAMBLER_EN_MASK                             0x1
#define CCI_SCRAMBLER_EN_MASK_SFT                         (0x1 << 0)

/* MT6358_AFUNC_AUD_CON1 */
#define AUD_SDM_TEST_L_SFT                                8
#define AUD_SDM_TEST_L_MASK                               0xff
#define AUD_SDM_TEST_L_MASK_SFT                           (0xff << 8)
#define AUD_SDM_TEST_R_SFT                                0
#define AUD_SDM_TEST_R_MASK                               0xff
#define AUD_SDM_TEST_R_MASK_SFT                           (0xff << 0)

/* MT6358_AFUNC_AUD_CON2 */
#define CCI_AUD_DAC_ANA_MUTE_SFT                          7
#define CCI_AUD_DAC_ANA_MUTE_MASK                         0x1
#define CCI_AUD_DAC_ANA_MUTE_MASK_SFT                     (0x1 << 7)
#define CCI_AUD_DAC_ANA_RSTB_SEL_SFT                      6
#define CCI_AUD_DAC_ANA_RSTB_SEL_MASK                     0x1
#define CCI_AUD_DAC_ANA_RSTB_SEL_MASK_SFT                 (0x1 << 6)
#define CCI_AUDIO_FIFO_CLKIN_INV_SFT                      4
#define CCI_AUDIO_FIFO_CLKIN_INV_MASK                     0x1
#define CCI_AUDIO_FIFO_CLKIN_INV_MASK_SFT                 (0x1 << 4)
#define CCI_AUDIO_FIFO_ENABLE_SFT                         3
#define CCI_AUDIO_FIFO_ENABLE_MASK                        0x1
#define CCI_AUDIO_FIFO_ENABLE_MASK_SFT                    (0x1 << 3)
#define CCI_ACD_MODE_SFT                                  2
#define CCI_ACD_MODE_MASK                                 0x1
#define CCI_ACD_MODE_MASK_SFT                             (0x1 << 2)
#define CCI_AFIFO_CLK_PWDB_SFT                            1
#define CCI_AFIFO_CLK_PWDB_MASK                           0x1
#define CCI_AFIFO_CLK_PWDB_MASK_SFT                       (0x1 << 1)
#define CCI_ACD_FUNC_RSTB_SFT                             0
#define CCI_ACD_FUNC_RSTB_MASK                            0x1
#define CCI_ACD_FUNC_RSTB_MASK_SFT                        (0x1 << 0)

/* MT6358_AFUNC_AUD_CON3 */
#define SDM_ANA13M_TESTCK_SEL_SFT                         15
#define SDM_ANA13M_TESTCK_SEL_MASK                        0x1
#define SDM_ANA13M_TESTCK_SEL_MASK_SFT                    (0x1 << 15)
#define SDM_ANA13M_TESTCK_SRC_SEL_SFT                     12
#define SDM_ANA13M_TESTCK_SRC_SEL_MASK                    0x7
#define SDM_ANA13M_TESTCK_SRC_SEL_MASK_SFT                (0x7 << 12)
#define SDM_TESTCK_SRC_SEL_SFT                            8
#define SDM_TESTCK_SRC_SEL_MASK                           0x7
#define SDM_TESTCK_SRC_SEL_MASK_SFT                       (0x7 << 8)
#define DIGMIC_TESTCK_SRC_SEL_SFT                         4
#define DIGMIC_TESTCK_SRC_SEL_MASK                        0x7
#define DIGMIC_TESTCK_SRC_SEL_MASK_SFT                    (0x7 << 4)
#define DIGMIC_TESTCK_SEL_SFT                             0
#define DIGMIC_TESTCK_SEL_MASK                            0x1
#define DIGMIC_TESTCK_SEL_MASK_SFT                        (0x1 << 0)

/* MT6358_AFUNC_AUD_CON4 */
#define UL_FIFO_WCLK_INV_SFT                              8
#define UL_FIFO_WCLK_INV_MASK                             0x1
#define UL_FIFO_WCLK_INV_MASK_SFT                         (0x1 << 8)
#define UL_FIFO_DIGMIC_WDATA_TESTSRC_SEL_SFT              6
#define UL_FIFO_DIGMIC_WDATA_TESTSRC_SEL_MASK             0x1
#define UL_FIFO_DIGMIC_WDATA_TESTSRC_SEL_MASK_SFT         (0x1 << 6)
#define UL_FIFO_WDATA_TESTEN_SFT                          5
#define UL_FIFO_WDATA_TESTEN_MASK                         0x1
#define UL_FIFO_WDATA_TESTEN_MASK_SFT                     (0x1 << 5)
#define UL_FIFO_WDATA_TESTSRC_SEL_SFT                     4
#define UL_FIFO_WDATA_TESTSRC_SEL_MASK                    0x1
#define UL_FIFO_WDATA_TESTSRC_SEL_MASK_SFT                (0x1 << 4)
#define UL_FIFO_WCLK_6P5M_TESTCK_SEL_SFT                  3
#define UL_FIFO_WCLK_6P5M_TESTCK_SEL_MASK                 0x1
#define UL_FIFO_WCLK_6P5M_TESTCK_SEL_MASK_SFT             (0x1 << 3)
#define UL_FIFO_WCLK_6P5M_TESTCK_SRC_SEL_SFT              0
#define UL_FIFO_WCLK_6P5M_TESTCK_SRC_SEL_MASK             0x7
#define UL_FIFO_WCLK_6P5M_TESTCK_SRC_SEL_MASK_SFT         (0x7 << 0)

/* MT6358_AFUNC_AUD_CON5 */
#define R_AUD_DAC_POS_LARGE_MONO_SFT                      8
#define R_AUD_DAC_POS_LARGE_MONO_MASK                     0xff
#define R_AUD_DAC_POS_LARGE_MONO_MASK_SFT                 (0xff << 8)
#define R_AUD_DAC_NEG_LARGE_MONO_SFT                      0
#define R_AUD_DAC_NEG_LARGE_MONO_MASK                     0xff
#define R_AUD_DAC_NEG_LARGE_MONO_MASK_SFT                 (0xff << 0)

/* MT6358_AFUNC_AUD_CON6 */
#define R_AUD_DAC_POS_SMALL_MONO_SFT                      12
#define R_AUD_DAC_POS_SMALL_MONO_MASK                     0xf
#define R_AUD_DAC_POS_SMALL_MONO_MASK_SFT                 (0xf << 12)
#define R_AUD_DAC_NEG_SMALL_MONO_SFT                      8
#define R_AUD_DAC_NEG_SMALL_MONO_MASK                     0xf
#define R_AUD_DAC_NEG_SMALL_MONO_MASK_SFT                 (0xf << 8)
#define R_AUD_DAC_POS_TINY_MONO_SFT                       6
#define R_AUD_DAC_POS_TINY_MONO_MASK                      0x3
#define R_AUD_DAC_POS_TINY_MONO_MASK_SFT                  (0x3 << 6)
#define R_AUD_DAC_NEG_TINY_MONO_SFT                       4
#define R_AUD_DAC_NEG_TINY_MONO_MASK                      0x3
#define R_AUD_DAC_NEG_TINY_MONO_MASK_SFT                  (0x3 << 4)
#define R_AUD_DAC_MONO_SEL_SFT                            3
#define R_AUD_DAC_MONO_SEL_MASK                           0x1
#define R_AUD_DAC_MONO_SEL_MASK_SFT                       (0x1 << 3)
#define R_AUD_DAC_SW_RSTB_SFT                             0
#define R_AUD_DAC_SW_RSTB_MASK                            0x1
#define R_AUD_DAC_SW_RSTB_MASK_SFT                        (0x1 << 0)

/* MT6358_AFUNC_AUD_MON0 */
#define AUD_SCR_OUT_L_SFT                                 8
#define AUD_SCR_OUT_L_MASK                                0xff
#define AUD_SCR_OUT_L_MASK_SFT                            (0xff << 8)
#define AUD_SCR_OUT_R_SFT                                 0
#define AUD_SCR_OUT_R_MASK                                0xff
#define AUD_SCR_OUT_R_MASK_SFT                            (0xff << 0)

/* MT6358_AUDRC_TUNE_MON0 */
#define ASYNC_TEST_OUT_BCK_SFT                            15
#define ASYNC_TEST_OUT_BCK_MASK                           0x1
#define ASYNC_TEST_OUT_BCK_MASK_SFT                       (0x1 << 15)
#define RGS_AUDRCTUNE1READ_SFT                            8
#define RGS_AUDRCTUNE1READ_MASK                           0x1f
#define RGS_AUDRCTUNE1READ_MASK_SFT                       (0x1f << 8)
#define RGS_AUDRCTUNE0READ_SFT                            0
#define RGS_AUDRCTUNE0READ_MASK                           0x1f
#define RGS_AUDRCTUNE0READ_MASK_SFT                       (0x1f << 0)

/* MT6358_AFE_ADDA_MTKAIF_FIFO_CFG0 */
#define AFE_RESERVED_SFT                                  1
#define AFE_RESERVED_MASK                                 0x7fff
#define AFE_RESERVED_MASK_SFT                             (0x7fff << 1)
#define RG_MTKAIF_RXIF_FIFO_INTEN_SFT                     0
#define RG_MTKAIF_RXIF_FIFO_INTEN_MASK                    0x1
#define RG_MTKAIF_RXIF_FIFO_INTEN_MASK_SFT                (0x1 << 0)

/* MT6358_AFE_ADDA_MTKAIF_FIFO_LOG_MON1 */
#define MTKAIF_RXIF_WR_FULL_STATUS_SFT                    1
#define MTKAIF_RXIF_WR_FULL_STATUS_MASK                   0x1
#define MTKAIF_RXIF_WR_FULL_STATUS_MASK_SFT               (0x1 << 1)
#define MTKAIF_RXIF_RD_EMPTY_STATUS_SFT                   0
#define MTKAIF_RXIF_RD_EMPTY_STATUS_MASK                  0x1
#define MTKAIF_RXIF_RD_EMPTY_STATUS_MASK_SFT              (0x1 << 0)

/* MT6358_AFE_ADDA_MTKAIF_MON0 */
#define MTKAIFTX_V3_SYNC_OUT_SFT                          14
#define MTKAIFTX_V3_SYNC_OUT_MASK                         0x1
#define MTKAIFTX_V3_SYNC_OUT_MASK_SFT                     (0x1 << 14)
#define MTKAIFTX_V3_SDATA_OUT2_SFT                        13
#define MTKAIFTX_V3_SDATA_OUT2_MASK                       0x1
#define MTKAIFTX_V3_SDATA_OUT2_MASK_SFT                   (0x1 << 13)
#define MTKAIFTX_V3_SDATA_OUT1_SFT                        12
#define MTKAIFTX_V3_SDATA_OUT1_MASK                       0x1
#define MTKAIFTX_V3_SDATA_OUT1_MASK_SFT                   (0x1 << 12)
#define MTKAIF_RXIF_FIFO_STATUS_SFT                       0
#define MTKAIF_RXIF_FIFO_STATUS_MASK                      0xfff
#define MTKAIF_RXIF_FIFO_STATUS_MASK_SFT                  (0xfff << 0)

/* MT6358_AFE_ADDA_MTKAIF_MON1 */
#define MTKAIFRX_V3_SYNC_IN_SFT                           14
#define MTKAIFRX_V3_SYNC_IN_MASK                          0x1
#define MTKAIFRX_V3_SYNC_IN_MASK_SFT                      (0x1 << 14)
#define MTKAIFRX_V3_SDATA_IN2_SFT                         13
#define MTKAIFRX_V3_SDATA_IN2_MASK                        0x1
#define MTKAIFRX_V3_SDATA_IN2_MASK_SFT                    (0x1 << 13)
#define MTKAIFRX_V3_SDATA_IN1_SFT                         12
#define MTKAIFRX_V3_SDATA_IN1_MASK                        0x1
#define MTKAIFRX_V3_SDATA_IN1_MASK_SFT                    (0x1 << 12)
#define MTKAIF_RXIF_SEARCH_FAIL_FLAG_SFT                  11
#define MTKAIF_RXIF_SEARCH_FAIL_FLAG_MASK                 0x1
#define MTKAIF_RXIF_SEARCH_FAIL_FLAG_MASK_SFT             (0x1 << 11)
#define MTKAIF_RXIF_INVALID_FLAG_SFT                      8
#define MTKAIF_RXIF_INVALID_FLAG_MASK                     0x1
#define MTKAIF_RXIF_INVALID_FLAG_MASK_SFT                 (0x1 << 8)
#define MTKAIF_RXIF_INVALID_CYCLE_SFT                     0
#define MTKAIF_RXIF_INVALID_CYCLE_MASK                    0xff
#define MTKAIF_RXIF_INVALID_CYCLE_MASK_SFT                (0xff << 0)

/* MT6358_AFE_ADDA_MTKAIF_MON2 */
#define MTKAIF_TXIF_IN_CH2_SFT                            8
#define MTKAIF_TXIF_IN_CH2_MASK                           0xff
#define MTKAIF_TXIF_IN_CH2_MASK_SFT                       (0xff << 8)
#define MTKAIF_TXIF_IN_CH1_SFT                            0
#define MTKAIF_TXIF_IN_CH1_MASK                           0xff
#define MTKAIF_TXIF_IN_CH1_MASK_SFT                       (0xff << 0)

/* MT6358_AFE_ADDA_MTKAIF_MON3 */
#define MTKAIF_RXIF_OUT_CH2_SFT                           8
#define MTKAIF_RXIF_OUT_CH2_MASK                          0xff
#define MTKAIF_RXIF_OUT_CH2_MASK_SFT                      (0xff << 8)
#define MTKAIF_RXIF_OUT_CH1_SFT                           0
#define MTKAIF_RXIF_OUT_CH1_MASK                          0xff
#define MTKAIF_RXIF_OUT_CH1_MASK_SFT                      (0xff << 0)

/* MT6358_AFE_ADDA_MTKAIF_CFG0 */
#define RG_MTKAIF_RXIF_CLKINV_SFT                         15
#define RG_MTKAIF_RXIF_CLKINV_MASK                        0x1
#define RG_MTKAIF_RXIF_CLKINV_MASK_SFT                    (0x1 << 15)
#define RG_MTKAIF_RXIF_PROTOCOL2_SFT                      8
#define RG_MTKAIF_RXIF_PROTOCOL2_MASK                     0x1
#define RG_MTKAIF_RXIF_PROTOCOL2_MASK_SFT                 (0x1 << 8)
#define RG_MTKAIF_BYPASS_SRC_MODE_SFT                     6
#define RG_MTKAIF_BYPASS_SRC_MODE_MASK                    0x3
#define RG_MTKAIF_BYPASS_SRC_MODE_MASK_SFT                (0x3 << 6)
#define RG_MTKAIF_BYPASS_SRC_TEST_SFT                     5
#define RG_MTKAIF_BYPASS_SRC_TEST_MASK                    0x1
#define RG_MTKAIF_BYPASS_SRC_TEST_MASK_SFT                (0x1 << 5)
#define RG_MTKAIF_TXIF_PROTOCOL2_SFT                      4
#define RG_MTKAIF_TXIF_PROTOCOL2_MASK                     0x1
#define RG_MTKAIF_TXIF_PROTOCOL2_MASK_SFT                 (0x1 << 4)
#define RG_MTKAIF_PMIC_TXIF_8TO5_SFT                      2
#define RG_MTKAIF_PMIC_TXIF_8TO5_MASK                     0x1
#define RG_MTKAIF_PMIC_TXIF_8TO5_MASK_SFT                 (0x1 << 2)
#define RG_MTKAIF_LOOPBACK_TEST2_SFT                      1
#define RG_MTKAIF_LOOPBACK_TEST2_MASK                     0x1
#define RG_MTKAIF_LOOPBACK_TEST2_MASK_SFT                 (0x1 << 1)
#define RG_MTKAIF_LOOPBACK_TEST1_SFT                      0
#define RG_MTKAIF_LOOPBACK_TEST1_MASK                     0x1
#define RG_MTKAIF_LOOPBACK_TEST1_MASK_SFT                 (0x1 << 0)

/* MT6358_AFE_ADDA_MTKAIF_RX_CFG0 */
#define RG_MTKAIF_RXIF_VOICE_MODE_SFT                     12
#define RG_MTKAIF_RXIF_VOICE_MODE_MASK                    0xf
#define RG_MTKAIF_RXIF_VOICE_MODE_MASK_SFT                (0xf << 12)
#define RG_MTKAIF_RXIF_DATA_BIT_SFT                       8
#define RG_MTKAIF_RXIF_DATA_BIT_MASK                      0x7
#define RG_MTKAIF_RXIF_DATA_BIT_MASK_SFT                  (0x7 << 8)
#define RG_MTKAIF_RXIF_FIFO_RSP_SFT                       4
#define RG_MTKAIF_RXIF_FIFO_RSP_MASK                      0x7
#define RG_MTKAIF_RXIF_FIFO_RSP_MASK_SFT                  (0x7 << 4)
#define RG_MTKAIF_RXIF_DETECT_ON_SFT                      3
#define RG_MTKAIF_RXIF_DETECT_ON_MASK                     0x1
#define RG_MTKAIF_RXIF_DETECT_ON_MASK_SFT                 (0x1 << 3)
#define RG_MTKAIF_RXIF_DATA_MODE_SFT                      0
#define RG_MTKAIF_RXIF_DATA_MODE_MASK                     0x1
#define RG_MTKAIF_RXIF_DATA_MODE_MASK_SFT                 (0x1 << 0)

/* MT6358_AFE_ADDA_MTKAIF_RX_CFG1 */
#define RG_MTKAIF_RXIF_SYNC_SEARCH_TABLE_SFT              12
#define RG_MTKAIF_RXIF_SYNC_SEARCH_TABLE_MASK             0xf
#define RG_MTKAIF_RXIF_SYNC_SEARCH_TABLE_MASK_SFT         (0xf << 12)
#define RG_MTKAIF_RXIF_INVALID_SYNC_CHECK_ROUND_SFT       8
#define RG_MTKAIF_RXIF_INVALID_SYNC_CHECK_ROUND_MASK      0xf
#define RG_MTKAIF_RXIF_INVALID_SYNC_CHECK_ROUND_MASK_SFT  (0xf << 8)
#define RG_MTKAIF_RXIF_SYNC_CHECK_ROUND_SFT               4
#define RG_MTKAIF_RXIF_SYNC_CHECK_ROUND_MASK              0xf
#define RG_MTKAIF_RXIF_SYNC_CHECK_ROUND_MASK_SFT          (0xf << 4)
#define RG_MTKAIF_RXIF_VOICE_MODE_PROTOCOL2_SFT           0
#define RG_MTKAIF_RXIF_VOICE_MODE_PROTOCOL2_MASK          0xf
#define RG_MTKAIF_RXIF_VOICE_MODE_PROTOCOL2_MASK_SFT      (0xf << 0)

/* MT6358_AFE_ADDA_MTKAIF_RX_CFG2 */
#define RG_MTKAIF_RXIF_CLEAR_SYNC_FAIL_SFT                12
#define RG_MTKAIF_RXIF_CLEAR_SYNC_FAIL_MASK               0x1
#define RG_MTKAIF_RXIF_CLEAR_SYNC_FAIL_MASK_SFT           (0x1 << 12)
#define RG_MTKAIF_RXIF_SYNC_CNT_TABLE_SFT                 0
#define RG_MTKAIF_RXIF_SYNC_CNT_TABLE_MASK                0xfff
#define RG_MTKAIF_RXIF_SYNC_CNT_TABLE_MASK_SFT            (0xfff << 0)

/* MT6358_AFE_ADDA_MTKAIF_RX_CFG3 */
#define RG_MTKAIF_RXIF_LOOPBACK_USE_NLE_SFT               7
#define RG_MTKAIF_RXIF_LOOPBACK_USE_NLE_MASK              0x1
#define RG_MTKAIF_RXIF_LOOPBACK_USE_NLE_MASK_SFT          (0x1 << 7)
#define RG_MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_SFT             4
#define RG_MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_MASK            0x7
#define RG_MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_MASK_SFT        (0x7 << 4)
#define RG_MTKAIF_RXIF_DETECT_ON_PROTOCOL2_SFT            3
#define RG_MTKAIF_RXIF_DETECT_ON_PROTOCOL2_MASK           0x1
#define RG_MTKAIF_RXIF_DETECT_ON_PROTOCOL2_MASK_SFT       (0x1 << 3)

/* MT6358_AFE_ADDA_MTKAIF_TX_CFG1 */
#define RG_MTKAIF_SYNC_WORD2_SFT                          4
#define RG_MTKAIF_SYNC_WORD2_MASK                         0x7
#define RG_MTKAIF_SYNC_WORD2_MASK_SFT                     (0x7 << 4)
#define RG_MTKAIF_SYNC_WORD1_SFT                          0
#define RG_MTKAIF_SYNC_WORD1_MASK                         0x7
#define RG_MTKAIF_SYNC_WORD1_MASK_SFT                     (0x7 << 0)

/* MT6358_AFE_SGEN_CFG0 */
#define SGEN_AMP_DIV_CH1_CTL_SFT                          12
#define SGEN_AMP_DIV_CH1_CTL_MASK                         0xf
#define SGEN_AMP_DIV_CH1_CTL_MASK_SFT                     (0xf << 12)
#define SGEN_DAC_EN_CTL_SFT                               7
#define SGEN_DAC_EN_CTL_MASK                              0x1
#define SGEN_DAC_EN_CTL_MASK_SFT                          (0x1 << 7)
#define SGEN_MUTE_SW_CTL_SFT                              6
#define SGEN_MUTE_SW_CTL_MASK                             0x1
#define SGEN_MUTE_SW_CTL_MASK_SFT                         (0x1 << 6)
#define R_AUD_SDM_MUTE_L_SFT                              5
#define R_AUD_SDM_MUTE_L_MASK                             0x1
#define R_AUD_SDM_MUTE_L_MASK_SFT                         (0x1 << 5)
#define R_AUD_SDM_MUTE_R_SFT                              4
#define R_AUD_SDM_MUTE_R_MASK                             0x1
#define R_AUD_SDM_MUTE_R_MASK_SFT                         (0x1 << 4)

/* MT6358_AFE_SGEN_CFG1 */
#define C_SGEN_RCH_INV_5BIT_SFT                           15
#define C_SGEN_RCH_INV_5BIT_MASK                          0x1
#define C_SGEN_RCH_INV_5BIT_MASK_SFT                      (0x1 << 15)
#define C_SGEN_RCH_INV_8BIT_SFT                           14
#define C_SGEN_RCH_INV_8BIT_MASK                          0x1
#define C_SGEN_RCH_INV_8BIT_MASK_SFT                      (0x1 << 14)
#define SGEN_FREQ_DIV_CH1_CTL_SFT                         0
#define SGEN_FREQ_DIV_CH1_CTL_MASK                        0x1f
#define SGEN_FREQ_DIV_CH1_CTL_MASK_SFT                    (0x1f << 0)

/* MT6358_AFE_ADC_ASYNC_FIFO_CFG */
#define RG_UL_ASYNC_FIFO_SOFT_RST_EN_SFT                  5
#define RG_UL_ASYNC_FIFO_SOFT_RST_EN_MASK                 0x1
#define RG_UL_ASYNC_FIFO_SOFT_RST_EN_MASK_SFT             (0x1 << 5)
#define RG_UL_ASYNC_FIFO_SOFT_RST_SFT                     4
#define RG_UL_ASYNC_FIFO_SOFT_RST_MASK                    0x1
#define RG_UL_ASYNC_FIFO_SOFT_RST_MASK_SFT                (0x1 << 4)
#define RG_AMIC_UL_ADC_CLK_SEL_SFT                        1
#define RG_AMIC_UL_ADC_CLK_SEL_MASK                       0x1
#define RG_AMIC_UL_ADC_CLK_SEL_MASK_SFT                   (0x1 << 1)

/* MT6358_AFE_DCCLK_CFG0 */
#define DCCLK_DIV_SFT                                     5
#define DCCLK_DIV_MASK                                    0x7ff
#define DCCLK_DIV_MASK_SFT                                (0x7ff << 5)
#define DCCLK_INV_SFT                                     4
#define DCCLK_INV_MASK                                    0x1
#define DCCLK_INV_MASK_SFT                                (0x1 << 4)
#define DCCLK_PDN_SFT                                     1
#define DCCLK_PDN_MASK                                    0x1
#define DCCLK_PDN_MASK_SFT                                (0x1 << 1)
#define DCCLK_GEN_ON_SFT                                  0
#define DCCLK_GEN_ON_MASK                                 0x1
#define DCCLK_GEN_ON_MASK_SFT                             (0x1 << 0)

/* MT6358_AFE_DCCLK_CFG1 */
#define RESYNC_SRC_SEL_SFT                                10
#define RESYNC_SRC_SEL_MASK                               0x3
#define RESYNC_SRC_SEL_MASK_SFT                           (0x3 << 10)
#define RESYNC_SRC_CK_INV_SFT                             9
#define RESYNC_SRC_CK_INV_MASK                            0x1
#define RESYNC_SRC_CK_INV_MASK_SFT                        (0x1 << 9)
#define DCCLK_RESYNC_BYPASS_SFT                           8
#define DCCLK_RESYNC_BYPASS_MASK                          0x1
#define DCCLK_RESYNC_BYPASS_MASK_SFT                      (0x1 << 8)
#define DCCLK_PHASE_SEL_SFT                               4
#define DCCLK_PHASE_SEL_MASK                              0xf
#define DCCLK_PHASE_SEL_MASK_SFT                          (0xf << 4)

/* MT6358_AUDIO_DIG_CFG */
#define RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_SFT             15
#define RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_MASK            0x1
#define RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_MASK_SFT        (0x1 << 15)
#define RG_AUD_PAD_TOP_PHASE_MODE2_SFT                    8
#define RG_AUD_PAD_TOP_PHASE_MODE2_MASK                   0x7f
#define RG_AUD_PAD_TOP_PHASE_MODE2_MASK_SFT               (0x7f << 8)
#define RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_SFT              7
#define RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_MASK             0x1
#define RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_MASK_SFT         (0x1 << 7)
#define RG_AUD_PAD_TOP_PHASE_MODE_SFT                     0
#define RG_AUD_PAD_TOP_PHASE_MODE_MASK                    0x7f
#define RG_AUD_PAD_TOP_PHASE_MODE_MASK_SFT                (0x7f << 0)

/* MT6358_AFE_AUD_PAD_TOP */
#define RG_AUD_PAD_TOP_TX_FIFO_RSP_SFT                    12
#define RG_AUD_PAD_TOP_TX_FIFO_RSP_MASK                   0x7
#define RG_AUD_PAD_TOP_TX_FIFO_RSP_MASK_SFT               (0x7 << 12)
#define RG_AUD_PAD_TOP_MTKAIF_CLK_PROTOCOL2_SFT           11
#define RG_AUD_PAD_TOP_MTKAIF_CLK_PROTOCOL2_MASK          0x1
#define RG_AUD_PAD_TOP_MTKAIF_CLK_PROTOCOL2_MASK_SFT      (0x1 << 11)
#define RG_AUD_PAD_TOP_TX_FIFO_ON_SFT                     8
#define RG_AUD_PAD_TOP_TX_FIFO_ON_MASK                    0x1
#define RG_AUD_PAD_TOP_TX_FIFO_ON_MASK_SFT                (0x1 << 8)

/* MT6358_AFE_AUD_PAD_TOP_MON */
#define ADDA_AUD_PAD_TOP_MON_SFT                          0
#define ADDA_AUD_PAD_TOP_MON_MASK                         0xffff
#define ADDA_AUD_PAD_TOP_MON_MASK_SFT                     (0xffff << 0)

/* MT6358_AFE_AUD_PAD_TOP_MON1 */
#define ADDA_AUD_PAD_TOP_MON1_SFT                         0
#define ADDA_AUD_PAD_TOP_MON1_MASK                        0xffff
#define ADDA_AUD_PAD_TOP_MON1_MASK_SFT                    (0xffff << 0)

/* MT6358_AFE_DL_NLE_CFG */
#define NLE_RCH_HPGAIN_SEL_SFT                            10
#define NLE_RCH_HPGAIN_SEL_MASK                           0x1
#define NLE_RCH_HPGAIN_SEL_MASK_SFT                       (0x1 << 10)
#define NLE_RCH_CH_SEL_SFT                                9
#define NLE_RCH_CH_SEL_MASK                               0x1
#define NLE_RCH_CH_SEL_MASK_SFT                           (0x1 << 9)
#define NLE_RCH_ON_SFT                                    8
#define NLE_RCH_ON_MASK                                   0x1
#define NLE_RCH_ON_MASK_SFT                               (0x1 << 8)
#define NLE_LCH_HPGAIN_SEL_SFT                            2
#define NLE_LCH_HPGAIN_SEL_MASK                           0x1
#define NLE_LCH_HPGAIN_SEL_MASK_SFT                       (0x1 << 2)
#define NLE_LCH_CH_SEL_SFT                                1
#define NLE_LCH_CH_SEL_MASK                               0x1
#define NLE_LCH_CH_SEL_MASK_SFT                           (0x1 << 1)
#define NLE_LCH_ON_SFT                                    0
#define NLE_LCH_ON_MASK                                   0x1
#define NLE_LCH_ON_MASK_SFT                               (0x1 << 0)

/* MT6358_AFE_DL_NLE_MON */
#define NLE_MONITOR_SFT                                   0
#define NLE_MONITOR_MASK                                  0x3fff
#define NLE_MONITOR_MASK_SFT                              (0x3fff << 0)

/* MT6358_AFE_CG_EN_MON */
#define CK_CG_EN_MON_SFT                                  0
#define CK_CG_EN_MON_MASK                                 0x3f
#define CK_CG_EN_MON_MASK_SFT                             (0x3f << 0)

/* MT6358_AFE_VOW_TOP */
#define PDN_VOW_SFT                                       15
#define PDN_VOW_MASK                                      0x1
#define PDN_VOW_MASK_SFT                                  (0x1 << 15)
#define VOW_1P6M_800K_SEL_SFT                             14
#define VOW_1P6M_800K_SEL_MASK                            0x1
#define VOW_1P6M_800K_SEL_MASK_SFT                        (0x1 << 14)
#define VOW_DIGMIC_ON_SFT                                 13
#define VOW_DIGMIC_ON_MASK                                0x1
#define VOW_DIGMIC_ON_MASK_SFT                            (0x1 << 13)
#define VOW_CK_DIV_RST_SFT                                12
#define VOW_CK_DIV_RST_MASK                               0x1
#define VOW_CK_DIV_RST_MASK_SFT                           (0x1 << 12)
#define VOW_ON_SFT                                        11
#define VOW_ON_MASK                                       0x1
#define VOW_ON_MASK_SFT                                   (0x1 << 11)
#define VOW_DIGMIC_CK_PHASE_SEL_SFT                       8
#define VOW_DIGMIC_CK_PHASE_SEL_MASK                      0x7
#define VOW_DIGMIC_CK_PHASE_SEL_MASK_SFT                  (0x7 << 8)
#define MAIN_DMIC_CK_VOW_SEL_SFT                          7
#define MAIN_DMIC_CK_VOW_SEL_MASK                         0x1
#define MAIN_DMIC_CK_VOW_SEL_MASK_SFT                     (0x1 << 7)
#define VOW_SDM_3_LEVEL_SFT                               6
#define VOW_SDM_3_LEVEL_MASK                              0x1
#define VOW_SDM_3_LEVEL_MASK_SFT                          (0x1 << 6)
#define VOW_LOOP_BACK_MODE_SFT                            5
#define VOW_LOOP_BACK_MODE_MASK                           0x1
#define VOW_LOOP_BACK_MODE_MASK_SFT                       (0x1 << 5)
#define VOW_INTR_SOURCE_SEL_SFT                           4
#define VOW_INTR_SOURCE_SEL_MASK                          0x1
#define VOW_INTR_SOURCE_SEL_MASK_SFT                      (0x1 << 4)
#define VOW_INTR_CLR_SFT                                  3
#define VOW_INTR_CLR_MASK                                 0x1
#define VOW_INTR_CLR_MASK_SFT                             (0x1 << 3)
#define S_N_VALUE_RST_SFT                                 2
#define S_N_VALUE_RST_MASK                                0x1
#define S_N_VALUE_RST_MASK_SFT                            (0x1 << 2)
#define SAMPLE_BASE_MODE_SFT                              1
#define SAMPLE_BASE_MODE_MASK                             0x1
#define SAMPLE_BASE_MODE_MASK_SFT                         (0x1 << 1)
#define VOW_INTR_FLAG_SFT                                 0
#define VOW_INTR_FLAG_MASK                                0x1
#define VOW_INTR_FLAG_MASK_SFT                            (0x1 << 0)

/* MT6358_AFE_VOW_CFG0 */
#define AMPREF_SFT                                        0
#define AMPREF_MASK                                       0xffff
#define AMPREF_MASK_SFT                                   (0xffff << 0)

/* MT6358_AFE_VOW_CFG1 */
#define TIMERINI_SFT                                      0
#define TIMERINI_MASK                                     0xffff
#define TIMERINI_MASK_SFT                                 (0xffff << 0)

/* MT6358_AFE_VOW_CFG2 */
#define B_DEFAULT_SFT                                     12
#define B_DEFAULT_MASK                                    0x7
#define B_DEFAULT_MASK_SFT                                (0x7 << 12)
#define A_DEFAULT_SFT                                     8
#define A_DEFAULT_MASK                                    0x7
#define A_DEFAULT_MASK_SFT                                (0x7 << 8)
#define B_INI_SFT                                         4
#define B_INI_MASK                                        0x7
#define B_INI_MASK_SFT                                    (0x7 << 4)
#define A_INI_SFT                                         0
#define A_INI_MASK                                        0x7
#define A_INI_MASK_SFT                                    (0x7 << 0)

/* MT6358_AFE_VOW_CFG3 */
#define K_BETA_RISE_SFT                                   12
#define K_BETA_RISE_MASK                                  0xf
#define K_BETA_RISE_MASK_SFT                              (0xf << 12)
#define K_BETA_FALL_SFT                                   8
#define K_BETA_FALL_MASK                                  0xf
#define K_BETA_FALL_MASK_SFT                              (0xf << 8)
#define K_ALPHA_RISE_SFT                                  4
#define K_ALPHA_RISE_MASK                                 0xf
#define K_ALPHA_RISE_MASK_SFT                             (0xf << 4)
#define K_ALPHA_FALL_SFT                                  0
#define K_ALPHA_FALL_MASK                                 0xf
#define K_ALPHA_FALL_MASK_SFT                             (0xf << 0)

/* MT6358_AFE_VOW_CFG4 */
#define VOW_TXIF_SCK_INV_SFT                              15
#define VOW_TXIF_SCK_INV_MASK                             0x1
#define VOW_TXIF_SCK_INV_MASK_SFT                         (0x1 << 15)
#define VOW_ADC_TESTCK_SRC_SEL_SFT                        12
#define VOW_ADC_TESTCK_SRC_SEL_MASK                       0x7
#define VOW_ADC_TESTCK_SRC_SEL_MASK_SFT                   (0x7 << 12)
#define VOW_ADC_TESTCK_SEL_SFT                            11
#define VOW_ADC_TESTCK_SEL_MASK                           0x1
#define VOW_ADC_TESTCK_SEL_MASK_SFT                       (0x1 << 11)
#define VOW_ADC_CLK_INV_SFT                               10
#define VOW_ADC_CLK_INV_MASK                              0x1
#define VOW_ADC_CLK_INV_MASK_SFT                          (0x1 << 10)
#define VOW_TXIF_MONO_SFT                                 9
#define VOW_TXIF_MONO_MASK                                0x1
#define VOW_TXIF_MONO_MASK_SFT                            (0x1 << 9)
#define VOW_TXIF_SCK_DIV_SFT                              4
#define VOW_TXIF_SCK_DIV_MASK                             0x1f
#define VOW_TXIF_SCK_DIV_MASK_SFT                         (0x1f << 4)
#define K_GAMMA_SFT                                       0
#define K_GAMMA_MASK                                      0xf
#define K_GAMMA_MASK_SFT                                  (0xf << 0)

/* MT6358_AFE_VOW_CFG5 */
#define N_MIN_SFT                                         0
#define N_MIN_MASK                                        0xffff
#define N_MIN_MASK_SFT                                    (0xffff << 0)

/* MT6358_AFE_VOW_CFG6 */
#define RG_WINDOW_SIZE_SEL_SFT                            12
#define RG_WINDOW_SIZE_SEL_MASK                           0x1
#define RG_WINDOW_SIZE_SEL_MASK_SFT                       (0x1 << 12)
#define RG_FLR_BYPASS_SFT                                 11
#define RG_FLR_BYPASS_MASK                                0x1
#define RG_FLR_BYPASS_MASK_SFT                            (0x1 << 11)
#define RG_FLR_RATIO_SFT                                  8
#define RG_FLR_RATIO_MASK                                 0x7
#define RG_FLR_RATIO_MASK_SFT                             (0x7 << 8)
#define RG_BUCK_DVFS_DONE_SW_CTL_SFT                      7
#define RG_BUCK_DVFS_DONE_SW_CTL_MASK                     0x1
#define RG_BUCK_DVFS_DONE_SW_CTL_MASK_SFT                 (0x1 << 7)
#define RG_BUCK_DVFS_DONE_HW_MODE_SFT                     6
#define RG_BUCK_DVFS_DONE_HW_MODE_MASK                    0x1
#define RG_BUCK_DVFS_DONE_HW_MODE_MASK_SFT                (0x1 << 6)
#define RG_BUCK_DVFS_HW_CNT_THR_SFT                       0
#define RG_BUCK_DVFS_HW_CNT_THR_MASK                      0x3f
#define RG_BUCK_DVFS_HW_CNT_THR_MASK_SFT                  (0x3f << 0)

/* MT6358_AFE_VOW_MON0 */
#define VOW_DOWNCNT_SFT                                   0
#define VOW_DOWNCNT_MASK                                  0xffff
#define VOW_DOWNCNT_MASK_SFT                              (0xffff << 0)

/* MT6358_AFE_VOW_MON1 */
#define K_TMP_MON_SFT                                     10
#define K_TMP_MON_MASK                                    0xf
#define K_TMP_MON_MASK_SFT                                (0xf << 10)
#define SLT_COUNTER_MON_SFT                               7
#define SLT_COUNTER_MON_MASK                              0x7
#define SLT_COUNTER_MON_MASK_SFT                          (0x7 << 7)
#define VOW_B_SFT                                         4
#define VOW_B_MASK                                        0x7
#define VOW_B_MASK_SFT                                    (0x7 << 4)
#define VOW_A_SFT                                         1
#define VOW_A_MASK                                        0x7
#define VOW_A_MASK_SFT                                    (0x7 << 1)
#define SECOND_CNT_START_SFT                              0
#define SECOND_CNT_START_MASK                             0x1
#define SECOND_CNT_START_MASK_SFT                         (0x1 << 0)

/* MT6358_AFE_VOW_MON2 */
#define VOW_S_L_SFT                                       0
#define VOW_S_L_MASK                                      0xffff
#define VOW_S_L_MASK_SFT                                  (0xffff << 0)

/* MT6358_AFE_VOW_MON3 */
#define VOW_S_H_SFT                                       0
#define VOW_S_H_MASK                                      0xffff
#define VOW_S_H_MASK_SFT                                  (0xffff << 0)

/* MT6358_AFE_VOW_MON4 */
#define VOW_N_L_SFT                                       0
#define VOW_N_L_MASK                                      0xffff
#define VOW_N_L_MASK_SFT                                  (0xffff << 0)

/* MT6358_AFE_VOW_MON5 */
#define VOW_N_H_SFT                                       0
#define VOW_N_H_MASK                                      0xffff
#define VOW_N_H_MASK_SFT                                  (0xffff << 0)

/* MT6358_AFE_VOW_SN_INI_CFG */
#define VOW_SN_INI_CFG_EN_SFT                             15
#define VOW_SN_INI_CFG_EN_MASK                            0x1
#define VOW_SN_INI_CFG_EN_MASK_SFT                        (0x1 << 15)
#define VOW_SN_INI_CFG_VAL_SFT                            0
#define VOW_SN_INI_CFG_VAL_MASK                           0x7fff
#define VOW_SN_INI_CFG_VAL_MASK_SFT                       (0x7fff << 0)

/* MT6358_AFE_VOW_TGEN_CFG0 */
#define VOW_TGEN_EN_SFT                                   15
#define VOW_TGEN_EN_MASK                                  0x1
#define VOW_TGEN_EN_MASK_SFT                              (0x1 << 15)
#define VOW_TGEN_MUTE_SW_SFT                              14
#define VOW_TGEN_MUTE_SW_MASK                             0x1
#define VOW_TGEN_MUTE_SW_MASK_SFT                         (0x1 << 14)
#define VOW_TGEN_FREQ_DIV_SFT                             0
#define VOW_TGEN_FREQ_DIV_MASK                            0x3fff
#define VOW_TGEN_FREQ_DIV_MASK_SFT                        (0x3fff << 0)

/* MT6358_AFE_VOW_POSDIV_CFG0 */
#define BUCK_DVFS_DONE_SFT                                15
#define BUCK_DVFS_DONE_MASK                               0x1
#define BUCK_DVFS_DONE_MASK_SFT                           (0x1 << 15)
#define VOW_32K_MODE_SFT                                  13
#define VOW_32K_MODE_MASK                                 0x1
#define VOW_32K_MODE_MASK_SFT                             (0x1 << 13)
#define RG_BUCK_CLK_DIV_SFT                               8
#define RG_BUCK_CLK_DIV_MASK                              0x1f
#define RG_BUCK_CLK_DIV_MASK_SFT                          (0x1f << 8)
#define RG_A1P6M_EN_SEL_SFT                               7
#define RG_A1P6M_EN_SEL_MASK                              0x1
#define RG_A1P6M_EN_SEL_MASK_SFT                          (0x1 << 7)
#define VOW_CLK_SEL_SFT                                   6
#define VOW_CLK_SEL_MASK                                  0x1
#define VOW_CLK_SEL_MASK_SFT                              (0x1 << 6)
#define VOW_INTR_SW_MODE_SFT                              5
#define VOW_INTR_SW_MODE_MASK                             0x1
#define VOW_INTR_SW_MODE_MASK_SFT                         (0x1 << 5)
#define VOW_INTR_SW_VAL_SFT                               4
#define VOW_INTR_SW_VAL_MASK                              0x1
#define VOW_INTR_SW_VAL_MASK_SFT                          (0x1 << 4)
#define VOW_CIC_MODE_SEL_SFT                              2
#define VOW_CIC_MODE_SEL_MASK                             0x3
#define VOW_CIC_MODE_SEL_MASK_SFT                         (0x3 << 2)
#define RG_VOW_POSDIV_SFT                                 0
#define RG_VOW_POSDIV_MASK                                0x3
#define RG_VOW_POSDIV_MASK_SFT                            (0x3 << 0)

/* MT6358_AFE_VOW_HPF_CFG0 */
#define VOW_HPF_DC_TEST_SFT                               12
#define VOW_HPF_DC_TEST_MASK                              0xf
#define VOW_HPF_DC_TEST_MASK_SFT                          (0xf << 12)
#define VOW_IRQ_LATCH_SNR_EN_SFT                          10
#define VOW_IRQ_LATCH_SNR_EN_MASK                         0x1
#define VOW_IRQ_LATCH_SNR_EN_MASK_SFT                     (0x1 << 10)
#define VOW_DMICCLK_PDN_SFT                               9
#define VOW_DMICCLK_PDN_MASK                              0x1
#define VOW_DMICCLK_PDN_MASK_SFT                          (0x1 << 9)
#define VOW_POSDIVCLK_PDN_SFT                             8
#define VOW_POSDIVCLK_PDN_MASK                            0x1
#define VOW_POSDIVCLK_PDN_MASK_SFT                        (0x1 << 8)
#define RG_BASELINE_ALPHA_ORDER_SFT                       4
#define RG_BASELINE_ALPHA_ORDER_MASK                      0xf
#define RG_BASELINE_ALPHA_ORDER_MASK_SFT                  (0xf << 4)
#define RG_MTKAIF_HPF_BYPASS_SFT                          2
#define RG_MTKAIF_HPF_BYPASS_MASK                         0x1
#define RG_MTKAIF_HPF_BYPASS_MASK_SFT                     (0x1 << 2)
#define RG_SNRDET_HPF_BYPASS_SFT                          1
#define RG_SNRDET_HPF_BYPASS_MASK                         0x1
#define RG_SNRDET_HPF_BYPASS_MASK_SFT                     (0x1 << 1)
#define RG_HPF_ON_SFT                                     0
#define RG_HPF_ON_MASK                                    0x1
#define RG_HPF_ON_MASK_SFT                                (0x1 << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG0 */
#define RG_PERIODIC_EN_SFT                                15
#define RG_PERIODIC_EN_MASK                               0x1
#define RG_PERIODIC_EN_MASK_SFT                           (0x1 << 15)
#define RG_PERIODIC_CNT_CLR_SFT                           14
#define RG_PERIODIC_CNT_CLR_MASK                          0x1
#define RG_PERIODIC_CNT_CLR_MASK_SFT                      (0x1 << 14)
#define RG_PERIODIC_CNT_PERIOD_SFT                        0
#define RG_PERIODIC_CNT_PERIOD_MASK                       0x3fff
#define RG_PERIODIC_CNT_PERIOD_MASK_SFT                   (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG1 */
#define RG_PERIODIC_CNT_SET_SFT                           15
#define RG_PERIODIC_CNT_SET_MASK                          0x1
#define RG_PERIODIC_CNT_SET_MASK_SFT                      (0x1 << 15)
#define RG_PERIODIC_CNT_PAUSE_SFT                         14
#define RG_PERIODIC_CNT_PAUSE_MASK                        0x1
#define RG_PERIODIC_CNT_PAUSE_MASK_SFT                    (0x1 << 14)
#define RG_PERIODIC_CNT_SET_VALUE_SFT                     0
#define RG_PERIODIC_CNT_SET_VALUE_MASK                    0x3fff
#define RG_PERIODIC_CNT_SET_VALUE_MASK_SFT                (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG2 */
#define AUDPREAMPLON_PERIODIC_MODE_SFT                    15
#define AUDPREAMPLON_PERIODIC_MODE_MASK                   0x1
#define AUDPREAMPLON_PERIODIC_MODE_MASK_SFT               (0x1 << 15)
#define AUDPREAMPLON_PERIODIC_INVERSE_SFT                 14
#define AUDPREAMPLON_PERIODIC_INVERSE_MASK                0x1
#define AUDPREAMPLON_PERIODIC_INVERSE_MASK_SFT            (0x1 << 14)
#define AUDPREAMPLON_PERIODIC_ON_CYCLE_SFT                0
#define AUDPREAMPLON_PERIODIC_ON_CYCLE_MASK               0x3fff
#define AUDPREAMPLON_PERIODIC_ON_CYCLE_MASK_SFT           (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG3 */
#define AUDPREAMPLDCPRECHARGE_PERIODIC_MODE_SFT           15
#define AUDPREAMPLDCPRECHARGE_PERIODIC_MODE_MASK          0x1
#define AUDPREAMPLDCPRECHARGE_PERIODIC_MODE_MASK_SFT      (0x1 << 15)
#define AUDPREAMPLDCPRECHARGE_PERIODIC_INVERSE_SFT        14
#define AUDPREAMPLDCPRECHARGE_PERIODIC_INVERSE_MASK       0x1
#define AUDPREAMPLDCPRECHARGE_PERIODIC_INVERSE_MASK_SFT   (0x1 << 14)
#define AUDPREAMPLDCPRECHARGE_PERIODIC_ON_CYCLE_SFT       0
#define AUDPREAMPLDCPRECHARGE_PERIODIC_ON_CYCLE_MASK      0x3fff
#define AUDPREAMPLDCPRECHARGE_PERIODIC_ON_CYCLE_MASK_SFT  (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG4 */
#define AUDADCLPWRUP_PERIODIC_MODE_SFT                    15
#define AUDADCLPWRUP_PERIODIC_MODE_MASK                   0x1
#define AUDADCLPWRUP_PERIODIC_MODE_MASK_SFT               (0x1 << 15)
#define AUDADCLPWRUP_PERIODIC_INVERSE_SFT                 14
#define AUDADCLPWRUP_PERIODIC_INVERSE_MASK                0x1
#define AUDADCLPWRUP_PERIODIC_INVERSE_MASK_SFT            (0x1 << 14)
#define AUDADCLPWRUP_PERIODIC_ON_CYCLE_SFT                0
#define AUDADCLPWRUP_PERIODIC_ON_CYCLE_MASK               0x3fff
#define AUDADCLPWRUP_PERIODIC_ON_CYCLE_MASK_SFT           (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG5 */
#define AUDGLBVOWLPWEN_PERIODIC_MODE_SFT                  15
#define AUDGLBVOWLPWEN_PERIODIC_MODE_MASK                 0x1
#define AUDGLBVOWLPWEN_PERIODIC_MODE_MASK_SFT             (0x1 << 15)
#define AUDGLBVOWLPWEN_PERIODIC_INVERSE_SFT               14
#define AUDGLBVOWLPWEN_PERIODIC_INVERSE_MASK              0x1
#define AUDGLBVOWLPWEN_PERIODIC_INVERSE_MASK_SFT          (0x1 << 14)
#define AUDGLBVOWLPWEN_PERIODIC_ON_CYCLE_SFT              0
#define AUDGLBVOWLPWEN_PERIODIC_ON_CYCLE_MASK             0x3fff
#define AUDGLBVOWLPWEN_PERIODIC_ON_CYCLE_MASK_SFT         (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG6 */
#define AUDDIGMICEN_PERIODIC_MODE_SFT                     15
#define AUDDIGMICEN_PERIODIC_MODE_MASK                    0x1
#define AUDDIGMICEN_PERIODIC_MODE_MASK_SFT                (0x1 << 15)
#define AUDDIGMICEN_PERIODIC_INVERSE_SFT                  14
#define AUDDIGMICEN_PERIODIC_INVERSE_MASK                 0x1
#define AUDDIGMICEN_PERIODIC_INVERSE_MASK_SFT             (0x1 << 14)
#define AUDDIGMICEN_PERIODIC_ON_CYCLE_SFT                 0
#define AUDDIGMICEN_PERIODIC_ON_CYCLE_MASK                0x3fff
#define AUDDIGMICEN_PERIODIC_ON_CYCLE_MASK_SFT            (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG7 */
#define AUDPWDBMICBIAS0_PERIODIC_MODE_SFT                 15
#define AUDPWDBMICBIAS0_PERIODIC_MODE_MASK                0x1
#define AUDPWDBMICBIAS0_PERIODIC_MODE_MASK_SFT            (0x1 << 15)
#define AUDPWDBMICBIAS0_PERIODIC_INVERSE_SFT              14
#define AUDPWDBMICBIAS0_PERIODIC_INVERSE_MASK             0x1
#define AUDPWDBMICBIAS0_PERIODIC_INVERSE_MASK_SFT         (0x1 << 14)
#define AUDPWDBMICBIAS0_PERIODIC_ON_CYCLE_SFT             0
#define AUDPWDBMICBIAS0_PERIODIC_ON_CYCLE_MASK            0x3fff
#define AUDPWDBMICBIAS0_PERIODIC_ON_CYCLE_MASK_SFT        (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG8 */
#define AUDPWDBMICBIAS1_PERIODIC_MODE_SFT                 15
#define AUDPWDBMICBIAS1_PERIODIC_MODE_MASK                0x1
#define AUDPWDBMICBIAS1_PERIODIC_MODE_MASK_SFT            (0x1 << 15)
#define AUDPWDBMICBIAS1_PERIODIC_INVERSE_SFT              14
#define AUDPWDBMICBIAS1_PERIODIC_INVERSE_MASK             0x1
#define AUDPWDBMICBIAS1_PERIODIC_INVERSE_MASK_SFT         (0x1 << 14)
#define AUDPWDBMICBIAS1_PERIODIC_ON_CYCLE_SFT             0
#define AUDPWDBMICBIAS1_PERIODIC_ON_CYCLE_MASK            0x3fff
#define AUDPWDBMICBIAS1_PERIODIC_ON_CYCLE_MASK_SFT        (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG9 */
#define XO_VOW_CK_EN_PERIODIC_MODE_SFT                    15
#define XO_VOW_CK_EN_PERIODIC_MODE_MASK                   0x1
#define XO_VOW_CK_EN_PERIODIC_MODE_MASK_SFT               (0x1 << 15)
#define XO_VOW_CK_EN_PERIODIC_INVERSE_SFT                 14
#define XO_VOW_CK_EN_PERIODIC_INVERSE_MASK                0x1
#define XO_VOW_CK_EN_PERIODIC_INVERSE_MASK_SFT            (0x1 << 14)
#define XO_VOW_CK_EN_PERIODIC_ON_CYCLE_SFT                0
#define XO_VOW_CK_EN_PERIODIC_ON_CYCLE_MASK               0x3fff
#define XO_VOW_CK_EN_PERIODIC_ON_CYCLE_MASK_SFT           (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG10 */
#define AUDGLB_PWRDN_PERIODIC_MODE_SFT                    15
#define AUDGLB_PWRDN_PERIODIC_MODE_MASK                   0x1
#define AUDGLB_PWRDN_PERIODIC_MODE_MASK_SFT               (0x1 << 15)
#define AUDGLB_PWRDN_PERIODIC_INVERSE_SFT                 14
#define AUDGLB_PWRDN_PERIODIC_INVERSE_MASK                0x1
#define AUDGLB_PWRDN_PERIODIC_INVERSE_MASK_SFT            (0x1 << 14)
#define AUDGLB_PWRDN_PERIODIC_ON_CYCLE_SFT                0
#define AUDGLB_PWRDN_PERIODIC_ON_CYCLE_MASK               0x3fff
#define AUDGLB_PWRDN_PERIODIC_ON_CYCLE_MASK_SFT           (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG11 */
#define VOW_ON_PERIODIC_MODE_SFT                          15
#define VOW_ON_PERIODIC_MODE_MASK                         0x1
#define VOW_ON_PERIODIC_MODE_MASK_SFT                     (0x1 << 15)
#define VOW_ON_PERIODIC_INVERSE_SFT                       14
#define VOW_ON_PERIODIC_INVERSE_MASK                      0x1
#define VOW_ON_PERIODIC_INVERSE_MASK_SFT                  (0x1 << 14)
#define VOW_ON_PERIODIC_ON_CYCLE_SFT                      0
#define VOW_ON_PERIODIC_ON_CYCLE_MASK                     0x3fff
#define VOW_ON_PERIODIC_ON_CYCLE_MASK_SFT                 (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG12 */
#define DMIC_ON_PERIODIC_MODE_SFT                         15
#define DMIC_ON_PERIODIC_MODE_MASK                        0x1
#define DMIC_ON_PERIODIC_MODE_MASK_SFT                    (0x1 << 15)
#define DMIC_ON_PERIODIC_INVERSE_SFT                      14
#define DMIC_ON_PERIODIC_INVERSE_MASK                     0x1
#define DMIC_ON_PERIODIC_INVERSE_MASK_SFT                 (0x1 << 14)
#define DMIC_ON_PERIODIC_ON_CYCLE_SFT                     0
#define DMIC_ON_PERIODIC_ON_CYCLE_MASK                    0x3fff
#define DMIC_ON_PERIODIC_ON_CYCLE_MASK_SFT                (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG13 */
#define PDN_VOW_F32K_CK_SFT                               15
#define PDN_VOW_F32K_CK_MASK                              0x1
#define PDN_VOW_F32K_CK_MASK_SFT                          (0x1 << 15)
#define AUDPREAMPLON_PERIODIC_OFF_CYCLE_SFT               0
#define AUDPREAMPLON_PERIODIC_OFF_CYCLE_MASK              0x3fff
#define AUDPREAMPLON_PERIODIC_OFF_CYCLE_MASK_SFT          (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG14 */
#define VOW_SNRDET_PERIODIC_CFG_SFT                       15
#define VOW_SNRDET_PERIODIC_CFG_MASK                      0x1
#define VOW_SNRDET_PERIODIC_CFG_MASK_SFT                  (0x1 << 15)
#define AUDPREAMPLDCPRECHARGE_PERIODIC_OFF_CYCLE_SFT      0
#define AUDPREAMPLDCPRECHARGE_PERIODIC_OFF_CYCLE_MASK     0x3fff
#define AUDPREAMPLDCPRECHARGE_PERIODIC_OFF_CYCLE_MASK_SFT (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG15 */
#define AUDADCLPWRUP_PERIODIC_OFF_CYCLE_SFT               0
#define AUDADCLPWRUP_PERIODIC_OFF_CYCLE_MASK              0x3fff
#define AUDADCLPWRUP_PERIODIC_OFF_CYCLE_MASK_SFT          (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG16 */
#define AUDGLBVOWLPWEN_PERIODIC_OFF_CYCLE_SFT             0
#define AUDGLBVOWLPWEN_PERIODIC_OFF_CYCLE_MASK            0x3fff
#define AUDGLBVOWLPWEN_PERIODIC_OFF_CYCLE_MASK_SFT        (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG17 */
#define AUDDIGMICEN_PERIODIC_OFF_CYCLE_SFT                0
#define AUDDIGMICEN_PERIODIC_OFF_CYCLE_MASK               0x3fff
#define AUDDIGMICEN_PERIODIC_OFF_CYCLE_MASK_SFT           (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG18 */
#define AUDPWDBMICBIAS0_PERIODIC_OFF_CYCLE_SFT            0
#define AUDPWDBMICBIAS0_PERIODIC_OFF_CYCLE_MASK           0x3fff
#define AUDPWDBMICBIAS0_PERIODIC_OFF_CYCLE_MASK_SFT       (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG19 */
#define AUDPWDBMICBIAS1_PERIODIC_OFF_CYCLE_SFT            0
#define AUDPWDBMICBIAS1_PERIODIC_OFF_CYCLE_MASK           0x3fff
#define AUDPWDBMICBIAS1_PERIODIC_OFF_CYCLE_MASK_SFT       (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG20 */
#define CLKSQ_EN_VOW_PERIODIC_MODE_SFT                    15
#define CLKSQ_EN_VOW_PERIODIC_MODE_MASK                   0x1
#define CLKSQ_EN_VOW_PERIODIC_MODE_MASK_SFT               (0x1 << 15)
#define XO_VOW_CK_EN_PERIODIC_OFF_CYCLE_SFT               0
#define XO_VOW_CK_EN_PERIODIC_OFF_CYCLE_MASK              0x3fff
#define XO_VOW_CK_EN_PERIODIC_OFF_CYCLE_MASK_SFT          (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG21 */
#define AUDGLB_PWRDN_PERIODIC_OFF_CYCLE_SFT               0
#define AUDGLB_PWRDN_PERIODIC_OFF_CYCLE_MASK              0x3fff
#define AUDGLB_PWRDN_PERIODIC_OFF_CYCLE_MASK_SFT          (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG22 */
#define VOW_ON_PERIODIC_OFF_CYCLE_SFT                     0
#define VOW_ON_PERIODIC_OFF_CYCLE_MASK                    0x3fff
#define VOW_ON_PERIODIC_OFF_CYCLE_MASK_SFT                (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_CFG23 */
#define DMIC_ON_PERIODIC_OFF_CYCLE_SFT                    0
#define DMIC_ON_PERIODIC_OFF_CYCLE_MASK                   0x3fff
#define DMIC_ON_PERIODIC_OFF_CYCLE_MASK_SFT               (0x3fff << 0)

/* MT6358_AFE_VOW_PERIODIC_MON0 */
#define VOW_PERIODIC_MON_SFT                              0
#define VOW_PERIODIC_MON_MASK                             0xffff
#define VOW_PERIODIC_MON_MASK_SFT                         (0xffff << 0)

/* MT6358_AFE_VOW_PERIODIC_MON1 */
#define VOW_PERIODIC_COUNT_MON_SFT                        0
#define VOW_PERIODIC_COUNT_MON_MASK                       0xffff
#define VOW_PERIODIC_COUNT_MON_MASK_SFT                   (0xffff << 0)

/* MT6358_AUDENC_DSN_ID */
#define AUDENC_ANA_ID_SFT                                 0
#define AUDENC_ANA_ID_MASK                                0xff
#define AUDENC_ANA_ID_MASK_SFT                            (0xff << 0)
#define AUDENC_DIG_ID_SFT                                 8
#define AUDENC_DIG_ID_MASK                                0xff
#define AUDENC_DIG_ID_MASK_SFT                            (0xff << 8)

/* MT6358_AUDENC_DSN_REV0 */
#define AUDENC_ANA_MINOR_REV_SFT                          0
#define AUDENC_ANA_MINOR_REV_MASK                         0xf
#define AUDENC_ANA_MINOR_REV_MASK_SFT                     (0xf << 0)
#define AUDENC_ANA_MAJOR_REV_SFT                          4
#define AUDENC_ANA_MAJOR_REV_MASK                         0xf
#define AUDENC_ANA_MAJOR_REV_MASK_SFT                     (0xf << 4)
#define AUDENC_DIG_MINOR_REV_SFT                          8
#define AUDENC_DIG_MINOR_REV_MASK                         0xf
#define AUDENC_DIG_MINOR_REV_MASK_SFT                     (0xf << 8)
#define AUDENC_DIG_MAJOR_REV_SFT                          12
#define AUDENC_DIG_MAJOR_REV_MASK                         0xf
#define AUDENC_DIG_MAJOR_REV_MASK_SFT                     (0xf << 12)

/* MT6358_AUDENC_DSN_DBI */
#define AUDENC_DSN_CBS_SFT                                0
#define AUDENC_DSN_CBS_MASK                               0x3
#define AUDENC_DSN_CBS_MASK_SFT                           (0x3 << 0)
#define AUDENC_DSN_BIX_SFT                                2
#define AUDENC_DSN_BIX_MASK                               0x3
#define AUDENC_DSN_BIX_MASK_SFT                           (0x3 << 2)
#define AUDENC_DSN_ESP_SFT                                8
#define AUDENC_DSN_ESP_MASK                               0xff
#define AUDENC_DSN_ESP_MASK_SFT                           (0xff << 8)

/* MT6358_AUDENC_DSN_FPI */
#define AUDENC_DSN_FPI_SFT                                0
#define AUDENC_DSN_FPI_MASK                               0xff
#define AUDENC_DSN_FPI_MASK_SFT                           (0xff << 0)

/* MT6358_AUDENC_ANA_CON0 */
#define RG_AUDPREAMPLON_SFT                               0
#define RG_AUDPREAMPLON_MASK                              0x1
#define RG_AUDPREAMPLON_MASK_SFT                          (0x1 << 0)
#define RG_AUDPREAMPLDCCEN_SFT                            1
#define RG_AUDPREAMPLDCCEN_MASK                           0x1
#define RG_AUDPREAMPLDCCEN_MASK_SFT                       (0x1 << 1)
#define RG_AUDPREAMPLDCPRECHARGE_SFT                      2
#define RG_AUDPREAMPLDCPRECHARGE_MASK                     0x1
#define RG_AUDPREAMPLDCPRECHARGE_MASK_SFT                 (0x1 << 2)
#define RG_AUDPREAMPLPGATEST_SFT                          3
#define RG_AUDPREAMPLPGATEST_MASK                         0x1
#define RG_AUDPREAMPLPGATEST_MASK_SFT                     (0x1 << 3)
#define RG_AUDPREAMPLVSCALE_SFT                           4
#define RG_AUDPREAMPLVSCALE_MASK                          0x3
#define RG_AUDPREAMPLVSCALE_MASK_SFT                      (0x3 << 4)
#define RG_AUDPREAMPLINPUTSEL_SFT                         6
#define RG_AUDPREAMPLINPUTSEL_MASK                        0x3
#define RG_AUDPREAMPLINPUTSEL_MASK_SFT                    (0x3 << 6)
#define RG_AUDPREAMPLGAIN_SFT                             8
#define RG_AUDPREAMPLGAIN_MASK                            0x7
#define RG_AUDPREAMPLGAIN_MASK_SFT                        (0x7 << 8)
#define RG_AUDADCLPWRUP_SFT                               12
#define RG_AUDADCLPWRUP_MASK                              0x1
#define RG_AUDADCLPWRUP_MASK_SFT                          (0x1 << 12)
#define RG_AUDADCLINPUTSEL_SFT                            13
#define RG_AUDADCLINPUTSEL_MASK                           0x3
#define RG_AUDADCLINPUTSEL_MASK_SFT                       (0x3 << 13)

/* MT6358_AUDENC_ANA_CON1 */
#define RG_AUDPREAMPRON_SFT                               0
#define RG_AUDPREAMPRON_MASK                              0x1
#define RG_AUDPREAMPRON_MASK_SFT                          (0x1 << 0)
#define RG_AUDPREAMPRDCCEN_SFT                            1
#define RG_AUDPREAMPRDCCEN_MASK                           0x1
#define RG_AUDPREAMPRDCCEN_MASK_SFT                       (0x1 << 1)
#define RG_AUDPREAMPRDCPRECHARGE_SFT                      2
#define RG_AUDPREAMPRDCPRECHARGE_MASK                     0x1
#define RG_AUDPREAMPRDCPRECHARGE_MASK_SFT                 (0x1 << 2)
#define RG_AUDPREAMPRPGATEST_SFT                          3
#define RG_AUDPREAMPRPGATEST_MASK                         0x1
#define RG_AUDPREAMPRPGATEST_MASK_SFT                     (0x1 << 3)
#define RG_AUDPREAMPRVSCALE_SFT                           4
#define RG_AUDPREAMPRVSCALE_MASK                          0x3
#define RG_AUDPREAMPRVSCALE_MASK_SFT                      (0x3 << 4)
#define RG_AUDPREAMPRINPUTSEL_SFT                         6
#define RG_AUDPREAMPRINPUTSEL_MASK                        0x3
#define RG_AUDPREAMPRINPUTSEL_MASK_SFT                    (0x3 << 6)
#define RG_AUDPREAMPRGAIN_SFT                             8
#define RG_AUDPREAMPRGAIN_MASK                            0x7
#define RG_AUDPREAMPRGAIN_MASK_SFT                        (0x7 << 8)
#define RG_AUDIO_VOW_EN_SFT                               11
#define RG_AUDIO_VOW_EN_MASK                              0x1
#define RG_AUDIO_VOW_EN_MASK_SFT                          (0x1 << 11)
#define RG_AUDADCRPWRUP_SFT                               12
#define RG_AUDADCRPWRUP_MASK                              0x1
#define RG_AUDADCRPWRUP_MASK_SFT                          (0x1 << 12)
#define RG_AUDADCRINPUTSEL_SFT                            13
#define RG_AUDADCRINPUTSEL_MASK                           0x3
#define RG_AUDADCRINPUTSEL_MASK_SFT                       (0x3 << 13)
#define RG_CLKSQ_EN_VOW_SFT                               15
#define RG_CLKSQ_EN_VOW_MASK                              0x1
#define RG_CLKSQ_EN_VOW_MASK_SFT                          (0x1 << 15)

/* MT6358_AUDENC_ANA_CON2 */
#define RG_AUDULHALFBIAS_SFT                              0
#define RG_AUDULHALFBIAS_MASK                             0x1
#define RG_AUDULHALFBIAS_MASK_SFT                         (0x1 << 0)
#define RG_AUDGLBVOWLPWEN_SFT                             1
#define RG_AUDGLBVOWLPWEN_MASK                            0x1
#define RG_AUDGLBVOWLPWEN_MASK_SFT                        (0x1 << 1)
#define RG_AUDPREAMPLPEN_SFT                              2
#define RG_AUDPREAMPLPEN_MASK                             0x1
#define RG_AUDPREAMPLPEN_MASK_SFT                         (0x1 << 2)
#define RG_AUDADC1STSTAGELPEN_SFT                         3
#define RG_AUDADC1STSTAGELPEN_MASK                        0x1
#define RG_AUDADC1STSTAGELPEN_MASK_SFT                    (0x1 << 3)
#define RG_AUDADC2NDSTAGELPEN_SFT                         4
#define RG_AUDADC2NDSTAGELPEN_MASK                        0x1
#define RG_AUDADC2NDSTAGELPEN_MASK_SFT                    (0x1 << 4)
#define RG_AUDADCFLASHLPEN_SFT                            5
#define RG_AUDADCFLASHLPEN_MASK                           0x1
#define RG_AUDADCFLASHLPEN_MASK_SFT                       (0x1 << 5)
#define RG_AUDPREAMPIDDTEST_SFT                           6
#define RG_AUDPREAMPIDDTEST_MASK                          0x3
#define RG_AUDPREAMPIDDTEST_MASK_SFT                      (0x3 << 6)
#define RG_AUDADC1STSTAGEIDDTEST_SFT                      8
#define RG_AUDADC1STSTAGEIDDTEST_MASK                     0x3
#define RG_AUDADC1STSTAGEIDDTEST_MASK_SFT                 (0x3 << 8)
#define RG_AUDADC2NDSTAGEIDDTEST_SFT                      10
#define RG_AUDADC2NDSTAGEIDDTEST_MASK                     0x3
#define RG_AUDADC2NDSTAGEIDDTEST_MASK_SFT                 (0x3 << 10)
#define RG_AUDADCREFBUFIDDTEST_SFT                        12
#define RG_AUDADCREFBUFIDDTEST_MASK                       0x3
#define RG_AUDADCREFBUFIDDTEST_MASK_SFT                   (0x3 << 12)
#define RG_AUDADCFLASHIDDTEST_SFT                         14
#define RG_AUDADCFLASHIDDTEST_MASK                        0x3
#define RG_AUDADCFLASHIDDTEST_MASK_SFT                    (0x3 << 14)

/* MT6358_AUDENC_ANA_CON3 */
#define RG_AUDADCDAC0P25FS_SFT                            0
#define RG_AUDADCDAC0P25FS_MASK                           0x1
#define RG_AUDADCDAC0P25FS_MASK_SFT                       (0x1 << 0)
#define RG_AUDADCCLKSEL_SFT                               1
#define RG_AUDADCCLKSEL_MASK                              0x1
#define RG_AUDADCCLKSEL_MASK_SFT                          (0x1 << 1)
#define RG_AUDADCCLKSOURCE_SFT                            2
#define RG_AUDADCCLKSOURCE_MASK                           0x3
#define RG_AUDADCCLKSOURCE_MASK_SFT                       (0x3 << 2)
#define RG_AUDPREAMPAAFEN_SFT                             8
#define RG_AUDPREAMPAAFEN_MASK                            0x1
#define RG_AUDPREAMPAAFEN_MASK_SFT                        (0x1 << 8)
#define RG_DCCVCMBUFLPMODSEL_SFT                          9
#define RG_DCCVCMBUFLPMODSEL_MASK                         0x1
#define RG_DCCVCMBUFLPMODSEL_MASK_SFT                     (0x1 << 9)
#define RG_DCCVCMBUFLPSWEN_SFT                            10
#define RG_DCCVCMBUFLPSWEN_MASK                           0x1
#define RG_DCCVCMBUFLPSWEN_MASK_SFT                       (0x1 << 10)
#define RG_CMSTBENH_SFT                                   11
#define RG_CMSTBENH_MASK                                  0x1
#define RG_CMSTBENH_MASK_SFT                              (0x1 << 11)
#define RG_PGABODYSW_SFT                                  12
#define RG_PGABODYSW_MASK                                 0x1
#define RG_PGABODYSW_MASK_SFT                             (0x1 << 12)

/* MT6358_AUDENC_ANA_CON4 */
#define RG_AUDADC1STSTAGESDENB_SFT                        0
#define RG_AUDADC1STSTAGESDENB_MASK                       0x1
#define RG_AUDADC1STSTAGESDENB_MASK_SFT                   (0x1 << 0)
#define RG_AUDADC2NDSTAGERESET_SFT                        1
#define RG_AUDADC2NDSTAGERESET_MASK                       0x1
#define RG_AUDADC2NDSTAGERESET_MASK_SFT                   (0x1 << 1)
#define RG_AUDADC3RDSTAGERESET_SFT                        2
#define RG_AUDADC3RDSTAGERESET_MASK                       0x1
#define RG_AUDADC3RDSTAGERESET_MASK_SFT                   (0x1 << 2)
#define RG_AUDADCFSRESET_SFT                              3
#define RG_AUDADCFSRESET_MASK                             0x1
#define RG_AUDADCFSRESET_MASK_SFT                         (0x1 << 3)
#define RG_AUDADCWIDECM_SFT                               4
#define RG_AUDADCWIDECM_MASK                              0x1
#define RG_AUDADCWIDECM_MASK_SFT                          (0x1 << 4)
#define RG_AUDADCNOPATEST_SFT                             5
#define RG_AUDADCNOPATEST_MASK                            0x1
#define RG_AUDADCNOPATEST_MASK_SFT                        (0x1 << 5)
#define RG_AUDADCBYPASS_SFT                               6
#define RG_AUDADCBYPASS_MASK                              0x1
#define RG_AUDADCBYPASS_MASK_SFT                          (0x1 << 6)
#define RG_AUDADCFFBYPASS_SFT                             7
#define RG_AUDADCFFBYPASS_MASK                            0x1
#define RG_AUDADCFFBYPASS_MASK_SFT                        (0x1 << 7)
#define RG_AUDADCDACFBCURRENT_SFT                         8
#define RG_AUDADCDACFBCURRENT_MASK                        0x1
#define RG_AUDADCDACFBCURRENT_MASK_SFT                    (0x1 << 8)
#define RG_AUDADCDACIDDTEST_SFT                           9
#define RG_AUDADCDACIDDTEST_MASK                          0x3
#define RG_AUDADCDACIDDTEST_MASK_SFT                      (0x3 << 9)
#define RG_AUDADCDACNRZ_SFT                               11
#define RG_AUDADCDACNRZ_MASK                              0x1
#define RG_AUDADCDACNRZ_MASK_SFT                          (0x1 << 11)
#define RG_AUDADCNODEM_SFT                                12
#define RG_AUDADCNODEM_MASK                               0x1
#define RG_AUDADCNODEM_MASK_SFT                           (0x1 << 12)
#define RG_AUDADCDACTEST_SFT                              13
#define RG_AUDADCDACTEST_MASK                             0x1
#define RG_AUDADCDACTEST_MASK_SFT                         (0x1 << 13)

/* MT6358_AUDENC_ANA_CON5 */
#define RG_AUDRCTUNEL_SFT                                 0
#define RG_AUDRCTUNEL_MASK                                0x1f
#define RG_AUDRCTUNEL_MASK_SFT                            (0x1f << 0)
#define RG_AUDRCTUNELSEL_SFT                              5
#define RG_AUDRCTUNELSEL_MASK                             0x1
#define RG_AUDRCTUNELSEL_MASK_SFT                         (0x1 << 5)
#define RG_AUDRCTUNER_SFT                                 8
#define RG_AUDRCTUNER_MASK                                0x1f
#define RG_AUDRCTUNER_MASK_SFT                            (0x1f << 8)
#define RG_AUDRCTUNERSEL_SFT                              13
#define RG_AUDRCTUNERSEL_MASK                             0x1
#define RG_AUDRCTUNERSEL_MASK_SFT                         (0x1 << 13)

/* MT6358_AUDENC_ANA_CON6 */
#define RG_CLKSQ_EN_SFT                                   0
#define RG_CLKSQ_EN_MASK                                  0x1
#define RG_CLKSQ_EN_MASK_SFT                              (0x1 << 0)
#define RG_CLKSQ_IN_SEL_TEST_SFT                          1
#define RG_CLKSQ_IN_SEL_TEST_MASK                         0x1
#define RG_CLKSQ_IN_SEL_TEST_MASK_SFT                     (0x1 << 1)
#define RG_CM_REFGENSEL_SFT                               2
#define RG_CM_REFGENSEL_MASK                              0x1
#define RG_CM_REFGENSEL_MASK_SFT                          (0x1 << 2)
#define RG_AUDSPARE_SFT                                   4
#define RG_AUDSPARE_MASK                                  0xf
#define RG_AUDSPARE_MASK_SFT                              (0xf << 4)
#define RG_AUDENCSPARE_SFT                                8
#define RG_AUDENCSPARE_MASK                               0x3f
#define RG_AUDENCSPARE_MASK_SFT                           (0x3f << 8)

/* MT6358_AUDENC_ANA_CON7 */
#define RG_AUDENCSPARE2_SFT                               0
#define RG_AUDENCSPARE2_MASK                              0xff
#define RG_AUDENCSPARE2_MASK_SFT                          (0xff << 0)

/* MT6358_AUDENC_ANA_CON8 */
#define RG_AUDDIGMICEN_SFT                                0
#define RG_AUDDIGMICEN_MASK                               0x1
#define RG_AUDDIGMICEN_MASK_SFT                           (0x1 << 0)
#define RG_AUDDIGMICBIAS_SFT                              1
#define RG_AUDDIGMICBIAS_MASK                             0x3
#define RG_AUDDIGMICBIAS_MASK_SFT                         (0x3 << 1)
#define RG_DMICHPCLKEN_SFT                                3
#define RG_DMICHPCLKEN_MASK                               0x1
#define RG_DMICHPCLKEN_MASK_SFT                           (0x1 << 3)
#define RG_AUDDIGMICPDUTY_SFT                             4
#define RG_AUDDIGMICPDUTY_MASK                            0x3
#define RG_AUDDIGMICPDUTY_MASK_SFT                        (0x3 << 4)
#define RG_AUDDIGMICNDUTY_SFT                             6
#define RG_AUDDIGMICNDUTY_MASK                            0x3
#define RG_AUDDIGMICNDUTY_MASK_SFT                        (0x3 << 6)
#define RG_DMICMONEN_SFT                                  8
#define RG_DMICMONEN_MASK                                 0x1
#define RG_DMICMONEN_MASK_SFT                             (0x1 << 8)
#define RG_DMICMONSEL_SFT                                 9
#define RG_DMICMONSEL_MASK                                0x7
#define RG_DMICMONSEL_MASK_SFT                            (0x7 << 9)
#define RG_AUDSPAREVMIC_SFT                               12
#define RG_AUDSPAREVMIC_MASK                              0xf
#define RG_AUDSPAREVMIC_MASK_SFT                          (0xf << 12)

/* MT6358_AUDENC_ANA_CON9 */
#define RG_AUDPWDBMICBIAS0_SFT                            0
#define RG_AUDPWDBMICBIAS0_MASK                           0x1
#define RG_AUDPWDBMICBIAS0_MASK_SFT                       (0x1 << 0)
#define RG_AUDMICBIAS0BYPASSEN_SFT                        1
#define RG_AUDMICBIAS0BYPASSEN_MASK                       0x1
#define RG_AUDMICBIAS0BYPASSEN_MASK_SFT                   (0x1 << 1)
#define RG_AUDMICBIAS0LOWPEN_SFT                          2
#define RG_AUDMICBIAS0LOWPEN_MASK                         0x1
#define RG_AUDMICBIAS0LOWPEN_MASK_SFT                     (0x1 << 2)
#define RG_AUDMICBIAS0VREF_SFT                            4
#define RG_AUDMICBIAS0VREF_MASK                           0x7
#define RG_AUDMICBIAS0VREF_MASK_SFT                       (0x7 << 4)
#define RG_AUDMICBIAS0DCSW0P1EN_SFT                       8
#define RG_AUDMICBIAS0DCSW0P1EN_MASK                      0x1
#define RG_AUDMICBIAS0DCSW0P1EN_MASK_SFT                  (0x1 << 8)
#define RG_AUDMICBIAS0DCSW0P2EN_SFT                       9
#define RG_AUDMICBIAS0DCSW0P2EN_MASK                      0x1
#define RG_AUDMICBIAS0DCSW0P2EN_MASK_SFT                  (0x1 << 9)
#define RG_AUDMICBIAS0DCSW0NEN_SFT                        10
#define RG_AUDMICBIAS0DCSW0NEN_MASK                       0x1
#define RG_AUDMICBIAS0DCSW0NEN_MASK_SFT                   (0x1 << 10)
#define RG_AUDMICBIAS0DCSW2P1EN_SFT                       12
#define RG_AUDMICBIAS0DCSW2P1EN_MASK                      0x1
#define RG_AUDMICBIAS0DCSW2P1EN_MASK_SFT                  (0x1 << 12)
#define RG_AUDMICBIAS0DCSW2P2EN_SFT                       13
#define RG_AUDMICBIAS0DCSW2P2EN_MASK                      0x1
#define RG_AUDMICBIAS0DCSW2P2EN_MASK_SFT                  (0x1 << 13)
#define RG_AUDMICBIAS0DCSW2NEN_SFT                        14
#define RG_AUDMICBIAS0DCSW2NEN_MASK                       0x1
#define RG_AUDMICBIAS0DCSW2NEN_MASK_SFT                   (0x1 << 14)

/* MT6358_AUDENC_ANA_CON10 */
#define RG_AUDPWDBMICBIAS1_SFT                            0
#define RG_AUDPWDBMICBIAS1_MASK                           0x1
#define RG_AUDPWDBMICBIAS1_MASK_SFT                       (0x1 << 0)
#define RG_AUDMICBIAS1BYPASSEN_SFT                        1
#define RG_AUDMICBIAS1BYPASSEN_MASK                       0x1
#define RG_AUDMICBIAS1BYPASSEN_MASK_SFT                   (0x1 << 1)
#define RG_AUDMICBIAS1LOWPEN_SFT                          2
#define RG_AUDMICBIAS1LOWPEN_MASK                         0x1
#define RG_AUDMICBIAS1LOWPEN_MASK_SFT                     (0x1 << 2)
#define RG_AUDMICBIAS1VREF_SFT                            4
#define RG_AUDMICBIAS1VREF_MASK                           0x7
#define RG_AUDMICBIAS1VREF_MASK_SFT                       (0x7 << 4)
#define RG_AUDMICBIAS1DCSW1PEN_SFT                        8
#define RG_AUDMICBIAS1DCSW1PEN_MASK                       0x1
#define RG_AUDMICBIAS1DCSW1PEN_MASK_SFT                   (0x1 << 8)
#define RG_AUDMICBIAS1DCSW1NEN_SFT                        9
#define RG_AUDMICBIAS1DCSW1NEN_MASK                       0x1
#define RG_AUDMICBIAS1DCSW1NEN_MASK_SFT                   (0x1 << 9)
#define RG_BANDGAPGEN_SFT                                 12
#define RG_BANDGAPGEN_MASK                                0x1
#define RG_BANDGAPGEN_MASK_SFT                            (0x1 << 12)
#define RG_MTEST_EN_SFT                                   13
#define RG_MTEST_EN_MASK                                  0x1
#define RG_MTEST_EN_MASK_SFT                              (0x1 << 13)
#define RG_MTEST_SEL_SFT                                  14
#define RG_MTEST_SEL_MASK                                 0x1
#define RG_MTEST_SEL_MASK_SFT                             (0x1 << 14)
#define RG_MTEST_CURRENT_SFT                              15
#define RG_MTEST_CURRENT_MASK                             0x1
#define RG_MTEST_CURRENT_MASK_SFT                         (0x1 << 15)

/* MT6358_AUDENC_ANA_CON11 */
#define RG_AUDACCDETMICBIAS0PULLLOW_SFT                   0
#define RG_AUDACCDETMICBIAS0PULLLOW_MASK                  0x1
#define RG_AUDACCDETMICBIAS0PULLLOW_MASK_SFT              (0x1 << 0)
#define RG_AUDACCDETMICBIAS1PULLLOW_SFT                   1
#define RG_AUDACCDETMICBIAS1PULLLOW_MASK                  0x1
#define RG_AUDACCDETMICBIAS1PULLLOW_MASK_SFT              (0x1 << 1)
#define RG_AUDACCDETVIN1PULLLOW_SFT                       2
#define RG_AUDACCDETVIN1PULLLOW_MASK                      0x1
#define RG_AUDACCDETVIN1PULLLOW_MASK_SFT                  (0x1 << 2)
#define RG_AUDACCDETVTHACAL_SFT                           4
#define RG_AUDACCDETVTHACAL_MASK                          0x1
#define RG_AUDACCDETVTHACAL_MASK_SFT                      (0x1 << 4)
#define RG_AUDACCDETVTHBCAL_SFT                           5
#define RG_AUDACCDETVTHBCAL_MASK                          0x1
#define RG_AUDACCDETVTHBCAL_MASK_SFT                      (0x1 << 5)
#define RG_AUDACCDETTVDET_SFT                             6
#define RG_AUDACCDETTVDET_MASK                            0x1
#define RG_AUDACCDETTVDET_MASK_SFT                        (0x1 << 6)
#define RG_ACCDETSEL_SFT                                  7
#define RG_ACCDETSEL_MASK                                 0x1
#define RG_ACCDETSEL_MASK_SFT                             (0x1 << 7)
#define RG_SWBUFMODSEL_SFT                                8
#define RG_SWBUFMODSEL_MASK                               0x1
#define RG_SWBUFMODSEL_MASK_SFT                           (0x1 << 8)
#define RG_SWBUFSWEN_SFT                                  9
#define RG_SWBUFSWEN_MASK                                 0x1
#define RG_SWBUFSWEN_MASK_SFT                             (0x1 << 9)
#define RG_EINTCOMPVTH_SFT                                10
#define RG_EINTCOMPVTH_MASK                               0x1
#define RG_EINTCOMPVTH_MASK_SFT                           (0x1 << 10)
#define RG_EINTCONFIGACCDET_SFT                           11
#define RG_EINTCONFIGACCDET_MASK                          0x1
#define RG_EINTCONFIGACCDET_MASK_SFT                      (0x1 << 11)
#define RG_EINTHIRENB_SFT                                 12
#define RG_EINTHIRENB_MASK                                0x1
#define RG_EINTHIRENB_MASK_SFT                            (0x1 << 12)
#define RG_ACCDET2AUXRESBYPASS_SFT                        13
#define RG_ACCDET2AUXRESBYPASS_MASK                       0x1
#define RG_ACCDET2AUXRESBYPASS_MASK_SFT                   (0x1 << 13)
#define RG_ACCDET2AUXBUFFERBYPASS_SFT                     14
#define RG_ACCDET2AUXBUFFERBYPASS_MASK                    0x1
#define RG_ACCDET2AUXBUFFERBYPASS_MASK_SFT                (0x1 << 14)
#define RG_ACCDET2AUXSWEN_SFT                             15
#define RG_ACCDET2AUXSWEN_MASK                            0x1
#define RG_ACCDET2AUXSWEN_MASK_SFT                        (0x1 << 15)

/* MT6358_AUDENC_ANA_CON12 */
#define RGS_AUDRCTUNELREAD_SFT                            0
#define RGS_AUDRCTUNELREAD_MASK                           0x1f
#define RGS_AUDRCTUNELREAD_MASK_SFT                       (0x1f << 0)
#define RGS_AUDRCTUNERREAD_SFT                            8
#define RGS_AUDRCTUNERREAD_MASK                           0x1f
#define RGS_AUDRCTUNERREAD_MASK_SFT                       (0x1f << 8)

/* MT6358_AUDDEC_DSN_ID */
#define AUDDEC_ANA_ID_SFT                                 0
#define AUDDEC_ANA_ID_MASK                                0xff
#define AUDDEC_ANA_ID_MASK_SFT                            (0xff << 0)
#define AUDDEC_DIG_ID_SFT                                 8
#define AUDDEC_DIG_ID_MASK                                0xff
#define AUDDEC_DIG_ID_MASK_SFT                            (0xff << 8)

/* MT6358_AUDDEC_DSN_REV0 */
#define AUDDEC_ANA_MINOR_REV_SFT                          0
#define AUDDEC_ANA_MINOR_REV_MASK                         0xf
#define AUDDEC_ANA_MINOR_REV_MASK_SFT                     (0xf << 0)
#define AUDDEC_ANA_MAJOR_REV_SFT                          4
#define AUDDEC_ANA_MAJOR_REV_MASK                         0xf
#define AUDDEC_ANA_MAJOR_REV_MASK_SFT                     (0xf << 4)
#define AUDDEC_DIG_MINOR_REV_SFT                          8
#define AUDDEC_DIG_MINOR_REV_MASK                         0xf
#define AUDDEC_DIG_MINOR_REV_MASK_SFT                     (0xf << 8)
#define AUDDEC_DIG_MAJOR_REV_SFT                          12
#define AUDDEC_DIG_MAJOR_REV_MASK                         0xf
#define AUDDEC_DIG_MAJOR_REV_MASK_SFT                     (0xf << 12)

/* MT6358_AUDDEC_DSN_DBI */
#define AUDDEC_DSN_CBS_SFT                                0
#define AUDDEC_DSN_CBS_MASK                               0x3
#define AUDDEC_DSN_CBS_MASK_SFT                           (0x3 << 0)
#define AUDDEC_DSN_BIX_SFT                                2
#define AUDDEC_DSN_BIX_MASK                               0x3
#define AUDDEC_DSN_BIX_MASK_SFT                           (0x3 << 2)
#define AUDDEC_DSN_ESP_SFT                                8
#define AUDDEC_DSN_ESP_MASK                               0xff
#define AUDDEC_DSN_ESP_MASK_SFT                           (0xff << 8)

/* MT6358_AUDDEC_DSN_FPI */
#define AUDDEC_DSN_FPI_SFT                                0
#define AUDDEC_DSN_FPI_MASK                               0xff
#define AUDDEC_DSN_FPI_MASK_SFT                           (0xff << 0)

/* MT6358_AUDDEC_ANA_CON0 */
#define RG_AUDDACLPWRUP_VAUDP15_SFT                       0
#define RG_AUDDACLPWRUP_VAUDP15_MASK                      0x1
#define RG_AUDDACLPWRUP_VAUDP15_MASK_SFT                  (0x1 << 0)
#define RG_AUDDACRPWRUP_VAUDP15_SFT                       1
#define RG_AUDDACRPWRUP_VAUDP15_MASK                      0x1
#define RG_AUDDACRPWRUP_VAUDP15_MASK_SFT                  (0x1 << 1)
#define RG_AUD_DAC_PWR_UP_VA28_SFT                        2
#define RG_AUD_DAC_PWR_UP_VA28_MASK                       0x1
#define RG_AUD_DAC_PWR_UP_VA28_MASK_SFT                   (0x1 << 2)
#define RG_AUD_DAC_PWL_UP_VA28_SFT                        3
#define RG_AUD_DAC_PWL_UP_VA28_MASK                       0x1
#define RG_AUD_DAC_PWL_UP_VA28_MASK_SFT                   (0x1 << 3)
#define RG_AUDHPLPWRUP_VAUDP15_SFT                        4
#define RG_AUDHPLPWRUP_VAUDP15_MASK                       0x1
#define RG_AUDHPLPWRUP_VAUDP15_MASK_SFT                   (0x1 << 4)
#define RG_AUDHPRPWRUP_VAUDP15_SFT                        5
#define RG_AUDHPRPWRUP_VAUDP15_MASK                       0x1
#define RG_AUDHPRPWRUP_VAUDP15_MASK_SFT                   (0x1 << 5)
#define RG_AUDHPLPWRUP_IBIAS_VAUDP15_SFT                  6
#define RG_AUDHPLPWRUP_IBIAS_VAUDP15_MASK                 0x1
#define RG_AUDHPLPWRUP_IBIAS_VAUDP15_MASK_SFT             (0x1 << 6)
#define RG_AUDHPRPWRUP_IBIAS_VAUDP15_SFT                  7
#define RG_AUDHPRPWRUP_IBIAS_VAUDP15_MASK                 0x1
#define RG_AUDHPRPWRUP_IBIAS_VAUDP15_MASK_SFT             (0x1 << 7)
#define RG_AUDHPLMUXINPUTSEL_VAUDP15_SFT                  8
#define RG_AUDHPLMUXINPUTSEL_VAUDP15_MASK                 0x3
#define RG_AUDHPLMUXINPUTSEL_VAUDP15_MASK_SFT             (0x3 << 8)
#define RG_AUDHPRMUXINPUTSEL_VAUDP15_SFT                  10
#define RG_AUDHPRMUXINPUTSEL_VAUDP15_MASK                 0x3
#define RG_AUDHPRMUXINPUTSEL_VAUDP15_MASK_SFT             (0x3 << 10)
#define RG_AUDHPLSCDISABLE_VAUDP15_SFT                    12
#define RG_AUDHPLSCDISABLE_VAUDP15_MASK                   0x1
#define RG_AUDHPLSCDISABLE_VAUDP15_MASK_SFT               (0x1 << 12)
#define RG_AUDHPRSCDISABLE_VAUDP15_SFT                    13
#define RG_AUDHPRSCDISABLE_VAUDP15_MASK                   0x1
#define RG_AUDHPRSCDISABLE_VAUDP15_MASK_SFT               (0x1 << 13)
#define RG_AUDHPLBSCCURRENT_VAUDP15_SFT                   14
#define RG_AUDHPLBSCCURRENT_VAUDP15_MASK                  0x1
#define RG_AUDHPLBSCCURRENT_VAUDP15_MASK_SFT              (0x1 << 14)
#define RG_AUDHPRBSCCURRENT_VAUDP15_SFT                   15
#define RG_AUDHPRBSCCURRENT_VAUDP15_MASK                  0x1
#define RG_AUDHPRBSCCURRENT_VAUDP15_MASK_SFT              (0x1 << 15)

/* MT6358_AUDDEC_ANA_CON1 */
#define RG_AUDHPLOUTPWRUP_VAUDP15_SFT                     0
#define RG_AUDHPLOUTPWRUP_VAUDP15_MASK                    0x1
#define RG_AUDHPLOUTPWRUP_VAUDP15_MASK_SFT                (0x1 << 0)
#define RG_AUDHPROUTPWRUP_VAUDP15_SFT                     1
#define RG_AUDHPROUTPWRUP_VAUDP15_MASK                    0x1
#define RG_AUDHPROUTPWRUP_VAUDP15_MASK_SFT                (0x1 << 1)
#define RG_AUDHPLOUTAUXPWRUP_VAUDP15_SFT                  2
#define RG_AUDHPLOUTAUXPWRUP_VAUDP15_MASK                 0x1
#define RG_AUDHPLOUTAUXPWRUP_VAUDP15_MASK_SFT             (0x1 << 2)
#define RG_AUDHPROUTAUXPWRUP_VAUDP15_SFT                  3
#define RG_AUDHPROUTAUXPWRUP_VAUDP15_MASK                 0x1
#define RG_AUDHPROUTAUXPWRUP_VAUDP15_MASK_SFT             (0x1 << 3)
#define RG_HPLAUXFBRSW_EN_VAUDP15_SFT                     4
#define RG_HPLAUXFBRSW_EN_VAUDP15_MASK                    0x1
#define RG_HPLAUXFBRSW_EN_VAUDP15_MASK_SFT                (0x1 << 4)
#define RG_HPRAUXFBRSW_EN_VAUDP15_SFT                     5
#define RG_HPRAUXFBRSW_EN_VAUDP15_MASK                    0x1
#define RG_HPRAUXFBRSW_EN_VAUDP15_MASK_SFT                (0x1 << 5)
#define RG_HPLSHORT2HPLAUX_EN_VAUDP15_SFT                 6
#define RG_HPLSHORT2HPLAUX_EN_VAUDP15_MASK                0x1
#define RG_HPLSHORT2HPLAUX_EN_VAUDP15_MASK_SFT            (0x1 << 6)
#define RG_HPRSHORT2HPRAUX_EN_VAUDP15_SFT                 7
#define RG_HPRSHORT2HPRAUX_EN_VAUDP15_MASK                0x1
#define RG_HPRSHORT2HPRAUX_EN_VAUDP15_MASK_SFT            (0x1 << 7)
#define RG_HPLOUTSTGCTRL_VAUDP15_SFT                      8
#define RG_HPLOUTSTGCTRL_VAUDP15_MASK                     0x7
#define RG_HPLOUTSTGCTRL_VAUDP15_MASK_SFT                 (0x7 << 8)
#define RG_HPROUTSTGCTRL_VAUDP15_SFT                      11
#define RG_HPROUTSTGCTRL_VAUDP15_MASK                     0x7
#define RG_HPROUTSTGCTRL_VAUDP15_MASK_SFT                 (0x7 << 11)

/* MT6358_AUDDEC_ANA_CON2 */
#define RG_HPLOUTPUTSTBENH_VAUDP15_SFT                    0
#define RG_HPLOUTPUTSTBENH_VAUDP15_MASK                   0x7
#define RG_HPLOUTPUTSTBENH_VAUDP15_MASK_SFT               (0x7 << 0)
#define RG_HPROUTPUTSTBENH_VAUDP15_SFT                    4
#define RG_HPROUTPUTSTBENH_VAUDP15_MASK                   0x7
#define RG_HPROUTPUTSTBENH_VAUDP15_MASK_SFT               (0x7 << 4)
#define RG_AUDHPSTARTUP_VAUDP15_SFT                       13
#define RG_AUDHPSTARTUP_VAUDP15_MASK                      0x1
#define RG_AUDHPSTARTUP_VAUDP15_MASK_SFT                  (0x1 << 13)
#define RG_AUDREFN_DERES_EN_VAUDP15_SFT                   14
#define RG_AUDREFN_DERES_EN_VAUDP15_MASK                  0x1
#define RG_AUDREFN_DERES_EN_VAUDP15_MASK_SFT              (0x1 << 14)
#define RG_HPPSHORT2VCM_VAUDP15_SFT                       15
#define RG_HPPSHORT2VCM_VAUDP15_MASK                      0x1
#define RG_HPPSHORT2VCM_VAUDP15_MASK_SFT                  (0x1 << 15)

/* MT6358_AUDDEC_ANA_CON3 */
#define RG_HPINPUTSTBENH_VAUDP15_SFT                      13
#define RG_HPINPUTSTBENH_VAUDP15_MASK                     0x1
#define RG_HPINPUTSTBENH_VAUDP15_MASK_SFT                 (0x1 << 13)
#define RG_HPINPUTRESET0_VAUDP15_SFT                      14
#define RG_HPINPUTRESET0_VAUDP15_MASK                     0x1
#define RG_HPINPUTRESET0_VAUDP15_MASK_SFT                 (0x1 << 14)
#define RG_HPOUTPUTRESET0_VAUDP15_SFT                     15
#define RG_HPOUTPUTRESET0_VAUDP15_MASK                    0x1
#define RG_HPOUTPUTRESET0_VAUDP15_MASK_SFT                (0x1 << 15)

/* MT6358_AUDDEC_ANA_CON4 */
#define RG_ABIDEC_RSVD0_VAUDP28_SFT                       0
#define RG_ABIDEC_RSVD0_VAUDP28_MASK                      0xff
#define RG_ABIDEC_RSVD0_VAUDP28_MASK_SFT                  (0xff << 0)

/* MT6358_AUDDEC_ANA_CON5 */
#define RG_AUDHPDECMGAINADJ_VAUDP15_SFT                   0
#define RG_AUDHPDECMGAINADJ_VAUDP15_MASK                  0x7
#define RG_AUDHPDECMGAINADJ_VAUDP15_MASK_SFT              (0x7 << 0)
#define RG_AUDHPDEDMGAINADJ_VAUDP15_SFT                   4
#define RG_AUDHPDEDMGAINADJ_VAUDP15_MASK                  0x7
#define RG_AUDHPDEDMGAINADJ_VAUDP15_MASK_SFT              (0x7 << 4)

/* MT6358_AUDDEC_ANA_CON6 */
#define RG_AUDHSPWRUP_VAUDP15_SFT                         0
#define RG_AUDHSPWRUP_VAUDP15_MASK                        0x1
#define RG_AUDHSPWRUP_VAUDP15_MASK_SFT                    (0x1 << 0)
#define RG_AUDHSPWRUP_IBIAS_VAUDP15_SFT                   1
#define RG_AUDHSPWRUP_IBIAS_VAUDP15_MASK                  0x1
#define RG_AUDHSPWRUP_IBIAS_VAUDP15_MASK_SFT              (0x1 << 1)
#define RG_AUDHSMUXINPUTSEL_VAUDP15_SFT                   2
#define RG_AUDHSMUXINPUTSEL_VAUDP15_MASK                  0x3
#define RG_AUDHSMUXINPUTSEL_VAUDP15_MASK_SFT              (0x3 << 2)
#define RG_AUDHSSCDISABLE_VAUDP15_SFT                     4
#define RG_AUDHSSCDISABLE_VAUDP15_MASK                    0x1
#define RG_AUDHSSCDISABLE_VAUDP15_MASK_SFT                (0x1 << 4)
#define RG_AUDHSBSCCURRENT_VAUDP15_SFT                    5
#define RG_AUDHSBSCCURRENT_VAUDP15_MASK                   0x1
#define RG_AUDHSBSCCURRENT_VAUDP15_MASK_SFT               (0x1 << 5)
#define RG_AUDHSSTARTUP_VAUDP15_SFT                       6
#define RG_AUDHSSTARTUP_VAUDP15_MASK                      0x1
#define RG_AUDHSSTARTUP_VAUDP15_MASK_SFT                  (0x1 << 6)
#define RG_HSOUTPUTSTBENH_VAUDP15_SFT                     7
#define RG_HSOUTPUTSTBENH_VAUDP15_MASK                    0x1
#define RG_HSOUTPUTSTBENH_VAUDP15_MASK_SFT                (0x1 << 7)
#define RG_HSINPUTSTBENH_VAUDP15_SFT                      8
#define RG_HSINPUTSTBENH_VAUDP15_MASK                     0x1
#define RG_HSINPUTSTBENH_VAUDP15_MASK_SFT                 (0x1 << 8)
#define RG_HSINPUTRESET0_VAUDP15_SFT                      9
#define RG_HSINPUTRESET0_VAUDP15_MASK                     0x1
#define RG_HSINPUTRESET0_VAUDP15_MASK_SFT                 (0x1 << 9)
#define RG_HSOUTPUTRESET0_VAUDP15_SFT                     10
#define RG_HSOUTPUTRESET0_VAUDP15_MASK                    0x1
#define RG_HSOUTPUTRESET0_VAUDP15_MASK_SFT                (0x1 << 10)
#define RG_HSOUT_SHORTVCM_VAUDP15_SFT                     11
#define RG_HSOUT_SHORTVCM_VAUDP15_MASK                    0x1
#define RG_HSOUT_SHORTVCM_VAUDP15_MASK_SFT                (0x1 << 11)

/* MT6358_AUDDEC_ANA_CON7 */
#define RG_AUDLOLPWRUP_VAUDP15_SFT                        0
#define RG_AUDLOLPWRUP_VAUDP15_MASK                       0x1
#define RG_AUDLOLPWRUP_VAUDP15_MASK_SFT                   (0x1 << 0)
#define RG_AUDLOLPWRUP_IBIAS_VAUDP15_SFT                  1
#define RG_AUDLOLPWRUP_IBIAS_VAUDP15_MASK                 0x1
#define RG_AUDLOLPWRUP_IBIAS_VAUDP15_MASK_SFT             (0x1 << 1)
#define RG_AUDLOLMUXINPUTSEL_VAUDP15_SFT                  2
#define RG_AUDLOLMUXINPUTSEL_VAUDP15_MASK                 0x3
#define RG_AUDLOLMUXINPUTSEL_VAUDP15_MASK_SFT             (0x3 << 2)
#define RG_AUDLOLSCDISABLE_VAUDP15_SFT                    4
#define RG_AUDLOLSCDISABLE_VAUDP15_MASK                   0x1
#define RG_AUDLOLSCDISABLE_VAUDP15_MASK_SFT               (0x1 << 4)
#define RG_AUDLOLBSCCURRENT_VAUDP15_SFT                   5
#define RG_AUDLOLBSCCURRENT_VAUDP15_MASK                  0x1
#define RG_AUDLOLBSCCURRENT_VAUDP15_MASK_SFT              (0x1 << 5)
#define RG_AUDLOSTARTUP_VAUDP15_SFT                       6
#define RG_AUDLOSTARTUP_VAUDP15_MASK                      0x1
#define RG_AUDLOSTARTUP_VAUDP15_MASK_SFT                  (0x1 << 6)
#define RG_LOINPUTSTBENH_VAUDP15_SFT                      7
#define RG_LOINPUTSTBENH_VAUDP15_MASK                     0x1
#define RG_LOINPUTSTBENH_VAUDP15_MASK_SFT                 (0x1 << 7)
#define RG_LOOUTPUTSTBENH_VAUDP15_SFT                     8
#define RG_LOOUTPUTSTBENH_VAUDP15_MASK                    0x1
#define RG_LOOUTPUTSTBENH_VAUDP15_MASK_SFT                (0x1 << 8)
#define RG_LOINPUTRESET0_VAUDP15_SFT                      9
#define RG_LOINPUTRESET0_VAUDP15_MASK                     0x1
#define RG_LOINPUTRESET0_VAUDP15_MASK_SFT                 (0x1 << 9)
#define RG_LOOUTPUTRESET0_VAUDP15_SFT                     10
#define RG_LOOUTPUTRESET0_VAUDP15_MASK                    0x1
#define RG_LOOUTPUTRESET0_VAUDP15_MASK_SFT                (0x1 << 10)
#define RG_LOOUT_SHORTVCM_VAUDP15_SFT                     11
#define RG_LOOUT_SHORTVCM_VAUDP15_MASK                    0x1
#define RG_LOOUT_SHORTVCM_VAUDP15_MASK_SFT                (0x1 << 11)

/* MT6358_AUDDEC_ANA_CON8 */
#define RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP15_SFT             0
#define RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP15_MASK            0xf
#define RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP15_MASK_SFT        (0xf << 0)
#define RG_AUDTRIMBUF_GAINSEL_VAUDP15_SFT                 4
#define RG_AUDTRIMBUF_GAINSEL_VAUDP15_MASK                0x3
#define RG_AUDTRIMBUF_GAINSEL_VAUDP15_MASK_SFT            (0x3 << 4)
#define RG_AUDTRIMBUF_EN_VAUDP15_SFT                      6
#define RG_AUDTRIMBUF_EN_VAUDP15_MASK                     0x1
#define RG_AUDTRIMBUF_EN_VAUDP15_MASK_SFT                 (0x1 << 6)
#define RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP15_SFT            8
#define RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP15_MASK           0x3
#define RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP15_MASK_SFT       (0x3 << 8)
#define RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP15_SFT           10
#define RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP15_MASK          0x3
#define RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP15_MASK_SFT      (0x3 << 10)
#define RG_AUDHPSPKDET_EN_VAUDP15_SFT                     12
#define RG_AUDHPSPKDET_EN_VAUDP15_MASK                    0x1
#define RG_AUDHPSPKDET_EN_VAUDP15_MASK_SFT                (0x1 << 12)

/* MT6358_AUDDEC_ANA_CON9 */
#define RG_ABIDEC_RSVD0_VA28_SFT                          0
#define RG_ABIDEC_RSVD0_VA28_MASK                         0xff
#define RG_ABIDEC_RSVD0_VA28_MASK_SFT                     (0xff << 0)
#define RG_ABIDEC_RSVD0_VAUDP15_SFT                       8
#define RG_ABIDEC_RSVD0_VAUDP15_MASK                      0xff
#define RG_ABIDEC_RSVD0_VAUDP15_MASK_SFT                  (0xff << 8)

/* MT6358_AUDDEC_ANA_CON10 */
#define RG_ABIDEC_RSVD1_VAUDP15_SFT                       0
#define RG_ABIDEC_RSVD1_VAUDP15_MASK                      0xff
#define RG_ABIDEC_RSVD1_VAUDP15_MASK_SFT                  (0xff << 0)
#define RG_ABIDEC_RSVD2_VAUDP15_SFT                       8
#define RG_ABIDEC_RSVD2_VAUDP15_MASK                      0xff
#define RG_ABIDEC_RSVD2_VAUDP15_MASK_SFT                  (0xff << 8)

/* MT6358_AUDDEC_ANA_CON11 */
#define RG_AUDZCDMUXSEL_VAUDP15_SFT                       0
#define RG_AUDZCDMUXSEL_VAUDP15_MASK                      0x7
#define RG_AUDZCDMUXSEL_VAUDP15_MASK_SFT                  (0x7 << 0)
#define RG_AUDZCDCLKSEL_VAUDP15_SFT                       3
#define RG_AUDZCDCLKSEL_VAUDP15_MASK                      0x1
#define RG_AUDZCDCLKSEL_VAUDP15_MASK_SFT                  (0x1 << 3)
#define RG_AUDBIASADJ_0_VAUDP15_SFT                       7
#define RG_AUDBIASADJ_0_VAUDP15_MASK                      0x1ff
#define RG_AUDBIASADJ_0_VAUDP15_MASK_SFT                  (0x1ff << 7)

/* MT6358_AUDDEC_ANA_CON12 */
#define RG_AUDBIASADJ_1_VAUDP15_SFT                       0
#define RG_AUDBIASADJ_1_VAUDP15_MASK                      0xff
#define RG_AUDBIASADJ_1_VAUDP15_MASK_SFT                  (0xff << 0)
#define RG_AUDIBIASPWRDN_VAUDP15_SFT                      8
#define RG_AUDIBIASPWRDN_VAUDP15_MASK                     0x1
#define RG_AUDIBIASPWRDN_VAUDP15_MASK_SFT                 (0x1 << 8)

/* MT6358_AUDDEC_ANA_CON13 */
#define RG_RSTB_DECODER_VA28_SFT                          0
#define RG_RSTB_DECODER_VA28_MASK                         0x1
#define RG_RSTB_DECODER_VA28_MASK_SFT                     (0x1 << 0)
#define RG_SEL_DECODER_96K_VA28_SFT                       1
#define RG_SEL_DECODER_96K_VA28_MASK                      0x1
#define RG_SEL_DECODER_96K_VA28_MASK_SFT                  (0x1 << 1)
#define RG_SEL_DELAY_VCORE_SFT                            2
#define RG_SEL_DELAY_VCORE_MASK                           0x1
#define RG_SEL_DELAY_VCORE_MASK_SFT                       (0x1 << 2)
#define RG_AUDGLB_PWRDN_VA28_SFT                          4
#define RG_AUDGLB_PWRDN_VA28_MASK                         0x1
#define RG_AUDGLB_PWRDN_VA28_MASK_SFT                     (0x1 << 4)
#define RG_RSTB_ENCODER_VA28_SFT                          5
#define RG_RSTB_ENCODER_VA28_MASK                         0x1
#define RG_RSTB_ENCODER_VA28_MASK_SFT                     (0x1 << 5)
#define RG_SEL_ENCODER_96K_VA28_SFT                       6
#define RG_SEL_ENCODER_96K_VA28_MASK                      0x1
#define RG_SEL_ENCODER_96K_VA28_MASK_SFT                  (0x1 << 6)

/* MT6358_AUDDEC_ANA_CON14 */
#define RG_HCLDO_EN_VA18_SFT                              0
#define RG_HCLDO_EN_VA18_MASK                             0x1
#define RG_HCLDO_EN_VA18_MASK_SFT                         (0x1 << 0)
#define RG_HCLDO_PDDIS_EN_VA18_SFT                        1
#define RG_HCLDO_PDDIS_EN_VA18_MASK                       0x1
#define RG_HCLDO_PDDIS_EN_VA18_MASK_SFT                   (0x1 << 1)
#define RG_HCLDO_REMOTE_SENSE_VA18_SFT                    2
#define RG_HCLDO_REMOTE_SENSE_VA18_MASK                   0x1
#define RG_HCLDO_REMOTE_SENSE_VA18_MASK_SFT               (0x1 << 2)
#define RG_LCLDO_EN_VA18_SFT                              4
#define RG_LCLDO_EN_VA18_MASK                             0x1
#define RG_LCLDO_EN_VA18_MASK_SFT                         (0x1 << 4)
#define RG_LCLDO_PDDIS_EN_VA18_SFT                        5
#define RG_LCLDO_PDDIS_EN_VA18_MASK                       0x1
#define RG_LCLDO_PDDIS_EN_VA18_MASK_SFT                   (0x1 << 5)
#define RG_LCLDO_REMOTE_SENSE_VA18_SFT                    6
#define RG_LCLDO_REMOTE_SENSE_VA18_MASK                   0x1
#define RG_LCLDO_REMOTE_SENSE_VA18_MASK_SFT               (0x1 << 6)
#define RG_LCLDO_ENC_EN_VA28_SFT                          8
#define RG_LCLDO_ENC_EN_VA28_MASK                         0x1
#define RG_LCLDO_ENC_EN_VA28_MASK_SFT                     (0x1 << 8)
#define RG_LCLDO_ENC_PDDIS_EN_VA28_SFT                    9
#define RG_LCLDO_ENC_PDDIS_EN_VA28_MASK                   0x1
#define RG_LCLDO_ENC_PDDIS_EN_VA28_MASK_SFT               (0x1 << 9)
#define RG_LCLDO_ENC_REMOTE_SENSE_VA28_SFT                10
#define RG_LCLDO_ENC_REMOTE_SENSE_VA28_MASK               0x1
#define RG_LCLDO_ENC_REMOTE_SENSE_VA28_MASK_SFT           (0x1 << 10)
#define RG_VA33REFGEN_EN_VA18_SFT                         12
#define RG_VA33REFGEN_EN_VA18_MASK                        0x1
#define RG_VA33REFGEN_EN_VA18_MASK_SFT                    (0x1 << 12)
#define RG_VA28REFGEN_EN_VA28_SFT                         13
#define RG_VA28REFGEN_EN_VA28_MASK                        0x1
#define RG_VA28REFGEN_EN_VA28_MASK_SFT                    (0x1 << 13)
#define RG_HCLDO_VOSEL_VA18_SFT                           14
#define RG_HCLDO_VOSEL_VA18_MASK                          0x1
#define RG_HCLDO_VOSEL_VA18_MASK_SFT                      (0x1 << 14)
#define RG_LCLDO_VOSEL_VA18_SFT                           15
#define RG_LCLDO_VOSEL_VA18_MASK                          0x1
#define RG_LCLDO_VOSEL_VA18_MASK_SFT                      (0x1 << 15)

/* MT6358_AUDDEC_ANA_CON15 */
#define RG_NVREG_EN_VAUDP15_SFT                           0
#define RG_NVREG_EN_VAUDP15_MASK                          0x1
#define RG_NVREG_EN_VAUDP15_MASK_SFT                      (0x1 << 0)
#define RG_NVREG_PULL0V_VAUDP15_SFT                       1
#define RG_NVREG_PULL0V_VAUDP15_MASK                      0x1
#define RG_NVREG_PULL0V_VAUDP15_MASK_SFT                  (0x1 << 1)
#define RG_AUDPMU_RSD0_VAUDP15_SFT                        4
#define RG_AUDPMU_RSD0_VAUDP15_MASK                       0xf
#define RG_AUDPMU_RSD0_VAUDP15_MASK_SFT                   (0xf << 4)
#define RG_AUDPMU_RSD0_VA18_SFT                           8
#define RG_AUDPMU_RSD0_VA18_MASK                          0xf
#define RG_AUDPMU_RSD0_VA18_MASK_SFT                      (0xf << 8)
#define RG_AUDPMU_RSD0_VA28_SFT                           12
#define RG_AUDPMU_RSD0_VA28_MASK                          0xf
#define RG_AUDPMU_RSD0_VA28_MASK_SFT                      (0xf << 12)

/* MT6358_ZCD_CON0 */
#define RG_AUDZCDENABLE_SFT                               0
#define RG_AUDZCDENABLE_MASK                              0x1
#define RG_AUDZCDENABLE_MASK_SFT                          (0x1 << 0)
#define RG_AUDZCDGAINSTEPTIME_SFT                         1
#define RG_AUDZCDGAINSTEPTIME_MASK                        0x7
#define RG_AUDZCDGAINSTEPTIME_MASK_SFT                    (0x7 << 1)
#define RG_AUDZCDGAINSTEPSIZE_SFT                         4
#define RG_AUDZCDGAINSTEPSIZE_MASK                        0x3
#define RG_AUDZCDGAINSTEPSIZE_MASK_SFT                    (0x3 << 4)
#define RG_AUDZCDTIMEOUTMODESEL_SFT                       6
#define RG_AUDZCDTIMEOUTMODESEL_MASK                      0x1
#define RG_AUDZCDTIMEOUTMODESEL_MASK_SFT                  (0x1 << 6)

/* MT6358_ZCD_CON1 */
#define RG_AUDLOLGAIN_SFT                                 0
#define RG_AUDLOLGAIN_MASK                                0x1f
#define RG_AUDLOLGAIN_MASK_SFT                            (0x1f << 0)
#define RG_AUDLORGAIN_SFT                                 7
#define RG_AUDLORGAIN_MASK                                0x1f
#define RG_AUDLORGAIN_MASK_SFT                            (0x1f << 7)

/* MT6358_ZCD_CON2 */
#define RG_AUDHPLGAIN_SFT                                 0
#define RG_AUDHPLGAIN_MASK                                0x1f
#define RG_AUDHPLGAIN_MASK_SFT                            (0x1f << 0)
#define RG_AUDHPRGAIN_SFT                                 7
#define RG_AUDHPRGAIN_MASK                                0x1f
#define RG_AUDHPRGAIN_MASK_SFT                            (0x1f << 7)

/* MT6358_ZCD_CON3 */
#define RG_AUDHSGAIN_SFT                                  0
#define RG_AUDHSGAIN_MASK                                 0x1f
#define RG_AUDHSGAIN_MASK_SFT                             (0x1f << 0)

/* MT6358_ZCD_CON4 */
#define RG_AUDIVLGAIN_SFT                                 0
#define RG_AUDIVLGAIN_MASK                                0x7
#define RG_AUDIVLGAIN_MASK_SFT                            (0x7 << 0)
#define RG_AUDIVRGAIN_SFT                                 8
#define RG_AUDIVRGAIN_MASK                                0x7
#define RG_AUDIVRGAIN_MASK_SFT                            (0x7 << 8)

/* MT6358_ZCD_CON5 */
#define RG_AUDINTGAIN1_SFT                                0
#define RG_AUDINTGAIN1_MASK                               0x3f
#define RG_AUDINTGAIN1_MASK_SFT                           (0x3f << 0)
#define RG_AUDINTGAIN2_SFT                                8
#define RG_AUDINTGAIN2_MASK                               0x3f
#define RG_AUDINTGAIN2_MASK_SFT                           (0x3f << 8)

#define MT6358_MAX_REGISTER MT6358_ZCD_CON5

#define REG_STRIDE 2
#define ANALOG_HPTRIM

/* dl pga gain */
enum {
	DL_GAIN_8DB = 0,
	DL_GAIN_0DB = 8,
	DL_GAIN_N_1DB = 9,
	DL_GAIN_N_10DB = 18,
	DL_GAIN_N_40DB = 0x1f,
};

enum {
	MT6358_MTKAIF_PROTOCOL_1 = 0,
	MT6358_MTKAIF_PROTOCOL_2,
	MT6358_MTKAIF_PROTOCOL_2_CLK_P2,
};

enum {
	AUDIO_ANALOG_VOLUME_HSOUTL,
	AUDIO_ANALOG_VOLUME_HSOUTR,
	AUDIO_ANALOG_VOLUME_HPOUTL,
	AUDIO_ANALOG_VOLUME_HPOUTR,
	AUDIO_ANALOG_VOLUME_LINEOUTL,
	AUDIO_ANALOG_VOLUME_LINEOUTR,
	AUDIO_ANALOG_VOLUME_MICAMP1,
	AUDIO_ANALOG_VOLUME_MICAMP2,
	AUDIO_ANALOG_VOLUME_TYPE_MAX
};

enum {
	MUX_ADC_L,
	MUX_ADC_R,
	MUX_PGA_L,
	MUX_PGA_R,
	MUX_MIC_TYPE,
	MUX_HP_L,
	MUX_HP_R,
	MUX_NUM,
};

enum {
	DEVICE_HP,
	DEVICE_LO,
	DEVICE_RCV,
	DEVICE_MIC1,
	DEVICE_MIC2,
	DEVICE_NUM
};

/* Supply widget subseq */
enum {
	/* common */
	SUPPLY_SEQ_CLK_BUF,
	SUPPLY_SEQ_AUD_GLB,
	SUPPLY_SEQ_CLKSQ,
	SUPPLY_SEQ_ADC_SUPPLY,
	SUPPLY_SEQ_AUD_VOW,
	SUPPLY_SEQ_VOW_CLK,
	SUPPLY_SEQ_VOW_LDO,
	SUPPLY_SEQ_TOP_CK,
	SUPPLY_SEQ_TOP_CK_LAST,
	SUPPLY_SEQ_AUD_TOP,
	SUPPLY_SEQ_AUD_TOP_LAST,
	SUPPLY_SEQ_AFE,

	/* capture */
	SUPPLY_SEQ_MIC_BIAS,
};

enum {
	CH_L = 0,
	CH_R,
	NUM_CH,
};

/* Auxadc average resolution */
enum {
	AUXADC_AVG_1 = 0,
	AUXADC_AVG_4,
	AUXADC_AVG_8,
	AUXADC_AVG_16,
	AUXADC_AVG_32,
	AUXADC_AVG_64,
	AUXADC_AVG_128,
	AUXADC_AVG_256,
};

enum {
	DBG_DCTRIM_BYPASS_4POLE = 0x1 << 0,
	DBG_DCTRIM_4POLE_LOG = 0x1 << 1,
};

/* LOL MUX */
enum {
	LOL_MUX_OPEN = 0,
	LOL_MUX_MUTE,
	LOL_MUX_PLAYBACK,
	LOL_MUX_TEST_MODE,
	LOL_MUX_MASK = 0x3,
};

/*HP MUX */
enum {
	HP_MUX_OPEN = 0,
	HP_MUX_HPSPK,
	HP_MUX_HP,
	HP_MUX_TEST_MODE,
	HP_MUX_HP_IMPEDANCE,
	HP_MUX_HP_DUALSPK,
	HP_MUX_MASK = 0x7,
};

/* RCV MUX */
enum {
	RCV_MUX_OPEN = 0,
	RCV_MUX_MUTE,
	RCV_MUX_VOICE_PLAYBACK,
	RCV_MUX_TEST_MODE,
	RCV_MUX_MASK = 0x3,
};

/* Mic Type MUX */
enum {
	MIC_TYPE_MUX_IDLE = 0,
	MIC_TYPE_MUX_ACC,
	MIC_TYPE_MUX_DMIC,
	MIC_TYPE_MUX_DCC,
	MIC_TYPE_MUX_DCC_ECM_DIFF,
	MIC_TYPE_MUX_DCC_ECM_SINGLE,
	MIC_TYPE_MUX_MASK = 0x7,
};

/* ADC L MUX */
enum {
	ADC_MUX_IDLE = 0,
	ADC_MUX_AIN0,
	ADC_MUX_PREAMPLIFIER,
	ADC_MUX_IDLE1,
	ADC_MUX_MASK = 0x3,
};

/* PGA L MUX */
enum {
	PGA_MUX_NONE = 0,
	PGA_MUX_AIN0,
	PGA_MUX_AIN1,
	PGA_MUX_AIN2,
	PGA_MUX_MASK = 0x3,
};

enum {
	HP_INPUT_MUX_OPEN = 0,
	HP_INPUT_MUX_LOL,
	HP_INPUT_MUX_IDACR,
	HP_INPUT_MUX_HS,
};

/* trim buffer */
enum {
	TRIM_BUF_MUX_OPEN = 0,
	TRIM_BUF_MUX_HPL,
	TRIM_BUF_MUX_HPR,
	TRIM_BUF_MUX_HSP,
	TRIM_BUF_MUX_HSN,
	TRIM_BUF_MUX_LOLP,
	TRIM_BUF_MUX_LOLN,
	TRIM_MUX_AU_REFN,
	TRIM_MUX_AVSS28,
	TRIM_MUX_AVSS28_2,
	TRIM_MUX_UNUSED,
	TRIM_BUF_MUX_GROUND,
};

enum {
	TRIM_BUF_GAIN_0DB = 0,
	TRIM_BUF_GAIN_6DB,
	TRIM_BUF_GAIN_12DB,
	TRIM_BUF_GAIN_18DB,
};

enum {
	MIC_BIAS_1P7 = 0,
	MIC_BIAS_1P8,
	MIC_BIAS_1P9,
	MIC_BIAS_2P0,
	MIC_BIAS_2P1,
	MIC_BIAS_2P5,
	MIC_BIAS_2P6,
	MIC_BIAS_2P7,
};

enum {
	RCV_MIC_OFF = 0,
	RCV_MIC_ACC,
	RCV_MIC_DCC,
};

#ifdef ANALOG_HPTRIM
struct ana_offset {
	int enable;
	int hp_trim_code[NUM_CH];
	int hp_fine_trim[NUM_CH];
};
#endif

struct dc_trim_data {
	bool calibrated;
	int hp_offset[NUM_CH];
	int hp_trim_offset[NUM_CH];
	int spk_l_offset;
	int pre_comp_value[NUM_CH];
	int mic_vinp_mv;
#ifdef ANALOG_HPTRIM
	int dc_compensation_disabled;
	unsigned int hp_3_pole_trim_setting;
	unsigned int hp_4_pole_trim_setting;
	unsigned int spk_hp_3_pole_trim_setting;
	unsigned int spk_hp_4_pole_trim_setting;
	struct ana_offset hp_3_pole_ana_offset;
	struct ana_offset hp_4_pole_ana_offset;
	struct ana_offset spk_3_pole_ana_offset;
	struct ana_offset spk_4_pole_ana_offset;
#endif
};

struct mt6358_codec_ops {
	int (*enable_dc_compensation)(bool enable);
	int (*set_lch_dc_compensation)(int value);
	int (*set_rch_dc_compensation)(int value);
	int (*adda_dl_gain_control)(bool mute);
};

struct mt6358_priv {
	struct device *dev;
	struct regmap *regmap;

	unsigned int dl_rate;
	unsigned int ul_rate;

	int ana_gain[AUDIO_ANALOG_VOLUME_TYPE_MAX];
	unsigned int mux_select[MUX_NUM];
	int dmic_one_wire_mode;

	int dev_counter[DEVICE_NUM];

	struct mt6358_codec_ops ops;
	bool apply_n12db_gain;
	int hp_plugged;

	/* dc trim */
	struct dc_trim_data dc_trim;
	struct iio_channel *hpofs_cal_auxadc;

	/* headphone impedence */
	struct nvmem_device *hp_efuse;
	int hp_impedance;
	int hp_current_calibrate_val;

	int mtkaif_protocol;

	struct dentry *debugfs;
	unsigned int debug_flag;
	/* regulator */
	struct regulator *reg_vaud28;

	/* vow control */
	int vow_enable;
	int reg_afe_vow_cfg0;
	int reg_afe_vow_cfg1;
	int reg_afe_vow_cfg2;
	int reg_afe_vow_cfg3;
	int reg_afe_vow_cfg4;
	int reg_afe_vow_cfg5;
	int reg_afe_vow_periodic;
	/* vow dmic low power mode, 1: enable, 0: disable */
	int vow_dmic_lp;

	int pull_down_stay_enable;
};

/* dl pga gain */
#define DL_GAIN_N_10DB_REG (DL_GAIN_N_10DB << 7 | DL_GAIN_N_10DB)
#define DL_GAIN_N_40DB_REG (DL_GAIN_N_40DB << 7 | DL_GAIN_N_40DB)
#define DL_GAIN_REG_MASK 0x0f9f

/* reg idx for -40dB*/
#define PGA_MINUS_40_DB_REG_VAL 0x1f
#define HP_PGA_MINUS_40_DB_REG_VAL 0x3f

/* mic type */
#define IS_DCC_BASE(x) (x == MIC_TYPE_MUX_DCC || \
			x == MIC_TYPE_MUX_DCC_ECM_DIFF || \
			x == MIC_TYPE_MUX_DCC_ECM_SINGLE)

#define IS_AMIC_BASE(x) (x == MIC_TYPE_MUX_ACC || IS_DCC_BASE(x))

#define MIC_VINP_4POLE_THRES_MV 283
#define VINP_NORMALIZED_TO_MV 1700

/* hp trim */
#ifdef ANALOG_HPTRIM
#define HPTRIM_L_SHIFT 0
#define HPTRIM_R_SHIFT 4
#define HPFINETRIM_L_SHIFT 8
#define HPFINETRIM_R_SHIFT 10
#define HPTRIM_EN_SHIFT 12
#define HPTRIM_L_MASK (0xf << HPTRIM_L_SHIFT)
#define HPTRIM_R_MASK (0xf << HPTRIM_R_SHIFT)
#define HPFINETRIM_L_MASK (0x3 << HPFINETRIM_L_SHIFT)
#define HPFINETRIM_R_MASK (0x3 << HPFINETRIM_R_SHIFT)
#define HPTRIM_EN_MASK (0x1 << HPTRIM_EN_SHIFT)
#endif

/* dc trim */
#ifdef ANALOG_HPTRIM
#define TRIM_TIMES 7
#else
#define TRIM_TIMES 26
#endif
#define TRIM_DISCARD_NUM 1
#define TRIM_USEFUL_NUM (TRIM_TIMES - (TRIM_DISCARD_NUM * 2))

/* headphone impedance detection */
#define PARALLEL_OHM 0

/* codec name */
#define CODEC_MT6358_NAME "mtk-codec-mt6358"
#define DEVICE_MT6358_NAME "mt6358-sound"

/* set only during init */
int mt6358_set_codec_ops(struct snd_soc_component *cmpnt,
			 struct mt6358_codec_ops *ops);
int mt6358_set_mtkaif_protocol(struct snd_soc_component *cmpnt,
			       int mtkaif_protocol);
int mt6358_mtkaif_calibration_enable(struct snd_soc_component *cmpnt);
int mt6358_mtkaif_calibration_disable(struct snd_soc_component *cmpnt);
int mt6358_set_mtkaif_calibration_phase(struct snd_soc_component *cmpnt,
					int phase_1, int phase_2);
#endif /* __MT6358_H__ */
