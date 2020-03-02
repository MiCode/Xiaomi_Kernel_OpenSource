/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_Ana.h
 *
 * Project:
 * --------
 *   Audio Driver Ana
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 *   Chipeng Chang (mtk02308)
 *
 *------------------------------------------------------------------------------
 *
 *
 ******************************************************************************/

#ifndef _AUDDRV_ANA_H_
#define _AUDDRV_ANA_H_

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/
#include "mtk-auddrv-def.h"

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/


/*****************************************************************************
 *                         M A C R O
 *****************************************************************************/

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
#define SWCID                         0x000A
#define AUD_TOP_ID                    0x2200
#define AUD_TOP_REV0                  0x2202
#define AUD_TOP_DBI                   0x2204
#define AUD_TOP_DXI                   0x2206
#define AUD_TOP_CKPDN_TPM0            0x2208
#define AUD_TOP_CKPDN_TPM1            0x220a
#define AUD_TOP_CKPDN_CON0            0x220c
#define AUD_TOP_CKPDN_CON0_SET        0x220e
#define AUD_TOP_CKPDN_CON0_CLR        0x2210
#define AUD_TOP_CKSEL_CON0            0x2212
#define AUD_TOP_CKSEL_CON0_SET        0x2214
#define AUD_TOP_CKSEL_CON0_CLR        0x2216
#define AUD_TOP_CKTST_CON0            0x2218
#define AUD_TOP_CLK_HWEN_CON0         0x221a
#define AUD_TOP_CLK_HWEN_CON0_SET     0x221c
#define AUD_TOP_CLK_HWEN_CON0_CLR     0x221e
#define AUD_TOP_RST_CON0              0x2220
#define AUD_TOP_RST_CON0_SET          0x2222
#define AUD_TOP_RST_CON0_CLR          0x2224
#define AUD_TOP_RST_BANK_CON0         0x2226
#define AUD_TOP_INT_CON0              0x2228
#define AUD_TOP_INT_CON0_SET          0x222a
#define AUD_TOP_INT_CON0_CLR          0x222c
#define AUD_TOP_INT_MASK_CON0         0x222e
#define AUD_TOP_INT_MASK_CON0_SET     0x2230
#define AUD_TOP_INT_MASK_CON0_CLR     0x2232
#define AUD_TOP_INT_STATUS0           0x2234
#define AUD_TOP_INT_RAW_STATUS0       0x2236
#define AUD_TOP_INT_MISC_CON0         0x2238
#define AUDNCP_CLKDIV_CON0            0x223a
#define AUDNCP_CLKDIV_CON1            0x223c
#define AUDNCP_CLKDIV_CON2            0x223e
#define AUDNCP_CLKDIV_CON3            0x2240
#define AUDNCP_CLKDIV_CON4            0x2242
#define AUD_TOP_MON_CON0              0x2244
#define AUDIO_DIG_DSN_ID              0x2280
#define AUDIO_DIG_DSN_REV0            0x2282
#define AUDIO_DIG_DSN_DBI             0x2284
#define AUDIO_DIG_DSN_DXI             0x2286
#define AFE_UL_DL_CON0                0x2288
#define AFE_DL_SRC2_CON0_L            0x228a
#define AFE_UL_SRC_CON0_H             0x228c
#define AFE_UL_SRC_CON0_L             0x228e
#define PMIC_AFE_TOP_CON0             0x2290
#define PMIC_AUDIO_TOP_CON0           0x2292
#define AFE_MON_DEBUG0                0x2294
#define AFUNC_AUD_CON0                0x2296
#define AFUNC_AUD_CON1                0x2298
#define AFUNC_AUD_CON2                0x229a
#define AFUNC_AUD_CON3                0x229c
#define AFUNC_AUD_CON4                0x229e
#define AFUNC_AUD_CON5                0x22a0
#define AFUNC_AUD_CON6                0x22a2
#define AFUNC_AUD_MON0                0x22a4
#define AUDRC_TUNE_MON0               0x22a6
#define AFE_ADDA_MTKAIF_FIFO_CFG0     0x22a8
#define AFE_ADDA_MTKAIF_FIFO_LOG_MON1 0x22aa
#define PMIC_AFE_ADDA_MTKAIF_MON0     0x22ac
#define PMIC_AFE_ADDA_MTKAIF_MON1     0x22ae
#define PMIC_AFE_ADDA_MTKAIF_MON2     0x22b0
#define PMIC_AFE_ADDA_MTKAIF_MON3     0x22b2
#define PMIC_AFE_ADDA_MTKAIF_CFG0     0x22b4
#define PMIC_AFE_ADDA_MTKAIF_RX_CFG0  0x22b6
#define PMIC_AFE_ADDA_MTKAIF_RX_CFG1  0x22b8
#define PMIC_AFE_ADDA_MTKAIF_RX_CFG2  0x22ba
#define PMIC_AFE_ADDA_MTKAIF_RX_CFG3  0x22bc
#define PMIC_AFE_ADDA_MTKAIF_TX_CFG1  0x22be
#define AFE_SGEN_CFG0                 0x22c0
#define AFE_SGEN_CFG1                 0x22c2
#define AFE_ADC_ASYNC_FIFO_CFG        0x22c4
#define AFE_DCCLK_CFG0                0x22c6
#define AFE_DCCLK_CFG1                0x22c8
#define AUDIO_DIG_CFG                 0x22ca
#define AFE_AUD_PAD_TOP               0x22cc
#define AFE_AUD_PAD_TOP_MON           0x22ce
#define AFE_AUD_PAD_TOP_MON1          0x22d0
#define AFE_DL_NLE_CFG                0x22d2
#define AFE_DL_NLE_MON                0x22d4
#define AFE_CG_EN_MON                 0x22d6
#define AUDIO_DIG_2ND_DSN_ID          0x2300
#define AUDIO_DIG_2ND_DSN_REV0        0x2302
#define AUDIO_DIG_2ND_DSN_DBI         0x2304
#define AUDIO_DIG_2ND_DSN_DXI         0x2306
#define AFE_PMIC_NEWIF_CFG3           0x2308
#define AFE_VOW_TOP                   0x230a
#define AFE_VOW_CFG0                  0x230c
#define AFE_VOW_CFG1                  0x230e
#define AFE_VOW_CFG2                  0x2310
#define AFE_VOW_CFG3                  0x2312
#define AFE_VOW_CFG4                  0x2314
#define AFE_VOW_CFG5                  0x2316
#define AFE_VOW_CFG6                  0x2318
#define AFE_VOW_MON0                  0x231a
#define AFE_VOW_MON1                  0x231c
#define AFE_VOW_MON2                  0x231e
#define AFE_VOW_MON3                  0x2320
#define AFE_VOW_MON4                  0x2322
#define AFE_VOW_MON5                  0x2324
#define AFE_VOW_SN_INI_CFG            0x2326
#define AFE_VOW_TGEN_CFG0             0x2328
#define AFE_VOW_POSDIV_CFG0           0x232a
#define AFE_VOW_HPF_CFG0              0x232c
#define AFE_VOW_PERIODIC_CFG0         0x232e
#define AFE_VOW_PERIODIC_CFG1         0x2330
#define AFE_VOW_PERIODIC_CFG2         0x2332
#define AFE_VOW_PERIODIC_CFG3         0x2334
#define AFE_VOW_PERIODIC_CFG4         0x2336
#define AFE_VOW_PERIODIC_CFG5         0x2338
#define AFE_VOW_PERIODIC_CFG6         0x233a
#define AFE_VOW_PERIODIC_CFG7         0x233c
#define AFE_VOW_PERIODIC_CFG8         0x233e
#define AFE_VOW_PERIODIC_CFG9         0x2340
#define AFE_VOW_PERIODIC_CFG10        0x2342
#define AFE_VOW_PERIODIC_CFG11        0x2344
#define AFE_VOW_PERIODIC_CFG12        0x2346
#define AFE_VOW_PERIODIC_CFG13        0x2348
#define AFE_VOW_PERIODIC_CFG14        0x234a
#define AFE_VOW_PERIODIC_CFG15        0x234c
#define AFE_VOW_PERIODIC_CFG16        0x234e
#define AFE_VOW_PERIODIC_CFG17        0x2350
#define AFE_VOW_PERIODIC_CFG18        0x2352
#define AFE_VOW_PERIODIC_CFG19        0x2354
#define AFE_VOW_PERIODIC_CFG20        0x2356
#define AFE_VOW_PERIODIC_CFG21        0x2358
#define AFE_VOW_PERIODIC_CFG22        0x235a
#define AFE_VOW_PERIODIC_CFG23        0x235c
#define AFE_VOW_PERIODIC_MON0         0x235e
#define AFE_VOW_PERIODIC_MON1         0x2360
#define AUDENC_DSN_ID                 0x2380
#define AUDENC_DSN_REV0               0x2382
#define AUDENC_DSN_DBI                0x2384
#define AUDENC_DSN_FPI                0x2386
#define AUDENC_ANA_CON0               0x2388
#define AUDENC_ANA_CON1               0x238a
#define AUDENC_ANA_CON2               0x238c
#define AUDENC_ANA_CON3               0x238e
#define AUDENC_ANA_CON4               0x2390
#define AUDENC_ANA_CON5               0x2392
#define AUDENC_ANA_CON6               0x2394
#define AUDENC_ANA_CON7               0x2396
#define AUDENC_ANA_CON8               0x2398
#define AUDENC_ANA_CON9               0x239a
#define AUDENC_ANA_CON10              0x239c
#define AUDENC_ANA_CON11              0x239e
#define AUDENC_ANA_CON12              0x23a0
#define AUDDEC_DSN_ID                 0x2400
#define AUDDEC_DSN_REV0               0x2402
#define AUDDEC_DSN_DBI                0x2404
#define AUDDEC_DSN_FPI                0x2406
#define AUDDEC_ANA_CON0               0x2408
#define AUDDEC_ANA_CON1               0x240a
#define AUDDEC_ANA_CON2               0x240c
#define AUDDEC_ANA_CON3               0x240e
#define AUDDEC_ANA_CON4               0x2410
#define AUDDEC_ANA_CON5               0x2412
#define AUDDEC_ANA_CON6               0x2414
#define AUDDEC_ANA_CON7               0x2416
#define AUDDEC_ANA_CON8               0x2418
#define AUDDEC_ANA_CON9               0x241a
#define AUDDEC_ANA_CON10              0x241c
#define AUDDEC_ANA_CON11              0x241e
#define AUDDEC_ANA_CON12              0x2420
#define AUDDEC_ANA_CON13              0x2422
#define AUDDEC_ANA_CON14              0x2424
#define AUDDEC_ANA_CON15              0x2426
#define AUDDEC_ELR_NUM                0x2428
#define AUDDEC_ELR_0                  0x242a
#define AUDZCD_DSN_ID                 0x2480
#define AUDZCD_DSN_REV0               0x2482
#define AUDZCD_DSN_DBI                0x2484
#define AUDZCD_DSN_FPI                0x2486
#define ZCD_CON0                      0x2488
#define ZCD_CON1                      0x248a
#define ZCD_CON2                      0x248c
#define ZCD_CON3                      0x248e
#define ZCD_CON4                      0x2490
#define ZCD_CON5                      0x2492
#define ACCDET_DSN_DIG_ID             0x2500
#define ACCDET_DSN_DIG_REV0           0x2502
#define ACCDET_DSN_DBI                0x2504
#define ACCDET_DSN_FPI                0x2506
#define ACCDET_CON0                   0x2508
#define ACCDET_CON1                   0x250a
#define ACCDET_CON2                   0x250c
#define ACCDET_CON3                   0x250e
#define ACCDET_CON4                   0x2510
#define ACCDET_CON5                   0x2512
#define ACCDET_CON6                   0x2514
#define ACCDET_CON7                   0x2516
#define ACCDET_CON8                   0x2518
#define ACCDET_CON9                   0x251a
#define ACCDET_CON10                  0x251c
#define ACCDET_CON11                  0x251e
#define ACCDET_CON12                  0x2520
#define ACCDET_CON13                  0x2522
#define ACCDET_CON14                  0x2524
#define ACCDET_CON15                  0x2526
#define ACCDET_CON16                  0x2528
#define ACCDET_CON17                  0x252a
#define ACCDET_CON18                  0x252c
#define ACCDET_CON19                  0x252e
#define ACCDET_CON20                  0x2530
#define ACCDET_CON21                  0x2532
#define ACCDET_CON22                  0x2534
#define ACCDET_CON23                  0x2536
#define ACCDET_CON24                  0x2538
#define ACCDET_CON25                  0x253a
#define ACCDET_CON26                  0x253c
#define ACCDET_CON27                  0x253e
#define ACCDET_CON28                  0x2540

#define TOP_CKPDN_CON0      0x10c
#define TOP_CKPDN_CON0_SET  0x10e
#define TOP_CKPDN_CON0_CLR  0x110

#define TOP_CKHWEN_CON0     0x12a
#define TOP_CKHWEN_CON0_SET 0x12c
#define TOP_CKHWEN_CON0_CLR 0x12e

#define OTP_CON0            0x38a
#define OTP_CON8            0x39a
#define OTP_CON11           0x3a0
#define OTP_CON12           0x3a2
#define OTP_CON13           0x3a4

#define SMT_CON1            0x30
#define DRV_CON3            0x3c
#define GPIO_DIR0           0x88

#define GPIO_MODE2          0xd8	/* mosi */
#define GPIO_MODE2_SET      0xda
#define GPIO_MODE2_CLR      0xdc

#define GPIO_MODE3          0xde	/* miso */
#define GPIO_MODE3_SET      0xe0
#define GPIO_MODE3_CLR      0xe2

#define DCXO_CW13           0x7aa
#define DCXO_CW14           0x7ac

#define AUXADC_CON1         0x118e
#define AUXADC_CON10        0x11a0

void Ana_Set_Reg(unsigned int offset, unsigned int value, unsigned int mask);
unsigned int Ana_Get_Reg(unsigned int offset);

/* for debug usage */
void Ana_Log_Print(void);

int Ana_Debug_Read(char *buffer, const int size);

#endif
